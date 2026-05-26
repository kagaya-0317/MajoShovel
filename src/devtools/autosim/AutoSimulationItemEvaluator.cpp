#include "devtools/autosim/AutoSimulationItemEvaluator.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>
#include <vector>

namespace majo::autosim {

namespace {

bool hasTag(const std::vector<std::string>& tags, std::string_view tag)
{
    return std::any_of(tags.begin(), tags.end(), [tag](const std::string& value) {
        return value == tag;
    });
}

bool hasAnyTag(const std::vector<std::string>& itemTags, std::initializer_list<std::string_view> tags)
{
    return std::any_of(tags.begin(), tags.end(), [&itemTags](std::string_view tag) {
        return hasTag(itemTags, tag);
    });
}

bool isEquipmentCategory(std::string_view category)
{
    return category == "武器" ||
        category == "掘削" ||
        category == "盾" ||
        category == "杖" ||
        category == "魔導書";
}

bool isTreasureCategory(std::string_view category)
{
    return category == "宝";
}

bool codexNeedsCare(const GameTestObjectEntrySnapshot& item)
{
    return static_cast<int>(item.codexStage) < static_cast<int>(GameTestCodexStage::Obtained);
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

float combatValue(
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
    if (category == "武器" || category == "掘削" || category == "魔導書") {
        score += 14.0f;
    }
    if (hasTag(tags, "weapon") || hasTag(tags, "contact_damage")) {
        score += 12.0f;
    }
    if (hasTag(tags, "boss") || hasTag(tags, "captured") || hasTag(tags, "unique")) {
        score += 12.0f;
    }
    if (category == "魔導書") {
        score += 12.0f;
    }
    if (damageType != "none" && damageType != "blunt") {
        score += 5.0f;
    }
    if (isTreasureCategory(category) && attack <= 4) {
        score *= 0.75f;
    }
    score *= durabilityMultiplier(durability, maxDurability, broken);
    score -= static_cast<float>(std::max(0.0, weightKg));
    return std::max(0.0f, score);
}

float digValue(
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
    if (category == "掘削") {
        score += 44.0f;
    } else if (category == "魔導書") {
        score += 16.0f;
    } else if (isTreasureCategory(category)) {
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
    if (isTreasureCategory(category)) {
        score *= 0.42f;
    }
    score *= durabilityMultiplier(durability, maxDurability, broken);
    score -= static_cast<float>(std::max(0.0, weightKg)) * 2.2f;
    return std::max(0.0f, score);
}

float lightValue(float lightRadius, int durability, int maxDurability, bool broken, double weightKg)
{
    if (lightRadius <= 0.0f) {
        return 0.0f;
    }

    float score = 34.0f + lightRadius * 0.42f;
    score *= durabilityMultiplier(durability, maxDurability, broken);
    score -= static_cast<float>(std::max(0.0, weightKg)) * 3.2f;
    return std::max(0.0f, score);
}

float utilityValue(int rarity, int price, int enhanceLevel, bool protectionEnabled, const std::vector<std::string>& tags)
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

float investmentValue(
    int attackPower,
    int digPower,
    int maxDurability,
    float lightRadius,
    int rarity,
    int price,
    int enhanceLevel,
    int bonusTotal,
    bool important,
    bool codexCare,
    bool protectionEnabled,
    bool equipped,
    bool broken,
    std::string_view category,
    const std::vector<std::string>& tags)
{
    const int attack = std::max(0, attackPower);
    const int dig = std::max(0, digPower);

    float score = static_cast<float>(std::clamp(rarity, 0, 10)) * 9.0f;
    score += std::min(34.0f, static_cast<float>(std::max(0, price)) * 0.05f);
    score += static_cast<float>(std::max(attack, dig)) * 8.0f;
    score += static_cast<float>(std::min(attack, dig)) * 3.0f;
    score += static_cast<float>(std::max(0, enhanceLevel)) * 10.0f;
    score += static_cast<float>(std::max(0, bonusTotal)) * 4.0f;
    if (maxDurability > 0) {
        score += std::min(14.0f, static_cast<float>(maxDurability) * 0.16f);
    }
    if (lightRadius > 0.0f) {
        score += 18.0f + std::min(46.0f, lightRadius * 0.14f);
    }
    if (isEquipmentCategory(category) && (attack > 0 || dig > 0 || category == "杖")) {
        score += 20.0f;
    }
    if (isTreasureCategory(category)) {
        score += 10.0f;
    }
    if (hasAnyTag(tags, {"captured", "boss", "rare", "unique", "quest", "key_item"})) {
        score += 22.0f;
    }
    if (hasAnyTag(tags, {"hidden_detection", "treasure_detection", "vacuum"})) {
        score += 8.0f;
    }
    if (important) {
        score += 80.0f;
    }
    if (codexCare) {
        score += 20.0f;
    }
    if (protectionEnabled || equipped) {
        score += 12.0f;
    }
    if (broken) {
        score -= 8.0f;
    }
    return std::max(0.0f, score);
}

std::string objectReason(const GameTestObjectEntrySnapshot& item, float keep, float investment)
{
    std::string reason = item.objectId.empty() ? item.name : item.objectId;
    reason += " keep=" + std::to_string(static_cast<int>(std::round(keep)));
    reason += " invest=" + std::to_string(static_cast<int>(std::round(investment)));
    if (item.important) {
        reason += " important";
    } else if (codexNeedsCare(item)) {
        reason += " codex";
    } else if (item.rarity >= 5) {
        reason += " rare";
    } else if (item.lightRadius > 0.0f) {
        reason += " light";
    }
    return reason;
}

std::string ringReason(const GameTestRingItemSnapshot& item, float keep, float investment)
{
    std::string reason = item.objectId.empty() ? item.name : item.objectId;
    reason += " keep=" + std::to_string(static_cast<int>(std::round(keep)));
    reason += " invest=" + std::to_string(static_cast<int>(std::round(investment)));
    if (item.rarity >= 5) {
        reason += " rare";
    } else if (item.lightRadius > 0.0f) {
        reason += " light";
    }
    return reason;
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

bool backpackPressure(const GameTestInventorySnapshot& inventory)
{
    if (inventory.backpackCapacity <= 0) {
        return false;
    }
    return inventory.backpackUsedSlots >= inventory.backpackCapacity - 2 ||
        static_cast<float>(inventory.backpackUsedSlots) / static_cast<float>(inventory.backpackCapacity) >= 0.78f;
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

float contextualNeedBonus(
    const AutoSimulationItemScore& score,
    const AutoSimulationItemEvaluationContext& context)
{
    float bonus = 0.0f;
    bonus += score.combat * std::max(0.0f, context.combatWeight - 1.0f) * 0.42f;
    bonus += score.dig * std::max(0.0f, context.digWeight - 1.0f) * 0.42f;
    bonus += score.light * std::max(0.0f, context.lightWeight - 1.0f) * 0.34f;
    bonus += score.utility * std::max(0.0f, context.utilityWeight - 1.0f) * 0.26f;
    return bonus;
}

} // namespace

AutoSimulationItemEvaluationContext autoSimulationItemEvaluationContextForSnapshot(
    const GameTestSnapshot& snapshot)
{
    AutoSimulationItemEvaluationContext context;
    const bool hasEnemies = !snapshot.enemies.empty();
    const bool boss = bossPressure(snapshot);
    const bool unknownWarp = unknownWarpRemaining(snapshot);
    const bool hardDig = hardDigWorkNearby(snapshot);
    const float hp = hpRatio(snapshot);

    if (hasEnemies) {
        context.combatWeight += 0.28f;
        if (hp <= 0.55f) {
            context.combatWeight += 0.18f;
            context.utilityWeight += 0.10f;
        }
    }
    if (boss) {
        context.combatWeight += 0.42f;
        context.utilityWeight += 0.12f;
    }
    if (hardDig) {
        context.digWeight += 0.34f;
    }
    if (!snapshot.ring.hasDigTool) {
        context.digWeight += 0.42f;
    }
    if (unknownWarp) {
        context.lightWeight += 0.38f;
        context.digWeight += 0.12f;
    }
    if (!snapshot.ring.hasLightTool || snapshot.ring.bestLightRadius < 140.0f) {
        context.lightWeight += 0.58f;
    }
    if (snapshot.screenMode == GameTestScreenMode::Base) {
        context.combatWeight += 0.10f;
        context.digWeight += 0.14f;
        context.lightWeight += 0.10f;
    }
    context.backpackPressure = backpackPressure(snapshot.inventory);
    return context;
}

AutoSimulationItemScore AutoSimulationItemEvaluator::evaluate(const GameTestObjectEntrySnapshot& item) const
{
    return evaluate(item, {});
}

AutoSimulationItemScore AutoSimulationItemEvaluator::evaluate(
    const GameTestObjectEntrySnapshot& item,
    const AutoSimulationItemEvaluationContext& context) const
{
    const int attackTotal = std::max(0, item.attackPower + item.attackBonus);
    const int digTotal = std::max(0, item.digPower + item.digBonus);
    const int currentDurability = item.currentDurability >= 0 ? item.currentDurability : item.durability;
    const int maxDurability = item.maxDurability >= 0 ? item.maxDurability : item.durability;
    const int positiveDurability = std::max(0, std::max(maxDurability, currentDurability));
    const int bonusTotal =
        std::max(0, item.attackBonus) +
        std::max(0, item.digBonus) +
        std::max(0, item.durabilityBonus);
    const bool codexCare = codexNeedsCare(item);

    AutoSimulationItemScore score;
    const float rawCombat = combatValue(
        attackTotal,
        11.0f,
        currentDurability,
        maxDurability,
        item.broken,
        item.weightKg,
        item.category,
        item.damageType,
        item.tags);
    const float rawDig = digValue(
        digTotal,
        11.0f,
        currentDurability,
        maxDurability,
        item.broken,
        item.weightKg,
        item.category,
        item.tags);
    const float rawLight = lightValue(
        item.lightRadius,
        currentDurability,
        maxDurability,
        item.broken,
        item.weightKg);
    const float rawUtility = utilityValue(item.rarity, item.price, item.enhanceLevel, item.protectionEnabled, item.tags);
    score.combat = rawCombat * context.combatWeight;
    score.dig = rawDig * context.digWeight;
    score.light = rawLight * context.lightWeight;
    score.utility = rawUtility * context.utilityWeight;
    score.loadout = score.combat + score.dig + score.light + score.utility * 0.35f;
    score.investment = investmentValue(
        attackTotal,
        digTotal,
        maxDurability,
        item.lightRadius,
        item.rarity,
        item.price,
        item.enhanceLevel,
        bonusTotal,
        item.important,
        codexCare,
        item.protectionEnabled,
        item.equipped,
        item.broken,
        item.category,
        item.tags);
    score.investment += std::min(54.0f, contextualNeedBonus(score, context) * 1.10f);
    const float potentialLight = lightValue(
        item.lightRadius,
        currentDurability,
        maxDurability,
        false,
        item.weightKg);

    float keep = 0.0f;
    keep += static_cast<float>(std::clamp(item.rarity, 0, 10)) * 8.0f;
    keep += std::min(36.0f, static_cast<float>(std::max(0, item.price)) * 0.075f);
    keep += static_cast<float>(attackTotal) * 4.5f;
    keep += static_cast<float>(digTotal) * 4.5f;
    keep += static_cast<float>(positiveDurability) * 0.18f;
    keep += static_cast<float>(std::max(0, item.enhanceLevel)) * 8.0f;
    keep += static_cast<float>(bonusTotal) * 3.0f;
    keep += std::min(52.0f, score.loadout * 0.35f);
    keep += std::min(42.0f, (item.broken ? potentialLight * context.lightWeight : score.light) * 0.40f);

    if (item.important) {
        keep += 100.0f;
    }
    if (codexCare) {
        keep += 28.0f;
    }
    if (isTreasureCategory(item.category)) {
        keep += 18.0f;
    }
    if (isEquipmentCategory(item.category)) {
        keep += 14.0f;
    }
    if (hasAnyTag(item.tags, {"captured", "boss", "rare", "unique", "quest", "key_item"})) {
        keep += 18.0f;
    }
    if (item.equipped || item.protectionEnabled) {
        keep += 24.0f;
    }
    if (item.broken) {
        keep -= 12.0f;
    }
    keep += std::min(48.0f, contextualNeedBonus(score, context));
    if (context.backpackPressure &&
        !item.important &&
        !codexCare &&
        !item.equipped &&
        !item.protectionEnabled) {
        keep -= 10.0f;
    }

    score.keep = std::max(0.0f, keep);
    score.sell = std::max(0.0f, 92.0f - score.keep);
    score.store = score.keep + (item.count > 1 ? 4.0f : 0.0f);
    score.protect = item.kind == GameTestObjectEntryKind::Instance
        ? std::max(score.keep, score.investment) + static_cast<float>(std::max(0, item.enhanceLevel)) * 5.0f
        : 0.0f;
    score.enhance =
        std::max(score.keep, score.investment) +
        std::max(score.combat, score.dig) * 0.35f -
        static_cast<float>(std::max(0, item.enhanceLevel)) * 7.0f;
    score.preferAttackEnhance = score.combat >= score.dig && attackTotal > 0;
    score.preferDigEnhance = score.dig > score.combat && digTotal > 0;
    if (!score.preferAttackEnhance && !score.preferDigEnhance) {
        score.preferDigEnhance = item.digPower > 0;
        score.preferAttackEnhance = !score.preferDigEnhance && item.attackPower > 0;
    }
    score.reason = objectReason(item, score.keep, score.investment);
    return score;
}

AutoSimulationItemScore AutoSimulationItemEvaluator::evaluate(const GameTestRingItemSnapshot& item) const
{
    return evaluate(item, {});
}

AutoSimulationItemScore AutoSimulationItemEvaluator::evaluate(
    const GameTestRingItemSnapshot& item,
    const AutoSimulationItemEvaluationContext& context) const
{
    const int attackTotal = std::max(0, item.damage);
    const int digTotal = std::max(0, item.digPower);
    const int maxDurability = item.maxDurability;
    const int positiveDurability = std::max(0, std::max(item.maxDurability, item.durability));
    const int bonusTotal =
        std::max(0, item.attackBonus) +
        std::max(0, item.digBonus) +
        std::max(0, item.durabilityBonus);

    AutoSimulationItemScore score;
    const float rawCombat = combatValue(
        attackTotal,
        item.hitRadius,
        item.durability,
        item.maxDurability,
        item.broken,
        item.weightKg,
        item.category,
        item.damageType,
        item.tags);
    const float rawDig = digValue(
        digTotal,
        item.hitRadius,
        item.durability,
        item.maxDurability,
        item.broken,
        item.weightKg,
        item.category,
        item.tags);
    const float rawLight = lightValue(
        item.lightRadius,
        item.durability,
        item.maxDurability,
        item.broken,
        item.weightKg);
    const float rawUtility = utilityValue(item.rarity, item.price, item.enhanceLevel, item.protectionEnabled, item.tags);
    score.combat = rawCombat * context.combatWeight;
    score.dig = rawDig * context.digWeight;
    score.light = rawLight * context.lightWeight;
    score.utility = rawUtility * context.utilityWeight;
    score.loadout = score.combat + score.dig + score.light + score.utility * 0.35f;
    score.investment = investmentValue(
        attackTotal,
        digTotal,
        maxDurability,
        item.lightRadius,
        item.rarity,
        item.price,
        item.enhanceLevel,
        bonusTotal,
        false,
        false,
        item.protectionEnabled,
        true,
        item.broken,
        item.category,
        item.tags);
    score.investment += std::min(54.0f, contextualNeedBonus(score, context) * 1.10f);
    const float potentialLight = lightValue(
        item.lightRadius,
        item.durability,
        item.maxDurability,
        false,
        item.weightKg);

    float keep = 0.0f;
    keep += static_cast<float>(std::clamp(item.rarity, 0, 10)) * 8.0f;
    keep += std::min(36.0f, static_cast<float>(std::max(0, item.price)) * 0.075f);
    keep += static_cast<float>(attackTotal) * 4.5f;
    keep += static_cast<float>(digTotal) * 4.5f;
    keep += static_cast<float>(positiveDurability) * 0.18f;
    keep += static_cast<float>(std::max(0, item.enhanceLevel)) * 8.0f;
    keep += static_cast<float>(bonusTotal) * 3.0f;
    keep += std::min(52.0f, score.loadout * 0.35f);
    keep += std::min(42.0f, (item.broken ? potentialLight * context.lightWeight : score.light) * 0.40f);
    if (isTreasureCategory(item.category)) {
        keep += 18.0f;
    }
    if (isEquipmentCategory(item.category)) {
        keep += 14.0f;
    }
    if (hasAnyTag(item.tags, {"captured", "boss", "rare", "unique", "quest", "key_item"})) {
        keep += 18.0f;
    }
    if (item.protectionEnabled) {
        keep += 24.0f;
    }
    if (item.broken) {
        keep -= 12.0f;
    }
    keep += std::min(48.0f, contextualNeedBonus(score, context));

    score.keep = std::max(0.0f, keep);
    score.sell = 0.0f;
    score.store = score.keep;
    score.protect = 0.0f;
    score.enhance =
        std::max(score.keep, score.investment) +
        std::max(score.combat, score.dig) * 0.35f -
        static_cast<float>(std::max(0, item.enhanceLevel)) * 7.0f;
    score.preferAttackEnhance = score.combat >= score.dig && attackTotal > 0;
    score.preferDigEnhance = score.dig > score.combat && digTotal > 0;
    if (!score.preferAttackEnhance && !score.preferDigEnhance) {
        score.preferDigEnhance = item.digPower > 0;
        score.preferAttackEnhance = !score.preferDigEnhance && item.damage > 0;
    }
    score.reason = ringReason(item, score.keep, score.investment);
    return score;
}

} // namespace majo::autosim
