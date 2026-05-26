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

struct LoadedDungeonMinimapCellSave {
    int x = 0;
    int y = 0;
    TileType type = TileType::Empty;
};

struct LoadedRewardNodeSave {
    DungeonTile tile{};
    int visibility = 0;
    bool revealed = false;
    bool spawned = false;
    bool collected = false;
};

struct LoadedMoneyNodeSave {
    DungeonTile tile{};
    int visibility = 0;
    bool collected = false;
};

struct LoadedMoonFragmentNodeSave {
    DungeonTile tile{};
    int visibility = 0;
    bool collected = false;
};

struct LoadedChestNodeSave {
    DungeonTile tile{};
    int visibility = 0;
    bool revealed = false;
    bool opened = false;
    bool lootSpawned = false;
    float openingSeconds = 0.0f;
};

struct LoadedCrateNodeSave {
    DungeonTile tile{};
    bool destroyed = false;
};

struct LoadedEnemyNodeSave {
    DungeonTile tile{};
    int placementType = 0;
    bool spawned = false;
};

struct LoadedDungeonEventInstanceSave {
    std::string id;
    std::string kindName;
    DungeonTile centerTile{};
    DungeonTile focusTile{};
    float discoveryRadiusTiles = 5.0f;
    bool discovered = false;
    bool completed = false;
    bool rewardSpawned = false;
    float selfLightRadiusTiles = 4.0f;
    std::vector<std::string> spawnedEntityIds;
    std::string params;
    std::string data;
};

struct LoadedDungeonStateSave {
    bool hasSeed = false;
    std::string stageId;
    int currentStage = 0;
    std::uint32_t seed = 0;
    std::vector<LoadedDungeonWarpPointSave> warpPoints;
    std::vector<LoadedDungeonMinimapCellSave> minimapCells;
    std::vector<TerrainTileEdit> terrainEdits;
    std::vector<LoadedRewardNodeSave> rewardNodes;
    std::vector<LoadedMoneyNodeSave> moneyNodes;
    std::vector<LoadedMoonFragmentNodeSave> moonFragmentNodes;
    std::vector<LoadedChestNodeSave> chestNodes;
    std::vector<LoadedCrateNodeSave> crateNodes;
    std::vector<LoadedEnemyNodeSave> enemyNodes;
    std::vector<LoadedDungeonEventInstanceSave> dungeonEventInstances;
    std::vector<WorldDropItem> worldDrops;
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

std::vector<std::string> splitSaveList(std::string_view text)
{
    std::vector<std::string> values;
    if (text.empty() || text == "-") {
        return values;
    }
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t comma = text.find(',', begin);
        const std::size_t end = comma == std::string_view::npos ? text.size() : comma;
        if (end > begin) {
            values.emplace_back(text.substr(begin, end - begin));
        }
        if (comma == std::string_view::npos) {
            break;
        }
        begin = comma + 1;
    }
    return values;
}

std::string joinSaveList(const std::vector<std::string>& values)
{
    std::string joined;
    for (const std::string& value : values) {
        if (value.empty()) {
            continue;
        }
        if (!joined.empty()) {
            joined += ',';
        }
        joined += value;
    }
    return joined.empty() ? std::string("-") : joined;
}

std::vector<std::string_view> splitTokenList(std::string_view text, char delimiter)
{
    std::vector<std::string_view> values;
    if (text.empty() || text == "-") {
        return values;
    }
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t next = text.find(delimiter, begin);
        const std::size_t end = next == std::string_view::npos ? text.size() : next;
        if (end > begin) {
            values.push_back(text.substr(begin, end - begin));
        }
        if (next == std::string_view::npos) {
            break;
        }
        begin = next + 1;
    }
    return values;
}

std::string tileSaveToken(DungeonTile tile)
{
    return std::to_string(tile.x) + ":" + std::to_string(tile.y);
}

bool parseTileSaveToken(std::string_view token, DungeonTile& outTile)
{
    const std::size_t colon = token.find(':');
    if (colon == std::string_view::npos) {
        return false;
    }
    try {
        outTile.x = std::stoi(std::string(token.substr(0, colon)));
        outTile.y = std::stoi(std::string(token.substr(colon + 1)));
        return true;
    } catch (...) {
        return false;
    }
}

std::string_view dungeonEventObjectKindToken(Game::DungeonEventObjectKind kind)
{
    switch (kind) {
    case Game::DungeonEventObjectKind::GlowingRock: return "rock";
    case Game::DungeonEventObjectKind::ElectricReceiver: return "receiver";
    case Game::DungeonEventObjectKind::BuriedDebris: return "debris";
    case Game::DungeonEventObjectKind::LostBaggage: return "baggage";
    case Game::DungeonEventObjectKind::Campfire: return "campfire";
    case Game::DungeonEventObjectKind::HeavyRock: return "heavy_rock";
    }
    return "rock";
}

bool parseDungeonEventObjectKind(std::string_view token, Game::DungeonEventObjectKind& outKind)
{
    if (token == "rock") {
        outKind = Game::DungeonEventObjectKind::GlowingRock;
        return true;
    }
    if (token == "receiver") {
        outKind = Game::DungeonEventObjectKind::ElectricReceiver;
        return true;
    }
    if (token == "debris") {
        outKind = Game::DungeonEventObjectKind::BuriedDebris;
        return true;
    }
    if (token == "baggage") {
        outKind = Game::DungeonEventObjectKind::LostBaggage;
        return true;
    }
    if (token == "campfire") {
        outKind = Game::DungeonEventObjectKind::Campfire;
        return true;
    }
    if (token == "heavy_rock") {
        outKind = Game::DungeonEventObjectKind::HeavyRock;
        return true;
    }
    return false;
}

bool hasSaveStoryFlag(const std::vector<std::string>& flags, std::string_view flag)
{
    return std::any_of(flags.begin(), flags.end(), [flag](const std::string& entry) {
        return std::string_view(entry.data(), entry.size()) == flag;
    });
}

bool ringLevelUpgradePointsEmpty(const RingLevelUpgradePointTable& table)
{
    return std::all_of(table.begin(), table.end(), [](const RingLevelUpgradePoints& points) {
        return points.radius == 0 && points.speed == 0 && points.weightLimit == 0;
    });
}

bool inventoryHasSavedProgress(const InventorySystem& inventory)
{
    if (!inventory.objectStacks().empty() ||
        !inventory.objectInstances().empty() ||
        !inventory.equippedStaffInstanceId().empty()) {
        return true;
    }
    for (int index = 0; index < static_cast<int>(MaterialType::Count); ++index) {
        if (inventory.materialCount(static_cast<MaterialType>(index)) > 0) {
            return true;
        }
    }
    return false;
}

bool dungeonStateHasSavedProgress(const LoadedDungeonStateSave& state)
{
    return state.hasSeed ||
        !state.warpPoints.empty() ||
        !state.minimapCells.empty() ||
        !state.terrainEdits.empty() ||
        !state.rewardNodes.empty() ||
        !state.moneyNodes.empty() ||
        !state.moonFragmentNodes.empty() ||
        !state.chestNodes.empty() ||
        !state.crateNodes.empty() ||
        !state.enemyNodes.empty() ||
        !state.dungeonEventInstances.empty() ||
        !state.worldDrops.empty();
}

std::string serializedDungeonEventParams(const Game::DungeonEventInstance& event)
{
    std::vector<std::string> parts;
    if (!event.data.empty()) {
        parts.push_back("source=" + event.data);
    }
    if (!event.selectedEnemyId.empty()) {
        parts.push_back("enemy=" + event.selectedEnemyId);
    }
    parts.push_back("reward=" + tileSaveToken(event.rewardTile));
    parts.push_back(std::string("encounter=") + (event.encounterSpawned ? "1" : "0"));
    parts.push_back(std::string("activated=") + (event.activated ? "1" : "0"));
    parts.push_back(std::string("bossDefeated=") + (event.bossDefeated ? "1" : "0"));
    parts.push_back(std::string("npcRequestKnown=") + (event.npcRequestKnown ? "1" : "0"));
    parts.push_back(std::string("objectiveResolved=") + (event.objectiveResolved ? "1" : "0"));
    parts.push_back(std::string("rewardClaimed=") + (event.rewardClaimed ? "1" : "0"));
    parts.push_back("bossId=" + std::to_string(event.bossEnemyRuntimeId));
    if (!event.requestKey.empty()) {
        parts.push_back("request=" + event.requestKey);
    }
    if (!event.deliveredObjectId.empty()) {
        parts.push_back("delivered=" + event.deliveredObjectId);
    }
    parts.push_back("guideTarget=" + std::to_string(event.guideTargetWarpPointIndex));
    parts.push_back("guideRemaining=" + std::to_string(event.guideRemainingSeconds));
    if (!event.nestHoles.empty()) {
        std::string holes = "holes=";
        bool first = true;
        for (const Game::DungeonEventNestHole& hole : event.nestHoles) {
            if (!first) {
                holes += '|';
            }
            first = false;
            holes += tileSaveToken(hole.tile) + ":" +
                std::to_string(hole.hp) + ":" +
                std::to_string(hole.maxHp) + ":" +
                (hole.destroyed ? "1" : "0") + ":" +
                (hole.rewardSpawned ? "1" : "0");
        }
        parts.push_back(std::move(holes));
    }
    if (!event.eventObjects.empty()) {
        std::string objects = "objects=";
        bool first = true;
        for (const Game::DungeonEventObject& object : event.eventObjects) {
            if (!first) {
                objects += '|';
            }
            first = false;
            objects += std::string(dungeonEventObjectKindToken(object.kind)) + ":" +
                tileSaveToken(object.tile) + ":" +
                std::to_string(object.hp) + ":" +
                std::to_string(object.maxHp) + ":" +
                (object.destroyed ? "1" : "0") + ":" +
                (object.powered ? "1" : "0");
        }
        parts.push_back(std::move(objects));
    }

    std::string result;
    for (const std::string& part : parts) {
        if (!result.empty()) {
            result += ';';
        }
        result += part;
    }
    return result.empty() ? std::string("-") : result;
}

void applyDungeonEventParams(Game::DungeonEventInstance& event, std::string_view params)
{
    if (params.empty() || params == "-") {
        return;
    }
    for (std::string_view part : splitTokenList(params, ';')) {
        const std::size_t equals = part.find('=');
        if (equals == std::string_view::npos) {
            continue;
        }
        const std::string_view key = part.substr(0, equals);
        const std::string_view value = part.substr(equals + 1);
        if (key == "source") {
            event.data = std::string(value);
        } else if (key == "enemy") {
            event.selectedEnemyId = std::string(value);
        } else if (key == "reward") {
            parseTileSaveToken(value, event.rewardTile);
        } else if (key == "encounter") {
            event.encounterSpawned = value == "1";
        } else if (key == "activated") {
            event.activated = value == "1";
        } else if (key == "bossDefeated") {
            event.bossDefeated = value == "1";
        } else if (key == "npcRequestKnown") {
            event.npcRequestKnown = value == "1";
        } else if (key == "objectiveResolved") {
            event.objectiveResolved = value == "1";
        } else if (key == "rewardClaimed") {
            event.rewardClaimed = value == "1";
        } else if (key == "bossId") {
            try {
                event.bossEnemyRuntimeId = std::stoi(std::string(value));
            } catch (...) {
                event.bossEnemyRuntimeId = 0;
            }
        } else if (key == "request") {
            event.requestKey = std::string(value);
        } else if (key == "delivered") {
            event.deliveredObjectId = std::string(value);
        } else if (key == "guideTarget") {
            try {
                event.guideTargetWarpPointIndex = std::stoi(std::string(value));
            } catch (...) {
                event.guideTargetWarpPointIndex = -1;
            }
        } else if (key == "guideRemaining") {
            try {
                event.guideRemainingSeconds = std::stof(std::string(value));
            } catch (...) {
                event.guideRemainingSeconds = 0.0f;
            }
        } else if (key == "holes") {
            event.nestHoles.clear();
            for (std::string_view holeToken : splitTokenList(value, '|')) {
                const std::vector<std::string_view> fields = splitTokenList(holeToken, ':');
                if (fields.size() < 6) {
                    continue;
                }
                Game::DungeonEventNestHole hole;
                try {
                    hole.tile.x = std::stoi(std::string(fields[0]));
                    hole.tile.y = std::stoi(std::string(fields[1]));
                    hole.hp = std::stoi(std::string(fields[2]));
                    hole.maxHp = std::stoi(std::string(fields[3]));
                    hole.destroyed = fields[4] == "1";
                    hole.rewardSpawned = fields[5] == "1";
                } catch (...) {
                    continue;
                }
                event.nestHoles.push_back(std::move(hole));
            }
        } else if (key == "objects") {
            event.eventObjects.clear();
            for (std::string_view objectToken : splitTokenList(value, '|')) {
                const std::vector<std::string_view> fields = splitTokenList(objectToken, ':');
                if (fields.size() < 7) {
                    continue;
                }
                Game::DungeonEventObject object;
                if (!parseDungeonEventObjectKind(fields[0], object.kind)) {
                    continue;
                }
                try {
                    object.tile.x = std::stoi(std::string(fields[1]));
                    object.tile.y = std::stoi(std::string(fields[2]));
                    object.hp = std::stoi(std::string(fields[3]));
                    object.maxHp = std::stoi(std::string(fields[4]));
                    object.destroyed = fields[5] == "1";
                    object.powered = fields[6] == "1";
                } catch (...) {
                    continue;
                }
                event.eventObjects.push_back(std::move(object));
            }
        }
    }
}

bool dungeonMinimapTileTypeFromSave(int value, TileType& outType)
{
    switch (value) {
    case static_cast<int>(TileType::Empty):
        outType = TileType::Empty;
        return true;
    case static_cast<int>(TileType::Dirt):
        outType = TileType::Dirt;
        return true;
    case static_cast<int>(TileType::Rock):
        outType = TileType::Rock;
        return true;
    case static_cast<int>(TileType::Ore):
        outType = TileType::Ore;
        return true;
    case static_cast<int>(TileType::HardRock):
        outType = TileType::HardRock;
        return true;
    default:
        return false;
    }
}

bool validPlacementVisibilitySaveValue(int value)
{
    switch (value) {
    case 0:
    case 1:
    case 2:
        return true;
    default:
        return false;
    }
}

bool validEnemyPlacementTypeSaveValue(int value)
{
    switch (value) {
    case 0:
    case 1:
        return true;
    default:
        return false;
    }
}

const char* worldDropKindSaveName(WorldDropKind kind)
{
    switch (kind) {
    case WorldDropKind::Object:
        return "object";
    case WorldDropKind::Money:
        return "money";
    case WorldDropKind::Material:
        return "material";
    }
    return "object";
}

bool worldDropKindFromSaveName(std::string_view name, WorldDropKind& outKind)
{
    if (name == "object") {
        outKind = WorldDropKind::Object;
        return true;
    }
    if (name == "money") {
        outKind = WorldDropKind::Money;
        return true;
    }
    if (name == "material") {
        outKind = WorldDropKind::Material;
        return true;
    }
    return false;
}

bool ringItemsContainInstanceId(
    const std::array<std::vector<SpellRingItem>, SpellRingCount>& ringItemsByRing,
    std::string_view instanceId)
{
    if (instanceId.empty()) {
        return false;
    }
    for (const std::vector<SpellRingItem>& ringItems : ringItemsByRing) {
        if (std::any_of(ringItems.begin(), ringItems.end(), [instanceId](const SpellRingItem& item) {
            return item.instanceId == instanceId;
        })) {
            return true;
        }
    }
    return false;
}

int unlockedRingCountFromStageClearFlags(const std::vector<std::string>& storyFlags)
{
    constexpr std::string_view Prefix = "stage_clear_";
    int result = 1;
    for (const std::string& flag : storyFlags) {
        std::string_view text(flag.data(), flag.size());
        if (text.rfind(Prefix, 0) != 0 || text.size() == Prefix.size()) {
            continue;
        }

        int stageNumber = 0;
        bool valid = true;
        for (char ch : text.substr(Prefix.size())) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                valid = false;
                break;
            }
            stageNumber = stageNumber * 10 + (ch - '0');
        }
        if (valid) {
            result = std::max(result, stageNumber + 1);
        }
    }
    return std::clamp(result, 1, SpellRingCount);
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
    std::unordered_map<std::string, int> loadedEncyclopediaOwnedSyncSuppressCounts;
    std::unordered_map<std::string, int> loadedEncyclopediaRingSyncSuppressCounts;
    std::array<std::vector<SpellRingItem>, SpellRingCount> loadedRingItemsByRing{};
    std::vector<InventoryObjectStack> loadedWarehouseStacks;
    std::vector<InventoryObjectInstance> loadedWarehouseInstances;
    int loadedMoney = 0;
    int loadedAstralHighScore = 0;
    int loadedCurrentStage = 0;
    std::string loadedCurrentStageId = currentStageId_;
    int loadedUnlockedStages = 1;
    int loadedUnlockedRingCount = 1;
    bool loadedUnlockedRingCountExplicit = false;
    int loadedUnlockedWarpPointCount = 0;
    bool loadedHasLatestWarpPointPosition = false;
    Vec2 loadedLatestWarpPointPosition{};
    int loadedPlayerLevel = 1;
    int loadedPlayerXp = 0;
    int loadedPlayerXpToNext = playerXpToNextForLevel(loadedPlayerLevel, balance_);
    int loadedPendingLevelBonusChoices = 0;
    int loadedMaxHpUpgradeLevel = 0;
    int loadedRingRadiusUpgradeLevel = 0;
    int loadedRingSpeedUpgradeLevel = 0;
    int loadedCollectionRangeUpgradeLevel = 0;
    int loadedLevelRingRadiusPoints = 0;
    int loadedLevelRingSpeedPoints = 0;
    int loadedLevelRingWeightLimitPoints = 0;
    bool loadedLevelRingPointTable = false;
    RingLevelUpgradePointTable loadedLevelRingUpgradePoints{};
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
    std::string loadedEquippedStaffInstanceId;
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
        } else if (key == "astral_high_score") {
            stream >> loadedAstralHighScore;
        } else if (key == "player_level") {
            stream >> loadedPlayerLevel;
        } else if (key == "player_xp") {
            stream >> loadedPlayerXp;
        } else if (key == "player_xp_to_next") {
            stream >> loadedPlayerXpToNext;
        } else if (key == "pending_level_bonus_choices") {
            stream >> loadedPendingLevelBonusChoices;
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
        } else if (key == "level_ring_points") {
            int ringNumber = 0;
            RingLevelUpgradePoints points;
            stream >> ringNumber >> points.radius >> points.speed >> points.weightLimit;
            if (!stream.fail() && ringNumber >= 1 && ringNumber <= SpellRingCount) {
                loadedLevelRingUpgradePoints[static_cast<std::size_t>(ringNumber - 1)] =
                    clampedRingLevelUpgradePoints(points);
                loadedLevelRingPointTable = true;
            }
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
        } else if (key == "unlocked_ring_count") {
            stream >> loadedUnlockedRingCount;
            loadedUnlockedRingCountExplicit = !stream.fail();
        } else if (key == "auto_save_on_return") {
            stream >> loadedAutoSaveOnReturn;
        } else if (key == "equipped_staff") {
            stream >> loadedEquippedStaffInstanceId;
            if (loadedEquippedStaffInstanceId == "-") {
                loadedEquippedStaffInstanceId.clear();
            }
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
        } else if (key == "codex_sync_suppress_owned") {
            std::string objectId;
            int count = 0;
            stream >> objectId >> count;
            if (!stream.fail() && !objectId.empty() && count > 0) {
                loadedEncyclopediaOwnedSyncSuppressCounts[std::move(objectId)] = count;
            }
        } else if (key == "codex_sync_suppress_ring") {
            std::string objectId;
            int count = 0;
            stream >> objectId >> count;
            if (!stream.fail() && !objectId.empty() && count > 0) {
                loadedEncyclopediaRingSyncSuppressCounts[std::move(objectId)] = count;
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
        } else if (key == "dungeon_minimap_cell") {
            std::string stageId;
            int x = 0;
            int y = 0;
            int typeValue = 0;
            stream >> stageId >> x >> y >> typeValue;
            TileType type = TileType::Empty;
            if (!stream.fail() && !stageId.empty() && dungeonMinimapTileTypeFromSave(typeValue, type)) {
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    loadedDungeonState.minimapCells.push_back(LoadedDungeonMinimapCellSave{x, y, type});
                }
            }
        } else if (key == "dungeon_tile_edit") {
            std::string stageId;
            int x = 0;
            int y = 0;
            int typeValue = 0;
            stream >> stageId >> x >> y >> typeValue;
            TileType type = TileType::Empty;
            if (!stream.fail() && !stageId.empty() && dungeonMinimapTileTypeFromSave(typeValue, type)) {
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    loadedDungeonState.terrainEdits.push_back(TerrainTileEdit{DungeonTile{x, y}, type});
                }
            }
        } else if (key == "dungeon_reward_node") {
            std::string stageId;
            LoadedRewardNodeSave node;
            int visibilityValue = 0;
            stream >> stageId
                >> node.tile.x
                >> node.tile.y
                >> visibilityValue
                >> node.revealed
                >> node.spawned
                >> node.collected;
            if (!stream.fail() && !stageId.empty() && validPlacementVisibilitySaveValue(visibilityValue)) {
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    node.visibility = visibilityValue;
                    loadedDungeonState.rewardNodes.push_back(node);
                }
            }
        } else if (key == "dungeon_money_node") {
            std::string stageId;
            LoadedMoneyNodeSave node;
            int visibilityValue = 0;
            stream >> stageId
                >> node.tile.x
                >> node.tile.y
                >> visibilityValue
                >> node.collected;
            if (!stream.fail() && !stageId.empty() && validPlacementVisibilitySaveValue(visibilityValue)) {
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    node.visibility = visibilityValue;
                    loadedDungeonState.moneyNodes.push_back(node);
                }
            }
        } else if (key == "dungeon_moon_fragment_node") {
            std::string stageId;
            LoadedMoonFragmentNodeSave node;
            int visibilityValue = 0;
            stream >> stageId
                >> node.tile.x
                >> node.tile.y
                >> visibilityValue
                >> node.collected;
            if (!stream.fail() && !stageId.empty() && validPlacementVisibilitySaveValue(visibilityValue)) {
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    node.visibility = visibilityValue;
                    loadedDungeonState.moonFragmentNodes.push_back(node);
                }
            }
        } else if (key == "dungeon_chest_node") {
            std::string stageId;
            LoadedChestNodeSave node;
            int visibilityValue = 0;
            stream >> stageId
                >> node.tile.x
                >> node.tile.y
                >> visibilityValue
                >> node.revealed
                >> node.opened
                >> node.lootSpawned
                >> node.openingSeconds;
            if (!stream.fail() && !stageId.empty() && validPlacementVisibilitySaveValue(visibilityValue)) {
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    node.visibility = visibilityValue;
                    node.openingSeconds = std::max(0.0f, node.openingSeconds);
                    loadedDungeonState.chestNodes.push_back(node);
                }
            }
        } else if (key == "dungeon_crate_node") {
            std::string stageId;
            LoadedCrateNodeSave node;
            stream >> stageId
                >> node.tile.x
                >> node.tile.y
                >> node.destroyed;
            if (!stream.fail() && !stageId.empty()) {
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    loadedDungeonState.crateNodes.push_back(node);
                }
            }
        } else if (key == "dungeon_enemy_node") {
            std::string stageId;
            LoadedEnemyNodeSave node;
            int placementTypeValue = 0;
            stream >> stageId
                >> node.tile.x
                >> node.tile.y
                >> placementTypeValue
                >> node.spawned;
            if (!stream.fail() && !stageId.empty() && validEnemyPlacementTypeSaveValue(placementTypeValue)) {
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    node.placementType = placementTypeValue;
                    loadedDungeonState.enemyNodes.push_back(node);
                }
            }
        } else if (key == "dungeon_event_instance") {
            std::string stageId;
            LoadedDungeonEventInstanceSave event;
            std::string data;
            stream >> stageId
                >> event.id
                >> event.kindName
                >> event.centerTile.x
                >> event.centerTile.y
                >> event.focusTile.x
                >> event.focusTile.y
                >> event.discoveryRadiusTiles
                >> event.discovered
                >> event.completed
                >> event.selfLightRadiusTiles
                >> data;
            if (!stream.fail() && !stageId.empty() && !event.id.empty()) {
                if (data != "-") {
                    event.data = std::move(data);
                }
                std::string spawnedEntityIds;
                std::string params;
                if (stream >> event.rewardSpawned) {
                    if (stream >> spawnedEntityIds) {
                        event.spawnedEntityIds = splitSaveList(spawnedEntityIds);
                    }
                    if (stream >> params && params != "-") {
                        event.params = std::move(params);
                    }
                }
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    loadedDungeonState.dungeonEventInstances.push_back(std::move(event));
                }
            }
        } else if (key == "dungeon_world_drop") {
            std::string stageId;
            std::string kindName;
            WorldDropItem drop;
            stream >> stageId
                >> kindName
                >> drop.id
                >> drop.quantity
                >> drop.position.x
                >> drop.position.y
                >> drop.spawnedAtSeconds
                >> drop.ageSeconds;
            WorldDropKind kind = WorldDropKind::Object;
            bool validDrop = !stream.fail() &&
                !stageId.empty() &&
                worldDropKindFromSaveName(kindName, kind) &&
                !drop.id.empty() &&
                drop.quantity > 0;
            if (validDrop && kind == WorldDropKind::Object && objectCatalog_.registry.findById(drop.id) == nullptr) {
                ++warningCount;
                logError("[warning] SaveData: dungeon_world_drop object_id=\"" + drop.id + "\" is missing from Objects DB; keeping ID");
            } else if (validDrop && kind == WorldDropKind::Material) {
                MaterialType materialType = MaterialType::Count;
                validDrop = materialTypeFromSaveName(drop.id, materialType);
            }
            if (validDrop) {
                drop.kind = kind;
                drop.quantity = std::max(1, drop.quantity);
                drop.spawnedAtSeconds = std::max(0.0f, drop.spawnedAtSeconds);
                drop.ageSeconds = std::max(0.0f, drop.ageSeconds);
                if (!loadedDungeonState.hasSeed || loadedDungeonState.stageId.empty() || loadedDungeonState.stageId == stageId) {
                    loadedDungeonState.stageId = std::move(stageId);
                    loadedDungeonState.worldDrops.push_back(std::move(drop));
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

    const bool loadedSaveHasIntroCompletion = hasSaveStoryFlag(loadedStoryFlags, IntroTutorialCompletedFlag);
    const bool loadedSaveHasUpgradeProgress =
        loadedMaxHpUpgradeLevel > 0 ||
        loadedRingRadiusUpgradeLevel > 0 ||
        loadedRingSpeedUpgradeLevel > 0 ||
        loadedCollectionRangeUpgradeLevel > 0 ||
        loadedLevelRingRadiusPoints > 0 ||
        loadedLevelRingSpeedPoints > 0 ||
        loadedLevelRingWeightLimitPoints > 0 ||
        !ringLevelUpgradePointsEmpty(loadedLevelRingUpgradePoints) ||
        loadedWorkshopInitialRadiusLevel > 0 ||
        loadedWorkshopInitialSpeedLevel > 0 ||
        loadedWorkshopShiftDistanceLevel > 0;
    const bool loadedSaveHasMerchantProgress =
        loadedMerchantRefreshPending ||
        loadedMerchantUpgradeLevel > 1 ||
        loadedMerchantStockVersion > 0 ||
        !loadedMerchantStock.empty() ||
        !loadedHighValueBuyCategory.empty() ||
        !loadedHighValueBuyObjectIds.empty();
    const bool loadedSaveHasBaseProgress =
        loadedWarehouseCapacityLevel > 0 ||
        loadedProcessingUnlockLevel > 0 ||
        loadedRingWorkshopUnlocked ||
        loadedAutoSaveOnReturn ||
        !loadedWarehouseStacks.empty() ||
        !loadedWarehouseInstances.empty();
    const bool loadedSaveHasStageProgress =
        loadedCurrentStage > 0 ||
        loadedCurrentStageId != "stage_01_stardust" ||
        loadedUnlockedStages > 1 ||
        (loadedUnlockedRingCountExplicit && loadedUnlockedRingCount > 1) ||
        loadedUnlockedWarpPointCount > 0 ||
        loadedHasLatestWarpPointPosition;
    const bool loadedSaveHasPlayerProgress =
        loadedMoney > 0 ||
        loadedAstralHighScore > 0 ||
        loadedPlayerLevel > 1 ||
        loadedPlayerXp > 0 ||
        loadedPendingLevelBonusChoices > 0;
    const bool loadedSaveHasProgressBeyondFreshStart =
        loadedSaveHasUpgradeProgress ||
        loadedSaveHasMerchantProgress ||
        loadedSaveHasBaseProgress ||
        loadedSaveHasStageProgress ||
        loadedSaveHasPlayerProgress ||
        !loadedStoryFlags.empty() ||
        inventoryHasSavedProgress(loadedInventory) ||
        !loadedEncyclopedia.saveEntries().empty() ||
        !loadedEncyclopedia.saveEffects().empty() ||
        !loadedEncyclopediaOwnedSyncSuppressCounts.empty() ||
        !loadedEncyclopediaRingSyncSuppressCounts.empty() ||
        dungeonStateHasSavedProgress(loadedDungeonState);
    if (!loadedSaveHasIntroCompletion && loadedSaveHasProgressBeyondFreshStart) {
        loadedStoryFlags.push_back(std::string(IntroTutorialCompletedFlag));
        logInfo("[save] migrated existing save to skip intro tutorial.");
    }

    inventory_ = loadedInventory;
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    if (!loadedEquippedStaffInstanceId.empty()) {
        if (ringItemsContainInstanceId(loadedRingItemsByRing, loadedEquippedStaffInstanceId)) {
            ++warningCount;
            inventory_.clearEquippedStaff();
            logError("[warning] SaveData: equipped_staff instance_id=\"" + loadedEquippedStaffInstanceId +
                "\" is also mounted on a ring; staff unequipped");
        } else {
            std::string staffWarning;
            if (!inventory_.restoreEquippedStaffInstanceId(loadedEquippedStaffInstanceId, &staffWarning)) {
                ++warningCount;
                logError("[warning] SaveData: " + staffWarning);
            }
        }
    }
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
    refreshEquipmentModifiers();
    refreshOrbitEffects();
    money_ = std::max(0, loadedMoney);
    astralHighScore_ = std::max(0, loadedAstralHighScore);
    unlockedStages_ = std::max(1, loadedUnlockedStages);
    const int migratedUnlockedRingCount = loadedUnlockedRingCountExplicit
        ? loadedUnlockedRingCount
        : std::max(unlockedStages_, unlockedRingCountFromStageClearFlags(loadedStoryFlags));
    setUnlockedRingCount(migratedUnlockedRingCount);
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
        dungeonEvents_.clear();
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

        dungeonMinimapCells_.clear();
        for (const LoadedDungeonMinimapCellSave& cell : loadedDungeonState.minimapCells) {
            setDungeonMinimapTile(cell.x, cell.y, cell.type);
        }

        initializeMoonFragmentNodesFromWarpPoints();
        initializeRewardNodesFromLayout();
        initializeChestNodesFromLayout();
        initializeCrateNodesFromLayout();
        initializeEnemyNodesFromLayout();
        initializeDungeonEventInstancesFromLayout();
        const auto sameTile = [](DungeonTile lhs, DungeonTile rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y;
        };
        for (const LoadedRewardNodeSave& savedNode : loadedDungeonState.rewardNodes) {
            auto nodeIt = std::find_if(rewardNodes_.begin(), rewardNodes_.end(), [&](const RewardNode& node) {
                return sameTile(node.tile, savedNode.tile);
            });
            if (nodeIt == rewardNodes_.end()) {
                continue;
            }
            nodeIt->visibility = static_cast<PlacementVisibility>(savedNode.visibility);
            nodeIt->revealed = savedNode.revealed;
            nodeIt->spawned = savedNode.spawned;
            nodeIt->collected = savedNode.collected;
        }
        for (const LoadedMoneyNodeSave& savedNode : loadedDungeonState.moneyNodes) {
            auto nodeIt = std::find_if(moneyNodes_.begin(), moneyNodes_.end(), [&](const MoneyNode& node) {
                return sameTile(node.tile, savedNode.tile);
            });
            if (nodeIt == moneyNodes_.end()) {
                continue;
            }
            nodeIt->visibility = static_cast<PlacementVisibility>(savedNode.visibility);
            nodeIt->collected = savedNode.collected;
        }
        for (const LoadedMoonFragmentNodeSave& savedNode : loadedDungeonState.moonFragmentNodes) {
            auto nodeIt = std::find_if(moonFragmentNodes_.begin(), moonFragmentNodes_.end(), [&](const MoonFragmentNode& node) {
                return sameTile(node.tile, savedNode.tile);
            });
            if (nodeIt == moonFragmentNodes_.end()) {
                continue;
            }
            nodeIt->visibility = static_cast<PlacementVisibility>(savedNode.visibility);
            nodeIt->collected = savedNode.collected;
        }
        for (const LoadedChestNodeSave& savedNode : loadedDungeonState.chestNodes) {
            auto nodeIt = std::find_if(chestNodes_.begin(), chestNodes_.end(), [&](const ChestNode& node) {
                return sameTile(node.tile, savedNode.tile);
            });
            if (nodeIt == chestNodes_.end()) {
                continue;
            }
            nodeIt->visibility = static_cast<PlacementVisibility>(savedNode.visibility);
            nodeIt->revealed = savedNode.revealed;
            nodeIt->opened = savedNode.opened;
            nodeIt->lootSpawned = savedNode.lootSpawned;
            nodeIt->openingSeconds = savedNode.openingSeconds;
        }
        for (const LoadedCrateNodeSave& savedNode : loadedDungeonState.crateNodes) {
            auto nodeIt = std::find_if(crateNodes_.begin(), crateNodes_.end(), [&](const CrateNode& node) {
                return sameTile(node.tile, savedNode.tile);
            });
            if (nodeIt == crateNodes_.end()) {
                continue;
            }
            nodeIt->destroyed = savedNode.destroyed;
        }
        for (const LoadedEnemyNodeSave& savedNode : loadedDungeonState.enemyNodes) {
            auto nodeIt = std::find_if(enemyNodes_.begin(), enemyNodes_.end(), [&](const EnemyNode& node) {
                return sameTile(node.tile, savedNode.tile);
            });
            if (nodeIt == enemyNodes_.end()) {
                continue;
            }
            nodeIt->placementType = static_cast<EnemyPlacementType>(savedNode.placementType);
            nodeIt->spawned = savedNode.spawned;
        }
        for (const LoadedDungeonEventInstanceSave& savedEvent : loadedDungeonState.dungeonEventInstances) {
            DungeonEventInstance* eventIt = dungeonEvents_.findById(savedEvent.id);
            if (eventIt == nullptr) {
                continue;
            }
            DungeonEventKind savedKind = eventIt->kind;
            if (dungeonEventKindFromId(savedEvent.kindName, savedKind)) {
                eventIt->kind = savedKind;
            }
            eventIt->centerTile = savedEvent.centerTile;
            eventIt->focusTile = savedEvent.focusTile;
            eventIt->discoveryRadiusTiles = std::max(0.0f, savedEvent.discoveryRadiusTiles);
            eventIt->discovered = savedEvent.discovered;
            eventIt->completed = savedEvent.completed;
            eventIt->rewardSpawned = savedEvent.rewardSpawned;
            eventIt->selfLightRadiusTiles = std::max(0.0f, savedEvent.selfLightRadiusTiles);
            eventIt->spawnedEntityIds = savedEvent.spawnedEntityIds;
            eventIt->params = savedEvent.params;
            eventIt->data = savedEvent.data;
            applyDungeonEventParams(*eventIt, savedEvent.params);
        }
        applyPlacementTerrainOverrides();
        for (const TerrainTileEdit& edit : loadedDungeonState.terrainEdits) {
            tileMap_.setTerrainEdit(edit.tile, edit.type);
        }
        worldDrops_.restoreDropsForSave(std::move(loadedDungeonState.worldDrops));
        captureDungeonState();
        syncWarpStateForCurrentStage();
        restoredDungeonStateFromSave = true;
    }
    if (!restoredDungeonStateFromSave && currentStageId_ != loadedStageIdForWarpState) {
        unlockedWarpPointCount_ = 0;
        hasLatestWarpPointPosition_ = false;
        latestWarpPointPosition_ = {};
    }
    player_.level = std::clamp(loadedPlayerLevel, 1, PlayerMaxLevel);
    player_.xpToNext = playerXpToNextForLevel(player_.level, balance_);
    player_.xp = playerAtMaxLevel(player_) ? 0 : std::max(0, loadedPlayerXp);
    maxHpUpgradeLevel_ = std::max(0, loadedMaxHpUpgradeLevel);
    ringRadiusUpgradeLevel_ = std::max(0, loadedRingRadiusUpgradeLevel);
    ringSpeedUpgradeLevel_ = std::max(0, loadedRingSpeedUpgradeLevel);
    collectionRangeUpgradeLevel_ = std::clamp(loadedCollectionRangeUpgradeLevel, 0, 5);
    if (!loadedLevelRingPointTable) {
        const RingLevelUpgradePoints legacyPoints{
            std::max(0, loadedLevelRingRadiusPoints),
            std::max(0, loadedLevelRingSpeedPoints),
            std::max(0, loadedLevelRingWeightLimitPoints),
        };
        loadedLevelRingUpgradePoints.fill(legacyPoints);
    }
    for (RingLevelUpgradePoints& points : loadedLevelRingUpgradePoints) {
        points = clampedRingLevelUpgradePoints(points);
    }
    levelRingUpgradePoints_ = loadedLevelRingUpgradePoints;
    levels_.setPendingChoiceCount(loadedPendingLevelBonusChoices);
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
    encyclopediaOwnedSyncSuppressCounts_ = std::move(loadedEncyclopediaOwnedSyncSuppressCounts);
    encyclopediaRingSyncSuppressCounts_ = std::move(loadedEncyclopediaRingSyncSuppressCounts);
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
    file << "astral_high_score " << astralHighScore_ << "\n";
    file << "player_level " << player_.level << "\n";
    file << "player_xp " << player_.xp << "\n";
    file << "player_xp_to_next " << player_.xpToNext << "\n";
    file << "pending_level_bonus_choices " << levels_.pendingChoiceCount() << "\n";
    file << "upgrade_max_hp " << maxHpUpgradeLevel_ << "\n";
    file << "upgrade_ring_radius " << ringRadiusUpgradeLevel_ << "\n";
    file << "upgrade_ring_speed " << ringSpeedUpgradeLevel_ << "\n";
    file << "upgrade_collection_range " << collectionRangeUpgradeLevel_ << "\n";
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const RingLevelUpgradePoints points = clampedRingLevelUpgradePoints(
            levelRingUpgradePoints_[static_cast<std::size_t>(ringIndex)]);
        file << "level_ring_points " << (ringIndex + 1) << " "
            << points.radius << " "
            << points.speed << " "
            << points.weightLimit << "\n";
    }
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
    file << "equipped_staff "
        << (inventory_.equippedStaffInstanceId().empty() ? "-" : inventory_.equippedStaffInstanceId())
        << "\n";
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
    for (const auto& [objectId, count] : encyclopediaOwnedSyncSuppressCounts_) {
        if (!objectId.empty() && count > 0) {
            file << "codex_sync_suppress_owned " << objectId << " " << count << "\n";
        }
    }
    for (const auto& [objectId, count] : encyclopediaRingSyncSuppressCounts_) {
        if (!objectId.empty() && count > 0) {
            file << "codex_sync_suppress_ring " << objectId << " " << count << "\n";
        }
    }
    file << "current_stage " << currentStage_ << "\n";
    file << "current_stage_id " << currentStageId_ << "\n";
    file << "unlocked_stages " << unlockedStages_ << "\n";
    file << "unlocked_ring_count " << unlockedRingCount() << "\n";
    file << "unlocked_warp_points " << unlockedWarpPointCount_ << "\n";
    if (hasLatestWarpPointPosition_) {
        file << "latest_warp " << latestWarpPointPosition_.x << " " << latestWarpPointPosition_.y << "\n";
    }
    const DungeonLayout* saveDungeonLayout = nullptr;
    const TileMap* saveDungeonTileMap = nullptr;
    const std::vector<WarpPoint>* saveDungeonWarpPoints = nullptr;
    const DungeonMinimapCells* saveDungeonMinimapCells = nullptr;
    const std::vector<RewardNode>* saveRewardNodes = nullptr;
    const std::vector<MoneyNode>* saveMoneyNodes = nullptr;
    const std::vector<MoonFragmentNode>* saveMoonFragmentNodes = nullptr;
    const std::vector<ChestNode>* saveChestNodes = nullptr;
    const std::vector<CrateNode>* saveCrateNodes = nullptr;
    const std::vector<EnemyNode>* saveEnemyNodes = nullptr;
    const std::vector<DungeonEventInstance>* saveDungeonEventInstances = nullptr;
    const WorldDropSystem* saveWorldDrops = nullptr;
    std::string saveDungeonStageId = currentStageId_;
    int saveDungeonCurrentStage = currentStage_;
    if (mode_ == ScreenMode::Playing &&
        !enemyTestActive_ &&
        hasSaveableDungeonLayout(dungeonLayout_) &&
        !isRoguelikeSaveStage(currentStageDefinition())) {
        saveDungeonLayout = &dungeonLayout_;
        saveDungeonTileMap = &tileMap_;
        saveDungeonWarpPoints = &warpPoints_;
        saveDungeonMinimapCells = &dungeonMinimapCells_;
        saveRewardNodes = &rewardNodes_;
        saveMoneyNodes = &moneyNodes_;
        saveMoonFragmentNodes = &moonFragmentNodes_;
        saveChestNodes = &chestNodes_;
        saveCrateNodes = &crateNodes_;
        saveEnemyNodes = &enemyNodes_;
        saveDungeonEventInstances = &dungeonEvents_.all();
        saveWorldDrops = &worldDrops_;
    } else {
        const auto retainedStage = dungeonStates_.find(currentStageId_);
        if (retainedStage != dungeonStates_.end() &&
            retainedStage->second.valid &&
            hasSaveableDungeonLayout(retainedStage->second.dungeonLayout) &&
            !isRoguelikeSaveStage(currentStageDefinition())) {
            saveDungeonLayout = &retainedStage->second.dungeonLayout;
            saveDungeonTileMap = &retainedStage->second.tileMap;
            saveDungeonWarpPoints = &retainedStage->second.warpPoints;
            saveDungeonMinimapCells = &retainedStage->second.dungeonMinimapCells;
            saveRewardNodes = &retainedStage->second.rewardNodes;
            saveMoneyNodes = &retainedStage->second.moneyNodes;
            saveMoonFragmentNodes = &retainedStage->second.moonFragmentNodes;
            saveChestNodes = &retainedStage->second.chestNodes;
            saveCrateNodes = &retainedStage->second.crateNodes;
            saveEnemyNodes = &retainedStage->second.enemyNodes;
            saveDungeonEventInstances = &retainedStage->second.dungeonEventInstances;
            saveWorldDrops = &retainedStage->second.worldDrops;
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
        if (saveDungeonMinimapCells != nullptr) {
            for (const auto& [key, cell] : *saveDungeonMinimapCells) {
                const DungeonTile tile = dungeonMinimapTileFromKey(key);
                file << "dungeon_minimap_cell "
                    << saveDungeonStageId << " "
                    << tile.x << " "
                    << tile.y << " "
                    << static_cast<int>(cell.type) << "\n";
            }
        }
        if (saveDungeonTileMap != nullptr) {
            for (const TerrainTileEdit& edit : saveDungeonTileMap->terrainEditsForSave()) {
                file << "dungeon_tile_edit "
                    << saveDungeonStageId << " "
                    << edit.tile.x << " "
                    << edit.tile.y << " "
                    << static_cast<int>(edit.type) << "\n";
            }
        }
        if (saveRewardNodes != nullptr) {
            for (const RewardNode& node : *saveRewardNodes) {
                file << "dungeon_reward_node "
                    << saveDungeonStageId << " "
                    << node.tile.x << " "
                    << node.tile.y << " "
                    << static_cast<int>(node.visibility) << " "
                    << node.revealed << " "
                    << node.spawned << " "
                    << node.collected << "\n";
            }
        }
        if (saveMoneyNodes != nullptr) {
            for (const MoneyNode& node : *saveMoneyNodes) {
                file << "dungeon_money_node "
                    << saveDungeonStageId << " "
                    << node.tile.x << " "
                    << node.tile.y << " "
                    << static_cast<int>(node.visibility) << " "
                    << node.collected << "\n";
            }
        }
        if (saveMoonFragmentNodes != nullptr) {
            for (const MoonFragmentNode& node : *saveMoonFragmentNodes) {
                file << "dungeon_moon_fragment_node "
                    << saveDungeonStageId << " "
                    << node.tile.x << " "
                    << node.tile.y << " "
                    << static_cast<int>(node.visibility) << " "
                    << node.collected << "\n";
            }
        }
        if (saveChestNodes != nullptr) {
            for (const ChestNode& node : *saveChestNodes) {
                file << "dungeon_chest_node "
                    << saveDungeonStageId << " "
                    << node.tile.x << " "
                    << node.tile.y << " "
                    << static_cast<int>(node.visibility) << " "
                    << node.revealed << " "
                    << node.opened << " "
                    << node.lootSpawned << " "
                    << node.openingSeconds << "\n";
            }
        }
        if (saveCrateNodes != nullptr) {
            for (const CrateNode& node : *saveCrateNodes) {
                file << "dungeon_crate_node "
                    << saveDungeonStageId << " "
                    << node.tile.x << " "
                    << node.tile.y << " "
                    << node.destroyed << "\n";
            }
        }
        if (saveEnemyNodes != nullptr) {
            for (const EnemyNode& node : *saveEnemyNodes) {
                file << "dungeon_enemy_node "
                    << saveDungeonStageId << " "
                    << node.tile.x << " "
                    << node.tile.y << " "
                    << static_cast<int>(node.placementType) << " "
                    << node.spawned << "\n";
            }
        }
        if (saveDungeonEventInstances != nullptr) {
            for (const DungeonEventInstance& event : *saveDungeonEventInstances) {
                if (event.data == "debug" || event.params.find("source=debug") != std::string::npos) {
                    continue;
                }
                file << "dungeon_event_instance "
                    << saveDungeonStageId << " "
                    << event.id << " "
                    << dungeonEventKindId(event.kind) << " "
                    << event.centerTile.x << " "
                    << event.centerTile.y << " "
                    << event.focusTile.x << " "
                    << event.focusTile.y << " "
                    << event.discoveryRadiusTiles << " "
                    << event.discovered << " "
                    << event.completed << " "
                    << event.selfLightRadiusTiles << " "
                    << (event.data.empty() ? "-" : event.data) << " "
                    << event.rewardSpawned << " "
                    << joinSaveList(event.spawnedEntityIds) << " "
                    << serializedDungeonEventParams(event) << "\n";
            }
        }
        if (saveWorldDrops != nullptr) {
            for (const WorldDropItem& drop : saveWorldDrops->drops()) {
                if (drop.temporary || drop.id.empty() || drop.quantity <= 0) {
                    continue;
                }
                file << "dungeon_world_drop "
                    << saveDungeonStageId << " "
                    << worldDropKindSaveName(drop.kind) << " "
                    << drop.id << " "
                    << drop.quantity << " "
                    << drop.position.x << " "
                    << drop.position.y << " "
                    << drop.spawnedAtSeconds << " "
                    << drop.ageSeconds << "\n";
            }
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
