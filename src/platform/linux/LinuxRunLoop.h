#pragma once

#if defined(__linux__)

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/base/funknown.h>

#include <atomic>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace dc
{

class LinuxRunLoop : public Steinberg::Linux::IRunLoop
{
public:
    static LinuxRunLoop& instance();

    // --- IRunLoop ---
    Steinberg::tresult PLUGIN_API registerEventHandler (Steinberg::Linux::IEventHandler* handler,
                                                        Steinberg::Linux::FileDescriptor fd) override;
    Steinberg::tresult PLUGIN_API unregisterEventHandler (Steinberg::Linux::IEventHandler* handler) override;
    Steinberg::tresult PLUGIN_API registerTimer (Steinberg::Linux::ITimerHandler* handler,
                                                 Steinberg::Linux::TimerInterval milliseconds) override;
    Steinberg::tresult PLUGIN_API unregisterTimer (Steinberg::Linux::ITimerHandler* handler) override;

    // --- FUnknown ---
    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID iid, void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override;
    Steinberg::uint32 PLUGIN_API release() override;

    /// Called from the main loop each frame to dispatch pending fd events and timers
    void poll();

private:
    LinuxRunLoop() = default;
    ~LinuxRunLoop() = default;

    LinuxRunLoop (const LinuxRunLoop&) = delete;
    LinuxRunLoop& operator= (const LinuxRunLoop&) = delete;

    // --- Event handler storage ---
    std::unordered_map<Steinberg::Linux::FileDescriptor,
                       Steinberg::Linux::IEventHandler*> eventHandlers_;

    // --- Timer storage ---
    struct TimerEntry
    {
        Steinberg::Linux::ITimerHandler* handler = nullptr;
        std::chrono::milliseconds intervalMs {0};
        std::chrono::steady_clock::time_point nextFire;
    };

    std::vector<TimerEntry> timers_;

    std::atomic<Steinberg::uint32> refCount_ {1};
};

} // namespace dc

#endif // __linux__
