#pragma once

#include <algorithm>
#include <array>

namespace majo::storage_rules {

inline constexpr std::array<int, 5> WarehouseCapacities{{48, 72, 100, 140, 200}};
inline constexpr int MaxWarehouseCapacityLevel = static_cast<int>(WarehouseCapacities.size()) - 1;
inline constexpr int MaxWarehouseCapacity = WarehouseCapacities.back();

constexpr int warehouseCapacityForLevel(int level)
{
    const int index = std::clamp(level, 0, MaxWarehouseCapacityLevel);
    return WarehouseCapacities[static_cast<std::size_t>(index)];
}

constexpr int requiredStackSlots(int count, int maxCountPerStack)
{
    if (count <= 0 || maxCountPerStack <= 0) {
        return 0;
    }
    return 1 + (count - 1) / maxCountPerStack;
}

}
