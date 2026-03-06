#pragma once
#include "vim/ContextAdapter.h"
#include <functional>

namespace dc
{

class PianoRollAdapter : public ContextAdapter
{
public:
    explicit PianoRollAdapter (VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::PianoRoll; }

    MotionRange resolveMotion (char32_t, int) const override { return {}; }
    MotionRange resolveLinewiseMotion (int) const override { return {}; }
    void executeMotion (char32_t, int) override {}
    void executeOperator (VimGrammar::Operator, const MotionRange&, char) override {}
    std::string getSupportedMotions() const override { return ""; }
    std::string getSupportedOperators() const override { return ""; }

    bool wantsRawKeys() const override { return true; }
    bool handleRawKey (const dc::KeyPress& key) override;

    void registerActions (ActionRegistry& registry) override;

    // Callbacks (moved from VimEngine)
    std::function<void (int tool)> onSetPianoRollTool;
    std::function<void (char reg)> onDeleteSelected;
    std::function<void (char reg)> onCopy;
    std::function<void (char reg)> onPaste;
    std::function<void()> onDuplicate;
    std::function<void (int)> onTranspose;
    std::function<void()> onSelectAll;
    std::function<void()> onQuantize;
    std::function<void()> onHumanize;
    std::function<void (bool)> onVelocityLane;
    std::function<void (float)> onZoom;
    std::function<void()> onZoomToFit;
    std::function<void (int)> onGridDiv;
    std::function<void (int, int)> onMoveCursor;
    std::function<void()> onAddNote;
    std::function<void (int, int)> onJumpCursor;
    std::function<void()> onClose;

    using ContextChangedCallback = std::function<void()>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }

private:
    VimContext& context;

    // Register prefix state ("x prefix)
    char pendingRegister = '\0';
    bool awaitingRegisterChar = false;
    char consumeRegister();

    // Pending key state (for gg, zi/zo/zf)
    char32_t pendingKey = 0;
    int64_t pendingTimestamp = 0;
    static constexpr int64_t pendingTimeoutMs = 1000;

    ContextChangedCallback onContextChanged;
};

} // namespace dc
