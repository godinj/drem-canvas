#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ListBoxWidget.h"
#include "graphics/widgets/ButtonWidget.h"
#include "plugins/PluginManager.h"
#include "dc/plugins/PluginDescription.h"
#include <functional>
#include <string>
#include <vector>

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

    // External filter API (driven by VimEngine)
    void setSearchFilter (const std::string& query);
    void clearSearchFilter();

    // Keyboard navigation
    int getNumPlugins() const;
    int getSelectedPluginIndex() const;
    void selectPlugin (int index);
    void moveSelection (int delta);
    void scrollByHalfPage (int direction);
    void confirmSelection();

    std::function<void (const dc::PluginDescription&)> onPluginSelected;

private:
    void filterPlugins();

    PluginManager& pluginManager;
    gfx::ButtonWidget scanButton;
    gfx::ListBoxWidget pluginList;
    std::vector<dc::PluginDescription> displayedPlugins;
    std::string searchBuffer;
    bool searchActive = false;

    static constexpr float searchFieldHeight = 28.0f;
};

} // namespace ui
} // namespace dc
