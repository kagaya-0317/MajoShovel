#include "devtools/autosim/AutoSimulationBaseTasks.hpp"

#include "devtools/autosim/AutoSimulationConsumablePlanner.hpp"
#include "game/ItemModel.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace majo::autosim {

namespace {

constexpr float ProtectThreshold = 86.0f;
constexpr float EnhanceThreshold = 92.0f;
constexpr int OptionalSpendBudgetDivisor = 4;
constexpr int RecoverySupplyTargetCount = 10;
constexpr int MaxRecoveryStackSlots = 3;
constexpr int MinDepartureFreeSlots = 8;
constexpr int MaxDepartureFreeSlots = 12;
constexpr int MinWarehouseFreeSlots = 2;
constexpr int MaxWarehouseFreeSlots = 6;
constexpr float CleanupDepositBaseScore = 0.0f;
constexpr float CleanupSellKeepThreshold = 64.0f;
constexpr float SevereCleanupSellKeepThreshold = 82.0f;
constexpr float CleanupUnprotectKeepThreshold = 58.0f;
constexpr float SevereCleanupUnprotectKeepThreshold = 78.0f;
constexpr float CleanupFallbackKeepThreshold = 140.0f;

bool baseBusy(const GameTestSnapshot& snapshot)
{
    return snapshot.worldLoading ||
        snapshot.transitionActive ||
        snapshot.dialogueActive ||
        snapshot.pendingStoryDelayActive ||
        snapshot.firstItemNoticeActive;
}

bool baseTasksAvailable(const GameTestSnapshot& snapshot)
{
    return snapshot.screenMode == GameTestScreenMode::Base &&
        snapshot.base.active &&
        !baseBusy(snapshot);
}

bool codexNeedsSync(const GameTestObjectEntrySnapshot& item)
{
    return static_cast<int>(item.codexStage) < static_cast<int>(GameTestCodexStage::Obtained);
}

bool canSell(const GameTestObjectEntrySnapshot& item)
{
    return item.sellable &&
        !item.important &&
        !item.equipped &&
        !item.protectionEnabled &&
        !codexNeedsSync(item);
}

bool canUnprotectForCleanup(const GameTestObjectEntrySnapshot& item)
{
    return item.kind == GameTestObjectEntryKind::Instance &&
        item.protectionEnabled &&
        item.sellable &&
        !item.important &&
        !item.equipped &&
        !codexNeedsSync(item);
}

bool backpackPressure(const GameTestInventorySnapshot& inventory)
{
    if (inventory.backpackCapacity <= 0) {
        return false;
    }
    return inventory.backpackUsedSlots >= inventory.backpackCapacity - 2 ||
        static_cast<float>(inventory.backpackUsedSlots) / static_cast<float>(inventory.backpackCapacity) >= 0.78f;
}

int backpackFreeSlots(const GameTestInventorySnapshot& inventory)
{
    if (inventory.backpackCapacity <= 0) {
        return 0;
    }
    return std::max(0, inventory.backpackCapacity - inventory.backpackUsedSlots);
}

int warehouseFreeSlots(const GameTestInventorySnapshot& inventory)
{
    if (inventory.warehouseCapacity <= 0) {
        return 0;
    }
    return std::max(0, inventory.warehouseCapacity - inventory.warehouseUsedSlots);
}

int desiredFreeSlotsForCapacity(int capacity)
{
    if (capacity <= 0) {
        return 0;
    }
    return std::min(
        std::min(MaxDepartureFreeSlots, capacity),
        std::max(MinDepartureFreeSlots, capacity / 3));
}

int desiredWarehouseFreeSlotsForCapacity(int capacity)
{
    if (capacity <= 0) {
        return 0;
    }
    return std::min(
        std::min(MaxWarehouseFreeSlots, capacity),
        std::max(MinWarehouseFreeSlots, capacity / 20));
}

bool warehouseHasRoomFor(const GameTestInventorySnapshot& inventory, const GameTestObjectEntrySnapshot& item)
{
    if (item.kind == GameTestObjectEntryKind::Stack) {
        return std::any_of(inventory.warehouseItems.begin(), inventory.warehouseItems.end(), [&item](const GameTestObjectEntrySnapshot& stored) {
            return stored.kind == GameTestObjectEntryKind::Stack &&
                stored.objectId == item.objectId &&
                stored.count < ObjectStackMaxCount;
        }) || inventory.warehouseUsedSlots < inventory.warehouseCapacity;
    }
    return inventory.warehouseUsedSlots < inventory.warehouseCapacity;
}

bool recoverySupplyItem(const GameTestObjectEntrySnapshot& item)
{
    if (item.kind != GameTestObjectEntryKind::Stack ||
        item.objectId.empty() ||
        item.category != "回復" ||
        item.broken ||
        item.important) {
        return false;
    }
    const AutoSimulationConsumableProfile profile = autoSimulationConsumableProfile(item);
    return profile.heal > 0.0 && !profile.unsafeSelfEffect;
}

int preferredRecoveryPriority(std::string_view objectId)
{
    constexpr std::array<std::string_view, 3> PreferredRecoveryIds{
        "item_luxury_apple",
        "item_good_apple",
        "item_apple",
    };
    const auto it = std::find(PreferredRecoveryIds.begin(), PreferredRecoveryIds.end(), objectId);
    return it == PreferredRecoveryIds.end()
        ? 0
        : static_cast<int>(PreferredRecoveryIds.end() - it);
}

struct RecoverySupplyCandidate {
    std::string objectId;
    int backpackCount = 0;
    int warehouseCount = 0;
    int merchantCount = 0;
    double heal = 0.0;
};

std::vector<std::string> selectedRecoveryObjectIds(const GameTestSnapshot& snapshot)
{
    std::vector<RecoverySupplyCandidate> candidates;
    const auto add = [&candidates](const GameTestObjectEntrySnapshot& item, int backpack, int warehouse, int merchant) {
        if (!recoverySupplyItem(item)) {
            return;
        }
        auto it = std::find_if(candidates.begin(), candidates.end(), [&item](const RecoverySupplyCandidate& candidate) {
            return candidate.objectId == item.objectId;
        });
        if (it == candidates.end()) {
            candidates.push_back(RecoverySupplyCandidate{
                .objectId = item.objectId,
                .backpackCount = backpack,
                .warehouseCount = warehouse,
                .merchantCount = merchant,
                .heal = autoSimulationConsumableProfile(item).heal,
            });
            return;
        }
        it->backpackCount += backpack;
        it->warehouseCount += warehouse;
        it->merchantCount += merchant;
        it->heal = std::max(it->heal, autoSimulationConsumableProfile(item).heal);
    };

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        add(item, std::max(0, item.count), 0, 0);
    }
    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.warehouseItems) {
        add(item, 0, std::max(0, item.count), 0);
    }
    if (snapshot.base.merchantStockPrepared) {
        for (const GameTestMerchantProductSnapshot& product : snapshot.base.merchantProducts) {
            add(product.item, 0, 0, std::max(0, product.stockCount));
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const RecoverySupplyCandidate& left, const RecoverySupplyCandidate& right) {
        const bool leftCarried = left.backpackCount > 0;
        const bool rightCarried = right.backpackCount > 0;
        if (leftCarried != rightCarried) {
            return leftCarried;
        }
        const int leftPreferred = preferredRecoveryPriority(left.objectId);
        const int rightPreferred = preferredRecoveryPriority(right.objectId);
        if (leftPreferred != rightPreferred) {
            return leftPreferred > rightPreferred;
        }
        if (left.warehouseCount != right.warehouseCount) {
            return left.warehouseCount > right.warehouseCount;
        }
        if (left.heal != right.heal) {
            return left.heal > right.heal;
        }
        return left.objectId < right.objectId;
    });

    std::vector<std::string> selected;
    const int count = std::min(MaxRecoveryStackSlots, static_cast<int>(candidates.size()));
    selected.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        selected.push_back(candidates[static_cast<std::size_t>(i)].objectId);
    }
    return selected;
}

bool recoveryObjectSelected(const std::vector<std::string>& selected, std::string_view objectId)
{
    return std::find(selected.begin(), selected.end(), objectId) != selected.end();
}

const GameTestObjectEntrySnapshot* recoveryStack(
    const std::vector<GameTestObjectEntrySnapshot>& items,
    std::string_view objectId)
{
    const auto it = std::max_element(items.begin(), items.end(), [objectId](
        const GameTestObjectEntrySnapshot& left,
        const GameTestObjectEntrySnapshot& right) {
        const int leftCount = left.kind == GameTestObjectEntryKind::Stack && left.objectId == objectId
            ? std::max(0, left.count)
            : -1;
        const int rightCount = right.kind == GameTestObjectEntryKind::Stack && right.objectId == objectId
            ? std::max(0, right.count)
            : -1;
        return leftCount < rightCount;
    });
    if (it == items.end() || it->kind != GameTestObjectEntryKind::Stack || it->objectId != objectId) {
        return nullptr;
    }
    return &*it;
}

bool recoveryStackReserved(
    const GameTestObjectEntrySnapshot& item,
    const std::vector<std::string>& selected,
    const GameTestInventorySnapshot& inventory)
{
    if (!recoveryObjectSelected(selected, item.objectId)) {
        return false;
    }
    const GameTestObjectEntrySnapshot* keeper = recoveryStack(inventory.backpackItems, item.objectId);
    return keeper != nullptr && keeper->stackRuntimeId == item.stackRuntimeId;
}

int safeMoneyProduct(int unitPrice, int count)
{
    if (unitPrice <= 0 || count <= 0) {
        return 0;
    }
    return count > std::numeric_limits<int>::max() / unitPrice
        ? std::numeric_limits<int>::max()
        : unitPrice * count;
}

float equippedStaffScore(const GameTestInventorySnapshot& inventory)
{
    float score = 0.0f;
    for (const GameTestObjectEntrySnapshot& item : inventory.backpackItems) {
        if (item.equipped && item.category == "杖") {
            score = std::max(score, item.staffEquipScore);
        }
    }
    return score;
}

bool reservedForStaffLoadout(const GameTestObjectEntrySnapshot& item, float currentStaffScore)
{
    return item.location == GameTestInventoryLocation::Backpack &&
        item.category == "杖" &&
        !item.equipped &&
        !item.broken &&
        item.staffEquipScore > currentStaffScore + 8.0f;
}

bool reservedForGearLoadout(
    const GameTestObjectEntrySnapshot& item,
    const AutoSimulationItemScore& score,
    float currentStaffScore)
{
    if (reservedForStaffLoadout(item, currentStaffScore)) {
        return true;
    }
    if (item.location != GameTestInventoryLocation::Backpack || item.equipped || item.broken) {
        return false;
    }
    if (score.combat <= 0.0f && score.dig <= 0.0f && score.light <= 0.0f) {
        return false;
    }
    return item.protectionEnabled ||
        score.investment >= 96.0f ||
        score.loadout >= 64.0f ||
        score.protect >= 100.0f;
}

float cleanupDepositPriority(
    const GameTestObjectEntrySnapshot& item,
    const AutoSimulationItemScore& score,
    bool loadoutReserved)
{
    float priority = CleanupDepositBaseScore;
    priority += std::min(score.keep, 120.0f) * 0.18f;
    priority += std::max(0.0f, 90.0f - score.loadout) * 0.28f;
    if (item.protectionEnabled) {
        priority += 34.0f;
    }
    if (item.important) {
        priority += 26.0f;
    }
    if (item.kind == GameTestObjectEntryKind::Stack && item.count > 1) {
        priority += 8.0f;
    }
    if (loadoutReserved && !item.protectionEnabled) {
        priority -= 18.0f;
    }
    return priority;
}

GameTestAction itemAction(
    GameTestActionKind kind,
    const GameTestObjectEntrySnapshot& item,
    int count,
    std::string reason,
    int estimatedMoneyCost = 0)
{
    GameTestAction action;
    action.kind = kind;
    action.stackRuntimeId = item.stackRuntimeId;
    action.objectId = item.objectId;
    action.instanceId = item.instanceId;
    action.count = count;
    action.estimatedMoneyCost = std::max(0, estimatedMoneyCost);
    action.reason = std::move(reason);
    return action;
}

GameTestActionKind sellKindForItem(const GameTestObjectEntrySnapshot& item)
{
    if (item.location == GameTestInventoryLocation::Warehouse) {
        return item.kind == GameTestObjectEntryKind::Stack
            ? GameTestActionKind::SellWarehouseStack
            : GameTestActionKind::SellWarehouseInstance;
    }
    return item.kind == GameTestObjectEntryKind::Stack
        ? GameTestActionKind::SellBackpackStack
        : GameTestActionKind::SellBackpackInstance;
}

GameTestActionKind unprotectKindForItem(const GameTestObjectEntrySnapshot& item)
{
    return item.location == GameTestInventoryLocation::Warehouse
        ? GameTestActionKind::UnprotectWarehouseInstance
        : GameTestActionKind::UnprotectBackpackInstance;
}

GameTestAction ringItemAction(
    GameTestActionKind kind,
    const GameTestRingItemSnapshot& item,
    std::string reason,
    int estimatedMoneyCost = 0)
{
    GameTestAction action;
    action.kind = kind;
    action.objectId = item.objectId;
    action.instanceId = item.instanceId;
    action.ringIndex = item.ringIndex;
    action.ringItemIndex = item.itemIndex;
    action.estimatedMoneyCost = std::max(0, estimatedMoneyCost);
    action.reason = std::move(reason);
    return action;
}

bool isItemEnhancementAction(GameTestActionKind kind)
{
    switch (kind) {
    case GameTestActionKind::EnhanceBackpackStackAttack:
    case GameTestActionKind::EnhanceBackpackStackDig:
    case GameTestActionKind::EnhanceBackpackInstanceAttack:
    case GameTestActionKind::EnhanceBackpackInstanceDig:
    case GameTestActionKind::EnhanceRingItemAttack:
    case GameTestActionKind::EnhanceRingItemDig:
        return true;
    default:
        return false;
    }
}

bool isOptionalSpendAction(GameTestActionKind kind)
{
    return isItemEnhancementAction(kind) || kind == GameTestActionKind::BuyMerchantProduct;
}

int enhancementMoneyCost(
    const GameTestObjectEntrySnapshot& item,
    GameTestActionKind kind)
{
    switch (kind) {
    case GameTestActionKind::EnhanceBackpackStackAttack:
    case GameTestActionKind::EnhanceBackpackInstanceAttack:
        return item.enhanceAttackMoneyCost;
    case GameTestActionKind::EnhanceBackpackStackDig:
    case GameTestActionKind::EnhanceBackpackInstanceDig:
        return item.enhanceDigMoneyCost;
    default:
        return 0;
    }
}

int enhancementMoneyCost(
    const GameTestRingItemSnapshot& item,
    GameTestActionKind kind)
{
    switch (kind) {
    case GameTestActionKind::EnhanceRingItemAttack:
        return item.enhanceAttackMoneyCost;
    case GameTestActionKind::EnhanceRingItemDig:
        return item.enhanceDigMoneyCost;
    default:
        return 0;
    }
}

std::optional<GameTestActionKind> backpackEnhanceKind(
    const GameTestObjectEntrySnapshot& item,
    const AutoSimulationItemScore& score)
{
    const GameTestActionKind attackKind = item.kind == GameTestObjectEntryKind::Stack
        ? GameTestActionKind::EnhanceBackpackStackAttack
        : GameTestActionKind::EnhanceBackpackInstanceAttack;
    const GameTestActionKind digKind = item.kind == GameTestObjectEntryKind::Stack
        ? GameTestActionKind::EnhanceBackpackStackDig
        : GameTestActionKind::EnhanceBackpackInstanceDig;

    if (score.preferAttackEnhance && item.canEnhanceAttack) {
        return attackKind;
    }
    if (score.preferDigEnhance && item.canEnhanceDig) {
        return digKind;
    }
    if (item.canEnhanceAttack && score.combat >= score.dig) {
        return attackKind;
    }
    if (item.canEnhanceDig) {
        return digKind;
    }
    if (item.canEnhanceAttack) {
        return attackKind;
    }
    return std::nullopt;
}

std::optional<GameTestActionKind> ringEnhanceKind(
    const GameTestRingItemSnapshot& item,
    const AutoSimulationItemScore& score)
{
    if (score.preferAttackEnhance && item.canEnhanceAttack) {
        return GameTestActionKind::EnhanceRingItemAttack;
    }
    if (score.preferDigEnhance && item.canEnhanceDig) {
        return GameTestActionKind::EnhanceRingItemDig;
    }
    if (item.canEnhanceAttack && score.combat >= score.dig) {
        return GameTestActionKind::EnhanceRingItemAttack;
    }
    if (item.canEnhanceDig) {
        return GameTestActionKind::EnhanceRingItemDig;
    }
    if (item.canEnhanceAttack) {
        return GameTestActionKind::EnhanceRingItemAttack;
    }
    return std::nullopt;
}

std::optional<GameTestAction> chooseCodexAction(const GameTestSnapshot& snapshot)
{
    if (!snapshot.base.encyclopediaSyncAvailable) {
        return std::nullopt;
    }
    GameTestAction action;
    action.kind = GameTestActionKind::SyncEncyclopedia;
    action.reason = "codex_obtained_sync";
    return action;
}

std::optional<GameTestAction> chooseRecoverySupplyAction(
    const GameTestSnapshot& snapshot,
    int optionalSpendBudgetRemaining)
{
    const std::vector<std::string> selected = selectedRecoveryObjectIds(snapshot);

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        if (!recoverySupplyItem(item)) {
            continue;
        }
        const GameTestObjectEntrySnapshot* keeper = recoveryStack(
            snapshot.inventory.backpackItems,
            item.objectId);
        const bool selectedKeeper = recoveryObjectSelected(selected, item.objectId) &&
            keeper != nullptr &&
            keeper->stackRuntimeId == item.stackRuntimeId;
        if (!selectedKeeper) {
            if (warehouseHasRoomFor(snapshot.inventory, item)) {
                return itemAction(
                    GameTestActionKind::DepositBackpackStack,
                    item,
                    std::max(1, item.count),
                    "recovery_stack_slot_limit");
            }
            continue;
        }
    }

    for (const std::string& objectId : selected) {
        const GameTestObjectEntrySnapshot* backpack = recoveryStack(snapshot.inventory.backpackItems, objectId);
        const GameTestObjectEntrySnapshot* warehouse = recoveryStack(snapshot.inventory.warehouseItems, objectId);
        const int backpackCount = backpack == nullptr ? 0 : std::max(0, backpack->count);
        const int warehouseCount = warehouse == nullptr ? 0 : std::max(0, warehouse->count);
        const int moveCount = std::min(warehouseCount, std::max(0, ObjectStackMaxCount - backpackCount));
        if (warehouse != nullptr && moveCount > 0) {
            return itemAction(
                GameTestActionKind::WithdrawWarehouseStack,
                *warehouse,
                moveCount,
                "carry_recovery_from_warehouse");
        }
    }

    int carriedRecoveryCount = 0;
    for (const std::string& objectId : selected) {
        if (const GameTestObjectEntrySnapshot* item = recoveryStack(snapshot.inventory.backpackItems, objectId)) {
            carriedRecoveryCount += std::min(ObjectStackMaxCount, std::max(0, item->count));
        }
    }
    if (carriedRecoveryCount >= RecoverySupplyTargetCount) {
        return std::nullopt;
    }
    if (!snapshot.base.merchantStockPrepared) {
        GameTestAction action;
        action.kind = GameTestActionKind::PrepareMerchantStock;
        action.reason = "recovery_supply_shortage";
        return action;
    }

    const int shortage = RecoverySupplyTargetCount - carriedRecoveryCount;
    const GameTestMerchantProductSnapshot* bestProduct = nullptr;
    int bestCount = 0;
    for (const GameTestMerchantProductSnapshot& product : snapshot.base.merchantProducts) {
        if (!recoveryObjectSelected(selected, product.item.objectId) ||
            !recoverySupplyItem(product.item) ||
            product.unitPrice <= 0) {
            continue;
        }
        const GameTestObjectEntrySnapshot* backpack =
            recoveryStack(snapshot.inventory.backpackItems, product.item.objectId);
        const int backpackCount = backpack == nullptr ? 0 : std::max(0, backpack->count);
        const int budgetCount = optionalSpendBudgetRemaining / product.unitPrice;
        const int purchaseCount = std::min({
            shortage,
            std::max(0, product.stockCount),
            std::max(0, product.purchasableCount),
            std::max(0, ObjectStackMaxCount - backpackCount),
            std::max(0, budgetCount),
        });
        if (purchaseCount <= 0) {
            continue;
        }
        const bool productMerges = backpackCount > 0;
        const bool bestMerges = bestProduct != nullptr &&
            recoveryStack(snapshot.inventory.backpackItems, bestProduct->item.objectId) != nullptr;
        if (bestProduct == nullptr ||
            (productMerges != bestMerges && productMerges) ||
            (productMerges == bestMerges && product.unitPrice < bestProduct->unitPrice) ||
            (productMerges == bestMerges && product.unitPrice == bestProduct->unitPrice && purchaseCount > bestCount)) {
            bestProduct = &product;
            bestCount = purchaseCount;
        }
    }
    if (bestProduct == nullptr) {
        return std::nullopt;
    }

    GameTestAction action;
    action.kind = GameTestActionKind::BuyMerchantProduct;
    action.objectId = bestProduct->item.objectId;
    action.merchantProductIndex = bestProduct->index;
    action.count = bestCount;
    action.estimatedMoneyCost = safeMoneyProduct(bestProduct->unitPrice, bestCount);
    action.reason = "buy_recovery_to_target";
    return action;
}

float upgradePriority(const GameTestSnapshot& snapshot, const GameTestUpgradeSnapshot& upgrade)
{
    if (!upgrade.affordable) {
        return -1.0f;
    }
    const bool warehouseTight =
        snapshot.inventory.warehouseCapacity > 0 &&
        snapshot.inventory.warehouseUsedSlots >= snapshot.inventory.warehouseCapacity - 2;
    switch (upgrade.index) {
    case 0: return warehouseTight ? 120.0f : 42.0f - static_cast<float>(upgrade.level) * 2.0f;
    case 7: return 95.0f - static_cast<float>(upgrade.level) * 5.0f;
    case 4: return 86.0f - static_cast<float>(upgrade.level) * 4.0f;
    case 5: return 82.0f - static_cast<float>(upgrade.level) * 4.0f;
    case 6: return 80.0f - static_cast<float>(upgrade.level) * 4.0f;
    case 1: return 70.0f - static_cast<float>(upgrade.level) * 3.0f;
    case 2: return 64.0f - static_cast<float>(upgrade.level) * 3.0f;
    case 3: return 58.0f;
    default: return -1.0f;
    }
}

} // namespace

int AutoSimulationBaseTasks::desiredBackpackFreeSlots(const GameTestInventorySnapshot& inventory)
{
    return desiredFreeSlotsForCapacity(inventory.backpackCapacity);
}

bool AutoSimulationBaseTasks::backpackReadyForDeparture(const GameTestInventorySnapshot& inventory)
{
    if (inventory.backpackCapacity <= 0) {
        return true;
    }
    return backpackFreeSlots(inventory) >= desiredBackpackFreeSlots(inventory);
}

bool AutoSimulationBaseTasks::backpackCanDepart(const GameTestInventorySnapshot& inventory)
{
    return inventory.backpackCapacity <= 0 || backpackFreeSlots(inventory) > 0;
}

int AutoSimulationBaseTasks::optionalSpendBudgetRemaining() const
{
    return std::max(0, optionalSpendBudgetLimit_ - optionalSpendBudgetSpent_);
}

void AutoSimulationBaseTasks::observeMoney(int money) const
{
    const int candidateLimit = std::max(0, money) / OptionalSpendBudgetDivisor;
    optionalSpendBudgetLimit_ = std::max(optionalSpendBudgetLimit_, candidateLimit);
}

void AutoSimulationBaseTasks::recordActionResult(
    const GameTestAction& action,
    const GameTestActionResult& result)
{
    if (!result.applied || !isOptionalSpendAction(action.kind)) {
        return;
    }
    const int cost = std::max(0, action.estimatedMoneyCost);
    optionalSpendBudgetSpent_ = cost > std::numeric_limits<int>::max() - optionalSpendBudgetSpent_
        ? std::numeric_limits<int>::max()
        : optionalSpendBudgetSpent_ + cost;
}

std::optional<GameTestAction> AutoSimulationBaseTasks::choosePreparationAction(const GameTestSnapshot& snapshot) const
{
    if (!baseTasksAvailable(snapshot)) {
        return std::nullopt;
    }
    observeMoney(snapshot.base.money);

    if (std::optional<GameTestAction> action = chooseCodexAction(snapshot)) {
        return action;
    }
    if (!backpackReadyForDeparture(snapshot.inventory)) {
        return std::nullopt;
    }
    if (std::optional<GameTestAction> action = chooseRecoverySupplyAction(
            snapshot,
            optionalSpendBudgetRemaining())) {
        return action;
    }
    return chooseCheckpointPrepAction(snapshot);
}

std::optional<GameTestAction> AutoSimulationBaseTasks::chooseCheckpointPrepAction(const GameTestSnapshot& snapshot) const
{
    if (!baseTasksAvailable(snapshot)) {
        return std::nullopt;
    }

    if (snapshot.base.bulkRepairExecutable &&
        snapshot.base.bulkRepairTargetCount > 0 &&
        snapshot.base.bulkRepairMoneyCost == 0 &&
        snapshot.base.bulkRepairOreCost == 0) {
        GameTestAction action;
        action.kind = GameTestActionKind::BulkRepairAtBase;
        action.count = snapshot.base.bulkRepairTargetCount;
        action.reason = "free_bulk_repair";
        return action;
    }

    std::optional<GameTestAction> bestProtect;
    std::optional<GameTestAction> bestEnhance;
    float bestProtectScore = ProtectThreshold;
    float bestEnhanceScore = EnhanceThreshold;
    const int remainingEnhancementBudget = optionalSpendBudgetRemaining();
    const AutoSimulationItemEvaluationContext itemContext =
        autoSimulationItemEvaluationContextForSnapshot(snapshot);

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(item, itemContext);
        if (item.kind == GameTestObjectEntryKind::Instance &&
            !item.protectionEnabled &&
            !item.equipped &&
            score.protect > bestProtectScore) {
            bestProtect = itemAction(
                GameTestActionKind::ProtectBackpackInstance,
                item,
                1,
                "protect " + score.reason);
            bestProtectScore = score.protect;
        }
        if (const std::optional<GameTestActionKind> kind = backpackEnhanceKind(item, score);
            kind &&
            !item.broken &&
            enhancementMoneyCost(item, *kind) <= remainingEnhancementBudget &&
            score.enhance > bestEnhanceScore) {
            bestEnhance = itemAction(
                *kind,
                item,
                1,
                "enhance " + score.reason,
                enhancementMoneyCost(item, *kind));
            bestEnhanceScore = score.enhance;
        }
    }

    for (const GameTestRingItemSnapshot& item : snapshot.ring.items) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(item, itemContext);
        if (const std::optional<GameTestActionKind> kind = ringEnhanceKind(item, score);
            kind &&
            !item.broken &&
            enhancementMoneyCost(item, *kind) <= remainingEnhancementBudget &&
            score.enhance > bestEnhanceScore) {
            bestEnhance = ringItemAction(
                *kind,
                item,
                "enhance_ring " + score.reason,
                enhancementMoneyCost(item, *kind));
            bestEnhanceScore = score.enhance;
        }
    }

    if (bestProtect) {
        return bestProtect;
    }
    if (bestEnhance) {
        return bestEnhance;
    }
    return std::nullopt;
}

std::optional<GameTestAction> AutoSimulationBaseTasks::chooseAction(const GameTestSnapshot& snapshot) const
{
    if (!baseTasksAvailable(snapshot)) {
        return std::nullopt;
    }

    if (std::optional<GameTestAction> preparationAction = choosePreparationAction(snapshot)) {
        return preparationAction;
    }

    const bool pressure = backpackPressure(snapshot.inventory);
    const int freeSlots = backpackFreeSlots(snapshot.inventory);
    const int desiredFreeSlots = desiredBackpackFreeSlots(snapshot.inventory);
    const bool backpackCleanup = freeSlots < desiredFreeSlots;
    const bool severeBackpackCleanup = backpackCleanup && freeSlots <= std::max(1, desiredFreeSlots / 2);
    const int warehouseFree = warehouseFreeSlots(snapshot.inventory);
    const int desiredWarehouseFree = desiredWarehouseFreeSlotsForCapacity(snapshot.inventory.warehouseCapacity);
    const bool warehouseCleanup = desiredWarehouseFree > 0 && warehouseFree < desiredWarehouseFree;
    const bool severeWarehouseCleanup = warehouseCleanup && warehouseFree <= 0;
    const bool cleanup = backpackCleanup || warehouseCleanup;
    const float currentStaffScore = equippedStaffScore(snapshot.inventory);
    const GameTestObjectEntrySnapshot* bestDeposit = nullptr;
    const GameTestObjectEntrySnapshot* bestSell = nullptr;
    const GameTestObjectEntrySnapshot* bestUnprotect = nullptr;
    const GameTestObjectEntrySnapshot* fallbackSell = nullptr;
    const GameTestObjectEntrySnapshot* fallbackUnprotect = nullptr;
    float bestDepositScore = backpackCleanup ? -std::numeric_limits<float>::max() : (pressure ? 48.0f : 72.0f);
    float bestSellScore = std::numeric_limits<float>::max();
    float bestUnprotectScore = std::numeric_limits<float>::max();
    float fallbackSellScore = std::numeric_limits<float>::max();
    float fallbackUnprotectScore = std::numeric_limits<float>::max();
    const AutoSimulationItemEvaluationContext itemContext =
        autoSimulationItemEvaluationContextForSnapshot(snapshot);
    const std::vector<std::string> selectedRecovery = selectedRecoveryObjectIds(snapshot);

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(item, itemContext);
        const bool loadoutReserved = reservedForGearLoadout(item, score, currentStaffScore);
        const bool recoveryReserved = recoveryStackReserved(item, selectedRecovery, snapshot.inventory);
        const bool depositReserved = recoveryReserved ||
            (loadoutReserved && !item.protectionEnabled && !backpackCleanup);
        const bool sellReserved = recoveryReserved || (loadoutReserved && !backpackCleanup);
        if (warehouseHasRoomFor(snapshot.inventory, item) &&
            !depositReserved &&
            !item.equipped &&
            (backpackCleanup || item.important || item.protectionEnabled || score.store > bestDepositScore)) {
            const float depositScore = backpackCleanup
                ? cleanupDepositPriority(item, score, loadoutReserved)
                : score.store;
            if (depositScore <= bestDepositScore) {
                continue;
            }
            bestDeposit = &item;
            bestDepositScore = depositScore;
        }
        const float sellLimit = backpackCleanup
            ? (severeBackpackCleanup ? SevereCleanupSellKeepThreshold : CleanupSellKeepThreshold)
            : (pressure ? 52.0f : 34.0f);
        if (!sellReserved && canSell(item) && score.keep < sellLimit && score.keep < bestSellScore) {
            bestSell = &item;
            bestSellScore = score.keep;
        }
        if (backpackCleanup && !sellReserved && canSell(item) && score.keep < fallbackSellScore) {
            fallbackSell = &item;
            fallbackSellScore = score.keep;
        }
        if (backpackCleanup && canUnprotectForCleanup(item)) {
            const float unprotectLimit = severeBackpackCleanup
                ? SevereCleanupUnprotectKeepThreshold
                : CleanupUnprotectKeepThreshold;
            if (score.keep < unprotectLimit && score.keep < bestUnprotectScore) {
                bestUnprotect = &item;
                bestUnprotectScore = score.keep;
            }
            if (score.keep < fallbackUnprotectScore) {
                fallbackUnprotect = &item;
                fallbackUnprotectScore = score.keep;
            }
        }
    }

    if (warehouseCleanup) {
        const float sellLimit = severeWarehouseCleanup
            ? SevereCleanupSellKeepThreshold
            : CleanupSellKeepThreshold;
        const float unprotectLimit = severeWarehouseCleanup
            ? SevereCleanupUnprotectKeepThreshold
            : CleanupUnprotectKeepThreshold;
        for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.warehouseItems) {
            const AutoSimulationItemScore score = itemEvaluator_.evaluate(item, itemContext);
            const bool recoveryReserved = recoverySupplyItem(item);
            if (!recoveryReserved && canSell(item) && score.keep < sellLimit && score.keep < bestSellScore) {
                bestSell = &item;
                bestSellScore = score.keep;
            }
            if (!recoveryReserved && canSell(item) && score.keep < fallbackSellScore) {
                fallbackSell = &item;
                fallbackSellScore = score.keep;
            }
            if (canUnprotectForCleanup(item)) {
                if (score.keep < unprotectLimit && score.keep < bestUnprotectScore) {
                    bestUnprotect = &item;
                    bestUnprotectScore = score.keep;
                }
                if (score.keep < fallbackUnprotectScore) {
                    fallbackUnprotect = &item;
                    fallbackUnprotectScore = score.keep;
                }
            }
        }
    }

    const bool backpackCapacityDeadlock = backpackCleanup && freeSlots <= 0;
    if (cleanup &&
        bestSell == nullptr &&
        fallbackSell != nullptr &&
        (fallbackSellScore <= CleanupFallbackKeepThreshold || backpackCapacityDeadlock)) {
        bestSell = fallbackSell;
    }
    if (cleanup &&
        bestUnprotect == nullptr &&
        fallbackUnprotect != nullptr &&
        (fallbackUnprotectScore <= CleanupFallbackKeepThreshold ||
            (bestDeposit == nullptr && bestSell == nullptr))) {
        bestUnprotect = fallbackUnprotect;
    }

    if (!backpackCleanup &&
        warehouseCleanup &&
        bestSell != nullptr &&
        bestSell->location == GameTestInventoryLocation::Warehouse) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestSell, itemContext);
        return itemAction(sellKindForItem(*bestSell), *bestSell, std::max(1, bestSell->count), "sell " + score.reason);
    }

    if (!backpackCleanup &&
        warehouseCleanup &&
        bestUnprotect != nullptr &&
        bestUnprotect->location == GameTestInventoryLocation::Warehouse) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestUnprotect, itemContext);
        return itemAction(
            unprotectKindForItem(*bestUnprotect),
            *bestUnprotect,
            1,
            "unprotect_for_cleanup " + score.reason);
    }

    if (bestDeposit != nullptr) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestDeposit, itemContext);
        const GameTestActionKind kind = bestDeposit->kind == GameTestObjectEntryKind::Stack
            ? GameTestActionKind::DepositBackpackStack
            : GameTestActionKind::DepositBackpackInstance;
        return itemAction(kind, *bestDeposit, std::max(1, bestDeposit->count), "store " + score.reason);
    }

    if (bestSell != nullptr) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestSell, itemContext);
        return itemAction(sellKindForItem(*bestSell), *bestSell, std::max(1, bestSell->count), "sell " + score.reason);
    }

    if (bestUnprotect != nullptr) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestUnprotect, itemContext);
        return itemAction(
            unprotectKindForItem(*bestUnprotect),
            *bestUnprotect,
            1,
            "unprotect_for_cleanup " + score.reason);
    }

    if (backpackCleanup) {
        return std::nullopt;
    }

    const GameTestUpgradeSnapshot* bestUpgrade = nullptr;
    float bestUpgradeScore = -1.0f;
    for (const GameTestUpgradeSnapshot& upgrade : snapshot.base.upgrades) {
        const float score = upgradePriority(snapshot, upgrade);
        if (score > bestUpgradeScore) {
            bestUpgrade = &upgrade;
            bestUpgradeScore = score;
        }
    }
    if (bestUpgrade != nullptr) {
        GameTestAction action;
        action.kind = GameTestActionKind::BuyBaseUpgrade;
        action.upgradeIndex = bestUpgrade->index;
        action.reason = "upgrade " + bestUpgrade->name;
        return action;
    }

    return std::nullopt;
}

} // namespace majo::autosim
