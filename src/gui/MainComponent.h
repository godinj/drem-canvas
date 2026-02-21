#pragma once
#include <JuceHeader.h>
#include "engine/AudioEngine.h"
#include "engine/TransportController.h"
#include "engine/MixBusProcessor.h"
#include "engine/TrackProcessor.h"
#include "model/Project.h"
#include "gui/transport/TransportBar.h"
#include "gui/arrangement/ArrangementView.h"
#include "gui/mixer/MixerPanel.h"
#include "gui/common/DremLookAndFeel.h"

namespace dc
{

class MainComponent : public juce::Component,
                      private juce::ValueTree::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void showAudioSettings();
    void openFile();
    void addTrackFromFile (const juce::File& file);
    void rebuildAudioGraph();
    void syncTrackProcessorsFromModel();
    void saveSession();
    void loadSession();

    // ValueTree listener
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    DremLookAndFeel lookAndFeel;

    // Engine
    AudioEngine audioEngine;
    TransportController transportController;
    juce::AudioProcessorGraph::Node::Ptr mixBusNode;
    juce::Array<TrackProcessor*> trackProcessors;              // non-owning; graph owns the processors
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> trackNodes;

    // Model
    Project project;

    // GUI
    TransportBar transportBar;
    std::unique_ptr<ArrangementView> arrangementView;
    std::unique_ptr<MixerPanel> mixerPanel;
    juce::TextButton saveSessionButton { "Save Session" };
    juce::TextButton loadSessionButton { "Load Session" };
    juce::TextButton audioSettingsButton { "Audio Settings" };
    juce::TextButton addTrackButton { "Import Audio" };

    juce::File currentSessionDirectory;

    juce::StretchableLayoutManager layout;
    juce::StretchableLayoutResizerBar layoutResizer { &layout, 1, false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace dc
