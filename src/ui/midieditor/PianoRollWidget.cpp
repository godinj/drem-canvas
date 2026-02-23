#include "PianoRollWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

PianoRollWidget::PianoRollWidget()
{
    addChild (&keyboard);
    addChild (&scrollView);
    scrollView.setContentWidget (&noteContainer);
}

void PianoRollWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), theme.panelBackground);
}

void PianoRollWidget::resized()
{
    float w = getWidth();
    float h = getHeight();

    keyboard.setBounds (0, 0, keyboardWidth, h);
    scrollView.setBounds (keyboardWidth, 0, w - keyboardWidth, h);

    float contentWidth = pixelsPerBeat * 64.0f; // 64 beats
    float contentHeight = 128.0f * rowHeight;
    scrollView.setContentSize (contentWidth, contentHeight);

    rebuildNotes();
}

void PianoRollWidget::loadClip (const juce::ValueTree& state)
{
    if (clipState.isValid())
        clipState.removeListener (this);

    clipState = state;

    if (clipState.isValid())
        clipState.addListener (this);

    rebuildNotes();
}

void PianoRollWidget::rebuildNotes()
{
    for (auto& nw : noteWidgets)
        noteContainer.removeChild (nw.get());
    noteWidgets.clear();

    if (!clipState.isValid())
        return;

    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        auto note = clipState.getChild (i);
        if (note.getType().toString() != "NOTE")
            continue;

        int noteNum = static_cast<int> (note.getProperty ("noteNumber", 60));
        auto startBeat = static_cast<double> (note.getProperty ("startBeat", 0.0));
        auto lengthBeats = static_cast<double> (note.getProperty ("lengthBeats", 0.25));
        int velocity = static_cast<int> (note.getProperty ("velocity", 100));

        float x = static_cast<float> (startBeat * pixelsPerBeat);
        float y = (127 - noteNum) * rowHeight;
        float w = static_cast<float> (lengthBeats * pixelsPerBeat);

        auto nw = std::make_unique<NoteWidget>();
        nw->setNoteNumber (noteNum);
        nw->setVelocity (velocity);
        nw->setBounds (x, y, w, rowHeight - 1.0f);
        noteContainer.addChild (nw.get());
        noteWidgets.push_back (std::move (nw));
    }
}

void PianoRollWidget::paintGrid (gfx::Canvas& canvas)
{
    // Grid painting is handled within the scrollView's content area
}

void PianoRollWidget::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    rebuildNotes();
}

void PianoRollWidget::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    rebuildNotes();
}

void PianoRollWidget::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    rebuildNotes();
}

} // namespace ui
} // namespace dc
