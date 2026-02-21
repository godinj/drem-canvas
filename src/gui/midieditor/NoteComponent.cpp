#include "NoteComponent.h"

namespace dc
{

NoteComponent::NoteComponent (int note, double start, double length, int vel)
    : noteNumber (note),
      startBeat (start),
      lengthInBeats (length),
      velocity (vel)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void NoteComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Velocity-based colour: blend from dark blue (low) to bright cyan (high)
    float velocityNorm = static_cast<float> (velocity) / 127.0f;
    auto noteColour = juce::Colour::fromHSV (0.55f,                            // hue: blue-cyan
                                              0.6f + 0.3f * (1.0f - velocityNorm), // less saturated at higher velocity
                                              0.4f + 0.6f * velocityNorm,          // brighter at higher velocity
                                              1.0f);

    // Fill the note rectangle with rounded corners
    g.setColour (noteColour);
    g.fillRoundedRectangle (bounds.reduced (0.5f), 2.0f);

    // Draw selected outline
    if (selected)
    {
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.5f);
    }
    else
    {
        g.setColour (noteColour.brighter (0.3f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 0.5f);
    }

    // Draw resize handle at right edge
    auto handleBounds = bounds.removeFromRight (static_cast<float> (resizeHandleWidth));
    g.setColour (noteColour.brighter (0.5f));
    g.fillRect (handleBounds.reduced (0.0f, 1.0f));
}

void NoteComponent::mouseDown (const juce::MouseEvent& e)
{
    dragStart = e.getPosition();
    originalStartBeat = startBeat;
    originalLength = lengthInBeats;

    // Check if clicking on resize handle
    resizing = (e.x >= getWidth() - resizeHandleWidth);

    if (resizing)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void NoteComponent::mouseDrag (const juce::MouseEvent& e)
{
    auto* parent = getParentComponent();
    if (parent == nullptr)
        return;

    int deltaX = e.x - dragStart.x;

    // Approximate pixels per beat from our current width and length
    double pixelsPerBeat = (lengthInBeats > 0.0)
                            ? static_cast<double> (getWidth()) / lengthInBeats
                            : 40.0;

    double beatDelta = static_cast<double> (deltaX) / pixelsPerBeat;

    if (resizing)
    {
        // Resize: change note length
        double newLength = originalLength + beatDelta;
        if (newLength < 0.0625) // Minimum 1/16th of a beat
            newLength = 0.0625;

        lengthInBeats = newLength;
    }
    else
    {
        // Move: change start position
        double newStart = originalStartBeat + beatDelta;
        if (newStart < 0.0)
            newStart = 0.0;

        startBeat = newStart;

        // Also handle vertical drag for pitch change
        int deltaY = e.y - dragStart.y;
        int noteHeight = getHeight();
        if (noteHeight > 0)
        {
            int noteDelta = -deltaY / noteHeight; // Negative because Y increases downward
            int newNote = noteNumber + noteDelta;
            newNote = juce::jlimit (0, 127, newNote);
            noteNumber = newNote;
        }
    }

    if (onMoved)
        onMoved();
}

void NoteComponent::mouseUp (const juce::MouseEvent& /*e*/)
{
    resizing = false;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);

    if (onMoved)
        onMoved();
}

} // namespace dc
