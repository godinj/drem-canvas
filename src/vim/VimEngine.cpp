#include "VimEngine.h"
#include "model/AudioClip.h"
#include "model/MidiClip.h"
#include "utils/UndoSystem.h"

namespace dc
{

VimEngine::VimEngine (Project& p, TransportController& t,
                      Arrangement& a, VimContext& c)
    : project (p), transport (t), arrangement (a), context (c)
{
}

bool VimEngine::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    // Ctrl+P opens command palette from any mode
    if (key.getModifiers().isCtrlDown() && key.getTextCharacter() == 'p')
    {
        if (onCommandPalette)
            onCommandPalette();
        return true;
    }

    if (mode == Command)
        return handleCommandKey (key);

    if (mode == Normal)
        return handleNormalKey (key);

    return handleInsertKey (key);
}

bool VimEngine::handleKeyEvent (const gfx::KeyEvent& event)
{
    // Convert gfx::KeyEvent to juce::KeyPress for the shared dispatch logic
    int juceKeyCode = 0;

    // Map macOS virtual key codes to JUCE key codes for special keys
    switch (event.keyCode)
    {
        case 0x35: juceKeyCode = juce::KeyPress::escapeKey;    break; // Escape
        case 0x24: juceKeyCode = juce::KeyPress::returnKey;    break; // Return
        case 0x30: juceKeyCode = juce::KeyPress::tabKey;       break; // Tab
        case 0x31: juceKeyCode = juce::KeyPress::spaceKey;     break; // Space
        case 0x33: juceKeyCode = juce::KeyPress::backspaceKey; break; // Backspace
        case 0x7E: juceKeyCode = juce::KeyPress::upKey;        break; // Up
        case 0x7D: juceKeyCode = juce::KeyPress::downKey;      break; // Down
        case 0x7B: juceKeyCode = juce::KeyPress::leftKey;      break; // Left
        case 0x7C: juceKeyCode = juce::KeyPress::rightKey;     break; // Right
        default:
            juceKeyCode = static_cast<int> (event.character);
            break;
    }

    juce::ModifierKeys mods;
    int modFlags = 0;
    if (event.shift)   modFlags |= juce::ModifierKeys::shiftModifier;
    if (event.control) modFlags |= juce::ModifierKeys::ctrlModifier;
    if (event.alt)     modFlags |= juce::ModifierKeys::altModifier;
    if (event.command) modFlags |= juce::ModifierKeys::commandModifier;
    mods = juce::ModifierKeys (modFlags);

    juce::KeyPress key (juceKeyCode, mods, static_cast<juce_wchar> (event.character));
    return keyPressed (key, nullptr);
}

bool VimEngine::handleInsertKey (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        enterNormalMode();
        return true;
    }

    return false;
}

// ── Normal-mode phased dispatch ─────────────────────────────────────────────

bool VimEngine::handleNormalKey (const juce::KeyPress& key)
{
    // Dispatch to panel-specific handlers
    if (context.getPanel() == VimContext::PianoRoll)
        return handlePianoRollNormalKey (key);

    if (context.getPanel() == VimContext::Sequencer)
        return handleSequencerNormalKey (key);

    auto keyChar = key.getTextCharacter();
    auto modifiers = key.getModifiers();

    // Phase 1: Handle pending 'g' (with operator-pending awareness for dgg)
    if (pendingKey == 'g')
    {
        if (keyChar == 'g'
            && (juce::Time::currentTimeMillis() - pendingTimestamp) < pendingTimeoutMs)
        {
            clearPending();

            if (isOperatorPending())
            {
                // e.g. dgg — resolve motion from first track to current
                auto range = resolveMotion ('g', getEffectiveCount());
                executeOperator (pendingOperator, range);
                pendingOperator = OpNone;
                resetCounts();
                listeners.call (&Listener::vimContextChanged);
            }
            else
            {
                int count = getEffectiveCount();
                resetCounts();

                if (count > 1)
                {
                    // gg with count: jump to track N (1-indexed)
                    int target = std::min (count, arrangement.getNumTracks()) - 1;
                    arrangement.selectTrack (target);
                    context.setSelectedClipIndex (0);
                    listeners.call (&Listener::vimContextChanged);
                }
                else
                {
                    jumpToFirstTrack();
                }
            }
            return true;
        }

        // Timeout or different key — clear pending g and fall through
        clearPending();
    }

    // Phase 2: Escape cancels operator + counts + pending
    if (key == juce::KeyPress::escapeKey)
    {
        cancelOperator();
        clearPending();
        return true;
    }

    // Phase 3: Digit accumulation
    if (isDigitForCount (keyChar))
    {
        accumulateDigit (keyChar);
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

    // Phase 4: Operator keys d/y/c
    auto op = charToOperator (keyChar);
    if (op != OpNone)
    {
        if (isOperatorPending() && pendingOperator == op)
        {
            // dd / yy / cc — linewise on current track(s)
            int count = getEffectiveCount();
            auto range = resolveLinewiseMotion (count);
            executeOperator (op, range);
            pendingOperator = OpNone;
            resetCounts();
            listeners.call (&Listener::vimContextChanged);
        }
        else
        {
            startOperator (op);
            listeners.call (&Listener::vimContextChanged);
        }
        return true;
    }

    // Phase 5: Motion keys — resolve + execute operator or plain motion
    if (isMotionKey (keyChar))
    {
        int count = getEffectiveCount();

        if (isOperatorPending())
        {
            auto range = resolveMotion (keyChar, count);
            if (range.valid)
                executeOperator (pendingOperator, range);

            pendingOperator = OpNone;
            resetCounts();
            listeners.call (&Listener::vimContextChanged);
        }
        else
        {
            resetCounts();
            executeMotion (keyChar, count);
        }
        return true;
    }

    // Phase 6: Single-key actions with count support
    if (keyChar == 'x')
    {
        int count = getEffectiveCount();
        resetCounts();
        cancelOperator();

        for (int i = 0; i < count; ++i)
            deleteSelectedRegions();

        return true;
    }

    if (keyChar == 'p')
    {
        int count = getEffectiveCount();
        resetCounts();
        cancelOperator();

        for (int i = 0; i < count; ++i)
            pasteAfterPlayhead();

        return true;
    }

    if (keyChar == 'P')
    {
        int count = getEffectiveCount();
        resetCounts();
        cancelOperator();

        for (int i = 0; i < count; ++i)
            pasteBeforePlayhead();

        return true;
    }

    if (keyChar == 'D')
    {
        int count = getEffectiveCount();
        resetCounts();
        cancelOperator();

        for (int i = 0; i < count; ++i)
            duplicateSelectedClip();

        return true;
    }

    // Phase 7: Non-count actions — cancel any pending state
    if (isOperatorPending())
    {
        // These keys are not motions — cancel operator
        cancelOperator();
    }

    resetCounts();

    if (keyChar == 's') { splitRegionAtPlayhead(); return true; }

    // Undo/redo
    if (keyChar == 'u') { project.getUndoSystem().undo(); return true; }
    if (keyChar == 'r' && modifiers.isCtrlDown())
    {
        project.getUndoSystem().redo();
        return true;
    }

    // Track state
    if (keyChar == 'M') { toggleMute();      return true; }
    if (keyChar == 'S') { toggleSolo();      return true; }
    if (keyChar == 'r') { toggleRecordArm(); return true; }

    // Mode switch
    if (keyChar == 'i') { enterInsertMode(); return true; }

    // Transport
    if (key == juce::KeyPress::spaceKey) { togglePlayStop(); return true; }

    // Panel
    if (key == juce::KeyPress::tabKey) { cycleFocusPanel(); return true; }

    // Open item (stub)
    if (key == juce::KeyPress::returnKey) { openFocusedItem(); return true; }

    // Command mode
    if (keyChar == ':')
    {
        mode = Command;
        commandBuffer.clear();
        listeners.call (&Listener::vimModeChanged, Command);
        listeners.call (&Listener::vimContextChanged);
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
        context.setSelectedClipIndex (0);
        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::moveSelectionDown()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < arrangement.getNumTracks() - 1)
    {
        arrangement.selectTrack (idx + 1);
        context.setSelectedClipIndex (0);
        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::moveSelectionLeft()
{
    int clipIdx = context.getSelectedClipIndex();
    if (clipIdx > 0)
    {
        context.setSelectedClipIndex (clipIdx - 1);
        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::moveSelectionRight()
{
    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (trackIdx);
    int clipIdx = context.getSelectedClipIndex();

    if (clipIdx < track.getNumClips() - 1)
    {
        context.setSelectedClipIndex (clipIdx + 1);
        listeners.call (&Listener::vimContextChanged);
    }
}

// ── Track jumps ─────────────────────────────────────────────────────────────

void VimEngine::jumpToFirstTrack()
{
    if (arrangement.getNumTracks() > 0)
    {
        arrangement.selectTrack (0);
        context.setSelectedClipIndex (0);
        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::jumpToLastTrack()
{
    int count = arrangement.getNumTracks();
    if (count > 0)
    {
        arrangement.selectTrack (count - 1);
        context.setSelectedClipIndex (0);
        listeners.call (&Listener::vimContextChanged);
    }
}

// ── Transport ───────────────────────────────────────────────────────────────

void VimEngine::jumpToSessionStart()
{
    transport.setPositionInSamples (0);
    listeners.call (&Listener::vimContextChanged);
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
            auto start = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::startPosition, 0)));
            auto length = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::length, 0)));
            maxEnd = std::max (maxEnd, start + length);
        }
    }

    transport.setPositionInSamples (maxEnd);
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::togglePlayStop()
{
    transport.togglePlayStop();
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
        ScopedTransaction txn (project.getUndoSystem(), "Delete Clip");
        track.removeClip (clipIdx, &project.getUndoManager());

        if (clipIdx >= track.getNumClips() && track.getNumClips() > 0)
            context.setSelectedClipIndex (track.getNumClips() - 1);

        listeners.call (&Listener::vimContextChanged);
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
        context.setClipboard (track.getClip (clipIdx));
    }
}

void VimEngine::pasteAfterPlayhead()
{
    if (! context.hasClipboardContent())
        return;

    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Paste Clip");
    auto& um = project.getUndoManager();

    auto clipData = context.getClipboard().createCopy();
    clipData.setProperty (IDs::startPosition,
                          static_cast<juce::int64> (transport.getPositionInSamples()),
                          &um);

    Track track = arrangement.getTrack (trackIdx);
    track.getState().appendChild (clipData, &um);
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::pasteBeforePlayhead()
{
    if (! context.hasClipboardContent())
        return;

    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Paste Clip");
    auto& um = project.getUndoManager();

    auto clipData = context.getClipboard().createCopy();
    auto length = static_cast<int64_t> (static_cast<juce::int64> (clipData.getProperty (IDs::length, 0)));
    auto pastePos = transport.getPositionInSamples() - length;
    if (pastePos < 0) pastePos = 0;

    clipData.setProperty (IDs::startPosition,
                          static_cast<juce::int64> (pastePos), &um);

    Track track = arrangement.getTrack (trackIdx);
    track.getState().appendChild (clipData, &um);
    listeners.call (&Listener::vimContextChanged);
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
    auto clipStart  = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::startPosition, 0)));
    auto clipLength = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::length, 0)));
    auto playhead   = transport.getPositionInSamples();

    if (playhead <= clipStart || playhead >= clipStart + clipLength)
        return;

    auto splitOffset = playhead - clipStart;
    ScopedTransaction txn (project.getUndoSystem(), "Split Clip");
    auto& um = project.getUndoManager();

    clipState.setProperty (IDs::length, static_cast<juce::int64> (splitOffset), &um);
    clipState.setProperty (IDs::trimEnd,
        static_cast<juce::int64> (clipState.getProperty (IDs::trimStart, 0))
            + static_cast<juce::int64> (splitOffset), &um);

    auto newClip = clipState.createCopy();
    newClip.setProperty (IDs::startPosition, static_cast<juce::int64> (playhead), &um);
    newClip.setProperty (IDs::length, static_cast<juce::int64> (clipLength - splitOffset), &um);
    newClip.setProperty (IDs::trimStart,
        static_cast<juce::int64> (clipState.getProperty (IDs::trimStart, 0))
            + static_cast<juce::int64> (splitOffset), &um);

    track.getState().appendChild (newClip, &um);
    listeners.call (&Listener::vimContextChanged);
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
    auto startPos = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::startPosition, 0)));
    auto length   = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::length, 0)));

    ScopedTransaction txn (project.getUndoSystem(), "Duplicate Clip");
    auto& um = project.getUndoManager();

    auto newClip = clipState.createCopy();
    newClip.setProperty (IDs::startPosition, static_cast<juce::int64> (startPos + length), &um);

    track.getState().appendChild (newClip, &um);

    context.setSelectedClipIndex (track.getNumClips() - 1);
    listeners.call (&Listener::vimContextChanged);
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
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::toggleSolo()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Solo");
    Track track = arrangement.getTrack (idx);
    track.setSolo (! track.isSolo(), &project.getUndoManager());
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::toggleRecordArm()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Record Arm");
    Track track = arrangement.getTrack (idx);
    track.setArmed (! track.isArmed(), &project.getUndoManager());
    listeners.call (&Listener::vimContextChanged);
}

// ── Command mode ────────────────────────────────────────────────────────

bool VimEngine::handleCommandKey (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        commandBuffer.clear();
        enterNormalMode();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        executeCommand();
        commandBuffer.clear();
        enterNormalMode();
        return true;
    }

    if (key == juce::KeyPress::backspaceKey)
    {
        if (commandBuffer.isNotEmpty())
            commandBuffer = commandBuffer.dropLastCharacters (1);

        if (commandBuffer.isEmpty())
        {
            enterNormalMode();
            return true;
        }

        listeners.call (&Listener::vimContextChanged);
        return true;
    }

    auto c = key.getTextCharacter();
    if (c >= 32)
    {
        commandBuffer += juce::String::charToString (c);
        listeners.call (&Listener::vimContextChanged);
    }

    return true;
}

void VimEngine::executeCommand()
{
    auto cmd = commandBuffer.trim();

    if (cmd.startsWith ("plugin ") || cmd.startsWith ("plug "))
    {
        auto pluginName = cmd.fromFirstOccurrenceOf (" ", false, false).trim();
        if (pluginName.isNotEmpty() && onPluginCommand)
            onPluginCommand (pluginName);
    }
    else if (cmd == "midi" || cmd.startsWith ("midi "))
    {
        auto trackName = cmd.fromFirstOccurrenceOf (" ", false, false).trim();
        if (trackName.isEmpty())
            trackName = "MIDI";
        if (onCreateMidiTrack)
            onCreateMidiTrack (trackName);
    }
}

// ── Mode switching ──────────────────────────────────────────────────────────

void VimEngine::enterInsertMode()
{
    mode = Insert;
    listeners.call (&Listener::vimModeChanged, Insert);
}

void VimEngine::enterNormalMode()
{
    mode = Normal;
    cancelOperator();
    clearPending();
    listeners.call (&Listener::vimModeChanged, Normal);
}

// ── Panel ───────────────────────────────────────────────────────────────────

void VimEngine::cycleFocusPanel()
{
    context.cyclePanel();
    listeners.call (&Listener::vimContextChanged);
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

    if (clipState.hasType (IDs::MIDI_CLIP))
    {
        MidiClip clip (clipState);
        clip.expandNotesToChildren();

        context.openClipState = clipState;
        context.setPanel (VimContext::PianoRoll);

        if (onOpenPianoRoll)
            onOpenPianoRoll (clipState);

        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::closePianoRoll()
{
    if (context.getPanel() == VimContext::PianoRoll)
    {
        // Collapse NOTE children back to base64 for storage
        if (context.openClipState.isValid() && context.openClipState.hasType (IDs::MIDI_CLIP))
        {
            MidiClip clip (context.openClipState);
            clip.collapseChildrenToMidiData (&project.getUndoManager());
        }

        context.openClipState = juce::ValueTree();
        context.setPanel (VimContext::Editor);
        listeners.call (&Listener::vimContextChanged);
    }
}

bool VimEngine::handlePianoRollNormalKey (const juce::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();
    auto modifiers = key.getModifiers();

    // Escape closes piano roll
    if (key == juce::KeyPress::escapeKey)
    {
        closePianoRoll();
        return true;
    }

    // Ctrl+P opens command palette
    if (modifiers.isCtrlDown() && keyChar == 'p')
    {
        if (onCommandPalette)
            onCommandPalette();
        return true;
    }

    // Ctrl+A selects all
    if (modifiers.isCtrlDown() && keyChar == 'a')
    {
        if (onPianoRollSelectAll) onPianoRollSelectAll();
        return true;
    }

    // Pending 'g' for gg (jump to highest note row)
    if (pendingKey == 'g')
    {
        if (keyChar == 'g'
            && (juce::Time::currentTimeMillis() - pendingTimestamp) < pendingTimeoutMs)
        {
            clearPending();
            if (onPianoRollJumpCursor) onPianoRollJumpCursor (-1, 127);
            return true;
        }
        clearPending();
    }

    // Pending 'z' for zi/zo/zf
    if (pendingKey == 'z')
    {
        clearPending();
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
    if (keyChar == 'u') { project.getUndoSystem().undo(); return true; }
    if (keyChar == 'r' && modifiers.isCtrlDown())
    {
        project.getUndoSystem().redo();
        return true;
    }

    // Transport
    if (key == juce::KeyPress::spaceKey)
    {
        // If no pending action, use space for adding note at cursor
        // But we use Space for add-note and Return for play/stop in piano roll
        if (onPianoRollAddNote) onPianoRollAddNote();
        return true;
    }

    if (key == juce::KeyPress::returnKey) { togglePlayStop(); return true; }

    // Panel cycling
    if (key == juce::KeyPress::tabKey) { cycleFocusPanel(); return true; }

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
        pendingKey = 'g';
        pendingTimestamp = juce::Time::currentTimeMillis();
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

    // Delete
    if (keyChar == 'x' || key == juce::KeyPress::deleteKey)
    {
        if (onPianoRollDeleteSelected) onPianoRollDeleteSelected();
        return true;
    }

    // Yank (copy)
    if (keyChar == 'y')
    {
        if (onPianoRollCopy) onPianoRollCopy();
        return true;
    }

    // Paste
    if (keyChar == 'p')
    {
        if (onPianoRollPaste) onPianoRollPaste();
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
        pendingKey = 'z';
        pendingTimestamp = juce::Time::currentTimeMillis();
        listeners.call (&Listener::vimContextChanged);
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
        listeners.call (&Listener::vimModeChanged, Command);
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

    return false;
}

// ── Pending key helpers ─────────────────────────────────────────────────────

void VimEngine::clearPending()
{
    pendingKey = 0;
    pendingTimestamp = 0;
    listeners.call (&Listener::vimContextChanged);
}

// ── Count helpers ───────────────────────────────────────────────────────────

bool VimEngine::isDigitForCount (juce_wchar c) const
{
    if (c >= '1' && c <= '9')
        return true;

    // '0' is a count digit only when we're already accumulating a count
    if (c == '0' && (countAccumulator > 0 || operatorCount > 0))
        return true;

    return false;
}

void VimEngine::accumulateDigit (juce_wchar c)
{
    int digit = c - '0';

    if (isOperatorPending())
        operatorCount = operatorCount * 10 + digit;
    else
        countAccumulator = countAccumulator * 10 + digit;
}

int VimEngine::getEffectiveCount() const
{
    return std::max (1, countAccumulator) * std::max (1, operatorCount);
}

void VimEngine::resetCounts()
{
    countAccumulator = 0;
    operatorCount = 0;
}

// ── Operator state ──────────────────────────────────────────────────────────

void VimEngine::startOperator (Operator op)
{
    pendingOperator = op;
    operatorCount = 0;
}

void VimEngine::cancelOperator()
{
    pendingOperator = OpNone;
    resetCounts();
}

VimEngine::Operator VimEngine::charToOperator (juce_wchar c) const
{
    if (c == 'd') return OpDelete;
    if (c == 'y') return OpYank;
    if (c == 'c') return OpChange;
    return OpNone;
}

// ── Motion resolution ───────────────────────────────────────────────────────

bool VimEngine::isMotionKey (juce_wchar c) const
{
    return c == 'h' || c == 'j' || c == 'k' || c == 'l'
        || c == '0' || c == '$' || c == 'G' || c == 'g';
}

VimEngine::MotionRange VimEngine::resolveMotion (juce_wchar key, int count) const
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
    // Yank first (Vim deletes always yank)
    executeYank (range);

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

        context.setSelectedClipIndex (0);
    }
    else
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

        // Adjust clip selection
        int remaining = track.getNumClips();
        if (remaining > 0)
            context.setSelectedClipIndex (std::min (range.startClip, remaining - 1));
        else
            context.setSelectedClipIndex (0);
    }

    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::executeYank (const MotionRange& range)
{
    juce::Array<juce::ValueTree> clips;

    if (range.linewise)
    {
        for (int t = range.startTrack; t <= range.endTrack; ++t)
        {
            if (t < 0 || t >= arrangement.getNumTracks())
                continue;

            Track track = arrangement.getTrack (t);

            for (int c = 0; c < track.getNumClips(); ++c)
                clips.add (track.getClip (c));
        }
    }
    else
    {
        int t = range.startTrack;
        if (t < 0 || t >= arrangement.getNumTracks())
            return;

        Track track = arrangement.getTrack (t);
        int endClip = std::min (range.endClip, track.getNumClips() - 1);

        for (int c = range.startClip; c <= endClip; ++c)
        {
            if (c >= 0 && c < track.getNumClips())
                clips.add (track.getClip (c));
        }
    }

    context.setClipboardMulti (clips, range.linewise);
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::executeChange (const MotionRange& range)
{
    executeDelete (range);
    enterInsertMode();
}

void VimEngine::executeMotion (juce_wchar key, int count)
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
            jumpToSessionStart();
            break;

        case '$':
            jumpToSessionEnd();
            break;

        case 'G':
            if (count > 1 || countAccumulator > 0)
            {
                // G with count: jump to track N (1-indexed)
                int target = std::min (count, arrangement.getNumTracks()) - 1;
                if (target >= 0)
                {
                    arrangement.selectTrack (target);
                    context.setSelectedClipIndex (0);
                    listeners.call (&Listener::vimContextChanged);
                }
            }
            else
            {
                jumpToLastTrack();
            }
            break;

        case 'g':
            // Start of gg sequence
            pendingKey = 'g';
            pendingTimestamp = juce::Time::currentTimeMillis();
            listeners.call (&Listener::vimContextChanged);
            break;

        default:
            break;
    }
}

// ── Pending display (for status bar) ────────────────────────────────────────

bool VimEngine::hasPendingState() const
{
    return pendingOperator != OpNone || countAccumulator > 0
        || operatorCount > 0 || pendingKey != 0;
}

juce::String VimEngine::getPendingDisplay() const
{
    juce::String display;

    if (countAccumulator > 0)
        display += juce::String (countAccumulator);

    switch (pendingOperator)
    {
        case OpDelete: display += "d"; break;
        case OpYank:   display += "y"; break;
        case OpChange: display += "c"; break;
        case OpNone:   break;
    }

    if (operatorCount > 0)
        display += juce::String (operatorCount);

    if (pendingKey != 0)
        display += juce::String::charToString (pendingKey);

    return display;
}

// ── Sequencer navigation ─────────────────────────────────────────────────────

bool VimEngine::handleSequencerNormalKey (const juce::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();
    auto modifiers = key.getModifiers();

    // Escape returns to normal mode
    if (key == juce::KeyPress::escapeKey)
    {
        enterNormalMode();
        return true;
    }

    // Undo/redo
    if (keyChar == 'u') { project.getUndoSystem().undo(); return true; }
    if (keyChar == 'r' && modifiers.isCtrlDown())
    {
        project.getUndoSystem().redo();
        return true;
    }

    // Pending 'g' for gg
    if (pendingKey == 'g')
    {
        if (keyChar == 'g'
            && (juce::Time::currentTimeMillis() - pendingTimestamp) < pendingTimeoutMs)
        {
            clearPending();
            seqJumpFirstRow();
            return true;
        }
        clearPending();
    }

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
        pendingKey = 'g';
        pendingTimestamp = juce::Time::currentTimeMillis();
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

    // Toggle step
    if (key == juce::KeyPress::spaceKey)
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
    if (key == juce::KeyPress::tabKey) { cycleFocusPanel(); return true; }

    // Mode switch
    if (keyChar == 'i') { enterInsertMode(); return true; }

    // Transport (play/stop via Enter in sequencer)
    if (key == juce::KeyPress::returnKey)
    {
        togglePlayStop();
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
        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::seqMoveRight()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int maxStep = 0;
    auto pattern = seq.getActivePattern();
    if (pattern.isValid())
        maxStep = static_cast<int> (pattern.getProperty (IDs::numSteps, 16)) - 1;

    int step = context.getSeqStep();
    if (step < maxStep)
    {
        context.setSeqStep (step + 1);
        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::seqMoveUp()
{
    int row = context.getSeqRow();
    if (row > 0)
    {
        context.setSeqRow (row - 1);
        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::seqMoveDown()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int maxRow = seq.getNumRows() - 1;

    int row = context.getSeqRow();
    if (row < maxRow)
    {
        context.setSeqRow (row + 1);
        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::seqJumpFirstStep()
{
    context.setSeqStep (0);
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::seqJumpLastStep()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto pattern = seq.getActivePattern();
    if (! pattern.isValid()) return;

    int lastStep = static_cast<int> (pattern.getProperty (IDs::numSteps, 16)) - 1;
    context.setSeqStep (std::max (0, lastStep));
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::seqJumpFirstRow()
{
    context.setSeqRow (0);
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::seqJumpLastRow()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int lastRow = seq.getNumRows() - 1;
    context.setSeqRow (std::max (0, lastRow));
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::seqToggleStep()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    auto step = StepSequencer::getStep (row, context.getSeqStep());
    if (! step.isValid()) return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Step");
    bool isActive = StepSequencer::isStepActive (step);
    step.setProperty (IDs::active, ! isActive, &project.getUndoManager());

    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::seqAdjustVelocity (int delta)
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    auto step = StepSequencer::getStep (row, context.getSeqStep());
    if (! step.isValid()) return;

    int vel = StepSequencer::getStepVelocity (step) + delta;
    vel = juce::jlimit (1, 127, vel);

    ScopedTransaction txn (project.getUndoSystem(), "Adjust Velocity");
    step.setProperty (IDs::velocity, vel, &project.getUndoManager());

    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::seqCycleVelocity()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
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

    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::seqToggleRowMute()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Row Mute");
    bool muted = StepSequencer::isRowMuted (row);
    row.setProperty (IDs::mute, ! muted, &project.getUndoManager());

    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::seqToggleRowSolo()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    auto row = seq.getRow (context.getSeqRow());
    if (! row.isValid()) return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Row Solo");
    bool soloed = StepSequencer::isRowSoloed (row);
    row.setProperty (IDs::solo, ! soloed, &project.getUndoManager());

    listeners.call (&Listener::vimContextChanged);
}

} // namespace dc
