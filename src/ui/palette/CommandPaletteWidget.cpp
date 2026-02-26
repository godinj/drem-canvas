#include "CommandPaletteWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"
#include "include/core/SkFont.h"

namespace dc
{
namespace ui
{

CommandPaletteWidget::CommandPaletteWidget (ActionRegistry& reg)
    : registry (reg)
{
    setFocusable (true);
}

void CommandPaletteWidget::show (VimContext::Panel panel)
{
    showing = true;
    currentPanel = panel;
    searchBuffer.clear();
    selectedIndex = 0;
    scrollOffset = 0.0f;
    updateResults();
    setVisible (true);
    repaint();
}

void CommandPaletteWidget::dismiss()
{
    showing = false;
    searchBuffer.clear();
    results.clear();
    setVisible (false);
    repaint();

    if (onDismiss)
        onDismiss();
}

void CommandPaletteWidget::updateResults()
{
    results = registry.search (searchBuffer, currentPanel);

    if (selectedIndex >= static_cast<int> (results.size()))
        selectedIndex = std::max (0, static_cast<int> (results.size()) - 1);
}

void CommandPaletteWidget::executeSelected()
{
    if (selectedIndex >= 0 && selectedIndex < static_cast<int> (results.size()))
    {
        auto* action = results[static_cast<size_t> (selectedIndex)].action;
        dismiss();

        if (action->execute)
            action->execute();
    }
}

void CommandPaletteWidget::paint (gfx::Canvas& canvas)
{
    if (! showing)
        return;

    auto& theme = gfx::Theme::getDefault();
    auto& fontMgr = gfx::FontManager::getInstance();
    auto& font = fontMgr.getDefaultFont();
    auto& smallFont = fontMgr.getSmallFont();

    float w = getWidth();
    float paletteX = (w - paletteWidth) * 0.5f;
    float paletteY = 0.0f;

    // Calculate total height
    int visibleCount = std::min (static_cast<int> (results.size()), maxVisibleRows);
    float resultsHeight = static_cast<float> (visibleCount) * rowHeight;
    float totalHeight = searchFieldHeight + resultsHeight;

    // Background + border
    gfx::Rect bgRect (paletteX, paletteY, paletteWidth, totalHeight);
    canvas.fillRoundedRect (bgRect, cornerRadius, theme.widgetBackground);
    canvas.strokeRect (bgRect, theme.outlineColor, 1.0f);

    // ─── Search field ────────────────────────────────────────
    gfx::Rect searchRect (paletteX, paletteY, paletteWidth, searchFieldHeight);
    canvas.fillRoundedRect (searchRect, cornerRadius, gfx::Color::fromARGB (0xff222233));

    // Search prompt
    float textY = paletteY + searchFieldHeight * 0.5f + 5.0f;
    canvas.drawText (">", paletteX + padding, textY, font, theme.accent);

    // Search text
    float searchTextX = paletteX + padding + 16.0f;
    if (! searchBuffer.empty())
    {
        canvas.drawText (searchBuffer, searchTextX, textY, font, theme.defaultText);
    }
    else
    {
        canvas.drawText ("Find action...", searchTextX, textY, font, theme.dimText);
    }

    // Blinking cursor (always visible when palette is showing)
    {
        SkScalar textWidth = 0.0f;
        if (! searchBuffer.empty())
            textWidth = font.measureText (searchBuffer.data(), searchBuffer.size(), SkTextEncoding::kUTF8);

        float cursorX = searchTextX + textWidth;
        canvas.drawLine (cursorX, paletteY + 10.0f, cursorX, paletteY + searchFieldHeight - 10.0f,
                         theme.defaultText, 1.5f);
    }

    // Separator below search field
    float sepY = paletteY + searchFieldHeight;
    canvas.drawLine (paletteX, sepY, paletteX + paletteWidth, sepY, theme.outlineColor);

    // ─── Results list ────────────────────────────────────────
    canvas.save();
    gfx::Rect clipRect (paletteX, sepY, paletteWidth, resultsHeight);
    canvas.clipRect (clipRect);

    std::string lastCategory;

    for (int i = 0; i < visibleCount; ++i)
    {
        int resultIdx = i + static_cast<int> (scrollOffset);
        if (resultIdx >= static_cast<int> (results.size()))
            break;

        const auto& scored = results[static_cast<size_t> (resultIdx)];
        const auto* action = scored.action;

        float rowY = sepY + static_cast<float> (i) * rowHeight;

        // Selection highlight
        if (resultIdx == selectedIndex)
        {
            gfx::Rect selRect (paletteX, rowY, paletteWidth, rowHeight);
            canvas.fillRect (selRect, theme.selection.withAlpha ((uint8_t) 50));
        }

        // Category header (inline, dimmed)
        float textOffsetX = paletteX + padding;

        if (action->category != lastCategory)
        {
            lastCategory = action->category;
            // Draw category tag
            canvas.drawText (action->category, textOffsetX, rowY + rowHeight * 0.5f + 4.0f,
                             smallFont, theme.dimText);
            textOffsetX += 70.0f;
        }
        else
        {
            textOffsetX += 70.0f;
        }

        // Action name
        gfx::Color nameColor = (resultIdx == selectedIndex)
                                    ? theme.brightText
                                    : theme.defaultText;
        canvas.drawText (action->name, textOffsetX, rowY + rowHeight * 0.5f + 4.0f,
                         font, nameColor);

        // Keybinding hint (right-aligned)
        if (! action->keybinding.empty())
        {
            gfx::Rect kbRect (paletteX + paletteWidth - 120.0f, rowY,
                               120.0f - padding, rowHeight);
            canvas.drawTextRight (action->keybinding, kbRect, smallFont, theme.dimText);
        }
    }

    canvas.restore();
}

bool CommandPaletteWidget::keyDown (const gfx::KeyEvent& e)
{
    if (! showing)
        return false;

    // Escape — dismiss
    if (e.keyCode == 0x35 || (e.character == 27)) // macOS escape or raw ESC
    {
        dismiss();
        return true;
    }

    // Backspace
    if (e.keyCode == 0x33 || e.character == 8) // macOS backspace or BS char
    {
        if (searchBuffer.empty())
        {
            dismiss();
            return true;
        }
        searchBuffer.pop_back();
        selectedIndex = 0;
        scrollOffset = 0.0f;
        updateResults();
        repaint();
        return true;
    }

    // Enter — execute selected
    if (e.keyCode == 0x24 || e.character == 13) // macOS return or CR
    {
        executeSelected();
        return true;
    }

    // Up arrow or Ctrl+K
    if (e.keyCode == 0x7E || (e.control && (e.character == 'k' || e.character == 11)))
    {
        if (selectedIndex > 0)
        {
            --selectedIndex;
            // Scroll up if needed
            if (selectedIndex < static_cast<int> (scrollOffset))
                scrollOffset = static_cast<float> (selectedIndex);
            repaint();
        }
        return true;
    }

    // Down arrow or Ctrl+J
    if (e.keyCode == 0x7D || (e.control && (e.character == 'j' || e.character == 10)))
    {
        if (selectedIndex < static_cast<int> (results.size()) - 1)
        {
            ++selectedIndex;
            // Scroll down if needed
            if (selectedIndex >= static_cast<int> (scrollOffset) + maxVisibleRows)
                scrollOffset = static_cast<float> (selectedIndex - maxVisibleRows + 1);
            repaint();
        }
        return true;
    }

    // Tab — move down (like VS Code)
    if (e.keyCode == 0x30 || e.character == 9) // tab
    {
        if (selectedIndex < static_cast<int> (results.size()) - 1)
        {
            ++selectedIndex;
            if (selectedIndex >= static_cast<int> (scrollOffset) + maxVisibleRows)
                scrollOffset = static_cast<float> (selectedIndex - maxVisibleRows + 1);
            repaint();
        }
        return true;
    }

    // Printable characters — append to search
    auto ch = e.character;
    if (ch >= 32 && ch < 127 && ! e.control && ! e.command)
    {
        searchBuffer += static_cast<char> (ch);
        selectedIndex = 0;
        scrollOffset = 0.0f;
        updateResults();
        repaint();
        return true;
    }

    // Consume all other keys while palette is showing
    return true;
}

} // namespace ui
} // namespace dc
