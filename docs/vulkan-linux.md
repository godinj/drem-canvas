# Vulkan/Linux Fixes (2026-02-24)

## Bug 1: JUCE Instance Detection on Wayland

**Symptom**: App exits silently (exit code 0, no output) before `initialise()` is called.

**Root cause**: `moreThanOneInstanceAllowed()` returned `false`, triggering JUCE's
`sendCommandLineToPreexistingInstance()` which uses X11 atoms. On Wayland/XWayland,
this falsely detects a "running" instance and returns true, causing JUCE to quit
before ever calling `initialise()`.

**Fix**: `moreThanOneInstanceAllowed()` returns `true` on Linux (ifdef), `false` on macOS.
File: `src/Main.cpp`

## Bug 2: Vulkan Synchronization

**Symptom**: Timer fires but only 2 frames in 3 seconds; window never appears on Wayland.

**Root cause**: `endFrame()` passed `renderFinishedSemaphore` to `vkQueuePresentKHR`,
but Skia never signals this semaphore (Skia manages its own command buffer submission).
Similarly, `frameFence` was waited on before acquire but never re-signaled after frame 0.
Result: present never completes, and second frame blocks forever on fence wait.

**Fix**:
- Use `GrSyncCpu::kYes` in `flushAndSubmit()` to CPU-wait for GPU completion
- Present with `waitSemaphoreCount = 0` (no semaphores needed after CPU sync)
- Move fence to `vkAcquireNextImageKHR` (signals fence when image is available)
- Wait+reset fence after acquire, before rendering

File: `src/platform/linux/VulkanBackend.cpp` (beginFrame/endFrame)

## Bug 3: Missing VK_IMAGE_USAGE_TRANSFER_SRC_BIT

**Symptom**: `SkSurfaces::WrapBackendRenderTarget()` returns null despite valid context,
valid format, correct color type, and working offscreen surfaces.

**Root cause**: Skia's `check_image_info()` (in `GrVkGpu.cpp`) requires:
```cpp
if (!SkToBool(info.fImageUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ||
    !SkToBool(info.fImageUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
    return false;
}
```
Swapchain only had `COLOR_ATTACHMENT_BIT | TRANSFER_DST_BIT` — missing `TRANSFER_SRC_BIT`.

**Fix**: Add `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` to both:
- `VkSwapchainCreateInfoKHR.imageUsage` (swapchain creation)
- `GrVkImageInfo.fImageUsageFlags` (Skia image info)

File: `src/platform/linux/VulkanBackend.cpp` (createSwapchain/beginFrame)

## Additional Fix: Skia Backend Context

**What**: Previously `fVkExtensions = nullptr` and `fDeviceFeatures = nullptr` in
`VulkanBackendContext`. Now properly initialized:
- `skgpu::VulkanExtensions` populated via `init()` with instance + device extensions
- `VkPhysicalDeviceFeatures` queried and passed to both `VkDeviceCreateInfo` and Skia

File: `src/platform/linux/VulkanBackend.cpp` (initVulkan), `VulkanBackend.h` (new members)
