#include <catch2/catch_test_macros.hpp>
#include <dc/plugins/PluginScanner.h>
#include <dc/plugins/ProbeCache.h>
#include <dc/plugins/PluginDescription.h>
#include <dc/foundation/file_utils.h>

#include <filesystem>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────

static fs::path createTempDir()
{
    static int counter = 0;
    auto base = fs::temp_directory_path()
                / ("dc_test_scan_filter_" + std::to_string (getpid())
                   + "_" + std::to_string (counter++));
    fs::create_directories (base);
    return base;
}

static fs::path createDummyBundle (const fs::path& dir, const std::string& name)
{
    auto bundle = dir / (name + ".vst3");
    fs::create_directories (bundle / "Contents" / "x86_64-linux");
    dc::writeStringToFile (bundle / "Contents" / "x86_64-linux" / (name + ".so"), "dummy");
    return bundle;
}

struct TempDirGuard
{
    fs::path path;

    explicit TempDirGuard (fs::path p) : path (std::move (p)) {}

    ~TempDirGuard()
    {
        std::error_code ec;
        fs::remove_all (path, ec);
    }

    TempDirGuard (const TempDirGuard&) = delete;
    TempDirGuard& operator= (const TempDirGuard&) = delete;
};

// ── Tests ────────────────────────────────────────────────────────

TEST_CASE ("PluginScanner setPreviousPlugins accepts empty list", "[plugins]")
{
    dc::PluginScanner scanner;
    std::vector<dc::PluginDescription> prev;
    REQUIRE_NOTHROW (scanner.setPreviousPlugins (prev));
}

TEST_CASE ("PluginScanner setPreviousPlugins accepts populated list", "[plugins]")
{
    dc::PluginScanner scanner;

    dc::PluginDescription desc;
    desc.name = "TestPlugin";
    desc.path = "/test/path.vst3";
    desc.uid = "aabbccdd11223344aabbccdd11223344";
    std::vector<dc::PluginDescription> prev = { desc };

    REQUIRE_NOTHROW (scanner.setPreviousPlugins (prev));
}

TEST_CASE ("PluginScanner setPreviousPlugins can be overwritten", "[plugins]")
{
    dc::PluginScanner scanner;

    dc::PluginDescription desc;
    desc.name = "TestPlugin";
    desc.path = "/test/path.vst3";
    std::vector<dc::PluginDescription> prev = { desc };
    scanner.setPreviousPlugins (prev);

    // Overwrite with empty
    scanner.setPreviousPlugins ({});

    // Should not crash
    REQUIRE (true);
}

TEST_CASE ("PluginScanner setProbeCache and setPreviousPlugins work together", "[plugins]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";

    dc::ProbeCache cache (cacheDir);
    dc::PluginScanner scanner;

    dc::PluginDescription desc;
    desc.name = "CachedPlugin";
    desc.path = "/cached/plugin.vst3";
    std::vector<dc::PluginDescription> prev = { desc };

    // Both can be set without conflict
    REQUIRE_NOTHROW (scanner.setProbeCache (&cache));
    REQUIRE_NOTHROW (scanner.setPreviousPlugins (prev));
}

TEST_CASE ("ProbeCache getStatus returns unknown for missing entry (scan filter)", "[plugins]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";

    dc::ProbeCache cache (cacheDir);
    REQUIRE (cache.getStatus ("/nonexistent/plugin.vst3") == dc::ProbeCache::Status::unknown);
}

TEST_CASE ("ProbeCache getStatus returns blocked for blocked entry (scan filter)", "[plugins]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";
    auto bundle = createDummyBundle (bundleDir, "BlockedPlug");

    dc::ProbeCache cache (cacheDir);
    cache.setStatus (bundle, dc::ProbeCache::Status::blocked);
    REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::blocked);
}

TEST_CASE ("ProbeCache getStatus returns safe for safe entry with matching mtime (scan filter)", "[plugins]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";
    auto bundle = createDummyBundle (bundleDir, "SafePlug");

    dc::ProbeCache cache (cacheDir);
    cache.setStatus (bundle, dc::ProbeCache::Status::safe);
    REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::safe);
}

TEST_CASE ("ProbeCache getStatus returns unknown when mtime changes (scan filter)", "[plugins]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";
    auto bundle = createDummyBundle (bundleDir, "MtimePlug");

    // Mark as safe
    dc::ProbeCache cache (cacheDir);
    cache.setStatus (bundle, dc::ProbeCache::Status::safe);
    REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::safe);

    // Change the bundle's mtime
    auto oldTime = fs::last_write_time (bundle);
    fs::last_write_time (bundle, oldTime + std::chrono::seconds (10));

    // Now the cache entry should be stale
    REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::unknown);
}
