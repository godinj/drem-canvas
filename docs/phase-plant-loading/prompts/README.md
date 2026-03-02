# Phase Plant Loading — Prompt Execution Plan

## Prompts

| # | Name | Tier | Dependencies | Files Created | Files Modified |
|---|------|------|-------------|---------------|----------------|
| 01 | Phase Plant fixture + CLI support | 1 | — | `tests/fixtures/e2e-phase-plant/session.yaml`, `track-0.yaml`, `capture.sh` | `src/Main.cpp` (optional `--expect-plugin-name` flag) |
| 02 | E2E test + regression test | 2 | 01 | `tests/e2e/test_phase_plant.sh`, `tests/e2e/test_phase_plant_scan.sh`, `tests/regression/issue_NNN_phase_plant_blocked.cpp` | `CMakeLists.txt` |

## Dependency Graph

```
01-phase-plant-fixture (Tier 1)
        │
        ▼
02-e2e-phase-plant-test (Tier 2)
```

## Prerequisites

- Phase Plant installed via yabridge: `~/.vst3/yabridge/Kilohearts/Phase Plant.vst3`
- Yabridge scan serialization fix merged (prompts `docs/fix-scan-ui/prompts/01-*` and `02-*`)
- Wine 9.21 pinned (see CLAUDE.md for details)

## Execution

```bash
# Tier 1
claude --agent docs/phase-plant-loading/prompts/01-phase-plant-fixture.md

# Tier 2 (after Tier 1 merges)
claude --agent docs/phase-plant-loading/prompts/02-e2e-phase-plant-test.md
```

## Validation

After both agents complete:
```bash
cmake --build --preset test
./tests/e2e/test_phase_plant.sh ./build-debug/DremCanvas
./tests/e2e/test_phase_plant_scan.sh ./build-debug/DremCanvas
ctest --test-dir build-debug --output-on-failure -R "phase_plant|yabridge.*blocked"
```
