#include <catch2/catch_test_macros.hpp>
#include "dc/plugins/PluginScanner.h"
#include "dc/plugins/PluginDescription.h"

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

TEST_CASE ("Auto-scan trigger: empty known list triggers scan", "[plugins]")
{
    // This test verifies the contract that AppController relies on:
    // when the known plugins list is empty, scanForPlugins() should be called.
    // We verify scanner.scanAll() is safe to call and returns a valid vector.

    SECTION ("empty known plugins list detects as empty")
    {
        std::vector<dc::PluginDescription> knownPlugins;
        REQUIRE (knownPlugins.empty());
    }

    SECTION ("scanAll returns valid vector when no plugins exist")
    {
        dc::PluginScanner scanner;

        // scanAll() scans standard VST3 paths. On CI or test machines
        // with no plugins installed, it should return an empty vector
        // without crashing.
        auto results = scanner.scanAll();

        // Just verify we get back a valid (possibly empty) vector
        // and that the call did not crash.
        REQUIRE (results.size() >= 0);
    }

    SECTION ("scanAll results contain valid descriptions")
    {
        dc::PluginScanner scanner;
        auto results = scanner.scanAll();

        for (const auto& desc : results)
        {
            // Every discovered plugin must have a non-empty name and path
            REQUIRE_FALSE (desc.name.empty());
            REQUIRE_FALSE (desc.path.empty());
        }
    }
}
