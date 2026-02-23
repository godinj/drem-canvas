#pragma once

#include "graphics/core/Types.h"

namespace dc
{
namespace gfx
{

struct Theme
{
    // ─── Background colors ───────────────────────────────
    Color windowBackground      = Color::fromARGB (0xff1e1e2e);
    Color widgetBackground      = Color::fromARGB (0xff2a2a3a);
    Color menuBackground        = Color::fromARGB (0xff252535);
    Color panelBackground       = Color::fromARGB (0xff1a1a2a);
    Color outlineColor          = Color::fromARGB (0xff3a3a4a);

    // ─── Text colors ─────────────────────────────────────
    Color defaultText           = Color::fromARGB (0xffe0e0e0);
    Color dimText               = Color::fromARGB (0xff808090);
    Color brightText            = Color::fromARGB (0xffffffff);
    Color menuText              = Color::fromARGB (0xffe0e0e0);

    // ─── Accent colors ──────────────────────────────────
    Color accent                = Color::fromARGB (0xff4a9eff);
    Color accentHighlight       = Color::fromARGB (0xff5ab0ff);
    Color selection             = Color::fromARGB (0xff50c878);
    Color playhead              = Color::fromARGB (0xffff3333);
    Color cursor                = Color::fromARGB (0xff00ffff);

    // ─── Button colors ──────────────────────────────────
    Color buttonDefault         = Color::fromARGB (0xff3a3a4a);
    Color buttonHover           = Color::fromARGB (0xff4a4a5a);
    Color buttonPressed         = Color::fromARGB (0xff5a5a6a);
    Color buttonToggled         = Color::fromARGB (0xff4a9eff);

    // ─── Slider colors ──────────────────────────────────
    Color sliderTrack           = Color::fromARGB (0xff555565);
    Color sliderThumb           = Color::fromARGB (0xff4a9eff);

    // ─── Meter colors ────────────────────────────────────
    Color meterGreen            = Color::fromARGB (0xff40c040);
    Color meterYellow           = Color::fromARGB (0xffe0c020);
    Color meterRed              = Color::fromARGB (0xffff3030);
    Color meterBackground       = Color::fromARGB (0xff1a1a2a);

    // ─── Waveform ────────────────────────────────────────
    Color waveformFill          = Color::fromARGB (0xff4a9eff);
    Color waveformOutline       = Color::fromARGB (0xff3080d0);

    // ─── Dimensions ──────────────────────────────────────
    float headerWidth           = 150.0f;
    float trackHeight           = 100.0f;
    float rulerHeight           = 30.0f;
    float stripWidth            = 80.0f;
    float transportHeight       = 40.0f;
    float statusBarHeight       = 24.0f;
    float buttonCornerRadius    = 4.0f;
    float scrollBarWidth        = 8.0f;

    // ─── Font sizes ──────────────────────────────────────
    float fontSizeSmall         = 11.0f;
    float fontSizeDefault       = 13.0f;
    float fontSizeLarge         = 16.0f;
    float fontSizeTitle         = 20.0f;

    // ─── Singleton ───────────────────────────────────────
    static const Theme& getDefault()
    {
        static Theme instance;
        return instance;
    }
};

} // namespace gfx
} // namespace dc
