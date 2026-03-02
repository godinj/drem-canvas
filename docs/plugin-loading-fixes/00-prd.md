# Plugin Loading Fixes — PRD

> Fix and harden VST3 plugin loading for yabridge-bridged and native plugins.
> Addresses crashes, legacy format issues, and missing test coverage for the
> new dc::plugins layer.

**Branch**: `master` (main worktree)
**Dependencies**: dc::foundation (Phase 0 complete), dc::plugins layer
**Related files**:
- `src/dc/plugins/VST3Module.h` / `.cpp` — module loading, fork-probe, yabridge detection
- `src/dc/plugins/VST3Host.h` / `.cpp` — module cache, probe cache integration, instance creation
- `src/dc/plugins/ProbeCache.h` / `.cpp` — persistent probe result cache with dead-man's-pedal
- `src/dc/plugins/PluginInstance.cpp` — component creation, HostContext, class enumeration fallback
- `src/dc/plugins/PluginDescription.h` — UID conversion, description serialization
- `src/plugins/PluginHost.cpp` — `descriptionFromPropertyTree`, bridge to VST3Host
- `src/ui/AppController.cpp` — `rebuildAudioGraph()` sequential plugin loading

---

## Observed Issues

### Issue 1: SIGSEGV loading two yabridge plugins back-to-back

**Severity**: Critical
**Symptom**: Loading a project with two yabridge-bridged plugins (e.g. Phase Plant + kHs Gate)
causes SIGSEGV during the second plugin's module loading (dlopen/ModuleEntry).
**Context**: `AppController::rebuildAudioGraph()` loads plugins sequentially via
`createPluginSync()` in a for loop (same thread, blocking). The first plugin loads
successfully; the second crashes.
**Root cause**: Unknown — likely yabridge global state conflict or Wine bridge
race condition between two chainloaders.
**Current mitigation**: Dead-man's-pedal catches the crash; on next startup the
crashing module is marked blocked. But the crash itself is not prevented.

**Key code paths**:
- `AppController::rebuildAudioGraph()` (`src/ui/AppController.cpp:1438-1456`) — sequential loop
- `VST3Host::createInstanceSync()` (`src/dc/plugins/VST3Host.cpp:91-109`) — blocking create
- `VST3Host::getOrLoadModule()` (`src/dc/plugins/VST3Host.cpp:142-233`) — yabridge path
- `VST3Module::load()` (`src/dc/plugins/VST3Module.cpp:145-246`) — dlopen + ModuleEntry

**Investigation needed**:
- Run under GDB to get exact crash backtrace
- Test whether adding a delay between yabridge loads helps
- Test whether loading into separate processes (out-of-process hosting) avoids it
- Check yabridge issue tracker for known multi-plugin loading bugs

### Issue 2: Legacy JUCE-format plugin state fails to restore

**Severity**: Medium
**Symptom**: `PluginInstance::setState` logs "invalid data size" when loading old projects.
**Context**: Old projects (created with JUCE-based plugin hosting) store plugin state
in JUCE's opaque binary format. The new dc:: setState expects
`[4 bytes componentSize][componentData][controllerData]` format.
**Root cause**: Format mismatch between JUCE's VST3 state wrapper and our raw VST3 format.

**Key code paths**:
- `PluginInstance::setState()` (`src/dc/plugins/PluginInstance.cpp:879-922`)
- `PluginHost::restorePluginState()` — base64 decode → setState
- Old format: JUCE wraps state with its own header/framing
- New format: `[uint32_t componentSize][componentData][controllerData]`

### Issue 3: descriptionFromPropertyTree sets UID to file path

**Severity**: Medium
**Symptom**: `desc.uid` is set to the plugin file path instead of the actual VST3 class UID.
Old projects store `unique_id` as a JUCE-format integer, not a hex UID string.
**Current workaround**: Factory class enumeration fallback in `PluginInstance::create()` —
when hex UID is invalid, enumerate all factory classes and pick the first audio effect.
**Root cause**: `descriptionFromPropertyTree()` in `src/plugins/PluginHost.cpp` maps
`pluginFileOrIdentifier` → `desc.uid`, which is incorrect.

**Key code paths**:
- `PluginHost::descriptionFromPropertyTree()` (`src/plugins/PluginHost.cpp`)
- `PluginDescription::hexStringToUid()` (`src/dc/plugins/PluginDescription.h`)
- `PluginInstance::create()` (`src/dc/plugins/PluginInstance.cpp:348-532`) — fallback at line 378

### Issue 4: ProbeCache has no test coverage

**Severity**: Medium
**Symptom**: ProbeCache is new code (written during this debugging session) with zero tests.
**Components needing tests**:
- YAML round-trip: save → load preserves all entries
- Mtime invalidation: status returns `unknown` when bundle mtime changes
- Dead-man's-pedal: leftover pedal file on load() marks module as blocked
- Status transitions: unknown → safe, unknown → blocked, safe → unknown (on load failure)
- File I/O edge cases: missing cache file, corrupted YAML, missing cache directory

**Key code paths**:
- `ProbeCache::load()` / `save()` (`src/dc/plugins/ProbeCache.cpp`)
- `ProbeCache::getStatus()` / `setStatus()` — mtime-aware lookup
- `ProbeCache::setPedal()` / `clearPedal()` / `checkPedal()` — crash recovery

### Issue 5: VST3Module entry point variants untested

**Severity**: Low
**Symptom**: No automated tests for the ModuleEntry/ModuleExit vs InitDll/ExitDll
symbol lookup logic.
**Context**: Linux VST3 plugins export either modern (`ModuleEntry(void*)` / `ModuleExit()`)
or legacy (`InitDll()` / `ExitDll()`) entry points. Both probe and load functions must
try modern first, then fall back to legacy.

**Key code paths**:
- `VST3Module::probeModuleSafe()` (`src/dc/plugins/VST3Module.cpp:29-143`) — fork probe
- `VST3Module::load()` (`src/dc/plugins/VST3Module.cpp:145-246`) — direct load
- Symbol lookup order: `ModuleEntry` → `InitDll`, `ModuleExit` → `ExitDll`

### Issue 6: Yabridge detection heuristic needs validation

**Severity**: Low
**Symptom**: `isYabridgeBundle()` checks for `Contents/x86_64-win/` directory.
This heuristic should be tested against real bundle layouts.

**Key code paths**:
- `VST3Module::isYabridgeBundle()` (`src/dc/plugins/VST3Module.cpp:276-281`)

---

## Test Strategy

### Unit tests (no plugin binaries needed)

These tests can run in CI without actual VST3 plugins:

1. **ProbeCache tests** (`test_probe_cache.cpp`)
   - YAML round-trip
   - Mtime invalidation
   - Dead-man's-pedal recovery
   - Corrupted/missing file handling

2. **PluginDescription UID tests** (`test_plugin_description.cpp`)
   - `hexStringToUid()` with valid hex, invalid hex, empty string, file paths
   - `fromMap()` / `toMap()` round-trip
   - Legacy integer UID handling

3. **VST3Module path resolution tests** (`test_vst3_module.cpp`)
   - `resolveLibraryPath()` for Linux and macOS layouts
   - `isYabridgeBundle()` with mock directory structures

4. **HostContext tests** (`test_host_context.cpp`)
   - IHostApplication interface compliance
   - queryInterface for supported/unsupported IIDs
   - Reference counting

### Integration tests (require plugin binaries)

These need actual VST3 plugins installed on the test machine:

5. **Single plugin load test**
   - Load a native Linux VST3
   - Load a yabridge-bridged VST3
   - Verify factory, component, processor, controller are non-null
   - Verify prepare/process/release cycle

6. **Multi-plugin sequential load test** (the SIGSEGV investigation)
   - Load two yabridge plugins sequentially
   - GDB backtrace on crash
   - Test with delay between loads
   - Test with forced dlclose between loads

7. **State save/restore round-trip**
   - Save state → restore state → verify parameter values match
   - Test with JUCE-format legacy state data

---

## Phase Structure

| Phase | Name | Deliverables | Exit Criteria |
|-------|------|-------------|---------------|
| **1** | ProbeCache unit tests | `tests/unit/plugins/test_probe_cache.cpp` | All ProbeCache paths covered; YAML round-trip verified |
| **2** | PluginDescription tests | `tests/unit/plugins/test_plugin_description.cpp` | UID conversion, map round-trip, legacy format handling |
| **3** | VST3Module unit tests | `tests/unit/plugins/test_vst3_module.cpp` | Path resolution, yabridge detection with mock dirs |
| **4** | Multi-plugin crash investigation | GDB backtrace, root cause analysis | Crash root cause identified; fix or workaround implemented |
| **5** | State format migration | Legacy JUCE state detection + conversion | Old projects' plugin state restores correctly |

### Phase Dependencies

```
Phase 1 (ProbeCache) ──┐
Phase 2 (Description) ─┼──→ Phase 4 (crash investigation)
Phase 3 (VST3Module) ──┘           │
                                   └──→ Phase 5 (state migration)
```

Phases 1–3 can run in parallel. Phase 4 requires understanding from 1–3.
Phase 5 can proceed independently after Phase 2.
