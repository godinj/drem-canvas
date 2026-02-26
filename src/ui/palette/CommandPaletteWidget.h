#pragma once

#include "graphics/core/Widget.h"
#include "vim/ActionRegistry.h"
#include <functional>
#include <string>
#include <vector>

namespace dc
{
namespace ui
{

class CommandPaletteWidget : public gfx::Widget
{
public:
    CommandPaletteWidget (ActionRegistry& registry);

    void paint (gfx::Canvas& canvas) override;
    bool keyDown (const gfx::KeyEvent& e) override;

    void show (VimContext::Panel currentPanel);
    void dismiss();
    bool isShowing() const { return showing; }

    std::function<void()> onDismiss;

private:
    void updateResults();
    void executeSelected();

    ActionRegistry& registry;
    bool showing = false;
    VimContext::Panel currentPanel = VimContext::Editor;

    std::string searchBuffer;
    std::vector<ScoredAction> results;
    int selectedIndex = 0;
    float scrollOffset = 0.0f;

    static constexpr float paletteWidth = 500.0f;
    static constexpr float searchFieldHeight = 40.0f;
    static constexpr float rowHeight = 28.0f;
    static constexpr float categoryHeaderHeight = 22.0f;
    static constexpr int maxVisibleRows = 12;
    static constexpr float cornerRadius = 8.0f;
    static constexpr float padding = 8.0f;
};

} // namespace ui
} // namespace dc
