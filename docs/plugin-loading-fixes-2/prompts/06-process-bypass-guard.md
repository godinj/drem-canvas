# Agent: Add Process Bypass Guard for Crashed/Unready Plugins

You are working on the `feature/plugin-loading-fixes-2` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting via yabridge (Wine bridge). Your task is adding defense-in-depth
to `PluginInstance::process()` so that a broken or unready plugin cannot SIGSEGV the audio
thread and crash the entire application.

## Context

Read these before starting:
- `src/dc/plugins/PluginInstance.cpp` (lines 594-732: `process()` — current implementation with `dc_assert(prepared_)` and null check)
- `src/dc/plugins/PluginInstance.h` (full file — class layout, `prepared_` flag, `processor_` pointer)
- `src/dc/engine/GraphExecutor.cpp` (full file — the execute loop that calls `node->process()` for each node)
- `src/dc/engine/AudioNode.h` (base class interface)
- `src/engine/PluginProcessorNode.h` (wrapper that delegates to PluginInstance)

## Problem

Currently, `PluginInstance::process()` has two guards:

```cpp
dc_assert(prepared_);          // Debug-only assertion — no-op in Release
if (processor_ == nullptr)     // Null check — good but insufficient
    return;
```

If `processor_` is non-null but the underlying yabridge IPC bridge is in a bad state
(e.g., Wine process died, plugin crashed internally, socket disconnected), the call into
`processor_->process(processData_)` will SIGSEGV. This takes down the entire application
because the crash happens on the PortAudio audio callback thread.

The audio thread must NEVER crash. A misbehaving plugin should be silently bypassed
(output silence) rather than killing the host.

## Deliverables

### Modified files

#### 1. `src/dc/plugins/PluginInstance.h`

Add a `bypassed_` atomic flag to the private section:

```cpp
// Process bypass flag — set to true if plugin crashes during process()
std::atomic<bool> bypassed_ {false};
```

Add a public method to query/reset bypass state:

```cpp
/// Returns true if the plugin has been bypassed due to a process() failure.
bool isBypassed() const;

/// Reset the bypass flag (e.g., after user re-enables the plugin).
void resetBypass();
```

#### 2. `src/dc/plugins/PluginInstance.cpp`

**In `process()` (line 594):** Add a bypass check at the top, before any processing:

```cpp
void PluginInstance::process (AudioBlock& audio, MidiBlock& midi, int numSamples)
{
    if (! prepared_ || processor_ == nullptr || bypassed_.load (std::memory_order_relaxed))
        return;
```

Remove or replace the `dc_assert(prepared_)` with the bypass-safe check above. In Release
builds the assert is a no-op anyway; in Debug builds we want the same silent-bypass
behavior to avoid crashing during development.

**Add signal-safe crash protection around the VST3 process call (line 700):**

Wrap the `processor_->process(processData_)` call with a POSIX signal handler that catches
SIGSEGV/SIGBUS on the audio thread. If the plugin's `process()` crashes:

1. Set `bypassed_ = true`
2. `longjmp` back to a safe point
3. The plugin is silently bypassed for all future calls (output silence)

Implementation:

```cpp
#include <csignal>
#include <csetjmp>

namespace {
    thread_local sigjmp_buf g_processJmpBuf;
    thread_local std::atomic<bool>* g_bypassFlag = nullptr;

    void processSignalHandler (int /*sig*/)
    {
        if (g_bypassFlag != nullptr)
            g_bypassFlag->store (true, std::memory_order_relaxed);
        siglongjmp (g_processJmpBuf, 1);
    }
}
```

In `process()`, around the `processor_->process(processData_)` call:

```cpp
// Install temporary signal handler for crash protection
g_bypassFlag = &bypassed_;
struct sigaction newAction {}, oldSegv {}, oldBus {};
newAction.sa_handler = processSignalHandler;
newAction.sa_flags = 0;
sigemptyset (&newAction.sa_mask);

sigaction (SIGSEGV, &newAction, &oldSegv);
sigaction (SIGBUS, &newAction, &oldBus);

if (sigsetjmp (g_processJmpBuf, 1) == 0)
{
    processor_->process (processData_);
}
else
{
    // Plugin crashed — bypassed_ is already set, just log
    dc_log ("PluginInstance::process: plugin '%s' crashed — bypassing",
            description_.name.c_str());
}

// Restore original signal handlers
sigaction (SIGSEGV, &oldSegv, nullptr);
sigaction (SIGBUS, &oldBus, nullptr);
g_bypassFlag = nullptr;
```

**IMPORTANT: Audio thread safety considerations for the signal handler approach:**

- `sigaction` is async-signal-safe per POSIX — safe to call on audio thread
- `siglongjmp` is async-signal-safe per POSIX
- `std::atomic::store` with `memory_order_relaxed` is signal-safe
- The `dc_log` call after `siglongjmp` is on the normal execution path (not in the signal
  handler), so it's safe
- `thread_local` variables ensure no cross-thread interference
- Signal handler installation/removal happens every process block — this is ~1μs overhead,
  negligible at 512-sample blocks (~11ms at 44.1kHz)

**Alternative approach (if signal handler is deemed too heavy):**

If you judge the signal handler approach to be too invasive for the audio thread, implement
a simpler version that only adds the bypass flag check without signal protection:

```cpp
void PluginInstance::process (AudioBlock& audio, MidiBlock& midi, int numSamples)
{
    if (! prepared_ || processor_ == nullptr || bypassed_.load (std::memory_order_relaxed))
        return;

    // ... existing setup code ...

    processor_->process (processData_);

    // ... existing MIDI output code ...
}
```

And add a `setBypass(bool)` public method so the session-restore code or error handling
can explicitly bypass a plugin that's known to be problematic. This is less automatic but
avoids signal handler complexity on the audio thread.

**Choose whichever approach best fits the project's audio-thread philosophy.** The signal
handler approach provides automatic crash recovery; the simple approach requires external
code to set the bypass flag.

#### 3. Implement `isBypassed()` and `resetBypass()`

```cpp
bool PluginInstance::isBypassed() const
{
    return bypassed_.load (std::memory_order_relaxed);
}

void PluginInstance::resetBypass()
{
    bypassed_.store (false, std::memory_order_relaxed);
}
```

#### 4. `tests/unit/plugins/test_plugin_instance_bypass.cpp` (NEW)

Unit test for the bypass guard logic:

- **Test: process returns immediately when bypassed** — Create a minimal test (can use
  a mock or just verify the flag logic) that confirms `isBypassed()` returns false by
  default, `setBypass(true)` / manual flag set makes `isBypassed()` return true, and
  `resetBypass()` clears it.
- **Test: bypass flag is atomic** — Verify the flag can be read/written from different
  threads without data races (use `std::thread` to write from one thread, read from another).
- **Test: prepared_ check prevents processing** — Verify that when `prepared_ == false`,
  `process()` is a no-op (doesn't crash even with null processor).

Since `PluginInstance` requires a real VST3 module to construct, test the bypass logic
through a public-interface test. You may need to:
- Test `isBypassed()`/`resetBypass()` via a test-only factory or friend, OR
- Test the atomic flag behavior in isolation with a standalone struct

#### 5. `tests/CMakeLists.txt`

Add `tests/unit/plugins/test_plugin_instance_bypass.cpp` to the unit test target.

## Scope Limitation

Do NOT modify:
- `setState()` — that fix is handled by prompt 05
- `GraphExecutor::execute()` — the guard lives inside `PluginInstance::process()`, not the executor
- `AudioGraph` or `AudioEngine` — no changes needed at the graph level
- `AppController` — no changes needed

## Audio Thread Safety Rules

- No heap allocations (`new`, `malloc`, `std::vector::push_back`) in `process()`
- No mutex locks in `process()`
- No blocking I/O in `process()`
- `dc_log` calls are acceptable only on the non-signal-handler path (after `siglongjmp` returns)
- All signal handler code must use only async-signal-safe functions per POSIX

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Test verification: `ctest --test-dir build-debug --output-on-failure -j$(nproc)`
- Run full verification: `scripts/verify.sh`
