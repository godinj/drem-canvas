Show the status of all git worktrees and feature branches.

1. Run `git worktree list` to show all worktrees
2. For each worktree/branch, run `git log --oneline master..<branch> | wc -l` to count commits ahead of master
3. Run `git diff --stat master..<branch>` for a file-change summary per branch
4. Present a clear table with: worktree path, branch name, commits ahead, files changed, and a brief description from the branch's CLAUDE.md mission line