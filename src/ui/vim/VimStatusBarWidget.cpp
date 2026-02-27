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
                                        Arrangement& a, TransportController& t,
                                        const GridSystem& gs)
    : engine (e), context (c), arrangement (a), transport (t), gridSystem (gs)
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
    Color modeColor;
    const char* modeText;

    switch (engine.getMode())
    {
        case VimEngine::Normal:     modeColor = Color::fromARGB (0xff50c878); modeText = "-- NORMAL --"; break;
        case VimEngine::Insert:     modeColor = Color::fromARGB (0xff4a9eff); modeText = "-- INSERT --"; break;
        case VimEngine::Keyboard:   modeColor = Color::fromARGB (0xffff9933); modeText = "-- KEYBOARD --"; break;
        case VimEngine::PluginMenu: modeColor = Color::fromARGB (0xffcba6f7); modeText = "-- PLUGIN --"; break;
        case VimEngine::Visual:     modeColor = Color::fromARGB (0xffff9944); modeText = "-- VISUAL --"; break;
        case VimEngine::VisualLine: modeColor = Color::fromARGB (0xffff9944); modeText = "-- V-LINE --"; break;
        default:                    modeColor = Color::fromARGB (0xff50c878); modeText = "-- NORMAL --"; break;
    }

    canvas.fillRect (Rect (x, 0, modeWidth, h), modeColor);
    canvas.drawText (modeText, x + 6.0f, h * 0.5f + 5.0f, font, Color::fromARGB (0xff181825));
    x += modeWidth;

    // ── Pending state indicator / keyboard info
    if (engine.getMode() == VimEngine::Keyboard)
    {
        auto& kbState = engine.getKeyboardState();
        auto kbInfo = "Oct:" + std::to_string (kbState.baseOctave)
                    + " Vel:" + std::to_string (kbState.velocity);
        float pendingWidth = 120.0f;
        canvas.drawText (kbInfo, x + 4.0f, h * 0.5f + 5.0f, font, Color::fromARGB (0xffffcc00));
        x += pendingWidth;
    }
    else if (engine.hasPendingState())
    {
        float pendingWidth = 80.0f;
        canvas.drawText (engine.getPendingDisplay().toStdString(),
                         x + 4.0f, h * 0.5f + 5.0f, font, Color::fromARGB (0xffffcc00));
        x += pendingWidth;
    }

    // ── Context panel segment (prominent green on dark bg)
    float panelWidth = 120.0f;
    auto& theme = Theme::getDefault();
    canvas.fillRect (Rect (x, 0, panelWidth, h), Color::fromARGB (0xff202030));
    canvas.drawText (context.getPanelName().toStdString(),
                     x + 6.0f, h * 0.5f + 5.0f, font, theme.selection);
    x += panelWidth;

    // ── Breadcrumb info segment (context-dependent)
    float breadcrumbWidth = 280.0f;
    int trackIdx = arrangement.getSelectedTrackIndex();
    std::string breadcrumb;

    if (trackIdx >= 0 && trackIdx < arrangement.getNumTracks())
    {
        Track track = arrangement.getTrack (trackIdx);
        std::string trackInfo = "T" + std::to_string (trackIdx + 1) + ":"
                              + track.getName().toStdString();

        auto panel = context.getPanel();
        auto& visSel = context.getVisualSelection();
        if (panel == VimContext::Editor && visSel.active)
        {
            int minT = std::min (visSel.startTrack, visSel.endTrack) + 1;
            int maxT = std::max (visSel.startTrack, visSel.endTrack) + 1;

            if (visSel.linewise)
            {
                breadcrumb = "> T" + std::to_string (minT) + "-T" + std::to_string (maxT);
            }
            else
            {
                // Show grid position range for visual selection
                double sr = transport.getSampleRate();
                std::string posStr = (sr > 0.0)
                    ? gridSystem.formatGridPosition (context.getGridCursorPosition(), sr).toStdString()
                    : "0.0.0";
                breadcrumb = "> T" + std::to_string (minT) + "-T" + std::to_string (maxT)
                           + " @ " + posStr;
            }
        }
        else if (panel == VimContext::Editor)
        {
            double sr = transport.getSampleRate();
            std::string posStr = (sr > 0.0)
                ? gridSystem.formatGridPosition (context.getGridCursorPosition(), sr).toStdString()
                : "0.0.0";

            int clipIdx = context.getSelectedClipIndex();
            if (clipIdx >= 0)
                breadcrumb = "> " + trackInfo + " @ " + posStr + " [C" + std::to_string (clipIdx + 1) + "]";
            else
                breadcrumb = "> " + trackInfo + " @ " + posStr;
        }
        else if (panel == VimContext::Mixer)
        {
            auto focusName = context.getMixerFocusName().toStdString();
            breadcrumb = "> " + trackInfo;
            if (! focusName.empty())
                breadcrumb += " > " + focusName;
        }
        else if (panel == VimContext::Sequencer)
        {
            breadcrumb = "> R" + std::to_string (context.getSeqRow() + 1)
                       + " > S" + std::to_string (context.getSeqStep() + 1);
        }
    }
    else
    {
        breadcrumb = "No track selected";
    }

    canvas.drawText (breadcrumb, x + 6.0f, h * 0.5f + 5.0f, font, Color::fromARGB (0xffa6adc8));

    // ── Grid division indicator (right-aligned, before playhead time)
    {
        auto gridStr = "Grid: " + gridSystem.getGridDivisionName().toStdString();
        Rect gridArea (totalWidth - 400.0f, 0, 120.0f, h);
        canvas.drawTextRight (gridStr, gridArea, fm.getMonoFont(), Color::fromARGB (0xff7f849c));
    }

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
