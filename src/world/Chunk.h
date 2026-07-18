#pragma once
#include "BlockType.h"
#include <cstdint>
#include <array>

namespace vc {

// Chunk dimensions
constexpr int CHUNK_SIZE  = 16;   // X
constexpr int CHUNK_HEIGHT = 128; // Y (taller for proper terrain)
constexpr int CHUNK_DEPTH  = 16;  // Z
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_DEPTH;

// World height of sea level
constexpr int SEA_LEVEL = 32;

struct ChunkPos {
    int32_t x, z;
    bool operator==(const ChunkPos& o) const { return x == o.x && z == o.z; }
};

struct ChunkPosHash {
    size_t operator()(const ChunkPos& p) const {
        // decent integer hash
        uint64_t key = ((uint64_t)(uint32_t)p.x << 32) | (uint64_t)(uint32_t)p.z;
        key ^= key >> 33; key *= 0xff51afd7ed558ccdULL;
        key ^= key >> 33; key *= 0xc4ceb9fe1a85ec53ULL;
        key ^= key >> 33;
        return (size_t)key;
    }
};

inline uint32_t blockIndex(int x, int y, int z) {
    return (uint32_t)((y * CHUNK_DEPTH + z) * CHUNK_SIZE + x);
}

class Chunk {
public:
    ChunkPos pos;
    std::array<BlockId, CHUNK_VOLUME> blocks{};
    bool dirty = true;          // mesh needs rebuilding
    bool generated = false;     // terrain filled in
    bool uploaded = false;      // mesh on GPU

    Chunk() : pos{0,0} {
        blocks.fill(BLOCK_AIR);
    }

    void setBlock(int x, int y, int z, BlockId b) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return;
        blocks[blockIndex(x, y, z)] = b;
        dirty = true;
    }
    BlockId getBlock(int x, int y, int z) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return BLOCK_AIR;
        return blocks[blockIndex(x, y, z)];
    }
    BlockId getBlockOr(int x, int y, int z, BlockId fallback) const {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) return fallback;
        return blocks[blockIndex(x, y, z)];
    }
};

} // namespace vc
