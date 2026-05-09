#include "game/SpellRingItem.hpp"

#include <algorithm>

namespace majo {

bool SpellRingItem::hasCapturedBehavior(std::string_view behaviorId) const
{
    if (capturedBehaviorId == behaviorId) {
        return true;
    }
    return std::any_of(capturedBehaviorIds.begin(), capturedBehaviorIds.end(), [behaviorId](const std::string& id) {
        return id == behaviorId;
    });
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
    item.damageType = "physical";
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
