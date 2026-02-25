#pragma once

#include "GpuBackend.h"

namespace dc
{

namespace platform { class MetalView; }

namespace gfx
{

class MetalBackend : public GpuBackend
{
public:
    explicit MetalBackend (platform::MetalView& metalView);
    ~MetalBackend() override;

    sk_sp<SkSurface> beginFrame() override;
    void endFrame (sk_sp<SkSurface>& surface) override;

    GrDirectContext* getContext() const override { return grContext.get(); }

    int getWidth() const override;
    int getHeight() const override;
    float getScale() const override;

    sk_sp<SkSurface> createOffscreenSurface (int width, int height) override;

private:
    platform::MetalView& metalView;
    sk_sp<GrDirectContext> grContext;
    void* currentDrawable = nullptr;   // id<CAMetalDrawable>
    void* commandBuffer = nullptr;     // id<MTLCommandBuffer>
};

} // namespace gfx
} // namespace dc
