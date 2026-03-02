# Phase 4 — VST3 Plugin Hosting: Agent Prompts

> Replace JUCE plugin hosting with direct Steinberg VST3 SDK integration.

## Prompt Index

| # | Name | Tier | Dependencies | New Files | Migrated Files |
|---|------|------|-------------|-----------|---------------|
| 01 | [VST3 SDK + Module Loading](01-vst3-sdk-module-loading.md) | 1 | — | `VST3Module.h/.cpp`, `PluginDescription.h` | `CMakeLists.txt` |
| 02 | [PluginScanner](02-plugin-scanner.md) | 2 | 01 | `PluginScanner.h/.cpp` | `CMakeLists.txt` |
| 03 | [PluginInstance + ComponentHandler](03-plugin-instance.md) | 2 | 01 | `PluginInstance.h/.cpp`, `ComponentHandler.h/.cpp`, stubs: `AudioNode.h`, `MidiBlock.h` | `CMakeLists.txt` |
| 04 | [PluginEditor](04-plugin-editor.md) | 3 | 03 | `PluginEditor.h/.cpp` | `CMakeLists.txt` |
| 05 | [VST3Host + Migration](05-vst3-host-migration.md) | 3 | 02, 03 | `VST3Host.h/.cpp` | `PluginManager.h/.cpp`, `PluginHost.h/.cpp` |
| 06 | [Platform Bridges + Cleanup](06-platform-bridges-cleanup.md) | 4 | 04, 05 | — | `PluginEditorBridge.h/.cpp`, `PluginWindowManager.h/.cpp`, `ParameterFinderScanner.h/.cpp`, `VST3ParameterFinderSupport.h` (delete), `MacPluginEditorBridge.h/.mm`, `X11PluginEditorBridge.h/.cpp`, `EmbeddedPluginEditor.h/.cpp`, `TrackProcessor.h/.cpp`, `scripts/bootstrap.sh`, `scripts/juce-patches/` (delete) |

## Dependency Graph

```
01 (VST3 SDK + Module + Description)
  ├──→ 02 (PluginScanner)            ──┐
  └──→ 03 (PluginInstance + Handler)  ──┼──→ 05 (VST3Host + Migration) ──┐
            └──→ 04 (PluginEditor) ──────────────────────────────────────┼──→ 06 (Bridges + Cleanup)
```

## Execution Order

### Tier 1 (sequential — foundation)

```bash
claude -p docs/sans-juce/prompts/01-vst3-sdk-module-loading.md
```

### Tier 2 (parallel — after Tier 1 merges)

```bash
claude -p docs/sans-juce/prompts/02-plugin-scanner.md &
claude -p docs/sans-juce/prompts/03-plugin-instance.md &
wait
```

### Tier 3 (parallel — after Tier 2 merges)

```bash
claude -p docs/sans-juce/prompts/04-plugin-editor.md &
claude -p docs/sans-juce/prompts/05-vst3-host-migration.md &
wait
```

### Tier 4 (sequential — after Tier 3 merges)

```bash
claude -p docs/sans-juce/prompts/06-platform-bridges-cleanup.md
```

## File Summary

### New files created (src/dc/)

| File | Agent | Purpose |
|------|-------|---------|
| `dc/plugins/VST3Module.h/.cpp` | 01 | dlopen-based VST3 bundle loading |
| `dc/plugins/PluginDescription.h` | 01 | Plugin metadata struct |
| `dc/plugins/PluginScanner.h/.cpp` | 02 | Out-of-process scanning with crash isolation |
| `dc/plugins/ComponentHandler.h/.cpp` | 03 | IComponentHandler impl (performEdit snoop) |
| `dc/plugins/PluginInstance.h/.cpp` | 03 | VST3 component wrapper, AudioNode interface |
| `dc/engine/AudioNode.h` | 03 | Phase 3 stub — minimal processor interface |
| `dc/engine/MidiBlock.h` | 03 | Phase 3 stub — non-owning MidiBuffer view |
| `dc/plugins/PluginEditor.h/.cpp` | 04 | IPlugView lifecycle + window embedding |
| `dc/plugins/VST3Host.h/.cpp` | 05 | Top-level host (scan, create, database) |

### Existing files migrated

| File | Agent | Key change |
|------|-------|-----------|
| `plugins/PluginManager.h/.cpp` | 05 | KnownPluginList → VST3Host database |
| `plugins/PluginHost.h/.cpp` | 05 | AudioPluginInstance → PluginInstance |
| `plugins/PluginEditorBridge.h/.cpp` | 06 | AudioProcessorEditor → PluginEditor |
| `plugins/PluginWindowManager.h/.cpp` | 06 | DocumentWindow → lightweight tracker |
| `plugins/ParameterFinderScanner.h/.cpp` | 06 | Native IParameterFinder via PluginInstance |
| `plugins/VST3ParameterFinderSupport.h` | 06 | Deleted (native SDK access) |
| `platform/MacPluginEditorBridge.h/.mm` | 06 | JUCE Component → dc::PluginEditor |
| `platform/linux/X11PluginEditorBridge.h/.cpp` | 06 | Same |
| `platform/linux/EmbeddedPluginEditor.h/.cpp` | 06 | Same |
| `engine/TrackProcessor.h/.cpp` | 06 | Plugin chain type updates |
| `scripts/juce-patches/` | 06 | Deleted entirely |
| `scripts/bootstrap.sh` | 06 | Remove JUCE patch code |

## Verification

After all agents complete:

```bash
# Zero JUCE plugin types
grep -rn "juce::AudioPluginInstance\|juce::AudioProcessorEditor\|juce::AudioPluginFormatManager\|juce::KnownPluginList\|juce::PluginDescription\|juce::PluginDirectoryScanner" src/ \
    --include="*.h" --include="*.cpp" --include="*.mm"
# Expected: zero hits

# Clean build
cmake --build --preset release

# Functional tests:
# 1. Scan for plugins (verify all VST3s found)
# 2. Load a VST3 plugin, verify audio processing
# 3. Open plugin editor, verify UI displays
# 4. Save/restore plugin state round-trip
# 5. Parameter finder overlay works
# 6. yabridge-bridged plugin works
# 7. Existing sessions load correctly
```
