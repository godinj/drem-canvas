# Agent: Multi-Plugin Crash Investigation

You are working on the `master` branch of Drem Canvas, a C++17 DAW with VST3 plugin hosting.
Your task is investigating and fixing the SIGSEGV crash that occurs when loading two
yabridge-bridged VST3 plugins back-to-back.

## Context

Read these before starting:
- `docs/plugin-loading-fixes/00-prd.md` (Issue 1: SIGSEGV loading two yabridge plugins)
- `src/dc/plugins/VST3Host.cpp` (`getOrLoadModule` at line 142 — yabridge detection + load path)
- `src/dc/plugins/VST3Module.cpp` (`load()` at line 145 — dlopen + ModuleEntry call)
- `src/dc/plugins/VST3Module.h` (ModuleEntryFunc typedef — `bool (*)(void*)`)
- `src/ui/AppController.cpp` (`rebuildAudioGraph()` at line 1438 — sequential plugin loading loop)
- `src/dc/plugins/ProbeCache.h` (dead-man's-pedal API — `setPedal`/`clearPedal`)

## Dependencies

This agent depends on Agents 01-03. If those test files don't exist yet, proceed
without them — the investigation doesn't require them.

## Background

Single yabridge plugin loading works. The crash occurs during the **second** plugin's
module loading when both plugins are yabridge-bridged (confirmed with Phase Plant + kHs Gate).
The crash is a SIGSEGV during `dlopen` or `ModuleEntry` of the second plugin.

The loading happens sequentially on the same thread via `createPluginSync()` in a for loop.
The `moduleMutex_` in `getOrLoadModule()` serializes module cache access but the
`PluginInstance::create()` calls are not serialized.

Current mitigation: Dead-man's-pedal correctly catches the crash and blocks the
offending module on next startup. But the crash itself still occurs.

## Investigation Steps

### Step 1: GDB backtrace

Build a debug build and run the app under GDB to capture the exact crash location:

```bash
cmake --preset debug
cmake --build --preset debug
```

Create or use the existing two-plugin test project (e.g. `~/2PhaseP/`).
Run under GDB:

```bash
gdb --args ./build-debug/DremCanvas_artefacts/Debug/DremCanvas
```

In GDB:
```
run
# Wait for SIGSEGV
bt full
info threads
```

Capture the full backtrace, noting:
- Which thread crashes
- The exact function/instruction
- Whether it's inside yabridge, Wine, or dc:: code
- Stack frames through dlopen/ModuleEntry

### Step 2: Identify root cause

Based on the backtrace, determine which of these is the cause:

1. **Yabridge global state**: The yabridge chainloader uses process-global state
   (e.g. a single Wine host process) that can't handle two simultaneous loads.

2. **dlopen conflict**: Two yabridge `.so` files loaded simultaneously share a
   dependency (e.g. `libyabridge-vst3.so`) that has global constructors/destructors
   conflicting.

3. **Wine bridge race**: Each chainloader starts its own Wine host; two hosts
   starting simultaneously compete for shared resources.

4. **Module initialization order**: The first plugin's `ModuleEntry` modifies global
   state that the second plugin's `ModuleEntry` assumes is clean.

### Step 3: Implement fix

Based on the root cause, implement one of these fixes (in order of preference):

**Option A: Serialize yabridge loads with delay**

If the issue is a timing/race condition, add serialization to `getOrLoadModule()`:

```cpp
// In VST3Host.h, add:
std::mutex yabridgeLoadMutex_;

// In getOrLoadModule(), for yabridge bundles:
if (VST3Module::isYabridgeBundle (bundlePath))
{
    // Release moduleMutex_ before acquiring yabridgeLoadMutex_
    // to avoid deadlock, then re-acquire moduleMutex_ after.
    lock.unlock();
    std::lock_guard<std::mutex> yabridgeLock (yabridgeLoadMutex_);
    lock.lock();

    // Re-check cache (another thread may have loaded it)
    auto it2 = loadedModules_.find (key);
    if (it2 != loadedModules_.end())
        return it2->second.get();

    // ... existing yabridge load path ...
    // Add delay after successful load:
    std::this_thread::sleep_for (std::chrono::milliseconds (500));
}
```

**Option B: Out-of-process loading for yabridge**

If serialization doesn't help, consider loading yabridge plugins in a child process
(similar to `probeModuleSafe` but keeping the process alive as a host).
This is significantly more complex — only attempt if Option A fails.

**Option C: Document as known limitation**

If the crash is a yabridge bug (not fixable on our side), document it clearly
and ensure the dead-man's-pedal gracefully handles it. Add a log message explaining
the limitation.

### Step 4: Regression test

If a fix is implemented, add a regression test:

#### `tests/regression/issue_001_multi_yabridge_load.cpp`

If the fix is testable without actual plugins (e.g., serialization logic),
write a unit test. If it requires actual plugins, document the manual test procedure
in a comment at the top of the file.

## Deliverables

- GDB backtrace saved as comment in the regression test or in this doc
- Root cause identified and documented
- Fix implemented (Option A, B, or C)
- Regression test if applicable
- Updated `docs/plugin-loading-fixes/00-prd.md` Issue 1 with findings

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`
- Run verification: `scripts/verify.sh`
