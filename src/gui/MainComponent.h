#pragma once
#include <JuceHeader.h>
#include "engine/AudioEngine.h"
#include "engine/TransportController.h"
#include "engine/MixBusProcessor.h"
#include "engine/TrackProcessor.h"
#include "engine/StepSequencerProcessor.h"
#include "engine/MidiClipProcessor.h"
#include "model/Project.h"
#include "model/Arrangement.h"
#include "model/StepSequencer.h"
#include "model/TempoMap.h"
#include "vim/VimEngine.h"
#include "vim/VimContext.h"
#include "gui/transport/TransportBar.h"
#include "gui/arrangement/ArrangementView.h"
#include "gui/mixer/MixerPanel.h"
#include "gui/sequencer/StepSequencerView.h"
#include "gui/vim/VimStatusBar.h"
#include "gui/common/DremLookAndFeel.h"
#include "plugins/PluginManager.h"
#include "plugins/PluginHost.h"
#include "plugins/PluginWindowManager.h"

namespace dc
{

class MainComponent : public juce::Component,
                      private juce::ValueTree::Listener,
                      private VimEngine::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void showAudioSettings();
    void openFile();
    void addTrackFromFile (const juce::File& file);
    void rebuildAudioGraph();
    void syncTrackProcessorsFromModel();
    void syncSequencerFromModel();
    void syncMidiClipFromModel (int trackIndex);
    void saveSession();
    void loadSession();

    // Plugin chain helpers
    void connectTrackPluginChain (int trackIndex);
    void disconnectTrackPluginChain (int trackIndex);
    void openPluginEditor (int trackIndex, int pluginIndex);
    void captureAllPluginStates();
    void insertPluginOnTrack (int trackIndex, const juce::PluginDescription& desc);
    void toggleBrowser();

    // ValueTree listener
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    // VimEngine::Listener
    void vimModeChanged (VimEngine::Mode newMode) override;
    void vimContextChanged() override;

    void updatePanelVisibility();

    DremLookAndFeel lookAndFeel;

    // Plugin infrastructure
    PluginManager pluginManager;
    PluginHost pluginHost { pluginManager };
    PluginWindowManager pluginWindowManager;

    struct PluginNodeInfo
    {
        juce::AudioProcessorGraph::Node::Ptr node;
        juce::AudioPluginInstance* plugin = nullptr;  // non-owning; graph owns
    };

    // Engine
    AudioEngine audioEngine;
    TransportController transportController;
    juce::AudioProcessorGraph::Node::Ptr mixBusNode;
    juce::Array<TrackProcessor*> trackProcessors;              // non-owning; graph owns the processors
    juce::Array<MidiClipProcessor*> midiClipProcessors;      // non-owning; graph owns (nullptr for audio tracks)
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> trackNodes;
    StepSequencerProcessor* sequencerProcessor = nullptr;      // non-owning; graph owns
    juce::AudioProcessorGraph::Node::Ptr sequencerNode;
    juce::Array<juce::Array<PluginNodeInfo>> trackPluginChains;

    // Model
    Project project;
    Arrangement arrangement { project };
    TempoMap tempoMap;
    VimContext vimContext;
    std::unique_ptr<VimEngine> vimEngine;

    // GUI
    TransportBar transportBar;
    std::unique_ptr<ArrangementView> arrangementView;
    std::unique_ptr<MixerPanel> mixerPanel;
    std::unique_ptr<StepSequencerView> sequencerView;
    juce::TextButton saveSessionButton { "Save Session" };
    juce::TextButton loadSessionButton { "Load Session" };
    juce::TextButton audioSettingsButton { "Audio Settings" };
    juce::TextButton addTrackButton { "Import Audio" };

    juce::File currentSessionDirectory;

    // Browser panel
    std::unique_ptr<juce::Component> browserPanel;
    bool browserVisible = false;
    juce::TextButton browserToggleButton { "Plugins" };

    std::unique_ptr<VimStatusBar> vimStatusBar;

    juce::StretchableLayoutManager layout;
    juce::StretchableLayoutResizerBar layoutResizer { &layout, 1, false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace dc
