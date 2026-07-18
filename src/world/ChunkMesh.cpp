#include "ChunkMesh.h"
#include <cmath>

namespace vc {

// Face data: for each of 6 faces, the 4 corner offsets (CCW when viewed from outside)
// and the normal direction (used for shading).
static const float FACE_CORNERS[6][4][3] = {
    // +X
    {{1,0,1},{1,0,0},{1,1,0},{1,1,1}},
    // -X
    {{0,0,0},{0,0,1},{0,1,1},{0,1,0}},
    // +Y (top)
    {{0,1,1},{1,1,1},{1,1,0},{0,1,0}},
    // -Y (bottom)
    {{0,0,0},{1,0,0},{1,0,1},{0,0,1}},
    // +Z
    {{0,0,1},{1,0,1},{1,1,1},{0,1,1}},
    // -Z
    {{1,0,0},{0,0,0},{0,1,0},{1,1,0}},
};

static const float FACE_NORMALS[6][3] = {
    { 1, 0, 0}, {-1, 0, 0},
    { 0, 1, 0}, { 0,-1, 0},
    { 0, 0, 1}, { 0, 0,-1},
};

// Per-face shading (top brightest, bottom darkest) — Minecraft-like
static const float FACE_SHADE[6] = {
    0.72f, // +X
    0.72f, // -X
    1.00f, // +Y (top)
    0.50f, // -Y (bottom)
    0.86f, // +Z
    0.86f, // -Z
};

// UVs per corner — match FACE_CORNERS winding
static const float FACE_UVS[6][4][2] = {
    // +X
    {{0,1},{1,1},{1,0},{0,0}},
    // -X
    {{0,1},{1,1},{1,0},{0,0}},
    // +Y
    {{0,1},{1,1},{1,0},{0,0}},
    // -Y
    {{0,1},{1,1},{1,0},{0,0}},
    // +Z
    {{0,1},{1,1},{1,0},{0,0}},
    // -Z
    {{0,1},{1,1},{1,0},{0,0}},
};

void buildChunkMesh(Chunk& chunk, ChunkMesh& mesh,
                    std::function<BlockId(int,int,int)> getWorldBlock,
                    int atlasCols, int atlasRows)
{
    mesh.clear();
    int baseX = chunk.pos.x * CHUNK_SIZE;
    int baseZ = chunk.pos.z * CHUNK_DEPTH;

    auto emitFace = [&](BlockId b, Face f, int x, int y, int z, std::vector<ChunkVertex>& verts, std::vector<uint32_t>& idx) {
        const auto& def = blockDef(b);
        uint8_t tile = def.texFaces[f];
        // Compute atlas UV rect for this tile
        float col = (float)(tile % atlasCols);
        float row = (float)(tile / atlasCols);
        float du = 1.0f / atlasCols;
        float dv = 1.0f / atlasRows;

        uint32_t startIdx = (uint32_t)verts.size();
        for (int i = 0; i < 4; i++) {
            ChunkVertex v;
            v.x = (float)(x + FACE_CORNERS[f][i][0]);
            v.y = (float)(y + FACE_CORNERS[f][i][1]);
            v.z = (float)(z + FACE_CORNERS[f][i][2]);
            // FACE_UVS gives (0..1, 0..1) — tile into atlas
            float uu = FACE_UVS[f][i][0];
            float vv = FACE_UVS[f][i][1];
            v.u = (col + uu) * du;
            v.v = (row + vv) * dv;
            v.tile  = (float)tile;
            v.shade = FACE_SHADE[f];
            verts.push_back(v);
        }
        idx.push_back(startIdx + 0);
        idx.push_back(startIdx + 1);
        idx.push_back(startIdx + 2);
        idx.push_back(startIdx + 0);
        idx.push_back(startIdx + 2);
        idx.push_back(startIdx + 3);
    };

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                BlockId b = chunk.getBlock(x, y, z);
                if (b == BLOCK_AIR) continue;

                int wx = baseX + x;
                int wy = y;
                int wz = baseZ + z;

                for (int f = 0; f < 6; f++) {
                    int nx = x + (int)FACE_NORMALS[f][0];
                    int ny = y + (int)FACE_NORMALS[f][1];
                    int nz = z + (int)FACE_NORMALS[f][2];

                    BlockId neighbour;
                    if (nx < 0 || nx >= CHUNK_SIZE || nz < 0 || nz >= CHUNK_DEPTH ||
                        ny < 0 || ny >= CHUNK_HEIGHT) {
                        neighbour = getWorldBlock(wx + (int)FACE_NORMALS[f][0],
                                                  wy + (int)FACE_NORMALS[f][1],
                                                  wz + (int)FACE_NORMALS[f][2]);
                    } else {
                        neighbour = chunk.getBlock(nx, ny, nz);
                    }

                    // Skip faces that are hidden by an opaque neighbour.
                    // For transparent/liquid blocks, only emit if neighbour is air OR a different transparent type.
                    if (b == BLOCK_WATER) {
                        if (f == FACE_NY) continue; // never draw bottom of water (covered by terrain)
                        if (!isTransparent(neighbour)) continue;
                        if (neighbour == BLOCK_WATER) continue;
                        emitFace(b, (Face)f, x, y, z, mesh.waterVertices, mesh.waterIndices);
                    } else if (isTransparent(b)) {
                        // leaves, glass: don't draw faces against same type
                        if (neighbour == b) continue;
                        if (!isTransparent(neighbour)) continue;
                        emitFace(b, (Face)f, x, y, z, mesh.transparentVertices, mesh.transparentIndices);
                    } else {
                        // opaque: skip face if neighbour is opaque (or water, since water is see-through but culls against opaque)
                        if (!isTransparent(neighbour)) continue;
                        emitFace(b, (Face)f, x, y, z, mesh.solidVertices, mesh.solidIndices);
                    }
                }
            }
        }
    }
    chunk.dirty = false;
}

static bool uploadLayer(const VulkanContext& ctx,
                        std::vector<ChunkVertex>& verts, std::vector<uint32_t>& idx,
                        VkBuffer& vbo, VkDeviceMemory& vboMem,
                        VkBuffer& ibo, VkDeviceMemory& iboMem,
                        uint32_t& indexCount)
{
    indexCount = (uint32_t)idx.size();
    if (verts.empty() || idx.empty()) return true;

    VkDeviceSize vboSize = verts.size() * sizeof(ChunkVertex);
    VkDeviceSize iboSize = idx.size()   * sizeof(uint32_t);

    VkBuffer stagingVbo, stagingIbo;
    VkDeviceMemory stagingVboMem, stagingIboMem;
    if (!createBuffer(ctx, vboSize,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingVbo, stagingVboMem)) return false;
    if (!createBuffer(ctx, iboSize,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingIbo, stagingIboMem)) {
        vkDestroyBuffer(ctx.device, stagingVbo, nullptr);
        vkFreeMemory(ctx.device, stagingVboMem, nullptr);
        return false;
    }

    void* p = nullptr;
    vkMapMemory(ctx.device, stagingVboMem, 0, vboSize, 0, &p);
    memcpy(p, verts.data(), (size_t)vboSize);
    vkUnmapMemory(ctx.device, stagingVboMem);
    vkMapMemory(ctx.device, stagingIboMem, 0, iboSize, 0, &p);
    memcpy(p, idx.data(), (size_t)iboSize);
    vkUnmapMemory(ctx.device, stagingIboMem);

    if (!createBuffer(ctx, vboSize,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      vbo, vboMem)) return false;
    if (!createBuffer(ctx, iboSize,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      ibo, iboMem)) return false;

    copyBuffer(ctx, stagingVbo, vbo, vboSize);
    copyBuffer(ctx, stagingIbo, ibo, iboSize);

    vkDestroyBuffer(ctx.device, stagingVbo, nullptr);
    vkFreeMemory(ctx.device, stagingVboMem, nullptr);
    vkDestroyBuffer(ctx.device, stagingIbo, nullptr);
    vkFreeMemory(ctx.device, stagingIboMem, nullptr);

    // Free CPU copies to save memory
    verts.clear();
    verts.shrink_to_fit();
    idx.clear();
    idx.shrink_to_fit();
    return true;
}

bool uploadChunkMesh(const VulkanContext& ctx, ChunkMesh& mesh) {
    if (!uploadLayer(ctx, mesh.solidVertices, mesh.solidIndices,
                     mesh.solidVbo, mesh.solidVboMem,
                     mesh.solidIbo, mesh.solidIboMem,
                     mesh.solidIndexCount)) return false;
    if (!uploadLayer(ctx, mesh.waterVertices, mesh.waterIndices,
                     mesh.waterVbo, mesh.waterVboMem,
                     mesh.waterIbo, mesh.waterIboMem,
                     mesh.waterIndexCount)) return false;
    if (!uploadLayer(ctx, mesh.transparentVertices, mesh.transparentIndices,
                     mesh.transVbo, mesh.transVboMem,
                     mesh.transIbo, mesh.transIboMem,
                     mesh.transIndexCount)) return false;
    mesh.gpuLoaded = true;
    return true;
}

void destroyChunkMesh(const VulkanContext& ctx, ChunkMesh& mesh) {
    auto destroy = [&](VkBuffer& b, VkDeviceMemory& m) {
        if (b) { vkDestroyBuffer(ctx.device, b, nullptr); b = VK_NULL_HANDLE; }
        if (m) { vkFreeMemory(ctx.device, m, nullptr);    m = VK_NULL_HANDLE; }
    };
    destroy(mesh.solidVbo, mesh.solidVboMem);
    destroy(mesh.solidIbo, mesh.solidIboMem);
    destroy(mesh.waterVbo, mesh.waterVboMem);
    destroy(mesh.waterIbo, mesh.waterIboMem);
    destroy(mesh.transVbo, mesh.transVboMem);
    destroy(mesh.transIbo, mesh.transIboMem);
    mesh.gpuLoaded = false;
    mesh.solidIndexCount = mesh.waterIndexCount = mesh.transIndexCount = 0;
}

} // namespace vc
