#include "TerrainGenerator.h"
#include <cmath>

namespace vc {

TerrainGenerator::TerrainGenerator(uint64_t seed)
    : seed_(seed),
      noiseHeight_(seed ^ 0xD1B54A32D192ED03ULL),
      noiseDetail_(seed ^ 0xCBF29CE484222325ULL),
      noiseBiome_(seed ^ 0x9E3779B97F4A7C15ULL),
      noiseCave_(seed ^ 0x2545F4914F6CDD1DULL)
{}

int TerrainGenerator::computeHeight(int wx, int wz) {
    // Multi-octave fBm for base height
    float n = noiseHeight_.fbm2D(wx * 0.0125f, wz * 0.0125f, 4, 0.5f, 2.0f); // ~[-1,1]
    float detail = noiseDetail_.fbm2D(wx * 0.06f, wz * 0.06f, 3, 0.4f, 2.0f) * 0.2f;

    // Mountain mask: boost areas where noise > 0.4
    float m = 0.0f;
    if (n > 0.4f) m = (n - 0.4f) * 1.6f;

    float h = baseHeight_ + (n + detail) * amplitude_ + m * amplitude_ * 1.5f;
    return (int)std::round(h);
}

int TerrainGenerator::surfaceHeightAt(int wx, int wz) {
    return computeHeight(wx, wz);
}

BlockId TerrainGenerator::pickSurfaceBlock(int height, int wx, int wz) {
    float biome = noiseBiome_.noise2D(wx * 0.008f, wz * 0.008f); // ~[-1,1]
    if (height <= SEA_LEVEL + 1) return BLOCK_SAND;     // beaches near water
    if (biome > 0.4f && height > SEA_LEVEL + 35) return BLOCK_SNOW;
    if (biome < -0.5f) return BLOCK_SAND;               // desert
    return BLOCK_GRASS;
}

bool TerrainGenerator::isCaveAt(int wx, int wy, int wz) {
    // 3D noise cave system — carve where noise > threshold (only below surface)
    float c = noiseCave_.noise3D(wx * 0.05f, wy * 0.08f, wz * 0.05f);
    return c > 0.55f;
}

void TerrainGenerator::generate(Chunk& chunk) {
    int baseX = chunk.pos.x * CHUNK_SIZE;
    int baseZ = chunk.pos.z * CHUNK_DEPTH;

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_DEPTH; z++) {
            int wx = baseX + x;
            int wz = baseZ + z;
            int h = computeHeight(wx, wz);
            if (h < 1) h = 1;
            if (h >= CHUNK_HEIGHT) h = CHUNK_HEIGHT - 1;

            BlockId surface = pickSurfaceBlock(h, wx, wz);

            for (int y = 0; y <= h; y++) {
                BlockId b;
                if (y == 0) {
                    b = BLOCK_BEDROCK;
                } else if (y == h) {
                    b = surface;
                } else if (y >= h - 3) {
                    // 3 blocks below surface = dirt (or sand under beaches)
                    b = (surface == BLOCK_SAND) ? BLOCK_SAND : BLOCK_DIRT;
                } else {
                    b = BLOCK_STONE;
                    // Ore generation: rare ore patches
                    float ore = noiseDetail_.noise3D(wx * 0.1f, y * 0.1f, wz * 0.1f);
                    if (y < 16 && ore > 0.85f) b = BLOCK_DIAMOND;
                    else if (y < 32 && ore > 0.80f) b = BLOCK_GOLD;
                    else if (y < 48 && ore > 0.75f) b = BLOCK_IRON;
                    else if (ore > 0.78f) b = BLOCK_COAL;
                }

                // Carve caves (but never carve bedrock)
                if (y > 0 && y < h && isCaveAt(wx, y, wz)) {
                    b = BLOCK_AIR;
                }

                chunk.blocks[blockIndex(x, y, z)] = b;
            }

            // Fill water up to sea level
            for (int y = h + 1; y <= SEA_LEVEL; y++) {
                chunk.blocks[blockIndex(x, y, z)] = BLOCK_WATER;
            }

            // Place trees on grassy surfaces (low probability, but deterministic per coord)
            if (surface == BLOCK_GRASS && h < CHUNK_HEIGHT - 8) {
                // Hashed pseudo-random per (wx, wz)
                uint32_t hsh = (uint32_t)((wx * 73856093) ^ (wz * 19349663));
                hsh ^= hsh >> 13; hsh *= 0x85ebca6b; hsh ^= hsh >> 13;
                if ((hsh & 0xFF) < 4) { // ~1.6% chance per surface column
                    int treeHeight = 4 + (hsh >> 8) % 3;
                    for (int t = 1; t <= treeHeight; t++) {
                        if (h + t < CHUNK_HEIGHT)
                            chunk.blocks[blockIndex(x, h + t, z)] = BLOCK_WOOD;
                    }
                    // Leaves canopy
                    int topY = h + treeHeight;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -2; dx <= 2; dx++) {
                            for (int dz = -2; dz <= 2; dz++) {
                                if (dx == 0 && dz == 0 && dy < 1) continue;
                                int lx = x + dx, ly = topY + dy, lz = z + dz;
                                if (lx < 0 || lx >= CHUNK_SIZE || lz < 0 || lz >= CHUNK_DEPTH) continue;
                                if (ly < 0 || ly >= CHUNK_HEIGHT) continue;
                                if (chunk.getBlock(lx, ly, lz) == BLOCK_AIR) {
                                    // Make a slightly rounded canopy
                                    int distSq = dx*dx + dz*dz + dy*dy;
                                    if (distSq <= 6 || (distSq <= 8 && dy == 0)) {
                                        chunk.blocks[blockIndex(lx, ly, lz)] = BLOCK_LEAVES;
                                    }
                                }
                            }
                        }
                    }
                    // Top leaf
                    if (topY + 1 < CHUNK_HEIGHT)
                        chunk.blocks[blockIndex(x, topY + 1, z)] = BLOCK_LEAVES;
                }
            }
        }
    }

    chunk.generated = true;
    chunk.dirty = true;
}

BlockId TerrainGenerator::blockAt(int wx, int wy, int wz) {
    if (wy < 0) return BLOCK_BEDROCK;
    if (wy >= CHUNK_HEIGHT) return BLOCK_AIR;
    int h = computeHeight(wx, wz);
    if (wy > h && wy <= SEA_LEVEL) return BLOCK_WATER;
    if (wy > h) return BLOCK_AIR;
    if (wy == 0) return BLOCK_BEDROCK;
    if (wy == h) return pickSurfaceBlock(h, wx, wz);
    if (wy >= h - 3) return (pickSurfaceBlock(h, wx, wz) == BLOCK_SAND) ? BLOCK_SAND : BLOCK_DIRT;
    if (isCaveAt(wx, wy, wz)) return BLOCK_AIR;
    return BLOCK_STONE;
}

} // namespace vc
