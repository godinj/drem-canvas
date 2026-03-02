# Test Harness — Agent Prompt Index

> Execution order, tier dependencies, and usage instructions for the test harness
> agent prompts.

---

## Execution Order

```
                                    ┌─── 03-foundation-tests ───┐
                                    │                           │
01-cmake-infrastructure ──→ 02-verification-scripts ──→ 04-model-tests ──────→ 06-integration-tests
                                    │                           │
                                    ├─── 05-midi-audio-tests ───┘
                                    │
                                    └─── 07-agent-guardrails
```

### Tier 1 (Sequential — Must Complete First)

| # | Prompt | Phase | Deliverables |
|---|--------|-------|-------------|
| 01 | [01-cmake-infrastructure.md](01-cmake-infrastructure.md) | 1 | dc:: library targets, Catch2, test presets, `tests/` directory |
| 02 | [02-verification-scripts.md](02-verification-scripts.md) | 2 | `check_architecture.sh`, `#error` guards, `.clang-tidy` |

### Tier 2 (Parallel — After Tier 1)

| # | Prompt | Phase | Deliverables |
|---|--------|-------|-------------|
| 03 | [03-foundation-tests.md](03-foundation-tests.md) | 3 | 8 test files for dc::foundation |
| 04 | [04-model-tests.md](04-model-tests.md) | 4 | 4 test files for dc::model |
| 05 | [05-midi-audio-tests.md](05-midi-audio-tests.md) | 5–6 | 7 test files for dc::midi + dc::audio |

### Tier 3 (After Tier 2)

| # | Prompt | Phase | Deliverables |
|---|--------|-------|-------------|
| 06 | [06-integration-tests.md](06-integration-tests.md) | 7–8 | Integration tests, higher-level model tests |
| 07 | [07-agent-guardrails.md](07-agent-guardrails.md) | 9 | `quick-check.sh`, `verify.sh`, hooks config |

---

## Usage

Run each prompt as a Claude Code agent session on the `feature/test-harness` branch:

```bash
# Tier 1 (sequential)
cd ~/git/drem-canvas.git/feature/test-harness
claude --agent docs/test-harness/prompts/01-cmake-infrastructure.md
claude --agent docs/test-harness/prompts/02-verification-scripts.md

# Tier 2 (can run in parallel across worktrees)
claude --agent docs/test-harness/prompts/03-foundation-tests.md
claude --agent docs/test-harness/prompts/04-model-tests.md
claude --agent docs/test-harness/prompts/05-midi-audio-tests.md

# Tier 3 (after Tier 2 merges)
claude --agent docs/test-harness/prompts/06-integration-tests.md
claude --agent docs/test-harness/prompts/07-agent-guardrails.md
```

---

## Specs

Each prompt references one or more design specs:

| Prompt | Primary Spec | Secondary Specs |
|--------|-------------|-----------------|
| 01 | [01-cmake-infrastructure.md](../01-cmake-infrastructure.md) | — |
| 02 | [03-architecture-guards.md](../03-architecture-guards.md) | — |
| 03 | [02-migration-tests.md](../02-migration-tests.md) | [../sans-juce/04-foundation-types.md](../../sans-juce/04-foundation-types.md) |
| 04 | [02-migration-tests.md](../02-migration-tests.md) | [../sans-juce/01-observable-model.md](../../sans-juce/01-observable-model.md) |
| 05 | [02-migration-tests.md](../02-migration-tests.md) | [../sans-juce/06-midi-subsystem.md](../../sans-juce/06-midi-subsystem.md), [../sans-juce/05-audio-io.md](../../sans-juce/05-audio-io.md) |
| 06 | [05-integration-and-advanced.md](../05-integration-and-advanced.md) | — |
| 07 | [04-agent-management.md](../04-agent-management.md) | [03-architecture-guards.md](../03-architecture-guards.md) |
