#pragma once

#include "Node.h"
#include "Event.h"
#include <JuceHeader.h>

namespace dc
{
namespace gfx
{

class Widget : public Node, public juce::ValueTree::Listener
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

    void setId (const juce::String& newId) { widgetId = newId; }
    const juce::String& getId() const { return widgetId; }

    // ─── ValueTree::Listener overrides (empty defaults) ──

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    // ─── Global focus management (set by EventDispatch) ──

    static Widget* getCurrentFocus();
    static void setCurrentFocus (Widget* w);

protected:
    bool focused = false;
    bool focusable = false;
    bool animating = false;
    juce::String widgetId;

private:
    static Widget* globalFocusedWidget;
};

} // namespace gfx
} // namespace dc
