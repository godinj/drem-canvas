# Phase 5 — Final Cleanup: Agent Prompts

> Remove JUCE entirely. Delete legacy GUI layer, migrate remaining JUCE types,
> clean up build system. Zero `juce::` symbols when complete.

## Prompt Index

| # | Name | Tier | Dependencies | New Files | Migrated Files |
|---|------|------|-------------|-----------|---------------|
| 01 | [VimEngine Key Migration](01-vim-engine-key-migration.md) | 1 | — | `dc/foundation/keycode.h` | `VimEngine.h/.cpp`, `VirtualKeyboardState.h` |
| 02 | [AppController JUCE Removal](02-app-controller-juce-removal.md) | 1 | — | — | `AppController.h/.cpp` |
| 03 | [Stale JuceHeader Cleanup](03-stale-juceheader-cleanup.md) | 1 | — | — | `VimContext.h`, `TransportController.h`, `MidiClip.h`, `TrackLaneWidget.h`, `NoteGridWidget.h`, `PluginSlotListWidget.h` |
| 04 | [Application Entry Point](04-app-entry-point.md) | 2 | 02 | — | `Main.cpp` |
| 05 | [GUI Deletion + Build Cleanup](05-gui-deletion-build-cleanup.md) | 3 | 01, 02, 03, 04 | — | `CMakeLists.txt`, `.gitmodules`, `bootstrap.sh`, `check_deps.sh`, 49 gui/ files (deleted), 6 dc/ comment cleanups |

## Dependency Graph

```
01 (VimEngine) ────────────────────────────────┐
02 (AppController) ──→ 04 (Main.cpp Entry) ────┼──→ 05 (GUI + Build Cleanup)
03 (JuceHeader) ───────────────────────────────┘
```

## Execution Order

### Tier 1 (parallel — no interdependencies)

```bash
claude -p docs/sans-juce/prompts/phase5/01-vim-engine-key-migration.md &
claude -p docs/sans-juce/prompts/phase5/02-app-controller-juce-removal.md &
claude -p docs/sans-juce/prompts/phase5/03-stale-juceheader-cleanup.md &
wait
```

### Tier 2 (after Tier 1 Agent 02 merges)

```bash
claude -p docs/sans-juce/prompts/phase5/04-app-entry-point.md
```

### Tier 3 (after all Tier 1 + 2 merge)

```bash
claude -p docs/sans-juce/prompts/phase5/05-gui-deletion-build-cleanup.md
```

## Verification

After all agents complete:

```bash
# Zero JUCE symbols
grep -rn "juce::" src/ --include="*.h" --include="*.cpp" --include="*.mm"
# Expected: zero hits

# Zero JUCE includes
grep -rn "JuceHeader\|juce_" src/ --include="*.h" --include="*.cpp" --include="*.mm"
# Expected: zero hits

# No JUCE in CMake
grep -n "juce" CMakeLists.txt
# Expected: zero hits

# Clean build from scratch
rm -rf build/
cmake --preset release
cmake --build --preset release

# No JUCE symbols in binary (Linux)
nm build/DremCanvas_artefacts/Release/DremCanvas 2>/dev/null | grep -i juce
# Expected: zero hits

# App launches
./build/DremCanvas_artefacts/Release/DremCanvas
```
