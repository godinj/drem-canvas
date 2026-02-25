#include "VulkanBackend.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "include/core/SkColorSpace.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "include/gpu/ganesh/vk/GrVkTypes.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/vk/VulkanBackendContext.h"
#include "include/gpu/vk/VulkanTypes.h"

#include <stdexcept>
#include <algorithm>

namespace dc
{
namespace platform
{

VulkanBackend::VulkanBackend (GLFWwindow* win, int w, int h, float s)
    : window (win), width (w), height (h), scale (s)
{
    initVulkan();
    createSwapchain();
}

VulkanBackend::~VulkanBackend()
{
    if (device != VK_NULL_HANDLE)
        vkDeviceWaitIdle (device);

    grContext.reset();

    cleanupSwapchain();

    if (frameFence != VK_NULL_HANDLE)
        vkDestroyFence (device, frameFence, nullptr);
    if (renderFinishedSemaphore != VK_NULL_HANDLE)
        vkDestroySemaphore (device, renderFinishedSemaphore, nullptr);
    if (imageAvailableSemaphore != VK_NULL_HANDLE)
        vkDestroySemaphore (device, imageAvailableSemaphore, nullptr);
    if (device != VK_NULL_HANDLE)
        vkDestroyDevice (device, nullptr);
    if (surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR (instance, surface, nullptr);
    if (instance != VK_NULL_HANDLE)
        vkDestroyInstance (instance, nullptr);
}

void VulkanBackend::initVulkan()
{
    // ── Instance ─────────────────────────────────────────────────────────────
    uint32_t glfwExtCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions (&glfwExtCount);

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Drem Canvas";
    appInfo.applicationVersion = VK_MAKE_VERSION (0, 1, 0);
    appInfo.pEngineName = "DremEngine";
    appInfo.engineVersion = VK_MAKE_VERSION (0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if (vkCreateInstance (&createInfo, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error ("Failed to create Vulkan instance");

    // ── Surface ──────────────────────────────────────────────────────────────
    if (glfwCreateWindowSurface (instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error ("Failed to create window surface");

    // ── Physical device ──────────────────────────────────────────────────────
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices (instance, &deviceCount, nullptr);
    if (deviceCount == 0)
        throw std::runtime_error ("No Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> devices (deviceCount);
    vkEnumeratePhysicalDevices (instance, &deviceCount, devices.data());

    // Pick first device with a graphics queue that supports presentation
    for (auto& dev : devices)
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties (dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies (queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties (dev, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR (dev, i, surface, &presentSupport);

            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
            {
                physicalDevice = dev;
                graphicsQueueIndex = i;
                break;
            }
        }
        if (physicalDevice != VK_NULL_HANDLE)
            break;
    }

    if (physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error ("No suitable GPU found");

    // ── Logical device + queue ───────────────────────────────────────────────
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice (physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error ("Failed to create logical device");

    vkGetDeviceQueue (device, graphicsQueueIndex, 0, &graphicsQueue);

    // ── Sync objects ─────────────────────────────────────────────────────────
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore (device, &semInfo, nullptr, &imageAvailableSemaphore);
    vkCreateSemaphore (device, &semInfo, nullptr, &renderFinishedSemaphore);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence (device, &fenceInfo, nullptr, &frameFence);

    // ── Skia GrDirectContext ─────────────────────────────────────────────────
    skgpu::VulkanBackendContext backendCtx{};
    backendCtx.fInstance = instance;
    backendCtx.fPhysicalDevice = physicalDevice;
    backendCtx.fDevice = device;
    backendCtx.fQueue = graphicsQueue;
    backendCtx.fGraphicsQueueIndex = graphicsQueueIndex;
    backendCtx.fMaxAPIVersion = VK_API_VERSION_1_1;
    backendCtx.fVkExtensions = nullptr;
    backendCtx.fGetProc = [](const char* name, VkInstance inst, VkDevice dev) -> PFN_vkVoidFunction
    {
        if (dev != VK_NULL_HANDLE)
            return vkGetDeviceProcAddr (dev, name);
        return vkGetInstanceProcAddr (inst, name);
    };

    grContext = GrDirectContexts::MakeVulkan (backendCtx);
    if (!grContext)
        throw std::runtime_error ("Failed to create Skia Vulkan context");
}

void VulkanBackend::createSwapchain()
{
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR (physicalDevice, surface, &caps);

    // Choose extent from framebuffer size
    int fbWidth, fbHeight;
    glfwGetFramebufferSize (window, &fbWidth, &fbHeight);

    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX)
    {
        extent = caps.currentExtent;
    }
    else
    {
        extent.width = std::clamp (static_cast<uint32_t> (fbWidth),
                                   caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp (static_cast<uint32_t> (fbHeight),
                                    caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR swapInfo{};
    swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapInfo.surface = surface;
    swapInfo.minImageCount = imageCount;
    swapInfo.imageFormat = swapchainFormat;
    swapInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapInfo.imageExtent = extent;
    swapInfo.imageArrayLayers = 1;
    swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform = caps.currentTransform;
    swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapInfo.clipped = VK_TRUE;
    swapInfo.oldSwapchain = swapchain;

    VkSwapchainKHR newSwapchain;
    if (vkCreateSwapchainKHR (device, &swapInfo, nullptr, &newSwapchain) != VK_SUCCESS)
        throw std::runtime_error ("Failed to create swapchain");

    if (swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR (device, swapchain, nullptr);
    swapchain = newSwapchain;

    uint32_t imgCount = 0;
    vkGetSwapchainImagesKHR (device, swapchain, &imgCount, nullptr);
    swapchainImages.resize (imgCount);
    vkGetSwapchainImagesKHR (device, swapchain, &imgCount, swapchainImages.data());
}

void VulkanBackend::cleanupSwapchain()
{
    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR (device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    swapchainImages.clear();
}

void VulkanBackend::resize (int newWidth, int newHeight, float newScale)
{
    width = newWidth;
    height = newHeight;
    scale = newScale;

    vkDeviceWaitIdle (device);
    createSwapchain();
}

sk_sp<SkSurface> VulkanBackend::beginFrame()
{
    vkWaitForFences (device, 1, &frameFence, VK_TRUE, UINT64_MAX);
    vkResetFences (device, 1, &frameFence);

    VkResult result = vkAcquireNextImageKHR (device, swapchain, UINT64_MAX,
                                              imageAvailableSemaphore, VK_NULL_HANDLE,
                                              &currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        createSwapchain();
        return nullptr;
    }

    // Get framebuffer dimensions
    int fbWidth, fbHeight;
    glfwGetFramebufferSize (window, &fbWidth, &fbHeight);

    // Wrap swapchain image in Skia render target
    GrVkImageInfo imageInfo{};
    imageInfo.fImage = swapchainImages[currentImageIndex];
    imageInfo.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.fFormat = swapchainFormat;
    imageInfo.fImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.fSampleCount = 1;
    imageInfo.fLevelCount = 1;
    imageInfo.fCurrentQueueFamily = graphicsQueueIndex;

    auto backendRT = GrBackendRenderTargets::MakeVk (fbWidth, fbHeight, imageInfo);

    auto surface = SkSurfaces::WrapBackendRenderTarget (
        grContext.get(),
        backendRT,
        kTopLeft_GrSurfaceOrigin,
        kBGRA_8888_SkColorType,
        nullptr,
        nullptr);

    return surface;
}

void VulkanBackend::endFrame (sk_sp<SkSurface>& surface)
{
    if (!surface)
        return;

    // Flush Skia rendering
    grContext->flushAndSubmit (surface.get(), GrSyncCpu::kNo);
    surface.reset();

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &currentImageIndex;

    VkResult result = vkQueuePresentKHR (graphicsQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        createSwapchain();
}

sk_sp<SkSurface> VulkanBackend::createOffscreenSurface (int w, int h)
{
    SkImageInfo info = SkImageInfo::MakeN32Premul (w, h);
    return SkSurfaces::RenderTarget (grContext.get(), skgpu::Budgeted::kYes, info);
}

} // namespace platform
} // namespace dc
