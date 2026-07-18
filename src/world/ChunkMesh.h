#pragma once
#include "Chunk.h"
#include "BlockType.h"
#include "../engine/VulkanContext.h"
#include <vector>
#include <cstdint>
#include <functional>

namespace vc {

// Per-vertex layout for chunk meshes
// Position (3 floats) + UV (2 floats) + face/normal-encoded-as-byte + blockID byte
// We'll keep it simple: 3 floats pos + 2 floats uv + 1 float tileIndex + 1 float shade
struct ChunkVertex {
    float x, y, z;     // local coords (0..16, 0..128, 0..16)
    float u, v;        // texture coords in atlas
    float tile;        // tile index
    float shade;       // face shading (AO/face-direction baked)
};

struct ChunkMesh {
    std::vector<ChunkVertex> solidVertices;
    std::vector<uint32_t>    solidIndices;
    std::vector<ChunkVertex> waterVertices;
    std::vector<uint32_t>    waterIndices;
    std::vector<ChunkVertex> transparentVertices; // leaves, glass
    std::vector<uint32_t>    transparentIndices;

    // GPU resources
    VkBuffer      solidVbo = VK_NULL_HANDLE;
    VkDeviceMemory solidVboMem = VK_NULL_HANDLE;
    VkBuffer      solidIbo = VK_NULL_HANDLE;
    VkDeviceMemory solidIboMem = VK_NULL_HANDLE;
    VkBuffer      waterVbo = VK_NULL_HANDLE;
    VkDeviceMemory waterVboMem = VK_NULL_HANDLE;
    VkBuffer      waterIbo = VK_NULL_HANDLE;
    VkDeviceMemory waterIboMem = VK_NULL_HANDLE;
    VkBuffer      transVbo = VK_NULL_HANDLE;
    VkDeviceMemory transVboMem = VK_NULL_HANDLE;
    VkBuffer      transIbo = VK_NULL_HANDLE;
    VkDeviceMemory transIboMem = VK_NULL_HANDLE;

    uint32_t solidIndexCount  = 0;
    uint32_t waterIndexCount  = 0;
    uint32_t transIndexCount  = 0;
    bool     gpuLoaded = false;

    void clear() {
        solidVertices.clear(); solidIndices.clear();
        waterVertices.clear(); waterIndices.clear();
        transparentVertices.clear(); transparentIndices.clear();
    }
};

// Build the mesh for a single chunk given a function that returns neighbour blocks
// at world-space coordinates (so we can do proper face culling at chunk borders).
// getWorldBlock(wx, wy, wz) returns BlockId at world coordinates
void buildChunkMesh(Chunk& chunk, ChunkMesh& mesh,
                    std::function<BlockId(int,int,int)> getWorldBlock,
                    int atlasCols, int atlasRows);

// Upload mesh data to GPU buffers
bool uploadChunkMesh(const VulkanContext& ctx, ChunkMesh& mesh);
void destroyChunkMesh(const VulkanContext& ctx, ChunkMesh& mesh);

} // namespace vc
