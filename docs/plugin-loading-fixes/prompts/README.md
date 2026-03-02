# Plugin Loading Fixes — Agent Prompt Index

> Execution order, tier dependencies, and usage instructions for the plugin loading
> fix agent prompts.

---

## Execution Order

```
01-probe-cache-tests ──────────┐
02-plugin-description-tests ───┼──→ 04-multi-plugin-crash
03-vst3-module-tests ──────────┘
                               └──→ 05-uid-and-state-fix
```

### Tier 1 (Parallel — No Dependencies)

| # | Prompt | Scope | Files Created | Files Modified |
|---|--------|-------|---------------|----------------|
| 01 | [01-probe-cache-tests.md](01-probe-cache-tests.md) | ProbeCache YAML round-trip, mtime, pedal, edge cases | `tests/unit/plugins/test_probe_cache.cpp` | `tests/CMakeLists.txt` |
| 02 | [02-plugin-description-tests.md](02-plugin-description-tests.md) | UID hex conversion, toMap/fromMap, legacy format | `tests/unit/plugins/test_plugin_description.cpp` | `tests/CMakeLists.txt` |
| 03 | [03-vst3-module-tests.md](03-vst3-module-tests.md) | isYabridgeBundle, path resolution, load error paths | `tests/unit/plugins/test_vst3_module.cpp` | `tests/CMakeLists.txt` |

### Tier 2 (After Tier 1)

| # | Prompt | Scope | Files Created | Files Modified |
|---|--------|-------|---------------|----------------|
| 04 | [04-multi-plugin-crash.md](04-multi-plugin-crash.md) | GDB investigation, root cause, fix SIGSEGV on 2nd yabridge load | regression test (if applicable) | `src/dc/plugins/VST3Host.cpp` |
| 05 | [05-uid-and-state-fix.md](05-uid-and-state-fix.md) | Fix UID mapping in descriptionFromPropertyTree, handle JUCE state format | `tests/regression/issue_001_*.cpp`, `tests/regression/issue_002_*.cpp` | `src/plugins/PluginHost.cpp`, `src/dc/plugins/PluginInstance.cpp` |

---

## Usage

Run each prompt as a Claude Code agent session:

```bash
cd ~/git/drem-canvas.git/main

# Tier 1 (can run in parallel across terminal sessions)
claude --agent docs/plugin-loading-fixes/prompts/01-probe-cache-tests.md
claude --agent docs/plugin-loading-fixes/prompts/02-plugin-description-tests.md
claude --agent docs/plugin-loading-fixes/prompts/03-vst3-module-tests.md

# Tier 2 (after Tier 1 completes)
claude --agent docs/plugin-loading-fixes/prompts/04-multi-plugin-crash.md
claude --agent docs/plugin-loading-fixes/prompts/05-uid-and-state-fix.md
```

Note: Prompts 04 and 05 can run in parallel with each other — they modify
different files and address different issues.

---

## Specs

Each prompt references the master PRD:

| Prompt | Primary Issue | Secondary Issues |
|--------|--------------|-----------------|
| 01 | Issue 4 (ProbeCache untested) | — |
| 02 | Issue 3 (UID from file path) | — |
| 03 | Issues 5, 6 (entry points, yabridge detection) | — |
| 04 | Issue 1 (SIGSEGV multi-plugin) | — |
| 05 | Issue 3 (UID mapping), Issue 2 (JUCE state format) | — |
