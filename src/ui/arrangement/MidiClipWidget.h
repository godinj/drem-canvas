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
    ~MidiClipWidget() override;

    void paint (gfx::Canvas& canvas) override;

    void setClipLengthInBeats (double beats) { clipLengthBeats = beats; repaint(); }

    // ValueTree::Listener — repaint when notes or midiData change
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { repaint(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { repaint(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { repaint(); }

private:
    juce::ValueTree clipState;
    double clipLengthBeats = 4.0;
};

} // namespace ui
} // namespace dc
