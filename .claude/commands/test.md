Build and run unit and integration tests.

Steps:
1. If `build-debug/CMakeCache.txt` doesn't exist, run `cmake --preset test`.
2. Run `cmake --build --preset test`. If the build fails, report errors and stop.
3. Run `ctest --test-dir build-debug --output-on-failure -j$(nproc) -L "unit|integration"` (this excludes e2e tests).
4. Report results: number of tests passed/failed, and for any failures show the test name and output.
