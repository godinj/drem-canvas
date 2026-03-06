# IRunLoop Implementation — Agent Prompts

## Summary

| # | Name | Tier | Dependencies | New Files | Modified Files |
|---|------|------|-------------|-----------|----------------|
| 01 | LinuxRunLoop Class | 1 | None | `LinuxRunLoop.h`, `LinuxRunLoop.cpp` | `Main.cpp`, `CMakeLists.txt` |
| 02 | Wire queryInterface | 1 | 01 (stub OK) | None | `PluginEditor.cpp`, `ComponentHandler.cpp`, `PluginInstance.cpp` |

## Execution

Both agents can run in parallel — Agent 02 can stub the header if Agent 01 hasn't finished yet.

```bash
# Parallel (both Tier 1)
claude --agent docs/linux-runloop/prompts/01-linux-runloop-class.md &
claude --agent docs/linux-runloop/prompts/02-wire-queryinterface.md &
wait
```

## Verification

After both agents complete:

```bash
cmake --build --preset release
./build/DremCanvas   # Open a Kilohearts plugin editor — should not freeze
```

Check for `[RunLoop] registered event handler` in log output to confirm yabridge registered its FDs.
