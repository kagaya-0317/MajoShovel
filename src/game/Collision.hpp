#pragma once

#include "engine/Math.hpp"

namespace majo {

bool circlesOverlap(Vec2 a, float ar, Vec2 b, float br);
bool circleIntersectsAabb(Vec2 center, float radius, Vec2 rectPos, Vec2 rectSize);

}
