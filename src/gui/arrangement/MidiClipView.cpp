#include "MidiClipView.h"

namespace dc
{

MidiClipView::MidiClipView() {}

void MidiClipView::setMidiSequence (const juce::MidiMessageSequence& seq)
{
    sequence = seq;
    repaint();
}

void MidiClipView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fill background with a darker shade of the clip colour
    g.setColour (clipColour.darker (0.6f));
    g.fillRoundedRectangle (bounds, 3.0f);

    if (sequence.getNumEvents() == 0)
    {
        // Draw clip border and return
        g.setColour (clipColour);
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
        const auto& msg = sequence.getEventPointer (i)->message;

        if (msg.isNoteOn())
        {
            minTime = juce::jmin (minTime, msg.getTimeStamp());
            maxTime = juce::jmax (maxTime, msg.getTimeStamp());
            minNote = juce::jmin (minNote, msg.getNoteNumber());
            maxNote = juce::jmax (maxNote, msg.getNoteNumber());
        }
        else if (msg.isNoteOff())
        {
            maxTime = juce::jmax (maxTime, msg.getTimeStamp());
        }
    }

    if (maxTime <= minTime)
        maxTime = minTime + 1.0;

    // Add padding to note range
    minNote = juce::jmax (0, minNote - 1);
    maxNote = juce::jmin (127, maxNote + 1);
    if (maxNote <= minNote)
        maxNote = minNote + 1;

    double timeRange = maxTime - minTime;
    int noteRange = maxNote - minNote;

    // Padding inside the clip
    auto drawArea = bounds.reduced (3.0f, 2.0f);

    // Draw each note as a small horizontal line
    // First, build matched pairs
    juce::MidiMessageSequence workingSeq (sequence);
    workingSeq.updateMatchedPairs();

    g.setColour (clipColour.brighter (0.3f));

    for (int i = 0; i < workingSeq.getNumEvents(); ++i)
    {
        const auto* event = workingSeq.getEventPointer (i);
        const auto& msg = event->message;

        if (! msg.isNoteOn())
            continue;

        double noteStart = msg.getTimeStamp();
        double noteEnd = noteStart + 0.25; // Default quarter beat length

        if (event->noteOffObject != nullptr)
            noteEnd = event->noteOffObject->message.getTimeStamp();

        int noteNum = msg.getNoteNumber();

        // Map to pixel coordinates
        float x1 = drawArea.getX() + static_cast<float> ((noteStart - minTime) / timeRange) * drawArea.getWidth();
        float x2 = drawArea.getX() + static_cast<float> ((noteEnd - minTime) / timeRange) * drawArea.getWidth();
        float y = drawArea.getBottom() - static_cast<float> (noteNum - minNote) / static_cast<float> (noteRange) * drawArea.getHeight();

        float noteHeight = juce::jmax (1.0f, drawArea.getHeight() / static_cast<float> (noteRange) * 0.7f);

        g.fillRect (x1, y - noteHeight * 0.5f,
                    juce::jmax (1.0f, x2 - x1), noteHeight);
    }

    // Draw clip border
    g.setColour (clipColour);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
}

} // namespace dc
