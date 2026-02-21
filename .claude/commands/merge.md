Merge a feature branch back into the current branch. The argument should be a branch name (e.g., "feature/vim-commands").

Steps:
1. Run `git status` to ensure the working tree is clean. If not, warn the user and stop.
2. Run `git log --oneline master..feature/$ARGUMENTS` (or the full branch name if given) to show what's being merged
3. Run `git merge $ARGUMENTS`
4. If there are conflicts:
   - List all conflicting files
   - For each conflict, read the file and resolve it intelligently based on the intent of both branches
   - Prefer keeping both sides' changes when they don't logically conflict
   - For CLAUDE.md conflicts, keep the main repo version but incorporate any useful info from the feature branch
   - Stage resolved files and complete the merge commit
5. Build the project to verify the merge doesn't break anything
6. Report what was merged and whether the build succeeds
7. Suggest removing the worktree with `git worktree remove` if the feature is complete