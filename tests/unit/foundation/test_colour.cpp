#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <dc/foundation/types.h>

using Catch::Approx;

TEST_CASE("Colour fromRGB constructs correct ARGB", "[foundation][colour]")
{
    auto c = dc::Colour::fromRGB(255, 128, 0);
    REQUIRE(c.getAlpha() == 255);
    REQUIRE(c.getRed() == 255);
    REQUIRE(c.getGreen() == 128);
    REQUIRE(c.getBlue() == 0);
    REQUIRE(c.argb == 0xffff8000);
}

TEST_CASE("Colour fromRGB alpha defaults to 0xFF", "[foundation][colour]")
{
    auto c = dc::Colour::fromRGB(0, 0, 0);
    REQUIRE(c.getAlpha() == 255);
}

TEST_CASE("Colour fromFloat clamps values to [0,1]", "[foundation][colour]")
{
    SECTION("normal values")
    {
        auto c = dc::Colour::fromFloat(0.5f, 0.5f, 0.5f, 1.0f);
        REQUIRE(c.getRed() == 128);
        REQUIRE(c.getGreen() == 128);
        REQUIRE(c.getBlue() == 128);
        REQUIRE(c.getAlpha() == 255);
    }

    SECTION("values above 1.0 are clamped")
    {
        auto c = dc::Colour::fromFloat(2.0f, 1.5f, 3.0f, 5.0f);
        REQUIRE(c.getRed() == 255);
        REQUIRE(c.getGreen() == 255);
        REQUIRE(c.getBlue() == 255);
        REQUIRE(c.getAlpha() == 255);
    }

    SECTION("values below 0.0 are clamped")
    {
        auto c = dc::Colour::fromFloat(-1.0f, -0.5f, -2.0f, -1.0f);
        REQUIRE(c.getRed() == 0);
        REQUIRE(c.getGreen() == 0);
        REQUIRE(c.getBlue() == 0);
        REQUIRE(c.getAlpha() == 0);
    }
}

TEST_CASE("Colour fromHSV with S=0 produces grayscale", "[foundation][colour]")
{
    // When saturation is 0, hue is irrelevant; R=G=B=V*255
    auto c = dc::Colour::fromHSV(0.33f, 0.0f, 0.8f);
    uint8_t expected = static_cast<uint8_t>(0.8f * 255.0f + 0.5f);
    REQUIRE(c.getRed() == expected);
    REQUIRE(c.getGreen() == expected);
    REQUIRE(c.getBlue() == expected);
    REQUIRE(c.getAlpha() == 255);
}

TEST_CASE("Colour fromHSV produces correct colours for known hues", "[foundation][colour]")
{
    SECTION("red (H=0)")
    {
        auto c = dc::Colour::fromHSV(0.0f, 1.0f, 1.0f);
        REQUIRE(c.getRed() == 255);
        REQUIRE(c.getGreen() == 0);
        REQUIRE(c.getBlue() == 0);
    }

    SECTION("green (H=1/3)")
    {
        auto c = dc::Colour::fromHSV(1.0f / 3.0f, 1.0f, 1.0f);
        REQUIRE(c.getRed() == 0);
        REQUIRE(c.getGreen() == 255);
        REQUIRE(c.getBlue() == 0);
    }

    SECTION("blue (H=2/3)")
    {
        auto c = dc::Colour::fromHSV(2.0f / 3.0f, 1.0f, 1.0f);
        REQUIRE(c.getRed() == 0);
        REQUIRE(c.getGreen() == 0);
        REQUIRE(c.getBlue() == 255);
    }
}

TEST_CASE("Colour toHexString / fromHexString round-trip", "[foundation][colour]")
{
    SECTION("opaque colour")
    {
        auto original = dc::Colour::fromRGB(37, 37, 53);
        auto hex = original.toHexString();
        auto restored = dc::Colour::fromHexString(hex);
        REQUIRE(restored == original);
    }

    SECTION("transparent colour")
    {
        auto original = dc::Colour::fromFloat(1.0f, 0.5f, 0.0f, 0.5f);
        auto hex = original.toHexString();
        auto restored = dc::Colour::fromHexString(hex);
        REQUIRE(restored == original);
    }

    SECTION("black")
    {
        auto hex = dc::Colours::black.toHexString();
        REQUIRE(hex == "ff000000");
        REQUIRE(dc::Colour::fromHexString(hex) == dc::Colours::black);
    }

    SECTION("white")
    {
        auto hex = dc::Colours::white.toHexString();
        REQUIRE(hex == "ffffffff");
        REQUIRE(dc::Colour::fromHexString(hex) == dc::Colours::white);
    }
}

TEST_CASE("Colour fromHexString with invalid chars", "[foundation][colour]")
{
    // Invalid characters are skipped (treated as 0 nibbles effectively)
    auto c = dc::Colour::fromHexString("gg");
    // 'g' is not hex, so nothing is added — result is 0
    REQUIRE(c.argb == 0);
}

TEST_CASE("Colour brighter at white stays white", "[foundation][colour]")
{
    auto c = dc::Colours::white.brighter(0.4f);
    REQUIRE(c.getRed() == 255);
    REQUIRE(c.getGreen() == 255);
    REQUIRE(c.getBlue() == 255);
    REQUIRE(c.getAlpha() == 255);
}

TEST_CASE("Colour brighter increases channel values", "[foundation][colour]")
{
    auto c = dc::Colour::fromRGB(100, 100, 100);
    auto b = c.brighter(0.4f);
    REQUIRE(b.getRed() > 100);
    REQUIRE(b.getGreen() > 100);
    REQUIRE(b.getBlue() > 100);
}

TEST_CASE("Colour darker at black stays black", "[foundation][colour]")
{
    auto c = dc::Colours::black.darker(0.4f);
    REQUIRE(c.getRed() == 0);
    REQUIRE(c.getGreen() == 0);
    REQUIRE(c.getBlue() == 0);
    REQUIRE(c.getAlpha() == 255);
}

TEST_CASE("Colour darker decreases channel values", "[foundation][colour]")
{
    auto c = dc::Colour::fromRGB(200, 200, 200);
    auto d = c.darker(0.4f);
    REQUIRE(d.getRed() < 200);
    REQUIRE(d.getGreen() < 200);
    REQUIRE(d.getBlue() < 200);
}

TEST_CASE("Colour interpolatedWith at t=0 returns this colour", "[foundation][colour]")
{
    auto a = dc::Colour::fromRGB(100, 50, 200);
    auto b = dc::Colour::fromRGB(200, 150, 50);
    auto result = a.interpolatedWith(b, 0.0f);
    REQUIRE(result == a);
}

TEST_CASE("Colour interpolatedWith at t=1 returns other colour", "[foundation][colour]")
{
    auto a = dc::Colour::fromRGB(100, 50, 200);
    auto b = dc::Colour::fromRGB(200, 150, 50);
    auto result = a.interpolatedWith(b, 1.0f);
    REQUIRE(result == b);
}

TEST_CASE("Colour interpolatedWith at t=0.5 returns midpoint", "[foundation][colour]")
{
    auto a = dc::Colour::fromRGB(0, 0, 0);
    auto b = dc::Colour::fromRGB(200, 100, 50);
    auto mid = a.interpolatedWith(b, 0.5f);
    REQUIRE(mid.getRed() == 100);
    REQUIRE(mid.getGreen() == 50);
    REQUIRE(mid.getBlue() == 25);
}

TEST_CASE("Colour withAlpha preserves RGB channels", "[foundation][colour]")
{
    auto c = dc::Colour::fromRGB(100, 150, 200);
    auto transparent = c.withAlpha(0.5f);
    REQUIRE(transparent.getRed() == 100);
    REQUIRE(transparent.getGreen() == 150);
    REQUIRE(transparent.getBlue() == 200);
    REQUIRE(transparent.getAlpha() == 128);
}

TEST_CASE("Colour withAlpha clamps alpha", "[foundation][colour]")
{
    auto c = dc::Colour::fromRGB(100, 100, 100);
    REQUIRE(c.withAlpha(0.0f).getAlpha() == 0);
    REQUIRE(c.withAlpha(1.0f).getAlpha() == 255);
}

TEST_CASE("Colours preset spot-checks", "[foundation][colour]")
{
    REQUIRE(dc::Colours::red == dc::Colour::fromRGB(255, 0, 0));
    REQUIRE(dc::Colours::green == dc::Colour::fromRGB(0, 255, 0));
    REQUIRE(dc::Colours::blue == dc::Colour::fromRGB(0, 0, 255));
    REQUIRE(dc::Colours::white == dc::Colour::fromRGB(255, 255, 255));
    REQUIRE(dc::Colours::black == dc::Colour::fromRGB(0, 0, 0));
    REQUIRE(dc::Colours::yellow == dc::Colour::fromRGB(255, 255, 0));
    REQUIRE(dc::Colours::cyan == dc::Colour::fromRGB(0, 255, 255));
    REQUIRE(dc::Colours::magenta == dc::Colour::fromRGB(255, 0, 255));
    REQUIRE(dc::Colours::grey.argb == 0xff808080);
    REQUIRE(dc::Colours::transparentBlack.argb == 0x00000000);
    REQUIRE(dc::Colours::transparentWhite.argb == 0x00ffffff);
}

TEST_CASE("Colour default constructor is opaque black", "[foundation][colour]")
{
    dc::Colour c;
    REQUIRE(c.getRed() == 0);
    REQUIRE(c.getGreen() == 0);
    REQUIRE(c.getBlue() == 0);
    REQUIRE(c.getAlpha() == 255);
}

TEST_CASE("Colour equality and inequality operators", "[foundation][colour]")
{
    auto a = dc::Colour::fromRGB(100, 200, 50);
    auto b = dc::Colour::fromRGB(100, 200, 50);
    auto c = dc::Colour::fromRGB(100, 200, 51);
    REQUIRE(a == b);
    REQUIRE(a != c);
}

TEST_CASE("Colour getFloat accessors", "[foundation][colour]")
{
    auto c = dc::Colour::fromRGB(255, 0, 128);
    REQUIRE(c.getFloatRed() == Approx(1.0f).margin(0.005f));
    REQUIRE(c.getFloatGreen() == Approx(0.0f));
    REQUIRE(c.getFloatBlue() == Approx(128.0f / 255.0f).margin(0.005f));
    REQUIRE(c.getFloatAlpha() == Approx(1.0f).margin(0.005f));
}

TEST_CASE("Colour toSkColor returns argb value", "[foundation][colour]")
{
    auto c = dc::Colour::fromRGB(0x12, 0x34, 0x56);
    REQUIRE(c.toSkColor() == c.argb);
}
