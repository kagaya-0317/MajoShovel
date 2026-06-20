#pragma once

#include "engine/Math.hpp"
#include "game/Enemy.hpp"
#include "game/Hitbox.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace majo {

struct EnemyPlacementEntry {
    std::optional<float> passageRadius;
    std::array<std::optional<Vec2>, HitboxDirectionCount> visualOffsets{};

    bool operator==(const EnemyPlacementEntry& other) const;
};

struct EnemyPlacementCatalog {
    std::unordered_map<std::string, EnemyPlacementEntry> enemies;

    bool operator==(const EnemyPlacementCatalog&) const = default;
};

[[nodiscard]] float sanitizeEnemyPlacementRadius(float radius);
[[nodiscard]] Vec2 sanitizeEnemyPlacementOffset(Vec2 offset);
[[nodiscard]] EnemyPlacementEntry sanitizeEnemyPlacementEntry(EnemyPlacementEntry entry);
[[nodiscard]] bool enemyPlacementEntryHasAny(const EnemyPlacementEntry& entry);
[[nodiscard]] const EnemyPlacementEntry* enemyPlacementEntryFor(
    const EnemyPlacementCatalog* catalog,
    std::string_view enemyId);
[[nodiscard]] bool enemyPlacementHasAny(
    const EnemyPlacementCatalog& catalog,
    std::string_view enemyId);
[[nodiscard]] bool enemyPlacementHasPassageRadius(
    const EnemyPlacementCatalog& catalog,
    std::string_view enemyId);
[[nodiscard]] bool enemyPlacementHasVisualOffset(
    const EnemyPlacementCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction);
[[nodiscard]] std::optional<float> enemyPlacementPassageRadiusFor(
    const EnemyPlacementCatalog* catalog,
    std::string_view enemyId);
[[nodiscard]] Vec2 resolvedEnemyVisualOffset(
    const EnemyPlacementCatalog* catalog,
    std::string_view enemyId,
    HitboxDirection direction);
[[nodiscard]] Vec2 resolvedEnemyVisualOffset(
    const EnemyPlacementCatalog* catalog,
    const Enemy& enemy);
[[nodiscard]] EnemyPlacementEntry& mutableEnemyPlacementEntry(
    EnemyPlacementCatalog& catalog,
    std::string_view enemyId);
bool eraseEnemyPlacementEntry(
    EnemyPlacementCatalog& catalog,
    std::string_view enemyId);
bool eraseEnemyPlacementPassageRadius(
    EnemyPlacementCatalog& catalog,
    std::string_view enemyId);
bool eraseEnemyPlacementVisualOffset(
    EnemyPlacementCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction);
[[nodiscard]] bool loadEnemyPlacementCatalog(
    const std::filesystem::path& path,
    EnemyPlacementCatalog& outCatalog,
    std::string& outMessage);
[[nodiscard]] bool saveEnemyPlacementCatalog(
    const std::filesystem::path& path,
    const EnemyPlacementCatalog& catalog,
    std::string& outMessage);

}
