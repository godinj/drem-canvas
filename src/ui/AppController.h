#pragma once

#include "graphics/core/Widget.h"
#include "graphics/core/EventDispatch.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/theme/Theme.h"
#include "engine/AudioEngine.h"
#include "engine/TransportController.h"
#include "engine/MixBusProcessor.h"
#include "engine/TrackProcessor.h"
#include "engine/StepSequencerProcessor.h"
#include "engine/MidiClipProcessor.h"
#include "engine/MidiEngine.h"
#include "model/Project.h"
#include "model/Arrangement.h"
#include "model/TempoMap.h"
#include "model/StepSequencer.h"
#include "vim/VimEngine.h"
#include "vim/VimContext.h"
#include "plugins/PluginManager.h"
#include "plugins/PluginHost.h"
#include "plugins/PluginWindowManager.h"
#include "ui/transport/TransportBarWidget.h"
#include "ui/vim/VimStatusBarWidget.h"
#include "ui/arrangement/ArrangementWidget.h"
#include "ui/mixer/MixerWidget.h"
#include "ui/sequencer/StepSequencerWidget.h"
#include "ui/midieditor/PianoRollWidget.h"
#include "ui/browser/BrowserWidget.h"
#include "ui/palette/CommandPaletteWidget.h"
#include "vim/ActionRegistry.h"

namespace dc
{
namespace ui
{

class AppController : public gfx::Widget,
                      private VimEngine::Listener
{
public:
    AppController();
    ~AppController() override;

    void paint (gfx::Canvas& canvas) override;
    void paintOverChildren (gfx::Canvas& canvas) override;
    void resized() override;
    bool keyDown (const gfx::KeyEvent& e) override;

    // Initialize the audio engine and all UI
    void initialise();

    // Access for wiring
    gfx::Renderer* getRenderer() { return renderer; }
    void setRenderer (gfx::Renderer* r);

private:
    // ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    // VimEngine::Listener
    void vimModeChanged (VimEngine::Mode newMode) override;
    void vimContextChanged() override;

    // Audio graph management
    void rebuildAudioGraph();
    void syncTrackProcessorsFromModel();
    void syncSequencerFromModel();
    void syncMidiClipFromModel (int trackIndex);

    // Plugin chain helpers
    struct PluginNodeInfo
    {
        juce::AudioProcessorGraph::Node::Ptr node;
        juce::AudioPluginInstance* plugin = nullptr;
    };

    void connectTrackPluginChain (int trackIndex);
    void disconnectTrackPluginChain (int trackIndex);
    void openPluginEditor (int trackIndex, int pluginIndex);
    void captureAllPluginStates();
    void insertPluginOnTrack (int trackIndex, const juce::PluginDescription& desc);

    // Session management
    void saveSession();
    void loadSession();
    void openFile();
    void addTrackFromFile (const juce::File& file);
    void addMidiTrack (const juce::String& name);
    void showAudioSettings();

    // Panel visibility
    void updatePanelVisibility();
    void toggleBrowser();

    // Command palette
    void registerAllActions();
    void showCommandPalette();
    void dismissCommandPalette();

    // ─── Plugin infrastructure ───────────────────────────
    PluginManager pluginManager;
    PluginHost pluginHost { pluginManager };
    PluginWindowManager pluginWindowManager;

    // ─── Engine ──────────────────────────────────────────
    AudioEngine audioEngine;
    TransportController transportController;
    juce::AudioProcessorGraph::Node::Ptr mixBusNode;
    juce::Array<TrackProcessor*> trackProcessors;
    juce::Array<MidiClipProcessor*> midiClipProcessors;
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> trackNodes;
    juce::Array<juce::Array<PluginNodeInfo>> trackPluginChains;
    StepSequencerProcessor* sequencerProcessor = nullptr;
    juce::AudioProcessorGraph::Node::Ptr sequencerNode;
    MidiEngine midiEngine;

    // ─── Model ───────────────────────────────────────────
    Project project;
    Arrangement arrangement { project };
    TempoMap tempoMap;
    VimContext vimContext;
    std::unique_ptr<VimEngine> vimEngine;
    ActionRegistry actionRegistry;

    juce::File currentSessionDirectory;

    // ─── UI widgets ──────────────────────────────────────
    std::unique_ptr<TransportBarWidget> transportBar;
    std::unique_ptr<VimStatusBarWidget> vimStatusBar;
    std::unique_ptr<ArrangementWidget> arrangementWidget;
    std::unique_ptr<MixerWidget> mixerWidget;
    std::unique_ptr<StepSequencerWidget> sequencerWidget;
    std::unique_ptr<PianoRollWidget> pianoRollWidget;
    std::unique_ptr<BrowserWidget> browserWidget;
    std::unique_ptr<CommandPaletteWidget> commandPalette;

    bool browserVisible = false;

    // Resizer bar position: fraction of center area occupied by top (arrangement)
    float splitRatio = 0.65f;

    gfx::Renderer* renderer = nullptr;
};

} // namespace ui
} // namespace dc
