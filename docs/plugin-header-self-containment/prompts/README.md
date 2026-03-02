# Plugin Header Self-Containment Fix — Agent Prompts

## Prompt Summary

| # | Name | Tier | Dependencies | Files Created | Files Modified |
|---|------|------|-------------|---------------|----------------|
| 01 | Fix VST3 SDK Include Path | 1 | None | None | `scripts/check_architecture.sh` |

## Dependency Graph

```
01-fix-vst3-include-path  (standalone)
```

## Execution

```bash
# Tier 1 (single prompt)
claude -p docs/plugin-header-self-containment/prompts/01-fix-vst3-include-path.md
```
