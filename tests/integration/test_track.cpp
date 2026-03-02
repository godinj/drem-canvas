#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "model/Project.h"
#include "model/Track.h"

using Catch::Matchers::WithinAbs;

// Helper: create a track in a project
static dc::Track createTestTrack (dc::Project& project, const std::string& name = "Test Track")
{
    auto state = project.addTrack (name);
    return dc::Track (state);
}

TEST_CASE ("Track properties: name set/get", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project, "Original Name");

    CHECK (track.getName() == "Original Name");

    track.setName ("Renamed");
    CHECK (track.getName() == "Renamed");
}

TEST_CASE ("Track properties: volume set/get", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project);

    CHECK_THAT (static_cast<double> (track.getVolume()), WithinAbs (1.0, 0.01));

    track.setVolume (0.5f);
    CHECK_THAT (static_cast<double> (track.getVolume()), WithinAbs (0.5, 0.01));

    track.setVolume (0.0f);
    CHECK_THAT (static_cast<double> (track.getVolume()), WithinAbs (0.0, 0.01));
}

TEST_CASE ("Track properties: pan set/get", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project);

    CHECK_THAT (static_cast<double> (track.getPan()), WithinAbs (0.0, 0.01));

    track.setPan (-1.0f);
    CHECK_THAT (static_cast<double> (track.getPan()), WithinAbs (-1.0, 0.01));

    track.setPan (1.0f);
    CHECK_THAT (static_cast<double> (track.getPan()), WithinAbs (1.0, 0.01));
}

TEST_CASE ("Track properties: mute/solo/armed toggle", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project);

    CHECK_FALSE (track.isMuted());
    CHECK_FALSE (track.isSolo());
    CHECK_FALSE (track.isArmed());

    track.setMuted (true);
    CHECK (track.isMuted());

    track.setSolo (true);
    CHECK (track.isSolo());

    track.setArmed (true);
    CHECK (track.isArmed());

    track.setMuted (false);
    CHECK_FALSE (track.isMuted());
}

TEST_CASE ("Track addAudioClip creates clip at position", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project);

    CHECK (track.getNumClips() == 0);

    auto clip = track.addAudioClip ("/path/to/audio.wav", 44100, 88200);
    CHECK (track.getNumClips() == 1);
    CHECK (clip.isValid());
    CHECK (clip.getType() == dc::IDs::AUDIO_CLIP);
    CHECK (clip.getProperty (dc::IDs::sourceFile).getStringOr ("") == "/path/to/audio.wav");
    CHECK (clip.getProperty (dc::IDs::startPosition).getIntOr (0) == 44100);
    CHECK (clip.getProperty (dc::IDs::length).getIntOr (0) == 88200);
}

TEST_CASE ("Track addMidiClip creates clip at position", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project);

    auto clip = track.addMidiClip (0, 44100);
    CHECK (track.getNumClips() == 1);
    CHECK (clip.isValid());
    CHECK (clip.getType() == dc::IDs::MIDI_CLIP);
    CHECK (clip.getProperty (dc::IDs::startPosition).getIntOr (-1) == 0);
    CHECK (clip.getProperty (dc::IDs::length).getIntOr (-1) == 44100);
}

TEST_CASE ("Track removeClip removes clip and shifts indices", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project);

    track.addAudioClip ("/path/a.wav", 0, 1000);
    track.addAudioClip ("/path/b.wav", 1000, 1000);
    track.addAudioClip ("/path/c.wav", 2000, 1000);

    REQUIRE (track.getNumClips() == 3);

    track.removeClip (1); // remove "b"
    CHECK (track.getNumClips() == 2);

    auto remaining0 = track.getClip (0);
    CHECK (remaining0.getProperty (dc::IDs::sourceFile).getStringOr ("") == "/path/a.wav");

    auto remaining1 = track.getClip (1);
    CHECK (remaining1.getProperty (dc::IDs::sourceFile).getStringOr ("") == "/path/c.wav");
}

TEST_CASE ("Track getClip out-of-range returns invalid", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project);

    auto invalid = track.getClip (0);
    CHECK_FALSE (invalid.isValid());
}

TEST_CASE ("Track plugin chain management", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project);

    CHECK (track.getNumPlugins() == 0);

    SECTION ("addPlugin creates plugin in chain")
    {
        auto plugin = track.addPlugin ("EQ", "VST3", "FabFilter", 1001, "/path/eq.vst3");
        CHECK (track.getNumPlugins() == 1);
        CHECK (plugin.isValid());
        CHECK (plugin.getProperty (dc::IDs::pluginName).getStringOr ("") == "EQ");
    }

    SECTION ("removePlugin removes from chain")
    {
        track.addPlugin ("EQ", "VST3", "Fab", 1, "/a");
        track.addPlugin ("Comp", "VST3", "Fab", 2, "/b");
        REQUIRE (track.getNumPlugins() == 2);

        track.removePlugin (0);
        CHECK (track.getNumPlugins() == 1);
        CHECK (track.getPlugin (0).getProperty (dc::IDs::pluginName).getStringOr ("") == "Comp");
    }

    SECTION ("pluginEnabled state")
    {
        track.addPlugin ("EQ", "VST3", "Fab", 1, "/a");
        CHECK (track.isPluginEnabled (0) == true);

        track.setPluginEnabled (0, false);
        CHECK (track.isPluginEnabled (0) == false);
    }

    SECTION ("pluginState (base64)")
    {
        track.addPlugin ("EQ", "VST3", "Fab", 1, "/a");
        track.setPluginState (0, "SGVsbG8=");
        CHECK (track.getPlugin (0).getProperty (dc::IDs::pluginState).getStringOr ("") == "SGVsbG8=");
    }
}

TEST_CASE ("Track movePlugin reorders chain", "[integration][track]")
{
    dc::Project project;
    auto track = createTestTrack (project);

    track.addPlugin ("A", "VST3", "Mfr", 1, "/a");
    track.addPlugin ("B", "VST3", "Mfr", 2, "/b");
    track.addPlugin ("C", "VST3", "Mfr", 3, "/c");

    REQUIRE (track.getNumPlugins() == 3);
    CHECK (track.getPlugin (0).getProperty (dc::IDs::pluginName).getStringOr ("") == "A");
    CHECK (track.getPlugin (1).getProperty (dc::IDs::pluginName).getStringOr ("") == "B");
    CHECK (track.getPlugin (2).getProperty (dc::IDs::pluginName).getStringOr ("") == "C");

    track.movePlugin (0, 2); // Move A to position 2

    CHECK (track.getPlugin (0).getProperty (dc::IDs::pluginName).getStringOr ("") == "B");
    CHECK (track.getPlugin (1).getProperty (dc::IDs::pluginName).getStringOr ("") == "C");
    CHECK (track.getPlugin (2).getProperty (dc::IDs::pluginName).getStringOr ("") == "A");
}
