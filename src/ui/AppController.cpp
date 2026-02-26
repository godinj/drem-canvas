#include "AppController.h"
#include "graphics/rendering/Canvas.h"
#include "model/Track.h"
#include "model/AudioClip.h"
#include "model/MidiClip.h"
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

    midiEngine.shutdown();
    pluginWindowManager.closeAll();
    trackPluginChains.clear();
    trackProcessors.clear();
    midiClipProcessors.clear();
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

    vimEngine->onCreateMidiTrack = [this] (const juce::String& name) { addMidiTrack (name); };

    // Wire piano roll open
    vimEngine->onOpenPianoRoll = [this] (const juce::ValueTree& clipState)
    {
        if (pianoRollWidget)
            pianoRollWidget->loadClip (clipState);
    };

    // Wire piano roll action callbacks
    vimEngine->onSetPianoRollTool = [this] (int tool)
    {
        if (pianoRollWidget)
            pianoRollWidget->setTool (static_cast<PianoRollWidget::Tool> (tool));
    };

    vimEngine->onPianoRollDeleteSelected = [this]()
    {
        if (pianoRollWidget) pianoRollWidget->deleteSelectedNotes();
    };

    vimEngine->onPianoRollCopy = [this]()
    {
        if (pianoRollWidget) pianoRollWidget->copySelectedNotes();
    };

    vimEngine->onPianoRollPaste = [this]()
    {
        if (pianoRollWidget) pianoRollWidget->pasteNotes();
    };

    vimEngine->onPianoRollDuplicate = [this]()
    {
        if (pianoRollWidget) pianoRollWidget->duplicateSelectedNotes();
    };

    vimEngine->onPianoRollTranspose = [this] (int semitones)
    {
        if (pianoRollWidget) pianoRollWidget->transposeSelected (semitones);
    };

    vimEngine->onPianoRollSelectAll = [this]()
    {
        if (pianoRollWidget) pianoRollWidget->selectAll();
    };

    vimEngine->onPianoRollQuantize = [this]()
    {
        if (pianoRollWidget) pianoRollWidget->quantizeSelected();
    };

    vimEngine->onPianoRollHumanize = [this]()
    {
        if (pianoRollWidget) pianoRollWidget->humanizeSelected();
    };

    vimEngine->onPianoRollVelocityLane = [this] (bool)
    {
        if (pianoRollWidget)
            pianoRollWidget->setVelocityLaneVisible (! pianoRollWidget->isVelocityLaneVisible());
    };

    vimEngine->onPianoRollZoom = [this] (float factor)
    {
        if (pianoRollWidget) pianoRollWidget->zoomHorizontal (factor);
    };

    vimEngine->onPianoRollZoomToFit = [this]()
    {
        if (pianoRollWidget) pianoRollWidget->zoomToFit();
    };

    vimEngine->onPianoRollGridDiv = [this] (int delta)
    {
        if (! pianoRollWidget) return;
        int div = pianoRollWidget->getGridDivision();
        if (delta > 0)
            div = std::min (16, div * 2);
        else
            div = std::max (1, div / 2);
        pianoRollWidget->setGridDivision (div);
    };

    vimEngine->onPianoRollMoveCursor = [this] (int dBeatCol, int dNoteRow)
    {
        if (! pianoRollWidget) return;
        int newCol = pianoRollWidget->getPrBeatCol() + dBeatCol;
        int newRow = pianoRollWidget->getPrNoteRow() + dNoteRow;
        newCol = std::max (0, newCol);
        newRow = juce::jlimit (0, 127, newRow);
        pianoRollWidget->setPrBeatCol (newCol);
        pianoRollWidget->setPrNoteRow (newRow);
    };

    vimEngine->onPianoRollAddNote = [this]()
    {
        if (! pianoRollWidget) return;
        auto& clipState = vimContext.openClipState;
        if (! clipState.isValid()) return;

        int noteNumber = pianoRollWidget->getPrNoteRow();
        double beat = static_cast<double> (pianoRollWidget->getPrBeatCol())
                    / pianoRollWidget->getGridDivision();
        double length = 1.0 / pianoRollWidget->getGridDivision();

        ScopedTransaction txn (project.getUndoSystem(), "Add Note");
        MidiClip clip (clipState);
        clip.addNote (noteNumber, beat, length, 100, &project.getUndoManager());
    };

    vimEngine->onPianoRollJumpCursor = [this] (int beatCol, int noteRow)
    {
        if (! pianoRollWidget) return;

        if (beatCol >= 0)
        {
            // Clamp to content bounds
            int maxCol = static_cast<int> (128.0 * pianoRollWidget->getGridDivision());
            pianoRollWidget->setPrBeatCol (std::min (beatCol, maxCol));
        }
        if (noteRow >= 0)
            pianoRollWidget->setPrNoteRow (juce::jlimit (0, 127, noteRow));
    };

    // Initialise MIDI engine
    midiEngine.initialise();

    // Wire MIDI recording: when piano roll is open and recording,
    // incoming MIDI notes create notes in real-time
    midiEngine.onMidiMessage = [this] (const juce::MidiMessage& msg)
    {
        if (! pianoRollWidget || ! pianoRollWidget->isVisible())
            return;

        if (! midiEngine.isRecording())
            return;

        auto& clipState = vimContext.openClipState;
        if (! clipState.isValid())
            return;

        if (msg.isNoteOn())
        {
            // Convert current transport position to beat-relative
            auto posSamples = transportController.getPositionInSamples();
            double sr = project.getSampleRate();
            double tempo = project.getTempo();
            int64_t clipStart = static_cast<int64_t> (
                static_cast<juce::int64> (clipState.getProperty (IDs::startPosition, 0)));

            double relativeSamples = static_cast<double> (posSamples - clipStart);
            double relativeSeconds = relativeSamples / sr;
            double relativeBeat = relativeSeconds * tempo / 60.0;

            if (relativeBeat >= 0.0)
            {
                MidiClip clip (clipState);
                clip.addNote (msg.getNoteNumber(), relativeBeat, 0.25,
                              msg.getVelocity(), &project.getUndoManager());
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

    mixerWidget->onPluginClicked = [this] (int trackIndex, int pluginIndex)
    {
        openPluginEditor (trackIndex, pluginIndex);
    };

    // Step sequencer (hidden initially)
    sequencerWidget = std::make_unique<StepSequencerWidget> (project);
    sequencerWidget->setVisible (false);
    addChild (sequencerWidget.get());

    // Piano roll (hidden initially)
    pianoRollWidget = std::make_unique<PianoRollWidget> (project, transportController);
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
        renderer->addAnimatingWidget (pianoRollWidget.get());
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

    // Bottom panel: mixer, sequencer, or piano roll
    auto currentPanel = vimContext.getPanel();
    bool showSequencer = (currentPanel == VimContext::Sequencer);
    bool showPianoRoll = (currentPanel == VimContext::PianoRoll);

    if (mixerWidget)
    {
        mixerWidget->setVisible (! showSequencer && ! showPianoRoll);
        if (! showSequencer && ! showPianoRoll)
            mixerWidget->setBounds (centerX, centerY + arrangementH, centerW, bottomH);
    }

    if (sequencerWidget)
    {
        sequencerWidget->setVisible (showSequencer);
        if (showSequencer)
            sequencerWidget->setBounds (centerX, centerY + arrangementH, centerW, bottomH);
    }

    if (pianoRollWidget)
    {
        pianoRollWidget->setVisible (showPianoRoll);
        if (showPianoRoll)
            pianoRollWidget->setBounds (centerX, centerY + arrangementH, centerW, bottomH);
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

    actionRegistry.registerAction ({
        "track.add_midi", "Create MIDI Track", "Track", ":midi",
        [this]() { addMidiTrack ("MIDI"); }, {}
    });

    actionRegistry.registerAction ({
        "track.add_plugin", "Add Plugin to Track", "Track", ":plugin",
        [this]() { if (! browserVisible) toggleBrowser(); }, {}
    });

    actionRegistry.registerAction ({
        "track.open_plugin", "Open Plugin Editor", "Track", "",
        [this]()
        {
            int trackIdx = arrangement.getSelectedTrackIndex();
            if (trackIdx >= 0)
                openPluginEditor (trackIdx, 0);
        }, {}
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

    actionRegistry.registerAction ({
        "edit.duplicate", "Duplicate Selected Clip", "Edit", "D",
        [this]() { vimEngine->duplicateSelectedClip(); },
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

    // ─── Piano Roll ─────────────────────────────────────────
    actionRegistry.registerAction ({
        "pr.tool_select", "Select Tool", "Piano Roll", "1",
        [this]() { if (pianoRollWidget) pianoRollWidget->setTool (PianoRollWidget::Select); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.tool_draw", "Draw Tool", "Piano Roll", "2",
        [this]() { if (pianoRollWidget) pianoRollWidget->setTool (PianoRollWidget::Draw); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.tool_erase", "Erase Tool", "Piano Roll", "3",
        [this]() { if (pianoRollWidget) pianoRollWidget->setTool (PianoRollWidget::Erase); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.delete", "Delete Selected Notes", "Piano Roll", "x",
        [this]() { if (pianoRollWidget) pianoRollWidget->deleteSelectedNotes(); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.copy", "Copy Notes", "Piano Roll", "y",
        [this]() { if (pianoRollWidget) pianoRollWidget->copySelectedNotes(); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.paste", "Paste Notes", "Piano Roll", "p",
        [this]() { if (pianoRollWidget) pianoRollWidget->pasteNotes(); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.duplicate", "Duplicate Notes", "Piano Roll", "D",
        [this]() { if (pianoRollWidget) pianoRollWidget->duplicateSelectedNotes(); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.transpose_up", "Transpose Up", "Piano Roll", "+",
        [this]() { if (pianoRollWidget) pianoRollWidget->transposeSelected (1); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.transpose_down", "Transpose Down", "Piano Roll", "-",
        [this]() { if (pianoRollWidget) pianoRollWidget->transposeSelected (-1); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.select_all", "Select All Notes", "Piano Roll", "Ctrl+A",
        [this]() { if (pianoRollWidget) pianoRollWidget->selectAll(); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.quantize", "Quantize Notes", "Piano Roll", "q",
        [this]() { if (pianoRollWidget) pianoRollWidget->quantizeSelected(); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.humanize", "Humanize Notes", "Piano Roll", "Q",
        [this]() { if (pianoRollWidget) pianoRollWidget->humanizeSelected(); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.zoom_in", "Zoom In", "Piano Roll", "zi",
        [this]() { if (pianoRollWidget) pianoRollWidget->zoomHorizontal (1.25f); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.zoom_out", "Zoom Out", "Piano Roll", "zo",
        [this]() { if (pianoRollWidget) pianoRollWidget->zoomHorizontal (0.8f); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.zoom_fit", "Zoom to Fit", "Piano Roll", "zf",
        [this]() { if (pianoRollWidget) pianoRollWidget->zoomToFit(); },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.velocity_lane", "Toggle Velocity Lane", "Piano Roll", "v",
        [this]()
        {
            if (pianoRollWidget)
                pianoRollWidget->setVelocityLaneVisible (! pianoRollWidget->isVelocityLaneVisible());
        },
        { VimContext::PianoRoll }
    });

    actionRegistry.registerAction ({
        "pr.cc_lane", "Toggle CC Lane", "Piano Roll", "",
        [this]()
        {
            if (pianoRollWidget)
                pianoRollWidget->setCCLaneVisible (! pianoRollWidget->isCCLaneVisible());
        },
        { VimContext::PianoRoll }
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
            trackProcessors.add (nullptr);
            midiClipProcessors.add (processorPtr);
            trackNodes.add (node);
        }
        else
        {
            auto processor = std::make_unique<TrackProcessor> (transportController);
            auto* processorPtr = processor.get();

            // Load the first audio clip's file (skip MIDI clips)
            for (int c = 0; c < track.getNumClips(); ++c)
            {
                auto clipState = track.getClip (c);
                if (clipState.hasType (IDs::AUDIO_CLIP))
                {
                    AudioClip clip (clipState);
                    processorPtr->loadFile (clip.getSourceFile());
                    break;
                }
            }

            // Sync gain/pan/mute from model
            processorPtr->setGain (track.getVolume());
            processorPtr->setPan (track.getPan());
            processorPtr->setMuted (track.isMuted());

            auto node = audioEngine.addProcessor (std::move (processor));
            trackProcessors.add (processorPtr);
            midiClipProcessors.add (nullptr);
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

        // Push initial MIDI clip data if this is a MIDI track
        if (midiClipProcessors[i] != nullptr)
            syncMidiClipFromModel (i);
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

void AppController::syncMidiClipFromModel (int trackIndex)
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

    // Wire audio: TrackNode → Plugin1 → Plugin2 → ... → MixBus
    auto prevNode = trackNode;
    for (auto& pluginNode : enabledNodes)
    {
        audioEngine.connectNodes (prevNode->nodeID, 0, pluginNode->nodeID, 0);
        audioEngine.connectNodes (prevNode->nodeID, 1, pluginNode->nodeID, 1);
        prevNode = pluginNode;
    }

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

            // Save ref to old state so we can detach listeners after replacement
            auto oldState = project.getState();
            oldState.getChildWithName (IDs::STEP_SEQUENCER).removeListener (this);
            oldState.getChildWithName (IDs::TRACKS).removeListener (this);
            oldState.removeListener (arrangementWidget.get());
            oldState.removeListener (mixerWidget.get());
            oldState.removeListener (sequencerWidget.get());

            if (project.loadSessionFromDirectory (dir))
            {
                currentSessionDirectory = dir;
                project.getState().getChildWithName (IDs::TRACKS).addListener (this);
                project.getState().getChildWithName (IDs::STEP_SEQUENCER).addListener (this);
                project.getState().addListener (arrangementWidget.get());
                project.getState().addListener (mixerWidget.get());
                project.getState().addListener (sequencerWidget.get());
                rebuildAudioGraph();
                syncSequencerFromModel();
            }
            else
            {
                oldState.getChildWithName (IDs::TRACKS).addListener (this);
                oldState.getChildWithName (IDs::STEP_SEQUENCER).addListener (this);
                oldState.addListener (arrangementWidget.get());
                oldState.addListener (mixerWidget.get());
                oldState.addListener (sequencerWidget.get());

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

void AppController::addMidiTrack (const juce::String& name)
{
    auto trackState = project.addTrack (name);
    Track track (trackState);

    // Default 4-bar clip length
    double tempo = project.getTempo();
    double sr = project.getSampleRate();
    auto lengthInSamples = static_cast<int64_t> ((16.0 / tempo) * 60.0 * sr);

    track.addMidiClip (0, lengthInSamples);

    // Select new track and its first clip
    int newIndex = project.getNumTracks() - 1;
    arrangement.selectTrack (newIndex);
    vimContext.setSelectedClipIndex (0);

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

    // Tempo change — sync to sequencer and MIDI clip processors
    if (tree.hasType (IDs::PROJECT) && property == IDs::tempo)
    {
        if (sequencerProcessor != nullptr)
            sequencerProcessor->setTempo (project.getTempo());

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

    if (tree.hasType (IDs::STEP_SEQUENCER) || tree.hasType (IDs::STEP_PATTERN)
        || tree.hasType (IDs::STEP_ROW) || tree.hasType (IDs::STEP))
    {
        syncSequencerFromModel();
    }

    repaint();
}

void AppController::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
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

void AppController::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
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

// ─── VimEngine::Listener ─────────────────────────────────────

void AppController::vimModeChanged (VimEngine::Mode)
{
    repaint();
}

void AppController::vimContextChanged()
{
    updatePanelVisibility();

    // Propagate active context indicator
    auto panel = vimContext.getPanel();
    if (arrangementWidget)
        arrangementWidget->setActiveContext (panel == VimContext::Editor);
    if (mixerWidget)
    {
        mixerWidget->setActiveContext (panel == VimContext::Mixer);
        mixerWidget->setSelectedStripIndex (arrangement.getSelectedTrackIndex());
        mixerWidget->setMixerFocus (vimContext.getMixerFocus());
    }

    // Sync grid cursor to VimContext
    if (sequencerWidget)
    {
        // Sequencer widget handles cursor updates via VimContext internally
    }

    // When piano roll is closed, clear its clip data
    if (panel != VimContext::PianoRoll && pianoRollWidget && pianoRollWidget->isVisible())
    {
        pianoRollWidget->loadClip (juce::ValueTree());
    }
}

} // namespace ui
} // namespace dc
