#include "devtools/autosim/AutoSimulationBaseTasks.hpp"

#include <algorithm>
#include <utility>

namespace majo::autosim {

namespace {

bool baseBusy(const GameTestSnapshot& snapshot)
{
    return snapshot.worldLoading ||
        snapshot.transitionActive ||
        snapshot.dialogueActive ||
        snapshot.pendingStoryDelayActive ||
        snapshot.firstItemNoticeActive;
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

bool backpackPressure(const GameTestInventorySnapshot& inventory)
{
    if (inventory.backpackCapacity <= 0) {
        return false;
    }
    return inventory.backpackUsedSlots >= inventory.backpackCapacity - 2 ||
        static_cast<float>(inventory.backpackUsedSlots) / static_cast<float>(inventory.backpackCapacity) >= 0.78f;
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

std::optional<GameTestAction> AutoSimulationBaseTasks::chooseAction(const GameTestSnapshot& snapshot) const
{
    if (snapshot.screenMode != GameTestScreenMode::Base || !snapshot.base.active || baseBusy(snapshot)) {
        return std::nullopt;
    }

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        if (codexNeedsSync(item)) {
            GameTestAction action;
            action.kind = GameTestActionKind::SyncEncyclopedia;
            action.reason = "codex_obtained_sync";
            return action;
        }
    }

    const bool pressure = backpackPressure(snapshot.inventory);
    const float currentStaffScore = equippedStaffScore(snapshot.inventory);
    const GameTestObjectEntrySnapshot* bestProtect = nullptr;
    const GameTestObjectEntrySnapshot* bestRepair = nullptr;
    const GameTestObjectEntrySnapshot* bestEnhance = nullptr;
    const GameTestObjectEntrySnapshot* bestDeposit = nullptr;
    const GameTestObjectEntrySnapshot* bestSell = nullptr;
    float bestProtectScore = 78.0f;
    float bestRepairScore = 72.0f;
    float bestEnhanceScore = 92.0f;
    float bestDepositScore = pressure ? 48.0f : 72.0f;
    float bestSellScore = pressure ? 52.0f : 34.0f;

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(item);
        const bool staffLoadoutReserved = reservedForStaffLoadout(item, currentStaffScore);
        if (item.kind == GameTestObjectEntryKind::Instance &&
            !item.protectionEnabled &&
            !item.equipped &&
            score.protect > bestProtectScore) {
            bestProtect = &item;
            bestProtectScore = score.protect;
        }
        if (item.kind == GameTestObjectEntryKind::Instance &&
            item.broken &&
            item.canRepair &&
            score.keep > bestRepairScore) {
            bestRepair = &item;
            bestRepairScore = score.keep;
        }
        const bool canEnhance =
            (score.preferAttackEnhance && item.canEnhanceAttack) ||
            (score.preferDigEnhance && item.canEnhanceDig);
        if (canEnhance && score.enhance > bestEnhanceScore) {
            bestEnhance = &item;
            bestEnhanceScore = score.enhance;
        }
        if (warehouseHasRoomFor(snapshot.inventory, item) &&
            !staffLoadoutReserved &&
            !item.equipped &&
            (item.important || item.protectionEnabled || score.store > bestDepositScore) &&
            score.store > bestDepositScore) {
            bestDeposit = &item;
            bestDepositScore = score.store;
        }
        if (!staffLoadoutReserved && canSell(item) && score.keep < bestSellScore) {
            bestSell = &item;
            bestSellScore = score.keep;
        }
    }

    if (bestProtect != nullptr) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestProtect);
        return itemAction(GameTestActionKind::ProtectBackpackInstance, *bestProtect, 1, "protect " + score.reason);
    }

    if (bestRepair != nullptr) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestRepair);
        return itemAction(GameTestActionKind::RepairBackpackInstance, *bestRepair, 1, "repair " + score.reason);
    }

    if (bestEnhance != nullptr) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestEnhance);
        GameTestActionKind kind = GameTestActionKind::None;
        if (bestEnhance->kind == GameTestObjectEntryKind::Stack) {
            kind = score.preferAttackEnhance && bestEnhance->canEnhanceAttack
                ? GameTestActionKind::EnhanceBackpackStackAttack
                : GameTestActionKind::EnhanceBackpackStackDig;
        } else {
            kind = score.preferAttackEnhance && bestEnhance->canEnhanceAttack
                ? GameTestActionKind::EnhanceBackpackInstanceAttack
                : GameTestActionKind::EnhanceBackpackInstanceDig;
        }
        return itemAction(kind, *bestEnhance, 1, "enhance " + score.reason);
    }

    if (bestDeposit != nullptr) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestDeposit);
        const GameTestActionKind kind = bestDeposit->kind == GameTestObjectEntryKind::Stack
            ? GameTestActionKind::DepositBackpackStack
            : GameTestActionKind::DepositBackpackInstance;
        return itemAction(kind, *bestDeposit, std::max(1, bestDeposit->count), "store " + score.reason);
    }

    if (bestSell != nullptr) {
        const AutoSimulationItemScore score = itemEvaluator_.evaluate(*bestSell);
        const GameTestActionKind kind = bestSell->kind == GameTestObjectEntryKind::Stack
            ? GameTestActionKind::SellBackpackStack
            : GameTestActionKind::SellBackpackInstance;
        return itemAction(kind, *bestSell, std::max(1, bestSell->count), "sell " + score.reason);
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
