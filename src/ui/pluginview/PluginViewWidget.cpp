#include "PluginViewWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"
#include "plugins/VST3ParameterFinderSupport.h"
#include "plugins/PluginEditorBridge.h"
#include "plugins/SyntheticInputProbe.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace dc
{
namespace ui
{

PluginViewWidget::PluginViewWidget()
{
    addChild (&paramGrid);
    inputProbe = SyntheticInputProbe::create();
}

PluginViewWidget::~PluginViewWidget()
{
    editorBridge.reset();
}

void PluginViewWidget::setEditorBridge (std::unique_ptr<PluginEditorBridge> bridge)
{
    editorBridge = std::move (bridge);
}

void PluginViewWidget::setPlugin (juce::AudioPluginInstance* plugin, const juce::String& name)
{
    pluginName = name;
    currentPlugin = plugin;
    paramGrid.setPlugin (plugin);

    spatialScanner.clear();
    spatialScanComplete = false;

    if (editorBridge)
    {
        editorBridge->closeEditor();
        if (plugin != nullptr)
        {
            editorBridge->openEditor (plugin);
            updateEditorBounds();
        }
    }

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

    if (editorBridge)
        editorBridge->closeEditor();

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
    if (! editorBridge || ! editorBridge->isOpen() || currentPlugin == nullptr)
        return;

    auto* editor = editorBridge->getEditor();
    if (editor == nullptr)
        return;

    auto* finder = dynamic_cast<VST3ParameterFinderSupport*> (editor);
    if (finder == nullptr || ! finder->hasParameterFinder())
        return;

    int nativeW = editorBridge->getNativeWidth();
    int nativeH = editorBridge->getNativeHeight();

    spatialScanner.scan (*finder, currentPlugin, nativeW, nativeH);

    // Phase 4: mouse probe — for params still unmapped after phases 1-3,
    // inject synthetic mouse clicks at their centroid positions and intercept
    // the plugin's performEdit callback to discover the real controller ParamID.
    if (inputProbe)
    {
        auto& results = spatialScanner.getMutableResults();
        auto& params = currentPlugin->getParameters();
        int probed = 0;
        int unmappedCount = 0;

        for (auto& info : results)
            if (info.juceParamIndex < 0)
                unmappedCount++;

        if (unmappedCount > 0 && inputProbe->beginProbing (*editorBridge))
        {
            // Multi-pass probing: try different mouse strategies to handle
            // vertical knobs, horizontal sliders, buttons, etc.
            const ProbeMode strategies[] = { ProbeMode::dragUp, ProbeMode::dragDown,
                                             ProbeMode::dragRight, ProbeMode::click };

            for (int pass = 0; pass < 4; ++pass)
            {
                int passAttempts = 0;

                for (auto& info : results)
                {
                    if (info.juceParamIndex >= 0)
                        continue;

                    finder->beginEditSnoop();

                    inputProbe->sendProbe (info.centerX, info.centerY, strategies[pass]);

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

            inputProbe->endProbing (*editorBridge);
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
}

PluginViewWidget::CompositeGeometry PluginViewWidget::computeCompositeGeometry() const
{
    CompositeGeometry geo;

    if (! editorBridge || ! editorBridge->isOpen() || ! editorBridge->isCompositing())
        return geo;

    float w = getWidth();
    float h = getHeight();
    float paramRatio = enlarged ? 0.3f : 0.5f;
    float halfW = w * paramRatio;

    int nativeW = editorBridge->getNativeWidth();
    int nativeH = editorBridge->getNativeHeight();
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

    return geo;
}

void PluginViewWidget::updateEditorBounds()
{
    if (! editorBridge || ! editorBridge->isOpen())
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

    editorBridge->setTargetBounds (panelX, panelY, panelW, panelH);
}

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

    if (editorBridge && editorBridge->isOpen())
    {
        float paramRatio = enlarged ? 0.3f : 0.5f;
        float halfW = w * paramRatio;
        float h = getHeight();

        // Subtle separator line between param grid and native editor
        canvas.fillRect (Rect (halfW - 1.0f, headerHeight, 1.0f, h - headerHeight),
                         Color::fromARGB (0xff313244));

        // Draw composited plugin editor image
        if (editorBridge->isCompositing())
        {
            editorBridge->hasDamage();
            auto image = editorBridge->capture();
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
}

void PluginViewWidget::resized()
{
    float w = getWidth();
    float h = getHeight();

    if (editorBridge && editorBridge->isOpen())
    {
        // Split: params left, native editor right (wider when enlarged)
        float paramRatio = enlarged ? 0.3f : 0.5f;
        float halfW = w * paramRatio;
        paramGrid.setBounds (0, headerHeight, halfW, h - headerHeight);
        updateEditorBounds();
    }
    else
    {
        paramGrid.setBounds (0, headerHeight, w, h - headerHeight);
    }
}

} // namespace ui
} // namespace dc
