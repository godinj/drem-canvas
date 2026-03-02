#pragma once

#include "graphics/core/Widget.h"
#include "dc/model/PropertyTree.h"

namespace dc
{
namespace ui
{

class MidiClipWidget : public gfx::Widget
{
public:
    explicit MidiClipWidget (const PropertyTree& clipState);
    ~MidiClipWidget() override;

    void paint (gfx::Canvas& canvas) override;

    void setClipLengthInBeats (double beats) { clipLengthBeats = beats; repaint(); }
    void setTrimOffsetBeats (double beats) { trimOffsetBeats = beats; repaint(); }

    // PropertyTree::Listener — repaint when notes or midiData change
    void childAdded (PropertyTree&, PropertyTree&) override { repaint(); }
    void childRemoved (PropertyTree&, PropertyTree&, int) override { repaint(); }
    void propertyChanged (PropertyTree&, PropertyId) override { repaint(); }

private:
    PropertyTree clipState;
    double clipLengthBeats = 4.0;
    double trimOffsetBeats = 0.0;
};

} // namespace ui
} // namespace dc
