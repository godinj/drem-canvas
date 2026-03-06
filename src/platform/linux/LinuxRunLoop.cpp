#include "platform/linux/LinuxRunLoop.h"

#if defined(__linux__)

#include "dc/foundation/assert.h"

#include <pluginterfaces/base/funknown.h>

#include <algorithm>
#include <chrono>
#include <poll.h>
#include <vector>

namespace dc
{

using namespace Steinberg;
using namespace Steinberg::Linux;

// ─── Singleton ───────────────────────────────────────────────────────────────

LinuxRunLoop& LinuxRunLoop::instance()
{
    static LinuxRunLoop inst;
    return inst;
}

// ─── IRunLoop: Event handlers ────────────────────────────────────────────────

tresult PLUGIN_API LinuxRunLoop::registerEventHandler (IEventHandler* handler, FileDescriptor fd)
{
    if (! handler)
        return kInvalidArgument;

    if (eventHandlers_.find (fd) != eventHandlers_.end())
        return kInvalidArgument;

    eventHandlers_[fd] = handler;
    dc_log ("[RunLoop] registerEventHandler fd=%d handler=%p", fd, static_cast<void*> (handler));
    return kResultOk;
}

tresult PLUGIN_API LinuxRunLoop::unregisterEventHandler (IEventHandler* handler)
{
    for (auto it = eventHandlers_.begin(); it != eventHandlers_.end(); ++it)
    {
        if (it->second == handler)
        {
            dc_log ("[RunLoop] unregisterEventHandler fd=%d handler=%p",
                     it->first, static_cast<void*> (handler));
            eventHandlers_.erase (it);
            return kResultOk;
        }
    }

    return kResultFalse;
}

// ─── IRunLoop: Timers ────────────────────────────────────────────────────────

tresult PLUGIN_API LinuxRunLoop::registerTimer (ITimerHandler* handler, TimerInterval milliseconds)
{
    if (! handler)
        return kInvalidArgument;

    if (milliseconds == 0)
        return kInvalidArgument;

    TimerEntry entry;
    entry.handler = handler;
    entry.intervalMs = std::chrono::milliseconds (milliseconds);
    entry.nextFire = std::chrono::steady_clock::now() + entry.intervalMs;

    timers_.push_back (entry);
    dc_log ("[RunLoop] registerTimer handler=%p interval=%lums",
             static_cast<void*> (handler), static_cast<unsigned long> (milliseconds));
    return kResultOk;
}

tresult PLUGIN_API LinuxRunLoop::unregisterTimer (ITimerHandler* handler)
{
    auto it = std::find_if (timers_.begin(), timers_.end(),
                            [handler] (const TimerEntry& e) { return e.handler == handler; });

    if (it == timers_.end())
        return kResultFalse;

    dc_log ("[RunLoop] unregisterTimer handler=%p", static_cast<void*> (handler));
    timers_.erase (it);
    return kResultOk;
}

// ─── FUnknown ────────────────────────────────────────────────────────────────

tresult PLUGIN_API LinuxRunLoop::queryInterface (const TUID iid, void** obj)
{
    if (! obj)
        return kInvalidArgument;

    if (FUnknownPrivate::iidEqual (iid, IRunLoop::iid)
        || FUnknownPrivate::iidEqual (iid, FUnknown::iid))
    {
        addRef();
        *obj = static_cast<IRunLoop*> (this);
        return kResultOk;
    }

    *obj = nullptr;
    return kNoInterface;
}

uint32 PLUGIN_API LinuxRunLoop::addRef()
{
    return ++refCount_;
}

uint32 PLUGIN_API LinuxRunLoop::release()
{
    auto count = --refCount_;
    // Singleton — never deleted via release
    return count;
}

// ─── Poll (called from main loop each frame) ────────────────────────────────

void LinuxRunLoop::poll()
{
    // --- File descriptor polling ---
    if (! eventHandlers_.empty())
    {
        // Copy keys before dispatching — callbacks may unregister handlers
        std::vector<FileDescriptor> fds;
        fds.reserve (eventHandlers_.size());
        for (const auto& pair : eventHandlers_)
            fds.push_back (pair.first);

        std::vector<struct pollfd> pfds;
        pfds.reserve (fds.size());
        for (auto fd : fds)
        {
            struct pollfd pfd {};
            pfd.fd = fd;
            pfd.events = POLLIN;
            pfds.push_back (pfd);
        }

        int ret = ::poll (pfds.data(), pfds.size(), 0);
        if (ret > 0)
        {
            for (size_t i = 0; i < pfds.size(); ++i)
            {
                if (pfds[i].revents & (POLLIN | POLLERR | POLLHUP))
                {
                    auto it = eventHandlers_.find (fds[i]);
                    if (it != eventHandlers_.end())
                        it->second->onFDIsSet (fds[i]);
                }
            }
        }
    }

    // --- Timer firing ---
    if (! timers_.empty())
    {
        auto now = std::chrono::steady_clock::now();

        // Copy timer vector before iterating — callbacks may unregister timers
        auto timersCopy = timers_;

        for (auto& timer : timersCopy)
        {
            if (now >= timer.nextFire)
            {
                timer.handler->onTimer();

                // Update nextFire in the original vector if the timer is still registered
                for (auto& orig : timers_)
                {
                    if (orig.handler == timer.handler)
                    {
                        orig.nextFire = now + orig.intervalMs;
                        break;
                    }
                }
            }
        }
    }
}

} // namespace dc

#endif // __linux__
