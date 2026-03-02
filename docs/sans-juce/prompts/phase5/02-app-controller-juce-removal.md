# Agent: AppController JUCE Removal

You are working on the `feature/sans-juce-cleanup` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 5 (Final Cleanup): remove all JUCE dependencies from `AppController` —
replace `juce::Timer` with a public `tick()` method, and replace `juce::AlertWindow` /
`juce::String` with native platform calls.

## Context

Read these specs before starting:
- `docs/sans-juce/08-migration-guide.md` (Phase 5 section)
- `src/ui/AppController.h` (line 49: `private juce::Timer` inheritance, line 81: `timerCallback()`)
- `src/ui/AppController.cpp` (line 721: `startTimerHz(30)`, line 40: `stopTimer()`, lines 1997-2002: `juce::String` + `juce::AlertWindow`)
- `src/platform/NativeDialogs.h` (`showAlert(title, message)` — already exists as the replacement)

## Deliverables

### Migration

#### 1. src/ui/AppController.h

Remove JUCE dependency.

- Remove `private juce::Timer` from the class inheritance list (line 49). The class still inherits from `gfx::Widget` and `VimEngine::Listener`.
- Remove `#include <JuceHeader.h>` if it's included (it's pulled in transitively through other headers currently — verify and remove any direct include)
- Remove the `void timerCallback() override;` declaration (line 81)
- Add a new **public** method: `void tick();` — this replaces the timer callback and will be called by the render loop in `Main.cpp`
- No other changes needed. All other members and methods stay the same.

#### 2. src/ui/AppController.cpp

Replace the 6 JUCE references.

**Timer replacement:**
- Rename `timerCallback()` implementation (line 2199) to `tick()` — same body, just a different name
- In `initialise()` (around line 721): remove the `startTimerHz(30);` call. Add a comment:
  ```cpp
  // Meter polling and message queue processing now driven by tick(),
  // called from the platform render loop (~60 Hz).
  ```
- In the destructor (around line 40): remove the `stopTimer();` call

**AlertWindow replacement (lines 1997-2002):**

Replace the `showAudioSettings()` method body. Current code:
```cpp
auto msg = juce::String ("Audio Device: ") + juce::String (deviceName)
         + "\nSample Rate: " + juce::String (audioEngine.getSampleRate()) + " Hz"
         + "\nBuffer Size: " + juce::String (audioEngine.getBufferSize()) + " samples";
juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                        "Audio Settings", msg);
```

Replace with:
```cpp
auto msg = "Audio Device: " + deviceName
         + "\nSample Rate: " + std::to_string (static_cast<int> (audioEngine.getSampleRate())) + " Hz"
         + "\nBuffer Size: " + std::to_string (audioEngine.getBufferSize()) + " samples";
dc::platform::NativeDialogs::showAlert ("Audio Settings", msg);
```

Add `#include "platform/NativeDialogs.h"` to the includes at the top of the file (if not already present).

## Scope Limitation

- Do NOT modify `src/Main.cpp` — that is handled by Agent 04 (Application Entry Point)
- Do NOT modify any `src/gui/` files — they will be deleted by Agent 05
- Do NOT modify `CMakeLists.txt`
- Only modify `src/ui/AppController.h` and `src/ui/AppController.cpp`
- After migration, verify that zero `juce::` references remain in these two files
- The `tick()` method must remain safe to call at any frequency (30-60 Hz). It processes `messageQueue.processAll()` and pushes meter levels — both are idempotent.

## Conventions

- Namespace: `dc::ui`
- JUCE coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods
- Header includes use project-relative paths (e.g., `"platform/NativeDialogs.h"`)
- Build verification: `cmake --build --preset release`
