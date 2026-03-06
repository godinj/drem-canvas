#include "EditorAdapter.h"
#include "vim/ActionRegistry.h"
#include "model/Project.h"
#include "model/Arrangement.h"
#include "model/Track.h"
#include "model/AudioClip.h"
#include "model/MidiClip.h"
#include "model/Clipboard.h"
#include "model/GridSystem.h"
#include "engine/TransportController.h"
#include "utils/UndoSystem.h"
#include <algorithm>
#include <limits>
#include <vector>

namespace dc
{

EditorAdapter::EditorAdapter (Project& p, Arrangement& a,
                              TransportController& t, GridSystem& gs,
                              VimContext& c)
    : project (p), arrangement (a), transport (t), gridSystem (gs), context (c)
{
}

// ── Navigation ──────────────────────────────────────────────────────────────

void EditorAdapter::moveSelectionUp()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx > 0)
    {
        arrangement.selectTrack (idx - 1);
        updateClipIndexFromGridCursor();
        if (onContextChanged) onContextChanged();
    }
}

void EditorAdapter::moveSelectionDown()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < arrangement.getNumTracks() - 1)
    {
        arrangement.selectTrack (idx + 1);
        updateClipIndexFromGridCursor();
        if (onContextChanged) onContextChanged();
    }
}

void EditorAdapter::moveSelectionLeft()
{
    double sr = transport.getSampleRate();
    if (sr <= 0.0) return;

    int64_t pos = context.getGridCursorPosition();
    int64_t newPos = gridSystem.moveByGridUnits (pos, -1, sr);
    context.setGridCursorPosition (newPos);
    updateClipIndexFromGridCursor();
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::moveSelectionRight()
{
    double sr = transport.getSampleRate();
    if (sr <= 0.0) return;

    int64_t pos = context.getGridCursorPosition();
    int64_t newPos = gridSystem.moveByGridUnits (pos, 1, sr);
    context.setGridCursorPosition (newPos);
    updateClipIndexFromGridCursor();
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::updateClipIndexFromGridCursor()
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

void EditorAdapter::jumpToFirstTrack()
{
    if (arrangement.getNumTracks() > 0)
    {
        arrangement.selectTrack (0);
        updateClipIndexFromGridCursor();
        if (onContextChanged) onContextChanged();
    }
}

void EditorAdapter::jumpToLastTrack()
{
    int count = arrangement.getNumTracks();
    if (count > 0)
    {
        arrangement.selectTrack (count - 1);
        updateClipIndexFromGridCursor();
        if (onContextChanged) onContextChanged();
    }
}

// ── Transport ───────────────────────────────────────────────────────────────

void EditorAdapter::jumpToSessionStart()
{
    transport.setPositionInSamples (0);
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::jumpToSessionEnd()
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
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::togglePlayStop()
{
    transport.togglePlayStop();
}

// ── Clip operations ─────────────────────────────────────────────────────────

void EditorAdapter::deleteSelectedRegions (char reg)
{
    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (trackIdx);
    int clipIdx = context.getSelectedClipIndex();

    if (clipIdx >= 0 && clipIdx < track.getNumClips())
    {
        // Yank before delete (Vim semantics: x always yanks)
        std::vector<Clipboard::ClipEntry> entries;
        entries.push_back ({ track.getClip (clipIdx), 0, 0 });
        project.getClipboard().storeClips (reg, entries, false, false);

        ScopedTransaction txn (project.getUndoSystem(), "Delete Clip");
        track.removeClip (clipIdx, &project.getUndoManager());

        if (clipIdx >= track.getNumClips() && track.getNumClips() > 0)
            context.setSelectedClipIndex (track.getNumClips() - 1);

        if (onContextChanged) onContextChanged();
    }
}

void EditorAdapter::yankSelectedRegions (char reg)
{
    int trackIdx = arrangement.getSelectedTrackIndex();
    if (trackIdx < 0 || trackIdx >= arrangement.getNumTracks())
        return;

    Track track = arrangement.getTrack (trackIdx);
    int clipIdx = context.getSelectedClipIndex();

    if (clipIdx >= 0 && clipIdx < track.getNumClips())
    {
        std::vector<Clipboard::ClipEntry> entries;
        entries.push_back ({ track.getClip (clipIdx), 0, 0 });
        project.getClipboard().storeClips (reg, entries, false, true);
    }
}

// Carve a gap [gapStart, gapEnd) in existing clips on the given track,
// splitting any clip that overlaps those boundaries.
void EditorAdapter::carveGap (Track& track, int64_t gapStart, int64_t gapEnd, dc::UndoManager& um)
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

void EditorAdapter::pasteAfterPlayhead (char reg)
{
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
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::pasteBeforePlayhead (char reg)
{
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
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::splitRegionAtPlayhead()
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
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::duplicateSelectedClip()
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
    if (onContextChanged) onContextChanged();
}

// ── Track state ─────────────────────────────────────────────────────────────

void EditorAdapter::toggleMute()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Mute");
    Track track = arrangement.getTrack (idx);
    track.setMuted (! track.isMuted(), &project.getUndoManager());
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::toggleSolo()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Solo");
    Track track = arrangement.getTrack (idx);
    track.setSolo (! track.isSolo(), &project.getUndoManager());
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::toggleRecordArm()
{
    int idx = arrangement.getSelectedTrackIndex();
    if (idx < 0 || idx >= arrangement.getNumTracks())
        return;

    ScopedTransaction txn (project.getUndoSystem(), "Toggle Record Arm");
    Track track = arrangement.getTrack (idx);
    track.setArmed (! track.isArmed(), &project.getUndoManager());
    if (onContextChanged) onContextChanged();
}

// ── Grid ────────────────────────────────────────────────────────────────────

void EditorAdapter::adjustGridDivision (int delta)
{
    gridSystem.adjustGridDivision (delta);
    double sr = transport.getSampleRate();
    if (sr > 0.0)
        context.setGridCursorPosition (gridSystem.snapFloor (context.getGridCursorPosition(), sr));
    updateClipIndexFromGridCursor();
    if (onContextChanged) onContextChanged();
}

// ── Open focused item ───────────────────────────────────────────────────────

void EditorAdapter::openFocusedItem()
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

        if (onContextChanged) onContextChanged();
    }
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

ContextAdapter::MotionRange EditorAdapter::resolveMotion (char32_t key, int count) const
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
            range.startIndex = curTrack;
            range.startSecondary = 0;
            range.endIndex   = std::min (curTrack + count, numTracks - 1);
            range.endSecondary = 0;
            range.valid      = true;
            break;
        }

        case 'k': // up count tracks (linewise)
        {
            range.linewise   = true;
            range.startIndex = std::max (curTrack - count, 0);
            range.startSecondary = 0;
            range.endIndex   = curTrack;
            range.endSecondary = 0;
            range.valid      = true;
            break;
        }

        case 'h': // left count clips (clipwise, same track)
        {
            range.linewise   = false;
            range.startIndex = curTrack;
            range.endIndex   = curTrack;
            range.startSecondary = std::max (curClip - count, 0);
            range.endSecondary   = curClip;
            range.valid      = true;
            break;
        }

        case 'l': // right count clips (clipwise, same track)
        {
            Track track = arrangement.getTrack (curTrack);
            int lastClip = track.getNumClips() - 1;
            range.linewise   = false;
            range.startIndex = curTrack;
            range.endIndex   = curTrack;
            range.startSecondary = curClip;
            range.endSecondary   = std::min (curClip + count, std::max (lastClip, 0));
            range.valid      = true;
            break;
        }

        case '$': // to end of track (clipwise)
        {
            Track track = arrangement.getTrack (curTrack);
            int lastClip = track.getNumClips() - 1;
            range.linewise   = false;
            range.startIndex = curTrack;
            range.endIndex   = curTrack;
            range.startSecondary = curClip;
            range.endSecondary   = std::max (lastClip, 0);
            range.valid      = true;
            break;
        }

        case '0': // to start of track (clipwise)
        {
            range.linewise   = false;
            range.startIndex = curTrack;
            range.endIndex   = curTrack;
            range.startSecondary = 0;
            range.endSecondary   = curClip;
            range.valid      = true;
            break;
        }

        case 'G': // to last track (linewise)
        {
            range.linewise   = true;
            range.startIndex = curTrack;
            range.startSecondary = 0;
            range.endIndex   = numTracks - 1;
            range.endSecondary = 0;
            range.valid      = true;
            break;
        }

        case 'g': // from gg -- to first track (linewise)
        {
            range.linewise   = true;
            range.startIndex = 0;
            range.startSecondary = 0;
            range.endIndex   = curTrack;
            range.endSecondary = 0;
            range.valid      = true;
            break;
        }

        case 'w': // next clip boundary
        case 'b': // previous clip boundary
        case 'e': // end of current/next clip
        {
            range.linewise   = false;
            range.startIndex = curTrack;
            range.endIndex   = curTrack;
            range.startSecondary = curClip;
            range.endSecondary   = curClip;
            range.valid      = true;
            break;
        }

        default:
            break;
    }

    return range;
}

ContextAdapter::MotionRange EditorAdapter::resolveLinewiseMotion (int count) const
{
    MotionRange range;
    int curTrack  = arrangement.getSelectedTrackIndex();
    int numTracks = arrangement.getNumTracks();

    if (curTrack < 0 || numTracks == 0)
        return range; // valid == false

    range.linewise   = true;
    range.startIndex = curTrack;
    range.startSecondary = 0;
    range.endIndex   = std::min (curTrack + count - 1, numTracks - 1);
    range.endSecondary = 0;
    range.valid      = true;

    return range;
}

// ── Operator execution ──────────────────────────────────────────────────────

std::vector<EditorAdapter::ClipEntry> EditorAdapter::collectClipsForRange (const MotionRange& range) const
{
    std::vector<ClipEntry> entries;
    int baseTrack = range.startIndex;
    int64_t minStart = std::numeric_limits<int64_t>::max();

    struct RawClip { PropertyTree data; int trackIdx; int64_t startPos; };
    std::vector<RawClip> rawClips;

    if (range.linewise)
    {
        for (int t = range.startIndex; t <= range.endIndex; ++t)
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
    else if (range.startIndex == range.endIndex)
    {
        int t = range.startIndex;
        if (t < 0 || t >= arrangement.getNumTracks())
            return entries;

        Track track = arrangement.getTrack (t);
        int endClip = std::min (range.endSecondary, track.getNumClips() - 1);

        for (int c = range.startSecondary; c <= endClip; ++c)
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
        for (int t = range.startIndex; t <= range.endIndex; ++t)
        {
            if (t < 0 || t >= arrangement.getNumTracks())
                continue;

            Track track = arrangement.getTrack (t);

            int startC = (t == range.startIndex) ? range.startSecondary : 0;
            int endC   = (t == range.endIndex)
                         ? std::min (range.endSecondary, track.getNumClips() - 1)
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

void EditorAdapter::executeOperator (VimGrammar::Operator op,
                                     const MotionRange& range, char reg)
{
    if (! range.valid)
        return;

    switch (op)
    {
        case VimGrammar::OpDelete: executeDelete (range, reg); break;
        case VimGrammar::OpYank:   executeYank (range, reg);   break;
        case VimGrammar::OpChange: executeChange (range, reg); break;
        case VimGrammar::OpNone:   break;
    }
}

void EditorAdapter::executeDelete (const MotionRange& range, char reg)
{
    // Store deleted clips (Vim delete -> unnamed + "1-"9 history)
    auto internalEntries = collectClipsForRange (range);

    // Convert to Clipboard::ClipEntry
    std::vector<Clipboard::ClipEntry> clipboardEntries;
    for (auto& e : internalEntries)
        clipboardEntries.push_back ({ e.data, e.trackOffset, e.timeOffset });

    if (! clipboardEntries.empty())
        project.getClipboard().storeClips (reg, clipboardEntries, range.linewise, false);

    auto& um = project.getUndoManager();

    if (range.linewise)
    {
        // Remove all clips from tracks in range (iterate backwards)
        for (int t = range.endIndex; t >= range.startIndex; --t)
        {
            if (t < 0 || t >= arrangement.getNumTracks())
                continue;

            Track track = arrangement.getTrack (t);

            for (int c = track.getNumClips() - 1; c >= 0; --c)
                track.removeClip (c, &um);
        }

        // Select the track at startIndex (or last valid)
        int selectTrack = std::min (range.startIndex, arrangement.getNumTracks() - 1);
        if (selectTrack >= 0)
            arrangement.selectTrack (selectTrack);

        updateClipIndexFromGridCursor();
    }
    else if (range.startIndex == range.endIndex)
    {
        // Clipwise -- remove clips in range on a single track
        int t = range.startIndex;
        if (t < 0 || t >= arrangement.getNumTracks())
            return;

        Track track = arrangement.getTrack (t);

        int endClip = std::min (range.endSecondary, track.getNumClips() - 1);

        for (int c = endClip; c >= range.startSecondary; --c)
        {
            if (c >= 0 && c < track.getNumClips())
                track.removeClip (c, &um);
        }

        // Re-derive clip index from grid cursor position
        updateClipIndexFromGridCursor();
    }
    else
    {
        // Multi-track clipwise
        for (int t = range.endIndex; t >= range.startIndex; --t)
        {
            if (t < 0 || t >= arrangement.getNumTracks())
                continue;

            Track track = arrangement.getTrack (t);

            if (t > range.startIndex && t < range.endIndex)
            {
                for (int c = track.getNumClips() - 1; c >= 0; --c)
                    track.removeClip (c, &um);
            }
            else if (t == range.startIndex)
            {
                for (int c = track.getNumClips() - 1; c >= range.startSecondary; --c)
                {
                    if (c >= 0 && c < track.getNumClips())
                        track.removeClip (c, &um);
                }
            }
            else // t == range.endIndex
            {
                int end = std::min (range.endSecondary, track.getNumClips() - 1);
                for (int c = end; c >= 0; --c)
                    track.removeClip (c, &um);
            }
        }

        arrangement.selectTrack (range.startIndex);
        int remaining = arrangement.getTrack (range.startIndex).getNumClips();
        context.setSelectedClipIndex (remaining > 0 ? std::min (range.startSecondary, remaining - 1) : 0);
    }

    if (onContextChanged) onContextChanged();
}

void EditorAdapter::executeYank (const MotionRange& range, char reg)
{
    auto internalEntries = collectClipsForRange (range);

    std::vector<Clipboard::ClipEntry> clipboardEntries;
    for (auto& e : internalEntries)
        clipboardEntries.push_back ({ e.data, e.trackOffset, e.timeOffset });

    if (! clipboardEntries.empty())
        project.getClipboard().storeClips (reg, clipboardEntries, range.linewise, true);

    if (onContextChanged) onContextChanged();
}

void EditorAdapter::executeChange (const MotionRange& range, char reg)
{
    executeDelete (range, reg);
    if (onModeChanged) onModeChanged (1); // 1 = Insert mode
}

// ── Motion execution ────────────────────────────────────────────────────────

void EditorAdapter::executeMotion (char32_t key, int count)
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
            context.setGridCursorPosition (0);
            updateClipIndexFromGridCursor();
            if (onContextChanged) onContextChanged();
            break;
        }

        case '$':
        {
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
                int64_t snapped = gridSystem.snapFloor (maxEnd - 1, sr);
                context.setGridCursorPosition (snapped);
            }
            else
            {
                context.setGridCursorPosition (maxEnd);
            }
            updateClipIndexFromGridCursor();
            if (onContextChanged) onContextChanged();
            break;
        }

        case 'G':
            if (count > 1)
            {
                int target = std::min (count, arrangement.getNumTracks()) - 1;
                if (target >= 0)
                {
                    arrangement.selectTrack (target);
                    updateClipIndexFromGridCursor();
                    if (onContextChanged) onContextChanged();
                }
            }
            else
            {
                jumpToLastTrack();
            }
            break;

        case 'g':
            jumpToFirstTrack();
            break;

        case 'w':
        {
            double sr = transport.getSampleRate();
            int trackIdx = arrangement.getSelectedTrackIndex();
            auto edges = collectClipEdges (arrangement, trackIdx);
            int64_t cursorPos = context.getGridCursorPosition();

            for (int n = 0; n < count; ++n)
            {
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
            if (onContextChanged) onContextChanged();
            break;
        }

        case 'b':
        {
            double sr = transport.getSampleRate();
            int trackIdx = arrangement.getSelectedTrackIndex();
            auto edges = collectClipEdges (arrangement, trackIdx);
            int64_t cursorPos = context.getGridCursorPosition();

            for (int n = 0; n < count; ++n)
            {
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
            if (onContextChanged) onContextChanged();
            break;
        }

        case 'e':
        {
            double sr = transport.getSampleRate();
            int trackIdx = arrangement.getSelectedTrackIndex();
            int64_t cursorPos = context.getGridCursorPosition();

            if (trackIdx >= 0 && trackIdx < arrangement.getNumTracks())
            {
                Track track = arrangement.getTrack (trackIdx);

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
            if (onContextChanged) onContextChanged();
            break;
        }

        default:
            break;
    }
}

// ── Visual mode ─────────────────────────────────────────────────────────────

void EditorAdapter::enterVisualMode()
{
    visualAnchorTrack = arrangement.getSelectedTrackIndex();
    visualAnchorClip  = context.getSelectedClipIndex();
    visualAnchorGridPos = context.getGridCursorPosition();

    updateVisualSelection();
    if (onModeChanged) onModeChanged (5); // 5 = Visual
}

void EditorAdapter::enterVisualLineMode()
{
    visualAnchorTrack = arrangement.getSelectedTrackIndex();
    visualAnchorClip  = context.getSelectedClipIndex();
    visualAnchorGridPos = context.getGridCursorPosition();

    updateVisualSelection();
    if (onModeChanged) onModeChanged (6); // 6 = VisualLine
}

void EditorAdapter::exitVisualMode()
{
    context.clearVisualSelection();
    context.clearGridVisualSelection();
    if (onModeChanged) onModeChanged (0); // 0 = Normal
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::updateVisualSelection()
{
    // Legacy clip-based visual selection (for rendering compatibility)
    VimContext::VisualSelection sel;
    sel.active     = true;
    sel.linewise   = false; // will be overridden by grid sel linewise below
    sel.startTrack = visualAnchorTrack;
    sel.startClip  = visualAnchorClip;
    sel.endTrack   = arrangement.getSelectedTrackIndex();
    sel.endClip    = context.getSelectedClipIndex();
    context.setVisualSelection (sel);

    // Grid-based visual selection
    VimContext::GridVisualSelection gridSel;
    gridSel.active     = true;
    gridSel.linewise   = false; // caller determines linewise from mode
    gridSel.startTrack = visualAnchorTrack;
    gridSel.endTrack   = arrangement.getSelectedTrackIndex();
    gridSel.startPos   = visualAnchorGridPos;
    gridSel.endPos     = context.getGridCursorPosition();
    context.setGridVisualSelection (gridSel);

    if (onContextChanged) onContextChanged();
}

ContextAdapter::MotionRange EditorAdapter::getVisualRange() const
{
    MotionRange range;
    auto& gridSel = context.getGridVisualSelection();

    if (! gridSel.active)
        return range; // valid == false

    range.linewise   = gridSel.linewise;
    range.startIndex = std::min (gridSel.startTrack, gridSel.endTrack);
    range.endIndex   = std::max (gridSel.startTrack, gridSel.endTrack);

    if (gridSel.linewise)
    {
        range.startSecondary = 0;
        range.endSecondary   = 0;
    }
    else
    {
        int64_t minPos = std::min (gridSel.startPos, gridSel.endPos);
        int64_t maxPos = std::max (gridSel.startPos, gridSel.endPos);
        double sr = transport.getSampleRate();
        if (sr > 0.0)
            maxPos += gridSystem.getGridUnitInSamples (sr);

        int primaryTrack = range.startIndex;
        if (primaryTrack >= 0 && primaryTrack < arrangement.getNumTracks())
        {
            Track track = arrangement.getTrack (primaryTrack);
            int firstClip = -1, lastClip = -1;

            for (int i = 0; i < track.getNumClips(); ++i)
            {
                auto clip = track.getClip (i);
                auto start = clip.getProperty (IDs::startPosition).getIntOr (0);
                auto length = clip.getProperty (IDs::length).getIntOr (0);

                if (start < maxPos && start + length > minPos)
                {
                    if (firstClip < 0)
                        firstClip = i;
                    lastClip = i;
                }
            }

            range.startSecondary = (firstClip >= 0) ? firstClip : 0;
            range.endSecondary   = (lastClip >= 0) ? lastClip : 0;
        }
        else
        {
            range.startSecondary = 0;
            range.endSecondary   = 0;
        }
    }

    range.valid = true;
    return range;
}

void EditorAdapter::executeVisualOperator (VimGrammar::Operator op, char reg)
{
    auto& gridSel = context.getGridVisualSelection();
    if (! gridSel.active)
    {
        exitVisualMode();
        return;
    }

    if (gridSel.linewise)
    {
        auto range = getVisualRange();
        if (! range.valid)
        {
            exitVisualMode();
            return;
        }

        ScopedTransaction txn (project.getUndoSystem(),
            op == VimGrammar::OpDelete ? "Visual Delete" :
            op == VimGrammar::OpYank   ? "Visual Yank"   : "Visual Change");

        executeOperator (op, range, reg);
        exitVisualMode();
        return;
    }

    // Grid-based visual
    ScopedTransaction txn (project.getUndoSystem(),
        op == VimGrammar::OpDelete ? "Visual Delete" :
        op == VimGrammar::OpYank   ? "Visual Yank"   : "Visual Change");

    switch (op)
    {
        case VimGrammar::OpDelete:
            executeGridVisualYank (false, reg);
            executeGridVisualDelete (reg);
            break;
        case VimGrammar::OpYank:
            executeGridVisualYank (true, reg);
            break;
        case VimGrammar::OpChange:
            executeGridVisualYank (false, reg);
            executeGridVisualDelete (reg);
            if (onModeChanged) onModeChanged (1); // Insert
            exitVisualMode();
            return;
        case VimGrammar::OpNone:
            break;
    }

    exitVisualMode();
}

void EditorAdapter::executeGridVisualDelete (char /*reg*/)
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

    auto& um = project.getUndoManager();

    for (int t = maxTrack; t >= minTrack; --t)
    {
        if (t < 0 || t >= arrangement.getNumTracks())
            continue;

        Track track = arrangement.getTrack (t);

        std::vector<PropertyTree> newClips;

        for (int c = track.getNumClips() - 1; c >= 0; --c)
        {
            auto clip = track.getClip (c);
            auto clipStart  = clip.getProperty (IDs::startPosition).getIntOr (0);
            auto clipLength = clip.getProperty (IDs::length).getIntOr (0);
            auto clipEnd    = clipStart + clipLength;

            if (clipStart >= maxPos || clipEnd <= minPos)
                continue;

            bool keepLeft  = clipStart < minPos;
            bool keepRight = clipEnd > maxPos;

            if (! keepLeft && ! keepRight)
            {
                track.removeClip (c, &um);
            }
            else if (keepLeft && keepRight)
            {
                auto origTrimStart = clip.getProperty (IDs::trimStart).getIntOr (0);

                int64_t leftLength = minPos - clipStart;
                clip.setProperty (IDs::length, Variant (leftLength), &um);

                auto rightClip = clip.createDeepCopy();
                int64_t rightOffset = maxPos - clipStart;
                rightClip.setProperty (IDs::startPosition, Variant (maxPos), nullptr);
                rightClip.setProperty (IDs::length, Variant (clipEnd - maxPos), nullptr);
                rightClip.setProperty (IDs::trimStart, Variant (origTrimStart + rightOffset), nullptr);
                newClips.push_back (rightClip);
            }
            else if (keepLeft)
            {
                int64_t leftLength = minPos - clipStart;
                clip.setProperty (IDs::length, Variant (leftLength), &um);
            }
            else // keepRight
            {
                auto origTrimStart = clip.getProperty (IDs::trimStart).getIntOr (0);
                int64_t rightOffset = maxPos - clipStart;

                clip.setProperty (IDs::startPosition, Variant (maxPos), &um);
                clip.setProperty (IDs::length, Variant (clipEnd - maxPos), &um);
                clip.setProperty (IDs::trimStart, Variant (origTrimStart + rightOffset), &um);
            }
        }

        for (auto& nc : newClips)
            track.getState().addChild (nc, -1, &um);
    }

    context.setGridCursorPosition (minPos);
    arrangement.selectTrack (minTrack);
    updateClipIndexFromGridCursor();
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::executeGridVisualYank (bool isYank, char reg)
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

    std::vector<Clipboard::ClipEntry> entries;
    int64_t globalMinStart = std::numeric_limits<int64_t>::max();

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

    for (auto& raw : rawClips)
        entries.push_back ({ raw.data, raw.trackIdx - minTrack, raw.startPos - globalMinStart });

    project.getClipboard().storeClips (reg, entries, false, isYank);
    if (onContextChanged) onContextChanged();
}

void EditorAdapter::executeVisualMute()
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

void EditorAdapter::executeVisualSolo()
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

bool EditorAdapter::handleVisualKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape or Ctrl-C or re-pressing v exits visual mode
    if (key == dc::KeyCode::Escape || keyChar == 'v')
    {
        exitVisualMode();
        return true;
    }
    if (key.control)
    {
        auto c = key.getTextCharacter();
        if (c == 3 || c == 'c' || c == 'C')
        {
            exitVisualMode();
            return true;
        }
    }

    // Switch to VisualLine
    if (keyChar == 'V')
    {
        if (onModeChanged) onModeChanged (6); // VisualLine
        updateVisualSelection();
        return true;
    }

    // Grid division change in visual mode
    if (keyChar == '[')
    {
        adjustGridDivision (-1);
        updateVisualSelection();
        return true;
    }
    if (keyChar == ']')
    {
        adjustGridDivision (1);
        updateVisualSelection();
        return true;
    }

    // Navigation motions
    if (keyChar == 'j') { moveSelectionDown(); updateVisualSelection(); return true; }
    if (keyChar == 'k') { moveSelectionUp(); updateVisualSelection(); return true; }
    if (keyChar == 'h') { moveSelectionLeft(); updateVisualSelection(); return true; }
    if (keyChar == 'l') { moveSelectionRight(); updateVisualSelection(); return true; }

    // Cycle: set cycle to visual selection
    if (key.control && (keyChar == 'l' || keyChar == 'L' || keyChar == 12))
    {
        if (onSetCycleToGridVisual) onSetCycleToGridVisual();
        exitVisualMode();
        return true;
    }

    // Operators in visual mode act on selection
    if (keyChar == 'd' || keyChar == 'x') { executeVisualOperator (VimGrammar::OpDelete, '\0'); return true; }
    if (keyChar == 'y')                   { executeVisualOperator (VimGrammar::OpYank,   '\0'); return true; }
    if (keyChar == 'c')                   { executeVisualOperator (VimGrammar::OpChange, '\0'); return true; }
    if (keyChar == 'p')
    {
        executeVisualOperator (VimGrammar::OpDelete, '\0');
        pasteAfterPlayhead();
        return true;
    }

    // Track state toggles
    if (keyChar == 'M') { executeVisualMute(); return true; }
    if (keyChar == 'S') { executeVisualSolo(); return true; }

    return true; // consume all keys in visual mode
}

bool EditorAdapter::handleVisualLineKey (const dc::KeyPress& key)
{
    auto keyChar = key.getTextCharacter();

    // Escape or Ctrl-C or re-pressing V exits
    if (key == dc::KeyCode::Escape || keyChar == 'V')
    {
        exitVisualMode();
        return true;
    }
    if (key.control)
    {
        auto c = key.getTextCharacter();
        if (c == 3 || c == 'c' || c == 'C')
        {
            exitVisualMode();
            return true;
        }
    }

    // Switch to clipwise Visual
    if (keyChar == 'v')
    {
        if (onModeChanged) onModeChanged (5); // Visual
        updateVisualSelection();
        return true;
    }

    // Grid division change in visual mode
    if (keyChar == '[')
    {
        adjustGridDivision (-1);
        updateVisualSelection();
        return true;
    }
    if (keyChar == ']')
    {
        adjustGridDivision (1);
        updateVisualSelection();
        return true;
    }

    // Only j/k/G/gg motions are meaningful in line mode
    if (keyChar == 'j') { moveSelectionDown(); updateVisualSelection(); return true; }
    if (keyChar == 'k') { moveSelectionUp(); updateVisualSelection(); return true; }
    if (keyChar == 'G') { jumpToLastTrack(); updateVisualSelection(); return true; }

    // Cycle: set cycle to visual selection
    if (key.control && (keyChar == 'l' || keyChar == 'L' || keyChar == 12))
    {
        if (onSetCycleToGridVisual) onSetCycleToGridVisual();
        exitVisualMode();
        return true;
    }

    // Operators
    if (keyChar == 'd' || keyChar == 'x') { executeVisualOperator (VimGrammar::OpDelete, '\0'); return true; }
    if (keyChar == 'y')                   { executeVisualOperator (VimGrammar::OpYank,   '\0'); return true; }
    if (keyChar == 'c')                   { executeVisualOperator (VimGrammar::OpChange, '\0'); return true; }
    if (keyChar == 'p')
    {
        executeVisualOperator (VimGrammar::OpDelete, '\0');
        pasteAfterPlayhead();
        return true;
    }

    // Track state toggles
    if (keyChar == 'M') { executeVisualMute(); return true; }
    if (keyChar == 'S') { executeVisualSolo(); return true; }

    return true; // consume all keys in visual-line mode
}

// ── Action registration ─────────────────────────────────────────────────────

void EditorAdapter::registerActions (ActionRegistry& registry)
{
    registry.registerAction ({
        "nav.up", "Move Up", "Navigation", "k",
        [this]() { moveSelectionUp(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "nav.down", "Move Down", "Navigation", "j",
        [this]() { moveSelectionDown(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "nav.left", "Move Left", "Navigation", "h",
        [this]() { moveSelectionLeft(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "nav.right", "Move Right", "Navigation", "l",
        [this]() { moveSelectionRight(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "nav.first_track", "Jump to First Track", "Navigation", "gg",
        [this]() { jumpToFirstTrack(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "nav.last_track", "Jump to Last Track", "Navigation", "G",
        [this]() { jumpToLastTrack(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "transport.play_stop", "Play / Stop", "Transport", "Space",
        [this]() { togglePlayStop(); }, {}
    });

    registry.registerAction ({
        "transport.jump_start", "Jump to Start", "Transport", "0",
        [this]() { jumpToSessionStart(); }, {}
    });

    registry.registerAction ({
        "transport.jump_end", "Jump to End", "Transport", "$",
        [this]() { jumpToSessionEnd(); }, {}
    });

    registry.registerAction ({
        "track.toggle_mute", "Toggle Mute", "Track", "M",
        [this]() { toggleMute(); }, {}
    });

    registry.registerAction ({
        "track.toggle_solo", "Toggle Solo", "Track", "S",
        [this]() { toggleSolo(); }, {}
    });

    registry.registerAction ({
        "track.toggle_record_arm", "Toggle Record Arm", "Track", "r",
        [this]() { toggleRecordArm(); }, {}
    });

    registry.registerAction ({
        "edit.delete", "Delete Selected Clip", "Edit", "x",
        [this]() { deleteSelectedRegions(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "edit.yank", "Yank (Copy) Selected Clip", "Edit", "yy",
        [this]() { yankSelectedRegions(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "edit.paste_after", "Paste After Playhead", "Edit", "p",
        [this]() { pasteAfterPlayhead(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "edit.paste_before", "Paste Before Playhead", "Edit", "P",
        [this]() { pasteBeforePlayhead(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "edit.split", "Split Clip at Playhead", "Edit", "s",
        [this]() { splitRegionAtPlayhead(); },
        { VimContext::Editor }
    });

    registry.registerAction ({
        "edit.duplicate", "Duplicate Selected Clip", "Edit", "D",
        [this]() { duplicateSelectedClip(); },
        { VimContext::Editor }
    });
}

} // namespace dc
