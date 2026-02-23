#include "VimStatusBarWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"
#include "model/Track.h"

namespace dc
{
namespace ui
{

VimStatusBarWidget::VimStatusBarWidget (VimEngine& e, VimContext& c,
                                        Arrangement& a, TransportController& t)
    : engine (e), context (c), arrangement (a), transport (t)
{
    engine.addListener (this);
    setAnimating (true);
}

VimStatusBarWidget::~VimStatusBarWidget()
{
    engine.removeListener (this);
}

void VimStatusBarWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& fm = FontManager::getInstance();
    auto& font = fm.getDefaultFont();
    float h = getHeight();
    float totalWidth = getWidth();

    // Background
    canvas.fillRect (Rect (0, 0, totalWidth, h), Color::fromARGB (0xff181825));

    // ── Command mode — full-width command line
    if (engine.getMode() == VimEngine::Command)
    {
        auto text = ":" + engine.getCommandBuffer().toStdString();
        canvas.drawText (text, 6.0f, h * 0.5f + 5.0f, font, Color::fromARGB (0xffcdd6f4));
        return;
    }

    float x = 0.0f;

    // ── Mode segment
    float modeWidth = 160.0f;
    bool isNormal = engine.getMode() == VimEngine::Normal;
    Color modeColor = isNormal ? Color::fromARGB (0xff50c878) : Color::fromARGB (0xff4a9eff);
    canvas.fillRect (Rect (x, 0, modeWidth, h), modeColor);

    auto modeText = isNormal ? "-- NORMAL --" : "-- INSERT --";
    canvas.drawText (modeText, x + 6.0f, h * 0.5f + 5.0f, font, Color::fromARGB (0xff181825));
    x += modeWidth;

    // ── Pending state indicator
    if (engine.hasPendingState())
    {
        float pendingWidth = 80.0f;
        canvas.drawText (engine.getPendingDisplay().toStdString(),
                         x + 4.0f, h * 0.5f + 5.0f, font, Color::fromARGB (0xffffcc00));
        x += pendingWidth;
    }

    // ── Context panel segment
    float panelWidth = 100.0f;
    canvas.drawText (context.getPanelName().toStdString(),
                     x + 6.0f, h * 0.5f + 5.0f, font, Color::fromARGB (0xffcdd6f4));
    x += panelWidth;

    // ── Cursor info segment
    int trackIdx = arrangement.getSelectedTrackIndex();
    std::string cursorText;

    if (trackIdx >= 0 && trackIdx < arrangement.getNumTracks())
    {
        Track track = arrangement.getTrack (trackIdx);
        cursorText = "T" + std::to_string (trackIdx + 1) + ":"
                   + track.getName().toStdString()
                   + " C" + std::to_string (context.getSelectedClipIndex() + 1);
    }
    else
    {
        cursorText = "No track selected";
    }

    canvas.drawText (cursorText, x + 6.0f, h * 0.5f + 5.0f, font, Color::fromARGB (0xffa6adc8));

    // ── Playhead info (right-aligned)
    auto timeStr = transport.getTimeString().toStdString();
    Rect rightArea (totalWidth - 200.0f, 0, 200.0f, h);
    canvas.drawTextRight (timeStr, rightArea, fm.getMonoFont(), Color::fromARGB (0xffa6adc8));
}

void VimStatusBarWidget::animationTick (double /*timestampMs*/)
{
    repaint();
}

void VimStatusBarWidget::vimModeChanged (VimEngine::Mode)
{
    repaint();
}

void VimStatusBarWidget::vimContextChanged()
{
    repaint();
}

} // namespace ui
} // namespace dc
