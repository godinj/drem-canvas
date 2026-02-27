#include "BrowserWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"
#include "include/core/SkFont.h"

namespace dc
{
namespace ui
{

BrowserWidget::BrowserWidget (PluginManager& pm)
    : pluginManager (pm), scanButton ("Scan Plugins")
{
    setFocusable (true);
    addChild (&scanButton);
    addChild (&pluginList);

    scanButton.onClick = [this]()
    {
        pluginManager.scanForPlugins();
        refreshPluginList();
    };

    pluginList.onDoubleClick = [this] (int index)
    {
        if (index >= 0 && index < displayedPlugins.size() && onPluginSelected)
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
    float searchY = 36.0f;
    float w = getWidth();
    Rect searchRect (4.0f, searchY, w - 8.0f, searchFieldHeight);
    canvas.fillRoundedRect (searchRect, 4.0f, theme.widgetBackground);

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

    // Cursor
    SkScalar textWidth = 0.0f;
    if (! searchBuffer.empty())
        textWidth = font.measureText (searchBuffer.data(), searchBuffer.size(), SkTextEncoding::kUTF8);

    float cursorX = textX + textWidth;
    canvas.drawLine (cursorX, searchY + 6.0f, cursorX, searchY + searchFieldHeight - 6.0f,
                     theme.defaultText, 1.5f);
}

void BrowserWidget::resized()
{
    float w = getWidth();
    float h = getHeight();

    scanButton.setBounds (4.0f, 4.0f, w - 8.0f, 28.0f);
    // Search field is drawn in paint() at y=36, height=28
    float listTop = 36.0f + searchFieldHeight + 4.0f;
    pluginList.setBounds (0, listTop, w, h - listTop);
}

bool BrowserWidget::keyDown (const gfx::KeyEvent& e)
{
    // Escape — clear search buffer
    if (e.keyCode == 0x35 || e.character == 27)
    {
        if (! searchBuffer.empty())
        {
            searchBuffer.clear();
            filterPlugins();
            repaint();
            return true;
        }
        return false;
    }

    // Backspace
    if (e.keyCode == 0x33 || e.character == 8)
    {
        if (! searchBuffer.empty())
        {
            searchBuffer.pop_back();
            filterPlugins();
            repaint();
            return true;
        }
        return false;
    }

    // Printable characters — append to search
    auto ch = e.character;
    if (ch >= 32 && ch < 127 && ! e.control && ! e.command)
    {
        searchBuffer += static_cast<char> (ch);
        filterPlugins();
        repaint();
        return true;
    }

    return false;
}

void BrowserWidget::filterPlugins()
{
    displayedPlugins.clear();
    auto& knownPlugins = pluginManager.getKnownPlugins();
    auto types = knownPlugins.getTypes();

    std::vector<std::string> names;

    for (const auto& type : types)
    {
        if (! searchBuffer.empty())
        {
            auto nameLower = type.name.toLowerCase();
            auto mfgLower = type.manufacturerName.toLowerCase();
            auto queryLower = juce::String (searchBuffer).toLowerCase();

            if (! nameLower.contains (queryLower) && ! mfgLower.contains (queryLower))
                continue;
        }

        displayedPlugins.add (type);
        names.push_back ((type.name + " (" + type.manufacturerName + ")").toStdString());
    }

    pluginList.setItems (names);
}

void BrowserWidget::refreshPluginList()
{
    searchBuffer.clear();
    filterPlugins();
}

} // namespace ui
} // namespace dc
