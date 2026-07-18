#include "VulkanTexture.h"

namespace vc {

bool createTextureFromRGBA8(const VulkanContext& ctx, const uint8_t* pixels,
                            uint32_t width, uint32_t height, Texture& outTex)
{
    outTex.width = width;
    outTex.height = height;
    outTex.format = VK_FORMAT_R8G8B8A8_UNORM;

    VkDeviceSize size = (VkDeviceSize)width * height * 4;
    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    if (!createBuffer(ctx, size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuf, stagingMem)) return false;

    void* data = nullptr;
    vkMapMemory(ctx.device, stagingMem, 0, size, 0, &data);
    memcpy(data, pixels, (size_t)size);
    vkUnmapMemory(ctx.device, stagingMem);

    if (!createImage2D(ctx, width, height, outTex.format,
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       VK_IMAGE_TILING_OPTIMAL,
                       outTex.image, outTex.memory)) return false;

    transitionImageLayout(ctx, outTex.image, outTex.format,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(ctx, stagingBuf, outTex.image, width, height);
    transitionImageLayout(ctx, outTex.image, outTex.format,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(ctx.device, stagingBuf, nullptr);
    vkFreeMemory(ctx.device, stagingMem, nullptr);

    outTex.view = createImageView(ctx, outTex.image, outTex.format, VK_IMAGE_ASPECT_COLOR_BIT);
    if (!outTex.view) return false;

    VkSamplerCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_NEAREST;
    sci.minFilter = VK_FILTER_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.anisotropyEnable = VK_FALSE;
    sci.maxAnisotropy = 1.0f;
    sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sci.unnormalizedCoordinates = VK_FALSE;
    sci.compareEnable = VK_FALSE;
    sci.compareOp = VK_COMPARE_OP_ALWAYS;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.mipLodBias = 0.0f;
    sci.minLod = 0.0f;
    sci.maxLod = 0.0f;
    if (vkCreateSampler(ctx.device, &sci, nullptr, &outTex.sampler) != VK_SUCCESS) return false;

    return true;
}

void destroyTexture(const VulkanContext& ctx, Texture& tex) {
    if (tex.sampler) { vkDestroySampler(ctx.device, tex.sampler, nullptr); tex.sampler = VK_NULL_HANDLE; }
    if (tex.view)    { vkDestroyImageView(ctx.device, tex.view, nullptr);  tex.view = VK_NULL_HANDLE; }
    if (tex.image)   { vkDestroyImage(ctx.device, tex.image, nullptr);     tex.image = VK_NULL_HANDLE; }
    if (tex.memory)  { vkFreeMemory(ctx.device, tex.memory, nullptr);      tex.memory = VK_NULL_HANDLE; }
}

} // namespace vc
