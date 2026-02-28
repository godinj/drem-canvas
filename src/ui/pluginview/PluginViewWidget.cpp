#include "PluginViewWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"
#include "plugins/VST3ParameterFinderSupport.h"

#if defined(__linux__)
#include "platform/linux/EmbeddedPluginEditor.h"
#include "platform/linux/X11Compositor.h"
#include "platform/linux/X11Reparent.h"
#include "platform/linux/X11MouseProbe.h"
#include <iostream>
#include <thread>
#include <chrono>
#endif

namespace dc
{
namespace ui
{

PluginViewWidget::PluginViewWidget()
{
    addChild (&paramGrid);
}

PluginViewWidget::~PluginViewWidget()
{
#if defined(__linux__)
    if (compositor)
        compositor->stopRedirect();
    embeddedEditor.reset();
#endif
}

void PluginViewWidget::setPlugin (juce::AudioPluginInstance* plugin, const juce::String& name)
{
    pluginName = name;
    currentPlugin = plugin;
    paramGrid.setPlugin (plugin);

    spatialScanner.clear();
    spatialScanComplete = false;

#if defined(__linux__)
    if (compositor)
        compositor->stopRedirect();
    compositorActive = false;

    if (embeddedEditor)
    {
        embeddedEditor->closeEditor();
        if (plugin != nullptr && glfwWindow != nullptr)
        {
            embeddedEditor->openEditor (plugin, glfwWindow);

            // Start compositor redirect BEFORE updateEditorBounds.
            // The editor is still at its native size right after openEditor,
            // so the compositor acquires a full-resolution pixmap. Calling
            // updateEditorBounds first would scale the editor down (possibly
            // to 1x1 if the widget hasn't been sized yet), and the compositor
            // would capture a tiny window that never recovers.
            if (compositor
                && embeddedEditor->getXDisplay() != nullptr
                && embeddedEditor->getXWindow() != 0)
            {
                compositorActive = compositor->startRedirect (
                    embeddedEditor->getXDisplay(),
                    embeddedEditor->getXWindow());

                // On native X11, hide the floating window so only the
                // composited Skia image is shown. On XWayland we must NOT
                // hide it — moving the window off-screen causes the Wayland
                // compositor to skip rendering, leaving the pixmap blank.
                if (compositorActive && embeddedEditor->isReparented())
                    compositor->hideWindow();
            }

            updateEditorBounds();
        }
    }
#endif

    repaint();
}

void PluginViewWidget::clearPlugin()
{
    pluginName.clear();
    currentPlugin = nullptr;
    paramGrid.clearPlugin();

    spatialScanner.clear();
    spatialScanComplete = false;
    paramGrid.setSpatialHintMap ({});

#if defined(__linux__)
    if (compositor)
        compositor->stopRedirect();
    compositorActive = false;
    if (embeddedEditor)
        embeddedEditor->closeEditor();
#endif

    repaint();
}

void PluginViewWidget::setActiveContext (bool active)
{
    activeContext = active;
    repaint();
}

void PluginViewWidget::setSelectedParamIndex (int index)
{
    paramGrid.setSelectedParamIndex (index);
}

void PluginViewWidget::setHintMode (VimContext::HintMode mode)
{
    spatialHintMode = (mode == VimContext::HintSpatial);
    paramGrid.setHintMode (mode);
    repaint();
}

void PluginViewWidget::setHintBuffer (const juce::String& buffer)
{
    spatialHintBuffer = buffer;
    paramGrid.setHintBuffer (buffer);
    repaint();
}

void PluginViewWidget::setNumberEntryActive (bool active)
{
    paramGrid.setNumberEntryActive (active);
}

void PluginViewWidget::setNumberBuffer (const juce::String& buffer)
{
    paramGrid.setNumberBuffer (buffer);
}

int PluginViewWidget::getNumParameters() const
{
    return paramGrid.getNumParameters();
}

void PluginViewWidget::setEnlarged (bool e)
{
    if (enlarged != e)
    {
        enlarged = e;
        resized();
        repaint();
    }
}

bool PluginViewWidget::hasSpatialHints()
{
    if (! spatialScanComplete)
        runSpatialScan();
    return spatialScanComplete;
}

void PluginViewWidget::runSpatialScan()
{
#if defined(__linux__)
    if (! embeddedEditor || ! embeddedEditor->isOpen() || currentPlugin == nullptr)
        return;

    auto* editor = embeddedEditor->getEditor();
    if (editor == nullptr)
        return;

    auto* finder = dynamic_cast<VST3ParameterFinderSupport*> (editor);
    if (finder == nullptr || ! finder->hasParameterFinder())
        return;

    int nativeW = embeddedEditor->getNativeWidth();
    int nativeH = embeddedEditor->getNativeHeight();

    spatialScanner.scan (*finder, currentPlugin, nativeW, nativeH);

    // Phase 4: mouse probe — for params still unmapped after phases 1-3,
    // inject synthetic mouse clicks at their centroid positions and intercept
    // the plugin's performEdit callback to discover the real controller ParamID.
    {
        auto& results = spatialScanner.getMutableResults();
        auto& params = currentPlugin->getParameters();
        void* xDisplay = embeddedEditor->getXDisplay();
        unsigned long xWindow = embeddedEditor->getXWindow();
        int probed = 0;
        int unmappedCount = 0;

        for (auto& info : results)
            if (info.juceParamIndex < 0)
                unmappedCount++;

        if (xDisplay != nullptr && xWindow != 0 && unmappedCount > 0)
        {
            // Move the editor on-screen so XTest root coords are positive.
            // In Wayland mode the editor lives at (-10000,-10000) for compositor
            // capture — XTest events at negative root coords never reach the
            // plugin window on XWayland.
            platform::x11::moveWindow (xDisplay, xWindow, 0, 0);
            std::this_thread::sleep_for (std::chrono::milliseconds (50));

            // Multi-pass probing: try different mouse strategies to handle
            // vertical knobs, horizontal sliders, buttons, etc.
            using PM = platform::x11::ProbeMode;
            const PM strategies[] = { PM::dragUp, PM::dragDown, PM::dragRight, PM::click };

            for (int pass = 0; pass < 4; ++pass)
            {
                int passAttempts = 0;

                for (auto& info : results)
                {
                    if (info.juceParamIndex >= 0)
                        continue;

                    finder->beginEditSnoop();

                    platform::x11::sendMouseProbe (xDisplay, xWindow,
                                                   info.centerX, info.centerY,
                                                   strategies[pass]);

                    // Wait for yabridge IPC round-trip
                    std::this_thread::sleep_for (std::chrono::milliseconds (50));

                    unsigned int capturedId = finder->endEditSnoop();

                    if (capturedId != 0xFFFFFFFF)
                    {
                        int juceIdx = finder->resolveParamIDToIndex (capturedId);
                        if (juceIdx >= 0 && juceIdx < params.size())
                        {
                            info.juceParamIndex = juceIdx;
                            info.name = params[juceIdx]->getName (64);
                            probed++;
                        }
                    }

                    passAttempts++;
                }

                // If no unmapped params remain, stop early
                if (passAttempts == 0)
                    break;
            }

            // Move editor back off-screen for compositor capture
            platform::x11::moveWindow (xDisplay, xWindow, -10000, -10000);
        }

        if (probed > 0)
            std::cerr << "[SpatialScan] Phase 4: " << probed
                      << " of " << unmappedCount << " resolved via mouse probe\n";
    }

    spatialScanComplete = spatialScanner.hasResults();

    // Build juceParamIndex -> hintLabel map for the parameter grid
    if (spatialScanComplete)
    {
        std::unordered_map<int, juce::String> hintMap;
        for (auto& info : spatialScanner.getResults())
            if (info.juceParamIndex >= 0)
                hintMap[info.juceParamIndex] = info.hintLabel;
        paramGrid.setSpatialHintMap (std::move (hintMap));
    }
#endif
}

PluginViewWidget::CompositeGeometry PluginViewWidget::computeCompositeGeometry() const
{
    CompositeGeometry geo;

#if defined(__linux__)
    if (! embeddedEditor || ! embeddedEditor->isOpen() || ! compositorActive || ! compositor)
        return geo;

    float w = getWidth();
    float h = getHeight();
    float paramRatio = enlarged ? 0.3f : 0.5f;
    float halfW = w * paramRatio;

    int nativeW = embeddedEditor->getNativeWidth();
    int nativeH = embeddedEditor->getNativeHeight();
    if (nativeW <= 0 || nativeH <= 0)
        return geo;

    float panelW = w - halfW;
    float panelH = h - headerHeight;

    float imgW = static_cast<float> (nativeW);
    float imgH = static_cast<float> (nativeH);
    float sx = panelW / imgW;
    float sy = panelH / imgH;
    float scale = std::min (sx, sy);

    geo.drawW = imgW * scale;
    geo.drawH = imgH * scale;
    geo.drawX = halfW + (panelW - geo.drawW);
    geo.drawY = headerHeight + (panelH - geo.drawH);
    geo.scaleX = scale;
    geo.scaleY = scale;
    geo.valid = true;
#endif

    return geo;
}

#if defined(__linux__)
void PluginViewWidget::setGlfwWindow (GLFWwindow* w)
{
    glfwWindow = w;
    embeddedEditor = std::make_unique<platform::EmbeddedPluginEditor>();
    compositor = std::make_unique<platform::x11::Compositor>();
}

void PluginViewWidget::updateEditorBounds()
{
    if (! embeddedEditor || ! embeddedEditor->isOpen())
        return;

    float w = getWidth();
    float h = getHeight();
    float paramRatio = enlarged ? 0.3f : 0.5f;
    float halfW = w * paramRatio;

    // Right portion of the widget, below the header
    int panelX = static_cast<int> (getX() + halfW);
    int panelY = static_cast<int> (getY() + headerHeight);
    int panelW = static_cast<int> (w - halfW);
    int panelH = static_cast<int> (h - headerHeight);

    if (compositorActive && ! embeddedEditor->isReparented())
    {
        // Wayland + compositor: keep the editor at its native size so the
        // plugin renders at full resolution. Use the stored native dimensions
        // (not getEditorWidth/Height which may reflect a previous scaling).
        // Position is off-screen; Skia handles the scaling for display.
        int nativeW = embeddedEditor->getNativeWidth();
        int nativeH = embeddedEditor->getNativeHeight();
        if (nativeW > 0 && nativeH > 0)
            embeddedEditor->setBounds (-10000, -10000, nativeW, nativeH);

        if (compositor)
            compositor->handleResize();
    }
    else if (panelW > 0 && panelH > 0)
    {
        if (embeddedEditor->isReparented())
        {
            // X11: coordinates are relative to the GLFW parent window.
            // setBounds scales the editor and anchors it bottom-right.
            embeddedEditor->setBounds (panelX, panelY, panelW, panelH);

            if (compositorActive && compositor)
                compositor->handleResize();
        }
        else
        {
            // Wayland without compositor: convert to absolute screen coords,
            // then setBounds handles scaling and bottom-right anchoring.
            int winX = 0, winY = 0;
            platform::x11::getWindowPos (glfwWindow, winX, winY);
            embeddedEditor->setBounds (winX + panelX, winY + panelY, panelW, panelH);
        }
    }
}
#endif

void PluginViewWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& fm = FontManager::getInstance();
    auto& font = fm.getDefaultFont();
    auto& theme = Theme::getDefault();
    float w = getWidth();

    // Header background
    canvas.fillRect (Rect (0, 0, w, headerHeight), Color::fromARGB (0xff181825));

    // Plugin name
    auto title = pluginName.isEmpty() ? "Plugin View" : pluginName.toStdString();
    Color titleColor = activeContext ? theme.selection : Color::fromARGB (0xffcdd6f4);
    canvas.drawText (title, 8.0f, headerHeight * 0.5f + 5.0f, font, titleColor);

    // Key hint text (right-aligned)
    auto hints = "z:zoom  f:hint  hjkl:nav  0-9:set  e:editor  Esc:close";
    Rect hintArea (w - 400.0f, 0, 392.0f, headerHeight);
    canvas.drawTextRight (hints, hintArea, fm.getMonoFont(), Color::fromARGB (0xff585b70));

    // Active border
    if (activeContext)
    {
        float h = getHeight();
        canvas.fillRect (Rect (0, 0, 2.0f, h), theme.selection);
    }

#if defined(__linux__)
    if (embeddedEditor && embeddedEditor->isOpen())
    {
        float paramRatio = enlarged ? 0.3f : 0.5f;
        float halfW = w * paramRatio;
        float h = getHeight();

        // Subtle separator line between param grid and native editor
        canvas.fillRect (Rect (halfW - 1.0f, headerHeight, 1.0f, h - headerHeight),
                         Color::fromARGB (0xff313244));

        // Draw composited plugin editor image
        if (compositorActive && compositor)
        {
            compositor->hasDamage();
            auto image = compositor->capture();
            if (image)
            {
                float panelW = w - halfW;
                float panelH = h - headerHeight;

                // Scale to fit panel, preserving aspect ratio
                float imgW = static_cast<float> (image->width());
                float imgH = static_cast<float> (image->height());
                float scaleX = panelW / imgW;
                float scaleY = panelH / imgH;
                float scale = std::min (scaleX, scaleY);
                float drawW = imgW * scale;
                float drawH = imgH * scale;

                // Anchor bottom-right within the panel
                float drawX = halfW + (panelW - drawW);
                float drawY = headerHeight + (panelH - drawH);

                canvas.drawImageScaled (image, Rect (drawX, drawY, drawW, drawH));

                // Draw spatial hint labels on top of composited image
                if (spatialHintMode && spatialScanComplete)
                {
                    auto& monoFont = fm.getMonoFont();
                    auto geo = computeCompositeGeometry();

                    if (geo.valid)
                    {
                        for (auto& info : spatialScanner.getResults())
                        {
                            // Filter by typed prefix
                            if (spatialHintBuffer.isNotEmpty()
                                && ! info.hintLabel.startsWith (spatialHintBuffer))
                                continue;

                            // Transform native coords to canvas coords
                            float sx = geo.drawX + static_cast<float> (info.centerX) * geo.scaleX;
                            float sy = geo.drawY + static_cast<float> (info.centerY) * geo.scaleY;

                            auto label = info.hintLabel.toStdString();
                            float labelW = static_cast<float> (label.size()) * 10.0f + 6.0f;
                            float labelH = 16.0f;

                            // Background
                            Rect bgRect (sx - labelW * 0.5f, sy - labelH * 0.5f, labelW, labelH);
                            canvas.fillRoundedRect (bgRect, 3.0f, Color::fromARGB (0xdd1e1e2e));

                            // Label text
                            canvas.drawTextCentred (label, bgRect, monoFont,
                                                    Color::fromARGB (0xfff9e2af));
                        }
                    }
                }
            }

            // Keep repainting for continuous compositor updates
            repaint();
        }
    }
#endif
}

void PluginViewWidget::resized()
{
    float w = getWidth();
    float h = getHeight();

#if defined(__linux__)
    if (embeddedEditor && embeddedEditor->isOpen())
    {
        // Split: params left, native editor right (wider when enlarged)
        float paramRatio = enlarged ? 0.3f : 0.5f;
        float halfW = w * paramRatio;
        paramGrid.setBounds (0, headerHeight, halfW, h - headerHeight);
        updateEditorBounds();
    }
    else
#endif
    {
        paramGrid.setBounds (0, headerHeight, w, h - headerHeight);
    }
}

} // namespace ui
} // namespace dc
