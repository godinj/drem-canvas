#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace dc {

/// Persistent cache of VST3 module probe results.
///
/// Stores per-bundle probe status (safe / blocked) keyed by path and mtime.
/// Uses a dead-man's-pedal file to detect crashes during direct load
/// attempts — if the pedal file exists on startup, the module it names
/// is marked as blocked.
class ProbeCache
{
public:
    enum class Status { unknown, safe, blocked };

    explicit ProbeCache (const std::filesystem::path& cacheDir);

    /// Load the cache from disk.  Safe to call if file doesn't exist.
    void load();

    /// Save the cache to disk.
    void save() const;

    /// Get cached status for a bundle.  Returns unknown if not cached
    /// or if the bundle's mtime has changed since the entry was written.
    Status getStatus (const std::filesystem::path& bundlePath) const;

    /// Set the status for a bundle (uses current mtime).
    void setStatus (const std::filesystem::path& bundlePath, Status status);

    // ── Unblock API ──

    /// Reset a blocked module to unknown, allowing retry on next load.
    /// Also clears any leftover pedal file for that path.
    void resetStatus (const std::filesystem::path& bundlePath);

    /// Return all paths currently marked as blocked.
    std::vector<std::filesystem::path> getBlockedPlugins() const;

    /// Reset ALL blocked entries to unknown.
    void resetAllBlocked();

    // ── Dead-man's-pedal ──

    /// Write the pedal file before a risky direct load.
    void setPedal (const std::filesystem::path& bundlePath);

    /// Clear the pedal file after a successful load.
    void clearPedal();

    /// Check for a leftover pedal from a previous crash.
    /// Returns the bundle path if the pedal file exists.
    std::optional<std::filesystem::path> checkPedal() const;

private:
    std::filesystem::path cacheFile_;
    std::filesystem::path pedalFile_;

    struct Entry
    {
        std::int64_t mtime = 0;  // seconds since epoch
        Status status = Status::unknown;
    };

    std::map<std::string, Entry> entries_;

    static std::int64_t getMtime (const std::filesystem::path& path);
};

} // namespace dc
