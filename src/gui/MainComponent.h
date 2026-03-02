#pragma once
#include <JuceHeader.h>
#include <filesystem>
#include <vector>
#include "engine/AudioEngine.h"
#include "engine/TransportController.h"
#include "engine/MixBusProcessor.h"
#include "engine/TrackProcessor.h"
#include "engine/StepSequencerProcessor.h"
#include "engine/MidiClipProcessor.h"
#include "engine/MetronomeProcessor.h"
#include "engine/MeterTapProcessor.h"
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
    void addTrackFromFile (const std::filesystem::path& file);
    void rebuildAudioGraph();
    void syncTrackProcessorsFromModel();
    void syncSequencerFromModel();
    void syncMidiClipFromModel (int trackIndex);
    void syncAudioClipFromModel (int trackIndex);
    void saveSession();
    void loadSession();

    // Plugin chain helpers
    void connectTrackPluginChain (int trackIndex);
    void disconnectTrackPluginChain (int trackIndex);
    void openPluginEditor (int trackIndex, int pluginIndex);
    void captureAllPluginStates();
    void insertPluginOnTrack (int trackIndex, const juce::PluginDescription& desc);
    void toggleBrowser();

    // Master bus plugin chain
    void connectMasterPluginChain();
    void disconnectMasterPluginChain();
    void insertPluginOnMaster (const juce::PluginDescription& desc);
    void openMasterPluginEditor (int pluginIndex);

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
    std::vector<TrackProcessor*> trackProcessors;              // non-owning; graph owns the processors
    std::vector<MidiClipProcessor*> midiClipProcessors;      // non-owning; graph owns (nullptr for audio tracks)
    std::vector<juce::AudioProcessorGraph::Node::Ptr> trackNodes;
    StepSequencerProcessor* sequencerProcessor = nullptr;      // non-owning; graph owns
    juce::AudioProcessorGraph::Node::Ptr sequencerNode;
    MetronomeProcessor* metronomeProcessor = nullptr;          // non-owning; graph owns
    juce::AudioProcessorGraph::Node::Ptr metronomeNode;
    std::vector<std::vector<PluginNodeInfo>> trackPluginChains;
    std::vector<MeterTapProcessor*> meterTapProcessors;              // non-owning; graph owns
    std::vector<juce::AudioProcessorGraph::Node::Ptr> meterTapNodes;

    // Master bus plugin chain
    std::vector<PluginNodeInfo> masterPluginChain;
    juce::AudioProcessorGraph::Node::Ptr masterMeterTapNode;
    MeterTapProcessor* masterMeterTapProcessor = nullptr;

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

    std::filesystem::path currentSessionDirectory;

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
