#pragma once
#include <cstdint>

namespace vc {

// 2D / 3D Simplex noise — based on Stefan Gustavson's reference implementation.
class SimplexNoise {
public:
    explicit SimplexNoise(uint64_t seed = 0x9E3779B97F4A7C15ULL);

    float noise2D(float x, float y) const;
    float noise3D(float x, float y, float z) const;

    // Fractal Brownian motion (octave summing)
    float fbm2D(float x, float y, int octaves = 4, float persistence = 0.5f, float lacunarity = 2.0f) const;
    float fbm3D(float x, float y, float z, int octaves = 4, float persistence = 0.5f, float lacunarity = 2.0f) const;

private:
    uint8_t perm[512];
    uint8_t permMod12[512];

    void init(uint64_t seed);
};

} // namespace vc
