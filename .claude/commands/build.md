Build the project with prerequisite checks.

Steps:
1. Check `libs/skia/lib/libskia.a` exists. If not, check if a shared Skia cache exists at the bare repo root (detect via `git rev-parse --git-common-dir`, then check `../.cache/skia/lib/libskia.a`). If the cache exists, symlink it: `ln -sfn <bare_root>/.cache/skia libs/skia`. If no cache is available, tell the user to run `scripts/bootstrap.sh` and stop.
2. If `build/CMakeCache.txt` doesn't exist, run `cmake --preset release`.
3. Run `cmake --build --preset release`.
4. If there are errors, summarize them concisely and suggest fixes. If the build succeeds, report success with the binary path.