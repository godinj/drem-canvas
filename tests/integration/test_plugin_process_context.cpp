#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dc/plugins/ProcessContextBuilder.h"
#include "engine/TransportController.h"
#include <pluginterfaces/vst/ivstprocesscontext.h>

using Catch::Matchers::WithinAbs;
using Steinberg::Vst::ProcessContext;

// ---- ProcessContextBuilder: populates tempo from transport ----

TEST_CASE ("ProcessContextBuilder: populates tempo from transport", "[integration][plugin]")
{
    dc::TransportController transport;
    transport.setSampleRate (44100.0);
    transport.setTempo (140.0);
    transport.setTimeSig (3, 4);
    transport.play();
    transport.setPositionInSamples (44100);

    ProcessContext ctx {};
    dc::ProcessContextBuilder::populate (ctx, transport, 512);

    CHECK (ctx.tempo == 140.0);
    CHECK (ctx.timeSigNumerator == 3);
    CHECK (ctx.timeSigDenominator == 4);
    CHECK (ctx.sampleRate == 44100.0);
    CHECK (ctx.projectTimeSamples == 44100);
    CHECK ((ctx.state & ProcessContext::kPlaying) != 0);
    CHECK ((ctx.state & ProcessContext::kTempoValid) != 0);
    CHECK ((ctx.state & ProcessContext::kTimeSigValid) != 0);
}

// ---- ProcessContextBuilder: stopped transport has no kPlaying flag ----

TEST_CASE ("ProcessContextBuilder: stopped transport has no kPlaying flag", "[integration][plugin]")
{
    dc::TransportController transport;
    transport.setSampleRate (44100.0);
    transport.setTempo (120.0);
    // NOT playing — default state

    ProcessContext ctx {};
    dc::ProcessContextBuilder::populate (ctx, transport, 512);

    CHECK ((ctx.state & ProcessContext::kPlaying) == 0);
    CHECK ((ctx.state & ProcessContext::kTempoValid) != 0);
}

// ---- ProcessContextBuilder: PPQ position calculation ----

TEST_CASE ("ProcessContextBuilder: PPQ position calculation", "[integration][plugin]")
{
    dc::TransportController transport;
    transport.setSampleRate (44100.0);
    transport.setTempo (120.0);
    transport.setPositionInSamples (44100); // 1 second

    // At 120 BPM: 1 second = 2 quarter notes
    ProcessContext ctx {};
    dc::ProcessContextBuilder::populate (ctx, transport, 512);

    CHECK_THAT (ctx.projectTimeMusic, WithinAbs (2.0, 0.001));
}

// ---- ProcessContextBuilder: bar position quantization ----

TEST_CASE ("ProcessContextBuilder: bar position quantization", "[integration][plugin]")
{
    dc::TransportController transport;
    transport.setSampleRate (44100.0);
    transport.setTempo (120.0);
    transport.setTimeSig (4, 4);
    transport.setPositionInSamples (132300); // 3 seconds

    // At 120 BPM, 3 seconds = 6 quarter notes (beats)
    // In 4/4 time, beatsPerBar = 4.0 * 4 / 4 = 4.0
    // barPosition = floor(6.0 / 4.0) * 4.0 = 1 * 4.0 = 4.0
    ProcessContext ctx {};
    dc::ProcessContextBuilder::populate (ctx, transport, 512);

    CHECK_THAT (ctx.barPositionMusic, WithinAbs (4.0, 0.001));
}
