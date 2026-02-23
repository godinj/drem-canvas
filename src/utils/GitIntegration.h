#pragma once
#include <JuceHeader.h>
#include <functional>

namespace dc
{

/** Wraps the git CLI for version-controlling session directories.
    All commands run asynchronously on a background thread and post
    results back to the message thread via the provided callback. */
class GitIntegration
{
public:
    using ResultCallback = std::function<void (int exitCode, juce::String output)>;

    explicit GitIntegration (const juce::File& sessionDirectory);
    ~GitIntegration();

    void setSessionDirectory (const juce::File& dir);
    juce::File getSessionDirectory() const;

    /** git init */
    void gitInit (ResultCallback callback);

    /** git status --short */
    void gitStatus (ResultCallback callback);

    /** git diff */
    void gitDiff (ResultCallback callback);

    /** git add -A && git commit -m "msg" */
    void gitCommit (const juce::String& message, ResultCallback callback);

    /** git log --oneline -N */
    void gitLog (int n, ResultCallback callback);

    /** git checkout -b name */
    void gitBranch (const juce::String& name, ResultCallback callback);

    /** git checkout branch */
    void gitCheckout (const juce::String& branch, ResultCallback callback);

private:
    /** Runs a git command asynchronously.  args are passed directly to
        juce::ChildProcess::start().  The callback is always invoked on
        the message thread. */
    void runGitCommand (const juce::StringArray& args, ResultCallback callback);

    /** Runs a shell command string via /bin/sh -c asynchronously. */
    void runShellCommand (const juce::String& command, ResultCallback callback);

    juce::File sessionDirectory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GitIntegration)
};

} // namespace dc
