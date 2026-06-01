#pragma once

#include "engine/Math.hpp"
#include "game/Enemy.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

inline constexpr int HitboxMaxCircles = 8;

struct HitCircle {
    Vec2 offset{};
    float radius = 10.0f;

    bool operator==(const HitCircle& other) const
    {
        return offset.x == other.offset.x &&
            offset.y == other.offset.y &&
            radius == other.radius;
    }
};

struct HitboxProfile {
    std::vector<HitCircle> circles;

    bool operator==(const HitboxProfile&) const = default;
};

struct HitboxCatalog {
    std::unordered_map<std::string, HitboxProfile> enemies;
    std::unordered_map<std::string, HitboxProfile> objects;

    bool operator==(const HitboxCatalog&) const = default;
};

[[nodiscard]] HitboxProfile singleCircleHitbox(float radius);
[[nodiscard]] const HitboxProfile* enemyHitboxProfileFor(
    const HitboxCatalog* catalog,
    const Enemy& enemy);
[[nodiscard]] const HitboxProfile* objectHitboxProfileFor(
    const HitboxCatalog* catalog,
    std::string_view objectId);
[[nodiscard]] HitCircle fallbackEnemyHitCircle(const Enemy& enemy);
[[nodiscard]] float hitboxProfileBoundsRadius(
    const HitboxProfile& profile,
    float scale = 1.0f,
    float radiusPadding = 0.0f);
[[nodiscard]] float enemyHitboxBoundsRadius(const Enemy& enemy, const HitboxCatalog* catalog);
[[nodiscard]] bool hitboxProfileOverlapsCircle(
    const HitboxProfile& profile,
    Vec2 center,
    float rotationRadians,
    float scale,
    float radiusPadding,
    Vec2 circleCenter,
    float circleRadius);
[[nodiscard]] bool hitboxProfilesOverlap(
    const HitboxProfile& lhs,
    Vec2 lhsCenter,
    float lhsRotationRadians,
    float lhsScale,
    float lhsRadiusPadding,
    const HitboxProfile& rhs,
    Vec2 rhsCenter,
    float rhsRotationRadians,
    float rhsScale,
    float rhsRadiusPadding);
[[nodiscard]] bool enemyHitboxOverlapsCircle(
    const Enemy& enemy,
    const HitboxCatalog* catalog,
    Vec2 circleCenter,
    float circleRadius);
[[nodiscard]] bool enemyHitboxOverlapsProfile(
    const Enemy& enemy,
    const HitboxCatalog* catalog,
    const HitboxProfile& profile,
    Vec2 profileCenter,
    float profileRotationRadians,
    float profileScale,
    float profileRadiusPadding = 0.0f);
[[nodiscard]] bool loadHitboxCatalog(
    const std::filesystem::path& path,
    HitboxCatalog& outCatalog,
    std::string& outMessage);
[[nodiscard]] bool saveHitboxCatalog(
    const std::filesystem::path& path,
    const HitboxCatalog& catalog,
    std::string& outMessage);

}
