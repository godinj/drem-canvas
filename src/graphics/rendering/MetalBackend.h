#pragma once

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/gpu/ganesh/GrDirectContext.h"

namespace dc
{

namespace platform { class MetalView; }

namespace gfx
{

class MetalBackend
{
public:
    explicit MetalBackend (platform::MetalView& metalView);
    ~MetalBackend();

    // Call at start of each frame — wraps current drawable into SkSurface
    sk_sp<SkSurface> beginFrame();

    // Call at end of frame — flushes Skia and presents drawable
    void endFrame (sk_sp<SkSurface>& surface);

    GrDirectContext* getContext() const { return grContext.get(); }

    int getWidth() const;
    int getHeight() const;
    float getScale() const;

    // Create offscreen surface for texture caching
    sk_sp<SkSurface> createOffscreenSurface (int width, int height);

private:
    platform::MetalView& metalView;
    sk_sp<GrDirectContext> grContext;
    void* currentDrawable = nullptr;   // id<CAMetalDrawable>
    void* commandBuffer = nullptr;     // id<MTLCommandBuffer>
};

} // namespace gfx
} // namespace dc
