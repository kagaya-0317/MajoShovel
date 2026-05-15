#pragma once

#include "engine/Math.hpp"

#include <span>

namespace majo {

struct CollisionRect {
    Vec2 pos{};
    Vec2 size{};
};

[[nodiscard]] CollisionRect collisionRectFromCenter(Vec2 center, Vec2 size);
bool circlesOverlap(Vec2 a, float ar, Vec2 b, float br);
bool circleIntersectsAabb(Vec2 center, float radius, Vec2 rectPos, Vec2 rectSize);
bool circleIntersectsRect(Vec2 center, float radius, CollisionRect rect);
bool circleIntersectsAnyRect(Vec2 center, float radius, std::span<const CollisionRect> rects);

}
