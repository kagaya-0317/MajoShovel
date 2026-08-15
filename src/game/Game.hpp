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
#include "game/EffectPreviewCatalog.hpp"
#include "game/EffectSystem.hpp"
#include "game/EnemySystem.hpp"
#include "game/EncyclopediaSystem.hpp"
#include "game/GroundLineSystem.hpp"
#include "game/GameTestAction.hpp"
#include "game/GameTestProbe.hpp"
#include "game/InventorySystem.hpp"
#include "game/InventoryUiCommon.hpp"
#include "game/ItemCollectionTypes.hpp"
#include "game/ItemGridInteraction.hpp"
#include "game/ItemSlotLayout.hpp"
#include "game/Kamishibai.hpp"
#include "game/LevelSystem.hpp"
#include "game/MagicFxSystem.hpp"
#include "game/MagicSystem.hpp"
#include "game/MoneyGainFxSystem.hpp"
#include "game/OpeningMetaSave.hpp"
#include "game/RingPresetSystem.hpp"
#include "game/RingLevelUpgrade.hpp"
#include "game/PortraitCatalog.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/StoryEvent.hpp"
#include "game/Player.hpp"
#include "game/ProjectileSystem.hpp"
#include "game/TileMap.hpp"
#include "game/UpgradeSystem.hpp"
#include "game/WetGroundSystem.hpp"
#include "game/WorldDropSystem.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
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

enum class DevBuildNoticeState {
    None,
    Building,
    Ready,
    Failed,
};

class UiContext;
class Renderer;
class AudioEngine;

struct PlayerDeathRingItemPresentation {
    SpellRingItem item;
    float dropDelaySeconds = 0.0f;
    bool dropped = false;
};

struct PlayerDeathRingPresentation {
    bool active = false;
    int ringIndex = 0;
    RingShape shape = RingShape::Circle;
    Vec2 center{};
    float orbitRadius = 0.0f;
    float pathPhase = 0.0f;
    float pathAngularSpeed = 0.0f;
    float initialPathAngularSpeed = 0.0f;
    float shapeRotation = 0.0f;
    float shapeRotationSpeed = 0.0f;
    float initialShapeRotationSpeed = 0.0f;
    float stopElapsedSeconds = 0.0f;
    float dropElapsedSeconds = 0.0f;
    std::vector<PlayerDeathRingItemPresentation> items;
};

struct PlayerDeathSequenceState {
    bool active = false;
    float elapsedSeconds = 0.0f;
    float durationSeconds = 1.5f;
    float completionHoldElapsedSeconds = 0.0f;
    bool finalizing = false;
    bool roguelike = false;
    std::array<PlayerDeathRingPresentation, SpellRingCount> ringPresentations;
};

struct DeathResultPreludeState {
    bool active = false;
    float elapsedSeconds = 0.0f;
};

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
    EnemyHitboxEdit,
    EnemyPlacementEdit,
    EnemyShadowEdit,
    AudioCueEdit,
    LevelUp,
    GameOver,
    StageClear,
    AstralResult
};

enum class TitleMenuPage {
    Main,
    Options,
    Credits,
};

enum class EndingKind {
    Main,
    EncyclopediaComplete,
    AstralClear,
    HiddenBad,
    MainFailedTrust,
    MainFailedMonicaMissing,
    EncyclopediaFailedTrust,
    AstralFailedTrust,
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

enum class BaseEditPassabilityLayer {
    Locked,
    Unlocked,
};

enum class ImageScaleEditTab {
    Objects,
    Others,
};

enum class HitboxEditTab {
    Enemies,
    Objects,
    Player,
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
    std::unordered_set<std::int64_t> outdoorBlockedTilesLocked;
    std::unordered_set<std::int64_t> outdoorBlockedTilesUnlocked;
    std::unordered_set<std::int64_t> homeBlockedTilesLocked;
    std::unordered_set<std::int64_t> homeBlockedTilesUnlocked;
};

struct HitboxEditSnapshot {
    HitboxCatalog catalog;
    HitboxEditTab tab = HitboxEditTab::Enemies;
    HitboxDirection enemyDirection = HitboxDirection::Default;
    bool enemyWeakPoint = false;
    std::string selectedId;
    int selectedCircleIndex = -1;

    bool operator==(const HitboxEditSnapshot&) const = default;
};

struct EnemyHitboxDirectionClipboard {
    std::array<std::vector<HitCircle>, HitboxDirectionCount> circles;
    std::array<bool, HitboxDirectionCount> hasProfile{};
};

struct EnemyShadowEditSnapshot {
    EnemyShadowCatalog catalog;
    std::string selectedId;

    bool operator==(const EnemyShadowEditSnapshot&) const = default;
};

struct EnemyPlacementEditSnapshot {
    EnemyPlacementCatalog catalog;
    HitboxDirection direction = HitboxDirection::Default;
    std::string selectedId;

    bool operator==(const EnemyPlacementEditSnapshot&) const = default;
};

struct AudioCueEditEntry {
    std::string id;
    std::string displayName;
    std::string type;
    std::string path;
    float volume = 1.0f;
    float pitch = 0.0f;
    bool loop = false;
    float cooldownMs = 0.0f;

    bool operator==(const AudioCueEditEntry&) const = default;
};

struct AudioCueFileEntry {
    std::string name;
    std::string relativePath;
    std::uintmax_t fileSize = 0;
};

struct PortraitExpressionPickerState {
    bool active = false;
    std::string speakerId;
    std::string speakerName;
    std::string sourcePath;
    int sourceCommandLineNumber = 0;
    bool targetLineValid = false;
    DialogueLine targetLine;
    int originalVariant = 0;
    int selectedVariant = 0;
    bool selectedVariantArmed = false;
    std::vector<PortraitVariant> variants;
    float scrollOffset = 0.0f;
    UiScrollAreaState scrollState{};
    std::string status;
};

class Game {
public:
    void initialize(int width, int height, bool testPlayMode = false);
    void beginInitialize(int width, int height, bool testPlayMode);
    bool advanceInitialize();
    bool initializeActive() const { return initializeJob_.active; }
    bool initializeComplete() const { return initializeJob_.step == InitializeStep::Done; }
    int initializeStepIndex() const;
    int initializeStepCount() const;
    float initializeProgress() const;
    std::string initializeStatusText() const;
    void setAudioEngine(AudioEngine* audio);
    void setSettingsAccessors(
        std::function<GameSettings()> getter,
        std::function<void(const GameSettings&)> applier);
    void setInputBindingAccessors(
        std::function<InputBindingMap()> getter,
        std::function<void(const InputBindingMap&)> applier);
    bool handleEvent(const SDL_Event& event);
    void resize(int width, int height);
    void update(const Input& input, const Time& time, Renderer& renderer);
    void render(Renderer& renderer, const Time& time);
    bool executeDebugCommand(std::string_view command);
    GameTestSnapshot makeTestSnapshot(GameTestSnapshotOptions options = {}) const;
    std::string crashContextSummary() const;
    GameTestActionResult applyTestAction(const GameTestAction& action);
    void setAutoSimulationIntentOverlay(bool active, std::vector<autosim::AutoSimulationIntent> history);
    void setAutoSimulationDebugOverlay(bool active, autosim::AutoSimulationDebugSnapshot debug);
    bool quitRequested() const { return quitRequested_; }
    void handleApplicationQuitRequested();
    void setAutoReloadBlocked(bool blocked);
    void setHotReloadEnabled(bool enabled);
    void setDevBuildNotice(DevBuildNoticeState state, const std::vector<std::string>& changeSummaries)
    {
        devBuildNoticeState_ = state;
        devBuildNoticeChangeSummaries_ = changeSummaries;
    }

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
        std::optional<bool> npcFlipHorizontal;
        bool objectiveResolved = false;
        bool rewardClaimed = false;
        int bossEnemyRuntimeId = 0;
        int encounterSpawnCount = 0;
        float selfLightRadiusTiles = 4.0f;
        std::vector<int> spawnedEnemyRuntimeIds;
        std::vector<DungeonEventNestHole> nestHoles;
        std::vector<DungeonEventObject> eventObjects;
        float cavityRadiusTiles = 0.0f;
        std::string requestKey;
        std::string deliveredObjectId;
        std::string resolvedRewardObjectId;
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
        float holdActionDelaySeconds = 0.0f;
        std::function<void()> onHoldAction;
        std::function<void()> onComplete;
        bool allowDuringBossEncounter = false;
        bool debugStoryReplay = false;
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

    struct AutoSimulationEnemyEncounter {
        std::string enemyId;
        std::string enemyName;
        float startedAtSeconds = 0.0f;
        int playerHitCount = 0;
    };

    struct AutoSimulationCheckpointMeasurementState {
        bool active = false;
        bool completed = false;
        bool gameplayStarted = false;
        std::string stageId;
        std::string stageName;
        std::uint32_t seed = 0;
        int totalWarpPoints = 0;
        int lastObservedAcquiredItems = 0;
        std::vector<std::string> ringLoadout;
        std::vector<std::string> backpackLoadout;
        GameTestCheckpointMeasurementTotals totals;
        std::vector<GameTestCheckpointMeasurementPoint> checkpoints;
        std::unordered_map<int, AutoSimulationEnemyEncounter> encounters;
    };

    struct DiarySaveSummary {
        bool hasSave = false;
        bool storyCleared = false;
        std::string latestStageId;
        std::string latestStageName;
        int discoveredWarpPoints = 0;
        int totalWarpPoints = 0;
        int playerLevel = 1;
        int itemCodexPercent = 0;
        int enemyCodexPercent = 0;
        std::int64_t playTimeSeconds = 0;
    };

    enum class BaseDiaryMode {
        Confirm,
        Saved,
        Error,
    };

    struct PlayerRegenSource {
        std::string objectId;
        std::string objectName;
        Vec2 position{};
        double ratePerSecond = 0.0;
    };

    struct AudioJingleState {
        bool active = false;
        float remainingSeconds = 0.0f;
        float resumeFadeSeconds = 0.25f;
        std::string resumeBgmCue;
    };

    struct LevelUpPresentationState {
        bool active = false;
        float elapsedSeconds = 0.0f;
        float durationSeconds = 1.5f;
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
        Completed,
    };

    struct AstralRunState {
        bool active = false;
        AstralDistortionKind distortion = AstralDistortionKind::None;
        int areaIndex = 0;
        int sectionRankOffset = 0;
        int currentDepthMeters = 0;
        int nextHoleDepthMeters = 500;
        int deepestDepthMeters = 0;
        int completionDepthMeters = 10000;
        int currentDepth = 1;
        int maxReachedDepth = 1;
        int maxDepth = 9;
        int maxReachedDepthMeters = 0;
        int distortionChanges = 0;
        float baseStageHardnessMultiplier = 1.0f;
    };

    struct RoguelikeBigHoleState {
        bool active = false;
        bool unlocked = false;
        Vec2 position{};
        DungeonTile tile{};
        int depthMeters = 500;
    };

public:
    enum class RoguelikeFacilityKind {
        Merchant,
        Artisan,
        Trainer,
    };

    enum class RoguelikeFacilityUiMode {
        None,
        Merchant,
        Artisan,
        Trainer,
    };

    struct RoguelikeFacilityInstance {
        std::string id;
        RoguelikeFacilityKind kind = RoguelikeFacilityKind::Merchant;
        DungeonTile centerTile{};
        DungeonTile npcTile{};
        DungeonTile propTile{};
        Vec2 centerPosition{};
        Vec2 npcPosition{};
        Vec2 propPosition{};
        int depthMeters = 0;
        float lightRadiusTiles = 5.0f;
    };

private:
    struct AstralRunSummary {
        AstralRunResult result = AstralRunResult::None;
        int reachedDepth = 1;
        int maxDepth = 9;
        int reachedDepthMeters = 0;
        int maxDepthMeters = 10000;
        int defeatedEnemies = 0;
        int dugTiles = 0;
        int acquiredItems = 0;
        int acquiredMaterials = 0;
        int acquiredMoney = 0;
        bool carriedOut = false;
        int score = 0;
        int highScore = 0;
        bool highScoreUpdated = false;
        std::string deathCauseText;
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

    struct RoguelikeCarryOutDelta {
        MaterialInventory materials;
        std::vector<InventoryObjectStack> objectStacks;
        std::vector<InventoryObjectInstance> objectInstances;
        int money = 0;
        bool valid = false;
    };

    struct RoguelikeCarryOutMergeResult {
        int objectItems = 0;
        int warehouseItems = 0;
        int skippedItems = 0;
        int materials = 0;
        int money = 0;
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

    enum class InitializeStep {
        None,
        LoadSheetSourceConfig,
        LoadBalance,
        LoadObjects,
        LoadStages,
        ResolveCurrentStage,
        LoadEnemies,
        ConfigureWatcher,
        ResetState,
        InitializeRing,
        LoadSave,
        LoadBaseEdit,
        LoadImageScale,
        LoadHitboxes,
        LoadOpening,
        LoadStoryEvents,
        LoadOpeningMeta,
        EnterInitialScreen,
        Done,
    };

    struct WorldBuildJob {
        bool active = false;
        bool useLatestWarpPoint = false;
        bool restoreRetainedInventory = true;
        InventoryCarryState retainedInventory;
        int retainedLevel = 1;
        int retainedXp = 0;
        int retainedXpToNext = 1;
        float elapsedSeconds = 0.0f;
        WorldBuildStep step = WorldBuildStep::None;
    };

    struct InitializeJob {
        bool active = false;
        bool allowSheetSource = true;
        bool saveDataLoaded = false;
        std::string loadMessage;
        std::string openingMetaMessage;
        InitializeStep step = InitializeStep::None;
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

    struct StoryEventStartOptions {
        bool respectOnceFlag = true;
        bool clearPendingStoryQueues = false;
        bool ignorePlayerDeath = false;
        bool logDebugReplay = false;
        std::function<void()> onComplete;
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
        bool detectorRevealed = false;
        bool spawned = false;
        bool collected = false;
    };

    struct MoneyNode {
        DungeonTile tile{};
        int amount = 1;
        PlacementVisibility visibility = PlacementVisibility::Exposed;
        bool detectorRevealed = false;
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
        bool detectorRevealed = false;
        bool opened = false;
        bool lootSpawned = false;
        float openingSeconds = 0.0f;
        std::string mimicEnemyId;
        bool mimicTriggered = false;
        bool spawnJumpActive = false;
        Vec2 spawnJumpStartPosition{};
        float spawnJumpElapsedSeconds = 0.0f;
        float spawnJumpDurationSeconds = 0.0f;
        float spawnJumpArcHeight = 0.0f;
        float openLockSeconds = 0.0f;
    };

    enum class CratePlacementKind {
        Anchor,
        MicroFeature,
        WallCavity,
    };

    struct CrateNode {
        DungeonTile tile{};
        int depthRank = 1;
        bool destroyed = false;
        CratePlacementKind placementKind = CratePlacementKind::Anchor;
        DungeonTile pathFacingStep{};
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

    struct PendingBuriedEnemySpawn {
        DungeonTile tile{};
        Vec2 position{};
        float timer = 0.0f;
        float duration = 0.0f;
        int depthRank = 1;
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
        ObjectDrop,
        RareObjectDrop,
        MaterialDrop,
        MoneyDrop,
    };

    struct DungeonEventRewardRequest {
        DungeonEventRewardKind kind = DungeonEventRewardKind::MaterialDrop;
        LootChestKind chestKind = LootChestKind::Common;
        std::string objectId;
        int count = 1;
        MaterialType materialType = MaterialType::EnhancementOre;
        int amount = 1;
    };

    struct DungeonEventItemRequestUiState {
        bool open = false;
        std::string eventId;
        int selection = 0;
        int confirmSlot = -1;
        std::string status;
        UiConfirmDialogState confirm{};
    };

    struct DungeonMinimapCell {
        TileType type = TileType::Empty;
    };

    using DungeonMinimapCells = std::unordered_map<std::int64_t, DungeonMinimapCell>;

    struct DungeonRouteDeviationState {
        Vec2 previousPlayerPosition{};
        float movingSecondsBeyondNoticeDistance = 0.0f;
        bool hasPreviousPlayerPosition = false;
        bool offMainRoute = false;
        bool noticeShownForCurrentExcursion = false;
    };

    struct RetrySnapshot {
        Vec2 playerPosition{};
        Vec2 playerFacing{1.0f, 0.0f};
        int playerHp = 10;
        int playerMaxHp = 10;
        int playerLevel = 1;
        int playerXp = 0;
        int playerXpToNext = 12;
        RingLevelUpgradePointTable levelRingUpgradePoints{};
        InventoryCarryState inventory;
        TileMapPersistentState tileMapState;
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
        bool bossPreviewSpawned = false;
        bool valid = false;
    };

    struct DebugRoguelikeRunSnapshot {
        bool valid = false;
        int currentStage = 0;
        std::string currentStageId;
        RetrySnapshot dungeon;
        AstralRunState astralRun{};
        RoguelikeBigHoleState roguelikeBigHole{};
        std::vector<RoguelikeFacilityInstance> roguelikeFacilities;
        std::array<int, 3> roguelikeFacilityLastDepthMeters{};
        InventoryCarryState runStartInventoryState;
        InventoryCarryState roguelikeReturnInventoryState;
        Vec2 latestWarpPointPosition{};
        bool hasLatestWarpPointPosition = false;
        bool roguelikeDungeon = false;
        bool restoreRunStartInventoryOnDeath = false;
        bool roguelikeCarryInRestricted = false;
        bool roguelikeCarryOutRestricted = false;
        bool warpPointsEnabled = false;
    };

    struct DungeonState {
        bool valid = false;
        int currentStage = 0;
        std::string currentStageId;
        TileMapPersistentState tileMapState;
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
        bool bossPreviewSpawned = false;
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
    enum class PlayerFootstepSurface {
        BaseOutdoor,
        HomeInterior,
        Dungeon,
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
    enum class ProcessingUiMode {
        Closed,
        ChooseAction,
        Enhance,
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
        Bulk,
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
    struct BaseItemTarget {
        BaseItemSource source = BaseItemSource::Backpack;
        int slotIndex = -1;
        StorageEntry storageEntry{};
        bool warehouseEntry = false;
        int ringIndex = -1;
        int ringItemIndex = -1;
        bool valid = false;
    };
    enum class BaseRingPreviewKind {
        Storage,
        Processing,
        Merchant,
    };
    enum class BaseRingInteractionMode {
        Manage,
        ActivateOnly,
    };
    struct BaseRingInteractionResult {
        bool consumed = false;
        int activateIndex = -1;
    };
    struct BaseRingItemInteractionState {
        ItemKey item{};
        float originalAngle = 0.0f;
        Vec2 pointerStart{};
        bool keyboardMoveActive = false;
        bool pointerPending = false;
        bool pointerDragging = false;

        bool active() const
        {
            return keyboardMoveActive || pointerPending || pointerDragging;
        }
    };
    using StorageTransferTarget = BaseItemTarget;
    struct BatchItemSelectionState {
        bool active = false;
        std::vector<ItemKey> selectedKeys;
        UiConfirmDialogState confirm{};
    };
    struct StorageBatchTransferSummary {
        int selectedCount = 0;
        int requiredSlots = 0;
        int freeSlots = 0;

        bool fits() const
        {
            return requiredSlots <= freeSlots;
        }
    };
    enum class StorageQuantityOperation {
        None,
        Deposit,
        Withdraw,
    };
    enum class BaseQuantityOperation {
        None,
        StorageDeposit,
        StorageWithdraw,
        MerchantBuy,
        MerchantSell,
    };
    struct BaseQuantityPending {
        BaseQuantityOperation operation = BaseQuantityOperation::None;
        BaseItemTarget target{};
        int merchantProductIndex = -1;
    };
    enum class RingWorkshopUpgrade {
        RadiusAdjust,
        RadiusMax,
        RadiusMin,
        Speed,
        WeightLimit,
        ShiftDistance,
        ThrowDistance,
        ThrowCooldown,
        WeightPenalty,
        EquipSlot,
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
        FinalBossEndingKamishibai,
        BaseArea,
        BossEncounterIntro,
        BossEncounterAfterDialogue,
        GameOverRetry,
        GameOverReturnToBase,
        AstralDeathReturnToBase,
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
    enum class ScreenTransitionSound {
        Generic,
        DungeonLadder,
        WarpPoint,
        Home,
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

        bool closeBaseUi = false;

        [[nodiscard]] bool active() const { return phase != ScreenTransitionPhase::Idle; }
    };
    enum class StoryPhoneSoundKind {
        None,
        Incoming,
        Outgoing,
        Hangup,
    };
    struct StoryPhoneSoundState {
        StoryPhoneSoundKind kind = StoryPhoneSoundKind::None;
        float elapsedSeconds = 0.0f;
        float durationSeconds = 0.0f;
    };
    struct StoryShakeCommandState {
        float elapsedSeconds = 0.0f;
        float durationSeconds = 0.0f;
        bool active = false;
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
        float holdActionDelaySeconds = 0.0f;
        std::string discoveryStoryEventId;
        DialogueSequence discoveryDialogue;
        std::function<void()> onHoldAction;
        std::function<void()> onComplete;
        bool allowDuringBossEncounter = false;
        bool debugStoryReplay = false;
    };
    enum class BossEncounterPhase {
        None,
        IntroTransition,
        WaitingBeforeDialogue,
        Fighting,
        DefeatPresentation,
        WaitingLevelUpAfterDefeat,
        AfterDialogueTransition,
        WaitingAfterDialogue,
    };
    enum class BossEncounterPurpose {
        StageClear,
        RoguelikeGate,
        HiddenMonicaDuel,
    };
    struct BossEncounterState {
        BossEncounterPhase phase = BossEncounterPhase::None;
        BossEncounterPurpose purpose = BossEncounterPurpose::StageClear;
        std::string stageId;
        std::string bossEnemyId;
        Vec2 spawnPoint{};
        Vec2 defeatPosition{};
        float timer = 0.0f;
        float returnToBaseAfterDialogueDelay = 0.0f;
        bool finalBoss = false;
        bool bossSpawnPresentationPlayed = false;
        bool returnToBaseAfterDialogue = false;
    };
    enum class DungeonStoryPresentationKind {
        None,
        BossAfterIdle,
        BossAfterDefeat,
        SmallMoleEscape,
        BossExplodeEscape,
        CameraFocus,
    };
    struct DungeonStoryPresentationState {
        DungeonStoryPresentationKind kind = DungeonStoryPresentationKind::None;
        Vec2 position{};
        Vec2 startPosition{};
        Vec2 targetPosition{};
        std::string bossEnemyId;
        std::string spriteKey;
        float elapsedSeconds = 0.0f;
        float durationSeconds = 0.0f;
        bool started = false;
        bool effectTriggered = false;
    };
    void initializeWorld(bool captureRunStartInventory = true);
    void resetWorldSimulationState();
    void resetWorldPlayerState();
    void resetWorldMapAndRingState();
    void resetWorldActionSystems();
    void resetWorldEffectState();
    void bindWorldEnemyCatalogs();
    void resetWorldEnemyState();
    void restoreWorldEnemyState(const EnemySystem& state);
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
        bool restoreRetainedInventory,
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
    void stopAudioBgm(float fadeSeconds = 0.0f);
    void playAudioSe(std::string_view id, float volumeScale = 1.0f, float pitchScale = 1.0f);
    void playAudioSeAt(std::string_view id, Vec2 worldPosition, float volumeScale = 1.0f, float pitchScale = 1.0f);
    float audioPanForWorldPosition(Vec2 worldPosition) const;
    float playAudioJingle(
        std::string_view id,
        float fallbackDurationSeconds,
        float bgmFadeOutSeconds = 0.08f,
        float bgmFadeInSeconds = 0.25f,
        float volumeScale = 1.0f,
        float pitchScale = 1.0f);
    void updateAudioJingle(float dt);
    void playUiSoundEvents(const UiContext& ui);
    void enterBase();
    void placeBasePlayerAtMineExitReturnPoint();
    void placeBasePlayerAtHomeDoorResumePoint();
    void startMiningFromBase(bool useLatestWarpPoint, bool forceRegenerate = false);
    void loadOpeningKamishibaiData();
    void loadEndingKamishibaiData();
    void loadTitleCreditsData();
    void loadStoryEvents();
    void startOpeningKamishibai();
    void finishOpeningKamishibai(bool completedPlayback);
    void updateOpeningKamishibai(const Input& input, float dt);
    EndingKind resolveEndingKamishibaiKind(EndingKind kind) const;
    void requestEndingKamishibai(EndingKind kind);
    void startEndingKamishibai(EndingKind kind = EndingKind::Main);
    void startEndingReplayKamishibai(EndingKind kind);
    void startEndingKamishibaiPlayback(EndingKind kind, bool replay);
    void startFinalBossEndingKamishibaiAfterTransition();
    void finishEndingKamishibai(bool completedPlayback);
    void updateEndingKamishibai(const Input& input, float dt);
    void updateTitleScreen(const Input& input, UiContext& ui);
    void openTitleOptions();
    void openTitleCredits();
    void returnToTitleMain();
    void startTitleGame();
    void requestScreenTransition(
        ScreenTransitionTarget target,
        ScreenTransitionSound sound = ScreenTransitionSound::Generic);
    void requestDeathResultExitTransition(ScreenTransitionTarget target);
    bool deathResultExitTransitionActive() const;
    void requestMiningStartTransition(bool useLatestWarpPoint, bool forceRegenerate);
    void requestReturnToBaseTransition(
        bool stageCleared,
        bool died,
        ScreenTransitionSound sound = ScreenTransitionSound::Generic);
    void requestBaseAreaCrossfade(BaseArea targetArea, Vec2 playerPosition, Vec2 playerFacing, std::string status);
    void requestBaseAreaFade(BaseArea targetArea, Vec2 playerPosition, Vec2 playerFacing, std::string status, bool closeBaseUi);
    void startScreenTransition(
        ScreenTransitionTarget target,
        ScreenTransitionPhase phase,
        ScreenTransitionSound sound);
    void playScreenTransitionSound(ScreenTransitionSound sound);
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
    void startLevelUpPresentation();
    void updateLevelUpPresentation(float dt);
    Vec2 levelUpPresentationAnchor() const;
    void updateLevelUpScreen(const Input& input, UiContext& ui, float dt);
    bool applyLevelUpSelection(RingLevelUpgradeSelection selection);
    void resetLevelRingUpgradePointsForRun();
    void refreshEquipmentModifiers();
    float effectiveInitialRingRadiusForRing(int ringIndex, int levelRadiusPoints) const;
    float effectiveInitialRingSpeedForRing(int ringIndex, int levelSpeedPoints) const;
    float effectiveInitialRingWeightLimitForRing(int ringIndex, int levelWeightLimitPoints) const;
    float effectiveRingShiftDistanceForRing(int ringIndex) const;
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
    struct CaptureAbsorbAnimation;
    void updateCapturedProjectileBehaviors(float dt);
    void updateCapturedUtilityBehaviors(float dt);
    void updateWetGroundFromStatus();
    void updateAmbientParticleEffects(float dt);
    bool handleCaptureResult(const CaptureResult& capture);
    bool storeCapturedEnemyDefinition(const EnemyDefinition& enemy);
    void startCaptureAbsorbAnimation(const CaptureResult& capture);
    void updateCaptureAbsorbAnimations(float dt);
    void finalizeCaptureAbsorbAnimation(const CaptureAbsorbAnimation& animation);
    Vec2 captureAbsorbPosition(const CaptureAbsorbAnimation& animation, Vec2 targetPosition) const;
    void handleCapturedExplosion(const CapturedExplosionRequest& request);
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
    enum class AcquisitionNoticeKind {
        Object,
        Material,
        Money,
    };
    enum class AcquisitionNoticePresentation {
        FirstDiscovery,
        Standard,
        Reward,
    };
    enum class AcquisitionNoticeAnimationPhase {
        Opening,
        Visible,
        Closing,
    };
    struct AcquisitionNotice {
        AcquisitionNoticeKind kind = AcquisitionNoticeKind::Object;
        AcquisitionNoticePresentation presentation = AcquisitionNoticePresentation::Standard;
        AcquisitionNoticeAnimationPhase animationPhase = AcquisitionNoticeAnimationPhase::Opening;
        std::string title;
        std::string objectId;
        std::string instanceId;
        std::string statusText;
        MaterialType materialType = MaterialType::EnhancementOre;
        int amount = 1;
        bool protectable = false;
        bool jingleOnShow = false;
        bool jinglePlayed = false;
        float animationProgress = 0.0f;
    };
    struct CaptureAbsorbAnimation {
        Enemy enemy;
        ItemData item;
        Vec2 startPosition{};
        Vec2 lastPosition{};
        float elapsedSeconds = 0.0f;
        float durationSeconds = 0.78f;
        float flyDelaySeconds = 0.24f;
        float sparkleTimer = 0.0f;
    };
    using MerchantSellTarget = BaseItemTarget;
    struct MerchantBulkSellSummary {
        int itemCount = 0;
        int totalPrice = 0;
    };
    struct BaseMiningRescueDropItem {
        std::string objectId;
        Vec2 startPosition{};
        Vec2 targetPosition{};
        float delaySeconds = 0.0f;
        bool granted = false;
    };
    struct BaseMiningRescueDropState {
        bool active = false;
        float elapsedSeconds = 0.0f;
        std::array<BaseMiningRescueDropItem, 2> items{};
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
    int merchantProductPurchasableQuantity(const MerchantProduct& product) const;
    bool canBuyMerchantProduct(const MerchantProduct& product) const;
    void refreshHighValueBuyObjects(bool force);
    void refreshMerchantStock(bool force);
    bool buyMerchantProduct(int index, int count);
    void sellMerchantEntry(int index, int count);
    MerchantSellTarget merchantSellTargetForSourceSlot(int source, int slotIndex) const;
    MerchantSellTarget merchantSellTargetForScreenSlot(int slotIndex) const;
    bool merchantSellTargetAvailable(MerchantSellTarget target) const;
    int merchantSellTargetPrice(MerchantSellTarget target) const;
    int merchantSellTargetQuantity(MerchantSellTarget target) const;
    std::vector<MerchantSellTarget> merchantSellTargetsForSource(int source) const;
    std::optional<ItemKey> itemKeyForBaseItemTarget(BaseItemTarget target) const;
    BaseItemTarget baseItemTargetForItemKey(const ItemKey& key) const;
    std::optional<ItemKey> itemKeyForProcessingTarget(ProcessingTarget target) const;
    bool moveItemKeyToGridPlacement(const ItemKey& key, int placement);
    std::optional<bool> itemProtectionEnabled(const ItemKey& key) const;
    ItemProtectionToggleResult toggleItemProtection(const ItemKey& key);
    bool sortBaseItemSource(int source);
    BaseRingInteractionResult updateBaseRingItemInteraction(
        const Input& input,
        UiContext& ui,
        int source,
        int& selection,
        BaseRingPreviewKind previewKind,
        float animationSeconds,
        BaseRingInteractionMode mode = BaseRingInteractionMode::Manage);
    bool cancelBaseRingItemInteraction(bool restoreOriginalAngle);
    void clearBaseItemInteractions();
    bool batchItemKeySelected(
        const BatchItemSelectionState& state,
        const ItemKey& key) const;
    bool batchItemSelectionTargetSelected(
        const BatchItemSelectionState& state,
        BaseItemTarget target) const;
    bool toggleBatchItemSelectionTarget(BatchItemSelectionState& state, BaseItemTarget target);
    bool addBatchItemSelectionTarget(BatchItemSelectionState& state, BaseItemTarget target);
    void clearBatchItemSelectionState(BatchItemSelectionState& state);
    bool merchantBulkSellTargetSelected(MerchantSellTarget target) const;
    bool toggleMerchantBulkSellTarget(MerchantSellTarget target);
    void selectAllMerchantBulkSellTargets(int source);
    void pruneMerchantBulkSellSelection();
    MerchantBulkSellSummary merchantBulkSellSummary() const;
    bool sellMerchantBulkSelection();
    void clearMerchantBulkSellState();
    bool sellMerchantTarget(MerchantSellTarget target, int count);
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
    std::vector<ProcessingMode> processingCommandModes(ProcessingTarget target) const;
    std::vector<UiCommandMenuItem> processingCommandItems(ProcessingTarget target) const;
    void applyProcessingBulkRepair();
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
    void sortWarehouseByItemOrder();
    int warehouseEntryIndexAtStorageSlot(int slot) const;
    void assignWarehouseEntryToStorageSlot(int entryIndex, int slot);
    void removeWarehouseDisplaySlotAtEntryIndex(int entryIndex);
    bool canAddWarehouseObjectStack(std::string_view objectId, int count) const;
    bool addWarehouseObjectStack(const InventoryObjectStack& stack, int count);
    StorageTransferTarget storageDepositTargetForSourceSlot(int source, int slotIndex) const;
    StorageTransferTarget storageDepositTargetForScreenSlot(int slotIndex) const;
    StorageTransferTarget storageWithdrawTargetForSlot(int slotIndex) const;
    bool storageTransferTargetAvailable(StorageTransferTarget target) const;
    bool storageTransferTargetIsStack(StorageTransferTarget target) const;
    int storageTransferTargetStackCount(StorageTransferTarget target) const;
    InventoryUiEntryView storageTransferTargetView(StorageTransferTarget target) const;
    bool depositStorageTarget(StorageTransferTarget target, int count);
    bool withdrawStorageTarget(StorageTransferTarget target, int count);
    std::vector<StorageTransferTarget> storageDepositTargetsForSource(int source) const;
    bool storageBulkDepositTargetSelected(StorageTransferTarget target) const;
    bool toggleStorageBulkDepositTarget(StorageTransferTarget target);
    void selectAllStorageBulkDepositTargets(int source);
    void pruneStorageBulkDepositSelection();
    StorageBatchTransferSummary storageBulkDepositSummary() const;
    bool depositStorageBulkSelection();
    std::vector<StorageTransferTarget> storageWithdrawTargets() const;
    bool storageBulkWithdrawTargetSelected(StorageTransferTarget target) const;
    bool toggleStorageBulkWithdrawTarget(StorageTransferTarget target);
    void selectAllStorageBulkWithdrawTargets();
    void pruneStorageBulkWithdrawSelection();
    StorageBatchTransferSummary storageBulkWithdrawSummary() const;
    bool withdrawStorageBulkSelection();
    void clearStorageBatchSelectionState();
    int storageBulkActionCount() const;
    UiButtonState storageBulkActionState(int actionIndex) const;
    bool canDepositAnyBackpackItem() const;
    void depositAllBackpackItems();
    void prepareRingPresetFromWarehouse(int presetIndex);
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
    bool upgradeExecutable(int index) const;
    void buyUpgrade(int index);
    void closeBaseFacilityScreens();
    void openRingWorkshop();
    void resetRingWorkshopDraft();
    int ringLevelUpgradePointTotal() const;
    bool ringWorkshopRespecChanged() const;
    int ringWorkshopRespecMoneyCost() const;
    int ringWorkshopRespecMoonCost() const;
    bool adjustRingWorkshopRespec(RingLevelUpgradeSelection from, RingLevelUpgradeSelection to);
    bool openRingWorkshopRespecConfirm();
    void applyRingWorkshopRespec();
    void drawRingWorkshopRespecConfirmDialog(Renderer& renderer, UiRect panel) const;
    const char* ringWorkshopUpgradeName(RingWorkshopUpgrade upgrade) const;
    int ringWorkshopUpgradeLevel(RingWorkshopUpgrade upgrade) const;
    int ringWorkshopUpgradeMaxLevel(RingWorkshopUpgrade upgrade) const;
    int ringWorkshopUpgradeMoneyCost(RingWorkshopUpgrade upgrade) const;
    int ringWorkshopUpgradeMoonCost(RingWorkshopUpgrade upgrade) const;
    bool ringWorkshopUpgradeExecutable(RingWorkshopUpgrade upgrade) const;
    float ringWorkshopUpgradeCurrentValue(RingWorkshopUpgrade upgrade) const;
    float ringWorkshopUpgradeNextValue(RingWorkshopUpgrade upgrade) const;
    std::string ringWorkshopUpgradeValueText(RingWorkshopUpgrade upgrade, float value) const;
    std::vector<UiResultDialogLine> ringWorkshopUpgradeResultLines(
        RingWorkshopUpgrade upgrade,
        int ringIndex,
        float beforeValue,
        float afterValue) const;
    RingWorkshopUpgrade ringWorkshopUpgradeForDisplayIndex(int index) const;
    float ringWorkshopRadiusMinForRing(int ringIndex) const;
    float ringWorkshopRadiusMaxForRing(int ringIndex) const;
    float ringWorkshopRadiusSettingForRing(int ringIndex) const;
    bool setRingWorkshopRadiusSettingForRing(int ringIndex, float meters);
    void buyRingWorkshopUpgrade(RingWorkshopUpgrade upgrade);
    void openBookshelf();
    std::vector<EndingKind> bookshelfEndingReplayChoices() const;
    int bookshelfMenuItemCount() const;
    bool encyclopediaComplete() const;
    bool canSyncEncyclopediaFromInventoryAndRing() const;
    void syncEncyclopediaFromInventoryAndRing();
    void captureEncyclopediaSyncSuppressState();
    void applyEffectDiscoveries(const std::vector<EffectDiscoveryEvent>& discoveries);
    bool shouldRecordEffectDiscoveries() const;
    void recordMainObjectObtained(std::string_view objectId);
    void recordMainCapturedEnemy(std::string_view enemyId);
    void recordObjectAcquisitionNotice(
        std::string_view objectId,
        std::string_view instanceId,
        bool protectable,
        Vec2 position,
        int amount = 1);
    void recordRewardObjectAcquisitionNotice(
        std::string_view objectId,
        std::string_view instanceId,
        bool protectable,
        Vec2 position);
    void recordRewardMaterialAcquisitionNotice(MaterialType materialType, int amount);
    void recordRewardMoneyAcquisitionNotice(int amount);
    bool itemAcquisitionNoticeActive() const;
    void playItemAcquisitionNoticeJingle();
    void closeItemAcquisitionNotice();
    void queueIntroTutorialChestLootDialogueIfReady();
    float measureItemAcquisitionNoticeContentHeight(
        Renderer& renderer,
        const AcquisitionNotice& notice) const;
    void updateItemAcquisitionNotice(const Input& input, UiContext& ui, Renderer& renderer, float dt);
    void addStoryFlag(std::string flag);
    void updateBookshelfScreen(const Input& input, UiContext& ui);
    void updateScreenMode(
        const Input& input,
        UiContext& ui,
        Renderer& renderer,
        float dt,
        std::vector<EffectDiscoveryEvent>* discoveryEvents);
    int unlockedRingPresetSlotCount() const;
    bool registerRingPresetShortcut(int presetIndex);
    bool applyRingPreset(int presetIndex);
    bool hasAnyMiningToolForBaseRescue() const;
    bool canAffordMerchantMiningToolForBaseRescue() const;
    bool shouldStartBaseMiningRescueDropEvent() const;
    void startBaseMiningRescueDropEvent();
    void updateBaseMiningRescueDropEvent(float dt, UiContext& ui);
    bool grantBaseMiningRescueTool(std::string_view objectId);
    void renderBaseMiningRescueDropEvent(Renderer& renderer) const;
    void updateBaseScreen(const Input& input, UiContext& ui, float dt);
    void updateStoryEventCommand(float dt);
    void beginStoryPhoneSound(StoryPhoneSoundKind kind);
    void updateStoryPhoneSound(float dt);
    bool storyPhoneSoundActive() const;
    void beginStoryShakeCommand(float amplitude, float duration, std::string_view soundId);
    void updateStoryShakeCommand(float dt);
    bool storyShakeCommandActive() const;
    void updateBaseStoryPresentationCommand(float dt);
    void clearBaseStoryPresentation();
    void applyBaseReturnSceneBeginPlacement();
    bool applyBaseReturnSceneBeginPlacementForTrigger(std::string_view trigger);
    void renderBaseStoryFacilityMarkers(Renderer& renderer) const;
    void renderBaseStoryChicoryFlight(Renderer& renderer) const;
    void renderBaseStoryRingDemo(Renderer& renderer) const;
    bool storyEventUsesBasePresentation(std::string_view id) const;
    void openBaseDiary();
    void closeBaseDiary();
    void updateBaseDiaryScreen(const Input& input, UiContext& ui);
    void updateBaseStorySpeakerFacing();
    void updateBasePlayerSpriteAnimation(float dt, bool walking);
    void updateBaseActorIdleAnimation(float dt);
    void updateBasePlayerSpriteFlipFromFacing();
    void updatePauseMenu(const Input& input, UiContext& ui);
    void choosePauseMenuItem(int item);
    void leavePausePage();
    enum class OperationSettingsBindingEditMode {
        Replace,
        Append,
    };
    void prepareOptionsMenu();
    void openOptionsMenu();
    bool optionsMenuActive() const;
    bool operationSettingsModalVisible() const;
    void loadOptionsSettings();
    void applyOptionsSettings(std::string status);
    void updateOptionsMenu(const Input& input, UiContext& ui);
    void updateOperationSettings(const Input& input, UiContext& ui);
    void updateAudioSettings(const Input& input, UiContext& ui);
    void updateVideoSettings(const Input& input, UiContext& ui);
    bool handleOperationSettingsEvent(const SDL_Event& event);
    bool handleOperationSettingsCaptureResult(
        InputRemapCaptureResult result,
        InputAction action,
        int column,
        const InputBinding& binding);
    void renderOptionsMenu(Renderer& renderer) const;
    void renderOperationSettings(Renderer& renderer) const;
    void renderAudioSettings(Renderer& renderer) const;
    void renderVideoSettings(Renderer& renderer) const;
    void beginOperationSettingsBindingCapture(OperationSettingsBindingEditMode mode);
    void clearOperationSettingsPendingEdit();
    void queueOperationSettingsBinding(InputAction action, int column, const InputBinding& binding);
    void applyOperationSettingsBinding(InputAction action, int column, const InputBinding& binding, bool removeConflicts);
    void clearOperationSettingsBinding(InputAction action, int column);
    void resetOperationSettingsAction(InputAction action);
    void resetOperationSettingsCategory();
    void resetOperationSettingsAll();
    void openRingScreen();
    void updateRingScreen(const Input& input, UiContext& ui, float dt);
    void cancelRingGrab();
    bool playerDeathSequenceActive() const;
    bool liveSpellRingHiddenForDeath() const;
    bool liveSpellRingHiddenForBossEncounter() const;
    bool gameplayRewardsEnabled() const;
    void beginPlayerDeathSequence();
    void updatePlayerDeathSequence(float dt);
    void freezePlayerDeathPoseForResult();
    void initializePlayerDeathRingPresentation();
    void updatePlayerDeathRingPresentation(float dt);
    bool playerDeathRingPresentationComplete() const;
    void dropPlayerDeathRingItem(int ringIndex, std::size_t itemIndex);
    void renderPlayerDeathRingPresentation(Renderer& renderer, float totalSeconds) const;
    void enterGameOver();
    void updateGameOverScreen(const Input& input, UiContext& ui, float dt);
    void retryAfterGameOver();
    void returnToBaseAfterGameOver();
    void startIntroTutorialDungeon();
    bool updateIntroTutorial(const Input& input, float dt);
    void equipIntroTutorialStartingTools();
    void addIntroTutorialTorchToRing();
    void startIntroTutorialEnemyEncounterEvent();
    bool startIntroTutorialEnemyEncounterPresentation(bool debugReplay, std::function<void()> onComplete);
    void startIntroTutorialSlimeFocusDialogue(bool debugReplay, std::function<void()> onComplete);
    void startIntroTutorialEnemyRetreatDialogue(bool debugReplay, std::function<void()> onComplete);
    void spawnIntroTutorialChest();
    void spawnIntroTutorialSecondChest();
    void unlockIntroTutorialFreeRoute();
    void completeIntroTutorialAndReturnToBase();
    void syncIntroTutorialTerrainDamageLocks();
    bool introTutorialActive() const;
    Vec2 introTutorialExitPosition() const;
    std::vector<LightSource> introTutorialLightSources(double totalSeconds) const;
    void enterAstralResult(AstralRunResult result);
    void updateAstralResultScreen(const Input& input, UiContext& ui, float dt);
    void returnToBaseAfterAstralResult();
    void beginDeathResultPrelude();
    bool updateDeathResultPrelude(float dt, UiContext& ui);
    bool deathResultPreludeBlocksWindow() const;
    float deathResultPreludeBlackAlpha() const;
    float deathResultPreludeStarAlpha() const;
    void recordAstralEchoStar(bool markRecent);
    void clearAstralEchoRecentStar();
    bool astralEchoQuitRecordable() const;
    bool saveAstralEchoMeta() const;
    AstralRunSummary makeAstralRunSummary(AstralRunResult result) const;
    int calculateAstralRunScore(const AstralRunSummary& summary) const;
    int astralRunMaterialDeltaFromStart() const;
    int astralRunMoneyDeltaFromStart() const;
    bool shouldRefreshMerchantOnReturn(bool stageCleared, bool died) const;
    void recordBaseHintDungeonReturn();
    bool queueBaseHintEventOnReturn(std::string_view returnedStageId, bool stageCleared);
    void returnToBaseFromNormalStage(bool stageCleared, bool died);
    InventoryCarryState captureInventoryCarryState() const;
    void restoreInventoryCarryState(const InventoryCarryState& state);
    RoguelikeCarryOutDelta collectRoguelikeCarryOutDelta() const;
    RoguelikeCarryOutMergeResult mergeRoguelikeCarryOutDelta(const RoguelikeCarryOutDelta& delta);
    void captureRunStartInventoryState();
    void clearTemporaryPlayerState(bool fullHeal);
    Vec2 latestWarpPointStartPosition() const;
    Vec2 warpPointStartPositionForCurrentRequest() const;
    std::vector<WarpPoint> selectableWarpPointsForCurrentStageStart() const;
    void rebuildUnlockedWarpPointsForStart(Vec2 latestPosition);
    void resetWarpPointRunState();
    void captureDungeonState();
    bool restoreDungeonState(bool useLatestWarpPoint);
    bool captureDebugRoguelikeRunSnapshot();
    bool restoreDebugRoguelikeRunSnapshot();
    bool canRegenerateCurrentStage() const;
    bool currentStageIsRoguelike() const;
    bool currentStageCleared() const;
    bool stageCleared(std::string_view stageId) const;
    std::string stageClearFlagForStage(std::string_view stageId) const;
    bool shouldRecordMainProgressKnowledge() const;
    std::string currentStageBossCaptureObjectId() const;
    bool hasCapturedBossForCurrentStage() const;
    std::size_t retainedWorldDropCountForCurrentStage() const;
    void initializeWarpPointsFromLayout();
    int discoveredWarpPointCount() const;
    std::vector<WarpPoint> discoveredWarpPoints() const;
    int nearestWarpPointIndex(Vec2 position) const;
    Vec2 safePlayerStartPosition(Vec2 preferredPosition);
    Vec2 dungeonEntrancePosition() const;
    UiRect dungeonInspectableRect(Vec2 center, Vec2 size) const;
    bool dungeonInspectableInRange(Vec2 center, Vec2 size) const;
    bool dungeonInspectableHovered(Vec2 center, Vec2 size, Vec2 worldPosition) const;
    int nearbyDiscoveredWarpPointIndex() const;
    bool warpReturnInteractionArmed(int pointVectorIndex) const;
    void disarmWarpReturnInteractionAt(Vec2 startPosition);
    void updateWarpReturnInteractionArming();
    bool updateWarpReturnUi(const Input& input, UiContext& ui);
    bool unlockAllWarpPointsForCurrentDungeon();
    void updateWarpPoints(float dt);
    void resetDungeonRouteDeviation();
    void updateDungeonRouteDeviation(float dt);
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
    bool lightweightModeEnabled() const;
    float screenShakeScale() const;
    void addScreenShake(float amplitude, float duration);
    void updateScreenShake(float dt);
    Vec2 screenShakeOffset(double totalSeconds) const;
    void addPlayerDamageVignetteFlash(int damageAmount);
    void updatePlayerDamageVignette(float dt);
    void renderPlayerDamageVignette(Renderer& renderer, double totalSeconds) const;
    static const char* dungeonEventKindId(DungeonEventKind kind);
    static bool dungeonEventKindFromId(std::string_view id, DungeonEventKind& outKind);
    static const char* dungeonEventKindDisplayName(DungeonEventKind kind);
    static std::string dungeonEventDiscoverySeenFlag(DungeonEventKind kind);
    static std::string dungeonEventDiscoveryStoryEventId(DungeonEventKind kind);
    void initializeDungeonEventInstancesFromLayout();
    bool spawnDungeonEventEnemy(
        DungeonEventInstance& event,
        Vec2 position,
        bool sleeping,
        bool bossVariant,
        int* outRuntimeId = nullptr);
    void ensureDungeonEventEncounterPrepared(DungeonEventInstance& event);
    void prepareDungeonEventEncountersForView();
    void updateDungeonEvents(float dt, double totalSeconds);
    void handleDungeonEventEnemyEvent(const EnemyEvent& enemyEvent);
    DungeonEventInstance* nearbyDungeonEventNpc();
    const DungeonEventInstance* nearbyDungeonEventNpc() const;
    DungeonEventInstance* pointedDungeonEventNpc(Vec2 worldPosition);
    const DungeonEventInstance* pointedDungeonEventNpc(Vec2 worldPosition) const;
    bool updateDungeonEventNpcInteraction(const Input& input, UiContext& ui);
    bool dungeonEventItemRequestUiActive() const;
    void openDungeonEventItemRequestUi(DungeonEventInstance& event);
    void closeDungeonEventItemRequestUi();
    void updateDungeonEventItemRequestUi(const Input& input, UiContext& ui);
    void renderDungeonEventItemRequestUi(Renderer& renderer, double totalSeconds) const;
    bool startDungeonEventWitchRewardDialogue(DungeonEventInstance& event);
    std::optional<DungeonEventRewardRequest> dungeonEventWitchRewardRequest(DungeonEventKind kind) const;
    std::string dungeonEventNpcPromptText() const;
    bool updateDungeonEventDiscovery(float dt);
    void appendDungeonEventRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const std::vector<LightSource>& extraLights,
        double totalSeconds) const;
    bool spawnDungeonEventReward(DungeonEventInstance& event, const DungeonEventRewardRequest& request);
    bool completeDungeonEvent(DungeonEventInstance& event, std::optional<DungeonEventRewardRequest> reward);
    bool ensureDungeonEventChest(DungeonEventInstance& event, DungeonTile tile, LootChestKind chestKind);
    bool scheduleDungeonEventChestReveal(
        DungeonEventInstance& event,
        std::vector<DungeonTile> tiles,
        LootChestKind chestKind);
    bool requestDungeonRewardChestFocus(Vec2 focusWorldPos, std::function<void()> onChestAppear);
    void applyDungeonEventCavity(const DungeonEventInstance& event);
    bool debugRequestDungeonEventPlacement(DungeonEventKind kind);
    bool debugPlaceDungeonEvent(DungeonEventKind kind);
    void flushPendingDebugDungeonEventPlacement();
    std::string nearestDungeonEventDebugText() const;
    bool dumpDungeonDebugState();
    void resetDungeonFocus();
    bool updateDungeonFocus(float dt);
    bool dungeonFocusActive() const;
    static const char* dungeonFocusPhaseName(DungeonFocusPhase phase);
    std::string dungeonFocusDebugText() const;
    void resetAstralRunState();
    void initializeAstralRunForLayout();
    void updateAstralRunProgress();
    int roguelikeSectionRankForDepthMeters(int depthMeters) const;
    int roguelikeDepthMetersForSectionRank(int sectionRank) const;
    int roguelikeAdjustedDepthRank(int localDepthRank) const;
    int roguelikeDepthMetersForWorldPosition(Vec2 position) const;
    int roguelikeDepthRankForWorldPosition(Vec2 position) const;
    void rebuildRoguelikeAreaFromAstralState();
    bool debugSetRoguelikeAreaForDepthMeters(int depthMeters);
    void initializeRoguelikeBigHoleFromLayout();
    void clearRoguelikeBigHoleState();
    bool updateRoguelikeBigHoleUi(const Input& input, UiContext& ui);
    void advanceRoguelikeAreaFromBigHole();
    void configureBossSpawnPointFromRoguelikeBigHole();
    void initializeRoguelikeFacilitiesFromLayout();
    void clearRoguelikeFacilities();
    bool roguelikeFacilityUiActive() const;
    bool updateRoguelikeFacilityUi(const Input& input, UiContext& ui, float dt);
    bool updateRoguelikeFacilityInteraction(const Input& input, UiContext& ui);
    std::string roguelikeFacilityPromptText() const;
    void openRoguelikeFacility(RoguelikeFacilityKind kind, std::string_view facilityId);
    void closeRoguelikeFacilityUi();
    void prepareRoguelikeMerchantStock();
    void restoreRoguelikeMerchantStock();
    int roguelikeMerchantStockLimit() const;
    int roguelikeFacilityCostStep() const;
    int roguelikeAdjustedFacilityMoneyCost(int baseCost) const;
    int roguelikeAdjustedFacilityMaterialCost(int baseCost) const;
    int processingBulkRepairTargetCount() const;
    int processingBulkRepairMoneyCost() const;
    int processingBulkRepairOreCost() const;
    bool processingBulkRepairExecutable() const;
    void updateDungeonDepthTutorials();
    void applyAstralDistortionToLayout();
    AstralDistortionKind chooseAstralDistortionForDepth(int depth, AstralDistortionKind previous) const;
    float astralLightRadiusMultiplier() const;
    float astralHardnessMultiplier() const;
    RuntimeBalance runtimeBalanceForDungeon() const;
    bool astralRunActive() const;
    bool spawnRewardNodeWorldDrop(
        RewardNode& node,
        Vec2 center,
        std::string_view sourceLabel,
        bool allowGeneratedRewardLoot);
    void initializeRewardNodesFromLayout();
    int grantDungeonMoney(int amount, Vec2 origin);
    int takeDungeonMoney(int amount, Vec2 origin);
    void revealRewardNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    void revealMoonFragmentNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    void materializeExposedPlacementDrops(bool allowGeneratedRewardLoot);
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
    void assignChestMimic(ChestNode& node);
    bool tryTriggerChestMimic(ChestNode& node);
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
        LootSourceKind sourceKind = LootSourceKind::Chest,
        std::string_view requiredTag = {});
    const ObjectDefinition* selectWeightedObjectLoot(
        LootChestKind chestKind,
        int depthRank,
        std::mt19937& rng,
        std::string_view sourceLabel,
        LootSourceKind sourceKind = LootSourceKind::Chest,
        std::string_view requiredTag = {}) const;
    bool roguelikeObjectAllowed(std::string_view objectId) const;
    bool roguelikeObjectAllowed(const ObjectDefinition& object) const;
    std::optional<std::string> firstRoguelikeAllowedObjectId() const;
    static double roguelikeSourceCategoryMultiplier(LootSourceKind sourceKind, std::string_view category);
    Vec2 safeLootLandingPosition(Vec2 center, std::mt19937& rng);
    void spawnInventoryDiscardRequests(std::vector<InventoryDiscardRequest> requests);
    void consumeInventoryUseEvents();
    void updateDigToolFailsafe(float dt);
    int countUsableDigToolsOnRing(bool& hasHalfDurabilityOrBelow) const;
    int countUsableDigToolsInInventory(bool& hasHalfDurabilityOrBelow) const;
    int countNearbyUsableDigToolDrops(float radius, bool& hasHalfDurabilityOrBelow) const;
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
    void updatePendingBuriedEnemySpawns(float dt);
    void updateRingEffectDiscoveries(std::vector<EffectDiscoveryEvent>& discoveryEvents);
    void updateOrbitAreaEffects(float dt, std::vector<EffectDiscoveryEvent>& discoveryEvents);
    void updateOrbitGroundEffects(float dt, std::vector<EffectDiscoveryEvent>& discoveryEvents);
    void schedulePendingBuriedEnemySpawn(DungeonTile tile, Vec2 position, int depthRank);
    void schedulePendingBuriedEnemySpawn(const EnemyNode& node);
    std::vector<Vec2> spawnHiddenEnemyNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles);
    int exposedEnemyNodeCount() const;
    int buriedEnemyNodeCount() const;
    int spawnedEnemyNodeCount() const;
    void configureBossSpawnPointFromWarp(Vec2 warpPosition);
    void carveBossArenaAroundSpawnPoint();
    void prepareBossEncounterAreaFromWarp(Vec2 warpPosition);
    bool currentBossUsesDungeonPreview() const;
    void ensureBossPreviewSpawned();
    Vec2 bossApproachPosition() const;
    bool bossArenaContains(Vec2 position) const;
    bool usesNormalBossStoryPlayerPosition() const;
    Vec2 bossIntroPlayerPosition() const;
    Vec2 normalBossStoryPlayerPosition() const;
    Vec2 bossStoryPlayerPosition() const;
    void requestBossEncounterIntro(BossEncounterPurpose purpose);
    void applyBossStoryPlayerPlacement();
    void finishBossEncounterIntroTransition();
    bool startBossBeforeStoryPresentation(std::string_view id, bool debugReplay, std::function<void()> onComplete);
    bool shouldPlayBossAfterStoryEvent() const;
    void requestBossEncounterAfterDialogueTransition();
    void finishBossEncounterAfterDialogueTransition();
    Vec2 bossAfterStoryPresentationPosition() const;
    std::string bossAfterStoryBossEnemyId() const;
    void beginBossAfterStoryPresentation();
    bool startBossAfterStoryPresentation(std::string_view id, bool debugReplay, std::function<void()> onComplete);
    bool isFinalBossFirstClearEncounter(BossEncounterPurpose purpose) const;
    bool startFinalBossAfterStoryInPlace();
    void updateBossSpawn();
    void resetBossEncounter();
    bool spawnBossForCurrentEncounter(EnemySpawnVisualKind spawnVisualKind);
    bool beginBossFightForCurrentEncounter();
    void updateDungeonStoryCommand(const DialogueCommand& command, float dt);
    void updateDungeonCameraFocusStoryCommand(const DialogueCommand& command, float dt);
    void updateDungeonBossSpawnStoryCommand(const DialogueCommand& command, float dt);
    void updateDungeonBossAfterDefeatStoryCommand(const DialogueCommand& command, float dt);
    void updateDungeonSmallMoleEscapeStoryCommand(const DialogueCommand& command, float dt);
    void updateDungeonBossExplodeEscapeStoryCommand(const DialogueCommand& command, float dt);
    void updateDungeonReturnToBaseAfterStoryCommand(const DialogueCommand& command);
    void clearDungeonStoryPresentation();
    void beginBossDefeatSequence(Vec2 position);
    bool updateBossEncounterFlow(float dt);
    void finishBossEncounterAfterDialogue();
    bool bossEncounterBlocksProgress() const;
    float bossDefeatPresentationProgress() const;
    void captureRetrySnapshotAtWarpPoint();
    void restoreRetrySnapshot();
    void renderDungeonEntrance(Renderer& renderer) const;
    void renderWarpPoints(Renderer& renderer) const;
    void renderRoguelikeBigHole(Renderer& renderer) const;
    void renderRoguelikeFacilities(Renderer& renderer) const;
    bool rewardNodeVisibleOnDungeonMap(const RewardNode& node) const;
    bool moneyNodeVisibleOnDungeonMap(const MoneyNode& node) const;
    bool moonFragmentNodeVisibleOnDungeonMap(const MoonFragmentNode& node) const;
    bool chestNodeVisibleOnDungeonMap(const ChestNode& node) const;
    void appendRewardNodeRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const std::vector<LightSource>& extraLights) const;
    void appendDungeonStoryPresentationRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        float totalSeconds) const;
    void renderPendingBuriedEnemySpawnWarnings(Renderer& renderer) const;
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
    void maybeTriggerPlayerFootstep(
        Vec2 footAnchor,
        Vec2 movementDirection,
        bool walking,
        int frameIndex,
        int& previousFrame,
        PlayerFootstepSurface surface);
    void spawnPlayerFootstepDust(Vec2 footAnchor, Vec2 movementDirection);
    void playPlayerFootstepSound(PlayerFootstepSurface surface, int frameIndex);
    void renderPlayerFootstepDust(Renderer& renderer) const;
    void spawnRingEquipFx(const RingEquipFxRequest& request);
    void updateRingEquipFx(float dt);
    Vec2 ringEquipFxTargetScreen(const RingEquipFx& fx) const;
    void renderRingEquipFx(Renderer& renderer) const;
    void initializeDefaultSpellRing();
    void observeRingItemInstanceIds();
    int debugStoryUnlockCountForStage(std::string_view stageId) const;
    bool selectDebugStageForTest(std::string_view stageId);
    void beginAutoSimulationCheckpointMeasurement();
    void updateAutoSimulationCheckpointMeasurement(float dt);
    void recordAutoSimulationEnemyEvent(const EnemyEvent& event);
    void recordAutoSimulationPlayerDamage(const PlayerDamageEvent& event);
    void recordAutoSimulationRecoveryUse();
    void recordAutoSimulationItemBreak();
    void captureAutoSimulationCheckpoint(int warpIndex);
    GameTestCheckpointMeasurementSnapshot autoSimulationCheckpointMeasurementSnapshot() const;
    bool loadSaveData();
    bool saveSaveData(std::string& message) const;
    bool loadSaveData(const std::filesystem::path& path);
    bool saveSaveData(const std::filesystem::path& path, std::string& message) const;
    struct DebugNamedSaveEntry {
        std::string name;
        std::filesystem::path path;
    };
    struct DebugNamedSaveTarget {
        std::string requestedName;
        std::string fileName;
        std::filesystem::path path;
        bool exists = false;
    };
    std::vector<DebugNamedSaveEntry> listDebugNamedSaveData() const;
    std::filesystem::path debugNamedSaveDataPath(std::string_view name) const;
    DiarySaveSummary currentDiarySaveSummary() const;
    DiarySaveSummary loadDiarySaveSummaryFromDisk() const;
    void loadBaseEditData();
    bool saveBaseEditData(std::string& message);
    bool handleBaseEditCommand(std::string_view normalized);
    bool loadObjectImageScaleData();
    bool saveObjectImageScaleData(std::string& message);
    bool handleObjectImageScaleCommand(std::string_view normalized);
    bool handlePortraitExpressionEditCommand(std::string_view normalized);
    bool handlePortraitExpressionEditEvent(const SDL_Event& event);
    bool openPortraitExpressionPickerForSlot(int slotIndex);
    void closePortraitExpressionPicker(bool restorePreview);
    bool savePortraitExpressionSelection(std::string& message);
    bool writePortraitExpressionToStoryFile(
        const std::filesystem::path& path,
        const DialogueLine& line,
        std::string_view speakerId,
        int variant,
        std::string& message);
    void updatePortraitExpressionPicker(const Input& input, UiContext& ui);
    void renderPortraitExpressionPicker(Renderer& renderer) const;
    void rebuildObjectImageScaleList();
    void applyObjectImageScaleFilter(std::string_view preferredSelection = {});
    bool handleObjectImageScaleEditEvent(const SDL_Event& event);
    void enterObjectImageScaleEditMode();
    void exitObjectImageScaleEditMode();
    void updateObjectImageScaleEditScreen(const Input& input, UiContext& ui);
    void renderObjectImageScaleEditScreen(Renderer& renderer) const;
    bool loadHitboxData();
    bool saveHitboxData(std::string& message);
    bool handleHitboxDisplayCommand(std::string_view normalized);
    bool handleEnemyHitboxEditCommand(std::string_view normalized);
    void rebuildEnemyHitboxEditList();
    void applyEnemyHitboxEditFilter(std::string_view preferredSelection = {});
    bool handleEnemyHitboxEditEvent(const SDL_Event& event);
    void enterEnemyHitboxEditMode();
    void exitEnemyHitboxEditMode();
    void updateEnemyHitboxEditScreen(const Input& input, UiContext& ui);
    void renderEnemyHitboxEditScreen(Renderer& renderer, double totalSeconds) const;
    HitboxEditSnapshot makeHitboxEditSnapshot() const;
    void restoreHitboxEditSnapshot(const HitboxEditSnapshot& snapshot);
    void pushHitboxEditUndoSnapshot();
    bool undoHitboxEdit();
    bool redoHitboxEdit();
    const EnemyDefinition* selectedEnemyHitboxDefinitionForEdit() const;
    const ObjectDefinition* selectedObjectHitboxDefinitionForEdit() const;
    std::vector<HitCircle> selectedHitboxEditCircles() const;
    HitboxProfile* ensureSelectedHitboxEditProfile();
    bool selectedEnemyHitboxWeakPointEditable() const;
    int selectedHitboxEditCircleLimit() const;
    bool copyCurrentHitboxEditProfile();
    bool pasteCurrentHitboxEditProfile(bool mirrorX);
    bool copyEnemyHitboxAllDirectionProfiles();
    bool pasteEnemyHitboxAllDirectionProfiles(bool mirrorX);
    bool loadEnemyPlacementData();
    bool saveEnemyPlacementData(std::string& message);
    bool handleEnemyPlacementEditCommand(std::string_view normalized);
    void rebuildEnemyPlacementEditList();
    void applyEnemyPlacementEditFilter(std::string_view preferredSelection = {});
    bool handleEnemyPlacementEditEvent(const SDL_Event& event);
    void enterEnemyPlacementEditMode();
    void exitEnemyPlacementEditMode();
    void updateEnemyPlacementEditScreen(const Input& input, UiContext& ui);
    void renderEnemyPlacementEditScreen(Renderer& renderer, double totalSeconds) const;
    EnemyPlacementEditSnapshot makeEnemyPlacementEditSnapshot() const;
    void restoreEnemyPlacementEditSnapshot(const EnemyPlacementEditSnapshot& snapshot);
    void pushEnemyPlacementEditUndoSnapshot();
    bool undoEnemyPlacementEdit();
    bool redoEnemyPlacementEdit();
    const EnemyDefinition* selectedEnemyPlacementDefinitionForEdit() const;
    EnemyPlacementEntry selectedEnemyPlacementEntryForEdit() const;
    EnemyPlacementEntry& mutableSelectedEnemyPlacementEntryForEdit();
    bool copyCurrentEnemyPlacementOffset();
    bool pasteCurrentEnemyPlacementOffset(bool mirrorX);
    bool copyEnemyPlacementAllDirections();
    bool pasteEnemyPlacementAllDirections(bool mirrorX);
    bool loadEnemyShadowData();
    bool saveEnemyShadowData(std::string& message);
    bool handleEnemyShadowEditCommand(std::string_view normalized);
    void rebuildEnemyShadowEditList();
    void applyEnemyShadowEditFilter(std::string_view preferredSelection = {});
    bool handleEnemyShadowEditEvent(const SDL_Event& event);
    void enterEnemyShadowEditMode();
    void exitEnemyShadowEditMode();
    void updateEnemyShadowEditScreen(const Input& input, UiContext& ui);
    void renderEnemyShadowEditScreen(Renderer& renderer, double totalSeconds) const;
    EnemyShadowEditSnapshot makeEnemyShadowEditSnapshot() const;
    void restoreEnemyShadowEditSnapshot(const EnemyShadowEditSnapshot& snapshot);
    void pushEnemyShadowEditUndoSnapshot();
    bool undoEnemyShadowEdit();
    bool redoEnemyShadowEdit();
    const EnemyDefinition* selectedEnemyShadowDefinitionForEdit() const;
    EnemyShadowSpec selectedEnemyShadowSpecForEdit() const;
    EnemyShadowSpec& mutableSelectedEnemyShadowSpecForEdit();
    bool copyCurrentEnemyShadowSpec();
    bool pasteCurrentEnemyShadowSpec(bool mirrorX);
    bool loadAudioCueManifestForEdit();
    bool saveAudioCueManifestFromEdit(std::string& message);
    bool handleAudioCueEditCommand(std::string_view normalized);
    bool handleAudioCueEditEvent(const SDL_Event& event);
    void rebuildAudioCueFileList();
    void enterAudioCueEditMode(AudioCueEditMode editMode);
    void exitAudioCueEditMode();
    void syncAudioCueEditDraftFromSelection();
    void copySelectedAudioCueSettings();
    void pasteCopiedAudioCueSettings();
    void previewSelectedAudioCueFile();
    void applySelectedAudioCueFile();
    void updateAudioCueEditScreen(const Input& input, UiContext& ui);
    void renderAudioCueEditScreen(Renderer& renderer) const;
    bool handleDebugItemPickerCommand(std::string_view normalized);
    bool handleDebugNamedSaveCommand(std::string_view normalized);
    bool handleDebugNamedSaveEvent(const SDL_Event& event);
    enum class DebugNamedSaveDialogMode {
        Closed,
        Save,
        Load,
    };
    void openDebugNamedSaveDialog();
    void closeDebugNamedSaveDialog();
    void openDebugNamedLoadDialog();
    void closeDebugNamedLoadDialog();
    void rebuildDebugNamedSaveEntries();
    DebugNamedSaveTarget resolveDebugNamedSaveTarget(std::string_view name) const;
    void refreshDebugNamedSaveTargetStatus();
    void requestDebugNamedSave();
    void commitDebugNamedSave(const DebugNamedSaveTarget& target);
    void loadSelectedDebugNamedSave();
    void updateDebugNamedSaveUi(const Input& input, UiContext& ui);
    void renderDebugNamedSaveUi(Renderer& renderer) const;
    void rebuildDebugItemPickerList();
    void applyDebugItemPickerFilter(std::string_view preferredSelection = {});
    bool handleDebugItemPickerEvent(const SDL_Event& event);
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
    void rebuildEffectTestEntries();
    void rebuildEffectTestVisibleEntries(std::string_view preferredEntryId = {});
    void enterEffectTestMode();
    void closeEffectTestMode();
    void exitEffectTestToBase();
    void resetEffectTestPlayback();
    void triggerEffectTestPlayback(const EffectPreviewEntry& entry);
    void updateEffectTestScreen(const Input& input, UiContext& ui, float dt);
    void renderEffectTestScreen(Renderer& renderer, double totalSeconds);
    void rebuildProjectileTestEntries();
    void enterProjectileTestMode();
    void closeProjectileTestMode();
    void exitProjectileTestToBase();
    void resetProjectileTestPlayback();
    void triggerProjectileTestPlayback(const ProjectileDefinition& entry);
    void updateProjectileTestScreen(const Input& input, UiContext& ui, float dt);
    void renderProjectileTestScreen(Renderer& renderer, double totalSeconds);
    void enterEnemyTestMode();
    void exitEnemyTestToBase();
    void spawnSelectedEnemyTestEnemy();
    bool spawnEnemyTestMimicChest(const EnemyDefinition& enemy, Vec2 desiredPosition);
    int spawnEnemyTestMagnetDrops(Vec2 center);
    int spawnEnemyTestHealSlimes(Vec2 center);
    int spawnEnemyTestSwarmMembers(const EnemyDefinition& enemy, Vec2 center);
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
    BaseEditPassabilityLayer currentBasePassabilityLayer() const;
    BaseEditPassabilityLayer editedBasePassabilityLayer() const;
    std::unordered_set<std::int64_t>& baseBlockedTilesFor(BaseArea area, BaseEditPassabilityLayer layer);
    const std::unordered_set<std::int64_t>& baseBlockedTilesFor(BaseArea area, BaseEditPassabilityLayer layer) const;
    bool copyBasePassabilityLayer();
    bool pasteBasePassabilityLayer();
    BaseEditRect baseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect fallback) const;
    void setBaseFacilityRectFor(BaseArea area, std::string_view facilityId, BaseEditRect rect);
    void updateBaseEditScreen(const Input& input, UiContext& ui, float dt);
    void renderBaseEditOverlay(Renderer& renderer) const;
    bool gameProgressPaused() const;
    bool dungeonEventUiSuppressed() const;
    void updatePausedDungeonPresentation(float dt);
    bool basePresentationActive() const;
    void clearBaseTalkSessionSelections();
    std::string selectBaseRandomTalkEventId(std::string_view speakerId);
    std::string baseTalkStoryTrigger(std::string_view speakerId) const;
    bool startBaseTalkStoryEvent(std::string_view speakerId, std::function<void()> onComplete);
    void startBaseMonicaDialogue();
    void startBaseElderDialogue();
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
    bool startStoryEventInternal(std::string_view id, StoryEventStartOptions options);
    bool startStoryEvent(std::string_view id);
    bool startStoryEventWithCompletion(std::string_view id, std::function<void()> onComplete);
    bool startDialogueSequenceWithCompletion(DialogueSequence sequence, std::function<void()> onComplete);
    void updateDialoguePlayerIdleAnimation(float dt);
    void runDialogueCompletionCallbackIfFinished(bool dialogueWasActive);
    bool startStoryEventForDebug(std::string_view id);
    bool startStoryEventForDebugWithCompletion(std::string_view id, std::function<void()> onComplete);
    bool startDebugStoryTestPresentation(std::string_view id, std::function<void()> onComplete);
    bool startStoryEventForTrigger(std::string_view trigger);
    void maybeStartOpeningBaseIntroEvent();
    void pushDungeonNotice(
        std::vector<DungeonLogEntry>& notices,
        std::string message,
        std::string mergeKey,
        float lifetime,
        float mergeSeconds,
        int maxVisible);
    void pushDungeonLog(std::string message, std::string mergeKey = {});
    void pushImportantDungeonNotice(std::string message, std::string mergeKey = {});
    void pushCountedDungeonLog(std::string label, int amount, std::string suffix, std::string mergeKey);
    void updateDungeonLogs(float dt);
    void handleRingItemAddedEvents();
    void appendMoneyPickupLog(int amount);
    void appendPickupLogs(const std::vector<WorldDropPickupEvent>& pickupEvents);
    void handleRingItemBreakEvents(std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr);
    void recordCapturedMonsterRingBreak(std::string_view objectId);
    bool hiddenBadEndingReady() const;
    void unlockHiddenBaseOrbitCorruption();
    bool hiddenBaseOrbitActive() const;
    bool hiddenBaseNpcRemoved(std::string_view facilityId) const;
    bool hiddenBaseAllNonMonicaNpcsRemoved() const;
    void updateHiddenBaseOrbit(const Input& input, UiContext& ui, float dt, bool interactionsEnabled);
    void renderHiddenBaseOrbit(Renderer& renderer) const;
    bool hiddenRouteNpcAttackActive() const;
    bool currentStageIsHiddenMonicaDuel() const;
    bool maybeStartHiddenMonicaDuel();
    void startHiddenMonicaDuel();
    void beginHiddenBadEndingSequence();
    void clearHiddenDungeonNpcTargets();
    void updateHiddenDungeonNpcTargets();
    bool hiddenDungeonNpcTargetRemoved(std::string_view targetKey) const;
    bool hiddenDungeonNpcTargetActive(std::string_view targetKey) const;
    bool handleHiddenDungeonNpcEnemyEvent(const EnemyEvent& enemyEvent);
    void handleHiddenDungeonNpcCaptureResult(const CaptureResult& capture);
    void switchActiveRingWithLog(int delta);
    int unlockedRingHudCount() const;
    UiRect ringStatusHudRect(int ringIndex, int unlockedRingCount) const;
    bool updateRingStatusHud(UiContext& ui, float dt);
    std::string currentMapDisplayName() const;
    void renderTopInfoBar(Renderer& renderer) const;
    void renderOpeningKamishibai(Renderer& renderer) const;
    void renderEndingKamishibai(Renderer& renderer) const;
    void renderTitleScreen(Renderer& renderer) const;
    void renderScreenTransitionOverlay(Renderer& renderer);
    void renderFinalScreenOverlays(Renderer& renderer);
    void renderDevBuildNotice(Renderer& renderer) const;
    bool basePanelUiActive() const;
    bool baseInteractionHintsVisible() const;
    void renderBaseBackdrop(Renderer& renderer) const;
    void renderBaseScreen(Renderer& renderer) const;
    void renderBaseStoryFadeOverlay(Renderer& renderer) const;
    void renderBaseDiaryScreen(Renderer& renderer, UiRect panel) const;
    void renderBookshelfScreen(Renderer& renderer) const;
    void renderLevelUpOverlay(Renderer& renderer);
    void renderPauseMenu(Renderer& renderer) const;
    void renderRingScreen(Renderer& renderer, float totalTime) const;
    void renderRingStatusHud(Renderer& renderer) const;
    void renderItemAcquisitionNotice(Renderer& renderer, float animationSeconds) const;
    UiRect dungeonMinimapRect() const;
    UiRect dungeonMapOverlayPanelRect() const;
    UiRect dungeonMapOverlayViewportRect() const;
    Vec2 dungeonMapOverlayMapSize(UiRect viewport) const;
    Vec2 dungeonMapOverlayMaxScroll() const;
    Vec2 dungeonMapOverlayPlayerCenteredScroll() const;
    UiRect dungeonMapOverlayVerticalScrollTrackRect() const;
    UiRect dungeonMapOverlayVerticalScrollThumbRect() const;
    UiRect dungeonMapOverlayHorizontalScrollTrackRect() const;
    UiRect dungeonMapOverlayHorizontalScrollThumbRect() const;
    void renderDungeonMinimap(Renderer& renderer, const std::vector<LightSource>& itemLights) const;
    void renderDungeonMapOverlay(Renderer& renderer, const std::vector<LightSource>& itemLights) const;
    void renderDungeonStatusHud(Renderer& renderer) const;
    void renderDungeonLogs(Renderer& renderer) const;
    void renderImportantDungeonNotices(Renderer& renderer) const;
    void renderDungeonControlHelp(Renderer& renderer) const;
    void renderWarpReturnUi(Renderer& renderer) const;
    void renderRoguelikeBigHoleUi(Renderer& renderer) const;
    void renderWorldLoadingScreen(Renderer& renderer, float totalSeconds) const;
    bool renderDeathResultPrelude(Renderer& renderer) const;
    void renderAstralEchoStarfield(Renderer& renderer, UiRect area, bool showConstellation, float alpha = 1.0f) const;
    void renderGameOverScreen(Renderer& renderer) const;
    void renderStageClearScreen(Renderer& renderer) const;
    void renderAstralResultScreen(Renderer& renderer) const;
    void renderBossDefeatPresentation(Renderer& renderer) const;
    void renderDungeonHitboxOverlay(Renderer& renderer, const Time& time) const;
    void renderBaseDebugOverlay(Renderer& renderer, const Time& time) const;
    void renderDebugOverlay(Renderer& renderer, const Time& time);
    void renderAutoSimulationIntentOverlay(Renderer& renderer) const;
    void beginDungeonRingIntro();
    void startDungeonRingIntroTimer();
    void updateDungeonRingIntro(float dt);
    bool dungeonRingIntroActive() const;
    float dungeonRingIntroProgress() const;
    void renderSpellRingForeground(
        Renderer& renderer,
        const std::vector<const SpellRingItem*>& runtimeItems,
        const std::vector<LightSource>& itemLights,
        float totalSeconds) const;
    void appendCaptureAbsorbRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
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
    MoneyGainFxSystem moneyGainFx_;
    EnemySystem enemies_;
    ProjectileSystem projectiles_;
    MagicSystem magic_;
    MagicFxSystem magicFx_;
    GroundLineSystem groundLines_;
    WetGroundSystem wetGround_;
    InventorySystem inventory_;
    RingPresetSystem ringPresets_;
    WorldDropSystem worldDrops_;
    EncyclopediaSystem encyclopedia_;
    InitializeJob initializeJob_;
    std::deque<AcquisitionNotice> itemAcquisitionNotices_;
    std::vector<CaptureAbsorbAnimation> captureAbsorbAnimations_;
    std::unordered_set<std::string> mainObtainedObjectIds_;
    std::unordered_set<std::string> mainCapturedEnemyIds_;
    std::unordered_map<std::string, int> encyclopediaOwnedSyncSuppressCounts_;
    std::unordered_map<std::string, int> encyclopediaRingSyncSuppressCounts_;
    LevelSystem levels_;
    UpgradeSystem upgrades_;
    LevelUpPresentationState levelUpPresentation_{};
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
    std::unordered_map<std::string, std::string> baseTalkSessionSelections_;
    std::unordered_map<std::string, int> hiddenBaseNpcHp_;
    std::unordered_map<std::string, float> hiddenBaseNpcHitCooldowns_;
    std::unordered_map<std::string, int> hiddenDungeonNpcRuntimeIds_;
    std::unordered_map<int, std::string> hiddenDungeonNpcTargetByRuntimeId_;
    std::unordered_set<std::string> hiddenDungeonNpcRemovedIds_;
    float basePlayerSpriteAnimationTime_ = 0.0f;
    float baseActorIdleAnimationTime_ = 0.0f;
    float baseRingPreviewAnimationTime_ = 0.0f;
    std::unordered_map<std::string, bool> baseNpcSpriteFlipHorizontal_;
    std::unordered_map<std::string, Vec2> baseStoryFacilityOffsets_;
    std::unordered_map<std::string, float> baseStoryMarkedFacilities_;
    struct BaseStoryCommandRuntime {
        int stepIndex = -1;
        std::string name;
        float elapsedSeconds = 0.0f;
        Vec2 startPosition{};
        Vec2 targetPosition{};
        Vec2 startFacing{0.0f, 1.0f};
        Vec2 targetFacing{0.0f, 1.0f};
    };
    struct BaseStoryChicoryFlightState {
        bool active = false;
        float elapsedSeconds = 0.0f;
        float durationSeconds = 0.0f;
        Vec2 startPosition{};
        Vec2 centerPosition{};
    };
    struct BaseStoryRingDemoState {
        bool active = false;
        bool closing = false;
        float elapsedSeconds = 0.0f;
        float durationSeconds = 0.0f;
        int visibleRingCount = 0;
        int itemRingIndex = 1;
        std::string itemObjectId;
    };
    BaseStoryCommandRuntime baseStoryCommand_;
    BaseStoryChicoryFlightState baseStoryChicoryFlight_;
    BaseStoryRingDemoState baseStoryRingDemo_;
    float baseStoryFadeAlpha_ = 0.0f;
    StoryPhoneSoundState storyPhoneSound_;
    StoryShakeCommandState storyShakeCommand_;
    bool basePlayerSpriteWalking_ = false;
    bool basePlayerSpriteFlipHorizontal_ = false;
    int baseMenuSelection_ = 0;
    bool baseMiningStartChoiceActive_ = false;
    int baseMiningStartSelection_ = 0;
    UiConfirmDialogState baseBrokenRingDepartureConfirm_{};
    bool baseWarpPointSelectActive_ = false;
    int baseWarpPointSelection_ = 0;
    bool baseStorageActive_ = false;
    StorageUiMode baseStorageMode_ = StorageUiMode::Closed;
    int baseStorageActionSelection_ = 0;
    int baseStorageBulkSelection_ = 0;
    int baseStorageDepositSource_ = 0;
    UiTabsState baseStorageDepositSourceTabs_{};
    int baseStorageDepositSelection_ = 0;
    int baseStorageWithdrawSelection_ = 0;
    int baseStorageWarehousePage_ = 0;
    UiQuantityDialogState baseQuantityDialog_{};
    BaseQuantityPending baseQuantityPending_{};
    UiCommandMenuState baseStorageCommandMenu_{};
    StorageQuantityOperation baseStorageCommandOperation_ = StorageQuantityOperation::None;
    StorageTransferTarget baseStorageCommandTarget_{};
    BatchItemSelectionState baseStorageBatchSelection_{};
    ItemGridInteractionController baseItemInteraction_{};
    BaseRingItemInteractionState baseRingItemInteraction_{};
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
    BatchItemSelectionState baseMerchantBulkSell_{};
    UiCommandMenuState baseMerchantBuyCommandMenu_{};
    int baseMerchantBuyCommandIndex_ = -1;
    bool baseUpgradeActive_ = false;
    int baseUpgradeSelection_ = 0;
    UiTabsState baseUpgradeTabs_{};
    UiResultDialogState baseResultDialog_{};
    UiConfirmDialogState baseRegenerateConfirm_{};
    UiConfirmDialogState baseRoguelikeDepartureConfirm_{};
    ProcessingUiMode baseProcessingUiMode_ = ProcessingUiMode::Closed;
    int baseProcessingActionSelection_ = 0;
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
    UiTabsState baseRingWorkshopUpgradeTabs_{};
    float baseRingWorkshopUpgradeScrollOffset_ = 0.0f;
    UiScrollAreaState baseRingWorkshopUpgradeScroll_{};
    std::optional<RingLevelUpgradeSelection> ringWorkshopRespecSource_;
    RingLevelUpgradePointTable ringWorkshopDraftUpgradePoints_{};
    bool baseBookshelfActive_ = false;
    BookshelfPage bookshelfPage_ = BookshelfPage::Menu;
    int bookshelfSelection_ = 0;
    float bookshelfScrollOffset_ = 0.0f;
    UiScrollAreaState bookshelfScrollState_{};
    UiCommandMenuState bookshelfEndingCommandMenu_{};
    bool baseDiaryActive_ = false;
    BaseDiaryMode baseDiaryMode_ = BaseDiaryMode::Confirm;
    int baseDiarySelection_ = 0;
    DiarySaveSummary baseDiarySummary_{};
    std::string baseDiaryMessage_;
    bool baseEditEnabled_ = false;
    BaseEditMode baseEditMode_ = BaseEditMode::None;
    std::unordered_map<std::string, BaseEditRect> baseFacilityRectsOutdoor_;
    std::unordered_map<std::string, BaseEditRect> baseFacilityRectsHome_;
    std::unordered_set<std::int64_t> baseBlockedTilesOutdoorLocked_;
    std::unordered_set<std::int64_t> baseBlockedTilesOutdoorUnlocked_;
    std::unordered_set<std::int64_t> baseBlockedTilesHomeLocked_;
    std::unordered_set<std::int64_t> baseBlockedTilesHomeUnlocked_;
    std::vector<BaseEditSnapshot> baseEditUndoStack_;
    std::vector<BaseEditSnapshot> baseEditRedoStack_;
    BaseEditPassabilityLayer baseEditPassabilityLayer_ = BaseEditPassabilityLayer::Locked;
    std::unordered_set<std::int64_t> baseEditPassabilityClipboard_;
    bool baseEditPassabilityClipboardValid_ = false;
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
    bool portraitExpressionEditEnabled_ = false;
    PortraitExpressionPickerState portraitExpressionPicker_{};
    std::unordered_map<std::string, float> objectImageScaleById_;
    std::unordered_map<std::string, float> otherImageScaleByKey_;
    HitboxCatalog hitboxes_;
    EnemyPlacementCatalog enemyPlacements_;
    EnemyShadowCatalog enemyShadows_;
    std::vector<std::string> objectImageScaleAllObjectIds_;
    std::vector<std::string> objectImageScaleObjectIds_;
    std::vector<std::string> otherImageScaleKeys_;
    std::vector<std::string> enemyHitboxAllEnemyIds_;
    std::vector<std::string> enemyHitboxEnemyIds_;
    std::vector<std::string> objectHitboxAllObjectIds_;
    std::vector<std::string> objectHitboxObjectIds_;
    std::vector<std::string> playerHitboxAllIds_;
    std::vector<std::string> playerHitboxIds_;
    std::vector<std::string> enemyPlacementAllEnemyIds_;
    std::vector<std::string> enemyPlacementEnemyIds_;
    std::vector<std::string> enemyShadowAllEnemyIds_;
    std::vector<std::string> enemyShadowEnemyIds_;
    std::vector<HitboxEditSnapshot> hitboxEditUndoStack_;
    std::vector<HitboxEditSnapshot> hitboxEditRedoStack_;
    std::vector<EnemyPlacementEditSnapshot> enemyPlacementEditUndoStack_;
    std::vector<EnemyPlacementEditSnapshot> enemyPlacementEditRedoStack_;
    std::vector<EnemyShadowEditSnapshot> enemyShadowEditUndoStack_;
    std::vector<EnemyShadowEditSnapshot> enemyShadowEditRedoStack_;
    UiTextInputState objectImageScaleSearchInput_;
    UiTextInputState enemyHitboxSearchInput_;
    UiTextInputState enemyPlacementSearchInput_;
    UiTextInputState enemyShadowSearchInput_;
    ScreenMode objectImageScaleReturnMode_ = ScreenMode::Playing;
    ScreenMode enemyHitboxEditReturnMode_ = ScreenMode::Playing;
    ScreenMode enemyPlacementEditReturnMode_ = ScreenMode::Playing;
    ScreenMode enemyShadowEditReturnMode_ = ScreenMode::Playing;
    ImageScaleEditTab imageScaleEditTab_ = ImageScaleEditTab::Objects;
    HitboxEditTab hitboxEditTab_ = HitboxEditTab::Enemies;
    HitboxDirection enemyHitboxDirection_ = HitboxDirection::Default;
    HitboxDirection enemyPlacementDirection_ = HitboxDirection::Default;
    bool enemyHitboxEditingWeakPoint_ = false;
    int objectImageScaleSelectedIndex_ = -1;
    int otherImageScaleSelectedIndex_ = -1;
    int enemyHitboxSelectedEnemyIndex_ = -1;
    int objectHitboxSelectedObjectIndex_ = -1;
    int playerHitboxSelectedIndex_ = 0;
    int enemyPlacementSelectedEnemyIndex_ = -1;
    int enemyShadowSelectedEnemyIndex_ = -1;
    int enemyHitboxSelectedCircleIndex_ = -1;
    float objectImageScaleScrollOffset_ = 0.0f;
    float otherImageScaleScrollOffset_ = 0.0f;
    float enemyHitboxScrollOffset_ = 0.0f;
    float objectHitboxScrollOffset_ = 0.0f;
    float playerHitboxScrollOffset_ = 0.0f;
    float enemyPlacementScrollOffset_ = 0.0f;
    float enemyShadowScrollOffset_ = 0.0f;
    bool objectImageScaleDirty_ = false;
    bool enemyHitboxDirty_ = false;
    bool enemyPlacementDirty_ = false;
    bool enemyShadowDirty_ = false;
    bool enemyHitboxDraggingCircle_ = false;
    bool enemyHitboxDragUndoSnapshotPushed_ = false;
    bool enemyPlacementDragging_ = false;
    bool enemyPlacementDragUndoSnapshotPushed_ = false;
    bool enemyShadowDragging_ = false;
    bool enemyShadowDragUndoSnapshotPushed_ = false;
    Vec2 enemyHitboxDragStartMouse_{};
    Vec2 enemyHitboxDragStartOffset_{};
    Vec2 enemyPlacementDragStartMouse_{};
    Vec2 enemyPlacementDragStartOffset_{};
    Vec2 enemyShadowDragStartMouse_{};
    Vec2 enemyShadowDragStartOffset_{};
    std::vector<HitCircle> enemyHitboxClipboard_;
    EnemyHitboxDirectionClipboard enemyHitboxAllDirectionClipboard_;
    Vec2 enemyPlacementOffsetClipboard_{};
    bool enemyPlacementOffsetClipboardValid_ = false;
    EnemyPlacementEntry enemyPlacementEntryClipboard_;
    bool enemyPlacementEntryClipboardValid_ = false;
    EnemyShadowSpec enemyShadowClipboard_;
    bool enemyShadowClipboardValid_ = false;
    std::string objectImageScaleStatus_;
    std::string enemyHitboxStatus_;
    std::string enemyPlacementStatus_;
    std::string enemyShadowStatus_;
    ScreenMode audioCueEditReturnMode_ = ScreenMode::Playing;
    AudioCueEditMode audioCueEditMode_ = AudioCueEditMode::Bgm;
    std::vector<AudioCueEditEntry> audioCueEditEntries_;
    std::vector<AudioCueFileEntry> audioCueEditFiles_;
    int audioCueEditCueIndex_ = -1;
    int audioCueEditFileIndex_ = -1;
    AudioCueEditEntry audioCueEditDraft_{};
    int audioCueEditDraftCueIndex_ = -1;
    AudioCueEditEntry audioCueEditClipboard_{};
    bool audioCueEditClipboardValid_ = false;
    float audioCueEditCueScrollOffset_ = 0.0f;
    float audioCueEditFileScrollOffset_ = 0.0f;
    UiScrollAreaState audioCueEditCueScrollState_{};
    UiScrollAreaState audioCueEditFileScrollState_{};
    UiSliderState audioCueEditVolumeSliderState_{};
    UiSliderState audioCueEditPitchSliderState_{};
    int audioCueEditLastFileClickIndex_ = -1;
    std::uint64_t audioCueEditLastFileClickTicks_ = 0;
    bool audioCueEditDirty_ = false;
    std::string audioCueEditStatus_;
    std::string audioCueEditPreviousBgmCue_;
    mutable UiCancelControlState audioCueEditCancelState_{};
    bool debugItemPickerActive_ = false;
    std::vector<std::string> debugItemPickerAllObjectIds_;
    std::vector<std::string> debugItemPickerObjectIds_;
    UiTextInputState debugItemPickerSearchInput_;
    int debugItemPickerSelectedIndex_ = -1;
    float debugItemPickerScrollOffset_ = 0.0f;
    std::string debugItemPickerStatus_;
    mutable UiCancelControlState debugItemPickerCancelState_{};
    DebugNamedSaveDialogMode debugNamedSaveDialogMode_ = DebugNamedSaveDialogMode::Closed;
    UiTextInputState debugNamedSaveInput_;
    std::string debugNamedSaveInputSnapshot_;
    std::vector<DebugNamedSaveEntry> debugNamedSaveEntries_;
    int debugNamedSaveSelectedIndex_ = -1;
    float debugNamedSaveScrollOffset_ = 0.0f;
    UiScrollAreaState debugNamedSaveScrollState_{};
    UiConfirmDialogState debugNamedSaveOverwriteConfirm_{};
    std::optional<DebugNamedSaveTarget> debugNamedSavePendingTarget_;
    std::string debugNamedSaveStatus_;
    mutable UiCancelControlState debugNamedSaveCancelState_{};
    std::optional<DebugRoguelikeRunSnapshot> debugRoguelikeRunSnapshot_;
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
    int debugPreviewBackgroundIndex_ = 0;
    bool effectTestActive_ = false;
    std::vector<const EffectPreviewEntry*> effectTestEntries_;
    std::vector<const EffectPreviewEntry*> effectTestVisibleEntries_;
    std::vector<std::string> effectTestTabKeys_;
    std::vector<std::string> effectTestTabLabels_;
    UiTabsState effectTestTabsState_{};
    int effectTestTabIndex_ = 0;
    int effectTestSelectedIndex_ = 0;
    float effectTestReplayTimerSeconds_ = 0.0f;
    float effectTestScrollOffset_ = 0.0f;
    UiScrollAreaState effectTestScrollState_{};
    MagicFxEmitterHandle effectTestEmitter_{};
    std::string effectTestStatus_;
    bool projectileTestActive_ = false;
    bool projectileTestTargetEnabled_ = false;
    std::vector<const ProjectileDefinition*> projectileTestEntries_;
    int projectileTestSelectedIndex_ = 0;
    float projectileTestReplayTimerSeconds_ = 0.0f;
    float projectileTestScrollOffset_ = 0.0f;
    UiScrollAreaState projectileTestScrollState_{};
    std::string projectileTestStatus_;
    std::function<GameSettings()> settingsGetter_;
    std::function<void(const GameSettings&)> settingsApplier_;
    std::function<InputBindingMap()> inputBindingGetter_;
    std::function<void(const InputBindingMap&)> inputBindingApplier_;
    bool lightweightModeActive_ = false;
    PresentationSettings presentationSettingsActive_{};
    float screenShakeTimer_ = 0.0f;
    float screenShakeDuration_ = 0.0f;
    float screenShakeAmplitude_ = 0.0f;
    unsigned int screenShakeSeed_ = 0;
    float playerDamageVignetteDanger_ = 0.0f;
    float playerDamageVignetteFlash_ = 0.0f;
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
    TitleMenuPage titleMenuPage_ = TitleMenuPage::Main;
    std::string titleCreditsText_;
    float titleCreditsScrollOffset_ = 0.0f;
    mutable float titleCreditsContentHeight_ = 0.0f;
    UiScrollAreaState titleCreditsScrollState_{};
    mutable UiCancelControlState titleCancelState_{};
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
    UiTabsState audioSettingsTabs_{};
    UiTabsState videoSettingsTabs_{};
    bool optionsSettingsLoaded_ = false;
    bool optionsSuppressCancelThisFrame_ = false;
    std::string optionsStatus_;
    UiTabsState operationSettingsTabs_{};
    UiSelectableTableState operationSettingsTable_{};
    int operationSettingsHoveredRow_ = -1;
    int operationSettingsHoveredColumn_ = -1;
    UiCommandMenuState operationSettingsCommandMenu_{};
    UiConfirmDialogState operationSettingsConflictConfirm_{};
    UiConfirmDialogState operationSettingsResetAllConfirm_{};
    UiResultDialogState operationSettingsReadOnlyDialog_{};
    InputRemapCapture operationSettingsCapture_{};
    InputBindingMap operationSettingsBindings_ = defaultInputBindings();
    std::vector<InputAction> operationSettingsConflictActions_;
    InputBinding operationSettingsPendingBinding_{};
    InputAction operationSettingsPendingAction_ = InputAction::Count;
    int operationSettingsPendingColumn_ = 0;
    OperationSettingsBindingEditMode operationSettingsPendingEditMode_ = OperationSettingsBindingEditMode::Replace;
    int operationSettingsCategory_ = 0;
    bool operationSettingsLoaded_ = false;
    mutable UiCancelControlState baseCancelState_{};
    mutable UiCancelControlState ringCancelState_{};
    UiTabsState ringTabs_{};
    enum class RingPresetMenuAction {
        None,
        Apply,
        Register,
    };
    UiCommandMenuState ringPresetMenu_{};
    RingPresetMenuAction ringPresetMenuAction_ = RingPresetMenuAction::None;
    UiCommandMenuState ringCommandMenu_{};
    int ringCommandItemIndex_ = -1;
    bool ringCommandPlaceActive_ = false;
    float ringCommandPlaceAngle_ = 0.0f;
    UiConfirmDialogState ringDiscardConfirm_{};
    int ringDiscardConfirmItemIndex_ = -1;
    bool ringPlaceModeActive_ = false;
    int ringPlaceSelection_ = 0;
    float ringPlaceTargetAngle_ = 0.0f;
    bool ringEmptyPressActive_ = false;
    Vec2 ringEmptyPressMouse_{};
    float ringEmptyPressAngle_ = 0.0f;
    int ringSlotSelection_ = 0;
    bool ringDetailShowsRing_ = true;
    bool ringItemMoveModeActive_ = false;
    int ringItemMoveIndex_ = -1;
    float ringItemMoveOriginalAngle_ = 0.0f;
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
    bool bossEncounterRingHidden_ = false;
    bool stageStartStoryPendingAfterRingIntro_ = false;
    bool endingKamishibaiPending_ = false;
    EndingKind endingKamishibaiKind_ = EndingKind::Main;
    bool endingKamishibaiReplay_ = false;
    IntroTutorialPhase introTutorialPhase_ = IntroTutorialPhase::Inactive;
    bool introTutorialLightTutorialQueued_ = false;
    bool introTutorialFirstEnemySpawned_ = false;
    bool introTutorialSecondEnemySpawned_ = false;
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
    std::string introTutorialChestLootObjectId_;
    std::string introTutorialChestLootInstanceId_;
    DungeonTile introTutorialFirstEnemyTile_{};
    DungeonTile introTutorialSecondEnemyTile_{};
    DungeonTile introTutorialChestTile_{};
    DungeonTile introTutorialSecondChestTile_{};
    DungeonTile introTutorialExitTile_{};
    std::vector<DungeonLogEntry> dungeonLogs_;
    std::vector<DungeonLogEntry> importantDungeonNotices_;
    DungeonRouteDeviationState dungeonRouteDeviation_{};
    WorldBuildJob worldBuildJob_;
    std::array<FootstepDustPuff, 10> playerFootstepDustPuffs_{};
    std::vector<RingEquipFx> ringEquipFx_;
    int nextPlayerFootstepDustPuff_ = 0;
    int nextPlayerFootstepDustShape_ = 0;
    int previousPlayerDustFrame_ = -1;
    int previousBasePlayerDustFrame_ = -1;
    RunStats runStats_{};
    AutoSimulationCheckpointMeasurementState autoSimulationCheckpointMeasurement_{};
    double playerRegenPerSecond_ = 0.0;
    double playerRegenAccumulator_ = 0.0;
    std::vector<PlayerRegenSource> playerRegenSources_;
    PlayerDeathSequenceState playerDeathSequence_{};
    DeathResultPreludeState deathResultPrelude_{};
    int gameOverSelection_ = 0;
    std::string gameOverStatus_;
    bool bossSpawned_ = false;
    bool bossPreviewSpawned_ = false;
    BossEncounterState bossEncounter_{};
    DungeonStoryPresentationState dungeonStoryPresentation_{};
    int stageClearSelection_ = 0;
    std::string stageClearStatus_;
    AstralRunState astralRun_{};
    AstralRunSummary astralResult_{};
    int astralResultSelection_ = 0;
    int astralHighScore_ = 0;
    std::array<int, 3> capturedMonsterRingBreaksBeforeStageClear_{};
    RoguelikeBigHoleState roguelikeBigHole_{};
    UiCommandMenuState roguelikeBigHoleMenu_{};
    int focusedRoguelikeBigHole_ = 0;
    bool hoveredRoguelikeBigHole_ = false;
    std::vector<RoguelikeFacilityInstance> roguelikeFacilities_;
    std::array<int, 3> roguelikeFacilityLastDepthMeters_{};
    RoguelikeFacilityUiMode roguelikeFacilityUiMode_ = RoguelikeFacilityUiMode::None;
    std::string activeRoguelikeFacilityId_;
    int focusedRoguelikeFacilityIndex_ = -1;
    int hoveredRoguelikeFacilityIndex_ = -1;
    bool roguelikeMerchantStockSuspended_ = false;
    std::vector<MerchantProduct> suspendedMerchantStock_;
    int suspendedMerchantStockVersion_ = 0;
    int debugAstralDepthMeters_ = 0;
    int debugAstralDepthRank_ = 1;
    std::string debugAstralMoveTarget_ = "meters";
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
    int debugMoneyAddAmount_ = 10000;
    int debugMaterialAddAmount_ = 100;
    int debugRandomItemCount_ = 8;
    int debugHpValue_ = 1;
    int debugTargetLevel_ = 1;
    InventoryCarryState runStartInventoryState_{};
    InventoryCarryState roguelikeReturnInventoryState_{};
    RetrySnapshot retrySnapshot_{};
    std::unordered_map<std::string, DungeonState> dungeonStates_;
    DungeonMinimapCells dungeonMinimapCells_;
    bool dungeonMapOverlayOpen_ = false;
    Vec2 dungeonMapOverlayScroll_{};
    int dungeonMapOverlayScrollbarDragAxis_ = 0;
    float dungeonMapOverlayScrollbarDragOffset_ = 0.0f;
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
    std::vector<PendingBuriedEnemySpawn> pendingBuriedEnemySpawns_;
    DungeonEventSystem dungeonEvents_;
    float dungeonEventDiscoveryCooldown_ = 0.0f;
    std::optional<DungeonEventKind> pendingDebugDungeonEventPlacement_;
    std::string hoveredDungeonEventNpcId_;
    DungeonEventItemRequestUiState dungeonEventItemRequestUi_{};
    mutable UiCancelControlState dungeonEventItemRequestCancelState_{};
    int hoveredChestNodeIndex_ = -1;
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
    int hoveredWarpReturnPointIndex_ = -1;
    int disarmedWarpReturnPointIndex_ = -1;
    bool introTutorialExitHovered_ = false;
    int money_ = 0;
    double playTimeSeconds_ = 0.0;
    int maxHpUpgradeLevel_ = 0;
    int ringRadiusUpgradeLevel_ = 0;
    int ringSpeedUpgradeLevel_ = 0;
    int collectionRangeUpgradeLevel_ = 0;
    RingLevelUpgradePointTable levelRingUpgradePoints_{};
    struct RingWorkshopRingUpgrades {
        int radiusMaxLevel = 0;
        int radiusMinLevel = 0;
        int speedLevel = 0;
        int weightLimitLevel = 0;
        int shiftDistanceLevel = 0;
        int throwDistanceLevel = 0;
        int throwCooldownLevel = 0;
        int weightPenaltyLevel = 0;
        int equipSlotLevel = 0;
        float radiusSettingMeters = 0.0f;
    };
    std::array<RingWorkshopRingUpgrades, SpellRingCount> workshopRingUpgrades_{};
    bool merchantRefreshPending_ = false;
    int merchantUpgradeLevel_ = 1;
    int merchantStockVersion_ = 0;
    std::vector<MerchantProduct> merchantStock_;
    std::string highValueBuyCategory_;
    std::vector<std::string> highValueBuyObjectIds_;
    int warehouseCapacityLevel_ = 0;
    int processingUnlockLevel_ = 0;
    bool ringWorkshopUnlocked_ = false;
    int ringPresetSlotLevel_ = 0;
    bool autoSaveOnReturn_ = false;
    BaseMiningRescueDropState baseMiningRescueDrop_{};
    std::vector<std::string> storyFlags_;
    std::vector<InventoryObjectStack> warehouseObjectStacks_;
    std::vector<InventoryObjectInstance> warehouseObjectInstances_;
    mutable ItemSlotLayout warehouseItemLayout_;
    bool roguelikeDungeon_ = false;
    bool warpPointsEnabled_ = true;
    // Normal stages keep current inventory on death. Future roguelike runs can
    // flip this and restore the run-start snapshot instead; save I/O is separate.
    bool restoreRunStartInventoryOnDeath_ = false;
    bool roguelikeCarryInRestricted_ = false;
    bool roguelikeCarryOutRestricted_ = false;
    bool inventoryReturnToPause_ = false;
    bool ringReturnToPause_ = false;
    bool quitRequested_ = false;
    int astralEchoStarCount_ = 0;
    int astralEchoRecentStarIndex_ = -1;
    bool astralEchoRecentStarVisible_ = false;
    bool saveDataLoaded_ = false;
    bool testPlayMode_ = false;
    bool debugPaused_ = false;
    bool hitboxDisplayEnabled_ = false;
    bool autoReloadBlocked_ = false;
    bool hotReloadEnabled_ = false;
    DevBuildNoticeState devBuildNoticeState_ = DevBuildNoticeState::None;
    std::vector<std::string> devBuildNoticeChangeSummaries_;
    bool navigationUiCursorEnabled_ = false;
    float hotReloadPollTimer_ = 0.0f;
    AudioEngine* audio_ = nullptr;
    std::string activeAudioBgmCue_;
    AudioJingleState audioJingle_{};
    struct RingStatusHudAnimation {
        float previousCooldownRatio = 0.0f;
        float readyHoldTimer = 0.0f;
        float visibility = 1.0f;
        float pulseTimer = 0.0f;
        bool initialized = false;
    };
    std::array<RingStatusHudAnimation, SpellRingCount> ringStatusHudAnimations_{};
    float ringTrailEffectTimer_ = 0.0f;
    float ambientParticleTimer_ = 0.0f;
    float reloadNoticeTimer_ = 0.0f;
    std::string reloadNotice_;
};

}
