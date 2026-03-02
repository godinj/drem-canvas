# Drem Canvas — DAW Project

## Build

```bash
cmake --build --preset release
```

Test build:
```bash
cmake --preset test
cmake --build --preset test
ctest --test-dir build-debug --output-on-failure -j$(nproc)
```

Run (Linux):
```bash
./build/DremCanvas
```

## Architecture

- **C++17** with **Skia** for rendering, **PortAudio** for audio I/O, **RtMidi** for MIDI, **VST3 SDK** for plugin hosting, **yaml-cpp** for serialization
- **GPU rendering**: Skia + Metal (macOS) / Skia + Vulkan (Linux)
- **Windowing**: Native Cocoa/MTKView (macOS) / GLFW 3 (Linux)
- Namespace: `dc`
- `src/engine/` — Real-time audio (never allocates or locks on audio thread)
- `src/model/` — Data model using `dc::PropertyTree` (message thread only)
- `src/gui/` — Presentational components observing model state
- `src/vim/` — Vim modal navigation (intercepts keys before child widgets)
- `src/plugins/` — VST3 plugin hosting
- `src/utils/` — Helpers (undo, audio file utils)
- `src/platform/` — macOS native layer (`.mm` files)
- `src/platform/linux/` — Linux platform layer (GLFW, Vulkan, zenity dialogs)
- `src/graphics/rendering/GpuBackend.h` — Abstract GPU interface (MetalBackend / VulkanBackend)

## Conventions

- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"model/Project.h"`)
- All new `.cpp` files must be added to `target_sources` in `CMakeLists.txt`
- Always verify with `cmake --build --preset release` after changes

## Testing

```bash
cmake --preset test
cmake --build --preset test
ctest --test-dir build-debug --output-on-failure -j$(nproc)
```

IMPORTANT: Run `scripts/verify.sh` before declaring any task complete.

### Agent Guardrails (Hooks)

- **Tier 1** (`scripts/quick-check.sh`): Runs after every Edit/Write. Checks real-time safety on changed engine files.
- **Tier 2** (`scripts/verify.sh`): Runs when the agent session ends. Full build, architecture check, tests, golden file comparisons.

## Parallel Feature Branches

Managed via `wt` (dotfiles worktree tool). Bare repo at `~/git/drem-canvas.git/` with nested worktrees.

| Worktree | Branch | Scope |
|----------|--------|-------|
| `main/` | `master` | Integration branch |
| `feature/vim-commands/` | `feature/vim-commands` | Phase 5: operators, visual mode, command line, search, registers |
| `feature/advanced-editing/` | `feature/advanced-editing` | Phase 11: clip drag/trim/fades, crossfades, undo polish, tempo map |
| `feature/git-integration/` | `feature/git-integration` | Phase 12: git commands, semantic diff, bounce/export, automation |
| `feature/mixer-implementation/` | `feature/mixer-implementation` | Phase 6: vim mixer context, fader/pan modes, strip selection, master bus |
| `feature/fix-scan-ui/` | `feature/fix-scan-ui` | Fix yabridge scan blocking + progress bar UI |

Create new feature: `wt new feature/my-feature`
Remove feature: `wt rm feature/my-feature`

Merge workflow (from `main/`):
```bash
git merge feature/vim-commands
git merge feature/mixer-implementation
git merge feature/advanced-editing
git merge feature/git-integration
```
