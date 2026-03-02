// tests/regression/issue_006_phase_plant_blocked.cpp
//
// Bug:  After merging feature/plugin-scan-bar and feature/fix-plugin-list-empty
//       independently into master, scanOneInProcess() loaded yabridge bundles
//       without serialization. Wine bridge setup races caused SIGSEGV, marking
//       all yabridge plugins (including Phase Plant) as blocked in ProbeCache.
//       On subsequent launches only native plugins (e.g. Vital) appeared in the
//       plugin browser list.
//
// Root cause: PluginScanner::scanOneInProcess() had no mutex or settle delay
//       for yabridge loads. When multiple yabridge plugins were scanned in rapid
//       succession on a background thread, the Wine bridge IPC setup for the
//       second plugin raced with the first's async thread startup, producing a
//       null function pointer dereference (SIGSEGV) on a yabridge bridge thread.
//
// Fix:  Added yabridgeLoadMutex_ serialization to scanOneInProcess() with
//       500ms settle delay after each successful yabridge load, matching the
//       pattern in VST3Host::getOrLoadModule().
//
// Verified by: tests/e2e/test_phase_plant.sh (loads Phase Plant end-to-end)
//              tests/e2e/test_phase_plant_scan.sh (Phase Plant in browser scan)

#include <catch2/catch_test_macros.hpp>
#include "dc/plugins/ProbeCache.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE ("Regression #006: yabridge plugins not blocked after serialized scan",
           "[regression][plugins]")
{
    auto tmpDir = fs::temp_directory_path() / "dc_test_issue_006";
    fs::create_directories (tmpDir);

    // Create a fake yabridge bundle to use as a ProbeCache key
    auto fakeBundle = tmpDir / "FakeYabridge.vst3";
    fs::create_directories (fakeBundle / "Contents" / "x86_64-linux");
    fs::create_directories (fakeBundle / "Contents" / "x86_64-win");

    // Write a dummy .so so the bundle has a non-zero mtime
    {
        std::ofstream ofs (fakeBundle / "Contents" / "x86_64-linux" / "FakeYabridge.so");
        ofs << "dummy";
    }

    dc::ProbeCache cache (tmpDir);

    SECTION ("ProbeCache marks bundle as safe after successful scan")
    {
        // Simulate what scanOneInProcess() does on success:
        //   1. setPedal (before load)
        //   2. clearPedal (after successful load)
        //   3. setStatus(safe) + save()
        cache.setPedal (fakeBundle);
        cache.clearPedal();
        cache.setStatus (fakeBundle, dc::ProbeCache::Status::safe);
        cache.save();

        // Verify: status is safe, not blocked
        dc::ProbeCache reloaded (tmpDir);
        reloaded.load();
        REQUIRE (reloaded.getStatus (fakeBundle) == dc::ProbeCache::Status::safe);
    }

    SECTION ("Dead-man's-pedal file is cleaned up after successful scan")
    {
        cache.setPedal (fakeBundle);

        // Pedal should exist
        auto pedal = cache.checkPedal();
        REQUIRE (pedal.has_value());
        REQUIRE (*pedal == fakeBundle);

        // After clearPedal, it should be gone
        cache.clearPedal();
        pedal = cache.checkPedal();
        REQUIRE_FALSE (pedal.has_value());
    }

    SECTION ("Leftover pedal marks bundle as blocked on next load (crash recovery)")
    {
        // Simulate a crash: setPedal is called but clearPedal is never called
        // because scanOneInProcess() crashed during VST3Module::load().
        cache.setPedal (fakeBundle);
        cache.save();

        // On next startup, ProbeCache::load() detects the leftover pedal
        // and marks the bundle as blocked.
        dc::ProbeCache freshCache (tmpDir);
        freshCache.load();

        REQUIRE (freshCache.getStatus (fakeBundle) == dc::ProbeCache::Status::blocked);

        // Pedal file should be cleaned up after recovery
        REQUIRE_FALSE (freshCache.checkPedal().has_value());
    }

    SECTION ("Blocked bundle is skipped during scan but can be unblocked")
    {
        cache.setStatus (fakeBundle, dc::ProbeCache::Status::blocked);
        cache.save();

        dc::ProbeCache reloaded (tmpDir);
        reloaded.load();

        // Blocked bundles should be reported
        auto blocked = reloaded.getBlockedPlugins();
        REQUIRE (blocked.size() == 1);
        REQUIRE (blocked[0] == fakeBundle);

        // After resetStatus, the bundle can be retried
        reloaded.resetStatus (fakeBundle);
        REQUIRE (reloaded.getStatus (fakeBundle) == dc::ProbeCache::Status::unknown);
    }

    SECTION ("resetAllBlocked clears all blocked entries for retry")
    {
        auto fakeBundle2 = tmpDir / "FakeYabridge2.vst3";
        fs::create_directories (fakeBundle2 / "Contents" / "x86_64-linux");
        fs::create_directories (fakeBundle2 / "Contents" / "x86_64-win");

        {
            std::ofstream ofs (fakeBundle2 / "Contents" / "x86_64-linux" / "FakeYabridge2.so");
            ofs << "dummy";
        }

        cache.setStatus (fakeBundle, dc::ProbeCache::Status::blocked);
        cache.setStatus (fakeBundle2, dc::ProbeCache::Status::blocked);
        cache.save();

        dc::ProbeCache reloaded (tmpDir);
        reloaded.load();
        REQUIRE (reloaded.getBlockedPlugins().size() == 2);

        reloaded.resetAllBlocked();

        REQUIRE (reloaded.getStatus (fakeBundle) == dc::ProbeCache::Status::unknown);
        REQUIRE (reloaded.getStatus (fakeBundle2) == dc::ProbeCache::Status::unknown);
        REQUIRE (reloaded.getBlockedPlugins().empty());

        fs::remove_all (fakeBundle2);
    }

    fs::remove_all (tmpDir);
}
