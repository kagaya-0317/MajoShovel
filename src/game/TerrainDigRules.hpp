#pragma once

#include "game/TileMap.hpp"

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace majo {

struct ObjectDefinition;
struct SpellRingItem;
class SpellRingSystem;

enum class TerrainDigMode {
    Normal,
    HardSpecialist,
    Multi,
};

struct TerrainDigProfile {
    bool enabled = false;
    TerrainDigMode mode = TerrainDigMode::Normal;
    int power = 0;
};

inline constexpr std::array<std::pair<int, int>, 5> TerrainDigMultiOffsets{{
    {0, 0},
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1},
}};

bool isTerrainDigTarget(std::string_view target);
bool isTerrainDigEffect(std::string_view effect);
std::optional<TerrainDigMode> terrainDigModeForEffect(std::string_view effect);
std::string_view terrainDigEffectForMode(TerrainDigMode mode);
TerrainDigModifier terrainDigModifierForMode(TerrainDigMode mode);
int terrainDigBasePowerForMode(int digPower, TerrainDigMode mode);
TerrainDigProfile terrainDigProfileFor(const ObjectDefinition* object, const SpellRingItem* item);
int terrainDigDamageForRingHit(
    const TerrainDigProfile& profile,
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    TerrainAttribute attribute);

}
