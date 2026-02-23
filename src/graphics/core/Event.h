#pragma once

#include <cstdint>

namespace dc
{
namespace gfx
{

struct MouseEvent
{
    float x = 0.0f;
    float y = 0.0f;
    int clickCount = 0;
    bool rightButton = false;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool command = false;
};

struct KeyEvent
{
    uint16_t keyCode = 0;
    char32_t character = 0;
    char32_t unmodifiedCharacter = 0;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool command = false;
    bool isRepeat = false;
};

struct WheelEvent
{
    float x = 0.0f;
    float y = 0.0f;
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    bool isPixelDelta = false;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool command = false;
};

} // namespace gfx
} // namespace dc
