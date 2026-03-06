# Agent: AppController Browser Accessor

You are working on the `feature/fix-vis-scan` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to add a `getBrowserWidget()` public accessor to `AppController` so the e2e smoke harness in `Main.cpp` can access the `BrowserWidget` for async scan testing.

## Context

Read these files before starting:
- `src/ui/AppController.h` (line 85-89 — existing e2e-support accessors: `getPluginManager()`, `toggleBrowser()`, `getPluginViewWidget()`)
- `src/ui/browser/BrowserWidget.h` (the type being exposed)

## Deliverables

### Modified files

#### 1. `src/ui/AppController.h`

Add one public inline getter after `toggleBrowser()` (line 89):

```cpp
    // Browser widget access (used by E2E --browser-async-scan flag)
    BrowserWidget* getBrowserWidget() { return browserWidget.get(); }
```

This follows the exact pattern of `getPluginViewWidget()` on line 80 which returns `pluginViewWidget.get()`.

No changes are needed to `AppController.cpp` — this is an inline getter on the existing `std::unique_ptr<BrowserWidget> browserWidget` private member (line 180).

## Scope Limitation

- Only modify `src/ui/AppController.h`
- Do not modify `AppController.cpp`, `BrowserWidget`, `Main.cpp`, or any other files
- Do not change any existing methods or members
- Do not add new files

## Conventions

- Namespace: `dc::ui`
- Spaces around operators, braces on new line for classes/functions
- `camelCase` methods, `PascalCase` classes
- Build verification: `cmake --build --preset release`
