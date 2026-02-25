#pragma once

#include "graphics/rendering/GpuBackend.h"

#include <vulkan/vulkan.h>
#include <vector>
#include "include/gpu/vk/VulkanExtensions.h"

struct GLFWwindow;

namespace dc
{
namespace platform
{

class VulkanBackend : public gfx::GpuBackend
{
public:
    VulkanBackend (GLFWwindow* window, int width, int height, float scale);
    ~VulkanBackend() override;

    // Non-copyable
    VulkanBackend (const VulkanBackend&) = delete;
    VulkanBackend& operator= (const VulkanBackend&) = delete;

    // GpuBackend interface
    sk_sp<SkSurface> beginFrame() override;
    void endFrame (sk_sp<SkSurface>& surface) override;
    GrDirectContext* getContext() const override { return grContext.get(); }
    int getWidth() const override { return width; }
    int getHeight() const override { return height; }
    float getScale() const override { return scale; }
    sk_sp<SkSurface> createOffscreenSurface (int w, int h) override;

    // Called on window resize
    void resize (int newWidth, int newHeight, float newScale);

private:
    void initVulkan();
    void createSwapchain();
    void cleanupSwapchain();

    GLFWwindow* window;
    int width;
    int height;
    float scale;

    // Vulkan handles
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueIndex = 0;

    // Swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    std::vector<VkImage> swapchainImages;
    uint32_t currentImageIndex = 0;

    // Synchronisation
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence frameFence = VK_NULL_HANDLE;

    // Device features (kept alive for Skia backend context)
    VkPhysicalDeviceFeatures deviceFeatures{};

    // Skia
    skgpu::VulkanExtensions vkExtensions;
    sk_sp<GrDirectContext> grContext;
};

} // namespace platform
} // namespace dc
