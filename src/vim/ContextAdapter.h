#pragma once
#include "VimGrammar.h"
#include "VimContext.h"
#include "dc/foundation/keycode.h"
#include <string>

namespace dc
{

class ActionRegistry;

class ContextAdapter
{
public:
    virtual ~ContextAdapter() = default;

    // Identity
    virtual VimContext::Panel getPanel() const = 0;

    // --- Motion resolution ---
    struct MotionRange
    {
        int startIndex     = 0;   // primary axis start (track, strip, row)
        int endIndex       = 0;   // primary axis end
        int startSecondary = 0;   // secondary axis start (clip, step, beat)
        int endSecondary   = 0;   // secondary axis end
        int64_t startPos   = 0;   // time position start (samples)
        int64_t endPos     = 0;   // time position end (samples)
        bool linewise      = false;
        bool valid         = false;
    };

    virtual MotionRange resolveMotion (char32_t motionKey, int count) const = 0;
    virtual MotionRange resolveLinewiseMotion (int count) const = 0;

    // --- Execute standalone motion (cursor movement without operator) ---
    virtual void executeMotion (char32_t motionKey, int count) = 0;

    // --- Execute operator on range ---
    virtual void executeOperator (VimGrammar::Operator op,
                                  const MotionRange& range, char reg) = 0;

    // --- Supported grammar characters for this panel ---
    virtual std::string getSupportedMotions() const = 0;
    virtual std::string getSupportedOperators() const = 0;

    // --- Raw key handling for sub-modes (hint mode, number entry, etc.) ---
    // Return true if this panel wants to intercept keys before grammar/keymap.
    virtual bool wantsRawKeys() const { return false; }
    virtual bool handleRawKey (const dc::KeyPress& key) { return false; }

    // --- Register panel-specific actions with the action registry ---
    virtual void registerActions (ActionRegistry& registry) = 0;

    // --- Visual mode support ---
    virtual void enterVisualMode() {}
    virtual void enterVisualLineMode() {}
    virtual void exitVisualMode() {}
    virtual void updateVisualSelection() {}
    virtual bool handleVisualKey (const dc::KeyPress& key) { return false; }
    virtual bool handleVisualLineKey (const dc::KeyPress& key) { return false; }
};

} // namespace dc
