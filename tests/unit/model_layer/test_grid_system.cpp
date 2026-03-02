#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "model/TempoMap.h"
#include "model/GridSystem.h"

using Catch::Matchers::WithinAbs;

static constexpr double SR = 44100.0;

TEST_CASE ("GridSystem default division is 1/16", "[model_layer][grid_system]")
{
    dc::TempoMap tm;
    dc::GridSystem grid (tm);

    CHECK (grid.getGridDivision() == 4);
    CHECK (grid.getGridDivisionName() == "1/16");
}

TEST_CASE ("GridSystem getGridUnitInSamples", "[model_layer][grid_system]")
{
    dc::TempoMap tm; // 120 BPM
    dc::GridSystem grid (tm);

    // At 120 BPM: one beat = 22050 samples
    // Division 4 (1/16): one grid unit = 22050 / 4 = 5512.5 -> rounded to 5513
    int64_t unit = grid.getGridUnitInSamples (SR);
    CHECK (unit == 5513);

    SECTION ("quarter note grid (division=1)")
    {
        // divisions[] = { 1, 2, 4, 8, 16 }, starting at idx=2 (div=4)
        // delta=-2 -> idx=0 -> div=1
        grid.adjustGridDivision (-2);
        CHECK (grid.getGridDivision() == 1);
        CHECK (grid.getGridUnitInSamples (SR) == 22050);
    }

    SECTION ("eighth note grid (division=2)")
    {
        // delta=-1 -> idx=1 -> div=2
        grid.adjustGridDivision (-1);
        CHECK (grid.getGridDivision() == 2);
        CHECK (grid.getGridUnitInSamples (SR) == 11025);
    }
}

TEST_CASE ("GridSystem adjustGridDivision cycles through powers of 2", "[model_layer][grid_system]")
{
    dc::TempoMap tm;
    dc::GridSystem grid (tm);

    // Start at 4 (1/16)
    CHECK (grid.getGridDivision() == 4);

    grid.adjustGridDivision (1); // -> 8 (1/32)
    CHECK (grid.getGridDivision() == 8);
    CHECK (grid.getGridDivisionName() == "1/32");

    grid.adjustGridDivision (1); // -> 16 (1/64)
    CHECK (grid.getGridDivision() == 16);
    CHECK (grid.getGridDivisionName() == "1/64");

    grid.adjustGridDivision (1); // -> clamped at 16
    CHECK (grid.getGridDivision() == 16);

    // Go back down
    grid.adjustGridDivision (-1); // -> 8
    CHECK (grid.getGridDivision() == 8);

    grid.adjustGridDivision (-1); // -> 4
    CHECK (grid.getGridDivision() == 4);

    grid.adjustGridDivision (-1); // -> 2
    CHECK (grid.getGridDivision() == 2);
    CHECK (grid.getGridDivisionName() == "1/8");

    grid.adjustGridDivision (-1); // -> 1
    CHECK (grid.getGridDivision() == 1);
    CHECK (grid.getGridDivisionName() == "1/4");

    grid.adjustGridDivision (-1); // clamped at 1
    CHECK (grid.getGridDivision() == 1);
}

TEST_CASE ("GridSystem snapFloor", "[model_layer][grid_system]")
{
    dc::TempoMap tm;
    dc::GridSystem grid (tm);

    int64_t unit = grid.getGridUnitInSamples (SR); // 5513

    SECTION ("position at grid boundary stays unchanged")
    {
        CHECK (grid.snapFloor (0, SR) == 0);
        CHECK (grid.snapFloor (unit, SR) == unit);
        CHECK (grid.snapFloor (unit * 4, SR) == unit * 4);
    }

    SECTION ("position between grid lines snaps down")
    {
        CHECK (grid.snapFloor (unit + 100, SR) == unit);
        CHECK (grid.snapFloor (unit * 2 - 1, SR) == unit);
    }

    SECTION ("negative/zero position")
    {
        CHECK (grid.snapFloor (0, SR) == 0);
        CHECK (grid.snapFloor (-100, SR) == 0);
    }
}

TEST_CASE ("GridSystem snapNearest", "[model_layer][grid_system]")
{
    dc::TempoMap tm;
    dc::GridSystem grid (tm);

    int64_t unit = grid.getGridUnitInSamples (SR); // 5513

    SECTION ("at grid boundary")
    {
        CHECK (grid.snapNearest (unit, SR) == unit);
    }

    SECTION ("just past a grid line snaps to it")
    {
        CHECK (grid.snapNearest (unit + 1, SR) == unit);
    }

    SECTION ("closer to upper boundary snaps up")
    {
        CHECK (grid.snapNearest (unit + unit - 1, SR) == unit * 2);
    }

    SECTION ("midpoint snaps to lower")
    {
        int64_t mid = unit + unit / 2;
        // When distance is equal, snapNearest uses <= so it goes to lower
        CHECK (grid.snapNearest (mid, SR) == unit);
    }
}

TEST_CASE ("GridSystem moveByGridUnits", "[model_layer][grid_system]")
{
    dc::TempoMap tm;
    dc::GridSystem grid (tm);

    int64_t unit = grid.getGridUnitInSamples (SR);

    SECTION ("move forward")
    {
        CHECK (grid.moveByGridUnits (0, 1, SR) == unit);
        CHECK (grid.moveByGridUnits (0, 4, SR) == unit * 4);
    }

    SECTION ("move backward clamped to zero")
    {
        CHECK (grid.moveByGridUnits (unit, -1, SR) == 0);
        CHECK (grid.moveByGridUnits (unit, -2, SR) == 0);
    }

    SECTION ("move backward from further position")
    {
        CHECK (grid.moveByGridUnits (unit * 5, -2, SR) == unit * 3);
    }
}

TEST_CASE ("GridSystem formatGridPosition", "[model_layer][grid_system]")
{
    dc::TempoMap tm;
    dc::GridSystem grid (tm);

    SECTION ("position zero")
    {
        std::string fmt = grid.formatGridPosition (0, SR);
        CHECK (fmt == "1.1.1");
    }

    SECTION ("one beat forward")
    {
        // One beat = 22050 samples
        std::string fmt = grid.formatGridPosition (22050, SR);
        CHECK (fmt == "1.2.1");
    }

    SECTION ("one bar forward (4 beats)")
    {
        std::string fmt = grid.formatGridPosition (88200, SR);
        CHECK (fmt == "2.1.1");
    }
}

TEST_CASE ("GridSystem with different tempos", "[model_layer][grid_system]")
{
    dc::TempoMap tm;
    tm.setTempo (60.0); // 1 beat/sec
    dc::GridSystem grid (tm);

    // Division 4 (1/16): one grid unit = 44100 / 4 = 11025
    CHECK (grid.getGridUnitInSamples (SR) == 11025);

    // Quarter-note grid
    grid.adjustGridDivision (-3); // -> 1
    CHECK (grid.getGridUnitInSamples (SR) == 44100); // one beat = one second
}
