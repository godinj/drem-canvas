#include "Widget.h"

namespace dc
{
namespace gfx
{

Widget* Widget::globalFocusedWidget = nullptr;

void Widget::grabFocus()
{
    if (!focusable)
        return;

    if (globalFocusedWidget && globalFocusedWidget != this)
        globalFocusedWidget->releaseFocus();

    focused = true;
    globalFocusedWidget = this;
    repaint();
}

void Widget::releaseFocus()
{
    focused = false;
    if (globalFocusedWidget == this)
        globalFocusedWidget = nullptr;
    repaint();
}

Widget* Widget::getCurrentFocus()
{
    return globalFocusedWidget;
}

void Widget::setCurrentFocus (Widget* w)
{
    if (globalFocusedWidget && globalFocusedWidget != w)
        globalFocusedWidget->releaseFocus();

    if (w)
        w->grabFocus();
    else
        globalFocusedWidget = nullptr;
}

} // namespace gfx
} // namespace dc
