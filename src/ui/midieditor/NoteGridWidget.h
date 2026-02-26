#pragma once

#include "graphics/core/Widget.h"
#include <JuceHeader.h>

namespace dc
{
namespace ui
{

class NoteGridWidget : public gfx::Widget
{
public:
    NoteGridWidget();

    void paint (gfx::Canvas& canvas) override;
    void paintOverChildren (gfx::Canvas& canvas) override;
    void mouseDown (const gfx::MouseEvent& e) override;
    void mouseDrag (const gfx::MouseEvent& e) override;
    void mouseUp (const gfx::MouseEvent& e) override;

    // Tool callbacks (wired by PianoRollWidget)
    std::function<void (int noteNumber, double beat)> onDrawNote;
    std::function<void (int noteNumber, double beat)> onEraseNote;
    std::function<void (float, float, float, float)> onRubberBandSelect; // startX, startY, endX, endY
    std::function<void()> onEmptyClick; // click on empty area in select mode

    // Coordinate helpers
    float beatsToX (double beats) const;
    double xToBeats (float x) const;
    float noteToY (int noteNumber) const;
    int yToNote (float y) const;

    // Grid properties
    void setPixelsPerBeat (float ppb) { pixelsPerBeat = ppb; repaint(); }
    float getPixelsPerBeat() const { return pixelsPerBeat; }

    void setRowHeight (float rh) { rowHeight = rh; repaint(); }
    float getRowHeight() const { return rowHeight; }

    void setGridDivision (int div) { gridDivision = div; repaint(); }
    int getGridDivision() const { return gridDivision; }

    void setTempo (double bpm) { tempo = bpm; repaint(); }
    void setTimeSigNumerator (int num) { timeSigNumerator = num; repaint(); }

    // Tool mode (set by PianoRollWidget)
    enum ToolMode { SelectTool, DrawTool, EraseTool };
    void setToolMode (ToolMode t) { toolMode = t; }

private:
    bool isBlackKey (int note) const;

    float pixelsPerBeat = 80.0f;
    float rowHeight = 12.0f;
    int gridDivision = 4; // subdivisions per beat
    double tempo = 120.0;
    int timeSigNumerator = 4;

    ToolMode toolMode = SelectTool;
    bool isRubberBanding = false;
    float rbStartX = 0.0f, rbStartY = 0.0f;
    float rbEndX = 0.0f, rbEndY = 0.0f;
};

} // namespace ui
} // namespace dc
