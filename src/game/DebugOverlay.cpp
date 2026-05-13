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
    const int playerTileX = map.worldToTile(player.position.x);
    const int playerTileY = map.worldToTile(player.position.y);

    char buffer[2048];
    std::snprintf(buffer, sizeof(buffer),
        "FPS: %03d   Auto reload block: %s\n"
        "Stage: %d %s / %s   Seed: %u\n"
        "Progress: %.1f%%   Dist: %.1f   Start(%d,%d) Goal(%d,%d)\n"
        "Player: HP %d/%d   Lv %02d XP %02d/%02d   Tile(%d,%d)\n"
        "Terrain: %s HP %d/%d Hard %.2f   MainPathDist %.1f\n"
        "Ring: %d/%d %s   R %03d Speed %.2f Throw %02d%%\n"
        "Warp: nearest %d %s   found %d/%d\n"
        "Chunks: active %02d generated %02zu   Enemies: active %03d",
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
        terrain.distanceFromMainPath,
        spellRing.activeRingIndex() + 1,
        spellRing.runtimeRingCount(),
        ringShapeName(spellRing.activeRingShape()),
        static_cast<int>(spellRing.radius()),
        spellRing.angularSpeed(),
        static_cast<int>(spellRing.cooldownRatio(player, balance) * 100.0f),
        nearestWarpIndex,
        nearestWarpDiscovered ? "found" : "hidden",
        discoveredWarpCount,
        currentStage.warpPointCount,
        map.activeChunkCount(),
        map.generatedChunkCount(),
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
        "\nProfile: gen=%s terrain=%s goal=%d hard=%.2f special=%d\nEnemies:\n%s",
        currentStage.generationProfile.c_str(),
        currentStage.terrainProfile.c_str(),
        currentStage.goalDistanceTiles,
        currentStage.terrainHardnessMultiplier,
        currentStage.specialRoomCount,
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
