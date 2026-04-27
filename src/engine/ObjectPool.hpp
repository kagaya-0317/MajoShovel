#pragma once

#include <array>
#include <cstddef>

namespace majo {

template <typename T, std::size_t Capacity>
class ObjectPool {
public:
    T* acquire()
    {
        for (auto& item : items_) {
            if (!item.active) {
                item = T{};
                item.active = true;
                return &item;
            }
        }
        return nullptr;
    }

    std::array<T, Capacity>& items() { return items_; }
    const std::array<T, Capacity>& items() const { return items_; }

    int activeCount() const
    {
        int count = 0;
        for (const auto& item : items_) {
            if (item.active) {
                ++count;
            }
        }
        return count;
    }

private:
    std::array<T, Capacity> items_{};
};

}
