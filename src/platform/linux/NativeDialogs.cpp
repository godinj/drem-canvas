#include "platform/NativeDialogs.h"
#include <cstdlib>
#include <cstdio>
#include <array>
#include <sstream>

namespace dc
{
namespace platform
{

namespace
{
    // Execute a shell command and return stdout
    std::string exec (const std::string& cmd)
    {
        std::array<char, 256> buffer;
        std::string result;

        FILE* pipe = popen (cmd.c_str(), "r");
        if (!pipe)
            return {};

        while (fgets (buffer.data(), static_cast<int> (buffer.size()), pipe) != nullptr)
            result += buffer.data();

        int status = pclose (pipe);
        if (status != 0)
            return {};

        // Trim trailing newline
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

        return result;
    }

    // Escape single quotes for shell
    std::string shellEscape (const std::string& s)
    {
        std::string result;
        for (char c : s)
        {
            if (c == '\'')
                result += "'\\''";
            else
                result += c;
        }
        return result;
    }
}

void NativeDialogs::showOpenPanel (const std::string& title,
                                   const std::vector<std::string>& fileTypes,
                                   std::function<void (const std::string&)> callback)
{
    std::ostringstream cmd;
    cmd << "zenity --file-selection --title='" << shellEscape (title) << "'";

    if (!fileTypes.empty())
    {
        cmd << " --file-filter='Supported files |";
        for (const auto& ext : fileTypes)
            cmd << " *." << ext;
        cmd << "'";
    }

    std::string path = exec (cmd.str());
    if (callback)
        callback (path);
}

void NativeDialogs::showSavePanel (const std::string& title,
                                   const std::string& defaultName,
                                   std::function<void (const std::string&)> callback)
{
    std::ostringstream cmd;
    cmd << "zenity --file-selection --save --confirm-overwrite"
        << " --title='" << shellEscape (title) << "'";

    if (!defaultName.empty())
        cmd << " --filename='" << shellEscape (defaultName) << "'";

    std::string path = exec (cmd.str());
    if (callback)
        callback (path);
}

void NativeDialogs::showAlert (const std::string& title,
                               const std::string& message)
{
    std::ostringstream cmd;
    cmd << "zenity --info"
        << " --title='" << shellEscape (title) << "'"
        << " --text='" << shellEscape (message) << "'";

    std::system (cmd.str().c_str());
}

bool NativeDialogs::showConfirmation (const std::string& title,
                                      const std::string& message)
{
    std::ostringstream cmd;
    cmd << "zenity --question"
        << " --title='" << shellEscape (title) << "'"
        << " --text='" << shellEscape (message) << "'";

    int result = std::system (cmd.str().c_str());
    return (result == 0);
}

} // namespace platform
} // namespace dc
