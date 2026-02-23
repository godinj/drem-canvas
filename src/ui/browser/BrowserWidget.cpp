#include "BrowserWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

BrowserWidget::BrowserWidget (PluginManager& pm)
    : pluginManager (pm), scanButton ("Scan Plugins")
{
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
    canvas.fillRect (Rect (0, 0, getWidth(), getHeight()), theme.panelBackground);
}

void BrowserWidget::resized()
{
    float w = getWidth();
    float h = getHeight();

    scanButton.setBounds (4.0f, 4.0f, w - 8.0f, 28.0f);
    pluginList.setBounds (0, 36.0f, w, h - 36.0f);
}

void BrowserWidget::refreshPluginList()
{
    displayedPlugins.clear();
    auto& knownPlugins = pluginManager.getKnownPlugins();
    auto types = knownPlugins.getTypes();

    std::vector<std::string> names;
    for (const auto& type : types)
    {
        displayedPlugins.add (type);
        names.push_back ((type.name + " (" + type.manufacturerName + ")").toStdString());
    }

    pluginList.setItems (names);
}

} // namespace ui
} // namespace dc
