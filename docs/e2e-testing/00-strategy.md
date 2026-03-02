# E2E Testing Strategy

## Overview

The app currently has unit tests (Catch2, JUCE-free `dc::` layers) and integration
tests (model/serialization round-trips), but no end-to-end tests that exercise the
real application binary. E2E testing requires launching the actual `DremCanvas`
executable, which means dealing with a GPU window (Vulkan/GLFW on Linux, Metal on
macOS), an audio engine, and real plugin hosting.

Three tiers with escalating complexity:

| Tier | Scope | Key challenge |
|------|-------|---------------|
| 1 | App launch and clean exit | No headless mode exists yet |
| 2 | Load a project with plugins | Real VST3 binaries must be present |
| 3 | Visual scan with/without spatial cache | Requires plugin editor, XTest, IParameterFinder |
