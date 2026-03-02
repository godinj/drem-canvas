#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace dc {

/// Read entire file to string
inline std::string readFileToString(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// Write string to file (atomic: write to .tmp, then rename)
inline bool writeStringToFile(const std::filesystem::path& path,
                              std::string_view content)
{
    auto tmp = path;
    tmp += ".tmp";

    {
        std::ofstream out(tmp, std::ios::binary);
        if (!out) return false;
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out) return false;
    }

    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec)
    {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

/// Get user home directory
inline std::filesystem::path getUserHomeDirectory()
{
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home);
    return {};
}

/// Get user application data directory
inline std::filesystem::path getUserAppDataDirectory()
{
#if defined(__APPLE__)
    return getUserHomeDirectory() / "Library" / "Application Support" / "DremCanvas";
#elif defined(__linux__)
    return getUserHomeDirectory() / ".config" / "DremCanvas";
#else
    return getUserHomeDirectory() / ".DremCanvas";
#endif
}

/// Get temporary directory
inline std::filesystem::path getTempDirectory()
{
    return std::filesystem::temp_directory_path();
}

} // namespace dc
