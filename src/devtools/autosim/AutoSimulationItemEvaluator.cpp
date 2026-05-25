#include "devtools/autosim/AutoSimulationItemEvaluator.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace majo::autosim {

namespace {

bool hasTag(const GameTestObjectEntrySnapshot& item, std::string_view tag)
{
    return std::any_of(item.tags.begin(), item.tags.end(), [tag](const std::string& value) {
        return value == tag;
    });
}

bool hasAnyTag(const GameTestObjectEntrySnapshot& item, std::initializer_list<std::string_view> tags)
{
    return std::any_of(tags.begin(), tags.end(), [&item](std::string_view tag) {
        return hasTag(item, tag);
    });
}

bool isEquipmentCategory(const GameTestObjectEntrySnapshot& item)
{
    return item.category == "武器" ||
        item.category == "掘削" ||
        item.category == "盾" ||
        item.category == "杖" ||
        item.category == "魔導書";
}

bool isTreasureCategory(const GameTestObjectEntrySnapshot& item)
{
    return item.category == "宝";
}

bool codexNeedsCare(const GameTestObjectEntrySnapshot& item)
{
    return static_cast<int>(item.codexStage) < static_cast<int>(GameTestCodexStage::Obtained);
}

std::string itemReason(const GameTestObjectEntrySnapshot& item, float keep)
{
    std::string reason = item.objectId.empty() ? item.name : item.objectId;
    reason += " keep=" + std::to_string(static_cast<int>(std::round(keep)));
    if (item.important) {
        reason += " important";
    } else if (codexNeedsCare(item)) {
        reason += " codex";
    } else if (item.rarity >= 5) {
        reason += " rare";
    }
    return reason;
}

} // namespace

AutoSimulationItemScore AutoSimulationItemEvaluator::evaluate(const GameTestObjectEntrySnapshot& item) const
{
    const int attackTotal = std::max(0, item.attackPower + item.attackBonus);
    const int digTotal = std::max(0, item.digPower + item.digBonus);
    const int durability = std::max(item.maxDurability, item.durability);
    const int positiveDurability = std::max(0, durability);

    float keep = 0.0f;
    keep += static_cast<float>(std::clamp(item.rarity, 0, 10)) * 8.0f;
    keep += std::min(36.0f, static_cast<float>(std::max(0, item.price)) * 0.075f);
    keep += static_cast<float>(attackTotal) * 4.5f;
    keep += static_cast<float>(digTotal) * 4.5f;
    keep += static_cast<float>(positiveDurability) * 0.18f;
    keep += static_cast<float>(std::max(0, item.enhanceLevel)) * 8.0f;
    keep += static_cast<float>(
        std::max(0, item.attackBonus) +
        std::max(0, item.digBonus) +
        std::max(0, item.durabilityBonus)) * 3.0f;

    if (item.important) {
        keep += 100.0f;
    }
    if (codexNeedsCare(item)) {
        keep += 28.0f;
    }
    if (isTreasureCategory(item)) {
        keep += 18.0f;
    }
    if (isEquipmentCategory(item)) {
        keep += 14.0f;
    }
    if (hasAnyTag(item, {"captured", "boss", "rare", "unique", "quest", "key_item"})) {
        keep += 18.0f;
    }
    if (item.equipped || item.protectionEnabled) {
        keep += 24.0f;
    }
    if (item.broken) {
        keep -= 12.0f;
    }

    AutoSimulationItemScore score;
    score.keep = std::max(0.0f, keep);
    score.sell = std::max(0.0f, 92.0f - score.keep);
    score.store = score.keep + (item.count > 1 ? 4.0f : 0.0f);
    score.protect = item.kind == GameTestObjectEntryKind::Instance
        ? score.keep + static_cast<float>(std::max(0, item.enhanceLevel)) * 5.0f
        : 0.0f;
    score.enhance =
        score.keep +
        static_cast<float>(attackTotal + digTotal) * 2.0f -
        static_cast<float>(std::max(0, item.enhanceLevel)) * 7.0f;
    score.preferAttackEnhance = attackTotal >= digTotal && attackTotal > 0;
    score.preferDigEnhance = digTotal > attackTotal && digTotal > 0;
    if (!score.preferAttackEnhance && !score.preferDigEnhance) {
        score.preferDigEnhance = item.digPower > 0;
        score.preferAttackEnhance = !score.preferDigEnhance && item.attackPower > 0;
    }
    score.reason = itemReason(item, score.keep);
    return score;
}

} // namespace majo::autosim
