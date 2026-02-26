#include "TrackLaneWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/FontManager.h"
#include "model/Track.h"
#include "model/Project.h"

namespace dc
{
namespace ui
{

TrackLaneWidget::TrackLaneWidget (const juce::ValueTree& state)
    : trackState (state)
{
    formatManager.registerBasicFormats();
}

void TrackLaneWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    auto& font = FontManager::getInstance().getDefaultFont();
    float h = getHeight();

    // Header background
    Rect headerRect (0, 0, headerWidth, h);
    if (selected)
    {
        canvas.fillRect (headerRect, Color::fromARGB (0xff353545));
        // Green accent strip
        canvas.fillRect (Rect (0, 0, 3.0f, h), theme.selection);
    }
    else
    {
        canvas.fillRect (headerRect, Color::fromARGB (0xff2a2a3a));
    }

    // Track name
    juce::String trackName = trackState.getProperty ("name", "Untitled");
    canvas.drawText (trackName.toStdString(), 8.0f, h * 0.5f + 4.0f, font, theme.defaultText);

    // Track lane background (right of header)
    float laneWidth = getWidth() - headerWidth;
    if (laneWidth > 0)
        canvas.fillRect (Rect (headerWidth, 0, laneWidth, h), theme.panelBackground);

    // Bottom separator
    canvas.drawLine (0, h - 1.0f, getWidth(), h - 1.0f, theme.outlineColor, 1.0f);
}

void TrackLaneWidget::paintOverChildren (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();

    // Draw selection highlight around selected clip
    if (selected && selectedClipIndex >= 0 && selectedClipIndex < static_cast<int> (clipViews.size()))
    {
        auto* clip = clipViews[static_cast<size_t> (selectedClipIndex)].get();
        Rect clipBounds = clip->getBounds();

        // Green glow
        canvas.fillRoundedRect (clipBounds.reduced (-2.0f), 3.0f,
                                theme.selection.withAlpha ((uint8_t) 64));
        // Green border
        canvas.strokeRect (clipBounds, theme.selection, 2.0f);
    }
}

void TrackLaneWidget::resized()
{
    rebuildClipViews();
}

void TrackLaneWidget::setPixelsPerSecond (double pps)
{
    pixelsPerSecond = pps;
    resized();
    repaint();
}

void TrackLaneWidget::setSampleRate (double sr)
{
    sampleRate = sr;
    resized();
    repaint();
}

void TrackLaneWidget::setSelected (bool sel)
{
    if (selected != sel)
    {
        selected = sel;
        repaint();
    }
}

void TrackLaneWidget::setSelectedClipIndex (int idx)
{
    if (selectedClipIndex != idx)
    {
        selectedClipIndex = idx;
        repaint();
    }
}

void TrackLaneWidget::rebuildClipViews()
{
    // Remove existing
    for (auto& cv : clipViews)
        removeChild (cv.get());
    clipViews.clear();
    waveformCaches.clear();

    float h = getHeight();

    for (int i = 0; i < trackState.getNumChildren(); ++i)
    {
        auto child = trackState.getChild (i);
        bool isAudio = child.hasType (IDs::AUDIO_CLIP);
        bool isMidi  = child.hasType (IDs::MIDI_CLIP);

        if (! isAudio && ! isMidi)
            continue;

        auto startPos = static_cast<int64_t> (static_cast<juce::int64> (child.getProperty (IDs::startPosition, 0)));
        auto clipLength = static_cast<int64_t> (static_cast<juce::int64> (child.getProperty (IDs::length, 0)));

        float x = static_cast<float> ((static_cast<double> (startPos) / sampleRate) * pixelsPerSecond) + headerWidth;
        float w = static_cast<float> ((static_cast<double> (clipLength) / sampleRate) * pixelsPerSecond);

        std::unique_ptr<gfx::Widget> widget;

        if (isAudio)
        {
            // Create waveform cache and load audio data
            auto cache = std::make_unique<gfx::WaveformCache>();
            juce::String sourceFilePath = child.getProperty ("sourceFile", "");
            if (sourceFilePath.isNotEmpty())
            {
                juce::File sourceFile (sourceFilePath);
                if (sourceFile.existsAsFile())
                    cache->loadFromFile (sourceFile, formatManager);
            }

            auto waveformWidget = std::make_unique<WaveformWidget>();
            waveformWidget->setWaveformCache (cache.get());
            waveformWidget->setPixelsPerSecond (pixelsPerSecond);
            waveformWidget->setSampleRate (sampleRate);
            waveformCaches.push_back (std::move (cache));
            widget = std::move (waveformWidget);
        }
        else
        {
            widget = std::make_unique<MidiClipWidget> (child);
        }

        widget->setBounds (x, 0, w, h);
        addChild (widget.get());
        clipViews.push_back (std::move (widget));
    }
}

} // namespace ui
} // namespace dc
