#include "game/ItemSlotLayout.hpp"

#include <algorithm>
#include <utility>

namespace majo {

void ItemSlotLayout::clear()
{
    entrySlots_.clear();
}

void ItemSlotLayout::sync(int entryCount, int capacity)
{
    entryCount = std::max(0, entryCount);
    capacity = std::max(0, capacity);
    if (entryCount <= 0 || capacity <= 0) {
        entrySlots_.clear();
        return;
    }

    std::vector<int> nextSlots(static_cast<std::size_t>(entryCount), -1);
    std::vector<bool> used(static_cast<std::size_t>(capacity), false);
    const int copyCount = std::min(entryCount, static_cast<int>(entrySlots_.size()));
    for (int entryIndex = 0; entryIndex < copyCount; ++entryIndex) {
        const int slot = entrySlots_[static_cast<std::size_t>(entryIndex)];
        if (slot >= 0 && slot < capacity && !used[static_cast<std::size_t>(slot)]) {
            nextSlots[static_cast<std::size_t>(entryIndex)] = slot;
            used[static_cast<std::size_t>(slot)] = true;
        }
    }

    int cursor = 0;
    for (int entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
        if (nextSlots[static_cast<std::size_t>(entryIndex)] >= 0) {
            continue;
        }
        while (cursor < capacity && used[static_cast<std::size_t>(cursor)]) {
            ++cursor;
        }
        nextSlots[static_cast<std::size_t>(entryIndex)] =
            cursor < capacity ? cursor : entryIndex % capacity;
        if (cursor < capacity) {
            used[static_cast<std::size_t>(cursor)] = true;
            ++cursor;
        }
    }

    entrySlots_ = std::move(nextSlots);
}

int ItemSlotLayout::entryAtSlot(int slot) const
{
    if (slot < 0) {
        return -1;
    }
    for (int entryIndex = 0; entryIndex < static_cast<int>(entrySlots_.size()); ++entryIndex) {
        if (entrySlots_[static_cast<std::size_t>(entryIndex)] == slot) {
            return entryIndex;
        }
    }
    return -1;
}

int ItemSlotLayout::slotForEntry(int entryIndex) const
{
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entrySlots_.size())) {
        return -1;
    }
    return entrySlots_[static_cast<std::size_t>(entryIndex)];
}

bool ItemSlotLayout::moveEntryToSlot(int entryIndex, int slot, int capacity)
{
    if (entryIndex < 0 ||
        entryIndex >= static_cast<int>(entrySlots_.size()) ||
        slot < 0 ||
        slot >= capacity) {
        return false;
    }

    const int destinationEntry = entryAtSlot(slot);
    if (destinationEntry >= 0 && destinationEntry != entryIndex) {
        std::swap(
            entrySlots_[static_cast<std::size_t>(entryIndex)],
            entrySlots_[static_cast<std::size_t>(destinationEntry)]);
    } else {
        entrySlots_[static_cast<std::size_t>(entryIndex)] = slot;
    }
    return true;
}

void ItemSlotLayout::insertEntry(int entryIndex, int preferredSlot)
{
    entryIndex = std::clamp(entryIndex, 0, static_cast<int>(entrySlots_.size()));
    entrySlots_.insert(entrySlots_.begin() + entryIndex, preferredSlot);
}

void ItemSlotLayout::eraseEntry(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entrySlots_.size())) {
        return;
    }
    entrySlots_.erase(entrySlots_.begin() + entryIndex);
}

void ItemSlotLayout::assignSequential(std::span<const int> entryOrder, int capacity)
{
    capacity = std::max(0, capacity);
    entrySlots_.assign(entryOrder.size(), -1);
    if (capacity <= 0) {
        return;
    }
    for (int slot = 0; slot < static_cast<int>(entryOrder.size()); ++slot) {
        const int entryIndex = entryOrder[static_cast<std::size_t>(slot)];
        if (entryIndex >= 0 && entryIndex < static_cast<int>(entrySlots_.size())) {
            entrySlots_[static_cast<std::size_t>(entryIndex)] = slot % capacity;
        }
    }
}

} // namespace majo
