#pragma once

#include "game/Game.hpp"

#include "engine/Log.hpp"
#include "engine/Renderer.hpp"
#include "engine/Ui.hpp"
#include "game/ActorVisual.hpp"
#include "game/InventoryUiCommon.hpp"
#include "game/ItemImageRenderer.hpp"
#include "game/ObjectImageRenderer.hpp"
#include "game/ObjectVisualPose.hpp"
#include "game/WorldIconRenderer.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace majo {

inline std::mt19937& lootRuntimeRng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}


namespace {
template <typename T>
void resetInPlace(T& value)
{
    // Avoid materializing large reset temporaries on the startup stack.
    std::destroy_at(std::addressof(value));
    std::construct_at(std::addressof(value));
}

constexpr float PlayerSpriteDrawSize = 96.0f;
constexpr float PlayerSpriteAnchorX = 0.5f;
constexpr float PlayerSpriteAnchorY = 0.95f;
constexpr int BaseMenuItemCount = 8;
constexpr int BaseMiningStartChoiceCount = 3;
constexpr int BaseUpgradeItemCount = 8;
constexpr int BaseProcessingModeCount = 7;
constexpr int BaseWarehouseSourceIndex = 1;
constexpr int BaseRingSourceOffset = 2;
constexpr int BaseItemSourceCount = BaseRingSourceOffset + SpellRingCount;
constexpr int BaseProcessingSourceCount = BaseItemSourceCount;
constexpr float BaseItemSourceTabX = 44.0f;
constexpr float BaseItemSourceTabWidth = 152.0f;
constexpr float BaseItemSourceTabPitch = 165.0f;
constexpr int BaseRingWorkshopItemCount = 14;
constexpr int BookshelfMenuItemCount = 3;
constexpr int BookshelfVisibleRows = 5;
constexpr int RingWorkshopImplementedUpgradeCount = 3;
constexpr int MaxItemEnhanceLevel = 5;
constexpr int MerchantRefreshDugTileThreshold = 10;
constexpr int StorageColumns = 8;
constexpr int StorageRowsPerPane = 3;
constexpr int StoragePaneSlotCount = StorageColumns * StorageRowsPerPane;
constexpr float StorageSlotW = 72.0f;
constexpr float StorageSlotH = 64.0f;
constexpr float StorageSlotGap = 6.0f;
constexpr float StorageGridW = static_cast<float>(StorageColumns) * StorageSlotW + static_cast<float>(StorageColumns - 1) * StorageSlotGap;
constexpr float StorageLeftPaneX = 44.0f;
constexpr float StorageDetailX = 864.0f;
constexpr float StorageGridX = StorageLeftPaneX + (StorageDetailX - StorageLeftPaneX - StorageGridW) * 0.5f;
constexpr float StorageGridRightX = StorageGridX + StorageGridW;
constexpr float StorageHeaderTextX = StorageGridX;
constexpr float StorageTopGridY = 154.0f;
constexpr float StorageBackpackHeaderY = 124.0f;
constexpr float StorageBottomHeaderY = 390.0f;
constexpr float StorageBottomGridY = 430.0f;
constexpr float StorageDividerY = 368.0f;
constexpr float StorageHeaderCountGap = 8.0f;
constexpr float StorageHeaderCountYOffset = 6.0f;
constexpr int StorageHeaderCountScale = 2;
constexpr float StoragePageButtonSize = 32.0f;
constexpr float StoragePageButtonGap = 6.0f;
constexpr float StoragePageTextWidth = 48.0f;
constexpr float StoragePageTextYOffset = 7.0f;
constexpr int StoragePageTextScale = 2;
constexpr float StorageDetailY = 108.0f;
constexpr float StorageDragStartDistanceSq = 36.0f;
constexpr int PauseMenuItemCount = 5;
constexpr int GameOverItemCount = 2;
constexpr int StageClearItemCount = 1;
constexpr int RingCount = 3;
constexpr int UnlockedRingCount = SpellRingCount;
constexpr float RingAngleStep = Pi / 36.0f;
constexpr float RingDragSnapMaxDelta = Pi / 6.0f;
constexpr float RingSnapDuration = 0.14f;
constexpr float RingDetailW = 330.0f;
constexpr float DetailOuterRightMargin = 42.0f;
constexpr float DetailOuterTopMargin = 50.0f;
constexpr float DetailOuterBottomMargin = 40.0f;
constexpr float RingUiFigureEightRadius = 230.0f;
constexpr float RingUiCometRadius = 210.0f;
constexpr float RingUiThirdRingCenterYOffset = 24.0f;
constexpr float RingUiCometCenterYOffset = 104.0f;
constexpr float RingUiCometArcRotation = Pi * 1.5f;
constexpr float WarpPointSpacing = 320.0f;
constexpr float WarpPointTouchRadius = 28.0f;
constexpr float WarpPointReturnRadius = 64.0f;
constexpr float DungeonEntranceYOffset = -36.0f;
constexpr float DungeonEntranceLightRadiusTiles = 4.0f;
constexpr int DungeonEntranceReturnFocusIndex = -2;
constexpr int MaxWarpPointsPerRun = 8;
constexpr int BossWarpPointIndex = 7;
constexpr int BossOffsetTiles = 20;
constexpr float BossSpawnTriggerRadius = 48.0f;
constexpr int MaxPlayerOrbitProjectiles = 48;
constexpr int RadialSpikeProjectileCount = 8;
constexpr float CapturedProjectileMinInterval = 0.75f;
constexpr float CapturedRadialSpikeMinInterval = 1.50f;
constexpr float CapturedProjectileRetryInterval = 0.25f;
constexpr float CapturedMagnetVisualInterval = 0.45f;
constexpr float CapturedWindInterval = 1.20f;
constexpr float CapturedExplosionTileRadius = 28.0f;
constexpr int CapturedExplosionTileDamage = 1;
constexpr int CapturedExplosionEnemyDamage = 3;
constexpr float RewardPickupRadius = 24.0f;
constexpr float MoonFragmentPickupRadius = 24.0f;
constexpr int MoonFragmentMinPerWarpPoint = 1;
constexpr int MoonFragmentMaxPerWarpPoint = 3;
constexpr int RewardNodeCountPerRun = 12;
constexpr int MoneyNodeCountPerRun = 18;
constexpr int ChestNodeCountPerRun = 18;
constexpr int CrateNodeCountPerRun = 30;
constexpr float ChestInteractRadius = 34.0f;
constexpr float ChestHitRadius = 20.0f;
constexpr Vec2 ChestCollisionSize{34.0f, 24.0f};
constexpr float ChestHorizontalStretchSeconds = 0.10f;
constexpr float ChestVerticalStretchSeconds = 0.16f;
constexpr float ChestReturnSeconds = 0.12f;
constexpr float ChestOpenVisualSeconds = ChestHorizontalStretchSeconds + 0.08f;
constexpr float ChestLootReleaseSeconds = ChestHorizontalStretchSeconds + ChestVerticalStretchSeconds + ChestReturnSeconds;
constexpr float CrateHitRadius = 18.0f;
constexpr Vec2 CrateCollisionSize{36.0f, 30.0f};
constexpr Color CrateBreakParticleColor{132, 88, 48, 255};
constexpr int EnemyNodeCountPerRun = 7;
constexpr float ExposedEnemyNodeSpawnPadding = static_cast<float>(balance::TileSize);
constexpr float TopInfoBarX = 0.0f;
constexpr float TopInfoBarY = 0.0f;
constexpr float TopInfoBarHeight = 42.0f;
constexpr float TopInfoBarPaddingX = 14.0f;
constexpr float TopInfoBarGroupGap = 22.0f;
constexpr float TopInfoBarIconSize = 36.0f;
constexpr float TopInfoBarIconTextGap = 7.0f;
constexpr float DungeonStatusHudWidth = 206.0f;
constexpr float DungeonStatusHudHeight = 134.0f;
constexpr float DungeonStatusHudRightMargin = 18.0f;
constexpr float DungeonStatusHudBottomMargin = 24.0f;
constexpr float DungeonStatusHudPadding = 14.0f;
constexpr float DungeonStatusHudBarHeight = 8.0f;
constexpr float RingStatusHudWidth = 206.0f;
constexpr float RingStatusHudHeight = 82.0f;
constexpr float RingStatusHudLeftMargin = 18.0f;
constexpr float RingStatusHudBottomMargin = 24.0f;
constexpr float RingStatusHudGap = 8.0f;
constexpr float RingStatusHudPadding = 12.0f;
constexpr float DungeonLogWidth = 360.0f;
constexpr float DungeonLogRowHeight = 30.0f;
constexpr float DungeonLogGap = 6.0f;
constexpr float DungeonLogRightMargin = 18.0f;
constexpr float DungeonLogTargetYRatio = 0.48f;
constexpr float DungeonLogStatusGap = 12.0f;
constexpr int DungeonLogMaxVisible = 6;
constexpr float DungeonLogLifetime = 3.4f;
constexpr float DungeonLogFadeStart = 2.5f;
constexpr float DungeonLogMergeSeconds = 0.8f;
constexpr int BaseEditGridSize = 12;
constexpr float BaseEditFacilityMinSize = 24.0f;
constexpr int BaseEditUndoLimit = 100;
constexpr float BaseEditHandleSize = 12.0f;
constexpr float BaseEditHandleHitPadding = 8.0f;
constexpr float RingObjectImageMaxSize = 48.0f;
constexpr float RingItemBaseAltitude = 8.0f;
constexpr float RingItemBobAmplitude = 3.0f;
constexpr float RingItemBobSpeed = 4.2f;
constexpr float RingItemRotationWobbleDegrees = 4.0f;
constexpr float ObjectImageScaleMin = 0.25f;
constexpr float ObjectImageScaleMax = 4.0f;
constexpr float ObjectImageScaleStep = 0.05f;
constexpr float ObjectImageScaleCardWidth = 168.0f;
constexpr float ObjectImageScaleCardHeight = 132.0f;
constexpr float ObjectImageScaleCardGap = 10.0f;
constexpr float ObjectImageScalePanelMargin = 20.0f;
constexpr float ObjectImageScaleHeaderHeight = 94.0f;
constexpr float ObjectImageScaleFooterHeight = 60.0f;
constexpr float ObjectImageScalePreviewSize = 48.0f;
constexpr int EnemyTestVisibleRows = 8;
constexpr float ScreenTransitionFadeOutSeconds = 0.45f;
constexpr float ScreenTransitionBlackHoldSeconds = 0.08f;
constexpr float ScreenTransitionFadeInSeconds = 0.45f;
constexpr float ScreenTransitionCrossFadeSeconds = 0.35f;
constexpr float FootstepDustLifetime = 0.30f;
constexpr float FootstepDustStartOffset = 8.0f;
constexpr float FootstepDustEndOffset = 20.0f;
constexpr unsigned char FootstepDustBaseAlpha = 255;
constexpr int FootstepDustCircleCapacity = 5;

struct TopInfoMaterial {
    MaterialType type = MaterialType::OldWoodBuildingMaterial;
};

struct FootstepDustShape {
    int circleCount = 0;
    std::array<Vec2, FootstepDustCircleCapacity> offsets{};
    std::array<float, FootstepDustCircleCapacity> radii{};
};

constexpr std::array<FootstepDustShape, 10> FootstepDustShapes{{
    {4, {{{-6.0f, -1.0f}, {-1.0f, 2.0f}, {5.0f, 0.0f}, {1.0f, -4.0f}, {0.0f, 0.0f}}}, {{4.8f, 5.8f, 4.2f, 3.5f, 0.0f}}},
    {5, {{{-7.0f, 1.0f}, {-2.0f, -3.0f}, {3.0f, 2.0f}, {8.0f, -1.0f}, {1.0f, 4.0f}}}, {{4.2f, 3.8f, 5.6f, 3.4f, 3.0f}}},
    {4, {{{-5.0f, 3.0f}, {0.0f, -2.0f}, {6.0f, 1.0f}, {2.0f, 5.0f}, {0.0f, 0.0f}}}, {{5.4f, 4.8f, 4.0f, 3.2f, 0.0f}}},
    {5, {{{-8.0f, -2.0f}, {-3.0f, 3.0f}, {2.0f, 0.0f}, {7.0f, 2.0f}, {3.0f, -4.0f}}}, {{3.6f, 5.2f, 4.6f, 3.8f, 3.0f}}},
    {4, {{{-6.0f, 0.0f}, {-1.0f, -4.0f}, {4.0f, 3.0f}, {8.0f, -1.0f}, {0.0f, 0.0f}}}, {{4.0f, 3.4f, 5.4f, 3.6f, 0.0f}}},
    {5, {{{-7.0f, 2.0f}, {-2.0f, 0.0f}, {2.0f, -4.0f}, {6.0f, 2.0f}, {0.0f, 5.0f}}}, {{4.4f, 5.8f, 3.2f, 4.0f, 3.2f}}},
    {4, {{{-5.0f, -3.0f}, {1.0f, 1.0f}, {6.0f, -1.0f}, {-1.0f, 5.0f}, {0.0f, 0.0f}}}, {{3.8f, 5.6f, 4.4f, 3.4f, 0.0f}}},
    {5, {{{-8.0f, 1.0f}, {-3.0f, -2.0f}, {1.0f, 3.0f}, {5.0f, -3.0f}, {8.0f, 1.0f}}}, {{3.6f, 4.8f, 5.4f, 3.4f, 3.0f}}},
    {4, {{{-6.0f, 4.0f}, {-2.0f, -1.0f}, {3.0f, 2.0f}, {7.0f, -2.0f}, {0.0f, 0.0f}}}, {{3.4f, 5.2f, 4.8f, 3.8f, 0.0f}}},
    {5, {{{-7.0f, -1.0f}, {-1.0f, 3.0f}, {3.0f, -2.0f}, {7.0f, 2.0f}, {1.0f, -5.0f}}}, {{4.2f, 5.0f, 4.0f, 3.6f, 3.0f}}},
}};

float flickeredLightRadius(float radius, float totalSeconds, float phase)
{
    if (radius <= 0.0f) {
        return radius;
    }
    const float waveA = std::sin(totalSeconds * 13.4f + phase);
    const float waveB = std::sin(totalSeconds * 34.7f + phase * 2.3f + 0.6f);
    const float offsetPx = std::clamp(waveA * 1.35f + waveB * 0.95f, -2.0f, 2.0f);
    return std::max(0.0f, radius + offsetPx);
}

Vec2 flickeredLightPosition(Vec2 position, float totalSeconds, float phase)
{
    const float x =
        std::sin(totalSeconds * 7.1f + phase * 1.37f) * 1.15f +
        std::sin(totalSeconds * 17.3f + phase * 2.11f + 1.2f) * 0.55f;
    const float y =
        std::sin(totalSeconds * 6.4f + phase * 1.83f + 2.0f) * 1.10f +
        std::sin(totalSeconds * 19.1f + phase * 2.47f + 0.4f) * 0.60f;
    return position + Vec2{
        std::clamp(x, -1.7f, 1.7f),
        std::clamp(y, -1.7f, 1.7f),
    };
}

float smoothStep01(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

std::string popUtf8Codepoint(std::string text)
{
    if (text.empty()) {
        return text;
    }
    text.pop_back();
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xc0U) == 0x80U) {
        text.pop_back();
    }
    return text;
}

Vec2 playerSpriteFootAnchor(Vec2 actorCenter)
{
    return actorCenter + Vec2{0.0f, PlayerSpriteDrawSize * (PlayerSpriteAnchorY - 0.5f)};
}

std::string fittedSingleLineText(Renderer& renderer, std::string text, float maxWidth, int scale)
{
    if (maxWidth <= 0.0f) {
        return "";
    }
    if (renderer.measureText(text, scale).x <= maxWidth) {
        return text;
    }

    constexpr std::string_view Ellipsis = "...";
    while (!text.empty()) {
        text = popUtf8Codepoint(std::move(text));
        std::string candidate = text + std::string(Ellipsis);
        if (renderer.measureText(candidate, scale).x <= maxWidth) {
            return candidate;
        }
    }
    return renderer.measureText(Ellipsis, scale).x <= maxWidth ? std::string(Ellipsis) : "";
}

std::string lowerAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string filenameOf(const std::string& path)
{
    return lowerAscii(std::filesystem::path(path).filename().string());
}

std::string trimAscii(std::string text)
{
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
    text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
    return text;
}

enum class BaseFacilityAction {
    MineExit,
    Storage,
    Bookshelf,
    Merchant,
    Processing,
    Forge,
    Diary,
    RingWorkshop,
    HomeEntrance,
    HomeExit,
    MonicaTalk,
};

struct BaseFacility {
    const char* facilityId = "";
    const char* displayName = "";
    UiRect rect{};
    float interactionRange = 56.0f;
    bool enabled = true;
    bool unlocked = true;
    BaseFacilityAction onInteract = BaseFacilityAction::MineExit;
};

Vec2 tileWorldCenter(DungeonTile tile)
{
    return {
        (static_cast<float>(tile.x) + 0.5f) * static_cast<float>(balance::TileSize),
        (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(balance::TileSize),
    };
}

DungeonTile roundDungeonTile(Vec2 tilePosition)
{
    return {
        static_cast<int>(std::round(tilePosition.x)),
        static_cast<int>(std::round(tilePosition.y)),
    };
}

Vec2 perpendicular(Vec2 value)
{
    return {-value.y, value.x};
}

std::string_view particleElementForProjectile(std::string_view projectileId)
{
    if (projectileId.find("fire") != std::string_view::npos || projectileId.find("explosion") != std::string_view::npos) {
        return "fire";
    }
    if (projectileId.find("water") != std::string_view::npos || projectileId.find("poison") != std::string_view::npos) {
        return "ice";
    }
    if (projectileId.find("wind") != std::string_view::npos) {
        return "wind";
    }
    if (projectileId.find("stone") != std::string_view::npos || projectileId.find("cactus") != std::string_view::npos) {
        return "earth";
    }
    return "magic";
}

Vec2 pointAtPathProgress(const std::vector<Vec2>& points, float progress)
{
    if (points.empty()) {
        return {};
    }
    if (points.size() == 1) {
        return points.front();
    }

    float totalLength = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        totalLength += length(points[i] - points[i - 1]);
    }
    if (totalLength <= 0.0001f) {
        return points.front();
    }

    const float target = totalLength * clamp(progress, 0.0f, 1.0f);
    float traveled = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const float segmentLength = length(points[i] - points[i - 1]);
        if (traveled + segmentLength >= target) {
            const float t = segmentLength > 0.0001f ? (target - traveled) / segmentLength : 0.0f;
            return lerp(points[i - 1], points[i], t);
        }
        traveled += segmentLength;
    }
    return points.back();
}

Vec2 tangentAtPathProgress(const std::vector<Vec2>& points, float progress)
{
    if (points.size() < 2) {
        return {1.0f, 0.0f};
    }

    const std::size_t index = std::min(
        points.size() - 2,
        static_cast<std::size_t>(clamp(progress, 0.0f, 1.0f) * static_cast<float>(points.size() - 1)));
    return normalize(points[index + 1] - points[index]);
}

std::optional<std::string> firstAvailableObjectId(const ObjectCatalog& catalog)
{
    for (const ObjectDefinition& object : catalog.objects) {
        if (!object.id.empty()) {
            return object.id;
        }
    }
    return std::nullopt;
}

std::string joinEffectLines(const std::vector<std::string>& lines)
{
    if (lines.empty()) {
        return "-";
    }
    if (lines.size() == 1 && lines.front() == "\xE3\x81\xAA\xE3\x81\x97") {
        return lines.front();
    }
    std::string text;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (!text.empty()) {
            text += '\n';
        }
        text += "\xE3\x83\xBB";
        text += lines[i];
    }
    return text;
}

const char* chestKindCode(LootChestKind kind)
{
    switch (kind) {
    case LootChestKind::Common: return "C";
    case LootChestKind::Rare: return "R";
    case LootChestKind::SuperRare: return "S";
    }
    return "C";
}

LootChestKind rollChestKind(std::mt19937& rng, float pathProgress)
{
    const float progress = clamp(pathProgress, 0.0f, 1.0f);
    const int rareWeight = 18 + static_cast<int>(progress * 10.0f);
    const int superRareWeight = 4 + static_cast<int>(progress * 4.0f);
    const int commonWeight = std::max(1, 100 - rareWeight - superRareWeight);
    std::discrete_distribution<int> distribution({
        static_cast<double>(commonWeight),
        static_cast<double>(rareWeight),
        static_cast<double>(superRareWeight),
    });
    const int roll = distribution(rng);
    if (roll == 1) {
        return LootChestKind::Rare;
    }
    if (roll == 2) {
        return LootChestKind::SuperRare;
    }
    return LootChestKind::Common;
}

int lootMaxDepthForStage(std::string_view stageId)
{
    return stageId == std::string_view("stage_04_astral_mine") ? 9 : 3;
}

int lootDepthRankForProgress(std::string_view stageId, float pathProgress)
{
    const int maxDepth = lootMaxDepthForStage(stageId);
    const float progress = clamp(pathProgress, 0.0f, 0.9999f);
    return std::clamp(static_cast<int>(progress * static_cast<float>(maxDepth)) + 1, 1, maxDepth);
}

int lootDepthRankForWorldPosition(
    const TileMap& tileMap,
    const DungeonLayout& dungeonLayout,
    std::string_view stageId,
    Vec2 position)
{
    const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout, {
        static_cast<float>(tileMap.worldToTile(position.x)),
        static_cast<float>(tileMap.worldToTile(position.y)),
    });
    return lootDepthRankForProgress(stageId, metrics.pathProgress);
}

float lootStageMultiplier(const RuntimeBalance& balance, std::string_view stageId)
{
    if (stageId == std::string_view("stage_02_junk_magic")) {
        return balance.lootStageMultiplierJK;
    }
    if (stageId == std::string_view("stage_03_star_core")) {
        return balance.lootStageMultiplierSC;
    }
    if (stageId == std::string_view("stage_04_astral_mine")) {
        return balance.lootStageMultiplierAS;
    }
    return balance.lootStageMultiplierST;
}

float lootDepthMultiplier(const RuntimeBalance& balance, std::string_view stageId, int depthRank)
{
    if (stageId == std::string_view("stage_04_astral_mine")) {
        const int index = std::clamp(depthRank, 1, 9) - 1;
        return balance.lootDepthMultiplierAS[static_cast<std::size_t>(index)];
    }
    const int index = std::clamp(depthRank, 1, 3) - 1;
    return balance.lootDepthMultiplier[static_cast<std::size_t>(index)];
}

float lootGradeMultiplier(const RuntimeBalance& balance, LootChestKind chestKind)
{
    switch (chestKind) {
    case LootChestKind::Common:
        return balance.lootGradeMultiplierC;
    case LootChestKind::Rare:
        return balance.lootGradeMultiplierR;
    case LootChestKind::SuperRare:
        return balance.lootGradeMultiplierS;
    }
    return balance.lootGradeMultiplierC;
}

int lootItemRollCount(LootChestKind kind, std::mt19937& rng)
{
    switch (kind) {
    case LootChestKind::Common: {
        std::uniform_int_distribution<int> distribution(1, 2);
        return distribution(rng);
    }
    case LootChestKind::Rare: {
        std::uniform_int_distribution<int> distribution(1, 3);
        return distribution(rng);
    }
    case LootChestKind::SuperRare: {
        std::uniform_int_distribution<int> distribution(2, 3);
        return distribution(rng);
    }
    }
    return 1;
}

std::pair<int, int> lootMoneyBaseRange(LootChestKind kind)
{
    switch (kind) {
    case LootChestKind::Common: return {20, 60};
    case LootChestKind::Rare: return {60, 160};
    case LootChestKind::SuperRare: return {150, 400};
    }
    return {20, 60};
}

int scaledLootAmount(int baseAmount, float multiplier)
{
    return std::max(1, static_cast<int>(std::ceil(static_cast<float>(std::max(1, baseAmount)) * std::max(0.0f, multiplier))));
}

bool rollChance(float chance, std::mt19937& rng)
{
    if (chance <= 0.0f) {
        return false;
    }
    if (chance >= 1.0f) {
        return true;
    }
    std::bernoulli_distribution distribution(chance);
    return distribution(rng);
}

bool digEventDue(int& counter, int minDugTiles, int guaranteeDugTiles, std::mt19937& rng)
{
    ++counter;
    const int minTiles = std::max(1, minDugTiles);
    const int guaranteeTiles = std::max(minTiles, guaranteeDugTiles);
    if (counter < minTiles) {
        return false;
    }
    if (counter >= guaranteeTiles) {
        return true;
    }

    const int randomWindow = std::max(1, guaranteeTiles - minTiles + 1);
    std::uniform_int_distribution<int> distribution(1, randomWindow);
    return distribution(rng) == 1;
}

MaterialType rollChestMaterial(std::mt19937& rng)
{
    constexpr std::array<MaterialType, 3> Materials{
        MaterialType::OldWoodBuildingMaterial,
        MaterialType::EnhancementOre,
        MaterialType::ManaDrop,
    };
    std::uniform_int_distribution<int> distribution(0, static_cast<int>(Materials.size()) - 1);
    return Materials[static_cast<std::size_t>(distribution(rng))];
}

Vec2 scatterLootPosition(Vec2 center, std::mt19937& rng)
{
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> radiusDistribution(10.0f, 30.0f);
    return center + fromAngle(angleDistribution(rng)) * radiusDistribution(rng);
}

WorldDropSpawnMotion makeWorldLootJumpMotion(Vec2 center, std::mt19937& rng)
{
    std::uniform_real_distribution<float> durationDistribution(0.34f, 0.48f);
    std::uniform_real_distribution<float> heightDistribution(24.0f, 40.0f);
    const float duration = durationDistribution(rng);
    return {
        .jump = true,
        .startPosition = center,
        .jumpDurationSeconds = duration,
        .jumpArcHeight = heightDistribution(rng),
        .pickupDelaySeconds = duration * 0.75f,
    };
}

float smooth01(float value)
{
    const float t = clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Vec2 chestOpeningScale(float openingSeconds)
{
    if (openingSeconds <= 0.0f || openingSeconds >= ChestLootReleaseSeconds) {
        return {1.0f, 1.0f};
    }

    if (openingSeconds < ChestHorizontalStretchSeconds) {
        const float t = smooth01(openingSeconds / ChestHorizontalStretchSeconds);
        return {
            lerp(1.0f, 1.24f, t),
            lerp(1.0f, 0.82f, t),
        };
    }

    if (openingSeconds < ChestHorizontalStretchSeconds + ChestVerticalStretchSeconds) {
        const float t = smooth01((openingSeconds - ChestHorizontalStretchSeconds) / ChestVerticalStretchSeconds);
        return {
            lerp(1.24f, 0.92f, t),
            lerp(0.82f, 1.30f, t),
        };
    }

    const float t = smooth01(
        (openingSeconds - ChestHorizontalStretchSeconds - ChestVerticalStretchSeconds) / ChestReturnSeconds);
    return {
        lerp(0.92f, 1.0f, t),
        lerp(1.30f, 1.0f, t),
    };
}

bool chestVisualOpened(bool opened, bool lootSpawned, float openingSeconds)
{
    return opened && (lootSpawned || openingSeconds >= ChestOpenVisualSeconds);
}

Color chestFillColor(LootChestKind kind, bool opened)
{
    if (opened) {
        return {72, 58, 44, 255};
    }
    switch (kind) {
    case LootChestKind::Common:
        return {146, 88, 42, 255};
    case LootChestKind::Rare:
        return {64, 116, 210, 255};
    case LootChestKind::SuperRare:
        return {238, 190, 54, 255};
    }
    return {146, 88, 42, 255};
}

Color chestOutlineColor(LootChestKind kind, bool opened)
{
    if (opened) {
        return {126, 106, 78, 210};
    }
    switch (kind) {
    case LootChestKind::Common:
        return {226, 170, 94, 255};
    case LootChestKind::Rare:
        return {214, 226, 238, 255};
    case LootChestKind::SuperRare:
        return {255, 118, 210, 255};
    }
    return {226, 170, 94, 255};
}

UiRect baseMapBounds()
{
    return {{0.0f, 0.0f}, {1280.0f, 720.0f}};
}

UiRect baseEditModeButtonRect(int index)
{
    return {{898.0f, 52.0f + static_cast<float>(index) * 42.0f}, {174.0f, 34.0f}};
}

UiRect baseEditSaveButtonRect()
{
    return {{1080.0f, 52.0f}, {150.0f, 34.0f}};
}

std::filesystem::path baseEditDataPath(BaseArea area)
{
    const char* fileName = area == BaseArea::Outdoor ? "base_edit_outdoor.cfg" : "base_edit_home.cfg";
    return std::filesystem::path("data") / fileName;
}

std::filesystem::path openingKamishibaiDataPath()
{
    return std::filesystem::path("data") / "opening_kamishibai.tsv";
}

std::filesystem::path endingKamishibaiDataPath()
{
    return std::filesystem::path("data") / "ending_kamishibai.tsv";
}

std::filesystem::path storyEventDataDirectory()
{
    return std::filesystem::path("data") / "story_events";
}

std::string openingTitleImagePath(const std::vector<KamishibaiPage>& pages)
{
    for (auto it = pages.rbegin(); it != pages.rend(); ++it) {
        if (it->effect == KamishibaiEffect::TitleFade && !it->imagePath.empty()) {
            return it->imagePath;
        }
    }
    for (auto it = pages.rbegin(); it != pages.rend(); ++it) {
        if (!it->imagePath.empty()) {
            return it->imagePath;
        }
    }
    return "assets/opening/op_8.png";
}

std::filesystem::path objectImageScaleDataPath()
{
    return std::filesystem::path("data") / "object_image_scale.cfg";
}

float clampObjectImageScale(float scale)
{
    return clamp(scale, ObjectImageScaleMin, ObjectImageScaleMax);
}

float snapObjectImageScale(float scale)
{
    const float steps = std::round(scale / ObjectImageScaleStep);
    return clampObjectImageScale(steps * ObjectImageScaleStep);
}

struct ObjectImageScaleLayout {
    UiRect viewport{};
    UiRect content{};
    int columns = 1;
    float rowHeight = 1.0f;
};

ObjectImageScaleLayout makeObjectImageScaleLayout(int screenWidth, int screenHeight)
{
    ObjectImageScaleLayout layout;
    const float width = static_cast<float>(std::max(1, screenWidth));
    const float height = static_cast<float>(std::max(1, screenHeight));
    layout.viewport.pos = {ObjectImageScalePanelMargin, ObjectImageScaleHeaderHeight};
    layout.viewport.size = {
        std::max(10.0f, width - ObjectImageScalePanelMargin * 2.0f),
        std::max(10.0f, height - ObjectImageScaleHeaderHeight - ObjectImageScaleFooterHeight - ObjectImageScalePanelMargin),
    };
    layout.content = layout.viewport;
    const float widthWithGap = ObjectImageScaleCardWidth + ObjectImageScaleCardGap;
    layout.columns = std::max(1, static_cast<int>((layout.viewport.size.x + ObjectImageScaleCardGap) / widthWithGap));
    layout.rowHeight = ObjectImageScaleCardHeight + ObjectImageScaleCardGap;
    return layout;
}

UiRect objectImageScaleCardRect(const ObjectImageScaleLayout& layout, int index, float scrollOffset)
{
    const int safeColumns = std::max(1, layout.columns);
    const int row = index / safeColumns;
    const int column = index % safeColumns;
    const float x = layout.content.pos.x + static_cast<float>(column) * (ObjectImageScaleCardWidth + ObjectImageScaleCardGap);
    const float y = layout.content.pos.y + static_cast<float>(row) * layout.rowHeight - scrollOffset;
    return {{x, y}, {ObjectImageScaleCardWidth, ObjectImageScaleCardHeight}};
}

UiRect objectImageScaleTabRect(int index)
{
    return {{420.0f + static_cast<float>(index) * 118.0f, 16.0f}, {108.0f, 32.0f}};
}

UiRect enemyTestToolbarRect()
{
    return {{18.0f, 52.0f}, {1016.0f, 86.0f}};
}

UiRect enemyTestRestoreButtonRect()
{
    return {{18.0f, 52.0f}, {124.0f, 38.0f}};
}

UiRect enemyTestSelectButtonRect()
{
    const UiRect panel = enemyTestToolbarRect();
    return {{panel.pos.x + 108.0f, panel.pos.y + 18.0f}, {342.0f, ui::ButtonHeight}};
}

UiRect enemyTestSummonButtonRect()
{
    const UiRect select = enemyTestSelectButtonRect();
    return {{select.pos.x + select.size.x + 12.0f, select.pos.y}, {124.0f, ui::ButtonHeight}};
}

UiRect enemyTestClearButtonRect()
{
    const UiRect summon = enemyTestSummonButtonRect();
    return {{summon.pos.x + summon.size.x + 12.0f, summon.pos.y}, {124.0f, ui::ButtonHeight}};
}

UiRect enemyTestHideButtonRect()
{
    const UiRect clear = enemyTestClearButtonRect();
    return {{clear.pos.x + clear.size.x + 12.0f, clear.pos.y}, {144.0f, ui::ButtonHeight}};
}

UiRect enemyTestExitButtonRect()
{
    const UiRect hide = enemyTestHideButtonRect();
    return {{hide.pos.x + hide.size.x + 12.0f, hide.pos.y}, {136.0f, ui::ButtonHeight}};
}

UiDropdownStyle enemyTestDropdownStyle()
{
    UiDropdownStyle style;
    style.visibleRows = EnemyTestVisibleRows;
    style.rowHeight = 38.0f;
    style.fill = {12, 18, 34, 238};
    style.fillHot = {38, 52, 82, 248};
    style.outline = {255, 255, 255, 220};
    style.emptyLabel = "敵データがありません";
    return style;
}

std::string enemyTestEnemyLabel(const EnemyDefinition& enemy)
{
    return (enemy.name.empty() ? enemy.id : enemy.name) + " (" + enemy.id + ")";
}

std::string enemyTestDropdownItemLabel(const EnemyDefinition& enemy, int index)
{
    char prefix[16];
    std::snprintf(prefix, sizeof(prefix), "%03d  ", index + 1);
    return std::string(prefix) + (enemy.name.empty() ? enemy.id : enemy.name) + "  (" + enemy.id + ")";
}

std::int64_t packBaseEditTile(int tileX, int tileY)
{
    return (static_cast<std::int64_t>(tileX) << 32) ^ static_cast<std::uint32_t>(tileY);
}

int baseEditTileXFromPacked(std::int64_t packed)
{
    return static_cast<int>(packed >> 32);
}

int baseEditTileYFromPacked(std::int64_t packed)
{
    return static_cast<int>(packed & 0xffffffffLL);
}

BaseEditRect toBaseEditRect(UiRect rect)
{
    return {rect.pos.x, rect.pos.y, rect.size.x, rect.size.y};
}

UiRect toUiRect(BaseEditRect rect)
{
    return {{rect.x, rect.y}, {rect.w, rect.h}};
}

bool sameBaseEditRect(BaseEditRect a, BaseEditRect b)
{
    return std::abs(a.x - b.x) <= 0.001f &&
        std::abs(a.y - b.y) <= 0.001f &&
        std::abs(a.w - b.w) <= 0.001f &&
        std::abs(a.h - b.h) <= 0.001f;
}

BaseEditRect normalizeBaseEditRect(BaseEditRect rect)
{
    const UiRect bounds = baseMapBounds();
    const float minSize = BaseEditFacilityMinSize;
    rect.w = std::max(minSize, rect.w);
    rect.h = std::max(minSize, rect.h);
    rect.x = clamp(rect.x, bounds.pos.x, bounds.pos.x + bounds.size.x - rect.w);
    rect.y = clamp(rect.y, bounds.pos.y, bounds.pos.y + bounds.size.y - rect.h);
    return rect;
}

bool pointInExpandedRect(Vec2 point, UiRect rect, float padding)
{
    const UiRect expanded{
        {rect.pos.x - padding, rect.pos.y - padding},
        {rect.size.x + padding * 2.0f, rect.size.y + padding * 2.0f},
    };
    return expanded.contains(point);
}

int baseEditResizeMaskAtPoint(UiRect rect, Vec2 point)
{
    constexpr int Left = 1;
    constexpr int Right = 2;
    constexpr int Top = 4;
    constexpr int Bottom = 8;

    int mask = 0;
    const float left = rect.pos.x;
    const float right = rect.pos.x + rect.size.x;
    const float top = rect.pos.y;
    const float bottom = rect.pos.y + rect.size.y;

    const UiRect leftRect{{left - BaseEditHandleHitPadding, top}, {BaseEditHandleHitPadding * 2.0f, rect.size.y}};
    const UiRect rightRect{{right - BaseEditHandleHitPadding, top}, {BaseEditHandleHitPadding * 2.0f, rect.size.y}};
    const UiRect topRect{{left, top - BaseEditHandleHitPadding}, {rect.size.x, BaseEditHandleHitPadding * 2.0f}};
    const UiRect bottomRect{{left, bottom - BaseEditHandleHitPadding}, {rect.size.x, BaseEditHandleHitPadding * 2.0f}};

    if (leftRect.contains(point)) {
        mask |= Left;
    }
    if (rightRect.contains(point)) {
        mask |= Right;
    }
    if (topRect.contains(point)) {
        mask |= Top;
    }
    if (bottomRect.contains(point)) {
        mask |= Bottom;
    }
    return mask;
}

float distanceToRect(Vec2 point, UiRect rect)
{
    const float closestX = clamp(point.x, rect.pos.x, rect.pos.x + rect.size.x);
    const float closestY = clamp(point.y, rect.pos.y, rect.pos.y + rect.size.y);
    return length(point - Vec2{closestX, closestY});
}

bool circleIntersectsRect(Vec2 center, float radius, UiRect rect)
{
    return distanceToRect(center, rect) < radius;
}

const char* screenModeName(ScreenMode mode)
{
    switch (mode) {
    case ScreenMode::OpeningKamishibai: return "OpeningKamishibai";
    case ScreenMode::EndingKamishibai: return "EndingKamishibai";
    case ScreenMode::Title: return "Title";
    case ScreenMode::Base: return "Base";
    case ScreenMode::WorldLoading: return "WorldLoading";
    case ScreenMode::Playing: return "Playing";
    case ScreenMode::PauseMenu: return "PauseMenu";
    case ScreenMode::Inventory: return "Inventory";
    case ScreenMode::Ring: return "Ring";
    case ScreenMode::ObjectImageScaleEdit: return "ObjectImageScaleEdit";
    case ScreenMode::AudioCueEdit: return "AudioCueEdit";
    case ScreenMode::LevelUp: return "LevelUp";
    case ScreenMode::GameOver: return "GameOver";
    case ScreenMode::StageClear: return "StageClear";
    case ScreenMode::AstralResult: return "AstralResult";
    }
    return "Unknown";
}

const char* baseAreaName(BaseArea area)
{
    switch (area) {
    case BaseArea::Outdoor: return "屋外拠点";
    case BaseArea::HomeInterior: return "ルネの家";
    }
    return "拠点";
}

std::vector<BaseFacility> baseFacilities(BaseArea area, bool ringWorkshopUnlocked)
{
    if (area == BaseArea::HomeInterior) {
        return {
            BaseFacility{"bookshelf", "本棚", {{150.0f, 128.0f}, {128.0f, 66.0f}}, 72.0f, true, true, BaseFacilityAction::Bookshelf},
            BaseFacility{"diary", "日記", {{940.0f, 174.0f}, {88.0f, 58.0f}}, 64.0f, true, true, BaseFacilityAction::Diary},
            BaseFacility{"home_exit", "屋外へ戻る出口", {{592.0f, 584.0f}, {96.0f, 42.0f}}, 80.0f, true, true, BaseFacilityAction::HomeExit},
            BaseFacility{"bed", "ベッド", {{156.0f, 452.0f}, {172.0f, 86.0f}}, 0.0f, false, true, BaseFacilityAction::Bookshelf},
            BaseFacility{"desk", "机", {{898.0f, 360.0f}, {150.0f, 76.0f}}, 0.0f, false, true, BaseFacilityAction::Diary},
        };
    }

    return {
        BaseFacility{"mine_exit", "ダンジョン入口", {{584.0f, 560.0f}, {112.0f, 64.0f}}, 78.0f, true, true, BaseFacilityAction::MineExit},
        BaseFacility{"storage_chest", "収納箱", {{158.0f, 320.0f}, {98.0f, 72.0f}}, 68.0f, true, true, BaseFacilityAction::Storage},
        BaseFacility{"merchant_wagon", "商人ワゴン", {{982.0f, 302.0f}, {150.0f, 90.0f}}, 78.0f, true, true, BaseFacilityAction::Merchant},
        BaseFacility{"processing_table", "作業台", {{930.0f, 128.0f}, {130.0f, 68.0f}}, 70.0f, true, true, BaseFacilityAction::Processing},
        BaseFacility{"upgrade_forge", "拠点強化炉", {{568.0f, 76.0f}, {144.0f, 74.0f}}, 76.0f, true, true, BaseFacilityAction::Forge},
        BaseFacility{"ring_workshop", "リング工房用スペース", {{902.0f, 520.0f}, {172.0f, 92.0f}}, 82.0f, true, ringWorkshopUnlocked, BaseFacilityAction::RingWorkshop},
        BaseFacility{"monica", "モニカ", {{760.0f, 230.0f}, {74.0f, 86.0f}}, 72.0f, true, true, BaseFacilityAction::MonicaTalk},
        BaseFacility{"home_entrance", "ルネの家の入口", {{382.0f, 158.0f}, {52.0f, 32.0f}}, 64.0f, true, true, BaseFacilityAction::HomeEntrance},
        BaseFacility{"home", "ルネの家", {{330.0f, 72.0f}, {154.0f, 100.0f}}, 0.0f, false, true, BaseFacilityAction::HomeEntrance},
    };
}

bool baseInteractionAvailable(Vec2 playerPosition, const BaseFacility& facility)
{
    return facility.enabled && distanceToRect(playerPosition, facility.rect) <= facility.interactionRange;
}

const char* baseInteractionPrompt(const BaseFacility& facility)
{
    switch (facility.onInteract) {
    case BaseFacilityAction::MonicaTalk:
        return "Enter: モニカと話す / クリック: 近くのNPCと話す / Esc: メニュー";
    case BaseFacilityAction::HomeEntrance:
        return "Enter: ルネの家に入る / クリック: 近くの入口を調べる / Esc: メニュー";
    case BaseFacilityAction::HomeExit:
        return "Enter: 屋外へ戻る / クリック: 近くの出口を調べる / Esc: メニュー";
    default:
        return nullptr;
    }
}

DialogueSequence baseMonicaDialogue()
{
    DialogueSequence sequence;
    sequence.id = "base_monica_default";
    sequence.lines.push_back(DialogueLine{
        "monica",
        "モニカ",
        "",
        "無理はしないで。準備ができたら、ダンジョン入口から向かおう。",
    });
    sequence.lines.push_back(DialogueLine{
        "monica",
        "モニカ",
        "",
        "チコリが反応したら、近くに星くずか古い魔導具があるかも。壁の色も見てみて。",
    });
    return sequence;
}

UiRect basePanelRect()
{
    return {{360.0f, 92.0f}, {560.0f, 536.0f}};
}

UiRect baseUpgradePanelRect()
{
    return {{220.0f, 42.0f}, {840.0f, 628.0f}};
}

UiRect baseResultDialogRect()
{
    return {{410.0f, 200.0f}, {460.0f, 260.0f}};
}

UiRect levelUpResultDialogRect()
{
    return baseResultDialogRect();
}

UiRect baseMenuItemRect(int index)
{
    return {{450.0f, 296.0f + static_cast<float>(index) * 36.0f}, {380.0f, 30.0f}};
}

float baseMiningContentLeft()
{
    return basePanelRect().pos.x + ui::ImageWindowHeaderTitlePadding.x;
}

struct UiPageSelectorRects {
    UiRect prev{};
    UiRect text{};
    UiRect next{};
};

UiPageSelectorRects uiPageSelectorRectsFromNextButton(Vec2 nextButtonPos, float textWidth, float buttonSize = StoragePageButtonSize, float gap = StoragePageButtonGap)
{
    const Vec2 buttonSizeVec{buttonSize, buttonSize};
    UiPageSelectorRects rects{};
    rects.next = {nextButtonPos, buttonSizeVec};
    rects.text = {{
        rects.next.pos.x - gap - textWidth,
        rects.next.pos.y,
    }, {textWidth, buttonSize}};
    rects.prev = {{
        rects.text.pos.x - gap - buttonSize,
        rects.text.pos.y,
    }, buttonSizeVec};
    return rects;
}

UiPageSelectorRects uiPageSelectorRectsCentered(Vec2 groupCenter, float textWidth, float buttonSize = StoragePageButtonSize, float gap = StoragePageButtonGap)
{
    const float groupWidth = buttonSize * 2.0f + gap * 2.0f + textWidth;
    const float left = groupCenter.x - groupWidth * 0.5f;
    return uiPageSelectorRectsFromNextButton(
        {left + buttonSize + gap + textWidth + gap, groupCenter.y - buttonSize * 0.5f},
        textWidth,
        buttonSize,
        gap);
}

UiPageSelectorRects baseMiningStageSelectorRects()
{
    constexpr float StageSelectorTextWidth = 252.0f;
    const UiRect body = uiBodyRect(basePanelRect());
    const UiRect panel = basePanelRect();
    return uiPageSelectorRectsCentered(
        {panel.pos.x + panel.size.x * 0.5f, body.pos.y + StoragePageButtonSize * 0.5f - 4.0f},
        StageSelectorTextWidth);
}

UiRect baseMiningStartChoiceRect(int index)
{
    const float left = baseMiningContentLeft();
    const float panelCenterX = basePanelRect().pos.x + basePanelRect().size.x * 0.5f;
    const float width = (panelCenterX - left) * 2.0f;
    return {{left, 322.0f + static_cast<float>(index) * 80.0f}, {width, ui::ButtonHeight}};
}

UiRect baseMiningWarpPointSelectRect()
{
    return {{360.0f, 174.0f}, {560.0f, 400.0f}};
}

UiRect baseMiningWarpPointSelectChoiceRect(int index)
{
    constexpr int Columns = 2;
    constexpr float Gap = 12.0f;
    constexpr float RowGap = 12.0f;
    constexpr float Height = 48.0f;
    const UiRect panel = baseMiningWarpPointSelectRect();
    const float left = panel.pos.x + 46.0f;
    const float top = panel.pos.y + 126.0f;
    const float width = (panel.size.x - 92.0f - Gap) / static_cast<float>(Columns);
    const int column = index % Columns;
    const int row = index / Columns;
    return {{
        left + static_cast<float>(column) * (width + Gap),
        top + static_cast<float>(row) * (Height + RowGap),
    }, {width, Height}};
}

UiRect baseMiningRegenerateConfirmRect()
{
    return {{430.0f, 232.0f}, {420.0f, 238.0f}};
}

UiRect baseMiningRegenerateConfirmButtonRect(int index)
{
    const Vec2 size{164.0f, ui::ButtonHeight};
    return index == 0
        ? uiBottomLeftButtonRect(baseMiningRegenerateConfirmRect(), size)
        : uiBottomRightButtonRect(baseMiningRegenerateConfirmRect(), size);
}

UiRect baseSellItemRect(int index)
{
    return {{420.0f, 248.0f + static_cast<float>(index) * 48.0f}, {440.0f, 38.0f}};
}

UiRect merchantSellItemRect(int index)
{
    return {{390.0f, 278.0f + static_cast<float>(index) * 58.0f}, {250.0f, ui::ButtonHeight}};
}

UiRect merchantBuyItemRect(int index)
{
    return {{660.0f, 278.0f + static_cast<float>(index) * 58.0f}, {230.0f, ui::ButtonHeight}};
}
UiRect merchantPanelRect()
{
    return {{44.0f, 58.0f}, {1192.0f, 610.0f}};
}

UiRect merchantChoiceRect(int index)
{
    return {{450.0f, 276.0f + static_cast<float>(index) * 68.0f}, {380.0f, ui::ButtonHeight}};
}

UiRect merchantGridSlotRect(int index)
{
    constexpr int Columns = 8;
    constexpr float SlotW = 88.0f;
    constexpr float SlotH = 76.0f;
    constexpr float Gap = 8.0f;
    const int row = index / Columns;
    const int column = index % Columns;
    return {{72.0f + static_cast<float>(column) * (SlotW + Gap), 170.0f + static_cast<float>(row) * (SlotH + Gap)}, {SlotW, SlotH}};
}

UiRect baseProcessingGridSlotRect(int index)
{
    UiRect rect = merchantGridSlotRect(index);
    rect.pos.x += 2.0f;
    rect.pos.y += 130.0f;
    return rect;
}

UiRect merchantDetailPanelRect()
{
    return {{864.0f, 108.0f}, {330.0f, 520.0f}};
}

void drawMoneySummaryText(Renderer& renderer, Vec2 topRight, int money)
{
    constexpr int LabelScale = 2;
    constexpr int ValueScale = 3;
    constexpr float Gap = 4.0f;
    const std::string label = "所持金";
    const std::string value = std::to_string(money) + "G";
    const Vec2 labelSize = renderer.measureText(label, LabelScale);
    const Vec2 valueSize = renderer.measureText(value, ValueScale);
    const float left = topRight.x - labelSize.x - Gap - valueSize.x;
    const float labelY = topRight.y + std::max(0.0f, valueSize.y - labelSize.y);
    renderer.drawText({left, labelY}, label, {198, 198, 206, 255}, LabelScale);
    renderer.drawText({left + labelSize.x + Gap, topRight.y}, value, {230, 230, 236, 255}, ValueScale);
}

UiRect baseUpgradeItemRect(int index)
{
    constexpr float x = 260.0f;
    constexpr float w = 292.0f;
    constexpr float h = 42.0f;
    constexpr std::array<float, BaseUpgradeItemCount> RowY{{
        176.0f,
        222.0f,
        298.0f,
        344.0f,
        420.0f,
        466.0f,
        512.0f,
        558.0f,
    }};
    const int clampedIndex = std::clamp(index, 0, BaseUpgradeItemCount - 1);
    return {{x, RowY[static_cast<std::size_t>(clampedIndex)]}, {w, h}};
}

UiRect baseUpgradeDetailPanelRect()
{
    return {{580.0f, 150.0f}, {432.0f, 396.0f}};
}

UiRect baseUpgradeConfirmRect()
{
    const UiRect detail = baseUpgradeDetailPanelRect();
    const Vec2 size{208.0f, ui::ButtonHeight};
    return {{detail.pos.x + (detail.size.x - size.x) * 0.5f, 568.0f}, size};
}

UiRect baseProcessingModeRect(int index)
{
    return {{96.0f + static_cast<float>(index) * 150.0f, 148.0f}, {142.0f, ui::ButtonHeight}};
}

UiRect baseProcessingSourceRect(int index)
{
    return {{BaseItemSourceTabX + static_cast<float>(index) * BaseItemSourceTabPitch, 238.0f}, {BaseItemSourceTabWidth, ui::ButtonHeight}};
}

UiRect baseProcessingItemRect(int index)
{
    return {{390.0f, 270.0f + static_cast<float>(index) * 58.0f}, {500.0f, ui::ButtonHeight}};
}

UiRect baseRingWorkshopItemRect(int index)
{
    const int column = index / 5;
    const int row = index % 5;
    return {{390.0f + static_cast<float>(column) * 252.0f, 252.0f + static_cast<float>(row) * 58.0f}, {240.0f, ui::ButtonHeight}};
}

UiRect ringWorkshopConfirmRect()
{
    return {{688.0f, 552.0f}, {190.0f, ui::ButtonHeight}};
}

UiRect bookshelfItemRect(int index)
{
    return {{402.0f, 250.0f + static_cast<float>(index) * 58.0f}, {474.0f, ui::ButtonHeight}};
}

UiRect storagePanelRect()
{
    return {{44.0f, 48.0f}, {1192.0f, 624.0f}};
}

UiRect storageBackpackSlotRect(int index)
{
    const int row = index / StorageColumns;
    const int column = index % StorageColumns;
    return {{
        StorageGridX + static_cast<float>(column) * (StorageSlotW + StorageSlotGap),
        StorageTopGridY + static_cast<float>(row) * (StorageSlotH + StorageSlotGap)
    }, {StorageSlotW, StorageSlotH}};
}

UiRect storageWarehouseSlotRect(int index)
{
    const int row = index / StorageColumns;
    const int column = index % StorageColumns;
    return {{
        StorageGridX + static_cast<float>(column) * (StorageSlotW + StorageSlotGap),
        StorageBottomGridY + static_cast<float>(row) * (StorageSlotH + StorageSlotGap)
    }, {StorageSlotW, StorageSlotH}};
}

UiRect storageNextPageButtonRect()
{
    return uiPageSelectorRectsFromNextButton(
        {StorageGridRightX - StoragePageButtonSize, StorageBottomHeaderY - 2.0f},
        StoragePageTextWidth).next;
}

UiRect storagePageTextRect()
{
    return uiPageSelectorRectsFromNextButton(
        {StorageGridRightX - StoragePageButtonSize, StorageBottomHeaderY - 2.0f},
        StoragePageTextWidth).text;
}

UiRect storagePrevPageButtonRect()
{
    return uiPageSelectorRectsFromNextButton(
        {StorageGridRightX - StoragePageButtonSize, StorageBottomHeaderY - 2.0f},
        StoragePageTextWidth).prev;
}

int wrapStoragePageIndex(int page, int delta, int pageCount)
{
    const int count = std::max(1, pageCount);
    return (page + delta % count + count) % count;
}

int storageGlobalSlotFromLocal(bool warehouse, int localIndex)
{
    return (warehouse ? StoragePaneSlotCount : 0) + localIndex;
}

bool storageGlobalSlotIsWarehouse(int slot)
{
    return slot >= StoragePaneSlotCount;
}

int storageLocalSlot(int globalSlot)
{
    return std::clamp(globalSlot % StoragePaneSlotCount, 0, StoragePaneSlotCount - 1);
}

UiRect storageSlotRectByGlobal(int slot)
{
    const bool warehouse = storageGlobalSlotIsWarehouse(slot);
    const int local = storageLocalSlot(slot);
    return warehouse ? storageWarehouseSlotRect(local) : storageBackpackSlotRect(local);
}

UiRect pausePanelRect()
{
    return {{390.0f, 130.0f}, {500.0f, 460.0f}};
}

UiRect pauseMenuItemRect(int index)
{
    return {{450.0f, 235.0f + static_cast<float>(index) * 58.0f}, {380.0f, ui::ButtonHeight}};
}

UiRect pauseBackButtonRect()
{
    const UiRect panel = pausePanelRect();
    const Vec2 size{180.0f, ui::ButtonHeight};
    const float footerTop = panel.pos.y + panel.size.y - uiFooterHeight("x");
    return {{panel.pos.x + (panel.size.x - size.x) * 0.5f, footerTop - size.y - 12.0f}, size};
}

UiRect quitConfirmRect()
{
    return {{420.0f, 240.0f}, {440.0f, 220.0f}};
}

UiRect quitConfirmButtonRect(int index)
{
    const Vec2 size{150.0f, 42.0f};
    return index == 0 ? uiBottomLeftButtonRect(quitConfirmRect(), size) : uiBottomRightButtonRect(quitConfirmRect(), size);
}

UiRect warpReturnConfirmRect()
{
    return {{410.0f, 220.0f}, {460.0f, 280.0f}};
}

UiRect warpReturnConfirmButtonRect(int index)
{
    const Vec2 size{172.0f, ui::ButtonHeight};
    UiRect rect = index == 0
        ? uiBottomLeftButtonRect(warpReturnConfirmRect(), size)
        : uiBottomRightButtonRect(warpReturnConfirmRect(), size);
    rect.pos.y += 28.0f;
    return rect;
}

UiRect gameOverPanelRect()
{
    return {{350.0f, 92.0f}, {580.0f, 520.0f}};
}

bool isCapturedProjectileBehavior(std::string_view behaviorId)
{
    return behaviorId == "shoot_web" ||
        behaviorId == "throw_object" ||
        behaviorId == "throw_stone" ||
        behaviorId == "shoot_poison" ||
        behaviorId == "radial_spike" ||
        behaviorId == "shoot_water" ||
        behaviorId == "shoot_fire";
}

std::string_view fallbackCapturedProjectileId(std::string_view behaviorId)
{
    if (behaviorId == "shoot_web") {
        return "web_thread";
    }
    if (behaviorId == "throw_stone" || behaviorId == "throw_object") {
        return "stone_bullet";
    }
    if (behaviorId == "shoot_poison") {
        return "poison_spit";
    }
    if (behaviorId == "radial_spike") {
        return "cactus_needle";
    }
    if (behaviorId == "shoot_water") {
        return "water_shot";
    }
    if (behaviorId == "shoot_fire") {
        return "fire_breath";
    }
    return "stone_bullet";
}

struct CapturedProjectileBehaviorPlan {
    std::string behaviorId;
    const CapturedBehaviorSpec* spec = nullptr;
};

std::optional<CapturedProjectileBehaviorPlan> capturedProjectileBehaviorPlan(const SpellRingItem& item)
{
    for (const CapturedBehaviorSpec& spec : item.capturedBehaviorSpecs) {
        if (!isCapturedProjectileBehavior(spec.behavior)) {
            continue;
        }
        return CapturedProjectileBehaviorPlan{
            .behaviorId = spec.behavior,
            .spec = &spec,
        };
    }
    if (isCapturedProjectileBehavior(item.capturedBehaviorId)) {
        return CapturedProjectileBehaviorPlan{
            .behaviorId = item.capturedBehaviorId,
            .spec = item.capturedBehaviorSpec(item.capturedBehaviorId),
        };
    }
    for (const std::string& behaviorId : item.capturedBehaviorIds) {
        if (isCapturedProjectileBehavior(behaviorId)) {
            return CapturedProjectileBehaviorPlan{
                .behaviorId = behaviorId,
                .spec = item.capturedBehaviorSpec(behaviorId),
            };
        }
    }
    return std::nullopt;
}

bool effectSpecsContain(const std::vector<EffectSpec>& specs, std::string_view effectId)
{
    for (const EffectSpec& spec : specs) {
        for (const std::string& effect : spec.effects) {
            if (effect == effectId) {
                return true;
            }
        }
    }
    return false;
}

std::vector<EffectSpec> capturedProjectileEffects(const SpellRingItem& item, std::string_view behaviorId, const BehaviorDefinition* behavior)
{
    std::vector<EffectSpec> effects = behavior != nullptr ? behavior->capturedDefaultEffects : std::vector<EffectSpec>{};
    if (behaviorId == "shoot_web" && !effectSpecsContain(effects, "status_slow")) {
        const double slowDuration = std::max(0.1, item.capturedBehaviorParamDouble("shoot_web", "duration", 2.0));
        const double slowMultiplier = std::clamp(item.capturedBehaviorParamDouble("shoot_web", "speedMultiplier", 0.70), 0.05, 1.0);
        EffectSpec slow;
        slow.target = "enemy";
        slow.effects = {"status_slow"};
        slow.values = {slowMultiplier};
        slow.duration = slowDuration;
        effects.push_back(std::move(slow));
    } else if (behaviorId == "shoot_poison" && !effectSpecsContain(effects, "status_poison")) {
        const double poisonDuration = std::max(0.1, item.capturedBehaviorParamDouble("shoot_poison", "duration", 2.0));
        const double poisonDps = std::max(0.1, item.capturedBehaviorParamDouble("shoot_poison", "damagePerSecond", 1.0));
        EffectSpec poison;
        poison.target = "enemy";
        poison.effects = {"status_poison"};
        poison.values = {poisonDps};
        poison.duration = poisonDuration;
        effects.push_back(std::move(poison));
    }
    return effects;
}

UiRect gameOverItemRect(int index)
{
    return {{460.0f, 446.0f + static_cast<float>(index) * 58.0f}, {360.0f, ui::ButtonHeight}};
}

UiRect stageClearPanelRect()
{
    return {{350.0f, 92.0f}, {580.0f, 520.0f}};
}

UiRect stageClearItemRect(int index)
{
    return {{460.0f, 446.0f + static_cast<float>(index) * 58.0f}, {360.0f, ui::ButtonHeight}};
}

const char* pauseMenuItemName(int index)
{
    switch (index) {
    case 0: return "ステータス";
    case 1: return "アイテム";
    case 2: return "リング";
    case 3: return "オプション";
    case 4: return "ゲーム終了";
    default: return "";
    }
}

const char* baseMenuItemName(int index)
{
    switch (index) {
    case 0: return "ダンジョン入口";
    case 1: return "収納箱";
    case 2: return "商人ワゴン";
    case 3: return "拠点強化炉";
    case 4: return "作業台";
    case 5: return "本棚";
    case 6: return "日記";
    case 7: return "リング工房用スペース";
    default: return "";
    }
}

const char* baseMiningStartChoiceName(int index)
{
    switch (index) {
    case 0: return "入口";
    case 1: return "ワープポイント";
    case 2: return "ダンジョンを再生成する";
    default: return "";
    }
}

const char* gameOverItemName(int index)
{
    switch (index) {
    case 0: return "リトライ";
    case 1: return "帰還";
    default: return "";
    }
}

const char* stageClearItemName(int index)
{
    switch (index) {
    case 0: return "拠点へ戻る";
    default: return "";
    }
}

std::string formatRunTime(float seconds)
{
    const int totalSeconds = std::max(0, static_cast<int>(seconds));
    const int minutes = totalSeconds / 60;
    const int restSeconds = totalSeconds % 60;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, restSeconds);
    return buffer;
}

UiRect ringPanelRect()
{
    return {{70.0f, 68.0f}, {1140.0f, 590.0f}};
}

UiRect ringTabRect(int index)
{
    return {{116.0f + static_cast<float>(index) * 174.0f, 148.0f}, {152.0f, ui::ButtonHeight}};
}

Vec2 ringOrbitCenter()
{
    const UiRect panel = ringPanelRect();
    const float detailLeft = panel.pos.x + panel.size.x - DetailOuterRightMargin - RingDetailW;
    return {panel.pos.x + (detailLeft - panel.pos.x) * 0.5f, 418.0f};
}

Vec2 ringUiOrbitCenter(const SpellRingSystem& spellRing)
{
    Vec2 center = ringOrbitCenter();
    if (spellRing.activeRingIndex() == 2) {
        center.y += RingUiThirdRingCenterYOffset;
    }
    if (spellRing.activeRingShape() == RingShape::Comet) {
        center.y += RingUiCometCenterYOffset;
    }
    return center;
}

float ringOrbitRadius()
{
    return 138.0f;
}

float ringUiShapeRadius(RingShape shape)
{
    if (shape == RingShape::FigureEight) {
        return RingUiFigureEightRadius;
    }
    if (shape == RingShape::Comet) {
        return RingUiCometRadius;
    }
    return ringOrbitRadius();
}

Vec2 ringWorldToUi(const SpellRingSystem& spellRing, Vec2 worldPosition)
{
    return worldPosition - spellRing.center() + ringUiOrbitCenter(spellRing);
}

Vec2 ringUiToWorld(const SpellRingSystem& spellRing, Vec2 uiPosition)
{
    return uiPosition - ringUiOrbitCenter(spellRing) + spellRing.center();
}

Vec2 rotateRingUiPoint(const SpellRingSystem& spellRing, Vec2 point, float radians)
{
    const Vec2 center = ringUiOrbitCenter(spellRing);
    const Vec2 local = point - center;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return center + Vec2{
        local.x * c - local.y * s,
        local.x * s + local.y * c,
    };
}

Vec2 applyRingUiShapeRotation(const SpellRingSystem& spellRing, Vec2 point)
{
    if (spellRing.activeRingShape() == RingShape::Comet) {
        return rotateRingUiPoint(spellRing, point, RingUiCometArcRotation);
    }
    return point;
}

Vec2 removeRingUiShapeRotation(const SpellRingSystem& spellRing, Vec2 point)
{
    if (spellRing.activeRingShape() == RingShape::Comet) {
        return rotateRingUiPoint(spellRing, point, -RingUiCometArcRotation);
    }
    return point;
}

RingOrbitContext ringUiOrbitContext(
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int itemIndex,
    int itemCount)
{
    RingOrbitContext context;
    context.shape = spellRing.activeRingShape();
    context.radius = ringUiShapeRadius(context.shape);
    context.shapeRotation = 0.0f;
    context.itemIndex = std::max(0, itemIndex);
    context.itemCount = std::max(1, itemCount);
    context.tuning = makeRingOrbitTuning(balance);
    return context;
}

float ringUiPathParam(const SpellRingSystem& spellRing, float localAngle, const RingOrbitContext& context)
{
    (void)spellRing;
    if (context.shape == RingShape::Comet) {
        return localAngle;
    }
    return localAngle;
}

Vec2 ringItemUiCenter(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int itemIndex,
    int itemCount)
{
    const RingOrbitContext context = ringUiOrbitContext(spellRing, balance, itemIndex, itemCount);
    const Vec2 runtimeWorld = getRingItemWorldPosition(
        spellRing.center(),
        ringUiPathParam(spellRing, item.localAngle, context),
        context);
    return applyRingUiShapeRotation(spellRing, ringWorldToUi(spellRing, runtimeWorld));
}

Vec2 ringItemUiCenterAtAngle(
    float angle,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int itemIndex,
    int itemCount)
{
    const RingOrbitContext context = ringUiOrbitContext(spellRing, balance, itemIndex, itemCount);
    const Vec2 runtimeWorld = getRingItemWorldPosition(
        spellRing.center(),
        ringUiPathParam(spellRing, angle, context),
        context);
    return applyRingUiShapeRotation(spellRing, ringWorldToUi(spellRing, runtimeWorld));
}

UiRect ringItemUiRect(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int itemIndex,
    int itemCount)
{
    constexpr Vec2 Size{54.0f, 54.0f};
    const Vec2 center = ringItemUiCenter(item, spellRing, balance, itemIndex, itemCount);
    return {center - Size * 0.5f, Size};
}

float normalizeRingAngle(float angle)
{
    const float full = Pi * 2.0f;
    angle = std::fmod(angle, full);
    if (angle < 0.0f) {
        angle += full;
    }
    return angle;
}

float shortestRingAngleDelta(float from, float to, RingShape shape, const RuntimeBalance& balance)
{
    if (shape == RingShape::Comet) {
        const RingOrbitTuning tuning = makeRingOrbitTuning(balance);
        const float arc = std::clamp(std::abs(tuning.cometArcDegrees), 10.0f, std::max(10.0f, tuning.cometMaxArcDegrees)) * (Pi / 180.0f);
        const float raw = to - from;
        return std::clamp(raw, -arc, arc);
    }

    float delta = normalizeRingAngle(to) - normalizeRingAngle(from);
    if (delta > Pi) {
        delta -= Pi * 2.0f;
    } else if (delta < -Pi) {
        delta += Pi * 2.0f;
    }
    return delta;
}

float ringAngleFromPoint(Vec2 point, const SpellRingSystem& spellRing, const RuntimeBalance& balance)
{
    const Vec2 worldPoint = ringUiToWorld(spellRing, removeRingUiShapeRotation(spellRing, point));
    const RingOrbitContext context = ringUiOrbitContext(spellRing, balance, 0, 1);
    const float param = findNearestRingPathParam(
        worldPoint,
        spellRing.center(),
        context,
        320);
    if (context.shape == RingShape::Comet) {
        return spellRing.quantizeLocalAngle(param, balance);
    }
    return spellRing.quantizeLocalAngle(param, balance);
}

struct MagicOrbitDrawOptions {
    RingShape shape = RingShape::Circle;
    bool active = false;
    bool energized = false;
    bool screenPresentation = false;
    bool centerSigil = false;
    int ringIndex = 0;
    float totalSeconds = 0.0f;
    float alphaScale = 1.0f;
};

constexpr std::array<RingShape, 3> MagicRingShapeRenderOrder{{
    RingShape::Circle,
    RingShape::FigureEight,
    RingShape::Comet,
}};

float wrap01(float value)
{
    value = std::fmod(value, 1.0f);
    if (value < 0.0f) {
        value += 1.0f;
    }
    return value;
}

std::uint8_t alphaByte(float alpha)
{
    return static_cast<std::uint8_t>(std::clamp(std::lround(alpha), 0L, 255L));
}

Color withAlpha(Color color, float alpha)
{
    color.a = alphaByte(alpha);
    return color;
}

Color scaledAlpha(Color color, float scale)
{
    color.a = alphaByte(static_cast<float>(color.a) * std::max(0.0f, scale));
    return color;
}

std::vector<Vec2> makeCirclePath(Vec2 center, float radius, int sampleCount)
{
    const int count = std::max(16, sampleCount);
    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float t01 = static_cast<float>(i) / static_cast<float>(count - 1);
        points.push_back(center + fromAngle(t01 * Pi * 2.0f) * radius);
    }
    return points;
}

Vec2 pathPointAt(const std::vector<Vec2>& points, float t01, bool wrap)
{
    if (points.empty()) {
        return {};
    }
    if (points.size() == 1) {
        return points.front();
    }

    const float t = wrap ? wrap01(t01) : clamp(t01, 0.0f, 1.0f);
    const float scaled = t * static_cast<float>(points.size() - 1);
    const std::size_t index = std::min<std::size_t>(
        points.size() - 2,
        static_cast<std::size_t>(std::max(0.0f, std::floor(scaled))));
    const float localT = scaled - static_cast<float>(index);
    return lerp(points[index], points[index + 1], localT);
}

Vec2 pathTangentAt(const std::vector<Vec2>& points, float t01, bool wrap)
{
    constexpr float SampleOffset = 0.006f;
    const Vec2 before = pathPointAt(points, t01 - SampleOffset, wrap);
    const Vec2 after = pathPointAt(points, t01 + SampleOffset, wrap);
    return normalize(after - before);
}

void drawMagicStar(Renderer& renderer, Vec2 center, float radius, Color color, float rotation)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    const Vec2 primary = fromAngle(rotation) * radius;
    const Vec2 secondary = fromAngle(rotation + Pi * 0.5f) * (radius * 0.68f);
    renderer.drawLine(center - primary, center + primary, color);
    renderer.drawLine(center - secondary, center + secondary, color);
    renderer.fillCircle(center, std::max(0.8f, radius * 0.18f), withAlpha(color, std::min(255.0f, static_cast<float>(color.a) + 45.0f)));
}

void drawMagicOrbitRunes(Renderer& renderer, const std::vector<Vec2>& orbitPath, const MagicOrbitDrawOptions& options)
{
    if (orbitPath.size() < 2) {
        return;
    }

    const int runeCount = options.screenPresentation ? 18 : 11;
    const bool wrap = options.shape != RingShape::Comet;
    const float lengthScale = options.screenPresentation ? 1.25f : 1.0f;
    for (int i = 0; i < runeCount; ++i) {
        if (!options.active && ((i + options.ringIndex) % 3) != 0) {
            continue;
        }

        const float t01 = (static_cast<float>(i) + 0.5f) / static_cast<float>(runeCount);
        const float pulse = 0.62f + 0.38f * std::sin(options.totalSeconds * 2.2f + static_cast<float>(i) * 0.83f + static_cast<float>(options.ringIndex));
        const float alpha = (options.active ? 64.0f : 28.0f) * options.alphaScale * pulse;
        const Vec2 point = pathPointAt(orbitPath, t01, wrap);
        const Vec2 tangent = pathTangentAt(orbitPath, t01, wrap);
        const Vec2 normal{-tangent.y, tangent.x};
        const float halfLength = (options.active ? 2.8f : 1.9f) * lengthScale;
        const Color runeColor = withAlpha(options.energized ? Color{255, 204, 100, 255} : Color{232, 236, 255, 255}, alpha);
        renderer.drawLine(point - normal * halfLength, point + normal * halfLength, runeColor);
    }
}

void drawMagicOrbitFlow(Renderer& renderer, const std::vector<Vec2>& orbitPath, const MagicOrbitDrawOptions& options)
{
    if (orbitPath.size() < 2) {
        return;
    }

    const bool wrap = options.shape != RingShape::Comet;
    const int beadCount = options.screenPresentation
        ? (options.active ? 6 : 4)
        : (options.active ? 3 : 2);
    const float speed = options.shape == RingShape::Comet ? 0.18f : 0.085f;
    const float radiusScale = options.screenPresentation ? 1.25f : 1.0f;
    for (int i = 0; i < beadCount; ++i) {
        const float t01 = wrap01(options.totalSeconds * speed + static_cast<float>(i) / static_cast<float>(beadCount) + static_cast<float>(options.ringIndex) * 0.173f);
        const float pulse = 0.70f + 0.30f * std::sin(options.totalSeconds * 5.8f + static_cast<float>(i) * 1.7f);
        const Vec2 point = pathPointAt(orbitPath, t01, wrap);
        const float radius = (options.active ? 1.8f : 1.3f) * radiusScale * pulse;
        const Color outer = withAlpha(options.energized ? Color{255, 198, 92, 255} : Color{120, 226, 255, 255}, (options.active ? 54.0f : 30.0f) * options.alphaScale);
        const Color inner = withAlpha(options.energized ? Color{255, 244, 176, 255} : Color{255, 250, 202, 255}, (options.active ? 154.0f : 86.0f) * options.alphaScale);
        renderer.fillCircle(point, radius * 2.1f, outer);
        renderer.fillCircle(point, radius, inner);
    }
}

void drawMagicOrbitSparkles(Renderer& renderer, const std::vector<Vec2>& orbitPath, const MagicOrbitDrawOptions& options)
{
    if (orbitPath.size() < 2) {
        return;
    }

    const bool wrap = options.shape != RingShape::Comet;
    const int sparkleCount = options.screenPresentation
        ? (options.active ? 7 : 4)
        : (options.active ? 3 : 1);
    const float radiusScale = options.screenPresentation ? 1.28f : 1.0f;
    for (int i = 0; i < sparkleCount; ++i) {
        const float seed = static_cast<float>(options.ringIndex) * 1.91f + static_cast<float>(i) * 2.37f;
        const float twinkle = 0.5f + 0.5f * std::sin(options.totalSeconds * (2.6f + static_cast<float>(i) * 0.11f) + seed);
        const float alpha = (options.active ? 112.0f : 52.0f) * options.alphaScale * twinkle;
        if (alpha < 18.0f) {
            continue;
        }

        const float t01 = wrap01(static_cast<float>(i) * 0.618f + static_cast<float>(options.ringIndex) * 0.117f + options.totalSeconds * 0.025f);
        const Vec2 tangent = pathTangentAt(orbitPath, t01, wrap);
        const Vec2 normal{-tangent.y, tangent.x};
        const float offset = std::sin(seed * 1.7f) * (options.screenPresentation ? 5.0f : 3.0f);
        const Vec2 point = pathPointAt(orbitPath, t01, wrap) + normal * offset;
        const Color sparkle = withAlpha(options.energized ? Color{255, 226, 126, 255} : Color{214, 240, 255, 255}, alpha);
        drawMagicStar(renderer, point, (1.6f + twinkle * 2.0f) * radiusScale, sparkle, seed + options.totalSeconds * 0.7f);
    }
}

void drawMagicOrbitCenter(Renderer& renderer, Vec2 center, const MagicOrbitDrawOptions& options)
{
    const float alpha = (options.active ? 118.0f : 62.0f) * options.alphaScale;
    const Color glow = withAlpha(options.energized ? Color{255, 210, 104, 255} : Color{150, 220, 255, 255}, alpha * 0.55f);
    const Color core = withAlpha(Color{255, 248, 196, 255}, alpha);
    renderer.fillCircle(center, options.screenPresentation ? 4.6f : 2.4f, glow);
    renderer.drawCircle(center, options.screenPresentation ? 7.6f : 4.4f, withAlpha(core, alpha * 0.64f));
    drawMagicStar(renderer, center, options.screenPresentation ? 5.8f : 3.3f, withAlpha(core, alpha), options.totalSeconds * 0.8f);
}

void drawTaperedMagicPolyline(Renderer& renderer, const std::vector<Vec2>& points, float tailWidth, float headWidth, Color color)
{
    if (points.size() < 2 || color.a == 0) {
        return;
    }

    const float baseAlpha = static_cast<float>(color.a);
    for (std::size_t i = 1; i < points.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(points.size() - 1);
        const float eased = t * t;
        const float width = lerp(tailWidth, headWidth, eased);
        const float alpha = baseAlpha * lerp(0.28f, 1.0f, eased);
        renderer.drawSoftLine(points[i - 1], points[i], width, withAlpha(color, alpha));
    }
}

void drawMagicOrbitPath(Renderer& renderer, const std::vector<Vec2>& orbitPath, Vec2 center, const MagicOrbitDrawOptions& options)
{
    if (orbitPath.size() < 2) {
        return;
    }

    const float widthScale = options.screenPresentation ? 1.35f : 1.0f;
    const Color auraColor = options.energized
        ? Color{255, 178, 74, 62}
        : (options.active ? Color{90, 200, 255, 64} : Color{116, 150, 210, 36});
    const Color coreColor = options.energized
        ? Color{255, 230, 142, 132}
        : (options.active ? Color{236, 242, 255, 124} : Color{178, 196, 238, 62});

    if (options.shape == RingShape::Comet) {
        drawTaperedMagicPolyline(
            renderer,
            orbitPath,
            (options.active ? 1.6f : 1.1f) * widthScale,
            (options.active ? 5.4f : 3.5f) * widthScale,
            scaledAlpha(auraColor, options.alphaScale));
        const std::size_t tailCount = std::min<std::size_t>(orbitPath.size(), options.screenPresentation ? 74 : 44);
        std::vector<Vec2> tailPath(orbitPath.end() - static_cast<std::ptrdiff_t>(tailCount), orbitPath.end());
        drawTaperedMagicPolyline(
            renderer,
            tailPath,
            (options.active ? 1.2f : 0.9f) * widthScale,
            (options.active ? 6.8f : 4.4f) * widthScale,
            scaledAlpha(Color{255, 216, 112, 64}, options.alphaScale));
    } else {
        renderer.drawSoftPolyline(orbitPath, (options.active ? 5.4f : 3.5f) * widthScale, scaledAlpha(auraColor, options.alphaScale));
    }

    if (options.shape == RingShape::Comet) {
        drawTaperedMagicPolyline(
            renderer,
            orbitPath,
            (options.active ? 0.7f : 0.5f) * widthScale,
            (options.active ? 1.8f : 1.2f) * widthScale,
            scaledAlpha(coreColor, options.alphaScale));
    } else {
        renderer.drawSoftPolyline(orbitPath, (options.active ? 1.8f : 1.2f) * widthScale, scaledAlpha(coreColor, options.alphaScale));
    }
    for (std::size_t i = 1; i < orbitPath.size(); ++i) {
        if (options.shape == RingShape::FigureEight && !options.screenPresentation && (i % 4) == 0) {
            continue;
        }
        const float lineAlphaScale = options.shape == RingShape::Comet
            ? options.alphaScale * 0.54f * lerp(0.22f, 1.0f, std::pow(static_cast<float>(i) / static_cast<float>(orbitPath.size() - 1), 2.0f))
            : options.alphaScale * 0.54f;
        renderer.drawLine(orbitPath[i - 1], orbitPath[i], scaledAlpha(coreColor, lineAlphaScale));
    }

    drawMagicOrbitRunes(renderer, orbitPath, options);
    drawMagicOrbitFlow(renderer, orbitPath, options);
    drawMagicOrbitSparkles(renderer, orbitPath, options);

    if (options.shape == RingShape::Comet) {
        const Vec2 head = orbitPath.back();
        renderer.fillCircle(head, (options.active ? 4.8f : 3.2f) * widthScale, withAlpha(Color{255, 218, 112, 255}, (options.active ? 98.0f : 54.0f) * options.alphaScale));
        renderer.fillCircle(head, (options.active ? 2.2f : 1.5f) * widthScale, withAlpha(Color{255, 252, 210, 255}, (options.active ? 172.0f : 100.0f) * options.alphaScale));
    }

    if (options.centerSigil || options.shape == RingShape::FigureEight) {
        drawMagicOrbitCenter(renderer, center, options);
    }
}

void drawSpellRingOrbitLayer(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    float totalSeconds,
    float alphaScale)
{
    const bool energized = spellRing.state() != SpellRingState::Normal;
    for (RingShape shapePass : MagicRingShapeRenderOrder) {
        for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
            const auto& ringItems = spellRing.itemsForRing(ringIndex);
            if (ringItems.empty() || spellRing.ringShapeForIndex(ringIndex) != shapePass) {
                continue;
            }

            std::vector<Vec2> orbitPath = shapePass == RingShape::Circle
                ? makeCirclePath(spellRing.center(), spellRing.radius(), 160)
                : spellRing.pathSamplePointsForRing(ringIndex, spellRing.center(), 1.0f, balance, 160);
            drawMagicOrbitPath(
                renderer,
                orbitPath,
                spellRing.center(),
                MagicOrbitDrawOptions{
                    shapePass,
                    ringIndex == spellRing.activeRingIndex(),
                    energized,
                    false,
                    false,
                    ringIndex,
                    totalSeconds,
                    alphaScale,
                });
        }
    }
}

float ringItemBobPhase(const SpellRingItem& item, float totalSeconds)
{
    return totalSeconds * RingItemBobSpeed + item.localAngle * 1.7f;
}

float ringItemAltitude(const SpellRingItem& item, float totalSeconds)
{
    return RingItemBaseAltitude + std::sin(ringItemBobPhase(item, totalSeconds)) * RingItemBobAmplitude;
}

float ringItemRotationWobbleDegrees(const SpellRingItem& item, float totalSeconds)
{
    return std::cos(ringItemBobPhase(item, totalSeconds)) * RingItemRotationWobbleDegrees;
}

Vec2 ringItemActionShakeOffset(const SpellRingItem& item, float totalSeconds)
{
    if (item.actionFlashTimer <= 0.0f) {
        return {};
    }

    const float t = std::clamp(item.actionFlashTimer / SpellRingItemActionFlashSeconds, 0.0f, 1.0f);
    const float eased = t * t * (3.0f - 2.0f * t);
    const float amplitude = 3.6f * eased;
    const float phase = item.localAngle * 17.0f + static_cast<float>(item.ringIndex) * 5.0f;
    constexpr float ShakeSpeed = 86.0f;
    return {
        std::sin(totalSeconds * ShakeSpeed + phase) * amplitude,
        std::cos(totalSeconds * ShakeSpeed * 1.27f + phase) * amplitude,
    };
}

Vec2 ringItemDrawPosition(const SpellRingItem& item, float totalSeconds)
{
    return elevatedDrawPosition(item.worldPosition, ringItemAltitude(item, totalSeconds)) +
        ringItemActionShakeOffset(item, totalSeconds);
}

float ringItemBaseShadowVisualSize(const SpellRingItem& item)
{
    switch (item.type) {
    case SpellRingItemType::Shovel:
    case SpellRingItemType::Torch:
    case SpellRingItemType::Object:
        return std::max(RingObjectImageMaxSize, item.hitRadius * 2.0f);
    }
    return RingObjectImageMaxSize;
}

float ringItemShadowVisualSize(const SpellRingItem& item, float totalSeconds)
{
    return actorShadowVisualSizeForAltitude(ringItemBaseShadowVisualSize(item), ringItemAltitude(item, totalSeconds));
}

Vec2 ringItemOutwardDirection(const SpellRingItem& item, Vec2 outward)
{
    if (lengthSquared(outward) > 0.0001f) {
        return normalize(outward);
    }
    return fromAngle(item.localAngle);
}

ObjectImageDrawOptions ringItemActionFlashOptions(const SpellRingItem& item, ObjectImageDrawOptions options = {})
{
    if (item.actionFlashTimer > 0.0f) {
        const float t = std::clamp(item.actionFlashTimer / SpellRingItemActionFlashSeconds, 0.0f, 1.0f);
        const float eased = t * t * (3.0f - 2.0f * t);
        options.maskOverlayColor = {
            255,
            244,
            214,
            static_cast<unsigned char>(std::round(150.0f * eased)),
        };
    }
    return options;
}

ItemImageDrawOptions ringWorldItemImageOptions(
    const SpellRingItem& item,
    Vec2 outward,
    float totalSeconds,
    const ObjectImageDrawOptions& options)
{
    ItemImageDrawOptions itemOptions = itemImageOptionsFromObjectOptions(options);
    itemOptions.enemyAnimationTimeSeconds = totalSeconds;
    itemOptions.enemy.directionOverrideEnabled = true;
    itemOptions.enemy.directionOverride = ringItemOutwardDirection(item, outward);
    itemOptions.enemy.rotationDegrees = 0.0f;
    return itemOptions;
}

void drawRingItemShape(
    Renderer& renderer,
    const SpellRingItem& item,
    const ItemData* object,
    Vec2 center,
    Vec2 outward,
    Vec2 forward,
    float totalSeconds,
    bool selected,
    bool invalid = false)
{
    const Color outline = selected
        ? (invalid ? Color{255, 92, 92, 255} : Color{255, 230, 150, 255})
        : Color{96, 104, 126, 220};

    if (object != nullptr) {
        ObjectImageDrawOptions baseImageOptions = ringItemActionFlashOptions(item);
        baseImageOptions.rotationDegrees = ringItemRotationWobbleDegrees(item, totalSeconds);
        ObjectImageDrawOptions imageOptions = objectRingImageOptions(
            *object,
            outward,
            forward,
            totalSeconds,
            baseImageOptions);
        if (selected) {
            imageOptions = withSelectedItemOutline(imageOptions, outline, 6);
        }
        if (drawItemImage(
                renderer,
                *object,
                center,
                {RingObjectImageMaxSize, RingObjectImageMaxSize},
                imageOptions)) {
            return;
        }
    } else if (item.objectVisual.imageNumber > 0) {
        ObjectImageDrawOptions imageOptions = ringItemActionFlashOptions(item);
        imageOptions.rotationDegrees = ringItemRotationWobbleDegrees(item, totalSeconds);
        if (selected) {
            imageOptions = withSelectedItemOutline(imageOptions, outline, 6);
        }
        if (drawItemVisual(
                renderer,
                item.objectVisual,
                center,
                {RingObjectImageMaxSize, RingObjectImageMaxSize},
                itemImageOptionsFromObjectOptions(imageOptions))) {
            return;
        }
    }

    if (item.type == SpellRingItemType::Shovel) {
        if (selected) {
            renderer.drawCircle(center, 15.0f, outline);
            renderer.drawLine(center, center + outward * 24.0f, outline);
            renderer.drawLine(center + Vec2{-3.0f, 0.0f}, center + outward * 24.0f + Vec2{-3.0f, 0.0f}, outline);
            renderer.drawLine(center + Vec2{3.0f, 0.0f}, center + outward * 24.0f + Vec2{3.0f, 0.0f}, outline);
        }
        renderer.fillCircle(center, 11.0f, {178, 184, 190, 255});
        renderer.drawLine(center, center + outward * 20.0f, {90, 96, 102, 255});
        return;
    }

    if (item.type == SpellRingItemType::Torch) {
        if (selected) {
            renderer.drawCircle(center, 17.0f, outline);
            renderer.drawCircle(center + Vec2{3.0f, -4.0f}, 8.0f, outline);
        }
        renderer.fillCircle(center, 13.0f, {242, 122, 25, 255});
        renderer.fillCircle(center + Vec2{3.0f, -4.0f}, 5.0f, {255, 238, 98, 255});
        return;
    }

    if (selected) {
        renderer.drawCircle(center, 17.0f, outline);
    }
    renderer.fillCircle(center, 12.0f, {96, 122, 210, 255});
    renderer.drawCircle(center, 15.0f, {160, 202, 255, 255});
}

bool drawRingItemObjectImage(
    Renderer& renderer,
    const SpellRingItem& item,
    const ItemData* object,
    Vec2 center,
    Vec2 maxSize,
    Vec2 outward,
    Vec2 forward,
    float totalSeconds,
    const ObjectImageDrawOptions& baseOptions = {})
{
    if (object == nullptr) {
        ObjectImageDrawOptions options = ringItemActionFlashOptions(item, baseOptions);
        options.rotationDegrees += ringItemRotationWobbleDegrees(item, totalSeconds);
        return drawItemVisual(
            renderer,
            item.objectVisual,
            center,
            maxSize,
            ringWorldItemImageOptions(item, outward, totalSeconds, options));
    }
    ObjectImageDrawOptions options = ringItemActionFlashOptions(item, baseOptions);
    options.rotationDegrees += ringItemRotationWobbleDegrees(item, totalSeconds);
    const ObjectImageDrawOptions imageOptions = objectRingImageOptions(
        *object,
        outward,
        forward,
        totalSeconds,
        options);
    return drawItemImage(
        renderer,
        *object,
        center,
        maxSize,
        ringWorldItemImageOptions(item, outward, totalSeconds, imageOptions));
}

UiRect ringDetailRect()
{
    const UiRect panel = ringPanelRect();
    const float detailX = panel.pos.x + panel.size.x - DetailOuterRightMargin - RingDetailW;
    const float detailY = panel.pos.y + DetailOuterTopMargin;
    const float detailH = panel.size.y - DetailOuterTopMargin - DetailOuterBottomMargin;
    return {{detailX, detailY}, {RingDetailW, detailH}};
}

const char* spellRingItemName(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return "スコップ";
    case SpellRingItemType::Torch: return "松明";
    case SpellRingItemType::Object: return "Object";
    }
    return "不明";
}

const char* spellRingItemCategory(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return "採掘";
    case SpellRingItemType::Torch: return "照明";
    case SpellRingItemType::Object: return "Objects DB";
    }
    return "不明";
}

const char* ringShapeDisplayName(RingShape shape)
{
    switch (shape) {
    case RingShape::Circle: return "円";
    case RingShape::FigureEight: return "8の字";
    case RingShape::Comet: return "彗星";
    }
    return "円";
}

const ItemData* objectForRingItem(const ObjectCatalog& catalog, const SpellRingItem& item)
{
    if (item.objectId.empty()) {
        return nullptr;
    }
    return catalog.registry.findById(item.objectId);
}

std::string ringItemDisplayName(const ObjectCatalog& catalog, const SpellRingItem& item)
{
    const ItemData* object = objectForRingItem(catalog, item);
    if (object != nullptr && !object->name.empty()) {
        return object->name;
    }
    return spellRingItemName(item.type);
}

ItemInstance inventoryInstanceFromRingItem(
    InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    const SpellRingItem& item)
{
    ItemInstance instance;
    if (item.instanceId.empty()) {
        const ItemData* object = objectCatalog.registry.findById(item.objectId);
        const ItemData missingObject = object == nullptr ? makeMissingItemData(item.objectId) : ItemData{};
        instance = inventory.createDetachedObjectInstance(object != nullptr ? *object : missingObject);
    } else {
        instance.instanceId = item.instanceId;
        instance.objectId = item.objectId;
    }

    instance.objectId = item.objectId;
    instance.currentDurability = item.durability;
    instance.maxDurability = item.maxDurability;
    instance.enhanceLevel = item.enhanceLevel;
    instance.attackBonus = item.attackBonus;
    instance.digBonus = item.digBonus;
    instance.durabilityBonus = item.durabilityBonus;
    instance.weightModifier = item.weightModifier;
    instance.sizeModifier = item.sizeModifier;
    instance.protectionEnabled = item.protectionEnabled;
    instance.isBroken = item.broken();
    instance.addedEffects = item.addedEffects;
    instance.addedTags = item.addedTags;
    return instance;
}

bool dbDebugLogEnabled()
{
#ifdef _MSC_VER
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, "MAJO_DB_DEBUG") != 0 || rawValue == nullptr) {
        return true;
    }
    const std::string value(rawValue);
    std::free(rawValue);
    const std::string_view text(value);
#else
    const char* value = std::getenv("MAJO_DB_DEBUG");
    if (value == nullptr) {
        return true;
    }
    const std::string_view text(value);
#endif
    return text != "0" && text != "false" && text != "FALSE" && text != "off" && text != "OFF";
}

bool effectDebugLogEnabled()
{
#ifdef _MSC_VER
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, "MAJO_EFFECT_DEBUG") != 0 || rawValue == nullptr) {
        return false;
    }
    const std::string value(rawValue);
    std::free(rawValue);
    const std::string_view text(value);
#else
    const char* value = std::getenv("MAJO_EFFECT_DEBUG");
    if (value == nullptr) {
        return false;
    }
    const std::string_view text(value);
#endif
    return text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON";
}

std::filesystem::path saveDataPath()
{
#ifdef _MSC_VER
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, "LOCALAPPDATA") == 0 && rawValue != nullptr && rawValue[0] != '\0') {
        const std::filesystem::path path = std::filesystem::path(rawValue) / "MajoShovel" / "save.dat";
        std::free(rawValue);
        return path;
    }
    if (rawValue != nullptr) {
        std::free(rawValue);
    }
#else
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData != nullptr && localAppData[0] != '\0') {
        return std::filesystem::path(localAppData) / "MajoShovel" / "save.dat";
    }
#endif
    return std::filesystem::path("save.dat");
}

const char* saveRingObjectId(const SpellRingItem& item)
{
    return item.objectId.empty() ? "-" : item.objectId.c_str();
}

std::string loadRingObjectId(std::string_view value)
{
    return value == "-" ? std::string{} : std::string(value);
}

std::string playerDeathCauseText(const Player& player)
{
    if ((player.lastDamageSource == DamageSource::SlimeContact ||
            player.lastDamageSource == DamageSource::SlimeAttack) &&
        !player.lastDamageEnemyName.empty()) {
        return player.lastDamageEnemyName +
            (player.lastDamageSource == DamageSource::SlimeAttack ? "の攻撃で死亡" : "の接触で死亡");
    }
    return std::string(deathCauseText(player.lastDamageSource));
}

ObjectDefinition makeCapturedObjectDefinition(const EnemyDefinition& enemy)
{
    ObjectDefinition item;
    item.id = "captured_" + enemy.id;
    item.name = enemy.name;
    item.category = "\xE8\xBB\x8C\xE9\x81\x93";
    item.description = enemy.capturedDescription;
    item.rarity = 1;
    item.price = 0;
    item.visual.source = ItemVisualSource::Enemy;
    item.visual.imageNumber = enemy.imageNumber;
    item.visual.sourceId = enemy.id;
    item.normalEffects = enemy.capturedNormalEffects;
    item.orbitEffects = enemy.capturedOrbitEffects;
    item.attackPower = enemy.capturedAttackPower;
    item.damageType = enemy.capturedDamageType.empty() ? "none" : enemy.capturedDamageType;
    const std::string normalizedDamageType = normalizeDamageType(item.damageType);
    if (normalizedDamageType.empty()) {
        if (item.damageType == "physical") {
            logError("[warning] Game: captured object damage type physical is deprecated; using blunt");
            item.damageType = "blunt";
        } else {
            logError("[warning] Game: captured object damage type \"" + item.damageType + "\" is invalid; using none");
            item.damageType = "none";
        }
    } else {
        item.damageType = normalizedDamageType;
    }
    item.digPower = enemy.capturedDigPower;
    item.durability = enemy.capturedDurability;
    item.weightKg = enemy.capturedWeight;
    item.tags = enemy.capturedTags;
    item.effectText = enemy.capturedEffectText;
    item.capturedBehaviorIds = enemy.capturedBehaviorIds;
    item.discoveryEffectLines = buildDiscoveryEffectLines(item);
    item.capturedBehaviorSpecs.reserve(enemy.capturedBehaviorSpecs.size());
    for (const EnemyBehaviorSpec& spec : enemy.capturedBehaviorSpecs) {
        CapturedBehaviorSpec runtimeSpec;
        runtimeSpec.trigger = spec.trigger;
        runtimeSpec.behavior = spec.behavior;
        runtimeSpec.params = spec.params;
        runtimeSpec.intervalSeconds = spec.intervalSeconds;
        item.capturedBehaviorSpecs.push_back(std::move(runtimeSpec));
    }
    return item;
}

void upsertObjectDefinition(ObjectCatalog& catalog, ObjectDefinition item)
{
    if (item.id.empty()) {
        return;
    }
    bool replaced = false;
    for (ObjectDefinition& existing : catalog.objects) {
        if (existing.id == item.id) {
            existing = item;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        catalog.objects.push_back(item);
    }
    catalog.objectsById[item.id] = item;
    catalog.registry.rebuild(catalog.objects);
}

int ringTypeToInt(SpellRingItemType type)
{
    return static_cast<int>(type);
}

SpellRingItemType ringTypeFromInt(int value)
{
    if (value == static_cast<int>(SpellRingItemType::Shovel)) {
        return SpellRingItemType::Shovel;
    }
    if (value == static_cast<int>(SpellRingItemType::Torch)) {
        return SpellRingItemType::Torch;
    }
    return SpellRingItemType::Object;
}

const char* saveRingShapeName(RingShape shape)
{
    switch (shape) {
    case RingShape::Circle: return "Circle";
    case RingShape::FigureEight: return "FigureEight";
    case RingShape::Comet: return "Comet";
    }
    return "Circle";
}

RingShape parseRingShapeValue(std::string_view value, RingShape fallback)
{
    const std::string lowered = lowerAscii(std::string(value));
    if (lowered == "circle" || lowered == "0") {
        return RingShape::Circle;
    }
    if (lowered == "figureeight" || lowered == "figure_eight" || lowered == "figure8" || lowered == "1") {
        return RingShape::FigureEight;
    }
    if (lowered == "comet" || lowered == "2") {
        return RingShape::Comet;
    }
    return fallback;
}

std::size_t countIssues(
    const ObjectCatalog& catalog,
    DbValidationSeverity severity,
    DbValidationCategory category)
{
    return static_cast<std::size_t>(std::count_if(
        catalog.validationIssues.begin(),
        catalog.validationIssues.end(),
        [severity, category](const DbValidationIssue& issue) {
            return issue.severity == severity && issue.category == category;
        }));
}

std::size_t countSeverity(const ObjectCatalog& catalog, DbValidationSeverity severity)
{
    return static_cast<std::size_t>(std::count_if(
        catalog.validationIssues.begin(),
        catalog.validationIssues.end(),
        [severity](const DbValidationIssue& issue) {
            return issue.severity == severity;
        }));
}

void logIssueList(
    const ObjectCatalog& catalog,
    std::string_view title,
    DbValidationSeverity severity,
    DbValidationCategory category)
{
    const std::size_t count = countIssues(catalog, severity, category);
    logError(std::string(title) + ": " + std::to_string(count));
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        if (issue.severity == severity && issue.category == category) {
            logError("  [" + std::string(dbValidationSeverityName(issue.severity)) + "] " + issue.message);
        }
    }
}

void addTargetCount(std::vector<std::pair<std::string, std::size_t>>& counts, std::string_view target)
{
    if (target.empty()) {
        return;
    }
    const auto it = std::find_if(counts.begin(), counts.end(), [target](const auto& entry) {
        return entry.first == target;
    });
    if (it != counts.end()) {
        ++it->second;
        return;
    }
    counts.emplace_back(target, 1);
}

void logEffectTargetUsage(const ObjectCatalog& catalog)
{
    std::vector<std::pair<std::string, std::size_t>> counts;
    for (const ObjectDefinition& object : catalog.objects) {
        for (const EffectSpec& spec : object.normalEffects) {
            addTargetCount(counts, spec.target);
        }
        for (const EffectSpec& spec : object.orbitEffects) {
            addTargetCount(counts, spec.target);
        }
    }

    std::sort(counts.begin(), counts.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });

    logError("EffectSpec targets used: " + std::to_string(counts.size()));
    for (const auto& [target, count] : counts) {
        logError("  target=\"" + target + "\" specs=" + std::to_string(count));
    }
}

void logDungeonGenerationAudit()
{
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

    logError("=== Dungeon generation extension pre-audit ===");
    logError("[audit] chunks: TileMap::updateAround keeps the player-centered active window; TileMap::render/tileAtWorld can also lazily create missing chunks.");
    logError("[audit] terrain: TileMap::initializeChunk remains chunk-local but samples Game-owned DungeonLayout distance fields for initial terrain.");
    logError("[audit] tile_hp: TileType={Empty,Dirt,Rock,Ore,HardRock}; HP is scaled by stage and local hardness.");
    logError("[audit] digging: DiggingSystem returns hit/opened/dug tiles, rewardDropRequests, and capturedExplosionRequests; opened tiles are the hook for buried rewards or hidden enemies.");
    logError("[audit] enemies: EnemySystem::spawnFromDugTiles uses dug-event min/guarantee counters; generated hidden enemies should be consumed through a separate per-tile marker before this random spawn path.");
    logError("[audit] lights: Game::render already composes extra LightSource entries for ring items, warp points, and boss spawn markers before TileMap/Drop/Projectile/Enemy rendering.");
    logError("[audit] drops: Game consumes DugTile event counters for dig money/item drops; WorldDropSystem pickup is player contact via InventorySystem.");
    logError("[audit] object_tags: special tags are metadata/filter labels; runtime effects go through EffectSpec and EffectDispatcher, not direct tag execution.");
    logError("[audit] stage_params: StageParams DB is intentionally not present for this pass.");
    logError("=== End dungeon generation extension pre-audit ===");
}

void logSpellRingShapeExtensionAudit()
{
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

    logError("=== Spell ring shape extension pre-audit ===");
    logError("[audit] orbit_calc: SpellRingSystem::update computes SpellRingItem::worldPosition via getRingItemWorldPosition().");
    logError("[audit] render/dig/hit/light/projectile: Game::render, DiggingSystem, EnemySystem, ProjectileSystem consume SpellRingItem::worldPosition.");
    logError("[audit] throw/offset: SpellRingSystem::center is driven by Player::spellRingShift (Input::ringOffsetHeld) and throw state transitions.");
    logError("[audit] ring_ui: Ring screen placement uses localAngle and findNearestRingPathParam()/getRingItemWorldPosition().");
    logError("[audit] save_load: ring lines persist item type/objectId/localAngle/instance data; load normalizes angles and falls back invalid item type to Object.");
    logError("[audit] ring_shape_save: ring_shape_1..3 keys persist RingShape; unknown/invalid values fall back to the ring's default shape.");
    logError("[audit] ring_slots: active ring index exists (0..2), ring runtime iterates all 3 slots via runtimeItems().");
    logError("[audit] orbit_data: radius/speed/weight/orbit modifiers are in SpellRingSystem; per-item orbit effects are applied in Game::refreshOrbitEffects.");
    logError("=== End spell ring shape extension pre-audit ===");
}

std::uint32_t makeDungeonSeed(int stageId, bool roguelike)
{
    std::random_device device;
    const std::uint32_t a = device();
    const std::uint32_t b = device();
    const std::uint32_t modeSalt = roguelike ? 0x9E3779B9u : 0x85EBCA6Bu;
    return a ^ (b << 1) ^ (static_cast<std::uint32_t>(std::max(1, stageId)) * 0x27D4EB2Du) ^ modeSalt;
}

float stageHardnessMultiplierForStageId(int stageId)
{
    switch (stageId) {
    case 1:
        return 1.0f;
    case 2:
        return 2.0f;
    case 3:
        return 3.5f;
    case 4:
        return 4.0f;
    default:
        return 1.0f + static_cast<float>(std::max(0, stageId - 1)) * 0.75f;
    }
}

StageDefinition makeCodeDefaultStageDefinition()
{
    StageDefinition stage;
    stage.id = "stage_01_stardust";
    stage.name = "星くずの浅坑";
    stage.type = "ストーリー";
    stage.displayOrder = 10;
    stage.implementationState = "code_default";
    stage.generationProfile = "natural_cave";
    stage.terrainProfile = "soft_stardust";
    stage.goalDistanceTiles = 320;
    stage.detourRate = 0.30;
    stage.branchDensity = 0.25;
    stage.cavernWidthMultiplier = 1.00;
    stage.terrainHardnessMultiplier = 1.00;
    stage.warpPointCount = 3;
    stage.specialRoomCount = 1;
    return stage;
}

void logCurrentStageDefinition(const StageDefinition& stage, std::string_view reason)
{
    logError(std::string("Current stage resolved") + (reason.empty() ? std::string{} : ": " + std::string(reason)));
    logError("  currentStageId=\"" + stage.id +
        "\" name=\"" + stage.name +
        "\" type=\"" + stage.type +
        "\" generation_profile=\"" + stage.generationProfile +
        "\" terrain_profile=\"" + stage.terrainProfile + "\"");
    logError("  goal_distance_tiles=" + std::to_string(stage.goalDistanceTiles) +
        " terrain_hardness_multiplier=" + std::to_string(stage.terrainHardnessMultiplier) +
        " warp_point_count=" + std::to_string(stage.warpPointCount) +
        " special_room_count=" + std::to_string(stage.specialRoomCount));
}

void logDbValidationReport(const ObjectCatalog& catalog)
{
    if (!dbDebugLogEnabled()) {
        return;
    }

    logError("=== Object DB validation report ===");
    logError("Objects: " + std::to_string(catalog.objects.size()));
    logError("Object IDs: " + std::to_string(catalog.objectsById.size()));
    logError("Effect codes: " + std::to_string(catalog.effectCodes.size()));
    logError("Special tags: " + std::to_string(catalog.specialTags.size()));
    logError("Errors: " + std::to_string(countSeverity(catalog, DbValidationSeverity::Error)));
    logError("Warnings: " + std::to_string(countSeverity(catalog, DbValidationSeverity::Warning)));
    logEffectTargetUsage(catalog);
    logIssueList(catalog, "EffectSpec syntax errors", DbValidationSeverity::Error, DbValidationCategory::EffectSpecSyntax);
    logIssueList(catalog, "Duplicate IDs", DbValidationSeverity::Warning, DbValidationCategory::DuplicateId);
    logIssueList(catalog, "Undefined effect codes", DbValidationSeverity::Warning, DbValidationCategory::UndefinedEffectCode);
    logIssueList(catalog, "Target mismatches", DbValidationSeverity::Warning, DbValidationCategory::TargetMismatch);
    logIssueList(catalog, "Undefined special tags", DbValidationSeverity::Warning, DbValidationCategory::UndefinedSpecialTag);
    logIssueList(catalog, "Exclusive tag conflicts", DbValidationSeverity::Warning, DbValidationCategory::ExclusiveTagConflict);
    logIssueList(catalog, "Deprecated or compatibility entries", DbValidationSeverity::Warning, DbValidationCategory::DeprecatedEntry);

    std::size_t otherCount = 0;
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        if (issue.category != DbValidationCategory::EffectSpecSyntax &&
            issue.category != DbValidationCategory::DuplicateId &&
            issue.category != DbValidationCategory::UndefinedEffectCode &&
            issue.category != DbValidationCategory::TargetMismatch &&
            issue.category != DbValidationCategory::UndefinedSpecialTag &&
            issue.category != DbValidationCategory::ExclusiveTagConflict &&
            issue.category != DbValidationCategory::DeprecatedEntry) {
            ++otherCount;
        }
    }
    logError("Other DB validation issues: " + std::to_string(otherCount));
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        if (issue.category != DbValidationCategory::EffectSpecSyntax &&
            issue.category != DbValidationCategory::DuplicateId &&
            issue.category != DbValidationCategory::UndefinedEffectCode &&
            issue.category != DbValidationCategory::TargetMismatch &&
            issue.category != DbValidationCategory::UndefinedSpecialTag &&
            issue.category != DbValidationCategory::ExclusiveTagConflict &&
            issue.category != DbValidationCategory::DeprecatedEntry) {
            logError("  [" + std::string(dbValidationSeverityName(issue.severity)) + "][" +
                std::string(dbValidationCategoryName(issue.category)) + "] " + issue.message);
        }
    }
    logError("=== End Object DB validation report ===");
}

std::size_t countEnemySeverity(const EnemyCatalog& catalog, DbValidationSeverity severity)
{
    return static_cast<std::size_t>(std::count_if(
        catalog.validationIssues.begin(),
        catalog.validationIssues.end(),
        [severity](const DbValidationIssue& issue) {
            return issue.severity == severity;
        }));
}

void logEnemyDbValidationReport(const EnemyCatalog& catalog)
{
    if (!dbDebugLogEnabled()) {
        return;
    }

    logError("=== Enemy DB validation report ===");
    logError("Enemies: " + std::to_string(catalog.enemies.size()));
    logError("Enemy IDs: " + std::to_string(catalog.enemiesById.size()));
    logError("Behaviors: " + std::to_string(catalog.behaviors.size()));
    logError("Behavior IDs: " + std::to_string(catalog.behaviorsById.size()));
    logError("Errors: " + std::to_string(countEnemySeverity(catalog, DbValidationSeverity::Error)));
    logError("Warnings: " + std::to_string(countEnemySeverity(catalog, DbValidationSeverity::Warning)));
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        logError("  [" + std::string(dbValidationSeverityName(issue.severity)) + "][" +
            std::string(dbValidationCategoryName(issue.category)) + "] " + issue.message);
    }
    logError("=== End Enemy DB validation report ===");
}

void logStageCatalogReport(const StageCatalog& catalog, bool loadedFromSheet, bool usedFallback, std::string_view error)
{
    logError("Stages sheet load " + std::string(loadedFromSheet ? "succeeded" : "failed"));
    if (!loadedFromSheet && !error.empty()) {
        logError("Stages sheet error: " + std::string(error));
    }
    logError("Stages fallback " + std::string(usedFallback ? "used" : "not used"));
    logError("Stages loaded: " + std::to_string(catalog.stages.size()));
    for (const StageDefinition& stage : catalog.getStagesSortedByDisplayOrder()) {
        logError("  stage id=\"" + stage.id +
            "\" name=\"" + stage.name +
            "\" generation_profile=\"" + stage.generationProfile +
            "\" terrain_profile=\"" + stage.terrainProfile + "\"");
    }
    logError("Stage validation warnings: " + std::to_string(catalog.validationWarnings.size()));
    for (const std::string& warning : catalog.validationWarnings) {
        logError("  [warning] " + warning);
    }
}

void logEffectDispatcherSmoke(const ObjectCatalog& catalog, const EffectDispatcher& dispatcher)
{
    if (!effectDebugLogEnabled()) {
        return;
    }

    logError("=== EffectDispatcher debug smoke ===");
    logError("Registered handlers: " + std::to_string(dispatcher.handlerCount()));
    for (const ObjectDefinition& object : catalog.objects) {
        EffectContext context;
        dispatcher.dispatchNormalEffects(object, context);
        context = EffectContext{};
        dispatcher.dispatchOrbitEffects(object, context);
    }
    logError("=== End EffectDispatcher debug smoke ===");
}
}

} // namespace majo
