#include "KeySequence.h"
#include <algorithm>

namespace dc
{

// ── Special key name set ─────────────────────────────────────────────────────

static const std::vector<std::string>& getSpecialKeyNames()
{
    static const std::vector<std::string> names = {
        "Space", "Return", "Tab", "Escape", "Backspace", "Delete",
        "UpArrow", "DownArrow", "LeftArrow", "RightArrow",
        "Home", "End", "PageUp", "PageDown"
    };
    return names;
}

static bool isSpecialKeyName (const std::string& name)
{
    for (auto& s : getSpecialKeyNames())
        if (s == name)
            return true;
    return false;
}

// ── Modifier prefix set ──────────────────────────────────────────────────────

struct ModifierPrefix
{
    std::string name;
    enum Which { Ctrl, Shift, Alt, Cmd };
    Which which;
};

static const std::vector<ModifierPrefix>& getModifierPrefixes()
{
    static const std::vector<ModifierPrefix> prefixes = {
        { "Ctrl",  ModifierPrefix::Ctrl  },
        { "Shift", ModifierPrefix::Shift },
        { "Alt",   ModifierPrefix::Alt   },
        { "Cmd",   ModifierPrefix::Cmd   }
    };
    return prefixes;
}

// ── Parse ────────────────────────────────────────────────────────────────────

KeySequence KeySequence::parse (const std::string& repr)
{
    KeySequence seq;

    if (repr.empty())
        return seq;

    // First, try to parse as modifier+key sequence (contains '+')
    // We need to detect if '+' is used as a modifier separator or is the key itself
    // Strategy: try to extract modifier prefixes from the front
    bool hasModifiers = false;
    bool shift = false, ctrl = false, alt = false, cmd = false;
    std::string remaining = repr;

    // Repeatedly try to consume "Modifier+" from the front
    bool consumed = true;
    while (consumed)
    {
        consumed = false;
        for (auto& mp : getModifierPrefixes())
        {
            auto prefix = mp.name + "+";
            if (remaining.size() > prefix.size()
                && remaining.substr (0, prefix.size()) == prefix)
            {
                switch (mp.which)
                {
                    case ModifierPrefix::Ctrl:  ctrl  = true; break;
                    case ModifierPrefix::Shift: shift = true; break;
                    case ModifierPrefix::Alt:   alt   = true; break;
                    case ModifierPrefix::Cmd:   cmd   = true; break;
                }
                remaining = remaining.substr (prefix.size());
                hasModifiers = true;
                consumed = true;
                break; // restart loop with updated remaining
            }
        }
    }

    // If we consumed modifiers, the remaining string is a single key
    if (hasModifiers)
    {
        Step step;
        step.shift   = shift;
        step.control = ctrl;
        step.alt     = alt;
        step.command = cmd;

        if (isSpecialKeyName (remaining))
        {
            step.specialKey = remaining;
        }
        else if (remaining.size() == 1)
        {
            step.character = static_cast<char32_t> (remaining[0]);
        }
        else
        {
            // Unknown multi-char key name after modifiers — treat as special
            step.specialKey = remaining;
        }

        seq.steps.push_back (step);
        return seq;
    }

    // No modifiers consumed — check if the whole string is a special key name
    if (isSpecialKeyName (repr))
    {
        Step step;
        step.specialKey = repr;
        seq.steps.push_back (step);
        return seq;
    }

    // Otherwise, treat as a multi-step sequence of individual characters
    // (e.g. "gg", "zi", "j", "$")
    for (char c : repr)
    {
        Step step;
        step.character = static_cast<char32_t> (c);
        seq.steps.push_back (step);
    }

    return seq;
}

// ── toString ─────────────────────────────────────────────────────────────────

std::string KeySequence::toString() const
{
    if (steps.empty())
        return "";

    // Check if this is a single step with modifiers
    if (steps.size() == 1)
    {
        auto& step = steps[0];
        std::string result;

        if (step.control) result += "Ctrl+";
        if (step.shift)   result += "Shift+";
        if (step.alt)     result += "Alt+";
        if (step.command) result += "Cmd+";

        if (! step.specialKey.empty())
            result += step.specialKey;
        else if (step.character != 0)
            result += static_cast<char> (step.character);

        return result;
    }

    // Multi-step: concatenate characters (no modifiers expected)
    std::string result;
    for (auto& step : steps)
    {
        if (! step.specialKey.empty())
            result += step.specialKey;
        else if (step.character != 0)
            result += static_cast<char> (step.character);
    }
    return result;
}

// ── Step::matches ────────────────────────────────────────────────────────────

bool KeySequence::Step::matches (char32_t keyChar, bool s, bool c, bool a, bool cmd) const
{
    // Modifiers must match exactly
    if (shift != s || control != c || alt != a || command != cmd)
        return false;

    if (! specialKey.empty())
    {
        // Compare against known special key character values
        char32_t expected = 0;
        if      (specialKey == "Space")      expected = 0x20;
        else if (specialKey == "Return")     expected = 0x0D;
        else if (specialKey == "Tab")        expected = 0x09;
        else if (specialKey == "Escape")     expected = 0x1B;
        else if (specialKey == "Backspace")  expected = 0x08;
        else if (specialKey == "Delete")     expected = 0x7F;
        else if (specialKey == "UpArrow")    expected = 0xF700;
        else if (specialKey == "DownArrow")  expected = 0xF701;
        else if (specialKey == "LeftArrow")  expected = 0xF702;
        else if (specialKey == "RightArrow") expected = 0xF703;
        else if (specialKey == "Home")       expected = 0xF729;
        else if (specialKey == "End")        expected = 0xF72B;
        else if (specialKey == "PageUp")     expected = 0xF72C;
        else if (specialKey == "PageDown")   expected = 0xF72D;
        else return false;

        return keyChar == expected;
    }

    return character == keyChar;
}

// ── Comparison operators ─────────────────────────────────────────────────────

bool KeySequence::operator== (const KeySequence& other) const
{
    if (steps.size() != other.steps.size())
        return false;

    for (size_t i = 0; i < steps.size(); ++i)
    {
        auto& a = steps[i];
        auto& b = other.steps[i];

        if (a.character != b.character
            || a.specialKey != b.specialKey
            || a.shift != b.shift
            || a.control != b.control
            || a.alt != b.alt
            || a.command != b.command)
            return false;
    }

    return true;
}

bool KeySequence::operator< (const KeySequence& other) const
{
    auto minLen = std::min (steps.size(), other.steps.size());
    for (size_t i = 0; i < minLen; ++i)
    {
        auto& a = steps[i];
        auto& b = other.steps[i];

        // Compare by specialKey first, then character, then modifiers
        if (a.specialKey != b.specialKey) return a.specialKey < b.specialKey;
        if (a.character != b.character)   return a.character < b.character;
        if (a.control != b.control)       return a.control < b.control;
        if (a.shift != b.shift)           return a.shift < b.shift;
        if (a.alt != b.alt)               return a.alt < b.alt;
        if (a.command != b.command)       return a.command < b.command;
    }

    return steps.size() < other.steps.size();
}

} // namespace dc
