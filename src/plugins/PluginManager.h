#pragma once
#include "dc/plugins/VST3Host.h"
#include "dc/plugins/PluginDescription.h"
#include "dc/foundation/worker_thread.h"
#include "dc/foundation/message_queue.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace dc
{

class PluginManager
{
public:
    explicit PluginManager (MessageQueue& messageQueue);
    ~PluginManager();

    // ─── Existing (sync) API ──────────────────────────────

    /// Scan state for UI feedback
    enum class ScanState { idle, scanning, complete };
    void scanForPlugins();

    /// Get current scan state
    ScanState getScanState() const { return scanState_; }

    /// Returns the scanned plugin list as dc::PluginDescription
    const std::vector<dc::PluginDescription>& getKnownPlugins() const;

    /// Access the VST3Host directly
    dc::VST3Host& getVST3Host();

    // Persistence
    void savePluginList (const std::filesystem::path& file) const;
    void loadPluginList (const std::filesystem::path& file);

    std::filesystem::path getDefaultPluginListFile() const;

    // ─── New async API ────────────────────────────────────

    /// Progress callback: (pluginName, current, total)
    using ScanProgressCallback = std::function<void (const std::string& pluginName,
                                                      int current, int total)>;
    /// Called on the message thread when scan finishes
    using ScanCompleteCallback = std::function<void()>;

    /// Start an async scan on a background thread.
    /// `onProgress` is called on the **message thread** for each plugin.
    /// `onComplete` is called on the **message thread** when done.
    /// No-op if a scan is already in progress.
    void scanForPluginsAsync (ScanProgressCallback onProgress = {},
                              ScanCompleteCallback onComplete = {});

    /// True while an async scan is running.
    bool isScanning() const;

private:
    dc::VST3Host vst3Host_;
    dc::MessageQueue& messageQueue_;
    dc::WorkerThread scanThread_ { "plugin-scan" };
    std::atomic<bool> scanning_ { false };
    std::atomic<ScanState> scanState_ { ScanState::idle };

    PluginManager (const PluginManager&) = delete;
    PluginManager& operator= (const PluginManager&) = delete;
};

} // namespace dc
