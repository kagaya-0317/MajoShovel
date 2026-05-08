#pragma once

#include "engine/Camera.hpp"
#include "engine/FileWatcher.hpp"
#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "data/GoogleSheetSource.hpp"
#include "data/EnemyCatalog.hpp"
#include "data/ObjectCatalog.hpp"
#include "data/RuntimeBalance.hpp"
#include "data/StageCatalog.hpp"
#include "game/DebugOverlay.hpp"
#include "game/DiggingSystem.hpp"
#include "game/DungeonLayout.hpp"
#include "game/EffectDispatcher.hpp"
#include "game/EffectSystem.hpp"
#include "game/EnemySystem.hpp"
#include "game/EncyclopediaSystem.hpp"
#include "game/InventorySystem.hpp"
#include "game/LevelSystem.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/ProjectileSystem.hpp"
#include "game/TileMap.hpp"
#include "game/UpgradeSystem.hpp"
#include "game/WorldDropSystem.hpp"

#include <optional>
#include <string>
#include <vector>

namespace majo {

class UiContext;

enum class ScreenMode {
    Base,
    Playing,
    PauseMenu,
    Inventory,
    Ring,
    LevelUp,
    GameOver,
    StageClear
};

enum class BaseArea {
    Outdoor,
    HomeInterior
};

enum class PauseMenuPage {
    Main,
    Status,
    Items,
    Ring,
    Options,
    QuitConfirm
};

class Game {
public:
    void initialize(int width, int height);
    void resize(int width, int height);
    void update(const Input& input, const Time& time);
    void render(Renderer& renderer, const Time& time);
    bool quitRequested() const { return quitRequested_; }

private:
    struct RunStats {
        float elapsedSeconds = 0.0f;
        int defeatedEnemies = 0;
        int dugTiles = 0;
        int acquiredItems = 0;
    };

    struct InventoryCarryState {
        InventorySystem inventory;
        std::vector<SpellRingItem> ringItems;
        bool valid = false;
    };

    struct WarpPoint {
        int stageId = 1;
        int index = 0;
        DungeonTile tilePosition{};
        Vec2 position{};
        bool discovered = false;
        float undiscoveredLightRadiusTiles = 3.0f;
        float discoveredLightRadiusTiles = 6.0f;
        // Compatibility state for the current checkpoint/base flow. New code
        // should prefer discovered/discoveredWarpPoints().
        bool unlocked = false;
        bool snapshotCaptured = false;
    };

    enum class PlacementVisibility {
        Exposed,
        BuriedVisible,
        BuriedHidden,
    };

    struct RewardNode {
        DungeonTile tile{};
        PlacementVisibility visibility = PlacementVisibility::Exposed;
        std::string rewardKind = "placeholder";
        std::optional<std::string> objectId;
        bool revealed = false;
        bool spawned = false;
        bool collected = false;
    };

    struct MoneyNode {
        DungeonTile tile{};
        int amount = 1;
        PlacementVisibility visibility = PlacementVisibility::Exposed;
        bool collected = false;
    };

    enum class EnemyPlacementType {
        Exposed,
        BuriedHidden,
    };

    struct EnemyNode {
        DungeonTile tile{};
        EnemyPlacementType placementType = EnemyPlacementType::Exposed;
        int dangerTier = 1;
        std::string enemySpawnGroup = "default";
        bool spawned = false;
    };

    struct RetrySnapshot {
        Vec2 playerPosition{};
        Vec2 playerFacing{1.0f, 0.0f};
        int playerHp = 10;
        int playerMaxHp = 10;
        int playerLevel = 1;
        int playerXp = 0;
        int playerXpToNext = 12;
        InventoryCarryState inventory;
        TileMap tileMap;
        DungeonLayout dungeonLayout;
        RunStats runStats{};
        std::vector<WarpPoint> warpPoints;
        std::vector<RewardNode> rewardNodes;
        std::vector<MoneyNode> moneyNodes;
        std::vector<EnemyNode> enemyNodes;
        int spawnedWarpPointCount = 0;
        int unlockedWarpPointCount = 0;
        Vec2 bossSpawnPoint{};
        bool hasBossSpawnPoint = false;
        bool valid = false;
    };

    enum class StorageEntryKind {
        Stack,
        Instance,
    };

    struct StorageEntry {
        StorageEntryKind kind = StorageEntryKind::Stack;
        int index = 0;
    };
    enum class ProcessingMode {
        Repair,
        Attack,
        Dig,
        Durability,
    };
    enum class RingWorkshopUpgrade {
        InitialRadius,
        InitialSpeed,
        ShiftDistance,
    };
    enum class BookshelfPage {
        Menu,
        Items,
        Treasures,
        Enemies,
    };

    void initializeWorld(bool captureRunStartInventory = true);
    void enterBase();
    void startMiningFromBase(bool useLatestWarpPoint);
    void applyPermanentUpgrades();
    float effectiveInitialRingRadius(int levelRadiusPoints) const;
    float effectiveInitialRingSpeed(int levelSpeedPoints) const;
    float effectiveRingShiftDistance() const;
    void configureWatcher();
    void checkHotReload();
    void loadSheetSourceConfig();
    bool loadBalanceFromSources(std::string& message);
    bool loadBalanceFromDisk(std::string& message);
    bool loadObjectsFromSheet();
    bool loadStagesFromSheet();
    bool loadEnemiesFromSheet();
    const StageDefinition& currentStageDefinition() const;
    void resolveCurrentStageDefinition();
    void refreshOrbitEffects();
    DungeonGenerationContext makeDungeonGenerationContext() const;
    void generateDungeonLayoutForRun();
    void updateCapturedProjectileBehaviors(float dt);
    void updateCapturedUtilityBehaviors(float dt);
    void updateAmbientParticleEffects(float dt);
    void handleCapturedExplosion(Vec2 position);
    enum class SellableKind {
        Stack,
        Instance,
    };
    struct SellableEntry {
        SellableKind kind = SellableKind::Stack;
        int index = 0;
        bool sellable = false;
        int price = 0;
        std::string blockedReason;
    };
    struct MerchantProduct {
        std::string objectId;
        int price = 0;
    };
    std::vector<SellableEntry> sellableObjects() const;
    bool isSellableObject(const ItemData& item) const;
    bool isStoryObject(const ItemData& item) const;
    int sellPrice(const ItemData& item, const ItemInstance* instance = nullptr) const;
    void refreshMerchantStock(bool force);
    void buyMerchantProduct(int index);
    std::vector<StorageEntry> processingEntries() const;
    const char* processingModeName(ProcessingMode mode) const;
    bool processingEntryAvailable(StorageEntry entry) const;
    int processingMoneyCost(StorageEntry entry, ProcessingMode mode) const;
    int processingOreCost(StorageEntry entry, ProcessingMode mode) const;
    void applyProcessing(int entryIndex);
    int warehouseCapacity() const;
    int warehouseUsedSlots() const;
    int backpackUsedSlots() const;
    std::vector<StorageEntry> backpackStorageEntries() const;
    std::vector<StorageEntry> warehouseStorageEntries() const;
    void depositBackpackEntry(int entryIndex);
    void withdrawWarehouseEntry(int entryIndex);
    std::string storageEntryLabel(StorageEntry entry, bool warehouseEntry) const;
    const ItemData* storageEntryItem(StorageEntry entry, bool warehouseEntry) const;
    const ItemInstance* storageEntryInstance(StorageEntry entry, bool warehouseEntry) const;
    int storageEntryStackCount(StorageEntry entry, bool warehouseEntry) const;
    int upgradeCost(int index) const;
    MaterialType upgradeMaterialType(int index) const;
    int upgradeMaterialCost(int index) const;
    const char* upgradeName(int index) const;
    int upgradeLevel(int index) const;
    int upgradeMaxLevel(int index) const;
    bool upgradeImplemented(int index) const;
    bool upgradeMaxed(int index) const;
    void buyUpgrade(int index);
    void openRingWorkshop();
    void resetRingWorkshopDraft();
    int ringLevelUpgradePointTotal() const;
    bool ringWorkshopRespecChanged() const;
    int ringWorkshopRespecMoneyCost() const;
    int ringWorkshopRespecMoonCost() const;
    bool adjustRingWorkshopRespec(int direction);
    void confirmRingWorkshopRespec();
    const char* ringWorkshopUpgradeName(RingWorkshopUpgrade upgrade) const;
    int ringWorkshopUpgradeLevel(RingWorkshopUpgrade upgrade) const;
    int ringWorkshopUpgradeMaxLevel(RingWorkshopUpgrade upgrade) const;
    int ringWorkshopUpgradeMoneyCost(RingWorkshopUpgrade upgrade) const;
    int ringWorkshopUpgradeMoonCost(RingWorkshopUpgrade upgrade) const;
    float ringWorkshopUpgradeCurrentValue(RingWorkshopUpgrade upgrade) const;
    float ringWorkshopUpgradeNextValue(RingWorkshopUpgrade upgrade) const;
    void buyRingWorkshopUpgrade(RingWorkshopUpgrade upgrade);
    void openBookshelf();
    void syncEncyclopediaFromInventoryAndRing();
    void applyEffectDiscoveries(const std::vector<EffectDiscoveryEvent>& discoveries);
    void addStoryFlag(std::string flag);
    void updateBookshelfScreen(const Input& input, UiContext& ui);
    void updateScreenMode(const Input& input, UiContext& ui, float dt);
    void updateBaseScreen(const Input& input, UiContext& ui, float dt);
    void updatePauseMenu(const Input& input, UiContext& ui);
    void choosePauseMenuItem(int item);
    void leavePausePage();
    void openRingScreen();
    void updateRingScreen(const Input& input, UiContext& ui);
    void cancelRingGrab();
    void enterGameOver();
    void updateGameOverScreen(const Input& input, UiContext& ui);
    void retryAfterGameOver();
    void returnToBaseAfterGameOver();
    bool shouldRefreshMerchantOnReturn(bool stageCleared, bool died) const;
    void returnToBaseFromNormalStage(bool stageCleared, bool died);
    InventoryCarryState captureInventoryCarryState() const;
    void restoreInventoryCarryState(const InventoryCarryState& state);
    void captureRunStartInventoryState();
    void clearTemporaryPlayerState(bool fullHeal);
    Vec2 latestWarpPointStartPosition() const;
    void rebuildUnlockedWarpPointsForStart(Vec2 latestPosition);
    void resetWarpPointRunState();
    void initializeWarpPointsFromLayout();
    int discoveredWarpPointCount() const;
    std::vector<WarpPoint> discoveredWarpPoints() const;
    int nearestWarpPointIndex(Vec2 position) const;
    void updateWarpPoints();
    void initializeRewardNodesFromLayout();
    void updateExposedRewardNodes();
    void revealRewardNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    int rewardNodeCount() const;
    int moneyNodeCount() const;
    int buriedVisibleNodeCount() const;
    int buriedHiddenNodeCount() const;
    void initializeEnemyNodesFromLayout();
    void updateExposedEnemyNodes();
    std::vector<Vec2> spawnHiddenEnemyNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    int exposedEnemyNodeCount() const;
    int buriedEnemyNodeCount() const;
    int spawnedEnemyNodeCount() const;
    void configureBossSpawnPointFromWarp(Vec2 warpPosition);
    void updateBossSpawn();
    void captureRetrySnapshotAtWarpPoint();
    void restoreRetrySnapshot();
    void renderWarpPoints(Renderer& renderer) const;
    void renderRewardNodes(Renderer& renderer, const std::vector<LightSource>& extraLights) const;
    void enterStageClear();
    void updateStageClearScreen(const Input& input, UiContext& ui);
    bool loadSaveData();
    bool saveSaveData(std::string& message) const;
    bool gameProgressPaused() const;
    bool basePresentationActive() const;
    void renderBaseScreen(Renderer& renderer) const;
    void renderBookshelfScreen(Renderer& renderer) const;
    void renderPauseMenu(Renderer& renderer) const;
    void renderRingScreen(Renderer& renderer) const;
    void renderGameOverScreen(Renderer& renderer) const;
    void renderStageClearScreen(Renderer& renderer) const;

    Camera camera_;
    RuntimeBalance balance_;
    GoogleSheetSourceConfig sheetSource_;
    EnemyCatalog enemyCatalog_;
    ObjectCatalog objectCatalog_;
    StageCatalog stageCatalog_;
    FileWatcher watcher_;
    Player player_;
    DungeonLayout dungeonLayout_;
    TileMap tileMap_;
    SpellRingSystem spellRing_;
    DiggingSystem digging_;
    EffectDispatcher effectDispatcher_;
    EffectSystem effects_;
    EnemySystem enemies_;
    ProjectileSystem projectiles_;
    InventorySystem inventory_;
    WorldDropSystem worldDrops_;
    EncyclopediaSystem encyclopedia_;
    LevelSystem levels_;
    UpgradeSystem upgrades_;
    DebugOverlay debug_;
    ScreenMode mode_ = ScreenMode::Base;
    BaseArea baseArea_ = BaseArea::Outdoor;
    Vec2 basePlayerPosition_{640.0f, 360.0f};
    Vec2 baseOutdoorPlayerPosition_{640.0f, 360.0f};
    Vec2 basePlayerFacing_{0.0f, 1.0f};
    float basePlayerSpriteAnimationTime_ = 0.0f;
    bool basePlayerSpriteWalking_ = false;
    int baseMenuSelection_ = 0;
    bool baseMiningStartChoiceActive_ = false;
    int baseMiningStartSelection_ = 0;
    bool baseStorageActive_ = false;
    bool baseStorageWarehousePane_ = false;
    int baseStorageBackpackSelection_ = 0;
    int baseStorageWarehouseSelection_ = 0;
    bool baseSellActive_ = false;
    bool baseMerchantBuyPane_ = false;
    int baseSellSelection_ = 0;
    int baseMerchantBuySelection_ = 0;
    bool baseUpgradeActive_ = false;
    int baseUpgradeSelection_ = 0;
    bool baseProcessingActive_ = false;
    int baseProcessingMode_ = 0;
    int baseProcessingSelection_ = 0;
    bool baseRingWorkshopActive_ = false;
    int baseRingWorkshopSelection_ = 0;
    int ringWorkshopDraftRadiusPoints_ = 0;
    int ringWorkshopDraftSpeedPoints_ = 0;
    bool baseBookshelfActive_ = false;
    BookshelfPage bookshelfPage_ = BookshelfPage::Menu;
    int bookshelfSelection_ = 0;
    std::string baseStatus_;
    PauseMenuPage pausePage_ = PauseMenuPage::Main;
    ScreenMode pauseReturnMode_ = ScreenMode::Playing;
    int pauseMenuSelection_ = 0;
    int pauseConfirmSelection_ = 1;
    int ringSlotSelection_ = 0;
    bool ringGrabActive_ = false;
    int ringGrabOrigin_ = -1;
    SpellRingItem ringGrabbedItem_{};
    std::string ringStatus_;
    RunStats runStats_{};
    int gameOverSelection_ = 0;
    std::string gameOverStatus_;
    bool bossSpawned_ = false;
    int stageClearSelection_ = 0;
    std::string stageClearStatus_;
    InventoryCarryState runStartInventoryState_{};
    RetrySnapshot retrySnapshot_{};
    std::vector<WarpPoint> warpPoints_;
    std::vector<RewardNode> rewardNodes_;
    std::vector<MoneyNode> moneyNodes_;
    std::vector<EnemyNode> enemyNodes_;
    int spawnedWarpPointCount_ = 0;
    Vec2 bossSpawnPoint_{};
    bool hasBossSpawnPoint_ = false;
    int currentStage_ = 0;
    std::string currentStageId_ = "stage_01_stardust";
    StageDefinition currentStageDefinition_{};
    int unlockedStages_ = 1;
    int unlockedWarpPointCount_ = 0;
    Vec2 latestWarpPointPosition_{};
    bool hasLatestWarpPointPosition_ = false;
    int money_ = 0;
    int maxHpUpgradeLevel_ = 0;
    int ringRadiusUpgradeLevel_ = 0;
    int ringSpeedUpgradeLevel_ = 0;
    int levelRingRadiusPoints_ = 0;
    int levelRingSpeedPoints_ = 0;
    int workshopInitialRadiusLevel_ = 0;
    int workshopInitialSpeedLevel_ = 0;
    int workshopShiftDistanceLevel_ = 0;
    bool merchantRefreshPending_ = false;
    int merchantUpgradeLevel_ = 1;
    int merchantStockVersion_ = 0;
    std::vector<MerchantProduct> merchantStock_;
    std::string highValueBuyCategory_;
    int warehouseCapacityLevel_ = 0;
    int processingUnlockLevel_ = 0;
    bool ringWorkshopUnlocked_ = false;
    bool autoSaveOnReturn_ = false;
    std::vector<std::string> storyFlags_;
    std::vector<InventoryObjectStack> warehouseObjectStacks_;
    std::vector<InventoryObjectInstance> warehouseObjectInstances_;
    bool roguelikeDungeon_ = false;
    bool warpPointsEnabled_ = true;
    // Normal stages keep current inventory on death. Future roguelike runs can
    // flip this and restore the run-start snapshot instead; save I/O is separate.
    bool restoreRunStartInventoryOnDeath_ = false;
    bool roguelikeCarryInRestricted_ = false;
    bool roguelikeCarryOutRestricted_ = false;
    bool inventoryReturnToPause_ = false;
    bool quitRequested_ = false;
    bool debugPaused_ = false;
    float captureCooldown_ = 0.0f;
    float ringTrailEffectTimer_ = 0.0f;
    float ambientParticleTimer_ = 0.0f;
    float reloadNoticeTimer_ = 0.0f;
    std::string reloadNotice_;
};

}
