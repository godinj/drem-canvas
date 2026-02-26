#include "AppController.h"
#include "graphics/rendering/Canvas.h"
#include "model/Track.h"
#include "model/AudioClip.h"
#include "model/StepSequencer.h"
#include "platform/NativeDialogs.h"
#include "utils/UndoSystem.h"

namespace dc
{
namespace ui
{

AppController::AppController()
{
}

AppController::~AppController()
{
    if (vimEngine)
    {
        vimEngine->removeListener (this);
        if (arrangementWidget)
            vimEngine->removeListener (arrangementWidget.get());
    }

    project.getState().getChildWithName (IDs::STEP_SEQUENCER).removeListener (this);
    project.getState().getChildWithName (IDs::TRACKS).removeListener (this);

    pluginWindowManager.closeAll();
    trackPluginChains.clear();
    trackProcessors.clear();
    trackNodes.clear();
    sequencerProcessor = nullptr;
    sequencerNode = nullptr;
    mixBusNode = nullptr;
    audioEngine.shutdown();
}

void AppController::initialise()
{
    // Initialise audio engine with stereo I/O
    audioEngine.initialise (2, 2);
    transportController.setSampleRate (
        audioEngine.getDeviceManager().getCurrentAudioDevice()
            ? audioEngine.getDeviceManager().getCurrentAudioDevice()->getCurrentSampleRate()
            : 44100.0);

    // Create mix bus processor
    mixBusNode = audioEngine.addProcessor (std::make_unique<MixBusProcessor> (transportController));
    audioEngine.connectNodes (mixBusNode->nodeID, 0,
                              audioEngine.getAudioOutputNode()->nodeID, 0);
    audioEngine.connectNodes (mixBusNode->nodeID, 1,
                              audioEngine.getAudioOutputNode()->nodeID, 1);

    // Create step sequencer processor
    {
        auto proc = std::make_unique<StepSequencerProcessor> (transportController);
        sequencerProcessor = proc.get();
        sequencerNode = audioEngine.addProcessor (std::move (proc));
        audioEngine.connectNodes (sequencerNode->nodeID, 0, mixBusNode->nodeID, 0);
        audioEngine.connectNodes (sequencerNode->nodeID, 1, mixBusNode->nodeID, 1);
        sequencerProcessor->setTempo (project.getTempo());
        syncSequencerFromModel();
    }

    // Load plugin list
    pluginManager.loadPluginList (pluginManager.getDefaultPluginListFile());

    // Create vim engine
    vimEngine = std::make_unique<VimEngine> (project, transportController, arrangement, vimContext);
    vimEngine->addListener (this);

    // Wire :plugin command
    vimEngine->onPluginCommand = [this] (const juce::String& pluginName)
    {
        auto& knownPlugins = pluginManager.getKnownPlugins();
        auto types = knownPlugins.getTypes();

        for (const auto& desc : types)
        {
            if (desc.name.containsIgnoreCase (pluginName))
            {
                insertPluginOnTrack (arrangement.getSelectedTrackIndex(), desc);
                return;
            }
        }
    };

    // ─── Create UI widgets ───────────────────────────────

    // Transport bar
    transportBar = std::make_unique<TransportBarWidget> (transportController);
    addChild (transportBar.get());

    transportBar->onSaveSession   = [this]() { saveSession(); };
    transportBar->onLoadSession   = [this]() { loadSession(); };
    transportBar->onImportAudio   = [this]() { openFile(); };
    transportBar->onAudioSettings = [this]() { showAudioSettings(); };
    transportBar->onToggleBrowser = [this]() { toggleBrowser(); };

    // Vim status bar
    vimStatusBar = std::make_unique<VimStatusBarWidget> (*vimEngine, vimContext,
                                                          arrangement, transportController);
    addChild (vimStatusBar.get());

    // Arrangement
    arrangementWidget = std::make_unique<ArrangementWidget> (project, transportController,
                                                               arrangement, vimContext);
    addChild (arrangementWidget.get());
    vimEngine->addListener (arrangementWidget.get());

    // Mixer
    mixerWidget = std::make_unique<MixerWidget> (project);
    addChild (mixerWidget.get());

    // Step sequencer (hidden initially)
    sequencerWidget = std::make_unique<StepSequencerWidget> (project);
    sequencerWidget->setVisible (false);
    addChild (sequencerWidget.get());

    // Piano roll (hidden initially)
    pianoRollWidget = std::make_unique<PianoRollWidget>();
    pianoRollWidget->setVisible (false);
    addChild (pianoRollWidget.get());

    // Browser (hidden initially)
    browserWidget = std::make_unique<BrowserWidget> (pluginManager);
    browserWidget->setVisible (false);
    addChild (browserWidget.get());

    browserWidget->onPluginSelected = [this] (const juce::PluginDescription& desc)
    {
        insertPluginOnTrack (arrangement.getSelectedTrackIndex(), desc);
    };

    // Command palette (added last so it renders on top)
    commandPalette = std::make_unique<CommandPaletteWidget> (actionRegistry);
    commandPalette->setVisible (false);
    addChild (commandPalette.get());

    commandPalette->onDismiss = [this]() { dismissCommandPalette(); };

    // Wire command palette trigger
    vimEngine->onCommandPalette = [this]() { showCommandPalette(); };

    // Register all actions in the palette
    registerAllActions();

    // Register animating widgets
    if (renderer)
    {
        renderer->addAnimatingWidget (transportBar.get());
        renderer->addAnimatingWidget (vimStatusBar.get());
        renderer->addAnimatingWidget (arrangementWidget.get());
    }

    // Listen to model changes
    project.getState().getChildWithName (IDs::TRACKS).addListener (this);
    project.getState().getChildWithName (IDs::STEP_SEQUENCER).addListener (this);

    // Sync tempo
    tempoMap.setTempo (project.getTempo());

    // Select first track
    if (arrangement.getNumTracks() > 0)
        arrangement.selectTrack (0);

    resized();
}

void AppController::setRenderer (gfx::Renderer* r)
{
    renderer = r;
}

void AppController::paint (gfx::Canvas& canvas)
{
    auto& theme = gfx::Theme::getDefault();
    canvas.clear (theme.windowBackground);
}

void AppController::resized()
{
    float w = getWidth();
    float h = getHeight();

    if (w <= 0 || h <= 0)
        return;

    auto& theme = gfx::Theme::getDefault();

    float transportH = theme.transportHeight;
    float statusH = theme.statusBarHeight;
    float browserW = browserVisible ? 200.0f : 0.0f;

    // Transport bar at top
    if (transportBar)
        transportBar->setBounds (0, 0, w, transportH);

    // Status bar at bottom
    if (vimStatusBar)
        vimStatusBar->setBounds (0, h - statusH, w, statusH);

    // Center area between transport and status bar
    float centerX = 0;
    float centerY = transportH;
    float centerW = w - browserW;
    float centerH = h - transportH - statusH;

    // Browser on the right
    if (browserWidget && browserVisible)
        browserWidget->setBounds (w - browserW, centerY, browserW, centerH);

    // Arrangement/mixer split
    float arrangementH = centerH * splitRatio;
    float bottomH = centerH - arrangementH;

    if (arrangementWidget)
        arrangementWidget->setBounds (centerX, centerY, centerW, arrangementH);

    // Bottom panel: mixer or sequencer
    bool showSequencer = (vimContext.getPanel() == VimContext::Sequencer);

    if (mixerWidget)
    {
        mixerWidget->setVisible (! showSequencer);
        if (! showSequencer)
            mixerWidget->setBounds (centerX, centerY + arrangementH, centerW, bottomH);
    }

    if (sequencerWidget)
    {
        sequencerWidget->setVisible (showSequencer);
        if (showSequencer)
            sequencerWidget->setBounds (centerX, centerY + arrangementH, centerW, bottomH);
    }

    // Command palette overlay (centered, top portion)
    if (commandPalette)
    {
        float paletteY = h * 0.12f;
        commandPalette->setBounds (0, paletteY, w, h - paletteY);
    }
}

bool AppController::keyDown (const gfx::KeyEvent& e)
{
    // Route all keys through VimEngine first
    if (vimEngine && vimEngine->handleKeyEvent (e))
        return true;

    return false;
}

// ─── Command palette ─────────────────────────────────────────

void AppController::paintOverChildren (gfx::Canvas& canvas)
{
    if (commandPalette && commandPalette->isShowing())
    {
        canvas.fillRect (gfx::Rect (0, 0, getWidth(), getHeight()),
                         gfx::Color (0, 0, 0, 128));
    }
}

void AppController::showCommandPalette()
{
    if (! commandPalette)
        return;

    commandPalette->show (vimContext.getPanel());
    gfx::Widget::setCurrentFocus (commandPalette.get());
}

void AppController::dismissCommandPalette()
{
    gfx::Widget::setCurrentFocus (nullptr);
}

void AppController::registerAllActions()
{
    // ─── Transport ───────────────────────────────────────────
    actionRegistry.registerAction ({
        "transport.play_stop", "Play / Stop", "Transport", "Space",
        [this]() { vimEngine->togglePlayStop(); }, {}
    });

    actionRegistry.registerAction ({
        "transport.jump_start", "Jump to Start", "Transport", "0",
        [this]() { vimEngine->jumpToSessionStart(); }, {}
    });

    actionRegistry.registerAction ({
        "transport.jump_end", "Jump to End", "Transport", "$",
        [this]() { vimEngine->jumpToSessionEnd(); }, {}
    });

    // ─── Track ───────────────────────────────────────────────
    actionRegistry.registerAction ({
        "track.toggle_mute", "Toggle Mute", "Track", "M",
        [this]() { vimEngine->toggleMute(); }, {}
    });

    actionRegistry.registerAction ({
        "track.toggle_solo", "Toggle Solo", "Track", "S",
        [this]() { vimEngine->toggleSolo(); }, {}
    });

    actionRegistry.registerAction ({
        "track.toggle_record_arm", "Toggle Record Arm", "Track", "r",
        [this]() { vimEngine->toggleRecordArm(); }, {}
    });

    actionRegistry.registerAction ({
        "track.add_from_file", "Import Audio File", "Track", "",
        [this]() { openFile(); }, {}
    });

    // ─── Edit ────────────────────────────────────────────────
    actionRegistry.registerAction ({
        "edit.undo", "Undo", "Edit", "u",
        [this]() { project.getUndoSystem().undo(); }, {}
    });

    actionRegistry.registerAction ({
        "edit.redo", "Redo", "Edit", "Ctrl+R",
        [this]() { project.getUndoSystem().redo(); }, {}
    });

    actionRegistry.registerAction ({
        "edit.delete", "Delete Selected Clip", "Edit", "x",
        [this]() { vimEngine->deleteSelectedRegions(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "edit.yank", "Yank (Copy) Selected Clip", "Edit", "yy",
        [this]() { vimEngine->yankSelectedRegions(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "edit.paste_after", "Paste After Playhead", "Edit", "p",
        [this]() { vimEngine->pasteAfterPlayhead(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "edit.paste_before", "Paste Before Playhead", "Edit", "P",
        [this]() { vimEngine->pasteBeforePlayhead(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "edit.split", "Split Clip at Playhead", "Edit", "s",
        [this]() { vimEngine->splitRegionAtPlayhead(); },
        { VimContext::Editor }
    });

    // ─── File ────────────────────────────────────────────────
    actionRegistry.registerAction ({
        "file.save", "Save Session", "File", "",
        [this]() { saveSession(); }, {}
    });

    actionRegistry.registerAction ({
        "file.load", "Load Session", "File", "",
        [this]() { loadSession(); }, {}
    });

    actionRegistry.registerAction ({
        "file.import_audio", "Import Audio", "File", "",
        [this]() { openFile(); }, {}
    });

    actionRegistry.registerAction ({
        "file.audio_settings", "Audio Settings", "File", "",
        [this]() { showAudioSettings(); }, {}
    });

    // ─── Navigation ──────────────────────────────────────────
    actionRegistry.registerAction ({
        "nav.up", "Move Up", "Navigation", "k",
        [this]() { vimEngine->moveSelectionUp(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "nav.down", "Move Down", "Navigation", "j",
        [this]() { vimEngine->moveSelectionDown(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "nav.left", "Move Left", "Navigation", "h",
        [this]() { vimEngine->moveSelectionLeft(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "nav.right", "Move Right", "Navigation", "l",
        [this]() { vimEngine->moveSelectionRight(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "nav.first_track", "Jump to First Track", "Navigation", "gg",
        [this]() { vimEngine->jumpToFirstTrack(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "nav.last_track", "Jump to Last Track", "Navigation", "G",
        [this]() { vimEngine->jumpToLastTrack(); },
        { VimContext::Editor }
    });

    actionRegistry.registerAction ({
        "nav.cycle_panel", "Cycle Panel", "Navigation", "Tab",
        [this]() { vimEngine->cycleFocusPanel(); }, {}
    });

    // ─── Mode ────────────────────────────────────────────────
    actionRegistry.registerAction ({
        "mode.insert", "Enter Insert Mode", "Mode", "i",
        [this]() { vimEngine->enterInsertMode(); }, {}
    });

    actionRegistry.registerAction ({
        "mode.normal", "Enter Normal Mode", "Mode", "Esc",
        [this]() { vimEngine->enterNormalMode(); }, {}
    });

    // ─── View ────────────────────────────────────────────────
    actionRegistry.registerAction ({
        "view.toggle_browser", "Toggle Browser", "View", "",
        [this]() { toggleBrowser(); }, {}
    });

    // ─── Sequencer ───────────────────────────────────────────
    actionRegistry.registerAction ({
        "seq.move_left", "Sequencer Move Left", "Sequencer", "h",
        [this]() { vimEngine->seqMoveLeft(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.move_right", "Sequencer Move Right", "Sequencer", "l",
        [this]() { vimEngine->seqMoveRight(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.move_up", "Sequencer Move Up", "Sequencer", "k",
        [this]() { vimEngine->seqMoveUp(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.move_down", "Sequencer Move Down", "Sequencer", "j",
        [this]() { vimEngine->seqMoveDown(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.toggle_step", "Toggle Step", "Sequencer", "Space",
        [this]() { vimEngine->seqToggleStep(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.cycle_velocity", "Cycle Velocity", "Sequencer", "v",
        [this]() { vimEngine->seqCycleVelocity(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.velocity_up", "Increase Velocity", "Sequencer", "+",
        [this]() { vimEngine->seqAdjustVelocity (10); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.velocity_down", "Decrease Velocity", "Sequencer", "-",
        [this]() { vimEngine->seqAdjustVelocity (-10); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.toggle_row_mute", "Toggle Row Mute", "Sequencer", "M",
        [this]() { vimEngine->seqToggleRowMute(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.toggle_row_solo", "Toggle Row Solo", "Sequencer", "S",
        [this]() { vimEngine->seqToggleRowSolo(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.jump_first_step", "Jump to First Step", "Sequencer", "0",
        [this]() { vimEngine->seqJumpFirstStep(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.jump_last_step", "Jump to Last Step", "Sequencer", "$",
        [this]() { vimEngine->seqJumpLastStep(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.jump_first_row", "Jump to First Row", "Sequencer", "gg",
        [this]() { vimEngine->seqJumpFirstRow(); },
        { VimContext::Sequencer }
    });

    actionRegistry.registerAction ({
        "seq.jump_last_row", "Jump to Last Row", "Sequencer", "G",
        [this]() { vimEngine->seqJumpLastRow(); },
        { VimContext::Sequencer }
    });
}

// ─── Audio graph ─────────────────────────────────────────────

void AppController::rebuildAudioGraph()
{
    audioEngine.getGraph().suspendProcessing (true);

    // Close plugin editor windows before removing nodes
    pluginWindowManager.closeAll();

    // Remove existing plugin chain nodes
    for (auto& chain : trackPluginChains)
        for (auto& info : chain)
            if (info.node != nullptr)
                audioEngine.removeProcessor (info.node->nodeID);
    trackPluginChains.clear();

    // Remove existing track nodes
    for (auto& node : trackNodes)
        if (node != nullptr)
            audioEngine.removeProcessor (node->nodeID);

    trackProcessors.clear();
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
        trackNodes.add (node);

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
                juce::String base64State = pluginState.getProperty (IDs::pluginState, juce::String());
                if (base64State.isNotEmpty())
                    PluginHost::restorePluginState (*instance, base64State);

                auto* pluginPtr = instance.get();
                auto pluginNode = audioEngine.addProcessor (std::move (instance));
                pluginChain.add ({ pluginNode, pluginPtr });
            }
        }

        trackPluginChains.add (pluginChain);
        connectTrackPluginChain (i);
    }

    audioEngine.getGraph().suspendProcessing (false);

    // Rebuild UI views
    if (arrangementWidget != nullptr)
        arrangementWidget->rebuildTrackLanes();

    if (mixerWidget != nullptr)
        mixerWidget->rebuildStrips();
}

void AppController::syncTrackProcessorsFromModel()
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

void AppController::syncSequencerFromModel()
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

// ─── Plugin chain wiring ─────────────────────────────────────

void AppController::connectTrackPluginChain (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= trackNodes.size()
        || trackIndex >= trackPluginChains.size() || mixBusNode == nullptr)
        return;

    auto trackNode = trackNodes[trackIndex];
    auto& chain = trackPluginChains.getReference (trackIndex);
    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    juce::Array<juce::AudioProcessorGraph::Node::Ptr> enabledNodes;
    for (int p = 0; p < chain.size(); ++p)
    {
        if (track.isPluginEnabled (p))
            enabledNodes.add (chain[p].node);
    }

    auto prevNode = trackNode;
    for (auto& pluginNode : enabledNodes)
    {
        audioEngine.connectNodes (prevNode->nodeID, 0, pluginNode->nodeID, 0);
        audioEngine.connectNodes (prevNode->nodeID, 1, pluginNode->nodeID, 1);
        prevNode = pluginNode;
    }

    audioEngine.connectNodes (prevNode->nodeID, 0, mixBusNode->nodeID, 0);
    audioEngine.connectNodes (prevNode->nodeID, 1, mixBusNode->nodeID, 1);
}

void AppController::disconnectTrackPluginChain (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= trackNodes.size()
        || trackIndex >= trackPluginChains.size() || mixBusNode == nullptr)
        return;

    auto& graph = audioEngine.getGraph();
    auto trackNode = trackNodes[trackIndex];
    auto& chain = trackPluginChains.getReference (trackIndex);

    for (auto& conn : graph.getConnections())
    {
        if (conn.source.nodeID == trackNode->nodeID)
            graph.removeConnection (conn);
    }

    for (auto& info : chain)
    {
        if (info.node == nullptr) continue;
        for (auto& conn : graph.getConnections())
        {
            if (conn.source.nodeID == info.node->nodeID)
                graph.removeConnection (conn);
        }
    }
}

void AppController::openPluginEditor (int trackIndex, int pluginIndex)
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

void AppController::captureAllPluginStates()
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

void AppController::insertPluginOnTrack (int trackIndex, const juce::PluginDescription& desc)
{
    if (trackIndex < 0 || trackIndex >= project.getNumTracks())
        return;

    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    track.addPlugin (desc.name, desc.pluginFormatName, desc.manufacturerName,
                     desc.uniqueId, desc.fileOrIdentifier, &project.getUndoManager());

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

// ─── Session management ──────────────────────────────────────

void AppController::saveSession()
{
    captureAllPluginStates();

    platform::NativeDialogs::showSavePanel ("Save Session", "Untitled Session",
        [this] (const std::string& path)
        {
            if (path.empty())
                return;

            juce::File dir (path);
            if (project.saveSessionToDirectory (dir))
            {
                currentSessionDirectory = dir;
            }
            else
            {
                platform::NativeDialogs::showAlert ("Save Error",
                    "Failed to save session to:\n" + path);
            }
        });
}

void AppController::loadSession()
{
    platform::NativeDialogs::showOpenPanel ("Load Session", {},
        [this] (const std::string& path)
        {
            if (path.empty())
                return;

            juce::File dir (path);
            if (! dir.isDirectory())
                return;

            project.getState().getChildWithName (IDs::STEP_SEQUENCER).removeListener (this);
            project.getState().getChildWithName (IDs::TRACKS).removeListener (this);

            if (project.loadSessionFromDirectory (dir))
            {
                currentSessionDirectory = dir;
                project.getState().getChildWithName (IDs::TRACKS).addListener (this);
                project.getState().getChildWithName (IDs::STEP_SEQUENCER).addListener (this);
                rebuildAudioGraph();
                syncSequencerFromModel();
            }
            else
            {
                project.getState().getChildWithName (IDs::TRACKS).addListener (this);
                project.getState().getChildWithName (IDs::STEP_SEQUENCER).addListener (this);

                platform::NativeDialogs::showAlert ("Load Error",
                    "Failed to load session from:\n" + path);
            }
        });
}

void AppController::openFile()
{
    platform::NativeDialogs::showOpenPanel ("Select an audio file...",
        { "wav", "aiff", "mp3", "flac", "ogg" },
        [this] (const std::string& path)
        {
            if (path.empty())
                return;
            juce::File file (path);
            if (file.existsAsFile())
                addTrackFromFile (file);
        });
}

void AppController::addTrackFromFile (const juce::File& file)
{
    auto trackName = file.getFileNameWithoutExtension();
    auto trackState = project.addTrack (trackName);

    auto tempProcessor = std::make_unique<TrackProcessor> (transportController);
    if (tempProcessor->loadFile (file))
    {
        auto length = tempProcessor->getFileLengthInSamples();
        Track track (trackState);
        track.addAudioClip (file, 0, length);
    }

    rebuildAudioGraph();
}

void AppController::showAudioSettings()
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

// ─── Panel visibility ────────────────────────────────────────

void AppController::updatePanelVisibility()
{
    resized();
    repaint();
}

void AppController::toggleBrowser()
{
    browserVisible = ! browserVisible;
    if (browserWidget)
        browserWidget->setVisible (browserVisible);
    resized();
    repaint();
}

// ─── ValueTree::Listener ─────────────────────────────────────

void AppController::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree.hasType (IDs::TRACK))
    {
        if (property == IDs::volume || property == IDs::pan || property == IDs::mute)
            syncTrackProcessorsFromModel();
    }

    if (tree.hasType (IDs::PROJECT) && property == IDs::tempo)
    {
        if (sequencerProcessor != nullptr)
            sequencerProcessor->setTempo (project.getTempo());
    }

    if (tree.hasType (IDs::STEP_SEQUENCER) || tree.hasType (IDs::STEP_PATTERN)
        || tree.hasType (IDs::STEP_ROW) || tree.hasType (IDs::STEP))
    {
        syncSequencerFromModel();
    }

    repaint();
}

void AppController::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildAudioGraph();

    if (parent.hasType (IDs::STEP_SEQUENCER) || parent.hasType (IDs::STEP_PATTERN)
        || parent.hasType (IDs::STEP_ROW))
        syncSequencerFromModel();
}

void AppController::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent.hasType (IDs::TRACKS))
        rebuildAudioGraph();

    if (parent.hasType (IDs::STEP_SEQUENCER) || parent.hasType (IDs::STEP_PATTERN)
        || parent.hasType (IDs::STEP_ROW))
        syncSequencerFromModel();
}

// ─── VimEngine::Listener ─────────────────────────────────────

void AppController::vimModeChanged (VimEngine::Mode)
{
    repaint();
}

void AppController::vimContextChanged()
{
    updatePanelVisibility();

    // Sync grid cursor to VimContext
    if (sequencerWidget)
    {
        // Sequencer widget handles cursor updates via VimContext internally
    }
}

} // namespace ui
} // namespace dc
