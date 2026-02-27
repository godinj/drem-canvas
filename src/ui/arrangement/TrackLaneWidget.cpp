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
    if (selected || inVisualSelection)
    {
        canvas.fillRect (headerRect, Color::fromARGB (0xff353545));
        // Orange accent strip for visual, green for normal
        auto accentColor = inVisualSelection ? Color::fromARGB (0xffff9944) : theme.selection;
        canvas.fillRect (Rect (0, 0, 3.0f, h), accentColor);
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
    {
        canvas.fillRect (Rect (headerWidth, 0, laneWidth, h), theme.panelBackground);

        // Subtle tint over lane body when selected or in visual selection
        if (selected || inVisualSelection)
        {
            auto tintColor = inVisualSelection ? Color::fromARGB (0xffff9944).withAlpha ((uint8_t) 15)
                                               : theme.selection.withAlpha ((uint8_t) 15);
            canvas.fillRect (Rect (headerWidth, 0, laneWidth, h), tintColor);
        }
    }

    // Bottom separator
    canvas.drawLine (0, h - 1.0f, getWidth(), h - 1.0f, theme.outlineColor, 1.0f);
}

void TrackLaneWidget::paintOverChildren (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    float h = getHeight();
    float laneWidth = getWidth() - headerWidth;

    // Draw subtle grid lines when zoom level makes them >= 8px apart
    if (selected && gridUnitInSamples > 0 && sampleRate > 0.0 && laneWidth > 0)
    {
        double gridPixels = (static_cast<double> (gridUnitInSamples) / sampleRate) * pixelsPerSecond;
        if (gridPixels >= 8.0)
        {
            Color gridColor = Color::fromARGB (0x18ffffff);
            // Find first visible grid line
            double startSample = 0.0;
            for (double pos = startSample; ; pos += static_cast<double> (gridUnitInSamples))
            {
                float x = static_cast<float> ((pos / sampleRate) * pixelsPerSecond) + headerWidth;
                if (x > getWidth())
                    break;
                if (x >= headerWidth)
                    canvas.drawLine (x, 0, x, h, gridColor, 1.0f);
            }
        }
    }

    // Visual selection highlight
    if (inVisualSelection)
    {
        Color highlightColor = Color::fromARGB (0xffff9944); // orange

        if (gridVisualActive && ! visualLinewise && sampleRate > 0.0)
        {
            // Grid-based visual selection: draw continuous orange rectangle
            int64_t minPos = std::min (gridVisualStartPos, gridVisualEndPos);
            int64_t maxPos = std::max (gridVisualStartPos, gridVisualEndPos);
            if (gridUnitInSamples > 0)
                maxPos += gridUnitInSamples; // extend to cover cursor's grid cell

            float startX = static_cast<float> ((static_cast<double> (minPos) / sampleRate) * pixelsPerSecond) + headerWidth;
            float endX   = static_cast<float> ((static_cast<double> (maxPos) / sampleRate) * pixelsPerSecond) + headerWidth;

            canvas.fillRect (Rect (startX, 0, endX - startX, h),
                             highlightColor.withAlpha ((uint8_t) 50));
            canvas.drawLine (startX, 0, startX, h, highlightColor, 2.0f);
            canvas.drawLine (endX, 0, endX, h, highlightColor, 2.0f);
        }
        else
        {
            // Linewise or fallback: highlight all clips on track
            auto drawClipHighlight = [&] (gfx::Widget* clip)
            {
                Rect clipBounds = clip->getBounds();
                canvas.fillRoundedRect (clipBounds.reduced (-2.0f), 3.0f,
                                        highlightColor.withAlpha ((uint8_t) 64));
                canvas.strokeRect (clipBounds, highlightColor, 2.0f);
            };

            if (visualLinewise)
            {
                for (auto& cv : clipViews)
                    drawClipHighlight (cv.get());
            }
            else
            {
                int start = std::max (0, visualStartClip);
                int end   = std::min (static_cast<int> (clipViews.size()) - 1, visualEndClip);
                for (int i = start; i <= end; ++i)
                    drawClipHighlight (clipViews[static_cast<size_t> (i)].get());
            }
        }
    }

    // Grid cursor rectangle (drawn on selected track only)
    if (selected && gridCursorPosition >= 0 && gridUnitInSamples > 0 && sampleRate > 0.0)
    {
        float cursorX = static_cast<float> ((static_cast<double> (gridCursorPosition) / sampleRate) * pixelsPerSecond) + headerWidth;
        float cursorW = static_cast<float> ((static_cast<double> (gridUnitInSamples) / sampleRate) * pixelsPerSecond);

        // Semi-transparent green rectangle (full track height)
        Color cursorFill = theme.selection.withAlpha ((uint8_t) 40);
        canvas.fillRect (Rect (cursorX, 0, cursorW, h), cursorFill);

        // Thin solid green line on left edge (the "position" line)
        canvas.drawLine (cursorX, 0, cursorX, h, theme.selection, 2.0f);
    }

    // Clip-under-cursor indicator (green border around clip containing cursor)
    if (selected && selectedClipIndex >= 0 && selectedClipIndex < static_cast<int> (clipViews.size())
        && ! inVisualSelection)
    {
        auto* clip = clipViews[static_cast<size_t> (selectedClipIndex)].get();
        Rect clipBounds = clip->getBounds();

        canvas.fillRoundedRect (clipBounds.reduced (-2.0f), 3.0f,
                                theme.selection.withAlpha ((uint8_t) 64));
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

void TrackLaneWidget::setTempo (double bpm)
{
    tempo = bpm;
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

void TrackLaneWidget::setVisualSelection (const VimContext::VisualSelection& sel, int trackIndex)
{
    bool wasInVisual = inVisualSelection;

    if (sel.active)
    {
        int minTrack = std::min (sel.startTrack, sel.endTrack);
        int maxTrack = std::max (sel.startTrack, sel.endTrack);
        inVisualSelection = (trackIndex >= minTrack && trackIndex <= maxTrack);
        visualLinewise = sel.linewise;

        if (inVisualSelection && ! sel.linewise)
        {
            if (minTrack == maxTrack)
            {
                visualStartClip = std::min (sel.startClip, sel.endClip);
                visualEndClip   = std::max (sel.startClip, sel.endClip);
            }
            else if (trackIndex > minTrack && trackIndex < maxTrack)
            {
                visualStartClip = 0;
                visualEndClip   = static_cast<int> (clipViews.size()) - 1;
            }
            else
            {
                bool startIsMin = (sel.startTrack <= sel.endTrack);
                int anchorClip  = startIsMin ? sel.startClip : sel.endClip;
                int cursorClip  = startIsMin ? sel.endClip   : sel.startClip;

                if (trackIndex == minTrack)
                {
                    visualStartClip = anchorClip;
                    visualEndClip   = static_cast<int> (clipViews.size()) - 1;
                }
                else
                {
                    visualStartClip = 0;
                    visualEndClip   = cursorClip;
                }
            }
        }
        else
        {
            visualStartClip = 0;
            visualEndClip   = static_cast<int> (clipViews.size()) - 1;
        }
    }
    else
    {
        inVisualSelection = false;
        visualLinewise = false;
        visualStartClip = -1;
        visualEndClip = -1;
    }

    if (wasInVisual != inVisualSelection)
        repaint();
}

void TrackLaneWidget::setGridCursorPosition (int64_t pos)
{
    if (gridCursorPosition != pos)
    {
        gridCursorPosition = pos;
        repaint();
    }
}

void TrackLaneWidget::setGridUnitInSamples (int64_t unit)
{
    if (gridUnitInSamples != unit)
    {
        gridUnitInSamples = unit;
        repaint();
    }
}

void TrackLaneWidget::setGridVisualSelection (int64_t startPos, int64_t endPos, bool active)
{
    gridVisualActive = active;
    gridVisualStartPos = startPos;
    gridVisualEndPos = endPos;
    repaint();
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
            auto midiWidget = std::make_unique<MidiClipWidget> (child);
            // Convert clip length from samples to beats
            double clipSeconds = static_cast<double> (clipLength) / sampleRate;
            double clipBeats = clipSeconds * tempo / 60.0;
            midiWidget->setClipLengthInBeats (clipBeats);
            widget = std::move (midiWidget);
        }

        widget->setBounds (x, 0, w, h);
        addChild (widget.get());
        clipViews.push_back (std::move (widget));
    }
}

} // namespace ui
} // namespace dc
