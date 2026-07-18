#include "VulkanContext.h"
#include <android/log.h>
#include <android/native_window.h>
#include <vector>
#include <set>
#include <cstring>
#include <algorithm>

#define LOG_TAG "VoxelCraft"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vc {

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* userData)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        LOGE("Vulkan: %s", data->pMessage);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        LOGW("Vulkan: %s", data->pMessage);
    else
        LOGI("Vulkan: %s", data->pMessage);
    return VK_FALSE;
}

bool createContext(VulkanContext& ctx, ANativeWindow* window, bool validation) {
    ctx.enableValidation = validation;

    // ----- Instance -----
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VoxelCraft";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VoxelCraft Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    std::vector<const char*> instanceExt = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };
    if (validation) instanceExt.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> layers;
    if (validation) layers.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &appInfo;
    ici.enabledExtensionCount = (uint32_t)instanceExt.size();
    ici.ppEnabledExtensionNames = instanceExt.data();
    ici.enabledLayerCount = (uint32_t)layers.size();
    ici.ppEnabledLayerNames = layers.data();

    if (vkCreateInstance(&ici, nullptr, &ctx.instance) != VK_SUCCESS) {
        LOGE("vkCreateInstance failed");
        return false;
    }

    if (validation) {
        VkDebugUtilsMessengerCreateInfoEXT dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dci.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dci.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dci.pfnUserCallback = debugCallback;
        auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(ctx.instance, "vkCreateDebugUtilsMessengerEXT");
        if (fn) fn(ctx.instance, &dci, nullptr, &ctx.debugMessenger);
    }

    // ----- Android surface -----
    VkAndroidSurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    sci.window = window;
    if (vkCreateAndroidSurfaceKHR(ctx.instance, &sci, nullptr, &ctx.surface) != VK_SUCCESS) {
        LOGE("vkCreateAndroidSurfaceKHR failed");
        return false;
    }

    // ----- Pick physical device -----
    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &gpuCount, nullptr);
    if (gpuCount == 0) { LOGE("No Vulkan GPU"); return false; }
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(ctx.instance, &gpuCount, gpus.data());

    ctx.physicalDevice = gpus[0];
    for (auto& g : gpus) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(g, &p);
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
            p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            ctx.physicalDevice = g;
            break;
        }
    }
    vkGetPhysicalDeviceProperties(ctx.physicalDevice, &ctx.props);
    vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice, &ctx.memProps);
    LOGI("Selected GPU: %s", ctx.props.deviceName);

    // ----- Queue families -----
    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &qfCount, qf.data());

    ctx.graphicsFamily = UINT32_MAX;
    ctx.presentFamily  = UINT32_MAX;
    for (uint32_t i = 0; i < qfCount; i++) {
        if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            if (ctx.graphicsFamily == UINT32_MAX) ctx.graphicsFamily = i;
        }
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physicalDevice, i, ctx.surface, &present);
        if (present) ctx.presentFamily = i;
        if (ctx.graphicsFamily != UINT32_MAX && ctx.presentFamily != UINT32_MAX) break;
    }
    if (ctx.graphicsFamily == UINT32_MAX || ctx.presentFamily == UINT32_MAX) {
        LOGE("Missing required queue families");
        return false;
    }

    // ----- Logical device -----
    std::vector<VkDeviceQueueCreateInfo> qis;
    std::set<uint32_t> uniq = { ctx.graphicsFamily, ctx.presentFamily };
    float priority = 1.0f;
    for (uint32_t q : uniq) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = q;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        qis.push_back(qi);
    }

    const char* deviceExt[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid  = VK_TRUE;

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = (uint32_t)qis.size();
    dci.pQueueCreateInfos = qis.data();
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = deviceExt;
    dci.pEnabledFeatures = &features;

    if (vkCreateDevice(ctx.physicalDevice, &dci, nullptr, &ctx.device) != VK_SUCCESS) {
        LOGE("vkCreateDevice failed");
        return false;
    }
    vkGetDeviceQueue(ctx.device, ctx.graphicsFamily, 0, &ctx.graphicsQueue);
    vkGetDeviceQueue(ctx.device, ctx.presentFamily, 0, &ctx.presentQueue);

    // ----- Command pool -----
    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = ctx.graphicsFamily;
    if (vkCreateCommandPool(ctx.device, &cpci, nullptr, &ctx.commandPool) != VK_SUCCESS) {
        LOGE("vkCreateCommandPool failed");
        return false;
    }

    // ----- Descriptor pool -----
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         16 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 },
    };
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets = 32;
    dpci.poolSizeCount = 2;
    dpci.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(ctx.device, &dpci, nullptr, &ctx.descriptorPool) != VK_SUCCESS) {
        LOGE("vkCreateDescriptorPool failed");
        return false;
    }

    LOGI("Vulkan context ready");
    return true;
}

void destroyContext(VulkanContext& ctx) {
    if (ctx.descriptorPool) { vkDestroyDescriptorPool(ctx.device, ctx.descriptorPool, nullptr); ctx.descriptorPool = VK_NULL_HANDLE; }
    if (ctx.commandPool)    { vkDestroyCommandPool(ctx.device, ctx.commandPool, nullptr);       ctx.commandPool = VK_NULL_HANDLE; }
    if (ctx.surface)        { vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);          ctx.surface = VK_NULL_HANDLE; }
    if (ctx.device)         { vkDestroyDevice(ctx.device, nullptr); ctx.device = VK_NULL_HANDLE; }
    if (ctx.debugMessenger) {
        auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (fn) fn(ctx.instance, ctx.debugMessenger, nullptr);
    }
    if (ctx.instance) { vkDestroyInstance(ctx.instance, nullptr); ctx.instance = VK_NULL_HANDLE; }
}

// ===================== Swapchain =====================
static VkSurfaceFormatKHR pickFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
    }
    return formats[0];
}

static VkPresentModeKHR pickMode(const std::vector<VkPresentModeKHR>& modes) {
    for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D pickExtent(const VkSurfaceCapabilitiesKHR& caps, ANativeWindow* window) {
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    int32_t w = ANativeWindow_getWidth(window);
    int32_t h = ANativeWindow_getHeight(window);
    VkExtent2D e;
    e.width  = std::clamp((uint32_t)w, caps.minImageExtent.width,  caps.maxImageExtent.width);
    e.height = std::clamp((uint32_t)h, caps.minImageExtent.height, caps.maxImageExtent.height);
    return e;
}

static VkFormat findSupportedFormat(const VulkanContext& ctx,
                                    const std::vector<VkFormat>& candidates,
                                    VkImageTiling tiling,
                                    VkFormatFeatureFlags features)
{
    for (auto f : candidates) {
        VkFormatProperties p;
        vkGetPhysicalDeviceFormatProperties(ctx.physicalDevice, f, &p);
        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (p.linearTilingFeatures & features) == features) return f;
        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
            (p.optimalTilingFeatures & features) == features) return f;
    }
    return VK_FORMAT_UNDEFINED;
}

static VkFormat findDepthFormat(const VulkanContext& ctx) {
    return findSupportedFormat(ctx,
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool createSwapchain(VulkanContext& ctx, SwapchainResources& sc) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physicalDevice, ctx.surface, &caps);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physicalDevice, ctx.surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physicalDevice, ctx.surface, &fmtCount, formats.data());

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physicalDevice, ctx.surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physicalDevice, ctx.surface, &modeCount, modes.data());

    VkSurfaceFormatKHR sf = pickFormat(formats);
    VkPresentModeKHR pm = pickMode(modes);
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width  = std::clamp(caps.currentExtent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(caps.currentExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0) return false;

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR scci{};
    scci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    scci.surface = ctx.surface;
    scci.minImageCount = imageCount;
    scci.imageFormat = sf.format;
    scci.imageColorSpace = sf.colorSpace;
    scci.imageExtent = extent;
    scci.imageArrayLayers = 1;
    scci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    scci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    scci.preTransform = caps.currentTransform;
    scci.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    scci.presentMode = pm;
    scci.clipped = VK_TRUE;
    scci.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(ctx.device, &scci, nullptr, &sc.swapchain) != VK_SUCCESS) {
        LOGE("vkCreateSwapchainKHR failed");
        return false;
    }
    sc.imageFormat = sf.format;
    sc.extent = extent;

    vkGetSwapchainImagesKHR(ctx.device, sc.swapchain, &imageCount, nullptr);
    sc.images.resize(imageCount);
    vkGetSwapchainImagesKHR(ctx.device, sc.swapchain, &imageCount, sc.images.data());
    sc.imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        sc.imageViews[i] = createImageView(ctx, sc.images[i], sf.format, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // ----- Depth image -----
    sc.depthFormat = findDepthFormat(ctx);
    if (sc.depthFormat != VK_FORMAT_UNDEFINED) {
        if (!createImage2D(ctx, extent.width, extent.height, sc.depthFormat,
                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           VK_IMAGE_TILING_OPTIMAL,
                           sc.depthImage, sc.depthMemory)) {
            LOGE("createImage2D(depth) failed");
            return false;
        }
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (sc.depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            sc.depthFormat == VK_FORMAT_D24_UNORM_S8_UINT)
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        sc.depthView = createImageView(ctx, sc.depthImage, sc.depthFormat, aspect);
        transitionImageLayout(ctx, sc.depthImage, sc.depthFormat,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    // ----- Render pass -----
    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = sc.imageFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = sc.depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = (sc.depthFormat != VK_FORMAT_UNDEFINED) ? &depthRef : nullptr;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = (sc.depthFormat != VK_FORMAT_UNDEFINED) ? 2 : 1;
    rpci.pAttachments = attachments;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;
    if (vkCreateRenderPass(ctx.device, &rpci, nullptr, &sc.renderPass) != VK_SUCCESS) {
        LOGE("vkCreateRenderPass failed");
        return false;
    }

    // ----- Framebuffers -----
    sc.framebuffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageView views[2] = { sc.imageViews[i], sc.depthView };
        VkFramebufferCreateInfo fbci{};
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = sc.renderPass;
        fbci.attachmentCount = (sc.depthFormat != VK_FORMAT_UNDEFINED) ? 2 : 1;
        fbci.pAttachments = views;
        fbci.width = extent.width;
        fbci.height = extent.height;
        fbci.layers = 1;
        if (vkCreateFramebuffer(ctx.device, &fbci, nullptr, &sc.framebuffers[i]) != VK_SUCCESS) {
            LOGE("vkCreateFramebuffer %u failed", i);
            return false;
        }
    }

    // ----- Sync objects -----
    sc.imageAvailable = VK_NULL_HANDLE;
    sc.renderFinished = VK_NULL_HANDLE;
    VkSemaphoreCreateInfo sci2{};
    sci2.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // Use arrays for frames-in-flight style synchronization
    sc.inFlightFences.resize(sc.maxFramesInFlight);
    // Re-use single imageAvailable / renderFinished pair to keep it simple but reliable
    vkCreateSemaphore(ctx.device, &sci2, nullptr, &sc.imageAvailable);
    vkCreateSemaphore(ctx.device, &sci2, nullptr, &sc.renderFinished);
    for (size_t i = 0; i < sc.maxFramesInFlight; i++) {
        vkCreateFence(ctx.device, &fci, nullptr, &sc.inFlightFences[i]);
    }
    sc.currentFrame = 0;

    // ----- Command buffers -----
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = ctx.commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = sc.maxFramesInFlight;
    sc.commandBuffers.resize(sc.maxFramesInFlight);
    if (vkAllocateCommandBuffers(ctx.device, &cbai, sc.commandBuffers.data()) != VK_SUCCESS) {
        LOGE("vkAllocateCommandBuffers failed");
        return false;
    }

    LOGI("Swapchain: %ux%u, %u images", extent.width, extent.height, imageCount);
    return true;
}

void destroySwapchain(VulkanContext& ctx, SwapchainResources& sc) {
    if (ctx.device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(ctx.device);
    if (!sc.commandBuffers.empty()) {
        vkFreeCommandBuffers(ctx.device, ctx.commandPool, (uint32_t)sc.commandBuffers.size(), sc.commandBuffers.data());
        sc.commandBuffers.clear();
    }
    for (auto& f : sc.inFlightFences) if (f) vkDestroyFence(ctx.device, f, nullptr);
    sc.inFlightFences.clear();
    if (sc.imageAvailable) vkDestroySemaphore(ctx.device, sc.imageAvailable, nullptr);
    if (sc.renderFinished) vkDestroySemaphore(ctx.device, sc.renderFinished, nullptr);
    for (auto& fb : sc.framebuffers) if (fb) vkDestroyFramebuffer(ctx.device, fb, nullptr);
    sc.framebuffers.clear();
    if (sc.renderPass) { vkDestroyRenderPass(ctx.device, sc.renderPass, nullptr); sc.renderPass = VK_NULL_HANDLE; }
    if (sc.depthView)  { vkDestroyImageView(ctx.device, sc.depthView, nullptr); sc.depthView = VK_NULL_HANDLE; }
    if (sc.depthImage) { vkDestroyImage(ctx.device, sc.depthImage, nullptr); sc.depthImage = VK_NULL_HANDLE; }
    if (sc.depthMemory){ vkFreeMemory(ctx.device, sc.depthMemory, nullptr); sc.depthMemory = VK_NULL_HANDLE; }
    for (auto& v : sc.imageViews) if (v) vkDestroyImageView(ctx.device, v, nullptr);
    sc.imageViews.clear();
    sc.images.clear();
    if (sc.swapchain) { vkDestroySwapchainKHR(ctx.device, sc.swapchain, nullptr); sc.swapchain = VK_NULL_HANDLE; }
}

// ===================== Helpers =====================
uint32_t findMemoryType(const VulkanContext& ctx, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    for (uint32_t i = 0; i < ctx.memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) &&
            (ctx.memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

VkCommandBuffer beginSingleCommands(const VulkanContext& ctx) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = ctx.commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx.device, &ai, &cmd);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void endSingleCommands(const VulkanContext& ctx, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.graphicsQueue);
    vkFreeCommandBuffers(ctx.device, ctx.commandPool, 1, &cmd);
}

bool createBuffer(const VulkanContext& ctx, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                  VkBuffer& buf, VkDeviceMemory& mem)
{
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx.device, &bci, nullptr, &buf) != VK_SUCCESS) return false;

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(ctx.device, buf, &mr);
    uint32_t type = findMemoryType(ctx, mr.memoryTypeBits, props);
    if (type == UINT32_MAX) return false;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = type;
    if (vkAllocateMemory(ctx.device, &mai, nullptr, &mem) != VK_SUCCESS) return false;
    vkBindBufferMemory(ctx.device, buf, mem, 0);
    return true;
}

void copyBuffer(const VulkanContext& ctx, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer cmd = beginSingleCommands(ctx);
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    endSingleCommands(ctx, cmd);
}

bool createImage2D(const VulkanContext& ctx, uint32_t w, uint32_t h, VkFormat format,
                   VkImageUsageFlags usage, VkImageTiling tiling,
                   VkImage& img, VkDeviceMemory& mem)
{
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent = { w, h, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = tiling;
    ici.usage = usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx.device, &ici, nullptr, &img) != VK_SUCCESS) return false;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(ctx.device, img, &mr);
    uint32_t type = findMemoryType(ctx, mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (type == UINT32_MAX) return false;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = type;
    if (vkAllocateMemory(ctx.device, &mai, nullptr, &mem) != VK_SUCCESS) return false;
    vkBindImageMemory(ctx.device, img, mem, 0);
    return true;
}

VkImageView createImageView(const VulkanContext& ctx, VkImage img, VkFormat fmt,
                            VkImageAspectFlags aspect)
{
    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = img;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = fmt;
    vci.subresourceRange.aspectMask = aspect;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = 1;
    VkImageView view;
    if (vkCreateImageView(ctx.device, &vci, nullptr, &view) != VK_SUCCESS) return VK_NULL_HANDLE;
    return view;
}

void transitionImageLayout(const VulkanContext& ctx, VkImage img, VkFormat fmt,
                           VkImageLayout oldLayout, VkImageLayout newLayout,
                           uint32_t mipLevels)
{
    VkCommandBuffer cmd = beginSingleCommands(ctx);
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = mipLevels;
    b.subresourceRange.layerCount = 1;
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_D24_UNORM_S8_UINT)
            b.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        b.srcAccessMask = 0;
        b.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    endSingleCommands(ctx, cmd);
}

void copyBufferToImage(const VulkanContext& ctx, VkBuffer buf, VkImage img,
                       uint32_t w, uint32_t h)
{
    VkCommandBuffer cmd = beginSingleCommands(ctx);
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {w, h, 1};
    vkCmdCopyBufferToImage(cmd, buf, img,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleCommands(ctx, cmd);
}

} // namespace vc
