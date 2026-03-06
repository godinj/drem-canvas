# Vim Engine Overhaul — Agent Prompts

Decouple the vim engine from DAW internals and make keybindings user-configurable.

## Prompts

| # | Name | Tier | Depends | Creates | Modifies |
|---|------|------|---------|---------|----------|
| 01 | vim-grammar | 1 | — | `VimGrammar.h/.cpp`, `test_vim_grammar.cpp` | `VimEngine.h/.cpp`, `CMakeLists.txt` |
| 02 | keymap-infrastructure | 1 | — | `KeySequence.h/.cpp`, `KeymapRegistry.h/.cpp`, `default_keymap.yaml`, `test_key_sequence.cpp`, `test_keymap_registry.cpp` | `CMakeLists.txt` |
| 03 | context-adapter-editor | 2 | 01 | `ContextAdapter.h`, `EditorAdapter.h/.cpp` | `VimEngine.h/.cpp`, `AppController.cpp`, `test_vim_commands.cpp`, `CMakeLists.txt` |
| 04 | secondary-adapters | 2 | 01, 03 | `MixerAdapter.h/.cpp`, `SequencerAdapter.h/.cpp`, `PianoRollAdapter.h/.cpp`, `PluginViewAdapter.h/.cpp` | `VimEngine.h/.cpp`, `AppController.cpp`, `CMakeLists.txt` |
| 05 | dispatch-integration | 3 | 01-04 | — | `VimEngine.h/.cpp`, `AppController.cpp`, `ActionRegistry.h/.cpp` |

## Dependency Graph

```
01-vim-grammar ──┬──→ 03-context-adapter-editor ──┬──→ 05-dispatch-integration
                 │                                 │
02-keymap-infra ─┤    04-secondary-adapters ───────┘
                 │         ↑
                 └─────────┘
```

## Execution

```bash
# Tier 1 (parallel)
claude -p docs/vim-overhaul/prompts/01-vim-grammar.md
claude -p docs/vim-overhaul/prompts/02-keymap-infrastructure.md

# Tier 2 (after Tier 1 merges)
claude -p docs/vim-overhaul/prompts/03-context-adapter-editor.md
claude -p docs/vim-overhaul/prompts/04-secondary-adapters.md

# Tier 3 (after Tier 2 merges)
claude -p docs/vim-overhaul/prompts/05-dispatch-integration.md
```

## Verification

After each agent completes:
```bash
cmake --build --preset release
cmake --preset test && cmake --build --preset test
ctest --test-dir build-debug --output-on-failure -j$(nproc)
scripts/verify.sh
```
