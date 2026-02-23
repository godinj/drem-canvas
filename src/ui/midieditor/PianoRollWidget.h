#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ScrollViewWidget.h"
#include "PianoKeyboardWidget.h"
#include "NoteWidget.h"
#include <JuceHeader.h>
#include <vector>
#include <memory>

namespace dc
{
namespace ui
{

class PianoRollWidget : public gfx::Widget
{
public:
    PianoRollWidget();

    void paint (gfx::Canvas& canvas) override;
    void resized() override;

    void loadClip (const juce::ValueTree& midiClipState);

    // ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

private:
    void rebuildNotes();
    void paintGrid (gfx::Canvas& canvas);

    juce::ValueTree clipState;
    PianoKeyboardWidget keyboard;
    gfx::ScrollViewWidget scrollView;
    gfx::Widget noteContainer;
    std::vector<std::unique_ptr<NoteWidget>> noteWidgets;

    float pixelsPerBeat = 80.0f;
    float rowHeight = 12.0f;
    static constexpr float keyboardWidth = 60.0f;
};

} // namespace ui
} // namespace dc
