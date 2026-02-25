#pragma once

#include "graphics/core/Node.h"
#include "graphics/core/Widget.h"
#include "Canvas.h"
#include "GpuBackend.h"
#include <vector>

namespace dc
{
namespace gfx
{

class Renderer
{
public:
    explicit Renderer (GpuBackend& backend);

    // Called from MTKView's drawInMTKView: callback
    void renderFrame (Widget& rootWidget);

    // Register widgets that need per-frame animation ticks
    void addAnimatingWidget (Widget* w);
    void removeAnimatingWidget (Widget* w);

    GpuBackend& getBackend() { return backend; }

    // Frame stats
    double getLastFrameTimeMs() const { return lastFrameTimeMs; }
    int getFrameCount() const { return frameCount; }
    int getSkippedFrames() const { return skippedFrames; }

    // Performance: force next frame to render (e.g., after resize)
    void forceNextFrame() { forcePaint = true; }

private:
    void animationTick (double timestampMs);
    void layoutPass (Widget& widget);
    void paintPass (Canvas& canvas, Node& node, float parentOpacity);
    void paintCached (Canvas& canvas, Node& node, float parentOpacity);
    bool isTreeDirty (const Node& node) const;
    bool hasActiveAnimations() const;

    GpuBackend& backend;
    std::vector<Widget*> animatingWidgets;
    double lastFrameTimeMs = 0.0;
    int frameCount = 0;
    int skippedFrames = 0;
    bool forcePaint = true;  // Always paint first frame
};

} // namespace gfx
} // namespace dc
