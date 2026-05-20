#include "game/ItemModel.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace majo {

ItemInstance makeItemInstanceFromDefinition(std::string instanceId, const ObjectDefinition& object)
{
    ItemInstance instance;
    instance.instanceId = std::move(instanceId);
    instance.objectId = object.id;
    instance.currentDurability = object.durability;
    instance.maxDurability = object.durability;
    instance.isBroken = object.durability == 0;
    return instance;
}

ItemVisualRef effectiveItemVisualRef(const ItemData& item)
{
    ItemVisualRef visual = item.visual;
    if (visual.source == ItemVisualSource::Object) {
        if (visual.imageNumber <= 0) {
            visual.imageNumber = item.imageNumber;
        }
        if (visual.sourceId.empty()) {
            visual.sourceId = item.id;
        }
    }
    return visual;
}

ItemData makeMissingItemData(std::string_view objectId)
{
    ItemData item;
    item.id = std::string(objectId);
    item.name = std::string("\xE6\xAC\xA0\xE6\x90\x8D\xE3\x82\xA2\xE3\x82\xA4\xE3\x83\x86\xE3\x83\xA0(") + item.id + ")";
    item.category = "\xE6\xAC\xA0\xE6\x90\x8D";
    item.description = "Objects DB definition is missing. The save keeps this objectId so the item can recover if the DB entry returns.";
    item.rarity = 0;
    item.price = 0;
    item.damageType = "none";
    item.tags = {"missing"};
    return item;
}

std::string_view materialTypeSaveName(MaterialType type)
{
    switch (type) {
    case MaterialType::OldWoodBuildingMaterial:
        return "old_wood_building_material";
    case MaterialType::EnhancementOre:
        return "enhancement_ore";
    case MaterialType::MoonFragment:
        return "moon_fragment";
    case MaterialType::ManaDrop:
        return "mana_drop";
    case MaterialType::Count:
        break;
    }
    return "unknown";
}

bool materialTypeFromSaveName(std::string_view name, MaterialType& outType)
{
    constexpr std::array<MaterialType, static_cast<std::size_t>(MaterialType::Count)> Types{{
        MaterialType::OldWoodBuildingMaterial,
        MaterialType::EnhancementOre,
        MaterialType::MoonFragment,
        MaterialType::ManaDrop,
    }};

    const auto it = std::find_if(Types.begin(), Types.end(), [name](MaterialType type) {
        return materialTypeSaveName(type) == name;
    });
    if (it == Types.end()) {
        return false;
    }

    outType = *it;
    return true;
}

std::string_view materialTypeDisplayName(MaterialType type)
{
    switch (type) {
    case MaterialType::OldWoodBuildingMaterial:
        return "\xE5\x8F\xA4\xE6\x9C\xA8\xE3\x81\xAE\xE5\xBB\xBA\xE6\x9D\x90";
    case MaterialType::EnhancementOre:
        return "\xE5\xBC\xB7\xE5\x8C\x96\xE9\x89\xB1\xE7\x9F\xB3";
    case MaterialType::MoonFragment:
        return "\xE6\x9C\x88\xE3\x81\xAE\xE3\x82\xAB\xE3\x82\xB1\xE3\x83\xA9";
    case MaterialType::ManaDrop:
        return "\xE9\xAD\x94\xE5\x8A\x9B\xE3\x81\xAE\xE3\x81\x97\xE3\x81\x9A\xE3\x81\x8F";
    case MaterialType::Count:
        break;
    }
    return "";
}

}
