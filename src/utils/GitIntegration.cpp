#include "GitIntegration.h"
#include <cstdio>
#include <thread>

namespace dc
{

GitIntegration::GitIntegration (const std::filesystem::path& dir, dc::MessageQueue& mq)
    : sessionDirectory_ (dir), messageQueue (mq)
{
}

GitIntegration::~GitIntegration() = default;

void GitIntegration::setSessionDirectory (const std::filesystem::path& dir)
{
    sessionDirectory_ = dir;
}

std::filesystem::path GitIntegration::getSessionDirectory() const
{
    return sessionDirectory_;
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

void GitIntegration::gitCommit (const std::string& message, ResultCallback callback)
{
    auto dirQuoted = dc::shellQuote (sessionDirectory_.string());
    auto msgQuoted = dc::shellQuote (message);

    auto cmd = "cd " + dirQuoted
             + " && git add -A && git commit -m " + msgQuoted;

    runShellCommand (cmd, std::move (callback));
}

void GitIntegration::gitLog (int n, ResultCallback callback)
{
    runGitCommand ({ "git", "log", "--oneline", "-" + std::to_string (n) }, std::move (callback));
}

void GitIntegration::gitBranch (const std::string& name, ResultCallback callback)
{
    runGitCommand ({ "git", "checkout", "-b", name }, std::move (callback));
}

void GitIntegration::gitCheckout (const std::string& branch, ResultCallback callback)
{
    runGitCommand ({ "git", "checkout", branch }, std::move (callback));
}

//==============================================================================
void GitIntegration::runGitCommand (const std::vector<std::string>& args, ResultCallback callback)
{
    auto dirQuoted = dc::shellQuote (sessionDirectory_.string());

    std::string cmd = "cd " + dirQuoted + " &&";
    for (const auto& arg : args)
        cmd += " " + dc::shellQuote (arg);

    runShellCommand (cmd, std::move (callback));
}

void GitIntegration::runShellCommand (const std::string& command, ResultCallback callback)
{
    std::thread ([this, command, cb = std::move (callback)]() mutable
    {
        std::string shellCmd = "/bin/sh -c " + dc::shellQuote (command);
        FILE* pipe = popen (shellCmd.c_str(), "r");

        if (pipe == nullptr)
        {
            auto errorCb = std::move (cb);
            messageQueue.post ([ecb = std::move (errorCb)]()
            {
                if (ecb)
                    ecb (-1, "Failed to start shell process");
            });
            return;
        }

        std::string output;
        char buf[256];
        while (fgets (buf, sizeof (buf), pipe) != nullptr)
            output += buf;

        int status = pclose (pipe);
        int exitCode = WIFEXITED (status) ? WEXITSTATUS (status) : -1;

        auto resultCb = std::move (cb);
        messageQueue.post ([rcb = std::move (resultCb), exitCode, out = std::move (output)]()
        {
            if (rcb)
                rcb (exitCode, out);
        });
    }).detach();
}

} // namespace dc
