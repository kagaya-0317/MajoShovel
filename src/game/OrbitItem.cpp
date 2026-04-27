#include "game/OrbitItem.hpp"

namespace majo {

OrbitItem makeShovel()
{
    OrbitItem item;
    item.type = OrbitItemType::Shovel;
    item.hitRadius = 11.0f;
    item.damage = 2;
    item.digPower = 1;
    item.hitInterval = 0.14f;
    return item;
}

OrbitItem makeTorch()
{
    OrbitItem item;
    item.type = OrbitItemType::Torch;
    item.localAngle = 0.0f;
    item.hitRadius = 13.0f;
    item.damage = 1;
    item.digPower = 0;
    item.hitInterval = 0.25f;
    return item;
}

}
