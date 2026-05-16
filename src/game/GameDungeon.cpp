#include "game/GameInternal.hpp"

namespace majo {

DungeonGenerationContext Game::makeDungeonGenerationContext() const
{
    const int stageId = currentStage_ + 1;
    // Future connection: pass currentStageDefinition() into DungeonLayout generation.
    // This pass keeps the existing one-stage vertical slice parameters unchanged.
    return DungeonGenerationContext{
        .stageId = stageId,
        .seed = makeDungeonSeed(stageId, roguelikeDungeon_),
        .stageHardnessMultiplier = stageHardnessMultiplierForStageId(stageId),
        .roguelike = roguelikeDungeon_,
    };
}

void Game::generateDungeonLayoutForRun()
{
    // Future connection: currentStageDefinition().goalDistanceTiles,
    // generationProfile, warpPointCount, and specialRoomCount become layout inputs here.
    dungeonLayout_ = generateDungeonLayout(makeDungeonGenerationContext());
}

void Game::refreshOrbitEffects()
{
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.clearOrbitModifiers();
    player_.status.removeModifiersBySourcePrefix("orbit:");
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

        EffectContext context;
        context.sourceObject = &objectIt->second;
        context.owner = &player_;
        context.orbit = &spellRing_;
        context.orbitItem = &item;
        context.effects = &effects_;
        context.encyclopedia = &encyclopedia_;
        context.position = item.worldPosition;
        context.triggerType = EffectTriggerType::Orbit;
        context.logUnimplementedEffects = false;
        effectDispatcher_.dispatchOrbitEffects(objectIt->second, context);
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
        const Vec2 outward = normalize(item.worldPosition - spellRing_.center());
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
            const bool affectMetal = targetTag.empty() || targetTag.find("metal") != std::string::npos;
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

void Game::updateAmbientParticleEffects(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    ringTrailEffectTimer_ -= dt;
    if (spellRing_.state() != SpellRingState::Normal && ringTrailEffectTimer_ <= 0.0f) {
        const Vec2 trailDirection = spellRing_.state() == SpellRingState::Thrown
            ? player_.facing
            : player_.position - spellRing_.center();
        effects_.spawnForegroundRingTrail(spellRing_.center(), trailDirection);
        const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
        for (const SpellRingItem* itemPtr : runtimeItems) {
            if (itemPtr == nullptr) {
                continue;
            }
            effects_.spawnForegroundRingTrail(itemPtr->worldPosition, trailDirection);
        }
        ringTrailEffectTimer_ = 0.055f;
    }

    ambientParticleTimer_ -= dt;
    if (ambientParticleTimer_ > 0.0f) {
        return;
    }
    ambientParticleTimer_ = 0.18f;

    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->broken()) {
            continue;
        }
        if (itemPtr->type == SpellRingItemType::Torch || itemPtr->lightRadius > 0.0f) {
            effects_.spawnForegroundTorchFlicker(itemPtr->worldPosition);
        }
        if (!itemPtr->objectId.empty() || !itemPtr->addedEffects.empty() || itemPtr->hiddenDetectionRadius > 0.0f || itemPtr->treasureDetectionRadius > 0.0f) {
            effects_.spawnForegroundSpecialItemGlimmer(itemPtr->worldPosition);
        }
    }

    enemies_.emitStatusParticles(effects_);

    if (warpPointsEnabled_) {
        for (const WarpPoint& point : warpPoints_) {
            if (point.discovered || point.unlocked) {
                effects_.spawnWarpCircle(point.position, false);
            }
        }
        if (hasBossSpawnPoint_ && !bossSpawned_) {
            effects_.spawnWarpCircle(bossSpawnPoint_, true);
        }
    }
}

void Game::handleCapturedExplosion(Vec2 position)
{
    effects_.spawnAreaPulse(position, 50.0f, {255, 128, 54, 190});
    const std::vector<DamagedTile> openedTiles = tileMap_.damageCircle(position, CapturedExplosionTileRadius, CapturedExplosionTileDamage);
    for (const DamagedTile& tile : openedTiles) {
        effects_.spawnTileBreak(tile.center, tile.type, tile.color);
        ++runStats_.dugTiles;
    }
    enemies_.applyCapturedExplosion(position, spellRing_, CapturedExplosionEnemyDamage);
}

void Game::resize(int width, int height)
{
    camera_.setViewport(width, height);
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
        pausePage_ = PauseMenuPage::Options;
        break;
    case 4:
        pausePage_ = PauseMenuPage::QuitConfirm;
        pauseConfirmSelection_ = 0;
        break;
    default:
        break;
    }
}

void Game::leavePausePage()
{
    if (pausePage_ == PauseMenuPage::Main) {
        mode_ = pauseReturnMode_;
        return;
    }

    pausePage_ = PauseMenuPage::Main;
}

void Game::openRingScreen()
{
    pausePage_ = PauseMenuPage::Main;
    mode_ = ScreenMode::Ring;
    const int maxIndex = std::max(0, static_cast<int>(spellRing_.items().size()) - 1);
    ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, maxIndex);
    ringDragPending_ = false;
    ringDragActive_ = false;
    ringSnapActive_ = false;
    ringDragItemIndex_ = -1;
    cancelRingGrab();
    ringStatus_.clear();
}

void Game::cancelRingGrab()
{
    if (!ringGrabActive_) {
        return;
    }

    ringDragPending_ = false;
    ringDragActive_ = false;
    ringSnapActive_ = false;
    ringDragItemIndex_ = -1;
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
    state.valid = true;
    return state;
}

void Game::restoreInventoryCarryState(const InventoryCarryState& state)
{
    if (!state.valid) {
        return;
    }

    inventory_ = state.inventory;
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    spellRing_.ringItems() = state.ringItemsByRing;
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.normalizeItemPlacements();
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
    player_.lastDamageSource = DamageSource::Unknown;
    player_.lastDamageEnemyName.clear();
    player_.damageFlash = 0.0f;
    player_.damageEvents.clear();
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
    if (unlockedWarpPointCount_ > BossWarpPointIndex) {
        configureBossSpawnPointFromWarp(latestPosition);
    }
}

void Game::retryAfterGameOver()
{
    const RetrySnapshot checkpoint = retrySnapshot_;
    if (checkpoint.valid) {
        initializeWorld(false);
        retrySnapshot_ = checkpoint;
        restoreRetrySnapshot();
        clearTemporaryPlayerState(true);
        mode_ = ScreenMode::Playing;
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
    projectiles_ = ProjectileSystem{};
    levels_ = LevelSystem{};
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    camera_.follow(player_.position, 1.0f);
    captureRunStartInventoryState();
    mode_ = ScreenMode::Playing;
}

void Game::returnToBaseAfterGameOver()
{
    returnToBaseFromNormalStage(false, true);
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

    const bool refreshMerchant = shouldRefreshMerchantOnReturn(stageCleared, died);
    merchantRefreshPending_ = merchantRefreshPending_ || refreshMerchant;
    clearTemporaryPlayerState(true);
    captureDungeonState();
    enemies_ = EnemySystem{};
    effects_ = EffectSystem{};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
    projectiles_ = ProjectileSystem{};
    worldDrops_ = WorldDropSystem{};
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    levels_ = LevelSystem{};
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    bossSpawned_ = false;
    hasBossSpawnPoint_ = false;
    retrySnapshot_ = RetrySnapshot{};
    warpPoints_.clear();
    spawnedWarpPointCount_ = 0;
    enterBase();
    baseStatus_ = refreshMerchant ? "帰還しました。商人ワゴン更新あり" : "帰還しました";
    if (autoSaveOnReturn_) {
        std::string message;
        if (saveSaveData(message)) {
            baseStatus_ += " / 自動保存";
        } else {
            baseStatus_ += " / " + message;
        }
    }
}

void Game::resetWarpPointRunState()
{
    hasBossSpawnPoint_ = false;
    retrySnapshot_ = RetrySnapshot{};
    warpPointsEnabled_ = !roguelikeDungeon_;
    initializeWarpPointsFromLayout();
}

void Game::captureDungeonState()
{
    if (enemyTestActive_ || roguelikeDungeon_ || currentStageId_.empty()) {
        return;
    }

    enemies_.clearTemporaryState();
    DungeonState state;
    state.valid = true;
    state.currentStage = currentStage_;
    state.currentStageId = currentStageId_;
    state.tileMap = tileMap_;
    state.dungeonLayout = dungeonLayout_;
    state.runStats = runStats_;
    state.warpPoints = warpPoints_;
    state.rewardNodes = rewardNodes_;
    state.moneyNodes = moneyNodes_;
    state.moonFragmentNodes = moonFragmentNodes_;
    state.chestNodes = chestNodes_;
    state.crateNodes = crateNodes_;
    state.enemyNodes = enemyNodes_;
    state.enemies = enemies_;
    state.worldDrops = worldDrops_;
    state.spawnedWarpPointCount = spawnedWarpPointCount_;
    state.bossSpawnPoint = bossSpawnPoint_;
    state.hasBossSpawnPoint = hasBossSpawnPoint_;
    state.bossSpawned = bossSpawned_;
    dungeonStates_[currentStageId_] = std::move(state);
}

bool Game::restoreDungeonState(bool useLatestWarpPoint)
{
    if (roguelikeDungeon_) {
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
    runStats_ = state.runStats;
    warpPoints_ = state.warpPoints;
    rewardNodes_ = state.rewardNodes;
    moneyNodes_ = state.moneyNodes;
    moonFragmentNodes_ = state.moonFragmentNodes;
    chestNodes_ = state.chestNodes;
    crateNodes_ = state.crateNodes;
    enemyNodes_ = state.enemyNodes;
    enemies_ = state.enemies;
    enemies_.clearTemporaryState();
    worldDrops_ = state.worldDrops;
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    spawnedWarpPointCount_ = state.spawnedWarpPointCount;
    bossSpawnPoint_ = state.bossSpawnPoint;
    hasBossSpawnPoint_ = state.hasBossSpawnPoint;
    bossSpawned_ = state.bossSpawned;
    warpPointsEnabled_ = true;
    retrySnapshot_ = RetrySnapshot{};
    effects_ = EffectSystem{};
    projectiles_ = ProjectileSystem{};
    levels_ = LevelSystem{};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;

    player_ = Player{};
    player_.xpToNext = balance_.xpBase + player_.level * balance_.xpPerLevel;
    player_.position = useLatestWarpPoint ? latestWarpPointStartPosition() : tileWorldCenter(dungeonLayout_.startTile);
    camera_.follow(player_.position, 1.0f);
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    return true;
}

bool Game::canRegenerateCurrentStage() const
{
    if (roguelikeDungeon_) {
        return false;
    }
    auto it = dungeonStates_.find(currentStageId_);
    const std::vector<WarpPoint>& points = it != dungeonStates_.end() && it->second.valid ? it->second.warpPoints : warpPoints_;
    if (points.empty()) {
        return unlockedWarpPointCount_ >= MaxWarpPointsPerRun;
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
        if (point.discovered) {
            continue;
        }
        if (distanceSquared(player_.position, point.position) <= WarpPointTouchRadius * WarpPointTouchRadius) {
            point.discovered = true;
            point.unlocked = true;
            unlockedWarpPointCount_ = std::max(unlockedWarpPointCount_, discoveredWarpPointCount());
            latestWarpPointPosition_ = point.position;
            hasLatestWarpPointPosition_ = true;
            point.snapshotCaptured = true;
            if (point.index == BossWarpPointIndex) {
                configureBossSpawnPointFromWarp(point.position);
            }
            captureRetrySnapshotAtWarpPoint();
            point.lightRevealTimer = 0.0f;
            point.lightRevealAnimating = true;
            reloadNotice_ = "ワープポイント発見";
            reloadNoticeTimer_ = 2.0f;
            pushDungeonLog("ワープポイントを発見", "warp_point");
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

    for (const WarpPoint& point : warpPoints_) {
        const int count = countDistribution(rng);
        for (int i = 0; i < count; ++i) {
            const bool buried = i % 2 == 1;
            const float radiusTiles = buried ? buriedRadiusDistribution(rng) : floorRadiusDistribution(rng);
            const Vec2 offset = fromAngle(angleDistribution(rng)) * radiusTiles;
            moonFragmentNodes_.push_back(MoonFragmentNode{
                .tile = roundDungeonTile(Vec2{
                    static_cast<float>(point.tilePosition.x),
                    static_cast<float>(point.tilePosition.y),
                } + offset),
                .visibility = buried ? PlacementVisibility::BuriedVisible : PlacementVisibility::Exposed,
                .collected = false,
            });
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
    std::uniform_int_distribution<int> signDist(0, 1);
    std::uniform_int_distribution<int> moneyDist(2, 8);
    const std::optional<std::string> fallbackObjectId = firstAvailableObjectId(objectCatalog_);

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

        RewardNode node;
        node.visibility = i % 3 == 0
            ? PlacementVisibility::Exposed
            : (i % 3 == 1 ? PlacementVisibility::BuriedVisible : PlacementVisibility::BuriedHidden);
        const float offsetTiles = node.visibility == PlacementVisibility::Exposed
            ? 0.0f
            : (node.visibility == PlacementVisibility::BuriedVisible ? 7.0f : 9.0f) + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.rewardKind = i % 4 == 0 ? "treasure" : "item";
        node.objectId = (i % 2 == 0) ? fallbackObjectId : std::nullopt;
        node.revealed = node.visibility == PlacementVisibility::Exposed;
        rewardNodes_.push_back(std::move(node));
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

        MoneyNode node;
        node.visibility = i % 3 == 0
            ? PlacementVisibility::Exposed
            : (i % 3 == 1 ? PlacementVisibility::BuriedVisible : PlacementVisibility::BuriedHidden);
        const float offsetTiles = node.visibility == PlacementVisibility::Exposed
            ? 0.0f
            : (node.visibility == PlacementVisibility::BuriedVisible ? 6.0f : 8.5f) + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.amount = std::max(1, moneyDist(rng) + dungeonLayout_.stageId * 2);
        moneyNodes_.push_back(node);
    }

    for (const SpecialRoomAnchor& room : dungeonLayout_.specialRoomAnchors) {
        const DungeonTile centerTile = roundDungeonTile(room.center);
        if (room.type == SpecialRoomType::CoinRoom) {
            for (int i = 0; i < 4; ++i) {
                const Vec2 offset = fromAngle(static_cast<float>(i) * Pi * 0.5f) * 2.0f;
                moneyNodes_.push_back(MoneyNode{
                    .tile = roundDungeonTile(room.center + offset),
                    .amount = std::max(4, moneyDist(rng) + dungeonLayout_.stageId * 4),
                    .visibility = i == 0 ? PlacementVisibility::Exposed : (i == 1 ? PlacementVisibility::BuriedVisible : PlacementVisibility::BuriedHidden),
                    .collected = false,
                });
            }
        } else if (room.type == SpecialRoomType::TreasureRoom) {
            rewardNodes_.push_back(RewardNode{
                .tile = centerTile,
                .visibility = PlacementVisibility::Exposed,
                .rewardKind = "treasure",
                .objectId = fallbackObjectId,
                .revealed = true,
                .spawned = false,
                .collected = false,
            });
            rewardNodes_.push_back(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{room.radius, 0.0f}),
                .visibility = PlacementVisibility::BuriedVisible,
                .rewardKind = "treasure",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
            rewardNodes_.push_back(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{-room.radius, 0.0f}),
                .visibility = PlacementVisibility::BuriedHidden,
                .rewardKind = "treasure",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
        } else if (room.type == SpecialRoomType::EnemyRoom) {
            rewardNodes_.push_back(RewardNode{
                .tile = roundDungeonTile(room.center + Vec2{0.0f, room.radius}),
                .visibility = PlacementVisibility::BuriedHidden,
                .rewardKind = "enemy_room_reward",
                .objectId = std::nullopt,
                .revealed = false,
                .spawned = false,
                .collected = false,
            });
        } else if (room.type == SpecialRoomType::OreRoom) {
            rewardNodes_.push_back(RewardNode{
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
        reloadNotice_ = node.objectId.has_value() ? "報酬を拾得" : "仮報酬を拾得";
        reloadNoticeTimer_ = 1.4f;
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
        reloadNotice_ = "金貨 +" + std::to_string(std::max(0, node.amount)) + "G";
        reloadNoticeTimer_ = 1.4f;
    }
}

void Game::revealRewardNodesFromOpenedTiles(const std::vector<Vec2>& openedTiles)
{
    if (openedTiles.empty()) {
        return;
    }

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
            reloadNotice_ = node.objectId.has_value() ? "埋まり報酬を発見" : "隠し報酬を発見";
            reloadNoticeTimer_ = 1.6f;
        }
        for (MoneyNode& node : moneyNodes_) {
            if (node.collected || node.visibility == PlacementVisibility::Exposed ||
                node.tile.x != tile.x || node.tile.y != tile.y) {
                continue;
            }
            worldDrops_.spawnMoneyDrop(node.amount, tileWorldCenter(node.tile), runStats_.elapsedSeconds);
            node.collected = true;
            reloadNotice_ = "埋まり金貨 +" + std::to_string(std::max(0, node.amount)) + "G";
            reloadNoticeTimer_ = 1.6f;
        }
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
        reloadNotice_ = "Moon fragment";
        reloadNoticeTimer_ = 1.4f;
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
            reloadNotice_ = "Moon fragment";
            reloadNoticeTimer_ = 1.4f;
        }
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
    std::uniform_int_distribution<int> signDist(0, 1);

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

        ChestNode node;
        node.visibility = i % 4 == 0
            ? PlacementVisibility::Exposed
            : (i % 4 == 1 ? PlacementVisibility::BuriedVisible : PlacementVisibility::BuriedHidden);
        const float offsetTiles = node.visibility == PlacementVisibility::Exposed
            ? (1.0f + sideJitter(rng) * 0.5f)
            : (node.visibility == PlacementVisibility::BuriedVisible ? 6.5f : 8.5f) + sideJitter(rng);
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.chestKind = rollChestKind(rng, progress);
        node.depthRank = lootDepthRankForProgress(currentStageId_, progress);
        node.revealed = node.visibility != PlacementVisibility::BuriedHidden;
        node.opened = false;
        node.lootSpawned = false;
        node.openingSeconds = 0.0f;
        chestNodes_.push_back(node);
    }
}

void Game::updateChestNodes(float dt, const Input& input)
{
    const bool interact = input.confirmPressed() || input.useItemPressed();
    const float interactRadiusSq = ChestInteractRadius * ChestInteractRadius;

    for (ChestNode& node : chestNodes_) {
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
            openChestNode(node);
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
            openChestNode(node);
        }
    }
}

bool Game::spawnWeightedObjectLoot(
    LootChestKind chestKind,
    int depthRank,
    Vec2 center,
    std::mt19937& rng,
    std::string_view sourceLabel,
    bool launchFromCenter)
{
    std::vector<const ObjectDefinition*> candidates;
    std::vector<double> weights;
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const double weight = lootWeightFor(object, currentStageId_, depthRank, chestKind);
        if (weight >= 1.0) {
            candidates.push_back(&object);
            weights.push_back(weight);
        }
    }

    if (candidates.empty()) {
        const std::string columnName = resolveLootWeightColumnName(currentStageId_, depthRank, chestKind);
        logError("[warning] " + std::string(sourceLabel) + ": no Objects candidates stage=\"" + currentStageId_ +
            "\" depth=" + std::to_string(depthRank) +
            " chest=" + chestKindCode(chestKind) +
            " column=\"" + columnName + "\"");
        return false;
    }

    std::discrete_distribution<int> itemDistribution(weights.begin(), weights.end());
    const int selected = itemDistribution(rng);
    const ObjectDefinition* object = candidates[static_cast<std::size_t>(selected)];
    const Vec2 target = scatterLootPosition(center, rng);
    return worldDrops_.spawnObjectDrop(
        objectCatalog_,
        object->id,
        target,
        runStats_.elapsedSeconds,
        launchFromCenter ? makeWorldLootJumpMotion(center, rng) : WorldDropSpawnMotion{});
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

    reloadNotice_ = "Chest opened";
    reloadNoticeTimer_ = 1.4f;
}

void Game::spawnChestLoot(ChestNode& node)
{
    if (node.lootSpawned) {
        return;
    }

    node.lootSpawned = true;
    const Vec2 center = tileWorldCenter(node.tile);
    std::mt19937 rng(
        dungeonLayout_.seed ^
        (static_cast<std::uint32_t>(node.tile.x) * 0x85EBCA6Bu) ^
        (static_cast<std::uint32_t>(node.tile.y) * 0xC2B2AE35u) ^
        (static_cast<std::uint32_t>(runStats_.elapsedSeconds * 1000.0f) * 0x27D4EB2Du));

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
        const Vec2 target = scatterLootPosition(center, rng);
        worldDrops_.spawnMoneyDrop(amount, target, runStats_.elapsedSeconds, makeWorldLootJumpMotion(center, rng));
    }

    std::bernoulli_distribution materialChance(balance_.lootMaterialChance);
    if (materialChance(rng)) {
        std::uniform_int_distribution<int> materialDistribution(1, 3);
        const int amount = scaledLootAmount(materialDistribution(rng), totalMultiplier);
        const Vec2 target = scatterLootPosition(center, rng);
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
    std::uniform_real_distribution<float> progressJitter(-0.030f, 0.030f);
    std::uniform_real_distribution<float> sideJitter(-1.6f, 1.6f);
    std::uniform_int_distribution<int> signDist(0, 1);

    for (int i = 0; i < CrateNodeCountPerRun; ++i) {
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
        crateNodes_.push_back(node);
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
    const Vec2 materialTarget = scatterLootPosition(center, rng);
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
        const Vec2 moneyTarget = scatterLootPosition(center, rng);
        worldDrops_.spawnMoneyDrop(
            amount,
            moneyTarget,
            runStats_.elapsedSeconds,
            makeWorldLootJumpMotion(center, rng));
    }

    std::bernoulli_distribution bonusChance(balance_.crateBonusChance);
    if (bonusChance(rng)) {
        spawnWeightedObjectLoot(LootChestKind::Common, node.depthRank, center, rng, "CrateBonusLoot", true);
    }

    reloadNotice_ = "Crate broken";
    reloadNoticeTimer_ = 1.2f;
}

std::vector<CollisionRect> Game::solidObjectCollisionRects() const
{
    std::vector<CollisionRect> rects;
    rects.reserve(chestNodes_.size() + crateNodes_.size());

    for (const ChestNode& node : chestNodes_) {
        if (!node.opened && (node.visibility != PlacementVisibility::Exposed || !node.revealed)) {
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

        EnemyNode node;
        node.placementType = i % 3 == 0 ? EnemyPlacementType::BuriedHidden : EnemyPlacementType::Exposed;
        const float offsetTiles = node.placementType == EnemyPlacementType::Exposed
            ? (1.0f + sideJitter(rng))
            : (8.0f + sideJitter(rng));
        node.tile = roundDungeonTile(anchor + side * offsetTiles);
        node.dangerTier = std::max(1, dungeonLayout_.stageId + (i % 4 == 0 ? 1 : 0));
        node.enemySpawnGroup = "default";
        enemyNodes_.push_back(std::move(node));
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

void Game::updateExposedEnemyNodes()
{
    const float spawnRadiusSq = ExposedEnemyNodeSpawnRadius * ExposedEnemyNodeSpawnRadius;
    for (EnemyNode& node : enemyNodes_) {
        if (node.spawned || node.placementType != EnemyPlacementType::Exposed) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (distanceSquared(center, player_.position) > spawnRadiusSq) {
            continue;
        }
        if (enemies_.spawnNodeEnemy(tileMap_, center, player_.position, balance_, enemyCatalog_, false)) {
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

        for (std::string_view orbitEffectKey : {"orbit_speed", "orbit_power", "damage_speed"}) {
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
            if (enemies_.spawnNodeEnemy(tileMap_, tileWorldCenter(node.tile), player_.position, balance_, enemyCatalog_, true, true)) {
                node.spawned = true;
                reloadNotice_ = "隠れ敵が出現";
                reloadNoticeTimer_ = 1.5f;
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

void Game::updateBossSpawn()
{
    if (!hasBossSpawnPoint_ || bossSpawned_ || !warpPointsEnabled_) {
        return;
    }
    if (distanceSquared(player_.position, bossSpawnPoint_) > BossSpawnTriggerRadius * BossSpawnTriggerRadius) {
        return;
    }

    bossSpawned_ = enemies_.spawnBossNear(tileMap_, bossSpawnPoint_, player_.position, balance_, enemyCatalog_);
    if (bossSpawned_) {
        reloadNotice_ = "深部の主が出現";
        reloadNoticeTimer_ = 2.0f;
    }
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
    retrySnapshot_.runStats = runStats_;
    retrySnapshot_.warpPoints = warpPoints_;
    retrySnapshot_.rewardNodes = rewardNodes_;
    retrySnapshot_.moneyNodes = moneyNodes_;
    retrySnapshot_.moonFragmentNodes = moonFragmentNodes_;
    retrySnapshot_.chestNodes = chestNodes_;
    retrySnapshot_.crateNodes = crateNodes_;
    retrySnapshot_.enemyNodes = enemyNodes_;
    retrySnapshot_.enemies = enemies_;
    retrySnapshot_.worldDrops = worldDrops_;
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
    player_.lastDamageSource = DamageSource::Unknown;
    player_.lastDamageEnemyName.clear();
    player_.status = EntityStatus{};

    tileMap_ = retrySnapshot_.tileMap;
    dungeonLayout_ = retrySnapshot_.dungeonLayout;
    restoreInventoryCarryState(retrySnapshot_.inventory);
    runStats_ = retrySnapshot_.runStats;
    warpPoints_ = retrySnapshot_.warpPoints;
    rewardNodes_ = retrySnapshot_.rewardNodes;
    moneyNodes_ = retrySnapshot_.moneyNodes;
    moonFragmentNodes_ = retrySnapshot_.moonFragmentNodes;
    chestNodes_ = retrySnapshot_.chestNodes;
    crateNodes_ = retrySnapshot_.crateNodes;
    enemyNodes_ = retrySnapshot_.enemyNodes;
    enemies_ = retrySnapshot_.enemies;
    enemies_.clearTemporaryState();
    worldDrops_ = retrySnapshot_.worldDrops;
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
    spawnedWarpPointCount_ = retrySnapshot_.spawnedWarpPointCount;
    unlockedWarpPointCount_ = retrySnapshot_.unlockedWarpPointCount;
    bossSpawnPoint_ = retrySnapshot_.bossSpawnPoint;
    hasBossSpawnPoint_ = retrySnapshot_.hasBossSpawnPoint;
    bossSpawned_ = retrySnapshot_.bossSpawned;
    effects_ = EffectSystem{};
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
    levels_ = LevelSystem{};
    inventoryReturnToPause_ = false;
    gameOverStatus_.clear();
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
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

void Game::maybeSpawnPlayerFootstepDust(Vec2 footAnchor, Vec2 movementDirection, bool walking, int frameIndex, int& previousFrame)
{
    const bool dustFrame = frameIndex == 3 || frameIndex == 6;
    if (walking && dustFrame && previousFrame != frameIndex) {
        spawnPlayerFootstepDust(footAnchor, movementDirection);
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

    if (hasBossSpawnPoint_ && !bossSpawned_) {
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

    for (const RewardNode& node : rewardNodes_) {
        if (node.collected) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
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
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
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
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
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
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
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
        if (!node.revealed && !node.opened) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
            continue;
        }
        if (node.visibility == PlacementVisibility::BuriedVisible && !node.opened) {
            entries.push_back(DepthRenderEntry{
                center.y,
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
            center.y,
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
