#pragma once

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/gpu/ganesh/GrDirectContext.h"

namespace dc
{
namespace gfx
{

class GpuBackend
{
public:
    virtual ~GpuBackend() = default;

    // Call at start of each frame — wraps current drawable into SkSurface
    virtual sk_sp<SkSurface> beginFrame() = 0;

    // Call at end of frame — flushes Skia and presents drawable
    virtual void endFrame (sk_sp<SkSurface>& surface) = 0;

    virtual GrDirectContext* getContext() const = 0;

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual float getScale() const = 0;

    // Create offscreen surface for texture caching
    virtual sk_sp<SkSurface> createOffscreenSurface (int width, int height) = 0;
};

} // namespace gfx
} // namespace dc
