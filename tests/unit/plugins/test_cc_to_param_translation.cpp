#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dc/plugins/MidiCCMapper.h"
#include "dc/midi/MidiMessage.h"

using Catch::Matchers::WithinAbs;

TEST_CASE ("MidiCCMapper: controller message translates to parameter change", "[unit][plugin]")
{
    dc::MidiCCMapper mapper;
    mapper.addMapping (1, 100);  // CC1 -> ParamID 100

    auto msg = dc::MidiMessage::controllerEvent (1, 1, 64);  // ch1, CC1, value 64
    dc::ParameterChangeQueue queue;

    bool result = mapper.translateToParameterChanges (msg, 0, queue);

    CHECK (result == true);
    CHECK (queue.getParameterCount() == 1);

    auto* paramQueue = queue.getParameterData (0);
    REQUIRE (paramQueue != nullptr);
    CHECK (paramQueue->getParameterId() == 100);
}

TEST_CASE ("MidiCCMapper: unmapped CC is ignored", "[unit][plugin]")
{
    dc::MidiCCMapper mapper;
    // No mappings added

    auto msg = dc::MidiMessage::controllerEvent (1, 1, 64);
    dc::ParameterChangeQueue queue;

    bool result = mapper.translateToParameterChanges (msg, 0, queue);

    CHECK (result == false);
    CHECK (queue.getParameterCount() == 0);
}

TEST_CASE ("MidiCCMapper: pitch bend translates to kPitchBend mapping", "[unit][plugin]")
{
    dc::MidiCCMapper mapper;
    mapper.addMapping (129, 200);  // kPitchBend -> ParamID 200

    auto msg = dc::MidiMessage::pitchWheel (1, 8192);  // center value
    dc::ParameterChangeQueue queue;

    bool result = mapper.translateToParameterChanges (msg, 0, queue);

    CHECK (result == true);
    CHECK (queue.getParameterCount() == 1);

    auto* paramQueue = queue.getParameterData (0);
    REQUIRE (paramQueue != nullptr);
    CHECK (paramQueue->getParameterId() == 200);

    Steinberg::int32 sampleOffset = 0;
    Steinberg::Vst::ParamValue value = 0.0;
    paramQueue->getPoint (0, sampleOffset, value);
    CHECK_THAT (value, WithinAbs (8192.0 / 16383.0, 0.001));
}

TEST_CASE ("MidiCCMapper: CC value normalized to 0-1 range", "[unit][plugin]")
{
    dc::MidiCCMapper mapper;
    mapper.addMapping (7, 50);  // CC7 -> ParamID 50

    SECTION ("CC value 127 normalizes to 1.0")
    {
        auto msg = dc::MidiMessage::controllerEvent (1, 7, 127);
        dc::ParameterChangeQueue queue;

        mapper.translateToParameterChanges (msg, 0, queue);

        auto* paramQueue = queue.getParameterData (0);
        REQUIRE (paramQueue != nullptr);

        Steinberg::int32 sampleOffset = 0;
        Steinberg::Vst::ParamValue value = 0.0;
        paramQueue->getPoint (0, sampleOffset, value);
        CHECK_THAT (value, WithinAbs (1.0, 0.001));
    }

    SECTION ("CC value 0 normalizes to 0.0")
    {
        auto msg = dc::MidiMessage::controllerEvent (1, 7, 0);
        dc::ParameterChangeQueue queue;

        mapper.translateToParameterChanges (msg, 0, queue);

        auto* paramQueue = queue.getParameterData (0);
        REQUIRE (paramQueue != nullptr);

        Steinberg::int32 sampleOffset = 0;
        Steinberg::Vst::ParamValue value = 0.0;
        paramQueue->getPoint (0, sampleOffset, value);
        CHECK_THAT (value, WithinAbs (0.0, 0.001));
    }
}
