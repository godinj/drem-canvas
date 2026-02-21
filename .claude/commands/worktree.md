Create a new git worktree for parallel feature development. The argument should be a feature name (e.g., "piano-roll-vim").

Steps:
1. Create the worktree: `git worktree add ../drem-canvas-$ARGUMENTS -b feature/$ARGUMENTS`
2. Read the PRD.md to understand the feature scope
3. Ask the user what the worktree's mission should be if it's not obvious from the name
4. Create a CLAUDE.md in the new worktree with: Mission, Build & Run, Architecture overview, What to Implement (detailed breakdown), Key files to modify/create, Verification criteria, and Conventions
5. Commit the CLAUDE.md in the worktree branch
6. Update the main CLAUDE.md's worktree table and merge workflow, and commit that too
7. Report the worktree path so the user can `cd` into it and run `claude`