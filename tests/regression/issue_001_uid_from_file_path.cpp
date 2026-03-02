// tests/regression/issue_001_uid_from_file_path.cpp
//
// Bug: descriptionFromPropertyTree set desc.uid to the file path (e.g.
//      "/usr/lib/vst3/Phase Plant.vst3") instead of the VST3 class UID hex
//      string.  Old projects also store unique_id as an old-format integer
//      (e.g. -3838345), which is equally invalid.
//
// Cause: Both desc.uid and desc.path were read from IDs::pluginFileOrIdentifier.
//
// Fix: Read uid from IDs::pluginUniqueId instead, validate with
//      hexStringToUid, and leave uid empty if validation fails so that
//      PluginInstance::create() falls through to the class enumeration
//      fallback.

#include <catch2/catch_test_macros.hpp>
#include <dc/plugins/PluginDescription.h>

TEST_CASE ("Regression #001: hexStringToUid rejects file paths", "[regression][plugin_description]")
{
    char uid[16] = {};

    // File paths are not valid hex UIDs
    REQUIRE_FALSE (dc::PluginDescription::hexStringToUid (
        "/usr/lib/vst3/Phase Plant.vst3", uid));

    // Windows-style paths are not valid hex UIDs
    REQUIRE_FALSE (dc::PluginDescription::hexStringToUid (
        "C:\\Program Files\\VST3\\Serum.vst3", uid));

    // Legacy integer UIDs are not valid hex UIDs
    REQUIRE_FALSE (dc::PluginDescription::hexStringToUid ("-3838345", uid));

    // Positive integer UID
    REQUIRE_FALSE (dc::PluginDescription::hexStringToUid ("12345678", uid));

    // Empty string is not valid
    REQUIRE_FALSE (dc::PluginDescription::hexStringToUid ("", uid));
}

TEST_CASE ("Regression #001: hexStringToUid accepts valid 32-char hex", "[regression][plugin_description]")
{
    char uid[16] = {};

    // A real VST3 class UID (32 hex chars)
    REQUIRE (dc::PluginDescription::hexStringToUid (
        "abcdef0123456789abcdef0123456789", uid));

    // Verify first byte decoded correctly
    REQUIRE (static_cast<unsigned char> (uid[0]) == 0xab);
}
