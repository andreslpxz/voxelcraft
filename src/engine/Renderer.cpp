#include "Renderer.h"
#include "../world/Chunk.h"
#include <android/log.h>
#include <cmath>
#include <cstring>
#include <algorithm>

#ifdef VOXELCRAFT_EMBEDDED_SHADERS
#include "block.vert.h"
#include "block.frag.h"
#include "sky.vert.h"
#include "sky.frag.h"
#include "ui.vert.h"
#include "ui.frag.h"
#endif

#define LOG_TAG "VoxelCraft"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vc {

// Push constant block for block shader
struct BlockPushConstant {
    Mat4 mvp;
    Mat4 model;
    Vec4 tintAndLight;       // xyz = tint, w = daylight
    Vec4 fogColorAndDist;    // xyz = fog color, w = fog distance
};
// Push constant block for sky shader
struct SkyPushConstant {
    Mat4 mvp;                // identity
    Vec4 sunDirAndDaylight;  // xyz = sun dir, w = daylight
    Vec4 skyColorTop;
    Vec4 skyColorBottom;
    Vec4 sunColor;
};
// Push constant block for UI shader
struct UIPushConstant {
    Vec2 screenSize;
};

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(VulkanContext* ctx, SwapchainResources* sc, World* world) {
    ctx_ = ctx;
    sc_  = sc;
    world_ = world;
    if (!createBlockPipeline()) return false;
    if (!createSkyPipeline())   return false;
    if (!createUIPipeline())    return false;
    LOGI("Renderer initialised");
    return true;
}

void Renderer::shutdown() {
    if (!ctx_) return;
    vkDeviceWaitIdle(ctx_->device);
    destroyPipelines();
    if (atlasLoaded_) destroyTexture(*ctx_, atlasTex_);
    atlasLoaded_ = false;
    ctx_ = nullptr; sc_ = nullptr; world_ = nullptr;
}

bool Renderer::recreateSwapchain() {
    vkDeviceWaitIdle(ctx_->device);
    destroyPipelines();
    if (!createBlockPipeline()) return false;
    if (!createSkyPipeline())   return false;
    if (!createUIPipeline())    return false;
    return true;
}

bool Renderer::loadAtlas(const uint8_t* rgba, uint32_t w, uint32_t h, int cols, int rows) {
    if (atlasLoaded_) destroyTexture(*ctx_, atlasTex_);
    if (!createTextureFromRGBA8(*ctx_, rgba, w, h, atlasTex_)) {
        LOGE("Failed to create atlas texture");
        return false;
    }
    atlasCols_ = cols;
    atlasRows_ = rows;
    atlasLoaded_ = true;
    world_->setAtlas(cols, rows);

    // Update block descriptor set to point to the atlas
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView   = atlasTex_.view;
    info.sampler     = atlasTex_.sampler;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = pipes_.blockDescSet;
    write.dstBinding = 1;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &info;
    vkUpdateDescriptorSets(ctx_->device, 1, &write, 0, nullptr);
    return true;
}

void Renderer::uploadPendingMeshes(int maxPerFrame) {
    if (!world_) return;
    int uploaded = 0;
    world_->forEachChunk([&](Chunk& c, ChunkMesh& m) {
        if (uploaded >= maxPerFrame) return;
        if (c.dirty && !m.gpuLoaded) {
            // (Re)build the mesh if needed
            // Note: mesh build is done in World::updateMeshes, so we only upload here.
        }
        if (!m.gpuLoaded && (m.solidIndexCount + m.waterIndexCount + m.transIndexCount) > 0) {
            uploadChunkMesh(*ctx_, m);
            uploaded++;
        } else if (!m.gpuLoaded && (m.solidIndexCount + m.waterIndexCount + m.transIndexCount) == 0
                   && (!m.solidVertices.empty() || !m.waterVertices.empty() || !m.transparentVertices.empty())) {
            uploadChunkMesh(*ctx_, m);
            uploaded++;
        }
    });
}

// ===================== Pipeline creation =====================
bool Renderer::createBlockPipeline() {
    // Descriptor set layout: binding 0 = uniform buffer (unused for now — we use push constants),
    // binding 1 = combined image sampler (atlas)
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dlci{};
    dlci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dlci.bindingCount = 2;
    dlci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(ctx_->device, &dlci, nullptr, &pipes_.blockDescLayout) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = ctx_->descriptorPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &pipes_.blockDescLayout;
    if (vkAllocateDescriptorSets(ctx_->device, &dsai, &pipes_.blockDescSet) != VK_SUCCESS) return false;

    // Pipeline layout
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(BlockPushConstant);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &pipes_.blockDescLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(ctx_->device, &plci, nullptr, &pipes_.blockLayout) != VK_SUCCESS) return false;

    // Shaders
    VkShaderModule vertMod = VK_NULL_HANDLE, fragMod = VK_NULL_HANDLE;
#ifdef VOXELCRAFT_EMBEDDED_SHADERS
    vertMod = createShaderModule(*ctx_, (const uint32_t*)block_vert_spv, block_vert_spv_len);
    fragMod = createShaderModule(*ctx_, (const uint32_t*)block_frag_spv, block_frag_spv_len);
#endif
    if (vertMod == VK_NULL_HANDLE || fragMod == VK_NULL_HANDLE) {
        LOGE("Block shader modules missing — set VOXELCRAFT_EMBEDDED_SHADERS or provide compiled shaders");
        return false;
    }
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    // Vertex input
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(ChunkVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[4] = {};
    attrs[0].location = 0; attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrs[0].offset = offsetof(ChunkVertex, x);
    attrs[1].location = 1; attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;    attrs[1].offset = offsetof(ChunkVertex, u);
    attrs[2].location = 2; attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32_SFLOAT;       attrs[2].offset = offsetof(ChunkVertex, tile);
    attrs[3].location = 3; attrs[3].binding = 0;
    attrs[3].format = VK_FORMAT_R32_SFLOAT;       attrs[3].offset = offsetof(ChunkVertex, shade);

    VkPipelineVertexInputStateCreateInfo visci{};
    visci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    visci.vertexBindingDescriptionCount = 1;
    visci.pVertexBindingDescriptions = &binding;
    visci.vertexAttributeDescriptionCount = 4;
    visci.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo iaci{};
    iaci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaci.primitiveRestartEnable = VK_FALSE;

    VkViewport vp{0,0,(float)sc_->extent.width,(float)sc_->extent.height,0,1};
    VkRect2D scissor{{0,0}, sc_->extent};
    VkPipelineViewportStateCreateInfo vpci{};
    vpci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpci.viewportCount = 1;
    vpci.pViewports = &vp;
    vpci.scissorCount = 1;
    vpci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rsci{};
    rsci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsci.depthClampEnable = VK_FALSE;
    rsci.rasterizerDiscardEnable = VK_FALSE;
    rsci.polygonMode = VK_POLYGON_MODE_FILL;
    rsci.cullMode = VK_CULL_MODE_BACK_BIT;
    rsci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsci.depthBiasEnable = VK_FALSE;
    rsci.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msci{};
    msci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dsci{};
    dsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsci.depthTestEnable = VK_TRUE;
    dsci.depthWriteEnable = VK_TRUE;
    dsci.depthCompareOp = VK_COMPARE_OP_LESS;
    dsci.depthBoundsTestEnable = VK_FALSE;
    dsci.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState att{};
    att.blendEnable = VK_FALSE;
    att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbsci{};
    cbsci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbsci.logicOpEnable = VK_FALSE;
    cbsci.attachmentCount = 1;
    cbsci.pAttachments = &att;

    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &visci;
    gpci.pInputAssemblyState = &iaci;
    gpci.pViewportState = &vpci;
    gpci.pRasterizationState = &rsci;
    gpci.pMultisampleState = &msci;
    gpci.pDepthStencilState = (sc_->depthFormat != VK_FORMAT_UNDEFINED) ? &dsci : nullptr;
    gpci.pColorBlendState = &cbsci;
    gpci.layout = pipes_.blockLayout;
    gpci.renderPass = sc_->renderPass;
    gpci.subpass = 0;

    bool ok = (vkCreateGraphicsPipelines(ctx_->device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipes_.blockPipeline) == VK_SUCCESS);
    vkDestroyShaderModule(ctx_->device, vertMod, nullptr);
    vkDestroyShaderModule(ctx_->device, fragMod, nullptr);
    if (!ok) { LOGE("vkCreateGraphicsPipelines(block) failed"); return false; }
    return true;
}

bool Renderer::createSkyPipeline() {
    // Pipeline layout: just push constants
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(SkyPushConstant);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(ctx_->device, &plci, nullptr, &pipes_.skyLayout) != VK_SUCCESS) return false;

    VkShaderModule vertMod = VK_NULL_HANDLE, fragMod = VK_NULL_HANDLE;
#ifdef VOXELCRAFT_EMBEDDED_SHADERS
    vertMod = createShaderModule(*ctx_, (const uint32_t*)sky_vert_spv, sky_vert_spv_len);
    fragMod = createShaderModule(*ctx_, (const uint32_t*)sky_frag_spv, sky_frag_spv_len);
#endif
    if (vertMod == VK_NULL_HANDLE || fragMod == VK_NULL_HANDLE) {
        LOGE("Sky shaders missing");
        return false;
    }
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo visci{};
    visci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo iaci{};
    iaci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{0,0,(float)sc_->extent.width,(float)sc_->extent.height,0,1};
    VkRect2D scissor{{0,0}, sc_->extent};
    VkPipelineViewportStateCreateInfo vpci{};
    vpci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpci.viewportCount = 1; vpci.pViewports = &vp;
    vpci.scissorCount = 1;  vpci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rsci{};
    rsci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsci.cullMode = VK_CULL_MODE_NONE;
    rsci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsci.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msci{};
    msci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dsci{};
    dsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsci.depthTestEnable = VK_FALSE;
    dsci.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState att{};
    att.blendEnable = VK_FALSE;
    att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cbsci{};
    cbsci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbsci.attachmentCount = 1; cbsci.pAttachments = &att;

    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount = 2; gpci.pStages = stages;
    gpci.pVertexInputState = &visci;
    gpci.pInputAssemblyState = &iaci;
    gpci.pViewportState = &vpci;
    gpci.pRasterizationState = &rsci;
    gpci.pMultisampleState = &msci;
    gpci.pDepthStencilState = &dsci;
    gpci.pColorBlendState = &cbsci;
    gpci.layout = pipes_.skyLayout;
    gpci.renderPass = sc_->renderPass;
    gpci.subpass = 0;

    bool ok = (vkCreateGraphicsPipelines(ctx_->device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipes_.skyPipeline) == VK_SUCCESS);
    vkDestroyShaderModule(ctx_->device, vertMod, nullptr);
    vkDestroyShaderModule(ctx_->device, fragMod, nullptr);
    if (!ok) { LOGE("vkCreateGraphicsPipelines(sky) failed"); return false; }
    return true;
}

bool Renderer::createUIPipeline() {
    // Reuse block descriptor layout for simplicity (binding 1 = sampler)
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dlci{};
    dlci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dlci.bindingCount = 1;
    dlci.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(ctx_->device, &dlci, nullptr, &pipes_.uiDescLayout) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = ctx_->descriptorPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &pipes_.uiDescLayout;
    if (vkAllocateDescriptorSets(ctx_->device, &dsai, &pipes_.uiDescSet) != VK_SUCCESS) return false;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(UIPushConstant);
    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &pipes_.uiDescLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(ctx_->device, &plci, nullptr, &pipes_.uiLayout) != VK_SUCCESS) return false;

    // UI shaders
    VkShaderModule vertMod = VK_NULL_HANDLE, fragMod = VK_NULL_HANDLE;
#ifdef VOXELCRAFT_EMBEDDED_SHADERS
    vertMod = createShaderModule(*ctx_, (const uint32_t*)ui_vert_spv, ui_vert_spv_len);
    fragMod = createShaderModule(*ctx_, (const uint32_t*)ui_frag_spv, ui_frag_spv_len);
#endif
    if (vertMod == VK_NULL_HANDLE || fragMod == VK_NULL_HANDLE) {
        LOGE("UI shaders missing");
        return false;
    }
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod; stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod; stages[1].pName = "main";

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 4; // pos.xy + uv.xy
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attrs[2] = {};
    attrs[0].location = 0; attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT; attrs[0].offset = 0;
    attrs[1].location = 1; attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT; attrs[1].offset = sizeof(float) * 2;

    VkPipelineVertexInputStateCreateInfo visci{};
    visci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    visci.vertexBindingDescriptionCount = 1;
    visci.pVertexBindingDescriptions = &bindingDesc;
    visci.vertexAttributeDescriptionCount = 2;
    visci.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo iaci{};
    iaci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{0,0,(float)sc_->extent.width,(float)sc_->extent.height,0,1};
    VkRect2D scissor{{0,0}, sc_->extent};
    VkPipelineViewportStateCreateInfo vpci{};
    vpci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpci.viewportCount = 1; vpci.pViewports = &vp;
    vpci.scissorCount = 1;  vpci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rsci{};
    rsci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsci.cullMode = VK_CULL_MODE_NONE;
    rsci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsci.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msci{};
    msci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dsci{};
    dsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsci.depthTestEnable = VK_FALSE;
    dsci.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState att{};
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cbsci{};
    cbsci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbsci.attachmentCount = 1; cbsci.pAttachments = &att;

    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount = 2; gpci.pStages = stages;
    gpci.pVertexInputState = &visci;
    gpci.pInputAssemblyState = &iaci;
    gpci.pViewportState = &vpci;
    gpci.pRasterizationState = &rsci;
    gpci.pMultisampleState = &msci;
    gpci.pDepthStencilState = &dsci;
    gpci.pColorBlendState = &cbsci;
    gpci.layout = pipes_.uiLayout;
    gpci.renderPass = sc_->renderPass;
    gpci.subpass = 0;

    bool ok = (vkCreateGraphicsPipelines(ctx_->device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipes_.uiPipeline) == VK_SUCCESS);
    vkDestroyShaderModule(ctx_->device, vertMod, nullptr);
    vkDestroyShaderModule(ctx_->device, fragMod, nullptr);
    if (!ok) { LOGE("vkCreateGraphicsPipelines(ui) failed"); return false; }

    // Bind atlas to UI descriptor set so the hotbar can sample block tiles
    if (atlasLoaded_) {
        VkDescriptorImageInfo info{};
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = atlasTex_.view;
        info.sampler = atlasTex_.sampler;
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = pipes_.uiDescSet;
        write.dstBinding = 1;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &info;
        vkUpdateDescriptorSets(ctx_->device, 1, &write, 0, nullptr);
    }
    return true;
}

void Renderer::destroyPipelines() {
    if (!ctx_) return;
    vkDeviceWaitIdle(ctx_->device);
    if (pipes_.blockPipeline) { vkDestroyPipeline(ctx_->device, pipes_.blockPipeline, nullptr); pipes_.blockPipeline = VK_NULL_HANDLE; }
    if (pipes_.blockLayout)   { vkDestroyPipelineLayout(ctx_->device, pipes_.blockLayout, nullptr); pipes_.blockLayout = VK_NULL_HANDLE; }
    if (pipes_.blockDescLayout) { vkDestroyDescriptorSetLayout(ctx_->device, pipes_.blockDescLayout, nullptr); pipes_.blockDescLayout = VK_NULL_HANDLE; }
    if (pipes_.skyPipeline)   { vkDestroyPipeline(ctx_->device, pipes_.skyPipeline, nullptr); pipes_.skyPipeline = VK_NULL_HANDLE; }
    if (pipes_.skyLayout)     { vkDestroyPipelineLayout(ctx_->device, pipes_.skyLayout, nullptr); pipes_.skyLayout = VK_NULL_HANDLE; }
    if (pipes_.uiPipeline)    { vkDestroyPipeline(ctx_->device, pipes_.uiPipeline, nullptr); pipes_.uiPipeline = VK_NULL_HANDLE; }
    if (pipes_.uiLayout)      { vkDestroyPipelineLayout(ctx_->device, pipes_.uiLayout, nullptr); pipes_.uiLayout = VK_NULL_HANDLE; }
    if (pipes_.uiDescLayout)  { vkDestroyDescriptorSetLayout(ctx_->device, pipes_.uiDescLayout, nullptr); pipes_.uiDescLayout = VK_NULL_HANDLE; }
    // Descriptor sets are freed when the pool is destroyed.
}

// ===================== Frame rendering =====================
void Renderer::render(const Camera& cam, const DayNightCycle& dayNight) {
    if (!ctx_ || !sc_) return;

    VkFence fence = sc_->inFlightFences[sc_->currentFrame];
    vkWaitForFences(ctx_->device, 1, &fence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(ctx_->device, sc_->swapchain, UINT64_MAX,
                                         sc_->imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain needs recreation — handled by caller
        return;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        return;
    }

    vkResetFences(ctx_->device, 1, &fence);
    VkCommandBuffer cmd = sc_->commandBuffers[sc_->currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    VkClearValue clears[2] = {};
    Vec3 skyColor = dayNight.skyColor();
    clears[0].color = {{ skyColor.x, skyColor.y, skyColor.z, 1.0f }};
    clears[1].depthStencil.depth = 1.0f;
    clears[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rpbi{};
    rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = sc_->renderPass;
    rpbi.framebuffer = sc_->framebuffers[imageIndex];
    rpbi.renderArea.offset = {0, 0};
    rpbi.renderArea.extent = sc_->extent;
    rpbi.clearValueCount = (sc_->depthFormat != VK_FORMAT_UNDEFINED) ? 2 : 1;
    rpbi.pClearValues = clears;
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // ---- Sky ----
    SkyPushConstant skyPush{};
    Vec3 sunDir = dayNight.sunDirection();
    skyPush.mvp = Mat4::identity();
    skyPush.sunDirAndDaylight = Vec4(sunDir, dayNight.daylight());
    skyPush.skyColorTop    = Vec4(skyColor * 1.1f, 1.0f);
    skyPush.skyColorBottom = Vec4(skyColor * 0.6f, 1.0f);
    skyPush.sunColor       = Vec4(1.0f, 0.95f, 0.8f, 1.0f);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipes_.skyPipeline);
    vkCmdPushConstants(cmd, pipes_.skyLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(SkyPushConstant), &skyPush);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // ---- Blocks ----
    float aspect = (float)sc_->extent.width / (float)sc_->extent.height;
    Mat4 viewProj = cam.projection(aspect) * cam.view();

    BlockPushConstant blockPush{};
    blockPush.model = Mat4::identity();
    blockPush.tintAndLight    = Vec4(Vec3(1.0f), dayNight.daylight());
    blockPush.fogColorAndDist = Vec4(skyColor, 60.0f);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipes_.blockPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipes_.blockLayout,
                            0, 1, &pipes_.blockDescSet, 0, nullptr);

    // Iterate chunks and draw
    world_->forEachChunk([&](Chunk& c, ChunkMesh& m) {
        if (!m.gpuLoaded) return;
        // Frustum cull (basic distance cull for now — proper frustum culling left as future work)
        Vec3 chunkCenter = Vec3(
            c.pos.x * CHUNK_SIZE + CHUNK_SIZE * 0.5f,
            CHUNK_HEIGHT * 0.5f,
            c.pos.z * CHUNK_DEPTH + CHUNK_DEPTH * 0.5f
        );
        Vec3 toChunk = chunkCenter - cam.position;
        float distSq = toChunk.lengthSq();
        if (distSq > 100.0f * 100.0f) return; // 100-block view distance

        // Solid layer
        if (m.solidIndexCount > 0) {
            blockPush.model = Mat4::translation(Vec3(
                c.pos.x * CHUNK_SIZE, 0, c.pos.z * CHUNK_DEPTH));
            blockPush.mvp = viewProj * blockPush.model;
            vkCmdPushConstants(cmd, pipes_.blockLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(BlockPushConstant), &blockPush);
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, &m.solidVbo, offsets);
            vkCmdBindIndexBuffer(cmd, m.solidIbo, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, m.solidIndexCount, 1, 0, 0, 0);
        }
        // Transparent layer (leaves, glass)
        if (m.transIndexCount > 0) {
            blockPush.model = Mat4::translation(Vec3(
                c.pos.x * CHUNK_SIZE, 0, c.pos.z * CHUNK_DEPTH));
            blockPush.mvp = viewProj * blockPush.model;
            vkCmdPushConstants(cmd, pipes_.blockLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(BlockPushConstant), &blockPush);
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, &m.transVbo, offsets);
            vkCmdBindIndexBuffer(cmd, m.transIbo, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, m.transIndexCount, 1, 0, 0, 0);
        }
        // Water layer (semi-transparent)
        if (m.waterIndexCount > 0) {
            blockPush.model = Mat4::translation(Vec3(
                c.pos.x * CHUNK_SIZE, 0, c.pos.z * CHUNK_DEPTH));
            blockPush.mvp = viewProj * blockPush.model;
            vkCmdPushConstants(cmd, pipes_.blockLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(BlockPushConstant), &blockPush);
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, &m.waterVbo, offsets);
            vkCmdBindIndexBuffer(cmd, m.waterIbo, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, m.waterIndexCount, 1, 0, 0, 0);
        }
    });

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSem[] = { sc_->imageAvailable };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = waitSem;
    si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    VkSemaphore sigSem[] = { sc_->renderFinished };
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = sigSem;
    if (vkQueueSubmit(ctx_->graphicsQueue, 1, &si, fence) != VK_SUCCESS) {
        LOGE("vkQueueSubmit failed");
    }

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = sigSem;
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc_->swapchain;
    pi.pImageIndices = &imageIndex;
    vkQueuePresentKHR(ctx_->presentQueue, &pi);

    sc_->currentFrame = (sc_->currentFrame + 1) % sc_->maxFramesInFlight;
}

} // namespace vc
