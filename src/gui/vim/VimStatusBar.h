#pragma once
#include <JuceHeader.h>
#include "vim/VimEngine.h"
#include "vim/VimContext.h"
#include "engine/TransportController.h"
#include "model/Arrangement.h"

namespace dc
{

class VimStatusBar : public juce::Component,
                     public VimEngine::Listener,
                     private juce::Timer
{
public:
    static constexpr int preferredHeight = 24;

    VimStatusBar (VimEngine& engine, VimContext& context,
                  Arrangement& arrangement, TransportController& transport);
    ~VimStatusBar() override;

    void paint (juce::Graphics& g) override;

    // VimEngine::Listener
    void vimModeChanged (VimEngine::Mode newMode) override;
    void vimContextChanged() override;

private:
    void timerCallback() override;

    VimEngine& engine;
    VimContext& context;
    Arrangement& arrangement;
    TransportController& transport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimStatusBar)
};

} // namespace dc
