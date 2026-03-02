#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>

namespace dc {

//==============================================================================
// Math utilities
//==============================================================================

inline int roundToInt(float x)  { return static_cast<int>(std::round(x)); }
inline int roundToInt(double x) { return static_cast<int>(std::round(x)); }

template<typename T>
constexpr T pi = T(3.14159265358979323846);

template<typename T>
bool approxEqual(T a, T b, T epsilon = T(1e-6))
{
    return std::abs(a - b) <= epsilon;
}

//==============================================================================
// Random
//==============================================================================

inline float randomFloat()
{
    thread_local std::mt19937 gen{std::random_device{}()};
    thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(gen);
}

inline int randomInt(int min, int max)
{
    thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}

//==============================================================================
// Colour
//==============================================================================

struct Colour
{
    uint32_t argb;  // 0xAARRGGBB

    constexpr Colour() : argb(0xff000000) {}
    constexpr explicit Colour(uint32_t value) : argb(value) {}

    static Colour fromFloat(float r, float g, float b, float a = 1.0f)
    {
        return Colour(
            (uint32_t(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f) << 24) |
            (uint32_t(std::clamp(r, 0.0f, 1.0f) * 255.0f + 0.5f) << 16) |
            (uint32_t(std::clamp(g, 0.0f, 1.0f) * 255.0f + 0.5f) << 8)  |
             uint32_t(std::clamp(b, 0.0f, 1.0f) * 255.0f + 0.5f));
    }

    static Colour fromRGB(uint8_t r, uint8_t g, uint8_t b)
    {
        return Colour(0xff000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b));
    }

    static Colour fromHSV(float h, float s, float v, float a = 1.0f)
    {
        float r, g, b;
        if (s <= 0.0f)
        {
            r = g = b = v;
        }
        else
        {
            float hue = h * 6.0f;
            if (hue >= 6.0f) hue = 0.0f;
            int i = static_cast<int>(hue);
            float f = hue - i;
            float p = v * (1.0f - s);
            float q = v * (1.0f - s * f);
            float t = v * (1.0f - s * (1.0f - f));
            switch (i)
            {
                case 0: r = v; g = t; b = p; break;
                case 1: r = q; g = v; b = p; break;
                case 2: r = p; g = v; b = t; break;
                case 3: r = p; g = q; b = v; break;
                case 4: r = t; g = p; b = v; break;
                default: r = v; g = p; b = q; break;
            }
        }
        return fromFloat(r, g, b, a);
    }

    uint8_t getAlpha() const { return static_cast<uint8_t>((argb >> 24) & 0xff); }
    uint8_t getRed()   const { return static_cast<uint8_t>((argb >> 16) & 0xff); }
    uint8_t getGreen() const { return static_cast<uint8_t>((argb >> 8)  & 0xff); }
    uint8_t getBlue()  const { return static_cast<uint8_t>(argb & 0xff); }

    float getFloatAlpha() const { return getAlpha() / 255.0f; }
    float getFloatRed()   const { return getRed()   / 255.0f; }
    float getFloatGreen() const { return getGreen() / 255.0f; }
    float getFloatBlue()  const { return getBlue()  / 255.0f; }

    Colour withAlpha(float a) const
    {
        uint8_t alpha = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
        return Colour((uint32_t(alpha) << 24) | (argb & 0x00ffffff));
    }

    Colour brighter(float amount = 0.4f) const
    {
        auto blend = [](uint8_t c, float amt) -> uint8_t {
            return static_cast<uint8_t>(c + (255 - c) * amt);
        };
        return Colour((argb & 0xff000000) |
                       (uint32_t(blend(getRed(),   amount)) << 16) |
                       (uint32_t(blend(getGreen(), amount)) << 8)  |
                        uint32_t(blend(getBlue(),  amount)));
    }

    Colour darker(float amount = 0.4f) const
    {
        auto dim = [](uint8_t c, float amt) -> uint8_t {
            return static_cast<uint8_t>(c * (1.0f - amt));
        };
        return Colour((argb & 0xff000000) |
                       (uint32_t(dim(getRed(),   amount)) << 16) |
                       (uint32_t(dim(getGreen(), amount)) << 8)  |
                        uint32_t(dim(getBlue(),  amount)));
    }

    Colour interpolatedWith(Colour other, float proportion) const
    {
        auto lerp = [](uint8_t a, uint8_t b, float t) -> uint8_t {
            return static_cast<uint8_t>(a + (b - a) * t);
        };
        return Colour((uint32_t(lerp(getAlpha(), other.getAlpha(), proportion)) << 24) |
                       (uint32_t(lerp(getRed(),   other.getRed(),   proportion)) << 16) |
                       (uint32_t(lerp(getGreen(), other.getGreen(), proportion)) << 8)  |
                        uint32_t(lerp(getBlue(),  other.getBlue(),  proportion)));
    }

    /// Convert to SkColor (same ARGB layout)
    uint32_t toSkColor() const { return argb; }

    /// Convert to hex string (e.g., "ff252535")
    std::string toHexString() const
    {
        char buf[9];
        std::snprintf(buf, sizeof(buf), "%08x", argb);
        return std::string(buf);
    }

    /// Convert from hex string (e.g., "ff252535")
    static Colour fromHexString(std::string_view hex)
    {
        uint32_t value = 0;
        for (char c : hex)
        {
            value <<= 4;
            if (c >= '0' && c <= '9')      value |= uint32_t(c - '0');
            else if (c >= 'a' && c <= 'f') value |= uint32_t(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= uint32_t(c - 'A' + 10);
        }
        return Colour(value);
    }

    bool operator==(const Colour& other) const { return argb == other.argb; }
    bool operator!=(const Colour& other) const { return argb != other.argb; }
};

namespace Colours
{
    constexpr Colour black{0xff000000};
    constexpr Colour white{0xffffffff};
    constexpr Colour red{0xffff0000};
    constexpr Colour green{0xff00ff00};
    constexpr Colour blue{0xff0000ff};
    constexpr Colour yellow{0xffffff00};
    constexpr Colour cyan{0xff00ffff};
    constexpr Colour magenta{0xffff00ff};
    constexpr Colour grey{0xff808080};
    constexpr Colour lightgrey{0xffd3d3d3};
    constexpr Colour darkgrey{0xffa9a9a9};
    constexpr Colour mediumpurple{0xff9370db};
    constexpr Colour transparentBlack{0x00000000};
    constexpr Colour transparentWhite{0x00ffffff};
}

} // namespace dc
