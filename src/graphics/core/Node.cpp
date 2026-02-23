#include "Node.h"

namespace dc
{
namespace gfx
{

Node::~Node()
{
    removeAllChildren();

    if (parent)
        parent->removeChild (this);
}

void Node::addChild (Node* child)
{
    if (child == nullptr || child == this)
        return;

    if (child->parent == this)
        return;

    if (child->parent)
        child->parent->removeChild (child);

    child->parent = this;
    children.push_back (child);
    invalidate();
}

void Node::removeChild (Node* child)
{
    auto it = std::find (children.begin(), children.end(), child);
    if (it != children.end())
    {
        (*it)->parent = nullptr;
        children.erase (it);
        invalidate();
    }
}

void Node::removeAllChildren()
{
    for (auto* child : children)
        child->parent = nullptr;
    children.clear();
    invalidate();
}

Node* Node::getChild (int index) const
{
    if (index >= 0 && index < static_cast<int> (children.size()))
        return children[static_cast<size_t> (index)];
    return nullptr;
}

void Node::setBounds (const Rect& newBounds)
{
    if (bounds != newBounds)
    {
        bounds = newBounds;
        invalidateCache();
    }
}

void Node::invalidate()
{
    dirty = true;
    if (parent)
        parent->invalidate();
}

bool Node::hitTest (Point localPoint) const
{
    return Rect (0, 0, bounds.width, bounds.height).contains (localPoint);
}

Node* Node::findNodeAt (Point parentPoint)
{
    if (!visible)
        return nullptr;

    Point localPoint = parentToLocal (parentPoint);

    if (!hitTest (localPoint))
        return nullptr;

    // Check children in reverse order (front to back)
    for (int i = static_cast<int> (children.size()) - 1; i >= 0; --i)
    {
        Node* hit = children[static_cast<size_t> (i)]->findNodeAt (localPoint);
        if (hit)
            return hit;
    }

    return this;
}

Point Node::localToParent (Point p) const
{
    Point translated = { p.x + bounds.x, p.y + bounds.y };
    if (!transform.isIdentity())
        translated = transform.apply (translated);
    return translated;
}

Point Node::parentToLocal (Point p) const
{
    // Simple inverse for translation-only (most common case)
    return { p.x - bounds.x, p.y - bounds.y };
}

Point Node::localToGlobal (Point p) const
{
    Point result = localToParent (p);
    if (parent)
        result = parent->localToGlobal (result);
    return result;
}

Point Node::globalToLocal (Point p) const
{
    if (parent)
        p = parent->globalToLocal (p);
    return parentToLocal (p);
}

} // namespace gfx
} // namespace dc
