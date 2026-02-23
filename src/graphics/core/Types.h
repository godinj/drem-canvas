#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace dc
{
namespace gfx
{

struct Point
{
    float x = 0.0f;
    float y = 0.0f;

    Point() = default;
    Point (float x_, float y_) : x (x_), y (y_) {}

    Point operator+ (const Point& other) const { return { x + other.x, y + other.y }; }
    Point operator- (const Point& other) const { return { x - other.x, y - other.y }; }
    Point operator* (float s) const { return { x * s, y * s }; }

    bool operator== (const Point& other) const { return x == other.x && y == other.y; }
    bool operator!= (const Point& other) const { return !(*this == other); }
};

struct Size
{
    float width = 0.0f;
    float height = 0.0f;

    Size() = default;
    Size (float w, float h) : width (w), height (h) {}

    bool isEmpty() const { return width <= 0.0f || height <= 0.0f; }

    bool operator== (const Size& other) const { return width == other.width && height == other.height; }
    bool operator!= (const Size& other) const { return !(*this == other); }
};

struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    Rect() = default;
    Rect (float x_, float y_, float w, float h) : x (x_), y (y_), width (w), height (h) {}
    Rect (Point origin, Size size) : x (origin.x), y (origin.y), width (size.width), height (size.height) {}

    float right() const { return x + width; }
    float bottom() const { return y + height; }
    Point origin() const { return { x, y }; }
    Size size() const { return { width, height }; }
    Point centre() const { return { x + width * 0.5f, y + height * 0.5f }; }

    bool isEmpty() const { return width <= 0.0f || height <= 0.0f; }

    bool contains (Point p) const
    {
        return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
    }

    bool intersects (const Rect& other) const
    {
        return x < other.right() && right() > other.x
            && y < other.bottom() && bottom() > other.y;
    }

    Rect intersection (const Rect& other) const
    {
        float nx = std::max (x, other.x);
        float ny = std::max (y, other.y);
        float nr = std::min (right(), other.right());
        float nb = std::min (bottom(), other.bottom());
        if (nr > nx && nb > ny)
            return { nx, ny, nr - nx, nb - ny };
        return {};
    }

    Rect united (const Rect& other) const
    {
        if (isEmpty()) return other;
        if (other.isEmpty()) return *this;
        float nx = std::min (x, other.x);
        float ny = std::min (y, other.y);
        float nr = std::max (right(), other.right());
        float nb = std::max (bottom(), other.bottom());
        return { nx, ny, nr - nx, nb - ny };
    }

    Rect reduced (float amount) const
    {
        return { x + amount, y + amount, width - amount * 2.0f, height - amount * 2.0f };
    }

    Rect translated (float dx, float dy) const
    {
        return { x + dx, y + dy, width, height };
    }

    bool operator== (const Rect& other) const
    {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
    bool operator!= (const Rect& other) const { return !(*this == other); }
};

struct Color
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    Color() = default;
    Color (uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r (r_), g (g_), b (b_), a (a_) {}

    static Color fromARGB (uint32_t argb)
    {
        return { static_cast<uint8_t> ((argb >> 16) & 0xFF),
                 static_cast<uint8_t> ((argb >> 8) & 0xFF),
                 static_cast<uint8_t> (argb & 0xFF),
                 static_cast<uint8_t> ((argb >> 24) & 0xFF) };
    }

    uint32_t toARGB() const
    {
        return (static_cast<uint32_t> (a) << 24)
             | (static_cast<uint32_t> (r) << 16)
             | (static_cast<uint32_t> (g) << 8)
             | static_cast<uint32_t> (b);
    }

    Color withAlpha (uint8_t newAlpha) const { return { r, g, b, newAlpha }; }
    Color withAlpha (float alpha) const { return { r, g, b, static_cast<uint8_t> (alpha * 255.0f) }; }

    bool operator== (const Color& other) const
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    bool operator!= (const Color& other) const { return !(*this == other); }
};

struct Transform2D
{
    // 2D affine transform as 3x2 matrix
    // [ a  b  tx ]
    // [ c  d  ty ]
    float a = 1.0f, b = 0.0f;
    float c = 0.0f, d = 1.0f;
    float tx = 0.0f, ty = 0.0f;

    Transform2D() = default;

    static Transform2D identity() { return {}; }

    static Transform2D translation (float x, float y)
    {
        Transform2D t;
        t.tx = x;
        t.ty = y;
        return t;
    }

    static Transform2D scale (float sx, float sy)
    {
        Transform2D t;
        t.a = sx;
        t.d = sy;
        return t;
    }

    bool isIdentity() const
    {
        return a == 1.0f && b == 0.0f && c == 0.0f && d == 1.0f && tx == 0.0f && ty == 0.0f;
    }

    Point apply (Point p) const
    {
        return { a * p.x + b * p.y + tx,
                 c * p.x + d * p.y + ty };
    }

    Transform2D then (const Transform2D& other) const
    {
        Transform2D result;
        result.a  = a * other.a  + b * other.c;
        result.b  = a * other.b  + b * other.d;
        result.c  = c * other.a  + d * other.c;
        result.d  = c * other.b  + d * other.d;
        result.tx = tx * other.a + ty * other.c + other.tx;
        result.ty = tx * other.b + ty * other.d + other.ty;
        return result;
    }
};

} // namespace gfx
} // namespace dc
