#include "MidiClipWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "model/MidiClip.h"

namespace dc
{
namespace ui
{

MidiClipWidget::MidiClipWidget (const juce::ValueTree& state)
    : clipState (state)
{
    clipState.addListener (this);
}

MidiClipWidget::~MidiClipWidget()
{
    clipState.removeListener (this);
}

void MidiClipWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();

    // Clip background
    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff2a3a4a));

    // First try NOTE children (available when piano roll has expanded them)
    bool hasNoteChildren = false;
    for (int i = 0; i < clipState.getNumChildren(); ++i)
    {
        if (clipState.getChild (i).hasType (juce::Identifier ("NOTE")))
        {
            hasNoteChildren = true;
            break;
        }
    }

    if (hasNoteChildren)
    {
        for (int i = 0; i < clipState.getNumChildren(); ++i)
        {
            auto note = clipState.getChild (i);
            if (! note.hasType (juce::Identifier ("NOTE")))
                continue;

            int noteNum = static_cast<int> (note.getProperty ("noteNumber", 60));
            auto startBeat = static_cast<double> (note.getProperty ("startBeat", 0.0));
            auto lengthBeats = static_cast<double> (note.getProperty ("lengthBeats", 0.25));

            float noteY = h - (static_cast<float> (noteNum) / 127.0f) * h;
            float noteX = static_cast<float> (startBeat / clipLengthBeats) * w;
            float noteW = std::max (2.0f, static_cast<float> (lengthBeats / clipLengthBeats) * w);
            float noteH = std::max (1.0f, h / 64.0f);

            canvas.fillRect (Rect (noteX, noteY - noteH * 0.5f, noteW, noteH), theme.accent);
        }
    }
    else
    {
        // Decode notes directly from the base64 midiData blob
        MidiClip clip (clipState);
        auto seq = clip.getMidiSequence();
        seq.updateMatchedPairs();

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto* event = seq.getEventPointer (i);
            const auto& msg = event->message;

            if (! msg.isNoteOn())
                continue;

            int noteNum = msg.getNoteNumber();
            double startBeat = msg.getTimeStamp();
            double lengthBeats = 0.25;

            if (event->noteOffObject != nullptr)
                lengthBeats = event->noteOffObject->message.getTimeStamp() - startBeat;
            if (lengthBeats <= 0.0)
                lengthBeats = 0.25;

            float noteY = h - (static_cast<float> (noteNum) / 127.0f) * h;
            float noteX = static_cast<float> (startBeat / clipLengthBeats) * w;
            float noteW = std::max (2.0f, static_cast<float> (lengthBeats / clipLengthBeats) * w);
            float noteH = std::max (1.0f, h / 64.0f);

            canvas.fillRect (Rect (noteX, noteY - noteH * 0.5f, noteW, noteH), theme.accent);
        }
    }

    // Border
    canvas.strokeRect (Rect (0, 0, w, h), theme.outlineColor, 1.0f);
}

} // namespace ui
} // namespace dc
