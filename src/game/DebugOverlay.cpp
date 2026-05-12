#include "game/DebugOverlay.hpp"

#include <cstdio>
#include <string>

namespace majo {

namespace {

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
    int rewardNodeCount,
    int moneyNodeCount,
    int buriedVisibleNodeCount,
    int buriedHiddenNodeCount,
    int exposedEnemyNodeCount,
    int buriedEnemyNodeCount,
    int spawnedEnemyNodeCount,
    bool autoReloadBlocked)
{
    if (!visible_) {
        return;
    }
    const std::string enemySummary = enemies.debugEnemySummary();
    const DungeonLayoutMetrics dungeonMetrics = calculateDungeonLayoutMetrics(
        dungeonLayout,
        {static_cast<float>(map.worldToTile(player.position.x)), static_cast<float>(map.worldToTile(player.position.y))});
    const SpecialRoomMetrics roomMetrics = calculateSpecialRoomMetrics(
        dungeonLayout,
        {static_cast<float>(map.worldToTile(player.position.x)), static_cast<float>(map.worldToTile(player.position.y))});
    const TerrainDebugInfo terrain = map.terrainDebugAtWorld(player.position);
    char buffer[1536];
    std::snprintf(buffer, sizeof(buffer),
        "FPS: %03d\n敵: %03d\nチャンク: %02d/%02zu\nリング半径: %03d\nリング速度: %.2f\nリング %d/%d 形状 %s\nレベル: %02d 経験値: %02d/%02d\n投げ待ち: %02d%%\nStage %d Seed %u\nStart (%d,%d) Goal (%d,%d)\n距離 %.1f 進捗 %.2f 主道距離 %.1f\n地形 %s HP %d/%d 硬さ %.2f\nWarp nearest %d %s discovered %d",
        static_cast<int>(time.fps()),
        enemies.activeCount(),
        map.activeChunkCount(),
        map.generatedChunkCount(),
        static_cast<int>(spellRing.radius()),
        spellRing.angularSpeed(),
        spellRing.activeRingIndex() + 1,
        spellRing.runtimeRingCount(),
        ringShapeName(spellRing.activeRingShape()),
        player.level,
        player.xp,
        player.xpToNext,
        static_cast<int>(spellRing.cooldownRatio(player, balance) * 100.0f),
        dungeonLayout.stageId,
        dungeonLayout.seed,
        dungeonLayout.startTile.x,
        dungeonLayout.startTile.y,
        dungeonLayout.goalTile.x,
        dungeonLayout.goalTile.y,
        dungeonMetrics.distanceFromStart,
        dungeonMetrics.pathProgress,
        terrain.distanceFromMainPath,
        tileTypeName(terrain.type),
        terrain.hp,
        terrain.effectiveHp,
        terrain.localHardnessMultiplier,
        nearestWarpIndex,
        nearestWarpDiscovered ? "found" : "hidden",
        discoveredWarpCount);
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nReward nodes %d Money nodes %d Buried visible %d hidden %d",
        rewardNodeCount,
        moneyNodeCount,
        buriedVisibleNodeCount,
        buriedHiddenNodeCount);
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nEnemy nodes exposed %d buried %d spawned %d",
        exposedEnemyNodeCount,
        buriedEnemyNodeCount,
        spawnedEnemyNodeCount);
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nSpecial rooms %zu current %s nearest %s",
        dungeonLayout.specialRoomAnchors.size(),
        specialRoomTypeName(roomMetrics.currentRoomType),
        specialRoomTypeName(roomMetrics.nearestRoomType));
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nStageCatalog %s / %s / %s\nProfile gen=%s terrain=%s goal=%d hard=%.2f warp=%d special=%d",
        currentStage.id.c_str(),
        currentStage.name.c_str(),
        currentStage.type.c_str(),
        currentStage.generationProfile.c_str(),
        currentStage.terrainProfile.c_str(),
        currentStage.goalDistanceTiles,
        currentStage.terrainHardnessMultiplier,
        currentStage.warpPointCount,
        currentStage.specialRoomCount);
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\n%s",
        enemySummary.c_str());
    std::snprintf(
        buffer + std::char_traits<char>::length(buffer),
        sizeof(buffer) - std::char_traits<char>::length(buffer),
        "\nAuto reload block %s",
        autoReloadBlocked ? "ON" : "OFF");
    renderer.setScreenSpace();
    renderer.fillRect({10.0f, 10.0f}, {620.0f, 492.0f}, {0, 0, 0, 180});
    renderer.drawText({20.0f, 20.0f}, buffer, {220, 244, 224, 255}, 2);
}

}
