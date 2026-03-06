#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "model/TempoMap.h"

using Catch::Matchers::WithinAbs;

TEST_CASE ("TempoMap default state", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;

    CHECK (tm.getTempo() == 120.0);
    CHECK (tm.getTimeSigNumerator() == 4);
    CHECK (tm.getTimeSigDenominator() == 4);
}

TEST_CASE ("TempoMap samplesToBeats at 120 BPM, 44100 SR", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    // 120 BPM = 2 beats per second
    // 44100 samples = 1 second = 2 beats
    double beats = tm.samplesToBeats (44100, 44100.0);
    CHECK_THAT (beats, WithinAbs (2.0, 1e-9));
}

TEST_CASE ("TempoMap beatsToSamples inverse", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;

    // Forward: 2 beats at 120 BPM, 44100 SR
    int64_t samples = tm.beatsToSamples (2.0, 44100.0);
    CHECK (samples == 44100);

    // Round-trip
    double beatsBack = tm.samplesToBeats (samples, 44100.0);
    CHECK_THAT (beatsBack, WithinAbs (2.0, 1e-9));
}

TEST_CASE ("TempoMap samplesToBeats round-trip various tempos", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    const double sampleRate = 48000.0;

    SECTION ("60 BPM")
    {
        tm.setTempo (60.0);
        // 60 BPM = 1 beat/sec, 48000 samples = 1 sec = 1 beat
        CHECK_THAT (tm.samplesToBeats (48000, sampleRate), WithinAbs (1.0, 1e-9));
        CHECK (tm.beatsToSamples (1.0, sampleRate) == 48000);
    }

    SECTION ("180 BPM")
    {
        tm.setTempo (180.0);
        // 180 BPM = 3 beats/sec, 48000 samples = 1 sec = 3 beats
        CHECK_THAT (tm.samplesToBeats (48000, sampleRate), WithinAbs (3.0, 1e-9));
        CHECK (tm.beatsToSamples (3.0, sampleRate) == 48000);
    }
}

TEST_CASE ("TempoMap samplesToSeconds and secondsToSamples", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    const double sampleRate = 44100.0;

    double seconds = tm.samplesToSeconds (44100, sampleRate);
    CHECK_THAT (seconds, WithinAbs (1.0, 1e-9));

    int64_t samples = tm.secondsToSamples (1.0, sampleRate);
    CHECK (samples == 44100);
}

TEST_CASE ("TempoMap beatsToSeconds and secondsToBeats", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    // 120 BPM: 1 beat = 0.5 seconds
    CHECK_THAT (tm.beatsToSeconds (1.0), WithinAbs (0.5, 1e-9));
    CHECK_THAT (tm.secondsToBeats (0.5), WithinAbs (1.0, 1e-9));

    // 4 beats = 2 seconds
    CHECK_THAT (tm.beatsToSeconds (4.0), WithinAbs (2.0, 1e-9));
    CHECK_THAT (tm.secondsToBeats (2.0), WithinAbs (4.0, 1e-9));
}

TEST_CASE ("TempoMap samplesToBarBeat with 4/4", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    const double sampleRate = 44100.0;

    SECTION ("zero samples = bar 1, beat 1")
    {
        auto pos = tm.samplesToBarBeat (0, sampleRate);
        CHECK (pos.bar == 1);
        CHECK (pos.beat == 1);
        CHECK_THAT (pos.tick, WithinAbs (0.0, 1e-6));
    }

    SECTION ("one bar (4 beats at 120 BPM)")
    {
        // 4 beats = 2 seconds = 88200 samples
        auto pos = tm.samplesToBarBeat (88200, sampleRate);
        CHECK (pos.bar == 2);
        CHECK (pos.beat == 1);
        CHECK_THAT (pos.tick, WithinAbs (0.0, 1e-6));
    }

    SECTION ("mid-bar position")
    {
        // 2 beats = 1 second = 44100 samples
        auto pos = tm.samplesToBarBeat (44100, sampleRate);
        CHECK (pos.bar == 1);
        CHECK (pos.beat == 3); // beats 1-based: beat 3 is the 3rd beat
        CHECK_THAT (pos.tick, WithinAbs (0.0, 1e-6));
    }
}

TEST_CASE ("TempoMap samplesToBarBeat with 3/4", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    tm.setTimeSig (3, 4);
    const double sampleRate = 44100.0;

    SECTION ("one bar in 3/4 (3 beats)")
    {
        // 3 beats at 120 BPM = 1.5 seconds = 66150 samples
        auto pos = tm.samplesToBarBeat (66150, sampleRate);
        CHECK (pos.bar == 2);
        CHECK (pos.beat == 1);
        CHECK_THAT (pos.tick, WithinAbs (0.0, 1e-6));
    }

    SECTION ("beat 2 of first bar")
    {
        // 1 beat = 22050 samples
        auto pos = tm.samplesToBarBeat (22050, sampleRate);
        CHECK (pos.bar == 1);
        CHECK (pos.beat == 2);
        CHECK_THAT (pos.tick, WithinAbs (0.0, 1e-6));
    }
}

TEST_CASE ("TempoMap samplesToBarBeat with 6/8", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    tm.setTimeSig (6, 8);
    const double sampleRate = 44100.0;

    // 6/8 means 6 beats per bar (in the TempoMap's simple model)
    // At 120 BPM: each beat = 0.5 sec = 22050 samples
    // One bar = 6 beats = 3 sec = 132300 samples
    auto pos = tm.samplesToBarBeat (132300, sampleRate);
    CHECK (pos.bar == 2);
    CHECK (pos.beat == 1);
    CHECK_THAT (pos.tick, WithinAbs (0.0, 1e-6));
}

TEST_CASE ("TempoMap very large sample counts", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    const double sampleRate = 44100.0;

    // 10 hours at 44100 SR = 1,587,600,000 samples
    int64_t tenHours = static_cast<int64_t> (10.0 * 3600.0 * sampleRate);
    double beats = tm.samplesToBeats (tenHours, sampleRate);

    // 10 hours at 120 BPM = 72000 beats
    CHECK_THAT (beats, WithinAbs (72000.0, 1.0));

    auto pos = tm.samplesToBarBeat (tenHours, sampleRate);
    // 72000 beats / 4 = 18000 bars
    CHECK (pos.bar == 18001);
    CHECK (pos.beat == 1);
}

TEST_CASE ("TempoMap formatBarBeat", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    const double sampleRate = 44100.0;

    auto pos = tm.samplesToBarBeat (0, sampleRate);
    std::string formatted = tm.formatBarBeat (pos);
    CHECK (formatted == "1.1.000");
}

TEST_CASE ("TempoMap setTempo changes conversions", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    const double sampleRate = 44100.0;

    tm.setTempo (240.0); // 4 beats per second
    double beats = tm.samplesToBeats (44100, sampleRate);
    CHECK_THAT (beats, WithinAbs (4.0, 1e-9));
}

TEST_CASE ("TempoMap setTempo changes all conversion results", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    const double sampleRate = 44100.0;

    // Record conversions at default 120 BPM
    double beats120 = tm.samplesToBeats (44100, sampleRate);
    int64_t samples120 = tm.beatsToSamples (4.0, sampleRate);
    auto barBeat120 = tm.samplesToBarBeat (88200, sampleRate);

    // Change to 60 BPM (half speed)
    tm.setTempo (60.0);

    // samplesToBeats: 1 sec at 60 BPM = 1 beat (was 2 beats at 120)
    double beats60 = tm.samplesToBeats (44100, sampleRate);
    CHECK_THAT (beats60, WithinAbs (1.0, 1e-9));
    CHECK (beats60 != beats120);

    // beatsToSamples: 4 beats at 60 BPM = 4 sec = 176400 samples (was 88200)
    int64_t samples60 = tm.beatsToSamples (4.0, sampleRate);
    CHECK (samples60 == 176400);
    CHECK (samples60 != samples120);

    // samplesToBarBeat: 88200 samples at 60 BPM = 2 beats = bar 1, beat 3
    // (was bar 2, beat 1 at 120 BPM since 88200 = 4 beats)
    auto barBeat60 = tm.samplesToBarBeat (88200, sampleRate);
    CHECK (barBeat60.bar == 1);
    CHECK (barBeat60.beat == 3);
    CHECK (barBeat60.bar != barBeat120.bar);
}

TEST_CASE ("TempoMap tempo clamping at boundaries", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;

    SECTION ("minimum boundary 20 BPM")
    {
        tm.setTempo (20.0);
        CHECK_THAT (tm.getTempo(), WithinAbs (20.0, 1e-9));
    }

    SECTION ("maximum boundary 300 BPM")
    {
        tm.setTempo (300.0);
        CHECK_THAT (tm.getTempo(), WithinAbs (300.0, 1e-9));
    }

    SECTION ("below minimum clamps to 20")
    {
        tm.setTempo (5.0);
        CHECK_THAT (tm.getTempo(), WithinAbs (20.0, 1e-9));
    }

    SECTION ("above maximum clamps to 300")
    {
        tm.setTempo (500.0);
        CHECK_THAT (tm.getTempo(), WithinAbs (300.0, 1e-9));
    }

    SECTION ("value within range is preserved")
    {
        tm.setTempo (140.0);
        CHECK_THAT (tm.getTempo(), WithinAbs (140.0, 1e-9));
    }
}

TEST_CASE ("TempoMap tempo change shifts MIDI note positions", "[model_layer][tempo_map]")
{
    dc::TempoMap tm;
    const double sampleRate = 44100.0;

    // Simulate a MIDI note at beat 4.0 (start of bar 2 in 4/4)
    double noteBeat = 4.0;

    // At 120 BPM: beat 4.0 = 2 seconds = 88200 samples
    tm.setTempo (120.0);
    int64_t pos120 = tm.beatsToSamples (noteBeat, sampleRate);
    CHECK (pos120 == 88200);

    // At 60 BPM: beat 4.0 = 4 seconds = 176400 samples
    tm.setTempo (60.0);
    int64_t pos60 = tm.beatsToSamples (noteBeat, sampleRate);
    CHECK (pos60 == 176400);

    // At 240 BPM: beat 4.0 = 1 second = 44100 samples
    tm.setTempo (240.0);
    int64_t pos240 = tm.beatsToSamples (noteBeat, sampleRate);
    CHECK (pos240 == 44100);

    // Doubling tempo halves the sample position
    CHECK (pos120 == pos240 * 2);
    // Halving tempo doubles the sample position
    CHECK (pos60 == pos120 * 2);
}
