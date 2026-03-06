#include "PluginViewWidget.h"
#include "dc/foundation/assert.h"
#include "dc/foundation/string_utils.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"
#include "dc/plugins/PluginEditor.h"
#include "plugins/PluginEditorBridge.h"
#include "plugins/SyntheticInputProbe.h"
#include "plugins/SyntheticMouseDrag.h"
#include "plugins/MouseEventForwarder.h"
#include "plugins/SpatialScanCache.h"
#include "vim/VimEngine.h"
#include <algorithm>
#include <cmath>
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
    syntheticDrag = SyntheticMouseDrag::create();
    mouseForwarder = MouseEventForwarder::create();
}

PluginViewWidget::~PluginViewWidget()
{
    editorBridge.reset();
}

void PluginViewWidget::setEditorBridge (std::unique_ptr<PluginEditorBridge> bridge)
{
    editorBridge = std::move (bridge);
}

void PluginViewWidget::setPlugin (dc::PluginInstance* plugin, const std::string& name,
                                  const std::string& fileOrIdentifier)
{
    endMouseDrag();
    pluginName = name;
    pluginFileOrIdentifier = fileOrIdentifier;
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
    endMouseDrag();
    forwardedButtonMask = 0;
    if (mouseForwarder)
        mouseForwarder->unbind();

    pluginName.clear();
    currentPlugin = nullptr;
    paramGrid.clearPlugin();

    spatialScanner.clear();
    spatialScanComplete = false;
    paramGrid.clearSpatialResults();

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

void PluginViewWidget::setHintBuffer (const std::string& buffer)
{
    spatialHintBuffer = buffer;
    paramGrid.setHintBuffer (buffer);
    repaint();
}

void PluginViewWidget::setNumberEntryActive (bool active)
{
    paramGrid.setNumberEntryActive (active);
}

void PluginViewWidget::setNumberBuffer (const std::string& buffer)
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

bool PluginViewWidget::isSpatialMode() const
{
    return spatialScanComplete && spatialScanner.hasResults();
}

void PluginViewWidget::forceSpatialRescan()
{
    if (! editorBridge || ! editorBridge->isOpen())
        return;

    int nativeW = editorBridge->getNativeWidth();
    int nativeH = editorBridge->getNativeHeight();

    if (! pluginFileOrIdentifier.empty())
        SpatialScanCache::invalidate (pluginFileOrIdentifier, nativeW, nativeH);

    spatialScanner.clear();
    spatialScanComplete = false;
    paramGrid.clearSpatialResults();
    runSpatialScan();
}

void PluginViewWidget::runSpatialScan()
{
    if (! editorBridge || ! editorBridge->isOpen() || currentPlugin == nullptr)
        return;

    int nativeW = editorBridge->getNativeWidth();
    int nativeH = editorBridge->getNativeHeight();

    // Try loading from cache first
    if (! pluginFileOrIdentifier.empty())
    {
        std::vector<SpatialParamInfo> cached;
        if (SpatialScanCache::load (pluginFileOrIdentifier, nativeW, nativeH, cached))
        {
            // Filter cached results: old caches may contain noise entries
            static constexpr int minHitCount = 3;
            cached.erase (
                std::remove_if (cached.begin(), cached.end(),
                    [] (const SpatialParamInfo& info) { return info.hitCount < minHitCount; }),
                cached.end());

            // Regenerate hint labels from position order
            int totalCount = static_cast<int> (cached.size());
            for (int i = 0; i < totalCount; ++i)
                cached[i].hintLabel = VimEngine::generateHintLabel (i, totalCount);

            spatialScanner.getMutableResults() = std::move (cached);
            spatialScanComplete = true;
            paramGrid.setSpatialResults (spatialScanner.getResults(), currentPlugin);
            return;
        }
    }

    // Phase 1: IParameterFinder grid scan (if available)
    if (currentPlugin->supportsParameterFinder())
        spatialScanner.scan (currentPlugin, nativeW, nativeH);

    // Fallback: mouse-probe grid scan when IParameterFinder unavailable
    // or Phase 1 returned no results
    if (! spatialScanner.hasResults() && inputProbe)
        runMouseProbeGridScan (nativeW, nativeH);

    // Phase 4: mouse probe — for params still unmapped after the grid scan,
    // inject synthetic mouse clicks at their centroid positions and intercept
    // the plugin's performEdit callback (via popLastEdit) to discover the
    // real controller ParamID.
    if (inputProbe && spatialScanner.hasResults())
    {
        auto& results = spatialScanner.getMutableResults();
        int numParams = currentPlugin->getNumParameters();
        int probed = 0;
        int unmappedCount = 0;

        for (auto& info : results)
            if (info.paramIndex < 0)
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
                    if (info.paramIndex >= 0)
                        continue;

                    // Drain any stale edits
                    while (currentPlugin->popLastEdit().has_value()) {}

                    inputProbe->sendProbe (info.centerX, info.centerY, strategies[pass]);

                    // Wait for yabridge IPC round-trip
                    std::this_thread::sleep_for (std::chrono::milliseconds (50));

                    auto editEvent = currentPlugin->popLastEdit();
                    if (editEvent.has_value())
                    {
                        unsigned int capturedId = static_cast<unsigned int> (editEvent->paramId);
                        // Resolve ParamID to parameter index
                        for (int idx = 0; idx < numParams; ++idx)
                        {
                            if (static_cast<unsigned int> (currentPlugin->getParameterId (idx)) == capturedId)
                            {
                                info.paramIndex = idx;
                                info.name = currentPlugin->getParameterName (idx);
                                probed++;
                                break;
                            }
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
            dc_log ("[SpatialScan] Phase 4: %d of %d resolved via mouse probe", probed, unmappedCount);
    }

    spatialScanComplete = true;

    // Push spatial results to the parameter grid — switches to spatial rendering mode
    if (spatialScanner.hasResults())
    {
        paramGrid.setSpatialResults (spatialScanner.getResults(), currentPlugin);

        // Save to disk cache for instant load next time
        if (! pluginFileOrIdentifier.empty())
            SpatialScanCache::save (pluginFileOrIdentifier, pluginName,
                                    nativeW, nativeH, spatialScanner.getResults());
    }
}

void PluginViewWidget::runMouseProbeGridScan (int nativeW, int nativeH)
{
    if (currentPlugin == nullptr || nativeW <= 0 || nativeH <= 0)
        return;

    // Fallback: enumerate parameters from the controller and arrange them
    // in a grid layout within the editor dimensions. This provides functional
    // vim hints and parameter navigation when IParameterFinder is unavailable
    // and XTest mouse probing cannot deliver events (e.g., on XWayland without
    // IRunLoop support).
    int numParams = currentPlugin->getNumParameters();
    if (numParams <= 0)
        return;

    // Determine grid layout: fill columns left-to-right, rows top-to-bottom
    int cols = std::max (1, static_cast<int> (std::ceil (std::sqrt (
        static_cast<double> (numParams) * nativeW / nativeH))));
    int rows = (numParams + cols - 1) / cols;

    int cellW = nativeW / cols;
    int cellH = nativeH / rows;

    auto& results = spatialScanner.getMutableResults();

    for (int i = 0; i < numParams; ++i)
    {
        int col = i % cols;
        int row = i / cols;

        SpatialParamInfo info;
        info.paramId = static_cast<unsigned int> (currentPlugin->getParameterId (i));
        info.paramIndex = i;
        info.name = currentPlugin->getParameterName (i);
        info.centerX = col * cellW + cellW / 2;
        info.centerY = row * cellH + cellH / 2;
        info.hitCount = 1;

        results.push_back (info);
    }

    // Assign hint labels (already in positional order)
    int totalCount = static_cast<int> (results.size());
    for (int i = 0; i < totalCount; ++i)
        results[i].hintLabel = VimEngine::generateHintLabel (i, totalCount);

    dc_log ("[SpatialScan] Controller enumeration fallback: %d params", totalCount);
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
    if (dragSuppressBounds)
        return;

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
    auto title = pluginName.empty() ? std::string ("Plugin View") : pluginName;
    Color titleColor = activeContext ? theme.selection : Color::fromARGB (0xffcdd6f4);
    canvas.drawText (title, 8.0f, headerHeight * 0.5f + 5.0f, font, titleColor);

    // Key hint text (right-aligned)
    auto hints = "z:zoom  f:hint  R:rescan  hjkl:nav  0-9:set  x:axis  c:center  e:editor  Esc:close";
    Rect hintArea (w - 560.0f, 0, 552.0f, headerHeight);
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
                            if (! spatialHintBuffer.empty()
                                && ! dc::startsWith (info.hintLabel, spatialHintBuffer))
                                continue;

                            // Transform native coords to canvas coords
                            float sx = geo.drawX + static_cast<float> (info.centerX) * geo.scaleX;
                            float sy = geo.drawY + static_cast<float> (info.centerY) * geo.scaleY;

                            auto& label = info.hintLabel;
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

            // Draw synthetic drag cursor on top of composited image
            if (dragSession.active)
            {
                auto geo = computeCompositeGeometry();
                if (geo.valid)
                {
                    float dragX = geo.drawX + static_cast<float> (dragSession.originX + dragSession.currentDeltaX) * geo.scaleX;
                    float dragY = geo.drawY + static_cast<float> (dragSession.originY + dragSession.currentDeltaY) * geo.scaleY;

                    // Crosshair + circle — clearly not a real cursor
                    Color cursorColor = Color::fromARGB (0xcc94e2d5);  // teal, slightly transparent
                    float r = 8.0f;
                    float lineLen = 14.0f;

                    canvas.fillCircle (dragX, dragY, 2.0f, cursorColor);
                    canvas.drawLine (dragX - lineLen, dragY, dragX - r, dragY, cursorColor, 1.5f);
                    canvas.drawLine (dragX + r, dragY, dragX + lineLen, dragY, cursorColor, 1.5f);
                    canvas.drawLine (dragX, dragY - lineLen, dragX, dragY - r, cursorColor, 1.5f);
                    canvas.drawLine (dragX, dragY + r, dragX, dragY + lineLen, cursorColor, 1.5f);
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

bool PluginViewWidget::getSpatialCentroid (int paramIndex, int& cx, int& cy) const
{
    if (! spatialScanComplete)
        return false;

    for (auto& info : spatialScanner.getResults())
    {
        if (info.paramIndex == paramIndex)
        {
            cx = info.centerX;
            cy = info.centerY;
            return true;
        }
    }

    return false;
}

bool PluginViewWidget::getSpatialCentroidBySpatialIndex (int spatialIdx, int& cx, int& cy) const
{
    auto& results = spatialScanner.getResults();
    if (spatialIdx < 0 || spatialIdx >= static_cast<int> (results.size()))
        return false;

    cx = results[static_cast<size_t> (spatialIdx)].centerX;
    cy = results[static_cast<size_t> (spatialIdx)].centerY;
    return true;
}

bool PluginViewWidget::applyMouseDrag (int paramIndex, int pixelDelta)
{
    if (! syntheticDrag || ! editorBridge || ! editorBridge->isOpen())
        return false;

    int cx, cy;
    bool found = isSpatialMode()
        ? getSpatialCentroidBySpatialIndex (paramIndex, cx, cy)
        : getSpatialCentroid (paramIndex, cx, cy);
    if (! found)
        return false;

    // If switching to a different parameter, end the previous drag first
    if (dragSession.active && dragSession.paramIndex != paramIndex)
        endMouseDrag();

    // Detect direction reversal — reset to centroid if enabled
    int newSign = (pixelDelta > 0) ? 1 : -1;
    if (dragSession.active && dragCenterOnReverse
        && dragSession.lastDragSign != 0 && newSign != dragSession.lastDragSign)
    {
        endMouseDrag();
    }

    if (! dragSession.active)
    {
        // Suppress bounds updates so setTargetBounds can't move the
        // editor window back off-screen during the XTest drag session
        dragSuppressBounds = true;

        // Begin new drag session at the parameter's centroid
        if (! syntheticDrag->beginDrag (*editorBridge, cx, cy))
        {
            dragSuppressBounds = false;
            return false;
        }

        dragSession.active = true;
        dragSession.paramIndex = paramIndex;
        dragSession.originX = cx;
        dragSession.originY = cy;
        dragSession.currentDeltaX = 0;
        dragSession.currentDeltaY = 0;
        dragSession.lastDragSign = 0;
    }

    dragSession.lastDragSign = newSign;

    // Accumulate delta along the active axis
    if (dragHorizontal)
        dragSession.currentDeltaX += pixelDelta;
    else
        dragSession.currentDeltaY -= pixelDelta;  // negative = drag up = increase value

    // Clamp accumulated offset to prevent runaway
    dragSession.currentDeltaX = std::clamp (dragSession.currentDeltaX, -500, 500);
    dragSession.currentDeltaY = std::clamp (dragSession.currentDeltaY, -500, 500);

    syntheticDrag->moveDrag (dragSession.originX + dragSession.currentDeltaX,
                             dragSession.originY + dragSession.currentDeltaY);
    return true;
}

bool PluginViewWidget::applyAbsoluteDrag (int spatialIndex, float value)
{
    if (! syntheticDrag || ! editorBridge || ! editorBridge->isOpen())
        return false;

    int cx, cy;
    if (! getSpatialCentroidBySpatialIndex (spatialIndex, cx, cy))
        return false;

    // End any active drag first
    endMouseDrag();

    constexpr int sweepPx = 300;

    // Begin drag at centroid
    if (! syntheticDrag->beginDrag (*editorBridge, cx, cy))
        return false;

    if (dragHorizontal)
    {
        // Sweep left to assumed 0%
        syntheticDrag->moveDrag (cx - sweepPx, cy);
        std::this_thread::sleep_for (std::chrono::milliseconds (20));

        // Move right to target value
        int targetX = cx - sweepPx + static_cast<int> (value * static_cast<float> (sweepPx));
        syntheticDrag->moveDrag (targetX, cy);
        std::this_thread::sleep_for (std::chrono::milliseconds (20));

        // End drag
        syntheticDrag->endDrag (targetX, cy);
    }
    else
    {
        // Sweep down to assumed 0%
        syntheticDrag->moveDrag (cx, cy + sweepPx);
        std::this_thread::sleep_for (std::chrono::milliseconds (20));

        // Move up to target value
        int targetY = cy + sweepPx - static_cast<int> (value * static_cast<float> (sweepPx));
        syntheticDrag->moveDrag (cx, targetY);
        std::this_thread::sleep_for (std::chrono::milliseconds (20));

        // End drag
        syntheticDrag->endDrag (cx, targetY);
    }
    return true;
}

void PluginViewWidget::toggleDragAxis()
{
    endMouseDrag();
    dragHorizontal = ! dragHorizontal;
}

void PluginViewWidget::toggleDragCenterOnReverse()
{
    dragCenterOnReverse = ! dragCenterOnReverse;
}

void PluginViewWidget::endMouseDrag()
{
    if (! dragSession.active || ! syntheticDrag)
    {
        dragSession = {};
        dragSuppressBounds = false;
        return;
    }

    syntheticDrag->endDrag (dragSession.originX + dragSession.currentDeltaX,
                        dragSession.originY + dragSession.currentDeltaY);
    dragSession = {};

    // Re-enable bounds updates and restore editor position
    dragSuppressBounds = false;
    updateEditorBounds();
}

bool PluginViewWidget::widgetToNativeCoords (float widgetX, float widgetY,
                                              int& nativeX, int& nativeY) const
{
    auto geo = computeCompositeGeometry();
    if (! geo.valid || geo.scaleX <= 0.0f || geo.scaleY <= 0.0f)
        return false;

    // Check if the point is within the composited image bounds
    if (widgetX < geo.drawX || widgetX > geo.drawX + geo.drawW
        || widgetY < geo.drawY || widgetY > geo.drawY + geo.drawH)
        return false;

    nativeX = static_cast<int> ((widgetX - geo.drawX) / geo.scaleX);
    nativeY = static_cast<int> ((widgetY - geo.drawY) / geo.scaleY);
    return true;
}

void PluginViewWidget::mouseDown (const gfx::MouseEvent& e)
{
    if (! mouseForwarder || ! editorBridge || ! editorBridge->isCompositing())
        return;

    // Don't forward mouse events while a synthetic drag is active
    if (dragSession.active)
        return;

    int nx, ny;
    if (! widgetToNativeCoords (e.x, e.y, nx, ny))
        return;

    if (! mouseForwarder->isBound())
        mouseForwarder->bind (*editorBridge);

    // Suppress bounds updates so setTargetBounds can't move the
    // editor window back off-screen during the forwarded drag
    dragSuppressBounds = true;

    // Remember widget-space origin for screen-pixel delta computation
    forwardStartX = e.x;
    forwardStartY = e.y;

    int button = e.rightButton ? 3 : 1;
    unsigned int bit = 1u << (static_cast<unsigned int> (button) - 1u);
    forwardedButtonMask |= bit;

    mouseForwarder->sendMouseDown (nx, ny, button);
}

void PluginViewWidget::mouseDrag (const gfx::MouseEvent& e)
{
    if (! mouseForwarder || ! mouseForwarder->isBound() || forwardedButtonMask == 0)
        return;

    // Pass screen-pixel delta directly — 1:1 with user's physical drag,
    // regardless of the composited image scale factor.
    int dx = static_cast<int> (e.x - forwardStartX);
    int dy = static_cast<int> (e.y - forwardStartY);

    mouseForwarder->sendDragDelta (dx, dy);
}

void PluginViewWidget::mouseUp (const gfx::MouseEvent& e)
{
    if (! mouseForwarder || ! mouseForwarder->isBound() || forwardedButtonMask == 0)
        return;

    int dx = static_cast<int> (e.x - forwardStartX);
    int dy = static_cast<int> (e.y - forwardStartY);

    int button = e.rightButton ? 3 : 1;
    mouseForwarder->sendMouseUp (dx, dy, button);

    unsigned int bit = 1u << (static_cast<unsigned int> (button) - 1u);
    forwardedButtonMask &= ~bit;

    // Re-enable bounds updates when all buttons released
    if (forwardedButtonMask == 0)
    {
        dragSuppressBounds = false;
        updateEditorBounds();
    }
}

void PluginViewWidget::mouseMove (const gfx::MouseEvent&)
{
    // Hover forwarding is not possible with XTest (it warps the cursor),
    // so mouseMove is intentionally a no-op.  Click+drag interactions
    // are handled via mouseDown/mouseDrag/mouseUp.
}

bool PluginViewWidget::mouseWheel (const gfx::WheelEvent& e)
{
    if (! mouseForwarder || ! editorBridge || ! editorBridge->isCompositing())
        return false;

    int nx, ny;
    if (! widgetToNativeCoords (e.x, e.y, nx, ny))
        return false;

    if (! mouseForwarder->isBound())
        mouseForwarder->bind (*editorBridge);

    mouseForwarder->sendMouseWheel (nx, ny, e.deltaX, e.deltaY);
    return true;
}

} // namespace ui
} // namespace dc
