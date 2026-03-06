#pragma once
#include "vim/ContextAdapter.h"
#include "model/Project.h"
#include <functional>

namespace dc
{

class SequencerAdapter : public ContextAdapter
{
public:
    SequencerAdapter (Project& project, VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::Sequencer; }

    // Sequencer doesn't use grammar operators
    MotionRange resolveMotion (char32_t, int) const override { return {}; }
    MotionRange resolveLinewiseMotion (int) const override { return {}; }
    void executeMotion (char32_t, int) override {}
    void executeOperator (VimGrammar::Operator, const MotionRange&, char) override {}
    std::string getSupportedMotions() const override { return ""; }
    std::string getSupportedOperators() const override { return ""; }

    // Sequencer handles all keys directly
    bool wantsRawKeys() const override { return true; }
    bool handleRawKey (const dc::KeyPress& key) override;

    void registerActions (ActionRegistry& registry) override;

    using ContextChangedCallback = std::function<void()>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }

private:
    // Navigation
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void jumpFirstStep();
    void jumpLastStep();
    void jumpFirstRow();
    void jumpLastRow();

    // Editing
    void toggleStep();
    void adjustVelocity (int delta);
    void cycleVelocity();
    void toggleRowMute();
    void toggleRowSolo();

    Project& project;
    VimContext& context;

    // Pending key state for gg
    char32_t pendingKey = 0;
    int64_t pendingTimestamp = 0;
    static constexpr int64_t pendingTimeoutMs = 1000;

    ContextChangedCallback onContextChanged;
};

} // namespace dc
