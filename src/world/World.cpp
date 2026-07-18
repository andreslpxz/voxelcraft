#include "World.h"
#include <android/log.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <cmath>

#define LOG_TAG "VoxelCraft"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vc {

World::World(uint64_t seed) : terrain_(seed) {}
World::~World() {
    for (auto& kv : chunks_) {
        // Mesh GPU resources are freed by renderer on shutdown — but try anyway
    }
}

Chunk* World::getChunk(int cx, int cz) {
    auto it = chunks_.find({cx, cz});
    if (it == chunks_.end()) return nullptr;
    return &it->second.first;
}

Chunk* World::getOrCreateChunk(int cx, int cz) {
    auto it = chunks_.find({cx, cz});
    if (it != chunks_.end()) return &it->second.first;
    // Try loading from disk first
    ChunkPos key{cx, cz};
    auto& entry = chunks_[key];
    Chunk& chunk = entry.first;
    chunk.pos = key;
    if (!loadChunk(cx, cz)) {
        terrain_.generate(chunk);
    }
    return &chunk;
}

ChunkMesh* World::getMesh(int cx, int cz) {
    auto it = chunks_.find({cx, cz});
    if (it == chunks_.end()) return nullptr;
    return &it->second.second;
}

BlockId World::getBlock(int wx, int wy, int wz) const {
    if (wy < 0) return BLOCK_BEDROCK;
    if (wy >= CHUNK_HEIGHT) return BLOCK_AIR;
    int cx = (wx >= 0) ? wx / CHUNK_SIZE  : (wx - CHUNK_SIZE  + 1) / CHUNK_SIZE;
    int cz = (wz >= 0) ? wz / CHUNK_DEPTH : (wz - CHUNK_DEPTH + 1) / CHUNK_DEPTH;
    auto it = chunks_.find({cx, cz});
    if (it == chunks_.end()) return BLOCK_AIR;
    int lx = wx - cx * CHUNK_SIZE;
    int lz = wz - cz * CHUNK_DEPTH;
    return it->second.first.getBlock(lx, wy, lz);
}

BlockId World::getBlockOrGenerate(int wx, int wy, int wz) {
    if (wy < 0) return BLOCK_BEDROCK;
    if (wy >= CHUNK_HEIGHT) return BLOCK_AIR;
    int cx = (wx >= 0) ? wx / CHUNK_SIZE  : (wx - CHUNK_SIZE  + 1) / CHUNK_SIZE;
    int cz = (wz >= 0) ? wz / CHUNK_DEPTH : (wz - CHUNK_DEPTH + 1) / CHUNK_DEPTH;
    Chunk* chunk = getOrCreateChunk(cx, cz);
    int lx = wx - cx * CHUNK_SIZE;
    int lz = wz - cz * CHUNK_DEPTH;
    return chunk->getBlock(lx, wy, lz);
}

void World::setBlock(int wx, int wy, int wz, BlockId b) {
    if (wy < 0 || wy >= CHUNK_HEIGHT) return;
    int cx = (wx >= 0) ? wx / CHUNK_SIZE  : (wx - CHUNK_SIZE  + 1) / CHUNK_SIZE;
    int cz = (wz >= 0) ? wz / CHUNK_DEPTH : (wz - CHUNK_DEPTH + 1) / CHUNK_DEPTH;
    Chunk* chunk = getOrCreateChunk(cx, cz);
    int lx = wx - cx * CHUNK_SIZE;
    int lz = wz - cz * CHUNK_DEPTH;
    chunk->setBlock(lx, wy, lz, b);
    markDirtyAround(wx, wy, wz);
    // Mark chunk for save
    if (!saveDir_.empty()) {
        // schedule saving later via dirty flag (we just mark dirty for mesh + don't auto-save here)
    }
}

void World::markDirtyAround(int wx, int wy, int wz) {
    int cx = (wx >= 0) ? wx / CHUNK_SIZE  : (wx - CHUNK_SIZE  + 1) / CHUNK_SIZE;
    int cz = (wz >= 0) ? wz / CHUNK_DEPTH : (wz - CHUNK_DEPTH + 1) / CHUNK_DEPTH;
    int lx = wx - cx * CHUNK_SIZE;
    int lz = wz - cz * CHUNK_DEPTH;

    if (Chunk* c = getChunk(cx, cz)) c->dirty = true;
    // Neighbours
    if (lx == 0)              { if (Chunk* n = getChunk(cx-1, cz)) n->dirty = true; }
    if (lx == CHUNK_SIZE - 1) { if (Chunk* n = getChunk(cx+1, cz)) n->dirty = true; }
    if (lz == 0)              { if (Chunk* n = getChunk(cx, cz-1)) n->dirty = true; }
    if (lz == CHUNK_DEPTH - 1){ if (Chunk* n = getChunk(cx, cz+1)) n->dirty = true; }
}

void World::updateMeshes(int maxPerFrame) {
    int built = 0;
    for (auto& kv : chunks_) {
        if (built >= maxPerFrame) break;
        Chunk& chunk = kv.second.first;
        if (!chunk.dirty) continue;
        // Ensure neighbour chunks are loaded so face culling is correct at borders.
        for (int dz = -1; dz <= 1; dz++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dz == 0) continue;
                getOrCreateChunk(chunk.pos.x + dx, chunk.pos.z + dz);
            }
        }
        ChunkMesh& mesh = kv.second.second;
        buildChunkMesh(chunk, mesh,
            [this](int wx, int wy, int wz) -> BlockId {
                if (wy < 0) return BLOCK_BEDROCK;
                if (wy >= CHUNK_HEIGHT) return BLOCK_AIR;
                int cx = (wx >= 0) ? wx / CHUNK_SIZE  : (wx - CHUNK_SIZE  + 1) / CHUNK_SIZE;
                int cz = (wz >= 0) ? wz / CHUNK_DEPTH : (wz - CHUNK_DEPTH + 1) / CHUNK_DEPTH;
                Chunk* c = getChunk(cx, cz);
                if (c) {
                    return c->getBlock(wx - cx*CHUNK_SIZE, wy, wz - cz*CHUNK_DEPTH);
                }
                // Not loaded — fall back to procedural terrain
                return terrain_.blockAt(wx, wy, wz);
            },
            atlasCols_, atlasRows_);
        chunk.dirty = false;
        // We won't upload here — the renderer will upload on next frame
        built++;
    }
}

void World::setSaveDir(const std::string& dir) {
    saveDir_ = dir;
    mkdir(dir.c_str(), 0700);
}

bool World::saveChunk(int cx, int cz) {
    if (saveDir_.empty()) return false;
    Chunk* c = getChunk(cx, cz);
    if (!c) return false;
    char path[256];
    snprintf(path, sizeof(path), "%s/chunk_%d_%d.bin", saveDir_.c_str(), cx, cz);
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    // Header
    const char magic[4] = {'V','C','1','\0'};
    fwrite(magic, 1, 4, f);
    fwrite(&c->pos.x, sizeof(int32_t), 1, f);
    fwrite(&c->pos.z, sizeof(int32_t), 1, f);
    fwrite(c->blocks.data(), sizeof(BlockId), CHUNK_VOLUME, f);
    fclose(f);
    return true;
}

bool World::loadChunk(int cx, int cz) {
    if (saveDir_.empty()) return false;
    char path[256];
    snprintf(path, sizeof(path), "%s/chunk_%d_%d.bin", saveDir_.c_str(), cx, cz);
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "VC1\0", 4) != 0) {
        fclose(f); return false;
    }
    int32_t x, z;
    if (fread(&x, sizeof(int32_t), 1, f) != 1) { fclose(f); return false; }
    if (fread(&z, sizeof(int32_t), 1, f) != 1) { fclose(f); return false; }
    Chunk* c = getChunk(cx, cz);
    if (!c) return false;
    if (fread(c->blocks.data(), sizeof(BlockId), CHUNK_VOLUME, f) != CHUNK_VOLUME) {
        fclose(f); return false;
    }
    c->generated = true;
    c->dirty = true;
    fclose(f);
    return true;
}

void World::saveAllDirty() {
    if (saveDir_.empty()) return;
    for (auto& kv : chunks_) {
        Chunk& c = kv.second.first;
        if (c.generated) saveChunk(c.pos.x, c.pos.z);
    }
}

void World::forEachChunk(std::function<void(Chunk&, ChunkMesh&)> fn) {
    for (auto& kv : chunks_) {
        fn(kv.second.first, kv.second.second);
    }
}

int World::unloadDistant(int playerCX, int playerCZ, int radius) {
    int unloaded = 0;
    std::vector<ChunkPos> toRemove;
    for (auto& kv : chunks_) {
        int dx = kv.first.x - playerCX;
        int dz = kv.first.z - playerCZ;
        if (std::abs(dx) > radius || std::abs(dz) > radius) {
            // Try to save before dropping
            if (kv.second.first.generated) saveChunk(kv.first.x, kv.first.z);
            toRemove.push_back(kv.first);
        }
    }
    for (auto& p : toRemove) {
        chunks_.erase(p);
        unloaded++;
    }
    return unloaded;
}

} // namespace vc
