#include "game/Game.hpp"

#include "engine/Log.hpp"
#include "engine/Ui.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace majo {

namespace {
constexpr int ShovelIconIndex = 0;
constexpr int TorchIconIndex = 1;
constexpr float IconDrawSize = 32.0f;
constexpr int BaseMenuItemCount = 8;
constexpr int BaseMiningStartChoiceCount = 2;
constexpr int BaseUpgradeItemCount = 8;
constexpr int BaseProcessingModeCount = 4;
constexpr int BaseRingWorkshopItemCount = 10;
constexpr int BookshelfMenuItemCount = 3;
constexpr int RingWorkshopImplementedUpgradeCount = 3;
constexpr int MaxItemEnhanceLevel = 5;
constexpr int MerchantRefreshDugTileThreshold = 10;
constexpr int WarehouseVisibleRows = 8;
constexpr int PauseMenuItemCount = 5;
constexpr int GameOverItemCount = 2;
constexpr int StageClearItemCount = 1;
constexpr int RingCount = 3;
constexpr int RingSlotCount = 8;
constexpr int RingSlotColumns = 4;
constexpr float WarpPointSpacing = 320.0f;
constexpr float WarpPointTouchRadius = 28.0f;
constexpr int MaxWarpPointsPerRun = 8;
constexpr int BossWarpPointIndex = 7;
constexpr int BossOffsetTiles = 20;
constexpr float BossSpawnTriggerRadius = 48.0f;
constexpr int MaxPlayerOrbitProjectiles = 48;
constexpr int RadialSpikeProjectileCount = 8;
constexpr float CapturedProjectileMinInterval = 0.75f;
constexpr float CapturedRadialSpikeMinInterval = 1.50f;
constexpr float CapturedProjectileRetryInterval = 0.25f;
constexpr float CapturedMagnetVisualInterval = 0.45f;
constexpr float CapturedWindInterval = 1.20f;
constexpr float CapturedExplosionTileRadius = 28.0f;
constexpr int CapturedExplosionTileDamage = 1;
constexpr int CapturedExplosionEnemyDamage = 3;
constexpr float RewardPickupRadius = 24.0f;
constexpr int RewardNodeCountPerRun = 12;
constexpr int MoneyNodeCountPerRun = 18;
constexpr int EnemyNodeCountPerRun = 14;
constexpr float ExposedEnemyNodeSpawnRadius = 820.0f;

enum class BaseFacilityAction {
    MineExit,
    Storage,
    Bookshelf,
    Merchant,
    Processing,
    Forge,
    Observatory,
    Diary,
    RingWorkshop,
};

struct BaseFacility {
    const char* facilityId = "";
    const char* displayName = "";
    UiRect rect{};
    float interactionRange = 56.0f;
    bool enabled = true;
    bool unlocked = true;
    BaseFacilityAction onInteract = BaseFacilityAction::MineExit;
};

Vec2 tileWorldCenter(DungeonTile tile)
{
    return {
        (static_cast<float>(tile.x) + 0.5f) * static_cast<float>(balance::TileSize),
        (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(balance::TileSize),
    };
}

DungeonTile roundDungeonTile(Vec2 tilePosition)
{
    return {
        static_cast<int>(std::round(tilePosition.x)),
        static_cast<int>(std::round(tilePosition.y)),
    };
}

Vec2 perpendicular(Vec2 value)
{
    return {-value.y, value.x};
}

Vec2 pointAtPathProgress(const std::vector<Vec2>& points, float progress)
{
    if (points.empty()) {
        return {};
    }
    if (points.size() == 1) {
        return points.front();
    }

    float totalLength = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        totalLength += length(points[i] - points[i - 1]);
    }
    if (totalLength <= 0.0001f) {
        return points.front();
    }

    const float target = totalLength * clamp(progress, 0.0f, 1.0f);
    float traveled = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const float segmentLength = length(points[i] - points[i - 1]);
        if (traveled + segmentLength >= target) {
            const float t = segmentLength > 0.0001f ? (target - traveled) / segmentLength : 0.0f;
            return lerp(points[i - 1], points[i], t);
        }
        traveled += segmentLength;
    }
    return points.back();
}

Vec2 tangentAtPathProgress(const std::vector<Vec2>& points, float progress)
{
    if (points.size() < 2) {
        return {1.0f, 0.0f};
    }

    const std::size_t index = std::min(
        points.size() - 2,
        static_cast<std::size_t>(clamp(progress, 0.0f, 1.0f) * static_cast<float>(points.size() - 1)));
    return normalize(points[index + 1] - points[index]);
}

std::optional<std::string> firstAvailableObjectId(const ObjectCatalog& catalog)
{
    for (const ObjectDefinition& object : catalog.objects) {
        if (!object.id.empty()) {
            return object.id;
        }
    }
    return std::nullopt;
}

UiRect baseMapBounds()
{
    return {{48.0f, 48.0f}, {1184.0f, 624.0f}};
}

float distanceToRect(Vec2 point, UiRect rect)
{
    const float closestX = clamp(point.x, rect.pos.x, rect.pos.x + rect.size.x);
    const float closestY = clamp(point.y, rect.pos.y, rect.pos.y + rect.size.y);
    return length(point - Vec2{closestX, closestY});
}

bool circleIntersectsRect(Vec2 center, float radius, UiRect rect)
{
    return distanceToRect(center, rect) < radius;
}

const char* screenModeName(ScreenMode mode)
{
    switch (mode) {
    case ScreenMode::Base: return "Base";
    case ScreenMode::Playing: return "Playing";
    case ScreenMode::PauseMenu: return "PauseMenu";
    case ScreenMode::Inventory: return "Inventory";
    case ScreenMode::Ring: return "Ring";
    case ScreenMode::LevelUp: return "LevelUp";
    case ScreenMode::GameOver: return "GameOver";
    case ScreenMode::StageClear: return "StageClear";
    }
    return "Unknown";
}

std::array<BaseFacility, 10> baseFacilities(bool ringWorkshopUnlocked)
{
    return {
        BaseFacility{"mine_exit", "採掘出口", {{584.0f, 560.0f}, {112.0f, 64.0f}}, 78.0f, true, true, BaseFacilityAction::MineExit},
        BaseFacility{"storage_chest", "収納箱", {{158.0f, 320.0f}, {98.0f, 72.0f}}, 68.0f, true, true, BaseFacilityAction::Storage},
        BaseFacility{"bookshelf", "本棚", {{150.0f, 118.0f}, {118.0f, 58.0f}}, 64.0f, true, true, BaseFacilityAction::Bookshelf},
        BaseFacility{"merchant_wagon", "商人ワゴン", {{982.0f, 302.0f}, {150.0f, 90.0f}}, 78.0f, true, true, BaseFacilityAction::Merchant},
        BaseFacility{"processing_table", "魔導加工台", {{930.0f, 128.0f}, {130.0f, 68.0f}}, 70.0f, true, true, BaseFacilityAction::Processing},
        BaseFacility{"upgrade_forge", "強化炉", {{568.0f, 76.0f}, {144.0f, 74.0f}}, 76.0f, true, true, BaseFacilityAction::Forge},
        BaseFacility{"observatory", "モニカの星見台", {{750.0f, 72.0f}, {164.0f, 68.0f}}, 72.0f, true, true, BaseFacilityAction::Observatory},
        BaseFacility{"diary", "日記", {{68.0f, 574.0f}, {78.0f, 54.0f}}, 58.0f, true, true, BaseFacilityAction::Diary},
        BaseFacility{"ring_workshop", "リング工房用スペース", {{902.0f, 520.0f}, {172.0f, 92.0f}}, 82.0f, true, ringWorkshopUnlocked, BaseFacilityAction::RingWorkshop},
        BaseFacility{"home", "主人公の家", {{332.0f, 74.0f}, {150.0f, 96.0f}}, 48.0f, false, true, BaseFacilityAction::Observatory},
    };
}

bool baseInteractionAvailable(Vec2 playerPosition, const BaseFacility& facility)
{
    return facility.enabled && distanceToRect(playerPosition, facility.rect) <= facility.interactionRange;
}

UiRect basePanelRect()
{
    return {{360.0f, 92.0f}, {560.0f, 536.0f}};
}

UiRect baseMenuItemRect(int index)
{
    return {{450.0f, 296.0f + static_cast<float>(index) * 36.0f}, {380.0f, 30.0f}};
}

UiRect baseMiningStartChoiceRect(int index)
{
    return {{450.0f, 318.0f + static_cast<float>(index) * 58.0f}, {380.0f, 44.0f}};
}

UiRect baseSellItemRect(int index)
{
    return {{420.0f, 248.0f + static_cast<float>(index) * 48.0f}, {440.0f, 38.0f}};
}

UiRect merchantSellItemRect(int index)
{
    return {{390.0f, 278.0f + static_cast<float>(index) * 42.0f}, {250.0f, 34.0f}};
}

UiRect merchantBuyItemRect(int index)
{
    return {{660.0f, 278.0f + static_cast<float>(index) * 42.0f}, {230.0f, 34.0f}};
}

UiRect baseUpgradeItemRect(int index)
{
    return {{392.0f, 246.0f + static_cast<float>(index) * 34.0f}, {496.0f, 30.0f}};
}

UiRect baseProcessingModeRect(int index)
{
    return {{386.0f + static_cast<float>(index) * 126.0f, 244.0f}, {118.0f, 34.0f}};
}

UiRect baseProcessingItemRect(int index)
{
    return {{390.0f, 298.0f + static_cast<float>(index) * 40.0f}, {500.0f, 34.0f}};
}

UiRect baseRingWorkshopItemRect(int index)
{
    return {{390.0f, 252.0f + static_cast<float>(index) * 24.0f}, {500.0f, 22.0f}};
}

UiRect ringWorkshopConfirmRect()
{
    return {{688.0f, 436.0f}, {190.0f, 34.0f}};
}

UiRect bookshelfItemRect(int index)
{
    return {{402.0f, 250.0f + static_cast<float>(index) * 34.0f}, {474.0f, 30.0f}};
}

UiRect storagePanelRect()
{
    return {{44.0f, 48.0f}, {1192.0f, 624.0f}};
}

UiRect storageBackpackItemRect(int index)
{
    return {{72.0f, 154.0f + static_cast<float>(index) * 42.0f}, {332.0f, 34.0f}};
}

UiRect storageWarehouseItemRect(int index)
{
    return {{430.0f, 154.0f + static_cast<float>(index) * 42.0f}, {360.0f, 34.0f}};
}

UiRect pausePanelRect()
{
    return {{390.0f, 130.0f}, {500.0f, 460.0f}};
}

UiRect pauseMenuItemRect(int index)
{
    return {{450.0f, 235.0f + static_cast<float>(index) * 56.0f}, {380.0f, 44.0f}};
}

UiRect pauseBackButtonRect()
{
    return uiBottomLeftButtonRect(pausePanelRect(), {180.0f, 42.0f});
}

UiRect quitConfirmRect()
{
    return {{420.0f, 240.0f}, {440.0f, 220.0f}};
}

UiRect quitConfirmButtonRect(int index)
{
    const Vec2 size{150.0f, 42.0f};
    return index == 0 ? uiBottomLeftButtonRect(quitConfirmRect(), size) : uiBottomRightButtonRect(quitConfirmRect(), size);
}

UiRect gameOverPanelRect()
{
    return {{350.0f, 92.0f}, {580.0f, 520.0f}};
}

bool isCapturedProjectileBehavior(std::string_view behaviorId)
{
    return behaviorId == "shoot_web" ||
        behaviorId == "throw_stone" ||
        behaviorId == "shoot_poison" ||
        behaviorId == "radial_spike" ||
        behaviorId == "shoot_water" ||
        behaviorId == "shoot_fire";
}

std::string_view fallbackCapturedProjectileId(std::string_view behaviorId)
{
    if (behaviorId == "shoot_web") {
        return "web_thread";
    }
    if (behaviorId == "throw_stone") {
        return "stone_bullet";
    }
    if (behaviorId == "shoot_poison") {
        return "poison_spit";
    }
    if (behaviorId == "radial_spike") {
        return "cactus_needle";
    }
    if (behaviorId == "shoot_water") {
        return "water_shot";
    }
    if (behaviorId == "shoot_fire") {
        return "fire_breath";
    }
    return "stone_bullet";
}

const std::string* capturedProjectileBehaviorId(const SpellRingItem& item)
{
    if (isCapturedProjectileBehavior(item.capturedBehaviorId)) {
        return &item.capturedBehaviorId;
    }
    for (const std::string& behaviorId : item.capturedBehaviorIds) {
        if (isCapturedProjectileBehavior(behaviorId)) {
            return &behaviorId;
        }
    }
    return nullptr;
}

bool effectSpecsContain(const std::vector<EffectSpec>& specs, std::string_view effectId)
{
    for (const EffectSpec& spec : specs) {
        for (const std::string& effect : spec.effects) {
            if (effect == effectId) {
                return true;
            }
        }
    }
    return false;
}

std::vector<EffectSpec> capturedProjectileEffects(std::string_view behaviorId, const BehaviorDefinition* behavior)
{
    std::vector<EffectSpec> effects = behavior != nullptr ? behavior->capturedDefaultEffects : std::vector<EffectSpec>{};
    if (behaviorId == "shoot_web" && !effectSpecsContain(effects, "status_slow")) {
        EffectSpec slow;
        slow.target = "enemy";
        slow.effects = {"status_slow"};
        slow.values = {0.70};
        slow.duration = 2.0;
        effects.push_back(std::move(slow));
    }
    return effects;
}

UiRect gameOverItemRect(int index)
{
    return {{460.0f, 446.0f + static_cast<float>(index) * 54.0f}, {360.0f, 42.0f}};
}

UiRect stageClearPanelRect()
{
    return {{350.0f, 92.0f}, {580.0f, 520.0f}};
}

UiRect stageClearItemRect(int index)
{
    return {{460.0f, 446.0f + static_cast<float>(index) * 54.0f}, {360.0f, 42.0f}};
}

const char* pauseMenuItemName(int index)
{
    switch (index) {
    case 0: return "ステータス";
    case 1: return "アイテム";
    case 2: return "リング";
    case 3: return "オプション";
    case 4: return "ゲーム終了";
    default: return "";
    }
}

const char* baseMenuItemName(int index)
{
    switch (index) {
    case 0: return "採掘出口";
    case 1: return "収納箱";
    case 2: return "商人ワゴン";
    case 3: return "強化炉";
    case 4: return "魔導加工台";
    case 5: return "本棚";
    case 6: return "日記";
    case 7: return "リング工房用スペース";
    default: return "";
    }
}

const char* baseMiningStartChoiceName(int index)
{
    switch (index) {
    case 0: return "開始地点";
    case 1: return "最新ワープポイント";
    default: return "";
    }
}

const char* gameOverItemName(int index)
{
    switch (index) {
    case 0: return "リトライ";
    case 1: return "帰還";
    default: return "";
    }
}

const char* stageClearItemName(int index)
{
    switch (index) {
    case 0: return "拠点へ戻る";
    default: return "";
    }
}

std::string formatRunTime(float seconds)
{
    const int totalSeconds = std::max(0, static_cast<int>(seconds));
    const int minutes = totalSeconds / 60;
    const int restSeconds = totalSeconds % 60;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, restSeconds);
    return buffer;
}

UiRect ringPanelRect()
{
    return {{70.0f, 68.0f}, {1140.0f, 590.0f}};
}

UiRect ringTabRect(int index)
{
    return {{116.0f + static_cast<float>(index) * 174.0f, 132.0f}, {152.0f, 40.0f}};
}

UiRect ringSlotRect(int index)
{
    const int column = index % RingSlotColumns;
    const int row = index / RingSlotColumns;
    return {{118.0f + static_cast<float>(column) * 146.0f, 250.0f + static_cast<float>(row) * 92.0f}, {128.0f, 70.0f}};
}

const char* spellRingItemName(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return "スコップ";
    case SpellRingItemType::Torch: return "松明";
    case SpellRingItemType::Stone: return "石";
    case SpellRingItemType::Ore: return "鉱石";
    case SpellRingItemType::Object: return "Object";
    }
    return "不明";
}

const char* spellRingItemCategory(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return "採掘";
    case SpellRingItemType::Torch: return "照明";
    case SpellRingItemType::Stone: return "打撃";
    case SpellRingItemType::Ore: return "素材";
    case SpellRingItemType::Object: return "Objects DB";
    }
    return "不明";
}

const ItemData* objectForRingItem(const ObjectCatalog& catalog, const SpellRingItem& item)
{
    if (item.objectId.empty()) {
        return nullptr;
    }
    return catalog.registry.findById(item.objectId);
}

std::string ringItemDisplayName(const ObjectCatalog& catalog, const SpellRingItem& item)
{
    const ItemData* object = objectForRingItem(catalog, item);
    if (object != nullptr && !object->name.empty()) {
        return object->name;
    }
    return spellRingItemName(item.type);
}

std::string ringItemDisplayCategory(const ObjectCatalog& catalog, const SpellRingItem& item)
{
    const ItemData* object = objectForRingItem(catalog, item);
    if (object != nullptr && !object->category.empty()) {
        return object->category;
    }
    return spellRingItemCategory(item.type);
}

bool dbDebugLogEnabled()
{
#ifdef _MSC_VER
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, "MAJO_DB_DEBUG") != 0 || rawValue == nullptr) {
        return true;
    }
    const std::string value(rawValue);
    std::free(rawValue);
    const std::string_view text(value);
#else
    const char* value = std::getenv("MAJO_DB_DEBUG");
    if (value == nullptr) {
        return true;
    }
    const std::string_view text(value);
#endif
    return text != "0" && text != "false" && text != "FALSE" && text != "off" && text != "OFF";
}

bool effectDebugLogEnabled()
{
#ifdef _MSC_VER
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, "MAJO_EFFECT_DEBUG") != 0 || rawValue == nullptr) {
        return false;
    }
    const std::string value(rawValue);
    std::free(rawValue);
    const std::string_view text(value);
#else
    const char* value = std::getenv("MAJO_EFFECT_DEBUG");
    if (value == nullptr) {
        return false;
    }
    const std::string_view text(value);
#endif
    return text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON";
}

std::filesystem::path saveDataPath()
{
#ifdef _MSC_VER
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, "LOCALAPPDATA") == 0 && rawValue != nullptr && rawValue[0] != '\0') {
        const std::filesystem::path path = std::filesystem::path(rawValue) / "MajoShovel" / "save.dat";
        std::free(rawValue);
        return path;
    }
    if (rawValue != nullptr) {
        std::free(rawValue);
    }
#else
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData != nullptr && localAppData[0] != '\0') {
        return std::filesystem::path(localAppData) / "MajoShovel" / "save.dat";
    }
#endif
    return std::filesystem::path("save.dat");
}

const char* saveRingObjectId(const SpellRingItem& item)
{
    return item.objectId.empty() ? "-" : item.objectId.c_str();
}

std::string loadRingObjectId(std::string_view value)
{
    return value == "-" ? std::string{} : std::string(value);
}

std::string playerDeathCauseText(const Player& player)
{
    if ((player.lastDamageSource == DamageSource::SlimeContact ||
            player.lastDamageSource == DamageSource::SlimeAttack) &&
        !player.lastDamageEnemyName.empty()) {
        return player.lastDamageEnemyName +
            (player.lastDamageSource == DamageSource::SlimeAttack ? "の攻撃で死亡" : "の接触で死亡");
    }
    return std::string(deathCauseText(player.lastDamageSource));
}

ObjectDefinition makeCapturedObjectDefinition(const EnemyDefinition& enemy)
{
    ObjectDefinition item;
    item.id = "captured_" + enemy.id;
    item.name = enemy.name;
    item.category = "\xE8\xBB\x8C\xE9\x81\x93";
    item.description = enemy.capturedDescription;
    item.rarity = 1;
    item.price = 0;
    item.normalEffects = enemy.capturedNormalEffects;
    item.orbitEffects = enemy.capturedOrbitEffects;
    item.attackPower = enemy.capturedAttackPower;
    item.damageType = enemy.capturedDamageType.empty() ? "none" : enemy.capturedDamageType;
    item.digPower = enemy.capturedDigPower;
    item.durability = enemy.capturedDurability;
    item.weightKg = enemy.capturedWeight;
    item.tags = enemy.capturedTags;
    item.effectText = enemy.capturedEffectText;
    item.capturedBehaviorIds = enemy.capturedBehaviorIds;
    return item;
}

void upsertObjectDefinition(ObjectCatalog& catalog, ObjectDefinition item)
{
    if (item.id.empty()) {
        return;
    }
    bool replaced = false;
    for (ObjectDefinition& existing : catalog.objects) {
        if (existing.id == item.id) {
            existing = item;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        catalog.objects.push_back(item);
    }
    catalog.objectsById[item.id] = item;
    catalog.registry.rebuild(catalog.objects);
}

int ringTypeToInt(SpellRingItemType type)
{
    return static_cast<int>(type);
}

SpellRingItemType ringTypeFromInt(int value)
{
    if (value < 0 || value > static_cast<int>(SpellRingItemType::Object)) {
        return SpellRingItemType::Stone;
    }
    return static_cast<SpellRingItemType>(value);
}

std::size_t countIssues(
    const ObjectCatalog& catalog,
    DbValidationSeverity severity,
    DbValidationCategory category)
{
    return static_cast<std::size_t>(std::count_if(
        catalog.validationIssues.begin(),
        catalog.validationIssues.end(),
        [severity, category](const DbValidationIssue& issue) {
            return issue.severity == severity && issue.category == category;
        }));
}

std::size_t countSeverity(const ObjectCatalog& catalog, DbValidationSeverity severity)
{
    return static_cast<std::size_t>(std::count_if(
        catalog.validationIssues.begin(),
        catalog.validationIssues.end(),
        [severity](const DbValidationIssue& issue) {
            return issue.severity == severity;
        }));
}

void logIssueList(
    const ObjectCatalog& catalog,
    std::string_view title,
    DbValidationSeverity severity,
    DbValidationCategory category)
{
    const std::size_t count = countIssues(catalog, severity, category);
    logError(std::string(title) + ": " + std::to_string(count));
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        if (issue.severity == severity && issue.category == category) {
            logError("  [" + std::string(dbValidationSeverityName(issue.severity)) + "] " + issue.message);
        }
    }
}

void addTargetCount(std::vector<std::pair<std::string, std::size_t>>& counts, std::string_view target)
{
    if (target.empty()) {
        return;
    }
    const auto it = std::find_if(counts.begin(), counts.end(), [target](const auto& entry) {
        return entry.first == target;
    });
    if (it != counts.end()) {
        ++it->second;
        return;
    }
    counts.emplace_back(target, 1);
}

void logEffectTargetUsage(const ObjectCatalog& catalog)
{
    std::vector<std::pair<std::string, std::size_t>> counts;
    for (const ObjectDefinition& object : catalog.objects) {
        for (const EffectSpec& spec : object.normalEffects) {
            addTargetCount(counts, spec.target);
        }
        for (const EffectSpec& spec : object.orbitEffects) {
            addTargetCount(counts, spec.target);
        }
    }

    std::sort(counts.begin(), counts.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });

    logError("EffectSpec targets used: " + std::to_string(counts.size()));
    for (const auto& [target, count] : counts) {
        logError("  target=\"" + target + "\" specs=" + std::to_string(count));
    }
}

void logDungeonGenerationAudit()
{
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

    logError("=== Dungeon generation extension pre-audit ===");
    logError("[audit] chunks: TileMap::updateAround keeps the player-centered active window; TileMap::render/tileAtWorld can also lazily create missing chunks.");
    logError("[audit] terrain: TileMap::initializeChunk remains chunk-local but samples Game-owned DungeonLayout distance fields for initial terrain.");
    logError("[audit] tile_hp: TileType={Empty,Dirt,Rock,Ore,HardRock}; HP is scaled by stage and local hardness.");
    logError("[audit] digging: DiggingSystem returns hit/opened/dug tiles, rewardDropRequests, and capturedExplosionRequests; opened tiles are the hook for buried rewards or hidden enemies.");
    logError("[audit] enemies: EnemySystem::spawnFromDugTiles uses openedTiles and enemyDugSpawnEvery; generated hidden enemies should be consumed through a separate per-tile marker before this random spawn path.");
    logError("[audit] lights: Game::render already composes extra LightSource entries for ring items, warp points, and boss spawn markers before TileMap/Drop/Projectile/Enemy rendering.");
    logError("[audit] drops: WorldDropSystem consumes DugTile entries and Objects DB candidates; pickup is player contact via InventorySystem.");
    logError("[audit] object_tags: special tags are metadata/filter labels; runtime effects go through EffectSpec and EffectDispatcher, not direct tag execution.");
    logError("[audit] stage_params: StageParams DB is intentionally not present for this pass.");
    logError("=== End dungeon generation extension pre-audit ===");
}

std::uint32_t makeDungeonSeed(int stageId, bool roguelike)
{
    std::random_device device;
    const std::uint32_t a = device();
    const std::uint32_t b = device();
    const std::uint32_t modeSalt = roguelike ? 0x9E3779B9u : 0x85EBCA6Bu;
    return a ^ (b << 1) ^ (static_cast<std::uint32_t>(std::max(1, stageId)) * 0x27D4EB2Du) ^ modeSalt;
}

float stageHardnessMultiplierForStageId(int stageId)
{
    switch (stageId) {
    case 1:
        return 1.0f;
    case 2:
        return 2.0f;
    case 3:
        return 3.5f;
    case 4:
        return 4.0f;
    default:
        return 1.0f + static_cast<float>(std::max(0, stageId - 1)) * 0.75f;
    }
}

StageDefinition makeCodeDefaultStageDefinition()
{
    StageDefinition stage;
    stage.id = "stage_01_stardust";
    stage.name = "星くずの浅坑";
    stage.type = "ストーリー";
    stage.displayOrder = 10;
    stage.implementationState = "code_default";
    stage.generationProfile = "natural_cave";
    stage.terrainProfile = "soft_stardust";
    stage.goalDistanceTiles = 320;
    stage.detourRate = 0.30;
    stage.branchDensity = 0.25;
    stage.cavernWidthMultiplier = 1.00;
    stage.terrainHardnessMultiplier = 1.00;
    stage.warpPointCount = 3;
    stage.specialRoomCount = 1;
    return stage;
}

void logCurrentStageDefinition(const StageDefinition& stage, std::string_view reason)
{
    logError(std::string("Current stage resolved") + (reason.empty() ? std::string{} : ": " + std::string(reason)));
    logError("  currentStageId=\"" + stage.id +
        "\" name=\"" + stage.name +
        "\" type=\"" + stage.type +
        "\" generation_profile=\"" + stage.generationProfile +
        "\" terrain_profile=\"" + stage.terrainProfile + "\"");
    logError("  goal_distance_tiles=" + std::to_string(stage.goalDistanceTiles) +
        " terrain_hardness_multiplier=" + std::to_string(stage.terrainHardnessMultiplier) +
        " warp_point_count=" + std::to_string(stage.warpPointCount) +
        " special_room_count=" + std::to_string(stage.specialRoomCount));
}

void logDbValidationReport(const ObjectCatalog& catalog)
{
    if (!dbDebugLogEnabled()) {
        return;
    }

    logError("=== Object DB validation report ===");
    logError("Objects: " + std::to_string(catalog.objects.size()));
    logError("Object IDs: " + std::to_string(catalog.objectsById.size()));
    logError("Effect codes: " + std::to_string(catalog.effectCodes.size()));
    logError("Special tags: " + std::to_string(catalog.specialTags.size()));
    logError("Errors: " + std::to_string(countSeverity(catalog, DbValidationSeverity::Error)));
    logError("Warnings: " + std::to_string(countSeverity(catalog, DbValidationSeverity::Warning)));
    logEffectTargetUsage(catalog);
    logIssueList(catalog, "EffectSpec syntax errors", DbValidationSeverity::Error, DbValidationCategory::EffectSpecSyntax);
    logIssueList(catalog, "Duplicate IDs", DbValidationSeverity::Warning, DbValidationCategory::DuplicateId);
    logIssueList(catalog, "Undefined effect codes", DbValidationSeverity::Warning, DbValidationCategory::UndefinedEffectCode);
    logIssueList(catalog, "Target mismatches", DbValidationSeverity::Warning, DbValidationCategory::TargetMismatch);
    logIssueList(catalog, "Undefined special tags", DbValidationSeverity::Warning, DbValidationCategory::UndefinedSpecialTag);
    logIssueList(catalog, "Exclusive tag conflicts", DbValidationSeverity::Warning, DbValidationCategory::ExclusiveTagConflict);
    logIssueList(catalog, "Deprecated or compatibility entries", DbValidationSeverity::Warning, DbValidationCategory::DeprecatedEntry);

    std::size_t otherCount = 0;
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        if (issue.category != DbValidationCategory::EffectSpecSyntax &&
            issue.category != DbValidationCategory::DuplicateId &&
            issue.category != DbValidationCategory::UndefinedEffectCode &&
            issue.category != DbValidationCategory::TargetMismatch &&
            issue.category != DbValidationCategory::UndefinedSpecialTag &&
            issue.category != DbValidationCategory::ExclusiveTagConflict &&
            issue.category != DbValidationCategory::DeprecatedEntry) {
            ++otherCount;
        }
    }
    logError("Other DB validation issues: " + std::to_string(otherCount));
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        if (issue.category != DbValidationCategory::EffectSpecSyntax &&
            issue.category != DbValidationCategory::DuplicateId &&
            issue.category != DbValidationCategory::UndefinedEffectCode &&
            issue.category != DbValidationCategory::TargetMismatch &&
            issue.category != DbValidationCategory::UndefinedSpecialTag &&
            issue.category != DbValidationCategory::ExclusiveTagConflict &&
            issue.category != DbValidationCategory::DeprecatedEntry) {
            logError("  [" + std::string(dbValidationSeverityName(issue.severity)) + "][" +
                std::string(dbValidationCategoryName(issue.category)) + "] " + issue.message);
        }
    }
    logError("=== End Object DB validation report ===");
}

std::size_t countEnemySeverity(const EnemyCatalog& catalog, DbValidationSeverity severity)
{
    return static_cast<std::size_t>(std::count_if(
        catalog.validationIssues.begin(),
        catalog.validationIssues.end(),
        [severity](const DbValidationIssue& issue) {
            return issue.severity == severity;
        }));
}

void logEnemyDbValidationReport(const EnemyCatalog& catalog)
{
    if (!dbDebugLogEnabled()) {
        return;
    }

    logError("=== Enemy DB validation report ===");
    logError("Enemies: " + std::to_string(catalog.enemies.size()));
    logError("Enemy IDs: " + std::to_string(catalog.enemiesById.size()));
    logError("Behaviors: " + std::to_string(catalog.behaviors.size()));
    logError("Behavior IDs: " + std::to_string(catalog.behaviorsById.size()));
    logError("Errors: " + std::to_string(countEnemySeverity(catalog, DbValidationSeverity::Error)));
    logError("Warnings: " + std::to_string(countEnemySeverity(catalog, DbValidationSeverity::Warning)));
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        logError("  [" + std::string(dbValidationSeverityName(issue.severity)) + "][" +
            std::string(dbValidationCategoryName(issue.category)) + "] " + issue.message);
    }
    logError("=== End Enemy DB validation report ===");
}

void logStageCatalogReport(const StageCatalog& catalog, bool loadedFromSheet, bool usedFallback, std::string_view error)
{
    logError("Stages sheet load " + std::string(loadedFromSheet ? "succeeded" : "failed"));
    if (!loadedFromSheet && !error.empty()) {
        logError("Stages sheet error: " + std::string(error));
    }
    logError("Stages fallback " + std::string(usedFallback ? "used" : "not used"));
    logError("Stages loaded: " + std::to_string(catalog.stages.size()));
    for (const StageDefinition& stage : catalog.getStagesSortedByDisplayOrder()) {
        logError("  stage id=\"" + stage.id +
            "\" name=\"" + stage.name +
            "\" generation_profile=\"" + stage.generationProfile +
            "\" terrain_profile=\"" + stage.terrainProfile + "\"");
    }
    logError("Stage validation warnings: " + std::to_string(catalog.validationWarnings.size()));
    for (const std::string& warning : catalog.validationWarnings) {
        logError("  [warning] " + warning);
    }
}

void logEffectDispatcherSmoke(const ObjectCatalog& catalog, const EffectDispatcher& dispatcher)
{
    if (!effectDebugLogEnabled()) {
        return;
    }

    logError("=== EffectDispatcher debug smoke ===");
    logError("Registered handlers: " + std::to_string(dispatcher.handlerCount()));
    for (const ObjectDefinition& object : catalog.objects) {
        EffectContext context;
        dispatcher.dispatchNormalEffects(object, context);
        context = EffectContext{};
        dispatcher.dispatchOrbitEffects(object, context);
    }
    logError("=== End EffectDispatcher debug smoke ===");
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
    initializeWorld(false);
    if (loadSaveData()) {
        reloadNotice_ = "セーブ読込完了";
    } else {
        reloadNotice_ = message.empty() ? "データ読込完了" : message;
    }
    enterBase();
    reloadNoticeTimer_ = 2.0f;
}

void Game::initializeWorld(bool captureRunStartInventory)
{
    player_ = Player{};
    player_.position = {0.0f, 0.0f};
    player_.xpToNext = balance_.xpBase + player_.level * balance_.xpPerLevel;
    tileMap_ = TileMap{};
    spellRing_ = SpellRingSystem{};
    effects_ = EffectSystem{};
    enemies_ = EnemySystem{};
    projectiles_ = ProjectileSystem{};
    inventory_ = InventorySystem{};
    worldDrops_ = WorldDropSystem{};
    levels_ = LevelSystem{};
    upgrades_ = UpgradeSystem{};
    mode_ = ScreenMode::Playing;
    baseMenuSelection_ = 0;
    baseMiningStartChoiceActive_ = false;
    baseMiningStartSelection_ = 0;
    baseStorageActive_ = false;
    baseStorageWarehousePane_ = false;
    baseStorageBackpackSelection_ = 0;
    baseStorageWarehouseSelection_ = 0;
    baseSellActive_ = false;
    baseMerchantBuyPane_ = false;
    baseSellSelection_ = 0;
    baseMerchantBuySelection_ = 0;
    baseUpgradeActive_ = false;
    baseUpgradeSelection_ = 0;
    baseProcessingActive_ = false;
    baseProcessingMode_ = 0;
    baseProcessingSelection_ = 0;
    baseRingWorkshopActive_ = false;
    baseRingWorkshopSelection_ = 0;
    ringWorkshopDraftRadiusPoints_ = levelRingRadiusPoints_;
    ringWorkshopDraftSpeedPoints_ = levelRingSpeedPoints_;
    baseBookshelfActive_ = false;
    bookshelfPage_ = BookshelfPage::Menu;
    bookshelfSelection_ = 0;
    baseStatus_.clear();
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    pauseMenuSelection_ = 0;
    pauseConfirmSelection_ = 0;
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
    retrySnapshot_ = RetrySnapshot{};
    inventoryReturnToPause_ = false;
    debugPaused_ = false;
    captureCooldown_ = 0.0f;
    currentStage_ = std::clamp(currentStage_, 0, std::max(0, unlockedStages_ - 1));
    generateDungeonLayoutForRun();
    resetWarpPointRunState();
    initializeRewardNodesFromLayout();
    initializeEnemyNodesFromLayout();
    spellRing_.initialize(balance_);
    applyPermanentUpgrades();
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    // Future connection: TileMap chunk initialization will consult
    // currentStageDefinition().terrainProfile and terrainHardnessMultiplier.
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    logDungeonGenerationAudit();
    if (captureRunStartInventory) {
        captureRunStartInventoryState();
    }
}

void Game::enterBase()
{
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
    baseMerchantBuyPane_ = false;
    baseUpgradeActive_ = false;
    baseProcessingActive_ = false;
    baseRingWorkshopActive_ = false;
    baseBookshelfActive_ = false;
    baseMenuSelection_ = std::clamp(baseMenuSelection_, 0, BaseMenuItemCount - 1);
    clearTemporaryPlayerState(true);
}

void Game::startMiningFromBase(bool useLatestWarpPoint)
{
    useLatestWarpPoint = useLatestWarpPoint && unlockedWarpPointCount_ > 0;
    InventoryCarryState retained = captureInventoryCarryState();
    const int retainedLevel = player_.level;
    const int retainedXp = player_.xp;
    const int retainedXpToNext = player_.xpToNext;
    initializeWorld(false);
    restoreInventoryCarryState(retained);
    player_.level = retainedLevel;
    player_.xp = retainedXp;
    player_.xpToNext = retainedXpToNext;
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    captureRunStartInventoryState();
    resetWarpPointRunState();
    if (useLatestWarpPoint) {
        const Vec2 startPosition = latestWarpPointStartPosition();
        latestWarpPointPosition_ = startPosition;
        hasLatestWarpPointPosition_ = true;
        player_.position = startPosition;
        rebuildUnlockedWarpPointsForStart(startPosition);
        tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
        captureRetrySnapshotAtWarpPoint();
    }
    mode_ = ScreenMode::Playing;
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
    watcher_.watchPath("assets");
    watcher_.reset();
}

void Game::loadSheetSourceConfig()
{
    std::string error;
    if (!loadGoogleSheetSourceConfig("data/google_sheet_source.cfg", sheetSource_, error)) {
        sheetSource_ = GoogleSheetSourceConfig{};
        std::fprintf(stderr, "Google Sheet source disabled: %s\n", error.c_str());
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
        std::fprintf(stderr, "Runtime balance load failed: %s\n", error.c_str());
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

    std::fprintf(stderr, "Google Sheet balance load failed: %s\n", sheetError.c_str());
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
        return false;
    }

    ObjectCatalog loaded;
    std::string error;
    if (!loadObjectCatalogFromGoogleSheet(sheetSource_, loaded, error)) {
        std::fprintf(stderr, "Objects sheet load failed: %s\n", error.c_str());
        return false;
    }

    objectCatalog_ = std::move(loaded);
    effectDispatcher_.clearHandlers();
    effectDispatcher_.registerFoundationHandlers(objectCatalog_);
    logDbValidationReport(objectCatalog_);
    logEffectDispatcherSmoke(objectCatalog_, effectDispatcher_);
    logError("Objects sheet loaded: " + std::to_string(objectCatalog_.registry.size()) + " items");
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
            << " normal_effects=" << object.normalEffects.size()
            << " orbit_effects=" << object.orbitEffects.size()
            << " tags=" << object.tags.size();
        logError(line.str());
    }
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
        std::fprintf(stderr, "Stages sheet load failed: %s\n", error.c_str());
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
        std::fprintf(stderr, "Enemies sheet load failed: %s\n", error.c_str());
        return false;
    }

    enemyCatalog_ = std::move(loaded);
    for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
        upsertObjectDefinition(objectCatalog_, makeCapturedObjectDefinition(enemy));
    }
    logEnemyDbValidationReport(enemyCatalog_);
    logError("Enemies sheet loaded: " + std::to_string(enemyCatalog_.enemies.size()) +
        " enemies, " + std::to_string(enemyCatalog_.enemiesById.size()) + " unique IDs");
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
            << " captured_tags=" << enemy.capturedTags.size();
        logError(line.str());
        ++enemyLogCount;
    }

    if (!enemyCatalog_.validationWarnings.empty()) {
        logError("Enemy DB warnings: " + std::to_string(enemyCatalog_.validationWarnings.size()));
    }
    return true;
}

bool Game::isSellableObject(const ItemData& item) const
{
    return !isStoryObject(item);
}

bool Game::isStoryObject(const ItemData& item) const
{
    if (item.category == "\xE3\x82\xB9\xE3\x83\x88\xE3\x83\xBC\xE3\x83\xAA\xE3\x83\xBC") {
        return true;
    }
    for (const std::string& tag : item.tags) {
        if (tag == "story" || tag == "story_item" || tag == "key_item" || tag == "unsellable" || tag == "no_sell") {
            return true;
        }
    }
    return false;
}

int Game::sellPrice(const ItemData& item, const ItemInstance* /*instance*/) const
{
    // Future hooks: enhancement bonus, broken penalty, category multiplier, merchant buy-rate.
    double multiplier = 1.0;
    if (merchantUpgradeLevel_ >= 6) {
        multiplier = 1.2;
    } else if (merchantUpgradeLevel_ >= 3) {
        multiplier = 1.1;
    }
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(item.price) * multiplier)));
}

std::vector<Game::SellableEntry> Game::sellableObjects() const
{
    std::vector<SellableEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(i)];
        if (stack.count <= 0) {
            continue;
        }
        SellableEntry entry{SellableKind::Stack, i};
        entry.price = sellPrice(stack.item);
        entry.sellable = isSellableObject(stack.item);
        if (!entry.sellable) {
            entry.blockedReason = "売却不可";
        }
        entries.push_back(std::move(entry));
    }
    const auto& instances = inventory_.objectInstances();
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(i)];
        SellableEntry entry{SellableKind::Instance, i};
        entry.price = sellPrice(instance.item, &instance.instance);
        if (instance.instance.protectionEnabled) {
            entry.sellable = false;
            entry.blockedReason = "保護ON";
        } else if (!isSellableObject(instance.item)) {
            entry.sellable = false;
            entry.blockedReason = "売却不可";
        } else {
            entry.sellable = true;
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

void Game::refreshMerchantStock(bool force)
{
    if (!force && !merchantStock_.empty()) {
        return;
    }

    std::vector<const ItemData*> candidates;
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const ItemData* item = objectCatalog_.registry.findById(object.id);
        if (item == nullptr || item->id.empty() || isStoryObject(*item)) {
            continue;
        }
        const bool basicCategory =
            item->category == "\xE5\x9B\x9E\xE5\xBE\xA9" ||
            item->category == "\xE6\x8E\xA2\xE7\xB4\xA2" ||
            item->category == "\xE5\xBC\xB7\xE5\x8C\x96";
        const bool basicTag = std::any_of(item->tags.begin(), item->tags.end(), [](const std::string& tag) {
            return tag == "consumable" || tag == "potion" || tag == "food";
        });
        if ((basicCategory || basicTag) && item->price > 0) {
            candidates.push_back(item);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const ItemData* left, const ItemData* right) {
        if (left->price != right->price) {
            return left->price < right->price;
        }
        return left->id < right->id;
    });

    merchantStock_.clear();
    if (candidates.empty()) {
        return;
    }

    ++merchantStockVersion_;
    const int stockCount = std::min(merchantUpgradeLevel_ >= 2 ? 5 : 4, static_cast<int>(candidates.size()));
    const int start = merchantStockVersion_ % static_cast<int>(candidates.size());
    for (int i = 0; i < stockCount; ++i) {
        const ItemData* item = candidates[static_cast<std::size_t>((start + i) % static_cast<int>(candidates.size()))];
        merchantStock_.push_back(MerchantProduct{item->id, std::max(1, item->price)});
    }
}

void Game::buyMerchantProduct(int index)
{
    refreshMerchantStock(false);
    if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
        baseStatus_ = "購入できる商品がありません";
        return;
    }

    const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(index)];
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    if (item == nullptr) {
        baseStatus_ = "商品データがありません";
        return;
    }
    if (money_ < product.price) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (!inventory_.addObjectItem(objectCatalog_, product.objectId)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    money_ -= product.price;
    baseStatus_ = "購入しました";
}

std::vector<Game::StorageEntry> Game::processingEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

const char* Game::processingModeName(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Repair: return "修理";
    case ProcessingMode::Attack: return "攻撃力強化";
    case ProcessingMode::Dig: return "掘削力強化";
    case ProcessingMode::Durability: return "耐久力強化";
    }
    return "";
}

bool Game::processingEntryAvailable(StorageEntry entry) const
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    if (entry.kind == StorageEntryKind::Stack) {
        return mode != ProcessingMode::Repair;
    }
    const InventoryObjectInstance& object = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    if (mode == ProcessingMode::Repair) {
        const ItemInstance& instance = object.instance;
        return instance.maxDurability >= 0 && (instance.isBroken || instance.currentDurability < instance.maxDurability);
    }
    return object.instance.enhanceLevel < MaxItemEnhanceLevel;
}

int Game::processingMoneyCost(StorageEntry entry, ProcessingMode mode) const
{
    const ItemData* item = storageEntryItem(entry, false);
    const int basePrice = std::max(1, item != nullptr ? item->price : 0);
    if (mode == ProcessingMode::Repair) {
        const ItemInstance* instance = storageEntryInstance(entry, false);
        if (instance == nullptr || instance->maxDurability <= 0) {
            return 0;
        }
        if (instance->isBroken) {
            return std::max(1, static_cast<int>(std::ceil(static_cast<double>(basePrice) * 0.6)));
        }
        const int missing = std::max(0, instance->maxDurability - instance->currentDurability);
        if (missing <= 0) {
            return 0;
        }
        const double ratio = static_cast<double>(missing) / static_cast<double>(instance->maxDurability);
        return std::max(1, static_cast<int>(std::ceil(static_cast<double>(basePrice) * ratio * 0.4)));
    }

    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, false)) {
        enhanceLevel = instance->enhanceLevel;
    }
    return std::max(20, basePrice / 2 + (enhanceLevel + 1) * 50);
}

int Game::processingOreCost(StorageEntry entry, ProcessingMode mode) const
{
    if (mode == ProcessingMode::Repair) {
        return 0;
    }
    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, false)) {
        enhanceLevel = instance->enhanceLevel;
    }
    return enhanceLevel + 1;
}

void Game::applyProcessing(int entryIndex)
{
    const std::vector<StorageEntry> entries = processingEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    if (!processingEntryAvailable(entry)) {
        baseStatus_ = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
        return;
    }

    const int moneyCost = processingMoneyCost(entry, mode);
    const int oreCost = processingOreCost(entry, mode);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りません";
        return;
    }

    bool processed = false;
    if (mode == ProcessingMode::Repair) {
        const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
        processed = inventory_.repairObjectInstance(instance.instance.instanceId);
    } else {
        int attackBonus = 0;
        int digBonus = 0;
        int durabilityBonus = 0;
        if (mode == ProcessingMode::Attack) {
            attackBonus = 1;
        } else if (mode == ProcessingMode::Dig) {
            digBonus = 1;
        } else if (mode == ProcessingMode::Durability) {
            durabilityBonus = 2;
        }
        if (entry.kind == StorageEntryKind::Stack) {
            const InventoryObjectStack& stack = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectStackItem(stack.objectId, attackBonus, digBonus, durabilityBonus, MaxItemEnhanceLevel);
        } else {
            const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectInstance(instance.instance.instanceId, attackBonus, digBonus, durabilityBonus, MaxItemEnhanceLevel);
        }
    }
    if (!processed) {
        baseStatus_ = "加工できません";
        return;
    }

    money_ -= moneyCost;
    if (oreCost > 0) {
        const bool spentOre = inventory_.materials().spend(MaterialType::EnhancementOre, oreCost);
        (void)spentOre;
    }
    baseStatus_ = mode == ProcessingMode::Repair ? "修理しました" : "強化しました";
    baseProcessingSelection_ = std::min(baseProcessingSelection_, std::max(0, static_cast<int>(processingEntries().size()) - 1));
}

int Game::warehouseCapacity() const
{
    constexpr std::array<int, 5> Capacities{{48, 72, 100, 140, 200}};
    const int level = std::clamp(warehouseCapacityLevel_, 0, static_cast<int>(Capacities.size()) - 1);
    return Capacities[static_cast<std::size_t>(level)];
}

int Game::warehouseUsedSlots() const
{
    return static_cast<int>(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
}

int Game::backpackUsedSlots() const
{
    return static_cast<int>(inventory_.objectStacks().size() + inventory_.objectInstances().size());
}

std::vector<Game::StorageEntry> Game::backpackStorageEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

std::vector<Game::StorageEntry> Game::warehouseStorageEntries() const
{
    std::vector<StorageEntry> entries;
    entries.reserve(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
    for (int i = 0; i < static_cast<int>(warehouseObjectStacks_.size()); ++i) {
        if (warehouseObjectStacks_[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(warehouseObjectInstances_.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

void Game::depositBackpackEntry(int entryIndex)
{
    const std::vector<StorageEntry> entries = backpackStorageEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "預けるアイテムがありません";
        return;
    }

    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    if (entry.kind == StorageEntryKind::Stack) {
        const InventoryObjectStack& source = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
        auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [&source](const InventoryObjectStack& stack) {
            return stack.objectId == source.objectId;
        });
        if (it == warehouseObjectStacks_.end()) {
            if (warehouseUsedSlots() >= warehouseCapacity()) {
                baseStatus_ = "倉庫がいっぱいです";
                return;
            }
            warehouseObjectStacks_.push_back(InventoryObjectStack{source.item, 0});
            it = warehouseObjectStacks_.end() - 1;
        }
        const std::string objectId = source.objectId;
        if (!inventory_.removeObjectItemCount(objectId, 1)) {
            baseStatus_ = "預け入れに失敗しました";
            return;
        }
        ++it->count;
        baseStatus_ = "倉庫へ預けました";
        baseStorageBackpackSelection_ = std::min(baseStorageBackpackSelection_, std::max(0, static_cast<int>(backpackStorageEntries().size()) - 1));
        return;
    }

    if (warehouseUsedSlots() >= warehouseCapacity()) {
        baseStatus_ = "倉庫がいっぱいです";
        return;
    }
    const InventoryObjectInstance& source = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    InventoryObjectInstance moved;
    if (!inventory_.takeObjectInstance(source.instance.instanceId, moved)) {
        baseStatus_ = "預け入れに失敗しました";
        return;
    }
    warehouseObjectInstances_.push_back(std::move(moved));
    baseStatus_ = "倉庫へ預けました";
    baseStorageBackpackSelection_ = std::min(baseStorageBackpackSelection_, std::max(0, static_cast<int>(backpackStorageEntries().size()) - 1));
}

void Game::withdrawWarehouseEntry(int entryIndex)
{
    const std::vector<StorageEntry> entries = warehouseStorageEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "取り出すアイテムがありません";
        return;
    }

    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    if (entry.kind == StorageEntryKind::Stack) {
        InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(entry.index)];
        const std::string objectId = stack.objectId;
        if (!inventory_.addObjectItem(objectCatalog_, objectId)) {
            baseStatus_ = "リュックがいっぱいです";
            return;
        }
        --stack.count;
        if (stack.count <= 0) {
            warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + entry.index);
        }
        baseStatus_ = "リュックへ取り出しました";
        baseStorageWarehouseSelection_ = std::min(baseStorageWarehouseSelection_, std::max(0, static_cast<int>(warehouseStorageEntries().size()) - 1));
        return;
    }

    InventoryObjectInstance moved = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)];
    if (!inventory_.addObjectInstance(objectCatalog_, moved.instance)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + entry.index);
    baseStatus_ = "リュックへ取り出しました";
    baseStorageWarehouseSelection_ = std::min(baseStorageWarehouseSelection_, std::max(0, static_cast<int>(warehouseStorageEntries().size()) - 1));
}

std::string Game::storageEntryLabel(StorageEntry entry, bool warehouseEntry) const
{
    char buffer[192];
    if (entry.kind == StorageEntryKind::Stack) {
        const InventoryObjectStack& stack = warehouseEntry
            ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)]
            : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
        std::snprintf(buffer, sizeof(buffer), "%s x%d", stack.item.name.c_str(), stack.count);
        return buffer;
    }

    const InventoryObjectInstance& instance = warehouseEntry
        ? warehouseObjectInstances_[static_cast<std::size_t>(entry.index)]
        : inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    std::snprintf(buffer, sizeof(buffer), "%s %s%s Lv.%d",
        instance.item.name.c_str(),
        instance.instance.protectionEnabled ? "[保護] " : "",
        instance.instance.isBroken ? "[破損] " : "",
        instance.instance.enhanceLevel);
    return buffer;
}

const ItemData* Game::storageEntryItem(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind == StorageEntryKind::Stack) {
        return warehouseEntry
            ? &warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].item
            : &inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].item;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].item
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].item;
}

const ItemInstance* Game::storageEntryInstance(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Instance) {
        return nullptr;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].instance;
}

int Game::storageEntryStackCount(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Stack) {
        return 1;
    }
    return warehouseEntry
        ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].count
        : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].count;
}

int Game::upgradeCost(int index) const
{
    switch (index) {
    case 0: return 150 + warehouseCapacityLevel_ * 100;
    case 1: return 120 + (merchantUpgradeLevel_ - 1) * 120;
    case 2: return 180 + processingUnlockLevel_ * 140;
    case 3: return 300;
    case 4: return 100 + maxHpUpgradeLevel_ * 50;
    case 5: return 150 + ringRadiusUpgradeLevel_ * 75;
    case 6: return 150 + ringSpeedUpgradeLevel_ * 75;
    default: return 0;
    }
}

MaterialType Game::upgradeMaterialType(int index) const
{
    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
        return MaterialType::OldWoodBuildingMaterial;
    case 4:
    case 5:
    case 6:
    case 7:
        return MaterialType::ManaDrop;
    default:
        return MaterialType::OldWoodBuildingMaterial;
    }
}

int Game::upgradeMaterialCost(int index) const
{
    switch (index) {
    case 0: return warehouseCapacityLevel_ + 2;
    case 1: return merchantUpgradeLevel_ + 1;
    case 2: return processingUnlockLevel_ + 2;
    case 3: return ringWorkshopUnlocked_ ? 0 : 5;
    case 4: return maxHpUpgradeLevel_ + 1;
    case 5: return ringRadiusUpgradeLevel_ + 1;
    case 6: return ringSpeedUpgradeLevel_ + 1;
    default: return 0;
    }
}

const char* Game::upgradeName(int index) const
{
    switch (index) {
    case 0: return "倉庫容量強化";
    case 1: return "商人機能強化";
    case 2: return "魔導加工台機能解禁";
    case 3: return "リング工房解禁";
    case 4: return "最大HPアップ";
    case 5: return "リング半径アップ";
    case 6: return "リング速度アップ";
    case 7: return "プレイ性能強化枠";
    default: return "";
    }
}

int Game::upgradeLevel(int index) const
{
    switch (index) {
    case 0: return warehouseCapacityLevel_ + 1;
    case 1: return merchantUpgradeLevel_;
    case 2: return processingUnlockLevel_;
    case 3: return ringWorkshopUnlocked_ ? 1 : 0;
    case 4: return maxHpUpgradeLevel_;
    case 5: return ringRadiusUpgradeLevel_;
    case 6: return ringSpeedUpgradeLevel_;
    case 7: return 0;
    default: return 0;
    }
}

int Game::upgradeMaxLevel(int index) const
{
    switch (index) {
    case 0: return 5;
    case 1: return 7;
    case 2: return 5;
    case 3: return 1;
    case 4:
    case 5:
    case 6:
        return 5;
    default:
        return 0;
    }
}

bool Game::upgradeImplemented(int index) const
{
    return index >= 0 && index <= 6;
}

bool Game::upgradeMaxed(int index) const
{
    const int maxLevel = upgradeMaxLevel(index);
    return maxLevel <= 0 || upgradeLevel(index) >= maxLevel;
}

void Game::buyUpgrade(int index)
{
    if (!upgradeImplemented(index)) {
        baseStatus_ = "この強化枠は未実装です";
        return;
    }
    if (upgradeMaxed(index)) {
        baseStatus_ = "強化上限です";
        return;
    }
    const int cost = upgradeCost(index);
    if (cost <= 0) {
        return;
    }
    if (money_ < cost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    const MaterialType materialType = upgradeMaterialType(index);
    const int materialCost = upgradeMaterialCost(index);
    if (materialCost > 0 && inventory_.materialCount(materialType) < materialCost) {
        baseStatus_ = std::string(materialTypeDisplayName(materialType)) + "が足りません";
        return;
    }

    money_ -= cost;
    if (materialCost > 0) {
        const bool spent = inventory_.materials().spend(materialType, materialCost);
        (void)spent;
    }
    switch (index) {
    case 0:
        ++warehouseCapacityLevel_;
        break;
    case 1:
        ++merchantUpgradeLevel_;
        refreshMerchantStock(true);
        break;
    case 2:
        ++processingUnlockLevel_;
        break;
    case 3:
        ringWorkshopUnlocked_ = true;
        break;
    case 4:
        ++maxHpUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 5:
        ++ringRadiusUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 6:
        ++ringSpeedUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    default:
        break;
    }
    baseStatus_ = "強化しました";
}

void Game::openRingWorkshop()
{
    if (!ringWorkshopUnlocked_) {
        baseStatus_ = "リング工房はまだ解禁されていません";
        return;
    }
    baseRingWorkshopActive_ = true;
    baseRingWorkshopSelection_ = 0;
    resetRingWorkshopDraft();
    baseStatus_.clear();
}

void Game::resetRingWorkshopDraft()
{
    ringWorkshopDraftRadiusPoints_ = levelRingRadiusPoints_;
    ringWorkshopDraftSpeedPoints_ = levelRingSpeedPoints_;
}

int Game::ringLevelUpgradePointTotal() const
{
    return std::max(0, levelRingRadiusPoints_) + std::max(0, levelRingSpeedPoints_);
}

bool Game::ringWorkshopRespecChanged() const
{
    return ringWorkshopDraftRadiusPoints_ != levelRingRadiusPoints_ ||
        ringWorkshopDraftSpeedPoints_ != levelRingSpeedPoints_;
}

int Game::ringWorkshopRespecMoneyCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    return 80 + ringLevelUpgradePointTotal() * 20;
}

int Game::ringWorkshopRespecMoonCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    return 1 + ringLevelUpgradePointTotal() / 3;
}

bool Game::adjustRingWorkshopRespec(int direction)
{
    if (ringLevelUpgradePointTotal() <= 0) {
        baseStatus_ = "再調整できるリング強化ポイントがありません";
        return false;
    }
    if (direction > 0) {
        if (ringWorkshopDraftSpeedPoints_ <= 0) {
            baseStatus_ = "速度から移せるポイントがありません";
            return false;
        }
        --ringWorkshopDraftSpeedPoints_;
        ++ringWorkshopDraftRadiusPoints_;
    } else {
        if (ringWorkshopDraftRadiusPoints_ <= 0) {
            baseStatus_ = "半径から移せるポイントがありません";
            return false;
        }
        --ringWorkshopDraftRadiusPoints_;
        ++ringWorkshopDraftSpeedPoints_;
    }
    baseStatus_ = "配分案を変更しました。確定で支払います";
    return true;
}

void Game::confirmRingWorkshopRespec()
{
    if (!ringWorkshopRespecChanged()) {
        baseStatus_ = "配分は変更されていません";
        return;
    }
    const int moneyCost = ringWorkshopRespecMoneyCost();
    const int moonCost = ringWorkshopRespecMoonCost();
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    levelRingRadiusPoints_ = std::max(0, ringWorkshopDraftRadiusPoints_);
    levelRingSpeedPoints_ = std::max(0, ringWorkshopDraftSpeedPoints_);
    applyPermanentUpgrades();
    baseStatus_ = "リング強化の配分を再調整しました";
}

const char* Game::ringWorkshopUpgradeName(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return "初期リング半径強化";
    case RingWorkshopUpgrade::InitialSpeed:
        return "初期リング速度強化";
    case RingWorkshopUpgrade::ShiftDistance:
        return "ずらし距離強化";
    }
    return "";
}

int Game::ringWorkshopUpgradeLevel(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return workshopInitialRadiusLevel_;
    case RingWorkshopUpgrade::InitialSpeed:
        return workshopInitialSpeedLevel_;
    case RingWorkshopUpgrade::ShiftDistance:
        return workshopShiftDistanceLevel_;
    }
    return 0;
}

int Game::ringWorkshopUpgradeMaxLevel(RingWorkshopUpgrade) const
{
    return 5;
}

int Game::ringWorkshopUpgradeMoneyCost(RingWorkshopUpgrade upgrade) const
{
    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    return 120 + level * 90;
}

int Game::ringWorkshopUpgradeMoonCost(RingWorkshopUpgrade upgrade) const
{
    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    return level + 1;
}

float Game::ringWorkshopUpgradeCurrentValue(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return effectiveInitialRingRadius(levelRingRadiusPoints_);
    case RingWorkshopUpgrade::InitialSpeed:
        return effectiveInitialRingSpeed(levelRingSpeedPoints_);
    case RingWorkshopUpgrade::ShiftDistance:
        return effectiveRingShiftDistance();
    }
    return 0.0f;
}

float Game::ringWorkshopUpgradeNextValue(RingWorkshopUpgrade upgrade) const
{
    const int currentLevel = ringWorkshopUpgradeLevel(upgrade);
    if (currentLevel >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return ringWorkshopUpgradeCurrentValue(upgrade);
    }
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius: {
        const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringRadiusUpgradeLevel_) * 0.08f;
        const float workshopMultiplier = 1.0f + static_cast<float>(currentLevel + 1) * 0.05f;
        const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelRingRadiusPoints_)));
        return balance_.spellRingRadius * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
    }
    case RingWorkshopUpgrade::InitialSpeed: {
        const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringSpeedUpgradeLevel_) * 0.08f;
        const float workshopMultiplier = 1.0f + static_cast<float>(currentLevel + 1) * 0.05f;
        const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelRingSpeedPoints_)));
        return balance_.spellRingSpeed * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
    }
    case RingWorkshopUpgrade::ShiftDistance:
        return balance_.spellRingShiftDistance + static_cast<float>(currentLevel + 1) * 8.0f;
    }
    return 0.0f;
}

void Game::buyRingWorkshopUpgrade(RingWorkshopUpgrade upgrade)
{
    if (ringWorkshopUpgradeLevel(upgrade) >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        baseStatus_ = "この強化は上限です";
        return;
    }
    const int moneyCost = ringWorkshopUpgradeMoneyCost(upgrade);
    const int moonCost = ringWorkshopUpgradeMoonCost(upgrade);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        ++workshopInitialRadiusLevel_;
        break;
    case RingWorkshopUpgrade::InitialSpeed:
        ++workshopInitialSpeedLevel_;
        break;
    case RingWorkshopUpgrade::ShiftDistance:
        ++workshopShiftDistanceLevel_;
        break;
    }
    applyPermanentUpgrades();
    resetRingWorkshopDraft();
    baseStatus_ = "リング工房強化を行いました";
}

void Game::openBookshelf()
{
    baseBookshelfActive_ = true;
    bookshelfPage_ = BookshelfPage::Menu;
    bookshelfSelection_ = 0;
    baseStatus_.clear();
    syncEncyclopediaFromInventoryAndRing();
}

void Game::syncEncyclopediaFromInventoryAndRing()
{
    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        if (!stack.objectId.empty() && stack.count > 0) {
            encyclopedia_.noteItemObtained(stack.item, player_.position);
        }
    }
    for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
        if (!objectInstance.item.id.empty()) {
            encyclopedia_.noteItemObtained(objectInstance.item, player_.position);
        }
    }
    for (const SpellRingItem& item : spellRing_.items()) {
        if (item.objectId.empty()) {
            continue;
        }
        const ObjectDefinition* object = objectCatalog_.registry.findById(item.objectId);
        if (object != nullptr) {
            encyclopedia_.noteItemEquipped(*object, item.worldPosition);
        }
    }
}

void Game::applyEffectDiscoveries(const std::vector<EffectDiscoveryEvent>& discoveries)
{
    for (const EffectDiscoveryEvent& discovery : discoveries) {
        encyclopedia_.noteEffectEvent(discovery, objectCatalog_);
    }
}

void Game::addStoryFlag(std::string flag)
{
    if (flag.empty()) {
        return;
    }
    if (std::find(storyFlags_.begin(), storyFlags_.end(), flag) == storyFlags_.end()) {
        storyFlags_.push_back(std::move(flag));
    }
}

void Game::updateBookshelfScreen(const Input& input, UiContext& ui)
{
    const auto objectCountForPage = [this](BookshelfPage page) {
        int count = 0;
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((page == BookshelfPage::Treasures && treasure) ||
                (page == BookshelfPage::Items && !treasure)) {
                ++count;
            }
        }
        return count;
    };

    if (input.backPressed()) {
        if (bookshelfPage_ == BookshelfPage::Menu) {
            baseBookshelfActive_ = false;
            baseStatus_.clear();
        } else {
            bookshelfPage_ = BookshelfPage::Menu;
            bookshelfSelection_ = 0;
        }
        return;
    }

    const int itemCount = bookshelfPage_ == BookshelfPage::Menu
        ? BookshelfMenuItemCount
        : (bookshelfPage_ == BookshelfPage::Enemies ? static_cast<int>(enemyCatalog_.enemies.size()) : objectCountForPage(bookshelfPage_));
    if (itemCount <= 0) {
        bookshelfSelection_ = 0;
    } else {
        if (input.pressed(InputAction::MoveUp)) {
            bookshelfSelection_ = (bookshelfSelection_ + itemCount - 1) % itemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            bookshelfSelection_ = (bookshelfSelection_ + 1) % itemCount;
        }
        bookshelfSelection_ = std::clamp(bookshelfSelection_, 0, itemCount - 1);
    }

    const int visibleCount = std::min(10, itemCount);
    const int firstVisibleIndex = std::clamp(bookshelfSelection_ - visibleCount / 2, 0, std::max(0, itemCount - visibleCount));
    for (int i = 0; i < visibleCount; ++i) {
        if (ui.pressed(bookshelfItemRect(i))) {
            bookshelfSelection_ = firstVisibleIndex + i;
            if (bookshelfPage_ == BookshelfPage::Menu) {
                bookshelfPage_ = static_cast<BookshelfPage>(bookshelfSelection_ + 1);
                bookshelfSelection_ = 0;
            }
            return;
        }
    }

    if ((input.confirmPressed() || input.useItemPressed()) && bookshelfPage_ == BookshelfPage::Menu) {
        bookshelfPage_ = static_cast<BookshelfPage>(bookshelfSelection_ + 1);
        bookshelfSelection_ = 0;
        return;
    }

    ui.block(basePanelRect());
}

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

    auto& ringItems = spellRing_.items();
    for (SpellRingItem& item : ringItems) {
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

    for (SpellRingItem& item : spellRing_.items()) {
        if (item.broken()) {
            continue;
        }

        const std::string* behaviorId = capturedProjectileBehaviorId(item);
        if (behaviorId == nullptr) {
            item.capturedProjectileTimer = 0.0f;
            continue;
        }

        const auto behaviorIt = enemyCatalog_.behaviorsById.find(*behaviorId);
        const BehaviorDefinition* behavior = behaviorIt != enemyCatalog_.behaviorsById.end() ? &behaviorIt->second : nullptr;
        std::string_view projectileId = fallbackCapturedProjectileId(*behaviorId);
        if (behavior != nullptr && !behavior->defaultProjectileId.empty() && behavior->defaultProjectileId != "none") {
            projectileId = behavior->defaultProjectileId;
        }

        const float intervalFloor = *behaviorId == "radial_spike" ? CapturedRadialSpikeMinInterval : CapturedProjectileMinInterval;
        const float configuredInterval = behavior != nullptr && behavior->defaultIntervalSeconds > 0.0 && std::isfinite(behavior->defaultIntervalSeconds)
            ? static_cast<float>(behavior->defaultIntervalSeconds)
            : intervalFloor;
        const float interval = std::max(intervalFloor, configuredInterval);

        item.capturedProjectileTimer = std::max(0.0f, item.capturedProjectileTimer - dt);
        if (item.capturedProjectileTimer > 0.0f) {
            continue;
        }

        const int activePlayerProjectiles = projectiles_.activeCount(ProjectileOwnerType::PlayerOrbit);
        const int requestedProjectiles = *behaviorId == "radial_spike" ? RadialSpikeProjectileCount : 1;
        if (activePlayerProjectiles + requestedProjectiles > MaxPlayerOrbitProjectiles) {
            item.capturedProjectileTimer = CapturedProjectileRetryInterval;
            continue;
        }

        const std::vector<EffectSpec> effects = capturedProjectileEffects(*behaviorId, behavior);
        const Vec2 outward = normalize(item.worldPosition - spellRing_.center());
        bool fired = false;
        if (*behaviorId == "radial_spike") {
            for (int i = 0; i < RadialSpikeProjectileCount; ++i) {
                const float angle = Pi * 2.0f * static_cast<float>(i) / static_cast<float>(RadialSpikeProjectileCount);
                const Vec2 direction = fromAngle(angle);
                const Vec2 origin = item.worldPosition + direction * (item.hitRadius + 5.0f);
                fired = projectiles_.spawn(projectileId, origin, direction, ProjectileOwnerType::PlayerOrbit, effects) || fired;
            }
        } else {
            const Vec2 origin = item.worldPosition + outward * (item.hitRadius + 5.0f);
            fired = projectiles_.spawn(projectileId, origin, outward, ProjectileOwnerType::PlayerOrbit, effects);
        }

        item.capturedProjectileTimer = fired ? interval : CapturedProjectileRetryInterval;
    }
}

void Game::updateCapturedUtilityBehaviors(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    for (SpellRingItem& item : spellRing_.items()) {
        item.capturedExplodeSleepTimer = std::max(0.0f, item.capturedExplodeSleepTimer - dt);
        item.capturedMagnetVisualTimer = std::max(0.0f, item.capturedMagnetVisualTimer - dt);

        if (item.broken()) {
            continue;
        }

        if (item.hasCapturedBehavior("magnet_pull")) {
            const int pulledDrops = worldDrops_.pullMetalDrops(objectCatalog_, item.worldPosition, dt);
            const int pulledEnemies = enemies_.pullMetalEnemies(item.worldPosition, tileMap_, dt);
            const int pulledProjectiles = projectiles_.pullMetalProjectiles(item.worldPosition, dt);
            if (pulledDrops + pulledEnemies + pulledProjectiles > 0 && item.capturedMagnetVisualTimer <= 0.0f) {
                effects_.spawnAreaPulse(item.worldPosition, 42.0f, {120, 190, 245, 150});
                item.capturedMagnetVisualTimer = CapturedMagnetVisualInterval;
            }
        }

        if (item.hasCapturedBehavior("wind_deflect")) {
            item.capturedWindTimer = std::max(0.0f, item.capturedWindTimer - dt);
            if (item.capturedWindTimer <= 0.0f) {
                const int deflected = projectiles_.deflectEnemyProjectiles(item.worldPosition, 1.0f);
                if (deflected > 0) {
                    effects_.spawnAreaPulse(item.worldPosition, 66.0f, {150, 235, 205, 155});
                }
                item.capturedWindTimer = CapturedWindInterval;
            }
        } else {
            item.capturedWindTimer = 0.0f;
        }
    }
}

void Game::handleCapturedExplosion(Vec2 position)
{
    effects_.spawnAreaPulse(position, 50.0f, {255, 128, 54, 190});
    const std::vector<Vec2> openedTiles = tileMap_.damageCircle(position, CapturedExplosionTileRadius, CapturedExplosionTileDamage);
    for (Vec2 tile : openedTiles) {
        effects_.spawnTileBreak(tile);
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
    ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, RingSlotCount - 1);
    cancelRingGrab();
    ringStatus_.clear();
}

void Game::cancelRingGrab()
{
    if (!ringGrabActive_) {
        return;
    }

    auto& items = spellRing_.items();
    const int insertIndex = std::clamp(ringGrabOrigin_, 0, static_cast<int>(items.size()));
    items.insert(items.begin() + insertIndex, ringGrabbedItem_);
    ringGrabActive_ = false;
    ringGrabOrigin_ = -1;
}

Game::InventoryCarryState Game::captureInventoryCarryState() const
{
    InventoryCarryState state;
    state.inventory = inventory_;
    state.ringItems = spellRing_.items();
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
    if (!state.ringItems.empty()) {
        spellRing_.items() = state.ringItems;
        spellRing_.applyObjectParameters(objectCatalog_);
        spellRing_.resetBaseWeightToCurrent();
        refreshOrbitEffects();
    }
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

    initializeWorld(false);
    restoreInventoryCarryState(retained);
    player_.level = retainedLevel;
    player_.xp = retainedXp;
    player_.xpToNext = retainedXpToNext;
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    captureRunStartInventoryState();
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
    const bool refreshMerchant = shouldRefreshMerchantOnReturn(stageCleared, died);
    merchantRefreshPending_ = merchantRefreshPending_ || refreshMerchant;
    clearTemporaryPlayerState(true);
    enemies_ = EnemySystem{};
    effects_ = EffectSystem{};
    projectiles_ = ProjectileSystem{};
    worldDrops_ = WorldDropSystem{};
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

void Game::updateWarpPoints()
{
    if (!warpPointsEnabled_) {
        return;
    }

    for (WarpPoint& point : warpPoints_) {
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
            reloadNotice_ = "ワープポイント発見";
            reloadNoticeTimer_ = 2.0f;
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

        if (node.objectId.has_value()) {
            if (!inventory_.addObjectItem(objectCatalog_, *node.objectId)) {
                continue;
            }
        }
        node.spawned = true;
        node.collected = true;
        ++runStats_.acquiredItems;
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
        money_ += std::max(0, node.amount);
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
                spawnedObject = worldDrops_.spawnObjectDrop(objectCatalog_, *node.objectId, tileWorldCenter(node.tile));
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
            money_ += std::max(0, node.amount);
            node.collected = true;
            reloadNotice_ = "埋まり金貨 +" + std::to_string(std::max(0, node.amount)) + "G";
            reloadNoticeTimer_ = 1.6f;
        }
    }
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
            if (enemies_.spawnNodeEnemy(tileMap_, tileWorldCenter(node.tile), player_.position, balance_, enemyCatalog_, true)) {
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
    retrySnapshot_.enemyNodes = enemyNodes_;
    retrySnapshot_.spawnedWarpPointCount = spawnedWarpPointCount_;
    retrySnapshot_.unlockedWarpPointCount = unlockedWarpPointCount_;
    retrySnapshot_.bossSpawnPoint = bossSpawnPoint_;
    retrySnapshot_.hasBossSpawnPoint = hasBossSpawnPoint_;
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
    enemyNodes_ = retrySnapshot_.enemyNodes;
    spawnedWarpPointCount_ = retrySnapshot_.spawnedWarpPointCount;
    unlockedWarpPointCount_ = retrySnapshot_.unlockedWarpPointCount;
    bossSpawnPoint_ = retrySnapshot_.bossSpawnPoint;
    hasBossSpawnPoint_ = retrySnapshot_.hasBossSpawnPoint;
    enemies_ = EnemySystem{};
    effects_ = EffectSystem{};
    worldDrops_ = WorldDropSystem{};
    levels_ = LevelSystem{};
    bossSpawned_ = false;
    inventoryReturnToPause_ = false;
    gameOverStatus_.clear();
    tileMap_.updateAround(player_.position, 0.0f, balance_, dungeonLayout_);
    camera_.follow(player_.position, 1.0f);
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
    std::vector<SpellRingItem> loadedRingItems;
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
    int loadedLevelRingRadiusPoints = 0;
    int loadedLevelRingSpeedPoints = 0;
    int loadedWorkshopInitialRadiusLevel = 0;
    int loadedWorkshopInitialSpeedLevel = 0;
    int loadedWorkshopShiftDistanceLevel = 0;
    bool loadedMerchantRefreshPending = false;
    int loadedMerchantUpgradeLevel = 1;
    int loadedMerchantStockVersion = 0;
    std::vector<MerchantProduct> loadedMerchantStock;
    std::string loadedHighValueBuyCategory;
    int loadedWarehouseCapacityLevel = 0;
    int loadedProcessingUnlockLevel = 0;
    bool loadedRingWorkshopUnlocked = false;
    bool loadedAutoSaveOnReturn = false;
    std::vector<std::string> loadedStoryFlags;
    int warningCount = 0;

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
        } else if (key == "level_ring_radius_points") {
            stream >> loadedLevelRingRadiusPoints;
        } else if (key == "level_ring_speed_points") {
            stream >> loadedLevelRingSpeedPoints;
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
            stream >> objectId >> price;
            if (!stream.fail()) {
                if (objectCatalog_.registry.findById(objectId) == nullptr) {
                    ++warningCount;
                    logError("[warning] SaveData: merchant_stock object_id=\"" + objectId + "\" is missing from Objects DB; keeping ID");
                }
                loadedMerchantStock.push_back(MerchantProduct{objectId, std::max(1, price)});
            }
        } else if (key == "high_value_buy_category") {
            stream >> loadedHighValueBuyCategory;
            if (loadedHighValueBuyCategory == "-") {
                loadedHighValueBuyCategory.clear();
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
        } else if (key == "inventory") {
            std::string type;
            int count = 0;
            stream >> type >> count;
            if (type == "dirt") {
                loadedInventory.setItemCount(InventoryItemType::Dirt, count);
            } else if (type == "stone") {
                loadedInventory.setItemCount(InventoryItemType::Stone, count);
            } else if (type == "ore") {
                loadedInventory.setItemCount(InventoryItemType::Ore, count);
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
        } else if (key == "ring") {
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
                loadedRingItems.push_back(item);
            }
        }
    }

    inventory_ = loadedInventory;
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    if (!loadedRingItems.empty()) {
        spellRing_.items() = std::move(loadedRingItems);
        spellRing_.applyObjectParameters(objectCatalog_);
        spellRing_.resetBaseWeightToCurrent();
        refreshOrbitEffects();
    }
    money_ = std::max(0, loadedMoney);
    unlockedStages_ = std::max(1, loadedUnlockedStages);
    unlockedWarpPointCount_ = std::max(0, loadedUnlockedWarpPointCount);
    hasLatestWarpPointPosition_ = loadedHasLatestWarpPointPosition;
    latestWarpPointPosition_ = loadedHasLatestWarpPointPosition
        ? loadedLatestWarpPointPosition
        : latestWarpPointStartPosition();
    currentStage_ = std::clamp(loadedCurrentStage, 0, unlockedStages_ - 1);
    if (!loadedCurrentStageId.empty()) {
        currentStageId_ = loadedCurrentStageId;
    }
    resolveCurrentStageDefinition();
    player_.level = std::max(1, loadedPlayerLevel);
    player_.xp = std::max(0, loadedPlayerXp);
    player_.xpToNext = std::max(1, loadedPlayerXpToNext);
    maxHpUpgradeLevel_ = std::max(0, loadedMaxHpUpgradeLevel);
    ringRadiusUpgradeLevel_ = std::max(0, loadedRingRadiusUpgradeLevel);
    ringSpeedUpgradeLevel_ = std::max(0, loadedRingSpeedUpgradeLevel);
    levelRingRadiusPoints_ = std::max(0, loadedLevelRingRadiusPoints);
    levelRingSpeedPoints_ = std::max(0, loadedLevelRingSpeedPoints);
    workshopInitialRadiusLevel_ = std::clamp(loadedWorkshopInitialRadiusLevel, 0, 5);
    workshopInitialSpeedLevel_ = std::clamp(loadedWorkshopInitialSpeedLevel, 0, 5);
    workshopShiftDistanceLevel_ = std::clamp(loadedWorkshopShiftDistanceLevel, 0, 5);
    merchantRefreshPending_ = loadedMerchantRefreshPending;
    merchantUpgradeLevel_ = std::clamp(loadedMerchantUpgradeLevel, 1, 7);
    merchantStockVersion_ = std::max(0, loadedMerchantStockVersion);
    merchantStock_ = std::move(loadedMerchantStock);
    highValueBuyCategory_ = std::move(loadedHighValueBuyCategory);
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
    file << "level_ring_radius_points " << levelRingRadiusPoints_ << "\n";
    file << "level_ring_speed_points " << levelRingSpeedPoints_ << "\n";
    file << "workshop_initial_radius_level " << workshopInitialRadiusLevel_ << "\n";
    file << "workshop_initial_speed_level " << workshopInitialSpeedLevel_ << "\n";
    file << "workshop_shift_distance_level " << workshopShiftDistanceLevel_ << "\n";
    file << "merchant_refresh_pending " << merchantRefreshPending_ << "\n";
    file << "merchant_upgrade_level " << merchantUpgradeLevel_ << "\n";
    file << "merchant_stock_version " << merchantStockVersion_ << "\n";
    file << "high_value_buy_category " << (highValueBuyCategory_.empty() ? "-" : highValueBuyCategory_) << "\n";
    for (const MerchantProduct& product : merchantStock_) {
        if (!product.objectId.empty()) {
            file << "merchant_stock " << product.objectId << " " << product.price << "\n";
        }
    }
    file << "warehouse_capacity_level " << warehouseCapacityLevel_ << "\n";
    file << "processing_unlock_level " << processingUnlockLevel_ << "\n";
    file << "ring_workshop_unlocked " << ringWorkshopUnlocked_ << "\n";
    file << "auto_save_on_return " << autoSaveOnReturn_ << "\n";
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
    file << "inventory dirt " << inventory_.itemCount(InventoryItemType::Dirt) << "\n";
    file << "inventory stone " << inventory_.itemCount(InventoryItemType::Stone) << "\n";
    file << "inventory ore " << inventory_.itemCount(InventoryItemType::Ore) << "\n";
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
    for (const SpellRingItem& item : spellRing_.items()) {
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
            << item.isBroken << "\n";
    }

    if (!file) {
        message = "セーブ書込に失敗";
        return false;
    }

    message = "セーブしました";
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

void Game::updateRingScreen(const Input& input, UiContext& ui)
{
    for (int i = 0; i < RingCount; ++i) {
        if (ui.pressed(ringTabRect(i))) {
            spellRing_.switchActiveRing(i - spellRing_.activeRingIndex());
            ringStatus_.clear();
            return;
        }
    }

    for (int i = 0; i < RingSlotCount; ++i) {
        if (ui.pressed(ringSlotRect(i))) {
            ringSlotSelection_ = i;
            return;
        }
    }
    ui.block(ringPanelRect());

    const int directRing = input.shortcutSlotPressed();
    if (directRing >= 0 && directRing < RingCount) {
        spellRing_.switchActiveRing(directRing - spellRing_.activeRingIndex());
        ringStatus_.clear();
    }
    if (input.activeRingDelta() != 0) {
        spellRing_.switchActiveRing(input.activeRingDelta());
        ringStatus_.clear();
    }

    int moveX = 0;
    int moveY = 0;
    if (input.pressed(InputAction::MoveLeft) || input.shortcutCursorDelta() < 0) {
        --moveX;
    }
    if (input.pressed(InputAction::MoveRight) || input.shortcutCursorDelta() > 0) {
        ++moveX;
    }
    if (input.pressed(InputAction::MoveUp)) {
        --moveY;
    }
    if (input.pressed(InputAction::MoveDown)) {
        ++moveY;
    }
    if (moveX != 0) {
        ringSlotSelection_ = (ringSlotSelection_ + moveX + RingSlotCount) % RingSlotCount;
    }
    if (moveY != 0) {
        ringSlotSelection_ = (ringSlotSelection_ + moveY * RingSlotColumns + RingSlotCount) % RingSlotCount;
    }

    if (input.pausePressed()) {
        if (ringGrabActive_) {
            cancelRingGrab();
            ringStatus_ = "つかみ操作をキャンセルしました";
        } else {
            mode_ = ScreenMode::PauseMenu;
            pausePage_ = PauseMenuPage::Main;
        }
        return;
    }

    const bool actualRing = spellRing_.activeRingIndex() == 0;
    auto& items = spellRing_.items();
    if (!actualRing) {
        if (input.useItemPressed() || input.confirmPressed() || input.addRingPressed() || input.grabOrPlacePressed()) {
            ringStatus_ = "このリングは未実装です";
        }
        return;
    }

    if (input.addRingPressed()) {
        if (ringGrabActive_) {
            ringStatus_ = "つかみ中は外せません";
        } else if (ringSlotSelection_ < static_cast<int>(items.size()) && items.size() > 1) {
            items.erase(items.begin() + ringSlotSelection_);
            ringSlotSelection_ = std::min(ringSlotSelection_, RingSlotCount - 1);
            ringStatus_ = "選択中スロットのアイテムを外しました";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ringStatus_ = "最後の1個は外せません";
        } else {
            ringStatus_ = "空きスロットです";
        }
        return;
    }

    if (input.pressed(InputAction::ToggleProtection)) {
        if (ringGrabActive_) {
            ringStatus_ = "つかみ中は保護変更できません";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            SpellRingItem& item = items[ringSlotSelection_];
            if (item.instanceId.empty()) {
                ringStatus_ = "個体アイテムのみ保護できます";
            } else {
                item.protectionEnabled = !item.protectionEnabled;
                ringStatus_ = item.protectionEnabled ? "保護ON" : "保護OFF";
            }
        } else {
            ringStatus_ = "空きスロットです";
        }
        return;
    }

    if (input.grabOrPlacePressed()) {
        if (ringGrabActive_) {
            const int insertIndex = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()));
            items.insert(items.begin() + insertIndex, ringGrabbedItem_);
            ringGrabActive_ = false;
            ringGrabOrigin_ = -1;
            ringStatus_ = "装着順を変更しました";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            if (items.size() <= 1) {
                ringStatus_ = "最後の1個は外せません";
            } else {
                ringGrabbedItem_ = items[ringSlotSelection_];
                ringGrabOrigin_ = ringSlotSelection_;
                items.erase(items.begin() + ringSlotSelection_);
                ringGrabActive_ = true;
                ringStatus_ = "装着アイテムをつかみました";
            }
        } else {
            ringStatus_ = "空きスロットです";
        }
        return;
    }

    if (input.useItemPressed() || input.confirmPressed()) {
        if (ringGrabActive_) {
            const int insertIndex = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()));
            items.insert(items.begin() + insertIndex, ringGrabbedItem_);
            ringGrabActive_ = false;
            ringGrabOrigin_ = -1;
            ringStatus_ = "装着順を変更しました";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ringStatus_ = "詳細を表示中";
        } else {
            ringStatus_ = "空きスロットです";
        }
    }
}

void Game::updateBaseScreen(const Input& input, UiContext& ui, float dt)
{
    if (baseBookshelfActive_) {
        updateBookshelfScreen(input, ui);
        return;
    }

    if (baseRingWorkshopActive_) {
        if (input.backPressed()) {
            baseRingWorkshopActive_ = false;
            resetRingWorkshopDraft();
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + BaseRingWorkshopItemCount - 1) % BaseRingWorkshopItemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + 1) % BaseRingWorkshopItemCount;
        }
        if (input.pressed(InputAction::MoveLeft)) {
            adjustRingWorkshopRespec(-1);
        }
        if (input.pressed(InputAction::MoveRight)) {
            adjustRingWorkshopRespec(1);
        }
        const auto chooseWorkshopItem = [this](int item) {
            if (item == 0) {
                adjustRingWorkshopRespec(1);
                return;
            }
            if (item == 1) {
                adjustRingWorkshopRespec(-1);
                return;
            }
            if (item == 2) {
                confirmRingWorkshopRespec();
                return;
            }
            if (item >= 3 && item < 3 + RingWorkshopImplementedUpgradeCount) {
                buyRingWorkshopUpgrade(static_cast<RingWorkshopUpgrade>(item - 3));
                return;
            }
            baseStatus_ = "この項目は未解禁です";
        };
        for (int i = 0; i < BaseRingWorkshopItemCount; ++i) {
            if (ui.pressed(baseRingWorkshopItemRect(i))) {
                baseRingWorkshopSelection_ = i;
                chooseWorkshopItem(i);
                return;
            }
        }
        if (ui.pressed(ringWorkshopConfirmRect())) {
            confirmRingWorkshopRespec();
            return;
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            chooseWorkshopItem(baseRingWorkshopSelection_);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (baseStorageActive_) {
        const std::vector<StorageEntry> backpackEntries = backpackStorageEntries();
        const std::vector<StorageEntry> warehouseEntries = warehouseStorageEntries();
        if (input.backPressed()) {
            baseStorageActive_ = false;
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveRight)) {
            baseStorageWarehousePane_ = !baseStorageWarehousePane_;
        }

        int& activeSelection = baseStorageWarehousePane_ ? baseStorageWarehouseSelection_ : baseStorageBackpackSelection_;
        const int activeCount = baseStorageWarehousePane_ ? static_cast<int>(warehouseEntries.size()) : static_cast<int>(backpackEntries.size());
        if (activeCount <= 0) {
            activeSelection = 0;
        } else {
            activeSelection = std::clamp(activeSelection, 0, activeCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                activeSelection = (activeSelection + activeCount - 1) % activeCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                activeSelection = (activeSelection + 1) % activeCount;
            }
        }

        const auto visibleOffset = [](int selection, int count) {
            if (count <= WarehouseVisibleRows) {
                return 0;
            }
            return std::clamp(selection - WarehouseVisibleRows / 2, 0, count - WarehouseVisibleRows);
        };
        const int backpackOffset = visibleOffset(baseStorageBackpackSelection_, static_cast<int>(backpackEntries.size()));
        const int warehouseOffset = visibleOffset(baseStorageWarehouseSelection_, static_cast<int>(warehouseEntries.size()));
        const int backpackVisible = std::min(WarehouseVisibleRows, static_cast<int>(backpackEntries.size()) - backpackOffset);
        const int warehouseVisible = std::min(WarehouseVisibleRows, static_cast<int>(warehouseEntries.size()) - warehouseOffset);
        for (int i = 0; i < backpackVisible; ++i) {
            if (ui.pressed(storageBackpackItemRect(i))) {
                baseStorageWarehousePane_ = false;
                baseStorageBackpackSelection_ = backpackOffset + i;
                depositBackpackEntry(baseStorageBackpackSelection_);
                return;
            }
        }
        for (int i = 0; i < warehouseVisible; ++i) {
            if (ui.pressed(storageWarehouseItemRect(i))) {
                baseStorageWarehousePane_ = true;
                baseStorageWarehouseSelection_ = warehouseOffset + i;
                withdrawWarehouseEntry(baseStorageWarehouseSelection_);
                return;
            }
        }

        if (input.confirmPressed() || input.useItemPressed()) {
            if (baseStorageWarehousePane_) {
                withdrawWarehouseEntry(baseStorageWarehouseSelection_);
            } else {
                depositBackpackEntry(baseStorageBackpackSelection_);
            }
            return;
        }
        ui.block(storagePanelRect());
        return;
    }

    if (baseProcessingActive_) {
        const std::vector<StorageEntry> entries = processingEntries();
        if (input.backPressed()) {
            baseProcessingActive_ = false;
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveLeft)) {
            baseProcessingMode_ = (baseProcessingMode_ + BaseProcessingModeCount - 1) % BaseProcessingModeCount;
        }
        if (input.pressed(InputAction::MoveRight)) {
            baseProcessingMode_ = (baseProcessingMode_ + 1) % BaseProcessingModeCount;
        }
        if (entries.empty()) {
            baseProcessingSelection_ = 0;
        } else {
            baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, static_cast<int>(entries.size()) - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseProcessingSelection_ = (baseProcessingSelection_ + static_cast<int>(entries.size()) - 1) % static_cast<int>(entries.size());
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseProcessingSelection_ = (baseProcessingSelection_ + 1) % static_cast<int>(entries.size());
            }
        }
        for (int i = 0; i < BaseProcessingModeCount; ++i) {
            if (ui.pressed(baseProcessingModeRect(i))) {
                baseProcessingMode_ = i;
                return;
            }
        }
        const int visibleCount = std::min(5, static_cast<int>(entries.size()));
        for (int i = 0; i < visibleCount; ++i) {
            if (ui.pressed(baseProcessingItemRect(i))) {
                baseProcessingSelection_ = i;
                applyProcessing(i);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            applyProcessing(baseProcessingSelection_);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (baseSellActive_) {
        const std::vector<SellableEntry> sellable = sellableObjects();
        refreshMerchantStock(false);
        if (input.backPressed()) {
            baseSellActive_ = false;
            baseStatus_.clear();
            return;
        }

        if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveRight)) {
            baseMerchantBuyPane_ = !baseMerchantBuyPane_;
        }

        int& activeSelection = baseMerchantBuyPane_ ? baseMerchantBuySelection_ : baseSellSelection_;
        const int activeCount = baseMerchantBuyPane_ ? static_cast<int>(merchantStock_.size()) : static_cast<int>(sellable.size());
        if (activeCount <= 0) {
            activeSelection = 0;
        } else {
            activeSelection = std::clamp(activeSelection, 0, activeCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                activeSelection = (activeSelection + activeCount - 1) % activeCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                activeSelection = (activeSelection + 1) % activeCount;
            }
        }

        const int sellVisible = std::min(5, static_cast<int>(sellable.size()));
        for (int i = 0; i < sellVisible; ++i) {
            if (ui.pressed(merchantSellItemRect(i))) {
                baseMerchantBuyPane_ = false;
                baseSellSelection_ = i;
                const SellableEntry entry = sellable[static_cast<std::size_t>(i)];
                if (!entry.sellable) {
                    baseStatus_ = entry.blockedReason.empty() ? "売却不可" : entry.blockedReason;
                    return;
                }
                bool sold = false;
                if (entry.kind == SellableKind::Stack) {
                    const InventoryObjectStack& stack = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
                    sold = inventory_.removeObjectItemCount(stack.objectId, 1);
                } else {
                    const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
                    sold = inventory_.removeObjectInstance(instance.instance.instanceId);
                }
                if (sold) {
                    money_ += entry.price;
                    baseStatus_ = "売却しました";
                }
                return;
            }
        }
        const int buyVisible = std::min(5, static_cast<int>(merchantStock_.size()));
        for (int i = 0; i < buyVisible; ++i) {
            if (ui.pressed(merchantBuyItemRect(i))) {
                baseMerchantBuyPane_ = true;
                baseMerchantBuySelection_ = i;
                buyMerchantProduct(i);
                return;
            }
        }

        if (input.confirmPressed() || input.useItemPressed()) {
            if (baseMerchantBuyPane_) {
                buyMerchantProduct(baseMerchantBuySelection_);
                return;
            }
            if (sellable.empty()) {
                baseStatus_ = "売却対象がありません";
                return;
            }
            const SellableEntry entry = sellable[static_cast<std::size_t>(baseSellSelection_)];
            if (!entry.sellable) {
                baseStatus_ = entry.blockedReason.empty() ? "売却不可" : entry.blockedReason;
                return;
            }
            bool sold = false;
            if (entry.kind == SellableKind::Stack) {
                const InventoryObjectStack& stack = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
                sold = inventory_.removeObjectItemCount(stack.objectId, 1);
            } else {
                const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
                sold = inventory_.removeObjectInstance(instance.instance.instanceId);
            }
            if (sold) {
                money_ += entry.price;
                baseStatus_ = "売却しました";
            }
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (baseUpgradeActive_) {
        if (input.backPressed()) {
            baseUpgradeActive_ = false;
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + BaseUpgradeItemCount - 1) % BaseUpgradeItemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + 1) % BaseUpgradeItemCount;
        }
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            if (ui.pressed(baseUpgradeItemRect(i))) {
                baseUpgradeSelection_ = i;
                buyUpgrade(i);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            buyUpgrade(baseUpgradeSelection_);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (baseMiningStartChoiceActive_) {
        if (input.backPressed()) {
            baseMiningStartChoiceActive_ = false;
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + BaseMiningStartChoiceCount - 1) % BaseMiningStartChoiceCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + 1) % BaseMiningStartChoiceCount;
        }
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            if (ui.pressed(baseMiningStartChoiceRect(i))) {
                baseMiningStartSelection_ = i;
                if (i == 1 && unlockedWarpPointCount_ <= 0) {
                    baseStatus_ = "解放済みワープポイントがありません";
                    return;
                }
                startMiningFromBase(i == 1);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            if (baseMiningStartSelection_ == 1 && unlockedWarpPointCount_ <= 0) {
                baseStatus_ = "解放済みワープポイントがありません";
                return;
            }
            startMiningFromBase(baseMiningStartSelection_ == 1);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (input.pausePressed()) {
        mode_ = ScreenMode::PauseMenu;
        pauseReturnMode_ = ScreenMode::Base;
        pausePage_ = PauseMenuPage::Main;
        return;
    }

    const auto interact = [this](const BaseFacility& facility) {
        if (!facility.enabled) {
            return;
        }
        switch (facility.onInteract) {
        case BaseFacilityAction::MineExit:
            baseMiningStartChoiceActive_ = true;
            baseMiningStartSelection_ = unlockedWarpPointCount_ > 0 ? 1 : 0;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Storage:
            baseStorageActive_ = true;
            baseStorageWarehousePane_ = false;
            baseStorageBackpackSelection_ = 0;
            baseStorageWarehouseSelection_ = 0;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Merchant:
            if (merchantRefreshPending_) {
                refreshMerchantStock(true);
                merchantRefreshPending_ = false;
            } else {
                refreshMerchantStock(false);
            }
            baseSellActive_ = true;
            baseMerchantBuyPane_ = false;
            baseSellSelection_ = 0;
            baseMerchantBuySelection_ = 0;
            baseStatus_ = "商人ワゴン";
            break;
        case BaseFacilityAction::Forge:
            baseUpgradeActive_ = true;
            baseUpgradeSelection_ = 0;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Processing:
            baseProcessingActive_ = true;
            baseProcessingMode_ = 0;
            baseProcessingSelection_ = 0;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Bookshelf:
            openBookshelf();
            break;
        case BaseFacilityAction::Diary: {
            std::string message;
            saveSaveData(message);
            baseStatus_ = message;
            break;
        }
        case BaseFacilityAction::RingWorkshop:
            if (facility.unlocked) {
                openRingWorkshop();
            } else {
                baseStatus_ = "リング工房用スペース: まだ解禁されていません";
            }
            break;
        case BaseFacilityAction::Observatory:
            baseStatus_ = "モニカの星見台: 星の流れを観測中です";
            break;
        }
    };

    const std::array<BaseFacility, 10> facilities = baseFacilities(ringWorkshopUnlocked_);
    const float playerRadius = balance_.playerRadius;
    const auto baseCollision = [&](Vec2 position) {
        const UiRect bounds = baseMapBounds();
        if (position.x - playerRadius < bounds.pos.x ||
            position.y - playerRadius < bounds.pos.y ||
            position.x + playerRadius > bounds.pos.x + bounds.size.x ||
            position.y + playerRadius > bounds.pos.y + bounds.size.y) {
            return true;
        }
        for (const BaseFacility& facility : facilities) {
            if (circleIntersectsRect(position, playerRadius, facility.rect)) {
                return true;
            }
        }
        return false;
    };

    const Vec2 moveAxis = input.moveAxis();
    if (lengthSquared(moveAxis) > 0.0001f) {
        basePlayerFacing_ = normalize(moveAxis);
        const Vec2 delta = moveAxis * balance_.playerSpeed * dt;
        Vec2 next = basePlayerPosition_ + Vec2{delta.x, 0.0f};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
        next = basePlayerPosition_ + Vec2{0.0f, delta.y};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
    }

    const BaseFacility* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const BaseFacility& facility : facilities) {
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }

    if (input.mouseLeftPressed() && !ui.pointerConsumed()) {
        for (const BaseFacility& facility : facilities) {
            if (!facility.rect.contains(ui.mouse())) {
                continue;
            }
            ui.consumePointer();
            if (baseInteractionAvailable(basePlayerPosition_, facility)) {
                interact(facility);
            } else if (facility.enabled) {
                baseStatus_ = "近くまで移動してください";
            }
            return;
        }
    }

    if (input.confirmPressed() && nearest != nullptr) {
        interact(*nearest);
        return;
    }
}

void Game::updatePauseMenu(const Input& input, UiContext& ui)
{
    if (input.backPressed()) {
        leavePausePage();
        return;
    }

    if (pausePage_ == PauseMenuPage::QuitConfirm) {
        if (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveDown)) {
            pauseConfirmSelection_ = 1 - pauseConfirmSelection_;
        }
        for (int i = 0; i < 2; ++i) {
            if (ui.pressed(quitConfirmButtonRect(i))) {
                pauseConfirmSelection_ = i;
                if (i == 0) {
                    pausePage_ = PauseMenuPage::Main;
                } else {
                    quitRequested_ = true;
                }
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            if (pauseConfirmSelection_ == 0) {
                pausePage_ = PauseMenuPage::Main;
            } else {
                quitRequested_ = true;
            }
            return;
        }
        ui.block(quitConfirmRect());
        return;
    }

    if (pausePage_ != PauseMenuPage::Main) {
        if (ui.pressed(pauseBackButtonRect())) {
            pausePage_ = PauseMenuPage::Main;
            return;
        }
        ui.block(pausePanelRect());
        return;
    }

    if (input.pressed(InputAction::MoveUp)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + PauseMenuItemCount - 1) % PauseMenuItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + 1) % PauseMenuItemCount;
    }
    for (int i = 0; i < PauseMenuItemCount; ++i) {
        if (ui.pressed(pauseMenuItemRect(i))) {
            pauseMenuSelection_ = i;
            choosePauseMenuItem(i);
            return;
        }
    }
    if (input.confirmPressed() || input.useItemPressed()) {
        choosePauseMenuItem(pauseMenuSelection_);
        return;
    }

    ui.block(pausePanelRect());
}

void Game::updateGameOverScreen(const Input& input, UiContext& ui)
{
    if (input.pressed(InputAction::MoveUp)) {
        gameOverSelection_ = (gameOverSelection_ + GameOverItemCount - 1) % GameOverItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        gameOverSelection_ = (gameOverSelection_ + 1) % GameOverItemCount;
    }

    for (int i = 0; i < GameOverItemCount; ++i) {
        if (ui.pressed(gameOverItemRect(i))) {
            gameOverSelection_ = i;
            if (i == 0) {
                retryAfterGameOver();
            } else {
                returnToBaseAfterGameOver();
            }
            return;
        }
    }

    if (input.confirmPressed() || input.useItemPressed()) {
        if (gameOverSelection_ == 0) {
            retryAfterGameOver();
        } else {
            returnToBaseAfterGameOver();
        }
        return;
    }

    ui.block(gameOverPanelRect());
}

void Game::updateStageClearScreen(const Input& input, UiContext& ui)
{
    if (input.pressed(InputAction::MoveUp)) {
        stageClearSelection_ = (stageClearSelection_ + StageClearItemCount - 1) % StageClearItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        stageClearSelection_ = (stageClearSelection_ + 1) % StageClearItemCount;
    }

    for (int i = 0; i < StageClearItemCount; ++i) {
        if (ui.pressed(stageClearItemRect(i))) {
            stageClearSelection_ = i;
            returnToBaseFromNormalStage(true, false);
            return;
        }
    }

    if (input.confirmPressed() || input.useItemPressed()) {
        returnToBaseFromNormalStage(true, false);
        return;
    }

    ui.block(stageClearPanelRect());
}

void Game::updateScreenMode(const Input& input, UiContext& ui, float dt)
{
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
    case ScreenMode::Base:
        updateBaseScreen(input, ui, dt);
        break;
    case ScreenMode::Playing:
        if (input.pausePressed()) {
            mode_ = ScreenMode::PauseMenu;
            pauseReturnMode_ = ScreenMode::Playing;
            pausePage_ = PauseMenuPage::Main;
            return;
        }
        inventory_.update(input, ui, player_, spellRing_, effectDispatcher_, false);
        if (inventory_.isOpen()) {
            inventoryReturnToPause_ = false;
            mode_ = ScreenMode::Inventory;
            return;
        }
        if (input.activeRingDelta() != 0) {
            spellRing_.switchActiveRing(input.activeRingDelta());
        }
        inventory_.updateShortcuts(input, player_, spellRing_, effectDispatcher_);
        break;
    case ScreenMode::PauseMenu:
        updatePauseMenu(input, ui);
        break;
    case ScreenMode::Inventory:
        inventory_.updateScreen(input, ui, player_, spellRing_, effectDispatcher_);
        if (!inventory_.isOpen()) {
            mode_ = inventoryReturnToPause_ ? ScreenMode::PauseMenu : ScreenMode::Playing;
            pausePage_ = PauseMenuPage::Main;
            inventoryReturnToPause_ = false;
        }
        break;
    case ScreenMode::Ring:
        updateRingScreen(input, ui);
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
    return mode_ != ScreenMode::Playing;
}

bool Game::basePresentationActive() const
{
    if (mode_ == ScreenMode::Base) {
        return true;
    }
    if (pauseReturnMode_ != ScreenMode::Base) {
        return false;
    }
    return mode_ == ScreenMode::PauseMenu ||
        mode_ == ScreenMode::Inventory ||
        mode_ == ScreenMode::Ring;
}

void Game::renderBookshelfScreen(Renderer& renderer) const
{
    const UiRect panel = basePanelRect();
    renderer.drawText(panel.pos + Vec2{202.0f, 214.0f}, "本棚", {246, 235, 255, 255}, 3);

    char buffer[256];
    const auto menuName = [](int index) {
        switch (index) {
        case 0:
            return "アイテム図鑑";
        case 1:
            return "宝図鑑";
        case 2:
            return "敵図鑑";
        default:
            return "";
        }
    };
    const auto objectAt = [this](BookshelfPage page, int targetIndex) -> const ObjectDefinition* {
        int index = 0;
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((page == BookshelfPage::Treasures && treasure) ||
                (page == BookshelfPage::Items && !treasure)) {
                if (index == targetIndex) {
                    return &object;
                }
                ++index;
            }
        }
        return nullptr;
    };

    if (bookshelfPage_ == BookshelfPage::Menu) {
        renderer.drawText(panel.pos + Vec2{76.0f, 224.0f}, "図鑑を選んでください", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BookshelfMenuItemCount; ++i) {
            drawUiButton(renderer, bookshelfItemRect(i), menuName(i), i == bookshelfSelection_);
        }
        renderer.drawText(panel.pos + Vec2{112.0f, 474.0f}, "F/Enter 決定  Esc 戻る", {198, 198, 206, 255}, 2);
        return;
    }

    std::vector<std::string> lines;
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (enemy.name.empty() ? enemy.id : enemy.name);
            std::snprintf(buffer, sizeof(buffer), "%s  [%s]", name.c_str(), encyclopediaStageName(stage));
            lines.emplace_back(buffer);
        }
    } else {
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((bookshelfPage_ == BookshelfPage::Treasures && !treasure) ||
                (bookshelfPage_ == BookshelfPage::Items && treasure)) {
                continue;
            }
            const EncyclopediaStage stage = encyclopedia_.objectStage(object.id, treasure);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object.name.empty() ? object.id : object.name);
            std::snprintf(buffer, sizeof(buffer), "%s  [%s]", name.c_str(), encyclopediaStageName(stage));
            lines.emplace_back(buffer);
        }
    }

    const char* title = bookshelfPage_ == BookshelfPage::Items
        ? "アイテム図鑑"
        : (bookshelfPage_ == BookshelfPage::Treasures ? "宝図鑑" : "敵図鑑");
    renderer.drawText(panel.pos + Vec2{74.0f, 224.0f}, title, {198, 198, 206, 255}, 2);
    if (lines.empty()) {
        renderer.drawText(panel.pos + Vec2{94.0f, 276.0f}, "記録対象がありません", {150, 150, 160, 255}, 2);
    } else {
        const int visibleCount = std::min(10, static_cast<int>(lines.size()));
        const int firstVisibleIndex = std::clamp(
            bookshelfSelection_ - visibleCount / 2,
            0,
            std::max(0, static_cast<int>(lines.size()) - visibleCount));
        for (int i = 0; i < visibleCount; ++i) {
            const int lineIndex = firstVisibleIndex + i;
            drawUiButton(renderer, bookshelfItemRect(i), lines[static_cast<std::size_t>(lineIndex)], lineIndex == bookshelfSelection_);
        }
    }

    renderer.fillRect({414.0f, 600.0f}, {452.0f, 42.0f}, {18, 18, 28, 220});
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        if (bookshelfSelection_ >= 0 && bookshelfSelection_ < static_cast<int>(enemyCatalog_.enemies.size())) {
            const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(bookshelfSelection_)];
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (enemy.name.empty() ? enemy.id : enemy.name);
            std::snprintf(buffer, sizeof(buffer), "%s / %s", name.c_str(), encyclopediaStageName(stage));
            renderer.drawText({426.0f, 610.0f}, buffer, {255, 230, 150, 255}, 2);
        }
    } else if (const ObjectDefinition* object = objectAt(bookshelfPage_, bookshelfSelection_)) {
        const bool treasure = object->category == "\xE5\xAE\x9D";
        const EncyclopediaStage stage = encyclopedia_.objectStage(object->id, treasure);
        const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object->name.empty() ? object->id : object->name);
        const std::vector<std::string> effects = encyclopedia_.objectEffects(object->id);
        std::string effectText = effects.empty() ? "効果未確認" : "判明効果 " + effects.front();
        std::snprintf(buffer, sizeof(buffer), "%s / %s / %s", name.c_str(), encyclopediaStageName(stage), effectText.c_str());
        renderer.drawText({426.0f, 610.0f}, buffer, {255, 230, 150, 255}, 2);
    }
    renderer.drawText(panel.pos + Vec2{112.0f, 474.0f}, "W/S 選択  Esc 一覧へ戻る", {198, 198, 206, 255}, 2);
}

void Game::renderBaseScreen(Renderer& renderer) const
{
    if (!basePresentationActive()) {
        return;
    }

    renderer.setScreenSpace();
    if (baseStorageActive_) {
        renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {10, 12, 18, 255});
        const UiRect panel = storagePanelRect();
        drawUiWindow(renderer, panel, "収納箱 / 倉庫", "←/→ リュック・倉庫切替  F/Enter 移動  Esc 戻る");

        const std::vector<StorageEntry> backpackEntries = backpackStorageEntries();
        const std::vector<StorageEntry> warehouseEntries = warehouseStorageEntries();
        const auto visibleOffset = [](int selection, int count) {
            if (count <= WarehouseVisibleRows) {
                return 0;
            }
            return std::clamp(selection - WarehouseVisibleRows / 2, 0, count - WarehouseVisibleRows);
        };
        const int backpackOffset = visibleOffset(baseStorageBackpackSelection_, static_cast<int>(backpackEntries.size()));
        const int warehouseOffset = visibleOffset(baseStorageWarehouseSelection_, static_cast<int>(warehouseEntries.size()));

        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "リュック %d/24", backpackUsedSlots());
        renderer.drawText({72.0f, 108.0f}, buffer, baseStorageWarehousePane_ ? Color{198, 198, 206, 255} : Color{255, 230, 150, 255}, 3);
        std::snprintf(buffer, sizeof(buffer), "倉庫 %d/%d", warehouseUsedSlots(), warehouseCapacity());
        renderer.drawText({430.0f, 108.0f}, buffer, baseStorageWarehousePane_ ? Color{255, 230, 150, 255} : Color{198, 198, 206, 255}, 3);
        renderer.drawText({824.0f, 108.0f}, "詳細 / 素材", {246, 235, 255, 255}, 3);

        const int backpackVisible = std::min(WarehouseVisibleRows, static_cast<int>(backpackEntries.size()) - backpackOffset);
        for (int i = 0; i < backpackVisible; ++i) {
            const int entryIndex = backpackOffset + i;
            drawUiButton(renderer, storageBackpackItemRect(i), storageEntryLabel(backpackEntries[static_cast<std::size_t>(entryIndex)], false).c_str(),
                !baseStorageWarehousePane_ && entryIndex == baseStorageBackpackSelection_);
        }
        if (backpackEntries.empty()) {
            renderer.drawText({88.0f, 168.0f}, "リュック内の通常アイテムなし", {150, 150, 160, 255}, 2);
        }

        const int warehouseVisible = std::min(WarehouseVisibleRows, static_cast<int>(warehouseEntries.size()) - warehouseOffset);
        for (int i = 0; i < warehouseVisible; ++i) {
            const int entryIndex = warehouseOffset + i;
            drawUiButton(renderer, storageWarehouseItemRect(i), storageEntryLabel(warehouseEntries[static_cast<std::size_t>(entryIndex)], true).c_str(),
                baseStorageWarehousePane_ && entryIndex == baseStorageWarehouseSelection_);
        }
        if (warehouseEntries.empty()) {
            renderer.drawText({448.0f, 168.0f}, "倉庫は空です", {150, 150, 160, 255}, 2);
        }

        renderer.fillRect({814.0f, 146.0f}, {380.0f, 336.0f}, {18, 18, 28, 230});
        renderer.drawRect({814.0f, 146.0f}, {380.0f, 336.0f}, {120, 122, 150, 255});

        const std::vector<StorageEntry>& activeEntries = baseStorageWarehousePane_ ? warehouseEntries : backpackEntries;
        const int activeSelection = baseStorageWarehousePane_ ? baseStorageWarehouseSelection_ : baseStorageBackpackSelection_;
        float detailY = 166.0f;
        if (!activeEntries.empty() && activeSelection >= 0 && activeSelection < static_cast<int>(activeEntries.size())) {
            const StorageEntry entry = activeEntries[static_cast<std::size_t>(activeSelection)];
            const bool warehouseEntry = baseStorageWarehousePane_;
            const ItemData* item = storageEntryItem(entry, warehouseEntry);
            const ItemInstance* instance = storageEntryInstance(entry, warehouseEntry);
            if (item != nullptr) {
                renderer.drawText({834.0f, detailY}, item->name, {246, 235, 255, 255}, 3);
                detailY += 42.0f;
                renderer.drawText({834.0f, detailY}, "カテゴリ", {168, 172, 184, 255}, 2);
                renderer.drawText({948.0f, detailY}, item->category, {235, 235, 240, 255}, 2);
                detailY += 30.0f;
                std::snprintf(buffer, sizeof(buffer), "%d", storageEntryStackCount(entry, warehouseEntry));
                renderer.drawText({834.0f, detailY}, "スタック数", {168, 172, 184, 255}, 2);
                renderer.drawText({948.0f, detailY}, buffer, {235, 235, 240, 255}, 2);
                detailY += 30.0f;
                if (instance != nullptr) {
                    std::snprintf(buffer, sizeof(buffer), "Lv.%d  攻撃+%d 掘削+%d 耐久+%d",
                        instance->enhanceLevel,
                        instance->attackBonus,
                        instance->digBonus,
                        instance->durabilityBonus);
                    renderer.drawText({834.0f, detailY}, "強化値", {168, 172, 184, 255}, 2);
                    renderer.drawText({948.0f, detailY}, buffer, {235, 235, 240, 255}, 2);
                    detailY += 30.0f;
                    renderer.drawText({834.0f, detailY}, "保護", {168, 172, 184, 255}, 2);
                    renderer.drawText({948.0f, detailY}, instance->protectionEnabled ? "ON 売却不可" : "OFF", instance->protectionEnabled ? Color{116, 220, 255, 255} : Color{235, 235, 240, 255}, 2);
                    detailY += 30.0f;
                    renderer.drawText({834.0f, detailY}, "状態", {168, 172, 184, 255}, 2);
                    renderer.drawText({948.0f, detailY}, instance->isBroken ? "破損" : "通常", instance->isBroken ? Color{255, 120, 120, 255} : Color{235, 235, 240, 255}, 2);
                    detailY += 30.0f;
                } else {
                    renderer.drawText({834.0f, detailY}, "保護", {168, 172, 184, 255}, 2);
                    renderer.drawText({948.0f, detailY}, "対象外", {150, 150, 160, 255}, 2);
                    detailY += 30.0f;
                    renderer.drawText({834.0f, detailY}, "状態", {168, 172, 184, 255}, 2);
                    renderer.drawText({948.0f, detailY}, "スタック", {235, 235, 240, 255}, 2);
                    detailY += 30.0f;
                }
                renderer.drawText({834.0f, detailY}, item->effectText.empty() ? item->description : item->effectText, {198, 198, 206, 255}, 2);
            }
        } else {
            renderer.drawText({834.0f, detailY}, "アイテム未選択", {150, 150, 160, 255}, 2);
        }

        renderer.fillRect({814.0f, 500.0f}, {380.0f, 118.0f}, {18, 18, 28, 230});
        renderer.drawRect({814.0f, 500.0f}, {380.0f, 118.0f}, {120, 122, 150, 255});
        renderer.drawText({834.0f, 516.0f}, "素材所持数", {246, 235, 255, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "土 %d / 石 %d / 鉱石 %d",
            inventory_.itemCount(InventoryItemType::Dirt),
            inventory_.itemCount(InventoryItemType::Stone),
            inventory_.itemCount(InventoryItemType::Ore));
        renderer.drawText({834.0f, 548.0f}, buffer, {198, 198, 206, 255}, 2);
        for (int i = 0; i < static_cast<int>(MaterialType::Count); ++i) {
            const MaterialType type = static_cast<MaterialType>(i);
            std::snprintf(buffer, sizeof(buffer), "%s %d", std::string(materialTypeDisplayName(type)).c_str(), inventory_.materialCount(type));
            renderer.drawText({834.0f + static_cast<float>(i % 2) * 172.0f, 578.0f + static_cast<float>(i / 2) * 24.0f}, buffer, {198, 198, 206, 255}, 2);
        }

        if (!baseStatus_.empty()) {
            renderer.drawText({74.0f, 626.0f}, baseStatus_, {255, 230, 150, 255}, 2);
        } else {
            renderer.drawText({74.0f, 626.0f}, "リュック側で決定すると預け入れ、倉庫側で決定すると取り出します。素材は表示のみです。", {198, 198, 206, 255}, 2);
        }
        return;
    }

    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    renderer.fillRect(map.pos, map.size, {68, 96, 58, 255});
    renderer.drawRect(map.pos, map.size, {156, 128, 82, 255});
    renderer.fillRect({62.0f, 456.0f}, {1156.0f, 88.0f}, {98, 84, 58, 255});
    renderer.fillRect({566.0f, 130.0f}, {132.0f, 430.0f}, {92, 78, 54, 255});
    renderer.fillRect({330.0f, 72.0f}, {154.0f, 100.0f}, {96, 54, 62, 255});
    renderer.drawRect({330.0f, 72.0f}, {154.0f, 100.0f}, {216, 184, 130, 255});
    renderer.drawText({350.0f, 106.0f}, "主人公の家", {246, 235, 255, 255}, 2);
    renderer.fillRect({600.0f, 586.0f}, {80.0f, 34.0f}, {38, 30, 36, 255});
    renderer.drawCircle({640.0f, 602.0f}, 42.0f, {160, 122, 80, 255});

    const std::array<BaseFacility, 10> facilities = baseFacilities(ringWorkshopUnlocked_);
    const BaseFacility* nearest = nullptr;
    const BaseFacility* hovered = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    const Vec2 mouse{mouseX, mouseY};
    for (const BaseFacility& facility : facilities) {
        if (!facility.enabled) {
            continue;
        }
        if (facility.rect.contains(mouse)) {
            hovered = &facility;
        }
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }
    for (const BaseFacility& facility : facilities) {
        Color fill = facility.enabled ? Color{96, 82, 82, 255} : Color{84, 62, 56, 255};
        if (!facility.unlocked) {
            fill = {58, 58, 64, 255};
        }
        if (&facility == nearest) {
            fill = {118, 98, 58, 255};
        }
        if (&facility == hovered) {
            fill = {124, 104, 72, 255};
        }
        renderer.fillRect(facility.rect.pos, facility.rect.size, fill);
        renderer.drawRect(facility.rect.pos, facility.rect.size, facility.enabled ? Color{220, 200, 150, 255} : Color{120, 108, 98, 255});
        renderer.drawText(facility.rect.pos + Vec2{8.0f, 8.0f}, facility.displayName, facility.enabled ? Color{248, 238, 214, 255} : Color{154, 146, 138, 255}, 2);
    }

    renderer.fillCircle(basePlayerPosition_, balance_.playerRadius, {118, 72, 168, 255});
    renderer.drawLine(basePlayerPosition_, basePlayerPosition_ + basePlayerFacing_ * 22.0f, {235, 210, 255, 255});

    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "Stage %d/%d   Warp %d   Money %dG   商人 %s",
        currentStage_ + 1,
        unlockedStages_,
        unlockedWarpPointCount_,
        money_,
        merchantRefreshPending_ ? "更新あり" : "通常");
    renderer.fillRect({20.0f, 14.0f}, {680.0f, 28.0f}, {0, 0, 0, 150});
    renderer.drawText({30.0f, 20.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "所持品: 土 %d / 石 %d / 鉱石 %d   リング装着 %d",
        inventory_.itemCount(InventoryItemType::Dirt),
        inventory_.itemCount(InventoryItemType::Stone),
        inventory_.itemCount(InventoryItemType::Ore),
        static_cast<int>(spellRing_.items().size()));
    renderer.fillRect({720.0f, 14.0f}, {520.0f, 28.0f}, {0, 0, 0, 150});
    renderer.drawText({730.0f, 20.0f}, buffer, {198, 198, 206, 255}, 2);

    const bool panelUiActive = baseRingWorkshopActive_ ||
        baseBookshelfActive_ ||
        baseProcessingActive_ ||
        baseSellActive_ ||
        baseUpgradeActive_ ||
        baseMiningStartChoiceActive_;
    const UiRect panel = basePanelRect();
    if (panelUiActive) {
        drawUiWindow(renderer, panel, "魔女の拠点", "W/S または ↑↓  F/Enter 決定  左クリック 決定");
    }

    if (baseBookshelfActive_) {
        renderBookshelfScreen(renderer);
    } else if (baseRingWorkshopActive_) {
        renderer.drawText(panel.pos + Vec2{168.0f, 214.0f}, "リング工房", {246, 235, 255, 255}, 3);
        const int totalPoints = ringLevelUpgradePointTotal();
        std::snprintf(buffer, sizeof(buffer), "レベルアップ強化点 %d  現在: 半径%d / 速度%d  配分案: 半径%d / 速度%d",
            totalPoints,
            levelRingRadiusPoints_,
            levelRingSpeedPoints_,
            ringWorkshopDraftRadiusPoints_,
            ringWorkshopDraftSpeedPoints_);
        renderer.drawText(panel.pos + Vec2{54.0f, 224.0f}, buffer, {198, 198, 206, 255}, 2);

        const float currentRadius = effectiveInitialRingRadius(levelRingRadiusPoints_);
        const float currentSpeed = effectiveInitialRingSpeed(levelRingSpeedPoints_);
        const float draftRadius = effectiveInitialRingRadius(ringWorkshopDraftRadiusPoints_);
        const float draftSpeed = effectiveInitialRingSpeed(ringWorkshopDraftSpeedPoints_);
        std::snprintf(buffer, sizeof(buffer), "再調整後: 半径 %.0f -> %.0f / 速度 %.2f -> %.2f  費用 %dG 月のカケラ%d",
            currentRadius,
            draftRadius,
            currentSpeed,
            draftSpeed,
            ringWorkshopRespecMoneyCost(),
            ringWorkshopRespecMoonCost());
        renderer.drawText(panel.pos + Vec2{54.0f, 448.0f}, buffer, {198, 198, 206, 255}, 2);

        for (int i = 0; i < BaseRingWorkshopItemCount; ++i) {
            if (i == 0) {
                std::snprintf(buffer, sizeof(buffer), "再調整: 速度から半径へ  半径%d / 速度%d",
                    ringWorkshopDraftRadiusPoints_,
                    ringWorkshopDraftSpeedPoints_);
            } else if (i == 1) {
                std::snprintf(buffer, sizeof(buffer), "再調整: 半径から速度へ  半径%d / 速度%d",
                    ringWorkshopDraftRadiusPoints_,
                    ringWorkshopDraftSpeedPoints_);
            } else if (i == 2) {
                std::snprintf(buffer, sizeof(buffer), "再調整確定  %dG 月のカケラ%d",
                    ringWorkshopRespecMoneyCost(),
                    ringWorkshopRespecMoonCost());
            } else if (i >= 3 && i < 3 + RingWorkshopImplementedUpgradeCount) {
                const auto upgrade = static_cast<RingWorkshopUpgrade>(i - 3);
                const int level = ringWorkshopUpgradeLevel(upgrade);
                const int maxLevel = ringWorkshopUpgradeMaxLevel(upgrade);
                if (level >= maxLevel) {
                    std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  上限",
                        ringWorkshopUpgradeName(upgrade),
                        level,
                        maxLevel);
                } else {
                    std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  %.2f -> %.2f  %dG 月のカケラ%d",
                        ringWorkshopUpgradeName(upgrade),
                        level,
                        maxLevel,
                        ringWorkshopUpgradeCurrentValue(upgrade),
                        ringWorkshopUpgradeNextValue(upgrade),
                        ringWorkshopUpgradeMoneyCost(upgrade),
                        ringWorkshopUpgradeMoonCost(upgrade));
                }
            } else {
                const char* futureName = "";
                switch (i) {
                case 6:
                    futureName = "リング投げ距離強化";
                    break;
                case 7:
                    futureName = "リング投げクールダウン短縮";
                    break;
                case 8:
                    futureName = "リング重量ペナルティ軽減";
                    break;
                case 9:
                    futureName = "リング装着枠増加";
                    break;
                default:
                    futureName = "未解禁項目";
                    break;
                }
                std::snprintf(buffer, sizeof(buffer), "%s  未解禁", futureName);
            }
            drawUiButton(renderer, baseRingWorkshopItemRect(i), buffer, i == baseRingWorkshopSelection_);
        }

        std::snprintf(buffer, sizeof(buffer), "所持 %dG / 月のカケラ %d   F/Enter 実行  左右で配分変更  Esc 戻る",
            money_,
            inventory_.materialCount(MaterialType::MoonFragment));
        renderer.drawText(panel.pos + Vec2{54.0f, 474.0f}, buffer, {198, 198, 206, 255}, 2);
        drawUiButton(renderer, ringWorkshopConfirmRect(), "再調整確定", false);
    } else if (baseProcessingActive_) {
        renderer.drawText(panel.pos + Vec2{164.0f, 214.0f}, "魔導加工台", {246, 235, 255, 255}, 3);
        std::snprintf(buffer, sizeof(buffer), "加工解禁 Lv.%d/5  将来枠: 軽量化 / 重量化 / 大型化 / 小型化 / 追加効果付与",
            processingUnlockLevel_);
        renderer.drawText(panel.pos + Vec2{54.0f, 224.0f}, buffer, {198, 198, 206, 255}, 2);
        for (int i = 0; i < BaseProcessingModeCount; ++i) {
            drawUiButton(renderer, baseProcessingModeRect(i), processingModeName(static_cast<ProcessingMode>(i)), i == baseProcessingMode_);
        }

        const std::vector<StorageEntry> entries = processingEntries();
        const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
        if (entries.empty()) {
            renderer.drawText(panel.pos + Vec2{96.0f, 314.0f}, "加工できる通常アイテムがありません。", {198, 198, 206, 255}, 2);
        } else {
            const int visibleCount = std::min(5, static_cast<int>(entries.size()));
            const auto& stacks = inventory_.objectStacks();
            const auto& instances = inventory_.objectInstances();
            for (int i = 0; i < visibleCount; ++i) {
                const StorageEntry entry = entries[static_cast<std::size_t>(i)];
                const bool available = processingEntryAvailable(entry);
                const int moneyCost = processingMoneyCost(entry, mode);
                const int oreCost = processingOreCost(entry, mode);
                if (entry.kind == StorageEntryKind::Stack) {
                    const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(entry.index)];
                    if (mode == ProcessingMode::Repair) {
                        std::snprintf(buffer, sizeof(buffer), "%s x%d  修理不可", stack.item.name.c_str(), stack.count);
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%s x%d  %dG 鉱石%d", stack.item.name.c_str(), stack.count, moneyCost, oreCost);
                    }
                } else {
                    const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(entry.index)];
                    if (!available) {
                        std::snprintf(buffer, sizeof(buffer), "%s Lv.%d  %s",
                            instance.item.name.c_str(),
                            instance.instance.enhanceLevel,
                            mode == ProcessingMode::Repair ? "修理不要" : "上限");
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%s Lv.%d  %dG 鉱石%d",
                            instance.item.name.c_str(),
                            instance.instance.enhanceLevel,
                            moneyCost,
                            oreCost);
                    }
                }
                drawUiButton(renderer, baseProcessingItemRect(i), buffer, i == baseProcessingSelection_);
            }
        }
        std::snprintf(buffer, sizeof(buffer), "所持金 %dG / 強化鉱石 %d / 最大強化 +%d",
            money_,
            inventory_.materialCount(MaterialType::EnhancementOre),
            MaxItemEnhanceLevel);
        renderer.drawText(panel.pos + Vec2{72.0f, 476.0f}, buffer, {198, 198, 206, 255}, 2);
    } else if (baseSellActive_) {
        renderer.drawText(panel.pos + Vec2{176.0f, 214.0f}, "商人ワゴン", {246, 235, 255, 255}, 3);
        const std::vector<SellableEntry> sellable = sellableObjects();
        renderer.drawText({390.0f, 246.0f}, "売却対象", baseMerchantBuyPane_ ? Color{198, 198, 206, 255} : Color{255, 230, 150, 255}, 2);
        renderer.drawText({660.0f, 246.0f}, "購入商品", baseMerchantBuyPane_ ? Color{255, 230, 150, 255} : Color{198, 198, 206, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "所持金 %dG", money_);
        renderer.drawText({752.0f, 218.0f}, buffer, {230, 230, 236, 255}, 2);
        if (sellable.empty()) {
            renderer.drawText({398.0f, 292.0f}, "売却対象がありません", {198, 198, 206, 255}, 2);
        } else {
            const auto& stacks = inventory_.objectStacks();
            const auto& instances = inventory_.objectInstances();
            const int visibleCount = std::min(static_cast<int>(sellable.size()), 5);
            for (int i = 0; i < visibleCount; ++i) {
                const SellableEntry entry = sellable[static_cast<std::size_t>(i)];
                if (entry.kind == SellableKind::Stack) {
                    const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(entry.index)];
                    if (entry.sellable) {
                        std::snprintf(buffer, sizeof(buffer), "%s x%d %dG", stack.item.name.c_str(), stack.count, entry.price);
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%s x%d [%s]", stack.item.name.c_str(), stack.count, entry.blockedReason.c_str());
                    }
                } else {
                    const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(entry.index)];
                    if (entry.sellable) {
                        std::snprintf(buffer, sizeof(buffer), "%s %dG", instance.item.name.c_str(), entry.price);
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%s [%s]", instance.item.name.c_str(), entry.blockedReason.c_str());
                    }
                }
                drawUiButton(renderer, merchantSellItemRect(i), buffer, !baseMerchantBuyPane_ && i == baseSellSelection_);
            }
        }
        if (merchantStock_.empty()) {
            renderer.drawText({670.0f, 292.0f}, "基本商品なし", {198, 198, 206, 255}, 2);
        } else {
            const int visibleCount = std::min(static_cast<int>(merchantStock_.size()), 5);
            for (int i = 0; i < visibleCount; ++i) {
                const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(i)];
                const ItemData* item = objectCatalog_.registry.findById(product.objectId);
                const std::string name = item != nullptr ? item->name : product.objectId;
                std::snprintf(buffer, sizeof(buffer), "%s %dG", name.c_str(), product.price);
                drawUiButton(renderer, merchantBuyItemRect(i), buffer, baseMerchantBuyPane_ && i == baseMerchantBuySelection_);
            }
        }
        std::snprintf(buffer, sizeof(buffer), "商人機能 Lv.%d/7  未実装枠: 高価買取カテゴリ / 珍品販売 / 珍品レア度上昇",
            merchantUpgradeLevel_);
        renderer.drawText(panel.pos + Vec2{54.0f, 422.0f}, buffer, {198, 198, 206, 255}, 2);
        renderer.drawText(panel.pos + Vec2{54.0f, 450.0f}, "保護ONの個体、リング装備中アイテム、素材、ストーリーアイテムは売却不可", {198, 198, 206, 255}, 2);
        renderer.drawText(panel.pos + Vec2{100.0f, 478.0f}, "←/→ 売却・購入切替  F/Enter 実行  Esc / 右クリックで戻る", {198, 198, 206, 255}, 2);
    } else if (baseUpgradeActive_) {
        renderer.drawText(panel.pos + Vec2{176.0f, 214.0f}, "強化炉", {246, 235, 255, 255}, 3);
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            if (!upgradeImplemented(i)) {
                std::snprintf(buffer, sizeof(buffer), "%s  未実装", upgradeName(i));
            } else if (upgradeMaxed(i)) {
                std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  上限", upgradeName(i), upgradeLevel(i), upgradeMaxLevel(i));
            } else {
                const MaterialType materialType = upgradeMaterialType(i);
                std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  %dG %s%d",
                    upgradeName(i),
                    upgradeLevel(i),
                    upgradeMaxLevel(i),
                    upgradeCost(i),
                    std::string(materialTypeDisplayName(materialType)).c_str(),
                    upgradeMaterialCost(i));
            }
            drawUiButton(renderer, baseUpgradeItemRect(i), buffer, i == baseUpgradeSelection_);
        }
        std::snprintf(buffer, sizeof(buffer), "所持: %dG / 古木 %d / 魔力のしずく %d",
            money_,
            inventory_.materialCount(MaterialType::OldWoodBuildingMaterial),
            inventory_.materialCount(MaterialType::ManaDrop));
        renderer.drawText(panel.pos + Vec2{54.0f, 448.0f}, buffer, {198, 198, 206, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "効果: 倉庫%d枠 / 商人Lv.%d / 加工解禁Lv.%d / HP+%d 半径+%d%% 速度+%d%%",
            warehouseCapacity(),
            merchantUpgradeLevel_,
            processingUnlockLevel_,
            maxHpUpgradeLevel_ * 2,
            ringRadiusUpgradeLevel_ * 8,
            ringSpeedUpgradeLevel_ * 8);
        renderer.drawText(panel.pos + Vec2{54.0f, 474.0f}, buffer, {198, 198, 206, 255}, 2);
        renderer.drawText(panel.pos + Vec2{122.0f, 502.0f}, "F/Enter 強化  Esc / 右クリックで戻る", {198, 198, 206, 255}, 2);
    } else if (baseMiningStartChoiceActive_) {
        renderer.drawText(panel.pos + Vec2{178.0f, 238.0f}, "採掘出口", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{116.0f, 278.0f}, "開始地点または最新ワープポイントから出発", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            const bool disabled = i == 1 && unlockedWarpPointCount_ <= 0;
            drawUiButton(renderer, baseMiningStartChoiceRect(i), baseMiningStartChoiceName(i), i == baseMiningStartSelection_ && !disabled);
            if (disabled) {
                const UiRect rect = baseMiningStartChoiceRect(i);
                renderer.fillRect(rect.pos, rect.size, {0, 0, 0, 110});
                renderer.drawText(rect.pos + Vec2{116.0f, 14.0f}, "未解放", {150, 150, 160, 255}, 2);
            }
        }
        renderer.drawText(panel.pos + Vec2{122.0f, 442.0f}, "Esc / 右クリックで戻る", {198, 198, 206, 255}, 2);
    } else {
        const char* promptName = nearest != nullptr ? nearest->displayName : "";
        renderer.fillRect({280.0f, 644.0f}, {720.0f, 34.0f}, {0, 0, 0, 170});
        if (nearest != nullptr) {
            std::snprintf(buffer, sizeof(buffer), "Enter: %sを調べる / クリック: 近くの施設を調べる / Esc: メニュー", promptName);
            renderer.drawText({300.0f, 652.0f}, buffer, {255, 230, 150, 255}, 2);
        } else {
            renderer.drawText({300.0f, 652.0f}, "WASD / 矢印キー: 移動  Enter: 近くの施設を調べる  Esc: メニュー", {198, 198, 206, 255}, 2);
        }
        if (!baseStatus_.empty()) {
            renderer.fillRect({280.0f, 604.0f}, {720.0f, 30.0f}, {0, 0, 0, 160});
            renderer.drawText({300.0f, 612.0f}, baseStatus_, {255, 230, 150, 255}, 2);
        }
        if (debug_.visible()) {
            char debugBuffer[512];
            std::snprintf(debugBuffer, sizeof(debugBuffer),
                "Base map mode\nplayer base position %.1f, %.1f\nnearest facility name %s\nhovered facility name %s\ninteraction available %s\npause menu return mode %s",
                basePlayerPosition_.x,
                basePlayerPosition_.y,
                nearest != nullptr ? nearest->displayName : "-",
                hovered != nullptr ? hovered->displayName : "-",
                nearest != nullptr ? "true" : "false",
                screenModeName(pauseReturnMode_));
            renderer.fillRect({10.0f, 10.0f}, {380.0f, 158.0f}, {0, 0, 0, 180});
            renderer.drawText({20.0f, 20.0f}, debugBuffer, {220, 244, 224, 255}, 2);
        }
        return;
    }

    if (!baseStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{54.0f, 504.0f}, baseStatus_, {255, 230, 150, 255}, 2);
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
        renderer.fillCircle(point.position, 12.0f, core);
        renderer.drawCircle(point.position, 20.0f, ring);
        renderer.drawCircle(point.position, point.discovered ? 34.0f : 24.0f, {150, 210, 255, 110});
        renderer.drawLine(point.position + Vec2{-18.0f, 0.0f}, point.position + Vec2{18.0f, 0.0f}, ring);
        renderer.drawLine(point.position + Vec2{0.0f, -18.0f}, point.position + Vec2{0.0f, 18.0f}, ring);
    }

    if (hasBossSpawnPoint_ && !bossSpawned_) {
        renderer.drawCircle(bossSpawnPoint_, BossSpawnTriggerRadius, {255, 98, 92, 150});
        renderer.drawCircle(bossSpawnPoint_, 18.0f, {255, 180, 80, 230});
        renderer.drawLine(bossSpawnPoint_ + Vec2{-22.0f, -22.0f}, bossSpawnPoint_ + Vec2{22.0f, 22.0f}, {255, 120, 90, 210});
        renderer.drawLine(bossSpawnPoint_ + Vec2{-22.0f, 22.0f}, bossSpawnPoint_ + Vec2{22.0f, -22.0f}, {255, 120, 90, 210});
    }
}

void Game::renderRewardNodes(Renderer& renderer, const std::vector<LightSource>& extraLights) const
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
        if (node.visibility == PlacementVisibility::Exposed) {
            renderer.fillCircle(center, 7.0f, exposedReward);
            renderer.drawCircle(center, 12.0f, {255, 246, 180, 210});
            renderer.drawLine(center + Vec2{-9.0f, 0.0f}, center + Vec2{9.0f, 0.0f}, {255, 250, 210, 220});
            renderer.drawLine(center + Vec2{0.0f, -9.0f}, center + Vec2{0.0f, 9.0f}, {255, 250, 210, 220});
        } else if (node.visibility == PlacementVisibility::BuriedVisible) {
            renderer.drawLine(center + Vec2{-8.0f, 0.0f}, center + Vec2{8.0f, 0.0f}, sparkle);
            renderer.drawLine(center + Vec2{0.0f, -8.0f}, center + Vec2{0.0f, 8.0f}, sparkle);
            renderer.fillCircle(center, 2.5f, {255, 255, 210, 240});
        }
    }

    for (const MoneyNode& node : moneyNodes_) {
        if (node.collected) {
            continue;
        }
        const Vec2 center = tileWorldCenter(node.tile);
        if (!tileMap_.isLit(center, player_.position, extraLights)) {
            continue;
        }
        if (node.visibility == PlacementVisibility::Exposed) {
            renderer.fillCircle(center, 5.5f, exposedMoney);
            renderer.drawCircle(center, 8.5f, {255, 230, 120, 210});
        } else if (node.visibility == PlacementVisibility::BuriedVisible) {
            renderer.drawLine(center + Vec2{-6.0f, -6.0f}, center + Vec2{6.0f, 6.0f}, sparkle);
            renderer.drawLine(center + Vec2{-6.0f, 6.0f}, center + Vec2{6.0f, -6.0f}, sparkle);
        }
    }
}

void Game::renderRingScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::Ring) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = ringPanelRect();
    drawUiWindow(renderer, panel, "リング管理", "Z/X・1-3 リング選択  WASD/矢印・Q/E スロット  F/Enter 詳細  R 外す  G つかむ/置く  P 保護  Esc 戻る");

    char buffer[192];
    for (int i = 0; i < RingCount; ++i) {
        std::snprintf(buffer, sizeof(buffer), "Ring %d%s", i + 1, i == 0 ? "" : " 未実装");
        drawUiButton(renderer, ringTabRect(i), buffer, i == spellRing_.activeRingIndex());
    }

    const bool actualRing = spellRing_.activeRingIndex() == 0;
    const auto& items = spellRing_.items();

    renderer.drawText(panel.pos + Vec2{48.0f, 128.0f}, "リング情報", {246, 248, 255, 255}, 3);
    std::snprintf(buffer, sizeof(buffer), "リング番号: %d / 3", spellRing_.activeRingIndex() + 1);
    renderer.drawText(panel.pos + Vec2{48.0f, 174.0f}, buffer, {232, 235, 242, 255}, 2);
    renderer.drawText(panel.pos + Vec2{48.0f, 204.0f}, actualRing ? "リング形状: 円形 (仮)" : "リング形状: 未実装", {232, 235, 242, 255}, 2);
    if (actualRing) {
        std::snprintf(buffer, sizeof(buffer), "半径 %.0f   速度 %.2f   重量 %.1fkg   重量補正 %.2f",
            spellRing_.radius(),
            spellRing_.effectiveAngularSpeed(),
            spellRing_.totalEquippedWeight(),
            spellRing_.weightSpeedMultiplier());
        renderer.drawText(panel.pos + Vec2{48.0f, 520.0f}, buffer, {202, 206, 216, 255}, 2);
    } else {
        renderer.drawText(panel.pos + Vec2{48.0f, 520.0f}, "半径 -   速度 -   ずらし距離 -   投げクールダウン -", {202, 206, 216, 255}, 2);
    }

    renderer.drawText(panel.pos + Vec2{48.0f, 276.0f}, "装着アイテム一覧", {246, 248, 255, 255}, 3);
    for (int i = 0; i < RingSlotCount; ++i) {
        const UiRect slot = ringSlotRect(i);
        const bool selected = i == ringSlotSelection_;
        renderer.fillRect(slot.pos, slot.size, selected ? Color{54, 56, 84, 245} : Color{22, 24, 34, 232});
        renderer.drawRect(slot.pos, slot.size, selected ? Color{255, 230, 150, 255} : Color{90, 98, 120, 255});
        std::snprintf(buffer, sizeof(buffer), "%d", i + 1);
        renderer.drawText(slot.pos + Vec2{8.0f, 8.0f}, buffer, {174, 182, 198, 255}, 2);
        if (actualRing && i < static_cast<int>(items.size())) {
            const std::string itemName = ringItemDisplayName(objectCatalog_, items[i]);
            std::snprintf(buffer, sizeof(buffer), "%02d %s", i + 1, itemName.c_str());
            renderer.drawText(slot.pos + Vec2{24.0f, 34.0f}, buffer, {238, 240, 246, 255}, 2);
        } else {
            renderer.drawText(slot.pos + Vec2{42.0f, 34.0f}, actualRing ? "空き" : "-", {150, 154, 166, 255}, 2);
        }
    }

    const Vec2 detailPos = panel.pos + Vec2{680.0f, 126.0f};
    renderer.drawText(detailPos, "選択中アイテム詳細", {246, 248, 255, 255}, 3);
    renderer.fillRect(detailPos + Vec2{0.0f, 46.0f}, {410.0f, 310.0f}, {18, 20, 30, 232});
    renderer.drawRect(detailPos + Vec2{0.0f, 46.0f}, {410.0f, 310.0f}, {92, 104, 132, 255});

    if (!actualRing) {
        renderer.drawText(detailPos + Vec2{24.0f, 78.0f}, "このリングは未実装です。", {232, 235, 242, 255}, 2);
        renderer.drawText(detailPos + Vec2{24.0f, 116.0f}, "最大3リング前提の表示枠です。", {202, 206, 216, 255}, 2);
    } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
        const SpellRingItem& item = items[ringSlotSelection_];
        const std::string itemName = ringItemDisplayName(objectCatalog_, item);
        const std::string itemCategory = ringItemDisplayCategory(objectCatalog_, item);
        std::snprintf(buffer, sizeof(buffer), "名前: %s", itemName.c_str());
        renderer.drawText(detailPos + Vec2{24.0f, 72.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "カテゴリ: %s", itemCategory.c_str());
        renderer.drawText(detailPos + Vec2{24.0f, 104.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "装着順: %d", ringSlotSelection_ + 1);
        renderer.drawText(detailPos + Vec2{24.0f, 136.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "攻撃力: %d   種別: %s   掘削力: %d", item.damage, item.damageType.c_str(), item.digPower);
        renderer.drawText(detailPos + Vec2{24.0f, 168.0f}, buffer, {232, 235, 242, 255}, 2);
        if (item.durability < 0) {
            std::snprintf(buffer, sizeof(buffer), "耐久力: 壊れない   重さ: %.1fkg", item.weight);
        } else {
            std::snprintf(buffer, sizeof(buffer), "耐久力: %d/%d   重さ: %.1fkg", item.durability, item.maxDurability, item.weight);
        }
        renderer.drawText(detailPos + Vec2{24.0f, 200.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "保護: %s   状態: %s", item.protectionEnabled ? "ON" : "OFF", item.broken() ? "破損" : "通常");
        renderer.drawText(detailPos + Vec2{24.0f, 232.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "強化Lv.%d  攻撃+%d 掘削+%d 耐久+%d",
            item.enhanceLevel,
            item.attackBonus,
            item.digBonus,
            item.durabilityBonus);
        renderer.drawText(detailPos + Vec2{24.0f, 264.0f}, buffer, {202, 206, 216, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "命中間隔: %.2fs   Pで保護ON/OFF", item.hitInterval);
        renderer.drawText(detailPos + Vec2{24.0f, 296.0f}, buffer, {202, 206, 216, 255}, 2);
    } else {
        renderer.drawText(detailPos + Vec2{24.0f, 78.0f}, "空きスロットです。", {202, 206, 216, 255}, 2);
    }

    if (ringGrabActive_) {
        const std::string grabbedName = ringItemDisplayName(objectCatalog_, ringGrabbedItem_);
        std::snprintf(buffer, sizeof(buffer), "つかみ中: %s   G または F で置く / Esc でキャンセル", grabbedName.c_str());
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, buffer, {255, 230, 150, 255}, 2);
    } else if (!ringStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, ringStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::update(const Input& input, const Time& time)
{
    checkHotReload();
    reloadNoticeTimer_ = std::max(0.0f, reloadNoticeTimer_ - time.deltaSeconds());
    encyclopedia_.update(time.deltaSeconds());

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
    captureCooldown_ = std::max(0.0f, captureCooldown_ - time.deltaSeconds());

    UiContext ui(input);
    const bool wasPaused = gameProgressPaused();
    updateScreenMode(input, ui, time.deltaSeconds());
    refreshOrbitEffects();
    const bool paused = gameProgressPaused() || (wasPaused && mode_ == ScreenMode::Playing);

    if (!paused) {
        std::vector<EffectDiscoveryEvent> effectDiscoveries;
        runStats_.elapsedSeconds += time.deltaSeconds();
        tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_, dungeonLayout_);
        player_.update(input, camera_, tileMap_, time.deltaSeconds(), false, balance_);
        if (player_.hp <= 0) {
            enterGameOver();
            refreshOrbitEffects();
            return;
        }
        updateWarpPoints();
        updateExposedRewardNodes();
        updateExposedEnemyNodes();
        if (input.capturePressed() && captureCooldown_ <= 0.0f) {
            const CaptureResult capture = enemies_.tryCapture(player_, spellRing_, inventory_);
            captureCooldown_ = capture.type == CaptureResultType::Success ? 0.35f : 0.75f;
            if (capture.type == CaptureResultType::Success) {
                reloadNotice_ = "捕獲: " + capture.enemyName;
                reloadNoticeTimer_ = 1.6f;
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

        for (const SpellRingItem& item : spellRing_.items()) {
            if (item.objectId.empty() || item.broken()) {
                continue;
            }
            const ObjectDefinition* object = objectCatalog_.registry.findById(item.objectId);
            if (object == nullptr) {
                continue;
            }
            if (item.lightRadius > 0.0f) {
                encyclopedia_.noteItemEffect(*object, "light", "リング上で周囲を照らす", item.worldPosition);
            }
            if (item.hiddenDetectionRadius > 0.0f || item.treasureDetectionRadius > 0.0f) {
                encyclopedia_.noteItemEffect(*object, "detect", "探知効果を発揮する", item.worldPosition);
            }
        }

        tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_, dungeonLayout_);
        digging_.update(tileMap_, spellRing_, player_, time.totalSeconds(), objectCatalog_, effectDispatcher_, &effectDiscoveries);
        for (Vec2 tile : digging_.hitTiles()) {
            effects_.spawnDigHit(tile);
        }
        for (Vec2 tile : digging_.openedTiles()) {
            effects_.spawnTileBreak(tile);
        }
        revealRewardNodesFromOpenedTiles(digging_.openedTiles());
        for (const DugTile& tile : digging_.dugTiles()) {
            ++runStats_.dugTiles;
            if (tile.type != TileType::Ore) {
                inventory_.addFromDugTile(tile.type);
                ++runStats_.acquiredItems;
            }
        }
        for (Vec2 rewardPosition : digging_.rewardDropRequests()) {
            worldDrops_.spawnRewardDrop(objectCatalog_, rewardPosition);
        }
        worldDrops_.spawnFromDugTiles(digging_.dugTiles(), objectCatalog_);
        runStats_.acquiredItems += worldDrops_.update(time.deltaSeconds(), player_, inventory_, objectCatalog_);
        const std::vector<Vec2> randomEnemySpawnTiles = spawnHiddenEnemyNodesFromOpenedTiles(digging_.openedTiles());
        enemies_.spawnFromDugTiles(randomEnemySpawnTiles, tileMap_, player_.position, balance_, enemyCatalog_);
        updateBossSpawn();

        enemies_.update(
            player_,
            spellRing_,
            tileMap_,
            time.deltaSeconds(),
            time.totalSeconds(),
            false,
            balance_,
            objectCatalog_,
            effectDispatcher_,
            projectiles_,
            &effectDiscoveries);
        for (Vec2 explosionPosition : digging_.capturedExplosionRequests()) {
            handleCapturedExplosion(explosionPosition);
        }
        updateCapturedUtilityBehaviors(time.deltaSeconds());
        updateCapturedProjectileBehaviors(time.deltaSeconds());
        projectiles_.update(player_, spellRing_, enemies_, tileMap_, time.deltaSeconds(), effectDispatcher_, objectCatalog_, &effectDiscoveries);

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
                if (!event.enemyId.empty()) {
                    encyclopedia_.noteEnemyDefeated(event.enemyId, event.enemyName, event.position);
                }
                if (event.type == EnemyEventType::BossDeath) {
                    bossDefeated = true;
                }
            } else if (event.type == EnemyEventType::RewardDrop) {
                worldDrops_.spawnRewardDrop(objectCatalog_, event.position);
            } else if (event.type == EnemyEventType::CapturedExplosion) {
                continue;
            } else {
                effects_.spawnEnemyHit(event.position);
            }
        }
        applyEffectDiscoveries(effectDiscoveries);
        syncEncyclopediaFromInventoryAndRing();
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
    if (loadBalanceFromDisk(message)) {
        const bool wasBase = mode_ == ScreenMode::Base;
        InventoryCarryState retained;
        if (wasBase) {
            retained = captureInventoryCarryState();
        }
        initializeWorld(!wasBase);
        if (wasBase) {
            restoreInventoryCarryState(retained);
            enterBase();
        }
        configureWatcher();
        reloadNotice_ = "再読込: " + changedPath;
    } else {
        reloadNotice_ = message;
    }
    reloadNoticeTimer_ = 3.0f;
}

void Game::renderPauseMenu(Renderer& renderer) const
{
    if (mode_ != ScreenMode::PauseMenu) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = pausePanelRect();
    drawUiWindow(renderer, panel, "PAUSED", "W/S または ↑↓  F/Enter 決定  Esc/右クリック 戻る");

    char buffer[160];
    if (pausePage_ == PauseMenuPage::Main) {
        for (int i = 0; i < PauseMenuItemCount; ++i) {
            const bool selected = i == pauseMenuSelection_;
            drawUiButton(renderer, pauseMenuItemRect(i), pauseMenuItemName(i), selected);
        }
        return;
    }

    if (pausePage_ == PauseMenuPage::Status) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "ステータス", {246, 235, 255, 255}, 3);
        std::snprintf(buffer, sizeof(buffer), "HP %02d   Level %02d   XP %02d/%02d", player_.hp, player_.level, player_.xp, player_.xpToNext);
        renderer.drawText(panel.pos + Vec2{58.0f, 160.0f}, buffer, {230, 230, 236, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "Ring %d   Items %02d/08   Radius %.0f   Speed %.1f",
            spellRing_.activeRingIndex() + 1,
            static_cast<int>(spellRing_.items().size()),
            spellRing_.radius(),
            spellRing_.effectiveAngularSpeed());
        renderer.drawText(panel.pos + Vec2{58.0f, 202.0f}, buffer, {230, 230, 236, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Items) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "アイテム", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, "通常画面のショートカットHUDとアイテム画面で管理します。", {230, 230, 236, 255}, 2);
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, "I キーでアイテム画面を開けます。", {198, 198, 206, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Ring) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "リング", {246, 235, 255, 255}, 3);
        std::snprintf(buffer, sizeof(buffer), "アクティブリング %d", spellRing_.activeRingIndex() + 1);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, buffer, {230, 230, 236, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "装着アイテム %02d/08", static_cast<int>(spellRing_.items().size()));
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
        renderer.drawText(panel.pos + Vec2{58.0f, 248.0f}, "Z/X でアクティブリング切替", {198, 198, 206, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Options) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "オプション", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, "仮画面です。設定項目は後続実装で追加します。", {230, 230, 236, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::QuitConfirm) {
        const UiRect confirm = quitConfirmRect();
        drawUiWindow(renderer, confirm, "確認", "Esc / 右クリックで戻る");
        renderer.drawText(confirm.pos + Vec2{58.0f, 82.0f}, "ゲームを終了しますか？", ui::Text, 3);
        renderer.drawText(confirm.pos + Vec2{62.0f, 126.0f}, "セーブは拠点でのみ実行できます。", ui::TextMuted, 2);
        drawUiButton(renderer, quitConfirmButtonRect(0), "キャンセル", pauseConfirmSelection_ == 0);
        drawUiButton(renderer, quitConfirmButtonRect(1), "終了する", pauseConfirmSelection_ == 1);
        return;
    }

    drawUiButton(renderer, pauseBackButtonRect(), "戻る", false);
}

void Game::renderGameOverScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::GameOver) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {0, 0, 0, 150});
    const UiRect panel = gameOverPanelRect();
    drawUiWindow(renderer, panel, "GAME OVER", "W/S または ↑↓  F/Enter 決定");
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "リザルト", ui::Text, 3);

    char buffer[160];
    const std::string deathCause = playerDeathCauseText(player_);
    std::snprintf(buffer, sizeof(buffer), "死因      %s", deathCause.c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, 130.0f}, buffer, {255, 214, 220, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "到達時間  %s", formatRunTime(runStats_.elapsedSeconds).c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, 168.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "レベル    %d", player_.level);
    renderer.drawText(panel.pos + Vec2{136.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "撃破数    %d", runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 244.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "掘削数    %d", runStats_.dugTiles);
    renderer.drawText(panel.pos + Vec2{136.0f, 282.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "入手アイテム数  %d", runStats_.acquiredItems);
    renderer.drawText(panel.pos + Vec2{136.0f, 320.0f}, buffer, {230, 230, 236, 255}, 2);

    for (int i = 0; i < GameOverItemCount; ++i) {
        drawUiButton(renderer, gameOverItemRect(i), gameOverItemName(i), i == gameOverSelection_);
    }
    if (!gameOverStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{152.0f, 474.0f}, gameOverStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::renderStageClearScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::StageClear) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {0, 0, 0, 130});
    const UiRect panel = stageClearPanelRect();
    drawUiWindow(renderer, panel, "STAGE CLEAR", "F/Enter 決定");
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "シナリオ", ui::Text, 3);

    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "深部の主を退け、落ちた星のかけらが静かに光を取り戻した。");
    renderer.drawText(panel.pos + Vec2{136.0f, 148.0f}, buffer, {230, 238, 232, 255}, 2);
    renderer.drawText(panel.pos + Vec2{136.0f, 186.0f}, "村の魔女たちは、さらに深い層へ続く道を見つける。", {230, 238, 232, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "次ステージ解禁: Stage %d", unlockedStages_);
    renderer.drawText(panel.pos + Vec2{136.0f, 238.0f}, buffer, {255, 230, 150, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "クリア時間 %s   掘削数 %d   撃破数 %d",
        formatRunTime(runStats_.elapsedSeconds).c_str(),
        runStats_.dugTiles,
        runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 292.0f}, buffer, {198, 208, 202, 255}, 2);

    for (int i = 0; i < StageClearItemCount; ++i) {
        drawUiButton(renderer, stageClearItemRect(i), stageClearItemName(i), i == stageClearSelection_);
    }
    if (!stageClearStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{152.0f, 474.0f}, stageClearStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::render(Renderer& renderer, const Time& time)
{
    renderer.clear({5, 5, 8, 255});
    if (basePresentationActive()) {
        renderBaseScreen(renderer);
        inventory_.render(renderer, player_, spellRing_);
        renderPauseMenu(renderer);
        renderRingScreen(renderer);
        renderer.present();
        return;
    }

    renderer.setWorldSpace(&camera_);

    std::vector<LightSource> itemLights;
    for (const auto& item : spellRing_.items()) {
        if (item.lightRadius > 0.0f) {
            itemLights.push_back({item.worldPosition, item.lightRadius});
        } else if (item.type == SpellRingItemType::Torch) {
            itemLights.push_back({item.worldPosition, balance_.lightRadius});
        }
    }
    if (warpPointsEnabled_) {
        for (const WarpPoint& point : warpPoints_) {
            const float radiusTiles = point.discovered ? point.discoveredLightRadiusTiles : point.undiscoveredLightRadiusTiles;
            itemLights.push_back({point.position, radiusTiles * static_cast<float>(balance::TileSize)});
        }
        if (hasBossSpawnPoint_ && !bossSpawned_) {
            itemLights.push_back({bossSpawnPoint_, 120.0f});
        }
    }
    tileMap_.render(renderer, camera_, player_.position, itemLights);
    renderRewardNodes(renderer, itemLights);
    worldDrops_.render(renderer, tileMap_, objectCatalog_, player_.position, itemLights);
    renderWarpPoints(renderer);

    const bool ringCenterVisible = tileMap_.isLit(spellRing_.center(), player_.position, itemLights);
    if (ringCenterVisible) {
        renderer.drawCircle(spellRing_.center(), spellRing_.radius(), spellRing_.state() == SpellRingState::Normal ? Color{130, 125, 160, 255} : Color{220, 185, 90, 255});
    }
    if (spellRing_.state() != SpellRingState::Normal && ringCenterVisible) {
        renderer.drawLine(player_.position, spellRing_.center(), {150, 110, 80, 100});
    }

    renderer.fillCircle(player_.position, balance_.playerRadius, {118, 72, 168, 255});
    renderer.drawLine(player_.position, player_.position + player_.facing * 22.0f, {235, 210, 255, 255});

    for (const auto& item : spellRing_.items()) {
        if (!tileMap_.isLit(item.worldPosition, player_.position, itemLights)) {
            continue;
        }
        if (item.type == SpellRingItemType::Shovel) {
            if (renderer.hasIconSheet()) {
                renderer.drawIcon(ShovelIconIndex, item.worldPosition - Vec2{IconDrawSize * 0.5f, IconDrawSize * 0.5f});
            } else {
                renderer.fillCircle(item.worldPosition, item.hitRadius, {178, 184, 190, 255});
                renderer.drawLine(item.worldPosition, item.worldPosition + normalize(item.worldPosition - spellRing_.center()) * 15.0f, {90, 96, 102, 255});
            }
        } else if (item.type == SpellRingItemType::Torch) {
            if (renderer.hasIconSheet()) {
                renderer.drawIcon(TorchIconIndex, item.worldPosition - Vec2{IconDrawSize * 0.5f, IconDrawSize * 0.5f});
            } else {
                renderer.fillCircle(item.worldPosition, item.hitRadius, {242, 122, 25, 255});
                renderer.fillCircle(item.worldPosition + Vec2{2.0f, -2.0f}, 4.0f, {255, 238, 98, 255});
            }
        } else if (item.type == SpellRingItemType::Stone) {
            renderer.fillCircle(item.worldPosition, item.hitRadius, {118, 122, 132, 255});
            renderer.drawCircle(item.worldPosition, item.hitRadius + 2.0f, {62, 64, 72, 255});
        } else {
            renderer.fillCircle(item.worldPosition, item.hitRadius, {96, 122, 210, 255});
            renderer.drawCircle(item.worldPosition, item.hitRadius + 3.0f, {160, 202, 255, 255});
        }
        if (item.hiddenDetectionRadius > 0.0f) {
            renderer.drawCircle(item.worldPosition, item.hiddenDetectionRadius, {126, 208, 255, 90});
        }
        if (item.treasureDetectionRadius > 0.0f) {
            renderer.drawCircle(item.worldPosition, item.treasureDetectionRadius, {255, 220, 92, 90});
        }
    }

    projectiles_.render(renderer, tileMap_, player_.position, itemLights);
    enemies_.render(renderer, tileMap_, player_.position, itemLights);
    effects_.render(renderer);

    renderer.setScreenSpace();
    if (reloadNoticeTimer_ > 0.0f) {
        renderer.fillRect({18.0f, 170.0f}, {430.0f, 26.0f}, {0, 0, 0, 180});
        renderer.drawText({26.0f, 176.0f}, reloadNotice_, {255, 235, 150, 255}, 2);
    }
    const int nearestWarp = nearestWarpPointIndex(player_.position);
    bool nearestWarpDiscovered = false;
    for (const WarpPoint& point : warpPoints_) {
        if (point.index == nearestWarp) {
            nearestWarpDiscovered = point.discovered;
            break;
        }
    }
    debug_.render(
        renderer,
        time,
        enemies_,
        tileMap_,
        spellRing_,
        player_,
        balance_,
        dungeonLayout_,
        currentStageDefinition(),
        nearestWarp,
        nearestWarpDiscovered,
        discoveredWarpPointCount(),
        rewardNodeCount(),
        moneyNodeCount(),
        buriedVisibleNodeCount(),
        buriedHiddenNodeCount(),
        exposedEnemyNodeCount(),
        buriedEnemyNodeCount(),
        spawnedEnemyNodeCount());
    if (debugPaused_) {
        renderer.fillRect({18.0f, 202.0f}, {190.0f, 28.0f}, {0, 0, 0, 190});
        renderer.drawText({28.0f, 208.0f}, "DEBUG PAUSED", {255, 230, 150, 255}, 2);
    }
    upgrades_.render(renderer, levels_);
    inventory_.render(renderer, player_, spellRing_);
    if (mode_ == ScreenMode::Playing) {
        inventory_.renderShortcutHud(renderer, spellRing_, camera_.width(), camera_.height());
    }
    renderPauseMenu(renderer);
    renderRingScreen(renderer);
    renderGameOverScreen(renderer);
    renderStageClearScreen(renderer);
    if (mode_ == ScreenMode::Playing || mode_ == ScreenMode::Inventory || mode_ == ScreenMode::PauseMenu || mode_ == ScreenMode::Ring) {
        encyclopedia_.renderPopups(renderer, camera_);
    }
    renderer.present();
}

}
