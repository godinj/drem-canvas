# Phase 2 Agent Prompts

Agent prompts for the Sans-JUCE Phase 2 (Audio I/O + MIDI) migration.
Each file is a self-contained prompt for a Claude Code agent.

## Execution Order

```
                  ┌─── 01-midi-types ──────────────┐
                  │                                 ├──→ 05-midi-device-io
Phase 2 start ───┤                                 ├──→ 06-processor-signatures
                  ├─── 02-audio-file-io ───────────┤
                  │                                 ├──→ 04-disk-streamer-recorder
                  └─── 03-audio-device-manager ────┘
```

Agents 01, 02, 03 can run in parallel (no dependencies).
Agents 04, 05, 06 depend on the first tier completing.

## Usage

Launch from the `feature/sans-juce-audio-io` worktree:

```bash
# Tier 1 (parallel)
claude --agent docs/sans-juce/prompts/01-midi-types.md
claude --agent docs/sans-juce/prompts/02-audio-file-io.md
claude --agent docs/sans-juce/prompts/03-audio-device-manager.md

# Tier 2 (after tier 1 merges)
claude --agent docs/sans-juce/prompts/04-disk-streamer-recorder.md
claude --agent docs/sans-juce/prompts/05-midi-device-io.md
claude --agent docs/sans-juce/prompts/06-processor-signatures.md
```

## Specs

- [05-audio-io.md](../05-audio-io.md) — Audio device & file I/O design
- [06-midi-subsystem.md](../06-midi-subsystem.md) — MIDI types & device I/O design
- [08-migration-guide.md](../08-migration-guide.md) — File-by-file checklist
