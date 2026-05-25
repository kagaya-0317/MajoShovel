#pragma once

#include "engine/Math.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace majo {

enum class GameTestScreenMode {
    OpeningKamishibai,
    EndingKamishibai,
    Title,
    Base,
    WorldLoading,
    Playing,
    PauseMenu,
    Inventory,
    Ring,
    ObjectImageScaleEdit,
    AudioCueEdit,
    LevelUp,
    GameOver,
    StageClear,
    AstralResult,
};

enum class GameTestRingState {
    Normal,
    Thrown,
    Returning,
};

enum class GameTestDropKind {
    Object,
    Money,
    Material,
};

enum class GameTestInventoryLocation {
    Backpack,
    Warehouse,
};

enum class GameTestObjectEntryKind {
    Stack,
    Instance,
};

enum class GameTestCodexStage {
    Undiscovered = 0,
    Discovered = 1,
    Obtained = 2,
    Equipped = 3,
    EffectTriggered = 4,
    Complete = 5,
};

enum class GameTestTerrainKind {
    Empty,
    Dirt,
    Rock,
    Ore,
    HardRock,
};

enum class GameTestTerrainAttribute {
    None,
    Soft,
    Hard,
    Ore,
};

enum class GameTestIconKind {
    None,
    Object,
    World,
};

struct GameTestRunStats {
    float elapsedSeconds = 0.0f;
    int defeatedEnemies = 0;
    int dugTiles = 0;
    int acquiredItems = 0;
    int acquiredObjectItems = 0;
};

struct GameTestPlayerSnapshot {
    Vec2 position{};
    Vec2 facing{1.0f, 0.0f};
    Vec2 velocity{};
    float radius = 0.0f;
    int hp = 0;
    int maxHp = 0;
    int level = 1;
};

struct GameTestEnemySnapshot {
    Vec2 position{};
    bool boss = false;
};

struct GameTestRingItemSnapshot {
    int ringIndex = 0;
    int itemIndex = 0;
    std::string objectId;
    std::string instanceId;
    std::string name;
    std::string category;
    std::string damageType;
    std::vector<std::string> tags;
    Vec2 worldPosition{};
    int damage = 0;
    int digPower = 0;
    float hitRadius = 0.0f;
    int durability = -1;
    int maxDurability = -1;
    int rarity = 0;
    int price = 0;
    double weightKg = 0.0;
    int enhanceLevel = 0;
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    bool protectionEnabled = false;
    bool broken = false;
};

struct GameTestRingSnapshot {
    int activeRingIndex = 0;
    float activeRadius = 0.0f;
    float activeAngularSpeed = 0.0f;
    float activeWeight = 0.0f;
    float activeMaxWeight = 0.0f;
    Vec2 anchorOffsetFromPlayer{};
    float currentOffsetDistance = 0.0f;
    float maxOffsetDistance = 0.0f;
    int activeItemCount = 0;
    int activeMaxItemCount = 0;
    bool activeCanAddItem = false;
    int bestDamage = 0;
    int bestDigPower = 0;
    float bestHitRadius = 0.0f;
    bool hasCombatTool = false;
    bool hasDigTool = false;
    std::vector<GameTestRingItemSnapshot> items;
};

struct GameTestWarpPointSnapshot {
    Vec2 position{};
    int index = 0;
    bool discovered = false;
};

struct GameTestChestSnapshot {
    Vec2 position{};
    bool revealed = false;
    bool opened = false;
};

struct GameTestDropSnapshot {
    GameTestDropKind kind = GameTestDropKind::Object;
    std::string id;
    std::string displayName;
    GameTestIconKind iconKind = GameTestIconKind::None;
    std::string iconKey;
    Vec2 position{};
    int quantity = 1;
};

struct GameTestMineTileSnapshot {
    Vec2 center{};
    Vec2 surfacePoint{};
    Vec2 outwardNormal{1.0f, 0.0f};
    int tileX = 0;
    int tileY = 0;
    int hp = 0;
    int effectiveHp = 0;
    GameTestTerrainKind terrainKind = GameTestTerrainKind::Empty;
    GameTestTerrainAttribute terrainAttribute = GameTestTerrainAttribute::None;
    float localHardnessMultiplier = 1.0f;
    float distanceFromMainPath = 0.0f;
    bool solid = false;
    bool diggable = false;
};

struct GameTestPathTileSnapshot {
    Vec2 center{};
    int tileX = 0;
    int tileY = 0;
    int hp = 0;
    int effectiveHp = 0;
    GameTestTerrainKind terrainKind = GameTestTerrainKind::Empty;
    GameTestTerrainAttribute terrainAttribute = GameTestTerrainAttribute::None;
    float localHardnessMultiplier = 1.0f;
    float distanceFromMainPath = 0.0f;
    bool solid = false;
    bool diggable = false;
};

struct GameTestCollisionRectSnapshot {
    Vec2 pos{};
    Vec2 size{};
};

struct GameTestPathGridSnapshot {
    int minTileX = 0;
    int minTileY = 0;
    int width = 0;
    int height = 0;
    std::vector<GameTestPathTileSnapshot> tiles;
    std::vector<GameTestCollisionRectSnapshot> objectBlockers;
};

struct GameTestObjectEntrySnapshot {
    GameTestInventoryLocation location = GameTestInventoryLocation::Backpack;
    GameTestObjectEntryKind kind = GameTestObjectEntryKind::Stack;
    std::string objectId;
    std::string instanceId;
    std::string name;
    std::string category;
    std::string damageType;
    std::vector<std::string> tags;
    int count = 1;
    int rarity = 0;
    int price = 0;
    int sellPrice = 0;
    int attackPower = 0;
    int digPower = 0;
    float staffEquipScore = 0.0f;
    int durability = -1;
    double weightKg = 0.0;
    int currentDurability = -1;
    int maxDurability = -1;
    int enhanceLevel = 0;
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    double weightModifier = 1.0;
    double sizeModifier = 1.0;
    bool protectionEnabled = false;
    bool equipped = false;
    bool broken = false;
    bool important = false;
    bool sellable = true;
    GameTestCodexStage codexStage = GameTestCodexStage::Undiscovered;
    bool canRepair = false;
    int repairMoneyCost = 0;
    bool canEnhanceAttack = false;
    int enhanceAttackMoneyCost = 0;
    int enhanceAttackOreCost = 0;
    bool canEnhanceDig = false;
    int enhanceDigMoneyCost = 0;
    int enhanceDigOreCost = 0;
};

struct GameTestInventorySnapshot {
    int backpackUsedSlots = 0;
    int backpackCapacity = 0;
    int warehouseUsedSlots = 0;
    int warehouseCapacity = 0;
    std::vector<GameTestObjectEntrySnapshot> backpackItems;
    std::vector<GameTestObjectEntrySnapshot> warehouseItems;
};

struct GameTestMaterialSnapshot {
    int oldWoodBuildingMaterial = 0;
    int enhancementOre = 0;
    int moonFragment = 0;
    int manaDrop = 0;
};

struct GameTestUpgradeSnapshot {
    int index = -1;
    std::string name;
    std::string materialName;
    int level = 0;
    int maxLevel = 0;
    int moneyCost = 0;
    int materialCost = 0;
    bool implemented = false;
    bool maxed = false;
    bool affordable = false;
};

struct GameTestBaseSnapshot {
    bool active = false;
    int money = 0;
    GameTestMaterialSnapshot materials;
    bool ringWorkshopUnlocked = false;
    std::vector<GameTestUpgradeSnapshot> upgrades;
};

struct GameTestDungeonSnapshot {
    bool active = false;
    std::uint32_t seed = 0;
    Vec2 startWorld{};
    Vec2 goalWorld{};
    std::vector<Vec2> mainPathWorldPoints;
    std::vector<GameTestWarpPointSnapshot> warpPoints;
    Vec2 bossSpawnPoint{};
    bool hasBossSpawnPoint = false;
    bool bossSpawned = false;
    int discoveredWarpPoints = 0;
    int unlockedWarpPoints = 0;
};

struct GameTestSnapshot {
    GameTestScreenMode screenMode = GameTestScreenMode::Base;
    std::string stageId;
    std::string stageName;
    bool worldLoading = false;
    bool transitionActive = false;
    bool dialogueActive = false;
    bool dungeonFocusActive = false;
    bool bossPresentationActive = false;
    bool firstItemNoticeActive = false;
    bool pendingStoryDelayActive = false;
    bool warpReturnConfirmOpen = false;
    bool introTutorialActive = false;
    Vec2 cameraPosition{};
    int viewportWidth = 1280;
    int viewportHeight = 720;
    GameTestPlayerSnapshot player;
    GameTestRingState ringState = GameTestRingState::Normal;
    Vec2 ringCenter{};
    GameTestRingSnapshot ring;
    GameTestDungeonSnapshot dungeon;
    std::vector<GameTestEnemySnapshot> enemies;
    std::vector<GameTestChestSnapshot> chests;
    std::vector<GameTestDropSnapshot> drops;
    std::vector<GameTestMineTileSnapshot> nearbyMineTiles;
    GameTestPathGridSnapshot pathGrid;
    GameTestInventorySnapshot inventory;
    GameTestBaseSnapshot base;
    GameTestRunStats runStats;
    int money = 0;
    int totalMaterials = 0;
};

} // namespace majo
