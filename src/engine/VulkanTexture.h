#pragma once
#include "VulkanContext.h"
#include <cstdint>
#include <vector>

namespace vc {

struct Texture {
    VkImage        image     = VK_NULL_HANDLE;
    VkDeviceMemory memory    = VK_NULL_HANDLE;
    VkImageView    view      = VK_NULL_HANDLE;
    VkSampler      sampler   = VK_NULL_HANDLE;
    uint32_t       width     = 0;
    uint32_t       height    = 0;
    VkFormat       format    = VK_FORMAT_R8G8B8A8_UNORM;
};

// Create a 2D texture from raw RGBA8 pixel data (4 bytes/pixel)
bool createTextureFromRGBA8(const VulkanContext& ctx, const uint8_t* pixels,
                            uint32_t width, uint32_t height, Texture& outTex);
void destroyTexture(const VulkanContext& ctx, Texture& tex);

} // namespace vc
