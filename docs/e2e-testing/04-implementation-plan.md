# Implementation Plan

## Phase ordering

| Phase | Work | Effort | Dependencies |
|-------|------|--------|-------------|
| **A** | Add `--smoke` flag to `Main.cpp` (both platforms) | Small | None |
| **B** | Write `test_smoke.sh`, CMake integration | Small | A |
| **C** | Add `--load`, `--expect-tracks`, `--expect-plugins` flags | Medium | A |
| **D** | Create fixture project YAML files | Small | None |
| **E** | Write `test_load_project.sh`, plugin availability guard | Small | C, D |
| **F** | Add `--scan-plugin`, `--no-spatial-cache`, `--expect-spatial-params-gt` flags | Medium | C |
| **G** | Write `test_scan_cold.sh`, `test_scan_warm.sh` | Small | F, D |
| **H** | Verify under Xvfb, tune timeouts and thresholds | Medium | All |

Phases A-B can ship independently. C-E can ship independently. F-G require C.

## Files touched

### New files

```
tests/e2e/test_smoke.sh
tests/e2e/test_load_project.sh
tests/e2e/test_scan_cold.sh
tests/e2e/test_scan_warm.sh
tests/fixtures/e2e-plugin-project/session.yaml
tests/fixtures/e2e-plugin-project/track-0.yaml
tests/fixtures/e2e-plugin-project/track-1.yaml
tests/fixtures/e2e-scan-project/session.yaml
tests/fixtures/e2e-scan-project/track-0.yaml
```

### Modified files

```
src/Main.cpp              — argv parsing, --smoke/--load/--scan-plugin flags
tests/CMakeLists.txt      — add_test() entries for e2e tests
```

`AppController` may need a small public accessor (e.g. `getProject()`,
`getPluginViewWidget()`) if the smoke-mode code path in `Main.cpp` needs to query
state. Keep the surface area minimal.

## Risks and mitigations

| Risk | Mitigation |
|------|-----------|
| Vulkan fails under Xvfb (no GPU) | Install SwiftShader as software ICD; or gate GPU tests behind `HAS_GPU` label |
| Plugin not installed on CI | Skip-if-missing guard in shell scripts; always-runnable no-plugin fixture |
| Spatial scan hangs (plugin doesn't respond to `findParameterAtPoint`) | Timeout in the scan loop + `timeout` wrapper in shell |
| Wine not available for yabridge plugins | Use native-only plugins (Vital) for the primary fixture; yabridge plugins are bonus |
| XTest mouse probe fails under Xvfb | Set low `--expect-spatial-params-gt` threshold; grid scan alone is sufficient for pass |
| Tests are slow (plugin load + scan can take 10-30s) | Label as `e2e`, exclude from default `ctest`; run only in CI nightly or via `ctest -L e2e` |

## Alternative considered: in-process E2E via Catch2

Instead of shell scripts driving the binary, we could link all of `DremCanvas`'s
sources into a Catch2 test executable and call `AppController::initialise()` directly.

Rejected because:

- It requires linking the full Vulkan/Skia/GLFW stack into the test binary.
- The CMake setup is significantly more complex (all 80+ source files + platform
  objects).
- It doesn't test the real binary — a test that passes in Catch2 but crashes in the
  real binary is a false positive.
- Shell-based tests are simpler, more portable, and test exactly what the user runs.
