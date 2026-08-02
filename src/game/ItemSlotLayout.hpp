#pragma once

#include <span>
#include <vector>

namespace majo {

class ItemSlotLayout {
public:
    void clear();
    void sync(int entryCount, int capacity);

    [[nodiscard]] int entryAtSlot(int slot) const;
    [[nodiscard]] int slotForEntry(int entryIndex) const;
    [[nodiscard]] bool moveEntryToSlot(int entryIndex, int slot, int capacity);

    void insertEntry(int entryIndex, int preferredSlot = -1);
    void eraseEntry(int entryIndex);
    void assignSequential(std::span<const int> entryOrder, int capacity);

private:
    std::vector<int> entrySlots_;
};

} // namespace majo
