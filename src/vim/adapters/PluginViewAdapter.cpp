#include "vim/adapters/PluginViewAdapter.h"
#include "vim/ActionRegistry.h"
#include <algorithm>
#include <string>

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

PluginViewAdapter::PluginViewAdapter (VimContext& ctx)
    : context (ctx)
{
}

// ── Hint label generation ───────────────────────────────────────────────────

std::string PluginViewAdapter::generateHintLabel (int index, int totalCount)
{
    // Home-row keys for hint labels: a,s,d,f,g,h,j,k,l
    // All hints are uniform length to avoid prefix conflicts
    static const char keys[] = "asdfghjkl";
    static const int numKeys = 9;

    if (totalCount <= numKeys)
    {
        // Single-char hints: a, s, d, ...
        if (index < numKeys)
            return std::string (1, keys[index]);
    }
    else if (totalCount <= numKeys * numKeys)
    {
        // Two-char hints for ALL entries: aa, as, ad, ...
        int first = index / numKeys;
        int second = index % numKeys;
        if (first < numKeys)
            return std::string (1, keys[first])
                 + std::string (1, keys[second]);
    }

    // Three-char for > 81 params (unlikely but safe)
    int first = (index / (numKeys * numKeys)) % numKeys;
    int second = (index / numKeys) % numKeys;
    int third = index % numKeys;
    return std::string (1, keys[first])
         + std::string (1, keys[second])
         + std::string (1, keys[third]);
}

int PluginViewAdapter::resolveHintLabel (const std::string& label, int totalCount)
{
    static const char keys[] = "asdfghjkl";
    static const int numKeys = 9;

    auto indexOf = [] (char c) -> int
    {
        for (int i = 0; i < numKeys; ++i)
            if (keys[i] == c) return i;
        return -1;
    };

    // Determine expected label length based on totalCount
    int expectedLen = (totalCount <= numKeys) ? 1
                    : (totalCount <= numKeys * numKeys) ? 2
                    : 3;

    // Only resolve when label reaches the expected length
    if (static_cast<int> (label.size()) < expectedLen)
        return -1;

    if (expectedLen == 1)
    {
        int i = indexOf (label[0]);
        return (i >= 0 && i < totalCount) ? i : -1;
    }

    if (expectedLen == 2)
    {
        int first = indexOf (label[0]);
        int second = indexOf (label[1]);
        if (first < 0 || second < 0) return -1;
        int idx = first * numKeys + second;
        return (idx < totalCount) ? idx : -1;
    }

    // Three-char
    if (label.size() >= 3)
    {
        int first = indexOf (label[0]);
        int second = indexOf (label[1]);
        int third = indexOf (label[2]);
        if (first < 0 || second < 0 || third < 0) return -1;
        int idx = first * numKeys * numKeys + second * numKeys + third;
        return (idx < totalCount) ? idx : -1;
    }

    return -1;
}

// ── Key handling ────────────────────────────────────────────────────────────

bool PluginViewAdapter::handleRawKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Number entry mode
    if (context.isNumberEntryActive())
    {
        if (keyChar >= '0' && keyChar <= '9')
        {
            auto buf = context.getNumberBuffer();
            buf += std::string (1, char (keyChar));
            context.setNumberBuffer (buf);
            if (onContextChanged) onContextChanged();
            return true;
        }

        if (keyChar == '.' && context.getNumberBuffer().find ('.') == std::string::npos)
        {
            auto buf = context.getNumberBuffer();
            buf += ".";
            context.setNumberBuffer (buf);
            if (onContextChanged) onContextChanged();
            return true;
        }

        if (key == dc::KeyCode::Return)
        {
            float pct = std::stof (context.getNumberBuffer());
            pct = std::clamp (pct, 0.0f, 100.0f);
            if (onParamChanged)
                onParamChanged (context.getSelectedParamIndex(), pct / 100.0f);
            context.clearNumberEntry();
            if (onContextChanged) onContextChanged();
            return true;
        }

        if (isEscapeOrCtrlC (key))
        {
            context.clearNumberEntry();
            if (onContextChanged) onContextChanged();
            return true;
        }

        if (key == dc::KeyCode::Backspace)
        {
            auto buf = context.getNumberBuffer();
            if (! buf.empty())
            {
                buf.pop_back();
                context.setNumberBuffer (buf);
            }
            if (onContextChanged) onContextChanged();
            return true;
        }

        return true; // absorb other keys during number entry
    }

    // Hint mode (both HintActive and HintSpatial)
    if (context.getHintMode() == VimContext::HintActive
        || context.getHintMode() == VimContext::HintSpatial)
    {
        bool isSpatial = (context.getHintMode() == VimContext::HintSpatial);

        if (isEscapeOrCtrlC (key))
        {
            context.setHintMode (VimContext::HintNone);
            context.clearHintBuffer();
            if (onContextChanged) onContextChanged();
            return true;
        }

        // Accept home-row hint chars
        static const std::string hintChars ("asdfghjkl");
        if (hintChars.find (char (keyChar)) != std::string::npos)
        {
            auto buf = context.getHintBuffer() + std::string (1, char (keyChar));
            context.setHintBuffer (buf);

            int resolved = resolveHintLabel (buf, context.getHintTotalCount());
            if (resolved >= 0)
            {
                if (isSpatial)
                {
                    if (onResolveSpatialHint)
                        onResolveSpatialHint (resolved);
                    context.setSelectedParamIndex (resolved);
                }
                else
                {
                    context.setSelectedParamIndex (resolved);
                }

                context.setHintMode (VimContext::HintNone);
                context.clearHintBuffer();
                if (onContextChanged) onContextChanged();
                return true;
            }

            // Could be a partial match (first char of two-char label)
            if (onContextChanged) onContextChanged();
            return true;
        }

        // Non-hint char cancels hint mode
        context.setHintMode (VimContext::HintNone);
        context.clearHintBuffer();
        if (onContextChanged) onContextChanged();
        return true;
    }

    // Normal plugin view keys

    // Escape: close plugin view
    if (isEscapeOrCtrlC (key))
    {
        if (onClose) onClose();
        return true;
    }

    // f: enter hint mode (spatial if available, otherwise parameter list)
    if (keyChar == 'f')
    {
        int spatialCount = onQuerySpatialHintCount ? onQuerySpatialHintCount() : 0;
        if (spatialCount > 0)
        {
            context.setHintMode (VimContext::HintSpatial);
            context.setHintTotalCount (spatialCount);
        }
        else
        {
            int paramCount = onQueryParamCount ? onQueryParamCount() : 0;
            context.setHintMode (VimContext::HintActive);
            context.setHintTotalCount (paramCount);
        }
        context.clearHintBuffer();
        if (onContextChanged) onContextChanged();
        return true;
    }

    // j/k: navigate parameters
    if (keyChar == 'j')
    {
        context.setSelectedParamIndex (context.getSelectedParamIndex() + 1);
        if (onContextChanged) onContextChanged();
        return true;
    }

    if (keyChar == 'k')
    {
        int idx = context.getSelectedParamIndex();
        if (idx > 0)
            context.setSelectedParamIndex (idx - 1);
        if (onContextChanged) onContextChanged();
        return true;
    }

    // h/l: coarse adjust +/-5%
    if (keyChar == 'h')
    {
        if (onParamAdjust)
            onParamAdjust (context.getSelectedParamIndex(), -0.05f);
        if (onContextChanged) onContextChanged();
        return true;
    }

    if (keyChar == 'l')
    {
        if (onParamAdjust)
            onParamAdjust (context.getSelectedParamIndex(), 0.05f);
        if (onContextChanged) onContextChanged();
        return true;
    }

    // H/L: fine adjust +/-1%
    if (keyChar == 'H')
    {
        if (onParamAdjust)
            onParamAdjust (context.getSelectedParamIndex(), -0.01f);
        if (onContextChanged) onContextChanged();
        return true;
    }

    if (keyChar == 'L')
    {
        if (onParamAdjust)
            onParamAdjust (context.getSelectedParamIndex(), 0.01f);
        if (onContextChanged) onContextChanged();
        return true;
    }

    // 0-9: start number entry
    if (keyChar >= '0' && keyChar <= '9')
    {
        context.setNumberEntryActive (true);
        context.setNumberBuffer (std::string (1, char (keyChar)));
        if (onContextChanged) onContextChanged();
        return true;
    }

    // e: open native editor popup
    if (keyChar == 'e')
    {
        int trackIdx = context.getPluginViewTrackIndex();
        int pluginIdx = context.getPluginViewPluginIndex();
        if (onNativeEditorOpen)
            onNativeEditorOpen (trackIdx, pluginIdx);
        return true;
    }

    // z: toggle enlarged plugin view
    if (keyChar == 'z')
    {
        context.setPluginViewEnlarged (! context.isPluginViewEnlarged());
        if (onContextChanged) onContextChanged();
        return true;
    }

    // R: force spatial rescan
    if (keyChar == 'R')
    {
        if (onRescan) onRescan();
        return true;
    }

    // x: toggle drag axis (horizontal <-> vertical)
    if (keyChar == 'x')
    {
        if (onToggleDragAxis) onToggleDragAxis();
        return true;
    }

    // q: end active drag session without closing plugin view
    if (keyChar == 'q')
    {
        if (onEndDrag) onEndDrag();
        return true;
    }

    // c: toggle center-on-reverse
    if (keyChar == 'c')
    {
        if (onToggleDragCenter) onToggleDragCenter();
        return true;
    }

    // Space: toggle play/stop (handled at VimEngine level, not here)

    return false;
}

void PluginViewAdapter::registerActions (ActionRegistry& registry)
{
    // Plugin view-specific actions can be registered here in the future
}

} // namespace dc
