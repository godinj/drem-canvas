#include <catch2/catch_test_macros.hpp>
#include <dc/plugins/PluginScanner.h>
#include <dc/plugins/VST3Module.h>
#include <filesystem>

namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════════════════
// Yabridge detection tests
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE ("isYabridgeBundle returns false for non-existent path", "[plugins][yabridge]")
{
    REQUIRE_FALSE (dc::VST3Module::isYabridgeBundle ("/nonexistent/plugin.vst3"));
}

// ═══════════════════════════════════════════════════════════════════════
// In-process scan path tests
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE ("scanOneInProcess does not crash on invalid path", "[plugins][yabridge]")
{
    dc::PluginScanner scanner;
    auto result = scanner.scanOneInProcess ("/nonexistent/plugin.vst3");
    REQUIRE_FALSE (result.has_value());
}

TEST_CASE ("scanOneInProcess returns nullopt for empty bundle", "[plugins][yabridge]")
{
    auto tmp = fs::temp_directory_path() / "dc_test_yabridge_scan_XXXXXX";
    auto tmpl = tmp.string();
    REQUIRE (mkdtemp (tmpl.data()) != nullptr);
    tmp = tmpl;

    auto bundle = tmp / "FakeYabridge.vst3";
    fs::create_directories (bundle / "Contents" / "x86_64-linux");
    fs::create_directories (bundle / "Contents" / "x86_64-win");

    // Should be detected as yabridge
    REQUIRE (dc::VST3Module::isYabridgeBundle (bundle));

    // But scanning should fail (no .so file)
    dc::PluginScanner scanner;
    auto result = scanner.scanOneInProcess (bundle);
    REQUIRE_FALSE (result.has_value());

    fs::remove_all (tmp);
}

TEST_CASE ("scanOne returns nullopt for nonexistent path", "[plugins][yabridge]")
{
    dc::PluginScanner scanner;
    auto result = scanner.scanOne ("/nonexistent/plugin.vst3");
    REQUIRE_FALSE (result.has_value());
}
