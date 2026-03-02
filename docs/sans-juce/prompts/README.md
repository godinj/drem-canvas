# Phase 4 — VST3 Plugin Hosting: Agent Prompts

> Replace JUCE plugin hosting with direct Steinberg VST3 SDK integration.

## Prompt Index

| # | Name | Tier | Dependencies | New Files | Migrated Files |
|---|------|------|-------------|-----------|---------------|
| 01 | [VST3 SDK + Module Loading](01-vst3-sdk-module-loading.md) | 1 | — | `VST3Module.h/.cpp`, `PluginDescription.h` | `CMakeLists.txt` |
| 02 | [PluginScanner](02-plugin-scanner.md) | 2 | 01 | `PluginScanner.h/.cpp` | `CMakeLists.txt` |
| 03 | [PluginInstance + ComponentHandler](03-plugin-instance.md) | 2 | 01 | `PluginInstance.h/.cpp`, `ComponentHandler.h/.cpp` | `CMakeLists.txt` |
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

## Verification

After all agents complete:

```bash
# Zero JUCE plugin types
grep -rn "juce::AudioPluginInstance\|juce::AudioProcessorEditor\|juce::AudioPluginFormatManager\|juce::KnownPluginList\|juce::PluginDescription\|juce::PluginDirectoryScanner" src/ \
    --include="*.h" --include="*.cpp" --include="*.mm"
# Expected: zero hits

# Clean build
cmake --build --preset release
```
