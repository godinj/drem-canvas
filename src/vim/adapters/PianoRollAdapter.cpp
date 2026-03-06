#include "vim/adapters/PianoRollAdapter.h"
#include "vim/ActionRegistry.h"
#include "model/Clipboard.h"
#include "dc/foundation/time.h"

namespace dc
{

static bool isEscapeOrCtrlC (const dc::KeyPress& key)
{
    if (key == dc::KeyCode::Escape)
        return true;

    if (key.control)
    {
        auto c = key.getTextCharacter();
        if (c == 3 || c == 'c' || c == 'C')
            return true;
    }

    return false;
}

PianoRollAdapter::PianoRollAdapter (VimContext& ctx)
    : context (ctx)
{
}

char PianoRollAdapter::consumeRegister()
{
    char reg = pendingRegister;
    pendingRegister = '\0';
    awaitingRegisterChar = false;
    return reg;
}

bool PianoRollAdapter::handleRawKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape / Ctrl-C closes piano roll
    if (isEscapeOrCtrlC (key))
    {
        if (onClose) onClose();
        return true;
    }

    // Register prefix ("x)
    if (awaitingRegisterChar)
    {
        char c = static_cast<char> (keyChar);
        if (Clipboard::isValidRegister (c) && c != '\0')
        {
            pendingRegister = c;
            awaitingRegisterChar = false;
            if (onContextChanged) onContextChanged();
            return true;
        }
        awaitingRegisterChar = false;
        if (onContextChanged) onContextChanged();
        return true;
    }

    if (keyChar == '"')
    {
        awaitingRegisterChar = true;
        if (onContextChanged) onContextChanged();
        return true;
    }

    // Ctrl+A selects all
    if (key.control && keyChar == 'a')
    {
        if (onSelectAll) onSelectAll();
        return true;
    }

    // Pending 'g' for gg (jump to highest note row)
    if (pendingKey == 'g')
    {
        if (keyChar == 'g'
            && (dc::currentTimeMillis() - pendingTimestamp) < pendingTimeoutMs)
        {
            pendingKey = 0;
            pendingTimestamp = 0;
            if (onJumpCursor) onJumpCursor (-1, 127);
            return true;
        }
        pendingKey = 0;
        pendingTimestamp = 0;
    }

    // Pending 'z' for zi/zo/zf
    if (pendingKey == 'z')
    {
        pendingKey = 0;
        pendingTimestamp = 0;
        if (keyChar == 'i')
        {
            if (onZoom) onZoom (1.25f);
            return true;
        }
        if (keyChar == 'o')
        {
            if (onZoom) onZoom (0.8f);
            return true;
        }
        if (keyChar == 'f')
        {
            if (onZoomToFit) onZoomToFit();
            return true;
        }
        return true; // consume unknown z-sequence
    }

    // Enter toggles note at cursor
    if (key == dc::KeyCode::Return)
    {
        if (onAddNote) onAddNote();
        return true;
    }

    // Tool switching
    if (keyChar == '1' || keyChar == 's')
    {
        if (onSetPianoRollTool) onSetPianoRollTool (0); // Select
        return true;
    }
    if (keyChar == '2' || keyChar == 'd')
    {
        if (onSetPianoRollTool) onSetPianoRollTool (1); // Draw
        return true;
    }
    if (keyChar == '3')
    {
        if (onSetPianoRollTool) onSetPianoRollTool (2); // Erase
        return true;
    }

    // Navigation hjkl
    if (keyChar == 'h') { if (onMoveCursor) onMoveCursor (-1, 0); return true; }
    if (keyChar == 'l') { if (onMoveCursor) onMoveCursor (1, 0); return true; }
    if (keyChar == 'k') { if (onMoveCursor) onMoveCursor (0, 1); return true; }
    if (keyChar == 'j') { if (onMoveCursor) onMoveCursor (0, -1); return true; }

    // Jump keys
    if (keyChar == '0') { if (onJumpCursor) onJumpCursor (0, -1); return true; }
    if (keyChar == '$')
    {
        if (onJumpCursor) onJumpCursor (99999, -1);
        return true;
    }
    if (keyChar == 'G') { if (onJumpCursor) onJumpCursor (-1, 0); return true; }
    if (keyChar == 'g')
    {
        pendingKey = 'g';
        pendingTimestamp = dc::currentTimeMillis();
        if (onContextChanged) onContextChanged();
        return true;
    }

    // Delete
    if (keyChar == 'x' || key == dc::KeyCode::Delete)
    {
        if (onDeleteSelected) onDeleteSelected (consumeRegister());
        return true;
    }

    // Yank (copy)
    if (keyChar == 'y')
    {
        if (onCopy) onCopy (consumeRegister());
        return true;
    }

    // Paste
    if (keyChar == 'p')
    {
        if (onPaste) onPaste (consumeRegister());
        return true;
    }

    // Duplicate
    if (keyChar == 'D')
    {
        if (onDuplicate) onDuplicate();
        return true;
    }

    // Transpose
    if (keyChar == '+' || keyChar == '=')
    {
        if (onTranspose) onTranspose (1);
        return true;
    }
    if (keyChar == '-')
    {
        if (onTranspose) onTranspose (-1);
        return true;
    }

    // Quantize / humanize
    if (keyChar == 'q')
    {
        if (onQuantize) onQuantize();
        return true;
    }
    if (keyChar == 'Q')
    {
        if (onHumanize) onHumanize();
        return true;
    }

    // Velocity lane toggle
    if (keyChar == 'v')
    {
        if (onVelocityLane) onVelocityLane (true);
        return true;
    }

    // Zoom
    if (keyChar == 'z')
    {
        pendingKey = 'z';
        pendingTimestamp = dc::currentTimeMillis();
        if (onContextChanged) onContextChanged();
        return true;
    }

    // Grid division coarser/finer
    if (keyChar == '[')
    {
        if (onGridDiv) onGridDiv (-1);
        return true;
    }
    if (keyChar == ']')
    {
        if (onGridDiv) onGridDiv (1);
        return true;
    }

    return false;
}

void PianoRollAdapter::registerActions (ActionRegistry& registry)
{
    // Piano roll-specific actions can be registered here in the future
}

} // namespace dc
