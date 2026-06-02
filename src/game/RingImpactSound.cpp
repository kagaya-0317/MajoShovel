#include "game/RingImpactSound.hpp"

#include "data/ObjectCatalog.hpp"
#include "game/Enemy.hpp"
#include "game/SpellRingItem.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>
#include <unordered_set>

namespace majo {

namespace {

constexpr float DefaultRandomPitch = 0.025f;

struct CueRule {
    std::string_view cueId;
    int priority = 0;
    float volume = 1.0f;
    float basePitch = 1.0f;
    float randomPitch = DefaultRandomPitch;
};

void appendUnique(std::vector<std::string>& tags, std::string_view tag)
{
    if (tag.empty()) {
        return;
    }
    if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
        tags.emplace_back(tag);
    }
}

void appendUnique(std::vector<std::string>& tags, const std::vector<std::string>& extraTags)
{
    for (const std::string& tag : extraTags) {
        appendUnique(tags, tag);
    }
}

bool hasTag(std::span<const std::string> tags, std::string_view expected)
{
    return std::any_of(tags.begin(), tags.end(), [expected](const std::string& tag) {
        return tag == expected;
    });
}

bool hasAnyTag(std::span<const std::string> tags, std::initializer_list<std::string_view> expectedTags)
{
    return std::any_of(expectedTags.begin(), expectedTags.end(), [tags](std::string_view expected) {
        return hasTag(tags, expected);
    });
}

void appendDerivedImpactTags(std::vector<std::string>& tags)
{
    if (hasAnyTag(tags, {"slime", "elastic"})) {
        appendUnique(tags, "soft");
    }
    if (hasTag(tags, "slime")) {
        appendUnique(tags, "elastic");
    }
}

bool isTerrain(const RingImpactSoundEvent& event)
{
    return event.targetKind == RingImpactTargetKind::Terrain;
}

bool isEnemy(const RingImpactSoundEvent& event)
{
    return event.targetKind == RingImpactTargetKind::Enemy;
}

bool sourceHas(const RingImpactSoundEvent& event, std::string_view tag)
{
    return hasTag(event.sourceTags, tag);
}

bool sourceHasAny(const RingImpactSoundEvent& event, std::initializer_list<std::string_view> tags)
{
    return hasAnyTag(event.sourceTags, tags);
}

bool targetHas(const RingImpactSoundEvent& event, std::string_view tag)
{
    return hasTag(event.targetTags, tag);
}

bool targetHasAny(const RingImpactSoundEvent& event, std::initializer_list<std::string_view> tags)
{
    return hasAnyTag(event.targetTags, tags);
}

bool sourceIsHard(const RingImpactSoundEvent& event)
{
    return sourceHasAny(event, {"hard", "metal", "stone", "rugged", "heavy", "blunt", "glass", "wood"});
}

bool targetIsHard(const RingImpactSoundEvent& event)
{
    return targetHasAny(event, {"hard", "metal", "stone", "rugged", "heavy"});
}

CueRule sourceTextureRule(const RingImpactSoundEvent& event)
{
    if (sourceHas(event, "crisp")) {
        return {"se.impact.crisp", 90, 0.92f, 1.06f, 0.030f};
    }
    if (sourceHas(event, "book")) {
        return {"se.impact.paper.book", 88, 0.82f, 1.0f, 0.035f};
    }
    if (sourceHasAny(event, {"mesh", "bristle"})) {
        return {"se.impact.fiber.rustle", 86, 0.76f, 1.04f, 0.040f};
    }
    if (sourceHasAny(event, {"cloth", "fluffy"})) {
        return {"se.impact.cloth.fluffy", 84, 0.80f, 0.98f, 0.035f};
    }
    if (sourceHas(event, "hollow") && sourceHas(event, "resonant")) {
        if (sourceHasAny(event, {"heavy", "large"}) || event.sourceWeightKg >= 0.75f) {
            return {"se.impact.resonant.gong", 92, 0.98f, 0.92f, 0.012f};
        }
        return {"se.impact.resonant.chime", 92, 0.88f, 1.08f, 0.018f};
    }
    if (sourceHas(event, "trinket") && sourceHas(event, "metal")) {
        return {"se.impact.metal.trinket", 86, 0.86f, 1.08f, 0.020f};
    }
    if (sourceHas(event, "glass")) {
        return {"se.impact.glass", 82, 0.86f, 1.04f, 0.025f};
    }
    return {};
}

CueRule terrainRule(const RingImpactSoundEvent& event)
{
    if (!isTerrain(event)) {
        return {};
    }
    if (targetHas(event, "dirt")) {
        if (sourceHas(event, "scoop")) {
            return {"se.impact.dirt.scoop", 110, 1.0f, 0.98f, 0.028f};
        }
        if (sourceHas(event, "blade")) {
            return {"se.impact.dirt.blade", 108, 0.94f, 0.96f, 0.026f};
        }
        if (sourceHasAny(event, {"pointed", "pierce", "spike"})) {
            return {"se.impact.dirt.pointed", 106, 0.92f, 1.0f, 0.028f};
        }
        return {};
    }
    if (targetHasAny(event, {"rock", "ore", "stone"})) {
        if (sourceHasAny(event, {"pointed", "hard_dig_tool", "pierce", "spike"})) {
            return {"se.impact.rock.pointed", 112, 1.0f, 0.94f, 0.020f};
        }
        if (sourceHas(event, "metal")) {
            return {"se.impact.rock.metal", 110, 0.98f, 0.98f, 0.018f};
        }
        if (sourceHasAny(event, {"stone", "rugged"})) {
            return {"se.impact.rock.stone", 106, 0.96f, 0.92f, 0.022f};
        }
        return {};
    }
    return {};
}

CueRule enemyMaterialRule(const RingImpactSoundEvent& event)
{
    if (!isEnemy(event)) {
        return {};
    }
    if (sourceHas(event, "slime") && targetHas(event, "slime")) {
        return {"se.impact.slime.slime", 120, 0.88f, 1.04f, 0.040f};
    }
    if (sourceHasAny(event, {"stone", "rugged"}) && targetHas(event, "metal")) {
        return {"se.impact.stone.metal", 112, 0.96f, 0.94f, 0.018f};
    }
    if (sourceHas(event, "metal") && targetHas(event, "metal")) {
        return {"se.impact.metal.metal", 112, 0.96f, 1.02f, 0.014f};
    }
    if ((sourceIsHard(event) && targetHas(event, "soft")) ||
        (sourceHas(event, "soft") && targetIsHard(event))) {
        return {"se.impact.hard.soft", 108, 0.92f, 0.96f, 0.035f};
    }
    if (sourceHas(event, "soft") && targetHas(event, "soft")) {
        return {"se.impact.soft.soft", 106, 0.84f, 1.02f, 0.040f};
    }
    if (sourceHas(event, "metal") && targetHasAny(event, {"stone", "rugged", "hard"})) {
        return {"se.impact.rock.metal", 104, 0.94f, 1.0f, 0.018f};
    }
    if (sourceHasAny(event, {"stone", "rugged"}) && targetHasAny(event, {"stone", "rugged", "hard"})) {
        return {"se.impact.rock.stone", 102, 0.92f, 0.92f, 0.022f};
    }
    if (sourceHas(event, "wood")) {
        return {"se.impact.wood", 72, 0.82f, 1.0f, 0.030f};
    }
    return {};
}

CueRule fallbackRule(const RingImpactSoundEvent& event)
{
    if (isTerrain(event)) {
        if (targetHas(event, "dirt")) {
            return {"se.impact.dirt.generic", 40, 0.82f, 1.0f, 0.030f};
        }
        if (targetHasAny(event, {"rock", "ore", "stone"})) {
            return {"se.impact.rock.stone", 40, 0.86f, 0.94f, 0.022f};
        }
    }
    if (sourceHas(event, "metal")) {
        return {"se.impact.metal.metal", 35, 0.78f, 1.0f, 0.018f};
    }
    if (sourceHasAny(event, {"stone", "rugged"})) {
        return {"se.impact.rock.stone", 34, 0.80f, 0.94f, 0.022f};
    }
    if (sourceHas(event, "soft")) {
        return {"se.impact.soft.soft", 33, 0.74f, 1.02f, 0.040f};
    }
    return {"se.impact.generic", 10, 0.74f, 1.0f, 0.030f};
}

CueRule chooseCueRule(const RingImpactSoundEvent& event)
{
    if (event.result == RingImpactResult::Guard) {
        return {"se.enemy.guard", 130, 1.0f, 1.0f, 0.018f};
    }
    if (const CueRule rule = terrainRule(event); !rule.cueId.empty()) {
        return rule;
    }
    if (const CueRule rule = sourceTextureRule(event); !rule.cueId.empty()) {
        return rule;
    }
    if (const CueRule rule = enemyMaterialRule(event); !rule.cueId.empty()) {
        return rule;
    }
    return fallbackRule(event);
}

float clampFloat(float value, float minValue, float maxValue)
{
    return std::max(minValue, std::min(maxValue, value));
}

float eventIntensity(const RingImpactSoundEvent& event)
{
    const float speedScore = clampFloat(std::abs(event.sourceSpeed) / 280.0f, 0.0f, 1.0f);
    const float weightScore = clampFloat(event.sourceWeightKg / 3.6f, 0.0f, 1.0f);
    const float powerScore = clampFloat(event.impactPower / 24.0f, 0.0f, 1.0f);
    const float breakScore = event.result == RingImpactResult::Break ? 0.18f : 0.0f;
    const float guardScore = event.result == RingImpactResult::Guard ? 0.22f : 0.0f;
    return clampFloat(speedScore * 0.35f + weightScore * 0.25f + powerScore * 0.25f + breakScore + guardScore, 0.0f, 1.0f);
}

float sizePitchOffset(const RingImpactSoundEvent& event)
{
    if (sourceHasAny(event, {"heavy", "large"})) {
        return -0.040f;
    }
    if (sourceHas(event, "small")) {
        return 0.030f;
    }
    return 0.0f;
}

RingImpactSoundPlayback makePlayback(const RingImpactSoundEvent& event, const CueRule& rule, std::mt19937& rng)
{
    std::uniform_real_distribution<float> pitchDist(-rule.randomPitch, rule.randomPitch);
    const float intensity = eventIntensity(event);
    return RingImpactSoundPlayback{
        .cueId = std::string(rule.cueId),
        .position = event.position,
        .volumeScale = clampFloat(rule.volume * (0.82f + intensity * 0.30f), 0.45f, 1.18f),
        .pitchScale = clampFloat(rule.basePitch + sizePitchOffset(event) + pitchDist(rng), 0.72f, 1.28f),
        .priority = rule.priority,
        .intensity = intensity,
    };
}

} // namespace

std::vector<std::string> collectRingImpactSourceTags(const ObjectDefinition* object, const SpellRingItem& item)
{
    std::vector<std::string> tags;
    if (object != nullptr) {
        appendUnique(tags, object->tags);
    }
    appendUnique(tags, item.addedTags);
    if (!item.damageType.empty() && item.damageType != "none") {
        appendUnique(tags, item.damageType);
    }
    appendDerivedImpactTags(tags);
    return tags;
}

std::vector<std::string> collectRingImpactEnemyTargetTags(const Enemy& enemy)
{
    std::vector<std::string> tags;
    appendUnique(tags, "enemy");
    appendUnique(tags, enemy.enemyTags);
    appendDerivedImpactTags(tags);
    if (tags.size() == 1) {
        appendUnique(tags, "soft");
    }
    return tags;
}

std::vector<std::string> collectRingImpactTerrainTargetTags(TileType tileType)
{
    std::vector<std::string> tags;
    appendUnique(tags, "terrain");
    switch (tileType) {
    case TileType::Dirt:
        appendUnique(tags, "dirt");
        appendUnique(tags, "soft");
        break;
    case TileType::Rock:
        appendUnique(tags, "rock");
        appendUnique(tags, "stone");
        appendUnique(tags, "hard");
        appendUnique(tags, "rugged");
        break;
    case TileType::Ore:
        appendUnique(tags, "ore");
        appendUnique(tags, "stone");
        appendUnique(tags, "metal");
        appendUnique(tags, "hard");
        appendUnique(tags, "rugged");
        break;
    case TileType::HardRock:
        appendUnique(tags, "rock");
        appendUnique(tags, "stone");
        appendUnique(tags, "hard");
        appendUnique(tags, "rugged");
        appendUnique(tags, "heavy");
        break;
    case TileType::Empty:
        break;
    }
    return tags;
}

RingImpactSoundEvent makeTerrainRingImpactSoundEvent(
    const SpellRingItem& item,
    const ObjectDefinition* object,
    TileType tileType,
    RingImpactResult result,
    Vec2 position,
    float impactPower)
{
    return RingImpactSoundEvent{
        .targetKind = RingImpactTargetKind::Terrain,
        .result = result,
        .position = position,
        .sourceObjectId = item.objectId,
        .targetId = "terrain",
        .sourceTags = collectRingImpactSourceTags(object, item),
        .targetTags = collectRingImpactTerrainTargetTags(tileType),
        .sourceWeightKg = item.weight,
        .sourceSpeed = item.orbitMotionSpeed,
        .impactPower = impactPower,
    };
}

RingImpactSoundEvent makeEnemyRingImpactSoundEvent(
    const SpellRingItem& item,
    const ObjectDefinition* object,
    const Enemy& enemy,
    RingImpactResult result,
    Vec2 position,
    float impactPower)
{
    return RingImpactSoundEvent{
        .targetKind = RingImpactTargetKind::Enemy,
        .result = result,
        .position = position,
        .sourceObjectId = item.objectId,
        .targetId = enemy.enemyId,
        .sourceTags = collectRingImpactSourceTags(object, item),
        .targetTags = collectRingImpactEnemyTargetTags(enemy),
        .sourceWeightKg = item.weight,
        .sourceSpeed = item.orbitMotionSpeed,
        .impactPower = impactPower,
    };
}

std::vector<RingImpactSoundPlayback> resolveRingImpactSoundEvents(
    std::span<const RingImpactSoundEvent> events,
    std::mt19937& rng,
    std::size_t maxCount)
{
    std::vector<RingImpactSoundPlayback> candidates;
    candidates.reserve(events.size());
    for (const RingImpactSoundEvent& event : events) {
        const CueRule rule = chooseCueRule(event);
        if (!rule.cueId.empty()) {
            candidates.push_back(makePlayback(event, rule, rng));
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const RingImpactSoundPlayback& lhs, const RingImpactSoundPlayback& rhs) {
        if (lhs.priority != rhs.priority) {
            return lhs.priority > rhs.priority;
        }
        return lhs.intensity > rhs.intensity;
    });

    std::vector<RingImpactSoundPlayback> result;
    result.reserve(std::min(maxCount, candidates.size()));
    std::unordered_set<std::string> usedCueIds;
    for (const RingImpactSoundPlayback& playback : candidates) {
        if (!usedCueIds.insert(playback.cueId).second) {
            continue;
        }
        result.push_back(playback);
        if (result.size() >= maxCount) {
            break;
        }
    }
    return result;
}

}
