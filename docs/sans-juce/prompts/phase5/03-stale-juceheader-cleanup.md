# Agent: Stale JuceHeader Include Cleanup

You are working on the `feature/sans-juce-cleanup` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 5 (Final Cleanup): remove vestigial `#include <JuceHeader.h>` from
source files that no longer use any JUCE types. These includes are left over from earlier
migration phases and are now dead weight.

## Context

Read these specs before starting:
- `docs/sans-juce/08-migration-guide.md` (Phase 5 section)

The following non-gui files still include `<JuceHeader.h>` but grep for `juce::` returns
zero hits in their contents (the JUCE types they once used were already migrated in
Phases 0-4):

| File | Why it had JuceHeader | Already migrated to |
|------|----------------------|-------------------|
| `src/vim/VimContext.h` | `juce::String` | `std::string` (Phase 0) |
| `src/engine/TransportController.h` | `juce::AudioProcessor`, `juce::Time` | `dc::AudioNode`, `dc::currentTimeMillis` (Phase 0/3) |
| `src/model/MidiClip.h` | `juce::MidiMessageSequence`, `juce::MemoryBlock` | `dc::MidiSequence`, `std::vector<uint8_t>` (Phase 0/2) |
| `src/ui/arrangement/TrackLaneWidget.h` | `juce::ValueTree` (comment only) | `dc::PropertyTree` (Phase 1) |
| `src/ui/midieditor/NoteGridWidget.h` | unknown | verify and remove |
| `src/ui/mixer/PluginSlotListWidget.h` | unknown | verify and remove |

## Deliverables

### Migration

For each file listed below, perform these steps:

1. Open the file and confirm `#include <JuceHeader.h>` is present
2. Search the **entire file** (including the corresponding `.cpp` if it exists) for any `juce::` or `juce_wchar` references
3. If zero JUCE references exist: remove the `#include <JuceHeader.h>` line
4. If the file needs standard library headers that were transitively provided by JuceHeader, add them explicitly (e.g., `<string>`, `<vector>`, `<cstdint>`, `<memory>`, `<functional>`)
5. Build to verify

#### 1. src/vim/VimContext.h

- Remove `#include <JuceHeader.h>`
- Likely needs: `#include <string>`, `#include <vector>` (verify actual usage)

#### 2. src/engine/TransportController.h

- Remove `#include <JuceHeader.h>`
- Likely needs: `#include <atomic>`, `#include <cstdint>` (verify actual usage — TransportController uses `std::atomic<int64_t>`)

#### 3. src/model/MidiClip.h

- Remove `#include <JuceHeader.h>`
- Likely needs: `#include <string>`, `#include <vector>`, `#include <cstdint>` (verify actual usage)

#### 4. src/ui/arrangement/TrackLaneWidget.h

- Remove `#include <JuceHeader.h>` (line 9 has comment: `// juce::ValueTree (Phase 3 migration)`)
- Remove the trailing comment as well — the migration is done
- Add any needed standard headers

#### 5. src/ui/midieditor/NoteGridWidget.h

- Read the file, verify no `juce::` usage
- Remove `#include <JuceHeader.h>`
- Add any needed standard headers

#### 6. src/ui/mixer/PluginSlotListWidget.h

- Read the file, verify no `juce::` usage
- Remove `#include <JuceHeader.h>`
- Add any needed standard headers

### Verification for each file

After removing the include from each file, run:
```bash
cmake --build --preset release 2>&1 | head -50
```

If compilation fails with missing type errors, add the specific standard library header
that was being provided transitively by JuceHeader.h. Common transitive includes:
- `<string>` (std::string)
- `<vector>` (std::vector)
- `<memory>` (std::unique_ptr, std::shared_ptr)
- `<functional>` (std::function)
- `<cstdint>` (int64_t, uint8_t)
- `<atomic>` (std::atomic)
- `<algorithm>` (std::min, std::max)

## Scope Limitation

- Do NOT modify any `src/gui/` files — they will be deleted by Agent 05
- Do NOT modify `src/vim/VimEngine.h` or `src/vim/VirtualKeyboardState.h` — those are handled by Agent 01
- Do NOT modify `src/ui/AppController.h` — that is handled by Agent 02
- Do NOT modify `src/Main.cpp` — that is handled by Agent 04
- If a file still has active `juce::` references, do NOT remove its JuceHeader include. Flag it and skip.
- Only remove includes, add standard headers. No functional changes.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, `camelCase` methods
- Header includes use `<angle brackets>` for system/standard headers, `"quotes"` for project headers
- Build verification: `cmake --build --preset release`
