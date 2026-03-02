# Agent: Browser Scan UI Integration

You are working on the `feature/plugin-scan-bar` branch of Drem Canvas, a C++17 DAW
using Skia for rendering. Your task is to wire the `ProgressBarWidget` and async
scan API into `BrowserWidget` so users see a visual loading bar with percentage
progress when plugins are scanned.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, architecture)
- `src/ui/browser/BrowserWidget.h` and `src/ui/browser/BrowserWidget.cpp` (current scan button + list)
- `src/graphics/widgets/ProgressBarWidget.h` (progress bar widget from Agent 01)
- `src/plugins/PluginManager.h` (async scan API from Agent 02: `scanForPluginsAsync`, `isScanning`)
- `src/graphics/widgets/LabelWidget.h` (for status text label)
- `src/graphics/widgets/ButtonWidget.h` (existing scan button)
- `src/graphics/theme/Theme.h` (colors and dimensions)

## Dependencies

This agent depends on Agent 01 (ProgressBarWidget) and Agent 02 (Async Plugin Scan).
If those files don't exist yet, create stub headers with the interfaces described in
their prompts and implement against them.

## Deliverables

### Migration

#### 1. `src/ui/browser/BrowserWidget.h`

Add new members:

```cpp
#include "graphics/widgets/ProgressBarWidget.h"
#include "graphics/widgets/LabelWidget.h"
```

New private members:

```cpp
    gfx::ProgressBarWidget progressBar;
    gfx::LabelWidget scanStatusLabel { "", gfx::LabelWidget::Centre };
    bool scanning_ = false;
```

#### 2. `src/ui/browser/BrowserWidget.cpp`

**Constructor changes:**

Add the progress bar and status label as children (initially hidden):

```cpp
addChild (&progressBar);
addChild (&scanStatusLabel);
progressBar.setVisible (false);
scanStatusLabel.setVisible (false);
scanStatusLabel.setFontSize (11.0f);
```

Replace the `scanButton.onClick` handler:

```cpp
scanButton.onClick = [this]()
{
    if (pluginManager.isScanning())
        return;  // already in progress

    scanning_ = true;
    scanButton.setEnabled (false);
    progressBar.setProgress (0.0);
    progressBar.setStatusText ("Starting scan...");
    progressBar.setVisible (true);
    scanStatusLabel.setVisible (true);
    scanStatusLabel.setText ("Starting scan...");
    resized();  // re-layout to show progress bar

    pluginManager.scanForPluginsAsync (
        // onProgress (message thread)
        [this] (const std::string& pluginName, int current, int total)
        {
            double pct = (total > 0) ? static_cast<double> (current) / total : 0.0;
            progressBar.setProgress (pct);

            std::string status = pluginName + "  " + std::to_string (current)
                                 + "/" + std::to_string (total);
            progressBar.setStatusText (status);
            scanStatusLabel.setText (status);
        },
        // onComplete (message thread)
        [this]()
        {
            scanning_ = false;
            scanButton.setEnabled (true);
            progressBar.setVisible (false);
            scanStatusLabel.setVisible (false);
            refreshPluginList();
            resized();  // re-layout to hide progress bar
        }
    );
};
```

**`resized()` changes:**

Layout the progress bar between the scan button and the search field when scanning:

```cpp
void BrowserWidget::resized()
{
    float w = getWidth();
    float h = getHeight();
    float y = 4.0f;

    // Scan button — always at top
    scanButton.setBounds (4.0f, y, w - 8.0f, 28.0f);
    y += 32.0f;

    // Progress bar — only visible during scan
    if (scanning_)
    {
        progressBar.setBounds (4.0f, y, w - 8.0f, 20.0f);
        y += 24.0f;
        scanStatusLabel.setBounds (4.0f, y, w - 8.0f, 16.0f);
        y += 20.0f;
    }

    // Search field is drawn in paint() at current y, height=searchFieldHeight
    // Update the search field Y offset for paint() to use
    searchFieldY_ = y;
    float listTop = y + searchFieldHeight + 4.0f;
    pluginList.setBounds (0, listTop, w, h - listTop);
}
```

This requires adding a `float searchFieldY_ = 36.0f;` private member to the header
and updating `paint()` to use `searchFieldY_` instead of the hardcoded `36.0f`:

```cpp
// In paint(), replace:
//   float searchY = 36.0f;
// with:
float searchY = searchFieldY_;
```

**`paint()` changes:**

Only the `searchY` variable changes as described above. No other paint changes needed —
the progress bar and label paint themselves as child widgets.

## Visual Layout (scanning state)

```
+---------------------------+
| [Scan Plugins] (disabled) |  <- scanButton (28px)
| [============>    ]  67%  |  <- progressBar (20px)
|  Vital  8/12              |  <- scanStatusLabel (16px)
| [Filter plugins...]       |  <- search field (28px)
| Plugin 1                  |  <- pluginList
| Plugin 2                  |
| ...                       |
+---------------------------+
```

## Visual Layout (idle state)

```
+---------------------------+
| [Scan Plugins]            |  <- scanButton (28px)
| [Filter plugins...]       |  <- search field (28px)
| Plugin 1                  |  <- pluginList
| Plugin 2                  |
| ...                       |
+---------------------------+
```

## Conventions

- Namespace: `dc::ui`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"graphics/widgets/ProgressBarWidget.h"`)
- Build verification: `cmake --build --preset release`
