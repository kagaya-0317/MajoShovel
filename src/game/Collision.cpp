#include "game/Collision.hpp"

namespace majo {

CollisionRect collisionRectFromCenter(Vec2 center, Vec2 size)
{
    return {
        center - size * 0.5f,
        size,
    };
}

bool circlesOverlap(Vec2 a, float ar, Vec2 b, float br)
{
    const float r = ar + br;
    return distanceSquared(a, b) <= r * r;
}

bool circleIntersectsAabb(Vec2 center, float radius, Vec2 rectPos, Vec2 rectSize)
{
    const float closestX = clamp(center.x, rectPos.x, rectPos.x + rectSize.x);
    const float closestY = clamp(center.y, rectPos.y, rectPos.y + rectSize.y);
    return distanceSquared(center, {closestX, closestY}) <= radius * radius;
}

bool circleIntersectsRect(Vec2 center, float radius, CollisionRect rect)
{
    return circleIntersectsAabb(center, radius, rect.pos, rect.size);
}

bool circleIntersectsAnyRect(Vec2 center, float radius, std::span<const CollisionRect> rects)
{
    for (CollisionRect rect : rects) {
        if (circleIntersectsRect(center, radius, rect)) {
            return true;
        }
    }
    return false;
}

}
