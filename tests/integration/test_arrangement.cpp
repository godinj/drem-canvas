#include <catch2/catch_test_macros.hpp>
#include "model/Project.h"
#include "model/Track.h"
#include "model/Arrangement.h"

TEST_CASE ("Arrangement wraps project track count", "[integration][arrangement]")
{
    dc::Project project;
    dc::Arrangement arrangement (project);

    CHECK (arrangement.getNumTracks() == 0);

    project.addTrack ("Track 1");
    CHECK (arrangement.getNumTracks() == 1);

    project.addTrack ("Track 2");
    CHECK (arrangement.getNumTracks() == 2);
}

TEST_CASE ("Arrangement addTrack creates track", "[integration][arrangement]")
{
    dc::Project project;
    dc::Arrangement arrangement (project);

    auto track = arrangement.addTrack ("New Track");
    CHECK (track.isValid());
    CHECK (track.getName() == "New Track");
    CHECK (arrangement.getNumTracks() == 1);
}

TEST_CASE ("Arrangement removeTrack removes track", "[integration][arrangement]")
{
    dc::Project project;
    dc::Arrangement arrangement (project);

    arrangement.addTrack ("A");
    arrangement.addTrack ("B");
    arrangement.addTrack ("C");

    REQUIRE (arrangement.getNumTracks() == 3);

    arrangement.removeTrack (1);
    CHECK (arrangement.getNumTracks() == 2);
}

TEST_CASE ("Arrangement track selection", "[integration][arrangement]")
{
    dc::Project project;
    dc::Arrangement arrangement (project);

    arrangement.addTrack ("Track 1");
    arrangement.addTrack ("Track 2");
    arrangement.addTrack ("Track 3");

    SECTION ("initial selection is -1 (none)")
    {
        CHECK (arrangement.getSelectedTrackIndex() == -1);
    }

    SECTION ("selectTrack sets index")
    {
        arrangement.selectTrack (1);
        CHECK (arrangement.getSelectedTrackIndex() == 1);
    }

    SECTION ("selectTrack out of range does not change selection")
    {
        arrangement.selectTrack (0);
        arrangement.selectTrack (10);
        CHECK (arrangement.getSelectedTrackIndex() == 0);
    }

    SECTION ("deselectAll resets to -1")
    {
        arrangement.selectTrack (2);
        arrangement.deselectAll();
        CHECK (arrangement.getSelectedTrackIndex() == -1);
    }
}

TEST_CASE ("Arrangement selection updates on track removal", "[integration][arrangement]")
{
    dc::Project project;
    dc::Arrangement arrangement (project);

    arrangement.addTrack ("A");
    arrangement.addTrack ("B");
    arrangement.addTrack ("C");

    SECTION ("removing selected track clears selection")
    {
        arrangement.selectTrack (1);
        arrangement.removeTrack (1);
        CHECK (arrangement.getSelectedTrackIndex() == -1);
    }

    SECTION ("removing track before selection decrements index")
    {
        arrangement.selectTrack (2);
        arrangement.removeTrack (0);
        CHECK (arrangement.getSelectedTrackIndex() == 1);
    }

    SECTION ("removing track after selection keeps index")
    {
        arrangement.selectTrack (0);
        arrangement.removeTrack (2);
        CHECK (arrangement.getSelectedTrackIndex() == 0);
    }
}

TEST_CASE ("Arrangement isTrackAudible with mute", "[integration][arrangement]")
{
    dc::Project project;
    dc::Arrangement arrangement (project);

    arrangement.addTrack ("Track 1");
    arrangement.addTrack ("Track 2");

    dc::Track t0 (project.getTrack (0));
    dc::Track t1 (project.getTrack (1));

    SECTION ("unmuted tracks are audible")
    {
        CHECK (arrangement.isTrackAudible (0));
        CHECK (arrangement.isTrackAudible (1));
    }

    SECTION ("muted track is not audible")
    {
        t0.setMuted (true);
        CHECK_FALSE (arrangement.isTrackAudible (0));
        CHECK (arrangement.isTrackAudible (1));
    }
}

TEST_CASE ("Arrangement isTrackAudible with solo", "[integration][arrangement]")
{
    dc::Project project;
    dc::Arrangement arrangement (project);

    arrangement.addTrack ("Track 1");
    arrangement.addTrack ("Track 2");
    arrangement.addTrack ("Track 3");

    dc::Track t0 (project.getTrack (0));
    dc::Track t1 (project.getTrack (1));

    SECTION ("solo'd track is audible, others are not")
    {
        t0.setSolo (true);
        CHECK (arrangement.isTrackAudible (0));
        CHECK_FALSE (arrangement.isTrackAudible (1));
        CHECK_FALSE (arrangement.isTrackAudible (2));
    }

    SECTION ("multiple solo'd tracks are all audible")
    {
        t0.setSolo (true);
        t1.setSolo (true);
        CHECK (arrangement.isTrackAudible (0));
        CHECK (arrangement.isTrackAudible (1));
        CHECK_FALSE (arrangement.isTrackAudible (2));
    }
}

TEST_CASE ("Arrangement isTrackAudible out-of-range", "[integration][arrangement]")
{
    dc::Project project;
    dc::Arrangement arrangement (project);

    arrangement.addTrack ("Track 1");

    CHECK_FALSE (arrangement.isTrackAudible (-1));
    CHECK_FALSE (arrangement.isTrackAudible (5));
}

TEST_CASE ("Arrangement moveTrack reorders tracks", "[integration][arrangement]")
{
    dc::Project project;
    dc::Arrangement arrangement (project);

    arrangement.addTrack ("A");
    arrangement.addTrack ("B");
    arrangement.addTrack ("C");

    arrangement.selectTrack (0); // select "A"
    arrangement.moveTrack (0, 2); // move "A" to position 2

    // After move: B, C, A
    CHECK (arrangement.getTrack (0).getName() == "B");
    CHECK (arrangement.getTrack (1).getName() == "C");
    CHECK (arrangement.getTrack (2).getName() == "A");

    // Selection should follow the moved track
    CHECK (arrangement.getSelectedTrackIndex() == 2);
}
