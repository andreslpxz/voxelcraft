#pragma once
#include "VulkanContext.h"
#include "VulkanTexture.h"
#include "Shader.h"
#include "../world/World.h"
#include "../world/ChunkMesh.h"
#include "../game/Camera.h"
#include "../game/DayNightCycle.h"
#include "../math/Math.h"
#include <unordered_map>
#include <vector>

namespace vc {

// Pipelines + descriptor sets for our 3 render passes (sky, blocks, ui)
struct Pipelines {
    // Block pipeline
    VkPipelineLayout       blockLayout   = VK_NULL_HANDLE;
    VkPipeline             blockPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout  blockDescLayout = VK_NULL_HANDLE;
    VkDescriptorSet        blockDescSet    = VK_NULL_HANDLE;

    // Sky pipeline
    VkPipelineLayout       skyLayout   = VK_NULL_HANDLE;
    VkPipeline             skyPipeline = VK_NULL_HANDLE;

    // UI pipeline
    VkPipelineLayout       uiLayout   = VK_NULL_HANDLE;
    VkPipeline             uiPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout  uiDescLayout = VK_NULL_HANDLE;
    VkDescriptorSet        uiDescSet    = VK_NULL_HANDLE;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    bool init(VulkanContext* ctx, SwapchainResources* sc, World* world);
    void shutdown();

    // Recreate swapchain-dependent resources (framebuffers, pipelines)
    bool recreateSwapchain();

    // Load the block atlas texture from RGBA8 pixel data
    bool loadAtlas(const uint8_t* rgba, uint32_t w, uint32_t h, int cols, int rows);

    // Upload any chunk meshes that need GPU upload
    void uploadPendingMeshes(int maxPerFrame = 4);

    // Render one frame
    void render(const Camera& cam, const DayNightCycle& dayNight);

    // Get the swapchain extent for UI layout
    VkExtent2D extent() const { return sc_ ? sc_->extent : VkExtent2D{0,0}; }

private:
    VulkanContext* ctx_ = nullptr;
    SwapchainResources* sc_ = nullptr;
    World* world_ = nullptr;
    Pipelines pipes_;
    Texture atlasTex_;
    int atlasCols_ = 4;
    int atlasRows_ = 4;
    bool atlasLoaded_ = false;

    bool createBlockPipeline();
    bool createSkyPipeline();
    bool createUIPipeline();
    void destroyPipelines();
};

} // namespace vc
