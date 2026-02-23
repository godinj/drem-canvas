# Plan: Sequencer Horizontal & Vertical Zoom

## Context

The step sequencer grid has hardcoded 32×32px cells with no zoom capability. Users can scroll via the Viewport but cannot change the visual scale. This makes it hard to work with patterns that have many steps (up to 64) or many rows. Adding zoom improves usability and follows the same UI pattern expected in any DAW grid editor.

## Approach

Convert `StepGrid`'s static cell dimensions to zoomable instance variables. Add Ctrl+scroll-wheel zoom on `StepGrid` (forwarded to `StepSequencerView` for viewport position preservation) and Vim `z`-prefix keybindings (`zi`/`zo`/`zI`/`zO`/`zr`). No model/ValueTree changes — zoom is purely view state.

## Files to Modify

| File | Changes |
|------|---------|
| `src/gui/sequencer/StepGrid.h` | Convert `static constexpr` → instance vars, add zoom API + constants + mouseWheelMove |
| `src/gui/sequencer/StepGrid.cpp` | Implement `setStepSize`, `setRowHeight`, `resetZoom`, `mouseWheelMove`, font scaling |
| `src/gui/sequencer/StepSequencerView.h` | Add `zoomHorizontal`, `zoomVertical`, `resetZoom`, `mouseWheelMove` declarations |
| `src/gui/sequencer/StepSequencerView.cpp` | Implement Ctrl+scroll zoom with anchor-point preservation, keyboard zoom helpers |
| `src/vim/VimEngine.h` | Add `std::function` zoom callbacks, private zoom method declarations |
| `src/vim/VimEngine.cpp` | Add `z` pending-key handling in `handleSequencerNormalKey`, zoom method bodies |
| `src/gui/MainComponent.cpp` | Wire VimEngine zoom callbacks to StepSequencerView |

## Steps

### 1. StepGrid.h — Make dimensions zoomable

- Remove `static constexpr` from `rowLabelWidth`, `stepSize`, `rowHeight`
- Make them `int` instance variables initialized to defaults (120, 32, 32)
- Add public zoom API:
  ```cpp
  void setStepSize (int newSize);
  void setRowHeight (int newHeight);
  void resetZoom();
  int getStepSize() const;
  int getRowHeight() const;
  int stepToX (int step) const;   // rowLabelWidth + step * stepSize
  int rowToY (int row) const;     // row * rowHeight
  void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
  ```
- Add private zoom constants:
  ```cpp
  static constexpr int defaultStepSize = 32, defaultRowHeight = 32;
  static constexpr int minStepSize = 16, maxStepSize = 96;
  static constexpr int minRowHeight = 16, maxRowHeight = 64;
  static constexpr int zoomStep = 8;
  ```

### 2. StepGrid.cpp — Implement zoom methods

- `setStepSize`: clamp to [16, 96], update `setSize()`, call `resized()` + `repaint()`
- `setRowHeight`: clamp to [16, 64], update `setSize()`, call `resized()` + `repaint()`
- `resetZoom`: restore defaults, re-layout
- `mouseWheelMove`: if Ctrl/Cmd held, forward to parent `StepSequencerView` via `findParentComponentOfClass`; otherwise fall through to default (Viewport scroll)
- `paint()`: scale font size proportionally — `13.0f * (rowHeight / 32.0f)`, clamped [9, 20]

### 3. StepSequencerView.h — Add zoom interface

- Add `mouseWheelMove` override (public)
- Add `zoomHorizontal(int delta)`, `zoomVertical(int delta)`, `resetZoom()` (public)

### 4. StepSequencerView.cpp — Zoom with anchor-point preservation

- `mouseWheelMove`: on Ctrl/Cmd modifier, compute mouse position in content-space as a fraction, apply zoom delta, then recompute scroll position so the point under the cursor stays fixed. Non-modified events pass through to `Component::mouseWheelMove`.
- `zoomHorizontal`/`zoomVertical`: apply delta to grid, then ensure Vim cursor cell stays visible in viewport
- `resetZoom`: delegate to `grid.resetZoom()`

### 5. VimEngine.h — Add zoom callbacks

- Add two `std::function` members:
  ```cpp
  std::function<void(int hDelta, int vDelta)> onSequencerZoom;
  std::function<void()> onSequencerZoomReset;
  ```
- Add private: `seqZoomInH()`, `seqZoomOutH()`, `seqZoomInV()`, `seqZoomOutV()`, `seqResetZoom()`

### 6. VimEngine.cpp — `z`-prefix keybindings

Add to `handleSequencerNormalKey`, using the existing pending-key mechanism:

| Sequence | Action |
|----------|--------|
| `zi` | Zoom in horizontally (wider steps) |
| `zo` | Zoom out horizontally (narrower steps) |
| `zI` | Zoom in vertically (taller rows) |
| `zO` | Zoom out vertically (shorter rows) |
| `zr` | Reset both axes to default |

- Insert `pendingKey == 'z'` block after the existing `pendingKey == 'g'` block
- Add `z` as a pending-key trigger before `return false`
- Zoom methods call `onSequencerZoom(±8, 0)` / `onSequencerZoom(0, ±8)` / `onSequencerZoomReset()`

### 7. MainComponent.cpp — Wire callbacks

After creating `sequencerView` (~line 56), set:
```cpp
vimEngine.onSequencerZoom = [this](int h, int v) {
    if (sequencerView) {
        if (h != 0) sequencerView->zoomHorizontal(h);
        if (v != 0) sequencerView->zoomVertical(v);
    }
};
vimEngine.onSequencerZoomReset = [this] {
    if (sequencerView) sequencerView->resetZoom();
};
```

## Zoom Parameters

| Axis | Default | Min | Max | Step |
|------|---------|-----|-----|------|
| Horizontal (stepSize) | 32px | 16px | 96px | 8px |
| Vertical (rowHeight) | 32px | 16px | 64px | 8px |
| Font size | 13pt | 9pt | 20pt | proportional to rowHeight |

## User-Facing Controls

| Input | Condition | Action |
|-------|-----------|--------|
| Ctrl+scroll-up | Mouse over grid | Vertical zoom in (taller rows) |
| Ctrl+scroll-down | Mouse over grid | Vertical zoom out (shorter rows) |
| Ctrl+scroll-left | Mouse over grid | Horizontal zoom out (narrower steps) |
| Ctrl+scroll-right | Mouse over grid | Horizontal zoom in (wider steps) |
| `zi` | Sequencer panel, Normal mode | Horizontal zoom in |
| `zo` | Sequencer panel, Normal mode | Horizontal zoom out |
| `zI` | Sequencer panel, Normal mode | Vertical zoom in |
| `zO` | Sequencer panel, Normal mode | Vertical zoom out |
| `zr` | Sequencer panel, Normal mode | Reset both axes to default |

## Key Design Decisions

- **Mouse wheel event interception**: `StepGrid` overrides `mouseWheelMove` and forwards Ctrl-modified events to `StepSequencerView` via `findParentComponentOfClass`. This is necessary because JUCE delivers mouse events to the deepest child first, and the `Viewport` would consume them before the parent sees them.
- **Viewport anchor-point preservation**: On zoom, the content-space fraction under the mouse is computed before the zoom, then scroll position is adjusted after zoom so the same content point stays under the cursor.
- **Zoom persists across pattern switches**: `rebuild()` uses the current `stepSize`/`rowHeight` instance variables, so switching patterns retains the zoom level.
- **VimEngine decoupling**: Zoom callbacks use `std::function` rather than the `Listener` interface since zoom is a view-only action that doesn't need to broadcast to all listeners.

## Verification

1. `cmake --build build` — must compile cleanly
2. Open app, press Tab to switch to Sequencer panel
3. Ctrl+scroll up/down over grid → rows grow/shrink, row labels scale
4. Ctrl+scroll left/right over grid → steps grow/shrink
5. Type `zi`/`zo` → steps widen/narrow, cursor stays visible
6. Type `zI`/`zO` → rows grow/shrink, cursor stays visible
7. Type `zr` → zoom resets to 32×32
8. Normal scroll (no Ctrl) still works for panning
9. Pattern switch (click pattern selector) preserves zoom level
10. At min/max zoom, further zoom attempts are no-ops (no crash, no layout break)
