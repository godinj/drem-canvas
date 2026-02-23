#include "EventBridge.h"

namespace dc
{
namespace platform
{

EventBridge::EventBridge (MetalView& view, gfx::EventDispatch& dispatch)
    : metalView (view), eventDispatch (dispatch)
{
    metalView.onMouseDown = [this] (const gfx::MouseEvent& e)
    {
        eventDispatch.dispatchMouseDown (e);
    };

    metalView.onMouseDrag = [this] (const gfx::MouseEvent& e)
    {
        eventDispatch.dispatchMouseDrag (e);
    };

    metalView.onMouseUp = [this] (const gfx::MouseEvent& e)
    {
        eventDispatch.dispatchMouseUp (e);
    };

    metalView.onMouseMove = [this] (const gfx::MouseEvent& e)
    {
        eventDispatch.dispatchMouseMove (e);
    };

    metalView.onKeyDown = [this] (const gfx::KeyEvent& e)
    {
        eventDispatch.dispatchKeyDown (e);
    };

    metalView.onKeyUp = [this] (const gfx::KeyEvent& e)
    {
        eventDispatch.dispatchKeyUp (e);
    };

    metalView.onWheel = [this] (const gfx::WheelEvent& e)
    {
        eventDispatch.dispatchWheel (e);
    };
}

EventBridge::~EventBridge()
{
    metalView.onMouseDown = nullptr;
    metalView.onMouseDrag = nullptr;
    metalView.onMouseUp = nullptr;
    metalView.onMouseMove = nullptr;
    metalView.onKeyDown = nullptr;
    metalView.onKeyUp = nullptr;
    metalView.onWheel = nullptr;
}

} // namespace platform
} // namespace dc
