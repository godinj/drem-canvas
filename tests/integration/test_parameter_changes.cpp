#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dc/plugins/ParameterChangeQueue.h"

using Catch::Matchers::WithinAbs;

// ─── ParameterChangeQueue ────────────────────────────────────────────────────

TEST_CASE ("ParameterChangeQueue: addParameterData creates entries", "[integration][plugin]")
{
    dc::ParameterChangeQueue queue;

    Steinberg::int32 index = -1;
    auto* paramQueue = queue.addParameterData (42, index);

    CHECK (paramQueue != nullptr);
    CHECK (index == 0);
    CHECK (queue.getParameterCount() == 1);
}

TEST_CASE ("ParameterChangeQueue: same param reuses existing entry", "[integration][plugin]")
{
    dc::ParameterChangeQueue queue;

    Steinberg::int32 index1 = -1;
    auto* q1 = queue.addParameterData (42, index1);

    Steinberg::int32 index2 = -1;
    auto* q2 = queue.addParameterData (42, index2);

    CHECK (q1 == q2);
    CHECK (index1 == index2);
    CHECK (queue.getParameterCount() == 1);
}

TEST_CASE ("ParameterChangeQueue: multiple params tracked separately", "[integration][plugin]")
{
    dc::ParameterChangeQueue queue;

    Steinberg::int32 index1 = -1;
    queue.addParameterData (42, index1);

    Steinberg::int32 index2 = -1;
    queue.addParameterData (99, index2);

    CHECK (index1 != index2);
    CHECK (queue.getParameterCount() == 2);
}

TEST_CASE ("ParameterChangeQueue: clear resets state", "[integration][plugin]")
{
    dc::ParameterChangeQueue queue;

    Steinberg::int32 index = -1;
    queue.addParameterData (42, index);
    queue.addParameterData (99, index);

    CHECK (queue.getParameterCount() == 2);

    queue.clear();

    CHECK (queue.getParameterCount() == 0);
}

// ─── ParamValueQueue ─────────────────────────────────────────────────────────

TEST_CASE ("ParamValueQueue: addPoint and getPoint round-trip", "[integration][plugin]")
{
    dc::ParamValueQueue pvq;
    pvq.setParameterId (7);

    Steinberg::int32 pointIndex = -1;
    auto result = pvq.addPoint (100, 0.5, pointIndex);

    CHECK (result == Steinberg::kResultOk);
    CHECK (pvq.getPointCount() == 1);
    CHECK (pointIndex == 0);

    Steinberg::int32 sampleOffset = 0;
    Steinberg::Vst::ParamValue value = 0.0;
    result = pvq.getPoint (0, sampleOffset, value);

    CHECK (result == Steinberg::kResultOk);
    CHECK (sampleOffset == 100);
    CHECK_THAT (value, WithinAbs (0.5, 0.001));
}
