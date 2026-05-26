#include "devtools/autosim/AutoSimulationBaseTasks.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace majo::autosim {

namespace {

constexpr float ProtectThreshold = 86.0f;
constexpr float RepairThreshold = 72.0f;
constexpr float EnhanceThreshold = 92.0f;
constexpr int MinDepartureFreeSlots = 8;
constexpr int MaxDepartureFreeSlots = 12;
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

int desiredFreeSlotsForCapacity(int capacity)
{
    if (capacity <= 0) {
        return 0;
    }
    return std::min(
        std::min(MaxDepartureFreeSlots, capacity),
        std::max(MinDepartureFreeSlots, capacity / 3));
}

bool warehouseHasRoomFor(const GameTestInventorySnapshot& inventory, const GameTestObjectEntrySnapshot& item)
{
    if (item.kind == GameTestObjectEntryKind::Stack) {
        return std::any_of(inventory.warehouseItems.begin(), inventory.warehouseItems.end(), [&item](const GameTestObjectEntrySnapshot& stored) {
            return stored.kind == GameTestObjectEntryKind::Stack && stored.objectId == item.objectId;
        }) || inventory.warehouseUsedSlots < inventory.warehouseCapacity;
    }
    return inventory.warehouseUsedSlots < inventory.warehouseCapacity;
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
    std::string reason)
{
    GameTestAction action;
    action.kind = kind;
    action.objectId = item.objectId;
    action.instanceId = item.instanceId;
    action.count = count;
    action.reason = std::move(reason);
    return action;
}

GameTestAction ringItemAction(
    GameTestActionKind kind,
    const GameTestRingItemSnapshot& item,
    std::string reason)
{
    GameTestAction action;
    action.kind = kind;
    action.objectId = item.objectId;
    action.instanceId = item.instanceId;
    action.ringIndex = item.ringIndex;
    action.ringItemIndex = item.itemIndex;
    action.reason = std::move(reason);
    return action;
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
    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        if (codexNeedsSync(item)) {
            GameTestAction action;
            action.kind = GameTestActionKind::SyncEncyclopedia;
            action.reason = "codex_obtained_sync";
            return action;
        }
    }
    return std::nullopt;
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

std::optional<GameTestAction> AutoSimulationBaseTasks::choosePreparationAction(const GameTestSnapshot& snapshot) const
{
    if (!baseTasksAvailable(snapshot)) {
        return std::nullopt;
    }

    if (std::optional<GameTestAction> action = chooseCodexAction(snapshot)) {
        return action;
    }
    if (!backpackReadyForDeparture(snapshot.inventory)) {
        return std::nullopt;
    }
    return chooseCheckpointPrepAction(snapshot);
}

std::optional<GameTestAction> AutoSimulationBaseTasks::chooseCheckpointPrepAction(const GameTestSnapshot& snapshot) const
{
    if (!baseTasksAvailable(snapshot)) {
        return std::nullopt;
    }

    std::optional<GameTestAction> bestProtect;
    std::optional<GameTestAction> bestRepair;
    std::optional<GameTestAction> bestEnhance;
    float bestProtectScore = ProtectThreshold;
    float bestRepairScore = RepairThreshold;
    float bestEnhanceScore = EnhanceThreshold;
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
        if (item.kind == GameTestObjectEntryKind::Instance &&
            item.broken &&
            item.canRepair &&
            score.keep > bestRepairScore) {
            bestRepair = itemAction(
                GameTestActionKind::RepairBackpackInstance,
                item,
                1,
                "repair " + score.reason);
            bestRepairScore = score.keep;
        }
        if (const std::optional<GameTestActionKind> kind = backpackEnhanceKind(item, score);
            kind && !item.broken && score.enhance > bestEnhanceScore) {
            bestEnhance = itemAction(*kind, item, 1, "enhance " + score.reason);
            bestEnhanceScore = score.enhance;
        }
    }

    for (const GameTestRingItemSnapshot& item : snapshot.ring.items) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(item, itemContext);
        if (item.broken && item.canRepair && score.keep > bestRepairScore) {
            bestRepair = ringItemAction(
                GameTestActionKind::RepairRingItem,
                item,
                "repair_ring " + score.reason);
            bestRepairScore = score.keep;
        }
        if (const std::optional<GameTestActionKind> kind = ringEnhanceKind(item, score);
            kind && !item.broken && score.enhance > bestEnhanceScore) {
            bestEnhance = ringItemAction(*kind, item, "enhance_ring " + score.reason);
            bestEnhanceScore = score.enhance;
        }
    }

    if (bestProtect) {
        return bestProtect;
    }
    if (bestRepair) {
        return bestRepair;
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
    const bool cleanup = freeSlots < desiredFreeSlots;
    const bool severeCleanup = cleanup && freeSlots <= std::max(1, desiredFreeSlots / 2);
    const float currentStaffScore = equippedStaffScore(snapshot.inventory);
    const GameTestObjectEntrySnapshot* bestDeposit = nullptr;
    const GameTestObjectEntrySnapshot* bestSell = nullptr;
    const GameTestObjectEntrySnapshot* bestUnprotect = nullptr;
    const GameTestObjectEntrySnapshot* fallbackSell = nullptr;
    const GameTestObjectEntrySnapshot* fallbackUnprotect = nullptr;
    float bestDepositScore = cleanup ? -std::numeric_limits<float>::max() : (pressure ? 48.0f : 72.0f);
    float bestSellScore = cleanup
        ? (severeCleanup ? SevereCleanupSellKeepThreshold : CleanupSellKeepThreshold)
        : (pressure ? 52.0f : 34.0f);
    float bestUnprotectScore = cleanup
        ? (severeCleanup ? SevereCleanupUnprotectKeepThreshold : CleanupUnprotectKeepThreshold)
        : -1.0f;
    float fallbackSellScore = std::numeric_limits<float>::max();
    float fallbackUnprotectScore = std::numeric_limits<float>::max();
    const AutoSimulationItemEvaluationContext itemContext =
        autoSimulationItemEvaluationContextForSnapshot(snapshot);

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(item, itemContext);
        const bool loadoutReserved = reservedForGearLoadout(item, score, currentStaffScore);
        const bool depositReserved = loadoutReserved && !item.protectionEnabled && !cleanup;
        const bool sellReserved = loadoutReserved && !cleanup;
        if (warehouseHasRoomFor(snapshot.inventory, item) &&
            !depositReserved &&
            !item.equipped &&
            (cleanup || item.important || item.protectionEnabled || score.store > bestDepositScore)) {
            const float depositScore = cleanup
                ? cleanupDepositPriority(item, score, loadoutReserved)
                : score.store;
            if (depositScore <= bestDepositScore) {
                continue;
            }
            bestDeposit = &item;
            bestDepositScore = depositScore;
        }
        if (!sellReserved && canSell(item) && score.keep < bestSellScore) {
            bestSell = &item;
            bestSellScore = score.keep;
        }
        if (cleanup && !sellReserved && canSell(item) && score.keep < fallbackSellScore) {
            fallbackSell = &item;
            fallbackSellScore = score.keep;
        }
        if (cleanup && canUnprotectForCleanup(item)) {
            if (score.keep < bestUnprotectScore) {
                bestUnprotect = &item;
                bestUnprotectScore = score.keep;
            }
            if (score.keep < fallbackUnprotectScore) {
                fallbackUnprotect = &item;
                fallbackUnprotectScore = score.keep;
            }
        }
    }

    if (cleanup &&
        bestSell == nullptr &&
        fallbackSell != nullptr &&
        fallbackSellScore <= CleanupFallbackKeepThreshold) {
        bestSell = fallbackSell;
    }
    if (cleanup &&
        bestUnprotect == nullptr &&
        fallbackUnprotect != nullptr &&
        (fallbackUnprotectScore <= CleanupFallbackKeepThreshold ||
            (bestDeposit == nullptr && bestSell == nullptr))) {
        bestUnprotect = fallbackUnprotect;
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
        const GameTestActionKind kind = bestSell->kind == GameTestObjectEntryKind::Stack
            ? GameTestActionKind::SellBackpackStack
            : GameTestActionKind::SellBackpackInstance;
        return itemAction(kind, *bestSell, std::max(1, bestSell->count), "sell " + score.reason);
    }

    if (bestUnprotect != nullptr) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestUnprotect, itemContext);
        return itemAction(
            GameTestActionKind::UnprotectBackpackInstance,
            *bestUnprotect,
            1,
            "unprotect_for_cleanup " + score.reason);
    }

    if (cleanup) {
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
