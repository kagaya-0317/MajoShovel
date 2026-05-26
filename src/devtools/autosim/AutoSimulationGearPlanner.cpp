#include "devtools/autosim/AutoSimulationGearPlanner.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace majo::autosim {

namespace {

bool busy(const GameTestSnapshot& snapshot)
{
    return snapshot.worldLoading ||
        snapshot.transitionActive ||
        snapshot.dialogueActive ||
        snapshot.pendingStoryDelayActive ||
        snapshot.firstItemNoticeActive;
}

bool hasTag(const GameTestObjectEntrySnapshot& item, std::string_view tag)
{
    return std::any_of(item.tags.begin(), item.tags.end(), [tag](const std::string& value) {
        return value == tag;
    });
}

bool gearLike(const GameTestObjectEntrySnapshot& item)
{
    return item.lightRadius > 0.0f ||
        item.category == "武器" ||
        item.category == "掘削" ||
        item.category == "盾" ||
        item.category == "杖" ||
        item.category == "魔導書" ||
        hasTag(item, "weapon") ||
        hasTag(item, "dig_tool") ||
        hasTag(item, "hard_dig_tool") ||
        hasTag(item, "magic_dig_tool") ||
        hasTag(item, "multi_hit") ||
        hasTag(item, "captured") ||
        hasTag(item, "unique");
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

bool canProtectInDungeon(const GameTestObjectEntrySnapshot& item)
{
    return item.location == GameTestInventoryLocation::Backpack &&
        item.kind == GameTestObjectEntryKind::Instance &&
        !item.instanceId.empty() &&
        !item.objectId.empty() &&
        !item.equipped &&
        !item.protectionEnabled;
}

bool strongEnoughToProtect(
    const GameTestObjectEntrySnapshot& item,
    const AutoSimulationItemScore& score,
    float currentStaffScore)
{
    if (!gearLike(item)) {
        return false;
    }
    if (item.category == "杖") {
        return item.staffEquipScore >= currentStaffScore + 8.0f && item.staffEquipScore >= 70.0f;
    }
    return score.investment >= 96.0f ||
        score.loadout >= 88.0f ||
        score.protect >= 104.0f;
}

GameTestAction protectAction(const GameTestObjectEntrySnapshot& item, const AutoSimulationItemScore& score)
{
    GameTestAction action;
    action.kind = GameTestActionKind::ProtectBackpackInstance;
    action.objectId = item.objectId;
    action.instanceId = item.instanceId;
    action.reason = "gear_protect " + score.reason;
    return action;
}

} // namespace

std::optional<GameTestAction> AutoSimulationGearPlanner::chooseAction(const GameTestSnapshot& snapshot) const
{
    if (snapshot.screenMode != GameTestScreenMode::Playing || busy(snapshot)) {
        return std::nullopt;
    }

    const float currentStaffScore = equippedStaffScore(snapshot.inventory);
    const AutoSimulationItemEvaluationContext itemContext =
        autoSimulationItemEvaluationContextForSnapshot(snapshot);
    const GameTestObjectEntrySnapshot* bestItem = nullptr;
    AutoSimulationItemScore bestScore;
    float bestPriority = 0.0f;

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        if (!canProtectInDungeon(item)) {
            continue;
        }

        const AutoSimulationItemScore score = itemEvaluator_.evaluate(item, itemContext);
        if (!strongEnoughToProtect(item, score, currentStaffScore)) {
            continue;
        }

        const float priority = std::max({score.investment, score.loadout, score.protect, item.staffEquipScore});
        if (priority > bestPriority) {
            bestItem = &item;
            bestScore = score;
            bestPriority = priority;
        }
    }

    if (bestItem == nullptr) {
        return std::nullopt;
    }
    return protectAction(*bestItem, bestScore);
}

} // namespace majo::autosim
