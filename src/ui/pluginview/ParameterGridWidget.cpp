#include "ParameterGridWidget.h"
#include "dc/foundation/string_utils.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"
#include <algorithm>

namespace dc
{
namespace ui
{

ParameterGridWidget::ParameterGridWidget()
{
}

ParameterGridWidget::~ParameterGridWidget()
{
}

void ParameterGridWidget::setPlugin (juce::AudioPluginInstance* plugin)
{
    currentPlugin = plugin;
    parameters.clear();

    if (plugin != nullptr)
    {
        auto& params = plugin->getParameters();
        for (auto* p : params)
            parameters.push_back (p);
    }

    selectedParam = 0;
    repaint();
}

void ParameterGridWidget::clearPlugin()
{
    currentPlugin = nullptr;
    parameters.clear();
    selectedParam = 0;
    repaint();
}

void ParameterGridWidget::setSelectedParamIndex (int index)
{
    selectedParam = std::clamp (index, 0, std::max (0, static_cast<int> (parameters.size()) - 1));
    repaint();
}

void ParameterGridWidget::setHintMode (VimContext::HintMode mode)
{
    hintMode = mode;
    repaint();
}

void ParameterGridWidget::setHintBuffer (const std::string& buffer)
{
    hintBuffer = buffer;
    repaint();
}

void ParameterGridWidget::setNumberEntryActive (bool active)
{
    numberEntry = active;
    repaint();
}

void ParameterGridWidget::setNumberBuffer (const std::string& buffer)
{
    numberBuffer = buffer;
    repaint();
}

void ParameterGridWidget::setSpatialHintMap (std::unordered_map<int, std::string> map)
{
    spatialHintMap = std::move (map);
    repaint();
}

int ParameterGridWidget::getNumParameters() const
{
    return static_cast<int> (parameters.size());
}

void ParameterGridWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& fm = FontManager::getInstance();
    auto& font = fm.getDefaultFont();
    auto& theme = Theme::getDefault();
    float w = getWidth();
    float h = getHeight();

    // Background
    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff1e1e2e));

    if (currentPlugin == nullptr || parameters.empty())
    {
        canvas.drawText ("No parameters", 10.0f, h * 0.5f + 5.0f,
                         font, Color::fromARGB (0xff585b70));
        return;
    }

    // Scroll offset to keep selected param visible
    float visibleRows = h / rowHeight;
    float scrollOffset = 0.0f;

    if (selectedParam >= static_cast<int> (visibleRows) - 1)
        scrollOffset = (selectedParam - visibleRows + 2) * rowHeight;

    for (int i = 0; i < static_cast<int> (parameters.size()); ++i)
    {
        float rowY = i * rowHeight - scrollOffset;

        // Skip rows outside visible area
        if (rowY + rowHeight < 0.0f || rowY > h)
            continue;

        auto* param = parameters[i];
        bool isSelected = (i == selectedParam);
        float value = param->getValue();

        // Selection highlight
        if (isSelected)
            canvas.fillRect (Rect (0, rowY, w, rowHeight), Color::fromARGB (0xff2a2a3e));

        float x = 4.0f;

        // Hint label column
        // When spatial data is available, show spatial hint labels (matching the overlay).
        // Otherwise fall back to generated hints for HintActive mode.
        {
            std::string hintLabel;
            bool hasSpatial = ! spatialHintMap.empty();

            if (hasSpatial)
            {
                auto it = spatialHintMap.find (i);
                if (it != spatialHintMap.end())
                    hintLabel = it->second;
            }
            else
            {
                hintLabel = VimEngine::generateHintLabel (i, static_cast<int> (parameters.size()));
            }

            if (! hintLabel.empty())
            {
                bool isHinting = (hintMode == VimContext::HintActive
                               || hintMode == VimContext::HintSpatial);
                bool matches = ! isHinting || hintBuffer.empty()
                             || dc::startsWith (hintLabel, hintBuffer);

                Color hintColor = isHinting
                    ? (matches ? Color::fromARGB (0xffffcc00) : Color::fromARGB (0xff45475a))
                    : Color::fromARGB (0xff585b70);

                canvas.drawText (hintLabel, x, rowY + rowHeight * 0.5f + 5.0f,
                                 fm.getMonoFont(), hintColor);
            }
        }
        x += hintColWidth;

        // Parameter name
        auto name = param->getName (24).toStdString();
        Color nameColor = isSelected
            ? Color::fromARGB (0xffcdd6f4)
            : Color::fromARGB (0xffa6adc8);
        canvas.drawText (name, x, rowY + rowHeight * 0.5f + 5.0f, font, nameColor);
        x += nameColWidth;

        // Value bar background
        float barX = x;
        float barY = rowY + 4.0f;
        float barH = rowHeight - 8.0f;
        canvas.fillRect (Rect (barX, barY, barColWidth, barH), Color::fromARGB (0xff313244));

        // Value bar fill
        float fillW = value * barColWidth;
        Color barColor = isSelected ? theme.selection : theme.accent;
        canvas.fillRect (Rect (barX, barY, fillW, barH), barColor);
        x += barColWidth + 8.0f;

        // Value text
        auto valueText = param->getCurrentValueAsText().toStdString();
        canvas.drawText (valueText, x, rowY + rowHeight * 0.5f + 5.0f,
                         fm.getMonoFont(), Color::fromARGB (0xffa6adc8));

        // Selection cursor bar (left edge)
        if (isSelected)
        {
            canvas.fillRect (Rect (0, rowY, 3.0f, rowHeight), theme.selection);
        }
    }
}

} // namespace ui
} // namespace dc
