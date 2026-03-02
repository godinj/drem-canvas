#pragma once

#include "graphics/core/Widget.h"
#include "graphics/widgets/ListBoxWidget.h"
#include "graphics/widgets/ButtonWidget.h"
#include "graphics/widgets/ProgressBarWidget.h"
#include "graphics/widgets/LabelWidget.h"
#include "plugins/PluginManager.h"
#include "dc/plugins/PluginDescription.h"
#include <atomic>
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

    /// Start async plugin scan with progress display
    void startAsyncScan();

    /// Called each frame to check for scan completion
    void tick();

    std::function<void (const dc::PluginDescription&)> onPluginSelected;

private:
    void filterPlugins();

    PluginManager& pluginManager;
    gfx::ButtonWidget scanButton;
    gfx::ProgressBarWidget progressBar;
    gfx::LabelWidget scanStatusLabel { "", gfx::LabelWidget::Centre };
    gfx::ListBoxWidget pluginList;
    std::vector<dc::PluginDescription> displayedPlugins;
    std::string searchBuffer;
    bool searchActive = false;
    float searchFieldY_ = 36.0f;

    // Async scan progress state
    std::atomic<bool> scanInProgress_ { false };
    std::atomic<int> scanCurrent_ { 0 };
    std::atomic<int> scanTotal_ { 0 };
    std::atomic<bool> scanResultReady_ { false };
    std::atomic<bool> scanNameDirty_ { false };
    std::string scanPluginName_;

    static constexpr float searchFieldHeight = 28.0f;
};

} // namespace ui
} // namespace dc
