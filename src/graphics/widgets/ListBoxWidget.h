#pragma once

#include "graphics/core/Widget.h"
#include "graphics/theme/Theme.h"
#include <functional>
#include <string>
#include <vector>

namespace dc
{
namespace gfx
{

class ListBoxWidget : public Widget
{
public:
    ListBoxWidget();

    void paint (Canvas& canvas) override;

    void mouseDown (const MouseEvent& e) override;
    void mouseDoubleClick (const MouseEvent& e) override;
    void mouseWheel (const WheelEvent& e) override;

    // Data model
    void setItems (const std::vector<std::string>& items);
    int getNumItems() const { return static_cast<int> (items.size()); }

    // Selection
    void setSelectedIndex (int index);
    int getSelectedIndex() const { return selectedIndex; }

    // Configuration
    void setRowHeight (float h) { rowHeight = h; repaint(); }
    float getRowHeight() const { return rowHeight; }

    // Callbacks
    std::function<void (int)> onSelectionChanged;
    std::function<void (int)> onDoubleClick;

    // Custom row painting (optional)
    std::function<void (Canvas&, int, const Rect&, bool)> customRowPaint;

private:
    int getVisibleRowStart() const;
    int getVisibleRowCount() const;
    void scrollToEnsureIndexVisible (int index);

    std::vector<std::string> items;
    int selectedIndex = -1;
    float rowHeight = 24.0f;
    float scrollOffset = 0.0f;
};

} // namespace gfx
} // namespace dc
