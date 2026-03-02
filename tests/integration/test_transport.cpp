#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "engine/TransportController.h"
#include <thread>

using Catch::Matchers::WithinAbs;

TEST_CASE ("TransportController default state", "[integration][transport]")
{
    dc::TransportController tc;

    CHECK_FALSE (tc.isPlaying());
    CHECK (tc.getPositionInSamples() == 0);
    CHECK_THAT (tc.getSampleRate(), WithinAbs (44100.0, 1e-6));
    CHECK_FALSE (tc.isLooping());
    CHECK_FALSE (tc.isRecordArmed());
}

TEST_CASE ("TransportController play/stop state", "[integration][transport]")
{
    dc::TransportController tc;

    tc.play();
    CHECK (tc.isPlaying());

    tc.stop();
    CHECK_FALSE (tc.isPlaying());
}

TEST_CASE ("TransportController togglePlayStop", "[integration][transport]")
{
    dc::TransportController tc;

    CHECK_FALSE (tc.isPlaying());

    tc.togglePlayStop();
    CHECK (tc.isPlaying());

    tc.togglePlayStop();
    CHECK_FALSE (tc.isPlaying());
}

TEST_CASE ("TransportController getPositionInSamples is atomic", "[integration][transport]")
{
    dc::TransportController tc;
    tc.setPositionInSamples (12345);
    CHECK (tc.getPositionInSamples() == 12345);

    // Verify we can read from another thread
    std::atomic<int64_t> readValue { 0 };
    std::thread reader ([&tc, &readValue] ()
    {
        readValue.store (tc.getPositionInSamples());
    });
    reader.join();
    CHECK (readValue.load() == 12345);
}

TEST_CASE ("TransportController setPosition updates atomically", "[integration][transport]")
{
    dc::TransportController tc;

    tc.setPositionInSamples (44100);
    CHECK (tc.getPositionInSamples() == 44100);

    tc.setPositionInSamples (0);
    CHECK (tc.getPositionInSamples() == 0);

    tc.setPositionInSamples (1000000);
    CHECK (tc.getPositionInSamples() == 1000000);
}

TEST_CASE ("TransportController position preserved after stop", "[integration][transport]")
{
    dc::TransportController tc;

    tc.setPositionInSamples (44100);
    tc.play();
    tc.advancePosition (512);
    tc.stop();

    // Position should NOT reset to 0 after stop
    CHECK (tc.getPositionInSamples() == 44100 + 512);
}

TEST_CASE ("TransportController returnToZero", "[integration][transport]")
{
    dc::TransportController tc;
    tc.setPositionInSamples (44100);
    CHECK (tc.getPositionInSamples() == 44100);

    tc.returnToZero();
    CHECK (tc.getPositionInSamples() == 0);
}

TEST_CASE ("TransportController advancePosition", "[integration][transport]")
{
    dc::TransportController tc;

    SECTION ("advances when playing")
    {
        tc.play();
        tc.advancePosition (512);
        CHECK (tc.getPositionInSamples() == 512);

        tc.advancePosition (256);
        CHECK (tc.getPositionInSamples() == 768);
    }

    SECTION ("does not advance when stopped")
    {
        tc.setPositionInSamples (1000);
        tc.stop();
        tc.advancePosition (512);
        CHECK (tc.getPositionInSamples() == 1000);
    }
}

TEST_CASE ("TransportController getPositionInSeconds", "[integration][transport]")
{
    dc::TransportController tc;
    tc.setSampleRate (44100.0);

    tc.setPositionInSamples (44100);
    CHECK_THAT (tc.getPositionInSeconds(), WithinAbs (1.0, 1e-6));

    tc.setPositionInSamples (88200);
    CHECK_THAT (tc.getPositionInSeconds(), WithinAbs (2.0, 1e-6));

    tc.setPositionInSamples (0);
    CHECK_THAT (tc.getPositionInSeconds(), WithinAbs (0.0, 1e-6));
}

TEST_CASE ("TransportController getTimeString format", "[integration][transport]")
{
    dc::TransportController tc;
    tc.setSampleRate (44100.0);
    tc.setPositionInSamples (0);

    CHECK (tc.getTimeString() == "00:00.000");

    // 1 minute 30 seconds = 90 seconds = 90 * 44100 = 3,969,000 samples
    tc.setPositionInSamples (static_cast<int64_t> (90 * 44100));
    CHECK (tc.getTimeString() == "01:30.000");
}

TEST_CASE ("TransportController loop control", "[integration][transport]")
{
    dc::TransportController tc;
    tc.setSampleRate (44100.0);

    SECTION ("loop disabled by default")
    {
        CHECK_FALSE (tc.isLooping());
    }

    SECTION ("set loop bounds")
    {
        tc.setLoopEnabled (true);
        tc.setLoopStartInSamples (44100);
        tc.setLoopEndInSamples (88200);

        CHECK (tc.isLooping());
        CHECK (tc.getLoopStartInSamples() == 44100);
        CHECK (tc.getLoopEndInSamples() == 88200);
    }

    SECTION ("loop wraps position during playback")
    {
        tc.setLoopEnabled (true);
        tc.setLoopStartInSamples (0);
        tc.setLoopEndInSamples (1000);

        tc.setPositionInSamples (900);
        tc.play();
        tc.advancePosition (200); // 900 + 200 = 1100, wraps to 0 + (1100 - 1000) = 100
        CHECK (tc.getPositionInSamples() == 100);
    }

    SECTION ("no wrap when loop disabled")
    {
        tc.setLoopEnabled (false);
        tc.setLoopStartInSamples (0);
        tc.setLoopEndInSamples (1000);

        tc.setPositionInSamples (900);
        tc.play();
        tc.advancePosition (200);
        CHECK (tc.getPositionInSamples() == 1100);
    }
}

TEST_CASE ("TransportController record arm", "[integration][transport]")
{
    dc::TransportController tc;

    CHECK_FALSE (tc.isRecordArmed());

    tc.setRecordArmed (true);
    CHECK (tc.isRecordArmed());

    tc.toggleRecordArm();
    CHECK_FALSE (tc.isRecordArmed());

    tc.toggleRecordArm();
    CHECK (tc.isRecordArmed());
}

TEST_CASE ("TransportController setSampleRate", "[integration][transport]")
{
    dc::TransportController tc;

    tc.setSampleRate (48000.0);
    CHECK_THAT (tc.getSampleRate(), WithinAbs (48000.0, 1e-6));

    tc.setSampleRate (96000.0);
    CHECK_THAT (tc.getSampleRate(), WithinAbs (96000.0, 1e-6));
}
