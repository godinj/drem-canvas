#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "MetalBackend.h"
#include "platform/MetalView.h"

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlTypes.h"

#include <cassert>

namespace dc
{
namespace gfx
{

MetalBackend::MetalBackend (platform::MetalView& view)
    : metalView (view)
{
    @autoreleasepool
    {
        id<MTLDevice> device = (__bridge id<MTLDevice>) metalView.getMetalDevice();
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>) metalView.getMetalCommandQueue();

        GrMtlBackendContext backendContext = {};
        backendContext.fDevice.retain ((__bridge void*) device);
        backendContext.fQueue.retain ((__bridge void*) queue);

        grContext = GrDirectContexts::MakeMetal (backendContext, {});
        assert (grContext != nullptr);
    }
}

MetalBackend::~MetalBackend()
{
    if (grContext)
    {
        grContext->abandonContext();
        grContext.reset();
    }
}

sk_sp<SkSurface> MetalBackend::beginFrame()
{
    @autoreleasepool
    {
        MTKView* view = (__bridge MTKView*) metalView.getMTKView();
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>) metalView.getMetalCommandQueue();

        id<CAMetalDrawable> drawable = [view currentDrawable];
        if (!drawable)
            return nullptr;

        currentDrawable = (__bridge_retained void*) drawable;

        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        commandBuffer = (__bridge_retained void*) cmdBuf;

        GrMtlTextureInfo textureInfo;
        textureInfo.fTexture.retain ((__bridge void*) drawable.texture);

        int width = static_cast<int> (drawable.texture.width);
        int height = static_cast<int> (drawable.texture.height);

        auto backendRT = GrBackendRenderTargets::MakeMtl (width, height, textureInfo);

        auto surface = SkSurfaces::WrapBackendRenderTarget (
            grContext.get(),
            backendRT,
            kTopLeft_GrSurfaceOrigin,
            kBGRA_8888_SkColorType,
            SkColorSpace::MakeSRGB(),
            nullptr);

        return surface;
    }
}

void MetalBackend::endFrame (sk_sp<SkSurface>& surface)
{
    @autoreleasepool
    {
        if (surface)
        {
            grContext->flushAndSubmit (surface.get(), GrSyncCpu::kNo);
            surface.reset();
        }

        if (commandBuffer && currentDrawable)
        {
            id<MTLCommandBuffer> cmdBuf = (__bridge_transfer id<MTLCommandBuffer>) commandBuffer;
            id<CAMetalDrawable> drawable = (__bridge_transfer id<CAMetalDrawable>) currentDrawable;

            [cmdBuf presentDrawable:drawable];
            [cmdBuf commit];
        }

        commandBuffer = nullptr;
        currentDrawable = nullptr;
    }
}

int MetalBackend::getWidth() const
{
    return metalView.getDrawableWidth();
}

int MetalBackend::getHeight() const
{
    return metalView.getDrawableHeight();
}

float MetalBackend::getScale() const
{
    return metalView.getContentsScale();
}

sk_sp<SkSurface> MetalBackend::createOffscreenSurface (int width, int height)
{
    SkImageInfo imageInfo = SkImageInfo::Make (
        width, height,
        kBGRA_8888_SkColorType,
        kPremul_SkAlphaType,
        SkColorSpace::MakeSRGB());

    return SkSurfaces::RenderTarget (
        grContext.get(),
        skgpu::Budgeted::kYes,
        imageInfo);
}

} // namespace gfx
} // namespace dc
