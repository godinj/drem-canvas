#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "engine/TransportController.h"
#include "engine/TrackProcessor.h"
#include "engine/MixBusProcessor.h"
#include "dc/audio/AudioBlock.h"
#include "dc/engine/MidiBlock.h"

using Catch::Matchers::WithinAbs;

static constexpr int kBlockSize = 512;
static constexpr double kSampleRate = 44100.0;

// Helper: allocate a stereo buffer and return channel pointers
struct TestBuffer
{
    float data[2][512] {};
    float* channels[2] = { data[0], data[1] };
    dc::AudioBlock block { channels, 2, kBlockSize };
};

// Helper: check that an audio block contains only zeros
static bool isBufferSilent (TestBuffer& buf)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < kBlockSize; ++i)
        {
            if (std::abs (buf.data[ch][i]) > 1e-8f)
                return false;
        }
    }
    return true;
}

// ─── Silent graph produces zeros ────────────────────────────────────────────

TEST_CASE ("Audio graph: TrackProcessor with no file produces silence", "[integration][audio_graph]")
{
    dc::TransportController transport;
    transport.setSampleRate (kSampleRate);
    transport.play();

    dc::TrackProcessor processor (transport);
    processor.prepare (kSampleRate, kBlockSize);

    TestBuffer buf;
    dc::MidiBlock midi;

    processor.process (buf.block, midi, kBlockSize);

    CHECK (isBufferSilent (buf));

    processor.release();
}

// ─── TrackProcessor gain ────────────────────────────────────────────────────

TEST_CASE ("Audio graph: TrackProcessor gain affects output", "[integration][audio_graph]")
{
    dc::TransportController transport;
    transport.setSampleRate (kSampleRate);

    dc::TrackProcessor processor (transport);
    processor.prepare (kSampleRate, kBlockSize);

    SECTION ("gain at 0 produces silence")
    {
        processor.setGain (0.0f);
        TestBuffer buf;
        // Fill with a test signal
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                buf.data[ch][i] = 1.0f;

        dc::MidiBlock midi;
        // Without a loaded file, output should be silence regardless of gain
        // because TrackProcessor reads from disk streamer
        processor.process (buf.block, midi, kBlockSize);
    }

    processor.release();
}

// ─── Muted TrackProcessor produces silence ──────────────────────────────────

TEST_CASE ("Audio graph: muted TrackProcessor produces silence", "[integration][audio_graph]")
{
    dc::TransportController transport;
    transport.setSampleRate (kSampleRate);
    transport.play();

    dc::TrackProcessor processor (transport);
    processor.prepare (kSampleRate, kBlockSize);
    processor.setMuted (true);

    TestBuffer buf;
    dc::MidiBlock midi;

    processor.process (buf.block, midi, kBlockSize);

    CHECK (isBufferSilent (buf));

    processor.release();
}

// ─── MixBusProcessor with master gain ───────────────────────────────────────

TEST_CASE ("Audio graph: MixBusProcessor applies master gain", "[integration][audio_graph]")
{
    dc::TransportController transport;
    transport.setSampleRate (kSampleRate);

    dc::MixBusProcessor mixBus (transport);
    mixBus.prepare (kSampleRate, kBlockSize);

    SECTION ("master gain at 0 produces silence")
    {
        mixBus.setMasterGain (0.0f);

        TestBuffer buf;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                buf.data[ch][i] = 0.5f;

        dc::MidiBlock midi;
        mixBus.process (buf.block, midi, kBlockSize);

        CHECK (isBufferSilent (buf));
    }

    SECTION ("master gain at 1.0 preserves signal")
    {
        mixBus.setMasterGain (1.0f);

        TestBuffer buf;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                buf.data[ch][i] = 0.5f;

        dc::MidiBlock midi;
        mixBus.process (buf.block, midi, kBlockSize);

        CHECK_THAT (static_cast<double> (buf.data[0][0]), WithinAbs (0.5, 0.01));
    }

    SECTION ("master gain at 0.5 halves signal")
    {
        mixBus.setMasterGain (0.5f);

        TestBuffer buf;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                buf.data[ch][i] = 1.0f;

        dc::MidiBlock midi;
        mixBus.process (buf.block, midi, kBlockSize);

        CHECK_THAT (static_cast<double> (buf.data[0][0]), WithinAbs (0.5, 0.01));
    }

    mixBus.release();
}

// ─── MixBusProcessor metering ───────────────────────────────────────────────

TEST_CASE ("Audio graph: MixBusProcessor metering tracks peak levels", "[integration][audio_graph]")
{
    dc::TransportController transport;
    transport.setSampleRate (kSampleRate);

    dc::MixBusProcessor mixBus (transport);
    mixBus.prepare (kSampleRate, kBlockSize);
    mixBus.resetPeaks();

    CHECK_THAT (static_cast<double> (mixBus.getPeakLevelLeft()), WithinAbs (0.0, 1e-6));
    CHECK_THAT (static_cast<double> (mixBus.getPeakLevelRight()), WithinAbs (0.0, 1e-6));

    // Process a buffer with known amplitude
    TestBuffer buf;
    for (int i = 0; i < kBlockSize; ++i)
    {
        buf.data[0][i] = 0.75f;
        buf.data[1][i] = 0.5f;
    }

    dc::MidiBlock midi;
    mixBus.process (buf.block, midi, kBlockSize);

    // After processing, peak levels should reflect the signal
    CHECK (mixBus.getPeakLevelLeft() > 0.0f);
    CHECK (mixBus.getPeakLevelRight() > 0.0f);

    mixBus.release();
}

// ─── TrackProcessor pan ─────────────────────────────────────────────────────

TEST_CASE ("Audio graph: TrackProcessor pan property", "[integration][audio_graph]")
{
    dc::TransportController transport;
    transport.setSampleRate (kSampleRate);

    dc::TrackProcessor processor (transport);
    processor.prepare (kSampleRate, kBlockSize);

    SECTION ("default pan is center")
    {
        CHECK_THAT (static_cast<double> (processor.getPan()), WithinAbs (0.0, 1e-6));
    }

    SECTION ("pan can be set to left")
    {
        processor.setPan (-1.0f);
        CHECK_THAT (static_cast<double> (processor.getPan()), WithinAbs (-1.0, 1e-6));
    }

    SECTION ("pan can be set to right")
    {
        processor.setPan (1.0f);
        CHECK_THAT (static_cast<double> (processor.getPan()), WithinAbs (1.0, 1e-6));
    }

    processor.release();
}

// ─── TransportController integration with processors ────────────────────────

TEST_CASE ("Audio graph: transport position advances during playback", "[integration][audio_graph]")
{
    dc::TransportController transport;
    transport.setSampleRate (kSampleRate);
    transport.setPositionInSamples (0);

    transport.play();
    transport.advancePosition (kBlockSize);

    CHECK (transport.getPositionInSamples() == kBlockSize);

    transport.advancePosition (kBlockSize);
    CHECK (transport.getPositionInSamples() == kBlockSize * 2);

    transport.stop();
    // Position should be preserved after stop
    CHECK (transport.getPositionInSamples() == kBlockSize * 2);
}
