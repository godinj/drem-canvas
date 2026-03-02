#pragma once
#include <cstdint>

namespace dc
{

enum class KeyCode : int
{
    // Printable characters use their char32_t value directly (cast).
    // These named constants cover non-printable / special keys.
    Unknown      = 0,
    Escape       = 0x1B,
    Return       = 0x0D,
    Tab          = 0x09,
    Space        = 0x20,
    Backspace    = 0x08,
    Delete       = 0x7F,
    UpArrow      = 0xF700,
    DownArrow    = 0xF701,
    LeftArrow    = 0xF702,
    RightArrow   = 0xF703,
    Home         = 0xF729,
    End          = 0xF72B,
    PageUp       = 0xF72C,
    PageDown     = 0xF72D,
    F1           = 0xF704,
    F2           = 0xF705,
    F3           = 0xF706,
    F4           = 0xF707,
    F5           = 0xF708,
    F6           = 0xF709,
    F7           = 0xF70A,
    F8           = 0xF70B,
    F9           = 0xF70C,
    F10          = 0xF70D,
    F11          = 0xF70E,
    F12          = 0xF70F,
};

/// Lightweight key press: key code + text character + modifiers.
/// Lightweight key press: key code + text character + modifiers.
struct KeyPress
{
    KeyCode code = KeyCode::Unknown;
    char32_t textCharacter = 0;
    bool shift   = false;
    bool control = false;
    bool alt     = false;
    bool command = false;

    /// Convenience: get the text character for comparisons (like key == 'j')
    char32_t getTextCharacter() const { return textCharacter; }

    /// Convenience: check if this is a specific KeyCode
    bool isKeyCode (KeyCode k) const { return code == k; }

    bool operator== (KeyCode k) const { return code == k; }
    bool operator!= (KeyCode k) const { return code != k; }
};

} // namespace dc
