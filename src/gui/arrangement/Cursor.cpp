#include "Cursor.h"

namespace dc
{

Cursor::Cursor()
{
    setInterceptsMouseClicks (false, false);
    setSize (2, 100); // Default thin width
}

void Cursor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Draw the vertical cursor line (2px wide, full height)
    g.setColour (cursorColour);
    g.fillRect (bounds.withWidth (2.0f));

    // Draw a small triangle/arrow at the top
    juce::Path arrow;
    float arrowSize = 6.0f;
    float centreX = 1.0f; // Centre of the 2px line

    arrow.addTriangle (centreX - arrowSize, 0.0f,   // top-left
                       centreX + arrowSize, 0.0f,    // top-right
                       centreX, arrowSize * 1.2f);   // bottom-centre

    g.setColour (cursorColour);
    g.fillPath (arrow);
}

} // namespace dc
