#pragma once

#include "graphics/core/Event.h"
#include "graphics/core/EventDispatch.h"
#include "MetalView.h"

namespace dc
{
namespace platform
{

class EventBridge
{
public:
    EventBridge (MetalView& view, gfx::EventDispatch& dispatch);
    ~EventBridge();

private:
    MetalView& metalView;
    gfx::EventDispatch& eventDispatch;
};

} // namespace platform
} // namespace dc
