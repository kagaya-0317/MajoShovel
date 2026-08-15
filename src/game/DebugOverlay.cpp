#include "game/DebugOverlay.hpp"

#include "engine/FrameProfiler.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace majo {

namespace {

const char* yesNo(bool value)
{
    return value ? "yes" : "no";
}

const char* tileTypeName(TileType type)
{
    switch (type) {
    case TileType::Empty:
        return "空洞";
    case TileType::Dirt:
        return "土";
    case TileType::Rock:
        return "岩";
    case TileType::Ore:
        return "鉱石";
    case TileType::HardRock:
        return "硬い岩";
    }
    return "不明";
}

double profileMilliseconds(const FrameProfileSnapshot& snapshot, const char* name)
{
    for (std::size_t i = 0; i < snapshot.count; ++i) {
        const FrameProfileSample& sample = snapshot.samples[i];
        if (std::strcmp(sample.name, name) == 0) {
            return sample.milliseconds;
        }
    }
    return 0.0;
}

}

void DebugOverlay::appendAutoSimulationText(
    char* buffer,
    std::size_t bufferSize,
    bool enabled,
    const autosim::AutoSimulationDebugSnapshot& debug,
    bool includeDungeonDetails)
{
    if (!enabled || buffer == nullptr || bufferSize == 0) {
        return;
    }

    const std::size_t used = std::char_traits<char>::length(buffer);
    if (used >= bufferSize) {
        return;
    }
    if (!debug.active) {
        std::snprintf(
            buffer + used,
            bufferSize - used,
            "\nAutoSim: idle"
            "\nAutoDecision: phase=idle detail=オートシミュは停止中");
        return;
    }

    std::snprintf(
        buffer + used,
        bufferSize - used,
        "\nAutoSim: %s x%d steps=%d plan=%s goal=%s reason=%s lock=%s %.1fs"
        "\nAutoDecision: phase=%s detail=%s"
        "\nAutoLastAction: %s"
        "\nAutoObjective: mission=%s(%s) task=%s(%s) noProgress=%.1f opportunity=%d suspend=%.1f"
        "\nAutoBase: screen=%s backpack=%d/%d free=%d desired=%d ready=%s canDepart=%s warehouse=%d/%d enhanceBudget=%d/%d remain=%d idle=%.2f cooldown=%.2f pending=%s",
        autosim::autoSimulationStateName(debug.state),
        debug.speedMultiplier,
        debug.simulationStepsLastFrame,
        yesNo(debug.hasPlan),
        autosim::autoSimulationGoalName(debug.goal),
        debug.reason.empty() ? "-" : debug.reason.c_str(),
        yesNo(debug.lockedPlanActive),
        debug.planLockSeconds,
        debug.decisionPhase.empty() ? "-" : debug.decisionPhase.c_str(),
        debug.decisionDetail.empty() ? "-" : debug.decisionDetail.c_str(),
        debug.lastActionResult.empty() ? "-" : debug.lastActionResult.c_str(),
        autosim::autoSimulationGoalName(debug.missionGoal),
        debug.missionReason.empty() ? "-" : debug.missionReason.c_str(),
        autosim::autoSimulationGoalName(debug.taskGoal),
        debug.taskReason.empty() ? "-" : debug.taskReason.c_str(),
        debug.missionNoProgressSeconds,
        debug.opportunityBudget,
        debug.opportunitySuspendSeconds,
        yesNo(debug.baseScreen),
        debug.backpackUsedSlots,
        debug.backpackCapacity,
        debug.backpackFreeSlots,
        debug.desiredBackpackFreeSlots,
        yesNo(debug.backpackReadyForDeparture),
        yesNo(debug.backpackCanDepart),
        debug.warehouseUsedSlots,
        debug.warehouseCapacity,
        debug.enhancementBudgetSpent,
        debug.enhancementBudgetLimit,
        debug.enhancementBudgetRemaining,
        debug.baseIdleSeconds,
        debug.actionCooldownSeconds,
        yesNo(debug.pendingAction));

    if (!includeDungeonDetails) {
        return;
    }
    const std::size_t commonUsed = std::char_traits<char>::length(buffer);
    if (commonUsed >= bufferSize) {
        return;
    }
    std::snprintf(
        buffer + commonUsed,
        bufferSize - commonUsed,
        "\nAutoPlan: player(%.0f,%.0f) target=%s(%.0f,%.0f) d=%.1f move=%s(%.0f,%.0f) d=%.1f arrive=%.1f axis(%.2f,%.2f)"
        "\nAutoRoute: path=%d wp=%d digAt=%d dig=%d hard=%d avoidHard=%s stuck=%d still=%.1f move=%.1f mineIdle=%.1f escape=%.1f"
        "\nAutoLight: active=%.0f backpack=%.0f missing=%s"
        "\nAutoWarp: known=%d discovered=%d unlocked=%d total=%d nearest=%d %s known=%s d=%.1f"
        "\nAutoWarpTarget: target=%d %s known=%s dToPlan=%.1f nextUnknown=%d %s d=%.1f",
        debug.playerWorld.x,
        debug.playerWorld.y,
        yesNo(debug.hasTarget),
        debug.targetWorld.x,
        debug.targetWorld.y,
        debug.distanceToTarget,
        yesNo(debug.hasMoveTarget),
        debug.moveTargetWorld.x,
        debug.moveTargetWorld.y,
        debug.distanceToMoveTarget,
        debug.moveTargetArriveDistance,
        debug.inputMoveAxis.x,
        debug.inputMoveAxis.y,
        debug.routePathTileCount,
        debug.routeWaypointPathIndex,
        debug.routeFirstDigPathIndex,
        debug.routeDigTileCount,
        debug.routeHardTileCount,
        yesNo(debug.routeAvoidingHardWall),
        debug.stuckCount,
        debug.stillSeconds,
        debug.stuckMoveDistance,
        debug.miningNoProgressSeconds,
        debug.escapeStuckSeconds,
        debug.activeLightRadius,
        debug.bestBackpackLightRadius,
        yesNo(debug.missingLight),
        debug.knownWarpPoints,
        debug.discoveredWarpPoints,
        debug.unlockedWarpPoints,
        debug.totalWarpPoints,
        debug.nearestWarpIndex,
        debug.nearestWarpDiscovered ? "found" : "hidden",
        yesNo(debug.nearestWarpKnown),
        debug.nearestWarpDistance,
        debug.targetWarpIndex,
        debug.targetWarpDiscovered ? "found" : "hidden",
        yesNo(debug.targetWarpKnown),
        debug.targetWarpDistance,
        debug.nextUnknownWarpIndex,
        debug.nextUnknownWarpDiscovered ? "found" : "hidden",
        debug.nextUnknownWarpDistance);
}

void DebugOverlay::render(
    Renderer& renderer,
    const Time& time,
    const EnemySystem& enemies,
    const TileMap& map,
    const SpellRingSystem& spellRing,
    const Player& player,
    const RuntimeBalance& balance,
    const DungeonLayout& dungeonLayout,
    const StageDefinition& currentStage,
    int nearestWarpIndex,
    bool nearestWarpDiscovered,
    int discoveredWarpCount,
    int unlockedWarpCount,
    bool hasLatestWarpPointPosition,
    Vec2 latestWarpPointPosition,
    bool hasRequestedWarpPointStartPosition,
    Vec2 requestedWarpPointStartPosition,
    int rewardNodeCount,
    int moneyNodeCount,
    int buriedVisibleNodeCount,
    int buriedHiddenNodeCount,
    int exposedEnemyNodeCount,
    int buriedEnemyNodeCount,
    int spawnedEnemyNodeCount,
    bool autoReloadBlocked,
    bool autoSimulationDebugActive,
    const autosim::AutoSimulationDebugSnapshot& autoSimulationDebug)
{
    if (!visible_) {
        return;
    }
    const std::string enemySummary = enemies.debugEnemySummary(player.position);
    const DungeonLayoutMetrics dungeonMetrics = calculateDungeonLayoutMetrics(
        dungeonLayout,
        {static_cast<float>(map.worldToTile(player.position.x)), static_cast<float>(map.worldToTile(player.position.y))});
    const SpecialRoomMetrics roomMetrics = calculateSpecialRoomMetrics(
        dungeonLayout,
        {static_cast<float>(map.worldToTile(player.position.x)), static_cast<float>(map.worldToTile(player.position.y))});
    const TerrainDebugInfo terrain = map.terrainDebugAtWorld(player.position);
    const int playerTileX = map.worldToTile(player.position.x);
    const int playerTileY = map.worldToTile(player.position.y);
    char latestWarpText[64];
    std::snprintf(
        latestWarpText,
        sizeof(latestWarpText),
        hasLatestWarpPointPosition ? "(%.0f,%.0f)" : "-",
        latestWarpPointPosition.x,
        latestWarpPointPosition.y);
    char requestedWarpStartText[64];
    std::snprintf(
        requestedWarpStartText,
        sizeof(requestedWarpStartText),
        hasRequestedWarpPointStartPosition ? "(%.0f,%.0f)" : "-",
        requestedWarpPointStartPosition.x,
        requestedWarpPointStartPosition.y);

    const FrameProfileSnapshot& profile = frameProfiler().snapshot();
    char buffer[8192];
    std::snprintf(buffer, sizeof(buffer),
        "FPS: %03d   Auto reload block: %s\n"
        "Stage: %d %s / %s   Seed: %u\n"
        "Progress: %.1f%%   Dist: %.1f   Start(%d,%d) Goal(%d,%d)\n"
        "Player: HP %d/%d   Lv %02d XP %02d/%02d   Tile(%d,%d)\n"
        "Terrain: %s HP %d/%d Hard %.2f Depth %d x%.2f   MainPathDist %.1f\n"
        "Ring: %d/%d %s   R %03d Speed %.2f Throw %02d%%\n"
        "Warp: nearest %d %s   found %d/%d unlocked %d latest %s startReq %s\n"
        "Chunks: active %02d generated %02zu   Enemies: ambient %02d/%02d event %02d boss %02d total %03d",
        static_cast<int>(time.fps()),
        autoReloadBlocked ? "ON" : "OFF",
        dungeonLayout.stageId,
        currentStage.name.c_str(),
        currentStage.type.c_str(),
        dungeonLayout.seed,
        dungeonMetrics.pathProgress * 100.0f,
        dungeonMetrics.distanceFromStart,
        dungeonLayout.startTile.x,
        dungeonLayout.startTile.y,
        dungeonLayout.goalTile.x,
        dungeonLayout.goalTile.y,
        player.hp,
        player.maxHp,
        player.level,
        player.xp,
        player.xpToNext,
        playerTileX,
        playerTileY,
        tileTypeName(terrain.type),
        terrain.hp,
        terrain.effectiveHp,
        terrain.localHardnessMultiplier,
        terrain.depthRank,
        terrain.depthHardnessMultiplier,
        terrain.distanceFromMainPath,
        spellRing.activeRingIndex() + 1,
        spellRing.runtimeRingCount(),
        ringShapeName(spellRing.activeRingShape()),
        static_cast<int>(spellRing.radius()),
        spellRing.angularSpeed(),
        static_cast<int>(spellRing.cooldownRatio(balance) * 100.0f),
        nearestWarpIndex,
        nearestWarpDiscovered ? "found" : "hidden",
        discoveredWarpCount,
        currentStage.warpPointCount,
        unlockedWarpCount,
        latestWarpText,
        requestedWarpStartText,
        map.activeChunkCount(),
        map.generatedChunkCount(),
        enemies.ambientActiveCount(),
        balance.enemySoftCap,
        enemies.eventActiveCount(),
        enemies.bossSourceActiveCount(),
        enemies.activeCount());
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nNodes: reward %d money %d buried visible %d hidden %d",
        rewardNodeCount,
        moneyNodeCount,
        buriedVisibleNodeCount,
        buriedHiddenNodeCount);
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nEnemyNodes: exposed %d buried %d spawned %d",
        exposedEnemyNodeCount,
        buriedEnemyNodeCount,
        spawnedEnemyNodeCount);
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nSpecialRoom: total %zu current %s nearest %s dist %.1f",
        dungeonLayout.specialRoomAnchors.size(),
        specialRoomTypeName(roomMetrics.currentRoomType),
        specialRoomTypeName(roomMetrics.nearestRoomType),
        roomMetrics.distanceToNearestRoom);
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nProfile: gen=%s terrain=%s goal=%d hard=%.2f special=%d",
        currentStage.generationProfile.c_str(),
        currentStage.terrainProfile.c_str(),
        currentStage.goalDistanceTiles,
        currentStage.terrainHardnessMultiplier,
        currentStage.specialRoomCount);
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nPerf: frame %.2fms app %.2f events %.2f input %.2f audio %.2f update %.2f render %.2f"
        "\nHot: gameU %.2f tileU %.2f dig %.2f enemy %.2f projU %.2f fxU %.2f gameR %.2f tileR %.2f depth %.2f ui %.2f",
        profile.frameMilliseconds,
        profileMilliseconds(profile, "App.frame"),
        profileMilliseconds(profile, "App.events"),
        profileMilliseconds(profile, "App.input"),
        profileMilliseconds(profile, "App.audio"),
        profileMilliseconds(profile, "App.update"),
        profileMilliseconds(profile, "App.render"),
        profileMilliseconds(profile, "Game.update"),
        profileMilliseconds(profile, "TileMap.update"),
        profileMilliseconds(profile, "Digging.update"),
        profileMilliseconds(profile, "Enemies.update"),
        profileMilliseconds(profile, "Projectiles.update"),
        profileMilliseconds(profile, "Fx.update"),
        profileMilliseconds(profile, "Game.render"),
        profileMilliseconds(profile, "TileMap.render"),
        profileMilliseconds(profile, "WorldDepth.draw"),
        profileMilliseconds(profile, "DungeonUI.render"));
    appendAutoSimulationText(
        buffer,
        sizeof(buffer),
        autoSimulationDebugActive,
        autoSimulationDebug,
        true);
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nEnemies:\n%s",
        enemySummary.c_str());
    renderer.setScreenSpace();
    constexpr Vec2 PanelPos{10.0f, 10.0f};
    constexpr float PanelWidth = 570.0f;
    constexpr float PanelPadding = 10.0f;
    constexpr int TextScale = 2;
    constexpr float MinPanelHeight = 40.0f;
    const float textWidth = PanelWidth - PanelPadding * 2.0f;
    const Vec2 textSize = renderer.measureWrappedText(buffer, textWidth, TextScale);
    const float panelHeight = std::max(MinPanelHeight, textSize.y + PanelPadding * 2.0f);
    renderer.fillRect(PanelPos, {PanelWidth, panelHeight}, {0, 0, 0, 125});
    renderer.drawWrappedText(
        PanelPos + Vec2{PanelPadding, PanelPadding},
        buffer,
        textWidth,
        {220, 244, 224, 255},
        TextScale);
}

}
