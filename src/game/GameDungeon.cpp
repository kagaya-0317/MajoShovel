#include "game/GameInternal.hpp"

#include "data/GameBalance.hpp"
#include "data/StageWeight.hpp"

#include "game/EntityStatusVisuals.hpp"

namespace majo {

namespace {

struct FloorCavernAnchor {
    Vec2 center{};
    float radius = 2.0f;
};

constexpr int WallPocketRewardNodeCount = 6;
constexpr int WallPocketMoneyNodeCount = 8;
constexpr int WallPocketChestNodeCount = 4;
constexpr int CoinRoomMoneyNodeMinCount = 5;
constexpr int CoinRoomMoneyNodeMaxCount = 8;
constexpr float CoinRoomMoneyInnerRadiusScale = 0.42f;
constexpr float CoinRoomMoneyOuterRadiusScale = 0.58f;
constexpr float CoinRoomMoneyMinRadiusTiles = 2.0f;
constexpr float WallPocketProgressStart = 0.12f;
constexpr float WallPocketProgressSpan = 0.76f;
constexpr float WallPocketMinOffsetTiles = 6.5f;
constexpr float WallPocketMaxOffsetTiles = 11.5f;
constexpr int StartReservationRadiusTiles = 2;
constexpr int GoalReservationRadiusTiles = 3;
constexpr int WarpReservationRadiusTiles = 2;
constexpr int ExposedPlacementReservationRadiusTiles = 1;
constexpr int SolidPlacementReservationRadiusTiles = 1;
constexpr int PlacementSearchRadiusTiles = 8;
constexpr int SolidPlacementSearchRadiusTiles = 10;
constexpr float LootLandingCollisionRadius = 14.0f;
constexpr float LootLandingDropSpacing = 28.0f;
constexpr float LootLandingWarpClearance = 42.0f;
constexpr int LootLandingRingCount = 12;
constexpr int LootLandingSamplesPerRing = 16;
constexpr float LootLandingFirstRadius = 18.0f;
constexpr float LootLandingRadiusStep = 18.0f;
constexpr float CommonChestMimicChance = 0.05f;
constexpr float RareChestMimicChance = 0.08f;
constexpr float SuperRareChestMimicChance = 0.12f;
constexpr float ChestMimicSpawnWarmupSeconds = 0.18f;
constexpr float MicroFeatureProgressStart = 0.08f;
constexpr float MicroFeatureProgressSpan = 0.84f;
constexpr float MicroFeatureSpacingTiles = 9.5f;
constexpr int MicroFeatureMinCount = 10;
constexpr int MicroFeatureMaxCount = 30;
constexpr double DungeonMinimapRevealIntervalSeconds = 0.15;
constexpr std::string_view FirstDungeonStageId = "stage_01_stardust";
constexpr std::string_view MagnifyingGlassObjectId = "item_magnifying_glass";
constexpr std::string_view CaptureNetObjectId = "item_capture_net";
constexpr float MagnifyingGlassGuaranteedMaxDepthTiles = 100.0f;
constexpr float MagnifyingGlassGuaranteedProgressFallback = 0.24f;
constexpr std::string_view FinalStoryStageId = "stage_03_star_core";
constexpr std::string_view EndingSeenFlag = "ending_seen";
constexpr std::string_view PostEndingIntroFlag = "story_post_ending_intro";
constexpr std::string_view AudioBgmDungeon = "bgm.dungeon";
constexpr std::string_view AudioSeChestOpen = "se.chest.open";
constexpr std::string_view AudioSeCrateBreak = "se.crate.break";
constexpr std::string_view AudioSeCaptureSuccess = "se.capture.success";
constexpr std::string_view AudioSeCaptureFail = "se.capture.fail";
constexpr std::string_view AudioSePickup = "se.pickup";
constexpr std::string_view AudioSeEnemySpawn = "se.enemy.spawn";
constexpr std::string_view AudioSeItemBreak = "se.item.break";
constexpr std::string_view AudioSeDiscovery = "se.discovery";
constexpr std::string_view AudioSeWarpDiscovery = "se.discovery.warp";
constexpr std::string_view AudioSeFootstepBaseOutdoor = "se.footstep.base_outdoor";
constexpr std::string_view AudioSeFootstepHomeInterior = "se.footstep.home";
constexpr std::string_view AudioSeFootstepDungeon = "se.footstep.dungeon";
constexpr std::string_view DigToolFailsafeShovelObjectId = "item_shovel";
constexpr std::string_view DigToolFailsafeDigCategory = "\xE6\x8E\x98\xE5\x89\x8A";
constexpr float CaptureAbsorbDurationSeconds = 0.78f;
constexpr float CaptureAbsorbFlyDelaySeconds = 0.24f;
constexpr float CaptureAbsorbSparkIntervalSeconds = 0.045f;
constexpr float DigToolFailsafeSpawnCooldownSeconds = 12.0f;
constexpr float DigToolFailsafeNearbyDropRadius = 220.0f;
constexpr float FootstepPitchSideOffset = 0.025f;
constexpr float WetGroundPlayerRadiusMultiplier = 1.45f;
constexpr float WetGroundPlayerMinRadius = 16.0f;
constexpr float WetGroundPlayerMaxRadius = 28.0f;
constexpr float FootstepPitchRandomJitter = 0.015f;
constexpr std::uint32_t IntroTutorialSeed = 0x1A57D00Du;
constexpr float IntroTutorialExitInteractRadius = 58.0f;
constexpr float IntroTutorialExitFoundRadiusTiles = 5.0f;
constexpr float IntroTutorialDarkCueTileX = 13.0f;
constexpr float IntroTutorialMidwayCueTileX = 59.0f;
constexpr float IntroTutorialEnemySpawnRadiusTiles = 9.0f;
constexpr float IntroTutorialEnemyEncounterRadiusTiles = 5.6f;
constexpr float IntroTutorialEnemyResolveRadiusTiles = 3.0f;

float captureAbsorbFlyProgress(float elapsedSeconds, float flyDelaySeconds, float durationSeconds)
{
    const float flySeconds = std::max(0.05f, durationSeconds - flyDelaySeconds);
    return smooth01((elapsedSeconds - flyDelaySeconds) / flySeconds);
}

Vec2 captureAbsorbCurvePosition(Vec2 start, Vec2 target, float flyProgress)
{
    const float t = clamp(flyProgress, 0.0f, 1.0f);
    const float rush = t * t * (2.2f - 1.2f * t);
    const Vec2 delta = target - start;
    const Vec2 side = lengthSquared(delta) > 0.01f
        ? normalize(Vec2{-delta.y, delta.x})
        : Vec2{0.0f, -1.0f};
    const float arc = std::sin(t * Pi);
    return lerp(start, target, rush) + side * (arc * 12.0f) + Vec2{0.0f, -arc * 18.0f};
}
constexpr float IntroTutorialSlimeLeashRadiusTiles = 2.4f;
constexpr float IntroTutorialExitLightRadiusTiles = 4.2f;
constexpr std::string_view IntroTutorialShovelObjectId = "item_shovel";
constexpr std::string_view IntroTutorialTorchObjectId = "item_torch";
constexpr std::string_view IntroTutorialCoinBagObjectId = "item_coin_bag";
constexpr std::string_view IntroTutorialIronSwordObjectId = "item_iron_sword";
constexpr std::string_view IntroTutorialSlimeGroup = "intro_slime";
constexpr std::string_view IntroTutorialMushroomGroup = "intro_mushroom";
constexpr float IntroTutorialSecondEnemyEncounterRadiusTiles = 4.8f;
constexpr int IntroTutorialSlimeCombatLockLeftTiles = 8;
constexpr int IntroTutorialSlimeCombatLockRightTiles = 14;
constexpr int IntroTutorialSlimeCombatLockVerticalTiles = 7;
constexpr std::string_view IntroTutorialShovelReadyTrigger = "intro_tutorial:shovel_ready";
constexpr std::string_view IntroTutorialTorchFoundTrigger = "intro_tutorial:torch_found";
constexpr std::string_view IntroTutorialTorchReadyTrigger = "intro_tutorial:torch_ready";
constexpr std::string_view IntroTutorialEnemyEncounterTrigger = "intro_tutorial:enemy_encounter";
constexpr std::string_view IntroTutorialEnemyEncounterFlag = "story_intro_tutorial_enemy_encounter";
constexpr std::string_view IntroTutorialEnemyDefeatedTrigger = "intro_tutorial:enemy_defeated";
constexpr std::string_view IntroTutorialChestFoundTrigger = "intro_tutorial:chest_found";
constexpr std::string_view IntroTutorialMidwayTrigger = "intro_tutorial:midway";
constexpr std::string_view IntroTutorialExitFoundTrigger = "intro_tutorial:exit_found";
constexpr float DiscardThrowStartOffset = 24.0f;
constexpr float DiscardThrowDistance = 310.0f;
constexpr float DiscardThrowLandingJitter = 40.0f;

float randomFootstepPitchJitter()
{
    static std::mt19937 rng{std::random_device{}()};
    static std::uniform_real_distribution<float> distribution(
        -FootstepPitchRandomJitter,
        FootstepPitchRandomJitter);
    return distribution(rng);
}

float chestMimicChanceForKind(LootChestKind kind)
{
    switch (kind) {
    case LootChestKind::Common: return CommonChestMimicChance;
    case LootChestKind::Rare: return RareChestMimicChance;
    case LootChestKind::SuperRare: return SuperRareChestMimicChance;
    }
    return 0.0f;
}
constexpr float DiscardThrowDurationMin = 0.48f;
constexpr float DiscardThrowDurationMax = 0.62f;
constexpr float DiscardThrowArcHeightMin = 52.0f;
constexpr float DiscardThrowArcHeightMax = 72.0f;
constexpr float BossDefeatPresentationSeconds = 1.85f;
constexpr float DungeonFocusMoveSeconds = 0.72f;
constexpr float DungeonFocusDefaultHoldSeconds = 2.0f;
constexpr float DungeonRewardFocusMoveSeconds = 0.35f;
constexpr float DungeonRewardFocusHoldSeconds = 0.8f;
constexpr float DungeonEventGuideSeconds = 45.0f;
constexpr float DungeonEventObjectHitPaddingPx = 16.0f;
constexpr float DungeonEventMinSpacingTiles = 8.0f;
constexpr float DungeonEventDiscoveryCooldownSeconds = 2.4f;
constexpr float DungeonEventDamageInterruptDelaySeconds = 1.1f;
constexpr float DungeonEventNpcTalkRadiusTiles = 2.2f;
constexpr std::string_view DungeonEventItemRequestHeal = "heal";
constexpr std::string_view DungeonEventItemRequestBlade = "blade";
constexpr std::string_view DungeonEventItemRequestTool = "tool";
constexpr double PlayerRegenRateCap = 0.5;

struct PlacementReservation {
    DungeonTile tile{};
    int radiusTiles = 0;
};

bool validDungeonFocusPosition(Vec2 position)
{
    return std::isfinite(position.x) && std::isfinite(position.y);
}

float dungeonFocusEase(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float dungeonFocusHoldSeconds(float seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0f) {
        return DungeonFocusDefaultHoldSeconds;
    }
    return seconds;
}

float dungeonFocusDurationSeconds(float seconds, float fallbackSeconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0f) {
        return fallbackSeconds;
    }
    return seconds;
}

bool sameDungeonTile(DungeonTile lhs, DungeonTile rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool tagListContains(std::string_view text, std::string_view token)
{
    const std::string normalizedToken = lowerAscii(std::string(token));
    std::string current;
    auto flush = [&]() {
        std::string trimmed = trimAscii(current);
        current.clear();
        return lowerAscii(std::move(trimmed));
    };
    for (char ch : text) {
        if (ch == '|') {
            if (flush() == normalizedToken) {
                return true;
            }
            continue;
        }
        current.push_back(ch);
    }
    return flush() == normalizedToken;
}

bool dungeonEventKindUsesDiscoveryFocus(Game::DungeonEventKind kind)
{
    switch (kind) {
    case Game::DungeonEventKind::CoinRoom:
        return false;
    default:
        return true;
    }
}

DungeonTile coinRoomMoneyNodeTile(const SpecialRoomAnchor& room, int index, int count, float angleOffset)
{
    const float angle = angleOffset + Pi * 2.0f * (static_cast<float>(index) / static_cast<float>(std::max(1, count)));
    const float radiusScale = count >= 7 && index % 2 == 1
        ? CoinRoomMoneyOuterRadiusScale
        : CoinRoomMoneyInnerRadiusScale;
    const float radiusTiles = std::max(CoinRoomMoneyMinRadiusTiles, room.radius * radiusScale);
    return roundDungeonTile(room.center + fromAngle(angle) * radiusTiles);
}

Vec2 dungeonEventSelfLightPosition(const Game::DungeonEventInstance& event)
{
    Vec2 position = tileWorldCenter(event.focusTile);
    if (dungeonEventKindIsWitch(event.kind)) {
        position = witchSelfLightCenter(position);
    }
    return position;
}

float dungeonEventSelfLightRadiusPx(const Game::DungeonEventInstance& event)
{
    const float baseRadiusTiles = event.selfLightRadiusTiles > 0.0f
        ? event.selfLightRadiusTiles
        : (dungeonEventKindIsWitch(event.kind) ? dungeonEventLightRadiusTiles(event.kind) : 0.0f);
    const float radiusPx = baseRadiusTiles * static_cast<float>(balance::TileSize);
    return dungeonEventKindIsWitch(event.kind) ? witchSelfLightRadius(radiusPx) : radiusPx;
}

bool dungeonEventSelfLightActive(const Game::DungeonEventInstance& event)
{
    return !event.completed || dungeonEventKindIsWitch(event.kind);
}

StageDefinition makeIntroTutorialStageDefinition()
{
    StageDefinition stage;
    stage.id = std::string(IntroTutorialStageId);
    stage.name = "落ちた星の道";
    stage.type = "チュートリアル";
    stage.displayOrder = 0;
    stage.implementationState = "code_intro";
    stage.generationProfile = std::string(IntroTutorialGenerationProfile);
    stage.terrainProfile = "soft_stardust";
    stage.goalDistanceTiles = 80;
    stage.detourRate = 0.0;
    stage.branchDensity = 0.0;
    stage.cavernWidthMultiplier = 0.82;
    stage.terrainHardnessMultiplier = 0.85;
    stage.warpPointCount = 0;
    stage.specialRoomCount = 0;
    return stage;
}

void carveTutorialPocket(TileMap& tileMap, DungeonTile center, int radius)
{
    const int clampedRadius = std::max(0, radius);
    const int radiusSq = clampedRadius * clampedRadius;
    for (int y = -clampedRadius; y <= clampedRadius; ++y) {
        for (int x = -clampedRadius; x <= clampedRadius; ++x) {
            if (x * x + y * y > radiusSq + clampedRadius) {
                continue;
            }
            tileMap.setTileOverride(DungeonTile{center.x + x, center.y + y}, TileType::Empty);
        }
    }
}

DungeonTile introTutorialRelativeTile(DungeonTile origin, int dx, int dy)
{
    return DungeonTile{origin.x + dx, origin.y + dy};
}

std::uint32_t introTutorialTerrainHash(std::uint32_t seed, int dx, int dy, std::uint32_t salt)
{
    std::uint32_t h = seed ^ salt;
    h ^= static_cast<std::uint32_t>(dx) * 0x9E3779B9u;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<std::uint32_t>(dy) * 0x85EBCA6Bu;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

TileType introTutorialDeadEndWallTile(std::uint32_t seed, int dx, int dy)
{
    const std::uint32_t roll = introTutorialTerrainHash(seed, dx, dy, 0xD1EAD00Du) % 100u;
    if (roll < 35u) {
        return TileType::Dirt;
    }
    if (roll < 85u) {
        return TileType::Rock;
    }
    return TileType::HardRock;
}

TileType introTutorialFirstWallTile(std::uint32_t seed, int dx, int dy)
{
    const int absY = std::abs(dy);
    if (absY == 0) {
        return TileType::Dirt;
    }

    const std::uint32_t roll = introTutorialTerrainHash(seed, dx, dy, 0xF1A57A11u) % 100u;
    if (absY == 1) {
        return roll < 75u ? TileType::Dirt : TileType::Rock;
    }
    if (roll < 10u) {
        return TileType::Dirt;
    }
    return roll < 75u ? TileType::Rock : TileType::HardRock;
}

TileType introTutorialExitWallTile(std::uint32_t seed, int dx, int dy)
{
    const std::uint32_t roll = introTutorialTerrainHash(seed, dx, dy, 0xE817CAFEu) % 100u;
    if (roll < 18u) {
        return TileType::Dirt;
    }
    return roll < 82u ? TileType::Rock : TileType::HardRock;
}

void applyIntroTutorialStartPocket(TileMap& tileMap, DungeonTile startTile, std::uint32_t seed)
{
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
            const bool rightOpenPocket = dx >= 0 && dx <= 4 && std::abs(dy) <= 2;
            const bool shallowLeftNotch = dx == -1 && dy == 0;
            tileMap.setTileOverride(
                introTutorialRelativeTile(startTile, dx, dy),
                rightOpenPocket || shallowLeftNotch
                    ? TileType::Empty
                    : introTutorialDeadEndWallTile(seed, dx, dy));
        }
    }
}

void applyIntroTutorialFirstRubbleWall(TileMap& tileMap, DungeonTile startTile, std::uint32_t seed)
{
    for (int dx = 5; dx <= 9; ++dx) {
        for (int dy = -2; dy <= 2; ++dy) {
            tileMap.setTileOverride(
                introTutorialRelativeTile(startTile, dx, dy),
                introTutorialFirstWallTile(seed, dx, dy));
        }
    }
}

void applyIntroTutorialPostWallCorridor(TileMap& tileMap, DungeonTile startTile)
{
    for (int dx = 10; dx <= 17; ++dx) {
        tileMap.setTileOverride(introTutorialRelativeTile(startTile, dx, 0), TileType::Empty);
        tileMap.setTileOverride(introTutorialRelativeTile(startTile, dx, -1), TileType::Empty);
        tileMap.setTileOverride(introTutorialRelativeTile(startTile, dx, 1), TileType::Empty);
    }
}

void applyIntroTutorialExitPocket(TileMap& tileMap, DungeonTile exitTile, std::uint32_t seed)
{
    for (int dy = -5; dy <= 5; ++dy) {
        for (int dx = -8; dx <= 5; ++dx) {
            const bool exitPocket = dx * dx + dy * dy <= 10;
            const bool approachCorridor = dx >= -8 && dx <= -2 && dy >= -1 && dy <= 2;
            tileMap.setTileOverride(
                introTutorialRelativeTile(exitTile, dx, dy),
                exitPocket || approachCorridor
                    ? TileType::Empty
                    : introTutorialExitWallTile(seed, dx, dy));
        }
    }
}

void applyIntroTutorialRubbleGate(TileMap& tileMap, const DungeonLayout& layout)
{
    applyIntroTutorialStartPocket(tileMap, layout.startTile, layout.seed);
    applyIntroTutorialFirstRubbleWall(tileMap, layout.startTile, layout.seed);
    applyIntroTutorialPostWallCorridor(tileMap, layout.startTile);
    applyIntroTutorialExitPocket(tileMap, layout.goalTile, layout.seed);
}

std::string introTutorialSlimeEnemyId(const EnemyCatalog& catalog)
{
    const auto matchesSlimeId = [](std::string_view id) {
        const std::string lower = lowerAscii(std::string(id));
        return lower == "slime" ||
            lower == "enemy_slime" ||
            lower.find("slime") != std::string::npos;
    };
    for (const EnemyDefinition& enemy : catalog.enemies) {
        if (enemy.name == "スライム") {
            return enemy.id;
        }
    }
    for (const EnemyDefinition& enemy : catalog.enemies) {
        if (matchesSlimeId(enemy.id)) {
            return enemy.id;
        }
    }
    return {};
}

std::string introTutorialMushroomEnemyId(const EnemyCatalog& catalog)
{
    const auto matchesMushroomId = [](std::string_view id) {
        const std::string lower = lowerAscii(std::string(id));
        return lower == "bake_kinoko" ||
            lower == "kinoko" ||
            lower == "enemy_kinoko" ||
            lower.find("kinoko") != std::string::npos ||
            lower.find("mush") != std::string::npos ||
            lower.find("shroom") != std::string::npos ||
            lower.find("fungus") != std::string::npos;
    };
    for (const EnemyDefinition& enemy : catalog.enemies) {
        if (enemy.name == "化けキノコ" || enemy.name.find("キノコ") != std::string::npos) {
            return enemy.id;
        }
    }
    for (const EnemyDefinition& enemy : catalog.enemies) {
        if (matchesMushroomId(enemy.id)) {
            return enemy.id;
        }
    }
    return {};
}

DialogueSequence singleLineDialogueSequence(
    std::string id,
    std::string speakerId,
    std::string speakerName,
    std::string text)
{
    DialogueLine line{
        .speakerId = std::move(speakerId),
        .speakerName = std::move(speakerName),
        .text = std::move(text),
    };

    DialogueSequence sequence;
    sequence.id = std::move(id);
    sequence.lines.push_back(line);
    sequence.steps.push_back(DialogueStep{DialogueStepKind::Line, std::move(line), 0.0f});
    return sequence;
}

int dungeonEventStageMaxCount(std::string_view stageId)
{
    if (stageId == "stage_01_stardust") {
        return 4;
    }
    if (stageId == "stage_04_astral_mine") {
        return 5;
    }
    return 5;
}

bool dungeonEventKindTooClose(const std::vector<Game::DungeonEventInstance>& instances, DungeonTile tile)
{
    const float minDistSq = DungeonEventMinSpacingTiles * DungeonEventMinSpacingTiles;
    const Vec2 candidate{static_cast<float>(tile.x), static_cast<float>(tile.y)};
    return std::any_of(instances.begin(), instances.end(), [&](const Game::DungeonEventInstance& event) {
        const Vec2 existing{static_cast<float>(event.centerTile.x), static_cast<float>(event.centerTile.y)};
        return distanceSquared(candidate, existing) < minDistSq;
    });
}

bool isDungeonEventMarkerKind(Game::DungeonEventKind kind)
{
    (void)kind;
    return true;
}

bool dungeonEventSameTile(DungeonTile lhs, DungeonTile rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

DungeonTile dungeonEventOffsetTile(DungeonTile tile, int dx, int dy)
{
    tile.x += dx;
    tile.y += dy;
    return tile;
}

DungeonTile dungeonEventObjectTile(DungeonTile centerTile, float angle, float radiusTiles)
{
    return dungeonEventOffsetTile(
        centerTile,
        static_cast<int>(std::round(std::cos(angle) * radiusTiles)),
        static_cast<int>(std::round(std::sin(angle) * radiusTiles)));
}

std::uint32_t dungeonEventCavitySeed(std::uint32_t layoutSeed, const Game::DungeonEventInstance& event)
{
    std::uint32_t seed = layoutSeed ^ 0xD17A6E33u;
    seed ^= static_cast<std::uint32_t>(std::hash<std::string>{}(event.id));
    seed ^= static_cast<std::uint32_t>(event.centerTile.x) * 0x85EBCA6Bu;
    seed ^= static_cast<std::uint32_t>(event.centerTile.y) * 0xC2B2AE35u;
    return seed;
}

std::uint32_t dungeonEventTileNoise(std::uint32_t seed, DungeonTile tile)
{
    std::uint32_t h = seed ^ 0x9E3779B9u;
    h ^= static_cast<std::uint32_t>(tile.x) + 0x85EBCA6Bu + (h << 6) + (h >> 2);
    h ^= static_cast<std::uint32_t>(tile.y) + 0xC2B2AE35u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

bool dungeonEventCavityContains(const std::vector<DungeonTile>& tiles, DungeonTile tile)
{
    return std::any_of(tiles.begin(), tiles.end(), [tile](DungeonTile existing) {
        return dungeonEventSameTile(existing, tile);
    });
}

void addDungeonEventCavityTile(std::vector<DungeonTile>& tiles, DungeonTile tile)
{
    if (!dungeonEventCavityContains(tiles, tile)) {
        tiles.push_back(tile);
    }
}

void addDungeonEventCavityLine(std::vector<DungeonTile>& tiles, DungeonTile from, DungeonTile to)
{
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    const int steps = std::max(std::abs(dx), std::abs(dy));
    if (steps <= 0) {
        addDungeonEventCavityTile(tiles, from);
        return;
    }
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        addDungeonEventCavityTile(tiles, DungeonTile{
            from.x + static_cast<int>(std::round(static_cast<float>(dx) * t)),
            from.y + static_cast<int>(std::round(static_cast<float>(dy) * t)),
        });
    }
}

void addDungeonEventCavityPocket(std::vector<DungeonTile>& tiles, DungeonTile center, int radiusTiles)
{
    const int radius = std::max(0, radiusTiles);
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y > radius * radius) {
                continue;
            }
            addDungeonEventCavityTile(tiles, DungeonTile{center.x + x, center.y + y});
        }
    }
}

std::vector<DungeonTile> dungeonEventForcedCavityTiles(const Game::DungeonEventInstance& event)
{
    std::vector<DungeonTile> forced;
    forced.push_back(event.centerTile);
    forced.push_back(event.focusTile);
    switch (event.kind) {
    case Game::DungeonEventKind::SleepingEnemyTreasure: {
        forced.push_back(dungeonEventOffsetTile(event.centerTile, 0, -2));
        constexpr std::array<DungeonTile, 6> Offsets{{
            {-2, 0},
            {2, 0},
            {-1, 2},
            {1, 2},
            {-2, -1},
            {2, -1},
        }};
        for (DungeonTile offset : Offsets) {
            forced.push_back(dungeonEventOffsetTile(event.centerTile, offset.x, offset.y));
        }
        break;
    }
    case Game::DungeonEventKind::MonsterSwarmRoom: {
        forced.push_back(dungeonEventOffsetTile(event.centerTile, 0, -3));
        for (int i = 0; i < 8; ++i) {
            const float angle = (Pi * 2.0f) * (static_cast<float>(i) / 8.0f);
            forced.push_back(dungeonEventObjectTile(event.centerTile, angle, 2.0f));
        }
        break;
    }
    case Game::DungeonEventKind::NestRoom: {
        const int holeRange = 2;
        const int holeCount = 1 + static_cast<int>(std::hash<std::string>{}(event.id) % static_cast<std::size_t>(holeRange + 1));
        for (int i = 0; i < holeCount; ++i) {
            const float angle = (Pi * 2.0f) * (static_cast<float>(i) / static_cast<float>(holeCount));
            forced.push_back(dungeonEventObjectTile(event.centerTile, angle, 1.8f));
        }
        break;
    }
    case Game::DungeonEventKind::BossMonsterRoom: {
        forced.push_back(dungeonEventOffsetTile(event.centerTile, 0, -3));
        constexpr std::array<DungeonTile, 3> Offsets{{
            {-2, 1},
            {2, 1},
            {0, 2},
        }};
        for (DungeonTile offset : Offsets) {
            forced.push_back(dungeonEventOffsetTile(event.centerTile, offset.x, offset.y));
        }
        break;
    }
    case Game::DungeonEventKind::GlowingRockRoom:
    case Game::DungeonEventKind::ElectricCircuitRoom: {
        const std::size_t seed = std::hash<std::string>{}(event.id);
        const int count = event.kind == Game::DungeonEventKind::GlowingRockRoom
            ? 3 + static_cast<int>(seed % 3u)
            : 2 + static_cast<int>(seed % 3u);
        const int angleShift = event.kind == Game::DungeonEventKind::GlowingRockRoom ? 8 : 10;
        for (int i = 0; i < count; ++i) {
            const float angle = (Pi * 2.0f) * (static_cast<float>(i) / static_cast<float>(count)) +
                static_cast<float>((seed >> angleShift) & 0xFFu) * 0.001f;
            forced.push_back(dungeonEventObjectTile(event.centerTile, angle, 2.0f));
        }
        break;
    }
    case Game::DungeonEventKind::BuriedWitch: {
        constexpr std::array<DungeonTile, 4> Offsets{{
            {0, -1},
            {-1, 0},
            {1, 0},
            {0, 1},
        }};
        for (DungeonTile offset : Offsets) {
            forced.push_back(dungeonEventOffsetTile(event.centerTile, offset.x, offset.y));
        }
        break;
    }
    case Game::DungeonEventKind::LostBaggageWitch: {
        const std::size_t seed = std::hash<std::string>{}(event.id);
        const float angle = (Pi * 2.0f) * (static_cast<float>(seed % 1009u) / 1009.0f);
        forced.push_back(dungeonEventObjectTile(event.centerTile, angle, 3.0f));
        break;
    }
    case Game::DungeonEventKind::SurroundedWitch: {
        constexpr std::array<DungeonTile, 3> Offsets{{
            {-2, 0},
            {2, 0},
            {0, 2},
        }};
        for (DungeonTile offset : Offsets) {
            forced.push_back(dungeonEventOffsetTile(event.centerTile, offset.x, offset.y));
        }
        break;
    }
    case Game::DungeonEventKind::ColdWitchCampfire:
        forced.push_back(dungeonEventOffsetTile(event.centerTile, 1, 1));
        break;
    case Game::DungeonEventKind::HeavyRockWitch:
        forced.push_back(dungeonEventOffsetTile(event.centerTile, 1, 0));
        break;
    case Game::DungeonEventKind::SafeCavern:
    case Game::DungeonEventKind::CoinRoom:
    case Game::DungeonEventKind::WarpGuideMap:
    case Game::DungeonEventKind::ItemRequestWitch:
        break;
    }
    return forced;
}

std::vector<DungeonTile> buildDungeonEventCavityTiles(
    const Game::DungeonEventInstance& event,
    std::uint32_t layoutSeed)
{
    const DungeonEventCavityProfile profile = dungeonEventCavityProfile(event.kind);
    const std::uint32_t seed = dungeonEventCavitySeed(layoutSeed, event);
    std::mt19937 rng(seed);
    int radiusTiles = profile.minRadiusTiles;
    if (event.cavityRadiusTiles > 0.0f) {
        radiusTiles = static_cast<int>(std::round(event.cavityRadiusTiles * profile.specialRoomRadiusScale));
        radiusTiles = std::clamp(radiusTiles, profile.minRadiusTiles, profile.maxRadiusTiles);
    } else if (profile.maxRadiusTiles > profile.minRadiusTiles) {
        std::uniform_int_distribution<int> radiusDistribution(profile.minRadiusTiles, profile.maxRadiusTiles);
        radiusTiles = radiusDistribution(rng);
    }

    std::vector<DungeonTile> tiles;
    const int innerRadius = std::min(profile.innerRadiusTiles, radiusTiles);
    addDungeonEventCavityPocket(tiles, event.centerTile, innerRadius);
    addDungeonEventCavityTile(tiles, event.focusTile);

    const int targetCount = std::max(
        static_cast<int>(tiles.size()),
        static_cast<int>(std::round(3.14159265f * static_cast<float>(radiusTiles * radiusTiles) * profile.fillRatio)));
    constexpr std::array<DungeonTile, 8> Steps{{
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
        {1, 1},
        {-1, 1},
        {1, -1},
        {-1, -1},
    }};
    std::uniform_int_distribution<std::size_t> stepDistribution(0, Steps.size() - 1);
    int attempts = 0;
    while (static_cast<int>(tiles.size()) < targetCount && attempts < targetCount * 20) {
        ++attempts;
        std::uniform_int_distribution<std::size_t> baseDistribution(0, tiles.size() - 1);
        const DungeonTile base = tiles[baseDistribution(rng)];
        const DungeonTile step = Steps[stepDistribution(rng)];
        const DungeonTile candidate{base.x + step.x, base.y + step.y};
        const int dx = candidate.x - event.centerTile.x;
        const int dy = candidate.y - event.centerTile.y;
        const int distSq = dx * dx + dy * dy;
        const std::uint32_t edgeNoise = dungeonEventTileNoise(seed, candidate);
        const float edgeAllowance = (static_cast<float>(edgeNoise & 0xFFu) / 255.0f - 0.5f) * 1.6f;
        const float localRadius = std::max(1.0f, static_cast<float>(radiusTiles) + edgeAllowance);
        if (static_cast<float>(distSq) > localRadius * localRadius) {
            continue;
        }
        addDungeonEventCavityTile(tiles, candidate);
    }

    for (DungeonTile forced : dungeonEventForcedCavityTiles(event)) {
        addDungeonEventCavityLine(tiles, event.centerTile, forced);
        addDungeonEventCavityPocket(tiles, forced, event.kind == Game::DungeonEventKind::BuriedWitch ? 0 : 1);
    }
    return tiles;
}

enum class DungeonEventHitRequirement {
    AnyDamage,
    Thunder,
    Fire,
    HeavyImpact,
};

bool dungeonEventEffectSpecsContainAny(const std::vector<EffectSpec>& specs, std::initializer_list<std::string_view> effectIds)
{
    for (std::string_view effectId : effectIds) {
        if (effectSpecsContain(specs, effectId)) {
            return true;
        }
    }
    return false;
}

bool dungeonEventEffectSpecsContainLight(const std::vector<EffectSpec>& specs)
{
    for (const EffectSpec& spec : specs) {
        for (const std::string& effect : spec.effects) {
            if (effect == "light" || effect.rfind("light_", 0) == 0 || effect.find("_light") != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

bool dungeonEventItemEffectMatches(
    const ObjectCatalog& catalog,
    const SpellRingItem& item,
    std::initializer_list<std::string_view> exactEffects,
    bool allowLightEffect)
{
    if (dungeonEventEffectSpecsContainAny(item.addedEffects, exactEffects) ||
        (allowLightEffect && dungeonEventEffectSpecsContainLight(item.addedEffects))) {
        return true;
    }
    if (item.objectId.empty()) {
        return false;
    }
    const ItemData* object = catalog.registry.findById(item.objectId);
    if (object == nullptr) {
        return false;
    }
    return dungeonEventEffectSpecsContainAny(object->normalEffects, exactEffects) ||
        dungeonEventEffectSpecsContainAny(object->orbitEffects, exactEffects) ||
        (allowLightEffect &&
            (dungeonEventEffectSpecsContainLight(object->normalEffects) ||
                dungeonEventEffectSpecsContainLight(object->orbitEffects)));
}

bool dungeonEventItemConductsThunder(const ObjectCatalog& catalog, const SpellRingItem& item)
{
    if (item.damageType == "thunder" ||
        (item.magicAuraTimer > 0.0f && item.magicAuraDamageType == "thunder")) {
        return true;
    }
    return dungeonEventItemEffectMatches(catalog, item, {"cast_thunder"}, false);
}

bool dungeonEventItemLightsFire(const ObjectCatalog& catalog, const SpellRingItem& item)
{
    if (item.damageType == "fire" ||
        (item.magicAuraTimer > 0.0f && item.magicAuraDamageType == "fire") ||
        item.lightRadius > 0.0f) {
        return true;
    }
    return dungeonEventItemEffectMatches(catalog, item, {"cast_fire"}, true);
}

int dungeonEventObjectHitDamageFor(const ObjectCatalog& catalog, const SpellRingItem& item, DungeonEventHitRequirement requirement)
{
    switch (requirement) {
    case DungeonEventHitRequirement::Thunder:
        return dungeonEventItemConductsThunder(catalog, item) ? 1 : 0;
    case DungeonEventHitRequirement::Fire:
        return dungeonEventItemLightsFire(catalog, item) ? 1 : 0;
    case DungeonEventHitRequirement::HeavyImpact: {
        int damage = std::max({0, item.damage, item.digPower});
        if (damage <= 0) {
            return 0;
        }
        if (item.damageType == "blunt" || item.weight >= 3.0f) {
            damage += 2;
        }
        return std::max(1, damage);
    }
    case DungeonEventHitRequirement::AnyDamage:
        break;
    }
    return std::max({0, item.damage, item.digPower});
}

bool dungeonEventObjectMatchesRequest(const ItemData& item, std::string_view requestKey)
{
    if (requestKey == DungeonEventItemRequestHeal) {
        return item.category == "\xE5\x9B\x9E\xE5\xBE\xA9" ||
            effectSpecsContain(item.normalEffects, "heal") ||
            effectSpecsContain(item.orbitEffects, "heal");
    }
    if (requestKey == DungeonEventItemRequestBlade) {
        return item.category == "\xE6\xAD\xA6\xE5\x99\xA8" ||
            item.damageType == "slash" ||
            std::any_of(item.tags.begin(), item.tags.end(), [](const std::string& tag) {
                const std::string lower = lowerAscii(tag);
                return lower == "blade" || lower == "slash" || lower == "knife" || lower == "sword";
            });
    }
    if (requestKey == DungeonEventItemRequestTool) {
        return item.category == "\xE6\x8E\x98\xE5\x89\x8A" ||
            item.category == "\xE6\x8E\xA2\xE7\xB4\xA2" ||
            item.digPower > 0 ||
            effectSpecsContain(item.normalEffects, "detect") ||
            effectSpecsContain(item.orbitEffects, "detect");
    }
    return false;
}

std::string dungeonEventItemRequestKeyFor(const Game::DungeonEventInstance& event)
{
    switch (std::hash<std::string>{}(event.id) % 3u) {
    case 0: return std::string(DungeonEventItemRequestHeal);
    case 1: return std::string(DungeonEventItemRequestBlade);
    default: return std::string(DungeonEventItemRequestTool);
    }
}

const char* dungeonEventItemRequestDisplayName(std::string_view requestKey)
{
    if (requestKey == DungeonEventItemRequestHeal) {
        return "回復できそうなもの";
    }
    if (requestKey == DungeonEventItemRequestBlade) {
        return "切れそうな道具";
    }
    if (requestKey == DungeonEventItemRequestTool) {
        return "探索や掘削に使えるもの";
    }
    return "役に立ちそうなもの";
}

bool consumeDungeonEventRequestItem(
    InventorySystem& inventory,
    std::string_view requestKey,
    std::string& outObjectId,
    std::string& outDisplayName)
{
    for (const InventoryObjectStack& stack : inventory.objectStacks()) {
        if (stack.count <= 0 || !dungeonEventObjectMatchesRequest(stack.item, requestKey)) {
            continue;
        }
        outObjectId = stack.objectId;
        outDisplayName = stack.item.name.empty() ? stack.objectId : stack.item.name;
        return inventory.removeObjectItemCount(stack.objectId, 1);
    }

    for (const InventoryObjectInstance& instance : inventory.objectInstances()) {
        if (instance.instance.protectionEnabled ||
            inventory.isStaffEquipped(instance.instance.instanceId) ||
            !dungeonEventObjectMatchesRequest(instance.item, requestKey)) {
            continue;
        }
        outObjectId = instance.item.id;
        outDisplayName = instance.item.name.empty() ? instance.item.id : instance.item.name;
        return inventory.removeObjectInstance(instance.instance.instanceId);
    }
    return false;
}

enum class DungeonEventNpcDialoguePhase {
    Request,
    Progress,
    Reward,
    Thanks,
};

std::string_view dungeonEventNpcDialoguePhaseId(DungeonEventNpcDialoguePhase phase)
{
    switch (phase) {
    case DungeonEventNpcDialoguePhase::Request:
        return "request";
    case DungeonEventNpcDialoguePhase::Progress:
        return "progress";
    case DungeonEventNpcDialoguePhase::Reward:
        return "reward";
    case DungeonEventNpcDialoguePhase::Thanks:
        return "thanks";
    }
    return "progress";
}

std::string dungeonEventNpcStoryEventId(Game::DungeonEventKind kind, DungeonEventNpcDialoguePhase phase)
{
    return "dungeon_witch:" +
        std::string(majo::dungeonEventKindId(kind)) +
        ":" +
        std::string(dungeonEventNpcDialoguePhaseId(phase));
}

std::string dungeonEventWitchRequestText(const Game::DungeonEventInstance& event)
{
    switch (event.kind) {
    case Game::DungeonEventKind::BuriedWitch:
        return "たすけて... 周りのガレキを壊してくれる？";
    case Game::DungeonEventKind::LostBaggageWitch:
        return "荷物を落としちゃったの。近くにあるはずなんだけど...";
    case Game::DungeonEventKind::ItemRequestWitch:
        return std::string(dungeonEventItemRequestDisplayName(event.requestKey)) + "がほしいの。持っていたら分けてくれる？";
    case Game::DungeonEventKind::SurroundedWitch:
        return "魔物に囲まれて動けないの。追い払ってくれる？";
    case Game::DungeonEventKind::ColdWitchCampfire:
        return "寒くて動けない... 焚き火に火をつけられる？";
    case Game::DungeonEventKind::HeavyRockWitch:
        return "この重い岩をどかしてほしいの。";
    default:
        return "ちょっと困ってるの。手を貸してくれる？";
    }
}

std::string dungeonEventWitchProgressText(const Game::DungeonEventInstance& event)
{
    switch (event.kind) {
    case Game::DungeonEventKind::BuriedWitch:
        return "ガレキがまだ残ってるみたい。周りを掘ってくれる？";
    case Game::DungeonEventKind::LostBaggageWitch:
        return "落とし物は、たぶん近くにあるはず。見つけたら持ってきて。";
    case Game::DungeonEventKind::ItemRequestWitch:
        return std::string("まだ") + dungeonEventItemRequestDisplayName(event.requestKey) + "を探してるの。持っていたら分けてくれる？";
    case Game::DungeonEventKind::SurroundedWitch:
        return "まだ魔物の気配がするの。もう少しだけお願い。";
    case Game::DungeonEventKind::ColdWitchCampfire:
        return "焚き火に火がつけば、動けそうなんだけど...";
    case Game::DungeonEventKind::HeavyRockWitch:
        return "岩がまだ動かないの。壊せそうならお願い。";
    default:
        return "まだ困ってるの。もう少し手を貸して。";
    }
}

std::string dungeonEventWitchRewardText(const Game::DungeonEventInstance& event)
{
    if (event.kind == Game::DungeonEventKind::ItemRequestWitch && !event.deliveredObjectId.empty()) {
        return "助かったよ。これ、少しだけど受け取って。";
    }
    return "ありがとう。これ、少しだけど受け取って。";
}

std::string dungeonEventWitchThanksText()
{
    return "本当に助かったよ。気をつけてね。";
}

std::string dungeonEventWitchDialogueText(
    const Game::DungeonEventInstance& event,
    DungeonEventNpcDialoguePhase phase)
{
    switch (phase) {
    case DungeonEventNpcDialoguePhase::Request:
        return dungeonEventWitchRequestText(event);
    case DungeonEventNpcDialoguePhase::Progress:
        return dungeonEventWitchProgressText(event);
    case DungeonEventNpcDialoguePhase::Reward:
        return dungeonEventWitchRewardText(event);
    case DungeonEventNpcDialoguePhase::Thanks:
        return dungeonEventWitchThanksText();
    }
    return dungeonEventWitchProgressText(event);
}

DialogueSequence makeDungeonEventWitchDialogueSequence(
    const Game::DungeonEventInstance& event,
    DungeonEventNpcDialoguePhase phase)
{
    DialogueSequence sequence;
    sequence.id =
        "dungeon_witch_dynamic:" +
        event.id +
        ":" +
        std::string(dungeonEventNpcDialoguePhaseId(phase));
    sequence.lines.push_back(DialogueLine{
        "witch",
        "魔女",
        "",
        dungeonEventWitchDialogueText(event, phase),
    });
    return sequence;
}

bool dungeonEventEnemyDefinitionExcluded(const EnemyDefinition& definition)
{
    for (const std::string& tag : definition.enemyTags) {
        const std::string lower = lowerAscii(tag);
        if (lower == "boss" ||
            lower == "boss_only" ||
            lower == "event_only" ||
            lower == "fixed_only" ||
            lower == "no_normal_spawn") {
            return true;
        }
    }
    return false;
}

std::string chooseDungeonEventEnemyId(
    const EnemyCatalog& catalog,
    std::string_view stageId,
    int depthRank,
    std::uint32_t seed)
{
    std::vector<const EnemyDefinition*> candidates;
    std::vector<double> weights;
    for (const EnemyDefinition& definition : catalog.enemies) {
        if (definition.id.empty() || dungeonEventEnemyDefinitionExcluded(definition)) {
            continue;
        }
        double weight = enemySpawnWeightFor(definition, stageId, depthRank);
        if (weight <= 0.0) {
            weight = 1.0;
        }
        candidates.push_back(&definition);
        weights.push_back(weight);
    }
    if (candidates.empty()) {
        for (const EnemyDefinition& definition : catalog.enemies) {
            if (!definition.id.empty()) {
                return definition.id;
            }
        }
        return {};
    }

    std::mt19937 rng(seed);
    std::discrete_distribution<std::size_t> distribution(weights.begin(), weights.end());
    const std::size_t index = std::min<std::size_t>(distribution(rng), candidates.size() - 1);
    return candidates[index]->id;
}

struct PlacementReservations {
    std::vector<PlacementReservation> entries;

    void reserve(DungeonTile tile, int radiusTiles)
    {
        entries.push_back(PlacementReservation{
            .tile = tile,
            .radiusTiles = std::max(0, radiusTiles),
        });
    }

    bool blocked(DungeonTile tile, int radiusTiles) const
    {
        const int candidateRadius = std::max(0, radiusTiles);
        for (const PlacementReservation& entry : entries) {
            const int minDistance = candidateRadius + entry.radiusTiles;
            const int dx = tile.x - entry.tile.x;
            const int dy = tile.y - entry.tile.y;
            if (dx * dx + dy * dy <= minDistance * minDistance) {
                return true;
            }
        }
        return false;
    }

    bool tryReserve(DungeonTile tile, int radiusTiles)
    {
        if (blocked(tile, radiusTiles)) {
            return false;
        }
        reserve(tile, radiusTiles);
        return true;
    }

    bool reserveNearest(DungeonTile preferred, int radiusTiles, int maxSearchTiles, DungeonTile& outTile)
    {
        if (tryReserve(preferred, radiusTiles)) {
            outTile = preferred;
            return true;
        }

        for (int ring = 1; ring <= maxSearchTiles; ++ring) {
            bool found = false;
            DungeonTile best{};
            int bestDistanceSq = std::numeric_limits<int>::max();
            for (int dy = -ring; dy <= ring; ++dy) {
                for (int dx = -ring; dx <= ring; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != ring) {
                        continue;
                    }
                    const DungeonTile candidate{preferred.x + dx, preferred.y + dy};
                    if (blocked(candidate, radiusTiles)) {
                        continue;
                    }
                    const int distanceSq = dx * dx + dy * dy;
                    if (!found || distanceSq < bestDistanceSq) {
                        found = true;
                        best = candidate;
                        bestDistanceSq = distanceSq;
                    }
                }
            }
            if (found) {
                reserve(best, radiusTiles);
                outTile = best;
                return true;
            }
        }

        return false;
    }
};

bool objectIsUsableDigTool(const ItemData& item)
{
    return item.category == DigToolFailsafeDigCategory && item.durability != 0;
}

bool inventoryInstanceIsUsableDigTool(const InventoryObjectInstance& instance)
{
    return instance.item.category == DigToolFailsafeDigCategory &&
        !instance.instance.isBroken &&
        instance.instance.currentDurability != 0;
}

Vec2 effectiveDropPosition(const WorldDropItem& drop)
{
    return drop.jumpActive ? drop.jumpTargetPosition : drop.position;
}

bool isPlayerRegenTarget(std::string_view target)
{
    return target == "player" || target == "owner" || target == "self";
}

bool discoveryQueueContainsObjectEffect(
    const std::vector<EffectDiscoveryEvent>& discoveryEvents,
    std::string_view objectId,
    std::string_view effectKey)
{
    if (objectId.empty() || effectKey.empty()) {
        return false;
    }
    for (const EffectDiscoveryEvent& event : discoveryEvents) {
        if (event.objectId == objectId && event.effectKey == effectKey) {
            return true;
        }
    }
    return false;
}

enum class ObjectBreakKind {
    Generic,
    Wood,
    Ceramic,
    Glass,
};

struct ObjectBreakSpec {
    ObjectBreakKind kind = ObjectBreakKind::Generic;
    std::string_view effectKey{};
    double value = 1.0;
};

struct ObjectBreakEffectEntry {
    std::string_view effectKey{};
    double value = 1.0;
    double duration = 0.0;
};

std::vector<ObjectBreakEffectEntry> objectBreakEffectEntriesFor(const ObjectDefinition* object)
{
    std::vector<ObjectBreakEffectEntry> entries;
    if (object == nullptr) {
        return entries;
    }
    for (const EffectSpec& spec : object->orbitEffects) {
        if (spec.target != "item") {
            continue;
        }
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            entries.push_back(ObjectBreakEffectEntry{
                .effectKey = spec.effects[index],
                .value = index < spec.values.size() ? spec.values[index] : 1.0,
                .duration = spec.duration,
            });
        }
    }
    return entries;
}

ObjectBreakSpec objectBreakSpecFor(const std::vector<ObjectBreakEffectEntry>& effects)
{
    for (const ObjectBreakEffectEntry& effect : effects) {
        if (effect.effectKey == "break_glass_shards") {
            return {.kind = ObjectBreakKind::Glass, .effectKey = "break_glass_shards", .value = effect.value};
        }
    }
    for (const ObjectBreakEffectEntry& effect : effects) {
        if (effect.effectKey == "break_ceramic_shards") {
            return {.kind = ObjectBreakKind::Ceramic, .effectKey = "break_ceramic_shards", .value = effect.value};
        }
    }
    for (const ObjectBreakEffectEntry& effect : effects) {
        if (effect.effectKey == "break_wood_fragments") {
            return {.kind = ObjectBreakKind::Wood, .effectKey = "break_wood_fragments", .value = effect.value};
        }
    }
    return {};
}

ItemBreakVisual itemBreakVisualFor(ObjectBreakKind kind)
{
    switch (kind) {
    case ObjectBreakKind::Wood:
        return ItemBreakVisual::Wood;
    case ObjectBreakKind::Ceramic:
        return ItemBreakVisual::Ceramic;
    case ObjectBreakKind::Glass:
        return ItemBreakVisual::Glass;
    case ObjectBreakKind::Generic:
        break;
    }
    return ItemBreakVisual::Generic;
}

float objectBreakVisualScale(double value)
{
    const double amount = std::max(1.0, value);
    return std::clamp(static_cast<float>(0.88 + (amount - 1.0) * 0.22), 0.75f, 1.65f);
}

float objectBreakShardRadius(const ObjectBreakSpec& spec)
{
    const double amount = std::max(1.0, spec.value);
    if (spec.kind == ObjectBreakKind::Glass) {
        return std::clamp(static_cast<float>(52.0 + (amount - 1.0) * 10.0), 52.0f, 78.0f);
    }
    if (spec.kind == ObjectBreakKind::Ceramic) {
        return std::clamp(static_cast<float>(44.0 + (amount - 1.0) * 8.0), 44.0f, 66.0f);
    }
    return 0.0f;
}

int objectBreakShardDamage(const ObjectBreakSpec& spec)
{
    const double amount = std::max(1.0, spec.value);
    if (spec.kind == ObjectBreakKind::Glass) {
        return std::max(1, static_cast<int>(std::ceil(3.0 + amount)));
    }
    if (spec.kind == ObjectBreakKind::Ceramic) {
        return std::max(1, static_cast<int>(std::ceil(2.0 + amount)));
    }
    return 0;
}

std::string_view objectBreakShardDamageType(ObjectBreakKind kind)
{
    if (kind == ObjectBreakKind::Glass) {
        return "slash";
    }
    if (kind == ObjectBreakKind::Ceramic) {
        return "pierce";
    }
    return "none";
}

float objectBreakElementRadius(std::string_view effectKey, double value)
{
    const double amount = std::max(1.0, value);
    if (effectKey == "break_fire_burst") {
        return std::clamp(static_cast<float>(48.0 + (amount - 1.0) * 10.0), 42.0f, 84.0f);
    }
    if (effectKey == "water_spray") {
        return std::clamp(static_cast<float>(56.0 + (amount - 1.0) * 12.0), 48.0f, 96.0f);
    }
    return 0.0f;
}

int objectBreakFireDamage(double value)
{
    const double amount = std::max(1.0, value);
    return std::clamp(static_cast<int>(std::ceil(2.0 + amount * 2.0)), 1, 12);
}

int dryWetBonusDamageFor(const std::vector<ObjectBreakEffectEntry>& effects)
{
    double total = 0.0;
    for (const ObjectBreakEffectEntry& effect : effects) {
        if (effect.effectKey == "dry_wet_bonus_damage") {
            total += std::max(0.0, effect.value);
        }
    }
    return std::max(0, static_cast<int>(std::ceil(total)));
}

int objectBreakCoinSpillAmount(const ObjectDefinition& object, double value)
{
    const int rarity = std::clamp(object.rarity, 1, 10);
    const int price = std::max(0, object.price);
    const int baseAmount = std::clamp(1 + rarity / 2 + price / 90, 1, 12);
    const double multiplier = value > 0.0 ? value : 1.0;
    return std::clamp(static_cast<int>(std::round(static_cast<double>(baseAmount) * multiplier)), 1, 30);
}

bool shouldGuaranteeFirstDiscovery(
    const std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem& encyclopedia,
    const ObjectDefinition& object,
    std::string_view effectKey)
{
    return discoveryEvents != nullptr &&
        !encyclopedia.hasObjectEffect(object.id, effectKey) &&
        !discoveryQueueContainsObjectEffect(*discoveryEvents, object.id, effectKey);
}

bool percentRollSucceeds(std::mt19937& rng, double percent)
{
    const double chance = std::clamp(percent, 0.0, 100.0);
    if (chance >= 100.0) {
        return true;
    }
    if (chance <= 0.0) {
        return false;
    }
    std::uniform_real_distribution<double> distribution(0.0, 100.0);
    return distribution(rng) <= chance;
}

void appendObjectEffectDiscovery(
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem& encyclopedia,
    const ObjectDefinition& object,
    std::string_view effectKey,
    Vec2 position)
{
    if (discoveryEvents == nullptr || object.id.empty() || effectKey.empty()) {
        return;
    }
    if (encyclopedia.hasObjectEffect(object.id, effectKey)) {
        return;
    }
    if (discoveryQueueContainsObjectEffect(*discoveryEvents, object.id, effectKey)) {
        return;
    }
    discoveryEvents->push_back(EffectDiscoveryEvent{
        .objectId = object.id,
        .objectName = object.name,
        .effectKey = std::string(effectKey),
        .description = {},
        .note = {},
        .position = position,
    });
}

void reserveLayoutAnchors(PlacementReservations& reservations, const DungeonLayout& layout)
{
    reservations.reserve(layout.startTile, StartReservationRadiusTiles);
    reservations.reserve(layout.goalTile, GoalReservationRadiusTiles);
}

float floorRadiusForRoom(const SpecialRoomAnchor& room)
{
    switch (room.type) {
    case SpecialRoomType::SafeCavern:
        return room.radius * 0.70f;
    case SpecialRoomType::CoinRoom:
        return room.radius * 0.64f;
    case SpecialRoomType::EnemyRoom:
        return room.radius * 0.56f;
    case SpecialRoomType::OreRoom:
        return room.radius * 0.34f;
    case SpecialRoomType::TreasureRoom:
        return room.radius * 0.40f;
    case SpecialRoomType::None:
        break;
    }
    return 0.0f;
}

std::vector<FloorCavernAnchor> collectFloorCavernAnchors(const DungeonLayout& layout)
{
    std::vector<FloorCavernAnchor> anchors;
    anchors.reserve(layout.warpPointAnchors.size() + layout.specialRoomAnchors.size() + 1);
    for (Vec2 anchor : layout.warpPointAnchors) {
        anchors.push_back(FloorCavernAnchor{
            .center = anchor,
            .radius = 2.4f,
        });
    }
    for (const SpecialRoomAnchor& room : layout.specialRoomAnchors) {
        const float radius = floorRadiusForRoom(room);
        if (radius <= 0.5f) {
            continue;
        }
        anchors.push_back(FloorCavernAnchor{
            .center = room.center,
            .radius = radius,
        });
    }
    anchors.push_back(FloorCavernAnchor{
        .center = {static_cast<float>(layout.goalTile.x), static_cast<float>(layout.goalTile.y)},
        .radius = 5.2f,
    });
    return anchors;
}

std::uint32_t placementTileHash(DungeonTile tile, std::uint32_t seed)
{
    std::uint32_t h = seed ^ 0xC2B2AE35u;
    h ^= static_cast<std::uint32_t>(tile.x) + 0x9E3779B9u + (h << 6) + (h >> 2);
    h ^= static_cast<std::uint32_t>(tile.y) + 0x85EBCA6Bu + (h << 6) + (h >> 2);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

TileType buriedPlacementTileType(DungeonTile tile, std::uint32_t seed, bool hidden, bool treasure)
{
    const std::uint32_t roll = placementTileHash(tile, seed) % 100u;
    if (treasure) {
        if (roll < 10u) {
            return TileType::Dirt;
        }
        if (roll < 20u) {
            return TileType::Ore;
        }
        return roll < 68u ? TileType::HardRock : TileType::Rock;
    }
    if (hidden) {
        if (roll < 35u) {
            return TileType::Dirt;
        }
        if (roll < 68u) {
            return TileType::Rock;
        }
        return roll < 88u ? TileType::HardRock : TileType::Ore;
    }
    if (roll < 70u) {
        return TileType::Dirt;
    }
    return roll < 90u ? TileType::Rock : TileType::Ore;
}

float wallPocketProgressForIndex(int index, int count, float jitter)
{
    return clamp(
        WallPocketProgressStart +
            WallPocketProgressSpan * (static_cast<float>(index + 1) / static_cast<float>(count + 1)) +
            jitter,
        0.08f,
        0.92f);
}

DungeonTile wallPocketTileAtProgress(const DungeonLayout& layout, float progress, float offsetTiles, bool positiveSide)
{
    const Vec2 anchor = pointAtPathProgress(layout.mainPathPoints, progress);
    const Vec2 tangent = tangentAtPathProgress(layout.mainPathPoints, progress);
    Vec2 side = perpendicular(tangent);
    if (!positiveSide) {
        side = side * -1.0f;
    }
    return roundDungeonTile(anchor + side * offsetTiles);
}

enum class MicroFeatureKind {
    OreNeedle,
    DoublePocketTreasure,
    BaitAndAmbush,
    CrateAlcove,
    OreVein,
};

enum class DoublePocketLootKind {
    Chest,
    Money,
    MoonFragment,
};

struct MicroFeature {
    MicroFeatureKind kind = MicroFeatureKind::OreNeedle;
    float progress = 0.0f;
    DungeonTile center{};
    DungeonTile entry{};
    DungeonTile second{};
    DungeonTile back{};
    DungeonTile tangentStep{};
    DungeonTile sideStep{};
};

float pathLengthTiles(const std::vector<Vec2>& points)
{
    float total = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        total += length(points[i] - points[i - 1]);
    }
    return total;
}

int signStep(float value)
{
    return value < 0.0f ? -1 : 1;
}

DungeonTile cardinalStep(Vec2 direction)
{
    if (std::abs(direction.x) >= std::abs(direction.y)) {
        return {signStep(direction.x), 0};
    }
    return {0, signStep(direction.y)};
}

DungeonTile addTile(DungeonTile tile, DungeonTile step, int scale = 1)
{
    return {tile.x + step.x * scale, tile.y + step.y * scale};
}

MicroFeatureKind chooseMicroFeatureKind(int index, std::mt19937& rng)
{
    if (index % 7 == 0) {
        return MicroFeatureKind::DoublePocketTreasure;
    }
    if (index % 5 == 0) {
        return MicroFeatureKind::OreVein;
    }

    std::discrete_distribution<int> distribution({
        4.0,
        3.0,
        3.0,
        2.0,
        4.0,
    });
    return static_cast<MicroFeatureKind>(distribution(rng));
}

float microFeatureOffsetTiles(MicroFeatureKind kind, float unit)
{
    switch (kind) {
    case MicroFeatureKind::OreNeedle:
        return 4.0f + unit * 2.6f;
    case MicroFeatureKind::DoublePocketTreasure:
        return 4.8f + unit * 2.8f;
    case MicroFeatureKind::BaitAndAmbush:
        return 4.6f + unit * 3.2f;
    case MicroFeatureKind::CrateAlcove:
        return 3.8f + unit * 2.4f;
    case MicroFeatureKind::OreVein:
        return 4.4f + unit * 3.4f;
    }
    return 5.0f;
}

std::vector<MicroFeature> microFeaturesForLayout(const DungeonLayout& layout)
{
    std::vector<MicroFeature> features;
    if (layout.mainPathPoints.size() < 2) {
        return features;
    }

    const int count = std::clamp(
        static_cast<int>(std::round(pathLengthTiles(layout.mainPathPoints) / MicroFeatureSpacingTiles)) + std::max(0, layout.stageId - 1),
        MicroFeatureMinCount,
        MicroFeatureMaxCount);
    features.reserve(static_cast<std::size_t>(count));

    std::mt19937 rng(layout.seed ^ 0x6D2B79F5u);
    std::uniform_real_distribution<float> progressJitter(-0.024f, 0.024f);
    std::uniform_real_distribution<float> unitDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> tangentDrift(-0.85f, 0.85f);
    std::uniform_int_distribution<int> signDist(0, 1);

    for (int i = 0; i < count; ++i) {
        const MicroFeatureKind kind = chooseMicroFeatureKind(i, rng);
        const float progress = clamp(
            MicroFeatureProgressStart +
                MicroFeatureProgressSpan * (static_cast<float>(i) + 0.5f) / static_cast<float>(count) +
                progressJitter(rng),
            0.06f,
            0.94f);
        const Vec2 anchor = pointAtPathProgress(layout.mainPathPoints, progress);
        const Vec2 tangent = tangentAtPathProgress(layout.mainPathPoints, progress);
        Vec2 side = perpendicular(tangent);
        if (signDist(rng) == 0) {
            side = side * -1.0f;
        }

        const DungeonTile tangentStep = cardinalStep(tangent);
        const DungeonTile sideStep = cardinalStep(side);
        const DungeonTile center = roundDungeonTile(
            anchor +
            side * microFeatureOffsetTiles(kind, unitDist(rng)) +
            tangent * tangentDrift(rng));

        features.push_back(MicroFeature{
            .kind = kind,
            .progress = progress,
            .center = center,
            .entry = addTile(center, sideStep, -1),
            .second = addTile(center, tangentStep),
            .back = addTile(center, sideStep),
            .tangentStep = tangentStep,
            .sideStep = sideStep,
        });
    }

    return features;
}

std::uint32_t microFeatureRoll(const MicroFeature& feature, std::uint32_t seed, std::uint32_t salt)
{
    return placementTileHash(feature.center, seed ^ salt);
}

DoublePocketLootKind doublePocketLootKind(const MicroFeature& feature, std::uint32_t seed)
{
    return static_cast<DoublePocketLootKind>(microFeatureRoll(feature, seed, 0xA92D4F17u) % 3u);
}

bool doublePocketUsesCrate(const MicroFeature& feature, std::uint32_t seed)
{
    return (microFeatureRoll(feature, seed, 0x3C7A6E21u) & 1u) != 0u;
}

bool baitUsesOreWall(const MicroFeature& feature, std::uint32_t seed)
{
    return (microFeatureRoll(feature, seed, 0x8F132B95u) & 1u) != 0u;
}

DungeonTile baitEnemyTile(const MicroFeature& feature)
{
    return addTile(feature.center, feature.sideStep, 2);
}

std::vector<DungeonTile> oreTilesForMicroFeature(const MicroFeature& feature)
{
    std::vector<DungeonTile> tiles;
    if (feature.kind == MicroFeatureKind::OreNeedle) {
        tiles.push_back(feature.center);
    } else if (feature.kind == MicroFeatureKind::OreVein) {
        tiles.push_back(feature.center);
        tiles.push_back(feature.second);
        tiles.push_back(addTile(feature.center, feature.tangentStep, -1));
        tiles.push_back(feature.back);
        tiles.push_back(addTile(feature.second, feature.sideStep));
    }
    return tiles;
}

}

DungeonGenerationContext Game::makeDungeonGenerationContext() const
{
    const int stageId = currentStage_ + 1;
    const StageDefinition& stage = currentStageDefinition();
    const bool stageIsRoguelike = stage.type == "ローグライク" || stage.generationProfile == "astral_rogue";
    return DungeonGenerationContext{
        .stageId = stageId,
        .seed = makeDungeonSeed(stageId, roguelikeDungeon_ || stageIsRoguelike),
        .stageHardnessMultiplier = static_cast<float>(std::max(0.25, stage.terrainHardnessMultiplier)),
        .goalDistanceTiles = stage.goalDistanceTiles,
        .detourRate = static_cast<float>(stage.detourRate),
        .branchDensity = static_cast<float>(stage.branchDensity),
        .cavernWidthMultiplier = static_cast<float>(stage.cavernWidthMultiplier),
        .warpPointCount = stage.warpPointCount,
        .specialRoomCount = stage.specialRoomCount,
        .generationProfile = stage.generationProfile,
        .terrainProfile = stage.terrainProfile,
        .roguelike = roguelikeDungeon_ || stageIsRoguelike,
    };
}

void Game::generateDungeonLayoutForRun()
{
    dungeonLayout_ = generateDungeonLayout(makeDungeonGenerationContext());
    initializeAstralRunForLayout();
}

bool Game::astralRunActive() const
{
    return astralRun_.active && currentStageIsRoguelike();
}

void Game::resetAstralRunState()
{
    astralRun_ = AstralRunState{};
    astralRun_.active = currentStageIsRoguelike();
    astralRun_.maxDepth = lootMaxDepthForStage(currentStageId_);
    astralRun_.maxReachedDepth = 1;
    astralRun_.currentDepth = 1;
    astralRun_.distortion = AstralDistortionKind::None;
}

Game::AstralDistortionKind Game::chooseAstralDistortionForDepth(int depth, Game::AstralDistortionKind previous) const
{
    if (debugAstralDistortionMode_ == "none") {
        return AstralDistortionKind::None;
    }
    if (debugAstralDistortionMode_ == "fading-starlight") {
        return AstralDistortionKind::FadingStarlight;
    }
    if (debugAstralDistortionMode_ == "star-hardened") {
        return AstralDistortionKind::StarHardened;
    }
    if (debugAstralDistortionMode_ == "echo-spawn") {
        return AstralDistortionKind::EchoSpawn;
    }

    std::uint32_t value = dungeonLayout_.seed ^
        (static_cast<std::uint32_t>(std::max(1, depth)) * 0x45D9F3Bu) ^
        0xA53C9E2Du;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;

    AstralDistortionKind chosen = AstralDistortionKind::FadingStarlight;
    switch (value % 3u) {
    case 0u:
        chosen = AstralDistortionKind::FadingStarlight;
        break;
    case 1u:
        chosen = AstralDistortionKind::StarHardened;
        break;
    default:
        chosen = AstralDistortionKind::EchoSpawn;
        break;
    }
    if (chosen == previous) {
        switch (chosen) {
        case AstralDistortionKind::FadingStarlight:
            return AstralDistortionKind::StarHardened;
        case AstralDistortionKind::StarHardened:
            return AstralDistortionKind::EchoSpawn;
        case AstralDistortionKind::EchoSpawn:
            return AstralDistortionKind::FadingStarlight;
        case AstralDistortionKind::None:
            break;
        }
    }
    return chosen;
}

float Game::astralLightRadiusMultiplier() const
{
    return astralRunActive() && astralRun_.distortion == AstralDistortionKind::FadingStarlight
        ? 0.85f
        : 1.0f;
}

float Game::astralHardnessMultiplier() const
{
    return astralRunActive() && astralRun_.distortion == AstralDistortionKind::StarHardened
        ? 1.20f
        : 1.0f;
}

RuntimeBalance Game::runtimeBalanceForDungeon() const
{
    RuntimeBalance adjusted = balance_;
    if (introTutorialActive()) {
        adjusted.playerLightRadius = std::min(adjusted.playerLightRadius, 28.0f);
        adjusted.playerLightRadius = witchSelfLightRadius(adjusted.playerLightRadius);
        adjusted.enemyMinDugTiles = 9999;
        adjusted.enemyGuaranteeDugTiles = 9999;
        return adjusted;
    }
    if (astralRunActive()) {
        const float lightMultiplier = astralLightRadiusMultiplier();
        adjusted.playerLightRadius = std::max(16.0f, adjusted.playerLightRadius * lightMultiplier);
        adjusted.lightRadius = std::max(16.0f, adjusted.lightRadius * lightMultiplier);
        if (astralRun_.distortion == AstralDistortionKind::EchoSpawn) {
            adjusted.enemyMinDugTiles = std::max(1, static_cast<int>(std::floor(static_cast<float>(adjusted.enemyMinDugTiles) * 0.75f)));
            adjusted.enemyGuaranteeDugTiles = std::max(
                adjusted.enemyMinDugTiles,
                static_cast<int>(std::floor(static_cast<float>(adjusted.enemyGuaranteeDugTiles) * 0.75f)));
        }
    }
    adjusted.playerLightRadius = witchSelfLightRadius(adjusted.playerLightRadius);
    return adjusted;
}

void Game::applyAstralDistortionToLayout()
{
    if (!astralRunActive()) {
        return;
    }
    dungeonLayout_.stageHardnessMultiplier = std::max(
        0.25f,
        astralRun_.baseStageHardnessMultiplier * astralHardnessMultiplier());
}

void Game::initializeAstralRunForLayout()
{
    if (!currentStageIsRoguelike()) {
        astralRun_ = AstralRunState{};
        return;
    }

    astralRun_.active = true;
    astralRun_.currentDepth = 1;
    astralRun_.maxReachedDepth = 1;
    astralRun_.maxDepth = lootMaxDepthForStage(currentStageId_);
    astralRun_.maxReachedDistanceTiles = 0;
    astralRun_.distortionChanges = 1;
    astralRun_.baseStageHardnessMultiplier = dungeonLayout_.stageHardnessMultiplier;
    astralRun_.distortion = chooseAstralDistortionForDepth(1, AstralDistortionKind::None);
    applyAstralDistortionToLayout();
    const char* distortionName = "なし";
    switch (astralRun_.distortion) {
    case AstralDistortionKind::FadingStarlight:
        distortionName = "星明かりが遠のく";
        break;
    case AstralDistortionKind::StarHardened:
        distortionName = "星硬化";
        break;
    case AstralDistortionKind::EchoSpawn:
        distortionName = "残響湧き";
        break;
    case AstralDistortionKind::None:
        break;
    }
    pushDungeonLog(std::string("星の歪み: ") + distortionName, "astral_distortion");
}

void Game::updateAstralRunProgress()
{
    if (!astralRunActive()) {
        return;
    }

    const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, {
        static_cast<float>(tileMap_.worldToTile(player_.position.x)),
        static_cast<float>(tileMap_.worldToTile(player_.position.y)),
    });
    const int depth = lootDepthRankForProgress(currentStageId_, metrics.pathProgress);
    astralRun_.maxReachedDepth = std::max(astralRun_.maxReachedDepth, depth);
    astralRun_.maxReachedDistanceTiles = std::max(
        astralRun_.maxReachedDistanceTiles,
        static_cast<int>(std::round(metrics.distanceFromStart)));

    if (depth <= astralRun_.currentDepth) {
        return;
    }

    astralRun_.currentDepth = depth;
    astralRun_.distortion = chooseAstralDistortionForDepth(depth, astralRun_.distortion);
    ++astralRun_.distortionChanges;
    applyAstralDistortionToLayout();
    const char* distortionName = "なし";
    switch (astralRun_.distortion) {
    case AstralDistortionKind::FadingStarlight:
        distortionName = "星明かりが遠のく";
        break;
    case AstralDistortionKind::StarHardened:
        distortionName = "星硬化";
        break;
    case AstralDistortionKind::EchoSpawn:
        distortionName = "残響湧き";
        break;
    case AstralDistortionKind::None:
        break;
    }
    pushDungeonLog(std::string("星の歪み: ") + distortionName, "astral_distortion");
}

void Game::refreshOrbitEffects()
{
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.clearOrbitModifiers();
    enemies_.clearSpawnBiases();
    player_.status.removeModifiersBySourcePrefix("orbit:");
    playerRegenPerSecond_ = 0.0;
    playerRegenSources_.clear();
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
        item.coldAirRadius = 0.0f;
        item.coldAirStrength = 0.0f;
        item.vacuumPullRadius = 0.0f;
        item.vacuumPullStrength = 0.0f;
        item.hotAirRadius = 0.0f;
        item.hotAirStrength = 0.0f;
        item.windPushRadius = 0.0f;
        item.windPushStrength = 0.0f;
        item.conductWaterPuddleRadius = 0.0f;
        item.conductWaterPuddleStrength = 0.0f;
        item.dryWetBonusDamage = 0;
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

        for (const EffectSpec& spec : objectIt->second.orbitEffects) {
            if (!isPlayerRegenTarget(spec.target)) {
                continue;
            }
            for (std::size_t index = 0; index < spec.effects.size(); ++index) {
                if (spec.effects[index] != "regen") {
                    continue;
                }
                const double rate = index < spec.values.size() ? std::max(0.0, spec.values[index]) : 0.0;
                if (rate <= 0.0) {
                    continue;
                }
                playerRegenPerSecond_ = std::min(PlayerRegenRateCap, playerRegenPerSecond_ + rate);
                playerRegenSources_.push_back(PlayerRegenSource{
                    .objectId = objectIt->second.id,
                    .objectName = objectIt->second.name,
                    .position = item.worldPosition,
                    .ratePerSecond = rate,
                });
            }
        }

        EffectContext context;
        context.sourceObject = &objectIt->second;
        context.owner = &player_;
        context.orbit = &spellRing_;
        context.orbitItem = &item;
        context.effects = &effects_;
        context.enemies = &enemies_;
        context.magic = &magic_;
        context.encyclopedia = &encyclopedia_;
        context.position = item.worldPosition;
        context.triggerType = EffectTriggerType::Orbit;
        context.logUnimplementedEffects = false;
        effectDispatcher_.dispatchOrbitEffects(objectIt->second, context);
        const RingEquipmentModifiers& equipment =
            spellRing_.equipmentModifiersForRing(item.ringIndex);
        item.lightRadius *= static_cast<float>(std::max(0.0, equipment.lightRadiusMul));
        item.hiddenDetectionRadius *= static_cast<float>(std::max(0.0, equipment.detectRangeMul));
        item.treasureDetectionRadius *= static_cast<float>(std::max(0.0, equipment.detectRangeMul));
    }
}

void Game::updatePlayerRegen(float dt, std::vector<EffectDiscoveryEvent>& discoveryEvents)
{
    if (dt <= 0.0f) {
        return;
    }

    const double ratePerSecond = std::clamp(playerRegenPerSecond_, 0.0, PlayerRegenRateCap);
    if (ratePerSecond <= 0.0 || player_.hp <= 0 || player_.hp >= player_.maxHp) {
        playerRegenAccumulator_ = 0.0;
        return;
    }

    playerRegenAccumulator_ += ratePerSecond * static_cast<double>(dt);
    const int requestedHeal = static_cast<int>(std::floor(playerRegenAccumulator_));
    if (requestedHeal <= 0) {
        return;
    }

    const int healed = player_.heal(requestedHeal);
    if (healed <= 0) {
        return;
    }
    playerRegenAccumulator_ = std::max(0.0, playerRegenAccumulator_ - static_cast<double>(healed));

    for (const PlayerRegenSource& source : playerRegenSources_) {
        if (source.objectId.empty() || source.ratePerSecond <= 0.0) {
            continue;
        }
        if (encyclopedia_.hasObjectEffect(source.objectId, "regen")) {
            continue;
        }
        if (discoveryQueueContainsObjectEffect(discoveryEvents, source.objectId, "regen")) {
            continue;
        }
        discoveryEvents.push_back(EffectDiscoveryEvent{
            .objectId = source.objectId,
            .objectName = source.objectName,
            .effectKey = "regen",
            .description = {},
            .note = {},
            .position = source.position,
        });
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
        const Vec2 outward = item.orbitOutward;
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
            const bool affectMetal = targetTag.empty() || tagListContains(targetTag, "metal");
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

void Game::updateWetGroundFromStatus()
{
    if (player_.status.hasState("status_wet")) {
        const float playerWetRadius = std::clamp(
            player_.effectiveRadius(balance_.playerRadius) * WetGroundPlayerRadiusMultiplier,
            WetGroundPlayerMinRadius,
            WetGroundPlayerMaxRadius);
        wetGround_.touchSource("player", player_.position, playerWetRadius);
    }

    std::vector<WetGroundEmitter> emitters;
    enemies_.appendWetGroundEmitters(emitters);
    for (const WetGroundEmitter& emitter : emitters) {
        wetGround_.touchSource(emitter.sourceKey, emitter.position, emitter.radius, emitter.strength);
    }
}

void Game::updateAmbientParticleEffects(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    const bool lightweight = lightweightModeEnabled();
    ringTrailEffectTimer_ -= dt;
    const int throwingRingIndex = spellRing_.throwingRingIndex();
    if (throwingRingIndex >= 0 && ringTrailEffectTimer_ <= 0.0f) {
        const Vec2 ringCenter = spellRing_.centerForRing(throwingRingIndex);
        const Vec2 trailDirection = spellRing_.stateForRing(throwingRingIndex) == SpellRingState::Thrown
            ? player_.facing
            : player_.position - ringCenter;
        effects_.spawnForegroundRingTrail(ringCenter, trailDirection);
        if (!lightweight) {
            for (const SpellRingItem& item : spellRing_.itemsForRing(throwingRingIndex)) {
                effects_.spawnForegroundRingTrail(item.worldPosition, trailDirection);
            }
        }
        ringTrailEffectTimer_ = lightweight ? 0.11f : 0.055f;
    }

    ambientParticleTimer_ -= dt;
    if (ambientParticleTimer_ > 0.0f) {
        return;
    }
    ambientParticleTimer_ = lightweight ? 0.36f : 0.18f;

    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->broken()) {
            continue;
        }
        if (itemPtr->type == SpellRingItemType::Torch || itemPtr->lightRadius > 0.0f) {
            effects_.spawnForegroundTorchFlicker(itemPtr->worldPosition);
        }
        if (!lightweight &&
            (!itemPtr->objectId.empty() || !itemPtr->addedEffects.empty() || itemPtr->hiddenDetectionRadius > 0.0f || itemPtr->treasureDetectionRadius > 0.0f)) {
            effects_.spawnForegroundSpecialItemGlimmer(itemPtr->worldPosition);
        }
    }

    emitEntityStatusAuras(player_.status, player_.position, effects_);
    enemies_.emitStatusParticles(effects_);
    updateWetGroundFromStatus();

    if (warpPointsEnabled_) {
        if (!lightweight) {
            for (const WarpPoint& point : warpPoints_) {
                if (point.discovered || point.unlocked) {
                    effects_.spawnWarpCircle(point.position, false);
                }
            }
        }
        if (hasBossSpawnPoint_ && !bossSpawned_ && !hasCapturedBossForCurrentStage()) {
            effects_.spawnWarpCircle(bossSpawnPoint_, true);
        }
    }
}

bool Game::handleCaptureResult(const CaptureResult& capture)
{
    if (capture.type == CaptureResultType::NoTarget) {
        return false;
    }

    if (capture.type == CaptureResultType::Success) {
        playAudioSe(AudioSeCaptureSuccess);
        if (!capture.capturedEnemy.enemyId.empty()) {
            encyclopedia_.noteEnemyDiscovered(
                capture.capturedEnemy.enemyId,
                capture.capturedEnemy.enemyName,
                capture.position);
        }
        startCaptureAbsorbAnimation(capture);
        return true;
    }

    playAudioSe(AudioSeCaptureFail);
    if (capture.type == CaptureResultType::OutOfRange) {
        pushDungeonLog("虫とりアミ: 遠すぎる", "capture_out_of_range");
    } else if (capture.type == CaptureResultType::InventoryFull) {
        pushDungeonLog("虫とりアミ: 持ち物がいっぱい", "capture_inventory_full");
    } else if (capture.type == CaptureResultType::BossLocked) {
        pushDungeonLog("虫とりアミ: 初回ボスは捕獲できない", "capture_boss_locked");
    } else if (capture.type == CaptureResultType::BossAlreadyOwned) {
        pushDungeonLog("虫とりアミ: 捕獲中のボスは再捕獲できない", "capture_boss_owned");
    } else if (capture.type == CaptureResultType::Failed) {
        pushDungeonLog("虫とりアミ: 逃げられた", "capture_failed:" + capture.enemyName);
    }
    return false;
}

void Game::startCaptureAbsorbAnimation(const CaptureResult& capture)
{
    if (capture.capturedItem.id.empty()) {
        return;
    }

    CaptureAbsorbAnimation animation;
    animation.enemy = capture.capturedEnemy;
    animation.enemy.active = true;
    animation.enemy.position = capture.position;
    animation.enemy.velocity = {};
    animation.enemy.knockbackVelocity = {};
    animation.item = capture.capturedItem;
    animation.startPosition = capture.position;
    animation.lastPosition = capture.position;
    animation.durationSeconds = CaptureAbsorbDurationSeconds;
    animation.flyDelaySeconds = CaptureAbsorbFlyDelaySeconds;
    animation.sparkleTimer = 0.0f;
    captureAbsorbAnimations_.push_back(std::move(animation));

    effects_.spawnCaptureSuccess(capture.position, player_.position - capture.position);
}

Vec2 Game::captureAbsorbPosition(const CaptureAbsorbAnimation& animation, Vec2 targetPosition) const
{
    const float flyProgress = captureAbsorbFlyProgress(
        animation.elapsedSeconds,
        animation.flyDelaySeconds,
        animation.durationSeconds);
    return captureAbsorbCurvePosition(animation.startPosition, targetPosition, flyProgress);
}

void Game::finalizeCaptureAbsorbAnimation(const CaptureAbsorbAnimation& animation)
{
    if (animation.item.id.empty()) {
        return;
    }

    upsertObjectDefinition(objectCatalog_, animation.item);

    InventoryAddResult addResult;
    if (inventory_.addRuntimeObjectItem(animation.item, &addResult)) {
        ++runStats_.acquiredItems;
        ++runStats_.acquiredObjectItems;
        recordObjectObtainedForFirstNotice(
            animation.item.id,
            addResult.instanceId,
            addResult.kind == InventoryAddKind::Instance && !addResult.instanceId.empty(),
            player_.position);
        pushDungeonLog(animation.enemy.enemyName + " を捕まえた", "capture_success:" + animation.enemy.enemyName);
    } else {
        ItemInstance instance = inventory_.createDetachedObjectInstance(animation.item);
        std::mt19937& rng = lootRuntimeRng();
        const Vec2 dropPosition = scatterLootPosition(player_.position, rng);
        const bool dropped = worldDrops_.spawnObjectInstanceDrop(
            objectCatalog_,
            std::move(instance),
            dropPosition,
            runStats_.elapsedSeconds,
            makeWorldLootJumpMotion(player_.position, rng));
        if (dropped) {
            pushDungeonLog(
                "リュックがいっぱいなので " + animation.enemy.enemyName + " は地面に落ちた",
                "capture_drop_full:" + animation.enemy.enemyName);
        } else {
            pushDungeonLog(
                "虫とりアミ: " + animation.enemy.enemyName + " を収納できませんでした",
                "capture_drop_failed:" + animation.enemy.enemyName);
        }
    }

    effects_.spawnDropPickup(player_.position, animation.startPosition - player_.position);
    playAudioSe(AudioSePickup);
}

void Game::updateCaptureAbsorbAnimations(float dt)
{
    if (captureAbsorbAnimations_.empty()) {
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    for (CaptureAbsorbAnimation& animation : captureAbsorbAnimations_) {
        animation.elapsedSeconds += safeDt;
        animation.lastPosition = captureAbsorbPosition(animation, player_.position);

        animation.sparkleTimer -= safeDt;
        if (animation.sparkleTimer <= 0.0f) {
            effects_.spawnForegroundSpecialItemGlimmer(animation.lastPosition);
            if (animation.elapsedSeconds > animation.flyDelaySeconds) {
                effects_.spawnForegroundRingTrail(animation.lastPosition, player_.position - animation.lastPosition);
            }
            animation.sparkleTimer = CaptureAbsorbSparkIntervalSeconds;
        }
    }

    auto removeBegin = std::remove_if(
        captureAbsorbAnimations_.begin(),
        captureAbsorbAnimations_.end(),
        [this](const CaptureAbsorbAnimation& animation) {
            if (animation.elapsedSeconds < animation.durationSeconds) {
                return false;
            }
            finalizeCaptureAbsorbAnimation(animation);
            return true;
        });
    captureAbsorbAnimations_.erase(removeBegin, captureAbsorbAnimations_.end());
}

void Game::handleCapturedExplosion(Vec2 position)
{
    effects_.spawnAreaPulse(position, 50.0f, {255, 128, 54, 190});
    const std::vector<DamagedTile> openedTiles = tileMap_.damageCircle(position, CapturedExplosionTileRadius, CapturedExplosionTileDamage);
    std::vector<Vec2> openedTileCenters;
    openedTileCenters.reserve(openedTiles.size());
    for (const DamagedTile& tile : openedTiles) {
        effects_.spawnTileBreak(tile.center, tile.type, tile.color);
        openedTileCenters.push_back(tile.center);
        ++runStats_.dugTiles;
    }
    revealDungeonMinimapOpenedTiles(openedTileCenters);
    enemies_.applyCapturedExplosion(position, spellRing_, CapturedExplosionEnemyDamage);
}

void Game::resize(int width, int height)
{
    (void)width;
    (void)height;
    camera_.setViewport(balance::ScreenWidth, balance::ScreenHeight);
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
        openOptionsMenu();
        break;
    case 4:
        pausePage_ = PauseMenuPage::QuitConfirm;
        openUiConfirmDialog(
            pauseQuitConfirm_,
            "確認",
            "ゲームを終了しますか？\nセーブは拠点でのみ実行できます。",
            "終了する",
            "戻る",
            1);
        break;
    default:
        break;
    }
}

void Game::leavePausePage()
{
    if (pausePage_ == PauseMenuPage::Main) {
        pauseQuitConfirm_ = {};
        mode_ = pauseReturnMode_;
        return;
    }

    if (pausePage_ == PauseMenuPage::QuitConfirm) {
        pauseQuitConfirm_ = {};
    } else if (pausePage_ == PauseMenuPage::Options) {
        operationSettingsCapture_.cancel();
        operationSettingsConflictConfirm_ = {};
        operationSettingsResetAllConfirm_ = {};
        operationSettingsPendingAction_ = InputAction::Count;
        operationSettingsConflictActions_.clear();
        optionsSettingsLoaded_ = false;
        operationSettingsLoaded_ = false;
    }
    pausePage_ = PauseMenuPage::Main;
}

void Game::openRingScreen()
{
    pausePage_ = PauseMenuPage::Main;
    mode_ = ScreenMode::Ring;
    const int maxIndex = std::max(0, static_cast<int>(spellRing_.items().size()) - 1);
    ringTabs_.focusedIndex = spellRing_.activeRingIndex();
    ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, maxIndex);
    ringDetailShowsRing_ = true;
    ringDragPending_ = false;
    ringDragActive_ = false;
    ringSnapActive_ = false;
    ringDragItemIndex_ = -1;
    closeUiCommandMenu(ringCommandMenu_);
    ringCommandItemIndex_ = -1;
    ringCommandPlaceActive_ = false;
    ringPlaceModeActive_ = false;
    ringEmptyPressActive_ = false;
    ringItemMoveModeActive_ = false;
    ringItemMoveIndex_ = -1;
    cancelRingGrab();
    ringStatus_.clear();
}

void Game::cancelRingGrab()
{
    ringItemMoveModeActive_ = false;
    ringItemMoveIndex_ = -1;
    if (!ringGrabActive_) {
        return;
    }

    ringDragPending_ = false;
    ringDragActive_ = false;
    ringSnapActive_ = false;
    ringDragItemIndex_ = -1;
    closeUiCommandMenu(ringCommandMenu_);
    ringCommandItemIndex_ = -1;
    ringCommandPlaceActive_ = false;
    ringPlaceModeActive_ = false;
    ringEmptyPressActive_ = false;
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
    state.money = money_;
    state.valid = true;
    return state;
}

void Game::restoreInventoryCarryState(const InventoryCarryState& state)
{
    if (!state.valid) {
        return;
    }

    inventory_ = state.inventory;
    money_ = std::max(0, state.money);
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    spellRing_.ringItems() = state.ringItemsByRing;
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.normalizeItemPlacements();
    observeRingItemInstanceIds();
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
    player_.hotDamageAccumulator = 0.0;
    player_.bleedDamageAccumulator = 0.0;
    player_.stunWakeTimer = 0.0f;
    player_.lastDamageSource = DamageSource::Unknown;
    player_.lastDamageEnemyName.clear();
    player_.damageFlash = 0.0f;
    player_.damageEvents.clear();
    player_.healEvents.clear();
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

Vec2 Game::warpPointStartPositionForCurrentRequest() const
{
    if (requestedWarpPointStartPosition_.has_value()) {
        return *requestedWarpPointStartPosition_;
    }
    return latestWarpPointStartPosition();
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
    if (!warpPoints_.empty() && unlockedWarpPointCount_ >= static_cast<int>(warpPoints_.size())) {
        configureBossSpawnPointFromWarp(latestPosition);
    }
    clearKnownWarpPointTerrain();
}

void Game::retryAfterGameOver()
{
    if (introTutorialActive()) {
        startIntroTutorialDungeon();
        return;
    }

    const RetrySnapshot checkpoint = retrySnapshot_;
    if (checkpoint.valid) {
        initializeWorld(false);
        retrySnapshot_ = checkpoint;
        restoreRetrySnapshot();
        clearTemporaryPlayerState(true);
        mode_ = ScreenMode::Playing;
        beginDungeonRingIntro();
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
    captureAbsorbAnimations_.clear();
    groundLines_ = GroundLineSystem{};
    wetGround_ = WetGroundSystem{};
    projectiles_ = ProjectileSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    levels_ = LevelSystem{};
    levelUpPresentation_ = {};
    tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
    normalizeOpenBuriedPlacementNodes();
    updateDungeonMinimap(0.0);
    camera_.follow(player_.position, 1.0f);
    captureRunStartInventoryState();
    mode_ = ScreenMode::Playing;
    beginDungeonRingIntro();
}

void Game::returnToBaseAfterGameOver()
{
    if (introTutorialActive()) {
        startIntroTutorialDungeon();
        return;
    }

    returnToBaseFromNormalStage(false, true);
}

int Game::astralRunMaterialDeltaFromStart() const
{
    if (!runStartInventoryState_.valid) {
        return 0;
    }

    int total = 0;
    for (int index = 0; index < static_cast<int>(MaterialType::Count); ++index) {
        const MaterialType type = static_cast<MaterialType>(index);
        total += std::max(
            0,
            inventory_.materialCount(type) - runStartInventoryState_.inventory.materialCount(type));
    }
    return total;
}

int Game::astralRunMoneyDeltaFromStart() const
{
    if (!runStartInventoryState_.valid) {
        return 0;
    }
    return std::max(0, money_ - runStartInventoryState_.money);
}

int Game::calculateAstralRunScore(const Game::AstralRunSummary& summary) const
{
    if (summary.result == AstralRunResult::Died) {
        return 0;
    }

    const int resultBonus = summary.result == AstralRunResult::DragonDefeated ? 10000 : 3000;
    return std::max(0,
        summary.reachedDepth * 1000 +
        summary.reachedDistanceTiles * 2 +
        summary.defeatedEnemies * 120 +
        summary.dugTiles * 3 +
        summary.acquiredItems * 250 +
        summary.acquiredMaterials * 40 +
        summary.acquiredMoney +
        resultBonus);
}

Game::AstralRunSummary Game::makeAstralRunSummary(Game::AstralRunResult result) const
{
    AstralRunSummary summary;
    summary.result = result;
    summary.reachedDepth = std::clamp(astralRun_.maxReachedDepth, 1, std::max(1, astralRun_.maxDepth));
    summary.maxDepth = std::max(1, astralRun_.maxDepth);
    summary.reachedDistanceTiles = std::max(0, astralRun_.maxReachedDistanceTiles);
    summary.defeatedEnemies = std::max(0, runStats_.defeatedEnemies);
    summary.dugTiles = std::max(0, runStats_.dugTiles);
    summary.acquiredItems = std::max(0, runStats_.acquiredObjectItems);
    summary.acquiredMaterials = astralRunMaterialDeltaFromStart();
    summary.acquiredMoney = astralRunMoneyDeltaFromStart();
    summary.carriedOut = result != AstralRunResult::Died;
    summary.score = calculateAstralRunScore(summary);
    summary.highScore = astralHighScore_;
    return summary;
}

void Game::enterAstralResult(Game::AstralRunResult result)
{
    if (mode_ == ScreenMode::AstralResult) {
        return;
    }

    resetBossEncounter();
    if (result == AstralRunResult::Died) {
        player_.hp = 0;
    } else {
        clearTemporaryPlayerState(true);
    }
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    closeDebugItemPicker();
    closeDebugStoryTest();
    debugStoryTestReturnAfterDialogue_ = false;
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    levelUpPresentation_ = {};
    levelUpResultDialog_ = {};

    astralResult_ = makeAstralRunSummary(result);
    if (astralResult_.carriedOut && astralResult_.score > astralHighScore_) {
        astralHighScore_ = astralResult_.score;
        astralResult_.highScore = astralHighScore_;
        astralResult_.highScoreUpdated = true;
    }

    mode_ = ScreenMode::AstralResult;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    astralResultSelection_ = 0;
}

void Game::returnToBaseAfterAstralResult()
{
    const bool dragonDefeated = astralResult_.result == AstralRunResult::DragonDefeated;
    const bool died = astralResult_.result == AstralRunResult::Died;
    requestReturnToBaseTransition(dragonDefeated, died);
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

    const std::string returnedStageId = currentStageId_;
    if (currentStageIsRoguelike() && died && restoreRunStartInventoryOnDeath_ && runStartInventoryState_.valid) {
        restoreInventoryCarryState(runStartInventoryState_);
    }
    const bool refreshMerchant = shouldRefreshMerchantOnReturn(stageCleared, died);
    merchantRefreshPending_ = merchantRefreshPending_ || refreshMerchant;
    clearTemporaryPlayerState(true);
    captureDungeonState();
    enemies_ = EnemySystem{};
    effects_ = EffectSystem{};
    captureAbsorbAnimations_.clear();
    groundLines_ = GroundLineSystem{};
    wetGround_ = WetGroundSystem{};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
    projectiles_ = ProjectileSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    levels_ = LevelSystem{};
    levelUpPresentation_ = {};
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    warpReturnConfirm_ = {};
    focusedWarpReturnPointIndex_ = -1;
    bossSpawned_ = false;
    hasBossSpawnPoint_ = false;
    resetBossEncounter();
    retrySnapshot_ = RetrySnapshot{};
    warpPoints_.clear();
    spawnedWarpPointCount_ = 0;
    placeBasePlayerAtMineExitReturnPoint();
    enterBase();
    baseStatus_ = testPlayMode_
        ? (refreshMerchant ? "帰還しました。商人ワゴン更新あり" : "帰還しました")
        : std::string{};
    if (autoSaveOnReturn_) {
        std::string message;
        if (saveSaveData(message)) {
            if (testPlayMode_) {
                baseStatus_ += " / 自動保存";
            }
        } else if (testPlayMode_) {
            baseStatus_ += " / " + message;
        }
    }
    if (stageCleared && !returnedStageId.empty()) {
        if (returnedStageId == FinalStoryStageId) {
            if (hasStoryFlag(EndingSeenFlag) && !hasStoryFlag(PostEndingIntroFlag)) {
                queueStoryEventForTrigger("post_ending:intro");
            } else if (!hasStoryFlag(EndingSeenFlag)) {
                queueStoryEventForTrigger("ending:main");
            }
            return;
        }
        queueStoryEventForTrigger("stage_clear:" + returnedStageId);
    }
}

void Game::startIntroTutorialDungeon()
{
    resetWorldSimulationState();
    resetWorldUiState();
    resetWorldRunState();
    tileMap_.clearDamageProtectionAreas();

    pendingStoryTrigger_.clear();
    pendingStoryTriggerDelaySeconds_ = 0.0f;
    pendingStoryTriggers_.clear();
    requestedWarpPointStartPosition_.reset();

    currentStageDefinition_ = makeIntroTutorialStageDefinition();
    currentStageId_ = currentStageDefinition_.id;
    currentStage_ = 0;
    roguelikeDungeon_ = false;
    restoreRunStartInventoryOnDeath_ = false;
    roguelikeCarryInRestricted_ = false;
    roguelikeCarryOutRestricted_ = false;
    warpPointsEnabled_ = false;
    astralRun_ = AstralRunState{};

    introTutorialPhase_ = IntroTutorialPhase::FallDialogue;
    introTutorialLightTutorialQueued_ = false;
    introTutorialFirstEnemySpawned_ = false;
    introTutorialSecondEnemySpawned_ = false;
    introTutorialSecondEnemyEncountered_ = false;
    introTutorialEnemyEncounterQueued_ = false;
    introTutorialEnemyDefeatedQueued_ = false;
    introTutorialChestFoundQueued_ = false;
    introTutorialSecondChestPlaced_ = false;
    introTutorialChestOpened_ = false;
    introTutorialChestLootPending_ = false;
    introTutorialChestLootDialogueQueued_ = false;
    introTutorialMidwayDialogueQueued_ = false;
    introTutorialExitDialogueQueued_ = false;
    introTutorialFirstEnemyRuntimeId_ = 0;
    introTutorialSecondEnemyRuntimeId_ = 0;
    introTutorialChestLootObjectId_.clear();
    introTutorialChestLootInstanceId_.clear();
    introTutorialFirstEnemyTile_ = {29, 0};
    introTutorialSecondEnemyTile_ = {52, 2};
    introTutorialChestTile_ = introTutorialFirstEnemyTile_;
    introTutorialSecondChestTile_ = {55, 2};
    introTutorialExitTile_ = {77, 2};

    dungeonLayout_ = generateDungeonLayout(DungeonGenerationContext{
        .stageId = 1,
        .seed = IntroTutorialSeed,
        .stageHardnessMultiplier = static_cast<float>(currentStageDefinition_.terrainHardnessMultiplier),
        .goalDistanceTiles = currentStageDefinition_.goalDistanceTiles,
        .detourRate = static_cast<float>(currentStageDefinition_.detourRate),
        .branchDensity = static_cast<float>(currentStageDefinition_.branchDensity),
        .cavernWidthMultiplier = static_cast<float>(currentStageDefinition_.cavernWidthMultiplier),
        .warpPointCount = 0,
        .specialRoomCount = 0,
        .generationProfile = currentStageDefinition_.generationProfile,
        .terrainProfile = currentStageDefinition_.terrainProfile,
        .roguelike = false,
    });

    player_.position = tileWorldCenter(DungeonTile{dungeonLayout_.startTile.x + 2, dungeonLayout_.startTile.y});
    player_.facing = {1.0f, 0.0f};
    player_.minimumHpAfterDamage = 1;
    player_.xpToNext = playerXpToNextForLevel(player_.level, balance_);

    rewardNodes_.clear();
    moneyNodes_.clear();
    moonFragmentNodes_.clear();
    chestNodes_.clear();
    crateNodes_.clear();
    enemyNodes_.clear();
    enemyNodes_.push_back(EnemyNode{
        .tile = introTutorialFirstEnemyTile_,
        .placementType = EnemyPlacementType::Exposed,
        .dangerTier = 1,
        .enemySpawnGroup = std::string(IntroTutorialSlimeGroup),
        .spawned = false,
    });
    enemyNodes_.push_back(EnemyNode{
        .tile = introTutorialSecondEnemyTile_,
        .placementType = EnemyPlacementType::Exposed,
        .dangerTier = 1,
        .enemySpawnGroup = std::string(IntroTutorialMushroomGroup),
        .spawned = false,
    });
    warpPoints_.clear();
    dungeonEvents_.clear();
    spawnedWarpPointCount_ = 0;
    unlockedWarpPointCount_ = 0;
    hasLatestWarpPointPosition_ = false;
    latestWarpPointPosition_ = {};

    spellRing_.initialize(balance_);
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        spellRing_.itemsForRing(ringIndex).clear();
    }
    refreshEquipmentModifiers();
    applyPermanentUpgrades();
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    spellRing_.resetRuntimeStateAtPlayer(player_, balance_);

    tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
    applyIntroTutorialRubbleGate(tileMap_, dungeonLayout_);
    normalizeOpenBuriedPlacementNodes();
    updateDungeonMinimap(0.0);
    camera_.follow(player_.position, 1.0f);
    captureRunStartInventoryState();

    mode_ = ScreenMode::Playing;
    pauseReturnMode_ = ScreenMode::Playing;
    baseEditEnabled_ = false;
    baseEditMode_ = BaseEditMode::None;
    resetPlayerFootstepDust();
    playAudioBgm(AudioBgmDungeon, 0.45f);
    pendingStoryTrigger_ = std::string(IntroTutorialFallTrigger);
}

bool Game::introTutorialActive() const
{
    return introTutorialPhase_ != IntroTutorialPhase::Inactive;
}

Vec2 Game::introTutorialExitPosition() const
{
    const DungeonTile tile = introTutorialExitTile_.x != 0 || introTutorialExitTile_.y != 0
        ? introTutorialExitTile_
        : dungeonLayout_.goalTile;
    return tileWorldCenter(tile);
}

std::vector<LightSource> Game::introTutorialLightSources(double totalSeconds) const
{
    std::vector<LightSource> lights;
    if (!introTutorialActive()) {
        return lights;
    }

    const float tileSize = static_cast<float>(balance::TileSize);
    const float pulse = 1.0f + std::sin(static_cast<float>(totalSeconds) * 1.7f) * 0.035f;
    const auto add = [&](DungeonTile tile, float radiusTiles) {
        lights.push_back({tileWorldCenter(tile), radiusTiles * tileSize * pulse});
    };
    const DungeonTile startTile = dungeonLayout_.startTile;
    add(startTile, 7.0f);
    add({startTile.x + 3, startTile.y}, 7.0f);
    add(introTutorialFirstEnemyTile_, 7.0f);
    add({introTutorialSecondEnemyTile_.x + 2, introTutorialSecondEnemyTile_.y}, 7.0f);
    add(introTutorialExitTile_, IntroTutorialExitLightRadiusTiles);
    return lights;
}

void Game::equipIntroTutorialStartingTools()
{
    spellRing_.initialize(balance_);
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        spellRing_.itemsForRing(ringIndex).clear();
    }

    const ItemData* shovel = objectCatalog_.registry.findById(IntroTutorialShovelObjectId);
    if (shovel != nullptr) {
        const ItemInstance shovelInstance = inventory_.createDetachedObjectInstance(*shovel);
        SpellRingAddResult result{};
        if (!spellRing_.addObjectItem(*shovel, shovelInstance, &result)) {
            spellRing_.initialize(balance_);
        } else {
            std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(result.ringIndex);
            if (result.itemIndex >= 0 && result.itemIndex < static_cast<int>(ringItems.size())) {
                SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(result.itemIndex)];
                ringItem.localAngle = 0.0f;
                ringItem.durabilityLocked = true;
            }
        }
    }
    refreshEquipmentModifiers();
    applyPermanentUpgrades();
    spellRing_.applyObjectParameters(objectCatalog_);
    observeRingItemInstanceIds();
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    beginDungeonRingIntro();
    introTutorialPhase_ = IntroTutorialPhase::ShovelRingIntro;
}

void Game::addIntroTutorialTorchToRing()
{
    const ItemData* torch = objectCatalog_.registry.findById(IntroTutorialTorchObjectId);
    if (torch != nullptr) {
        const ItemInstance torchInstance = inventory_.createDetachedObjectInstance(*torch);
        SpellRingAddResult result{};
        if (spellRing_.addObjectItem(*torch, torchInstance, &result)) {
            std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(result.ringIndex);
            if (result.itemIndex >= 0 && result.itemIndex < static_cast<int>(ringItems.size())) {
                ringItems[static_cast<std::size_t>(result.itemIndex)].localAngle = Pi;
            }
        }
    }
    refreshEquipmentModifiers();
    applyPermanentUpgrades();
    spellRing_.applyObjectParameters(objectCatalog_);
    observeRingItemInstanceIds();
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    beginDungeonRingIntro();
    introTutorialPhase_ = IntroTutorialPhase::TorchRingIntro;
}

void Game::startIntroTutorialEnemyEncounterEvent()
{
    addStoryFlag(std::string(IntroTutorialEnemyEncounterFlag));
    DialogueSequence sequence = singleLineDialogueSequence(
        "intro_tutorial_enemy_encounter_intro",
        "player",
        "ルネ",
        "うわ！モンスターだ！");
    if (!startDialogueSequenceWithCompletion(std::move(sequence), [this]() {
            startIntroTutorialSlimeFocusDialogue();
        })) {
        startIntroTutorialSlimeFocusDialogue();
    }
}

void Game::startIntroTutorialSlimeFocusDialogue()
{
    Vec2 focusPosition = tileWorldCenter(introTutorialFirstEnemyTile_);
    enemies_.runtimeEnemyPosition(introTutorialFirstEnemyRuntimeId_, focusPosition);

    DungeonFocusRequest request;
    request.eventKind = "intro_tutorial_slime_encounter";
    request.focusWorldPos = focusPosition;
    request.discoveryDialogue = singleLineDialogueSequence(
        "intro_tutorial_enemy_encounter_slime",
        "slime",
        "スライム",
        "グヘヘヘ！襲ってやるぜ～！");
    request.holdSecondsIfNoDialogue = 0.0f;
    request.moveSeconds = 0.55f;
    request.returnSeconds = 0.55f;
    request.onComplete = [this]() {
        startIntroTutorialEnemyRetreatDialogue();
    };

    if (requestDungeonFocus(std::move(request))) {
        return;
    }

    DialogueSequence fallback = singleLineDialogueSequence(
        "intro_tutorial_enemy_encounter_slime",
        "slime",
        "スライム",
        "グヘヘヘ！襲ってやるぜ～！");
    if (!startDialogueSequenceWithCompletion(std::move(fallback), [this]() {
            startIntroTutorialEnemyRetreatDialogue();
        })) {
        startIntroTutorialEnemyRetreatDialogue();
    }
}

void Game::startIntroTutorialEnemyRetreatDialogue()
{
    DialogueSequence sequence = singleLineDialogueSequence(
        "intro_tutorial_enemy_encounter_retreat",
        "player",
        "ルネ",
        "こっちに来ないで～！");
    startDialogueSequenceWithCompletion(std::move(sequence), {});
}

void Game::spawnIntroTutorialChest()
{
    if (!introTutorialActive()) {
        return;
    }

    const auto hasChestAt = [&](DungeonTile tile) {
        return std::any_of(chestNodes_.begin(), chestNodes_.end(), [&](const ChestNode& node) {
            return sameDungeonTile(node.tile, tile);
        });
    };
    if (hasChestAt(introTutorialChestTile_)) {
        return;
    }

    carveTutorialPocket(tileMap_, introTutorialChestTile_, 1);
    spawnAppearingChestNode(
        introTutorialChestTile_,
        LootChestKind::Common,
        1,
        tileWorldCenter(introTutorialChestTile_),
        "intro_tutorial_chest");
}

void Game::spawnIntroTutorialSecondChest()
{
    if (!introTutorialActive() || introTutorialSecondChestPlaced_) {
        return;
    }

    const auto hasChestAt = [&](DungeonTile tile) {
        return std::any_of(chestNodes_.begin(), chestNodes_.end(), [&](const ChestNode& node) {
            return sameDungeonTile(node.tile, tile);
        });
    };
    if (hasChestAt(introTutorialSecondChestTile_)) {
        introTutorialSecondChestPlaced_ = true;
        return;
    }

    carveTutorialPocket(tileMap_, introTutorialSecondChestTile_, 1);
    spawnAppearingChestNode(
        introTutorialSecondChestTile_,
        LootChestKind::Common,
        1,
        tileWorldCenter(introTutorialSecondChestTile_),
        std::string_view{});
    introTutorialSecondChestPlaced_ = true;
}

void Game::unlockIntroTutorialFreeRoute()
{
    if (!introTutorialActive()) {
        return;
    }
    introTutorialPhase_ = IntroTutorialPhase::FreeToExit;
}

void Game::syncIntroTutorialTerrainDamageLocks()
{
    if (introTutorialActive() && introTutorialPhase_ == IntroTutorialPhase::DefeatEnemy) {
        tileMap_.setDamageProtectionAreas({
            TerrainDamageProtectionArea{
                .minTile = {
                    introTutorialFirstEnemyTile_.x - IntroTutorialSlimeCombatLockLeftTiles,
                    introTutorialFirstEnemyTile_.y - IntroTutorialSlimeCombatLockVerticalTiles,
                },
                .maxTile = {
                    introTutorialFirstEnemyTile_.x + IntroTutorialSlimeCombatLockRightTiles,
                    introTutorialFirstEnemyTile_.y + IntroTutorialSlimeCombatLockVerticalTiles,
                },
            },
        });
        return;
    }
    tileMap_.clearDamageProtectionAreas();
}

bool Game::updateIntroTutorial(const Input& input, float)
{
    if (!introTutorialActive() || mode_ != ScreenMode::Playing || screenTransition_.active()) {
        return false;
    }

    if (introTutorialPhase_ == IntroTutorialPhase::FallDialogue) {
        if (!dialogue_.active() && pendingStoryTriggers_.empty() && pendingStoryTrigger_.empty()) {
            equipIntroTutorialStartingTools();
        }
        return false;
    }

    if (introTutorialPhase_ == IntroTutorialPhase::ShovelRingIntro) {
        if (!dungeonRingIntroActive()) {
            queueStoryEventForTrigger(std::string(IntroTutorialShovelReadyTrigger));
            introTutorialPhase_ = IntroTutorialPhase::ExploreToTorch;
        }
        return false;
    }

    const float cueWorldX = IntroTutorialDarkCueTileX * static_cast<float>(balance::TileSize);
    if (introTutorialPhase_ == IntroTutorialPhase::ExploreToTorch &&
        !introTutorialLightTutorialQueued_ &&
        player_.position.x >= cueWorldX) {
        introTutorialLightTutorialQueued_ = true;
        queueStoryEventForTrigger(std::string(IntroTutorialTorchFoundTrigger));
        introTutorialPhase_ = IntroTutorialPhase::TorchDialogue;
        return false;
    }

    if (introTutorialPhase_ == IntroTutorialPhase::TorchDialogue) {
        if (!dialogue_.active() && pendingStoryTriggers_.empty() && pendingStoryTrigger_.empty()) {
            addIntroTutorialTorchToRing();
        }
        return false;
    }

    if (introTutorialPhase_ == IntroTutorialPhase::TorchRingIntro) {
        if (!dungeonRingIntroActive()) {
            queueStoryEventForTrigger(std::string(IntroTutorialTorchReadyTrigger));
            introTutorialPhase_ = IntroTutorialPhase::ExploreToEnemy;
        }
        return false;
    }

    if (introTutorialPhase_ == IntroTutorialPhase::ExploreToEnemy) {
        if (!enemyNodes_.empty() && enemyNodes_.front().spawned) {
            introTutorialFirstEnemySpawned_ = true;
            const float encounterRadius =
                IntroTutorialEnemyEncounterRadiusTiles * static_cast<float>(balance::TileSize);
            const Vec2 enemyRoomCenter = tileWorldCenter(introTutorialFirstEnemyTile_);
            if (!introTutorialEnemyEncounterQueued_ &&
                distanceSquared(player_.position, enemyRoomCenter) <= encounterRadius * encounterRadius) {
                const bool detected = (introTutorialFirstEnemyRuntimeId_ > 0 &&
                    enemies_.forceDetectRuntimeEnemy(introTutorialFirstEnemyRuntimeId_, player_.position, true)) ||
                    enemies_.forceDetectEnemyNear(enemyRoomCenter, encounterRadius, player_.position, true);
                if (!detected) {
                    return false;
                }
                introTutorialEnemyEncounterQueued_ = true;
                startIntroTutorialEnemyEncounterEvent();
                introTutorialPhase_ = IntroTutorialPhase::DefeatEnemy;
            }
        }
    }

    if (introTutorialPhase_ == IntroTutorialPhase::DefeatEnemy &&
        introTutorialFirstEnemySpawned_ &&
        enemies_.activeCount() == 0) {
        if (!introTutorialEnemyDefeatedQueued_) {
            introTutorialEnemyDefeatedQueued_ = true;
            queueStoryEventForTrigger(std::string(IntroTutorialEnemyDefeatedTrigger));
        }
        introTutorialPhase_ = IntroTutorialPhase::EnemyDefeatedDialogue;
        return false;
    }

    if (introTutorialPhase_ == IntroTutorialPhase::EnemyDefeatedDialogue) {
        if (!dialogue_.active() && pendingStoryTriggers_.empty() && pendingStoryTrigger_.empty()) {
            spawnIntroTutorialChest();
            if (!introTutorialChestFoundQueued_) {
                const Vec2 toChest = tileWorldCenter(introTutorialChestTile_) - player_.position;
                if (lengthSquared(toChest) > 0.0001f) {
                    player_.facing = normalize(toChest);
                }
                introTutorialChestFoundQueued_ = true;
                queueStoryEventForTrigger(std::string(IntroTutorialChestFoundTrigger));
            }
            unlockIntroTutorialFreeRoute();
        }
        return false;
    }

    if (introTutorialPhase_ == IntroTutorialPhase::FreeToExit) {
        EnemyNode* secondEnemyNode = nullptr;
        for (EnemyNode& node : enemyNodes_) {
            const std::string_view group(node.enemySpawnGroup.data(), node.enemySpawnGroup.size());
            if (group == IntroTutorialMushroomGroup) {
                secondEnemyNode = &node;
                break;
            }
        }
        if (secondEnemyNode != nullptr && secondEnemyNode->spawned) {
            introTutorialSecondEnemySpawned_ = true;
            if (!introTutorialSecondEnemyEncountered_) {
                const float encounterRadius =
                    IntroTutorialSecondEnemyEncounterRadiusTiles * static_cast<float>(balance::TileSize);
                if (distanceSquared(player_.position, tileWorldCenter(introTutorialSecondEnemyTile_)) <=
                    encounterRadius * encounterRadius) {
                    const float resolveRadius =
                        IntroTutorialEnemyResolveRadiusTiles * static_cast<float>(balance::TileSize);
                    const bool detected = (introTutorialSecondEnemyRuntimeId_ > 0 &&
                        enemies_.forceDetectRuntimeEnemy(introTutorialSecondEnemyRuntimeId_, player_.position, true)) ||
                        enemies_.forceDetectEnemyNear(
                            tileWorldCenter(introTutorialSecondEnemyTile_),
                            resolveRadius,
                            player_.position,
                            true);
                    if (detected) {
                        introTutorialSecondEnemyEncountered_ = true;
                    }
                }
            }
        }
        if (introTutorialSecondEnemySpawned_ &&
            !introTutorialSecondChestPlaced_ &&
            enemies_.activeCount() == 0) {
            spawnIntroTutorialSecondChest();
        }
    }

    if (introTutorialPhase_ == IntroTutorialPhase::FreeToExit) {
        const float midwayWorldX = IntroTutorialMidwayCueTileX * static_cast<float>(balance::TileSize);
        if (!introTutorialMidwayDialogueQueued_ && player_.position.x >= midwayWorldX) {
            introTutorialMidwayDialogueQueued_ = true;
            queueStoryEventForTrigger(std::string(IntroTutorialMidwayTrigger));
        }
    }

    if (introTutorialPhase_ == IntroTutorialPhase::FreeToExit) {
        const float distanceToExitSq = distanceSquared(player_.position, introTutorialExitPosition());
        const float exitFoundRadius =
            IntroTutorialExitFoundRadiusTiles * static_cast<float>(balance::TileSize);
        if (!introTutorialExitDialogueQueued_ && distanceToExitSq <= exitFoundRadius * exitFoundRadius) {
            introTutorialExitDialogueQueued_ = true;
            queueStoryEventForTrigger(std::string(IntroTutorialExitFoundTrigger));
            return false;
        }

        const float exitInteractRadiusSq = IntroTutorialExitInteractRadius * IntroTutorialExitInteractRadius;
        if (distanceToExitSq <= exitInteractRadiusSq) {
            if (input.confirmPressed() || input.useItemPressed()) {
                introTutorialPhase_ = IntroTutorialPhase::Returning;
                requestScreenTransition(ScreenTransitionTarget::IntroTutorialToBase);
                return true;
            }
        }
    }

    return false;
}

void Game::completeIntroTutorialAndReturnToBase()
{
    addStoryFlag(std::string(IntroTutorialCompletedFlag));
    tileMap_.clearDamageProtectionAreas();
    introTutorialPhase_ = IntroTutorialPhase::Inactive;
    player_.minimumHpAfterDamage = 0;
    introTutorialLightTutorialQueued_ = false;
    introTutorialFirstEnemySpawned_ = false;
    introTutorialSecondEnemySpawned_ = false;
    introTutorialSecondEnemyEncountered_ = false;
    introTutorialEnemyEncounterQueued_ = false;
    introTutorialEnemyDefeatedQueued_ = false;
    introTutorialChestFoundQueued_ = false;
    introTutorialSecondChestPlaced_ = false;
    introTutorialChestOpened_ = false;
    introTutorialChestLootPending_ = false;
    introTutorialChestLootDialogueQueued_ = false;
    introTutorialMidwayDialogueQueued_ = false;
    introTutorialExitDialogueQueued_ = false;
    introTutorialFirstEnemyRuntimeId_ = 0;
    introTutorialSecondEnemyRuntimeId_ = 0;
    introTutorialChestLootObjectId_.clear();
    introTutorialChestLootInstanceId_.clear();

    clearTemporaryPlayerState(true);
    enemies_ = EnemySystem{};
    effects_ = EffectSystem{};
    captureAbsorbAnimations_.clear();
    groundLines_ = GroundLineSystem{};
    wetGround_ = WetGroundSystem{};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
    projectiles_ = ProjectileSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    levels_ = LevelSystem{};
    levelUpPresentation_ = {};
    dungeonEvents_.clear();
    rewardNodes_.clear();
    moneyNodes_.clear();
    moonFragmentNodes_.clear();
    chestNodes_.clear();
    crateNodes_.clear();
    enemyNodes_.clear();
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    warpReturnConfirm_ = {};
    focusedWarpReturnPointIndex_ = -1;
    retrySnapshot_ = RetrySnapshot{};
    warpPoints_.clear();
    spawnedWarpPointCount_ = 0;
    warpPointsEnabled_ = true;

    currentStageId_ = "stage_01_stardust";
    resolveCurrentStageDefinition();
    roguelikeDungeon_ = currentStageIsRoguelike();
    restoreRunStartInventoryOnDeath_ = roguelikeDungeon_;
    roguelikeCarryInRestricted_ = roguelikeDungeon_;
    roguelikeCarryOutRestricted_ = roguelikeDungeon_;
    syncWarpStateForCurrentStage();

    placeBasePlayerAtMineExitReturnPoint();
    enterBase();
    baseStatus_ = testPlayMode_ ? "脱出しました" : std::string{};
    pendingStoryTrigger_ = std::string(IntroTutorialBaseReturnTrigger);
    if (autoSaveOnReturn_) {
        std::string message;
        if (saveSaveData(message)) {
            if (testPlayMode_) {
                baseStatus_ += " / 自動保存";
            }
        } else if (testPlayMode_) {
            baseStatus_ += " / " + message;
        }
    }
}

void Game::resetWarpPointRunState()
{
    hasBossSpawnPoint_ = false;
    resetBossEncounter();
    retrySnapshot_ = RetrySnapshot{};
    const bool stageIsRoguelike = currentStageDefinition_.type == "ローグライク" ||
        currentStageDefinition_.generationProfile == "astral_rogue";
    warpPointsEnabled_ = !(roguelikeDungeon_ || stageIsRoguelike);
    initializeWarpPointsFromLayout();
}

void Game::captureDungeonState()
{
    const bool stageIsRoguelike = currentStageDefinition_.type == "ローグライク" ||
        currentStageDefinition_.generationProfile == "astral_rogue";
    if (enemyTestActive_ || roguelikeDungeon_ || stageIsRoguelike || currentStageId_.empty()) {
        return;
    }

    enemies_.clearTemporaryState();
    DungeonState state;
    state.valid = true;
    state.currentStage = currentStage_;
    state.currentStageId = currentStageId_;
    state.tileMap = tileMap_;
    state.dungeonLayout = dungeonLayout_;
    state.dungeonMinimapCells = dungeonMinimapCells_;
    state.runStats = runStats_;
    state.warpPoints = warpPoints_;
    state.rewardNodes = rewardNodes_;
    state.moneyNodes = moneyNodes_;
    state.moonFragmentNodes = moonFragmentNodes_;
    state.chestNodes = chestNodes_;
    state.crateNodes = crateNodes_;
    state.enemyNodes = enemyNodes_;
    state.dungeonEventInstances = dungeonEvents_.all();
    state.enemies = enemies_;
    state.worldDrops = worldDrops_;
    state.worldDrops.removeTemporaryDrops();
    state.spawnedWarpPointCount = spawnedWarpPointCount_;
    state.unlockedWarpPointCount = unlockedWarpPointCount_;
    state.latestWarpPointPosition = latestWarpPointPosition_;
    state.hasLatestWarpPointPosition = hasLatestWarpPointPosition_;
    state.bossSpawnPoint = bossSpawnPoint_;
    state.hasBossSpawnPoint = hasBossSpawnPoint_;
    state.bossSpawned = bossSpawned_;
    dungeonStates_[currentStageId_] = std::move(state);
}

bool Game::restoreDungeonState(bool useLatestWarpPoint)
{
    const bool stageIsRoguelike = currentStageDefinition_.type == "ローグライク" ||
        currentStageDefinition_.generationProfile == "astral_rogue";
    if (roguelikeDungeon_ || stageIsRoguelike) {
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
    dungeonMinimapCells_ = state.dungeonMinimapCells;
    runStats_ = state.runStats;
    warpPoints_ = state.warpPoints;
    rewardNodes_ = state.rewardNodes;
    moneyNodes_ = state.moneyNodes;
    moonFragmentNodes_ = state.moonFragmentNodes;
    chestNodes_ = state.chestNodes;
    crateNodes_ = state.crateNodes;
    enemyNodes_ = state.enemyNodes;
    dungeonEvents_.setInstances(state.dungeonEventInstances);
    enemies_ = state.enemies;
    enemies_.clearTemporaryState();
    worldDrops_ = state.worldDrops;
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    spawnedWarpPointCount_ = state.spawnedWarpPointCount;
    unlockedWarpPointCount_ = state.unlockedWarpPointCount;
    latestWarpPointPosition_ = state.latestWarpPointPosition;
    hasLatestWarpPointPosition_ = state.hasLatestWarpPointPosition;
    bossSpawnPoint_ = state.bossSpawnPoint;
    hasBossSpawnPoint_ = state.hasBossSpawnPoint;
    bossSpawned_ = state.bossSpawned;
    resetBossEncounter();
    warpPointsEnabled_ = true;
    retrySnapshot_ = RetrySnapshot{};
    effects_ = EffectSystem{};
    captureAbsorbAnimations_.clear();
    groundLines_ = GroundLineSystem{};
    wetGround_ = WetGroundSystem{};
    projectiles_ = ProjectileSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    levels_ = LevelSystem{};
    levelUpPresentation_ = {};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;

    player_ = Player{};
    player_.xpToNext = playerXpToNextForLevel(player_.level, balance_);
    clearKnownWarpPointTerrain();
    const Vec2 preferredStartPosition = useLatestWarpPoint
        ? warpPointStartPositionForCurrentRequest()
        : tileWorldCenter(dungeonLayout_.startTile);
    player_.position = safePlayerStartPosition(preferredStartPosition);
    camera_.follow(player_.position, 1.0f);
    tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
    normalizeOpenBuriedPlacementNodes();
    updateDungeonMinimap(0.0);
    return true;
}

bool Game::canRegenerateCurrentStage() const
{
    const bool stageIsRoguelike = currentStageDefinition_.type == "ローグライク" ||
        currentStageDefinition_.generationProfile == "astral_rogue";
    if (roguelikeDungeon_ || stageIsRoguelike) {
        return false;
    }
    if (!currentStageCleared()) {
        return false;
    }
    auto it = dungeonStates_.find(currentStageId_);
    const std::vector<WarpPoint>& points = it != dungeonStates_.end() && it->second.valid ? it->second.warpPoints : warpPoints_;
    if (points.empty()) {
        const int requiredWarpPoints = currentStageDefinition_.warpPointCount > 0
            ? std::min(currentStageDefinition_.warpPointCount, MaxWarpPointsPerRun)
            : MaxWarpPointsPerRun;
        return unlockedWarpPointCount_ >= requiredWarpPoints;
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

std::int64_t Game::dungeonMinimapKey(int tx, int ty)
{
    const std::uint64_t x = static_cast<std::uint32_t>(tx);
    const std::uint64_t y = static_cast<std::uint32_t>(ty);
    return static_cast<std::int64_t>((x << 32) | y);
}

DungeonTile Game::dungeonMinimapTileFromKey(std::int64_t key)
{
    const std::uint64_t raw = static_cast<std::uint64_t>(key);
    const auto signedFromU32 = [](std::uint32_t value) {
        if (value <= static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
            return static_cast<int>(value);
        }
        return -1 - static_cast<int>(~value);
    };
    return {
        signedFromU32(static_cast<std::uint32_t>(raw >> 32)),
        signedFromU32(static_cast<std::uint32_t>(raw & 0xFFFFFFFFull)),
    };
}

void Game::resetDungeonMinimap()
{
    dungeonMinimapCells_.clear();
    dungeonMinimapLastRevealSeconds_ = -1.0e9;
    dungeonMinimapLastPlayerTileX_ = std::numeric_limits<int>::min();
    dungeonMinimapLastPlayerTileY_ = std::numeric_limits<int>::min();
}

void Game::setDungeonMinimapTile(int tx, int ty, TileType type)
{
    dungeonMinimapCells_[dungeonMinimapKey(tx, ty)] = DungeonMinimapCell{type};
}

bool Game::dungeonMinimapTileSeen(int tx, int ty) const
{
    return dungeonMinimapCells_.find(dungeonMinimapKey(tx, ty)) != dungeonMinimapCells_.end();
}

void Game::revealDungeonMinimapAround(Vec2 center, float radius)
{
    if (radius <= 0.0f) {
        return;
    }

    const float tileSize = static_cast<float>(balance::TileSize);
    const int minTileX = tileMap_.worldToTile(center.x - radius) - 1;
    const int maxTileX = tileMap_.worldToTile(center.x + radius) + 1;
    const int minTileY = tileMap_.worldToTile(center.y - radius) - 1;
    const int maxTileY = tileMap_.worldToTile(center.y + radius) + 1;
    for (int ty = minTileY; ty <= maxTileY; ++ty) {
        for (int tx = minTileX; tx <= maxTileX; ++tx) {
            const Vec2 tilePos{static_cast<float>(tx) * tileSize, static_cast<float>(ty) * tileSize};
            if (!circleIntersectsAabb(center, radius, tilePos, {tileSize, tileSize})) {
                continue;
            }
            const TerrainDebugInfo info = tileMap_.terrainDebugAtWorld(tileMap_.tileCenter(tx, ty));
            setDungeonMinimapTile(tx, ty, info.type);
        }
    }
}

void Game::revealDungeonMinimapOpenedTiles(const std::vector<Vec2>& openedTiles)
{
    for (Vec2 openedTile : openedTiles) {
        setDungeonMinimapTile(
            tileMap_.worldToTile(openedTile.x),
            tileMap_.worldToTile(openedTile.y),
            TileType::Empty);
    }
}

void Game::updateDungeonMinimap(double totalSeconds)
{
    if (enemyTestActive_ || dungeonLayout_.mainPathPoints.empty()) {
        return;
    }

    const int playerTileX = tileMap_.worldToTile(player_.position.x);
    const int playerTileY = tileMap_.worldToTile(player_.position.y);
    const bool playerTileChanged =
        playerTileX != dungeonMinimapLastPlayerTileX_ ||
        playerTileY != dungeonMinimapLastPlayerTileY_;
    const bool intervalElapsed =
        totalSeconds < dungeonMinimapLastRevealSeconds_ ||
        totalSeconds - dungeonMinimapLastRevealSeconds_ >= DungeonMinimapRevealIntervalSeconds;
    if (!playerTileChanged && !intervalElapsed) {
        return;
    }

    dungeonMinimapLastRevealSeconds_ = totalSeconds;
    dungeonMinimapLastPlayerTileX_ = playerTileX;
    dungeonMinimapLastPlayerTileY_ = playerTileY;

    const RuntimeBalance dungeonBalance = runtimeBalanceForDungeon();
    const Vec2 playerLightCenter = witchSelfLightCenter(player_.position);
    revealDungeonMinimapAround(playerLightCenter, dungeonBalance.playerLightRadius);

    const Vec2 viewTopLeft = camera_.screenToWorld({0.0f, 0.0f});
    const Vec2 viewBottomRight = camera_.screenToWorld({
        static_cast<float>(camera_.width()),
        static_cast<float>(camera_.height()),
    });
    const float viewLeft = std::min(viewTopLeft.x, viewBottomRight.x);
    const float viewRight = std::max(viewTopLeft.x, viewBottomRight.x);
    const float viewTop = std::min(viewTopLeft.y, viewBottomRight.y);
    const float viewBottom = std::max(viewTopLeft.y, viewBottomRight.y);
    const auto lightIntersectsView = [&](const LightSource& light, float radius) {
        return light.position.x + radius >= viewLeft &&
            light.position.x - radius <= viewRight &&
            light.position.y + radius >= viewTop &&
            light.position.y - radius <= viewBottom;
    };

    for (const LightSource& light : collectDungeonLightSources(totalSeconds)) {
        const float radius = light.radius > 0.0f ? light.radius : dungeonBalance.lightRadius;
        if (!lightIntersectsView(light, radius)) {
            continue;
        }
        revealDungeonMinimapAround(light.position, radius);
    }
}

void Game::DungeonEventSystem::clear()
{
    instances.clear();
}

void Game::DungeonEventSystem::setInstances(std::vector<DungeonEventInstance> nextInstances)
{
    instances = std::move(nextInstances);
}

const std::vector<Game::DungeonEventInstance>& Game::DungeonEventSystem::all() const
{
    return instances;
}

std::vector<Game::DungeonEventInstance>& Game::DungeonEventSystem::mutableAll()
{
    return instances;
}

bool Game::DungeonEventSystem::empty() const
{
    return instances.empty();
}

std::size_t Game::DungeonEventSystem::size() const
{
    return instances.size();
}

void Game::DungeonEventSystem::generateFromLayout(
    const DungeonLayout& layout,
    const std::vector<WarpPoint>& warpPoints,
    bool warpPointsEnabled,
    std::string_view stageId)
{
    clear();
    if (layout.mainPathPoints.empty()) {
        logWarning("[dungeon_event] generation skipped: layout has no main path");
        return;
    }

    const auto makeInstance = [](
        std::string id,
        DungeonEventKind kind,
        DungeonTile centerTile,
        DungeonTile focusTile,
        std::string data = {},
        float cavityRadiusTiles = 0.0f) {
        DungeonEventInstance instance;
        instance.id = std::move(id);
        instance.kind = kind;
        instance.centerTile = centerTile;
        instance.focusTile = focusTile;
        instance.rewardTile = focusTile;
        instance.discoveryRadiusTiles = dungeonEventDiscoveryRadiusTiles(kind);
        instance.selfLightRadiusTiles = dungeonEventLightRadiusTiles(kind);
        instance.cavityRadiusTiles = std::max(0.0f, cavityRadiusTiles);
        instance.params = data.empty() ? std::string{} : "source=" + data;
        instance.data = std::move(data);
        return instance;
    };
    const auto hasKind = [this](DungeonEventKind kind) {
        return std::any_of(instances.begin(), instances.end(), [kind](const DungeonEventInstance& event) {
            return event.kind == kind;
        });
    };
    const auto countIf = [this](auto predicate) {
        return static_cast<int>(std::count_if(instances.begin(), instances.end(), predicate));
    };
    const int maxEvents =
        static_cast<int>(layout.specialRoomAnchors.size()) +
        dungeonEventStageMaxCount(stageId);
    const bool hasUndiscoveredWarpPoint = warpPointsEnabled &&
        std::any_of(warpPoints.begin(), warpPoints.end(), [](const WarpPoint& point) {
            return !point.discovered;
        });
    const auto canAdd = [&](
        DungeonEventKind kind,
        DungeonTile tile,
        float pathProgress,
        bool enforceSpacing,
        bool enforceKindLimit,
        bool enforceCategoryLimit,
        bool enforceStageLimit) {
        if (static_cast<int>(instances.size()) >= maxEvents) {
            return false;
        }
        if (enforceStageLimit && !dungeonEventKindAllowedForStage(kind, stageId)) {
            return false;
        }
        if (enforceKindLimit && hasKind(kind)) {
            return false;
        }
        if (enforceCategoryLimit && dungeonEventKindIsWitch(kind) && countIf([](const DungeonEventInstance& event) {
                return dungeonEventKindIsWitch(event.kind);
            }) >= 2) {
            return false;
        }
        if (enforceCategoryLimit && dungeonEventKindIsCombat(kind) && countIf([](const DungeonEventInstance& event) {
                return dungeonEventKindIsCombat(event.kind);
            }) >= 2) {
            return false;
        }
        if (enforceCategoryLimit && dungeonEventKindIsHighDanger(kind) && pathProgress < 0.28f) {
            return false;
        }
        return !enforceSpacing || !dungeonEventKindTooClose(instances, tile);
    };
    const auto addInstance = [&](
        std::string id,
        DungeonEventKind kind,
        DungeonTile centerTile,
        DungeonTile focusTile,
        std::string data,
        float cavityRadiusTiles = 0.0f) {
        instances.push_back(makeInstance(
            std::move(id),
            kind,
            centerTile,
            focusTile,
            std::move(data),
            cavityRadiusTiles));
    };

    int eventIndex = 0;
    for (const SpecialRoomAnchor& room : layout.specialRoomAnchors) {
        if (static_cast<int>(instances.size()) >= maxEvents) {
            break;
        }
        const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(layout, room.center);
        const std::optional<DungeonEventKind> kind = dungeonEventKindForSpecialRoom(room.type, eventIndex);
        if (!kind) {
            ++eventIndex;
            continue;
        }

        const DungeonTile centerTile = roundDungeonTile(room.center);
        if (!canAdd(*kind, centerTile, metrics.pathProgress, false, false, false, false)) {
            ++eventIndex;
            continue;
        }
        std::string id = "room_" + std::to_string(eventIndex) + "_" + Game::dungeonEventKindId(*kind);
        addInstance(std::move(id), *kind, centerTile, centerTile, specialRoomTypeName(room.type), room.radius);
        ++eventIndex;
    }

    if (dungeonEventKindAllowedForStage(DungeonEventKind::BossMonsterRoom, stageId)) {
        const DungeonEventKind kind = DungeonEventKind::BossMonsterRoom;
        constexpr float BossRoomProgress = 0.76f;
        const DungeonTile bossRoomTile = roundDungeonTile(pointAtPathProgress(layout.mainPathPoints, BossRoomProgress));
        if (canAdd(kind, bossRoomTile, BossRoomProgress, true, true, true, true)) {
            addInstance("boss_monster_room_0", kind, bossRoomTile, bossRoomTile, "path_boss");
        }
    }

    const bool guideRoll =
        ((layout.seed ^ static_cast<std::uint32_t>(std::hash<std::string>{}(std::string(stageId)))) % 100u) < 35u;
    if (hasUndiscoveredWarpPoint && guideRoll && static_cast<int>(instances.size()) < maxEvents) {
        const DungeonEventKind kind = DungeonEventKind::WarpGuideMap;
        const DungeonTile guideTile = roundDungeonTile(pointAtPathProgress(layout.mainPathPoints, 0.48f));
        if (canAdd(kind, guideTile, 0.48f, true, true, true, true)) {
            addInstance("warp_guide_map_0", kind, guideTile, guideTile, "warp");
        }
    }

    constexpr std::array<float, 12> FallbackProgress{
        0.18f,
        0.26f,
        0.34f,
        0.42f,
        0.50f,
        0.58f,
        0.64f,
        0.69f,
        0.74f,
        0.79f,
        0.84f,
        0.88f,
    };
    const std::span<const DungeonEventKind> fallbackKinds = dungeonEventStageCandidateKinds(stageId);
    for (std::size_t i = 0; i < fallbackKinds.size() && static_cast<int>(instances.size()) < maxEvents; ++i) {
        const DungeonEventKind kind = fallbackKinds[i];
        if (kind == DungeonEventKind::WarpGuideMap && !hasUndiscoveredWarpPoint) {
            continue;
        }
        const float progress = FallbackProgress[std::min<std::size_t>(i, FallbackProgress.size() - 1)];
        const DungeonTile tile = roundDungeonTile(pointAtPathProgress(layout.mainPathPoints, progress));
        if (!canAdd(kind, tile, progress, true, true, true, true)) {
            continue;
        }
        addInstance(
            "path_" + std::to_string(i) + "_" + Game::dungeonEventKindId(kind),
            kind,
            tile,
            tile,
            "path");
    }

    if (instances.empty()) {
        logWarning("[dungeon_event] generation produced no events stage=" + std::string(stageId));
        return;
    }

    std::string summary = "[dungeon_event] generated stage=" + std::string(stageId) +
        " total=" + std::to_string(instances.size());
    for (const DungeonEventDefinition& definition : dungeonEventDefinitions()) {
        const DungeonEventKind kind = definition.kind;
        const int count = countIf([kind](const DungeonEventInstance& event) {
            return event.kind == kind;
        });
        if (count > 0) {
            summary += " ";
            summary += Game::dungeonEventKindId(kind);
            summary += "=" + std::to_string(count);
        }
    }
    logInfo(summary);
}

void Game::DungeonEventSystem::appendLightSources(std::vector<LightSource>& lights, double totalSeconds) const
{
    for (const DungeonEventInstance& event : instances) {
        const float radiusPx = dungeonEventSelfLightRadiusPx(event);
        if (radiusPx <= 0.0f) {
            continue;
        }
        const float phase = static_cast<float>(std::hash<std::string>{}(event.id) % 997u) * 0.017f;
        if (dungeonEventSelfLightActive(event)) {
            lights.push_back({
                flickeredLightPosition(dungeonEventSelfLightPosition(event), static_cast<float>(totalSeconds), phase),
                flickeredLightRadius(radiusPx, static_cast<float>(totalSeconds), phase),
            });
        }
        for (const DungeonEventNestHole& hole : event.nestHoles) {
            if (hole.destroyed) {
                continue;
            }
            const float holePhase = phase + 1.7f;
            lights.push_back({
                flickeredLightPosition(tileWorldCenter(hole.tile), static_cast<float>(totalSeconds), holePhase),
                flickeredLightRadius(static_cast<float>(balance::TileSize) * 2.5f, static_cast<float>(totalSeconds), holePhase),
            });
        }
        for (const DungeonEventObject& object : event.eventObjects) {
            if (object.destroyed) {
                continue;
            }
            const float objectPhase = phase + (object.powered ? 3.4f : 2.5f);
            float objectRadiusTiles = 2.75f;
            switch (object.kind) {
            case DungeonEventObjectKind::ElectricReceiver:
                objectRadiusTiles = object.powered ? 3.25f : 2.75f;
                break;
            case DungeonEventObjectKind::Campfire:
                objectRadiusTiles = object.powered ? 4.5f : 2.5f;
                break;
            case DungeonEventObjectKind::LostBaggage:
                objectRadiusTiles = 2.25f;
                break;
            case DungeonEventObjectKind::BuriedDebris:
            case DungeonEventObjectKind::HeavyRock:
            case DungeonEventObjectKind::GlowingRock:
                objectRadiusTiles = 2.75f;
                break;
            }
            lights.push_back({
                flickeredLightPosition(tileWorldCenter(object.tile), static_cast<float>(totalSeconds), objectPhase),
                flickeredLightRadius(static_cast<float>(balance::TileSize) * objectRadiusTiles, static_cast<float>(totalSeconds), objectPhase),
            });
        }
    }
}

Game::DungeonEventInstance* Game::DungeonEventSystem::firstDiscoverable(Vec2 playerPosition, float tileSize)
{
    for (DungeonEventInstance& event : instances) {
        if (event.discovered || event.completed) {
            continue;
        }
        const float radius = std::max(0.0f, event.discoveryRadiusTiles) * tileSize;
        if (distanceSquared(playerPosition, tileWorldCenter(event.centerTile)) <= radius * radius) {
            return &event;
        }
    }
    return nullptr;
}

Game::DungeonEventInstance* Game::DungeonEventSystem::findById(std::string_view id)
{
    auto eventIt = std::find_if(instances.begin(), instances.end(), [&](const DungeonEventInstance& event) {
        return event.id == id;
    });
    return eventIt == instances.end() ? nullptr : &*eventIt;
}

const Game::DungeonEventInstance* Game::DungeonEventSystem::nearest(Vec2 playerPosition) const
{
    const DungeonEventInstance* nearestEvent = nullptr;
    float nearestDistanceSq = std::numeric_limits<float>::max();
    for (const DungeonEventInstance& event : instances) {
        const float distSq = distanceSquared(playerPosition, tileWorldCenter(event.centerTile));
        if (distSq < nearestDistanceSq) {
            nearestDistanceSq = distSq;
            nearestEvent = &event;
        }
    }
    return nearestEvent;
}

const char* Game::dungeonEventKindId(DungeonEventKind kind)
{
    return majo::dungeonEventKindId(kind).data();
}

bool Game::dungeonEventKindFromId(std::string_view id, DungeonEventKind& outKind)
{
    return majo::dungeonEventKindFromId(id, outKind);
}

const char* Game::dungeonEventKindDisplayName(DungeonEventKind kind)
{
    return majo::dungeonEventKindDisplayName(kind).data();
}

std::string Game::dungeonEventDiscoverySeenFlag(DungeonEventKind kind)
{
    return "dungeon_discovery_seen_" + std::string(dungeonEventKindId(kind));
}

std::string Game::dungeonEventDiscoveryStoryEventId(DungeonEventKind kind)
{
    return "dungeon_discovery:" + std::string(dungeonEventKindId(kind));
}

void Game::initializeDungeonEventInstancesFromLayout()
{
    dungeonEvents_.generateFromLayout(dungeonLayout_, warpPoints_, warpPointsEnabled_, currentStageId_);
    dungeonEventDiscoveryCooldown_ = 0.0f;
}

void Game::updateDungeonEvents(float dt, double totalSeconds)
{
    (void)totalSeconds;
    if (mode_ != ScreenMode::Playing ||
        enemyTestActive_ ||
        dungeonFocusActive() ||
        dialogue_.active() ||
        screenTransition_.active() ||
        worldBuildActive()) {
        return;
    }

    const float tileSize = static_cast<float>(balance::TileSize);
    const RuntimeBalance dungeonBalance = runtimeBalanceForDungeon();
    const auto eventDepthRank = [this](const DungeonEventInstance& event) {
        const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, {
            static_cast<float>(event.centerTile.x),
            static_cast<float>(event.centerTile.y),
        });
        return lootDepthRankForProgress(currentStageId_, metrics.pathProgress);
    };
    const auto ensureSelectedEnemy = [&](DungeonEventInstance& event) -> const std::string& {
        if (event.selectedEnemyId.empty()) {
            const std::uint32_t seed =
                dungeonLayout_.seed ^
                static_cast<std::uint32_t>(std::hash<std::string>{}(event.id)) ^
                (static_cast<std::uint32_t>(event.centerTile.x) * 0x85EBCA6Bu) ^
                (static_cast<std::uint32_t>(event.centerTile.y) * 0xC2B2AE35u);
            event.selectedEnemyId = chooseDungeonEventEnemyId(enemyCatalog_, currentStageId_, eventDepthRank(event), seed);
        }
        return event.selectedEnemyId;
    };
    const auto spawnEnemy = [&](DungeonEventInstance& event, Vec2 position, bool sleeping, bool bossVariant, int* outRuntimeId = nullptr) {
        EventEnemySpawnOptions options;
        options.enemyId = ensureSelectedEnemy(event);
        options.dungeonEventId = event.id;
        options.stageId = currentStageId_;
        options.depthRank = eventDepthRank(event);
        options.allowNearPlayer = true;
        options.detectedOnSpawn = !sleeping;
        options.fixedPosition = false;
        options.sleeping = sleeping;
        options.bossVariant = bossVariant;
        if (bossVariant) {
            options.hpMultiplier = 2.5f;
            options.contactDamageMultiplier = 1.5f;
            options.radiusMultiplier = 1.2f;
            options.xpMultiplier = 3.0f;
        }
        int runtimeId = 0;
        if (!enemies_.spawnEventEnemy(tileMap_, position, player_.position, dungeonBalance, enemyCatalog_, options, &runtimeId)) {
            return false;
        }
        event.spawnedEnemyRuntimeIds.push_back(runtimeId);
        if (outRuntimeId != nullptr) {
            *outRuntimeId = runtimeId;
        }
        return true;
    };
    const auto chestOpenedAt = [&](DungeonTile tile) {
        return std::any_of(chestNodes_.begin(), chestNodes_.end(), [tile](const ChestNode& node) {
            return dungeonEventSameTile(node.tile, tile) && node.opened;
        });
    };
    const auto ensureRewardChest = [&](DungeonEventInstance& event, DungeonTile tile, LootChestKind chestKind) {
        const bool spawned = ensureDungeonEventChest(event, tile, chestKind);
        event.rewardSpawned = true;
        if (spawned) {
            requestDungeonRewardChestFocus(tileWorldCenter(tile));
        }
        return spawned;
    };
    const auto activeEventEnemies = [&](const DungeonEventInstance& event) {
        return enemies_.activeDungeonEventEnemyCount(event.id);
    };
    const auto playerNearTile = [&](DungeonTile tile, float radiusTiles) {
        const float radius = std::max(0.0f, radiusTiles) * tileSize;
        return distanceSquared(player_.position, tileWorldCenter(tile)) <= radius * radius;
    };
    const auto nearestUndiscoveredWarpPointIndex = [&](Vec2 from) {
        int nearestIndex = -1;
        float nearestDistanceSq = std::numeric_limits<float>::max();
        if (!warpPointsEnabled_) {
            return nearestIndex;
        }
        for (int i = 0; i < static_cast<int>(warpPoints_.size()); ++i) {
            const WarpPoint& point = warpPoints_[static_cast<std::size_t>(i)];
            if (point.discovered) {
                continue;
            }
            const float distSq = distanceSquared(from, point.position);
            if (distSq < nearestDistanceSq) {
                nearestDistanceSq = distSq;
                nearestIndex = i;
            }
        }
        return nearestIndex;
    };
    const auto hitEventObjectWithRing = [&](DungeonEventObject& object, DungeonEventHitRequirement requirement, int* outDamage = nullptr) {
        object.hitCooldown = std::max(0.0f, object.hitCooldown - dt);
        if (object.hitCooldown > 0.0f || object.destroyed) {
            return false;
        }

        const Vec2 objectCenter = tileWorldCenter(object.tile);
        for (SpellRingItem* item : spellRing_.runtimeItemsMutable()) {
            if (item == nullptr || item->isBroken) {
                continue;
            }
            const float hitRadius = std::max(8.0f, item->hitRadius);
            if (distanceSquared(item->worldPosition, objectCenter) >
                (hitRadius + DungeonEventObjectHitPaddingPx) * (hitRadius + DungeonEventObjectHitPaddingPx)) {
                continue;
            }

            const int damage = dungeonEventObjectHitDamageFor(objectCatalog_, *item, requirement);
            if (damage <= 0) {
                continue;
            }

            object.hitCooldown = std::max(0.12f, item->hitInterval);
            item->actionFlashTimer = SpellRingItemActionFlashSeconds;
            effects_.spawnEnemyHit(objectCenter, {});
            if (outDamage != nullptr) {
                *outDamage = std::max(1, damage);
            }
            return true;
        }
        return false;
    };

    for (DungeonEventInstance& event : dungeonEvents_.mutableAll()) {
        if (!event.discovered || event.completed) {
            continue;
        }

        if (!event.encounterSpawned) {
            const DungeonTile centerTile = event.centerTile;
            switch (event.kind) {
            case DungeonEventKind::SleepingEnemyTreasure: {
                event.rewardTile = dungeonEventOffsetTile(centerTile, 0, -2);
                const bool lowDifficulty = currentStageId_ == "stage_01_stardust";
                ensureDungeonEventChest(event, event.rewardTile, lowDifficulty ? LootChestKind::Common : LootChestKind::Rare);
                event.rewardSpawned = true;
                constexpr std::array<DungeonTile, 6> Offsets{{
                    {-2, 0},
                    {2, 0},
                    {-1, 2},
                    {1, 2},
                    {-2, -1},
                    {2, -1},
                }};
                const int sleepingCount =
                    currentStageId_ == "stage_03_star_core" || currentStageId_ == "stage_04_astral_mine" ? 6 :
                    currentStageId_ == "stage_01_stardust" ? 3 :
                    4;
                for (int i = 0; i < sleepingCount; ++i) {
                    const DungeonTile offset = Offsets[static_cast<std::size_t>(i)];
                    spawnEnemy(event, tileWorldCenter(dungeonEventOffsetTile(centerTile, offset.x, offset.y)), true, false);
                }
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::MonsterSwarmRoom: {
                event.rewardTile = dungeonEventOffsetTile(centerTile, 0, -3);
                const int SwarmCount =
                    currentStageId_ == "stage_04_astral_mine" ? 10 :
                    currentStageId_ == "stage_01_stardust" ? 6 :
                    8;
                for (int i = 0; i < SwarmCount; ++i) {
                    const float angle = (Pi * 2.0f) * (static_cast<float>(i) / static_cast<float>(SwarmCount));
                    spawnEnemy(event, tileWorldCenter(centerTile) + fromAngle(angle) * (tileSize * 2.0f), false, false);
                }
                event.activated = true;
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::NestRoom: {
                if (event.nestHoles.empty()) {
                    const int holeRange = currentStageId_ == "stage_04_astral_mine" ? 2 : 1;
                    const int holeCount = 1 + static_cast<int>(std::hash<std::string>{}(event.id) % static_cast<std::size_t>(holeRange + 1));
                    for (int i = 0; i < holeCount; ++i) {
                        const float angle = (Pi * 2.0f) * (static_cast<float>(i) / static_cast<float>(holeCount));
                        const Vec2 holeWorld = tileWorldCenter(centerTile) + fromAngle(angle) * (tileSize * 1.8f);
                        DungeonEventNestHole hole;
                        hole.tile = roundDungeonTile({
                            static_cast<float>(tileMap_.worldToTile(holeWorld.x)),
                            static_cast<float>(tileMap_.worldToTile(holeWorld.y)),
                        });
                        hole.maxHp = 18;
                        hole.hp = hole.maxHp;
                        hole.spawnCooldown = 0.8f + static_cast<float>(i) * 0.6f;
                        event.nestHoles.push_back(std::move(hole));
                    }
                }
                for (DungeonEventNestHole& hole : event.nestHoles) {
                    for (int i = 0; i < 2; ++i) {
                        int runtimeId = 0;
                        if (spawnEnemy(event, tileWorldCenter(hole.tile) + fromAngle(static_cast<float>(i) * Pi) * tileSize, false, false, &runtimeId)) {
                            hole.spawnedEnemyRuntimeIds.push_back(runtimeId);
                        }
                    }
                }
                event.activated = true;
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::BossMonsterRoom: {
                event.rewardTile = dungeonEventOffsetTile(centerTile, 0, -3);
                constexpr std::array<DungeonTile, 3> Offsets{{
                    {-2, 1},
                    {2, 1},
                    {0, 2},
                }};
                for (DungeonTile offset : Offsets) {
                    spawnEnemy(event, tileWorldCenter(dungeonEventOffsetTile(centerTile, offset.x, offset.y)), false, false);
                }
                spawnEnemy(event, tileWorldCenter(centerTile), false, true, &event.bossEnemyRuntimeId);
                event.activated = true;
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::GlowingRockRoom: {
                if (event.eventObjects.empty()) {
                    const std::size_t seed = std::hash<std::string>{}(event.id);
                    const int rockCount = 3 + static_cast<int>(seed % 3u);
                    event.rewardTile = dungeonEventOffsetTile(centerTile, 0, -3);
                    for (int i = 0; i < rockCount; ++i) {
                        const float angle = (Pi * 2.0f) * (static_cast<float>(i) / static_cast<float>(rockCount)) +
                            static_cast<float>((seed >> 8) & 0xFFu) * 0.001f;
                        DungeonEventObject rock;
                        rock.kind = DungeonEventObjectKind::GlowingRock;
                        rock.tile = dungeonEventObjectTile(centerTile, angle, 2.0f);
                        rock.maxHp = 10;
                        rock.hp = rock.maxHp;
                        event.eventObjects.push_back(std::move(rock));
                    }
                }
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::ElectricCircuitRoom: {
                if (event.eventObjects.empty()) {
                    const std::size_t seed = std::hash<std::string>{}(event.id);
                    const int receiverCount = 2 + static_cast<int>(seed % 3u);
                    event.rewardTile = dungeonEventOffsetTile(centerTile, 0, -3);
                    for (int i = 0; i < receiverCount; ++i) {
                        const float angle = (Pi * 2.0f) * (static_cast<float>(i) / static_cast<float>(receiverCount)) +
                            static_cast<float>((seed >> 10) & 0xFFu) * 0.001f;
                        DungeonEventObject receiver;
                        receiver.kind = DungeonEventObjectKind::ElectricReceiver;
                        receiver.tile = dungeonEventObjectTile(centerTile, angle, 2.0f);
                        receiver.maxHp = 1;
                        receiver.hp = receiver.maxHp;
                        event.eventObjects.push_back(std::move(receiver));
                    }
                }
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::SafeCavern:
            case DungeonEventKind::CoinRoom:
                event.encounterSpawned = true;
                completeDungeonEvent(event, std::nullopt);
                break;
            case DungeonEventKind::BuriedWitch: {
                if (event.eventObjects.empty()) {
                    event.rewardTile = centerTile;
                    constexpr std::array<DungeonTile, 4> Offsets{{
                        {0, -1},
                        {-1, 0},
                        {1, 0},
                        {0, 1},
                    }};
                    for (DungeonTile offset : Offsets) {
                        DungeonEventObject debris;
                        debris.kind = DungeonEventObjectKind::BuriedDebris;
                        debris.tile = dungeonEventOffsetTile(centerTile, offset.x, offset.y);
                        debris.maxHp = 6;
                        debris.hp = debris.maxHp;
                        event.eventObjects.push_back(std::move(debris));
                    }
                }
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::LostBaggageWitch: {
                if (event.eventObjects.empty()) {
                    event.rewardTile = centerTile;
                    const std::size_t seed = std::hash<std::string>{}(event.id);
                    const float angle = (Pi * 2.0f) * (static_cast<float>(seed % 1009u) / 1009.0f);
                    DungeonEventObject baggage;
                    baggage.kind = DungeonEventObjectKind::LostBaggage;
                    baggage.tile = dungeonEventObjectTile(centerTile, angle, 3.0f);
                    baggage.maxHp = 1;
                    baggage.hp = baggage.maxHp;
                    event.eventObjects.push_back(std::move(baggage));
                }
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::ItemRequestWitch: {
                event.rewardTile = centerTile;
                if (event.requestKey.empty()) {
                    event.requestKey = dungeonEventItemRequestKeyFor(event);
                }
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::SurroundedWitch: {
                event.rewardTile = centerTile;
                constexpr std::array<DungeonTile, 3> Offsets{{
                    {-2, 0},
                    {2, 0},
                    {0, 2},
                }};
                for (DungeonTile offset : Offsets) {
                    spawnEnemy(event, tileWorldCenter(dungeonEventOffsetTile(centerTile, offset.x, offset.y)), false, false);
                }
                event.activated = true;
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::ColdWitchCampfire: {
                if (event.eventObjects.empty()) {
                    event.rewardTile = centerTile;
                    DungeonEventObject campfire;
                    campfire.kind = DungeonEventObjectKind::Campfire;
                    campfire.tile = dungeonEventOffsetTile(centerTile, 1, 1);
                    campfire.maxHp = 1;
                    campfire.hp = campfire.maxHp;
                    event.eventObjects.push_back(std::move(campfire));
                }
                event.encounterSpawned = true;
                break;
            }
            case DungeonEventKind::HeavyRockWitch: {
                if (event.eventObjects.empty()) {
                    event.rewardTile = centerTile;
                    DungeonEventObject rock;
                    rock.kind = DungeonEventObjectKind::HeavyRock;
                    rock.tile = dungeonEventOffsetTile(centerTile, 1, 0);
                    rock.maxHp = 34;
                    rock.hp = rock.maxHp;
                    event.eventObjects.push_back(std::move(rock));
                }
                event.encounterSpawned = true;
                break;
            }
            default:
                break;
            }
        }

        if (event.kind == DungeonEventKind::SleepingEnemyTreasure) {
            if (!event.activated && chestOpenedAt(event.rewardTile)) {
                event.activated = true;
                enemies_.wakeDungeonEventEnemies(event.id);
                pushDungeonLog("眠っていた敵が起きた", "dungeon_event_wake:" + event.id);
            }
            if (event.activated && chestOpenedAt(event.rewardTile) && activeEventEnemies(event) <= 0) {
                completeDungeonEvent(event, std::nullopt);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::MonsterSwarmRoom) {
            if (event.activated && activeEventEnemies(event) <= 0) {
                ensureRewardChest(event, event.rewardTile, LootChestKind::Rare);
                completeDungeonEvent(event, std::nullopt);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::NestRoom) {
            bool allHolesDestroyed = !event.nestHoles.empty();
            for (DungeonEventNestHole& hole : event.nestHoles) {
                hole.hitCooldown = std::max(0.0f, hole.hitCooldown - dt);
                if (hole.destroyed) {
                    continue;
                }
                allHolesDestroyed = false;
                const Vec2 holeCenter = tileWorldCenter(hole.tile);
                if (hole.hitCooldown <= 0.0f) {
                    for (SpellRingItem* item : spellRing_.runtimeItemsMutable()) {
                        if (item == nullptr || item->isBroken) {
                            continue;
                        }
                        const float hitRadius = std::max(8.0f, item->hitRadius);
                        if (distanceSquared(item->worldPosition, holeCenter) > (hitRadius + 16.0f) * (hitRadius + 16.0f)) {
                            continue;
                        }
                        const int damage = std::max({1, item->damage, item->digPower});
                        hole.hp = std::max(0, hole.hp - damage);
                        hole.hitCooldown = std::max(0.12f, item->hitInterval);
                        item->actionFlashTimer = SpellRingItemActionFlashSeconds;
                        effects_.spawnEnemyHit(holeCenter, {});
                        if (hole.hp <= 0) {
                            hole.destroyed = true;
                            playAudioSe("se.dig.break");
                            if (!hole.rewardSpawned) {
                                hole.rewardSpawned = true;
                                ensureDungeonEventChest(event, dungeonEventOffsetTile(hole.tile, 1, 0), LootChestKind::Common);
                            }
                        }
                        break;
                    }
                }

                hole.spawnCooldown = std::max(0.0f, hole.spawnCooldown - dt);
                const int activeHoleEnemies = enemies_.activeRuntimeEnemyCount(hole.spawnedEnemyRuntimeIds);
                if (hole.spawnCooldown <= 0.0f && activeHoleEnemies < 3 && activeEventEnemies(event) < 9) {
                    int runtimeId = 0;
                    if (spawnEnemy(event, tileWorldCenter(hole.tile), false, false, &runtimeId)) {
                        hole.spawnedEnemyRuntimeIds.push_back(runtimeId);
                        playAudioSe("se.enemy.spawn");
                    }
                    hole.spawnCooldown = 4.0f;
                }
            }
            if (allHolesDestroyed && activeEventEnemies(event) <= 0) {
                event.rewardSpawned = true;
                completeDungeonEvent(event, std::nullopt);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::BossMonsterRoom) {
            if (event.bossDefeated) {
                ensureRewardChest(event, event.rewardTile, LootChestKind::Rare);
                completeDungeonEvent(event, std::nullopt);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::GlowingRockRoom) {
            for (DungeonEventObject& object : event.eventObjects) {
                if (object.kind != DungeonEventObjectKind::GlowingRock) {
                    continue;
                }
                if (object.destroyed) {
                    continue;
                }
                int hitDamage = 0;
                if (hitEventObjectWithRing(object, DungeonEventHitRequirement::AnyDamage, &hitDamage)) {
                    object.hp = std::max(0, object.hp - std::max(1, hitDamage));
                    if (object.hp <= 0) {
                        object.destroyed = true;
                        playAudioSe("se.dig.break");
                    }
                }
            }
            const bool allDestroyed = !event.eventObjects.empty() &&
                std::all_of(event.eventObjects.begin(), event.eventObjects.end(), [](const DungeonEventObject& object) {
                    return object.kind != DungeonEventObjectKind::GlowingRock || object.destroyed;
            });
            if (allDestroyed) {
                ensureRewardChest(event, event.rewardTile, LootChestKind::Rare);
                completeDungeonEvent(event, std::nullopt);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::ElectricCircuitRoom) {
            for (DungeonEventObject& object : event.eventObjects) {
                if (object.kind != DungeonEventObjectKind::ElectricReceiver) {
                    continue;
                }
                if (object.powered) {
                    continue;
                }
                if (hitEventObjectWithRing(object, DungeonEventHitRequirement::Thunder)) {
                    object.powered = true;
                    playAudioSe("se.discovery");
                    pushDungeonLog("受電石が通電した", "dungeon_event_power:" + event.id);
                }
            }
            const bool allPowered = !event.eventObjects.empty() &&
                std::all_of(event.eventObjects.begin(), event.eventObjects.end(), [](const DungeonEventObject& object) {
                    return object.kind != DungeonEventObjectKind::ElectricReceiver || object.powered;
            });
            if (allPowered) {
                ensureRewardChest(event, event.rewardTile, LootChestKind::Rare);
                completeDungeonEvent(event, std::nullopt);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::BuriedWitch) {
            for (DungeonEventObject& object : event.eventObjects) {
                if (object.kind != DungeonEventObjectKind::BuriedDebris || object.destroyed) {
                    continue;
                }
                int hitDamage = 0;
                if (hitEventObjectWithRing(object, DungeonEventHitRequirement::AnyDamage, &hitDamage)) {
                    object.hp = std::max(0, object.hp - std::max(1, hitDamage));
                    if (object.hp <= 0) {
                        object.destroyed = true;
                        playAudioSe("se.dig.break");
                    }
                }
            }
            const bool allDebrisCleared = !event.eventObjects.empty() &&
                std::all_of(event.eventObjects.begin(), event.eventObjects.end(), [](const DungeonEventObject& object) {
                    return object.kind != DungeonEventObjectKind::BuriedDebris || object.destroyed;
                });
            if (allDebrisCleared && !event.objectiveResolved) {
                event.objectiveResolved = true;
                pushDungeonLog("魔女を助け出せそう", "dungeon_event_witch_ready:" + event.id);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::LostBaggageWitch) {
            for (DungeonEventObject& object : event.eventObjects) {
                if (object.kind != DungeonEventObjectKind::LostBaggage || object.destroyed) {
                    continue;
                }
                if (playerNearTile(object.tile, 1.5f)) {
                    object.destroyed = true;
                    event.activated = true;
                    event.objectiveResolved = true;
                    pushDungeonLog("落とし物を拾った", "dungeon_event_baggage_pickup:" + event.id);
                    playAudioSe("se.pickup");
                }
            }
            continue;
        }

        if (event.kind == DungeonEventKind::ItemRequestWitch) {
            if (event.requestKey.empty()) {
                event.requestKey = dungeonEventItemRequestKeyFor(event);
            }
            if (!event.deliveredObjectId.empty()) {
                event.objectiveResolved = true;
            }
            continue;
        }

        if (event.kind == DungeonEventKind::SurroundedWitch) {
            if (event.activated && activeEventEnemies(event) <= 0 && !event.objectiveResolved) {
                event.objectiveResolved = true;
                pushDungeonLog("囲まれた魔女を助けた", "dungeon_event_witch_ready:" + event.id);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::ColdWitchCampfire) {
            for (DungeonEventObject& object : event.eventObjects) {
                if (object.kind != DungeonEventObjectKind::Campfire || object.powered) {
                    continue;
                }
                if (hitEventObjectWithRing(object, DungeonEventHitRequirement::Fire)) {
                    object.powered = true;
                    event.activated = true;
                    pushDungeonLog("焚き火に火がついた", "dungeon_event_campfire:" + event.id);
                    playAudioSe("se.discovery");
                }
            }
            const bool lit = !event.eventObjects.empty() &&
                std::any_of(event.eventObjects.begin(), event.eventObjects.end(), [](const DungeonEventObject& object) {
                    return object.kind == DungeonEventObjectKind::Campfire && object.powered;
                });
            if (lit && !event.objectiveResolved) {
                event.objectiveResolved = true;
                pushDungeonLog("魔女が温まれそう", "dungeon_event_witch_ready:" + event.id);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::HeavyRockWitch) {
            for (DungeonEventObject& object : event.eventObjects) {
                if (object.kind != DungeonEventObjectKind::HeavyRock || object.destroyed) {
                    continue;
                }
                int hitDamage = 0;
                if (hitEventObjectWithRing(object, DungeonEventHitRequirement::HeavyImpact, &hitDamage)) {
                    object.hp = std::max(0, object.hp - std::max(1, hitDamage));
                    if (object.hp <= 0) {
                        object.destroyed = true;
                        playAudioSe("se.dig.break");
                    }
                }
            }
            const bool rockCleared = !event.eventObjects.empty() &&
                std::all_of(event.eventObjects.begin(), event.eventObjects.end(), [](const DungeonEventObject& object) {
                    return object.kind != DungeonEventObjectKind::HeavyRock || object.destroyed;
            });
            if (rockCleared && !event.objectiveResolved) {
                event.objectiveResolved = true;
                pushDungeonLog("重い岩をどかした", "dungeon_event_witch_ready:" + event.id);
            }
            continue;
        }

        if (event.kind == DungeonEventKind::WarpGuideMap) {
            if (!event.activated) {
                const int targetIndex = nearestUndiscoveredWarpPointIndex(player_.position);
                event.activated = true;
                event.guideTargetWarpPointIndex = targetIndex;
                event.guideRemainingSeconds = targetIndex >= 0 ? DungeonEventGuideSeconds : 0.0f;
                if (targetIndex >= 0) {
                    pushDungeonLog("ワープの方角がミニマップに浮かんだ", "dungeon_event_warp_guide:" + event.id);
                    playAudioSe("se.discovery");
                } else {
                    completeDungeonEvent(event, std::nullopt);
                }
            }

            if (event.guideTargetWarpPointIndex >= 0 &&
                event.guideTargetWarpPointIndex < static_cast<int>(warpPoints_.size()) &&
                !warpPoints_[static_cast<std::size_t>(event.guideTargetWarpPointIndex)].discovered) {
                event.guideRemainingSeconds = std::max(0.0f, event.guideRemainingSeconds - dt);
                if (event.guideRemainingSeconds <= 0.0f) {
                    completeDungeonEvent(event, std::nullopt);
                }
            } else {
                completeDungeonEvent(event, std::nullopt);
            }
            continue;
        }
    }
}

void Game::handleDungeonEventEnemyEvent(const EnemyEvent& enemyEvent)
{
    if ((enemyEvent.type != EnemyEventType::Death && enemyEvent.type != EnemyEventType::BossDeath) ||
        enemyEvent.dungeonEventId.empty()) {
        return;
    }

    DungeonEventInstance* event = dungeonEvents_.findById(enemyEvent.dungeonEventId);
    if (event == nullptr || event->completed) {
        return;
    }
    if (event->kind == DungeonEventKind::BossMonsterRoom &&
        event->bossEnemyRuntimeId > 0 &&
        event->bossEnemyRuntimeId == enemyEvent.enemyRuntimeId) {
        event->bossDefeated = true;
        pushDungeonLog("親分を倒した", "dungeon_event_boss_defeated:" + event->id);
    }
}

bool Game::updateDungeonEventNpcInteraction(const Input& input, UiContext& ui)
{
    if (mode_ != ScreenMode::Playing ||
        enemyTestActive_ ||
        dungeonFocusActive() ||
        dialogue_.active() ||
        screenTransition_.active() ||
        worldBuildActive() ||
        !(input.confirmPressed() || input.useItemPressed())) {
        return false;
    }

    const float tileSize = static_cast<float>(balance::TileSize);
    const float talkRadius = DungeonEventNpcTalkRadiusTiles * tileSize;
    const float talkRadiusSq = talkRadius * talkRadius;
    DungeonEventInstance* target = nullptr;
    float bestDistanceSq = talkRadiusSq;
    for (DungeonEventInstance& event : dungeonEvents_.mutableAll()) {
        if (!event.discovered || !dungeonEventKindIsWitch(event.kind)) {
            continue;
        }
        const float distSq = distanceSquared(player_.position, tileWorldCenter(event.focusTile));
        if (distSq <= bestDistanceSq) {
            bestDistanceSq = distSq;
            target = &event;
        }
    }
    if (target == nullptr) {
        return false;
    }

    DungeonEventInstance& event = *target;
    if (event.kind == DungeonEventKind::ItemRequestWitch && event.requestKey.empty()) {
        event.requestKey = dungeonEventItemRequestKeyFor(event);
    }

    DungeonEventNpcDialoguePhase phase = DungeonEventNpcDialoguePhase::Progress;
    std::function<void()> onComplete;

    if (event.completed || event.rewardClaimed) {
        phase = DungeonEventNpcDialoguePhase::Thanks;
    } else if (event.objectiveResolved) {
        event.npcRequestKnown = true;
        phase = DungeonEventNpcDialoguePhase::Reward;
    } else if (!event.npcRequestKnown) {
        event.npcRequestKnown = true;
        phase = DungeonEventNpcDialoguePhase::Request;
    } else if (event.kind == DungeonEventKind::ItemRequestWitch) {
        std::string objectId;
        std::string displayName;
        if (consumeDungeonEventRequestItem(inventory_, event.requestKey, objectId, displayName)) {
            event.deliveredObjectId = objectId;
            event.objectiveResolved = true;
            event.activated = true;
            phase = DungeonEventNpcDialoguePhase::Reward;
            pushDungeonLog("魔女に" + displayName + "を渡した", "dungeon_event_item_request:" + event.id);
        } else {
            phase = DungeonEventNpcDialoguePhase::Progress;
        }
    } else {
        phase = DungeonEventNpcDialoguePhase::Progress;
    }

    if (phase == DungeonEventNpcDialoguePhase::Reward) {
        const auto witchRewardRequest = [](DungeonEventKind kind) -> std::optional<DungeonEventRewardRequest> {
            switch (kind) {
            case DungeonEventKind::BuriedWitch:
                return DungeonEventRewardRequest{
                    .kind = DungeonEventRewardKind::MoneyDrop,
                    .amount = 25,
                };
            case DungeonEventKind::LostBaggageWitch:
                return DungeonEventRewardRequest{
                    .kind = DungeonEventRewardKind::MoneyDrop,
                    .amount = 30,
                };
            case DungeonEventKind::ItemRequestWitch:
            case DungeonEventKind::ColdWitchCampfire:
                return DungeonEventRewardRequest{
                    .kind = DungeonEventRewardKind::MaterialDrop,
                    .materialType = MaterialType::ManaDrop,
                    .amount = 1,
                };
            case DungeonEventKind::SurroundedWitch:
            case DungeonEventKind::HeavyRockWitch:
                return DungeonEventRewardRequest{
                    .kind = DungeonEventRewardKind::ChestDrop,
                    .chestKind = LootChestKind::Common,
                    .count = 1,
                };
            default:
                return std::nullopt;
            }
        };
        const std::string eventId = event.id;
        const std::optional<DungeonEventRewardRequest> reward = witchRewardRequest(event.kind);
        onComplete = [this, eventId, reward]() {
            DungeonEventInstance* completedEvent = dungeonEvents_.findById(eventId);
            if (completedEvent == nullptr || completedEvent->completed) {
                return;
            }
            completedEvent->rewardClaimed = true;
            completeDungeonEvent(*completedEvent, reward);
        };
    }

    const std::string storyEventId = dungeonEventNpcStoryEventId(event.kind, phase);
    bool started = false;
    if (findStoryEvent(storyEventId) != nullptr) {
        started = startStoryEventWithCompletion(storyEventId, onComplete);
    }
    if (!started) {
        started = startDialogueSequenceWithCompletion(
            makeDungeonEventWitchDialogueSequence(event, phase),
            std::move(onComplete));
    }
    if (!started) {
        logWarning("[dungeon_event] failed to start npc dialogue: " + event.id);
        return false;
    }

    ui.emitSound(UiSoundEvent::Confirm);
    ui.consumePointer();
    return true;
}

std::string Game::dungeonEventNpcPromptText() const
{
    if (mode_ != ScreenMode::Playing ||
        enemyTestActive_ ||
        dungeonFocusActive() ||
        dialogue_.active() ||
        screenTransition_.active() ||
        worldBuildActive()) {
        return {};
    }

    const float tileSize = static_cast<float>(balance::TileSize);
    const float talkRadius = DungeonEventNpcTalkRadiusTiles * tileSize;
    const float talkRadiusSq = talkRadius * talkRadius;
    const DungeonEventInstance* target = nullptr;
    float bestDistanceSq = talkRadiusSq;
    for (const DungeonEventInstance& event : dungeonEvents_.all()) {
        if (!event.discovered || !dungeonEventKindIsWitch(event.kind)) {
            continue;
        }
        const float distSq = distanceSquared(player_.position, tileWorldCenter(event.focusTile));
        if (distSq <= bestDistanceSq) {
            bestDistanceSq = distSq;
            target = &event;
        }
    }
    if (target == nullptr) {
        return {};
    }
    if (target->completed || target->rewardClaimed) {
        return "魔女   F/Enter 話す";
    }
    if (target->objectiveResolved) {
        return "魔女   F/Enter お礼を受け取る";
    }
    if (target->npcRequestKnown) {
        return "魔女   F/Enter 条件を確認";
    }
    return "魔女   F/Enter 話す";
}

bool Game::updateDungeonEventDiscovery(float dt)
{
    dungeonEventDiscoveryCooldown_ = std::max(0.0f, dungeonEventDiscoveryCooldown_ - std::max(0.0f, dt));
    if (mode_ != ScreenMode::Playing ||
        enemyTestActive_ ||
        dungeonFocusActive() ||
        dialogue_.active() ||
        dungeonRingIntroActive() ||
        screenTransition_.active() ||
        worldBuildActive()) {
        return false;
    }
    if (dungeonEventDiscoveryCooldown_ > 0.0f) {
        return false;
    }
    if (player_.damageFlash > 0.0f) {
        dungeonEventDiscoveryCooldown_ = DungeonEventDamageInterruptDelaySeconds;
        return false;
    }

    const float tileSize = static_cast<float>(balance::TileSize);
    DungeonEventInstance* event = dungeonEvents_.firstDiscoverable(player_.position, tileSize);
    if (event == nullptr) {
        return false;
    }
    if (event->kind == DungeonEventKind::WarpGuideMap) {
        const bool hasUndiscoveredWarpPoint = warpPointsEnabled_ &&
            std::any_of(warpPoints_.begin(), warpPoints_.end(), [](const WarpPoint& point) {
                return !point.discovered;
            });
        if (!hasUndiscoveredWarpPoint) {
            event->completed = true;
            return false;
        }
    }

    if (dungeonEventKindUsesDiscoveryFocus(event->kind)) {
        const std::string seenFlag = dungeonEventDiscoverySeenFlag(event->kind);
        DungeonFocusRequest request;
        request.eventKind = dungeonEventKindId(event->kind);
        request.focusWorldPos = tileWorldCenter(event->focusTile);
        request.discoveryStoryEventId =
            dungeonEventKindHasDiscoveryDialogue(event->kind) && !hasStoryFlag(seenFlag)
                ? dungeonEventDiscoveryStoryEventId(event->kind)
                : std::string{};
        request.holdSecondsIfNoDialogue = DungeonFocusDefaultHoldSeconds;
        if (!requestDungeonFocus(std::move(request))) {
            return false;
        }
    }

    event->discovered = true;
    dungeonEventDiscoveryCooldown_ = DungeonEventDiscoveryCooldownSeconds;
    playAudioSe(AudioSeDiscovery);
    pushDungeonLog(
        std::string("発見: ") + dungeonEventKindDisplayName(event->kind),
        "dungeon_event:" + std::string(dungeonEventKindId(event->kind)));
    return true;
}

void Game::appendDungeonEventRenderEntries(
    std::vector<DepthRenderEntry>& entries,
    Renderer& renderer,
    const std::vector<LightSource>& extraLights,
    double totalSeconds) const
{
    const bool debugVisible = debug_.visible();
    const float tileSize = static_cast<float>(balance::TileSize);
    const Vec2 playerLightCenter = witchSelfLightCenter(player_.position);
    for (const DungeonEventInstance& event : dungeonEvents_.all()) {
        if (!isDungeonEventMarkerKind(event.kind)) {
            continue;
        }

        const Vec2 center = tileWorldCenter(event.focusTile);
        const Vec2 selfLightCenter = dungeonEventSelfLightPosition(event);
        const bool centerVisible = debugVisible || tileMap_.isLit(center, playerLightCenter, extraLights);
        if (debugVisible) {
            entries.push_back(DepthRenderEntry{
                center.y - 5.0f,
                [&renderer, center, selfLightCenter, discoveryRadius = event.discoveryRadiusTiles * tileSize, selfLightRadius = dungeonEventSelfLightRadiusPx(event)]() {
                    renderer.drawCircle(center, discoveryRadius, {255, 238, 130, 92});
                    renderer.drawCircle(selfLightCenter, selfLightRadius, {120, 220, 255, 72});
                    renderer.fillCircle(center, 3.0f, {255, 255, 255, 220});
                },
            });
        }

        if (centerVisible && (event.kind == DungeonEventKind::WarpGuideMap || dungeonEventKindIsWitch(event.kind))) {
            entries.push_back(DepthRenderEntry{
                center.y - 4.0f,
                [&renderer, center, kind = event.kind, completed = event.completed, discovered = event.discovered]() {
                    const unsigned char alpha = static_cast<unsigned char>(completed ? 145 : 245);
                    if (kind == DungeonEventKind::WarpGuideMap) {
                        const Color frame = discovered ? Color{150, 245, 210, 190} : Color{255, 230, 150, 190};
                        renderer.fillRect(center + Vec2{-14.0f, -10.0f}, {28.0f, 20.0f}, {76, 126, 156, alpha});
                        renderer.drawRect(center + Vec2{-14.0f, -10.0f}, {28.0f, 20.0f}, frame);
                        renderer.drawLine(center + Vec2{-6.0f, -8.0f}, center + Vec2{-6.0f, 8.0f}, {190, 238, 222, alpha});
                        renderer.drawLine(center + Vec2{5.0f, -8.0f}, center + Vec2{5.0f, 8.0f}, {190, 238, 222, alpha});
                        return;
                    }

                    renderer.fillCircle(center + Vec2{0.0f, -9.0f}, 6.0f, {246, 218, 206, alpha});
                    renderer.fillRect(center + Vec2{-7.0f, -3.0f}, {14.0f, 17.0f}, {112, 78, 156, alpha});
                    renderer.drawLine(center + Vec2{-10.0f, -3.0f}, center + Vec2{10.0f, -3.0f}, {230, 202, 255, alpha});
                    renderer.drawLine(center + Vec2{-5.0f, 14.0f}, center + Vec2{-10.0f, 21.0f}, {96, 66, 132, alpha});
                    renderer.drawLine(center + Vec2{5.0f, 14.0f}, center + Vec2{10.0f, 21.0f}, {96, 66, 132, alpha});
                    if (kind == DungeonEventKind::BuriedWitch) {
                        renderer.fillCircle(center + Vec2{-14.0f, 7.0f}, 5.0f, {118, 96, 72, alpha});
                    } else if (kind == DungeonEventKind::LostBaggageWitch) {
                        renderer.fillRect(center + Vec2{10.0f, 6.0f}, {12.0f, 9.0f}, {174, 116, 68, alpha});
                    } else if (kind == DungeonEventKind::ItemRequestWitch) {
                        renderer.drawText(center + Vec2{11.0f, -20.0f}, "?", {255, 238, 150, alpha}, 2);
                    } else if (kind == DungeonEventKind::ColdWitchCampfire) {
                        renderer.drawLine(center + Vec2{12.0f, 12.0f}, center + Vec2{19.0f, 4.0f}, {255, 188, 108, alpha});
                    } else if (kind == DungeonEventKind::HeavyRockWitch) {
                        renderer.fillCircle(center + Vec2{15.0f, 8.0f}, 7.0f, {104, 108, 118, alpha});
                    }
                },
            });
        }
        if (event.kind == DungeonEventKind::NestRoom) {
            for (const DungeonEventNestHole& hole : event.nestHoles) {
                const Vec2 holeCenter = tileWorldCenter(hole.tile);
                if (!debugVisible && !tileMap_.isLit(holeCenter, playerLightCenter, extraLights)) {
                    continue;
                }
                entries.push_back(DepthRenderEntry{
                    holeCenter.y - 2.0f,
                    [&renderer, holeCenter, hp = hole.hp, maxHp = hole.maxHp, destroyed = hole.destroyed]() {
                        const unsigned char alpha = static_cast<unsigned char>(destroyed ? 120 : 245);
                        renderer.drawCircle(holeCenter, 18.0f, {150, 220, 130, alpha});
                        renderer.fillCircle(holeCenter, 10.0f, destroyed ? Color{80, 86, 72, alpha} : Color{78, 118, 66, alpha});
                        renderer.fillCircle(holeCenter, 5.0f, destroyed ? Color{42, 42, 38, alpha} : Color{22, 34, 24, alpha});
                        if (!destroyed && maxHp > 0 && hp < maxHp) {
                            const float ratio = clamp(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f);
                            renderer.fillRect(holeCenter + Vec2{-12.0f, -22.0f}, {24.0f, 3.0f}, {28, 16, 18, 210});
                            renderer.fillRect(holeCenter + Vec2{-12.0f, -22.0f}, {24.0f * ratio, 3.0f}, {154, 232, 116, 230});
                        }
                    },
                });
            }
        }
        for (const DungeonEventObject& object : event.eventObjects) {
            if (object.destroyed) {
                continue;
            }
            const Vec2 objectCenter = tileWorldCenter(object.tile);
            if (!debugVisible && !tileMap_.isLit(objectCenter, playerLightCenter, extraLights)) {
                continue;
            }
            entries.push_back(DepthRenderEntry{
                objectCenter.y - 2.0f,
                [&renderer,
                    objectCenter,
                    kind = object.kind,
                    hp = object.hp,
                    maxHp = object.maxHp,
                    powered = object.powered,
                    totalSeconds]() {
                    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(totalSeconds) * (powered ? 5.8f : 3.6f));
                    switch (kind) {
                    case DungeonEventObjectKind::GlowingRock:
                        renderer.drawCircle(objectCenter, 17.0f + pulse * 2.0f, {110, 242, 220, 230});
                        renderer.fillCircle(objectCenter, 10.0f, {72, 174, 186, 238});
                        renderer.drawLine(objectCenter + Vec2{-9.0f, 1.0f}, objectCenter + Vec2{-1.0f, -11.0f}, {224, 255, 246, 230});
                        renderer.drawLine(objectCenter + Vec2{-1.0f, -11.0f}, objectCenter + Vec2{10.0f, 0.0f}, {224, 255, 246, 230});
                        renderer.drawLine(objectCenter + Vec2{10.0f, 0.0f}, objectCenter + Vec2{1.0f, 11.0f}, {92, 118, 132, 210});
                        break;
                    case DungeonEventObjectKind::ElectricReceiver: {
                        const Color core = powered ? Color{246, 250, 154, 245} : Color{94, 176, 232, 235};
                        const Color wire = powered ? Color{255, 238, 112, 230} : Color{94, 208, 255, 190};
                        renderer.drawCircle(objectCenter, 16.0f + pulse * 2.0f, wire);
                        renderer.fillCircle(objectCenter, 7.0f, core);
                        renderer.drawLine(objectCenter + Vec2{-15.0f, 0.0f}, objectCenter + Vec2{-6.0f, 0.0f}, wire);
                        renderer.drawLine(objectCenter + Vec2{6.0f, 0.0f}, objectCenter + Vec2{15.0f, 0.0f}, wire);
                        renderer.drawLine(objectCenter + Vec2{0.0f, -15.0f}, objectCenter + Vec2{0.0f, -6.0f}, wire);
                        renderer.drawLine(objectCenter + Vec2{0.0f, 6.0f}, objectCenter + Vec2{0.0f, 15.0f}, wire);
                        break;
                    }
                    case DungeonEventObjectKind::BuriedDebris:
                        renderer.drawCircle(objectCenter, 13.0f, {168, 134, 92, 210});
                        renderer.fillCircle(objectCenter, 9.0f, {122, 94, 66, 235});
                        renderer.drawLine(objectCenter + Vec2{-8.0f, 2.0f}, objectCenter + Vec2{6.0f, -6.0f}, {210, 188, 142, 220});
                        break;
                    case DungeonEventObjectKind::LostBaggage:
                        renderer.drawCircle(objectCenter, 14.0f + pulse * 1.5f, {255, 218, 128, 210});
                        renderer.fillRect(objectCenter + Vec2{-10.0f, -7.0f}, {20.0f, 15.0f}, {172, 106, 58, 238});
                        renderer.drawRect(objectCenter + Vec2{-10.0f, -7.0f}, {20.0f, 15.0f}, {255, 222, 146, 225});
                        renderer.drawLine(objectCenter + Vec2{-4.0f, -7.0f}, objectCenter + Vec2{4.0f, -7.0f}, {92, 56, 34, 230});
                        break;
                    case DungeonEventObjectKind::Campfire:
                        renderer.drawCircle(objectCenter, powered ? 22.0f + pulse * 3.0f : 13.0f, powered ? Color{255, 182, 86, 220} : Color{122, 150, 170, 180});
                        renderer.drawLine(objectCenter + Vec2{-11.0f, 8.0f}, objectCenter + Vec2{11.0f, -2.0f}, {116, 78, 48, 230});
                        renderer.drawLine(objectCenter + Vec2{-11.0f, -2.0f}, objectCenter + Vec2{11.0f, 8.0f}, {116, 78, 48, 230});
                        if (powered) {
                            renderer.fillCircle(objectCenter + Vec2{0.0f, -5.0f}, 7.0f, {255, 116, 54, 238});
                            renderer.fillCircle(objectCenter + Vec2{0.0f, -8.0f}, 4.0f, {255, 234, 112, 240});
                        } else {
                            renderer.fillCircle(objectCenter + Vec2{0.0f, -4.0f}, 5.0f, {70, 76, 82, 225});
                        }
                        break;
                    case DungeonEventObjectKind::HeavyRock:
                        renderer.drawCircle(objectCenter, 17.0f, {138, 144, 154, 210});
                        renderer.fillCircle(objectCenter, 12.0f, {92, 96, 106, 238});
                        renderer.drawLine(objectCenter + Vec2{-10.0f, 3.0f}, objectCenter + Vec2{-2.0f, -10.0f}, {170, 178, 188, 210});
                        renderer.drawLine(objectCenter + Vec2{-2.0f, -10.0f}, objectCenter + Vec2{10.0f, 0.0f}, {170, 178, 188, 210});
                        break;
                    }
                    if (maxHp > 1 && hp < maxHp) {
                        const float ratio = clamp(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f);
                        renderer.fillRect(objectCenter + Vec2{-12.0f, -21.0f}, {24.0f, 3.0f}, {22, 24, 28, 210});
                        renderer.fillRect(objectCenter + Vec2{-12.0f, -21.0f}, {24.0f * ratio, 3.0f}, {102, 236, 218, 235});
                    }
                },
            });
        }
    }
}

bool Game::spawnDungeonEventReward(DungeonEventInstance& event, const DungeonEventRewardRequest& request)
{
    if (event.rewardSpawned) {
        return false;
    }

    const Vec2 center = tileWorldCenter(event.focusTile);
    std::mt19937 rng(
        dungeonLayout_.seed ^
        (static_cast<std::uint32_t>(event.centerTile.x) * 0x7F4A7C15u) ^
        (static_cast<std::uint32_t>(event.centerTile.y) * 0x94D049BBu) ^
        static_cast<std::uint32_t>(std::hash<std::string>{}(event.id)));

    bool spawned = false;
    switch (request.kind) {
    case DungeonEventRewardKind::ChestDrop:
    case DungeonEventRewardKind::MultiChestDrop: {
        const int count = std::max(1, request.kind == DungeonEventRewardKind::ChestDrop ? 1 : request.count);
        constexpr std::array<DungeonTile, 9> ChestOffsets{{
            {0, 0},
            {1, 0},
            {-1, 0},
            {0, 1},
            {0, -1},
            {1, 1},
            {-1, 1},
            {1, -1},
            {-1, -1},
        }};
        for (int i = 0; i < count; ++i) {
            const DungeonTile offset = ChestOffsets[static_cast<std::size_t>(i % static_cast<int>(ChestOffsets.size()))];
            const DungeonTile tile = dungeonEventOffsetTile(event.rewardTile, offset.x, offset.y);
            const bool alreadyPresent = std::any_of(chestNodes_.begin(), chestNodes_.end(), [tile](const ChestNode& node) {
                return sameDungeonTile(node.tile, tile);
            });
            const bool chestSpawned = ensureDungeonEventChest(event, tile, request.chestKind);
            spawned = alreadyPresent || chestSpawned || spawned;
        }
        break;
    }
    case DungeonEventRewardKind::MaterialDrop: {
        const int amount = std::max(1, request.amount);
        const Vec2 target = safeLootLandingPosition(center, rng);
        spawned = worldDrops_.spawnMaterialDrop(
            request.materialType,
            amount,
            target,
            runStats_.elapsedSeconds,
            makeWorldLootJumpMotion(center, rng));
        break;
    }
    case DungeonEventRewardKind::MoneyDrop: {
        const int amount = std::max(1, request.amount);
        const Vec2 target = safeLootLandingPosition(center, rng);
        spawned = worldDrops_.spawnMoneyDrop(
            amount,
            target,
            runStats_.elapsedSeconds,
            makeWorldLootJumpMotion(center, rng));
        break;
    }
    }

    if (spawned) {
        event.rewardSpawned = true;
        if (request.kind == DungeonEventRewardKind::ChestDrop ||
            request.kind == DungeonEventRewardKind::MultiChestDrop) {
            requestDungeonRewardChestFocus(tileWorldCenter(event.rewardTile));
        } else {
            event.spawnedEntityIds.push_back("world_drop");
        }
    }
    return spawned;
}

void Game::completeDungeonEvent(DungeonEventInstance& event, std::optional<DungeonEventRewardRequest> reward)
{
    if (event.completed) {
        return;
    }
    if (reward) {
        spawnDungeonEventReward(event, *reward);
    }
    if (dungeonEventKindIsWitch(event.kind)) {
        event.objectiveResolved = true;
        event.rewardClaimed = true;
    }
    event.completed = true;
    pushDungeonLog(
        std::string("完了: ") + dungeonEventKindDisplayName(event.kind),
        "dungeon_event_complete:" + std::string(dungeonEventKindId(event.kind)));
}

bool Game::ensureDungeonEventChest(DungeonEventInstance& event, DungeonTile tile, LootChestKind chestKind)
{
    auto existing = std::find_if(chestNodes_.begin(), chestNodes_.end(), [tile](const ChestNode& node) {
        return dungeonEventSameTile(node.tile, tile);
    });
    if (existing != chestNodes_.end()) {
        existing->visibility = PlacementVisibility::Exposed;
        existing->revealed = true;
        existing->chestKind = chestKind;
        return false;
    }

    const int depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, tileWorldCenter(tile));
    const bool spawned = spawnAppearingChestNode(
        tile,
        chestKind,
        depthRank,
        tileWorldCenter(event.focusTile),
        "dungeon_event_chest:" + event.id + ":" + std::to_string(tile.x) + ":" + std::to_string(tile.y));
    if (!spawned) {
        return false;
    }
    event.spawnedEntityIds.push_back("chest:" + std::to_string(tile.x) + ":" + std::to_string(tile.y));
    return true;
}

void Game::requestDungeonRewardChestFocus(Vec2 focusWorldPos)
{
    DungeonFocusRequest request;
    request.eventKind = "reward_chest";
    request.focusWorldPos = focusWorldPos;
    request.holdSecondsIfNoDialogue = DungeonRewardFocusHoldSeconds;
    request.moveSeconds = DungeonRewardFocusMoveSeconds;
    request.returnSeconds = DungeonRewardFocusMoveSeconds;
    requestDungeonFocus(std::move(request));
}

bool Game::debugRequestDungeonEventPlacement(DungeonEventKind kind)
{
    if (mode_ == ScreenMode::Playing && !enemyTestActive_ && !worldBuildActive()) {
        return debugPlaceDungeonEvent(kind);
    }
    const bool dungeonOverlayActive =
        pauseReturnMode_ == ScreenMode::Playing &&
        (mode_ == ScreenMode::PauseMenu || mode_ == ScreenMode::Inventory || mode_ == ScreenMode::Ring);
    if (dungeonOverlayActive && !enemyTestActive_ && !worldBuildActive()) {
        inventory_.setOpen(false);
        inventory_.cancelGrab();
        cancelRingGrab();
        mode_ = ScreenMode::Playing;
        pauseReturnMode_ = ScreenMode::Playing;
        return debugPlaceDungeonEvent(kind);
    }

    pendingDebugDungeonEventPlacement_ = kind;
    if (worldBuildActive()) {
        logInfo("Debug: dungeon event placement queued until world build completes.");
        return true;
    }

    if (enemyTestActive_) {
        exitEnemyTestToBase();
    }
    startMiningFromBase(false, false);
    logInfo("Debug: moving to dungeon for event placement.");
    return true;
}

void Game::flushPendingDebugDungeonEventPlacement()
{
    if (!pendingDebugDungeonEventPlacement_) {
        return;
    }
    const DungeonEventKind kind = *pendingDebugDungeonEventPlacement_;
    pendingDebugDungeonEventPlacement_.reset();
    debugPlaceDungeonEvent(kind);
}

bool Game::debugPlaceDungeonEvent(DungeonEventKind kind)
{
    if (mode_ != ScreenMode::Playing || worldBuildActive()) {
        logWarning("Debug: dungeon event placement requires an active dungeon run.");
        return false;
    }
    if (kind == DungeonEventKind::WarpGuideMap) {
        const bool hasUndiscoveredWarpPoint = warpPointsEnabled_ &&
            std::any_of(warpPoints_.begin(), warpPoints_.end(), [](const WarpPoint& point) {
                return !point.discovered;
            });
        if (!hasUndiscoveredWarpPoint) {
            logWarning("Debug: warp guide event placement skipped because there are no undiscovered warp points.");
            return false;
        }
    }

    const float tileSize = static_cast<float>(balance::TileSize);
    const std::array<Vec2, 8> DirectionOffsets{{
        {4.0f, 0.0f},
        {0.0f, -4.0f},
        {0.0f, 4.0f},
        {-4.0f, 0.0f},
        {3.0f, -3.0f},
        {3.0f, 3.0f},
        {-3.0f, -3.0f},
        {-3.0f, 3.0f},
    }};

    Vec2 placement = player_.position + DirectionOffsets.front() * tileSize;
    for (Vec2 offset : DirectionOffsets) {
        const Vec2 candidate = safePlayerStartPosition(player_.position + offset * tileSize);
        if (distanceSquared(candidate, player_.position) >= tileSize * tileSize) {
            placement = candidate;
            break;
        }
    }

    const DungeonTile centerTile{
        tileMap_.worldToTile(placement.x),
        tileMap_.worldToTile(placement.y),
    };

    const std::string kindId = dungeonEventKindId(kind);
    int serial = 1;
    std::string eventId;
    do {
        eventId = "debug_event_" + kindId + "_" + std::to_string(serial++);
    } while (dungeonEvents_.findById(eventId) != nullptr);

    DungeonEventInstance event;
    event.id = eventId;
    event.kind = kind;
    event.centerTile = centerTile;
    event.focusTile = centerTile;
    event.rewardTile = centerTile;
    event.discoveryRadiusTiles = dungeonEventDiscoveryRadiusTiles(kind);
    event.selfLightRadiusTiles = dungeonEventLightRadiusTiles(kind);
    event.data = "debug";
    event.params = "source=debug";

    dungeonEvents_.mutableAll().push_back(std::move(event));
    applyDungeonEventCavity(dungeonEvents_.mutableAll().back());
    normalizeOpenBuriedPlacementNodes();
    dungeonEventDiscoveryCooldown_ = 0.0f;
    logInfo("Debug: placed dungeon event " + kindId + " near player.");
    pushDungeonLog(
        std::string("デバッグ配置: ") + dungeonEventKindDisplayName(kind),
        "debug_dungeon_event:" + kindId);
    return true;
}

std::string Game::nearestDungeonEventDebugText() const
{
    if (dungeonEvents_.empty()) {
        return "DungeonEvent: none";
    }

    const DungeonEventInstance* nearest = dungeonEvents_.nearest(player_.position);
    if (nearest == nullptr) {
        return "DungeonEvent: none";
    }
    const float nearestDistanceSq = distanceSquared(player_.position, tileWorldCenter(nearest->centerTile));
    const auto& allEvents = dungeonEvents_.all();
    const int discoveredCount = static_cast<int>(std::count_if(allEvents.begin(), allEvents.end(), [](const DungeonEventInstance& event) {
        return event.discovered;
    }));
    const int completedCount = static_cast<int>(std::count_if(allEvents.begin(), allEvents.end(), [](const DungeonEventInstance& event) {
        return event.completed;
    }));
    const int witchCount = static_cast<int>(std::count_if(allEvents.begin(), allEvents.end(), [](const DungeonEventInstance& event) {
        return dungeonEventKindIsWitch(event.kind);
    }));
    const int combatCount = static_cast<int>(std::count_if(allEvents.begin(), allEvents.end(), [](const DungeonEventInstance& event) {
        return dungeonEventKindIsCombat(event.kind);
    }));

    char buffer[512];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "DungeonEvent: total=%zu discovered=%d completed=%d combat=%d witch=%d cooldown=%.1f nearest %s id=%s discovered=%s completed=%s npcKnown=%s resolved=%s reward=%s objs=%zu guide=%.1f dist=%.1ft",
        allEvents.size(),
        discoveredCount,
        completedCount,
        combatCount,
        witchCount,
        dungeonEventDiscoveryCooldown_,
        dungeonEventKindId(nearest->kind),
        nearest->id.empty() ? "-" : nearest->id.c_str(),
        nearest->discovered ? "true" : "false",
        nearest->completed ? "true" : "false",
        nearest->npcRequestKnown ? "true" : "false",
        nearest->objectiveResolved ? "true" : "false",
        nearest->rewardSpawned ? "true" : "false",
        nearest->eventObjects.size(),
        nearest->guideRemainingSeconds,
        std::sqrt(nearestDistanceSq) / static_cast<float>(balance::TileSize));
    return buffer;
}

const char* Game::dungeonFocusPhaseName(DungeonFocusPhase phase)
{
    switch (phase) {
    case DungeonFocusPhase::Idle: return "Idle";
    case DungeonFocusPhase::MoveToTarget: return "MoveToTarget";
    case DungeonFocusPhase::Hold: return "Hold";
    case DungeonFocusPhase::PlayDiscoveryDialogue: return "PlayDiscoveryDialogue";
    case DungeonFocusPhase::ReturnToPlayer: return "ReturnToPlayer";
    }
    return "Unknown";
}

bool Game::dungeonFocusActive() const
{
    return dungeonFocus_.phase != DungeonFocusPhase::Idle;
}

void Game::resetDungeonFocus()
{
    dungeonFocus_ = DungeonFocusState{};
}

std::string Game::dungeonFocusDebugText() const
{
    char buffer[384];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "DungeonFocus: %s kind=%s target=(%.1f, %.1f) timer=%.2f/%.2f story=%s",
        dungeonFocusPhaseName(dungeonFocus_.phase),
        dungeonFocus_.eventKind.empty() ? "-" : dungeonFocus_.eventKind.c_str(),
        dungeonFocus_.focusWorldPos.x,
        dungeonFocus_.focusWorldPos.y,
        dungeonFocus_.elapsed,
        dungeonFocus_.phase == DungeonFocusPhase::Hold ? dungeonFocus_.holdSeconds : dungeonFocus_.duration,
        dungeonFocus_.discoveryStoryEventId.empty() ? "-" : dungeonFocus_.discoveryStoryEventId.c_str());
    return buffer;
}

bool Game::requestDungeonFocus(DungeonFocusRequest request)
{
    if (dungeonFocusActive()) {
        logInfo(
            "[dungeon_focus] ignored request while active: kind=" +
            (request.eventKind.empty() ? std::string("-") : request.eventKind) +
            " current=" + dungeonFocusPhaseName(dungeonFocus_.phase));
        return false;
    }
    if (mode_ != ScreenMode::Playing || screenTransition_.active() || worldBuildActive()) {
        logWarning(
            "[dungeon_focus] rejected request outside active dungeon: kind=" +
            (request.eventKind.empty() ? std::string("-") : request.eventKind) +
            " mode=" + screenModeName(mode_));
        return false;
    }
    if (dialogue_.active() ||
        firstItemAcquisitionNoticeActive() ||
        warpReturnConfirm_.open ||
        bossEncounterBlocksProgress() ||
        dungeonRingIntroActive() ||
        endingKamishibaiPending_ ||
        levels_.isChoosing()) {
        logWarning(
            "[dungeon_focus] rejected request during another blocking presentation: kind=" +
            (request.eventKind.empty() ? std::string("-") : request.eventKind));
        return false;
    }

    Vec2 focusWorldPos = request.focusWorldPos;
    float holdSeconds = dungeonFocusHoldSeconds(request.holdSecondsIfNoDialogue);
    const float moveSeconds = dungeonFocusDurationSeconds(request.moveSeconds, DungeonFocusMoveSeconds);
    const float returnSeconds = dungeonFocusDurationSeconds(request.returnSeconds, DungeonFocusMoveSeconds);
    std::string storyEventId = std::move(request.discoveryStoryEventId);
    DialogueSequence focusDialogue = std::move(request.discoveryDialogue);
    if (!validDungeonFocusPosition(focusWorldPos)) {
        logWarning("[dungeon_focus] invalid target position; falling back to current camera position");
        focusWorldPos = camera_.position();
        storyEventId.clear();
        focusDialogue = {};
        holdSeconds = DungeonFocusDefaultHoldSeconds;
    } else if (!storyEventId.empty()) {
        const StoryEvent* event = findStoryEvent(storyEventId);
        if (event == nullptr) {
            logWarning("[dungeon_focus] discovery story event not found: " + storyEventId);
            storyEventId.clear();
            holdSeconds = DungeonFocusDefaultHoldSeconds;
        } else if (event->dialogue.steps.empty() && event->dialogue.lines.empty()) {
            logWarning("[dungeon_focus] discovery story event has no dialogue: " + storyEventId);
            storyEventId.clear();
            holdSeconds = DungeonFocusDefaultHoldSeconds;
        }
    }
    dungeonFocus_ = DungeonFocusState{};
    dungeonFocus_.phase = DungeonFocusPhase::MoveToTarget;
    dungeonFocus_.eventKind = std::move(request.eventKind);
    dungeonFocus_.startCameraPos = camera_.position();
    dungeonFocus_.focusWorldPos = focusWorldPos;
    dungeonFocus_.moveFrom = dungeonFocus_.startCameraPos;
    dungeonFocus_.moveTo = focusWorldPos;
    dungeonFocus_.duration = moveSeconds;
    dungeonFocus_.holdSeconds = holdSeconds;
    dungeonFocus_.moveSeconds = moveSeconds;
    dungeonFocus_.returnSeconds = returnSeconds;
    dungeonFocus_.discoveryStoryEventId = std::move(storyEventId);
    dungeonFocus_.discoveryDialogue = std::move(focusDialogue);
    dungeonFocus_.onComplete = std::move(request.onComplete);

    logInfo(
        "[dungeon_focus] state=MoveToTarget kind=" +
        (dungeonFocus_.eventKind.empty() ? std::string("-") : dungeonFocus_.eventKind));
    return true;
}

bool Game::updateDungeonFocus(float dt)
{
    if (!dungeonFocusActive()) {
        return false;
    }

    const float safeDt = std::max(0.0f, dt);
    const auto beginHold = [this]() {
        dungeonFocus_.phase = DungeonFocusPhase::Hold;
        dungeonFocus_.elapsed = 0.0f;
        dungeonFocus_.duration = dungeonFocus_.holdSeconds;
        camera_.setPosition(dungeonFocus_.focusWorldPos);
        logInfo(
            "[dungeon_focus] state=Hold kind=" +
            (dungeonFocus_.eventKind.empty() ? std::string("-") : dungeonFocus_.eventKind));
    };
    const auto beginReturn = [this]() {
        dungeonFocus_.phase = DungeonFocusPhase::ReturnToPlayer;
        dungeonFocus_.elapsed = 0.0f;
        dungeonFocus_.duration = dungeonFocusDurationSeconds(dungeonFocus_.returnSeconds, DungeonFocusMoveSeconds);
        dungeonFocus_.moveFrom = camera_.position();
        dungeonFocus_.moveTo = player_.position;
        logInfo(
            "[dungeon_focus] state=ReturnToPlayer kind=" +
            (dungeonFocus_.eventKind.empty() ? std::string("-") : dungeonFocus_.eventKind));
    };
    const auto finish = [this]() {
        std::function<void()> onComplete = std::move(dungeonFocus_.onComplete);
        const std::string eventKind = dungeonFocus_.eventKind;
        resetDungeonFocus();
        logInfo("[dungeon_focus] state=Idle kind=" + (eventKind.empty() ? std::string("-") : eventKind));
        if (onComplete) {
            onComplete();
        }
    };
    const auto advanceMove = [this, safeDt]() {
        dungeonFocus_.elapsed += safeDt;
        const float duration = std::max(0.001f, dungeonFocus_.duration);
        const float t = clamp(dungeonFocus_.elapsed / duration, 0.0f, 1.0f);
        camera_.setPosition(lerp(dungeonFocus_.moveFrom, dungeonFocus_.moveTo, dungeonFocusEase(t)));
        return t >= 1.0f;
    };

    switch (dungeonFocus_.phase) {
    case DungeonFocusPhase::Idle:
        return false;
    case DungeonFocusPhase::MoveToTarget:
        if (!advanceMove()) {
            return true;
        }
        camera_.setPosition(dungeonFocus_.focusWorldPos);
        if (!dungeonFocus_.discoveryStoryEventId.empty()) {
            const std::string storyEventId = dungeonFocus_.discoveryStoryEventId;
            if (startStoryEvent(storyEventId)) {
                dungeonFocus_.phase = DungeonFocusPhase::PlayDiscoveryDialogue;
                dungeonFocus_.elapsed = 0.0f;
                dungeonFocus_.duration = 0.0f;
                logInfo("[dungeon_focus] state=PlayDiscoveryDialogue story=" + storyEventId);
                return true;
            }
            logInfo("[dungeon_focus] discovery dialogue skipped: " + storyEventId);
        }
        if (!dungeonFocus_.discoveryDialogue.steps.empty() || !dungeonFocus_.discoveryDialogue.lines.empty()) {
            dialogue_.start(std::move(dungeonFocus_.discoveryDialogue));
            if (dialogue_.active()) {
                dungeonFocus_.phase = DungeonFocusPhase::PlayDiscoveryDialogue;
                dungeonFocus_.elapsed = 0.0f;
                dungeonFocus_.duration = 0.0f;
                logInfo("[dungeon_focus] state=PlayDiscoveryDialogue sequence");
                return true;
            }
            logInfo("[dungeon_focus] inline dialogue skipped");
        }
        beginHold();
        return true;
    case DungeonFocusPhase::Hold:
        camera_.setPosition(dungeonFocus_.focusWorldPos);
        dungeonFocus_.elapsed += safeDt;
        if (dungeonFocus_.elapsed >= dungeonFocus_.holdSeconds) {
            beginReturn();
        }
        return true;
    case DungeonFocusPhase::PlayDiscoveryDialogue:
        camera_.setPosition(dungeonFocus_.focusWorldPos);
        if (!dialogue_.active()) {
            beginReturn();
        }
        return true;
    case DungeonFocusPhase::ReturnToPlayer:
        dungeonFocus_.moveTo = player_.position;
        if (!advanceMove()) {
            return true;
        }
        camera_.setPosition(player_.position);
        finish();
        return true;
    }

    return false;
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

Vec2 Game::safePlayerStartPosition(Vec2 preferredPosition)
{
    const std::vector<CollisionRect> objectBlockers = solidObjectCollisionRects();
    const auto blocked = [&](Vec2 position) {
        if (tileMap_.isCircleBlocked(position, balance_.playerRadius)) {
            return true;
        }
        return circleIntersectsAnyRect(
            position,
            balance_.playerRadius,
            std::span<const CollisionRect>{objectBlockers.data(), objectBlockers.size()});
    };

    if (!blocked(preferredPosition)) {
        return preferredPosition;
    }

    const DungeonTile preferredTile{
        tileMap_.worldToTile(preferredPosition.x),
        tileMap_.worldToTile(preferredPosition.y),
    };
    for (int ring = 1; ring <= SolidPlacementSearchRadiusTiles; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring) {
                    continue;
                }
                const Vec2 candidate = tileWorldCenter(DungeonTile{
                    preferredTile.x + dx,
                    preferredTile.y + dy,
                });
                if (!blocked(candidate)) {
                    return candidate;
                }
            }
        }
    }

    return preferredPosition;
}

Vec2 Game::dungeonEntrancePosition() const
{
    if (introTutorialActive()) {
        return introTutorialExitPosition();
    }
    return tileWorldCenter(dungeonLayout_.startTile) + Vec2{0.0f, DungeonEntranceYOffset};
}

int Game::nearbyDiscoveredWarpPointIndex() const
{
    if (!warpPointsEnabled_) {
        return -1;
    }

    int nearest = -1;
    float nearestDistanceSq = WarpPointReturnRadius * WarpPointReturnRadius;
    for (int i = 0; i < static_cast<int>(warpPoints_.size()); ++i) {
        const WarpPoint& point = warpPoints_[static_cast<std::size_t>(i)];
        if (!point.discovered) {
            continue;
        }
        const float distSq = distanceSquared(player_.position, point.position);
        if (distSq <= nearestDistanceSq) {
            nearestDistanceSq = distSq;
            nearest = i;
        }
    }
    return nearest;
}

bool Game::updateWarpReturnUi(const Input& input, UiContext& ui)
{
    if (mode_ != ScreenMode::Playing || enemyTestActive_ || introTutorialActive()) {
        warpReturnConfirm_ = {};
        focusedWarpReturnPointIndex_ = -1;
        return false;
    }

    if (warpReturnConfirm_.open) {
        const UiRect confirmPanel = warpReturnConfirmRect();
        const UiConfirmDialogResult result = updateUiConfirmDialog(warpReturnConfirm_, ui, input, confirmPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            focusedWarpReturnPointIndex_ = -1;
            if (currentStageIsRoguelike()) {
                enterAstralResult(AstralRunResult::Returned);
                return true;
            }
            requestReturnToBaseTransition(false, false);
            return true;
        }
        if (result == UiConfirmDialogResult::Cancelled) {
            const bool entranceNearby =
                distanceSquared(player_.position, dungeonEntrancePosition()) <= WarpPointReturnRadius * WarpPointReturnRadius;
            focusedWarpReturnPointIndex_ = entranceNearby
                ? DungeonEntranceReturnFocusIndex
                : nearbyDiscoveredWarpPointIndex();
            return true;
        }

        ui.block(confirmPanel);
        return true;
    }

    const bool entranceNearby =
        distanceSquared(player_.position, dungeonEntrancePosition()) <= WarpPointReturnRadius * WarpPointReturnRadius;
    focusedWarpReturnPointIndex_ = entranceNearby
        ? DungeonEntranceReturnFocusIndex
        : nearbyDiscoveredWarpPointIndex();
    const bool returnPromptFocused =
        focusedWarpReturnPointIndex_ >= 0 ||
        focusedWarpReturnPointIndex_ == DungeonEntranceReturnFocusIndex;
    if (returnPromptFocused && (input.confirmPressed() || input.useItemPressed())) {
        ui.emitSound(UiSoundEvent::MenuOpen);
        openUiConfirmDialog(
            warpReturnConfirm_,
            "帰還確認",
            "拠点へ帰還しますか？\n現在のダンジョン状態を保持したまま、ダンジョン入口へ戻ります。",
            "帰還する",
            "戻る",
            0);
        ui.block(warpReturnConfirmRect());
        return true;
    }
    return false;
}

bool Game::unlockAllWarpPointsForCurrentDungeon()
{
    if (mode_ != ScreenMode::Playing || enemyTestActive_ || !warpPointsEnabled_ || warpPoints_.empty()) {
        return false;
    }

    int newlyDiscovered = 0;
    Vec2 latestPosition{};
    bool hasLatest = false;
    for (WarpPoint& point : warpPoints_) {
        const bool wasDiscovered = point.discovered;
        point.discovered = true;
        point.unlocked = true;
        point.snapshotCaptured = true;
        if (!wasDiscovered) {
            ++newlyDiscovered;
            point.lightRevealTimer = 0.0f;
            point.lightRevealAnimating = true;
        }
        latestPosition = point.position;
        hasLatest = true;
    }

    unlockedWarpPointCount_ = discoveredWarpPointCount();
    latestWarpPointPosition_ = latestPosition;
    hasLatestWarpPointPosition_ = hasLatest;

    const int bossWarpPointIndex = std::max(0, static_cast<int>(warpPoints_.size()) - 1);
    for (const WarpPoint& point : warpPoints_) {
        if (point.index == bossWarpPointIndex) {
            configureBossSpawnPointFromWarp(point.position);
            queueStoryEventForCurrentStage("boss_before");
            break;
        }
    }

    captureRetrySnapshotAtWarpPoint();
    captureDungeonState();
    pushDungeonLog(
        newlyDiscovered > 0 ? "ワープポイント全開放" : "ワープポイントは全開放済み",
        "warp_point_all");
    if (newlyDiscovered > 0) {
        playAudioSe(AudioSeWarpDiscovery);
    }
    return true;
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
        if (distanceSquared(player_.position, point.position) <= WarpPointTouchRadius * WarpPointTouchRadius) {
            if (player_.heal(player_.maxHp) > 0) {
                magicFx_.playHealPulse(player_.position, 30.0f);
            }
            if (point.discovered) {
                continue;
            }
            point.discovered = true;
            point.unlocked = true;
            unlockedWarpPointCount_ = std::max(unlockedWarpPointCount_, discoveredWarpPointCount());
            latestWarpPointPosition_ = point.position;
            hasLatestWarpPointPosition_ = true;
            point.snapshotCaptured = true;
            const int bossWarpPointIndex = std::max(0, static_cast<int>(warpPoints_.size()) - 1);
            if (point.index == bossWarpPointIndex) {
                configureBossSpawnPointFromWarp(point.position);
                queueStoryEventForCurrentStage("boss_before");
            }
            captureRetrySnapshotAtWarpPoint();
            point.lightRevealTimer = 0.0f;
            point.lightRevealAnimating = true;
            pushDungeonLog("ワープポイント発見", "warp_point");
            playAudioSe(AudioSeWarpDiscovery);
            queueStoryEventForTrigger("tutorial:warp");
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

    PlacementReservations reservations;
    reserveLayoutAnchors(reservations, dungeonLayout_);
    for (const WarpPoint& point : warpPoints_) {
        reservations.reserve(point.tilePosition, WarpReservationRadiusTiles);
    }
    const auto nodeRadius = [](PlacementVisibility visibility) {
        return visibility == PlacementVisibility::Exposed ? ExposedPlacementReservationRadiusTiles : 0;
    };
    for (const RewardNode& node : rewardNodes_) {
        if (!node.collected) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const MoneyNode& node : moneyNodes_) {
        if (!node.collected) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const ChestNode& node : chestNodes_) {
        if (!node.opened) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const CrateNode& node : crateNodes_) {
        if (!node.destroyed) {
            reservations.reserve(node.tile, SolidPlacementReservationRadiusTiles);
        }
    }

    for (const WarpPoint& point : warpPoints_) {
        const int count = countDistribution(rng);
        for (int i = 0; i < count; ++i) {
            const bool buried = i % 2 == 1;
            const float radiusTiles = buried ? buriedRadiusDistribution(rng) : floorRadiusDistribution(rng);
            const Vec2 offset = fromAngle(angleDistribution(rng)) * radiusTiles;
            MoonFragmentNode node{
                .tile = roundDungeonTile(Vec2{
                    static_cast<float>(point.tilePosition.x),
                    static_cast<float>(point.tilePosition.y),
                } + offset),
                .visibility = buried ? PlacementVisibility::BuriedVisible : PlacementVisibility::Exposed,
                .collected = false,
            };
            if (reservations.reserveNearest(
                    node.tile,
                    nodeRadius(node.visibility),
                    PlacementSearchRadiusTiles,
                    node.tile)) {
                moonFragmentNodes_.push_back(node);
            }
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
    std::uniform_real_distribution<float> pocketProgressJitter(-0.030f, 0.030f);
    std::uniform_real_distribution<float> pocketOffsetDist(WallPocketMinOffsetTiles, WallPocketMaxOffsetTiles);
    std::uniform_real_distribution<float> coinRoomAngleDist(0.0f, Pi * 2.0f);
    std::uniform_int_distribution<int> signDist(0, 1);
    std::uniform_int_distribution<int> moneyDist(2, 8);
    std::uniform_int_distribution<int> coinRoomMoneyCountDist(CoinRoomMoneyNodeMinCount, CoinRoomMoneyNodeMaxCount);
    const std::optional<std::string> fallbackObjectId = firstAvailableObjectId(objectCatalog_);

    PlacementReservations reservations;
    reserveLayoutAnchors(reservations, dungeonLayout_);
    for (const WarpPoint& point : warpPoints_) {
        reservations.reserve(point.tilePosition, WarpReservationRadiusTiles);
    }
    const auto nodeRadius = [](PlacementVisibility visibility) {
        return visibility == PlacementVisibility::Exposed ? ExposedPlacementReservationRadiusTiles : 0;
    };
    for (const MoonFragmentNode& node : moonFragmentNodes_) {
        if (!node.collected) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const ChestNode& node : chestNodes_) {
        if (!node.opened) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const CrateNode& node : crateNodes_) {
        if (!node.destroyed) {
            reservations.reserve(node.tile, SolidPlacementReservationRadiusTiles);
        }
    }

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

        const bool pathHint = i % 3 == 0;
        RewardNode node;
        node.visibility = pathHint || i % 3 == 1
            ? PlacementVisibility::BuriedVisible
            : PlacementVisibility::BuriedHidden;
        const float offsetTiles =
            (node.visibility == PlacementVisibility::BuriedVisible ? 7.0f : 9.0f) + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.rewardKind = i % 4 == 0 ? "treasure" : "item";
        node.objectId = (i % 2 == 0) ? fallbackObjectId : std::nullopt;
        node.revealed = false;
        if (reservations.reserveNearest(
                node.tile,
                nodeRadius(node.visibility),
                PlacementSearchRadiusTiles,
                node.tile)) {
            rewardNodes_.push_back(std::move(node));
        }
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

        const bool pathHint = i % 3 == 0;
        MoneyNode node;
        node.visibility = pathHint || i % 3 == 1
            ? PlacementVisibility::BuriedVisible
            : PlacementVisibility::BuriedHidden;
        const float offsetTiles =
            (node.visibility == PlacementVisibility::BuriedVisible ? 6.0f : 8.5f) + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.amount = std::max(1, moneyDist(rng) + dungeonLayout_.stageId * 2);
        if (reservations.reserveNearest(
                node.tile,
                nodeRadius(node.visibility),
                PlacementSearchRadiusTiles,
                node.tile)) {
            moneyNodes_.push_back(node);
        }
    }

    for (int i = 0; i < WallPocketRewardNodeCount; ++i) {
        const float progress = wallPocketProgressForIndex(i, WallPocketRewardNodeCount, pocketProgressJitter(rng));
        RewardNode node{
            .tile = wallPocketTileAtProgress(dungeonLayout_, progress, pocketOffsetDist(rng), signDist(rng) == 1),
            .visibility = PlacementVisibility::Exposed,
            .rewardKind = i % 3 == 0 ? "wall_pocket_treasure" : "wall_pocket_item",
            .objectId = (i % 2 == 0) ? fallbackObjectId : std::nullopt,
            .revealed = true,
            .spawned = false,
            .collected = false,
        };
        if (reservations.reserveNearest(
                node.tile,
                nodeRadius(node.visibility),
                PlacementSearchRadiusTiles,
                node.tile)) {
            rewardNodes_.push_back(std::move(node));
        }
    }

    for (int i = 0; i < WallPocketMoneyNodeCount; ++i) {
        const float progress = wallPocketProgressForIndex(i, WallPocketMoneyNodeCount, pocketProgressJitter(rng));
        MoneyNode node{
            .tile = wallPocketTileAtProgress(dungeonLayout_, progress, pocketOffsetDist(rng), signDist(rng) == 1),
            .amount = std::max(2, moneyDist(rng) + dungeonLayout_.stageId * 3),
            .visibility = PlacementVisibility::Exposed,
            .collected = false,
        };
        if (reservations.reserveNearest(
                node.tile,
                nodeRadius(node.visibility),
                PlacementSearchRadiusTiles,
                node.tile)) {
            moneyNodes_.push_back(node);
        }
    }

    for (const MicroFeature& feature : microFeaturesForLayout(dungeonLayout_)) {
        if (feature.kind == MicroFeatureKind::DoublePocketTreasure) {
            if (doublePocketLootKind(feature, dungeonLayout_.seed) == DoublePocketLootKind::Money) {
                MoneyNode node{
                    .tile = feature.center,
                    .amount = std::max(3, moneyDist(rng) + dungeonLayout_.stageId),
                    .visibility = PlacementVisibility::Exposed,
                    .collected = false,
                };
                if (reservations.tryReserve(node.tile, nodeRadius(node.visibility))) {
                    moneyNodes_.push_back(node);
                }
            } else if (doublePocketLootKind(feature, dungeonLayout_.seed) == DoublePocketLootKind::MoonFragment) {
                MoonFragmentNode node{
                    .tile = feature.center,
                    .visibility = PlacementVisibility::Exposed,
                    .collected = false,
                };
                if (reservations.tryReserve(node.tile, nodeRadius(node.visibility))) {
                    moonFragmentNodes_.push_back(node);
                }
            }
        } else if (feature.kind == MicroFeatureKind::BaitAndAmbush) {
            if (!baitUsesOreWall(feature, dungeonLayout_.seed)) {
                MoneyNode node{
                    .tile = feature.center,
                    .amount = std::max(2, dungeonLayout_.stageId + 2),
                    .visibility = PlacementVisibility::BuriedVisible,
                    .collected = false,
                };
                if (reservations.tryReserve(node.tile, nodeRadius(node.visibility))) {
                    moneyNodes_.push_back(node);
                }
            }
        } else if (feature.kind == MicroFeatureKind::CrateAlcove) {
            MoneyNode node{
                .tile = feature.second,
                .amount = std::max(2, moneyDist(rng) + dungeonLayout_.stageId),
                .visibility = PlacementVisibility::Exposed,
                .collected = false,
            };
            if (reservations.tryReserve(node.tile, 0)) {
                moneyNodes_.push_back(node);
            }
        }
    }

    const auto placeRewardNode = [&](RewardNode node) {
        if (reservations.reserveNearest(
                node.tile,
                nodeRadius(node.visibility),
                PlacementSearchRadiusTiles,
                node.tile)) {
            rewardNodes_.push_back(std::move(node));
        }
    };
    const auto placeMoneyNode = [&](MoneyNode node) {
        if (reservations.reserveNearest(
                node.tile,
                nodeRadius(node.visibility),
                PlacementSearchRadiusTiles,
                node.tile)) {
            moneyNodes_.push_back(node);
        }
    };

    if (std::string_view(currentStageId_) == FirstDungeonStageId) {
        const float pathLength = pathLengthTiles(dungeonLayout_.mainPathPoints);
        const float maxDepthProgress = pathLength > 1.0f
            ? MagnifyingGlassGuaranteedMaxDepthTiles / pathLength
            : MagnifyingGlassGuaranteedProgressFallback;
        const float progress = clamp(
            std::min(MagnifyingGlassGuaranteedProgressFallback, maxDepthProgress * 0.88f),
            WallPocketProgressStart,
            0.32f);
        const bool firstRewardRightSide = signDist(rng) == 1;
        const auto placeFirstDungeonGuaranteedReward = [&](std::string_view objectId, std::string_view rewardKind, bool rightSide) {
            if (objectCatalog_.registry.findById(objectId) == nullptr ||
                encyclopedia_.objectStage(objectId, false) != EncyclopediaStage::Undiscovered) {
                return;
            }

            RewardNode node{
                .tile = wallPocketTileAtProgress(dungeonLayout_, progress, WallPocketMinOffsetTiles, rightSide),
                .visibility = PlacementVisibility::Exposed,
                .rewardKind = std::string(rewardKind),
                .objectId = std::string(objectId),
                .revealed = true,
                .spawned = false,
                .collected = false,
            };
            placeRewardNode(std::move(node));
        };
        placeFirstDungeonGuaranteedReward(MagnifyingGlassObjectId, "first_dungeon_magnifying_glass", firstRewardRightSide);
        placeFirstDungeonGuaranteedReward(CaptureNetObjectId, "first_dungeon_capture_net", !firstRewardRightSide);
    }

    for (const SpecialRoomAnchor& room : dungeonLayout_.specialRoomAnchors) {
        const DungeonTile centerTile = roundDungeonTile(room.center);
        if (room.type == SpecialRoomType::CoinRoom) {
            const int moneyNodeCount = coinRoomMoneyCountDist(rng);
            const float angleOffset = coinRoomAngleDist(rng);
            for (int i = 0; i < moneyNodeCount; ++i) {
                placeMoneyNode(MoneyNode{
                    .tile = coinRoomMoneyNodeTile(room, i, moneyNodeCount, angleOffset),
                    .amount = std::max(4, moneyDist(rng) + dungeonLayout_.stageId * 4),
                    .visibility = PlacementVisibility::Exposed,
                    .collected = false,
                });
            }
        } else if (room.type == SpecialRoomType::TreasureRoom) {
            placeRewardNode(RewardNode{
                .tile = centerTile,
                .visibility = PlacementVisibility::Exposed,
                .rewardKind = "treasure",
                .objectId = fallbackObjectId,
                .revealed = true,
                .spawned = false,
                .collected = false,
            });
            placeRewardNode(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{room.radius, 0.0f}),
                .visibility = PlacementVisibility::BuriedVisible,
                .rewardKind = "treasure",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
            placeRewardNode(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{-room.radius, 0.0f}),
                .visibility = PlacementVisibility::BuriedHidden,
                .rewardKind = "treasure",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
        } else if (room.type == SpecialRoomType::EnemyRoom) {
            placeRewardNode(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{0.0f, room.radius}),
                .visibility = PlacementVisibility::BuriedHidden,
                .rewardKind = "enemy_room_reward",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
        } else if (room.type == SpecialRoomType::OreRoom) {
            placeRewardNode(RewardNode{
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
    }
}

void Game::revealRewardNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles)
{
    if (openedTiles.empty()) {
        return;
    }

    bool discoveredReward = false;
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
            discoveredReward = true;
        }
        for (MoneyNode& node : moneyNodes_) {
            if (node.collected || node.visibility == PlacementVisibility::Exposed ||
                node.tile.x != tile.x || node.tile.y != tile.y) {
                continue;
            }
            worldDrops_.spawnMoneyDrop(node.amount, tileWorldCenter(node.tile), runStats_.elapsedSeconds);
            node.collected = true;
            discoveredReward = true;
        }
    }
    if (discoveredReward) {
        playAudioSe(AudioSeDiscovery);
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
        }
    }
}

void Game::normalizeOpenBuriedPlacementNodes()
{
    const auto tileIsOpen = [this](DungeonTile tile) {
        return tileMap_.terrainDebugAtWorld(tileWorldCenter(tile)).type == TileType::Empty;
    };

    for (RewardNode& node : rewardNodes_) {
        if (node.collected || node.visibility == PlacementVisibility::Exposed || !tileIsOpen(node.tile)) {
            continue;
        }
        node.visibility = PlacementVisibility::Exposed;
        node.revealed = true;
    }

    for (MoneyNode& node : moneyNodes_) {
        if (node.collected || node.visibility == PlacementVisibility::Exposed || !tileIsOpen(node.tile)) {
            continue;
        }
        node.visibility = PlacementVisibility::Exposed;
    }

    for (MoonFragmentNode& node : moonFragmentNodes_) {
        if (node.collected || node.visibility == PlacementVisibility::Exposed || !tileIsOpen(node.tile)) {
            continue;
        }
        node.visibility = PlacementVisibility::Exposed;
    }

    for (ChestNode& node : chestNodes_) {
        if (node.opened || node.visibility == PlacementVisibility::Exposed || !tileIsOpen(node.tile)) {
            continue;
        }
        node.visibility = PlacementVisibility::Exposed;
        node.revealed = true;
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
    std::uniform_real_distribution<float> pocketProgressJitter(-0.028f, 0.028f);
    std::uniform_real_distribution<float> pocketOffsetDist(WallPocketMinOffsetTiles + 0.5f, WallPocketMaxOffsetTiles + 1.0f);
    std::uniform_int_distribution<int> signDist(0, 1);

    PlacementReservations reservations;
    reserveLayoutAnchors(reservations, dungeonLayout_);
    for (const WarpPoint& point : warpPoints_) {
        reservations.reserve(point.tilePosition, WarpReservationRadiusTiles);
    }
    const auto nodeRadius = [](PlacementVisibility visibility) {
        return visibility == PlacementVisibility::Exposed ? ExposedPlacementReservationRadiusTiles : 0;
    };
    for (const MoonFragmentNode& node : moonFragmentNodes_) {
        if (!node.collected) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const RewardNode& node : rewardNodes_) {
        if (!node.collected) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const MoneyNode& node : moneyNodes_) {
        if (!node.collected) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const CrateNode& node : crateNodes_) {
        if (!node.destroyed) {
            reservations.reserve(node.tile, SolidPlacementReservationRadiusTiles);
        }
    }

    for (const MicroFeature& feature : microFeaturesForLayout(dungeonLayout_)) {
        if (feature.kind != MicroFeatureKind::DoublePocketTreasure) {
            continue;
        }
        if (doublePocketLootKind(feature, dungeonLayout_.seed) != DoublePocketLootKind::Chest) {
            continue;
        }

        ChestNode node;
        node.visibility = PlacementVisibility::Exposed;
        node.tile = feature.center;
        node.chestKind = rollChestKind(rng, feature.progress);
        node.depthRank = lootDepthRankForProgress(currentStageId_, feature.progress);
        node.revealed = true;
        node.opened = false;
        node.lootSpawned = false;
        node.openingSeconds = 0.0f;
        assignChestMimic(node);
        if (reservations.tryReserve(node.tile, ExposedPlacementReservationRadiusTiles)) {
            chestNodes_.push_back(node);
        }
    }

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

        const bool pathHint = i % 4 == 0;
        ChestNode node;
        node.visibility = pathHint || i % 4 == 1
            ? PlacementVisibility::BuriedVisible
            : PlacementVisibility::BuriedHidden;
        const float offsetTiles =
            (node.visibility == PlacementVisibility::BuriedVisible ? 6.5f : 8.5f) + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.chestKind = rollChestKind(rng, progress);
        node.depthRank = lootDepthRankForProgress(currentStageId_, progress);
        node.revealed = node.visibility != PlacementVisibility::BuriedHidden;
        node.opened = false;
        node.lootSpawned = false;
        node.openingSeconds = 0.0f;
        if (reservations.reserveNearest(
                node.tile,
                nodeRadius(node.visibility),
                SolidPlacementSearchRadiusTiles,
                node.tile)) {
            assignChestMimic(node);
            chestNodes_.push_back(node);
        }
    }

    for (int i = 0; i < WallPocketChestNodeCount; ++i) {
        const float progress = wallPocketProgressForIndex(i, WallPocketChestNodeCount, pocketProgressJitter(rng));
        ChestNode node;
        node.visibility = PlacementVisibility::Exposed;
        node.tile = wallPocketTileAtProgress(dungeonLayout_, progress, pocketOffsetDist(rng), signDist(rng) == 1);
        node.chestKind = rollChestKind(rng, progress);
        node.depthRank = lootDepthRankForProgress(currentStageId_, progress);
        node.revealed = true;
        node.opened = false;
        node.lootSpawned = false;
        node.openingSeconds = 0.0f;
        if (reservations.reserveNearest(
                node.tile,
                nodeRadius(node.visibility),
                SolidPlacementSearchRadiusTiles,
                node.tile)) {
            assignChestMimic(node);
            chestNodes_.push_back(node);
        }
    }
}

void Game::assignChestMimic(ChestNode& node)
{
    node.mimicEnemyId.clear();
    node.mimicTriggered = false;

    const float chance = chestMimicChanceForKind(node.chestKind);
    if (chance <= 0.0f) {
        return;
    }

    const std::string_view enemyId = chestMimicEnemyIdForChestKind(node.chestKind);
    if (enemyCatalog_.enemiesById.find(std::string(enemyId)) == enemyCatalog_.enemiesById.end()) {
        return;
    }

    std::mt19937 rng(
        dungeonLayout_.seed ^
        0x9E3779B9u ^
        (static_cast<std::uint32_t>(node.tile.x) * 0x85EBCA6Bu) ^
        (static_cast<std::uint32_t>(node.tile.y) * 0xC2B2AE35u) ^
        (static_cast<std::uint32_t>(static_cast<int>(node.chestKind) + 1) * 0x27D4EB2Du));
    std::bernoulli_distribution mimicChance(static_cast<double>(std::clamp(chance, 0.0f, 1.0f)));
    if (mimicChance(rng)) {
        node.mimicEnemyId = std::string(enemyId);
    }
}

bool Game::spawnAppearingChestNode(
    DungeonTile tile,
    LootChestKind chestKind,
    int depthRank,
    Vec2 sourceWorldPosition,
    std::string_view logMergeKey)
{
    auto existing = std::find_if(chestNodes_.begin(), chestNodes_.end(), [tile](const ChestNode& node) {
        return sameDungeonTile(node.tile, tile);
    });
    if (existing != chestNodes_.end()) {
        existing->visibility = PlacementVisibility::Exposed;
        existing->revealed = true;
        existing->chestKind = chestKind;
        existing->depthRank = std::max(1, depthRank);
        existing->mimicEnemyId.clear();
        existing->mimicTriggered = false;
        return false;
    }

    ChestNode node;
    node.tile = tile;
    node.visibility = PlacementVisibility::Exposed;
    node.chestKind = chestKind;
    node.depthRank = std::max(1, depthRank);
    node.revealed = true;
    node.opened = false;
    node.lootSpawned = false;
    node.openingSeconds = 0.0f;

    std::mt19937 rng(
        dungeonLayout_.seed ^
        (static_cast<std::uint32_t>(tile.x) * 0x85EBCA6Bu) ^
        (static_cast<std::uint32_t>(tile.y) * 0xC2B2AE35u) ^
        (static_cast<std::uint32_t>(runStats_.elapsedSeconds * 1000.0f) * 0x27D4EB2Du));
    startChestSpawnJump(node, sourceWorldPosition, rng);
    chestNodes_.push_back(node);
    if (!logMergeKey.empty()) {
        pushDungeonLog("宝箱が現れた", std::string(logMergeKey));
    }
    return true;
}

void Game::startChestSpawnJump(ChestNode& node, Vec2 sourceWorldPosition, std::mt19937& rng)
{
    const Vec2 target = tileWorldCenter(node.tile);
    if (!std::isfinite(sourceWorldPosition.x) || !std::isfinite(sourceWorldPosition.y)) {
        sourceWorldPosition = target;
    }

    const WorldDropSpawnMotion motion = makeWorldLootJumpMotion(sourceWorldPosition, rng);
    node.spawnJumpActive = motion.jump && motion.jumpDurationSeconds > 0.0f;
    node.spawnJumpStartPosition = motion.startPosition;
    node.spawnJumpElapsedSeconds = 0.0f;
    node.spawnJumpDurationSeconds = std::max(0.05f, motion.jumpDurationSeconds);
    node.spawnJumpArcHeight = std::max(0.0f, motion.jumpArcHeight);
}

void Game::updateChestSpawnJump(ChestNode& node, float dt)
{
    if (!node.spawnJumpActive) {
        return;
    }

    node.spawnJumpElapsedSeconds = std::min(
        node.spawnJumpDurationSeconds,
        node.spawnJumpElapsedSeconds + std::max(0.0f, dt));
    const float t = node.spawnJumpDurationSeconds > 0.0f
        ? clamp(node.spawnJumpElapsedSeconds / node.spawnJumpDurationSeconds, 0.0f, 1.0f)
        : 1.0f;
    if (t >= 1.0f) {
        node.spawnJumpActive = false;
        node.spawnJumpStartPosition = {};
        node.spawnJumpElapsedSeconds = 0.0f;
        node.spawnJumpDurationSeconds = 0.0f;
        node.spawnJumpArcHeight = 0.0f;
    }
}

Vec2 Game::chestVisualCenter(const ChestNode& node) const
{
    const Vec2 target = tileWorldCenter(node.tile);
    if (!node.spawnJumpActive) {
        return target;
    }

    const float t = node.spawnJumpDurationSeconds > 0.0f
        ? clamp(node.spawnJumpElapsedSeconds / node.spawnJumpDurationSeconds, 0.0f, 1.0f)
        : 1.0f;
    return lerp(node.spawnJumpStartPosition, target, t);
}

float Game::chestVisualAltitude(const ChestNode& node) const
{
    if (!node.spawnJumpActive) {
        return 0.0f;
    }

    const float t = node.spawnJumpDurationSeconds > 0.0f
        ? clamp(node.spawnJumpElapsedSeconds / node.spawnJumpDurationSeconds, 0.0f, 1.0f)
        : 1.0f;
    return std::sin(t * Pi) * std::max(0.0f, node.spawnJumpArcHeight);
}

void Game::updateChestNodes(float dt, const Input& input)
{
    const bool interact = input.confirmPressed() || input.useItemPressed();
    const float interactRadiusSq = ChestInteractRadius * ChestInteractRadius;

    for (ChestNode& node : chestNodes_) {
        updateChestSpawnJump(node, dt);
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
        if (node.spawnJumpActive) {
            continue;
        }
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
            if (!tryTriggerChestMimic(node)) {
                openChestNode(node);
            }
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
            if (!tryTriggerChestMimic(node)) {
                openChestNode(node);
            }
        }
    }
}

bool Game::spawnWeightedObjectLoot(
    LootChestKind chestKind,
    int depthRank,
    Vec2 center,
    std::mt19937& rng,
    std::string_view sourceLabel,
    bool launchFromCenter,
    LootSourceKind sourceKind,
    std::string_view requiredTag)
{
    const auto sourceWeightMultiplier = [sourceKind](const ObjectDefinition& object) {
        constexpr std::string_view RecoveryCategory = "\xE5\x9B\x9E\xE5\xBE\xA9";
        constexpr std::string_view WeaponCategory = "\xE6\xAD\xA6\xE5\x99\xA8";
        constexpr std::string_view ShieldCategory = "\xE7\x9B\xBE";
        constexpr std::string_view TreasureCategory = "\xE5\xAE\x9D";

        switch (sourceKind) {
        case LootSourceKind::Chest:
        case LootSourceKind::CrateBonus:
            return 1.0;
        case LootSourceKind::DigItem:
        case LootSourceKind::CapturedReward:
            if (object.category == RecoveryCategory) {
                return 1.35;
            }
            if (object.category == TreasureCategory) {
                return 1.0;
            }
            return 0.75;
        case LootSourceKind::EnemyDrop:
            if (object.category == RecoveryCategory) {
                return 1.35;
            }
            if (object.category == WeaponCategory || object.category == ShieldCategory) {
                return 1.0;
            }
            if (object.category == TreasureCategory) {
                return 0.25;
            }
            return 0.75;
        }
        return 1.0;
    };
    const auto hasRequiredTag = [requiredTag](const ObjectDefinition& object) {
        if (requiredTag.empty()) {
            return true;
        }
        return std::any_of(object.tags.begin(), object.tags.end(), [requiredTag](const std::string& tag) {
            return tag == requiredTag;
        });
    };

    std::vector<const ObjectDefinition*> candidates;
    std::vector<double> weights;
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        if (!hasRequiredTag(object)) {
            continue;
        }
        const double baseWeight = lootWeightFor(object, currentStageId_, depthRank, chestKind);
        if (baseWeight >= 1.0) {
            const double weight = baseWeight * sourceWeightMultiplier(object);
            if (weight <= 0.0) {
                continue;
            }
            candidates.push_back(&object);
            weights.push_back(weight);
        }
    }

    if (candidates.empty()) {
        const std::string columnName = resolveLootWeightColumnName(currentStageId_, depthRank, chestKind);
        std::string message = "[warning] " + std::string(sourceLabel) + ": no Objects candidates stage=\"" + currentStageId_ +
            "\" depth=" + std::to_string(depthRank) +
            " chest=" + chestKindCode(chestKind) +
            " column=\"" + columnName + "\"";
        if (!requiredTag.empty()) {
            message += " tag=\"" + std::string(requiredTag) + "\"";
        }
        logError(message);
        return false;
    }

    const std::optional<std::size_t> selected = selectWeightedIndex(weights, rng);
    if (!selected || *selected >= candidates.size()) {
        logError("[warning] " + std::string(sourceLabel) + ": failed Objects weighted selection");
        return false;
    }
    const ObjectDefinition* object = candidates[*selected];
    const bool safeLanding = sourceKind == LootSourceKind::Chest || sourceKind == LootSourceKind::CrateBonus;
    const Vec2 target = safeLanding
        ? safeLootLandingPosition(center, rng)
        : scatterLootPosition(center, rng);
    return worldDrops_.spawnObjectDrop(
        objectCatalog_,
        object->id,
        target,
        runStats_.elapsedSeconds,
        launchFromCenter ? makeWorldLootJumpMotion(center, rng) : WorldDropSpawnMotion{});
}

Vec2 Game::safeLootLandingPosition(Vec2 center, std::mt19937& rng)
{
    const auto blockedByTerrain = [&](Vec2 candidate) {
        return tileMap_.isCircleBlocked(candidate, LootLandingCollisionRadius);
    };
    const auto blockedByWarp = [&](Vec2 candidate) {
        const float radiusSq = LootLandingWarpClearance * LootLandingWarpClearance;
        for (const WarpPoint& point : warpPoints_) {
            if (distanceSquared(candidate, point.position) <= radiusSq) {
                return true;
            }
        }
        return false;
    };
    const auto blockedByObject = [&](Vec2 candidate) {
        for (const ChestNode& node : chestNodes_) {
            if (node.mimicTriggered) {
                continue;
            }
            if (!node.revealed && !node.opened && node.visibility != PlacementVisibility::Exposed) {
                continue;
            }
            const CollisionRect rect = collisionRectFromCenter(tileWorldCenter(node.tile), ChestCollisionSize);
            if (circleIntersectsRect(candidate, LootLandingCollisionRadius, rect)) {
                return true;
            }
        }
        for (const CrateNode& node : crateNodes_) {
            if (node.destroyed) {
                continue;
            }
            const CollisionRect rect = collisionRectFromCenter(tileWorldCenter(node.tile), CrateCollisionSize);
            if (circleIntersectsRect(candidate, LootLandingCollisionRadius, rect)) {
                return true;
            }
        }
        return false;
    };
    const auto blockedByDrop = [&](Vec2 candidate) {
        const float spacingSq = LootLandingDropSpacing * LootLandingDropSpacing;
        for (const WorldDropItem& drop : worldDrops_.drops()) {
            const Vec2 occupiedPosition = drop.jumpActive ? drop.jumpTargetPosition : drop.position;
            if (distanceSquared(candidate, occupiedPosition) <= spacingSq) {
                return true;
            }
        }
        return false;
    };
    const auto safe = [&](Vec2 candidate) {
        return !blockedByTerrain(candidate) &&
            !blockedByWarp(candidate) &&
            !blockedByObject(candidate) &&
            !blockedByDrop(candidate);
    };

    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    const float baseAngle = angleDistribution(rng);
    for (int ring = 0; ring < LootLandingRingCount; ++ring) {
        const float radius = LootLandingFirstRadius + LootLandingRadiusStep * static_cast<float>(ring);
        const int samples = LootLandingSamplesPerRing + ring * 4;
        for (int i = 0; i < samples; ++i) {
            const float angle = baseAngle + (Pi * 2.0f) * (static_cast<float>(i) / static_cast<float>(samples));
            const Vec2 candidate = center + fromAngle(angle) * radius;
            if (safe(candidate)) {
                return candidate;
            }
        }
    }

    return scatterLootPosition(center, rng);
}

void Game::spawnInventoryDiscardRequests(std::vector<InventoryDiscardRequest> requests)
{
    if (requests.empty() || enemyTestActive_) {
        return;
    }
    const bool canSpawnDiscardDrop =
        mode_ == ScreenMode::Playing ||
        (mode_ == ScreenMode::Inventory && pauseReturnMode_ != ScreenMode::Base);
    if (!canSpawnDiscardDrop) {
        return;
    }

    std::mt19937& rng = lootRuntimeRng();
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> jitterDistribution(0.0f, DiscardThrowLandingJitter);
    std::uniform_real_distribution<float> durationDistribution(DiscardThrowDurationMin, DiscardThrowDurationMax);
    std::uniform_real_distribution<float> heightDistribution(DiscardThrowArcHeightMin, DiscardThrowArcHeightMax);

    Vec2 direction = lengthSquared(player_.facing) > 0.0001f ? normalize(player_.facing) : Vec2{1.0f, 0.0f};
    const Vec2 start = player_.position + direction * DiscardThrowStartOffset;

    for (InventoryDiscardRequest& request : requests) {
        if (request.item.id.empty() || request.quantity <= 0 || isImportantItem(request.item)) {
            continue;
        }

        const Vec2 jitter = fromAngle(angleDistribution(rng)) * jitterDistribution(rng);
        Vec2 target = player_.position + direction * DiscardThrowDistance + jitter;
        for (float distance = DiscardThrowDistance; distance >= 96.0f; distance -= 36.0f) {
            const Vec2 candidate = player_.position + direction * distance + jitter;
            if (!tileMap_.isCircleBlocked(candidate, LootLandingCollisionRadius)) {
                target = candidate;
                break;
            }
        }

        const float duration = durationDistribution(rng);
        const WorldDropSpawnMotion motion{
            .jump = true,
            .startPosition = start,
            .jumpDurationSeconds = duration,
            .jumpArcHeight = heightDistribution(rng),
            .pickupDelaySeconds = duration * 0.9f,
        };

        bool spawned = false;
        if (request.instance) {
            spawned = worldDrops_.spawnObjectInstanceDrop(
                objectCatalog_,
                std::move(*request.instance),
                target,
                runStats_.elapsedSeconds,
                motion,
                true);
        } else {
            for (int i = 0; i < request.quantity; ++i) {
                spawned = worldDrops_.spawnObjectDrop(
                    objectCatalog_,
                    request.item.id,
                    target,
                    runStats_.elapsedSeconds,
                    motion,
                    true) || spawned;
            }
        }

        if (spawned) {
            const std::string name = request.item.name.empty() ? request.item.id : request.item.name;
            const std::string icon = objectCatalog_.registry.findById(request.item.id) != nullptr
                ? inlineItemTag(request.item.id)
                : "";
            pushDungeonLog(icon + name + "を捨てた");
        }
    }
}

void Game::consumeInventoryUseEvents()
{
    std::vector<InventoryUseEvent> events = inventory_.consumeUseEvents();
    if (events.empty()) {
        return;
    }

    const bool showDungeonFeedback =
        mode_ == ScreenMode::Playing ||
        (mode_ == ScreenMode::Inventory && pauseReturnMode_ != ScreenMode::Base);
    if (!showDungeonFeedback) {
        return;
    }

    for (const InventoryUseEvent& event : events) {
        if (event.item.id.empty()) {
            continue;
        }

        if (event.healedAmount > 0) {
            magicFx_.playHealPulse(player_.position, 28.0f);
        }

        const std::string name = event.item.name.empty() ? event.item.id : event.item.name;
        const std::string icon = objectCatalog_.registry.findById(event.item.id) != nullptr
            ? inlineItemTag(event.item.id)
            : "";
        pushDungeonLog("ルネは" + icon + name + "を使った！");
    }
}

void Game::updateDigToolFailsafe(float dt)
{
    if (enemyTestActive_ || mode_ != ScreenMode::Playing) {
        return;
    }

    digToolFailsafeSpawnCooldown_ = std::max(0.0f, digToolFailsafeSpawnCooldown_ - dt);
}

bool Game::hasUsableDigToolOnRing() const
{
    for (const SpellRingItem* item : spellRing_.runtimeItems()) {
        if (item == nullptr || item->broken() || item->objectId.empty()) {
            continue;
        }
        const ItemData* object = objectCatalog_.registry.findById(item->objectId);
        if (object != nullptr && objectIsUsableDigTool(*object)) {
            return true;
        }
    }
    return false;
}

bool Game::hasUsableDigToolInInventory() const
{
    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        if (stack.count > 0 && objectIsUsableDigTool(stack.item)) {
            return true;
        }
    }
    for (const InventoryObjectInstance& instance : inventory_.objectInstances()) {
        if (inventoryInstanceIsUsableDigTool(instance)) {
            return true;
        }
    }
    return false;
}

bool Game::hasNearbyUsableDigToolDrop(float radius) const
{
    const float radiusSq = std::max(0.0f, radius) * std::max(0.0f, radius);
    for (const WorldDropItem& drop : worldDrops_.drops()) {
        if (drop.kind != WorldDropKind::Object || distanceSquared(player_.position, effectiveDropPosition(drop)) > radiusSq) {
            continue;
        }
        const ItemData* object = objectCatalog_.registry.findById(drop.id);
        if (object != nullptr && objectIsUsableDigTool(*object)) {
            return true;
        }
    }
    return false;
}

bool Game::trySpawnFailsafeShovelDropFromWall(Vec2 wallCenter)
{
    if (enemyTestActive_ ||
        mode_ != ScreenMode::Playing ||
        digToolFailsafeSpawnCooldown_ > 0.0f ||
        hasUsableDigToolOnRing() ||
        hasUsableDigToolInInventory() ||
        hasNearbyUsableDigToolDrop(DigToolFailsafeNearbyDropRadius)) {
        return false;
    }

    if (!spawnFailsafeShovelDropFromWall(wallCenter)) {
        return false;
    }

    digToolFailsafeSpawnCooldown_ = DigToolFailsafeSpawnCooldownSeconds;
    pushDungeonLog(inlineItemTag(DigToolFailsafeShovelObjectId) + " 壁からスコップが出た", "dig_tool_failsafe");
    return true;
}

bool Game::spawnFailsafeShovelDropFromWall(Vec2 wallCenter)
{
    const ItemData* shovel = objectCatalog_.registry.findById(DigToolFailsafeShovelObjectId);
    if (shovel == nullptr || !objectIsUsableDigTool(*shovel)) {
        return false;
    }

    std::mt19937& rng = lootRuntimeRng();
    const Vec2 target = safeLootLandingPosition(wallCenter, rng);
    return worldDrops_.spawnObjectDrop(
        objectCatalog_,
        DigToolFailsafeShovelObjectId,
        target,
        runStats_.elapsedSeconds,
        makeWorldLootJumpMotion(wallCenter, rng));
}

bool Game::tryTriggerChestMimic(ChestNode& node)
{
    if (node.opened || node.mimicTriggered || node.mimicEnemyId.empty()) {
        return false;
    }

    const Vec2 center = tileWorldCenter(node.tile);
    const RuntimeBalance dungeonBalance = runtimeBalanceForDungeon();
    const int lootDepthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, center);
    int spawnedRuntimeId = 0;
    if (!enemies_.spawnSpecificEnemyAtPosition(
            tileMap_,
            node.mimicEnemyId,
            center,
            player_.position,
            dungeonBalance,
            enemyCatalog_,
            true,
            ChestMimicSpawnWarmupSeconds,
            &spawnedRuntimeId,
            currentStageId_,
            lootDepthRank)) {
        logError("[warning] Chest mimic spawn failed: enemyId=\"" + node.mimicEnemyId + "\"");
        node.mimicEnemyId.clear();
        return false;
    }

    node.opened = true;
    node.revealed = true;
    node.lootSpawned = true;
    node.openingSeconds = ChestLootReleaseSeconds;
    node.mimicTriggered = true;
    node.spawnJumpActive = false;

    playAudioSe(AudioSeChestOpen);
    playAudioSe(AudioSeEnemySpawn);
    effects_.spawnEnemyTransform(center);
    pushDungeonLog("宝箱はミミックだった", "chest_mimic:" + std::to_string(spawnedRuntimeId));
    return true;
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

    playAudioSe(AudioSeChestOpen);
    if (introTutorialActive()) {
        introTutorialChestOpened_ = true;
    }
}

void Game::spawnChestLoot(ChestNode& node)
{
    if (node.lootSpawned || node.mimicTriggered) {
        return;
    }

    node.lootSpawned = true;
    const Vec2 center = tileWorldCenter(node.tile);
    std::mt19937 rng(
        dungeonLayout_.seed ^
        (static_cast<std::uint32_t>(node.tile.x) * 0x85EBCA6Bu) ^
        (static_cast<std::uint32_t>(node.tile.y) * 0xC2B2AE35u) ^
        (static_cast<std::uint32_t>(runStats_.elapsedSeconds * 1000.0f) * 0x27D4EB2Du));

    if (introTutorialActive() && sameDungeonTile(node.tile, introTutorialChestTile_)) {
        const Vec2 target = safeLootLandingPosition(center, rng);
        worldDrops_.spawnObjectDrop(
            objectCatalog_,
            IntroTutorialCoinBagObjectId,
            target,
            runStats_.elapsedSeconds,
            makeWorldLootJumpMotion(center, rng));
        return;
    }

    if (introTutorialActive() && sameDungeonTile(node.tile, introTutorialSecondChestTile_)) {
        const Vec2 target = safeLootLandingPosition(center, rng);
        worldDrops_.spawnObjectDrop(
            objectCatalog_,
            IntroTutorialIronSwordObjectId,
            target,
            runStats_.elapsedSeconds,
            makeWorldLootJumpMotion(center, rng));
        return;
    }

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
        const Vec2 target = safeLootLandingPosition(center, rng);
        worldDrops_.spawnMoneyDrop(amount, target, runStats_.elapsedSeconds, makeWorldLootJumpMotion(center, rng));
    }

    std::bernoulli_distribution materialChance(balance_.lootMaterialChance);
    if (materialChance(rng)) {
        std::uniform_int_distribution<int> materialDistribution(1, 3);
        const int amount = scaledLootAmount(materialDistribution(rng), totalMultiplier);
        const Vec2 target = safeLootLandingPosition(center, rng);
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
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> unitDistribution(0.0f, 1.0f);
    std::uniform_real_distribution<float> progressJitter(-0.030f, 0.030f);
    std::uniform_real_distribution<float> sideJitter(-1.6f, 1.6f);
    std::uniform_int_distribution<int> signDist(0, 1);
    const std::vector<FloorCavernAnchor> floorAnchors = collectFloorCavernAnchors(dungeonLayout_);

    PlacementReservations reservations;
    reserveLayoutAnchors(reservations, dungeonLayout_);
    for (const WarpPoint& point : warpPoints_) {
        reservations.reserve(point.tilePosition, WarpReservationRadiusTiles);
    }
    const auto nodeRadius = [](PlacementVisibility visibility) {
        return visibility == PlacementVisibility::Exposed ? ExposedPlacementReservationRadiusTiles : 0;
    };
    for (const MoonFragmentNode& node : moonFragmentNodes_) {
        if (!node.collected) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const RewardNode& node : rewardNodes_) {
        if (!node.collected) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const MoneyNode& node : moneyNodes_) {
        if (!node.collected) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }
    for (const ChestNode& node : chestNodes_) {
        if (!node.opened) {
            reservations.reserve(node.tile, nodeRadius(node.visibility));
        }
    }

    for (const MicroFeature& feature : microFeaturesForLayout(dungeonLayout_)) {
        if (feature.kind == MicroFeatureKind::CrateAlcove) {
            CrateNode node;
            node.tile = feature.center;
            node.depthRank = lootDepthRankForProgress(currentStageId_, feature.progress);
            node.destroyed = false;
            crateNodes_.push_back(node);
        } else if (feature.kind == MicroFeatureKind::DoublePocketTreasure &&
            doublePocketUsesCrate(feature, dungeonLayout_.seed)) {
            CrateNode node;
            node.tile = feature.second;
            node.depthRank = lootDepthRankForProgress(currentStageId_, feature.progress);
            node.destroyed = false;
            crateNodes_.push_back(node);
        }
    }

    for (int i = 0; i < CrateNodeCountPerRun; ++i) {
        if (!floorAnchors.empty()) {
            const FloorCavernAnchor& floor = floorAnchors[static_cast<std::size_t>(i) % floorAnchors.size()];
            const float offsetRadius = 1.1f + unitDistribution(rng) * std::max(0.1f, floor.radius - 1.2f);
            const Vec2 offset = fromAngle(angleDistribution(rng)) * offsetRadius;
            const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, floor.center);

            CrateNode node;
            node.tile = roundDungeonTile(floor.center + offset);
            node.depthRank = lootDepthRankForProgress(currentStageId_, metrics.pathProgress);
            node.destroyed = false;
            if (reservations.reserveNearest(
                    node.tile,
                    SolidPlacementReservationRadiusTiles,
                    SolidPlacementSearchRadiusTiles,
                    node.tile)) {
                crateNodes_.push_back(node);
            }
            continue;
        }

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
        if (reservations.reserveNearest(
                node.tile,
                SolidPlacementReservationRadiusTiles,
                SolidPlacementSearchRadiusTiles,
                node.tile)) {
            crateNodes_.push_back(node);
        }
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
    playAudioSe(AudioSeCrateBreak);

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
    const Vec2 materialTarget = safeLootLandingPosition(center, rng);
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
        const Vec2 moneyTarget = safeLootLandingPosition(center, rng);
        worldDrops_.spawnMoneyDrop(
            amount,
            moneyTarget,
            runStats_.elapsedSeconds,
            makeWorldLootJumpMotion(center, rng));
    }

    std::bernoulli_distribution bonusChance(balance_.crateBonusChance);
    if (bonusChance(rng)) {
        spawnWeightedObjectLoot(LootChestKind::Common, node.depthRank, center, rng, "CrateBonusLoot", true, LootSourceKind::CrateBonus);
    }
}

std::vector<CollisionRect> Game::solidObjectCollisionRects() const
{
    std::vector<CollisionRect> rects;
    rects.reserve(chestNodes_.size() + crateNodes_.size());

    for (const ChestNode& node : chestNodes_) {
        if (node.opened) {
            continue;
        }
        if (node.visibility != PlacementVisibility::Exposed || !node.revealed) {
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

        const bool pathAmbush = i % 3 != 0;
        EnemyNode node;
        node.placementType = EnemyPlacementType::BuriedHidden;
        const float offsetTiles = pathAmbush
            ? (1.0f + sideJitter(rng))
            : (8.0f + sideJitter(rng));
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.dangerTier = std::max(1, dungeonLayout_.stageId + (i % 4 == 0 ? 1 : 0));
        node.enemySpawnGroup = "default";
        enemyNodes_.push_back(std::move(node));
    }

    for (const MicroFeature& feature : microFeaturesForLayout(dungeonLayout_)) {
        if (feature.kind == MicroFeatureKind::DoublePocketTreasure && !doublePocketUsesCrate(feature, dungeonLayout_.seed)) {
            enemyNodes_.push_back(EnemyNode{
                .tile = feature.second,
                .placementType = EnemyPlacementType::Exposed,
                .dangerTier = std::max(1, dungeonLayout_.stageId),
                .enemySpawnGroup = "micro_pocket_guard",
                .spawned = false,
            });
        } else if (feature.kind == MicroFeatureKind::BaitAndAmbush) {
            enemyNodes_.push_back(EnemyNode{
                .tile = baitEnemyTile(feature),
                .placementType = EnemyPlacementType::Exposed,
                .dangerTier = std::max(1, dungeonLayout_.stageId),
                .enemySpawnGroup = "micro_bait_guard",
                .spawned = false,
            });
        }
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

void Game::applyDungeonEventCavity(const DungeonEventInstance& event)
{
    if (event.id.empty()) {
        return;
    }
    const std::vector<DungeonTile> cavityTiles = buildDungeonEventCavityTiles(event, dungeonLayout_.seed);
    for (DungeonTile tile : cavityTiles) {
        tileMap_.setTileOverride(tile, TileType::Empty);
    }
}

void Game::clearKnownWarpPointTerrain()
{
    const auto clearPocket = [this](DungeonTile tile) {
        for (int y = -WarpReservationRadiusTiles; y <= WarpReservationRadiusTiles; ++y) {
            for (int x = -WarpReservationRadiusTiles; x <= WarpReservationRadiusTiles; ++x) {
                tileMap_.setTileOverride(
                    DungeonTile{tile.x + x, tile.y + y},
                    TileType::Empty);
            }
        }
        tileMap_.setTileOverride(tile, TileType::Empty);
    };

    for (const WarpPoint& point : warpPoints_) {
        const bool known =
            point.discovered ||
            point.unlocked ||
            point.snapshotCaptured ||
            point.index < unlockedWarpPointCount_;
        if (known) {
            clearPocket(point.tilePosition);
        }
    }
    if (hasLatestWarpPointPosition_) {
        clearPocket(DungeonTile{
            tileMap_.worldToTile(latestWarpPointPosition_.x),
            tileMap_.worldToTile(latestWarpPointPosition_.y),
        });
    }
}

void Game::applyPlacementTerrainOverrides()
{
    const auto applyExposedPocket = [this](DungeonTile tile, int radius) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                tileMap_.setTileOverride(
                    DungeonTile{tile.x + x, tile.y + y},
                    TileType::Empty);
            }
        }
    };
    const auto applyExposedCenter = [this](DungeonTile tile) {
        tileMap_.setTileOverride(tile, TileType::Empty);
    };
    const auto applyBuriedWall = [this](DungeonTile tile, PlacementVisibility visibility, bool treasureWall) {
        tileMap_.setTileOverride(
            tile,
            buriedPlacementTileType(
                tile,
                dungeonLayout_.seed,
                visibility == PlacementVisibility::BuriedHidden,
                treasureWall));
    };

    for (const MicroFeature& feature : microFeaturesForLayout(dungeonLayout_)) {
        switch (feature.kind) {
        case MicroFeatureKind::OreNeedle:
            tileMap_.setTileOverride(feature.entry, TileType::Dirt);
            tileMap_.setTileOverride(feature.center, TileType::Ore);
            tileMap_.setTileOverride(feature.second, TileType::Dirt);
            tileMap_.setTileOverride(addTile(feature.center, feature.tangentStep, -1), TileType::Rock);
            tileMap_.setTileOverride(feature.back, TileType::Rock);
            break;
        case MicroFeatureKind::DoublePocketTreasure:
            tileMap_.setTileOverride(feature.entry, TileType::Dirt);
            tileMap_.setTileOverride(feature.center, TileType::Empty);
            tileMap_.setTileOverride(feature.second, TileType::Empty);
            tileMap_.setTileOverride(feature.back, TileType::Rock);
            tileMap_.setTileOverride(addTile(feature.second, feature.sideStep), TileType::Rock);
            break;
        case MicroFeatureKind::BaitAndAmbush:
            tileMap_.setTileOverride(feature.entry, TileType::Dirt);
            tileMap_.setTileOverride(feature.center, baitUsesOreWall(feature, dungeonLayout_.seed) ? TileType::Ore : TileType::Rock);
            tileMap_.setTileOverride(feature.back, TileType::Empty);
            tileMap_.setTileOverride(baitEnemyTile(feature), TileType::Empty);
            tileMap_.setTileOverride(addTile(baitEnemyTile(feature), feature.tangentStep), TileType::Rock);
            break;
        case MicroFeatureKind::CrateAlcove:
            tileMap_.setTileOverride(feature.entry, TileType::Empty);
            tileMap_.setTileOverride(feature.center, TileType::Empty);
            tileMap_.setTileOverride(feature.second, TileType::Empty);
            tileMap_.setTileOverride(feature.back, TileType::Dirt);
            break;
        case MicroFeatureKind::OreVein:
            tileMap_.setTileOverride(feature.entry, TileType::Dirt);
            for (DungeonTile tile : oreTilesForMicroFeature(feature)) {
                tileMap_.setTileOverride(tile, TileType::Ore);
            }
            tileMap_.setTileOverride(addTile(feature.center, feature.sideStep, 2), TileType::Rock);
            tileMap_.setTileOverride(addTile(feature.second, feature.tangentStep), TileType::Rock);
            break;
        }
    }

    for (const RewardNode& node : rewardNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            applyExposedPocket(node.tile, 1);
        }
    }
    for (const MoneyNode& node : moneyNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            applyExposedPocket(node.tile, 1);
        }
    }
    for (const MoonFragmentNode& node : moonFragmentNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            applyExposedPocket(node.tile, 1);
        }
    }
    for (const ChestNode& node : chestNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            applyExposedPocket(node.tile, 1);
        }
    }
    for (const EnemyNode& node : enemyNodes_) {
        if (node.placementType == EnemyPlacementType::Exposed) {
            applyExposedPocket(node.tile, 1);
        }
    }
    for (const CrateNode& node : crateNodes_) {
        applyExposedPocket(node.tile, 1);
    }
    clearKnownWarpPointTerrain();

    for (const RewardNode& node : rewardNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            continue;
        }
        const bool treasureWall = node.rewardKind.find("treasure") != std::string::npos;
        applyBuriedWall(node.tile, node.visibility, treasureWall);
    }
    for (const MoneyNode& node : moneyNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            continue;
        }
        applyBuriedWall(node.tile, node.visibility, false);
    }
    for (const MoonFragmentNode& node : moonFragmentNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            continue;
        }
        applyBuriedWall(node.tile, node.visibility, false);
    }
    for (const ChestNode& node : chestNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            continue;
        }
        applyBuriedWall(node.tile, node.visibility, true);
    }
    for (const EnemyNode& node : enemyNodes_) {
        if (node.placementType == EnemyPlacementType::Exposed) {
            continue;
        }
        tileMap_.setTileOverride(
            node.tile,
            buriedPlacementTileType(node.tile, dungeonLayout_.seed, true, false));
    }

    for (const RewardNode& node : rewardNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            applyExposedCenter(node.tile);
        }
    }
    for (const MoneyNode& node : moneyNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            applyExposedCenter(node.tile);
        }
    }
    for (const MoonFragmentNode& node : moonFragmentNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            applyExposedCenter(node.tile);
        }
    }
    for (const ChestNode& node : chestNodes_) {
        if (node.visibility == PlacementVisibility::Exposed) {
            applyExposedCenter(node.tile);
        }
    }
    for (const EnemyNode& node : enemyNodes_) {
        if (node.placementType == EnemyPlacementType::Exposed) {
            applyExposedCenter(node.tile);
        }
    }
    for (const CrateNode& node : crateNodes_) {
        applyExposedCenter(node.tile);
    }
    clearKnownWarpPointTerrain();

    for (const DungeonEventInstance& event : dungeonEvents_.all()) {
        applyDungeonEventCavity(event);
    }
}

void Game::updateExposedEnemyNodes()
{
    const RuntimeBalance dungeonBalance = runtimeBalanceForDungeon();
    const Vec2 playerLightCenter = witchSelfLightCenter(player_.position);
    const float spawnRadius =
        std::max(dungeonBalance.playerLightRadius, dungeonBalance.lightRadius) + ExposedEnemyNodeSpawnPadding;
    const float spawnRadiusSq = spawnRadius * spawnRadius;
    const float introTutorialSpawnRadius =
        IntroTutorialEnemySpawnRadiusTiles * static_cast<float>(balance::TileSize);
    const float introTutorialSpawnRadiusSq = introTutorialSpawnRadius * introTutorialSpawnRadius;
    for (EnemyNode& node : enemyNodes_) {
        if (node.spawned || node.placementType != EnemyPlacementType::Exposed) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        const bool introTutorialSlimeNode = introTutorialActive() &&
            std::string_view(node.enemySpawnGroup.data(), node.enemySpawnGroup.size()) == IntroTutorialSlimeGroup;
        const bool introTutorialMushroomNode = introTutorialActive() &&
            std::string_view(node.enemySpawnGroup.data(), node.enemySpawnGroup.size()) == IntroTutorialMushroomGroup;
        if (introTutorialMushroomNode && introTutorialPhase_ != IntroTutorialPhase::FreeToExit) {
            continue;
        }
        const bool introTutorialFixedNode = introTutorialSlimeNode || introTutorialMushroomNode;
        const float effectiveSpawnRadiusSq = introTutorialFixedNode
            ? introTutorialSpawnRadiusSq
            : spawnRadiusSq;
        if (distanceSquared(center, playerLightCenter) > effectiveSpawnRadiusSq) {
            continue;
        }
        bool spawned = false;
        int spawnedRuntimeId = 0;
        const int lootDepthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, center);
        if (introTutorialSlimeNode) {
            const std::string slimeId = introTutorialSlimeEnemyId(enemyCatalog_);
            if (!slimeId.empty()) {
                spawned = enemies_.spawnSpecificEnemy(
                    tileMap_,
                    slimeId,
                    center,
                    player_.position,
                    balance_,
                    enemyCatalog_,
                    true,
                    false,
                    0.0f,
                    &spawnedRuntimeId,
                    currentStageId_,
                    lootDepthRank);
            }
            if (!spawned) {
                spawned = enemies_.spawnFixedNodeEnemy(tileMap_, center, player_.position, balance_, enemyCatalog_, false, &spawnedRuntimeId, currentStageId_, lootDepthRank);
            }
        } else if (introTutorialMushroomNode) {
            const std::string mushroomId = introTutorialMushroomEnemyId(enemyCatalog_);
            if (!mushroomId.empty()) {
                spawned = enemies_.spawnSpecificEnemy(
                    tileMap_,
                    mushroomId,
                    center,
                    player_.position,
                    balance_,
                    enemyCatalog_,
                    true,
                    false,
                    0.0f,
                    &spawnedRuntimeId,
                    currentStageId_,
                    lootDepthRank);
            }
            if (!spawned) {
                spawned = enemies_.spawnFixedNodeEnemy(tileMap_, center, player_.position, balance_, enemyCatalog_, false, &spawnedRuntimeId, currentStageId_, lootDepthRank);
            }
        } else {
            spawned = enemies_.spawnFixedNodeEnemy(tileMap_, center, player_.position, balance_, enemyCatalog_, false, nullptr, currentStageId_, lootDepthRank);
        }
        if (spawned) {
            if (introTutorialFixedNode) {
                const float resolveRadius =
                    IntroTutorialEnemyResolveRadiusTiles * static_cast<float>(balance::TileSize);
                const bool manualSet = spawnedRuntimeId > 0 &&
                    enemies_.setManualDetectionOnlyForRuntimeEnemy(spawnedRuntimeId, true);
                if (!manualSet) {
                    enemies_.setManualDetectionOnlyNear(center, resolveRadius, true);
                }
            }
            if (introTutorialSlimeNode) {
                introTutorialFirstEnemyRuntimeId_ = spawnedRuntimeId;
                if (spawnedRuntimeId > 0) {
                    enemies_.setRuntimeEnemyMovementLeash(
                        spawnedRuntimeId,
                        center,
                        IntroTutorialSlimeLeashRadiusTiles * static_cast<float>(balance::TileSize));
                }
            } else if (introTutorialMushroomNode) {
                introTutorialSecondEnemyRuntimeId_ = spawnedRuntimeId;
            }
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

        for (std::string_view orbitEffectKey : {"orbit_gravity", "orbit_power", "orbit_antigravity", "orbit_speed", "orbit_anchor", "orbit_shift", "damage_speed", "item_orbit_offset"}) {
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
                playAudioSe(AudioSeDiscovery);
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
                playAudioSe(AudioSeDiscovery);
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

void Game::updateOrbitAreaEffects(float dt, std::vector<EffectDiscoveryEvent>& discoveryEvents)
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
        item.coldAirFxTimer = std::max(0.0f, item.coldAirFxTimer - dt);
        item.vacuumPullFxTimer = std::max(0.0f, item.vacuumPullFxTimer - dt);
        item.hotAirFxTimer = std::max(0.0f, item.hotAirFxTimer - dt);
        item.windPushFxTimer = std::max(0.0f, item.windPushFxTimer - dt);
        if (item.broken()) {
            continue;
        }

        const ObjectDefinition* object = item.objectId.empty()
            ? nullptr
            : objectCatalog_.registry.findById(item.objectId);

        if (item.vacuumPullRadius > 0.0f && item.vacuumPullStrength > 0.0f) {
            const int pulledDrops = worldDrops_.pullLightDrops(
                objectCatalog_,
                item.worldPosition,
                dt,
                item.vacuumPullRadius,
                item.vacuumPullStrength,
                &inventory_);
            const int pulledEnemies = enemies_.pullLightEnemies(
                item.worldPosition,
                tileMap_,
                dt,
                item.vacuumPullRadius,
                item.vacuumPullStrength);
            if (pulledDrops + pulledEnemies > 0) {
                if (item.vacuumPullFxTimer <= 0.0f) {
                    effects_.spawnAreaPulse(item.worldPosition, item.vacuumPullRadius, {232, 226, 154, 118});
                    item.vacuumPullFxTimer = 0.30f;
                }
                if (object != nullptr) {
                    appendObjectEffectDiscovery(&discoveryEvents, encyclopedia_, *object, "vacuum_pull_light", item.worldPosition);
                }
            }
        }

        if (item.windPushRadius > 0.0f && item.windPushStrength > 0.0f) {
            const int pushedDrops = worldDrops_.pushLightDrops(
                objectCatalog_,
                item.worldPosition,
                dt,
                item.windPushRadius,
                item.windPushStrength);
            const int pushedEnemies = enemies_.pushLightEnemies(
                item.worldPosition,
                tileMap_,
                dt,
                item.windPushRadius,
                item.windPushStrength);
            if (pushedDrops + pushedEnemies > 0) {
                if (item.windPushFxTimer <= 0.0f) {
                    effects_.spawnAreaPulse(item.worldPosition, item.windPushRadius, {142, 238, 198, 124});
                    item.windPushFxTimer = 0.28f;
                }
                if (object != nullptr) {
                    appendObjectEffectDiscovery(&discoveryEvents, encyclopedia_, *object, "wind_push_light", item.worldPosition);
                }
            }
        }

        if (item.coldAirRadius > 0.0f && item.coldAirStrength > 0.0f) {
            int frozenCount = 0;
            const std::string source = item.objectId.empty()
                ? "orbit:cold_air_aura"
                : "orbit:" + item.objectId;
            const int touched = enemies_.applyColdAirAura(
                item.worldPosition,
                item.coldAirRadius,
                item.coldAirStrength,
                dt,
                source,
                &frozenCount);
            if (touched > 0) {
                if (item.coldAirFxTimer <= 0.0f) {
                    effects_.spawnAreaPulse(item.worldPosition, item.coldAirRadius, {126, 218, 255, 135});
                    item.coldAirFxTimer = 0.34f;
                }
                if (object != nullptr) {
                    appendObjectEffectDiscovery(&discoveryEvents, encyclopedia_, *object, "cold_air_aura", item.worldPosition);
                    if (frozenCount > 0) {
                        appendObjectEffectDiscovery(&discoveryEvents, encyclopedia_, *object, "status_frozen", item.worldPosition);
                    }
                }
            }
        }

        if (item.hotAirRadius > 0.0f && item.hotAirStrength > 0.0f) {
            int hotCount = 0;
            int driedWetCount = 0;
            const std::string source = item.objectId.empty()
                ? "orbit:hot_air"
                : "orbit:" + item.objectId;
            const int touched = enemies_.applyHotAir(
                item.worldPosition,
                item.hotAirRadius,
                item.hotAirStrength,
                dt,
                source,
                spellRing_,
                item.dryWetBonusDamage,
                &hotCount,
                &driedWetCount);
            if (touched > 0) {
                if (item.hotAirFxTimer <= 0.0f) {
                    effects_.spawnAreaPulse(item.worldPosition, item.hotAirRadius, {255, 134, 66, 132});
                    item.hotAirFxTimer = 0.32f;
                }
                if (object != nullptr) {
                    appendObjectEffectDiscovery(&discoveryEvents, encyclopedia_, *object, "hot_air", item.worldPosition);
                    if (hotCount > 0) {
                        appendObjectEffectDiscovery(&discoveryEvents, encyclopedia_, *object, "status_hot", item.worldPosition);
                    }
                    if (driedWetCount > 0 && item.dryWetBonusDamage > 0) {
                        appendObjectEffectDiscovery(&discoveryEvents, encyclopedia_, *object, "dry_wet_bonus_damage", item.worldPosition);
                    }
                }
            }
        }
    }
}

void Game::updateOrbitGroundEffects(float dt, std::vector<EffectDiscoveryEvent>& discoveryEvents)
{
    if (dt <= 0.0f || objectCatalog_.objectsById.empty()) {
        return;
    }

    std::vector<SpellRingItem*> runtimeItems = spellRing_.runtimeItemsMutable();
    for (SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->broken() || itemPtr->objectId.empty()) {
            continue;
        }

        SpellRingItem& item = *itemPtr;
        const auto objectIt = objectCatalog_.objectsById.find(item.objectId);
        if (objectIt == objectCatalog_.objectsById.end()) {
            continue;
        }

        bool hasGroundEffect = false;
        for (const EffectSpec& spec : objectIt->second.orbitEffects) {
            if (spec.target == "ground") {
                hasGroundEffect = true;
                break;
            }
        }
        if (!hasGroundEffect) {
            continue;
        }

        EffectContext context;
        context.sourceObject = &objectIt->second;
        context.owner = &player_;
        context.orbit = &spellRing_;
        context.orbitItem = &item;
        context.tileMap = &tileMap_;
        context.effects = &effects_;
        context.enemies = &enemies_;
        context.groundLines = &groundLines_;
        context.magic = &magic_;
        context.encyclopedia = &encyclopedia_;
        context.discoveryEvents = &discoveryEvents;
        context.position = item.worldPosition;
        context.triggerType = EffectTriggerType::Orbit;
        context.logUnimplementedEffects = false;
        effectDispatcher_.dispatchTargetEffects(objectIt->second.orbitEffects, "ground", context);
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
            const Vec2 center = tileWorldCenter(node.tile);
            const int lootDepthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, center);
            if (enemies_.spawnNodeEnemy(tileMap_, center, player_.position, balance_, enemyCatalog_, true, true, currentStageId_, lootDepthRank)) {
                node.spawned = true;
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

void Game::resetBossEncounter()
{
    bossEncounter_ = BossEncounterState{};
}

bool Game::bossEncounterBlocksProgress() const
{
    return bossEncounter_.phase == BossEncounterPhase::WaitingBeforeDialogue ||
        bossEncounter_.phase == BossEncounterPhase::DefeatPresentation ||
        bossEncounter_.phase == BossEncounterPhase::WaitingAfterDialogue;
}

float Game::bossDefeatPresentationProgress() const
{
    if (bossEncounter_.phase != BossEncounterPhase::DefeatPresentation) {
        return 0.0f;
    }
    return clamp(bossEncounter_.timer / std::max(0.001f, BossDefeatPresentationSeconds), 0.0f, 1.0f);
}

bool Game::beginBossFightForCurrentEncounter()
{
    if (!hasBossSpawnPoint_ || bossSpawned_ || !warpPointsEnabled_ || hasCapturedBossForCurrentStage()) {
        resetBossEncounter();
        return false;
    }

    bossSpawned_ = enemies_.spawnBossNear(
        tileMap_,
        bossSpawnPoint_,
        player_.position,
        balance_,
        enemyCatalog_,
        currentStageDefinition().bossEnemyId);
    if (!bossSpawned_) {
        resetBossEncounter();
        return false;
    }

    bossEncounter_.phase = BossEncounterPhase::Fighting;
    bossEncounter_.stageId = currentStageId_;
    bossEncounter_.spawnPoint = bossSpawnPoint_;
    playAudioSe("se.boss.spawn");
    playAudioBgm("bgm.boss", 0.45f);
    return true;
}

void Game::beginBossDefeatSequence(Vec2 position)
{
    pendingStoryTriggers_.clear();
    bossEncounter_.phase = BossEncounterPhase::DefeatPresentation;
    bossEncounter_.stageId = currentStageId_;
    bossEncounter_.defeatPosition = position;
    bossEncounter_.timer = 0.0f;
    bossEncounter_.finalBoss = currentStageId_ == FinalStoryStageId && !hasStoryFlag(EndingSeenFlag);
    playAudioSe("se.boss.defeat");
    playAudioBgm("bgm.dungeon", 0.70f);
    effects_.spawnAreaPulse(position, 92.0f, {255, 214, 110, 210});
}

void Game::finishBossEncounterAfterDialogue()
{
    const bool finalBoss = bossEncounter_.finalBoss;
    resetBossEncounter();
    if (finalBoss) {
        beginFinalBossEndingSequence();
    } else if (currentStageIsRoguelike()) {
        enterAstralResult(AstralRunResult::DragonDefeated);
    } else {
        enterStageClear();
    }
}

bool Game::updateBossEncounterFlow(float dt)
{
    switch (bossEncounter_.phase) {
    case BossEncounterPhase::None:
    case BossEncounterPhase::Fighting:
        return false;
    case BossEncounterPhase::WaitingBeforeDialogue:
        if (dialogue_.active() || !pendingStoryTriggers_.empty()) {
            return true;
        }
        beginBossFightForCurrentEncounter();
        return true;
    case BossEncounterPhase::DefeatPresentation:
        bossEncounter_.timer += std::max(0.0f, dt);
        effects_.update(std::max(0.0f, dt));
        magicFx_.update(std::max(0.0f, dt));
        if (bossEncounter_.timer < BossDefeatPresentationSeconds) {
            return true;
        }
        bossEncounter_.phase = BossEncounterPhase::WaitingAfterDialogue;
        bossEncounter_.timer = 0.0f;
        if (!queueStoryEventForCurrentStage("boss_after")) {
            finishBossEncounterAfterDialogue();
        }
        return true;
    case BossEncounterPhase::WaitingAfterDialogue:
        if (dialogue_.active() || !pendingStoryTriggers_.empty()) {
            return true;
        }
        finishBossEncounterAfterDialogue();
        return true;
    }
    return false;
}

void Game::updateBossSpawn()
{
    if (!hasBossSpawnPoint_ || bossSpawned_ || !warpPointsEnabled_) {
        return;
    }
    if (hasCapturedBossForCurrentStage()) {
        resetBossEncounter();
        return;
    }
    if (bossEncounter_.phase != BossEncounterPhase::None) {
        return;
    }
    if (distanceSquared(player_.position, bossSpawnPoint_) > BossSpawnTriggerRadius * BossSpawnTriggerRadius) {
        return;
    }

    bossEncounter_ = BossEncounterState{};
    bossEncounter_.phase = BossEncounterPhase::WaitingBeforeDialogue;
    bossEncounter_.stageId = currentStageId_;
    bossEncounter_.spawnPoint = bossSpawnPoint_;
    if (queueStoryEventForCurrentStage("boss_before")) {
        return;
    }

    beginBossFightForCurrentEncounter();
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
    retrySnapshot_.dungeonMinimapCells = dungeonMinimapCells_;
    retrySnapshot_.runStats = runStats_;
    retrySnapshot_.warpPoints = warpPoints_;
    retrySnapshot_.rewardNodes = rewardNodes_;
    retrySnapshot_.moneyNodes = moneyNodes_;
    retrySnapshot_.moonFragmentNodes = moonFragmentNodes_;
    retrySnapshot_.chestNodes = chestNodes_;
    retrySnapshot_.crateNodes = crateNodes_;
    retrySnapshot_.enemyNodes = enemyNodes_;
    retrySnapshot_.dungeonEventInstances = dungeonEvents_.all();
    retrySnapshot_.enemies = enemies_;
    retrySnapshot_.worldDrops = worldDrops_;
    retrySnapshot_.worldDrops.removeTemporaryDrops();
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
    player_.hotDamageAccumulator = 0.0;
    player_.bleedDamageAccumulator = 0.0;
    player_.stunWakeTimer = 0.0f;
    player_.lastDamageSource = DamageSource::Unknown;
    player_.lastDamageEnemyName.clear();
    player_.status = EntityStatus{};

    tileMap_ = retrySnapshot_.tileMap;
    dungeonLayout_ = retrySnapshot_.dungeonLayout;
    dungeonMinimapCells_ = retrySnapshot_.dungeonMinimapCells;
    restoreInventoryCarryState(retrySnapshot_.inventory);
    runStats_ = retrySnapshot_.runStats;
    warpPoints_ = retrySnapshot_.warpPoints;
    rewardNodes_ = retrySnapshot_.rewardNodes;
    moneyNodes_ = retrySnapshot_.moneyNodes;
    moonFragmentNodes_ = retrySnapshot_.moonFragmentNodes;
    chestNodes_ = retrySnapshot_.chestNodes;
    crateNodes_ = retrySnapshot_.crateNodes;
    enemyNodes_ = retrySnapshot_.enemyNodes;
    dungeonEvents_.setInstances(retrySnapshot_.dungeonEventInstances);
    enemies_ = retrySnapshot_.enemies;
    enemies_.clearTemporaryState();
    worldDrops_ = retrySnapshot_.worldDrops;
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    spawnedWarpPointCount_ = retrySnapshot_.spawnedWarpPointCount;
    unlockedWarpPointCount_ = retrySnapshot_.unlockedWarpPointCount;
    bossSpawnPoint_ = retrySnapshot_.bossSpawnPoint;
    hasBossSpawnPoint_ = retrySnapshot_.hasBossSpawnPoint;
    bossSpawned_ = retrySnapshot_.bossSpawned;
    resetBossEncounter();
    effects_ = EffectSystem{};
    captureAbsorbAnimations_.clear();
    groundLines_ = GroundLineSystem{};
    wetGround_ = WetGroundSystem{};
    magic_ = MagicSystem{};
    magicFx_ = MagicFxSystem{};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
    levels_ = LevelSystem{};
    levelUpPresentation_ = {};
    inventoryReturnToPause_ = false;
    gameOverStatus_.clear();
    tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
    normalizeOpenBuriedPlacementNodes();
    updateDungeonMinimap(0.0);
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

void Game::maybeTriggerPlayerFootstep(
    Vec2 footAnchor,
    Vec2 movementDirection,
    bool walking,
    int frameIndex,
    int& previousFrame,
    PlayerFootstepSurface surface)
{
    const bool dustFrame = frameIndex == 3 || frameIndex == 6;
    if (walking && dustFrame && previousFrame != frameIndex) {
        if (!lightweightModeEnabled()) {
            spawnPlayerFootstepDust(footAnchor, movementDirection);
        }
        playPlayerFootstepSound(surface, frameIndex);
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

void Game::playPlayerFootstepSound(PlayerFootstepSurface surface, int frameIndex)
{
    std::string_view cueId;
    switch (surface) {
    case PlayerFootstepSurface::BaseOutdoor:
        cueId = AudioSeFootstepBaseOutdoor;
        break;
    case PlayerFootstepSurface::HomeInterior:
        cueId = AudioSeFootstepHomeInterior;
        break;
    case PlayerFootstepSurface::Dungeon:
        cueId = AudioSeFootstepDungeon;
        break;
    }
    if (cueId.empty()) {
        return;
    }

    const float sidePitchOffset = frameIndex == 3 ? -FootstepPitchSideOffset : FootstepPitchSideOffset;
    const float pitchScale = 1.0f + sidePitchOffset + randomFootstepPitchJitter();
    playAudioSe(cueId, 1.0f, pitchScale);
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

void Game::handleRingItemBreakEvents(std::vector<EffectDiscoveryEvent>* discoveryEvents)
{
    for (const RingItemBreakEvent& event : spellRing_.consumeItemBreakEvents()) {
        const ItemData* object = objectCatalog_.registry.findById(event.objectId);
        const std::vector<ObjectBreakEffectEntry> breakEffects = objectBreakEffectEntriesFor(object);
        const ObjectBreakSpec breakSpec = objectBreakSpecFor(breakEffects);
        const float breakScale = objectBreakVisualScale(breakSpec.value);
        const std::string name = object != nullptr && !object->name.empty()
            ? object->name
            : std::string(spellRingItemName(event.type));
        const std::string iconPrefix = object != nullptr ? inlineItemTag(event.objectId) + " " : "";
        const std::string message = iconPrefix + name + " が壊れた";
        const std::string mergeKey = event.instanceId.empty()
            ? "item_break:" + event.objectId
            : "item_break:" + event.instanceId;

        effects_.spawnItemBreak(event.position, itemBreakVisualFor(breakSpec.kind), breakScale);
        if (object != nullptr) {
            std::mt19937& rng = lootRuntimeRng();
            const int dryWetBonusDamage = dryWetBonusDamageFor(breakEffects);
            for (const ObjectBreakEffectEntry& effect : breakEffects) {
                if (effect.effectKey == "break_glass_shards" ||
                    effect.effectKey == "break_ceramic_shards" ||
                    effect.effectKey == "break_wood_fragments") {
                    ObjectBreakSpec shardSpec;
                    if (effect.effectKey == "break_glass_shards") {
                        shardSpec = {.kind = ObjectBreakKind::Glass, .effectKey = effect.effectKey, .value = effect.value};
                    } else if (effect.effectKey == "break_ceramic_shards") {
                        shardSpec = {.kind = ObjectBreakKind::Ceramic, .effectKey = effect.effectKey, .value = effect.value};
                    } else {
                        shardSpec = {.kind = ObjectBreakKind::Wood, .effectKey = effect.effectKey, .value = effect.value};
                    }
                    const int shardDamage = objectBreakShardDamage(shardSpec);
                    if (shardDamage > 0) {
                        enemies_.applyObjectBreakShardDamage(
                            event.position,
                            objectBreakShardRadius(shardSpec),
                            shardDamage,
                            objectBreakShardDamageType(shardSpec.kind),
                            shardSpec.effectKey,
                            spellRing_);
                        appendObjectEffectDiscovery(discoveryEvents, encyclopedia_, *object, shardSpec.effectKey, event.position);
                    }
                } else if (effect.effectKey == "break_fire_burst") {
                    EnemyMagicHitSpec spec;
                    spec.position = event.position;
                    spec.radius = objectBreakElementRadius(effect.effectKey, effect.value);
                    spec.damage = objectBreakFireDamage(effect.value);
                    spec.damageType = "fire";
                    spec.effectId = "break_fire_burst";
                    int driedWetCount = 0;
                    if (dryWetBonusDamage > 0) {
                        spec.consumeStateForBonus = "status_wet";
                        spec.consumeStateBonusDamage = dryWetBonusDamage;
                        spec.consumeStateBonusDamageType = "fire";
                        spec.consumeStateBonusEffectId = "dry_wet_bonus_damage";
                        spec.outConsumedStateCount = &driedWetCount;
                    }
                    enemies_.applyMagicArea(spec, spellRing_);
                    effects_.spawnAreaPulse(event.position, spec.radius, {255, 116, 54, 150});
                    appendObjectEffectDiscovery(discoveryEvents, encyclopedia_, *object, "break_fire_burst", event.position);
                    if (driedWetCount > 0) {
                        appendObjectEffectDiscovery(discoveryEvents, encyclopedia_, *object, "dry_wet_bonus_damage", event.position);
                    }
                } else if (effect.effectKey == "water_spray") {
                    EnemyMagicHitSpec spec;
                    spec.position = event.position;
                    spec.radius = objectBreakElementRadius(effect.effectKey, effect.value);
                    spec.damage = 0;
                    spec.damageType = "water";
                    spec.effectId = "water_spray";
                    spec.statusEffect = "status_wet";
                    spec.statusValue = std::max(1.0, effect.value);
                    spec.statusDuration = effect.duration > 0.0 ? effect.duration : 4.0;
                    spec.statusChance = 100.0;
                    const int hitCount = enemies_.applyMagicArea(spec, spellRing_);
                    effects_.spawnAreaPulse(event.position, spec.radius, {98, 186, 255, 145});
                    appendObjectEffectDiscovery(discoveryEvents, encyclopedia_, *object, "water_spray", event.position);
                    if (hitCount > 0) {
                        appendObjectEffectDiscovery(discoveryEvents, encyclopedia_, *object, "status_wet", event.position);
                    }
                } else if (effect.effectKey == "break_coin_spill") {
                    const int amount = objectBreakCoinSpillAmount(*object, effect.value);
                    if (worldDrops_.spawnMoneyDrop(
                            amount,
                            scatterLootPosition(event.position, rng),
                            runStats_.elapsedSeconds,
                            makeWorldLootJumpMotion(event.position, rng))) {
                        effects_.spawnAreaPulse(event.position, 38.0f, {246, 214, 108, 142});
                        appendObjectEffectDiscovery(discoveryEvents, encyclopedia_, *object, "break_coin_spill", event.position);
                    }
                } else if (effect.effectKey == "break_random_item_drop") {
                    const bool guarantee = shouldGuaranteeFirstDiscovery(
                        discoveryEvents,
                        encyclopedia_,
                        *object,
                        "break_random_item_drop");
                    if (guarantee || percentRollSucceeds(rng, effect.value)) {
                        const int depthRank = lootDepthRankForWorldPosition(
                            tileMap_,
                            dungeonLayout_,
                            currentStageId_,
                            event.position);
                        if (spawnWeightedObjectLoot(
                                LootChestKind::Common,
                                depthRank,
                                event.position,
                                rng,
                                "BreakRandomItemDrop",
                                true,
                                LootSourceKind::DigItem)) {
                            appendObjectEffectDiscovery(discoveryEvents, encyclopedia_, *object, "break_random_item_drop", event.position);
                        }
                    }
                }
            }
        }
        playAudioSe(AudioSeItemBreak);
        pushDungeonLog(message, mergeKey);
    }
}

void Game::renderDungeonEntrance(Renderer& renderer) const
{
    if (enemyTestActive_) {
        return;
    }

    const Vec2 center = dungeonEntrancePosition();
    renderer.fillEllipse(center + Vec2{0.0f, 44.0f}, {54.0f, 15.0f}, {0, 0, 0, 110});
    renderer.fillSoftCircle(center + Vec2{0.0f, 10.0f}, 68.0f, {96, 190, 220, 44});

    renderer.fillCircle(center + Vec2{0.0f, 6.0f}, 48.0f, {58, 62, 78, 245});
    renderer.fillRect(center + Vec2{-48.0f, 4.0f}, {96.0f, 54.0f}, {58, 62, 78, 245});
    renderer.drawCircle(center + Vec2{0.0f, 6.0f}, 48.0f, {170, 186, 204, 210});
    renderer.drawRect(center + Vec2{-48.0f, 4.0f}, {96.0f, 54.0f}, {170, 186, 204, 210});

    renderer.fillCircle(center + Vec2{0.0f, 13.0f}, 32.0f, {8, 12, 22, 252});
    renderer.fillRect(center + Vec2{-32.0f, 13.0f}, {64.0f, 45.0f}, {8, 12, 22, 252});
    renderer.drawCircle(center + Vec2{0.0f, 13.0f}, 32.0f, {64, 180, 218, 150});
    renderer.drawRect(center + Vec2{-32.0f, 13.0f}, {64.0f, 45.0f}, {64, 180, 218, 150});

    renderer.fillRect(center + Vec2{-42.0f, 52.0f}, {84.0f, 10.0f}, {122, 92, 62, 240});
    renderer.drawLine(center + Vec2{-38.0f, 52.0f}, center + Vec2{38.0f, 52.0f}, {234, 202, 132, 210});
    renderer.fillCircle(center + Vec2{-39.0f, 8.0f}, 6.0f, {134, 140, 154, 235});
    renderer.fillCircle(center + Vec2{37.0f, 11.0f}, 5.0f, {134, 140, 154, 235});
    renderer.fillCircle(center + Vec2{-24.0f, -24.0f}, 5.0f, {128, 134, 150, 225});
    renderer.fillCircle(center + Vec2{22.0f, -27.0f}, 5.0f, {128, 134, 150, 225});
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

    if (hasBossSpawnPoint_ && !bossSpawned_ && !hasCapturedBossForCurrentStage()) {
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
    const Vec2 playerLightCenter = witchSelfLightCenter(player_.position);

    for (const RewardNode& node : rewardNodes_) {
        if (node.collected) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(center, playerLightCenter, extraLights)) {
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
        if (!tileMap_.isLit(center, playerLightCenter, extraLights)) {
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
        if (!tileMap_.isLit(center, playerLightCenter, extraLights)) {
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
        if (!tileMap_.isLit(center, playerLightCenter, extraLights)) {
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
        if (node.mimicTriggered) {
            continue;
        }
        if (!node.revealed && !node.opened) {
            continue;
        }
        const Vec2 groundCenter = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(groundCenter, playerLightCenter, extraLights)) {
            continue;
        }
        const Vec2 center = chestVisualCenter(node) + Vec2{0.0f, -chestVisualAltitude(node)};
        if (node.visibility == PlacementVisibility::BuriedVisible && !node.opened) {
            entries.push_back(DepthRenderEntry{
                groundCenter.y,
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
            groundCenter.y,
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

} // namespace majo
