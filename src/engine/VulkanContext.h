#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace vc {

struct VulkanContext {
    VkInstance       instance       = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;
    VkQueue          presentQueue   = VK_NULL_HANDLE;
    uint32_t         graphicsFamily = 0;
    uint32_t         presentFamily  = 0;
    VkCommandPool    commandPool    = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkSurfaceKHR     surface        = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties       props;
    VkPhysicalDeviceMemoryProperties memProps;

    bool enableValidation = false;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
};

struct SwapchainResources {
    VkSwapchainKHR        swapchain    = VK_NULL_HANDLE;
    VkFormat              imageFormat  = VK_FORMAT_UNDEFINED;
    VkExtent2D            extent       = {0, 0};
    std::vector<VkImage>  images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass          renderPass   = VK_NULL_HANDLE;
    VkSemaphore           imageAvailable = VK_NULL_HANDLE;
    VkSemaphore           renderFinished = VK_NULL_HANDLE;
    std::vector<VkFence>  inFlightFences;
    std::vector<VkCommandBuffer> commandBuffers;
    uint32_t              currentFrame = 0;
    VkImage               depthImage   = VK_NULL_HANDLE;
    VkDeviceMemory        depthMemory  = VK_NULL_HANDLE;
    VkImageView           depthView    = VK_NULL_HANDLE;
    VkFormat              depthFormat  = VK_FORMAT_UNDEFINED;
    size_t                maxFramesInFlight = 2;
};

// Create instance + pick physical device + create logical device + create surface from Android window
bool createContext(VulkanContext& ctx, ANativeWindow* window, bool validation = false);
void destroyContext(VulkanContext& ctx);

// Create / recreate swapchain to match window
bool createSwapchain(VulkanContext& ctx, SwapchainResources& sc);
void destroySwapchain(VulkanContext& ctx, SwapchainResources& sc);

// Single-shot command buffer helper
VkCommandBuffer beginSingleCommands(const VulkanContext& ctx);
void endSingleCommands(const VulkanContext& ctx, VkCommandBuffer cmd);

// Memory helper
uint32_t findMemoryType(const VulkanContext& ctx, uint32_t typeFilter, VkMemoryPropertyFlags props);

// Create buffer + memory
bool createBuffer(const VulkanContext& ctx, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                  VkBuffer& buf, VkDeviceMemory& mem);

// Copy buffer src -> dst (host visible staging often used)
void copyBuffer(const VulkanContext& ctx, VkBuffer src, VkBuffer dst, VkDeviceSize size);

// Create a 2D image + allocate + bind
bool createImage2D(const VulkanContext& ctx, uint32_t w, uint32_t h, VkFormat format,
                   VkImageUsageFlags usage, VkImageTiling tiling,
                   VkImage& img, VkDeviceMemory& mem);

VkImageView createImageView(const VulkanContext& ctx, VkImage img, VkFormat fmt,
                            VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

void transitionImageLayout(const VulkanContext& ctx, VkImage img, VkFormat fmt,
                           VkImageLayout oldLayout, VkImageLayout newLayout,
                           uint32_t mipLevels = 1);

void copyBufferToImage(const VulkanContext& ctx, VkBuffer buf, VkImage img,
                       uint32_t w, uint32_t h);

} // namespace vc
