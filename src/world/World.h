#pragma once
#include "Chunk.h"
#include "ChunkMesh.h"
#include "TerrainGenerator.h"
#include "BlockType.h"
#include "../engine/VulkanContext.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace vc {

class World {
public:
    World(uint64_t seed);
    ~World();

    // Block access in world coordinates
    BlockId getBlock(int wx, int wy, int wz) const;
    BlockId getBlockOrGenerate(int wx, int wy, int wz);
    void setBlock(int wx, int wy, int wz, BlockId b);

    // Get or load a chunk at (cx, cz). If not present, generate it.
    Chunk* getOrCreateChunk(int cx, int cz);
    Chunk* getChunk(int cx, int cz);
    ChunkMesh* getMesh(int cx, int cz);

    // Mark chunk & neighbours dirty (for mesh rebuild after a block edit)
    void markDirtyAround(int wx, int wy, int wz);

    // Build / rebuild pending chunk meshes (one or a few per call to spread cost).
    void updateMeshes(int maxPerFrame = 2);

    // Save/load chunk to disk under given directory
    void setSaveDir(const std::string& dir);
    bool saveChunk(int cx, int cz);
    bool loadChunk(int cx, int cz);
    void saveAllDirty();

    TerrainGenerator& terrain() { return terrain_; }
    int atlasCols() const { return atlasCols_; }
    int atlasRows() const { return atlasRows_; }
    void setAtlas(int cols, int rows) { atlasCols_ = cols; atlasRows_ = rows; }

    // Iterate over all loaded chunks
    void forEachChunk(std::function<void(Chunk&, ChunkMesh&)> fn);

    // Unload distant chunks (return number unloaded)
    int unloadDistant(int playerCX, int playerCZ, int radius);

private:
    TerrainGenerator terrain_;
    std::unordered_map<ChunkPos, std::pair<Chunk, ChunkMesh>, ChunkPosHash> chunks_;
    std::string saveDir_;
    int atlasCols_ = 4;
    int atlasRows_ = 4;
};

} // namespace vc
