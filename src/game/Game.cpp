#include "Game.h"
#include "../world/Chunk.h"
#include "../world/ChunkMesh.h"
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Global asset manager — set from JNI in main.cpp
extern AAssetManager* g_assetManager;
extern std::string g_filesDir;

#define LOG_TAG "VoxelCraft"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vc {

Game::Game() {
    lastTick_ = Clock::now();
}

Game::~Game() {
    if (initialized_) {
        if (save_) savePlayerState();
        if (world_) world_->saveAllDirty();
        renderer_.shutdown();
        destroySwapchain(ctx_, sc_);
        destroyContext(ctx_);
    }
}

bool Game::onWindowCreated(ANativeWindow* window, int32_t width, int32_t height) {
    if (!initialized_) {
        LOGI("Initialising Vulkan context...");
        if (!createContext(ctx_, window, /*validation=*/false)) {
            LOGE("createContext failed");
            return false;
        }
        if (!createSwapchain(ctx_, sc_)) {
            LOGE("createSwapchain failed");
            return false;
        }
        // Set up save dir
        save_ = std::make_unique<SaveManager>(g_filesDir + "/voxelcraft");
        save_->ensureDirs();
        world_ = std::make_unique<World>(worldSeed_);
        world_->setSaveDir(save_->worldDir());
        if (!renderer_.init(&ctx_, &sc_, world_.get())) {
            LOGE("Renderer init failed");
            return false;
        }
        if (!loadAtlasTexture()) {
            LOGW("Atlas texture failed to load — blocks will be untextured");
        }
        // Spawn player on top of terrain at origin
        loadPlayerState();
        if (player_.position.y == 0.0f) {
            // No save — find a safe spawn
            int spawnH = world_->terrain().surfaceHeightAt(0, 0);
            player_.position = Vec3(0.5f, (float)(spawnH + 2), 0.5f);
            player_.yaw = 0;
            player_.pitch = 0;
        }
        player_.updateCamera();
        dayNight_.timeOfDay = 0.30f; // morning
        generateSpawnArea();
        input_.setScreenSize(width, height);
        initialized_ = true;
        LOGI("Game initialised. Player spawn: (%.1f, %.1f, %.1f)",
             player_.position.x, player_.position.y, player_.position.z);
    } else {
        // Recreate surface
        vkDeviceWaitIdle(ctx_.device);
        destroySwapchain(ctx_, sc_);
        VkAndroidSurfaceCreateInfoKHR sci{};
        sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        sci.window = window;
        if (vkCreateAndroidSurfaceKHR(ctx_.instance, &sci, nullptr, &ctx_.surface) != VK_SUCCESS) {
            LOGE("Recreate surface failed");
            return false;
        }
        if (!createSwapchain(ctx_, sc_)) {
            LOGE("Recreate swapchain failed");
            return false;
        }
        if (!renderer_.recreateSwapchain()) {
            LOGE("Recreate pipelines failed");
            return false;
        }
        input_.setScreenSize(width, height);
        swapchainDirty_ = false;
    }
    lastTick_ = Clock::now();
    return true;
}

void Game::onWindowResized(int32_t width, int32_t height) {
    input_.setScreenSize(width, height);
    swapchainDirty_ = true;
}

void Game::onWindowDestroyed() {
    if (!initialized_) return;
    vkDeviceWaitIdle(ctx_.device);
    destroySwapchain(ctx_, sc_);
    if (ctx_.surface) { vkDestroySurfaceKHR(ctx_.instance, ctx_.surface, nullptr); ctx_.surface = VK_NULL_HANDLE; }
}

void Game::onPause() {
    paused_ = true;
    if (save_) savePlayerState();
    if (world_) world_->saveAllDirty();
}

void Game::onResume() {
    paused_ = false;
    lastTick_ = Clock::now();
}

void Game::onMotionEvent(int action, int pointerId, float x, float y, int64_t timeMs) {
    input_.onMotion(action, pointerId, x, y, timeMs);
}

void Game::onKeyEvent(int keyCode, bool down) {
    input_.onKey(keyCode, down);
}

bool Game::loadAtlasTexture() {
    if (!g_assetManager) {
        LOGE("No AAssetManager available");
        return false;
    }
    // Load the raw RGBA8 atlas (no PNG decoder needed). Atlas dimensions are 64x64 (4x4 tiles of 16x16).
    AAsset* asset = AAssetManager_open(g_assetManager, "textures/block_atlas.rgba", AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Could not open textures/block_atlas.rgba");
        return false;
    }
    size_t size = AAsset_getLength(asset);
    const uint32_t ATLAS_W = 64;
    const uint32_t ATLAS_H = 64;
    if (size != ATLAS_W * ATLAS_H * 4) {
        LOGE("Atlas file size mismatch: got %zu, expected %u", size, ATLAS_W * ATLAS_H * 4);
        AAsset_close(asset);
        return false;
    }
    std::vector<uint8_t> pixels(size);
    AAsset_read(asset, pixels.data(), size);
    AAsset_close(asset);

    bool ok = renderer_.loadAtlas(pixels.data(), ATLAS_W, ATLAS_H, /*cols=*/4, /*rows=*/4);
    if (ok) LOGI("Block atlas loaded (64x64, 4x4 tiles)");
    return ok;
}

void Game::loadPlayerState() {
    if (!save_) return;
    PlayerSaveData d{};
    if (save_->loadPlayer(d)) {
        player_.position = Vec3(d.x, d.y, d.z);
        player_.yaw = d.yaw;
        player_.pitch = d.pitch;
        dayNight_.timeOfDay = d.timeOfDay;
    }
}

void Game::savePlayerState() {
    if (!save_) return;
    PlayerSaveData d{};
    d.x = player_.position.x;
    d.y = player_.position.y;
    d.z = player_.position.z;
    d.yaw = player_.yaw;
    d.pitch = player_.pitch;
    d.timeOfDay = dayNight_.timeOfDay;
    save_->savePlayer(d);
}

void Game::generateSpawnArea() {
    // Pre-generate chunks in a small radius around spawn
    int pcx = (int)std::floor(player_.position.x / CHUNK_SIZE);
    int pcz = (int)std::floor(player_.position.z / CHUNK_DEPTH);
    for (int dz = -2; dz <= 2; dz++) {
        for (int dx = -2; dx <= 2; dx++) {
            world_->getOrCreateChunk(pcx + dx, pcz + dz);
        }
    }
    world_->updateMeshes(64);
    // Upload them all immediately
    world_->forEachChunk([&](Chunk& c, ChunkMesh& m) {
        if (!m.gpuLoaded) uploadChunkMesh(ctx_, m);
    });
}

void Game::ensureChunksAroundPlayer() {
    int pcx = (int)std::floor(player_.position.x / CHUNK_SIZE);
    int pcz = (int)std::floor(player_.position.z / CHUNK_DEPTH);
    // Generate chunks in a spiral-ish order from player outward
    for (int r = 0; r <= viewDistance_; r++) {
        for (int dz = -r; dz <= r; dz++) {
            for (int dx = -r; dx <= r; dx++) {
                if (std::abs(dx) != r && std::abs(dz) != r) continue; // ring only
                world_->getOrCreateChunk(pcx + dx, pcz + dz);
            }
        }
    }
    world_->updateMeshes(2); // build up to 2 chunk meshes per frame
    renderer_.uploadPendingMeshes(2);
    world_->unloadDistant(pcx, pcz, viewDistance_ + 2);
}

void Game::handleBlockEdit() {
    // Single tap on right side = break block
    if (input_.state().lookTapped) {
        Vec3 origin = player_.camera.position;
        Vec3 dir = player_.camera.forward();
        RaycastHit hit = raycastVoxels(*world_, origin, dir, 6.0f);
        if (hit.hit) {
            int bx = (int)hit.blockPos.x;
            int by = (int)hit.blockPos.y;
            int bz = (int)hit.blockPos.z;
            world_->setBlock(bx, by, bz, BLOCK_AIR);
        }
    }
    // Long press / second tap = place block (using inventory.current())
    // For simplicity, we use a tap when player is currently not flying and lookTapped triggered.
    // A future improvement: distinguish tap vs long-press for break vs place.
    // Here we add a hotbar slot to place block via tap on top-half of right side... but we already
    // used lookTapped for break. So we just rely on tap = break. Place is via hotbar slot tap = swap slot.
    // TODO: long-press to place (requires tracking touch duration).
}

void Game::tick() {
    if (!initialized_ || paused_) return;

    // Compute dt
    auto now = Clock::now();
    float dt = std::chrono::duration<float>(now - lastTick_).count();
    lastTick_ = now;
    if (dt > 0.25f) dt = 0.25f; // clamp huge gaps (e.g. after pause)

    // Update day/night
    dayNight_.update(dt);

    // Update input flags before player update
    // Reset jumpPressed at the start of each tick
    // (jumpHeld is updated via input state — but we treat any lookTap as jumpPressed for now)
    // Simplification: jump happens when lookTapped AND not in flying mode... no, that would conflict with break.
    // For now, set jumpHeld = true when moveActive and moveDY < -0.7 (joystick pushed up beyond threshold)
    input_.state().jumpHeld = (input_.state().moveActive && input_.state().moveDY < -0.7f);
    input_.state().jumpPressed = input_.state().jumpHeld && (player_.onGround || player_.inWater);

    // Update player physics
    updatePlayer(player_, *world_, input_.state(), dt);

    // Hotbar selection from input
    if (input_.state().hotbarSelected >= 0) {
        inventory_.select(input_.state().hotbarSelected);
    }

    // Block edit handling
    handleBlockEdit();

    // Stream chunks
    ensureChunksAroundPlayer();

    // Periodic save (every ~10s)
    static float saveTimer = 0.0f;
    saveTimer += dt;
    if (saveTimer > 10.0f) {
        saveTimer = 0.0f;
        if (save_) savePlayerState();
        if (world_) world_->saveAllDirty();
    }

    // Render
    renderer_.render(player_.camera, dayNight_);

    // Recreate swapchain if dirty
    if (swapchainDirty_) {
        vkDeviceWaitIdle(ctx_.device);
        destroySwapchain(ctx_, sc_);
        if (createSwapchain(ctx_, sc_)) {
            renderer_.recreateSwapchain();
        }
        swapchainDirty_ = false;
    }

    // Reset per-frame input
    input_.state().resetFrame();
}

} // namespace vc
