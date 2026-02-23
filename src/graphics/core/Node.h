#pragma once

#include "Types.h"
#include "include/core/SkSurface.h"
#include <vector>
#include <algorithm>

namespace dc
{
namespace gfx
{

class Canvas;

class Node
{
public:
    Node() = default;
    virtual ~Node();

    // ─── Tree structure ──────────────────────────────────

    void addChild (Node* child);
    void removeChild (Node* child);
    void removeAllChildren();
    Node* getParent() const { return parent; }

    int getNumChildren() const { return static_cast<int> (children.size()); }
    Node* getChild (int index) const;
    const std::vector<Node*>& getChildren() const { return children; }

    // ─── Bounds ──────────────────────────────────────────

    void setBounds (const Rect& newBounds);
    const Rect& getBounds() const { return bounds; }

    float getX() const { return bounds.x; }
    float getY() const { return bounds.y; }
    float getWidth() const { return bounds.width; }
    float getHeight() const { return bounds.height; }

    // ─── Transform & visibility ──────────────────────────

    void setTransform (const Transform2D& t) { transform = t; invalidate(); }
    const Transform2D& getTransform() const { return transform; }

    void setOpacity (float o) { opacity = o; invalidate(); }
    float getOpacity() const { return opacity; }

    void setVisible (bool v) { visible = v; invalidate(); }
    bool isVisible() const { return visible; }

    // ─── Dirty tracking ──────────────────────────────────

    void invalidate();
    bool isDirty() const { return dirty; }
    void clearDirty() { dirty = false; }

    // ─── Texture cache ───────────────────────────────────

    bool hasTextureCache() const { return cachedSurface != nullptr; }
    sk_sp<SkSurface> getCachedSurface() const { return cachedSurface; }
    void setCachedSurface (sk_sp<SkSurface> surface) { cachedSurface = std::move (surface); }
    void invalidateCache() { cachedSurface.reset(); invalidate(); }

    bool useTextureCache = false;

    // ─── Painting ────────────────────────────────────────

    virtual void paint (Canvas& canvas) {}
    virtual void paintOverChildren (Canvas& canvas) {}

    // ─── Hit testing ─────────────────────────────────────

    virtual bool hitTest (Point localPoint) const;
    Node* findNodeAt (Point parentPoint);

    // ─── Coordinate conversion ───────────────────────────

    Point localToParent (Point p) const;
    Point parentToLocal (Point p) const;
    Point localToGlobal (Point p) const;
    Point globalToLocal (Point p) const;

protected:
    Rect bounds;
    Transform2D transform;
    float opacity = 1.0f;
    bool visible = true;
    bool dirty = true;

    Node* parent = nullptr;
    std::vector<Node*> children;

    sk_sp<SkSurface> cachedSurface;
};

} // namespace gfx
} // namespace dc
