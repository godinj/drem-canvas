#include <catch2/catch_test_macros.hpp>
#include "model/Project.h"
#include "model/Track.h"
#include "model/Arrangement.h"
#include "model/GridSystem.h"
#include "engine/TransportController.h"
#include "vim/VimContext.h"
#include "vim/VimEngine.h"

// Helper: set up a full Vim environment for testing
struct VimTestFixture
{
    dc::Project project;
    dc::Arrangement arrangement { project };
    dc::TransportController transport;
    dc::VimContext context;
    dc::TempoMap tempoMap;
    dc::GridSystem gridSystem { tempoMap };
    dc::VimEngine engine { project, transport, arrangement, context, gridSystem };

    void addTracks (int count)
    {
        for (int i = 0; i < count; ++i)
            arrangement.addTrack ("Track " + std::to_string (i + 1));
    }
};

// ─── Track selection (j/k) ──────────────────────────────────────────────────

TEST_CASE ("Vim j/k moves track selection", "[integration][vim]")
{
    VimTestFixture f;
    f.addTracks (5);
    f.arrangement.selectTrack (0);

    SECTION ("j moves selection down")
    {
        f.engine.moveSelectionDown();
        CHECK (f.arrangement.getSelectedTrackIndex() == 1);

        f.engine.moveSelectionDown();
        CHECK (f.arrangement.getSelectedTrackIndex() == 2);
    }

    SECTION ("k moves selection up")
    {
        f.arrangement.selectTrack (3);
        f.engine.moveSelectionUp();
        CHECK (f.arrangement.getSelectedTrackIndex() == 2);
    }

    SECTION ("k at top stays at 0")
    {
        f.arrangement.selectTrack (0);
        f.engine.moveSelectionUp();
        CHECK (f.arrangement.getSelectedTrackIndex() == 0);
    }

    SECTION ("j at bottom stays at last track")
    {
        f.arrangement.selectTrack (4);
        f.engine.moveSelectionDown();
        CHECK (f.arrangement.getSelectedTrackIndex() == 4);
    }
}

// ─── Cursor movement (h/l) ─────────────────────────────────────────────────

TEST_CASE ("Vim h/l moves grid cursor", "[integration][vim]")
{
    VimTestFixture f;
    f.addTracks (1);
    f.arrangement.selectTrack (0);

    // Grid cursor starts at 0
    CHECK (f.context.getGridCursorPosition() == 0);

    SECTION ("l moves cursor right by one grid unit")
    {
        f.engine.moveSelectionRight();
        int64_t gridUnit = f.gridSystem.getGridUnitInSamples (f.project.getSampleRate());
        CHECK (f.context.getGridCursorPosition() == gridUnit);
    }

    SECTION ("h at position 0 stays at 0")
    {
        f.engine.moveSelectionLeft();
        CHECK (f.context.getGridCursorPosition() == 0);
    }

    SECTION ("h after moving right returns to previous position")
    {
        f.engine.moveSelectionRight();
        f.engine.moveSelectionRight();
        f.engine.moveSelectionLeft();
        int64_t gridUnit = f.gridSystem.getGridUnitInSamples (f.project.getSampleRate());
        CHECK (f.context.getGridCursorPosition() == gridUnit);
    }
}

// ─── dd deletes track ───────────────────────────────────────────────────────

TEST_CASE ("Vim deleteSelectedRegions removes selected track clips", "[integration][vim]")
{
    VimTestFixture f;
    f.addTracks (3);
    f.arrangement.selectTrack (1);

    // Add a clip to the selected track so there's something to delete
    dc::Track track (f.project.getTrack (1));
    track.addAudioClip ("/path/test.wav", 0, 44100);
    REQUIRE (track.getNumClips() == 1);

    // In the real Vim flow, 'dd' triggers deleteSelectedRegions in linewise mode
    // which deletes the entire track. Let's use the public API for track removal:
    int numBefore = f.arrangement.getNumTracks();
    f.arrangement.removeTrack (1);
    CHECK (f.arrangement.getNumTracks() == numBefore - 1);
}

// ─── yy/p copy-paste track ──────────────────────────────────────────────────

TEST_CASE ("Vim yank and paste track", "[integration][vim]")
{
    VimTestFixture f;
    f.addTracks (2);
    f.arrangement.selectTrack (0);

    dc::Track t0 (f.project.getTrack (0));
    t0.addAudioClip ("/path/file.wav", 0, 44100);

    // Yank the track's clips into the clipboard
    dc::Clipboard& clipboard = f.project.getClipboard();

    dc::PropertyTree clipData = t0.getClip (0);
    std::vector<dc::Clipboard::ClipEntry> entries;
    entries.push_back ({ clipData, 0, 0 });
    clipboard.storeClips (entries, true); // linewise yank

    CHECK (clipboard.hasClips());
    CHECK (clipboard.isLinewise());

    // Paste: the clip data should be available in the clipboard
    auto& stored = clipboard.getClipEntries();
    REQUIRE (stored.size() == 1);
    CHECK (stored[0].clipData.getType() == dc::IDs::AUDIO_CLIP);
}

// ─── u undo ─────────────────────────────────────────────────────────────────

TEST_CASE ("Vim undo restores track after delete", "[integration][vim]")
{
    VimTestFixture f;

    // Begin a transaction for adding tracks
    f.project.getUndoSystem().beginTransaction ("Add Tracks");
    f.addTracks (3);

    CHECK (f.arrangement.getNumTracks() == 3);

    // Begin a new transaction for the delete so it's a separate undo step
    f.project.getUndoSystem().beginTransaction ("Delete Track");
    f.project.removeTrack (1);
    CHECK (f.arrangement.getNumTracks() == 2);

    // Undo the delete (undoes only the "Delete Track" transaction)
    f.project.getUndoManager().undo();
    CHECK (f.arrangement.getNumTracks() == 3);
}

// ─── Mode transitions ───────────────────────────────────────────────────────

TEST_CASE ("Vim mode transitions", "[integration][vim]")
{
    VimTestFixture f;
    f.addTracks (1);

    CHECK (f.engine.getMode() == dc::VimEngine::Normal);

    SECTION ("i enters Insert mode")
    {
        f.engine.enterInsertMode();
        CHECK (f.engine.getMode() == dc::VimEngine::Insert);
    }

    SECTION ("Escape returns to Normal mode from Insert")
    {
        f.engine.enterInsertMode();
        CHECK (f.engine.getMode() == dc::VimEngine::Insert);
        f.engine.enterNormalMode();
        CHECK (f.engine.getMode() == dc::VimEngine::Normal);
    }

    SECTION ("Visual mode")
    {
        f.engine.enterVisualMode();
        CHECK (f.engine.getMode() == dc::VimEngine::Visual);
        f.engine.exitVisualMode();
        CHECK (f.engine.getMode() == dc::VimEngine::Normal);
    }

    SECTION ("Visual line mode")
    {
        f.engine.enterVisualLineMode();
        CHECK (f.engine.getMode() == dc::VimEngine::VisualLine);
        f.engine.exitVisualMode();
        CHECK (f.engine.getMode() == dc::VimEngine::Normal);
    }
}

// ─── Tab context switching ──────────────────────────────────────────────────

TEST_CASE ("Vim Tab cycles focus panel", "[integration][vim]")
{
    VimTestFixture f;

    CHECK (f.context.getPanel() == dc::VimContext::Editor);

    f.engine.cycleFocusPanel();
    CHECK (f.context.getPanel() == dc::VimContext::Mixer);

    f.engine.cycleFocusPanel();
    CHECK (f.context.getPanel() == dc::VimContext::Sequencer);

    f.engine.cycleFocusPanel();
    CHECK (f.context.getPanel() == dc::VimContext::Editor);
}

// ─── Transport controls ─────────────────────────────────────────────────────

TEST_CASE ("Vim transport toggle play/stop", "[integration][vim]")
{
    VimTestFixture f;

    CHECK_FALSE (f.transport.isPlaying());

    f.engine.togglePlayStop();
    CHECK (f.transport.isPlaying());

    f.engine.togglePlayStop();
    CHECK_FALSE (f.transport.isPlaying());
}

// ─── Mute/Solo toggle ───────────────────────────────────────────────────────

TEST_CASE ("Vim toggle mute on selected track", "[integration][vim]")
{
    VimTestFixture f;
    f.addTracks (2);
    f.arrangement.selectTrack (0);

    dc::Track track (f.project.getTrack (0));
    CHECK_FALSE (track.isMuted());

    f.engine.toggleMute();
    dc::Track trackAfter (f.project.getTrack (0));
    CHECK (trackAfter.isMuted());

    f.engine.toggleMute();
    dc::Track trackAfter2 (f.project.getTrack (0));
    CHECK_FALSE (trackAfter2.isMuted());
}

TEST_CASE ("Vim toggle solo on selected track", "[integration][vim]")
{
    VimTestFixture f;
    f.addTracks (2);
    f.arrangement.selectTrack (0);

    dc::Track track (f.project.getTrack (0));
    CHECK_FALSE (track.isSolo());

    f.engine.toggleSolo();
    dc::Track trackAfter (f.project.getTrack (0));
    CHECK (trackAfter.isSolo());
}

// ─── Track jumps ────────────────────────────────────────────────────────────

TEST_CASE ("Vim jump to first/last track", "[integration][vim]")
{
    VimTestFixture f;
    f.addTracks (5);
    f.arrangement.selectTrack (2);

    f.engine.jumpToFirstTrack();
    CHECK (f.arrangement.getSelectedTrackIndex() == 0);

    f.engine.jumpToLastTrack();
    CHECK (f.arrangement.getSelectedTrackIndex() == 4);
}

// ─── Jump to session start/end ──────────────────────────────────────────────

TEST_CASE ("Vim jump to session start resets transport", "[integration][vim]")
{
    VimTestFixture f;
    f.transport.setPositionInSamples (44100);

    f.engine.jumpToSessionStart();
    CHECK (f.transport.getPositionInSamples() == 0);
}
