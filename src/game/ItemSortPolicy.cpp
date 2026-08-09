#include "game/ItemSortPolicy.hpp"

#include "data/ObjectCatalog.hpp"

#include <array>
#include <limits>

namespace majo {

namespace {

constexpr std::array<std::string_view, 11> CategoryOrder = {
    "杖",
    "回復",
    "強化",
    "弱体",
    "掘削",
    "探索",
    "軌道",
    "魔導書",
    "武器",
    "盾",
    "宝",
};

int categoryRank(std::string_view category)
{
    for (int i = 0; i < static_cast<int>(CategoryOrder.size()); ++i) {
        if (CategoryOrder[static_cast<std::size_t>(i)] == category) {
            return i;
        }
    }
    return static_cast<int>(CategoryOrder.size());
}

}

ItemSortPolicy::ItemSortPolicy(const ObjectCatalog& catalog)
{
    for (int sourceOrder = 0; sourceOrder < static_cast<int>(catalog.objects.size()); ++sourceOrder) {
        const ObjectDefinition& object = catalog.objects[static_cast<std::size_t>(sourceOrder)];
        if (!object.id.empty()) {
            ranks_.try_emplace(object.id, Rank{categoryRank(object.category), sourceOrder});
        }
    }
}

bool ItemSortPolicy::less(ItemSortSubject left, ItemSortSubject right) const
{
    if (left.equippedStaff != right.equippedStaff) {
        return left.equippedStaff;
    }

    const Rank leftRank = rankFor(left.objectId);
    const Rank rightRank = rankFor(right.objectId);
    if (leftRank.category != rightRank.category) {
        return leftRank.category < rightRank.category;
    }
    if (leftRank.sourceOrder != rightRank.sourceOrder) {
        return leftRank.sourceOrder < rightRank.sourceOrder;
    }
    return left.objectId < right.objectId;
}

ItemSortPolicy::Rank ItemSortPolicy::rankFor(std::string_view objectId) const
{
    const auto it = ranks_.find(objectId);
    if (it != ranks_.end()) {
        return it->second;
    }
    return {
        static_cast<int>(CategoryOrder.size()),
        std::numeric_limits<int>::max(),
    };
}

}
