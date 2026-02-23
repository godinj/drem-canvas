#pragma once

#include "graphics/core/Types.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkFont.h"
#include "include/core/SkColor.h"
#include "include/core/SkRRect.h"
#include <string>
#include <vector>

namespace dc
{
namespace gfx
{

class Canvas
{
public:
    explicit Canvas (SkCanvas* skCanvas);

    // ─── State management ────────────────────────────────

    void save();
    void restore();
    void translate (float dx, float dy);
    void scale (float sx, float sy);
    void clipRect (const Rect& r);

    // ─── Shapes ──────────────────────────────────────────

    void clear (Color c);
    void fillRect (const Rect& r, Color c);
    void fillRoundedRect (const Rect& r, float radius, Color c);
    void strokeRect (const Rect& r, Color c, float strokeWidth = 1.0f);
    void drawLine (float x1, float y1, float x2, float y2, Color c, float width = 1.0f);
    void fillCircle (float cx, float cy, float radius, Color c);
    void strokeCircle (float cx, float cy, float radius, Color c, float strokeWidth = 1.0f);
    void fillEllipse (const Rect& r, Color c);

    // ─── Paths ───────────────────────────────────────────

    void fillPath (const SkPath& path, Color c);
    void strokePath (const SkPath& path, Color c, float strokeWidth = 1.0f);

    // ─── Gradients ───────────────────────────────────────

    void fillRectWithVerticalGradient (const Rect& r, Color top, Color bottom);
    void fillRectWithHorizontalGradient (const Rect& r, Color left, Color right);

    // ─── Text ────────────────────────────────────────────

    void drawText (const std::string& text, float x, float y, const SkFont& font, Color c);
    void drawTextCentred (const std::string& text, const Rect& r, const SkFont& font, Color c);
    void drawTextRight (const std::string& text, const Rect& r, const SkFont& font, Color c);

    // ─── Waveform ────────────────────────────────────────

    struct WaveformSample
    {
        float minVal;
        float maxVal;
    };

    void drawWaveform (const Rect& r, const std::vector<WaveformSample>& samples, Color c);

    // ─── Images ──────────────────────────────────────────

    void drawImage (const sk_sp<SkImage>& image, float x, float y);
    void drawImageScaled (const sk_sp<SkImage>& image, const Rect& destRect);

    // ─── Raw access ──────────────────────────────────────

    SkCanvas* getSkCanvas() const { return canvas; }

private:
    static SkColor toSkColor (Color c);
    static SkRect toSkRect (const Rect& r);

    SkCanvas* canvas;
};

} // namespace gfx
} // namespace dc
