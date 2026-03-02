# Plugin Loading Fixes 2 — Agent Prompt Index

> Follow-up fixes discovered during testing of the plugin-loading-fixes branch.
> All prompts are independent and can run in parallel.

---

## Execution Order

```
01-process-context ──────────────┐
02-probe-unblock ────────────────┼──→ (all independent, run in parallel)
03-audio-graph-suspend ──────────┘
04-set-state-deactivate-test ────
```

### All Prompts (Parallel — No Dependencies)

| # | Prompt | Scope | Files Created | Files Modified |
|---|--------|-------|---------------|----------------|
| 01 | [01-process-context.md](01-process-context.md) | Populate ProcessContext for VST3 plugins (tempo, transport, loop) | `tests/unit/plugins/test_plugin_instance.cpp` | `src/dc/plugins/PluginInstance.h`, `src/dc/plugins/PluginInstance.cpp`, `src/engine/AudioEngine.cpp` |
| 02 | [02-probe-unblock.md](02-probe-unblock.md) | API to retry/unblock crashed plugins in ProbeCache | — | `src/dc/plugins/ProbeCache.h`, `src/dc/plugins/ProbeCache.cpp`, `src/dc/plugins/VST3Host.h`, `src/dc/plugins/VST3Host.cpp`, `tests/unit/plugins/test_probe_cache.cpp` |
| 03 | [03-audio-graph-suspend.md](03-audio-graph-suspend.md) | Thread-safe audio graph topology mutations via suspend/resume | `tests/unit/engine/test_audio_graph_suspend.cpp` | `src/dc/engine/AudioGraph.h`, `src/dc/engine/AudioGraph.cpp`, `src/engine/AudioEngine.h`, `src/engine/AudioEngine.cpp`, `src/ui/AppController.cpp` |
| 04 | [04-set-state-deactivate-test.md](04-set-state-deactivate-test.md) | Regression test for setState deactivation fix | `tests/regression/issue_003_set_state_deactivation.cpp` | `tests/CMakeLists.txt` |

---

## Usage

```bash
cd ~/git/drem-canvas.git/feature/plugin-loading-fixes

# All parallel
claude --agent docs/plugin-loading-fixes-2/prompts/01-process-context.md
claude --agent docs/plugin-loading-fixes-2/prompts/02-probe-unblock.md
claude --agent docs/plugin-loading-fixes-2/prompts/03-audio-graph-suspend.md
claude --agent docs/plugin-loading-fixes-2/prompts/04-set-state-deactivate-test.md
```

---

## End-to-End Verification

Each agent **must** run the e2e tests in `tests/e2e/` as part of its own verification
step, not just the unit tests. The e2e suite exercises the full plugin pipeline (scan →
probe → load → process → setState → unload) with real VST3 binaries, including
yabridge-bridged Windows plugins. Unit tests validate logic in isolation; e2e tests
confirm the changes actually work (or don't) against real plugins on the system.

```bash
# Build and run e2e tests (requires VST3 plugins installed on the system)
cmake --build --preset test
ctest --test-dir build-debug --output-on-failure -L e2e
```

E2e tests are tagged `[e2e]` and live in `tests/e2e/`. They are slower than unit tests
(real plugin instantiation, audio processing) and may be skipped in CI if no plugins are
available. **Run them locally before declaring your prompt's task complete.**

---

## Issue Map

| Prompt | Issue | Severity | Discovery |
|--------|-------|----------|-----------|
| 01 | `processContext = nullptr` in `PluginInstance::process()` — tempo-synced plugins crash or misbehave | HIGH | GDB backtrace of yabridge crash during `process()` |
| 02 | No unblock mechanism for ProbeCache blocked plugins (kHs Gate permanently blocked) | MEDIUM | Startup log: "previous load of kHs Gate crashed — marking blocked" |
| 03 | Audio graph race: `createPluginAsync` callback mutates topology while audio thread processes | HIGH | TODO comments in AppController + code review |
| 04 | setState deactivation fix (VST3 spec compliance) has no regression test | MEDIUM | Fix applied in this session, needs test coverage |
