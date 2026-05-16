#include "game/GameInternal.hpp"

namespace majo {

bool Game::isSellableObject(const ItemData& item) const
{
    return !isStoryObject(item);
}

bool Game::isStoryObject(const ItemData& item) const
{
    if (item.category == "\xE3\x82\xB9\xE3\x83\x88\xE3\x83\xBC\xE3\x83\xAA\xE3\x83\xBC") {
        return true;
    }
    for (const std::string& tag : item.tags) {
        if (tag == "story" || tag == "story_item" || tag == "key_item" || tag == "unsellable" || tag == "no_sell") {
            return true;
        }
    }
    return false;
}

int Game::sellPrice(const ItemData& item, const ItemInstance* /*instance*/) const
{
    // Future hooks: enhancement bonus, broken penalty, category multiplier, merchant buy-rate.
    double multiplier = 1.0;
    if (merchantUpgradeLevel_ >= 6) {
        multiplier = 1.2;
    } else if (merchantUpgradeLevel_ >= 3) {
        multiplier = 1.1;
    }
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(item.price) * multiplier)));
}

bool Game::merchantProductCanFit(const ItemData* item) const
{
    if (item == nullptr) {
        return false;
    }
    const auto& stacks = inventory_.objectStacks();
    const bool existingStack = std::any_of(stacks.begin(), stacks.end(), [&](const InventoryObjectStack& stack) {
        return stack.objectId == item->id;
    });
    return existingStack || backpackUsedSlots() < inventory_.screenSlotCount();
}

bool Game::canBuyMerchantProduct(const MerchantProduct& product) const
{
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    return product.quantity > 0 && item != nullptr && money_ >= product.price && merchantProductCanFit(item);
}

std::vector<Game::SellableEntry> Game::sellableObjects() const
{
    std::vector<SellableEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(i)];
        if (stack.count <= 0) {
            continue;
        }
        SellableEntry entry{SellableKind::Stack, i};
        entry.price = sellPrice(stack.item);
        entry.sellable = true;
        entries.push_back(std::move(entry));
    }
    const auto& instances = inventory_.objectInstances();
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(i)];
        SellableEntry entry{SellableKind::Instance, i};
        entry.price = sellPrice(instance.item, &instance.instance);
        entry.sellable = !instance.instance.protectionEnabled;
        if (!entry.sellable) {
            entry.blockedReason = "保護中";
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

void Game::refreshMerchantStock(bool force)
{
    if (!force && !merchantStock_.empty()) {
        return;
    }

    std::vector<const ItemData*> candidates;
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const ItemData* item = objectCatalog_.registry.findById(object.id);
        if (item == nullptr || item->id.empty() || isStoryObject(*item)) {
            continue;
        }
        const bool basicCategory =
            item->category == "\xE5\x9B\x9E\xE5\xBE\xA9" ||
            item->category == "\xE6\x8E\xA2\xE7\xB4\xA2" ||
            item->category == "\xE5\xBC\xB7\xE5\x8C\x96";
        const bool basicTag = std::any_of(item->tags.begin(), item->tags.end(), [](const std::string& tag) {
            return tag == "consumable" || tag == "potion" || tag == "food";
        });
        if ((basicCategory || basicTag) && item->price > 0) {
            candidates.push_back(item);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const ItemData* left, const ItemData* right) {
        if (left->price != right->price) {
            return left->price < right->price;
        }
        return left->id < right->id;
    });

    merchantStock_.clear();
    if (candidates.empty()) {
        return;
    }

    ++merchantStockVersion_;
    const int stockCount = std::min(merchantUpgradeLevel_ >= 2 ? 5 : 4, static_cast<int>(candidates.size()));
    const int start = merchantStockVersion_ % static_cast<int>(candidates.size());
    std::mt19937& rng = lootRuntimeRng();
    std::uniform_int_distribution<int> quantityDistribution(1, 5);
    for (int i = 0; i < stockCount; ++i) {
        const ItemData* item = candidates[static_cast<std::size_t>((start + i) % static_cast<int>(candidates.size()))];
        merchantStock_.push_back(MerchantProduct{item->id, std::max(1, item->price), quantityDistribution(rng)});
    }
}

void Game::sellMerchantEntry(int index, int count)
{
    const std::vector<SellableEntry> sellable = sellableObjects();
    if (index < 0 || index >= static_cast<int>(sellable.size())) {
        baseStatus_ = "売却対象がありません";
        return;
    }

    const SellableEntry entry = sellable[static_cast<std::size_t>(index)];
    if (!entry.sellable) {
        baseStatus_ = entry.blockedReason.empty() ? "売れません" : entry.blockedReason;
        return;
    }

    bool sold = false;
    int soldCount = 1;
    if (entry.kind == SellableKind::Stack) {
        const auto& stacks = inventory_.objectStacks();
        if (entry.index < 0 || entry.index >= static_cast<int>(stacks.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(entry.index)];
        soldCount = count <= 0 ? stack.count : std::min(count, stack.count);
        sold = inventory_.removeObjectItemCount(stack.objectId, soldCount);
    } else {
        const auto& instances = inventory_.objectInstances();
        if (entry.index < 0 || entry.index >= static_cast<int>(instances.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(entry.index)];
        sold = inventory_.removeObjectInstance(instance.instance.instanceId);
    }

    if (sold) {
        money_ += entry.price * std::max(1, soldCount);
        baseStatus_ = "売却しました";
        baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, static_cast<int>(sellableObjects().size()) - 1));
    }
}

void Game::sellMerchantScreenSlot(int slotIndex, int count)
{
    if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
        baseStatus_ = "売却対象がありません";
        return;
    }

    if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slotIndex)) {
        const int soldCount = count <= 0 ? stack->count : std::min(count, stack->count);
        const std::string objectId = stack->objectId;
        const int price = sellPrice(stack->item) * std::max(1, soldCount);
        if (inventory_.removeObjectItemCount(objectId, soldCount)) {
            money_ += price;
            baseStatus_ = "売却しました";
        }
        return;
    }

    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slotIndex)) {
        if (instance->instance.protectionEnabled) {
            baseStatus_ = "保護中";
            return;
        }
        const std::string instanceId = instance->instance.instanceId;
        const int price = sellPrice(instance->item, &instance->instance);
        if (inventory_.removeObjectInstance(instanceId)) {
            money_ += price;
            baseStatus_ = "売却しました";
        }
        return;
    }

    baseStatus_ = "売却対象がありません";
}

void Game::buyMerchantProduct(int index)
{
    refreshMerchantStock(false);
    if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
        baseStatus_ = "購入できる商品がありません";
        return;
    }

    MerchantProduct& product = merchantStock_[static_cast<std::size_t>(index)];
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    if (item == nullptr) {
        baseStatus_ = "商品データがありません";
        return;
    }
    if (product.quantity <= 0) {
        baseStatus_ = "品切れです";
        return;
    }
    if (money_ < product.price) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (!merchantProductCanFit(item)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    if (!inventory_.addObjectItem(objectCatalog_, product.objectId)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    money_ -= product.price;
    --product.quantity;
    baseStatus_ = product.quantity <= 0 ? "購入しました（品切れ）" : "購入しました";
}

std::vector<Game::StorageEntry> Game::processingEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

std::optional<Game::StorageEntry> Game::processingEntryForScreenSlot(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
        return std::nullopt;
    }
    if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slotIndex)) {
        const auto& stacks = inventory_.objectStacks();
        for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
            if (stacks[static_cast<std::size_t>(i)].objectId == stack->objectId) {
                return StorageEntry{StorageEntryKind::Stack, i};
            }
        }
    }
    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slotIndex)) {
        const auto& instances = inventory_.objectInstances();
        for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
            if (instances[static_cast<std::size_t>(i)].instance.instanceId == instance->instance.instanceId) {
                return StorageEntry{StorageEntryKind::Instance, i};
            }
        }
    }
    return std::nullopt;
}

const char* Game::processingModeName(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Repair: return "修理";
    case ProcessingMode::Attack: return "攻撃力強化";
    case ProcessingMode::Dig: return "抑制力強化";
    case ProcessingMode::Durability: return "耐久力強化";
    }
    return "";
}

bool Game::processingEntryAvailable(StorageEntry entry) const
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    if (entry.kind == StorageEntryKind::Stack) {
        return mode != ProcessingMode::Repair;
    }
    const InventoryObjectInstance& object = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    if (mode == ProcessingMode::Repair) {
        const ItemInstance& instance = object.instance;
        return instance.maxDurability >= 0 && (instance.isBroken || instance.currentDurability < instance.maxDurability);
    }
    return object.instance.enhanceLevel < MaxItemEnhanceLevel;
}

bool Game::processingScreenSlotAvailable(int slotIndex) const
{
    const std::optional<StorageEntry> entry = processingEntryForScreenSlot(slotIndex);
    return entry.has_value() && processingEntryAvailable(*entry);
}

int Game::processingMoneyCost(StorageEntry entry, ProcessingMode mode) const
{
    const ItemData* item = storageEntryItem(entry, false);
    const int basePrice = std::max(1, item != nullptr ? item->price : 0);
    if (mode == ProcessingMode::Repair) {
        const ItemInstance* instance = storageEntryInstance(entry, false);
        if (instance == nullptr || instance->maxDurability <= 0) {
            return 0;
        }
        if (instance->isBroken) {
            return std::max(1, static_cast<int>(std::ceil(static_cast<double>(basePrice) * 0.6)));
        }
        const int missing = std::max(0, instance->maxDurability - instance->currentDurability);
        if (missing <= 0) {
            return 0;
        }
        const double ratio = static_cast<double>(missing) / static_cast<double>(instance->maxDurability);
        return std::max(1, static_cast<int>(std::ceil(static_cast<double>(basePrice) * ratio * 0.4)));
    }

    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, false)) {
        enhanceLevel = instance->enhanceLevel;
    }
    return std::max(20, basePrice / 2 + (enhanceLevel + 1) * 50);
}

int Game::processingOreCost(StorageEntry entry, ProcessingMode mode) const
{
    if (mode == ProcessingMode::Repair) {
        return 0;
    }
    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, false)) {
        enhanceLevel = instance->enhanceLevel;
    }
    return enhanceLevel + 1;
}

void Game::applyProcessing(int entryIndex)
{
    const std::vector<StorageEntry> entries = processingEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    applyProcessingEntry(entry);
}

void Game::applyProcessingScreenSlot(int slotIndex)
{
    const std::optional<StorageEntry> entry = processingEntryForScreenSlot(slotIndex);
    if (!entry) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    applyProcessingEntry(*entry);
}

void Game::applyProcessingEntry(StorageEntry entry)
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    if (!processingEntryAvailable(entry)) {
        if (mode == ProcessingMode::Repair && entry.kind == StorageEntryKind::Stack) {
            baseStatus_ = "この作業はできません";
        } else {
            baseStatus_ = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
        }
        return;
    }

    const int moneyCost = processingMoneyCost(entry, mode);
    const int oreCost = processingOreCost(entry, mode);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りません";
        return;
    }

    bool processed = false;
    if (mode == ProcessingMode::Repair) {
        const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
        processed = inventory_.repairObjectInstance(instance.instance.instanceId);
    } else {
        int attackBonus = 0;
        int digBonus = 0;
        int durabilityBonus = 0;
        if (mode == ProcessingMode::Attack) {
            attackBonus = 1;
        } else if (mode == ProcessingMode::Dig) {
            digBonus = 1;
        } else if (mode == ProcessingMode::Durability) {
            durabilityBonus = 2;
        }
        if (entry.kind == StorageEntryKind::Stack) {
            const InventoryObjectStack& stack = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectStackItem(stack.objectId, attackBonus, digBonus, durabilityBonus, MaxItemEnhanceLevel);
        } else {
            const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectInstance(instance.instance.instanceId, attackBonus, digBonus, durabilityBonus, MaxItemEnhanceLevel);
        }
    }
    if (!processed) {
        baseStatus_ = "加工できません";
        return;
    }

    money_ -= moneyCost;
    if (oreCost > 0) {
        const bool spentOre = inventory_.materials().spend(MaterialType::EnhancementOre, oreCost);
        (void)spentOre;
    }
    baseStatus_ = mode == ProcessingMode::Repair ? "修理しました" : "強化しました";
    baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
}

int Game::warehouseCapacity() const
{
    constexpr std::array<int, 5> Capacities{{48, 72, 100, 140, 200}};
    const int level = std::clamp(warehouseCapacityLevel_, 0, static_cast<int>(Capacities.size()) - 1);
    return Capacities[static_cast<std::size_t>(level)];
}

int Game::warehouseUsedSlots() const
{
    return static_cast<int>(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
}

int Game::backpackUsedSlots() const
{
    return static_cast<int>(inventory_.objectStacks().size() + inventory_.objectInstances().size());
}

std::vector<Game::StorageEntry> Game::backpackStorageEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

std::vector<Game::StorageEntry> Game::warehouseStorageEntries() const
{
    std::vector<StorageEntry> entries;
    entries.reserve(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
    for (int i = 0; i < static_cast<int>(warehouseObjectStacks_.size()); ++i) {
        if (warehouseObjectStacks_[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(warehouseObjectInstances_.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

void Game::syncWarehouseDisplaySlots() const
{
    const int totalCount = warehouseUsedSlots();
    if (totalCount <= 0) {
        warehouseDisplaySlots_.clear();
        return;
    }

    const int capacity = warehouseCapacity();
    std::vector<int> nextSlots(static_cast<std::size_t>(totalCount), -1);
    std::vector<bool> used(static_cast<std::size_t>(capacity), false);
    const int copyCount = std::min(totalCount, static_cast<int>(warehouseDisplaySlots_.size()));
    for (int i = 0; i < copyCount; ++i) {
        const int slot = warehouseDisplaySlots_[static_cast<std::size_t>(i)];
        if (slot >= 0 && slot < capacity && !used[static_cast<std::size_t>(slot)]) {
            nextSlots[static_cast<std::size_t>(i)] = slot;
            used[static_cast<std::size_t>(slot)] = true;
        }
    }

    int cursor = 0;
    for (int i = 0; i < totalCount; ++i) {
        if (nextSlots[static_cast<std::size_t>(i)] >= 0) {
            continue;
        }
        while (cursor < capacity && used[static_cast<std::size_t>(cursor)]) {
            ++cursor;
        }
        if (cursor >= capacity) {
            nextSlots[static_cast<std::size_t>(i)] = i % capacity;
        } else {
            nextSlots[static_cast<std::size_t>(i)] = cursor;
            used[static_cast<std::size_t>(cursor)] = true;
            ++cursor;
        }
    }
    warehouseDisplaySlots_ = std::move(nextSlots);
}

int Game::warehouseEntryIndexAtStorageSlot(int slot) const
{
    syncWarehouseDisplaySlots();
    if (slot < 0 || slot >= warehouseCapacity()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(warehouseDisplaySlots_.size()); ++i) {
        if (warehouseDisplaySlots_[static_cast<std::size_t>(i)] == slot) {
            return i;
        }
    }
    return -1;
}

void Game::assignWarehouseEntryToStorageSlot(int entryIndex, int slot)
{
    syncWarehouseDisplaySlots();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseDisplaySlots_.size()) || slot < 0 || slot >= warehouseCapacity()) {
        return;
    }
    for (int i = 0; i < static_cast<int>(warehouseDisplaySlots_.size()); ++i) {
        if (i != entryIndex && warehouseDisplaySlots_[static_cast<std::size_t>(i)] == slot) {
            std::swap(warehouseDisplaySlots_[static_cast<std::size_t>(i)], warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)]);
            return;
        }
    }
    warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)] = slot;
}

void Game::removeWarehouseDisplaySlotAtEntryIndex(int entryIndex)
{
    syncWarehouseDisplaySlots();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseDisplaySlots_.size())) {
        return;
    }
    warehouseDisplaySlots_.erase(warehouseDisplaySlots_.begin() + entryIndex);
}

void Game::depositBackpackEntry(int entryIndex)
{
    const std::vector<StorageEntry> entries = backpackStorageEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "預けるアイテムがありません";
        return;
    }

    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    if (entry.kind == StorageEntryKind::Stack) {
        const InventoryObjectStack& source = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
        auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [&source](const InventoryObjectStack& stack) {
            return stack.objectId == source.objectId;
        });
        if (it == warehouseObjectStacks_.end()) {
            if (warehouseUsedSlots() >= warehouseCapacity()) {
                baseStatus_ = "倉庫がいっぱいです";
                return;
            }
            syncWarehouseDisplaySlots();
            const int newStackIndex = static_cast<int>(warehouseObjectStacks_.size());
            warehouseDisplaySlots_.insert(warehouseDisplaySlots_.begin() + newStackIndex, -1);
            warehouseObjectStacks_.push_back(InventoryObjectStack{source.item, 0});
            it = warehouseObjectStacks_.end() - 1;
        }
        const std::string objectId = source.objectId;
        if (!inventory_.removeObjectItemCount(objectId, 1)) {
            baseStatus_ = "預け入れに失敗しました";
            return;
        }
        ++it->count;
        baseStatus_.clear();
        baseStorageBackpackCursor_ = std::clamp(baseStorageBackpackCursor_, 0, StoragePaneSlotCount - 1);
        return;
    }

    if (warehouseUsedSlots() >= warehouseCapacity()) {
        baseStatus_ = "倉庫がいっぱいです";
        return;
    }
    const InventoryObjectInstance& source = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    InventoryObjectInstance moved;
    if (!inventory_.takeObjectInstance(source.instance.instanceId, moved)) {
        baseStatus_ = "預け入れに失敗しました";
        return;
    }
    warehouseObjectInstances_.push_back(std::move(moved));
    baseStatus_.clear();
    baseStorageBackpackCursor_ = std::clamp(baseStorageBackpackCursor_, 0, StoragePaneSlotCount - 1);
}

void Game::withdrawWarehouseEntry(int entryIndex)
{
    const std::vector<StorageEntry> entries = warehouseStorageEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "取り出すアイテムがありません";
        return;
    }

    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    if (entry.kind == StorageEntryKind::Stack) {
        InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(entry.index)];
        const std::string objectId = stack.objectId;
        if (!inventory_.addObjectItem(objectCatalog_, objectId)) {
            baseStatus_ = "リュックがいっぱいです";
            return;
        }
        --stack.count;
        if (stack.count <= 0) {
            removeWarehouseDisplaySlotAtEntryIndex(entry.index);
            warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + entry.index);
        }
        baseStatus_.clear();
        baseStorageWarehouseCursor_ = std::clamp(baseStorageWarehouseCursor_, 0, StoragePaneSlotCount - 1);
        return;
    }

    InventoryObjectInstance moved = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)];
    if (!inventory_.addObjectInstance(objectCatalog_, moved.instance)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + entry.index);
    warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + entry.index);
    baseStatus_.clear();
    baseStorageWarehouseCursor_ = std::clamp(baseStorageWarehouseCursor_, 0, StoragePaneSlotCount - 1);
}

std::string Game::storageEntryLabel(StorageEntry entry, bool warehouseEntry) const
{
    char buffer[192];
    if (entry.kind == StorageEntryKind::Stack) {
        const InventoryObjectStack& stack = warehouseEntry
            ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)]
            : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
        std::snprintf(buffer, sizeof(buffer), "%s x%d", stack.item.name.c_str(), stack.count);
        return buffer;
    }

    const InventoryObjectInstance& instance = warehouseEntry
        ? warehouseObjectInstances_[static_cast<std::size_t>(entry.index)]
        : inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    std::snprintf(buffer, sizeof(buffer), "%s %s%s Lv.%d",
        instance.item.name.c_str(),
        instance.instance.protectionEnabled ? "[保護] " : "",
        instance.instance.isBroken ? "[破損] " : "",
        instance.instance.enhanceLevel);
    return buffer;
}

const ItemData* Game::storageEntryItem(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind == StorageEntryKind::Stack) {
        return warehouseEntry
            ? &warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].item
            : &inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].item;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].item
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].item;
}

const ItemInstance* Game::storageEntryInstance(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Instance) {
        return nullptr;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].instance;
}

int Game::storageEntryStackCount(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Stack) {
        return 1;
    }
    return warehouseEntry
        ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].count
        : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].count;
}

int Game::upgradeCost(int index) const
{
    switch (index) {
    case 0: return 150 + warehouseCapacityLevel_ * 100;
    case 1: return 120 + (merchantUpgradeLevel_ - 1) * 120;
    case 2: return 180 + processingUnlockLevel_ * 140;
    case 3: return 300;
    case 4: return 100 + maxHpUpgradeLevel_ * 50;
    case 5: return 150 + ringRadiusUpgradeLevel_ * 75;
    case 6: return 150 + ringSpeedUpgradeLevel_ * 75;
    default: return 0;
    }
}

MaterialType Game::upgradeMaterialType(int index) const
{
    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
        return MaterialType::OldWoodBuildingMaterial;
    case 4:
    case 5:
    case 6:
    case 7:
        return MaterialType::ManaDrop;
    default:
        return MaterialType::OldWoodBuildingMaterial;
    }
}

int Game::upgradeMaterialCost(int index) const
{
    switch (index) {
    case 0: return warehouseCapacityLevel_ + 2;
    case 1: return merchantUpgradeLevel_ + 1;
    case 2: return processingUnlockLevel_ + 2;
    case 3: return ringWorkshopUnlocked_ ? 0 : 5;
    case 4: return maxHpUpgradeLevel_ + 1;
    case 5: return ringRadiusUpgradeLevel_ + 1;
    case 6: return ringSpeedUpgradeLevel_ + 1;
    default: return 0;
    }
}

const char* Game::upgradeName(int index) const
{
    switch (index) {
    case 0: return "倉庫容量強化";
    case 1: return "商人機能強化";
    case 2: return "作業台機能解禁";
    case 3: return "リング工房解禁";
    case 4: return "最大HPアップ";
    case 5: return "リング半径アップ";
    case 6: return "リング速度アップ";
    case 7: return "プレイ性能強化枠";
    default: return "";
    }
}

int Game::upgradeLevel(int index) const
{
    switch (index) {
    case 0: return warehouseCapacityLevel_ + 1;
    case 1: return merchantUpgradeLevel_;
    case 2: return processingUnlockLevel_;
    case 3: return ringWorkshopUnlocked_ ? 1 : 0;
    case 4: return maxHpUpgradeLevel_;
    case 5: return ringRadiusUpgradeLevel_;
    case 6: return ringSpeedUpgradeLevel_;
    case 7: return 0;
    default: return 0;
    }
}

int Game::upgradeMaxLevel(int index) const
{
    switch (index) {
    case 0: return 5;
    case 1: return 7;
    case 2: return 5;
    case 3: return 1;
    case 4:
    case 5:
    case 6:
        return 5;
    default:
        return 0;
    }
}

bool Game::upgradeImplemented(int index) const
{
    return index >= 0 && index <= 6;
}

bool Game::upgradeMaxed(int index) const
{
    const int maxLevel = upgradeMaxLevel(index);
    return maxLevel <= 0 || upgradeLevel(index) >= maxLevel;
}

void Game::buyUpgrade(int index)
{
    if (!upgradeImplemented(index)) {
        baseStatus_ = "この強化枠は未実装です";
        return;
    }
    if (upgradeMaxed(index)) {
        baseStatus_ = "強化上限です";
        return;
    }
    const int cost = upgradeCost(index);
    if (cost <= 0) {
        return;
    }
    if (money_ < cost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    const MaterialType materialType = upgradeMaterialType(index);
    const int materialCost = upgradeMaterialCost(index);
    if (materialCost > 0 && inventory_.materialCount(materialType) < materialCost) {
        baseStatus_ = std::string(materialTypeDisplayName(materialType)) + "が足りません";
        return;
    }

    money_ -= cost;
    if (materialCost > 0) {
        const bool spent = inventory_.materials().spend(materialType, materialCost);
        (void)spent;
    }
    switch (index) {
    case 0:
        ++warehouseCapacityLevel_;
        break;
    case 1:
        ++merchantUpgradeLevel_;
        refreshMerchantStock(true);
        break;
    case 2:
        ++processingUnlockLevel_;
        break;
    case 3:
        ringWorkshopUnlocked_ = true;
        break;
    case 4:
        ++maxHpUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 5:
        ++ringRadiusUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 6:
        ++ringSpeedUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    default:
        break;
    }
    baseStatus_ = "強化しました";
}

void Game::openRingWorkshop()
{
    if (!ringWorkshopUnlocked_) {
        baseStatus_ = "リング工房はまだ解禁されていません";
        return;
    }
    baseRingWorkshopActive_ = true;
    baseRingWorkshopSelection_ = 0;
    resetRingWorkshopDraft();
    baseStatus_.clear();
}

void Game::resetRingWorkshopDraft()
{
    ringWorkshopDraftRadiusPoints_ = levelRingRadiusPoints_;
    ringWorkshopDraftSpeedPoints_ = levelRingSpeedPoints_;
}

int Game::ringLevelUpgradePointTotal() const
{
    return std::max(0, levelRingRadiusPoints_) + std::max(0, levelRingSpeedPoints_);
}

bool Game::ringWorkshopRespecChanged() const
{
    return ringWorkshopDraftRadiusPoints_ != levelRingRadiusPoints_ ||
        ringWorkshopDraftSpeedPoints_ != levelRingSpeedPoints_;
}

int Game::ringWorkshopRespecMoneyCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    return 80 + ringLevelUpgradePointTotal() * 20;
}

int Game::ringWorkshopRespecMoonCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    return 1 + ringLevelUpgradePointTotal() / 3;
}

bool Game::adjustRingWorkshopRespec(int direction)
{
    if (ringLevelUpgradePointTotal() <= 0) {
        baseStatus_ = "再調整できるリング強化ポイントがありません";
        return false;
    }
    if (direction > 0) {
        if (ringWorkshopDraftSpeedPoints_ <= 0) {
            baseStatus_ = "速度から移せるポイントがありません";
            return false;
        }
        --ringWorkshopDraftSpeedPoints_;
        ++ringWorkshopDraftRadiusPoints_;
    } else {
        if (ringWorkshopDraftRadiusPoints_ <= 0) {
            baseStatus_ = "半径から移せるポイントがありません";
            return false;
        }
        --ringWorkshopDraftRadiusPoints_;
        ++ringWorkshopDraftSpeedPoints_;
    }
    baseStatus_ = "配分案を変更しました。確定で支払います";
    return true;
}

void Game::confirmRingWorkshopRespec()
{
    if (!ringWorkshopRespecChanged()) {
        baseStatus_ = "配分は変更されていません";
        return;
    }
    const int moneyCost = ringWorkshopRespecMoneyCost();
    const int moonCost = ringWorkshopRespecMoonCost();
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    levelRingRadiusPoints_ = std::max(0, ringWorkshopDraftRadiusPoints_);
    levelRingSpeedPoints_ = std::max(0, ringWorkshopDraftSpeedPoints_);
    applyPermanentUpgrades();
    baseStatus_ = "リング強化の配分を再調整しました";
}

const char* Game::ringWorkshopUpgradeName(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return "初期リング半径強化";
    case RingWorkshopUpgrade::InitialSpeed:
        return "初期リング速度強化";
    case RingWorkshopUpgrade::ShiftDistance:
        return "ずらし距離強化";
    }
    return "";
}

int Game::ringWorkshopUpgradeLevel(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return workshopInitialRadiusLevel_;
    case RingWorkshopUpgrade::InitialSpeed:
        return workshopInitialSpeedLevel_;
    case RingWorkshopUpgrade::ShiftDistance:
        return workshopShiftDistanceLevel_;
    }
    return 0;
}

int Game::ringWorkshopUpgradeMaxLevel(RingWorkshopUpgrade) const
{
    return 5;
}

int Game::ringWorkshopUpgradeMoneyCost(RingWorkshopUpgrade upgrade) const
{
    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    return 120 + level * 90;
}

int Game::ringWorkshopUpgradeMoonCost(RingWorkshopUpgrade upgrade) const
{
    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    return level + 1;
}

float Game::ringWorkshopUpgradeCurrentValue(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return effectiveInitialRingRadius(levelRingRadiusPoints_);
    case RingWorkshopUpgrade::InitialSpeed:
        return effectiveInitialRingSpeed(levelRingSpeedPoints_);
    case RingWorkshopUpgrade::ShiftDistance:
        return effectiveRingShiftDistance();
    }
    return 0.0f;
}

float Game::ringWorkshopUpgradeNextValue(RingWorkshopUpgrade upgrade) const
{
    const int currentLevel = ringWorkshopUpgradeLevel(upgrade);
    if (currentLevel >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return ringWorkshopUpgradeCurrentValue(upgrade);
    }
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius: {
        const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringRadiusUpgradeLevel_) * 0.08f;
        const float workshopMultiplier = 1.0f + static_cast<float>(currentLevel + 1) * 0.05f;
        const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelRingRadiusPoints_)));
        return balance_.spellRingRadius * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
    }
    case RingWorkshopUpgrade::InitialSpeed: {
        const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringSpeedUpgradeLevel_) * 0.08f;
        const float workshopMultiplier = 1.0f + static_cast<float>(currentLevel + 1) * 0.05f;
        const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelRingSpeedPoints_)));
        return balance_.spellRingSpeed * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
    }
    case RingWorkshopUpgrade::ShiftDistance:
        return balance_.spellRingShiftDistance + static_cast<float>(currentLevel + 1) * 8.0f;
    }
    return 0.0f;
}

void Game::buyRingWorkshopUpgrade(RingWorkshopUpgrade upgrade)
{
    if (ringWorkshopUpgradeLevel(upgrade) >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        baseStatus_ = "この強化は上限です";
        return;
    }
    const int moneyCost = ringWorkshopUpgradeMoneyCost(upgrade);
    const int moonCost = ringWorkshopUpgradeMoonCost(upgrade);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        ++workshopInitialRadiusLevel_;
        break;
    case RingWorkshopUpgrade::InitialSpeed:
        ++workshopInitialSpeedLevel_;
        break;
    case RingWorkshopUpgrade::ShiftDistance:
        ++workshopShiftDistanceLevel_;
        break;
    }
    applyPermanentUpgrades();
    resetRingWorkshopDraft();
    baseStatus_ = "リング工房強化を行いました";
}

void Game::openBookshelf()
{
    baseBookshelfActive_ = true;
    bookshelfPage_ = BookshelfPage::Menu;
    bookshelfSelection_ = 0;
    baseStatus_.clear();
    syncEncyclopediaFromInventoryAndRing();
}

void Game::syncEncyclopediaFromInventoryAndRing()
{
    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        if (!stack.objectId.empty() && stack.count > 0) {
            encyclopedia_.noteItemObtained(stack.item, player_.position);
        }
    }
    for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
        if (!objectInstance.item.id.empty()) {
            encyclopedia_.noteItemObtained(objectInstance.item, player_.position);
        }
    }
    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->objectId.empty()) {
            continue;
        }
        const ObjectDefinition* object = objectCatalog_.registry.findById(itemPtr->objectId);
        if (object != nullptr) {
            encyclopedia_.noteItemEquipped(*object, itemPtr->worldPosition);
        }
    }
}

void Game::applyEffectDiscoveries(const std::vector<EffectDiscoveryEvent>& discoveries)
{
    for (const EffectDiscoveryEvent& discovery : discoveries) {
        if (!encyclopedia_.discoverObjectEffect(
                discovery.objectId,
                discovery.effectKey,
                objectCatalog_,
                discovery.position,
                discovery.note)) {
            encyclopedia_.noteEffectEvent(discovery, objectCatalog_);
        }
    }
}

void Game::addStoryFlag(std::string flag)
{
    if (flag.empty()) {
        return;
    }
    if (std::find(storyFlags_.begin(), storyFlags_.end(), flag) == storyFlags_.end()) {
        storyFlags_.push_back(std::move(flag));
    }
}

void Game::startBaseMonicaDialogue()
{
    baseStatus_.clear();
    dialogue_.start(baseMonicaDialogue());
}

void Game::maybeStartFirstDungeonDialogue()
{
    constexpr std::string_view Flag = "dialogue_first_dungeon";
    const bool alreadySeen = std::find(storyFlags_.begin(), storyFlags_.end(), std::string(Flag)) != storyFlags_.end();
    if (alreadySeen) {
        return;
    }

    addStoryFlag(std::string(Flag));
    dialogue_.start(firstDungeonDialogue());
}

void Game::updateBookshelfScreen(const Input& input, UiContext& ui)
{
    const auto objectCountForPage = [this](BookshelfPage page) {
        int count = 0;
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((page == BookshelfPage::Treasures && treasure) ||
                (page == BookshelfPage::Items && !treasure)) {
                ++count;
            }
        }
        return count;
    };

    if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
        if (bookshelfPage_ == BookshelfPage::Menu) {
            baseBookshelfActive_ = false;
            baseStatus_.clear();
        } else {
            bookshelfPage_ = BookshelfPage::Menu;
            bookshelfSelection_ = 0;
        }
        return;
    }

    const int itemCount = bookshelfPage_ == BookshelfPage::Menu
        ? BookshelfMenuItemCount
        : (bookshelfPage_ == BookshelfPage::Enemies ? static_cast<int>(enemyCatalog_.enemies.size()) : objectCountForPage(bookshelfPage_));
    if (itemCount <= 0) {
        bookshelfSelection_ = 0;
    } else {
        if (input.pressed(InputAction::MoveUp)) {
            bookshelfSelection_ = (bookshelfSelection_ + itemCount - 1) % itemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            bookshelfSelection_ = (bookshelfSelection_ + 1) % itemCount;
        }
        bookshelfSelection_ = std::clamp(bookshelfSelection_, 0, itemCount - 1);
    }

    const int visibleCount = std::min(BookshelfVisibleRows, itemCount);
    const int firstVisibleIndex = std::clamp(bookshelfSelection_ - visibleCount / 2, 0, std::max(0, itemCount - visibleCount));
    for (int i = 0; i < visibleCount; ++i) {
        const UiRect rect = bookshelfItemRect(i);
        const int itemIndex = firstVisibleIndex + i;
        if (rect.contains(ui.mouse())) {
            bookshelfSelection_ = itemIndex;
        }
        if (ui.pressed(rect)) {
            bookshelfSelection_ = itemIndex;
            if (bookshelfPage_ == BookshelfPage::Menu) {
                bookshelfPage_ = static_cast<BookshelfPage>(bookshelfSelection_ + 1);
                bookshelfSelection_ = 0;
            }
            return;
        }
    }

    if ((input.confirmPressed() || input.useItemPressed()) && bookshelfPage_ == BookshelfPage::Menu) {
        bookshelfPage_ = static_cast<BookshelfPage>(bookshelfSelection_ + 1);
        bookshelfSelection_ = 0;
        return;
    }

    ui.block(basePanelRect());
}

void Game::updateBaseScreen(const Input& input, UiContext& ui, float dt)
{
    updatePlayerFootstepDust(dt);

    if (baseEditEnabled_) {
        updateBaseEditScreen(input, ui, dt);
        return;
    }

    if (baseBookshelfActive_) {
        updateBookshelfScreen(input, ui);
        return;
    }

    if (baseRingWorkshopActive_) {
        if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
            baseRingWorkshopActive_ = false;
            resetRingWorkshopDraft();
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + BaseRingWorkshopItemCount - 1) % BaseRingWorkshopItemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + 1) % BaseRingWorkshopItemCount;
        }
        if (input.pressed(InputAction::MoveLeft)) {
            adjustRingWorkshopRespec(-1);
        }
        if (input.pressed(InputAction::MoveRight)) {
            adjustRingWorkshopRespec(1);
        }
        const auto chooseWorkshopItem = [this](int item) {
            if (item == 0) {
                adjustRingWorkshopRespec(1);
                return;
            }
            if (item == 1) {
                adjustRingWorkshopRespec(-1);
                return;
            }
            if (item == 2) {
                confirmRingWorkshopRespec();
                return;
            }
            if (item >= 3 && item < 3 + RingWorkshopImplementedUpgradeCount) {
                buyRingWorkshopUpgrade(static_cast<RingWorkshopUpgrade>(item - 3));
                return;
            }
            baseStatus_ = "この項目は未解禁です";
        };
        for (int i = 0; i < BaseRingWorkshopItemCount; ++i) {
            const UiRect rect = baseRingWorkshopItemRect(i);
            if (rect.contains(ui.mouse())) {
                baseRingWorkshopSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseRingWorkshopSelection_ = i;
                chooseWorkshopItem(i);
                return;
            }
        }
        if (ui.pressed(ringWorkshopConfirmRect())) {
            confirmRingWorkshopRespec();
            return;
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            chooseWorkshopItem(baseRingWorkshopSelection_);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (baseStorageActive_) {
        const std::vector<StorageEntry> warehouseEntries = warehouseStorageEntries();
        const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
        baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
        baseStorageBackpackCursor_ = std::clamp(baseStorageBackpackCursor_, 0, StoragePaneSlotCount - 1);
        baseStorageWarehouseCursor_ = std::clamp(baseStorageWarehouseCursor_, 0, StoragePaneSlotCount - 1);

        const auto hasBackpackItemAt = [this](int localSlot) {
            return inventory_.hasScreenItemAt(localSlot);
        };
        const auto warehouseStorageSlotFromLocal = [this](int localSlot) {
            return baseStorageWarehousePage_ * StoragePaneSlotCount + localSlot;
        };
        const auto hasWarehouseItemAt = [this, &warehouseStorageSlotFromLocal](int localSlot) {
            return warehouseEntryIndexAtStorageSlot(warehouseStorageSlotFromLocal(localSlot)) >= 0;
        };
        const auto depositBackpackSlot = [this, &warehouseStorageSlotFromLocal](int localSlot, int targetWarehouseLocalSlot = -1) {
            const int targetWarehouseSlot = targetWarehouseLocalSlot >= 0 ? warehouseStorageSlotFromLocal(targetWarehouseLocalSlot) : -1;
            const InventoryObjectStack* stack = inventory_.screenObjectStackAt(localSlot);
            if (stack != nullptr) {
                const std::string objectId = stack->objectId;
                auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [stack](const InventoryObjectStack& candidate) {
                    return candidate.objectId == stack->objectId;
                });
                int warehouseEntryIndex = -1;
                if (it == warehouseObjectStacks_.end()) {
                    if (warehouseUsedSlots() >= warehouseCapacity()) {
                        baseStatus_ = "倉庫がいっぱいです";
                        return;
                    }
                    syncWarehouseDisplaySlots();
                    const int newStackIndex = static_cast<int>(warehouseObjectStacks_.size());
                    warehouseDisplaySlots_.insert(warehouseDisplaySlots_.begin() + newStackIndex, -1);
                    warehouseObjectStacks_.push_back(InventoryObjectStack{stack->item, 0});
                    it = warehouseObjectStacks_.end() - 1;
                    warehouseEntryIndex = newStackIndex;
                } else {
                    warehouseEntryIndex = static_cast<int>(std::distance(warehouseObjectStacks_.begin(), it));
                }
                if (!inventory_.removeObjectItemCount(objectId, 1)) {
                    baseStatus_ = "預け入れに失敗しました";
                    return;
                }
                ++it->count;
                if (targetWarehouseSlot >= 0) {
                    assignWarehouseEntryToStorageSlot(warehouseEntryIndex, targetWarehouseSlot);
                }
                baseStatus_.clear();
                return;
            }
            const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(localSlot);
            if (instance == nullptr) {
                baseStatus_ = "預けるアイテムがありません";
                return;
            }
            if (warehouseUsedSlots() >= warehouseCapacity()) {
                baseStatus_ = "倉庫がいっぱいです";
                return;
            }
            InventoryObjectInstance moved;
            if (!inventory_.takeObjectInstance(instance->instance.instanceId, moved)) {
                baseStatus_ = "預け入れに失敗しました";
                return;
            }
            warehouseObjectInstances_.push_back(std::move(moved));
            if (targetWarehouseSlot >= 0) {
                assignWarehouseEntryToStorageSlot(warehouseUsedSlots() - 1, targetWarehouseSlot);
            }
            baseStatus_.clear();
        };
        const auto withdrawWarehouseSlot = [&warehouseEntries, this, &warehouseStorageSlotFromLocal](int localSlot, int targetBackpackSlot = -1) {
            const int entryIndex = warehouseEntryIndexAtStorageSlot(warehouseStorageSlotFromLocal(localSlot));
            if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseEntries.size())) {
                baseStatus_ = "取り出すアイテムがありません";
                return;
            }
            const StorageEntry entry = warehouseEntries[static_cast<std::size_t>(entryIndex)];
            if (entry.kind == StorageEntryKind::Stack) {
                InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(entry.index)];
                const std::string objectId = stack.objectId;
                if (!inventory_.addObjectItem(objectCatalog_, objectId)) {
                    baseStatus_ = "リュックがいっぱいです";
                    return;
                }
                if (targetBackpackSlot >= 0) {
                    (void)inventory_.moveObjectStackToScreenSlot(objectId, targetBackpackSlot);
                }
                --stack.count;
                if (stack.count <= 0) {
                    removeWarehouseDisplaySlotAtEntryIndex(entry.index);
                    warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + entry.index);
                }
                baseStatus_.clear();
                return;
            }

            InventoryObjectInstance moved = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)];
            const std::string instanceId = moved.instance.instanceId;
            if (!inventory_.addObjectInstance(objectCatalog_, moved.instance)) {
                baseStatus_ = "リュックがいっぱいです";
                return;
            }
            if (targetBackpackSlot >= 0) {
                (void)inventory_.moveObjectInstanceToScreenSlot(instanceId, targetBackpackSlot);
            }
            removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + entry.index);
            warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + entry.index);
            baseStatus_.clear();
        };
        const auto openStorageCommandMenuAt = [this, &hasBackpackItemAt, &hasWarehouseItemAt](int globalSlot) {
            const bool warehouse = storageGlobalSlotIsWarehouse(globalSlot);
            const int localSlot = storageLocalSlot(globalSlot);
            if (warehouse ? !hasWarehouseItemAt(localSlot) : !hasBackpackItemAt(localSlot)) {
                closeUiCommandMenu(baseStorageCommandMenu_);
                baseStorageCommandSlot_ = -1;
                return;
            }
            const char* label = warehouse ? "取り出す" : "倉庫へしまう";
            const std::array<UiCommandMenuItem, 1> items{{{label, true}}};
            baseStorageCommandSlot_ = globalSlot;
            const UiRect slotRect = storageSlotRectByGlobal(globalSlot);
            openUiCommandMenu(
                baseStorageCommandMenu_,
                slotRect.pos + Vec2{slotRect.size.x - 20.0f, 0.0f},
                storagePanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                160.0f,
                2);
        };
        const auto tryTransferBySlots = [this, &depositBackpackSlot, &withdrawWarehouseSlot](int fromGlobalSlot, int toGlobalSlot) {
            if (fromGlobalSlot < 0 || toGlobalSlot < 0) {
                return false;
            }
            const bool fromWarehouse = storageGlobalSlotIsWarehouse(fromGlobalSlot);
            const bool toWarehouse = storageGlobalSlotIsWarehouse(toGlobalSlot);
            if (fromWarehouse == toWarehouse) {
                return false;
            }
            if (fromWarehouse) {
                withdrawWarehouseSlot(storageLocalSlot(fromGlobalSlot), storageLocalSlot(toGlobalSlot));
            } else {
                depositBackpackSlot(storageLocalSlot(fromGlobalSlot), storageLocalSlot(toGlobalSlot));
            }
            return true;
        };
        const auto selectedGlobalSlot = [this]() {
            return storageGlobalSlotFromLocal(
                baseStorageFocusWarehouse_,
                baseStorageFocusWarehouse_ ? baseStorageWarehouseCursor_ : baseStorageBackpackCursor_);
        };
        const auto setSelectedGlobalSlot = [this](int globalSlot) {
            const int clamped = std::clamp(globalSlot, 0, StoragePaneSlotCount * 2 - 1);
            if (storageGlobalSlotIsWarehouse(clamped)) {
                baseStorageFocusWarehouse_ = true;
                baseStorageWarehouseCursor_ = storageLocalSlot(clamped);
            } else {
                baseStorageFocusWarehouse_ = false;
                baseStorageBackpackCursor_ = storageLocalSlot(clamped);
            }
        };

        const int commandSlotIndex = baseStorageCommandSlot_ >= 0 ? baseStorageCommandSlot_ : selectedGlobalSlot();
        const bool commandWarehouse = storageGlobalSlotIsWarehouse(commandSlotIndex);
        const char* commandLabel = commandWarehouse ? "取り出す" : "倉庫へしまう";
        const bool commandEnabled = commandWarehouse
            ? hasWarehouseItemAt(storageLocalSlot(commandSlotIndex))
            : hasBackpackItemAt(storageLocalSlot(commandSlotIndex));
        const std::array<UiCommandMenuItem, 1> commandItems{{{commandLabel, commandEnabled}}};
        const bool commandOpenBeforeUpdate = baseStorageCommandMenu_.open;
        const int commandSelection = updateUiCommandMenu(
            baseStorageCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0 && baseStorageCommandSlot_ >= 0) {
            if (storageGlobalSlotIsWarehouse(baseStorageCommandSlot_)) {
                withdrawWarehouseSlot(storageLocalSlot(baseStorageCommandSlot_));
            } else {
                depositBackpackSlot(storageLocalSlot(baseStorageCommandSlot_));
            }
            baseStorageCommandSlot_ = -1;
            baseStoragePointerPressSlot_ = -1;
            baseStoragePointerPressCanOpenMenu_ = false;
            baseStoragePointerDragTriggered_ = false;
            ui.block(storagePanelRect());
            return;
        }
        if (!baseStorageCommandMenu_.open) {
            if (commandOpenBeforeUpdate && input.backPressed()) {
                baseStorageCommandSlot_ = -1;
                baseStoragePointerPressSlot_ = -1;
                baseStoragePointerPressCanOpenMenu_ = false;
                baseStoragePointerDragTriggered_ = false;
                ui.block(storagePanelRect());
                return;
            }
            baseStorageCommandSlot_ = -1;
        }

        if (uiCancelRequested(baseCancelState_, input, ui, storagePanelRect())) {
            if (baseStorageCommandMenu_.open) {
                closeUiCommandMenu(baseStorageCommandMenu_);
                baseStorageCommandSlot_ = -1;
                baseStoragePointerPressSlot_ = -1;
                baseStoragePointerPressCanOpenMenu_ = false;
                baseStoragePointerDragTriggered_ = false;
                ui.block(storagePanelRect());
                return;
            }
            baseStorageActive_ = false;
            baseStatus_.clear();
            baseStorageGrabbedActive_ = false;
            baseStorageGrabbedFromSlot_ = -1;
            ui.block(storagePanelRect());
            return;
        }

        if (baseStorageCommandMenu_.open) {
            ui.block(storagePanelRect());
            return;
        }

        if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveRight) ||
            input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveDown)) {
            const int current = selectedGlobalSlot();
            int row = current / StorageColumns;
            int column = current % StorageColumns;
            if (input.pressed(InputAction::MoveLeft)) {
                column = (column + StorageColumns - 1) % StorageColumns;
            }
            if (input.pressed(InputAction::MoveRight)) {
                column = (column + 1) % StorageColumns;
            }
            const int totalRows = StorageRowsPerPane * 2;
            if (input.pressed(InputAction::MoveUp)) {
                row = (row + totalRows - 1) % totalRows;
            }
            if (input.pressed(InputAction::MoveDown)) {
                row = (row + 1) % totalRows;
            }
            setSelectedGlobalSlot(row * StorageColumns + column);
        }
        if (input.shortcutCursorDelta() != 0) {
            const int current = selectedGlobalSlot();
            const int next = (current + input.shortcutCursorDelta() + StoragePaneSlotCount * 2) % (StoragePaneSlotCount * 2);
            setSelectedGlobalSlot(next);
        }
        const int directSlot = input.shortcutSlotPressed();
        if (directSlot >= 0 && directSlot < StorageColumns) {
            const int current = selectedGlobalSlot();
            const int row = current / StorageColumns;
            setSelectedGlobalSlot(row * StorageColumns + directSlot);
        }

        if (input.activeRingDelta() != 0) {
            baseStorageWarehousePage_ = wrapStoragePageIndex(
                baseStorageWarehousePage_,
                input.activeRingDelta(),
                warehousePageCount);
        }

        const UiRect prevPageRect = storagePrevPageButtonRect();
        const UiRect nextPageRect = storageNextPageButtonRect();
        if (ui.pressed(prevPageRect)) {
            baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, -1, warehousePageCount);
        }
        if (ui.pressed(nextPageRect)) {
            baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, 1, warehousePageCount);
        }

        int hoveredSlot = -1;
        for (int i = 0; i < StoragePaneSlotCount * 2; ++i) {
            if (storageSlotRectByGlobal(i).contains(ui.mouse())) {
                hoveredSlot = i;
                setSelectedGlobalSlot(i);
                break;
            }
        }

        if (input.mouseLeftPressed() && hoveredSlot >= 0 && !ui.pointerConsumed()) {
            baseStoragePointerPressSlot_ = hoveredSlot;
            baseStoragePointerPressMouse_ = input.mouseScreen();
            baseStoragePointerPressCanOpenMenu_ = storageGlobalSlotIsWarehouse(hoveredSlot)
                ? hasWarehouseItemAt(storageLocalSlot(hoveredSlot))
                : hasBackpackItemAt(storageLocalSlot(hoveredSlot));
            baseStoragePointerDragTriggered_ = false;
            ui.consumePointer();
        }

        if (baseStoragePointerPressSlot_ >= 0 &&
            input.mouseLeftHeld() &&
            !baseStoragePointerDragTriggered_ &&
            !baseStorageGrabbedActive_ &&
            lengthSquared(input.mouseScreen() - baseStoragePointerPressMouse_) >= StorageDragStartDistanceSq) {
            const bool sourceWarehouse = storageGlobalSlotIsWarehouse(baseStoragePointerPressSlot_);
            const bool hasSourceItem = sourceWarehouse
                ? hasWarehouseItemAt(storageLocalSlot(baseStoragePointerPressSlot_))
                : hasBackpackItemAt(storageLocalSlot(baseStoragePointerPressSlot_));
            if (hasSourceItem) {
                baseStorageGrabbedActive_ = true;
                baseStorageGrabbedFromSlot_ = baseStoragePointerPressSlot_;
                baseStoragePointerDragTriggered_ = true;
                baseStoragePointerPressCanOpenMenu_ = false;
                closeUiCommandMenu(baseStorageCommandMenu_);
                baseStorageCommandSlot_ = -1;
            }
        }

        if (baseStoragePointerPressSlot_ >= 0 && input.mouseLeftReleased()) {
            if (baseStoragePointerDragTriggered_ && baseStorageGrabbedActive_) {
                if (hoveredSlot >= 0) {
                    setSelectedGlobalSlot(hoveredSlot);
                    (void)tryTransferBySlots(baseStorageGrabbedFromSlot_, hoveredSlot);
                }
                baseStorageGrabbedActive_ = false;
                baseStorageGrabbedFromSlot_ = -1;
            } else if (baseStoragePointerPressCanOpenMenu_ && hoveredSlot == baseStoragePointerPressSlot_) {
                openStorageCommandMenuAt(baseStoragePointerPressSlot_);
            }
            baseStoragePointerPressSlot_ = -1;
            baseStoragePointerPressCanOpenMenu_ = false;
            baseStoragePointerDragTriggered_ = false;
        }

        if (input.grabOrPlacePressed()) {
            closeUiCommandMenu(baseStorageCommandMenu_);
            baseStorageCommandSlot_ = -1;
            const int current = selectedGlobalSlot();
            if (!baseStorageGrabbedActive_) {
                const bool hasCurrent = storageGlobalSlotIsWarehouse(current)
                    ? hasWarehouseItemAt(storageLocalSlot(current))
                    : hasBackpackItemAt(storageLocalSlot(current));
                if (hasCurrent) {
                    baseStorageGrabbedActive_ = true;
                    baseStorageGrabbedFromSlot_ = current;
                    baseStatus_ = "つかみ中";
                } else {
                    baseStatus_ = "アイテム未選択";
                }
            } else {
                if (!tryTransferBySlots(baseStorageGrabbedFromSlot_, current)) {
                    baseStatus_ = "移動先を反対側にしてください";
                }
                baseStorageGrabbedActive_ = false;
                baseStorageGrabbedFromSlot_ = -1;
            }
        }

        if (input.useItemPressed() || input.confirmPressed()) {
            if (baseStorageGrabbedActive_) {
                const int current = selectedGlobalSlot();
                if (!tryTransferBySlots(baseStorageGrabbedFromSlot_, current)) {
                    baseStatus_ = "移動先を反対側にしてください";
                }
                baseStorageGrabbedActive_ = false;
                baseStorageGrabbedFromSlot_ = -1;
            } else {
                openStorageCommandMenuAt(selectedGlobalSlot());
            }
        }

        ui.block(storagePanelRect());
        return;
    }

    if (baseProcessingActive_) {
        const auto closeProcessingCommand = [this]() {
            closeUiCommandMenu(baseProcessingCommandMenu_);
            baseProcessingCommandSlot_ = -1;
        };
        const auto openProcessingCommand = [&](int slotIndex) {
            if (!processingScreenSlotAvailable(slotIndex)) {
                baseStatus_ = "この作業はできません";
                return false;
            }
            const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
            const std::array<UiCommandMenuItem, 1> items{{{mode == ProcessingMode::Repair ? "修理する" : "強化する", true}}};
            baseProcessingCommandSlot_ = slotIndex;
            openUiCommandMenu(
                baseProcessingCommandMenu_,
                baseProcessingGridSlotRect(slotIndex).pos,
                merchantPanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                144.0f,
                2);
            return true;
        };
        if (uiCancelRequested(baseCancelState_, input, ui, merchantPanelRect())) {
            if (baseProcessingCommandMenu_.open) {
                closeProcessingCommand();
            } else {
                baseProcessingActive_ = false;
                baseStatus_.clear();
            }
            return;
        }
        if (input.toggleShortcutRowPressed()) {
            closeProcessingCommand();
            baseProcessingMode_ = (baseProcessingMode_ + 1) % BaseProcessingModeCount;
        }
        const ProcessingMode currentProcessingMode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
        const std::array<UiCommandMenuItem, 1> commandItems{{{currentProcessingMode == ProcessingMode::Repair ? "修理する" : "強化する", true}}};
        const bool commandOpenBeforeUpdate = baseProcessingCommandMenu_.open;
        const int commandSelection = updateUiCommandMenu(
            baseProcessingCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0 && baseProcessingCommandSlot_ >= 0) {
            applyProcessingScreenSlot(baseProcessingCommandSlot_);
            closeProcessingCommand();
            ui.block(merchantPanelRect());
            return;
        } else if (!baseProcessingCommandMenu_.open && commandOpenBeforeUpdate) {
            baseProcessingCommandSlot_ = -1;
        }
        if (baseProcessingCommandMenu_.open) {
            ui.block(merchantPanelRect());
            return;
        }
        constexpr int Columns = 8;
        const int slotCount = inventory_.screenSlotCount();
        baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, slotCount - 1));
        if (input.pressed(InputAction::MoveLeft)) {
            baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - 1);
        }
        if (input.pressed(InputAction::MoveRight)) {
            baseProcessingSelection_ = std::min(slotCount - 1, baseProcessingSelection_ + 1);
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - Columns);
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseProcessingSelection_ = std::min(slotCount - 1, baseProcessingSelection_ + Columns);
        }
        for (int i = 0; i < BaseProcessingModeCount; ++i) {
            const UiRect rect = baseProcessingModeRect(i);
            if (rect.contains(ui.mouse())) {
                baseProcessingMode_ = i;
            }
            if (ui.pressed(rect)) {
                baseProcessingMode_ = i;
                return;
            }
        }
        for (int i = 0; i < slotCount; ++i) {
            const UiRect rect = baseProcessingGridSlotRect(i);
            if (rect.contains(ui.mouse())) {
                baseProcessingSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseProcessingSelection_ = i;
                openProcessingCommand(i);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            openProcessingCommand(baseProcessingSelection_);
            return;
        }
        ui.block(merchantPanelRect());
        return;
    }

    if (baseSellActive_) {
        refreshMerchantStock(false);
        const UiRect merchantBounds = baseMerchantMode_ == MerchantUiMode::ChooseAction ? basePanelRect() : merchantPanelRect();
        const auto closeMerchantCommands = [this]() {
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandIndex_ = -1;
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
        };
        const auto closeMerchant = [&]() {
            closeMerchantCommands();
            baseSellActive_ = false;
            baseMerchantMode_ = MerchantUiMode::Closed;
            baseStatus_.clear();
        };
        const auto returnToMerchantMenu = [&]() {
            closeMerchantCommands();
            baseMerchantMode_ = MerchantUiMode::ChooseAction;
            baseMerchantActionSelection_ = 0;
            baseStatus_.clear();
        };
        const auto moveGridSelection = [&input](int& selection, int count) {
            constexpr int Columns = 8;
            if (count <= 0) {
                selection = 0;
                return;
            }
            selection = std::clamp(selection, 0, count - 1);
            if (input.pressed(InputAction::MoveLeft)) {
                selection = std::max(0, selection - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                selection = std::min(count - 1, selection + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                selection = std::max(0, selection - Columns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                selection = std::min(count - 1, selection + Columns);
            }
        };
        const auto openSellCommand = [&](int slotIndex) {
            if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
                baseStatus_ = "売却対象がありません";
                return;
            }
            const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slotIndex);
            const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slotIndex);
            if (stack == nullptr && instance == nullptr) {
                baseStatus_ = "売却対象がありません";
                return;
            }
            if (instance != nullptr && instance->instance.protectionEnabled) {
                baseStatus_ = "保護中";
                return;
            }
            baseMerchantSellCommandIndex_ = slotIndex;
            const bool stackItem = stack != nullptr;
            const std::array<UiCommandMenuItem, 2> items{{{stackItem ? "1個売る" : "売る", true}, {"すべて売る", stackItem}}};
            openUiCommandMenu(
                baseMerchantSellCommandMenu_,
                merchantGridSlotRect(slotIndex).pos,
                merchantPanelRect(),
                stackItem ? 2 : 1,
                items.data(),
                168.0f,
                2);
        };
        const auto openBuyCommand = [&](int index) {
            if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
                baseStatus_ = "購入できる商品がありません";
                return;
            }
            baseMerchantBuyCommandIndex_ = index;
            const std::array<UiCommandMenuItem, 1> items{{{"買う", canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(index)])}}};
            openUiCommandMenu(
                baseMerchantBuyCommandMenu_,
                merchantGridSlotRect(index).pos,
                merchantPanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                120.0f,
                2);
        };

        if (uiCancelRequested(baseCancelState_, input, ui, merchantBounds)) {
            if (baseMerchantSellCommandMenu_.open || baseMerchantBuyCommandMenu_.open) {
                closeMerchantCommands();
            } else if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
                closeMerchant();
            } else {
                returnToMerchantMenu();
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
            closeMerchantCommands();
            constexpr int ChoiceCount = 2;
            baseMerchantActionSelection_ = std::clamp(baseMerchantActionSelection_, 0, ChoiceCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseMerchantActionSelection_ = (baseMerchantActionSelection_ + ChoiceCount - 1) % ChoiceCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseMerchantActionSelection_ = (baseMerchantActionSelection_ + 1) % ChoiceCount;
            }
            for (int i = 0; i < ChoiceCount; ++i) {
                const UiRect rect = merchantChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseMerchantActionSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseMerchantActionSelection_ = i;
                    if (i == 0) {
                        baseMerchantMode_ = MerchantUiMode::Buy;
                    } else if (i == 1) {
                        baseMerchantMode_ = MerchantUiMode::Sell;
                    }
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                if (baseMerchantActionSelection_ == 0) {
                    baseMerchantMode_ = MerchantUiMode::Buy;
                } else if (baseMerchantActionSelection_ == 1) {
                    baseMerchantMode_ = MerchantUiMode::Sell;
                }
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::Sell) {
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
            const bool stackCommand = baseMerchantSellCommandIndex_ >= 0 && inventory_.screenObjectStackAt(baseMerchantSellCommandIndex_) != nullptr;
            const std::array<UiCommandMenuItem, 2> commandItems{{{stackCommand ? "1個売る" : "売る", true}, {"すべて売る", stackCommand}}};
            const int commandItemCount = stackCommand ? 2 : 1;
            const bool commandOpenBeforeUpdate = baseMerchantSellCommandMenu_.open;
            const int commandSelection = updateUiCommandMenu(
                baseMerchantSellCommandMenu_,
                ui,
                input,
                commandItems.data(),
                commandItemCount);
            if (commandSelection >= 0 && baseMerchantSellCommandIndex_ >= 0) {
                sellMerchantScreenSlot(baseMerchantSellCommandIndex_, commandSelection == 1 && stackCommand ? 0 : 1);
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            } else if (!baseMerchantSellCommandMenu_.open && commandOpenBeforeUpdate) {
                baseMerchantSellCommandIndex_ = -1;
            }
            if (baseMerchantSellCommandMenu_.open) {
                ui.block(merchantBounds);
                return;
            }

            moveGridSelection(baseSellSelection_, inventory_.screenSlotCount());
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const UiRect rect = merchantGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseSellSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseSellSelection_ = i;
                    openSellCommand(i);
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openSellCommand(baseSellSelection_);
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::Buy) {
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandIndex_ = -1;
            const bool commandEnabled = baseMerchantBuyCommandIndex_ >= 0 &&
                baseMerchantBuyCommandIndex_ < static_cast<int>(merchantStock_.size()) &&
                canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(baseMerchantBuyCommandIndex_)]);
            const std::array<UiCommandMenuItem, 1> commandItems{{{"買う", commandEnabled}}};
            const bool commandOpenBeforeUpdate = baseMerchantBuyCommandMenu_.open;
            const int commandSelection = updateUiCommandMenu(
                baseMerchantBuyCommandMenu_,
                ui,
                input,
                commandItems.data(),
                static_cast<int>(commandItems.size()));
            if (commandSelection >= 0 && baseMerchantBuyCommandIndex_ >= 0) {
                buyMerchantProduct(baseMerchantBuyCommandIndex_);
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            } else if (!baseMerchantBuyCommandMenu_.open && commandOpenBeforeUpdate) {
                baseMerchantBuyCommandIndex_ = -1;
            }
            if (baseMerchantBuyCommandMenu_.open) {
                ui.block(merchantBounds);
                return;
            }

            moveGridSelection(baseMerchantBuySelection_, static_cast<int>(merchantStock_.size()));
            for (int i = 0; i < static_cast<int>(merchantStock_.size()); ++i) {
                const UiRect rect = merchantGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseMerchantBuySelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseMerchantBuySelection_ = i;
                    openBuyCommand(i);
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openBuyCommand(baseMerchantBuySelection_);
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        returnToMerchantMenu();
        ui.block(merchantBounds);
        return;
    }    if (baseUpgradeActive_) {
        if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
            baseUpgradeActive_ = false;
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + BaseUpgradeItemCount - 1) % BaseUpgradeItemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + 1) % BaseUpgradeItemCount;
        }
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            const UiRect rect = baseUpgradeItemRect(i);
            if (rect.contains(ui.mouse())) {
                baseUpgradeSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseUpgradeSelection_ = i;
                buyUpgrade(i);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            buyUpgrade(baseUpgradeSelection_);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (baseMiningStartChoiceActive_) {
        if (input.backPressed()) {
            baseMiningStartChoiceActive_ = false;
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + BaseMiningStartChoiceCount - 1) % BaseMiningStartChoiceCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + 1) % BaseMiningStartChoiceCount;
        }
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            const UiRect rect = baseMiningStartChoiceRect(i);
            if (rect.contains(ui.mouse())) {
                baseMiningStartSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseMiningStartSelection_ = i;
                if (i == 1 && unlockedWarpPointCount_ <= 0) {
                    baseStatus_ = "解放済みワープポイントがありません";
                    return;
                }
                if (i == 2) {
                    if (!canRegenerateCurrentStage()) {
                        baseStatus_ = "全ワープポイント発見後に再生成できます";
                        return;
                    }
                    if (retainedWorldDropCountForCurrentStage() > 0 && !baseRegenerateConfirmActive_) {
                        baseRegenerateConfirmActive_ = true;
                        baseStatus_ = "未回収ドロップが消えます。もう一度選ぶと再生成します";
                        return;
                    }
                    baseRegenerateConfirmActive_ = false;
                    requestMiningStartTransition(false, true);
                    return;
                }
                baseRegenerateConfirmActive_ = false;
                requestMiningStartTransition(i == 1, false);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            if (baseMiningStartSelection_ == 1 && unlockedWarpPointCount_ <= 0) {
                baseStatus_ = "解放済みワープポイントがありません";
                return;
            }
            if (baseMiningStartSelection_ == 2) {
                if (!canRegenerateCurrentStage()) {
                    baseStatus_ = "全ワープポイント発見後に再生成できます";
                    return;
                }
                if (retainedWorldDropCountForCurrentStage() > 0 && !baseRegenerateConfirmActive_) {
                    baseRegenerateConfirmActive_ = true;
                    baseStatus_ = "未回収ドロップが消えます。もう一度選ぶと再生成します";
                    return;
                }
                baseRegenerateConfirmActive_ = false;
                requestMiningStartTransition(false, true);
                return;
            }
            baseRegenerateConfirmActive_ = false;
            requestMiningStartTransition(baseMiningStartSelection_ == 1, false);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (input.pausePressed()) {
        mode_ = ScreenMode::PauseMenu;
        pauseReturnMode_ = ScreenMode::Base;
        pausePage_ = PauseMenuPage::Main;
        return;
    }

    const auto interact = [this](const BaseFacility& facility) {
        if (!facility.enabled) {
            return;
        }
        switch (facility.onInteract) {
        case BaseFacilityAction::MineExit:
            baseMiningStartChoiceActive_ = true;
            baseMiningStartSelection_ = unlockedWarpPointCount_ > 0 ? 1 : 0;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Storage:
            baseStorageActive_ = true;
            baseStorageFocusWarehouse_ = false;
            baseStorageBackpackCursor_ = 0;
            baseStorageWarehouseCursor_ = 0;
            baseStorageWarehousePage_ = 0;
            closeUiCommandMenu(baseStorageCommandMenu_);
            baseStorageCommandSlot_ = -1;
            baseStoragePointerPressSlot_ = -1;
            baseStoragePointerPressMouse_ = {};
            baseStoragePointerPressCanOpenMenu_ = false;
            baseStoragePointerDragTriggered_ = false;
            baseStorageGrabbedActive_ = false;
            baseStorageGrabbedFromSlot_ = -1;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Merchant:
            if (merchantRefreshPending_) {
                refreshMerchantStock(true);
                merchantRefreshPending_ = false;
            } else {
                refreshMerchantStock(false);
            }
            baseSellActive_ = true;
            baseMerchantMode_ = MerchantUiMode::ChooseAction;
            baseMerchantActionSelection_ = 0;
            baseSellSelection_ = 0;
            baseMerchantBuySelection_ = 0;
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandIndex_ = -1;
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Forge:
            baseUpgradeActive_ = true;
            baseUpgradeSelection_ = 0;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Processing:
            baseProcessingActive_ = true;
            baseProcessingMode_ = 0;
            baseProcessingSelection_ = 0;
            closeUiCommandMenu(baseProcessingCommandMenu_);
            baseProcessingCommandSlot_ = -1;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Bookshelf:
            openBookshelf();
            break;
        case BaseFacilityAction::Diary: {
            std::string message;
            saveSaveData(message);
            baseStatus_ = message;
            break;
        }
        case BaseFacilityAction::RingWorkshop:
            if (facility.unlocked) {
                openRingWorkshop();
            } else {
                baseStatus_ = "リング工房用スペース: まだ解禁されていません";
            }
            break;
        case BaseFacilityAction::HomeEntrance:
            baseOutdoorPlayerPosition_ = basePlayerPosition_;
            requestBaseAreaCrossfade(
                BaseArea::HomeInterior,
                {640.0f, 542.0f},
                {0.0f, -1.0f},
                "主人公の家に入りました");
            break;
        case BaseFacilityAction::HomeExit:
            requestBaseAreaCrossfade(
                BaseArea::Outdoor,
                baseOutdoorPlayerPosition_,
                {0.0f, 1.0f},
                "屋外拠点に戻りました");
            break;
        case BaseFacilityAction::MonicaTalk:
            startBaseMonicaDialogue();
            break;
        }
    };

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    const float playerRadius = balance_.playerRadius;
    const auto baseCollision = [&](Vec2 position) {
        const UiRect bounds = baseMapBounds();
        if (position.x - playerRadius < bounds.pos.x ||
            position.y - playerRadius < bounds.pos.y ||
            position.x + playerRadius > bounds.pos.x + bounds.size.x ||
            position.y + playerRadius > bounds.pos.y + bounds.size.y) {
            return true;
        }
        const Vec2 passabilityProbe = playerSpriteFootAnchor(position);
        const int minTileX = static_cast<int>(std::floor((passabilityProbe.x - playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int maxTileX = static_cast<int>(std::floor((passabilityProbe.x + playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int minTileY = static_cast<int>(std::floor((passabilityProbe.y - playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int maxTileY = static_cast<int>(std::floor((passabilityProbe.y + playerRadius) / static_cast<float>(BaseEditGridSize)));
        for (int tileY = minTileY; tileY <= maxTileY; ++tileY) {
            for (int tileX = minTileX; tileX <= maxTileX; ++tileX) {
                if (!isBasePassabilityBlocked(baseArea_, tileX, tileY)) {
                    continue;
                }
                const UiRect tileRect{
                    {static_cast<float>(tileX * BaseEditGridSize), static_cast<float>(tileY * BaseEditGridSize)},
                    {static_cast<float>(BaseEditGridSize), static_cast<float>(BaseEditGridSize)},
                };
                if (circleIntersectsRect(passabilityProbe, playerRadius, tileRect)) {
                    return true;
                }
            }
        }
        return false;
    };

    const Vec2 moveAxis = input.moveAxis();
    const bool walkingNow = lengthSquared(moveAxis) > 0.0001f;
    if (walkingNow != basePlayerSpriteWalking_) {
        basePlayerSpriteWalking_ = walkingNow;
        basePlayerSpriteAnimationTime_ = 0.0f;
    } else {
        basePlayerSpriteAnimationTime_ += dt;
    }
    maybeSpawnPlayerFootstepDust(
        playerSpriteFootAnchor(basePlayerPosition_),
        lengthSquared(moveAxis) > 0.0001f ? moveAxis : basePlayerFacing_,
        basePlayerSpriteWalking_,
        playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
        previousBasePlayerDustFrame_);
    if (lengthSquared(moveAxis) > 0.0001f) {
        basePlayerFacing_ = normalize(moveAxis);
        const Vec2 delta = moveAxis * balance_.playerSpeed * dt;
        Vec2 next = basePlayerPosition_ + Vec2{delta.x, 0.0f};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
        next = basePlayerPosition_ + Vec2{0.0f, delta.y};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
    }

    const BaseFacility* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const BaseFacility& facility : facilities) {
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }

    if (input.mouseLeftPressed() && !ui.pointerConsumed()) {
        for (const BaseFacility& facility : facilities) {
            if (!facility.rect.contains(ui.mouse())) {
                continue;
            }
            ui.consumePointer();
            if (baseInteractionAvailable(basePlayerPosition_, facility)) {
                interact(facility);
            } else if (facility.enabled) {
                baseStatus_ = "近くまで移動してください";
            }
            return;
        }
    }

    if (input.confirmPressed() && nearest != nullptr) {
        interact(*nearest);
        return;
    }
}

void Game::renderBookshelfScreen(Renderer& renderer) const
{
    const UiRect panel = basePanelRect();
    renderer.drawText(panel.pos + Vec2{202.0f, 214.0f}, "本棚", {246, 235, 255, 255}, 3);

    char buffer[256];
    const auto menuName = [](int index) {
        switch (index) {
        case 0:
            return "アイテム図鑑";
        case 1:
            return "宝図鑑";
        case 2:
            return "敵図鑑";
        default:
            return "";
        }
    };
    const auto objectAt = [this](BookshelfPage page, int targetIndex) -> const ObjectDefinition* {
        int index = 0;
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((page == BookshelfPage::Treasures && treasure) ||
                (page == BookshelfPage::Items && !treasure)) {
                if (index == targetIndex) {
                    return &object;
                }
                ++index;
            }
        }
        return nullptr;
    };

    if (bookshelfPage_ == BookshelfPage::Menu) {
        renderer.drawText(panel.pos + Vec2{76.0f, 224.0f}, "図鑑を選んでください", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BookshelfMenuItemCount; ++i) {
            drawUiButton(renderer, bookshelfItemRect(i), menuName(i), i == bookshelfSelection_);
        }
        return;
    }

    std::vector<std::string> lines;
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (enemy.name.empty() ? enemy.id : enemy.name);
            std::snprintf(buffer, sizeof(buffer), "%s  [%s]", name.c_str(), encyclopediaStageName(stage));
            lines.emplace_back(buffer);
        }
    } else {
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((bookshelfPage_ == BookshelfPage::Treasures && !treasure) ||
                (bookshelfPage_ == BookshelfPage::Items && treasure)) {
                continue;
            }
            const EncyclopediaStage stage = encyclopedia_.objectStage(object.id, treasure);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object.name.empty() ? object.id : object.name);
            std::snprintf(buffer, sizeof(buffer), "%s  [%s]", name.c_str(), encyclopediaStageName(stage));
            lines.emplace_back(buffer);
        }
    }

    const char* title = bookshelfPage_ == BookshelfPage::Items
        ? "アイテム図鑑"
        : (bookshelfPage_ == BookshelfPage::Treasures ? "宝図鑑" : "敵図鑑");
    renderer.drawText(panel.pos + Vec2{74.0f, 224.0f}, title, {198, 198, 206, 255}, 2);
    if (lines.empty()) {
        renderer.drawText(panel.pos + Vec2{94.0f, 276.0f}, "記録対象がありません", {150, 150, 160, 255}, 2);
    } else {
        const int visibleCount = std::min(BookshelfVisibleRows, static_cast<int>(lines.size()));
        const int firstVisibleIndex = std::clamp(
            bookshelfSelection_ - visibleCount / 2,
            0,
            std::max(0, static_cast<int>(lines.size()) - visibleCount));
        for (int i = 0; i < visibleCount; ++i) {
            const int lineIndex = firstVisibleIndex + i;
            drawUiButton(renderer, bookshelfItemRect(i), lines[static_cast<std::size_t>(lineIndex)], lineIndex == bookshelfSelection_);
        }
    }

    const UiRect bookshelfDetailPanel{{414.0f, 548.0f}, {452.0f, 144.0f}};
    const Vec2 bookshelfDetailContent = uiSubPanelContentPos(bookshelfDetailPanel);
    drawUiSubPanel(renderer, bookshelfDetailPanel);
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        if (bookshelfSelection_ >= 0 && bookshelfSelection_ < static_cast<int>(enemyCatalog_.enemies.size())) {
            const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(bookshelfSelection_)];
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (enemy.name.empty() ? enemy.id : enemy.name);
            std::snprintf(buffer, sizeof(buffer), "%s / %s", name.c_str(), encyclopediaStageName(stage));
            renderer.drawText(bookshelfDetailContent, buffer, {255, 230, 150, 255}, 2);
        }
    } else if (const ObjectDefinition* object = objectAt(bookshelfPage_, bookshelfSelection_)) {
        const bool treasure = object->category == "\xE5\xAE\x9D";
        const EncyclopediaStage stage = encyclopedia_.objectStage(object->id, treasure);
        const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object->name.empty() ? object->id : object->name);
        std::snprintf(buffer, sizeof(buffer), "%s / %s", name.c_str(), encyclopediaStageName(stage));
        renderer.drawText(bookshelfDetailContent, buffer, {255, 230, 150, 255}, 2);
        const std::vector<std::string> effects = encyclopedia_.getObjectEffectDisplayLines(
            object->id,
            objectCatalog_,
            EffectRevealMode::WithUnknown);
        const std::string effectText = joinEffectLines(effects);
        renderer.drawWrappedText(
            bookshelfDetailContent + Vec2{0.0f, 34.0f},
            effectText,
            bookshelfDetailPanel.size.x - ui::SubPanelPadding.x * 2.0f,
            {232, 234, 242, 255},
            2);
    }
}

void Game::renderBaseBackdrop(Renderer& renderer) const
{
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    if (baseArea_ == BaseArea::HomeInterior) {
        renderer.fillRect(map.pos, map.size, {74, 58, 52, 255});
        renderer.drawRect(map.pos, map.size, {184, 150, 108, 255});
        renderer.fillRect({76.0f, 72.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 584.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({1160.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({132.0f, 132.0f}, {1016.0f, 436.0f}, {118, 92, 66, 255});
        renderer.drawText({558.0f, 88.0f}, "主人公の家", {246, 235, 255, 255}, 2);
    } else {
        if (renderer.hasBaseMapTexture()) {
            renderer.drawBaseMapTexture(map.pos, map.size);
        } else {
            renderer.fillRect(map.pos, map.size, {68, 96, 58, 255});
            renderer.drawRect(map.pos, map.size, {156, 128, 82, 255});
            renderer.fillRect({62.0f, 456.0f}, {1156.0f, 88.0f}, {98, 84, 58, 255});
            renderer.fillRect({566.0f, 130.0f}, {132.0f, 430.0f}, {92, 78, 54, 255});
            renderer.fillRect({330.0f, 72.0f}, {154.0f, 100.0f}, {96, 54, 62, 255});
            renderer.drawRect({330.0f, 72.0f}, {154.0f, 100.0f}, {216, 184, 130, 255});
            renderer.drawText({350.0f, 106.0f}, "主人公の家", {246, 235, 255, 255}, 2);
            renderer.fillRect({600.0f, 586.0f}, {80.0f, 34.0f}, {38, 30, 36, 255});
            renderer.drawCircle({640.0f, 602.0f}, 42.0f, {160, 122, 80, 255});
        }
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    const BaseFacility* nearest = nullptr;
    const BaseFacility* hovered = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    const Vec2 mouse{mouseX, mouseY};
    for (const BaseFacility& facility : facilities) {
        if (!facility.enabled) {
            continue;
        }
        if (facility.rect.contains(mouse)) {
            hovered = &facility;
        }
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }
    for (int pass = 0; pass < 2; ++pass) {
        const bool drawEnabled = pass == 1;
        for (const BaseFacility& facility : facilities) {
            if (facility.enabled != drawEnabled) {
                continue;
            }
            const bool texturedOutdoorBase = baseArea_ == BaseArea::Outdoor && renderer.hasBaseMapTexture();
            Color fill = facility.enabled ? Color{96, 82, 82, 255} : Color{84, 62, 56, 255};
            if (texturedOutdoorBase) {
                fill.a = facility.enabled ? 120 : 80;
            }
            if (!facility.unlocked) {
                fill = {58, 58, 64, 255};
                if (texturedOutdoorBase) {
                    fill.a = 120;
                }
            }
            if (&facility == nearest) {
                fill = {118, 98, 58, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            if (&facility == hovered) {
                fill = {124, 104, 72, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            renderer.fillRect(facility.rect.pos, facility.rect.size, fill);
            renderer.drawRect(facility.rect.pos, facility.rect.size, facility.enabled ? Color{220, 200, 150, 255} : Color{120, 108, 98, 255});
            renderer.drawText(facility.rect.pos + Vec2{8.0f, 8.0f}, facility.displayName, facility.enabled ? Color{248, 238, 214, 255} : Color{154, 146, 138, 255}, 2);
        }
    }
    renderBaseEditOverlay(renderer);

    const Vec2 basePlayerFootAnchor = playerSpriteFootAnchor(basePlayerPosition_);
    renderer.drawActorShadow(basePlayerFootAnchor, PlayerSpriteDrawSize);
    renderPlayerFootstepDust(renderer);
    if (renderer.hasPlayerSheet()) {
        renderer.drawPlayerSprite(
            playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
            basePlayerFootAnchor,
            PlayerSpriteDrawSize,
            basePlayerFacing_.x > 0.0f,
            {255, 255, 255, 255},
            {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
    } else {
        renderer.fillCircle(basePlayerPosition_, balance_.playerRadius, {118, 72, 168, 255});
        renderer.drawLine(basePlayerPosition_, basePlayerPosition_ + basePlayerFacing_ * 22.0f, {235, 210, 255, 255});
    }

    renderTopInfoBar(renderer);
}

void Game::renderBaseScreen(Renderer& renderer) const
{
    if (!basePresentationActive()) {
        return;
    }

    renderer.setScreenSpace();
    if (baseStorageActive_) {
        renderBaseBackdrop(renderer);
        const UiRect panel = storagePanelRect();
        UiCancelControlScope cancelScope(baseCancelState_);
        UiWindowScope storageWindow(
            renderer,
            "base.storage",
            panel,
            "収納箱",
            "F/Enter コマンド  G つかむ/置く  Z/X 倉庫ページ  Esc/右クリック 戻る",
            UiWindowOptions{true, true});

        const std::vector<StorageEntry> warehouseEntries = warehouseStorageEntries();
        const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
        const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);

        const auto selectedGlobalSlot = [this]() {
            return storageGlobalSlotFromLocal(
                baseStorageFocusWarehouse_,
                baseStorageFocusWarehouse_ ? baseStorageWarehouseCursor_ : baseStorageBackpackCursor_);
        };
        const auto entryViewForBackpackSlot = [this](int slot) {
            InventoryUiEntryView entry{};
            if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot)) {
                entry.item = &stack->item;
                entry.stackCount = stack->count;
                return entry;
            }
            if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot)) {
                entry.item = &instance->item;
                entry.instance = &instance->instance;
                entry.stackCount = 1;
            }
            return entry;
        };
        const auto entryViewForWarehouseSlot = [this, &warehouseEntries, warehousePage](int slot) {
            InventoryUiEntryView entry{};
            const int entryIndex = warehouseEntryIndexAtStorageSlot(warehousePage * StoragePaneSlotCount + slot);
            if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseEntries.size())) {
                return entry;
            }
            const StorageEntry storageEntry = warehouseEntries[static_cast<std::size_t>(entryIndex)];
            entry.item = storageEntryItem(storageEntry, true);
            entry.instance = storageEntryInstance(storageEntry, true);
            entry.stackCount = storageEntryStackCount(storageEntry, true);
            return entry;
        };

        const auto drawTextCentered = [&renderer](UiRect rect, float y, std::string_view text, Color color, int scale) {
            const Vec2 size = renderer.measureText(text, scale);
            renderer.drawText({rect.pos.x + (rect.size.x - size.x) * 0.5f, y}, text, color, scale);
        };
        const auto drawStorageHeader = [&renderer](float x, float y, std::string_view title, std::string_view count, Color color) {
            renderer.drawText({x, y}, title, color, 3);
            const Vec2 titleSize = renderer.measureText(title, 3);
            renderer.drawText(
                {x + titleSize.x + StorageHeaderCountGap, y + StorageHeaderCountYOffset},
                count,
                color,
                StorageHeaderCountScale);
        };

        char buffer[256];
        const Color backpackHeaderColor = baseStorageFocusWarehouse_ ? Color{198, 198, 206, 255} : Color{255, 230, 150, 255};
        std::snprintf(buffer, sizeof(buffer), "\xEF\xBC\x88%d/24\xEF\xBC\x89", backpackUsedSlots());
        drawStorageHeader(StorageHeaderTextX, StorageBackpackHeaderY, "リュック", buffer, backpackHeaderColor);

        drawUiSeparator(
            renderer,
            {{StorageGridX, StorageDividerY - ui::SeparatorHeight * 0.5f}, {StorageGridW, ui::SeparatorHeight}},
            {255, 255, 255, 220});
        const Color warehouseHeaderColor = baseStorageFocusWarehouse_ ? Color{255, 230, 150, 255} : Color{198, 198, 206, 255};
        std::snprintf(buffer, sizeof(buffer), "\xEF\xBC\x88%d/%d\xEF\xBC\x89", warehouseUsedSlots(), warehouseCapacity());
        drawStorageHeader(StorageHeaderTextX, StorageBottomHeaderY, "倉庫", buffer, warehouseHeaderColor);

        const UiRect prevPageRect = storagePrevPageButtonRect();
        const UiRect pageTextRect = storagePageTextRect();
        const UiRect nextPageRect = storageNextPageButtonRect();
        std::snprintf(buffer, sizeof(buffer), "%d/%d", warehousePage + 1, warehousePageCount);
        drawTextCentered(pageTextRect, StorageBottomHeaderY + StoragePageTextYOffset, buffer, {198, 198, 206, 255}, StoragePageTextScale);
        drawUiRectButton(renderer, prevPageRect, "<", false);
        drawUiRectButton(renderer, nextPageRect, ">", false);

        for (int i = 0; i < StoragePaneSlotCount; ++i) {
            const int backpackGlobalSlot = storageGlobalSlotFromLocal(false, i);
            drawInventoryUiSlot(
                renderer,
                storageBackpackSlotRect(i),
                entryViewForBackpackSlot(i),
                selectedGlobalSlot() == backpackGlobalSlot,
                40.0f);
        }
        for (int i = 0; i < StoragePaneSlotCount; ++i) {
            const int warehouseGlobalSlot = storageGlobalSlotFromLocal(true, i);
            drawInventoryUiSlot(
                renderer,
                storageWarehouseSlotRect(i),
                entryViewForWarehouseSlot(i),
                selectedGlobalSlot() == warehouseGlobalSlot,
                40.0f);
        }

        const int detailSlot = (baseStorageCommandMenu_.open && baseStorageCommandSlot_ >= 0)
            ? baseStorageCommandSlot_
            : selectedGlobalSlot();
        InventoryUiEntryView detailEntry{};
        if (storageGlobalSlotIsWarehouse(detailSlot)) {
            detailEntry = entryViewForWarehouseSlot(storageLocalSlot(detailSlot));
        } else {
            detailEntry = entryViewForBackpackSlot(storageLocalSlot(detailSlot));
        }
        const UiRect detailPanel{{StorageDetailX, StorageDetailY}, {330.0f, 520.0f}};
        drawInventoryUiDetailPanel(renderer, detailPanel, detailEntry, objectCatalog_, encyclopedia_, false);

        const int commandSlotIndex = baseStorageCommandSlot_ >= 0 ? baseStorageCommandSlot_ : selectedGlobalSlot();
        const bool commandWarehouse = storageGlobalSlotIsWarehouse(commandSlotIndex);
        const char* commandLabel = commandWarehouse ? "取り出す" : "倉庫へしまう";
        const bool hasCommandItem = commandWarehouse
            ? (entryViewForWarehouseSlot(storageLocalSlot(commandSlotIndex)).item != nullptr)
            : (entryViewForBackpackSlot(storageLocalSlot(commandSlotIndex)).item != nullptr);
        const std::array<UiCommandMenuItem, 1> commandItems{{{commandLabel, hasCommandItem}}};
        drawUiCommandMenu(renderer, baseStorageCommandMenu_, commandItems.data(), static_cast<int>(commandItems.size()));

        if (!baseStatus_.empty()) {
            drawUiBodyMessageBelow(renderer, storageSlotRectByGlobal(selectedGlobalSlot()), baseStatus_, ui::Text);
        }
        return;
    }

    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    if (baseArea_ == BaseArea::HomeInterior) {
        renderer.fillRect(map.pos, map.size, {74, 58, 52, 255});
        renderer.drawRect(map.pos, map.size, {184, 150, 108, 255});
        renderer.fillRect({76.0f, 72.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 584.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({1160.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({132.0f, 132.0f}, {1016.0f, 436.0f}, {118, 92, 66, 255});
        renderer.drawText({558.0f, 88.0f}, "主人公の家", {246, 235, 255, 255}, 2);
    } else {
        if (renderer.hasBaseMapTexture()) {
            renderer.drawBaseMapTexture(map.pos, map.size);
        } else {
            renderer.fillRect(map.pos, map.size, {68, 96, 58, 255});
            renderer.drawRect(map.pos, map.size, {156, 128, 82, 255});
        renderer.fillRect({62.0f, 456.0f}, {1156.0f, 88.0f}, {98, 84, 58, 255});
        renderer.fillRect({566.0f, 130.0f}, {132.0f, 430.0f}, {92, 78, 54, 255});
        renderer.fillRect({330.0f, 72.0f}, {154.0f, 100.0f}, {96, 54, 62, 255});
        renderer.drawRect({330.0f, 72.0f}, {154.0f, 100.0f}, {216, 184, 130, 255});
        renderer.drawText({350.0f, 106.0f}, "主人公の家", {246, 235, 255, 255}, 2);
        renderer.fillRect({600.0f, 586.0f}, {80.0f, 34.0f}, {38, 30, 36, 255});
            renderer.drawCircle({640.0f, 602.0f}, 42.0f, {160, 122, 80, 255});
        }
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    const BaseFacility* nearest = nullptr;
    const BaseFacility* hovered = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    const Vec2 mouse{mouseX, mouseY};
    for (const BaseFacility& facility : facilities) {
        if (!facility.enabled) {
            continue;
        }
        if (facility.rect.contains(mouse)) {
            hovered = &facility;
        }
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }
    for (int pass = 0; pass < 2; ++pass) {
        const bool drawEnabled = pass == 1;
        for (const BaseFacility& facility : facilities) {
            if (facility.enabled != drawEnabled) {
                continue;
            }
            const bool texturedOutdoorBase = baseArea_ == BaseArea::Outdoor && renderer.hasBaseMapTexture();
            Color fill = facility.enabled ? Color{96, 82, 82, 255} : Color{84, 62, 56, 255};
            if (texturedOutdoorBase) {
                fill.a = facility.enabled ? 120 : 80;
            }
            if (!facility.unlocked) {
                fill = {58, 58, 64, 255};
                if (texturedOutdoorBase) {
                    fill.a = 120;
                }
            }
            if (&facility == nearest) {
                fill = {118, 98, 58, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            if (&facility == hovered) {
                fill = {124, 104, 72, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            renderer.fillRect(facility.rect.pos, facility.rect.size, fill);
            renderer.drawRect(facility.rect.pos, facility.rect.size, facility.enabled ? Color{220, 200, 150, 255} : Color{120, 108, 98, 255});
            renderer.drawText(facility.rect.pos + Vec2{8.0f, 8.0f}, facility.displayName, facility.enabled ? Color{248, 238, 214, 255} : Color{154, 146, 138, 255}, 2);
        }
    }
    renderBaseEditOverlay(renderer);

    const Vec2 basePlayerFootAnchor = playerSpriteFootAnchor(basePlayerPosition_);
    renderer.drawActorShadow(basePlayerFootAnchor, PlayerSpriteDrawSize);
    renderPlayerFootstepDust(renderer);
    if (renderer.hasPlayerSheet()) {
        renderer.drawPlayerSprite(
            playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
            basePlayerFootAnchor,
            PlayerSpriteDrawSize,
            basePlayerFacing_.x > 0.0f,
            {255, 255, 255, 255},
            {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
    } else {
        renderer.fillCircle(basePlayerPosition_, balance_.playerRadius, {118, 72, 168, 255});
        renderer.drawLine(basePlayerPosition_, basePlayerPosition_ + basePlayerFacing_ * 22.0f, {235, 210, 255, 255});
    }

    renderTopInfoBar(renderer);

    char buffer[256];
    const bool panelUiActive = baseRingWorkshopActive_ ||
        baseBookshelfActive_ ||
        baseProcessingActive_ ||
        baseSellActive_ ||
        baseUpgradeActive_ ||
        baseMiningStartChoiceActive_;
    const UiRect panel = (baseProcessingActive_ || (baseSellActive_ && baseMerchantMode_ != MerchantUiMode::ChooseAction))
        ? merchantPanelRect()
        : basePanelRect();
    std::optional<UiWindowScope> panelWindow;
    std::optional<UiCancelControlScope> panelCancelScope;
    if (panelUiActive) {
        const char* panelHelp = "F/Enter 決定  左クリック 決定  Esc/右クリック 戻る";
        if (baseBookshelfActive_) {
            panelHelp = bookshelfPage_ == BookshelfPage::Menu
                ? "F/Enter 決定  Esc/右クリック 戻る"
                : "Esc/右クリック 戻る";
        } else if (baseSellActive_) {
            panelHelp = baseMerchantMode_ == MerchantUiMode::Buy
                ? "F/Enter 買う  Esc/右クリック 戻る"
                : (baseMerchantMode_ == MerchantUiMode::Sell ? "F/Enter 売る  Esc/右クリック 戻る" : "F/Enter 決定  Esc/右クリック 戻る");
        } else if (baseProcessingActive_) {
            panelHelp = "F/Enter 実行  Tab 作業切替  Esc/右クリック 戻る";
        } else if (baseUpgradeActive_) {
            panelHelp = "F/Enter 強化  Esc/右クリック 戻る";
        } else if (baseMiningStartChoiceActive_) {
            panelHelp = "Esc/右クリック 戻る";
        }
        const char* panelTitle = "魔女の拠点";
        if (baseBookshelfActive_) {
            panelTitle = "本棚";
        } else if (baseRingWorkshopActive_) {
            panelTitle = "リング工房";
        } else if (baseProcessingActive_) {
            panelTitle = "作業台";
        } else if (baseSellActive_) {
            if (baseMerchantMode_ == MerchantUiMode::Buy) {
                panelTitle = "商人ワゴン 購入";
            } else if (baseMerchantMode_ == MerchantUiMode::Sell) {
                panelTitle = "商人ワゴン 売却";
            } else {
                panelTitle = "商人ワゴン";
            }
        } else if (baseUpgradeActive_) {
            panelTitle = "拠点強化炉";
        } else if (baseMiningStartChoiceActive_) {
            panelTitle = "採掘出口";
        }
        const bool panelCancelButton = !baseMiningStartChoiceActive_;
        if (panelCancelButton) {
            panelCancelScope.emplace(baseCancelState_);
        }
        panelWindow.emplace(renderer, "base.panel", panel, panelTitle, panelHelp, UiWindowOptions{true, panelCancelButton});
    }

    if (baseBookshelfActive_) {
        renderBookshelfScreen(renderer);
    } else if (baseRingWorkshopActive_) {
        renderer.drawText(panel.pos + Vec2{168.0f, 214.0f}, "リング工房", {246, 235, 255, 255}, 3);
        const int totalPoints = ringLevelUpgradePointTotal();
        std::snprintf(buffer, sizeof(buffer), "レベルアップ強化点 %d  現在: 半径%d / 速度%d  配分案: 半径%d / 速度%d",
            totalPoints,
            levelRingRadiusPoints_,
            levelRingSpeedPoints_,
            ringWorkshopDraftRadiusPoints_,
            ringWorkshopDraftSpeedPoints_);
        renderer.drawText(panel.pos + Vec2{54.0f, 224.0f}, buffer, {198, 198, 206, 255}, 2);

        const float currentRadius = effectiveInitialRingRadius(levelRingRadiusPoints_);
        const float currentSpeed = effectiveInitialRingSpeed(levelRingSpeedPoints_);
        const float draftRadius = effectiveInitialRingRadius(ringWorkshopDraftRadiusPoints_);
        const float draftSpeed = effectiveInitialRingSpeed(ringWorkshopDraftSpeedPoints_);
        std::snprintf(buffer, sizeof(buffer), "再調整後: 半径 %.0f -> %.0f / 速度 %.2f -> %.2f  費用 %dG 月のカケラ%d",
            currentRadius,
            draftRadius,
            currentSpeed,
            draftSpeed,
            ringWorkshopRespecMoneyCost(),
            ringWorkshopRespecMoonCost());
        renderer.drawText(panel.pos + Vec2{54.0f, 448.0f}, buffer, {198, 198, 206, 255}, 2);

        for (int i = 0; i < BaseRingWorkshopItemCount; ++i) {
            if (i == 0) {
                std::snprintf(buffer, sizeof(buffer), "再調整: 速度から半径へ  半径%d / 速度%d",
                    ringWorkshopDraftRadiusPoints_,
                    ringWorkshopDraftSpeedPoints_);
            } else if (i == 1) {
                std::snprintf(buffer, sizeof(buffer), "再調整: 半径から速度へ  半径%d / 速度%d",
                    ringWorkshopDraftRadiusPoints_,
                    ringWorkshopDraftSpeedPoints_);
            } else if (i == 2) {
                std::snprintf(buffer, sizeof(buffer), "再調整確定  %dG 月のカケラ%d",
                    ringWorkshopRespecMoneyCost(),
                    ringWorkshopRespecMoonCost());
            } else if (i >= 3 && i < 3 + RingWorkshopImplementedUpgradeCount) {
                const auto upgrade = static_cast<RingWorkshopUpgrade>(i - 3);
                const int level = ringWorkshopUpgradeLevel(upgrade);
                const int maxLevel = ringWorkshopUpgradeMaxLevel(upgrade);
                if (level >= maxLevel) {
                    std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  上限",
                        ringWorkshopUpgradeName(upgrade),
                        level,
                        maxLevel);
                } else {
                    std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  %.2f -> %.2f  %dG 月のカケラ%d",
                        ringWorkshopUpgradeName(upgrade),
                        level,
                        maxLevel,
                        ringWorkshopUpgradeCurrentValue(upgrade),
                        ringWorkshopUpgradeNextValue(upgrade),
                        ringWorkshopUpgradeMoneyCost(upgrade),
                        ringWorkshopUpgradeMoonCost(upgrade));
                }
            } else {
                const char* futureName = "";
                switch (i) {
                case 6:
                    futureName = "リング投げ距離強化";
                    break;
                case 7:
                    futureName = "リング投げクールダウン短縮";
                    break;
                case 8:
                    futureName = "リング重量ペナルティ軽減";
                    break;
                case 9:
                    futureName = "リング装着枠増加";
                    break;
                default:
                    futureName = "未解禁項目";
                    break;
                }
                std::snprintf(buffer, sizeof(buffer), "%s  未解禁", futureName);
            }
            drawUiButton(renderer, baseRingWorkshopItemRect(i), buffer, i == baseRingWorkshopSelection_, uiActionButtonStyle());
        }

        std::snprintf(buffer, sizeof(buffer), "所持 %dG / 月のカケラ %d   F/Enter 実行  左右で配分変更  Esc/右クリック 戻る",
            money_,
            inventory_.materialCount(MaterialType::MoonFragment));
        renderer.drawText(panel.pos + Vec2{54.0f, 448.0f}, buffer, {198, 198, 206, 255}, 2);
        drawUiButton(renderer, ringWorkshopConfirmRect(), "再調整確定", false, uiActionButtonStyle());
    } else if (baseProcessingActive_) {
        for (int i = 0; i < BaseProcessingModeCount; ++i) {
            drawUiButton(renderer, baseProcessingModeRect(i), processingModeName(static_cast<ProcessingMode>(i)), i == baseProcessingMode_);
        }

        const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
        const auto entryViewForScreenSlot = [this](int slot) {
            InventoryUiEntryView view{};
            if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot)) {
                view.item = &stack->item;
                view.stackCount = stack->count;
                return view;
            }
            if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot)) {
                view.item = &instance->item;
                view.instance = &instance->instance;
                view.stackCount = 1;
            }
            return view;
        };

        for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
            const InventoryUiEntryView view = entryViewForScreenSlot(i);
            const bool unavailable = view.item != nullptr && !processingScreenSlotAvailable(i);
            InventoryUiSlotStyle style{i == baseProcessingSelection_, unavailable, 48.0f};
            if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                style.showTopRightCount = true;
                style.topRightCount = view.stackCount;
            }
            drawInventoryUiSlot(renderer, baseProcessingGridSlotRect(i), view, style);
        }

        const UiRect detailPanel = merchantDetailPanelRect();
        const float moneyRight = detailPanel.pos.x;
        drawMoneySummaryText(renderer, {moneyRight, detailPanel.pos.y + 12.0f}, money_);

        const int selected = std::clamp(baseProcessingSelection_, 0, inventory_.screenSlotCount() - 1);
        const InventoryUiEntryView detailEntry = entryViewForScreenSlot(selected);
        const std::optional<StorageEntry> selectedEntry = processingEntryForScreenSlot(selected);
        drawUiSubPanel(renderer, detailPanel);

        const auto drawSectionLabel = [&renderer](UiRect targetPanel, float& y, std::string_view text) {
            const UiRect content = uiSubPanelContentRect(targetPanel);
            renderer.drawText({content.pos.x, y}, text, ui::Text, 2);
            y += 31.0f;
        };
        const auto drawTextRun = [&renderer](Vec2& pos, std::string_view text, Color color, int scale) {
            renderer.drawText(pos, text, color, scale);
            pos.x += renderer.measureText(text, scale).x;
        };
        const auto drawMoneyCostLine = [&](UiRect targetPanel, float& y, int moneyCost) {
            const UiRect content = uiSubPanelContentRect(targetPanel);
            Vec2 pos{content.pos.x, y};
            const Color numberColor = money_ >= moneyCost ? ui::Text : Color{238, 82, 82, 255};
            const std::string number = std::to_string(moneyCost);
            drawTextRun(pos, number, numberColor, 2);
            drawTextRun(pos, "G", ui::Text, 2);
            y += 31.0f;
        };
        const auto drawOreCostLine = [&](UiRect targetPanel, float& y, int oreCost) {
            const int ownedOre = inventory_.materialCount(MaterialType::EnhancementOre);
            const bool enough = ownedOre >= oreCost;
            const Color numberColor = enough ? ui::Text : Color{238, 82, 82, 255};
            const UiRect content = uiSubPanelContentRect(targetPanel);
            Vec2 pos{content.pos.x, y};
            drawTextRun(pos, "強化鉱石 ×", ui::Text, 2);
            drawTextRun(pos, std::to_string(oreCost), numberColor, 2);
            drawTextRun(pos, " (", ui::Text, 2);
            drawTextRun(pos, std::to_string(ownedOre), numberColor, 2);
            drawTextRun(pos, ")", ui::Text, 2);
            y += 31.0f;
        };

        if (detailEntry.item == nullptr || !selectedEntry) {
            float detailLineY = drawUiDetailHeader(renderer, detailPanel, "アイテム未選択");
            drawUiDetailText(renderer, detailPanel, detailLineY, "加工するアイテムを選択してください。");
        } else {
            const ItemData& item = *detailEntry.item;
            const ItemInstance* instance = detailEntry.instance;
            std::string detailTitle = item.name;
            if (instance == nullptr) {
                std::snprintf(buffer, sizeof(buffer), "%s x%d", item.name.c_str(), detailEntry.stackCount);
                detailTitle = buffer;
            }
            float detailLineY = drawUiDetailHeader(renderer, detailPanel, detailTitle);
            drawUiDetailText(renderer, detailPanel, detailLineY, item.description.empty() ? "-" : item.description);

            const int enhanceLevel = instance != nullptr ? instance->enhanceLevel : 0;
            const int attackBonus = instance != nullptr ? instance->attackBonus : 0;
            const int digBonus = instance != nullptr ? instance->digBonus : 0;
            const int durabilityBonus = instance != nullptr ? instance->durabilityBonus : 0;
            const int currentDurability = instance != nullptr ? instance->currentDurability : item.durability;
            const int maxDurability = instance != nullptr ? instance->maxDurability : item.durability;

            drawSectionLabel(detailPanel, detailLineY, "現在");
            std::snprintf(buffer, sizeof(buffer), "%d / %d", enhanceLevel, MaxItemEnhanceLevel);
            drawUiDetailLine(renderer, detailPanel, detailLineY, "強化Lv", buffer);
            if (maxDurability < 0) {
                std::snprintf(buffer, sizeof(buffer), "壊れない");
            } else {
                std::snprintf(buffer, sizeof(buffer), "%d/%d", currentDurability, maxDurability);
            }
            drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
            std::snprintf(buffer, sizeof(buffer), "+%d / +%d / +%d", attackBonus, digBonus, durabilityBonus);
            drawUiDetailLine(renderer, detailPanel, detailLineY, "補正", buffer);

            const bool available = processingEntryAvailable(*selectedEntry);
            std::string blockedReason;
            if (!available) {
                if (mode == ProcessingMode::Repair && selectedEntry->kind == StorageEntryKind::Stack) {
                    blockedReason = "この作業はできません";
                } else {
                    blockedReason = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
                }
            }

            if (mode == ProcessingMode::Repair) {
                drawSectionLabel(detailPanel, detailLineY, "修理後");
                if (maxDurability < 0 || selectedEntry->kind == StorageEntryKind::Stack) {
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", "-");
                } else {
                    std::snprintf(buffer, sizeof(buffer), "%d/%d -> %d/%d",
                        currentDurability,
                        maxDurability,
                        maxDurability,
                        maxDurability);
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
                }
            } else {
                drawSectionLabel(detailPanel, detailLineY, "強化後");
                std::snprintf(buffer, sizeof(buffer), "%d -> %d", enhanceLevel, std::min(MaxItemEnhanceLevel, enhanceLevel + 1));
                drawUiDetailLine(renderer, detailPanel, detailLineY, "強化Lv", buffer);
                if (mode == ProcessingMode::Attack) {
                    std::snprintf(buffer, sizeof(buffer), "+%d -> +%d", attackBonus, attackBonus + 1);
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "攻撃力", buffer);
                } else if (mode == ProcessingMode::Dig) {
                    std::snprintf(buffer, sizeof(buffer), "+%d -> +%d", digBonus, digBonus + 1);
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "抑制力", buffer);
                } else if (mode == ProcessingMode::Durability) {
                    if (maxDurability < 0) {
                        drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", "-");
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%d -> %d", maxDurability, maxDurability + 2);
                        drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
                    }
                }
            }

            if (available) {
                const int moneyCost = processingMoneyCost(*selectedEntry, mode);
                const int oreCost = processingOreCost(*selectedEntry, mode);
                drawSectionLabel(detailPanel, detailLineY, "強化素材");
                drawMoneyCostLine(detailPanel, detailLineY, moneyCost);
                if (oreCost > 0) {
                    drawOreCostLine(detailPanel, detailLineY, oreCost);
                }
                if (blockedReason.empty()) {
                    if (money_ < moneyCost) {
                        blockedReason = "所持金が足りません";
                    } else if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
                        blockedReason = "強化鉱石が足りません";
                    }
                }
            }

            if (blockedReason.empty()) {
                drawUiDetailText(renderer, detailPanel, detailLineY, mode == ProcessingMode::Repair ? "Enter 修理コマンド" : "Enter 強化コマンド");
            } else {
                drawUiDetailText(renderer, detailPanel, detailLineY, blockedReason);
            }
        }
        const std::array<UiCommandMenuItem, 1> processingCommandItems{{{mode == ProcessingMode::Repair ? "修理する" : "強化する", true}}};
        drawUiCommandMenu(
            renderer,
            baseProcessingCommandMenu_,
            processingCommandItems.data(),
            static_cast<int>(processingCommandItems.size()));
    } else if (baseSellActive_) {
        if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
            renderer.drawText(panel.pos + Vec2{176.0f, 214.0f}, "商人ワゴン", {246, 235, 255, 255}, 3);
            renderer.drawText(panel.pos + Vec2{88.0f, 250.0f}, "何を見ていくんだい？", {198, 198, 206, 255}, 2);
            constexpr std::array<std::string_view, 2> Choices{"買う", "売る"};
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                drawUiButton(renderer, merchantChoiceRect(i), Choices[static_cast<std::size_t>(i)], i == baseMerchantActionSelection_, uiActionButtonStyle());
            }
        } else {
            const bool buyMode = baseMerchantMode_ == MerchantUiMode::Buy;
            const auto entryViewForScreenSlot = [this](int slot) {
                InventoryUiEntryView view{};
                if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot)) {
                    view.item = &stack->item;
                    view.stackCount = stack->count;
                    return view;
                }
                if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot)) {
                    view.item = &instance->item;
                    view.instance = &instance->instance;
                    view.stackCount = 1;
                }
                return view;
            };

            const UiRect detailPanel = merchantDetailPanelRect();
            drawMoneySummaryText(renderer, {detailPanel.pos.x, detailPanel.pos.y + 12.0f}, money_);

            InventoryUiEntryView detailEntry{};
            std::vector<InventoryUiDetailExtraLine> extraLines;
            if (buyMode) {
                if (merchantStock_.empty()) {
                    renderer.drawText({92.0f, 210.0f}, "商品がありません", {198, 198, 206, 255}, 2);
                }
                for (int i = 0; i < static_cast<int>(merchantStock_.size()); ++i) {
                    const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(i)];
                    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
                    InventoryUiEntryView entry{};
                    entry.item = item;
                    entry.stackCount = 1;
                    const bool disabled = !canBuyMerchantProduct(product);
                    std::snprintf(buffer, sizeof(buffer), "%dG", product.price);
                    InventoryUiSlotStyle style{i == baseMerchantBuySelection_, disabled, 48.0f};
                    style.bottomLabel = buffer;
                    style.bottomLabelColor = disabled ? Color{238, 82, 82, 255} : ui::Text;
                    style.showTopRightCount = true;
                    style.topRightCount = product.quantity;
                    drawInventoryUiSlot(renderer, merchantGridSlotRect(i), entry, style);
                }
                if (!merchantStock_.empty()) {
                    const int selected = std::clamp(baseMerchantBuySelection_, 0, static_cast<int>(merchantStock_.size()) - 1);
                    const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(selected)];
                    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
                    detailEntry.item = item;
                    detailEntry.stackCount = 1;
                    std::snprintf(buffer, sizeof(buffer), "%dG", product.price);
                    extraLines.push_back({"価格", buffer, canBuyMerchantProduct(product) ? ui::Text : Color{238, 82, 82, 255}});
                    std::snprintf(buffer, sizeof(buffer), "%d", product.quantity);
                    extraLines.push_back({"在庫", buffer, product.quantity > 0 ? ui::Text : ui::TextDisabled});
                }
            } else {
                for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                    const InventoryUiEntryView view = entryViewForScreenSlot(i);
                    const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(i);
                    const InventoryObjectStack* stack = inventory_.screenObjectStackAt(i);
                    const bool protectedItem = instance != nullptr && instance->instance.protectionEnabled;
                    const UiRect rect = merchantGridSlotRect(i);
                    InventoryUiSlotStyle style{i == baseSellSelection_, protectedItem, 48.0f};
                    if (instance != nullptr) {
                        if (instance->instance.protectionEnabled) {
                            style.bottomLabel = "保護中";
                            style.bottomLabelColor = ui::TextDisabled;
                        } else {
                            std::snprintf(buffer, sizeof(buffer), "%dG", sellPrice(instance->item, &instance->instance));
                            style.bottomLabel = buffer;
                        }
                    } else if (stack != nullptr) {
                        std::snprintf(buffer, sizeof(buffer), "%dG", sellPrice(stack->item));
                        style.bottomLabel = buffer;
                    }
                    drawInventoryUiSlot(renderer, rect, view, style);
                    if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                        std::snprintf(buffer, sizeof(buffer), "x%d", view.stackCount);
                        renderer.drawText(rect.pos + Vec2{rect.size.x - 36.0f, 8.0f}, buffer, {255, 255, 255, 230}, 2);
                    }
                }
                const int selected = std::clamp(baseSellSelection_, 0, inventory_.screenSlotCount() - 1);
                detailEntry = entryViewForScreenSlot(selected);
                if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(selected)) {
                    if (instance->instance.protectionEnabled) {
                        extraLines.push_back({"売値", "保護中", ui::TextDisabled});
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%dG", sellPrice(instance->item, &instance->instance));
                        extraLines.push_back({"売値", buffer, ui::Text});
                    }
                } else if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(selected)) {
                    std::snprintf(buffer, sizeof(buffer), "%dG", sellPrice(stack->item));
                    extraLines.push_back({"売値", buffer, ui::Text});
                }
            }

            drawInventoryUiDetailPanel(renderer, detailPanel, detailEntry, objectCatalog_, encyclopedia_, false, extraLines);
            if (buyMode) {
                const bool buyCommandEnabled = baseMerchantBuyCommandIndex_ >= 0 &&
                    baseMerchantBuyCommandIndex_ < static_cast<int>(merchantStock_.size()) &&
                    canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(baseMerchantBuyCommandIndex_)]);
                const std::array<UiCommandMenuItem, 1> buyItems{{{"買う", buyCommandEnabled}}};
                drawUiCommandMenu(renderer, baseMerchantBuyCommandMenu_, buyItems.data(), static_cast<int>(buyItems.size()));
            } else {
                const bool stackCommand = baseMerchantSellCommandIndex_ >= 0 && inventory_.screenObjectStackAt(baseMerchantSellCommandIndex_) != nullptr;
                const std::array<UiCommandMenuItem, 2> sellItems{{{stackCommand ? "1個売る" : "売る", true}, {"すべて売る", stackCommand}}};
                drawUiCommandMenu(renderer, baseMerchantSellCommandMenu_, sellItems.data(), stackCommand ? 2 : 1);
            }
            if (!baseStatus_.empty()) {
                renderer.drawText({80.0f, 626.0f}, baseStatus_, {255, 230, 150, 255}, 2);
            }
        }
    } else if (baseUpgradeActive_) {
        renderer.drawText(panel.pos + Vec2{152.0f, 118.0f}, "拠点強化炉", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{54.0f, 146.0f}, "拠点拡張・機能解禁", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            if (!upgradeImplemented(i)) {
                std::snprintf(buffer, sizeof(buffer), "%s  未実装", upgradeName(i));
            } else if (upgradeMaxed(i)) {
                std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  上限", upgradeName(i), upgradeLevel(i), upgradeMaxLevel(i));
            } else {
                const MaterialType materialType = upgradeMaterialType(i);
                std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  %dG %s%d",
                    upgradeName(i),
                    upgradeLevel(i),
                    upgradeMaxLevel(i),
                    upgradeCost(i),
                    std::string(materialTypeDisplayName(materialType)).c_str(),
                    upgradeMaterialCost(i));
            }
            drawUiButton(renderer, baseUpgradeItemRect(i), buffer, i == baseUpgradeSelection_, uiActionButtonStyle());
        }
        std::snprintf(buffer, sizeof(buffer), "所持: %dG / 古木 %d / 魔力のしずく %d",
            money_,
            inventory_.materialCount(MaterialType::OldWoodBuildingMaterial),
            inventory_.materialCount(MaterialType::ManaDrop));
        renderer.drawText(panel.pos + Vec2{54.0f, 448.0f}, buffer, {198, 198, 206, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "効果: 倉庫%d枠 / 商人Lv.%d / 加工解禁Lv.%d / HP+%d 半径+%d%% 速度+%d%%",
            warehouseCapacity(),
            merchantUpgradeLevel_,
            processingUnlockLevel_,
            maxHpUpgradeLevel_ * 2,
            ringRadiusUpgradeLevel_ * 8,
            ringSpeedUpgradeLevel_ * 8);
        renderer.drawText(panel.pos + Vec2{54.0f, 474.0f}, buffer, {198, 198, 206, 255}, 2);
    } else if (baseMiningStartChoiceActive_) {
        renderer.drawText(panel.pos + Vec2{178.0f, 238.0f}, "採掘出口", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{116.0f, 278.0f}, "開始地点または最新ワープポイントから出発", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            const bool disabled = (i == 1 && unlockedWarpPointCount_ <= 0) || (i == 2 && !canRegenerateCurrentStage());
            drawUiButton(renderer, baseMiningStartChoiceRect(i), baseMiningStartChoiceName(i), i == baseMiningStartSelection_ && !disabled, uiActionButtonStyle());
            if (disabled) {
                const UiRect rect = baseMiningStartChoiceRect(i);
                renderer.fillRect(rect.pos, rect.size, {0, 0, 0, 110});
                renderer.drawText(rect.pos + Vec2{116.0f, 14.0f}, "未解放", {150, 150, 160, 255}, 2);
            }
        }
    } else {
        const char* promptName = nearest != nullptr ? nearest->displayName : "";
        renderer.fillRect({280.0f, 644.0f}, {720.0f, 34.0f}, ui::FooterFill);
        if (nearest != nullptr) {
            if (const char* specialPrompt = baseInteractionPrompt(*nearest)) {
                std::snprintf(buffer, sizeof(buffer), "%s", specialPrompt);
            } else {
                std::snprintf(buffer, sizeof(buffer), "Enter: %sを調べる / クリック: 近くの施設を調べる / Esc: メニュー", promptName);
            }
            renderer.drawText({300.0f, 652.0f}, buffer, {255, 230, 150, 255}, 2);
        } else {
            renderer.drawText({300.0f, 652.0f}, "Enter: 近くの施設を調べる  Esc: メニュー", {198, 198, 206, 255}, 2);
        }
        if (!baseStatus_.empty()) {
            renderer.fillRect({280.0f, 604.0f}, {720.0f, 30.0f}, {0, 0, 0, 160});
            renderer.drawText({300.0f, 612.0f}, baseStatus_, {255, 230, 150, 255}, 2);
        }
        return;
    }

    if (!baseStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{54.0f, 504.0f}, baseStatus_, {255, 230, 150, 255}, 2);
    }
}

} // namespace majo
