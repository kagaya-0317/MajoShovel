#pragma once

#include "engine/Math.hpp"
#include "game/Chunk.hpp"

#include <cstddef>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace majo {

struct Enemy;
struct ObjectDefinition;
struct SpellRingItem;

enum class RingImpactTargetKind {
    Terrain,
    Enemy,
};

enum class RingImpactResult {
    Hit,
    Break,
};

struct RingImpactSoundEvent {
    RingImpactTargetKind targetKind = RingImpactTargetKind::Terrain;
    RingImpactResult result = RingImpactResult::Hit;
    Vec2 position{};
    std::string sourceObjectId;
    std::string targetId;
    std::vector<std::string> sourceTags;
    std::vector<std::string> targetTags;
    float sourceWeightKg = 0.0f;
    float sourceSpeed = 0.0f;
    float impactPower = 0.0f;
};

struct RingImpactSoundPlayback {
    std::string cueId;
    float volumeScale = 1.0f;
    float pitchScale = 1.0f;
    int priority = 0;
    float intensity = 0.0f;
};

std::vector<std::string> collectRingImpactSourceTags(const ObjectDefinition* object, const SpellRingItem& item);
std::vector<std::string> collectRingImpactEnemyTargetTags(const Enemy& enemy);
std::vector<std::string> collectRingImpactTerrainTargetTags(TileType tileType);

RingImpactSoundEvent makeTerrainRingImpactSoundEvent(
    const SpellRingItem& item,
    const ObjectDefinition* object,
    TileType tileType,
    RingImpactResult result,
    Vec2 position,
    float impactPower);

RingImpactSoundEvent makeEnemyRingImpactSoundEvent(
    const SpellRingItem& item,
    const ObjectDefinition* object,
    const Enemy& enemy,
    RingImpactResult result,
    Vec2 position,
    float impactPower);

std::vector<RingImpactSoundPlayback> resolveRingImpactSoundEvents(
    std::span<const RingImpactSoundEvent> events,
    std::mt19937& rng,
    std::size_t maxCount = 4);

}
