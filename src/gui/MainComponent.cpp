#include "MainComponent.h"
#include "model/Track.h"
#include "model/AudioClip.h"

namespace dc
{

MainComponent::MainComponent()
    : transportBar (transportController)
{
    setLookAndFeel (&lookAndFeel);

    // Initialise audio engine with stereo I/O
    audioEngine.initialise (2, 2);
    transportController.setSampleRate (audioEngine.getDeviceManager().getCurrentAudioDevice()
        ? audioEngine.getDeviceManager().getCurrentAudioDevice()->getCurrentSampleRate()
        : 44100.0);

    // Create mix bus processor and add to graph
    mixBusProcessor = std::make_unique<MixBusProcessor>();
    mixBusNode = audioEngine.addProcessor (std::make_unique<MixBusProcessor>());

    // Keep a reference to the actual processor in the graph
    mixBusProcessor.reset();
    auto* mixBusInGraph = dynamic_cast<MixBusProcessor*> (mixBusNode->getProcessor());

    // Connect mix bus output to audio output
    audioEngine.connectNodes (mixBusNode->nodeID, 0,
                              audioEngine.getAudioOutputNode()->nodeID, 0);
    audioEngine.connectNodes (mixBusNode->nodeID, 1,
                              audioEngine.getAudioOutputNode()->nodeID, 1);

    // Set up GUI components
    transportBar.onOpenFile = [this] { openFile(); };
    addAndMakeVisible (transportBar);

    arrangementView = std::make_unique<ArrangementView> (project, transportController);
    addAndMakeVisible (*arrangementView);

    mixerPanel = std::make_unique<MixerPanel> (project,
        *dynamic_cast<MixBusProcessor*> (mixBusNode->getProcessor()));
    addAndMakeVisible (*mixerPanel);

    addAndMakeVisible (layoutResizer);

    audioSettingsButton.onClick = [this] { showAudioSettings(); };
    addAndMakeVisible (audioSettingsButton);

    addTrackButton.onClick = [this] { openFile(); };
    addAndMakeVisible (addTrackButton);

    // Listen to track changes for audio graph sync
    project.getState().getChildWithName (IDs::TRACKS).addListener (this);

    // Layout: arrangement on top, resizer, mixer on bottom
    layout.setItemLayout (0, 100, -1.0, -0.65);  // arrangement: 65%
    layout.setItemLayout (1, 4, 4, 4);            // resizer bar
    layout.setItemLayout (2, 100, -1.0, -0.35);   // mixer: 35%

    setSize (1400, 900);
}

MainComponent::~MainComponent()
{
    project.getState().getChildWithName (IDs::TRACKS).removeListener (this);
    setLookAndFeel (nullptr);
    audioEngine.shutdown();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // Top bar: transport + buttons
    auto topBar = area.removeFromTop (40);
    audioSettingsButton.setBounds (topBar.removeFromRight (120).reduced (4));
    addTrackButton.setBounds (topBar.removeFromRight (100).reduced (4));
    transportBar.setBounds (topBar);

    // Resizable layout for arrangement and mixer
    juce::Component* comps[] = { arrangementView.get(), &layoutResizer, mixerPanel.get() };
    layout.layOutComponents (comps, 3, area.getX(), area.getY(),
                             area.getWidth(), area.getHeight(), true, true);
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (
        audioEngine.getDeviceManager(), 0, 2, 0, 2, true, false, true, false);
    selector->setSize (500, 400);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector);
    options.dialogTitle = "Audio Settings";
    options.dialogBackgroundColour = juce::Colour (0xff1e1e2e);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void MainComponent::openFile()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Select an audio file...", juce::File{},
        "*.wav;*.aiff;*.mp3;*.flac;*.ogg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
                addTrackFromFile (file);
        });
}

void MainComponent::addTrackFromFile (const juce::File& file)
{
    auto trackName = file.getFileNameWithoutExtension();
    auto trackState = project.addTrack (trackName);

    // Create track processor to get file length
    auto tempProcessor = std::make_unique<TrackProcessor> (transportController);
    if (tempProcessor->loadFile (file))
    {
        auto length = tempProcessor->getFileLengthInSamples();
        Track track (trackState);
        track.addAudioClip (file, 0, length);
    }

    rebuildAudioGraph();
}

void MainComponent::rebuildAudioGraph()
{
    // Remove existing track nodes
    for (auto& node : trackNodes)
        if (node != nullptr)
            audioEngine.removeProcessor (node->nodeID);

    trackProcessors.clear();
    trackNodes.clear();

    // Create a processor for each track
    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        auto trackState = project.getTrack (i);
        Track track (trackState);

        auto* processor = new TrackProcessor (transportController);
        trackProcessors.add (processor);

        // Load the first clip's audio file
        if (track.getNumClips() > 0)
        {
            AudioClip clip (track.getClip (0));
            processor->loadFile (clip.getSourceFile());
        }

        // Sync gain/pan/mute from model
        processor->setGain (track.getVolume());
        processor->setPan (track.getPan());
        processor->setMuted (track.isMuted());

        // Add to graph
        auto node = audioEngine.addProcessor (
            std::unique_ptr<juce::AudioProcessor> (processor));
        trackNodes.add (node);

        // Connect to mix bus (stereo)
        if (mixBusNode != nullptr)
        {
            audioEngine.connectNodes (node->nodeID, 0, mixBusNode->nodeID, 0);
            audioEngine.connectNodes (node->nodeID, 1, mixBusNode->nodeID, 1);
        }
    }

    // Wire mixer panel meters
    if (mixerPanel != nullptr)
    {
        mixerPanel->rebuildStrips();
    }
}

void MainComponent::syncTrackProcessorsFromModel()
{
    for (int i = 0; i < project.getNumTracks() && i < trackProcessors.size(); ++i)
    {
        auto trackState = project.getTrack (i);
        Track track (trackState);
        auto* processor = trackProcessors[i];

        processor->setGain (track.getVolume());
        processor->setPan (track.getPan());
        processor->setMuted (track.isMuted());
    }
}

void MainComponent::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree.hasType (IDs::TRACK))
    {
        if (property == IDs::volume || property == IDs::pan || property == IDs::mute)
            syncTrackProcessorsFromModel();
    }
}

void MainComponent::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildAudioGraph();
}

void MainComponent::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildAudioGraph();
}

} // namespace dc
