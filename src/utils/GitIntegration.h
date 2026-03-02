#pragma once
#include "dc/foundation/message_queue.h"
#include "dc/foundation/string_utils.h"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace dc
{

/** Wraps the git CLI for version-controlling session directories.
    All commands run asynchronously on a background thread and post
    results back to the message thread via the provided callback. */
class GitIntegration
{
public:
    using ResultCallback = std::function<void (int exitCode, std::string output)>;

    explicit GitIntegration (const std::filesystem::path& sessionDirectory,
                             dc::MessageQueue& mq);
    ~GitIntegration();

    void setSessionDirectory (const std::filesystem::path& dir);
    std::filesystem::path getSessionDirectory() const;

    /** git init */
    void gitInit (ResultCallback callback);

    /** git status --short */
    void gitStatus (ResultCallback callback);

    /** git diff */
    void gitDiff (ResultCallback callback);

    /** git add -A && git commit -m "msg" */
    void gitCommit (const std::string& message, ResultCallback callback);

    /** git log --oneline -N */
    void gitLog (int n, ResultCallback callback);

    /** git checkout -b name */
    void gitBranch (const std::string& name, ResultCallback callback);

    /** git checkout branch */
    void gitCheckout (const std::string& branch, ResultCallback callback);

private:
    /** Runs a git command asynchronously.  args are passed directly to
        the shell.  The callback is always invoked on the message thread. */
    void runGitCommand (const std::vector<std::string>& args, ResultCallback callback);

    /** Runs a shell command string via /bin/sh -c asynchronously. */
    void runShellCommand (const std::string& command, ResultCallback callback);

    std::filesystem::path sessionDirectory_;
    dc::MessageQueue& messageQueue;

    GitIntegration (const GitIntegration&) = delete;
    GitIntegration& operator= (const GitIntegration&) = delete;
};

} // namespace dc
