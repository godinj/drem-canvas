# Agent: Fix setState to Call setupProcessing Before Reactivation

You are working on the `feature/plugin-loading-fixes-2` branch of Drem Canvas, a C++17 DAW
with VST3 plugin hosting via yabridge (Wine bridge). Your task is fixing a SIGSEGV crash
that occurs on the audio thread when `PluginInstance::process()` is called after
`setState()` during session restore.

## Context

Read these before starting:
- `src/dc/plugins/PluginInstance.cpp` (lines 879-954: `setState()` — deactivates/reactivates but skips `setupProcessing()`)
- `src/dc/plugins/PluginInstance.cpp` (lines 1062-1076: `setupProcessing()` — calls `processor_->setupProcessing(setup)`, `setActive(true)`, `setProcessing(true)`)
- `src/dc/plugins/PluginInstance.cpp` (lines 524-526: `create()` — correctly calls `setupProcessing()` then `prepare()`)
- `src/dc/plugins/PluginInstance.h` (full file — class layout, member variables, `setupProcessing` signature)
- `src/ui/AppController.cpp` (lines 1344-1526: `rebuildAudioGraph()` — session restore flow: suspend → create plugins → restorePluginState → resume)
- `tests/regression/issue_003_set_state_deactivation.cpp` (existing regression test for the deactivation contract)

Also check the VST3 SDK header for the lifecycle spec:
- `build/_deps/vst3sdk-src/pluginterfaces/vst/ivstaudioprocessor.h` (ProcessSetup struct, setupProcessing docs)

## Problem

`PluginInstance::setState()` correctly deactivates the plugin before state restoration
(VST3 spec compliance, fixed in commit `95f8ae5`), but it **does not** call
`setupProcessing()` before reactivation. The VST3 lifecycle requires:

```
setupProcessing(setup)  →  setActive(true)  →  setProcessing(true)
```

The current `setState()` only does:

```
setProcessing(false)  →  setActive(false)  →  [restore state]  →  setActive(true)  →  setProcessing(true)
```

This crashes yabridge-bridged plugins (specifically Phase Plant) because the Wine-side
plugin expects `setupProcessing()` to be called before `setActive(true)` after a
deactivation cycle. Native VST3 plugins tolerate the missing call, but yabridge's IPC
bridge does not — the `process()` call segfaults inside `libyabridge-vst3.so`.

**Crash stack:**
```
PortAudio callback → AudioEngine::GraphCallback::audioCallback()
  → AudioGraph::processBlock() → GraphExecutor::execute()
    → PluginInstance::process() → libyabridge-vst3.so  ← SIGSEGV
```

This is deterministic — 5 consecutive crashes in coredump history during session restore
loading Phase Plant.

## Deliverables

### Modified files

#### 1. `src/dc/plugins/PluginInstance.cpp`

**Fix `setState()` (line 879):** Add `setupProcessing()` call before `setActive(true)` in
both the format-mismatch path and the normal path.

Current reactivation pattern (appears twice in `setState`):
```cpp
// Reactivate regardless of success or failure
component_->setActive (true);
if (processor_ != nullptr)
    processor_->setProcessing (true);
```

Must become:
```cpp
// Reactivate with full VST3 lifecycle (setupProcessing required before setActive)
setupProcessing (currentSampleRate_, currentBlockSize_);
```

Note that `setupProcessing()` already calls `setActive(true)` and `setProcessing(true)`
internally (see lines 1062-1076), so replace the three calls with one `setupProcessing()`
call. Do this for **both** reactivation sites in `setState()`:

1. **Format mismatch path** (around line 914-918): Replace the `setActive(true)` +
   `setProcessing(true)` block with `setupProcessing(currentSampleRate_, currentBlockSize_)`.

2. **Normal path** (around line 950-953): Replace the `setActive(true)` +
   `setProcessing(true)` block with `setupProcessing(currentSampleRate_, currentBlockSize_)`.

The `currentSampleRate_` and `currentBlockSize_` members are already set during `prepare()`
and persist across `setState()` calls, so they are safe to use here.

#### 2. `tests/regression/issue_004_set_state_setup_processing.cpp` (NEW)

Regression test verifying that `setupProcessing()` is called between deactivation and
reactivation in `setState()`. Extend the spy pattern from `issue_003`:

```cpp
// tests/regression/issue_004_set_state_setup_processing.cpp
//
// Bug: PluginInstance::process() SIGSEGV inside yabridge after setState()
//      during session restore. Phase Plant crashed because setState()
//      called setActive(true) without first calling setupProcessing().
//
// Cause: VST3 lifecycle requires setupProcessing() before setActive(true)
//        after a deactivation cycle. yabridge-bridged plugins enforce this
//        strictly; native plugins tolerate the omission.
//
// Fix: setState() now calls setupProcessing() before reactivation in both
//      the format-mismatch and normal state restoration paths.
```

Use the same spy approach as `issue_003_set_state_deactivation.cpp`:

- Add a `SetupProcessing` entry to the `CallRecord::Type` enum
- Add a `SpyProcessor::setupProcessing(double, int)` method that records the call
- Update `setStateWithDeactivation()` to mirror the fixed production code:
  call `setupProcessing()` before `setActive(true)` (the spy's `setupProcessing` should
  also call `setActive(true)` and `setProcessing(true)` to match the production method)
- **Test cases:**
  - Normal path: verify `SetupProcessing` appears after `SetActive(false)` and before
    any `SetActive(true)`
  - Format mismatch path: same verification
  - Failed setState: verify `setupProcessing()` is called even when state restore fails
  - Verify the complete call sequence: `SetProcessing(false)` → `SetActive(false)` →
    `ComponentSetState` → `SetupProcessing` → (implicit `SetActive(true)` + `SetProcessing(true)`)

#### 3. `tests/CMakeLists.txt`

Add `tests/regression/issue_004_set_state_setup_processing.cpp` to the regression test
target. Follow the pattern used for `issue_003_set_state_deactivation.cpp`.

### Note on issue_004_teardown_sigsegv.cpp

There is already a file `tests/regression/issue_004_teardown_sigsegv.cpp`. Name your new
test `issue_005_set_state_setup_processing.cpp` instead if issue_004 is taken. Check the
existing files first and pick the next available issue number.

## E2E Verification (Required)

After making the code fix and regression test, you **must** verify the fix against real
plugins using the e2e state-restore test created by prompt 07.

### Prerequisite: prompt 07 must run first

Prompt 07 (`07-e2e-state-restore.md`) creates the e2e test infrastructure:
- `--capture-plugin-state` and `--process-frames N` CLI flags in `Main.cpp`
- `tests/fixtures/e2e-state-restore/` fixture with real saved plugin state
- `tests/e2e/test_state_restore.sh` test script

If the e2e fixture has not been populated yet (i.e., the track YAML files still contain
`<CAPTURED_STATE>` placeholders), run the capture script first:

```bash
cmake --build --preset release
./tests/fixtures/e2e-state-restore/capture.sh
```

### Run the e2e test

After your fix is applied, build and run the state-restore e2e test:

```bash
cmake --build --preset release
tests/e2e/test_state_restore.sh ./build/DremCanvas_artefacts/Release/DremCanvas
```

This test loads a project with **non-empty saved plugin state** (including a yabridge
plugin), triggers `setState()` with real data, then runs 50 audio frames. Before your
fix, this crashes with SIGSEGV inside `libyabridge-vst3.so`. After your fix, it should
print `PASS: plugin state restore + audio processing survived`.

**Do not declare this prompt complete until the e2e test passes.** The regression test
(spy pattern) validates the contract in isolation; the e2e test validates the actual
production code against real VST3 binaries.

## Scope Limitation

Do NOT modify:
- `PluginInstance::create()` — its lifecycle is already correct
- `PluginInstance::prepare()` or `release()` — not affected
- `PluginInstance::process()` — the guard is handled by prompt 06
- `AppController::rebuildAudioGraph()` — the session restore flow is correct
- `Main.cpp` — the e2e flags are handled by prompt 07
- Any other file not listed above

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Regression tests go in `tests/regression/` with `[regression]` tag
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Test verification: `ctest --test-dir build-debug --output-on-failure -j$(nproc)`
- E2E verification: `tests/e2e/test_state_restore.sh` (must pass)
- Run full verification: `scripts/verify.sh`
