# Agent: Wire IRunLoop into queryInterface

You are working on the `master` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to expose the `LinuxRunLoop` singleton via `queryInterface` on the three VST3 host objects that plugins query for `IRunLoop`: `PlugFrame`, `ComponentHandler`, and `HostContext`.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions)
- `src/platform/linux/LinuxRunLoop.h` (the `IRunLoop` singleton — provides `LinuxRunLoop::instance()`)
- `src/dc/plugins/PluginEditor.cpp` (contains inner class `PlugFrame` with `queryInterface` at line ~41)
- `src/dc/plugins/ComponentHandler.cpp` (`queryInterface` at line ~51)
- `src/dc/plugins/PluginInstance.cpp` (anonymous `HostContext` class with `queryInterface` at line ~49)

## Dependencies

This agent depends on Agent 01 (LinuxRunLoop Class). If `src/platform/linux/LinuxRunLoop.h` doesn't exist yet, create a stub header with:

```cpp
#pragma once
#if defined(__linux__)
namespace dc {
class LinuxRunLoop {
public:
    static LinuxRunLoop& instance();
};
} // namespace dc
#endif
```

## Deliverables

### Migration

#### 1. `src/dc/plugins/PluginEditor.cpp`

This is the **primary** location per the VST3 spec — `IRunLoop` extends `IPlugFrame`.

Add a conditional include at the top of the file (after existing includes):

```cpp
#if defined(__linux__)
#include "platform/linux/LinuxRunLoop.h"
#endif
```

In `PlugFrame::queryInterface()` (line ~41), add an `IRunLoop` check **before** the final `*obj = nullptr` fallthrough. The existing code:

```cpp
Steinberg::tresult PLUGIN_API queryInterface (
    const Steinberg::TUID iid, void** obj) override
{
    if (Steinberg::FUnknownPrivate::iidEqual (iid,
        Steinberg::IPlugFrame::iid))
    {
        addRef();
        *obj = static_cast<IPlugFrame*> (this);
        return Steinberg::kResultOk;
    }

    if (Steinberg::FUnknownPrivate::iidEqual (iid,
        Steinberg::FUnknown::iid))
    {
        addRef();
        *obj = static_cast<Steinberg::FUnknown*> (
            static_cast<IPlugFrame*> (this));
        return Steinberg::kResultOk;
    }

    *obj = nullptr;
    return Steinberg::kNoInterface;
}
```

Insert before `*obj = nullptr`:

```cpp
#if defined(__linux__)
    if (Steinberg::FUnknownPrivate::iidEqual (iid,
        Steinberg::Linux::IRunLoop::iid))
    {
        auto& runLoop = dc::LinuxRunLoop::instance();
        runLoop.addRef();
        *obj = static_cast<Steinberg::Linux::IRunLoop*> (&runLoop);
        return Steinberg::kResultOk;
    }
#endif
```

Note: We call `addRef()` on the `LinuxRunLoop` instance (not on `this`) and return a pointer to the singleton. The singleton's ref count is cosmetic (it's never deleted via release), but COM protocol requires it.

#### 2. `src/dc/plugins/ComponentHandler.cpp`

Add a conditional include at the top:

```cpp
#if defined(__linux__)
#include "platform/linux/LinuxRunLoop.h"
#endif
```

In `ComponentHandler::queryInterface()` (line ~51), add the same `IRunLoop` check before `*obj = nullptr`. The existing code:

```cpp
Steinberg::tresult PLUGIN_API ComponentHandler::queryInterface (
    const Steinberg::TUID iid, void** obj)
{
    if (Steinberg::FUnknownPrivate::iidEqual (iid,
            Steinberg::Vst::IComponentHandler::iid))
    {
        addRef();
        *obj = static_cast<Steinberg::Vst::IComponentHandler*> (this);
        return Steinberg::kResultOk;
    }

    if (Steinberg::FUnknownPrivate::iidEqual (iid, Steinberg::FUnknown::iid))
    {
        addRef();
        *obj = static_cast<Steinberg::FUnknown*> (this);
        return Steinberg::kResultOk;
    }

    *obj = nullptr;
    return Steinberg::kNoInterface;
}
```

Insert before `*obj = nullptr`:

```cpp
#if defined(__linux__)
    if (Steinberg::FUnknownPrivate::iidEqual (iid,
        Steinberg::Linux::IRunLoop::iid))
    {
        auto& runLoop = dc::LinuxRunLoop::instance();
        runLoop.addRef();
        *obj = static_cast<Steinberg::Linux::IRunLoop*> (&runLoop);
        return Steinberg::kResultOk;
    }
#endif
```

#### 3. `src/dc/plugins/PluginInstance.cpp`

Add a conditional include near the top (after existing includes, before `namespace dc`):

```cpp
#if defined(__linux__)
#include "platform/linux/LinuxRunLoop.h"
#endif
```

In the anonymous `HostContext::queryInterface()` (line ~49), add the `IRunLoop` check before `*obj = nullptr`. The existing code:

```cpp
Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID _iid,
                                               void** obj) override
{
    if (Steinberg::FUnknownPrivate::iidEqual (_iid,
            Steinberg::Vst::IHostApplication::iid))
    {
        addRef();
        *obj = static_cast<IHostApplication*> (this);
        return Steinberg::kResultOk;
    }
    if (Steinberg::FUnknownPrivate::iidEqual (_iid,
            Steinberg::FUnknown::iid))
    {
        addRef();
        *obj = static_cast<FUnknown*> (this);
        return Steinberg::kResultOk;
    }
    *obj = nullptr;
    return Steinberg::kNoInterface;
}
```

Insert before `*obj = nullptr`:

```cpp
#if defined(__linux__)
    if (Steinberg::FUnknownPrivate::iidEqual (_iid,
        Steinberg::Linux::IRunLoop::iid))
    {
        auto& runLoop = dc::LinuxRunLoop::instance();
        runLoop.addRef();
        *obj = static_cast<Steinberg::Linux::IRunLoop*> (&runLoop);
        return Steinberg::kResultOk;
    }
#endif
```

Note: `HostContext` is in an anonymous namespace inside `PluginInstance.cpp`, but since it includes `LinuxRunLoop.h` which declares `dc::LinuxRunLoop`, the qualified name `dc::LinuxRunLoop::instance()` is used.

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"platform/linux/LinuxRunLoop.h"`)
- All `#if defined(__linux__)` guards around Linux-only code
- Build verification: `cmake --build --preset release`
