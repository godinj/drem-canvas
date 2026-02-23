#pragma once

#include "graphics/core/Widget.h"
#include "vim/VimEngine.h"
#include "vim/VimContext.h"
#include "engine/TransportController.h"
#include "model/Arrangement.h"

namespace dc
{
namespace ui
{

class VimStatusBarWidget : public gfx::Widget,
                           public VimEngine::Listener
{
public:
    static constexpr float preferredHeight = 24.0f;

    VimStatusBarWidget (VimEngine& engine, VimContext& context,
                        Arrangement& arrangement, TransportController& transport);
    ~VimStatusBarWidget() override;

    void paint (gfx::Canvas& canvas) override;
    void animationTick (double timestampMs) override;

    // VimEngine::Listener
    void vimModeChanged (VimEngine::Mode newMode) override;
    void vimContextChanged() override;

private:
    VimEngine& engine;
    VimContext& context;
    Arrangement& arrangement;
    TransportController& transport;
};

} // namespace ui
} // namespace dc
