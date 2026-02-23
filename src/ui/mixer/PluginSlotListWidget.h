#pragma once

#include "graphics/core/Widget.h"
#include <JuceHeader.h>
#include <functional>
#include <vector>
#include <string>

namespace dc
{
namespace ui
{

class PluginSlotListWidget : public gfx::Widget
{
public:
    PluginSlotListWidget();

    void paint (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;

    struct PluginSlot
    {
        std::string name;
        bool bypassed = false;
    };

    void setSlots (const std::vector<PluginSlot>& slots);

    std::function<void (int)> onSlotClicked;        // Left click opens editor
    std::function<void (int)> onSlotRightClicked;   // Right click opens context menu

private:
    std::vector<PluginSlot> slots;
    static constexpr float slotHeight = 20.0f;
};

} // namespace ui
} // namespace dc
