#include "Renderer.h"
#include "include/core/SkCanvas.h"
#include <chrono>
#include <algorithm>

namespace dc
{
namespace gfx
{

Renderer::Renderer (MetalBackend& b)
    : backend (b)
{
}

void Renderer::renderFrame (Widget& rootWidget)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Get timestamp for animations
    double timestampMs = std::chrono::duration<double, std::milli> (
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    // Phase 1: Animation tick (always runs — may mark widgets dirty)
    animationTick (timestampMs);

    // Phase 2: Check if we can skip this frame entirely
    // If nothing is dirty, no animations are active, and we don't need a forced paint,
    // skip the expensive GPU work.
    if (!forcePaint && !isTreeDirty (rootWidget) && !hasActiveAnimations())
    {
        skippedFrames++;
        return;
    }

    forcePaint = false;

    auto surface = backend.beginFrame();
    if (!surface)
        return;

    SkCanvas* skCanvas = surface->getCanvas();
    float scale = backend.getScale();

    // Scale for HiDPI — MTKView.drawableSize is in physical pixels,
    // but our widget coordinates are in logical points.
    skCanvas->save();
    skCanvas->scale (scale, scale);

    // Phase 3: Layout pass (top-down)
    layoutPass (rootWidget);

    // Phase 4: Paint pass (depth-first)
    Canvas canvas (skCanvas);
    paintPass (canvas, rootWidget, 1.0f);

    skCanvas->restore();

    backend.endFrame (surface);

    // Track frame time
    auto endTime = std::chrono::high_resolution_clock::now();
    lastFrameTimeMs = std::chrono::duration<double, std::milli> (endTime - startTime).count();
    frameCount++;
}

void Renderer::addAnimatingWidget (Widget* w)
{
    if (std::find (animatingWidgets.begin(), animatingWidgets.end(), w) == animatingWidgets.end())
        animatingWidgets.push_back (w);
}

void Renderer::removeAnimatingWidget (Widget* w)
{
    animatingWidgets.erase (
        std::remove (animatingWidgets.begin(), animatingWidgets.end(), w),
        animatingWidgets.end());
}

void Renderer::animationTick (double timestampMs)
{
    for (auto* w : animatingWidgets)
    {
        if (w->isAnimating())
            w->animationTick (timestampMs);
    }
}

void Renderer::layoutPass (Widget& widget)
{
    for (auto* child : widget.getChildren())
    {
        if (auto* childWidget = dynamic_cast<Widget*> (child))
            layoutPass (*childWidget);
    }
}

void Renderer::paintPass (Canvas& canvas, Node& node, float parentOpacity)
{
    if (!node.isVisible())
        return;

    float effectiveOpacity = node.getOpacity() * parentOpacity;
    if (effectiveOpacity <= 0.0f)
        return;

    const Rect& bounds = node.getBounds();

    canvas.save();
    canvas.translate (bounds.x, bounds.y);

    if (!node.getTransform().isIdentity())
    {
        const auto& t = node.getTransform();
        SkMatrix m;
        m.setAll (t.a, t.b, t.tx, t.c, t.d, t.ty, 0, 0, 1);
        canvas.getSkCanvas()->concat (m);
    }

    // Use texture cache if available and clean — skip entire subtree
    if (node.useTextureCache && !node.isDirty() && node.hasTextureCache())
    {
        paintCached (canvas, node, effectiveOpacity);
    }
    else
    {
        // Clip to local bounds
        canvas.clipRect (Rect (0, 0, bounds.width, bounds.height));

        // Paint this node
        node.paint (canvas);

        // Paint children
        for (auto* child : node.getChildren())
            paintPass (canvas, *child, effectiveOpacity);

        // Paint overlay
        node.paintOverChildren (canvas);

        // Update texture cache if enabled
        if (node.useTextureCache)
        {
            int w = static_cast<int> (bounds.width * backend.getScale());
            int h = static_cast<int> (bounds.height * backend.getScale());
            if (w > 0 && h > 0)
            {
                auto offscreen = backend.createOffscreenSurface (w, h);
                if (offscreen)
                {
                    SkCanvas* offCanvas = offscreen->getCanvas();
                    offCanvas->scale (backend.getScale(), backend.getScale());
                    Canvas offCanvasWrapper (offCanvas);
                    node.paint (offCanvasWrapper);
                    for (auto* child : node.getChildren())
                        paintPass (offCanvasWrapper, *child, 1.0f);
                    node.paintOverChildren (offCanvasWrapper);
                    node.setCachedSurface (offscreen);
                }
            }
        }

        node.clearDirty();
    }

    canvas.restore();
}

void Renderer::paintCached (Canvas& canvas, Node& node, float parentOpacity)
{
    auto surface = node.getCachedSurface();
    if (surface)
    {
        auto image = surface->makeImageSnapshot();
        if (image)
        {
            float invScale = 1.0f / backend.getScale();
            canvas.getSkCanvas()->save();
            canvas.getSkCanvas()->scale (invScale, invScale);
            canvas.drawImage (image, 0, 0);
            canvas.getSkCanvas()->restore();
        }
    }
}

bool Renderer::isTreeDirty (const Node& node) const
{
    if (node.isDirty())
        return true;

    for (auto* child : node.getChildren())
    {
        if (isTreeDirty (*child))
            return true;
    }

    return false;
}

bool Renderer::hasActiveAnimations() const
{
    for (auto* w : animatingWidgets)
    {
        if (w->isAnimating())
            return true;
    }
    return false;
}

} // namespace gfx
} // namespace dc
