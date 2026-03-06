#include "VimEngine.h"
#include "model/AudioClip.h"
#include "model/MidiClip.h"
#include "model/Clipboard.h"
#include "utils/UndoSystem.h"
#include "dc/foundation/time.h"
#include "dc/foundation/string_utils.h"
#include <vector>
#include <algorithm>
#include <limits>
#include <filesystem>
namespace dc
{

static bool isEscapeOrCtrlC (const dc::KeyPress& key)
{
    if (key == dc::KeyCode::Escape)
        return true;

    if (key.control)
    {
        auto c = key.getTextCharacter();
        // Ctrl-C: character may be ETX (3), 'c', or 'C'
        if (c == 3 || c == 'c' || c == 'C')
            return true;
    }

    return false;
}

VimEngine::VimEngine (Project& p, TransportController& t,
                      Arrangement& a, VimContext& c,
                      GridSystem& gs)
    : project (p), transport (t), arrangement (a), context (c), gridSystem (gs)
{
}

// consumeRegister() now delegated to grammar.consumeRegister()

void VimEngine::setMode (Mode m)
{
    mode = m;
    listeners.call ([m](Listener& l) { l.vimModeChanged (m); });
}

void VimEngine::registerAdapter (std::unique_ptr<ContextAdapter> adapter)
{
    int panel = static_cast<int> (adapter->getPanel());
    adapters[panel] = std::move (adapter);
}

ContextAdapter* VimEngine::getActiveAdapter() const
{
    auto it = adapters.find (static_cast<int> (context.getPanel()));
    return (it != adapters.end()) ? it->second.get() : nullptr;
}

ContextAdapter* VimEngine::getAdapter (VimContext::Panel panel) const
{
    auto it = adapters.find (static_cast<int> (panel));
    return (it != adapters.end()) ? it->second.get() : nullptr;
}

bool VimEngine::dispatch (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // 1. Global keymap bindings (Ctrl+P, etc.) — check before mode dispatch
    if (key.control && keyChar == 'p')
    {
        actionRegistry.executeAction ("command_palette");
        keymap.resetFeed();
        return true;
    }

    // 2. Mode-based dispatch (Keyboard, PluginMenu, Command stay internal)
    if (mode == Keyboard)
        return handleKeyboardKey (key);

    if (mode == PluginMenu)
        return handlePluginMenuKey (key);

    if (mode == Command)
        return handleCommandKey (key);

    // 3. Visual modes
    if (mode == Visual)
    {
        // Delegate to adapter if available for current panel
        auto* adapter = getActiveAdapter();
        if (adapter)
            return adapter->handleVisualKey (key);
        return handleVisualKey (key);
    }

    if (mode == VisualLine)
    {
        auto* adapter = getActiveAdapter();
        if (adapter)
            return adapter->handleVisualLineKey (key);
        return handleVisualLineKey (key);
    }

    // 4. Normal mode dispatch
    if (mode == Normal)
        return handleNormalKey (key);

    // 5. Insert mode
    return handleInsertKey (key);
}

void VimEngine::loadDefaultKeymap()
{
    // Try to find the default keymap relative to the executable
    // or in a known config location
    std::string paths[] = {
        "config/default_keymap.yaml",
        "../config/default_keymap.yaml",
        "../share/drem-canvas/default_keymap.yaml"
    };

    for (auto& path : paths)
    {
        if (std::filesystem::exists (path))
        {
            keymap.loadFromYAML (path);
            return;
        }
    }
}

void VimEngine::loadUserKeymap (const std::string& path)
{
    keymap.overlayFromYAML (path);
}

bool VimEngine::handleKeyEvent (const gfx::KeyEvent& event)
{
    dc::KeyCode code = dc::KeyCode::Unknown;

    switch (event.keyCode)
    {
        case 0x35: code = dc::KeyCode::Escape;     break;
        case 0x24: code = dc::KeyCode::Return;      break;
        case 0x30: code = dc::KeyCode::Tab;          break;
        case 0x31: code = dc::KeyCode::Space;        break;
        case 0x33: code = dc::KeyCode::Backspace;    break;
        case 0x7E: code = dc::KeyCode::UpArrow;      break;
        case 0x7D: code = dc::KeyCode::DownArrow;    break;
        case 0x7B: code = dc::KeyCode::LeftArrow;    break;
        case 0x7C: code = dc::KeyCode::RightArrow;   break;
        case 0x75: code = dc::KeyCode::Delete;       break;
        default:
            code = static_cast<dc::KeyCode> (static_cast<int> (event.character));
            break;
    }

    auto textChar = event.unmodifiedCharacter ? event.unmodifiedCharacter : event.character;

    dc::KeyPress key { code, textChar, event.shift, event.control, event.alt, event.command };
    return dispatch (key);
}

bool VimEngine::handleInsertKey (const dc::KeyPress& key)
{
    if (isEscapeOrCtrlC (key))
    {
        enterNormalMode();
        return true;
    }

    return false;
}

// ── Normal-mode phased dispatch ─────────────────────────────────────────────

bool VimEngine::handleNormalKey (const dc::KeyPress& key)
{
    // gp/gk — global g-prefix commands (work from any panel context)
    if (grammar.hasPendingKey() && grammar.getPendingKey() == 'g')
    {
        auto kc = key.getTextCharacter();
        if (kc == 'p')
        {
            grammar.clearPendingKey();
            actionRegistry.executeAction ("view.toggle_browser");
            return true;
        }
        if (kc == 'k')
        {
            grammar.clearPendingKey();
            enterKeyboardMode();
            return true;
        }
    }

    // Dispatch to panel-specific handlers
    if (context.getPanel() == VimContext::PianoRoll)
        return handlePianoRollNormalKey (key);

    if (context.getPanel() == VimContext::Sequencer)
        return handleSequencerNormalKey (key);

    if (context.getPanel() == VimContext::Mixer)
        return handleMixerNormalKey (key);

    if (context.getPanel() == VimContext::PluginView)
        return handlePluginViewNormalKey (key);

    auto keyChar = key.getTextCharacter();

    // Ctrl+K enters keyboard mode (from any panel context)
    if (key.control && (keyChar == 'k' || keyChar == 'K'))
    {
        enterKeyboardMode();
        return true;
    }

    // Phase: Escape / Ctrl-C cancels all pending state
    if (isEscapeOrCtrlC (key))
    {
        grammar.reset();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Phase: Grammar (replaces inline count/operator/motion handling)
    auto result = grammar.feed (keyChar, key.shift, key.control, key.alt, key.command);

    // Check for EditorAdapter — if present, delegate grammar results to it
    auto* editorAdapter = getActiveAdapter();

    switch (result.type)
    {
        case VimGrammar::ParseResult::Incomplete:
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;

        case VimGrammar::ParseResult::Motion:
            if (editorAdapter)
                editorAdapter->executeMotion (result.motionKey, result.count);
            else
                executeMotion (result.motionKey, result.count);
            return true;

        case VimGrammar::ParseResult::OperatorMotion:
        {
            if (editorAdapter)
            {
                auto aRange = editorAdapter->resolveMotion (result.motionKey, result.count);
                if (aRange.valid)
                    editorAdapter->executeOperator (result.op, aRange, result.reg);
            }
            else
            {
                auto range = resolveMotion (result.motionKey, result.count);
                if (range.valid)
                    executeOperator (static_cast<Operator> (result.op), range);
            }
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;
        }

        case VimGrammar::ParseResult::LinewiseOperator:
        {
            if (editorAdapter)
            {
                auto aRange = editorAdapter->resolveLinewiseMotion (result.count);
                editorAdapter->executeOperator (result.op, aRange, result.reg);
            }
            else
            {
                auto range = resolveLinewiseMotion (result.count);
                executeOperator (static_cast<Operator> (result.op), range);
            }
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;
        }

        case VimGrammar::ParseResult::NoMatch:
            break; // fall through to single-key actions
    }

    // Ctrl+L toggles cycle (before single-key actions)
    if (key.control && (keyChar == 'l' || keyChar == 'L' || keyChar == 12))
    { toggleCycle(); return true; }

    // Phase 6: Single-key actions (NoMatch from grammar)
    if (keyChar == 'x')
    {
        deleteSelectedRegions();
        return true;
    }

    if (keyChar == 'p')
    {
        pasteAfterPlayhead();
        return true;
    }

    if (keyChar == 'P')
    {
        pasteBeforePlayhead();
        return true;
    }

    if (keyChar == 'D')
    {
        duplicateSelectedClip();
        return true;
    }

    // Phase 7: Non-count actions

    // Visual modes (Editor panel only)
    if (keyChar == 'v' && context.getPanel() == VimContext::Editor)
    {
        auto* adapter = getActiveAdapter();
        if (adapter)
            adapter->enterVisualMode();
        else
            enterVisualMode();
        return true;
    }
    if (keyChar == 'V' && context.getPanel() == VimContext::Editor)
    {
        auto* adapter = getActiveAdapter();
        if (adapter)
            adapter->enterVisualLineMode();
        else
            enterVisualLineMode();
        return true;
    }

    if (keyChar == 's') { splitRegionAtPlayhead(); return true; }

    // Undo/redo
    if (keyChar == 'u' || (key.control && keyChar == 'z'))
    {
        project.getUndoSystem().undo();
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }
    if (keyChar == 'r' && key.control)
    {
        project.getUndoSystem().redo();
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Track state
    if (keyChar == 'M') { toggleMute();      return true; }
    if (keyChar == 'S') { toggleSolo();      return true; }
    if (keyChar == 'r') { toggleRecordArm(); return true; }

    // Grid division change
    if (keyChar == '[')
    {
        gridSystem.adjustGridDivision (-1);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            context.setGridCursorPosition (gridSystem.snapFloor (context.getGridCursorPosition(), sr));
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }
    if (keyChar == ']')
    {
        gridSystem.adjustGridDivision (1);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            context.setGridCursorPosition (gridSystem.snapFloor (context.getGridCursorPosition(), sr));
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Mode switch
    if (keyChar == 'i') { enterInsertMode(); return true; }

    // Transport
    if (key == dc::KeyCode::Space) { togglePlayStop(); return true; }

    // Panel
    if (key == dc::KeyCode::Tab) { cycleFocusPanel(); return true; }

    // Open item (stub)
    if (key == dc::KeyCode::Return) { openFocusedItem(); return true; }

    // Command mode
    if (keyChar == ':')
    {
        mode = Command;
        commandBuffer.clear();
        listeners.call ([](Listener& l) { l.vimModeChanged (Command); });
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    return false;
}

// ── Navigation ──────────────────────────────────────────────────────────────

void VimEngine::moveSelectionUp()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx > 0)
    {
        arrangement.selectTrack (idx - 1);
        // Preserve grid cursor position (don't reset to 0)
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

void VimEngine::moveSelectionDown()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < arrangement.getNumTracks() - 1)
    {
        arrangement.selectTrack (idx + 1);
        // Preserve grid cursor position (don't reset to 0)
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

void VimEngine::moveSelectionLeft()
{
    double sr = transport.getSampleRate();
    if (sr <= 0.0) return;

    int64_t pos = context.getGridCursorPosition();
    int64_t newPos = gridSystem.moveByGridUnits (pos, -1, sr);
    context.setGridCursorPosition (newPos);
    updateClipIndexFromGridCursor();
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::moveSelectionRight()
{
    double sr = transport.getSampleRate();
    if (sr <= 0.0) return;

    int64_t pos = context.getGridCursorPosition();
    int64_t newPos = gridSystem.moveByGridUnits (pos, 1, sr);
    context.setGridCursorPosition (newPos);
    updateClipIndexFromGridCursor();
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::updateClipIndexFromGridCursor()
{
    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
    {
        context.setSelectedClipIndex (-1);
        return;
    }

    Track track = arrangement.getTrack (trackIdx);
    int64_t cursorPos = context.getGridCursorPosition();

    for (int i = 0; i < track.getNumClips(); ++i)
    {
        auto clipState = track.getClip (i);
        auto start = clipState.getProperty (IDs::startPosition).getIntOr (0);
        auto length = clipState.getProperty (IDs::length).getIntOr (0);

        if (cursorPos >= start && cursorPos < start + length)
        {
            context.setSelectedClipIndex (i);
            return;
        }
    }

    context.setSelectedClipIndex (-1);
}

// ── Track jumps ─────────────────────────────────────────────────────────────

void VimEngine::jumpToFirstTrack()
{
    if (arrangement.getNumTracks() > 0)
    {
        arrangement.selectTrack (0);
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

void VimEngine::jumpToLastTrack()
{
    int count = arrangement.getNumTracks();
    if (count > 0)
    {
        arrangement.selectTrack (count - 1);
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

// ── Transport ───────────────────────────────────────────────────────────────

void VimEngine::jumpToSessionStart()
{
    transport.setPositionInSamples (0);
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::jumpToSessionEnd()
{
    int64_t maxEnd = 0;

    for (int i = 0; i < arrangement.getNumTracks(); ++i)
    {
        Track track = arrangement.getTrack (i);

        for (int c = 0; c < track.getNumClips(); ++c)
        {
            auto clipState = track.getClip (c);
            auto start = clipState.getProperty (IDs::startPosition).getIntOr (0);
            auto length = clipState.getProperty (IDs::length).getIntOr (0);
            maxEnd = std::max (maxEnd, start + length);
        }
    }

    transport.setPositionInSamples (maxEnd);
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::togglePlayStop()
{
    transport.togglePlayStop();
}

void VimEngine::toggleCycle()
{
    project.setCycleEnabled (! project.getCycleEnabled());
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::setCycleToGridVisual()
{
    auto& gvs = context.getGridVisualSelection();
    if (gvs.active)
    {
        int64_t start = std::min (gvs.startPos, gvs.endPos);
        int64_t end = std::max (gvs.startPos, gvs.endPos);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            end += gridSystem.getGridUnitInSamples (sr);
        project.setCycleStart (start);
        project.setCycleEnd (end);
        project.setCycleEnabled (true);
    }
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

// ── Clip operations ─────────────────────────────────────────────────────────

void VimEngine::deleteSelectedRegions()
{
    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (trackIdx);
    int clipIdx = context.getSelectedClipIndex();

    if (clipIdx >= 0 && clipIdx < track.getNumClips())
    {
        // Yank before delete (Vim semantics: x always yanks)
        char reg = grammar.consumeRegister();
        std::vector<Clipboard::ClipEntry> entries;
        entries.push_back ({ track.getClip (clipIdx), 0, 0 });
        project.getClipboard().storeClips (reg, entries, false, false);

        ScopedTransaction txn (project.getUndoSystem(), "Delete Clip");
        track.removeClip (clipIdx, &project.getUndoManager());

        if (clipIdx >= track.getNumClips() && track.getNumClips() > 0)
            context.setSelectedClipIndex (track.getNumClips() - 1);

        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

void VimEngine::yankSelectedRegions()
{
    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (trackIdx);
    int clipIdx = context.getSelectedClipIndex();

    if (clipIdx >= 0 && clipIdx < track.getNumClips())
    {
        char reg = grammar.consumeRegister();
        std::vector<Clipboard::ClipEntry> entries;
        entries.push_back ({ track.getClip (clipIdx), 0, 0 });
        project.getClipboard().storeClips (reg, entries, false, true);
    }
}

// Carve a gap [gapStart, gapEnd) in existing clips on the given track,
// splitting any clip that overlaps those boundaries.
static void carveGap (Track& track, int64_t gapStart, int64_t gapEnd, dc::UndoManager& um)
{
    std::vector<PropertyTree> newClips;

    for (int c = track.getNumClips() - 1; c >= 0; --c)
    {
        auto clip = track.getClip (c);
        auto clipStart  = clip.getProperty (IDs::startPosition).getIntOr (0);
        auto clipLength = clip.getProperty (IDs::length).getIntOr (0);
        auto clipEnd    = clipStart + clipLength;

        // Skip non-overlapping clips
        if (clipStart >= gapEnd || clipEnd <= gapStart)
            continue;

        bool keepLeft  = clipStart < gapStart;
        bool keepRight = clipEnd > gapEnd;

        if (! keepLeft && ! keepRight)
        {
            track.removeClip (c, &um);
        }
        else if (keepLeft && keepRight)
        {
            auto origTrimStart = clip.getProperty (IDs::trimStart).getIntOr (0);
            int64_t leftLength = gapStart - clipStart;
            clip.setProperty (IDs::length, Variant (leftLength), &um);

            auto rightClip = clip.createDeepCopy();
            int64_t rightOffset = gapEnd - clipStart;
            rightClip.setProperty (IDs::startPosition, Variant (gapEnd), nullptr);
            rightClip.setProperty (IDs::length, Variant (clipEnd - gapEnd), nullptr);
            rightClip.setProperty (IDs::trimStart, Variant (origTrimStart + rightOffset), nullptr);
            newClips.push_back (rightClip);
        }
        else if (keepLeft)
        {
            int64_t leftLength = gapStart - clipStart;
            clip.setProperty (IDs::length, Variant (leftLength), &um);
        }
        else // keepRight
        {
            auto origTrimStart = clip.getProperty (IDs::trimStart).getIntOr (0);
            int64_t rightOffset = gapEnd - clipStart;
            clip.setProperty (IDs::startPosition, Variant (gapEnd), &um);
            clip.setProperty (IDs::length, Variant (clipEnd - gapEnd), &um);
            clip.setProperty (IDs::trimStart, Variant (origTrimStart + rightOffset), &um);
        }
    }

    for (auto& nc : newClips)
        track.getState().addChild (nc, -1, &um);
}

void VimEngine::pasteAfterPlayhead()
{
    char reg = grammar.consumeRegister();
    auto& entry = project.getClipboard().get (reg);
    if (! entry.hasClips())
        return;

    int baseTrack = arrangement.getSelectedTrackIndex();
    if (baseTrack < 0 || baseTrack >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Paste Clip");
    auto& um = project.getUndoManager();
    int64_t pastePos = context.getGridCursorPosition();

    for (auto& clip : entry.clipEntries)
    {
        int targetTrack = baseTrack + clip.trackOffset;
        if (targetTrack < 0 || targetTrack >= arrangement.getNumTracks())
            continue;

        Track track = arrangement.getTrack (targetTrack);
        auto clipData = clip.clipData.createDeepCopy();
        int64_t finalPos = pastePos + clip.timeOffset;
        auto pasteLen = clipData.getProperty (IDs::length).getIntOr (0);

        carveGap (track, finalPos, finalPos + pasteLen, um);

        clipData.setProperty (IDs::startPosition,
                              Variant (finalPos), &um);
        track.getState().addChild (clipData, -1, &um);
    }

    updateClipIndexFromGridCursor();
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::pasteBeforePlayhead()
{
    char reg = grammar.consumeRegister();
    auto& regEntry = project.getClipboard().get (reg);
    if (! regEntry.hasClips())
        return;

    int baseTrack = arrangement.getSelectedTrackIndex();
    if (baseTrack < 0 || baseTrack >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Paste Clip");
    auto& um = project.getUndoManager();

    // Find the total extent so we can place everything before the cursor
    int64_t maxEnd = 0;
    for (auto& clip : regEntry.clipEntries)
    {
        auto len = clip.clipData.getProperty (IDs::length).getIntOr (0);
        maxEnd = std::max (maxEnd, clip.timeOffset + len);
    }

    int64_t pasteBase = context.getGridCursorPosition() - maxEnd;
    if (pasteBase < 0) pasteBase = 0;

    for (auto& clip : regEntry.clipEntries)
    {
        int targetTrack = baseTrack + clip.trackOffset;
        if (targetTrack < 0 || targetTrack >= arrangement.getNumTracks())
            continue;

        Track track = arrangement.getTrack (targetTrack);
        auto clipData = clip.clipData.createDeepCopy();
        int64_t finalPos = pasteBase + clip.timeOffset;
        auto pasteLen = clipData.getProperty (IDs::length).getIntOr (0);

        carveGap (track, finalPos, finalPos + pasteLen, um);

        clipData.setProperty (IDs::startPosition,
                              Variant (finalPos), &um);
        track.getState().addChild (clipData, -1, &um);
    }

    updateClipIndexFromGridCursor();
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::splitRegionAtPlayhead()
{
    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (trackIdx);
    int clipIdx = context.getSelectedClipIndex();

    if (clipIdx < 0 || clipIdx >= track.getNumClips())
        return;

    auto clipState = track.getClip (clipIdx);
    auto clipStart  = clipState.getProperty (IDs::startPosition).getIntOr (0);
    auto clipLength = clipState.getProperty (IDs::length).getIntOr (0);
    auto playhead   = transport.getPositionInSamples();

    if (playhead <= clipStart || playhead >= clipStart + clipLength)
        return;

    auto splitOffset = playhead - clipStart;
    ScopedTransaction txn (project.getUndoSystem(), "Split Clip");
    auto& um = project.getUndoManager();

    clipState.setProperty (IDs::length, Variant (splitOffset), &um);
    clipState.setProperty (IDs::trimEnd,
        Variant (clipState.getProperty (IDs::trimStart).getIntOr (0)
            + splitOffset), &um);

    auto newClip = clipState.createDeepCopy();
    newClip.setProperty (IDs::startPosition, Variant (playhead), &um);
    newClip.setProperty (IDs::length, Variant (clipLength - splitOffset), &um);
    newClip.setProperty (IDs::trimStart,
        Variant (clipState.getProperty (IDs::trimStart).getIntOr (0)
            + splitOffset), &um);

    track.getState().addChild (newClip, -1, &um);
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::duplicateSelectedClip()
{
    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (trackIdx);
    int clipIdx = context.getSelectedClipIndex();

    if (clipIdx < 0 || clipIdx >= track.getNumClips())
        return;

    auto clipState = track.getClip (clipIdx);
    auto startPos = clipState.getProperty (IDs::startPosition).getIntOr (0);
    auto length   = clipState.getProperty (IDs::length).getIntOr (0);

    ScopedTransaction txn (project.getUndoSystem(), "Duplicate Clip");
    auto& um = project.getUndoManager();

    auto newClip = clipState.createDeepCopy();
    newClip.setProperty (IDs::startPosition, Variant (startPos + length), &um);

    track.getState().addChild (newClip, -1, &um);

    context.setSelectedClipIndex (track.getNumClips() - 1);
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

// ── Track state ─────────────────────────────────────────────────────────────

void VimEngine::toggleMute()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Mute");
    Track track = arrangement.getTrack (idx);
    track.setMuted (! track.isMuted(), &project.getUndoManager());
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::toggleSolo()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Solo");
    Track track = arrangement.getTrack (idx);
    track.setSolo (! track.isSolo(), &project.getUndoManager());
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::toggleRecordArm()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Record Arm");
    Track track = arrangement.getTrack (idx);
    track.setArmed (! track.isArmed(), &project.getUndoManager());
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

// ── Command mode ────────────────────────────────────────────────────────

bool VimEngine::handleCommandKey (const dc::KeyPress& key)
{
    if (isEscapeOrCtrlC (key))
    {
        commandBuffer.clear();
        enterNormalMode();
        return true;
    }

    if (key == dc::KeyCode::Return)
    {
        executeCommand();
        commandBuffer.clear();
        enterNormalMode();
        return true;
    }

    if (key == dc::KeyCode::Backspace)
    {
        if (! commandBuffer.empty())
            commandBuffer.pop_back();

        if (commandBuffer.empty())
        {
            enterNormalMode();
            return true;
        }

        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    auto c = key.getTextCharacter();
    if (c >= 32)
    {
        commandBuffer += std::string (1, char (c));
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }

    return true;
}

void VimEngine::executeCommand()
{
    auto cmd = dc::trim (commandBuffer);

    if (dc::startsWith (cmd, "plugin ") || dc::startsWith (cmd, "plug "))
    {
        actionRegistry.executeAction ("command.plugin");
    }
    else if (cmd == "midi" || dc::startsWith (cmd, "midi "))
    {
        actionRegistry.executeAction ("command.midi");
    }
    else if (cmd == "cycle" || cmd == "loop")
    {
        toggleCycle();
    }
    else if (cmd == "cs" || cmd == "cyclestart")
    {
        project.setCycleStart (context.getGridCursorPosition());
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
    else if (cmd == "ce" || cmd == "cycleend")
    {
        project.setCycleEnd (context.getGridCursorPosition());
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

// ── Mode switching ──────────────────────────────────────────────────────────

void VimEngine::enterInsertMode()
{
    mode = Insert;
    listeners.call ([](Listener& l) { l.vimModeChanged (Insert); });
}

void VimEngine::enterNormalMode()
{
    bool wasPluginMenu = (mode == PluginMenu);
    mode = Normal;
    pluginSearchActive = false;
    pluginSearchBuffer.clear();
    grammar.reset();
    context.clearVisualSelection();
    listeners.call ([](Listener& l) { l.vimModeChanged (Normal); });

    if (wasPluginMenu && onPluginMenuCancel)
        onPluginMenuCancel();
}

void VimEngine::enterPluginMenuMode()
{
    pluginSearchActive = false;
    pluginSearchBuffer.clear();
    mode = PluginMenu;
    listeners.call ([](Listener& l) { l.vimModeChanged (PluginMenu); });
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

bool VimEngine::handlePluginSearchKey (const dc::KeyPress& key)
{
    // Escape / Ctrl-C — clear filter, back to browse
    if (isEscapeOrCtrlC (key))
    {
        pluginSearchActive = false;
        pluginSearchBuffer.clear();
        if (onPluginMenuClearFilter)
            onPluginMenuClearFilter();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Return — accept filter, back to browse
    if (key == dc::KeyCode::Return)
    {
        pluginSearchActive = false;
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Backspace — remove last char
    if (key == dc::KeyCode::Backspace)
    {
        if (! pluginSearchBuffer.empty())
            pluginSearchBuffer.pop_back();

        if (onPluginMenuFilter)
            onPluginMenuFilter (pluginSearchBuffer);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Printable char (no Ctrl/Cmd) — append to buffer
    auto c = key.getTextCharacter();
    if (c >= 32 && ! key.control && ! key.command)
    {
        pluginSearchBuffer += std::string (1, char (c));
        if (onPluginMenuFilter)
            onPluginMenuFilter (pluginSearchBuffer);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    return true; // consume all keys while searching
}

bool VimEngine::handlePluginMenuKey (const dc::KeyPress& key)
{
    // Delegate to search handler when search is active
    if (pluginSearchActive)
        return handlePluginSearchKey (key);

    if (isEscapeOrCtrlC (key))
    {
        enterNormalMode();
        return true;
    }

    if (key == dc::KeyCode::Return)
    {
        if (onPluginMenuConfirm)
            onPluginMenuConfirm();
        enterNormalMode();
        return true;
    }

    auto keyChar = key.getTextCharacter();

    // / — enter search sub-mode
    if (keyChar == '/')
    {
        pluginSearchActive = true;
        pluginSearchBuffer.clear();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // j / k — single-step navigation
    if (keyChar == 'j')
    {
        if (onPluginMenuMove)
            onPluginMenuMove (1);
        return true;
    }

    if (keyChar == 'k')
    {
        if (onPluginMenuMove)
            onPluginMenuMove (-1);
        return true;
    }

    // Ctrl-D — half-page down
    if (key.control
        && (keyChar == 'd' || keyChar == 'D'
            || keyChar == 4 /* Ctrl-D */))
    {
        if (onPluginMenuScroll)
            onPluginMenuScroll (1);
        return true;
    }

    // Ctrl-U — half-page up
    if (key.control
        && (keyChar == 'u' || keyChar == 'U'
            || keyChar == 21 /* Ctrl-U */))
    {
        if (onPluginMenuScroll)
            onPluginMenuScroll (-1);
        return true;
    }

    return true; // consume all keys in plugin menu mode
}

// ── Panel ───────────────────────────────────────────────────────────────────

void VimEngine::cycleFocusPanel()
{
    context.cyclePanel();
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::openFocusedItem()
{
    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (trackIdx);
    int clipIdx = context.getSelectedClipIndex();

    if (clipIdx < 0 || clipIdx >= track.getNumClips())
        return;

    auto clipState = track.getClip (clipIdx);

    if (clipState.getType() == IDs::MIDI_CLIP)
    {
        MidiClip clip (clipState);
        clip.expandNotesToChildren();

        context.openClipState = clipState;
        context.setPanel (VimContext::PianoRoll);

        if (onOpenPianoRoll)
            onOpenPianoRoll (clipState);

        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

void VimEngine::closePianoRoll()
{
    if (context.getPanel() == VimContext::PianoRoll)
    {
        // Collapse NOTE children back to base64 for storage
        if (context.openClipState.isValid() && context.openClipState.getType() == IDs::MIDI_CLIP)
        {
            MidiClip clip (context.openClipState);
            clip.collapseChildrenToMidiData (&project.getUndoManager());
        }

        context.openClipState = PropertyTree();
        context.setPanel (VimContext::Editor);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

bool VimEngine::handlePianoRollNormalKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape / Ctrl-C closes piano roll
    if (isEscapeOrCtrlC (key))
    {
        closePianoRoll();
        return true;
    }

    // Register prefix — use grammar for "x handling
    if (keyChar == '"')
    {
        grammar.feed (keyChar, key.shift, key.control, key.alt, key.command);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // If grammar is awaiting register char, feed it
    if (grammar.hasPendingState() && grammar.getPendingDisplay() == "\"")
    {
        grammar.feed (keyChar, key.shift, key.control, key.alt, key.command);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Ctrl+P opens command palette
    if (key.control && keyChar == 'p')
    {
        actionRegistry.executeAction ("command_palette");
        return true;
    }

    // Ctrl+A selects all
    if (key.control && keyChar == 'a')
    {
        if (onPianoRollSelectAll) onPianoRollSelectAll();
        return true;
    }

    // Pending 'g' for gg (jump to highest note row)
    if (grammar.hasPendingKey() && grammar.getPendingKey() == 'g')
    {
        if (keyChar == 'g')
        {
            grammar.clearPendingKey();
            if (onPianoRollJumpCursor) onPianoRollJumpCursor (-1, 127);
            return true;
        }
        grammar.clearPendingKey();
    }

    // Pending 'z' for zi/zo/zf
    if (grammar.hasPendingKey() && grammar.getPendingKey() == 'z')
    {
        grammar.clearPendingKey();
        if (keyChar == 'i')
        {
            if (onPianoRollZoom) onPianoRollZoom (1.25f);
            return true;
        }
        if (keyChar == 'o')
        {
            if (onPianoRollZoom) onPianoRollZoom (0.8f);
            return true;
        }
        if (keyChar == 'f')
        {
            if (onPianoRollZoomToFit) onPianoRollZoomToFit();
            return true;
        }
        return true; // consume unknown z-sequence
    }

    // Undo/redo
    if (keyChar == 'u' || (key.control && keyChar == 'z'))
    {
        project.getUndoSystem().undo();
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }
    if (keyChar == 'r' && key.control)
    {
        project.getUndoSystem().redo();
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Transport — Space is play/stop (consistent with other modes)
    if (key == dc::KeyCode::Space) { togglePlayStop(); return true; }

    // Enter toggles note at cursor
    if (key == dc::KeyCode::Return)
    {
        if (onPianoRollAddNote) onPianoRollAddNote();
        return true;
    }

    // Panel cycling
    if (key == dc::KeyCode::Tab) { cycleFocusPanel(); return true; }

    // Tool switching
    if (keyChar == '1' || keyChar == 's')
    {
        if (onSetPianoRollTool) onSetPianoRollTool (0); // Select
        return true;
    }
    if (keyChar == '2' || keyChar == 'd')
    {
        if (onSetPianoRollTool) onSetPianoRollTool (1); // Draw
        return true;
    }
    if (keyChar == '3')
    {
        if (onSetPianoRollTool) onSetPianoRollTool (2); // Erase
        return true;
    }

    // Ctrl+L toggles cycle (before navigation dispatch)
    if (key.control && (keyChar == 'l' || keyChar == 'L' || keyChar == 12))
    { toggleCycle(); return true; }

    // Navigation hjkl
    if (keyChar == 'h') { if (onPianoRollMoveCursor) onPianoRollMoveCursor (-1, 0); return true; }
    if (keyChar == 'l') { if (onPianoRollMoveCursor) onPianoRollMoveCursor (1, 0); return true; }
    if (keyChar == 'k') { if (onPianoRollMoveCursor) onPianoRollMoveCursor (0, 1); return true; }
    if (keyChar == 'j') { if (onPianoRollMoveCursor) onPianoRollMoveCursor (0, -1); return true; }

    // Jump keys
    if (keyChar == '0') { if (onPianoRollJumpCursor) onPianoRollJumpCursor (0, -1); return true; }
    if (keyChar == '$')
    {
        // Jump to end — will be interpreted as "large value"
        if (onPianoRollJumpCursor) onPianoRollJumpCursor (99999, -1);
        return true;
    }
    if (keyChar == 'G') { if (onPianoRollJumpCursor) onPianoRollJumpCursor (-1, 0); return true; }
    if (keyChar == 'g')
    {
        grammar.setPendingKey ('g');
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Delete
    if (keyChar == 'x' || key == dc::KeyCode::Delete)
    {
        if (onPianoRollDeleteSelected) onPianoRollDeleteSelected (grammar.consumeRegister());
        return true;
    }

    // Yank (copy)
    if (keyChar == 'y')
    {
        if (onPianoRollCopy) onPianoRollCopy (grammar.consumeRegister());
        return true;
    }

    // Paste
    if (keyChar == 'p')
    {
        if (onPianoRollPaste) onPianoRollPaste (grammar.consumeRegister());
        return true;
    }

    // Duplicate
    if (keyChar == 'D')
    {
        if (onPianoRollDuplicate) onPianoRollDuplicate();
        return true;
    }

    // Transpose
    if (keyChar == '+' || keyChar == '=')
    {
        if (onPianoRollTranspose) onPianoRollTranspose (1);
        return true;
    }
    if (keyChar == '-')
    {
        if (onPianoRollTranspose) onPianoRollTranspose (-1);
        return true;
    }

    // Quantize / humanize
    if (keyChar == 'q')
    {
        if (onPianoRollQuantize) onPianoRollQuantize();
        return true;
    }
    if (keyChar == 'Q')
    {
        if (onPianoRollHumanize) onPianoRollHumanize();
        return true;
    }

    // Velocity lane toggle
    if (keyChar == 'v')
    {
        if (onPianoRollVelocityLane) onPianoRollVelocityLane (true);
        return true;
    }

    // Zoom
    if (keyChar == 'z')
    {
        grammar.setPendingKey ('z');
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Grid division coarser/finer
    if (keyChar == '[')
    {
        if (onPianoRollGridDiv) onPianoRollGridDiv (-1);
        return true;
    }
    if (keyChar == ']')
    {
        if (onPianoRollGridDiv) onPianoRollGridDiv (1);
        return true;
    }

    // Command mode
    if (keyChar == ':')
    {
        mode = Command;
        commandBuffer.clear();
        listeners.call ([](Listener& l) { l.vimModeChanged (Command); });
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    return false;
}

// Count/operator/pending-key helpers now live in VimGrammar.
// clearPending, isDigitForCount, accumulateDigit, getEffectiveCount,
// resetCounts, startOperator, cancelOperator, charToOperator, isMotionKey
// have all been removed from VimEngine.

// ── Clip edge helpers ────────────────────────────────────────────────────────

// Collect all clip start/end positions on a track, sorted ascending
static std::vector<int64_t> collectClipEdges (const Arrangement& arr, int trackIdx)
{
    std::vector<int64_t> edges;
    if (trackIdx < 0 || trackIdx >= arr.getNumTracks())
        return edges;

    Track track = arr.getTrack (trackIdx);
    for (int i = 0; i < track.getNumClips(); ++i)
    {
        auto clip = track.getClip (i);
        auto start = clip.getProperty (IDs::startPosition).getIntOr (0);
        auto length = clip.getProperty (IDs::length).getIntOr (0);
        edges.push_back (start);
        edges.push_back (start + length);
    }

    std::sort (edges.begin(), edges.end());
    edges.erase (std::unique (edges.begin(), edges.end()), edges.end());
    return edges;
}

// ── Motion resolution ───────────────────────────────────────────────────────

VimEngine::MotionRange VimEngine::resolveMotion (char32_t key, int count) const
{
    MotionRange range;
    int curTrack = arrangement.getSelectedTrackIndex();
    int curClip  = context.getSelectedClipIndex();
    int numTracks = arrangement.getNumTracks();

    if (curTrack < 0 || numTracks == 0)
        return range; // valid == false

    switch (key)
    {
        case 'j': // down count tracks (linewise)
        {
            range.linewise   = true;
            range.startTrack = curTrack;
            range.startClip  = 0;
            range.endTrack   = std::min (curTrack + count, numTracks - 1);
            range.endClip    = 0;
            range.valid      = true;
            break;
        }

        case 'k': // up count tracks (linewise)
        {
            range.linewise   = true;
            range.startTrack = std::max (curTrack - count, 0);
            range.startClip  = 0;
            range.endTrack   = curTrack;
            range.endClip    = 0;
            range.valid      = true;
            break;
        }

        case 'h': // left count clips (clipwise, same track)
        {
            range.linewise   = false;
            range.startTrack = curTrack;
            range.endTrack   = curTrack;
            range.startClip  = std::max (curClip - count, 0);
            range.endClip    = curClip;
            range.valid      = true;
            break;
        }

        case 'l': // right count clips (clipwise, same track)
        {
            Track track = arrangement.getTrack (curTrack);
            int lastClip = track.getNumClips() - 1;
            range.linewise   = false;
            range.startTrack = curTrack;
            range.endTrack   = curTrack;
            range.startClip  = curClip;
            range.endClip    = std::min (curClip + count, std::max (lastClip, 0));
            range.valid      = true;
            break;
        }

        case '$': // to end of track (clipwise)
        {
            Track track = arrangement.getTrack (curTrack);
            int lastClip = track.getNumClips() - 1;
            range.linewise   = false;
            range.startTrack = curTrack;
            range.endTrack   = curTrack;
            range.startClip  = curClip;
            range.endClip    = std::max (lastClip, 0);
            range.valid      = true;
            break;
        }

        case '0': // to start of track (clipwise)
        {
            range.linewise   = false;
            range.startTrack = curTrack;
            range.endTrack   = curTrack;
            range.startClip  = 0;
            range.endClip    = curClip;
            range.valid      = true;
            break;
        }

        case 'G': // to last track (linewise)
        {
            range.linewise   = true;
            range.startTrack = curTrack;
            range.startClip  = 0;
            range.endTrack   = numTracks - 1;
            range.endClip    = 0;
            range.valid      = true;
            break;
        }

        case 'g': // from gg — to first track (linewise)
        {
            range.linewise   = true;
            range.startTrack = 0;
            range.startClip  = 0;
            range.endTrack   = curTrack;
            range.endClip    = 0;
            range.valid      = true;
            break;
        }

        case 'w': // next clip boundary (clipwise range from cursor to next edge)
        case 'b': // previous clip boundary
        case 'e': // end of current/next clip
        {
            // For operator resolution, define range from current clip to target clip
            range.linewise   = false;
            range.startTrack = curTrack;
            range.endTrack   = curTrack;
            range.startClip  = curClip;
            range.endClip    = curClip; // operators will use grid positions in Phase 6
            range.valid      = true;
            break;
        }

        default:
            break;
    }

    return range;
}

VimEngine::MotionRange VimEngine::resolveLinewiseMotion (int count) const
{
    MotionRange range;
    int curTrack  = arrangement.getSelectedTrackIndex();
    int numTracks = arrangement.getNumTracks();

    if (curTrack < 0 || numTracks == 0)
        return range; // valid == false

    range.linewise   = true;
    range.startTrack = curTrack;
    range.startClip  = 0;
    range.endTrack   = std::min (curTrack + count - 1, numTracks - 1);
    range.endClip    = 0;
    range.valid      = true;

    return range;
}

// ── Operator execution ──────────────────────────────────────────────────────

std::vector<Clipboard::ClipEntry> VimEngine::collectClipsForRange (const MotionRange& range) const
{
    std::vector<Clipboard::ClipEntry> entries;
    int baseTrack = range.startTrack;
    int64_t minStart = std::numeric_limits<int64_t>::max();

    struct RawClip { PropertyTree data; int trackIdx; int64_t startPos; };
    std::vector<RawClip> rawClips;

    if (range.linewise)
    {
        for (int t = range.startTrack; t <= range.endTrack; ++t)
        {
            if (t < 0 || t >= arrangement.getNumTracks())
                continue;

            Track track = arrangement.getTrack (t);

            for (int c = 0; c < track.getNumClips(); ++c)
            {
                auto clip = track.getClip (c);
                auto startPos = clip.getProperty (IDs::startPosition).getIntOr (0);
                RawClip rc { clip, t, startPos };
                rawClips.push_back (rc);
                minStart = std::min (minStart, startPos);
            }
        }
    }
    else if (range.startTrack == range.endTrack)
    {
        int t = range.startTrack;
        if (t < 0 || t >= arrangement.getNumTracks())
            return entries;

        Track track = arrangement.getTrack (t);
        int endClip = std::min (range.endClip, track.getNumClips() - 1);

        for (int c = range.startClip; c <= endClip; ++c)
        {
            if (c >= 0 && c < track.getNumClips())
            {
                auto clip = track.getClip (c);
                auto startPos = clip.getProperty (IDs::startPosition).getIntOr (0);
                RawClip rc { clip, t, startPos };
                rawClips.push_back (rc);
                minStart = std::min (minStart, startPos);
            }
        }
    }
    else
    {
        for (int t = range.startTrack; t <= range.endTrack; ++t)
        {
            if (t < 0 || t >= arrangement.getNumTracks())
                continue;

            Track track = arrangement.getTrack (t);

            int startC = (t == range.startTrack) ? range.startClip : 0;
            int endC   = (t == range.endTrack)
                         ? std::min (range.endClip, track.getNumClips() - 1)
                         : track.getNumClips() - 1;

            for (int c = startC; c <= endC; ++c)
            {
                if (c >= 0 && c < track.getNumClips())
                {
                    auto clip = track.getClip (c);
                    auto startPos = clip.getProperty (IDs::startPosition).getIntOr (0);
                    RawClip rc { clip, t, startPos };
                    rawClips.push_back (rc);
                    minStart = std::min (minStart, startPos);
                }
            }
        }
    }

    if (minStart == std::numeric_limits<int64_t>::max())
        minStart = 0;

    for (auto& raw : rawClips)
        entries.push_back ({ raw.data, raw.trackIdx - baseTrack, raw.startPos - minStart });

    return entries;
}

void VimEngine::executeOperator (Operator op, const MotionRange& range)
{
    if (! range.valid)
        return;

    switch (op)
    {
        case OpDelete: executeDelete (range); break;
        case OpYank:   executeYank (range);   break;
        case OpChange: executeChange (range); break;
        case OpNone:   break;
    }
}

void VimEngine::executeDelete (const MotionRange& range)
{
    // Store deleted clips (Vim delete → unnamed + "1-"9 history)
    char reg = grammar.consumeRegister();
    auto entries = collectClipsForRange (range);
    if (! entries.empty())
        project.getClipboard().storeClips (reg, entries, range.linewise, false);

    auto& um = project.getUndoManager();

    if (range.linewise)
    {
        // Remove all clips from tracks in range (iterate backwards)
        for (int t = range.endTrack; t >= range.startTrack; --t)
        {
            if (t < 0 || t >= arrangement.getNumTracks())
                continue;

            Track track = arrangement.getTrack (t);

            for (int c = track.getNumClips() - 1; c >= 0; --c)
                track.removeClip (c, &um);
        }

        // Select the track at startTrack (or last valid)
        int selectTrack = std::min (range.startTrack, arrangement.getNumTracks() - 1);
        if (selectTrack >= 0)
            arrangement.selectTrack (selectTrack);

        updateClipIndexFromGridCursor();
    }
    else if (range.startTrack == range.endTrack)
    {
        // Clipwise — remove clips in range on a single track
        int t = range.startTrack;
        if (t < 0 || t >= arrangement.getNumTracks())
            return;

        Track track = arrangement.getTrack (t);

        int endClip = std::min (range.endClip, track.getNumClips() - 1);

        for (int c = endClip; c >= range.startClip; --c)
        {
            if (c >= 0 && c < track.getNumClips())
                track.removeClip (c, &um);
        }

        // Re-derive clip index from grid cursor position
        updateClipIndexFromGridCursor();
    }
    else
    {
        // Multi-track clipwise — boundary tracks have partial range, intermediate tracks all clips
        for (int t = range.endTrack; t >= range.startTrack; --t)
        {
            if (t < 0 || t >= arrangement.getNumTracks())
                continue;

            Track track = arrangement.getTrack (t);

            if (t > range.startTrack && t < range.endTrack)
            {
                // Intermediate track — remove all clips
                for (int c = track.getNumClips() - 1; c >= 0; --c)
                    track.removeClip (c, &um);
            }
            else if (t == range.startTrack)
            {
                // Start track — from startClip to end
                for (int c = track.getNumClips() - 1; c >= range.startClip; --c)
                {
                    if (c >= 0 && c < track.getNumClips())
                        track.removeClip (c, &um);
                }
            }
            else // t == range.endTrack
            {
                // End track — from beginning to endClip
                int end = std::min (range.endClip, track.getNumClips() - 1);
                for (int c = end; c >= 0; --c)
                    track.removeClip (c, &um);
            }
        }

        arrangement.selectTrack (range.startTrack);
        int remaining = arrangement.getTrack (range.startTrack).getNumClips();
        context.setSelectedClipIndex (remaining > 0 ? std::min (range.startClip, remaining - 1) : 0);
    }

    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::executeYank (const MotionRange& range)
{
    char reg = grammar.consumeRegister();
    auto entries = collectClipsForRange (range);
    if (! entries.empty())
        project.getClipboard().storeClips (reg, entries, range.linewise, true);
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::executeChange (const MotionRange& range)
{
    executeDelete (range);
    enterInsertMode();
}

void VimEngine::executeMotion (char32_t key, int count)
{
    switch (key)
    {
        case 'j':
            for (int i = 0; i < count; ++i)
                moveSelectionDown();
            break;

        case 'k':
            for (int i = 0; i < count; ++i)
                moveSelectionUp();
            break;

        case 'h':
            for (int i = 0; i < count; ++i)
                moveSelectionLeft();
            break;

        case 'l':
            for (int i = 0; i < count; ++i)
                moveSelectionRight();
            break;

        case '0':
        {
            // Move grid cursor to start of timeline
            context.setGridCursorPosition (0);
            updateClipIndexFromGridCursor();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            break;
        }

        case '$':
        {
            // Move grid cursor to snapped end of last clip on current track
            double sr = transport.getSampleRate();
            int trackIdx = arrangement.getSelectedTrackIndex();
            int64_t maxEnd = 0;

            if (trackIdx >= 0 && trackIdx < arrangement.getNumTracks())
            {
                Track track = arrangement.getTrack (trackIdx);
                for (int ci = 0; ci < track.getNumClips(); ++ci)
                {
                    auto clipState = track.getClip (ci);
                    auto start = clipState.getProperty (IDs::startPosition).getIntOr (0);
                    auto length = clipState.getProperty (IDs::length).getIntOr (0);
                    maxEnd = std::max (maxEnd, start + length);
                }
            }

            if (sr > 0.0 && maxEnd > 0)
            {
                // Snap to last grid unit that's still within the last clip
                int64_t snapped = gridSystem.snapFloor (maxEnd - 1, sr);
                context.setGridCursorPosition (snapped);
            }
            else
            {
                context.setGridCursorPosition (maxEnd);
            }
            updateClipIndexFromGridCursor();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            break;
        }

        case 'G':
            if (count > 1)
            {
                // G with count: jump to track N (1-indexed)
                int target = std::min (count, arrangement.getNumTracks()) - 1;
                if (target >= 0)
                {
                    arrangement.selectTrack (target);
                    updateClipIndexFromGridCursor();
                    listeners.call ([](Listener& l) { l.vimContextChanged(); });
                }
            }
            else
            {
                jumpToLastTrack();
            }
            break;

        case 'g':
            // gg resolved by grammar — jump to first track (already handled)
            jumpToFirstTrack();
            break;

        case 'w':
        {
            // Jump forward to next clip edge, count times
            double sr = transport.getSampleRate();
            int trackIdx = arrangement.getSelectedTrackIndex();
            auto edges = collectClipEdges (arrangement, trackIdx);
            int64_t cursorPos = context.getGridCursorPosition();

            for (int n = 0; n < count; ++n)
            {
                // Find first edge strictly after cursor
                auto it = std::upper_bound (edges.begin(), edges.end(), cursorPos);
                if (it != edges.end())
                    cursorPos = *it;
                else
                    break;
            }

            if (sr > 0.0)
                cursorPos = gridSystem.snapFloor (cursorPos, sr);
            context.setGridCursorPosition (cursorPos);
            updateClipIndexFromGridCursor();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            break;
        }

        case 'b':
        {
            // Jump backward to previous clip edge, count times
            double sr = transport.getSampleRate();
            int trackIdx = arrangement.getSelectedTrackIndex();
            auto edges = collectClipEdges (arrangement, trackIdx);
            int64_t cursorPos = context.getGridCursorPosition();

            for (int n = 0; n < count; ++n)
            {
                // Find last edge strictly before cursor
                auto it = std::lower_bound (edges.begin(), edges.end(), cursorPos);
                if (it != edges.begin())
                {
                    --it;
                    cursorPos = *it;
                }
                else
                    break;
            }

            if (sr > 0.0)
                cursorPos = gridSystem.snapFloor (cursorPos, sr);
            context.setGridCursorPosition (cursorPos);
            updateClipIndexFromGridCursor();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            break;
        }

        case 'e':
        {
            // Jump to end of current/next clip, count times
            double sr = transport.getSampleRate();
            int trackIdx = arrangement.getSelectedTrackIndex();
            int64_t cursorPos = context.getGridCursorPosition();

            if (trackIdx >= 0 && trackIdx < arrangement.getNumTracks())
            {
                Track track = arrangement.getTrack (trackIdx);

                // Collect just clip end positions
                std::vector<int64_t> endEdges;
                for (int ci = 0; ci < track.getNumClips(); ++ci)
                {
                    auto clip = track.getClip (ci);
                    auto start = clip.getProperty (IDs::startPosition).getIntOr (0);
                    auto length = clip.getProperty (IDs::length).getIntOr (0);
                    endEdges.push_back (start + length);
                }
                std::sort (endEdges.begin(), endEdges.end());

                for (int n = 0; n < count; ++n)
                {
                    auto it = std::upper_bound (endEdges.begin(), endEdges.end(), cursorPos);
                    if (it != endEdges.end())
                    {
                        // Move to grid position just before end (inside the clip)
                        int64_t endPos = *it;
                        if (sr > 0.0)
                        {
                            int64_t snapped = gridSystem.snapFloor (endPos - 1, sr);
                            cursorPos = std::max (snapped, static_cast<int64_t> (0));
                        }
                        else
                        {
                            cursorPos = endPos;
                        }
                    }
                    else
                        break;
                }
            }

            context.setGridCursorPosition (cursorPos);
            updateClipIndexFromGridCursor();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            break;
        }

        default:
            break;
    }
}

// ── Visual mode ─────────────────────────────────────────────────────────────

void VimEngine::enterVisualMode()
{
    visualAnchorTrack = arrangement.getSelectedTrackIndex();
    visualAnchorClip  = context.getSelectedClipIndex();
    visualAnchorGridPos = context.getGridCursorPosition();
    mode = Visual;

    updateVisualSelection();
    listeners.call ([](Listener& l) { l.vimModeChanged (Visual); });
}

void VimEngine::enterVisualLineMode()
{
    visualAnchorTrack = arrangement.getSelectedTrackIndex();
    visualAnchorClip  = context.getSelectedClipIndex();
    visualAnchorGridPos = context.getGridCursorPosition();
    mode = VisualLine;

    updateVisualSelection();
    listeners.call ([](Listener& l) { l.vimModeChanged (VisualLine); });
}

void VimEngine::exitVisualMode()
{
    context.clearVisualSelection();
    context.clearGridVisualSelection();
    mode = Normal;
    grammar.reset();
    listeners.call ([](Listener& l) { l.vimModeChanged (Normal); });
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::updateVisualSelection()
{
    // Legacy clip-based visual selection (for rendering compatibility)
    VimContext::VisualSelection sel;
    sel.active     = true;
    sel.linewise   = (mode == VisualLine);
    sel.startTrack = visualAnchorTrack;
    sel.startClip  = visualAnchorClip;
    sel.endTrack   = arrangement.getSelectedTrackIndex();
    sel.endClip    = context.getSelectedClipIndex();
    context.setVisualSelection (sel);

    // Grid-based visual selection
    VimContext::GridVisualSelection gridSel;
    gridSel.active     = true;
    gridSel.linewise   = (mode == VisualLine);
    gridSel.startTrack = visualAnchorTrack;
    gridSel.endTrack   = arrangement.getSelectedTrackIndex();
    gridSel.startPos   = visualAnchorGridPos;
    gridSel.endPos     = context.getGridCursorPosition();
    context.setGridVisualSelection (gridSel);

    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

VimEngine::MotionRange VimEngine::getVisualRange() const
{
    MotionRange range;
    auto& gridSel = context.getGridVisualSelection();

    if (! gridSel.active)
        return range; // valid == false

    range.linewise   = gridSel.linewise;
    range.startTrack = std::min (gridSel.startTrack, gridSel.endTrack);
    range.endTrack   = std::max (gridSel.startTrack, gridSel.endTrack);

    if (gridSel.linewise)
    {
        range.startClip = 0;
        range.endClip   = 0;
    }
    else
    {
        // For grid-based visual mode, find clips that overlap the grid range
        // and set clip indices accordingly. Operators will delete/yank all
        // clips that overlap the [minPos, maxPos) range.
        int64_t minPos = std::min (gridSel.startPos, gridSel.endPos);
        int64_t maxPos = std::max (gridSel.startPos, gridSel.endPos);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            maxPos += gridSystem.getGridUnitInSamples (sr); // include cursor's grid unit

        // Find first and last clip indices that overlap the grid range on the primary track
        int primaryTrack = range.startTrack;
        if (primaryTrack >= 0 && primaryTrack < arrangement.getNumTracks())
        {
            Track track = arrangement.getTrack (primaryTrack);
            int firstClip = -1, lastClip = -1;

            for (int i = 0; i < track.getNumClips(); ++i)
            {
                auto clip = track.getClip (i);
                auto start = clip.getProperty (IDs::startPosition).getIntOr (0);
                auto length = clip.getProperty (IDs::length).getIntOr (0);

                // Clip overlaps if clip range [start, start+length) intersects [minPos, maxPos)
                if (start < maxPos && start + length > minPos)
                {
                    if (firstClip < 0)
                        firstClip = i;
                    lastClip = i;
                }
            }

            range.startClip = (firstClip >= 0) ? firstClip : 0;
            range.endClip   = (lastClip >= 0) ? lastClip : 0;
        }
        else
        {
            range.startClip = 0;
            range.endClip   = 0;
        }
    }

    range.valid = true;
    return range;
}

void VimEngine::executeVisualOperator (Operator op)
{
    auto& gridSel = context.getGridVisualSelection();
    if (! gridSel.active)
    {
        exitVisualMode();
        return;
    }

    if (gridSel.linewise)
    {
        // Linewise: use existing MotionRange path (operates on whole tracks)
        auto range = getVisualRange();
        if (! range.valid)
        {
            exitVisualMode();
            return;
        }

        ScopedTransaction txn (project.getUndoSystem(),
            op == OpDelete ? "Visual Delete" :
            op == OpYank   ? "Visual Yank"   : "Visual Change");

        executeOperator (op, range);
        exitVisualMode();
        return;
    }

    // Grid-based visual: use grid positions to find overlapping clips per track
    ScopedTransaction txn (project.getUndoSystem(),
        op == OpDelete ? "Visual Delete" :
        op == OpYank   ? "Visual Yank"   : "Visual Change");

    switch (op)
    {
        case OpDelete:
            executeGridVisualYank (false); // store as delete (rotates "1-"9)
            executeGridVisualDelete();
            break;
        case OpYank:
            executeGridVisualYank (true);
            break;
        case OpChange:
            executeGridVisualYank (false);
            executeGridVisualDelete();
            enterInsertMode();
            exitVisualMode();
            return;
        case OpNone:
            break;
    }

    exitVisualMode();
}

void VimEngine::executeGridVisualDelete()
{
    auto& gridSel = context.getGridVisualSelection();
    double sr = transport.getSampleRate();
    if (sr <= 0.0)
        return;

    int64_t minPos = std::min (gridSel.startPos, gridSel.endPos);
    int64_t maxPos = std::max (gridSel.startPos, gridSel.endPos);
    maxPos += gridSystem.getGridUnitInSamples (sr); // include cursor's grid cell

    int minTrack = std::min (gridSel.startTrack, gridSel.endTrack);
    int maxTrack = std::max (gridSel.startTrack, gridSel.endTrack);

    auto& um = project.getUndoManager();

    for (int t = maxTrack; t >= minTrack; --t)
    {
        if (t < 0 || t >= arrangement.getNumTracks())
            continue;

        Track track = arrangement.getTrack (t);

        // Collect new clips to add (from splits) after iterating
        std::vector<PropertyTree> newClips;

        // Process clips overlapping [minPos, maxPos) — iterate backwards for safe removal
        for (int c = track.getNumClips() - 1; c >= 0; --c)
        {
            auto clip = track.getClip (c);
            auto clipStart  = clip.getProperty (IDs::startPosition).getIntOr (0);
            auto clipLength = clip.getProperty (IDs::length).getIntOr (0);
            auto clipEnd    = clipStart + clipLength;

            // Skip non-overlapping clips
            if (clipStart >= maxPos || clipEnd <= minPos)
                continue;

            bool keepLeft  = clipStart < minPos;
            bool keepRight = clipEnd > maxPos;

            if (! keepLeft && ! keepRight)
            {
                // Fully inside selection — remove entirely
                track.removeClip (c, &um);
            }
            else if (keepLeft && keepRight)
            {
                // Selection is in the middle — split into left and right parts
                auto origTrimStart = clip.getProperty (IDs::trimStart).getIntOr (0);

                // Truncate original clip to be the left part [clipStart, minPos)
                int64_t leftLength = minPos - clipStart;
                clip.setProperty (IDs::length, Variant (leftLength), &um);

                // Create right part [maxPos, clipEnd)
                // Use nullptr for properties on detached tree — addChild with &um
                // handles undo of the whole subtree addition in one transaction
                auto rightClip = clip.createDeepCopy();
                int64_t rightOffset = maxPos - clipStart;
                rightClip.setProperty (IDs::startPosition, Variant (maxPos), nullptr);
                rightClip.setProperty (IDs::length, Variant (clipEnd - maxPos), nullptr);
                rightClip.setProperty (IDs::trimStart, Variant (origTrimStart + rightOffset), nullptr);
                newClips.push_back (rightClip);
            }
            else if (keepLeft)
            {
                // Selection covers the right side — truncate to [clipStart, minPos)
                int64_t leftLength = minPos - clipStart;
                clip.setProperty (IDs::length, Variant (leftLength), &um);
            }
            else // keepRight
            {
                // Selection covers the left side — shrink to [maxPos, clipEnd)
                auto origTrimStart = clip.getProperty (IDs::trimStart).getIntOr (0);
                int64_t rightOffset = maxPos - clipStart;

                clip.setProperty (IDs::startPosition, Variant (maxPos), &um);
                clip.setProperty (IDs::length, Variant (clipEnd - maxPos), &um);
                clip.setProperty (IDs::trimStart, Variant (origTrimStart + rightOffset), &um);
            }
        }

        // Append any new clips created from splits
        for (auto& nc : newClips)
            track.getState().addChild (nc, -1, &um);
    }

    // Move cursor to start of deleted range (like vim's d motion)
    context.setGridCursorPosition (minPos);
    arrangement.selectTrack (minTrack);
    updateClipIndexFromGridCursor();
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::executeGridVisualYank (bool isYank)
{
    char reg = grammar.consumeRegister();
    auto& gridSel = context.getGridVisualSelection();
    double sr = transport.getSampleRate();
    if (sr <= 0.0)
        return;

    int64_t minPos = std::min (gridSel.startPos, gridSel.endPos);
    int64_t maxPos = std::max (gridSel.startPos, gridSel.endPos);
    maxPos += gridSystem.getGridUnitInSamples (sr);

    int minTrack = std::min (gridSel.startTrack, gridSel.endTrack);
    int maxTrack = std::max (gridSel.startTrack, gridSel.endTrack);

    std::vector<Clipboard::ClipEntry> entries;
    int64_t globalMinStart = std::numeric_limits<int64_t>::max();

    // First pass: collect trimmed clips with raw positions
    struct RawClip { PropertyTree data; int trackIdx; int64_t startPos; };
    std::vector<RawClip> rawClips;

    for (int t = minTrack; t <= maxTrack; ++t)
    {
        if (t < 0 || t >= arrangement.getNumTracks())
            continue;

        Track track = arrangement.getTrack (t);

        for (int c = 0; c < track.getNumClips(); ++c)
        {
            auto clip = track.getClip (c);
            auto clipStart  = clip.getProperty (IDs::startPosition).getIntOr (0);
            auto clipLength = clip.getProperty (IDs::length).getIntOr (0);
            auto clipEnd    = clipStart + clipLength;

            if (clipStart >= maxPos || clipEnd <= minPos)
                continue;

            // Trim the yanked copy to only the portion within [minPos, maxPos)
            auto trimmedCopy = clip.createDeepCopy();
            auto origTrimStart = trimmedCopy.getProperty (IDs::trimStart).getIntOr (0);

            int64_t newStart  = std::max (clipStart, minPos);
            int64_t newEnd    = std::min (clipEnd, maxPos);
            int64_t trimDelta = newStart - clipStart;

            trimmedCopy.setProperty (IDs::startPosition,
                                     Variant (newStart), nullptr);
            trimmedCopy.setProperty (IDs::length,
                                     Variant (newEnd - newStart), nullptr);
            trimmedCopy.setProperty (IDs::trimStart,
                                     Variant (origTrimStart + trimDelta), nullptr);

            RawClip rc { trimmedCopy, t, newStart };
            rawClips.push_back (rc);
            globalMinStart = std::min (globalMinStart, newStart);
        }
    }

    if (globalMinStart == std::numeric_limits<int64_t>::max())
        globalMinStart = 0;

    // Second pass: build entries with relative offsets
    for (auto& raw : rawClips)
        entries.push_back ({ raw.data, raw.trackIdx - minTrack, raw.startPos - globalMinStart });

    project.getClipboard().storeClips (reg, entries, false, isYank);
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::executeVisualMute()
{
    auto& sel = context.getVisualSelection();
    if (! sel.active)
    {
        exitVisualMode();
        return;
    }

    int minTrack = std::min (sel.startTrack, sel.endTrack);
    int maxTrack = std::max (sel.startTrack, sel.endTrack);

    ScopedTransaction txn (project.getUndoSystem(), "Visual Mute");

    for (int t = minTrack; t <= maxTrack; ++t)
    {
        if (t < 0 || t >= arrangement.getNumTracks())
            continue;

        Track track = arrangement.getTrack (t);
        track.setMuted (! track.isMuted(), &project.getUndoManager());
    }

    exitVisualMode();
}

void VimEngine::executeVisualSolo()
{
    auto& sel = context.getVisualSelection();
    if (! sel.active)
    {
        exitVisualMode();
        return;
    }

    int minTrack = std::min (sel.startTrack, sel.endTrack);
    int maxTrack = std::max (sel.startTrack, sel.endTrack);

    ScopedTransaction txn (project.getUndoSystem(), "Visual Solo");

    for (int t = minTrack; t <= maxTrack; ++t)
    {
        if (t < 0 || t >= arrangement.getNumTracks())
            continue;

        Track track = arrangement.getTrack (t);
        track.setSolo (! track.isSolo(), &project.getUndoManager());
    }

    exitVisualMode();
}

bool VimEngine::handleVisualKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape or Ctrl-C exits visual mode
    if (isEscapeOrCtrlC (key) || keyChar == 'v')
    {
        grammar.reset();
        exitVisualMode();
        return true;
    }

    // Switch to VisualLine (before grammar to avoid operator interception)
    if (keyChar == 'V')
    {
        mode = VisualLine;
        updateVisualSelection();
        listeners.call ([](Listener& l) { l.vimModeChanged (VisualLine); });
        return true;
    }

    // Grid division change in visual mode
    if (keyChar == '[')
    {
        gridSystem.adjustGridDivision (-1);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            context.setGridCursorPosition (gridSystem.snapFloor (context.getGridCursorPosition(), sr));
        updateClipIndexFromGridCursor();
        updateVisualSelection();
        return true;
    }
    if (keyChar == ']')
    {
        gridSystem.adjustGridDivision (1);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            context.setGridCursorPosition (gridSystem.snapFloor (context.getGridCursorPosition(), sr));
        updateClipIndexFromGridCursor();
        updateVisualSelection();
        return true;
    }

    // Use grammar for register prefix, count, and motion parsing
    auto result = grammar.feed (keyChar, key.shift, key.control, key.alt, key.command);

    switch (result.type)
    {
        case VimGrammar::ParseResult::Incomplete:
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;

        case VimGrammar::ParseResult::Motion:
            executeMotion (result.motionKey, result.count);
            updateVisualSelection();
            return true;

        case VimGrammar::ParseResult::OperatorMotion:
        case VimGrammar::ParseResult::LinewiseOperator:
            // In visual mode, operators act on the selection directly
            // (grammar may have parsed d/y/c as operator-pending then doubled)
            break;

        case VimGrammar::ParseResult::NoMatch:
            break;
    }

    // Cycle: set cycle to visual selection, return to normal
    if (key.control && (keyChar == 'l' || keyChar == 'L' || keyChar == 12))
    { setCycleToGridVisual(); exitVisualMode(); return true; }

    // Operators in visual mode act on selection, not grammar motions
    if (keyChar == 'd' || keyChar == 'x') { executeVisualOperator (OpDelete); return true; }
    if (keyChar == 'y')                   { executeVisualOperator (OpYank);   return true; }
    if (keyChar == 'c')                   { executeVisualOperator (OpChange); return true; }
    if (keyChar == 'p')
    {
        // Visual paste: delete selection, then paste
        executeVisualOperator (OpDelete);
        pasteAfterPlayhead();
        return true;
    }

    // Track state toggles
    if (keyChar == 'M') { executeVisualMute(); return true; }
    if (keyChar == 'S') { executeVisualSolo(); return true; }

    return true; // consume all keys in visual mode
}

bool VimEngine::handleVisualLineKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape or Ctrl-C or re-pressing V exits
    if (isEscapeOrCtrlC (key) || keyChar == 'V')
    {
        grammar.reset();
        exitVisualMode();
        return true;
    }

    // Switch to clipwise Visual
    if (keyChar == 'v')
    {
        mode = Visual;
        updateVisualSelection();
        listeners.call ([](Listener& l) { l.vimModeChanged (Visual); });
        return true;
    }

    // Grid division change in visual mode
    if (keyChar == '[')
    {
        gridSystem.adjustGridDivision (-1);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            context.setGridCursorPosition (gridSystem.snapFloor (context.getGridCursorPosition(), sr));
        updateClipIndexFromGridCursor();
        updateVisualSelection();
        return true;
    }
    if (keyChar == ']')
    {
        gridSystem.adjustGridDivision (1);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            context.setGridCursorPosition (gridSystem.snapFloor (context.getGridCursorPosition(), sr));
        updateClipIndexFromGridCursor();
        updateVisualSelection();
        return true;
    }

    // Use grammar for register prefix, count, and motion parsing
    auto result = grammar.feed (keyChar, key.shift, key.control, key.alt, key.command);

    switch (result.type)
    {
        case VimGrammar::ParseResult::Incomplete:
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;

        case VimGrammar::ParseResult::Motion:
        {
            // Only j/k/G/gg motions are meaningful in line mode
            char32_t mk = result.motionKey;
            if (mk == 'j' || mk == 'k' || mk == 'G' || mk == 'g')
            {
                executeMotion (mk, result.count);
                updateVisualSelection();
            }
            return true;
        }

        case VimGrammar::ParseResult::OperatorMotion:
        case VimGrammar::ParseResult::LinewiseOperator:
            break;

        case VimGrammar::ParseResult::NoMatch:
            break;
    }

    // Cycle: set cycle to visual selection, return to normal
    if (key.control && (keyChar == 'l' || keyChar == 'L' || keyChar == 12))
    { setCycleToGridVisual(); exitVisualMode(); return true; }

    // Operators
    if (keyChar == 'd' || keyChar == 'x') { executeVisualOperator (OpDelete); return true; }
    if (keyChar == 'y')                   { executeVisualOperator (OpYank);   return true; }
    if (keyChar == 'c')                   { executeVisualOperator (OpChange); return true; }
    if (keyChar == 'p')
    {
        executeVisualOperator (OpDelete);
        pasteAfterPlayhead();
        return true;
    }

    // Track state toggles
    if (keyChar == 'M') { executeVisualMute(); return true; }
    if (keyChar == 'S') { executeVisualSolo(); return true; }

    return true; // consume all keys in visual-line mode
}

// hasPendingState() and getPendingDisplay() now inlined in header, delegating to grammar

// ── Sequencer navigation ─────────────────────────────────────────────────────

bool VimEngine::handleSequencerNormalKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape / Ctrl-C returns to normal mode
    if (isEscapeOrCtrlC (key))
    {
        enterNormalMode();
        return true;
    }

    // Undo/redo
    if (keyChar == 'u' || (key.control && keyChar == 'z'))
    {
        project.getUndoSystem().undo();
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }
    if (keyChar == 'r' && key.control)
    {
        project.getUndoSystem().redo();
        updateClipIndexFromGridCursor();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Pending 'g' for gg
    if (grammar.hasPendingKey() && grammar.getPendingKey() == 'g')
    {
        if (keyChar == 'g')
        {
            grammar.clearPendingKey();
            seqJumpFirstRow();
            return true;
        }
        grammar.clearPendingKey();
    }

    // Ctrl+L toggles cycle (before navigation dispatch)
    if (key.control && (keyChar == 'l' || keyChar == 'L' || keyChar == 12))
    { toggleCycle(); return true; }

    // Navigation
    if (keyChar == 'h') { seqMoveLeft();  return true; }
    if (keyChar == 'l') { seqMoveRight(); return true; }
    if (keyChar == 'j') { seqMoveDown();  return true; }
    if (keyChar == 'k') { seqMoveUp();    return true; }

    // Jump keys
    if (keyChar == '0') { seqJumpFirstStep(); return true; }
    if (keyChar == '$') { seqJumpLastStep();  return true; }
    if (keyChar == 'G') { seqJumpLastRow();   return true; }
    if (keyChar == 'g')
    {
        grammar.setPendingKey ('g');
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Toggle step
    if (key == dc::KeyCode::Space)
    {
        seqToggleStep();
        return true;
    }

    // Velocity adjust
    if (keyChar == '+' || keyChar == '=') { seqAdjustVelocity (10);  return true; }
    if (keyChar == '-')                   { seqAdjustVelocity (-10); return true; }
    if (keyChar == 'v')                   { seqCycleVelocity();      return true; }

    // Row mute/solo
    if (keyChar == 'M') { seqToggleRowMute(); return true; }
    if (keyChar == 'S') { seqToggleRowSolo(); return true; }

    // Panel cycling
    if (key == dc::KeyCode::Tab) { cycleFocusPanel(); return true; }

    // Mode switch
    if (keyChar == 'i') { enterInsertMode(); return true; }

    // Transport (play/stop via Enter in sequencer)
    if (key == dc::KeyCode::Return)
    {
        togglePlayStop();
        return true;
    }

    // Command mode
    if (keyChar == ':')
    {
        mode = Command;
        commandBuffer.clear();
        listeners.call ([](Listener& l) { l.vimModeChanged (Command); });
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    return false;
}

void VimEngine::seqMoveLeft()
{
    int step = context.getSeqStep();
    if (step > 0)
    {
        context.setSeqStep (step - 1);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

void VimEngine::seqMoveRight()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int maxStep = 0;
    auto pattern = seq.getActivePattern();
    if (pattern.isValid())
        maxStep = static_cast<int> (pattern.getProperty (IDs::numSteps).getIntOr (16)) - 1;

    int step = context.getSeqStep();
    if (step < maxStep)
    {
        context.setSeqStep (step + 1);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

void VimEngine::seqMoveUp()
{
    int row = context.getSeqRow();
    if (row > 0)
    {
        context.setSeqRow (row - 1);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

void VimEngine::seqMoveDown()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int maxRow = seq.getNumRows() - 1;

    int row = context.getSeqRow();
    if (row < maxRow)
    {
        context.setSeqRow (row + 1);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
    }
}

void VimEngine::seqJumpFirstStep()
{
    context.setSeqStep (0);
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::seqJumpLastStep()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto pattern = seq.getActivePattern();
    if (! pattern.isValid()) return;

    int lastStep = static_cast<int> (pattern.getProperty (IDs::numSteps).getIntOr (16)) - 1;
    context.setSeqStep (std::max (0, lastStep));
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::seqJumpFirstRow()
{
    context.setSeqRow (0);
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::seqJumpLastRow()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int lastRow = seq.getNumRows() - 1;
    context.setSeqRow (std::max (0, lastRow));
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::seqToggleStep()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    auto step = StepSequencer::getStep (row, context.getSeqStep());
    if (! step.isValid()) return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Step");
    bool isActive = StepSequencer::isStepActive (step);
    step.setProperty (IDs::active, ! isActive, &project.getUndoManager());

    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::seqAdjustVelocity (int delta)
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    auto step = StepSequencer::getStep (row, context.getSeqStep());
    if (! step.isValid()) return;

    int vel = StepSequencer::getStepVelocity (step) + delta;
    vel = std::clamp (vel, 1, 127);

    ScopedTransaction txn (project.getUndoSystem(), "Adjust Velocity");
    step.setProperty (IDs::velocity, vel, &project.getUndoManager());

    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::seqCycleVelocity()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    auto step = StepSequencer::getStep (row, context.getSeqStep());
    if (! step.isValid()) return;

    // Cycle through preset velocities
    const int presets[] = { 25, 50, 75, 100, 127 };
    const int numPresets = 5;
    int currentVel = StepSequencer::getStepVelocity (step);

    int nextIdx = 0;
    for (int i = 0; i < numPresets; ++i)
    {
        if (presets[i] > currentVel)
        {
            nextIdx = i;
            break;
        }
        if (i == numPresets - 1)
            nextIdx = 0; // wrap around
    }

    ScopedTransaction txn (project.getUndoSystem(), "Cycle Velocity");
    step.setProperty (IDs::velocity, presets[nextIdx], &project.getUndoManager());

    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::seqToggleRowMute()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Row Mute");
    bool muted = StepSequencer::isRowMuted (row);
    row.setProperty (IDs::mute, ! muted, &project.getUndoManager());

    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::seqToggleRowSolo()
{
    auto seqState = project.getState().getChildWithType (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Row Solo");
    bool soloed = StepSequencer::isRowSoloed (row);
    row.setProperty (IDs::solo, ! soloed, &project.getUndoManager());

    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

// ── Keyboard mode ───────────────────────────────────────────────────────────

void VimEngine::enterKeyboardMode()
{
    mode = Keyboard;
    listeners.call ([](Listener& l) { l.vimModeChanged (Keyboard); });
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::exitKeyboardMode()
{
    // Send note-off for all held notes
    for (int note : keyboardState.heldNotes)
    {
        if (onLiveMidiNote)
            onLiveMidiNote (dc::MidiMessage::noteOff (keyboardState.midiChannel, note));
    }
    keyboardState.heldNotes.clear();
    keyboardState.notifyListeners();

    mode = Normal;
    listeners.call ([](Listener& l) { l.vimModeChanged (Normal); });
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

bool VimEngine::handleKeyboardKey (const dc::KeyPress& key)
{
    if (isEscapeOrCtrlC (key))
    {
        exitKeyboardMode();
        return true;
    }

    auto keyChar = key.getTextCharacter();

    // Control keys
    switch (keyChar)
    {
        case 'z': case 'Z':
            keyboardState.octaveDown();
            keyboardState.notifyListeners();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;

        case 'x': case 'X':
            keyboardState.octaveUp();
            keyboardState.notifyListeners();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;

        case 'c': case 'C':
            keyboardState.velocityDown();
            keyboardState.notifyListeners();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;

        case 'v': case 'V':
            keyboardState.velocityUp();
            keyboardState.notifyListeners();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;

        default:
            break;
    }

    // Piano key
    int note = keyboardState.keyToNote (keyChar);
    if (note >= 0 && keyboardState.heldNotes.find (note) == keyboardState.heldNotes.end())
    {
        keyboardState.heldNotes.insert (note);

        if (onLiveMidiNote)
            onLiveMidiNote (dc::MidiMessage::noteOn (keyboardState.midiChannel, note,
                                                      static_cast<float> (keyboardState.velocity) / 127.0f));

        keyboardState.notifyListeners();
        return true;
    }

    // Already held or not a piano key — consume if it's a piano key (prevent repeats)
    if (note >= 0)
        return true;

    return false;
}

bool VimEngine::handleKeyUp (const gfx::KeyEvent& event)
{
    if (mode != Keyboard)
        return false;

    auto keyChar = static_cast<char32_t> (event.unmodifiedCharacter
                                                ? event.unmodifiedCharacter
                                                : event.character);

    int note = keyboardState.keyToNote (keyChar);
    if (note >= 0 && keyboardState.heldNotes.find (note) != keyboardState.heldNotes.end())
    {
        keyboardState.heldNotes.erase (note);

        if (onLiveMidiNote)
            onLiveMidiNote (dc::MidiMessage::noteOff (keyboardState.midiChannel, note));

        keyboardState.notifyListeners();
        return true;
    }

    return false;
}

// ── Mixer panel ─────────────────────────────────────────────────────────────

int VimEngine::getMixerPluginCount() const
{
    if (context.isMasterStripSelected())
    {
        auto masterBus = project.getState().getChildWithType (IDs::MASTER_BUS);
        if (masterBus.isValid())
        {
            auto chain = masterBus.getChildWithType (IDs::PLUGIN_CHAIN);
            return chain.isValid() ? chain.getNumChildren() : 0;
        }
        return 0;
    }

    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return 0;

    Track track = arrangement.getTrack (trackIdx);
    return track.getNumPlugins();
}

bool VimEngine::handleMixerNormalKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // ── Escape / Ctrl-C
    if (isEscapeOrCtrlC (key))
    {
        grammar.reset();
        return true;
    }

    // ── g-prefix: gp toggles browser, gk enters keyboard
    if (grammar.hasPendingKey() && grammar.getPendingKey() == 'g')
    {
        if (keyChar == 'p')
        {
            grammar.clearPendingKey();
            actionRegistry.executeAction ("view.toggle_browser");
            return true;
        }
        if (keyChar == 'k')
        {
            grammar.clearPendingKey();
            enterKeyboardMode();
            return true;
        }
        grammar.clearPendingKey();
    }

    if (keyChar == 'g')
    {
        grammar.setPendingKey ('g');
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // ── Ctrl+L toggles cycle (before navigation dispatch)
    if (key.control && (keyChar == 'l' || keyChar == 'L' || keyChar == 12))
    { toggleCycle(); return true; }

    // ── h/l: move between strips
    if (keyChar == 'h')
    {
        if (context.isMasterStripSelected())
        {
            // Move from master to last regular track
            context.setMasterStripSelected (false);
            int numTracks = arrangement.getNumTracks();
            if (numTracks > 0)
                arrangement.selectTrack (numTracks - 1);
        }
        else
        {
            int idx = arrangement.getSelectedTrackIndex();
            if (idx > 0)
                arrangement.selectTrack (idx - 1);
        }
        context.setSelectedPluginSlot (0);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    if (keyChar == 'l')
    {
        if (! context.isMasterStripSelected())
        {
            int idx = arrangement.getSelectedTrackIndex();
            int numTracks = arrangement.getNumTracks();

            if (idx < numTracks - 1)
            {
                arrangement.selectTrack (idx + 1);
            }
            else
            {
                // Past last track → select master
                context.setMasterStripSelected (true);
            }
        }
        context.setSelectedPluginSlot (0);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // ── j/k: focus cycling and plugin slot navigation
    auto focus = context.getMixerFocus();

    if (keyChar == 'j')
    {
        if (focus == VimContext::FocusPlugins)
        {
            // Navigate plugin slots downward — allow moving through empty
            // slots (min 4 visible) plus one "add" slot past the last plugin
            int numPlugins = getMixerPluginCount();
            int maxSlot = std::max (numPlugins, 3); // 0..3 = 4 visible slots, plus add slot
            int slot = context.getSelectedPluginSlot();
            if (slot < maxSlot)
            {
                context.setSelectedPluginSlot (slot + 1);
                listeners.call ([](Listener& l) { l.vimContextChanged(); });
            }
        }
        else if (focus == VimContext::FocusVolume)
        {
            context.setMixerFocus (VimContext::FocusPan);
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
        }
        else if (focus == VimContext::FocusPan)
        {
            context.setMixerFocus (VimContext::FocusPlugins);
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
        }
        return true;
    }

    if (keyChar == 'k')
    {
        if (focus == VimContext::FocusPlugins)
        {
            int slot = context.getSelectedPluginSlot();
            if (slot > 0)
            {
                context.setSelectedPluginSlot (slot - 1);
                listeners.call ([](Listener& l) { l.vimContextChanged(); });
            }
            else
            {
                // At slot 0, exit back to Pan focus
                context.setMixerFocus (VimContext::FocusPan);
                listeners.call ([](Listener& l) { l.vimContextChanged(); });
            }
        }
        else if (focus == VimContext::FocusPan)
        {
            context.setMixerFocus (VimContext::FocusVolume);
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
        }
        else if (focus == VimContext::FocusPlugins)
        {
            context.setMixerFocus (VimContext::FocusPan);
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
        }
        return true;
    }

    // ── Return: open plugin view or add plugin
    if (key == dc::KeyCode::Return && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();
        int numPlugins = getMixerPluginCount();

        if (slot < numPlugins)
        {
            openPluginView (trackIdx, slot);
        }
        else
        {
            if (onMixerPluginAdd)
                onMixerPluginAdd (trackIdx);
        }
        return true;
    }

    // ── x: remove plugin
    if (keyChar == 'x' && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();
        int numPlugins = getMixerPluginCount();

        if (slot < numPlugins && onMixerPluginRemove)
        {
            onMixerPluginRemove (trackIdx, slot);
            // Clamp slot index after removal
            int newNum = getMixerPluginCount();
            if (context.getSelectedPluginSlot() > newNum)
                context.setSelectedPluginSlot (newNum);
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
        }
        return true;
    }

    // ── b: toggle bypass
    if (keyChar == 'b' && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();
        int numPlugins = getMixerPluginCount();

        if (slot < numPlugins && onMixerPluginBypass)
            onMixerPluginBypass (trackIdx, slot);
        return true;
    }

    // ── J/K (shift): reorder plugins
    if (keyChar == 'J' && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();
        int numPlugins = getMixerPluginCount();

        if (slot < numPlugins - 1 && onMixerPluginReorder)
        {
            onMixerPluginReorder (trackIdx, slot, slot + 1);
            context.setSelectedPluginSlot (slot + 1);
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
        }
        return true;
    }

    if (keyChar == 'K' && focus == VimContext::FocusPlugins)
    {
        int trackIdx = context.isMasterStripSelected() ? -1 : arrangement.getSelectedTrackIndex();
        int slot = context.getSelectedPluginSlot();

        if (slot > 0 && onMixerPluginReorder)
        {
            onMixerPluginReorder (trackIdx, slot, slot - 1);
            context.setSelectedPluginSlot (slot - 1);
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
        }
        return true;
    }

    // ── Track state toggles
    if (keyChar == 'M') { toggleMute();      return true; }
    if (keyChar == 'S') { toggleSolo();      return true; }
    if (keyChar == 'r') { toggleRecordArm(); return true; }

    // ── Mode switch
    if (keyChar == 'i') { enterInsertMode(); return true; }

    // ── Transport
    if (key == dc::KeyCode::Space) { togglePlayStop(); return true; }

    // ── Panel cycling
    if (key == dc::KeyCode::Tab) { cycleFocusPanel(); return true; }

    // ── Command mode
    if (keyChar == ':')
    {
        mode = Command;
        commandBuffer.clear();
        listeners.call ([](Listener& l) { l.vimModeChanged (Command); });
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    return false;
}

// ─── Plugin View ─────────────────────────────────────────────────────────────

std::string VimEngine::generateHintLabel (int index, int totalCount)
{
    // Home-row keys for hint labels: a,s,d,f,g,h,j,k,l
    // All hints are uniform length to avoid prefix conflicts
    // (e.g. 'a' would block 'aa','as' if mixed lengths were used)
    static const char keys[] = "asdfghjkl";
    static const int numKeys = 9;

    if (totalCount <= numKeys)
    {
        // Single-char hints: a, s, d, ...
        if (index < numKeys)
            return std::string (1, keys[index]);
    }
    else if (totalCount <= numKeys * numKeys)
    {
        // Two-char hints for ALL entries: aa, as, ad, ...
        int first = index / numKeys;
        int second = index % numKeys;
        if (first < numKeys)
            return std::string (1, keys[first])
                 + std::string (1, keys[second]);
    }

    // Three-char for > 81 params (unlikely but safe)
    int first = (index / (numKeys * numKeys)) % numKeys;
    int second = (index / numKeys) % numKeys;
    int third = index % numKeys;
    return std::string (1, keys[first])
         + std::string (1, keys[second])
         + std::string (1, keys[third]);
}

int VimEngine::resolveHintLabel (const std::string& label, int totalCount)
{
    static const char keys[] = "asdfghjkl";
    static const int numKeys = 9;

    auto indexOf = [] (char c) -> int
    {
        for (int i = 0; i < numKeys; ++i)
            if (keys[i] == c) return i;
        return -1;
    };

    // Determine expected label length based on totalCount
    int expectedLen = (totalCount <= numKeys) ? 1
                    : (totalCount <= numKeys * numKeys) ? 2
                    : 3;

    // Only resolve when label reaches the expected length
    if (static_cast<int> (label.size()) < expectedLen)
        return -1;

    if (expectedLen == 1)
    {
        int i = indexOf (label[0]);
        return (i >= 0 && i < totalCount) ? i : -1;
    }

    if (expectedLen == 2)
    {
        int first = indexOf (label[0]);
        int second = indexOf (label[1]);
        if (first < 0 || second < 0) return -1;
        int idx = first * numKeys + second;
        return (idx < totalCount) ? idx : -1;
    }

    // Three-char
    if (label.size() >= 3)
    {
        int first = indexOf (label[0]);
        int second = indexOf (label[1]);
        int third = indexOf (label[2]);
        if (first < 0 || second < 0 || third < 0) return -1;
        int idx = first * numKeys * numKeys + second * numKeys + third;
        return (idx < totalCount) ? idx : -1;
    }

    return -1;
}

void VimEngine::openPluginView (int trackIndex, int pluginIndex)
{
    context.setPluginViewTarget (trackIndex, pluginIndex);
    context.setPanel (VimContext::PluginView);

    if (onOpenPluginView)
        onOpenPluginView (trackIndex, pluginIndex);

    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

void VimEngine::closePluginView()
{
    context.clearPluginViewTarget();

    if (onClosePluginView)
        onClosePluginView();

    context.setPanel (VimContext::Mixer);
    listeners.call ([](Listener& l) { l.vimContextChanged(); });
}

bool VimEngine::handlePluginViewNormalKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // ── Number entry mode
    if (context.isNumberEntryActive())
    {
        if (keyChar >= '0' && keyChar <= '9')
        {
            auto buf = context.getNumberBuffer();
            buf += std::string (1, char (keyChar));
            context.setNumberBuffer (buf);
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;
        }

        if (keyChar == '.' && context.getNumberBuffer().find ('.') == std::string::npos)
        {
            auto buf = context.getNumberBuffer();
            buf += ".";
            context.setNumberBuffer (buf);
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;
        }

        if (key == dc::KeyCode::Return)
        {
            float pct = std::stof (context.getNumberBuffer());
            pct = std::clamp (pct, 0.0f, 100.0f);
            if (onPluginParamChanged)
                onPluginParamChanged (context.getSelectedParamIndex(), pct / 100.0f);
            context.clearNumberEntry();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;
        }

        if (isEscapeOrCtrlC (key))
        {
            context.clearNumberEntry();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;
        }

        if (key == dc::KeyCode::Backspace)
        {
            auto buf = context.getNumberBuffer();
            if (! buf.empty())
            {
                buf.pop_back();
                context.setNumberBuffer (buf);
            }
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;
        }

        return true; // absorb other keys during number entry
    }

    // ── Hint mode (both HintActive and HintSpatial)
    if (context.getHintMode() == VimContext::HintActive
        || context.getHintMode() == VimContext::HintSpatial)
    {
        bool isSpatial = (context.getHintMode() == VimContext::HintSpatial);

        if (isEscapeOrCtrlC (key))
        {
            context.setHintMode (VimContext::HintNone);
            context.clearHintBuffer();
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;
        }

        // Accept home-row hint chars
        static const std::string hintChars ("asdfghjkl");
        if (hintChars.find (char (keyChar)) != std::string::npos)
        {
            auto buf = context.getHintBuffer() + std::string (1, char (keyChar));
            context.setHintBuffer (buf);

            int resolved = resolveHintLabel (buf, context.getHintTotalCount());
            if (resolved >= 0)
            {
                if (isSpatial)
                {
                    // Log/diagnostics only — spatial index IS the selection index
                    if (onResolveSpatialHint)
                        onResolveSpatialHint (resolved);
                    context.setSelectedParamIndex (resolved);
                }
                else
                {
                    context.setSelectedParamIndex (resolved);
                }

                context.setHintMode (VimContext::HintNone);
                context.clearHintBuffer();
                listeners.call ([](Listener& l) { l.vimContextChanged(); });
                return true;
            }

            // Could be a partial match (first char of two-char label) — wait for more
            listeners.call ([](Listener& l) { l.vimContextChanged(); });
            return true;
        }

        // Non-hint char cancels hint mode
        context.setHintMode (VimContext::HintNone);
        context.clearHintBuffer();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // ── Normal plugin view keys

    // Escape: close plugin view
    if (isEscapeOrCtrlC (key))
    {
        closePluginView();
        return true;
    }

    // f: enter hint mode (spatial if available, otherwise parameter list)
    if (keyChar == 'f')
    {
        int spatialCount = onQuerySpatialHintCount ? onQuerySpatialHintCount() : 0;
        if (spatialCount > 0)
        {
            context.setHintMode (VimContext::HintSpatial);
            context.setHintTotalCount (spatialCount);
        }
        else
        {
            int paramCount = onQueryPluginParamCount ? onQueryPluginParamCount() : 0;
            context.setHintMode (VimContext::HintActive);
            context.setHintTotalCount (paramCount);
        }
        context.clearHintBuffer();
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // Ctrl+L toggles cycle (before navigation dispatch)
    if (key.control && (keyChar == 'l' || keyChar == 'L' || keyChar == 12))
    { toggleCycle(); return true; }

    // j/k: navigate parameters
    if (keyChar == 'j')
    {
        context.setSelectedParamIndex (context.getSelectedParamIndex() + 1);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    if (keyChar == 'k')
    {
        int idx = context.getSelectedParamIndex();
        if (idx > 0)
            context.setSelectedParamIndex (idx - 1);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // h/l: coarse adjust ±5%
    if (keyChar == 'h')
    {
        if (onPluginParamAdjust)
            onPluginParamAdjust (context.getSelectedParamIndex(), -0.05f);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    if (keyChar == 'l')
    {
        if (onPluginParamAdjust)
            onPluginParamAdjust (context.getSelectedParamIndex(), 0.05f);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // H/L: fine adjust ±1%
    if (keyChar == 'H')
    {
        if (onPluginParamAdjust)
            onPluginParamAdjust (context.getSelectedParamIndex(), -0.01f);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    if (keyChar == 'L')
    {
        if (onPluginParamAdjust)
            onPluginParamAdjust (context.getSelectedParamIndex(), 0.01f);
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // 0-9: start number entry
    if (keyChar >= '0' && keyChar <= '9')
    {
        context.setNumberEntryActive (true);
        context.setNumberBuffer (std::string (1, char (keyChar)));
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // e: open native editor popup (existing behavior)
    if (keyChar == 'e')
    {
        int trackIdx = context.getPluginViewTrackIndex();
        int pluginIdx = context.getPluginViewPluginIndex();
        if (onMixerPluginOpen)
            onMixerPluginOpen (trackIdx, pluginIdx);
        return true;
    }

    // z: toggle enlarged plugin view
    if (keyChar == 'z')
    {
        context.setPluginViewEnlarged (! context.isPluginViewEnlarged());
        listeners.call ([](Listener& l) { l.vimContextChanged(); });
        return true;
    }

    // R: force spatial rescan (invalidate cache and re-run)
    if (keyChar == 'R')
    {
        if (onPluginViewRescan)
            onPluginViewRescan();
        return true;
    }

    // x: toggle drag axis (horizontal ↔ vertical)
    if (keyChar == 'x')
    {
        if (onPluginViewToggleDragAxis)
            onPluginViewToggleDragAxis();
        return true;
    }

    // q: end active drag session without closing plugin view
    if (keyChar == 'q')
    {
        if (onPluginViewEndDrag)
            onPluginViewEndDrag();
        return true;
    }

    // c: toggle center-on-reverse (reset to middle when h↔l direction changes)
    if (keyChar == 'c')
    {
        if (onPluginViewToggleDragCenter)
            onPluginViewToggleDragCenter();
        return true;
    }

    // Tab: cycle panel
    if (key == dc::KeyCode::Tab)
    {
        closePluginView();
        cycleFocusPanel();
        return true;
    }

    // Space: toggle play/stop
    if (key == dc::KeyCode::Space)
    {
        togglePlayStop();
        return true;
    }

    return false;
}

} // namespace dc
