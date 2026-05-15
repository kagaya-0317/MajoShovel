#pragma once

#include "engine/Camera.hpp"
#include "engine/FileWatcher.hpp"
#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "engine/Ui.hpp"
#include "data/GoogleSheetSource.hpp"
#include "data/EnemyCatalog.hpp"
#include "data/ObjectCatalog.hpp"
#include "data/RuntimeBalance.hpp"
#include "data/StageCatalog.hpp"
#include "game/DebugOverlay.hpp"
#include "game/DepthRender.hpp"
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

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace majo {

class UiContext;

enum class ScreenMode {
    Base,
    WorldLoading,
    Playing,
    PauseMenu,
    Inventory,
    Ring,
    ObjectImageScaleEdit,
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

enum class BaseEditMode {
    None,
    Facility,
    Passability,
};

enum class ImageScaleEditTab {
    Objects,
    Others,
};

struct BaseEditRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct BaseEditSnapshot {
    std::unordered_map<std::string, BaseEditRect> outdoorFacilityRects;
    std::unordered_map<std::string, BaseEditRect> homeFacilityRects;
    std::unordered_set<std::int64_t> outdoorBlockedTiles;
    std::unordered_set<std::int64_t> homeBlockedTiles;
};

class Game {
public:
    void initialize(int width, int height);
    void resize(int width, int height);
    void update(const Input& input, const Time& time);
    void render(Renderer& renderer, const Time& time);
    bool executeDebugCommand(std::string_view command);
    bool quitRequested() const { return quitRequested_; }
    void setAutoReloadBlocked(bool blocked) { autoReloadBlocked_ = blocked; }

private:
    struct RunStats {
        float elapsedSeconds = 0.0f;
        int defeatedEnemies = 0;
        int dugTiles = 0;
        int acquiredItems = 0;
        int dugTilesSinceMoneyDrop = 0;
        int dugTilesSinceItemDrop = 0;
    };

    struct DungeonLogEntry {
        std::string message;
        std::string label;
        std::string suffix;
        std::string mergeKey;
        int count = 0;
        float age = 0.0f;
        float lifetime = 3.4f;
    };

    struct InventoryCarryState {
        InventorySystem inventory;
        std::array<std::vector<SpellRingItem>, SpellRingCount> ringItemsByRing;
        bool valid = false;
    };

    enum class WorldBuildStep {
        None,
        ResetSimulation,
        ResetUi,
        ResetRun,
        GenerateLayout,
        ResetWarpPoints,
        InitializeMoonFragments,
        InitializeRewards,
        InitializeChests,
        InitializeCrates,
        InitializeEnemies,
        InitializeRing,
        WarmInitialTiles,
        Finalize,
        Done,
    };

    struct WorldBuildJob {
        bool active = false;
        bool useLatestWarpPoint = false;
        InventoryCarryState retainedInventory;
        int retainedLevel = 1;
        int retainedXp = 0;
        int retainedXpToNext = 1;
        float elapsedSeconds = 0.0f;
        WorldBuildStep step = WorldBuildStep::None;
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
        float lightRevealTimer = 0.0f;
        float lightRevealDuration = 0.55f;
        bool lightRevealAnimating = false;
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

    struct MoonFragmentNode {
        DungeonTile tile{};
        PlacementVisibility visibility = PlacementVisibility::Exposed;
        bool collected = false;
    };

    struct ChestNode {
        DungeonTile tile{};
        PlacementVisibility visibility = PlacementVisibility::Exposed;
        LootChestKind chestKind = LootChestKind::Common;
        int depthRank = 1;
        bool revealed = false;
        bool opened = false;
    };

    struct CrateNode {
        DungeonTile tile{};
        int depthRank = 1;
        bool destroyed = false;
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
        std::vector<MoonFragmentNode> moonFragmentNodes;
        std::vector<ChestNode> chestNodes;
        std::vector<CrateNode> crateNodes;
        std::vector<EnemyNode> enemyNodes;
        EnemySystem enemies;
        WorldDropSystem worldDrops;
        int spawnedWarpPointCount = 0;
        int unlockedWarpPointCount = 0;
        Vec2 bossSpawnPoint{};
        bool hasBossSpawnPoint = false;
        bool bossSpawned = false;
        bool valid = false;
    };

    struct DungeonState {
        bool valid = false;
        int currentStage = 0;
        std::string currentStageId;
        TileMap tileMap;
        DungeonLayout dungeonLayout;
        RunStats runStats{};
        std::vector<WarpPoint> warpPoints;
        std::vector<RewardNode> rewardNodes;
        std::vector<MoneyNode> moneyNodes;
        std::vector<MoonFragmentNode> moonFragmentNodes;
        std::vector<ChestNode> chestNodes;
        std::vector<CrateNode> crateNodes;
        std::vector<EnemyNode> enemyNodes;
        EnemySystem enemies;
        WorldDropSystem worldDrops;
        int spawnedWarpPointCount = 0;
        Vec2 bossSpawnPoint{};
        bool hasBossSpawnPoint = false;
        bool bossSpawned = false;
    };

    enum class StorageEntryKind {
        Stack,
        Instance,
    };

    struct StorageEntry {
        StorageEntryKind kind = StorageEntryKind::Stack;
        int index = 0;
    };
    struct FootstepDustPuff {
        Vec2 startPosition{};
        Vec2 endPosition{};
        float age = 0.0f;
        float lifetime = 0.0f;
        int shapeIndex = 0;
        bool active = false;
    };
    struct RingEquipFx {
        Vec2 sourceScreen{};
        int ringIndex = 0;
        int itemIndex = -1;
        float localAngle = 0.0f;
        std::string objectId;
        std::string instanceId;
        float age = 0.0f;
        float duration = 0.36f;
        float arcSign = 1.0f;
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
    void resetWorldSimulationState();
    void resetWorldPlayerState();
    void resetWorldMapAndRingState();
    void resetWorldActionSystems();
    void resetWorldEffectState();
    void resetWorldEnemyState();
    void resetWorldProjectileState();
    void resetWorldDropState();
    void resetWorldProgressionState();
    void resetWorldInventoryState();
    void resetWorldUiState();
    void resetWorldRunState();
    void buildWorldForRun(bool captureRunStartInventory);
    void beginWorldBuildFromBase(
        bool useLatestWarpPoint,
        InventoryCarryState retainedInventory,
        int retainedLevel,
        int retainedXp,
        int retainedXpToNext);
    void updateWorldBuild(float dt);
    void advanceWorldBuildOneStep();
    void finishWorldBuild();
    bool worldBuildActive() const { return worldBuildJob_.active; }
    int worldBuildStepIndex() const;
    int worldBuildStepCount() const;
    float worldBuildProgress() const;
    std::string worldBuildStatusText() const;
    void enterBase();
    void startMiningFromBase(bool useLatestWarpPoint, bool forceRegenerate = false);
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
        int quantity = 0;
    };
    enum class MerchantUiMode {
        Closed,
        ChooseAction,
        Buy,
        Sell,
    };
    std::vector<SellableEntry> sellableObjects() const;
    bool isSellableObject(const ItemData& item) const;
    bool isStoryObject(const ItemData& item) const;
    int sellPrice(const ItemData& item, const ItemInstance* instance = nullptr) const;
    bool merchantProductCanFit(const ItemData* item) const;
    bool canBuyMerchantProduct(const MerchantProduct& product) const;
    void refreshMerchantStock(bool force);
    void buyMerchantProduct(int index);
    void sellMerchantEntry(int index, int count);
    void sellMerchantScreenSlot(int slotIndex, int count);
    std::vector<StorageEntry> processingEntries() const;
    std::optional<StorageEntry> processingEntryForScreenSlot(int slotIndex) const;
    const char* processingModeName(ProcessingMode mode) const;
    bool processingEntryAvailable(StorageEntry entry) const;
    bool processingScreenSlotAvailable(int slotIndex) const;
    int processingMoneyCost(StorageEntry entry, ProcessingMode mode) const;
    int processingOreCost(StorageEntry entry, ProcessingMode mode) const;
    void applyProcessing(int entryIndex);
    void applyProcessingScreenSlot(int slotIndex);
    void applyProcessingEntry(StorageEntry entry);
    int warehouseCapacity() const;
    int warehouseUsedSlots() const;
    int backpackUsedSlots() const;
    std::vector<StorageEntry> backpackStorageEntries() const;
    std::vector<StorageEntry> warehouseStorageEntries() const;
    void syncWarehouseDisplaySlots() const;
    int warehouseEntryIndexAtStorageSlot(int slot) const;
    void assignWarehouseEntryToStorageSlot(int entryIndex, int slot);
    void removeWarehouseDisplaySlotAtEntryIndex(int entryIndex);
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
    void updateScreenMode(
        const Input& input,
        UiContext& ui,
        float dt,
        std::vector<EffectDiscoveryEvent>* discoveryEvents);
    void updateBaseScreen(const Input& input, UiContext& ui, float dt);
    void updatePauseMenu(const Input& input, UiContext& ui);
    void choosePauseMenuItem(int item);
    void leavePausePage();
    void openRingScreen();
    void updateRingScreen(const Input& input, UiContext& ui, float dt);
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
    void captureDungeonState();
    bool restoreDungeonState(bool useLatestWarpPoint);
    bool canRegenerateCurrentStage() const;
    std::size_t retainedWorldDropCountForCurrentStage() const;
    void initializeWarpPointsFromLayout();
    int discoveredWarpPointCount() const;
    std::vector<WarpPoint> discoveredWarpPoints() const;
    int nearestWarpPointIndex(Vec2 position) const;
    void updateWarpPoints(float dt);
    void initializeMoonFragmentNodesFromWarpPoints();
    void initializeRewardNodesFromLayout();
    void updateExposedRewardNodes();
    void revealRewardNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    void updateExposedMoonFragmentNodes();
    void revealMoonFragmentNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    void initializeChestNodesFromLayout();
    void updateChestNodes(const Input& input);
    void revealChestNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    void openChestNode(ChestNode& node);
    void initializeCrateNodesFromLayout();
    void updateCrateNodes();
    void destroyCrateNode(CrateNode& node);
    bool spawnWeightedObjectLoot(LootChestKind chestKind, int depthRank, Vec2 center, std::mt19937& rng, std::string_view sourceLabel);
    int rewardNodeCount() const;
    int moneyNodeCount() const;
    int buriedVisibleNodeCount() const;
    int buriedHiddenNodeCount() const;
    void initializeEnemyNodesFromLayout();
    void updateExposedEnemyNodes();
    void updateRingEffectDiscoveries(std::vector<EffectDiscoveryEvent>& discoveryEvents);
    std::vector<Vec2> spawnHiddenEnemyNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    int exposedEnemyNodeCount() const;
    int buriedEnemyNodeCount() const;
    int spawnedEnemyNodeCount() const;
    void configureBossSpawnPointFromWarp(Vec2 warpPosition);
    void updateBossSpawn();
    void captureRetrySnapshotAtWarpPoint();
    void restoreRetrySnapshot();
    void renderWarpPoints(Renderer& renderer) const;
    void appendRewardNodeRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const std::vector<LightSource>& extraLights) const;
    void renderRewardNodes(Renderer& renderer, const std::vector<LightSource>& extraLights) const;
    void enterStageClear();
    void updateStageClearScreen(const Input& input, UiContext& ui);
    void resetPlayerFootstepDust();
    void updatePlayerFootstepDust(float dt);
    void maybeSpawnPlayerFootstepDust(Vec2 footAnchor, Vec2 movementDirection, bool walking, int frameIndex, int& previousFrame);
    void spawnPlayerFootstepDust(Vec2 footAnchor, Vec2 movementDirection);
    void renderPlayerFootstepDust(Renderer& renderer) const;
    void spawnRingEquipFx(const RingEquipFxRequest& request);
    void updateRingEquipFx(float dt);
    Vec2 ringEquipFxTargetScreen(const RingEquipFx& fx) const;
    void renderRingEquipFx(Renderer& renderer) const;
    bool loadSaveData();
    bool saveSaveData(std::string& message) const;
    void loadBaseEditData();
    bool saveBaseEditData(std::string& message);
    bool handleBaseEditCommand(std::string_view normalized);
    bool loadObjectImageScaleData();
    bool saveObjectImageScaleData(std::string& message);
    bool handleObjectImageScaleCommand(std::string_view normalized);
    void rebuildObjectImageScaleList();
    void enterObjectImageScaleEditMode();
    void exitObjectImageScaleEditMode();
    void updateObjectImageScaleEditScreen(const Input& input, UiContext& ui);
    void renderObjectImageScaleEditScreen(Renderer& renderer) const;
    void enterEnemyTestMode();
    void exitEnemyTestToBase();
    void spawnSelectedEnemyTestEnemy();
    void clearEnemyTestArena();
    void updateEnemyTestUi(const Input& input, UiContext& ui);
    void renderEnemyTestUi(Renderer& renderer) const;
    void enterBaseEditMode();
    void exitBaseEditMode();
    void resetBaseEditDragState();
    void pushBaseEditUndoSnapshot();
    bool undoBaseEdit();
    bool redoBaseEdit();
    bool isBasePassabilityBlocked(BaseArea area, int tileX, int tileY) const;
    void setBasePassabilityBlocked(BaseArea area, int tileX, int tileY, bool blocked);
    BaseEditRect baseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect fallback) const;
    void setBaseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect rect);
    void updateBaseEditScreen(const Input& input, UiContext& ui, float dt);
    void renderBaseEditOverlay(Renderer& renderer) const;
    bool gameProgressPaused() const;
    bool basePresentationActive() const;
    void pushDungeonLog(std::string message, std::string mergeKey = {});
    void pushCountedDungeonLog(std::string label, int amount, std::string suffix, std::string mergeKey);
    void updateDungeonLogs(float dt);
    void appendPickupLogs(const std::vector<WorldDropPickupEvent>& pickupEvents);
    void switchActiveRingWithLog(int delta);
    int unlockedRingHudCount() const;
    UiRect ringStatusHudRect(int ringIndex, int unlockedRingCount) const;
    void updateRingStatusHud(UiContext& ui);
    std::string currentMapDisplayName() const;
    void renderTopInfoBar(Renderer& renderer) const;
    void renderBaseBackdrop(Renderer& renderer) const;
    void renderBaseScreen(Renderer& renderer) const;
    void renderBookshelfScreen(Renderer& renderer) const;
    void renderPauseMenu(Renderer& renderer) const;
    void renderRingScreen(Renderer& renderer, float totalTime) const;
    void renderRingStatusHud(Renderer& renderer) const;
    void renderDungeonStatusHud(Renderer& renderer) const;
    void renderDungeonLogs(Renderer& renderer) const;
    void renderWorldLoadingScreen(Renderer& renderer, float totalSeconds) const;
    void renderGameOverScreen(Renderer& renderer) const;
    void renderStageClearScreen(Renderer& renderer) const;
    void renderBaseDebugOverlay(Renderer& renderer, const Time& time) const;
    void renderDebugOverlay(Renderer& renderer, const Time& time);
    void renderSpellRingForeground(
        Renderer& renderer,
        const std::vector<const SpellRingItem*>& runtimeItems,
        const std::vector<LightSource>& itemLights,
        float totalSeconds) const;

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
    bool baseStorageFocusWarehouse_ = false;
    int baseStorageBackpackCursor_ = 0;
    int baseStorageWarehouseCursor_ = 0;
    int baseStorageWarehousePage_ = 0;
    UiCommandMenuState baseStorageCommandMenu_{};
    int baseStorageCommandSlot_ = -1;
    int baseStoragePointerPressSlot_ = -1;
    Vec2 baseStoragePointerPressMouse_{};
    bool baseStoragePointerPressCanOpenMenu_ = false;
    bool baseStoragePointerDragTriggered_ = false;
    bool baseStorageGrabbedActive_ = false;
    int baseStorageGrabbedFromSlot_ = -1;
    bool baseSellActive_ = false;
    MerchantUiMode baseMerchantMode_ = MerchantUiMode::Closed;
    int baseMerchantActionSelection_ = 0;
    int baseSellSelection_ = 0;
    int baseMerchantBuySelection_ = 0;
    UiCommandMenuState baseMerchantSellCommandMenu_{};
    int baseMerchantSellCommandIndex_ = -1;
    UiCommandMenuState baseMerchantBuyCommandMenu_{};
    int baseMerchantBuyCommandIndex_ = -1;
    bool baseUpgradeActive_ = false;
    int baseUpgradeSelection_ = 0;
    bool baseProcessingActive_ = false;
    int baseProcessingMode_ = 0;
    int baseProcessingSelection_ = 0;
    UiCommandMenuState baseProcessingCommandMenu_{};
    int baseProcessingCommandSlot_ = -1;
    bool baseRingWorkshopActive_ = false;
    int baseRingWorkshopSelection_ = 0;
    int ringWorkshopDraftRadiusPoints_ = 0;
    int ringWorkshopDraftSpeedPoints_ = 0;
    bool baseBookshelfActive_ = false;
    BookshelfPage bookshelfPage_ = BookshelfPage::Menu;
    int bookshelfSelection_ = 0;
    bool baseEditEnabled_ = false;
    BaseEditMode baseEditMode_ = BaseEditMode::None;
    std::unordered_map<std::string, BaseEditRect> baseFacilityRectsOutdoor_;
    std::unordered_map<std::string, BaseEditRect> baseFacilityRectsHome_;
    std::unordered_set<std::int64_t> baseBlockedTilesOutdoor_;
    std::unordered_set<std::int64_t> baseBlockedTilesHome_;
    std::vector<BaseEditSnapshot> baseEditUndoStack_;
    std::vector<BaseEditSnapshot> baseEditRedoStack_;
    int baseEditSelectedFacilityIndex_ = -1;
    bool baseEditDraggingFacilityMove_ = false;
    bool baseEditDraggingFacilityResize_ = false;
    int baseEditResizeMask_ = 0;
    Vec2 baseEditDragStartMouse_{};
    BaseEditRect baseEditDragStartRect_{};
    bool baseEditPassPaintActive_ = false;
    bool baseEditPassPaintSetBlocked_ = false;
    int baseEditPassPaintLastTileX_ = std::numeric_limits<int>::min();
    int baseEditPassPaintLastTileY_ = std::numeric_limits<int>::min();
    bool baseEditDirty_ = false;
    std::unordered_map<std::string, float> objectImageScaleById_;
    std::unordered_map<std::string, float> otherImageScaleByKey_;
    std::vector<std::string> objectImageScaleObjectIds_;
    std::vector<std::string> otherImageScaleKeys_;
    ScreenMode objectImageScaleReturnMode_ = ScreenMode::Playing;
    ImageScaleEditTab imageScaleEditTab_ = ImageScaleEditTab::Objects;
    int objectImageScaleSelectedIndex_ = -1;
    int otherImageScaleSelectedIndex_ = -1;
    float objectImageScaleScrollOffset_ = 0.0f;
    float otherImageScaleScrollOffset_ = 0.0f;
    bool objectImageScaleDirty_ = false;
    std::string objectImageScaleStatus_;
    bool enemyTestActive_ = false;
    bool enemyTestUiVisible_ = true;
    UiDropdownState enemyTestDropdown_{};
    int enemyTestSelectedIndex_ = 0;
    std::string enemyTestStatus_;
    std::string baseStatus_;
    PauseMenuPage pausePage_ = PauseMenuPage::Main;
    ScreenMode pauseReturnMode_ = ScreenMode::Playing;
    int pauseMenuSelection_ = 0;
    int pauseConfirmSelection_ = 1;
    mutable UiCancelControlState pauseCancelState_{};
    mutable UiCancelControlState baseCancelState_{};
    mutable UiCancelControlState ringCancelState_{};
    int ringSlotSelection_ = 0;
    bool ringGrabActive_ = false;
    int ringGrabOrigin_ = -1;
    SpellRingItem ringGrabbedItem_{};
    bool ringDragPending_ = false;
    bool ringDragActive_ = false;
    bool ringSnapActive_ = false;
    int ringDragItemIndex_ = -1;
    float ringDragOriginalAngle_ = 0.0f;
    float ringDragDisplayAngle_ = 0.0f;
    float ringSnapStartAngle_ = 0.0f;
    float ringSnapTargetAngle_ = 0.0f;
    float ringSnapElapsed_ = 0.0f;
    Vec2 ringDragStartMouse_{};
    std::string ringStatus_;
    std::vector<DungeonLogEntry> dungeonLogs_;
    WorldBuildJob worldBuildJob_;
    std::array<FootstepDustPuff, 10> playerFootstepDustPuffs_{};
    std::vector<RingEquipFx> ringEquipFx_;
    int nextPlayerFootstepDustPuff_ = 0;
    int nextPlayerFootstepDustShape_ = 0;
    int previousPlayerDustFrame_ = -1;
    int previousBasePlayerDustFrame_ = -1;
    RunStats runStats_{};
    int gameOverSelection_ = 0;
    std::string gameOverStatus_;
    bool bossSpawned_ = false;
    int stageClearSelection_ = 0;
    std::string stageClearStatus_;
    InventoryCarryState runStartInventoryState_{};
    RetrySnapshot retrySnapshot_{};
    std::unordered_map<std::string, DungeonState> dungeonStates_;
    std::vector<WarpPoint> warpPoints_;
    std::vector<RewardNode> rewardNodes_;
    std::vector<MoneyNode> moneyNodes_;
    std::vector<MoonFragmentNode> moonFragmentNodes_;
    std::vector<ChestNode> chestNodes_;
    std::vector<CrateNode> crateNodes_;
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
    bool baseRegenerateConfirmActive_ = false;
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
    mutable std::vector<int> warehouseDisplaySlots_;
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
    bool autoReloadBlocked_ = false;
    float captureCooldown_ = 0.0f;
    float ringTrailEffectTimer_ = 0.0f;
    float ambientParticleTimer_ = 0.0f;
    float reloadNoticeTimer_ = 0.0f;
    std::string reloadNotice_;
};

}
