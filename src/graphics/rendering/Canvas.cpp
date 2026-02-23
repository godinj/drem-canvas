#include "Canvas.h"
#include "include/core/SkImage.h"
#include "include/effects/SkGradientShader.h"

namespace dc
{
namespace gfx
{

Canvas::Canvas (SkCanvas* skCanvas)
    : canvas (skCanvas)
{
}

// ─── State ───────────────────────────────────────────────────

void Canvas::save() { canvas->save(); }
void Canvas::restore() { canvas->restore(); }
void Canvas::translate (float dx, float dy) { canvas->translate (dx, dy); }
void Canvas::scale (float sx, float sy) { canvas->scale (sx, sy); }
void Canvas::clipRect (const Rect& r) { canvas->clipRect (toSkRect (r)); }

// ─── Shapes ──────────────────────────────────────────────────

void Canvas::clear (Color c)
{
    canvas->clear (toSkColor (c));
}

void Canvas::fillRect (const Rect& r, Color c)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setAntiAlias (true);
    canvas->drawRect (toSkRect (r), paint);
}

void Canvas::fillRoundedRect (const Rect& r, float radius, Color c)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setAntiAlias (true);
    SkRRect rr;
    rr.setRectXY (toSkRect (r), radius, radius);
    canvas->drawRRect (rr, paint);
}

void Canvas::strokeRect (const Rect& r, Color c, float strokeWidth)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setStyle (SkPaint::kStroke_Style);
    paint.setStrokeWidth (strokeWidth);
    paint.setAntiAlias (true);
    canvas->drawRect (toSkRect (r), paint);
}

void Canvas::drawLine (float x1, float y1, float x2, float y2, Color c, float width)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setStrokeWidth (width);
    paint.setAntiAlias (true);
    canvas->drawLine (x1, y1, x2, y2, paint);
}

void Canvas::fillCircle (float cx, float cy, float radius, Color c)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setAntiAlias (true);
    canvas->drawCircle (cx, cy, radius, paint);
}

void Canvas::strokeCircle (float cx, float cy, float radius, Color c, float strokeWidth)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setStyle (SkPaint::kStroke_Style);
    paint.setStrokeWidth (strokeWidth);
    paint.setAntiAlias (true);
    canvas->drawCircle (cx, cy, radius, paint);
}

void Canvas::fillEllipse (const Rect& r, Color c)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setAntiAlias (true);
    canvas->drawOval (toSkRect (r), paint);
}

// ─── Paths ───────────────────────────────────────────────────

void Canvas::fillPath (const SkPath& path, Color c)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setAntiAlias (true);
    canvas->drawPath (path, paint);
}

void Canvas::strokePath (const SkPath& path, Color c, float strokeWidth)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setStyle (SkPaint::kStroke_Style);
    paint.setStrokeWidth (strokeWidth);
    paint.setAntiAlias (true);
    canvas->drawPath (path, paint);
}

// ─── Gradients ───────────────────────────────────────────────

void Canvas::fillRectWithVerticalGradient (const Rect& r, Color top, Color bottom)
{
    SkPoint pts[2] = { { r.x, r.y }, { r.x, r.bottom() } };
    SkColor colors[2] = { toSkColor (top), toSkColor (bottom) };
    auto shader = SkGradientShader::MakeLinear (pts, colors, nullptr, 2, SkTileMode::kClamp);

    SkPaint paint;
    paint.setShader (shader);
    paint.setAntiAlias (true);
    canvas->drawRect (toSkRect (r), paint);
}

void Canvas::fillRectWithHorizontalGradient (const Rect& r, Color left, Color right)
{
    SkPoint pts[2] = { { r.x, r.y }, { r.right(), r.y } };
    SkColor colors[2] = { toSkColor (left), toSkColor (right) };
    auto shader = SkGradientShader::MakeLinear (pts, colors, nullptr, 2, SkTileMode::kClamp);

    SkPaint paint;
    paint.setShader (shader);
    paint.setAntiAlias (true);
    canvas->drawRect (toSkRect (r), paint);
}

// ─── Text ────────────────────────────────────────────────────

void Canvas::drawText (const std::string& text, float x, float y, const SkFont& font, Color c)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setAntiAlias (true);
    canvas->drawSimpleText (text.c_str(), text.size(), SkTextEncoding::kUTF8, x, y, font, paint);
}

void Canvas::drawTextCentred (const std::string& text, const Rect& r, const SkFont& font, Color c)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setAntiAlias (true);

    SkRect textBounds;
    font.measureText (text.c_str(), text.size(), SkTextEncoding::kUTF8, &textBounds);

    float x = r.x + (r.width - textBounds.width()) * 0.5f - textBounds.fLeft;
    float y = r.y + (r.height - textBounds.height()) * 0.5f - textBounds.fTop;

    canvas->drawSimpleText (text.c_str(), text.size(), SkTextEncoding::kUTF8, x, y, font, paint);
}

void Canvas::drawTextRight (const std::string& text, const Rect& r, const SkFont& font, Color c)
{
    SkPaint paint;
    paint.setColor (toSkColor (c));
    paint.setAntiAlias (true);

    SkRect textBounds;
    font.measureText (text.c_str(), text.size(), SkTextEncoding::kUTF8, &textBounds);

    float x = r.right() - textBounds.width() - textBounds.fLeft - 4.0f;
    float y = r.y + (r.height - textBounds.height()) * 0.5f - textBounds.fTop;

    canvas->drawSimpleText (text.c_str(), text.size(), SkTextEncoding::kUTF8, x, y, font, paint);
}

// ─── Waveform ────────────────────────────────────────────────

void Canvas::drawWaveform (const Rect& r, const std::vector<WaveformSample>& samples, Color c)
{
    if (samples.empty() || r.isEmpty())
        return;

    float centreY = r.y + r.height * 0.5f;
    float halfHeight = r.height * 0.5f;
    float xStep = r.width / static_cast<float> (samples.size());

    SkPath path;
    // Draw top half (maxVal)
    path.moveTo (r.x, centreY - samples[0].maxVal * halfHeight);
    for (size_t i = 1; i < samples.size(); ++i)
    {
        float x = r.x + static_cast<float> (i) * xStep;
        path.lineTo (x, centreY - samples[i].maxVal * halfHeight);
    }
    // Draw bottom half (minVal) in reverse
    for (int i = static_cast<int> (samples.size()) - 1; i >= 0; --i)
    {
        float x = r.x + static_cast<float> (i) * xStep;
        path.lineTo (x, centreY - samples[static_cast<size_t> (i)].minVal * halfHeight);
    }
    path.close();

    fillPath (path, c);
}

// ─── Images ──────────────────────────────────────────────────

void Canvas::drawImage (const sk_sp<SkImage>& image, float x, float y)
{
    if (image)
        canvas->drawImage (image, x, y);
}

void Canvas::drawImageScaled (const sk_sp<SkImage>& image, const Rect& destRect)
{
    if (image)
    {
        SkRect src = SkRect::MakeWH (static_cast<float> (image->width()),
                                     static_cast<float> (image->height()));
        canvas->drawImageRect (image, src, toSkRect (destRect), SkSamplingOptions(), nullptr,
                               SkCanvas::kStrict_SrcRectConstraint);
    }
}

// ─── Helpers ─────────────────────────────────────────────────

SkColor Canvas::toSkColor (Color c)
{
    return SkColorSetARGB (c.a, c.r, c.g, c.b);
}

SkRect Canvas::toSkRect (const Rect& r)
{
    return SkRect::MakeXYWH (r.x, r.y, r.width, r.height);
}

} // namespace gfx
} // namespace dc
