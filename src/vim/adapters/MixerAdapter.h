#pragma once
#include "vim/ContextAdapter.h"
#include "model/Arrangement.h"
#include "model/Project.h"
#include <functional>

namespace dc
{

class MixerAdapter : public ContextAdapter
{
public:
    MixerAdapter (Arrangement& arrangement, VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::Mixer; }

    // Mixer doesn't use the grammar (no operators/motions in vim sense)
    MotionRange resolveMotion (char32_t, int) const override { return {}; }
    MotionRange resolveLinewiseMotion (int) const override { return {}; }
    void executeMotion (char32_t, int) override {}
    void executeOperator (VimGrammar::Operator, const MotionRange&, char) override {}
    std::string getSupportedMotions() const override { return ""; }
    std::string getSupportedOperators() const override { return ""; }

    // Mixer handles all keys directly (focus cycling, strip selection, plugin ops)
    bool wantsRawKeys() const override { return true; }
    bool handleRawKey (const dc::KeyPress& key) override;

    void registerActions (ActionRegistry& registry) override;

    // Callbacks (moved from VimEngine)
    std::function<void (int trackIdx, int pluginIdx)> onMixerPluginOpen;
    std::function<void (int trackIdx)> onMixerPluginAdd;
    std::function<void (int trackIdx, int pluginIdx)> onMixerPluginRemove;
    std::function<void (int trackIdx, int pluginIdx)> onMixerPluginBypass;
    std::function<void (int trackIdx, int fromIdx, int toIdx)> onMixerPluginReorder;

    // Plugin view open callback (used when Return opens a plugin)
    std::function<void (int trackIdx, int pluginIdx)> onOpenPluginView;

    // Toggle browser callback
    std::function<void()> onToggleBrowser;

    using ContextChangedCallback = std::function<void()>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }

private:
    int getMixerPluginCount() const;

    Arrangement& arrangement;
    VimContext& context;
    ContextChangedCallback onContextChanged;
};

} // namespace dc
