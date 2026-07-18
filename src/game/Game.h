#pragma once
#include "../engine/VulkanContext.h"
#include "../engine/Renderer.h"
#include "../world/World.h"
#include "../game/Player.h"
#include "../game/InputManager.h"
#include "../game/Inventory.h"
#include "../game/DayNightCycle.h"
#include "../game/SaveManager.h"
#include <memory>
#include <string>
#include <chrono>

namespace vc {

class Game {
public:
    Game();
    ~Game();

    // Called when the Android window is ready
    bool onWindowCreated(ANativeWindow* window, int32_t width, int32_t height);
    void onWindowResized(int32_t width, int32_t height);
    void onWindowDestroyed();
    void onPause();
    void onResume();

    // Input
    void onMotionEvent(int action, int pointerId, float x, float y, int64_t timeMs);
    void onKeyEvent(int keyCode, bool down);

    // One tick of the main loop (called by Android app's main thread)
    void tick();

private:
    // ----- Engine -----
    VulkanContext ctx_;
    SwapchainResources sc_;
    Renderer renderer_;
    bool initialized_ = false;
    bool paused_      = false;
    bool swapchainDirty_ = false;

    // ----- Game state -----
    std::unique_ptr<World> world_;
    Player player_;
    InputManager input_;
    Inventory inventory_;
    DayNightCycle dayNight_;
    std::unique_ptr<SaveManager> save_;
    uint64_t worldSeed_ = 0xC0FFEEULL;

    int viewDistance_ = 6; // chunks radius

    // ----- Timing -----
    using Clock = std::chrono::steady_clock;
    Clock::time_point lastTick_;

    // ----- Helpers -----
    bool loadAtlasTexture();
    void generateSpawnArea();
    void ensureChunksAroundPlayer();
    void handleBlockEdit();
    void savePlayerState();
    void loadPlayerState();
};

} // namespace vc
