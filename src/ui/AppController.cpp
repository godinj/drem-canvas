#include "AppController.h"
#include "engine/PluginProcessorNode.h"
#include "dc/plugins/PluginDescription.h"
#include "graphics/rendering/Canvas.h"
#include "model/Track.h"
#include "model/AudioClip.h"
#include "model/MidiClip.h"
#include "model/StepSequencer.h"
#include "platform/NativeDialogs.h"
#include "plugins/PluginEditorBridge.h"
#include "utils/UndoSystem.h"
#include "dc/foundation/assert.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

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

    project.getState().removeListener (this);

    stopTimer();
    midiEngine.shutdown();
    pluginWindowManager.closeAll();
    trackPluginChains.clear();
    meterTapProcessors.clear();
    meterTapNodes.clear();
    fallbackSynthNodes.clear();
    trackProcessors.clear();
    midiClipProcessors.clear();
    trackNodes.clear();
    sequencerProcessor = nullptr;
    sequencerNode = 0;
    mixBusNode = 0;
    audioEngine.shutdown();
}

void AppController::initialise()
{
    // Initialise audio engine with stereo I/O
    audioEngine.initialise (2, 2);
    transportController.setSampleRate (audioEngine.getSampleRate());

    // Create mix bus processor
    {
        auto proc = std::make_unique<MixBusProcessor> (transportController);
        mixBusProcessor = proc.get();
        mixBusNode = audioEngine.addProcessor (std::move (proc));
    }
    audioEngine.connectNodes (mixBusNode, 0,
                              audioEngine.getAudioOutputNodeId(), 0);
    audioEngine.connectNodes (mixBusNode, 1,
                              audioEngine.getAudioOutputNodeId(), 1);

    // Create step sequencer processor
    {
        auto proc = std::make_unique<StepSequencerProcessor> (transportController);
        sequencerProcessor = proc.get();
        sequencerNode = audioEngine.addProcessor (std::move (proc));
        audioEngine.connectNodes (sequencerNode, 0, mixBusNode, 0);
        audioEngine.connectNodes (sequencerNode, 1, mixBusNode, 1);
        sequencerProcessor->setTempo (project.getTempo());
        syncSequencerFromModel();
    }

    // Load plugin list
    pluginManager.loadPluginList (pluginManager.getDefaultPluginListFile());

    // Create vim engine
    vimEngine = std::make_unique<VimEngine> (project, transportController, arrangement, vimContext, gridSystem);
    vimEngine->addListener (this);

    // Wire :plugin command
    vimEngine->onPluginCommand = [this] (const std::string& pluginName)
    {
        auto& knownPlugins = pluginManager.getKnownPlugins();

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

    vimEngine->onCreateMidiTrack = [this] (const std::string& name) { addMidiTrack (name); };

    // Wire live MIDI keyboard output to selected MIDI track
    vimEngine->onLiveMidiNote = [this] (const dc::MidiMessage& msg)
    {
        int trackIndex = arrangement.getSelectedTrackIndex();
        if (trackIndex >= 0 && trackIndex < static_cast<int> (midiClipProcessors.size()))
            if (auto* proc = midiClipProcessors[static_cast<size_t> (trackIndex)])
                proc->injectLiveMidi (msg);
    };

    // Wire browser toggle (gp keybinding)
    vimEngine->onToggleBrowser = [this]() { toggleBrowser(); };

    // Wire mixer plugin navigation callbacks
    vimEngine->onMixerPluginOpen = [this] (int trackIdx, int pluginIndex)
    {
        if (trackIdx >= 0)
            openPluginEditor (trackIdx, pluginIndex);
    };

    vimEngine->onMixerPluginAdd = [this] (int trackIdx)
    {
        if (trackIdx >= 0)
            arrangement.selectTrack (trackIdx);
        toggleBrowser();
    };

    vimEngine->onMixerPluginRemove = [this] (int trackIdx, int pluginIndex)
    {
        if (trackIdx < 0)
            return; // master strip not supported in graphics path yet

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
            trackPluginChains[static_cast<size_t> (trackIdx)].erase (
                trackPluginChains[static_cast<size_t> (trackIdx)].begin() + pluginIndex);
            connectTrackPluginChain (trackIdx);
        }

        t.removePlugin (pluginIndex, &project.getUndoManager());

        if (mixerWidget != nullptr)
            mixerWidget->rebuildStrips();
    };

    vimEngine->onMixerPluginBypass = [this] (int trackIdx, int pluginIndex)
    {
        if (trackIdx < 0)
            return; // master strip not supported in graphics path yet

        auto trackState = project.getTrack (trackIdx);
        Track t (trackState);
        bool enabled = t.isPluginEnabled (pluginIndex);
        t.setPluginEnabled (pluginIndex, ! enabled, &project.getUndoManager());

        // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
        disconnectTrackPluginChain (trackIdx);
        connectTrackPluginChain (trackIdx);
    };

    vimEngine->onMixerPluginReorder = [this] (int trackIdx, int fromIndex, int toIndex)
    {
        if (trackIdx < 0)
            return; // master strip not supported in graphics path yet

        auto trackState = project.getTrack (trackIdx);
        Track t (trackState);

        if (trackIdx < static_cast<int> (trackPluginChains.size()))
        {
            // TODO: add suspendProcessing to dc::AudioGraph for thread-safe topology mutations
            disconnectTrackPluginChain (trackIdx);

            t.movePlugin (fromIndex, toIndex, &project.getUndoManager());
            std::swap (trackPluginChains[static_cast<size_t> (trackIdx)][static_cast<size_t> (fromIndex)],
                       trackPluginChains[static_cast<size_t> (trackIdx)][static_cast<size_t> (toIndex)]);

            connectTrackPluginChain (trackIdx);
        }
    };

    // Wire plugin view callbacks
    vimEngine->onOpenPluginView = [this] (int trackIdx, int pluginIdx)
    {
        if (trackIdx < 0 || trackIdx >= static_cast<int> (trackPluginChains.size()))
            return;
        if (pluginIdx < 0 || pluginIdx >= static_cast<int> (trackPluginChains[static_cast<size_t> (trackIdx)].size()))
            return;

        auto* plugin = trackPluginChains[static_cast<size_t> (trackIdx)][static_cast<size_t> (pluginIdx)].plugin;
        if (plugin == nullptr)
            return;

        auto trackState = project.getTrack (trackIdx);
        Track t (trackState);
        std::string name;
        std::string fileOrId;
        if (pluginIdx < t.getNumPlugins())
        {
            auto pluginState = t.getPlugin (pluginIdx);
            name = pluginState.getProperty (IDs::pluginName).getStringOr ("Plugin");
            fileOrId = pluginState.getProperty (IDs::pluginFileOrIdentifier).getStringOr ("");
        }

        if (pluginViewWidget)
            pluginViewWidget->setPlugin (plugin, name, fileOrId);
    };

    vimEngine->onClosePluginView = [this]
    {
        if (pluginViewWidget)
        {
            pluginViewWidget->endMouseDrag();
            pluginViewWidget->clearPlugin();
        }
    };

    vimEngine->onPluginViewRescan = [this]
    {
        if (pluginViewWidget)
            pluginViewWidget->forceSpatialRescan();
    };

    vimEngine->onPluginViewToggleDragAxis = [this]
    {
        if (pluginViewWidget)
        {
            pluginViewWidget->toggleDragAxis();
            vimContext.setPluginViewDragHorizontal (pluginViewWidget->isDragHorizontal());
        }
    };

    vimEngine->onPluginViewEndDrag = [this]
    {
        if (pluginViewWidget)
            pluginViewWidget->endMouseDrag();
    };

    vimEngine->onPluginViewToggleDragCenter = [this]
    {
        if (pluginViewWidget)
        {
            pluginViewWidget->toggleDragCenterOnReverse();
            vimContext.setPluginViewCenterOnReverse (pluginViewWidget->isDragCenterOnReverse());
        }
    };

    vimEngine->onPluginParamAdjust = [this] (int paramIndex, float delta)
    {
        int trackIdx = vimContext.getPluginViewTrackIndex();
        int pluginIdx = vimContext.getPluginViewPluginIndex();

        if (trackIdx < 0 || trackIdx >= static_cast<int> (trackPluginChains.size()))
            return;
        if (pluginIdx < 0 || pluginIdx >= static_cast<int> (trackPluginChains[static_cast<size_t> (trackIdx)].size()))
            return;

        auto* plugin = trackPluginChains[static_cast<size_t> (trackIdx)][static_cast<size_t> (pluginIdx)].plugin;
        if (plugin == nullptr) return;

        if (pluginViewWidget && pluginViewWidget->isSpatialMode())
        {
            // Spatial mode: paramIndex is a spatial index
            auto& results = pluginViewWidget->getSpatialResults();
            if (paramIndex < 0 || paramIndex >= static_cast<int> (results.size()))
                return;

            auto& info = results[static_cast<size_t> (paramIndex)];

            if (info.paramIndex >= 0 && info.paramIndex < plugin->getNumParameters())
            {
                // Mapped param: host API only
                float current = plugin->getParameterValue (info.paramIndex);
                float newVal = std::clamp (current + delta, 0.0f, 1.0f);
                plugin->setParameterValue (info.paramIndex, newVal);
            }
            else
            {
                // Unmapped param: synthetic micro-drag at centroid
                int pixelDelta = (std::abs (delta) > 0.02f) ? 8 : 2;
                if (delta < 0.0f)
                    pixelDelta = -pixelDelta;
                pluginViewWidget->applyMouseDrag (paramIndex, pixelDelta);
            }
        }
        else
        {
            // dc::PluginInstance param mode — all params are mapped
            if (paramIndex < 0 || paramIndex >= plugin->getNumParameters())
                return;

            float current = plugin->getParameterValue (paramIndex);
            float newVal = std::clamp (current + delta, 0.0f, 1.0f);
            plugin->setParameterValue (paramIndex, newVal);
        }
    };

    vimEngine->onQuerySpatialHintCount = [this]() -> int {
        if (! pluginViewWidget || ! pluginViewWidget->hasSpatialHints())
            return 0;
        return static_cast<int> (pluginViewWidget->getSpatialResults().size());
    };

    vimEngine->onResolveSpatialHint = [this] (int spatialIndex) -> int {
        if (! pluginViewWidget)
            return -1;
        auto& results = pluginViewWidget->getSpatialResults();
        if (spatialIndex < 0 || spatialIndex >= static_cast<int> (results.size()))
            return -1;
        auto& info = results[spatialIndex];
        dc_log ("[SpatialHint] resolved index=%d paramId=%u paramIndex=%d name=%s",
                spatialIndex, info.paramId, info.paramIndex, info.name.c_str());
        return info.paramIndex;
    };

    vimEngine->onQueryPluginParamCount = [this]() -> int {
        if (! pluginViewWidget)
            return 0;
        return pluginViewWidget->getNumParameters();
    };

    vimEngine->onPluginParamChanged = [this] (int paramIndex, float newValue)
    {
        int trackIdx = vimContext.getPluginViewTrackIndex();
        int pluginIdx = vimContext.getPluginViewPluginIndex();

        if (trackIdx < 0 || trackIdx >= static_cast<int> (trackPluginChains.size()))
            return;
        if (pluginIdx < 0 || pluginIdx >= static_cast<int> (trackPluginChains[static_cast<size_t> (trackIdx)].size()))
            return;

        auto* plugin = trackPluginChains[static_cast<size_t> (trackIdx)][static_cast<size_t> (pluginIdx)].plugin;
        if (plugin == nullptr) return;

        if (pluginViewWidget && pluginViewWidget->isSpatialMode())
        {
            // Spatial mode: paramIndex is a spatial index
            auto& results = pluginViewWidget->getSpatialResults();
            if (paramIndex < 0 || paramIndex >= static_cast<int> (results.size()))
                return;

            auto& info = results[static_cast<size_t> (paramIndex)];

            if (info.paramIndex >= 0 && info.paramIndex < plugin->getNumParameters())
            {
                // Mapped: set via host API
                plugin->setParameterValue (info.paramIndex,
                    std::clamp (newValue, 0.0f, 1.0f));
            }
            else
            {
                // Unmapped: sweep-and-position drag
                pluginViewWidget->applyAbsoluteDrag (paramIndex,
                    std::clamp (newValue, 0.0f, 1.0f));
            }
        }
        else
        {
            // dc::PluginInstance param mode — all params are mapped
            if (paramIndex < 0 || paramIndex >= plugin->getNumParameters())
                return;

            plugin->setParameterValue (paramIndex, std::clamp (newValue, 0.0f, 1.0f));
        }
    };

    // Wire plugin menu callbacks
    vimEngine->onPluginMenuMove = [this] (int delta)
    {
        if (browserWidget)
            browserWidget->moveSelection (delta);
    };

    vimEngine->onPluginMenuScroll = [this] (int direction)
    {
        if (browserWidget)
            browserWidget->scrollByHalfPage (direction);
    };

    vimEngine->onPluginMenuConfirm = [this]
    {
        if (browserWidget)
            browserWidget->confirmSelection();

        // Close browser after confirming
        browserVisible = false;
        if (browserWidget)
            browserWidget->setVisible (false);
        resized();
        repaint();
    };

    vimEngine->onPluginMenuCancel = [this]
    {
        browserVisible = false;
        if (browserWidget)
            browserWidget->setVisible (false);
        resized();
        repaint();
    };

    vimEngine->onPluginMenuFilter = [this] (const std::string& query)
    {
        if (browserWidget)
            browserWidget->setSearchFilter (query);
    };

    vimEngine->onPluginMenuClearFilter = [this]
    {
        if (browserWidget)
            browserWidget->clearSearchFilter();
    };

    // Wire piano roll open
    vimEngine->onOpenPianoRoll = [this] (const PropertyTree& clipState)
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

    vimEngine->onPianoRollDeleteSelected = [this] (char reg)
    {
        if (pianoRollWidget) pianoRollWidget->deleteSelectedNotes (reg);
    };

    vimEngine->onPianoRollCopy = [this] (char reg)
    {
        if (pianoRollWidget) pianoRollWidget->copySelectedNotes (reg);
    };

    vimEngine->onPianoRollPaste = [this] (char reg)
    {
        if (pianoRollWidget) pianoRollWidget->pasteNotes (reg);
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
        newRow = std::clamp (newRow, 0, 127);
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

        MidiClip clip (clipState);

        // Toggle: remove existing note at cursor, or add a new one
        for (int i = 0; i < clipState.getNumChildren(); ++i)
        {
            auto child = clipState.getChild (i);
            if (child.getType() == IDs::NOTE
                && static_cast<int> (child.getProperty (IDs::noteNumber).getIntOr (60)) == noteNumber
                && std::abs (child.getProperty (IDs::startBeat).getDoubleOr (0.0) - beat) < 0.001)
            {
                ScopedTransaction txn (project.getUndoSystem(), "Remove Note");
                clip.removeNote (i, &project.getUndoManager());
                return;
            }
        }

        ScopedTransaction txn (project.getUndoSystem(), "Add Note");
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
            pianoRollWidget->setPrNoteRow (std::clamp (noteRow, 0, 127));
    };

    // Initialise MIDI engine
    midiEngine.initialise();

    // Wire MIDI recording: when piano roll is open and recording,
    // incoming MIDI notes create notes in real-time
    midiEngine.onMidiMessage = [this] (const dc::MidiMessage& msg)
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
            int64_t clipStart = clipState.getProperty (IDs::startPosition).getIntOr (0);

            double relativeSamples = static_cast<double> (posSamples - clipStart);
            double relativeSeconds = relativeSamples / sr;
            double relativeBeat = relativeSeconds * tempo / 60.0;

            if (relativeBeat >= 0.0)
            {
                MidiClip clip (clipState);
                clip.addNote (msg.getNoteNumber(), relativeBeat, 0.25,
                              msg.getRawVelocity(), &project.getUndoManager());
            }
        }
    };

    // ─── Create UI widgets ───────────────────────────────

    // Transport bar
    transportBar = std::make_unique<TransportBarWidget> (transportController, tempoMap);

    addChild (transportBar.get());

    transportBar->onSaveSession   = [this]() { saveSession(); };
    transportBar->onLoadSession   = [this]() { loadSession(); };
    transportBar->onImportAudio   = [this]() { openFile(); };
    transportBar->onAudioSettings = [this]() { showAudioSettings(); };
    transportBar->onToggleBrowser = [this]() { toggleBrowser(); };

    // Vim status bar
    vimStatusBar = std::make_unique<VimStatusBarWidget> (*vimEngine, vimContext,
                                                          arrangement, transportController,
                                                          gridSystem);
    addChild (vimStatusBar.get());

    // Arrangement
    arrangementWidget = std::make_unique<ArrangementWidget> (project, transportController,
                                                               arrangement, vimContext,
                                                               tempoMap, gridSystem);
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

    browserWidget->onPluginSelected = [this] (const dc::PluginDescription& desc)
    {
        insertPluginOnTrack (arrangement.getSelectedTrackIndex(), desc);
    };

    // Plugin view (hidden initially, shown when entering PluginView panel)
    pluginViewWidget = std::make_unique<PluginViewWidget>();
    pluginViewWidget->setVisible (false);
    addChild (pluginViewWidget.get());

    if (nativeWindowHandle != nullptr)
        pluginViewWidget->setEditorBridge (PluginEditorBridge::create (nativeWindowHandle));

    // Virtual keyboard (hidden initially, shown when entering Keyboard mode)
    keyboardWidget = std::make_unique<VirtualKeyboardWidget> (vimEngine->getKeyboardState());
    keyboardWidget->setVisible (false);
    addChild (keyboardWidget.get());

    // Command palette (added last so it renders on top)
    commandPalette = std::make_unique<CommandPaletteWidget> (actionRegistry);
    commandPalette->setVisible (false);
    addChild (commandPalette.get());

    commandPalette->onDismiss = [this]() { dismissCommandPalette(); };

    // Wire command palette trigger
    vimEngine->onCommandPalette = [this]() { showCommandPalette(); };

    // Register all actions in the palette
    registerAllActions();

    // Load recent projects and populate palette
    recentProjects.load();
    refreshRecentProjectActions();

    // Register animating widgets
    if (renderer)
    {
        renderer->addAnimatingWidget (transportBar.get());
        renderer->addAnimatingWidget (vimStatusBar.get());
        renderer->addAnimatingWidget (arrangementWidget.get());
        renderer->addAnimatingWidget (pianoRollWidget.get());
        renderer->addAnimatingWidget (mixerWidget.get());
        renderer->addAnimatingWidget (keyboardWidget.get());
    }

    // Listen to model changes — register on the project root so we receive
    // all descendant notifications (TRACKS, TRACK, MIDI_CLIP, etc.)
    project.getState().addListener (this);

    // Sync tempo
    tempoMap.setTempo (project.getTempo());

    // Select first track
    if (arrangement.getNumTracks() > 0)
        arrangement.selectTrack (0);

    // Start meter polling timer (30 Hz)
    startTimerHz (30);

    resized();
}

void AppController::setRenderer (gfx::Renderer* r)
{
    renderer = r;
}

void AppController::setNativeWindowHandle (void* handle)
{
    nativeWindowHandle = handle;
    if (pluginViewWidget)
        pluginViewWidget->setEditorBridge (PluginEditorBridge::create (handle));
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
    float keyboardH = (vimEngine && vimEngine->getMode() == VimEngine::Keyboard) ? 80.0f : 0.0f;

    // Transport bar at top
    if (transportBar)
        transportBar->setBounds (0, 0, w, transportH);

    // Status bar at bottom
    if (vimStatusBar)
        vimStatusBar->setBounds (0, h - statusH, w, statusH);

    // Virtual keyboard strip (above status bar)
    if (keyboardWidget)
    {
        keyboardWidget->setVisible (keyboardH > 0);
        keyboardWidget->setBounds (0, h - statusH - keyboardH, w, keyboardH);
    }

    // Center area between transport and status bar + keyboard
    float centerX = 0;
    float centerY = transportH;
    float centerW = w - browserW;
    float centerH = h - transportH - statusH - keyboardH;

    // Browser on the right
    if (browserWidget && browserVisible)
        browserWidget->setBounds (w - browserW, centerY, browserW, centerH);

    // Arrangement/mixer split
    float arrangementH = centerH * splitRatio;
    float bottomH = centerH - arrangementH;

    if (arrangementWidget)
    {
        arrangementWidget->setVisible (true);
        arrangementWidget->setBounds (centerX, centerY, centerW, arrangementH);
    }

    // Bottom panel: mixer, sequencer, piano roll, or plugin view + mixer split
    auto currentPanel = vimContext.getPanel();
    bool showSequencer  = (currentPanel == VimContext::Sequencer);
    bool showPianoRoll  = (currentPanel == VimContext::PianoRoll);
    bool showPluginView = (currentPanel == VimContext::PluginView);

    float bottomY = centerY + arrangementH;

    bool pluginEnlarged = showPluginView && vimContext.isPluginViewEnlarged();

    if (showPluginView && pluginEnlarged)
    {
        // Enlarged: plugin view fills arrangement area, mixer full-width bottom
        if (arrangementWidget)
            arrangementWidget->setVisible (false);

        if (pluginViewWidget)
        {
            pluginViewWidget->setEnlarged (true);
            pluginViewWidget->setVisible (true);
            pluginViewWidget->setBounds (centerX, centerY, centerW, arrangementH);
            pluginViewWidget->updateEditorBounds();
        }

        if (mixerWidget)
        {
            mixerWidget->setVisible (true);
            mixerWidget->setBounds (centerX, bottomY, centerW, bottomH);
        }
    }
    else if (showPluginView)
    {
        // Split bottom: 55% plugin view (left), 45% mixer (right)
        float pluginViewW = centerW * 0.55f;
        float mixerW = centerW - pluginViewW;

        if (pluginViewWidget)
        {
            pluginViewWidget->setEnlarged (false);
            pluginViewWidget->setVisible (true);
            pluginViewWidget->setBounds (centerX, bottomY, pluginViewW, bottomH);
            pluginViewWidget->updateEditorBounds();
        }

        if (mixerWidget)
        {
            mixerWidget->setVisible (true);
            mixerWidget->setBounds (centerX + pluginViewW, bottomY, mixerW, bottomH);
        }
    }
    else
    {
        if (pluginViewWidget)
            pluginViewWidget->setVisible (false);

        if (mixerWidget)
        {
            mixerWidget->setVisible (! showSequencer && ! showPianoRoll);
            if (! showSequencer && ! showPianoRoll)
                mixerWidget->setBounds (centerX, bottomY, centerW, bottomH);
        }
    }

    if (sequencerWidget)
    {
        sequencerWidget->setVisible (showSequencer);
        if (showSequencer)
            sequencerWidget->setBounds (centerX, bottomY, centerW, bottomH);
    }

    if (pianoRollWidget)
    {
        pianoRollWidget->setVisible (showPianoRoll);
        if (showPianoRoll)
            pianoRollWidget->setBounds (centerX, bottomY, centerW, bottomH);
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

bool AppController::keyUp (const gfx::KeyEvent& e)
{
    // Route key-up through VimEngine for Keyboard mode note-off
    if (vimEngine && vimEngine->handleKeyUp (e))
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

void AppController::refreshRecentProjectActions()
{
    actionRegistry.removeActionsWithPrefix ("recent.");

    auto& entries = recentProjects.getEntries();
    for (size_t i = 0; i < entries.size(); ++i)
    {
        auto path = entries[i].path;
        actionRegistry.registerAction ({
            "recent." + std::to_string (i),
            entries[i].displayName,
            "Recent",
            "",
            [this, path]() { loadSessionFromDirectory (std::filesystem::path (path)); },
            {}
        });
    }
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

    actionRegistry.registerAction ({
        "mode.keyboard", "Toggle Virtual Keyboard", "Mode", "gk / Ctrl+K",
        [this]()
        {
            if (vimEngine->getMode() == VimEngine::Keyboard)
                vimEngine->exitKeyboardMode();
            else
                vimEngine->enterKeyboardMode();
        }, {}
    });

    // ─── View ────────────────────────────────────────────────
    actionRegistry.registerAction ({
        "view.toggle_browser", "Toggle Browser", "View", "gp",
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

    // Remove existing fallback synth nodes
    for (auto& nodeId : fallbackSynthNodes)
        if (nodeId != 0)
            audioEngine.removeProcessor (nodeId);
    fallbackSynthNodes.clear();

    // Remove existing track nodes
    for (auto& nodeId : trackNodes)
        if (nodeId != 0)
            audioEngine.removeProcessor (nodeId);

    trackProcessors.clear();
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
            if (track.getClip (c).getType() == IDs::MIDI_CLIP)
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

            auto nodeId = audioEngine.addProcessor (std::move (processor));
            trackProcessors.push_back (nullptr);
            midiClipProcessors.push_back (processorPtr);
            trackNodes.push_back (nodeId);
        }
        else
        {
            auto processor = std::make_unique<TrackProcessor> (transportController);
            auto* processorPtr = processor.get();

            // Load the first audio clip's file (skip MIDI clips)
            for (int c = 0; c < track.getNumClips(); ++c)
            {
                auto clipState = track.getClip (c);
                if (clipState.getType() == IDs::AUDIO_CLIP)
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

            auto nodeId = audioEngine.addProcessor (std::move (processor));
            trackProcessors.push_back (processorPtr);
            midiClipProcessors.push_back (nullptr);
            trackNodes.push_back (nodeId);
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

        // For MIDI tracks, create a fallback sine-wave synth so there is
        // always an instrument in the chain.  If the user has loaded a real
        // plugin the fallback is bypassed by connectTrackPluginChain().
        if (midiClipProcessors[i] != nullptr)
        {
            auto synth = std::make_unique<SimpleSynthProcessor>();
            auto synthNode = audioEngine.addProcessor (std::move (synth));
            fallbackSynthNodes.push_back (synthNode);
        }
        else
        {
            fallbackSynthNodes.push_back (0);
        }

        connectTrackPluginChain (i);

        // Push initial MIDI clip data if this is a MIDI track
        if (midiClipProcessors[i] != nullptr)
            syncMidiClipFromModel (i);
    }

    // Connect step sequencer MIDI to all MIDI tracks so sequencer
    // patterns reach the instrument plugins on those tracks.
    if (sequencerNode != 0)
    {
        for (int i = 0; i < static_cast<int> (trackNodes.size()); ++i)
        {
            if (midiClipProcessors[i] != nullptr)
            {
                audioEngine.connectNodes (sequencerNode,
                    -1,  // MIDI channel
                    trackNodes[static_cast<size_t> (i)],
                    -1);  // MIDI channel
            }
        }
    }

    // Rebuild UI views
    if (arrangementWidget != nullptr)
        arrangementWidget->rebuildTrackLanes();

    if (mixerWidget != nullptr)
        mixerWidget->rebuildStrips();
}

void AppController::syncTrackProcessorsFromModel()
{
    for (int i = 0; i < project.getNumTracks() && i < static_cast<int> (trackProcessors.size()); ++i)
    {
        auto trackState = project.getTrack (i);
        Track track (trackState);

        if (auto* processor = trackProcessors[i])
        {
            processor->setGain (track.getVolume());
            processor->setPan (track.getPan());
            processor->setMuted (track.isMuted());
        }
        else if (i < static_cast<int> (midiClipProcessors.size()))
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
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid() || sequencerProcessor == nullptr)
        return;

    StepSequencer seq (seqState);
    auto pattern = seq.getActivePattern();
    if (! pattern.isValid())
        return;

    StepSequencerProcessor::PatternSnapshot snapshot;
    snapshot.numRows      = seq.getNumRows();
    snapshot.numSteps     = static_cast<int> (pattern.getProperty (IDs::numSteps).getIntOr (16));
    snapshot.stepDivision = static_cast<int> (pattern.getProperty (IDs::stepDivision).getIntOr (4));
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
    if (trackIndex < 0 || trackIndex >= static_cast<int> (midiClipProcessors.size()))
        return;

    auto* midiProc = midiClipProcessors[static_cast<size_t> (trackIndex)];
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
        if (clipState.getType() != IDs::MIDI_CLIP)
            continue;

        MidiClip clip (clipState);
        int64_t clipStartSample = clip.getStartPosition();
        auto seq = clip.getMidiSequence();

        // Match note-on/off pairs and convert to absolute sample positions
        seq.updateMatchedPairs();

        for (int e = 0; e < seq.getNumEvents(); ++e)
        {
            const auto& event = seq.getEvent (e);
            const auto& msg = event.message;

            if (! msg.isNoteOn())
                continue;

            if (snapshot.numEvents >= MidiClipProcessor::MidiTrackSnapshot::maxEvents)
                break;

            // Timestamps are in beats
            double onBeat = event.timeInBeats;
            int64_t onSample = clipStartSample
                + static_cast<int64_t> (onBeat * 60.0 / currentTempo * sr);

            int64_t offSample = onSample;
            if (event.matchedPairIndex >= 0)
            {
                double offBeat = seq.getEvent (event.matchedPairIndex).timeInBeats;
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
            evt.velocity   = msg.getRawVelocity();
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
    if (trackIndex < 0 || trackIndex >= static_cast<int> (trackNodes.size())
        || trackIndex >= static_cast<int> (trackPluginChains.size()) || mixBusNode == 0)
        return;

    auto trackNodeId = trackNodes[static_cast<size_t> (trackIndex)];
    auto& chain = trackPluginChains[static_cast<size_t> (trackIndex)];
    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    bool isMidi = (trackIndex < static_cast<int> (midiClipProcessors.size()) && midiClipProcessors[static_cast<size_t> (trackIndex)] != nullptr);

    std::vector<NodeId> enabledNodes;
    for (int p = 0; p < static_cast<int> (chain.size()); ++p)
    {
        if (track.isPluginEnabled (p))
            enabledNodes.push_back (chain[static_cast<size_t> (p)].node);
    }

    // Determine the instrument node: prefer real plugins, fall back to built-in synth.
    // The fallback synth is used when no instrument plugin is loaded so that
    // MIDI tracks always produce audible output.
    bool hasInstrumentPlugin = false;
    for (auto pluginNodeId : enabledNodes)
    {
        if (auto* proc = audioEngine.getGraph().getNode (pluginNodeId))
        {
            if (proc->acceptsMidi())
            {
                hasInstrumentPlugin = true;
                break;
            }
        }
    }

    auto fallbackNodeId = (trackIndex < static_cast<int> (fallbackSynthNodes.size()))
                              ? fallbackSynthNodes[static_cast<size_t> (trackIndex)]
                              : NodeId (0);

    // Helper: route prevNode through the meter tap (if present) into MixBus
    auto connectToMixBusViaMeterTap = [&] (NodeId prevId)
    {
        if (trackIndex < static_cast<int> (meterTapNodes.size()) && meterTapNodes[static_cast<size_t> (trackIndex)] != 0)
        {
            auto tapId = meterTapNodes[static_cast<size_t> (trackIndex)];
            audioEngine.connectNodes (prevId, 0, tapId, 0);
            audioEngine.connectNodes (prevId, 1, tapId, 1);
            prevId = tapId;
        }
        audioEngine.connectNodes (prevId, 0, mixBusNode, 0);
        audioEngine.connectNodes (prevId, 1, mixBusNode, 1);
    };

    bool useFallback = isMidi && ! hasInstrumentPlugin && fallbackNodeId != 0;

    if (useFallback)
    {
        // MIDI track with no instrument plugin: route through fallback synth.
        //   MidiClipProcessor --MIDI--> SimpleSynth --audio--> MeterTap -> MixBus
        audioEngine.connectNodes (trackNodeId, -1, fallbackNodeId, -1);  // MIDI channel
        connectToMixBusViaMeterTap (fallbackNodeId);
    }
    else
    {
        // Wire audio: TrackNode -> Plugin1 -> Plugin2 -> ... -> MeterTap -> MixBus
        auto prevId = trackNodeId;
        for (auto pluginNodeId : enabledNodes)
        {
            audioEngine.connectNodes (prevId, 0, pluginNodeId, 0);
            audioEngine.connectNodes (prevId, 1, pluginNodeId, 1);
            prevId = pluginNodeId;
        }

        connectToMixBusViaMeterTap (prevId);

        // Wire MIDI through the plugin chain (for MIDI tracks with instrument plugins)
        auto prevMidiId = trackNodeId;
        for (auto pluginNodeId : enabledNodes)
        {
            audioEngine.connectNodes (prevMidiId, -1, pluginNodeId, -1);  // MIDI channel
            prevMidiId = pluginNodeId;
        }
    }
}

void AppController::disconnectTrackPluginChain (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= static_cast<int> (trackNodes.size())
        || trackIndex >= static_cast<int> (trackPluginChains.size()) || mixBusNode == 0)
        return;

    auto& graph = audioEngine.getGraph();
    auto trackNodeId = trackNodes[static_cast<size_t> (trackIndex)];
    auto& chain = trackPluginChains[static_cast<size_t> (trackIndex)];

    // Disconnect all connections from the track node
    graph.disconnectNode (trackNodeId);

    // Disconnect all connections from each plugin in the chain
    for (auto& info : chain)
    {
        if (info.node != 0)
            graph.disconnectNode (info.node);
    }

    // Remove connections from the meter tap node
    if (trackIndex < static_cast<int> (meterTapNodes.size()) && meterTapNodes[static_cast<size_t> (trackIndex)] != 0)
    {
        graph.disconnectNode (meterTapNodes[static_cast<size_t> (trackIndex)]);
    }

    // Disconnect fallback synth if present
    if (trackIndex < static_cast<int> (fallbackSynthNodes.size()) && fallbackSynthNodes[static_cast<size_t> (trackIndex)] != 0)
    {
        graph.disconnectNode (fallbackSynthNodes[static_cast<size_t> (trackIndex)]);
    }
}

void AppController::openPluginEditor (int trackIndex, int pluginIndex)
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

void AppController::captureAllPluginStates()
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
}

void AppController::insertPluginOnTrack (int trackIndex, const dc::PluginDescription& desc)
{
    if (trackIndex < 0 || trackIndex >= project.getNumTracks())
        return;

    auto trackState = project.getTrack (trackIndex);
    Track track (trackState);

    track.addPlugin (desc.name, "VST3",
                     desc.manufacturer,
                     0, desc.path.string(),
                     &project.getUndoManager());

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

// ─── Session management ──────────────────────────────────────

void AppController::saveSession()
{
    captureAllPluginStates();

    platform::NativeDialogs::showSavePanel ("Save Session", "Untitled Session",
        [this] (const std::string& path)
        {
            if (path.empty())
                return;

            std::filesystem::path dir (path);
            if (project.saveSessionToDirectory (path))
            {
                currentSessionDirectory = dir;
                recentProjects.addProject (dir.string());
                refreshRecentProjectActions();
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

            loadSessionFromDirectory (std::filesystem::path (path));
        });
}

void AppController::loadSessionFromDirectory (const std::filesystem::path& dir)
{
    if (! std::filesystem::is_directory (dir))
        return;

    // Save ref to old state so we can detach listeners after replacement
    auto oldState = project.getState();
    oldState.getChildWithType (IDs::STEP_SEQUENCER).removeListener (this);
    oldState.getChildWithType (IDs::TRACKS).removeListener (this);
    oldState.removeListener (arrangementWidget.get());
    oldState.removeListener (mixerWidget.get());
    oldState.removeListener (sequencerWidget.get());

    if (project.loadSessionFromDirectory (dir.string()))
    {
        currentSessionDirectory = dir;
        project.getState().getChildWithType (IDs::TRACKS).addListener (this);
        project.getState().getChildWithType (IDs::STEP_SEQUENCER).addListener (this);
        project.getState().addListener (arrangementWidget.get());
        project.getState().addListener (mixerWidget.get());
        project.getState().addListener (sequencerWidget.get());
        rebuildAudioGraph();
        syncSequencerFromModel();

        recentProjects.addProject (dir.string());
        refreshRecentProjectActions();
    }
    else
    {
        oldState.getChildWithType (IDs::TRACKS).addListener (this);
        oldState.getChildWithType (IDs::STEP_SEQUENCER).addListener (this);
        oldState.addListener (arrangementWidget.get());
        oldState.addListener (mixerWidget.get());
        oldState.addListener (sequencerWidget.get());

        auto path = dir.string();
        platform::NativeDialogs::showAlert ("Load Error",
            "Failed to load session from:\n" + path);
    }
}

void AppController::openFile()
{
    platform::NativeDialogs::showOpenPanel ("Select an audio file...",
        { "wav", "aiff", "mp3", "flac", "ogg" },
        [this] (const std::string& path)
        {
            if (path.empty())
                return;
            std::filesystem::path file (path);
            if (std::filesystem::exists (file) && std::filesystem::is_regular_file (file))
                addTrackFromFile (file);
        });
}

void AppController::addTrackFromFile (const std::filesystem::path& file)
{
    auto trackName = file.stem().string();
    auto trackState = project.addTrack (trackName);

    auto tempProcessor = std::make_unique<TrackProcessor> (transportController);
    if (tempProcessor->loadFile (file))
    {
        auto length = tempProcessor->getFileLengthInSamples();
        Track track (trackState);
        track.addAudioClip (file.string(), 0, length);
    }

    rebuildAudioGraph();
}

void AppController::addMidiTrack (const std::string& name)
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
    auto deviceName = audioEngine.getCurrentDeviceName();
    auto msg = juce::String ("Audio Device: ") + juce::String (deviceName)
             + "\nSample Rate: " + juce::String (audioEngine.getSampleRate()) + " Hz"
             + "\nBuffer Size: " + juce::String (audioEngine.getBufferSize()) + " samples";

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                            "Audio Settings", msg);
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

    if (browserVisible)
    {
        // Enter plugin menu mode and select first item
        if (vimEngine)
            vimEngine->enterPluginMenuMode();
        if (browserWidget && browserWidget->getSelectedPluginIndex() < 0
            && browserWidget->getNumPlugins() > 0)
            browserWidget->selectPlugin (0);
    }
    else
    {
        if (browserWidget)
            browserWidget->clearSearchFilter();
        // Return to normal mode if we were in plugin menu
        if (vimEngine && vimEngine->getMode() == VimEngine::PluginMenu)
            vimEngine->enterNormalMode();
    }
}

/// ─── PropertyTree::Listener ──────────────────────────────────

void AppController::propertyChanged (PropertyTree& tree, PropertyId property)
{
    if (tree.getType() == IDs::TRACK)
    {
        if (property == IDs::volume || property == IDs::pan || property == IDs::mute)
            syncTrackProcessorsFromModel();
    }

    // Tempo change — sync to sequencer and MIDI clip processors
    if (tree.getType() == IDs::PROJECT && property == IDs::tempo)
    {
        if (sequencerProcessor != nullptr)
            sequencerProcessor->setTempo (project.getTempo());

        // Re-sync all MIDI tracks (beat→sample conversion depends on tempo)
        for (int i = 0; i < static_cast<int> (midiClipProcessors.size()); ++i)
        {
            if (midiClipProcessors[i] != nullptr)
            {
                midiClipProcessors[i]->setTempo (project.getTempo());
                syncMidiClipFromModel (i);
            }
        }
    }

    // MIDI clip property changed (e.g. midiData, startPosition, length)
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

    if (tree.getType() == IDs::STEP_SEQUENCER || tree.getType() == IDs::STEP_PATTERN
        || tree.getType() == IDs::STEP_ROW || tree.getType() == IDs::STEP)
    {
        syncSequencerFromModel();
    }

    repaint();
}

void AppController::childAdded (PropertyTree& parent, PropertyTree& child)
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

    if (parent.getType() == IDs::STEP_SEQUENCER || parent.getType() == IDs::STEP_PATTERN
        || parent.getType() == IDs::STEP_ROW)
        syncSequencerFromModel();
}

void AppController::childRemoved (PropertyTree& parent, PropertyTree& child, int)
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

    if (parent.getType() == IDs::STEP_SEQUENCER || parent.getType() == IDs::STEP_PATTERN
        || parent.getType() == IDs::STEP_ROW)
        syncSequencerFromModel();
}

// ─── VimEngine::Listener ─────────────────────────────────────

void AppController::vimModeChanged (VimEngine::Mode)
{
    resized();
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

        if (vimContext.isMasterStripSelected())
            mixerWidget->setSelectedStripIndex (project.getNumTracks());
        else
            mixerWidget->setSelectedStripIndex (arrangement.getSelectedTrackIndex());

        mixerWidget->setMixerFocus (vimContext.getMixerFocus());

        if (vimContext.getMixerFocus() == VimContext::FocusPlugins)
            mixerWidget->setSelectedPluginSlot (vimContext.getSelectedPluginSlot());
        else
            mixerWidget->setSelectedPluginSlot (-1);
    }

    // Propagate plugin view state
    if (pluginViewWidget)
    {
        pluginViewWidget->setActiveContext (panel == VimContext::PluginView);

        if (panel == VimContext::PluginView)
        {
            // Clamp selected param to valid range
            int numParams = pluginViewWidget->getNumParameters();
            if (numParams > 0 && vimContext.getSelectedParamIndex() >= numParams)
                vimContext.setSelectedParamIndex (numParams - 1);

            pluginViewWidget->setSelectedParamIndex (vimContext.getSelectedParamIndex());
            pluginViewWidget->setHintMode (vimContext.getHintMode());
            pluginViewWidget->setHintBuffer (vimContext.getHintBuffer());
            pluginViewWidget->setNumberEntryActive (vimContext.isNumberEntryActive());
            pluginViewWidget->setNumberBuffer (vimContext.getNumberBuffer());
        }
    }

    // Sync grid cursor to VimContext
    if (sequencerWidget)
    {
        // Sequencer widget handles cursor updates via VimContext internally
    }

    // When piano roll is closed, clear its clip data
    if (panel != VimContext::PianoRoll && pianoRollWidget && pianoRollWidget->isVisible())
    {
        pianoRollWidget->loadClip (PropertyTree());
    }

    // When plugin view is closed, clear its plugin data
    if (panel != VimContext::PluginView && pluginViewWidget && pluginViewWidget->isVisible())
    {
        pluginViewWidget->clearPlugin();
    }
}

void AppController::timerCallback()
{
    messageQueue.processAll();

    if (! mixerWidget)
        return;

    auto& strips = mixerWidget->getStrips();

    // Convert linear amplitude to dB for MeterWidget (which expects dB)
    auto linearToDb = [] (float linear) -> float
    {
        if (linear <= 0.0f) return -60.0f;
        float db = 20.0f * std::log10 (linear);
        return std::max (db, -60.0f);
    };

    // Push track meter levels
    for (int i = 0; i < static_cast<int> (strips.size()) && i < static_cast<int> (meterTapProcessors.size()); ++i)
    {
        if (auto* tap = meterTapProcessors[i])
        {
            float leftDb  = linearToDb (tap->getPeakLevelLeft());
            float rightDb = linearToDb (tap->getPeakLevelRight());
            strips[static_cast<size_t> (i)]->getMeter().setLevel (leftDb, rightDb);
        }
    }

    // Push master meter levels from mix bus
    auto* masterStrip = mixerWidget->getMasterStrip();
    if (masterStrip != nullptr && mixBusProcessor != nullptr)
    {
        float leftDb  = linearToDb (mixBusProcessor->getPeakLevelLeft());
        float rightDb = linearToDb (mixBusProcessor->getPeakLevelRight());
        masterStrip->getMeter().setLevel (leftDb, rightDb);
    }
}

} // namespace ui
} // namespace dc
