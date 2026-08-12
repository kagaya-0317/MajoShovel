#pragma once

#include "engine/Math.hpp"

#include <cmath>

namespace majo {

enum class EnemyFacingDirection {
    Down,
    Left,
    Right,
    Up,
};

[[nodiscard]] inline EnemyFacingDirection enemyFacingDirection(Vec2 direction)
{
    if (std::abs(direction.x) > std::abs(direction.y)) {
        return direction.x >= 0.0f ? EnemyFacingDirection::Right : EnemyFacingDirection::Left;
    }
    return direction.y >= 0.0f ? EnemyFacingDirection::Down : EnemyFacingDirection::Up;
}

[[nodiscard]] inline EnemyFacingDirection enemyFacingDirection(float facingAngle)
{
    return enemyFacingDirection({std::cos(facingAngle), std::sin(facingAngle)});
}

[[nodiscard]] inline Vec2 enemyFacingDirectionVector(EnemyFacingDirection direction)
{
    switch (direction) {
    case EnemyFacingDirection::Down:
        return {0.0f, 1.0f};
    case EnemyFacingDirection::Left:
        return {-1.0f, 0.0f};
    case EnemyFacingDirection::Right:
        return {1.0f, 0.0f};
    case EnemyFacingDirection::Up:
        return {0.0f, -1.0f};
    }
    return {0.0f, 1.0f};
}

[[nodiscard]] inline Vec2 enemyFacingDirectionVector(float facingAngle)
{
    return enemyFacingDirectionVector(enemyFacingDirection(facingAngle));
}

}
