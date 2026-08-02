#pragma once

#include <string>

namespace majo {

enum class ItemContainerKind {
    Backpack,
    Warehouse,
    Ring,
};

struct ItemContainerId {
    ItemContainerKind kind = ItemContainerKind::Backpack;
    int index = -1;

    bool operator==(const ItemContainerId&) const = default;
};

struct ItemKey {
    ItemContainerId container{};
    bool stack = false;
    std::string stableId;
    int fallbackIndex = -1;

    [[nodiscard]] bool valid() const
    {
        return !stableId.empty() ||
            (container.kind == ItemContainerKind::Ring && fallbackIndex >= 0);
    }

    bool operator==(const ItemKey&) const = default;
};

enum class ItemProtectionToggleResult {
    Changed,
    Unsupported,
    Missing,
};

} // namespace majo
