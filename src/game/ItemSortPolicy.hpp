#pragma once

#include <map>
#include <string>
#include <string_view>

namespace majo {

struct ObjectCatalog;

struct ItemSortSubject {
    std::string_view objectId;
    bool equippedStaff = false;
};

class ItemSortPolicy {
public:
    explicit ItemSortPolicy(const ObjectCatalog& catalog);

    [[nodiscard]] bool less(ItemSortSubject left, ItemSortSubject right) const;

private:
    struct Rank {
        int category = 0;
        int sourceOrder = 0;
    };

    [[nodiscard]] Rank rankFor(std::string_view objectId) const;

    std::map<std::string, Rank, std::less<>> ranks_;
};

}
