#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ListBoxWidget.h"
#include "graphics/widgets/ButtonWidget.h"
#include "plugins/PluginManager.h"
#include <functional>

namespace dc
{
namespace ui
{

class BrowserWidget : public gfx::Widget
{
public:
    explicit BrowserWidget (PluginManager& pluginManager);

    void paint (gfx::Canvas& canvas) override;
    void resized() override;

    void refreshPluginList();

    std::function<void (const juce::PluginDescription&)> onPluginSelected;

private:
    PluginManager& pluginManager;
    gfx::ButtonWidget scanButton;
    gfx::ListBoxWidget pluginList;
    juce::Array<juce::PluginDescription> displayedPlugins;
};

} // namespace ui
} // namespace dc
