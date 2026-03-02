#include "MainComponent.h"
#include "engine/PluginProcessorNode.h"
#include "dc/plugins/PluginDescription.h"
#include "model/Track.h"
#include "model/AudioClip.h"
#include "model/MidiClip.h"
#include "model/StepSequencer.h"
#include "gui/mixer/ChannelStrip.h"
#include "gui/browser/BrowserPanel.h"
#include "gui/common/ColourBridge.h"
#include "dc/foundation/types.h"
#include <filesystem>
#include <string>

using dc::bridge::toJuce;

namespace dc
{

MainComponent::MainComponent()
    : transportBar (transportController, project, tempoMap)
{
    setLookAndFeel (&lookAndFeel);

    // Initialise audio engine with stereo I/O
    audioEngine.initialise (2, 2);
    transportController.setSampleRate (audioEngine.getSampleRate());

    // Create mix bus processor and add to graph
    MixBusProcessor* mixBusProcessorPtr = nullptr;
    {
        auto proc = std::make_unique<MixBusProcessor> (transportController);
        mixBusProcessorPtr = proc.get();
        mixBusNode = audioEngine.addProcessor (std::move (proc));
    }

    // Create master meter tap (sits after master plugin chain, before audio output)
    {
        auto masterTap = std::make_unique<MeterTapProcessor>();
        masterMeterTapProcessor = masterTap.get();
        masterMeterTapNode = audioEngine.addProcessor (std::move (masterTap));
    }

    // Connect: MixBus -> [master plugins] -> masterMeterTap -> AudioOutput
    connectMasterPluginChain();

    // Create step sequencer processor and add to graph
    {
        auto proc = std::make_unique<StepSequencerProcessor> (transportController);
        sequencerProcessor = proc.get();
        sequencerNode = audioEngine.addProcessor (std::move (proc));

        // Connect sequencer to mix bus (stereo -- MIDI flows internally)
        audioEngine.connectNodes (sequencerNode, 0, mixBusNode, 0);
        audioEngine.connectNodes (sequencerNode, 1, mixBusNode, 1);

        sequencerProcessor->setTempo (project.getTempo());
        syncSequencerFromModel();
    }

    // Create metronome processor and add to graph
    {
        auto proc = std::make_unique<MetronomeProcessor> (transportController);
        metronomeProcessor = proc.get();
        metronomeNode = audioEngine.addProcessor (std::move (proc));

        // Connect metronome directly to audio output (monitoring signal, not through mix bus)
        audioEngine.connectNodes (metronomeNode, 0,
                                  audioEngine.getAudioOutputNodeId(), 0);
        audioEngine.connectNodes (metronomeNode, 1,
                                  audioEngine.getAudioOutputNodeId(), 1);

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
        *mixBusProcessorPtr,
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
    browser->onPluginSelected = [this] (const dc::PluginDescription& desc)
    {
        if (vimContext.isMasterStripSelected())
            insertPluginOnMaster (desc);
        else
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
    vimEngine->onPluginCommand = [this] (const std::string& pluginName)
    {
        auto& knownPlugins = pluginManager.getKnownPlugins();

        // Fuzzy search: find first plugin whose name contains the search term (case-insensitive)
        std::string queryLower = pluginName;
        std::transform (queryLower.begin(), queryLower.end(), queryLower.begin(),
                        [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });

        for (const auto& desc : knownPlugins)
        {
            std::string nameLower = desc.name;
            std::transform (nameLower.begin(), nameLower.end(), nameLower.begin(),
                            [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });

            if (nameLower.find (queryLower) != std::string::npos)
            {
                insertPluginOnTrack (arrangement.getSelectedTrackIndex(), desc);
                return;
            }
        }
    };

    // Wire browser toggle (gp keybinding)
    vimEngine->onToggleBrowser = [this] { toggleBrowser(); };

    // Wire mixer plugin navigation callbacks
    vimEngine->onMixerPluginOpen = [this] (int trackIdx, int pluginIndex)
    {
        if (trackIdx < 0)
            openMasterPluginEditor (pluginIndex);
        else
            openPluginEditor (trackIdx, pluginIndex);
    };

    vimEngine->onMixerPluginAdd = [this] (int trackIdx)
    {
        if (trackIdx >= 0)
            arrangement.selectTrack (trackIdx);
        // Store context for browser: -1 means master
        toggleBrowser();
    };

    vimEngine->onMixerPluginRemove = [this] (int trackIdx, int pluginIndex)
    {
        if (trackIdx < 0)
        {
            // Master strip
            if (pluginIndex < static_cast<int> (masterPluginChain.size()))
            {
                auto& info = masterPluginChain[static_cast<size_t> (pluginIndex)];
                pluginWindowManager.closeEditorForPlugin (info.plugin);

                // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
                disconnectMasterPluginChain();
                audioEngine.removeProcessor (info.node);
                masterPluginChain.erase (masterPluginChain.begin() + pluginIndex);

                // Remove from model
                auto masterBus = project.getMasterBusState();
                auto chain = masterBus.getChildWithType (IDs::PLUGIN_CHAIN);
                if (chain.isValid() && pluginIndex < chain.getNumChildren())
                    chain.removeChild (pluginIndex, &project.getUndoManager());

                connectMasterPluginChain();
            }
        }
        else
        {
            // Regular track
            auto trackState = project.getTrack (trackIdx);
            Track t (trackState);

            if (trackIdx < static_cast<int> (trackPluginChains.size())
                && pluginIndex < static_cast<int> (trackPluginChains[static_cast<size_t> (trackIdx)].size()))
            {
                auto& info = trackPluginChains[static_cast<size_t> (trackIdx)][static_cast<size_t> (pluginIndex)];
                pluginWindowManager.closeEditorForPlugin (info.plugin);

                // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
                disconnectTrackPluginChain (trackIdx);
                audioEngine.removeProcessor (info.node);
                auto& chainVec = trackPluginChains[static_cast<size_t> (trackIdx)];
                chainVec.erase (chainVec.begin() + pluginIndex);
                connectTrackPluginChain (trackIdx);
            }

            t.removePlugin (pluginIndex, &project.getUndoManager());
        }

        if (mixerPanel != nullptr)
            mixerPanel->rebuildStrips();
    };

    vimEngine->onMixerPluginBypass = [this] (int trackIdx, int pluginIndex)
    {
        if (trackIdx < 0)
        {
            // Master strip
            auto masterBus = project.getMasterBusState();
            auto chain = masterBus.getChildWithType (IDs::PLUGIN_CHAIN);
            if (chain.isValid() && pluginIndex < chain.getNumChildren())
            {
                auto plugin = chain.getChild (pluginIndex);
                bool enabled = plugin.getProperty (IDs::pluginEnabled, true);
                plugin.setProperty (IDs::pluginEnabled, ! enabled, &project.getUndoManager());

                // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
                disconnectMasterPluginChain();
                connectMasterPluginChain();
            }
        }
        else
        {
            auto trackState = project.getTrack (trackIdx);
            Track t (trackState);
            bool enabled = t.isPluginEnabled (pluginIndex);
            t.setPluginEnabled (pluginIndex, ! enabled, &project.getUndoManager());

            // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
            disconnectTrackPluginChain (trackIdx);
            connectTrackPluginChain (trackIdx);
        }
    };

    vimEngine->onMixerPluginReorder = [this] (int trackIdx, int fromIndex, int toIndex)
    {
        if (trackIdx < 0)
        {
            // Master strip
            auto masterBus = project.getMasterBusState();
            auto chain = masterBus.getChildWithType (IDs::PLUGIN_CHAIN);
            if (chain.isValid() && fromIndex < chain.getNumChildren() && toIndex < chain.getNumChildren())
            {
                // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
                disconnectMasterPluginChain();

                chain.moveChild (fromIndex, toIndex, &project.getUndoManager());
                std::swap (masterPluginChain[static_cast<size_t> (fromIndex)],
                           masterPluginChain[static_cast<size_t> (toIndex)]);

                connectMasterPluginChain();
            }
        }
        else
        {
            auto trackState = project.getTrack (trackIdx);
            Track t (trackState);

            if (trackIdx < static_cast<int> (trackPluginChains.size()))
            {
                // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
                disconnectTrackPluginChain (trackIdx);

                t.movePlugin (fromIndex, toIndex, &project.getUndoManager());
                auto& chainVec = trackPluginChains[static_cast<size_t> (trackIdx)];
                std::swap (chainVec[static_cast<size_t> (fromIndex)],
                           chainVec[static_cast<size_t> (toIndex)]);

                connectTrackPluginChain (trackIdx);
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

    vimEngine->onPluginMenuFilter = [this] (const std::string& query)
    {
        if (auto* bp = dynamic_cast<BrowserPanel*> (browserPanel.get()))
            bp->setSearchFilter (query);
    };

    vimEngine->onPluginMenuClearFilter = [this]
    {
        if (auto* bp = dynamic_cast<BrowserPanel*> (browserPanel.get()))
            bp->clearSearchFilter();
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
    g.fillAll (toJuce (0xff1e1e2e));
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
    auto deviceName = audioEngine.getCurrentDeviceName();
    auto msg = juce::String ("Audio Device: ") + juce::String (deviceName)
             + "\nSample Rate: " + juce::String (audioEngine.getSampleRate()) + " Hz"
             + "\nBuffer Size: " + juce::String (audioEngine.getBufferSize()) + " samples";

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                            "Audio Settings", msg);
}

void MainComponent::openFile()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Select an audio file...", juce::File{}, // JUCE API boundary
        "*.wav;*.aiff;*.mp3;*.flac;*.ogg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
                addTrackFromFile (std::filesystem::path (file.getFullPathName().toStdString()));
        });
}

void MainComponent::addTrackFromFile (const std::filesystem::path& file)
{
    auto trackName = file.stem().string();
    auto trackState = project.addTrack (trackName);

    // Create track processor to get file length
    auto tempProcessor = std::make_unique<TrackProcessor> (transportController);
    if (tempProcessor->loadFile (file))
    {
        auto length = tempProcessor->getFileLengthInSamples();
        Track track (trackState);
        track.addAudioClip (file.string(), 0, length);
    }

    rebuildAudioGraph();
}

void MainComponent::rebuildAudioGraph()
{
    // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations

    // Close plugin editor windows before removing nodes
    pluginWindowManager.closeAll();

    // Remove existing plugin chain nodes
    for (auto& chain : trackPluginChains)
        for (auto& info : chain)
            if (info.node != 0)
                audioEngine.removeProcessor (info.node);
    trackPluginChains.clear();

    // Remove existing meter tap nodes
    for (auto& nodeId : meterTapNodes)
        if (nodeId != 0)
            audioEngine.removeProcessor (nodeId);
    meterTapProcessors.clear();
    meterTapNodes.clear();

    // Remove existing track nodes
    for (auto& nodeId : trackNodes)
        if (nodeId != 0)
            audioEngine.removeProcessor (nodeId);

    trackProcessors.clear();   // non-owning -- graph deleted the processors above
    midiClipProcessors.clear();
    trackNodes.clear();

    auto sampleRate = audioEngine.getSampleRate();
    auto blockSize = audioEngine.getBufferSize();

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
            trackProcessors.push_back (nullptr);           // no TrackProcessor for MIDI tracks
            midiClipProcessors.push_back (processorPtr);
            trackNodes.push_back (node);
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
            trackProcessors.push_back (processorPtr);
            midiClipProcessors.push_back (nullptr);        // no MidiClipProcessor for audio tracks
            trackNodes.push_back (node);
        }

        // Instantiate plugin chain from model
        std::vector<PluginNodeInfo> pluginChain;

        for (int p = 0; p < track.getNumPlugins(); ++p)
        {
            auto pluginState = track.getPlugin (p);
            auto desc = PluginHost::descriptionFromPropertyTree (pluginState);

            auto instance = pluginHost.createPluginSync (desc, sampleRate, blockSize);

            if (instance != nullptr)
            {
                // Restore plugin state
                std::string base64State = pluginState.getProperty (IDs::pluginState).getStringOr ("");
                if (! base64State.empty())
                    PluginHost::restorePluginState (*instance, base64State);

                auto* pluginPtr = instance.get();
                auto wrapper = std::make_unique<PluginProcessorNode> (std::move (instance));
                auto pluginNode = audioEngine.addProcessor (std::move (wrapper));
                pluginChain.push_back ({ pluginNode, pluginPtr });
            }
        }

        trackPluginChains.push_back (pluginChain);

        // Create meter tap for this track (sits at end of chain, before MixBus)
        auto meterTap = std::make_unique<MeterTapProcessor>();
        auto* meterTapPtr = meterTap.get();
        auto meterTapNode = audioEngine.addProcessor (std::move (meterTap));
        meterTapProcessors.push_back (meterTapPtr);
        meterTapNodes.push_back (meterTapNode);

        // Wire chain: TrackNode → Plugin1 → Plugin2 → ... → MeterTap → MixBus
        connectTrackPluginChain (i);

        // Push initial MIDI clip data if this is a MIDI track
        if (midiClipProcessors[i] != nullptr)
            syncMidiClipFromModel (i);
    }

    // Rebuild UI views
    if (arrangementView != nullptr)
        arrangementView->rebuildTrackLanes();

    if (mixerPanel != nullptr)
    {
        mixerPanel->onWireMeter = [this] (int trackIndex, ChannelStrip& strip)
        {
            if (trackIndex < static_cast<int> (meterTapProcessors.size()))
            {
                auto* tap = meterTapProcessors[static_cast<size_t> (trackIndex)];

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

                    // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
                    disconnectTrackPluginChain (trackIndex);
                    connectTrackPluginChain (trackIndex);
                };

                strip.onPluginRemoveRequested = [this, trackIndex] (int pluginIndex)
                {
                    auto trackState = project.getTrack (trackIndex);
                    Track t (trackState);

                    // Close editor if open
                    if (trackIndex < static_cast<int> (trackPluginChains.size())
                        && pluginIndex < static_cast<int> (trackPluginChains[static_cast<size_t> (trackIndex)].size()))
                    {
                        auto& info = trackPluginChains[static_cast<size_t> (trackIndex)][static_cast<size_t> (pluginIndex)];
                        pluginWindowManager.closeEditorForPlugin (info.plugin);

                        // Remove from graph
                        // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
                        disconnectTrackPluginChain (trackIndex);
                        audioEngine.removeProcessor (info.node);
                        auto& chainVec = trackPluginChains[static_cast<size_t> (trackIndex)];
                        chainVec.erase (chainVec.begin() + pluginIndex);
                        connectTrackPluginChain (trackIndex);
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
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
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
        int64_t clipLength = clip.getLength();
        int64_t trimStart = static_cast<int64_t> (
            static_cast<juce::int64> (clipState.getProperty (IDs::trimStart, 0)));

        // Convert trimStart to beats for filtering
        double trimStartBeats = (sr > 0.0 && currentTempo > 0.0)
            ? (static_cast<double> (trimStart) / sr) * currentTempo / 60.0
            : 0.0;
        double clipLengthBeats = (sr > 0.0 && currentTempo > 0.0)
            ? (static_cast<double> (clipLength) / sr) * currentTempo / 60.0
            : 1e12;

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

            // Timestamps are in beats (relative to original clip start)
            double onBeat = msg.getTimeStamp();

            // Skip notes outside the trimmed region
            double offBeat = onBeat + 0.25;
            if (event->noteOffObject != nullptr)
                offBeat = event->noteOffObject->message.getTimeStamp();

            if (offBeat <= trimStartBeats || onBeat >= trimStartBeats + clipLengthBeats)
                continue;

            // Offset by trimStart: beat position relative to the trimmed clip start
            double adjustedOnBeat = onBeat - trimStartBeats;
            double adjustedOffBeat = offBeat - trimStartBeats;

            // Clamp to clip boundaries
            if (adjustedOnBeat < 0.0) adjustedOnBeat = 0.0;
            if (adjustedOffBeat > clipLengthBeats) adjustedOffBeat = clipLengthBeats;

            int64_t onSample = clipStartSample
                + static_cast<int64_t> (adjustedOnBeat * 60.0 / currentTempo * sr);

            int64_t offSample = clipStartSample
                + static_cast<int64_t> (adjustedOffBeat * 60.0 / currentTempo * sr);

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

void MainComponent::syncAudioClipFromModel (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= trackProcessors.size())
        return;

    auto* proc = trackProcessors[trackIndex];
    if (proc == nullptr)
        return;

    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    // Find the first audio clip on this track
    bool hasAudioClip = false;
    for (int c = 0; c < track.getNumClips(); ++c)
    {
        auto clipState = track.getClip (c);
        if (clipState.hasType (IDs::AUDIO_CLIP))
        {
            AudioClip clip (clipState);
            proc->loadFile (clip.getSourceFile());
            hasAudioClip = true;
            break;
        }
    }

    if (! hasAudioClip)
        proc->clearFile();
}

void MainComponent::saveSession()
{
    // Capture live plugin state before saving
    captureAllPluginStates();

    auto chooser = std::make_shared<juce::FileChooser> (
        "Save Session Directory...",
        std::filesystem::exists (currentSessionDirectory) ? juce::File (currentSessionDirectory.string()) : juce::File::getSpecialLocation (juce::File::userHomeDirectory), // JUCE API boundary
        "");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                        | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir == juce::File()) // JUCE API boundary
                return;

            if (project.saveSessionToDirectory (dir.getFullPathName().toStdString()))
            {
                currentSessionDirectory = std::filesystem::path (dir.getFullPathName().toStdString());
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
        std::filesystem::exists (currentSessionDirectory) ? juce::File (currentSessionDirectory.string()) : juce::File::getSpecialLocation (juce::File::userHomeDirectory), // JUCE API boundary
        "");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir == juce::File() || ! dir.isDirectory()) // JUCE API boundary
                return;

            // Save ref to old state so we can detach widget listeners after replacement
            auto oldState = project.getState();
            oldState.removeListener (this);
            oldState.getChildWithType (IDs::TRACKS).removeListener (arrangementView.get());
            oldState.getChildWithType (IDs::TRACKS).removeListener (mixerPanel.get());

            if (project.loadSessionFromDirectory (dir.getFullPathName().toStdString()))
            {
                currentSessionDirectory = std::filesystem::path (dir.getFullPathName().toStdString());

                // Re-add listeners on the new state tree
                project.getState().addListener (this);
                project.getState().getChildWithType (IDs::TRACKS).addListener (arrangementView.get());
                project.getState().getChildWithType (IDs::TRACKS).addListener (mixerPanel.get());
                rebuildAudioGraph();
                syncSequencerFromModel();
            }
            else
            {
                // Restore listeners on old (unchanged) state
                oldState.addListener (this);
                oldState.getChildWithType (IDs::TRACKS).addListener (arrangementView.get());
                oldState.getChildWithType (IDs::TRACKS).addListener (mixerPanel.get());

                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                    "Load Error", "Failed to load session from:\n" + dir.getFullPathName());
            }
        });
}

void MainComponent::propertyChanged (PropertyTree& tree, PropertyId property)
{
    if (tree.getType() == IDs::TRACK)
    {
        if (property == IDs::volume || property == IDs::pan || property == IDs::mute)
            syncTrackProcessorsFromModel();
    }

    // Tempo change — sync to sequencer, metronome, and MIDI clip processors
    if (tree.getType() == IDs::PROJECT && property == IDs::tempo)
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
    if (tree.getType() == IDs::PROJECT && (property == IDs::timeSigNumerator || property == IDs::timeSigDenominator))
    {
        if (metronomeProcessor != nullptr)
            metronomeProcessor->setBeatsPerBar (project.getTimeSigNumerator());

        tempoMap.setTimeSig (project.getTimeSigNumerator(), project.getTimeSigDenominator());
    }

    // MIDI clip property changed (e.g. midiData, startPosition, length, trimStart)
    if (tree.getType() == IDs::MIDI_CLIP)
    {
        auto trackState = tree.getParent();
        if (trackState.getType() == IDs::TRACK)
        {
            auto tracksNode = project.getState().getChildWithType (IDs::TRACKS);
            int trackIndex = tracksNode.indexOf (trackState);
            if (trackIndex >= 0)
                syncMidiClipFromModel (trackIndex);
        }
    }

    // Audio clip property changed (e.g. startPosition, length, trimStart)
    if (tree.getType() == IDs::AUDIO_CLIP)
    {
        auto trackState = tree.getParent();
        if (trackState.getType() == IDs::TRACK)
        {
            auto tracksNode = project.getState().getChildWithType (IDs::TRACKS);
            int trackIndex = tracksNode.indexOf (trackState);
            if (trackIndex >= 0)
                syncAudioClipFromModel (trackIndex);
        }
    }

    // Any step sequencer property change
    if (tree.getType() == IDs::STEP_SEQUENCER || tree.getType() == IDs::STEP_PATTERN
        || tree.getType() == IDs::STEP_ROW || tree.getType() == IDs::STEP)
    {
        syncSequencerFromModel();
    }
}

void MainComponent::childAdded (PropertyTree& parent, PropertyTree& child)
{
    if (parent.getType() == IDs::TRACKS)
        rebuildAudioGraph();

    // MIDI clip added to a track
    if (parent.getType() == IDs::TRACK && child.getType() == IDs::MIDI_CLIP)
    {
        auto tracksNode = project.getState().getChildWithType (IDs::TRACKS);
        int trackIndex = tracksNode.indexOf (parent);
        if (trackIndex >= 0)
            syncMidiClipFromModel (trackIndex);
    }

    // Audio clip added to a track
    if (parent.getType() == IDs::TRACK && child.getType() == IDs::AUDIO_CLIP)
    {
        auto tracksNode = project.getState().getChildWithType (IDs::TRACKS);
        int trackIndex = tracksNode.indexOf (parent);
        if (trackIndex >= 0)
            syncAudioClipFromModel (trackIndex);
    }

    if (parent.getType() == IDs::STEP_SEQUENCER || parent.getType() == IDs::STEP_PATTERN
        || parent.getType() == IDs::STEP_ROW)
        syncSequencerFromModel();
}

void MainComponent::childRemoved (PropertyTree& parent, PropertyTree& child, int)
{
    if (parent.getType() == IDs::TRACKS)
        rebuildAudioGraph();

    // MIDI clip removed from a track
    if (parent.getType() == IDs::TRACK && child.getType() == IDs::MIDI_CLIP)
    {
        auto tracksNode = project.getState().getChildWithType (IDs::TRACKS);
        int trackIndex = tracksNode.indexOf (parent);
        if (trackIndex >= 0)
            syncMidiClipFromModel (trackIndex);
    }

    // Audio clip removed from a track
    if (parent.getType() == IDs::TRACK && child.getType() == IDs::AUDIO_CLIP)
    {
        auto tracksNode = project.getState().getChildWithType (IDs::TRACKS);
        int trackIndex = tracksNode.indexOf (parent);
        if (trackIndex >= 0)
            syncAudioClipFromModel (trackIndex);
    }

    if (parent.getType() == IDs::STEP_SEQUENCER || parent.getType() == IDs::STEP_PATTERN
        || parent.getType() == IDs::STEP_ROW)
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

        // Select master strip or track strip
        if (vimContext.isMasterStripSelected())
            mixerPanel->setSelectedStripIndex (project.getNumTracks());
        else
            mixerPanel->setSelectedStripIndex (arrangement.getSelectedTrackIndex());

        mixerPanel->setMixerFocus (vimContext.getMixerFocus());

        // Propagate plugin slot selection
        if (vimContext.getMixerFocus() == VimContext::FocusPlugins)
            mixerPanel->setSelectedPluginSlot (vimContext.getSelectedPluginSlot());
        else
            mixerPanel->setSelectedPluginSlot (-1);
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
    if (trackIndex < 0 || trackIndex >= static_cast<int> (trackNodes.size())
        || trackIndex >= static_cast<int> (trackPluginChains.size()) || mixBusNode == 0)
        return;

    auto trackNodeId = trackNodes[static_cast<size_t> (trackIndex)];
    auto& chain = trackPluginChains[static_cast<size_t> (trackIndex)];
    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    // Build list of enabled plugin nodes
    std::vector<NodeId> enabledNodes;
    for (int p = 0; p < static_cast<int> (chain.size()); ++p)
    {
        if (track.isPluginEnabled (p))
            enabledNodes.push_back (chain[static_cast<size_t> (p)].node);
    }

    // Wire: TrackNode -> Plugin1 -> Plugin2 -> ... -> MixBus (audio)
    auto prevId = trackNodeId;

    for (auto pluginNodeId : enabledNodes)
    {
        audioEngine.connectNodes (prevId, 0, pluginNodeId, 0);
        audioEngine.connectNodes (prevId, 1, pluginNodeId, 1);
        prevId = pluginNodeId;
    }

    // Route through meter tap (if available) before mix bus
    if (trackIndex < static_cast<int> (meterTapNodes.size()) && meterTapNodes[static_cast<size_t> (trackIndex)] != 0)
    {
        auto tapId = meterTapNodes[static_cast<size_t> (trackIndex)];
        audioEngine.connectNodes (prevId, 0, tapId, 0);
        audioEngine.connectNodes (prevId, 1, tapId, 1);
        prevId = tapId;
    }

    // Final connection to mix bus
    audioEngine.connectNodes (prevId, 0, mixBusNode, 0);
    audioEngine.connectNodes (prevId, 1, mixBusNode, 1);

    // Wire MIDI through the plugin chain (for MIDI tracks with instrument plugins)
    auto prevMidiId = trackNodeId;
    for (auto pluginNodeId : enabledNodes)
    {
        audioEngine.connectNodes (prevMidiId, -1, pluginNodeId, -1);  // MIDI channel
        prevMidiId = pluginNodeId;
    }
}

void MainComponent::disconnectTrackPluginChain (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= static_cast<int> (trackNodes.size())
        || trackIndex >= static_cast<int> (trackPluginChains.size()) || mixBusNode == 0)
        return;

    auto& graph = audioEngine.getGraph();
    auto trackNodeId = trackNodes[static_cast<size_t> (trackIndex)];
    auto& chain = trackPluginChains[static_cast<size_t> (trackIndex)];

    // Disconnect all connections from the track node
    graph.disconnectNode (trackNodeId);

    // Disconnect all connections from plugin nodes in this chain
    for (auto& info : chain)
    {
        if (info.node != 0)
            graph.disconnectNode (info.node);
    }

    // Disconnect all connections from the meter tap node
    if (trackIndex < static_cast<int> (meterTapNodes.size()) && meterTapNodes[static_cast<size_t> (trackIndex)] != 0)
    {
        graph.disconnectNode (meterTapNodes[static_cast<size_t> (trackIndex)]);
    }
}

void MainComponent::openPluginEditor (int trackIndex, int pluginIndex)
{
    if (trackIndex < 0 || trackIndex >= static_cast<int> (trackPluginChains.size()))
        return;

    auto& chain = trackPluginChains[static_cast<size_t> (trackIndex)];
    if (pluginIndex < 0 || pluginIndex >= static_cast<int> (chain.size()))
        return;

    auto* plugin = chain[pluginIndex].plugin;
    if (plugin != nullptr)
        pluginWindowManager.showEditorForPlugin (*plugin);
}

void MainComponent::captureAllPluginStates()
{
    for (int i = 0; i < project.getNumTracks() && i < static_cast<int> (trackPluginChains.size()); ++i)
    {
        auto trackState = project.getTrack (i);
        Track track (trackState);
        auto& chain = trackPluginChains[static_cast<size_t> (i)];

        for (int p = 0; p < static_cast<int> (chain.size()) && p < track.getNumPlugins(); ++p)
        {
            if (chain[p].plugin != nullptr)
            {
                auto base64State = PluginHost::savePluginState (*chain[p].plugin);
                track.setPluginState (p, base64State);
            }
        }
    }

    // Capture master plugin states
    auto masterBus = project.getMasterBusState();
    auto masterChainTree = masterBus.getChildWithType (IDs::PLUGIN_CHAIN);
    for (int p = 0; p < static_cast<int> (masterPluginChain.size()) && p < masterChainTree.getNumChildren(); ++p)
    {
        if (masterPluginChain[static_cast<size_t> (p)].plugin != nullptr)
        {
            auto base64State = PluginHost::savePluginState (*masterPluginChain[static_cast<size_t> (p)].plugin);
            masterChainTree.getChild (p).setProperty (IDs::pluginState, base64State, nullptr);
        }
    }
}

// ── Master bus plugin chain ──────────────────────────────────────────────────

void MainComponent::connectMasterPluginChain()
{
    if (mixBusNode == 0 || masterMeterTapNode == 0)
        return;

    auto masterBus = project.getMasterBusState();
    auto chain = masterBus.getChildWithType (IDs::PLUGIN_CHAIN);

    // Build list of enabled plugin nodes
    std::vector<NodeId> enabledNodes;
    for (int p = 0; p < static_cast<int> (masterPluginChain.size()); ++p)
    {
        if (chain.isValid() && p < chain.getNumChildren())
        {
            bool enabled = chain.getChild (p).getProperty (IDs::pluginEnabled, true);
            if (enabled)
                enabledNodes.push_back (masterPluginChain[static_cast<size_t> (p)].node);
        }
    }

    // Wire: MixBus -> [enabled plugins] -> masterMeterTap -> AudioOutput
    auto prevId = mixBusNode;
    for (auto pluginNodeId : enabledNodes)
    {
        audioEngine.connectNodes (prevId, 0, pluginNodeId, 0);
        audioEngine.connectNodes (prevId, 1, pluginNodeId, 1);
        prevId = pluginNodeId;
    }

    audioEngine.connectNodes (prevId, 0, masterMeterTapNode, 0);
    audioEngine.connectNodes (prevId, 1, masterMeterTapNode, 1);

    audioEngine.connectNodes (masterMeterTapNode, 0,
                              audioEngine.getAudioOutputNodeId(), 0);
    audioEngine.connectNodes (masterMeterTapNode, 1,
                              audioEngine.getAudioOutputNodeId(), 1);
}

void MainComponent::disconnectMasterPluginChain()
{
    if (mixBusNode == 0)
        return;

    auto& graph = audioEngine.getGraph();

    // Disconnect all nodes in the master chain
    graph.disconnectNode (mixBusNode);

    for (auto& info : masterPluginChain)
    {
        if (info.node != 0)
            graph.disconnectNode (info.node);
    }

    if (masterMeterTapNode != 0)
        graph.disconnectNode (masterMeterTapNode);
}

void MainComponent::openMasterPluginEditor (int pluginIndex)
{
    if (pluginIndex < 0 || pluginIndex >= static_cast<int> (masterPluginChain.size()))
        return;

    auto* plugin = masterPluginChain[static_cast<size_t> (pluginIndex)].plugin;
    if (plugin != nullptr)
        pluginWindowManager.showEditorForPlugin (*plugin);
}

void MainComponent::insertPluginOnMaster (const dc::PluginDescription& desc)
{
    auto masterBus = project.getMasterBusState();
    auto chain = masterBus.getChildWithType (IDs::PLUGIN_CHAIN);
    if (! chain.isValid())
    {
        chain = PropertyTree (IDs::PLUGIN_CHAIN);
        masterBus.addChild (chain, -1, nullptr);
    }

    // Add to model
    auto pluginNode = PropertyTree (IDs::PLUGIN);
    pluginNode.setProperty (IDs::pluginName, desc.name, nullptr);
    pluginNode.setProperty (IDs::pluginFormat, "VST3", nullptr);
    pluginNode.setProperty (IDs::pluginManufacturer, desc.manufacturer, nullptr);
    pluginNode.setProperty (IDs::pluginUniqueId, 0, nullptr);
    pluginNode.setProperty (IDs::pluginFileOrIdentifier, desc.path.string(), nullptr);
    pluginNode.setProperty (IDs::pluginEnabled, true, nullptr);
    chain.addChild (pluginNode, -1, &project.getUndoManager());

    // Async instantiate and add to graph
    auto sampleRate = audioEngine.getSampleRate();
    auto blockSize = audioEngine.getBufferSize();

    pluginHost.createPluginAsync (desc, sampleRate, blockSize,
        [this] (std::unique_ptr<dc::PluginInstance> instance, const std::string& errorMessage)
        {
            (void) errorMessage;
            if (instance == nullptr)
                return;

            auto* pluginPtr = instance.get();

            // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
            disconnectMasterPluginChain();

            auto wrapper = std::make_unique<PluginProcessorNode> (std::move (instance));
            auto node = audioEngine.addProcessor (std::move (wrapper));
            masterPluginChain.push_back ({ node, pluginPtr });

            connectMasterPluginChain();
        });
}

void MainComponent::insertPluginOnTrack (int trackIndex, const dc::PluginDescription& desc)
{
    if (trackIndex < 0 || trackIndex >= project.getNumTracks())
        return;

    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    // Add to model
    track.addPlugin (desc.name, "VST3",
                     desc.manufacturer,
                     0, desc.path.string(),
                     &project.getUndoManager());

    // Async instantiate and add to graph
    auto sampleRate = audioEngine.getSampleRate();
    auto blockSize = audioEngine.getBufferSize();

    pluginHost.createPluginAsync (desc, sampleRate, blockSize,
        [this, trackIndex] (std::unique_ptr<dc::PluginInstance> instance, const std::string& errorMessage)
        {
            (void) errorMessage;
            if (instance == nullptr)
                return;

            auto* pluginPtr = instance.get();

            // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
            disconnectTrackPluginChain (trackIndex);

            auto wrapper = std::make_unique<PluginProcessorNode> (std::move (instance));
            auto pluginNode = audioEngine.addProcessor (std::move (wrapper));

            if (trackIndex < static_cast<int> (trackPluginChains.size()))
                trackPluginChains[static_cast<size_t> (trackIndex)].push_back ({ pluginNode, pluginPtr });

            connectTrackPluginChain (trackIndex);
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
        if (auto* bp = dynamic_cast<BrowserPanel*> (browserPanel.get()))
            bp->clearSearchFilter();
        // Return to normal mode if we were in plugin menu
        if (vimEngine->getMode() == VimEngine::PluginMenu)
            vimEngine->enterNormalMode();
    }
}

} // namespace dc
