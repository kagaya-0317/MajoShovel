#include "game/Collision.hpp"

namespace majo {

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

}
