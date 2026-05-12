#include "game/SpellRingItem.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>

namespace majo {

namespace {

bool parseIntStrict(std::string_view text, int& value)
{
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseDoubleStrict(std::string_view text, double& value)
{
    if (text.empty()) {
        return false;
    }
    std::string copy(text);
    char* parsedEnd = nullptr;
    value = std::strtod(copy.c_str(), &parsedEnd);
    return parsedEnd == copy.c_str() + copy.size();
}

}

bool SpellRingItem::hasCapturedBehavior(std::string_view behaviorId) const
{
    if (capturedBehaviorId == behaviorId) {
        return true;
    }
    return std::any_of(capturedBehaviorIds.begin(), capturedBehaviorIds.end(), [behaviorId](const std::string& id) {
        return id == behaviorId;
    });
}

const CapturedBehaviorSpec* SpellRingItem::capturedBehaviorSpec(std::string_view behaviorId) const
{
    for (const CapturedBehaviorSpec& spec : capturedBehaviorSpecs) {
        if (spec.behavior == behaviorId) {
            return &spec;
        }
        if (behaviorId == "throw_stone" && spec.behavior == "throw_object") {
            return &spec;
        }
    }
    return nullptr;
}

double SpellRingItem::capturedBehaviorInterval(std::string_view behaviorId, double fallbackSeconds) const
{
    const CapturedBehaviorSpec* spec = capturedBehaviorSpec(behaviorId);
    if (spec == nullptr || spec->intervalSeconds <= 0.0) {
        return fallbackSeconds;
    }
    return spec->intervalSeconds;
}

double SpellRingItem::capturedBehaviorParamDouble(std::string_view behaviorId, std::string_view key, double fallbackValue) const
{
    const CapturedBehaviorSpec* spec = capturedBehaviorSpec(behaviorId);
    if (spec == nullptr) {
        return fallbackValue;
    }
    const auto it = spec->params.find(std::string(key));
    if (it == spec->params.end()) {
        return fallbackValue;
    }
    double value = fallbackValue;
    if (!parseDoubleStrict(it->second, value) || !std::isfinite(value)) {
        return fallbackValue;
    }
    return value;
}

int SpellRingItem::capturedBehaviorParamInt(std::string_view behaviorId, std::string_view key, int fallbackValue) const
{
    const CapturedBehaviorSpec* spec = capturedBehaviorSpec(behaviorId);
    if (spec == nullptr) {
        return fallbackValue;
    }
    const auto it = spec->params.find(std::string(key));
    if (it == spec->params.end()) {
        return fallbackValue;
    }
    int value = fallbackValue;
    if (!parseIntStrict(it->second, value)) {
        return fallbackValue;
    }
    return value;
}

std::string SpellRingItem::capturedBehaviorParamString(std::string_view behaviorId, std::string_view key, std::string_view fallbackValue) const
{
    const CapturedBehaviorSpec* spec = capturedBehaviorSpec(behaviorId);
    if (spec == nullptr) {
        return std::string(fallbackValue);
    }
    const auto it = spec->params.find(std::string(key));
    if (it == spec->params.end()) {
        return std::string(fallbackValue);
    }
    return it->second;
}

bool SpellRingItem::isEnemyLatched(int enemyId) const
{
    for (int latchedId : latchedEnemyIds) {
        if (latchedId == enemyId) {
            return true;
        }
    }
    return false;
}

void SpellRingItem::latchEnemy(int enemyId)
{
    for (int& latchedId : latchedEnemyIds) {
        if (latchedId == enemyId) {
            return;
        }
        if (latchedId == 0) {
            latchedId = enemyId;
            return;
        }
    }
}

void SpellRingItem::unlatchEnemy(int enemyId)
{
    for (int& latchedId : latchedEnemyIds) {
        if (latchedId == enemyId) {
            latchedId = 0;
            return;
        }
    }
}

bool SpellRingItem::consumeDurability(int amount)
{
    if (amount <= 0 || durability < 0) {
        return false;
    }

    durability = std::max(0, durability - amount);
    isBroken = durability == 0;
    return isBroken;
}

bool SpellRingItem::broken() const
{
    return isBroken || durability == 0;
}

SpellRingItem makeShovel()
{
    SpellRingItem item;
    item.type = SpellRingItemType::Shovel;
    item.objectId = "item_shovel";
    item.hitRadius = 11.0f;
    item.damage = 2;
    item.damageType = "blunt";
    item.digPower = 1;
    item.durability = -1;
    item.maxDurability = -1;
    item.hitInterval = 0.14f;
    return item;
}

SpellRingItem makeTorch()
{
    SpellRingItem item;
    item.type = SpellRingItemType::Torch;
    item.objectId = "item_torch";
    item.localAngle = 0.0f;
    item.hitRadius = 13.0f;
    item.damage = 1;
    item.damageType = "fire";
    item.digPower = 0;
    item.durability = -1;
    item.maxDurability = -1;
    item.hitInterval = 0.25f;
    return item;
}

SpellRingItem makeObjectRingItem(std::string_view objectId)
{
    SpellRingItem item;
    item.type = SpellRingItemType::Object;
    item.objectId = std::string(objectId);
    item.hitRadius = 11.0f;
    item.damage = 0;
    item.damageType = "none";
    item.digPower = 0;
    item.durability = -1;
    item.maxDurability = -1;
    item.weight = 1.0f;
    item.hitInterval = 0.26f;
    return item;
}

}
