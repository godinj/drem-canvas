#pragma once

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace dc {

/// Trim whitespace from both ends
inline std::string trim(std::string_view s)
{
    auto start = s.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string_view::npos) return {};
    auto end = s.find_last_not_of(" \t\n\r\f\v");
    return std::string(s.substr(start, end - start + 1));
}

/// Replace all occurrences of `from` with `to`
inline std::string replace(std::string str, std::string_view from, std::string_view to)
{
    if (from.empty()) return str;
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos)
    {
        str.replace(pos, from.size(), to);
        pos += to.size();
    }
    return str;
}

/// Check if string contains a substring
inline bool contains(std::string_view haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string_view::npos;
}

/// Check if string starts with a prefix
inline bool startsWith(std::string_view s, std::string_view prefix)
{
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

/// Return substring after first occurrence of delimiter, or empty if not found
inline std::string afterFirst(std::string_view s, std::string_view delimiter)
{
    auto pos = s.find(delimiter);
    if (pos == std::string_view::npos) return {};
    return std::string(s.substr(pos + delimiter.size()));
}

/// Shell-safe quoting (wraps in single quotes, escapes embedded single quotes)
inline std::string shellQuote(std::string_view s)
{
    std::string result = "'";
    for (char c : s)
    {
        if (c == '\'')
            result += "'\\''";
        else
            result += c;
    }
    result += '\'';
    return result;
}

/// printf-style string formatting
template<typename... Args>
std::string format(const char* fmt, Args... args)
{
    int size = std::snprintf(nullptr, 0, fmt, args...);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    std::snprintf(result.data(), result.size() + 1, fmt, args...);
    return result;
}

} // namespace dc
