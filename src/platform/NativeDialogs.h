#pragma once

#include <string>
#include <vector>
#include <functional>

namespace dc
{
namespace platform
{

class NativeDialogs
{
public:
    // File open panel
    static void showOpenPanel (const std::string& title,
                               const std::vector<std::string>& fileTypes,
                               std::function<void (const std::string&)> callback);

    // File save panel
    static void showSavePanel (const std::string& title,
                               const std::string& defaultName,
                               std::function<void (const std::string&)> callback);

    // Alert dialog
    static void showAlert (const std::string& title,
                           const std::string& message);

    // Confirmation dialog
    static bool showConfirmation (const std::string& title,
                                  const std::string& message);
};

} // namespace platform
} // namespace dc
