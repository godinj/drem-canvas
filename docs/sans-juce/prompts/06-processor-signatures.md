# Agent: Processor Signature Migration

You are working on the `feature/sans-juce-audio-io` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 2 of the Sans-JUCE migration: update all audio processor signatures
from JUCE buffer types to dc:: buffer types.

## Context

Read these specs before starting:
- `docs/sans-juce/08-migration-guide.md` (Phase 2 "Files to migrate" table)
- `docs/sans-juce/02-audio-graph.md` (AudioBlock, MidiBlock design — Phase 3 types)

## IMPORTANT: Scope Limitation

In Phase 2, processors still inherit from `juce::AudioProcessor` (that changes in Phase 3).
Your job is ONLY to replace the buffer types used INSIDE `processBlock`:

- `juce::AudioBuffer<float>` → use `float**` / AudioBlock views where possible
- `juce::MidiBuffer` → `dc::MidiBuffer` (or conversion at boundary)
- `juce::MidiMessage` → `dc::MidiMessage`

If a clean swap isn't possible because `processBlock`'s JUCE signature requires
`juce::AudioBuffer` / `juce::MidiBuffer` parameters, create thin conversion shims at the
boundary (convert juce→dc at top of `processBlock`, dc→juce at bottom if needed).

## Dependencies

This agent depends on Agent 01 (MidiMessage, MidiBuffer). Coordinate with Agent 01.

## Files to migrate

All processors that use MidiMessage/MidiBuffer internally:

### 1. src/engine/TrackProcessor.h/.cpp
`juce::MidiBuffer` internal usage → `dc::MidiBuffer`

### 2. src/engine/MidiClipProcessor.h/.cpp
MidiMessage factory calls, MidiBuffer iteration

### 3. src/engine/StepSequencerProcessor.h/.cpp
Same pattern

### 4. src/engine/SimpleSynthProcessor.h
MidiMessage queries in processBlock

### 5. src/engine/MetronomeProcessor.h/.cpp
MidiBuffer pass-through

### 6. src/engine/MixBusProcessor.h/.cpp
AudioBuffer → AudioBlock-style access, MidiBuffer pass-through

### 7. src/engine/MeterTapProcessor.h/.cpp
AudioBuffer read access

### 8. src/engine/BounceProcessor.h/.cpp
`AudioFormatWriter` → `dc::AudioFileWriter`

## Migration Pattern

For each processor:
1. Add conversion at the top of `processBlock`: wrap `juce::AudioBuffer` as `float**` and
   convert `juce::MidiBuffer` → `dc::MidiBuffer`
2. Replace all internal `juce::MidiMessage` usage with `dc::MidiMessage`
3. Replace all internal `juce::MidiBuffer` iteration with `dc::MidiBuffer` iteration

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Build verification: `cmake --build --preset release`
