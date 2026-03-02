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

void ParameterGridWidget::setPlugin (dc::PluginInstance* plugin)
{
    currentPlugin = plugin;
    selectedParam = 0;
    repaint();
}

void ParameterGridWidget::clearPlugin()
{
    currentPlugin = nullptr;
    selectedParam = 0;
    repaint();
}

void ParameterGridWidget::setSelectedParamIndex (int index)
{
    int count = getNumParameters();
    selectedParam = std::clamp (index, 0, std::max (0, count - 1));
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

void ParameterGridWidget::setSpatialResults (const std::vector<SpatialParamInfo>& results,
                                              dc::PluginInstance* plugin)
{
    spatialResults = results;
    spatialMode = true;
    pluginForValues = plugin;
    selectedParam = 0;
    repaint();
}

void ParameterGridWidget::clearSpatialResults()
{
    spatialResults.clear();
    spatialMode = false;
    pluginForValues = nullptr;
    repaint();
}

int ParameterGridWidget::getNumParameters() const
{
    if (spatialMode)
        return static_cast<int> (spatialResults.size());
    if (currentPlugin != nullptr)
        return currentPlugin->getNumParameters();
    return 0;
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

    int numParams = getNumParameters();
    if (numParams == 0)
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

    for (int i = 0; i < numParams; ++i)
    {
        float rowY = i * rowHeight - scrollOffset;

        // Skip rows outside visible area
        if (rowY + rowHeight < 0.0f || rowY > h)
            continue;

        bool isSelected = (i == selectedParam);

        // Selection highlight
        if (isSelected)
            canvas.fillRect (Rect (0, rowY, w, rowHeight), Color::fromARGB (0xff2a2a3e));

        float x = 4.0f;

        if (spatialMode)
        {
            // --- Spatial mode: render from spatialResults ---
            auto& info = spatialResults[static_cast<size_t> (i)];
            bool isMapped = (info.paramIndex >= 0);

            // Hint label
            {
                bool isHinting = (hintMode == VimContext::HintActive
                               || hintMode == VimContext::HintSpatial);
                bool matches = ! isHinting || hintBuffer.empty()
                             || dc::startsWith (info.hintLabel, hintBuffer);

                Color hintColor = isHinting
                    ? (matches ? Color::fromARGB (0xffffcc00) : Color::fromARGB (0xff45475a))
                    : Color::fromARGB (0xff585b70);

                canvas.drawText (info.hintLabel, x, rowY + rowHeight * 0.5f + 5.0f,
                                 fm.getMonoFont(), hintColor);
            }
            x += hintColWidth;

            // Parameter name
            auto name = info.name.substr (0, 24);
            Color nameColor;
            if (isMapped)
                nameColor = isSelected ? Color::fromARGB (0xffcdd6f4) : Color::fromARGB (0xffa6adc8);
            else
                nameColor = isSelected ? Color::fromARGB (0xffa6adc8) : Color::fromARGB (0xff7f849c);
            canvas.drawText (name, x, rowY + rowHeight * 0.5f + 5.0f, font, nameColor);
            x += nameColWidth;

            if (isMapped && pluginForValues != nullptr
                && info.paramIndex < pluginForValues->getNumParameters())
            {
                float value = pluginForValues->getParameterValue (info.paramIndex);

                // Value bar
                float barX = x;
                float barY = rowY + 4.0f;
                float barH = rowHeight - 8.0f;
                canvas.fillRect (Rect (barX, barY, barColWidth, barH), Color::fromARGB (0xff313244));
                float fillW = value * barColWidth;
                Color barColor = isSelected ? theme.selection : theme.accent;
                canvas.fillRect (Rect (barX, barY, fillW, barH), barColor);
                x += barColWidth + 8.0f;

                // Value text
                auto valueText = pluginForValues->getParameterDisplay (info.paramIndex);
                canvas.drawText (valueText, x, rowY + rowHeight * 0.5f + 5.0f,
                                 fm.getMonoFont(), Color::fromARGB (0xffa6adc8));
            }
            else
            {
                // Unmapped — show "--" placeholder
                canvas.drawText ("--", x, rowY + rowHeight * 0.5f + 5.0f,
                                 fm.getMonoFont(), Color::fromARGB (0xff585b70));
            }
        }
        else
        {
            // --- dc::PluginInstance param mode ---
            float value = currentPlugin->getParameterValue (i);

            // Hint label
            {
                std::string hintLabel = VimEngine::generateHintLabel (i, numParams);

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
            auto name = currentPlugin->getParameterName (i);
            if (name.size() > 24)
                name = name.substr (0, 24);
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
            auto valueText = currentPlugin->getParameterDisplay (i);
            canvas.drawText (valueText, x, rowY + rowHeight * 0.5f + 5.0f,
                             fm.getMonoFont(), Color::fromARGB (0xffa6adc8));
        }

        // Selection cursor bar (left edge)
        if (isSelected)
        {
            canvas.fillRect (Rect (0, rowY, 3.0f, rowHeight), theme.selection);
        }
    }
}

} // namespace ui
} // namespace dc
