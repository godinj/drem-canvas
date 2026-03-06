#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace dc
{

struct KeySequence
{
    struct Step
    {
        char32_t character = 0;    // 'j', 'g', etc. or 0 for special keys
        std::string specialKey;    // "Space", "Return", "Tab", "Escape", "Backspace", "Delete"
        bool shift   = false;
        bool control = false;
        bool alt     = false;
        bool command = false;

        bool matches (char32_t keyChar, bool s, bool c, bool a, bool cmd) const;
    };

    std::vector<Step> steps;

    // Parse from string representation: "Ctrl+j", "gg", "Space", "zi"
    static KeySequence parse (const std::string& repr);

    // Convert back to display string
    std::string toString() const;

    // Single-step convenience
    bool isSingleKey() const { return steps.size() == 1; }

    bool operator== (const KeySequence& other) const;
    bool operator< (const KeySequence& other) const;
};

} // namespace dc
