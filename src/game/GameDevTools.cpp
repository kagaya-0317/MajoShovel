#include "game/GameInternal.hpp"

#include "engine/Audio.hpp"
#include "game/EffectPreviewCatalog.hpp"
#include "game/EnemyImageRenderer.hpp"
#include "game/EntityStatusVisuals.hpp"
#include "game/WorldIconRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

namespace majo {

namespace {

constexpr float DebugItemPickerPanelMaxWidth = 1200.0f;
constexpr float DebugItemPickerPanelMaxHeight = 660.0f;
constexpr float DebugItemPickerPanelMargin = 24.0f;
constexpr float DebugItemPickerDetailWidth = 332.0f;
constexpr float DebugItemPickerPaneGap = 18.0f;
constexpr float DebugItemPickerButtonAreaHeight = 66.0f;
constexpr float DebugItemPickerSearchHeight = 44.0f;
constexpr float DebugItemPickerSearchGap = 12.0f;
constexpr float DebugItemPickerCardWidth = 118.0f;
constexpr float DebugItemPickerCardHeight = 106.0f;
constexpr float DebugItemPickerCardGap = 10.0f;
constexpr float DebugItemPickerIconSize = 58.0f;
constexpr float DebugNamedSavePanelWidth = 620.0f;
constexpr float DebugNamedSaveInputPanelHeight = 260.0f;
constexpr float DebugNamedSaveLoadPanelHeight = 520.0f;
constexpr float DebugNamedSaveRowHeight = 46.0f;
constexpr float DebugNamedSaveRowGap = 5.0f;
constexpr float ObjectImageScaleSearchHeight = 42.0f;
constexpr float ObjectImageScaleSearchGap = 10.0f;
constexpr float EnemyHitboxHeaderHeight = 82.0f;
constexpr float EnemyHitboxFooterHeight = 58.0f;
constexpr float EnemyHitboxPanelMargin = 20.0f;
constexpr float EnemyHitboxPanelGap = 16.0f;
constexpr float EnemyHitboxListWidth = 330.0f;
constexpr float EnemyHitboxSearchHeight = 40.0f;
constexpr float EnemyHitboxRowHeight = 44.0f;
constexpr float EnemyHitboxRowGap = 4.0f;
constexpr float EnemyHitboxDetailMinWidth = 226.0f;
constexpr float EnemyHitboxDetailPreferredWidth = 320.0f;
constexpr float EnemyHitboxDetailRightMargin = 22.0f;
constexpr float EnemyHitboxButtonHeight = 34.0f;
constexpr float EnemyHitboxButtonGap = 8.0f;
constexpr int EnemyHitboxDetailButtonColumns = 2;
constexpr float EnemyHitboxDetailButtonTop = 114.0f;
constexpr float EnemyHitboxDirectionButtonTop = 338.0f;
constexpr float EnemyHitboxSelectedCircleInfoTop = 390.0f;
constexpr float EnemyHitboxCircleStep = 1.0f;
constexpr float EnemyHitboxCircleRadiusStep = 1.0f;
constexpr float EnemyHitboxMinRadius = 1.0f;
constexpr float EnemyHitboxMaxRadius = 256.0f;
constexpr float EnemyShadowOffsetStep = 1.0f;
constexpr float EnemyShadowScaleStep = 0.05f;
constexpr float EnemyShadowPreviewScale = 4.0f;
constexpr float EnemyShadowPreviewDirectionSeconds = 0.5f;
constexpr int HitboxEditUndoLimit = 100;
constexpr float DebugStoryTestDetailWidth = 360.0f;
constexpr float DebugStoryTestRowHeight = 56.0f;
constexpr float DebugStoryTestRowGap = 5.0f;
constexpr float DebugPreviewTestPanelMargin = 22.0f;
constexpr float DebugPreviewTestListWidth = 360.0f;
constexpr float DebugPreviewTestPanelGap = 16.0f;
constexpr float DebugPreviewTestFooterHeight = 62.0f;
constexpr float DebugPreviewTestRowHeight = 48.0f;
constexpr float DebugPreviewTestRowGap = 5.0f;
constexpr float DebugPreviewTestHeaderHeight = 42.0f;
constexpr float DebugPreviewTestTabRowGap = 4.0f;
constexpr float DebugPreviewBackgroundLabelWidth = 52.0f;
constexpr float DebugPreviewBackgroundSwatchSize = 28.0f;
constexpr float DebugPreviewBackgroundSwatchGap = 7.0f;
constexpr int DebugEffectPreviewTestLoopFrames = 40;

std::filesystem::path dungeonDebugDumpPath()
{
    return std::filesystem::path(".local") / "dungeon_dump_latest.txt";
}

float debugPathLengthTiles(const std::vector<Vec2>& points)
{
    float total = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        total += length(points[i] - points[i - 1]);
    }
    return total;
}
constexpr int DebugProjectilePreviewReplayGapFrames = 40;
constexpr float DebugPreviewAssumedFrameRate = 60.0f;
constexpr float DebugEffectPreviewTestLoopSeconds =
    static_cast<float>(DebugEffectPreviewTestLoopFrames) / DebugPreviewAssumedFrameRate;
constexpr float DebugProjectilePreviewReplayGapSeconds =
    static_cast<float>(DebugProjectilePreviewReplayGapFrames) / DebugPreviewAssumedFrameRate;
constexpr std::string_view DebugPreviewTestSlimeEnemyId = "slime";
constexpr std::string_view DebugFinalStoryStageId = "stage_03_star_core";
constexpr std::string_view DebugEndingSeenFlag = "ending_seen";
constexpr std::string_view DebugEndingMainFlag = "story_ending_main";
constexpr std::string_view DebugStage03ClearFlag = "story_stage_03_clear";
constexpr std::string_view DebugPostEndingIntroFlag = "story_post_ending_intro";
constexpr std::string_view AudioCueEditManifestPath = "assets/audio/audio_manifest.tsv";
constexpr std::string_view AudioCueEditAudioRoot = "assets/audio";
constexpr float AudioCueEditPanelMaxWidth = 1200.0f;
constexpr float AudioCueEditPanelMaxHeight = 680.0f;
constexpr float AudioCueEditPanelMargin = 24.0f;
constexpr float AudioCueEditPaneGap = 14.0f;
constexpr float AudioCueEditCueListWidth = 330.0f;
constexpr float AudioCueEditDetailWidth = 330.0f;
constexpr float AudioCueEditButtonAreaHeight = 64.0f;
constexpr float AudioCueEditRowHeight = 44.0f;
constexpr float AudioCueEditRowGap = 4.0f;
constexpr int EnemyTestMagnetDropCount = 7;
constexpr float EnemyTestMagnetDropMinRadius = 72.0f;
constexpr float EnemyTestMagnetDropMaxRadius = 178.0f;
constexpr int EnemyTestStealMoneyDropCount = 4;
constexpr int EnemyTestStealTreasureDropCount = 4;
constexpr float EnemyTestStealDropMinRadius = 62.0f;
constexpr float EnemyTestStealDropMaxRadius = 112.0f;
constexpr int EnemyTestHealSlimeCount = 6;
constexpr float EnemyTestHealSlimeMinRadius = 46.0f;
constexpr float EnemyTestHealSlimeMaxRadius = 92.0f;
constexpr float EnemyTestHealSlimeLeashRadius = 125.0f;
constexpr std::string_view EnemyTestHealSlimeEnemyId = "slime";
constexpr int EnemyTestSwarmExtraCount = 5;
constexpr float EnemyTestSwarmMinRadius = 34.0f;
constexpr float EnemyTestSwarmMaxRadius = 92.0f;
constexpr float EnemyTestSwarmLeashRadius = 150.0f;

bool enemyDefinitionHasBehavior(const EnemyDefinition& enemy, std::string_view behaviorId)
{
    return std::any_of(enemy.enemyBehaviorIds.begin(), enemy.enemyBehaviorIds.end(), [behaviorId](const std::string& id) {
        return id == behaviorId;
    });
}

LootChestKind chestKindForEnemyTestMimic(const EnemyDefinition& enemy)
{
    LootChestKind kind = LootChestKind::Rare;
    if (chestKindForChestMimicEnemyId(enemy.id, kind)) {
        return kind;
    }

    for (const EnemyBehaviorSpec& spec : enemy.enemyBehaviorSpecs) {
        if (spec.behavior != "drop_item") {
            continue;
        }
        const auto profileIt = spec.params.find("profile");
        if (profileIt != spec.params.end() && chestKindForBoxDropProfile(profileIt->second, kind)) {
            return kind;
        }
    }

    return kind;
}

bool enemyDefinitionIsChestMimic(const EnemyDefinition& enemy)
{
    LootChestKind unused = LootChestKind::Common;
    if (chestKindForChestMimicEnemyId(enemy.id, unused)) {
        return true;
    }
    return enemyDefinitionHasBehavior(enemy, "chest_bite") && enemyDefinitionHasBehavior(enemy, "drop_item");
}

bool objectDefinitionHasTag(const ObjectDefinition& object, std::string_view tag)
{
    return std::any_of(object.tags.begin(), object.tags.end(), [tag](const std::string& objectTag) {
        return objectTag == tag;
    });
}

bool objectDefinitionHasAnyTag(const ObjectDefinition& object, std::initializer_list<std::string_view> tags)
{
    return std::any_of(tags.begin(), tags.end(), [&object](std::string_view tag) {
        return objectDefinitionHasTag(object, tag);
    });
}

bool objectIsEnemyTestMetalDropCandidate(const ObjectDefinition& object)
{
    return !object.id.empty() &&
        objectDefinitionHasTag(object, "metal") &&
        !objectDefinitionHasAnyTag(object, {"no_drop", "nodrop", "shop_only", "ショップ専用"});
}

bool objectIsEnemyTestTreasureDropCandidate(const ObjectDefinition& object)
{
    return !object.id.empty() &&
        (object.category == "\xE5\xAE\x9D" || objectDefinitionHasTag(object, "treasure")) &&
        !objectDefinitionHasAnyTag(object, {"no_drop", "nodrop", "shop_only", "ショップ専用"});
}

const EnemyBehaviorSpec* enemyDefinitionBehaviorSpec(const EnemyDefinition& enemy, std::string_view behaviorId)
{
    for (const EnemyBehaviorSpec& spec : enemy.enemyBehaviorSpecs) {
        if (spec.behavior == behaviorId) {
            return &spec;
        }
    }
    return nullptr;
}

std::string enemyTestStealTargetFilter(const EnemyDefinition& enemy)
{
    const EnemyBehaviorSpec* spec = enemyDefinitionBehaviorSpec(enemy, "steal_item");
    if (spec == nullptr) {
        return {};
    }
    const auto targetIt = spec->params.find("target");
    if (targetIt == spec->params.end()) {
        return "money|treasure|drop";
    }
    return targetIt->second;
}

bool enemyTestStealTargetContains(std::string_view targetFilter, std::string_view token)
{
    std::size_t start = 0;
    while (start <= targetFilter.size()) {
        const std::size_t end = targetFilter.find('|', start);
        const std::size_t count = end == std::string_view::npos ? std::string_view::npos : end - start;
        const std::string normalized = lowerAscii(trimAscii(std::string(targetFilter.substr(start, count))));
        if (normalized == std::string(token)) {
            return true;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

Vec2 randomEnemyTestMagnetDropPosition(Vec2 center, TileMap& tileMap, std::mt19937& rng)
{
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> radiusDistribution(EnemyTestMagnetDropMinRadius, EnemyTestMagnetDropMaxRadius);
    for (int attempt = 0; attempt < 16; ++attempt) {
        const Vec2 candidate = center + fromAngle(angleDistribution(rng)) * radiusDistribution(rng);
        if (!tileMap.isCircleBlocked(candidate, 12.0f)) {
            return candidate;
        }
    }
    return center + fromAngle(angleDistribution(rng)) * EnemyTestMagnetDropMinRadius;
}

Vec2 randomEnemyTestStealDropPosition(Vec2 center, TileMap& tileMap, std::mt19937& rng)
{
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> radiusDistribution(EnemyTestStealDropMinRadius, EnemyTestStealDropMaxRadius);
    for (int attempt = 0; attempt < 16; ++attempt) {
        const Vec2 candidate = center + fromAngle(angleDistribution(rng)) * radiusDistribution(rng);
        if (!tileMap.isCircleBlocked(candidate, 12.0f)) {
            return candidate;
        }
    }
    return center + fromAngle(angleDistribution(rng)) * EnemyTestStealDropMinRadius;
}

Vec2 randomEnemyTestHealSlimePosition(Vec2 center, std::mt19937& rng)
{
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> radiusDistribution(EnemyTestHealSlimeMinRadius, EnemyTestHealSlimeMaxRadius);
    return center + fromAngle(angleDistribution(rng)) * radiusDistribution(rng);
}

Vec2 randomEnemyTestSwarmPosition(Vec2 center, TileMap& tileMap, std::mt19937& rng)
{
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> radiusDistribution(EnemyTestSwarmMinRadius, EnemyTestSwarmMaxRadius);
    for (int attempt = 0; attempt < 16; ++attempt) {
        const Vec2 candidate = center + fromAngle(angleDistribution(rng)) * radiusDistribution(rng);
        if (!tileMap.isCircleBlocked(candidate, 10.0f)) {
            return candidate;
        }
    }
    return center + fromAngle(angleDistribution(rng)) * EnemyTestSwarmMinRadius;
}

GameTestTerrainKind gameTestTerrainKind(TileType type)
{
    switch (type) {
    case TileType::Empty: return GameTestTerrainKind::Empty;
    case TileType::Dirt: return GameTestTerrainKind::Dirt;
    case TileType::Rock: return GameTestTerrainKind::Rock;
    case TileType::Ore: return GameTestTerrainKind::Ore;
    case TileType::HardRock: return GameTestTerrainKind::HardRock;
    }
    return GameTestTerrainKind::Empty;
}

GameTestTerrainAttribute gameTestTerrainAttribute(TerrainAttribute attribute)
{
    switch (attribute) {
    case TerrainAttribute::None: return GameTestTerrainAttribute::None;
    case TerrainAttribute::Soft: return GameTestTerrainAttribute::Soft;
    case TerrainAttribute::Hard: return GameTestTerrainAttribute::Hard;
    case TerrainAttribute::Ore: return GameTestTerrainAttribute::Ore;
    }
    return GameTestTerrainAttribute::None;
}

RingLevelUpgradeKind gameTestLevelUpUpgradeKind(int option)
{
    switch (option) {
    case 1:
        return RingLevelUpgradeKind::Speed;
    case 2:
        return RingLevelUpgradeKind::WeightLimit;
    case 0:
    default:
        return RingLevelUpgradeKind::Radius;
    }
}

float positiveStaffMultiplierScore(double multiplier, float weight)
{
    if (!std::isfinite(multiplier)) {
        return 0.0f;
    }
    return static_cast<float>(std::max(0.0, multiplier - 1.0) * static_cast<double>(weight));
}

float lowerStaffMultiplierScore(double multiplier, float weight)
{
    if (!std::isfinite(multiplier)) {
        return 0.0f;
    }
    return static_cast<float>(std::max(0.0, 1.0 - multiplier) * static_cast<double>(weight));
}

float autoSimulationStaffEquipScore(const ItemData& item)
{
    if (!isStaffObject(item) || item.durability == 0) {
        return 0.0f;
    }

    const EquipmentModifiers modifiers = collectStaffEquipmentModifiers(item, "autosim");
    float score = 24.0f;
    score += static_cast<float>(std::clamp(item.rarity, 0, 10)) * 6.0f;
    score += std::min(30.0f, static_cast<float>(std::max(0, item.price)) * 0.015f);
    score += static_cast<float>(modifiers.sources.size()) * 6.0f;

    for (const RingEquipmentModifiers& ring : modifiers.rings) {
        score += positiveStaffMultiplierScore(ring.ringOutputMul, 85.0f);
        score += positiveStaffMultiplierScore(ring.digPowerMul, 80.0f);
        score += positiveStaffMultiplierScore(ring.ringSpeedMul, 32.0f);
        score += positiveStaffMultiplierScore(ring.ringRadiusMul, 26.0f);
        score += positiveStaffMultiplierScore(ring.ringShiftDistanceMul, 20.0f);
        score += positiveStaffMultiplierScore(ring.ringThrowDistanceMul, 22.0f);
        score += positiveStaffMultiplierScore(ring.ringThrowSpeedMul, 18.0f);
        score += lowerStaffMultiplierScore(ring.ringThrowCooldownMul, 30.0f);
        score += positiveStaffMultiplierScore(ring.ringReturnSpeedMul, 16.0f);
        score += positiveStaffMultiplierScore(ring.ringDamageSpeedMul, 24.0f);
        score += positiveStaffMultiplierScore(ring.lightRadiusMul, 12.0f);
        score += positiveStaffMultiplierScore(ring.detectRangeMul, 18.0f);
        score += positiveStaffMultiplierScore(ring.guardAreaMul, 18.0f);
        score += positiveStaffMultiplierScore(ring.reflectPowerMul, 18.0f);
        score += static_cast<float>(std::max(0.0, ring.reflectChanceAdd)) * 40.0f;
        score += lowerStaffMultiplierScore(ring.metalWeightPenaltyMul, 18.0f);
        score += static_cast<float>(std::max(0.0, ring.ringWeightLimitAdd)) * 1.8f;
    }

    score += lowerStaffMultiplierScore(modifiers.durabilityCostMul, 40.0f);
    score += positiveStaffMultiplierScore(modifiers.sellPriceMul, 8.0f);
    score += static_cast<float>(std::max(0, modifiers.moneyVisibleLevel)) * 6.0f;
    score += static_cast<float>(std::max(0, modifiers.dangerHintLevel)) * 6.0f;
    return std::max(0.0f, score);
}

float autoSimulationLightRadiusFromOrbitEffects(const std::vector<EffectSpec>& effects)
{
    float radius = 0.0f;
    for (const EffectSpec& spec : effects) {
        if (spec.target != "area") {
            continue;
        }
        const std::size_t count = std::min(spec.effects.size(), spec.values.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (spec.effects[i] == "light") {
                radius = std::max(radius, areaEffectRadiusFromValue(spec.values[i]));
            }
        }
    }
    return radius;
}

bool autoSimulationIntentHasIcon(const autosim::AutoSimulationIntent& intent)
{
    return intent.iconKind != autosim::AutoSimulationIntentIconKind::None;
}

std::string autoSimulationIntentSubjectText(const autosim::AutoSimulationIntent& intent)
{
    return intent.subject + intent.suffix;
}

bool drawAutoSimulationIntentIcon(
    Renderer& renderer,
    const ObjectCatalog& objectCatalog,
    const autosim::AutoSimulationIntent& intent,
    Vec2 center,
    float size,
    unsigned char alpha)
{
    switch (intent.iconKind) {
    case autosim::AutoSimulationIntentIconKind::Object: {
        ObjectImageDrawOptions options;
        options.tint.a = alpha;
        options.outlineColor.a = alpha;
        options.applyScaleOverride = false;
        return drawObjectImageById(renderer, objectCatalog, intent.iconKey, center, {size, size}, options);
    }
    case autosim::AutoSimulationIntentIconKind::World: {
        const WorldIconDefinition* definition = worldIconDefinitionByKey(intent.iconKey);
        if (definition == nullptr) {
            return false;
        }
        WorldIconDrawOptions options;
        options.tint.a = alpha;
        options.outlineColor.a = alpha;
        options.applyScaleOverride = false;
        return drawWorldIcon(renderer, definition->iconId, center, {size, size}, options);
    }
    case autosim::AutoSimulationIntentIconKind::Chest: {
        WorldIconDrawOptions options;
        options.tint.a = alpha;
        options.outlineColor.a = alpha;
        options.applyScaleOverride = false;
        return drawWorldIcon(renderer, WorldIconId::Chest, center, {size, size}, options);
    }
    case autosim::AutoSimulationIntentIconKind::Warp: {
        WorldIconDrawOptions options;
        options.tint.a = alpha;
        options.outlineColor.a = alpha;
        options.applyScaleOverride = false;
        return drawWorldIcon(renderer, WorldIconId::WarpPoint, center, {size, size}, options);
    }
    case autosim::AutoSimulationIntentIconKind::Enemy:
        renderer.fillCircle(center, size * 0.36f, {206, 62, 78, alpha});
        renderer.drawCircle(center, size * 0.40f, {255, 214, 176, alpha});
        return true;
    case autosim::AutoSimulationIntentIconKind::Dig:
        renderer.fillRect(center - Vec2{size * 0.32f, size * 0.24f}, {size * 0.64f, size * 0.48f}, {98, 80, 62, alpha});
        renderer.drawRect(center - Vec2{size * 0.32f, size * 0.24f}, {size * 0.64f, size * 0.48f}, {235, 220, 184, alpha});
        return true;
    case autosim::AutoSimulationIntentIconKind::Path:
        renderer.drawSoftLine(center - Vec2{size * 0.34f, 0.0f}, center + Vec2{size * 0.28f, 0.0f}, 3.0f, {126, 214, 232, alpha});
        renderer.drawLine(center + Vec2{size * 0.28f, 0.0f}, center + Vec2{size * 0.08f, -size * 0.16f}, {230, 250, 255, alpha});
        renderer.drawLine(center + Vec2{size * 0.28f, 0.0f}, center + Vec2{size * 0.08f, size * 0.16f}, {230, 250, 255, alpha});
        return true;
    case autosim::AutoSimulationIntentIconKind::Base: {
        const Vec2 roof[] = {
            center + Vec2{-size * 0.38f, -size * 0.02f},
            center + Vec2{0.0f, -size * 0.34f},
            center + Vec2{size * 0.38f, -size * 0.02f},
        };
        renderer.fillPolygon(roof, 3, {236, 180, 92, alpha});
        renderer.fillRect(center + Vec2{-size * 0.26f, -size * 0.02f}, {size * 0.52f, size * 0.34f}, {84, 122, 176, alpha});
        return true;
    }
    case autosim::AutoSimulationIntentIconKind::None:
        break;
    }
    return false;
}

struct AutoSimulationIntentLineLayout {
    std::string prefix;
    std::string subjectText;
    float width = 0.0f;
    bool showIcon = false;
};

AutoSimulationIntentLineLayout makeAutoSimulationIntentLineLayout(
    Renderer& renderer,
    const autosim::AutoSimulationIntent& intent,
    float maxWidth,
    int textScale,
    float iconSize,
    float iconGap)
{
    AutoSimulationIntentLineLayout layout;
    layout.prefix = intent.prefix;
    layout.subjectText = autoSimulationIntentSubjectText(intent);
    layout.showIcon = autoSimulationIntentHasIcon(intent);

    const float prefixWidth = renderer.measureText(layout.prefix, textScale).x;
    const float fixedWidth = prefixWidth + (layout.showIcon ? iconSize + iconGap : 0.0f);
    const float subjectMaxWidth = std::max(0.0f, maxWidth - fixedWidth);
    layout.subjectText = fittedSingleLineText(renderer, layout.subjectText, subjectMaxWidth, textScale);
    layout.width = fixedWidth + renderer.measureText(layout.subjectText, textScale).x;
    return layout;
}

void drawAutoSimulationIntentLine(
    Renderer& renderer,
    const ObjectCatalog& objectCatalog,
    const autosim::AutoSimulationIntent& intent,
    const AutoSimulationIntentLineLayout& layout,
    Vec2 pos,
    int textScale,
    float iconSize,
    float iconGap,
    Color textColor,
    Color outlineColor)
{
    Vec2 cursor = pos;
    if (!layout.prefix.empty()) {
        renderer.drawOutlinedText(cursor, layout.prefix, textColor, outlineColor, 2, textScale);
        cursor.x += renderer.measureText(layout.prefix, textScale).x;
    }
    if (layout.showIcon) {
        const Vec2 textMeasure = renderer.measureText("0", textScale);
        const Vec2 center{
            cursor.x + iconSize * 0.5f,
            pos.y + std::max(0.0f, (textMeasure.y - iconSize) * 0.5f) + iconSize * 0.5f - 2.0f,
        };
        if (drawAutoSimulationIntentIcon(renderer, objectCatalog, intent, center, iconSize, textColor.a)) {
            cursor.x += iconSize + iconGap;
        }
    }
    if (!layout.subjectText.empty()) {
        renderer.drawOutlinedText(cursor, layout.subjectText, textColor, outlineColor, 2, textScale);
    }
}

struct DebugItemPickerLayout {
    UiRect panel{};
    UiRect search{};
    UiRect grid{};
    UiRect detail{};
    int columns = 1;
    float rowHeight = DebugItemPickerCardHeight + DebugItemPickerCardGap;
};

struct DebugStoryTestLayout {
    UiRect panel{};
    UiRect list{};
    UiRect detail{};
    float rowPitch = DebugStoryTestRowHeight + DebugStoryTestRowGap;
};

struct DebugNamedSaveLayout {
    UiRect panel{};
    UiRect input{};
    UiRect list{};
    UiRect primaryButton{};
    UiRect secondaryButton{};
    UiRect status{};
    float rowPitch = DebugNamedSaveRowHeight + DebugNamedSaveRowGap;
};

struct EnemyHitboxEditLayout {
    UiRect bounds{};
    UiRect tabs{};
    UiRect listPanel{};
    UiRect search{};
    UiRect list{};
    UiRect previewPanel{};
    UiRect preview{};
    UiRect detail{};
    UiRect footer{};
    float rowPitch = EnemyHitboxRowHeight + EnemyHitboxRowGap;
};

struct DebugPreviewTestLayout {
    UiRect bounds{};
    UiRect listPanel{};
    UiRect tabs{};
    UiRect list{};
    UiRect preview{};
    UiRect footer{};
    UiRect closeButton{};
    int tabRows = 0;
};

struct DebugPreviewListRow {
    std::string label;
    std::string group;
};

struct DebugPreviewBackgroundPreset {
    std::string_view label;
    Color fill;
    Color guideMajor;
    Color guideMinor;
};

struct AudioCueEditLayout {
    UiRect panel{};
    UiRect cueList{};
    UiRect fileList{};
    UiRect detail{};
    float rowPitch = AudioCueEditRowHeight + AudioCueEditRowGap;
};

struct AudioCueManifestRow {
    AudioCueEditEntry entry;
    bool valid = false;
};

constexpr std::array<DebugPreviewBackgroundPreset, 5> DebugPreviewBackgroundPresets{{
    {"暗", {10, 12, 18, 255}, {255, 255, 255, 26}, {255, 255, 255, 18}},
    {"灰", {46, 48, 56, 255}, {255, 255, 255, 34}, {255, 255, 255, 22}},
    {"明", {218, 216, 204, 255}, {38, 42, 52, 42}, {38, 42, 52, 28}},
    {"洞", {36, 29, 24, 255}, {236, 204, 150, 32}, {236, 204, 150, 20}},
    {"青", {16, 30, 46, 255}, {132, 204, 255, 38}, {132, 204, 255, 24}},
}};

UiScrollAreaStyle debugListScrollStyle(float wheelStep = 48.0f)
{
    UiScrollAreaStyle style;
    style.wheelStep = wheelStep;
    style.scrollbarWidth = 8.0f;
    style.scrollbarGap = 8.0f;
    style.scrollbarPaddingX = 6.0f;
    style.scrollbarPaddingY = 8.0f;
    style.scrollbarMinThumbHeight = 28.0f;
    style.scrollbarTrack = {255, 255, 255, 42};
    style.scrollbarThumb = {255, 230, 150, 190};
    style.outline = {126, 138, 168, 220};
    return style;
}

UiScrollableListStyle audioCueEditListStyle()
{
    UiScrollableListStyle style;
    style.rowHeight = AudioCueEditRowHeight;
    style.rowGap = AudioCueEditRowGap;
    style.rowInsetX = 8.0f;
    style.scroll = debugListScrollStyle(36.0f);
    return style;
}

UiScrollableListStyle debugStoryTestListStyle()
{
    UiScrollableListStyle style;
    style.rowHeight = DebugStoryTestRowHeight;
    style.rowGap = DebugStoryTestRowGap;
    style.leadingPadding = 8.0f;
    style.trailingPadding = 8.0f;
    style.rowInsetX = 8.0f;
    style.scroll = debugListScrollStyle(DebugStoryTestRowHeight + DebugStoryTestRowGap);
    return style;
}

UiScrollableListStyle debugPreviewTestListStyle()
{
    UiScrollableListStyle style;
    style.rowHeight = DebugPreviewTestRowHeight;
    style.rowGap = DebugPreviewTestRowGap;
    style.leadingPadding = 8.0f;
    style.trailingPadding = 8.0f;
    style.rowInsetX = 8.0f;
    style.scroll = debugListScrollStyle(DebugPreviewTestRowHeight + DebugPreviewTestRowGap);
    return style;
}

UiRect audioCueEditListViewport(UiRect listPanel)
{
    constexpr float HeaderHeight = 36.0f;
    constexpr float BottomPadding = 8.0f;
    return {{
        listPanel.pos.x,
        listPanel.pos.y + HeaderHeight,
    }, {
        listPanel.size.x,
        std::max(1.0f, listPanel.size.y - HeaderHeight - BottomPadding),
    }};
}

int debugPreviewTestTabColumnCount(int tabCount)
{
    if (tabCount <= 0) {
        return 0;
    }
    return std::min(3, std::max(1, tabCount));
}

int debugPreviewTestTabRowCount(int tabCount)
{
    const int columns = debugPreviewTestTabColumnCount(tabCount);
    if (columns <= 0) {
        return 0;
    }
    return (tabCount + columns - 1) / columns;
}

float debugPreviewTestTabsHeight(int tabCount)
{
    const int rows = debugPreviewTestTabRowCount(tabCount);
    if (rows <= 0) {
        return 0.0f;
    }
    return static_cast<float>(rows) * ui::ButtonHeight +
        static_cast<float>(std::max(0, rows - 1)) * DebugPreviewTestTabRowGap +
        DebugPreviewTestTabRowGap;
}

DebugPreviewTestLayout makeDebugPreviewTestLayout(int screenWidth, int screenHeight, int tabCount = 0)
{
    DebugPreviewTestLayout layout;
    layout.bounds = {{0.0f, 0.0f}, {static_cast<float>(std::max(1, screenWidth)), static_cast<float>(std::max(1, screenHeight))}};
    const float footerTop = std::max(DebugPreviewTestPanelMargin, layout.bounds.size.y - DebugPreviewTestFooterHeight - DebugPreviewTestPanelMargin);
    layout.footer = {{
        DebugPreviewTestPanelMargin,
        footerTop,
    }, {
        std::max(1.0f, layout.bounds.size.x - DebugPreviewTestPanelMargin * 2.0f),
        DebugPreviewTestFooterHeight,
    }};
    const float contentHeight = std::max(120.0f, footerTop - DebugPreviewTestPanelMargin - DebugPreviewTestPanelGap);
    layout.listPanel = {{
        DebugPreviewTestPanelMargin,
        DebugPreviewTestPanelMargin,
    }, {
        std::min(DebugPreviewTestListWidth, std::max(220.0f, layout.bounds.size.x * 0.38f)),
        contentHeight,
    }};
    layout.preview = {{
        layout.listPanel.pos.x + layout.listPanel.size.x + DebugPreviewTestPanelGap,
        DebugPreviewTestPanelMargin,
    }, {
        std::max(160.0f, layout.bounds.size.x - layout.listPanel.size.x - DebugPreviewTestPanelMargin * 2.0f - DebugPreviewTestPanelGap),
        contentHeight,
    }};
    layout.tabRows = debugPreviewTestTabRowCount(tabCount);
    const float tabsHeight = debugPreviewTestTabsHeight(tabCount);
    layout.tabs = {{
        layout.listPanel.pos.x + 6.0f,
        layout.listPanel.pos.y + DebugPreviewTestHeaderHeight,
    }, {
        std::max(1.0f, layout.listPanel.size.x - 12.0f),
        tabsHeight,
    }};
    layout.list = {{
        layout.listPanel.pos.x,
        layout.listPanel.pos.y + DebugPreviewTestHeaderHeight + tabsHeight,
    }, {
        layout.listPanel.size.x,
        std::max(1.0f, layout.listPanel.size.y - DebugPreviewTestHeaderHeight - tabsHeight - 8.0f),
    }};
    layout.closeButton = {{
        layout.footer.pos.x + layout.footer.size.x - 142.0f,
        layout.footer.pos.y + 13.0f,
    }, {
        124.0f,
        ui::ButtonHeight,
    }};
    return layout;
}

int normalizedDebugPreviewBackgroundIndex(int index)
{
    return std::clamp(index, 0, static_cast<int>(DebugPreviewBackgroundPresets.size()) - 1);
}

float debugPreviewBackgroundControlsWidth()
{
    return DebugPreviewBackgroundLabelWidth +
        static_cast<float>(DebugPreviewBackgroundPresets.size()) * DebugPreviewBackgroundSwatchSize +
        static_cast<float>(DebugPreviewBackgroundPresets.size() - 1) * DebugPreviewBackgroundSwatchGap;
}

UiRect debugPreviewBackgroundLabelRect(const DebugPreviewTestLayout& layout)
{
    const float controlsWidth = debugPreviewBackgroundControlsWidth();
    return {{
        layout.closeButton.pos.x - 20.0f - controlsWidth,
        layout.footer.pos.y + 20.0f,
    }, {
        DebugPreviewBackgroundLabelWidth,
        20.0f,
    }};
}

UiRect debugPreviewBackgroundSwatchRect(const DebugPreviewTestLayout& layout, int index)
{
    const UiRect label = debugPreviewBackgroundLabelRect(layout);
    const float x = label.pos.x + label.size.x +
        static_cast<float>(index) * (DebugPreviewBackgroundSwatchSize + DebugPreviewBackgroundSwatchGap);
    return {{
        x,
        layout.footer.pos.y + 17.0f,
    }, {
        DebugPreviewBackgroundSwatchSize,
        DebugPreviewBackgroundSwatchSize,
    }};
}

Vec2 effectTestTargetPosition(const DebugPreviewTestLayout& layout, EffectPreviewTarget target)
{
    const Vec2 center{
        layout.preview.pos.x + layout.preview.size.x * 0.52f,
        layout.preview.pos.y + layout.preview.size.y * 0.56f,
    };
    if (target == EffectPreviewTarget::Player) {
        return center + Vec2{0.0f, 42.0f};
    }
    return center;
}

std::string effectTestTargetLabel(EffectPreviewTarget target)
{
    switch (target) {
    case EffectPreviewTarget::Player:
        return "プレイヤー";
    case EffectPreviewTarget::EnemySlime:
        return "敵: スライム";
    case EffectPreviewTarget::WallTile:
        return "壁: タイル1";
    }
    return "対象";
}

std::string effectTestPlaybackLabel(EffectPreviewPlayback playback)
{
    switch (playback) {
    case EffectPreviewPlayback::BurstEvery20Frames:
        return "40フレーム間隔";
    case EffectPreviewPlayback::PersistentEmitter:
        return "常駐再生";
    case EffectPreviewPlayback::StatusLoop:
        return "状態異常ループ";
    }
    return "再生";
}

Vec2 projectileTestSourcePosition(const DebugPreviewTestLayout& layout)
{
    return {
        layout.preview.pos.x + std::min(160.0f, layout.preview.size.x * 0.24f),
        layout.preview.pos.y + layout.preview.size.y * 0.58f,
    };
}

Vec2 projectileTestFireOrigin(const DebugPreviewTestLayout& layout)
{
    return projectileTestSourcePosition(layout) + Vec2{32.0f, -4.0f};
}

Vec2 projectileTestTargetPosition(const DebugPreviewTestLayout& layout, const ProjectileDefinition* projectile)
{
    const Vec2 origin = projectileTestFireOrigin(layout);
    const float range = projectile != nullptr
        ? projectile->speed * projectile->lifetime
        : 260.0f;
    const float preferredDistance = std::clamp(range * 0.58f, 44.0f, 260.0f);
    const float previewRight = layout.preview.pos.x + layout.preview.size.x - 42.0f;
    const float minX = std::min(previewRight, origin.x + 42.0f);
    return {
        std::clamp(origin.x + preferredDistance, minX, std::max(minX, previewRight)),
        projectileTestSourcePosition(layout).y,
    };
}

int previewSecondsToFrames(float seconds)
{
    return static_cast<int>(std::ceil(std::max(0.0f, seconds) * DebugPreviewAssumedFrameRate));
}

float projectileTestReplaySeconds(
    const DebugPreviewTestLayout& layout,
    const ProjectileDefinition& projectile,
    bool targetEnabled)
{
    float activeSeconds = std::max(0.05f, projectile.lifetime);
    if (targetEnabled && !projectile.piercesTargets && projectile.speed > 0.0f) {
        const Vec2 origin = projectileTestFireOrigin(layout);
        const Vec2 target = projectileTestTargetPosition(layout, &projectile);
        const float travelSeconds =
            length(target - origin) / std::max(1.0f, projectile.speed);
        activeSeconds = std::min(activeSeconds, std::max(0.05f, travelSeconds) + 0.45f);
    }
    return std::max(
        DebugProjectilePreviewReplayGapSeconds,
        activeSeconds + DebugProjectilePreviewReplayGapSeconds);
}

int projectileTestReplayFrames(
    const DebugPreviewTestLayout& layout,
    const ProjectileDefinition& projectile,
    bool targetEnabled)
{
    return std::max(
        DebugProjectilePreviewReplayGapFrames,
        previewSecondsToFrames(projectileTestReplaySeconds(layout, projectile, targetEnabled)));
}

UiRect projectileTestTargetToggleRect(const DebugPreviewTestLayout& layout)
{
    const float width = 142.0f;
    const UiRect backgroundLabel = debugPreviewBackgroundLabelRect(layout);
    const float maxX = backgroundLabel.pos.x - width - 12.0f;
    return {{
        std::max(layout.footer.pos.x + 18.0f, maxX),
        layout.footer.pos.y + 13.0f,
    }, {width, ui::ButtonHeight}};
}

std::string projectileTestDefaultStatus(bool targetEnabled)
{
    return targetEnabled
        ? "ターゲットON: ヒット時エフェクトを確認できます"
        : "ターゲットOFF: 寿命で消滅エフェクトを確認できます";
}

float projectileTestSlimeRadius(const EnemyCatalog& enemyCatalog, float fallbackRadius)
{
    const auto slimeIt = enemyCatalog.enemiesById.find(std::string(DebugPreviewTestSlimeEnemyId));
    if (slimeIt != enemyCatalog.enemiesById.end() && slimeIt->second.radius > 0.0) {
        return static_cast<float>(slimeIt->second.radius);
    }
    return fallbackRadius;
}

void drawProjectileTestSlime(
    Renderer& renderer,
    const EnemyCatalog& enemyCatalog,
    Vec2 position,
    float fallbackRadius,
    float facingAngle,
    double totalSeconds,
    Color fallbackFill)
{
    Enemy previewEnemy;
    const auto slimeIt = enemyCatalog.enemiesById.find(std::string(DebugPreviewTestSlimeEnemyId));
    if (slimeIt != enemyCatalog.enemiesById.end()) {
        const EnemyDefinition& definition = slimeIt->second;
        previewEnemy.active = true;
        previewEnemy.definition = &definition;
        previewEnemy.enemyId = definition.id;
        previewEnemy.enemyName = definition.name;
        previewEnemy.radius = definition.radius > 0.0 ? static_cast<float>(definition.radius) : fallbackRadius;
        previewEnemy.position = position;
        previewEnemy.facingAngle = facingAngle;
        previewEnemy.behaviorTimer = static_cast<float>(totalSeconds);

        Vec2 drawSize{};
        const bool sizeResolved = enemyImageDrawSize(renderer, previewEnemy, EnemyImageDrawOptions{}, drawSize);
        renderer.drawActorShadow(position, sizeResolved ? drawSize.y : previewEnemy.radius * 2.0f);
        if (!drawEnemyImage(renderer, previewEnemy, position, static_cast<float>(totalSeconds), EnemyImageDrawOptions{}, &drawSize)) {
            renderer.fillCircle(position, previewEnemy.radius, fallbackFill);
            renderer.drawCircle(position, previewEnemy.radius + 3.0f, {42, 72, 48, 255});
        }
        return;
    }

    renderer.drawActorShadow(position, fallbackRadius * 1.9f);
    renderer.fillCircle(position, fallbackRadius, fallbackFill);
    renderer.drawCircle(position, fallbackRadius + 3.0f, {42, 72, 48, 255});
}

std::string projectileTagsText(const ProjectileDefinition& projectile)
{
    std::string text;
    for (const std::string& tag : projectile.tags) {
        if (!text.empty()) {
            text += ",";
        }
        text += tag;
    }
    return text.empty() ? "-" : text;
}

std::string_view projectileDisplayName(const ProjectileDefinition& projectile)
{
    return projectile.displayName.empty()
        ? std::string_view(projectile.id.data(), projectile.id.size())
        : std::string_view(projectile.displayName.data(), projectile.displayName.size());
}

std::string_view projectileDamageTypeLabel(std::string_view damageType)
{
    if (damageType == "blunt") {
        return "打撃";
    }
    if (damageType == "pierce") {
        return "刺突";
    }
    if (damageType == "fire") {
        return "火";
    }
    if (damageType == "water") {
        return "水";
    }
    if (damageType == "earth") {
        return "土";
    }
    if (damageType == "wind") {
        return "風";
    }
    if (damageType == "magic") {
        return "魔法";
    }
    if (damageType == "none") {
        return "なし";
    }
    return damageType;
}

std::string projectileTestRowGroup(const ProjectileDefinition& projectile)
{
    return "属性 " + std::string(projectileDamageTypeLabel(projectile.damageType)) + " / ダメージ " + std::to_string(projectile.damage);
}

std::string projectileTestDetail(const ProjectileDefinition& projectile, int replayFrames)
{
    return "ProjectileSystem / " + projectile.id + " / " + std::to_string(replayFrames) + "フレーム間隔 / speed " + std::to_string(static_cast<int>(projectile.speed)) +
        " / radius " + std::to_string(static_cast<int>(std::round(projectile.radius))) +
        " / life " + std::to_string(static_cast<int>(std::round(projectile.lifetime * 100.0f))) + "cs" +
        " / target " + std::string(projectile.piercesTargets ? "貫通" : "停止") +
        " / tags " + projectileTagsText(projectile);
}

const EffectPreviewEntry* effectTestEntryAt(const std::vector<const EffectPreviewEntry*>& entries, int index)
{
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        return nullptr;
    }
    return entries[static_cast<std::size_t>(index)];
}

const ProjectileDefinition* projectileTestEntryAt(const std::vector<const ProjectileDefinition*>& entries, int index)
{
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        return nullptr;
    }
    return entries[static_cast<std::size_t>(index)];
}

std::string effectTestTabLabelForGroup(std::string_view group)
{
    if (group == "EffectSystem / 粒子プリセット") {
        return "粒子";
    }
    if (group == "EffectSystem / 高水準API") {
        return "API";
    }
    if (group == "MagicFx / 常駐") {
        return "魔法常駐";
    }
    if (group == "MagicFx / 単発") {
        return "魔法単発";
    }
    if (group == "状態異常") {
        return "状態異常";
    }

    const std::size_t slash = group.find('/');
    if (slash != std::string_view::npos) {
        std::string label(group.substr(slash + 1));
        while (!label.empty() && label.front() == ' ') {
            label.erase(label.begin());
        }
        return label.empty() ? std::string(group) : label;
    }
    return std::string(group);
}

bool effectTestEntryMatchesTab(const EffectPreviewEntry& entry, std::string_view tabKey)
{
    return tabKey.empty() || entry.group == tabKey;
}

std::vector<UiRect> debugPreviewTestTabRects(const DebugPreviewTestLayout& layout, int tabCount)
{
    std::vector<UiRect> rects;
    if (tabCount <= 0 || layout.tabRows <= 0) {
        return rects;
    }

    const int columns = debugPreviewTestTabColumnCount(tabCount);
    const float columnWidth = layout.tabs.size.x / static_cast<float>(std::max(1, columns));
    rects.reserve(static_cast<std::size_t>(tabCount));
    for (int i = 0; i < tabCount; ++i) {
        const int row = i / columns;
        const int column = i % columns;
        rects.push_back(UiRect{{
            layout.tabs.pos.x + static_cast<float>(column) * columnWidth,
            layout.tabs.pos.y + static_cast<float>(row) * (ui::ButtonHeight + DebugPreviewTestTabRowGap),
        }, {
            columnWidth,
            ui::ButtonHeight,
        }});
    }
    return rects;
}

std::vector<UiTabItem> debugPreviewTestTabItems(const std::vector<std::string>& labels)
{
    std::vector<UiTabItem> items;
    items.reserve(labels.size());
    for (const std::string& label : labels) {
        items.push_back(UiTabItem{std::string_view(label.data(), label.size()), true});
    }
    return items;
}

UiTabsStyle debugPreviewTestTabStyle()
{
    UiTabsStyle style;
    style.visualGap = 4.0f;
    style.imageOutset = 8.0f;
    style.activeScale = 1.0f;
    return style;
}

int updateDebugPreviewTestTabs(
    const Input& input,
    UiContext& ui,
    const DebugPreviewTestLayout& layout,
    const std::vector<std::string>& labels,
    UiTabsState& state,
    int selectedIndex)
{
    if (labels.empty()) {
        state.focusedIndex = -1;
        return -1;
    }

    const std::vector<UiTabItem> items = debugPreviewTestTabItems(labels);
    const std::vector<UiRect> rects = debugPreviewTestTabRects(layout, static_cast<int>(items.size()));
    UiTabsInput tabsInput{};
    tabsInput.focusDelta =
        (input.pressed(InputAction::MoveRight) ? 1 : 0) -
        (input.pressed(InputAction::MoveLeft) ? 1 : 0);
    tabsInput.commit = tabsInput.focusDelta != 0;
    return updateUiTabs(
        state,
        ui,
        tabsInput,
        selectedIndex,
        items.data(),
        static_cast<int>(items.size()),
        rects.data(),
        debugPreviewTestTabStyle());
}

void renderDebugPreviewTestTabs(
    Renderer& renderer,
    const DebugPreviewTestLayout& layout,
    const std::vector<std::string>& labels,
    const UiTabsState& state,
    int selectedIndex)
{
    if (labels.empty()) {
        return;
    }

    const std::vector<UiTabItem> items = debugPreviewTestTabItems(labels);
    const std::vector<UiRect> rects = debugPreviewTestTabRects(layout, static_cast<int>(items.size()));
    drawUiTabs(
        renderer,
        state,
        selectedIndex,
        items.data(),
        static_cast<int>(items.size()),
        rects.data(),
        debugPreviewTestTabStyle());
}

bool updateDebugPreviewBackgroundControls(UiContext& ui, const DebugPreviewTestLayout& layout, int& backgroundIndex)
{
    bool changed = false;
    for (int i = 0; i < static_cast<int>(DebugPreviewBackgroundPresets.size()); ++i) {
        if (!ui.pressed(debugPreviewBackgroundSwatchRect(layout, i))) {
            continue;
        }
        if (backgroundIndex != i) {
            backgroundIndex = i;
            ui.emitSound(UiSoundEvent::TabSwitch);
            changed = true;
        }
        break;
    }
    backgroundIndex = normalizedDebugPreviewBackgroundIndex(backgroundIndex);
    return changed;
}

void drawDebugPreviewBackgroundControls(Renderer& renderer, const DebugPreviewTestLayout& layout, int backgroundIndex)
{
    const int selectedIndex = normalizedDebugPreviewBackgroundIndex(backgroundIndex);
    const UiRect labelRect = debugPreviewBackgroundLabelRect(layout);
    renderer.drawText(labelRect.pos, "背景", ui::TextMuted, 2);

    for (int i = 0; i < static_cast<int>(DebugPreviewBackgroundPresets.size()); ++i) {
        const UiRect rect = debugPreviewBackgroundSwatchRect(layout, i);
        const bool selected = i == selectedIndex;
        const DebugPreviewBackgroundPreset& preset = DebugPreviewBackgroundPresets[static_cast<std::size_t>(i)];
        renderer.fillRect(rect.pos, rect.size, preset.fill);
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 230, 150, 255} : Color{142, 154, 182, 210});
        if (selected) {
            renderer.drawRect(rect.pos - Vec2{2.0f, 2.0f}, rect.size + Vec2{4.0f, 4.0f}, {255, 246, 190, 230});
        }
        const Vec2 textSize = renderer.measureText(preset.label, 1);
        renderer.drawText(
            rect.pos + Vec2{
                std::max(0.0f, (rect.size.x - textSize.x) * 0.5f),
                rect.size.y + 4.0f,
            },
            preset.label,
            selected ? Color{255, 246, 190, 255} : ui::TextMuted,
            1);
    }
}

UiScrollAreaLayout updateDebugPreviewTestList(
    const Input& input,
    UiContext& ui,
    const DebugPreviewTestLayout& layout,
    int itemCount,
    int& selectedIndex,
    float& scrollOffset,
    UiScrollAreaState& scrollState,
    bool& selectionChanged)
{
    const int previousSelection = selectedIndex;
    if (itemCount > 0) {
        selectedIndex = std::clamp(selectedIndex, 0, itemCount - 1);
    } else {
        selectedIndex = 0;
    }

    const UiScrollableListStyle listStyle = debugPreviewTestListStyle();
    UiScrollAreaLayout listLayout = updateUiScrollableList(
        ui,
        input,
        layout.list,
        itemCount,
        scrollOffset,
        listStyle,
        &scrollState);

    if (input.pressed(InputAction::MoveUp) && itemCount > 0) {
        selectedIndex = (selectedIndex + itemCount - 1) % itemCount;
    }
    if (input.pressed(InputAction::MoveDown) && itemCount > 0) {
        selectedIndex = (selectedIndex + 1) % itemCount;
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect row = uiScrollableListItemRect(listLayout, i, listStyle);
        if (!uiScrollAreaRectVisible(listLayout, row)) {
            continue;
        }
        if (ui.pressed(row)) {
            selectedIndex = i;
            break;
        }
    }

    selectionChanged = selectedIndex != previousSelection;
    if (selectionChanged) {
        keepUiScrollableListItemVisible(layout.list, selectedIndex, itemCount, scrollOffset, listStyle);
        listLayout = makeUiScrollableListLayout(layout.list, itemCount, scrollOffset, listStyle);
    }
    return listLayout;
}

template <typename RowGetter>
void renderDebugPreviewTestList(
    Renderer& renderer,
    const DebugPreviewTestLayout& layout,
    std::string_view title,
    int itemCount,
    int selectedIndex,
    float scrollOffset,
    RowGetter rowGetter)
{
    renderer.drawText(layout.listPanel.pos + Vec2{14.0f, 12.0f}, title, {255, 230, 150, 255}, 2);

    const UiScrollableListStyle listStyle = debugPreviewTestListStyle();
    const UiScrollAreaLayout listLayout = makeUiScrollableListLayout(
        layout.list,
        itemCount,
        scrollOffset,
        listStyle);

    renderer.pushClipRect(listLayout.viewport.pos, listLayout.viewport.size);
    for (int i = 0; i < itemCount; ++i) {
        const UiRect row = uiScrollableListItemRect(listLayout, i, listStyle);
        if (!uiScrollAreaRectVisible(listLayout, row)) {
            continue;
        }
        const DebugPreviewListRow rowText = rowGetter(i);
        const bool selected = i == selectedIndex;
        renderer.fillRect(row.pos, row.size, selected ? Color{58, 72, 104, 238} : Color{18, 26, 44, 210});
        renderer.drawRect(row.pos, row.size, selected ? Color{255, 230, 150, 230} : Color{92, 104, 132, 180});
        renderer.drawText(row.pos + Vec2{10.0f, 7.0f}, fittedSingleLineText(renderer, rowText.label, row.size.x - 20.0f, 2), ui::Text, 2);
        renderer.drawText(row.pos + Vec2{10.0f, 29.0f}, fittedSingleLineText(renderer, rowText.group, row.size.x - 20.0f, 1), ui::TextMuted, 1);
    }
    renderer.popClipRect();
    drawUiScrollAreaFrame(renderer, listLayout, listStyle.scroll);
}

void drawDebugPreviewBackground(Renderer& renderer, const DebugPreviewTestLayout& layout)
{
    renderer.fillRect(layout.bounds.pos, layout.bounds.size, {5, 7, 12, 255});
    drawUiSubPanel(renderer, layout.listPanel);
    drawUiSubPanel(renderer, layout.preview);
    drawUiSubPanel(renderer, layout.footer);
}

void drawDebugPreviewGuide(Renderer& renderer, const DebugPreviewTestLayout& layout, int backgroundIndex)
{
    const DebugPreviewBackgroundPreset& preset =
        DebugPreviewBackgroundPresets[static_cast<std::size_t>(normalizedDebugPreviewBackgroundIndex(backgroundIndex))];
    renderer.fillRect(layout.preview.pos, layout.preview.size, preset.fill);
    const Vec2 previewCenter{
        layout.preview.pos.x + layout.preview.size.x * 0.5f,
        layout.preview.pos.y + layout.preview.size.y * 0.5f,
    };
    renderer.drawLine({layout.preview.pos.x + 20.0f, previewCenter.y}, {layout.preview.pos.x + layout.preview.size.x - 20.0f, previewCenter.y}, preset.guideMajor);
    renderer.drawLine({previewCenter.x, layout.preview.pos.y + 20.0f}, {previewCenter.x, layout.preview.pos.y + layout.preview.size.y - 20.0f}, preset.guideMinor);
}

void drawDebugPreviewFooter(
    Renderer& renderer,
    const DebugPreviewTestLayout& layout,
    std::string_view label,
    const std::string& detail,
    std::string_view status,
    int backgroundIndex,
    float detailRightLimit = -1.0f)
{
    if (!label.empty()) {
        const float textRight = detailRightLimit > 0.0f
            ? std::min(detailRightLimit, debugPreviewBackgroundLabelRect(layout).pos.x)
            : debugPreviewBackgroundLabelRect(layout).pos.x;
        const float textMaxWidth = std::max(120.0f, textRight - layout.footer.pos.x - 36.0f);
        renderer.drawText(layout.footer.pos + Vec2{18.0f, 13.0f}, fittedSingleLineText(renderer, std::string(label), textMaxWidth, 2), {255, 230, 150, 255}, 2);
        renderer.drawText(layout.footer.pos + Vec2{18.0f, 38.0f}, fittedSingleLineText(renderer, detail, textMaxWidth, 1), ui::TextMuted, 1);
    }
    if (!status.empty()) {
        renderer.drawText(layout.preview.pos + Vec2{16.0f, 14.0f}, fittedSingleLineText(renderer, std::string(status), layout.preview.size.x - 32.0f, 2), ui::TextMuted, 2);
    }
    drawDebugPreviewBackgroundControls(renderer, layout, backgroundIndex);
    drawUiRectButton(renderer, layout.closeButton, "終了", false, uiCancelButtonStyle());
}

void drawProjectileTestTargetToggle(Renderer& renderer, const DebugPreviewTestLayout& layout, bool enabled)
{
    UiButtonStyle style = enabled ? uiActionButtonStyle() : UiButtonStyle{};
    if (enabled) {
        style.fill = {54, 88, 72, 224};
        style.fillHot = {70, 116, 92, 238};
        style.outline = {190, 244, 186, 235};
        style.outlineHot = {226, 255, 210, 255};
    }
    drawUiRectButton(renderer, projectileTestTargetToggleRect(layout), enabled ? "ターゲット ON" : "ターゲット OFF", enabled, style);
}

DebugItemPickerLayout makeDebugItemPickerLayout(int screenWidth, int screenHeight)
{
    const float width = std::min(
        DebugItemPickerPanelMaxWidth,
        std::max(760.0f, static_cast<float>(screenWidth) - DebugItemPickerPanelMargin * 2.0f));
    const float height = std::min(
        DebugItemPickerPanelMaxHeight,
        std::max(520.0f, static_cast<float>(screenHeight) - DebugItemPickerPanelMargin * 1.5f));

    DebugItemPickerLayout layout;
    layout.panel = {{
        (static_cast<float>(screenWidth) - width) * 0.5f,
        (static_cast<float>(screenHeight) - height) * 0.5f,
    }, {width, height}};

    const UiRect body = uiBodyRect(layout.panel);
    const float contentHeight = std::max(80.0f, body.size.y - DebugItemPickerButtonAreaHeight);
    layout.detail = {{
        body.pos.x + body.size.x - DebugItemPickerDetailWidth,
        body.pos.y,
    }, {DebugItemPickerDetailWidth, contentHeight}};
    const UiRect listArea{{
        body.pos.x,
        body.pos.y,
    }, {
        std::max(120.0f, layout.detail.pos.x - body.pos.x - DebugItemPickerPaneGap),
        contentHeight,
    }};
    layout.search = {listArea.pos, {listArea.size.x, DebugItemPickerSearchHeight}};
    layout.grid = {{
        listArea.pos.x,
        listArea.pos.y + DebugItemPickerSearchHeight + DebugItemPickerSearchGap,
    }, {
        listArea.size.x,
        std::max(1.0f, listArea.size.y - DebugItemPickerSearchHeight - DebugItemPickerSearchGap),
    }};

    const float pitch = DebugItemPickerCardWidth + DebugItemPickerCardGap;
    layout.columns = std::max(1, static_cast<int>((layout.grid.size.x + DebugItemPickerCardGap) / pitch));
    return layout;
}

DebugStoryTestLayout makeDebugStoryTestLayout(int screenWidth, int screenHeight)
{
    const float width = std::min(
        DebugItemPickerPanelMaxWidth,
        std::max(760.0f, static_cast<float>(screenWidth) - DebugItemPickerPanelMargin * 2.0f));
    const float height = std::min(
        DebugItemPickerPanelMaxHeight,
        std::max(520.0f, static_cast<float>(screenHeight) - DebugItemPickerPanelMargin * 1.5f));

    DebugStoryTestLayout layout;
    layout.panel = {{
        (static_cast<float>(screenWidth) - width) * 0.5f,
        (static_cast<float>(screenHeight) - height) * 0.5f,
    }, {width, height}};

    const UiRect body = uiBodyRect(layout.panel);
    const float contentHeight = std::max(80.0f, body.size.y - DebugItemPickerButtonAreaHeight);
    layout.detail = {{
        body.pos.x + body.size.x - DebugStoryTestDetailWidth,
        body.pos.y,
    }, {DebugStoryTestDetailWidth, contentHeight}};
    layout.list = {{
        body.pos.x,
        body.pos.y,
    }, {
        std::max(220.0f, layout.detail.pos.x - body.pos.x - DebugItemPickerPaneGap),
        contentHeight,
    }};
    return layout;
}

DebugNamedSaveLayout makeDebugNamedSaveInputLayout(int screenWidth, int screenHeight)
{
    DebugNamedSaveLayout layout;
    const float width = std::min(DebugNamedSavePanelWidth, std::max(360.0f, static_cast<float>(screenWidth) - 48.0f));
    const float height = std::min(DebugNamedSaveInputPanelHeight, std::max(220.0f, static_cast<float>(screenHeight) - 48.0f));
    layout.panel = {{
        std::max(0.0f, (static_cast<float>(screenWidth) - width) * 0.5f),
        std::max(0.0f, (static_cast<float>(screenHeight) - height) * 0.5f),
    }, {width, height}};
    layout.input = {{layout.panel.pos.x + 36.0f, layout.panel.pos.y + 98.0f}, {layout.panel.size.x - 72.0f, 44.0f}};
    layout.status = {{layout.panel.pos.x + 36.0f, layout.panel.pos.y + 150.0f}, {layout.panel.size.x - 72.0f, 28.0f}};
    layout.primaryButton = {{layout.panel.pos.x + layout.panel.size.x - 200.0f, layout.panel.pos.y + layout.panel.size.y - 72.0f}, {156.0f, ui::ButtonHeight}};
    layout.secondaryButton = {{layout.panel.pos.x + 44.0f, layout.panel.pos.y + layout.panel.size.y - 72.0f}, {156.0f, ui::ButtonHeight}};
    return layout;
}

DebugNamedSaveLayout makeDebugNamedSaveLoadLayout(int screenWidth, int screenHeight)
{
    DebugNamedSaveLayout layout;
    const float width = std::min(DebugNamedSavePanelWidth, std::max(360.0f, static_cast<float>(screenWidth) - 48.0f));
    const float height = std::min(DebugNamedSaveLoadPanelHeight, std::max(300.0f, static_cast<float>(screenHeight) - 48.0f));
    layout.panel = {{
        std::max(0.0f, (static_cast<float>(screenWidth) - width) * 0.5f),
        std::max(0.0f, (static_cast<float>(screenHeight) - height) * 0.5f),
    }, {width, height}};
    layout.list = {{layout.panel.pos.x + 32.0f, layout.panel.pos.y + 74.0f}, {layout.panel.size.x - 64.0f, layout.panel.size.y - 174.0f}};
    layout.status = {{layout.panel.pos.x + 32.0f, layout.panel.pos.y + layout.panel.size.y - 92.0f}, {layout.panel.size.x - 64.0f, 28.0f}};
    layout.primaryButton = {{layout.panel.pos.x + layout.panel.size.x - 200.0f, layout.panel.pos.y + layout.panel.size.y - 58.0f}, {156.0f, ui::ButtonHeight}};
    layout.secondaryButton = {{layout.panel.pos.x + 44.0f, layout.panel.pos.y + layout.panel.size.y - 58.0f}, {156.0f, ui::ButtonHeight}};
    return layout;
}

UiRect debugNamedSaveLoadRowRect(const DebugNamedSaveLayout& layout, int index, float scrollOffset)
{
    return {{
        layout.list.pos.x + 10.0f,
        layout.list.pos.y + 10.0f + static_cast<float>(index) * layout.rowPitch - scrollOffset,
    }, {
        std::max(1.0f, layout.list.size.x - 20.0f),
        DebugNamedSaveRowHeight,
    }};
}

float debugNamedSaveLoadContentHeight(const DebugNamedSaveLayout& layout, int itemCount)
{
    if (itemCount <= 0) {
        return layout.list.size.y;
    }
    return 20.0f + static_cast<float>(itemCount) * DebugNamedSaveRowHeight +
        static_cast<float>(std::max(0, itemCount - 1)) * DebugNamedSaveRowGap;
}

float debugNamedSaveLoadMaxScroll(const DebugNamedSaveLayout& layout, int itemCount)
{
    return std::max(0.0f, debugNamedSaveLoadContentHeight(layout, itemCount) - layout.list.size.y);
}

AudioCueEditLayout makeAudioCueEditLayout(int screenWidth, int screenHeight)
{
    const float width = std::min(
        AudioCueEditPanelMaxWidth,
        std::max(900.0f, static_cast<float>(screenWidth) - AudioCueEditPanelMargin * 2.0f));
    const float height = std::min(
        AudioCueEditPanelMaxHeight,
        std::max(540.0f, static_cast<float>(screenHeight) - AudioCueEditPanelMargin * 1.5f));

    AudioCueEditLayout layout;
    layout.panel = {{
        (static_cast<float>(screenWidth) - width) * 0.5f,
        (static_cast<float>(screenHeight) - height) * 0.5f,
    }, {width, height}};

    const UiRect body = uiBodyRect(layout.panel);
    const float contentHeight = std::max(120.0f, body.size.y - AudioCueEditButtonAreaHeight);
    layout.cueList = {body.pos, {AudioCueEditCueListWidth, contentHeight}};
    layout.detail = {{
        body.pos.x + body.size.x - AudioCueEditDetailWidth,
        body.pos.y,
    }, {AudioCueEditDetailWidth, contentHeight}};
    layout.fileList = {{
        layout.cueList.pos.x + layout.cueList.size.x + AudioCueEditPaneGap,
        body.pos.y,
    }, {
        std::max(180.0f, layout.detail.pos.x - (layout.cueList.pos.x + layout.cueList.size.x) - AudioCueEditPaneGap * 2.0f),
        contentHeight,
    }};
    return layout;
}

UiRect debugItemPickerCardRect(const DebugItemPickerLayout& layout, int index, float scrollOffset)
{
    const int columns = std::max(1, layout.columns);
    const int row = index / columns;
    const int column = index % columns;
    return {{
        layout.grid.pos.x + static_cast<float>(column) * (DebugItemPickerCardWidth + DebugItemPickerCardGap),
        layout.grid.pos.y + static_cast<float>(row) * layout.rowHeight - scrollOffset,
    }, {DebugItemPickerCardWidth, DebugItemPickerCardHeight}};
}

float debugItemPickerContentHeight(const DebugItemPickerLayout& layout, int itemCount)
{
    const int columns = std::max(1, layout.columns);
    const int rows = itemCount <= 0 ? 0 : (itemCount + columns - 1) / columns;
    return rows <= 0
        ? 0.0f
        : static_cast<float>(rows) * DebugItemPickerCardHeight +
            static_cast<float>(rows - 1) * DebugItemPickerCardGap;
}

float debugItemPickerMaxScroll(const DebugItemPickerLayout& layout, int itemCount)
{
    const float contentHeight = debugItemPickerContentHeight(layout, itemCount);
    return std::max(0.0f, contentHeight - layout.grid.size.y);
}

UiRect debugItemPickerSearchInputRect(const DebugItemPickerLayout& layout)
{
    constexpr float ClearButtonWidth = 72.0f;
    constexpr float CountWidth = 118.0f;
    constexpr float Gap = 8.0f;
    return {layout.search.pos, {std::max(180.0f, layout.search.size.x - ClearButtonWidth - CountWidth - Gap * 2.0f), layout.search.size.y}};
}

UiRect debugItemPickerSearchClearButtonRect(const DebugItemPickerLayout& layout)
{
    constexpr float ClearButtonWidth = 72.0f;
    constexpr float Gap = 8.0f;
    const UiRect input = debugItemPickerSearchInputRect(layout);
    return {{input.pos.x + input.size.x + Gap, layout.search.pos.y}, {ClearButtonWidth, layout.search.size.y}};
}

UiRect debugItemPickerSearchCountRect(const DebugItemPickerLayout& layout)
{
    constexpr float Gap = 8.0f;
    const UiRect clear = debugItemPickerSearchClearButtonRect(layout);
    const float x = clear.pos.x + clear.size.x + Gap;
    return {{x, layout.search.pos.y}, {std::max(0.0f, layout.search.pos.x + layout.search.size.x - x), layout.search.size.y}};
}

std::filesystem::path hitboxDataPath()
{
    return std::filesystem::path("data") / "hitboxes.cfg";
}

std::filesystem::path legacyEnemyHitboxDataPath()
{
    return std::filesystem::path("data") / "enemy_hitboxes.cfg";
}

std::filesystem::path enemyShadowDataPath()
{
    return std::filesystem::path("data") / "enemy_shadows.cfg";
}

EnemyHitboxEditLayout makeEnemyHitboxEditLayout(int screenWidth, int screenHeight)
{
    const float width = static_cast<float>(screenWidth);
    const float height = static_cast<float>(screenHeight);
    EnemyHitboxEditLayout layout;
    layout.bounds = {{0.0f, 0.0f}, {width, height}};
    layout.tabs = {{22.0f, 44.0f}, {360.0f, 30.0f}};
    layout.footer = {{0.0f, height - EnemyHitboxFooterHeight}, {width, EnemyHitboxFooterHeight}};
    const float top = EnemyHitboxHeaderHeight;
    const float bottom = height - EnemyHitboxFooterHeight - EnemyHitboxPanelMargin;
    const float contentHeight = std::max(1.0f, bottom - top);
    layout.listPanel = {
        {EnemyHitboxPanelMargin, top},
        {EnemyHitboxListWidth, contentHeight},
    };
    layout.search = {
        layout.listPanel.pos + Vec2{12.0f, 12.0f},
        {layout.listPanel.size.x - 24.0f, EnemyHitboxSearchHeight},
    };
    layout.list = {
        {layout.listPanel.pos.x + 12.0f, layout.search.pos.y + layout.search.size.y + 12.0f},
        {layout.listPanel.size.x - 24.0f, std::max(1.0f, layout.listPanel.size.y - EnemyHitboxSearchHeight - 36.0f)},
    };
    layout.previewPanel = {
        {layout.listPanel.pos.x + layout.listPanel.size.x + EnemyHitboxPanelGap, top},
        {std::max(1.0f, width - layout.listPanel.pos.x - layout.listPanel.size.x - EnemyHitboxPanelGap - EnemyHitboxPanelMargin), contentHeight},
    };
    const float detailWidth = std::min(
        EnemyHitboxDetailPreferredWidth,
        std::max(EnemyHitboxDetailMinWidth, layout.previewPanel.size.x - 120.0f));
    layout.detail = {
        {
            layout.previewPanel.pos.x + layout.previewPanel.size.x - detailWidth - EnemyHitboxDetailRightMargin,
            layout.previewPanel.pos.y + 14.0f,
        },
        {detailWidth, layout.previewPanel.size.y - 28.0f},
    };
    layout.preview = {
        {layout.previewPanel.pos.x + 16.0f, layout.previewPanel.pos.y + 16.0f},
        {std::max(1.0f, layout.detail.pos.x - layout.previewPanel.pos.x - 28.0f), layout.previewPanel.size.y - 32.0f},
    };
    return layout;
}

UiRect enemyHitboxSearchInputRect(const EnemyHitboxEditLayout& layout)
{
    constexpr float ClearButtonWidth = 62.0f;
    constexpr float CountWidth = 82.0f;
    constexpr float Gap = 8.0f;
    return {layout.search.pos, {std::max(120.0f, layout.search.size.x - ClearButtonWidth - CountWidth - Gap * 2.0f), layout.search.size.y}};
}

UiRect enemyHitboxSearchClearButtonRect(const EnemyHitboxEditLayout& layout)
{
    constexpr float ClearButtonWidth = 62.0f;
    constexpr float Gap = 8.0f;
    const UiRect input = enemyHitboxSearchInputRect(layout);
    return {{input.pos.x + input.size.x + Gap, layout.search.pos.y}, {ClearButtonWidth, layout.search.size.y}};
}

UiRect enemyHitboxSearchCountRect(const EnemyHitboxEditLayout& layout)
{
    constexpr float Gap = 8.0f;
    const UiRect clear = enemyHitboxSearchClearButtonRect(layout);
    const float x = clear.pos.x + clear.size.x + Gap;
    return {{x, layout.search.pos.y}, {std::max(0.0f, layout.search.pos.x + layout.search.size.x - x), layout.search.size.y}};
}

UiRect enemyHitboxListRowRect(const EnemyHitboxEditLayout& layout, int index, float scrollOffset)
{
    return {
        {layout.list.pos.x, layout.list.pos.y + static_cast<float>(index) * layout.rowPitch - scrollOffset},
        {layout.list.size.x, EnemyHitboxRowHeight},
    };
}

float enemyHitboxListContentHeight(const EnemyHitboxEditLayout& layout, int itemCount)
{
    (void)layout;
    if (itemCount <= 0) {
        return 0.0f;
    }
    return static_cast<float>(itemCount) * EnemyHitboxRowHeight +
        static_cast<float>(itemCount - 1) * EnemyHitboxRowGap;
}

float enemyHitboxMaxScroll(const EnemyHitboxEditLayout& layout, int itemCount)
{
    return std::max(0.0f, enemyHitboxListContentHeight(layout, itemCount) - layout.list.size.y);
}

void keepEnemyHitboxSelectionVisible(
    const EnemyHitboxEditLayout& layout,
    int selectedIndex,
    int itemCount,
    float& scrollOffset)
{
    if (selectedIndex < 0 || selectedIndex >= itemCount) {
        scrollOffset = clamp(scrollOffset, 0.0f, enemyHitboxMaxScroll(layout, itemCount));
        return;
    }

    const UiRect rect = enemyHitboxListRowRect(layout, selectedIndex, scrollOffset);
    if (rect.pos.y < layout.list.pos.y) {
        scrollOffset -= layout.list.pos.y - rect.pos.y;
    } else if (rect.pos.y + rect.size.y > layout.list.pos.y + layout.list.size.y) {
        scrollOffset += rect.pos.y + rect.size.y - (layout.list.pos.y + layout.list.size.y);
    }
    scrollOffset = clamp(scrollOffset, 0.0f, enemyHitboxMaxScroll(layout, itemCount));
}

UiRect enemyHitboxDetailButtonRect(const EnemyHitboxEditLayout& layout, int index)
{
    const float width = (layout.detail.size.x -
        EnemyHitboxButtonGap * static_cast<float>(EnemyHitboxDetailButtonColumns - 1)) /
        static_cast<float>(EnemyHitboxDetailButtonColumns);
    const int row = index / EnemyHitboxDetailButtonColumns;
    const int column = index % EnemyHitboxDetailButtonColumns;
    return {
        {
            layout.detail.pos.x + static_cast<float>(column) * (width + EnemyHitboxButtonGap),
            layout.detail.pos.y + EnemyHitboxDetailButtonTop +
                static_cast<float>(row) * (EnemyHitboxButtonHeight + EnemyHitboxButtonGap),
        },
        {width, EnemyHitboxButtonHeight},
    };
}

UiRect hitboxEditTabRect(const EnemyHitboxEditLayout& layout, int index)
{
    constexpr int Count = 3;
    constexpr float Gap = 8.0f;
    const float width = (layout.tabs.size.x - Gap * static_cast<float>(Count - 1)) / static_cast<float>(Count);
    return {
        {layout.tabs.pos.x + static_cast<float>(index) * (width + Gap), layout.tabs.pos.y},
        {width, layout.tabs.size.y},
    };
}

UiRect enemyHitboxDirectionButtonRect(const EnemyHitboxEditLayout& layout, int index)
{
    constexpr int Count = HitboxDirectionCount;
    constexpr float Gap = 6.0f;
    const float width = (layout.detail.size.x - Gap * static_cast<float>(Count - 1)) / static_cast<float>(Count);
    return {
        {
            layout.detail.pos.x + static_cast<float>(index) * (width + Gap),
            layout.detail.pos.y + EnemyHitboxDirectionButtonTop,
        },
        {width, EnemyHitboxButtonHeight},
    };
}

ObjectImageScaleLayout objectImageScaleGridLayout(ObjectImageScaleLayout layout, bool searchVisible)
{
    if (!searchVisible) {
        return layout;
    }
    const float offsetY = ObjectImageScaleSearchHeight + ObjectImageScaleSearchGap;
    layout.viewport.pos.y += offsetY;
    layout.viewport.size.y = std::max(1.0f, layout.viewport.size.y - offsetY);
    layout.content = layout.viewport;
    return layout;
}

float objectImageScaleContentHeight(const ObjectImageScaleLayout& layout, int itemCount)
{
    const int columns = std::max(1, layout.columns);
    const int rows = itemCount <= 0 ? 0 : (itemCount + columns - 1) / columns;
    return rows <= 0
        ? 0.0f
        : static_cast<float>(rows) * ObjectImageScaleCardHeight + static_cast<float>(rows - 1) * ObjectImageScaleCardGap;
}

float objectImageScaleMaxScroll(const ObjectImageScaleLayout& layout, int itemCount)
{
    return std::max(0.0f, objectImageScaleContentHeight(layout, itemCount) - layout.viewport.size.y);
}

void keepObjectImageScaleSelectionVisible(
    const ObjectImageScaleLayout& layout,
    int selectedIndex,
    int itemCount,
    float& scrollOffset)
{
    if (selectedIndex < 0 || selectedIndex >= itemCount) {
        scrollOffset = clamp(scrollOffset, 0.0f, objectImageScaleMaxScroll(layout, itemCount));
        return;
    }

    const UiRect rect = objectImageScaleCardRect(layout, selectedIndex, scrollOffset);
    const float top = layout.viewport.pos.y;
    const float bottom = layout.viewport.pos.y + layout.viewport.size.y;
    if (rect.pos.y < top) {
        scrollOffset -= top - rect.pos.y;
    } else if (rect.pos.y + rect.size.y > bottom) {
        scrollOffset += rect.pos.y + rect.size.y - bottom;
    }
    scrollOffset = clamp(scrollOffset, 0.0f, objectImageScaleMaxScroll(layout, itemCount));
}

UiRect objectImageScaleSearchInputRect(const ObjectImageScaleLayout& layout)
{
    constexpr float ClearButtonWidth = 72.0f;
    constexpr float CountWidth = 124.0f;
    constexpr float Gap = 8.0f;
    return {layout.viewport.pos, {std::max(220.0f, layout.viewport.size.x - ClearButtonWidth - CountWidth - Gap * 2.0f), ObjectImageScaleSearchHeight}};
}

UiRect objectImageScaleSearchClearButtonRect(const ObjectImageScaleLayout& layout)
{
    constexpr float ClearButtonWidth = 72.0f;
    constexpr float Gap = 8.0f;
    const UiRect input = objectImageScaleSearchInputRect(layout);
    return {{input.pos.x + input.size.x + Gap, layout.viewport.pos.y}, {ClearButtonWidth, ObjectImageScaleSearchHeight}};
}

UiRect objectImageScaleSearchCountRect(const ObjectImageScaleLayout& layout)
{
    constexpr float Gap = 8.0f;
    const UiRect clear = objectImageScaleSearchClearButtonRect(layout);
    const float x = clear.pos.x + clear.size.x + Gap;
    return {{x, layout.viewport.pos.y}, {std::max(0.0f, layout.viewport.pos.x + layout.viewport.size.x - x), ObjectImageScaleSearchHeight}};
}

UiRect audioCueEditCloseButtonRect(UiRect panel)
{
    return uiBottomLeftButtonRect(panel, {130.0f, ui::ButtonHeight});
}

UiRect audioCueEditRescanButtonRect(UiRect panel)
{
    const UiRect close = audioCueEditCloseButtonRect(panel);
    return {{close.pos.x + close.size.x + 12.0f, close.pos.y}, {150.0f, ui::ButtonHeight}};
}

UiRect audioCueEditPreviewButtonRect(UiRect panel)
{
    const UiRect body = uiBodyRect(panel);
    return {{body.pos.x + body.size.x - 404.0f, body.pos.y + body.size.y - ui::ButtonHeight}, {126.0f, ui::ButtonHeight}};
}

UiRect audioCueEditApplyButtonRect(UiRect panel)
{
    const UiRect preview = audioCueEditPreviewButtonRect(panel);
    return {{preview.pos.x + preview.size.x + 12.0f, preview.pos.y}, {126.0f, ui::ButtonHeight}};
}

UiRect audioCueEditSaveButtonRect(UiRect panel)
{
    return uiBottomRightButtonRect(panel, {128.0f, ui::ButtonHeight});
}

void keepDebugItemPickerSelectionVisible(
    const DebugItemPickerLayout& layout,
    int selectedIndex,
    int itemCount,
    float& scrollOffset)
{
    if (selectedIndex < 0 || selectedIndex >= itemCount) {
        scrollOffset = clamp(scrollOffset, 0.0f, debugItemPickerMaxScroll(layout, itemCount));
        return;
    }

    const UiRect rect = debugItemPickerCardRect(layout, selectedIndex, scrollOffset);
    const float top = layout.grid.pos.y;
    const float bottom = layout.grid.pos.y + layout.grid.size.y;
    if (rect.pos.y < top) {
        scrollOffset -= top - rect.pos.y;
    } else if (rect.pos.y + rect.size.y > bottom) {
        scrollOffset += rect.pos.y + rect.size.y - bottom;
    }
    scrollOffset = clamp(scrollOffset, 0.0f, debugItemPickerMaxScroll(layout, itemCount));
}

void keepDebugStoryTestSelectionVisible(
    const DebugStoryTestLayout& layout,
    int selectedIndex,
    int itemCount,
    float& scrollOffset)
{
    keepUiScrollableListItemVisible(
        layout.list,
        selectedIndex,
        itemCount,
        scrollOffset,
        debugStoryTestListStyle());
}

void keepAudioCueEditSelectionVisible(UiRect list, int selectedIndex, int itemCount, float& scrollOffset)
{
    keepUiScrollableListItemVisible(audioCueEditListViewport(list), selectedIndex, itemCount, scrollOffset, audioCueEditListStyle());
}

UiRect debugItemPickerAddButtonRect(UiRect panel)
{
    return uiBottomRightButtonRect(panel, {180.0f, ui::ButtonHeight});
}

UiRect debugItemPickerCloseButtonRect(UiRect panel)
{
    return uiBottomLeftButtonRect(panel, {150.0f, ui::ButtonHeight});
}

UiRect debugStoryTestPlayButtonRect(UiRect panel)
{
    return debugItemPickerAddButtonRect(panel);
}

UiRect debugStoryTestCloseButtonRect(UiRect panel)
{
    return debugItemPickerCloseButtonRect(panel);
}

void stripUtf8Bom(std::string& text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

std::vector<std::string> splitTabs(std::string_view line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string_view::npos) {
            fields.emplace_back(line.substr(start));
            break;
        }
        fields.emplace_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return fields;
}

bool parseFloatOrDefault(std::string_view text, float& outValue, float fallback)
{
    const std::string trimmed = trimAscii(std::string(text));
    if (trimmed.empty()) {
        outValue = fallback;
        return false;
    }

    char* end = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &end);
    if (end == trimmed.c_str()) {
        outValue = fallback;
        return false;
    }
    outValue = parsed;
    return true;
}

bool parseBoolOrDefault(std::string_view text, bool fallback)
{
    const std::string normalized = lowerAscii(trimAscii(std::string(text)));
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
        return false;
    }
    return fallback;
}

std::string_view fieldOrEmpty(const std::vector<std::string>& fields, std::size_t index)
{
    return index < fields.size() ? std::string_view(fields[index]) : std::string_view{};
}

std::string formatAudioFloat(float value)
{
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    std::string text(buffer);
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text.empty() ? std::string("0") : text;
}

std::string normalizeAudioRelativePath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.rfind("./", 0) == 0) {
        path.erase(0, 2);
    }
    constexpr std::string_view AudioRootPrefix = "assets/audio/";
    if (path.rfind(AudioRootPrefix, 0) == 0) {
        path.erase(0, AudioRootPrefix.size());
    }
    return path;
}

std::string audioCueEditTypeText(AudioCueEditMode mode)
{
    return mode == AudioCueEditMode::Bgm ? "bgm" : "se";
}

std::string audioCueEditFolderText(AudioCueEditMode mode)
{
    return std::string(AudioCueEditAudioRoot) + "/" + audioCueEditTypeText(mode);
}

std::string audioCueEditTitle(AudioCueEditMode mode)
{
    return mode == AudioCueEditMode::Bgm ? "BGM編集" : "効果音編集";
}

AudioCueType audioCueEngineType(AudioCueEditMode mode)
{
    return mode == AudioCueEditMode::Bgm ? AudioCueType::Bgm : AudioCueType::Se;
}

bool audioCueEditEntryMatchesMode(const AudioCueEditEntry& entry, AudioCueEditMode mode)
{
    return lowerAscii(trimAscii(entry.type)) == audioCueEditTypeText(mode);
}

std::string audioCueDisplayName(const AudioCueEditEntry& entry)
{
    return entry.displayName.empty() ? entry.id : entry.displayName;
}

int audioCueEditFileIndexForPath(const std::vector<AudioCueFileEntry>& files, std::string path)
{
    const std::string normalized = lowerAscii(normalizeAudioRelativePath(trimAscii(std::move(path))));
    for (int i = 0; i < static_cast<int>(files.size()); ++i) {
        if (lowerAscii(normalizeAudioRelativePath(files[static_cast<std::size_t>(i)].relativePath)) == normalized) {
            return i;
        }
    }
    return -1;
}

AudioCueEditEntry parseAudioCueManifestEntry(const std::vector<std::string>& fields)
{
    AudioCueEditEntry entry;
    if (fields.size() >= 1) {
        entry.id = trimAscii(fields[0]);
    }
    if (fields.size() >= 2) {
        entry.type = lowerAscii(trimAscii(fields[1]));
    }
    if (fields.size() >= 3) {
        entry.path = normalizeAudioRelativePath(trimAscii(fields[2]));
    }
    if (fields.size() >= 7) {
        entry.displayName = trimAscii(fields[6]);
    }
    parseFloatOrDefault(fieldOrEmpty(fields, 3), entry.volume, 1.0f);
    entry.volume = clamp(entry.volume, 0.0f, 1.0f);
    entry.loop = parseBoolOrDefault(fieldOrEmpty(fields, 4), entry.type == "bgm");
    parseFloatOrDefault(fieldOrEmpty(fields, 5), entry.cooldownMs, 0.0f);
    entry.cooldownMs = std::max(0.0f, entry.cooldownMs);
    return entry;
}

bool loadAudioCueManifestRows(std::vector<AudioCueManifestRow>& rows, std::string& message)
{
    rows.clear();

    std::ifstream file(std::filesystem::path(std::string(AudioCueEditManifestPath)), std::ios::binary);
    if (!file) {
        message = "Audio manifest not found";
        return false;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (firstLine) {
            firstLine = false;
            stripUtf8Bom(line);
        }
        const std::string trimmed = trimAscii(line);
        if (trimmed.empty() || trimmed.rfind("#", 0) == 0) {
            continue;
        }

        std::vector<std::string> fields = splitTabs(line);
        if (!fields.empty() && lowerAscii(trimAscii(fields[0])) == "id") {
            continue;
        }
        if (fields.size() < 3) {
            continue;
        }

        AudioCueManifestRow row;
        row.entry = parseAudioCueManifestEntry(fields);
        row.valid = !row.entry.id.empty() && (row.entry.type == "bgm" || row.entry.type == "se");
        if (row.valid) {
            rows.push_back(std::move(row));
        }
    }

    message = "Audio manifest loaded";
    return true;
}

bool writeAudioCueManifestRows(const std::vector<AudioCueManifestRow>& rows, std::string& message)
{
    std::ofstream file(std::filesystem::path(std::string(AudioCueEditManifestPath)), std::ios::binary | std::ios::trunc);
    if (!file) {
        message = "Audio manifest save failed";
        return false;
    }

    file << "\xEF\xBB\xBF";
    file << "id\ttype\tpath\tvolume\tloop\tcooldown_ms\tdisplay_name\n";
    for (const AudioCueManifestRow& row : rows) {
        if (!row.valid || row.entry.id.empty()) {
            continue;
        }
        file
            << row.entry.id << '\t'
            << row.entry.type << '\t'
            << normalizeAudioRelativePath(row.entry.path) << '\t'
            << formatAudioFloat(row.entry.volume) << '\t'
            << (row.entry.loop ? "true" : "false") << '\t'
            << formatAudioFloat(row.entry.cooldownMs) << '\t'
            << row.entry.displayName << '\n';
    }

    if (!file) {
        message = "Audio manifest write failed";
        return false;
    }
    message = "Audio manifest saved";
    return true;
}

std::string debugItemPickerDisplayName(const ItemData& item)
{
    return item.name.empty() ? item.id : item.name;
}

std::string debugItemPickerSubtitle(const ItemData& item)
{
    std::string text = item.category.empty() ? std::string("未分類") : item.category;
    if (item.rarity > 0) {
        text += " / R";
        text += std::to_string(item.rarity);
    }
    return text;
}

std::string normalizedUiSearchText(std::string_view text)
{
    std::string normalized;
    normalized.reserve(text.size());
    for (unsigned char ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            normalized.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else {
            normalized.push_back(static_cast<char>(ch));
        }
    }
    return normalized;
}

bool uiSearchTextContains(std::string_view text, std::string_view normalizedQuery)
{
    if (normalizedQuery.empty()) {
        return true;
    }
    return normalizedUiSearchText(text).find(normalizedQuery) != std::string::npos;
}

bool debugItemPickerNameMatchesSearch(const ItemData& item, std::string_view normalizedQuery)
{
    return uiSearchTextContains(debugItemPickerDisplayName(item), normalizedQuery);
}

std::string objectImageScaleDisplayName(const ObjectDefinition& object)
{
    return object.name.empty() ? object.id : object.name;
}

bool objectImageScaleObjectMatchesSearch(const ObjectDefinition& object, std::string_view normalizedQuery)
{
    return uiSearchTextContains(objectImageScaleDisplayName(object), normalizedQuery);
}

bool objectHitboxObjectMatchesSearch(const ObjectDefinition& object, std::string_view normalizedQuery)
{
    return uiSearchTextContains(objectImageScaleDisplayName(object), normalizedQuery);
}

bool objectUsesCapturedEnemyVisual(const ObjectDefinition& object)
{
    return object.visual.source == ItemVisualSource::Enemy ||
        object.id.rfind("captured_", 0) == 0;
}

void eraseCapturedEnemyObjectHitboxes(HitboxCatalog& catalog, const ObjectCatalog& objects)
{
    for (auto it = catalog.objects.begin(); it != catalog.objects.end();) {
        const auto objectIt = objects.objectsById.find(it->first);
        const bool capturedEnemyObject = objectIt != objects.objectsById.end()
            ? objectUsesCapturedEnemyVisual(objectIt->second)
            : (it->first.rfind("captured_", 0) == 0);
        if (capturedEnemyObject) {
            it = catalog.objects.erase(it);
        } else {
            ++it;
        }
    }
}

float objectHitboxDefaultRadiusFor(const ObjectDefinition& object)
{
    if (object.id == "item_torch") {
        return 13.0f;
    }
    return 11.0f;
}

HitboxProfile fallbackObjectHitboxProfileFor(const ObjectDefinition& object)
{
    return singleCircleHitbox(objectHitboxDefaultRadiusFor(object));
}

HitboxProfile fallbackPlayerHitboxProfileFor(const RuntimeBalance& balance)
{
    return singleCircleHitbox(std::max(EnemyHitboxMinRadius, balance.playerRadius));
}

std::vector<HitCircle> playerHitboxEditCirclesFor(const HitboxCatalog& catalog, const RuntimeBalance& balance)
{
    if (const HitboxProfile* profile = playerHitboxProfileFor(&catalog)) {
        return profile->circles;
    }
    return fallbackPlayerHitboxProfileFor(balance).circles;
}

bool playerHitboxMatchesSearch(std::string_view normalizedQuery)
{
    return uiSearchTextContains("Player player プレイヤー", normalizedQuery);
}

std::string enemyHitboxDisplayName(const EnemyDefinition& enemy)
{
    return enemy.name.empty() ? enemy.id : enemy.name;
}

bool enemyHitboxEnemyMatchesSearch(const EnemyDefinition& enemy, std::string_view normalizedQuery)
{
    return uiSearchTextContains(enemyHitboxDisplayName(enemy), normalizedQuery);
}

float enemyHitboxDefaultRadiusFor(const EnemyDefinition& enemy, const RuntimeBalance& balance)
{
    return enemy.radius > 0.0 && std::isfinite(enemy.radius)
        ? static_cast<float>(enemy.radius)
        : balance.enemyRadius;
}

bool enemyHitboxDefinitionIsBoss(const EnemyDefinition& enemy)
{
    if (enemy.id == "stardust_mole" || enemy.id == "junk_crab" || enemy.id == "astragna" || enemy.id == "star_vein_dragon") {
        return true;
    }
    return std::any_of(enemy.enemyTags.begin(), enemy.enemyTags.end(), [](const std::string& tag) {
        return tag == "boss" || tag == "boss_only";
    });
}

Enemy makeEnemyHitboxPreviewEnemy(const EnemyDefinition& definition, const RuntimeBalance& balance)
{
    Enemy enemy;
    enemy.active = true;
    enemy.enemyId = definition.id;
    enemy.enemyName = enemyHitboxDisplayName(definition);
    enemy.definition = &definition;
    enemy.radius = enemyHitboxDefaultRadiusFor(definition, balance);
    enemy.isBoss = enemyHitboxDefinitionIsBoss(definition);
    enemy.facingAngle = Pi * 0.5f;
    return enemy;
}

HitboxProfile fallbackHitboxProfileFor(const EnemyDefinition& definition, const RuntimeBalance& balance)
{
    return singleCircleHitbox(std::max(EnemyHitboxMinRadius, enemyHitboxDefaultRadiusFor(definition, balance)));
}

HitboxDirection hitboxDirectionForEditorIndex(int index)
{
    switch (index) {
    case 1: return HitboxDirection::Down;
    case 2: return HitboxDirection::Left;
    case 3: return HitboxDirection::Right;
    case 4: return HitboxDirection::Up;
    default: return HitboxDirection::Default;
    }
}

HitboxDirection mirroredHitboxDirection(HitboxDirection direction)
{
    switch (direction) {
    case HitboxDirection::Left: return HitboxDirection::Right;
    case HitboxDirection::Right: return HitboxDirection::Left;
    case HitboxDirection::Default:
    case HitboxDirection::Down:
    case HitboxDirection::Up:
        return direction;
    }
    return direction;
}

HitCircle clampEnemyHitboxEditorCircle(HitCircle circle)
{
    circle.offset.x = clamp(circle.offset.x, -512.0f, 512.0f);
    circle.offset.y = clamp(circle.offset.y, -512.0f, 512.0f);
    circle.radius = clamp(circle.radius, EnemyHitboxMinRadius, EnemyHitboxMaxRadius);
    return circle;
}

void normalizeHitboxEditorCircles(std::vector<HitCircle>& circles)
{
    if (static_cast<int>(circles.size()) > HitboxMaxCircles) {
        circles.resize(HitboxMaxCircles);
    }
    for (HitCircle& circle : circles) {
        circle = clampEnemyHitboxEditorCircle(circle);
    }
}

std::vector<HitCircle> mirroredHitboxEditorCircles(std::vector<HitCircle> circles)
{
    for (HitCircle& circle : circles) {
        circle.offset.x = -circle.offset.x;
    }
    normalizeHitboxEditorCircles(circles);
    return circles;
}

EnemyShadowSpec mirroredEnemyShadowSpec(EnemyShadowSpec spec)
{
    spec.offset.x = -spec.offset.x;
    return sanitizeEnemyShadowSpec(spec);
}

float enemyShadowPreviewFacingAngle(double totalSeconds)
{
    const int step = static_cast<int>(std::floor(std::max(0.0, totalSeconds) / EnemyShadowPreviewDirectionSeconds)) % 4;
    switch (step) {
    case 1:
        return 0.0f;
    case 2:
        return -Pi * 0.5f;
    case 3:
        return Pi;
    default:
        return Pi * 0.5f;
    }
}

bool enemyHitboxDirectionClipboardHasAny(const EnemyHitboxDirectionClipboard& clipboard)
{
    return std::any_of(clipboard.hasProfile.begin(), clipboard.hasProfile.end(), [](bool hasProfile) {
        return hasProfile;
    });
}

std::vector<HitCircle> enemyHitboxEditCirclesFor(
    const HitboxCatalog& catalog,
    const EnemyDefinition& definition,
    const RuntimeBalance& balance,
    HitboxDirection direction)
{
    if (const HitboxProfile* profile = enemyHitboxProfileFor(&catalog, definition.id, direction)) {
        return profile->circles;
    }
    if (direction != HitboxDirection::Default) {
        if (const HitboxProfile* profile = enemyHitboxProfileFor(&catalog, definition.id, HitboxDirection::Default)) {
            return profile->circles;
        }
    }
    return fallbackHitboxProfileFor(definition, balance).circles;
}

std::vector<HitCircle> objectHitboxEditCirclesFor(
    const HitboxCatalog& catalog,
    const ObjectDefinition& object)
{
    const auto it = catalog.objects.find(object.id);
    if (it != catalog.objects.end() && !it->second.circles.empty()) {
        return it->second.circles;
    }
    return fallbackObjectHitboxProfileFor(object).circles;
}

bool debugStoryTestIsTutorialTrigger(std::string_view trigger)
{
    constexpr std::string_view Prefix = "tutorial:";
    return trigger.size() >= Prefix.size() && trigger.substr(0, Prefix.size()) == Prefix;
}

std::string debugStoryTestDisplayTitle(const StoryEvent& event)
{
    return event.title.empty() ? event.id : event.title;
}

std::string debugStoryTestDisplayTrigger(const StoryEvent& event)
{
    return event.trigger.empty() ? std::string("triggerなし") : event.trigger;
}

std::string debugStoryTestDisplayOnceFlag(const StoryEvent& event)
{
    if (event.repeatable) {
        return "repeat";
    }
    return event.onceFlag.empty() ? std::string("onceなし") : event.onceFlag;
}

std::string debugStageIdForToken(std::string_view token)
{
    if (token == "stage1" || token == "stage-1" || token == "1" || token == "stage_01_stardust") {
        return "stage_01_stardust";
    }
    if (token == "stage2" || token == "stage-2" || token == "2" || token == "stage_02_junk_magic") {
        return "stage_02_junk_magic";
    }
    if (token == "stage3" || token == "stage-3" || token == "3" || token == "stage_03_star_core") {
        return "stage_03_star_core";
    }
    if (token == "stage4" || token == "stage-4" || token == "4" || token == "stage_04_astral_mine" || token == "astral") {
        return "stage_04_astral_mine";
    }
    return {};
}

bool debugObjectInstanceMatchesObjectId(const InventoryObjectInstance& instance, std::string_view objectId)
{
    return instance.instance.objectId == objectId || instance.item.id == objectId;
}

bool debugIsRoguelikeStageDefinition(const StageDefinition& stage)
{
    return stage.id == "stage_04_astral_mine" ||
        stage.type == "ローグライク" ||
        stage.generationProfile == "astral_rogue";
}

enum class DebugAstralRoomTargetKind {
    None,
    SpecialRoom,
    Facility,
};

struct DebugAstralRoomTarget {
    DebugAstralRoomTargetKind kind = DebugAstralRoomTargetKind::None;
    SpecialRoomType specialRoom = SpecialRoomType::None;
    Game::RoguelikeFacilityKind facilityKind = Game::RoguelikeFacilityKind::Merchant;
};

SpecialRoomType debugSpecialRoomTypeForToken(std::string_view token)
{
    if (token == "ore" || token == "ore-room") {
        return SpecialRoomType::OreRoom;
    }
    if (token == "safe" || token == "safe-cavern") {
        return SpecialRoomType::SafeCavern;
    }
    if (token == "coin" || token == "coin-room") {
        return SpecialRoomType::CoinRoom;
    }
    if (token == "treasure" || token == "treasure-room") {
        return SpecialRoomType::TreasureRoom;
    }
    if (token == "enemy" || token == "enemy-room") {
        return SpecialRoomType::EnemyRoom;
    }
    return SpecialRoomType::None;
}

const char* debugSpecialRoomTypeLabel(SpecialRoomType type)
{
    switch (type) {
    case SpecialRoomType::OreRoom:
        return "鉱石";
    case SpecialRoomType::SafeCavern:
        return "安全";
    case SpecialRoomType::CoinRoom:
        return "コイン";
    case SpecialRoomType::TreasureRoom:
        return "宝物";
    case SpecialRoomType::EnemyRoom:
        return "敵";
    case SpecialRoomType::None:
        break;
    }
    return "なし";
}

DebugAstralRoomTarget debugAstralRoomTargetForToken(std::string_view token)
{
    if (token == "ore" || token == "ore-room") {
        return {DebugAstralRoomTargetKind::SpecialRoom, SpecialRoomType::OreRoom};
    }
    if (token == "coin" || token == "coin-room") {
        return {DebugAstralRoomTargetKind::SpecialRoom, SpecialRoomType::CoinRoom};
    }
    if (token == "treasure" || token == "treasure-room") {
        return {DebugAstralRoomTargetKind::SpecialRoom, SpecialRoomType::TreasureRoom};
    }
    if (token == "enemy" || token == "enemy-room") {
        return {DebugAstralRoomTargetKind::SpecialRoom, SpecialRoomType::EnemyRoom};
    }
    if (token == "merchant" || token == "facility-merchant") {
        return {DebugAstralRoomTargetKind::Facility, SpecialRoomType::None, Game::RoguelikeFacilityKind::Merchant};
    }
    if (token == "artisan" || token == "processor" || token == "facility-artisan") {
        return {DebugAstralRoomTargetKind::Facility, SpecialRoomType::None, Game::RoguelikeFacilityKind::Artisan};
    }
    if (token == "trainer" || token == "facility-trainer") {
        return {DebugAstralRoomTargetKind::Facility, SpecialRoomType::None, Game::RoguelikeFacilityKind::Trainer};
    }
    return {};
}

const char* debugAstralRoomTargetLabel(const DebugAstralRoomTarget& target)
{
    if (target.kind == DebugAstralRoomTargetKind::SpecialRoom) {
        return debugSpecialRoomTypeLabel(target.specialRoom);
    }
    if (target.kind == DebugAstralRoomTargetKind::Facility) {
        switch (target.facilityKind) {
        case Game::RoguelikeFacilityKind::Merchant:
            return "野良商人";
        case Game::RoguelikeFacilityKind::Artisan:
            return "野良加工職人";
        case Game::RoguelikeFacilityKind::Trainer:
            return "修練者";
        }
    }
    return "なし";
}

bool outdoorCharacterFacilityId(std::string_view facilityId)
{
    return facilityId == "merchant_npc" ||
        facilityId == "processor_npc" ||
        facilityId == "monica" ||
        facilityId == "elder";
}

BaseEditRect normalizeBaseFacilityRectForEdit(BaseArea area, std::string_view facilityId, BaseEditRect rect)
{
    rect = normalizeBaseEditRect(rect);
    if (area == BaseArea::Outdoor && outdoorCharacterFacilityId(facilityId)) {
        rect.w = PlayerSpriteDrawSize;
        rect.h = PlayerSpriteDrawSize;
        rect = normalizeBaseEditRect(rect);
    }
    return rect;
}

bool legacyOutdoorCharacterFacilityRect(std::string_view facilityId, BaseEditRect rect)
{
    if (facilityId == "merchant_npc") {
        return sameBaseEditRect(rect, {874.0f, 154.0f, 62.0f, 106.0f});
    }
    if (facilityId == "processor_npc") {
        return sameBaseEditRect(rect, {714.0f, 164.0f, 62.0f, 106.0f});
    }
    if (facilityId == "monica") {
        return sameBaseEditRect(rect, {841.0f, 245.0f, 74.0f, 86.0f});
    }
    if (facilityId == "elder") {
        return sameBaseEditRect(rect, {420.0f, 322.0f, 74.0f, 86.0f});
    }
    return false;
}

} // namespace

BaseEditRect Game::baseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect fallback) const
{
    const auto& table = area == BaseArea::Outdoor ? baseFacilityRectsOutdoor_ : baseFacilityRectsHome_;
    const auto it = table.find(std::string(facilityId));
    if (it == table.end()) {
        return normalizeBaseFacilityRectForEdit(area, facilityId, fallback);
    }
    const BaseEditRect rect = normalizeBaseEditRect(it->second);
    if (area == BaseArea::Outdoor && legacyOutdoorCharacterFacilityRect(facilityId, rect)) {
        return normalizeBaseFacilityRectForEdit(area, facilityId, fallback);
    }
    if (area == BaseArea::HomeInterior &&
        facilityId == std::string_view("bookshelf") &&
        sameBaseEditRect(rect, {368.0f, 322.0f, 118.0f, 157.0f})) {
        return normalizeBaseEditRect(fallback);
    }
    if (area == BaseArea::HomeInterior &&
        facilityId == std::string_view("diary") &&
        sameBaseEditRect(rect, {802.0f, 488.0f, 78.0f, 50.0f})) {
        return normalizeBaseEditRect(fallback);
    }
    if (area == BaseArea::HomeInterior &&
        facilityId == std::string_view("home_exit") &&
        sameBaseEditRect(rect, {592.0f, 574.0f, 96.0f, 42.0f})) {
        return normalizeBaseEditRect(fallback);
    }
    if (area == BaseArea::HomeInterior &&
        facilityId == std::string_view("bed") &&
        sameBaseEditRect(rect, {680.0f, 188.0f, 233.0f, 107.0f})) {
        return normalizeBaseEditRect(fallback);
    }
    if (area == BaseArea::Outdoor &&
        facilityId == std::string_view("home") &&
        (rect.w < 220.0f || rect.h < 220.0f)) {
        return normalizeBaseEditRect(fallback);
    }
    if (area == BaseArea::Outdoor &&
        facilityId == std::string_view("upgrade_forge") &&
        (rect.w < 180.0f || rect.h < 180.0f)) {
        return normalizeBaseEditRect(fallback);
    }
    return normalizeBaseFacilityRectForEdit(area, facilityId, rect);
}

void Game::setBaseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect rect)
{
    auto& table = area == BaseArea::Outdoor ? baseFacilityRectsOutdoor_ : baseFacilityRectsHome_;
    table[std::string(facilityId)] = normalizeBaseFacilityRectForEdit(area, facilityId, rect);
}

bool Game::isBasePassabilityBlocked(BaseArea area, int tileX, int tileY) const
{
    const auto& blocked = baseBlockedTilesFor(area, editedBasePassabilityLayer());
    return blocked.find(packBaseEditTile(tileX, tileY)) != blocked.end();
}

void Game::setBasePassabilityBlocked(BaseArea area, int tileX, int tileY, bool blocked)
{
    auto& table = baseBlockedTilesFor(area, editedBasePassabilityLayer());
    const std::int64_t key = packBaseEditTile(tileX, tileY);
    if (blocked) {
        table.insert(key);
    } else {
        table.erase(key);
    }
}

BaseEditPassabilityLayer Game::currentBasePassabilityLayer() const
{
    return ringWorkshopUnlocked_ ? BaseEditPassabilityLayer::Unlocked : BaseEditPassabilityLayer::Locked;
}

BaseEditPassabilityLayer Game::editedBasePassabilityLayer() const
{
    return baseEditEnabled_ ? baseEditPassabilityLayer_ : currentBasePassabilityLayer();
}

std::unordered_set<std::int64_t>& Game::baseBlockedTilesFor(BaseArea area, BaseEditPassabilityLayer layer)
{
    if (area == BaseArea::Outdoor) {
        return layer == BaseEditPassabilityLayer::Unlocked
            ? baseBlockedTilesOutdoorUnlocked_
            : baseBlockedTilesOutdoorLocked_;
    }
    return layer == BaseEditPassabilityLayer::Unlocked
        ? baseBlockedTilesHomeUnlocked_
        : baseBlockedTilesHomeLocked_;
}

const std::unordered_set<std::int64_t>& Game::baseBlockedTilesFor(BaseArea area, BaseEditPassabilityLayer layer) const
{
    if (area == BaseArea::Outdoor) {
        return layer == BaseEditPassabilityLayer::Unlocked
            ? baseBlockedTilesOutdoorUnlocked_
            : baseBlockedTilesOutdoorLocked_;
    }
    return layer == BaseEditPassabilityLayer::Unlocked
        ? baseBlockedTilesHomeUnlocked_
        : baseBlockedTilesHomeLocked_;
}

bool Game::copyBasePassabilityLayer()
{
    baseEditPassabilityClipboard_ = baseBlockedTilesFor(baseArea_, editedBasePassabilityLayer());
    baseEditPassabilityClipboardValid_ = true;
    return true;
}

bool Game::pasteBasePassabilityLayer()
{
    if (!baseEditPassabilityClipboardValid_) {
        return false;
    }
    auto& target = baseBlockedTilesFor(baseArea_, editedBasePassabilityLayer());
    if (target == baseEditPassabilityClipboard_) {
        return false;
    }
    pushBaseEditUndoSnapshot();
    target = baseEditPassabilityClipboard_;
    baseEditDirty_ = true;
    return true;
}

void Game::loadBaseEditData()
{
    baseFacilityRectsOutdoor_.clear();
    baseFacilityRectsHome_.clear();
    baseBlockedTilesOutdoorLocked_.clear();
    baseBlockedTilesOutdoorUnlocked_.clear();
    baseBlockedTilesHomeLocked_.clear();
    baseBlockedTilesHomeUnlocked_.clear();
    baseEditDirty_ = false;

    for (BaseArea area : {BaseArea::Outdoor, BaseArea::HomeInterior}) {
        auto& facilityRects = area == BaseArea::Outdoor ? baseFacilityRectsOutdoor_ : baseFacilityRectsHome_;
        auto& blockedLocked = baseBlockedTilesFor(area, BaseEditPassabilityLayer::Locked);
        auto& blockedUnlocked = baseBlockedTilesFor(area, BaseEditPassabilityLayer::Unlocked);
        const std::filesystem::path path = baseEditDataPath(area);
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            continue;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream stream(line);
            std::string key;
            stream >> key;
            if (key == "facility") {
                std::string id;
                BaseEditRect rect{};
                stream >> id >> rect.x >> rect.y >> rect.w >> rect.h;
                if (!stream.fail() && !id.empty()) {
                    const BaseEditRect normalizedRect = normalizeBaseEditRect(rect);
                    facilityRects[id] =
                        area == BaseArea::Outdoor && legacyOutdoorCharacterFacilityRect(id, normalizedRect)
                        ? normalizedRect
                        : normalizeBaseFacilityRectForEdit(area, id, normalizedRect);
                }
            } else if (key == "blocked") {
                std::string layerOrTileX;
                stream >> layerOrTileX;
                if (layerOrTileX == "locked" || layerOrTileX == "unlocked") {
                    int tileX = 0;
                    int tileY = 0;
                    stream >> tileX >> tileY;
                    if (!stream.fail()) {
                        baseBlockedTilesFor(
                            area,
                            layerOrTileX == "unlocked"
                                ? BaseEditPassabilityLayer::Unlocked
                                : BaseEditPassabilityLayer::Locked).insert(packBaseEditTile(tileX, tileY));
                    }
                } else {
                    int tileX = 0;
                    int tileY = 0;
                    std::istringstream tileStream(layerOrTileX);
                    tileStream >> tileX;
                    stream >> tileY;
                    if (!tileStream.fail() && !stream.fail()) {
                        const std::int64_t packed = packBaseEditTile(tileX, tileY);
                        blockedLocked.insert(packed);
                        blockedUnlocked.insert(packed);
                    }
                }
            }
        }
    }
}

bool Game::saveBaseEditData(std::string& message)
{
    std::error_code error;
    std::filesystem::create_directories("data", error);
    if (error) {
        message = "Base edit save failed: could not create data directory";
        return false;
    }

    for (BaseArea area : {BaseArea::Outdoor, BaseArea::HomeInterior}) {
        const std::filesystem::path path = baseEditDataPath(area);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            message = "Base edit save failed: could not open " + path.string();
            return false;
        }

        file << "\xEF\xBB\xBF" << "MAJO_BASE_EDIT_V2\n";

        std::vector<BaseFacility> facilities = baseFacilities(area, ringWorkshopUnlocked_);
        for (BaseFacility& facility : facilities) {
            const BaseEditRect rect = baseFacilityRectFor(area, facility.facilityId, toBaseEditRect(facility.rect));
            file << "facility "
                << facility.facilityId << " "
                << rect.x << " " << rect.y << " " << rect.w << " " << rect.h << "\n";
        }

        const auto writeBlocked = [&](BaseEditPassabilityLayer layer, std::string_view layerName) {
            const auto& blocked = baseBlockedTilesFor(area, layer);
            std::vector<std::int64_t> blockedSorted(blocked.begin(), blocked.end());
            std::sort(blockedSorted.begin(), blockedSorted.end());
            for (const std::int64_t packed : blockedSorted) {
                file << "blocked " << layerName << " " <<
                    baseEditTileXFromPacked(packed) << " " << baseEditTileYFromPacked(packed) << "\n";
            }
        };
        writeBlocked(BaseEditPassabilityLayer::Locked, "locked");
        writeBlocked(BaseEditPassabilityLayer::Unlocked, "unlocked");

        if (!file) {
            message = "Base edit save failed while writing " + path.string();
            return false;
        }
    }

    baseEditDirty_ = false;
    message = "Base edit saved";
    return true;
}

bool Game::loadObjectImageScaleData()
{
    objectImageScaleById_.clear();
    otherImageScaleByKey_.clear();
    objectImageScaleDirty_ = false;
    objectImageScaleStatus_.clear();

    const std::filesystem::path path = objectImageScaleDataPath();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        rebuildObjectImageScaleList();
        return false;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (firstLine) {
            firstLine = false;
            if (line == "MAJO_OBJECT_IMAGE_SCALE_V1" || line == "MAJO_IMAGE_SCALE_V2") {
                continue;
            }
        }

        std::istringstream stream(line);
        std::string kind;
        std::string id;
        float scale = 1.0f;
        stream >> kind >> id >> scale;
        if (stream.fail() || id.empty()) {
            continue;
        }

        std::unordered_map<std::string, float>* target = nullptr;
        if (kind == "scale" || kind == "object") {
            target = &objectImageScaleById_;
        } else if (kind == "other") {
            target = &otherImageScaleByKey_;
        } else {
            continue;
        }

        const float snapped = snapObjectImageScale(scale);
        if (std::abs(snapped - 1.0f) <= 0.0001f) {
            continue;
        }
        (*target)[id] = snapped;
    }

    rebuildObjectImageScaleList();
    return true;
}

bool Game::saveObjectImageScaleData(std::string& message)
{
    std::error_code error;
    std::filesystem::create_directories("data", error);
    if (error) {
        message = "Image scale save failed: could not create data directory";
        return false;
    }

    const std::filesystem::path path = objectImageScaleDataPath();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        message = "Image scale save failed: could not open " + path.string();
        return false;
    }

    std::vector<std::pair<std::string, float>> objectEntries;
    objectEntries.reserve(objectImageScaleById_.size());
    for (const auto& [objectId, scale] : objectImageScaleById_) {
        if (objectId.empty()) {
            continue;
        }
        const float snapped = snapObjectImageScale(scale);
        if (std::abs(snapped - 1.0f) <= 0.0001f) {
            continue;
        }
        objectEntries.push_back({objectId, snapped});
    }
    std::sort(objectEntries.begin(), objectEntries.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    std::vector<std::pair<std::string, float>> otherEntries;
    otherEntries.reserve(otherImageScaleByKey_.size());
    for (const auto& [key, scale] : otherImageScaleByKey_) {
        if (key.empty() || worldIconDefinitionByKey(key) == nullptr) {
            continue;
        }
        const float snapped = snapObjectImageScale(scale);
        if (std::abs(snapped - 1.0f) <= 0.0001f) {
            continue;
        }
        otherEntries.push_back({key, snapped});
    }
    std::sort(otherEntries.begin(), otherEntries.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    file << "MAJO_IMAGE_SCALE_V2\n";
    for (const auto& [objectId, scale] : objectEntries) {
        file << "object " << objectId << " " << scale << "\n";
    }
    for (const auto& [key, scale] : otherEntries) {
        file << "other " << key << " " << scale << "\n";
    }

    if (!file) {
        message = "Image scale save failed while writing " + path.string();
        return false;
    }

    objectImageScaleDirty_ = false;
    message = "Image scale saved";
    return true;
}

void Game::rebuildObjectImageScaleList()
{
    std::string previousObjectSelection;
    if (objectImageScaleSelectedIndex_ >= 0 &&
        objectImageScaleSelectedIndex_ < static_cast<int>(objectImageScaleObjectIds_.size())) {
        previousObjectSelection = objectImageScaleObjectIds_[static_cast<std::size_t>(objectImageScaleSelectedIndex_)];
    }
    std::string previousOtherSelection;
    if (otherImageScaleSelectedIndex_ >= 0 &&
        otherImageScaleSelectedIndex_ < static_cast<int>(otherImageScaleKeys_.size())) {
        previousOtherSelection = otherImageScaleKeys_[static_cast<std::size_t>(otherImageScaleSelectedIndex_)];
    }

    objectImageScaleAllObjectIds_.clear();
    objectImageScaleAllObjectIds_.reserve(objectCatalog_.objects.size());
    std::unordered_set<std::string> seen;
    seen.reserve(objectCatalog_.objects.size());

    for (const ObjectDefinition& object : objectCatalog_.objects) {
        if (object.id.empty() || object.imageNumber <= 0) {
            continue;
        }
        if (!seen.insert(object.id).second) {
            continue;
        }
        objectImageScaleAllObjectIds_.push_back(object.id);
    }

    std::sort(objectImageScaleAllObjectIds_.begin(), objectImageScaleAllObjectIds_.end(), [this](const std::string& left, const std::string& right) {
        const ObjectDefinition* lhs = objectCatalog_.registry.findById(left);
        const ObjectDefinition* rhs = objectCatalog_.registry.findById(right);
        if (lhs == nullptr || rhs == nullptr) {
            return left < right;
        }
        if (lhs->imageNumber != rhs->imageNumber) {
            return lhs->imageNumber < rhs->imageNumber;
        }
        return left < right;
    });

    applyObjectImageScaleFilter(previousObjectSelection);

    otherImageScaleKeys_.clear();
    for (const WorldIconDefinition& definition : worldIconDefinitions()) {
        if (!definition.key.empty() && definition.imageNumber > 0) {
            otherImageScaleKeys_.push_back(std::string(definition.key));
        }
    }

    otherImageScaleSelectedIndex_ = -1;
    if (!previousOtherSelection.empty()) {
        const auto it = std::find(otherImageScaleKeys_.begin(), otherImageScaleKeys_.end(), previousOtherSelection);
        if (it != otherImageScaleKeys_.end()) {
            otherImageScaleSelectedIndex_ = static_cast<int>(std::distance(otherImageScaleKeys_.begin(), it));
        }
    }
    if (otherImageScaleSelectedIndex_ < 0 && !otherImageScaleKeys_.empty()) {
        otherImageScaleSelectedIndex_ = 0;
    }
}

void Game::applyObjectImageScaleFilter(std::string_view preferredSelection)
{
    std::string previousSelection{preferredSelection};
    if (previousSelection.empty() &&
        objectImageScaleSelectedIndex_ >= 0 &&
        objectImageScaleSelectedIndex_ < static_cast<int>(objectImageScaleObjectIds_.size())) {
        previousSelection = objectImageScaleObjectIds_[static_cast<std::size_t>(objectImageScaleSelectedIndex_)];
    }

    objectImageScaleObjectIds_.clear();
    objectImageScaleObjectIds_.reserve(objectImageScaleAllObjectIds_.size());
    const std::string normalizedQuery = normalizedUiSearchText(objectImageScaleSearchInput_.text);
    for (const std::string& objectId : objectImageScaleAllObjectIds_) {
        const ObjectDefinition* object = objectCatalog_.registry.findById(objectId);
        if (object != nullptr && objectImageScaleObjectMatchesSearch(*object, normalizedQuery)) {
            objectImageScaleObjectIds_.push_back(objectId);
        }
    }

    objectImageScaleSelectedIndex_ = -1;
    if (!previousSelection.empty()) {
        const auto it = std::find(objectImageScaleObjectIds_.begin(), objectImageScaleObjectIds_.end(), previousSelection);
        if (it != objectImageScaleObjectIds_.end()) {
            objectImageScaleSelectedIndex_ = static_cast<int>(std::distance(objectImageScaleObjectIds_.begin(), it));
        }
    }
    if (objectImageScaleSelectedIndex_ < 0 && !objectImageScaleObjectIds_.empty()) {
        objectImageScaleSelectedIndex_ = 0;
    }
}

bool Game::handleObjectImageScaleEditEvent(const SDL_Event& event)
{
    if (mode_ != ScreenMode::ObjectImageScaleEdit || imageScaleEditTab_ != ImageScaleEditTab::Objects) {
        return false;
    }

    std::string previousSelection;
    if (objectImageScaleSelectedIndex_ >= 0 &&
        objectImageScaleSelectedIndex_ < static_cast<int>(objectImageScaleObjectIds_.size())) {
        previousSelection = objectImageScaleObjectIds_[static_cast<std::size_t>(objectImageScaleSelectedIndex_)];
    }

    const std::string previousText = objectImageScaleSearchInput_.text;
    const bool consumed = handleUiTextInputEvent(objectImageScaleSearchInput_, event, 48);
    if (objectImageScaleSearchInput_.text != previousText) {
        applyObjectImageScaleFilter(previousSelection);
        objectImageScaleScrollOffset_ = 0.0f;
        const ObjectImageScaleLayout layout = objectImageScaleGridLayout(
            makeObjectImageScaleLayout(camera_.width(), camera_.height()),
            true);
        keepObjectImageScaleSelectionVisible(
            layout,
            objectImageScaleSelectedIndex_,
            static_cast<int>(objectImageScaleObjectIds_.size()),
            objectImageScaleScrollOffset_);
    }
    return consumed;
}

void Game::enterObjectImageScaleEditMode()
{
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        return;
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }
    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        exitEnemyHitboxEditMode();
    }
    if (mode_ == ScreenMode::EnemyShadowEdit) {
        exitEnemyShadowEditMode();
    }

    objectImageScaleSearchInput_.text.clear();
    rebuildObjectImageScaleList();
    objectImageScaleReturnMode_ = mode_;
    if (objectImageScaleReturnMode_ == ScreenMode::ObjectImageScaleEdit) {
        objectImageScaleReturnMode_ = ScreenMode::Playing;
    }

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    objectImageScaleScrollOffset_ = std::max(0.0f, objectImageScaleScrollOffset_);
    otherImageScaleScrollOffset_ = std::max(0.0f, otherImageScaleScrollOffset_);
    objectImageScaleStatus_ = objectImageScaleObjectIds_.empty() && otherImageScaleKeys_.empty()
        ? "No images available"
        : "画像サイズ編集";
    mode_ = ScreenMode::ObjectImageScaleEdit;
    if (imageScaleEditTab_ == ImageScaleEditTab::Objects) {
        focusUiTextInput(objectImageScaleSearchInput_);
    }
}

void Game::exitObjectImageScaleEditMode()
{
    if (mode_ != ScreenMode::ObjectImageScaleEdit) {
        return;
    }

    blurUiTextInput(objectImageScaleSearchInput_);
    mode_ = objectImageScaleReturnMode_;
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        mode_ = ScreenMode::Playing;
    }
}

void Game::updateObjectImageScaleEditScreen(const Input& input, UiContext& ui)
{
    if (mode_ != ScreenMode::ObjectImageScaleEdit) {
        return;
    }

    if (input.backPressed()) {
        exitObjectImageScaleEditMode();
        return;
    }

    if (ui.pressed(objectImageScaleTabRect(0))) {
        imageScaleEditTab_ = ImageScaleEditTab::Objects;
        objectImageScaleStatus_ = "Objects";
        focusUiTextInput(objectImageScaleSearchInput_);
    }
    if (ui.pressed(objectImageScaleTabRect(1))) {
        imageScaleEditTab_ = ImageScaleEditTab::Others;
        objectImageScaleStatus_ = "Others";
        blurUiTextInput(objectImageScaleSearchInput_);
    }

    std::vector<std::string>& itemKeys = imageScaleEditTab_ == ImageScaleEditTab::Others
        ? otherImageScaleKeys_
        : objectImageScaleObjectIds_;
    std::unordered_map<std::string, float>& scaleMap = imageScaleEditTab_ == ImageScaleEditTab::Others
        ? otherImageScaleByKey_
        : objectImageScaleById_;
    int& selectedIndex = imageScaleEditTab_ == ImageScaleEditTab::Others
        ? otherImageScaleSelectedIndex_
        : objectImageScaleSelectedIndex_;
    float& scrollOffset = imageScaleEditTab_ == ImageScaleEditTab::Others
        ? otherImageScaleScrollOffset_
        : objectImageScaleScrollOffset_;
    const bool editingOthers = imageScaleEditTab_ == ImageScaleEditTab::Others;

    const ObjectImageScaleLayout baseLayout = makeObjectImageScaleLayout(camera_.width(), camera_.height());
    const ObjectImageScaleLayout layout = objectImageScaleGridLayout(baseLayout, !editingOthers);
    int itemCount = static_cast<int>(itemKeys.size());
    float maxScroll = objectImageScaleMaxScroll(layout, itemCount);
    scrollOffset = clamp(scrollOffset, 0.0f, maxScroll);

    if (!editingOthers) {
        updateUiTextInput(objectImageScaleSearchInput_, ui, objectImageScaleSearchInputRect(baseLayout));
        if (ui.pressed(objectImageScaleSearchClearButtonRect(baseLayout))) {
            if (!objectImageScaleSearchInput_.text.empty()) {
                std::string previousSelection;
                if (objectImageScaleSelectedIndex_ >= 0 &&
                    objectImageScaleSelectedIndex_ < static_cast<int>(objectImageScaleObjectIds_.size())) {
                    previousSelection = objectImageScaleObjectIds_[static_cast<std::size_t>(objectImageScaleSelectedIndex_)];
                }
                objectImageScaleSearchInput_.text.clear();
                applyObjectImageScaleFilter(previousSelection);
                objectImageScaleScrollOffset_ = 0.0f;
                itemCount = static_cast<int>(objectImageScaleObjectIds_.size());
                maxScroll = objectImageScaleMaxScroll(layout, itemCount);
                keepObjectImageScaleSelectionVisible(layout, objectImageScaleSelectedIndex_, itemCount, objectImageScaleScrollOffset_);
            }
            focusUiTextInput(objectImageScaleSearchInput_);
        }
    }

    if (input.saveShortcutPressed()) {
        std::string message;
        if (saveObjectImageScaleData(message)) {
            objectImageScaleStatus_ = message;
        } else {
            objectImageScaleStatus_ = message;
        }
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = objectImageScaleCardRect(layout, i, scrollOffset);
        if (ui.pressed(rect)) {
            selectedIndex = i;
            break;
        }
    }

    auto adjustSelectedScale = [this, &itemKeys, &scaleMap, &selectedIndex, editingOthers](int direction) {
        if (direction == 0 || selectedIndex < 0 || selectedIndex >= static_cast<int>(itemKeys.size())) {
            return;
        }

        const std::string& key = itemKeys[static_cast<std::size_t>(selectedIndex)];
        float currentScale = 1.0f;
        if (const auto it = scaleMap.find(key); it != scaleMap.end()) {
            currentScale = it->second;
        }
        const float nextScale = snapObjectImageScale(currentScale + ObjectImageScaleStep * static_cast<float>(direction));
        if (std::abs(nextScale - 1.0f) <= 0.0001f) {
            scaleMap.erase(key);
        } else {
            scaleMap[key] = nextScale;
        }
        objectImageScaleDirty_ = true;

        char status[160];
        std::snprintf(status, sizeof(status), "%s %s scale = %.2f", editingOthers ? "other" : "object", key.c_str(), nextScale);
        objectImageScaleStatus_ = status;
    };

    if (input.pressed(InputAction::MoveUp)) {
        adjustSelectedScale(+1);
    }
    if (input.pressed(InputAction::MoveDown)) {
        adjustSelectedScale(-1);
    }

    const int wheel = input.shortcutCursorDelta();
    if (wheel != 0) {
        int hoveredIndex = -1;
        for (int i = 0; i < itemCount; ++i) {
            const UiRect rect = objectImageScaleCardRect(layout, i, scrollOffset);
            if (rect.contains(ui.mouse())) {
                hoveredIndex = i;
                break;
            }
        }

        if (hoveredIndex == selectedIndex && selectedIndex >= 0) {
            adjustSelectedScale(-wheel);
        } else {
            scrollOffset = clamp(
                scrollOffset + static_cast<float>(wheel) * 36.0f,
                0.0f,
                maxScroll);
        }
    }
}

void Game::renderObjectImageScaleEditScreen(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const bool editingOthers = imageScaleEditTab_ == ImageScaleEditTab::Others;
    const std::vector<std::string>& itemKeys = editingOthers ? otherImageScaleKeys_ : objectImageScaleObjectIds_;
    const std::unordered_map<std::string, float>& scaleMap = editingOthers ? otherImageScaleByKey_ : objectImageScaleById_;
    const int selectedIndex = editingOthers ? otherImageScaleSelectedIndex_ : objectImageScaleSelectedIndex_;
    const float activeScrollOffset = editingOthers ? otherImageScaleScrollOffset_ : objectImageScaleScrollOffset_;

    const ObjectImageScaleLayout baseLayout = makeObjectImageScaleLayout(camera_.width(), camera_.height());
    const ObjectImageScaleLayout layout = objectImageScaleGridLayout(baseLayout, !editingOthers);
    const int itemCount = static_cast<int>(itemKeys.size());
    const float maxScroll = objectImageScaleMaxScroll(layout, itemCount);
    const float scrollOffset = clamp(activeScrollOffset, 0.0f, maxScroll);

    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {10, 12, 18, 255});
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), ObjectImageScaleHeaderHeight}, {18, 24, 38, 255});
    renderer.fillRect({0.0f, static_cast<float>(camera_.height()) - ObjectImageScaleFooterHeight}, {static_cast<float>(camera_.width()), ObjectImageScaleFooterHeight}, {18, 24, 38, 255});
    renderer.drawText({22.0f, 18.0f}, "画像サイズ編集 (48px baseline)", {245, 245, 252, 255}, 3);

    for (int tab = 0; tab < 2; ++tab) {
        const bool active = (tab == 0 && !editingOthers) || (tab == 1 && editingOthers);
        const UiRect rect = objectImageScaleTabRect(tab);
        renderer.fillRect(rect.pos, rect.size, active ? Color{58, 76, 118, 255} : Color{26, 34, 50, 255});
        renderer.drawRect(rect.pos, rect.size, active ? Color{255, 228, 138, 255} : Color{92, 104, 126, 255});
        renderer.drawText(rect.pos + Vec2{14.0f, 8.0f}, tab == 0 ? "Objects" : "Others", active ? Color{255, 236, 166, 255} : Color{198, 206, 222, 255}, 2);
    }

    renderer.drawText(
        {22.0f, 58.0f},
        editingOthers
            ? "Up/Down: scale  Ctrl+S: save  Esc: exit"
            : "Type: search  Up/Down: scale  Ctrl+S: save  Esc: exit",
        {198, 206, 222, 255},
        2);
    if (!editingOthers) {
        drawUiTextInput(renderer, objectImageScaleSearchInputRect(baseLayout), objectImageScaleSearchInput_, "アイテム名で検索", {});
        UiButtonStyle clearStyle;
        if (objectImageScaleSearchInput_.text.empty()) {
            clearStyle.text = ui::TextDisabled;
            clearStyle.fill = {18, 24, 42, 170};
            clearStyle.outline = {90, 84, 108, 170};
        }
        drawUiRectButton(renderer, objectImageScaleSearchClearButtonRect(baseLayout), "消去", false, clearStyle);
        const std::string countText = std::to_string(itemCount) + "/" + std::to_string(static_cast<int>(objectImageScaleAllObjectIds_.size())) + "件";
        const UiRect countRect = objectImageScaleSearchCountRect(baseLayout);
        const std::string fittedCountText = fittedSingleLineText(renderer, countText, countRect.size.x, 2);
        renderer.drawText(
            {
                countRect.pos.x + std::max(0.0f, countRect.size.x - renderer.measureText(fittedCountText, 2).x),
                countRect.pos.y + 11.0f,
            },
            fittedCountText,
            {198, 206, 222, 255},
            2);
    }
    renderer.drawRect(layout.viewport.pos, layout.viewport.size, {96, 108, 132, 255});

    if (itemCount <= 0 && !editingOthers) {
        const std::string emptyMessage = objectImageScaleSearchInput_.text.empty()
            ? "画像付きアイテムがありません"
            : "検索に一致するアイテムがありません";
        renderer.drawText(layout.viewport.pos + Vec2{18.0f, 18.0f}, emptyMessage, {198, 206, 222, 255}, 2);
    }

    renderer.pushClipRect(layout.viewport.pos, layout.viewport.size);
    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = objectImageScaleCardRect(layout, i, scrollOffset);
        if (rect.pos.y + rect.size.y < layout.viewport.pos.y || rect.pos.y > layout.viewport.pos.y + layout.viewport.size.y) {
            continue;
        }

        const bool selected = i == selectedIndex;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{44, 58, 92, 255} : Color{24, 30, 44, 255});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 228, 138, 255} : Color{74, 86, 108, 255});

        const std::string& key = itemKeys[static_cast<std::size_t>(i)];
        float scale = 1.0f;
        if (const auto it = scaleMap.find(key); it != scaleMap.end()) {
            scale = it->second;
        }
        scale = snapObjectImageScale(scale);

        std::string name;
        std::string subtitle;
        bool drewImage = false;
        const Vec2 previewCenter = rect.pos + Vec2{rect.size.x * 0.5f, 38.0f};
        if (editingOthers) {
            const WorldIconDefinition* definition = worldIconDefinitionByKey(key);
            if (definition != nullptr) {
                WorldIconDrawOptions options;
                options.allowUpscale = true;
                drewImage = drawWorldIcon(renderer, definition->iconId, previewCenter, {ObjectImageScalePreviewSize, ObjectImageScalePreviewSize}, options);
                name = std::string(definition->displayName);
                subtitle = "img_" + std::to_string(definition->imageNumber) + " / " + key;
            } else {
                name = key;
                subtitle = "missing icon";
            }
        } else {
            const ObjectDefinition* object = objectCatalog_.registry.findById(key);
            if (object != nullptr) {
                ObjectImageDrawOptions options;
                options.allowUpscale = true;
                drewImage = drawObjectImage(
                    renderer,
                    *object,
                    previewCenter,
                    {ObjectImageScalePreviewSize, ObjectImageScalePreviewSize},
                    objectGroundImageOptions(*object, options));
                name = objectImageScaleDisplayName(*object);
                subtitle = key;
            } else {
                name = key;
                subtitle = "missing object";
            }
        }

        if (!drewImage) {
            renderer.fillCircle(previewCenter, 22.0f, {92, 102, 120, 255});
        }

        const std::string shownName = fittedSingleLineText(renderer, name, rect.size.x - 12.0f, 2);
        renderer.drawText(rect.pos + Vec2{6.0f, 68.0f}, shownName, {232, 236, 245, 255}, 2);
        const std::string shownSubtitle = fittedSingleLineText(renderer, subtitle, rect.size.x - 12.0f, 1);
        renderer.drawText(rect.pos + Vec2{6.0f, 92.0f}, shownSubtitle, {146, 158, 178, 255}, 1);

        char scaleText[64];
        std::snprintf(scaleText, sizeof(scaleText), "x%.2f", scale);
        renderer.drawText(rect.pos + Vec2{6.0f, 108.0f}, scaleText, selected ? Color{255, 232, 148, 255} : Color{186, 198, 216, 255}, 2);
    }
    renderer.popClipRect();

    const char* dirty = objectImageScaleDirty_ ? "Unsaved (*)" : "Saved";
    renderer.drawText({22.0f, static_cast<float>(camera_.height()) - 40.0f}, dirty, objectImageScaleDirty_ ? Color{255, 230, 150, 255} : Color{170, 220, 170, 255}, 2);
    if (!objectImageScaleStatus_.empty()) {
        renderer.drawText({220.0f, static_cast<float>(camera_.height()) - 40.0f}, objectImageScaleStatus_, {198, 206, 222, 255}, 2);
    }
}

bool Game::loadHitboxData()
{
    std::string message;
    bool loaded = loadHitboxCatalog(hitboxDataPath(), hitboxes_, message);
    if (!loaded && std::filesystem::exists(legacyEnemyHitboxDataPath())) {
        loaded = loadHitboxCatalog(legacyEnemyHitboxDataPath(), hitboxes_, message);
    }
    eraseCapturedEnemyObjectHitboxes(hitboxes_, objectCatalog_);
    enemies_.setHitboxCatalog(&hitboxes_);
    enemyHitboxDirty_ = false;
    hitboxEditUndoStack_.clear();
    hitboxEditRedoStack_.clear();
    enemyHitboxStatus_ = loaded ? message : "Hitbox fallback active";
    rebuildEnemyHitboxEditList();
    return loaded;
}

bool Game::saveHitboxData(std::string& message)
{
    const bool saved = saveHitboxCatalog(hitboxDataPath(), hitboxes_, message);
    if (saved) {
        enemyHitboxDirty_ = false;
    }
    enemyHitboxStatus_ = message;
    enemies_.setHitboxCatalog(&hitboxes_);
    return saved;
}

void Game::rebuildEnemyHitboxEditList()
{
    std::string previousSelection;
    if (hitboxEditTab_ == HitboxEditTab::Enemies &&
        enemyHitboxSelectedEnemyIndex_ >= 0 &&
        enemyHitboxSelectedEnemyIndex_ < static_cast<int>(enemyHitboxEnemyIds_.size())) {
        previousSelection = enemyHitboxEnemyIds_[static_cast<std::size_t>(enemyHitboxSelectedEnemyIndex_)];
    } else if (hitboxEditTab_ == HitboxEditTab::Objects &&
        objectHitboxSelectedObjectIndex_ >= 0 &&
        objectHitboxSelectedObjectIndex_ < static_cast<int>(objectHitboxObjectIds_.size())) {
        previousSelection = objectHitboxObjectIds_[static_cast<std::size_t>(objectHitboxSelectedObjectIndex_)];
    } else if (hitboxEditTab_ == HitboxEditTab::Player &&
        playerHitboxSelectedIndex_ >= 0 &&
        playerHitboxSelectedIndex_ < static_cast<int>(playerHitboxIds_.size())) {
        previousSelection = playerHitboxIds_[static_cast<std::size_t>(playerHitboxSelectedIndex_)];
    }

    enemyHitboxAllEnemyIds_.clear();
    enemyHitboxAllEnemyIds_.reserve(enemyCatalog_.enemies.size());
    std::unordered_set<std::string> seen;
    seen.reserve(std::max(enemyCatalog_.enemies.size(), objectCatalog_.objects.size()));
    for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
        if (enemy.id.empty() || !seen.insert(enemy.id).second) {
            continue;
        }
        enemyHitboxAllEnemyIds_.push_back(enemy.id);
    }

    std::sort(enemyHitboxAllEnemyIds_.begin(), enemyHitboxAllEnemyIds_.end(), [this](const std::string& left, const std::string& right) {
        const auto lhs = enemyCatalog_.enemiesById.find(left);
        const auto rhs = enemyCatalog_.enemiesById.find(right);
        if (lhs == enemyCatalog_.enemiesById.end() || rhs == enemyCatalog_.enemiesById.end()) {
            return left < right;
        }
        if (lhs->second.imageNumber != rhs->second.imageNumber) {
            return lhs->second.imageNumber < rhs->second.imageNumber;
        }
        return left < right;
    });

    seen.clear();
    objectHitboxAllObjectIds_.clear();
    objectHitboxAllObjectIds_.reserve(objectCatalog_.objects.size());
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        if (object.id.empty() || objectUsesCapturedEnemyVisual(object) || !seen.insert(object.id).second) {
            continue;
        }
        objectHitboxAllObjectIds_.push_back(object.id);
    }
    std::sort(objectHitboxAllObjectIds_.begin(), objectHitboxAllObjectIds_.end(), [this](const std::string& left, const std::string& right) {
        const auto lhs = objectCatalog_.objectsById.find(left);
        const auto rhs = objectCatalog_.objectsById.find(right);
        if (lhs == objectCatalog_.objectsById.end() || rhs == objectCatalog_.objectsById.end()) {
            return left < right;
        }
        if (lhs->second.imageNumber != rhs->second.imageNumber) {
            return lhs->second.imageNumber < rhs->second.imageNumber;
        }
        return left < right;
    });

    playerHitboxAllIds_.assign(1, "player");

    applyEnemyHitboxEditFilter(previousSelection);
}

void Game::applyEnemyHitboxEditFilter(std::string_view preferredSelection)
{
    std::string previousSelection(preferredSelection);
    if (previousSelection.empty() && hitboxEditTab_ == HitboxEditTab::Enemies &&
        enemyHitboxSelectedEnemyIndex_ >= 0 &&
        enemyHitboxSelectedEnemyIndex_ < static_cast<int>(enemyHitboxEnemyIds_.size())) {
        previousSelection = enemyHitboxEnemyIds_[static_cast<std::size_t>(enemyHitboxSelectedEnemyIndex_)];
    } else if (previousSelection.empty() && hitboxEditTab_ == HitboxEditTab::Objects &&
        objectHitboxSelectedObjectIndex_ >= 0 &&
        objectHitboxSelectedObjectIndex_ < static_cast<int>(objectHitboxObjectIds_.size())) {
        previousSelection = objectHitboxObjectIds_[static_cast<std::size_t>(objectHitboxSelectedObjectIndex_)];
    } else if (previousSelection.empty() && hitboxEditTab_ == HitboxEditTab::Player &&
        playerHitboxSelectedIndex_ >= 0 &&
        playerHitboxSelectedIndex_ < static_cast<int>(playerHitboxIds_.size())) {
        previousSelection = playerHitboxIds_[static_cast<std::size_t>(playerHitboxSelectedIndex_)];
    }

    enemyHitboxEnemyIds_.clear();
    objectHitboxObjectIds_.clear();
    playerHitboxIds_.clear();
    const std::string normalizedQuery = normalizedUiSearchText(enemyHitboxSearchInput_.text);
    for (const std::string& enemyId : enemyHitboxAllEnemyIds_) {
        const auto it = enemyCatalog_.enemiesById.find(enemyId);
        if (it != enemyCatalog_.enemiesById.end() &&
            enemyHitboxEnemyMatchesSearch(it->second, normalizedQuery)) {
            enemyHitboxEnemyIds_.push_back(enemyId);
        }
    }
    for (const std::string& objectId : objectHitboxAllObjectIds_) {
        const auto it = objectCatalog_.objectsById.find(objectId);
        if (it != objectCatalog_.objectsById.end() &&
            objectHitboxObjectMatchesSearch(it->second, normalizedQuery)) {
            objectHitboxObjectIds_.push_back(objectId);
        }
    }
    for (const std::string& playerId : playerHitboxAllIds_) {
        if (playerId == "player" && playerHitboxMatchesSearch(normalizedQuery)) {
            playerHitboxIds_.push_back(playerId);
        }
    }

    enemyHitboxSelectedEnemyIndex_ = -1;
    objectHitboxSelectedObjectIndex_ = -1;
    playerHitboxSelectedIndex_ = -1;
    if (!previousSelection.empty()) {
        if (hitboxEditTab_ == HitboxEditTab::Enemies) {
            const auto it = std::find(enemyHitboxEnemyIds_.begin(), enemyHitboxEnemyIds_.end(), previousSelection);
            if (it != enemyHitboxEnemyIds_.end()) {
                enemyHitboxSelectedEnemyIndex_ = static_cast<int>(std::distance(enemyHitboxEnemyIds_.begin(), it));
            }
        } else if (hitboxEditTab_ == HitboxEditTab::Objects) {
            const auto it = std::find(objectHitboxObjectIds_.begin(), objectHitboxObjectIds_.end(), previousSelection);
            if (it != objectHitboxObjectIds_.end()) {
                objectHitboxSelectedObjectIndex_ = static_cast<int>(std::distance(objectHitboxObjectIds_.begin(), it));
            }
        } else {
            const auto it = std::find(playerHitboxIds_.begin(), playerHitboxIds_.end(), previousSelection);
            if (it != playerHitboxIds_.end()) {
                playerHitboxSelectedIndex_ = static_cast<int>(std::distance(playerHitboxIds_.begin(), it));
            }
        }
    }
    if (enemyHitboxSelectedEnemyIndex_ < 0 && !enemyHitboxEnemyIds_.empty()) {
        enemyHitboxSelectedEnemyIndex_ = 0;
    }
    if (objectHitboxSelectedObjectIndex_ < 0 && !objectHitboxObjectIds_.empty()) {
        objectHitboxSelectedObjectIndex_ = 0;
    }
    if (playerHitboxSelectedIndex_ < 0 && !playerHitboxIds_.empty()) {
        playerHitboxSelectedIndex_ = 0;
    }
    enemyHitboxSelectedCircleIndex_ = 0;
}

HitboxEditSnapshot Game::makeHitboxEditSnapshot() const
{
    HitboxEditSnapshot snapshot;
    snapshot.catalog = hitboxes_;
    snapshot.tab = hitboxEditTab_;
    snapshot.enemyDirection = enemyHitboxDirection_;
    snapshot.selectedCircleIndex = enemyHitboxSelectedCircleIndex_;
    if (hitboxEditTab_ == HitboxEditTab::Player) {
        snapshot.selectedId = "player";
    } else if (hitboxEditTab_ == HitboxEditTab::Objects) {
        if (objectHitboxSelectedObjectIndex_ >= 0 &&
            objectHitboxSelectedObjectIndex_ < static_cast<int>(objectHitboxObjectIds_.size())) {
            snapshot.selectedId = objectHitboxObjectIds_[static_cast<std::size_t>(objectHitboxSelectedObjectIndex_)];
        }
    } else if (enemyHitboxSelectedEnemyIndex_ >= 0 &&
        enemyHitboxSelectedEnemyIndex_ < static_cast<int>(enemyHitboxEnemyIds_.size())) {
        snapshot.selectedId = enemyHitboxEnemyIds_[static_cast<std::size_t>(enemyHitboxSelectedEnemyIndex_)];
    }
    return snapshot;
}

void Game::restoreHitboxEditSnapshot(const HitboxEditSnapshot& snapshot)
{
    hitboxes_ = snapshot.catalog;
    hitboxEditTab_ = snapshot.tab;
    enemyHitboxDirection_ = snapshot.enemyDirection;
    enemyHitboxDraggingCircle_ = false;
    enemyHitboxDragUndoSnapshotPushed_ = false;
    applyEnemyHitboxEditFilter(snapshot.selectedId);

    int circleCount = 0;
    if (hitboxEditTab_ == HitboxEditTab::Player &&
        playerHitboxSelectedIndex_ >= 0 &&
        playerHitboxSelectedIndex_ < static_cast<int>(playerHitboxIds_.size())) {
        circleCount = static_cast<int>(playerHitboxEditCirclesFor(hitboxes_, balance_).size());
    } else if (hitboxEditTab_ == HitboxEditTab::Objects) {
        if (objectHitboxSelectedObjectIndex_ >= 0 &&
            objectHitboxSelectedObjectIndex_ < static_cast<int>(objectHitboxObjectIds_.size())) {
            const std::string& objectId = objectHitboxObjectIds_[static_cast<std::size_t>(objectHitboxSelectedObjectIndex_)];
            const auto it = objectCatalog_.objectsById.find(objectId);
            if (it != objectCatalog_.objectsById.end()) {
                circleCount = static_cast<int>(objectHitboxEditCirclesFor(hitboxes_, it->second).size());
            }
        }
    } else if (enemyHitboxSelectedEnemyIndex_ >= 0 &&
        enemyHitboxSelectedEnemyIndex_ < static_cast<int>(enemyHitboxEnemyIds_.size())) {
        const std::string& enemyId = enemyHitboxEnemyIds_[static_cast<std::size_t>(enemyHitboxSelectedEnemyIndex_)];
        const auto it = enemyCatalog_.enemiesById.find(enemyId);
        if (it != enemyCatalog_.enemiesById.end()) {
            circleCount = static_cast<int>(enemyHitboxEditCirclesFor(hitboxes_, it->second, balance_, enemyHitboxDirection_).size());
        }
    }

    enemyHitboxSelectedCircleIndex_ = circleCount > 0
        ? std::clamp(snapshot.selectedCircleIndex, 0, circleCount - 1)
        : -1;
    enemyHitboxDirty_ = true;
    enemies_.setHitboxCatalog(&hitboxes_);
}

void Game::pushHitboxEditUndoSnapshot()
{
    HitboxEditSnapshot snapshot = makeHitboxEditSnapshot();
    if (!hitboxEditUndoStack_.empty() && hitboxEditUndoStack_.back() == snapshot) {
        return;
    }

    hitboxEditUndoStack_.push_back(std::move(snapshot));
    if (static_cast<int>(hitboxEditUndoStack_.size()) > HitboxEditUndoLimit) {
        hitboxEditUndoStack_.erase(hitboxEditUndoStack_.begin());
    }
    hitboxEditRedoStack_.clear();
}

bool Game::undoHitboxEdit()
{
    if (hitboxEditUndoStack_.empty()) {
        return false;
    }

    hitboxEditRedoStack_.push_back(makeHitboxEditSnapshot());
    const HitboxEditSnapshot snapshot = std::move(hitboxEditUndoStack_.back());
    hitboxEditUndoStack_.pop_back();
    restoreHitboxEditSnapshot(snapshot);
    return true;
}

bool Game::redoHitboxEdit()
{
    if (hitboxEditRedoStack_.empty()) {
        return false;
    }

    hitboxEditUndoStack_.push_back(makeHitboxEditSnapshot());
    if (static_cast<int>(hitboxEditUndoStack_.size()) > HitboxEditUndoLimit) {
        hitboxEditUndoStack_.erase(hitboxEditUndoStack_.begin());
    }

    const HitboxEditSnapshot snapshot = std::move(hitboxEditRedoStack_.back());
    hitboxEditRedoStack_.pop_back();
    restoreHitboxEditSnapshot(snapshot);
    return true;
}

const EnemyDefinition* Game::selectedEnemyHitboxDefinitionForEdit() const
{
    if (enemyHitboxSelectedEnemyIndex_ < 0 ||
        enemyHitboxSelectedEnemyIndex_ >= static_cast<int>(enemyHitboxEnemyIds_.size())) {
        return nullptr;
    }

    const std::string& enemyId = enemyHitboxEnemyIds_[static_cast<std::size_t>(enemyHitboxSelectedEnemyIndex_)];
    const auto it = enemyCatalog_.enemiesById.find(enemyId);
    return it != enemyCatalog_.enemiesById.end() ? &it->second : nullptr;
}

const ObjectDefinition* Game::selectedObjectHitboxDefinitionForEdit() const
{
    if (objectHitboxSelectedObjectIndex_ < 0 ||
        objectHitboxSelectedObjectIndex_ >= static_cast<int>(objectHitboxObjectIds_.size())) {
        return nullptr;
    }

    const std::string& objectId = objectHitboxObjectIds_[static_cast<std::size_t>(objectHitboxSelectedObjectIndex_)];
    const auto it = objectCatalog_.objectsById.find(objectId);
    return it != objectCatalog_.objectsById.end() ? &it->second : nullptr;
}

std::vector<HitCircle> Game::selectedHitboxEditCircles() const
{
    if (hitboxEditTab_ == HitboxEditTab::Player) {
        if (playerHitboxSelectedIndex_ < 0 ||
            playerHitboxSelectedIndex_ >= static_cast<int>(playerHitboxIds_.size())) {
            return {};
        }
        return playerHitboxEditCirclesFor(hitboxes_, balance_);
    }

    if (hitboxEditTab_ == HitboxEditTab::Objects) {
        if (const ObjectDefinition* object = selectedObjectHitboxDefinitionForEdit()) {
            return objectHitboxEditCirclesFor(hitboxes_, *object);
        }
        return {};
    }

    if (const EnemyDefinition* definition = selectedEnemyHitboxDefinitionForEdit()) {
        return enemyHitboxEditCirclesFor(hitboxes_, *definition, balance_, enemyHitboxDirection_);
    }
    return {};
}

HitboxProfile* Game::ensureSelectedHitboxEditProfile()
{
    if (hitboxEditTab_ == HitboxEditTab::Player) {
        if (playerHitboxSelectedIndex_ < 0 ||
            playerHitboxSelectedIndex_ >= static_cast<int>(playerHitboxIds_.size())) {
            return nullptr;
        }
        HitboxProfile& profile = mutablePlayerHitboxProfile(hitboxes_);
        if (profile.circles.empty()) {
            profile = fallbackPlayerHitboxProfileFor(balance_);
        }
        enemyHitboxSelectedCircleIndex_ = std::clamp(
            enemyHitboxSelectedCircleIndex_,
            0,
            std::max(0, static_cast<int>(profile.circles.size()) - 1));
        return &profile;
    }

    if (hitboxEditTab_ == HitboxEditTab::Objects) {
        const ObjectDefinition* object = selectedObjectHitboxDefinitionForEdit();
        if (object == nullptr || object->id.empty()) {
            return nullptr;
        }

        HitboxProfile& profile = hitboxes_.objects[object->id];
        if (profile.circles.empty()) {
            profile = fallbackObjectHitboxProfileFor(*object);
        }
        enemyHitboxSelectedCircleIndex_ = std::clamp(
            enemyHitboxSelectedCircleIndex_,
            0,
            std::max(0, static_cast<int>(profile.circles.size()) - 1));
        return &profile;
    }

    const EnemyDefinition* definition = selectedEnemyHitboxDefinitionForEdit();
    if (definition == nullptr || definition->id.empty()) {
        return nullptr;
    }

    const std::vector<HitCircle> inheritedCircles =
        enemyHitboxEditCirclesFor(hitboxes_, *definition, balance_, enemyHitboxDirection_);
    HitboxProfile& profile = mutableEnemyHitboxProfile(hitboxes_, definition->id, enemyHitboxDirection_);
    if (profile.circles.empty()) {
        profile.circles = inheritedCircles;
    }
    enemyHitboxSelectedCircleIndex_ = std::clamp(
        enemyHitboxSelectedCircleIndex_,
        0,
        std::max(0, static_cast<int>(profile.circles.size()) - 1));
    return &profile;
}

bool Game::copyCurrentHitboxEditProfile()
{
    enemyHitboxClipboard_ = selectedHitboxEditCircles();
    normalizeHitboxEditorCircles(enemyHitboxClipboard_);
    if (enemyHitboxClipboard_.empty()) {
        enemyHitboxStatus_ = "Nothing to copy";
        return false;
    }

    enemyHitboxStatus_ = "Single hitbox copied";
    return true;
}

bool Game::pasteCurrentHitboxEditProfile(bool mirrorX)
{
    if (enemyHitboxClipboard_.empty()) {
        enemyHitboxStatus_ = "Single clipboard empty";
        return false;
    }
    if (selectedHitboxEditCircles().empty()) {
        enemyHitboxStatus_ = "No target";
        return false;
    }

    pushHitboxEditUndoSnapshot();
    HitboxProfile* targetProfile = ensureSelectedHitboxEditProfile();
    if (targetProfile == nullptr) {
        enemyHitboxStatus_ = "No target";
        return false;
    }

    targetProfile->circles = mirrorX
        ? mirroredHitboxEditorCircles(enemyHitboxClipboard_)
        : enemyHitboxClipboard_;
    normalizeHitboxEditorCircles(targetProfile->circles);
    enemyHitboxSelectedCircleIndex_ = targetProfile->circles.empty() ? -1 : 0;
    enemyHitboxDirty_ = true;
    enemies_.setHitboxCatalog(&hitboxes_);
    enemyHitboxStatus_ = mirrorX ? "Single hitbox mirrored" : "Single hitbox pasted";
    return true;
}

bool Game::copyEnemyHitboxAllDirectionProfiles()
{
    if (hitboxEditTab_ != HitboxEditTab::Enemies) {
        enemyHitboxStatus_ = "Enemies only";
        return false;
    }

    const EnemyDefinition* definition = selectedEnemyHitboxDefinitionForEdit();
    if (definition == nullptr || definition->id.empty()) {
        enemyHitboxStatus_ = "No target";
        return false;
    }

    enemyHitboxAllDirectionClipboard_ = {};
    const auto it = hitboxes_.enemies.find(definition->id);
    if (it != hitboxes_.enemies.end()) {
        for (int i = 0; i < HitboxDirectionCount; ++i) {
            const HitboxProfile& profile = it->second.directions[static_cast<std::size_t>(i)];
            if (profile.circles.empty()) {
                continue;
            }

            enemyHitboxAllDirectionClipboard_.hasProfile[static_cast<std::size_t>(i)] = true;
            enemyHitboxAllDirectionClipboard_.circles[static_cast<std::size_t>(i)] = profile.circles;
            normalizeHitboxEditorCircles(enemyHitboxAllDirectionClipboard_.circles[static_cast<std::size_t>(i)]);
        }
    }

    if (!enemyHitboxDirectionClipboardHasAny(enemyHitboxAllDirectionClipboard_)) {
        enemyHitboxStatus_ = "No custom directions";
        return false;
    }

    enemyHitboxStatus_ = "All directions copied";
    return true;
}

bool Game::pasteEnemyHitboxAllDirectionProfiles(bool mirrorX)
{
    if (hitboxEditTab_ != HitboxEditTab::Enemies) {
        enemyHitboxStatus_ = "Enemies only";
        return false;
    }
    if (!enemyHitboxDirectionClipboardHasAny(enemyHitboxAllDirectionClipboard_)) {
        enemyHitboxStatus_ = "All-direction clipboard empty";
        return false;
    }

    const EnemyDefinition* definition = selectedEnemyHitboxDefinitionForEdit();
    if (definition == nullptr || definition->id.empty()) {
        enemyHitboxStatus_ = "No target";
        return false;
    }

    pushHitboxEditUndoSnapshot();
    EnemyHitboxProfiles& profiles = hitboxes_.enemies[definition->id];
    for (HitboxProfile& profile : profiles.directions) {
        profile.circles.clear();
    }

    for (int i = 0; i < HitboxDirectionCount; ++i) {
        const std::size_t sourceIndex = static_cast<std::size_t>(i);
        if (!enemyHitboxAllDirectionClipboard_.hasProfile[sourceIndex]) {
            continue;
        }

        const HitboxDirection sourceDirection = hitboxDirectionForEditorIndex(i);
        const HitboxDirection targetDirection = mirrorX
            ? mirroredHitboxDirection(sourceDirection)
            : sourceDirection;
        std::vector<HitCircle> circles = enemyHitboxAllDirectionClipboard_.circles[sourceIndex];
        if (mirrorX) {
            circles = mirroredHitboxEditorCircles(std::move(circles));
        } else {
            normalizeHitboxEditorCircles(circles);
        }
        profiles.directions[static_cast<std::size_t>(hitboxDirectionIndex(targetDirection))].circles = std::move(circles);
    }

    enemyHitboxSelectedCircleIndex_ = 0;
    enemyHitboxDirty_ = true;
    enemies_.setHitboxCatalog(&hitboxes_);
    enemyHitboxStatus_ = mirrorX ? "All directions mirrored" : "All directions pasted";
    return true;
}

bool Game::handleEnemyHitboxEditEvent(const SDL_Event& event)
{
    if (mode_ != ScreenMode::EnemyHitboxEdit) {
        return false;
    }

    auto selectedSubjectCircles = [this]() -> std::vector<HitCircle> {
        return selectedHitboxEditCircles();
    };
    auto ensureProfile = [this]() -> HitboxProfile* {
        return ensureSelectedHitboxEditProfile();
    };

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        const SDL_Keymod mods = SDL_GetModState();
        const bool ctrlDown = (mods & SDL_KMOD_CTRL) != 0;
        const bool shiftDown = (mods & SDL_KMOD_SHIFT) != 0;
        const bool altDown = (mods & SDL_KMOD_ALT) != 0;
        if (ctrlDown && event.key.scancode == SDL_SCANCODE_Z) {
            enemyHitboxStatus_ = undoHitboxEdit() ? "Undo" : "Nothing to undo";
            return true;
        }
        if (ctrlDown && event.key.scancode == SDL_SCANCODE_Y) {
            enemyHitboxStatus_ = redoHitboxEdit() ? "Redo" : "Nothing to redo";
            return true;
        }
        if (ctrlDown && event.key.scancode == SDL_SCANCODE_C) {
            if (shiftDown && hitboxEditTab_ == HitboxEditTab::Enemies) {
                (void)copyEnemyHitboxAllDirectionProfiles();
            } else {
                (void)copyCurrentHitboxEditProfile();
            }
            return true;
        }
        if (ctrlDown && event.key.scancode == SDL_SCANCODE_V) {
            if (shiftDown && hitboxEditTab_ == HitboxEditTab::Enemies) {
                (void)pasteEnemyHitboxAllDirectionProfiles(altDown);
            } else {
                (void)pasteCurrentHitboxEditProfile(altDown);
            }
            return true;
        }
        if (event.key.scancode == SDL_SCANCODE_A) {
            const std::vector<HitCircle> currentCircles = selectedSubjectCircles();
            if (!currentCircles.empty() && static_cast<int>(currentCircles.size()) < HitboxMaxCircles) {
                pushHitboxEditUndoSnapshot();
                if (HitboxProfile* profile = ensureProfile()) {
                    profile->circles.push_back({{}, 10.0f});
                    enemyHitboxSelectedCircleIndex_ = static_cast<int>(profile->circles.size()) - 1;
                    enemyHitboxDirty_ = true;
                    enemies_.setHitboxCatalog(&hitboxes_);
                    enemyHitboxStatus_ = "Circle added";
                }
            } else if (!currentCircles.empty()) {
                enemyHitboxStatus_ = "Circle limit reached";
            }
            return true;
        }
        if (event.key.scancode == SDL_SCANCODE_DELETE) {
            const std::vector<HitCircle> currentCircles = selectedSubjectCircles();
            if (!currentCircles.empty() &&
                enemyHitboxSelectedCircleIndex_ >= 0 &&
                enemyHitboxSelectedCircleIndex_ < static_cast<int>(currentCircles.size())) {
                pushHitboxEditUndoSnapshot();
                if (HitboxProfile* profile = ensureProfile()) {
                    profile->circles.erase(profile->circles.begin() + enemyHitboxSelectedCircleIndex_);
                    enemyHitboxSelectedCircleIndex_ = std::min(
                        enemyHitboxSelectedCircleIndex_,
                        static_cast<int>(profile->circles.size()) - 1);
                    if (profile->circles.empty()) {
                        enemyHitboxSelectedCircleIndex_ = -1;
                    }
                    enemyHitboxDirty_ = true;
                    enemies_.setHitboxCatalog(&hitboxes_);
                    enemyHitboxStatus_ = "Circle deleted";
                }
            }
            return true;
        }
    }

    std::string previousSelection;
    if (hitboxEditTab_ == HitboxEditTab::Enemies &&
        enemyHitboxSelectedEnemyIndex_ >= 0 &&
        enemyHitboxSelectedEnemyIndex_ < static_cast<int>(enemyHitboxEnemyIds_.size())) {
        previousSelection = enemyHitboxEnemyIds_[static_cast<std::size_t>(enemyHitboxSelectedEnemyIndex_)];
    } else if (hitboxEditTab_ == HitboxEditTab::Objects &&
        objectHitboxSelectedObjectIndex_ >= 0 &&
        objectHitboxSelectedObjectIndex_ < static_cast<int>(objectHitboxObjectIds_.size())) {
        previousSelection = objectHitboxObjectIds_[static_cast<std::size_t>(objectHitboxSelectedObjectIndex_)];
    } else if (hitboxEditTab_ == HitboxEditTab::Player &&
        playerHitboxSelectedIndex_ >= 0 &&
        playerHitboxSelectedIndex_ < static_cast<int>(playerHitboxIds_.size())) {
        previousSelection = playerHitboxIds_[static_cast<std::size_t>(playerHitboxSelectedIndex_)];
    }
    const std::string previousText = enemyHitboxSearchInput_.text;
    const bool consumed = handleUiTextInputEvent(enemyHitboxSearchInput_, event, 48);
    if (enemyHitboxSearchInput_.text != previousText) {
        applyEnemyHitboxEditFilter(previousSelection);
        auto activeScrollOffset = [this]() -> float& {
            if (hitboxEditTab_ == HitboxEditTab::Player) {
                return playerHitboxScrollOffset_;
            }
            if (hitboxEditTab_ == HitboxEditTab::Objects) {
                return objectHitboxScrollOffset_;
            }
            return enemyHitboxScrollOffset_;
        };
        const int selectedIndex = hitboxEditTab_ == HitboxEditTab::Player
            ? playerHitboxSelectedIndex_
            : (hitboxEditTab_ == HitboxEditTab::Objects ? objectHitboxSelectedObjectIndex_ : enemyHitboxSelectedEnemyIndex_);
        const int itemCount = hitboxEditTab_ == HitboxEditTab::Player
            ? static_cast<int>(playerHitboxIds_.size())
            : (hitboxEditTab_ == HitboxEditTab::Objects
                ? static_cast<int>(objectHitboxObjectIds_.size())
                : static_cast<int>(enemyHitboxEnemyIds_.size()));
        float& scrollOffset = activeScrollOffset();
        scrollOffset = 0.0f;
        const EnemyHitboxEditLayout layout = makeEnemyHitboxEditLayout(camera_.width(), camera_.height());
        keepEnemyHitboxSelectionVisible(
            layout,
            selectedIndex,
            itemCount,
            scrollOffset);
    }
    return consumed;
}

void Game::enterEnemyHitboxEditMode()
{
    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        exitObjectImageScaleEditMode();
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }

    closeDebugItemPicker();
    closeDebugStoryTest();
    if (baseEditEnabled_) {
        exitBaseEditMode();
    }
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    enemyHitboxSearchInput_.text.clear();
    rebuildEnemyHitboxEditList();
    enemyHitboxEditReturnMode_ = mode_;
    if (enemyHitboxEditReturnMode_ == ScreenMode::EnemyHitboxEdit) {
        enemyHitboxEditReturnMode_ = ScreenMode::Playing;
    }
    enemyHitboxScrollOffset_ = std::max(0.0f, enemyHitboxScrollOffset_);
    objectHitboxScrollOffset_ = std::max(0.0f, objectHitboxScrollOffset_);
    playerHitboxScrollOffset_ = std::max(0.0f, playerHitboxScrollOffset_);
    enemyHitboxStatus_ = "Hitbox edit";
    mode_ = ScreenMode::EnemyHitboxEdit;
    focusUiTextInput(enemyHitboxSearchInput_);
}

void Game::exitEnemyHitboxEditMode()
{
    if (mode_ != ScreenMode::EnemyHitboxEdit) {
        return;
    }
    blurUiTextInput(enemyHitboxSearchInput_);
    enemyHitboxDraggingCircle_ = false;
    enemyHitboxDragUndoSnapshotPushed_ = false;
    mode_ = enemyHitboxEditReturnMode_;
    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        mode_ = ScreenMode::Playing;
    }
}

void Game::updateEnemyHitboxEditScreen(const Input& input, UiContext& ui)
{
    if (mode_ != ScreenMode::EnemyHitboxEdit) {
        return;
    }
    if (input.backPressed() || input.pausePressed()) {
        exitEnemyHitboxEditMode();
        return;
    }

    const EnemyHitboxEditLayout layout = makeEnemyHitboxEditLayout(camera_.width(), camera_.height());
    if (ui.pressed(hitboxEditTabRect(layout, 0)) && hitboxEditTab_ != HitboxEditTab::Enemies) {
        hitboxEditTab_ = HitboxEditTab::Enemies;
        enemyHitboxSelectedCircleIndex_ = 0;
        enemyHitboxDraggingCircle_ = false;
        enemyHitboxStatus_ = "Enemies";
        focusUiTextInput(enemyHitboxSearchInput_);
    }
    if (ui.pressed(hitboxEditTabRect(layout, 1)) && hitboxEditTab_ != HitboxEditTab::Objects) {
        hitboxEditTab_ = HitboxEditTab::Objects;
        enemyHitboxSelectedCircleIndex_ = 0;
        enemyHitboxDraggingCircle_ = false;
        enemyHitboxStatus_ = "Objects";
        focusUiTextInput(enemyHitboxSearchInput_);
    }
    if (ui.pressed(hitboxEditTabRect(layout, 2)) && hitboxEditTab_ != HitboxEditTab::Player) {
        hitboxEditTab_ = HitboxEditTab::Player;
        enemyHitboxSelectedCircleIndex_ = 0;
        enemyHitboxDraggingCircle_ = false;
        enemyHitboxStatus_ = "Player";
        focusUiTextInput(enemyHitboxSearchInput_);
    }

    auto activeItemIds = [this]() -> std::vector<std::string>& {
        if (hitboxEditTab_ == HitboxEditTab::Player) {
            return playerHitboxIds_;
        }
        if (hitboxEditTab_ == HitboxEditTab::Objects) {
            return objectHitboxObjectIds_;
        }
        return enemyHitboxEnemyIds_;
    };
    auto activeSelectedIndex = [this]() -> int& {
        if (hitboxEditTab_ == HitboxEditTab::Player) {
            return playerHitboxSelectedIndex_;
        }
        if (hitboxEditTab_ == HitboxEditTab::Objects) {
            return objectHitboxSelectedObjectIndex_;
        }
        return enemyHitboxSelectedEnemyIndex_;
    };
    auto activeScrollOffset = [this]() -> float& {
        if (hitboxEditTab_ == HitboxEditTab::Player) {
            return playerHitboxScrollOffset_;
        }
        if (hitboxEditTab_ == HitboxEditTab::Objects) {
            return objectHitboxScrollOffset_;
        }
        return enemyHitboxScrollOffset_;
    };
    std::vector<std::string>& itemIds = activeItemIds();
    int& selectedIndex = activeSelectedIndex();
    float& scrollOffset = activeScrollOffset();
    int itemCount = static_cast<int>(itemIds.size());
    float maxScroll = enemyHitboxMaxScroll(layout, itemCount);
    scrollOffset = clamp(scrollOffset, 0.0f, maxScroll);

    auto selectedEnemyDefinition = [this]() -> const EnemyDefinition* {
        if (enemyHitboxSelectedEnemyIndex_ < 0 ||
            enemyHitboxSelectedEnemyIndex_ >= static_cast<int>(enemyHitboxEnemyIds_.size())) {
            return nullptr;
        }
        const std::string& enemyId = enemyHitboxEnemyIds_[static_cast<std::size_t>(enemyHitboxSelectedEnemyIndex_)];
        const auto it = enemyCatalog_.enemiesById.find(enemyId);
        return it != enemyCatalog_.enemiesById.end() ? &it->second : nullptr;
    };
    auto selectedObjectDefinition = [this]() -> const ObjectDefinition* {
        if (objectHitboxSelectedObjectIndex_ < 0 ||
            objectHitboxSelectedObjectIndex_ >= static_cast<int>(objectHitboxObjectIds_.size())) {
            return nullptr;
        }
        const std::string& objectId = objectHitboxObjectIds_[static_cast<std::size_t>(objectHitboxSelectedObjectIndex_)];
        const auto it = objectCatalog_.objectsById.find(objectId);
        return it != objectCatalog_.objectsById.end() ? &it->second : nullptr;
    };
    auto selectedSubjectCircles = [this]() -> std::vector<HitCircle> {
        return selectedHitboxEditCircles();
    };
    auto ensureProfile = [this]() -> HitboxProfile* {
        return ensureSelectedHitboxEditProfile();
    };
    auto markDirty = [this](std::string status) {
        enemyHitboxDirty_ = true;
        enemyHitboxStatus_ = std::move(status);
        enemies_.setHitboxCatalog(&hitboxes_);
    };

    if (hitboxEditTab_ == HitboxEditTab::Enemies) {
        for (int i = 0; i < HitboxDirectionCount; ++i) {
            const HitboxDirection direction = hitboxDirectionForEditorIndex(i);
            if (ui.pressed(enemyHitboxDirectionButtonRect(layout, i)) && enemyHitboxDirection_ != direction) {
                enemyHitboxDirection_ = direction;
                enemyHitboxSelectedCircleIndex_ = 0;
                enemyHitboxDraggingCircle_ = false;
                enemyHitboxDragUndoSnapshotPushed_ = false;
                enemyHitboxStatus_ = std::string("Direction: ") + std::string(hitboxDirectionId(direction));
            }
        }
    }

    updateUiTextInput(enemyHitboxSearchInput_, ui, enemyHitboxSearchInputRect(layout));
    if (ui.pressed(enemyHitboxSearchClearButtonRect(layout))) {
        if (!enemyHitboxSearchInput_.text.empty()) {
            std::string previousSelection;
            if (selectedIndex >= 0 && selectedIndex < static_cast<int>(itemIds.size())) {
                previousSelection = itemIds[static_cast<std::size_t>(selectedIndex)];
            }
            enemyHitboxSearchInput_.text.clear();
            applyEnemyHitboxEditFilter(previousSelection);
            itemCount = static_cast<int>(itemIds.size());
            maxScroll = enemyHitboxMaxScroll(layout, itemCount);
            scrollOffset = 0.0f;
            keepEnemyHitboxSelectionVisible(layout, selectedIndex, itemCount, scrollOffset);
        }
        focusUiTextInput(enemyHitboxSearchInput_);
    }

    if (input.saveShortcutPressed() || ui.pressed(enemyHitboxDetailButtonRect(layout, 0))) {
        std::string message;
        if (saveHitboxData(message)) {
            logInfo("Debug: " + message);
        } else {
            logWarning("Debug: " + message);
        }
    }
    if (ui.pressed(enemyHitboxDetailButtonRect(layout, 1))) {
        const std::vector<HitCircle> currentCircles = selectedSubjectCircles();
        if (!currentCircles.empty() && static_cast<int>(currentCircles.size()) < HitboxMaxCircles) {
            pushHitboxEditUndoSnapshot();
            if (HitboxProfile* profile = ensureProfile()) {
                profile->circles.push_back({{}, 10.0f});
                enemyHitboxSelectedCircleIndex_ = static_cast<int>(profile->circles.size()) - 1;
                markDirty("Circle added");
            }
        } else if (!currentCircles.empty()) {
            enemyHitboxStatus_ = "Circle limit reached";
        }
    }
    if (ui.pressed(enemyHitboxDetailButtonRect(layout, 2))) {
        const std::vector<HitCircle> currentCircles = selectedSubjectCircles();
        if (!currentCircles.empty() &&
            enemyHitboxSelectedCircleIndex_ >= 0 &&
            enemyHitboxSelectedCircleIndex_ < static_cast<int>(currentCircles.size())) {
            pushHitboxEditUndoSnapshot();
            if (HitboxProfile* profile = ensureProfile()) {
                profile->circles.erase(profile->circles.begin() + enemyHitboxSelectedCircleIndex_);
                enemyHitboxSelectedCircleIndex_ = std::min(
                    enemyHitboxSelectedCircleIndex_,
                    static_cast<int>(profile->circles.size()) - 1);
                markDirty("Circle deleted");
            }
        }
    }
    if (ui.pressed(enemyHitboxDetailButtonRect(layout, 3))) {
        (void)copyCurrentHitboxEditProfile();
    }
    if (ui.pressed(enemyHitboxDetailButtonRect(layout, 4))) {
        (void)pasteCurrentHitboxEditProfile(false);
    }
    if (ui.pressed(enemyHitboxDetailButtonRect(layout, 5))) {
        (void)pasteCurrentHitboxEditProfile(true);
    }
    if (ui.pressed(enemyHitboxDetailButtonRect(layout, 6))) {
        (void)copyEnemyHitboxAllDirectionProfiles();
    }
    if (ui.pressed(enemyHitboxDetailButtonRect(layout, 7))) {
        (void)pasteEnemyHitboxAllDirectionProfiles(false);
    }
    if (ui.pressed(enemyHitboxDetailButtonRect(layout, 8))) {
        (void)pasteEnemyHitboxAllDirectionProfiles(true);
    }
    if (ui.pressed(enemyHitboxDetailButtonRect(layout, 9))) {
        if (hitboxEditTab_ == HitboxEditTab::Player) {
            if (playerHitboxSelectedIndex_ >= 0 &&
                playerHitboxSelectedIndex_ < static_cast<int>(playerHitboxIds_.size()) &&
                playerHitboxHasProfile(hitboxes_)) {
                pushHitboxEditUndoSnapshot();
                erasePlayerHitboxProfile(hitboxes_);
                enemyHitboxSelectedCircleIndex_ = 0;
                markDirty("Fallback restored");
            }
        } else if (hitboxEditTab_ == HitboxEditTab::Objects) {
            if (const ObjectDefinition* object = selectedObjectDefinition()) {
                if (hitboxes_.objects.find(object->id) != hitboxes_.objects.end()) {
                    pushHitboxEditUndoSnapshot();
                    hitboxes_.objects.erase(object->id);
                    enemyHitboxSelectedCircleIndex_ = 0;
                    markDirty("Fallback restored");
                }
            }
        } else {
            if (const EnemyDefinition* definition = selectedEnemyDefinition()) {
                if (enemyHitboxHasProfile(hitboxes_, definition->id, enemyHitboxDirection_)) {
                    pushHitboxEditUndoSnapshot();
                    eraseEnemyHitboxProfile(hitboxes_, definition->id, enemyHitboxDirection_);
                    enemyHitboxSelectedCircleIndex_ = 0;
                    markDirty("Fallback restored");
                }
            }
        }
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = enemyHitboxListRowRect(layout, i, scrollOffset);
        if (layout.list.contains(ui.mouse()) && ui.pressed(rect)) {
            selectedIndex = i;
            enemyHitboxSelectedCircleIndex_ = 0;
            break;
        }
    }

    const int wheel = input.mouseWheelDelta();
    if (wheel != 0 && layout.list.contains(ui.mouse())) {
        scrollOffset = clamp(
            scrollOffset + static_cast<float>(wheel) * 38.0f,
            0.0f,
            maxScroll);
    }

    const bool hasSubject = hitboxEditTab_ == HitboxEditTab::Player
        ? playerHitboxSelectedIndex_ >= 0 && playerHitboxSelectedIndex_ < static_cast<int>(playerHitboxIds_.size())
        : (hitboxEditTab_ == HitboxEditTab::Objects
            ? selectedObjectDefinition() != nullptr
            : selectedEnemyDefinition() != nullptr);
    const float previewScale = 4.0f;
    const Vec2 previewCenter = layout.preview.pos + layout.preview.size * 0.5f;
    std::vector<HitCircle> circles = selectedSubjectCircles();

    const auto circleAtMouse = [&]() {
        int best = -1;
        float bestDistanceSq = std::numeric_limits<float>::max();
        for (int i = static_cast<int>(circles.size()) - 1; i >= 0; --i) {
            const HitCircle circle = clampEnemyHitboxEditorCircle(circles[static_cast<std::size_t>(i)]);
            const Vec2 center = previewCenter + circle.offset * previewScale;
            const float radius = circle.radius * previewScale;
            const float distanceSq = distanceSquared(ui.mouse(), center);
            const float pickRadius = radius + 8.0f;
            if (distanceSq <= pickRadius * pickRadius && distanceSq < bestDistanceSq) {
                bestDistanceSq = distanceSq;
                best = i;
            }
        }
        return best;
    };

    if (input.mouseLeftPressed() && !ui.pointerConsumed() && layout.preview.contains(ui.mouse()) && hasSubject) {
        const int pickedCircle = circleAtMouse();
        if (pickedCircle >= 0) {
            enemyHitboxSelectedCircleIndex_ = pickedCircle;
            enemyHitboxDraggingCircle_ = true;
            enemyHitboxDragUndoSnapshotPushed_ = false;
            enemyHitboxDragStartMouse_ = ui.mouse();
            enemyHitboxDragStartOffset_ = clampEnemyHitboxEditorCircle(circles[static_cast<std::size_t>(pickedCircle)]).offset;
            ui.consumePointer();
        }
    }
    if (enemyHitboxDraggingCircle_ && input.mouseLeftHeld()) {
        const Vec2 dragOffset = (ui.mouse() - enemyHitboxDragStartMouse_) / previewScale;
        if (lengthSquared(dragOffset) > 0.000001f) {
            if (!enemyHitboxDragUndoSnapshotPushed_) {
                pushHitboxEditUndoSnapshot();
                enemyHitboxDragUndoSnapshotPushed_ = true;
            }
            if (HitboxProfile* profile = ensureProfile()) {
                if (enemyHitboxSelectedCircleIndex_ >= 0 &&
                    enemyHitboxSelectedCircleIndex_ < static_cast<int>(profile->circles.size())) {
                    HitCircle& circle = profile->circles[static_cast<std::size_t>(enemyHitboxSelectedCircleIndex_)];
                    circle.offset = enemyHitboxDragStartOffset_ + dragOffset;
                    circle.offset.x = std::round(circle.offset.x * 2.0f) * 0.5f;
                    circle.offset.y = std::round(circle.offset.y * 2.0f) * 0.5f;
                    circle = clampEnemyHitboxEditorCircle(circle);
                    markDirty("Circle moved");
                }
            }
        }
    }
    if (input.mouseLeftReleased()) {
        enemyHitboxDraggingCircle_ = false;
        enemyHitboxDragUndoSnapshotPushed_ = false;
    }

    if (hasSubject && layout.preview.contains(ui.mouse()) && input.mouseWheelDelta() != 0) {
        const std::vector<HitCircle> currentCircles = selectedSubjectCircles();
        if (enemyHitboxSelectedCircleIndex_ >= 0 &&
            enemyHitboxSelectedCircleIndex_ < static_cast<int>(currentCircles.size())) {
            pushHitboxEditUndoSnapshot();
            if (HitboxProfile* profile = ensureProfile()) {
                HitCircle& circle = profile->circles[static_cast<std::size_t>(enemyHitboxSelectedCircleIndex_)];
                circle.radius += -static_cast<float>(input.mouseWheelDelta()) * EnemyHitboxCircleRadiusStep;
                circle = clampEnemyHitboxEditorCircle(circle);
                markDirty("Circle resized");
            }
        }
    }

    const int moveX = (input.pressed(InputAction::MoveRight) ? 1 : 0) - (input.pressed(InputAction::MoveLeft) ? 1 : 0);
    const int moveY = (input.pressed(InputAction::MoveDown) ? 1 : 0) - (input.pressed(InputAction::MoveUp) ? 1 : 0);
    if ((moveX != 0 || moveY != 0) && hasSubject) {
        const std::vector<HitCircle> currentCircles = selectedSubjectCircles();
        if (enemyHitboxSelectedCircleIndex_ >= 0 &&
            enemyHitboxSelectedCircleIndex_ < static_cast<int>(currentCircles.size())) {
            pushHitboxEditUndoSnapshot();
            if (HitboxProfile* profile = ensureProfile()) {
                HitCircle& circle = profile->circles[static_cast<std::size_t>(enemyHitboxSelectedCircleIndex_)];
                circle.offset.x += static_cast<float>(moveX) * EnemyHitboxCircleStep;
                circle.offset.y += static_cast<float>(moveY) * EnemyHitboxCircleStep;
                circle = clampEnemyHitboxEditorCircle(circle);
                markDirty("Circle moved");
            }
        }
    }
}

void Game::renderEnemyHitboxEditScreen(Renderer& renderer, double totalSeconds) const
{
    renderer.setScreenSpace();

    const EnemyHitboxEditLayout layout = makeEnemyHitboxEditLayout(camera_.width(), camera_.height());
    const bool editingObjects = hitboxEditTab_ == HitboxEditTab::Objects;
    const bool editingPlayer = hitboxEditTab_ == HitboxEditTab::Player;
    const std::vector<std::string>& itemIds = editingPlayer
        ? playerHitboxIds_
        : (editingObjects ? objectHitboxObjectIds_ : enemyHitboxEnemyIds_);
    const std::vector<std::string>& allIds = editingPlayer
        ? playerHitboxAllIds_
        : (editingObjects ? objectHitboxAllObjectIds_ : enemyHitboxAllEnemyIds_);
    const int selectedIndex = editingPlayer
        ? playerHitboxSelectedIndex_
        : (editingObjects ? objectHitboxSelectedObjectIndex_ : enemyHitboxSelectedEnemyIndex_);
    const float scrollOffset = clamp(
        editingPlayer ? playerHitboxScrollOffset_ : (editingObjects ? objectHitboxScrollOffset_ : enemyHitboxScrollOffset_),
        0.0f,
        enemyHitboxMaxScroll(layout, static_cast<int>(itemIds.size())));

    renderer.fillRect(layout.bounds.pos, layout.bounds.size, {10, 12, 18, 255});
    renderer.fillRect({0.0f, 0.0f}, {layout.bounds.size.x, EnemyHitboxHeaderHeight}, {18, 24, 38, 255});
    renderer.fillRect(layout.footer.pos, layout.footer.size, {18, 24, 38, 255});
    renderer.drawText({22.0f, 18.0f}, "当たり判定編集", {245, 245, 252, 255}, 3);
    drawUiRectButton(renderer, hitboxEditTabRect(layout, 0), "Enemies", hitboxEditTab_ == HitboxEditTab::Enemies);
    drawUiRectButton(renderer, hitboxEditTabRect(layout, 1), "Objects", editingObjects);
    drawUiRectButton(renderer, hitboxEditTabRect(layout, 2), "Player", editingPlayer);
    renderer.drawText({410.0f, 42.0f}, "Ctrl+S save / Ctrl+Z,Y undo redo", {198, 206, 222, 255}, 2);
    renderer.drawText({410.0f, 62.0f}, "Ctrl+C,V single / Ctrl+Shift+C,V all / Ctrl+Alt+V mirror", {198, 206, 222, 255}, 1);

    renderer.fillRect(layout.listPanel.pos, layout.listPanel.size, {18, 24, 36, 255});
    renderer.drawRect(layout.listPanel.pos, layout.listPanel.size, {72, 86, 112, 255});
    drawUiTextInput(
        renderer,
        enemyHitboxSearchInputRect(layout),
        enemyHitboxSearchInput_,
        editingPlayer ? "プレイヤーで検索" : (editingObjects ? "アイテム名で検索" : "敵名で検索"),
        {});
    drawUiRectButton(renderer, enemyHitboxSearchClearButtonRect(layout), "消去", false);
    const std::string countText = std::to_string(static_cast<int>(itemIds.size())) + "/" +
        std::to_string(static_cast<int>(allIds.size()));
    renderer.drawText(enemyHitboxSearchCountRect(layout).pos + Vec2{2.0f, 11.0f}, countText, {198, 206, 222, 255}, 2);

    renderer.drawRect(layout.list.pos, layout.list.size, {78, 92, 116, 255});
    renderer.pushClipRect(layout.list.pos, layout.list.size);
    for (int i = 0; i < static_cast<int>(itemIds.size()); ++i) {
        const UiRect rect = enemyHitboxListRowRect(layout, i, scrollOffset);
        if (rect.pos.y + rect.size.y < layout.list.pos.y || rect.pos.y > layout.list.pos.y + layout.list.size.y) {
            continue;
        }
        const bool selected = i == selectedIndex;
        const std::string& id = itemIds[static_cast<std::size_t>(i)];
        const bool customized = editingPlayer
            ? playerHitboxHasProfile(hitboxes_)
            : (editingObjects
                ? hitboxes_.objects.find(id) != hitboxes_.objects.end()
                : enemyHitboxHasAnyProfile(hitboxes_, id));
        std::string name = id;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{44, 58, 92, 255} : Color{24, 30, 44, 255});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 228, 138, 255} : Color{74, 86, 108, 255});
        if (editingPlayer) {
            name = "Player";
            renderer.fillCircle(rect.pos + Vec2{22.0f, 22.0f}, 12.0f, {96, 142, 188, 255});
            renderer.drawCircle(rect.pos + Vec2{22.0f, 22.0f}, 12.0f, {190, 222, 255, 255});
        } else if (editingObjects) {
            const auto objectIt = objectCatalog_.objectsById.find(id);
            if (objectIt != objectCatalog_.objectsById.end()) {
                name = objectImageScaleDisplayName(objectIt->second);
                ObjectImageDrawOptions iconOptions;
                iconOptions.allowUpscale = true;
                iconOptions.outlineEnabled = false;
                (void)drawItemImage(renderer, objectIt->second, rect.pos + Vec2{22.0f, 22.0f}, {34.0f, 34.0f}, iconOptions);
            } else {
                renderer.fillCircle(rect.pos + Vec2{22.0f, 22.0f}, 12.0f, {82, 92, 110, 255});
            }
        } else {
            int imageNumber = 0;
            const auto enemyIt = enemyCatalog_.enemiesById.find(id);
            if (enemyIt != enemyCatalog_.enemiesById.end()) {
                name = enemyHitboxDisplayName(enemyIt->second);
                imageNumber = enemyIt->second.imageNumber;
            }
            EnemyImageDrawOptions iconOptions;
            iconOptions.allowUpscale = true;
            iconOptions.outlineEnabled = false;
            if (imageNumber <= 0 || !drawEnemyImageIcon(renderer, imageNumber, rect.pos + Vec2{22.0f, 22.0f}, {34.0f, 34.0f}, 0.0f, iconOptions)) {
                renderer.fillCircle(rect.pos + Vec2{22.0f, 22.0f}, 12.0f, {82, 92, 110, 255});
            }
        }
        const std::string title = fittedSingleLineText(renderer, name, rect.size.x - 62.0f, 2);
        renderer.drawText(rect.pos + Vec2{44.0f, 6.0f}, title, {232, 236, 245, 255}, 2);
        const std::string subtitle = fittedSingleLineText(renderer, id + (customized ? " *" : ""), rect.size.x - 62.0f, 1);
        renderer.drawText(rect.pos + Vec2{44.0f, 28.0f}, subtitle, customized ? Color{255, 226, 138, 255} : Color{146, 158, 178, 255}, 1);
    }
    renderer.popClipRect();

    renderer.fillRect(layout.previewPanel.pos, layout.previewPanel.size, {16, 21, 32, 255});
    renderer.drawRect(layout.previewPanel.pos, layout.previewPanel.size, {72, 86, 112, 255});
    renderer.fillRect(layout.preview.pos, layout.preview.size, {8, 10, 16, 255});
    renderer.drawRect(layout.preview.pos, layout.preview.size, {58, 70, 92, 255});
    renderer.fillRect(layout.detail.pos, layout.detail.size, {20, 26, 38, 255});
    renderer.drawRect(layout.detail.pos, layout.detail.size, {78, 92, 116, 255});

    const EnemyDefinition* definition = nullptr;
    const ObjectDefinition* object = nullptr;
    const bool selectedPlayer = editingPlayer &&
        playerHitboxSelectedIndex_ >= 0 &&
        playerHitboxSelectedIndex_ < static_cast<int>(playerHitboxIds_.size());
    if (!editingObjects && !editingPlayer && enemyHitboxSelectedEnemyIndex_ >= 0 &&
        enemyHitboxSelectedEnemyIndex_ < static_cast<int>(enemyHitboxEnemyIds_.size())) {
        const auto it = enemyCatalog_.enemiesById.find(enemyHitboxEnemyIds_[static_cast<std::size_t>(enemyHitboxSelectedEnemyIndex_)]);
        if (it != enemyCatalog_.enemiesById.end()) {
            definition = &it->second;
        }
    } else if (editingObjects && objectHitboxSelectedObjectIndex_ >= 0 &&
        objectHitboxSelectedObjectIndex_ < static_cast<int>(objectHitboxObjectIds_.size())) {
        const auto it = objectCatalog_.objectsById.find(objectHitboxObjectIds_[static_cast<std::size_t>(objectHitboxSelectedObjectIndex_)]);
        if (it != objectCatalog_.objectsById.end()) {
            object = &it->second;
        }
    }

    if ((!editingPlayer && definition == nullptr && object == nullptr) || (editingPlayer && !selectedPlayer)) {
        const char* emptyText = editingPlayer
            ? "プレイヤーが選択されていません"
            : (editingObjects ? "アイテムが選択されていません" : "敵が選択されていません");
        renderer.drawText(layout.preview.pos + Vec2{18.0f, 18.0f}, emptyText, {198, 206, 222, 255}, 2);
    } else {
        const Vec2 previewCenter = layout.preview.pos + layout.preview.size * 0.5f;
        std::vector<HitCircle> circles;
        bool customized = false;
        std::string profileStatus = "fallback";
        std::string title;
        std::string id;
        if (editingPlayer) {
            const float playerDrawSize = PlayerSpriteDrawSize * 1.35f;
            const Vec2 playerFootAnchor = previewCenter + Vec2{0.0f, playerDrawSize * (PlayerSpriteAnchorY - 0.5f)};
            if (renderer.hasPlayerSheet()) {
                renderer.drawPlayerSprite(
                    playerSpriteFrameIndex(static_cast<float>(totalSeconds), true),
                    playerFootAnchor,
                    playerDrawSize,
                    false,
                    {255, 255, 255, 255},
                    {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
            } else {
                renderer.fillCircle(previewCenter, balance_.playerRadius * 4.0f, {118, 72, 168, 255});
                renderer.drawLine(previewCenter, previewCenter + Vec2{22.0f, 0.0f}, {235, 210, 255, 255});
            }
            circles = playerHitboxEditCirclesFor(hitboxes_, balance_);
            customized = playerHitboxHasProfile(hitboxes_);
            profileStatus = customized ? "custom" : "fallback";
            title = "Player";
            id = "player";
        } else if (object != nullptr) {
            ObjectImageDrawOptions imageOptions;
            imageOptions.allowUpscale = true;
            imageOptions.scaleMultiplier = 4.0f;
            imageOptions.selectedOutlineEnabled = true;
            imageOptions.selectedOutlineColor = {255, 255, 255, 70};
            imageOptions.selectedOutlinePx = 2;
            if (!drawItemImage(renderer, *object, previewCenter, {96.0f, 96.0f}, imageOptions)) {
                renderer.fillCircle(previewCenter, objectHitboxDefaultRadiusFor(*object) * 4.0f, {92, 102, 120, 255});
            }
            circles = objectHitboxEditCirclesFor(hitboxes_, *object);
            customized = hitboxes_.objects.find(object->id) != hitboxes_.objects.end();
            profileStatus = customized ? "custom" : "fallback";
            title = objectImageScaleDisplayName(*object);
            id = object->id;
        } else {
            const Enemy previewEnemy = makeEnemyHitboxPreviewEnemy(*definition, balance_);
            EnemyImageDrawOptions imageOptions;
            imageOptions.allowUpscale = true;
            imageOptions.scaleMultiplier = 4.0f;
            imageOptions.selectedOutlineEnabled = true;
            imageOptions.selectedOutlineColor = {255, 255, 255, 70};
            imageOptions.selectedOutlinePx = 2;
            imageOptions.directionOverrideEnabled = true;
            imageOptions.directionOverride = hitboxDirectionVector(enemyHitboxDirection_);
            Vec2 imageSize{};
            const bool drewImage = drawEnemyImage(renderer, previewEnemy, previewCenter, static_cast<float>(totalSeconds), imageOptions, &imageSize);
            if (!drewImage) {
                renderer.fillCircle(previewCenter, enemyHitboxDefaultRadiusFor(*definition, balance_) * 4.0f, {92, 102, 120, 255});
            }
            circles = enemyHitboxEditCirclesFor(hitboxes_, *definition, balance_, enemyHitboxDirection_);
            customized = enemyHitboxHasProfile(hitboxes_, definition->id, enemyHitboxDirection_);
            const bool inheritedDefault = !customized &&
                enemyHitboxDirection_ != HitboxDirection::Default &&
                enemyHitboxHasProfile(hitboxes_, definition->id, HitboxDirection::Default);
            profileStatus = customized ? "custom" : (inheritedDefault ? "default" : "fallback");
            title = enemyHitboxDisplayName(*definition);
            id = definition->id;
        }
        renderer.drawLine({layout.preview.pos.x, previewCenter.y}, {layout.preview.pos.x + layout.preview.size.x, previewCenter.y}, {255, 255, 255, 26});
        renderer.drawLine({previewCenter.x, layout.preview.pos.y}, {previewCenter.x, layout.preview.pos.y + layout.preview.size.y}, {255, 255, 255, 26});

        for (int i = 0; i < static_cast<int>(circles.size()); ++i) {
            const HitCircle circle = clampEnemyHitboxEditorCircle(circles[static_cast<std::size_t>(i)]);
            const bool selected = i == enemyHitboxSelectedCircleIndex_;
            const Vec2 center = previewCenter + circle.offset * 4.0f;
            const float radius = circle.radius * 4.0f;
            renderer.fillCircle(center, radius, selected ? Color{255, 214, 88, 54} : Color{92, 196, 255, 42});
            renderer.drawCircle(center, radius, selected ? Color{255, 228, 138, 255} : Color{92, 196, 255, 210});
            renderer.fillCircle(center, 3.5f, selected ? Color{255, 245, 180, 255} : Color{150, 218, 255, 230});
        }

        const std::string fittedTitle = fittedSingleLineText(renderer, title, layout.detail.size.x - 18.0f, 2);
        renderer.drawText(layout.detail.pos + Vec2{10.0f, 10.0f}, fittedTitle, {232, 236, 245, 255}, 2);
        renderer.drawText(layout.detail.pos + Vec2{10.0f, 36.0f}, id, {146, 158, 178, 255}, 1);
        renderer.drawText(layout.detail.pos + Vec2{10.0f, 56.0f}, profileStatus, customized ? Color{255, 226, 138, 255} : Color{146, 158, 178, 255}, 2);
        std::string circleText = "circles " + std::to_string(static_cast<int>(circles.size())) + "/" + std::to_string(HitboxMaxCircles);
        renderer.drawText(layout.detail.pos + Vec2{10.0f, 82.0f}, circleText, {198, 206, 222, 255}, 2);

        if (definition != nullptr) {
            for (int i = 0; i < HitboxDirectionCount; ++i) {
                const HitboxDirection direction = hitboxDirectionForEditorIndex(i);
                const bool active = enemyHitboxDirection_ == direction;
                drawUiRectButton(
                    renderer,
                    enemyHitboxDirectionButtonRect(layout, i),
                    hitboxDirectionDisplayName(direction),
                    active);
            }
        }

        if (enemyHitboxSelectedCircleIndex_ >= 0 && enemyHitboxSelectedCircleIndex_ < static_cast<int>(circles.size())) {
            const HitCircle circle = clampEnemyHitboxEditorCircle(circles[static_cast<std::size_t>(enemyHitboxSelectedCircleIndex_)]);
            char buffer[128];
            std::snprintf(buffer, sizeof(buffer), "x %.1f  y %.1f  r %.1f", circle.offset.x, circle.offset.y, circle.radius);
            renderer.drawText(layout.detail.pos + Vec2{10.0f, EnemyHitboxSelectedCircleInfoTop}, buffer, {198, 206, 222, 255}, 2);
        }
    }

    const char* labels[] = {"保存", "追加", "削除", "単体コピー", "単体貼付", "反転貼付", "全方向コピー", "全方向貼付", "全方向反転", "戻す"};
    for (int i = 0; i < static_cast<int>(sizeof(labels) / sizeof(labels[0])); ++i) {
        drawUiRectButton(renderer, enemyHitboxDetailButtonRect(layout, i), labels[i], false);
    }

    const char* dirty = enemyHitboxDirty_ ? "Unsaved (*)" : "Saved";
    renderer.drawText(layout.footer.pos + Vec2{22.0f, 18.0f}, dirty, enemyHitboxDirty_ ? Color{255, 230, 150, 255} : Color{170, 220, 170, 255}, 2);
    if (!enemyHitboxStatus_.empty()) {
        renderer.drawText(layout.footer.pos + Vec2{190.0f, 18.0f}, enemyHitboxStatus_, {198, 206, 222, 255}, 2);
    }
}

bool Game::loadEnemyShadowData()
{
    std::string message;
    const bool loaded = loadEnemyShadowCatalog(enemyShadowDataPath(), enemyShadows_, message);
    enemies_.setShadowCatalog(&enemyShadows_);
    enemyShadowDirty_ = false;
    enemyShadowEditUndoStack_.clear();
    enemyShadowEditRedoStack_.clear();
    enemyShadowStatus_ = loaded ? message : "Enemy shadow fallback active";
    rebuildEnemyShadowEditList();
    return loaded;
}

bool Game::saveEnemyShadowData(std::string& message)
{
    const bool saved = saveEnemyShadowCatalog(enemyShadowDataPath(), enemyShadows_, message);
    if (saved) {
        enemyShadowDirty_ = false;
    }
    enemyShadowStatus_ = message;
    enemies_.setShadowCatalog(&enemyShadows_);
    return saved;
}

void Game::rebuildEnemyShadowEditList()
{
    std::string previousSelection;
    if (enemyShadowSelectedEnemyIndex_ >= 0 &&
        enemyShadowSelectedEnemyIndex_ < static_cast<int>(enemyShadowEnemyIds_.size())) {
        previousSelection = enemyShadowEnemyIds_[static_cast<std::size_t>(enemyShadowSelectedEnemyIndex_)];
    }

    enemyShadowAllEnemyIds_.clear();
    enemyShadowAllEnemyIds_.reserve(enemyCatalog_.enemies.size());
    std::unordered_set<std::string> seen;
    for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
        if (!enemy.id.empty() && seen.insert(enemy.id).second) {
            enemyShadowAllEnemyIds_.push_back(enemy.id);
        }
    }
    std::sort(enemyShadowAllEnemyIds_.begin(), enemyShadowAllEnemyIds_.end(), [this](const std::string& left, const std::string& right) {
        const auto lhs = enemyCatalog_.enemiesById.find(left);
        const auto rhs = enemyCatalog_.enemiesById.find(right);
        if (lhs == enemyCatalog_.enemiesById.end() || rhs == enemyCatalog_.enemiesById.end()) {
            return left < right;
        }
        if (lhs->second.imageNumber != rhs->second.imageNumber) {
            return lhs->second.imageNumber < rhs->second.imageNumber;
        }
        return left < right;
    });

    applyEnemyShadowEditFilter(previousSelection);
}

void Game::applyEnemyShadowEditFilter(std::string_view preferredSelection)
{
    std::string previousSelection(preferredSelection);
    if (previousSelection.empty() &&
        enemyShadowSelectedEnemyIndex_ >= 0 &&
        enemyShadowSelectedEnemyIndex_ < static_cast<int>(enemyShadowEnemyIds_.size())) {
        previousSelection = enemyShadowEnemyIds_[static_cast<std::size_t>(enemyShadowSelectedEnemyIndex_)];
    }

    enemyShadowEnemyIds_.clear();
    const std::string normalizedQuery = normalizedUiSearchText(enemyShadowSearchInput_.text);
    for (const std::string& enemyId : enemyShadowAllEnemyIds_) {
        const auto it = enemyCatalog_.enemiesById.find(enemyId);
        if (it != enemyCatalog_.enemiesById.end() && enemyHitboxEnemyMatchesSearch(it->second, normalizedQuery)) {
            enemyShadowEnemyIds_.push_back(enemyId);
        }
    }

    enemyShadowSelectedEnemyIndex_ = -1;
    if (!previousSelection.empty()) {
        const auto it = std::find(enemyShadowEnemyIds_.begin(), enemyShadowEnemyIds_.end(), previousSelection);
        if (it != enemyShadowEnemyIds_.end()) {
            enemyShadowSelectedEnemyIndex_ = static_cast<int>(std::distance(enemyShadowEnemyIds_.begin(), it));
        }
    }
    if (enemyShadowSelectedEnemyIndex_ < 0 && !enemyShadowEnemyIds_.empty()) {
        enemyShadowSelectedEnemyIndex_ = 0;
    }
}

EnemyShadowEditSnapshot Game::makeEnemyShadowEditSnapshot() const
{
    EnemyShadowEditSnapshot snapshot;
    snapshot.catalog = enemyShadows_;
    if (enemyShadowSelectedEnemyIndex_ >= 0 &&
        enemyShadowSelectedEnemyIndex_ < static_cast<int>(enemyShadowEnemyIds_.size())) {
        snapshot.selectedId = enemyShadowEnemyIds_[static_cast<std::size_t>(enemyShadowSelectedEnemyIndex_)];
    }
    return snapshot;
}

void Game::restoreEnemyShadowEditSnapshot(const EnemyShadowEditSnapshot& snapshot)
{
    enemyShadows_ = snapshot.catalog;
    enemyShadowDragging_ = false;
    enemyShadowDragUndoSnapshotPushed_ = false;
    applyEnemyShadowEditFilter(snapshot.selectedId);
    enemyShadowDirty_ = true;
    enemies_.setShadowCatalog(&enemyShadows_);
}

void Game::pushEnemyShadowEditUndoSnapshot()
{
    EnemyShadowEditSnapshot snapshot = makeEnemyShadowEditSnapshot();
    if (!enemyShadowEditUndoStack_.empty() && enemyShadowEditUndoStack_.back() == snapshot) {
        return;
    }
    enemyShadowEditUndoStack_.push_back(std::move(snapshot));
    if (static_cast<int>(enemyShadowEditUndoStack_.size()) > HitboxEditUndoLimit) {
        enemyShadowEditUndoStack_.erase(enemyShadowEditUndoStack_.begin());
    }
    enemyShadowEditRedoStack_.clear();
}

bool Game::undoEnemyShadowEdit()
{
    if (enemyShadowEditUndoStack_.empty()) {
        return false;
    }
    enemyShadowEditRedoStack_.push_back(makeEnemyShadowEditSnapshot());
    const EnemyShadowEditSnapshot snapshot = std::move(enemyShadowEditUndoStack_.back());
    enemyShadowEditUndoStack_.pop_back();
    restoreEnemyShadowEditSnapshot(snapshot);
    return true;
}

bool Game::redoEnemyShadowEdit()
{
    if (enemyShadowEditRedoStack_.empty()) {
        return false;
    }
    enemyShadowEditUndoStack_.push_back(makeEnemyShadowEditSnapshot());
    if (static_cast<int>(enemyShadowEditUndoStack_.size()) > HitboxEditUndoLimit) {
        enemyShadowEditUndoStack_.erase(enemyShadowEditUndoStack_.begin());
    }
    const EnemyShadowEditSnapshot snapshot = std::move(enemyShadowEditRedoStack_.back());
    enemyShadowEditRedoStack_.pop_back();
    restoreEnemyShadowEditSnapshot(snapshot);
    return true;
}

const EnemyDefinition* Game::selectedEnemyShadowDefinitionForEdit() const
{
    if (enemyShadowSelectedEnemyIndex_ < 0 ||
        enemyShadowSelectedEnemyIndex_ >= static_cast<int>(enemyShadowEnemyIds_.size())) {
        return nullptr;
    }
    const std::string& enemyId = enemyShadowEnemyIds_[static_cast<std::size_t>(enemyShadowSelectedEnemyIndex_)];
    const auto it = enemyCatalog_.enemiesById.find(enemyId);
    return it != enemyCatalog_.enemiesById.end() ? &it->second : nullptr;
}

EnemyShadowSpec Game::selectedEnemyShadowSpecForEdit() const
{
    if (const EnemyDefinition* definition = selectedEnemyShadowDefinitionForEdit()) {
        return resolvedEnemyShadowSpec(&enemyShadows_, definition->id);
    }
    return defaultEnemyShadowSpec();
}

EnemyShadowSpec& Game::mutableSelectedEnemyShadowSpecForEdit()
{
    static EnemyShadowSpec fallback = defaultEnemyShadowSpec();
    const EnemyDefinition* definition = selectedEnemyShadowDefinitionForEdit();
    if (definition == nullptr || definition->id.empty()) {
        fallback = defaultEnemyShadowSpec();
        return fallback;
    }
    EnemyShadowSpec& spec = enemyShadows_.enemies[definition->id];
    spec = selectedEnemyShadowSpecForEdit();
    return spec;
}

bool Game::copyCurrentEnemyShadowSpec()
{
    if (selectedEnemyShadowDefinitionForEdit() == nullptr) {
        enemyShadowStatus_ = "No target";
        return false;
    }

    enemyShadowClipboard_ = sanitizeEnemyShadowSpec(selectedEnemyShadowSpecForEdit());
    enemyShadowClipboardValid_ = true;
    enemyShadowStatus_ = "Shadow copied";
    return true;
}

bool Game::pasteCurrentEnemyShadowSpec(bool mirrorX)
{
    if (!enemyShadowClipboardValid_) {
        enemyShadowStatus_ = "Shadow clipboard empty";
        return false;
    }
    if (selectedEnemyShadowDefinitionForEdit() == nullptr) {
        enemyShadowStatus_ = "No target";
        return false;
    }

    pushEnemyShadowEditUndoSnapshot();
    EnemyShadowSpec& targetSpec = mutableSelectedEnemyShadowSpecForEdit();
    targetSpec = mirrorX
        ? mirroredEnemyShadowSpec(enemyShadowClipboard_)
        : sanitizeEnemyShadowSpec(enemyShadowClipboard_);
    enemyShadowDirty_ = true;
    enemies_.setShadowCatalog(&enemyShadows_);
    enemyShadowStatus_ = mirrorX ? "Shadow mirrored" : "Shadow pasted";
    return true;
}

bool Game::handleEnemyShadowEditEvent(const SDL_Event& event)
{
    if (mode_ != ScreenMode::EnemyShadowEdit) {
        return false;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        const SDL_Keymod mods = SDL_GetModState();
        const bool ctrlDown = (mods & SDL_KMOD_CTRL) != 0;
        const bool altDown = (mods & SDL_KMOD_ALT) != 0;
        if (ctrlDown && event.key.scancode == SDL_SCANCODE_Z) {
            enemyShadowStatus_ = undoEnemyShadowEdit() ? "Undo" : "Nothing to undo";
            return true;
        }
        if (ctrlDown && event.key.scancode == SDL_SCANCODE_Y) {
            enemyShadowStatus_ = redoEnemyShadowEdit() ? "Redo" : "Nothing to redo";
            return true;
        }
        if (ctrlDown && event.key.scancode == SDL_SCANCODE_C) {
            (void)copyCurrentEnemyShadowSpec();
            return true;
        }
        if (ctrlDown && event.key.scancode == SDL_SCANCODE_V) {
            (void)pasteCurrentEnemyShadowSpec(altDown);
            return true;
        }
    }

    std::string previousSelection;
    if (enemyShadowSelectedEnemyIndex_ >= 0 &&
        enemyShadowSelectedEnemyIndex_ < static_cast<int>(enemyShadowEnemyIds_.size())) {
        previousSelection = enemyShadowEnemyIds_[static_cast<std::size_t>(enemyShadowSelectedEnemyIndex_)];
    }
    const std::string previousText = enemyShadowSearchInput_.text;
    const bool consumed = handleUiTextInputEvent(enemyShadowSearchInput_, event, 48);
    if (enemyShadowSearchInput_.text != previousText) {
        applyEnemyShadowEditFilter(previousSelection);
        enemyShadowScrollOffset_ = 0.0f;
        const EnemyHitboxEditLayout layout = makeEnemyHitboxEditLayout(camera_.width(), camera_.height());
        keepEnemyHitboxSelectionVisible(layout, enemyShadowSelectedEnemyIndex_, static_cast<int>(enemyShadowEnemyIds_.size()), enemyShadowScrollOffset_);
    }
    return consumed;
}

void Game::enterEnemyShadowEditMode()
{
    if (mode_ == ScreenMode::EnemyShadowEdit) {
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        exitObjectImageScaleEditMode();
    }
    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        exitEnemyHitboxEditMode();
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }

    closeDebugItemPicker();
    closeDebugStoryTest();
    if (baseEditEnabled_) {
        exitBaseEditMode();
    }
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    enemyShadowSearchInput_.text.clear();
    rebuildEnemyShadowEditList();
    enemyShadowEditReturnMode_ = mode_;
    if (enemyShadowEditReturnMode_ == ScreenMode::EnemyShadowEdit) {
        enemyShadowEditReturnMode_ = ScreenMode::Playing;
    }
    enemyShadowScrollOffset_ = std::max(0.0f, enemyShadowScrollOffset_);
    enemyShadowStatus_ = "Shadow edit";
    mode_ = ScreenMode::EnemyShadowEdit;
    focusUiTextInput(enemyShadowSearchInput_);
}

void Game::exitEnemyShadowEditMode()
{
    if (mode_ != ScreenMode::EnemyShadowEdit) {
        return;
    }
    blurUiTextInput(enemyShadowSearchInput_);
    enemyShadowDragging_ = false;
    enemyShadowDragUndoSnapshotPushed_ = false;
    mode_ = enemyShadowEditReturnMode_;
    if (mode_ == ScreenMode::EnemyShadowEdit) {
        mode_ = ScreenMode::Playing;
    }
}

void Game::updateEnemyShadowEditScreen(const Input& input, UiContext& ui)
{
    if (mode_ != ScreenMode::EnemyShadowEdit) {
        return;
    }
    if (input.backPressed() || input.pausePressed()) {
        exitEnemyShadowEditMode();
        return;
    }

    const EnemyHitboxEditLayout layout = makeEnemyHitboxEditLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(enemyShadowEnemyIds_.size());
    const float maxScroll = enemyHitboxMaxScroll(layout, itemCount);
    enemyShadowScrollOffset_ = clamp(enemyShadowScrollOffset_, 0.0f, maxScroll);

    auto markDirty = [this](std::string status) {
        enemyShadowDirty_ = true;
        enemyShadowStatus_ = std::move(status);
        enemies_.setShadowCatalog(&enemyShadows_);
    };

    updateUiTextInput(enemyShadowSearchInput_, ui, enemyHitboxSearchInputRect(layout));
    if (ui.pressed(enemyHitboxSearchClearButtonRect(layout))) {
        if (!enemyShadowSearchInput_.text.empty()) {
            std::string previousSelection;
            if (enemyShadowSelectedEnemyIndex_ >= 0 && enemyShadowSelectedEnemyIndex_ < static_cast<int>(enemyShadowEnemyIds_.size())) {
                previousSelection = enemyShadowEnemyIds_[static_cast<std::size_t>(enemyShadowSelectedEnemyIndex_)];
            }
            enemyShadowSearchInput_.text.clear();
            applyEnemyShadowEditFilter(previousSelection);
            enemyShadowScrollOffset_ = 0.0f;
        }
        focusUiTextInput(enemyShadowSearchInput_);
    }

    if (input.saveShortcutPressed() || ui.pressed(enemyHitboxDetailButtonRect(layout, 0))) {
        std::string message;
        if (saveEnemyShadowData(message)) {
            logInfo("Debug: " + message);
        } else {
            logWarning("Debug: " + message);
        }
    }

    const bool hasSubject = selectedEnemyShadowDefinitionForEdit() != nullptr;
    if (hasSubject) {
        auto adjustScale = [&](float dx, float dy, std::string status) {
            pushEnemyShadowEditUndoSnapshot();
            EnemyShadowSpec& spec = mutableSelectedEnemyShadowSpecForEdit();
            spec.scale.x += dx;
            spec.scale.y += dy;
            spec = sanitizeEnemyShadowSpec(spec);
            markDirty(std::move(status));
        };
        if (ui.pressed(enemyHitboxDetailButtonRect(layout, 1))) {
            adjustScale(-EnemyShadowScaleStep, 0.0f, "Width resized");
        }
        if (ui.pressed(enemyHitboxDetailButtonRect(layout, 2))) {
            adjustScale(EnemyShadowScaleStep, 0.0f, "Width resized");
        }
        if (ui.pressed(enemyHitboxDetailButtonRect(layout, 3))) {
            adjustScale(0.0f, -EnemyShadowScaleStep, "Height resized");
        }
        if (ui.pressed(enemyHitboxDetailButtonRect(layout, 4))) {
            adjustScale(0.0f, EnemyShadowScaleStep, "Height resized");
        }
        if (ui.pressed(enemyHitboxDetailButtonRect(layout, 5))) {
            (void)copyCurrentEnemyShadowSpec();
        }
        if (ui.pressed(enemyHitboxDetailButtonRect(layout, 6))) {
            (void)pasteCurrentEnemyShadowSpec(false);
        }
        if (ui.pressed(enemyHitboxDetailButtonRect(layout, 7))) {
            (void)pasteCurrentEnemyShadowSpec(true);
        }
        if (ui.pressed(enemyHitboxDetailButtonRect(layout, 8))) {
            const EnemyDefinition* definition = selectedEnemyShadowDefinitionForEdit();
            if (definition != nullptr && enemyShadows_.enemies.find(definition->id) != enemyShadows_.enemies.end()) {
                pushEnemyShadowEditUndoSnapshot();
                eraseEnemyShadowSpec(enemyShadows_, definition->id);
                markDirty("Fallback restored");
            }
        }
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = enemyHitboxListRowRect(layout, i, enemyShadowScrollOffset_);
        if (layout.list.contains(ui.mouse()) && ui.pressed(rect)) {
            enemyShadowSelectedEnemyIndex_ = i;
            break;
        }
    }

    const int wheel = input.mouseWheelDelta();
    if (wheel != 0 && layout.list.contains(ui.mouse())) {
        enemyShadowScrollOffset_ = clamp(enemyShadowScrollOffset_ + static_cast<float>(wheel) * 38.0f, 0.0f, maxScroll);
    }

    const Vec2 previewCenter = layout.preview.pos + layout.preview.size * 0.5f;
    const EnemyShadowSpec currentSpec = selectedEnemyShadowSpecForEdit();
    const Vec2 shadowCenter = previewCenter + currentSpec.offset * EnemyShadowPreviewScale;
    const float pickRadius = 42.0f;
    if (input.mouseLeftPressed() && !ui.pointerConsumed() && layout.preview.contains(ui.mouse()) && hasSubject) {
        if (distanceSquared(ui.mouse(), shadowCenter) <= pickRadius * pickRadius) {
            enemyShadowDragging_ = true;
            enemyShadowDragUndoSnapshotPushed_ = false;
            enemyShadowDragStartMouse_ = ui.mouse();
            enemyShadowDragStartOffset_ = currentSpec.offset;
            ui.consumePointer();
        }
    }
    if (enemyShadowDragging_ && input.mouseLeftHeld()) {
        const Vec2 dragOffset = (ui.mouse() - enemyShadowDragStartMouse_) / EnemyShadowPreviewScale;
        if (lengthSquared(dragOffset) > 0.000001f) {
            if (!enemyShadowDragUndoSnapshotPushed_) {
                pushEnemyShadowEditUndoSnapshot();
                enemyShadowDragUndoSnapshotPushed_ = true;
            }
            EnemyShadowSpec& spec = mutableSelectedEnemyShadowSpecForEdit();
            spec.offset = enemyShadowDragStartOffset_ + dragOffset;
            spec.offset.x = std::round(spec.offset.x * 2.0f) * 0.5f;
            spec.offset.y = std::round(spec.offset.y * 2.0f) * 0.5f;
            spec = sanitizeEnemyShadowSpec(spec);
            markDirty("Shadow moved");
        }
    }
    if (input.mouseLeftReleased()) {
        enemyShadowDragging_ = false;
        enemyShadowDragUndoSnapshotPushed_ = false;
    }

    if (hasSubject && layout.preview.contains(ui.mouse()) && input.mouseWheelDelta() != 0) {
        pushEnemyShadowEditUndoSnapshot();
        EnemyShadowSpec& spec = mutableSelectedEnemyShadowSpecForEdit();
        const float delta = -static_cast<float>(input.mouseWheelDelta()) * EnemyShadowScaleStep;
        spec.scale.x += delta;
        spec.scale.y += delta;
        spec = sanitizeEnemyShadowSpec(spec);
        markDirty("Shadow resized");
    }

    const int moveX = (input.pressed(InputAction::MoveRight) ? 1 : 0) - (input.pressed(InputAction::MoveLeft) ? 1 : 0);
    const int moveY = (input.pressed(InputAction::MoveDown) ? 1 : 0) - (input.pressed(InputAction::MoveUp) ? 1 : 0);
    if ((moveX != 0 || moveY != 0) && hasSubject) {
        pushEnemyShadowEditUndoSnapshot();
        EnemyShadowSpec& spec = mutableSelectedEnemyShadowSpecForEdit();
        spec.offset.x += static_cast<float>(moveX) * EnemyShadowOffsetStep;
        spec.offset.y += static_cast<float>(moveY) * EnemyShadowOffsetStep;
        spec = sanitizeEnemyShadowSpec(spec);
        markDirty("Shadow moved");
    }
}

void Game::renderEnemyShadowEditScreen(Renderer& renderer, double totalSeconds) const
{
    renderer.setScreenSpace();

    const EnemyHitboxEditLayout layout = makeEnemyHitboxEditLayout(camera_.width(), camera_.height());
    const int selectedIndex = enemyShadowSelectedEnemyIndex_;
    const float scrollOffset = clamp(enemyShadowScrollOffset_, 0.0f, enemyHitboxMaxScroll(layout, static_cast<int>(enemyShadowEnemyIds_.size())));

    renderer.fillRect(layout.bounds.pos, layout.bounds.size, {10, 12, 18, 255});
    renderer.fillRect({0.0f, 0.0f}, {layout.bounds.size.x, EnemyHitboxHeaderHeight}, {18, 24, 38, 255});
    renderer.fillRect(layout.footer.pos, layout.footer.size, {18, 24, 38, 255});
    renderer.drawText({22.0f, 18.0f}, "影編集", {245, 245, 252, 255}, 3);
    renderer.drawText({220.0f, 42.0f}, "Ctrl+S save / Ctrl+Z,Y undo redo / Ctrl+C,V copy paste", {198, 206, 222, 255}, 2);
    renderer.drawText({220.0f, 62.0f}, "Drag shadow / Wheel scale / Arrow move / Alt+Ctrl+V mirror paste", {198, 206, 222, 255}, 1);

    renderer.fillRect(layout.listPanel.pos, layout.listPanel.size, {18, 24, 36, 255});
    renderer.drawRect(layout.listPanel.pos, layout.listPanel.size, {72, 86, 112, 255});
    drawUiTextInput(renderer, enemyHitboxSearchInputRect(layout), enemyShadowSearchInput_, "敵名で検索", {});
    drawUiRectButton(renderer, enemyHitboxSearchClearButtonRect(layout), "消去", false);
    const std::string countText = std::to_string(static_cast<int>(enemyShadowEnemyIds_.size())) + "/" + std::to_string(static_cast<int>(enemyShadowAllEnemyIds_.size()));
    renderer.drawText(enemyHitboxSearchCountRect(layout).pos + Vec2{2.0f, 11.0f}, countText, {198, 206, 222, 255}, 2);

    renderer.drawRect(layout.list.pos, layout.list.size, {78, 92, 116, 255});
    renderer.pushClipRect(layout.list.pos, layout.list.size);
    for (int i = 0; i < static_cast<int>(enemyShadowEnemyIds_.size()); ++i) {
        const UiRect rect = enemyHitboxListRowRect(layout, i, scrollOffset);
        if (rect.pos.y + rect.size.y < layout.list.pos.y || rect.pos.y > layout.list.pos.y + layout.list.size.y) {
            continue;
        }
        const bool selected = i == selectedIndex;
        const std::string& id = enemyShadowEnemyIds_[static_cast<std::size_t>(i)];
        const bool customized = enemyShadows_.enemies.find(id) != enemyShadows_.enemies.end();
        std::string name = id;
        int imageNumber = 0;
        const auto enemyIt = enemyCatalog_.enemiesById.find(id);
        if (enemyIt != enemyCatalog_.enemiesById.end()) {
            name = enemyHitboxDisplayName(enemyIt->second);
            imageNumber = enemyIt->second.imageNumber;
        }
        renderer.fillRect(rect.pos, rect.size, selected ? Color{44, 58, 92, 255} : Color{24, 30, 44, 255});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 228, 138, 255} : Color{74, 86, 108, 255});
        EnemyImageDrawOptions iconOptions;
        iconOptions.allowUpscale = true;
        iconOptions.outlineEnabled = false;
        if (imageNumber <= 0 || !drawEnemyImageIcon(renderer, imageNumber, rect.pos + Vec2{22.0f, 22.0f}, {34.0f, 34.0f}, 0.0f, iconOptions)) {
            renderer.fillCircle(rect.pos + Vec2{22.0f, 22.0f}, 12.0f, {82, 92, 110, 255});
        }
        renderer.drawText(rect.pos + Vec2{44.0f, 6.0f}, fittedSingleLineText(renderer, name, rect.size.x - 62.0f, 2), {232, 236, 245, 255}, 2);
        renderer.drawText(rect.pos + Vec2{44.0f, 28.0f}, fittedSingleLineText(renderer, id + (customized ? " *" : ""), rect.size.x - 62.0f, 1), customized ? Color{255, 226, 138, 255} : Color{146, 158, 178, 255}, 1);
    }
    renderer.popClipRect();

    renderer.fillRect(layout.previewPanel.pos, layout.previewPanel.size, {16, 21, 32, 255});
    renderer.drawRect(layout.previewPanel.pos, layout.previewPanel.size, {72, 86, 112, 255});
    renderer.fillRect(layout.preview.pos, layout.preview.size, {196, 202, 210, 255});
    renderer.drawRect(layout.preview.pos, layout.preview.size, {110, 120, 132, 255});
    renderer.fillRect(layout.detail.pos, layout.detail.size, {20, 26, 38, 255});
    renderer.drawRect(layout.detail.pos, layout.detail.size, {78, 92, 116, 255});

    const EnemyDefinition* definition = selectedEnemyShadowDefinitionForEdit();
    if (definition == nullptr) {
        renderer.drawText(layout.preview.pos + Vec2{18.0f, 18.0f}, "敵が選択されていません", {198, 206, 222, 255}, 2);
    } else {
        const Vec2 previewCenter = layout.preview.pos + layout.preview.size * 0.5f;
        Enemy previewEnemy = makeEnemyHitboxPreviewEnemy(*definition, balance_);
        previewEnemy.facingAngle = enemyShadowPreviewFacingAngle(totalSeconds);
        const EnemyShadowSpec spec = selectedEnemyShadowSpecForEdit();
        EnemyImageDrawOptions imageOptions;
        imageOptions.allowUpscale = true;
        imageOptions.scaleMultiplier = EnemyShadowPreviewScale;
        imageOptions.selectedOutlineEnabled = true;
        imageOptions.selectedOutlineColor = {255, 255, 255, 70};
        imageOptions.selectedOutlinePx = 2;
        Vec2 imageSize{};
        const bool sizeResolved = enemyImageDrawSize(renderer, previewEnemy, imageOptions, imageSize);
        if (!sizeResolved) {
            const float fallbackRadius = enemyHitboxDefaultRadiusFor(*definition, balance_);
            imageSize = {fallbackRadius * 2.0f * EnemyShadowPreviewScale, fallbackRadius * 2.0f * EnemyShadowPreviewScale};
        }
        const float baseSize = std::max(1.0f, std::max(imageSize.x, imageSize.y));
        const Vec2 shadowAnchor = previewCenter + spec.offset * EnemyShadowPreviewScale;
        renderer.drawActorShadow(shadowAnchor, baseSize, spec.scale, {0, 0, 0, 100});
        renderer.drawCircle(shadowAnchor + Vec2{0.0f, baseSize * 0.02f}, 5.0f, {255, 228, 138, 255});
        if (!drawEnemyImage(renderer, previewEnemy, previewCenter, static_cast<float>(totalSeconds), imageOptions, &imageSize)) {
            const float fallbackRadius = enemyHitboxDefaultRadiusFor(*definition, balance_);
            renderer.fillCircle(previewCenter, fallbackRadius * EnemyShadowPreviewScale, {92, 102, 120, 255});
        }
        renderer.drawLine({layout.preview.pos.x, previewCenter.y}, {layout.preview.pos.x + layout.preview.size.x, previewCenter.y}, {255, 255, 255, 26});
        renderer.drawLine({previewCenter.x, layout.preview.pos.y}, {previewCenter.x, layout.preview.pos.y + layout.preview.size.y}, {255, 255, 255, 26});

        const bool customized = enemyShadows_.enemies.find(definition->id) != enemyShadows_.enemies.end();
        renderer.drawText(layout.detail.pos + Vec2{10.0f, 10.0f}, fittedSingleLineText(renderer, enemyHitboxDisplayName(*definition), layout.detail.size.x - 18.0f, 2), {232, 236, 245, 255}, 2);
        renderer.drawText(layout.detail.pos + Vec2{10.0f, 36.0f}, definition->id, {146, 158, 178, 255}, 1);
        renderer.drawText(layout.detail.pos + Vec2{10.0f, 56.0f}, customized ? "custom" : "fallback", customized ? Color{255, 226, 138, 255} : Color{146, 158, 178, 255}, 2);
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer), "x %.1f  y %.1f", spec.offset.x, spec.offset.y);
        renderer.drawText(layout.detail.pos + Vec2{10.0f, 86.0f}, buffer, {198, 206, 222, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "w %.2f  h %.2f", spec.scale.x, spec.scale.y);
        renderer.drawText(layout.detail.pos + Vec2{10.0f, 112.0f}, buffer, {198, 206, 222, 255}, 2);
    }

    const char* labels[] = {"保存", "横-", "横+", "縦-", "縦+", "コピー", "貼付", "反転貼付", "戻す"};
    for (int i = 0; i < static_cast<int>(sizeof(labels) / sizeof(labels[0])); ++i) {
        drawUiRectButton(renderer, enemyHitboxDetailButtonRect(layout, i), labels[i], false);
    }

    const char* dirty = enemyShadowDirty_ ? "Unsaved (*)" : "Saved";
    renderer.drawText(layout.footer.pos + Vec2{22.0f, 18.0f}, dirty, enemyShadowDirty_ ? Color{255, 230, 150, 255} : Color{170, 220, 170, 255}, 2);
    if (!enemyShadowStatus_.empty()) {
        renderer.drawText(layout.footer.pos + Vec2{190.0f, 18.0f}, enemyShadowStatus_, {198, 206, 222, 255}, 2);
    }
}

bool Game::loadAudioCueManifestForEdit()
{
    std::string previousId;
    if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < static_cast<int>(audioCueEditEntries_.size())) {
        previousId = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)].id;
    }

    std::vector<AudioCueManifestRow> rows;
    std::string message;
    if (!loadAudioCueManifestRows(rows, message)) {
        audioCueEditEntries_.clear();
        audioCueEditCueIndex_ = -1;
        audioCueEditDirty_ = false;
        audioCueEditStatus_ = message;
        return false;
    }

    audioCueEditEntries_.clear();
    for (const AudioCueManifestRow& row : rows) {
        if (row.valid && audioCueEditEntryMatchesMode(row.entry, audioCueEditMode_)) {
            audioCueEditEntries_.push_back(row.entry);
        }
    }

    audioCueEditCueIndex_ = -1;
    if (!previousId.empty()) {
        for (int i = 0; i < static_cast<int>(audioCueEditEntries_.size()); ++i) {
            if (audioCueEditEntries_[static_cast<std::size_t>(i)].id == previousId) {
                audioCueEditCueIndex_ = i;
                break;
            }
        }
    }
    if (audioCueEditCueIndex_ < 0 && !audioCueEditEntries_.empty()) {
        audioCueEditCueIndex_ = 0;
    }

    if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < static_cast<int>(audioCueEditEntries_.size())) {
        const AudioCueEditEntry& cue = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)];
        const int fileIndex = audioCueEditFileIndexForPath(audioCueEditFiles_, cue.path);
        if (fileIndex >= 0) {
            audioCueEditFileIndex_ = fileIndex;
        }
    }

    audioCueEditDirty_ = false;
    audioCueEditStatus_ = audioCueEditEntries_.empty()
        ? audioCueEditTitle(audioCueEditMode_) + std::string(": cueなし")
        : audioCueEditTitle(audioCueEditMode_) + std::string(": manifest読込");
    return true;
}

bool Game::saveAudioCueManifestFromEdit(std::string& message)
{
    std::vector<AudioCueManifestRow> rows;
    if (!loadAudioCueManifestRows(rows, message)) {
        return false;
    }

    std::vector<bool> saved(audioCueEditEntries_.size(), false);
    for (AudioCueManifestRow& row : rows) {
        if (!row.valid || !audioCueEditEntryMatchesMode(row.entry, audioCueEditMode_)) {
            continue;
        }
        for (int i = 0; i < static_cast<int>(audioCueEditEntries_.size()); ++i) {
            const AudioCueEditEntry& edited = audioCueEditEntries_[static_cast<std::size_t>(i)];
            if (edited.id == row.entry.id) {
                row.entry = edited;
                saved[static_cast<std::size_t>(i)] = true;
                break;
            }
        }
    }

    for (int i = 0; i < static_cast<int>(audioCueEditEntries_.size()); ++i) {
        if (saved[static_cast<std::size_t>(i)]) {
            continue;
        }
        AudioCueManifestRow row;
        row.entry = audioCueEditEntries_[static_cast<std::size_t>(i)];
        row.valid = true;
        rows.push_back(std::move(row));
    }

    if (!writeAudioCueManifestRows(rows, message)) {
        return false;
    }

    audioCueEditDirty_ = false;
    if (audio_ != nullptr && !audio_->reloadManifest()) {
        message += " / reload failed: " + audio_->lastError();
        return false;
    }
    return true;
}

void Game::rebuildAudioCueFileList()
{
    std::string previousPath;
    if (audioCueEditFileIndex_ >= 0 && audioCueEditFileIndex_ < static_cast<int>(audioCueEditFiles_.size())) {
        previousPath = audioCueEditFiles_[static_cast<std::size_t>(audioCueEditFileIndex_)].relativePath;
    } else if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < static_cast<int>(audioCueEditEntries_.size())) {
        previousPath = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)].path;
    }

    audioCueEditFiles_.clear();
    const std::filesystem::path folder = audioCueEditFolderText(audioCueEditMode_);
    const std::filesystem::path audioRoot = std::filesystem::path(std::string(AudioCueEditAudioRoot));
    std::error_code error;
    if (std::filesystem::exists(folder, error)) {
        std::filesystem::recursive_directory_iterator it(
            folder,
            std::filesystem::directory_options::skip_permission_denied,
            error);
        const std::filesystem::recursive_directory_iterator end;
        while (!error && it != end) {
            const std::filesystem::directory_entry entry = *it;
            it.increment(error);
            if (error) {
                break;
            }
            std::error_code entryError;
            if (!entry.is_regular_file(entryError)) {
                continue;
            }
            const std::string extension = lowerAscii(entry.path().extension().string());
            if (extension != ".wav") {
                continue;
            }

            std::filesystem::path relative = std::filesystem::relative(entry.path(), audioRoot, entryError);
            if (entryError) {
                relative = entry.path().lexically_relative(audioRoot);
            }

            AudioCueFileEntry file;
            file.name = entry.path().filename().string();
            file.relativePath = normalizeAudioRelativePath(relative.generic_string());
            file.fileSize = entry.file_size(entryError);
            if (entryError) {
                file.fileSize = 0;
            }
            audioCueEditFiles_.push_back(std::move(file));
        }
    }

    std::sort(audioCueEditFiles_.begin(), audioCueEditFiles_.end(), [](const AudioCueFileEntry& lhs, const AudioCueFileEntry& rhs) {
        return lowerAscii(lhs.relativePath) < lowerAscii(rhs.relativePath);
    });

    audioCueEditFileIndex_ = audioCueEditFileIndexForPath(audioCueEditFiles_, previousPath);
    if (audioCueEditFileIndex_ < 0 && !audioCueEditFiles_.empty()) {
        audioCueEditFileIndex_ = 0;
    }
    if (audioCueEditFiles_.empty()) {
        audioCueEditFileIndex_ = -1;
    }

    audioCueEditStatus_ = std::to_string(audioCueEditFiles_.size()) + " files: " + audioCueEditFolderText(audioCueEditMode_);
}

void Game::enterAudioCueEditMode(AudioCueEditMode editMode)
{
    if (mode_ == ScreenMode::AudioCueEdit && audioCueEditMode_ == editMode) {
        rebuildAudioCueFileList();
        loadAudioCueManifestForEdit();
        return;
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }
    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        exitEnemyHitboxEditMode();
    }
    if (mode_ == ScreenMode::EnemyShadowEdit) {
        exitEnemyShadowEditMode();
    }

    closeDebugItemPicker();
    closeDebugStoryTest();
    if (baseEditEnabled_) {
        exitBaseEditMode();
    }

    audioCueEditMode_ = editMode;
    audioCueEditReturnMode_ = mode_;
    if (audioCueEditReturnMode_ == ScreenMode::AudioCueEdit ||
        audioCueEditReturnMode_ == ScreenMode::ObjectImageScaleEdit ||
        audioCueEditReturnMode_ == ScreenMode::EnemyHitboxEdit ||
        audioCueEditReturnMode_ == ScreenMode::EnemyShadowEdit) {
        audioCueEditReturnMode_ = ScreenMode::Playing;
    }
    audioCueEditPreviousBgmCue_ = activeAudioBgmCue_;

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    audioCueEditCueScrollOffset_ = 0.0f;
    audioCueEditFileScrollOffset_ = 0.0f;
    audioCueEditCueScrollState_ = {};
    audioCueEditFileScrollState_ = {};
    audioCueEditCancelState_ = {};

    rebuildAudioCueFileList();
    loadAudioCueManifestForEdit();
    mode_ = ScreenMode::AudioCueEdit;
}

void Game::exitAudioCueEditMode()
{
    if (mode_ != ScreenMode::AudioCueEdit) {
        return;
    }

    if (audio_ != nullptr) {
        if (!audioCueEditPreviousBgmCue_.empty()) {
            audio_->playBgm(audioCueEditPreviousBgmCue_, 0.12f, true);
            activeAudioBgmCue_ = audioCueEditPreviousBgmCue_;
        } else if (audioCueEditMode_ == AudioCueEditMode::Bgm) {
            audio_->stopBgm(0.12f);
            activeAudioBgmCue_.clear();
        }
    }

    mode_ = audioCueEditReturnMode_;
    if (mode_ == ScreenMode::AudioCueEdit) {
        mode_ = ScreenMode::Playing;
    }
}

void Game::previewSelectedAudioCueFile()
{
    if (audio_ == nullptr) {
        audioCueEditStatus_ = "AudioEngine unavailable";
        return;
    }
    if (audioCueEditFileIndex_ < 0 || audioCueEditFileIndex_ >= static_cast<int>(audioCueEditFiles_.size())) {
        audioCueEditStatus_ = "ファイルを選択してください";
        return;
    }

    AudioCueOptions options;
    if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < static_cast<int>(audioCueEditEntries_.size())) {
        const AudioCueEditEntry& cue = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)];
        options.volume = cue.volume;
        options.loop = cue.loop;
        options.cooldownSeconds = cue.cooldownMs / 1000.0f;
    } else {
        options.loop = audioCueEditMode_ == AudioCueEditMode::Bgm;
    }
    if (audioCueEditMode_ == AudioCueEditMode::Bgm) {
        options.loop = true;
    }

    const AudioCueFileEntry& file = audioCueEditFiles_[static_cast<std::size_t>(audioCueEditFileIndex_)];
    const std::filesystem::path clipPath = std::filesystem::path(std::string(AudioCueEditAudioRoot)) / std::filesystem::path(file.relativePath);
    const std::string previewId = audioCueEditMode_ == AudioCueEditMode::Bgm ? "__preview.bgm" : "__preview.se";
    if (!audio_->loadCue(previewId, audioCueEngineType(audioCueEditMode_), clipPath, options)) {
        audioCueEditStatus_ = "試聴失敗: " + audio_->lastError();
        return;
    }

    if (audioCueEditMode_ == AudioCueEditMode::Bgm) {
        audio_->playBgm(previewId, 0.08f, true);
    } else {
        audio_->playSe(previewId);
    }
    audioCueEditStatus_ = "試聴: " + file.relativePath;
}

void Game::applySelectedAudioCueFile()
{
    if (audioCueEditCueIndex_ < 0 || audioCueEditCueIndex_ >= static_cast<int>(audioCueEditEntries_.size())) {
        audioCueEditStatus_ = "cueを選択してください";
        return;
    }
    if (audioCueEditFileIndex_ < 0 || audioCueEditFileIndex_ >= static_cast<int>(audioCueEditFiles_.size())) {
        audioCueEditStatus_ = "ファイルを選択してください";
        return;
    }

    AudioCueEditEntry& cue = audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)];
    const AudioCueFileEntry& file = audioCueEditFiles_[static_cast<std::size_t>(audioCueEditFileIndex_)];
    cue.path = file.relativePath;
    audioCueEditDirty_ = true;

    AudioCueOptions options;
    options.volume = cue.volume;
    options.loop = cue.loop;
    options.cooldownSeconds = cue.cooldownMs / 1000.0f;
    const std::filesystem::path clipPath = std::filesystem::path(std::string(AudioCueEditAudioRoot)) / std::filesystem::path(file.relativePath);
    if (audio_ != nullptr) {
        if (audio_->loadCue(cue.id, audioCueEngineType(audioCueEditMode_), clipPath, options)) {
            if (audioCueEditMode_ == AudioCueEditMode::Bgm) {
                audio_->playBgm(cue.id, 0.08f, true);
            } else {
                audio_->playSe(cue.id);
            }
            audioCueEditStatus_ = audioCueDisplayName(cue) + " => " + cue.path;
        } else {
            audioCueEditStatus_ = "適用したが試聴失敗: " + audio_->lastError();
        }
    } else {
        audioCueEditStatus_ = audioCueDisplayName(cue) + " => " + cue.path;
    }
}

void Game::updateAudioCueEditScreen(const Input& input, UiContext& ui)
{
    if (mode_ != ScreenMode::AudioCueEdit) {
        return;
    }

    const AudioCueEditLayout layout = makeAudioCueEditLayout(camera_.width(), camera_.height());
    const int cueCount = static_cast<int>(audioCueEditEntries_.size());
    const int fileCount = static_cast<int>(audioCueEditFiles_.size());
    const UiScrollableListStyle listStyle = audioCueEditListStyle();
    const UiRect cueViewport = audioCueEditListViewport(layout.cueList);
    const UiRect fileViewport = audioCueEditListViewport(layout.fileList);
    UiScrollAreaLayout cueList = updateUiScrollableList(
        ui,
        input,
        cueViewport,
        cueCount,
        audioCueEditCueScrollOffset_,
        listStyle,
        &audioCueEditCueScrollState_);
    UiScrollAreaLayout fileList = updateUiScrollableList(
        ui,
        input,
        fileViewport,
        fileCount,
        audioCueEditFileScrollOffset_,
        listStyle,
        &audioCueEditFileScrollState_);
    bool keepCueSelectionVisible = false;
    bool keepFileSelectionVisible = false;

    if (uiCancelRequested(audioCueEditCancelState_, input, ui, layout.panel) ||
        ui.pressed(audioCueEditCloseButtonRect(layout.panel))) {
        exitAudioCueEditMode();
        return;
    }

    if (input.saveShortcutPressed() || ui.pressed(audioCueEditSaveButtonRect(layout.panel))) {
        std::string message;
        if (saveAudioCueManifestFromEdit(message)) {
            audioCueEditStatus_ = message;
        } else {
            audioCueEditStatus_ = message;
        }
    }

    if (ui.pressed(audioCueEditRescanButtonRect(layout.panel))) {
        rebuildAudioCueFileList();
        if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < cueCount) {
            const int fileIndex = audioCueEditFileIndexForPath(audioCueEditFiles_, audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)].path);
            if (fileIndex >= 0) {
                audioCueEditFileIndex_ = fileIndex;
                keepFileSelectionVisible = true;
            }
        }
    }

    if (ui.pressed(audioCueEditPreviewButtonRect(layout.panel))) {
        previewSelectedAudioCueFile();
    }
    if (ui.pressed(audioCueEditApplyButtonRect(layout.panel)) || input.confirmPressed()) {
        applySelectedAudioCueFile();
    }
    if (input.useItemPressed()) {
        previewSelectedAudioCueFile();
    }

    for (int i = 0; i < cueCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(cueList, i, listStyle);
        if (!uiScrollAreaRectVisible(cueList, rect)) {
            continue;
        }
        if (cueViewport.contains(ui.mouse()) && ui.pressed(rect)) {
            audioCueEditCueIndex_ = i;
            const int fileIndex = audioCueEditFileIndexForPath(audioCueEditFiles_, audioCueEditEntries_[static_cast<std::size_t>(i)].path);
            if (fileIndex >= 0) {
                audioCueEditFileIndex_ = fileIndex;
                keepFileSelectionVisible = true;
            }
            audioCueEditStatus_ = audioCueDisplayName(audioCueEditEntries_[static_cast<std::size_t>(i)]);
            break;
        }
    }

    for (int i = 0; i < fileCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(fileList, i, listStyle);
        if (!uiScrollAreaRectVisible(fileList, rect)) {
            continue;
        }
        if (fileViewport.contains(ui.mouse()) && ui.pressed(rect)) {
            audioCueEditFileIndex_ = i;
            audioCueEditStatus_ = audioCueEditFiles_[static_cast<std::size_t>(i)].relativePath;
            break;
        }
    }

    const int cueDelta =
        (input.pressed(InputAction::MoveRight) ? 1 : 0) -
        (input.pressed(InputAction::MoveLeft) ? 1 : 0);
    if (cueDelta != 0 && cueCount > 0) {
        audioCueEditCueIndex_ = std::clamp(audioCueEditCueIndex_ + cueDelta, 0, cueCount - 1);
        keepCueSelectionVisible = true;
        const int fileIndex = audioCueEditFileIndexForPath(audioCueEditFiles_, audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)].path);
        if (fileIndex >= 0) {
            audioCueEditFileIndex_ = fileIndex;
            keepFileSelectionVisible = true;
        }
    }

    const int fileDelta =
        (input.pressed(InputAction::MoveDown) ? 1 : 0) -
        (input.pressed(InputAction::MoveUp) ? 1 : 0);
    if (fileDelta != 0 && fileCount > 0) {
        audioCueEditFileIndex_ = std::clamp(audioCueEditFileIndex_ + fileDelta, 0, fileCount - 1);
        keepFileSelectionVisible = true;
    }

    if (keepCueSelectionVisible) {
        keepAudioCueEditSelectionVisible(layout.cueList, audioCueEditCueIndex_, cueCount, audioCueEditCueScrollOffset_);
    }
    if (keepFileSelectionVisible) {
        keepAudioCueEditSelectionVisible(layout.fileList, audioCueEditFileIndex_, fileCount, audioCueEditFileScrollOffset_);
    }
}

void Game::renderAudioCueEditScreen(Renderer& renderer) const
{
    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {8, 10, 16, 255});

    const AudioCueEditLayout layout = makeAudioCueEditLayout(camera_.width(), camera_.height());
    const std::string title = audioCueEditTitle(audioCueEditMode_);
    UiWindowScope window(
        renderer,
        audioCueEditMode_ == AudioCueEditMode::Bgm ? "audio.cue_edit.bgm" : "audio.cue_edit.se",
        layout.panel,
        title,
        "F適用 / Space試聴 / Ctrl+S保存 / Esc戻る",
        UiWindowOptions{true, true});

    drawUiSubPanel(renderer, layout.cueList);
    drawUiSubPanel(renderer, layout.fileList);
    drawUiSubPanel(renderer, layout.detail);

    renderer.drawText(layout.cueList.pos + Vec2{12.0f, 10.0f}, "cue ID", ui::TextMuted, 2);
    renderer.drawText(layout.fileList.pos + Vec2{12.0f, 10.0f}, audioCueEditFolderText(audioCueEditMode_), ui::TextMuted, 2);

    const int cueCount = static_cast<int>(audioCueEditEntries_.size());
    const int fileCount = static_cast<int>(audioCueEditFiles_.size());
    const UiScrollableListStyle listStyle = audioCueEditListStyle();
    const UiScrollAreaLayout cueList = makeUiScrollableListLayout(
        audioCueEditListViewport(layout.cueList),
        cueCount,
        audioCueEditCueScrollOffset_,
        listStyle);
    const UiScrollAreaLayout fileList = makeUiScrollableListLayout(
        audioCueEditListViewport(layout.fileList),
        fileCount,
        audioCueEditFileScrollOffset_,
        listStyle);
    renderer.pushClipRect(cueList.viewport.pos, cueList.viewport.size);
    for (int i = 0; i < cueCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(cueList, i, listStyle);
        if (!uiScrollAreaRectVisible(cueList, rect)) {
            continue;
        }

        const bool selected = i == audioCueEditCueIndex_;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{54, 70, 108, 255} : Color{24, 30, 44, 255});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 228, 138, 255} : Color{76, 88, 112, 255});

        const AudioCueEditEntry& cue = audioCueEditEntries_[static_cast<std::size_t>(i)];
        renderer.drawText(rect.pos + Vec2{10.0f, 6.0f}, fittedSingleLineText(renderer, audioCueDisplayName(cue), rect.size.x - 20.0f, 2), selected ? Color{255, 236, 166, 255} : ui::Text, 2);
        renderer.drawText(rect.pos + Vec2{10.0f, 28.0f}, fittedSingleLineText(renderer, cue.path, rect.size.x - 20.0f, 1), ui::TextMuted, 1);
    }
    renderer.popClipRect();
    if (cueCount == 0) {
        renderer.drawText(layout.cueList.pos + Vec2{18.0f, 54.0f}, "cueがありません", ui::TextDisabled, 2);
    }

    renderer.pushClipRect(fileList.viewport.pos, fileList.viewport.size);
    for (int i = 0; i < fileCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(fileList, i, listStyle);
        if (!uiScrollAreaRectVisible(fileList, rect)) {
            continue;
        }

        const bool selected = i == audioCueEditFileIndex_;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{52, 76, 70, 255} : Color{24, 30, 44, 255});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{170, 240, 190, 255} : Color{76, 88, 112, 255});

        const AudioCueFileEntry& file = audioCueEditFiles_[static_cast<std::size_t>(i)];
        renderer.drawText(rect.pos + Vec2{10.0f, 6.0f}, fittedSingleLineText(renderer, file.name, rect.size.x - 20.0f, 2), selected ? Color{202, 255, 216, 255} : ui::Text, 2);
        renderer.drawText(rect.pos + Vec2{10.0f, 28.0f}, fittedSingleLineText(renderer, file.relativePath, rect.size.x - 20.0f, 1), ui::TextMuted, 1);
    }
    renderer.popClipRect();
    if (fileCount == 0) {
        renderer.drawText(layout.fileList.pos + Vec2{18.0f, 54.0f}, "WAVがありません", ui::TextDisabled, 2);
    }
    drawUiScrollAreaFrame(renderer, cueList, listStyle.scroll);
    drawUiScrollAreaFrame(renderer, fileList, listStyle.scroll);

    const AudioCueEditEntry* cue = nullptr;
    const AudioCueFileEntry* file = nullptr;
    if (audioCueEditCueIndex_ >= 0 && audioCueEditCueIndex_ < cueCount) {
        cue = &audioCueEditEntries_[static_cast<std::size_t>(audioCueEditCueIndex_)];
    }
    if (audioCueEditFileIndex_ >= 0 && audioCueEditFileIndex_ < fileCount) {
        file = &audioCueEditFiles_[static_cast<std::size_t>(audioCueEditFileIndex_)];
    }

    float y = drawUiDetailHeader(renderer, layout.detail, "選択内容");
    drawUiDetailLine(renderer, layout.detail, y, "種類", audioCueEditTypeText(audioCueEditMode_));
    drawUiDetailLine(renderer, layout.detail, y, "表示名", cue != nullptr ? audioCueDisplayName(*cue) : "未選択");
    drawUiDetailLine(renderer, layout.detail, y, "内部ID", cue != nullptr ? cue->id : "-");
    drawUiDetailLine(renderer, layout.detail, y, "現在", cue != nullptr ? cue->path : "-");
    drawUiDetailLine(renderer, layout.detail, y, "候補", file != nullptr ? file->relativePath : "-");
    if (cue != nullptr) {
        drawUiDetailLine(renderer, layout.detail, y, "volume", formatAudioFloat(cue->volume));
        drawUiDetailLine(renderer, layout.detail, y, "loop", cue->loop ? "true" : "false");
        drawUiDetailLine(renderer, layout.detail, y, "cooldown", formatAudioFloat(cue->cooldownMs) + " ms");
    }
    if (file != nullptr) {
        const float kib = static_cast<float>(file->fileSize) / 1024.0f;
        drawUiDetailLine(renderer, layout.detail, y, "size", formatAudioFloat(kib) + " KiB");
    }

    const bool canPreview = file != nullptr;
    const bool canApply = cue != nullptr && file != nullptr;
    drawUiButton(renderer, audioCueEditCloseButtonRect(layout.panel), "閉じる", false, uiCancelButtonStyle());
    drawUiButton(renderer, audioCueEditRescanButtonRect(layout.panel), "再スキャン", false, uiActionButtonStyle());
    drawUiButton(renderer, audioCueEditPreviewButtonRect(layout.panel), "試聴", canPreview, uiActionButtonStyle());
    drawUiButton(renderer, audioCueEditApplyButtonRect(layout.panel), "適用", canApply, uiActionButtonStyle());
    drawUiButton(renderer, audioCueEditSaveButtonRect(layout.panel), "保存", audioCueEditDirty_, uiActionButtonStyle());

    const UiRect rescan = audioCueEditRescanButtonRect(layout.panel);
    const UiRect preview = audioCueEditPreviewButtonRect(layout.panel);
    const float statusX = rescan.pos.x + rescan.size.x + 16.0f;
    const float statusW = std::max(0.0f, preview.pos.x - statusX - 16.0f);
    const std::string dirty = audioCueEditDirty_ ? "未保存" : "保存済み";
    const std::string status = dirty + (audioCueEditStatus_.empty() ? std::string{} : " / " + audioCueEditStatus_);
    renderer.drawText(
        {statusX, rescan.pos.y + 9.0f},
        fittedSingleLineText(renderer, status, statusW, 2),
        audioCueEditDirty_ ? Color{255, 230, 150, 255} : ui::TextMuted,
        2);
}

bool Game::handleDebugNamedSaveCommand(std::string_view normalized)
{
    if (normalized == "game debug-save named" ||
        normalized == "game debug save named" ||
        normalized == "game named-save" ||
        normalized == "game save named") {
        openDebugNamedSaveDialog();
        return true;
    }
    if (normalized == "game debug-save load" ||
        normalized == "game debug save load" ||
        normalized == "game named-load" ||
        normalized == "game load named") {
        openDebugNamedLoadDialog();
        return true;
    }
    return false;
}

bool Game::handleDebugNamedSaveEvent(const SDL_Event& event)
{
    if (!debugNamedSaveInputActive_) {
        return false;
    }
    return handleUiTextInputEvent(debugNamedSaveInput_, event, 48);
}

void Game::openDebugNamedSaveDialog()
{
    if (!testPlayMode_) {
        logWarning("Debug: named save is available only in test-play mode.");
        return;
    }
    if (mode_ == ScreenMode::OpeningKamishibai ||
        mode_ == ScreenMode::EndingKamishibai ||
        mode_ == ScreenMode::Title ||
        mode_ == ScreenMode::WorldLoading) {
        logWarning("Debug: named save requires an active game screen.");
        return;
    }

    closeDebugItemPicker();
    closeDebugStoryTest();
    closeDebugNamedLoadDialog();
    debugNamedSaveInput_.text.clear();
    debugNamedSaveStatus_ = "保存名を入力してください";
    debugNamedSaveCancelState_ = {};
    debugNamedSaveInputActive_ = true;
    focusUiTextInput(debugNamedSaveInput_);
}

void Game::closeDebugNamedSaveDialog()
{
    blurUiTextInput(debugNamedSaveInput_);
    debugNamedSaveInputActive_ = false;
    debugNamedSaveCancelState_ = {};
}

void Game::rebuildDebugNamedLoadEntries()
{
    std::string previousSelection;
    if (debugNamedLoadSelectedIndex_ >= 0 &&
        debugNamedLoadSelectedIndex_ < static_cast<int>(debugNamedLoadEntries_.size())) {
        previousSelection = debugNamedLoadEntries_[static_cast<std::size_t>(debugNamedLoadSelectedIndex_)].name;
    }

    debugNamedLoadEntries_ = listDebugNamedSaveData();
    debugNamedLoadSelectedIndex_ = -1;
    if (!previousSelection.empty()) {
        for (int i = 0; i < static_cast<int>(debugNamedLoadEntries_.size()); ++i) {
            if (debugNamedLoadEntries_[static_cast<std::size_t>(i)].name == previousSelection) {
                debugNamedLoadSelectedIndex_ = i;
                break;
            }
        }
    }
    if (debugNamedLoadSelectedIndex_ < 0 && !debugNamedLoadEntries_.empty()) {
        debugNamedLoadSelectedIndex_ = 0;
    }
}

void Game::openDebugNamedLoadDialog()
{
    if (!testPlayMode_) {
        logWarning("Debug: named save load is available only in test-play mode.");
        return;
    }
    if (mode_ == ScreenMode::OpeningKamishibai ||
        mode_ == ScreenMode::EndingKamishibai ||
        mode_ == ScreenMode::Title ||
        mode_ == ScreenMode::WorldLoading) {
        logWarning("Debug: named save load requires an active game screen.");
        return;
    }

    closeDebugItemPicker();
    closeDebugStoryTest();
    closeDebugNamedSaveDialog();
    rebuildDebugNamedLoadEntries();
    debugNamedLoadScrollOffset_ = 0.0f;
    debugNamedSaveStatus_ = debugNamedLoadEntries_.empty()
        ? "名前付きセーブがありません"
        : "ロードするセーブを選択してください";
    debugNamedSaveCancelState_ = {};
    debugNamedLoadActive_ = true;
}

void Game::closeDebugNamedLoadDialog()
{
    debugNamedLoadActive_ = false;
    debugNamedSaveCancelState_ = {};
}

void Game::commitDebugNamedSave()
{
    const std::string name = trimAscii(debugNamedSaveInput_.text);
    if (name.empty()) {
        debugNamedSaveStatus_ = "保存名が空です";
        focusUiTextInput(debugNamedSaveInput_);
        return;
    }

    std::string message;
    const std::filesystem::path path = debugNamedSaveDataPath(name);
    if (saveSaveData(path, message)) {
        debugNamedSaveStatus_ = "保存しました: " + name;
        reloadNotice_ = debugNamedSaveStatus_;
        reloadNoticeTimer_ = 1.8f;
        logInfo("Debug: named save created: " + path.string());
        closeDebugNamedSaveDialog();
    } else {
        debugNamedSaveStatus_ = message.empty() ? "セーブ失敗" : message;
        logWarning("Debug: named save failed: " + debugNamedSaveStatus_);
        focusUiTextInput(debugNamedSaveInput_);
    }
}

void Game::loadSelectedDebugNamedSave()
{
    if (debugNamedLoadSelectedIndex_ < 0 ||
        debugNamedLoadSelectedIndex_ >= static_cast<int>(debugNamedLoadEntries_.size())) {
        debugNamedSaveStatus_ = "ロードするセーブが選択されていません";
        return;
    }

    const DebugNamedSaveEntry entry = debugNamedLoadEntries_[static_cast<std::size_t>(debugNamedLoadSelectedIndex_)];
    closeDebugNamedLoadDialog();
    if (!loadSaveData(entry.path)) {
        debugNamedSaveStatus_ = "ロード失敗: " + entry.name;
        reloadNotice_ = debugNamedSaveStatus_;
        reloadNoticeTimer_ = 1.8f;
        logWarning("Debug: named save load failed: " + entry.path.string());
        return;
    }

    placeBasePlayerAtHomeDoorResumePoint();
    enterBase();
    baseStatus_ = "テストセーブをロードしました: " + entry.name;
    reloadNotice_ = baseStatus_;
    reloadNoticeTimer_ = 2.0f;
    logInfo("Debug: named save loaded: " + entry.path.string());
}

void Game::updateDebugNamedSaveUi(const Input& input, UiContext& ui)
{
    if (debugNamedSaveInputActive_) {
        const DebugNamedSaveLayout layout = makeDebugNamedSaveInputLayout(camera_.width(), camera_.height());
        updateUiTextInput(debugNamedSaveInput_, ui, layout.input);
        if (uiCancelRequested(debugNamedSaveCancelState_, input, ui, layout.panel) ||
            ui.pressed(layout.secondaryButton)) {
            closeDebugNamedSaveDialog();
            return;
        }
        if (ui.pressed(layout.primaryButton) || input.confirmPressed() || input.useItemPressed()) {
            commitDebugNamedSave();
            return;
        }
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        return;
    }

    if (!debugNamedLoadActive_) {
        return;
    }

    const DebugNamedSaveLayout layout = makeDebugNamedSaveLoadLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(debugNamedLoadEntries_.size());
    if (itemCount > 0) {
        debugNamedLoadSelectedIndex_ = std::clamp(debugNamedLoadSelectedIndex_, 0, itemCount - 1);
    } else {
        debugNamedLoadSelectedIndex_ = -1;
    }

    const float maxScroll = debugNamedSaveLoadMaxScroll(layout, itemCount);
    debugNamedLoadScrollOffset_ = clamp(debugNamedLoadScrollOffset_, 0.0f, maxScroll);
    const int wheel = input.shortcutCursorDelta();
    if (wheel != 0) {
        debugNamedLoadScrollOffset_ = clamp(
            debugNamedLoadScrollOffset_ + static_cast<float>(wheel) * 42.0f,
            0.0f,
            maxScroll);
    }

    if (itemCount > 0) {
        const int moveY =
            (input.pressed(InputAction::MoveDown) ? 1 : 0) -
            (input.pressed(InputAction::MoveUp) ? 1 : 0);
        if (moveY != 0) {
            debugNamedLoadSelectedIndex_ = std::clamp(debugNamedLoadSelectedIndex_ + moveY, 0, itemCount - 1);
        }
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect row = debugNamedSaveLoadRowRect(layout, i, debugNamedLoadScrollOffset_);
        if (row.pos.y + row.size.y < layout.list.pos.y ||
            row.pos.y > layout.list.pos.y + layout.list.size.y) {
            continue;
        }
        if (ui.pressed(row)) {
            debugNamedLoadSelectedIndex_ = i;
            break;
        }
    }

    if (uiCancelRequested(debugNamedSaveCancelState_, input, ui, layout.panel) ||
        ui.pressed(layout.secondaryButton)) {
        closeDebugNamedLoadDialog();
        return;
    }
    if (ui.pressed(layout.primaryButton) || input.confirmPressed() || input.useItemPressed()) {
        loadSelectedDebugNamedSave();
        return;
    }

    ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
}

void Game::renderDebugNamedSaveUi(Renderer& renderer) const
{
    if (!debugNamedSaveInputActive_ && !debugNamedLoadActive_) {
        return;
    }

    renderer.setScreenSpace();
    drawUiModalBackdrop(
        renderer,
        {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
        {0, 0, 0, 142});

    UiCancelControlScope cancelScope(debugNamedSaveCancelState_);
    if (debugNamedSaveInputActive_) {
        const DebugNamedSaveLayout layout = makeDebugNamedSaveInputLayout(camera_.width(), camera_.height());
        UiWindowScope window(
            renderer,
            "debug.named_save.input",
            layout.panel,
            "デバッグ: 名前を付けてセーブ",
            "キーボード入力  Enter 保存  Esc 戻る",
            UiWindowOptions{true, true});

        renderer.drawText(layout.panel.pos + Vec2{36.0f, 70.0f}, "保存名", ui::TextMuted, 2);
        drawUiTextInput(renderer, layout.input, debugNamedSaveInput_, "例: boss_before_stage3", {});
        renderer.drawText(layout.status.pos, fittedSingleLineText(renderer, debugNamedSaveStatus_, layout.status.size.x, 2), {255, 230, 150, 255}, 2);
        drawUiRectButton(renderer, layout.secondaryButton, "キャンセル", false, uiCancelButtonStyle());
        drawUiRectButton(renderer, layout.primaryButton, "保存", true, uiActionButtonStyle());
        return;
    }

    const DebugNamedSaveLayout layout = makeDebugNamedSaveLoadLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(debugNamedLoadEntries_.size());
    const float scrollOffset = clamp(debugNamedLoadScrollOffset_, 0.0f, debugNamedSaveLoadMaxScroll(layout, itemCount));
    UiWindowScope window(
        renderer,
        "debug.named_save.load",
        layout.panel,
        "デバッグ: 名前付きセーブロード",
        "方向キー 選択  Enter ロード  Esc 戻る",
        UiWindowOptions{true, true});

    drawUiSubPanel(renderer, layout.list);
    if (itemCount <= 0) {
        renderer.drawText(layout.list.pos + Vec2{22.0f, 22.0f}, "名前付きセーブがありません", ui::TextMuted, 2);
    }

    renderer.pushClipRect(layout.list.pos, layout.list.size);
    for (int i = 0; i < itemCount; ++i) {
        const UiRect row = debugNamedSaveLoadRowRect(layout, i, scrollOffset);
        if (row.pos.y + row.size.y < layout.list.pos.y ||
            row.pos.y > layout.list.pos.y + layout.list.size.y) {
            continue;
        }
        const bool selected = i == debugNamedLoadSelectedIndex_;
        renderer.fillRect(row.pos, row.size, selected ? Color{54, 44, 72, 232} : Color{18, 20, 30, 218});
        renderer.drawRect(row.pos, row.size, selected ? Color{255, 230, 150, 255} : Color{92, 100, 126, 210});
        const DebugNamedSaveEntry& entry = debugNamedLoadEntries_[static_cast<std::size_t>(i)];
        renderer.drawText(row.pos + Vec2{14.0f, 10.0f}, fittedSingleLineText(renderer, entry.name, row.size.x - 28.0f, 2), selected ? Color{255, 236, 166, 255} : ui::Text, 2);
    }
    renderer.popClipRect();

    const UiScrollAreaLayout scrollLayout = makeUiScrollAreaLayout(
        layout.list,
        debugNamedSaveLoadContentHeight(layout, itemCount),
        scrollOffset);
    drawUiScrollAreaFrame(renderer, scrollLayout);
    renderer.drawText(layout.status.pos, fittedSingleLineText(renderer, debugNamedSaveStatus_, layout.status.size.x, 2), {255, 230, 150, 255}, 2);
    drawUiRectButton(renderer, layout.secondaryButton, "キャンセル", false, uiCancelButtonStyle());
    UiButtonStyle loadStyle = uiActionButtonStyle();
    if (itemCount <= 0) {
        loadStyle.text = ui::TextDisabled;
    }
    drawUiRectButton(renderer, layout.primaryButton, "ロード", itemCount > 0, loadStyle);
}

void Game::rebuildDebugItemPickerList()
{
    std::string previousSelection;
    if (debugItemPickerSelectedIndex_ >= 0 &&
        debugItemPickerSelectedIndex_ < static_cast<int>(debugItemPickerObjectIds_.size())) {
        previousSelection = debugItemPickerObjectIds_[static_cast<std::size_t>(debugItemPickerSelectedIndex_)];
    }

    debugItemPickerAllObjectIds_.clear();
    debugItemPickerAllObjectIds_.reserve(objectCatalog_.registry.size());
    for (const ItemData& item : objectCatalog_.registry.items()) {
        if (!item.id.empty()) {
            debugItemPickerAllObjectIds_.push_back(item.id);
        }
    }

    std::sort(debugItemPickerAllObjectIds_.begin(), debugItemPickerAllObjectIds_.end(), [this](const std::string& left, const std::string& right) {
        const ItemData* lhs = objectCatalog_.registry.findById(left);
        const ItemData* rhs = objectCatalog_.registry.findById(right);
        if (lhs == nullptr || rhs == nullptr) {
            return left < right;
        }
        if (lhs->category != rhs->category) {
            return lhs->category < rhs->category;
        }
        if (lhs->rarity != rhs->rarity) {
            return lhs->rarity < rhs->rarity;
        }
        const std::string lhsName = debugItemPickerDisplayName(*lhs);
        const std::string rhsName = debugItemPickerDisplayName(*rhs);
        if (lhsName != rhsName) {
            return lhsName < rhsName;
        }
        return left < right;
    });

    applyDebugItemPickerFilter(previousSelection);
}

void Game::applyDebugItemPickerFilter(std::string_view preferredSelection)
{
    std::string previousSelection{preferredSelection};
    if (previousSelection.empty() &&
        debugItemPickerSelectedIndex_ >= 0 &&
        debugItemPickerSelectedIndex_ < static_cast<int>(debugItemPickerObjectIds_.size())) {
        previousSelection = debugItemPickerObjectIds_[static_cast<std::size_t>(debugItemPickerSelectedIndex_)];
    }

    debugItemPickerObjectIds_.clear();
    debugItemPickerObjectIds_.reserve(debugItemPickerAllObjectIds_.size());
    const std::string normalizedQuery = normalizedUiSearchText(debugItemPickerSearchInput_.text);
    for (const std::string& objectId : debugItemPickerAllObjectIds_) {
        const ItemData* item = objectCatalog_.registry.findById(objectId);
        if (item != nullptr && debugItemPickerNameMatchesSearch(*item, normalizedQuery)) {
            debugItemPickerObjectIds_.push_back(objectId);
        }
    }

    debugItemPickerSelectedIndex_ = -1;
    if (!previousSelection.empty()) {
        const auto it = std::find(debugItemPickerObjectIds_.begin(), debugItemPickerObjectIds_.end(), previousSelection);
        if (it != debugItemPickerObjectIds_.end()) {
            debugItemPickerSelectedIndex_ = static_cast<int>(std::distance(debugItemPickerObjectIds_.begin(), it));
        }
    }
    if (debugItemPickerSelectedIndex_ < 0 && !debugItemPickerObjectIds_.empty()) {
        debugItemPickerSelectedIndex_ = 0;
    }
}

bool Game::handleDebugItemPickerEvent(const SDL_Event& event)
{
    if (!debugItemPickerActive_) {
        return false;
    }

    std::string previousSelection;
    if (debugItemPickerSelectedIndex_ >= 0 &&
        debugItemPickerSelectedIndex_ < static_cast<int>(debugItemPickerObjectIds_.size())) {
        previousSelection = debugItemPickerObjectIds_[static_cast<std::size_t>(debugItemPickerSelectedIndex_)];
    }

    const std::string previousText = debugItemPickerSearchInput_.text;
    const bool consumed = handleUiTextInputEvent(debugItemPickerSearchInput_, event, 48);
    if (debugItemPickerSearchInput_.text != previousText) {
        applyDebugItemPickerFilter(previousSelection);
        debugItemPickerScrollOffset_ = 0.0f;
        const DebugItemPickerLayout layout = makeDebugItemPickerLayout(camera_.width(), camera_.height());
        keepDebugItemPickerSelectionVisible(
            layout,
            debugItemPickerSelectedIndex_,
            static_cast<int>(debugItemPickerObjectIds_.size()),
            debugItemPickerScrollOffset_);
    }
    return consumed;
}

void Game::openDebugItemPicker()
{
    if (mode_ == ScreenMode::OpeningKamishibai || mode_ == ScreenMode::EndingKamishibai || mode_ == ScreenMode::Title || mode_ == ScreenMode::WorldLoading) {
        logWarning("Debug: item picker requires an active game screen.");
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        exitObjectImageScaleEditMode();
    }
    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        exitEnemyHitboxEditMode();
    }
    if (mode_ == ScreenMode::EnemyShadowEdit) {
        exitEnemyShadowEditMode();
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }

    closeDebugStoryTest();
    debugStoryTestReturnAfterDialogue_ = false;
    debugItemPickerSearchInput_.text.clear();
    rebuildDebugItemPickerList();
    inventory_.cancelGrab();
    cancelRingGrab();
    debugItemPickerScrollOffset_ = std::max(0.0f, debugItemPickerScrollOffset_);
    debugItemPickerStatus_ = debugItemPickerObjectIds_.empty()
        ? "Objects DBにアイテムがありません"
        : "アイテムを選んで追加できます";
    debugItemPickerCancelState_ = {};
    debugItemPickerActive_ = true;
    focusUiTextInput(debugItemPickerSearchInput_);
}

void Game::closeDebugItemPicker()
{
    blurUiTextInput(debugItemPickerSearchInput_);
    debugItemPickerActive_ = false;
    debugItemPickerCancelState_ = {};
}

void Game::addSelectedDebugItem()
{
    if (debugItemPickerSelectedIndex_ < 0 ||
        debugItemPickerSelectedIndex_ >= static_cast<int>(debugItemPickerObjectIds_.size())) {
        debugItemPickerStatus_ = "追加するアイテムが選択されていません";
        return;
    }

    const std::string& objectId = debugItemPickerObjectIds_[static_cast<std::size_t>(debugItemPickerSelectedIndex_)];
    const ItemData* item = objectCatalog_.registry.findById(objectId);
    if (item == nullptr) {
        debugItemPickerStatus_ = "Objects DBに見つかりません: " + objectId;
        return;
    }

    const std::string itemName = debugItemPickerDisplayName(*item);
    InventoryAddResult addResult;
    if (!inventory_.addObjectItem(objectCatalog_, objectId, &addResult)) {
        debugItemPickerStatus_ = "追加できません: " + itemName;
        reloadNotice_ = debugItemPickerStatus_;
        reloadNoticeTimer_ = 1.4f;
        logWarning("Debug: failed to add object_id=\"" + objectId + "\".");
        return;
    }

    debugItemPickerStatus_ = "追加: " + itemName;
    reloadNotice_ = "Debug add: " + inlineItemTag(objectId) + " " + itemName;
    reloadNoticeTimer_ = 1.6f;
    recordObjectObtainedForFirstNotice(
        objectId,
        addResult.instanceId,
        addResult.kind == InventoryAddKind::Instance && !addResult.instanceId.empty(),
        player_.position);
    syncEncyclopediaFromInventoryAndRing();
    logInfo("Debug: added object_id=\"" + objectId + "\".");
}

void Game::updateDebugItemPicker(const Input& input, UiContext& ui)
{
    if (!debugItemPickerActive_) {
        return;
    }

    const DebugItemPickerLayout layout = makeDebugItemPickerLayout(camera_.width(), camera_.height());
    int itemCount = static_cast<int>(debugItemPickerObjectIds_.size());
    if (itemCount > 0) {
        debugItemPickerSelectedIndex_ = std::clamp(debugItemPickerSelectedIndex_, 0, itemCount - 1);
    } else {
        debugItemPickerSelectedIndex_ = -1;
    }

    float maxScroll = debugItemPickerMaxScroll(layout, itemCount);
    debugItemPickerScrollOffset_ = clamp(debugItemPickerScrollOffset_, 0.0f, maxScroll);

    updateUiTextInput(debugItemPickerSearchInput_, ui, debugItemPickerSearchInputRect(layout));
    if (ui.pressed(debugItemPickerSearchClearButtonRect(layout))) {
        if (!debugItemPickerSearchInput_.text.empty()) {
            std::string previousSelection;
            if (debugItemPickerSelectedIndex_ >= 0 &&
                debugItemPickerSelectedIndex_ < static_cast<int>(debugItemPickerObjectIds_.size())) {
                previousSelection = debugItemPickerObjectIds_[static_cast<std::size_t>(debugItemPickerSelectedIndex_)];
            }
            debugItemPickerSearchInput_.text.clear();
            applyDebugItemPickerFilter(previousSelection);
            debugItemPickerScrollOffset_ = 0.0f;
            itemCount = static_cast<int>(debugItemPickerObjectIds_.size());
            maxScroll = debugItemPickerMaxScroll(layout, itemCount);
            keepDebugItemPickerSelectionVisible(layout, debugItemPickerSelectedIndex_, itemCount, debugItemPickerScrollOffset_);
        }
        focusUiTextInput(debugItemPickerSearchInput_);
    }

    if (uiCancelRequested(debugItemPickerCancelState_, input, ui, layout.panel) ||
        ui.pressed(debugItemPickerCloseButtonRect(layout.panel))) {
        closeDebugItemPicker();
        return;
    }

    if (itemCount > 0) {
        const int moveX =
            (input.pressed(InputAction::MoveRight) ? 1 : 0) -
            (input.pressed(InputAction::MoveLeft) ? 1 : 0);
        const int moveY =
            (input.pressed(InputAction::MoveDown) ? 1 : 0) -
            (input.pressed(InputAction::MoveUp) ? 1 : 0);
        const int moveDelta = moveX + moveY * std::max(1, layout.columns);
        if (moveDelta != 0) {
            debugItemPickerSelectedIndex_ = std::clamp(debugItemPickerSelectedIndex_ + moveDelta, 0, itemCount - 1);
            keepDebugItemPickerSelectionVisible(layout, debugItemPickerSelectedIndex_, itemCount, debugItemPickerScrollOffset_);
        }
    }

    const int wheel = input.shortcutCursorDelta();
    if (wheel != 0) {
        debugItemPickerScrollOffset_ = clamp(
            debugItemPickerScrollOffset_ + static_cast<float>(wheel) * 48.0f,
            0.0f,
            maxScroll);
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = debugItemPickerCardRect(layout, i, debugItemPickerScrollOffset_);
        if (rect.pos.y + rect.size.y < layout.grid.pos.y ||
            rect.pos.y > layout.grid.pos.y + layout.grid.size.y) {
            continue;
        }
        if (ui.pressed(rect)) {
            debugItemPickerSelectedIndex_ = i;
            keepDebugItemPickerSelectionVisible(layout, debugItemPickerSelectedIndex_, itemCount, debugItemPickerScrollOffset_);
            break;
        }
    }

    if (ui.pressed(debugItemPickerAddButtonRect(layout.panel)) || input.confirmPressed() || input.useItemPressed()) {
        addSelectedDebugItem();
    }

    ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
}

void Game::renderDebugItemPicker(Renderer& renderer) const
{
    if (!debugItemPickerActive_) {
        return;
    }

    renderer.setScreenSpace();
    const DebugItemPickerLayout layout = makeDebugItemPickerLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(debugItemPickerObjectIds_.size());
    const float scrollOffset = clamp(
        debugItemPickerScrollOffset_,
        0.0f,
        debugItemPickerMaxScroll(layout, itemCount));

    drawUiModalBackdrop(
        renderer,
        {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
        {0, 0, 0, 142});

    UiCancelControlScope cancelScope(debugItemPickerCancelState_);
    UiWindowScope pickerWindow(
        renderer,
        "debug.item_picker",
        layout.panel,
        "デバッグ: 任意アイテム追加",
        "キーボード入力 検索  方向キー 選択  Enter 追加  Esc 戻る",
        UiWindowOptions{true, true});

    drawUiSubPanel(renderer, layout.grid);
    drawUiSubPanel(renderer, layout.detail);
    drawUiTextInput(renderer, debugItemPickerSearchInputRect(layout), debugItemPickerSearchInput_, "アイテム名で検索", {});
    UiButtonStyle clearStyle;
    if (debugItemPickerSearchInput_.text.empty()) {
        clearStyle.text = ui::TextDisabled;
        clearStyle.fill = {18, 24, 42, 170};
        clearStyle.outline = {90, 84, 108, 170};
    }
    drawUiRectButton(renderer, debugItemPickerSearchClearButtonRect(layout), "消去", false, clearStyle);
    const int totalItemCount = static_cast<int>(debugItemPickerAllObjectIds_.size());
    const std::string countText = std::to_string(itemCount) + "/" + std::to_string(totalItemCount) + "件";
    const UiRect countRect = debugItemPickerSearchCountRect(layout);
    const std::string fittedCountText = fittedSingleLineText(renderer, countText, countRect.size.x, 2);
    renderer.drawText(
        {
            countRect.pos.x + std::max(0.0f, countRect.size.x - renderer.measureText(fittedCountText, 2).x),
            countRect.pos.y + 12.0f,
        },
        fittedCountText,
        ui::TextMuted,
        2);

    if (itemCount <= 0) {
        const std::string emptyMessage = debugItemPickerSearchInput_.text.empty()
            ? "Objects DBにアイテムがありません"
            : "検索に一致するアイテムがありません";
        renderer.drawText(layout.grid.pos + Vec2{22.0f, 24.0f}, emptyMessage, ui::TextMuted, 2);
    }

    renderer.pushClipRect(layout.grid.pos, layout.grid.size);
    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = debugItemPickerCardRect(layout, i, scrollOffset);
        if (rect.pos.y + rect.size.y < layout.grid.pos.y ||
            rect.pos.y > layout.grid.pos.y + layout.grid.size.y) {
            continue;
        }

        const std::string& objectId = debugItemPickerObjectIds_[static_cast<std::size_t>(i)];
        const ItemData* item = objectCatalog_.registry.findById(objectId);
        const bool selected = i == debugItemPickerSelectedIndex_;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{54, 44, 72, 232} : Color{18, 20, 30, 218});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 230, 150, 255} : Color{92, 100, 126, 210});

        if (item == nullptr) {
            renderer.drawText(rect.pos + Vec2{8.0f, 36.0f}, "missing", ui::TextDisabled, 2);
            continue;
        }

        InventoryUiEntryView entry{};
        entry.item = item;
        entry.stackCount = 1;
        InventoryUiSlotStyle style;
        style.selected = selected;
        style.imageMaxSize = 46.0f;
        style.showProtectionLabel = false;
        const UiRect iconRect{{
            rect.pos.x + (rect.size.x - DebugItemPickerIconSize) * 0.5f,
            rect.pos.y + 7.0f,
        }, {DebugItemPickerIconSize, DebugItemPickerIconSize}};
        drawInventoryUiSlot(renderer, iconRect, entry, style);

        const std::string name = fittedSingleLineText(renderer, debugItemPickerDisplayName(*item), rect.size.x - 12.0f, 2);
        renderer.drawText(rect.pos + Vec2{6.0f, 68.0f}, name, ui::Text, 2);
        const std::string subtitle = fittedSingleLineText(renderer, debugItemPickerSubtitle(*item), rect.size.x - 12.0f, 1);
        renderer.drawText(rect.pos + Vec2{6.0f, 91.0f}, subtitle, ui::TextMuted, 1);
    }
    renderer.popClipRect();
    const UiScrollAreaStyle scrollStyle = debugListScrollStyle(48.0f);
    const UiScrollAreaLayout scrollLayout = makeUiScrollAreaLayout(
        layout.grid,
        debugItemPickerContentHeight(layout, itemCount),
        scrollOffset,
        scrollStyle);
    drawUiScrollAreaScrollbar(renderer, scrollLayout, scrollStyle);

    InventoryUiEntryView detailEntry{};
    std::vector<InventoryUiDetailExtraLine> detailLines;
    if (debugItemPickerSelectedIndex_ >= 0 &&
        debugItemPickerSelectedIndex_ < itemCount) {
        const std::string& objectId = debugItemPickerObjectIds_[static_cast<std::size_t>(debugItemPickerSelectedIndex_)];
        if (const ItemData* item = objectCatalog_.registry.findById(objectId)) {
            detailEntry.item = item;
            detailEntry.stackCount = 1;
            detailLines.push_back({"ID", item->id, ui::TextMuted});
            if (!item->category.empty()) {
                detailLines.push_back({"カテゴリ", item->category, ui::Text});
            }
        }
    }
    drawInventoryUiDetailPanel(renderer, layout.detail, detailEntry, objectCatalog_, encyclopedia_, {}, detailLines);

    drawUiButton(renderer, debugItemPickerCloseButtonRect(layout.panel), "閉じる", false, uiCancelButtonStyle());
    drawUiButton(renderer, debugItemPickerAddButtonRect(layout.panel), "追加", detailEntry.item != nullptr, uiActionButtonStyle());
    if (!debugItemPickerStatus_.empty()) {
        const UiRect closeRect = debugItemPickerCloseButtonRect(layout.panel);
        const UiRect addRect = debugItemPickerAddButtonRect(layout.panel);
        const float statusX = closeRect.pos.x + closeRect.size.x + 22.0f;
        const float statusW = std::max(40.0f, addRect.pos.x - statusX - 18.0f);
        renderer.drawText(
            {statusX, closeRect.pos.y + 17.0f},
            fittedSingleLineText(renderer, debugItemPickerStatus_, statusW, 2),
            {255, 230, 150, 255},
            2);
    }
}

void Game::rebuildDebugStoryTestList()
{
    std::string previousSelection;
    if (debugStoryTestSelectedIndex_ >= 0 &&
        debugStoryTestSelectedIndex_ < static_cast<int>(debugStoryTestEntries_.size())) {
        previousSelection = debugStoryTestEntries_[static_cast<std::size_t>(debugStoryTestSelectedIndex_)].eventId;
    }

    const bool tutorials = debugStoryTestMode_ == DebugStoryTestMode::Tutorials;
    debugStoryTestEntries_.clear();
    debugStoryTestEntries_.reserve(storyEvents_.size());
    for (const StoryEvent& event : storyEvents_) {
        if (event.id.empty()) {
            continue;
        }
        if (debugStoryTestIsTutorialTrigger(event.trigger) != tutorials) {
            continue;
        }

        DebugStoryTestEntry entry;
        entry.eventId = event.id;
        entry.title = debugStoryTestDisplayTitle(event);
        entry.trigger = debugStoryTestDisplayTrigger(event);
        entry.onceFlag = debugStoryTestDisplayOnceFlag(event);
        entry.repeatable = event.repeatable;
        entry.alreadySeen = !event.onceFlag.empty() &&
            std::find(storyFlags_.begin(), storyFlags_.end(), event.onceFlag) != storyFlags_.end();
        debugStoryTestEntries_.push_back(std::move(entry));
    }

    debugStoryTestSelectedIndex_ = -1;
    if (!previousSelection.empty()) {
        const auto it = std::find_if(
            debugStoryTestEntries_.begin(),
            debugStoryTestEntries_.end(),
            [&](const DebugStoryTestEntry& entry) { return entry.eventId == previousSelection; });
        if (it != debugStoryTestEntries_.end()) {
            debugStoryTestSelectedIndex_ = static_cast<int>(std::distance(debugStoryTestEntries_.begin(), it));
        }
    }
    if (debugStoryTestSelectedIndex_ < 0 && !debugStoryTestEntries_.empty()) {
        debugStoryTestSelectedIndex_ = 0;
    }
    debugStoryTestLoadedRevision_ = storyEventsRevision_;
}

void Game::openDebugStoryTest(DebugStoryTestMode mode)
{
    if (mode_ == ScreenMode::OpeningKamishibai || mode_ == ScreenMode::EndingKamishibai || mode_ == ScreenMode::Title || mode_ == ScreenMode::WorldLoading) {
        logWarning("Debug: story test requires an active game screen.");
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        exitObjectImageScaleEditMode();
    }
    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        exitEnemyHitboxEditMode();
    }
    if (mode_ == ScreenMode::EnemyShadowEdit) {
        exitEnemyShadowEditMode();
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        exitAudioCueEditMode();
    }

    closeDebugItemPicker();
    debugStoryTestReturnAfterDialogue_ = false;
    debugStoryTestMode_ = mode;
    rebuildDebugStoryTestList();
    inventory_.cancelGrab();
    cancelRingGrab();
    debugStoryTestScrollOffset_ = std::max(0.0f, debugStoryTestScrollOffset_);
    debugStoryTestScrollState_ = {};
    const bool tutorials = debugStoryTestMode_ == DebugStoryTestMode::Tutorials;
    debugStoryTestStatus_ = debugStoryTestEntries_.empty()
        ? (tutorials ? "チュートリアルがありません" : "イベントがありません")
        : (tutorials ? "チュートリアルを選んで再生できます" : "イベントを選んで再生できます");
    debugStoryTestCancelState_ = {};
    debugStoryTestActive_ = true;
}

void Game::closeDebugStoryTest()
{
    debugStoryTestActive_ = false;
    debugStoryTestReturnAfterDialogue_ = false;
    debugStoryTestScrollState_ = {};
    debugStoryTestCancelState_ = {};
}

void Game::playSelectedDebugStoryTest()
{
    if (debugStoryTestSelectedIndex_ < 0 ||
        debugStoryTestSelectedIndex_ >= static_cast<int>(debugStoryTestEntries_.size())) {
        debugStoryTestStatus_ = "再生する会話が選択されていません";
        return;
    }

    const DebugStoryTestEntry entry = debugStoryTestEntries_[static_cast<std::size_t>(debugStoryTestSelectedIndex_)];
    if (!startStoryEventForDebug(entry.eventId)) {
        debugStoryTestStatus_ = "再生できません: " + entry.eventId;
        reloadNotice_ = debugStoryTestStatus_;
        reloadNoticeTimer_ = 1.6f;
        return;
    }

    reloadNotice_ = "Story test: " + entry.title;
    reloadNoticeTimer_ = 1.6f;
    closeDebugStoryTest();
    debugStoryTestReturnAfterDialogue_ = true;
}

void Game::updateDebugStoryTest(const Input& input, UiContext& ui)
{
    if (!debugStoryTestActive_) {
        return;
    }

    if (debugStoryTestLoadedRevision_ != storyEventsRevision_) {
        rebuildDebugStoryTestList();
    }

    const DebugStoryTestLayout layout = makeDebugStoryTestLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(debugStoryTestEntries_.size());
    const UiScrollableListStyle listStyle = debugStoryTestListStyle();
    if (itemCount > 0) {
        debugStoryTestSelectedIndex_ = std::clamp(debugStoryTestSelectedIndex_, 0, itemCount - 1);
    } else {
        debugStoryTestSelectedIndex_ = -1;
    }

    UiScrollAreaLayout listLayout = updateUiScrollableList(
        ui,
        input,
        layout.list,
        itemCount,
        debugStoryTestScrollOffset_,
        listStyle,
        &debugStoryTestScrollState_);

    if (uiCancelRequested(debugStoryTestCancelState_, input, ui, layout.panel) ||
        ui.pressed(debugStoryTestCloseButtonRect(layout.panel))) {
        closeDebugStoryTest();
        closeEffectTestMode();
        return;
    }

    if (itemCount > 0) {
        const int moveDelta =
            (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight) ? 1 : 0) -
            (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft) ? 1 : 0);
        if (moveDelta != 0) {
            debugStoryTestSelectedIndex_ = std::clamp(debugStoryTestSelectedIndex_ + moveDelta, 0, itemCount - 1);
            keepDebugStoryTestSelectionVisible(layout, debugStoryTestSelectedIndex_, itemCount, debugStoryTestScrollOffset_);
            listLayout = makeUiScrollableListLayout(layout.list, itemCount, debugStoryTestScrollOffset_, listStyle);
        }
    }

    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(listLayout, i, listStyle);
        if (!uiScrollAreaRectVisible(listLayout, rect)) {
            continue;
        }
        if (listLayout.viewport.contains(ui.mouse()) && ui.pressed(rect)) {
            debugStoryTestSelectedIndex_ = i;
            keepDebugStoryTestSelectionVisible(layout, debugStoryTestSelectedIndex_, itemCount, debugStoryTestScrollOffset_);
            break;
        }
    }

    if (ui.pressed(debugStoryTestPlayButtonRect(layout.panel)) || input.confirmPressed() || input.useItemPressed()) {
        playSelectedDebugStoryTest();
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        return;
    }

    ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
}

void Game::renderDebugStoryTest(Renderer& renderer) const
{
    if (!debugStoryTestActive_) {
        return;
    }

    renderer.setScreenSpace();
    const DebugStoryTestLayout layout = makeDebugStoryTestLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(debugStoryTestEntries_.size());
    const UiScrollableListStyle listStyle = debugStoryTestListStyle();
    const UiScrollAreaLayout listLayout = makeUiScrollableListLayout(
        layout.list,
        itemCount,
        debugStoryTestScrollOffset_,
        listStyle);
    const bool tutorials = debugStoryTestMode_ == DebugStoryTestMode::Tutorials;

    drawUiModalBackdrop(
        renderer,
        {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
        {0, 0, 0, 142});

    UiCancelControlScope cancelScope(debugStoryTestCancelState_);
    UiWindowScope pickerWindow(
        renderer,
        tutorials ? "debug.story_test.tutorials" : "debug.story_test.events",
        layout.panel,
        tutorials ? "デバッグ: チュートリアルテスト" : "デバッグ: イベントテスト",
        "方向キー 選択  F/Enter 再生  Esc 戻る",
        UiWindowOptions{true, true});

    drawUiSubPanel(renderer, layout.list);
    drawUiSubPanel(renderer, layout.detail);

    if (itemCount <= 0) {
        renderer.drawText(
            layout.list.pos + Vec2{22.0f, 24.0f},
            tutorials ? "チュートリアルがありません" : "イベントがありません",
            ui::TextMuted,
            2);
    }

    renderer.pushClipRect(listLayout.viewport.pos, listLayout.viewport.size);
    for (int i = 0; i < itemCount; ++i) {
        const UiRect rect = uiScrollableListItemRect(listLayout, i, listStyle);
        if (!uiScrollAreaRectVisible(listLayout, rect)) {
            continue;
        }

        const DebugStoryTestEntry& entry = debugStoryTestEntries_[static_cast<std::size_t>(i)];
        const bool selected = i == debugStoryTestSelectedIndex_;
        renderer.fillRect(rect.pos, rect.size, selected ? Color{54, 44, 72, 232} : Color{18, 20, 30, 218});
        renderer.drawRect(rect.pos, rect.size, selected ? Color{255, 230, 150, 255} : Color{92, 100, 126, 210});

        const char* stateText = entry.repeatable ? "REPEAT" : (entry.alreadySeen ? "SEEN" : "ONCE");
        const Color stateColor = entry.repeatable
            ? Color{146, 224, 176, 255}
            : (entry.alreadySeen ? Color{168, 186, 214, 255} : Color{255, 230, 150, 255});
        const Vec2 stateSize = renderer.measureText(stateText, 2);
        const float stateX = rect.pos.x + rect.size.x - stateSize.x - 16.0f;
        renderer.drawText({stateX, rect.pos.y + 16.0f}, stateText, stateColor, 2);

        const float titleMaxWidth = std::max(120.0f, stateX - rect.pos.x - 28.0f);
        const std::string title = fittedSingleLineText(renderer, entry.title, titleMaxWidth, 2);
        renderer.drawText(rect.pos + Vec2{14.0f, 7.0f}, title, ui::Text, 2);

        const std::string meta = entry.eventId + " / " + entry.trigger;
        const std::string shownMeta = fittedSingleLineText(renderer, meta, rect.size.x - 28.0f, 1);
        renderer.drawText(rect.pos + Vec2{14.0f, 34.0f}, shownMeta, ui::TextMuted, 1);
    }
    renderer.popClipRect();
    drawUiScrollAreaFrame(renderer, listLayout, listStyle.scroll);

    bool canPlay = false;
    if (debugStoryTestSelectedIndex_ >= 0 &&
        debugStoryTestSelectedIndex_ < itemCount) {
        canPlay = true;
        const DebugStoryTestEntry& entry = debugStoryTestEntries_[static_cast<std::size_t>(debugStoryTestSelectedIndex_)];
        float y = drawUiDetailHeader(renderer, layout.detail, entry.title);
        drawUiDetailLine(renderer, layout.detail, y, "ID", entry.eventId, ui::TextMuted);
        drawUiDetailLine(renderer, layout.detail, y, "Trigger", entry.trigger, ui::Text);
        drawUiDetailLine(renderer, layout.detail, y, "Once", entry.onceFlag, entry.repeatable ? Color{146, 224, 176, 255} : ui::Text);
        drawUiDetailLine(
            renderer,
            layout.detail,
            y,
            "状態",
            entry.repeatable ? "繰り返し" : (entry.alreadySeen ? "再生済み" : "未再生"),
            entry.alreadySeen ? ui::TextMuted : ui::Text);
        drawUiDetailText(renderer, layout.detail, y, "テスト再生では @once フラグを追加しません。");
    } else {
        renderer.drawText(
            uiSubPanelContentPos(layout.detail),
            tutorials ? "チュートリアルを選択してください" : "イベントを選択してください",
            ui::TextMuted,
            2);
    }

    drawUiButton(renderer, debugStoryTestCloseButtonRect(layout.panel), "閉じる", false, uiCancelButtonStyle());
    drawUiButton(renderer, debugStoryTestPlayButtonRect(layout.panel), "再生", canPlay, uiActionButtonStyle());
    if (!debugStoryTestStatus_.empty()) {
        const UiRect closeRect = debugStoryTestCloseButtonRect(layout.panel);
        const UiRect addRect = debugStoryTestPlayButtonRect(layout.panel);
        const float statusX = closeRect.pos.x + closeRect.size.x + 22.0f;
        const float statusW = std::max(40.0f, addRect.pos.x - statusX - 18.0f);
        renderer.drawText(
            {statusX, closeRect.pos.y + 17.0f},
            fittedSingleLineText(renderer, debugStoryTestStatus_, statusW, 2),
            {255, 230, 150, 255},
            2);
    }
}

void Game::rebuildEffectTestEntries()
{
    const EffectPreviewEntry* selectedEntry = effectTestEntryAt(effectTestVisibleEntries_, effectTestSelectedIndex_);
    const std::string preferredEntryId = selectedEntry != nullptr ? std::string(selectedEntry->id) : std::string{};
    const std::string previousTabKey =
        effectTestTabIndex_ >= 0 && effectTestTabIndex_ < static_cast<int>(effectTestTabKeys_.size())
        ? effectTestTabKeys_[static_cast<std::size_t>(effectTestTabIndex_)]
        : std::string{};

    effectTestEntries_.clear();
    effectTestVisibleEntries_.clear();
    effectTestTabKeys_.clear();
    effectTestTabLabels_.clear();
    const auto appendEntries = [&](std::span<const EffectPreviewEntry> entries) {
        for (const EffectPreviewEntry& entry : entries) {
            effectTestEntries_.push_back(&entry);
        }
    };
    appendEntries(effectSystemPreviewEntries());
    appendEntries(magicFxPreviewEntries());
    appendEntries(entityStatusPreviewEntries());

    effectTestTabKeys_.push_back({});
    effectTestTabLabels_.push_back("すべて");
    for (const EffectPreviewEntry* entry : effectTestEntries_) {
        if (entry == nullptr) {
            continue;
        }
        const std::string key(entry->group);
        if (std::find(effectTestTabKeys_.begin(), effectTestTabKeys_.end(), key) != effectTestTabKeys_.end()) {
            continue;
        }
        effectTestTabKeys_.push_back(key);
        effectTestTabLabels_.push_back(effectTestTabLabelForGroup(entry->group));
    }

    effectTestTabIndex_ = 0;
    if (!previousTabKey.empty()) {
        const auto tabIt = std::find(effectTestTabKeys_.begin(), effectTestTabKeys_.end(), previousTabKey);
        if (tabIt != effectTestTabKeys_.end()) {
            effectTestTabIndex_ = static_cast<int>(std::distance(effectTestTabKeys_.begin(), tabIt));
        }
    }
    effectTestTabsState_.focusedIndex = effectTestTabIndex_;
    rebuildEffectTestVisibleEntries(preferredEntryId);
}

void Game::rebuildEffectTestVisibleEntries(std::string_view preferredEntryId)
{
    effectTestVisibleEntries_.clear();
    std::string_view tabKey;
    if (effectTestTabIndex_ >= 0 && effectTestTabIndex_ < static_cast<int>(effectTestTabKeys_.size())) {
        const std::string& key = effectTestTabKeys_[static_cast<std::size_t>(effectTestTabIndex_)];
        tabKey = std::string_view(key.data(), key.size());
    }

    for (const EffectPreviewEntry* entry : effectTestEntries_) {
        if (entry != nullptr && effectTestEntryMatchesTab(*entry, tabKey)) {
            effectTestVisibleEntries_.push_back(entry);
        }
    }

    if (effectTestEntries_.empty()) {
        effectTestSelectedIndex_ = 0;
        effectTestStatus_ = "エフェクト項目がありません";
    } else if (effectTestVisibleEntries_.empty()) {
        effectTestSelectedIndex_ = 0;
        effectTestStatus_ = "このタブにはエフェクト項目がありません";
    } else {
        int preferredIndex = -1;
        if (!preferredEntryId.empty()) {
            for (int i = 0; i < static_cast<int>(effectTestVisibleEntries_.size()); ++i) {
                const EffectPreviewEntry* entry = effectTestVisibleEntries_[static_cast<std::size_t>(i)];
                if (entry != nullptr && entry->id == preferredEntryId) {
                    preferredIndex = i;
                    break;
                }
            }
        }
        effectTestSelectedIndex_ = preferredIndex >= 0
            ? preferredIndex
            : std::clamp(effectTestSelectedIndex_, 0, static_cast<int>(effectTestVisibleEntries_.size()) - 1);
        effectTestStatus_ = "選択中のエフェクトを自動再生します";
    }
}

void Game::resetEffectTestPlayback()
{
    if (effectTestEmitter_.valid()) {
        magicFx_.stopEmitter(effectTestEmitter_);
    }
    effectTestEmitter_ = {};
    effectTestReplayTimerSeconds_ = 0.0f;
    effects_ = EffectSystem{};
    magicFx_ = MagicFxSystem{};
}

void Game::enterEffectTestMode()
{
    if (projectileTestActive_) {
        closeProjectileTestMode();
    }
    if (enemyTestActive_) {
        exitEnemyTestToBase();
    }

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    closeDebugItemPicker();
    closeDebugStoryTest();
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    levelUpPresentation_ = {};

    mode_ = ScreenMode::Playing;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    effectTestActive_ = true;
    effectTestScrollState_ = {};
    rebuildEffectTestEntries();
    resetEffectTestPlayback();
}

void Game::closeEffectTestMode()
{
    if (!effectTestActive_) {
        return;
    }
    effectTestActive_ = false;
    effectTestStatus_.clear();
    effectTestScrollState_ = {};
    effectTestScrollOffset_ = 0.0f;
    effectTestTabsState_ = {};
    effectTestTabIndex_ = 0;
    effectTestVisibleEntries_.clear();
    resetEffectTestPlayback();
}

void Game::exitEffectTestToBase()
{
    closeEffectTestMode();
    clearTemporaryPlayerState(true);
    enterBase();
}

void Game::triggerEffectTestPlayback(const EffectPreviewEntry& entry)
{
    const DebugPreviewTestLayout layout = makeDebugPreviewTestLayout(camera_.width(), camera_.height());
    const Vec2 targetPosition = effectTestTargetPosition(layout, entry.target);
    const Vec2 direction = lengthSquared(entry.direction) > 0.0001f ? normalize(entry.direction) : Vec2{1.0f, 0.0f};
    constexpr Color StageOneWallColor{105, 68, 37, 255};

    if (entry.source == EffectPreviewSource::EffectSystem) {
        playEffectSystemPreview(effects_, entry, targetPosition, direction, TileType::Dirt, StageOneWallColor);
    } else if (entry.source == EffectPreviewSource::MagicFx) {
        if (entry.playback == EffectPreviewPlayback::PersistentEmitter) {
            effectTestEmitter_ = startMagicFxPreview(magicFx_, entry, targetPosition, direction);
        } else {
            playMagicFxPreview(magicFx_, entry, targetPosition, direction);
        }
    } else if (entry.source == EffectPreviewSource::StatusVisual && !entry.argument.empty()) {
        EntityStatus status;
        (void)status.applyState(std::string(entry.argument), 1.0, -1.0, "effect_test");
        emitEntityStatusAuras(status, targetPosition, effects_);
    }
}

void Game::updateEffectTestScreen(const Input& input, UiContext& ui, float dt)
{
    if (!effectTestActive_) {
        return;
    }
    if (effectTestEntries_.empty()) {
        rebuildEffectTestEntries();
    }

    const DebugPreviewTestLayout layout = makeDebugPreviewTestLayout(
        camera_.width(),
        camera_.height(),
        static_cast<int>(effectTestTabLabels_.size()));

    const EffectPreviewEntry* previousEntry = effectTestEntryAt(effectTestVisibleEntries_, effectTestSelectedIndex_);
    const std::string previousEntryId = previousEntry != nullptr ? std::string(previousEntry->id) : std::string{};
    const int selectedTab = updateDebugPreviewTestTabs(
        input,
        ui,
        layout,
        effectTestTabLabels_,
        effectTestTabsState_,
        effectTestTabIndex_);
    bool tabChanged = false;
    bool playbackResetThisFrame = false;
    if (selectedTab >= 0 && selectedTab != effectTestTabIndex_) {
        effectTestTabIndex_ = std::clamp(selectedTab, 0, std::max(0, static_cast<int>(effectTestTabLabels_.size()) - 1));
        effectTestScrollState_ = {};
        effectTestScrollOffset_ = 0.0f;
        rebuildEffectTestVisibleEntries(previousEntryId);
        resetEffectTestPlayback();
        playbackResetThisFrame = true;
        tabChanged = true;
    }

    const int itemCount = static_cast<int>(effectTestVisibleEntries_.size());
    bool selectionChanged = false;
    (void)updateDebugPreviewTestList(
        input,
        ui,
        layout,
        itemCount,
        effectTestSelectedIndex_,
        effectTestScrollOffset_,
        effectTestScrollState_,
        selectionChanged);

    if (selectionChanged && !tabChanged) {
        resetEffectTestPlayback();
        playbackResetThisFrame = true;
    }

    (void)updateDebugPreviewBackgroundControls(ui, layout, debugPreviewBackgroundIndex_);

    if (ui.pressed(layout.closeButton) || input.backPressed() || input.pausePressed()) {
        exitEffectTestToBase();
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    const EffectPreviewEntry* entry = effectTestEntryAt(effectTestVisibleEntries_, effectTestSelectedIndex_);
    if (entry != nullptr && !playbackResetThisFrame) {
        if (entry->playback == EffectPreviewPlayback::PersistentEmitter) {
            const Vec2 position = effectTestTargetPosition(layout, entry->target);
            if (effectTestEmitter_.valid()) {
                magicFx_.setEmitterPosition(effectTestEmitter_, position);
            } else {
                triggerEffectTestPlayback(*entry);
            }
        } else if (effectTestReplayTimerSeconds_ <= 0.0f) {
            triggerEffectTestPlayback(*entry);
            effectTestReplayTimerSeconds_ = DebugEffectPreviewTestLoopSeconds;
        }
    }

    magicFx_.update(safeDt);
    effects_.update(safeDt);
    for (const MagicFxSoundEvent& event : magicFx_.consumeSoundEvents()) {
        playAudioSe(event.cueId, event.volumeScale, event.pitchScale);
    }
    for (const EffectSoundEvent& event : effects_.consumeSoundEvents()) {
        playAudioSe(event.cueId, event.volumeScale, event.pitchScale);
    }
    if (entry != nullptr && entry->playback != EffectPreviewPlayback::PersistentEmitter && effectTestReplayTimerSeconds_ > 0.0f) {
        effectTestReplayTimerSeconds_ = std::max(0.0f, effectTestReplayTimerSeconds_ - safeDt);
    }
    ui.block(layout.bounds);
}

void Game::renderEffectTestScreen(Renderer& renderer, double totalSeconds)
{
    if (!effectTestActive_) {
        return;
    }

    renderer.setScreenSpace();
    const DebugPreviewTestLayout layout = makeDebugPreviewTestLayout(
        camera_.width(),
        camera_.height(),
        static_cast<int>(effectTestTabLabels_.size()));
    const EffectPreviewEntry* entry = effectTestEntryAt(effectTestVisibleEntries_, effectTestSelectedIndex_);

    drawDebugPreviewBackground(renderer, layout);
    renderDebugPreviewTestTabs(renderer, layout, effectTestTabLabels_, effectTestTabsState_, effectTestTabIndex_);
    renderDebugPreviewTestList(
        renderer,
        layout,
        "エフェクトテスト",
        static_cast<int>(effectTestVisibleEntries_.size()),
        effectTestSelectedIndex_,
        effectTestScrollOffset_,
        [&](int index) {
            const EffectPreviewEntry* rowEntry = effectTestVisibleEntries_[static_cast<std::size_t>(index)];
            return DebugPreviewListRow{
                rowEntry != nullptr ? std::string(rowEntry->label) : std::string{},
                rowEntry != nullptr ? std::string(rowEntry->group) : std::string{},
            };
        });

    renderer.pushClipRect(layout.preview.pos, layout.preview.size);
    drawDebugPreviewGuide(renderer, layout, debugPreviewBackgroundIndex_);
    const Vec2 previewCenter{
        layout.preview.pos.x + layout.preview.size.x * 0.5f,
        layout.preview.pos.y + layout.preview.size.y * 0.5f,
    };

    const Vec2 targetPosition = entry != nullptr ? effectTestTargetPosition(layout, entry->target) : previewCenter;
    EntityStatus previewStatus;
    if (entry != nullptr && entry->source == EffectPreviewSource::StatusVisual && !entry->argument.empty()) {
        (void)previewStatus.applyState(std::string(entry->argument), 1.0, -1.0, "effect_test");
    }
    const EntityStatusVisualStyle statusVisual = entityStatusVisualStyle(previewStatus);
    const Vec2 jitter = entityStatusJitterOffset(previewStatus, totalSeconds);
    effects_.renderShadows(renderer);

    if (entry != nullptr && entry->target == EffectPreviewTarget::WallTile) {
        const float tileSize = static_cast<float>(balance::TileSize);
        tileMap_.renderTilePreview(renderer, targetPosition - Vec2{tileSize * 0.5f, tileSize * 0.5f}, 1, TileType::Dirt);
    } else if (entry != nullptr && entry->target == EffectPreviewTarget::EnemySlime) {
        Enemy previewEnemy;
        const auto slimeIt = enemyCatalog_.enemiesById.find(std::string(DebugPreviewTestSlimeEnemyId));
        if (slimeIt != enemyCatalog_.enemiesById.end()) {
            const EnemyDefinition& definition = slimeIt->second;
            previewEnemy.active = true;
            previewEnemy.definition = &definition;
            previewEnemy.enemyId = definition.id;
            previewEnemy.enemyName = definition.name;
            previewEnemy.radius = definition.radius > 0.0 ? static_cast<float>(definition.radius) : balance_.enemyRadius;
            previewEnemy.position = targetPosition;
            previewEnemy.facingAngle = Pi * 0.5f;
            previewEnemy.behaviorTimer = static_cast<float>(totalSeconds);
            previewEnemy.status = previewStatus;

            EnemyImageDrawOptions options;
            options.tint = statusVisual.hasTint ? statusVisual.tint : Color{255, 255, 255, 255};
            options.flipY = statusVisual.flipVertical;
            options.scaleMultiplier = statusVisual.scaleMultiplier;
            Vec2 drawSize{};
            const Vec2 drawPosition = targetPosition + jitter;
            const bool sizeResolved = enemyImageDrawSize(renderer, previewEnemy, options, drawSize);
            renderer.drawActorShadow(targetPosition, sizeResolved ? drawSize.y : previewEnemy.radius * 2.0f);
            if (drawEnemyImage(renderer, previewEnemy, drawPosition, static_cast<float>(totalSeconds), options, &drawSize)) {
                renderEntityStatusOverlays(renderer, previewStatus, drawPosition, drawSize.y, totalSeconds);
            } else {
                renderer.fillCircle(drawPosition, previewEnemy.radius, statusVisual.hasTint ? statusVisual.tint : Color{112, 204, 112, 255});
                renderer.drawCircle(drawPosition, previewEnemy.radius + 3.0f, {42, 72, 48, 255});
                renderEntityStatusOverlays(renderer, previewStatus, drawPosition, previewEnemy.radius * 2.0f, totalSeconds);
            }
        } else {
            renderer.drawActorShadow(targetPosition, 42.0f);
            renderer.fillCircle(targetPosition + jitter, 22.0f, statusVisual.hasTint ? statusVisual.tint : Color{112, 204, 112, 255});
            renderer.drawCircle(targetPosition + jitter, 25.0f, {42, 72, 48, 255});
            renderEntityStatusOverlays(renderer, previewStatus, targetPosition + jitter, 50.0f, totalSeconds);
        }
    } else {
        const float playerSize = PlayerSpriteDrawSize * statusVisual.scaleMultiplier;
        const Vec2 drawPosition = targetPosition + jitter;
        renderer.drawActorShadow(targetPosition, playerSize);
        if (renderer.hasPlayerSheet()) {
            renderer.drawPlayerSprite(
                0,
                drawPosition,
                playerSize,
                false,
                statusVisual.hasTint ? statusVisual.tint : Color{255, 255, 255, 255},
                {PlayerSpriteAnchorX, PlayerSpriteAnchorY},
                statusVisual.flipVertical);
        } else {
            renderer.fillCircle(drawPosition, player_.effectiveRadius(balance_.playerRadius), statusVisual.hasTint ? statusVisual.tint : Color{118, 72, 168, 255});
            renderer.drawLine(drawPosition, drawPosition + Vec2{22.0f, 0.0f}, {235, 210, 255, 255});
        }
        renderEntityStatusOverlays(renderer, previewStatus, drawPosition, playerSize, totalSeconds);
    }

    std::vector<DepthRenderEntry> depthEntries;
    effects_.appendRenderEntries(depthEntries, renderer);
    magicFx_.appendRenderEntries(depthEntries, renderer);
    std::stable_sort(depthEntries.begin(), depthEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& depthEntry : depthEntries) {
        depthEntry.draw();
    }
    effects_.render(renderer);
    std::vector<DepthRenderEntry> foregroundEntries;
    magicFx_.appendForegroundRenderEntries(foregroundEntries, renderer);
    std::stable_sort(foregroundEntries.begin(), foregroundEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& foregroundEntry : foregroundEntries) {
        foregroundEntry.draw();
    }
    effects_.renderForeground(renderer);
    renderer.popClipRect();

    const std::string detail = entry != nullptr
        ? std::string(entry->group) + " / " + effectTestTargetLabel(entry->target) + " / " + effectTestPlaybackLabel(entry->playback)
        : std::string{};
    drawDebugPreviewFooter(
        renderer,
        layout,
        entry != nullptr ? entry->label : std::string_view{},
        detail,
        effectTestStatus_,
        debugPreviewBackgroundIndex_);
}

void Game::rebuildProjectileTestEntries()
{
    projectileTestEntries_.clear();
    for (const ProjectileDefinition& definition : projectileDefinitions()) {
        projectileTestEntries_.push_back(&definition);
    }

    if (projectileTestEntries_.empty()) {
        projectileTestSelectedIndex_ = 0;
        projectileTestStatus_ = "弾項目がありません";
    } else {
        projectileTestSelectedIndex_ = std::clamp(projectileTestSelectedIndex_, 0, static_cast<int>(projectileTestEntries_.size()) - 1);
        projectileTestStatus_ = projectileTestDefaultStatus(projectileTestTargetEnabled_);
    }
}

void Game::resetProjectileTestPlayback()
{
    projectileTestReplayTimerSeconds_ = 0.0f;
    projectiles_ = ProjectileSystem{};
}

void Game::enterProjectileTestMode()
{
    if (effectTestActive_) {
        closeEffectTestMode();
    }
    if (enemyTestActive_) {
        exitEnemyTestToBase();
    }

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    closeDebugItemPicker();
    closeDebugStoryTest();
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    levelUpPresentation_ = {};

    mode_ = ScreenMode::Playing;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    projectileTestActive_ = true;
    projectileTestScrollState_ = {};
    rebuildProjectileTestEntries();
    resetProjectileTestPlayback();
}

void Game::closeProjectileTestMode()
{
    if (!projectileTestActive_) {
        return;
    }
    projectileTestActive_ = false;
    projectileTestStatus_.clear();
    projectileTestScrollState_ = {};
    projectileTestScrollOffset_ = 0.0f;
    resetProjectileTestPlayback();
}

void Game::exitProjectileTestToBase()
{
    closeProjectileTestMode();
    clearTemporaryPlayerState(true);
    enterBase();
}

void Game::triggerProjectileTestPlayback(const ProjectileDefinition& entry)
{
    const DebugPreviewTestLayout layout = makeDebugPreviewTestLayout(camera_.width(), camera_.height());
    (void)projectiles_.spawn(entry.id, projectileTestFireOrigin(layout), {1.0f, 0.0f}, ProjectileOwnerType::Enemy);
}

void Game::updateProjectileTestScreen(const Input& input, UiContext& ui, float dt)
{
    if (!projectileTestActive_) {
        return;
    }
    if (projectileTestEntries_.empty()) {
        rebuildProjectileTestEntries();
    }

    const DebugPreviewTestLayout layout = makeDebugPreviewTestLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(projectileTestEntries_.size());
    bool selectionChanged = false;
    (void)updateDebugPreviewTestList(
        input,
        ui,
        layout,
        itemCount,
        projectileTestSelectedIndex_,
        projectileTestScrollOffset_,
        projectileTestScrollState_,
        selectionChanged);

    if (selectionChanged) {
        resetProjectileTestPlayback();
    }

    (void)updateDebugPreviewBackgroundControls(ui, layout, debugPreviewBackgroundIndex_);
    if (ui.pressed(projectileTestTargetToggleRect(layout))) {
        projectileTestTargetEnabled_ = !projectileTestTargetEnabled_;
        projectileTestStatus_ = projectileTestDefaultStatus(projectileTestTargetEnabled_);
        resetProjectileTestPlayback();
        ui.emitSound(UiSoundEvent::Confirm);
    }

    if (ui.pressed(layout.closeButton) || input.backPressed() || input.pausePressed()) {
        exitProjectileTestToBase();
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    const ProjectileDefinition* entry = projectileTestEntryAt(projectileTestEntries_, projectileTestSelectedIndex_);
    if (entry != nullptr && projectileTestReplayTimerSeconds_ <= 0.0f) {
        triggerProjectileTestPlayback(*entry);
        projectileTestReplayTimerSeconds_ = projectileTestReplaySeconds(layout, *entry, projectileTestTargetEnabled_);
    }

    if (projectileTestTargetEnabled_) {
        projectiles_.updatePreview(
            safeDt,
            ProjectilePreviewTarget{
                projectileTestTargetPosition(layout, entry),
                projectileTestSlimeRadius(enemyCatalog_, balance_.enemyRadius),
                true,
            });
    } else {
        projectiles_.updatePreview(safeDt);
    }
    for (const ProjectileSoundEvent& event : projectiles_.consumeSoundEvents()) {
        playAudioSe(event.cueId, event.volumeScale, event.pitchScale);
    }
    if (projectileTestReplayTimerSeconds_ > 0.0f) {
        projectileTestReplayTimerSeconds_ = std::max(0.0f, projectileTestReplayTimerSeconds_ - safeDt);
    }
    ui.block(layout.bounds);
}

void Game::renderProjectileTestScreen(Renderer& renderer, double totalSeconds)
{
    if (!projectileTestActive_) {
        return;
    }

    renderer.setScreenSpace();
    const DebugPreviewTestLayout layout = makeDebugPreviewTestLayout(camera_.width(), camera_.height());
    const ProjectileDefinition* entry = projectileTestEntryAt(projectileTestEntries_, projectileTestSelectedIndex_);
    const int replayFrames = entry != nullptr
        ? projectileTestReplayFrames(layout, *entry, projectileTestTargetEnabled_)
        : DebugProjectilePreviewReplayGapFrames;

    drawDebugPreviewBackground(renderer, layout);
    renderDebugPreviewTestList(
        renderer,
        layout,
        "弾テスト",
        static_cast<int>(projectileTestEntries_.size()),
        projectileTestSelectedIndex_,
        projectileTestScrollOffset_,
        [&](int index) {
            const ProjectileDefinition* rowEntry = projectileTestEntries_[static_cast<std::size_t>(index)];
            return DebugPreviewListRow{
                rowEntry != nullptr ? std::string(projectileDisplayName(*rowEntry)) : std::string{},
                rowEntry != nullptr ? projectileTestRowGroup(*rowEntry) : std::string{},
            };
        });

    renderer.pushClipRect(layout.preview.pos, layout.preview.size);
    drawDebugPreviewGuide(renderer, layout, debugPreviewBackgroundIndex_);
    const Vec2 sourcePosition = projectileTestSourcePosition(layout);
    const Vec2 origin = projectileTestFireOrigin(layout);
    renderer.drawLine(origin, {layout.preview.pos.x + layout.preview.size.x - 44.0f, origin.y}, {255, 255, 255, 34});

    drawProjectileTestSlime(renderer, enemyCatalog_, sourcePosition, balance_.enemyRadius, 0.0f, totalSeconds, {112, 204, 112, 255});
    if (projectileTestTargetEnabled_) {
        const Vec2 targetPosition = projectileTestTargetPosition(layout, entry);
        renderer.drawSoftRing(
            targetPosition,
            projectileTestSlimeRadius(enemyCatalog_, balance_.enemyRadius) + 9.0f,
            2.0f,
            {190, 244, 186, 118});
        drawProjectileTestSlime(renderer, enemyCatalog_, targetPosition, balance_.enemyRadius, Pi, totalSeconds, {128, 218, 128, 255});
    }

    std::vector<DepthRenderEntry> projectileEntries;
    projectiles_.appendPreviewRenderEntries(projectileEntries, renderer);
    std::stable_sort(projectileEntries.begin(), projectileEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& projectileEntry : projectileEntries) {
        projectileEntry.draw();
    }
    renderer.popClipRect();

    const UiRect targetToggle = projectileTestTargetToggleRect(layout);
    drawDebugPreviewFooter(
        renderer,
        layout,
        entry != nullptr ? projectileDisplayName(*entry) : std::string_view{},
        entry != nullptr ? projectileTestDetail(*entry, replayFrames) : std::string{},
        projectileTestStatus_,
        debugPreviewBackgroundIndex_,
        targetToggle.pos.x);
    drawProjectileTestTargetToggle(renderer, layout, projectileTestTargetEnabled_);
}

void Game::enterEnemyTestMode()
{
    if (effectTestActive_) {
        closeEffectTestMode();
    }
    if (projectileTestActive_) {
        closeProjectileTestMode();
    }
    if (!enemyTestActive_ && mode_ == ScreenMode::Playing) {
        captureDungeonState();
    }

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    levelUpPresentation_ = {};

    enemyTestActive_ = true;
    enemyTestUiVisible_ = true;
    enemyTestDropdown_ = {};
    enemyTestSelectedIndex_ = std::clamp(enemyTestSelectedIndex_, 0, std::max(0, static_cast<int>(enemyCatalog_.enemies.size()) - 1));
    enemyTestStatus_ = enemyCatalog_.enemies.empty() ? "敵データがありません" : "敵を選んで召喚できます";

    mode_ = ScreenMode::Playing;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    tileMap_ = TileMap{};
    digging_ = DiggingSystem{};
    effects_ = EffectSystem{};
    captureAbsorbAnimations_.clear();
    groundLines_ = GroundLineSystem{};
    wetGround_ = WetGroundSystem{};
    enemies_ = EnemySystem{};
    projectiles_ = ProjectileSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    runStats_ = RunStats{};
    warpPoints_.clear();
    rewardNodes_.clear();
    moneyNodes_.clear();
    moonFragmentNodes_.clear();
    chestNodes_.clear();
    crateNodes_.clear();
    enemyNodes_.clear();
    spawnedWarpPointCount_ = 0;
    bossSpawnPoint_ = {};
    hasBossSpawnPoint_ = false;
    bossSpawned_ = false;

    dungeonLayout_ = generateDungeonLayout(DungeonGenerationContext{
        .stageId = 1,
        .seed = 0xE17E57u,
        .stageHardnessMultiplier = 1.0f,
        .roguelike = false,
    });
    player_.position = tileWorldCenter(dungeonLayout_.startTile);
    player_.velocity = {};
    player_.facing = {1.0f, 0.0f};
    player_.updateSpriteFlipFromFacing();
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    resetPlayerFootstepDust();
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    camera_.follow(player_.position, 1.0f);
}

void Game::exitEnemyTestToBase()
{
    enemyTestActive_ = false;
    enemyTestUiVisible_ = true;
    enemyTestDropdown_ = {};
    enemyTestStatus_.clear();
    enemies_ = EnemySystem{};
    projectiles_ = ProjectileSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    groundLines_ = GroundLineSystem{};
    wetGround_ = WetGroundSystem{};
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    clearTemporaryPlayerState(true);
    enterBase();
}

bool Game::spawnEnemyTestMimicChest(const EnemyDefinition& enemy, Vec2 desiredPosition)
{
    if (enemy.id.empty()) {
        return false;
    }

    const DungeonTile tile{
        tileMap_.worldToTile(desiredPosition.x),
        tileMap_.worldToTile(desiredPosition.y),
    };
    const Vec2 center = tileWorldCenter(tile);
    if (tileMap_.isCircleBlocked(center, ChestHitRadius)) {
        return false;
    }

    auto existing = std::find_if(chestNodes_.begin(), chestNodes_.end(), [tile](const ChestNode& node) {
        return node.tile.x == tile.x && node.tile.y == tile.y;
    });

    ChestNode node;
    node.tile = tile;
    node.visibility = PlacementVisibility::Exposed;
    node.chestKind = chestKindForEnemyTestMimic(enemy);
    node.depthRank = 1;
    node.revealed = true;
    node.opened = false;
    node.lootSpawned = false;
    node.openingSeconds = 0.0f;
    node.mimicEnemyId = enemy.id;
    node.mimicTriggered = false;

    if (existing != chestNodes_.end()) {
        *existing = std::move(node);
    } else {
        chestNodes_.push_back(std::move(node));
    }
    return true;
}

int Game::spawnEnemyTestMagnetDrops(Vec2 center)
{
    std::vector<const ObjectDefinition*> candidates;
    candidates.reserve(objectCatalog_.objects.size());
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        if (objectIsEnemyTestMetalDropCandidate(object)) {
            candidates.push_back(&object);
        }
    }

    if (candidates.empty()) {
        return 0;
    }

    std::mt19937& rng = lootRuntimeRng();
    std::uniform_int_distribution<std::size_t> objectDistribution(0, candidates.size() - 1);
    int spawned = 0;
    for (int i = 0; i < EnemyTestMagnetDropCount; ++i) {
        const ObjectDefinition& object = *candidates[objectDistribution(rng)];
        const Vec2 target = randomEnemyTestMagnetDropPosition(center, tileMap_, rng);
        if (worldDrops_.spawnObjectDrop(
                objectCatalog_,
                object.id,
                target,
                runStats_.elapsedSeconds,
                makeWorldLootJumpMotion(center, rng),
                true)) {
            ++spawned;
        }
    }
    return spawned;
}

int Game::spawnEnemyTestStealBaitDrops(const EnemyDefinition& enemy, Vec2 center)
{
    const std::string targetFilter = enemyTestStealTargetFilter(enemy);
    const bool allowAnyDrop = targetFilter.empty() || enemyTestStealTargetContains(targetFilter, "drop");
    const bool allowMoney = allowAnyDrop || enemyTestStealTargetContains(targetFilter, "money");
    const bool allowTreasure = allowAnyDrop || enemyTestStealTargetContains(targetFilter, "treasure");
    if (!allowMoney && !allowTreasure) {
        return 0;
    }

    std::vector<const ObjectDefinition*> treasureCandidates;
    if (allowTreasure) {
        treasureCandidates.reserve(objectCatalog_.objects.size());
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            if (objectIsEnemyTestTreasureDropCandidate(object)) {
                treasureCandidates.push_back(&object);
            }
        }
    }

    std::mt19937& rng = lootRuntimeRng();
    int spawned = 0;
    if (allowMoney) {
        std::uniform_int_distribution<int> moneyDistribution(16, 55);
        for (int i = 0; i < EnemyTestStealMoneyDropCount; ++i) {
            const Vec2 target = randomEnemyTestStealDropPosition(center, tileMap_, rng);
            if (worldDrops_.spawnMoneyDrop(
                    moneyDistribution(rng),
                    target,
                    runStats_.elapsedSeconds,
                    makeWorldLootJumpMotion(center, rng))) {
                ++spawned;
            }
        }
    }

    if (!treasureCandidates.empty()) {
        std::uniform_int_distribution<std::size_t> objectDistribution(0, treasureCandidates.size() - 1);
        for (int i = 0; i < EnemyTestStealTreasureDropCount; ++i) {
            const ObjectDefinition& object = *treasureCandidates[objectDistribution(rng)];
            const Vec2 target = randomEnemyTestStealDropPosition(center, tileMap_, rng);
            if (worldDrops_.spawnObjectDrop(
                    objectCatalog_,
                    object.id,
                    target,
                    runStats_.elapsedSeconds,
                    makeWorldLootJumpMotion(center, rng),
                    true)) {
                ++spawned;
            }
        }
    }
    return spawned;
}

int Game::spawnEnemyTestHealSlimes(Vec2 center)
{
    if (enemyCatalog_.enemiesById.find(std::string(EnemyTestHealSlimeEnemyId)) == enemyCatalog_.enemiesById.end()) {
        return 0;
    }

    std::mt19937& rng = lootRuntimeRng();
    int spawned = 0;
    const int maxAttempts = EnemyTestHealSlimeCount * 8;
    for (int attempt = 0; attempt < maxAttempts && spawned < EnemyTestHealSlimeCount; ++attempt) {
        const Vec2 target = randomEnemyTestHealSlimePosition(center, rng);
        int runtimeId = 0;
        if (!enemies_.spawnSpecificEnemy(
                tileMap_,
                EnemyTestHealSlimeEnemyId,
                target,
                player_.position,
                balance_,
                enemyCatalog_,
                true,
                false,
                0.0f,
                &runtimeId)) {
            continue;
        }
        enemies_.setRuntimeEnemyHp(runtimeId, 1);
        enemies_.setRuntimeEnemyMovementLeash(runtimeId, center, EnemyTestHealSlimeLeashRadius);
        ++spawned;
    }
    return spawned;
}

int Game::spawnEnemyTestSwarmMembers(const EnemyDefinition& enemy, Vec2 center)
{
    std::mt19937& rng = lootRuntimeRng();
    int spawned = 0;
    const int maxAttempts = EnemyTestSwarmExtraCount * 8;
    for (int attempt = 0; attempt < maxAttempts && spawned < EnemyTestSwarmExtraCount; ++attempt) {
        const Vec2 target = randomEnemyTestSwarmPosition(center, tileMap_, rng);
        int runtimeId = 0;
        if (!enemies_.spawnSpecificEnemy(
                tileMap_,
                enemy.id,
                target,
                player_.position,
                balance_,
                enemyCatalog_,
                true,
                true,
                -1.0f,
                &runtimeId)) {
            continue;
        }
        enemies_.setRuntimeEnemyMovementLeash(runtimeId, center, EnemyTestSwarmLeashRadius);
        ++spawned;
    }
    return spawned;
}

void Game::spawnSelectedEnemyTestEnemy()
{
    if (enemyCatalog_.enemies.empty()) {
        enemyTestStatus_ = "敵データがありません";
        return;
    }
    enemyTestSelectedIndex_ = std::clamp(enemyTestSelectedIndex_, 0, static_cast<int>(enemyCatalog_.enemies.size()) - 1);
    const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(enemyTestSelectedIndex_)];
    Vec2 facing = lengthSquared(player_.facing) > 0.0001f ? normalize(player_.facing) : Vec2{1.0f, 0.0f};
    const Vec2 desiredPosition = player_.position + facing * 120.0f;
    if (enemyDefinitionIsChestMimic(enemy)) {
        if (spawnEnemyTestMimicChest(enemy, desiredPosition)) {
            enemyTestStatus_ = "宝箱として配置: " + (enemy.name.empty() ? enemy.id : enemy.name);
        } else {
            enemyTestStatus_ = "宝箱を配置できませんでした: " + (enemy.name.empty() ? enemy.id : enemy.name);
        }
    } else {
        int spawnedRuntimeId = 0;
        if (!enemies_.spawnSpecificEnemy(
                tileMap_,
                enemy.id,
                desiredPosition,
                player_.position,
                balance_,
                enemyCatalog_,
                true,
                true,
                -1.0f,
                &spawnedRuntimeId)) {
            enemyTestStatus_ = "召喚できませんでした: " + (enemy.name.empty() ? enemy.id : enemy.name);
            return;
        }

        Vec2 spawnedPosition = desiredPosition;
        enemies_.runtimeEnemyPosition(spawnedRuntimeId, spawnedPosition);
        enemyTestStatus_ = "召喚: " + (enemy.name.empty() ? enemy.id : enemy.name);
        if (enemyDefinitionHasBehavior(enemy, "steal_item")) {
            const int baitDropCount = spawnEnemyTestStealBaitDrops(enemy, spawnedPosition);
            if (baitDropCount > 0) {
                enemyTestStatus_ += " / 盗み対象 " + std::to_string(baitDropCount) + "個";
            } else {
                enemyTestStatus_ += " / 盗み対象なし";
            }
        }
        if (enemyDefinitionHasBehavior(enemy, "magnet_disturb")) {
            const int dropCount = spawnEnemyTestMagnetDrops(spawnedPosition);
            if (dropCount > 0) {
                enemyTestStatus_ += " / 金属アイテム " + std::to_string(dropCount) + "個";
            } else {
                enemyTestStatus_ += " / 金属アイテム候補なし";
            }
        }
        if (enemyDefinitionHasBehavior(enemy, "enemy_heal")) {
            const int slimeCount = spawnEnemyTestHealSlimes(spawnedPosition);
            if (slimeCount > 0) {
                enemyTestStatus_ += " / HP1スライム " + std::to_string(slimeCount) + "体";
            } else {
                enemyTestStatus_ += " / スライム召喚なし";
            }
        }
        if (enemyDefinitionHasBehavior(enemy, "swarm_alert")) {
            const int swarmCount = spawnEnemyTestSwarmMembers(enemy, spawnedPosition);
            if (swarmCount > 0) {
                enemyTestStatus_ += " / 群れ " + std::to_string(swarmCount + 1) + "体";
            } else {
                enemyTestStatus_ += " / 群れ追加なし";
            }
        }
    }
}

void Game::clearEnemyTestArena()
{
    enemies_ = EnemySystem{};
    projectiles_ = ProjectileSystem{};
    effects_ = EffectSystem{};
    captureAbsorbAnimations_.clear();
    groundLines_ = GroundLineSystem{};
    wetGround_ = WetGroundSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    chestNodes_.clear();
    enemyTestStatus_ = "敵・宝箱・弾を消去しました";
}

void Game::updateEnemyTestUi(const Input& input, UiContext& ui)
{
    if (!enemyTestActive_) {
        return;
    }

    const int itemCount = static_cast<int>(enemyCatalog_.enemies.size());
    if (!enemyTestUiVisible_) {
        if (ui.pressed(enemyTestRestoreButtonRect())) {
            enemyTestUiVisible_ = true;
        }
        return;
    }

    if (itemCount > 0) {
        enemyTestSelectedIndex_ = std::clamp(enemyTestSelectedIndex_, 0, itemCount - 1);
    } else {
        enemyTestSelectedIndex_ = 0;
    }

    std::vector<std::string> labels;
    std::vector<UiDropdownItem> items;
    labels.reserve(enemyCatalog_.enemies.size());
    items.reserve(enemyCatalog_.enemies.size());
    for (std::size_t i = 0; i < enemyCatalog_.enemies.size(); ++i) {
        labels.push_back(enemyTestDropdownItemLabel(enemyCatalog_.enemies[i], static_cast<int>(i)));
        items.push_back(UiDropdownItem{labels.back(), true});
    }

    const bool dropdownWasOpen = enemyTestDropdown_.open;
    const int dropdownSelection = updateUiDropdown(
        enemyTestDropdown_,
        ui,
        input,
        enemyTestSelectButtonRect(),
        enemyTestSelectedIndex_,
        items.empty() ? nullptr : items.data(),
        itemCount,
        enemyTestDropdownStyle());
    if (dropdownSelection >= 0) {
        enemyTestSelectedIndex_ = dropdownSelection;
    }

    if (!dropdownWasOpen && !enemyTestDropdown_.open && (input.confirmPressed() || input.useItemPressed())) {
        spawnSelectedEnemyTestEnemy();
    }

    if (ui.pressed(enemyTestSummonButtonRect())) {
        enemyTestDropdown_.open = false;
        spawnSelectedEnemyTestEnemy();
    }
    if (ui.pressed(enemyTestClearButtonRect())) {
        enemyTestDropdown_.open = false;
        clearEnemyTestArena();
    }
    if (ui.pressed(enemyTestHideButtonRect())) {
        enemyTestUiVisible_ = false;
        enemyTestDropdown_.open = false;
        return;
    }
    if (ui.pressed(enemyTestExitButtonRect())) {
        exitEnemyTestToBase();
        return;
    }
    if (input.backPressed()) {
        if (dropdownWasOpen || enemyTestDropdown_.open) {
            enemyTestDropdown_.open = false;
        } else {
            exitEnemyTestToBase();
        }
        return;
    }

    ui.block(enemyTestToolbarRect());
}

void Game::renderEnemyTestUi(Renderer& renderer) const
{
    if (!enemyTestActive_ || mode_ != ScreenMode::Playing) {
        return;
    }

    renderer.setScreenSpace();
    const int itemCount = static_cast<int>(enemyCatalog_.enemies.size());
    const int selected = itemCount > 0 ? std::clamp(enemyTestSelectedIndex_, 0, itemCount - 1) : 0;

    if (!enemyTestUiVisible_) {
        drawUiRectButton(renderer, enemyTestRestoreButtonRect(), "敵UI", false, uiActionButtonStyle());
        return;
    }

    const UiRect panel = enemyTestToolbarRect();
    renderer.fillRect(panel.pos, panel.size, {12, 18, 34, 218});
    renderer.drawRect(panel.pos, panel.size, {255, 255, 255, 210});
    renderer.drawText(panel.pos + Vec2{18.0f, 31.0f}, "敵テスト", {255, 230, 150, 255}, 2);

    std::string selectedLabel = "敵データなし";
    if (itemCount > 0) {
        const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(selected)];
        selectedLabel = enemyTestEnemyLabel(enemy);
    }

    std::vector<std::string> labels;
    std::vector<UiDropdownItem> items;
    labels.reserve(enemyCatalog_.enemies.size());
    items.reserve(enemyCatalog_.enemies.size());
    for (std::size_t i = 0; i < enemyCatalog_.enemies.size(); ++i) {
        labels.push_back(enemyTestDropdownItemLabel(enemyCatalog_.enemies[i], static_cast<int>(i)));
        items.push_back(UiDropdownItem{labels.back(), true});
    }

    const UiRect selectRect = enemyTestSelectButtonRect();
    drawUiRectButton(renderer, enemyTestSummonButtonRect(), "召喚", false, uiActionButtonStyle());
    drawUiRectButton(renderer, enemyTestClearButtonRect(), "全消去", false, uiCancelButtonStyle());
    drawUiRectButton(renderer, enemyTestHideButtonRect(), "UI非表示", false);
    drawUiRectButton(renderer, enemyTestExitButtonRect(), "終了", false, uiCancelButtonStyle());

    if (!enemyTestStatus_.empty()) {
        renderer.fillRect({18.0f, 144.0f}, {430.0f, 26.0f}, {0, 0, 0, 160});
        renderer.drawText({26.0f, 150.0f}, fittedSingleLineText(renderer, enemyTestStatus_, 410.0f, 2), {255, 230, 150, 255}, 2);
    }

    drawUiDropdown(
        renderer,
        enemyTestDropdown_,
        selectRect,
        selectedLabel,
        items.empty() ? nullptr : items.data(),
        itemCount,
        enemyTestDropdownStyle());
}

void Game::resetBaseEditDragState()
{
    baseEditDraggingFacilityMove_ = false;
    baseEditDraggingFacilityResize_ = false;
    baseEditResizeMask_ = 0;
    baseEditPassPaintActive_ = false;
    baseEditPassPaintSetBlocked_ = false;
    baseEditPassPaintLastTileX_ = std::numeric_limits<int>::min();
    baseEditPassPaintLastTileY_ = std::numeric_limits<int>::min();
}

void Game::enterBaseEditMode()
{
    if (mode_ != ScreenMode::Base) {
        return;
    }
    baseMiningStartChoiceActive_ = false;
    baseWarpPointSelectActive_ = false;
    baseStorageActive_ = false;
    baseStorageMode_ = StorageUiMode::Closed;
    baseStorageQuantityDialog_ = {};
    baseStorageQuantityPending_ = {};
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    baseMerchantSellSource_ = 0;
    baseMerchantSellSourceTabs_ = {};
    closeUiCommandMenu(baseMerchantSellCommandMenu_);
    baseMerchantSellCommandSource_ = 0;
    baseMerchantSellCommandIndex_ = -1;
    closeUiCommandMenu(baseMerchantBuyCommandMenu_);
    baseMerchantBuyCommandIndex_ = -1;
    baseUpgradeActive_ = false;
    baseProcessingUiMode_ = ProcessingUiMode::Closed;
    baseProcessingActionSelection_ = 0;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingCommandSlot_ = -1;
    baseRingWorkshopActive_ = false;
    baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
    baseRingWorkshopSelection_ = 0;
    baseRingWorkshopRingIndex_ = 0;
    baseRingWorkshopRingTabs_ = {};
    baseRingWorkshopUpgradeTabs_ = {};
    baseRingWorkshopUpgradeScrollOffset_ = 0.0f;
    baseRingWorkshopUpgradeScroll_ = {};
    baseBookshelfActive_ = false;
    bookshelfScrollOffset_ = 0.0f;
    bookshelfScrollState_ = {};

    baseEditEnabled_ = true;
    baseEditMode_ = BaseEditMode::Facility;
    baseEditPassabilityLayer_ = currentBasePassabilityLayer();
    baseEditSelectedFacilityIndex_ = -1;
    resetBaseEditDragState();
}

void Game::exitBaseEditMode()
{
    baseEditEnabled_ = false;
    baseEditMode_ = BaseEditMode::None;
    baseEditSelectedFacilityIndex_ = -1;
    resetBaseEditDragState();
}

void Game::pushBaseEditUndoSnapshot()
{
    BaseEditSnapshot snapshot;
    snapshot.outdoorFacilityRects = baseFacilityRectsOutdoor_;
    snapshot.homeFacilityRects = baseFacilityRectsHome_;
    snapshot.outdoorBlockedTilesLocked = baseBlockedTilesOutdoorLocked_;
    snapshot.outdoorBlockedTilesUnlocked = baseBlockedTilesOutdoorUnlocked_;
    snapshot.homeBlockedTilesLocked = baseBlockedTilesHomeLocked_;
    snapshot.homeBlockedTilesUnlocked = baseBlockedTilesHomeUnlocked_;
    baseEditUndoStack_.push_back(std::move(snapshot));
    if (static_cast<int>(baseEditUndoStack_.size()) > BaseEditUndoLimit) {
        baseEditUndoStack_.erase(baseEditUndoStack_.begin());
    }
    baseEditRedoStack_.clear();
}

bool Game::undoBaseEdit()
{
    if (baseEditUndoStack_.empty()) {
        return false;
    }

    BaseEditSnapshot current;
    current.outdoorFacilityRects = baseFacilityRectsOutdoor_;
    current.homeFacilityRects = baseFacilityRectsHome_;
    current.outdoorBlockedTilesLocked = baseBlockedTilesOutdoorLocked_;
    current.outdoorBlockedTilesUnlocked = baseBlockedTilesOutdoorUnlocked_;
    current.homeBlockedTilesLocked = baseBlockedTilesHomeLocked_;
    current.homeBlockedTilesUnlocked = baseBlockedTilesHomeUnlocked_;
    baseEditRedoStack_.push_back(std::move(current));

    const BaseEditSnapshot snapshot = std::move(baseEditUndoStack_.back());
    baseEditUndoStack_.pop_back();
    baseFacilityRectsOutdoor_ = snapshot.outdoorFacilityRects;
    baseFacilityRectsHome_ = snapshot.homeFacilityRects;
    baseBlockedTilesOutdoorLocked_ = snapshot.outdoorBlockedTilesLocked;
    baseBlockedTilesOutdoorUnlocked_ = snapshot.outdoorBlockedTilesUnlocked;
    baseBlockedTilesHomeLocked_ = snapshot.homeBlockedTilesLocked;
    baseBlockedTilesHomeUnlocked_ = snapshot.homeBlockedTilesUnlocked;
    baseEditDirty_ = true;
    return true;
}

bool Game::redoBaseEdit()
{
    if (baseEditRedoStack_.empty()) {
        return false;
    }

    BaseEditSnapshot current;
    current.outdoorFacilityRects = baseFacilityRectsOutdoor_;
    current.homeFacilityRects = baseFacilityRectsHome_;
    current.outdoorBlockedTilesLocked = baseBlockedTilesOutdoorLocked_;
    current.outdoorBlockedTilesUnlocked = baseBlockedTilesOutdoorUnlocked_;
    current.homeBlockedTilesLocked = baseBlockedTilesHomeLocked_;
    current.homeBlockedTilesUnlocked = baseBlockedTilesHomeUnlocked_;
    baseEditUndoStack_.push_back(std::move(current));
    if (static_cast<int>(baseEditUndoStack_.size()) > BaseEditUndoLimit) {
        baseEditUndoStack_.erase(baseEditUndoStack_.begin());
    }

    const BaseEditSnapshot snapshot = std::move(baseEditRedoStack_.back());
    baseEditRedoStack_.pop_back();
    baseFacilityRectsOutdoor_ = snapshot.outdoorFacilityRects;
    baseFacilityRectsHome_ = snapshot.homeFacilityRects;
    baseBlockedTilesOutdoorLocked_ = snapshot.outdoorBlockedTilesLocked;
    baseBlockedTilesOutdoorUnlocked_ = snapshot.outdoorBlockedTilesUnlocked;
    baseBlockedTilesHomeLocked_ = snapshot.homeBlockedTilesLocked;
    baseBlockedTilesHomeUnlocked_ = snapshot.homeBlockedTilesUnlocked;
    baseEditDirty_ = true;
    return true;
}

void Game::updateBaseEditScreen(const Input& input, UiContext& ui, float)
{
    if (!baseEditEnabled_ || mode_ != ScreenMode::Base) {
        return;
    }

    if (input.undoShortcutPressed()) {
        if (undoBaseEdit()) {
            baseStatus_ = "Base edit: undo";
        }
    }
    if (input.redoShortcutPressed()) {
        if (redoBaseEdit()) {
            baseStatus_ = "Base edit: redo";
        }
    }
    if (input.saveShortcutPressed()) {
        std::string message;
        if (saveBaseEditData(message)) {
            baseStatus_ = message;
        } else {
            baseStatus_ = message;
        }
    }
    if (input.backPressed()) {
        exitBaseEditMode();
        baseStatus_ = "Base edit: off";
        return;
    }

    if (ui.pressed(baseEditModeButtonRect(0))) {
        baseEditMode_ = BaseEditMode::Facility;
        resetBaseEditDragState();
        return;
    }
    if (ui.pressed(baseEditModeButtonRect(1))) {
        baseEditMode_ = BaseEditMode::Passability;
        resetBaseEditDragState();
        return;
    }
    if (ui.pressed(baseEditSaveButtonRect())) {
        std::string message;
        saveBaseEditData(message);
        baseStatus_ = message;
        return;
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }

    if (baseEditMode_ == BaseEditMode::Facility) {
        if (baseEditSelectedFacilityIndex_ < 0 || baseEditSelectedFacilityIndex_ >= static_cast<int>(facilities.size())) {
            baseEditSelectedFacilityIndex_ = -1;
        }

        if (input.mouseLeftPressed() && !ui.pointerConsumed()) {
            const Vec2 mouse = ui.mouse();
            bool startedDrag = false;
            if (baseEditSelectedFacilityIndex_ >= 0) {
                const BaseFacility& selected = facilities[static_cast<std::size_t>(baseEditSelectedFacilityIndex_)];
                const int resizeMask = baseEditResizeMaskAtPoint(selected.rect, mouse);
                if (resizeMask != 0) {
                    pushBaseEditUndoSnapshot();
                    baseEditDraggingFacilityResize_ = true;
                    baseEditResizeMask_ = resizeMask;
                    baseEditDragStartMouse_ = mouse;
                    baseEditDragStartRect_ = toBaseEditRect(selected.rect);
                    startedDrag = true;
                } else if (pointInExpandedRect(mouse, selected.rect, 0.0f)) {
                    pushBaseEditUndoSnapshot();
                    baseEditDraggingFacilityMove_ = true;
                    baseEditDragStartMouse_ = mouse;
                    baseEditDragStartRect_ = toBaseEditRect(selected.rect);
                    startedDrag = true;
                }
            }

            if (!startedDrag) {
                for (int i = static_cast<int>(facilities.size()) - 1; i >= 0; --i) {
                    if (!facilities[static_cast<std::size_t>(i)].rect.contains(mouse)) {
                        continue;
                    }
                    baseEditSelectedFacilityIndex_ = i;
                    pushBaseEditUndoSnapshot();
                    baseEditDraggingFacilityMove_ = true;
                    baseEditDragStartMouse_ = mouse;
                    baseEditDragStartRect_ = toBaseEditRect(facilities[static_cast<std::size_t>(i)].rect);
                    startedDrag = true;
                    break;
                }
            }

            if (!startedDrag) {
                baseEditSelectedFacilityIndex_ = -1;
            } else {
                ui.consumePointer();
            }
        }

        if ((baseEditDraggingFacilityMove_ || baseEditDraggingFacilityResize_) && input.mouseLeftHeld() && baseEditSelectedFacilityIndex_ >= 0) {
            const Vec2 mouse = ui.mouse();
            const Vec2 delta = mouse - baseEditDragStartMouse_;
            BaseEditRect rect = baseEditDragStartRect_;
            if (baseEditDraggingFacilityMove_) {
                rect.x += delta.x;
                rect.y += delta.y;
                rect = normalizeBaseEditRect(rect);
            } else if (baseEditDraggingFacilityResize_) {
                const float minSize = BaseEditFacilityMinSize;
                float left = rect.x;
                float right = rect.x + rect.w;
                float top = rect.y;
                float bottom = rect.y + rect.h;
                if ((baseEditResizeMask_ & 1) != 0) {
                    left += delta.x;
                }
                if ((baseEditResizeMask_ & 2) != 0) {
                    right += delta.x;
                }
                if ((baseEditResizeMask_ & 4) != 0) {
                    top += delta.y;
                }
                if ((baseEditResizeMask_ & 8) != 0) {
                    bottom += delta.y;
                }
                if (right - left < minSize) {
                    if ((baseEditResizeMask_ & 1) != 0 && (baseEditResizeMask_ & 2) == 0) {
                        left = right - minSize;
                    } else {
                        right = left + minSize;
                    }
                }
                if (bottom - top < minSize) {
                    if ((baseEditResizeMask_ & 4) != 0 && (baseEditResizeMask_ & 8) == 0) {
                        top = bottom - minSize;
                    } else {
                        bottom = top + minSize;
                    }
                }
                rect = normalizeBaseEditRect({left, top, right - left, bottom - top});
            }
            const BaseFacility& facility = facilities[static_cast<std::size_t>(baseEditSelectedFacilityIndex_)];
            setBaseFacilityRectFor(baseArea_, facility.facilityId, rect);
            baseEditDirty_ = true;
        }

        if (input.mouseLeftReleased()) {
            resetBaseEditDragState();
        }
    } else if (baseEditMode_ == BaseEditMode::Passability) {
        if (!baseEditPassPaintActive_ && input.toggleShortcutRowPressed()) {
            baseEditPassabilityLayer_ = baseEditPassabilityLayer_ == BaseEditPassabilityLayer::Locked
                ? BaseEditPassabilityLayer::Unlocked
                : BaseEditPassabilityLayer::Locked;
            baseStatus_ = baseEditPassabilityLayer_ == BaseEditPassabilityLayer::Unlocked
                ? "Passability layer: unlocked"
                : "Passability layer: locked";
            resetBaseEditDragState();
            return;
        }
        if (input.copyShortcutPressed()) {
            copyBasePassabilityLayer();
            baseStatus_ = "Passability copied";
            return;
        }
        if (input.pasteShortcutPressed()) {
            if (pasteBasePassabilityLayer()) {
                baseStatus_ = "Passability pasted";
            } else {
                baseStatus_ = baseEditPassabilityClipboardValid_
                    ? "Passability paste: no changes"
                    : "Passability paste: clipboard empty";
            }
            return;
        }

        const UiRect map = baseMapBounds();
        const auto toTile = [](Vec2 position) {
            return std::pair<int, int>{
                static_cast<int>(std::floor(position.x / static_cast<float>(BaseEditGridSize))),
                static_cast<int>(std::floor(position.y / static_cast<float>(BaseEditGridSize))),
            };
        };
        const auto validTile = [](int tileX, int tileY) {
            return tileX >= 0 && tileY >= 0 &&
                tileX * BaseEditGridSize < static_cast<int>(baseMapBounds().size.x) &&
                tileY * BaseEditGridSize < static_cast<int>(baseMapBounds().size.y);
        };
        const auto floodFillBlocked = [this, &validTile](int startTileX, int startTileY) {
            if (!validTile(startTileX, startTileY) || isBasePassabilityBlocked(baseArea_, startTileX, startTileY)) {
                return false;
            }

            std::vector<std::pair<int, int>> queue;
            queue.reserve(512);
            queue.push_back({startTileX, startTileY});
            std::size_t head = 0;
            bool changed = false;

            while (head < queue.size()) {
                const auto [tileX, tileY] = queue[head++];
                if (!validTile(tileX, tileY) || isBasePassabilityBlocked(baseArea_, tileX, tileY)) {
                    continue;
                }

                setBasePassabilityBlocked(baseArea_, tileX, tileY, true);
                changed = true;
                queue.push_back({tileX - 1, tileY});
                queue.push_back({tileX + 1, tileY});
                queue.push_back({tileX, tileY - 1});
                queue.push_back({tileX, tileY + 1});
            }

            return changed;
        };

        if (input.mouseLeftPressed() && !ui.pointerConsumed() && map.contains(ui.mouse())) {
            const auto [tileX, tileY] = toTile(ui.mouse());
            if (validTile(tileX, tileY)) {
                const bool shiftDown = []() {
                    const bool* keys = SDL_GetKeyboardState(nullptr);
                    return keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
                }();
                if (shiftDown) {
                    pushBaseEditUndoSnapshot();
                    const bool changed = floodFillBlocked(tileX, tileY);
                    if (!changed && !baseEditUndoStack_.empty()) {
                        baseEditUndoStack_.pop_back();
                    } else if (changed) {
                        baseEditDirty_ = true;
                        baseStatus_ = "Passability fill: blocked";
                    }
                    ui.consumePointer();
                    return;
                }
                pushBaseEditUndoSnapshot();
                baseEditPassPaintActive_ = true;
                baseEditPassPaintSetBlocked_ = !isBasePassabilityBlocked(baseArea_, tileX, tileY);
                setBasePassabilityBlocked(baseArea_, tileX, tileY, baseEditPassPaintSetBlocked_);
                baseEditPassPaintLastTileX_ = tileX;
                baseEditPassPaintLastTileY_ = tileY;
                baseEditDirty_ = true;
                ui.consumePointer();
            }
        }
        if (baseEditPassPaintActive_ && input.mouseLeftHeld() && map.contains(ui.mouse())) {
            const auto [tileX, tileY] = toTile(ui.mouse());
            if (validTile(tileX, tileY) &&
                (tileX != baseEditPassPaintLastTileX_ || tileY != baseEditPassPaintLastTileY_)) {
                setBasePassabilityBlocked(baseArea_, tileX, tileY, baseEditPassPaintSetBlocked_);
                baseEditPassPaintLastTileX_ = tileX;
                baseEditPassPaintLastTileY_ = tileY;
                baseEditDirty_ = true;
            }
        }
        if (input.mouseLeftReleased()) {
            resetBaseEditDragState();
        }
    }

    ui.block(baseMapBounds());
}

void Game::renderBaseEditOverlay(Renderer& renderer) const
{
    if (!baseEditEnabled_ || !basePresentationActive()) {
        return;
    }

    const bool facilityMode = baseEditMode_ == BaseEditMode::Facility;
    const bool passabilityMode = baseEditMode_ == BaseEditMode::Passability;

    if (passabilityMode) {
        const auto& blocked = baseBlockedTilesFor(baseArea_, editedBasePassabilityLayer());
        for (const std::int64_t packed : blocked) {
            const float x = static_cast<float>(baseEditTileXFromPacked(packed) * BaseEditGridSize);
            const float y = static_cast<float>(baseEditTileYFromPacked(packed) * BaseEditGridSize);
            renderer.fillRect({x, y}, {static_cast<float>(BaseEditGridSize), static_cast<float>(BaseEditGridSize)}, {220, 50, 50, 120});
        }
    }

    if (facilityMode) {
        std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
        for (BaseFacility& facility : facilities) {
            facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
        }
        for (const BaseFacility& facility : facilities) {
            const std::string_view facilityId = facility.facilityId;
            const bool hiddenTransitionRect =
                (baseArea_ == BaseArea::Outdoor && facilityId == "home_entrance") ||
                (baseArea_ == BaseArea::HomeInterior && facilityId == "home_exit");
            if (!hiddenTransitionRect) {
                continue;
            }

            renderer.fillRect(facility.rect.pos, facility.rect.size, {255, 64, 180, 70});
            renderer.drawRect(facility.rect.pos, facility.rect.size, {255, 220, 80, 255});
            renderer.drawOutlinedText(
                facility.rect.pos + Vec2{4.0f, -24.0f},
                facility.displayName,
                {255, 244, 180, 255},
                {0, 0, 0, 180},
                4,
                2);
        }
        if (baseEditSelectedFacilityIndex_ >= 0 && baseEditSelectedFacilityIndex_ < static_cast<int>(facilities.size())) {
            const UiRect rect = facilities[static_cast<std::size_t>(baseEditSelectedFacilityIndex_)].rect;
            renderer.drawRect(rect.pos, rect.size, {70, 220, 255, 255});
            const Vec2 center = rect.pos + rect.size * 0.5f;
            const std::array<Vec2, 8> handles{
                Vec2{rect.pos.x, rect.pos.y},
                Vec2{center.x, rect.pos.y},
                Vec2{rect.pos.x + rect.size.x, rect.pos.y},
                Vec2{rect.pos.x, center.y},
                Vec2{rect.pos.x + rect.size.x, center.y},
                Vec2{rect.pos.x, rect.pos.y + rect.size.y},
                Vec2{center.x, rect.pos.y + rect.size.y},
                Vec2{rect.pos.x + rect.size.x, rect.pos.y + rect.size.y},
            };
            for (Vec2 handle : handles) {
                renderer.fillRect(
                    {handle.x - BaseEditHandleSize * 0.5f, handle.y - BaseEditHandleSize * 0.5f},
                    {BaseEditHandleSize, BaseEditHandleSize},
                    {70, 220, 255, 255});
            }
        }
    }

    renderer.fillRect({884.0f, 38.0f}, {360.0f, passabilityMode ? 172.0f : 112.0f}, {10, 16, 28, 210});
    renderer.drawRect({884.0f, 38.0f}, {360.0f, passabilityMode ? 172.0f : 112.0f}, {130, 168, 232, 255});
    drawUiButton(renderer, baseEditModeButtonRect(0), "Facility", facilityMode);
    drawUiButton(renderer, baseEditModeButtonRect(1), "Passability", passabilityMode);
    drawUiButton(renderer, baseEditSaveButtonRect(), "Save", false, uiActionButtonStyle());
    if (passabilityMode) {
        const std::string layer = baseEditPassabilityLayer_ == BaseEditPassabilityLayer::Unlocked
            ? "Layer: unlocked"
            : "Layer: locked";
        renderer.drawText({898.0f, 132.0f}, layer + "   Tab switch", {218, 232, 255, 255}, 2);
        renderer.drawText({898.0f, 158.0f}, "Ctrl+C copy / Ctrl+V paste", {198, 198, 206, 255}, 2);
    } else {
        renderer.drawText({898.0f, 132.0f}, baseEditDirty_ ? "Unsaved changes (*)" : "Saved", {255, 230, 150, 255}, 2);
        renderer.drawText({898.0f, 158.0f}, "Ctrl+S save / Ctrl+Z undo / Esc exit", {198, 198, 206, 255}, 2);
    }
    if (passabilityMode) {
        renderer.drawText({898.0f, 184.0f}, baseEditDirty_ ? "Unsaved changes (*)" : "Saved", {255, 230, 150, 255}, 2);
    }
}

bool Game::handleBaseEditCommand(std::string_view normalized)
{
    const auto ensureBaseMode = [this]() {
        if (mode_ == ScreenMode::Base) {
            return true;
        }
        logWarning("Debug: base edit is available only while in base.");
        return false;
    };

    if (normalized == "game base-edit toggle") {
        if (!baseEditEnabled_) {
            if (!ensureBaseMode()) {
                return true;
            }
            enterBaseEditMode();
            logInfo("Debug: base edit enabled.");
        } else {
            exitBaseEditMode();
            logInfo("Debug: base edit disabled.");
        }
        return true;
    }
    if (normalized == "game base-edit on") {
        if (!ensureBaseMode()) {
            return true;
        }
        enterBaseEditMode();
        logInfo("Debug: base edit enabled.");
        return true;
    }
    if (normalized == "game base-edit off") {
        exitBaseEditMode();
        logInfo("Debug: base edit disabled.");
        return true;
    }
    if (normalized == "game base-edit facility") {
        if (!ensureBaseMode()) {
            return true;
        }
        if (!baseEditEnabled_) {
            enterBaseEditMode();
        }
        baseEditMode_ = BaseEditMode::Facility;
        logInfo("Debug: base edit mode = facility.");
        return true;
    }
    if (normalized == "game base-edit passability") {
        if (!ensureBaseMode()) {
            return true;
        }
        if (!baseEditEnabled_) {
            enterBaseEditMode();
        }
        baseEditMode_ = BaseEditMode::Passability;
        logInfo("Debug: base edit mode = passability.");
        return true;
    }
    if (normalized == "game base-edit save") {
        if (!ensureBaseMode()) {
            return true;
        }
        std::string message;
        if (saveBaseEditData(message)) {
            logInfo("Debug: " + message);
        } else {
            logWarning("Debug: " + message);
        }
        return true;
    }

    return false;
}

bool Game::handleObjectImageScaleCommand(std::string_view normalized)
{
    const bool toggle = normalized == "game obj-image-scale toggle" || normalized == "game image-scale toggle";
    const bool enable = normalized == "game obj-image-scale on" || normalized == "game image-scale on";
    const bool disable = normalized == "game obj-image-scale off" || normalized == "game image-scale off";
    const bool save = normalized == "game obj-image-scale save" || normalized == "game image-scale save";

    if (toggle) {
        if (mode_ == ScreenMode::ObjectImageScaleEdit) {
            exitObjectImageScaleEditMode();
            logInfo("Debug: image scale edit disabled.");
        } else {
            enterObjectImageScaleEditMode();
            logInfo("Debug: image scale edit enabled.");
        }
        return true;
    }
    if (enable) {
        enterObjectImageScaleEditMode();
        logInfo("Debug: image scale edit enabled.");
        return true;
    }
    if (disable) {
        exitObjectImageScaleEditMode();
        logInfo("Debug: image scale edit disabled.");
        return true;
    }
    if (save) {
        std::string message;
        if (saveObjectImageScaleData(message)) {
            objectImageScaleStatus_ = message;
            logInfo("Debug: " + message);
        } else {
            objectImageScaleStatus_ = message;
            logWarning("Debug: " + message);
        }
        return true;
    }

    return false;
}

bool Game::handleEnemyHitboxEditCommand(std::string_view normalized)
{
    const bool toggle = normalized == "game enemy-hitbox toggle" ||
        normalized == "game enemy hitbox toggle" ||
        normalized == "game hitbox toggle";
    const bool enable = normalized == "game enemy-hitbox on" ||
        normalized == "game enemy hitbox on" ||
        normalized == "game hitbox on";
    const bool disable = normalized == "game enemy-hitbox off" ||
        normalized == "game enemy hitbox off" ||
        normalized == "game hitbox off";
    const bool save = normalized == "game enemy-hitbox save" ||
        normalized == "game enemy hitbox save" ||
        normalized == "game hitbox save";
    const bool reload = normalized == "game enemy-hitbox reload" ||
        normalized == "game enemy hitbox reload" ||
        normalized == "game hitbox reload";

    if (toggle) {
        if (mode_ == ScreenMode::EnemyHitboxEdit) {
            exitEnemyHitboxEditMode();
            logInfo("Debug: hitbox edit disabled.");
        } else {
            enterEnemyHitboxEditMode();
            logInfo("Debug: hitbox edit enabled.");
        }
        return true;
    }
    if (enable) {
        enterEnemyHitboxEditMode();
        logInfo("Debug: hitbox edit enabled.");
        return true;
    }
    if (disable) {
        exitEnemyHitboxEditMode();
        logInfo("Debug: hitbox edit disabled.");
        return true;
    }
    if (save) {
        std::string message;
        if (saveHitboxData(message)) {
            enemyHitboxStatus_ = message;
            logInfo("Debug: " + message);
        } else {
            enemyHitboxStatus_ = message;
            logWarning("Debug: " + message);
        }
        return true;
    }
    if (reload) {
        if (loadHitboxData()) {
            enemyHitboxStatus_ = "Hitboxes reloaded";
            logInfo("Debug: hitboxes reloaded.");
        } else {
            enemyHitboxStatus_ = "Hitbox reload failed";
            logWarning("Debug: hitbox reload failed.");
        }
        rebuildEnemyHitboxEditList();
        enemies_.setHitboxCatalog(&hitboxes_);
        return true;
    }

    return false;
}

bool Game::handleEnemyShadowEditCommand(std::string_view normalized)
{
    const bool toggle = normalized == "game enemy-shadow toggle" ||
        normalized == "game enemy shadow toggle" ||
        normalized == "game shadow toggle";
    const bool enable = normalized == "game enemy-shadow on" ||
        normalized == "game enemy shadow on" ||
        normalized == "game shadow on";
    const bool disable = normalized == "game enemy-shadow off" ||
        normalized == "game enemy shadow off" ||
        normalized == "game shadow off";
    const bool save = normalized == "game enemy-shadow save" ||
        normalized == "game enemy shadow save" ||
        normalized == "game shadow save";
    const bool reload = normalized == "game enemy-shadow reload" ||
        normalized == "game enemy shadow reload" ||
        normalized == "game shadow reload";

    if (toggle) {
        if (mode_ == ScreenMode::EnemyShadowEdit) {
            exitEnemyShadowEditMode();
            logInfo("Debug: shadow edit disabled.");
        } else {
            enterEnemyShadowEditMode();
            logInfo("Debug: shadow edit enabled.");
        }
        return true;
    }
    if (enable) {
        enterEnemyShadowEditMode();
        logInfo("Debug: shadow edit enabled.");
        return true;
    }
    if (disable) {
        exitEnemyShadowEditMode();
        logInfo("Debug: shadow edit disabled.");
        return true;
    }
    if (save) {
        std::string message;
        if (saveEnemyShadowData(message)) {
            enemyShadowStatus_ = message;
            logInfo("Debug: " + message);
        } else {
            enemyShadowStatus_ = message;
            logWarning("Debug: " + message);
        }
        return true;
    }
    if (reload) {
        if (loadEnemyShadowData()) {
            enemyShadowStatus_ = "Enemy shadows reloaded";
            logInfo("Debug: enemy shadows reloaded.");
        } else {
            enemyShadowStatus_ = "Enemy shadow reload failed";
            logWarning("Debug: enemy shadow reload failed.");
        }
        enemies_.setShadowCatalog(&enemyShadows_);
        return true;
    }

    return false;
}
bool Game::handleAudioCueEditCommand(std::string_view normalized)
{
    const bool bgm = normalized == "game audio-edit bgm" ||
        normalized == "game bgm-edit" ||
        normalized == "game audio bgm";
    const bool se = normalized == "game audio-edit se" ||
        normalized == "game se-edit" ||
        normalized == "game sfx-edit" ||
        normalized == "game audio se";
    const bool close = normalized == "game audio-edit close" ||
        normalized == "game audio-edit off";
    const bool save = normalized == "game audio-edit save" ||
        normalized == "game audio save";

    if (bgm) {
        enterAudioCueEditMode(AudioCueEditMode::Bgm);
        logInfo("Debug: BGM edit opened.");
        return true;
    }
    if (se) {
        enterAudioCueEditMode(AudioCueEditMode::Se);
        logInfo("Debug: sound effect edit opened.");
        return true;
    }
    if (close) {
        exitAudioCueEditMode();
        logInfo("Debug: audio edit closed.");
        return true;
    }
    if (save) {
        std::string message;
        if (saveAudioCueManifestFromEdit(message)) {
            audioCueEditStatus_ = message;
            logInfo("Debug: " + message);
        } else {
            audioCueEditStatus_ = message;
            logWarning("Debug: " + message);
        }
        return true;
    }

    return false;
}

bool Game::handleDebugItemPickerCommand(std::string_view normalized)
{
    const bool toggle = normalized == "game items picker" ||
        normalized == "game item picker" ||
        normalized == "game item-picker toggle" ||
        normalized == "game items-picker toggle";
    const bool open = normalized == "game items picker on" ||
        normalized == "game item-picker on" ||
        normalized == "game items add-select";
    const bool close = normalized == "game items picker off" ||
        normalized == "game item-picker off" ||
        normalized == "game item-picker close";

    if (toggle) {
        if (debugItemPickerActive_) {
            closeDebugItemPicker();
            logInfo("Debug: item picker closed.");
        } else {
            openDebugItemPicker();
            if (debugItemPickerActive_) {
                logInfo("Debug: item picker opened.");
            }
        }
        return true;
    }
    if (open) {
        openDebugItemPicker();
        if (debugItemPickerActive_) {
            logInfo("Debug: item picker opened.");
        }
        return true;
    }
    if (close) {
        closeDebugItemPicker();
        logInfo("Debug: item picker closed.");
        return true;
    }

    return false;
}

bool Game::handleDebugStoryTestCommand(std::string_view normalized)
{
    const bool events = normalized == "game story-test events" ||
        normalized == "game story test events" ||
        normalized == "game event-test" ||
        normalized == "game event test";
    const bool tutorials = normalized == "game story-test tutorials" ||
        normalized == "game story test tutorials" ||
        normalized == "game tutorial-test" ||
        normalized == "game tutorial test";
    const bool close = normalized == "game story-test close" ||
        normalized == "game story test close" ||
        normalized == "game dialogue-test close";

    if (events) {
        openDebugStoryTest(DebugStoryTestMode::Events);
        if (debugStoryTestActive_) {
            logInfo("Debug: story event test opened.");
        }
        return true;
    }
    if (tutorials) {
        openDebugStoryTest(DebugStoryTestMode::Tutorials);
        if (debugStoryTestActive_) {
            logInfo("Debug: story tutorial test opened.");
        }
        return true;
    }
    if (close) {
        closeDebugStoryTest();
        logInfo("Debug: story test closed.");
        return true;
    }

    return false;
}

GameTestSnapshot Game::makeTestSnapshot(GameTestSnapshotOptions options) const
{
    const int ringCount = unlockedRingCount();
    const auto screenMode = [](ScreenMode mode) {
        switch (mode) {
        case ScreenMode::OpeningKamishibai: return GameTestScreenMode::OpeningKamishibai;
        case ScreenMode::EndingKamishibai: return GameTestScreenMode::EndingKamishibai;
        case ScreenMode::Title: return GameTestScreenMode::Title;
        case ScreenMode::Base: return GameTestScreenMode::Base;
        case ScreenMode::WorldLoading: return GameTestScreenMode::WorldLoading;
        case ScreenMode::Playing: return GameTestScreenMode::Playing;
        case ScreenMode::PauseMenu: return GameTestScreenMode::PauseMenu;
        case ScreenMode::Inventory: return GameTestScreenMode::Inventory;
        case ScreenMode::Ring: return GameTestScreenMode::Ring;
        case ScreenMode::ObjectImageScaleEdit: return GameTestScreenMode::ObjectImageScaleEdit;
        case ScreenMode::EnemyHitboxEdit: return GameTestScreenMode::EnemyHitboxEdit;
        case ScreenMode::EnemyShadowEdit: return GameTestScreenMode::EnemyHitboxEdit;
        case ScreenMode::AudioCueEdit: return GameTestScreenMode::AudioCueEdit;
        case ScreenMode::LevelUp: return GameTestScreenMode::LevelUp;
        case ScreenMode::GameOver: return GameTestScreenMode::GameOver;
        case ScreenMode::StageClear: return GameTestScreenMode::StageClear;
        case ScreenMode::AstralResult: return GameTestScreenMode::AstralResult;
        }
        return GameTestScreenMode::Base;
    };
    const auto ringState = [](SpellRingState state) {
        switch (state) {
        case SpellRingState::Normal: return GameTestRingState::Normal;
        case SpellRingState::Thrown: return GameTestRingState::Thrown;
        case SpellRingState::Returning: return GameTestRingState::Returning;
        }
        return GameTestRingState::Normal;
    };
    const auto dropKind = [](WorldDropKind kind) {
        switch (kind) {
        case WorldDropKind::Object: return GameTestDropKind::Object;
        case WorldDropKind::Money: return GameTestDropKind::Money;
        case WorldDropKind::Material: return GameTestDropKind::Material;
        }
        return GameTestDropKind::Object;
    };
    const auto codexStageForItem = [this](const ItemData& item) {
        const bool treasure = item.category == "\xE5\xAE\x9D";
        return static_cast<GameTestCodexStage>(static_cast<int>(encyclopedia_.objectStage(item.id, treasure)));
    };
    const auto fillProcessingState = [this](
        GameTestObjectEntrySnapshot& entry,
        StorageEntry storageEntry,
        bool warehouseEntry,
        int sourceCount) {
        const int usedSlots = warehouseEntry ? warehouseUsedSlots() : backpackUsedSlots();
        const int capacity = warehouseEntry ? warehouseCapacity() : inventory_.screenSlotCount();
        const bool canCreateInstanceSlot =
            storageEntry.kind != StorageEntryKind::Stack ||
            sourceCount <= 1 ||
            usedSlots < capacity;
        const int enhancementOre = inventory_.materialCount(MaterialType::EnhancementOre);

        const auto fillMode = [&](ProcessingMode mode, bool& canUse, int& moneyCost, int& oreCost) {
            moneyCost = processingMoneyCost(storageEntry, mode, warehouseEntry);
            oreCost = processingOreCost(storageEntry, mode, warehouseEntry);
            canUse =
                processingEntryAvailable(storageEntry, mode, warehouseEntry) &&
                (mode == ProcessingMode::Repair || canCreateInstanceSlot) &&
                money_ >= moneyCost &&
                enhancementOre >= oreCost;
        };

        int repairOreCost = 0;
        fillMode(ProcessingMode::Repair, entry.canRepair, entry.repairMoneyCost, repairOreCost);
        fillMode(ProcessingMode::Attack, entry.canEnhanceAttack, entry.enhanceAttackMoneyCost, entry.enhanceAttackOreCost);
        fillMode(ProcessingMode::Dig, entry.canEnhanceDig, entry.enhanceDigMoneyCost, entry.enhanceDigOreCost);
    };
    const auto ringSourceForIndex = [](int ringIndex) {
        switch (ringIndex) {
        case 0: return BaseItemSource::Ring0;
        case 1: return BaseItemSource::Ring1;
        default: return BaseItemSource::Ring2;
        }
    };
    const auto fillRingProcessingState = [this, &ringSourceForIndex](
        GameTestRingItemSnapshot& entry,
        int ringIndex,
        int itemIndex) {
        ProcessingTarget target;
        target.source = ringSourceForIndex(ringIndex);
        target.ringIndex = ringIndex;
        target.ringItemIndex = itemIndex;
        target.valid = true;
        const int enhancementOre = inventory_.materialCount(MaterialType::EnhancementOre);

        const auto fillMode = [&](ProcessingMode mode, bool& canUse, int& moneyCost, int& oreCost) {
            moneyCost = processingMoneyCost(target, mode);
            oreCost = processingOreCost(target, mode);
            canUse =
                processingTargetAvailable(target, mode) &&
                money_ >= moneyCost &&
                enhancementOre >= oreCost;
        };

        int repairOreCost = 0;
        fillMode(ProcessingMode::Repair, entry.canRepair, entry.repairMoneyCost, repairOreCost);
        fillMode(ProcessingMode::Attack, entry.canEnhanceAttack, entry.enhanceAttackMoneyCost, entry.enhanceAttackOreCost);
        fillMode(ProcessingMode::Dig, entry.canEnhanceDig, entry.enhanceDigMoneyCost, entry.enhanceDigOreCost);
    };
    const auto fillUseEffects = [](GameTestObjectEntrySnapshot& entry, const ItemData& item) {
        for (const EffectSpec& spec : item.normalEffects) {
            const std::size_t count = std::min(spec.effects.size(), spec.values.size());
            for (std::size_t i = 0; i < count; ++i) {
                entry.useEffects.push_back(GameTestUseEffectSnapshot{
                    .target = spec.target,
                    .effect = spec.effects[i],
                    .value = spec.values[i],
                    .duration = spec.duration,
                });
            }
        }
    };
    const auto expectedLoadoutLightRadius = [this](const ItemData& item) {
        const RingEquipmentModifiers& equipment =
            spellRing_.equipmentModifiersForRing(spellRing_.activeRingIndex());
        return autoSimulationLightRadiusFromOrbitEffects(item.orbitEffects) *
            static_cast<float>(std::max(0.0, equipment.lightRadiusMul));
    };
    const auto fillRingAddability = [this, ringCount](
        GameTestObjectEntrySnapshot& entry,
        const ItemData& item,
        const ItemInstance* instance) {
        if (entry.location != GameTestInventoryLocation::Backpack ||
            entry.equipped ||
            entry.broken ||
            item.id.empty()) {
            return;
        }

        for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
            SpellRingSystem ringProbe = spellRing_;
            const int activeRing = ringProbe.activeRingIndex();
            if (activeRing != ringIndex) {
                ringProbe.switchActiveRing(ringIndex - activeRing);
            }
            const bool canAdd = instance != nullptr
                ? ringProbe.canAddObjectItem(item, *instance)
                : ringProbe.canAddObjectItem(item);
            if (canAdd) {
                entry.addableRingIndices.push_back(ringIndex);
            }
        }
    };
    const auto makeStackEntry = [
        this,
        &codexStageForItem,
        &fillProcessingState,
        &fillUseEffects,
        &expectedLoadoutLightRadius,
        &fillRingAddability](
        const InventoryObjectStack& stack,
        GameTestInventoryLocation location,
        int index) {
        GameTestObjectEntrySnapshot entry;
        entry.location = location;
        entry.kind = GameTestObjectEntryKind::Stack;
        entry.objectId = stack.objectId;
        entry.name = stack.item.name;
        entry.category = stack.item.category;
        entry.damageType = stack.item.damageType;
        entry.tags = stack.item.tags;
        fillUseEffects(entry, stack.item);
        entry.count = stack.count;
        entry.rarity = stack.item.rarity;
        entry.price = stack.item.price;
        entry.sellPrice = sellPrice(stack.item);
        entry.attackPower = stack.item.attackPower;
        entry.digPower = stack.item.digPower;
        entry.staffEquipScore = autoSimulationStaffEquipScore(stack.item);
        entry.lightRadius = expectedLoadoutLightRadius(stack.item);
        entry.durability = stack.item.durability;
        entry.weightKg = stack.item.weightKg;
        entry.currentDurability = stack.item.durability;
        entry.maxDurability = stack.item.durability;
        entry.broken = stack.item.durability == 0;
        entry.important = isImportantItem(stack.item);
        entry.sellable = isSellableObject(stack.item);
        entry.codexStage = codexStageForItem(stack.item);
        fillRingAddability(entry, stack.item, nullptr);
        fillProcessingState(
            entry,
            StorageEntry{StorageEntryKind::Stack, index},
            location == GameTestInventoryLocation::Warehouse,
            stack.count);
        return entry;
    };
    const auto makeInstanceEntry = [
        this,
        &codexStageForItem,
        &fillProcessingState,
        &fillUseEffects,
        &expectedLoadoutLightRadius,
        &fillRingAddability](
        const InventoryObjectInstance& objectInstance,
        GameTestInventoryLocation location,
        int index) {
        GameTestObjectEntrySnapshot entry;
        entry.location = location;
        entry.kind = GameTestObjectEntryKind::Instance;
        entry.objectId = objectInstance.item.id;
        entry.instanceId = objectInstance.instance.instanceId;
        entry.name = objectInstance.item.name;
        entry.category = objectInstance.item.category;
        entry.damageType = objectInstance.item.damageType;
        entry.tags = objectInstance.item.tags;
        fillUseEffects(entry, objectInstance.item);
        entry.count = 1;
        entry.rarity = objectInstance.item.rarity;
        entry.price = objectInstance.item.price;
        entry.sellPrice = sellPrice(objectInstance.item, &objectInstance.instance);
        entry.attackPower = objectInstance.item.attackPower;
        entry.digPower = objectInstance.item.digPower;
        entry.staffEquipScore = autoSimulationStaffEquipScore(objectInstance.item);
        entry.lightRadius = expectedLoadoutLightRadius(objectInstance.item);
        entry.durability = objectInstance.item.durability;
        entry.weightKg = objectInstance.item.weightKg;
        entry.currentDurability = objectInstance.instance.currentDurability;
        entry.maxDurability = objectInstance.instance.maxDurability;
        entry.enhanceLevel = objectInstance.instance.enhanceLevel;
        entry.attackBonus = objectInstance.instance.attackBonus;
        entry.digBonus = objectInstance.instance.digBonus;
        entry.durabilityBonus = objectInstance.instance.durabilityBonus;
        entry.weightModifier = objectInstance.instance.weightModifier;
        entry.sizeModifier = objectInstance.instance.sizeModifier;
        entry.protectionEnabled = objectInstance.instance.protectionEnabled;
        entry.equipped =
            location == GameTestInventoryLocation::Backpack &&
            inventory_.isStaffEquipped(objectInstance.instance.instanceId);
        entry.broken = objectInstance.instance.isBroken;
        entry.important = isImportantItem(objectInstance.item);
        entry.sellable = isSellableObject(objectInstance.item);
        entry.codexStage = codexStageForItem(objectInstance.item);
        fillRingAddability(entry, objectInstance.item, &objectInstance.instance);
        fillProcessingState(
            entry,
            StorageEntry{StorageEntryKind::Instance, index},
            location == GameTestInventoryLocation::Warehouse,
            1);
        return entry;
    };

    GameTestSnapshot snapshot;
    snapshot.screenMode = screenMode(mode_);
    snapshot.stageId = currentStageId_;
    snapshot.stageName = currentStageDefinition_.name;
    snapshot.worldLoading = mode_ == ScreenMode::WorldLoading || worldBuildActive();
    snapshot.transitionActive = screenTransition_.active();
    snapshot.dialogueActive = dialogue_.active();
    snapshot.dungeonFocusActive = dungeonFocusActive();
    snapshot.bossPresentationActive = bossEncounterBlocksProgress();
    snapshot.firstItemNoticeActive = firstItemAcquisitionNoticeActive();
    snapshot.pendingStoryDelayActive = pendingStoryTriggerDelayActive();
    snapshot.warpReturnConfirmOpen = warpReturnConfirm_.open;
    snapshot.introTutorialActive = introTutorialActive();
    snapshot.cameraPosition = camera_.position();
    snapshot.viewportWidth = camera_.width();
    snapshot.viewportHeight = camera_.height();
    snapshot.levelUp.choiceActive =
        mode_ == ScreenMode::LevelUp &&
        levels_.isChoosing() &&
        !levelUpPresentation_.active &&
        !levelUpResultDialog_.open;
    snapshot.levelUp.pendingChoices = levels_.pendingChoiceCount();
    snapshot.player.position = player_.position;
    snapshot.player.facing = player_.facing;
    snapshot.player.velocity = player_.velocity;
    snapshot.player.radius = player_.effectiveRadius(balance_.playerRadius);
    snapshot.player.hp = player_.hp;
    snapshot.player.maxHp = player_.maxHp;
    snapshot.player.level = player_.level;
    snapshot.player.states.reserve(player_.status.states().size());
    for (const EntityState& state : player_.status.states()) {
        snapshot.player.states.push_back(GameTestPlayerStateSnapshot{
            .id = state.stateId,
            .value = state.value,
            .duration = state.duration,
        });
    }
    snapshot.player.modifiers.reserve(player_.status.modifiers().size());
    for (const EntityModifier& modifier : player_.status.modifiers()) {
        snapshot.player.modifiers.push_back(GameTestPlayerModifierSnapshot{
            .id = modifier.modifierId,
            .stat = std::string(modifierStatName(modifier.stat)),
            .multiplier = modifier.multiplier,
            .flat = modifier.flat,
            .duration = modifier.duration,
        });
    }
    snapshot.ringState = ringState(spellRing_.state());
    snapshot.ringCenter = spellRing_.center();
    snapshot.ring.activeRingIndex = spellRing_.activeRingIndex();
    snapshot.ring.unlockedRingCount = ringCount;
    snapshot.ring.activeRadius = spellRing_.radius();
    snapshot.ring.activeAngularSpeed = spellRing_.effectiveAngularSpeed();
    snapshot.ring.activeWeight = spellRing_.totalEquippedWeight();
    snapshot.ring.activeMaxWeight = spellRing_.maxEquippedWeight();
    snapshot.ring.anchorOffsetFromPlayer = witchSelfLightCenter(player_.position) - player_.position;
    snapshot.ring.currentOffsetDistance = player_.spellRingShift;
    snapshot.ring.maxOffsetDistance =
        (balance_.spellRingShiftDistance + player_.spellRingShiftDistanceBonus) *
        clamp(player_.spellRingShiftDistanceMultiplier, 0.25f, 3.0f);
    snapshot.ring.activeItemCount = static_cast<int>(spellRing_.items().size());
    snapshot.ring.activeMaxItemCount = spellRing_.maxItemCountForRing(spellRing_.activeRingIndex());
    snapshot.ring.activeCanAddItem = spellRing_.canAddItem();
    snapshot.ring.rings.reserve(ringCount);
    for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
        const RingLevelUpgradePoints points = clampedRingLevelUpgradePoints(
            levelRingUpgradePoints_[static_cast<std::size_t>(ringIndex)]);
        GameTestRingLoadoutSnapshot ring;
        ring.ringIndex = ringIndex;
        ring.radius = spellRing_.radiusForRing(ringIndex);
        ring.angularSpeed = spellRing_.effectiveAngularSpeedForRing(ringIndex);
        ring.weight = spellRing_.totalEquippedWeightForRing(ringIndex);
        ring.maxWeight = spellRing_.maxEquippedWeightForRing(ringIndex);
        ring.itemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
        ring.maxItemCount = spellRing_.maxItemCountForRing(ringIndex);
        ring.radiusUpgradePoints = points.radius;
        ring.speedUpgradePoints = points.speed;
        ring.weightLimitUpgradePoints = points.weightLimit;
        snapshot.ring.rings.push_back(ring);
    }
    for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
        for (int itemIndex = 0; itemIndex < static_cast<int>(ringItems.size()); ++itemIndex) {
            const SpellRingItem& item = ringItems[static_cast<std::size_t>(itemIndex)];
            const auto objectIt = objectCatalog_.objectsById.find(item.objectId);
            const ObjectDefinition* object = objectIt == objectCatalog_.objectsById.end() ? nullptr : &objectIt->second;
            std::vector<std::string> tags = object != nullptr ? object->tags : std::vector<std::string>{};
            tags.insert(tags.end(), item.addedTags.begin(), item.addedTags.end());
            const float itemLightRadius = item.lightRadius > 0.0f
                ? item.lightRadius
                : (item.type == SpellRingItemType::Torch ? balance_.lightRadius : 0.0f);
            GameTestRingItemSnapshot ringEntry{
                .ringIndex = ringIndex,
                .itemIndex = itemIndex,
                .objectId = item.objectId,
                .instanceId = item.instanceId,
                .name = object != nullptr ? object->name : std::string{},
                .category = object != nullptr ? object->category : std::string{},
                .damageType = item.damageType,
                .tags = std::move(tags),
                .worldPosition = item.worldPosition,
                .damage = item.damage,
                .digPower = item.digPower,
                .hitRadius = item.hitRadius,
                .lightRadius = itemLightRadius,
                .durability = item.durability,
                .maxDurability = item.maxDurability,
                .rarity = object != nullptr ? object->rarity : 0,
                .price = object != nullptr ? object->price : 0,
                .weightKg = item.weight,
                .enhanceLevel = item.enhanceLevel,
                .attackBonus = item.attackBonus,
                .digBonus = item.digBonus,
                .durabilityBonus = item.durabilityBonus,
                .protectionEnabled = item.protectionEnabled,
                .broken = item.broken(),
            };
            fillRingProcessingState(ringEntry, ringIndex, itemIndex);
            snapshot.ring.items.push_back(std::move(ringEntry));
            if (ringIndex == spellRing_.activeRingIndex() && !item.broken()) {
                snapshot.ring.bestDamage = std::max(snapshot.ring.bestDamage, item.damage);
                snapshot.ring.bestDigPower = std::max(snapshot.ring.bestDigPower, item.digPower);
                snapshot.ring.bestHitRadius = std::max(snapshot.ring.bestHitRadius, item.hitRadius);
                snapshot.ring.bestLightRadius = std::max(snapshot.ring.bestLightRadius, itemLightRadius);
            }
            if (ringIndex >= 0 && ringIndex < static_cast<int>(snapshot.ring.rings.size()) && !item.broken()) {
                GameTestRingLoadoutSnapshot& ring = snapshot.ring.rings[static_cast<std::size_t>(ringIndex)];
                ring.bestDamage = std::max(ring.bestDamage, item.damage);
                ring.bestDigPower = std::max(ring.bestDigPower, item.digPower);
                ring.bestHitRadius = std::max(ring.bestHitRadius, item.hitRadius);
                ring.bestLightRadius = std::max(ring.bestLightRadius, itemLightRadius);
                ring.hasCombatTool = ring.bestDamage > 0;
                ring.hasDigTool = ring.bestDigPower > 0;
                ring.hasLightTool = ring.bestLightRadius > 0.0f;
            }
        }
    }
    snapshot.ring.hasCombatTool = snapshot.ring.bestDamage > 0;
    snapshot.ring.hasDigTool = snapshot.ring.bestDigPower > 0;
    snapshot.ring.hasLightTool = snapshot.ring.bestLightRadius > 0.0f;
    snapshot.dungeon.active = mode_ == ScreenMode::Playing ||
        mode_ == ScreenMode::Inventory ||
        mode_ == ScreenMode::PauseMenu ||
        mode_ == ScreenMode::Ring ||
        mode_ == ScreenMode::LevelUp ||
        mode_ == ScreenMode::GameOver ||
        mode_ == ScreenMode::StageClear ||
        mode_ == ScreenMode::AstralResult;
    snapshot.dungeon.seed = dungeonLayout_.seed;
    snapshot.dungeon.startWorld = tileWorldCenter(dungeonLayout_.startTile);
    snapshot.dungeon.goalWorld = tileWorldCenter(dungeonLayout_.goalTile);
    snapshot.dungeon.hasBossSpawnPoint = hasBossSpawnPoint_;
    snapshot.dungeon.bossSpawnPoint = bossSpawnPoint_;
    snapshot.dungeon.bossSpawned = bossSpawned_;
    snapshot.dungeon.discoveredWarpPoints = discoveredWarpPointCount();
    snapshot.dungeon.unlockedWarpPoints = unlockedWarpPointCount_;
    snapshot.dungeon.mainPathWorldPoints.reserve(dungeonLayout_.mainPathPoints.size());
    for (Vec2 tilePoint : dungeonLayout_.mainPathPoints) {
        snapshot.dungeon.mainPathWorldPoints.push_back(tileWorldCenter(roundDungeonTile(tilePoint)));
    }
    const Vec2 cameraCenter = camera_.position();
    const float visibleWarpMargin = 72.0f;
    const float visibleWarpHalfWidth = static_cast<float>(camera_.width()) * 0.5f + visibleWarpMargin;
    const float visibleWarpHalfHeight = static_cast<float>(camera_.height()) * 0.5f + visibleWarpMargin;
    snapshot.dungeon.warpPoints.reserve(warpPoints_.size());
    snapshot.dungeon.mapClues.reserve(warpPoints_.size());
    for (const WarpPoint& point : warpPoints_) {
        const bool knownDiscovered =
            point.discovered ||
            point.unlocked ||
            point.index < unlockedWarpPointCount_;
        const bool visibleOnScreen =
            std::abs(point.position.x - cameraCenter.x) <= visibleWarpHalfWidth &&
            std::abs(point.position.y - cameraCenter.y) <= visibleWarpHalfHeight;
        snapshot.dungeon.warpPoints.push_back(GameTestWarpPointSnapshot{
            .position = point.position,
            .index = point.index,
            .discovered = knownDiscovered,
            .visible = knownDiscovered || visibleOnScreen,
        });

        if (knownDiscovered) {
            continue;
        }

        const int clueRadiusTiles = std::max(1, static_cast<int>(std::ceil(point.undiscoveredLightRadiusTiles)));
        int seenTiles = 0;
        Vec2 seenCenterSum{};
        for (int dy = -clueRadiusTiles; dy <= clueRadiusTiles; ++dy) {
            for (int dx = -clueRadiusTiles; dx <= clueRadiusTiles; ++dx) {
                if (dx * dx + dy * dy > clueRadiusTiles * clueRadiusTiles) {
                    continue;
                }
                const int tx = point.tilePosition.x + dx;
                const int ty = point.tilePosition.y + dy;
                if (!dungeonMinimapTileSeen(tx, ty)) {
                    continue;
                }
                ++seenTiles;
                seenCenterSum += tileMap_.tileCenter(tx, ty);
            }
        }

        const bool centerSeen = dungeonMinimapTileSeen(point.tilePosition.x, point.tilePosition.y);
        if (!centerSeen && seenTiles < 4) {
            continue;
        }

        const float maxSeenTiles = std::max(1.0f, 3.14159f * static_cast<float>(clueRadiusTiles * clueRadiusTiles));
        const float coverage = std::clamp(static_cast<float>(seenTiles) / maxSeenTiles, 0.0f, 1.0f);
        snapshot.dungeon.mapClues.push_back(GameTestMapClueSnapshot{
            .position = centerSeen || seenTiles <= 0 ? point.position : seenCenterSum * (1.0f / static_cast<float>(seenTiles)),
            .kind = GameTestMapClueKind::WarpGlow,
            .visibleOnMinimap = true,
            .alreadyVisited = false,
            .confidence = std::clamp(coverage + (centerSeen ? 0.35f : 0.0f), 0.0f, 1.0f),
        });
    }

    std::vector<EnemyMinimapMarker> markers;
    enemies_.appendMinimapMarkers(markers);
    snapshot.enemies.reserve(markers.size());
    for (const EnemyMinimapMarker& marker : markers) {
        snapshot.enemies.push_back(GameTestEnemySnapshot{
            .position = marker.position,
            .radius = marker.radius,
            .jumpLandingRadius = marker.jumpLandingRadius,
            .countdownExplodeRadius = marker.countdownExplodeRadius,
            .contactAttackPower = marker.contactAttackPower,
            .contactDamageMultiplier = marker.contactDamageMultiplier,
            .ranged = marker.ranged,
            .boss = marker.boss,
        });
    }

    snapshot.chests.reserve(chestNodes_.size());
    for (const ChestNode& node : chestNodes_) {
        snapshot.chests.push_back(GameTestChestSnapshot{
            .position = chestVisualCenter(node),
            .revealed = node.revealed,
            .opened = node.opened,
        });
    }

    const std::vector<WorldDropItem>& drops = worldDrops_.drops();
    snapshot.drops.reserve(drops.size());
    for (const WorldDropItem& drop : drops) {
        std::string displayName = drop.id;
        std::string category;
        std::string damageType;
        std::vector<std::string> tags;
        GameTestIconKind iconKind = GameTestIconKind::None;
        std::string iconKey;
        int rarity = 0;
        int price = 0;
        int attackPower = 0;
        int digPower = 0;
        float lightRadius = 0.0f;
        int durability = -1;
        double weightKg = 0.0;
        if (drop.kind == WorldDropKind::Object) {
            const ItemData* object = objectCatalog_.registry.findById(drop.id);
            displayName = object != nullptr ? object->name : drop.id;
            if (object != nullptr) {
                category = object->category;
                damageType = object->damageType;
                tags = object->tags;
                rarity = object->rarity;
                price = object->price;
                attackPower = object->attackPower;
                digPower = object->digPower;
                lightRadius = expectedLoadoutLightRadius(*object);
                durability = object->durability;
                weightKg = object->weightKg;
            }
            iconKind = GameTestIconKind::Object;
            iconKey = drop.id;
        } else if (drop.kind == WorldDropKind::Money) {
            displayName = std::to_string(drop.quantity) + "G";
            iconKind = GameTestIconKind::World;
            iconKey = std::string(worldIconKey(moneyWorldIconForAmount(drop.quantity)));
        } else {
            MaterialType materialType = MaterialType::Count;
            if (materialTypeFromSaveName(drop.id, materialType)) {
                displayName = std::string(materialTypeDisplayName(materialType));
                iconKind = GameTestIconKind::World;
                iconKey = std::string(worldIconKey(materialWorldIcon(materialType)));
            }
        }
        snapshot.drops.push_back(GameTestDropSnapshot{
            .kind = dropKind(drop.kind),
            .id = drop.id,
            .displayName = std::move(displayName),
            .category = std::move(category),
            .damageType = std::move(damageType),
            .tags = std::move(tags),
            .iconKind = iconKind,
            .iconKey = std::move(iconKey),
            .position = drop.position,
            .quantity = drop.quantity,
            .rarity = rarity,
            .price = price,
            .attackPower = attackPower,
            .digPower = digPower,
            .lightRadius = lightRadius,
            .durability = durability,
            .weightKg = weightKg,
        });
    }

    if (snapshot.dungeon.active) {
        constexpr int MineTileScanRadius = 5;
        const float halfTile = static_cast<float>(balance::TileSize) * 0.5f;
        const int playerTileX = tileMap_.worldToTile(player_.position.x);
        const int playerTileY = tileMap_.worldToTile(player_.position.y);

        if (options.includePathGrid) {
            constexpr int PathGridScanRadius = 30;
            const std::vector<CollisionRect> objectBlockers = enemyTestActive_
                ? std::vector<CollisionRect>{}
                : solidObjectCollisionRects();
            snapshot.pathGrid.objectBlockers.reserve(objectBlockers.size());
            for (const CollisionRect& rect : objectBlockers) {
                snapshot.pathGrid.objectBlockers.push_back(GameTestCollisionRectSnapshot{
                    .pos = rect.pos,
                    .size = rect.size,
                });
            }
            const auto objectBlocksPlayer = [&](Vec2 center) {
                return circleIntersectsAnyRect(
                    center,
                    snapshot.player.radius,
                    std::span<const CollisionRect>{objectBlockers.data(), objectBlockers.size()});
            };
            const auto terrainBlocksPlayer = [&](Vec2 center) {
                const float sample = snapshot.player.radius * 0.55f;
                const Vec2 points[] = {
                    center,
                    center + Vec2{sample, 0.0f},
                    center + Vec2{-sample, 0.0f},
                    center + Vec2{0.0f, sample},
                    center + Vec2{0.0f, -sample},
                    center + Vec2{sample, sample},
                    center + Vec2{-sample, sample},
                    center + Vec2{sample, -sample},
                    center + Vec2{-sample, -sample},
                };
                for (Vec2 point : points) {
                    if (tileMap_.terrainDebugAtWorld(point).type != TileType::Empty) {
                        return true;
                    }
                }
                return false;
            };
            snapshot.pathGrid.minTileX = playerTileX - PathGridScanRadius;
            snapshot.pathGrid.minTileY = playerTileY - PathGridScanRadius;
            snapshot.pathGrid.width = PathGridScanRadius * 2 + 1;
            snapshot.pathGrid.height = PathGridScanRadius * 2 + 1;
            snapshot.pathGrid.tiles.reserve(static_cast<std::size_t>(snapshot.pathGrid.width * snapshot.pathGrid.height));
            for (int dy = -PathGridScanRadius; dy <= PathGridScanRadius; ++dy) {
                for (int dx = -PathGridScanRadius; dx <= PathGridScanRadius; ++dx) {
                    const int tx = playerTileX + dx;
                    const int ty = playerTileY + dy;
                    const Vec2 center = tileMap_.tileCenter(tx, ty);
                    const TerrainDebugInfo terrain = tileMap_.terrainDebugAtWorld(center);
                    const bool terrainSolid = terrain.type != TileType::Empty;
                    const bool terrainBlocked = terrainBlocksPlayer(center);
                    const bool objectBlocked = objectBlocksPlayer(center);
                    snapshot.pathGrid.tiles.push_back(GameTestPathTileSnapshot{
                        .center = center,
                        .tileX = tx,
                        .tileY = ty,
                        .hp = terrain.hp,
                        .effectiveHp = terrain.effectiveHp,
                        .terrainKind = gameTestTerrainKind(terrain.type),
                        .terrainAttribute = gameTestTerrainAttribute(terrain.attribute),
                        .localHardnessMultiplier = terrain.localHardnessMultiplier,
                        .distanceFromMainPath = terrain.distanceFromMainPath,
                        .solid = terrainSolid || terrainBlocked || objectBlocked,
                        .diggable = terrainSolid && !objectBlocked,
                    });
                }
            }
        }

        for (int dy = -MineTileScanRadius; dy <= MineTileScanRadius; ++dy) {
            for (int dx = -MineTileScanRadius; dx <= MineTileScanRadius; ++dx) {
                const int tx = playerTileX + dx;
                const int ty = playerTileY + dy;
                const Vec2 center = tileMap_.tileCenter(tx, ty);
                const TerrainDebugInfo terrain = tileMap_.terrainDebugAtWorld(center);
                if (terrain.type == TileType::Empty) {
                    continue;
                }
                Vec2 normal = normalize(player_.position - center);
                if (lengthSquared(normal) <= 0.0001f) {
                    normal = normalize(player_.facing);
                }
                snapshot.nearbyMineTiles.push_back(GameTestMineTileSnapshot{
                    .center = center,
                    .surfacePoint = center + normal * halfTile,
                    .outwardNormal = normal,
                    .tileX = tx,
                    .tileY = ty,
                    .hp = terrain.hp,
                    .effectiveHp = terrain.effectiveHp,
                    .terrainKind = gameTestTerrainKind(terrain.type),
                    .terrainAttribute = gameTestTerrainAttribute(terrain.attribute),
                    .localHardnessMultiplier = terrain.localHardnessMultiplier,
                    .distanceFromMainPath = terrain.distanceFromMainPath,
                    .solid = true,
                    .diggable = terrain.type != TileType::Empty,
                });
            }
        }
    }

    snapshot.inventory.backpackUsedSlots = backpackUsedSlots();
    snapshot.inventory.backpackCapacity = inventory_.screenSlotCount();
    snapshot.inventory.warehouseUsedSlots = warehouseUsedSlots();
    snapshot.inventory.warehouseCapacity = warehouseCapacity();
    const auto& backpackStacks = inventory_.objectStacks();
    const auto& backpackInstances = inventory_.objectInstances();
    snapshot.inventory.backpackItems.reserve(backpackStacks.size() + backpackInstances.size());
    for (int i = 0; i < static_cast<int>(backpackStacks.size()); ++i) {
        const InventoryObjectStack& stack = backpackStacks[static_cast<std::size_t>(i)];
        if (stack.count > 0) {
            snapshot.inventory.backpackItems.push_back(makeStackEntry(stack, GameTestInventoryLocation::Backpack, i));
        }
    }
    for (int i = 0; i < static_cast<int>(backpackInstances.size()); ++i) {
        snapshot.inventory.backpackItems.push_back(makeInstanceEntry(
            backpackInstances[static_cast<std::size_t>(i)],
            GameTestInventoryLocation::Backpack,
            i));
    }
    snapshot.inventory.warehouseItems.reserve(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
    for (int i = 0; i < static_cast<int>(warehouseObjectStacks_.size()); ++i) {
        const InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(i)];
        if (stack.count > 0) {
            snapshot.inventory.warehouseItems.push_back(makeStackEntry(stack, GameTestInventoryLocation::Warehouse, i));
        }
    }
    for (int i = 0; i < static_cast<int>(warehouseObjectInstances_.size()); ++i) {
        snapshot.inventory.warehouseItems.push_back(makeInstanceEntry(
            warehouseObjectInstances_[static_cast<std::size_t>(i)],
            GameTestInventoryLocation::Warehouse,
            i));
    }

    snapshot.base.active = mode_ == ScreenMode::Base;
    snapshot.base.money = money_;
    snapshot.base.materials.oldWoodBuildingMaterial = inventory_.materialCount(MaterialType::OldWoodBuildingMaterial);
    snapshot.base.materials.enhancementOre = inventory_.materialCount(MaterialType::EnhancementOre);
    snapshot.base.materials.moonFragment = inventory_.materialCount(MaterialType::MoonFragment);
    snapshot.base.materials.manaDrop = inventory_.materialCount(MaterialType::ManaDrop);
    snapshot.base.ringWorkshopUnlocked = ringWorkshopUnlocked_;
    snapshot.base.upgrades.reserve(BaseUpgradeItemCount);
    for (int index = 0; index < BaseUpgradeItemCount; ++index) {
        const MaterialType materialType = upgradeMaterialType(index);
        const int materialCost = upgradeMaterialCost(index);
        const int moneyCost = upgradeCost(index);
        GameTestUpgradeSnapshot upgrade;
        upgrade.index = index;
        upgrade.name = upgradeName(index);
        upgrade.materialName = std::string(materialTypeDisplayName(materialType));
        upgrade.level = upgradeLevel(index);
        upgrade.maxLevel = upgradeMaxLevel(index);
        upgrade.moneyCost = moneyCost;
        upgrade.materialCost = materialCost;
        upgrade.implemented = upgradeImplemented(index);
        upgrade.maxed = upgradeMaxed(index);
        upgrade.affordable =
            upgrade.implemented &&
            !upgrade.maxed &&
            moneyCost > 0 &&
            money_ >= moneyCost &&
            inventory_.materialCount(materialType) >= materialCost;
        snapshot.base.upgrades.push_back(std::move(upgrade));
    }

    snapshot.runStats = GameTestRunStats{
        .elapsedSeconds = runStats_.elapsedSeconds,
        .defeatedEnemies = runStats_.defeatedEnemies,
        .dugTiles = runStats_.dugTiles,
        .acquiredItems = runStats_.acquiredItems,
        .acquiredObjectItems = runStats_.acquiredObjectItems,
    };
    snapshot.money = money_;
    for (int materialIndex = 0; materialIndex < static_cast<int>(MaterialType::Count); ++materialIndex) {
        snapshot.totalMaterials += inventory_.materials().counts[static_cast<std::size_t>(materialIndex)];
    }

    return snapshot;
}

GameTestActionResult Game::applyTestAction(const GameTestAction& action)
{
    const auto result = [](bool applied, std::string message) {
        return GameTestActionResult{applied, std::move(message)};
    };
    const auto screenSlotForStack = [this](std::string_view objectId) {
        for (int slot = 0; slot < inventory_.screenSlotCount(); ++slot) {
            const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot);
            if (stack != nullptr && stack->objectId == objectId) {
                return slot;
            }
        }
        return -1;
    };
    const auto screenSlotForInstance = [this](std::string_view instanceId) {
        for (int slot = 0; slot < inventory_.screenSlotCount(); ++slot) {
            const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot);
            if (instance != nullptr && instance->instance.instanceId == instanceId) {
                return slot;
            }
        }
        return -1;
    };
    const auto backpackStackCount = [this](std::string_view objectId) {
        const auto& stacks = inventory_.objectStacks();
        const auto it = std::find_if(stacks.begin(), stacks.end(), [objectId](const InventoryObjectStack& stack) {
            return stack.objectId == objectId;
        });
        return it == stacks.end() ? 0 : it->count;
    };
    const auto backpackHasInstance = [this](std::string_view instanceId) {
        const auto& instances = inventory_.objectInstances();
        return std::any_of(instances.begin(), instances.end(), [instanceId](const InventoryObjectInstance& instance) {
            return instance.instance.instanceId == instanceId;
        });
    };
    const auto backpackInstanceEnhanceLevel = [this](std::string_view instanceId) {
        const auto& instances = inventory_.objectInstances();
        const auto it = std::find_if(instances.begin(), instances.end(), [instanceId](const InventoryObjectInstance& instance) {
            return instance.instance.instanceId == instanceId;
        });
        return it == instances.end() ? -1 : it->instance.enhanceLevel;
    };
    const auto warehouseStackCount = [this](std::string_view objectId) {
        const auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [objectId](const InventoryObjectStack& stack) {
            return stack.objectId == objectId;
        });
        return it == warehouseObjectStacks_.end() ? 0 : it->count;
    };
    const auto warehouseEntryForStack = [this](std::string_view objectId) -> std::optional<StorageEntry> {
        for (int i = 0; i < static_cast<int>(warehouseObjectStacks_.size()); ++i) {
            if (warehouseObjectStacks_[static_cast<std::size_t>(i)].objectId == objectId) {
                return StorageEntry{StorageEntryKind::Stack, i};
            }
        }
        return std::nullopt;
    };
    const auto warehouseEntryForInstance = [this](std::string_view instanceId) -> std::optional<StorageEntry> {
        for (int i = 0; i < static_cast<int>(warehouseObjectInstances_.size()); ++i) {
            if (warehouseObjectInstances_[static_cast<std::size_t>(i)].instance.instanceId == instanceId) {
                return StorageEntry{StorageEntryKind::Instance, i};
            }
        }
        return std::nullopt;
    };
    const auto warehouseHasInstance = [this](std::string_view instanceId) {
        return std::any_of(warehouseObjectInstances_.begin(), warehouseObjectInstances_.end(), [instanceId](const InventoryObjectInstance& instance) {
            return instance.instance.instanceId == instanceId;
        });
    };
    const auto warehouseSellTargetForEntry = [](StorageEntry entry) {
        MerchantSellTarget target;
        target.source = BaseItemSource::Warehouse;
        target.storageEntry = entry;
        target.warehouseEntry = true;
        target.valid = true;
        return target;
    };
    const auto processingEntryForStack = [this](std::string_view objectId) -> std::optional<StorageEntry> {
        const auto& stacks = inventory_.objectStacks();
        for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
            if (stacks[static_cast<std::size_t>(i)].objectId == objectId) {
                return StorageEntry{StorageEntryKind::Stack, i};
            }
        }
        return std::nullopt;
    };
    const auto processingEntryForInstance = [this](std::string_view instanceId) -> std::optional<StorageEntry> {
        const auto& instances = inventory_.objectInstances();
        for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
            if (instances[static_cast<std::size_t>(i)].instance.instanceId == instanceId) {
                return StorageEntry{StorageEntryKind::Instance, i};
            }
        }
        return std::nullopt;
    };
    const auto resolveUnlockedRingIndex = [this](int requestedRingIndex) -> std::optional<int> {
        const int ringCount = unlockedRingCount();
        const int ringIndex = requestedRingIndex >= 0
            ? requestedRingIndex
            : spellRing_.activeRingIndex();
        if (ringIndex < 0 || ringIndex >= ringCount) {
            return std::nullopt;
        }
        return ringIndex;
    };
    const auto ringSourceForIndex = [](int ringIndex) {
        switch (ringIndex) {
        case 0: return BaseItemSource::Ring0;
        case 1: return BaseItemSource::Ring1;
        default: return BaseItemSource::Ring2;
        }
    };
    const auto processingTargetForRingItem = [this, &ringSourceForIndex, &resolveUnlockedRingIndex](int ringIndex, int itemIndex) {
        ProcessingTarget target;
        const std::optional<int> resolvedRing = resolveUnlockedRingIndex(ringIndex);
        if (!resolvedRing) {
            return target;
        }
        ringIndex = *resolvedRing;
        target.source = ringSourceForIndex(ringIndex);
        target.ringIndex = ringIndex;
        target.ringItemIndex = itemIndex;
        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
        target.valid = itemIndex >= 0 && itemIndex < static_cast<int>(ringItems.size());
        return target;
    };
    const auto ringItemEnhanceLevel = [this, &resolveUnlockedRingIndex](int ringIndex, int itemIndex) {
        const std::optional<int> resolvedRing = resolveUnlockedRingIndex(ringIndex);
        if (!resolvedRing) {
            return -1;
        }
        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(*resolvedRing);
        if (itemIndex < 0 || itemIndex >= static_cast<int>(ringItems.size())) {
            return -1;
        }
        return ringItems[static_cast<std::size_t>(itemIndex)].enhanceLevel;
    };
    const auto ringItemDurability = [this, &resolveUnlockedRingIndex](int ringIndex, int itemIndex) {
        const std::optional<int> resolvedRing = resolveUnlockedRingIndex(ringIndex);
        if (!resolvedRing) {
            return -1;
        }
        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(*resolvedRing);
        if (itemIndex < 0 || itemIndex >= static_cast<int>(ringItems.size())) {
            return -1;
        }
        return ringItems[static_cast<std::size_t>(itemIndex)].durability;
    };
    const auto ringItemBroken = [this, &resolveUnlockedRingIndex](int ringIndex, int itemIndex) {
        const std::optional<int> resolvedRing = resolveUnlockedRingIndex(ringIndex);
        if (!resolvedRing) {
            return false;
        }
        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(*resolvedRing);
        if (itemIndex < 0 || itemIndex >= static_cast<int>(ringItems.size())) {
            return false;
        }
        return ringItems[static_cast<std::size_t>(itemIndex)].broken();
    };

    switch (action.kind) {
    case GameTestActionKind::None:
        return result(false, "no action");

    case GameTestActionKind::ReturnToBaseViaWarp:
    {
        if (mode_ != ScreenMode::Playing || worldBuildActive() || screenTransition_.active() || introTutorialActive()) {
            return result(false, "cannot return now");
        }
        const bool entranceNearby =
            distanceSquared(player_.position, dungeonEntrancePosition()) <=
            DungeonInspectableInteractionRange * DungeonInspectableInteractionRange;
        if (!entranceNearby && nearbyDiscoveredWarpPointIndex() < 0) {
            return result(false, "no nearby discovered warp");
        }
        if (currentStageIsRoguelike()) {
            enterAstralResult(AstralRunResult::Returned);
        } else {
            requestReturnToBaseTransition(false, false);
        }
        return result(true, "return requested");
    }

    case GameTestActionKind::ReturnToBaseAfterGameOver:
        if (mode_ != ScreenMode::GameOver || worldBuildActive() || screenTransition_.active()) {
            return result(false, "cannot return from game over now");
        }
        returnToBaseAfterGameOver();
        return result(mode_ != ScreenMode::GameOver || screenTransition_.active(), "game over return requested");

    case GameTestActionKind::StartMiningFromBase:
        if (mode_ != ScreenMode::Base || worldBuildActive() || screenTransition_.active()) {
            return result(false, "cannot start mining now");
        }
        requestMiningStartTransition(true, false);
        return result(screenTransition_.active(), "mining start requested");

    case GameTestActionKind::SyncEncyclopedia:
    {
        const std::size_t before = encyclopedia_.updateLog().size();
        syncEncyclopediaFromInventoryAndRing();
        const bool changed = encyclopedia_.updateLog().size() != before;
        baseStatus_ = changed ? "図鑑を同期しました" : baseStatus_;
        return result(true, changed ? "encyclopedia updated" : "encyclopedia already current");
    }

    case GameTestActionKind::UseBackpackStackItem:
    {
        if (mode_ != ScreenMode::Playing || worldBuildActive() || screenTransition_.active()) {
            return result(false, "cannot use item now");
        }
        std::string status;
        std::vector<EffectDiscoveryEvent> discoveries;
        const bool used = inventory_.useObjectStackById(
            action.objectId,
            player_,
            effectDispatcher_,
            &magic_,
            &discoveries,
            &encyclopedia_,
            &status);
        if (used && !discoveries.empty()) {
            applyEffectDiscoveries(discoveries);
        }
        return result(used, status.empty() ? (used ? "used stack item" : "item use failed") : status);
    }

    case GameTestActionKind::UseBackpackInstanceItem:
    {
        if (mode_ != ScreenMode::Playing || worldBuildActive() || screenTransition_.active()) {
            return result(false, "cannot use item now");
        }
        std::string status;
        std::vector<EffectDiscoveryEvent> discoveries;
        const bool used = inventory_.useObjectInstanceById(
            action.instanceId,
            player_,
            effectDispatcher_,
            &magic_,
            &discoveries,
            &encyclopedia_,
            &status);
        if (used && !discoveries.empty()) {
            applyEffectDiscoveries(discoveries);
        }
        return result(used, status.empty() ? (used ? "used instance item" : "item use failed") : status);
    }

    case GameTestActionKind::EquipBackpackStaff:
    {
        std::string status;
        const bool equipped = inventory_.equipStaffObject(action.objectId, action.instanceId, spellRing_, &status);
        if (equipped) {
            refreshEquipmentModifiers();
            applyPermanentUpgrades();
            refreshOrbitEffects();
            baseStatus_ = "杖を装備しました";
        }
        return result(equipped, status.empty() ? (equipped ? "staff equipped" : "staff equip failed") : status);
    }

    case GameTestActionKind::SwitchActiveRing:
    {
        const std::optional<int> targetRing = resolveUnlockedRingIndex(action.ringIndex);
        if (!targetRing) {
            return result(false, "ring locked");
        }
        switchActiveRingWithLog(*targetRing - spellRing_.activeRingIndex());
        return result(spellRing_.activeRingIndex() == *targetRing, "active ring switched");
    }

    case GameTestActionKind::EquipBackpackItemToRing:
    {
        const std::optional<int> resolvedRing = resolveUnlockedRingIndex(action.ringIndex);
        if (!resolvedRing) {
            return result(false, "ring locked");
        }
        const int targetRing = *resolvedRing;
        if (targetRing != spellRing_.activeRingIndex()) {
            switchActiveRingWithLog(targetRing - spellRing_.activeRingIndex());
        }
        SpellRingAddResult addResult{};
        std::string status;
        const bool equipped = inventory_.addObjectToRing(action.objectId, action.instanceId, spellRing_, &addResult, &status);
        if (equipped) {
            refreshOrbitEffects();
            baseStatus_ = "リングに装備しました";
        }
        return result(equipped, status.empty() ? (equipped ? "equipped to ring" : "ring equip failed") : status);
    }

    case GameTestActionKind::RemoveRingItemToBackpack:
    {
        const std::optional<int> resolvedRing = resolveUnlockedRingIndex(action.ringIndex);
        if (!resolvedRing) {
            return result(false, "ring locked");
        }
        const int targetRing = *resolvedRing;
        std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(targetRing);
        if (action.ringItemIndex < 0 || action.ringItemIndex >= static_cast<int>(ringItems.size())) {
            return result(false, "ring item not found");
        }

        const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(action.ringItemIndex)];
        if (ringItem.objectId.empty()) {
            return result(false, "ring item cannot be removed");
        }
        if (!inventory_.addObjectInstance(
                objectCatalog_,
                inventoryInstanceFromRingItem(inventory_, objectCatalog_, ringItem))) {
            return result(false, "backpack full");
        }

        ringItems.erase(ringItems.begin() + action.ringItemIndex);
        refreshOrbitEffects();
        baseStatus_ = "リングから外しました";
        return result(true, "removed ring item to backpack");
    }

    case GameTestActionKind::DiscardBackpackStack:
    {
        if (mode_ != ScreenMode::Playing || worldBuildActive() || screenTransition_.active()) {
            return result(false, "cannot discard item now");
        }

        const InventoryObjectStack* targetStack = nullptr;
        for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
            if (stack.objectId == action.objectId) {
                targetStack = &stack;
                break;
            }
        }
        if (targetStack == nullptr || targetStack->count <= 0) {
            return result(false, "stack not found");
        }
        if (isImportantItem(targetStack->item)) {
            return result(false, "important item cannot be discarded");
        }

        const ItemData item = targetStack->item;
        const int discardCount = action.count <= 0
            ? targetStack->count
            : std::min(action.count, targetStack->count);
        if (discardCount <= 0 || !inventory_.removeObjectItemCount(action.objectId, discardCount)) {
            return result(false, "discard failed");
        }

        std::vector<InventoryDiscardRequest> requests;
        requests.push_back(InventoryDiscardRequest{
            .item = item,
            .quantity = discardCount,
        });
        spawnInventoryDiscardRequests(std::move(requests));
        baseStatus_ = "リュックを空けました";
        return result(true, "discarded backpack stack");
    }

    case GameTestActionKind::DiscardBackpackInstance:
    {
        if (mode_ != ScreenMode::Playing || worldBuildActive() || screenTransition_.active()) {
            return result(false, "cannot discard item now");
        }

        const InventoryObjectInstance* targetInstance = nullptr;
        for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
            if (objectInstance.instance.instanceId == action.instanceId) {
                targetInstance = &objectInstance;
                break;
            }
        }
        if (targetInstance == nullptr) {
            return result(false, "instance not found");
        }
        if (inventory_.isStaffEquipped(action.instanceId)) {
            return result(false, "equipped staff cannot be discarded");
        }
        if (targetInstance->instance.protectionEnabled) {
            return result(false, "protected item cannot be discarded");
        }
        if (isImportantItem(targetInstance->item)) {
            return result(false, "important item cannot be discarded");
        }

        InventoryObjectInstance removedInstance;
        if (!inventory_.takeObjectInstance(action.instanceId, removedInstance)) {
            return result(false, "discard failed");
        }

        std::vector<InventoryDiscardRequest> requests;
        requests.push_back(InventoryDiscardRequest{
            .item = removedInstance.item,
            .instance = std::move(removedInstance.instance),
            .quantity = 1,
        });
        spawnInventoryDiscardRequests(std::move(requests));
        baseStatus_ = "リュックを空けました";
        return result(true, "discarded backpack instance");
    }

    case GameTestActionKind::DepositBackpackStack:
    {
        const int slot = screenSlotForStack(action.objectId);
        if (slot < 0) {
            return result(false, "stack not found");
        }
        const StorageTransferTarget target = storageDepositTargetForSourceSlot(0, slot);
        if (!storageTransferTargetAvailable(target)) {
            return result(false, "deposit unavailable");
        }
        const int beforeCount = backpackStackCount(action.objectId);
        const int moveCount = action.count <= 0 ? beforeCount : std::min(action.count, beforeCount);
        depositStorageTarget(target, moveCount);
        return result(backpackStackCount(action.objectId) < beforeCount, baseStatus_.empty() ? "deposited stack" : baseStatus_);
    }

    case GameTestActionKind::DepositBackpackInstance:
    {
        const int slot = screenSlotForInstance(action.instanceId);
        if (slot < 0) {
            return result(false, "instance not found");
        }
        const StorageTransferTarget target = storageDepositTargetForSourceSlot(0, slot);
        if (!storageTransferTargetAvailable(target)) {
            return result(false, "deposit unavailable");
        }
        depositStorageTarget(target, 1);
        return result(!backpackHasInstance(action.instanceId), baseStatus_.empty() ? "deposited instance" : baseStatus_);
    }

    case GameTestActionKind::SellBackpackStack:
    {
        const int slot = screenSlotForStack(action.objectId);
        if (slot < 0) {
            return result(false, "stack not found");
        }
        const MerchantSellTarget target = merchantSellTargetForSourceSlot(0, slot);
        if (!merchantSellTargetAvailable(target)) {
            return result(false, "sell unavailable");
        }
        const int beforeCount = backpackStackCount(action.objectId);
        const int sellCount = action.count <= 0 ? beforeCount : std::min(action.count, beforeCount);
        sellMerchantTarget(target, sellCount);
        return result(backpackStackCount(action.objectId) < beforeCount, baseStatus_.empty() ? "sold stack" : baseStatus_);
    }

    case GameTestActionKind::SellBackpackInstance:
    {
        const int slot = screenSlotForInstance(action.instanceId);
        if (slot < 0) {
            return result(false, "instance not found");
        }
        const MerchantSellTarget target = merchantSellTargetForSourceSlot(0, slot);
        if (!merchantSellTargetAvailable(target)) {
            return result(false, "sell unavailable");
        }
        sellMerchantTarget(target, 1);
        return result(!backpackHasInstance(action.instanceId), baseStatus_.empty() ? "sold instance" : baseStatus_);
    }

    case GameTestActionKind::SellWarehouseStack:
    {
        const std::optional<StorageEntry> entry = warehouseEntryForStack(action.objectId);
        if (!entry) {
            return result(false, "warehouse stack not found");
        }
        const MerchantSellTarget target = warehouseSellTargetForEntry(*entry);
        if (!merchantSellTargetAvailable(target)) {
            return result(false, "warehouse sell unavailable");
        }
        const int beforeCount = warehouseStackCount(action.objectId);
        const int sellCount = action.count <= 0 ? beforeCount : std::min(action.count, beforeCount);
        sellMerchantTarget(target, sellCount);
        return result(warehouseStackCount(action.objectId) < beforeCount, baseStatus_.empty() ? "sold warehouse stack" : baseStatus_);
    }

    case GameTestActionKind::SellWarehouseInstance:
    {
        const std::optional<StorageEntry> entry = warehouseEntryForInstance(action.instanceId);
        if (!entry) {
            return result(false, "warehouse instance not found");
        }
        const MerchantSellTarget target = warehouseSellTargetForEntry(*entry);
        if (!merchantSellTargetAvailable(target)) {
            return result(false, "warehouse sell unavailable");
        }
        sellMerchantTarget(target, 1);
        return result(!warehouseHasInstance(action.instanceId), baseStatus_.empty() ? "sold warehouse instance" : baseStatus_);
    }

    case GameTestActionKind::ProtectBackpackInstance:
        if (action.instanceId.empty()) {
            return result(false, "missing instance id");
        }
        return result(
            inventory_.setObjectInstanceProtection(action.instanceId, true),
            "protection enabled");

    case GameTestActionKind::UnprotectBackpackInstance:
        if (action.instanceId.empty()) {
            return result(false, "missing instance id");
        }
        return result(
            inventory_.setObjectInstanceProtection(action.instanceId, false),
            "protection disabled");

    case GameTestActionKind::UnprotectWarehouseInstance:
    {
        const std::optional<StorageEntry> entry = warehouseEntryForInstance(action.instanceId);
        if (!entry) {
            return result(false, "warehouse instance not found");
        }
        InventoryObjectInstance& objectInstance = warehouseObjectInstances_[static_cast<std::size_t>(entry->index)];
        objectInstance.instance.protectionEnabled = false;
        baseStatus_ = "保護OFF";
        return result(true, "warehouse protection disabled");
    }

    case GameTestActionKind::RepairBackpackInstance:
    {
        const std::optional<StorageEntry> entry = processingEntryForInstance(action.instanceId);
        if (!entry) {
            return result(false, "instance not found");
        }
        applyProcessingEntry(*entry, ProcessingMode::Repair, false);
        baseResultDialog_ = {};
        return result(baseStatus_.empty(), baseStatus_.empty() ? "repaired instance" : baseStatus_);
    }

    case GameTestActionKind::RepairRingItem:
    {
        if (mode_ != ScreenMode::Base) {
            return result(false, "not in base");
        }
        const ProcessingTarget target = processingTargetForRingItem(action.ringIndex, action.ringItemIndex);
        if (!target.valid) {
            return result(false, "ring item not found");
        }
        const int beforeDurability = ringItemDurability(target.ringIndex, target.ringItemIndex);
        const bool beforeBroken = ringItemBroken(target.ringIndex, target.ringItemIndex);
        applyProcessingTarget(target, ProcessingMode::Repair);
        baseResultDialog_ = {};
        const int afterDurability = ringItemDurability(target.ringIndex, target.ringItemIndex);
        const bool afterBroken = ringItemBroken(target.ringIndex, target.ringItemIndex);
        const bool changed = afterDurability > beforeDurability || (beforeBroken && !afterBroken);
        return result(changed, baseStatus_.empty() ? "repaired ring item" : baseStatus_);
    }

    case GameTestActionKind::EnhanceBackpackStackAttack:
    case GameTestActionKind::EnhanceBackpackStackDig:
    {
        const std::optional<StorageEntry> entry = processingEntryForStack(action.objectId);
        if (!entry) {
            return result(false, "stack not found");
        }
        const int beforeCount = backpackStackCount(action.objectId);
        const int beforeInstances = static_cast<int>(inventory_.objectInstances().size());
        const ProcessingMode mode = action.kind == GameTestActionKind::EnhanceBackpackStackAttack
            ? ProcessingMode::Attack
            : ProcessingMode::Dig;
        applyProcessingEntry(*entry, mode, false);
        baseResultDialog_ = {};
        const bool changed =
            backpackStackCount(action.objectId) < beforeCount ||
            static_cast<int>(inventory_.objectInstances().size()) > beforeInstances;
        return result(changed, baseStatus_.empty() ? "enhanced stack item" : baseStatus_);
    }

    case GameTestActionKind::EnhanceBackpackInstanceAttack:
    case GameTestActionKind::EnhanceBackpackInstanceDig:
    {
        const std::optional<StorageEntry> entry = processingEntryForInstance(action.instanceId);
        if (!entry) {
            return result(false, "instance not found");
        }
        const int beforeLevel = backpackInstanceEnhanceLevel(action.instanceId);
        const ProcessingMode mode = action.kind == GameTestActionKind::EnhanceBackpackInstanceAttack
            ? ProcessingMode::Attack
            : ProcessingMode::Dig;
        applyProcessingEntry(*entry, mode, false);
        baseResultDialog_ = {};
        return result(
            backpackInstanceEnhanceLevel(action.instanceId) > beforeLevel,
            baseStatus_.empty() ? "enhanced instance" : baseStatus_);
    }

    case GameTestActionKind::EnhanceRingItemAttack:
    case GameTestActionKind::EnhanceRingItemDig:
    {
        if (mode_ != ScreenMode::Base) {
            return result(false, "not in base");
        }
        const ProcessingTarget target = processingTargetForRingItem(action.ringIndex, action.ringItemIndex);
        if (!target.valid) {
            return result(false, "ring item not found");
        }
        const int beforeLevel = ringItemEnhanceLevel(target.ringIndex, target.ringItemIndex);
        const ProcessingMode mode = action.kind == GameTestActionKind::EnhanceRingItemAttack
            ? ProcessingMode::Attack
            : ProcessingMode::Dig;
        applyProcessingTarget(target, mode);
        baseResultDialog_ = {};
        return result(
            ringItemEnhanceLevel(target.ringIndex, target.ringItemIndex) > beforeLevel,
            baseStatus_.empty() ? "enhanced ring item" : baseStatus_);
    }

    case GameTestActionKind::BuyBaseUpgrade:
    {
        if (mode_ != ScreenMode::Base || action.upgradeIndex < 0) {
            return result(false, "upgrade unavailable");
        }
        const int beforeLevel = upgradeLevel(action.upgradeIndex);
        buyUpgrade(action.upgradeIndex);
        baseResultDialog_ = {};
        return result(upgradeLevel(action.upgradeIndex) > beforeLevel, baseStatus_.empty() ? "bought upgrade" : baseStatus_);
    }

    case GameTestActionKind::ChooseLevelUpUpgrade:
    {
        if (mode_ != ScreenMode::LevelUp || !levels_.isChoosing() || levelUpPresentation_.active || levelUpResultDialog_.open) {
            return result(false, "level up unavailable");
        }
        const std::optional<int> ringIndex = resolveUnlockedRingIndex(action.ringIndex);
        if (!ringIndex) {
            return result(false, "ring locked");
        }
        const int option = std::clamp(action.upgradeIndex, 0, 2);
        const bool applied = applyLevelUpSelection(RingLevelUpgradeSelection{
            *ringIndex,
            gameTestLevelUpUpgradeKind(option),
        });
        return result(applied, applied ? "level up upgrade chosen" : "level up unavailable");
    }
    }

    return result(false, "unknown action");
}

bool Game::dumpDungeonDebugState()
{
    std::error_code error;
    std::filesystem::create_directories(".local", error);
    if (error) {
        logWarning("Debug: failed to create dungeon dump directory: " + error.message());
        return false;
    }

    const std::filesystem::path path = dungeonDebugDumpPath();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        logWarning("Debug: failed to open dungeon dump: " + path.string());
        return false;
    }

    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    file.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    file << std::fixed << std::setprecision(2);

    const auto writeVec = [&](std::string_view label, Vec2 value) {
        file << label << " world=(" << value.x << "," << value.y << ")"
            << " tile=(" << tileMap_.worldToTile(value.x) << "," << tileMap_.worldToTile(value.y) << ")\n";
    };
    const auto writeTile = [&](std::string_view label, DungeonTile tile) {
        file << label << " tile=(" << tile.x << "," << tile.y << ")"
            << " world=(" << tileWorldCenter(tile).x << "," << tileWorldCenter(tile).y << ")\n";
    };

    file << "MajoShovel Dungeon Debug Dump\n";
    file << "stageId=" << currentStageId_ << "\n";
    file << "stageName=" << currentStageDefinition_.name << "\n";
    file << "screenMode=" << static_cast<int>(mode_) << "\n";
    file << "seed=" << dungeonLayout_.seed << "\n";
    file << "runSeconds=" << runStats_.elapsedSeconds << "\n";
    file << "warpPointsEnabled=" << (warpPointsEnabled_ ? "true" : "false") << "\n";
    file << "worldBuildActive=" << (worldBuildActive() ? "true" : "false") << "\n";
    file << "enemyTestActive=" << (enemyTestActive_ ? "true" : "false") << "\n";
    file << "\n";

    writeVec("player", player_.position);
    writeVec("camera", camera_.position());
    writeTile("start", dungeonLayout_.startTile);
    writeTile("goal", dungeonLayout_.goalTile);
    file << "\n";

    file << "boss has=" << (hasBossSpawnPoint_ ? "true" : "false")
        << " spawned=" << (bossSpawned_ ? "true" : "false")
        << " capturedForStage=" << (hasCapturedBossForCurrentStage() ? "true" : "false") << "\n";
    if (hasBossSpawnPoint_) {
        writeVec("bossSpawnPoint", bossSpawnPoint_);
        const DungeonLayoutMetrics bossMetrics = calculateDungeonLayoutMetrics(
            dungeonLayout_,
            {static_cast<float>(tileMap_.worldToTile(bossSpawnPoint_.x)), static_cast<float>(tileMap_.worldToTile(bossSpawnPoint_.y))});
        file << "bossPath progress=" << bossMetrics.pathProgress
            << " distFromMainPath=" << bossMetrics.distanceFromMainPath
            << " distFromStart=" << bossMetrics.distanceFromStart << "\n";
        file << "distancePlayerToBoss=" << length(player_.position - bossSpawnPoint_) << "\n";
    }
    file << "\n";

    file << "warpPoints count=" << warpPoints_.size()
        << " discovered=" << discoveredWarpPointCount()
        << " unlocked=" << unlockedWarpPointCount_ << "\n";
    for (const WarpPoint& point : warpPoints_) {
        const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, {
            static_cast<float>(point.tilePosition.x),
            static_cast<float>(point.tilePosition.y),
        });
        file << "warp index=" << point.index
            << " discovered=" << (point.discovered ? "true" : "false")
            << " unlocked=" << (point.unlocked ? "true" : "false")
            << " snapshot=" << (point.snapshotCaptured ? "true" : "false")
            << " pathProgress=" << metrics.pathProgress
            << " tile=(" << point.tilePosition.x << "," << point.tilePosition.y << ")"
            << " world=(" << point.position.x << "," << point.position.y << ")\n";
    }
    file << "\n";

    file << "mainPath count=" << dungeonLayout_.mainPathPoints.size()
        << " lengthTiles=" << debugPathLengthTiles(dungeonLayout_.mainPathPoints) << "\n";
    for (std::size_t i = 0; i < dungeonLayout_.mainPathPoints.size(); ++i) {
        const Vec2 point = dungeonLayout_.mainPathPoints[i];
        file << "mainPath[" << i << "] tile=(" << point.x << "," << point.y << ")"
            << " world=(" << tileWorldCenter(roundDungeonTile(point)).x << ","
            << tileWorldCenter(roundDungeonTile(point)).y << ")\n";
    }
    file << "\n";

    file << "events count=" << dungeonEvents_.size() << "\n";
    for (const DungeonEventInstance& event : dungeonEvents_.all()) {
        file << "event id=" << event.id
            << " kind=" << dungeonEventKindId(event.kind)
            << " discovered=" << (event.discovered ? "true" : "false")
            << " completed=" << (event.completed ? "true" : "false")
            << " encounterSpawned=" << (event.encounterSpawned ? "true" : "false")
            << " activated=" << (event.activated ? "true" : "false")
            << " center=(" << event.centerTile.x << "," << event.centerTile.y << ")"
            << " focus=(" << event.focusTile.x << "," << event.focusTile.y << ")"
            << " reward=(" << event.rewardTile.x << "," << event.rewardTile.y << ")"
            << " objects=" << event.eventObjects.size()
            << " nestHoles=" << event.nestHoles.size()
            << " enemies=" << event.spawnedEnemyRuntimeIds.size() << "\n";
        for (const DungeonEventNestHole& hole : event.nestHoles) {
            file << "  nest tile=(" << hole.tile.x << "," << hole.tile.y << ")"
                << " hp=" << hole.hp << "/" << hole.maxHp
                << " destroyed=" << (hole.destroyed ? "true" : "false")
                << " rewardSpawned=" << (hole.rewardSpawned ? "true" : "false") << "\n";
        }
        for (const DungeonEventObject& object : event.eventObjects) {
            file << "  object kind=" << static_cast<int>(object.kind)
                << " tile=(" << object.tile.x << "," << object.tile.y << ")"
                << " hp=" << object.hp << "/" << object.maxHp
                << " destroyed=" << (object.destroyed ? "true" : "false")
                << " powered=" << (object.powered ? "true" : "false") << "\n";
        }
    }
    file << "\n";

    file << "nodes reward=" << rewardNodeCount()
        << " money=" << moneyNodeCount()
        << " chest=" << chestNodes_.size()
        << " crate=" << crateNodes_.size()
        << " exposedEnemies=" << exposedEnemyNodeCount()
        << " buriedEnemies=" << buriedEnemyNodeCount()
        << " spawnedEnemyNodes=" << spawnedEnemyNodeCount()
        << " worldDrops=" << worldDrops_.size() << "\n";

    file.flush();
    if (!file) {
        logWarning("Debug: failed while writing dungeon dump: " + path.string());
        return false;
    }

    logInfo("Debug: dungeon dump written: " + path.string());
    return true;
}

bool Game::executeDebugCommand(std::string_view command)
{
    const std::string normalized = lowerAscii(trimAscii(std::string(command)));

    if (handleBaseEditCommand(normalized)) {
        return true;
    }
    if (handleObjectImageScaleCommand(normalized)) {
        return true;
    }
    if (handleEnemyHitboxEditCommand(normalized)) {
        return true;
    }
    if (handleEnemyShadowEditCommand(normalized)) {
        return true;
    }
    if (handleAudioCueEditCommand(normalized)) {
        return true;
    }
    if (handleDebugNamedSaveCommand(normalized)) {
        return true;
    }
    if (handleDebugItemPickerCommand(normalized)) {
        return true;
    }
    if (handleDebugStoryTestCommand(normalized)) {
        return true;
    }
    if (normalized == "game dungeon-dump" ||
        normalized == "game dungeon dump") {
        dumpDungeonDebugState();
        return true;
    }
    constexpr std::string_view DungeonEventPlacePrefix = "game dungeon-event place ";
    constexpr std::string_view DungeonEventPlaceAltPrefix = "game dungeon event place ";
    if (normalized.rfind(DungeonEventPlacePrefix, 0) == 0 ||
        normalized.rfind(DungeonEventPlaceAltPrefix, 0) == 0) {
        const std::size_t prefixLength = normalized.rfind(DungeonEventPlacePrefix, 0) == 0
            ? DungeonEventPlacePrefix.size()
            : DungeonEventPlaceAltPrefix.size();
        const std::string kindId = trimAscii(normalized.substr(prefixLength));
        DungeonEventKind kind = DungeonEventKind::SleepingEnemyTreasure;
        if (kindId.empty() || !dungeonEventKindFromId(kindId, kind)) {
            logWarning("Debug: unknown dungeon event kind '" + kindId + "'.");
            return true;
        }
        debugRequestDungeonEventPlacement(kind);
        return true;
    }
    if (normalized == "game dungeon-focus test" ||
        normalized == "game dungeon focus test" ||
        normalized == "game focus test") {
        DungeonFocusRequest request;
        request.eventKind = "debug";
        request.focusWorldPos = player_.position + Vec2{240.0f, 0.0f};
        request.holdSecondsIfNoDialogue = 2.0f;
        const bool requested = requestDungeonFocus(std::move(request));
        if (requested) {
            logInfo("Debug: dungeon focus test requested.");
        }
        return true;
    }

    const auto applyStageUnlockDebugCommand = [&](int unlockedStoryStages, std::string_view label) {
        if (effectTestActive_) {
            exitEffectTestToBase();
        } else if (projectileTestActive_) {
            exitProjectileTestToBase();
        } else if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (!basePresentationActive() && mode_ != ScreenMode::OpeningKamishibai && mode_ != ScreenMode::EndingKamishibai && mode_ != ScreenMode::Title) {
            returnToBaseFromNormalStage(false, false);
        }
        applyDebugStageUnlockState(unlockedStoryStages);
        logInfo("Debug: stage unlock state set to " + std::string(label) + ".");
        std::string saveMessage;
        if (saveSaveData(saveMessage)) {
            logInfo("Debug: stage unlock state saved.");
        } else {
            logWarning("Debug: stage unlock state changed in memory, but save failed: " + saveMessage);
        }
        return true;
    };

    const auto eraseStoryFlag = [&](std::string_view flag) {
        storyFlags_.erase(
            std::remove(storyFlags_.begin(), storyFlags_.end(), std::string(flag)),
            storyFlags_.end());
    };

    const auto clearRuntimeDebugPresentation = [&]() {
        pendingStoryTriggers_.clear();
        dialogue_.clear();
        endingKamishibaiPending_ = false;
        resetBossEncounter();
        resetDungeonFocus();
        screenTransition_ = ScreenTransitionState{};
        worldBuildJob_ = WorldBuildJob{};
        inventory_.setOpen(false);
        inventory_.cancelGrab();
        cancelRingGrab();
        closeDebugItemPicker();
        closeDebugStoryTest();
        closeProjectileTestMode();
        closeEffectTestMode();
        if (levels_.isChoosing()) {
            levels_ = LevelSystem{};
        }
        levelUpPresentation_ = {};
        levelUpResultDialog_ = {};
        baseRegenerateConfirm_ = {};
        baseBrokenRingDepartureConfirm_ = {};
        baseWarpPointSelectActive_ = false;
        warpReturnConfirm_ = {};
    };

    const auto storyUnlockCountForStageId = [&](std::string_view stageId) {
        if (stageId == "stage_04_astral_mine") {
            return 2;
        }

        int storyStageNumber = 0;
        for (const StageDefinition& stage : stageCatalog_.getStagesSortedByDisplayOrder()) {
            if (debugIsRoguelikeStageDefinition(stage)) {
                continue;
            }
            ++storyStageNumber;
            if (stage.id == stageId) {
                return std::max(1, storyStageNumber);
            }
        }
        return 1;
    };

    const auto selectDebugStage = [&](std::string_view stageId) {
        if (stageId.empty() || stageCatalog_.getStageById(stageId) == nullptr) {
            logWarning("Debug: stage not found: " + std::string(stageId));
            return false;
        }
        if (effectTestActive_) {
            exitEffectTestToBase();
        } else if (projectileTestActive_) {
            exitProjectileTestToBase();
        } else if (enemyTestActive_) {
            exitEnemyTestToBase();
        }

        unlockedStages_ = std::max(unlockedStages_, storyUnlockCountForStageId(stageId));
        currentStageId_ = std::string(stageId);
        currentStage_ = stageCatalogIndexForId(currentStageId_);
        resolveCurrentStageDefinition();
        syncWarpStateForCurrentStage();
        baseMiningStartSelection_ = unlockedWarpPointCount_ > 0 ? 1 : 0;
        baseWarpPointSelectActive_ = false;
        baseWarpPointSelection_ = 0;
        baseRegenerateConfirm_ = {};
        baseBrokenRingDepartureConfirm_ = {};
        return true;
    };

    const auto markStoryTriggerSeenForCurrentStage = [&](std::string_view triggerName) {
        const std::string trigger = currentStageStoryTrigger(triggerName);
        if (trigger.empty()) {
            return;
        }
        for (const StoryEvent& event : storyEvents_) {
            if (event.trigger == trigger && !event.onceFlag.empty()) {
                addStoryFlag(event.onceFlag);
            }
        }
    };

    const auto eraseStoryTriggerSeenForCurrentStage = [&](std::string_view triggerName) {
        const std::string trigger = currentStageStoryTrigger(triggerName);
        if (trigger.empty()) {
            return;
        }
        for (const StoryEvent& event : storyEvents_) {
            if (event.trigger == trigger && !event.onceFlag.empty()) {
                eraseStoryFlag(event.onceFlag);
            }
        }
    };

    const auto markCurrentStageClearedForDebug = [&]() {
        const std::string clearFlag = stageClearFlagForStage(currentStageId_);
        if (!clearFlag.empty()) {
            addStoryFlag(clearFlag);
        }
        unlockedStages_ = std::max(unlockedStages_, storyUnlockCountForStageId(currentStageId_) + 1);
        setUnlockedRingCount(std::max(unlockedRingCount(), storyUnlockCountForStageId(currentStageId_) + 1));
        markStoryTriggerSeenForCurrentStage("boss_before");
        markStoryTriggerSeenForCurrentStage("boss_after");
        markStoryTriggerSeenForCurrentStage("stage_clear");
        syncWarpStateForCurrentStage();
    };

    const auto enterDebugBase = [&]() {
        if (effectTestActive_) {
            exitEffectTestToBase();
        } else if (projectileTestActive_) {
            exitProjectileTestToBase();
        } else if (enemyTestActive_) {
            exitEnemyTestToBase();
        }
        enterBase();
        baseMiningStartChoiceActive_ = false;
        baseWarpPointSelectActive_ = false;
        baseRegenerateConfirm_ = {};
        baseBrokenRingDepartureConfirm_ = {};
        baseStatus_.clear();
    };

    const auto placePlayerAtBossApproach = [&]() {
        if (!hasBossSpawnPoint_) {
            return;
        }
        Vec2 direction = normalize(bossSpawnPoint_);
        if (lengthSquared(direction) <= 0.0001f) {
            direction = {1.0f, 0.0f};
        }
        player_.position = bossSpawnPoint_ - direction * (BossSpawnTriggerRadius + 18.0f);
        player_.facing = direction;
        player_.updateSpriteFlipFromFacing();
        tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        updateDungeonMinimap(0.0);
        camera_.follow(player_.position, 1.0f);
    };

    const auto buildDebugDungeonWithAllWarps = [&](bool markCleared, bool suppressBossBeforeStory) {
        if (suppressBossBeforeStory) {
            markStoryTriggerSeenForCurrentStage("boss_before");
        }
        clearRuntimeDebugPresentation();
        initializeWorld(false);
        if (!unlockAllWarpPointsForCurrentDungeon()) {
            logWarning("Debug: failed to unlock warp points for " + currentStageId_ + ".");
            return false;
        }
        if (markCleared) {
            markCurrentStageClearedForDebug();
        }
        return true;
    };

    const auto removeCapturedBossForCurrentStage = [&]() {
        const std::string objectId = currentStageBossCaptureObjectId();
        if (objectId.empty()) {
            return 0;
        }

        int removedCount = 0;
        while (inventory_.removeObjectItemCount(objectId, 1)) {
            ++removedCount;
        }

        std::vector<std::string> instanceIds;
        for (const InventoryObjectInstance& instance : inventory_.objectInstances()) {
            if (debugObjectInstanceMatchesObjectId(instance, objectId)) {
                instanceIds.push_back(instance.instance.instanceId);
            }
        }
        for (const std::string& instanceId : instanceIds) {
            if (inventory_.removeObjectInstance(instanceId)) {
                ++removedCount;
            }
        }

        for (int i = static_cast<int>(warehouseObjectStacks_.size()) - 1; i >= 0; --i) {
            InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(i)];
            if (stack.objectId != objectId) {
                continue;
            }
            removedCount += std::max(1, stack.count);
            removeWarehouseDisplaySlotAtEntryIndex(i);
            warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + i);
        }

        for (int i = static_cast<int>(warehouseObjectInstances_.size()) - 1; i >= 0; --i) {
            const InventoryObjectInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(i)];
            if (!debugObjectInstanceMatchesObjectId(instance, objectId)) {
                continue;
            }
            removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + i);
            warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + i);
            ++removedCount;
        }

        for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
            std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
            const auto before = ringItems.size();
            ringItems.erase(
                std::remove_if(ringItems.begin(), ringItems.end(), [&](const SpellRingItem& item) {
                    return item.objectId == objectId;
                }),
                ringItems.end());
            removedCount += static_cast<int>(before - ringItems.size());
        }

        if (removedCount > 0) {
            warehouseDisplaySlots_.clear();
            refreshOrbitEffects();
        }
        return removedCount;
    };

    const auto resetBossFlowStoryForCurrentStage = [&]() {
        eraseStoryTriggerSeenForCurrentStage("boss_before");
        eraseStoryTriggerSeenForCurrentStage("boss_after");
        eraseStoryTriggerSeenForCurrentStage("stage_clear");
        eraseStoryFlag(stageClearFlagForStage(currentStageId_));
        if (currentStageId_ == DebugFinalStoryStageId) {
            eraseStoryFlag(DebugEndingSeenFlag);
            eraseStoryFlag(DebugEndingMainFlag);
            eraseStoryFlag(DebugStage03ClearFlag);
            eraseStoryFlag(DebugPostEndingIntroFlag);
        }
    };

    const auto parseDebugInt = [](std::string_view text, int fallback) {
        const std::string trimmed = trimAscii(std::string(text));
        if (trimmed.empty()) {
            return fallback;
        }
        try {
            return std::stoi(trimmed);
        } catch (...) {
            return fallback;
        }
    };

    const auto setPlayerHpForDebug = [&](int value) {
        applyPermanentUpgrades();
        player_.hp = std::clamp(value, 1, std::max(1, player_.maxHp));
        player_.status = EntityStatus{};
        player_.poisonDamageAccumulator = 0.0;
        player_.hotDamageAccumulator = 0.0;
        player_.bleedDamageAccumulator = 0.0;
        logInfo("Debug: player HP set to " + std::to_string(player_.hp) + "/" + std::to_string(player_.maxHp) + ".");
    };

    const auto setPlayerLevelForDebug = [&](int value) {
        player_.level = std::clamp(value, 1, PlayerMaxLevel);
        player_.xp = 0;
        applyPermanentUpgrades();
        logInfo("Debug: player level set to Lv " + std::to_string(player_.level) + ".");
    };

    const auto addRandomDebugItems = [&](int count) {
        const std::vector<ItemData>& objects = objectCatalog_.registry.items();
        std::vector<std::string_view> candidateIds;
        candidateIds.reserve(objects.size());
        for (const ItemData& object : objects) {
            if (!object.id.empty()) {
                candidateIds.push_back(object.id);
            }
        }

        if (candidateIds.empty()) {
            logWarning("Debug: no object entries available; random item add skipped.");
            return;
        }

        std::mt19937& rng = lootRuntimeRng();
        std::uniform_int_distribution<std::size_t> pick(0, candidateIds.size() - 1);
        int acquiredCount = 0;
        int skippedCount = 0;
        const int itemCount = std::clamp(count, 1, 99);
        for (int i = 0; i < itemCount; ++i) {
            const std::string_view objectId = candidateIds[pick(rng)];
            if (inventory_.addObjectItem(objectCatalog_, objectId)) {
                ++acquiredCount;
            } else {
                ++skippedCount;
            }
        }

        logInfo("Debug: random object items added " + std::to_string(acquiredCount) +
            " / skipped " + std::to_string(skippedCount) + ".");
    };

    const auto distortionLabel = [](AstralDistortionKind distortion) {
        switch (distortion) {
        case AstralDistortionKind::FadingStarlight:
            return "星明かりが遠のく";
        case AstralDistortionKind::StarHardened:
            return "星硬化";
        case AstralDistortionKind::EchoSpawn:
            return "残響湧き";
        case AstralDistortionKind::None:
            break;
        }
        return "なし";
    };

    const auto resultForDebugToken = [](std::string_view token) {
        if (token == "died" || token == "death") {
            return AstralRunResult::Died;
        }
        if (token == "dragon-defeated" || token == "dragon") {
            return AstralRunResult::DragonDefeated;
        }
        if (token == "completed" || token == "complete" || token == "10000m") {
            return AstralRunResult::Completed;
        }
        return AstralRunResult::Returned;
    };

    const auto resultLabel = [](AstralRunResult result) {
        switch (result) {
        case AstralRunResult::Returned:
            return "帰還成功";
        case AstralRunResult::Died:
            return "死亡";
        case AstralRunResult::DragonDefeated:
            return "星脈竜撃破";
        case AstralRunResult::Completed:
            return "10000m到達";
        case AstralRunResult::None:
            break;
        }
        return "なし";
    };

    const auto astralDungeonReady = [&]() {
        if (worldBuildJob_.active) {
            logWarning("Debug: astral dungeon is still loading.");
            return false;
        }
        if (!currentStageIsRoguelike() || mode_ != ScreenMode::Playing) {
            logWarning("Debug: astral command requires an active 星間廃坑 run.");
            return false;
        }
        return true;
    };

    const auto forceAstralRankAndDistortion = [&](int rank) {
        if (!astralRunActive()) {
            return;
        }
        const int maxDepth = std::max(1, astralRun_.maxDepth);
        const int clampedRank = std::clamp(rank, 1, maxDepth);
        astralRun_.currentDepth = clampedRank;
        astralRun_.maxReachedDepth = std::max(astralRun_.maxReachedDepth, clampedRank);
        astralRun_.distortion = chooseAstralDistortionForDepth(clampedRank, AstralDistortionKind::None);
        applyAstralDistortionToLayout();
    };

    const auto clearDebugWarpTile = [&](Vec2 position) {
        tileMap_.setTileOverride(
            DungeonTile{
                tileMap_.worldToTile(position.x),
                tileMap_.worldToTile(position.y),
            },
            TileType::Empty);
    };

    const auto placePlayerForAstralDebug = [&](Vec2 position, int forcedRank) {
        if (forcedRank > 0) {
            forceAstralRankAndDistortion(forcedRank);
        }
        clearDebugWarpTile(position);
        player_.position = safePlayerStartPosition(position);
        clearDebugWarpTile(player_.position);
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        updateDungeonMinimap(0.0);
        camera_.follow(player_.position, 1.0f);
        updateAstralRunProgress();
    };

    const auto mainPathPositionForAreaProgress = [&](float progress) {
        if (dungeonLayout_.mainPathPoints.empty()) {
            return tileWorldCenter(dungeonLayout_.startTile);
        }
        const int index = std::clamp(
            static_cast<int>(std::round(std::clamp(progress, 0.0f, 1.0f) * static_cast<float>(dungeonLayout_.mainPathPoints.size() - 1))),
            0,
            static_cast<int>(dungeonLayout_.mainPathPoints.size() - 1));
        return tileWorldCenter(roundDungeonTile(dungeonLayout_.mainPathPoints[static_cast<std::size_t>(index)]));
    };

    const auto mainPathPositionForDepthMeters = [&](int depthMeters) {
        const int areaStart = std::max(0, astralRun_.currentDepthMeters);
        const int areaEnd = std::max(areaStart + 1, astralRun_.nextHoleDepthMeters);
        const int clampedMeters = std::clamp(depthMeters, areaStart, areaEnd);
        const float progress = static_cast<float>(clampedMeters - areaStart) / static_cast<float>(areaEnd - areaStart);
        return mainPathPositionForAreaProgress(progress);
    };

    const auto ensureAstralAreaForDepthMeters = [&](int depthMeters) {
        if (!astralRunActive()) {
            return false;
        }
        const int areaStart = std::max(0, astralRun_.currentDepthMeters);
        const int areaEnd = std::max(areaStart + 1, astralRun_.nextHoleDepthMeters);
        if (depthMeters < areaStart || depthMeters > areaEnd) {
            return debugSetRoguelikeAreaForDepthMeters(depthMeters);
        }
        return true;
    };

    const auto applyDebugAstralDistortion = [&]() {
        if (!astralRunActive()) {
            logInfo("Debug: astral distortion mode set to " + debugAstralDistortionMode_ + " for the next active run.");
            return;
        }
        astralRun_.distortion = chooseAstralDistortionForDepth(astralRun_.currentDepth, AstralDistortionKind::None);
        applyAstralDistortionToLayout();
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        logInfo("Debug: astral distortion => " + std::string(distortionLabel(astralRun_.distortion)) + ".");
    };

    constexpr std::string_view AstralDepthPrefix = "game astral depth ";
    if (normalized.rfind(AstralDepthPrefix, 0) == 0) {
        debugAstralDepthMeters_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(AstralDepthPrefix.size()), debugAstralDepthMeters_),
            0,
            std::max(1, astralRun_.completionDepthMeters));
        debugAstralDepthRank_ = roguelikeSectionRankForDepthMeters(debugAstralDepthMeters_);
        logInfo("Debug: astral depth meters target => " + std::to_string(debugAstralDepthMeters_) +
            "m / rank " + std::to_string(debugAstralDepthRank_) + ".");
        return true;
    }

    constexpr std::string_view AstralRankPrefix = "game astral rank ";
    if (normalized.rfind(AstralRankPrefix, 0) == 0) {
        const int maxRank = std::max(
            std::max(1, astralRun_.maxDepth),
            roguelikeSectionRankForDepthMeters(std::max(1, astralRun_.completionDepthMeters)));
        debugAstralDepthRank_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(AstralRankPrefix.size()), debugAstralDepthRank_),
            1,
            maxRank);
        debugAstralDepthMeters_ = roguelikeDepthMetersForSectionRank(debugAstralDepthRank_);
        logInfo("Debug: astral depth rank target => " + std::to_string(debugAstralDepthRank_) +
            " / " + std::to_string(debugAstralDepthMeters_) + "m.");
        return true;
    }

    constexpr std::string_view AstralMoveTargetPrefix = "game astral move-target ";
    if (normalized.rfind(AstralMoveTargetPrefix, 0) == 0) {
        std::string token = trimAscii(normalized.substr(AstralMoveTargetPrefix.size()));
        if (token == "depth") {
            token = "meters";
        } else if (token == "return") {
            token = "hole";
        }
        if (token == "entrance" || token == "meters" || token == "rank" || token == "boss" || token == "hole") {
            debugAstralMoveTarget_ = token;
            logInfo("Debug: astral move target => " + token + ".");
        } else {
            logWarning("Debug: unknown astral move target: " + token);
        }
        return true;
    }

    constexpr std::string_view AstralDistortionPrefix = "game astral distortion ";
    if (normalized.rfind(AstralDistortionPrefix, 0) == 0) {
        const std::string token = trimAscii(normalized.substr(AstralDistortionPrefix.size()));
        if (token == "auto" || token == "none" || token == "fading-starlight" ||
            token == "star-hardened" || token == "echo-spawn") {
            debugAstralDistortionMode_ = token;
            applyDebugAstralDistortion();
        } else {
            logWarning("Debug: unknown astral distortion mode: " + token);
        }
        return true;
    }

    constexpr std::string_view AstralRoomPrefix = "game astral room ";
    if (normalized.rfind(AstralRoomPrefix, 0) == 0) {
        const std::string token = trimAscii(normalized.substr(AstralRoomPrefix.size()));
        const DebugAstralRoomTarget target = debugAstralRoomTargetForToken(token);
        if (target.kind == DebugAstralRoomTargetKind::None) {
            logWarning("Debug: unknown astral room target: " + token);
        } else {
            debugAstralRoomType_ = token;
            logInfo("Debug: astral room target => " + std::string(debugAstralRoomTargetLabel(target)) + ".");
        }
        return true;
    }

    constexpr std::string_view AstralRoomIndexPrefix = "game astral room-index ";
    if (normalized.rfind(AstralRoomIndexPrefix, 0) == 0) {
        debugAstralRoomIndex_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(AstralRoomIndexPrefix.size()), debugAstralRoomIndex_),
            1,
            20);
        logInfo("Debug: astral room index => " + std::to_string(debugAstralRoomIndex_) + ".");
        return true;
    }

    constexpr std::string_view AstralResultPrefix = "game astral result ";
    if (normalized.rfind(AstralResultPrefix, 0) == 0) {
        const std::string token = trimAscii(normalized.substr(AstralResultPrefix.size()));
        if (token == "returned" || token == "died" || token == "dragon-defeated" || token == "completed") {
            debugAstralResultKind_ = token;
            logInfo("Debug: astral result target => " + token + ".");
        } else {
            logWarning("Debug: unknown astral result: " + token);
        }
        return true;
    }

    constexpr std::string_view AstralStatOverridePrefix = "game astral stat-override ";
    if (normalized.rfind(AstralStatOverridePrefix, 0) == 0) {
        const std::string token = trimAscii(normalized.substr(AstralStatOverridePrefix.size()));
        debugAstralStatOverride_ = token == "on" || token == "true" || token == "1";
        logInfo(std::string("Debug: astral stat override => ") + (debugAstralStatOverride_ ? "on." : "off."));
        return true;
    }

    constexpr std::string_view AstralStatPrefix = "game astral stat ";
    if (normalized.rfind(AstralStatPrefix, 0) == 0) {
        const std::string rest = trimAscii(normalized.substr(AstralStatPrefix.size()));
        const std::size_t split = rest.find(' ');
        const std::string key = split == std::string::npos ? rest : rest.substr(0, split);
        const std::string valueText = split == std::string::npos ? std::string{} : rest.substr(split + 1);
        const int value = parseDebugInt(valueText, 0);
        if (key == "kills") {
            debugAstralStatKills_ = std::clamp(value, 0, 9999);
        } else if (key == "dug") {
            debugAstralStatDugTiles_ = std::clamp(value, 0, 99999);
        } else if (key == "items") {
            debugAstralStatItems_ = std::clamp(value, 0, 9999);
        } else if (key == "materials") {
            debugAstralStatMaterials_ = std::clamp(value, 0, 99999);
        } else if (key == "money") {
            debugAstralStatMoney_ = std::clamp(value, 0, 999999);
        } else {
            logWarning("Debug: unknown astral stat key: " + key);
            return true;
        }
        logInfo("Debug: astral stat " + key + " => " + std::to_string(value) + ".");
        return true;
    }

    const auto startOrRegenerateAstralRun = [&]() {
        clearRuntimeDebugPresentation();
        enterDebugBase();
        if (selectDebugStage("stage_04_astral_mine")) {
            startMiningFromBase(false, true);
            logInfo("Debug: astral run started/regenerated.");
        }
    };

    if (normalized == "game astral start" ||
        normalized == "game astral start-new" ||
        normalized == "game astral regenerate") {
        startOrRegenerateAstralRun();
        return true;
    }

    if (normalized == "game astral return-base") {
        if (mode_ == ScreenMode::AstralResult) {
            returnToBaseAfterAstralResult();
        } else if (basePresentationActive()) {
            enterDebugBase();
        } else {
            returnToBaseFromNormalStage(false, false);
        }
        logInfo("Debug: returned from astral debug.");
        return true;
    }

    if (normalized == "game astral warp") {
        if (!astralDungeonReady()) {
            return true;
        }

        if (debugAstralMoveTarget_ == "boss") {
            if (!hasBossSpawnPoint_) {
                logWarning("Debug: astral boss point is not available.");
                return true;
            }
            Vec2 direction = normalize(bossSpawnPoint_);
            if (lengthSquared(direction) <= 0.0001f) {
                direction = {1.0f, 0.0f};
            }
            const int bossRank = roguelikeSectionRankForDepthMeters(
                roguelikeBigHole_.active ? roguelikeBigHole_.depthMeters : astralRun_.nextHoleDepthMeters);
            placePlayerForAstralDebug(bossSpawnPoint_ - direction * (BossSpawnTriggerRadius + 18.0f), bossRank);
        } else if (debugAstralMoveTarget_ == "hole") {
            if (!roguelikeBigHole_.active) {
                logWarning("Debug: astral big hole is not available.");
                return true;
            }
            placePlayerForAstralDebug(
                roguelikeBigHole_.position,
                roguelikeSectionRankForDepthMeters(roguelikeBigHole_.depthMeters));
        } else if (debugAstralMoveTarget_ == "rank") {
            const int depthMeters = roguelikeDepthMetersForSectionRank(debugAstralDepthRank_);
            if (!ensureAstralAreaForDepthMeters(depthMeters)) {
                logWarning("Debug: failed to switch astral area for rank " + std::to_string(debugAstralDepthRank_) + ".");
                return true;
            }
            placePlayerForAstralDebug(mainPathPositionForDepthMeters(depthMeters), debugAstralDepthRank_);
        } else if (debugAstralMoveTarget_ == "entrance") {
            placePlayerForAstralDebug(dungeonEntrancePosition(), 1);
        } else {
            if (!ensureAstralAreaForDepthMeters(debugAstralDepthMeters_)) {
                logWarning("Debug: failed to switch astral area for " + std::to_string(debugAstralDepthMeters_) + "m.");
                return true;
            }
            placePlayerForAstralDebug(
                mainPathPositionForDepthMeters(debugAstralDepthMeters_),
                roguelikeSectionRankForDepthMeters(debugAstralDepthMeters_));
        }
        logInfo("Debug: astral player warped to " + debugAstralMoveTarget_ + ".");
        return true;
    }

    if (normalized == "game astral jump-meters") {
        if (!astralDungeonReady()) {
            return true;
        }
        if (!debugSetRoguelikeAreaForDepthMeters(debugAstralDepthMeters_)) {
            logWarning("Debug: failed to jump astral area to " + std::to_string(debugAstralDepthMeters_) + "m.");
            return true;
        }
        placePlayerForAstralDebug(
            mainPathPositionForDepthMeters(debugAstralDepthMeters_),
            roguelikeSectionRankForDepthMeters(debugAstralDepthMeters_));
        logInfo("Debug: astral area jumped to " + std::to_string(debugAstralDepthMeters_) + "m.");
        return true;
    }

    if (normalized == "game astral room-warp") {
        if (!astralDungeonReady()) {
            return true;
        }
        const DebugAstralRoomTarget target = debugAstralRoomTargetForToken(debugAstralRoomType_);
        if (target.kind == DebugAstralRoomTargetKind::None) {
            logWarning("Debug: astral room target is not valid: " + debugAstralRoomType_);
            return true;
        }
        const int targetIndex = std::max(1, debugAstralRoomIndex_);
        int seen = 0;
        if (target.kind == DebugAstralRoomTargetKind::SpecialRoom) {
            for (const SpecialRoomAnchor& room : dungeonLayout_.specialRoomAnchors) {
                if (room.type != target.specialRoom) {
                    continue;
                }
                ++seen;
                if (seen != targetIndex) {
                    continue;
                }
                const Vec2 center = tileWorldCenter(roundDungeonTile(room.center));
                placePlayerForAstralDebug(center, roguelikeDepthRankForWorldPosition(center));
                logInfo("Debug: warped to astral room " +
                    std::string(debugAstralRoomTargetLabel(target)) +
                    " #" + std::to_string(targetIndex) + ".");
                return true;
            }
        } else if (target.kind == DebugAstralRoomTargetKind::Facility) {
            for (const RoguelikeFacilityInstance& facility : roguelikeFacilities_) {
                if (facility.kind != target.facilityKind) {
                    continue;
                }
                ++seen;
                if (seen != targetIndex) {
                    continue;
                }
                placePlayerForAstralDebug(
                    facility.centerPosition,
                    roguelikeSectionRankForDepthMeters(facility.depthMeters));
                logInfo("Debug: warped to astral room " +
                    std::string(debugAstralRoomTargetLabel(target)) +
                    " #" + std::to_string(targetIndex) + ".");
                return true;
            }
        }
        logWarning("Debug: astral room not found: " +
            std::string(debugAstralRoomTargetLabel(target)) +
            " #" + std::to_string(targetIndex) + ".");
        return true;
    }

    if (normalized == "game astral report-generation") {
        const int maxDepth = astralRunActive() ? std::max(1, astralRun_.maxDepth) : 9;
        std::array<int, 6> roomCounts{};
        for (const SpecialRoomAnchor& room : dungeonLayout_.specialRoomAnchors) {
            const int index = static_cast<int>(room.type);
            if (index >= 0 && index < static_cast<int>(roomCounts.size())) {
                ++roomCounts[static_cast<std::size_t>(index)];
            }
        }
        std::array<int, 3> facilityCounts{};
        for (const RoguelikeFacilityInstance& facility : roguelikeFacilities_) {
            switch (facility.kind) {
            case RoguelikeFacilityKind::Merchant:
                ++facilityCounts[0];
                break;
            case RoguelikeFacilityKind::Artisan:
                ++facilityCounts[1];
                break;
            case RoguelikeFacilityKind::Trainer:
                ++facilityCounts[2];
                break;
            }
        }
        logInfo("Debug: astral generation stage=" + currentStageId_ +
            " active=" + (astralRunActive() ? std::string("true") : std::string("false")) +
            " rank=" + std::to_string(astralRun_.currentDepth) + "/" + std::to_string(maxDepth) +
            " area=" + std::to_string(astralRun_.currentDepthMeters) + "-" + std::to_string(astralRun_.nextHoleDepthMeters) + "m" +
            " distortionMode=" + debugAstralDistortionMode_ +
            " currentDistortion=" + distortionLabel(astralRun_.distortion));
        logInfo("Debug: astral rooms ore=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::OreRoom)]) +
            " safe=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::SafeCavern)]) +
            " coin=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::CoinRoom)]) +
            " treasure=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::TreasureRoom)]) +
            " enemy=" + std::to_string(roomCounts[static_cast<std::size_t>(SpecialRoomType::EnemyRoom)]) +
            " boss=" + (hasBossSpawnPoint_ ? "true" : "false"));
        logInfo("Debug: astral facilities merchant=" + std::to_string(facilityCounts[0]) +
            " artisan=" + std::to_string(facilityCounts[1]) +
            " trainer=" + std::to_string(facilityCounts[2]) +
            " bigHole=" + (roguelikeBigHole_.active ? std::to_string(roguelikeBigHole_.depthMeters) + "m" : std::string("none")) +
            " unlocked=" + (roguelikeBigHole_.unlocked ? "true" : "false"));
        return true;
    }

    if (normalized == "game astral report-stats") {
        logInfo("Debug: astral stats kills=" + std::to_string(runStats_.defeatedEnemies) +
            " dug=" + std::to_string(runStats_.dugTiles) +
            " items=" + std::to_string(runStats_.acquiredObjectItems) +
            " materials=" + std::to_string(astralRunMaterialDeltaFromStart()) +
            " money=" + std::to_string(astralRunMoneyDeltaFromStart()) +
            " highScore=" + std::to_string(astralHighScore_));
        return true;
    }

    if (normalized == "game astral high-score reset") {
        astralHighScore_ = 0;
        logInfo("Debug: astral high score reset.");
        return true;
    }

    if (normalized == "game astral finish") {
        const AstralRunResult result = resultForDebugToken(debugAstralResultKind_);
        const int highScoreBeforeResult = astralHighScore_;
        enterAstralResult(result);
        if (debugAstralStatOverride_) {
            astralHighScore_ = highScoreBeforeResult;
            AstralRunSummary summary = makeAstralRunSummary(result);
            summary.reachedDepth = std::clamp(debugAstralDepthRank_, 1, std::max(1, summary.maxDepth));
            summary.defeatedEnemies = std::max(0, debugAstralStatKills_);
            summary.dugTiles = std::max(0, debugAstralStatDugTiles_);
            summary.acquiredItems = std::max(0, debugAstralStatItems_);
            summary.acquiredMaterials = std::max(0, debugAstralStatMaterials_);
            summary.acquiredMoney = std::max(0, debugAstralStatMoney_);
            summary.carriedOut = result != AstralRunResult::Died;
            summary.score = calculateAstralRunScore(summary);
            summary.highScore = astralHighScore_;
            summary.highScoreUpdated = false;
            if (summary.carriedOut && summary.score > astralHighScore_) {
                astralHighScore_ = summary.score;
                summary.highScore = astralHighScore_;
                summary.highScoreUpdated = true;
            }
            astralResult_ = summary;
        } else if (mode_ == ScreenMode::AstralResult && astralResult_.result != result) {
            AstralRunSummary summary = makeAstralRunSummary(result);
            if (summary.carriedOut && summary.score > astralHighScore_) {
                astralHighScore_ = summary.score;
                summary.highScore = astralHighScore_;
                summary.highScoreUpdated = true;
            }
            astralResult_ = summary;
        }
        logInfo("Debug: astral result shown: " + std::string(resultLabel(result)) + ".");
        return true;
    }

    constexpr std::string_view BossFlowTargetPrefix = "game boss-flow target ";
    if (normalized.rfind(BossFlowTargetPrefix, 0) == 0) {
        const std::string stageId = debugStageIdForToken(std::string_view(normalized).substr(BossFlowTargetPrefix.size()));
        if (stageId.empty() || stageId == "stage_04_astral_mine") {
            logWarning("Debug: unknown boss-flow target: " + normalized.substr(BossFlowTargetPrefix.size()));
            return true;
        }
        clearRuntimeDebugPresentation();
        enterDebugBase();
        if (selectDebugStage(stageId)) {
            logInfo("Debug: boss flow target set to " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game boss-flow before") {
        clearRuntimeDebugPresentation();
        resetBossFlowStoryForCurrentStage();
        removeCapturedBossForCurrentStage();
        if (buildDebugDungeonWithAllWarps(false, false)) {
            placePlayerAtBossApproach();
            baseStatus_.clear();
            logInfo("Debug: boss flow set to boss approach for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game boss-flow defeated") {
        clearRuntimeDebugPresentation();
        resetBossFlowStoryForCurrentStage();
        markStoryTriggerSeenForCurrentStage("boss_before");
        removeCapturedBossForCurrentStage();
        if (buildDebugDungeonWithAllWarps(false, true)) {
            const Vec2 defeatPosition = hasBossSpawnPoint_ ? bossSpawnPoint_ : player_.position;
            player_.position = defeatPosition;
            camera_.follow(player_.position, 1.0f);
            beginBossDefeatSequence(defeatPosition);
            logInfo("Debug: boss flow set to defeated presentation for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game boss-flow clear") {
        clearRuntimeDebugPresentation();
        markStoryTriggerSeenForCurrentStage("boss_before");
        markStoryTriggerSeenForCurrentStage("boss_after");
        if (buildDebugDungeonWithAllWarps(true, true)) {
            enterStageClear();
            logInfo("Debug: boss flow set to stage clear result for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game reset-data") {
        money_ = 0;
        maxHpUpgradeLevel_ = 0;
        ringRadiusUpgradeLevel_ = 0;
        ringSpeedUpgradeLevel_ = 0;
        collectionRangeUpgradeLevel_ = 0;
        levelRingUpgradePoints_ = {};
        workshopRingUpgrades_ = {};
        merchantRefreshPending_ = false;
        merchantUpgradeLevel_ = 1;
        merchantStockVersion_ = 0;
        merchantStock_.clear();
        highValueBuyCategory_.clear();
        highValueBuyObjectIds_.clear();
        warehouseCapacityLevel_ = 0;
        processingUnlockLevel_ = 0;
        ringWorkshopUnlocked_ = false;
        ringPresetSlotLevel_ = 0;
        autoSaveOnReturn_ = false;
        storyFlags_.clear();
        warehouseObjectStacks_.clear();
        warehouseObjectInstances_.clear();
        unlockedStages_ = 1;
        setUnlockedRingCount(1);
        unlockedWarpPointCount_ = 0;
        hasLatestWarpPointPosition_ = false;
        dungeonStates_.clear();
        requestedWarpPointStartPosition_.reset();
        currentStage_ = 0;
        currentStageId_ = "stage_01_stardust";
        resolveCurrentStageDefinition();
        encyclopedia_ = EncyclopediaSystem{};
        encyclopediaOwnedSyncSuppressCounts_.clear();
        encyclopediaRingSyncSuppressCounts_.clear();
        initializeWorld(false);
        enterBase();
        captureRunStartInventoryState();
        captureEncyclopediaSyncSuppressState();

        std::string saveMessage;
        if (saveSaveData(saveMessage)) {
            logInfo("Debug: game data reset and saved.");
        } else {
            logWarning("Debug: game data reset in memory, but save failed: " + saveMessage);
        }
        return true;
    }

    if (normalized == "game codex reset" || normalized == "game encyclopedia reset") {
        encyclopedia_ = EncyclopediaSystem{};
        captureEncyclopediaSyncSuppressState();
        reloadNotice_ = "図鑑をリセット";
        reloadNoticeTimer_ = 1.8f;
        baseStatus_ = "図鑑をリセットしました";

        std::string saveMessage;
        if (saveSaveData(saveMessage)) {
            logInfo("Debug: codex reset and saved.");
        } else {
            logWarning("Debug: codex reset in memory, but save failed: " + saveMessage);
        }
        return true;
    }

    if (normalized == "game codex complete" || normalized == "game encyclopedia complete") {
        encyclopedia_ = EncyclopediaSystem{};

        int objectCount = 0;
        for (const ItemData& object : objectCatalog_.registry.items()) {
            if (object.id.empty() || isCodexHiddenObject(object)) {
                continue;
            }
            const bool treasure = object.category == "宝";
            encyclopedia_.loadEntry(
                treasure ? EncyclopediaKind::Treasure : EncyclopediaKind::Item,
                object.id,
                EncyclopediaStage::Complete);
            ++objectCount;

            for (const DiscoveryEffectLine& line : object.discoveryEffectLines) {
                if (!line.effectKey.empty()) {
                    encyclopedia_.loadEffect(object.id, line.effectKey);
                }
            }
        }

        int enemyCount = 0;
        for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
            if (enemy.id.empty() || isCodexHiddenEnemy(enemy)) {
                continue;
            }
            encyclopedia_.loadEntry(EncyclopediaKind::Enemy, enemy.id, EncyclopediaStage::Complete);
            ++enemyCount;
        }

        captureEncyclopediaSyncSuppressState();
        reloadNotice_ = "図鑑を完成";
        reloadNoticeTimer_ = 1.8f;
        baseStatus_ = "図鑑を完成状態にしました";

        std::string saveMessage;
        if (saveSaveData(saveMessage)) {
            logInfo("Debug: codex completed and saved. objects=" + std::to_string(objectCount) +
                " enemies=" + std::to_string(enemyCount) +
                " effects=" + std::to_string(encyclopedia_.saveEffects().size()) + ".");
        } else {
            logWarning("Debug: codex completed in memory, but save failed: " + saveMessage);
        }
        return true;
    }

    if (normalized == "game stage-unlock initial" ||
        normalized == "game stage-unlock reset" ||
        normalized == "game stage-unlock stage1" ||
        normalized == "game stage-unlock 初期状態") {
        return applyStageUnlockDebugCommand(1, "initial");
    }

    if (normalized == "game stage-unlock stage2" ||
        normalized == "game stage-unlock 2" ||
        normalized == "game stage-unlock ステージ2解放") {
        return applyStageUnlockDebugCommand(2, "stage2");
    }

    if (normalized == "game stage-unlock stage3" ||
        normalized == "game stage-unlock 3" ||
        normalized == "game stage-unlock ステージ3解放") {
        return applyStageUnlockDebugCommand(3, "stage3");
    }

    if (normalized == "game return-base") {
        if (effectTestActive_) {
            exitEffectTestToBase();
            logInfo("Debug: effect test exited to base.");
            return true;
        }
        if (projectileTestActive_) {
            exitProjectileTestToBase();
            logInfo("Debug: projectile test exited to base.");
            return true;
        }
        if (enemyTestActive_) {
            exitEnemyTestToBase();
            logInfo("Debug: enemy test exited to base.");
            return true;
        }
        returnToBaseFromNormalStage(false, false);
        logInfo("Debug: returned to base.");
        return true;
    }

    if (normalized == "game warp-points unlock-all" ||
        normalized == "game warp-point unlock-all" ||
        normalized == "game unlock-warps") {
        if (unlockAllWarpPointsForCurrentDungeon()) {
            logInfo("Debug: all warp points unlocked for current dungeon.");
        } else {
            logWarning("Debug: warp point unlock-all requires an active dungeon with warp points.");
        }
        return true;
    }

    constexpr std::string_view RematchTargetPrefix = "game rematch target ";
    if (normalized.rfind(RematchTargetPrefix, 0) == 0) {
        const std::string stageId = debugStageIdForToken(std::string_view(normalized).substr(RematchTargetPrefix.size()));
        if (stageId.empty()) {
            logWarning("Debug: unknown rematch target: " + normalized.substr(RematchTargetPrefix.size()));
            return true;
        }
        clearRuntimeDebugPresentation();
        enterDebugBase();
        if (selectDebugStage(stageId)) {
            logInfo("Debug: rematch target set to " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game rematch unlock-warps") {
        clearRuntimeDebugPresentation();
        if (buildDebugDungeonWithAllWarps(false, true)) {
            returnToBaseFromNormalStage(false, false);
            logInfo("Debug: all warp points discovered for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game rematch mark-clear") {
        clearRuntimeDebugPresentation();
        markCurrentStageClearedForDebug();
        enterDebugBase();
        logInfo("Debug: stage marked cleared for rematch: " + currentStageId_ + ".");
        return true;
    }

    if (normalized == "game rematch setup-regenerate") {
        clearRuntimeDebugPresentation();
        if (buildDebugDungeonWithAllWarps(true, true)) {
            returnToBaseFromNormalStage(false, false);
            baseMiningStartChoiceActive_ = true;
            baseMiningStartSelection_ = 2;
            logInfo("Debug: regenerate-ready rematch state prepared for " + currentStageId_ + ".");
        }
        return true;
    }

    if (normalized == "game rematch captured-boss add") {
        clearRuntimeDebugPresentation();
        if (hasCapturedBossForCurrentStage()) {
            logInfo("Debug: captured boss already owned for " + currentStageId_ + ".");
            return true;
        }

        ItemData capturedBoss;
        capturedBoss.id = currentStageBossCaptureObjectId();
        capturedBoss.name = "捕獲ボス";
        capturedBoss.category = "捕獲";
        capturedBoss.description = "デバッグ用の捕獲済みボス判定アイテムです。";
        capturedBoss.rarity = 5;
        capturedBoss.price = 0;
        capturedBoss.damageType = "none";
        capturedBoss.durability = -1;
        capturedBoss.weightKg = 1.0;
        capturedBoss.tags = {"captured_boss"};
        if (inventory_.addRuntimeObjectItem(capturedBoss)) {
            logInfo("Debug: captured boss item added for " + currentStageId_ + ".");
        } else {
            logWarning("Debug: failed to add captured boss item; inventory may be full.");
        }
        return true;
    }

    if (normalized == "game rematch captured-boss remove") {
        clearRuntimeDebugPresentation();
        const int removedCount = removeCapturedBossForCurrentStage();
        logInfo("Debug: captured boss items removed: " + std::to_string(removedCount) + ".");
        return true;
    }

    if (normalized == "game launch-mode pre-title" ||
        normalized == "game launch-mode before-title" ||
        normalized == "game launch-mode opening") {
        if (effectTestActive_) {
            exitEffectTestToBase();
        } else if (projectileTestActive_) {
            exitProjectileTestToBase();
        } else if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (!basePresentationActive() && mode_ != ScreenMode::OpeningKamishibai && mode_ != ScreenMode::EndingKamishibai && mode_ != ScreenMode::Title) {
            returnToBaseFromNormalStage(false, false);
        } else {
            enterBase();
        }
        startOpeningKamishibai();
        logInfo("Debug: launch mode set to pre-title opening.");
        return true;
    }

    if (normalized == "game launch-mode base") {
        if (effectTestActive_) {
            exitEffectTestToBase();
        } else if (projectileTestActive_) {
            exitProjectileTestToBase();
        } else if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (basePresentationActive() || mode_ == ScreenMode::OpeningKamishibai || mode_ == ScreenMode::EndingKamishibai || mode_ == ScreenMode::Title) {
            enterBase();
        } else {
            returnToBaseFromNormalStage(false, false);
        }
        logInfo("Debug: launch mode set to base.");
        return true;
    }

    if (normalized == "game launch-mode dungeon") {
        if (effectTestActive_) {
            exitEffectTestToBase();
        } else if (projectileTestActive_) {
            exitProjectileTestToBase();
        } else if (enemyTestActive_) {
            exitEnemyTestToBase();
        }
        startMiningFromBase(false, false);
        logInfo("Debug: launch mode set to dungeon start.");
        return true;
    }

    if (normalized == "game launch-mode enemy-test") {
        enterEnemyTestMode();
        logInfo("Debug: launch mode set to enemy test.");
        return true;
    }

    if (normalized == "game launch-mode projectile-test" ||
        normalized == "game launch-mode bullet-test") {
        enterProjectileTestMode();
        logInfo("Debug: launch mode set to projectile test.");
        return true;
    }

    if (normalized == "game launch-mode final-boss-before" ||
        normalized == "game launch-mode final-boss") {
        clearRuntimeDebugPresentation();
        applyDebugStageUnlockState(3);
        if (!selectDebugStage(DebugFinalStoryStageId)) {
            return true;
        }
        eraseStoryFlag(DebugEndingSeenFlag);
        eraseStoryFlag(DebugEndingMainFlag);
        eraseStoryFlag(DebugStage03ClearFlag);
        eraseStoryFlag(DebugPostEndingIntroFlag);
        eraseStoryFlag("story_stage_03_boss_before");
        eraseStoryFlag("story_stage_03_boss_after");
        eraseStoryFlag(stageClearFlagForStage(currentStageId_));
        removeCapturedBossForCurrentStage();
        if (buildDebugDungeonWithAllWarps(false, false)) {
            placePlayerAtBossApproach();
            baseStatus_.clear();
            logInfo("Debug: launch mode set to final boss approach.");
        }
        return true;
    }

    if (normalized == "game launch-mode final-boss-after") {
        clearRuntimeDebugPresentation();
        applyDebugStageUnlockState(3);
        if (!selectDebugStage(DebugFinalStoryStageId)) {
            return true;
        }
        eraseStoryFlag(DebugEndingSeenFlag);
        eraseStoryFlag(DebugEndingMainFlag);
        eraseStoryFlag(DebugStage03ClearFlag);
        eraseStoryFlag(DebugPostEndingIntroFlag);
        eraseStoryFlag("story_stage_03_boss_after");
        removeCapturedBossForCurrentStage();
        if (buildDebugDungeonWithAllWarps(false, true)) {
            beginBossDefeatSequence(player_.position);
            logInfo("Debug: launch mode set to final boss defeated event.");
        }
        return true;
    }

    if (normalized == "game launch-mode ending-kamishibai" ||
        normalized == "game launch-mode ending-paper") {
        clearRuntimeDebugPresentation();
        applyDebugStageUnlockState(3);
        if (!selectDebugStage(DebugFinalStoryStageId)) {
            return true;
        }
        eraseStoryFlag(DebugEndingSeenFlag);
        eraseStoryFlag(DebugEndingMainFlag);
        eraseStoryFlag(DebugStage03ClearFlag);
        eraseStoryFlag(DebugPostEndingIntroFlag);
        startEndingKamishibai();
        logInfo("Debug: launch mode set to ending kamishibai.");
        return true;
    }

    if (normalized == "game launch-mode post-ending-base" ||
        normalized == "game launch-mode ending-base") {
        clearRuntimeDebugPresentation();
        applyDebugStageUnlockState(3);
        if (!selectDebugStage(DebugFinalStoryStageId)) {
            return true;
        }
        addStoryFlag(std::string(DebugEndingSeenFlag));
        addStoryFlag(std::string(DebugEndingMainFlag));
        addStoryFlag(std::string(DebugStage03ClearFlag));
        addStoryFlag(stageClearFlagForStage(currentStageId_));
        eraseStoryFlag(DebugPostEndingIntroFlag);
        placeBasePlayerAtMineExitReturnPoint();
        enterDebugBase();
        queueStoryEventForTrigger("post_ending:intro");
        logInfo("Debug: launch mode set to post-ending base.");
        return true;
    }

    if (normalized == "game enemy-test") {
        enterEnemyTestMode();
        logInfo("Debug: enemy test mode started.");
        return true;
    }

    if (normalized == "game effect-test" || normalized == "game effects-test") {
        enterEffectTestMode();
        logInfo("Debug: effect test mode started.");
        return true;
    }

    if (normalized == "game projectile-test" ||
        normalized == "game projectiles-test" ||
        normalized == "game bullet-test" ||
        normalized == "game bullets-test") {
        enterProjectileTestMode();
        logInfo("Debug: projectile test mode started.");
        return true;
    }

    if (normalized == "game save") {
        std::string message;
        if (saveSaveData(message)) {
            logInfo("Debug: " + message);
        } else {
            logWarning("Debug: " + message);
        }
        return true;
    }

    constexpr std::string_view DebugMoneyAmountPrefix = "game debug money-amount ";
    if (normalized.rfind(DebugMoneyAmountPrefix, 0) == 0) {
        debugMoneyAddAmount_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(DebugMoneyAmountPrefix.size()), debugMoneyAddAmount_),
            1,
            999999);
        logInfo("Debug: money add amount => " + std::to_string(debugMoneyAddAmount_) + "G.");
        return true;
    }

    constexpr std::string_view MoneyAddPrefix = "game money add ";
    if (normalized == "game money add-debug" || normalized.rfind(MoneyAddPrefix, 0) == 0) {
        const int amount = normalized == "game money add-debug"
            ? debugMoneyAddAmount_
            : parseDebugInt(std::string_view(normalized).substr(MoneyAddPrefix.size()), debugMoneyAddAmount_);
        const int clampedAmount = std::clamp(amount, 1, 999999);
        money_ = std::max(0, money_ + clampedAmount);
        logInfo("Debug: money +" + std::to_string(clampedAmount) + " => " + std::to_string(money_) + "G");
        return true;
    }

    if (normalized == "game money reset") {
        money_ = 0;
        baseStatus_ = "所持金をリセットしました";
        logInfo("Debug: money reset to 0G.");
        return true;
    }

    constexpr std::string_view DebugMaterialAmountPrefix = "game debug material-amount ";
    if (normalized.rfind(DebugMaterialAmountPrefix, 0) == 0) {
        debugMaterialAddAmount_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(DebugMaterialAmountPrefix.size()), debugMaterialAddAmount_),
            1,
            99999);
        logInfo("Debug: material add amount => " + std::to_string(debugMaterialAddAmount_) + ".");
        return true;
    }

    constexpr std::string_view MaterialsAddPrefix = "game materials add ";
    if (normalized == "game materials add-debug" || normalized.rfind(MaterialsAddPrefix, 0) == 0) {
        const int amount = normalized == "game materials add-debug"
            ? debugMaterialAddAmount_
            : parseDebugInt(std::string_view(normalized).substr(MaterialsAddPrefix.size()), debugMaterialAddAmount_);
        const int clampedAmount = std::clamp(amount, 1, 99999);
        for (int index = 0; index < static_cast<int>(MaterialType::Count); ++index) {
            inventory_.addMaterial(static_cast<MaterialType>(index), clampedAmount);
        }
        logInfo("Debug: all upgrade materials +" + std::to_string(clampedAmount) + ".");
        return true;
    }

    if (normalized == "game materials reset") {
        for (int index = 0; index < static_cast<int>(MaterialType::Count); ++index) {
            inventory_.setMaterialCount(static_cast<MaterialType>(index), 0);
        }
        baseStatus_ = "強化素材をリセットしました";
        logInfo("Debug: all upgrade materials reset.");
        return true;
    }

    if (normalized == "game ring-workshop unlock" ||
        normalized == "game ring workshop unlock") {
        ringWorkshopUnlocked_ = true;
        baseStatus_ = "リング工房を解禁しました";
        logInfo("Debug: ring workshop unlocked.");
        return true;
    }

    if (normalized == "game ring unlock 2" ||
        normalized == "game rings unlock 2" ||
        normalized == "game ring2 unlock" ||
        normalized == "game ring 2 unlock") {
        setUnlockedRingCount(std::max(unlockedRingCount(), 2));
        baseStatus_ = "リング2を解禁しました";
        logInfo("Debug: unlocked rings up to Ring 2.");
        return true;
    }

    if (normalized == "game ring unlock 3" ||
        normalized == "game rings unlock 3" ||
        normalized == "game ring3 unlock" ||
        normalized == "game ring 3 unlock") {
        setUnlockedRingCount(std::max(unlockedRingCount(), 3));
        baseStatus_ = "リング3を解禁しました";
        logInfo("Debug: unlocked rings up to Ring 3.");
        return true;
    }

    if (normalized == "game ring unlock reset" ||
        normalized == "game rings unlock reset" ||
        normalized == "game ring reset" ||
        normalized == "game rings reset") {
        setUnlockedRingCount(1);
        baseStatus_ = "リング解禁状態をリセットしました";
        logInfo("Debug: ring unlock state reset to Ring 1 only.");
        return true;
    }

    constexpr std::string_view DebugRandomItemCountPrefix = "game debug random-item-count ";
    if (normalized.rfind(DebugRandomItemCountPrefix, 0) == 0) {
        debugRandomItemCount_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(DebugRandomItemCountPrefix.size()), debugRandomItemCount_),
            1,
            99);
        logInfo("Debug: random item count => " + std::to_string(debugRandomItemCount_) + ".");
        return true;
    }

    constexpr std::string_view ItemsRandomPrefix = "game items random ";
    if (normalized == "game items random8" ||
        normalized == "game items random-debug" ||
        normalized.rfind(ItemsRandomPrefix, 0) == 0) {
        const int count = normalized == "game items random8"
            ? 8
            : (normalized == "game items random-debug"
                ? debugRandomItemCount_
                : parseDebugInt(std::string_view(normalized).substr(ItemsRandomPrefix.size()), debugRandomItemCount_));
        addRandomDebugItems(count);
        return true;
    }

    if (normalized == "game items reset" || normalized == "game inventory reset") {
        inventory_.clearObjectStacks();
        inventory_.setOpen(false);
        inventory_.cancelGrab();
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ringPlaceModeActive_ = false;
        ringGrabActive_ = false;
        ringGrabOrigin_ = -1;
        ringGrabbedItem_ = {};
        firstItemAcquisitionNotices_.clear();

        initializeDefaultSpellRing();
        refreshEquipmentModifiers();
        applyPermanentUpgrades();
        spellRing_.applyObjectParameters(objectCatalog_);
        refreshOrbitEffects();
        captureRunStartInventoryState();

        baseStatus_ = "所持アイテムをリセットしました";
        logInfo("Debug: carried inventory reset to default ring items.");
        return true;
    }

    if (normalized == "game hp full") {
        applyPermanentUpgrades();
        player_.hp = player_.maxHp;
        player_.status = EntityStatus{};
        player_.poisonDamageAccumulator = 0.0;
        player_.hotDamageAccumulator = 0.0;
        player_.bleedDamageAccumulator = 0.0;
        logInfo("Debug: player HP restored to " + std::to_string(player_.maxHp) + ".");
        return true;
    }

    constexpr std::string_view DebugHpValuePrefix = "game debug hp-value ";
    if (normalized.rfind(DebugHpValuePrefix, 0) == 0) {
        debugHpValue_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(DebugHpValuePrefix.size()), debugHpValue_),
            1,
            999);
        logInfo("Debug: HP value => " + std::to_string(debugHpValue_) + ".");
        return true;
    }

    constexpr std::string_view HpSetPrefix = "game hp set ";
    if (normalized == "game hp set-debug" || normalized.rfind(HpSetPrefix, 0) == 0) {
        const int hp = normalized == "game hp set-debug"
            ? debugHpValue_
            : parseDebugInt(std::string_view(normalized).substr(HpSetPrefix.size()), debugHpValue_);
        setPlayerHpForDebug(hp);
        return true;
    }

    constexpr std::string_view DebugTargetLevelPrefix = "game debug target-level ";
    if (normalized.rfind(DebugTargetLevelPrefix, 0) == 0) {
        debugTargetLevel_ = std::clamp(
            parseDebugInt(std::string_view(normalized).substr(DebugTargetLevelPrefix.size()), debugTargetLevel_),
            1,
            PlayerMaxLevel);
        logInfo("Debug: target level => Lv " + std::to_string(debugTargetLevel_) + ".");
        return true;
    }

    constexpr std::string_view LevelSetPrefix = "game level set ";
    if (normalized == "game level set-debug" || normalized.rfind(LevelSetPrefix, 0) == 0) {
        const int level = normalized == "game level set-debug"
            ? debugTargetLevel_
            : parseDebugInt(std::string_view(normalized).substr(LevelSetPrefix.size()), debugTargetLevel_);
        setPlayerLevelForDebug(level);
        return true;
    }

    if (normalized == "game level-up" || normalized == "game levelup") {
        if (levels_.isChoosing() || levelUpResultDialog_.open) {
            logWarning("Debug: level-up choice is already active.");
            return true;
        }
        const bool baseContext =
            mode_ == ScreenMode::Base ||
            (pauseReturnMode_ == ScreenMode::Base &&
                (mode_ == ScreenMode::PauseMenu || mode_ == ScreenMode::Inventory || mode_ == ScreenMode::Ring));
        const bool dungeonRunMode =
            mode_ == ScreenMode::Playing ||
            (pauseReturnMode_ == ScreenMode::Playing &&
                (mode_ == ScreenMode::PauseMenu || mode_ == ScreenMode::Inventory || mode_ == ScreenMode::Ring));
        if (!baseContext && !dungeonRunMode) {
            logWarning("Debug: level-up requires base or an active dungeon run.");
            return true;
        }

        if (playerAtMaxLevel(player_)) {
            player_.level = PlayerMaxLevel;
            player_.xp = 0;
            player_.xpToNext = 0;
            logWarning("Debug: player level is already max.");
            return true;
        }

        player_.xpToNext = playerXpToNextForLevel(player_.level, balance_);
        const int xpNeeded = std::max(1, player_.xpToNext - player_.xp);
        const LevelGainResult result = gainPlayerXp(xpNeeded);
        if (result.levelsGained <= 0) {
            logWarning("Debug: level-up did not change player level.");
            return true;
        }
        inventory_.setOpen(false);
        inventoryReturnToPause_ = false;
        levelUpPresentation_ = {};
        levelUpResultDialog_ = {};
        openLevelUpChoice(baseContext ? ScreenMode::Base : ScreenMode::Playing);
        logInfo("Debug: forced level up to Lv " + std::to_string(player_.level) + ".");
        return true;
    }

    return false;
}

void Game::renderBaseDebugOverlay(Renderer& renderer, const Time& time) const
{
    if (!debug_.visible() || !basePresentationActive()) {
        return;
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }

    const BaseFacility* nearest = nullptr;
    const BaseFacility* hovered = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    const Vec2 mouse{mouseX, mouseY};
    for (const BaseFacility& facility : facilities) {
        if (!facility.enabled) {
            continue;
        }
        if (facility.rect.contains(mouse)) {
            hovered = &facility;
        }
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }

    char debugBuffer[768];
    std::snprintf(debugBuffer, sizeof(debugBuffer),
        "FPS: %03d   Auto reload block: %s\n"
        "Base: area %s   mode %s\n"
        "Player: pos %.1f, %.1f\n"
        "Nearest: %s   interact %s\n"
        "Hovered: %s\n"
        "ReturnMode: %s",
        static_cast<int>(time.fps()),
        autoReloadBlocked_ ? "ON" : "OFF",
        baseAreaName(baseArea_),
        baseEditEnabled_ ? "edit" : "normal",
        basePlayerPosition_.x,
        basePlayerPosition_.y,
        nearest != nullptr ? nearest->displayName : "-",
        nearest != nullptr ? "true" : "false",
        hovered != nullptr ? hovered->displayName : "-",
        screenModeName(pauseReturnMode_));

    renderer.setScreenSpace();
    constexpr Vec2 PanelPos{10.0f, 10.0f};
    constexpr float PanelWidth = 570.0f;
    constexpr float PanelPadding = 10.0f;
    constexpr int TextScale = 2;
    constexpr float MinPanelHeight = 40.0f;
    const float textWidth = PanelWidth - PanelPadding * 2.0f;
    const Vec2 textSize = renderer.measureWrappedText(debugBuffer, textWidth, TextScale);
    const float panelHeight = std::max(MinPanelHeight, textSize.y + PanelPadding * 2.0f);
    renderer.fillRect(PanelPos, {PanelWidth, panelHeight}, {0, 0, 0, 125});
    renderer.drawWrappedText(
        PanelPos + Vec2{PanelPadding, PanelPadding},
        debugBuffer,
        textWidth,
        {220, 244, 224, 255},
        TextScale);
}

void Game::setAutoSimulationIntentOverlay(bool active, std::vector<autosim::AutoSimulationIntent> history)
{
    autoSimulationIntentOverlayActive_ = active;
    autoSimulationIntentHistory_ = std::move(history);
}

void Game::setAutoSimulationDebugOverlay(bool active, autosim::AutoSimulationDebugSnapshot debug)
{
    autoSimulationDebugOverlayActive_ = active;
    autoSimulationDebug_ = std::move(debug);
}

void Game::renderAutoSimulationIntentOverlay(Renderer& renderer) const
{
    if (!autoSimulationIntentOverlayActive_ || autoSimulationIntentHistory_.empty()) {
        return;
    }

    renderer.setScreenSpace();
    constexpr int TextScale = 2;
    constexpr float IconSize = 29.0f;
    constexpr float IconGap = 6.0f;
    constexpr float PaddingX = 18.0f;
    constexpr float PaddingY = 10.0f;
    constexpr float LineGap = 3.0f;
    constexpr float MaxPanelWidth = 790.0f;
    const float screenWidth = static_cast<float>(camera_.width());
    const float maxContentWidth = std::max(80.0f, std::min(MaxPanelWidth, screenWidth - 36.0f) - PaddingX * 2.0f);
    const int lineCount = std::min<int>(3, static_cast<int>(autoSimulationIntentHistory_.size()));

    std::vector<AutoSimulationIntentLineLayout> layouts;
    layouts.reserve(static_cast<std::size_t>(lineCount));
    float contentWidth = 0.0f;
    for (int i = 0; i < lineCount; ++i) {
        layouts.push_back(makeAutoSimulationIntentLineLayout(
            renderer,
            autoSimulationIntentHistory_[static_cast<std::size_t>(i)],
            maxContentWidth,
            TextScale,
            IconSize,
            IconGap));
        contentWidth = std::max(contentWidth, layouts.back().width);
    }

    const Vec2 textMeasure = renderer.measureText("0", TextScale);
    const float lineHeight = std::max(textMeasure.y, IconSize + 2.0f);
    const float panelWidth = std::min(MaxPanelWidth, std::max(220.0f, contentWidth + PaddingX * 2.0f));
    const float panelHeight = PaddingY * 2.0f + lineHeight * static_cast<float>(lineCount) + LineGap * static_cast<float>(std::max(0, lineCount - 1));
    const Vec2 panelPos{
        std::max(12.0f, (screenWidth - panelWidth) * 0.5f),
        TopInfoBarY + TopInfoBarHeight + 10.0f,
    };

    renderer.fillRect(panelPos, {panelWidth, panelHeight}, {0, 0, 0, 148});
    renderer.drawRect(panelPos, {panelWidth, panelHeight}, {190, 218, 236, 132});

    for (int i = 0; i < lineCount; ++i) {
        const float fade = i == 0 ? 1.0f : (i == 1 ? 0.70f : 0.48f);
        const unsigned char textAlpha = alphaByte(255.0f * fade);
        const unsigned char outlineAlpha = alphaByte(210.0f * fade);
        const Color textColor = i == 0
            ? Color{255, 248, 210, textAlpha}
            : Color{214, 226, 236, textAlpha};
        const Color outlineColor{0, 0, 0, outlineAlpha};
        const Vec2 linePos{
            panelPos.x + (panelWidth - layouts[static_cast<std::size_t>(i)].width) * 0.5f,
            panelPos.y + PaddingY + static_cast<float>(i) * (lineHeight + LineGap) + 2.0f,
        };
        drawAutoSimulationIntentLine(
            renderer,
            objectCatalog_,
            autoSimulationIntentHistory_[static_cast<std::size_t>(i)],
            layouts[static_cast<std::size_t>(i)],
            linePos,
            TextScale,
            IconSize,
            IconGap,
            textColor,
            outlineColor);
    }
}

void Game::renderDebugOverlay(Renderer& renderer, const Time& time)
{
    if (!debug_.visible()) {
        return;
    }

    const int nearestWarp = nearestWarpPointIndex(player_.position);
    bool nearestWarpDiscovered = false;
    for (const WarpPoint& point : warpPoints_) {
        if (point.index == nearestWarp) {
            nearestWarpDiscovered = point.discovered;
            break;
        }
    }

    debug_.render(
        renderer,
        time,
        enemies_,
        tileMap_,
        spellRing_,
        player_,
        balance_,
        dungeonLayout_,
        currentStageDefinition(),
        nearestWarp,
        nearestWarpDiscovered,
        discoveredWarpPointCount(),
        unlockedWarpPointCount_,
        hasLatestWarpPointPosition_,
        latestWarpPointPosition_,
        requestedWarpPointStartPosition_.has_value(),
        requestedWarpPointStartPosition_.value_or(Vec2{}),
        rewardNodeCount(),
        moneyNodeCount(),
        buriedVisibleNodeCount(),
        buriedHiddenNodeCount(),
        exposedEnemyNodeCount(),
        buriedEnemyNodeCount(),
        spawnedEnemyNodeCount(),
        autoReloadBlocked_,
        autoSimulationDebugOverlayActive_,
        autoSimulationDebug_);

    const std::string focusDebug = dungeonFocusDebugText() + "\n" + nearestDungeonEventDebugText();
    renderer.setScreenSpace();
    constexpr float PanelWidth = 570.0f;
    constexpr float PanelPadding = 8.0f;
    constexpr int TextScale = 2;
    const float textWidth = PanelWidth - PanelPadding * 2.0f;
    const Vec2 textSize = renderer.measureWrappedText(focusDebug, textWidth, TextScale);
    const float panelHeight = std::max(34.0f, textSize.y + PanelPadding * 2.0f);
    const Vec2 panelPos{
        10.0f,
        std::max(10.0f, static_cast<float>(camera_.height()) - panelHeight - 10.0f),
    };
    renderer.fillRect(panelPos, {PanelWidth, panelHeight}, {0, 0, 0, 125});
    renderer.drawWrappedText(
        panelPos + Vec2{PanelPadding, PanelPadding},
        focusDebug,
        textWidth,
        {220, 244, 224, 255},
        TextScale);
}

} // namespace majo
