# Agent: Progress Bar Wiring

You are working on the `feature/fix-scan-ui` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to fix a bug where the `ProgressBarWidget` never appears during async plugin scanning. Currently only the scan button text updates; the progress bar and status label remain hidden.

## Context

Read these files before starting:
- `src/ui/browser/BrowserWidget.cpp` (`startAsyncScan()` at line 220 — never shows progressBar; `tick()` at line 244 — only updates button text; `resized()` at line 87 — layout logic already handles `scanInProgress_` flag)
- `src/ui/browser/BrowserWidget.h` (members: `progressBar`, `scanStatusLabel`, `scanCurrent_`, `scanTotal_`, `scanInProgress_`)
- `src/graphics/widgets/ProgressBarWidget.h` (API: `setProgress(double)`, `setStatusText(const std::string&)`, `setVisible(bool)`)
- `src/graphics/widgets/LabelWidget.h` (API: `setText(const std::string&)`, `setVisible(bool)`)

## Root Cause

`BrowserWidget::startAsyncScan()` never calls:
- `progressBar.setVisible(true)` to show the progress bar
- `scanStatusLabel.setVisible(true)` to show the status label
- `resized()` to re-layout (the progress bar needs bounds assigned)

`BrowserWidget::tick()` only updates `scanButton.setText()` during scanning and never calls:
- `progressBar.setProgress()` to update the fill level
- `progressBar.setStatusText()` to show the current plugin name
- `scanStatusLabel.setText()` to show the current plugin name

On scan completion, `tick()` never calls:
- `progressBar.setVisible(false)` to hide the progress bar
- `scanStatusLabel.setVisible(false)` to hide the status label
- `resized()` to re-layout after hiding

The `resized()` method already checks `scanInProgress_` and lays out the progress bar and status label correctly when visible — it just never gets called at the right times.

## Deliverables

### Modified files

#### 1. `src/ui/browser/BrowserWidget.h`

Add a new atomic member to pass the current plugin name from the async callback to the UI thread:

- Add `std::atomic<bool> scanNameDirty_ { false };` (signals a new name is ready)
- Add `std::string scanPluginName_;` (written by callback, read by tick — guarded by scanNameDirty_ flag)

Note: `scanPluginName_` is written on the message thread (the progress callback is posted via `MessageQueue`) and read on the message thread in `tick()`, so no mutex is needed — the `scanNameDirty_` atomic provides ordering.

#### 2. `src/ui/browser/BrowserWidget.cpp`

**In `startAsyncScan()`** — after setting `scanInProgress_ = true` and before `pluginManager.scanForPluginsAsync()`:

- Call `progressBar.setProgress (0.0)`
- Call `progressBar.setStatusText ("")`
- Call `progressBar.setVisible (true)`
- Call `scanStatusLabel.setVisible (true)`
- Call `scanStatusLabel.setText ("Preparing scan...")`
- Call `resized()` to lay out the newly visible widgets

**In the progress callback lambda** inside `startAsyncScan()` — capture `name` and update atomics:

Change the progress callback from:
```cpp
[this] (const std::string& /*name*/, int current, int total)
{
    scanCurrent_ = current;
    scanTotal_ = total;
}
```
To:
```cpp
[this] (const std::string& name, int current, int total)
{
    scanCurrent_ = current;
    scanTotal_ = total;
    scanPluginName_ = name;
    scanNameDirty_ = true;
}
```

**In `tick()`** — during active scan (the `else if (scanInProgress_)` branch):

After updating the button text, also update the progress bar and status label:
```cpp
else if (scanInProgress_)
{
    int cur = scanCurrent_;
    int tot = scanTotal_;
    if (tot > 0)
    {
        scanButton.setText ("Scanning " + std::to_string (cur) + "/" + std::to_string (tot));
        progressBar.setProgress (static_cast<double> (cur) / static_cast<double> (tot));

        if (scanNameDirty_.exchange (false))
        {
            progressBar.setStatusText (scanPluginName_);
            scanStatusLabel.setText (scanPluginName_);
        }

        repaint();
    }
}
```

**In `tick()`** — on scan completion (the `if (scanInProgress_ && scanResultReady_)` branch):

After resetting flags and refreshing the plugin list, hide the progress widgets:
```cpp
if (scanInProgress_ && scanResultReady_)
{
    scanInProgress_ = false;
    scanResultReady_ = false;
    scanButton.setText ("Scan Plugins");
    progressBar.setVisible (false);
    scanStatusLabel.setVisible (false);
    refreshPluginList();
    resized();
    repaint();
}
```

## Scope Limitation

- Only modify `BrowserWidget.h` and `BrowserWidget.cpp`
- Do not modify `ProgressBarWidget`, `LabelWidget`, `PluginManager`, `PluginScanner`, or any other files
- Do not change the `paint()` or `resized()` methods (they already handle the layout correctly)
- Do not add new files

## Conventions

- Namespace: `dc::ui`
- Spaces around operators, braces on new line for classes/functions
- `camelCase` methods, `PascalCase` classes
- Build verification: `cmake --build --preset release`
