# Drem Canvas — feature/mixer-implementation

## Mission

Implement **Phase 6 mixer enhancements** from `PRD.md`: full vim keyboard control of the mixer, improved metering, and mixer-context vim bindings.

## Build & Run

```bash
cmake --build build
open "build/DremCanvas_artefacts/Release/Drem Canvas.app"
```

If no `build/` dir: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`

## Architecture

- **C++17**, **JUCE 8**, namespace `dc`
- `src/gui/mixer/MixerPanel.h/.cpp` — Container for channel strips
- `src/gui/mixer/ChannelStrip.h/.cpp` — Per-track strip (fader, pan, mute/solo, meter)
- `src/gui/mixer/MeterComponent.h/.cpp` — Stereo peak meter with hold
- `src/engine/MixBusProcessor.h/.cpp` — Master bus summing and metering
- `src/engine/TrackProcessor.h/.cpp` — Per-track processor (gain, pan, mute)
- `src/vim/VimEngine.h/.cpp` — Key dispatch (currently handles arrangement context only)
- `src/vim/VimContext.h/.cpp` — Panel focus (Editor/Mixer), clip selection

## Current Mixer State

The mixer exists with basic channel strips, faders, pan knobs, and meters. What's missing:
- No keyboard navigation — mixer is mouse-only
- No vim context bindings for the mixer panel
- No fader sub-mode for fine adjustment
- No plugin chain navigation
- No visual selection indicator on the focused strip

## What to Implement

### 1. Mixer Vim Context

When `VimContext::activePanel == Mixer`, hjkl should control the mixer instead of the arrangement:
- `h` / `l` — select previous/next channel strip
- `j` / `k` — select previous/next plugin in the selected strip's chain
- Visual highlight on the selected channel strip (similar to arrangement track selection)

Modify `VimEngine::handleNormalKey()` to dispatch differently based on `context.getPanel()`.

### 2. Fader Sub-Mode

When in mixer context:
- `f` enters fader-adjust mode on the selected strip
- `j` / `k` adjusts fader up/down (fine: 0.5dB steps)
- `Shift+j` / `Shift+k` — coarse adjustment (3dB steps)
- `0` resets fader to unity (0dB / volume 1.0)
- `Escape` or `Enter` exits fader mode
- Status bar shows "-- FADER --" indicator during fader mode

### 3. Pan Sub-Mode

- `Shift+p` enters pan-adjust mode (lowercase `p` is paste)
- `h` / `l` adjusts pan left/right
- `0` centers pan
- `Escape` exits

### 4. Plugin Chain Navigation

- `j` / `k` in mixer context navigates the plugin chain on the selected strip
- `Enter` opens the focused plugin's editor window (via `PluginWindowManager`)
- `A` adds a plugin to the selected strip (stub for now — will wire to plugin browser)
- `d` removes the focused plugin from the chain

### 5. Channel Strip Selection Visual

Add selection highlighting to `ChannelStrip`:
- `setSelected(bool)` method
- Selected strip gets a colored border/accent (match the green `0xff50c878` from arrangement)
- `MixerPanel` propagates selection from `VimContext`/`Arrangement`

### 6. MixerPanel Vim Listener

Make `MixerPanel` implement `VimEngine::Listener`:
- On `vimContextChanged()`, update strip selection visuals
- Ensure the selected strip is scrolled into view if the mixer has many tracks

### 7. Master Bus Strip

- Add a master/output channel strip (always visible, rightmost position)
- Shows master fader, master meter, master mute
- Controlled via `VimEngine` when selected

## Key Files to Modify

- `src/vim/VimEngine.h/.cpp` — Add mixer-context dispatch, fader/pan sub-modes
- `src/vim/VimContext.h/.cpp` — Add selected mixer strip index, selected plugin index
- `src/gui/mixer/MixerPanel.h/.cpp` — VimEngine::Listener, selection propagation, scroll-to-selected
- `src/gui/mixer/ChannelStrip.h/.cpp` — Selection visual, plugin chain display
- `src/gui/mixer/MeterComponent.h/.cpp` — Improve meter (peak hold, decay rate)
- `src/gui/vim/VimStatusBar.h/.cpp` — Show fader/pan sub-mode indicators
- `src/gui/MainComponent.cpp` — Register MixerPanel as VimEngine listener

## Verification

- `Tab` switches to mixer context (status bar shows "Mixer")
- `h` / `l` selects channel strips with green highlight
- `f` enters fader mode, `j`/`k` adjusts gain, status bar shows "-- FADER --"
- `0` in fader mode resets to unity
- `Escape` exits fader mode back to normal mixer navigation
- `M` / `S` toggles mute/solo on the selected mixer strip
- Meters animate smoothly with peak hold
- All mixer actions are undoable

## Conventions

- JUCE coding style (spaces around operators, camelCase methods, PascalCase classes)
- All new `.cpp` files go in `CMakeLists.txt` `target_sources`
- Always verify: `cmake --build build`
