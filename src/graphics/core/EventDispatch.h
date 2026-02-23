#pragma once

#include "Event.h"
#include "Widget.h"

namespace dc
{
namespace gfx
{

class EventDispatch
{
public:
    explicit EventDispatch (Widget& root);

    void dispatchMouseDown (const MouseEvent& e);
    void dispatchMouseDrag (const MouseEvent& e);
    void dispatchMouseUp (const MouseEvent& e);
    void dispatchMouseMove (const MouseEvent& e);
    void dispatchWheel (const WheelEvent& e);
    void dispatchKeyDown (const KeyEvent& e);
    void dispatchKeyUp (const KeyEvent& e);

private:
    Widget* findWidgetAt (float x, float y);
    MouseEvent transformEvent (const MouseEvent& e, Widget* target);
    WheelEvent transformWheelEvent (const WheelEvent& e, Widget* target);

    Widget& rootWidget;
    Widget* hoveredWidget = nullptr;
    Widget* pressedWidget = nullptr;
    float pressOffsetX = 0.0f;
    float pressOffsetY = 0.0f;
};

} // namespace gfx
} // namespace dc
