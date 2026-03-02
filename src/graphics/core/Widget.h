#pragma once

#include "Node.h"
#include "Event.h"
#include "dc/model/PropertyTree.h"

namespace dc
{
namespace gfx
{

class Widget : public Node, public PropertyTree::Listener
{
public:
    Widget() = default;
    ~Widget() override = default;

    // ─── Mouse events ────────────────────────────────────

    virtual void mouseDown (const MouseEvent& e) {}
    virtual void mouseDrag (const MouseEvent& e) {}
    virtual void mouseUp (const MouseEvent& e) {}
    virtual void mouseMove (const MouseEvent& e) {}
    virtual void mouseEnter (const MouseEvent& e) {}
    virtual void mouseExit (const MouseEvent& e) {}
    virtual bool mouseWheel (const WheelEvent& e) { return false; }
    virtual void mouseDoubleClick (const MouseEvent& e) {}

    // ─── Keyboard events ─────────────────────────────────

    virtual bool keyDown (const KeyEvent& e) { return false; }
    virtual bool keyUp (const KeyEvent& e) { return false; }

    // ─── Focus ───────────────────────────────────────────

    void grabFocus();
    void releaseFocus();
    bool hasFocus() const { return focused; }

    bool isFocusable() const { return focusable; }
    void setFocusable (bool f) { focusable = f; }

    // ─── Layout ──────────────────────────────────────────

    virtual void resized() {}

    void setBounds (const Rect& newBounds)
    {
        if (getBounds() != newBounds)
        {
            Node::setBounds (newBounds);
            resized();
        }
    }

    void setBounds (float x, float y, float w, float h)
    {
        setBounds (Rect (x, y, w, h));
    }

    // ─── Repaint ─────────────────────────────────────────

    void repaint() { invalidate(); }

    // ─── Animation ───────────────────────────────────────

    bool isAnimating() const { return animating; }
    void setAnimating (bool a) { animating = a; }
    virtual void animationTick (double timestampMs) {}

    // ─── Identification ──────────────────────────────────

    void setId (const std::string& newId) { widgetId = newId; }
    const std::string& getId() const { return widgetId; }

    // ─── PropertyTree::Listener overrides (empty defaults) ──

    void propertyChanged (PropertyTree&, PropertyId) override {}
    void childAdded (PropertyTree&, PropertyTree&) override {}
    void childRemoved (PropertyTree&, PropertyTree&, int) override {}
    void childOrderChanged (PropertyTree&, int, int) override {}
    void parentChanged (PropertyTree&) override {}

    // ─── Global focus management (set by EventDispatch) ──

    static Widget* getCurrentFocus();
    static void setCurrentFocus (Widget* w);

protected:
    bool focused = false;
    bool focusable = false;
    bool animating = false;
    std::string widgetId;

private:
    static Widget* globalFocusedWidget;
};

} // namespace gfx
} // namespace dc
