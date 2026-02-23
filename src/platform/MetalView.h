#pragma once

#include <functional>
#include <cstdint>

namespace dc
{
namespace gfx
{
    struct KeyEvent;
    struct MouseEvent;
    struct WheelEvent;
}

namespace platform
{

class MetalView
{
public:
    explicit MetalView (void* nsWindow);
    ~MetalView();

    // Metal device and layer access (for MetalBackend)
    void* getMetalDevice() const;     // id<MTLDevice>
    void* getMetalCommandQueue() const; // id<MTLCommandQueue>
    void* getMTKView() const;         // MTKView*

    int getDrawableWidth() const;
    int getDrawableHeight() const;
    float getContentsScale() const;

    // Frame callback — called from MTKView's drawInMTKView:
    std::function<void()> onFrame;

    // Event callbacks
    std::function<void (const gfx::MouseEvent&)> onMouseDown;
    std::function<void (const gfx::MouseEvent&)> onMouseDrag;
    std::function<void (const gfx::MouseEvent&)> onMouseUp;
    std::function<void (const gfx::MouseEvent&)> onMouseMove;
    std::function<void (const gfx::KeyEvent&)>   onKeyDown;
    std::function<void (const gfx::KeyEvent&)>   onKeyUp;
    std::function<void (const gfx::WheelEvent&)> onWheel;

    void setPaused (bool paused);

private:
    void* mtkView = nullptr;          // MTKView*
    void* metalDevice = nullptr;      // id<MTLDevice>
    void* metalCommandQueue = nullptr; // id<MTLCommandQueue>
    void* viewDelegate = nullptr;     // MTKViewDelegate
};

} // namespace platform
} // namespace dc
