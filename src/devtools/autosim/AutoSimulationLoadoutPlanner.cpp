#include "devtools/autosim/AutoSimulationLoadoutPlanner.hpp"

#include "devtools/autosim/AutoSimulationItemEvaluator.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace majo::autosim {

namespace {

constexpr float MeaningfulDigScore = 70.0f;
constexpr float MeaningfulCombatScore = 55.0f;
constexpr float MeaningfulLightRadius = 140.0f;
constexpr float MeaningfulLightScore = 70.0f;
constexpr float UsefulEquipScore = 58.0f;
constexpr float ReplaceGainScore = 26.0f;
constexpr float BrokenProtectedReplaceGainScore = 6.0f;
constexpr float StaffReplaceGainScore = 8.0f;
constexpr float LowHpDiscardAvoidRatio = 0.55f;
constexpr float PreferredUtilityRingScore = 260.0f;
constexpr std::string_view MagnifyingGlassObjectId = "item_magnifying_glass";
constexpr std::string_view CaptureNetObjectId = "item_capture_net";

struct LoadoutProfile {
    float digWeight = 1.0f;
    float combatWeight = 1.0f;
    float lightWeight = 1.0f;
    float utilityWeight = 1.0f;
    bool missingDig = true;
    bool missingCombat = true;
    bool missingLight = true;
};

struct LoadoutScore {
    float dig = 0.0f;
    float combat = 0.0f;
    float light = 0.0f;
    float utility = 0.0f;
    float total = 0.0f;
};

struct RingChoice {
    const GameTestRingItemSnapshot* item = nullptr;
    LoadoutScore score;
};

bool canEquipFromBackpack(const GameTestObjectEntrySnapshot& item)
{
    return item.location == GameTestInventoryLocation::Backpack &&
        !item.objectId.empty() &&
        !item.equipped &&
        !item.broken &&
        item.category != "杖" &&
        (item.attackPower + item.attackBonus > 0 ||
            item.digPower + item.digBonus > 0 ||
            item.lightRadius > 0.0f ||
            item.objectId == MagnifyingGlassObjectId ||
            item.objectId == CaptureNetObjectId);
}

bool canAddToRing(const GameTestObjectEntrySnapshot& item, int ringIndex)
{
    return std::find(item.addableRingIndices.begin(), item.addableRingIndices.end(), ringIndex) !=
        item.addableRingIndices.end();
}

bool hasTag(const std::vector<std::string>& tags, std::string_view tag)
{
    return std::any_of(tags.begin(), tags.end(), [tag](const std::string& value) {
        return value == tag;
    });
}

bool preferredUtilityRingItem(std::string_view objectId)
{
    return objectId == MagnifyingGlassObjectId || objectId == CaptureNetObjectId;
}

bool ringAlreadyHasObject(const GameTestSnapshot& snapshot, std::string_view objectId)
{
    if (objectId.empty()) {
        return false;
    }
    return std::any_of(snapshot.ring.items.begin(), snapshot.ring.items.end(), [objectId](const GameTestRingItemSnapshot& item) {
        return item.objectId == objectId;
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

float lightScore(
    float lightRadius,
    int durability,
    int maxDurability,
    bool broken,
    double weightKg)
{
    if (lightRadius <= 0.0f) {
        return 0.0f;
    }

    float score = 34.0f + lightRadius * 0.42f;
    score *= durabilityMultiplier(durability, maxDurability, broken);
    score -= static_cast<float>(std::max(0.0, weightKg)) * 3.2f;
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
    if (hasTag(tags, "hidden_detection") || hasTag(tags, "treasure_detection") || hasTag(tags, "vacuum")) {
        score += 8.0f;
    }
    return score;
}

LoadoutScore makeTotalScore(LoadoutScore score, const LoadoutProfile& profile)
{
    score.total =
        score.dig * profile.digWeight +
        score.combat * profile.combatWeight +
        score.light * profile.lightWeight +
        score.utility * profile.utilityWeight * 0.35f;
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
    score.light = lightScore(
        item.lightRadius,
        item.currentDurability,
        item.maxDurability,
        item.broken,
        item.weightKg);
    score.utility = utilityScore(item.rarity, item.price, item.enhanceLevel, item.protectionEnabled, item.tags);
    if (preferredUtilityRingItem(item.objectId)) {
        score.utility += PreferredUtilityRingScore;
    }
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
    score.light = lightScore(
        item.lightRadius,
        item.durability,
        item.maxDurability,
        item.broken,
        item.weightKg);
    score.utility = utilityScore(item.rarity, item.price, item.enhanceLevel, item.protectionEnabled, item.tags);
    if (preferredUtilityRingItem(item.objectId)) {
        score.utility += PreferredUtilityRingScore;
    }
    return makeTotalScore(score, profile);
}

LoadoutScore scoreRingItemRaw(const GameTestRingItemSnapshot& item)
{
    const LoadoutProfile neutralProfile;
    return scoreRingItem(item, neutralProfile);
}

bool unknownWarpRemaining(const GameTestSnapshot& snapshot)
{
    const int knownWarps = std::max(snapshot.dungeon.discoveredWarpPoints, snapshot.dungeon.unlockedWarpPoints);
    return knownWarps < static_cast<int>(snapshot.dungeon.warpPoints.size());
}

bool bossPressure(const GameTestSnapshot& snapshot)
{
    if (snapshot.dungeon.bossSpawned || snapshot.dungeon.hasBossSpawnPoint) {
        return true;
    }
    return std::any_of(snapshot.enemies.begin(), snapshot.enemies.end(), [](const GameTestEnemySnapshot& enemy) {
        return enemy.boss;
    });
}

bool hardDigWorkNearby(const GameTestSnapshot& snapshot)
{
    return std::any_of(snapshot.nearbyMineTiles.begin(), snapshot.nearbyMineTiles.end(), [](const GameTestMineTileSnapshot& tile) {
        return tile.diggable &&
            (tile.terrainKind == GameTestTerrainKind::Rock ||
                tile.terrainKind == GameTestTerrainKind::HardRock ||
                tile.terrainAttribute == GameTestTerrainAttribute::Hard);
    });
}

float hpRatio(const GameTestSnapshot& snapshot)
{
    if (snapshot.player.maxHp <= 0) {
        return 1.0f;
    }
    return std::clamp(
        static_cast<float>(snapshot.player.hp) / static_cast<float>(snapshot.player.maxHp),
        0.0f,
        1.0f);
}

LoadoutProfile makeProfile(const GameTestSnapshot& snapshot, const GameTestRingLoadoutSnapshot& ring)
{
    float bestDig = 0.0f;
    float bestCombat = 0.0f;
    float bestLightRadius = 0.0f;
    for (const GameTestRingItemSnapshot& item : snapshot.ring.items) {
        if (item.ringIndex != ring.ringIndex || item.broken) {
            continue;
        }
        const LoadoutScore score = scoreRingItemRaw(item);
        bestDig = std::max(bestDig, score.dig);
        bestCombat = std::max(bestCombat, score.combat);
        bestLightRadius = std::max(bestLightRadius, item.lightRadius);
    }

    LoadoutProfile profile;
    profile.missingDig = bestDig < MeaningfulDigScore;
    profile.missingCombat = bestCombat < MeaningfulCombatScore;
    profile.missingLight = bestLightRadius < MeaningfulLightRadius;
    profile.digWeight = 1.15f + (snapshot.nearbyMineTiles.empty() ? 0.0f : 0.45f) + (profile.missingDig ? 0.75f : 0.0f);
    profile.combatWeight = 1.05f + (snapshot.enemies.empty() ? 0.0f : 0.60f) + (profile.missingCombat ? 0.55f : 0.0f);
    profile.lightWeight = 0.60f + (profile.missingLight ? 1.30f : 0.0f);
    if (snapshot.screenMode == GameTestScreenMode::Playing) {
        profile.lightWeight += 0.20f;
    }
    if (snapshot.screenMode == GameTestScreenMode::Base) {
        profile.digWeight += 0.20f;
        profile.combatWeight += 0.15f;
        profile.lightWeight += 0.15f;
    }
    if (unknownWarpRemaining(snapshot)) {
        profile.lightWeight += 0.34f;
        profile.digWeight += 0.12f;
    }
    if (hardDigWorkNearby(snapshot) || !snapshot.ring.hasDigTool) {
        profile.digWeight += 0.32f;
    }
    if (bossPressure(snapshot)) {
        profile.combatWeight += 0.38f;
        profile.utilityWeight += 0.08f;
    }
    if (!snapshot.enemies.empty() && hpRatio(snapshot) <= 0.55f) {
        profile.combatWeight += 0.16f;
        profile.utilityWeight += 0.08f;
    }
    return profile;
}

bool usefulCandidate(const LoadoutScore& score, const LoadoutProfile& profile)
{
    return score.total >= UsefulEquipScore ||
        (profile.missingDig && score.dig > 0.0f) ||
        (profile.missingCombat && score.combat > 0.0f) ||
        (profile.missingLight && score.light > 0.0f);
}

bool fitsWeight(const GameTestRingLoadoutSnapshot& ring, double removeWeightKg, double addWeightKg)
{
    if (ring.maxWeight <= 0.0f) {
        return true;
    }
    const float nextWeight =
        ring.weight -
        static_cast<float>(std::max(0.0, removeWeightKg)) +
        static_cast<float>(std::max(0.0, addWeightKg));
    return nextWeight <= ring.maxWeight + 0.001f;
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
    if (removed.score.light >= MeaningfulLightScore && candidateScore.light < MeaningfulLightScore) {
        const bool hasOtherLight = std::any_of(ringChoices.begin(), ringChoices.end(), [&removed](const RingChoice& choice) {
            return choice.item != removed.item && choice.score.light >= MeaningfulLightScore;
        });
        if (!hasOtherLight) {
            return false;
        }
    }
    return true;
}

bool replacementRestoresMissingRole(const LoadoutProfile& profile, const LoadoutScore& candidateScore)
{
    return (profile.missingDig && candidateScore.dig > 0.0f) ||
        (profile.missingCombat && candidateScore.combat > 0.0f) ||
        (profile.missingLight && candidateScore.light > 0.0f);
}

bool replacementRestoresEmergencyDig(const LoadoutProfile& profile, const LoadoutScore& candidateScore)
{
    return profile.missingDig && candidateScore.dig > 0.0f;
}

bool removableForReplacement(
    const RingChoice& target,
    const LoadoutProfile& profile,
    const LoadoutScore& candidateScore)
{
    if (target.item == nullptr) {
        return false;
    }
    if (!target.item->protectionEnabled) {
        return true;
    }
    return target.item->broken && replacementRestoresMissingRole(profile, candidateScore);
}

float replacementGainThreshold(
    const RingChoice& target,
    const LoadoutProfile& profile,
    const LoadoutScore& candidateScore)
{
    if (target.item != nullptr &&
        target.item->protectionEnabled &&
        target.item->broken &&
        replacementRestoresMissingRole(profile, candidateScore)) {
        return BrokenProtectedReplaceGainScore;
    }
    return ReplaceGainScore;
}

bool emergencyDiscardCanUnblockReplacement(
    const RingChoice& target,
    const LoadoutProfile& profile,
    const LoadoutScore& candidateScore)
{
    return target.item != nullptr &&
        target.item->broken &&
        replacementRestoresEmergencyDig(profile, candidateScore);
}

bool codexObtained(const GameTestObjectEntrySnapshot& item)
{
    return static_cast<int>(item.codexStage) >= static_cast<int>(GameTestCodexStage::Obtained);
}

bool selfTarget(std::string_view target)
{
    return target == "player" || target == "owner" || target == "self";
}

bool hasSelfHeal(const GameTestObjectEntrySnapshot& item)
{
    return std::any_of(item.useEffects.begin(), item.useEffects.end(), [](const GameTestUseEffectSnapshot& effect) {
        return selfTarget(effect.target) && effect.effect == "heal" && effect.value > 0.0;
    });
}

bool sameBackpackEntry(
    const GameTestObjectEntrySnapshot& lhs,
    const GameTestObjectEntrySnapshot& rhs)
{
    if (!lhs.instanceId.empty() || !rhs.instanceId.empty()) {
        return !lhs.instanceId.empty() && lhs.instanceId == rhs.instanceId;
    }
    return !lhs.objectId.empty() && lhs.objectId == rhs.objectId;
}

bool discardableForRingRecovery(
    const GameTestSnapshot& snapshot,
    const GameTestObjectEntrySnapshot& item,
    const GameTestObjectEntrySnapshot& replacementItem)
{
    if (item.location != GameTestInventoryLocation::Backpack ||
        item.objectId.empty() ||
        item.important ||
        item.equipped ||
        item.protectionEnabled ||
        !codexObtained(item) ||
        sameBackpackEntry(item, replacementItem)) {
        return false;
    }
    if (item.kind == GameTestObjectEntryKind::Stack && item.count <= 0) {
        return false;
    }
    if (item.kind == GameTestObjectEntryKind::Instance && item.instanceId.empty()) {
        return false;
    }
    if (hasSelfHeal(item) && hpRatio(snapshot) <= LowHpDiscardAvoidRatio) {
        return false;
    }
    return true;
}

float emergencyDiscardCost(
    const GameTestObjectEntrySnapshot& item,
    const AutoSimulationItemScore& score)
{
    float cost = score.keep + score.investment * 0.35f + score.loadout * 0.25f;
    cost += std::min(36.0f, static_cast<float>(std::max(0, item.sellPrice)) * 0.025f);
    if (item.kind == GameTestObjectEntryKind::Stack) {
        cost += std::min(80.0f, static_cast<float>(std::max(0, item.count - 1)) * 5.0f);
    } else {
        cost += 8.0f;
    }
    if (canEquipFromBackpack(item)) {
        cost += 42.0f + score.loadout * 0.35f;
    }
    if (isStaff(item)) {
        cost += 24.0f + item.staffEquipScore * 0.60f;
    }
    if (hasSelfHeal(item)) {
        cost += 24.0f;
    }
    if (item.broken) {
        cost -= 18.0f;
    }
    return cost;
}

GameTestAction makeDiscardAction(
    const GameTestObjectEntrySnapshot& item,
    const AutoSimulationItemScore& score,
    const GameTestObjectEntrySnapshot& replacementItem)
{
    GameTestAction action;
    action.kind = item.kind == GameTestObjectEntryKind::Instance
        ? GameTestActionKind::DiscardBackpackInstance
        : GameTestActionKind::DiscardBackpackStack;
    action.objectId = item.objectId;
    action.instanceId = item.instanceId;
    action.count = item.kind == GameTestObjectEntryKind::Stack ? std::max(1, item.count) : 1;
    action.reason = "emergency_ring_recovery_free_slot " + score.reason + " for " + replacementItem.objectId;
    return action;
}

std::optional<GameTestAction> chooseEmergencyDiscardAction(
    const GameTestSnapshot& snapshot,
    const GameTestObjectEntrySnapshot& replacementItem)
{
    const GameTestObjectEntrySnapshot* bestDiscard = nullptr;
    AutoSimulationItemScore bestScore;
    float bestCost = std::numeric_limits<float>::max();
    AutoSimulationItemEvaluator evaluator;
    AutoSimulationItemEvaluationContext itemContext =
        autoSimulationItemEvaluationContextForSnapshot(snapshot);
    itemContext.backpackPressure = true;

    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        if (!discardableForRingRecovery(snapshot, item, replacementItem)) {
            continue;
        }

        const AutoSimulationItemScore score = evaluator.evaluate(item, itemContext);
        const float cost = emergencyDiscardCost(item, score);
        if (cost < bestCost) {
            bestDiscard = &item;
            bestScore = score;
            bestCost = cost;
        }
    }

    if (bestDiscard == nullptr) {
        return std::nullopt;
    }
    return makeDiscardAction(*bestDiscard, bestScore, replacementItem);
}

std::string loadoutReason(std::string_view prefix, const LoadoutScore& score)
{
    std::string reason(prefix);
    if (score.light >= score.dig && score.light >= score.combat) {
        reason += "_light_radius";
    } else if (score.dig >= score.combat) {
        reason += "_dig_efficiency";
    } else {
        reason += "_combat_efficiency";
    }
    return reason;
}

GameTestAction makeEquipAction(
    const GameTestObjectEntrySnapshot& item,
    int ringIndex,
    std::string reason)
{
    GameTestAction action;
    action.kind = GameTestActionKind::EquipBackpackItemToRing;
    action.objectId = item.objectId;
    action.instanceId = item.instanceId;
    action.ringIndex = ringIndex;
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

GameTestRingLoadoutSnapshot fallbackActiveRing(const GameTestSnapshot& snapshot)
{
    GameTestRingLoadoutSnapshot ring;
    ring.ringIndex = std::max(0, snapshot.ring.activeRingIndex);
    ring.radius = snapshot.ring.activeRadius;
    ring.angularSpeed = snapshot.ring.activeAngularSpeed;
    ring.weight = snapshot.ring.activeWeight;
    ring.maxWeight = snapshot.ring.activeMaxWeight;
    ring.itemCount = snapshot.ring.activeItemCount;
    ring.maxItemCount = snapshot.ring.activeMaxItemCount;
    ring.bestDamage = snapshot.ring.bestDamage;
    ring.bestDigPower = snapshot.ring.bestDigPower;
    ring.bestHitRadius = snapshot.ring.bestHitRadius;
    ring.bestLightRadius = snapshot.ring.bestLightRadius;
    ring.hasCombatTool = snapshot.ring.hasCombatTool;
    ring.hasDigTool = snapshot.ring.hasDigTool;
    ring.hasLightTool = snapshot.ring.hasLightTool;
    return ring;
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

    std::vector<GameTestRingLoadoutSnapshot> rings = snapshot.ring.rings;
    if (rings.empty()) {
        rings.push_back(fallbackActiveRing(snapshot));
    }

    const GameTestObjectEntrySnapshot* directItem = nullptr;
    int directRingIndex = -1;
    LoadoutScore directScore;
    const GameTestObjectEntrySnapshot* replacementItem = nullptr;
    const GameTestRingItemSnapshot* replacementTarget = nullptr;
    LoadoutScore replacementScore;
    float bestReplaceGain = -std::numeric_limits<float>::max();
    const GameTestObjectEntrySnapshot* emergencyReplacementItem = nullptr;
    float bestEmergencyReplaceGain = -std::numeric_limits<float>::max();

    const bool backpackHasFreeSlot =
        snapshot.inventory.backpackCapacity <= 0 ||
        snapshot.inventory.backpackUsedSlots < snapshot.inventory.backpackCapacity;

    for (const GameTestRingLoadoutSnapshot& ring : rings) {
        const LoadoutProfile profile = makeProfile(snapshot, ring);
        std::vector<RingChoice> ringChoices;
        for (const GameTestRingItemSnapshot& item : snapshot.ring.items) {
            if (item.ringIndex != ring.ringIndex || item.objectId.empty()) {
                continue;
            }
            ringChoices.push_back(RingChoice{&item, scoreRingItem(item, profile)});
        }

        const bool hasCountSlot =
            ring.maxItemCount > 0 &&
            ring.itemCount < ring.maxItemCount;

        for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
            if (!canEquipFromBackpack(item)) {
                continue;
            }
            if (preferredUtilityRingItem(item.objectId) && ringAlreadyHasObject(snapshot, item.objectId)) {
                continue;
            }

            const LoadoutScore candidateScore = scoreBackpackItem(item, profile);
            if (!usefulCandidate(candidateScore, profile)) {
                continue;
            }

            if (hasCountSlot &&
                canAddToRing(item, ring.ringIndex) &&
                fitsWeight(ring, 0.0, item.weightKg) &&
                candidateScore.total > directScore.total) {
                directItem = &item;
                directRingIndex = ring.ringIndex;
                directScore = candidateScore;
            }

            for (const RingChoice& target : ringChoices) {
                if (!removableForReplacement(target, profile, candidateScore)) {
                    continue;
                }
                if (!fitsWeight(ring, target.item->weightKg, item.weightKg)) {
                    continue;
                }
                if (!rolePreservedByReplacement(ringChoices, target, candidateScore)) {
                    continue;
                }

                const float threshold = replacementGainThreshold(target, profile, candidateScore);
                const float gain = candidateScore.total - target.score.total;
                if (gain <= threshold) {
                    continue;
                }
                if (backpackHasFreeSlot && gain > bestReplaceGain) {
                    bestReplaceGain = gain;
                    replacementItem = &item;
                    replacementTarget = target.item;
                    replacementScore = candidateScore;
                } else if (!backpackHasFreeSlot &&
                    emergencyDiscardCanUnblockReplacement(target, profile, candidateScore) &&
                    gain > bestEmergencyReplaceGain) {
                    bestEmergencyReplaceGain = gain;
                    emergencyReplacementItem = &item;
                }
            }
        }
    }

    if (directItem != nullptr && directRingIndex >= 0) {
        return makeEquipAction(*directItem, directRingIndex, loadoutReason("loadout_add", directScore));
    }

    if (!backpackHasFreeSlot && emergencyReplacementItem != nullptr) {
        if (std::optional<GameTestAction> action =
            chooseEmergencyDiscardAction(snapshot, *emergencyReplacementItem)) {
            return action;
        }
    }

    if (replacementItem == nullptr || replacementTarget == nullptr) {
        return std::nullopt;
    }

    return makeRemoveAction(*replacementTarget, loadoutReason("loadout_replace_for", replacementScore));
}

} // namespace majo::autosim
