#pragma once

#include "graphics/core/Widget.h"
#include <JuceHeader.h>

namespace dc
{
namespace ui
{

class MidiClipWidget : public gfx::Widget
{
public:
    explicit MidiClipWidget (const juce::ValueTree& clipState);

    void paint (gfx::Canvas& canvas) override;

private:
    juce::ValueTree clipState;
};

} // namespace ui
} // namespace dc
