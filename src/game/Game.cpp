#include "game/Game.hpp"

#include "engine/Log.hpp"
#include "engine/Ui.hpp"
#include "game/ActorVisual.hpp"
#include "game/InventoryUiCommon.hpp"
#include "game/ObjectImageRenderer.hpp"
#include "game/WorldIconRenderer.hpp"

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

namespace {
template <typename T>
void resetInPlace(T& value)
{
    // Avoid materializing large reset temporaries on the startup stack.
    std::destroy_at(std::addressof(value));
    std::construct_at(std::addressof(value));
}

constexpr int ShovelIconIndex = 0;
constexpr int TorchIconIndex = 1;
constexpr float IconDrawSize = 32.0f;
constexpr float PlayerSpriteDrawSize = 96.0f;
constexpr float PlayerSpriteAnchorX = 0.5f;
constexpr float PlayerSpriteAnchorY = 0.95f;
constexpr int BaseMenuItemCount = 8;
constexpr int BaseMiningStartChoiceCount = 3;
constexpr int BaseUpgradeItemCount = 8;
constexpr int BaseProcessingModeCount = 4;
constexpr int BaseRingWorkshopItemCount = 10;
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
constexpr float RingUiCometCenterYOffset = 104.0f;
constexpr float RingUiCometArcRotation = Pi * 1.5f;
constexpr float WarpPointSpacing = 320.0f;
constexpr float WarpPointTouchRadius = 28.0f;
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
constexpr float ExposedEnemyNodeSpawnRadius = 820.0f;
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

std::mt19937& lootRuntimeRng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
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
    case ScreenMode::Title: return "Title";
    case ScreenMode::Base: return "Base";
    case ScreenMode::WorldLoading: return "WorldLoading";
    case ScreenMode::Playing: return "Playing";
    case ScreenMode::PauseMenu: return "PauseMenu";
    case ScreenMode::Inventory: return "Inventory";
    case ScreenMode::Ring: return "Ring";
    case ScreenMode::ObjectImageScaleEdit: return "ObjectImageScaleEdit";
    case ScreenMode::LevelUp: return "LevelUp";
    case ScreenMode::GameOver: return "GameOver";
    case ScreenMode::StageClear: return "StageClear";
    }
    return "Unknown";
}

const char* baseAreaName(BaseArea area)
{
    switch (area) {
    case BaseArea::Outdoor: return "屋外拠点";
    case BaseArea::HomeInterior: return "主人公の家";
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
        BaseFacility{"mine_exit", "採掘出口", {{584.0f, 560.0f}, {112.0f, 64.0f}}, 78.0f, true, true, BaseFacilityAction::MineExit},
        BaseFacility{"storage_chest", "収納箱", {{158.0f, 320.0f}, {98.0f, 72.0f}}, 68.0f, true, true, BaseFacilityAction::Storage},
        BaseFacility{"merchant_wagon", "商人ワゴン", {{982.0f, 302.0f}, {150.0f, 90.0f}}, 78.0f, true, true, BaseFacilityAction::Merchant},
        BaseFacility{"processing_table", "作業台", {{930.0f, 128.0f}, {130.0f, 68.0f}}, 70.0f, true, true, BaseFacilityAction::Processing},
        BaseFacility{"upgrade_forge", "拠点強化炉", {{568.0f, 76.0f}, {144.0f, 74.0f}}, 76.0f, true, true, BaseFacilityAction::Forge},
        BaseFacility{"ring_workshop", "リング工房用スペース", {{902.0f, 520.0f}, {172.0f, 92.0f}}, 82.0f, true, ringWorkshopUnlocked, BaseFacilityAction::RingWorkshop},
        BaseFacility{"monica", "モニカ", {{760.0f, 230.0f}, {74.0f, 86.0f}}, 72.0f, true, true, BaseFacilityAction::MonicaTalk},
        BaseFacility{"home_entrance", "主人公の家の入口", {{382.0f, 158.0f}, {52.0f, 32.0f}}, 64.0f, true, true, BaseFacilityAction::HomeEntrance},
        BaseFacility{"home", "主人公の家", {{330.0f, 72.0f}, {154.0f, 100.0f}}, 0.0f, false, true, BaseFacilityAction::HomeEntrance},
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
        return "Enter: 主人公の家に入る / クリック: 近くの入口を調べる / Esc: メニュー";
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
        "無理はしないで。準備ができたら、採掘出口から向かおう。",
        false,
    });
    sequence.lines.push_back(DialogueLine{
        "monica",
        "モニカ",
        "",
        "チコリが反応したら、近くに星くずか古い魔導具があるかも。壁の色も見てみて。",
        false,
    });
    return sequence;
}

DialogueSequence firstDungeonDialogue()
{
    DialogueSequence sequence;
    sequence.id = "dungeon_first_monica";
    sequence.lines.push_back(DialogueLine{
        "monica",
        "モニカ",
        "",
        "通信はつながってる。暗い場所では、松明の光から離れすぎないで。",
        false,
    });
    sequence.lines.push_back(DialogueLine{
        "monica",
        "モニカ",
        "",
        "答えを全部は見通せないけど、異変の気配なら拾える。気になる壁は少し掘ってみよう。",
        false,
    });
    return sequence;
}

UiRect basePanelRect()
{
    return {{360.0f, 92.0f}, {560.0f, 536.0f}};
}

UiRect baseMenuItemRect(int index)
{
    return {{450.0f, 296.0f + static_cast<float>(index) * 36.0f}, {380.0f, 30.0f}};
}

UiRect baseMiningStartChoiceRect(int index)
{
    return {{450.0f, 318.0f + static_cast<float>(index) * 58.0f}, {380.0f, ui::ButtonHeight}};
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
    rect.pos.y += 48.0f;
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
    const int column = index / 4;
    const int row = index % 4;
    return {{392.0f + static_cast<float>(column) * 252.0f, 214.0f + static_cast<float>(row) * 58.0f}, {240.0f, ui::ButtonHeight}};
}

UiRect baseProcessingModeRect(int index)
{
    return {{96.0f + static_cast<float>(index) * 150.0f, 148.0f}, {142.0f, ui::ButtonHeight}};
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
    return {{
        StorageGridRightX - StoragePageButtonSize,
        StorageBottomHeaderY - 2.0f
    }, {StoragePageButtonSize, StoragePageButtonSize}};
}

UiRect storagePageTextRect()
{
    const UiRect next = storageNextPageButtonRect();
    return {{
        next.pos.x - StoragePageButtonGap - StoragePageTextWidth,
        StorageBottomHeaderY - 2.0f
    }, {StoragePageTextWidth, StoragePageButtonSize}};
}

UiRect storagePrevPageButtonRect()
{
    const UiRect page = storagePageTextRect();
    return {{
        page.pos.x - StoragePageButtonGap - StoragePageButtonSize,
        page.pos.y
    }, {StoragePageButtonSize, StoragePageButtonSize}};
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
    case 0: return "採掘出口";
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
    case 0: return "開始地点";
    case 1: return "最新ワープポイント";
    case 2: return "再生成";
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
    return spellRing.quantizeLocalAngle(param - spellRing.ringBaseAngleForIndex(spellRing.activeRingIndex()), balance);
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

float ringItemAltitude(const SpellRingItem& item, float totalSeconds)
{
    const float phase = totalSeconds * RingItemBobSpeed + item.localAngle * 1.7f;
    return RingItemBaseAltitude + std::sin(phase) * RingItemBobAmplitude;
}

Vec2 ringItemDrawPosition(const SpellRingItem& item, float totalSeconds)
{
    return elevatedDrawPosition(item.worldPosition, ringItemAltitude(item, totalSeconds));
}

float ringItemBaseShadowVisualSize(const SpellRingItem& item)
{
    switch (item.type) {
    case SpellRingItemType::Shovel:
    case SpellRingItemType::Torch:
        return IconDrawSize;
    case SpellRingItemType::Object:
        return std::max(RingObjectImageMaxSize, item.hitRadius * 2.0f);
    }
    return RingObjectImageMaxSize;
}

float ringItemShadowVisualSize(const SpellRingItem& item, float totalSeconds)
{
    return actorShadowVisualSizeForAltitude(ringItemBaseShadowVisualSize(item), ringItemAltitude(item, totalSeconds));
}

void drawRingItemShape(
    Renderer& renderer,
    const SpellRingItem& item,
    const ItemData* object,
    Vec2 center,
    Vec2 outward,
    bool selected,
    bool invalid = false)
{
    const Color outline = selected
        ? (invalid ? Color{255, 92, 92, 255} : Color{255, 230, 150, 255})
        : Color{96, 104, 126, 220};

    if (object != nullptr) {
        if (selected) {
            const ObjectImageDrawOptions selectedOutlineOptions = withSelectedItemOutline({}, outline, 6);
            if (drawObjectImage(
                    renderer,
                    *object,
                    center,
                    {RingObjectImageMaxSize, RingObjectImageMaxSize},
                    selectedOutlineOptions)) {
                // Keep the original 1px black outline and sprite on top.
                (void)drawObjectImage(
                    renderer,
                    *object,
                    center,
                    {RingObjectImageMaxSize, RingObjectImageMaxSize},
                    {});
                return;
            }
        } else if (drawObjectImage(
                       renderer,
                       *object,
                       center,
                       {RingObjectImageMaxSize, RingObjectImageMaxSize},
                       {})) {
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

void Game::initialize(int width, int height)
{
    camera_.setViewport(width, height);
    loadSheetSourceConfig();
    std::string message;
    loadBalanceFromSources(message);
    loadObjectsFromSheet();
    loadStagesFromSheet();
    resolveCurrentStageDefinition();
    loadEnemiesFromSheet();
    configureWatcher();
    resetWorldSimulationState();
    resetWorldUiState();
    resetWorldRunState();
    spellRing_.initialize(balance_);
    applyPermanentUpgrades();
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    if (loadSaveData()) {
        reloadNotice_ = "セーブ読込完了";
    } else {
        reloadNotice_ = message.empty() ? "データ読込完了" : message;
    }
    baseArea_ = BaseArea::Outdoor;
    basePlayerPosition_ = {640.0f, 360.0f};
    baseOutdoorPlayerPosition_ = basePlayerPosition_;
    basePlayerFacing_ = {0.0f, 1.0f};
    loadBaseEditData();
    loadObjectImageScaleData();
    setObjectImageScaleOverrides(&objectImageScaleById_);
    setWorldIconScaleOverrides(&otherImageScaleByKey_);
    loadOpeningKamishibaiData();
    std::string openingMetaMessage;
    openingMeta_ = openingMetaSave_.load(&openingMetaMessage);
    logInfo(
        "[opening] " + openingMetaMessage +
        " openingEverWatched=" + (openingMeta_.openingEverWatched ? std::string("true") : std::string("false")));
    enterBase();
    startOpeningKamishibai();
    reloadNoticeTimer_ = 2.0f;
}

void Game::initializeWorld(bool captureRunStartInventory)
{
    resetWorldSimulationState();
    resetWorldUiState();
    resetWorldRunState();
    buildWorldForRun(captureRunStartInventory);
}

void Game::resetWorldSimulationState()
{
    resetWorldPlayerState();
    resetWorldMapAndRingState();
    resetWorldActionSystems();
    resetWorldInventoryState();
}

void Game::resetWorldPlayerState()
{
    resetInPlace(player_);
    player_.position = {0.0f, 0.0f};
    player_.xpToNext = balance_.xpBase + player_.level * balance_.xpPerLevel;
}

void Game::resetWorldMapAndRingState()
{
    resetInPlace(tileMap_);
    resetInPlace(spellRing_);
}

void Game::resetWorldActionSystems()
{
    resetWorldEffectState();
    resetWorldEnemyState();
    resetWorldProjectileState();
    resetWorldDropState();
    resetWorldProgressionState();
}

void Game::resetWorldEffectState()
{
    resetInPlace(effects_);
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
}

void Game::resetWorldEnemyState()
{
    resetInPlace(enemies_);
}

void Game::resetWorldProjectileState()
{
    resetInPlace(projectiles_);
}

void Game::resetWorldDropState()
{
    resetInPlace(worldDrops_);
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
}

void Game::resetWorldProgressionState()
{
    resetInPlace(levels_);
    resetInPlace(upgrades_);
}

void Game::resetWorldInventoryState()
{
    resetInPlace(inventory_);
}

void Game::resetWorldUiState()
{
    mode_ = ScreenMode::Playing;
    baseMenuSelection_ = 0;
    baseMiningStartChoiceActive_ = false;
    baseMiningStartSelection_ = 0;
    baseStorageActive_ = false;
    baseStorageFocusWarehouse_ = false;
    baseStorageBackpackCursor_ = 0;
    baseStorageWarehouseCursor_ = 0;
    baseStorageWarehousePage_ = 0;
    closeUiCommandMenu(baseStorageCommandMenu_);
    baseStorageCommandSlot_ = -1;
    baseStoragePointerPressSlot_ = -1;
    baseStoragePointerPressMouse_ = {};
    baseStoragePointerPressCanOpenMenu_ = false;
    baseStoragePointerDragTriggered_ = false;
    baseStorageGrabbedActive_ = false;
    baseStorageGrabbedFromSlot_ = -1;
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    baseMerchantActionSelection_ = 0;
    baseSellSelection_ = 0;
    baseMerchantBuySelection_ = 0;
    closeUiCommandMenu(baseMerchantSellCommandMenu_);
    baseMerchantSellCommandIndex_ = -1;
    closeUiCommandMenu(baseMerchantBuyCommandMenu_);
    baseMerchantBuyCommandIndex_ = -1;
    baseUpgradeActive_ = false;
    baseUpgradeSelection_ = 0;
    baseProcessingActive_ = false;
    baseProcessingMode_ = 0;
    baseProcessingSelection_ = 0;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingCommandSlot_ = -1;
    baseRingWorkshopActive_ = false;
    baseRingWorkshopSelection_ = 0;
    ringWorkshopDraftRadiusPoints_ = levelRingRadiusPoints_;
    ringWorkshopDraftSpeedPoints_ = levelRingSpeedPoints_;
    baseBookshelfActive_ = false;
    bookshelfPage_ = BookshelfPage::Menu;
    bookshelfSelection_ = 0;
    baseEditEnabled_ = false;
    baseEditMode_ = BaseEditMode::None;
    baseEditDirty_ = false;
    baseEditUndoStack_.clear();
    baseEditRedoStack_.clear();
    resetBaseEditDragState();
    objectImageScaleReturnMode_ = ScreenMode::Playing;
    imageScaleEditTab_ = ImageScaleEditTab::Objects;
    objectImageScaleSelectedIndex_ = -1;
    otherImageScaleSelectedIndex_ = -1;
    objectImageScaleScrollOffset_ = 0.0f;
    otherImageScaleScrollOffset_ = 0.0f;
    objectImageScaleDirty_ = false;
    objectImageScaleStatus_.clear();
    enemyTestActive_ = false;
    enemyTestUiVisible_ = true;
    enemyTestDropdown_ = {};
    enemyTestSelectedIndex_ = 0;
    enemyTestStatus_.clear();
    baseStatus_.clear();
}

void Game::resetWorldRunState()
{
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    pauseMenuSelection_ = 0;
    pauseConfirmSelection_ = 0;
    ringSlotSelection_ = 0;
    ringGrabActive_ = false;
    ringGrabOrigin_ = -1;
    ringStatus_.clear();
    runStats_ = RunStats{};
    gameOverSelection_ = 0;
    gameOverStatus_.clear();
    bossSpawned_ = false;
    hasBossSpawnPoint_ = false;
    stageClearSelection_ = 0;
    stageClearStatus_.clear();
    inventoryReturnToPause_ = false;
    debugPaused_ = false;
    captureCooldown_ = 0.0f;
    currentStage_ = std::clamp(currentStage_, 0, std::max(0, unlockedStages_ - 1));
}

void Game::buildWorldForRun(bool captureRunStartInventory)
{
    generateDungeonLayoutForRun();
    resetWarpPointRunState();
    initializeMoonFragmentNodesFromWarpPoints();
    initializeRewardNodesFromLayout();
    initializeChestNodesFromLayout();
    initializeCrateNodesFromLayout();
    initializeEnemyNodesFromLayout();
    spellRing_.initialize(balance_);
    applyPermanentUpgrades();
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    // Future connection: TileMap chunk initialization will consult
    // currentStageDefinition().terrainProfile and terrainHardnessMultiplier.
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    logDungeonGenerationAudit();
    logSpellRingShapeExtensionAudit();
    if (captureRunStartInventory) {
        captureRunStartInventoryState();
    }
}

void Game::beginWorldBuildFromBase(
    bool useLatestWarpPoint,
    InventoryCarryState retainedInventory,
    int retainedLevel,
    int retainedXp,
    int retainedXpToNext)
{
    worldBuildJob_ = WorldBuildJob{};
    worldBuildJob_.active = true;
    worldBuildJob_.useLatestWarpPoint = useLatestWarpPoint;
    worldBuildJob_.retainedInventory = std::move(retainedInventory);
    worldBuildJob_.retainedLevel = retainedLevel;
    worldBuildJob_.retainedXp = retainedXp;
    worldBuildJob_.retainedXpToNext = retainedXpToNext;
    worldBuildJob_.step = WorldBuildStep::ResetSimulation;

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    baseMiningStartChoiceActive_ = false;
    baseRegenerateConfirmActive_ = false;
    baseStatus_.clear();
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
    mode_ = ScreenMode::WorldLoading;
}

void Game::updateWorldBuild(float dt)
{
    if (!worldBuildJob_.active) {
        return;
    }

    worldBuildJob_.elapsedSeconds += dt;
    advanceWorldBuildOneStep();
}

void Game::advanceWorldBuildOneStep()
{
    switch (worldBuildJob_.step) {
    case WorldBuildStep::None:
        worldBuildJob_.step = WorldBuildStep::ResetSimulation;
        break;
    case WorldBuildStep::ResetSimulation:
        resetWorldSimulationState();
        worldBuildJob_.step = WorldBuildStep::ResetUi;
        break;
    case WorldBuildStep::ResetUi:
        resetWorldUiState();
        mode_ = ScreenMode::WorldLoading;
        pauseReturnMode_ = ScreenMode::Base;
        worldBuildJob_.step = WorldBuildStep::ResetRun;
        break;
    case WorldBuildStep::ResetRun:
        resetWorldRunState();
        worldBuildJob_.step = WorldBuildStep::GenerateLayout;
        break;
    case WorldBuildStep::GenerateLayout:
        generateDungeonLayoutForRun();
        worldBuildJob_.step = WorldBuildStep::ResetWarpPoints;
        break;
    case WorldBuildStep::ResetWarpPoints:
        resetWarpPointRunState();
        worldBuildJob_.step = WorldBuildStep::InitializeMoonFragments;
        break;
    case WorldBuildStep::InitializeMoonFragments:
        initializeMoonFragmentNodesFromWarpPoints();
        worldBuildJob_.step = WorldBuildStep::InitializeRewards;
        break;
    case WorldBuildStep::InitializeRewards:
        initializeRewardNodesFromLayout();
        worldBuildJob_.step = WorldBuildStep::InitializeChests;
        break;
    case WorldBuildStep::InitializeChests:
        initializeChestNodesFromLayout();
        worldBuildJob_.step = WorldBuildStep::InitializeCrates;
        break;
    case WorldBuildStep::InitializeCrates:
        initializeCrateNodesFromLayout();
        worldBuildJob_.step = WorldBuildStep::InitializeEnemies;
        break;
    case WorldBuildStep::InitializeEnemies:
        initializeEnemyNodesFromLayout();
        worldBuildJob_.step = WorldBuildStep::InitializeRing;
        break;
    case WorldBuildStep::InitializeRing:
        spellRing_.initialize(balance_);
        applyPermanentUpgrades();
        spellRing_.applyObjectParameters(objectCatalog_);
        spellRing_.resetBaseWeightToCurrent();
        refreshOrbitEffects();
        worldBuildJob_.step = WorldBuildStep::WarmInitialTiles;
        break;
    case WorldBuildStep::WarmInitialTiles:
        tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
        logDungeonGenerationAudit();
        logSpellRingShapeExtensionAudit();
        worldBuildJob_.step = WorldBuildStep::Finalize;
        break;
    case WorldBuildStep::Finalize:
        finishWorldBuild();
        break;
    case WorldBuildStep::Done:
        worldBuildJob_.active = false;
        break;
    }
}

void Game::finishWorldBuild()
{
    WorldBuildJob job = std::move(worldBuildJob_);
    worldBuildJob_ = WorldBuildJob{};

    restoreInventoryCarryState(job.retainedInventory);
    player_.level = job.retainedLevel;
    player_.xp = job.retainedXp;
    player_.xpToNext = job.retainedXpToNext;
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    captureRunStartInventoryState();

    resetWarpPointRunState();
    initializeMoonFragmentNodesFromWarpPoints();
    if (job.useLatestWarpPoint) {
        const Vec2 startPosition = latestWarpPointStartPosition();
        latestWarpPointPosition_ = startPosition;
        hasLatestWarpPointPosition_ = true;
        player_.position = startPosition;
        rebuildUnlockedWarpPointsForStart(startPosition);
        tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
        captureRetrySnapshotAtWarpPoint();
    }

    baseEditEnabled_ = false;
    baseEditMode_ = BaseEditMode::None;
    resetBaseEditDragState();
    mode_ = ScreenMode::Playing;
    pauseReturnMode_ = ScreenMode::Playing;
    resetPlayerFootstepDust();
    camera_.follow(player_.position, 1.0f);
    maybeStartFirstDungeonDialogue();
}

int Game::worldBuildStepIndex() const
{
    switch (worldBuildJob_.step) {
    case WorldBuildStep::None: return 0;
    case WorldBuildStep::ResetSimulation: return 0;
    case WorldBuildStep::ResetUi: return 1;
    case WorldBuildStep::ResetRun: return 2;
    case WorldBuildStep::GenerateLayout: return 3;
    case WorldBuildStep::ResetWarpPoints: return 4;
    case WorldBuildStep::InitializeMoonFragments: return 5;
    case WorldBuildStep::InitializeRewards: return 6;
    case WorldBuildStep::InitializeChests: return 7;
    case WorldBuildStep::InitializeCrates: return 8;
    case WorldBuildStep::InitializeEnemies: return 9;
    case WorldBuildStep::InitializeRing: return 10;
    case WorldBuildStep::WarmInitialTiles: return 11;
    case WorldBuildStep::Finalize: return 12;
    case WorldBuildStep::Done: return worldBuildStepCount();
    }
    return 0;
}

int Game::worldBuildStepCount() const
{
    return 13;
}

float Game::worldBuildProgress() const
{
    if (!worldBuildJob_.active) {
        return 1.0f;
    }
    return std::clamp(
        static_cast<float>(worldBuildStepIndex()) / static_cast<float>(std::max(1, worldBuildStepCount())),
        0.0f,
        1.0f);
}

std::string Game::worldBuildStatusText() const
{
    switch (worldBuildJob_.step) {
    case WorldBuildStep::None:
    case WorldBuildStep::ResetSimulation:
        return "採掘状態を初期化中";
    case WorldBuildStep::ResetUi:
        return "画面状態を整理中";
    case WorldBuildStep::ResetRun:
        return "ラン情報を初期化中";
    case WorldBuildStep::GenerateLayout:
        return "ダンジョン形状を生成中";
    case WorldBuildStep::ResetWarpPoints:
        return "ワープポイントを配置中";
    case WorldBuildStep::InitializeMoonFragments:
        return "月のカケラを配置中";
    case WorldBuildStep::InitializeRewards:
        return "報酬を配置中";
    case WorldBuildStep::InitializeChests:
        return "宝箱を配置中";
    case WorldBuildStep::InitializeCrates:
        return "木箱を配置中";
    case WorldBuildStep::InitializeEnemies:
        return "敵の配置を準備中";
    case WorldBuildStep::InitializeRing:
        return "スペルリングを準備中";
    case WorldBuildStep::WarmInitialTiles:
        return "開始地点を読み込み中";
    case WorldBuildStep::Finalize:
        return "採掘へ移行中";
    case WorldBuildStep::Done:
        return "完了";
    }
    return "採掘準備中";
}

void Game::enterBase()
{
    worldBuildJob_ = WorldBuildJob{};
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    mode_ = ScreenMode::Base;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
    baseMiningStartChoiceActive_ = false;
    baseStorageActive_ = false;
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    closeUiCommandMenu(baseMerchantSellCommandMenu_);
    baseMerchantSellCommandIndex_ = -1;
    closeUiCommandMenu(baseMerchantBuyCommandMenu_);
    baseMerchantBuyCommandIndex_ = -1;
    baseUpgradeActive_ = false;
    baseProcessingActive_ = false;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingCommandSlot_ = -1;
    baseRingWorkshopActive_ = false;
    baseBookshelfActive_ = false;
    baseMenuSelection_ = std::clamp(baseMenuSelection_, 0, BaseMenuItemCount - 1);
    clearTemporaryPlayerState(true);
    resetPlayerFootstepDust();
    if (baseEditEnabled_) {
        baseEditMode_ = BaseEditMode::Facility;
        resetBaseEditDragState();
    }
}

void Game::loadOpeningKamishibaiData()
{
    KamishibaiLoader loader;
    KamishibaiLoadResult result = loader.load(openingKamishibaiDataPath());
    openingPages_ = std::move(result.pages);
    logInfo("[opening] kamishibai pages loaded: " + std::to_string(openingPages_.size()));
    for (const std::string& warning : result.warnings) {
        logWarning("[opening] " + warning);
    }
}

void Game::startOpeningKamishibai()
{
    if (openingPages_.empty()) {
        loadOpeningKamishibaiData();
    }
    openingPlayer_.start(openingPages_, openingMeta_.openingEverWatched);
    mode_ = ScreenMode::OpeningKamishibai;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
}

void Game::finishOpeningKamishibai(bool completedPlayback)
{
    if (completedPlayback && !openingMeta_.openingEverWatched) {
        openingMeta_.openingEverWatched = true;
        std::string error;
        if (openingMetaSave_.save(openingMeta_, &error)) {
            logInfo("[opening] openingEverWatched saved: " + openingMetaSave_.path().generic_string());
        } else {
            logError("[opening] " + error);
        }
    }
    mode_ = ScreenMode::Title;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
}

void Game::updateOpeningKamishibai(const Input& input, float dt)
{
    bool skipped = false;
    if (openingPlayer_.canSkipImmediately() &&
        (input.mouseLeftPressed() || input.confirmPressed() || input.useItemPressed())) {
        openingPlayer_.finishImmediately();
        skipped = true;
    }

    openingPlayer_.update(dt);
    if (openingPlayer_.finished()) {
        finishOpeningKamishibai(!skipped);
    }
}

void Game::updateTitleScreen(const Input& input, UiContext& ui)
{
    if (input.mouseLeftPressed() || input.confirmPressed() || input.useItemPressed()) {
        if (input.mouseLeftPressed()) {
            ui.consumePointer();
        }
        requestScreenTransition(ScreenTransitionTarget::Base);
    }
}

void Game::requestScreenTransition(ScreenTransitionTarget target)
{
    if (target == ScreenTransitionTarget::None || screenTransition_.active()) {
        return;
    }

    screenTransition_.target = target;
    screenTransition_.phase = ScreenTransitionPhase::FadingOut;
    screenTransition_.elapsed = 0.0f;
    screenTransition_.applied = false;
}

void Game::requestMiningStartTransition(bool useLatestWarpPoint, bool forceRegenerate)
{
    if (screenTransition_.active()) {
        return;
    }

    screenTransition_.target = ScreenTransitionTarget::MiningStart;
    screenTransition_.phase = ScreenTransitionPhase::FadingOut;
    screenTransition_.elapsed = 0.0f;
    screenTransition_.applied = false;
    screenTransition_.useLatestWarpPoint = useLatestWarpPoint;
    screenTransition_.forceRegenerate = forceRegenerate;
}

void Game::requestBaseAreaCrossfade(BaseArea targetArea, Vec2 playerPosition, Vec2 playerFacing, std::string status)
{
    if (screenTransition_.active()) {
        return;
    }

    screenTransition_.target = ScreenTransitionTarget::BaseArea;
    screenTransition_.phase = ScreenTransitionPhase::CrossFadeCapture;
    screenTransition_.elapsed = 0.0f;
    screenTransition_.applied = false;
    screenTransition_.targetBaseArea = targetArea;
    screenTransition_.targetBasePlayerPosition = playerPosition;
    screenTransition_.targetBasePlayerFacing = playerFacing;
    screenTransition_.targetBaseStatus = std::move(status);
}

void Game::updateScreenTransition(float dt)
{
    if (!screenTransition_.active()) {
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    switch (screenTransition_.phase) {
    case ScreenTransitionPhase::Idle:
        break;
    case ScreenTransitionPhase::CrossFadeCapture:
        break;
    case ScreenTransitionPhase::CrossFading:
        screenTransition_.elapsed += safeDt;
        if (screenTransition_.elapsed >= ScreenTransitionCrossFadeSeconds) {
            screenTransition_ = ScreenTransitionState{};
        }
        break;
    case ScreenTransitionPhase::FadingOut:
        screenTransition_.elapsed += safeDt;
        if (screenTransition_.elapsed >= ScreenTransitionFadeOutSeconds) {
            screenTransition_.elapsed = 0.0f;
            if (!screenTransition_.applied) {
                applyScreenTransitionTarget(screenTransition_.target);
                screenTransition_.applied = true;
            }
            screenTransition_.phase = ScreenTransitionPhase::HoldBlack;
        }
        break;
    case ScreenTransitionPhase::HoldBlack:
        screenTransition_.elapsed += safeDt;
        if (screenTransition_.target == ScreenTransitionTarget::MiningStart && worldBuildActive()) {
            updateWorldBuild(safeDt);
        }
        if (screenTransition_.elapsed >= ScreenTransitionBlackHoldSeconds && !worldBuildActive()) {
            screenTransition_.elapsed = 0.0f;
            screenTransition_.phase = ScreenTransitionPhase::FadingIn;
        }
        break;
    case ScreenTransitionPhase::FadingIn:
        screenTransition_.elapsed += safeDt;
        if (screenTransition_.elapsed >= ScreenTransitionFadeInSeconds) {
            screenTransition_ = ScreenTransitionState{};
        }
        break;
    }
}

void Game::applyScreenTransitionTarget(ScreenTransitionTarget target)
{
    switch (target) {
    case ScreenTransitionTarget::None:
        break;
    case ScreenTransitionTarget::Base:
        enterBase();
        break;
    case ScreenTransitionTarget::MiningStart:
        startMiningFromBase(screenTransition_.useLatestWarpPoint, screenTransition_.forceRegenerate);
        break;
    case ScreenTransitionTarget::BaseArea:
        baseArea_ = screenTransition_.targetBaseArea;
        basePlayerPosition_ = screenTransition_.targetBasePlayerPosition;
        basePlayerFacing_ = screenTransition_.targetBasePlayerFacing;
        resetPlayerFootstepDust();
        baseStatus_ = std::move(screenTransition_.targetBaseStatus);
        break;
    }
}

void Game::startMiningFromBase(bool useLatestWarpPoint, bool forceRegenerate)
{
    useLatestWarpPoint = useLatestWarpPoint && unlockedWarpPointCount_ > 0;
    InventoryCarryState retained = captureInventoryCarryState();
    const int retainedLevel = player_.level;
    const int retainedXp = player_.xp;
    const int retainedXpToNext = player_.xpToNext;
    const bool restoredDungeon = !forceRegenerate && restoreDungeonState(useLatestWarpPoint);
    if (!restoredDungeon) {
        if (forceRegenerate) {
            dungeonStates_.erase(currentStageId_);
            unlockedWarpPointCount_ = 0;
            hasLatestWarpPointPosition_ = false;
            latestWarpPointPosition_ = {};
        }
        beginWorldBuildFromBase(
            useLatestWarpPoint,
            std::move(retained),
            retainedLevel,
            retainedXp,
            retainedXpToNext);
        return;
    }
    restoreInventoryCarryState(retained);
    player_.level = retainedLevel;
    player_.xp = retainedXp;
    player_.xpToNext = retainedXpToNext;
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    captureRunStartInventoryState();
    if (useLatestWarpPoint) {
        const Vec2 startPosition = latestWarpPointStartPosition();
        latestWarpPointPosition_ = startPosition;
        hasLatestWarpPointPosition_ = true;
        player_.position = startPosition;
        tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
        captureRetrySnapshotAtWarpPoint();
    }
    baseEditEnabled_ = false;
    baseEditMode_ = BaseEditMode::None;
    resetBaseEditDragState();
    mode_ = ScreenMode::Playing;
    pauseReturnMode_ = ScreenMode::Playing;
    resetPlayerFootstepDust();
    camera_.follow(player_.position, 1.0f);
    maybeStartFirstDungeonDialogue();
}

void Game::applyPermanentUpgrades()
{
    player_.maxHp = 10 + maxHpUpgradeLevel_ * 2;
    player_.hp = std::min(player_.hp, player_.maxHp);
    player_.spellRingShiftDistanceBonus = effectiveRingShiftDistance() - balance_.spellRingShiftDistance;
    spellRing_.setRadius(effectiveInitialRingRadius(levelRingRadiusPoints_));
    spellRing_.setAngularSpeed(effectiveInitialRingSpeed(levelRingSpeedPoints_));
}

float Game::effectiveInitialRingRadius(int levelRadiusPoints) const
{
    const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringRadiusUpgradeLevel_) * 0.08f;
    const float workshopMultiplier = 1.0f + static_cast<float>(workshopInitialRadiusLevel_) * 0.05f;
    const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelRadiusPoints)));
    return balance_.spellRingRadius * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
}

float Game::effectiveInitialRingSpeed(int levelSpeedPoints) const
{
    const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringSpeedUpgradeLevel_) * 0.08f;
    const float workshopMultiplier = 1.0f + static_cast<float>(workshopInitialSpeedLevel_) * 0.05f;
    const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelSpeedPoints)));
    return balance_.spellRingSpeed * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
}

float Game::effectiveRingShiftDistance() const
{
    return balance_.spellRingShiftDistance + static_cast<float>(workshopShiftDistanceLevel_) * 8.0f;
}

void Game::configureWatcher()
{
    watcher_ = FileWatcher{};
    watcher_.watchPath("data");
    watcher_.reset();
}

void Game::loadSheetSourceConfig()
{
    std::string error;
    if (!loadGoogleSheetSourceConfig("data/google_sheet_source.cfg", sheetSource_, error)) {
        sheetSource_ = GoogleSheetSourceConfig{};
        logWarning("Google Sheet source disabled: " + error);
        return;
    }
}

bool Game::loadBalanceFromDisk(std::string& message)
{
    RuntimeBalance loaded;
    std::string error;
    const std::filesystem::path primary = "data/game_balance.cfg";
    const bool loadedLocal = loadRuntimeBalance(primary, loaded, error);
    if (loadedLocal) {
        balance_ = loaded;
        message = "ローカルデータ読込完了";
    } else {
        balance_ = RuntimeBalance{};
        message = "データ読込失敗";
        logError("Runtime balance load failed: " + error);
    }

    return loadedLocal;
}

bool Game::loadBalanceFromSources(std::string& message)
{
    const bool loadedLocal = loadBalanceFromDisk(message);

    if (!sheetSource_.enabled) {
        return loadedLocal;
    }

    RuntimeBalance sheetBalance;
    std::string sheetError;
    if (loadRuntimeBalanceFromGoogleSheet(sheetSource_, sheetBalance, sheetError)) {
        balance_ = sheetBalance;
        message = "シートデータ読込完了";
        return true;
    }

    logError("Google Sheet balance load failed: " + sheetError);
    if (loadedLocal) {
        message = "ローカルデータ読込完了";
        return true;
    }

    message = "シートデータ読込失敗";
    return false;
}

bool Game::loadObjectsFromSheet()
{
    if (!sheetSource_.enabled) {
        objectCatalog_ = ObjectCatalog{};
        rebuildObjectImageScaleList();
        return false;
    }

    ObjectCatalog loaded;
    std::string error;
    if (!loadObjectCatalogFromGoogleSheet(sheetSource_, loaded, error)) {
        logError("Objects sheet load failed: " + error);
        return false;
    }

    objectCatalog_ = std::move(loaded);
    effectDispatcher_.clearHandlers();
    effectDispatcher_.registerFoundationHandlers(objectCatalog_);
    logDbValidationReport(objectCatalog_);
    logEffectDispatcherSmoke(objectCatalog_, effectDispatcher_);
    logError("Objects sheet loaded: " + std::to_string(objectCatalog_.registry.size()) + " items");
    logError("Objects loot weight columns detected: " + std::to_string(objectCatalog_.lootWeightStats.detectedColumnCount));
    logError("Objects loot weighted items: " + std::to_string(objectCatalog_.lootWeightStats.weightedItemCount));
    logError("Objects loot weight warnings: " + std::to_string(objectCatalog_.lootWeightStats.warningCount));
    const std::vector<const ItemData*> weapons = objectCatalog_.registry.findByCategory("\xE6\xAD\xA6\xE5\x99\xA8");
    const std::vector<const ItemData*> consumables = objectCatalog_.registry.findByTag("consumable");
    logError("Objects registry check: weapons=" + std::to_string(weapons.size()) +
        " consumable=" + std::to_string(consumables.size()));
    logError("Effect code sheet loaded: " + std::to_string(objectCatalog_.effectCodes.size()) + " codes");
    std::size_t effectCodeLogCount = 0;
    for (const auto& [code, definition] : objectCatalog_.effectCodes) {
        if (effectCodeLogCount >= 5) {
            break;
        }
        std::ostringstream line;
        line << "  effect_code=\"" << code
            << "\" name=\"" << definition.displayName
            << "\" category=\"" << definition.category
            << "\" targets=" << definition.targets.size()
            << " status=\"" << definition.implementationState << "\"";
        logError(line.str());
        ++effectCodeLogCount;
    }
    logError("Special tag sheet loaded: " + std::to_string(objectCatalog_.specialTags.size()) + " tags");
    std::size_t specialTagLogCount = 0;
    for (const auto& [tag, definition] : objectCatalog_.specialTags) {
        if (specialTagLogCount >= 5) {
            break;
        }
        std::ostringstream line;
        line << "  special_tag=\"" << tag
            << "\" name=\"" << definition.displayName
            << "\" category=\"" << definition.category
            << "\" target_categories=" << definition.targetCategories.size()
            << " related_effect_codes=" << definition.relatedEffectCodes.size()
            << " status=\"" << definition.implementationState << "\"";
        logError(line.str());
        ++specialTagLogCount;
    }
    logError("Objects by ID map loaded: " + std::to_string(objectCatalog_.objectsById.size()) + " unique IDs");
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        std::ostringstream line;
        line << "  item id=\"" << object.id
            << "\" name=\"" << object.name
            << "\" category=\"" << object.category
            << "\" rarity=" << object.rarity
            << " price=" << object.price
            << " attack_power=" << object.attackPower
            << " damage_type=\"" << object.damageType
            << "\" dig_power=" << object.digPower
            << " durability=" << object.durability
            << " weight_kg=" << object.weightKg
            << " image_number=" << object.imageNumber
            << " normal_effects=" << object.normalEffects.size()
            << " orbit_effects=" << object.orbitEffects.size()
            << " tags=" << object.tags.size()
            << " loot_weights=" << object.lootWeights.byColumn.size();
        logError(line.str());
    }
    rebuildObjectImageScaleList();
    return true;
}

bool Game::loadStagesFromSheet()
{
    if (!sheetSource_.enabled) {
        stageCatalog_.loadDefaultStages();
        stageCatalog_.validationWarnings.push_back("Google Sheet source is disabled; using default stages");
        logStageCatalogReport(stageCatalog_, false, true, "Google Sheet source is disabled");
        return false;
    }

    StageCatalog loaded;
    std::string error;
    if (!loadStageCatalogFromGoogleSheet(sheetSource_, loaded, error)) {
        std::vector<std::string> warnings = std::move(loaded.validationWarnings);
        stageCatalog_.loadDefaultStages();
        stageCatalog_.validationWarnings = std::move(warnings);
        stageCatalog_.validationWarnings.push_back("Stages sheet fallback used: " + error);
        logError("Stages sheet load failed: " + error);
        logStageCatalogReport(stageCatalog_, false, true, error);
        return false;
    }

    stageCatalog_ = std::move(loaded);
    logStageCatalogReport(stageCatalog_, true, false, {});
    return true;
}

const StageDefinition& Game::currentStageDefinition() const
{
    return currentStageDefinition_;
}

void Game::resolveCurrentStageDefinition()
{
    if (const StageDefinition* stage = stageCatalog_.getStageById(currentStageId_)) {
        currentStageDefinition_ = *stage;
        logCurrentStageDefinition(currentStageDefinition_, {});
        return;
    }

    if (!stageCatalog_.stages.empty()) {
        std::vector<StageDefinition> sorted = stageCatalog_.getStagesSortedByDisplayOrder();
        currentStageDefinition_ = sorted.front();
        logError("[warning] currentStageId=\"" + currentStageId_ +
            "\" was not found in StageCatalog; using first display-order stage \"" +
            currentStageDefinition_.id + "\"");
        currentStageId_ = currentStageDefinition_.id;
        logCurrentStageDefinition(currentStageDefinition_, "fallback_to_catalog_first");
        return;
    }

    logError("[warning] StageCatalog is empty; using code default stage_01_stardust");
    currentStageDefinition_ = makeCodeDefaultStageDefinition();
    currentStageId_ = currentStageDefinition_.id;
    logCurrentStageDefinition(currentStageDefinition_, "fallback_to_code_default");
}

bool Game::loadEnemiesFromSheet()
{
    if (!sheetSource_.enabled) {
        enemyCatalog_ = EnemyCatalog{};
        return false;
    }

    EnemyCatalog loaded;
    std::string error;
    if (!loadEnemyCatalogFromGoogleSheet(sheetSource_, objectCatalog_.specialTags, loaded, error)) {
        logError("Enemies sheet load failed: " + error);
        return false;
    }

    enemyCatalog_ = std::move(loaded);
    for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
        upsertObjectDefinition(objectCatalog_, makeCapturedObjectDefinition(enemy));
    }
    logEnemyDbValidationReport(enemyCatalog_);
    logError("Enemies sheet loaded: " + std::to_string(enemyCatalog_.enemies.size()) +
        " enemies, " + std::to_string(enemyCatalog_.enemiesById.size()) + " unique IDs");
    logError("Behavior sheet loaded: " + std::to_string(enemyCatalog_.behaviors.size()) +
        " behaviors, " + std::to_string(enemyCatalog_.behaviorsById.size()) + " unique IDs");

    std::size_t enemyLogCount = 0;
    for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
        if (enemyLogCount >= 5) {
            break;
        }
        std::ostringstream line;
        line << "  enemy id=\"" << enemy.id
            << "\" name=\"" << enemy.name
            << "\" hp=" << enemy.hp
            << " contact_attack=" << enemy.contactAttackPower
            << " damage_type=\"" << enemy.contactDamageType
            << "\" move_speed=" << enemy.moveSpeed
            << " radius=" << enemy.radius
            << " xp=" << enemy.xp
            << " enemy_behaviors=" << enemy.enemyBehaviorIds.size()
            << " captured_behaviors=" << enemy.capturedBehaviorIds.size()
            << " tags=" << enemy.enemyTags.size()
            << " captured_tags=" << enemy.capturedTags.size();
        logError(line.str());
        ++enemyLogCount;
    }

    if (!enemyCatalog_.validationWarnings.empty()) {
        logError("Enemy DB warnings: " + std::to_string(enemyCatalog_.validationWarnings.size()));
        for (const std::string& warning : enemyCatalog_.validationWarnings) {
            logError("  [warning] " + warning);
        }
    }
    return true;
}

bool Game::isSellableObject(const ItemData& item) const
{
    return !isStoryObject(item);
}

bool Game::isStoryObject(const ItemData& item) const
{
    if (item.category == "\xE3\x82\xB9\xE3\x83\x88\xE3\x83\xBC\xE3\x83\xAA\xE3\x83\xBC") {
        return true;
    }
    for (const std::string& tag : item.tags) {
        if (tag == "story" || tag == "story_item" || tag == "key_item" || tag == "unsellable" || tag == "no_sell") {
            return true;
        }
    }
    return false;
}

int Game::sellPrice(const ItemData& item, const ItemInstance* /*instance*/) const
{
    // Future hooks: enhancement bonus, broken penalty, category multiplier, merchant buy-rate.
    double multiplier = 1.0;
    if (merchantUpgradeLevel_ >= 6) {
        multiplier = 1.2;
    } else if (merchantUpgradeLevel_ >= 3) {
        multiplier = 1.1;
    }
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(item.price) * multiplier)));
}

bool Game::merchantProductCanFit(const ItemData* item) const
{
    if (item == nullptr) {
        return false;
    }
    const auto& stacks = inventory_.objectStacks();
    const bool existingStack = std::any_of(stacks.begin(), stacks.end(), [&](const InventoryObjectStack& stack) {
        return stack.objectId == item->id;
    });
    return existingStack || backpackUsedSlots() < inventory_.screenSlotCount();
}

bool Game::canBuyMerchantProduct(const MerchantProduct& product) const
{
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    return product.quantity > 0 && item != nullptr && money_ >= product.price && merchantProductCanFit(item);
}

std::vector<Game::SellableEntry> Game::sellableObjects() const
{
    std::vector<SellableEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(i)];
        if (stack.count <= 0) {
            continue;
        }
        SellableEntry entry{SellableKind::Stack, i};
        entry.price = sellPrice(stack.item);
        entry.sellable = true;
        entries.push_back(std::move(entry));
    }
    const auto& instances = inventory_.objectInstances();
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(i)];
        SellableEntry entry{SellableKind::Instance, i};
        entry.price = sellPrice(instance.item, &instance.instance);
        entry.sellable = !instance.instance.protectionEnabled;
        if (!entry.sellable) {
            entry.blockedReason = "保護中";
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}
void Game::refreshMerchantStock(bool force)
{
    if (!force && !merchantStock_.empty()) {
        return;
    }

    std::vector<const ItemData*> candidates;
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const ItemData* item = objectCatalog_.registry.findById(object.id);
        if (item == nullptr || item->id.empty() || isStoryObject(*item)) {
            continue;
        }
        const bool basicCategory =
            item->category == "\xE5\x9B\x9E\xE5\xBE\xA9" ||
            item->category == "\xE6\x8E\xA2\xE7\xB4\xA2" ||
            item->category == "\xE5\xBC\xB7\xE5\x8C\x96";
        const bool basicTag = std::any_of(item->tags.begin(), item->tags.end(), [](const std::string& tag) {
            return tag == "consumable" || tag == "potion" || tag == "food";
        });
        if ((basicCategory || basicTag) && item->price > 0) {
            candidates.push_back(item);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const ItemData* left, const ItemData* right) {
        if (left->price != right->price) {
            return left->price < right->price;
        }
        return left->id < right->id;
    });

    merchantStock_.clear();
    if (candidates.empty()) {
        return;
    }

    ++merchantStockVersion_;
    const int stockCount = std::min(merchantUpgradeLevel_ >= 2 ? 5 : 4, static_cast<int>(candidates.size()));
    const int start = merchantStockVersion_ % static_cast<int>(candidates.size());
    std::mt19937& rng = lootRuntimeRng();
    std::uniform_int_distribution<int> quantityDistribution(1, 5);
    for (int i = 0; i < stockCount; ++i) {
        const ItemData* item = candidates[static_cast<std::size_t>((start + i) % static_cast<int>(candidates.size()))];
        merchantStock_.push_back(MerchantProduct{item->id, std::max(1, item->price), quantityDistribution(rng)});
    }
}

void Game::sellMerchantEntry(int index, int count)
{
    const std::vector<SellableEntry> sellable = sellableObjects();
    if (index < 0 || index >= static_cast<int>(sellable.size())) {
        baseStatus_ = "売却対象がありません";
        return;
    }

    const SellableEntry entry = sellable[static_cast<std::size_t>(index)];
    if (!entry.sellable) {
        baseStatus_ = entry.blockedReason.empty() ? "売れません" : entry.blockedReason;
        return;
    }

    bool sold = false;
    int soldCount = 1;
    if (entry.kind == SellableKind::Stack) {
        const auto& stacks = inventory_.objectStacks();
        if (entry.index < 0 || entry.index >= static_cast<int>(stacks.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(entry.index)];
        soldCount = count <= 0 ? stack.count : std::min(count, stack.count);
        sold = inventory_.removeObjectItemCount(stack.objectId, soldCount);
    } else {
        const auto& instances = inventory_.objectInstances();
        if (entry.index < 0 || entry.index >= static_cast<int>(instances.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(entry.index)];
        sold = inventory_.removeObjectInstance(instance.instance.instanceId);
    }

    if (sold) {
        money_ += entry.price * std::max(1, soldCount);
        baseStatus_ = "売却しました";
        baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, static_cast<int>(sellableObjects().size()) - 1));
    }
}
void Game::sellMerchantScreenSlot(int slotIndex, int count)
{
    if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
        baseStatus_ = "売却対象がありません";
        return;
    }

    if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slotIndex)) {
        const int soldCount = count <= 0 ? stack->count : std::min(count, stack->count);
        const std::string objectId = stack->objectId;
        const int price = sellPrice(stack->item) * std::max(1, soldCount);
        if (inventory_.removeObjectItemCount(objectId, soldCount)) {
            money_ += price;
            baseStatus_ = "売却しました";
        }
        return;
    }

    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slotIndex)) {
        if (instance->instance.protectionEnabled) {
            baseStatus_ = "保護中";
            return;
        }
        const std::string instanceId = instance->instance.instanceId;
        const int price = sellPrice(instance->item, &instance->instance);
        if (inventory_.removeObjectInstance(instanceId)) {
            money_ += price;
            baseStatus_ = "売却しました";
        }
        return;
    }

    baseStatus_ = "売却対象がありません";
}
void Game::buyMerchantProduct(int index)
{
    refreshMerchantStock(false);
    if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
        baseStatus_ = "購入できる商品がありません";
        return;
    }

    MerchantProduct& product = merchantStock_[static_cast<std::size_t>(index)];
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    if (item == nullptr) {
        baseStatus_ = "商品データがありません";
        return;
    }
    if (product.quantity <= 0) {
        baseStatus_ = "品切れです";
        return;
    }
    if (money_ < product.price) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (!merchantProductCanFit(item)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    if (!inventory_.addObjectItem(objectCatalog_, product.objectId)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    money_ -= product.price;
    --product.quantity;
    baseStatus_ = product.quantity <= 0 ? "購入しました（品切れ）" : "購入しました";
}

std::vector<Game::StorageEntry> Game::processingEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

std::optional<Game::StorageEntry> Game::processingEntryForScreenSlot(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
        return std::nullopt;
    }
    if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slotIndex)) {
        const auto& stacks = inventory_.objectStacks();
        for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
            if (stacks[static_cast<std::size_t>(i)].objectId == stack->objectId) {
                return StorageEntry{StorageEntryKind::Stack, i};
            }
        }
    }
    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slotIndex)) {
        const auto& instances = inventory_.objectInstances();
        for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
            if (instances[static_cast<std::size_t>(i)].instance.instanceId == instance->instance.instanceId) {
                return StorageEntry{StorageEntryKind::Instance, i};
            }
        }
    }
    return std::nullopt;
}

const char* Game::processingModeName(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Repair: return "修理";
    case ProcessingMode::Attack: return "攻撃力強化";
    case ProcessingMode::Dig: return "抑制力強化";
    case ProcessingMode::Durability: return "耐久力強化";
    }
    return "";
}

bool Game::processingEntryAvailable(StorageEntry entry) const
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    if (entry.kind == StorageEntryKind::Stack) {
        return mode != ProcessingMode::Repair;
    }
    const InventoryObjectInstance& object = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    if (mode == ProcessingMode::Repair) {
        const ItemInstance& instance = object.instance;
        return instance.maxDurability >= 0 && (instance.isBroken || instance.currentDurability < instance.maxDurability);
    }
    return object.instance.enhanceLevel < MaxItemEnhanceLevel;
}

bool Game::processingScreenSlotAvailable(int slotIndex) const
{
    const std::optional<StorageEntry> entry = processingEntryForScreenSlot(slotIndex);
    return entry.has_value() && processingEntryAvailable(*entry);
}

int Game::processingMoneyCost(StorageEntry entry, ProcessingMode mode) const
{
    const ItemData* item = storageEntryItem(entry, false);
    const int basePrice = std::max(1, item != nullptr ? item->price : 0);
    if (mode == ProcessingMode::Repair) {
        const ItemInstance* instance = storageEntryInstance(entry, false);
        if (instance == nullptr || instance->maxDurability <= 0) {
            return 0;
        }
        if (instance->isBroken) {
            return std::max(1, static_cast<int>(std::ceil(static_cast<double>(basePrice) * 0.6)));
        }
        const int missing = std::max(0, instance->maxDurability - instance->currentDurability);
        if (missing <= 0) {
            return 0;
        }
        const double ratio = static_cast<double>(missing) / static_cast<double>(instance->maxDurability);
        return std::max(1, static_cast<int>(std::ceil(static_cast<double>(basePrice) * ratio * 0.4)));
    }

    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, false)) {
        enhanceLevel = instance->enhanceLevel;
    }
    return std::max(20, basePrice / 2 + (enhanceLevel + 1) * 50);
}

int Game::processingOreCost(StorageEntry entry, ProcessingMode mode) const
{
    if (mode == ProcessingMode::Repair) {
        return 0;
    }
    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, false)) {
        enhanceLevel = instance->enhanceLevel;
    }
    return enhanceLevel + 1;
}

void Game::applyProcessing(int entryIndex)
{
    const std::vector<StorageEntry> entries = processingEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    applyProcessingEntry(entry);
}

void Game::applyProcessingScreenSlot(int slotIndex)
{
    const std::optional<StorageEntry> entry = processingEntryForScreenSlot(slotIndex);
    if (!entry) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    applyProcessingEntry(*entry);
}

void Game::applyProcessingEntry(StorageEntry entry)
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    if (!processingEntryAvailable(entry)) {
        if (mode == ProcessingMode::Repair && entry.kind == StorageEntryKind::Stack) {
            baseStatus_ = "この作業はできません";
        } else {
            baseStatus_ = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
        }
        return;
    }

    const int moneyCost = processingMoneyCost(entry, mode);
    const int oreCost = processingOreCost(entry, mode);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りません";
        return;
    }

    bool processed = false;
    if (mode == ProcessingMode::Repair) {
        const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
        processed = inventory_.repairObjectInstance(instance.instance.instanceId);
    } else {
        int attackBonus = 0;
        int digBonus = 0;
        int durabilityBonus = 0;
        if (mode == ProcessingMode::Attack) {
            attackBonus = 1;
        } else if (mode == ProcessingMode::Dig) {
            digBonus = 1;
        } else if (mode == ProcessingMode::Durability) {
            durabilityBonus = 2;
        }
        if (entry.kind == StorageEntryKind::Stack) {
            const InventoryObjectStack& stack = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectStackItem(stack.objectId, attackBonus, digBonus, durabilityBonus, MaxItemEnhanceLevel);
        } else {
            const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectInstance(instance.instance.instanceId, attackBonus, digBonus, durabilityBonus, MaxItemEnhanceLevel);
        }
    }
    if (!processed) {
        baseStatus_ = "加工できません";
        return;
    }

    money_ -= moneyCost;
    if (oreCost > 0) {
        const bool spentOre = inventory_.materials().spend(MaterialType::EnhancementOre, oreCost);
        (void)spentOre;
    }
    baseStatus_ = mode == ProcessingMode::Repair ? "修理しました" : "強化しました";
    baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
}

int Game::warehouseCapacity() const
{
    constexpr std::array<int, 5> Capacities{{48, 72, 100, 140, 200}};
    const int level = std::clamp(warehouseCapacityLevel_, 0, static_cast<int>(Capacities.size()) - 1);
    return Capacities[static_cast<std::size_t>(level)];
}

int Game::warehouseUsedSlots() const
{
    return static_cast<int>(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
}

int Game::backpackUsedSlots() const
{
    return static_cast<int>(inventory_.objectStacks().size() + inventory_.objectInstances().size());
}

std::vector<Game::StorageEntry> Game::backpackStorageEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

std::vector<Game::StorageEntry> Game::warehouseStorageEntries() const
{
    std::vector<StorageEntry> entries;
    entries.reserve(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
    for (int i = 0; i < static_cast<int>(warehouseObjectStacks_.size()); ++i) {
        if (warehouseObjectStacks_[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(warehouseObjectInstances_.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

void Game::syncWarehouseDisplaySlots() const
{
    const int totalCount = warehouseUsedSlots();
    if (totalCount <= 0) {
        warehouseDisplaySlots_.clear();
        return;
    }

    const int capacity = warehouseCapacity();
    std::vector<int> nextSlots(static_cast<std::size_t>(totalCount), -1);
    std::vector<bool> used(static_cast<std::size_t>(capacity), false);
    const int copyCount = std::min(totalCount, static_cast<int>(warehouseDisplaySlots_.size()));
    for (int i = 0; i < copyCount; ++i) {
        const int slot = warehouseDisplaySlots_[static_cast<std::size_t>(i)];
        if (slot >= 0 && slot < capacity && !used[static_cast<std::size_t>(slot)]) {
            nextSlots[static_cast<std::size_t>(i)] = slot;
            used[static_cast<std::size_t>(slot)] = true;
        }
    }

    int cursor = 0;
    for (int i = 0; i < totalCount; ++i) {
        if (nextSlots[static_cast<std::size_t>(i)] >= 0) {
            continue;
        }
        while (cursor < capacity && used[static_cast<std::size_t>(cursor)]) {
            ++cursor;
        }
        if (cursor >= capacity) {
            nextSlots[static_cast<std::size_t>(i)] = i % capacity;
        } else {
            nextSlots[static_cast<std::size_t>(i)] = cursor;
            used[static_cast<std::size_t>(cursor)] = true;
            ++cursor;
        }
    }
    warehouseDisplaySlots_ = std::move(nextSlots);
}

int Game::warehouseEntryIndexAtStorageSlot(int slot) const
{
    syncWarehouseDisplaySlots();
    if (slot < 0 || slot >= warehouseCapacity()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(warehouseDisplaySlots_.size()); ++i) {
        if (warehouseDisplaySlots_[static_cast<std::size_t>(i)] == slot) {
            return i;
        }
    }
    return -1;
}

void Game::assignWarehouseEntryToStorageSlot(int entryIndex, int slot)
{
    syncWarehouseDisplaySlots();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseDisplaySlots_.size()) || slot < 0 || slot >= warehouseCapacity()) {
        return;
    }
    for (int i = 0; i < static_cast<int>(warehouseDisplaySlots_.size()); ++i) {
        if (i != entryIndex && warehouseDisplaySlots_[static_cast<std::size_t>(i)] == slot) {
            std::swap(warehouseDisplaySlots_[static_cast<std::size_t>(i)], warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)]);
            return;
        }
    }
    warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)] = slot;
}

void Game::removeWarehouseDisplaySlotAtEntryIndex(int entryIndex)
{
    syncWarehouseDisplaySlots();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseDisplaySlots_.size())) {
        return;
    }
    warehouseDisplaySlots_.erase(warehouseDisplaySlots_.begin() + entryIndex);
}

void Game::depositBackpackEntry(int entryIndex)
{
    const std::vector<StorageEntry> entries = backpackStorageEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "預けるアイテムがありません";
        return;
    }

    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    if (entry.kind == StorageEntryKind::Stack) {
        const InventoryObjectStack& source = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
        auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [&source](const InventoryObjectStack& stack) {
            return stack.objectId == source.objectId;
        });
        if (it == warehouseObjectStacks_.end()) {
            if (warehouseUsedSlots() >= warehouseCapacity()) {
                baseStatus_ = "倉庫がいっぱいです";
                return;
            }
            syncWarehouseDisplaySlots();
            const int newStackIndex = static_cast<int>(warehouseObjectStacks_.size());
            warehouseDisplaySlots_.insert(warehouseDisplaySlots_.begin() + newStackIndex, -1);
            warehouseObjectStacks_.push_back(InventoryObjectStack{source.item, 0});
            it = warehouseObjectStacks_.end() - 1;
        }
        const std::string objectId = source.objectId;
        if (!inventory_.removeObjectItemCount(objectId, 1)) {
            baseStatus_ = "預け入れに失敗しました";
            return;
        }
        ++it->count;
        baseStatus_.clear();
        baseStorageBackpackCursor_ = std::clamp(baseStorageBackpackCursor_, 0, StoragePaneSlotCount - 1);
        return;
    }

    if (warehouseUsedSlots() >= warehouseCapacity()) {
        baseStatus_ = "倉庫がいっぱいです";
        return;
    }
    const InventoryObjectInstance& source = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    InventoryObjectInstance moved;
    if (!inventory_.takeObjectInstance(source.instance.instanceId, moved)) {
        baseStatus_ = "預け入れに失敗しました";
        return;
    }
    warehouseObjectInstances_.push_back(std::move(moved));
    baseStatus_.clear();
    baseStorageBackpackCursor_ = std::clamp(baseStorageBackpackCursor_, 0, StoragePaneSlotCount - 1);
}

void Game::withdrawWarehouseEntry(int entryIndex)
{
    const std::vector<StorageEntry> entries = warehouseStorageEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "取り出すアイテムがありません";
        return;
    }

    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    if (entry.kind == StorageEntryKind::Stack) {
        InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(entry.index)];
        const std::string objectId = stack.objectId;
        if (!inventory_.addObjectItem(objectCatalog_, objectId)) {
            baseStatus_ = "リュックがいっぱいです";
            return;
        }
        --stack.count;
        if (stack.count <= 0) {
            removeWarehouseDisplaySlotAtEntryIndex(entry.index);
            warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + entry.index);
        }
        baseStatus_.clear();
        baseStorageWarehouseCursor_ = std::clamp(baseStorageWarehouseCursor_, 0, StoragePaneSlotCount - 1);
        return;
    }

    InventoryObjectInstance moved = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)];
    if (!inventory_.addObjectInstance(objectCatalog_, moved.instance)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + entry.index);
    warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + entry.index);
    baseStatus_.clear();
    baseStorageWarehouseCursor_ = std::clamp(baseStorageWarehouseCursor_, 0, StoragePaneSlotCount - 1);
}

std::string Game::storageEntryLabel(StorageEntry entry, bool warehouseEntry) const
{
    char buffer[192];
    if (entry.kind == StorageEntryKind::Stack) {
        const InventoryObjectStack& stack = warehouseEntry
            ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)]
            : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
        std::snprintf(buffer, sizeof(buffer), "%s x%d", stack.item.name.c_str(), stack.count);
        return buffer;
    }

    const InventoryObjectInstance& instance = warehouseEntry
        ? warehouseObjectInstances_[static_cast<std::size_t>(entry.index)]
        : inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    std::snprintf(buffer, sizeof(buffer), "%s %s%s Lv.%d",
        instance.item.name.c_str(),
        instance.instance.protectionEnabled ? "[保護] " : "",
        instance.instance.isBroken ? "[破損] " : "",
        instance.instance.enhanceLevel);
    return buffer;
}

const ItemData* Game::storageEntryItem(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind == StorageEntryKind::Stack) {
        return warehouseEntry
            ? &warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].item
            : &inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].item;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].item
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].item;
}

const ItemInstance* Game::storageEntryInstance(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Instance) {
        return nullptr;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].instance;
}

int Game::storageEntryStackCount(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Stack) {
        return 1;
    }
    return warehouseEntry
        ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].count
        : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].count;
}

int Game::upgradeCost(int index) const
{
    switch (index) {
    case 0: return 150 + warehouseCapacityLevel_ * 100;
    case 1: return 120 + (merchantUpgradeLevel_ - 1) * 120;
    case 2: return 180 + processingUnlockLevel_ * 140;
    case 3: return 300;
    case 4: return 100 + maxHpUpgradeLevel_ * 50;
    case 5: return 150 + ringRadiusUpgradeLevel_ * 75;
    case 6: return 150 + ringSpeedUpgradeLevel_ * 75;
    default: return 0;
    }
}

MaterialType Game::upgradeMaterialType(int index) const
{
    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
        return MaterialType::OldWoodBuildingMaterial;
    case 4:
    case 5:
    case 6:
    case 7:
        return MaterialType::ManaDrop;
    default:
        return MaterialType::OldWoodBuildingMaterial;
    }
}

int Game::upgradeMaterialCost(int index) const
{
    switch (index) {
    case 0: return warehouseCapacityLevel_ + 2;
    case 1: return merchantUpgradeLevel_ + 1;
    case 2: return processingUnlockLevel_ + 2;
    case 3: return ringWorkshopUnlocked_ ? 0 : 5;
    case 4: return maxHpUpgradeLevel_ + 1;
    case 5: return ringRadiusUpgradeLevel_ + 1;
    case 6: return ringSpeedUpgradeLevel_ + 1;
    default: return 0;
    }
}

const char* Game::upgradeName(int index) const
{
    switch (index) {
    case 0: return "倉庫容量強化";
    case 1: return "商人機能強化";
    case 2: return "作業台機能解禁";
    case 3: return "リング工房解禁";
    case 4: return "最大HPアップ";
    case 5: return "リング半径アップ";
    case 6: return "リング速度アップ";
    case 7: return "プレイ性能強化枠";
    default: return "";
    }
}

int Game::upgradeLevel(int index) const
{
    switch (index) {
    case 0: return warehouseCapacityLevel_ + 1;
    case 1: return merchantUpgradeLevel_;
    case 2: return processingUnlockLevel_;
    case 3: return ringWorkshopUnlocked_ ? 1 : 0;
    case 4: return maxHpUpgradeLevel_;
    case 5: return ringRadiusUpgradeLevel_;
    case 6: return ringSpeedUpgradeLevel_;
    case 7: return 0;
    default: return 0;
    }
}

int Game::upgradeMaxLevel(int index) const
{
    switch (index) {
    case 0: return 5;
    case 1: return 7;
    case 2: return 5;
    case 3: return 1;
    case 4:
    case 5:
    case 6:
        return 5;
    default:
        return 0;
    }
}

bool Game::upgradeImplemented(int index) const
{
    return index >= 0 && index <= 6;
}

bool Game::upgradeMaxed(int index) const
{
    const int maxLevel = upgradeMaxLevel(index);
    return maxLevel <= 0 || upgradeLevel(index) >= maxLevel;
}

void Game::buyUpgrade(int index)
{
    if (!upgradeImplemented(index)) {
        baseStatus_ = "この強化枠は未実装です";
        return;
    }
    if (upgradeMaxed(index)) {
        baseStatus_ = "強化上限です";
        return;
    }
    const int cost = upgradeCost(index);
    if (cost <= 0) {
        return;
    }
    if (money_ < cost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    const MaterialType materialType = upgradeMaterialType(index);
    const int materialCost = upgradeMaterialCost(index);
    if (materialCost > 0 && inventory_.materialCount(materialType) < materialCost) {
        baseStatus_ = std::string(materialTypeDisplayName(materialType)) + "が足りません";
        return;
    }

    money_ -= cost;
    if (materialCost > 0) {
        const bool spent = inventory_.materials().spend(materialType, materialCost);
        (void)spent;
    }
    switch (index) {
    case 0:
        ++warehouseCapacityLevel_;
        break;
    case 1:
        ++merchantUpgradeLevel_;
        refreshMerchantStock(true);
        break;
    case 2:
        ++processingUnlockLevel_;
        break;
    case 3:
        ringWorkshopUnlocked_ = true;
        break;
    case 4:
        ++maxHpUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 5:
        ++ringRadiusUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 6:
        ++ringSpeedUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    default:
        break;
    }
    baseStatus_ = "強化しました";
}

void Game::openRingWorkshop()
{
    if (!ringWorkshopUnlocked_) {
        baseStatus_ = "リング工房はまだ解禁されていません";
        return;
    }
    baseRingWorkshopActive_ = true;
    baseRingWorkshopSelection_ = 0;
    resetRingWorkshopDraft();
    baseStatus_.clear();
}

void Game::resetRingWorkshopDraft()
{
    ringWorkshopDraftRadiusPoints_ = levelRingRadiusPoints_;
    ringWorkshopDraftSpeedPoints_ = levelRingSpeedPoints_;
}

int Game::ringLevelUpgradePointTotal() const
{
    return std::max(0, levelRingRadiusPoints_) + std::max(0, levelRingSpeedPoints_);
}

bool Game::ringWorkshopRespecChanged() const
{
    return ringWorkshopDraftRadiusPoints_ != levelRingRadiusPoints_ ||
        ringWorkshopDraftSpeedPoints_ != levelRingSpeedPoints_;
}

int Game::ringWorkshopRespecMoneyCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    return 80 + ringLevelUpgradePointTotal() * 20;
}

int Game::ringWorkshopRespecMoonCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    return 1 + ringLevelUpgradePointTotal() / 3;
}

bool Game::adjustRingWorkshopRespec(int direction)
{
    if (ringLevelUpgradePointTotal() <= 0) {
        baseStatus_ = "再調整できるリング強化ポイントがありません";
        return false;
    }
    if (direction > 0) {
        if (ringWorkshopDraftSpeedPoints_ <= 0) {
            baseStatus_ = "速度から移せるポイントがありません";
            return false;
        }
        --ringWorkshopDraftSpeedPoints_;
        ++ringWorkshopDraftRadiusPoints_;
    } else {
        if (ringWorkshopDraftRadiusPoints_ <= 0) {
            baseStatus_ = "半径から移せるポイントがありません";
            return false;
        }
        --ringWorkshopDraftRadiusPoints_;
        ++ringWorkshopDraftSpeedPoints_;
    }
    baseStatus_ = "配分案を変更しました。確定で支払います";
    return true;
}

void Game::confirmRingWorkshopRespec()
{
    if (!ringWorkshopRespecChanged()) {
        baseStatus_ = "配分は変更されていません";
        return;
    }
    const int moneyCost = ringWorkshopRespecMoneyCost();
    const int moonCost = ringWorkshopRespecMoonCost();
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    levelRingRadiusPoints_ = std::max(0, ringWorkshopDraftRadiusPoints_);
    levelRingSpeedPoints_ = std::max(0, ringWorkshopDraftSpeedPoints_);
    applyPermanentUpgrades();
    baseStatus_ = "リング強化の配分を再調整しました";
}

const char* Game::ringWorkshopUpgradeName(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return "初期リング半径強化";
    case RingWorkshopUpgrade::InitialSpeed:
        return "初期リング速度強化";
    case RingWorkshopUpgrade::ShiftDistance:
        return "ずらし距離強化";
    }
    return "";
}

int Game::ringWorkshopUpgradeLevel(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return workshopInitialRadiusLevel_;
    case RingWorkshopUpgrade::InitialSpeed:
        return workshopInitialSpeedLevel_;
    case RingWorkshopUpgrade::ShiftDistance:
        return workshopShiftDistanceLevel_;
    }
    return 0;
}

int Game::ringWorkshopUpgradeMaxLevel(RingWorkshopUpgrade) const
{
    return 5;
}

int Game::ringWorkshopUpgradeMoneyCost(RingWorkshopUpgrade upgrade) const
{
    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    return 120 + level * 90;
}

int Game::ringWorkshopUpgradeMoonCost(RingWorkshopUpgrade upgrade) const
{
    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    return level + 1;
}

float Game::ringWorkshopUpgradeCurrentValue(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return effectiveInitialRingRadius(levelRingRadiusPoints_);
    case RingWorkshopUpgrade::InitialSpeed:
        return effectiveInitialRingSpeed(levelRingSpeedPoints_);
    case RingWorkshopUpgrade::ShiftDistance:
        return effectiveRingShiftDistance();
    }
    return 0.0f;
}

float Game::ringWorkshopUpgradeNextValue(RingWorkshopUpgrade upgrade) const
{
    const int currentLevel = ringWorkshopUpgradeLevel(upgrade);
    if (currentLevel >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return ringWorkshopUpgradeCurrentValue(upgrade);
    }
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius: {
        const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringRadiusUpgradeLevel_) * 0.08f;
        const float workshopMultiplier = 1.0f + static_cast<float>(currentLevel + 1) * 0.05f;
        const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelRingRadiusPoints_)));
        return balance_.spellRingRadius * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
    }
    case RingWorkshopUpgrade::InitialSpeed: {
        const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringSpeedUpgradeLevel_) * 0.08f;
        const float workshopMultiplier = 1.0f + static_cast<float>(currentLevel + 1) * 0.05f;
        const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelRingSpeedPoints_)));
        return balance_.spellRingSpeed * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
    }
    case RingWorkshopUpgrade::ShiftDistance:
        return balance_.spellRingShiftDistance + static_cast<float>(currentLevel + 1) * 8.0f;
    }
    return 0.0f;
}

void Game::buyRingWorkshopUpgrade(RingWorkshopUpgrade upgrade)
{
    if (ringWorkshopUpgradeLevel(upgrade) >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        baseStatus_ = "この強化は上限です";
        return;
    }
    const int moneyCost = ringWorkshopUpgradeMoneyCost(upgrade);
    const int moonCost = ringWorkshopUpgradeMoonCost(upgrade);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        ++workshopInitialRadiusLevel_;
        break;
    case RingWorkshopUpgrade::InitialSpeed:
        ++workshopInitialSpeedLevel_;
        break;
    case RingWorkshopUpgrade::ShiftDistance:
        ++workshopShiftDistanceLevel_;
        break;
    }
    applyPermanentUpgrades();
    resetRingWorkshopDraft();
    baseStatus_ = "リング工房強化を行いました";
}

void Game::openBookshelf()
{
    baseBookshelfActive_ = true;
    bookshelfPage_ = BookshelfPage::Menu;
    bookshelfSelection_ = 0;
    baseStatus_.clear();
    syncEncyclopediaFromInventoryAndRing();
}

void Game::syncEncyclopediaFromInventoryAndRing()
{
    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        if (!stack.objectId.empty() && stack.count > 0) {
            encyclopedia_.noteItemObtained(stack.item, player_.position);
        }
    }
    for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
        if (!objectInstance.item.id.empty()) {
            encyclopedia_.noteItemObtained(objectInstance.item, player_.position);
        }
    }
    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->objectId.empty()) {
            continue;
        }
        const ObjectDefinition* object = objectCatalog_.registry.findById(itemPtr->objectId);
        if (object != nullptr) {
            encyclopedia_.noteItemEquipped(*object, itemPtr->worldPosition);
        }
    }
}

void Game::applyEffectDiscoveries(const std::vector<EffectDiscoveryEvent>& discoveries)
{
    for (const EffectDiscoveryEvent& discovery : discoveries) {
        if (!encyclopedia_.discoverObjectEffect(
                discovery.objectId,
                discovery.effectKey,
                objectCatalog_,
                discovery.position,
                discovery.note)) {
            encyclopedia_.noteEffectEvent(discovery, objectCatalog_);
        }
    }
}

void Game::addStoryFlag(std::string flag)
{
    if (flag.empty()) {
        return;
    }
    if (std::find(storyFlags_.begin(), storyFlags_.end(), flag) == storyFlags_.end()) {
        storyFlags_.push_back(std::move(flag));
    }
}

void Game::startBaseMonicaDialogue()
{
    baseStatus_.clear();
    dialogue_.start(baseMonicaDialogue());
}

void Game::maybeStartFirstDungeonDialogue()
{
    constexpr std::string_view Flag = "dialogue_first_dungeon";
    const bool alreadySeen = std::find(storyFlags_.begin(), storyFlags_.end(), std::string(Flag)) != storyFlags_.end();
    if (alreadySeen) {
        return;
    }

    addStoryFlag(std::string(Flag));
    dialogue_.start(firstDungeonDialogue());
}

void Game::updateBookshelfScreen(const Input& input, UiContext& ui)
{
    const auto objectCountForPage = [this](BookshelfPage page) {
        int count = 0;
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((page == BookshelfPage::Treasures && treasure) ||
                (page == BookshelfPage::Items && !treasure)) {
                ++count;
            }
        }
        return count;
    };

    if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
        if (bookshelfPage_ == BookshelfPage::Menu) {
            baseBookshelfActive_ = false;
            baseStatus_.clear();
        } else {
            bookshelfPage_ = BookshelfPage::Menu;
            bookshelfSelection_ = 0;
        }
        return;
    }

    const int itemCount = bookshelfPage_ == BookshelfPage::Menu
        ? BookshelfMenuItemCount
        : (bookshelfPage_ == BookshelfPage::Enemies ? static_cast<int>(enemyCatalog_.enemies.size()) : objectCountForPage(bookshelfPage_));
    if (itemCount <= 0) {
        bookshelfSelection_ = 0;
    } else {
        if (input.pressed(InputAction::MoveUp)) {
            bookshelfSelection_ = (bookshelfSelection_ + itemCount - 1) % itemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            bookshelfSelection_ = (bookshelfSelection_ + 1) % itemCount;
        }
        bookshelfSelection_ = std::clamp(bookshelfSelection_, 0, itemCount - 1);
    }

    const int visibleCount = std::min(BookshelfVisibleRows, itemCount);
    const int firstVisibleIndex = std::clamp(bookshelfSelection_ - visibleCount / 2, 0, std::max(0, itemCount - visibleCount));
    for (int i = 0; i < visibleCount; ++i) {
        const UiRect rect = bookshelfItemRect(i);
        const int itemIndex = firstVisibleIndex + i;
        if (rect.contains(ui.mouse())) {
            bookshelfSelection_ = itemIndex;
        }
        if (ui.pressed(rect)) {
            bookshelfSelection_ = itemIndex;
            if (bookshelfPage_ == BookshelfPage::Menu) {
                bookshelfPage_ = static_cast<BookshelfPage>(bookshelfSelection_ + 1);
                bookshelfSelection_ = 0;
            }
            return;
        }
    }

    if ((input.confirmPressed() || input.useItemPressed()) && bookshelfPage_ == BookshelfPage::Menu) {
        bookshelfPage_ = static_cast<BookshelfPage>(bookshelfSelection_ + 1);
        bookshelfSelection_ = 0;
        return;
    }

    ui.block(basePanelRect());
}

DungeonGenerationContext Game::makeDungeonGenerationContext() const
{
    const int stageId = currentStage_ + 1;
    // Future connection: pass currentStageDefinition() into DungeonLayout generation.
    // This pass keeps the existing one-stage vertical slice parameters unchanged.
    return DungeonGenerationContext{
        .stageId = stageId,
        .seed = makeDungeonSeed(stageId, roguelikeDungeon_),
        .stageHardnessMultiplier = stageHardnessMultiplierForStageId(stageId),
        .roguelike = roguelikeDungeon_,
    };
}

void Game::generateDungeonLayoutForRun()
{
    // Future connection: currentStageDefinition().goalDistanceTiles,
    // generationProfile, warpPointCount, and specialRoomCount become layout inputs here.
    dungeonLayout_ = generateDungeonLayout(makeDungeonGenerationContext());
}

void Game::refreshOrbitEffects()
{
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.clearOrbitModifiers();
    player_.status.removeModifiersBySourcePrefix("orbit:");
    if (objectCatalog_.objectsById.empty()) {
        return;
    }

    std::vector<SpellRingItem*> runtimeItems = spellRing_.runtimeItemsMutable();
    for (SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        SpellRingItem& item = *itemPtr;
        item.lightRadius = 0.0f;
        item.hiddenDetectionRadius = 0.0f;
        item.treasureDetectionRadius = 0.0f;
        if (item.broken()) {
            continue;
        }
        if (item.objectId.empty()) {
            continue;
        }

        const auto objectIt = objectCatalog_.objectsById.find(item.objectId);
        if (objectIt == objectCatalog_.objectsById.end()) {
            continue;
        }

        EffectContext context;
        context.sourceObject = &objectIt->second;
        context.owner = &player_;
        context.orbit = &spellRing_;
        context.orbitItem = &item;
        context.effects = &effects_;
        context.encyclopedia = &encyclopedia_;
        context.position = item.worldPosition;
        context.triggerType = EffectTriggerType::Orbit;
        context.logUnimplementedEffects = false;
        effectDispatcher_.dispatchOrbitEffects(objectIt->second, context);
    }
}

void Game::updateCapturedProjectileBehaviors(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    std::vector<SpellRingItem*> runtimeItems = spellRing_.runtimeItemsMutable();
    for (SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        SpellRingItem& item = *itemPtr;
        if (item.broken()) {
            continue;
        }

        const std::optional<CapturedProjectileBehaviorPlan> plan = capturedProjectileBehaviorPlan(item);
        if (!plan.has_value()) {
            item.capturedProjectileTimer = 0.0f;
            item.capturedProjectileBurstRemaining = 0;
            continue;
        }

        const std::string& behaviorId = plan->behaviorId;
        const auto behaviorIt = enemyCatalog_.behaviorsById.find(behaviorId);
        const BehaviorDefinition* behavior = behaviorIt != enemyCatalog_.behaviorsById.end() ? &behaviorIt->second : nullptr;
        std::string projectileId = std::string(fallbackCapturedProjectileId(behaviorId));
        if (behavior != nullptr && !behavior->defaultProjectileId.empty() && behavior->defaultProjectileId != "none") {
            projectileId = behavior->defaultProjectileId;
        }
        if (behaviorId == "throw_object" || behaviorId == "throw_stone") {
            projectileId = item.capturedBehaviorParamString("throw_object", "projectile", projectileId);
        }

        const float intervalFloor = behaviorId == "radial_spike" ? CapturedRadialSpikeMinInterval : CapturedProjectileMinInterval;
        const float configuredInterval = behavior != nullptr && behavior->defaultIntervalSeconds > 0.0 && std::isfinite(behavior->defaultIntervalSeconds)
            ? static_cast<float>(behavior->defaultIntervalSeconds)
            : intervalFloor;
        const float codedInterval = static_cast<float>(std::max(0.0, item.capturedBehaviorInterval(behaviorId, configuredInterval)));
        const float interval = std::max(intervalFloor, codedInterval > 0.0f ? codedInterval : configuredInterval);

        item.capturedProjectileTimer = std::max(0.0f, item.capturedProjectileTimer - dt);
        if (item.capturedProjectileTimer > 0.0f) {
            continue;
        }

        const int activePlayerProjectiles = projectiles_.activeCount(ProjectileOwnerType::PlayerOrbit);
        const int radialCount = std::clamp(item.capturedBehaviorParamInt("radial_spike", "count", RadialSpikeProjectileCount), 1, 16);
        const int requestedProjectiles = behaviorId == "radial_spike" ? radialCount : 1;
        if (activePlayerProjectiles + requestedProjectiles > MaxPlayerOrbitProjectiles) {
            item.capturedProjectileTimer = CapturedProjectileRetryInterval;
            continue;
        }

        ProjectileSpawnTuning tuning;
        tuning.speedMultiplier = static_cast<float>(std::max(0.05, item.capturedBehaviorParamDouble(behaviorId, "projectileSpeed", 1.0)));
        if (behaviorId == "shoot_fire") {
            tuning.radiusScale = static_cast<float>(std::max(0.2, item.capturedBehaviorParamDouble("shoot_fire", "scale", 1.0)));
        }
        const int damageOverride = item.capturedBehaviorParamInt(behaviorId, "damage", -1);
        if (damageOverride >= 0) {
            tuning.damageOverride = damageOverride;
        }

        const std::vector<EffectSpec> effects = capturedProjectileEffects(item, behaviorId, behavior);
        const Vec2 outward = normalize(item.worldPosition - spellRing_.center());
        bool fired = false;
        if (behaviorId == "radial_spike") {
            for (int i = 0; i < radialCount; ++i) {
                const float angle = Pi * 2.0f * static_cast<float>(i) / static_cast<float>(radialCount);
                const Vec2 direction = fromAngle(angle);
                const Vec2 origin = item.worldPosition + direction * (item.hitRadius + 5.0f);
                const bool spawned = projectiles_.spawn(projectileId, origin, direction, ProjectileOwnerType::PlayerOrbit, effects, tuning);
                if (spawned) {
                    effects_.spawnMagicCast(origin, direction, particleElementForProjectile(projectileId), 8.0f);
                }
                fired = spawned || fired;
            }
        } else if (behaviorId == "shoot_fire") {
            const int volleyCount = std::clamp(item.capturedBehaviorParamInt("shoot_fire", "count", 1), 1, 5);
            const float spreadDegrees = static_cast<float>(std::max(0.0, item.capturedBehaviorParamDouble("shoot_fire", "spread", 12.0)));
            if (volleyCount > 1) {
                const float spreadRadians = clamp(spreadDegrees, 0.0f, 90.0f) * (Pi / 180.0f);
                const float start = -spreadRadians * 0.5f;
                const float step = spreadRadians / static_cast<float>(volleyCount - 1);
                const float baseAngle = std::atan2(outward.y, outward.x);
                for (int i = 0; i < volleyCount; ++i) {
                    const float angle = baseAngle + start + step * static_cast<float>(i);
                    const Vec2 direction = fromAngle(angle);
                    const Vec2 origin = item.worldPosition + direction * (item.hitRadius + 5.0f);
                    const bool spawned = projectiles_.spawn(projectileId, origin, direction, ProjectileOwnerType::PlayerOrbit, effects, tuning);
                    if (spawned) {
                        effects_.spawnMagicCast(origin, direction, particleElementForProjectile(projectileId), 8.0f);
                    }
                    fired = spawned || fired;
                }
            }
        }

        if (!fired) {
            const Vec2 origin = item.worldPosition + outward * (item.hitRadius + 5.0f);
            fired = projectiles_.spawn(projectileId, origin, outward, ProjectileOwnerType::PlayerOrbit, effects, tuning);
            if (fired) {
                effects_.spawnMagicCast(origin, outward, particleElementForProjectile(projectileId), 8.0f);
            }
        }

        if (!fired) {
            item.capturedProjectileTimer = CapturedProjectileRetryInterval;
            continue;
        }

        if (behaviorId == "shoot_water") {
            const int burstCount = std::clamp(item.capturedBehaviorParamInt("shoot_water", "burstCount", 1), 1, 6);
            const float burstInterval = static_cast<float>(std::max(0.02, item.capturedBehaviorParamDouble("shoot_water", "burstInterval", 0.14)));
            if (burstCount > 1) {
                if (item.capturedProjectileBurstRemaining <= 0) {
                    item.capturedProjectileBurstRemaining = burstCount;
                }
                if (item.capturedProjectileBurstRemaining > 1) {
                    --item.capturedProjectileBurstRemaining;
                    item.capturedProjectileTimer = burstInterval;
                } else {
                    item.capturedProjectileBurstRemaining = burstCount;
                    item.capturedProjectileTimer = interval;
                }
            } else {
                item.capturedProjectileBurstRemaining = 0;
                item.capturedProjectileTimer = interval;
            }
        } else {
            item.capturedProjectileBurstRemaining = 0;
            item.capturedProjectileTimer = interval;
        }
    }
}

void Game::updateCapturedUtilityBehaviors(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    std::vector<SpellRingItem*> runtimeItems = spellRing_.runtimeItemsMutable();
    for (SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        SpellRingItem& item = *itemPtr;
        item.capturedExplodeSleepTimer = std::max(0.0f, item.capturedExplodeSleepTimer - dt);
        item.capturedMagnetVisualTimer = std::max(0.0f, item.capturedMagnetVisualTimer - dt);

        if (item.broken()) {
            continue;
        }

        if (item.hasCapturedBehavior("magnet_pull")) {
            const float radius = static_cast<float>(std::max(32.0, item.capturedBehaviorParamDouble("magnet_pull", "radius", 170.0)));
            const float strength = static_cast<float>(std::max(0.05, item.capturedBehaviorParamDouble("magnet_pull", "strength", 1.0)));
            const std::string targetTag = item.capturedBehaviorParamString("magnet_pull", "targetTag", "metal");
            const bool affectMetal = targetTag.empty() || targetTag.find("metal") != std::string::npos;
            const int pulledDrops = affectMetal ? worldDrops_.pullMetalDrops(objectCatalog_, item.worldPosition, dt * strength, radius) : 0;
            const int pulledEnemies = affectMetal ? enemies_.pullMetalEnemies(item.worldPosition, tileMap_, dt * strength, radius) : 0;
            const int pulledProjectiles = affectMetal ? projectiles_.pullMetalProjectiles(item.worldPosition, dt * strength, radius) : 0;
            if (pulledDrops + pulledEnemies + pulledProjectiles > 0 && item.capturedMagnetVisualTimer <= 0.0f) {
                effects_.spawnAreaPulse(item.worldPosition, 42.0f, {120, 190, 245, 150});
                item.capturedMagnetVisualTimer = CapturedMagnetVisualInterval;
            }
        }

        if (item.hasCapturedBehavior("wind_deflect")) {
            const float interval = static_cast<float>(std::max(0.2, item.capturedBehaviorInterval("wind_deflect", CapturedWindInterval)));
            const float radius = static_cast<float>(std::max(24.0, item.capturedBehaviorParamDouble("wind_deflect", "radius", 150.0)));
            const float strength = static_cast<float>(std::max(0.1, item.capturedBehaviorParamDouble("wind_deflect", "strength", 1.0)));
            item.capturedWindTimer = std::max(0.0f, item.capturedWindTimer - dt);
            if (item.capturedWindTimer <= 0.0f) {
                const int deflected = projectiles_.deflectEnemyProjectiles(item.worldPosition, strength, radius);
                if (deflected > 0) {
                    effects_.spawnAreaPulse(item.worldPosition, 66.0f, {150, 235, 205, 155});
                }
                item.capturedWindTimer = interval;
            }
        } else {
            item.capturedWindTimer = 0.0f;
        }
    }
}

void Game::updateAmbientParticleEffects(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    ringTrailEffectTimer_ -= dt;
    if (spellRing_.state() != SpellRingState::Normal && ringTrailEffectTimer_ <= 0.0f) {
        const Vec2 trailDirection = spellRing_.state() == SpellRingState::Thrown
            ? player_.facing
            : player_.position - spellRing_.center();
        effects_.spawnForegroundRingTrail(spellRing_.center(), trailDirection);
        const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
        for (const SpellRingItem* itemPtr : runtimeItems) {
            if (itemPtr == nullptr) {
                continue;
            }
            effects_.spawnForegroundRingTrail(itemPtr->worldPosition, trailDirection);
        }
        ringTrailEffectTimer_ = 0.055f;
    }

    ambientParticleTimer_ -= dt;
    if (ambientParticleTimer_ > 0.0f) {
        return;
    }
    ambientParticleTimer_ = 0.18f;

    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->broken()) {
            continue;
        }
        if (itemPtr->type == SpellRingItemType::Torch || itemPtr->lightRadius > 0.0f) {
            effects_.spawnForegroundTorchFlicker(itemPtr->worldPosition);
        }
        if (!itemPtr->objectId.empty() || !itemPtr->addedEffects.empty() || itemPtr->hiddenDetectionRadius > 0.0f || itemPtr->treasureDetectionRadius > 0.0f) {
            effects_.spawnForegroundSpecialItemGlimmer(itemPtr->worldPosition);
        }
    }

    enemies_.emitStatusParticles(effects_);

    if (warpPointsEnabled_) {
        for (const WarpPoint& point : warpPoints_) {
            if (point.discovered || point.unlocked) {
                effects_.spawnWarpCircle(point.position, false);
            }
        }
        if (hasBossSpawnPoint_ && !bossSpawned_) {
            effects_.spawnWarpCircle(bossSpawnPoint_, true);
        }
    }
}

void Game::handleCapturedExplosion(Vec2 position)
{
    effects_.spawnAreaPulse(position, 50.0f, {255, 128, 54, 190});
    const std::vector<DamagedTile> openedTiles = tileMap_.damageCircle(position, CapturedExplosionTileRadius, CapturedExplosionTileDamage);
    for (const DamagedTile& tile : openedTiles) {
        effects_.spawnTileBreak(tile.center, tile.type, tile.color);
        ++runStats_.dugTiles;
    }
    enemies_.applyCapturedExplosion(position, spellRing_, CapturedExplosionEnemyDamage);
}

void Game::resize(int width, int height)
{
    camera_.setViewport(width, height);
}

void Game::choosePauseMenuItem(int item)
{
    switch (item) {
    case 0:
        pausePage_ = PauseMenuPage::Status;
        break;
    case 1:
        inventory_.setOpen(true);
        inventory_.cancelGrab();
        inventoryReturnToPause_ = true;
        pausePage_ = PauseMenuPage::Main;
        mode_ = ScreenMode::Inventory;
        break;
    case 2:
        openRingScreen();
        break;
    case 3:
        pausePage_ = PauseMenuPage::Options;
        break;
    case 4:
        pausePage_ = PauseMenuPage::QuitConfirm;
        pauseConfirmSelection_ = 0;
        break;
    default:
        break;
    }
}

void Game::leavePausePage()
{
    if (pausePage_ == PauseMenuPage::Main) {
        mode_ = pauseReturnMode_;
        return;
    }

    pausePage_ = PauseMenuPage::Main;
}

void Game::openRingScreen()
{
    pausePage_ = PauseMenuPage::Main;
    mode_ = ScreenMode::Ring;
    const int maxIndex = std::max(0, static_cast<int>(spellRing_.items().size()) - 1);
    ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, maxIndex);
    ringDragPending_ = false;
    ringDragActive_ = false;
    ringSnapActive_ = false;
    ringDragItemIndex_ = -1;
    cancelRingGrab();
    ringStatus_.clear();
}

void Game::cancelRingGrab()
{
    if (!ringGrabActive_) {
        return;
    }

    ringDragPending_ = false;
    ringDragActive_ = false;
    ringSnapActive_ = false;
    ringDragItemIndex_ = -1;
    if (!spellRing_.addItem(ringGrabbedItem_)) {
        ringGrabbedItem_.ringIndex = spellRing_.activeRingIndex();
        spellRing_.items().push_back(ringGrabbedItem_);
        spellRing_.normalizeItemPlacements();
    }
    ringGrabActive_ = false;
    ringGrabOrigin_ = -1;
}

Game::InventoryCarryState Game::captureInventoryCarryState() const
{
    InventoryCarryState state;
    state.inventory = inventory_;
    state.ringItemsByRing = spellRing_.ringItems();
    state.valid = true;
    return state;
}

void Game::restoreInventoryCarryState(const InventoryCarryState& state)
{
    if (!state.valid) {
        return;
    }

    inventory_ = state.inventory;
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    spellRing_.ringItems() = state.ringItemsByRing;
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.normalizeItemPlacements();
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
}

void Game::captureRunStartInventoryState()
{
    runStartInventoryState_ = captureInventoryCarryState();
}

void Game::clearTemporaryPlayerState(bool fullHeal)
{
    if (fullHeal) {
        player_.hp = player_.maxHp;
    } else {
        player_.hp = std::max(1, player_.hp);
    }
    player_.velocity = {};
    player_.throwCooldownRemaining = 0.0f;
    player_.poisonDamageAccumulator = 0.0;
    player_.lastDamageSource = DamageSource::Unknown;
    player_.lastDamageEnemyName.clear();
    player_.damageFlash = 0.0f;
    player_.damageEvents.clear();
    player_.status = EntityStatus{};
}

Vec2 Game::latestWarpPointStartPosition() const
{
    if (hasLatestWarpPointPosition_) {
        return latestWarpPointPosition_;
    }
    for (auto it = warpPoints_.rbegin(); it != warpPoints_.rend(); ++it) {
        if (it->discovered) {
            return it->position;
        }
    }
    return {};
}

void Game::rebuildUnlockedWarpPointsForStart(Vec2 latestPosition)
{
    initializeWarpPointsFromLayout();
    int discoveredCount = 0;
    for (WarpPoint& point : warpPoints_) {
        if (discoveredCount < unlockedWarpPointCount_) {
            point.discovered = true;
            point.unlocked = true;
            point.snapshotCaptured = true;
            ++discoveredCount;
        }
    }
    if (!warpPoints_.empty()) {
        WarpPoint& latest = warpPoints_[static_cast<std::size_t>(std::clamp(unlockedWarpPointCount_ - 1, 0, static_cast<int>(warpPoints_.size()) - 1))];
        latest.position = latestPosition;
        latest.tilePosition = {
            tileMap_.worldToTile(latestPosition.x),
            tileMap_.worldToTile(latestPosition.y),
        };
        latest.discovered = true;
        latest.unlocked = true;
        latest.snapshotCaptured = true;
    }
    if (unlockedWarpPointCount_ > BossWarpPointIndex) {
        configureBossSpawnPointFromWarp(latestPosition);
    }
}

void Game::retryAfterGameOver()
{
    const RetrySnapshot checkpoint = retrySnapshot_;
    if (checkpoint.valid) {
        initializeWorld(false);
        retrySnapshot_ = checkpoint;
        restoreRetrySnapshot();
        clearTemporaryPlayerState(true);
        mode_ = ScreenMode::Playing;
        return;
    }

    InventoryCarryState retained = captureInventoryCarryState();
    const int retainedLevel = player_.level;
    const int retainedXp = player_.xp;
    const int retainedXpToNext = player_.xpToNext;
    if (restoreRunStartInventoryOnDeath_ && runStartInventoryState_.valid) {
        retained = runStartInventoryState_;
    }

    player_ = Player{};
    player_.position = tileWorldCenter(dungeonLayout_.startTile);
    restoreInventoryCarryState(retained);
    player_.level = retainedLevel;
    player_.xp = retainedXp;
    player_.xpToNext = retainedXpToNext;
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    enemies_.clearTemporaryState();
    effects_ = EffectSystem{};
    projectiles_ = ProjectileSystem{};
    levels_ = LevelSystem{};
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    camera_.follow(player_.position, 1.0f);
    captureRunStartInventoryState();
    mode_ = ScreenMode::Playing;
}

void Game::returnToBaseAfterGameOver()
{
    returnToBaseFromNormalStage(false, true);
}

bool Game::shouldRefreshMerchantOnReturn(bool stageCleared, bool died) const
{
    return stageCleared ||
        died ||
        runStats_.dugTiles >= MerchantRefreshDugTileThreshold ||
        runStats_.acquiredItems > 0 ||
        runStats_.defeatedEnemies > 0;
}

void Game::returnToBaseFromNormalStage(bool stageCleared, bool died)
{
    if (enemyTestActive_) {
        exitEnemyTestToBase();
        return;
    }

    const bool refreshMerchant = shouldRefreshMerchantOnReturn(stageCleared, died);
    merchantRefreshPending_ = merchantRefreshPending_ || refreshMerchant;
    clearTemporaryPlayerState(true);
    captureDungeonState();
    enemies_ = EnemySystem{};
    effects_ = EffectSystem{};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
    projectiles_ = ProjectileSystem{};
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    levels_ = LevelSystem{};
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    bossSpawned_ = false;
    hasBossSpawnPoint_ = false;
    retrySnapshot_ = RetrySnapshot{};
    warpPoints_.clear();
    spawnedWarpPointCount_ = 0;
    enterBase();
    baseStatus_ = refreshMerchant ? "帰還しました。商人ワゴン更新あり" : "帰還しました";
    if (autoSaveOnReturn_) {
        std::string message;
        if (saveSaveData(message)) {
            baseStatus_ += " / 自動保存";
        } else {
            baseStatus_ += " / " + message;
        }
    }
}

void Game::resetWarpPointRunState()
{
    hasBossSpawnPoint_ = false;
    retrySnapshot_ = RetrySnapshot{};
    warpPointsEnabled_ = !roguelikeDungeon_;
    initializeWarpPointsFromLayout();
}

void Game::captureDungeonState()
{
    if (enemyTestActive_ || roguelikeDungeon_ || currentStageId_.empty()) {
        return;
    }

    enemies_.clearTemporaryState();
    DungeonState state;
    state.valid = true;
    state.currentStage = currentStage_;
    state.currentStageId = currentStageId_;
    state.tileMap = tileMap_;
    state.dungeonLayout = dungeonLayout_;
    state.runStats = runStats_;
    state.warpPoints = warpPoints_;
    state.rewardNodes = rewardNodes_;
    state.moneyNodes = moneyNodes_;
    state.moonFragmentNodes = moonFragmentNodes_;
    state.chestNodes = chestNodes_;
    state.crateNodes = crateNodes_;
    state.enemyNodes = enemyNodes_;
    state.enemies = enemies_;
    state.worldDrops = worldDrops_;
    state.spawnedWarpPointCount = spawnedWarpPointCount_;
    state.bossSpawnPoint = bossSpawnPoint_;
    state.hasBossSpawnPoint = hasBossSpawnPoint_;
    state.bossSpawned = bossSpawned_;
    dungeonStates_[currentStageId_] = std::move(state);
}

bool Game::restoreDungeonState(bool useLatestWarpPoint)
{
    if (roguelikeDungeon_) {
        return false;
    }
    auto it = dungeonStates_.find(currentStageId_);
    if (it == dungeonStates_.end() || !it->second.valid) {
        return false;
    }

    const DungeonState& state = it->second;
    currentStage_ = state.currentStage;
    currentStageId_ = state.currentStageId;
    resolveCurrentStageDefinition();
    tileMap_ = state.tileMap;
    dungeonLayout_ = state.dungeonLayout;
    runStats_ = state.runStats;
    warpPoints_ = state.warpPoints;
    rewardNodes_ = state.rewardNodes;
    moneyNodes_ = state.moneyNodes;
    moonFragmentNodes_ = state.moonFragmentNodes;
    chestNodes_ = state.chestNodes;
    crateNodes_ = state.crateNodes;
    enemyNodes_ = state.enemyNodes;
    enemies_ = state.enemies;
    enemies_.clearTemporaryState();
    worldDrops_ = state.worldDrops;
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    spawnedWarpPointCount_ = state.spawnedWarpPointCount;
    bossSpawnPoint_ = state.bossSpawnPoint;
    hasBossSpawnPoint_ = state.hasBossSpawnPoint;
    bossSpawned_ = state.bossSpawned;
    warpPointsEnabled_ = true;
    retrySnapshot_ = RetrySnapshot{};
    effects_ = EffectSystem{};
    projectiles_ = ProjectileSystem{};
    levels_ = LevelSystem{};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;

    player_ = Player{};
    player_.xpToNext = balance_.xpBase + player_.level * balance_.xpPerLevel;
    player_.position = useLatestWarpPoint ? latestWarpPointStartPosition() : tileWorldCenter(dungeonLayout_.startTile);
    camera_.follow(player_.position, 1.0f);
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    return true;
}

bool Game::canRegenerateCurrentStage() const
{
    if (roguelikeDungeon_) {
        return false;
    }
    auto it = dungeonStates_.find(currentStageId_);
    const std::vector<WarpPoint>& points = it != dungeonStates_.end() && it->second.valid ? it->second.warpPoints : warpPoints_;
    if (points.empty()) {
        return unlockedWarpPointCount_ >= MaxWarpPointsPerRun;
    }
    return std::all_of(points.begin(), points.end(), [](const WarpPoint& point) {
        return point.discovered;
    });
}

std::size_t Game::retainedWorldDropCountForCurrentStage() const
{
    auto it = dungeonStates_.find(currentStageId_);
    if (it != dungeonStates_.end() && it->second.valid) {
        return it->second.worldDrops.size();
    }
    return worldDrops_.size();
}

void Game::initializeWarpPointsFromLayout()
{
    // Future connection: currentStageDefinition().warpPointCount will cap or drive
    // placement. Current placement remains DungeonLayout-anchor based.
    warpPoints_.clear();
    spawnedWarpPointCount_ = 0;
    if (!warpPointsEnabled_) {
        return;
    }

    const int previousUnlockedCount = std::clamp(unlockedWarpPointCount_, 0, MaxWarpPointsPerRun);
    int index = 0;
    for (Vec2 anchor : dungeonLayout_.warpPointAnchors) {
        if (index >= MaxWarpPointsPerRun) {
            break;
        }
        const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, anchor);
        if (metrics.pathProgress < 0.10f || metrics.pathProgress > 0.90f) {
            continue;
        }

        WarpPoint point;
        point.stageId = dungeonLayout_.stageId;
        point.index = index;
        point.tilePosition = roundDungeonTile(anchor);
        point.position = tileWorldCenter(point.tilePosition);
        point.discovered = index < previousUnlockedCount;
        point.unlocked = point.discovered;
        point.snapshotCaptured = point.discovered;
        warpPoints_.push_back(point);
        ++index;
    }
    spawnedWarpPointCount_ = static_cast<int>(warpPoints_.size());
}

int Game::discoveredWarpPointCount() const
{
    return static_cast<int>(std::count_if(warpPoints_.begin(), warpPoints_.end(), [](const WarpPoint& point) {
        return point.discovered;
    }));
}

std::vector<Game::WarpPoint> Game::discoveredWarpPoints() const
{
    std::vector<WarpPoint> discovered;
    for (const WarpPoint& point : warpPoints_) {
        if (point.discovered) {
            discovered.push_back(point);
        }
    }
    return discovered;
}

int Game::nearestWarpPointIndex(Vec2 position) const
{
    int nearest = -1;
    float nearestDistanceSq = 1.0e30f;
    for (const WarpPoint& point : warpPoints_) {
        const float distSq = distanceSquared(position, point.position);
        if (distSq < nearestDistanceSq) {
            nearestDistanceSq = distSq;
            nearest = point.index;
        }
    }
    return nearest;
}

void Game::updateWarpPoints(float dt)
{
    if (!warpPointsEnabled_) {
        return;
    }

    for (WarpPoint& point : warpPoints_) {
        if (point.lightRevealAnimating) {
            point.lightRevealTimer += dt;
            if (point.lightRevealTimer >= point.lightRevealDuration) {
                point.lightRevealTimer = point.lightRevealDuration;
                point.lightRevealAnimating = false;
            }
        }
        if (point.discovered) {
            continue;
        }
        if (distanceSquared(player_.position, point.position) <= WarpPointTouchRadius * WarpPointTouchRadius) {
            point.discovered = true;
            point.unlocked = true;
            unlockedWarpPointCount_ = std::max(unlockedWarpPointCount_, discoveredWarpPointCount());
            latestWarpPointPosition_ = point.position;
            hasLatestWarpPointPosition_ = true;
            point.snapshotCaptured = true;
            if (point.index == BossWarpPointIndex) {
                configureBossSpawnPointFromWarp(point.position);
            }
            captureRetrySnapshotAtWarpPoint();
            point.lightRevealTimer = 0.0f;
            point.lightRevealAnimating = true;
            reloadNotice_ = "ワープポイント発見";
            reloadNoticeTimer_ = 2.0f;
            pushDungeonLog("ワープポイントを発見", "warp_point");
        }
    }
}

void Game::initializeMoonFragmentNodesFromWarpPoints()
{
    moonFragmentNodes_.clear();
    if (!warpPointsEnabled_ || warpPoints_.empty()) {
        return;
    }

    std::mt19937 rng(dungeonLayout_.seed ^ 0x4D6F6F4Eu);
    std::uniform_int_distribution<int> countDistribution(MoonFragmentMinPerWarpPoint, MoonFragmentMaxPerWarpPoint);
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> floorRadiusDistribution(1.5f, 3.5f);
    std::uniform_real_distribution<float> buriedRadiusDistribution(2.5f, 4.5f);

    for (const WarpPoint& point : warpPoints_) {
        const int count = countDistribution(rng);
        for (int i = 0; i < count; ++i) {
            const bool buried = i % 2 == 1;
            const float radiusTiles = buried ? buriedRadiusDistribution(rng) : floorRadiusDistribution(rng);
            const Vec2 offset = fromAngle(angleDistribution(rng)) * radiusTiles;
            moonFragmentNodes_.push_back(MoonFragmentNode{
                .tile = roundDungeonTile(Vec2{
                    static_cast<float>(point.tilePosition.x),
                    static_cast<float>(point.tilePosition.y),
                } + offset),
                .visibility = buried ? PlacementVisibility::BuriedVisible : PlacementVisibility::Exposed,
                .collected = false,
            });
        }
    }
}

void Game::initializeRewardNodesFromLayout()
{
    // Future connection: currentStageDefinition().specialRoomCount will drive
    // special-room placement before reward and money nodes are materialized.
    rewardNodes_.clear();
    moneyNodes_.clear();
    if (dungeonLayout_.mainPathPoints.size() < 2) {
        return;
    }

    std::mt19937 rng(dungeonLayout_.seed ^ 0xB77A4C29u);
    std::uniform_real_distribution<float> progressJitter(-0.018f, 0.018f);
    std::uniform_real_distribution<float> sideJitter(-1.2f, 1.2f);
    std::uniform_int_distribution<int> signDist(0, 1);
    std::uniform_int_distribution<int> moneyDist(2, 8);
    const std::optional<std::string> fallbackObjectId = firstAvailableObjectId(objectCatalog_);

    for (int i = 0; i < RewardNodeCountPerRun; ++i) {
        const float progress = clamp(
            0.10f + 0.80f * (static_cast<float>(i + 1) / static_cast<float>(RewardNodeCountPerRun + 1)) + progressJitter(rng),
            0.10f,
            0.90f);
        const Vec2 anchor = pointAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        const Vec2 tangent = tangentAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        Vec2 side = perpendicular(tangent);
        if (signDist(rng) == 0) {
            side = side * -1.0f;
        }

        RewardNode node;
        node.visibility = i % 3 == 0
            ? PlacementVisibility::Exposed
            : (i % 3 == 1 ? PlacementVisibility::BuriedVisible : PlacementVisibility::BuriedHidden);
        const float offsetTiles = node.visibility == PlacementVisibility::Exposed
            ? 0.0f
            : (node.visibility == PlacementVisibility::BuriedVisible ? 7.0f : 9.0f) + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.rewardKind = i % 4 == 0 ? "treasure" : "item";
        node.objectId = (i % 2 == 0) ? fallbackObjectId : std::nullopt;
        node.revealed = node.visibility == PlacementVisibility::Exposed;
        rewardNodes_.push_back(std::move(node));
    }

    for (int i = 0; i < MoneyNodeCountPerRun; ++i) {
        const bool useBranch = !dungeonLayout_.branchPathPoints.empty() && i % 5 == 0;
        const float progress = clamp(
            0.08f + 0.84f * (static_cast<float>(i + 1) / static_cast<float>(MoneyNodeCountPerRun + 1)) + progressJitter(rng),
            0.08f,
            0.92f);
        Vec2 anchor = pointAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        Vec2 tangent = tangentAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        if (useBranch) {
            const DungeonPath& branch = dungeonLayout_.branchPathPoints[static_cast<std::size_t>(i) % dungeonLayout_.branchPathPoints.size()];
            anchor = pointAtPathProgress(branch.points, 0.65f);
            tangent = tangentAtPathProgress(branch.points, 0.65f);
        }
        Vec2 side = perpendicular(tangent);
        if (signDist(rng) == 0) {
            side = side * -1.0f;
        }

        MoneyNode node;
        node.visibility = i % 3 == 0
            ? PlacementVisibility::Exposed
            : (i % 3 == 1 ? PlacementVisibility::BuriedVisible : PlacementVisibility::BuriedHidden);
        const float offsetTiles = node.visibility == PlacementVisibility::Exposed
            ? 0.0f
            : (node.visibility == PlacementVisibility::BuriedVisible ? 6.0f : 8.5f) + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.amount = std::max(1, moneyDist(rng) + dungeonLayout_.stageId * 2);
        moneyNodes_.push_back(node);
    }

    for (const SpecialRoomAnchor& room : dungeonLayout_.specialRoomAnchors) {
        const DungeonTile centerTile = roundDungeonTile(room.center);
        if (room.type == SpecialRoomType::CoinRoom) {
            for (int i = 0; i < 4; ++i) {
                const Vec2 offset = fromAngle(static_cast<float>(i) * Pi * 0.5f) * 2.0f;
                moneyNodes_.push_back(MoneyNode{
                    .tile = roundDungeonTile(room.center + offset),
                    .amount = std::max(4, moneyDist(rng) + dungeonLayout_.stageId * 4),
                    .visibility = i == 0 ? PlacementVisibility::Exposed : (i == 1 ? PlacementVisibility::BuriedVisible : PlacementVisibility::BuriedHidden),
                    .collected = false,
                });
            }
        } else if (room.type == SpecialRoomType::TreasureRoom) {
            rewardNodes_.push_back(RewardNode{
                .tile = centerTile,
                .visibility = PlacementVisibility::Exposed,
                .rewardKind = "treasure",
                .objectId = fallbackObjectId,
                .revealed = true,
                .spawned = false,
                .collected = false,
            });
            rewardNodes_.push_back(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{room.radius, 0.0f}),
                .visibility = PlacementVisibility::BuriedVisible,
                .rewardKind = "treasure",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
            rewardNodes_.push_back(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{-room.radius, 0.0f}),
                .visibility = PlacementVisibility::BuriedHidden,
                .rewardKind = "treasure",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
        } else if (room.type == SpecialRoomType::EnemyRoom) {
            rewardNodes_.push_back(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{0.0f, room.radius}),
                .visibility = PlacementVisibility::BuriedHidden,
                .rewardKind = "enemy_room_reward",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
        } else if (room.type == SpecialRoomType::OreRoom) {
            rewardNodes_.push_back(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{room.radius, 0.0f}),
                .visibility = PlacementVisibility::BuriedVisible,
                .rewardKind = "ore_room_reward",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
        }
    }
}

void Game::updateExposedRewardNodes()
{
    const float pickupRadiusSq = RewardPickupRadius * RewardPickupRadius;
    for (RewardNode& node : rewardNodes_) {
        if (node.collected || node.visibility != PlacementVisibility::Exposed) {
            continue;
        }
        if (distanceSquared(player_.position, tileWorldCenter(node.tile)) > pickupRadiusSq) {
            continue;
        }

        bool spawnedObject = false;
        if (node.objectId.has_value()) {
            spawnedObject = worldDrops_.spawnObjectDrop(objectCatalog_, *node.objectId, tileWorldCenter(node.tile), runStats_.elapsedSeconds);
        }
        node.spawned = true;
        node.collected = true;
        if (!spawnedObject) {
            ++runStats_.acquiredItems;
        }
        reloadNotice_ = node.objectId.has_value() ? "報酬を拾得" : "仮報酬を拾得";
        reloadNoticeTimer_ = 1.4f;
    }

    for (MoneyNode& node : moneyNodes_) {
        if (node.collected || node.visibility != PlacementVisibility::Exposed) {
            continue;
        }
        if (distanceSquared(player_.position, tileWorldCenter(node.tile)) > pickupRadiusSq) {
            continue;
        }
        worldDrops_.spawnMoneyDrop(node.amount, tileWorldCenter(node.tile), runStats_.elapsedSeconds);
        node.collected = true;
        reloadNotice_ = "金貨 +" + std::to_string(std::max(0, node.amount)) + "G";
        reloadNoticeTimer_ = 1.4f;
    }
}

void Game::revealRewardNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles)
{
    if (openedTiles.empty()) {
        return;
    }

    for (Vec2 openedTile : openedTiles) {
        const DungeonTile tile{
            tileMap_.worldToTile(openedTile.x),
            tileMap_.worldToTile(openedTile.y),
        };
        for (RewardNode& node : rewardNodes_) {
            if (node.collected || node.visibility == PlacementVisibility::Exposed ||
                node.tile.x != tile.x || node.tile.y != tile.y) {
                continue;
            }
            node.revealed = true;
            node.spawned = true;
            bool spawnedObject = false;
            if (node.objectId.has_value()) {
                spawnedObject = worldDrops_.spawnObjectDrop(objectCatalog_, *node.objectId, tileWorldCenter(node.tile), runStats_.elapsedSeconds);
            }
            node.collected = true;
            if (!spawnedObject) {
                ++runStats_.acquiredItems;
            }
            reloadNotice_ = node.objectId.has_value() ? "埋まり報酬を発見" : "隠し報酬を発見";
            reloadNoticeTimer_ = 1.6f;
        }
        for (MoneyNode& node : moneyNodes_) {
            if (node.collected || node.visibility == PlacementVisibility::Exposed ||
                node.tile.x != tile.x || node.tile.y != tile.y) {
                continue;
            }
            worldDrops_.spawnMoneyDrop(node.amount, tileWorldCenter(node.tile), runStats_.elapsedSeconds);
            node.collected = true;
            reloadNotice_ = "埋まり金貨 +" + std::to_string(std::max(0, node.amount)) + "G";
            reloadNoticeTimer_ = 1.6f;
        }
    }
}

void Game::updateExposedMoonFragmentNodes()
{
    const float pickupRadiusSq = MoonFragmentPickupRadius * MoonFragmentPickupRadius;
    for (MoonFragmentNode& node : moonFragmentNodes_) {
        if (node.collected || node.visibility != PlacementVisibility::Exposed) {
            continue;
        }
        if (distanceSquared(player_.position, tileWorldCenter(node.tile)) > pickupRadiusSq) {
            continue;
        }
        worldDrops_.spawnMaterialDrop(MaterialType::MoonFragment, 1, tileWorldCenter(node.tile), runStats_.elapsedSeconds);
        node.collected = true;
        reloadNotice_ = "Moon fragment";
        reloadNoticeTimer_ = 1.4f;
    }
}

void Game::revealMoonFragmentNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles)
{
    if (openedTiles.empty()) {
        return;
    }

    for (Vec2 openedTile : openedTiles) {
        const DungeonTile tile{
            tileMap_.worldToTile(openedTile.x),
            tileMap_.worldToTile(openedTile.y),
        };
        for (MoonFragmentNode& node : moonFragmentNodes_) {
            if (node.collected || node.visibility != PlacementVisibility::BuriedVisible ||
                node.tile.x != tile.x || node.tile.y != tile.y) {
                continue;
            }
            worldDrops_.spawnMaterialDrop(MaterialType::MoonFragment, 1, tileWorldCenter(node.tile), runStats_.elapsedSeconds);
            node.collected = true;
            reloadNotice_ = "Moon fragment";
            reloadNoticeTimer_ = 1.4f;
        }
    }
}

void Game::initializeChestNodesFromLayout()
{
    chestNodes_.clear();
    if (dungeonLayout_.mainPathPoints.size() < 2) {
        return;
    }

    std::mt19937 rng(dungeonLayout_.seed ^ 0xC45E7A91u);
    std::uniform_real_distribution<float> progressJitter(-0.026f, 0.026f);
    std::uniform_real_distribution<float> sideJitter(-1.1f, 1.1f);
    std::uniform_int_distribution<int> signDist(0, 1);

    for (int i = 0; i < ChestNodeCountPerRun; ++i) {
        const float progress = clamp(
            0.08f + 0.84f * (static_cast<float>(i + 1) / static_cast<float>(ChestNodeCountPerRun + 1)) + progressJitter(rng),
            0.08f,
            0.92f);
        const Vec2 anchor = pointAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        const Vec2 tangent = tangentAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        Vec2 side = perpendicular(tangent);
        if (signDist(rng) == 0) {
            side = side * -1.0f;
        }

        ChestNode node;
        node.visibility = i % 4 == 0
            ? PlacementVisibility::Exposed
            : (i % 4 == 1 ? PlacementVisibility::BuriedVisible : PlacementVisibility::BuriedHidden);
        const float offsetTiles = node.visibility == PlacementVisibility::Exposed
            ? (1.0f + sideJitter(rng) * 0.5f)
            : (node.visibility == PlacementVisibility::BuriedVisible ? 6.5f : 8.5f) + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.chestKind = rollChestKind(rng, progress);
        node.depthRank = lootDepthRankForProgress(currentStageId_, progress);
        node.revealed = node.visibility != PlacementVisibility::BuriedHidden;
        node.opened = false;
        node.lootSpawned = false;
        node.openingSeconds = 0.0f;
        chestNodes_.push_back(node);
    }
}

void Game::updateChestNodes(float dt, const Input& input)
{
    const bool interact = input.confirmPressed() || input.useItemPressed();
    const float interactRadiusSq = ChestInteractRadius * ChestInteractRadius;

    for (ChestNode& node : chestNodes_) {
        if (node.opened) {
            if (!node.lootSpawned) {
                node.openingSeconds += dt;
                if (node.openingSeconds >= ChestLootReleaseSeconds) {
                    node.openingSeconds = ChestLootReleaseSeconds;
                    spawnChestLoot(node);
                }
            }
            continue;
        }

        if (node.visibility != PlacementVisibility::Exposed) {
            continue;
        }

        const Vec2 center = tileWorldCenter(node.tile);
        bool shouldOpen = interact && distanceSquared(player_.position, center) <= interactRadiusSq;
        if (!shouldOpen) {
            const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
            for (const SpellRingItem* itemPtr : runtimeItems) {
                if (itemPtr == nullptr || itemPtr->broken()) {
                    continue;
                }
                const float radius = itemPtr->hitRadius + ChestHitRadius;
                if (distanceSquared(itemPtr->worldPosition, center) <= radius * radius) {
                    shouldOpen = true;
                    break;
                }
            }
        }

        if (shouldOpen) {
            openChestNode(node);
        }
    }
}

void Game::revealChestNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles)
{
    if (openedTiles.empty()) {
        return;
    }

    for (Vec2 openedTile : openedTiles) {
        const DungeonTile tile{
            tileMap_.worldToTile(openedTile.x),
            tileMap_.worldToTile(openedTile.y),
        };
        for (ChestNode& node : chestNodes_) {
            if (node.opened || node.visibility == PlacementVisibility::Exposed ||
                node.tile.x != tile.x || node.tile.y != tile.y) {
                continue;
            }
            node.revealed = true;
            openChestNode(node);
        }
    }
}

bool Game::spawnWeightedObjectLoot(
    LootChestKind chestKind,
    int depthRank,
    Vec2 center,
    std::mt19937& rng,
    std::string_view sourceLabel,
    bool launchFromCenter)
{
    std::vector<const ObjectDefinition*> candidates;
    std::vector<double> weights;
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const double weight = lootWeightFor(object, currentStageId_, depthRank, chestKind);
        if (weight >= 1.0) {
            candidates.push_back(&object);
            weights.push_back(weight);
        }
    }

    if (candidates.empty()) {
        const std::string columnName = resolveLootWeightColumnName(currentStageId_, depthRank, chestKind);
        logError("[warning] " + std::string(sourceLabel) + ": no Objects candidates stage=\"" + currentStageId_ +
            "\" depth=" + std::to_string(depthRank) +
            " chest=" + chestKindCode(chestKind) +
            " column=\"" + columnName + "\"");
        return false;
    }

    std::discrete_distribution<int> itemDistribution(weights.begin(), weights.end());
    const int selected = itemDistribution(rng);
    const ObjectDefinition* object = candidates[static_cast<std::size_t>(selected)];
    const Vec2 target = scatterLootPosition(center, rng);
    return worldDrops_.spawnObjectDrop(
        objectCatalog_,
        object->id,
        target,
        runStats_.elapsedSeconds,
        launchFromCenter ? makeWorldLootJumpMotion(center, rng) : WorldDropSpawnMotion{});
}

void Game::openChestNode(ChestNode& node)
{
    if (node.opened) {
        return;
    }

    node.opened = true;
    node.revealed = true;
    node.lootSpawned = false;
    node.openingSeconds = 0.0f;

    reloadNotice_ = "Chest opened";
    reloadNoticeTimer_ = 1.4f;
}

void Game::spawnChestLoot(ChestNode& node)
{
    if (node.lootSpawned) {
        return;
    }

    node.lootSpawned = true;
    const Vec2 center = tileWorldCenter(node.tile);
    std::mt19937 rng(
        dungeonLayout_.seed ^
        (static_cast<std::uint32_t>(node.tile.x) * 0x85EBCA6Bu) ^
        (static_cast<std::uint32_t>(node.tile.y) * 0xC2B2AE35u) ^
        (static_cast<std::uint32_t>(runStats_.elapsedSeconds * 1000.0f) * 0x27D4EB2Du));

    const int itemRolls = lootItemRollCount(node.chestKind, rng);
    for (int i = 0; i < itemRolls; ++i) {
        if (!spawnWeightedObjectLoot(node.chestKind, node.depthRank, center, rng, "ChestLoot", true)) {
            break;
        }
    }

    const float totalMultiplier =
        lootStageMultiplier(balance_, currentStageId_) *
        lootDepthMultiplier(balance_, currentStageId_, node.depthRank) *
        lootGradeMultiplier(balance_, node.chestKind);

    std::bernoulli_distribution moneyChance(balance_.lootMoneyChance);
    if (moneyChance(rng)) {
        const auto [minMoney, maxMoney] = lootMoneyBaseRange(node.chestKind);
        std::uniform_int_distribution<int> moneyDistribution(minMoney, maxMoney);
        const int amount = scaledLootAmount(moneyDistribution(rng), totalMultiplier);
        const Vec2 target = scatterLootPosition(center, rng);
        worldDrops_.spawnMoneyDrop(amount, target, runStats_.elapsedSeconds, makeWorldLootJumpMotion(center, rng));
    }

    std::bernoulli_distribution materialChance(balance_.lootMaterialChance);
    if (materialChance(rng)) {
        std::uniform_int_distribution<int> materialDistribution(1, 3);
        const int amount = scaledLootAmount(materialDistribution(rng), totalMultiplier);
        const Vec2 target = scatterLootPosition(center, rng);
        const MaterialType materialType = rollChestMaterial(rng);
        worldDrops_.spawnMaterialDrop(
            materialType,
            amount,
            target,
            runStats_.elapsedSeconds,
            makeWorldLootJumpMotion(center, rng));
    }
}

void Game::initializeCrateNodesFromLayout()
{
    crateNodes_.clear();
    if (dungeonLayout_.mainPathPoints.size() < 2) {
        return;
    }

    std::mt19937 rng(dungeonLayout_.seed ^ 0xA31C2F17u);
    std::uniform_real_distribution<float> progressJitter(-0.030f, 0.030f);
    std::uniform_real_distribution<float> sideJitter(-1.6f, 1.6f);
    std::uniform_int_distribution<int> signDist(0, 1);

    for (int i = 0; i < CrateNodeCountPerRun; ++i) {
        const bool useBranch = !dungeonLayout_.branchPathPoints.empty() && i % 4 == 0;
        float progress = clamp(
            0.05f + 0.90f * (static_cast<float>(i + 1) / static_cast<float>(CrateNodeCountPerRun + 1)) + progressJitter(rng),
            0.05f,
            0.95f);
        Vec2 anchor = pointAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        Vec2 tangent = tangentAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        if (useBranch) {
            const DungeonPath& branch = dungeonLayout_.branchPathPoints[static_cast<std::size_t>(i) % dungeonLayout_.branchPathPoints.size()];
            const float branchProgress = clamp(0.30f + progressJitter(rng) * 4.0f, 0.15f, 0.85f);
            anchor = pointAtPathProgress(branch.points, branchProgress);
            tangent = tangentAtPathProgress(branch.points, branchProgress);
        }

        Vec2 side = perpendicular(tangent);
        if (signDist(rng) == 0) {
            side = side * -1.0f;
        }

        CrateNode node;
        const float offsetTiles = 1.2f + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.depthRank = lootDepthRankForProgress(currentStageId_, progress);
        node.destroyed = false;
        crateNodes_.push_back(node);
    }
}

void Game::updateCrateNodes()
{
    for (CrateNode& node : crateNodes_) {
        if (node.destroyed) {
            continue;
        }

        const Vec2 center = tileWorldCenter(node.tile);
        const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
        for (const SpellRingItem* itemPtr : runtimeItems) {
            if (itemPtr == nullptr || itemPtr->broken()) {
                continue;
            }
            const float radius = itemPtr->hitRadius + CrateHitRadius;
            if (distanceSquared(itemPtr->worldPosition, center) <= radius * radius) {
                destroyCrateNode(node);
                break;
            }
        }
    }
}

void Game::destroyCrateNode(CrateNode& node)
{
    if (node.destroyed) {
        return;
    }
    node.destroyed = true;

    const Vec2 center = tileWorldCenter(node.tile);
    effects_.spawnTileBreak(center, TileType::Dirt, CrateBreakParticleColor);

    std::mt19937 rng(
        dungeonLayout_.seed ^
        (static_cast<std::uint32_t>(node.tile.x) * 0x9E3779B9u) ^
        (static_cast<std::uint32_t>(node.tile.y) * 0x7F4A7C15u) ^
        (static_cast<std::uint32_t>(runStats_.elapsedSeconds * 1000.0f) * 0x165667B1u));

    const float totalMultiplier =
        lootStageMultiplier(balance_, currentStageId_) *
        lootDepthMultiplier(balance_, currentStageId_, node.depthRank) *
        lootGradeMultiplier(balance_, LootChestKind::Common);

    std::uniform_int_distribution<int> materialDistribution(1, 3);
    const int materialAmount = scaledLootAmount(materialDistribution(rng), totalMultiplier);
    const Vec2 materialTarget = scatterLootPosition(center, rng);
    worldDrops_.spawnMaterialDrop(
        MaterialType::OldWoodBuildingMaterial,
        materialAmount,
        materialTarget,
        runStats_.elapsedSeconds,
        makeWorldLootJumpMotion(center, rng));

    std::bernoulli_distribution moneyChance(balance_.crateMoneyChance);
    if (moneyChance(rng)) {
        std::uniform_int_distribution<int> moneyDistribution(5, 20);
        const int amount = scaledLootAmount(moneyDistribution(rng), totalMultiplier);
        const Vec2 moneyTarget = scatterLootPosition(center, rng);
        worldDrops_.spawnMoneyDrop(
            amount,
            moneyTarget,
            runStats_.elapsedSeconds,
            makeWorldLootJumpMotion(center, rng));
    }

    std::bernoulli_distribution bonusChance(balance_.crateBonusChance);
    if (bonusChance(rng)) {
        spawnWeightedObjectLoot(LootChestKind::Common, node.depthRank, center, rng, "CrateBonusLoot", true);
    }

    reloadNotice_ = "Crate broken";
    reloadNoticeTimer_ = 1.2f;
}

std::vector<CollisionRect> Game::solidObjectCollisionRects() const
{
    std::vector<CollisionRect> rects;
    rects.reserve(chestNodes_.size() + crateNodes_.size());

    for (const ChestNode& node : chestNodes_) {
        if (!node.opened && (node.visibility != PlacementVisibility::Exposed || !node.revealed)) {
            continue;
        }
        rects.push_back(collisionRectFromCenter(tileWorldCenter(node.tile), ChestCollisionSize));
    }

    for (const CrateNode& node : crateNodes_) {
        if (node.destroyed) {
            continue;
        }
        rects.push_back(collisionRectFromCenter(tileWorldCenter(node.tile), CrateCollisionSize));
    }

    return rects;
}

int Game::rewardNodeCount() const
{
    return static_cast<int>(std::count_if(rewardNodes_.begin(), rewardNodes_.end(), [](const RewardNode& node) {
        return !node.collected;
    }));
}

int Game::moneyNodeCount() const
{
    return static_cast<int>(std::count_if(moneyNodes_.begin(), moneyNodes_.end(), [](const MoneyNode& node) {
        return !node.collected;
    }));
}

int Game::buriedVisibleNodeCount() const
{
    int count = static_cast<int>(std::count_if(rewardNodes_.begin(), rewardNodes_.end(), [](const RewardNode& node) {
        return !node.collected && node.visibility == PlacementVisibility::BuriedVisible;
    }));
    count += static_cast<int>(std::count_if(moneyNodes_.begin(), moneyNodes_.end(), [](const MoneyNode& node) {
        return !node.collected && node.visibility == PlacementVisibility::BuriedVisible;
    }));
    count += static_cast<int>(std::count_if(moonFragmentNodes_.begin(), moonFragmentNodes_.end(), [](const MoonFragmentNode& node) {
        return !node.collected && node.visibility == PlacementVisibility::BuriedVisible;
    }));
    count += static_cast<int>(std::count_if(chestNodes_.begin(), chestNodes_.end(), [](const ChestNode& node) {
        return !node.opened && node.visibility == PlacementVisibility::BuriedVisible;
    }));
    return count;
}

int Game::buriedHiddenNodeCount() const
{
    int count = static_cast<int>(std::count_if(rewardNodes_.begin(), rewardNodes_.end(), [](const RewardNode& node) {
        return !node.collected && node.visibility == PlacementVisibility::BuriedHidden;
    }));
    count += static_cast<int>(std::count_if(moneyNodes_.begin(), moneyNodes_.end(), [](const MoneyNode& node) {
        return !node.collected && node.visibility == PlacementVisibility::BuriedHidden;
    }));
    count += static_cast<int>(std::count_if(chestNodes_.begin(), chestNodes_.end(), [](const ChestNode& node) {
        return !node.opened && node.visibility == PlacementVisibility::BuriedHidden;
    }));
    return count;
}

void Game::initializeEnemyNodesFromLayout()
{
    enemyNodes_.clear();
    if (dungeonLayout_.mainPathPoints.size() < 2) {
        return;
    }

    std::mt19937 rng(dungeonLayout_.seed ^ 0xE14B9D73u);
    std::uniform_real_distribution<float> progressJitter(-0.022f, 0.022f);
    std::uniform_real_distribution<float> sideJitter(-1.0f, 1.0f);
    std::uniform_int_distribution<int> signDist(0, 1);

    for (int i = 0; i < EnemyNodeCountPerRun; ++i) {
        const float progress = clamp(
            0.12f + 0.76f * (static_cast<float>(i + 1) / static_cast<float>(EnemyNodeCountPerRun + 1)) + progressJitter(rng),
            0.12f,
            0.88f);
        const Vec2 anchor = pointAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        const Vec2 tangent = tangentAtPathProgress(dungeonLayout_.mainPathPoints, progress);
        Vec2 side = perpendicular(tangent);
        if (signDist(rng) == 0) {
            side = side * -1.0f;
        }

        EnemyNode node;
        node.placementType = i % 3 == 0 ? EnemyPlacementType::BuriedHidden : EnemyPlacementType::Exposed;
        const float offsetTiles = node.placementType == EnemyPlacementType::Exposed
            ? (1.0f + sideJitter(rng))
            : (8.0f + sideJitter(rng));
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.dangerTier = std::max(1, dungeonLayout_.stageId + (i % 4 == 0 ? 1 : 0));
        node.enemySpawnGroup = "default";
        enemyNodes_.push_back(std::move(node));
    }

    for (const SpecialRoomAnchor& room : dungeonLayout_.specialRoomAnchors) {
        if (room.type == SpecialRoomType::SafeCavern) {
            continue;
        }
        if (room.type == SpecialRoomType::EnemyRoom) {
            for (int i = 0; i < 4; ++i) {
                const Vec2 offset = fromAngle(static_cast<float>(i) * Pi * 0.5f) * std::max(1.0f, room.radius * 0.45f);
                enemyNodes_.push_back(EnemyNode{
                    .tile = roundDungeonTile(room.center + offset),
                    .placementType = i < 2 ? EnemyPlacementType::Exposed : EnemyPlacementType::BuriedHidden,
                    .dangerTier = std::max(2, dungeonLayout_.stageId + 1),
                    .enemySpawnGroup = "enemy_room",
                    .spawned = false,
                });
            }
        } else if (room.type == SpecialRoomType::TreasureRoom && dungeonLayout_.stageId >= 2) {
            enemyNodes_.push_back(EnemyNode{
                .tile = roundDungeonTile(room.center + Vec2{0.0f, -room.radius}),
                .placementType = EnemyPlacementType::BuriedHidden,
                .dangerTier = std::max(2, dungeonLayout_.stageId),
                .enemySpawnGroup = "treasure_guard",
                .spawned = false,
            });
        } else if (room.type == SpecialRoomType::OreRoom && dungeonLayout_.stageId >= 3) {
            enemyNodes_.push_back(EnemyNode{
                .tile = roundDungeonTile(room.center),
                .placementType = EnemyPlacementType::Exposed,
                .dangerTier = dungeonLayout_.stageId,
                .enemySpawnGroup = "ore_guard",
                .spawned = false,
            });
        }
    }
}

void Game::updateExposedEnemyNodes()
{
    const float spawnRadiusSq = ExposedEnemyNodeSpawnRadius * ExposedEnemyNodeSpawnRadius;
    for (EnemyNode& node : enemyNodes_) {
        if (node.spawned || node.placementType != EnemyPlacementType::Exposed) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (distanceSquared(center, player_.position) > spawnRadiusSq) {
            continue;
        }
        if (enemies_.spawnNodeEnemy(tileMap_, center, player_.position, balance_, enemyCatalog_, false)) {
            node.spawned = true;
        }
    }
}

void Game::updateRingEffectDiscoveries(std::vector<EffectDiscoveryEvent>& discoveryEvents)
{
    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->objectId.empty() || itemPtr->broken()) {
            continue;
        }
        const ObjectDefinition* object = objectCatalog_.registry.findById(itemPtr->objectId);
        if (object == nullptr) {
            continue;
        }
        const auto hasOrbitEffect = [object](std::string_view effectCode) {
            for (const EffectSpec& spec : object->orbitEffects) {
                for (const std::string& effect : spec.effects) {
                    if (effect == effectCode) {
                        return true;
                    }
                }
            }
            return false;
        };

        for (std::string_view orbitEffectKey : {"orbit_speed", "orbit_power", "damage_speed"}) {
            if (hasOrbitEffect(orbitEffectKey) && !encyclopedia_.hasObjectEffect(object->id, orbitEffectKey)) {
                discoveryEvents.push_back(EffectDiscoveryEvent{
                    .objectId = object->id,
                    .objectName = object->name,
                    .effectKey = std::string(orbitEffectKey),
                    .description = {},
                    .note = {},
                    .position = itemPtr->worldPosition,
                });
            }
        }

        if (itemPtr->lightRadius > 0.0f && !encyclopedia_.hasObjectEffect(object->id, "light")) {
            discoveryEvents.push_back(EffectDiscoveryEvent{
                .objectId = object->id,
                .objectName = object->name,
                .effectKey = "light",
                .description = {},
                .note = {},
                .position = itemPtr->worldPosition,
            });
        }

        bool detectedHidden = false;
        if (itemPtr->hiddenDetectionRadius > 0.0f && !encyclopedia_.hasObjectEffect(object->id, "detect_hidden")) {
            const float radiusSq = itemPtr->hiddenDetectionRadius * itemPtr->hiddenDetectionRadius;
            for (RewardNode& node : rewardNodes_) {
                if (node.collected || node.visibility != PlacementVisibility::BuriedHidden) {
                    continue;
                }
                if (distanceSquared(tileWorldCenter(node.tile), itemPtr->worldPosition) > radiusSq) {
                    continue;
                }
                node.visibility = PlacementVisibility::BuriedVisible;
                node.revealed = true;
                detectedHidden = true;
            }
            for (MoneyNode& node : moneyNodes_) {
                if (node.collected || node.visibility != PlacementVisibility::BuriedHidden) {
                    continue;
                }
                if (distanceSquared(tileWorldCenter(node.tile), itemPtr->worldPosition) > radiusSq) {
                    continue;
                }
                node.visibility = PlacementVisibility::BuriedVisible;
                detectedHidden = true;
            }
            for (ChestNode& node : chestNodes_) {
                if (node.opened || node.visibility != PlacementVisibility::BuriedHidden) {
                    continue;
                }
                if (distanceSquared(tileWorldCenter(node.tile), itemPtr->worldPosition) > radiusSq) {
                    continue;
                }
                node.visibility = PlacementVisibility::BuriedVisible;
                node.revealed = true;
                detectedHidden = true;
            }
            for (const EnemyNode& node : enemyNodes_) {
                if (node.spawned || node.placementType != EnemyPlacementType::BuriedHidden) {
                    continue;
                }
                if (distanceSquared(tileWorldCenter(node.tile), itemPtr->worldPosition) > radiusSq) {
                    continue;
                }
                detectedHidden = true;
            }
            if (detectedHidden) {
                discoveryEvents.push_back(EffectDiscoveryEvent{
                    .objectId = object->id,
                    .objectName = object->name,
                    .effectKey = "detect_hidden",
                    .description = {},
                    .note = {},
                    .position = itemPtr->worldPosition,
                });
            }
        }

        if (itemPtr->treasureDetectionRadius > 0.0f && !encyclopedia_.hasObjectEffect(object->id, "detect_treasure")) {
            bool detectedTreasure = false;
            const float radiusSq = itemPtr->treasureDetectionRadius * itemPtr->treasureDetectionRadius;
            for (RewardNode& node : rewardNodes_) {
                if (node.collected) {
                    continue;
                }
                const bool treasureNode = node.rewardKind.find("treasure") != std::string::npos;
                if (!treasureNode) {
                    continue;
                }
                if (distanceSquared(tileWorldCenter(node.tile), itemPtr->worldPosition) > radiusSq) {
                    continue;
                }
                if (node.visibility == PlacementVisibility::BuriedHidden) {
                    node.visibility = PlacementVisibility::BuriedVisible;
                    node.revealed = true;
                }
                detectedTreasure = true;
            }
            for (ChestNode& node : chestNodes_) {
                if (node.opened) {
                    continue;
                }
                if (distanceSquared(tileWorldCenter(node.tile), itemPtr->worldPosition) > radiusSq) {
                    continue;
                }
                if (node.visibility == PlacementVisibility::BuriedHidden) {
                    node.visibility = PlacementVisibility::BuriedVisible;
                    node.revealed = true;
                }
                detectedTreasure = true;
            }
            if (detectedTreasure) {
                discoveryEvents.push_back(EffectDiscoveryEvent{
                    .objectId = object->id,
                    .objectName = object->name,
                    .effectKey = "detect_treasure",
                    .description = {},
                    .note = {},
                    .position = itemPtr->worldPosition,
                });
            }
        }
    }
}

std::vector<Vec2> Game::spawnHiddenEnemyNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles)
{
    std::vector<Vec2> randomSpawnTiles;
    randomSpawnTiles.reserve(openedTiles.size());

    for (Vec2 openedTile : openedTiles) {
        const DungeonTile tile{
            tileMap_.worldToTile(openedTile.x),
            tileMap_.worldToTile(openedTile.y),
        };

        bool consumedByHiddenNode = false;
        for (EnemyNode& node : enemyNodes_) {
            if (node.spawned || node.placementType != EnemyPlacementType::BuriedHidden ||
                node.tile.x != tile.x || node.tile.y != tile.y) {
                continue;
            }

            consumedByHiddenNode = true;
            if (enemies_.spawnNodeEnemy(tileMap_, tileWorldCenter(node.tile), player_.position, balance_, enemyCatalog_, true, true)) {
                node.spawned = true;
                reloadNotice_ = "隠れ敵が出現";
                reloadNoticeTimer_ = 1.5f;
            }
            break;
        }

        if (!consumedByHiddenNode) {
            randomSpawnTiles.push_back(openedTile);
        }
    }

    return randomSpawnTiles;
}

int Game::exposedEnemyNodeCount() const
{
    return static_cast<int>(std::count_if(enemyNodes_.begin(), enemyNodes_.end(), [](const EnemyNode& node) {
        return !node.spawned && node.placementType == EnemyPlacementType::Exposed;
    }));
}

int Game::buriedEnemyNodeCount() const
{
    return static_cast<int>(std::count_if(enemyNodes_.begin(), enemyNodes_.end(), [](const EnemyNode& node) {
        return !node.spawned && node.placementType == EnemyPlacementType::BuriedHidden;
    }));
}

int Game::spawnedEnemyNodeCount() const
{
    return static_cast<int>(std::count_if(enemyNodes_.begin(), enemyNodes_.end(), [](const EnemyNode& node) {
        return node.spawned;
    }));
}

void Game::configureBossSpawnPointFromWarp(Vec2 warpPosition)
{
    Vec2 direction = normalize(warpPosition);
    if (lengthSquared(direction) <= 0.0001f) {
        direction = {1.0f, 0.0f};
    }
    bossSpawnPoint_ = warpPosition + direction * static_cast<float>(BossOffsetTiles * balance::TileSize);
    hasBossSpawnPoint_ = true;
}

void Game::updateBossSpawn()
{
    if (!hasBossSpawnPoint_ || bossSpawned_ || !warpPointsEnabled_) {
        return;
    }
    if (distanceSquared(player_.position, bossSpawnPoint_) > BossSpawnTriggerRadius * BossSpawnTriggerRadius) {
        return;
    }

    bossSpawned_ = enemies_.spawnBossNear(tileMap_, bossSpawnPoint_, player_.position, balance_, enemyCatalog_);
    if (bossSpawned_) {
        reloadNotice_ = "深部の主が出現";
        reloadNoticeTimer_ = 2.0f;
    }
}

void Game::captureRetrySnapshotAtWarpPoint()
{
    retrySnapshot_.playerPosition = player_.position;
    retrySnapshot_.playerFacing = player_.facing;
    retrySnapshot_.playerHp = player_.hp;
    retrySnapshot_.playerMaxHp = player_.maxHp;
    retrySnapshot_.playerLevel = player_.level;
    retrySnapshot_.playerXp = player_.xp;
    retrySnapshot_.playerXpToNext = player_.xpToNext;
    retrySnapshot_.inventory = captureInventoryCarryState();
    retrySnapshot_.tileMap = tileMap_;
    retrySnapshot_.dungeonLayout = dungeonLayout_;
    retrySnapshot_.runStats = runStats_;
    retrySnapshot_.warpPoints = warpPoints_;
    retrySnapshot_.rewardNodes = rewardNodes_;
    retrySnapshot_.moneyNodes = moneyNodes_;
    retrySnapshot_.moonFragmentNodes = moonFragmentNodes_;
    retrySnapshot_.chestNodes = chestNodes_;
    retrySnapshot_.crateNodes = crateNodes_;
    retrySnapshot_.enemyNodes = enemyNodes_;
    retrySnapshot_.enemies = enemies_;
    retrySnapshot_.worldDrops = worldDrops_;
    retrySnapshot_.spawnedWarpPointCount = spawnedWarpPointCount_;
    retrySnapshot_.unlockedWarpPointCount = unlockedWarpPointCount_;
    retrySnapshot_.bossSpawnPoint = bossSpawnPoint_;
    retrySnapshot_.hasBossSpawnPoint = hasBossSpawnPoint_;
    retrySnapshot_.bossSpawned = bossSpawned_;
    retrySnapshot_.valid = true;
}

void Game::restoreRetrySnapshot()
{
    if (!retrySnapshot_.valid) {
        return;
    }

    player_.position = retrySnapshot_.playerPosition;
    player_.facing = retrySnapshot_.playerFacing;
    player_.maxHp = retrySnapshot_.playerMaxHp;
    player_.hp = retrySnapshot_.playerMaxHp;
    player_.level = retrySnapshot_.playerLevel;
    player_.xp = retrySnapshot_.playerXp;
    player_.xpToNext = retrySnapshot_.playerXpToNext;
    player_.velocity = {};
    player_.throwCooldownRemaining = 0.0f;
    player_.poisonDamageAccumulator = 0.0;
    player_.lastDamageSource = DamageSource::Unknown;
    player_.lastDamageEnemyName.clear();
    player_.status = EntityStatus{};

    tileMap_ = retrySnapshot_.tileMap;
    dungeonLayout_ = retrySnapshot_.dungeonLayout;
    restoreInventoryCarryState(retrySnapshot_.inventory);
    runStats_ = retrySnapshot_.runStats;
    warpPoints_ = retrySnapshot_.warpPoints;
    rewardNodes_ = retrySnapshot_.rewardNodes;
    moneyNodes_ = retrySnapshot_.moneyNodes;
    moonFragmentNodes_ = retrySnapshot_.moonFragmentNodes;
    chestNodes_ = retrySnapshot_.chestNodes;
    crateNodes_ = retrySnapshot_.crateNodes;
    enemyNodes_ = retrySnapshot_.enemyNodes;
    enemies_ = retrySnapshot_.enemies;
    enemies_.clearTemporaryState();
    worldDrops_ = retrySnapshot_.worldDrops;
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    spawnedWarpPointCount_ = retrySnapshot_.spawnedWarpPointCount;
    unlockedWarpPointCount_ = retrySnapshot_.unlockedWarpPointCount;
    bossSpawnPoint_ = retrySnapshot_.bossSpawnPoint;
    hasBossSpawnPoint_ = retrySnapshot_.hasBossSpawnPoint;
    bossSpawned_ = retrySnapshot_.bossSpawned;
    effects_ = EffectSystem{};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
    levels_ = LevelSystem{};
    inventoryReturnToPause_ = false;
    gameOverStatus_.clear();
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    camera_.follow(player_.position, 1.0f);
    resetPlayerFootstepDust();
}

void Game::resetPlayerFootstepDust()
{
    for (FootstepDustPuff& puff : playerFootstepDustPuffs_) {
        puff = {};
    }
    nextPlayerFootstepDustPuff_ = 0;
    nextPlayerFootstepDustShape_ = 0;
    previousPlayerDustFrame_ = -1;
    previousBasePlayerDustFrame_ = -1;
}

void Game::updatePlayerFootstepDust(float dt)
{
    for (FootstepDustPuff& puff : playerFootstepDustPuffs_) {
        if (!puff.active) {
            continue;
        }
        puff.age += dt;
        if (puff.age >= puff.lifetime) {
            puff.active = false;
        }
    }
}

void Game::maybeSpawnPlayerFootstepDust(Vec2 footAnchor, Vec2 movementDirection, bool walking, int frameIndex, int& previousFrame)
{
    const bool dustFrame = frameIndex == 3 || frameIndex == 6;
    if (walking && dustFrame && previousFrame != frameIndex) {
        spawnPlayerFootstepDust(footAnchor, movementDirection);
    }
    previousFrame = frameIndex;
}

void Game::spawnPlayerFootstepDust(Vec2 footAnchor, Vec2 movementDirection)
{
    const Vec2 backward = lengthSquared(movementDirection) > 0.0001f
        ? normalize(movementDirection) * -1.0f
        : Vec2{0.0f, 1.0f};

    FootstepDustPuff& puff = playerFootstepDustPuffs_[static_cast<std::size_t>(nextPlayerFootstepDustPuff_)];
    puff.active = true;
    puff.age = 0.0f;
    puff.lifetime = FootstepDustLifetime;
    puff.shapeIndex = nextPlayerFootstepDustShape_;
    puff.startPosition = footAnchor + backward * FootstepDustStartOffset;
    puff.endPosition = footAnchor + backward * FootstepDustEndOffset;

    nextPlayerFootstepDustPuff_ = (nextPlayerFootstepDustPuff_ + 1) % static_cast<int>(playerFootstepDustPuffs_.size());
    nextPlayerFootstepDustShape_ = (nextPlayerFootstepDustShape_ + 1) % static_cast<int>(FootstepDustShapes.size());
}

void Game::renderPlayerFootstepDust(Renderer& renderer) const
{
    for (const FootstepDustPuff& puff : playerFootstepDustPuffs_) {
        if (!puff.active || puff.lifetime <= 0.0f) {
            continue;
        }

        const float t = clamp(puff.age / puff.lifetime, 0.0f, 1.0f);
        const float easedMove = 1.0f - (1.0f - t) * (1.0f - t);
        const float easedScale = 1.0f + t * 0.28f;
        const unsigned char alpha = static_cast<unsigned char>(
            std::clamp(std::lround(static_cast<float>(FootstepDustBaseAlpha) * (1.0f - t)), 0L, 255L));
        if (alpha == 0) {
            continue;
        }

        const Vec2 position = lerp(puff.startPosition, puff.endPosition, easedMove);
        const FootstepDustShape& shape = FootstepDustShapes[static_cast<std::size_t>(
            std::clamp(puff.shapeIndex, 0, static_cast<int>(FootstepDustShapes.size()) - 1))];
        for (int i = 0; i < shape.circleCount; ++i) {
            const Vec2 offset = shape.offsets[static_cast<std::size_t>(i)] * easedScale;
            const float radius = shape.radii[static_cast<std::size_t>(i)] * easedScale;
            renderer.fillCircle(position + offset, radius, {222, 200, 200, alpha});
        }
    }
}

void Game::spawnRingEquipFx(const RingEquipFxRequest& request)
{
    RingEquipFx fx;
    fx.sourceScreen = request.sourceScreen;
    fx.ringIndex = request.ringIndex;
    fx.itemIndex = request.itemIndex;
    fx.localAngle = request.localAngle;
    fx.objectId = request.objectId;
    fx.instanceId = request.instanceId;
    fx.duration = 0.36f;
    fx.arcSign = ((request.itemIndex + request.ringIndex) % 2 == 0) ? 1.0f : -1.0f;
    ringEquipFx_.push_back(std::move(fx));
    if (ringEquipFx_.size() > 8) {
        ringEquipFx_.erase(ringEquipFx_.begin());
    }
}

void Game::updateRingEquipFx(float dt)
{
    for (RingEquipFx& fx : ringEquipFx_) {
        fx.age += dt;
    }
    ringEquipFx_.erase(
        std::remove_if(ringEquipFx_.begin(), ringEquipFx_.end(), [](const RingEquipFx& fx) {
            return fx.age >= fx.duration;
        }),
        ringEquipFx_.end());
}

Vec2 Game::ringEquipFxTargetScreen(const RingEquipFx& fx) const
{
    const int ringIndex = std::clamp(fx.ringIndex, 0, SpellRingCount - 1);
    const std::vector<SpellRingItem>& items = spellRing_.itemsForRing(ringIndex);
    const auto matchesFx = [&fx](const SpellRingItem& item) {
        if (!fx.instanceId.empty()) {
            return item.instanceId == fx.instanceId;
        }
        return fx.objectId.empty() || item.objectId == fx.objectId;
    };

    if (fx.itemIndex >= 0 && fx.itemIndex < static_cast<int>(items.size())) {
        const SpellRingItem& item = items[static_cast<std::size_t>(fx.itemIndex)];
        if (matchesFx(item)) {
            return camera_.worldToScreen(item.worldPosition);
        }
    }
    for (const SpellRingItem& item : items) {
        if (matchesFx(item)) {
            return camera_.worldToScreen(item.worldPosition);
        }
    }

    const int itemCount = std::max(1, static_cast<int>(items.size()));
    const int itemIndex = std::clamp(fx.itemIndex, 0, itemCount - 1);
    const Vec2 fallbackWorld = spellRing_.sampleItemWorldPositionForRing(
        ringIndex,
        fx.localAngle,
        itemIndex,
        itemCount,
        1.0f,
        balance_);
    return camera_.worldToScreen(fallbackWorld);
}

void Game::renderRingEquipFx(Renderer& renderer) const
{
    if (ringEquipFx_.empty() || mode_ != ScreenMode::Playing) {
        return;
    }

    renderer.setScreenSpace();
    const auto easeOutCubic = [](float t) {
        const float inv = 1.0f - clamp(t, 0.0f, 1.0f);
        return 1.0f - inv * inv * inv;
    };
    const auto bezier = [](Vec2 a, Vec2 b, Vec2 c, Vec2 d, float t) {
        const float u = 1.0f - t;
        return a * (u * u * u) + b * (3.0f * u * u * t) + c * (3.0f * u * t * t) + d * (t * t * t);
    };

    for (const RingEquipFx& fx : ringEquipFx_) {
        if (fx.duration <= 0.0f) {
            continue;
        }
        const float t = clamp(fx.age / fx.duration, 0.0f, 1.0f);
        const float progress = easeOutCubic(t);
        const float fade = std::sin(clamp(t, 0.0f, 1.0f) * Pi);
        if (fade <= 0.001f) {
            continue;
        }

        const Vec2 p0 = fx.sourceScreen;
        const Vec2 p3 = ringEquipFxTargetScreen(fx);
        const Vec2 delta = p3 - p0;
        const float dist = std::max(1.0f, length(delta));
        const Vec2 side = Vec2{-delta.y / dist, delta.x / dist} * fx.arcSign;
        const float arc = std::clamp(dist * 0.16f, 24.0f, 86.0f);
        const Vec2 p1 = p0 + delta * 0.28f + side * arc;
        const Vec2 p2 = p0 + delta * 0.76f + side * (arc * 0.42f);
        const float tail = 0.34f;
        const float start = std::max(0.0f, progress - tail);
        constexpr int SampleCount = 12;
        std::array<Vec2, SampleCount> points{};
        for (int i = 0; i < SampleCount; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(SampleCount - 1);
            points[static_cast<std::size_t>(i)] = bezier(p0, p1, p2, p3, lerp(start, progress, u));
        }

        for (int i = 1; i < SampleCount; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(SampleCount - 1);
            const unsigned char glowAlpha = static_cast<unsigned char>(std::clamp(std::lround(82.0f * fade * u), 0L, 255L));
            const unsigned char coreAlpha = static_cast<unsigned char>(std::clamp(std::lround(220.0f * fade * u), 0L, 255L));
            renderer.drawSoftLine(points[static_cast<std::size_t>(i - 1)], points[static_cast<std::size_t>(i)], 18.0f, {132, 204, 255, glowAlpha});
            renderer.drawSoftLine(points[static_cast<std::size_t>(i - 1)], points[static_cast<std::size_t>(i)], 7.0f, {255, 228, 128, coreAlpha});
            renderer.drawSoftLine(points[static_cast<std::size_t>(i - 1)], points[static_cast<std::size_t>(i)], 2.5f, {255, 255, 245, coreAlpha});
        }

        const Vec2 head = points.back();
        const unsigned char headAlpha = static_cast<unsigned char>(std::clamp(std::lround(235.0f * fade), 0L, 255L));
        renderer.fillSoftCircle(head, 13.0f, {126, 214, 255, static_cast<unsigned char>(headAlpha / 2)});
        renderer.fillSoftCircle(head, 6.5f, {255, 240, 154, headAlpha});
        renderer.fillSoftCircle(head, 2.2f, {255, 255, 255, headAlpha});

        if (t > 0.62f) {
            const float hitT = clamp((t - 0.62f) / 0.38f, 0.0f, 1.0f);
            const unsigned char ringAlpha = static_cast<unsigned char>(std::clamp(std::lround(190.0f * (1.0f - hitT)), 0L, 255L));
            renderer.drawSoftRing(p3, lerp(7.0f, 24.0f, hitT), 5.0f, {255, 232, 136, ringAlpha});
        }
    }
}

BaseEditRect Game::baseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect fallback) const
{
    const auto& table = area == BaseArea::Outdoor ? baseFacilityRectsOutdoor_ : baseFacilityRectsHome_;
    const auto it = table.find(std::string(facilityId));
    if (it == table.end()) {
        return normalizeBaseEditRect(fallback);
    }
    return normalizeBaseEditRect(it->second);
}

void Game::setBaseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect rect)
{
    auto& table = area == BaseArea::Outdoor ? baseFacilityRectsOutdoor_ : baseFacilityRectsHome_;
    table[std::string(facilityId)] = normalizeBaseEditRect(rect);
}

bool Game::isBasePassabilityBlocked(BaseArea area, int tileX, int tileY) const
{
    const auto& blocked = area == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
    return blocked.find(packBaseEditTile(tileX, tileY)) != blocked.end();
}

void Game::setBasePassabilityBlocked(BaseArea area, int tileX, int tileY, bool blocked)
{
    auto& table = area == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
    const std::int64_t key = packBaseEditTile(tileX, tileY);
    if (blocked) {
        table.insert(key);
    } else {
        table.erase(key);
    }
}

void Game::loadBaseEditData()
{
    baseFacilityRectsOutdoor_.clear();
    baseFacilityRectsHome_.clear();
    baseBlockedTilesOutdoor_.clear();
    baseBlockedTilesHome_.clear();
    baseEditDirty_ = false;

    for (BaseArea area : {BaseArea::Outdoor, BaseArea::HomeInterior}) {
        auto& facilityRects = area == BaseArea::Outdoor ? baseFacilityRectsOutdoor_ : baseFacilityRectsHome_;
        auto& blockedTiles = area == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
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
                    facilityRects[id] = normalizeBaseEditRect(rect);
                }
            } else if (key == "blocked") {
                int tileX = 0;
                int tileY = 0;
                stream >> tileX >> tileY;
                if (!stream.fail()) {
                    blockedTiles.insert(packBaseEditTile(tileX, tileY));
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

        file << "MAJO_BASE_EDIT_V1\n";

        std::vector<BaseFacility> facilities = baseFacilities(area, ringWorkshopUnlocked_);
        for (BaseFacility& facility : facilities) {
            const BaseEditRect rect = baseFacilityRectFor(area, facility.facilityId, toBaseEditRect(facility.rect));
            file << "facility "
                << facility.facilityId << " "
                << rect.x << " " << rect.y << " " << rect.w << " " << rect.h << "\n";
        }

        const auto& blocked = area == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
        std::vector<std::int64_t> blockedSorted(blocked.begin(), blocked.end());
        std::sort(blockedSorted.begin(), blockedSorted.end());
        for (const std::int64_t packed : blockedSorted) {
            file << "blocked " << baseEditTileXFromPacked(packed) << " " << baseEditTileYFromPacked(packed) << "\n";
        }

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
    objectImageScaleObjectIds_.clear();
    objectImageScaleObjectIds_.reserve(objectCatalog_.objects.size());
    std::unordered_set<std::string> seen;
    seen.reserve(objectCatalog_.objects.size());

    for (const ObjectDefinition& object : objectCatalog_.objects) {
        if (object.id.empty() || object.imageNumber <= 0) {
            continue;
        }
        if (!seen.insert(object.id).second) {
            continue;
        }
        objectImageScaleObjectIds_.push_back(object.id);
    }

    std::sort(objectImageScaleObjectIds_.begin(), objectImageScaleObjectIds_.end(), [this](const std::string& left, const std::string& right) {
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

    if (objectImageScaleObjectIds_.empty()) {
        objectImageScaleSelectedIndex_ = -1;
    } else if (objectImageScaleSelectedIndex_ < 0 || objectImageScaleSelectedIndex_ >= static_cast<int>(objectImageScaleObjectIds_.size())) {
        objectImageScaleSelectedIndex_ = 0;
    }

    otherImageScaleKeys_.clear();
    for (const WorldIconDefinition& definition : worldIconDefinitions()) {
        if (!definition.key.empty() && definition.imageNumber > 0) {
            otherImageScaleKeys_.push_back(std::string(definition.key));
        }
    }

    if (otherImageScaleKeys_.empty()) {
        otherImageScaleSelectedIndex_ = -1;
    } else if (otherImageScaleSelectedIndex_ < 0 || otherImageScaleSelectedIndex_ >= static_cast<int>(otherImageScaleKeys_.size())) {
        otherImageScaleSelectedIndex_ = 0;
    }
}

void Game::enterObjectImageScaleEditMode()
{
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        return;
    }

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
}

void Game::exitObjectImageScaleEditMode()
{
    if (mode_ != ScreenMode::ObjectImageScaleEdit) {
        return;
    }

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
    }
    if (ui.pressed(objectImageScaleTabRect(1))) {
        imageScaleEditTab_ = ImageScaleEditTab::Others;
        objectImageScaleStatus_ = "Others";
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

    const ObjectImageScaleLayout layout = makeObjectImageScaleLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(itemKeys.size());
    const int columns = std::max(1, layout.columns);
    const int rows = itemCount <= 0 ? 0 : (itemCount + columns - 1) / columns;
    const float contentHeight = rows <= 0
        ? 0.0f
        : static_cast<float>(rows) * ObjectImageScaleCardHeight + static_cast<float>(rows - 1) * ObjectImageScaleCardGap;
    const float maxScroll = std::max(0.0f, contentHeight - layout.viewport.size.y);
    scrollOffset = clamp(scrollOffset, 0.0f, maxScroll);

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

    const ObjectImageScaleLayout layout = makeObjectImageScaleLayout(camera_.width(), camera_.height());
    const int itemCount = static_cast<int>(itemKeys.size());
    const int columns = std::max(1, layout.columns);
    const int rows = itemCount <= 0 ? 0 : (itemCount + columns - 1) / columns;
    const float contentHeight = rows <= 0
        ? 0.0f
        : static_cast<float>(rows) * ObjectImageScaleCardHeight + static_cast<float>(rows - 1) * ObjectImageScaleCardGap;
    const float maxScroll = std::max(0.0f, contentHeight - layout.viewport.size.y);
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

    renderer.drawText({22.0f, 58.0f}, "Click to select  Wheel: scroll / selected card wheel: scale  Up/Down: scale  Ctrl+S: save  Esc: exit", {198, 206, 222, 255}, 2);
    renderer.drawRect(layout.viewport.pos, layout.viewport.size, {96, 108, 132, 255});

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
                drewImage = drawObjectImage(renderer, *object, previewCenter, {ObjectImageScalePreviewSize, ObjectImageScalePreviewSize}, options);
                name = !object->name.empty() ? object->name : key;
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

    const char* dirty = objectImageScaleDirty_ ? "Unsaved (*)" : "Saved";
    renderer.drawText({22.0f, static_cast<float>(camera_.height()) - 40.0f}, dirty, objectImageScaleDirty_ ? Color{255, 230, 150, 255} : Color{170, 220, 170, 255}, 2);
    if (!objectImageScaleStatus_.empty()) {
        renderer.drawText({220.0f, static_cast<float>(camera_.height()) - 40.0f}, objectImageScaleStatus_, {198, 206, 222, 255}, 2);
    }
}

void Game::enterEnemyTestMode()
{
    if (!enemyTestActive_ && mode_ == ScreenMode::Playing) {
        captureDungeonState();
    }

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }

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
    enemies_ = EnemySystem{};
    projectiles_ = ProjectileSystem{};
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
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    clearTemporaryPlayerState(true);
    enterBase();
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
    if (enemies_.spawnSpecificEnemy(tileMap_, enemy.id, desiredPosition, player_.position, balance_, enemyCatalog_, true, true)) {
        enemyTestStatus_ = "召喚: " + (enemy.name.empty() ? enemy.id : enemy.name);
    } else {
        enemyTestStatus_ = "召喚できませんでした: " + (enemy.name.empty() ? enemy.id : enemy.name);
    }
}

void Game::clearEnemyTestArena()
{
    enemies_ = EnemySystem{};
    projectiles_ = ProjectileSystem{};
    effects_ = EffectSystem{};
    enemyTestStatus_ = "敵と弾を消去しました";
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
    baseStorageActive_ = false;
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    closeUiCommandMenu(baseMerchantSellCommandMenu_);
    baseMerchantSellCommandIndex_ = -1;
    closeUiCommandMenu(baseMerchantBuyCommandMenu_);
    baseMerchantBuyCommandIndex_ = -1;
    baseUpgradeActive_ = false;
    baseProcessingActive_ = false;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingCommandSlot_ = -1;
    baseRingWorkshopActive_ = false;
    baseBookshelfActive_ = false;

    baseEditEnabled_ = true;
    baseEditMode_ = BaseEditMode::Facility;
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
    snapshot.outdoorBlockedTiles = baseBlockedTilesOutdoor_;
    snapshot.homeBlockedTiles = baseBlockedTilesHome_;
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
    current.outdoorBlockedTiles = baseBlockedTilesOutdoor_;
    current.homeBlockedTiles = baseBlockedTilesHome_;
    baseEditRedoStack_.push_back(std::move(current));

    const BaseEditSnapshot snapshot = std::move(baseEditUndoStack_.back());
    baseEditUndoStack_.pop_back();
    baseFacilityRectsOutdoor_ = snapshot.outdoorFacilityRects;
    baseFacilityRectsHome_ = snapshot.homeFacilityRects;
    baseBlockedTilesOutdoor_ = snapshot.outdoorBlockedTiles;
    baseBlockedTilesHome_ = snapshot.homeBlockedTiles;
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
    current.outdoorBlockedTiles = baseBlockedTilesOutdoor_;
    current.homeBlockedTiles = baseBlockedTilesHome_;
    baseEditUndoStack_.push_back(std::move(current));
    if (static_cast<int>(baseEditUndoStack_.size()) > BaseEditUndoLimit) {
        baseEditUndoStack_.erase(baseEditUndoStack_.begin());
    }

    const BaseEditSnapshot snapshot = std::move(baseEditRedoStack_.back());
    baseEditRedoStack_.pop_back();
    baseFacilityRectsOutdoor_ = snapshot.outdoorFacilityRects;
    baseFacilityRectsHome_ = snapshot.homeFacilityRects;
    baseBlockedTilesOutdoor_ = snapshot.outdoorBlockedTiles;
    baseBlockedTilesHome_ = snapshot.homeBlockedTiles;
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
        const auto& blocked = baseArea_ == BaseArea::Outdoor ? baseBlockedTilesOutdoor_ : baseBlockedTilesHome_;
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

    renderer.fillRect({884.0f, 38.0f}, {360.0f, 112.0f}, {10, 16, 28, 210});
    renderer.drawRect({884.0f, 38.0f}, {360.0f, 112.0f}, {130, 168, 232, 255});
    drawUiButton(renderer, baseEditModeButtonRect(0), "Facility", facilityMode);
    drawUiButton(renderer, baseEditModeButtonRect(1), "Passability", passabilityMode);
    drawUiButton(renderer, baseEditSaveButtonRect(), "Save", false, uiActionButtonStyle());
    renderer.drawText({898.0f, 132.0f}, baseEditDirty_ ? "Unsaved changes (*)" : "Saved", {255, 230, 150, 255}, 2);
    renderer.drawText({898.0f, 158.0f}, "Ctrl+S save / Ctrl+Z undo / Shift+Click fill / Esc exit", {198, 198, 206, 255}, 2);
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

bool Game::loadSaveData()
{
    const std::filesystem::path path = saveDataPath();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        logError("[save] no save file: " + path.string());
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        logError("[warning] SaveData: empty or unreadable file; starting with new data");
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line != "MAJO_SHOVEL_SAVE_V1") {
        logError("[warning] SaveData: invalid header; starting with new data");
        return false;
    }

    InventorySystem loadedInventory;
    EncyclopediaSystem loadedEncyclopedia;
    std::array<std::vector<SpellRingItem>, SpellRingCount> loadedRingItemsByRing{};
    std::vector<InventoryObjectStack> loadedWarehouseStacks;
    std::vector<InventoryObjectInstance> loadedWarehouseInstances;
    int loadedMoney = 0;
    int loadedCurrentStage = 0;
    std::string loadedCurrentStageId = currentStageId_;
    int loadedUnlockedStages = 1;
    int loadedUnlockedWarpPointCount = 0;
    bool loadedHasLatestWarpPointPosition = false;
    Vec2 loadedLatestWarpPointPosition{};
    int loadedPlayerLevel = 1;
    int loadedPlayerXp = 0;
    int loadedPlayerXpToNext = balance_.xpBase + loadedPlayerLevel * balance_.xpPerLevel;
    int loadedMaxHpUpgradeLevel = 0;
    int loadedRingRadiusUpgradeLevel = 0;
    int loadedRingSpeedUpgradeLevel = 0;
    int loadedLevelRingRadiusPoints = 0;
    int loadedLevelRingSpeedPoints = 0;
    int loadedWorkshopInitialRadiusLevel = 0;
    int loadedWorkshopInitialSpeedLevel = 0;
    int loadedWorkshopShiftDistanceLevel = 0;
    bool loadedMerchantRefreshPending = false;
    int loadedMerchantUpgradeLevel = 1;
    int loadedMerchantStockVersion = 0;
    std::vector<MerchantProduct> loadedMerchantStock;
    std::string loadedHighValueBuyCategory;
    int loadedWarehouseCapacityLevel = 0;
    int loadedProcessingUnlockLevel = 0;
    bool loadedRingWorkshopUnlocked = false;
    bool loadedAutoSaveOnReturn = false;
    std::vector<std::string> loadedStoryFlags;
    int warningCount = 0;
    std::array<RingShape, SpellRingCount> loadedRingShapes{};
    for (int i = 0; i < SpellRingCount; ++i) {
        loadedRingShapes[static_cast<std::size_t>(i)] = defaultRingShapeForIndex(i);
    }

    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string key;
        stream >> key;
        if (key.empty()) {
            continue;
        }
        if (key == "money") {
            stream >> loadedMoney;
        } else if (key == "player_level") {
            stream >> loadedPlayerLevel;
        } else if (key == "player_xp") {
            stream >> loadedPlayerXp;
        } else if (key == "player_xp_to_next") {
            stream >> loadedPlayerXpToNext;
        } else if (key == "upgrade_max_hp") {
            stream >> loadedMaxHpUpgradeLevel;
        } else if (key == "upgrade_ring_radius") {
            stream >> loadedRingRadiusUpgradeLevel;
        } else if (key == "upgrade_ring_speed") {
            stream >> loadedRingSpeedUpgradeLevel;
        } else if (key == "level_ring_radius_points") {
            stream >> loadedLevelRingRadiusPoints;
        } else if (key == "level_ring_speed_points") {
            stream >> loadedLevelRingSpeedPoints;
        } else if (key == "workshop_initial_radius_level") {
            stream >> loadedWorkshopInitialRadiusLevel;
        } else if (key == "workshop_initial_speed_level") {
            stream >> loadedWorkshopInitialSpeedLevel;
        } else if (key == "workshop_shift_distance_level") {
            stream >> loadedWorkshopShiftDistanceLevel;
        } else if (key == "merchant_refresh_pending") {
            stream >> loadedMerchantRefreshPending;
        } else if (key == "merchant_upgrade_level") {
            stream >> loadedMerchantUpgradeLevel;
        } else if (key == "merchant_stock_version") {
            stream >> loadedMerchantStockVersion;
        } else if (key == "merchant_stock") {
            std::string objectId;
            int price = 0;
            int quantity = 1;
            stream >> objectId >> price;
            if (!stream.fail()) {
                if (!(stream >> quantity)) {
                    quantity = 1;
                }
                if (objectCatalog_.registry.findById(objectId) == nullptr) {
                    ++warningCount;
                    logError("[warning] SaveData: merchant_stock object_id=\"" + objectId + "\" is missing from Objects DB; keeping ID");
                }
                loadedMerchantStock.push_back(MerchantProduct{objectId, std::max(1, price), std::max(0, quantity)});
            }
        } else if (key == "high_value_buy_category") {
            stream >> loadedHighValueBuyCategory;
            if (loadedHighValueBuyCategory == "-") {
                loadedHighValueBuyCategory.clear();
            }
        } else if (key == "warehouse_capacity_level") {
            stream >> loadedWarehouseCapacityLevel;
        } else if (key == "processing_unlock_level") {
            stream >> loadedProcessingUnlockLevel;
        } else if (key == "ring_workshop_unlocked") {
            stream >> loadedRingWorkshopUnlocked;
        } else if (key == "auto_save_on_return") {
            stream >> loadedAutoSaveOnReturn;
        } else if (key == "story_flag") {
            std::string flag;
            stream >> flag;
            if (!stream.fail() && !flag.empty()) {
                loadedStoryFlags.push_back(std::move(flag));
            }
        } else if (key == "codex_entry") {
            std::string kindName;
            std::string id;
            int stage = 0;
            stream >> kindName >> id >> stage;
            EncyclopediaKind kind = EncyclopediaKind::Item;
            if (!stream.fail() && encyclopediaKindFromSaveName(kindName, kind)) {
                loadedEncyclopedia.loadEntry(kind, std::move(id), encyclopediaStageFromInt(stage));
            }
        } else if (key == "codex_effect") {
            std::string objectId;
            std::string effectKey;
            stream >> objectId >> effectKey;
            if (!stream.fail()) {
                loadedEncyclopedia.loadEffect(std::move(objectId), std::move(effectKey));
            }
        } else if (key == "current_stage") {
            stream >> loadedCurrentStage;
        } else if (key == "current_stage_id") {
            stream >> loadedCurrentStageId;
        } else if (key == "unlocked_stages") {
            stream >> loadedUnlockedStages;
        } else if (key == "unlocked_warp_points") {
            stream >> loadedUnlockedWarpPointCount;
        } else if (key == "latest_warp") {
            stream >> loadedLatestWarpPointPosition.x >> loadedLatestWarpPointPosition.y;
            loadedHasLatestWarpPointPosition = !stream.fail();
        } else if (key == "object") {
            std::string objectId;
            int count = 0;
            stream >> objectId >> count;
            loadedInventory.setObjectItemCount(objectCatalog_, objectId, count);
            if (!stream.fail() && objectCatalog_.registry.findById(objectId) == nullptr) {
                ++warningCount;
            }
        } else if (key == "object_instance") {
            ItemInstance instance;
            stream >> instance.instanceId
                >> instance.objectId
                >> instance.currentDurability
                >> instance.maxDurability
                >> instance.enhanceLevel
                >> instance.attackBonus
                >> instance.digBonus
                >> instance.durabilityBonus
                >> instance.weightModifier
                >> instance.sizeModifier
                >> instance.protectionEnabled
                >> instance.isBroken;
            if (!stream.fail()) {
                if (objectCatalog_.registry.findById(instance.objectId) == nullptr) {
                    ++warningCount;
                }
                loadedInventory.addObjectInstance(objectCatalog_, std::move(instance));
            }
        } else if (key == "warehouse_object") {
            std::string objectId;
            int count = 0;
            stream >> objectId >> count;
            const ItemData* item = objectCatalog_.registry.findById(objectId);
            if (!stream.fail() && count > 0) {
                if (item != nullptr) {
                    loadedWarehouseStacks.push_back(InventoryObjectStack{*item, count});
                } else {
                    ++warningCount;
                    logError("[warning] SaveData: warehouse_object object_id=\"" + objectId + "\" is missing from Objects DB; restored as missing stack item");
                    loadedWarehouseStacks.push_back(InventoryObjectStack{makeMissingItemData(objectId), count});
                }
            }
        } else if (key == "warehouse_object_instance") {
            ItemInstance instance;
            stream >> instance.instanceId
                >> instance.objectId
                >> instance.currentDurability
                >> instance.maxDurability
                >> instance.enhanceLevel
                >> instance.attackBonus
                >> instance.digBonus
                >> instance.durabilityBonus
                >> instance.weightModifier
                >> instance.sizeModifier
                >> instance.protectionEnabled
                >> instance.isBroken;
            const ItemData* item = objectCatalog_.registry.findById(instance.objectId);
            if (!stream.fail()) {
                if (item == nullptr) {
                    ++warningCount;
                    logError("[warning] SaveData: warehouse_object_instance object_id=\"" + instance.objectId + "\" is missing from Objects DB; restored as missing ItemInstance");
                }
                loadedWarehouseInstances.push_back(InventoryObjectInstance{
                    .item = item != nullptr ? *item : makeMissingItemData(instance.objectId),
                    .instance = std::move(instance),
                });
            }
        } else if (key == "material") {
            std::string materialId;
            int count = 0;
            stream >> materialId >> count;
            MaterialType materialType = MaterialType::Count;
            if (!stream.fail() && materialTypeFromSaveName(materialId, materialType)) {
                loadedInventory.setMaterialCount(materialType, count);
            }
        } else if (key == "ring_shape_1" || key == "ring_shape_2" || key == "ring_shape_3") {
            std::string shapeValue;
            stream >> shapeValue;
            if (!stream.fail()) {
                int index = 0;
                if (key == "ring_shape_2") {
                    index = 1;
                } else if (key == "ring_shape_3") {
                    index = 2;
                }
                loadedRingShapes[static_cast<std::size_t>(index)] = parseRingShapeValue(shapeValue, defaultRingShapeForIndex(index));
            }
        } else if (key == "ring_shape") {
            int index = 0;
            std::string shapeValue;
            stream >> index >> shapeValue;
            if (!stream.fail() && index >= 1 && index <= SpellRingCount) {
                const int ringIndex = index - 1;
                loadedRingShapes[static_cast<std::size_t>(ringIndex)] = parseRingShapeValue(shapeValue, defaultRingShapeForIndex(ringIndex));
            }
        } else if (key == "ring") {
            // Legacy per-item record. Ring shape is stored separately in ring_shape_1..3.
            int type = 0;
            std::string objectId;
            std::string damageType;
            SpellRingItem item;
            stream >> type >> objectId >> item.damage >> damageType >> item.digPower
                >> item.durability >> item.maxDurability >> item.weight
                >> item.hitRadius >> item.hitInterval >> item.localAngle;
            if (!stream.fail()) {
                item.type = ringTypeFromInt(type);
                item.objectId = loadRingObjectId(objectId);
                item.damageType = damageType;
                const std::string normalizedDamageType = normalizeDamageType(item.damageType);
                if (normalizedDamageType.empty()) {
                    if (item.damageType == "physical") {
                        ++warningCount;
                        logError("[warning] SaveData: ring damageType physical is deprecated; using blunt");
                        item.damageType = "blunt";
                    } else {
                        ++warningCount;
                        logError("[warning] SaveData: ring damageType \"" + item.damageType + "\" is invalid; using none");
                        item.damageType = "none";
                    }
                } else {
                    if (item.damageType == "physical" && normalizedDamageType == "blunt") {
                        ++warningCount;
                        logError("[warning] SaveData: ring damageType physical is deprecated; using blunt");
                    }
                    item.damageType = normalizedDamageType;
                }
                item.objectStatsApplied = false;
                stream >> item.instanceId
                    >> item.enhanceLevel
                    >> item.attackBonus
                    >> item.digBonus
                    >> item.durabilityBonus
                    >> item.weightModifier
                    >> item.sizeModifier
                    >> item.protectionEnabled
                    >> item.isBroken;
                if (item.instanceId == "-") {
                    item.instanceId.clear();
                }
                int loadedRingIndex = 0;
                stream >> loadedRingIndex;
                if (stream.fail()) {
                    loadedRingIndex = 0;
                    stream.clear();
                }
                loadedRingIndex = std::clamp(loadedRingIndex, 0, SpellRingCount - 1);
                item.ringIndex = loadedRingIndex;
                loadedRingItemsByRing[static_cast<std::size_t>(loadedRingIndex)].push_back(item);
            }
        }
    }

    inventory_ = loadedInventory;
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    if (loadedRingShapes[0] == RingShape::Circle &&
        loadedRingShapes[1] == RingShape::Circle &&
        loadedRingShapes[2] == RingShape::Circle) {
        for (int i = 0; i < SpellRingCount; ++i) {
            loadedRingShapes[static_cast<std::size_t>(i)] = defaultRingShapeForIndex(i);
        }
    }
    for (int i = 0; i < SpellRingCount; ++i) {
        spellRing_.setRingShapeForIndex(i, loadedRingShapes[static_cast<std::size_t>(i)]);
    }
    spellRing_.ringItems() = std::move(loadedRingItemsByRing);
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.normalizeItemPlacements();
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    money_ = std::max(0, loadedMoney);
    unlockedStages_ = std::max(1, loadedUnlockedStages);
    unlockedWarpPointCount_ = std::max(0, loadedUnlockedWarpPointCount);
    hasLatestWarpPointPosition_ = loadedHasLatestWarpPointPosition;
    latestWarpPointPosition_ = loadedHasLatestWarpPointPosition
        ? loadedLatestWarpPointPosition
        : latestWarpPointStartPosition();
    currentStage_ = std::clamp(loadedCurrentStage, 0, unlockedStages_ - 1);
    if (!loadedCurrentStageId.empty()) {
        currentStageId_ = loadedCurrentStageId;
    }
    resolveCurrentStageDefinition();
    player_.level = std::max(1, loadedPlayerLevel);
    player_.xp = std::max(0, loadedPlayerXp);
    player_.xpToNext = std::max(1, loadedPlayerXpToNext);
    maxHpUpgradeLevel_ = std::max(0, loadedMaxHpUpgradeLevel);
    ringRadiusUpgradeLevel_ = std::max(0, loadedRingRadiusUpgradeLevel);
    ringSpeedUpgradeLevel_ = std::max(0, loadedRingSpeedUpgradeLevel);
    levelRingRadiusPoints_ = std::max(0, loadedLevelRingRadiusPoints);
    levelRingSpeedPoints_ = std::max(0, loadedLevelRingSpeedPoints);
    workshopInitialRadiusLevel_ = std::clamp(loadedWorkshopInitialRadiusLevel, 0, 5);
    workshopInitialSpeedLevel_ = std::clamp(loadedWorkshopInitialSpeedLevel, 0, 5);
    workshopShiftDistanceLevel_ = std::clamp(loadedWorkshopShiftDistanceLevel, 0, 5);
    merchantRefreshPending_ = loadedMerchantRefreshPending;
    merchantUpgradeLevel_ = std::clamp(loadedMerchantUpgradeLevel, 1, 7);
    merchantStockVersion_ = std::max(0, loadedMerchantStockVersion);
    merchantStock_ = std::move(loadedMerchantStock);
    highValueBuyCategory_ = std::move(loadedHighValueBuyCategory);
    warehouseCapacityLevel_ = std::clamp(loadedWarehouseCapacityLevel, 0, 4);
    processingUnlockLevel_ = std::clamp(loadedProcessingUnlockLevel, 0, 5);
    ringWorkshopUnlocked_ = loadedRingWorkshopUnlocked;
    autoSaveOnReturn_ = loadedAutoSaveOnReturn;
    storyFlags_ = std::move(loadedStoryFlags);
    encyclopedia_ = std::move(loadedEncyclopedia);
    warehouseObjectStacks_ = std::move(loadedWarehouseStacks);
    warehouseObjectInstances_ = std::move(loadedWarehouseInstances);
    applyPermanentUpgrades();
    resetRingWorkshopDraft();
    syncEncyclopediaFromInventoryAndRing();
    captureRunStartInventoryState();
    if (warningCount > 0) {
        logError("[warning] SaveData loaded with " + std::to_string(warningCount) + " warning(s)");
    }
    return true;
}

bool Game::saveSaveData(std::string& message) const
{
    const std::filesystem::path path = saveDataPath();
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            message = "保存先作成に失敗";
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        message = "セーブ失敗";
        return false;
    }

    file << "MAJO_SHOVEL_SAVE_V1\n";
    file << "money " << money_ << "\n";
    file << "player_level " << player_.level << "\n";
    file << "player_xp " << player_.xp << "\n";
    file << "player_xp_to_next " << player_.xpToNext << "\n";
    file << "upgrade_max_hp " << maxHpUpgradeLevel_ << "\n";
    file << "upgrade_ring_radius " << ringRadiusUpgradeLevel_ << "\n";
    file << "upgrade_ring_speed " << ringSpeedUpgradeLevel_ << "\n";
    file << "level_ring_radius_points " << levelRingRadiusPoints_ << "\n";
    file << "level_ring_speed_points " << levelRingSpeedPoints_ << "\n";
    file << "workshop_initial_radius_level " << workshopInitialRadiusLevel_ << "\n";
    file << "workshop_initial_speed_level " << workshopInitialSpeedLevel_ << "\n";
    file << "workshop_shift_distance_level " << workshopShiftDistanceLevel_ << "\n";
    file << "merchant_refresh_pending " << merchantRefreshPending_ << "\n";
    file << "merchant_upgrade_level " << merchantUpgradeLevel_ << "\n";
    file << "merchant_stock_version " << merchantStockVersion_ << "\n";
    file << "high_value_buy_category " << (highValueBuyCategory_.empty() ? "-" : highValueBuyCategory_) << "\n";
    for (const MerchantProduct& product : merchantStock_) {
        if (!product.objectId.empty()) {
            file << "merchant_stock " << product.objectId << " " << product.price << " " << product.quantity << "\n";
        }
    }
    file << "warehouse_capacity_level " << warehouseCapacityLevel_ << "\n";
    file << "processing_unlock_level " << processingUnlockLevel_ << "\n";
    file << "ring_workshop_unlocked " << ringWorkshopUnlocked_ << "\n";
    file << "auto_save_on_return " << autoSaveOnReturn_ << "\n";
    for (int i = 0; i < SpellRingCount; ++i) {
        file << "ring_shape_" << (i + 1) << " " << saveRingShapeName(spellRing_.ringShapeForIndex(i)) << "\n";
    }
    for (const std::string& flag : storyFlags_) {
        if (!flag.empty()) {
            file << "story_flag " << flag << "\n";
        }
    }
    for (const EncyclopediaEntrySave& entry : encyclopedia_.saveEntries()) {
        if (!entry.id.empty()) {
            file << "codex_entry "
                << encyclopediaKindSaveName(entry.kind) << " "
                << entry.id << " "
                << static_cast<int>(entry.stage) << "\n";
        }
    }
    for (const EncyclopediaEffectSave& effect : encyclopedia_.saveEffects()) {
        if (!effect.objectId.empty() && !effect.effectKey.empty()) {
            file << "codex_effect " << effect.objectId << " " << effect.effectKey << "\n";
        }
    }
    file << "current_stage " << currentStage_ << "\n";
    file << "current_stage_id " << currentStageId_ << "\n";
    file << "unlocked_stages " << unlockedStages_ << "\n";
    file << "unlocked_warp_points " << unlockedWarpPointCount_ << "\n";
    if (hasLatestWarpPointPosition_) {
        file << "latest_warp " << latestWarpPointPosition_.x << " " << latestWarpPointPosition_.y << "\n";
    }
    for (const StackItem& stack : inventory_.stackItemsForSave()) {
        if (!stack.objectId.empty() && stack.count > 0) {
            file << "object " << stack.objectId << " " << stack.count << "\n";
        }
    }
    for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
        const ItemInstance& instance = objectInstance.instance;
        if (!instance.instanceId.empty() && !instance.objectId.empty()) {
            file << "object_instance "
                << instance.instanceId << " "
                << instance.objectId << " "
                << instance.currentDurability << " "
                << instance.maxDurability << " "
                << instance.enhanceLevel << " "
                << instance.attackBonus << " "
                << instance.digBonus << " "
                << instance.durabilityBonus << " "
                << instance.weightModifier << " "
                << instance.sizeModifier << " "
                << instance.protectionEnabled << " "
                << instance.isBroken << "\n";
        }
    }
    for (const InventoryObjectStack& stack : warehouseObjectStacks_) {
        if (!stack.objectId.empty() && stack.count > 0) {
            file << "warehouse_object " << stack.objectId << " " << stack.count << "\n";
        }
    }
    for (const InventoryObjectInstance& objectInstance : warehouseObjectInstances_) {
        const ItemInstance& instance = objectInstance.instance;
        if (!instance.instanceId.empty() && !instance.objectId.empty()) {
            file << "warehouse_object_instance "
                << instance.instanceId << " "
                << instance.objectId << " "
                << instance.currentDurability << " "
                << instance.maxDurability << " "
                << instance.enhanceLevel << " "
                << instance.attackBonus << " "
                << instance.digBonus << " "
                << instance.durabilityBonus << " "
                << instance.weightModifier << " "
                << instance.sizeModifier << " "
                << instance.protectionEnabled << " "
                << instance.isBroken << "\n";
        }
    }
    for (int index = 0; index < static_cast<int>(MaterialType::Count); ++index) {
        const MaterialType type = static_cast<MaterialType>(index);
        file << "material " << materialTypeSaveName(type) << " " << inventory_.materialCount(type) << "\n";
    }
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        for (const SpellRingItem& item : spellRing_.itemsForRing(ringIndex)) {
            file << "ring "
                << ringTypeToInt(item.type) << " "
                << saveRingObjectId(item) << " "
                << item.damage << " "
                << item.damageType << " "
                << item.digPower << " "
                << item.durability << " "
                << item.maxDurability << " "
                << item.weight << " "
                << item.hitRadius << " "
                << item.hitInterval << " "
                << item.localAngle << " "
                << (item.instanceId.empty() ? "-" : item.instanceId) << " "
                << item.enhanceLevel << " "
                << item.attackBonus << " "
                << item.digBonus << " "
                << item.durabilityBonus << " "
                << item.weightModifier << " "
                << item.sizeModifier << " "
                << item.protectionEnabled << " "
                << item.isBroken << " "
                << ringIndex << "\n";
        }
    }

    if (!file) {
        message = "セーブ書込に失敗";
        return false;
    }

    message = "セーブしました";
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

    if (normalized == "game reset-data") {
        money_ = 0;
        maxHpUpgradeLevel_ = 0;
        ringRadiusUpgradeLevel_ = 0;
        ringSpeedUpgradeLevel_ = 0;
        levelRingRadiusPoints_ = 0;
        levelRingSpeedPoints_ = 0;
        workshopInitialRadiusLevel_ = 0;
        workshopInitialSpeedLevel_ = 0;
        workshopShiftDistanceLevel_ = 0;
        merchantRefreshPending_ = false;
        merchantUpgradeLevel_ = 1;
        merchantStockVersion_ = 0;
        merchantStock_.clear();
        highValueBuyCategory_.clear();
        warehouseCapacityLevel_ = 0;
        processingUnlockLevel_ = 0;
        ringWorkshopUnlocked_ = false;
        autoSaveOnReturn_ = false;
        storyFlags_.clear();
        warehouseObjectStacks_.clear();
        warehouseObjectInstances_.clear();
        unlockedStages_ = 1;
        unlockedWarpPointCount_ = 0;
        hasLatestWarpPointPosition_ = false;
        currentStage_ = 0;
        currentStageId_ = "stage_01_stardust";
        resolveCurrentStageDefinition();
        encyclopedia_ = EncyclopediaSystem{};
        initializeWorld(false);
        enterBase();
        captureRunStartInventoryState();

        std::string saveMessage;
        if (saveSaveData(saveMessage)) {
            logInfo("Debug: game data reset and saved.");
        } else {
            logWarning("Debug: game data reset in memory, but save failed: " + saveMessage);
        }
        return true;
    }

    if (normalized == "game return-base") {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
            logInfo("Debug: enemy test exited to base.");
            return true;
        }
        returnToBaseFromNormalStage(false, false);
        logInfo("Debug: returned to base.");
        return true;
    }

    if (normalized == "game launch-mode pre-title" ||
        normalized == "game launch-mode before-title" ||
        normalized == "game launch-mode opening") {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (!basePresentationActive() && mode_ != ScreenMode::OpeningKamishibai && mode_ != ScreenMode::Title) {
            returnToBaseFromNormalStage(false, false);
        } else {
            enterBase();
        }
        startOpeningKamishibai();
        logInfo("Debug: launch mode set to pre-title opening.");
        return true;
    }

    if (normalized == "game launch-mode base") {
        if (enemyTestActive_) {
            exitEnemyTestToBase();
        } else if (basePresentationActive() || mode_ == ScreenMode::OpeningKamishibai || mode_ == ScreenMode::Title) {
            enterBase();
        } else {
            returnToBaseFromNormalStage(false, false);
        }
        logInfo("Debug: launch mode set to base.");
        return true;
    }

    if (normalized == "game launch-mode dungeon") {
        if (enemyTestActive_) {
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

    if (normalized == "game enemy-test") {
        enterEnemyTestMode();
        logInfo("Debug: enemy test mode started.");
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

    if (normalized == "game money add 10000") {
        money_ = std::max(0, money_ + 10000);
        logInfo("Debug: money +10000 => " + std::to_string(money_) + "G");
        return true;
    }

    if (normalized == "game materials add 100") {
        for (int index = 0; index < static_cast<int>(MaterialType::Count); ++index) {
            inventory_.addMaterial(static_cast<MaterialType>(index), 100);
        }
        logInfo("Debug: all upgrade materials +100.");
        return true;
    }

    if (normalized == "game items random8") {
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
            return true;
        }

        std::mt19937& rng = lootRuntimeRng();
        std::uniform_int_distribution<std::size_t> pick(0, candidateIds.size() - 1);
        int acquiredCount = 0;
        int skippedCount = 0;
        for (int i = 0; i < 8; ++i) {
            const std::string_view objectId = candidateIds[pick(rng)];
            if (inventory_.addObjectItem(objectCatalog_, objectId)) {
                ++acquiredCount;
            } else {
                ++skippedCount;
            }
        }

        logInfo("Debug: random object items added " + std::to_string(acquiredCount) +
            " / skipped " + std::to_string(skippedCount) + ".");
        return true;
    }

    if (normalized == "game hp full") {
        applyPermanentUpgrades();
        player_.hp = player_.maxHp;
        player_.status = EntityStatus{};
        player_.poisonDamageAccumulator = 0.0;
        logInfo("Debug: player HP restored to " + std::to_string(player_.maxHp) + ".");
        return true;
    }

    if (normalized == "game hp set 1") {
        applyPermanentUpgrades();
        player_.hp = std::min(1, std::max(1, player_.maxHp));
        player_.status = EntityStatus{};
        player_.poisonDamageAccumulator = 0.0;
        logInfo("Debug: player HP set to 1.");
        return true;
    }

    return false;
}

void Game::enterGameOver()
{
    if (mode_ == ScreenMode::GameOver || mode_ == ScreenMode::StageClear) {
        return;
    }

    player_.hp = 0;
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    mode_ = ScreenMode::GameOver;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    gameOverSelection_ = 0;
    gameOverStatus_.clear();
}

void Game::enterStageClear()
{
    if (mode_ == ScreenMode::StageClear) {
        return;
    }

    unlockedStages_ = std::max(unlockedStages_, currentStage_ + 2);
    addStoryFlag("stage_clear_" + std::to_string(currentStage_ + 1));
    clearTemporaryPlayerState(true);
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    mode_ = ScreenMode::StageClear;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    stageClearSelection_ = 0;
    stageClearStatus_.clear();
}

void Game::updateRingScreen(const Input& input, UiContext& ui, float dt)
{
    auto& items = spellRing_.items();
    if (ringSnapActive_) {
        if (ringDragItemIndex_ < 0 || ringDragItemIndex_ >= static_cast<int>(items.size())) {
            ringSnapActive_ = false;
            ringDragItemIndex_ = -1;
        } else {
            ringSnapElapsed_ = std::min(RingSnapDuration, ringSnapElapsed_ + dt);
            const float t = RingSnapDuration <= 0.0f ? 1.0f : clamp(ringSnapElapsed_ / RingSnapDuration, 0.0f, 1.0f);
            const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            ringDragDisplayAngle_ = spellRing_.normalizeLocalAngle(
                ringSnapStartAngle_ + shortestRingAngleDelta(ringSnapStartAngle_, ringSnapTargetAngle_, spellRing_.runtimeRingShape(), balance_) * eased,
                balance_);
            if (t >= 1.0f) {
                items[static_cast<std::size_t>(ringDragItemIndex_)].localAngle = ringSnapTargetAngle_;
                ringSnapActive_ = false;
                ringDragItemIndex_ = -1;
            }
        }
        ui.block(ringPanelRect());
        return;
    }

    if (spellRing_.activeRingIndex() >= UnlockedRingCount) {
        spellRing_.switchActiveRing(-spellRing_.activeRingIndex());
        ringStatus_.clear();
    }

    for (int i = 0; i < UnlockedRingCount; ++i) {
        const UiRect rect = ringTabRect(i);
        if (rect.contains(ui.mouse()) && i != spellRing_.activeRingIndex()) {
            spellRing_.switchActiveRing(i - spellRing_.activeRingIndex());
            ringStatus_.clear();
        }
        if (ui.pressed(rect)) {
            spellRing_.switchActiveRing(i - spellRing_.activeRingIndex());
            ringStatus_.clear();
            return;
        }
    }

    if (!items.empty()) {
        ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()) - 1);
    } else {
        ringSlotSelection_ = 0;
    }
    const bool actualRing = true;

    if ((ringDragPending_ || ringDragActive_) && uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
        ringDragPending_ = false;
        ringDragActive_ = false;
        ringDragItemIndex_ = -1;
        ringStatus_ = "ドラッグをキャンセルしました";
        ui.block(ringPanelRect());
        return;
    }

    if (ringDragPending_ || ringDragActive_) {
        if (ringDragItemIndex_ < 0 || ringDragItemIndex_ >= static_cast<int>(items.size())) {
            ringDragPending_ = false;
            ringDragActive_ = false;
            ringDragItemIndex_ = -1;
            ui.block(ringPanelRect());
            return;
        }

        if (input.mouseLeftHeld()) {
            if (!ringDragActive_ && distanceSquared(input.mouseScreen(), ringDragStartMouse_) >= 36.0f) {
                ringDragActive_ = true;
                ringDragPending_ = false;
            }
            if (ringDragActive_) {
                ringDragDisplayAngle_ = ringAngleFromPoint(input.mouseScreen(), spellRing_, balance_);
            }
        }

        if (input.mouseLeftReleased()) {
            if (ringDragActive_) {
                const float desired = ringAngleFromPoint(input.mouseScreen(), spellRing_, balance_);
                const std::optional<float> snapAngle = spellRing_.nearestPlaceableAngle(ringDragItemIndex_, desired, RingDragSnapMaxDelta);
                ringSnapStartAngle_ = desired;
                ringSnapTargetAngle_ = snapAngle.value_or(ringDragOriginalAngle_);
                ringDragDisplayAngle_ = ringSnapStartAngle_;
                ringSnapElapsed_ = 0.0f;
                ringSnapActive_ = true;
                ringStatus_ = snapAngle ? "近い空き位置へ補正しました" : "近くに空きがないため元の位置へ戻しました";
            }
            ringDragPending_ = false;
            ringDragActive_ = false;
            ui.block(ringPanelRect());
            return;
        }

        ui.block(ringPanelRect());
        return;
    }

    if (uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
        if (ringGrabActive_) {
            cancelRingGrab();
            ringStatus_ = "つかみ操作をキャンセルしました";
        } else {
            mode_ = ScreenMode::PauseMenu;
            pausePage_ = PauseMenuPage::Main;
        }
        return;
    }

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const UiRect rect = ringItemUiRect(items[static_cast<std::size_t>(i)], spellRing_, balance_, i, static_cast<int>(items.size()));
        if (rect.contains(ui.mouse())) {
            ringSlotSelection_ = i;
        }
        if (ui.pressed(rect)) {
            ringSlotSelection_ = i;
            ringDragPending_ = true;
            ringDragActive_ = false;
            ringDragItemIndex_ = i;
            ringDragOriginalAngle_ = items[static_cast<std::size_t>(i)].localAngle;
            ringDragDisplayAngle_ = ringDragOriginalAngle_;
            ringDragStartMouse_ = input.mouseScreen();
            return;
        }
    }
    ui.block(ringPanelRect());

    const int directRing = input.shortcutSlotPressed();
    if (directRing >= 0 && directRing < UnlockedRingCount) {
        spellRing_.switchActiveRing(directRing - spellRing_.activeRingIndex());
        ringStatus_.clear();
    }
    if (input.activeRingDelta() != 0) {
        if constexpr (UnlockedRingCount > 1) {
            spellRing_.switchActiveRing(input.activeRingDelta());
            ringStatus_.clear();
        }
    }

    if (!items.empty() && input.shortcutCursorDelta() != 0) {
        const int count = static_cast<int>(items.size());
        ringSlotSelection_ = (ringSlotSelection_ + input.shortcutCursorDelta() + count) % count;
    }
    if (input.pressed(InputAction::MoveLeft)) {
        if (!spellRing_.moveItemAngle(ringSlotSelection_, -RingAngleStep)) {
            ringStatus_ = "その位置には移動できません";
        }
    }
    if (input.pressed(InputAction::MoveRight)) {
        if (!spellRing_.moveItemAngle(ringSlotSelection_, RingAngleStep)) {
            ringStatus_ = "その位置には移動できません";
        }
    }

    (void)actualRing;

    if (input.addRingPressed()) {
        if (ringGrabActive_) {
            ringStatus_ = "つかみ中は外せません";
        } else if (ringSlotSelection_ < static_cast<int>(items.size()) && items.size() > 1) {
            items.erase(items.begin() + ringSlotSelection_);
            ringSlotSelection_ = std::min(ringSlotSelection_, std::max(0, static_cast<int>(items.size()) - 1));
            ringStatus_ = "選択中アイテムを外しました";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ringStatus_ = "最後の1個は外せません";
        } else {
            ringStatus_ = "アイテム未選択です";
        }
        return;
    }

    if (input.pressed(InputAction::ToggleProtection)) {
        if (ringGrabActive_) {
            ringStatus_ = "つかみ中は保護変更できません";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            SpellRingItem& item = items[ringSlotSelection_];
            if (item.instanceId.empty()) {
                ringStatus_ = "個体アイテムのみ保護できます";
            } else {
                item.protectionEnabled = !item.protectionEnabled;
                ringStatus_ = item.protectionEnabled ? "保護ON" : "保護OFF";
            }
        } else {
            ringStatus_ = "アイテム未選択です";
        }
        return;
    }

    if (input.grabOrPlacePressed()) {
        if (ringGrabActive_) {
            if (spellRing_.addItem(ringGrabbedItem_)) {
                ringSlotSelection_ = std::max(0, static_cast<int>(items.size()) - 1);
                ringGrabActive_ = false;
                ringGrabOrigin_ = -1;
                ringStatus_ = "最も広い空きに配置しました";
            } else {
                ringStatus_ = "配置できる空きがありません";
            }
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            if (items.size() <= 1) {
                ringStatus_ = "最後の1個は外せません";
            } else {
                ringGrabbedItem_ = items[ringSlotSelection_];
                ringGrabOrigin_ = ringSlotSelection_;
                items.erase(items.begin() + ringSlotSelection_);
                ringGrabActive_ = true;
                ringStatus_ = "装着アイテムをつかみました";
            }
        } else {
            ringStatus_ = "アイテム未選択です";
        }
        return;
    }

    if (input.useItemPressed() || input.confirmPressed()) {
        if (ringGrabActive_) {
            if (spellRing_.addItem(ringGrabbedItem_)) {
                ringSlotSelection_ = std::max(0, static_cast<int>(items.size()) - 1);
                ringGrabActive_ = false;
                ringGrabOrigin_ = -1;
                ringStatus_ = "最も広い空きに配置しました";
            } else {
                ringStatus_ = "配置できる空きがありません";
            }
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ringStatus_ = "詳細を表示中";
        } else {
            ringStatus_ = "アイテム未選択です";
        }
    }
}

void Game::updateBaseScreen(const Input& input, UiContext& ui, float dt)
{
    updatePlayerFootstepDust(dt);

    if (baseEditEnabled_) {
        updateBaseEditScreen(input, ui, dt);
        return;
    }

    if (baseBookshelfActive_) {
        updateBookshelfScreen(input, ui);
        return;
    }

    if (baseRingWorkshopActive_) {
        if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
            baseRingWorkshopActive_ = false;
            resetRingWorkshopDraft();
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + BaseRingWorkshopItemCount - 1) % BaseRingWorkshopItemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + 1) % BaseRingWorkshopItemCount;
        }
        if (input.pressed(InputAction::MoveLeft)) {
            adjustRingWorkshopRespec(-1);
        }
        if (input.pressed(InputAction::MoveRight)) {
            adjustRingWorkshopRespec(1);
        }
        const auto chooseWorkshopItem = [this](int item) {
            if (item == 0) {
                adjustRingWorkshopRespec(1);
                return;
            }
            if (item == 1) {
                adjustRingWorkshopRespec(-1);
                return;
            }
            if (item == 2) {
                confirmRingWorkshopRespec();
                return;
            }
            if (item >= 3 && item < 3 + RingWorkshopImplementedUpgradeCount) {
                buyRingWorkshopUpgrade(static_cast<RingWorkshopUpgrade>(item - 3));
                return;
            }
            baseStatus_ = "この項目は未解禁です";
        };
        for (int i = 0; i < BaseRingWorkshopItemCount; ++i) {
            const UiRect rect = baseRingWorkshopItemRect(i);
            if (rect.contains(ui.mouse())) {
                baseRingWorkshopSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseRingWorkshopSelection_ = i;
                chooseWorkshopItem(i);
                return;
            }
        }
        if (ui.pressed(ringWorkshopConfirmRect())) {
            confirmRingWorkshopRespec();
            return;
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            chooseWorkshopItem(baseRingWorkshopSelection_);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (baseStorageActive_) {
        const std::vector<StorageEntry> warehouseEntries = warehouseStorageEntries();
        const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
        baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
        baseStorageBackpackCursor_ = std::clamp(baseStorageBackpackCursor_, 0, StoragePaneSlotCount - 1);
        baseStorageWarehouseCursor_ = std::clamp(baseStorageWarehouseCursor_, 0, StoragePaneSlotCount - 1);

        const auto hasBackpackItemAt = [this](int localSlot) {
            return inventory_.hasScreenItemAt(localSlot);
        };
        const auto warehouseStorageSlotFromLocal = [this](int localSlot) {
            return baseStorageWarehousePage_ * StoragePaneSlotCount + localSlot;
        };
        const auto hasWarehouseItemAt = [this, &warehouseStorageSlotFromLocal](int localSlot) {
            return warehouseEntryIndexAtStorageSlot(warehouseStorageSlotFromLocal(localSlot)) >= 0;
        };
        const auto depositBackpackSlot = [this, &warehouseStorageSlotFromLocal](int localSlot, int targetWarehouseLocalSlot = -1) {
            const int targetWarehouseSlot = targetWarehouseLocalSlot >= 0 ? warehouseStorageSlotFromLocal(targetWarehouseLocalSlot) : -1;
            const InventoryObjectStack* stack = inventory_.screenObjectStackAt(localSlot);
            if (stack != nullptr) {
                const std::string objectId = stack->objectId;
                auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [stack](const InventoryObjectStack& candidate) {
                    return candidate.objectId == stack->objectId;
                });
                int warehouseEntryIndex = -1;
                if (it == warehouseObjectStacks_.end()) {
                    if (warehouseUsedSlots() >= warehouseCapacity()) {
                        baseStatus_ = "倉庫がいっぱいです";
                        return;
                    }
                    syncWarehouseDisplaySlots();
                    const int newStackIndex = static_cast<int>(warehouseObjectStacks_.size());
                    warehouseDisplaySlots_.insert(warehouseDisplaySlots_.begin() + newStackIndex, -1);
                    warehouseObjectStacks_.push_back(InventoryObjectStack{stack->item, 0});
                    it = warehouseObjectStacks_.end() - 1;
                    warehouseEntryIndex = newStackIndex;
                } else {
                    warehouseEntryIndex = static_cast<int>(std::distance(warehouseObjectStacks_.begin(), it));
                }
                if (!inventory_.removeObjectItemCount(objectId, 1)) {
                    baseStatus_ = "預け入れに失敗しました";
                    return;
                }
                ++it->count;
                if (targetWarehouseSlot >= 0) {
                    assignWarehouseEntryToStorageSlot(warehouseEntryIndex, targetWarehouseSlot);
                }
                baseStatus_.clear();
                return;
            }
            const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(localSlot);
            if (instance == nullptr) {
                baseStatus_ = "預けるアイテムがありません";
                return;
            }
            if (warehouseUsedSlots() >= warehouseCapacity()) {
                baseStatus_ = "倉庫がいっぱいです";
                return;
            }
            InventoryObjectInstance moved;
            if (!inventory_.takeObjectInstance(instance->instance.instanceId, moved)) {
                baseStatus_ = "預け入れに失敗しました";
                return;
            }
            warehouseObjectInstances_.push_back(std::move(moved));
            if (targetWarehouseSlot >= 0) {
                assignWarehouseEntryToStorageSlot(warehouseUsedSlots() - 1, targetWarehouseSlot);
            }
            baseStatus_.clear();
        };
        const auto withdrawWarehouseSlot = [&warehouseEntries, this, &warehouseStorageSlotFromLocal](int localSlot, int targetBackpackSlot = -1) {
            const int entryIndex = warehouseEntryIndexAtStorageSlot(warehouseStorageSlotFromLocal(localSlot));
            if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseEntries.size())) {
                baseStatus_ = "取り出すアイテムがありません";
                return;
            }
            const StorageEntry entry = warehouseEntries[static_cast<std::size_t>(entryIndex)];
            if (entry.kind == StorageEntryKind::Stack) {
                InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(entry.index)];
                const std::string objectId = stack.objectId;
                if (!inventory_.addObjectItem(objectCatalog_, objectId)) {
                    baseStatus_ = "リュックがいっぱいです";
                    return;
                }
                if (targetBackpackSlot >= 0) {
                    (void)inventory_.moveObjectStackToScreenSlot(objectId, targetBackpackSlot);
                }
                --stack.count;
                if (stack.count <= 0) {
                    removeWarehouseDisplaySlotAtEntryIndex(entry.index);
                    warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + entry.index);
                }
                baseStatus_.clear();
                return;
            }

            InventoryObjectInstance moved = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)];
            const std::string instanceId = moved.instance.instanceId;
            if (!inventory_.addObjectInstance(objectCatalog_, moved.instance)) {
                baseStatus_ = "リュックがいっぱいです";
                return;
            }
            if (targetBackpackSlot >= 0) {
                (void)inventory_.moveObjectInstanceToScreenSlot(instanceId, targetBackpackSlot);
            }
            removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + entry.index);
            warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + entry.index);
            baseStatus_.clear();
        };
        const auto openStorageCommandMenuAt = [this, &hasBackpackItemAt, &hasWarehouseItemAt](int globalSlot) {
            const bool warehouse = storageGlobalSlotIsWarehouse(globalSlot);
            const int localSlot = storageLocalSlot(globalSlot);
            if (warehouse ? !hasWarehouseItemAt(localSlot) : !hasBackpackItemAt(localSlot)) {
                closeUiCommandMenu(baseStorageCommandMenu_);
                baseStorageCommandSlot_ = -1;
                return;
            }
            const char* label = warehouse ? "取り出す" : "倉庫へしまう";
            const std::array<UiCommandMenuItem, 1> items{{{label, true}}};
            baseStorageCommandSlot_ = globalSlot;
            const UiRect slotRect = storageSlotRectByGlobal(globalSlot);
            openUiCommandMenu(
                baseStorageCommandMenu_,
                slotRect.pos + Vec2{slotRect.size.x - 20.0f, 0.0f},
                storagePanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                160.0f,
                2);
        };
        const auto tryTransferBySlots = [this, &depositBackpackSlot, &withdrawWarehouseSlot](int fromGlobalSlot, int toGlobalSlot) {
            if (fromGlobalSlot < 0 || toGlobalSlot < 0) {
                return false;
            }
            const bool fromWarehouse = storageGlobalSlotIsWarehouse(fromGlobalSlot);
            const bool toWarehouse = storageGlobalSlotIsWarehouse(toGlobalSlot);
            if (fromWarehouse == toWarehouse) {
                return false;
            }
            if (fromWarehouse) {
                withdrawWarehouseSlot(storageLocalSlot(fromGlobalSlot), storageLocalSlot(toGlobalSlot));
            } else {
                depositBackpackSlot(storageLocalSlot(fromGlobalSlot), storageLocalSlot(toGlobalSlot));
            }
            return true;
        };
        const auto selectedGlobalSlot = [this]() {
            return storageGlobalSlotFromLocal(
                baseStorageFocusWarehouse_,
                baseStorageFocusWarehouse_ ? baseStorageWarehouseCursor_ : baseStorageBackpackCursor_);
        };
        const auto setSelectedGlobalSlot = [this](int globalSlot) {
            const int clamped = std::clamp(globalSlot, 0, StoragePaneSlotCount * 2 - 1);
            if (storageGlobalSlotIsWarehouse(clamped)) {
                baseStorageFocusWarehouse_ = true;
                baseStorageWarehouseCursor_ = storageLocalSlot(clamped);
            } else {
                baseStorageFocusWarehouse_ = false;
                baseStorageBackpackCursor_ = storageLocalSlot(clamped);
            }
        };

        const int commandSlotIndex = baseStorageCommandSlot_ >= 0 ? baseStorageCommandSlot_ : selectedGlobalSlot();
        const bool commandWarehouse = storageGlobalSlotIsWarehouse(commandSlotIndex);
        const char* commandLabel = commandWarehouse ? "取り出す" : "倉庫へしまう";
        const bool commandEnabled = commandWarehouse
            ? hasWarehouseItemAt(storageLocalSlot(commandSlotIndex))
            : hasBackpackItemAt(storageLocalSlot(commandSlotIndex));
        const std::array<UiCommandMenuItem, 1> commandItems{{{commandLabel, commandEnabled}}};
        const bool commandOpenBeforeUpdate = baseStorageCommandMenu_.open;
        const int commandSelection = updateUiCommandMenu(
            baseStorageCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0 && baseStorageCommandSlot_ >= 0) {
            if (storageGlobalSlotIsWarehouse(baseStorageCommandSlot_)) {
                withdrawWarehouseSlot(storageLocalSlot(baseStorageCommandSlot_));
            } else {
                depositBackpackSlot(storageLocalSlot(baseStorageCommandSlot_));
            }
            baseStorageCommandSlot_ = -1;
            baseStoragePointerPressSlot_ = -1;
            baseStoragePointerPressCanOpenMenu_ = false;
            baseStoragePointerDragTriggered_ = false;
            ui.block(storagePanelRect());
            return;
        }
        if (!baseStorageCommandMenu_.open) {
            if (commandOpenBeforeUpdate && input.backPressed()) {
                baseStorageCommandSlot_ = -1;
                baseStoragePointerPressSlot_ = -1;
                baseStoragePointerPressCanOpenMenu_ = false;
                baseStoragePointerDragTriggered_ = false;
                ui.block(storagePanelRect());
                return;
            }
            baseStorageCommandSlot_ = -1;
        }

        if (uiCancelRequested(baseCancelState_, input, ui, storagePanelRect())) {
            if (baseStorageCommandMenu_.open) {
                closeUiCommandMenu(baseStorageCommandMenu_);
                baseStorageCommandSlot_ = -1;
                baseStoragePointerPressSlot_ = -1;
                baseStoragePointerPressCanOpenMenu_ = false;
                baseStoragePointerDragTriggered_ = false;
                ui.block(storagePanelRect());
                return;
            }
            baseStorageActive_ = false;
            baseStatus_.clear();
            baseStorageGrabbedActive_ = false;
            baseStorageGrabbedFromSlot_ = -1;
            ui.block(storagePanelRect());
            return;
        }

        if (baseStorageCommandMenu_.open) {
            ui.block(storagePanelRect());
            return;
        }

        if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveRight) ||
            input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveDown)) {
            const int current = selectedGlobalSlot();
            int row = current / StorageColumns;
            int column = current % StorageColumns;
            if (input.pressed(InputAction::MoveLeft)) {
                column = (column + StorageColumns - 1) % StorageColumns;
            }
            if (input.pressed(InputAction::MoveRight)) {
                column = (column + 1) % StorageColumns;
            }
            const int totalRows = StorageRowsPerPane * 2;
            if (input.pressed(InputAction::MoveUp)) {
                row = (row + totalRows - 1) % totalRows;
            }
            if (input.pressed(InputAction::MoveDown)) {
                row = (row + 1) % totalRows;
            }
            setSelectedGlobalSlot(row * StorageColumns + column);
        }
        if (input.shortcutCursorDelta() != 0) {
            const int current = selectedGlobalSlot();
            const int next = (current + input.shortcutCursorDelta() + StoragePaneSlotCount * 2) % (StoragePaneSlotCount * 2);
            setSelectedGlobalSlot(next);
        }
        const int directSlot = input.shortcutSlotPressed();
        if (directSlot >= 0 && directSlot < StorageColumns) {
            const int current = selectedGlobalSlot();
            const int row = current / StorageColumns;
            setSelectedGlobalSlot(row * StorageColumns + directSlot);
        }

        if (input.activeRingDelta() != 0) {
            baseStorageWarehousePage_ = wrapStoragePageIndex(
                baseStorageWarehousePage_,
                input.activeRingDelta(),
                warehousePageCount);
        }

        const UiRect prevPageRect = storagePrevPageButtonRect();
        const UiRect nextPageRect = storageNextPageButtonRect();
        if (ui.pressed(prevPageRect)) {
            baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, -1, warehousePageCount);
        }
        if (ui.pressed(nextPageRect)) {
            baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, 1, warehousePageCount);
        }

        int hoveredSlot = -1;
        for (int i = 0; i < StoragePaneSlotCount * 2; ++i) {
            if (storageSlotRectByGlobal(i).contains(ui.mouse())) {
                hoveredSlot = i;
                setSelectedGlobalSlot(i);
                break;
            }
        }

        if (input.mouseLeftPressed() && hoveredSlot >= 0 && !ui.pointerConsumed()) {
            baseStoragePointerPressSlot_ = hoveredSlot;
            baseStoragePointerPressMouse_ = input.mouseScreen();
            baseStoragePointerPressCanOpenMenu_ = storageGlobalSlotIsWarehouse(hoveredSlot)
                ? hasWarehouseItemAt(storageLocalSlot(hoveredSlot))
                : hasBackpackItemAt(storageLocalSlot(hoveredSlot));
            baseStoragePointerDragTriggered_ = false;
            ui.consumePointer();
        }

        if (baseStoragePointerPressSlot_ >= 0 &&
            input.mouseLeftHeld() &&
            !baseStoragePointerDragTriggered_ &&
            !baseStorageGrabbedActive_ &&
            lengthSquared(input.mouseScreen() - baseStoragePointerPressMouse_) >= StorageDragStartDistanceSq) {
            const bool sourceWarehouse = storageGlobalSlotIsWarehouse(baseStoragePointerPressSlot_);
            const bool hasSourceItem = sourceWarehouse
                ? hasWarehouseItemAt(storageLocalSlot(baseStoragePointerPressSlot_))
                : hasBackpackItemAt(storageLocalSlot(baseStoragePointerPressSlot_));
            if (hasSourceItem) {
                baseStorageGrabbedActive_ = true;
                baseStorageGrabbedFromSlot_ = baseStoragePointerPressSlot_;
                baseStoragePointerDragTriggered_ = true;
                baseStoragePointerPressCanOpenMenu_ = false;
                closeUiCommandMenu(baseStorageCommandMenu_);
                baseStorageCommandSlot_ = -1;
            }
        }

        if (baseStoragePointerPressSlot_ >= 0 && input.mouseLeftReleased()) {
            if (baseStoragePointerDragTriggered_ && baseStorageGrabbedActive_) {
                if (hoveredSlot >= 0) {
                    setSelectedGlobalSlot(hoveredSlot);
                    (void)tryTransferBySlots(baseStorageGrabbedFromSlot_, hoveredSlot);
                }
                baseStorageGrabbedActive_ = false;
                baseStorageGrabbedFromSlot_ = -1;
            } else if (baseStoragePointerPressCanOpenMenu_ && hoveredSlot == baseStoragePointerPressSlot_) {
                openStorageCommandMenuAt(baseStoragePointerPressSlot_);
            }
            baseStoragePointerPressSlot_ = -1;
            baseStoragePointerPressCanOpenMenu_ = false;
            baseStoragePointerDragTriggered_ = false;
        }

        if (input.grabOrPlacePressed()) {
            closeUiCommandMenu(baseStorageCommandMenu_);
            baseStorageCommandSlot_ = -1;
            const int current = selectedGlobalSlot();
            if (!baseStorageGrabbedActive_) {
                const bool hasCurrent = storageGlobalSlotIsWarehouse(current)
                    ? hasWarehouseItemAt(storageLocalSlot(current))
                    : hasBackpackItemAt(storageLocalSlot(current));
                if (hasCurrent) {
                    baseStorageGrabbedActive_ = true;
                    baseStorageGrabbedFromSlot_ = current;
                    baseStatus_ = "つかみ中";
                } else {
                    baseStatus_ = "アイテム未選択";
                }
            } else {
                if (!tryTransferBySlots(baseStorageGrabbedFromSlot_, current)) {
                    baseStatus_ = "移動先を反対側にしてください";
                }
                baseStorageGrabbedActive_ = false;
                baseStorageGrabbedFromSlot_ = -1;
            }
        }

        if (input.useItemPressed() || input.confirmPressed()) {
            if (baseStorageGrabbedActive_) {
                const int current = selectedGlobalSlot();
                if (!tryTransferBySlots(baseStorageGrabbedFromSlot_, current)) {
                    baseStatus_ = "移動先を反対側にしてください";
                }
                baseStorageGrabbedActive_ = false;
                baseStorageGrabbedFromSlot_ = -1;
            } else {
                openStorageCommandMenuAt(selectedGlobalSlot());
            }
        }

        ui.block(storagePanelRect());
        return;
    }

    if (baseProcessingActive_) {
        const auto closeProcessingCommand = [this]() {
            closeUiCommandMenu(baseProcessingCommandMenu_);
            baseProcessingCommandSlot_ = -1;
        };
        const auto openProcessingCommand = [&](int slotIndex) {
            if (!processingScreenSlotAvailable(slotIndex)) {
                baseStatus_ = "この作業はできません";
                return false;
            }
            const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
            const std::array<UiCommandMenuItem, 1> items{{{mode == ProcessingMode::Repair ? "修理する" : "強化する", true}}};
            baseProcessingCommandSlot_ = slotIndex;
            openUiCommandMenu(
                baseProcessingCommandMenu_,
                baseProcessingGridSlotRect(slotIndex).pos,
                merchantPanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                144.0f,
                2);
            return true;
        };
        if (uiCancelRequested(baseCancelState_, input, ui, merchantPanelRect())) {
            if (baseProcessingCommandMenu_.open) {
                closeProcessingCommand();
            } else {
                baseProcessingActive_ = false;
                baseStatus_.clear();
            }
            return;
        }
        if (input.toggleShortcutRowPressed()) {
            closeProcessingCommand();
            baseProcessingMode_ = (baseProcessingMode_ + 1) % BaseProcessingModeCount;
        }
        const ProcessingMode currentProcessingMode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
        const std::array<UiCommandMenuItem, 1> commandItems{{{currentProcessingMode == ProcessingMode::Repair ? "修理する" : "強化する", true}}};
        const bool commandOpenBeforeUpdate = baseProcessingCommandMenu_.open;
        const int commandSelection = updateUiCommandMenu(
            baseProcessingCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0 && baseProcessingCommandSlot_ >= 0) {
            applyProcessingScreenSlot(baseProcessingCommandSlot_);
            closeProcessingCommand();
            ui.block(merchantPanelRect());
            return;
        } else if (!baseProcessingCommandMenu_.open && commandOpenBeforeUpdate) {
            baseProcessingCommandSlot_ = -1;
        }
        if (baseProcessingCommandMenu_.open) {
            ui.block(merchantPanelRect());
            return;
        }
        constexpr int Columns = 8;
        const int slotCount = inventory_.screenSlotCount();
        baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, slotCount - 1));
        if (input.pressed(InputAction::MoveLeft)) {
            baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - 1);
        }
        if (input.pressed(InputAction::MoveRight)) {
            baseProcessingSelection_ = std::min(slotCount - 1, baseProcessingSelection_ + 1);
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - Columns);
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseProcessingSelection_ = std::min(slotCount - 1, baseProcessingSelection_ + Columns);
        }
        for (int i = 0; i < BaseProcessingModeCount; ++i) {
            const UiRect rect = baseProcessingModeRect(i);
            if (rect.contains(ui.mouse())) {
                baseProcessingMode_ = i;
            }
            if (ui.pressed(rect)) {
                baseProcessingMode_ = i;
                return;
            }
        }
        for (int i = 0; i < slotCount; ++i) {
            const UiRect rect = baseProcessingGridSlotRect(i);
            if (rect.contains(ui.mouse())) {
                baseProcessingSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseProcessingSelection_ = i;
                openProcessingCommand(i);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            openProcessingCommand(baseProcessingSelection_);
            return;
        }
        ui.block(merchantPanelRect());
        return;
    }

    if (baseSellActive_) {
        refreshMerchantStock(false);
        const UiRect merchantBounds = baseMerchantMode_ == MerchantUiMode::ChooseAction ? basePanelRect() : merchantPanelRect();
        const auto closeMerchantCommands = [this]() {
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandIndex_ = -1;
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
        };
        const auto closeMerchant = [&]() {
            closeMerchantCommands();
            baseSellActive_ = false;
            baseMerchantMode_ = MerchantUiMode::Closed;
            baseStatus_.clear();
        };
        const auto returnToMerchantMenu = [&]() {
            closeMerchantCommands();
            baseMerchantMode_ = MerchantUiMode::ChooseAction;
            baseMerchantActionSelection_ = 0;
            baseStatus_.clear();
        };
        const auto moveGridSelection = [&input](int& selection, int count) {
            constexpr int Columns = 8;
            if (count <= 0) {
                selection = 0;
                return;
            }
            selection = std::clamp(selection, 0, count - 1);
            if (input.pressed(InputAction::MoveLeft)) {
                selection = std::max(0, selection - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                selection = std::min(count - 1, selection + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                selection = std::max(0, selection - Columns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                selection = std::min(count - 1, selection + Columns);
            }
        };
        const auto openSellCommand = [&](int slotIndex) {
            if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
                baseStatus_ = "売却対象がありません";
                return;
            }
            const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slotIndex);
            const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slotIndex);
            if (stack == nullptr && instance == nullptr) {
                baseStatus_ = "売却対象がありません";
                return;
            }
            if (instance != nullptr && instance->instance.protectionEnabled) {
                baseStatus_ = "保護中";
                return;
            }
            baseMerchantSellCommandIndex_ = slotIndex;
            const bool stackItem = stack != nullptr;
            const std::array<UiCommandMenuItem, 2> items{{{stackItem ? "1個売る" : "売る", true}, {"すべて売る", stackItem}}};
            openUiCommandMenu(
                baseMerchantSellCommandMenu_,
                merchantGridSlotRect(slotIndex).pos,
                merchantPanelRect(),
                stackItem ? 2 : 1,
                items.data(),
                168.0f,
                2);
        };
        const auto openBuyCommand = [&](int index) {
            if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
                baseStatus_ = "購入できる商品がありません";
                return;
            }
            baseMerchantBuyCommandIndex_ = index;
            const std::array<UiCommandMenuItem, 1> items{{{"買う", canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(index)])}}};
            openUiCommandMenu(
                baseMerchantBuyCommandMenu_,
                merchantGridSlotRect(index).pos,
                merchantPanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                120.0f,
                2);
        };

        if (uiCancelRequested(baseCancelState_, input, ui, merchantBounds)) {
            if (baseMerchantSellCommandMenu_.open || baseMerchantBuyCommandMenu_.open) {
                closeMerchantCommands();
            } else if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
                closeMerchant();
            } else {
                returnToMerchantMenu();
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
            closeMerchantCommands();
            constexpr int ChoiceCount = 2;
            baseMerchantActionSelection_ = std::clamp(baseMerchantActionSelection_, 0, ChoiceCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseMerchantActionSelection_ = (baseMerchantActionSelection_ + ChoiceCount - 1) % ChoiceCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseMerchantActionSelection_ = (baseMerchantActionSelection_ + 1) % ChoiceCount;
            }
            for (int i = 0; i < ChoiceCount; ++i) {
                const UiRect rect = merchantChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseMerchantActionSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseMerchantActionSelection_ = i;
                    if (i == 0) {
                        baseMerchantMode_ = MerchantUiMode::Buy;
                    } else if (i == 1) {
                        baseMerchantMode_ = MerchantUiMode::Sell;
                    }
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                if (baseMerchantActionSelection_ == 0) {
                    baseMerchantMode_ = MerchantUiMode::Buy;
                } else if (baseMerchantActionSelection_ == 1) {
                    baseMerchantMode_ = MerchantUiMode::Sell;
                }
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::Sell) {
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
            const bool stackCommand = baseMerchantSellCommandIndex_ >= 0 && inventory_.screenObjectStackAt(baseMerchantSellCommandIndex_) != nullptr;
            const std::array<UiCommandMenuItem, 2> commandItems{{{stackCommand ? "1個売る" : "売る", true}, {"すべて売る", stackCommand}}};
            const int commandItemCount = stackCommand ? 2 : 1;
            const bool commandOpenBeforeUpdate = baseMerchantSellCommandMenu_.open;
            const int commandSelection = updateUiCommandMenu(
                baseMerchantSellCommandMenu_,
                ui,
                input,
                commandItems.data(),
                commandItemCount);
            if (commandSelection >= 0 && baseMerchantSellCommandIndex_ >= 0) {
                sellMerchantScreenSlot(baseMerchantSellCommandIndex_, commandSelection == 1 && stackCommand ? 0 : 1);
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            } else if (!baseMerchantSellCommandMenu_.open && commandOpenBeforeUpdate) {
                baseMerchantSellCommandIndex_ = -1;
            }
            if (baseMerchantSellCommandMenu_.open) {
                ui.block(merchantBounds);
                return;
            }

            moveGridSelection(baseSellSelection_, inventory_.screenSlotCount());
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const UiRect rect = merchantGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseSellSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseSellSelection_ = i;
                    openSellCommand(i);
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openSellCommand(baseSellSelection_);
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::Buy) {
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandIndex_ = -1;
            const bool commandEnabled = baseMerchantBuyCommandIndex_ >= 0 &&
                baseMerchantBuyCommandIndex_ < static_cast<int>(merchantStock_.size()) &&
                canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(baseMerchantBuyCommandIndex_)]);
            const std::array<UiCommandMenuItem, 1> commandItems{{{"買う", commandEnabled}}};
            const bool commandOpenBeforeUpdate = baseMerchantBuyCommandMenu_.open;
            const int commandSelection = updateUiCommandMenu(
                baseMerchantBuyCommandMenu_,
                ui,
                input,
                commandItems.data(),
                static_cast<int>(commandItems.size()));
            if (commandSelection >= 0 && baseMerchantBuyCommandIndex_ >= 0) {
                buyMerchantProduct(baseMerchantBuyCommandIndex_);
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            } else if (!baseMerchantBuyCommandMenu_.open && commandOpenBeforeUpdate) {
                baseMerchantBuyCommandIndex_ = -1;
            }
            if (baseMerchantBuyCommandMenu_.open) {
                ui.block(merchantBounds);
                return;
            }

            moveGridSelection(baseMerchantBuySelection_, static_cast<int>(merchantStock_.size()));
            for (int i = 0; i < static_cast<int>(merchantStock_.size()); ++i) {
                const UiRect rect = merchantGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseMerchantBuySelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseMerchantBuySelection_ = i;
                    openBuyCommand(i);
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openBuyCommand(baseMerchantBuySelection_);
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        returnToMerchantMenu();
        ui.block(merchantBounds);
        return;
    }    if (baseUpgradeActive_) {
        if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
            baseUpgradeActive_ = false;
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + BaseUpgradeItemCount - 1) % BaseUpgradeItemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + 1) % BaseUpgradeItemCount;
        }
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            const UiRect rect = baseUpgradeItemRect(i);
            if (rect.contains(ui.mouse())) {
                baseUpgradeSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseUpgradeSelection_ = i;
                buyUpgrade(i);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            buyUpgrade(baseUpgradeSelection_);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (baseMiningStartChoiceActive_) {
        if (input.backPressed()) {
            baseMiningStartChoiceActive_ = false;
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + BaseMiningStartChoiceCount - 1) % BaseMiningStartChoiceCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + 1) % BaseMiningStartChoiceCount;
        }
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            const UiRect rect = baseMiningStartChoiceRect(i);
            if (rect.contains(ui.mouse())) {
                baseMiningStartSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseMiningStartSelection_ = i;
                if (i == 1 && unlockedWarpPointCount_ <= 0) {
                    baseStatus_ = "解放済みワープポイントがありません";
                    return;
                }
                if (i == 2) {
                    if (!canRegenerateCurrentStage()) {
                        baseStatus_ = "全ワープポイント発見後に再生成できます";
                        return;
                    }
                    if (retainedWorldDropCountForCurrentStage() > 0 && !baseRegenerateConfirmActive_) {
                        baseRegenerateConfirmActive_ = true;
                        baseStatus_ = "未回収ドロップが消えます。もう一度選ぶと再生成します";
                        return;
                    }
                    baseRegenerateConfirmActive_ = false;
                    requestMiningStartTransition(false, true);
                    return;
                }
                baseRegenerateConfirmActive_ = false;
                requestMiningStartTransition(i == 1, false);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            if (baseMiningStartSelection_ == 1 && unlockedWarpPointCount_ <= 0) {
                baseStatus_ = "解放済みワープポイントがありません";
                return;
            }
            if (baseMiningStartSelection_ == 2) {
                if (!canRegenerateCurrentStage()) {
                    baseStatus_ = "全ワープポイント発見後に再生成できます";
                    return;
                }
                if (retainedWorldDropCountForCurrentStage() > 0 && !baseRegenerateConfirmActive_) {
                    baseRegenerateConfirmActive_ = true;
                    baseStatus_ = "未回収ドロップが消えます。もう一度選ぶと再生成します";
                    return;
                }
                baseRegenerateConfirmActive_ = false;
                requestMiningStartTransition(false, true);
                return;
            }
            baseRegenerateConfirmActive_ = false;
            requestMiningStartTransition(baseMiningStartSelection_ == 1, false);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (input.pausePressed()) {
        mode_ = ScreenMode::PauseMenu;
        pauseReturnMode_ = ScreenMode::Base;
        pausePage_ = PauseMenuPage::Main;
        return;
    }

    const auto interact = [this](const BaseFacility& facility) {
        if (!facility.enabled) {
            return;
        }
        switch (facility.onInteract) {
        case BaseFacilityAction::MineExit:
            baseMiningStartChoiceActive_ = true;
            baseMiningStartSelection_ = unlockedWarpPointCount_ > 0 ? 1 : 0;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Storage:
            baseStorageActive_ = true;
            baseStorageFocusWarehouse_ = false;
            baseStorageBackpackCursor_ = 0;
            baseStorageWarehouseCursor_ = 0;
            baseStorageWarehousePage_ = 0;
            closeUiCommandMenu(baseStorageCommandMenu_);
            baseStorageCommandSlot_ = -1;
            baseStoragePointerPressSlot_ = -1;
            baseStoragePointerPressMouse_ = {};
            baseStoragePointerPressCanOpenMenu_ = false;
            baseStoragePointerDragTriggered_ = false;
            baseStorageGrabbedActive_ = false;
            baseStorageGrabbedFromSlot_ = -1;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Merchant:
            if (merchantRefreshPending_) {
                refreshMerchantStock(true);
                merchantRefreshPending_ = false;
            } else {
                refreshMerchantStock(false);
            }
            baseSellActive_ = true;
            baseMerchantMode_ = MerchantUiMode::ChooseAction;
            baseMerchantActionSelection_ = 0;
            baseSellSelection_ = 0;
            baseMerchantBuySelection_ = 0;
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandIndex_ = -1;
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Forge:
            baseUpgradeActive_ = true;
            baseUpgradeSelection_ = 0;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Processing:
            baseProcessingActive_ = true;
            baseProcessingMode_ = 0;
            baseProcessingSelection_ = 0;
            closeUiCommandMenu(baseProcessingCommandMenu_);
            baseProcessingCommandSlot_ = -1;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Bookshelf:
            openBookshelf();
            break;
        case BaseFacilityAction::Diary: {
            std::string message;
            saveSaveData(message);
            baseStatus_ = message;
            break;
        }
        case BaseFacilityAction::RingWorkshop:
            if (facility.unlocked) {
                openRingWorkshop();
            } else {
                baseStatus_ = "リング工房用スペース: まだ解禁されていません";
            }
            break;
        case BaseFacilityAction::HomeEntrance:
            baseOutdoorPlayerPosition_ = basePlayerPosition_;
            requestBaseAreaCrossfade(
                BaseArea::HomeInterior,
                {640.0f, 542.0f},
                {0.0f, -1.0f},
                "主人公の家に入りました");
            break;
        case BaseFacilityAction::HomeExit:
            requestBaseAreaCrossfade(
                BaseArea::Outdoor,
                baseOutdoorPlayerPosition_,
                {0.0f, 1.0f},
                "屋外拠点に戻りました");
            break;
        case BaseFacilityAction::MonicaTalk:
            startBaseMonicaDialogue();
            break;
        }
    };

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    const float playerRadius = balance_.playerRadius;
    const auto baseCollision = [&](Vec2 position) {
        const UiRect bounds = baseMapBounds();
        if (position.x - playerRadius < bounds.pos.x ||
            position.y - playerRadius < bounds.pos.y ||
            position.x + playerRadius > bounds.pos.x + bounds.size.x ||
            position.y + playerRadius > bounds.pos.y + bounds.size.y) {
            return true;
        }
        const Vec2 passabilityProbe = playerSpriteFootAnchor(position);
        const int minTileX = static_cast<int>(std::floor((passabilityProbe.x - playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int maxTileX = static_cast<int>(std::floor((passabilityProbe.x + playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int minTileY = static_cast<int>(std::floor((passabilityProbe.y - playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int maxTileY = static_cast<int>(std::floor((passabilityProbe.y + playerRadius) / static_cast<float>(BaseEditGridSize)));
        for (int tileY = minTileY; tileY <= maxTileY; ++tileY) {
            for (int tileX = minTileX; tileX <= maxTileX; ++tileX) {
                if (!isBasePassabilityBlocked(baseArea_, tileX, tileY)) {
                    continue;
                }
                const UiRect tileRect{
                    {static_cast<float>(tileX * BaseEditGridSize), static_cast<float>(tileY * BaseEditGridSize)},
                    {static_cast<float>(BaseEditGridSize), static_cast<float>(BaseEditGridSize)},
                };
                if (circleIntersectsRect(passabilityProbe, playerRadius, tileRect)) {
                    return true;
                }
            }
        }
        return false;
    };

    const Vec2 moveAxis = input.moveAxis();
    const bool walkingNow = lengthSquared(moveAxis) > 0.0001f;
    if (walkingNow != basePlayerSpriteWalking_) {
        basePlayerSpriteWalking_ = walkingNow;
        basePlayerSpriteAnimationTime_ = 0.0f;
    } else {
        basePlayerSpriteAnimationTime_ += dt;
    }
    maybeSpawnPlayerFootstepDust(
        playerSpriteFootAnchor(basePlayerPosition_),
        lengthSquared(moveAxis) > 0.0001f ? moveAxis : basePlayerFacing_,
        basePlayerSpriteWalking_,
        playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
        previousBasePlayerDustFrame_);
    if (lengthSquared(moveAxis) > 0.0001f) {
        basePlayerFacing_ = normalize(moveAxis);
        const Vec2 delta = moveAxis * balance_.playerSpeed * dt;
        Vec2 next = basePlayerPosition_ + Vec2{delta.x, 0.0f};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
        next = basePlayerPosition_ + Vec2{0.0f, delta.y};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
    }

    const BaseFacility* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const BaseFacility& facility : facilities) {
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }

    if (input.mouseLeftPressed() && !ui.pointerConsumed()) {
        for (const BaseFacility& facility : facilities) {
            if (!facility.rect.contains(ui.mouse())) {
                continue;
            }
            ui.consumePointer();
            if (baseInteractionAvailable(basePlayerPosition_, facility)) {
                interact(facility);
            } else if (facility.enabled) {
                baseStatus_ = "近くまで移動してください";
            }
            return;
        }
    }

    if (input.confirmPressed() && nearest != nullptr) {
        interact(*nearest);
        return;
    }
}

void Game::updatePauseMenu(const Input& input, UiContext& ui)
{
    const UiRect cancelPanel = pausePage_ == PauseMenuPage::QuitConfirm ? quitConfirmRect() : pausePanelRect();
    if (uiCancelRequested(pauseCancelState_, input, ui, cancelPanel)) {
        leavePausePage();
        return;
    }

    if (pausePage_ == PauseMenuPage::QuitConfirm) {
        pauseConfirmSelection_ = 1;
        const UiRect quitRect = quitConfirmButtonRect(1);
        if (ui.pressed(quitRect)) {
            quitRequested_ = true;
            return;
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            quitRequested_ = true;
            return;
        }
        ui.block(quitConfirmRect());
        return;
    }

    if (pausePage_ != PauseMenuPage::Main) {
        ui.block(pausePanelRect());
        return;
    }

    if (input.pressed(InputAction::MoveUp)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + PauseMenuItemCount - 1) % PauseMenuItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + 1) % PauseMenuItemCount;
    }
    for (int i = 0; i < PauseMenuItemCount; ++i) {
        const UiRect rect = pauseMenuItemRect(i);
        if (rect.contains(ui.mouse())) {
            pauseMenuSelection_ = i;
        }
        if (ui.pressed(rect)) {
            pauseMenuSelection_ = i;
            choosePauseMenuItem(i);
            return;
        }
    }
    if (input.confirmPressed() || input.useItemPressed()) {
        choosePauseMenuItem(pauseMenuSelection_);
        return;
    }

    ui.block(pausePanelRect());
}

void Game::updateGameOverScreen(const Input& input, UiContext& ui)
{
    if (input.pressed(InputAction::MoveUp)) {
        gameOverSelection_ = (gameOverSelection_ + GameOverItemCount - 1) % GameOverItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        gameOverSelection_ = (gameOverSelection_ + 1) % GameOverItemCount;
    }

    for (int i = 0; i < GameOverItemCount; ++i) {
        const UiRect rect = gameOverItemRect(i);
        if (rect.contains(ui.mouse())) {
            gameOverSelection_ = i;
        }
        if (ui.pressed(rect)) {
            gameOverSelection_ = i;
            if (i == 0) {
                retryAfterGameOver();
            } else {
                returnToBaseAfterGameOver();
            }
            return;
        }
    }

    if (input.confirmPressed() || input.useItemPressed()) {
        if (gameOverSelection_ == 0) {
            retryAfterGameOver();
        } else {
            returnToBaseAfterGameOver();
        }
        return;
    }

    ui.block(gameOverPanelRect());
}

void Game::updateStageClearScreen(const Input& input, UiContext& ui)
{
    if (input.pressed(InputAction::MoveUp)) {
        stageClearSelection_ = (stageClearSelection_ + StageClearItemCount - 1) % StageClearItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        stageClearSelection_ = (stageClearSelection_ + 1) % StageClearItemCount;
    }

    for (int i = 0; i < StageClearItemCount; ++i) {
        const UiRect rect = stageClearItemRect(i);
        if (rect.contains(ui.mouse())) {
            stageClearSelection_ = i;
        }
        if (ui.pressed(rect)) {
            stageClearSelection_ = i;
            returnToBaseFromNormalStage(true, false);
            return;
        }
    }

    if (input.confirmPressed() || input.useItemPressed()) {
        returnToBaseFromNormalStage(true, false);
        return;
    }

    ui.block(stageClearPanelRect());
}

void Game::updateScreenMode(
    const Input& input,
    UiContext& ui,
    float dt,
    std::vector<EffectDiscoveryEvent>* discoveryEvents)
{
    if (mode_ == ScreenMode::OpeningKamishibai) {
        updateOpeningKamishibai(input, dt);
        return;
    }

    if (mode_ == ScreenMode::Title) {
        updateTitleScreen(input, ui);
        return;
    }

    if (mode_ == ScreenMode::WorldLoading) {
        return;
    }

    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        updateObjectImageScaleEditScreen(input, ui);
        return;
    }

    if (dialogue_.active()) {
        dialogue_.update(input, dt);
        ui.consumePointer();
        return;
    }

    if (mode_ == ScreenMode::Base) {
        updateBaseScreen(input, ui, dt);
        return;
    }

    if (player_.hp <= 0 && mode_ != ScreenMode::GameOver && mode_ != ScreenMode::StageClear) {
        enterGameOver();
    }
    if (mode_ == ScreenMode::GameOver) {
        updateGameOverScreen(input, ui);
        return;
    }
    if (mode_ == ScreenMode::StageClear) {
        updateStageClearScreen(input, ui);
        return;
    }

    if (levels_.isChoosing()) {
        inventory_.setOpen(false);
        mode_ = ScreenMode::LevelUp;
    } else if (mode_ == ScreenMode::LevelUp) {
        mode_ = ScreenMode::Playing;
    }

    switch (mode_) {
    case ScreenMode::OpeningKamishibai:
        updateOpeningKamishibai(input, dt);
        break;
    case ScreenMode::Title:
        updateTitleScreen(input, ui);
        break;
    case ScreenMode::Base:
        updateBaseScreen(input, ui, dt);
        break;
    case ScreenMode::WorldLoading:
        break;
    case ScreenMode::Playing:
        if (enemyTestActive_) {
            updateEnemyTestUi(input, ui);
            if (mode_ != ScreenMode::Playing || (enemyTestUiVisible_ && input.pausePressed())) {
                return;
            }
        }
        if (input.pausePressed()) {
            mode_ = ScreenMode::PauseMenu;
            pauseReturnMode_ = ScreenMode::Playing;
            pausePage_ = PauseMenuPage::Main;
            return;
        }
        updateRingStatusHud(ui);
        inventory_.update(input, ui, player_, spellRing_, effectDispatcher_, false, discoveryEvents, &encyclopedia_);
        if (inventory_.isOpen()) {
            inventoryReturnToPause_ = false;
            mode_ = ScreenMode::Inventory;
            return;
        }
        if (input.activeRingDelta() != 0) {
            switchActiveRingWithLog(input.activeRingDelta());
        }
        inventory_.updateShortcuts(
            input,
            ui,
            player_,
            spellRing_,
            effectDispatcher_,
            camera_.width(),
            camera_.height(),
            discoveryEvents,
            &encyclopedia_);
        break;
    case ScreenMode::PauseMenu:
        updatePauseMenu(input, ui);
        break;
    case ScreenMode::Inventory:
        inventory_.updateScreen(input, ui, player_, spellRing_, effectDispatcher_, discoveryEvents, &encyclopedia_);
        if (!inventory_.isOpen()) {
            mode_ = inventoryReturnToPause_ ? ScreenMode::PauseMenu : ScreenMode::Playing;
            pausePage_ = PauseMenuPage::Main;
            inventoryReturnToPause_ = false;
        }
        break;
    case ScreenMode::Ring:
        updateRingScreen(input, ui, dt);
        break;
    case ScreenMode::ObjectImageScaleEdit:
        updateObjectImageScaleEditScreen(input, ui);
        break;
    case ScreenMode::LevelUp:
        upgrades_.update(input, ui, levels_, spellRing_, levelRingRadiusPoints_, levelRingSpeedPoints_);
        if (!levels_.isChoosing()) {
            mode_ = ScreenMode::Playing;
        }
        break;
    case ScreenMode::GameOver:
        break;
    case ScreenMode::StageClear:
        break;
    }
}

bool Game::gameProgressPaused() const
{
    return dialogue_.active() || mode_ != ScreenMode::Playing;
}

bool Game::basePresentationActive() const
{
    if (mode_ == ScreenMode::Base || mode_ == ScreenMode::WorldLoading) {
        return true;
    }
    if (pauseReturnMode_ != ScreenMode::Base) {
        return false;
    }
    return mode_ == ScreenMode::PauseMenu ||
        mode_ == ScreenMode::Inventory ||
        mode_ == ScreenMode::Ring;
}

void Game::pushDungeonLog(std::string message, std::string mergeKey)
{
    if (message.empty()) {
        return;
    }

    if (!mergeKey.empty()) {
        for (DungeonLogEntry& entry : dungeonLogs_) {
            if (entry.mergeKey == mergeKey && entry.count == 0 && entry.age <= DungeonLogMergeSeconds) {
                entry.message = std::move(message);
                entry.age = 0.0f;
                entry.lifetime = DungeonLogLifetime;
                return;
            }
        }
    }

    DungeonLogEntry entry;
    entry.message = std::move(message);
    entry.mergeKey = std::move(mergeKey);
    entry.lifetime = DungeonLogLifetime;
    dungeonLogs_.push_back(std::move(entry));
    while (static_cast<int>(dungeonLogs_.size()) > DungeonLogMaxVisible) {
        dungeonLogs_.erase(dungeonLogs_.begin());
    }
}

void Game::pushCountedDungeonLog(std::string label, int amount, std::string suffix, std::string mergeKey)
{
    if (amount <= 0 || label.empty()) {
        return;
    }

    if (!mergeKey.empty()) {
        for (DungeonLogEntry& entry : dungeonLogs_) {
            if (entry.mergeKey == mergeKey && entry.count > 0 && entry.age <= DungeonLogMergeSeconds) {
                entry.count += amount;
                entry.age = 0.0f;
                entry.lifetime = DungeonLogLifetime;
                return;
            }
        }
    }

    DungeonLogEntry entry;
    entry.label = std::move(label);
    entry.suffix = std::move(suffix);
    entry.mergeKey = std::move(mergeKey);
    entry.count = amount;
    entry.lifetime = DungeonLogLifetime;
    dungeonLogs_.push_back(std::move(entry));
    while (static_cast<int>(dungeonLogs_.size()) > DungeonLogMaxVisible) {
        dungeonLogs_.erase(dungeonLogs_.begin());
    }
}

void Game::updateDungeonLogs(float dt)
{
    for (DungeonLogEntry& entry : dungeonLogs_) {
        entry.age += dt;
    }
    dungeonLogs_.erase(
        std::remove_if(dungeonLogs_.begin(), dungeonLogs_.end(), [](const DungeonLogEntry& entry) {
            return entry.age >= entry.lifetime;
        }),
        dungeonLogs_.end());
}

void Game::appendPickupLogs(const std::vector<WorldDropPickupEvent>& pickupEvents)
{
    const auto moneyLogLabel = [](int amount) {
        return inlineWorldIconTag(worldIconKey(moneyWorldIconForAmount(amount))) + " お金";
    };

    for (const WorldDropPickupEvent& event : pickupEvents) {
        const int quantity = std::max(1, event.quantity);
        if (event.kind == WorldDropKind::Money) {
            bool merged = false;
            for (DungeonLogEntry& entry : dungeonLogs_) {
                if (entry.mergeKey == "money" && entry.count > 0 && entry.age <= DungeonLogMergeSeconds) {
                    entry.count += quantity;
                    entry.label = moneyLogLabel(entry.count);
                    entry.suffix = " を入手";
                    entry.age = 0.0f;
                    entry.lifetime = DungeonLogLifetime;
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                pushCountedDungeonLog(moneyLogLabel(quantity), quantity, " を入手", "money");
            }
        } else if (event.kind == WorldDropKind::Material) {
            MaterialType materialType = MaterialType::Count;
            const bool knownMaterial = materialTypeFromSaveName(event.id, materialType);
            const std::string label = knownMaterial
                ? std::string(materialTypeDisplayName(materialType))
                : (event.name.empty() ? event.id : event.name);
            const std::string iconTag = knownMaterial ? inlineMaterialIconTag(materialType) : "";
            const std::string iconPrefix = iconTag.empty() ? "" : iconTag + " ";
            pushCountedDungeonLog(iconPrefix + label, quantity, " を入手", "material:" + event.id);
        } else if (event.kind == WorldDropKind::Object) {
            const std::string label = event.name.empty() ? event.id : event.name;
            const std::string iconPrefix = objectCatalog_.registry.findById(event.id) != nullptr ? inlineItemTag(event.id) + " " : "";
            pushCountedDungeonLog(iconPrefix + label, quantity, " を入手", "object:" + event.id);
        }
    }
}

void Game::switchActiveRingWithLog(int delta)
{
    if (delta == 0) {
        return;
    }

    const int previousRing = spellRing_.activeRingIndex();
    spellRing_.switchActiveRing(delta);
    const int currentRing = spellRing_.activeRingIndex();
    if (currentRing != previousRing && mode_ == ScreenMode::Playing) {
        pushDungeonLog("Ring " + std::to_string(currentRing + 1) + " に切替", "ring_switch");
    }
}

int Game::unlockedRingHudCount() const
{
    return std::clamp(UnlockedRingCount, 0, SpellRingCount);
}

UiRect Game::ringStatusHudRect(int ringIndex, int unlockedRingCount) const
{
    const float screenHeight = static_cast<float>(camera_.height());
    const float totalHeight = static_cast<float>(unlockedRingCount) * RingStatusHudHeight +
        static_cast<float>(std::max(0, unlockedRingCount - 1)) * RingStatusHudGap;
    const float startY = std::max(
        TopInfoBarY + TopInfoBarHeight + 8.0f,
        screenHeight - RingStatusHudBottomMargin - totalHeight);
    return {{
        RingStatusHudLeftMargin,
        startY + static_cast<float>(ringIndex) * (RingStatusHudHeight + RingStatusHudGap),
    }, {RingStatusHudWidth, RingStatusHudHeight}};
}

void Game::updateRingStatusHud(UiContext& ui)
{
    const int unlockedRingCount = unlockedRingHudCount();
    for (int ringIndex = 0; ringIndex < unlockedRingCount; ++ringIndex) {
        if (!ui.pressed(ringStatusHudRect(ringIndex, unlockedRingCount))) {
            continue;
        }
        switchActiveRingWithLog(ringIndex - spellRing_.activeRingIndex());
        return;
    }
}

std::string Game::currentMapDisplayName() const
{
    if (basePresentationActive()) {
        return baseAreaName(baseArea_);
    }
    if (enemyTestActive_) {
        return "敵テスト";
    }
    if (!currentStageDefinition_.name.empty()) {
        return currentStageDefinition_.name;
    }
    return currentStageId_.empty() ? std::string{"Unknown Map"} : currentStageId_;
}

void Game::renderTopInfoBar(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float barWidth = std::max(1.0f, screenWidth - TopInfoBarX * 2.0f);
    renderer.fillRect({TopInfoBarX, TopInfoBarY}, {barWidth, TopInfoBarHeight}, {8, 10, 18, 210});
    renderer.drawRect({TopInfoBarX, TopInfoBarY}, {barWidth, TopInfoBarHeight}, {96, 104, 132, 170});

    const int textScale = 2;
    const Vec2 textMeasure = renderer.measureText("0", textScale);
    const float textY = TopInfoBarY + std::max(0.0f, (TopInfoBarHeight - textMeasure.y) * 0.5f) + 8.0f;
    InlineItemTextStyle moneyStyle;
    moneyStyle.text = {246, 230, 174, 255};
    moneyStyle.scale = textScale;
    moneyStyle.iconTextGap = TopInfoBarIconTextGap;
    moneyStyle.iconScale = TopInfoBarIconSize / std::max(1.0f, textMeasure.y);

    InlineItemTextStyle materialStyle = moneyStyle;
    materialStyle.text = {232, 236, 244, 255};

    const std::string moneyEntry = inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + std::to_string(money_) + "G";
    const Vec2 moneyEntrySize = measureInlineItemText(renderer, moneyEntry, moneyStyle);
    constexpr std::array<TopInfoMaterial, 4> Materials{{
        {MaterialType::OldWoodBuildingMaterial},
        {MaterialType::EnhancementOre},
        {MaterialType::MoonFragment},
        {MaterialType::ManaDrop},
    }};

    std::array<std::string, Materials.size()> materialEntries{};
    std::array<Vec2, Materials.size()> materialEntrySizes{};
    float rightGroupWidth = moneyEntrySize.x;
    for (std::size_t i = 0; i < Materials.size(); ++i) {
        materialEntries[i] = inlineMaterialIconTag(Materials[i].type) + std::to_string(inventory_.materialCount(Materials[i].type));
        materialEntrySizes[i] = measureInlineItemText(renderer, materialEntries[i], materialStyle);
        rightGroupWidth += TopInfoBarGroupGap + materialEntrySizes[i].x;
    }

    const float rightEdge = TopInfoBarX + barWidth - TopInfoBarPaddingX;
    float rightX = rightEdge - rightGroupWidth;
    rightX = std::max(TopInfoBarX + TopInfoBarPaddingX, rightX);

    const float mapX = TopInfoBarX + TopInfoBarPaddingX;
    const float mapMaxWidth = std::max(0.0f, rightX - mapX - 18.0f);
    const std::string mapName = fittedSingleLineText(renderer, currentMapDisplayName(), mapMaxWidth, textScale);
    renderer.drawText({mapX, textY}, mapName, {246, 246, 252, 255}, textScale);

    float x = rightX;
    drawInlineItemText(renderer, objectCatalog_, {x, textY}, moneyEntry, moneyStyle);
    x += moneyEntrySize.x;

    for (std::size_t i = 0; i < Materials.size(); ++i) {
        x += TopInfoBarGroupGap;
        drawInlineItemText(renderer, objectCatalog_, {x, textY}, materialEntries[i], materialStyle);
        x += materialEntrySizes[i].x;
    }
}

void Game::renderOpeningKamishibai(Renderer& renderer) const
{
    openingRenderer_.render(renderer, openingPlayer_, camera_.width(), camera_.height());
}

void Game::renderTitleScreen(Renderer& renderer) const
{
    openingRenderer_.renderTitleScreen(renderer, openingTitleImagePath(openingPages_), camera_.width(), camera_.height());
}

void Game::renderScreenTransitionOverlay(Renderer& renderer)
{
    if (!screenTransition_.active()) {
        renderer.destroyFrameSnapshot(screenTransitionSnapshot_);
        return;
    }

    if (screenTransition_.phase == ScreenTransitionPhase::CrossFadeCapture) {
        renderer.destroyFrameSnapshot(screenTransitionSnapshot_);
        screenTransitionSnapshot_ = renderer.captureFrameSnapshot();
        if (!screenTransition_.applied) {
            applyScreenTransitionTarget(screenTransition_.target);
            screenTransition_.applied = true;
        }
        screenTransition_.elapsed = 0.0f;
        screenTransition_.phase = ScreenTransitionPhase::CrossFading;
    }

    float alpha = 0.0f;
    switch (screenTransition_.phase) {
    case ScreenTransitionPhase::Idle:
        break;
    case ScreenTransitionPhase::CrossFadeCapture:
        break;
    case ScreenTransitionPhase::CrossFading:
        alpha = 1.0f - smoothStep01(screenTransition_.elapsed / std::max(0.001f, ScreenTransitionCrossFadeSeconds));
        if (alpha > 0.0f) {
            renderer.setScreenSpace();
            renderer.drawFrameSnapshot(
                screenTransitionSnapshot_,
                {0.0f, 0.0f},
                {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
                {255, 255, 255, alphaByte(255.0f * alpha)});
        }
        return;
    case ScreenTransitionPhase::FadingOut:
        alpha = smoothStep01(screenTransition_.elapsed / std::max(0.001f, ScreenTransitionFadeOutSeconds));
        break;
    case ScreenTransitionPhase::HoldBlack:
        alpha = 1.0f;
        break;
    case ScreenTransitionPhase::FadingIn:
        alpha = 1.0f - smoothStep01(screenTransition_.elapsed / std::max(0.001f, ScreenTransitionFadeInSeconds));
        break;
    }

    if (alpha <= 0.0f) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect(
        {0.0f, 0.0f},
        {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
        {0, 0, 0, alphaByte(255.0f * alpha)});
}

void Game::renderDungeonStatusHud(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());
    const UiRect panel{{
        std::max(8.0f, screenWidth - DungeonStatusHudRightMargin - DungeonStatusHudWidth),
        std::max(TopInfoBarY + TopInfoBarHeight + 8.0f, screenHeight - DungeonStatusHudBottomMargin - DungeonStatusHudHeight),
    }, {DungeonStatusHudWidth, DungeonStatusHudHeight}};

    drawUiSubPanel(renderer, panel);

    const Vec2 content = panel.pos + Vec2{DungeonStatusHudPadding, DungeonStatusHudPadding};
    constexpr int TextScale = 2;
    char buffer[64];

    renderer.drawText(content, "STATUS", {246, 246, 252, 255}, TextScale);

    const int hpMax = std::max(1, player_.maxHp);
    const int hp = std::clamp(player_.hp, 0, hpMax);
    std::snprintf(buffer, sizeof(buffer), "HP %02d/%02d", hp, hpMax);
    renderer.drawText(content + Vec2{0.0f, 28.0f}, buffer, {255, 232, 232, 255}, TextScale);

    const float barWidth = panel.size.x - DungeonStatusHudPadding * 2.0f;
    const Vec2 hpBarPos = content + Vec2{0.0f, 52.0f};
    UiGaugeStyle hpGaugeStyle;
    hpGaugeStyle.fill.start = {224, 74, 84, 255};
    hpGaugeStyle.fill.end = {255, 126, 116, 255};
    hpGaugeStyle.track = {42, 18, 24, 230};
    hpGaugeStyle.trackInner = {58, 24, 32, 220};
    hpGaugeStyle.trackOuter = {255, 220, 224, 82};
    hpGaugeStyle.shadow = {0, 0, 0, 90};
    hpGaugeStyle.highlight = {255, 244, 244, 92};
    hpGaugeStyle.capGlow = {255, 116, 128, 58};
    hpGaugeStyle.capCore = {255, 244, 244, 210};
    hpGaugeStyle.trackInnerInset = 4.0f;
    hpGaugeStyle.shadowOffsetY = 2.0f;
    hpGaugeStyle.shadowExtra = 5.0f;
    drawUiGauge(
        renderer,
        {hpBarPos, {barWidth, DungeonStatusHudBarHeight}},
        static_cast<float>(hp) / static_cast<float>(hpMax),
        hpGaugeStyle);

    std::snprintf(buffer, sizeof(buffer), "Lv %02d", std::max(1, player_.level));
    renderer.drawText(content + Vec2{0.0f, 70.0f}, buffer, {232, 236, 244, 255}, TextScale);

    const int xpToNext = std::max(1, player_.xpToNext);
    const int xp = std::clamp(player_.xp, 0, xpToNext);
    std::snprintf(buffer, sizeof(buffer), "EXP %02d/%02d", xp, xpToNext);
    renderer.drawText(content + Vec2{0.0f, 94.0f}, buffer, {222, 236, 255, 255}, TextScale);
}

void Game::renderRingStatusHud(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const int unlockedRingCount = unlockedRingHudCount();

    char buffer[96];
    for (int ringIndex = 0; ringIndex < unlockedRingCount; ++ringIndex) {
        const UiRect panel = ringStatusHudRect(ringIndex, unlockedRingCount);
        const bool active = ringIndex == spellRing_.activeRingIndex();

        drawUiSubPanel(renderer, panel);
        if (active) {
            renderer.drawRect(panel.pos, panel.size, {255, 236, 158, 255});
            renderer.drawRect(panel.pos + Vec2{1.0f, 1.0f}, panel.size - Vec2{2.0f, 2.0f}, {255, 236, 158, 190});
        }

        const Vec2 content = panel.pos + Vec2{RingStatusHudPadding, RingStatusHudPadding};
        const auto& items = spellRing_.itemsForRing(ringIndex);
        float ringWeight = 0.0f;
        for (const SpellRingItem& item : items) {
            ringWeight += std::max(0.0f, item.weight);
        }

        std::snprintf(buffer, sizeof(buffer), "%sRing %d", active ? "> " : "", ringIndex + 1);
        renderer.drawText(content, buffer, active ? Color{255, 236, 158, 255} : ui::Text, 2);

        std::snprintf(buffer, sizeof(buffer), "アイテム数 %02d / %02d", static_cast<int>(items.size()), spellRing_.maxItemCount());
        renderer.drawText(content + Vec2{0.0f, 24.0f}, buffer, {232, 236, 244, 255}, 2);

        std::snprintf(buffer, sizeof(buffer), "重量 %.1f / %.1f", ringWeight, spellRing_.maxEquippedWeight());
        renderer.drawText(content + Vec2{0.0f, 48.0f}, buffer, {222, 236, 255, 255}, 2);
    }
}

void Game::renderDungeonLogs(Renderer& renderer) const
{
    if (dungeonLogs_.empty()) {
        return;
    }

    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());
    const float topLimit = TopInfoBarY + TopInfoBarHeight + 8.0f;
    const float statusTopY = screenHeight - DungeonStatusHudBottomMargin - DungeonStatusHudHeight;
    const float maxBottomY = std::max(topLimit + DungeonLogRowHeight, statusTopY - DungeonLogStatusGap);

    int visibleCount = std::min(static_cast<int>(dungeonLogs_.size()), DungeonLogMaxVisible);
    const auto blockHeight = [](int count) {
        return static_cast<float>(count) * DungeonLogRowHeight +
            static_cast<float>(std::max(0, count - 1)) * DungeonLogGap;
    };
    while (visibleCount > 0 && blockHeight(visibleCount) > maxBottomY - topLimit) {
        --visibleCount;
    }
    if (visibleCount <= 0) {
        return;
    }

    const float totalHeight = blockHeight(visibleCount);
    const float x = std::max(8.0f, screenWidth - DungeonLogRightMargin - DungeonLogWidth);
    const float y = std::clamp(screenHeight * DungeonLogTargetYRatio, topLimit, maxBottomY - totalHeight);
    const int firstIndex = static_cast<int>(dungeonLogs_.size()) - visibleCount;

    constexpr int TextScale = 2;
    const Vec2 textMeasure = renderer.measureText("0", TextScale);
    for (int i = 0; i < visibleCount; ++i) {
        const DungeonLogEntry& entry = dungeonLogs_[static_cast<std::size_t>(firstIndex + i)];
        const float rowY = y + static_cast<float>(i) * (DungeonLogRowHeight + DungeonLogGap);
        const float fadeDenom = std::max(0.01f, entry.lifetime - DungeonLogFadeStart);
        const float fade = entry.age <= DungeonLogFadeStart
            ? 1.0f
            : std::clamp(1.0f - (entry.age - DungeonLogFadeStart) / fadeDenom, 0.0f, 1.0f);

        const unsigned char rightAlpha = static_cast<unsigned char>(std::clamp(210.0f * fade, 0.0f, 210.0f));
        renderer.fillGradientRect(
            {x, rowY},
            {DungeonLogWidth, DungeonLogRowHeight},
            {0, 0, 0, 0},
            {0, 0, 0, rightAlpha},
            GradientDirection::LeftToRight);

        std::string message = entry.message;
        if (entry.count > 0) {
            message = entry.label + " x" + std::to_string(entry.count) + entry.suffix;
        }
        const unsigned char textAlpha = static_cast<unsigned char>(std::clamp(255.0f * fade, 0.0f, 255.0f));
        InlineItemTextStyle textStyle;
        textStyle.text = {246, 246, 252, textAlpha};
        textStyle.scale = TextScale;
        textStyle.iconTextGap = 4.0f;
        textStyle.iconScale = TopInfoBarIconSize / std::max(1.0f, textMeasure.y);
        textStyle.outlineEnabled = true;
        textStyle.outline = {0, 0, 0, textAlpha};
        textStyle.outlinePx = 2;
        message = fittedInlineItemText(renderer, std::move(message), DungeonLogWidth - 26.0f, textStyle);
        const Vec2 textPos{
            x + DungeonLogWidth - 14.0f,
            rowY + std::max(0.0f, (DungeonLogRowHeight - textMeasure.y) * 0.5f) + 6.0f,
        };
        drawInlineItemTextRightAligned(renderer, objectCatalog_, textPos, message, textStyle);
    }
}

void Game::renderWorldLoadingScreen(Renderer& renderer, float totalSeconds) const
{
    if (mode_ != ScreenMode::WorldLoading) {
        return;
    }

    renderer.setScreenSpace();
    const float screenW = static_cast<float>(camera_.width());
    const float screenH = static_cast<float>(camera_.height());

    constexpr float BarW = 280.0f;
    constexpr float TrackH = 16.0f;
    constexpr float MarginRight = 28.0f;
    constexpr float MarginBottom = 28.0f;
    const float barLeft = std::max(12.0f, screenW - MarginRight - BarW);
    const float barCenterY = std::max(20.0f, screenH - MarginBottom - TrackH * 0.5f);
    const UiRect bar{{barLeft, barCenterY - TrackH * 0.5f}, {BarW, TrackH}};
    const float progress = std::clamp(worldBuildProgress(), 0.0f, 1.0f);
    const float pulse = 0.76f + 0.24f * std::sin(totalSeconds * 5.0f);
    UiGaugeStyle loadingGaugeStyle;
    loadingGaugeStyle.fill.start = {static_cast<unsigned char>(108.0f + 48.0f * pulse), 206, 236, 230};
    loadingGaugeStyle.fill.end = {132, 230, 250, 230};
    loadingGaugeStyle.track = {12, 16, 24, 190};
    loadingGaugeStyle.trackInner = {30, 38, 52, 220};
    loadingGaugeStyle.trackOuter = {218, 228, 244, 78};
    loadingGaugeStyle.shadow = {0, 0, 0, 105};
    loadingGaugeStyle.tick = {255, 255, 255, 32};
    loadingGaugeStyle.highlight = {255, 255, 255, 118};
    loadingGaugeStyle.capGlow = {132, 230, 250, 78};
    loadingGaugeStyle.capCore = {246, 252, 255, 225};
    loadingGaugeStyle.tickCount = 8;
    loadingGaugeStyle.shimmer = {255, 255, 255, 76};
    loadingGaugeStyle.shimmerPhase =
        std::fmod(std::max(0.0f, totalSeconds) * 116.0f, BarW + loadingGaugeStyle.shimmerWidth) /
        (BarW + loadingGaugeStyle.shimmerWidth);
    drawUiGauge(renderer, bar, progress, loadingGaugeStyle);

    const std::string label = "LOADING";
    const Vec2 labelSize = renderer.measureText(label, 2);
    const Vec2 labelPos{bar.pos.x + bar.size.x - labelSize.x, bar.pos.y - labelSize.y - 9.0f};
    renderer.drawText(labelPos + Vec2{1.0f, 1.0f}, label, {0, 0, 0, 170}, 2);
    renderer.drawText(labelPos, label, {246, 246, 252, 230}, 2);
}

void Game::renderBookshelfScreen(Renderer& renderer) const
{
    const UiRect panel = basePanelRect();
    renderer.drawText(panel.pos + Vec2{202.0f, 214.0f}, "本棚", {246, 235, 255, 255}, 3);

    char buffer[256];
    const auto menuName = [](int index) {
        switch (index) {
        case 0:
            return "アイテム図鑑";
        case 1:
            return "宝図鑑";
        case 2:
            return "敵図鑑";
        default:
            return "";
        }
    };
    const auto objectAt = [this](BookshelfPage page, int targetIndex) -> const ObjectDefinition* {
        int index = 0;
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((page == BookshelfPage::Treasures && treasure) ||
                (page == BookshelfPage::Items && !treasure)) {
                if (index == targetIndex) {
                    return &object;
                }
                ++index;
            }
        }
        return nullptr;
    };

    if (bookshelfPage_ == BookshelfPage::Menu) {
        renderer.drawText(panel.pos + Vec2{76.0f, 224.0f}, "図鑑を選んでください", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BookshelfMenuItemCount; ++i) {
            drawUiButton(renderer, bookshelfItemRect(i), menuName(i), i == bookshelfSelection_);
        }
        return;
    }

    std::vector<std::string> lines;
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (enemy.name.empty() ? enemy.id : enemy.name);
            std::snprintf(buffer, sizeof(buffer), "%s  [%s]", name.c_str(), encyclopediaStageName(stage));
            lines.emplace_back(buffer);
        }
    } else {
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((bookshelfPage_ == BookshelfPage::Treasures && !treasure) ||
                (bookshelfPage_ == BookshelfPage::Items && treasure)) {
                continue;
            }
            const EncyclopediaStage stage = encyclopedia_.objectStage(object.id, treasure);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object.name.empty() ? object.id : object.name);
            std::snprintf(buffer, sizeof(buffer), "%s  [%s]", name.c_str(), encyclopediaStageName(stage));
            lines.emplace_back(buffer);
        }
    }

    const char* title = bookshelfPage_ == BookshelfPage::Items
        ? "アイテム図鑑"
        : (bookshelfPage_ == BookshelfPage::Treasures ? "宝図鑑" : "敵図鑑");
    renderer.drawText(panel.pos + Vec2{74.0f, 224.0f}, title, {198, 198, 206, 255}, 2);
    if (lines.empty()) {
        renderer.drawText(panel.pos + Vec2{94.0f, 276.0f}, "記録対象がありません", {150, 150, 160, 255}, 2);
    } else {
        const int visibleCount = std::min(BookshelfVisibleRows, static_cast<int>(lines.size()));
        const int firstVisibleIndex = std::clamp(
            bookshelfSelection_ - visibleCount / 2,
            0,
            std::max(0, static_cast<int>(lines.size()) - visibleCount));
        for (int i = 0; i < visibleCount; ++i) {
            const int lineIndex = firstVisibleIndex + i;
            drawUiButton(renderer, bookshelfItemRect(i), lines[static_cast<std::size_t>(lineIndex)], lineIndex == bookshelfSelection_);
        }
    }

    const UiRect bookshelfDetailPanel{{414.0f, 548.0f}, {452.0f, 144.0f}};
    const Vec2 bookshelfDetailContent = uiSubPanelContentPos(bookshelfDetailPanel);
    drawUiSubPanel(renderer, bookshelfDetailPanel);
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        if (bookshelfSelection_ >= 0 && bookshelfSelection_ < static_cast<int>(enemyCatalog_.enemies.size())) {
            const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(bookshelfSelection_)];
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (enemy.name.empty() ? enemy.id : enemy.name);
            std::snprintf(buffer, sizeof(buffer), "%s / %s", name.c_str(), encyclopediaStageName(stage));
            renderer.drawText(bookshelfDetailContent, buffer, {255, 230, 150, 255}, 2);
        }
    } else if (const ObjectDefinition* object = objectAt(bookshelfPage_, bookshelfSelection_)) {
        const bool treasure = object->category == "\xE5\xAE\x9D";
        const EncyclopediaStage stage = encyclopedia_.objectStage(object->id, treasure);
        const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object->name.empty() ? object->id : object->name);
        std::snprintf(buffer, sizeof(buffer), "%s / %s", name.c_str(), encyclopediaStageName(stage));
        renderer.drawText(bookshelfDetailContent, buffer, {255, 230, 150, 255}, 2);
        const std::vector<std::string> effects = encyclopedia_.getObjectEffectDisplayLines(
            object->id,
            objectCatalog_,
            EffectRevealMode::WithUnknown);
        const std::string effectText = joinEffectLines(effects);
        renderer.drawWrappedText(
            bookshelfDetailContent + Vec2{0.0f, 34.0f},
            effectText,
            bookshelfDetailPanel.size.x - ui::SubPanelPadding.x * 2.0f,
            {232, 234, 242, 255},
            2);
    }
}

void Game::renderBaseBackdrop(Renderer& renderer) const
{
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    if (baseArea_ == BaseArea::HomeInterior) {
        renderer.fillRect(map.pos, map.size, {74, 58, 52, 255});
        renderer.drawRect(map.pos, map.size, {184, 150, 108, 255});
        renderer.fillRect({76.0f, 72.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 584.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({1160.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({132.0f, 132.0f}, {1016.0f, 436.0f}, {118, 92, 66, 255});
        renderer.drawText({558.0f, 88.0f}, "主人公の家", {246, 235, 255, 255}, 2);
    } else {
        if (renderer.hasBaseMapTexture()) {
            renderer.drawBaseMapTexture(map.pos, map.size);
        } else {
            renderer.fillRect(map.pos, map.size, {68, 96, 58, 255});
            renderer.drawRect(map.pos, map.size, {156, 128, 82, 255});
            renderer.fillRect({62.0f, 456.0f}, {1156.0f, 88.0f}, {98, 84, 58, 255});
            renderer.fillRect({566.0f, 130.0f}, {132.0f, 430.0f}, {92, 78, 54, 255});
            renderer.fillRect({330.0f, 72.0f}, {154.0f, 100.0f}, {96, 54, 62, 255});
            renderer.drawRect({330.0f, 72.0f}, {154.0f, 100.0f}, {216, 184, 130, 255});
            renderer.drawText({350.0f, 106.0f}, "主人公の家", {246, 235, 255, 255}, 2);
            renderer.fillRect({600.0f, 586.0f}, {80.0f, 34.0f}, {38, 30, 36, 255});
            renderer.drawCircle({640.0f, 602.0f}, 42.0f, {160, 122, 80, 255});
        }
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
    for (int pass = 0; pass < 2; ++pass) {
        const bool drawEnabled = pass == 1;
        for (const BaseFacility& facility : facilities) {
            if (facility.enabled != drawEnabled) {
                continue;
            }
            const bool texturedOutdoorBase = baseArea_ == BaseArea::Outdoor && renderer.hasBaseMapTexture();
            Color fill = facility.enabled ? Color{96, 82, 82, 255} : Color{84, 62, 56, 255};
            if (texturedOutdoorBase) {
                fill.a = facility.enabled ? 120 : 80;
            }
            if (!facility.unlocked) {
                fill = {58, 58, 64, 255};
                if (texturedOutdoorBase) {
                    fill.a = 120;
                }
            }
            if (&facility == nearest) {
                fill = {118, 98, 58, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            if (&facility == hovered) {
                fill = {124, 104, 72, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            renderer.fillRect(facility.rect.pos, facility.rect.size, fill);
            renderer.drawRect(facility.rect.pos, facility.rect.size, facility.enabled ? Color{220, 200, 150, 255} : Color{120, 108, 98, 255});
            renderer.drawText(facility.rect.pos + Vec2{8.0f, 8.0f}, facility.displayName, facility.enabled ? Color{248, 238, 214, 255} : Color{154, 146, 138, 255}, 2);
        }
    }
    renderBaseEditOverlay(renderer);

    const Vec2 basePlayerFootAnchor = playerSpriteFootAnchor(basePlayerPosition_);
    renderer.drawActorShadow(basePlayerFootAnchor, PlayerSpriteDrawSize);
    renderPlayerFootstepDust(renderer);
    if (renderer.hasPlayerSheet()) {
        renderer.drawPlayerSprite(
            playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
            basePlayerFootAnchor,
            PlayerSpriteDrawSize,
            basePlayerFacing_.x > 0.0f,
            {255, 255, 255, 255},
            {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
    } else {
        renderer.fillCircle(basePlayerPosition_, balance_.playerRadius, {118, 72, 168, 255});
        renderer.drawLine(basePlayerPosition_, basePlayerPosition_ + basePlayerFacing_ * 22.0f, {235, 210, 255, 255});
    }

    renderTopInfoBar(renderer);
}

void Game::renderBaseScreen(Renderer& renderer) const
{
    if (!basePresentationActive()) {
        return;
    }

    renderer.setScreenSpace();
    if (baseStorageActive_) {
        renderBaseBackdrop(renderer);
        const UiRect panel = storagePanelRect();
        UiCancelControlScope cancelScope(baseCancelState_);
        UiWindowScope storageWindow(
            renderer,
            "base.storage",
            panel,
            "収納箱",
            "F/Enter コマンド  G つかむ/置く  Z/X 倉庫ページ  Esc/右クリック 戻る",
            UiWindowOptions{true, true});

        const std::vector<StorageEntry> warehouseEntries = warehouseStorageEntries();
        const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
        const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);

        const auto selectedGlobalSlot = [this]() {
            return storageGlobalSlotFromLocal(
                baseStorageFocusWarehouse_,
                baseStorageFocusWarehouse_ ? baseStorageWarehouseCursor_ : baseStorageBackpackCursor_);
        };
        const auto entryViewForBackpackSlot = [this](int slot) {
            InventoryUiEntryView entry{};
            if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot)) {
                entry.item = &stack->item;
                entry.stackCount = stack->count;
                return entry;
            }
            if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot)) {
                entry.item = &instance->item;
                entry.instance = &instance->instance;
                entry.stackCount = 1;
            }
            return entry;
        };
        const auto entryViewForWarehouseSlot = [this, &warehouseEntries, warehousePage](int slot) {
            InventoryUiEntryView entry{};
            const int entryIndex = warehouseEntryIndexAtStorageSlot(warehousePage * StoragePaneSlotCount + slot);
            if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseEntries.size())) {
                return entry;
            }
            const StorageEntry storageEntry = warehouseEntries[static_cast<std::size_t>(entryIndex)];
            entry.item = storageEntryItem(storageEntry, true);
            entry.instance = storageEntryInstance(storageEntry, true);
            entry.stackCount = storageEntryStackCount(storageEntry, true);
            return entry;
        };

        const auto drawTextCentered = [&renderer](UiRect rect, float y, std::string_view text, Color color, int scale) {
            const Vec2 size = renderer.measureText(text, scale);
            renderer.drawText({rect.pos.x + (rect.size.x - size.x) * 0.5f, y}, text, color, scale);
        };
        const auto drawStorageHeader = [&renderer](float x, float y, std::string_view title, std::string_view count, Color color) {
            renderer.drawText({x, y}, title, color, 3);
            const Vec2 titleSize = renderer.measureText(title, 3);
            renderer.drawText(
                {x + titleSize.x + StorageHeaderCountGap, y + StorageHeaderCountYOffset},
                count,
                color,
                StorageHeaderCountScale);
        };

        char buffer[256];
        const Color backpackHeaderColor = baseStorageFocusWarehouse_ ? Color{198, 198, 206, 255} : Color{255, 230, 150, 255};
        std::snprintf(buffer, sizeof(buffer), "\xEF\xBC\x88%d/24\xEF\xBC\x89", backpackUsedSlots());
        drawStorageHeader(StorageHeaderTextX, StorageBackpackHeaderY, "リュック", buffer, backpackHeaderColor);

        drawUiSeparator(
            renderer,
            {{StorageGridX, StorageDividerY - ui::SeparatorHeight * 0.5f}, {StorageGridW, ui::SeparatorHeight}},
            {255, 255, 255, 220});
        const Color warehouseHeaderColor = baseStorageFocusWarehouse_ ? Color{255, 230, 150, 255} : Color{198, 198, 206, 255};
        std::snprintf(buffer, sizeof(buffer), "\xEF\xBC\x88%d/%d\xEF\xBC\x89", warehouseUsedSlots(), warehouseCapacity());
        drawStorageHeader(StorageHeaderTextX, StorageBottomHeaderY, "倉庫", buffer, warehouseHeaderColor);

        const UiRect prevPageRect = storagePrevPageButtonRect();
        const UiRect pageTextRect = storagePageTextRect();
        const UiRect nextPageRect = storageNextPageButtonRect();
        std::snprintf(buffer, sizeof(buffer), "%d/%d", warehousePage + 1, warehousePageCount);
        drawTextCentered(pageTextRect, StorageBottomHeaderY + StoragePageTextYOffset, buffer, {198, 198, 206, 255}, StoragePageTextScale);
        drawUiRectButton(renderer, prevPageRect, "<", false);
        drawUiRectButton(renderer, nextPageRect, ">", false);

        for (int i = 0; i < StoragePaneSlotCount; ++i) {
            const int backpackGlobalSlot = storageGlobalSlotFromLocal(false, i);
            drawInventoryUiSlot(
                renderer,
                storageBackpackSlotRect(i),
                entryViewForBackpackSlot(i),
                selectedGlobalSlot() == backpackGlobalSlot,
                40.0f);
        }
        for (int i = 0; i < StoragePaneSlotCount; ++i) {
            const int warehouseGlobalSlot = storageGlobalSlotFromLocal(true, i);
            drawInventoryUiSlot(
                renderer,
                storageWarehouseSlotRect(i),
                entryViewForWarehouseSlot(i),
                selectedGlobalSlot() == warehouseGlobalSlot,
                40.0f);
        }

        const int detailSlot = (baseStorageCommandMenu_.open && baseStorageCommandSlot_ >= 0)
            ? baseStorageCommandSlot_
            : selectedGlobalSlot();
        InventoryUiEntryView detailEntry{};
        if (storageGlobalSlotIsWarehouse(detailSlot)) {
            detailEntry = entryViewForWarehouseSlot(storageLocalSlot(detailSlot));
        } else {
            detailEntry = entryViewForBackpackSlot(storageLocalSlot(detailSlot));
        }
        const UiRect detailPanel{{StorageDetailX, StorageDetailY}, {330.0f, 520.0f}};
        drawInventoryUiDetailPanel(renderer, detailPanel, detailEntry, objectCatalog_, encyclopedia_, false);

        const int commandSlotIndex = baseStorageCommandSlot_ >= 0 ? baseStorageCommandSlot_ : selectedGlobalSlot();
        const bool commandWarehouse = storageGlobalSlotIsWarehouse(commandSlotIndex);
        const char* commandLabel = commandWarehouse ? "取り出す" : "倉庫へしまう";
        const bool hasCommandItem = commandWarehouse
            ? (entryViewForWarehouseSlot(storageLocalSlot(commandSlotIndex)).item != nullptr)
            : (entryViewForBackpackSlot(storageLocalSlot(commandSlotIndex)).item != nullptr);
        const std::array<UiCommandMenuItem, 1> commandItems{{{commandLabel, hasCommandItem}}};
        drawUiCommandMenu(renderer, baseStorageCommandMenu_, commandItems.data(), static_cast<int>(commandItems.size()));

        if (!baseStatus_.empty()) {
            drawUiBodyMessageBelow(renderer, storageSlotRectByGlobal(selectedGlobalSlot()), baseStatus_, ui::Text);
        }
        return;
    }

    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    if (baseArea_ == BaseArea::HomeInterior) {
        renderer.fillRect(map.pos, map.size, {74, 58, 52, 255});
        renderer.drawRect(map.pos, map.size, {184, 150, 108, 255});
        renderer.fillRect({76.0f, 72.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 584.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({1160.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({132.0f, 132.0f}, {1016.0f, 436.0f}, {118, 92, 66, 255});
        renderer.drawText({558.0f, 88.0f}, "主人公の家", {246, 235, 255, 255}, 2);
    } else {
        if (renderer.hasBaseMapTexture()) {
            renderer.drawBaseMapTexture(map.pos, map.size);
        } else {
            renderer.fillRect(map.pos, map.size, {68, 96, 58, 255});
            renderer.drawRect(map.pos, map.size, {156, 128, 82, 255});
        renderer.fillRect({62.0f, 456.0f}, {1156.0f, 88.0f}, {98, 84, 58, 255});
        renderer.fillRect({566.0f, 130.0f}, {132.0f, 430.0f}, {92, 78, 54, 255});
        renderer.fillRect({330.0f, 72.0f}, {154.0f, 100.0f}, {96, 54, 62, 255});
        renderer.drawRect({330.0f, 72.0f}, {154.0f, 100.0f}, {216, 184, 130, 255});
        renderer.drawText({350.0f, 106.0f}, "主人公の家", {246, 235, 255, 255}, 2);
        renderer.fillRect({600.0f, 586.0f}, {80.0f, 34.0f}, {38, 30, 36, 255});
            renderer.drawCircle({640.0f, 602.0f}, 42.0f, {160, 122, 80, 255});
        }
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
    for (int pass = 0; pass < 2; ++pass) {
        const bool drawEnabled = pass == 1;
        for (const BaseFacility& facility : facilities) {
            if (facility.enabled != drawEnabled) {
                continue;
            }
            const bool texturedOutdoorBase = baseArea_ == BaseArea::Outdoor && renderer.hasBaseMapTexture();
            Color fill = facility.enabled ? Color{96, 82, 82, 255} : Color{84, 62, 56, 255};
            if (texturedOutdoorBase) {
                fill.a = facility.enabled ? 120 : 80;
            }
            if (!facility.unlocked) {
                fill = {58, 58, 64, 255};
                if (texturedOutdoorBase) {
                    fill.a = 120;
                }
            }
            if (&facility == nearest) {
                fill = {118, 98, 58, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            if (&facility == hovered) {
                fill = {124, 104, 72, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            renderer.fillRect(facility.rect.pos, facility.rect.size, fill);
            renderer.drawRect(facility.rect.pos, facility.rect.size, facility.enabled ? Color{220, 200, 150, 255} : Color{120, 108, 98, 255});
            renderer.drawText(facility.rect.pos + Vec2{8.0f, 8.0f}, facility.displayName, facility.enabled ? Color{248, 238, 214, 255} : Color{154, 146, 138, 255}, 2);
        }
    }
    renderBaseEditOverlay(renderer);

    const Vec2 basePlayerFootAnchor = playerSpriteFootAnchor(basePlayerPosition_);
    renderer.drawActorShadow(basePlayerFootAnchor, PlayerSpriteDrawSize);
    renderPlayerFootstepDust(renderer);
    if (renderer.hasPlayerSheet()) {
        renderer.drawPlayerSprite(
            playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
            basePlayerFootAnchor,
            PlayerSpriteDrawSize,
            basePlayerFacing_.x > 0.0f,
            {255, 255, 255, 255},
            {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
    } else {
        renderer.fillCircle(basePlayerPosition_, balance_.playerRadius, {118, 72, 168, 255});
        renderer.drawLine(basePlayerPosition_, basePlayerPosition_ + basePlayerFacing_ * 22.0f, {235, 210, 255, 255});
    }

    renderTopInfoBar(renderer);

    char buffer[256];
    const bool panelUiActive = baseRingWorkshopActive_ ||
        baseBookshelfActive_ ||
        baseProcessingActive_ ||
        baseSellActive_ ||
        baseUpgradeActive_ ||
        baseMiningStartChoiceActive_;
    const UiRect panel = (baseProcessingActive_ || (baseSellActive_ && baseMerchantMode_ != MerchantUiMode::ChooseAction))
        ? merchantPanelRect()
        : basePanelRect();
    std::optional<UiWindowScope> panelWindow;
    std::optional<UiCancelControlScope> panelCancelScope;
    if (panelUiActive) {
        const char* panelHelp = "F/Enter 決定  左クリック 決定  Esc/右クリック 戻る";
        if (baseBookshelfActive_) {
            panelHelp = bookshelfPage_ == BookshelfPage::Menu
                ? "F/Enter 決定  Esc/右クリック 戻る"
                : "Esc/右クリック 戻る";
        } else if (baseSellActive_) {
            panelHelp = baseMerchantMode_ == MerchantUiMode::Buy
                ? "F/Enter 買う  Esc/右クリック 戻る"
                : (baseMerchantMode_ == MerchantUiMode::Sell ? "F/Enter 売る  Esc/右クリック 戻る" : "F/Enter 決定  Esc/右クリック 戻る");
        } else if (baseProcessingActive_) {
            panelHelp = "F/Enter 実行  Tab 作業切替  Esc/右クリック 戻る";
        } else if (baseUpgradeActive_) {
            panelHelp = "F/Enter 強化  Esc/右クリック 戻る";
        } else if (baseMiningStartChoiceActive_) {
            panelHelp = "Esc/右クリック 戻る";
        }
        const char* panelTitle = "魔女の拠点";
        if (baseBookshelfActive_) {
            panelTitle = "本棚";
        } else if (baseRingWorkshopActive_) {
            panelTitle = "リング工房";
        } else if (baseProcessingActive_) {
            panelTitle = "作業台";
        } else if (baseSellActive_) {
            if (baseMerchantMode_ == MerchantUiMode::Buy) {
                panelTitle = "商人ワゴン 購入";
            } else if (baseMerchantMode_ == MerchantUiMode::Sell) {
                panelTitle = "商人ワゴン 売却";
            } else {
                panelTitle = "商人ワゴン";
            }
        } else if (baseUpgradeActive_) {
            panelTitle = "拠点強化炉";
        } else if (baseMiningStartChoiceActive_) {
            panelTitle = "採掘出口";
        }
        const bool panelCancelButton = !baseMiningStartChoiceActive_;
        if (panelCancelButton) {
            panelCancelScope.emplace(baseCancelState_);
        }
        panelWindow.emplace(renderer, "base.panel", panel, panelTitle, panelHelp, UiWindowOptions{true, panelCancelButton});
    }

    if (baseBookshelfActive_) {
        renderBookshelfScreen(renderer);
    } else if (baseRingWorkshopActive_) {
        renderer.drawText(panel.pos + Vec2{168.0f, 214.0f}, "リング工房", {246, 235, 255, 255}, 3);
        const int totalPoints = ringLevelUpgradePointTotal();
        std::snprintf(buffer, sizeof(buffer), "レベルアップ強化点 %d  現在: 半径%d / 速度%d  配分案: 半径%d / 速度%d",
            totalPoints,
            levelRingRadiusPoints_,
            levelRingSpeedPoints_,
            ringWorkshopDraftRadiusPoints_,
            ringWorkshopDraftSpeedPoints_);
        renderer.drawText(panel.pos + Vec2{54.0f, 224.0f}, buffer, {198, 198, 206, 255}, 2);

        const float currentRadius = effectiveInitialRingRadius(levelRingRadiusPoints_);
        const float currentSpeed = effectiveInitialRingSpeed(levelRingSpeedPoints_);
        const float draftRadius = effectiveInitialRingRadius(ringWorkshopDraftRadiusPoints_);
        const float draftSpeed = effectiveInitialRingSpeed(ringWorkshopDraftSpeedPoints_);
        std::snprintf(buffer, sizeof(buffer), "再調整後: 半径 %.0f -> %.0f / 速度 %.2f -> %.2f  費用 %dG 月のカケラ%d",
            currentRadius,
            draftRadius,
            currentSpeed,
            draftSpeed,
            ringWorkshopRespecMoneyCost(),
            ringWorkshopRespecMoonCost());
        renderer.drawText(panel.pos + Vec2{54.0f, 448.0f}, buffer, {198, 198, 206, 255}, 2);

        for (int i = 0; i < BaseRingWorkshopItemCount; ++i) {
            if (i == 0) {
                std::snprintf(buffer, sizeof(buffer), "再調整: 速度から半径へ  半径%d / 速度%d",
                    ringWorkshopDraftRadiusPoints_,
                    ringWorkshopDraftSpeedPoints_);
            } else if (i == 1) {
                std::snprintf(buffer, sizeof(buffer), "再調整: 半径から速度へ  半径%d / 速度%d",
                    ringWorkshopDraftRadiusPoints_,
                    ringWorkshopDraftSpeedPoints_);
            } else if (i == 2) {
                std::snprintf(buffer, sizeof(buffer), "再調整確定  %dG 月のカケラ%d",
                    ringWorkshopRespecMoneyCost(),
                    ringWorkshopRespecMoonCost());
            } else if (i >= 3 && i < 3 + RingWorkshopImplementedUpgradeCount) {
                const auto upgrade = static_cast<RingWorkshopUpgrade>(i - 3);
                const int level = ringWorkshopUpgradeLevel(upgrade);
                const int maxLevel = ringWorkshopUpgradeMaxLevel(upgrade);
                if (level >= maxLevel) {
                    std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  上限",
                        ringWorkshopUpgradeName(upgrade),
                        level,
                        maxLevel);
                } else {
                    std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  %.2f -> %.2f  %dG 月のカケラ%d",
                        ringWorkshopUpgradeName(upgrade),
                        level,
                        maxLevel,
                        ringWorkshopUpgradeCurrentValue(upgrade),
                        ringWorkshopUpgradeNextValue(upgrade),
                        ringWorkshopUpgradeMoneyCost(upgrade),
                        ringWorkshopUpgradeMoonCost(upgrade));
                }
            } else {
                const char* futureName = "";
                switch (i) {
                case 6:
                    futureName = "リング投げ距離強化";
                    break;
                case 7:
                    futureName = "リング投げクールダウン短縮";
                    break;
                case 8:
                    futureName = "リング重量ペナルティ軽減";
                    break;
                case 9:
                    futureName = "リング装着枠増加";
                    break;
                default:
                    futureName = "未解禁項目";
                    break;
                }
                std::snprintf(buffer, sizeof(buffer), "%s  未解禁", futureName);
            }
            drawUiButton(renderer, baseRingWorkshopItemRect(i), buffer, i == baseRingWorkshopSelection_, uiActionButtonStyle());
        }

        std::snprintf(buffer, sizeof(buffer), "所持 %dG / 月のカケラ %d   F/Enter 実行  左右で配分変更  Esc/右クリック 戻る",
            money_,
            inventory_.materialCount(MaterialType::MoonFragment));
        renderer.drawText(panel.pos + Vec2{54.0f, 448.0f}, buffer, {198, 198, 206, 255}, 2);
        drawUiButton(renderer, ringWorkshopConfirmRect(), "再調整確定", false, uiActionButtonStyle());
    } else if (baseProcessingActive_) {
        for (int i = 0; i < BaseProcessingModeCount; ++i) {
            drawUiButton(renderer, baseProcessingModeRect(i), processingModeName(static_cast<ProcessingMode>(i)), i == baseProcessingMode_);
        }

        const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
        const auto entryViewForScreenSlot = [this](int slot) {
            InventoryUiEntryView view{};
            if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot)) {
                view.item = &stack->item;
                view.stackCount = stack->count;
                return view;
            }
            if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot)) {
                view.item = &instance->item;
                view.instance = &instance->instance;
                view.stackCount = 1;
            }
            return view;
        };

        for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
            const InventoryUiEntryView view = entryViewForScreenSlot(i);
            const bool unavailable = view.item != nullptr && !processingScreenSlotAvailable(i);
            InventoryUiSlotStyle style{i == baseProcessingSelection_, unavailable, 48.0f};
            if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                style.showTopRightCount = true;
                style.topRightCount = view.stackCount;
            }
            drawInventoryUiSlot(renderer, baseProcessingGridSlotRect(i), view, style);
        }

        const UiRect detailPanel = merchantDetailPanelRect();
        const float moneyRight = detailPanel.pos.x;
        drawMoneySummaryText(renderer, {moneyRight, detailPanel.pos.y + 12.0f}, money_);

        const int selected = std::clamp(baseProcessingSelection_, 0, inventory_.screenSlotCount() - 1);
        const InventoryUiEntryView detailEntry = entryViewForScreenSlot(selected);
        const std::optional<StorageEntry> selectedEntry = processingEntryForScreenSlot(selected);
        drawUiSubPanel(renderer, detailPanel);

        const auto drawSectionLabel = [&renderer](UiRect targetPanel, float& y, std::string_view text) {
            const UiRect content = uiSubPanelContentRect(targetPanel);
            renderer.drawText({content.pos.x, y}, text, ui::Text, 2);
            y += 31.0f;
        };
        const auto drawTextRun = [&renderer](Vec2& pos, std::string_view text, Color color, int scale) {
            renderer.drawText(pos, text, color, scale);
            pos.x += renderer.measureText(text, scale).x;
        };
        const auto drawMoneyCostLine = [&](UiRect targetPanel, float& y, int moneyCost) {
            const UiRect content = uiSubPanelContentRect(targetPanel);
            Vec2 pos{content.pos.x, y};
            const Color numberColor = money_ >= moneyCost ? ui::Text : Color{238, 82, 82, 255};
            const std::string number = std::to_string(moneyCost);
            drawTextRun(pos, number, numberColor, 2);
            drawTextRun(pos, "G", ui::Text, 2);
            y += 31.0f;
        };
        const auto drawOreCostLine = [&](UiRect targetPanel, float& y, int oreCost) {
            const int ownedOre = inventory_.materialCount(MaterialType::EnhancementOre);
            const bool enough = ownedOre >= oreCost;
            const Color numberColor = enough ? ui::Text : Color{238, 82, 82, 255};
            const UiRect content = uiSubPanelContentRect(targetPanel);
            Vec2 pos{content.pos.x, y};
            drawTextRun(pos, "強化鉱石 ×", ui::Text, 2);
            drawTextRun(pos, std::to_string(oreCost), numberColor, 2);
            drawTextRun(pos, " (", ui::Text, 2);
            drawTextRun(pos, std::to_string(ownedOre), numberColor, 2);
            drawTextRun(pos, ")", ui::Text, 2);
            y += 31.0f;
        };

        if (detailEntry.item == nullptr || !selectedEntry) {
            float detailLineY = drawUiDetailHeader(renderer, detailPanel, "アイテム未選択");
            drawUiDetailText(renderer, detailPanel, detailLineY, "加工するアイテムを選択してください。");
        } else {
            const ItemData& item = *detailEntry.item;
            const ItemInstance* instance = detailEntry.instance;
            std::string detailTitle = item.name;
            if (instance == nullptr) {
                std::snprintf(buffer, sizeof(buffer), "%s x%d", item.name.c_str(), detailEntry.stackCount);
                detailTitle = buffer;
            }
            float detailLineY = drawUiDetailHeader(renderer, detailPanel, detailTitle);
            drawUiDetailText(renderer, detailPanel, detailLineY, item.description.empty() ? "-" : item.description);

            const int enhanceLevel = instance != nullptr ? instance->enhanceLevel : 0;
            const int attackBonus = instance != nullptr ? instance->attackBonus : 0;
            const int digBonus = instance != nullptr ? instance->digBonus : 0;
            const int durabilityBonus = instance != nullptr ? instance->durabilityBonus : 0;
            const int currentDurability = instance != nullptr ? instance->currentDurability : item.durability;
            const int maxDurability = instance != nullptr ? instance->maxDurability : item.durability;

            drawSectionLabel(detailPanel, detailLineY, "現在");
            std::snprintf(buffer, sizeof(buffer), "%d / %d", enhanceLevel, MaxItemEnhanceLevel);
            drawUiDetailLine(renderer, detailPanel, detailLineY, "強化Lv", buffer);
            if (maxDurability < 0) {
                std::snprintf(buffer, sizeof(buffer), "壊れない");
            } else {
                std::snprintf(buffer, sizeof(buffer), "%d/%d", currentDurability, maxDurability);
            }
            drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
            std::snprintf(buffer, sizeof(buffer), "+%d / +%d / +%d", attackBonus, digBonus, durabilityBonus);
            drawUiDetailLine(renderer, detailPanel, detailLineY, "補正", buffer);

            const bool available = processingEntryAvailable(*selectedEntry);
            std::string blockedReason;
            if (!available) {
                if (mode == ProcessingMode::Repair && selectedEntry->kind == StorageEntryKind::Stack) {
                    blockedReason = "この作業はできません";
                } else {
                    blockedReason = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
                }
            }

            if (mode == ProcessingMode::Repair) {
                drawSectionLabel(detailPanel, detailLineY, "修理後");
                if (maxDurability < 0 || selectedEntry->kind == StorageEntryKind::Stack) {
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", "-");
                } else {
                    std::snprintf(buffer, sizeof(buffer), "%d/%d -> %d/%d",
                        currentDurability,
                        maxDurability,
                        maxDurability,
                        maxDurability);
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
                }
            } else {
                drawSectionLabel(detailPanel, detailLineY, "強化後");
                std::snprintf(buffer, sizeof(buffer), "%d -> %d", enhanceLevel, std::min(MaxItemEnhanceLevel, enhanceLevel + 1));
                drawUiDetailLine(renderer, detailPanel, detailLineY, "強化Lv", buffer);
                if (mode == ProcessingMode::Attack) {
                    std::snprintf(buffer, sizeof(buffer), "+%d -> +%d", attackBonus, attackBonus + 1);
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "攻撃力", buffer);
                } else if (mode == ProcessingMode::Dig) {
                    std::snprintf(buffer, sizeof(buffer), "+%d -> +%d", digBonus, digBonus + 1);
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "抑制力", buffer);
                } else if (mode == ProcessingMode::Durability) {
                    if (maxDurability < 0) {
                        drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", "-");
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%d -> %d", maxDurability, maxDurability + 2);
                        drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
                    }
                }
            }

            if (available) {
                const int moneyCost = processingMoneyCost(*selectedEntry, mode);
                const int oreCost = processingOreCost(*selectedEntry, mode);
                drawSectionLabel(detailPanel, detailLineY, "強化素材");
                drawMoneyCostLine(detailPanel, detailLineY, moneyCost);
                if (oreCost > 0) {
                    drawOreCostLine(detailPanel, detailLineY, oreCost);
                }
                if (blockedReason.empty()) {
                    if (money_ < moneyCost) {
                        blockedReason = "所持金が足りません";
                    } else if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
                        blockedReason = "強化鉱石が足りません";
                    }
                }
            }

            if (blockedReason.empty()) {
                drawUiDetailText(renderer, detailPanel, detailLineY, mode == ProcessingMode::Repair ? "Enter 修理コマンド" : "Enter 強化コマンド");
            } else {
                drawUiDetailText(renderer, detailPanel, detailLineY, blockedReason);
            }
        }
        const std::array<UiCommandMenuItem, 1> processingCommandItems{{{mode == ProcessingMode::Repair ? "修理する" : "強化する", true}}};
        drawUiCommandMenu(
            renderer,
            baseProcessingCommandMenu_,
            processingCommandItems.data(),
            static_cast<int>(processingCommandItems.size()));
    } else if (baseSellActive_) {
        if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
            renderer.drawText(panel.pos + Vec2{176.0f, 214.0f}, "商人ワゴン", {246, 235, 255, 255}, 3);
            renderer.drawText(panel.pos + Vec2{88.0f, 250.0f}, "何を見ていくんだい？", {198, 198, 206, 255}, 2);
            constexpr std::array<std::string_view, 2> Choices{"買う", "売る"};
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                drawUiButton(renderer, merchantChoiceRect(i), Choices[static_cast<std::size_t>(i)], i == baseMerchantActionSelection_, uiActionButtonStyle());
            }
        } else {
            const bool buyMode = baseMerchantMode_ == MerchantUiMode::Buy;
            const auto entryViewForScreenSlot = [this](int slot) {
                InventoryUiEntryView view{};
                if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot)) {
                    view.item = &stack->item;
                    view.stackCount = stack->count;
                    return view;
                }
                if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot)) {
                    view.item = &instance->item;
                    view.instance = &instance->instance;
                    view.stackCount = 1;
                }
                return view;
            };

            const UiRect detailPanel = merchantDetailPanelRect();
            drawMoneySummaryText(renderer, {detailPanel.pos.x, detailPanel.pos.y + 12.0f}, money_);

            InventoryUiEntryView detailEntry{};
            std::vector<InventoryUiDetailExtraLine> extraLines;
            if (buyMode) {
                if (merchantStock_.empty()) {
                    renderer.drawText({92.0f, 210.0f}, "商品がありません", {198, 198, 206, 255}, 2);
                }
                for (int i = 0; i < static_cast<int>(merchantStock_.size()); ++i) {
                    const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(i)];
                    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
                    InventoryUiEntryView entry{};
                    entry.item = item;
                    entry.stackCount = 1;
                    const bool disabled = !canBuyMerchantProduct(product);
                    std::snprintf(buffer, sizeof(buffer), "%dG", product.price);
                    InventoryUiSlotStyle style{i == baseMerchantBuySelection_, disabled, 48.0f};
                    style.bottomLabel = buffer;
                    style.bottomLabelColor = disabled ? Color{238, 82, 82, 255} : ui::Text;
                    style.showTopRightCount = true;
                    style.topRightCount = product.quantity;
                    drawInventoryUiSlot(renderer, merchantGridSlotRect(i), entry, style);
                }
                if (!merchantStock_.empty()) {
                    const int selected = std::clamp(baseMerchantBuySelection_, 0, static_cast<int>(merchantStock_.size()) - 1);
                    const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(selected)];
                    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
                    detailEntry.item = item;
                    detailEntry.stackCount = 1;
                    std::snprintf(buffer, sizeof(buffer), "%dG", product.price);
                    extraLines.push_back({"価格", buffer, canBuyMerchantProduct(product) ? ui::Text : Color{238, 82, 82, 255}});
                    std::snprintf(buffer, sizeof(buffer), "%d", product.quantity);
                    extraLines.push_back({"在庫", buffer, product.quantity > 0 ? ui::Text : ui::TextDisabled});
                }
            } else {
                for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                    const InventoryUiEntryView view = entryViewForScreenSlot(i);
                    const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(i);
                    const InventoryObjectStack* stack = inventory_.screenObjectStackAt(i);
                    const bool protectedItem = instance != nullptr && instance->instance.protectionEnabled;
                    const UiRect rect = merchantGridSlotRect(i);
                    InventoryUiSlotStyle style{i == baseSellSelection_, protectedItem, 48.0f};
                    if (instance != nullptr) {
                        if (instance->instance.protectionEnabled) {
                            style.bottomLabel = "保護中";
                            style.bottomLabelColor = ui::TextDisabled;
                        } else {
                            std::snprintf(buffer, sizeof(buffer), "%dG", sellPrice(instance->item, &instance->instance));
                            style.bottomLabel = buffer;
                        }
                    } else if (stack != nullptr) {
                        std::snprintf(buffer, sizeof(buffer), "%dG", sellPrice(stack->item));
                        style.bottomLabel = buffer;
                    }
                    drawInventoryUiSlot(renderer, rect, view, style);
                    if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                        std::snprintf(buffer, sizeof(buffer), "x%d", view.stackCount);
                        renderer.drawText(rect.pos + Vec2{rect.size.x - 36.0f, 8.0f}, buffer, {255, 255, 255, 230}, 2);
                    }
                }
                const int selected = std::clamp(baseSellSelection_, 0, inventory_.screenSlotCount() - 1);
                detailEntry = entryViewForScreenSlot(selected);
                if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(selected)) {
                    if (instance->instance.protectionEnabled) {
                        extraLines.push_back({"売値", "保護中", ui::TextDisabled});
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%dG", sellPrice(instance->item, &instance->instance));
                        extraLines.push_back({"売値", buffer, ui::Text});
                    }
                } else if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(selected)) {
                    std::snprintf(buffer, sizeof(buffer), "%dG", sellPrice(stack->item));
                    extraLines.push_back({"売値", buffer, ui::Text});
                }
            }

            drawInventoryUiDetailPanel(renderer, detailPanel, detailEntry, objectCatalog_, encyclopedia_, false, extraLines);
            if (buyMode) {
                const bool buyCommandEnabled = baseMerchantBuyCommandIndex_ >= 0 &&
                    baseMerchantBuyCommandIndex_ < static_cast<int>(merchantStock_.size()) &&
                    canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(baseMerchantBuyCommandIndex_)]);
                const std::array<UiCommandMenuItem, 1> buyItems{{{"買う", buyCommandEnabled}}};
                drawUiCommandMenu(renderer, baseMerchantBuyCommandMenu_, buyItems.data(), static_cast<int>(buyItems.size()));
            } else {
                const bool stackCommand = baseMerchantSellCommandIndex_ >= 0 && inventory_.screenObjectStackAt(baseMerchantSellCommandIndex_) != nullptr;
                const std::array<UiCommandMenuItem, 2> sellItems{{{stackCommand ? "1個売る" : "売る", true}, {"すべて売る", stackCommand}}};
                drawUiCommandMenu(renderer, baseMerchantSellCommandMenu_, sellItems.data(), stackCommand ? 2 : 1);
            }
            if (!baseStatus_.empty()) {
                renderer.drawText({80.0f, 626.0f}, baseStatus_, {255, 230, 150, 255}, 2);
            }
        }
    } else if (baseUpgradeActive_) {
        renderer.drawText(panel.pos + Vec2{152.0f, 118.0f}, "拠点強化炉", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{54.0f, 146.0f}, "拠点拡張・機能解禁", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            if (!upgradeImplemented(i)) {
                std::snprintf(buffer, sizeof(buffer), "%s  未実装", upgradeName(i));
            } else if (upgradeMaxed(i)) {
                std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  上限", upgradeName(i), upgradeLevel(i), upgradeMaxLevel(i));
            } else {
                const MaterialType materialType = upgradeMaterialType(i);
                std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  %dG %s%d",
                    upgradeName(i),
                    upgradeLevel(i),
                    upgradeMaxLevel(i),
                    upgradeCost(i),
                    std::string(materialTypeDisplayName(materialType)).c_str(),
                    upgradeMaterialCost(i));
            }
            drawUiButton(renderer, baseUpgradeItemRect(i), buffer, i == baseUpgradeSelection_, uiActionButtonStyle());
        }
        std::snprintf(buffer, sizeof(buffer), "所持: %dG / 古木 %d / 魔力のしずく %d",
            money_,
            inventory_.materialCount(MaterialType::OldWoodBuildingMaterial),
            inventory_.materialCount(MaterialType::ManaDrop));
        renderer.drawText(panel.pos + Vec2{54.0f, 448.0f}, buffer, {198, 198, 206, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "効果: 倉庫%d枠 / 商人Lv.%d / 加工解禁Lv.%d / HP+%d 半径+%d%% 速度+%d%%",
            warehouseCapacity(),
            merchantUpgradeLevel_,
            processingUnlockLevel_,
            maxHpUpgradeLevel_ * 2,
            ringRadiusUpgradeLevel_ * 8,
            ringSpeedUpgradeLevel_ * 8);
        renderer.drawText(panel.pos + Vec2{54.0f, 474.0f}, buffer, {198, 198, 206, 255}, 2);
    } else if (baseMiningStartChoiceActive_) {
        renderer.drawText(panel.pos + Vec2{178.0f, 238.0f}, "採掘出口", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{116.0f, 278.0f}, "開始地点または最新ワープポイントから出発", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            const bool disabled = (i == 1 && unlockedWarpPointCount_ <= 0) || (i == 2 && !canRegenerateCurrentStage());
            drawUiButton(renderer, baseMiningStartChoiceRect(i), baseMiningStartChoiceName(i), i == baseMiningStartSelection_ && !disabled, uiActionButtonStyle());
            if (disabled) {
                const UiRect rect = baseMiningStartChoiceRect(i);
                renderer.fillRect(rect.pos, rect.size, {0, 0, 0, 110});
                renderer.drawText(rect.pos + Vec2{116.0f, 14.0f}, "未解放", {150, 150, 160, 255}, 2);
            }
        }
    } else {
        const char* promptName = nearest != nullptr ? nearest->displayName : "";
        renderer.fillRect({280.0f, 644.0f}, {720.0f, 34.0f}, ui::FooterFill);
        if (nearest != nullptr) {
            if (const char* specialPrompt = baseInteractionPrompt(*nearest)) {
                std::snprintf(buffer, sizeof(buffer), "%s", specialPrompt);
            } else {
                std::snprintf(buffer, sizeof(buffer), "Enter: %sを調べる / クリック: 近くの施設を調べる / Esc: メニュー", promptName);
            }
            renderer.drawText({300.0f, 652.0f}, buffer, {255, 230, 150, 255}, 2);
        } else {
            renderer.drawText({300.0f, 652.0f}, "Enter: 近くの施設を調べる  Esc: メニュー", {198, 198, 206, 255}, 2);
        }
        if (!baseStatus_.empty()) {
            renderer.fillRect({280.0f, 604.0f}, {720.0f, 30.0f}, {0, 0, 0, 160});
            renderer.drawText({300.0f, 612.0f}, baseStatus_, {255, 230, 150, 255}, 2);
        }
        return;
    }

    if (!baseStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{54.0f, 504.0f}, baseStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::renderWarpPoints(Renderer& renderer) const
{
    if (!warpPointsEnabled_) {
        return;
    }

    for (const WarpPoint& point : warpPoints_) {
        const Color core = point.discovered ? Color{92, 236, 210, 255} : Color{255, 208, 92, 255};
        const Color ring = point.discovered ? Color{170, 255, 238, 220} : Color{255, 232, 150, 220};
        renderer.drawCircle(point.position, point.discovered ? 34.0f : 24.0f, {150, 210, 255, 110});
        renderer.drawCircle(point.position, 20.0f, ring);
        if (!drawWorldIcon(renderer, WorldIconId::WarpPoint, point.position, {42.0f, 42.0f})) {
            renderer.fillCircle(point.position, 12.0f, core);
            renderer.drawLine(point.position + Vec2{-18.0f, 0.0f}, point.position + Vec2{18.0f, 0.0f}, ring);
            renderer.drawLine(point.position + Vec2{0.0f, -18.0f}, point.position + Vec2{0.0f, 18.0f}, ring);
        }
    }

    if (hasBossSpawnPoint_ && !bossSpawned_) {
        renderer.drawCircle(bossSpawnPoint_, BossSpawnTriggerRadius, {255, 98, 92, 150});
        renderer.drawCircle(bossSpawnPoint_, 18.0f, {255, 180, 80, 230});
        renderer.drawLine(bossSpawnPoint_ + Vec2{-22.0f, -22.0f}, bossSpawnPoint_ + Vec2{22.0f, 22.0f}, {255, 120, 90, 210});
        renderer.drawLine(bossSpawnPoint_ + Vec2{-22.0f, 22.0f}, bossSpawnPoint_ + Vec2{22.0f, -22.0f}, {255, 120, 90, 210});
    }
}

void Game::appendRewardNodeRenderEntries(
    std::vector<DepthRenderEntry>& entries,
    Renderer& renderer,
    const std::vector<LightSource>& extraLights) const
{
    const Color exposedReward{255, 222, 94, 255};
    const Color exposedMoney{246, 190, 64, 255};
    const Color sparkle{255, 242, 164, 230};

    for (const RewardNode& node : rewardNodes_) {
        if (node.collected) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
            continue;
        }
        if (node.visibility != PlacementVisibility::Exposed && node.visibility != PlacementVisibility::BuriedVisible) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            center.y,
            [&renderer, center, visibility = node.visibility, exposedReward, sparkle]() {
                if (visibility == PlacementVisibility::Exposed) {
                    renderer.fillCircle(center, 7.0f, exposedReward);
                    renderer.drawCircle(center, 12.0f, {255, 246, 180, 210});
                    renderer.drawLine(center + Vec2{-9.0f, 0.0f}, center + Vec2{9.0f, 0.0f}, {255, 250, 210, 220});
                    renderer.drawLine(center + Vec2{0.0f, -9.0f}, center + Vec2{0.0f, 9.0f}, {255, 250, 210, 220});
                } else if (visibility == PlacementVisibility::BuriedVisible) {
                    renderer.drawLine(center + Vec2{-8.0f, 0.0f}, center + Vec2{8.0f, 0.0f}, sparkle);
                    renderer.drawLine(center + Vec2{0.0f, -8.0f}, center + Vec2{0.0f, 8.0f}, sparkle);
                    renderer.fillCircle(center, 2.5f, {255, 255, 210, 240});
                }
            },
        });
    }

    for (const MoneyNode& node : moneyNodes_) {
        if (node.collected) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
            continue;
        }
        if (node.visibility != PlacementVisibility::Exposed && node.visibility != PlacementVisibility::BuriedVisible) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            center.y,
            [&renderer, center, visibility = node.visibility, amount = node.amount, exposedMoney, sparkle]() {
                if (visibility == PlacementVisibility::Exposed) {
                    if (!drawWorldIcon(renderer, moneyWorldIconForAmount(amount), center, {30.0f, 30.0f})) {
                        renderer.fillCircle(center, 5.5f, exposedMoney);
                        renderer.drawCircle(center, 8.5f, {255, 230, 120, 210});
                    }
                } else if (visibility == PlacementVisibility::BuriedVisible) {
                    renderer.drawLine(center + Vec2{-6.0f, -6.0f}, center + Vec2{6.0f, 6.0f}, sparkle);
                    renderer.drawLine(center + Vec2{-6.0f, 6.0f}, center + Vec2{6.0f, -6.0f}, sparkle);
                }
            },
        });
    }

    for (const MoonFragmentNode& node : moonFragmentNodes_) {
        if (node.collected) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
            continue;
        }
        if (node.visibility != PlacementVisibility::Exposed && node.visibility != PlacementVisibility::BuriedVisible) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            center.y,
            [&renderer, center, visibility = node.visibility]() {
                const Color moonFill{232, 224, 166, static_cast<unsigned char>(visibility == PlacementVisibility::Exposed ? 255 : 165)};
                const Color moonGlow{255, 250, 198, static_cast<unsigned char>(visibility == PlacementVisibility::Exposed ? 210 : 135)};
                if (visibility == PlacementVisibility::Exposed) {
                    if (drawWorldIcon(renderer, materialWorldIcon(MaterialType::MoonFragment), center, {30.0f, 30.0f})) {
                        renderer.drawCircle(center, 11.0f, moonGlow);
                    } else {
                        renderer.fillCircle(center, 5.5f, moonFill);
                        renderer.drawCircle(center, 9.0f, moonGlow);
                        renderer.drawLine(center + Vec2{-7.0f, 0.0f}, center + Vec2{7.0f, 0.0f}, moonGlow);
                    }
                } else if (visibility == PlacementVisibility::BuriedVisible) {
                    renderer.drawCircle(center, 7.0f, moonGlow);
                    renderer.fillCircle(center, 2.0f, moonFill);
                    renderer.drawLine(center + Vec2{-5.0f, -5.0f}, center + Vec2{5.0f, 5.0f}, moonGlow);
                    renderer.drawLine(center + Vec2{-5.0f, 5.0f}, center + Vec2{5.0f, -5.0f}, moonGlow);
                }
            },
        });
    }

    for (const CrateNode& node : crateNodes_) {
        if (node.destroyed) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            center.y,
            [&renderer, center]() {
                if (!drawWorldIcon(renderer, WorldIconId::Crate, center, {38.0f, 38.0f})) {
                    renderer.fillRect(center + Vec2{-9.0f, -8.0f}, {18.0f, 16.0f}, {132, 88, 48, 255});
                    renderer.drawRect(center + Vec2{-9.0f, -8.0f}, {18.0f, 16.0f}, {218, 160, 92, 255});
                    renderer.drawLine(center + Vec2{-7.0f, -6.0f}, center + Vec2{7.0f, 6.0f}, {92, 58, 34, 230});
                    renderer.drawLine(center + Vec2{7.0f, -6.0f}, center + Vec2{-7.0f, 6.0f}, {92, 58, 34, 230});
                }
            },
        });
    }

    for (const ChestNode& node : chestNodes_) {
        if (!node.revealed && !node.opened) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
            continue;
        }
        if (node.visibility == PlacementVisibility::BuriedVisible && !node.opened) {
            entries.push_back(DepthRenderEntry{
                center.y,
                [&renderer, center, chestKind = node.chestKind]() {
                    WorldIconDrawOptions options;
                    options.tint = {255, 255, 255, 165};
                    if (!drawWorldIcon(renderer, chestWorldIcon(chestKind, false), center, {30.0f, 30.0f}, options)) {
                        const Color outline = chestOutlineColor(chestKind, false);
                        renderer.drawLine(center + Vec2{-9.0f, -4.0f}, center + Vec2{9.0f, -4.0f}, outline);
                        renderer.drawLine(center + Vec2{-9.0f, 4.0f}, center + Vec2{9.0f, 4.0f}, outline);
                        renderer.fillCircle(center, 2.5f, outline);
                    }
                },
            });
            continue;
        }
        if (node.visibility != PlacementVisibility::Exposed && !node.opened) {
            continue;
        }

        entries.push_back(DepthRenderEntry{
            center.y,
            [&renderer,
                center,
                chestKind = node.chestKind,
                visualOpened = chestVisualOpened(node.opened, node.lootSpawned, node.openingSeconds),
                openingScale = chestOpeningScale(node.openingSeconds)]() {
                if (chestKind == LootChestKind::SuperRare && !visualOpened) {
                    renderer.drawCircle(center, 20.0f, {255, 242, 164, 170});
                }
                WorldIconDrawOptions options;
                options.sizeMultiplier = openingScale;
                if (!drawWorldIcon(renderer, chestWorldIcon(chestKind, visualOpened), center, {42.0f, 42.0f}, options)) {
                    const Color fill = chestFillColor(chestKind, visualOpened);
                    const Color outline = chestOutlineColor(chestKind, visualOpened);
                    const Vec2 bodySize{20.0f * openingScale.x, 13.0f * openingScale.y};
                    const Vec2 bodyPos = center - bodySize * 0.5f;
                    renderer.fillRect(bodyPos, bodySize, fill);
                    renderer.drawRect(bodyPos, bodySize, outline);
                    renderer.drawLine(
                        center + Vec2{-8.0f * openingScale.x, -2.0f * openingScale.y},
                        center + Vec2{8.0f * openingScale.x, -2.0f * openingScale.y},
                        outline);
                    renderer.drawLine(
                        center + Vec2{0.0f, -6.0f * openingScale.y},
                        center + Vec2{0.0f, 7.0f * openingScale.y},
                        outline);
                }
            },
        });
    }
}

void Game::renderRewardNodes(Renderer& renderer, const std::vector<LightSource>& extraLights) const
{
    std::vector<DepthRenderEntry> entries;
    appendRewardNodeRenderEntries(entries, renderer, extraLights);
    std::stable_sort(entries.begin(), entries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : entries) {
        entry.draw();
    }
}

void Game::renderRingScreen(Renderer& renderer, float totalTime) const
{
    if (mode_ != ScreenMode::Ring) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = ringPanelRect();
    UiCancelControlScope cancelScope(ringCancelState_);
    constexpr std::string_view RingHelpText = UnlockedRingCount > 1
        ? "Z/X・1-3 リング選択  Q/E アイテム選択  A/D・←/→ 位置  F/Enter 詳細  R 外す  G つかむ/置く  P 保護  Esc/右クリック 戻る"
        : "Q/E アイテム選択  A/D・←/→ 位置  F/Enter 詳細  R 外す  G つかむ/置く  P 保護  Esc/右クリック 戻る";
    UiWindowScope ringWindow(renderer, "ring.manage", panel, "リング", RingHelpText, UiWindowOptions{true, true});

    char buffer[192];
    for (int i = 0; i < UnlockedRingCount; ++i) {
        std::snprintf(buffer, sizeof(buffer), "リング %d", i + 1);
        drawUiButton(renderer, ringTabRect(i), buffer, i == spellRing_.activeRingIndex());
    }

    const bool actualRing = true;
    const auto& items = spellRing_.items();
    (void)actualRing;
    const RingShape activeShape = spellRing_.activeRingShape();

    std::snprintf(buffer, sizeof(buffer), "形状 %s   装着 %02d/%02d   半径 %.0f   速度 %.2f   重量 %.1f/%.1fkg   重量補正 %.2f",
        ringShapeDisplayName(activeShape),
        static_cast<int>(items.size()),
        spellRing_.maxItemCount(),
        spellRing_.radius(),
        spellRing_.effectiveAngularSpeed(),
        spellRing_.totalEquippedWeight(),
        spellRing_.maxEquippedWeight(),
        spellRing_.weightSpeedMultiplier());
    renderer.drawText(panel.pos + Vec2{48.0f, 520.0f}, buffer, {202, 206, 216, 255}, 2);

    renderer.drawText(panel.pos + Vec2{48.0f, 160.0f}, "リング配置", {246, 248, 255, 255}, 3);
    const Vec2 orbitCenter = ringUiOrbitCenter(spellRing_);
    std::vector<Vec2> orbitPath = getRingPathSamplePoints(
        spellRing_.center(),
        ringUiOrbitContext(spellRing_, balance_, 0, 1),
        160);
    for (Vec2& point : orbitPath) {
        point = applyRingUiShapeRotation(spellRing_, ringWorldToUi(spellRing_, point));
    }
    drawMagicOrbitPath(
        renderer,
        orbitPath,
        orbitCenter,
        MagicOrbitDrawOptions{
            activeShape,
            true,
            false,
            true,
            true,
            spellRing_.activeRingIndex(),
            totalTime,
            1.0f,
        });
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const SpellRingItem& item = items[static_cast<std::size_t>(i)];
        float displayAngle = item.localAngle;
        if (i == ringDragItemIndex_ && (ringDragActive_ || ringSnapActive_)) {
            displayAngle = ringDragDisplayAngle_;
        }
        const Vec2 itemAnchor = ringItemUiCenterAtAngle(
            displayAngle,
            spellRing_,
            balance_,
            i,
            static_cast<int>(items.size()));
        SpellRingItem displayItem = item;
        displayItem.worldPosition = itemAnchor;
        const Vec2 itemCenter = ringItemDrawPosition(displayItem, totalTime);
        const Vec2 outward = normalize(itemAnchor - orbitCenter);
        const bool selected = i == ringSlotSelection_;
        const bool invalidDragPosition = selected && ringDragActive_ && !spellRing_.canPlaceItemAtAngle(i, displayAngle);
        const ItemData* object = objectForRingItem(objectCatalog_, item);
        if (activeShape != RingShape::FigureEight) {
            const Color angleLineColor = selected ? Color{255, 230, 150, 120} : Color{94, 102, 128, 85};
            const Vec2 radial = itemAnchor - orbitCenter;
            Vec2 tangent = normalize(Vec2{-radial.y, radial.x});
            if (lengthSquared(tangent) <= 0.0001f) {
                tangent = {0.0f, 1.0f};
            }
            constexpr float AngleLineHalfWidthPx = 0.5f;
            renderer.drawLine(orbitCenter + tangent * AngleLineHalfWidthPx, itemAnchor + tangent * AngleLineHalfWidthPx, angleLineColor);
            renderer.drawLine(orbitCenter - tangent * AngleLineHalfWidthPx, itemAnchor - tangent * AngleLineHalfWidthPx, angleLineColor);
        }
        drawRingItemShape(renderer, item, object, itemCenter, outward, selected, invalidDragPosition);
        std::snprintf(buffer, sizeof(buffer), "%d", i + 1);
        renderer.drawText(itemCenter + Vec2{-5.0f, 22.0f}, buffer, selected ? Color{255, 230, 150, 255} : Color{174, 182, 198, 255}, 1);
    }

    std::string ringDetailTitle = "空き";
    if (ringSlotSelection_ < static_cast<int>(items.size())) {
        ringDetailTitle = ringItemDisplayName(objectCatalog_, items[ringSlotSelection_]);
    }

    const UiRect ringDetailPanel = ringDetailRect();
    drawUiSubPanel(renderer, ringDetailPanel);
    float detailLineY = drawUiDetailHeader(renderer, ringDetailPanel, ringDetailTitle);

    if (ringSlotSelection_ < static_cast<int>(items.size())) {
        const SpellRingItem& item = items[ringSlotSelection_];
        const ItemData* object = objectForRingItem(objectCatalog_, item);
        const std::vector<std::string> effectLines = object != nullptr
            ? encyclopedia_.getObjectEffectDisplayLines(object->id, objectCatalog_, EffectRevealMode::WithUnknown)
            : std::vector<std::string>{};
        const std::string effectText = joinEffectLines(effectLines);
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, object != nullptr && !object->description.empty() ? object->description : "-");
        std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(effectLines.size()));
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "効果数", buffer);
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "効果");
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, effectText);
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "形状", ringShapeDisplayName(activeShape));
        if (item.durability < 0) {
            std::snprintf(buffer, sizeof(buffer), "壊れない");
        } else {
            std::snprintf(buffer, sizeof(buffer), "%d/%d", item.durability, item.maxDurability);
        }
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "耐久力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", item.weight);
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "重さ", buffer);
    } else {
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "アイテム未配置");
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "-");
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "効果数", "0");
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "効果");
        drawUiDetailText(renderer, ringDetailPanel, detailLineY, "-");
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "形状", ringShapeDisplayName(activeShape));
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "耐久力", "-");
        drawUiDetailLine(renderer, ringDetailPanel, detailLineY, "重さ", "-");
    }

    if (ringGrabActive_) {
        const std::string grabbedName = ringItemDisplayName(objectCatalog_, ringGrabbedItem_);
        std::snprintf(buffer, sizeof(buffer), "つかみ中: %s   G または F で置く / Esc でキャンセル", grabbedName.c_str());
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, buffer, {255, 230, 150, 255}, 2);
    } else if (!ringStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, ringStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::update(const Input& input, const Time& time)
{
    checkHotReload();
    reloadNoticeTimer_ = std::max(0.0f, reloadNoticeTimer_ - time.deltaSeconds());
    encyclopedia_.update(time.deltaSeconds());
    updateDungeonLogs(time.deltaSeconds());

    if (input.debugPressed()) {
        debug_.toggle();
    }
    if (!debug_.visible()) {
        debugPaused_ = false;
    } else if (input.debugPausePressed()) {
        debugPaused_ = !debugPaused_;
    }
    if (debugPaused_) {
        return;
    }
    const bool transitionWasActive = screenTransition_.active();
    updateScreenTransition(time.deltaSeconds());
    if (transitionWasActive || screenTransition_.active()) {
        return;
    }
    if (worldBuildActive()) {
        updateWorldBuild(time.deltaSeconds());
        return;
    }
    captureCooldown_ = std::max(0.0f, captureCooldown_ - time.deltaSeconds());

    std::vector<EffectDiscoveryEvent> effectDiscoveries;
    UiContext ui(input);
    const bool wasPaused = gameProgressPaused();
    updateScreenMode(input, ui, time.deltaSeconds(), &effectDiscoveries);
    for (const RingEquipFxRequest& request : inventory_.consumeRingEquipFxRequests()) {
        spawnRingEquipFx(request);
    }
    updateRingEquipFx(time.deltaSeconds());
    refreshOrbitEffects();
    const bool paused = gameProgressPaused() || (wasPaused && mode_ == ScreenMode::Playing);
    if (paused && !effectDiscoveries.empty()) {
        applyEffectDiscoveries(effectDiscoveries);
    }

    if (!paused) {
        runStats_.elapsedSeconds += time.deltaSeconds();
        updatePlayerFootstepDust(time.deltaSeconds());
        tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_, dungeonLayout_);
        std::vector<CollisionRect> objectBlockers;
        if (!enemyTestActive_) {
            objectBlockers = solidObjectCollisionRects();
        }
        player_.update(
            input,
            camera_,
            tileMap_,
            time.deltaSeconds(),
            false,
            balance_,
            std::span<const CollisionRect>{objectBlockers.data(), objectBlockers.size()});
        maybeSpawnPlayerFootstepDust(
            player_.position,
            lengthSquared(player_.velocity) > 0.0001f ? player_.velocity : player_.facing,
            player_.spriteWalking,
            player_.spriteFrameIndex(),
            previousPlayerDustFrame_);
        if (player_.hp <= 0) {
            enterGameOver();
            refreshOrbitEffects();
            return;
        }
        if (!enemyTestActive_) {
            updateWarpPoints(time.deltaSeconds());
            updateExposedRewardNodes();
            updateExposedMoonFragmentNodes();
            updateExposedEnemyNodes();
        }
        updateRingEffectDiscoveries(effectDiscoveries);
        if (input.capturePressed() && captureCooldown_ <= 0.0f) {
            const CaptureResult capture = enemies_.tryCapture(player_, spellRing_, inventory_);
            captureCooldown_ = capture.type == CaptureResultType::Success ? 0.35f : 0.75f;
            if (capture.type == CaptureResultType::Success) {
                reloadNotice_ = "捕獲: " + capture.enemyName;
                reloadNoticeTimer_ = 1.6f;
                effects_.spawnCaptureSuccess(capture.position, player_.position - capture.position);
            } else if (capture.type == CaptureResultType::InventoryFull) {
                reloadNotice_ = "捕獲失敗: インベントリ満杯";
                reloadNoticeTimer_ = 1.6f;
            } else if (capture.type == CaptureResultType::Failed) {
                reloadNotice_ = "捕獲失敗";
                reloadNoticeTimer_ = 1.2f;
            }
        }
        camera_.follow(player_.position, time.deltaSeconds());

        const SpellRingState previousSpellRingState = spellRing_.state();
        const Vec2 previousRingCenter = spellRing_.center();
        spellRing_.update(player_, input, time.deltaSeconds(), time.totalSeconds(), false, ui.pointerConsumed(), balance_);
        if (previousSpellRingState == SpellRingState::Normal && spellRing_.state() == SpellRingState::Thrown) {
            effects_.spawnThrowStart(previousRingCenter, player_.facing);
        } else if (previousSpellRingState == SpellRingState::Returning && spellRing_.state() == SpellRingState::Normal) {
            effects_.spawnReturn(spellRing_.center());
        }
        if (!enemyTestActive_) {
            updateChestNodes(time.deltaSeconds(), input);
            updateCrateNodes();
        }

        tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_, dungeonLayout_);
        digging_.update(
            tileMap_,
            spellRing_,
            player_,
            time.totalSeconds(),
            objectCatalog_,
            effectDispatcher_,
            &effectDiscoveries,
            &encyclopedia_);
        for (const TerrainHitTile& tile : digging_.hitTiles()) {
            effects_.spawnDigHit(tile.center, tile.center - spellRing_.center(), tile.color);
        }
        if (digging_.dugTiles().empty()) {
            for (Vec2 tile : digging_.openedTiles()) {
                effects_.spawnTileBreak(tile, TileType::Dirt, tileMap_.tileColorAtWorld(tile));
            }
        }
        if (!enemyTestActive_) {
            revealRewardNodesFromOpenedTiles(digging_.openedTiles());
            revealMoonFragmentNodesFromOpenedTiles(digging_.openedTiles());
            revealChestNodesFromOpenedTiles(digging_.openedTiles());
        }
        for (const DugTile& tile : digging_.dugTiles()) {
            effects_.spawnTileBreak(tile.center, tile.type, tile.color);
            ++runStats_.dugTiles;

            std::mt19937& rng = lootRuntimeRng();
            if (digEventDue(
                    runStats_.dugTilesSinceMoneyDrop,
                    balance_.digMoneyMinDugTiles,
                    balance_.digMoneyGuaranteeDugTiles,
                    rng)) {
                const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, {
                    static_cast<float>(tileMap_.worldToTile(tile.center.x)),
                    static_cast<float>(tileMap_.worldToTile(tile.center.y)),
                });
                const int depthRank = lootDepthRankForProgress(currentStageId_, metrics.pathProgress);
                const float multiplier =
                    lootStageMultiplier(balance_, currentStageId_) *
                    lootDepthMultiplier(balance_, currentStageId_, depthRank);
                std::uniform_int_distribution<int> moneyDistribution(2, 6);
                const int amount = scaledLootAmount(moneyDistribution(rng), multiplier);
                if (worldDrops_.spawnMoneyDrop(amount, scatterLootPosition(tile.center, rng), runStats_.elapsedSeconds)) {
                    runStats_.dugTilesSinceMoneyDrop = 0;
                }
            }

            if (digEventDue(
                    runStats_.dugTilesSinceItemDrop,
                    balance_.digItemMinDugTiles,
                    balance_.digItemGuaranteeDugTiles,
                    rng)) {
                if (worldDrops_.spawnDigItemDrop(objectCatalog_, scatterLootPosition(tile.center, rng), runStats_.elapsedSeconds)) {
                    runStats_.dugTilesSinceItemDrop = 0;
                }
            }
        }
        for (Vec2 rewardPosition : digging_.rewardDropRequests()) {
            worldDrops_.spawnRewardDrop(objectCatalog_, rewardPosition, runStats_.elapsedSeconds);
        }
        for (const DugTile& tile : digging_.dugTiles()) {
            if (tile.type != TileType::Ore) {
                continue;
            }
            const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, {
                static_cast<float>(tileMap_.worldToTile(tile.center.x)),
                static_cast<float>(tileMap_.worldToTile(tile.center.y)),
            });
            const int depthRank = lootDepthRankForProgress(currentStageId_, metrics.pathProgress);
            const float multiplier =
                lootStageMultiplier(balance_, currentStageId_) *
                lootDepthMultiplier(balance_, currentStageId_, depthRank);
            std::uniform_int_distribution<int> oreAmountDistribution(balance_.oreMaterialMin, balance_.oreMaterialMax);
            const int amount = scaledLootAmount(oreAmountDistribution(lootRuntimeRng()), multiplier);
            worldDrops_.spawnMaterialDrop(MaterialType::EnhancementOre, amount, tile.center, runStats_.elapsedSeconds);
        }
        std::vector<WorldDropPickupEvent> pickupEvents;
        runStats_.acquiredItems += worldDrops_.update(
            time.deltaSeconds(),
            player_,
            inventory_,
            money_,
            objectCatalog_,
            &effects_,
            &pickupEvents);
        appendPickupLogs(pickupEvents);
        if (!enemyTestActive_) {
            const std::vector<Vec2> randomEnemySpawnTiles = spawnHiddenEnemyNodesFromOpenedTiles(digging_.openedTiles());
            enemies_.spawnFromDugTiles(randomEnemySpawnTiles, tileMap_, player_.position, balance_, enemyCatalog_);
            updateBossSpawn();
        }

        enemies_.update(
            player_,
            spellRing_,
            tileMap_,
            time.deltaSeconds(),
            time.totalSeconds(),
            false,
            balance_,
            objectCatalog_,
            worldDrops_,
            player_.position,
            std::vector<LightSource>{},
            effectDispatcher_,
            projectiles_,
            &effectDiscoveries,
            &encyclopedia_);
        for (Vec2 explosionPosition : digging_.capturedExplosionRequests()) {
            handleCapturedExplosion(explosionPosition);
        }
        updateCapturedUtilityBehaviors(time.deltaSeconds());
        updateCapturedProjectileBehaviors(time.deltaSeconds());
        projectiles_.update(
            player_,
            spellRing_,
            enemies_,
            tileMap_,
            time.deltaSeconds(),
            effectDispatcher_,
            objectCatalog_,
            &effectDiscoveries,
            &encyclopedia_);

        std::vector<Vec2> capturedExplosionPositions;
        for (const EnemyEvent& event : enemies_.events()) {
            if (!event.enemyId.empty()) {
                encyclopedia_.noteEnemyDiscovered(event.enemyId, event.enemyName, event.position);
            }
            if (event.type == EnemyEventType::CapturedExplosion) {
                capturedExplosionPositions.push_back(event.position);
            }
        }
        for (Vec2 explosionPosition : capturedExplosionPositions) {
            handleCapturedExplosion(explosionPosition);
        }

        bool bossDefeated = false;
        for (const EnemyEvent& event : enemies_.events()) {
            if (event.type == EnemyEventType::Death || event.type == EnemyEventType::BossDeath) {
                ++runStats_.defeatedEnemies;
                effects_.spawnEnemyDeath(event.position);
                if (event.moneyDrop > 0) {
                    worldDrops_.spawnMoneyDrop(event.moneyDrop, event.position, runStats_.elapsedSeconds);
                }
                std::mt19937& rng = lootRuntimeRng();
                const bool bossDeath = event.type == EnemyEventType::BossDeath;
                const float manaChance = bossDeath ? balance_.bossManaDropChance : balance_.enemyManaDropChance;
                const float moonChance = bossDeath ? balance_.bossMoonFragmentChance : balance_.enemyMoonFragmentChance;
                if (rollChance(manaChance, rng)) {
                    const int amount = bossDeath ? scaledLootAmount(std::uniform_int_distribution<int>(1, 3)(rng), 1.0f) : 1;
                    worldDrops_.spawnMaterialDrop(MaterialType::ManaDrop, amount, scatterLootPosition(event.position, rng), runStats_.elapsedSeconds);
                }
                if (rollChance(moonChance, rng)) {
                    const int amount = bossDeath ? scaledLootAmount(std::uniform_int_distribution<int>(1, 3)(rng), 1.0f) : 1;
                    worldDrops_.spawnMaterialDrop(MaterialType::MoonFragment, amount, scatterLootPosition(event.position, rng), runStats_.elapsedSeconds);
                }
                if (!event.enemyId.empty()) {
                    encyclopedia_.noteEnemyDefeated(event.enemyId, event.enemyName, event.position);
                }
                if (event.type == EnemyEventType::BossDeath) {
                    bossDefeated = true;
                }
            } else if (event.type == EnemyEventType::RewardDrop) {
                worldDrops_.spawnRewardDrop(objectCatalog_, event.position, runStats_.elapsedSeconds);
            } else if (event.type == EnemyEventType::ObjectDrop) {
                const int dropCount = std::max(1, event.objectDropCount);
                for (int i = 0; i < dropCount; ++i) {
                    worldDrops_.spawnObjectDrop(objectCatalog_, event.objectDropId, event.position, runStats_.elapsedSeconds);
                }
            } else if (event.type == EnemyEventType::CapturedExplosion) {
                continue;
            } else if (event.type == EnemyEventType::AttackHit) {
                effects_.spawnEnemyHit(event.position, event.effectId);
                if (event.damageAmount >= 0) {
                    effects_.spawnDamagePopup(event.position, event.damageAmount, DamagePopupStyle::Enemy);
                }
            } else if (event.damageAmount >= 0) {
                effects_.spawnDamagePopup(event.position, event.damageAmount, DamagePopupStyle::Enemy);
            }
        }
        for (const PlayerDamageEvent& event : player_.damageEvents) {
            effects_.spawnDamagePopup(event.position, event.amount, DamagePopupStyle::Player);
        }
        player_.damageEvents.clear();
        applyEffectDiscoveries(effectDiscoveries);
        syncEncyclopediaFromInventoryAndRing();
        updateAmbientParticleEffects(time.deltaSeconds());
        effects_.update(time.deltaSeconds());
        levels_.addXp(player_, enemies_.consumePendingXp(), balance_);
        if (bossDefeated) {
            enterStageClear();
            return;
        }
        if (player_.hp <= 0) {
            enterGameOver();
            return;
        }
        if (levels_.isChoosing()) {
            mode_ = ScreenMode::LevelUp;
        }
    }
}

void Game::checkHotReload()
{
    std::string changedPath;
    if (!watcher_.poll(changedPath)) {
        return;
    }

    std::string message;
    bool reloaded = false;
    const std::string fileName = filenameOf(changedPath);

    if (fileName == "google_sheet_source.cfg") {
        loadSheetSourceConfig();
        reloaded = loadBalanceFromSources(message);
    } else if (fileName == "game_balance.cfg") {
        reloaded = loadBalanceFromDisk(message);
    } else if (fileName == "opening_kamishibai.tsv") {
        loadOpeningKamishibaiData();
        if (mode_ == ScreenMode::OpeningKamishibai) {
            openingPlayer_.start(openingPages_, openingMeta_.openingEverWatched);
        }
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        configureWatcher();
        return;
    } else {
        reloaded = loadBalanceFromSources(message);
    }

    if (reloaded) {
        player_.xpToNext = std::max(1, balance_.xpBase + player_.level * balance_.xpPerLevel);
        worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
        applyPermanentUpgrades();
        refreshOrbitEffects();
        tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
        configureWatcher();
        reloadNotice_ = "Hot reload: " + changedPath;
    } else {
        reloadNotice_ = message;
    }
    reloadNoticeTimer_ = 3.0f;
}

void Game::renderPauseMenu(Renderer& renderer) const
{
    if (mode_ != ScreenMode::PauseMenu) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = pausePanelRect();
    UiCancelControlScope cancelScope(pauseCancelState_);
    const char* pauseTitle = pausePage_ == PauseMenuPage::Status ? "ステータス" : "PAUSED";
    const char* pauseHelp = pausePage_ == PauseMenuPage::Status
        ? "Esc/右クリック 戻る"
        : (pausePage_ == PauseMenuPage::Ring ? "Z/X でアクティブリング切替  Esc/右クリック 戻る" : "F/Enter 決定  Esc/右クリック 戻る");
    UiWindowScope pauseWindow(
        renderer,
        "pause.main",
        panel,
        pauseTitle,
        pauseHelp,
        UiWindowOptions{true, pausePage_ != PauseMenuPage::QuitConfirm});

    char buffer[160];
    if (pausePage_ == PauseMenuPage::Main) {
        for (int i = 0; i < PauseMenuItemCount; ++i) {
            const bool selected = i == pauseMenuSelection_;
            drawUiButton(renderer, pauseMenuItemRect(i), pauseMenuItemName(i), selected);
        }
        return;
    }

    if (pausePage_ == PauseMenuPage::Status) {
        const Vec2 labelPos = panel.pos + Vec2{74.0f, 110.0f};
        const Vec2 valuePos = panel.pos + Vec2{236.0f, 110.0f};
        constexpr float RowHeight = 32.0f;
        const auto drawStatusRow = [&](int row, std::string_view label, std::string_view value) {
            const float y = static_cast<float>(row) * RowHeight;
            renderer.drawText(labelPos + Vec2{0.0f, y}, label, ui::TextMuted, 2);
            renderer.drawText(valuePos + Vec2{0.0f, y}, value, {230, 230, 236, 255}, 2);
        };

        std::snprintf(buffer, sizeof(buffer), "%02d", player_.hp);
        drawStatusRow(0, "HP", buffer);
        std::snprintf(buffer, sizeof(buffer), "%02d", player_.level);
        drawStatusRow(1, "Level", buffer);
        std::snprintf(buffer, sizeof(buffer), "%02d/%02d", player_.xp, player_.xpToNext);
        drawStatusRow(2, "XP", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d", spellRing_.activeRingIndex() + 1);
        drawStatusRow(3, "Ring", buffer);
        std::snprintf(buffer, sizeof(buffer), "%02d/%02d", static_cast<int>(spellRing_.items().size()), spellRing_.maxItemCount());
        drawStatusRow(4, "Items", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.0f", spellRing_.radius());
        drawStatusRow(5, "Radius", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.1f", spellRing_.effectiveAngularSpeed());
        drawStatusRow(6, "Speed", buffer);
    } else if (pausePage_ == PauseMenuPage::Items) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "アイテム", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, "通常画面のショートカットHUDとアイテム画面で管理します。", {230, 230, 236, 255}, 2);
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, "I キーでアイテム画面を開けます。", {198, 198, 206, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Ring) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "リング", {246, 235, 255, 255}, 3);
        std::snprintf(buffer, sizeof(buffer), "アクティブリング %d", spellRing_.activeRingIndex() + 1);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, buffer, {230, 230, 236, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "装着アイテム %02d/%02d", static_cast<int>(spellRing_.items().size()), spellRing_.maxItemCount());
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Options) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "オプション", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, "仮画面です。設定項目は後続実装で追加します。", {230, 230, 236, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::QuitConfirm) {
        const UiRect confirm = quitConfirmRect();
        UiWindowScope confirmWindow(renderer, "pause.quit_confirm", confirm, "確認", "Esc/右クリック 戻る", UiWindowOptions{true, true});
        renderer.drawText(confirm.pos + Vec2{58.0f, 82.0f}, "ゲームを終了しますか？", ui::Text, 3);
        renderer.drawText(confirm.pos + Vec2{62.0f, 126.0f}, "セーブは拠点でのみ実行できます。", ui::TextMuted, 2);
        drawUiButton(renderer, quitConfirmButtonRect(1), "終了する", true, uiActionButtonStyle());
        return;
    }
}

void Game::renderGameOverScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::GameOver) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {0, 0, 0, 150});
    const UiRect panel = gameOverPanelRect();
    UiWindowScope gameOverWindow(renderer, "game_over", panel, "GAME OVER", "F/Enter 決定");
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "リザルト", ui::Text, 3);

    char buffer[160];
    const std::string deathCause = playerDeathCauseText(player_);
    std::snprintf(buffer, sizeof(buffer), "死因      %s", deathCause.c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, 130.0f}, buffer, {255, 214, 220, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "到達時間  %s", formatRunTime(runStats_.elapsedSeconds).c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, 168.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "レベル    %d", player_.level);
    renderer.drawText(panel.pos + Vec2{136.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "撃破数    %d", runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 244.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "掘削数    %d", runStats_.dugTiles);
    renderer.drawText(panel.pos + Vec2{136.0f, 282.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "入手アイテム数  %d", runStats_.acquiredItems);
    renderer.drawText(panel.pos + Vec2{136.0f, 320.0f}, buffer, {230, 230, 236, 255}, 2);

    for (int i = 0; i < GameOverItemCount; ++i) {
        drawUiButton(renderer, gameOverItemRect(i), gameOverItemName(i), i == gameOverSelection_, i == 0 ? uiActionButtonStyle() : uiCancelButtonStyle());
    }
    if (!gameOverStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{152.0f, 474.0f}, gameOverStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::renderStageClearScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::StageClear) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {0, 0, 0, 130});
    const UiRect panel = stageClearPanelRect();
    UiWindowScope stageClearWindow(renderer, "stage_clear", panel, "STAGE CLEAR", "F/Enter 決定");
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "シナリオ", ui::Text, 3);

    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "深部の主を退け、落ちた星のかけらが静かに光を取り戻した。");
    renderer.drawText(panel.pos + Vec2{136.0f, 148.0f}, buffer, {230, 238, 232, 255}, 2);
    renderer.drawText(panel.pos + Vec2{136.0f, 186.0f}, "村の魔女たちは、さらに深い層へ続く道を見つける。", {230, 238, 232, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "次ステージ解禁: Stage %d", unlockedStages_);
    renderer.drawText(panel.pos + Vec2{136.0f, 238.0f}, buffer, {255, 230, 150, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "クリア時間 %s   掘削数 %d   撃破数 %d",
        formatRunTime(runStats_.elapsedSeconds).c_str(),
        runStats_.dugTiles,
        runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 292.0f}, buffer, {198, 208, 202, 255}, 2);

    for (int i = 0; i < StageClearItemCount; ++i) {
        drawUiButton(renderer, stageClearItemRect(i), stageClearItemName(i), i == stageClearSelection_, uiActionButtonStyle());
    }
    if (!stageClearStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{152.0f, 474.0f}, stageClearStatus_, {255, 230, 150, 255}, 2);
    }
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

void Game::renderDebugOverlay(Renderer& renderer, const Time& time)
{
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
        rewardNodeCount(),
        moneyNodeCount(),
        buriedVisibleNodeCount(),
        buriedHiddenNodeCount(),
        exposedEnemyNodeCount(),
        buriedEnemyNodeCount(),
        spawnedEnemyNodeCount(),
        autoReloadBlocked_);
}

void Game::renderSpellRingForeground(
    Renderer& renderer,
    const std::vector<const SpellRingItem*>& runtimeItems,
    const std::vector<LightSource>&,
    float totalSeconds) const
{
    drawSpellRingOrbitLayer(renderer, spellRing_, balance_, totalSeconds, 0.78f);

    if (spellRing_.state() != SpellRingState::Normal) {
        renderer.drawLine(player_.position, spellRing_.center(), {150, 110, 80, 100});
    }

    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        const SpellRingItem& item = *itemPtr;
        const int ringIndex = std::clamp(item.ringIndex, 0, SpellRingCount - 1);
        const RingShape ringShape = spellRing_.ringShapeForIndex(ringIndex);
        const int ringItemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
        const float cometVisualScale = ringShape == RingShape::Comet
            ? std::clamp(1.0f - std::max(0, ringItemCount - 10) * 0.014f, 0.76f, 1.0f)
            : 1.0f;
        const Vec2 drawPosition = ringItemDrawPosition(item, totalSeconds);
        const ItemData* object = objectForRingItem(objectCatalog_, item);
        if (item.type == SpellRingItemType::Shovel) {
            if (renderer.hasIconSheet()) {
                const float iconSize = IconDrawSize * cometVisualScale;
                renderer.drawIcon(ShovelIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize});
            } else {
                const bool drewImage = object != nullptr &&
                    drawObjectImage(
                        renderer,
                        *object,
                        drawPosition,
                        {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                if (!drewImage) {
                    renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {178, 184, 190, 255});
                    const Vec2 outward = normalize(item.worldPosition - spellRing_.center());
                    renderer.drawLine(drawPosition, drawPosition + outward * (15.0f * cometVisualScale), {90, 96, 102, 255});
                }
            }
        } else if (item.type == SpellRingItemType::Torch) {
            if (renderer.hasIconSheet()) {
                const float iconSize = IconDrawSize * cometVisualScale;
                renderer.drawIcon(TorchIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize});
            } else {
                const bool drewImage = object != nullptr &&
                    drawObjectImage(
                        renderer,
                        *object,
                        drawPosition,
                        {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                if (!drewImage) {
                    renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {242, 122, 25, 255});
                    renderer.fillCircle(drawPosition + Vec2{2.0f, -2.0f} * cometVisualScale, 4.0f * cometVisualScale, {255, 238, 98, 255});
                }
            }
        } else {
            const bool drewImage = object != nullptr &&
                drawObjectImage(
                    renderer,
                    *object,
                    drawPosition,
                    {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
            if (!drewImage) {
                renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {96, 122, 210, 255});
                renderer.drawCircle(drawPosition, item.hitRadius * cometVisualScale + 3.0f, {160, 202, 255, 255});
            }
        }
        if (item.hiddenDetectionRadius > 0.0f) {
            renderer.drawCircle(drawPosition, item.hiddenDetectionRadius, {126, 208, 255, 90});
        }
        if (item.treasureDetectionRadius > 0.0f) {
            renderer.drawCircle(drawPosition, item.treasureDetectionRadius, {255, 220, 92, 90});
        }
    }
}

void Game::render(Renderer& renderer, const Time& time)
{
    renderer.clear({5, 5, 8, 255});
    beginUiFrame(time.deltaSeconds());
    if (mode_ == ScreenMode::OpeningKamishibai) {
        renderOpeningKamishibai(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::Title) {
        renderTitleScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        renderObjectImageScaleEditScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (basePresentationActive()) {
        renderBaseScreen(renderer);
        inventory_.render(renderer, player_, spellRing_, objectCatalog_, encyclopedia_);
        renderPauseMenu(renderer);
        renderRingScreen(renderer, time.totalSeconds());
        dialogue_.render(renderer, camera_.width(), camera_.height());
        finishUiFrame(renderer);
        renderBaseDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderWorldLoadingScreen(renderer, time.totalSeconds());
        renderer.present();
        return;
    }

    renderer.setWorldSpace(&camera_);

    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    std::vector<LightSource> itemLights;
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        const float phase = itemPtr->localAngle * 1.7f + static_cast<float>(itemPtr->ringIndex) * 2.3f;
        if (itemPtr->lightRadius > 0.0f) {
            itemLights.push_back({
                flickeredLightPosition(itemPtr->worldPosition, time.totalSeconds(), phase),
                flickeredLightRadius(itemPtr->lightRadius, time.totalSeconds(), phase),
            });
        } else if (itemPtr->type == SpellRingItemType::Torch) {
            const float torchPhase = phase + 0.47f;
            itemLights.push_back({
                flickeredLightPosition(itemPtr->worldPosition, time.totalSeconds(), torchPhase),
                flickeredLightRadius(balance_.lightRadius, time.totalSeconds(), torchPhase),
            });
        }
    }
    if (warpPointsEnabled_) {
        for (const WarpPoint& point : warpPoints_) {
            float radiusTiles = point.discovered ? point.discoveredLightRadiusTiles : point.undiscoveredLightRadiusTiles;
            if (point.lightRevealAnimating && point.lightRevealDuration > 0.0f) {
                const float t = std::clamp(point.lightRevealTimer / point.lightRevealDuration, 0.0f, 1.0f);
                const float inv = 1.0f - t;
                const float eased = 1.0f - inv * inv * inv;
                radiusTiles = point.undiscoveredLightRadiusTiles +
                    (point.discoveredLightRadiusTiles - point.undiscoveredLightRadiusTiles) * eased;
            }
            const float radiusPx = radiusTiles * static_cast<float>(balance::TileSize);
            const float phase = static_cast<float>(point.index) * 1.73f;
            itemLights.push_back({
                flickeredLightPosition(point.position, time.totalSeconds(), phase),
                flickeredLightRadius(radiusPx, time.totalSeconds(), phase),
            });
        }
        if (hasBossSpawnPoint_ && !bossSpawned_) {
            itemLights.push_back({
                flickeredLightPosition(bossSpawnPoint_, time.totalSeconds(), 4.8f),
                flickeredLightRadius(120.0f, time.totalSeconds(), 4.8f),
            });
        }
    }
    tileMap_.render(renderer, camera_, player_.position, itemLights);
    std::vector<DepthRenderEntry> worldDepthEntries;
    if (!enemyTestActive_) {
        appendRewardNodeRenderEntries(worldDepthEntries, renderer, itemLights);
    }
    worldDrops_.appendRenderEntries(worldDepthEntries, renderer, tileMap_, objectCatalog_, player_.position, itemLights);
    if (!enemyTestActive_) {
        renderWarpPoints(renderer);
    }

    const bool ringCenterVisible = tileMap_.isLit(spellRing_.center(), player_.position, itemLights);
    if (ringCenterVisible) {
        drawSpellRingOrbitLayer(renderer, spellRing_, balance_, time.totalSeconds(), 0.46f);
    }
    if (spellRing_.state() != SpellRingState::Normal && ringCenterVisible) {
        renderer.drawLine(player_.position, spellRing_.center(), {150, 110, 80, 100});
    }

    const Vec2 playerFootAnchor = player_.position;
    renderer.drawActorShadow(playerFootAnchor, PlayerSpriteDrawSize);
    worldDrops_.renderShadows(renderer, tileMap_, objectCatalog_, player_.position, itemLights);
    enemies_.renderShadows(renderer, tileMap_, player_.position, itemLights);
    renderPlayerFootstepDust(renderer);
    worldDepthEntries.push_back(DepthRenderEntry{
        player_.position.y,
        [&]() {
            if (renderer.hasPlayerSheet()) {
                const int playerFrame = player_.spriteFrameIndex();
                const bool playerFlip = player_.facing.x > 0.0f;
                renderer.drawPlayerSprite(
                    playerFrame,
                    playerFootAnchor,
                    PlayerSpriteDrawSize,
                    playerFlip,
                    {255, 255, 255, 255},
                    {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
                if (player_.damageFlash > 0.0f) {
                    const float flash = clamp(player_.damageFlash / 0.16f, 0.0f, 1.0f);
                    const unsigned char alpha = static_cast<unsigned char>(std::round(185.0f * flash));
                    renderer.drawPlayerSprite(
                        playerFrame,
                        playerFootAnchor,
                        PlayerSpriteDrawSize,
                        playerFlip,
                        {255, 52, 52, alpha},
                        {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
                }
            } else {
                const Color playerColor = player_.damageFlash > 0.0f
                    ? Color{255, 72, 72, 255}
                    : Color{118, 72, 168, 255};
                renderer.fillCircle(player_.position, balance_.playerRadius, playerColor);
                renderer.drawLine(player_.position, player_.position + player_.facing * 22.0f, {235, 210, 255, 255});
            }

            for (const SpellRingItem* itemPtr : runtimeItems) {
                if (itemPtr == nullptr || !tileMap_.isLit(itemPtr->worldPosition, player_.position, itemLights)) {
                    continue;
                }
                const SpellRingItem& item = *itemPtr;
                const int ringIndex = std::clamp(item.ringIndex, 0, SpellRingCount - 1);
                const RingShape ringShape = spellRing_.ringShapeForIndex(ringIndex);
                const int ringItemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
                const float cometVisualScale = ringShape == RingShape::Comet
                    ? std::clamp(1.0f - std::max(0, ringItemCount - 10) * 0.014f, 0.76f, 1.0f)
                    : 1.0f;
                const Vec2 drawPosition = ringItemDrawPosition(item, time.totalSeconds());
                renderer.drawActorShadow(
                    actorShadowAnchor(item.worldPosition, ItemShadowGroundOffsetY),
                    ringItemShadowVisualSize(item, time.totalSeconds()) * cometVisualScale);
                const ItemData* object = objectForRingItem(objectCatalog_, item);
                if (item.type == SpellRingItemType::Shovel) {
                    if (renderer.hasIconSheet()) {
                        const float iconSize = IconDrawSize * cometVisualScale;
                        renderer.drawIcon(ShovelIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize});
                    } else {
                        const bool drewImage = object != nullptr &&
                            drawObjectImage(
                                renderer,
                                *object,
                                drawPosition,
                                {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                        if (!drewImage) {
                            renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {178, 184, 190, 255});
                            const Vec2 outward = normalize(item.worldPosition - spellRing_.center());
                            renderer.drawLine(drawPosition, drawPosition + outward * (15.0f * cometVisualScale), {90, 96, 102, 255});
                        }
                    }
                } else if (item.type == SpellRingItemType::Torch) {
                    if (renderer.hasIconSheet()) {
                        const float iconSize = IconDrawSize * cometVisualScale;
                        renderer.drawIcon(TorchIconIndex, drawPosition - Vec2{iconSize * 0.5f, iconSize * 0.5f}, {iconSize, iconSize});
                    } else {
                        const bool drewImage = object != nullptr &&
                            drawObjectImage(
                                renderer,
                                *object,
                                drawPosition,
                                {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                        if (!drewImage) {
                            renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {242, 122, 25, 255});
                            renderer.fillCircle(drawPosition + Vec2{2.0f, -2.0f} * cometVisualScale, 4.0f * cometVisualScale, {255, 238, 98, 255});
                        }
                    }
                } else {
                    const bool drewImage = object != nullptr &&
                        drawObjectImage(
                            renderer,
                            *object,
                            drawPosition,
                            {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale});
                    if (!drewImage) {
                        renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {96, 122, 210, 255});
                        renderer.drawCircle(drawPosition, item.hitRadius * cometVisualScale + 3.0f, {160, 202, 255, 255});
                    }
                }
                if (item.hiddenDetectionRadius > 0.0f) {
                    renderer.drawCircle(drawPosition, item.hiddenDetectionRadius, {126, 208, 255, 90});
                }
                if (item.treasureDetectionRadius > 0.0f) {
                    renderer.drawCircle(drawPosition, item.treasureDetectionRadius, {255, 220, 92, 90});
                }
            }
        },
    });
    enemies_.appendRenderEntries(worldDepthEntries, renderer, tileMap_, player_.position, itemLights);
    std::stable_sort(worldDepthEntries.begin(), worldDepthEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : worldDepthEntries) {
        entry.draw();
    }

    std::vector<DepthRenderEntry> projectileDepthEntries;
    projectiles_.appendRenderEntries(projectileDepthEntries, renderer, tileMap_, player_.position, itemLights);
    std::stable_sort(projectileDepthEntries.begin(), projectileDepthEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : projectileDepthEntries) {
        entry.draw();
    }
    effects_.render(renderer);
    tileMap_.renderDarknessOverlay(renderer, camera_, player_.position, itemLights);
    renderSpellRingForeground(renderer, runtimeItems, itemLights, time.totalSeconds());
    effects_.renderForeground(renderer);
    effects_.renderDamagePopups(renderer);

    renderer.setScreenSpace();
    renderTopInfoBar(renderer);
    if (mode_ == ScreenMode::Playing) {
        renderRingStatusHud(renderer);
        renderDungeonLogs(renderer);
        renderDungeonStatusHud(renderer);
    }
    if (reloadNoticeTimer_ > 0.0f) {
        renderer.fillRect({18.0f, 170.0f}, {430.0f, 26.0f}, {0, 0, 0, 180});
        renderer.drawText({26.0f, 176.0f}, reloadNotice_, {255, 235, 150, 255}, 2);
    }
    if (debugPaused_) {
        renderer.fillRect({18.0f, 202.0f}, {190.0f, 28.0f}, {0, 0, 0, 190});
        renderer.drawText({28.0f, 208.0f}, "DEBUG PAUSED", {255, 230, 150, 255}, 2);
    }
    upgrades_.render(renderer, levels_);
    inventory_.render(renderer, player_, spellRing_, objectCatalog_, encyclopedia_);
    if (mode_ == ScreenMode::Playing) {
        inventory_.renderShortcutHud(renderer, spellRing_, camera_.width(), camera_.height());
        renderRingEquipFx(renderer);
    }
    renderPauseMenu(renderer);
    renderRingScreen(renderer, time.totalSeconds());
    renderGameOverScreen(renderer);
    renderStageClearScreen(renderer);
    renderEnemyTestUi(renderer);
    if (mode_ == ScreenMode::Playing || mode_ == ScreenMode::Inventory || mode_ == ScreenMode::PauseMenu || mode_ == ScreenMode::Ring) {
        encyclopedia_.renderPopups(renderer, camera_);
    }
    dialogue_.render(renderer, camera_.width(), camera_.height());
    finishUiFrame(renderer);
    renderDebugOverlay(renderer, time);
    renderScreenTransitionOverlay(renderer);
    renderer.present();
}

}
