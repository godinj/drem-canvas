#include "MidiClipView.h"
#include "gui/common/ColourBridge.h"
#include <algorithm>
#include <limits>

using dc::bridge::toJuce;

namespace dc
{

MidiClipView::MidiClipView() {}

void MidiClipView::setMidiSequence (const dc::MidiSequence& seq)
{
    sequence = seq;
    repaint();
}

void MidiClipView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fill background with a darker shade of the clip colour
    g.setColour (toJuce (clipColour.darker (0.6f)));
    g.fillRoundedRectangle (bounds, 3.0f);

    if (sequence.getNumEvents() == 0)
    {
        // Draw clip border and return
        g.setColour (toJuce (clipColour));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
        return;
    }

    // Find time range and note range for mapping
    double minTime = std::numeric_limits<double>::max();
    double maxTime = 0.0;
    int minNote = 127;
    int maxNote = 0;

    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const auto& evt = sequence.getEvent (i);
        const auto& msg = evt.message;

        if (msg.isNoteOn())
        {
            minTime = std::min (minTime, evt.timeInBeats);
            maxTime = std::max (maxTime, evt.timeInBeats);
            minNote = std::min (minNote, msg.getNoteNumber());
            maxNote = std::max (maxNote, msg.getNoteNumber());
        }
        else if (msg.isNoteOff())
        {
            maxTime = std::max (maxTime, evt.timeInBeats);
        }
    }

    if (maxTime <= minTime)
        maxTime = minTime + 1.0;

    // Add padding to note range
    minNote = std::max (0, minNote - 1);
    maxNote = std::min (127, maxNote + 1);
    if (maxNote <= minNote)
        maxNote = minNote + 1;

    double timeRange = maxTime - minTime;
    int noteRange = maxNote - minNote;

    // Padding inside the clip
    auto drawArea = bounds.reduced (3.0f, 2.0f);

    // Build matched pairs for note-on/note-off pairing
    dc::MidiSequence workingSeq = sequence;
    workingSeq.updateMatchedPairs();

    g.setColour (toJuce (clipColour.brighter (0.3f)));

    for (int i = 0; i < workingSeq.getNumEvents(); ++i)
    {
        const auto& evt = workingSeq.getEvent (i);
        const auto& msg = evt.message;

        if (! msg.isNoteOn())
            continue;

        double noteStart = evt.timeInBeats;
        double noteEnd = noteStart + 0.25; // Default quarter beat length

        if (evt.matchedPairIndex >= 0)
            noteEnd = workingSeq.getEvent (evt.matchedPairIndex).timeInBeats;

        int noteNum = msg.getNoteNumber();

        // Map to pixel coordinates
        float x1 = drawArea.getX() + static_cast<float> ((noteStart - minTime) / timeRange) * drawArea.getWidth();
        float x2 = drawArea.getX() + static_cast<float> ((noteEnd - minTime) / timeRange) * drawArea.getWidth();
        float y = drawArea.getBottom() - static_cast<float> (noteNum - minNote) / static_cast<float> (noteRange) * drawArea.getHeight();

        float noteHeight = std::max (1.0f, drawArea.getHeight() / static_cast<float> (noteRange) * 0.7f);

        g.fillRect (x1, y - noteHeight * 0.5f,
                    std::max (1.0f, x2 - x1), noteHeight);
    }

    // Draw clip border
    g.setColour (toJuce (clipColour));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
}

} // namespace dc
