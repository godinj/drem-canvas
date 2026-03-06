Build and launch the app.

Steps:
0. If the current directory is a bare git repo root (i.e., `CMakeLists.txt` does not exist but `main/CMakeLists.txt` does), `cd main` first. All subsequent paths are relative to the worktree root.
1. Check `libs/skia/lib/libskia.a` exists. If not, check if a shared Skia cache exists at the bare repo root (detect via `git rev-parse --git-common-dir`, then check `../.cache/skia/lib/libskia.a`). If the cache exists, symlink it: `ln -sfn <bare_root>/.cache/skia libs/skia`. If no cache is available, tell the user to run `scripts/bootstrap.sh` and stop.
2. If `build/CMakeCache.txt` doesn't exist, run `cmake --preset release`.
3. Run `cmake --build --preset release`. If the build fails, report errors and stop.
4. Stop any previous instance and launch: `scripts/app-ctl.sh start`
