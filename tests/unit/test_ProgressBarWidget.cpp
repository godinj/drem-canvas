#include <catch2/catch_test_macros.hpp>

#include "graphics/widgets/ProgressBarWidget.h"

using dc::gfx::ProgressBarWidget;

TEST_CASE ("ProgressBarWidget default state", "[graphics][widgets]")
{
    ProgressBarWidget bar;

    SECTION ("progress is 0.0 by default")
    {
        REQUIRE (bar.getProgress() == 0.0);
    }

    SECTION ("status text is empty by default")
    {
        REQUIRE (bar.getStatusText().empty());
    }
}

TEST_CASE ("ProgressBarWidget setProgress clamps values", "[graphics][widgets]")
{
    ProgressBarWidget bar;

    SECTION ("clamps values below 0.0")
    {
        bar.setProgress (-0.5);
        REQUIRE (bar.getProgress() == 0.0);
    }

    SECTION ("clamps values above 1.0")
    {
        bar.setProgress (1.5);
        REQUIRE (bar.getProgress() == 1.0);
    }

    SECTION ("clamps large negative values")
    {
        bar.setProgress (-100.0);
        REQUIRE (bar.getProgress() == 0.0);
    }

    SECTION ("clamps large positive values")
    {
        bar.setProgress (100.0);
        REQUIRE (bar.getProgress() == 1.0);
    }
}

TEST_CASE ("ProgressBarWidget setProgress stores valid values", "[graphics][widgets]")
{
    ProgressBarWidget bar;

    SECTION ("stores 0.5")
    {
        bar.setProgress (0.5);
        REQUIRE (bar.getProgress() == 0.5);
    }

    SECTION ("stores 0.0")
    {
        bar.setProgress (0.0);
        REQUIRE (bar.getProgress() == 0.0);
    }

    SECTION ("stores 1.0")
    {
        bar.setProgress (1.0);
        REQUIRE (bar.getProgress() == 1.0);
    }

    SECTION ("stores boundary value near 0")
    {
        bar.setProgress (0.001);
        REQUIRE (bar.getProgress() == 0.001);
    }

    SECTION ("stores boundary value near 1")
    {
        bar.setProgress (0.999);
        REQUIRE (bar.getProgress() == 0.999);
    }
}

TEST_CASE ("ProgressBarWidget setStatusText", "[graphics][widgets]")
{
    ProgressBarWidget bar;

    SECTION ("stores status text")
    {
        bar.setStatusText ("Scanning...");
        REQUIRE (bar.getStatusText() == "Scanning...");
    }

    SECTION ("stores empty text")
    {
        bar.setStatusText ("Some text");
        bar.setStatusText ("");
        REQUIRE (bar.getStatusText().empty());
    }

    SECTION ("stores plugin name")
    {
        bar.setStatusText ("Scanning Vital...");
        REQUIRE (bar.getStatusText() == "Scanning Vital...");
    }
}
