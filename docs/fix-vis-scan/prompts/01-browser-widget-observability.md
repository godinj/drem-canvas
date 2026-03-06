# Agent: BrowserWidget Observability

You are working on the `feature/fix-vis-scan` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to add test-observable query methods to `BrowserWidget` so the e2e smoke harness can verify the async scan UI lifecycle.

## Context

Read these files before starting:
- `src/ui/browser/BrowserWidget.h` (class definition — note existing `startAsyncScan()`, `tick()`, and the `scanInProgress_` atomic)
- `src/ui/browser/BrowserWidget.cpp` (`startAsyncScan()` at line 220 — shows progress bar; `tick()` at line 253 — updates and hides progress bar)
- `src/graphics/core/Node.h` (line 50-51 — `setVisible(bool)` / `isVisible()` API inherited by all widgets)

## Deliverables

### Modified files

#### 1. `src/ui/browser/BrowserWidget.h`

Add two public query methods after `tick()` (line 47):

```cpp
    /// Returns true if the progress bar was visible at any point during the
    /// most recent async scan.  Resets on each new scan start.
    bool wasProgressBarVisible() const { return progressBarWasVisible_; }

    /// Returns true while an async scan is in progress.
    bool isScanInProgress() const { return scanInProgress_.load(); }
```

Add one private member after `scanPluginName_` (line 70):

```cpp
    bool progressBarWasVisible_ = false;
```

#### 2. `src/ui/browser/BrowserWidget.cpp`

**In `startAsyncScan()`** — after line 225 (`scanInProgress_ = true;`), reset the latch:

```cpp
    progressBarWasVisible_ = false;
```

**In `tick()`** — at the top of the `else if (scanInProgress_)` branch (line 266), before the existing `int cur = scanCurrent_;` line, add:

```cpp
        if (progressBar.isVisible())
            progressBarWasVisible_ = true;
```

### Thread safety note

`progressBarWasVisible_` is written in `startAsyncScan()` and `tick()`, and read by the e2e harness — all on the message thread. No synchronization is needed; a plain `bool` is correct.

The latch triggers on the very first `tick()` after `startAsyncScan()` because `progressBar.setVisible(true)` is called synchronously before `scanForPluginsAsync()` launches the background thread — zero race window.

## Scope Limitation

- Only modify `BrowserWidget.h` and `BrowserWidget.cpp`
- Do not modify `ProgressBarWidget`, `LabelWidget`, `PluginManager`, `AppController`, `Main.cpp`, or any other files
- Do not change `paint()`, `resized()`, `startAsyncScan()`'s existing logic, or `tick()`'s existing logic — only add the latch check and reset
- Do not add new files

## Conventions

- Namespace: `dc::ui`
- Spaces around operators, braces on new line for classes/functions
- `camelCase` methods, `PascalCase` classes
- Build verification: `cmake --build --preset release`
