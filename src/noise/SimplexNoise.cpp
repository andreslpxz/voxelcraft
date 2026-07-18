#include "SimplexNoise.h"
#include <cmath>

namespace vc {

// Gradient vectors for 2D / 3D
static const float grad3[12][3] = {
    {1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
    {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1},
    {0,1,1}, {0,-1,1}, {0,1,-1}, {0,-1,-1}
};

static const float grad2[8][2] = {
    {1,0}, {-1,0}, {0,1}, {0,-1},
    {1,1}, {-1,1}, {1,-1}, {-1,-1}
};

static inline uint64_t splitmix64(uint64_t& z) {
    z += 0x9E3779B97F4A7C15ULL;
    uint64_t r = z;
    r = (r ^ (r >> 30)) * 0xBF58476D1CE4E5B9ULL;
    r = (r ^ (r >> 27)) * 0x94D049BB133111EBULL;
    return r ^ (r >> 31);
}

SimplexNoise::SimplexNoise(uint64_t seed) {
    init(seed);
}

void SimplexNoise::init(uint64_t seed) {
    uint64_t s = seed;
    uint8_t p[256];
    for (int i = 0; i < 256; i++) p[i] = (uint8_t)i;
    // Fisher-Yates shuffle with our PRNG
    for (int i = 255; i > 0; i--) {
        int j = (int)(splitmix64(s) % (uint64_t)(i + 1));
        uint8_t t = p[i]; p[i] = p[j]; p[j] = t;
    }
    for (int i = 0; i < 512; i++) {
        perm[i] = p[i & 255];
        permMod12[i] = perm[i] % 12;
    }
}

static inline float fastFloor(float x) {
    int i = (int)x;
    return (x < 0 && x != (float)i) ? (float)(i - 1) : (float)i;
}

float SimplexNoise::noise2D(float xin, float yin) const {
    const float F2 = 0.366025403f; // 0.5 * (sqrt(3) - 1)
    const float G2 = 0.211324865f; // (3 - sqrt(3)) / 6

    float s = (xin + yin) * F2;
    int i = (int)fastFloor(xin + s);
    int j = (int)fastFloor(yin + s);
    float t = (i + j) * G2;
    float X0 = i - t;
    float Y0 = j - t;
    float x0 = xin - X0;
    float y0 = yin - Y0;

    int i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }
    else         { i1 = 0; j1 = 1; }

    float x1 = x0 - i1 + G2;
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2;
    float y2 = y0 - 1.0f + 2.0f * G2;

    int ii = i & 255;
    int jj = j & 255;
    int gi0 = perm[ii + perm[jj]] & 7;
    int gi1 = perm[ii + i1 + perm[jj + j1]] & 7;
    int gi2 = perm[ii + 1 + perm[jj + 1]] & 7;

    float n0 = 0, n1 = 0, n2 = 0;
    float t0 = 0.5f - x0*x0 - y0*y0;
    if (t0 >= 0) { t0 *= t0; n0 = t0 * t0 * (grad2[gi0][0]*x0 + grad2[gi0][1]*y0); }
    float t1 = 0.5f - x1*x1 - y1*y1;
    if (t1 >= 0) { t1 *= t1; n1 = t1 * t1 * (grad2[gi1][0]*x1 + grad2[gi1][1]*y1); }
    float t2 = 0.5f - x2*x2 - y2*y2;
    if (t2 >= 0) { t2 *= t2; n2 = t2 * t2 * (grad2[gi2][0]*x2 + grad2[gi2][1]*y2); }

    return 70.0f * (n0 + n1 + n2); // ~[-1, 1]
}

float SimplexNoise::noise3D(float xin, float yin, float zin) const {
    const float F3 = 1.0f / 3.0f;
    const float G3 = 1.0f / 6.0f;

    float s = (xin + yin + zin) * F3;
    int i = (int)fastFloor(xin + s);
    int j = (int)fastFloor(yin + s);
    int k = (int)fastFloor(zin + s);
    float t = (i + j + k) * G3;
    float x0 = xin - (i - t);
    float y0 = yin - (j - t);
    float z0 = zin - (k - t);

    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0)      { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
        else if (x0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
        else               { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
    } else {
        if (y0 < z0)       { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
        else if (x0 < z0)  { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
        else               { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
    }

    float x1 = x0 - i1 + G3;
    float y1 = y0 - j1 + G3;
    float z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2.0f * G3;
    float y2 = y0 - j2 + 2.0f * G3;
    float z2 = z0 - k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3;
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;

    int ii = i & 255, jj = j & 255, kk = k & 255;
    int gi0 = permMod12[ii + perm[jj + perm[kk]]];
    int gi1 = permMod12[ii + i1 + perm[jj + j1 + perm[kk + k1]]];
    int gi2 = permMod12[ii + i2 + perm[jj + j2 + perm[kk + k2]]];
    int gi3 = permMod12[ii + 1 + perm[jj + 1 + perm[kk + 1]]];

    float n0 = 0, n1 = 0, n2 = 0, n3 = 0;
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
    if (t0 >= 0) { t0 *= t0; n0 = t0 * t0 * (grad3[gi0][0]*x0 + grad3[gi0][1]*y0 + grad3[gi0][2]*z0); }
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
    if (t1 >= 0) { t1 *= t1; n1 = t1 * t1 * (grad3[gi1][0]*x1 + grad3[gi1][1]*y1 + grad3[gi1][2]*z1); }
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
    if (t2 >= 0) { t2 *= t2; n2 = t2 * t2 * (grad3[gi2][0]*x2 + grad3[gi2][1]*y2 + grad3[gi2][2]*z2); }
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
    if (t3 >= 0) { t3 *= t3; n3 = t3 * t3 * (grad3[gi3][0]*x3 + grad3[gi3][1]*y3 + grad3[gi3][2]*z3); }

    return 32.0f * (n0 + n1 + n2 + n3);
}

float SimplexNoise::fbm2D(float x, float y, int octaves, float persistence, float lacunarity) const {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float max = 0.0f;
    for (int i = 0; i < octaves; i++) {
        total += noise2D(x * frequency, y * frequency) * amplitude;
        max += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return total / max;
}

float SimplexNoise::fbm3D(float x, float y, float z, int octaves, float persistence, float lacunarity) const {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float max = 0.0f;
    for (int i = 0; i < octaves; i++) {
        total += noise3D(x * frequency, y * frequency, z * frequency) * amplitude;
        max += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return total / max;
}

} // namespace vc
