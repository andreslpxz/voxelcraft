#pragma once
#include <cstdint>
#include <string>

namespace vc {

// Block IDs — keep compact so we can store them in a byte per voxel.
enum BlockId : uint8_t {
    BLOCK_AIR     = 0,
    BLOCK_GRASS   = 1,
    BLOCK_DIRT    = 2,
    BLOCK_STONE   = 3,
    BLOCK_SAND    = 4,
    BLOCK_WOOD    = 5,
    BLOCK_LEAVES  = 6,
    BLOCK_WATER   = 7,
    BLOCK_COBBLE  = 8,
    BLOCK_PLANKS  = 9,
    BLOCK_BEDROCK = 10,
    BLOCK_GLASS   = 11,
    BLOCK_COAL    = 12,
    BLOCK_IRON    = 13,
    BLOCK_GOLD    = 14,
    BLOCK_DIAMOND = 15,
    BLOCK_SNOW    = 16,
    BLOCK_BRICK   = 17,
    BLOCK_COUNT
};

inline bool isTransparent(BlockId b) {
    return b == BLOCK_AIR || b == BLOCK_GLASS || b == BLOCK_LEAVES || b == BLOCK_WATER;
}
inline bool isSolid(BlockId b) {
    return b != BLOCK_AIR && b != BLOCK_WATER;
}
inline bool isLiquid(BlockId b) {
    return b == BLOCK_WATER;
}

// 6 faces: +X, -X, +Y, -Y, +Z, -Z
enum Face : uint8_t {
    FACE_PX = 0, FACE_NX = 1,
    FACE_PY = 2, FACE_NY = 3,
    FACE_PZ = 4, FACE_NZ = 5,
    FACE_COUNT
};

// Each block has 6 face texture indices into the texture atlas (per face).
// If all 6 are the same, it's a uniform block.
struct BlockDef {
    const char* name;
    uint8_t texFaces[6];   // texture tile index per face
    bool transparent;
    bool solid;
    bool liquid;
};

// Atlas layout: 4x4 grid of 16x16 tiles = 64x64 px texture (or larger).
// Tile indices:
//   0 grass top, 1 grass side, 2 dirt, 3 stone,
//   4 sand, 5 wood side, 6 wood top, 7 leaves,
//   8 water, 9 cobble, 10 planks, 11 bedrock,
//   12 glass, 13 coal ore, 14 iron ore, 15 diamond ore
static const BlockDef BLOCK_DEFS[BLOCK_COUNT] = {
    /* AIR     */ { "air",     { 0,0,0,0,0,0 }, true,  false, false },
    /* GRASS   */ { "grass",   { 1,1,0,2,1,1 }, false, true,  false },
    /* DIRT    */ { "dirt",    { 2,2,2,2,2,2 }, false, true,  false },
    /* STONE   */ { "stone",   { 3,3,3,3,3,3 }, false, true,  false },
    /* SAND    */ { "sand",    { 4,4,4,4,4,4 }, false, true,  false },
    /* WOOD    */ { "wood",    { 5,5,6,6,5,5 }, false, true,  false },
    /* LEAVES  */ { "leaves",  { 7,7,7,7,7,7 }, true,  true,  false },
    /* WATER   */ { "water",   { 8,8,8,8,8,8 }, true,  false, true  },
    /* COBBLE  */ { "cobble",  { 9,9,9,9,9,9 }, false, true,  false },
    /* PLANKS  */ { "planks",  {10,10,10,10,10,10},false,true, false },
    /* BEDROCK */ { "bedrock", {11,11,11,11,11,11},false,true, false },
    /* GLASS   */ { "glass",   {12,12,12,12,12,12},true, true, false },
    /* COAL    */ { "coal",    {13,13,13,13,13,13},false,true, false },
    /* IRON    */ { "iron",    {14,14,14,14,14,14},false,true, false },
    /* GOLD    */ { "gold",    {15,15,15,15,15,15},false,true, false },
    /* DIAMOND */ { "diamond", {15,15,15,15,15,15},false,true, false },
    /* SNOW    */ { "snow",    { 0,0,0,0,0,0 },    false, true,  false },
    /* BRICK   */ { "brick",   { 9,9,9,9,9,9 },    false, true,  false },
};

inline const BlockDef& blockDef(BlockId b) { return BLOCK_DEFS[(int)b]; }

} // namespace vc
