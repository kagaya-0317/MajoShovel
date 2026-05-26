#pragma once

#include "engine/Camera.hpp"
#include "engine/FileWatcher.hpp"
#include "engine/Input.hpp"
#include "engine/InputRemapCapture.hpp"
#include "engine/RendererTypes.hpp"
#include "engine/Settings.hpp"
#include "engine/Time.hpp"
#include "engine/Ui.hpp"
#include "data/GoogleSheetSource.hpp"
#include "data/EnemyCatalog.hpp"
#include "data/ObjectCatalog.hpp"
#include "data/RuntimeBalance.hpp"
#include "data/StageCatalog.hpp"
#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "game/DebugOverlay.hpp"
#include "game/Collision.hpp"
#include "game/DepthRender.hpp"
#include "game/DialogueSystem.hpp"
#include "game/DiggingSystem.hpp"
#include "game/DungeonEventDefinition.hpp"
#include "game/DungeonLayout.hpp"
#include "game/EffectDispatcher.hpp"
#include "game/EffectSystem.hpp"
#include "game/EnemySystem.hpp"
#include "game/EncyclopediaSystem.hpp"
#include "game/GroundLineSystem.hpp"
#include "game/GameTestAction.hpp"
#include "game/GameTestProbe.hpp"
#include "game/InventorySystem.hpp"
#include "game/InventoryUiCommon.hpp"
#include "game/Kamishibai.hpp"
#include "game/LevelSystem.hpp"
#include "game/MagicFxSystem.hpp"
#include "game/MagicSystem.hpp"
#include "game/OpeningMetaSave.hpp"
#include "game/RingLevelUpgrade.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/StoryEvent.hpp"
#include "game/Player.hpp"
#include "game/ProjectileSystem.hpp"
#include "game/TileMap.hpp"
#include "game/UpgradeSystem.hpp"
#include "game/WorldDropSystem.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
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
class Renderer;
class AudioEngine;

enum class ScreenMode {
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
    AstralResult
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

enum class AudioCueEditMode {
    Bgm,
    Se,
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

struct AudioCueEditEntry {
    std::string id;
    std::string displayName;
    std::string type;
    std::string path;
    float volume = 1.0f;
    bool loop = false;
    float cooldownMs = 0.0f;
};

struct AudioCueFileEntry {
    std::string name;
    std::string relativePath;
    std::uintmax_t fileSize = 0;
};

class Game {
public:
    void initialize(int width, int height);
    void setAudioEngine(AudioEngine* audio);
    void setSettingsAccessors(
        std::function<GameSettings()> getter,
        std::function<void(const GameSettings&)> applier);
    void setInputBindingAccessors(
        std::function<InputBindingMap()> getter,
        std::function<void(const InputBindingMap&)> applier);
    bool handleEvent(const SDL_Event& event);
    void resize(int width, int height);
    void update(const Input& input, const Time& time);
    void render(Renderer& renderer, const Time& time);
    bool executeDebugCommand(std::string_view command);
    GameTestSnapshot makeTestSnapshot(GameTestSnapshotOptions options = {}) const;
    GameTestActionResult applyTestAction(const GameTestAction& action);
    void setAutoSimulationIntentOverlay(bool active, std::vector<autosim::AutoSimulationIntent> history);
    void setAutoSimulationDebugOverlay(bool active, autosim::AutoSimulationDebugSnapshot debug);
    bool quitRequested() const { return quitRequested_; }
    void setAutoReloadBlocked(bool blocked);
    void setHotReloadEnabled(bool enabled);

    using DungeonEventKind = majo::DungeonEventKind;

    struct DungeonEventNestHole {
        DungeonTile tile{};
        int hp = 16;
        int maxHp = 16;
        bool destroyed = false;
        bool rewardSpawned = false;
        float spawnCooldown = 0.0f;
        float hitCooldown = 0.0f;
        std::vector<int> spawnedEnemyRuntimeIds;
    };

    enum class DungeonEventObjectKind {
        GlowingRock,
        ElectricReceiver,
        BuriedDebris,
        LostBaggage,
        Campfire,
        HeavyRock,
    };

    struct DungeonEventObject {
        DungeonEventObjectKind kind = DungeonEventObjectKind::GlowingRock;
        DungeonTile tile{};
        int hp = 10;
        int maxHp = 10;
        bool destroyed = false;
        bool powered = false;
        float hitCooldown = 0.0f;
    };

    struct DungeonEventInstance {
        std::string id;
        DungeonEventKind kind = DungeonEventKind::SleepingEnemyTreasure;
        DungeonTile centerTile{};
        DungeonTile focusTile{};
        DungeonTile rewardTile{};
        float discoveryRadiusTiles = 5.0f;
        bool discovered = false;
        bool completed = false;
        bool encounterSpawned = false;
        bool activated = false;
        bool rewardSpawned = false;
        bool bossDefeated = false;
        bool npcRequestKnown = false;
        bool objectiveResolved = false;
        bool rewardClaimed = false;
        int bossEnemyRuntimeId = 0;
        float selfLightRadiusTiles = 4.0f;
        std::string selectedEnemyId;
        std::vector<int> spawnedEnemyRuntimeIds;
        std::vector<DungeonEventNestHole> nestHoles;
        std::vector<DungeonEventObject> eventObjects;
        float cavityRadiusTiles = 0.0f;
        std::string requestKey;
        std::string deliveredObjectId;
        int guideTargetWarpPointIndex = -1;
        float guideRemainingSeconds = 0.0f;
        std::vector<std::string> spawnedEntityIds;
        std::string params;
        std::string data;
    };

    struct DungeonFocusRequest {
        std::string eventKind;
        Vec2 focusWorldPos{};
        std::string discoveryStoryEventId;
        DialogueSequence discoveryDialogue;
        float holdSecondsIfNoDialogue = 2.0f;
        float moveSeconds = 0.0f;
        float returnSeconds = 0.0f;
        std::function<void()> onComplete;
    };
    bool requestDungeonFocus(DungeonFocusRequest request);

private:
    struct RunStats {
        float elapsedSeconds = 0.0f;
        int defeatedEnemies = 0;
        int dugTiles = 0;
        int acquiredItems = 0;
        int acquiredObjectItems = 0;
        int dugTilesSinceMoneyDrop = 0;
        int dugTilesSinceItemDrop = 0;
    };

    struct PlayerRegenSource {
        std::string objectId;
        std::string objectName;
        Vec2 position{};
        double ratePerSecond = 0.0;
    };

    enum class AstralDistortionKind {
        None,
        FadingStarlight,
        StarHardened,
        EchoSpawn,
    };

    enum class AstralRunResult {
        None,
        Returned,
        Died,
        DragonDefeated,
    };

    struct AstralRunState {
        bool active = false;
        AstralDistortionKind distortion = AstralDistortionKind::None;
        int currentDepth = 1;
        int maxReachedDepth = 1;
        int maxDepth = 9;
        int maxReachedDistanceTiles = 0;
        int distortionChanges = 0;
        float baseStageHardnessMultiplier = 1.0f;
    };

    struct AstralRunSummary {
        AstralRunResult result = AstralRunResult::None;
        int reachedDepth = 1;
        int maxDepth = 9;
        int reachedDistanceTiles = 0;
        int defeatedEnemies = 0;
        int dugTiles = 0;
        int acquiredItems = 0;
        int acquiredMaterials = 0;
        int acquiredMoney = 0;
        bool carriedOut = false;
        int score = 0;
        int highScore = 0;
        bool highScoreUpdated = false;
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
        int money = 0;
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

    enum class DebugStoryTestMode {
        Events,
        Tutorials,
    };

    struct DebugStoryTestEntry {
        std::string eventId;
        std::string title;
        std::string trigger;
        std::string onceFlag;
        bool repeatable = false;
        bool alreadySeen = false;
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
        bool lootSpawned = false;
        float openingSeconds = 0.0f;
        bool spawnJumpActive = false;
        Vec2 spawnJumpStartPosition{};
        float spawnJumpElapsedSeconds = 0.0f;
        float spawnJumpDurationSeconds = 0.0f;
        float spawnJumpArcHeight = 0.0f;
    };

    struct CrateNode {
        DungeonTile tile{};
        int depthRank = 1;
        bool destroyed = false;
    };

    enum class LootSourceKind {
        Chest,
        CrateBonus,
        DigItem,
        EnemyDrop,
        CapturedReward,
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

    struct DungeonEventSystem {
        void clear();
        void setInstances(std::vector<DungeonEventInstance> instances);
        [[nodiscard]] const std::vector<DungeonEventInstance>& all() const;
        [[nodiscard]] std::vector<DungeonEventInstance>& mutableAll();
        [[nodiscard]] bool empty() const;
        [[nodiscard]] std::size_t size() const;
        void generateFromLayout(
            const DungeonLayout& layout,
            const std::vector<WarpPoint>& warpPoints,
            bool warpPointsEnabled,
            std::string_view stageId);
        void appendLightSources(std::vector<LightSource>& lights, double totalSeconds) const;
        [[nodiscard]] DungeonEventInstance* firstDiscoverable(Vec2 playerPosition, float tileSize);
        [[nodiscard]] DungeonEventInstance* findById(std::string_view id);
        [[nodiscard]] const DungeonEventInstance* nearest(Vec2 playerPosition) const;

        std::vector<DungeonEventInstance> instances;
    };

    enum class DungeonEventRewardKind {
        ChestDrop,
        MultiChestDrop,
        MaterialDrop,
        MoneyDrop,
    };

    struct DungeonEventRewardRequest {
        DungeonEventRewardKind kind = DungeonEventRewardKind::MaterialDrop;
        LootChestKind chestKind = LootChestKind::Common;
        int count = 1;
        MaterialType materialType = MaterialType::EnhancementOre;
        int amount = 1;
    };

    struct DungeonMinimapCell {
        TileType type = TileType::Empty;
    };

    using DungeonMinimapCells = std::unordered_map<std::int64_t, DungeonMinimapCell>;

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
        DungeonMinimapCells dungeonMinimapCells;
        RunStats runStats{};
        std::vector<WarpPoint> warpPoints;
        std::vector<RewardNode> rewardNodes;
        std::vector<MoneyNode> moneyNodes;
        std::vector<MoonFragmentNode> moonFragmentNodes;
        std::vector<ChestNode> chestNodes;
        std::vector<CrateNode> crateNodes;
        std::vector<EnemyNode> enemyNodes;
        std::vector<DungeonEventInstance> dungeonEventInstances;
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
        DungeonMinimapCells dungeonMinimapCells;
        RunStats runStats{};
        std::vector<WarpPoint> warpPoints;
        std::vector<RewardNode> rewardNodes;
        std::vector<MoneyNode> moneyNodes;
        std::vector<MoonFragmentNode> moonFragmentNodes;
        std::vector<ChestNode> chestNodes;
        std::vector<CrateNode> crateNodes;
        std::vector<EnemyNode> enemyNodes;
        std::vector<DungeonEventInstance> dungeonEventInstances;
        EnemySystem enemies;
        WorldDropSystem worldDrops;
        int spawnedWarpPointCount = 0;
        int unlockedWarpPointCount = 0;
        Vec2 latestWarpPointPosition{};
        bool hasLatestWarpPointPosition = false;
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
        ResetEnhancement,
        Lighten,
        Enlarge,
    };
    enum class BaseItemSource {
        Backpack,
        Warehouse,
        Ring0,
        Ring1,
        Ring2,
    };
    enum class StorageUiMode {
        Closed,
        ChooseAction,
        Deposit,
        Withdraw,
    };
    struct ProcessingTarget {
        BaseItemSource source = BaseItemSource::Backpack;
        int slotIndex = -1;
        StorageEntry backpackEntry{};
        bool warehouseEntry = false;
        int ringIndex = -1;
        int ringItemIndex = -1;
        bool valid = false;
    };
    struct StorageTransferTarget {
        BaseItemSource source = BaseItemSource::Backpack;
        int slotIndex = -1;
        StorageEntry storageEntry{};
        bool warehouseEntry = false;
        int ringIndex = -1;
        int ringItemIndex = -1;
        bool valid = false;
    };
    enum class StorageQuantityOperation {
        None,
        Deposit,
        Withdraw,
    };
    struct StorageQuantityPending {
        StorageQuantityOperation operation = StorageQuantityOperation::None;
        StorageTransferTarget target{};
    };
    enum class RingWorkshopUpgrade {
        InitialRadius,
        InitialSpeed,
        ShiftDistance,
    };
    enum class RingWorkshopMode {
        ChooseAction,
        Respec,
        Upgrade,
    };
    enum class BookshelfPage {
        Menu,
        Items,
        Enemies,
    };
    enum class ScreenTransitionTarget {
        None,
        Base,
        TitleToBase,
        TitleToIntroTutorial,
        MiningStart,
        ReturnToBase,
        IntroTutorialToBase,
        BaseArea,
    };
    enum class IntroTutorialPhase {
        Inactive,
        FallDialogue,
        ShovelRingIntro,
        ExploreToTorch,
        TorchDialogue,
        TorchRingIntro,
        ExploreToEnemy,
        DefeatEnemy,
        EnemyDefeatedDialogue,
        FreeToExit,
        Returning,
    };
    enum class ScreenTransitionPhase {
        Idle,
        CrossFadeCapture,
        CrossFading,
        FadingOut,
        Hold,
        FadingIn,
    };
    enum class ScreenTransitionFadeColor {
        Black,
        White,
    };
    struct ScreenTransitionState {
        ScreenTransitionTarget target = ScreenTransitionTarget::None;
        ScreenTransitionPhase phase = ScreenTransitionPhase::Idle;
        ScreenTransitionFadeColor fadeColor = ScreenTransitionFadeColor::Black;
        float holdSeconds = 0.0f;
        float fadeInSeconds = 0.0f;
        float postTransitionStoryDelaySeconds = 0.0f;
        float elapsed = 0.0f;
        bool applied = false;
        bool useLatestWarpPoint = false;
        bool forceRegenerate = false;
        bool returnStageCleared = false;
        bool returnDied = false;
        BaseArea targetBaseArea = BaseArea::Outdoor;
        Vec2 targetBasePlayerPosition{};
        Vec2 targetBasePlayerFacing{0.0f, 1.0f};
        std::string targetBaseStatus;

        [[nodiscard]] bool active() const { return phase != ScreenTransitionPhase::Idle; }
    };
    enum class DungeonFocusPhase {
        Idle,
        MoveToTarget,
        Hold,
        PlayDiscoveryDialogue,
        ReturnToPlayer,
    };
    struct DungeonFocusState {
        DungeonFocusPhase phase = DungeonFocusPhase::Idle;
        std::string eventKind;
        Vec2 startCameraPos{};
        Vec2 focusWorldPos{};
        Vec2 moveFrom{};
        Vec2 moveTo{};
        float elapsed = 0.0f;
        float duration = 0.0f;
        float holdSeconds = 2.0f;
        float moveSeconds = 0.0f;
        float returnSeconds = 0.0f;
        std::string discoveryStoryEventId;
        DialogueSequence discoveryDialogue;
        std::function<void()> onComplete;
    };
    enum class BossEncounterPhase {
        None,
        WaitingBeforeDialogue,
        Fighting,
        DefeatPresentation,
        WaitingAfterDialogue,
    };
    struct BossEncounterState {
        BossEncounterPhase phase = BossEncounterPhase::None;
        std::string stageId;
        std::string bossEnemyId;
        Vec2 spawnPoint{};
        Vec2 defeatPosition{};
        float timer = 0.0f;
        bool finalBoss = false;
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
    void playAudioBgm(std::string_view id, float fadeSeconds = 0.0f, bool restart = false);
    void playAudioSe(std::string_view id, float volumeScale = 1.0f);
    void playUiSoundEvents(const UiContext& ui);
    void enterBase();
    void placeBasePlayerAtMineExitReturnPoint();
    void placeBasePlayerAtHomeDoorResumePoint();
    void startMiningFromBase(bool useLatestWarpPoint, bool forceRegenerate = false);
    void loadOpeningKamishibaiData();
    void loadEndingKamishibaiData();
    void loadStoryEvents();
    void startOpeningKamishibai();
    void finishOpeningKamishibai(bool completedPlayback);
    void updateOpeningKamishibai(const Input& input, float dt);
    void startEndingKamishibai();
    void finishEndingKamishibai(bool completedPlayback);
    void updateEndingKamishibai(const Input& input, float dt);
    void updateTitleScreen(const Input& input, UiContext& ui);
    void requestScreenTransition(ScreenTransitionTarget target);
    void requestMiningStartTransition(bool useLatestWarpPoint, bool forceRegenerate);
    void requestReturnToBaseTransition(bool stageCleared, bool died);
    void requestBaseAreaCrossfade(BaseArea targetArea, Vec2 playerPosition, Vec2 playerFacing, std::string status);
    void startScreenTransition(ScreenTransitionTarget target, ScreenTransitionPhase phase);
    static ScreenTransitionFadeColor fadeColorForScreenTransitionTarget(ScreenTransitionTarget target);
    static float holdSecondsForScreenTransitionTarget(ScreenTransitionTarget target);
    static float fadeInSecondsForScreenTransitionTarget(ScreenTransitionTarget target);
    static float postTransitionStoryDelaySecondsForScreenTransitionTarget(ScreenTransitionTarget target);
    void updateScreenTransition(float dt);
    void applyScreenTransitionTarget(ScreenTransitionTarget target);
    void updatePendingStoryTriggerDelay(float dt);
    void queuePendingStoryTriggerIfReady();
    void applyPermanentUpgrades();
    LevelGainResult gainPlayerXp(int amount);
    void openLevelUpChoice(ScreenMode returnMode);
    void updateLevelUpScreen(const Input& input, UiContext& ui, float dt);
    bool applyLevelUpSelection(RingLevelUpgradeSelection selection);
    void refreshEquipmentModifiers();
    float effectiveInitialRingRadiusForRing(int ringIndex, int levelRadiusPoints) const;
    float effectiveInitialRingSpeedForRing(int ringIndex, int levelSpeedPoints) const;
    float effectiveInitialRingWeightLimitForRing(int ringIndex, int levelWeightLimitPoints) const;
    float effectiveRingShiftDistance() const;
    float effectiveCollectionPullRadius(int collectionLevel) const;
    void configureWatcher();
    void checkHotReload(float dt);
    void loadSheetSourceConfig();
    bool loadBalanceFromSources(std::string& message);
    bool loadBalanceFromDisk(std::string& message);
    bool loadObjectsFromSheet();
    bool loadStagesFromSheet();
    bool loadEnemiesFromSheet();
    const StageDefinition& currentStageDefinition() const;
    std::vector<StageDefinition> selectableStageDefinitionsForCurrentUnlockState() const;
    int stageCatalogIndexForId(std::string_view stageId) const;
    void clampCurrentStageToSelectableStages();
    void syncWarpStateForCurrentStage();
    void applyDebugStageUnlockState(int unlockedStoryStages);
    void resolveCurrentStageDefinition();
    void refreshOrbitEffects();
    void updatePlayerRegen(float dt, std::vector<EffectDiscoveryEvent>& discoveryEvents);
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
    struct FirstItemAcquisitionNotice {
        std::string objectId;
        std::string instanceId;
        std::string statusText;
        bool protectable = false;
    };
    struct MerchantSellTarget {
        BaseItemSource source = BaseItemSource::Backpack;
        int slotIndex = -1;
        StorageEntry storageEntry{};
        bool warehouseEntry = false;
        int ringIndex = -1;
        int ringItemIndex = -1;
        bool valid = false;
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
    int sellPrice(const ItemData& item, const SpellRingItem* ringItem) const;
    bool isHighValueBuyObject(const ItemData& item) const;
    bool merchantProductCanFit(const ItemData* item) const;
    bool canBuyMerchantProduct(const MerchantProduct& product) const;
    void refreshHighValueBuyObjects(bool force);
    void refreshMerchantStock(bool force);
    void buyMerchantProduct(int index);
    void sellMerchantEntry(int index, int count);
    MerchantSellTarget merchantSellTargetForSourceSlot(int source, int slotIndex) const;
    MerchantSellTarget merchantSellTargetForScreenSlot(int slotIndex) const;
    bool merchantSellTargetAvailable(MerchantSellTarget target) const;
    int merchantSellTargetPrice(MerchantSellTarget target) const;
    void sellMerchantTarget(MerchantSellTarget target, int count);
    void sellMerchantScreenSlot(int slotIndex, int count);
    std::vector<StorageEntry> processingEntries() const;
    std::optional<StorageEntry> processingEntryForScreenSlot(int slotIndex) const;
    std::optional<StorageEntry> warehouseEntryForPageSlot(int slotIndex, int page) const;
    std::optional<StorageEntry> warehouseEntryForPageSlot(int slotIndex, int page, int slotsPerPage) const;
    InventoryUiEntryView storageEntryView(StorageEntry entry, bool warehouseEntry) const;
    ProcessingTarget processingTargetForScreenSlot(int slotIndex) const;
    const char* processingModeName(ProcessingMode mode) const;
    const char* processingActionName(ProcessingMode mode) const;
    bool processingModeUnlocked(ProcessingMode mode) const;
    bool processingEntryAvailable(StorageEntry entry, ProcessingMode mode, bool warehouseEntry = false) const;
    bool processingEntryAvailable(StorageEntry entry, bool warehouseEntry = false) const;
    bool processingScreenSlotAvailable(int slotIndex) const;
    bool processingTargetAvailable(ProcessingTarget target, ProcessingMode mode) const;
    bool processingTargetAvailable(ProcessingTarget target) const;
    bool processingTargetHasAvailableCommand(ProcessingTarget target) const;
    bool processingCommandExecutable(ProcessingTarget target, ProcessingMode mode) const;
    int processingMoneyCost(StorageEntry entry, ProcessingMode mode, bool warehouseEntry = false) const;
    int processingOreCost(StorageEntry entry, ProcessingMode mode, bool warehouseEntry = false) const;
    int processingMoneyCost(ProcessingTarget target, ProcessingMode mode) const;
    int processingOreCost(ProcessingTarget target, ProcessingMode mode) const;
    std::vector<UiCommandMenuItem> processingCommandItems(ProcessingTarget target) const;
    void openProcessingConfirm(ProcessingTarget target, ProcessingMode mode);
    void drawProcessingConfirmDialog(Renderer& renderer, UiRect panel) const;
    void applyProcessing(int entryIndex);
    void applyProcessingScreenSlot(int slotIndex);
    void applyProcessingEntry(StorageEntry entry, ProcessingMode mode, bool warehouseEntry = false);
    void applyProcessingEntry(StorageEntry entry, bool warehouseEntry = false);
    void applyProcessingTarget(ProcessingTarget target, ProcessingMode mode);
    void applyProcessingTarget(ProcessingTarget target);
    int warehouseCapacity() const;
    int warehouseUsedSlots() const;
    int backpackUsedSlots() const;
    std::vector<StorageEntry> backpackStorageEntries() const;
    std::vector<StorageEntry> warehouseStorageEntries() const;
    void syncWarehouseDisplaySlots() const;
    void sortWarehouseByCatalogOrder();
    int warehouseEntryIndexAtStorageSlot(int slot) const;
    void assignWarehouseEntryToStorageSlot(int entryIndex, int slot);
    void removeWarehouseDisplaySlotAtEntryIndex(int entryIndex);
    StorageTransferTarget storageDepositTargetForSourceSlot(int source, int slotIndex) const;
    StorageTransferTarget storageDepositTargetForScreenSlot(int slotIndex) const;
    StorageTransferTarget storageWithdrawTargetForSlot(int slotIndex) const;
    bool storageTransferTargetAvailable(StorageTransferTarget target) const;
    bool storageTransferTargetIsStack(StorageTransferTarget target) const;
    int storageTransferTargetStackCount(StorageTransferTarget target) const;
    InventoryUiEntryView storageTransferTargetView(StorageTransferTarget target) const;
    void depositStorageTarget(StorageTransferTarget target, int count);
    void withdrawStorageTarget(StorageTransferTarget target, int count);
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
    bool adjustRingWorkshopRespec(RingLevelUpgradeSelection from, RingLevelUpgradeSelection to);
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
    void captureEncyclopediaSyncSuppressState();
    void applyEffectDiscoveries(const std::vector<EffectDiscoveryEvent>& discoveries);
    void recordObjectObtainedForFirstNotice(
        std::string_view objectId,
        std::string_view instanceId,
        bool protectable,
        Vec2 position);
    bool firstItemAcquisitionNoticeActive() const;
    void closeFirstItemAcquisitionNotice();
    void queueIntroTutorialChestLootDialogueIfReady();
    void updateFirstItemAcquisitionNotice(const Input& input, UiContext& ui);
    void addStoryFlag(std::string flag);
    void updateBookshelfScreen(const Input& input, UiContext& ui);
    void updateScreenMode(
        const Input& input,
        UiContext& ui,
        float dt,
        std::vector<EffectDiscoveryEvent>* discoveryEvents);
    void updateBaseScreen(const Input& input, UiContext& ui, float dt);
    void updateBasePlayerSpriteAnimation(float dt, bool walking);
    void updatePauseMenu(const Input& input, UiContext& ui);
    void choosePauseMenuItem(int item);
    void leavePausePage();
    void openOptionsMenu();
    void loadOptionsSettings();
    void applyOptionsSettings(std::string status);
    void updateOptionsMenu(const Input& input, UiContext& ui);
    void updateOperationSettings(const Input& input, UiContext& ui);
    void updateAudioSettings(const Input& input, UiContext& ui);
    void updateVideoSettings(const Input& input, UiContext& ui);
    bool handleOperationSettingsEvent(const SDL_Event& event);
    void renderOptionsMenu(Renderer& renderer) const;
    void renderOperationSettings(Renderer& renderer) const;
    void renderAudioSettings(Renderer& renderer) const;
    void renderVideoSettings(Renderer& renderer) const;
    void queueOperationSettingsBinding(InputAction action, int column, const InputBinding& binding);
    void applyOperationSettingsBinding(InputAction action, int column, const InputBinding& binding, bool removeConflicts);
    void clearOperationSettingsBinding(InputAction action, int column);
    void resetOperationSettingsAction(InputAction action);
    void resetOperationSettingsCategory();
    void resetOperationSettingsAll();
    void openRingScreen();
    void updateRingScreen(const Input& input, UiContext& ui, float dt);
    void cancelRingGrab();
    void enterGameOver();
    void updateGameOverScreen(const Input& input, UiContext& ui);
    void retryAfterGameOver();
    void returnToBaseAfterGameOver();
    void startIntroTutorialDungeon();
    bool updateIntroTutorial(const Input& input, float dt);
    void equipIntroTutorialStartingTools();
    void addIntroTutorialTorchToRing();
    void startIntroTutorialEnemyEncounterEvent();
    void startIntroTutorialSlimeFocusDialogue();
    void startIntroTutorialEnemyRetreatDialogue();
    void spawnIntroTutorialChest();
    void spawnIntroTutorialSecondChest();
    void unlockIntroTutorialFreeRoute();
    void completeIntroTutorialAndReturnToBase();
    void syncIntroTutorialTerrainDamageLocks();
    bool introTutorialActive() const;
    Vec2 introTutorialExitPosition() const;
    std::vector<LightSource> introTutorialLightSources(double totalSeconds) const;
    void enterAstralResult(AstralRunResult result);
    void updateAstralResultScreen(const Input& input, UiContext& ui);
    void returnToBaseAfterAstralResult();
    AstralRunSummary makeAstralRunSummary(AstralRunResult result) const;
    int calculateAstralRunScore(const AstralRunSummary& summary) const;
    int astralRunMaterialDeltaFromStart() const;
    int astralRunMoneyDeltaFromStart() const;
    bool shouldRefreshMerchantOnReturn(bool stageCleared, bool died) const;
    void returnToBaseFromNormalStage(bool stageCleared, bool died);
    InventoryCarryState captureInventoryCarryState() const;
    void restoreInventoryCarryState(const InventoryCarryState& state);
    void captureRunStartInventoryState();
    void clearTemporaryPlayerState(bool fullHeal);
    Vec2 latestWarpPointStartPosition() const;
    Vec2 warpPointStartPositionForCurrentRequest() const;
    std::vector<WarpPoint> selectableWarpPointsForCurrentStageStart() const;
    void rebuildUnlockedWarpPointsForStart(Vec2 latestPosition);
    void resetWarpPointRunState();
    void captureDungeonState();
    bool restoreDungeonState(bool useLatestWarpPoint);
    bool canRegenerateCurrentStage() const;
    bool currentStageIsRoguelike() const;
    bool currentStageCleared() const;
    bool stageCleared(std::string_view stageId) const;
    std::string stageClearFlagForStage(std::string_view stageId) const;
    std::string currentStageBossCaptureObjectId() const;
    bool hasCapturedBossForCurrentStage() const;
    std::size_t retainedWorldDropCountForCurrentStage() const;
    void initializeWarpPointsFromLayout();
    int discoveredWarpPointCount() const;
    std::vector<WarpPoint> discoveredWarpPoints() const;
    int nearestWarpPointIndex(Vec2 position) const;
    Vec2 safePlayerStartPosition(Vec2 preferredPosition);
    Vec2 dungeonEntrancePosition() const;
    int nearbyDiscoveredWarpPointIndex() const;
    bool updateWarpReturnUi(const Input& input, UiContext& ui);
    bool unlockAllWarpPointsForCurrentDungeon();
    void updateWarpPoints(float dt);
    void initializeMoonFragmentNodesFromWarpPoints();
    static std::int64_t dungeonMinimapKey(int tx, int ty);
    static DungeonTile dungeonMinimapTileFromKey(std::int64_t key);
    void resetDungeonMinimap();
    void setDungeonMinimapTile(int tx, int ty, TileType type);
    bool dungeonMinimapTileSeen(int tx, int ty) const;
    void revealDungeonMinimapAround(Vec2 center, float radius);
    void revealDungeonMinimapOpenedTiles(const std::vector<Vec2>& openedTiles);
    void updateDungeonMinimap(double totalSeconds);
    std::vector<LightSource> collectDungeonLightSources(double totalSeconds) const;
    static const char* dungeonEventKindId(DungeonEventKind kind);
    static bool dungeonEventKindFromId(std::string_view id, DungeonEventKind& outKind);
    static const char* dungeonEventKindDisplayName(DungeonEventKind kind);
    static std::string dungeonEventDiscoverySeenFlag(DungeonEventKind kind);
    static std::string dungeonEventDiscoveryStoryEventId(DungeonEventKind kind);
    void initializeDungeonEventInstancesFromLayout();
    void updateDungeonEvents(float dt, double totalSeconds);
    void handleDungeonEventEnemyEvent(const EnemyEvent& enemyEvent);
    bool updateDungeonEventNpcInteraction(const Input& input, UiContext& ui);
    std::string dungeonEventNpcPromptText() const;
    bool updateDungeonEventDiscovery(float dt);
    void appendDungeonEventRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const std::vector<LightSource>& extraLights,
        double totalSeconds) const;
    bool spawnDungeonEventReward(DungeonEventInstance& event, const DungeonEventRewardRequest& request);
    void completeDungeonEvent(DungeonEventInstance& event, std::optional<DungeonEventRewardRequest> reward);
    bool ensureDungeonEventChest(DungeonEventInstance& event, DungeonTile tile, LootChestKind chestKind);
    void requestDungeonRewardChestFocus(Vec2 focusWorldPos);
    void applyDungeonEventCavity(const DungeonEventInstance& event);
    bool debugRequestDungeonEventPlacement(DungeonEventKind kind);
    bool debugPlaceDungeonEvent(DungeonEventKind kind);
    void flushPendingDebugDungeonEventPlacement();
    std::string nearestDungeonEventDebugText() const;
    void resetDungeonFocus();
    bool updateDungeonFocus(float dt);
    bool dungeonFocusActive() const;
    static const char* dungeonFocusPhaseName(DungeonFocusPhase phase);
    std::string dungeonFocusDebugText() const;
    void resetAstralRunState();
    void initializeAstralRunForLayout();
    void updateAstralRunProgress();
    void applyAstralDistortionToLayout();
    AstralDistortionKind chooseAstralDistortionForDepth(int depth, AstralDistortionKind previous) const;
    float astralLightRadiusMultiplier() const;
    float astralHardnessMultiplier() const;
    RuntimeBalance runtimeBalanceForDungeon() const;
    bool astralRunActive() const;
    void initializeRewardNodesFromLayout();
    void updateExposedRewardNodes();
    void revealRewardNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    void updateExposedMoonFragmentNodes();
    void revealMoonFragmentNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    void normalizeOpenBuriedPlacementNodes();
    void initializeChestNodesFromLayout();
    void updateChestNodes(float dt, const Input& input);
    void revealChestNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    bool spawnAppearingChestNode(
        DungeonTile tile,
        LootChestKind chestKind,
        int depthRank,
        Vec2 sourceWorldPosition,
        std::string_view logMergeKey = {});
    void startChestSpawnJump(ChestNode& node, Vec2 sourceWorldPosition, std::mt19937& rng);
    void updateChestSpawnJump(ChestNode& node, float dt);
    Vec2 chestVisualCenter(const ChestNode& node) const;
    float chestVisualAltitude(const ChestNode& node) const;
    void openChestNode(ChestNode& node);
    void spawnChestLoot(ChestNode& node);
    void initializeCrateNodesFromLayout();
    void updateCrateNodes();
    void destroyCrateNode(CrateNode& node);
    std::vector<CollisionRect> solidObjectCollisionRects() const;
    bool spawnWeightedObjectLoot(
        LootChestKind chestKind,
        int depthRank,
        Vec2 center,
        std::mt19937& rng,
        std::string_view sourceLabel,
        bool launchFromCenter = false,
        LootSourceKind sourceKind = LootSourceKind::Chest);
    Vec2 safeLootLandingPosition(Vec2 center, std::mt19937& rng);
    void spawnInventoryDiscardRequests(std::vector<InventoryDiscardRequest> requests);
    void updateDigToolFailsafe(float dt);
    bool hasUsableDigToolOnRing() const;
    bool hasUsableDigToolInInventory() const;
    bool hasNearbyUsableDigToolDrop(float radius) const;
    bool trySpawnFailsafeShovelDropFromWall(Vec2 wallCenter);
    bool spawnFailsafeShovelDropFromWall(Vec2 wallCenter);
    int rewardNodeCount() const;
    int moneyNodeCount() const;
    int buriedVisibleNodeCount() const;
    int buriedHiddenNodeCount() const;
    void initializeEnemyNodesFromLayout();
    void clearKnownWarpPointTerrain();
    void applyPlacementTerrainOverrides();
    void updateExposedEnemyNodes();
    void updateRingEffectDiscoveries(std::vector<EffectDiscoveryEvent>& discoveryEvents);
    void updateOrbitAreaEffects(float dt, std::vector<EffectDiscoveryEvent>& discoveryEvents);
    void updateOrbitGroundEffects(float dt, std::vector<EffectDiscoveryEvent>& discoveryEvents);
    std::vector<Vec2> spawnHiddenEnemyNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    int exposedEnemyNodeCount() const;
    int buriedEnemyNodeCount() const;
    int spawnedEnemyNodeCount() const;
    void configureBossSpawnPointFromWarp(Vec2 warpPosition);
    void updateBossSpawn();
    void resetBossEncounter();
    bool beginBossFightForCurrentEncounter();
    void beginBossDefeatSequence(Vec2 position);
    bool updateBossEncounterFlow(float dt);
    void finishBossEncounterAfterDialogue();
    bool bossEncounterBlocksProgress() const;
    float bossDefeatPresentationProgress() const;
    void captureRetrySnapshotAtWarpPoint();
    void restoreRetrySnapshot();
    void renderDungeonEntrance(Renderer& renderer) const;
    void renderWarpPoints(Renderer& renderer) const;
    void appendRewardNodeRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const std::vector<LightSource>& extraLights) const;
    void renderRewardNodes(Renderer& renderer, const std::vector<LightSource>& extraLights) const;
    int unlockedRingCount() const;
    void setUnlockedRingCount(int count);
    void clampActiveRingToUnlocked();
    bool unlockRingsForCurrentStageClear();
    void markCurrentStageCleared();
    void enterStageClear();
    void beginFinalBossEndingSequence();
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
    void initializeDefaultSpellRing();
    void observeRingItemInstanceIds();
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
    bool loadAudioCueManifestForEdit();
    bool saveAudioCueManifestFromEdit(std::string& message);
    bool handleAudioCueEditCommand(std::string_view normalized);
    void rebuildAudioCueFileList();
    void enterAudioCueEditMode(AudioCueEditMode editMode);
    void exitAudioCueEditMode();
    void previewSelectedAudioCueFile();
    void applySelectedAudioCueFile();
    void updateAudioCueEditScreen(const Input& input, UiContext& ui);
    void renderAudioCueEditScreen(Renderer& renderer) const;
    bool handleDebugItemPickerCommand(std::string_view normalized);
    void rebuildDebugItemPickerList();
    void openDebugItemPicker();
    void closeDebugItemPicker();
    void addSelectedDebugItem();
    void updateDebugItemPicker(const Input& input, UiContext& ui);
    void renderDebugItemPicker(Renderer& renderer) const;
    bool handleDebugStoryTestCommand(std::string_view normalized);
    void rebuildDebugStoryTestList();
    void openDebugStoryTest(DebugStoryTestMode mode);
    void closeDebugStoryTest();
    void playSelectedDebugStoryTest();
    void updateDebugStoryTest(const Input& input, UiContext& ui);
    void renderDebugStoryTest(Renderer& renderer) const;
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
    bool dungeonEventUiSuppressed() const;
    void updatePausedDungeonPresentation(float dt);
    bool basePresentationActive() const;
    void startBaseMonicaDialogue();
    bool hasBrokenRingItemForDeparture() const;
    void openBaseMiningStartChoice();
    void maybeQueueStageStartStory();
    bool hasStoryFlag(std::string_view flag) const;
    const StoryEvent* findStoryEvent(std::string_view id) const;
    const StoryEvent* findStoryEventForTrigger(std::string_view trigger) const;
    std::string currentStageStoryTrigger(std::string_view triggerName) const;
    bool queueStoryEventForTrigger(std::string trigger);
    bool queueStoryEventForCurrentStage(std::string_view triggerName);
    void updateQueuedStoryEvents();
    bool pendingStoryTriggerDelayActive() const;
    bool startStoryEvent(std::string_view id);
    bool startStoryEventWithCompletion(std::string_view id, std::function<void()> onComplete);
    bool startDialogueSequenceWithCompletion(DialogueSequence sequence, std::function<void()> onComplete);
    void updateDialoguePlayerIdleAnimation(float dt);
    void runDialogueCompletionCallbackIfFinished(bool dialogueWasActive);
    bool startStoryEventForDebug(std::string_view id);
    bool startStoryEventForTrigger(std::string_view trigger);
    void maybeStartOpeningBaseIntroEvent();
    void pushDungeonLog(std::string message, std::string mergeKey = {});
    void pushCountedDungeonLog(std::string label, int amount, std::string suffix, std::string mergeKey);
    void updateDungeonLogs(float dt);
    void appendPickupLogs(const std::vector<WorldDropPickupEvent>& pickupEvents);
    void handleRingItemBreakEvents(std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr);
    void switchActiveRingWithLog(int delta);
    int unlockedRingHudCount() const;
    UiRect ringStatusHudRect(int ringIndex, int unlockedRingCount) const;
    void updateRingStatusHud(UiContext& ui);
    std::string currentMapDisplayName() const;
    void renderTopInfoBar(Renderer& renderer) const;
    void renderOpeningKamishibai(Renderer& renderer) const;
    void renderEndingKamishibai(Renderer& renderer) const;
    void renderTitleScreen(Renderer& renderer) const;
    void renderScreenTransitionOverlay(Renderer& renderer);
    void renderBaseBackdrop(Renderer& renderer) const;
    void renderBaseScreen(Renderer& renderer) const;
    void renderBookshelfScreen(Renderer& renderer) const;
    void renderLevelUpOverlay(Renderer& renderer);
    void renderPauseMenu(Renderer& renderer) const;
    void renderRingScreen(Renderer& renderer, float totalTime) const;
    void renderRingStatusHud(Renderer& renderer) const;
    void renderFirstItemAcquisitionNotice(Renderer& renderer) const;
    void renderDungeonMinimap(Renderer& renderer, const std::vector<LightSource>& itemLights) const;
    void renderDungeonStatusHud(Renderer& renderer) const;
    void renderDungeonLogs(Renderer& renderer) const;
    void renderDungeonControlHelp(Renderer& renderer) const;
    void renderWarpReturnUi(Renderer& renderer) const;
    void renderWorldLoadingScreen(Renderer& renderer, float totalSeconds) const;
    void renderGameOverScreen(Renderer& renderer) const;
    void renderStageClearScreen(Renderer& renderer) const;
    void renderAstralResultScreen(Renderer& renderer) const;
    void renderBossDefeatPresentation(Renderer& renderer) const;
    void renderBaseDebugOverlay(Renderer& renderer, const Time& time) const;
    void renderDebugOverlay(Renderer& renderer, const Time& time);
    void renderAutoSimulationIntentOverlay(Renderer& renderer) const;
    void beginDungeonRingIntro();
    void updateDungeonRingIntro(float dt);
    bool dungeonRingIntroActive() const;
    float dungeonRingIntroProgress() const;
    void renderSpellRingForeground(
        Renderer& renderer,
        const std::vector<const SpellRingItem*>& runtimeItems,
        const std::vector<LightSource>& itemLights,
        float totalSeconds) const;

    Camera camera_;
    DungeonFocusState dungeonFocus_;
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
    EquipmentModifiers equipmentModifiers_;
    DiggingSystem digging_;
    EffectDispatcher effectDispatcher_;
    EffectSystem effects_;
    EnemySystem enemies_;
    ProjectileSystem projectiles_;
    MagicSystem magic_;
    MagicFxSystem magicFx_;
    GroundLineSystem groundLines_;
    InventorySystem inventory_;
    WorldDropSystem worldDrops_;
    EncyclopediaSystem encyclopedia_;
    std::deque<FirstItemAcquisitionNotice> firstItemAcquisitionNotices_;
    std::unordered_map<std::string, int> encyclopediaOwnedSyncSuppressCounts_;
    std::unordered_map<std::string, int> encyclopediaRingSyncSuppressCounts_;
    LevelSystem levels_;
    UpgradeSystem upgrades_;
    UiResultDialogState levelUpResultDialog_{};
    ScreenMode levelUpReturnMode_ = ScreenMode::Playing;
    DialoguePlayer dialogue_;
    std::function<void()> pendingDialogueCompletion_;
    DebugOverlay debug_;
    std::string observedEquippedStaffInstanceId_;
    std::string equipmentModifierLogKey_;
    OpeningMetaSave openingMetaSave_;
    OpeningMetaData openingMeta_;
    std::vector<KamishibaiPage> openingPages_;
    std::vector<KamishibaiPage> endingPages_;
    KamishibaiPlayer openingPlayer_;
    KamishibaiPlayer endingPlayer_;
    KamishibaiRenderer openingRenderer_;
    std::vector<StoryEvent> storyEvents_;
    int storyEventsRevision_ = 0;
    std::string pendingStoryTrigger_;
    float pendingStoryTriggerDelaySeconds_ = 0.0f;
    std::vector<std::string> pendingStoryTriggers_;
    ScreenTransitionState screenTransition_;
    FrameSnapshot screenTransitionSnapshot_;
    ScreenMode mode_ = ScreenMode::Base;
    BaseArea baseArea_ = BaseArea::Outdoor;
    Vec2 basePlayerPosition_{640.0f, 360.0f};
    Vec2 baseOutdoorPlayerPosition_{640.0f, 360.0f};
    Vec2 basePlayerFacing_{0.0f, 1.0f};
    float basePlayerSpriteAnimationTime_ = 0.0f;
    float baseRingPreviewAnimationTime_ = 0.0f;
    bool basePlayerSpriteWalking_ = false;
    int baseMenuSelection_ = 0;
    bool baseMiningStartChoiceActive_ = false;
    int baseMiningStartSelection_ = 0;
    UiConfirmDialogState baseBrokenRingDepartureConfirm_{};
    bool baseWarpPointSelectActive_ = false;
    int baseWarpPointSelection_ = 0;
    bool baseStorageActive_ = false;
    StorageUiMode baseStorageMode_ = StorageUiMode::Closed;
    int baseStorageActionSelection_ = 0;
    int baseStorageDepositSource_ = 0;
    UiTabsState baseStorageDepositSourceTabs_{};
    int baseStorageDepositSelection_ = 0;
    int baseStorageWithdrawSelection_ = 0;
    int baseStorageWarehousePage_ = 0;
    UiQuantityDialogState baseStorageQuantityDialog_{};
    StorageQuantityPending baseStorageQuantityPending_{};
    UiCommandMenuState baseStorageCommandMenu_{};
    StorageQuantityOperation baseStorageCommandOperation_ = StorageQuantityOperation::None;
    StorageTransferTarget baseStorageCommandTarget_{};
    StorageQuantityOperation baseStoragePointerOperation_ = StorageQuantityOperation::None;
    StorageTransferTarget baseStoragePointerTarget_{};
    Vec2 baseStoragePointerPressMouse_{};
    bool baseStoragePointerPressCanOpenMenu_ = false;
    bool baseStoragePointerDragTriggered_ = false;
    bool baseSellActive_ = false;
    MerchantUiMode baseMerchantMode_ = MerchantUiMode::Closed;
    int baseMerchantActionSelection_ = 0;
    int baseMerchantSellSource_ = 0;
    UiTabsState baseMerchantSellSourceTabs_{};
    int baseSellSelection_ = 0;
    int baseMerchantBuySelection_ = 0;
    UiCommandMenuState baseMerchantSellCommandMenu_{};
    int baseMerchantSellCommandSource_ = 0;
    int baseMerchantSellCommandIndex_ = -1;
    UiCommandMenuState baseMerchantBuyCommandMenu_{};
    int baseMerchantBuyCommandIndex_ = -1;
    bool baseUpgradeActive_ = false;
    int baseUpgradeSelection_ = 0;
    UiTabsState baseUpgradeTabs_{};
    UiResultDialogState baseResultDialog_{};
    UiConfirmDialogState baseRegenerateConfirm_{};
    bool baseProcessingActive_ = false;
    int baseProcessingMode_ = 0;
    UiTabsState baseProcessingTabs_{};
    int baseProcessingSource_ = 0;
    UiTabsState baseProcessingSourceTabs_{};
    int baseProcessingSelection_ = 0;
    UiCommandMenuState baseProcessingCommandMenu_{};
    int baseProcessingCommandSlot_ = -1;
    UiConfirmDialogState baseProcessingConfirm_{};
    ProcessingTarget baseProcessingConfirmTarget_{};
    ProcessingMode baseProcessingConfirmMode_ = ProcessingMode::Repair;
    bool baseRingWorkshopActive_ = false;
    RingWorkshopMode baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
    int baseRingWorkshopSelection_ = 0;
    int baseRingWorkshopRingIndex_ = 0;
    UiTabsState baseRingWorkshopRingTabs_{};
    std::optional<RingLevelUpgradeSelection> ringWorkshopRespecSource_;
    RingLevelUpgradePointTable ringWorkshopDraftUpgradePoints_{};
    bool baseBookshelfActive_ = false;
    BookshelfPage bookshelfPage_ = BookshelfPage::Menu;
    int bookshelfSelection_ = 0;
    float bookshelfScrollOffset_ = 0.0f;
    UiScrollAreaState bookshelfScrollState_{};
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
    ScreenMode audioCueEditReturnMode_ = ScreenMode::Playing;
    AudioCueEditMode audioCueEditMode_ = AudioCueEditMode::Bgm;
    std::vector<AudioCueEditEntry> audioCueEditEntries_;
    std::vector<AudioCueFileEntry> audioCueEditFiles_;
    int audioCueEditCueIndex_ = -1;
    int audioCueEditFileIndex_ = -1;
    float audioCueEditCueScrollOffset_ = 0.0f;
    float audioCueEditFileScrollOffset_ = 0.0f;
    UiScrollAreaState audioCueEditCueScrollState_{};
    UiScrollAreaState audioCueEditFileScrollState_{};
    bool audioCueEditDirty_ = false;
    std::string audioCueEditStatus_;
    std::string audioCueEditPreviousBgmCue_;
    mutable UiCancelControlState audioCueEditCancelState_{};
    bool debugItemPickerActive_ = false;
    std::vector<std::string> debugItemPickerObjectIds_;
    int debugItemPickerSelectedIndex_ = -1;
    float debugItemPickerScrollOffset_ = 0.0f;
    std::string debugItemPickerStatus_;
    mutable UiCancelControlState debugItemPickerCancelState_{};
    bool debugStoryTestActive_ = false;
    DebugStoryTestMode debugStoryTestMode_ = DebugStoryTestMode::Events;
    std::vector<DebugStoryTestEntry> debugStoryTestEntries_;
    int debugStoryTestSelectedIndex_ = -1;
    float debugStoryTestScrollOffset_ = 0.0f;
    UiScrollAreaState debugStoryTestScrollState_{};
    std::string debugStoryTestStatus_;
    int debugStoryTestLoadedRevision_ = -1;
    bool debugStoryTestReturnAfterDialogue_ = false;
    mutable UiCancelControlState debugStoryTestCancelState_{};
    std::function<GameSettings()> settingsGetter_;
    std::function<void(const GameSettings&)> settingsApplier_;
    std::function<InputBindingMap()> inputBindingGetter_;
    std::function<void(const InputBindingMap&)> inputBindingApplier_;
    bool enemyTestActive_ = false;
    bool enemyTestUiVisible_ = true;
    UiDropdownState enemyTestDropdown_{};
    int enemyTestSelectedIndex_ = 0;
    std::string enemyTestStatus_;
    bool autoSimulationIntentOverlayActive_ = false;
    std::vector<autosim::AutoSimulationIntent> autoSimulationIntentHistory_;
    bool autoSimulationDebugOverlayActive_ = false;
    autosim::AutoSimulationDebugSnapshot autoSimulationDebug_;
    std::string baseStatus_;
    PauseMenuPage pausePage_ = PauseMenuPage::Main;
    ScreenMode pauseReturnMode_ = ScreenMode::Playing;
    int pauseMenuSelection_ = 0;
    UiConfirmDialogState pauseQuitConfirm_{};
    mutable UiCancelControlState pauseCancelState_{};
    UiTabsState optionsTabs_{};
    GameSettings optionsSettings_{};
    int optionsPage_ = 0;
    int audioSettingsSelection_ = 0;
    int videoSettingsSelection_ = 0;
    bool optionsSettingsLoaded_ = false;
    bool optionsSuppressCancelThisFrame_ = false;
    std::string optionsStatus_;
    UiTabsState operationSettingsTabs_{};
    UiSelectableTableState operationSettingsTable_{};
    UiConfirmDialogState operationSettingsConflictConfirm_{};
    UiConfirmDialogState operationSettingsResetAllConfirm_{};
    InputRemapCapture operationSettingsCapture_{};
    InputBindingMap operationSettingsBindings_ = defaultInputBindings();
    std::vector<InputAction> operationSettingsConflictActions_;
    InputBinding operationSettingsPendingBinding_{};
    InputAction operationSettingsPendingAction_ = InputAction::Count;
    int operationSettingsPendingColumn_ = 0;
    int operationSettingsCategory_ = 0;
    bool operationSettingsLoaded_ = false;
    std::string operationSettingsStatus_;
    mutable UiCancelControlState baseCancelState_{};
    mutable UiCancelControlState ringCancelState_{};
    UiTabsState ringTabs_{};
    UiCommandMenuState ringCommandMenu_{};
    int ringCommandItemIndex_ = -1;
    bool ringCommandPlaceActive_ = false;
    float ringCommandPlaceAngle_ = 0.0f;
    bool ringPlaceModeActive_ = false;
    int ringPlaceSelection_ = 0;
    float ringPlaceTargetAngle_ = 0.0f;
    bool ringEmptyPressActive_ = false;
    Vec2 ringEmptyPressMouse_{};
    float ringEmptyPressAngle_ = 0.0f;
    int ringSlotSelection_ = 0;
    bool ringDetailShowsRing_ = true;
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
    float digToolFailsafeSpawnCooldown_ = 0.0f;
    float dungeonRingIntroTimer_ = 0.0f;
    bool dungeonRingIntroStartPending_ = false;
    bool stageStartStoryPendingAfterRingIntro_ = false;
    bool endingKamishibaiPending_ = false;
    IntroTutorialPhase introTutorialPhase_ = IntroTutorialPhase::Inactive;
    bool introTutorialLightTutorialQueued_ = false;
    bool introTutorialFirstEnemySpawned_ = false;
    bool introTutorialSecondEnemySpawned_ = false;
    bool introTutorialSecondEnemyEncountered_ = false;
    bool introTutorialEnemyEncounterQueued_ = false;
    bool introTutorialEnemyDefeatedQueued_ = false;
    bool introTutorialChestFoundQueued_ = false;
    bool introTutorialSecondChestPlaced_ = false;
    bool introTutorialChestOpened_ = false;
    bool introTutorialChestLootPending_ = false;
    bool introTutorialChestLootDialogueQueued_ = false;
    bool introTutorialMidwayDialogueQueued_ = false;
    bool introTutorialExitDialogueQueued_ = false;
    int introTutorialFirstEnemyRuntimeId_ = 0;
    int introTutorialSecondEnemyRuntimeId_ = 0;
    std::string introTutorialChestLootObjectId_;
    std::string introTutorialChestLootInstanceId_;
    DungeonTile introTutorialFirstEnemyTile_{};
    DungeonTile introTutorialSecondEnemyTile_{};
    DungeonTile introTutorialChestTile_{};
    DungeonTile introTutorialSecondChestTile_{};
    DungeonTile introTutorialExitTile_{};
    std::vector<DungeonLogEntry> dungeonLogs_;
    WorldBuildJob worldBuildJob_;
    std::array<FootstepDustPuff, 10> playerFootstepDustPuffs_{};
    std::vector<RingEquipFx> ringEquipFx_;
    int nextPlayerFootstepDustPuff_ = 0;
    int nextPlayerFootstepDustShape_ = 0;
    int previousPlayerDustFrame_ = -1;
    int previousBasePlayerDustFrame_ = -1;
    RunStats runStats_{};
    double playerRegenPerSecond_ = 0.0;
    double playerRegenAccumulator_ = 0.0;
    std::vector<PlayerRegenSource> playerRegenSources_;
    int gameOverSelection_ = 0;
    std::string gameOverStatus_;
    bool bossSpawned_ = false;
    BossEncounterState bossEncounter_{};
    int stageClearSelection_ = 0;
    std::string stageClearStatus_;
    AstralRunState astralRun_{};
    AstralRunSummary astralResult_{};
    int astralResultSelection_ = 0;
    int astralHighScore_ = 0;
    int debugAstralDepth_ = 1;
    std::string debugAstralMoveTarget_ = "depth";
    std::string debugAstralDistortionMode_ = "auto";
    std::string debugAstralRoomType_ = "ore";
    int debugAstralRoomIndex_ = 1;
    std::string debugAstralResultKind_ = "returned";
    bool debugAstralStatOverride_ = false;
    int debugAstralStatKills_ = 0;
    int debugAstralStatDugTiles_ = 0;
    int debugAstralStatItems_ = 0;
    int debugAstralStatMaterials_ = 0;
    int debugAstralStatMoney_ = 0;
    InventoryCarryState runStartInventoryState_{};
    RetrySnapshot retrySnapshot_{};
    std::unordered_map<std::string, DungeonState> dungeonStates_;
    DungeonMinimapCells dungeonMinimapCells_;
    double dungeonMinimapLastRevealSeconds_ = -1.0e9;
    int dungeonMinimapLastPlayerTileX_ = std::numeric_limits<int>::min();
    int dungeonMinimapLastPlayerTileY_ = std::numeric_limits<int>::min();
    std::vector<WarpPoint> warpPoints_;
    std::vector<RewardNode> rewardNodes_;
    std::vector<MoneyNode> moneyNodes_;
    std::vector<MoonFragmentNode> moonFragmentNodes_;
    std::vector<ChestNode> chestNodes_;
    std::vector<CrateNode> crateNodes_;
    std::vector<EnemyNode> enemyNodes_;
    DungeonEventSystem dungeonEvents_;
    float dungeonEventDiscoveryCooldown_ = 0.0f;
    std::optional<DungeonEventKind> pendingDebugDungeonEventPlacement_;
    int spawnedWarpPointCount_ = 0;
    Vec2 bossSpawnPoint_{};
    bool hasBossSpawnPoint_ = false;
    int currentStage_ = 0;
    std::string currentStageId_ = "stage_01_stardust";
    StageDefinition currentStageDefinition_{};
    int unlockedStages_ = 1;
    int unlockedRingCount_ = 1;
    int unlockedWarpPointCount_ = 0;
    Vec2 latestWarpPointPosition_{};
    bool hasLatestWarpPointPosition_ = false;
    std::optional<Vec2> requestedWarpPointStartPosition_;
    UiConfirmDialogState warpReturnConfirm_{};
    int focusedWarpReturnPointIndex_ = -1;
    int money_ = 0;
    int maxHpUpgradeLevel_ = 0;
    int ringRadiusUpgradeLevel_ = 0;
    int ringSpeedUpgradeLevel_ = 0;
    int collectionRangeUpgradeLevel_ = 0;
    RingLevelUpgradePointTable levelRingUpgradePoints_{};
    int workshopInitialRadiusLevel_ = 0;
    int workshopInitialSpeedLevel_ = 0;
    int workshopShiftDistanceLevel_ = 0;
    bool merchantRefreshPending_ = false;
    int merchantUpgradeLevel_ = 1;
    int merchantStockVersion_ = 0;
    std::vector<MerchantProduct> merchantStock_;
    std::string highValueBuyCategory_;
    std::vector<std::string> highValueBuyObjectIds_;
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
    bool saveDataLoaded_ = false;
    bool debugPaused_ = false;
    bool autoReloadBlocked_ = false;
    bool hotReloadEnabled_ = false;
    float hotReloadPollTimer_ = 0.0f;
    AudioEngine* audio_ = nullptr;
    std::string activeAudioBgmCue_;
    float captureCooldown_ = 0.0f;
    int captureHoverEnemyId_ = 0;
    float ringTrailEffectTimer_ = 0.0f;
    float ambientParticleTimer_ = 0.0f;
    float reloadNoticeTimer_ = 0.0f;
    std::string reloadNotice_;
};

}
