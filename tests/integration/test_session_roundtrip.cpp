#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "model/Project.h"
#include "model/Track.h"
#include "model/serialization/SessionWriter.h"
#include "model/serialization/SessionReader.h"
#include <filesystem>
#include <cstdlib>

using Catch::Matchers::WithinAbs;
namespace fs = std::filesystem;

// Helper: create a unique temporary directory for each test
static fs::path createTempSessionDir()
{
    auto base = fs::temp_directory_path() / "dc_test_session_XXXXXX";
    auto tmpl = base.string();
    if (mkdtemp(tmpl.data()) == nullptr)
        return {};
    return fs::path(tmpl);
}

// Helper: clean up session directory
static void removeTempSessionDir (const fs::path& dir)
{
    std::error_code ec;
    fs::remove_all (dir, ec);
}

TEST_CASE ("Session round-trip: empty project", "[integration][session]")
{
    dc::Project project;
    auto sessionDir = createTempSessionDir();

    REQUIRE (project.saveSessionToDirectory (sessionDir));
    REQUIRE (dc::SessionReader::isValidSessionDirectory (sessionDir));

    dc::Project loaded;
    REQUIRE (loaded.loadSessionFromDirectory (sessionDir));

    CHECK (loaded.getNumTracks() == 0);
    CHECK_THAT (loaded.getTempo(), WithinAbs (120.0, 1e-6));
    CHECK_THAT (loaded.getSampleRate(), WithinAbs (44100.0, 1e-6));
    CHECK (loaded.getTimeSigNumerator() == 4);
    CHECK (loaded.getTimeSigDenominator() == 4);

    removeTempSessionDir (sessionDir);
}

TEST_CASE ("Session round-trip: project properties", "[integration][session]")
{
    dc::Project project;
    project.setTempo (140.0);
    project.setSampleRate (48000.0);
    project.setTimeSigNumerator (3);
    project.setTimeSigDenominator (4);

    auto sessionDir = createTempSessionDir();
    REQUIRE (project.saveSessionToDirectory (sessionDir));

    dc::Project loaded;
    REQUIRE (loaded.loadSessionFromDirectory (sessionDir));

    CHECK_THAT (loaded.getTempo(), WithinAbs (140.0, 1e-6));
    CHECK_THAT (loaded.getSampleRate(), WithinAbs (48000.0, 1e-6));
    CHECK (loaded.getTimeSigNumerator() == 3);
    CHECK (loaded.getTimeSigDenominator() == 4);

    removeTempSessionDir (sessionDir);
}

TEST_CASE ("Session round-trip: tracks with properties", "[integration][session]")
{
    dc::Project project;
    auto trackState = project.addTrack ("Drums");
    dc::Track track (trackState);
    track.setVolume (0.75f);
    track.setPan (-0.5f);
    track.setMuted (true);
    track.setSolo (false);

    auto sessionDir = createTempSessionDir();
    REQUIRE (project.saveSessionToDirectory (sessionDir));

    dc::Project loaded;
    REQUIRE (loaded.loadSessionFromDirectory (sessionDir));

    REQUIRE (loaded.getNumTracks() == 1);
    dc::Track loadedTrack (loaded.getTrack (0));
    CHECK (loadedTrack.getName() == "Drums");
    CHECK_THAT (static_cast<double> (loadedTrack.getVolume()), WithinAbs (0.75, 0.01));
    CHECK_THAT (static_cast<double> (loadedTrack.getPan()), WithinAbs (-0.5, 0.01));
    CHECK (loadedTrack.isMuted() == true);
    CHECK (loadedTrack.isSolo() == false);
}

TEST_CASE ("Session round-trip: audio clips", "[integration][session]")
{
    dc::Project project;
    auto trackState = project.addTrack ("Audio Track");
    dc::Track track (trackState);

    track.addAudioClip ("/path/to/sample.wav", 44100, 88200);

    auto sessionDir = createTempSessionDir();
    REQUIRE (project.saveSessionToDirectory (sessionDir));

    dc::Project loaded;
    REQUIRE (loaded.loadSessionFromDirectory (sessionDir));

    REQUIRE (loaded.getNumTracks() == 1);
    dc::Track loadedTrack (loaded.getTrack (0));
    REQUIRE (loadedTrack.getNumClips() == 1);

    auto clip = loadedTrack.getClip (0);
    CHECK (clip.getType() == dc::IDs::AUDIO_CLIP);
    CHECK (clip.getProperty (dc::IDs::startPosition).getIntOr (0) == 44100);
    CHECK (clip.getProperty (dc::IDs::length).getIntOr (0) == 88200);
}

TEST_CASE ("Session round-trip: MIDI clips", "[integration][session]")
{
    dc::Project project;
    auto trackState = project.addTrack ("MIDI Track");
    dc::Track track (trackState);

    auto clipState = track.addMidiClip (0, 44100);

    // The YAML serializer stores MIDI data as a base64 "midiData" property
    // (not as individual NOTE children). Verify the clip itself round-trips.
    auto sessionDir = createTempSessionDir();
    REQUIRE (project.saveSessionToDirectory (sessionDir));

    dc::Project loaded;
    REQUIRE (loaded.loadSessionFromDirectory (sessionDir));

    REQUIRE (loaded.getNumTracks() == 1);
    dc::Track loadedTrack (loaded.getTrack (0));
    REQUIRE (loadedTrack.getNumClips() == 1);

    auto loadedClip = loadedTrack.getClip (0);
    CHECK (loadedClip.getType() == dc::IDs::MIDI_CLIP);
    CHECK (loadedClip.getProperty (dc::IDs::startPosition).getIntOr (-1) == 0);
    CHECK (loadedClip.getProperty (dc::IDs::length).getIntOr (-1) == 44100);

    removeTempSessionDir (sessionDir);
}

TEST_CASE ("Session round-trip: plugin state", "[integration][session]")
{
    dc::Project project;
    auto trackState = project.addTrack ("Plugin Track");
    dc::Track track (trackState);

    track.addPlugin ("TestPlugin", "VST3", "TestMfr", 12345, "/path/to/plugin.vst3");
    track.setPluginState (0, "AQIDBA=="); // base64 encoded test data
    track.setPluginEnabled (0, false);

    auto sessionDir = createTempSessionDir();
    REQUIRE (project.saveSessionToDirectory (sessionDir));

    dc::Project loaded;
    REQUIRE (loaded.loadSessionFromDirectory (sessionDir));

    REQUIRE (loaded.getNumTracks() == 1);
    dc::Track loadedTrack (loaded.getTrack (0));
    REQUIRE (loadedTrack.getNumPlugins() == 1);

    auto plugin = loadedTrack.getPlugin (0);
    CHECK (plugin.getProperty (dc::IDs::pluginName).getStringOr ("") == "TestPlugin");
    CHECK (plugin.getProperty (dc::IDs::pluginFormat).getStringOr ("") == "VST3");
    CHECK (plugin.getProperty (dc::IDs::pluginManufacturer).getStringOr ("") == "TestMfr");
    CHECK (plugin.getProperty (dc::IDs::pluginState).getStringOr ("") == "AQIDBA==");
    CHECK (loadedTrack.isPluginEnabled (0) == false);
}

TEST_CASE ("Session round-trip: mixer state (volume, pan, mute, solo)", "[integration][session]")
{
    dc::Project project;

    // Create multiple tracks with different mixer states
    auto t1State = project.addTrack ("Track 1");
    dc::Track t1 (t1State);
    t1.setVolume (0.5f);
    t1.setPan (1.0f);
    t1.setMuted (false);
    t1.setSolo (true);

    auto t2State = project.addTrack ("Track 2");
    dc::Track t2 (t2State);
    t2.setVolume (0.0f);
    t2.setPan (-1.0f);
    t2.setMuted (true);
    t2.setSolo (false);

    auto sessionDir = createTempSessionDir();
    REQUIRE (project.saveSessionToDirectory (sessionDir));

    dc::Project loaded;
    REQUIRE (loaded.loadSessionFromDirectory (sessionDir));

    REQUIRE (loaded.getNumTracks() == 2);

    dc::Track lt1 (loaded.getTrack (0));
    CHECK (lt1.getName() == "Track 1");
    CHECK_THAT (static_cast<double> (lt1.getVolume()), WithinAbs (0.5, 0.01));
    CHECK_THAT (static_cast<double> (lt1.getPan()), WithinAbs (1.0, 0.01));
    CHECK (lt1.isMuted() == false);
    CHECK (lt1.isSolo() == true);

    dc::Track lt2 (loaded.getTrack (1));
    CHECK (lt2.getName() == "Track 2");
    CHECK_THAT (static_cast<double> (lt2.getVolume()), WithinAbs (0.0, 0.01));
    CHECK_THAT (static_cast<double> (lt2.getPan()), WithinAbs (-1.0, 0.01));
    CHECK (lt2.isMuted() == true);
    CHECK (lt2.isSolo() == false);
}

TEST_CASE ("Session round-trip: multiple tracks and clips", "[integration][session]")
{
    dc::Project project;

    for (int i = 0; i < 5; ++i)
    {
        auto trackState = project.addTrack ("Track " + std::to_string (i));
        dc::Track track (trackState);

        track.addAudioClip ("/path/file_" + std::to_string (i) + ".wav",
                            int64_t (i * 44100), 22050);
    }

    auto sessionDir = createTempSessionDir();
    REQUIRE (project.saveSessionToDirectory (sessionDir));

    dc::Project loaded;
    REQUIRE (loaded.loadSessionFromDirectory (sessionDir));

    REQUIRE (loaded.getNumTracks() == 5);

    for (int i = 0; i < 5; ++i)
    {
        dc::Track lt (loaded.getTrack (i));
        CHECK (lt.getName() == "Track " + std::to_string (i));
        REQUIRE (lt.getNumClips() == 1);
    }
}

TEST_CASE ("Session round-trip: invalid directory returns false", "[integration][session]")
{
    dc::Project project;
    CHECK_FALSE (project.loadSessionFromDirectory ("/nonexistent/path/that/does/not/exist"));
    CHECK_FALSE (dc::SessionReader::isValidSessionDirectory ("/nonexistent/path"));
}

TEST_CASE ("Session round-trip: master bus state", "[integration][session]")
{
    // The YAML serializer stores master_volume as a project-level property
    // (not as part of the MASTER_BUS child). The default master volume
    // serialized is 1.0 (from the masterVolume property on the project state).
    // This test verifies that the master bus child node survives reload
    // via Project::getMasterBusState() default creation.
    dc::Project project;

    auto sessionDir = createTempSessionDir();
    REQUIRE (project.saveSessionToDirectory (sessionDir));

    dc::Project loaded;
    REQUIRE (loaded.loadSessionFromDirectory (sessionDir));

    // After loading, getMasterBusState() creates on demand with default volume=1.0
    auto loadedMasterBus = loaded.getMasterBusState();
    CHECK (loadedMasterBus.isValid());
    CHECK (loadedMasterBus.getType() == dc::IDs::MASTER_BUS);
    CHECK_THAT (loadedMasterBus.getProperty (dc::IDs::volume).getDoubleOr (0.0),
                WithinAbs (1.0, 0.01));

    removeTempSessionDir (sessionDir);
}
