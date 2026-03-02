# Agent: ProgressBarWidget

You are working on the `feature/plugin-scan-bar` branch of Drem Canvas, a C++17 DAW
using Skia for rendering. Your task is to create a reusable `ProgressBarWidget` in the
graphics widget library.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, architecture)
- `src/graphics/widgets/SliderWidget.h` (reference widget — similar paint/layout pattern)
- `src/graphics/widgets/LabelWidget.h` (text rendering pattern)
- `src/graphics/core/Widget.h` (base class API)
- `src/graphics/rendering/Canvas.h` (available drawing primitives)
- `src/graphics/theme/Theme.h` (color palette and dimensions)
- `src/graphics/theme/FontManager.h` (font access)

## Deliverables

### New files (`src/graphics/widgets/`)

#### 1. `ProgressBarWidget.h`

Reusable horizontal progress bar with percentage text overlay.

```cpp
namespace dc::gfx {

class ProgressBarWidget : public Widget
{
public:
    ProgressBarWidget();

    void paint (Canvas& canvas) override;

    /// Set progress value in range [0.0, 1.0]. Clamps to bounds.
    void setProgress (double progress);
    double getProgress() const;

    /// Optional status text shown left-aligned inside the bar
    /// (e.g., "Scanning Vital..."). Empty string shows only percentage.
    void setStatusText (const std::string& text);
    const std::string& getStatusText() const;

private:
    double progress_ = 0.0;
    std::string statusText_;
};

} // namespace dc::gfx
```

#### 2. `ProgressBarWidget.cpp`

Implementation notes for `paint()`:

- **Track (background):** `fillRoundedRect` with full widget bounds, radius 4px,
  color `theme.sliderTrack`
- **Fill (foreground):** `fillRoundedRect` with width = `getWidth() * progress_`,
  radius 4px, color `theme.accent`
- **Percentage text:** Right-aligned inside the bar with 4px padding. Format: `"NN%"`.
  Use `FontManager::getInstance().getDefaultFont()`, color `theme.brightText`.
  Use `drawTextRight` with a rect inset 4px from the right edge.
- **Status text:** Left-aligned inside the bar with 8px left padding. Use
  `drawText` at vertical center, color `theme.defaultText`. Truncate or elide
  if it would overlap the percentage text (simple approach: just draw both and
  let them overlap — status text is short enough in practice).
- When `progress_` is 0.0, still draw the track but no fill.
- Minimum recommended height: 20px. The widget works at any size the parent gives it.

`setProgress()` must clamp to `[0.0, 1.0]` and call `repaint()`.
`setStatusText()` must call `repaint()`.

### CMakeLists.txt modification

Add to the `# Graphics - Widgets` section (after `LayoutWidget.cpp`):

```
    src/graphics/widgets/ProgressBarWidget.cpp
```

### New test (`tests/unit/test_ProgressBarWidget.cpp`)

Unit test covering:
- Default state: progress 0.0, empty status text
- `setProgress` clamps values below 0.0 and above 1.0
- `setProgress(0.5)` followed by `getProgress()` returns 0.5
- `setStatusText("Scanning...")` followed by `getStatusText()` returns the string

Add the test file to `tests/CMakeLists.txt` in the unit test sources.

## Conventions

- Namespace: `dc::gfx`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"graphics/core/Widget.h"`)
- All new `.cpp` files must be added to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
