#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ListBoxWidget.h"
#include "graphics/widgets/ButtonWidget.h"
#include "plugins/PluginManager.h"
#include <functional>
#include <string>

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
    bool keyDown (const gfx::KeyEvent& e) override;

    void refreshPluginList();

    // Keyboard navigation
    int getNumPlugins() const;
    int getSelectedPluginIndex() const;
    void selectPlugin (int index);
    void moveSelection (int delta);
    void scrollByHalfPage (int direction);
    void confirmSelection();

    std::function<void (const juce::PluginDescription&)> onPluginSelected;

private:
    void filterPlugins();

    PluginManager& pluginManager;
    gfx::ButtonWidget scanButton;
    gfx::ListBoxWidget pluginList;
    juce::Array<juce::PluginDescription> displayedPlugins;
    std::string searchBuffer;

    static constexpr float searchFieldHeight = 28.0f;
};

} // namespace ui
} // namespace dc
