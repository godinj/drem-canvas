#include "MainComponent.h"
#include "model/Track.h"
#include "model/AudioClip.h"
#include "model/StepSequencer.h"
#include "gui/mixer/ChannelStrip.h"

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
    mixBusNode = audioEngine.addProcessor (std::make_unique<MixBusProcessor> (transportController));

    // Connect mix bus output to audio output
    audioEngine.connectNodes (mixBusNode->nodeID, 0,
                              audioEngine.getAudioOutputNode()->nodeID, 0);
    audioEngine.connectNodes (mixBusNode->nodeID, 1,
                              audioEngine.getAudioOutputNode()->nodeID, 1);

    // Create step sequencer processor and add to graph
    {
        auto proc = std::make_unique<StepSequencerProcessor> (transportController);
        sequencerProcessor = proc.get();
        sequencerNode = audioEngine.addProcessor (std::move (proc));

        // Connect sequencer to mix bus (stereo — MIDI flows internally)
        audioEngine.connectNodes (sequencerNode->nodeID, 0, mixBusNode->nodeID, 0);
        audioEngine.connectNodes (sequencerNode->nodeID, 1, mixBusNode->nodeID, 1);

        sequencerProcessor->setTempo (project.getTempo());
        syncSequencerFromModel();
    }

    // Set up GUI components
    addAndMakeVisible (transportBar);

    arrangementView = std::make_unique<ArrangementView> (project, transportController, arrangement, vimContext);
    addAndMakeVisible (*arrangementView);

    mixerPanel = std::make_unique<MixerPanel> (project,
        *dynamic_cast<MixBusProcessor*> (mixBusNode->getProcessor()),
        &project.getUndoSystem());
    addAndMakeVisible (*mixerPanel);

    sequencerView = std::make_unique<StepSequencerView> (project, sequencerProcessor);
    addChildComponent (*sequencerView); // Hidden initially; shown when panel == Sequencer

    addAndMakeVisible (layoutResizer);

    saveSessionButton.onClick = [this] { saveSession(); };
    addAndMakeVisible (saveSessionButton);

    loadSessionButton.onClick = [this] { loadSession(); };
    addAndMakeVisible (loadSessionButton);

    audioSettingsButton.onClick = [this] { showAudioSettings(); };
    addAndMakeVisible (audioSettingsButton);

    addTrackButton.onClick = [this] { openFile(); };
    addAndMakeVisible (addTrackButton);

    // Vim modal engine
    vimEngine = std::make_unique<VimEngine> (project, transportController, arrangement, vimContext);
    addKeyListener (vimEngine.get());

    vimEngine->addListener (arrangementView.get());
    vimEngine->addListener (this);

    vimStatusBar = std::make_unique<VimStatusBar> (*vimEngine, vimContext, arrangement, transportController);
    addAndMakeVisible (*vimStatusBar);

    // Sync tempo map from project
    tempoMap.setTempo (project.getTempo());

    // Listen to track changes for audio graph sync
    project.getState().getChildWithName (IDs::TRACKS).addListener (this);

    // Listen to step sequencer changes for engine sync
    project.getState().getChildWithName (IDs::STEP_SEQUENCER).addListener (this);

    // Select first track if available
    if (arrangement.getNumTracks() > 0)
        arrangement.selectTrack (0);

    // Layout: arrangement on top, resizer, mixer on bottom
    layout.setItemLayout (0, 100, -1.0, -0.65);  // arrangement: 65%
    layout.setItemLayout (1, 4, 4, 4);            // resizer bar
    layout.setItemLayout (2, 100, -1.0, -0.35);   // mixer: 35%

    setWantsKeyboardFocus (true);
    setSize (1400, 900);
}

MainComponent::~MainComponent()
{
    vimEngine->removeListener (this);
    vimEngine->removeListener (arrangementView.get());
    removeKeyListener (vimEngine.get());
    project.getState().getChildWithName (IDs::STEP_SEQUENCER).removeListener (this);
    project.getState().getChildWithName (IDs::TRACKS).removeListener (this);
    setLookAndFeel (nullptr);
    trackProcessors.clear();
    trackNodes.clear();
    sequencerProcessor = nullptr;
    sequencerNode = nullptr;
    mixBusNode = nullptr;
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
    addTrackButton.setBounds (topBar.removeFromRight (120).reduced (4));
    loadSessionButton.setBounds (topBar.removeFromRight (120).reduced (4));
    saveSessionButton.setBounds (topBar.removeFromRight (120).reduced (4));
    transportBar.setBounds (topBar);

    // Status bar at bottom
    if (vimStatusBar != nullptr)
        vimStatusBar->setBounds (area.removeFromBottom (VimStatusBar::preferredHeight));

    // Determine which bottom panel to show
    juce::Component* bottomPanel = nullptr;
    if (vimContext.getPanel() == VimContext::Sequencer && sequencerView != nullptr)
        bottomPanel = sequencerView.get();
    else
        bottomPanel = mixerPanel.get();

    // Resizable layout for arrangement and bottom panel
    juce::Component* comps[] = { arrangementView.get(), &layoutResizer, bottomPanel };
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
    // Suspend audio processing while modifying the graph to avoid
    // the audio thread dereferencing nodes we're about to remove.
    audioEngine.getGraph().suspendProcessing (true);

    // Remove existing track nodes
    for (auto& node : trackNodes)
        if (node != nullptr)
            audioEngine.removeProcessor (node->nodeID);

    trackProcessors.clear();   // non-owning — graph deleted the processors above
    trackNodes.clear();

    // Create a processor for each track
    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        auto trackState = project.getTrack (i);
        Track track (trackState);

        auto processor = std::make_unique<TrackProcessor> (transportController);
        auto* processorPtr = processor.get();

        // Load the first clip's audio file
        if (track.getNumClips() > 0)
        {
            AudioClip clip (track.getClip (0));
            processorPtr->loadFile (clip.getSourceFile());
        }

        // Sync gain/pan/mute from model
        processorPtr->setGain (track.getVolume());
        processorPtr->setPan (track.getPan());
        processorPtr->setMuted (track.isMuted());

        // Add to graph — graph takes sole ownership
        auto node = audioEngine.addProcessor (std::move (processor));
        trackProcessors.add (processorPtr);
        trackNodes.add (node);

        // Connect to mix bus (stereo)
        if (mixBusNode != nullptr)
        {
            audioEngine.connectNodes (node->nodeID, 0, mixBusNode->nodeID, 0);
            audioEngine.connectNodes (node->nodeID, 1, mixBusNode->nodeID, 1);
        }
    }

    audioEngine.getGraph().suspendProcessing (false);

    // Rebuild UI views
    if (arrangementView != nullptr)
        arrangementView->rebuildTrackLanes();

    if (mixerPanel != nullptr)
    {
        mixerPanel->onWireMeter = [this] (int trackIndex, ChannelStrip& strip)
        {
            if (trackIndex < trackProcessors.size())
            {
                auto* proc = trackProcessors[trackIndex];
                strip.getMeter().getLeftLevel  = [proc] { return proc->getPeakLevelLeft(); };
                strip.getMeter().getRightLevel = [proc] { return proc->getPeakLevelRight(); };

                // Wire fader/pan/mute changes to push directly to the track processor
                strip.onStateChanged = [this] { syncTrackProcessorsFromModel(); };
            }
        };
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

void MainComponent::syncSequencerFromModel()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid() || sequencerProcessor == nullptr)
        return;

    StepSequencer seq (seqState);
    auto pattern = seq.getActivePattern();
    if (! pattern.isValid())
        return;

    StepSequencerProcessor::PatternSnapshot snapshot;
    snapshot.numRows      = seq.getNumRows();
    snapshot.numSteps     = static_cast<int> (pattern.getProperty (IDs::numSteps, 16));
    snapshot.stepDivision = static_cast<int> (pattern.getProperty (IDs::stepDivision, 4));
    snapshot.swing        = seq.getSwing();

    // Check for any soloed row
    snapshot.hasSoloedRow = false;
    for (int r = 0; r < snapshot.numRows; ++r)
    {
        auto row = seq.getRow (r);
        if (StepSequencer::isRowSoloed (row))
        {
            snapshot.hasSoloedRow = true;
            break;
        }
    }

    for (int r = 0; r < snapshot.numRows && r < StepSequencerProcessor::maxRows; ++r)
    {
        auto rowState = seq.getRow (r);
        auto& rowData = snapshot.rows[r];

        rowData.noteNumber = StepSequencer::getRowNoteNumber (rowState);
        rowData.mute       = StepSequencer::isRowMuted (rowState);
        rowData.solo       = StepSequencer::isRowSoloed (rowState);

        int stepCount = StepSequencer::getStepCount (rowState);
        for (int s = 0; s < stepCount && s < StepSequencerProcessor::maxSteps; ++s)
        {
            auto stepState = StepSequencer::getStep (rowState, s);
            auto& stepData = rowData.steps[s];

            stepData.active      = StepSequencer::isStepActive (stepState);
            stepData.velocity    = StepSequencer::getStepVelocity (stepState);
            stepData.probability = StepSequencer::getStepProbability (stepState);
            stepData.noteLength  = StepSequencer::getStepNoteLength (stepState);
        }
    }

    sequencerProcessor->updatePatternSnapshot (snapshot);
}

void MainComponent::saveSession()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Save Session Directory...",
        currentSessionDirectory.exists() ? currentSessionDirectory : juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                        | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir == juce::File())
                return;

            if (project.saveSessionToDirectory (dir))
            {
                currentSessionDirectory = dir;
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                    "Save Error", "Failed to save session to:\n" + dir.getFullPathName());
            }
        });
}

void MainComponent::loadSession()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load Session Directory...",
        currentSessionDirectory.exists() ? currentSessionDirectory : juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir == juce::File() || ! dir.isDirectory())
                return;

            // Remove listeners from old nodes before replacing state
            project.getState().getChildWithName (IDs::STEP_SEQUENCER).removeListener (this);
            project.getState().getChildWithName (IDs::TRACKS).removeListener (this);

            if (project.loadSessionFromDirectory (dir))
            {
                currentSessionDirectory = dir;

                // Re-add listeners on the new nodes
                project.getState().getChildWithName (IDs::TRACKS).addListener (this);
                project.getState().getChildWithName (IDs::STEP_SEQUENCER).addListener (this);
                rebuildAudioGraph();
                syncSequencerFromModel();
            }
            else
            {
                // Restore listeners on old (unchanged) nodes
                project.getState().getChildWithName (IDs::TRACKS).addListener (this);
                project.getState().getChildWithName (IDs::STEP_SEQUENCER).addListener (this);

                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                    "Load Error", "Failed to load session from:\n" + dir.getFullPathName());
            }
        });
}

void MainComponent::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree.hasType (IDs::TRACK))
    {
        if (property == IDs::volume || property == IDs::pan || property == IDs::mute)
            syncTrackProcessorsFromModel();
    }

    // Tempo change — sync to sequencer processor
    if (tree.hasType (IDs::PROJECT) && property == IDs::tempo)
    {
        if (sequencerProcessor != nullptr)
            sequencerProcessor->setTempo (project.getTempo());
    }

    // Any step sequencer property change
    if (tree.hasType (IDs::STEP_SEQUENCER) || tree.hasType (IDs::STEP_PATTERN)
        || tree.hasType (IDs::STEP_ROW) || tree.hasType (IDs::STEP))
    {
        syncSequencerFromModel();
    }
}

void MainComponent::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildAudioGraph();

    if (parent.hasType (IDs::STEP_SEQUENCER) || parent.hasType (IDs::STEP_PATTERN)
        || parent.hasType (IDs::STEP_ROW))
        syncSequencerFromModel();
}

void MainComponent::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildAudioGraph();

    if (parent.hasType (IDs::STEP_SEQUENCER) || parent.hasType (IDs::STEP_PATTERN)
        || parent.hasType (IDs::STEP_ROW))
        syncSequencerFromModel();
}

// ── VimEngine::Listener ─────────────────────────────────────────────────────

void MainComponent::vimModeChanged (VimEngine::Mode)
{
    // No panel work needed on mode change
}

void MainComponent::vimContextChanged()
{
    updatePanelVisibility();

    // Sync grid cursor to VimContext
    if (sequencerView != nullptr)
        sequencerView->getGrid().setCursorPosition (vimContext.getSeqRow(), vimContext.getSeqStep());
}

void MainComponent::updatePanelVisibility()
{
    bool showSequencer = (vimContext.getPanel() == VimContext::Sequencer);

    if (mixerPanel != nullptr)
        mixerPanel->setVisible (! showSequencer);

    if (sequencerView != nullptr)
        sequencerView->setVisible (showSequencer);

    resized();
}

} // namespace dc
