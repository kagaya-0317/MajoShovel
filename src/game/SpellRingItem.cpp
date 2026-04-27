#include "game/SpellRingItem.hpp"

namespace majo {

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

SpellRingItem makeShovel()
{
    SpellRingItem item;
    item.type = SpellRingItemType::Shovel;
    item.hitRadius = 11.0f;
    item.damage = 2;
    item.digPower = 1;
    item.hitInterval = 0.14f;
    return item;
}

SpellRingItem makeTorch()
{
    SpellRingItem item;
    item.type = SpellRingItemType::Torch;
    item.localAngle = 0.0f;
    item.hitRadius = 13.0f;
    item.damage = 1;
    item.digPower = 0;
    item.hitInterval = 0.25f;
    return item;
}

SpellRingItem makeStone()
{
    SpellRingItem item;
    item.type = SpellRingItemType::Stone;
    item.hitRadius = 10.0f;
    item.damage = 2;
    item.digPower = 0;
    item.weight = 1.25f;
    item.hitInterval = 0.28f;
    return item;
}

SpellRingItem makeOre()
{
    SpellRingItem item;
    item.type = SpellRingItemType::Ore;
    item.hitRadius = 12.0f;
    item.damage = 4;
    item.digPower = 0;
    item.weight = 1.45f;
    item.hitInterval = 0.32f;
    return item;
}

}
