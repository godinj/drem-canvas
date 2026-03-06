#pragma once
#include "vim/ContextAdapter.h"
#include <functional>

namespace dc
{

class PluginViewAdapter : public ContextAdapter
{
public:
    explicit PluginViewAdapter (VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::PluginView; }

    MotionRange resolveMotion (char32_t, int) const override { return {}; }
    MotionRange resolveLinewiseMotion (int) const override { return {}; }
    void executeMotion (char32_t, int) override {}
    void executeOperator (VimGrammar::Operator, const MotionRange&, char) override {}
    std::string getSupportedMotions() const override { return ""; }
    std::string getSupportedOperators() const override { return ""; }

    bool wantsRawKeys() const override { return true; }
    bool handleRawKey (const dc::KeyPress& key) override;

    void registerActions (ActionRegistry& registry) override;

    // Hint label generation (static, keep accessible)
    static std::string generateHintLabel (int index, int totalCount);
    static int resolveHintLabel (const std::string& label, int totalCount);

    // Callbacks (moved from VimEngine)
    std::function<void()> onRescan;
    std::function<void()> onToggleDragAxis;
    std::function<void()> onToggleDragCenter;
    std::function<void()> onEndDrag;
    std::function<void (int paramIdx, float delta)> onParamAdjust;
    std::function<void (int paramIdx, float newValue)> onParamChanged;
    std::function<int()> onQuerySpatialHintCount;
    std::function<int (int spatialIdx)> onResolveSpatialHint;
    std::function<int()> onQueryParamCount;
    std::function<void()> onClose;
    std::function<void (int trackIdx, int pluginIdx)> onOpen;

    // Native editor open callback (reuses mixer plugin open)
    std::function<void (int trackIdx, int pluginIdx)> onNativeEditorOpen;

    using ContextChangedCallback = std::function<void()>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }

private:
    VimContext& context;
    ContextChangedCallback onContextChanged;
};

} // namespace dc
