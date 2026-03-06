# Agent: LinuxRunLoop Class + Main Loop Integration

You are working on the `master` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to implement the VST3 `IRunLoop` interface for Linux so that yabridge plugins can register file descriptors and timers with the host, and integrate its polling into the main loop.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, architecture)
- `src/platform/linux/GlfwWindow.cpp` (main window — `pollEvents()` called each frame)
- `src/Main.cpp` (main loop at line ~780: `pollEvents()` → `tick()` → `renderFrame()`)
- `CMakeLists.txt` (Linux `target_sources` block at line ~357)

The VST3 SDK defines `IRunLoop` in `pluginterfaces/gui/iplugview.h` (under `Steinberg::Linux` namespace):

```cpp
namespace Steinberg { namespace Linux {

using TimerInterval = uint64;
using FileDescriptor = int;

class IEventHandler : public FUnknown
{
public:
    virtual void PLUGIN_API onFDIsSet (FileDescriptor fd) = 0;
    static const FUID iid;
};

class ITimerHandler : public FUnknown
{
public:
    virtual void PLUGIN_API onTimer () = 0;
    static const FUID iid;
};

class IRunLoop : public FUnknown
{
public:
    virtual tresult PLUGIN_API registerEventHandler (IEventHandler* handler, FileDescriptor fd) = 0;
    virtual tresult PLUGIN_API unregisterEventHandler (IEventHandler* handler) = 0;
    virtual tresult PLUGIN_API registerTimer (ITimerHandler* handler, TimerInterval milliseconds) = 0;
    virtual tresult PLUGIN_API unregisterTimer (ITimerHandler* handler) = 0;
    static const FUID iid;
};

}} // namespace Steinberg::Linux
```

## Deliverables

### New files (`src/platform/linux/`)

#### 1. `LinuxRunLoop.h`

Header for the singleton `IRunLoop` implementation.

- `#pragma once` with `#if defined(__linux__)` guard
- `class LinuxRunLoop` implementing `Steinberg::Linux::IRunLoop`
- `static LinuxRunLoop& instance()` — singleton accessor
- `registerEventHandler(IEventHandler* handler, FileDescriptor fd)` — store fd→handler mapping; return `kInvalidArgument` if handler is null or fd is already registered
- `unregisterEventHandler(IEventHandler* handler)` — find by handler pointer and remove; return `kResultFalse` if not found
- `registerTimer(ITimerHandler* handler, TimerInterval milliseconds)` — store handler with interval and next-fire timestamp; return `kInvalidArgument` if handler is null or interval is 0
- `unregisterTimer(ITimerHandler* handler)` — find by handler pointer and remove; return `kResultFalse` if not found
- `queryInterface(TUID iid, void** obj)` — return `IRunLoop` or `FUnknown`
- `addRef()` / `release()` — simple atomic ref count (singleton is never deleted via release)
- `void poll()` — public, called from main loop each frame
- Private storage: `std::unordered_map<FileDescriptor, IEventHandler*>` for event handlers; a `std::vector<TimerEntry>` for timers where `TimerEntry` holds `{handler, intervalMs, nextFire}` using `std::chrono::steady_clock::time_point`
- Use `dc_log` (from `dc/foundation/assert.h`) for registration/unregistration logging with `[RunLoop]` prefix

#### 2. `LinuxRunLoop.cpp`

Implementation.

- `instance()` — Meyer's singleton (`static LinuxRunLoop inst; return inst;`)
- `poll()`:
  - If `eventHandlers_` is non-empty: build a `std::vector<struct pollfd>` with `POLLIN` for each fd, call `::poll(pfds.data(), pfds.size(), 0)` (non-blocking, timeout=0), dispatch `onFDIsSet(fd)` for any fd with `revents & (POLLIN | POLLERR | POLLHUP)`
  - Iterate timers: if `now >= timer.nextFire`, call `handler->onTimer()` and advance `nextFire` by `intervalMs`
  - IMPORTANT: When iterating event handlers to dispatch, copy the map keys first (into a local vector) before dispatching, because `onFDIsSet` callbacks may call `unregisterEventHandler` which would invalidate iterators. Same for timers — copy the timer vector before iterating.
- Include `<poll.h>` for `::poll()`
- Include `<algorithm>` and `<chrono>`

### Modified files

#### 3. `src/Main.cpp`

In the main loop (line ~780), add a `LinuxRunLoop::poll()` call between `pollEvents()` and `tick()`:

```cpp
#if defined(__linux__)
#include "platform/linux/LinuxRunLoop.h"
#endif
```

In the loop body:
```cpp
glfwWindow->pollEvents();
#if defined(__linux__)
dc::LinuxRunLoop::instance().poll();
#endif
appController->tick();
```

#### 4. `CMakeLists.txt`

Add `src/platform/linux/LinuxRunLoop.cpp` to the Linux `target_sources` block (after line 368, before the closing `)`):

```cmake
    target_sources(DremCanvas PRIVATE
        src/platform/linux/GlfwWindow.cpp
        src/platform/linux/VulkanBackend.cpp
        src/platform/linux/NativeDialogs.cpp
        src/platform/linux/EmbeddedPluginEditor.cpp
        src/platform/linux/X11Reparent.cpp
        src/platform/linux/X11Compositor.cpp
        src/platform/linux/X11MouseProbe.cpp
        src/platform/linux/X11PluginEditorBridge.cpp
        src/platform/linux/X11SyntheticInputProbe.cpp
        src/platform/linux/X11SyntheticMouseDrag.cpp
        src/platform/linux/X11MouseEventForwarder.cpp
        src/platform/linux/LinuxRunLoop.cpp
    )
```

## Scope Limitation

Do NOT modify `PluginEditor.cpp`, `ComponentHandler.cpp`, or `PluginInstance.cpp`. Agent 02 handles wiring `IRunLoop` into those `queryInterface` methods.

## Conventions

- Namespace: `dc` (the `LinuxRunLoop` class lives in `namespace dc`)
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"platform/linux/LinuxRunLoop.h"`)
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`
