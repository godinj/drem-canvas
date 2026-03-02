// tests/regression/issue_001_multi_yabridge_load.cpp
//
// Bug: Loading two yabridge-bridged VST3 plugins back-to-back causes SIGSEGV.
//      The crash occurs on a yabridge background thread (Wine bridge IPC thread)
//      during ModuleEntry of the second plugin, producing a null function pointer
//      call (PC = 0x0000000000000000, completely corrupted stack).
//
// Root cause: Yabridge chainloaders (copies of libyabridge-chainloader-vst3.so)
//      each spawn a Wine host process and set up IPC sockets during ModuleEntry.
//      When two chainloaders initialise concurrently (even sequentially on the
//      same thread), the second Wine bridge's setup races with the first's
//      async IPC thread startup, causing a null function pointer dereference
//      on the bridge thread.
//
// Fix: Added yabridgeLoadMutex_ to VST3Host that serialises all yabridge
//      module loads.  After each successful yabridge load, a 500ms settling
//      delay gives the Wine bridge time to complete async initialisation
//      before the next chainloader's ModuleEntry is called.
//
// GDB backtrace (original crash):
//   Thread 26 "DremCanvas" received signal SIGSEGV, Segmentation fault.
//   [Switching to Thread 0x7fff8bfff6c0 (LWP 224480)]
//   0x0000000000000000 in ?? ()
//   #0  0x0000000000000000 in ??? ()
//   #1  0x0000000000000000 in ??? ()
//   (stack fully corrupted -- null function pointer call on yabridge bridge thread)
//
// Manual verification (requires yabridge + Wine + two bridged VST3 plugins):
//   1. Create a project with two yabridge plugins (e.g. Phase Plant + kHs Gate)
//   2. Reset probeCache.yaml to mark both plugins as 'safe'
//   3. Run: ./build-debug/DremCanvas --load ~/2PhaseP/
//   4. Verify probeCache.yaml still shows both as 'safe' (no pedal file left)
//   5. Before the fix, the second plugin caused SIGSEGV during load
//   6. After the fix, both plugins load successfully
//
// This test verifies the yabridge bundle detection heuristic that gates the
// serialization logic.  Full end-to-end multi-plugin load testing requires
// actual yabridge plugins and is documented above as a manual test procedure.

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace fs = std::filesystem;

// Mirror of VST3Module::isYabridgeBundle -- checks for Contents/x86_64-win/
// directory which is present in yabridge bundles but not native VST3 plugins.
// This is the heuristic that gates yabridgeLoadMutex_ acquisition.
static bool isYabridgeBundle (const fs::path& bundlePath)
{
    return fs::is_directory (bundlePath / "Contents" / "x86_64-win");
}

TEST_CASE ("Regression #001: yabridge detection for load serialization", "[regression]")
{
    auto tmpDir = fs::temp_directory_path() / "dc_test_issue_001";
    fs::create_directories (tmpDir);

    SECTION ("Native VST3 (x86_64-linux only) is not detected as yabridge")
    {
        auto nativeBundle = tmpDir / "NativePlugin.vst3";
        fs::create_directories (nativeBundle / "Contents" / "x86_64-linux");

        REQUIRE_FALSE (isYabridgeBundle (nativeBundle));
    }

    SECTION ("Yabridge bundle (has x86_64-win) is detected")
    {
        auto ybBundle = tmpDir / "YabridgePlugin.vst3";
        fs::create_directories (ybBundle / "Contents" / "x86_64-linux");
        fs::create_directories (ybBundle / "Contents" / "x86_64-win");

        REQUIRE (isYabridgeBundle (ybBundle));
    }

    SECTION ("Bundle without Contents dir is not yabridge")
    {
        auto emptyBundle = tmpDir / "EmptyPlugin.vst3";
        fs::create_directories (emptyBundle);

        REQUIRE_FALSE (isYabridgeBundle (emptyBundle));
    }

    SECTION ("macOS AU bundle (Contents/MacOS) is not yabridge")
    {
        auto macBundle = tmpDir / "MacPlugin.vst3";
        fs::create_directories (macBundle / "Contents" / "MacOS");

        REQUIRE_FALSE (isYabridgeBundle (macBundle));
    }

    SECTION ("Bundle with only x86_64-win (no linux) is still detected")
    {
        // Edge case: a standalone Windows plugin without a Linux chainloader
        auto winOnly = tmpDir / "WindowsOnly.vst3";
        fs::create_directories (winOnly / "Contents" / "x86_64-win");

        REQUIRE (isYabridgeBundle (winOnly));
    }

    fs::remove_all (tmpDir);
}
