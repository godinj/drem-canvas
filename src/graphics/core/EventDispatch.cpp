#include "EventDispatch.h"

namespace dc
{
namespace gfx
{

EventDispatch::EventDispatch (Widget& root)
    : rootWidget (root)
{
}

Widget* EventDispatch::findWidgetAt (float x, float y)
{
    Node* node = rootWidget.findNodeAt (Point (x, y));
    // Walk up to find nearest Widget (Node may not be a Widget)
    while (node)
    {
        if (auto* widget = dynamic_cast<Widget*> (node))
            return widget;
        node = node->getParent();
    }
    return &rootWidget;
}

MouseEvent EventDispatch::transformEvent (const MouseEvent& e, Widget* target)
{
    MouseEvent local = e;
    if (target)
    {
        Point globalPt (e.x, e.y);
        Point localPt = target->globalToLocal (globalPt);
        local.x = localPt.x;
        local.y = localPt.y;
    }
    return local;
}

WheelEvent EventDispatch::transformWheelEvent (const WheelEvent& e, Widget* target)
{
    WheelEvent local = e;
    if (target)
    {
        Point globalPt (e.x, e.y);
        Point localPt = target->globalToLocal (globalPt);
        local.x = localPt.x;
        local.y = localPt.y;
    }
    return local;
}

void EventDispatch::dispatchMouseDown (const MouseEvent& e)
{
    Widget* target = findWidgetAt (e.x, e.y);
    pressedWidget = target;

    if (target && target->isFocusable())
        Widget::setCurrentFocus (target);

    if (target)
    {
        auto local = transformEvent (e, target);

        if (e.clickCount >= 2)
            target->mouseDoubleClick (local);
        else
            target->mouseDown (local);
    }
}

void EventDispatch::dispatchMouseDrag (const MouseEvent& e)
{
    if (pressedWidget)
    {
        auto local = transformEvent (e, pressedWidget);
        pressedWidget->mouseDrag (local);
    }
}

void EventDispatch::dispatchMouseUp (const MouseEvent& e)
{
    if (pressedWidget)
    {
        auto local = transformEvent (e, pressedWidget);
        pressedWidget->mouseUp (local);
        pressedWidget = nullptr;
    }
}

void EventDispatch::dispatchMouseMove (const MouseEvent& e)
{
    Widget* target = findWidgetAt (e.x, e.y);

    if (target != hoveredWidget)
    {
        if (hoveredWidget)
        {
            auto exitEvent = transformEvent (e, hoveredWidget);
            hoveredWidget->mouseExit (exitEvent);
        }
        hoveredWidget = target;
        if (hoveredWidget)
        {
            auto enterEvent = transformEvent (e, hoveredWidget);
            hoveredWidget->mouseEnter (enterEvent);
        }
    }

    if (target)
    {
        auto local = transformEvent (e, target);
        target->mouseMove (local);
    }
}

void EventDispatch::dispatchWheel (const WheelEvent& e)
{
    Widget* target = findWidgetAt (e.x, e.y);

    // Bubble up the widget tree until someone handles the event
    while (target)
    {
        auto local = transformWheelEvent (e, target);
        if (target->mouseWheel (local))
            return;

        auto* parent = target->getParent();
        target = parent ? dynamic_cast<Widget*> (parent) : nullptr;
    }
}

void EventDispatch::dispatchKeyDown (const KeyEvent& e)
{
    Widget* focus = Widget::getCurrentFocus();
    if (focus)
    {
        if (focus->keyDown (e))
            return;
    }
    // Fall through to root widget if not consumed
    rootWidget.keyDown (e);
}

void EventDispatch::dispatchKeyUp (const KeyEvent& e)
{
    Widget* focus = Widget::getCurrentFocus();
    if (focus)
    {
        if (focus->keyUp (e))
            return;
    }
    rootWidget.keyUp (e);
}

} // namespace gfx
} // namespace dc
