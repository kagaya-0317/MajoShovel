#include "game/GameInternal.hpp"

namespace majo {

namespace {

bool lootChestKindForDropProfile(std::string_view profile, LootChestKind& outKind)
{
    if (profile == "box_common") {
        outKind = LootChestKind::Common;
        return true;
    }
    if (profile == "box_rare") {
        outKind = LootChestKind::Rare;
        return true;
    }
    if (profile == "box_super") {
        outKind = LootChestKind::SuperRare;
        return true;
    }
    return false;
}

int lootDepthRankForWorldPosition(
    const TileMap& tileMap,
    const DungeonLayout& dungeonLayout,
    std::string_view stageId,
    Vec2 position)
{
    const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout, {
        static_cast<float>(tileMap.worldToTile(position.x)),
        static_cast<float>(tileMap.worldToTile(position.y)),
    });
    return lootDepthRankForProgress(stageId, metrics.pathProgress);
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
    loadStoryEvents();
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
    baseProcessingTabs_ = {};
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
    ringTabs_ = {};
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
    applyPlacementTerrainOverrides();
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
        applyPlacementTerrainOverrides();
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
    applyPlacementTerrainOverrides();
    if (job.useLatestWarpPoint) {
        const Vec2 warpStartPosition = latestWarpPointStartPosition();
        latestWarpPointPosition_ = warpStartPosition;
        hasLatestWarpPointPosition_ = true;
        rebuildUnlockedWarpPointsForStart(warpStartPosition);
        player_.position = safePlayerStartPosition(warpStartPosition);
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

void Game::loadStoryEvents()
{
    StoryEventLoader loader;
    StoryEventLoadResult result = loader.loadDirectory(storyEventDataDirectory());
    storyEvents_ = std::move(result.events);
    logInfo("[story] events loaded: " + std::to_string(storyEvents_.size()));
    for (const std::string& warning : result.warnings) {
        logWarning("[story] " + warning);
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
        requestScreenTransition(ScreenTransitionTarget::TitleToBase);
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
            if (!pendingStoryTrigger_.empty()) {
                std::string trigger = std::move(pendingStoryTrigger_);
                pendingStoryTrigger_.clear();
                startStoryEventForTrigger(trigger);
            }
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
    case ScreenTransitionTarget::TitleToBase:
        enterBase();
        pendingStoryTrigger_ = "title_base_enter";
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
    logError("Enemies spawn weight columns detected: " + std::to_string(enemyCatalog_.spawnWeightStats.detectedColumnCount));
    logError("Enemies spawn weighted enemies: " + std::to_string(enemyCatalog_.spawnWeightStats.weightedEnemyCount));
    logError("Enemies spawn weight warnings: " + std::to_string(enemyCatalog_.spawnWeightStats.warningCount));
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
            << " captured_tags=" << enemy.capturedTags.size()
            << " spawn_weights=" << enemy.spawnWeights.size();
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
                const int depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, tile.center);
                if (spawnWeightedObjectLoot(
                        LootChestKind::Common,
                        depthRank,
                        tile.center,
                        rng,
                        "DigItemLoot",
                        false,
                        LootSourceKind::DigItem)) {
                    runStats_.dugTilesSinceItemDrop = 0;
                }
            }
        }
        for (Vec2 rewardPosition : digging_.rewardDropRequests()) {
            std::mt19937& rng = lootRuntimeRng();
            const int depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, rewardPosition);
            spawnWeightedObjectLoot(
                LootChestKind::Common,
                depthRank,
                rewardPosition,
                rng,
                "CapturedRewardLoot",
                true,
                LootSourceKind::CapturedReward);
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
            std::vector<DugEnemySpawnPoint> randomEnemySpawnPoints;
            randomEnemySpawnPoints.reserve(randomEnemySpawnTiles.size());
            for (Vec2 spawnTile : randomEnemySpawnTiles) {
                randomEnemySpawnPoints.push_back(DugEnemySpawnPoint{
                    .tileCenter = spawnTile,
                    .depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, spawnTile),
                });
            }
            enemies_.spawnFromDugTiles(randomEnemySpawnPoints, tileMap_, player_.position, balance_, enemyCatalog_, currentStageId_);
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
                std::mt19937& rng = lootRuntimeRng();
                if (event.moneyDrop > 0) {
                    worldDrops_.spawnMoneyDrop(
                        event.moneyDrop,
                        scatterLootPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng));
                }
                const bool bossDeath = event.type == EnemyEventType::BossDeath;
                const float manaChance = bossDeath ? balance_.bossManaDropChance : balance_.enemyManaDropChance;
                const float moonChance = bossDeath ? balance_.bossMoonFragmentChance : balance_.enemyMoonFragmentChance;
                if (rollChance(manaChance, rng)) {
                    const int amount = bossDeath ? scaledLootAmount(std::uniform_int_distribution<int>(1, 3)(rng), 1.0f) : 1;
                    worldDrops_.spawnMaterialDrop(
                        MaterialType::ManaDrop,
                        amount,
                        scatterLootPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng));
                }
                if (rollChance(moonChance, rng)) {
                    const int amount = bossDeath ? scaledLootAmount(std::uniform_int_distribution<int>(1, 3)(rng), 1.0f) : 1;
                    worldDrops_.spawnMaterialDrop(
                        MaterialType::MoonFragment,
                        amount,
                        scatterLootPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng));
                }
                if (!event.enemyId.empty()) {
                    encyclopedia_.noteEnemyDefeated(event.enemyId, event.enemyName, event.position);
                }
                if (event.type == EnemyEventType::BossDeath) {
                    bossDefeated = true;
                }
            } else if (event.type == EnemyEventType::RewardDrop) {
                std::mt19937& rng = lootRuntimeRng();
                const int depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, event.position);
                spawnWeightedObjectLoot(
                    LootChestKind::Common,
                    depthRank,
                    event.position,
                    rng,
                    "CapturedRewardLoot",
                    true,
                    LootSourceKind::CapturedReward);
            } else if (event.type == EnemyEventType::ObjectDrop) {
                std::mt19937& rng = lootRuntimeRng();
                const int dropCount = std::max(1, event.objectDropCount);
                if (!event.objectDropProfile.empty()) {
                    LootChestKind chestKind = LootChestKind::Common;
                    if (!lootChestKindForDropProfile(event.objectDropProfile, chestKind)) {
                        logError("[warning] EnemyDropLoot: unknown drop profile \"" + event.objectDropProfile + "\"; no item drop");
                    } else {
                        const int depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, event.position);
                        for (int i = 0; i < dropCount; ++i) {
                            spawnWeightedObjectLoot(
                                chestKind,
                                depthRank,
                                event.position,
                                rng,
                                "EnemyDropLoot",
                                true,
                                LootSourceKind::EnemyDrop);
                        }
                    }
                } else if (!event.objectDropId.empty()) {
                    for (int i = 0; i < dropCount; ++i) {
                        worldDrops_.spawnObjectDrop(
                            objectCatalog_,
                            event.objectDropId,
                            scatterLootPosition(event.position, rng),
                            runStats_.elapsedSeconds,
                            makeWorldLootJumpMotion(event.position, rng));
                    }
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
    } else if (std::filesystem::path(changedPath).extension() == ".story") {
        loadStoryEvents();
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

} // namespace majo
