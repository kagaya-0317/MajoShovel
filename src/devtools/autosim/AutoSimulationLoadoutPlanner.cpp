#include "devtools/autosim/AutoSimulationLoadoutPlanner.hpp"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace majo::autosim {

namespace {

constexpr float MeaningfulDigScore = 70.0f;
constexpr float MeaningfulCombatScore = 55.0f;
constexpr float UsefulEquipScore = 58.0f;
constexpr float ReplaceGainScore = 26.0f;
constexpr float StaffReplaceGainScore = 8.0f;

struct LoadoutProfile {
    float digWeight = 1.0f;
    float combatWeight = 1.0f;
    bool missingDig = true;
    bool missingCombat = true;
};

struct LoadoutScore {
    float dig = 0.0f;
    float combat = 0.0f;
    float utility = 0.0f;
    float total = 0.0f;
};

struct RingChoice {
    const GameTestRingItemSnapshot* item = nullptr;
    LoadoutScore score;
};

bool canEquipFromBackpack(const GameTestObjectEntrySnapshot& item)
{
    return !item.objectId.empty() &&
        !item.equipped &&
        !item.broken &&
        item.category != "杖" &&
        (item.attackPower + item.attackBonus > 0 || item.digPower + item.digBonus > 0);
}

bool hasTag(const std::vector<std::string>& tags, std::string_view tag)
{
    return std::any_of(tags.begin(), tags.end(), [tag](const std::string& value) {
        return value == tag;
    });
}

bool isTreasure(std::string_view category)
{
    return category == "宝";
}

bool isStaff(const GameTestObjectEntrySnapshot& item)
{
    return item.category == "杖";
}

bool canEquipStaffFromBackpack(const GameTestObjectEntrySnapshot& item)
{
    return item.location == GameTestInventoryLocation::Backpack &&
        isStaff(item) &&
        !item.equipped &&
        !item.broken &&
        !item.objectId.empty() &&
        item.staffEquipScore > 0.0f;
}

bool isDigSpecialist(std::string_view category, const std::vector<std::string>& tags)
{
    return category == "掘削" ||
        hasTag(tags, "dig_tool") ||
        hasTag(tags, "hard_dig_tool") ||
        hasTag(tags, "magic_dig_tool") ||
        hasTag(tags, "multi_hit");
}

bool isCombatSpecialist(std::string_view category, const std::vector<std::string>& tags)
{
    return category == "武器" ||
        category == "掘削" ||
        category == "魔導書" ||
        hasTag(tags, "weapon") ||
        hasTag(tags, "contact_damage") ||
        hasTag(tags, "boss") ||
        hasTag(tags, "captured");
}

float durabilityMultiplier(int durability, int maxDurability, bool broken)
{
    if (broken || durability == 0) {
        return 0.0f;
    }
    if (maxDurability <= 0 || durability < 0) {
        return 1.0f;
    }
    return std::clamp(static_cast<float>(durability) / static_cast<float>(maxDurability), 0.35f, 1.0f);
}

float digScore(
    int digPower,
    float hitRadius,
    int durability,
    int maxDurability,
    bool broken,
    double weightKg,
    std::string_view category,
    const std::vector<std::string>& tags)
{
    const int dig = std::max(0, digPower);
    if (dig <= 0) {
        return 0.0f;
    }

    float score = static_cast<float>(dig) * 9.5f + hitRadius * 0.35f;
    if (isDigSpecialist(category, tags)) {
        score += 44.0f;
    } else if (category == "魔導書") {
        score += 16.0f;
    } else if (isTreasure(category)) {
        score -= 18.0f;
    }
    if (hasTag(tags, "hard_dig_tool")) {
        score += 22.0f;
    }
    if (hasTag(tags, "multi_hit")) {
        score += 20.0f;
    }
    if (hasTag(tags, "dig_tool")) {
        score += 12.0f;
    }
    if (hasTag(tags, "magic_dig_tool")) {
        score += 10.0f;
    }
    if (isTreasure(category)) {
        score *= 0.42f;
    }
    score *= durabilityMultiplier(durability, maxDurability, broken);
    score -= static_cast<float>(std::max(0.0, weightKg)) * 2.2f;
    return std::max(0.0f, score);
}

float combatScore(
    int attackPower,
    float hitRadius,
    int durability,
    int maxDurability,
    bool broken,
    double weightKg,
    std::string_view category,
    std::string_view damageType,
    const std::vector<std::string>& tags)
{
    const int attack = std::max(0, attackPower);
    if (attack <= 0) {
        return 0.0f;
    }

    float score = static_cast<float>(attack) * 8.5f + hitRadius * 0.25f;
    if (isCombatSpecialist(category, tags)) {
        score += 12.0f;
    }
    if (category == "魔導書") {
        score += 18.0f;
    }
    if (hasTag(tags, "contact_damage")) {
        score += 10.0f;
    }
    if (damageType != "none" && damageType != "blunt") {
        score += 5.0f;
    }
    if (isTreasure(category) && attack <= 4) {
        score *= 0.75f;
    }
    score *= durabilityMultiplier(durability, maxDurability, broken);
    score -= static_cast<float>(std::max(0.0, weightKg));
    return std::max(0.0f, score);
}

float utilityScore(
    int rarity,
    int price,
    int enhanceLevel,
    bool protectionEnabled,
    const std::vector<std::string>& tags)
{
    float score = static_cast<float>(std::clamp(rarity, 0, 10)) * 2.0f;
    score += std::min(8.0f, static_cast<float>(std::max(0, price)) * 0.01f);
    score += static_cast<float>(std::max(0, enhanceLevel)) * 4.0f;
    if (protectionEnabled) {
        score += 8.0f;
    }
    if (hasTag(tags, "light_source")) {
        score += 7.0f;
    }
    if (hasTag(tags, "hidden_detection") || hasTag(tags, "treasure_detection") || hasTag(tags, "vacuum")) {
        score += 8.0f;
    }
    return score;
}

LoadoutScore makeTotalScore(LoadoutScore score, const LoadoutProfile& profile)
{
    score.total = score.dig * profile.digWeight + score.combat * profile.combatWeight + score.utility * 0.35f;
    return score;
}

LoadoutScore scoreBackpackItem(const GameTestObjectEntrySnapshot& item, const LoadoutProfile& profile)
{
    LoadoutScore score;
    score.dig = digScore(
        item.digPower + item.digBonus,
        11.0f,
        item.currentDurability,
        item.maxDurability,
        item.broken,
        item.weightKg,
        item.category,
        item.tags);
    score.combat = combatScore(
        item.attackPower + item.attackBonus,
        11.0f,
        item.currentDurability,
        item.maxDurability,
        item.broken,
        item.weightKg,
        item.category,
        item.damageType,
        item.tags);
    score.utility = utilityScore(item.rarity, item.price, item.enhanceLevel, item.protectionEnabled, item.tags);
    return makeTotalScore(score, profile);
}

LoadoutScore scoreRingItem(const GameTestRingItemSnapshot& item, const LoadoutProfile& profile)
{
    LoadoutScore score;
    score.dig = digScore(
        item.digPower,
        item.hitRadius,
        item.durability,
        item.maxDurability,
        item.broken,
        item.weightKg,
        item.category,
        item.tags);
    score.combat = combatScore(
        item.damage,
        item.hitRadius,
        item.durability,
        item.maxDurability,
        item.broken,
        item.weightKg,
        item.category,
        item.damageType,
        item.tags);
    score.utility = utilityScore(item.rarity, item.price, item.enhanceLevel, item.protectionEnabled, item.tags);
    return makeTotalScore(score, profile);
}

LoadoutScore scoreRingItemRaw(const GameTestRingItemSnapshot& item)
{
    const LoadoutProfile neutralProfile{1.0f, 1.0f, false, false};
    return scoreRingItem(item, neutralProfile);
}

LoadoutProfile makeProfile(const GameTestSnapshot& snapshot)
{
    float bestDig = 0.0f;
    float bestCombat = 0.0f;
    for (const GameTestRingItemSnapshot& item : snapshot.ring.items) {
        if (item.ringIndex != snapshot.ring.activeRingIndex || item.broken) {
            continue;
        }
        const LoadoutScore score = scoreRingItemRaw(item);
        bestDig = std::max(bestDig, score.dig);
        bestCombat = std::max(bestCombat, score.combat);
    }

    LoadoutProfile profile;
    profile.missingDig = bestDig < MeaningfulDigScore;
    profile.missingCombat = bestCombat < MeaningfulCombatScore;
    profile.digWeight = 1.15f + (snapshot.nearbyMineTiles.empty() ? 0.0f : 0.45f) + (profile.missingDig ? 0.75f : 0.0f);
    profile.combatWeight = 1.05f + (snapshot.enemies.empty() ? 0.0f : 0.60f) + (profile.missingCombat ? 0.55f : 0.0f);
    if (snapshot.screenMode == GameTestScreenMode::Base) {
        profile.digWeight += 0.20f;
        profile.combatWeight += 0.15f;
    }
    return profile;
}

bool usefulCandidate(const LoadoutScore& score, const LoadoutProfile& profile)
{
    return score.total >= UsefulEquipScore ||
        (profile.missingDig && score.dig > 0.0f) ||
        (profile.missingCombat && score.combat > 0.0f);
}

bool fitsWeight(const GameTestSnapshot& snapshot, double removeWeightKg, double addWeightKg)
{
    if (snapshot.ring.activeMaxWeight <= 0.0f) {
        return true;
    }
    const float nextWeight =
        snapshot.ring.activeWeight -
        static_cast<float>(std::max(0.0, removeWeightKg)) +
        static_cast<float>(std::max(0.0, addWeightKg));
    return nextWeight <= snapshot.ring.activeMaxWeight + 0.001f;
}

bool rolePreservedByReplacement(
    const std::vector<RingChoice>& ringChoices,
    const RingChoice& removed,
    const LoadoutScore& candidateScore)
{
    if (removed.score.dig >= MeaningfulDigScore && candidateScore.dig < MeaningfulDigScore) {
        const bool hasOtherDig = std::any_of(ringChoices.begin(), ringChoices.end(), [&removed](const RingChoice& choice) {
            return choice.item != removed.item && choice.score.dig >= MeaningfulDigScore;
        });
        if (!hasOtherDig) {
            return false;
        }
    }
    if (removed.score.combat >= MeaningfulCombatScore && candidateScore.combat < MeaningfulCombatScore) {
        const bool hasOtherCombat = std::any_of(ringChoices.begin(), ringChoices.end(), [&removed](const RingChoice& choice) {
            return choice.item != removed.item && choice.score.combat >= MeaningfulCombatScore;
        });
        if (!hasOtherCombat) {
            return false;
        }
    }
    return true;
}

GameTestAction makeEquipAction(
    const GameTestSnapshot& snapshot,
    const GameTestObjectEntrySnapshot& item,
    std::string reason)
{
    GameTestAction action;
    action.kind = GameTestActionKind::EquipBackpackItemToRing;
    action.objectId = item.objectId;
    action.instanceId = item.instanceId;
    action.ringIndex = snapshot.ring.activeRingIndex;
    action.reason = std::move(reason) + " " + item.objectId;
    return action;
}

GameTestAction makeRemoveAction(const GameTestRingItemSnapshot& item, std::string reason)
{
    GameTestAction action;
    action.kind = GameTestActionKind::RemoveRingItemToBackpack;
    action.objectId = item.objectId;
    action.instanceId = item.instanceId;
    action.ringIndex = item.ringIndex;
    action.ringItemIndex = item.itemIndex;
    action.reason = std::move(reason) + " " + item.objectId;
    return action;
}

std::optional<GameTestAction> chooseStaffAction(const GameTestSnapshot& snapshot)
{
    const GameTestObjectEntrySnapshot* bestStaff = nullptr;
    float equippedScore = 0.0f;
    float bestScore = 0.0f;

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        if (!isStaff(item)) {
            continue;
        }
        if (item.equipped) {
            equippedScore = std::max(equippedScore, item.staffEquipScore);
            continue;
        }
        if (!canEquipStaffFromBackpack(item)) {
            continue;
        }
        if (item.staffEquipScore > bestScore) {
            bestStaff = &item;
            bestScore = item.staffEquipScore;
        }
    }

    if (bestStaff == nullptr || bestScore < equippedScore + StaffReplaceGainScore) {
        return std::nullopt;
    }

    GameTestAction action;
    action.kind = GameTestActionKind::EquipBackpackStaff;
    action.objectId = bestStaff->objectId;
    action.instanceId = bestStaff->instanceId;
    action.reason = "staff_loadout_score " + bestStaff->objectId;
    return action;
}

} // namespace

std::optional<GameTestAction> AutoSimulationLoadoutPlanner::chooseAction(const GameTestSnapshot& snapshot) const
{
    if (snapshot.worldLoading || snapshot.transitionActive || snapshot.dialogueActive) {
        return std::nullopt;
    }
    if (snapshot.firstItemNoticeActive || snapshot.pendingStoryDelayActive) {
        return std::nullopt;
    }

    if (std::optional<GameTestAction> staffAction = chooseStaffAction(snapshot)) {
        return staffAction;
    }

    const LoadoutProfile profile = makeProfile(snapshot);
    std::vector<RingChoice> ringChoices;
    for (const GameTestRingItemSnapshot& item : snapshot.ring.items) {
        if (item.ringIndex != snapshot.ring.activeRingIndex || item.broken || item.objectId.empty()) {
            continue;
        }
        ringChoices.push_back(RingChoice{&item, scoreRingItem(item, profile)});
    }

    const GameTestObjectEntrySnapshot* directItem = nullptr;
    LoadoutScore directScore;
    const GameTestObjectEntrySnapshot* replacementItem = nullptr;
    const RingChoice* replacementTarget = nullptr;
    LoadoutScore replacementScore;
    float bestReplaceGain = ReplaceGainScore;

    const bool hasCountSlot =
        snapshot.ring.activeCanAddItem &&
        snapshot.ring.activeItemCount < snapshot.ring.activeMaxItemCount;
    const bool backpackHasFreeSlot =
        snapshot.inventory.backpackCapacity <= 0 ||
        snapshot.inventory.backpackUsedSlots < snapshot.inventory.backpackCapacity;

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        if (!canEquipFromBackpack(item)) {
            continue;
        }

        const LoadoutScore candidateScore = scoreBackpackItem(item, profile);
        if (!usefulCandidate(candidateScore, profile)) {
            continue;
        }

        if (hasCountSlot && fitsWeight(snapshot, 0.0, item.weightKg) && candidateScore.total > directScore.total) {
            directItem = &item;
            directScore = candidateScore;
        }

        if (!backpackHasFreeSlot) {
            continue;
        }

        for (const RingChoice& target : ringChoices) {
            if (target.item == nullptr || target.item->protectionEnabled) {
                continue;
            }
            if (!fitsWeight(snapshot, target.item->weightKg, item.weightKg)) {
                continue;
            }
            if (!rolePreservedByReplacement(ringChoices, target, candidateScore)) {
                continue;
            }

            const float gain = candidateScore.total - target.score.total;
            if (gain > bestReplaceGain) {
                bestReplaceGain = gain;
                replacementItem = &item;
                replacementTarget = &target;
                replacementScore = candidateScore;
            }
        }
    }

    if (directItem != nullptr) {
        const std::string reason = directScore.dig >= directScore.combat
            ? "loadout_add_dig_efficiency"
            : "loadout_add_combat_efficiency";
        return makeEquipAction(snapshot, *directItem, reason);
    }

    if (replacementItem == nullptr || replacementTarget == nullptr || replacementTarget->item == nullptr) {
        return std::nullopt;
    }

    const std::string reason = replacementScore.dig >= replacementScore.combat
        ? "loadout_replace_for_dig_efficiency"
        : "loadout_replace_for_combat_efficiency";
    return makeRemoveAction(*replacementTarget->item, reason);
}

} // namespace majo::autosim
