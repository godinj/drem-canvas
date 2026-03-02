#include <catch2/catch_test_macros.hpp>
#include <climits>
#include <cstring>
#include <dc/plugins/PluginDescription.h>

// ──────────────────────────────────────────────────────────────
// hexStringToUid
// ──────────────────────────────────────────────────────────────

TEST_CASE("hexStringToUid valid lowercase 32-char hex", "[plugin_description]")
{
    char uid[16];
    std::string hex = "0123456789abcdef0123456789abcdef";
    REQUIRE(dc::PluginDescription::hexStringToUid(hex, uid) == true);

    REQUIRE(static_cast<unsigned char>(uid[0])  == 0x01);
    REQUIRE(static_cast<unsigned char>(uid[1])  == 0x23);
    REQUIRE(static_cast<unsigned char>(uid[2])  == 0x45);
    REQUIRE(static_cast<unsigned char>(uid[3])  == 0x67);
    REQUIRE(static_cast<unsigned char>(uid[4])  == 0x89);
    REQUIRE(static_cast<unsigned char>(uid[5])  == 0xab);
    REQUIRE(static_cast<unsigned char>(uid[6])  == 0xcd);
    REQUIRE(static_cast<unsigned char>(uid[7])  == 0xef);
    REQUIRE(static_cast<unsigned char>(uid[8])  == 0x01);
    REQUIRE(static_cast<unsigned char>(uid[9])  == 0x23);
    REQUIRE(static_cast<unsigned char>(uid[10]) == 0x45);
    REQUIRE(static_cast<unsigned char>(uid[11]) == 0x67);
    REQUIRE(static_cast<unsigned char>(uid[12]) == 0x89);
    REQUIRE(static_cast<unsigned char>(uid[13]) == 0xab);
    REQUIRE(static_cast<unsigned char>(uid[14]) == 0xcd);
    REQUIRE(static_cast<unsigned char>(uid[15]) == 0xef);
}

TEST_CASE("hexStringToUid valid uppercase hex", "[plugin_description]")
{
    char uid[16];
    std::string hex = "0123456789ABCDEF0123456789ABCDEF";
    REQUIRE(dc::PluginDescription::hexStringToUid(hex, uid) == true);

    REQUIRE(static_cast<unsigned char>(uid[5])  == 0xAB);
    REQUIRE(static_cast<unsigned char>(uid[6])  == 0xCD);
    REQUIRE(static_cast<unsigned char>(uid[7])  == 0xEF);
    REQUIRE(static_cast<unsigned char>(uid[13]) == 0xAB);
    REQUIRE(static_cast<unsigned char>(uid[14]) == 0xCD);
    REQUIRE(static_cast<unsigned char>(uid[15]) == 0xEF);
}

TEST_CASE("hexStringToUid valid mixed case", "[plugin_description]")
{
    char uid[16];
    std::string hex = "aAbBcCdDeEfF0011aAbBcCdDeEfF0011";
    REQUIRE(dc::PluginDescription::hexStringToUid(hex, uid) == true);

    REQUIRE(static_cast<unsigned char>(uid[0]) == 0xAA);
    REQUIRE(static_cast<unsigned char>(uid[1]) == 0xBB);
    REQUIRE(static_cast<unsigned char>(uid[2]) == 0xCC);
    REQUIRE(static_cast<unsigned char>(uid[3]) == 0xDD);
    REQUIRE(static_cast<unsigned char>(uid[4]) == 0xEE);
    REQUIRE(static_cast<unsigned char>(uid[5]) == 0xFF);
    REQUIRE(static_cast<unsigned char>(uid[6]) == 0x00);
    REQUIRE(static_cast<unsigned char>(uid[7]) == 0x11);
}

TEST_CASE("hexStringToUid wrong length returns false", "[plugin_description]")
{
    char uid[16];

    SECTION("empty string")
    {
        REQUIRE(dc::PluginDescription::hexStringToUid("", uid) == false);
    }

    SECTION("16 chars")
    {
        REQUIRE(dc::PluginDescription::hexStringToUid("0123456789abcdef", uid) == false);
    }

    SECTION("31 chars")
    {
        REQUIRE(dc::PluginDescription::hexStringToUid("0123456789abcdef0123456789abcde", uid) == false);
    }

    SECTION("33 chars")
    {
        REQUIRE(dc::PluginDescription::hexStringToUid("0123456789abcdef0123456789abcdef0", uid) == false);
    }

    SECTION("64 chars")
    {
        REQUIRE(dc::PluginDescription::hexStringToUid(
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", uid) == false);
    }
}

TEST_CASE("hexStringToUid non-hex chars returns false", "[plugin_description]")
{
    char uid[16];

    SECTION("contains 'g'")
    {
        REQUIRE(dc::PluginDescription::hexStringToUid("0123456789abcdeg0123456789abcdef", uid) == false);
    }

    SECTION("contains 'z'")
    {
        REQUIRE(dc::PluginDescription::hexStringToUid("0123456789abcdez0123456789abcdef", uid) == false);
    }

    SECTION("contains '-'")
    {
        REQUIRE(dc::PluginDescription::hexStringToUid("01234567-9abcdef0123456789abcdef", uid) == false);
    }

    SECTION("contains ' '")
    {
        REQUIRE(dc::PluginDescription::hexStringToUid("01234567 9abcdef0123456789abcdef", uid) == false);
    }
}

TEST_CASE("hexStringToUid file path returns false", "[plugin_description]")
{
    char uid[16];
    REQUIRE(dc::PluginDescription::hexStringToUid("/usr/lib/vst3/MyPlugin.vst3", uid) == false);
}

// ──────────────────────────────────────────────────────────────
// uidToHexString
// ──────────────────────────────────────────────────────────────

TEST_CASE("uidToHexString round-trip", "[plugin_description]")
{
    const char original[16] = {
        '\x01', '\x23', '\x45', '\x67',
        '\x89', '\xAB', '\xCD', '\xEF',
        '\xFE', '\xDC', '\xBA', '\x98',
        '\x76', '\x54', '\x32', '\x10'
    };

    auto hex = dc::PluginDescription::uidToHexString(original);

    char roundTripped[16];
    REQUIRE(dc::PluginDescription::hexStringToUid(hex, roundTripped) == true);
    REQUIRE(std::memcmp(original, roundTripped, 16) == 0);
}

TEST_CASE("uidToHexString produces lowercase only", "[plugin_description]")
{
    const char uid[16] = {
        '\xAB', '\xCD', '\xEF', '\xFF',
        '\x00', '\x11', '\x22', '\x33',
        '\x44', '\x55', '\x66', '\x77',
        '\x88', '\x99', '\xAA', '\xBB'
    };

    auto hex = dc::PluginDescription::uidToHexString(uid);

    REQUIRE(hex.size() == 32);

    for (char c : hex)
    {
        bool isLowerHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        REQUIRE(isLowerHex);
    }
}

// ──────────────────────────────────────────────────────────────
// toMap / fromMap
// ──────────────────────────────────────────────────────────────

TEST_CASE("toMap / fromMap round-trip with all fields", "[plugin_description]")
{
    dc::PluginDescription original;
    original.name             = "Diva";
    original.manufacturer     = "u-he";
    original.category         = "Instrument";
    original.version          = "1.4.6";
    original.uid              = "abcdef0123456789abcdef0123456789";
    original.path             = "/usr/lib/vst3/Diva.vst3";
    original.numInputChannels  = 0;
    original.numOutputChannels = 2;
    original.hasEditor         = true;
    original.acceptsMidi       = true;
    original.producesMidi      = false;

    auto m = original.toMap();
    auto restored = dc::PluginDescription::fromMap(m);

    REQUIRE(restored.name             == original.name);
    REQUIRE(restored.manufacturer     == original.manufacturer);
    REQUIRE(restored.category         == original.category);
    REQUIRE(restored.version          == original.version);
    REQUIRE(restored.uid              == original.uid);
    REQUIRE(restored.path             == original.path);
    REQUIRE(restored.numInputChannels  == original.numInputChannels);
    REQUIRE(restored.numOutputChannels == original.numOutputChannels);
    REQUIRE(restored.hasEditor         == original.hasEditor);
    REQUIRE(restored.acceptsMidi       == original.acceptsMidi);
    REQUIRE(restored.producesMidi      == original.producesMidi);
}

TEST_CASE("fromMap with empty map produces defaults and no crash", "[plugin_description]")
{
    std::map<std::string, std::string> empty;
    auto d = dc::PluginDescription::fromMap(empty);

    REQUIRE(d.name.empty());
    REQUIRE(d.manufacturer.empty());
    REQUIRE(d.category.empty());
    REQUIRE(d.version.empty());
    REQUIRE(d.uid.empty());
    REQUIRE(d.path.empty());
    REQUIRE(d.numInputChannels  == 0);
    REQUIRE(d.numOutputChannels == 0);
    REQUIRE(d.hasEditor         == false);
    REQUIRE(d.acceptsMidi       == false);
    REQUIRE(d.producesMidi      == false);
}

TEST_CASE("fromMap with partial keys sets those and defaults the rest", "[plugin_description]")
{
    std::map<std::string, std::string> partial;
    partial["name"] = "Vital";
    partial["path"] = "/opt/vst3/Vital.vst3";

    auto d = dc::PluginDescription::fromMap(partial);

    REQUIRE(d.name == "Vital");
    REQUIRE(d.path == std::filesystem::path("/opt/vst3/Vital.vst3"));
    REQUIRE(d.manufacturer.empty());
    REQUIRE(d.uid.empty());
    REQUIRE(d.numInputChannels  == 0);
    REQUIRE(d.numOutputChannels == 0);
    REQUIRE(d.hasEditor         == false);
    REQUIRE(d.acceptsMidi       == false);
    REQUIRE(d.producesMidi      == false);
}

TEST_CASE("toMap integer fields survive string round-trip", "[plugin_description]")
{
    SECTION("zero values")
    {
        dc::PluginDescription d;
        d.numInputChannels  = 0;
        d.numOutputChannels = 0;
        d.hasEditor         = false;
        d.acceptsMidi       = false;
        d.producesMidi      = false;

        auto restored = dc::PluginDescription::fromMap(d.toMap());

        REQUIRE(restored.numInputChannels  == 0);
        REQUIRE(restored.numOutputChannels == 0);
        REQUIRE(restored.hasEditor         == false);
        REQUIRE(restored.acceptsMidi       == false);
        REQUIRE(restored.producesMidi      == false);
    }

    SECTION("max int for channel counts")
    {
        dc::PluginDescription d;
        d.numInputChannels  = INT_MAX;
        d.numOutputChannels = INT_MAX;

        auto restored = dc::PluginDescription::fromMap(d.toMap());

        REQUIRE(restored.numInputChannels  == INT_MAX);
        REQUIRE(restored.numOutputChannels == INT_MAX);
    }

    SECTION("boolean true values")
    {
        dc::PluginDescription d;
        d.hasEditor    = true;
        d.acceptsMidi  = true;
        d.producesMidi = true;

        auto restored = dc::PluginDescription::fromMap(d.toMap());

        REQUIRE(restored.hasEditor    == true);
        REQUIRE(restored.acceptsMidi  == true);
        REQUIRE(restored.producesMidi == true);
    }

    SECTION("boolean false values")
    {
        dc::PluginDescription d;
        d.hasEditor    = false;
        d.acceptsMidi  = false;
        d.producesMidi = false;

        auto restored = dc::PluginDescription::fromMap(d.toMap());

        REQUIRE(restored.hasEditor    == false);
        REQUIRE(restored.acceptsMidi  == false);
        REQUIRE(restored.producesMidi == false);
    }
}
