#pragma once

#include "data/ObjectCatalog.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

struct StackItem {
    std::string objectId;
    int count = 0;

    bool operator==(const StackItem&) const = default;
};

struct ItemInstance {
    std::string instanceId;
    std::string objectId;
    int currentDurability = -1;
    int maxDurability = -1;
    int enhanceLevel = 0;
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    double weightModifier = 1.0;
    double sizeModifier = 1.0;
    bool protectionEnabled = false;
    bool isBroken = false;
    std::vector<EffectSpec> addedEffects;
    std::vector<std::string> addedTags;

    bool operator==(const ItemInstance&) const = default;
};

enum class MaterialType {
    OldWoodBuildingMaterial,
    EnhancementOre,
    MoonFragment,
    ManaDrop,
    Count,
};

struct MaterialInventory {
    std::array<int, static_cast<std::size_t>(MaterialType::Count)> counts{};

    [[nodiscard]] int count(MaterialType type) const
    {
        return counts[static_cast<std::size_t>(type)];
    }

    void setCount(MaterialType type, int value)
    {
        counts[static_cast<std::size_t>(type)] = value < 0 ? 0 : value;
    }

    void add(MaterialType type, int value)
    {
        if (value <= 0) {
            return;
        }
        setCount(type, count(type) + value);
    }

    [[nodiscard]] bool spend(MaterialType type, int value)
    {
        if (value <= 0 || count(type) < value) {
            return false;
        }
        setCount(type, count(type) - value);
        return true;
    }

    bool operator==(const MaterialInventory&) const = default;
};

ItemInstance makeItemInstanceFromDefinition(std::string instanceId, const ObjectDefinition& object);
ItemData makeMissingItemData(std::string_view objectId);
std::string_view materialTypeSaveName(MaterialType type);
bool materialTypeFromSaveName(std::string_view name, MaterialType& outType);
std::string_view materialTypeDisplayName(MaterialType type);

}
