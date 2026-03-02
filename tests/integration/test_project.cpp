#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "model/Project.h"
#include "model/Track.h"

using Catch::Matchers::WithinAbs;

TEST_CASE ("Project default state", "[integration][project]")
{
    dc::Project project;

    CHECK (project.getNumTracks() == 0);
    CHECK_THAT (project.getTempo(), WithinAbs (120.0, 1e-6));
    CHECK_THAT (project.getSampleRate(), WithinAbs (44100.0, 1e-6));
    CHECK (project.getTimeSigNumerator() == 4);
    CHECK (project.getTimeSigDenominator() == 4);
}

TEST_CASE ("Project addTrack increases count", "[integration][project]")
{
    dc::Project project;

    project.addTrack ("Track 1");
    CHECK (project.getNumTracks() == 1);

    project.addTrack ("Track 2");
    CHECK (project.getNumTracks() == 2);

    project.addTrack ("Track 3");
    CHECK (project.getNumTracks() == 3);
}

TEST_CASE ("Project addTrack returns valid PropertyTree", "[integration][project]")
{
    dc::Project project;

    auto state = project.addTrack ("My Track");
    CHECK (state.isValid());
    CHECK (state.getType() == dc::IDs::TRACK);
    CHECK (state.getProperty (dc::IDs::name).getStringOr ("") == "My Track");
}

TEST_CASE ("Project removeTrack decreases count", "[integration][project]")
{
    dc::Project project;
    project.addTrack ("A");
    project.addTrack ("B");
    project.addTrack ("C");

    REQUIRE (project.getNumTracks() == 3);

    project.removeTrack (1); // remove "B"
    CHECK (project.getNumTracks() == 2);

    dc::Track remaining0 (project.getTrack (0));
    CHECK (remaining0.getName() == "A");

    dc::Track remaining1 (project.getTrack (1));
    CHECK (remaining1.getName() == "C");
}

TEST_CASE ("Project getTrack out-of-range returns invalid tree", "[integration][project]")
{
    dc::Project project;
    project.addTrack ("Only Track");

    auto outOfRange = project.getTrack (5);
    CHECK_FALSE (outOfRange.isValid());

    auto negative = project.getTrack (-1);
    CHECK_FALSE (negative.isValid());
}

TEST_CASE ("Project getSampleRate returns configured rate", "[integration][project]")
{
    dc::Project project;

    CHECK_THAT (project.getSampleRate(), WithinAbs (44100.0, 1e-6));

    project.setSampleRate (48000.0);
    CHECK_THAT (project.getSampleRate(), WithinAbs (48000.0, 1e-6));

    project.setSampleRate (96000.0);
    CHECK_THAT (project.getSampleRate(), WithinAbs (96000.0, 1e-6));
}

TEST_CASE ("Project undo after addTrack removes track", "[integration][project]")
{
    dc::Project project;
    project.getUndoSystem().beginTransaction ("Add Track");
    project.addTrack ("Undoable Track");
    CHECK (project.getNumTracks() == 1);

    project.getUndoManager().undo();
    CHECK (project.getNumTracks() == 0);
}

TEST_CASE ("Project undo after removeTrack restores track", "[integration][project]")
{
    dc::Project project;
    project.getUndoSystem().beginTransaction ("Add");
    project.addTrack ("Track 1");
    project.addTrack ("Track 2");
    project.addTrack ("Track 3");

    REQUIRE (project.getNumTracks() == 3);

    project.getUndoSystem().beginTransaction ("Remove");
    project.removeTrack (1);
    CHECK (project.getNumTracks() == 2);

    project.getUndoManager().undo();
    CHECK (project.getNumTracks() == 3);
}

TEST_CASE ("Project setTempo updates tempo", "[integration][project]")
{
    dc::Project project;
    project.setTempo (140.0);
    CHECK_THAT (project.getTempo(), WithinAbs (140.0, 1e-6));

    project.setTempo (90.0);
    CHECK_THAT (project.getTempo(), WithinAbs (90.0, 1e-6));
}

TEST_CASE ("Project time signature", "[integration][project]")
{
    dc::Project project;

    project.setTimeSigNumerator (3);
    project.setTimeSigDenominator (8);

    CHECK (project.getTimeSigNumerator() == 3);
    CHECK (project.getTimeSigDenominator() == 8);
}

TEST_CASE ("Project getMasterBusState returns valid state", "[integration][project]")
{
    dc::Project project;
    auto masterBus = project.getMasterBusState();

    CHECK (masterBus.isValid());
    CHECK (masterBus.getType() == dc::IDs::MASTER_BUS);
    CHECK_THAT (masterBus.getProperty (dc::IDs::volume).getDoubleOr (0.0),
                WithinAbs (1.0, 1e-6));
}

TEST_CASE ("Project clipboard is accessible", "[integration][project]")
{
    dc::Project project;
    auto& clipboard = project.getClipboard();

    CHECK (clipboard.isEmpty());
}
