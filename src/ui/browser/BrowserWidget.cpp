#include "BrowserWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"
#include "include/core/SkFont.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace dc
{
namespace ui
{

BrowserWidget::BrowserWidget (PluginManager& pm)
    : pluginManager (pm), scanButton ("Scan Plugins")
{
    setFocusable (true);
    addChild (&scanButton);
    addChild (&progressBar);
    addChild (&scanStatusLabel);
    addChild (&pluginList);

    progressBar.setVisible (false);
    scanStatusLabel.setVisible (false);
    scanStatusLabel.setFontSize (11.0f);

    scanButton.onClick = [this]()
    {
        startAsyncScan();
    };

    pluginList.onDoubleClick = [this] (int index)
    {
        if (index >= 0 && index < static_cast<int> (displayedPlugins.size()) && onPluginSelected)
            onPluginSelected (displayedPlugins[index]);
    };

    refreshPluginList();
}

void BrowserWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& theme = Theme::getDefault();
    auto& fontMgr = FontManager::getInstance();
    auto& font = fontMgr.getDefaultFont();

    canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), theme.panelBackground);

    // ─── Search field ────────────────────────────────────────
    float searchY = searchFieldY_;
    float w = getWidth();
    Rect searchRect (4.0f, searchY, w - 8.0f, searchFieldHeight);
    canvas.fillRoundedRect (searchRect, 4.0f, theme.widgetBackground);

    // Highlight border when search is active
    if (searchActive)
        canvas.strokeRect (searchRect, theme.selection, 1.5f);

    float textY = searchY + searchFieldHeight * 0.5f + 4.0f;
    float textX = 10.0f;

    if (! searchBuffer.empty())
    {
        canvas.drawText (searchBuffer, textX, textY, font, theme.defaultText);
    }
    else
    {
        canvas.drawText ("Filter plugins...", textX, textY, font, theme.dimText);
    }

    // Cursor (only when search is active)
    if (searchActive)
    {
        SkScalar textWidth = 0.0f;
        if (! searchBuffer.empty())
            textWidth = font.measureText (searchBuffer.data(), searchBuffer.size(), SkTextEncoding::kUTF8);

        float cursorX = textX + textWidth;
        canvas.drawLine (cursorX, searchY + 6.0f, cursorX, searchY + searchFieldHeight - 6.0f,
                         theme.defaultText, 1.5f);
    }
}

void BrowserWidget::resized()
{
    float w = getWidth();
    float h = getHeight();
    float y = 4.0f;

    // Scan button — always at top
    scanButton.setBounds (4.0f, y, w - 8.0f, 28.0f);
    y += 32.0f;

    // Progress bar — only visible during scan
    if (scanInProgress_)
    {
        progressBar.setBounds (4.0f, y, w - 8.0f, 20.0f);
        y += 24.0f;
        scanStatusLabel.setBounds (4.0f, y, w - 8.0f, 16.0f);
        y += 20.0f;
    }

    // Search field is drawn in paint() at searchFieldY_, height=searchFieldHeight
    searchFieldY_ = y;
    float listTop = y + searchFieldHeight + 4.0f;
    pluginList.setBounds (0, listTop, w, h - listTop);
}

bool BrowserWidget::keyDown (const gfx::KeyEvent&)
{
    // All input is driven by VimEngine; widget does not handle keys directly
    return false;
}

void BrowserWidget::setSearchFilter (const std::string& query)
{
    searchBuffer = query;
    searchActive = true;
    filterPlugins();
    // Auto-select first result
    if (! displayedPlugins.empty())
        selectPlugin (0);
    repaint();
}

void BrowserWidget::clearSearchFilter()
{
    searchBuffer.clear();
    searchActive = false;
    filterPlugins();
    if (! displayedPlugins.empty())
        selectPlugin (0);
    repaint();
}

void BrowserWidget::filterPlugins()
{
    displayedPlugins.clear();
    auto& knownPlugins = pluginManager.getKnownPlugins();

    std::vector<std::string> names;

    for (const auto& type : knownPlugins)
    {
        if (! searchBuffer.empty())
        {
            std::string nameLower = type.name;
            std::transform (nameLower.begin(), nameLower.end(), nameLower.begin(),
                            [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
            std::string mfgLower = type.manufacturer;
            std::transform (mfgLower.begin(), mfgLower.end(), mfgLower.begin(),
                            [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
            std::string queryLower = searchBuffer;
            std::transform (queryLower.begin(), queryLower.end(), queryLower.begin(),
                            [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });

            if (nameLower.find (queryLower) == std::string::npos
                && mfgLower.find (queryLower) == std::string::npos)
                continue;
        }

        displayedPlugins.push_back (type);
        names.push_back (type.name + " (" + type.manufacturer + ")");
    }

    pluginList.setItems (names);
}

void BrowserWidget::refreshPluginList()
{
    searchBuffer.clear();
    filterPlugins();
}

int BrowserWidget::getNumPlugins() const
{
    return static_cast<int> (displayedPlugins.size());
}

int BrowserWidget::getSelectedPluginIndex() const
{
    return pluginList.getSelectedIndex();
}

void BrowserWidget::selectPlugin (int index)
{
    int numRows = static_cast<int> (displayedPlugins.size());
    if (numRows == 0) return;

    index = ((index % numRows) + numRows) % numRows;
    pluginList.setSelectedIndex (index);
}

void BrowserWidget::moveSelection (int delta)
{
    int current = pluginList.getSelectedIndex();
    if (current < 0) current = 0;
    selectPlugin (current + delta);
}

void BrowserWidget::scrollByHalfPage (int direction)
{
    float listHeight = pluginList.getHeight();
    float rowH = pluginList.getRowHeight();
    int visibleRows = static_cast<int> (listHeight / rowH);
    int halfPage = std::max (1, visibleRows / 2);
    moveSelection (direction * halfPage);
}

void BrowserWidget::confirmSelection()
{
    int idx = pluginList.getSelectedIndex();
    if (idx >= 0 && idx < static_cast<int> (displayedPlugins.size()) && onPluginSelected)
        onPluginSelected (displayedPlugins[idx]);
}

void BrowserWidget::startAsyncScan()
{
    if (scanInProgress_)
        return;

    scanInProgress_ = true;
    scanCurrent_ = 0;
    scanTotal_ = 0;
    scanResultReady_ = false;
    scanButton.setText ("Scanning...");
    repaint();

    pluginManager.scanForPluginsAsync (
        [this] (const std::string& /*name*/, int current, int total)
        {
            scanCurrent_ = current;
            scanTotal_ = total;
        },
        [this] ()
        {
            scanResultReady_ = true;
        });
}

void BrowserWidget::tick()
{
    if (scanInProgress_ && scanResultReady_)
    {
        scanInProgress_ = false;
        scanResultReady_ = false;
        scanButton.setText ("Scan Plugins");
        refreshPluginList();
        repaint();
    }
    else if (scanInProgress_)
    {
        // Update button text with progress
        int cur = scanCurrent_;
        int tot = scanTotal_;
        if (tot > 0)
        {
            scanButton.setText ("Scanning " + std::to_string (cur) + "/" + std::to_string (tot));
            repaint();
        }
    }
}

} // namespace ui
} // namespace dc
