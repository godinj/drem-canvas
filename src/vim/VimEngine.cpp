#include "VimEngine.h"
#include "model/AudioClip.h"
#include "model/MidiClip.h"
#include "utils/UndoSystem.h"
#include <vector>
#include <algorithm>
namespace dc
{

static bool isEscapeOrCtrlC (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
        return true;

    if (key.getModifiers().isCtrlDown())
    {
        auto c = key.getTextCharacter();
        auto code = key.getKeyCode();
        // Ctrl-C: character may be ETX (3), 'c', 'C', or keyCode 'c'/'C'
        if (c == 3 || c == 'c' || c == 'C'
            || code == 'c' || code == 'C')
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

bool VimEngine::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    // Ctrl+P opens command palette from any mode
    if (key.getModifiers().isCtrlDown() && key.getTextCharacter() == 'p')
    {
        if (onCommandPalette)
            onCommandPalette();
        return true;
    }

    if (mode == Keyboard)
        return handleKeyboardKey (key);

    if (mode == PluginMenu)
        return handlePluginMenuKey (key);

    if (mode == Command)
        return handleCommandKey (key);

    if (mode == Visual)
        return handleVisualKey (key);

    if (mode == VisualLine)
        return handleVisualLineKey (key);

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

    // Use unmodifiedCharacter so Ctrl+key checks (e.g. Ctrl+P == 'p') work correctly;
    // event.character is a control code when modifiers are held on macOS.
    auto textChar = event.unmodifiedCharacter ? event.unmodifiedCharacter : event.character;
    juce::KeyPress key (juceKeyCode, mods, static_cast<juce_wchar> (textChar));
    return keyPressed (key, nullptr);
}

bool VimEngine::handleInsertKey (const juce::KeyPress& key)
{
    if (isEscapeOrCtrlC (key))
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

    // Ctrl+K enters keyboard mode (from any panel context)
    if (modifiers.isCtrlDown() && (keyChar == 'k' || keyChar == 'K'))
    {
        enterKeyboardMode();
        return true;
    }

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
                    updateClipIndexFromGridCursor();
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

    // Phase 2: Escape / Ctrl-C cancels operator + counts + pending
    if (isEscapeOrCtrlC (key))
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

    // Visual modes (Editor panel only)
    if (keyChar == 'v' && context.getPanel() == VimContext::Editor)
    {
        enterVisualMode();
        return true;
    }
    if (keyChar == 'V' && context.getPanel() == VimContext::Editor)
    {
        enterVisualLineMode();
        return true;
    }

    if (keyChar == 's') { splitRegionAtPlayhead(); return true; }

    // Undo/redo
    if (keyChar == 'u' || (modifiers.isCtrlDown() && keyChar == 'z'))
    {
        project.getUndoSystem().undo();
        updateClipIndexFromGridCursor();
        listeners.call (&Listener::vimContextChanged);
        return true;
    }
    if (keyChar == 'r' && modifiers.isCtrlDown())
    {
        project.getUndoSystem().redo();
        updateClipIndexFromGridCursor();
        listeners.call (&Listener::vimContextChanged);
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
        listeners.call (&Listener::vimContextChanged);
        return true;
    }
    if (keyChar == ']')
    {
        gridSystem.adjustGridDivision (1);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            context.setGridCursorPosition (gridSystem.snapFloor (context.getGridCursorPosition(), sr));
        updateClipIndexFromGridCursor();
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

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
        // Preserve grid cursor position (don't reset to 0)
        updateClipIndexFromGridCursor();
        listeners.call (&Listener::vimContextChanged);
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
        listeners.call (&Listener::vimContextChanged);
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
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::moveSelectionRight()
{
    double sr = transport.getSampleRate();
    if (sr <= 0.0) return;

    int64_t pos = context.getGridCursorPosition();
    int64_t newPos = gridSystem.moveByGridUnits (pos, 1, sr);
    context.setGridCursorPosition (newPos);
    updateClipIndexFromGridCursor();
    listeners.call (&Listener::vimContextChanged);
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
        auto start = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::startPosition, 0)));
        auto length = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::length, 0)));

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
        listeners.call (&Listener::vimContextChanged);
    }
}

void VimEngine::jumpToLastTrack()
{
    int count = arrangement.getNumTracks();
    if (count > 0)
    {
        arrangement.selectTrack (count - 1);
        updateClipIndexFromGridCursor();
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

// Carve a gap [gapStart, gapEnd) in existing clips on the given track,
// splitting any clip that overlaps those boundaries.
static void carveGap (Track& track, int64_t gapStart, int64_t gapEnd, juce::UndoManager& um)
{
    juce::Array<juce::ValueTree> newClips;

    for (int c = track.getNumClips() - 1; c >= 0; --c)
    {
        auto clip = track.getClip (c);
        auto clipStart  = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::startPosition, 0)));
        auto clipLength = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::length, 0)));
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
            auto origTrimStart = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::trimStart, 0)));
            int64_t leftLength = gapStart - clipStart;
            clip.setProperty (IDs::length, static_cast<juce::int64> (leftLength), &um);

            auto rightClip = clip.createCopy();
            int64_t rightOffset = gapEnd - clipStart;
            rightClip.setProperty (IDs::startPosition, static_cast<juce::int64> (gapEnd), nullptr);
            rightClip.setProperty (IDs::length, static_cast<juce::int64> (clipEnd - gapEnd), nullptr);
            rightClip.setProperty (IDs::trimStart, static_cast<juce::int64> (origTrimStart + rightOffset), nullptr);
            newClips.add (rightClip);
        }
        else if (keepLeft)
        {
            int64_t leftLength = gapStart - clipStart;
            clip.setProperty (IDs::length, static_cast<juce::int64> (leftLength), &um);
        }
        else // keepRight
        {
            auto origTrimStart = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::trimStart, 0)));
            int64_t rightOffset = gapEnd - clipStart;
            clip.setProperty (IDs::startPosition, static_cast<juce::int64> (gapEnd), &um);
            clip.setProperty (IDs::length, static_cast<juce::int64> (clipEnd - gapEnd), &um);
            clip.setProperty (IDs::trimStart, static_cast<juce::int64> (origTrimStart + rightOffset), &um);
        }
    }

    for (auto& nc : newClips)
        track.getState().appendChild (nc, &um);
}

void VimEngine::pasteAfterPlayhead()
{
    auto& multi = context.getClipboardMulti();
    bool hasMulti = ! multi.isEmpty();

    if (! hasMulti && ! context.hasClipboardContent())
        return;

    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Paste Clip");
    auto& um = project.getUndoManager();

    int64_t pastePos = context.getGridCursorPosition();
    Track track = arrangement.getTrack (trackIdx);

    if (hasMulti)
    {
        // Find the earliest start position in the clipboard to compute offsets
        int64_t basePos = std::numeric_limits<int64_t>::max();
        int64_t totalEnd = 0;
        for (auto& c : multi)
        {
            auto s = static_cast<int64_t> (static_cast<juce::int64> (c.getProperty (IDs::startPosition, 0)));
            auto l = static_cast<int64_t> (static_cast<juce::int64> (c.getProperty (IDs::length, 0)));
            basePos = std::min (basePos, s);
            totalEnd = std::max (totalEnd, s + l);
        }

        int64_t totalSpan = totalEnd - basePos;

        // Carve a gap in existing clips to make room for the pasted material
        carveGap (track, pastePos, pastePos + totalSpan, um);

        for (auto& c : multi)
        {
            auto copy = c.createCopy();
            auto origStart = static_cast<int64_t> (static_cast<juce::int64> (copy.getProperty (IDs::startPosition, 0)));
            int64_t offset = origStart - basePos;
            int64_t finalPos = pastePos + offset;
            copy.setProperty (IDs::startPosition,
                              static_cast<juce::int64> (finalPos), nullptr);
            track.getState().appendChild (copy, &um);
        }
    }
    else
    {
        auto clipData = context.getClipboard().createCopy();
        auto pasteLen = static_cast<int64_t> (static_cast<juce::int64> (clipData.getProperty (IDs::length, 0)));

        // Carve a gap in existing clips to make room
        carveGap (track, pastePos, pastePos + pasteLen, um);

        clipData.setProperty (IDs::startPosition,
                              static_cast<juce::int64> (pastePos), nullptr);
        track.getState().appendChild (clipData, &um);
    }

    updateClipIndexFromGridCursor();
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::pasteBeforePlayhead()
{
    auto& multi = context.getClipboardMulti();
    bool hasMulti = ! multi.isEmpty();

    if (! hasMulti && ! context.hasClipboardContent())
        return;

    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Paste Clip");
    auto& um = project.getUndoManager();

    Track track = arrangement.getTrack (trackIdx);

    if (hasMulti)
    {
        // Find earliest start and latest end to compute total span
        int64_t basePos = std::numeric_limits<int64_t>::max();
        int64_t maxEnd = 0;
        for (auto& c : multi)
        {
            auto s = static_cast<int64_t> (static_cast<juce::int64> (c.getProperty (IDs::startPosition, 0)));
            auto l = static_cast<int64_t> (static_cast<juce::int64> (c.getProperty (IDs::length, 0)));
            basePos = std::min (basePos, s);
            maxEnd  = std::max (maxEnd, s + l);
        }

        int64_t totalSpan = maxEnd - basePos;
        int64_t pastePos = context.getGridCursorPosition() - totalSpan;
        if (pastePos < 0) pastePos = 0;

        carveGap (track, pastePos, pastePos + totalSpan, um);

        for (auto& c : multi)
        {
            auto copy = c.createCopy();
            auto origStart = static_cast<int64_t> (static_cast<juce::int64> (copy.getProperty (IDs::startPosition, 0)));
            int64_t offset = origStart - basePos;
            copy.setProperty (IDs::startPosition,
                              static_cast<juce::int64> (pastePos + offset), nullptr);
            track.getState().appendChild (copy, &um);
        }
    }
    else
    {
        auto clipData = context.getClipboard().createCopy();
        auto pasteLen = static_cast<int64_t> (static_cast<juce::int64> (clipData.getProperty (IDs::length, 0)));
        auto pastePos = context.getGridCursorPosition() - pasteLen;
        if (pastePos < 0) pastePos = 0;

        carveGap (track, pastePos, pastePos + pasteLen, um);

        clipData.setProperty (IDs::startPosition,
                              static_cast<juce::int64> (pastePos), nullptr);
        track.getState().appendChild (clipData, &um);
    }

    updateClipIndexFromGridCursor();
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
    if (isEscapeOrCtrlC (key))
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
    bool wasPluginMenu = (mode == PluginMenu);
    mode = Normal;
    cancelOperator();
    clearPending();
    context.clearVisualSelection();
    listeners.call (&Listener::vimModeChanged, Normal);

    if (wasPluginMenu && onPluginMenuCancel)
        onPluginMenuCancel();
}

void VimEngine::enterPluginMenuMode()
{
    mode = PluginMenu;
    listeners.call (&Listener::vimModeChanged, PluginMenu);
    listeners.call (&Listener::vimContextChanged);
}

bool VimEngine::handlePluginMenuKey (const juce::KeyPress& key)
{
    if (isEscapeOrCtrlC (key))
    {
        enterNormalMode();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        if (onPluginMenuConfirm)
            onPluginMenuConfirm();
        enterNormalMode();
        return true;
    }

    auto keyChar = key.getTextCharacter();
    auto modifiers = key.getModifiers();

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
    if (modifiers.isCtrlDown()
        && (keyChar == 'd' || keyChar == 'D'
            || keyChar == 4 /* Ctrl-D */
            || key.getKeyCode() == 'd' || key.getKeyCode() == 'D'))
    {
        if (onPluginMenuScroll)
            onPluginMenuScroll (1);
        return true;
    }

    // Ctrl-U — half-page up
    if (modifiers.isCtrlDown()
        && (keyChar == 'u' || keyChar == 'U'
            || keyChar == 21 /* Ctrl-U */
            || key.getKeyCode() == 'u' || key.getKeyCode() == 'U'))
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

    // Escape / Ctrl-C closes piano roll
    if (isEscapeOrCtrlC (key))
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
    if (keyChar == 'u' || (modifiers.isCtrlDown() && keyChar == 'z'))
    {
        project.getUndoSystem().undo();
        updateClipIndexFromGridCursor();
        listeners.call (&Listener::vimContextChanged);
        return true;
    }
    if (keyChar == 'r' && modifiers.isCtrlDown())
    {
        project.getUndoSystem().redo();
        updateClipIndexFromGridCursor();
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

    // Transport — Space is play/stop (consistent with other modes)
    if (key == juce::KeyPress::spaceKey) { togglePlayStop(); return true; }

    // Enter toggles note at cursor
    if (key == juce::KeyPress::returnKey)
    {
        if (onPianoRollAddNote) onPianoRollAddNote();
        return true;
    }

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
        auto start = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::startPosition, 0)));
        auto length = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::length, 0)));
        edges.push_back (start);
        edges.push_back (start + length);
    }

    std::sort (edges.begin(), edges.end());
    edges.erase (std::unique (edges.begin(), edges.end()), edges.end());
    return edges;
}

// ── Motion resolution ───────────────────────────────────────────────────────

bool VimEngine::isMotionKey (juce_wchar c) const
{
    return c == 'h' || c == 'j' || c == 'k' || c == 'l'
        || c == '0' || c == '$' || c == 'G' || c == 'g'
        || c == 'w' || c == 'b' || c == 'e';
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
    else if (range.startTrack == range.endTrack)
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
    else
    {
        // Multi-track clipwise
        for (int t = range.startTrack; t <= range.endTrack; ++t)
        {
            if (t < 0 || t >= arrangement.getNumTracks())
                continue;

            Track track = arrangement.getTrack (t);

            if (t > range.startTrack && t < range.endTrack)
            {
                for (int c = 0; c < track.getNumClips(); ++c)
                    clips.add (track.getClip (c));
            }
            else if (t == range.startTrack)
            {
                for (int c = range.startClip; c < track.getNumClips(); ++c)
                    clips.add (track.getClip (c));
            }
            else // t == range.endTrack
            {
                int end = std::min (range.endClip, track.getNumClips() - 1);
                for (int c = 0; c <= end; ++c)
                    clips.add (track.getClip (c));
            }
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
        {
            // Move grid cursor to start of timeline
            context.setGridCursorPosition (0);
            updateClipIndexFromGridCursor();
            listeners.call (&Listener::vimContextChanged);
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
                    auto start = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::startPosition, 0)));
                    auto length = static_cast<int64_t> (static_cast<juce::int64> (clipState.getProperty (IDs::length, 0)));
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
            listeners.call (&Listener::vimContextChanged);
            break;
        }

        case 'G':
            if (count > 1 || countAccumulator > 0)
            {
                // G with count: jump to track N (1-indexed)
                int target = std::min (count, arrangement.getNumTracks()) - 1;
                if (target >= 0)
                {
                    arrangement.selectTrack (target);
                    updateClipIndexFromGridCursor();
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
            listeners.call (&Listener::vimContextChanged);
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
            listeners.call (&Listener::vimContextChanged);
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
                    auto start = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::startPosition, 0)));
                    auto length = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::length, 0)));
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
            listeners.call (&Listener::vimContextChanged);
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
    listeners.call (&Listener::vimModeChanged, Visual);
}

void VimEngine::enterVisualLineMode()
{
    visualAnchorTrack = arrangement.getSelectedTrackIndex();
    visualAnchorClip  = context.getSelectedClipIndex();
    visualAnchorGridPos = context.getGridCursorPosition();
    mode = VisualLine;

    updateVisualSelection();
    listeners.call (&Listener::vimModeChanged, VisualLine);
}

void VimEngine::exitVisualMode()
{
    context.clearVisualSelection();
    context.clearGridVisualSelection();
    mode = Normal;
    cancelOperator();
    clearPending();
    listeners.call (&Listener::vimModeChanged, Normal);
    listeners.call (&Listener::vimContextChanged);
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

    listeners.call (&Listener::vimContextChanged);
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
                auto start = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::startPosition, 0)));
                auto length = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::length, 0)));

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
            executeGridVisualYank(); // Vim always yanks before delete
            executeGridVisualDelete();
            break;
        case OpYank:
            executeGridVisualYank();
            break;
        case OpChange:
            executeGridVisualYank();
            executeGridVisualDelete();
            enterInsertMode();
            exitVisualMode();
            return; // enterInsertMode already called, don't exit twice
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
        juce::Array<juce::ValueTree> newClips;

        // Process clips overlapping [minPos, maxPos) — iterate backwards for safe removal
        for (int c = track.getNumClips() - 1; c >= 0; --c)
        {
            auto clip = track.getClip (c);
            auto clipStart  = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::startPosition, 0)));
            auto clipLength = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::length, 0)));
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
                auto origTrimStart = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::trimStart, 0)));

                // Truncate original clip to be the left part [clipStart, minPos)
                int64_t leftLength = minPos - clipStart;
                clip.setProperty (IDs::length, static_cast<juce::int64> (leftLength), &um);

                // Create right part [maxPos, clipEnd)
                // Use nullptr for properties on detached tree — appendChild with &um
                // handles undo of the whole subtree addition in one transaction
                auto rightClip = clip.createCopy();
                int64_t rightOffset = maxPos - clipStart;
                rightClip.setProperty (IDs::startPosition, static_cast<juce::int64> (maxPos), nullptr);
                rightClip.setProperty (IDs::length, static_cast<juce::int64> (clipEnd - maxPos), nullptr);
                rightClip.setProperty (IDs::trimStart, static_cast<juce::int64> (origTrimStart + rightOffset), nullptr);
                newClips.add (rightClip);
            }
            else if (keepLeft)
            {
                // Selection covers the right side — truncate to [clipStart, minPos)
                int64_t leftLength = minPos - clipStart;
                clip.setProperty (IDs::length, static_cast<juce::int64> (leftLength), &um);
            }
            else // keepRight
            {
                // Selection covers the left side — shrink to [maxPos, clipEnd)
                auto origTrimStart = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::trimStart, 0)));
                int64_t rightOffset = maxPos - clipStart;

                clip.setProperty (IDs::startPosition, static_cast<juce::int64> (maxPos), &um);
                clip.setProperty (IDs::length, static_cast<juce::int64> (clipEnd - maxPos), &um);
                clip.setProperty (IDs::trimStart, static_cast<juce::int64> (origTrimStart + rightOffset), &um);
            }
        }

        // Append any new clips created from splits
        for (auto& nc : newClips)
            track.getState().appendChild (nc, &um);
    }

    // Move cursor to start of deleted range (like vim's d motion)
    context.setGridCursorPosition (minPos);
    arrangement.selectTrack (minTrack);
    updateClipIndexFromGridCursor();
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::executeGridVisualYank()
{
    auto& gridSel = context.getGridVisualSelection();
    double sr = transport.getSampleRate();
    if (sr <= 0.0)
        return;

    int64_t minPos = std::min (gridSel.startPos, gridSel.endPos);
    int64_t maxPos = std::max (gridSel.startPos, gridSel.endPos);
    maxPos += gridSystem.getGridUnitInSamples (sr);

    int minTrack = std::min (gridSel.startTrack, gridSel.endTrack);
    int maxTrack = std::max (gridSel.startTrack, gridSel.endTrack);

    juce::Array<juce::ValueTree> clips;

    for (int t = minTrack; t <= maxTrack; ++t)
    {
        if (t < 0 || t >= arrangement.getNumTracks())
            continue;

        Track track = arrangement.getTrack (t);

        for (int c = 0; c < track.getNumClips(); ++c)
        {
            auto clip = track.getClip (c);
            auto clipStart  = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::startPosition, 0)));
            auto clipLength = static_cast<int64_t> (static_cast<juce::int64> (clip.getProperty (IDs::length, 0)));
            auto clipEnd    = clipStart + clipLength;

            if (clipStart >= maxPos || clipEnd <= minPos)
                continue;

            // Trim the yanked copy to only the portion within [minPos, maxPos)
            auto trimmedCopy = clip.createCopy();
            auto origTrimStart = static_cast<int64_t> (static_cast<juce::int64> (
                trimmedCopy.getProperty (IDs::trimStart, 0)));

            int64_t newStart  = std::max (clipStart, minPos);
            int64_t newEnd    = std::min (clipEnd, maxPos);
            int64_t trimDelta = newStart - clipStart;

            trimmedCopy.setProperty (IDs::startPosition,
                                     static_cast<juce::int64> (newStart), nullptr);
            trimmedCopy.setProperty (IDs::length,
                                     static_cast<juce::int64> (newEnd - newStart), nullptr);
            trimmedCopy.setProperty (IDs::trimStart,
                                     static_cast<juce::int64> (origTrimStart + trimDelta), nullptr);

            clips.add (trimmedCopy);
        }
    }

    context.setClipboardMulti (clips, false);
    listeners.call (&Listener::vimContextChanged);
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

bool VimEngine::handleVisualKey (const juce::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape or Ctrl-C exits visual mode
    if (isEscapeOrCtrlC (key) || keyChar == 'v')
    {
        exitVisualMode();
        return true;
    }

    // Switch to VisualLine
    if (keyChar == 'V')
    {
        mode = VisualLine;
        updateVisualSelection();
        listeners.call (&Listener::vimModeChanged, VisualLine);
        return true;
    }

    // Pending 'g' for gg
    if (pendingKey == 'g')
    {
        if (keyChar == 'g'
            && (juce::Time::currentTimeMillis() - pendingTimestamp) < pendingTimeoutMs)
        {
            clearPending();
            int count = getEffectiveCount();
            resetCounts();

            if (count > 1)
            {
                int target = std::min (count, arrangement.getNumTracks()) - 1;
                arrangement.selectTrack (target);
                updateClipIndexFromGridCursor();
            }
            else
            {
                jumpToFirstTrack();
            }
            updateVisualSelection();
            return true;
        }
        clearPending();
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

    // Digit accumulation
    if (isDigitForCount (keyChar))
    {
        accumulateDigit (keyChar);
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

    // Motion keys
    if (isMotionKey (keyChar))
    {
        int count = getEffectiveCount();
        resetCounts();
        executeMotion (keyChar, count);
        updateVisualSelection();
        return true;
    }

    // Operators
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

bool VimEngine::handleVisualLineKey (const juce::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape or Ctrl-C or re-pressing V exits
    if (isEscapeOrCtrlC (key) || keyChar == 'V')
    {
        exitVisualMode();
        return true;
    }

    // Switch to clipwise Visual
    if (keyChar == 'v')
    {
        mode = Visual;
        updateVisualSelection();
        listeners.call (&Listener::vimModeChanged, Visual);
        return true;
    }

    // Pending 'g' for gg
    if (pendingKey == 'g')
    {
        if (keyChar == 'g'
            && (juce::Time::currentTimeMillis() - pendingTimestamp) < pendingTimeoutMs)
        {
            clearPending();
            int count = getEffectiveCount();
            resetCounts();

            if (count > 1)
            {
                int target = std::min (count, arrangement.getNumTracks()) - 1;
                arrangement.selectTrack (target);
                updateClipIndexFromGridCursor();
            }
            else
            {
                jumpToFirstTrack();
            }
            updateVisualSelection();
            return true;
        }
        clearPending();
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

    // Digit accumulation
    if (isDigitForCount (keyChar))
    {
        accumulateDigit (keyChar);
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

    // Only j/k/G/gg motions are meaningful in line mode
    if (keyChar == 'j' || keyChar == 'k' || keyChar == 'G')
    {
        int count = getEffectiveCount();
        resetCounts();
        executeMotion (keyChar, count);
        updateVisualSelection();
        return true;
    }

    if (keyChar == 'g')
    {
        pendingKey = 'g';
        pendingTimestamp = juce::Time::currentTimeMillis();
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

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

    // Escape / Ctrl-C returns to normal mode
    if (isEscapeOrCtrlC (key))
    {
        enterNormalMode();
        return true;
    }

    // Undo/redo
    if (keyChar == 'u' || (modifiers.isCtrlDown() && keyChar == 'z'))
    {
        project.getUndoSystem().undo();
        updateClipIndexFromGridCursor();
        listeners.call (&Listener::vimContextChanged);
        return true;
    }
    if (keyChar == 'r' && modifiers.isCtrlDown())
    {
        project.getUndoSystem().redo();
        updateClipIndexFromGridCursor();
        listeners.call (&Listener::vimContextChanged);
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

// ── Keyboard mode ───────────────────────────────────────────────────────────

void VimEngine::enterKeyboardMode()
{
    mode = Keyboard;
    listeners.call (&Listener::vimModeChanged, Keyboard);
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::exitKeyboardMode()
{
    // Send note-off for all held notes
    for (int note : keyboardState.heldNotes)
    {
        if (onLiveMidiNote)
            onLiveMidiNote (juce::MidiMessage::noteOff (keyboardState.midiChannel, note));
    }
    keyboardState.heldNotes.clear();
    keyboardState.notifyListeners();

    mode = Normal;
    listeners.call (&Listener::vimModeChanged, Normal);
    listeners.call (&Listener::vimContextChanged);
}

bool VimEngine::handleKeyboardKey (const juce::KeyPress& key)
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
            listeners.call (&Listener::vimContextChanged);
            return true;

        case 'x': case 'X':
            keyboardState.octaveUp();
            keyboardState.notifyListeners();
            listeners.call (&Listener::vimContextChanged);
            return true;

        case 'c': case 'C':
            keyboardState.velocityDown();
            keyboardState.notifyListeners();
            listeners.call (&Listener::vimContextChanged);
            return true;

        case 'v': case 'V':
            keyboardState.velocityUp();
            keyboardState.notifyListeners();
            listeners.call (&Listener::vimContextChanged);
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
            onLiveMidiNote (juce::MidiMessage::noteOn (keyboardState.midiChannel, note,
                                                        static_cast<juce::uint8> (keyboardState.velocity)));

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

    auto keyChar = static_cast<juce_wchar> (event.unmodifiedCharacter
                                                ? event.unmodifiedCharacter
                                                : event.character);

    int note = keyboardState.keyToNote (keyChar);
    if (note >= 0 && keyboardState.heldNotes.find (note) != keyboardState.heldNotes.end())
    {
        keyboardState.heldNotes.erase (note);

        if (onLiveMidiNote)
            onLiveMidiNote (juce::MidiMessage::noteOff (keyboardState.midiChannel, note));

        keyboardState.notifyListeners();
        return true;
    }

    return false;
}

} // namespace dc
