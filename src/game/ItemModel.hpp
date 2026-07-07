#pragma once

#include "data/ObjectCatalog.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

constexpr int DurabilityUnitsPerPoint = 3;
constexpr int TerrainHitDurabilityCostUnits = 1;
constexpr int FullPointDurabilityCostUnits = DurabilityUnitsPerPoint;

[[nodiscard]] constexpr int durabilityPointsToUnits(int durability)
{
    if (durability < 0) {
        return durability;
    }
    return durability > std::numeric_limits<int>::max() / DurabilityUnitsPerPoint
        ? std::numeric_limits<int>::max()
        : durability * DurabilityUnitsPerPoint;
}

[[nodiscard]] constexpr int durabilityUnitsToDisplayPoints(int durabilityUnits)
{
    if (durabilityUnits < 0) {
        return durabilityUnits;
    }
    return (durabilityUnits + DurabilityUnitsPerPoint - 1) / DurabilityUnitsPerPoint;
}

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
    int attackEnhanceLevel = 0;
    int digEnhanceLevel = 0;
    int durabilityEnhanceLevel = 0;
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
ItemVisualRef effectiveItemVisualRef(const ItemData& item);
ItemData makeMissingItemData(std::string_view objectId);
bool isImportantItem(const ItemData& item);
std::string_view materialTypeSaveName(MaterialType type);
bool materialTypeFromSaveName(std::string_view name, MaterialType& outType);
std::string_view materialTypeDisplayName(MaterialType type);

}
