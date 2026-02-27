#include "MainComponent.h"
#include "model/Track.h"
#include "model/AudioClip.h"
#include "model/MidiClip.h"
#include "model/StepSequencer.h"
#include "gui/mixer/ChannelStrip.h"
#include "gui/browser/BrowserPanel.h"

namespace dc
{

MainComponent::MainComponent()
    : transportBar (transportController, project, tempoMap)
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

    // Create metronome processor and add to graph
    {
        auto proc = std::make_unique<MetronomeProcessor> (transportController);
        metronomeProcessor = proc.get();
        metronomeNode = audioEngine.addProcessor (std::move (proc));

        // Connect metronome directly to audio output (monitoring signal, not through mix bus)
        audioEngine.connectNodes (metronomeNode->nodeID, 0,
                                  audioEngine.getAudioOutputNode()->nodeID, 0);
        audioEngine.connectNodes (metronomeNode->nodeID, 1,
                                  audioEngine.getAudioOutputNode()->nodeID, 1);

        metronomeProcessor->setTempo (project.getTempo());
        metronomeProcessor->setBeatsPerBar (project.getTimeSigNumerator());
    }

    // Set up GUI components
    addAndMakeVisible (transportBar);
    transportBar.onMetronomeToggled = [this] (bool enabled)
    {
        if (metronomeProcessor != nullptr)
            metronomeProcessor->setEnabled (enabled);
    };

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

    // Browser panel (hidden by default)
    pluginManager.loadPluginList (pluginManager.getDefaultPluginListFile());
    auto* browser = new BrowserPanel (pluginManager);
    browser->onPluginSelected = [this] (const juce::PluginDescription& desc)
    {
        insertPluginOnTrack (arrangement.getSelectedTrackIndex(), desc);
    };
    browserPanel.reset (browser);
    browserPanel->setVisible (false);
    addChildComponent (*browserPanel);

    browserToggleButton.onClick = [this] { toggleBrowser(); };
    addAndMakeVisible (browserToggleButton);

    // Vim modal engine
    vimEngine = std::make_unique<VimEngine> (project, transportController, arrangement, vimContext);
    addKeyListener (vimEngine.get());

    vimEngine->addListener (arrangementView.get());
    vimEngine->addListener (this);

    // Wire :plugin command
    vimEngine->onPluginCommand = [this] (const juce::String& pluginName)
    {
        auto& knownPlugins = pluginManager.getKnownPlugins();
        auto types = knownPlugins.getTypes();

        // Fuzzy search: find first plugin whose name contains the search term (case-insensitive)
        for (const auto& desc : types)
        {
            if (desc.name.containsIgnoreCase (pluginName))
            {
                insertPluginOnTrack (arrangement.getSelectedTrackIndex(), desc);
                return;
            }
        }
    };

    // Wire plugin menu callbacks
    vimEngine->onPluginMenuMove = [this] (int delta)
    {
        if (auto* bp = dynamic_cast<BrowserPanel*> (browserPanel.get()))
            bp->moveSelection (delta);
    };

    vimEngine->onPluginMenuScroll = [this] (int direction)
    {
        if (auto* bp = dynamic_cast<BrowserPanel*> (browserPanel.get()))
            bp->scrollByHalfPage (direction);
    };

    vimEngine->onPluginMenuConfirm = [this]
    {
        if (auto* bp = dynamic_cast<BrowserPanel*> (browserPanel.get()))
            bp->confirmSelection();

        // Close browser after confirming
        browserVisible = false;
        if (browserPanel != nullptr)
            browserPanel->setVisible (false);
        resized();
    };

    vimEngine->onPluginMenuCancel = [this]
    {
        browserVisible = false;
        if (browserPanel != nullptr)
            browserPanel->setVisible (false);
        resized();
    };

    vimStatusBar = std::make_unique<VimStatusBar> (*vimEngine, vimContext, arrangement, transportController);
    addAndMakeVisible (*vimStatusBar);

    // Sync tempo map from project
    tempoMap.setTempo (project.getTempo());

    // Listen to all project state changes (tracks, tempo, time sig, step sequencer)
    project.getState().addListener (this);

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
    project.getState().removeListener (this);
    setLookAndFeel (nullptr);
    pluginWindowManager.closeAll();
    trackPluginChains.clear();
    meterTapProcessors.clear();
    meterTapNodes.clear();
    trackProcessors.clear();
    midiClipProcessors.clear();
    trackNodes.clear();
    metronomeProcessor = nullptr;
    metronomeNode = nullptr;
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
    browserToggleButton.setBounds (topBar.removeFromRight (100).reduced (4));
    transportBar.setBounds (topBar);

    // Status bar at bottom
    if (vimStatusBar != nullptr)
        vimStatusBar->setBounds (area.removeFromBottom (VimStatusBar::preferredHeight));

    // Browser panel on the right (when visible)
    if (browserVisible && browserPanel != nullptr)
        browserPanel->setBounds (area.removeFromRight (200));

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

    // Close plugin editor windows before removing nodes
    pluginWindowManager.closeAll();

    // Remove existing plugin chain nodes
    for (auto& chain : trackPluginChains)
        for (auto& info : chain)
            if (info.node != nullptr)
                audioEngine.removeProcessor (info.node->nodeID);
    trackPluginChains.clear();

    // Remove existing meter tap nodes
    for (auto& node : meterTapNodes)
        if (node != nullptr)
            audioEngine.removeProcessor (node->nodeID);
    meterTapProcessors.clear();
    meterTapNodes.clear();

    // Remove existing track nodes
    for (auto& node : trackNodes)
        if (node != nullptr)
            audioEngine.removeProcessor (node->nodeID);

    trackProcessors.clear();   // non-owning — graph deleted the processors above
    midiClipProcessors.clear();
    trackNodes.clear();

    auto sampleRate = audioEngine.getDeviceManager().getCurrentAudioDevice()
                          ? audioEngine.getDeviceManager().getCurrentAudioDevice()->getCurrentSampleRate()
                          : 44100.0;
    auto blockSize = audioEngine.getDeviceManager().getCurrentAudioDevice()
                         ? audioEngine.getDeviceManager().getCurrentAudioDevice()->getCurrentBufferSizeSamples()
                         : 512;

    // Create a processor for each track
    for (int i = 0; i < project.getNumTracks(); ++i)
    {
        auto trackState = project.getTrack (i);
        Track track (trackState);

        // Detect MIDI tracks: any child is a MIDI_CLIP
        bool isMidiTrack = false;
        for (int c = 0; c < track.getNumClips(); ++c)
        {
            if (track.getClip (c).hasType (IDs::MIDI_CLIP))
            {
                isMidiTrack = true;
                break;
            }
        }

        if (isMidiTrack)
        {
            auto processor = std::make_unique<MidiClipProcessor> (transportController);
            auto* processorPtr = processor.get();

            processorPtr->setGain (track.getVolume());
            processorPtr->setPan (track.getPan());
            processorPtr->setMuted (track.isMuted());
            processorPtr->setTempo (project.getTempo());

            auto node = audioEngine.addProcessor (std::move (processor));
            trackProcessors.add (nullptr);           // no TrackProcessor for MIDI tracks
            midiClipProcessors.add (processorPtr);
            trackNodes.add (node);
        }
        else
        {
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

            auto node = audioEngine.addProcessor (std::move (processor));
            trackProcessors.add (processorPtr);
            midiClipProcessors.add (nullptr);        // no MidiClipProcessor for audio tracks
            trackNodes.add (node);
        }

        // Instantiate plugin chain from model
        juce::Array<PluginNodeInfo> pluginChain;

        for (int p = 0; p < track.getNumPlugins(); ++p)
        {
            auto pluginState = track.getPlugin (p);
            auto desc = PluginHost::descriptionFromValueTree (pluginState);

            juce::String error;
            auto instance = pluginManager.getFormatManager().createPluginInstance (
                desc, sampleRate, blockSize, error);

            if (instance != nullptr)
            {
                // Restore plugin state
                juce::String base64State = pluginState.getProperty (IDs::pluginState, juce::String());
                if (base64State.isNotEmpty())
                    PluginHost::restorePluginState (*instance, base64State);

                auto* pluginPtr = instance.get();
                auto pluginNode = audioEngine.addProcessor (std::move (instance));
                pluginChain.add ({ pluginNode, pluginPtr });
            }
        }

        trackPluginChains.add (pluginChain);

        // Create meter tap for this track (sits at end of chain, before MixBus)
        auto meterTap = std::make_unique<MeterTapProcessor>();
        auto* meterTapPtr = meterTap.get();
        auto meterTapNode = audioEngine.addProcessor (std::move (meterTap));
        meterTapProcessors.add (meterTapPtr);
        meterTapNodes.add (meterTapNode);

        // Wire chain: TrackNode → Plugin1 → Plugin2 → ... → MeterTap → MixBus
        connectTrackPluginChain (i);

        // Push initial MIDI clip data if this is a MIDI track
        if (midiClipProcessors[i] != nullptr)
            syncMidiClipFromModel (i);
    }

    audioEngine.getGraph().suspendProcessing (false);

    // Rebuild UI views
    if (arrangementView != nullptr)
        arrangementView->rebuildTrackLanes();

    if (mixerPanel != nullptr)
    {
        mixerPanel->onWireMeter = [this] (int trackIndex, ChannelStrip& strip)
        {
            if (trackIndex < meterTapProcessors.size())
            {
                auto* tap = meterTapProcessors[trackIndex];

                if (tap != nullptr)
                {
                    strip.getMeter().getLeftLevel  = [tap] { return tap->getPeakLevelLeft(); };
                    strip.getMeter().getRightLevel = [tap] { return tap->getPeakLevelRight(); };
                }

                // Wire fader/pan/mute changes to push directly to the track processor
                strip.onStateChanged = [this] { syncTrackProcessorsFromModel(); };

                // Wire plugin callbacks
                strip.onPluginClicked = [this, trackIndex] (int pluginIndex)
                {
                    openPluginEditor (trackIndex, pluginIndex);
                };

                strip.onPluginBypassToggled = [this, trackIndex] (int pluginIndex)
                {
                    auto trackState = project.getTrack (trackIndex);
                    Track t (trackState);
                    bool enabled = t.isPluginEnabled (pluginIndex);
                    t.setPluginEnabled (pluginIndex, ! enabled, &project.getUndoManager());

                    audioEngine.getGraph().suspendProcessing (true);
                    disconnectTrackPluginChain (trackIndex);
                    connectTrackPluginChain (trackIndex);
                    audioEngine.getGraph().suspendProcessing (false);
                };

                strip.onPluginRemoveRequested = [this, trackIndex] (int pluginIndex)
                {
                    auto trackState = project.getTrack (trackIndex);
                    Track t (trackState);

                    // Close editor if open
                    if (trackIndex < trackPluginChains.size()
                        && pluginIndex < trackPluginChains[trackIndex].size())
                    {
                        auto& info = trackPluginChains[trackIndex].getReference (pluginIndex);
                        pluginWindowManager.closeEditorForPlugin (info.plugin);

                        // Remove from graph
                        audioEngine.getGraph().suspendProcessing (true);
                        disconnectTrackPluginChain (trackIndex);
                        audioEngine.removeProcessor (info.node->nodeID);
                        trackPluginChains.getReference (trackIndex).remove (pluginIndex);
                        connectTrackPluginChain (trackIndex);
                        audioEngine.getGraph().suspendProcessing (false);
                    }

                    // Remove from model
                    t.removePlugin (pluginIndex, &project.getUndoManager());
                };
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

        if (auto* processor = trackProcessors[i])
        {
            processor->setGain (track.getVolume());
            processor->setPan (track.getPan());
            processor->setMuted (track.isMuted());
        }
        else if (i < midiClipProcessors.size())
        {
            if (auto* midiProc = midiClipProcessors[i])
            {
                midiProc->setGain (track.getVolume());
                midiProc->setPan (track.getPan());
                midiProc->setMuted (track.isMuted());
            }
        }
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

void MainComponent::syncMidiClipFromModel (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= midiClipProcessors.size())
        return;

    auto* midiProc = midiClipProcessors[trackIndex];
    if (midiProc == nullptr)
        return;

    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    double currentTempo = project.getTempo();
    double sr = project.getSampleRate();

    MidiClipProcessor::MidiTrackSnapshot snapshot;
    snapshot.numEvents = 0;

    for (int c = 0; c < track.getNumClips(); ++c)
    {
        auto clipState = track.getClip (c);
        if (! clipState.hasType (IDs::MIDI_CLIP))
            continue;

        MidiClip clip (clipState);
        int64_t clipStartSample = clip.getStartPosition();
        auto seq = clip.getMidiSequence();

        // Match note-on/off pairs and convert to absolute sample positions
        seq.updateMatchedPairs();

        for (int e = 0; e < seq.getNumEvents(); ++e)
        {
            const auto* event = seq.getEventPointer (e);
            const auto& msg = event->message;

            if (! msg.isNoteOn())
                continue;

            if (snapshot.numEvents >= MidiClipProcessor::MidiTrackSnapshot::maxEvents)
                break;

            // Timestamps are in beats
            double onBeat = msg.getTimeStamp();
            int64_t onSample = clipStartSample
                + static_cast<int64_t> (onBeat * 60.0 / currentTempo * sr);

            int64_t offSample = onSample;
            if (event->noteOffObject != nullptr)
            {
                double offBeat = event->noteOffObject->message.getTimeStamp();
                offSample = clipStartSample
                    + static_cast<int64_t> (offBeat * 60.0 / currentTempo * sr);
            }
            else
            {
                // Default note length: 1/4 beat
                offSample = onSample + static_cast<int64_t> (0.25 * 60.0 / currentTempo * sr);
            }

            auto& evt = snapshot.events[snapshot.numEvents++];
            evt.noteNumber = msg.getNoteNumber();
            evt.channel    = msg.getChannel();
            evt.velocity   = msg.getVelocity();
            evt.onSample   = onSample;
            evt.offSample  = offSample;
        }
    }

    // Sort by onSample for efficient scanning in processBlock
    std::sort (snapshot.events.begin(),
               snapshot.events.begin() + snapshot.numEvents,
               [] (const MidiClipProcessor::MidiNoteEvent& a,
                   const MidiClipProcessor::MidiNoteEvent& b)
               {
                   return a.onSample < b.onSample;
               });

    midiProc->updateSnapshot (snapshot);
}

void MainComponent::saveSession()
{
    // Capture live plugin state before saving
    captureAllPluginStates();

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

            // Save ref to old state so we can detach widget listeners after replacement
            auto oldState = project.getState();
            oldState.removeListener (this);
            oldState.getChildWithName (IDs::TRACKS).removeListener (arrangementView.get());
            oldState.getChildWithName (IDs::TRACKS).removeListener (mixerPanel.get());

            if (project.loadSessionFromDirectory (dir))
            {
                currentSessionDirectory = dir;

                // Re-add listeners on the new state tree
                project.getState().addListener (this);
                project.getState().getChildWithName (IDs::TRACKS).addListener (arrangementView.get());
                project.getState().getChildWithName (IDs::TRACKS).addListener (mixerPanel.get());
                rebuildAudioGraph();
                syncSequencerFromModel();
            }
            else
            {
                // Restore listeners on old (unchanged) state
                oldState.addListener (this);
                oldState.getChildWithName (IDs::TRACKS).addListener (arrangementView.get());
                oldState.getChildWithName (IDs::TRACKS).addListener (mixerPanel.get());

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

    // Tempo change — sync to sequencer, metronome, and MIDI clip processors
    if (tree.hasType (IDs::PROJECT) && property == IDs::tempo)
    {
        if (sequencerProcessor != nullptr)
            sequencerProcessor->setTempo (project.getTempo());

        if (metronomeProcessor != nullptr)
            metronomeProcessor->setTempo (project.getTempo());

        tempoMap.setTempo (project.getTempo());

        // Re-sync all MIDI tracks (beat→sample conversion depends on tempo)
        for (int i = 0; i < midiClipProcessors.size(); ++i)
        {
            if (midiClipProcessors[i] != nullptr)
            {
                midiClipProcessors[i]->setTempo (project.getTempo());
                syncMidiClipFromModel (i);
            }
        }
    }

    // Time signature change — sync to metronome and tempo map
    if (tree.hasType (IDs::PROJECT) && (property == IDs::timeSigNumerator || property == IDs::timeSigDenominator))
    {
        if (metronomeProcessor != nullptr)
            metronomeProcessor->setBeatsPerBar (project.getTimeSigNumerator());

        tempoMap.setTimeSig (project.getTimeSigNumerator(), project.getTimeSigDenominator());
    }

    // MIDI clip property changed (e.g. midiData, startPosition, length)
    if (tree.hasType (IDs::MIDI_CLIP))
    {
        auto trackState = tree.getParent();
        if (trackState.hasType (IDs::TRACK))
        {
            auto tracksNode = project.getState().getChildWithName (IDs::TRACKS);
            int trackIndex = tracksNode.indexOf (trackState);
            if (trackIndex >= 0)
                syncMidiClipFromModel (trackIndex);
        }
    }

    // Any step sequencer property change
    if (tree.hasType (IDs::STEP_SEQUENCER) || tree.hasType (IDs::STEP_PATTERN)
        || tree.hasType (IDs::STEP_ROW) || tree.hasType (IDs::STEP))
    {
        syncSequencerFromModel();
    }
}

void MainComponent::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildAudioGraph();

    // MIDI clip added to a track
    if (parent.hasType (IDs::TRACK) && child.hasType (IDs::MIDI_CLIP))
    {
        auto tracksNode = project.getState().getChildWithName (IDs::TRACKS);
        int trackIndex = tracksNode.indexOf (parent);
        if (trackIndex >= 0)
            syncMidiClipFromModel (trackIndex);
    }

    if (parent.hasType (IDs::STEP_SEQUENCER) || parent.hasType (IDs::STEP_PATTERN)
        || parent.hasType (IDs::STEP_ROW))
        syncSequencerFromModel();
}

void MainComponent::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildAudioGraph();

    // MIDI clip removed from a track
    if (parent.hasType (IDs::TRACK) && child.hasType (IDs::MIDI_CLIP))
    {
        auto tracksNode = project.getState().getChildWithName (IDs::TRACKS);
        int trackIndex = tracksNode.indexOf (parent);
        if (trackIndex >= 0)
            syncMidiClipFromModel (trackIndex);
    }

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

    // Propagate active context indicator
    auto panel = vimContext.getPanel();
    if (arrangementView != nullptr)
        arrangementView->setActiveContext (panel == VimContext::Editor);
    if (mixerPanel != nullptr)
    {
        mixerPanel->setActiveContext (panel == VimContext::Mixer);
        mixerPanel->setSelectedStripIndex (arrangement.getSelectedTrackIndex());
        mixerPanel->setMixerFocus (vimContext.getMixerFocus());
    }

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

// ── Plugin chain wiring ──────────────────────────────────────────────────────

void MainComponent::connectTrackPluginChain (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= trackNodes.size()
        || trackIndex >= trackPluginChains.size() || mixBusNode == nullptr)
        return;

    auto trackNode = trackNodes[trackIndex];
    auto& chain = trackPluginChains.getReference (trackIndex);
    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    // Build list of enabled plugin nodes
    juce::Array<juce::AudioProcessorGraph::Node::Ptr> enabledNodes;
    for (int p = 0; p < chain.size(); ++p)
    {
        if (track.isPluginEnabled (p))
            enabledNodes.add (chain[p].node);
    }

    // Wire: TrackNode → Plugin1 → Plugin2 → ... → MixBus (audio)
    auto prevNode = trackNode;

    for (auto& pluginNode : enabledNodes)
    {
        audioEngine.connectNodes (prevNode->nodeID, 0, pluginNode->nodeID, 0);
        audioEngine.connectNodes (prevNode->nodeID, 1, pluginNode->nodeID, 1);
        prevNode = pluginNode;
    }

    // Route through meter tap (if available) before mix bus
    if (trackIndex < meterTapNodes.size() && meterTapNodes[trackIndex] != nullptr)
    {
        auto tapNode = meterTapNodes[trackIndex];
        audioEngine.connectNodes (prevNode->nodeID, 0, tapNode->nodeID, 0);
        audioEngine.connectNodes (prevNode->nodeID, 1, tapNode->nodeID, 1);
        prevNode = tapNode;
    }

    // Final connection to mix bus
    audioEngine.connectNodes (prevNode->nodeID, 0, mixBusNode->nodeID, 0);
    audioEngine.connectNodes (prevNode->nodeID, 1, mixBusNode->nodeID, 1);

    // Wire MIDI through the plugin chain (for MIDI tracks with instrument plugins)
    auto prevNodeForMidi = trackNode;
    for (auto& pluginNode : enabledNodes)
    {
        audioEngine.connectNodes (prevNodeForMidi->nodeID,
            juce::AudioProcessorGraph::midiChannelIndex,
            pluginNode->nodeID,
            juce::AudioProcessorGraph::midiChannelIndex);
        prevNodeForMidi = pluginNode;
    }
}

void MainComponent::disconnectTrackPluginChain (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= trackNodes.size()
        || trackIndex >= trackPluginChains.size() || mixBusNode == nullptr)
        return;

    auto& graph = audioEngine.getGraph();
    auto trackNode = trackNodes[trackIndex];
    auto& chain = trackPluginChains.getReference (trackIndex);

    // Remove all connections from the track node output
    for (auto& conn : graph.getConnections())
    {
        if (conn.source.nodeID == trackNode->nodeID)
            graph.removeConnection (conn);
    }

    // Remove all connections from plugin nodes in this chain
    for (auto& info : chain)
    {
        if (info.node == nullptr) continue;
        for (auto& conn : graph.getConnections())
        {
            if (conn.source.nodeID == info.node->nodeID)
                graph.removeConnection (conn);
        }
    }

    // Remove all connections from the meter tap node
    if (trackIndex < meterTapNodes.size() && meterTapNodes[trackIndex] != nullptr)
    {
        for (auto& conn : graph.getConnections())
        {
            if (conn.source.nodeID == meterTapNodes[trackIndex]->nodeID)
                graph.removeConnection (conn);
        }
    }
}

void MainComponent::openPluginEditor (int trackIndex, int pluginIndex)
{
    if (trackIndex < 0 || trackIndex >= trackPluginChains.size())
        return;

    auto& chain = trackPluginChains.getReference (trackIndex);
    if (pluginIndex < 0 || pluginIndex >= chain.size())
        return;

    auto* plugin = chain[pluginIndex].plugin;
    if (plugin != nullptr)
        pluginWindowManager.showEditorForPlugin (*plugin);
}

void MainComponent::captureAllPluginStates()
{
    for (int i = 0; i < project.getNumTracks() && i < trackPluginChains.size(); ++i)
    {
        auto trackState = project.getTrack (i);
        Track track (trackState);
        auto& chain = trackPluginChains.getReference (i);

        for (int p = 0; p < chain.size() && p < track.getNumPlugins(); ++p)
        {
            if (chain[p].plugin != nullptr)
            {
                auto base64State = PluginHost::savePluginState (*chain[p].plugin);
                track.setPluginState (p, base64State);
            }
        }
    }
}

void MainComponent::insertPluginOnTrack (int trackIndex, const juce::PluginDescription& desc)
{
    if (trackIndex < 0 || trackIndex >= project.getNumTracks())
        return;

    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    // Add to model
    track.addPlugin (desc.name, desc.pluginFormatName, desc.manufacturerName,
                     desc.uniqueId, desc.fileOrIdentifier, &project.getUndoManager());

    // Async instantiate and add to graph
    auto sampleRate = audioEngine.getDeviceManager().getCurrentAudioDevice()
                          ? audioEngine.getDeviceManager().getCurrentAudioDevice()->getCurrentSampleRate()
                          : 44100.0;
    auto blockSize = audioEngine.getDeviceManager().getCurrentAudioDevice()
                         ? audioEngine.getDeviceManager().getCurrentAudioDevice()->getCurrentBufferSizeSamples()
                         : 512;

    pluginHost.createPluginAsync (desc, sampleRate, blockSize,
        [this, trackIndex] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& errorMessage)
        {
            juce::ignoreUnused (errorMessage);
            if (instance == nullptr)
                return;

            auto* pluginPtr = instance.get();

            audioEngine.getGraph().suspendProcessing (true);
            disconnectTrackPluginChain (trackIndex);

            auto pluginNode = audioEngine.addProcessor (std::move (instance));

            if (trackIndex < trackPluginChains.size())
                trackPluginChains.getReference (trackIndex).add ({ pluginNode, pluginPtr });

            connectTrackPluginChain (trackIndex);
            audioEngine.getGraph().suspendProcessing (false);
        });
}

void MainComponent::toggleBrowser()
{
    browserVisible = ! browserVisible;
    if (browserPanel != nullptr)
        browserPanel->setVisible (browserVisible);
    resized();

    if (browserVisible)
    {
        // Enter plugin menu mode and select first item
        vimEngine->enterPluginMenuMode();
        if (auto* bp = dynamic_cast<BrowserPanel*> (browserPanel.get()))
        {
            if (bp->getSelectedPluginIndex() < 0 && bp->getNumPlugins() > 0)
                bp->selectPlugin (0);
        }
        // Keep focus on MainComponent so VimEngine receives key events
        grabKeyboardFocus();
    }
    else
    {
        // Return to normal mode if we were in plugin menu
        if (vimEngine->getMode() == VimEngine::PluginMenu)
            vimEngine->enterNormalMode();
    }
}

} // namespace dc
