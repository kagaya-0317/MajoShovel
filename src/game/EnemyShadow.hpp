#pragma once

#include "engine/Math.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace majo {

struct EnemyShadowSpec {
    Vec2 offset{0.0f, 22.0f};
    Vec2 scale{1.0f, 1.0f};

    bool operator==(const EnemyShadowSpec& other) const
    {
        return offset.x == other.offset.x &&
            offset.y == other.offset.y &&
            scale.x == other.scale.x &&
            scale.y == other.scale.y;
    }
};

struct EnemyShadowCatalog {
    std::unordered_map<std::string, EnemyShadowSpec> enemies;

    bool operator==(const EnemyShadowCatalog&) const = default;
};

[[nodiscard]] EnemyShadowSpec sanitizeEnemyShadowSpec(EnemyShadowSpec spec);
[[nodiscard]] EnemyShadowSpec defaultEnemyShadowSpec();
[[nodiscard]] const EnemyShadowSpec* enemyShadowSpecFor(
    const EnemyShadowCatalog* catalog,
    std::string_view enemyId);
[[nodiscard]] EnemyShadowSpec resolvedEnemyShadowSpec(
    const EnemyShadowCatalog* catalog,
    std::string_view enemyId);
bool eraseEnemyShadowSpec(EnemyShadowCatalog& catalog, std::string_view enemyId);
[[nodiscard]] bool loadEnemyShadowCatalog(
    const std::filesystem::path& path,
    EnemyShadowCatalog& outCatalog,
    std::string& outMessage);
[[nodiscard]] bool saveEnemyShadowCatalog(
    const std::filesystem::path& path,
    const EnemyShadowCatalog& catalog,
    std::string& outMessage);

}
