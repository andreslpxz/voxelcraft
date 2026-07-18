#pragma once
#include "Chunk.h"
#include "../noise/SimplexNoise.h"
#include <cstdint>

namespace vc {

class TerrainGenerator {
public:
    explicit TerrainGenerator(uint64_t seed);

    // Fill `chunk` with terrain based on its (x, z) world position.
    void generate(Chunk& chunk);

    // Get the surface height (top solid block Y) at world (x, z).
    int surfaceHeightAt(int wx, int wz);

    // Get block at world (x, y, z) — used for cross-chunk neighbour queries during meshing
    // when the neighbour chunk isn't loaded yet (procedural terrain fallback).
    BlockId blockAt(int wx, int wy, int wz);

    uint64_t seed() const { return seed_; }

private:
    uint64_t seed_;
    SimplexNoise noiseHeight_;
    SimplexNoise noiseDetail_;
    SimplexNoise noiseBiome_;
    SimplexNoise noiseCave_;

    int baseHeight_ = 48;
    int amplitude_  = 32;

    int computeHeight(int wx, int wz);
    BlockId pickSurfaceBlock(int height, int wx, int wz);
    bool isCaveAt(int wx, int wy, int wz);
};

} // namespace vc
