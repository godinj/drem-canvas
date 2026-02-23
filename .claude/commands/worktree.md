Create a new git worktree for parallel feature development. The argument should be a feature name (e.g., "piano-roll-vim").

Steps:
1. Create the worktree: `git worktree add ../drem-canvas-$ARGUMENTS -b feature/$ARGUMENTS`
2. Bootstrap the new worktree:
   a. Init JUCE submodule: `git -C ../drem-canvas-$ARGUMENTS submodule update --init --depth 1 libs/JUCE`
   b. Check for shared Skia cache: detect bare repo root via `git rev-parse --git-common-dir`, check if `<bare_root>/.cache/skia/lib/libskia.a` exists. If it does, create the symlink: `ln -sfn <bare_root>/.cache/skia ../drem-canvas-$ARGUMENTS/libs/skia`. If not, warn that Skia is not available and suggest running `scripts/bootstrap.sh` in the new worktree.
3. Read the PRD.md to understand the feature scope
4. Ask the user what the worktree's mission should be if it's not obvious from the name
5. Create a CLAUDE.md in the new worktree with: Mission, Build & Run, Architecture overview, What to Implement (detailed breakdown), Key files to modify/create, Verification criteria, and Conventions
6. Commit the CLAUDE.md in the worktree branch
7. Update the main CLAUDE.md's worktree table and merge workflow, and commit that too
8. Report the worktree path so the user can `cd` into it and run `claude`