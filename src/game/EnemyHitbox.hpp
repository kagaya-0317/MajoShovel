#pragma once

#include "engine/Math.hpp"
#include "game/Enemy.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

inline constexpr int EnemyHitboxMaxCircles = 8;

struct EnemyHitCircle {
    Vec2 offset{};
    float radius = 10.0f;
};

struct EnemyHitboxProfile {
    std::vector<EnemyHitCircle> circles;
};

struct EnemyHitboxCatalog {
    std::unordered_map<std::string, EnemyHitboxProfile> profiles;
};

[[nodiscard]] const EnemyHitboxProfile* enemyHitboxProfileFor(
    const EnemyHitboxCatalog* catalog,
    const Enemy& enemy);
[[nodiscard]] EnemyHitCircle fallbackEnemyHitCircle(const Enemy& enemy);
[[nodiscard]] float enemyHitboxBoundsRadius(const Enemy& enemy, const EnemyHitboxCatalog* catalog);
[[nodiscard]] bool enemyHitboxOverlapsCircle(
    const Enemy& enemy,
    const EnemyHitboxCatalog* catalog,
    Vec2 circleCenter,
    float circleRadius);
[[nodiscard]] bool loadEnemyHitboxCatalog(
    const std::filesystem::path& path,
    EnemyHitboxCatalog& outCatalog,
    std::string& outMessage);
[[nodiscard]] bool saveEnemyHitboxCatalog(
    const std::filesystem::path& path,
    const EnemyHitboxCatalog& catalog,
    std::string& outMessage);

}
