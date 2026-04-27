#pragma once

#include <algorithm>
#include <cmath>

namespace majo {

constexpr float Pi = 3.14159265358979323846f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline Vec2 operator/(Vec2 a, float s) { return {a.x / s, a.y / s}; }
inline Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }
inline Vec2& operator-=(Vec2& a, Vec2 b) { a.x -= b.x; a.y -= b.y; return a; }

inline float lengthSquared(Vec2 v) { return v.x * v.x + v.y * v.y; }
inline float length(Vec2 v) { return std::sqrt(lengthSquared(v)); }
inline float distanceSquared(Vec2 a, Vec2 b) { return lengthSquared(a - b); }
inline float clamp(float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); }

inline Vec2 normalize(Vec2 v)
{
    const float len = length(v);
    if (len <= 0.0001f) {
        return {1.0f, 0.0f};
    }
    return v / len;
}

inline Vec2 fromAngle(float radians)
{
    return {std::cos(radians), std::sin(radians)};
}

inline float lerp(float a, float b, float t)
{
    return a + (b - a) * clamp(t, 0.0f, 1.0f);
}

inline Vec2 lerp(Vec2 a, Vec2 b, float t)
{
    return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)};
}

}
