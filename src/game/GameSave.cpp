#include "game/GameInternal.hpp"

namespace majo {

namespace {

struct LoadedDungeonWarpPointSave {
    int index = 0;
    Vec2 position{};
    bool discovered = false;
    bool unlocked = false;
    bool snapshotCaptured = false;
};

struct LoadedDungeonStateSave {
    bool hasSeed = false;
    std::string stageId;
    int currentStage = 0;
    std::uint32_t seed = 0;
    std::vector<LoadedDungeonWarpPointSave> warpPoints;
};

bool isRoguelikeSaveStage(const StageDefinition& stage)
{
    return stage.id == "stage_04_astral_mine" ||
        stage.type == "ローグライク" ||
        stage.generationProfile == "astral_rogue";
}

bool hasSaveableDungeonLayout(const DungeonLayout& layout)
{
    return !layout.mainPathPoints.empty();
}

}

bool Game::loadSaveData()
{
    const std::filesystem::path path = saveDataPath();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        logError("[save] no save file: " + path.string());
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        logError("[warning] SaveData: empty or unreadable file; starting with new data");
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line != "MAJO_SHOVEL_SAVE_V1") {
        logError("[warning] SaveData: invalid header; starting with new data");
        return false;
    }

    InventorySystem loadedInventory;
    EncyclopediaSystem loadedEncyclopedia;
    std::array<std::vector<SpellRingItem>, SpellRingCount> loadedRingItemsByRing{};
    std::vector<InventoryObjectStack> loadedWarehouseStacks;
    std::vector<InventoryObjectInstance> loadedWarehouseInstances;
    int loadedMoney = 0;
    int loadedCurrentStage = 0;
    std::string loadedCurrentStageId = currentStageId_;
    int loadedUnlockedStages = 1;
    int loadedUnlockedWarpPointCount = 0;
    bool loadedHasLatestWarpPointPosition = false;
    Vec2 loadedLatestWarpPointPosition{};
    int loadedPlayerLevel = 1;
    int loadedPlayerXp = 0;
    int loadedPlayerXpToNext = balance_.xpBase + loadedPlayerLevel * balance_.xpPerLevel;
    int loadedMaxHpUpgradeLevel = 0;
    int loadedRingRadiusUpgradeLevel = 0;
    int loadedRingSpeedUpgradeLevel = 0;
    int loadedCollectionRangeUpgradeLevel = 0;
    int loadedLevelRingRadiusPoints = 0;
    int loadedLevelRingSpeedPoints = 0;
    int loadedLevelRingWeightLimitPoints = 0;
    int loadedWorkshopInitialRadiusLevel = 0;
    int loadedWorkshopInitialSpeedLevel = 0;
    int loadedWorkshopShiftDistanceLevel = 0;
    bool loadedMerchantRefreshPending = false;
    int loadedMerchantUpgradeLevel = 1;
    int loadedMerchantStockVersion = 0;
    std::vector<MerchantProduct> loadedMerchantStock;
    std::string loadedHighValueBuyCategory;
    std::vector<std::string> loadedHighValueBuyObjectIds;
    int loadedWarehouseCapacityLevel = 0;
    int loadedProcessingUnlockLevel = 0;
    bool loadedRingWorkshopUnlocked = false;
    bool loadedAutoSaveOnReturn = false;
    std::vector<std::string> loadedStoryFlags;
    int warningCount = 0;
    LoadedDungeonStateSave loadedDungeonState;
    std::array<RingShape, SpellRingCount> loadedRingShapes{};
    for (int i = 0; i < SpellRingCount; ++i) {
        loadedRingShapes[static_cast<std::size_t>(i)] = defaultRingShapeForIndex(i);
    }

    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string key;
        stream >> key;
        if (key.empty()) {
            continue;
        }
        if (key == "money") {
            stream >> loadedMoney;
        } else if (key == "player_level") {
            stream >> loadedPlayerLevel;
        } else if (key == "player_xp") {
            stream >> loadedPlayerXp;
        } else if (key == "player_xp_to_next") {
            stream >> loadedPlayerXpToNext;
        } else if (key == "upgrade_max_hp") {
            stream >> loadedMaxHpUpgradeLevel;
        } else if (key == "upgrade_ring_radius") {
            stream >> loadedRingRadiusUpgradeLevel;
        } else if (key == "upgrade_ring_speed") {
            stream >> loadedRingSpeedUpgradeLevel;
        } else if (key == "upgrade_collection_range") {
            stream >> loadedCollectionRangeUpgradeLevel;
        } else if (key == "level_ring_radius_points") {
            stream >> loadedLevelRingRadiusPoints;
        } else if (key == "level_ring_speed_points") {
            stream >> loadedLevelRingSpeedPoints;
        } else if (key == "level_ring_weight_limit_points") {
            stream >> loadedLevelRingWeightLimitPoints;
        } else if (key == "workshop_initial_radius_level") {
            stream >> loadedWorkshopInitialRadiusLevel;
        } else if (key == "workshop_initial_speed_level") {
            stream >> loadedWorkshopInitialSpeedLevel;
        } else if (key == "workshop_shift_distance_level") {
            stream >> loadedWorkshopShiftDistanceLevel;
        } else if (key == "merchant_refresh_pending") {
            stream >> loadedMerchantRefreshPending;
        } else if (key == "merchant_upgrade_level") {
            stream >> loadedMerchantUpgradeLevel;
        } else if (key == "merchant_stock_version") {
            stream >> loadedMerchantStockVersion;
        } else if (key == "merchant_stock") {
            std::string objectId;
            int price = 0;
            int quantity = 1;
            stream >> objectId >> price;
            if (!stream.fail()) {
                if (!(stream >> quantity)) {
                    quantity = 1;
                }
                if (objectCatalog_.registry.findById(objectId) == nullptr) {
                    ++warningCount;
                    logError("[warning] SaveData: merchant_stock object_id=\"" + objectId + "\" is missing from Objects DB; keeping ID");
                }
                loadedMerchantStock.push_back(MerchantProduct{objectId, std::max(1, price), std::max(0, quantity)});
            }
        } else if (key == "high_value_buy_category") {
            stream >> loadedHighValueBuyCategory;
            if (loadedHighValueBuyCategory == "-") {
                loadedHighValueBuyCategory.clear();
            }
        } else if (key == "high_value_buy_object") {
            std::string objectId;
            stream >> objectId;
            if (!stream.fail() && !objectId.empty()) {
                if (objectCatalog_.registry.findById(objectId) == nullptr) {
                    ++warningCount;
                    logError("[warning] SaveData: high_value_buy_object object_id=\"" + objectId + "\" is missing from Objects DB; keeping ID");
                }
                loadedHighValueBuyObjectIds.push_back(std::move(objectId));
            }
        } else if (key == "warehouse_capacity_level") {
            stream >> loadedWarehouseCapacityLevel;
        } else if (key == "processing_unlock_level") {
            stream >> loadedProcessingUnlockLevel;
        } else if (key == "ring_workshop_unlocked") {
            stream >> loadedRingWorkshopUnlocked;
        } else if (key == "auto_save_on_return") {
            stream >> loadedAutoSaveOnReturn;
        } else if (key == "story_flag") {
            std::string flag;
            stream >> flag;
            if (!stream.fail() && !flag.empty()) {
                loadedStoryFlags.push_back(std::move(flag));
            }
        } else if (key == "codex_entry") {
            std::string kindName;
            std::string id;
            int stage = 0;
            stream >> kindName >> id >> stage;
            EncyclopediaKind kind = EncyclopediaKind::Item;
            if (!stream.fail() && encyclopediaKindFromSaveName(kindName, kind)) {
                loadedEncyclopedia.loadEntry(kind, std::move(id), encyclopediaStageFromInt(stage));
            }
        } else if (key == "codex_effect") {
            std::string objectId;
            std::string effectKey;
            stream >> objectId >> effectKey;
            if (!stream.fail()) {
                loadedEncyclopedia.loadEffect(std::move(objectId), std::move(effectKey));
            }
        } else if (key == "current_stage") {
            stream >> loadedCurrentStage;
        } else if (key == "current_stage_id") {
            stream >> loadedCurrentStageId;
        } else if (key == "unlocked_stages") {
            stream >> loadedUnlockedStages;
        } else if (key == "unlocked_warp_points") {
            stream >> loadedUnlockedWarpPointCount;
        } else if (key == "latest_warp") {
            stream >> loadedLatestWarpPointPosition.x >> loadedLatestWarpPointPosition.y;
            loadedHasLatestWarpPointPosition = !stream.fail();
        } else if (key == "dungeon_seed") {
            std::string stageId;
            int currentStage = 0;
            std::uint32_t seed = 0;
            stream >> stageId >> currentStage >> seed;
            if (!stream.fail() && !stageId.empty()) {
                loadedDungeonState.hasSeed = true;
                loadedDungeonState.stageId = std::move(stageId);
                loadedDungeonState.currentStage = std::max(0, currentStage);
                loadedDungeonState.seed = seed;
            }
        } else if (key == "dungeon_warp_point") {
            std::string stageId;
            LoadedDungeonWarpPointSave point;
            stream >> stageId
                >> point.index
                >> point.position.x
                >> point.position.y
                >> point.discovered
                >> point.unlocked
                >> point.snapshotCaptured;
            if (!stream.fail() && !stageId.empty()) {
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    loadedDungeonState.warpPoints.push_back(point);
                }
            }
        } else if (key == "object") {
            std::string objectId;
            int count = 0;
            stream >> objectId >> count;
            loadedInventory.setObjectItemCount(objectCatalog_, objectId, count);
            if (!stream.fail() && objectCatalog_.registry.findById(objectId) == nullptr) {
                ++warningCount;
            }
        } else if (key == "object_instance") {
            ItemInstance instance;
            stream >> instance.instanceId
                >> instance.objectId
                >> instance.currentDurability
                >> instance.maxDurability
                >> instance.enhanceLevel
                >> instance.attackBonus
                >> instance.digBonus
                >> instance.durabilityBonus
                >> instance.weightModifier
                >> instance.sizeModifier
                >> instance.protectionEnabled
                >> instance.isBroken;
            if (!stream.fail()) {
                if (objectCatalog_.registry.findById(instance.objectId) == nullptr) {
                    ++warningCount;
                }
                loadedInventory.addObjectInstance(objectCatalog_, std::move(instance));
            }
        } else if (key == "warehouse_object") {
            std::string objectId;
            int count = 0;
            stream >> objectId >> count;
            const ItemData* item = objectCatalog_.registry.findById(objectId);
            if (!stream.fail() && count > 0) {
                if (item != nullptr) {
                    loadedWarehouseStacks.push_back(InventoryObjectStack{*item, count});
                } else {
                    ++warningCount;
                    logError("[warning] SaveData: warehouse_object object_id=\"" + objectId + "\" is missing from Objects DB; restored as missing stack item");
                    loadedWarehouseStacks.push_back(InventoryObjectStack{makeMissingItemData(objectId), count});
                }
            }
        } else if (key == "warehouse_object_instance") {
            ItemInstance instance;
            stream >> instance.instanceId
                >> instance.objectId
                >> instance.currentDurability
                >> instance.maxDurability
                >> instance.enhanceLevel
                >> instance.attackBonus
                >> instance.digBonus
                >> instance.durabilityBonus
                >> instance.weightModifier
                >> instance.sizeModifier
                >> instance.protectionEnabled
                >> instance.isBroken;
            const ItemData* item = objectCatalog_.registry.findById(instance.objectId);
            if (!stream.fail()) {
                if (item == nullptr) {
                    ++warningCount;
                    logError("[warning] SaveData: warehouse_object_instance object_id=\"" + instance.objectId + "\" is missing from Objects DB; restored as missing ItemInstance");
                }
                loadedWarehouseInstances.push_back(InventoryObjectInstance{
                    .item = item != nullptr ? *item : makeMissingItemData(instance.objectId),
                    .instance = std::move(instance),
                });
            }
        } else if (key == "material") {
            std::string materialId;
            int count = 0;
            stream >> materialId >> count;
            MaterialType materialType = MaterialType::Count;
            if (!stream.fail() && materialTypeFromSaveName(materialId, materialType)) {
                loadedInventory.setMaterialCount(materialType, count);
            }
        } else if (key == "ring_shape_1" || key == "ring_shape_2" || key == "ring_shape_3") {
            std::string shapeValue;
            stream >> shapeValue;
            if (!stream.fail()) {
                int index = 0;
                if (key == "ring_shape_2") {
                    index = 1;
                } else if (key == "ring_shape_3") {
                    index = 2;
                }
                loadedRingShapes[static_cast<std::size_t>(index)] = parseRingShapeValue(shapeValue, defaultRingShapeForIndex(index));
            }
        } else if (key == "ring_shape") {
            int index = 0;
            std::string shapeValue;
            stream >> index >> shapeValue;
            if (!stream.fail() && index >= 1 && index <= SpellRingCount) {
                const int ringIndex = index - 1;
                loadedRingShapes[static_cast<std::size_t>(ringIndex)] = parseRingShapeValue(shapeValue, defaultRingShapeForIndex(ringIndex));
            }
        } else if (key == "ring") {
            // Legacy per-item record. Ring shape is stored separately in ring_shape_1..3.
            int type = 0;
            std::string objectId;
            std::string damageType;
            SpellRingItem item;
            stream >> type >> objectId >> item.damage >> damageType >> item.digPower
                >> item.durability >> item.maxDurability >> item.weight
                >> item.hitRadius >> item.hitInterval >> item.localAngle;
            if (!stream.fail()) {
                item.type = ringTypeFromInt(type);
                item.objectId = loadRingObjectId(objectId);
                item.damageType = damageType;
                const std::string normalizedDamageType = normalizeDamageType(item.damageType);
                if (normalizedDamageType.empty()) {
                    if (item.damageType == "physical") {
                        ++warningCount;
                        logError("[warning] SaveData: ring damageType physical is deprecated; using blunt");
                        item.damageType = "blunt";
                    } else {
                        ++warningCount;
                        logError("[warning] SaveData: ring damageType \"" + item.damageType + "\" is invalid; using none");
                        item.damageType = "none";
                    }
                } else {
                    if (item.damageType == "physical" && normalizedDamageType == "blunt") {
                        ++warningCount;
                        logError("[warning] SaveData: ring damageType physical is deprecated; using blunt");
                    }
                    item.damageType = normalizedDamageType;
                }
                item.objectStatsApplied = false;
                stream >> item.instanceId
                    >> item.enhanceLevel
                    >> item.attackBonus
                    >> item.digBonus
                    >> item.durabilityBonus
                    >> item.weightModifier
                    >> item.sizeModifier
                    >> item.protectionEnabled
                    >> item.isBroken;
                if (item.instanceId == "-") {
                    item.instanceId.clear();
                }
                int loadedRingIndex = 0;
                stream >> loadedRingIndex;
                if (stream.fail()) {
                    loadedRingIndex = 0;
                    stream.clear();
                }
                loadedRingIndex = std::clamp(loadedRingIndex, 0, SpellRingCount - 1);
                item.ringIndex = loadedRingIndex;
                loadedRingItemsByRing[static_cast<std::size_t>(loadedRingIndex)].push_back(item);
            }
        }
    }

    inventory_ = loadedInventory;
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    if (loadedRingShapes[0] == RingShape::Circle &&
        loadedRingShapes[1] == RingShape::Circle &&
        loadedRingShapes[2] == RingShape::Circle) {
        for (int i = 0; i < SpellRingCount; ++i) {
            loadedRingShapes[static_cast<std::size_t>(i)] = defaultRingShapeForIndex(i);
        }
    }
    for (int i = 0; i < SpellRingCount; ++i) {
        spellRing_.setRingShapeForIndex(i, loadedRingShapes[static_cast<std::size_t>(i)]);
    }
    spellRing_.ringItems() = std::move(loadedRingItemsByRing);
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.normalizeItemPlacements();
    observeRingItemInstanceIds();
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    money_ = std::max(0, loadedMoney);
    unlockedStages_ = std::max(1, loadedUnlockedStages);
    unlockedWarpPointCount_ = std::max(0, loadedUnlockedWarpPointCount);
    hasLatestWarpPointPosition_ = loadedHasLatestWarpPointPosition;
    latestWarpPointPosition_ = loadedHasLatestWarpPointPosition
        ? loadedLatestWarpPointPosition
        : latestWarpPointStartPosition();
    currentStage_ = std::max(0, loadedCurrentStage);
    if (!loadedCurrentStageId.empty()) {
        currentStageId_ = loadedCurrentStageId;
    }
    const std::string loadedStageIdForWarpState = currentStageId_;
    clampCurrentStageToSelectableStages();
    bool restoredDungeonStateFromSave = false;
    if (loadedDungeonState.hasSeed &&
        !loadedDungeonState.stageId.empty() &&
        loadedDungeonState.stageId == currentStageId_ &&
        !isRoguelikeSaveStage(currentStageDefinition())) {
        DungeonGenerationContext context = makeDungeonGenerationContext();
        context.seed = loadedDungeonState.seed;
        dungeonLayout_ = generateDungeonLayout(context);
        tileMap_ = TileMap{};
        runStats_ = RunStats{};
        enemies_ = EnemySystem{};
        worldDrops_ = WorldDropSystem{};
        worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
        rewardNodes_.clear();
        moneyNodes_.clear();
        moonFragmentNodes_.clear();
        chestNodes_.clear();
        crateNodes_.clear();
        enemyNodes_.clear();
        spawnedWarpPointCount_ = 0;
        bossSpawnPoint_ = {};
        hasBossSpawnPoint_ = false;
        bossSpawned_ = false;

        resetWarpPointRunState();
        if (!loadedDungeonState.warpPoints.empty()) {
            for (WarpPoint& point : warpPoints_) {
                point.discovered = false;
                point.unlocked = false;
                point.snapshotCaptured = false;
            }
            for (const LoadedDungeonWarpPointSave& savedPoint : loadedDungeonState.warpPoints) {
                auto pointIt = std::find_if(warpPoints_.begin(), warpPoints_.end(), [&](const WarpPoint& point) {
                    return point.index == savedPoint.index;
                });
                if (pointIt == warpPoints_.end()) {
                    continue;
                }
                pointIt->discovered = savedPoint.discovered;
                pointIt->unlocked = savedPoint.unlocked || savedPoint.discovered;
                pointIt->snapshotCaptured = savedPoint.snapshotCaptured || savedPoint.discovered;
            }
            unlockedWarpPointCount_ = discoveredWarpPointCount();
        }

        if (loadedHasLatestWarpPointPosition) {
            hasLatestWarpPointPosition_ = true;
            latestWarpPointPosition_ = loadedLatestWarpPointPosition;
        } else {
            hasLatestWarpPointPosition_ = false;
            latestWarpPointPosition_ = {};
            for (const WarpPoint& point : warpPoints_) {
                if (point.discovered) {
                    hasLatestWarpPointPosition_ = true;
                    latestWarpPointPosition_ = point.position;
                }
            }
        }

        initializeMoonFragmentNodesFromWarpPoints();
        initializeRewardNodesFromLayout();
        initializeChestNodesFromLayout();
        initializeCrateNodesFromLayout();
        initializeEnemyNodesFromLayout();
        applyPlacementTerrainOverrides();
        captureDungeonState();
        syncWarpStateForCurrentStage();
        restoredDungeonStateFromSave = true;
    }
    if (!restoredDungeonStateFromSave && currentStageId_ != loadedStageIdForWarpState) {
        unlockedWarpPointCount_ = 0;
        hasLatestWarpPointPosition_ = false;
        latestWarpPointPosition_ = {};
    }
    player_.level = std::max(1, loadedPlayerLevel);
    player_.xp = std::max(0, loadedPlayerXp);
    player_.xpToNext = std::max(1, loadedPlayerXpToNext);
    maxHpUpgradeLevel_ = std::max(0, loadedMaxHpUpgradeLevel);
    ringRadiusUpgradeLevel_ = std::max(0, loadedRingRadiusUpgradeLevel);
    ringSpeedUpgradeLevel_ = std::max(0, loadedRingSpeedUpgradeLevel);
    collectionRangeUpgradeLevel_ = std::clamp(loadedCollectionRangeUpgradeLevel, 0, 5);
    levelRingRadiusPoints_ = std::max(0, loadedLevelRingRadiusPoints);
    levelRingSpeedPoints_ = std::max(0, loadedLevelRingSpeedPoints);
    levelRingWeightLimitPoints_ = std::max(0, loadedLevelRingWeightLimitPoints);
    workshopInitialRadiusLevel_ = std::clamp(loadedWorkshopInitialRadiusLevel, 0, 5);
    workshopInitialSpeedLevel_ = std::clamp(loadedWorkshopInitialSpeedLevel, 0, 5);
    workshopShiftDistanceLevel_ = std::clamp(loadedWorkshopShiftDistanceLevel, 0, 5);
    merchantRefreshPending_ = loadedMerchantRefreshPending;
    merchantUpgradeLevel_ = std::clamp(loadedMerchantUpgradeLevel, 1, 7);
    merchantStockVersion_ = std::max(0, loadedMerchantStockVersion);
    merchantStock_ = std::move(loadedMerchantStock);
    highValueBuyCategory_ = std::move(loadedHighValueBuyCategory);
    highValueBuyObjectIds_ = std::move(loadedHighValueBuyObjectIds);
    warehouseCapacityLevel_ = std::clamp(loadedWarehouseCapacityLevel, 0, 4);
    processingUnlockLevel_ = std::clamp(loadedProcessingUnlockLevel, 0, 5);
    ringWorkshopUnlocked_ = loadedRingWorkshopUnlocked;
    autoSaveOnReturn_ = loadedAutoSaveOnReturn;
    storyFlags_ = std::move(loadedStoryFlags);
    encyclopedia_ = std::move(loadedEncyclopedia);
    warehouseObjectStacks_ = std::move(loadedWarehouseStacks);
    warehouseObjectInstances_ = std::move(loadedWarehouseInstances);
    applyPermanentUpgrades();
    resetRingWorkshopDraft();
    syncEncyclopediaFromInventoryAndRing();
    captureRunStartInventoryState();
    if (warningCount > 0) {
        logError("[warning] SaveData loaded with " + std::to_string(warningCount) + " warning(s)");
    }
    return true;
}

bool Game::saveSaveData(std::string& message) const
{
    const std::filesystem::path path = saveDataPath();
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            message = "保存先作成に失敗";
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        message = "セーブ失敗";
        return false;
    }

    file << "MAJO_SHOVEL_SAVE_V1\n";
    file << "money " << money_ << "\n";
    file << "player_level " << player_.level << "\n";
    file << "player_xp " << player_.xp << "\n";
    file << "player_xp_to_next " << player_.xpToNext << "\n";
    file << "upgrade_max_hp " << maxHpUpgradeLevel_ << "\n";
    file << "upgrade_ring_radius " << ringRadiusUpgradeLevel_ << "\n";
    file << "upgrade_ring_speed " << ringSpeedUpgradeLevel_ << "\n";
    file << "upgrade_collection_range " << collectionRangeUpgradeLevel_ << "\n";
    file << "level_ring_radius_points " << levelRingRadiusPoints_ << "\n";
    file << "level_ring_speed_points " << levelRingSpeedPoints_ << "\n";
    file << "level_ring_weight_limit_points " << levelRingWeightLimitPoints_ << "\n";
    file << "workshop_initial_radius_level " << workshopInitialRadiusLevel_ << "\n";
    file << "workshop_initial_speed_level " << workshopInitialSpeedLevel_ << "\n";
    file << "workshop_shift_distance_level " << workshopShiftDistanceLevel_ << "\n";
    file << "merchant_refresh_pending " << merchantRefreshPending_ << "\n";
    file << "merchant_upgrade_level " << merchantUpgradeLevel_ << "\n";
    file << "merchant_stock_version " << merchantStockVersion_ << "\n";
    for (const MerchantProduct& product : merchantStock_) {
        if (!product.objectId.empty()) {
            file << "merchant_stock " << product.objectId << " " << product.price << " " << product.quantity << "\n";
        }
    }
    for (const std::string& objectId : highValueBuyObjectIds_) {
        if (!objectId.empty()) {
            file << "high_value_buy_object " << objectId << "\n";
        }
    }
    file << "warehouse_capacity_level " << warehouseCapacityLevel_ << "\n";
    file << "processing_unlock_level " << processingUnlockLevel_ << "\n";
    file << "ring_workshop_unlocked " << ringWorkshopUnlocked_ << "\n";
    file << "auto_save_on_return " << autoSaveOnReturn_ << "\n";
    for (int i = 0; i < SpellRingCount; ++i) {
        file << "ring_shape_" << (i + 1) << " " << saveRingShapeName(spellRing_.ringShapeForIndex(i)) << "\n";
    }
    for (const std::string& flag : storyFlags_) {
        if (!flag.empty()) {
            file << "story_flag " << flag << "\n";
        }
    }
    for (const EncyclopediaEntrySave& entry : encyclopedia_.saveEntries()) {
        if (!entry.id.empty()) {
            file << "codex_entry "
                << encyclopediaKindSaveName(entry.kind) << " "
                << entry.id << " "
                << static_cast<int>(entry.stage) << "\n";
        }
    }
    for (const EncyclopediaEffectSave& effect : encyclopedia_.saveEffects()) {
        if (!effect.objectId.empty() && !effect.effectKey.empty()) {
            file << "codex_effect " << effect.objectId << " " << effect.effectKey << "\n";
        }
    }
    file << "current_stage " << currentStage_ << "\n";
    file << "current_stage_id " << currentStageId_ << "\n";
    file << "unlocked_stages " << unlockedStages_ << "\n";
    file << "unlocked_warp_points " << unlockedWarpPointCount_ << "\n";
    if (hasLatestWarpPointPosition_) {
        file << "latest_warp " << latestWarpPointPosition_.x << " " << latestWarpPointPosition_.y << "\n";
    }
    const DungeonLayout* saveDungeonLayout = nullptr;
    const std::vector<WarpPoint>* saveDungeonWarpPoints = nullptr;
    std::string saveDungeonStageId = currentStageId_;
    int saveDungeonCurrentStage = currentStage_;
    if (mode_ == ScreenMode::Playing &&
        !enemyTestActive_ &&
        hasSaveableDungeonLayout(dungeonLayout_) &&
        !isRoguelikeSaveStage(currentStageDefinition())) {
        saveDungeonLayout = &dungeonLayout_;
        saveDungeonWarpPoints = &warpPoints_;
    } else {
        const auto retainedStage = dungeonStates_.find(currentStageId_);
        if (retainedStage != dungeonStates_.end() &&
            retainedStage->second.valid &&
            hasSaveableDungeonLayout(retainedStage->second.dungeonLayout) &&
            !isRoguelikeSaveStage(currentStageDefinition())) {
            saveDungeonLayout = &retainedStage->second.dungeonLayout;
            saveDungeonWarpPoints = &retainedStage->second.warpPoints;
            saveDungeonStageId = retainedStage->second.currentStageId;
            saveDungeonCurrentStage = retainedStage->second.currentStage;
        }
    }
    if (saveDungeonLayout != nullptr && saveDungeonWarpPoints != nullptr && !saveDungeonStageId.empty()) {
        file << "dungeon_seed "
            << saveDungeonStageId << " "
            << saveDungeonCurrentStage << " "
            << saveDungeonLayout->seed << "\n";
        for (const WarpPoint& point : *saveDungeonWarpPoints) {
            file << "dungeon_warp_point "
                << saveDungeonStageId << " "
                << point.index << " "
                << point.position.x << " "
                << point.position.y << " "
                << point.discovered << " "
                << point.unlocked << " "
                << point.snapshotCaptured << "\n";
        }
    }
    for (const StackItem& stack : inventory_.stackItemsForSave()) {
        if (!stack.objectId.empty() && stack.count > 0) {
            file << "object " << stack.objectId << " " << stack.count << "\n";
        }
    }
    for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
        const ItemInstance& instance = objectInstance.instance;
        if (!instance.instanceId.empty() && !instance.objectId.empty()) {
            file << "object_instance "
                << instance.instanceId << " "
                << instance.objectId << " "
                << instance.currentDurability << " "
                << instance.maxDurability << " "
                << instance.enhanceLevel << " "
                << instance.attackBonus << " "
                << instance.digBonus << " "
                << instance.durabilityBonus << " "
                << instance.weightModifier << " "
                << instance.sizeModifier << " "
                << instance.protectionEnabled << " "
                << instance.isBroken << "\n";
        }
    }
    for (const InventoryObjectStack& stack : warehouseObjectStacks_) {
        if (!stack.objectId.empty() && stack.count > 0) {
            file << "warehouse_object " << stack.objectId << " " << stack.count << "\n";
        }
    }
    for (const InventoryObjectInstance& objectInstance : warehouseObjectInstances_) {
        const ItemInstance& instance = objectInstance.instance;
        if (!instance.instanceId.empty() && !instance.objectId.empty()) {
            file << "warehouse_object_instance "
                << instance.instanceId << " "
                << instance.objectId << " "
                << instance.currentDurability << " "
                << instance.maxDurability << " "
                << instance.enhanceLevel << " "
                << instance.attackBonus << " "
                << instance.digBonus << " "
                << instance.durabilityBonus << " "
                << instance.weightModifier << " "
                << instance.sizeModifier << " "
                << instance.protectionEnabled << " "
                << instance.isBroken << "\n";
        }
    }
    for (int index = 0; index < static_cast<int>(MaterialType::Count); ++index) {
        const MaterialType type = static_cast<MaterialType>(index);
        file << "material " << materialTypeSaveName(type) << " " << inventory_.materialCount(type) << "\n";
    }
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        for (const SpellRingItem& item : spellRing_.itemsForRing(ringIndex)) {
            file << "ring "
                << ringTypeToInt(item.type) << " "
                << saveRingObjectId(item) << " "
                << item.damage << " "
                << item.damageType << " "
                << item.digPower << " "
                << item.durability << " "
                << item.maxDurability << " "
                << item.weight << " "
                << item.hitRadius << " "
                << item.hitInterval << " "
                << item.localAngle << " "
                << (item.instanceId.empty() ? "-" : item.instanceId) << " "
                << item.enhanceLevel << " "
                << item.attackBonus << " "
                << item.digBonus << " "
                << item.durabilityBonus << " "
                << item.weightModifier << " "
                << item.sizeModifier << " "
                << item.protectionEnabled << " "
                << item.isBroken << " "
                << ringIndex << "\n";
        }
    }

    if (!file) {
        message = "セーブ書込に失敗";
        return false;
    }

    message = "セーブしました";
    return true;
}

} // namespace majo
