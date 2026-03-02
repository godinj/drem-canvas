#include <catch2/catch_test_macros.hpp>
#include <dc/plugins/ProbeCache.h>
#include <dc/foundation/file_utils.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────

/// Create a unique temp directory for a single TEST_CASE.
/// Uses the system temp dir plus a per-PID, per-invocation suffix
/// so parallel test runs cannot collide.
static fs::path createTempDir()
{
    static int counter = 0;
    auto base = fs::temp_directory_path()
                / ("dc_test_probe_cache_" + std::to_string (getpid())
                   + "_" + std::to_string (counter++));
    fs::create_directories (base);
    return base;
}

/// Create a dummy .vst3 bundle directory with a file inside
/// (so it has a meaningful mtime).
static fs::path createDummyBundle (const fs::path& dir, const std::string& name)
{
    auto bundle = dir / (name + ".vst3");
    fs::create_directories (bundle / "Contents" / "x86_64-linux");
    dc::writeStringToFile (bundle / "Contents" / "x86_64-linux" / (name + ".so"), "dummy");
    return bundle;
}

/// RAII guard that removes a directory tree on destruction.
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

TEST_CASE ("ProbeCache YAML round-trip", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";

    auto bundleA = createDummyBundle (bundleDir, "SynthA");
    auto bundleB = createDummyBundle (bundleDir, "SynthB");
    auto bundleC = createDummyBundle (bundleDir, "CrasherC");

    // Populate and save
    {
        dc::ProbeCache cache (cacheDir);
        cache.setStatus (bundleA, dc::ProbeCache::Status::safe);
        cache.setStatus (bundleB, dc::ProbeCache::Status::safe);
        cache.setStatus (bundleC, dc::ProbeCache::Status::blocked);
        cache.save();
    }

    // Load into a fresh instance and verify
    {
        dc::ProbeCache cache (cacheDir);
        cache.load();

        REQUIRE (cache.getStatus (bundleA) == dc::ProbeCache::Status::safe);
        REQUIRE (cache.getStatus (bundleB) == dc::ProbeCache::Status::safe);
        REQUIRE (cache.getStatus (bundleC) == dc::ProbeCache::Status::blocked);
    }
}

TEST_CASE ("ProbeCache mtime invalidation", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";

    auto bundle = createDummyBundle (bundleDir, "PlugMtime");

    // Mark as safe and save
    {
        dc::ProbeCache cache (cacheDir);
        cache.setStatus (bundle, dc::ProbeCache::Status::safe);
        cache.save();
    }

    // Change the bundle's mtime.  ProbeCache::getMtime reads the
    // last_write_time of the bundle path (the .vst3 directory).
    // Explicitly set a different mtime to invalidate the cached entry.
    auto oldTime = fs::last_write_time (bundle);
    fs::last_write_time (bundle, oldTime + std::chrono::seconds (10));

    // Reload — the cached entry should be stale, returning unknown
    {
        dc::ProbeCache cache (cacheDir);
        cache.load();

        REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::unknown);
    }
}

TEST_CASE ("ProbeCache dead-man's-pedal recovery", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";

    auto bundle = createDummyBundle (bundleDir, "CrashPlug");

    // Simulate a crash: write a pedal file manually
    fs::create_directories (cacheDir);
    dc::writeStringToFile (cacheDir / ".probe-pedal", bundle.string());

    // Load — should detect leftover pedal and mark the module as blocked
    {
        dc::ProbeCache cache (cacheDir);
        cache.load();

        REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::blocked);

        // Pedal file should have been cleaned up
        REQUIRE_FALSE (fs::exists (cacheDir / ".probe-pedal"));
    }
}

TEST_CASE ("ProbeCache setPedal / clearPedal round-trip", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";

    auto bundle = createDummyBundle (bundleDir, "PedalPlug");

    dc::ProbeCache cache (cacheDir);

    SECTION ("setPedal creates pedal file with correct content")
    {
        cache.setPedal (bundle);

        auto pedalPath = cacheDir / ".probe-pedal";
        REQUIRE (fs::exists (pedalPath));

        auto content = dc::readFileToString (pedalPath);
        REQUIRE (content == bundle.string());
    }

    SECTION ("clearPedal removes the pedal file")
    {
        cache.setPedal (bundle);
        REQUIRE (fs::exists (cacheDir / ".probe-pedal"));

        cache.clearPedal();
        REQUIRE_FALSE (fs::exists (cacheDir / ".probe-pedal"));
    }

    SECTION ("checkPedal returns path when pedal exists")
    {
        cache.setPedal (bundle);

        auto result = cache.checkPedal();
        REQUIRE (result.has_value());
        REQUIRE (*result == bundle);
    }

    SECTION ("checkPedal returns nullopt when no pedal")
    {
        auto result = cache.checkPedal();
        REQUIRE_FALSE (result.has_value());
    }
}

TEST_CASE ("ProbeCache missing cache file", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "empty_cache";
    // Do NOT create any file in cacheDir

    dc::ProbeCache cache (cacheDir);
    // Should not crash
    REQUIRE_NOTHROW (cache.load());

    // All bundles should be unknown
    auto fakePath = fs::path ("/nonexistent/plugin.vst3");
    REQUIRE (cache.getStatus (fakePath) == dc::ProbeCache::Status::unknown);
}

TEST_CASE ("ProbeCache corrupted YAML", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    fs::create_directories (cacheDir);

    // Write garbage to the cache file
    dc::writeStringToFile (cacheDir / "probeCache.yaml", "{{{{not valid yaml!@#$%");

    dc::ProbeCache cache (cacheDir);

    // Should not crash
    REQUIRE_NOTHROW (cache.load());

    // Entries should be empty — everything unknown
    auto fakePath = fs::path ("/some/plugin.vst3");
    REQUIRE (cache.getStatus (fakePath) == dc::ProbeCache::Status::unknown);
}

TEST_CASE ("ProbeCache missing cache directory on save", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "deeply" / "nested" / "cache";
    // cacheDir does not exist yet

    auto bundleDir = tmp / "bundles";
    auto bundle = createDummyBundle (bundleDir, "DeepPlug");

    dc::ProbeCache cache (cacheDir);
    cache.setStatus (bundle, dc::ProbeCache::Status::safe);

    // save() should create the directory and file
    REQUIRE_NOTHROW (cache.save());
    REQUIRE (fs::exists (cacheDir / "probeCache.yaml"));

    // Verify the saved data is loadable
    {
        dc::ProbeCache cache2 (cacheDir);
        cache2.load();
        REQUIRE (cache2.getStatus (bundle) == dc::ProbeCache::Status::safe);
    }
}

TEST_CASE ("ProbeCache status transitions", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";

    auto bundle = createDummyBundle (bundleDir, "TransPlug");

    dc::ProbeCache cache (cacheDir);

    SECTION ("unknown to safe")
    {
        REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::unknown);
        cache.setStatus (bundle, dc::ProbeCache::Status::safe);
        REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::safe);
    }

    SECTION ("unknown to blocked")
    {
        REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::unknown);
        cache.setStatus (bundle, dc::ProbeCache::Status::blocked);
        REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::blocked);
    }

    SECTION ("safe to unknown via setStatus")
    {
        cache.setStatus (bundle, dc::ProbeCache::Status::safe);
        REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::safe);

        cache.setStatus (bundle, dc::ProbeCache::Status::unknown);
        REQUIRE (cache.getStatus (bundle) == dc::ProbeCache::Status::unknown);
    }
}

TEST_CASE ("ProbeCache multiple entries round-trip", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";

    // Create 7 dummy bundles
    std::vector<fs::path> bundles;
    for (int i = 0; i < 7; ++i)
        bundles.push_back (createDummyBundle (bundleDir, "Plugin_" + std::to_string (i)));

    // Assign a mix of statuses
    {
        dc::ProbeCache cache (cacheDir);
        cache.setStatus (bundles[0], dc::ProbeCache::Status::safe);
        cache.setStatus (bundles[1], dc::ProbeCache::Status::safe);
        cache.setStatus (bundles[2], dc::ProbeCache::Status::blocked);
        cache.setStatus (bundles[3], dc::ProbeCache::Status::safe);
        cache.setStatus (bundles[4], dc::ProbeCache::Status::blocked);
        cache.setStatus (bundles[5], dc::ProbeCache::Status::safe);
        cache.setStatus (bundles[6], dc::ProbeCache::Status::blocked);
        cache.save();
    }

    // Reload and verify every entry independently
    {
        dc::ProbeCache cache (cacheDir);
        cache.load();

        REQUIRE (cache.getStatus (bundles[0]) == dc::ProbeCache::Status::safe);
        REQUIRE (cache.getStatus (bundles[1]) == dc::ProbeCache::Status::safe);
        REQUIRE (cache.getStatus (bundles[2]) == dc::ProbeCache::Status::blocked);
        REQUIRE (cache.getStatus (bundles[3]) == dc::ProbeCache::Status::safe);
        REQUIRE (cache.getStatus (bundles[4]) == dc::ProbeCache::Status::blocked);
        REQUIRE (cache.getStatus (bundles[5]) == dc::ProbeCache::Status::safe);
        REQUIRE (cache.getStatus (bundles[6]) == dc::ProbeCache::Status::blocked);
    }
}

TEST_CASE ("ProbeCache pedal recovery does not overwrite existing entries", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    auto bundleDir = tmp / "bundles";

    auto safePlug = createDummyBundle (bundleDir, "SafePlug");
    auto crashPlug = createDummyBundle (bundleDir, "CrashPlug");

    // Save one safe entry
    {
        dc::ProbeCache cache (cacheDir);
        cache.setStatus (safePlug, dc::ProbeCache::Status::safe);
        cache.save();
    }

    // Simulate crash by writing pedal for crashPlug
    dc::writeStringToFile (cacheDir / ".probe-pedal", crashPlug.string());

    // Load — should mark crashPlug as blocked and keep safePlug as safe
    {
        dc::ProbeCache cache (cacheDir);
        cache.load();

        REQUIRE (cache.getStatus (safePlug) == dc::ProbeCache::Status::safe);
        REQUIRE (cache.getStatus (crashPlug) == dc::ProbeCache::Status::blocked);
    }
}

TEST_CASE ("ProbeCache clearPedal is safe when no pedal exists", "[probe_cache]")
{
    auto tmp = createTempDir();
    TempDirGuard guard (tmp);

    auto cacheDir = tmp / "cache";
    fs::create_directories (cacheDir);

    dc::ProbeCache cache (cacheDir);

    // Should not throw when clearing a non-existent pedal
    REQUIRE_NOTHROW (cache.clearPedal());
}
