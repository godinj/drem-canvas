# Fix Routing — Agent Prompts

Fix MIDI routing to VST3 plugins (Phase Plant patch not applied). Three root causes: null ProcessContext, dropped MIDI CC, null IParameterChanges.

## Prompts

| # | Name | Tier | Dependencies | New Files | Modified Files |
|---|------|------|-------------|-----------|----------------|
| 01 | process-context | 1 | — | `ProcessContextBuilder.h/.cpp`, `test_plugin_process_context.cpp` | `TransportController.h`, `CMakeLists.txt`, `tests/CMakeLists.txt` |
| 02 | parameter-changes | 1 | — | `ParameterChangeQueue.h/.cpp`, `MidiCCMapper.h/.cpp`, `test_parameter_changes.cpp`, `test_cc_to_param_translation.cpp` | `CMakeLists.txt`, `tests/CMakeLists.txt` |
| 03 | bounce-cli | 1 | — | `test_phase_plant_routing.sh`, `e2e-phase-plant-routing/` fixture | `Main.cpp`, `tests/CMakeLists.txt` |
| 04 | plugin-instance-wiring | 2 | 01, 02 | — | `PluginInstance.h/.cpp`, `AppController.cpp`, `CMakeLists.txt` |

## Dependency Graph

```
01-process-context ──┐
                     ├──▶ 04-plugin-instance-wiring
02-parameter-changes ┤
                     │
03-bounce-cli ───────┘
```

## Execution

```bash
# Tier 1 (parallel)
/swarm docs/fix-routing/prompts
```
