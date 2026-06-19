#pragma once

#include "engine/Math.hpp"
#include "game/Enemy.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

inline constexpr int HitboxMaxCircles = 8;
inline constexpr int HitboxDirectionCount = 5;

enum class HitboxDirection {
    Default = 0,
    Down = 1,
    Left = 2,
    Right = 3,
    Up = 4,
};

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

struct EnemyHitboxProfiles {
    std::array<HitboxProfile, HitboxDirectionCount> directions;

    bool operator==(const EnemyHitboxProfiles&) const = default;
};

struct HitboxCatalog {
    HitboxProfile player;
    std::unordered_map<std::string, EnemyHitboxProfiles> enemies;
    std::unordered_map<std::string, EnemyHitboxProfiles> bossWeakPoints;
    std::unordered_map<std::string, HitboxProfile> objects;

    bool operator==(const HitboxCatalog&) const = default;
};

[[nodiscard]] int hitboxDirectionIndex(HitboxDirection direction);
[[nodiscard]] std::string_view hitboxDirectionId(HitboxDirection direction);
[[nodiscard]] std::string_view hitboxDirectionDisplayName(HitboxDirection direction);
[[nodiscard]] Vec2 hitboxDirectionVector(HitboxDirection direction);
[[nodiscard]] HitboxDirection enemyHitboxDirectionForFacing(float facingAngle);
[[nodiscard]] HitboxProfile singleCircleHitbox(float radius);
[[nodiscard]] const HitboxProfile* enemyHitboxProfileFor(
    const HitboxCatalog* catalog,
    const Enemy& enemy);
[[nodiscard]] const HitboxProfile* enemyHitboxProfileFor(
    const HitboxCatalog* catalog,
    std::string_view enemyId,
    HitboxDirection direction);
[[nodiscard]] bool enemyHitboxHasProfile(
    const HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction);
[[nodiscard]] bool enemyHitboxHasAnyProfile(
    const HitboxCatalog& catalog,
    std::string_view enemyId);
[[nodiscard]] HitboxProfile& mutableEnemyHitboxProfile(
    HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction);
bool eraseEnemyHitboxProfile(
    HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction);
[[nodiscard]] const HitboxProfile* bossWeakPointProfileFor(
    const HitboxCatalog* catalog,
    const Enemy& enemy);
[[nodiscard]] const HitboxProfile* bossWeakPointProfileFor(
    const HitboxCatalog* catalog,
    std::string_view enemyId,
    HitboxDirection direction);
[[nodiscard]] bool bossWeakPointHasProfile(
    const HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction);
[[nodiscard]] bool bossWeakPointHasAnyProfile(
    const HitboxCatalog& catalog,
    std::string_view enemyId);
[[nodiscard]] HitboxProfile& mutableBossWeakPointProfile(
    HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction);
bool eraseBossWeakPointProfile(
    HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction);
[[nodiscard]] const HitboxProfile* objectHitboxProfileFor(
    const HitboxCatalog* catalog,
    std::string_view objectId);
[[nodiscard]] const HitboxProfile* playerHitboxProfileFor(const HitboxCatalog* catalog);
[[nodiscard]] bool playerHitboxHasProfile(const HitboxCatalog& catalog);
[[nodiscard]] HitboxProfile& mutablePlayerHitboxProfile(HitboxCatalog& catalog);
bool erasePlayerHitboxProfile(HitboxCatalog& catalog);
[[nodiscard]] HitCircle fallbackEnemyHitCircle(const Enemy& enemy);
[[nodiscard]] float hitboxProfileBoundsRadius(
    const HitboxProfile& profile,
    float scale = 1.0f,
    float radiusPadding = 0.0f);
[[nodiscard]] float enemyHitboxBoundsRadius(const Enemy& enemy, const HitboxCatalog* catalog);
[[nodiscard]] float enemyHitboxBoundsRadius(
    const Enemy& enemy,
    const HitboxCatalog* catalog,
    Vec2 centerOffset);
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
    float circleRadius,
    Vec2 centerOffset = {});
[[nodiscard]] bool enemyHitboxOverlapsProfile(
    const Enemy& enemy,
    const HitboxCatalog* catalog,
    const HitboxProfile& profile,
    Vec2 profileCenter,
    float profileRotationRadians,
    float profileScale,
    float profileRadiusPadding = 0.0f,
    Vec2 centerOffset = {});
[[nodiscard]] bool loadHitboxCatalog(
    const std::filesystem::path& path,
    HitboxCatalog& outCatalog,
    std::string& outMessage);
[[nodiscard]] bool saveHitboxCatalog(
    const std::filesystem::path& path,
    const HitboxCatalog& catalog,
    std::string& outMessage);

}
