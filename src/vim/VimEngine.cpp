#include "VimEngine.h"
#include "model/AudioClip.h"

namespace dc
{

VimEngine::VimEngine (Project& p, TransportController& t,
                      Arrangement& a, VimContext& c)
    : project (p), transport (t), arrangement (a), context (c)
{
}

bool VimEngine::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (mode == Normal)
        return handleNormalKey (key);

    return handleInsertKey (key);
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

bool VimEngine::handleNormalKey (const juce::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();
    auto modifiers = key.getModifiers();

    // Handle pending 'g' for gg sequence
    if (pendingKey == 'g')
    {
        if (keyChar == 'g'
            && (juce::Time::currentTimeMillis() - pendingTimestamp) < pendingTimeoutMs)
        {
            clearPending();
            jumpToFirstTrack();
            return true;
        }

        // Timeout or different key — clear pending and fall through
        clearPending();
    }

    // Escape clears any pending state
    if (key == juce::KeyPress::escapeKey)
    {
        clearPending();
        return true;
    }

    // Navigation: hjkl
    if (keyChar == 'h') { moveSelectionLeft();  return true; }
    if (keyChar == 'j') { moveSelectionDown();  return true; }
    if (keyChar == 'k') { moveSelectionUp();    return true; }
    if (keyChar == 'l') { moveSelectionRight(); return true; }

    // Transport position
    if (keyChar == '0') { jumpToSessionStart(); return true; }
    if (keyChar == '$') { jumpToSessionEnd();   return true; }

    // Track jumps
    if (keyChar == 'g')
    {
        pendingKey = 'g';
        pendingTimestamp = juce::Time::currentTimeMillis();
        listeners.call (&Listener::vimContextChanged);
        return true;
    }

    if (keyChar == 'G') { jumpToLastTrack(); return true; }

    // Transport
    if (key == juce::KeyPress::spaceKey) { togglePlayStop(); return true; }

    // Clip operations
    if (keyChar == 'x') { deleteSelectedRegions(); return true; }
    if (keyChar == 'y') { yankSelectedRegions();   return true; }
    if (keyChar == 'p') { pasteAfterPlayhead();    return true; }
    if (keyChar == 'P') { pasteBeforePlayhead();   return true; }
    if (keyChar == 's') { splitRegionAtPlayhead();  return true; }

    // Undo/redo
    if (keyChar == 'u') { project.getUndoManager().undo(); return true; }
    if (keyChar == 'r' && modifiers.isCtrlDown())
    {
        project.getUndoManager().redo();
        return true;
    }

    // Track state
    if (keyChar == 'M') { toggleMute();      return true; }
    if (keyChar == 'S') { toggleSolo();      return true; }
    if (keyChar == 'r') { toggleRecordArm(); return true; }

    // Mode switch
    if (keyChar == 'i') { enterInsertMode(); return true; }

    // Panel
    if (key == juce::KeyPress::tabKey) { cycleFocusPanel(); return true; }

    // Open item (stub)
    if (key == juce::KeyPress::returnKey) { openFocusedItem(); return true; }

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
    // Find the end of the last clip across all tracks
    int64_t maxEnd = 0;

    for (int i = 0; i < arrangement.getNumTracks(); ++i)
    {
        Track track = arrangement.getTrack (i);

        for (int c = 0; c < track.getNumClips(); ++c)
        {
            auto clipState = track.getClip (c);
            auto start = static_cast<int64_t> (clipState.getProperty (IDs::startPosition, 0));
            auto length = static_cast<int64_t> (clipState.getProperty (IDs::length, 0));
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
        track.removeClip (clipIdx, &project.getUndoManager());

        // Adjust clip selection
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

    auto clipData = context.getClipboard().createCopy();
    clipData.setProperty (IDs::startPosition,
                          static_cast<juce::int64> (transport.getPositionInSamples()),
                          nullptr);

    Track track = arrangement.getTrack (trackIdx);
    track.getState().appendChild (clipData, &project.getUndoManager());
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::pasteBeforePlayhead()
{
    if (! context.hasClipboardContent())
        return;

    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    auto clipData = context.getClipboard().createCopy();
    auto length = static_cast<int64_t> (clipData.getProperty (IDs::length, 0));
    auto pastePos = transport.getPositionInSamples() - length;
    if (pastePos < 0) pastePos = 0;

    clipData.setProperty (IDs::startPosition,
                          static_cast<juce::int64> (pastePos), nullptr);

    Track track = arrangement.getTrack (trackIdx);
    track.getState().appendChild (clipData, &project.getUndoManager());
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
    auto clipStart  = static_cast<int64_t> (clipState.getProperty (IDs::startPosition, 0));
    auto clipLength = static_cast<int64_t> (clipState.getProperty (IDs::length, 0));
    auto playhead   = transport.getPositionInSamples();

    // Only split if playhead is within the clip
    if (playhead <= clipStart || playhead >= clipStart + clipLength)
        return;

    auto splitOffset = playhead - clipStart;
    auto& um = project.getUndoManager();

    // Shorten the original clip
    clipState.setProperty (IDs::length, static_cast<juce::int64> (splitOffset), &um);
    clipState.setProperty (IDs::trimEnd,
        static_cast<juce::int64> (clipState.getProperty (IDs::trimStart, 0))
            + static_cast<juce::int64> (splitOffset), &um);

    // Create the second half
    auto newClip = clipState.createCopy();
    newClip.setProperty (IDs::startPosition, static_cast<juce::int64> (playhead), &um);
    newClip.setProperty (IDs::length, static_cast<juce::int64> (clipLength - splitOffset), &um);
    newClip.setProperty (IDs::trimStart,
        static_cast<juce::int64> (clipState.getProperty (IDs::trimStart, 0))
            + static_cast<juce::int64> (splitOffset), &um);

    track.getState().appendChild (newClip, &um);
    listeners.call (&Listener::vimContextChanged);
}

// ── Track state ─────────────────────────────────────────────────────────────

void VimEngine::toggleMute()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (idx);
    track.setMuted (! track.isMuted(), &project.getUndoManager());
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::toggleSolo()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (idx);
    track.setSolo (! track.isSolo(), &project.getUndoManager());
    listeners.call (&Listener::vimContextChanged);
}

void VimEngine::toggleRecordArm()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (idx);
    track.setArmed (! track.isArmed(), &project.getUndoManager());
    listeners.call (&Listener::vimContextChanged);
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
    clearPending();
    listeners.call (&Listener::vimModeChanged, Normal);
}

// ── Panel ───────────────────────────────────────────────────────────────────

void VimEngine::cycleFocusPanel()
{
    context.cyclePanel();
    listeners.call (&Listener::vimContextChanged);
}

// ── Stubs ───────────────────────────────────────────────────────────────────

void VimEngine::openFocusedItem()
{
    // Phase 5 stub — will open piano roll / clip editor
}

// ── Pending key helpers ─────────────────────────────────────────────────────

void VimEngine::clearPending()
{
    pendingKey = 0;
    pendingTimestamp = 0;
    listeners.call (&Listener::vimContextChanged);
}

} // namespace dc
