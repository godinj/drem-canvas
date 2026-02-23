#include "GitIntegration.h"

namespace dc
{

GitIntegration::GitIntegration (const juce::File& dir)
    : sessionDirectory (dir)
{
}

GitIntegration::~GitIntegration() = default;

void GitIntegration::setSessionDirectory (const juce::File& dir)
{
    sessionDirectory = dir;
}

juce::File GitIntegration::getSessionDirectory() const
{
    return sessionDirectory;
}

void GitIntegration::gitInit (ResultCallback callback)
{
    runGitCommand ({ "git", "init" }, std::move (callback));
}

void GitIntegration::gitStatus (ResultCallback callback)
{
    runGitCommand ({ "git", "status", "--short" }, std::move (callback));
}

void GitIntegration::gitDiff (ResultCallback callback)
{
    runGitCommand ({ "git", "diff" }, std::move (callback));
}

void GitIntegration::gitCommit (const juce::String& message, ResultCallback callback)
{
    // Escape characters that are special inside double-quoted shell strings
    auto escaped = message.replace ("\\", "\\\\")
                          .replace ("\"", "\\\"")
                          .replace ("$", "\\$")
                          .replace ("`", "\\`");

    auto cmd = "cd " + sessionDirectory.getFullPathName().quoted()
             + " && git add -A && git commit -m \"" + escaped + "\"";

    runShellCommand (cmd, std::move (callback));
}

void GitIntegration::gitLog (int n, ResultCallback callback)
{
    runGitCommand ({ "git", "log", "--oneline", "-" + juce::String (n) }, std::move (callback));
}

void GitIntegration::gitBranch (const juce::String& name, ResultCallback callback)
{
    runGitCommand ({ "git", "checkout", "-b", name }, std::move (callback));
}

void GitIntegration::gitCheckout (const juce::String& branch, ResultCallback callback)
{
    runGitCommand ({ "git", "checkout", branch }, std::move (callback));
}

//==============================================================================
void GitIntegration::runGitCommand (const juce::StringArray& args, ResultCallback callback)
{
    // juce::ChildProcess doesn't support setting a working directory,
    // so we go through the shell with a cd prefix for all commands.
    auto cmd = "cd " + sessionDirectory.getFullPathName().quoted()
             + " && " + args.joinIntoString (" ");

    runShellCommand (cmd, std::move (callback));
}

void GitIntegration::runShellCommand (const juce::String& command, ResultCallback callback)
{
    std::thread ([command, cb = std::move (callback)]() mutable
    {
        juce::ChildProcess process;

        if (! process.start ("/bin/sh -c " + command.quoted()))
        {
            auto errorCb = std::move (cb);
            juce::MessageManager::callAsync ([ecb = std::move (errorCb)]()
            {
                if (ecb)
                    ecb (-1, "Failed to start shell process");
            });
            return;
        }

        auto output = process.readAllProcessOutput();
        process.waitForProcessToFinish (-1);
        auto exitCode = static_cast<int> (process.getExitCode());
        auto resultCb = std::move (cb);

        juce::MessageManager::callAsync ([rcb = std::move (resultCb), exitCode, out = std::move (output)]()
        {
            if (rcb)
                rcb (exitCode, out);
        });
    }).detach();
}

} // namespace dc
