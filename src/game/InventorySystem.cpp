#include "game/InventorySystem.hpp"

#include "engine/Log.hpp"
#include "game/EffectDispatcher.hpp"
#include "game/EncyclopediaSystem.hpp"
#include "game/ObjectImageRenderer.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace majo {

namespace {
constexpr int InventoryColumns = 8;
constexpr int InventoryRows = 3;
constexpr float PanelX = 790.0f;
constexpr float PanelY = 70.0f;
constexpr float PanelW = 430.0f;
constexpr float PanelH = 440.0f;
constexpr float RowX = PanelX + 24.0f;
constexpr float RowW = PanelW - 48.0f;
constexpr float RowH = 58.0f;
constexpr float HudMargin = 16.0f;
constexpr float HudHeight = 112.0f;
constexpr float HudSlotGap = 6.0f;
constexpr float HudSlotHeight = 48.0f;
constexpr float ScreenX = 44.0f;
constexpr float ScreenY = 58.0f;
constexpr float ScreenW = 1192.0f;
constexpr float ScreenH = 610.0f;
constexpr float ScreenGridX = ScreenX + 28.0f;
constexpr float ScreenGridY = ScreenY + 84.0f;
constexpr float ScreenSlotW = 88.0f;
constexpr float ScreenSlotH = 76.0f;
constexpr float ScreenSlotGap = 8.0f;
constexpr float InventoryObjectImageMaxSize = 48.0f;
constexpr float ShortcutObjectImageMaxSize = 48.0f;
constexpr float SlotDragStartDistanceSq = 36.0f;
constexpr float DetailX = ScreenX + 820.0f;
constexpr float DetailY = ScreenY + 50.0f;
constexpr float DetailW = 330.0f;
constexpr float DetailH = 520.0f;
UiRect panelRect()
{
    return {{PanelX, PanelY}, {PanelW, PanelH}};
}

UiRect rowRect(int index)
{
    return {{RowX, PanelY + 122.0f + static_cast<float>(index) * 74.0f}, {RowW, RowH}};
}

UiRect useButtonRect()
{
    return {{PanelX + 26.0f, PanelY + 350.0f}, {130.0f, ui::ButtonHeight}};
}

UiRect ringButtonRect()
{
    return {{PanelX + 166.0f, PanelY + 350.0f}, {130.0f, ui::ButtonHeight}};
}

UiRect closeButtonRect()
{
    return {{PanelX + PanelW - 70.0f, PanelY + 18.0f}, {46.0f, 34.0f}};
}

UiRect inventoryScreenRect()
{
    return {{ScreenX, ScreenY}, {ScreenW, ScreenH}};
}

UiRect inventorySlotRect(int index)
{
    const int row = index / InventoryColumns;
    const int column = index % InventoryColumns;
    return {{
        ScreenGridX + static_cast<float>(column) * (ScreenSlotW + ScreenSlotGap),
        ScreenGridY + static_cast<float>(row) * (ScreenSlotH + ScreenSlotGap)
    }, {ScreenSlotW, ScreenSlotH}};
}

Vec2 uiRectCenter(const UiRect& rect)
{
    return rect.pos + rect.size * 0.5f;
}

float inventorySlotFrameRadius(const UiRect& rect)
{
    return std::min(rect.size.x, rect.size.y) * 0.5f;
}

void drawSelectedItemCircleOutline(Renderer& renderer, Vec2 center, float radius)
{
    const Color outline = selectedItemOutlineColor();
    for (int i = 0; i < 6; ++i) {
        renderer.drawCircle(center, radius + static_cast<float>(i), outline);
    }
}

bool objectCategoryEquals(const ItemData& item, std::string_view category)
{
    return item.category == category;
}

bool objectHasTag(const ItemData& item, std::string_view tag)
{
    return std::any_of(item.tags.begin(), item.tags.end(), [tag](const std::string& candidate) {
        return candidate == tag;
    });
}

bool isCapturedObject(const ItemData& item)
{
    return item.id.rfind("captured_", 0) == 0 || !item.capturedBehaviorIds.empty();
}

bool materialTypeForObject(const ItemData& item, MaterialType& outType)
{
    if (item.id == "old_wood_building_material" || item.id == "material_old_wood" ||
        item.name == "\xE5\x8F\xA4\xE6\x9C\xA8\xE3\x81\xAE\xE5\xBB\xBA\xE6\x9D\x90") {
        outType = MaterialType::OldWoodBuildingMaterial;
        return true;
    }
    if (item.id == "enhancement_ore" || item.id == "material_enhancement_ore" ||
        item.name == "\xE5\xBC\xB7\xE5\x8C\x96\xE9\x89\xB1\xE7\x9F\xB3") {
        outType = MaterialType::EnhancementOre;
        return true;
    }
    if (item.id == "moon_fragment" || item.id == "material_moon_fragment" ||
        item.name == "\xE6\x9C\x88\xE3\x81\xAE\xE3\x82\xAB\xE3\x82\xB1\xE3\x83\xA9") {
        outType = MaterialType::MoonFragment;
        return true;
    }
    if (item.id == "mana_drop" || item.id == "material_mana_drop" ||
        item.name == "\xE9\xAD\x94\xE5\x8A\x9B\xE3\x81\xAE\xE3\x81\x97\xE3\x81\x9A\xE3\x81\x8F") {
        outType = MaterialType::ManaDrop;
        return true;
    }
    return false;
}

bool isStackableObject(const ItemData& item)
{
    if (isCapturedObject(item)) {
        return false;
    }
    if (objectCategoryEquals(item, "\xE5\x9B\x9E\xE5\xBE\xA9") ||
        objectCategoryEquals(item, "\xE5\xBC\xB7\xE5\x8C\x96") ||
        objectCategoryEquals(item, "\xE5\xBC\xB1\xE4\xBD\x93")) {
        return true;
    }
    if (objectCategoryEquals(item, "\xE6\xAD\xA6\xE5\x99\xA8") ||
        objectCategoryEquals(item, "\xE7\x9B\xBE") ||
        objectCategoryEquals(item, "\xE6\x8E\x98\xE5\x89\x8A") ||
        objectCategoryEquals(item, "\xE6\x8E\xA2\xE7\xB4\xA2") ||
        objectCategoryEquals(item, "\xE9\xAD\x94\xE5\xB0\x8E\xE6\x9B\xB8") ||
        objectCategoryEquals(item, "\xE5\xAE\x9D")) {
        return false;
    }
    return objectHasTag(item, "consumable") ||
        objectHasTag(item, "food") ||
        objectHasTag(item, "potion");
}

Color inventoryObjectColor(const ItemData& item)
{
    if (objectCategoryEquals(item, "\xE5\x9B\x9E\xE5\xBE\xA9")) {
        return {116, 220, 144, 255};
    }
    if (objectCategoryEquals(item, "\xE6\xAD\xA6\xE5\x99\xA8")) {
        return {224, 96, 86, 255};
    }
    if (objectCategoryEquals(item, "\xE7\x9B\xBE")) {
        return {104, 168, 226, 255};
    }
    if (objectCategoryEquals(item, "\xE5\xAE\x9D")) {
        return {244, 206, 78, 255};
    }
    if (objectCategoryEquals(item, "\xE6\x8E\xA2\xE7\xB4\xA2")) {
        return {136, 214, 214, 255};
    }
    return {188, 152, 236, 255};
}

std::string joinEffectLines(const std::vector<std::string>& lines)
{
    if (lines.empty()) {
        return "-";
    }
    std::string text;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (!text.empty()) {
            text += '\n';
        }
        text += "\xE3\x83\xBB";
        text += lines[i];
    }
    return text;
}

}

void InventorySystem::clearObjectStacks()
{
    objectStacks_.clear();
    objectInstances_.clear();
}

std::vector<StackItem> InventorySystem::stackItemsForSave() const
{
    std::vector<StackItem> items;
    items.reserve(objectStacks_.size());
    for (const InventoryObjectStack& stack : objectStacks_) {
        if (!stack.objectId.empty() && stack.count > 0) {
            items.push_back(StackItem{
                .objectId = stack.objectId,
                .count = stack.count,
            });
        }
    }
    return items;
}

void InventorySystem::addMaterial(MaterialType type, int count)
{
    materials_.add(type, count);
}

void InventorySystem::setMaterialCount(MaterialType type, int count)
{
    materials_.setCount(type, count);
}

int InventorySystem::materialCount(MaterialType type) const
{
    return materials_.count(type);
}

bool InventorySystem::setObjectItemCount(const ObjectCatalog& catalog, std::string_view objectId, int count)
{
    if (objectId.empty() || count <= 0) {
        return false;
    }
    const ItemData* item = catalog.registry.findById(objectId);
    const ItemData missingItem = item == nullptr ? makeMissingItemData(objectId) : ItemData{};
    const ItemData& resolvedItem = item != nullptr ? *item : missingItem;
    if (item == nullptr) {
        logError("[warning] SaveData: object_id=\"" + std::string(objectId) + "\" is missing from Objects DB; restored as missing stack item");
    }

    for (InventoryObjectStack& stack : objectStacks_) {
        if (stack.objectId == resolvedItem.id) {
            stack.count = count;
            stack.item = resolvedItem;
            return true;
        }
    }

    objectStacks_.push_back(InventoryObjectStack{resolvedItem, count});
    return true;
}

bool InventorySystem::addObjectInstance(const ObjectCatalog& catalog, ItemInstance instance)
{
    if (instance.instanceId.empty() || instance.objectId.empty()) {
        return false;
    }
    const ItemData* item = catalog.registry.findById(instance.objectId);
    const ItemData missingItem = item == nullptr ? makeMissingItemData(instance.objectId) : ItemData{};
    const ItemData& resolvedItem = item != nullptr ? *item : missingItem;
    if (item == nullptr) {
        logError("[warning] SaveData: object_id=\"" + instance.objectId + "\" is missing from Objects DB; restored as missing ItemInstance");
    }
    if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
        return false;
    }

    objectInstances_.push_back(InventoryObjectInstance{
        .item = resolvedItem,
        .instance = std::move(instance),
    });
    const std::string& storedId = objectInstances_.back().instance.instanceId;
    constexpr std::string_view Prefix = "iteminst_";
    if (storedId.rfind(Prefix, 0) == 0) {
        const unsigned long long parsed = std::strtoull(storedId.c_str() + Prefix.size(), nullptr, 10);
        nextInstanceId_ = std::max(nextInstanceId_, parsed + 1);
    }
    return true;
}

bool InventorySystem::removeObjectItemCount(std::string_view objectId, int count)
{
    if (objectId.empty() || count <= 0) {
        return false;
    }

    for (auto it = objectStacks_.begin(); it != objectStacks_.end(); ++it) {
        if (it->objectId != objectId || it->count < count) {
            continue;
        }

        it->count -= count;
        if (it->count <= 0) {
            const int stackIndex = static_cast<int>(std::distance(objectStacks_.begin(), it));
            removePackedSlotAtPackedIndex(stackIndex);
            objectStacks_.erase(it);
        }
        status_ = "売却しました";
        return true;
    }

    return false;
}

bool InventorySystem::removeObjectInstance(std::string_view instanceId)
{
    if (instanceId.empty()) {
        return false;
    }
    const auto it = std::find_if(objectInstances_.begin(), objectInstances_.end(), [instanceId](const InventoryObjectInstance& entry) {
        return entry.instance.instanceId == instanceId;
    });
    if (it == objectInstances_.end()) {
        return false;
    }
    if (it->instance.protectionEnabled) {
        status_ = "保護中は売却不可";
        return false;
    }
    const int instanceIndex = static_cast<int>(std::distance(objectInstances_.begin(), it));
    removePackedSlotAtPackedIndex(static_cast<int>(objectStacks_.size()) + instanceIndex);
    objectInstances_.erase(it);
    status_ = "売却しました";
    return true;
}

bool InventorySystem::takeObjectInstance(std::string_view instanceId, InventoryObjectInstance& outInstance)
{
    if (instanceId.empty()) {
        return false;
    }
    const auto it = std::find_if(objectInstances_.begin(), objectInstances_.end(), [instanceId](const InventoryObjectInstance& entry) {
        return entry.instance.instanceId == instanceId;
    });
    if (it == objectInstances_.end()) {
        return false;
    }
    outInstance = std::move(*it);
    const int instanceIndex = static_cast<int>(std::distance(objectInstances_.begin(), it));
    removePackedSlotAtPackedIndex(static_cast<int>(objectStacks_.size()) + instanceIndex);
    objectInstances_.erase(it);
    status_ = "倉庫へ預けました";
    return true;
}

bool InventorySystem::repairObjectInstance(std::string_view instanceId)
{
    if (instanceId.empty()) {
        return false;
    }
    const auto it = std::find_if(objectInstances_.begin(), objectInstances_.end(), [instanceId](const InventoryObjectInstance& entry) {
        return entry.instance.instanceId == instanceId;
    });
    if (it == objectInstances_.end()) {
        return false;
    }
    ItemInstance& instance = it->instance;
    if (instance.maxDurability < 0) {
        status_ = "修理不要";
        return false;
    }
    instance.currentDurability = instance.maxDurability;
    instance.isBroken = false;
    status_ = "修理しました";
    return true;
}

bool InventorySystem::enhanceObjectInstance(std::string_view instanceId, int attackBonus, int digBonus, int durabilityBonus, int maxEnhanceLevel)
{
    if (instanceId.empty()) {
        return false;
    }
    const auto it = std::find_if(objectInstances_.begin(), objectInstances_.end(), [instanceId](const InventoryObjectInstance& entry) {
        return entry.instance.instanceId == instanceId;
    });
    if (it == objectInstances_.end()) {
        return false;
    }

    ItemInstance& instance = it->instance;
    if (instance.enhanceLevel >= maxEnhanceLevel) {
        status_ = "強化上限です";
        return false;
    }
    ++instance.enhanceLevel;
    instance.attackBonus += attackBonus;
    instance.digBonus += digBonus;
    instance.durabilityBonus += durabilityBonus;
    if (durabilityBonus > 0 && instance.maxDurability >= 0) {
        instance.maxDurability += durabilityBonus;
        instance.currentDurability = std::min(instance.maxDurability, std::max(0, instance.currentDurability + durabilityBonus));
    }
    status_ = "個体強化しました";
    return true;
}

bool InventorySystem::enhanceObjectStackItem(std::string_view objectId, int attackBonus, int digBonus, int durabilityBonus, int maxEnhanceLevel)
{
    if (objectId.empty()) {
        return false;
    }
    const auto it = std::find_if(objectStacks_.begin(), objectStacks_.end(), [objectId](const InventoryObjectStack& stack) {
        return stack.objectId == objectId && stack.count > 0;
    });
    if (it == objectStacks_.end()) {
        return false;
    }
    const bool stackSlotWillRemain = it->count > 1;
    if (stackSlotWillRemain && static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
        status_ = "インベントリ満杯";
        return false;
    }

    createObjectInstance(it->item);
    ItemInstance& itemInstance = objectInstances_.back().instance;
    if (itemInstance.enhanceLevel >= maxEnhanceLevel) {
        return false;
    }
    ++itemInstance.enhanceLevel;
    itemInstance.attackBonus += attackBonus;
    itemInstance.digBonus += digBonus;
    itemInstance.durabilityBonus += durabilityBonus;
    if (durabilityBonus > 0 && itemInstance.maxDurability >= 0) {
        itemInstance.maxDurability += durabilityBonus;
        itemInstance.currentDurability = std::min(itemInstance.maxDurability, std::max(0, itemInstance.currentDurability + durabilityBonus));
    }

    --it->count;
    if (it->count <= 0) {
        const int stackIndex = static_cast<int>(std::distance(objectStacks_.begin(), it));
        removePackedSlotAtPackedIndex(stackIndex);
        objectStacks_.erase(it);
    }
    status_ = "個体化して強化しました";
    return true;
}

bool InventorySystem::enhanceSelectedObjectInstance(int attackBonus, int digBonus, int durabilityBonus)
{
    InventoryObjectInstance* selectedInstance = selectedObjectInstance();
    if (selectedInstance == nullptr) {
        InventoryObjectStack* selectedStack = objectStackAtScreenIndex(selectedShortcutIndex());
        if (selectedStack == nullptr || selectedStack->count <= 0) {
            status_ = "強化対象がありません";
            return false;
        }
        if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
            status_ = "インベントリ満杯";
            return false;
        }
        createObjectInstance(selectedStack->item);
        --selectedStack->count;
        if (selectedStack->count <= 0) {
            const int objectIndex = stackIndexAtScreenIndex(selectedShortcutIndex());
            removePackedSlotAtPackedIndex(objectIndex);
            objectStacks_.erase(objectStacks_.begin() + objectIndex);
        }
        selectedInstance = &objectInstances_.back();
    }

    ItemInstance& instance = selectedInstance->instance;
    ++instance.enhanceLevel;
    instance.attackBonus += attackBonus;
    instance.digBonus += digBonus;
    instance.durabilityBonus += durabilityBonus;
    if (instance.maxDurability >= 0) {
        instance.maxDurability += durabilityBonus;
        instance.currentDurability = std::min(instance.maxDurability, std::max(0, instance.currentDurability + durabilityBonus));
    }
    status_ = "個体強化しました";
    return true;
}

const ShortcutSlot& InventorySystem::selectedShortcutSlot() const
{
    return shortcutSlots_[selectedShortcutIndex()];
}

ShortcutSlot& InventorySystem::selectedShortcutSlot()
{
    return shortcutSlots_[selectedShortcutIndex()];
}

const InventoryObjectStack* InventorySystem::objectStackAtScreenIndex(int index) const
{
    const int objectIndex = stackIndexAtScreenIndex(index);
    if (objectIndex < 0) {
        return nullptr;
    }
    return &objectStacks_[static_cast<std::size_t>(objectIndex)];
}

InventoryObjectStack* InventorySystem::objectStackAtScreenIndex(int index)
{
    const int objectIndex = stackIndexAtScreenIndex(index);
    if (objectIndex < 0) {
        return nullptr;
    }
    return &objectStacks_[static_cast<std::size_t>(objectIndex)];
}

const InventoryObjectInstance* InventorySystem::objectInstanceAtScreenIndex(int index) const
{
    const int instanceIndex = instanceIndexAtScreenIndex(index);
    if (instanceIndex < 0) {
        return nullptr;
    }
    return &objectInstances_[static_cast<std::size_t>(instanceIndex)];
}

InventoryObjectInstance* InventorySystem::objectInstanceAtScreenIndex(int index)
{
    const int instanceIndex = instanceIndexAtScreenIndex(index);
    if (instanceIndex < 0) {
        return nullptr;
    }
    return &objectInstances_[static_cast<std::size_t>(instanceIndex)];
}

std::string InventorySystem::allocateInstanceId()
{
    return "iteminst_" + std::to_string(nextInstanceId_++);
}

InventoryObjectInstance InventorySystem::createObjectInstance(const ItemData& item)
{
    ItemInstance instance = makeItemInstanceFromDefinition(allocateInstanceId(), item);
    InventoryObjectInstance objectInstance{
        .item = item,
        .instance = std::move(instance),
    };
    objectInstances_.push_back(objectInstance);
    return objectInstance;
}

const InventoryObjectStack* InventorySystem::selectedObjectStack() const
{
    if (selectedShortcutSlot().assigned) {
        return nullptr;
    }
    return objectStackAtScreenIndex(selectedShortcutIndex());
}

InventoryObjectInstance* InventorySystem::selectedObjectInstance()
{
    if (selectedShortcutSlot().assigned) {
        return nullptr;
    }
    return objectInstanceAtScreenIndex(selectedShortcutIndex());
}

const InventoryObjectInstance* InventorySystem::selectedObjectInstance() const
{
    if (selectedShortcutSlot().assigned) {
        return nullptr;
    }
    return objectInstanceAtScreenIndex(selectedShortcutIndex());
}

int InventorySystem::selectedShortcutIndex() const
{
    return shortcutRow_ * ShortcutColumns + selectedShortcutColumn_;
}

bool InventorySystem::addObjectItem(const ObjectCatalog& catalog, std::string_view objectId)
{
    if (objectId.empty()) {
        logError("[warning] InventorySystem: empty object ID cannot be added");
        return false;
    }
    if (catalog.registry.empty()) {
        logError("[warning] InventorySystem: Objects DB is empty; cannot add object_id=\"" + std::string(objectId) + "\"");
        return false;
    }

    const ItemData* item = catalog.registry.findById(objectId);
    if (item == nullptr) {
        logError("[warning] InventorySystem: invalid object_id=\"" + std::string(objectId) + "\" cannot be added");
        return false;
    }

    MaterialType materialType = MaterialType::Count;
    if (materialTypeForObject(*item, materialType)) {
        materials_.add(materialType, 1);
        status_ = "素材入手: " + std::string(materialTypeDisplayName(materialType));
        return true;
    }

    if (!isStackableObject(*item)) {
        if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
            status_ = "インベントリ満杯";
            return false;
        }
        createObjectInstance(*item);
        status_ = "Picked up: " + item->name;
        return true;
    }

    for (InventoryObjectStack& stack : objectStacks_) {
        if (stack.objectId == item->id) {
            ++stack.count;
            stack.item = *item;
            status_ = "Picked up: " + item->name;
            return true;
        }
    }

    if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
        status_ = "インベントリ満杯";
        return false;
    }

    objectStacks_.push_back(InventoryObjectStack{*item, 1});
    status_ = "Picked up: " + item->name;
    return true;
}

bool InventorySystem::addRuntimeObjectItem(const ItemData& item)
{
    if (item.id.empty()) {
        return false;
    }

    MaterialType materialType = MaterialType::Count;
    if (materialTypeForObject(item, materialType)) {
        materials_.add(materialType, 1);
        status_ = "素材入手: " + std::string(materialTypeDisplayName(materialType));
        return true;
    }

    if (!isStackableObject(item)) {
        if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
            status_ = "インベントリ満杯";
            return false;
        }
        createObjectInstance(item);
        status_ = "Picked up: " + item.name;
        return true;
    }

    for (InventoryObjectStack& stack : objectStacks_) {
        if (stack.objectId == item.id) {
            ++stack.count;
            stack.item = item;
            stack.objectId = item.id;
            status_ = "Picked up: " + item.name;
            return true;
        }
    }

    if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
        status_ = "インベントリ満杯";
        return false;
    }

    objectStacks_.push_back(InventoryObjectStack{item, 1});
    status_ = "Picked up: " + item.name;
    return true;
}

void InventorySystem::moveShortcutCursor(int delta)
{
    selectedShortcutColumn_ = (selectedShortcutColumn_ + delta) % ShortcutColumns;
    if (selectedShortcutColumn_ < 0) {
        selectedShortcutColumn_ += ShortcutColumns;
    }
}

void InventorySystem::moveShortcutCursorGrid(int dx, int dy)
{
    selectedShortcutColumn_ = (selectedShortcutColumn_ + dx) % ShortcutColumns;
    if (selectedShortcutColumn_ < 0) {
        selectedShortcutColumn_ += ShortcutColumns;
    }
    shortcutRow_ = (shortcutRow_ + dy) % ShortcutRows;
    if (shortcutRow_ < 0) {
        shortcutRow_ += ShortcutRows;
    }
}

void InventorySystem::selectShortcutSlot(int slot)
{
    if (slot < 0 || slot >= ShortcutColumns) {
        return;
    }
    selectedShortcutColumn_ = slot;
}

void InventorySystem::selectShortcutIndex(int index)
{
    if (index < 0 || index >= ShortcutSlotCount) {
        return;
    }
    shortcutRow_ = index / ShortcutColumns;
    selectedShortcutColumn_ = index % ShortcutColumns;
}

void InventorySystem::syncPackedItemSlots() const
{
    const int totalCount = static_cast<int>(objectStacks_.size() + objectInstances_.size());
    if (totalCount <= 0) {
        packedItemSlots_.clear();
        return;
    }

    std::vector<int> nextSlots(static_cast<std::size_t>(totalCount), -1);
    std::array<bool, ShortcutSlotCount> used{};
    const int copyCount = std::min(totalCount, static_cast<int>(packedItemSlots_.size()));
    for (int i = 0; i < copyCount; ++i) {
        const int slot = packedItemSlots_[static_cast<std::size_t>(i)];
        if (slot >= 0 && slot < ShortcutSlotCount && !used[static_cast<std::size_t>(slot)]) {
            nextSlots[static_cast<std::size_t>(i)] = slot;
            used[static_cast<std::size_t>(slot)] = true;
        }
    }

    int cursor = 0;
    for (int i = 0; i < totalCount; ++i) {
        if (nextSlots[static_cast<std::size_t>(i)] >= 0) {
            continue;
        }
        while (cursor < ShortcutSlotCount && used[static_cast<std::size_t>(cursor)]) {
            ++cursor;
        }
        if (cursor >= ShortcutSlotCount) {
            nextSlots[static_cast<std::size_t>(i)] = i % ShortcutSlotCount;
        } else {
            nextSlots[static_cast<std::size_t>(i)] = cursor;
            used[static_cast<std::size_t>(cursor)] = true;
            ++cursor;
        }
    }
    packedItemSlots_ = std::move(nextSlots);
}

int InventorySystem::packedItemIndexAtScreenIndex(int index) const
{
    syncPackedItemSlots();
    for (int i = 0; i < static_cast<int>(packedItemSlots_.size()); ++i) {
        if (packedItemSlots_[static_cast<std::size_t>(i)] == index) {
            return i;
        }
    }
    return -1;
}

int InventorySystem::stackIndexAtScreenIndex(int index) const
{
    const int packedIndex = packedItemIndexAtScreenIndex(index);
    const int stackCount = static_cast<int>(objectStacks_.size());
    if (packedIndex < 0 || packedIndex >= stackCount) {
        return -1;
    }
    return packedIndex;
}

int InventorySystem::instanceIndexAtScreenIndex(int index) const
{
    const int packedIndex = packedItemIndexAtScreenIndex(index);
    const int stackCount = static_cast<int>(objectStacks_.size());
    const int instanceIndex = packedIndex - stackCount;
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(objectInstances_.size())) {
        return -1;
    }
    return instanceIndex;
}

void InventorySystem::removePackedSlotAtPackedIndex(int packedIndex) const
{
    syncPackedItemSlots();
    if (packedIndex < 0 || packedIndex >= static_cast<int>(packedItemSlots_.size())) {
        return;
    }
    packedItemSlots_.erase(packedItemSlots_.begin() + packedIndex);
}

bool InventorySystem::hasScreenItem(int index) const
{
    return objectStackAtScreenIndex(index) != nullptr || objectInstanceAtScreenIndex(index) != nullptr;
}

bool InventorySystem::canUseScreenItem(int index) const
{
    if (const InventoryObjectStack* objectStack = objectStackAtScreenIndex(index)) {
        return !objectStack->item.normalEffects.empty();
    }
    if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(index)) {
        return !objectInstance->instance.isBroken && !objectInstance->item.normalEffects.empty();
    }
    return false;
}

std::array<UiCommandMenuItem, 3> InventorySystem::buildSlotCommandItems(int slotIndex) const
{
    const bool hasItem = hasScreenItem(slotIndex);
    std::array<UiCommandMenuItem, 3> items{{
        {"使用する", canUseScreenItem(slotIndex)},
        {"リングへ", hasItem},
        {"保護", objectInstanceAtScreenIndex(slotIndex) != nullptr},
    }};
    return items;
}

bool InventorySystem::moveScreenItem(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= ShortcutSlotCount || toIndex < 0 || toIndex >= ShortcutSlotCount) {
        return false;
    }
    const int fromPacked = packedItemIndexAtScreenIndex(fromIndex);
    if (fromPacked < 0) {
        return false;
    }
    const int toPacked = packedItemIndexAtScreenIndex(toIndex);
    syncPackedItemSlots();
    if (toPacked < 0) {
        packedItemSlots_[static_cast<std::size_t>(fromPacked)] = toIndex;
        return true;
    }
    std::swap(
        packedItemSlots_[static_cast<std::size_t>(fromPacked)],
        packedItemSlots_[static_cast<std::size_t>(toPacked)]);
    return true;
}

void InventorySystem::toggleShortcutRow()
{
    shortcutRow_ = (shortcutRow_ + 1) % ShortcutRows;
}

void InventorySystem::grabOrPlaceSelected()
{
    if (grabbedSlotActive_) {
        placeGrabbedAtSelected();
        return;
    }
    const int index = selectedShortcutIndex();
    if (!hasScreenItem(index)) {
        status_ = "アイテム未選択";
        return;
    }
    grabbedSlotOrigin_ = index;
    grabbedSlotActive_ = true;
    status_ = "つかみ中";
}

void InventorySystem::placeGrabbedAtSelected()
{
    if (!grabbedSlotActive_) {
        return;
    }
    const int targetIndex = selectedShortcutIndex();
    if (moveScreenItem(grabbedSlotOrigin_, targetIndex)) {
        status_ = grabbedSlotOrigin_ == targetIndex ? "配置" : "入れ替え";
    }
    grabbedSlotActive_ = false;
    grabbedSlotOrigin_ = -1;
}

void InventorySystem::cancelGrab()
{
    if (!grabbedSlotActive_) {
        return;
    }
    grabbedSlotActive_ = false;
    grabbedSlotOrigin_ = -1;
    status_ = "キャンセル";
}

void InventorySystem::openSlotCommandMenu(int slotIndex)
{
    if (!hasScreenItem(slotIndex)) {
        closeUiCommandMenu(slotCommandMenu_);
        slotCommandMenuIndex_ = -1;
        return;
    }
    const std::array<UiCommandMenuItem, 3> commandItems = buildSlotCommandItems(slotIndex);
    slotCommandMenuIndex_ = slotIndex;
    const UiRect slotRect = inventorySlotRect(slotIndex);
    openUiCommandMenu(
        slotCommandMenu_,
        slotRect.pos + Vec2{slotRect.size.x - 20.0f, 0.0f},
        inventoryScreenRect(),
        static_cast<int>(commandItems.size()),
        commandItems.data(),
        120.0f,
        2);
}

void InventorySystem::resetSlotPointerPress()
{
    slotPointerPressIndex_ = -1;
    slotPointerPressMouse_ = {};
    slotPointerPressCanOpenMenu_ = false;
    slotPointerDragTriggered_ = false;
}

bool InventorySystem::addObjectSelectionToRing(SpellRingSystem& spellRing)
{
    const int index = selectedShortcutIndex();
    InventoryObjectStack* selected = objectStackAtScreenIndex(index);
    if (selected == nullptr || selected->count <= 0) {
        status_ = "Objects DB item not selected";
        return false;
    }
    if (!spellRing.canAddItem()) {
        status_ = "リング満杯";
        return false;
    }
    InventoryObjectInstance objectInstance = createObjectInstance(selected->item);
    if (!spellRing.addObjectItem(selected->item, objectInstance.instance)) {
        status_ = "リング満杯";
        objectInstances_.pop_back();
        return false;
    }
    objectInstances_.pop_back();

    status_ = "リングに追加: " + selected->item.name;
    --selected->count;
    if (selected->count <= 0) {
        const int objectIndex = stackIndexAtScreenIndex(index);
        removePackedSlotAtPackedIndex(objectIndex);
        objectStacks_.erase(objectStacks_.begin() + objectIndex);
    }
    return true;
}

bool InventorySystem::addObjectInstanceSelectionToRing(SpellRingSystem& spellRing)
{
    const int index = selectedShortcutIndex();
    InventoryObjectInstance* selected = objectInstanceAtScreenIndex(index);
    if (selected == nullptr) {
        status_ = "個体アイテム未選択";
        return false;
    }
    if (selected->instance.isBroken) {
        status_ = "壊れています";
        return false;
    }
    if (!spellRing.addObjectItem(selected->item, selected->instance)) {
        status_ = "リング満杯";
        return false;
    }
    status_ = "リングに追加: " + selected->item.name;
    const int instanceIndex = instanceIndexAtScreenIndex(index);
    removePackedSlotAtPackedIndex(static_cast<int>(objectStacks_.size()) + instanceIndex);
    objectInstances_.erase(objectInstances_.begin() + instanceIndex);
    return true;
}

bool InventorySystem::useObjectSelection(
    Player& player,
    const EffectDispatcher& effectDispatcher,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    const int index = selectedShortcutIndex();
    InventoryObjectStack* selected = objectStackAtScreenIndex(index);
    if (selected == nullptr || selected->count <= 0) {
        status_ = "Objects DB item not selected";
        return false;
    }
    if (selected->item.normalEffects.empty()) {
        status_ = "通常効果なし";
        return false;
    }

    bool hasImplementedEffect = false;
    for (const EffectSpec& spec : selected->item.normalEffects) {
        for (const std::string& effect : spec.effects) {
            if (effectDispatcher.hasHandler(effect)) {
                hasImplementedEffect = true;
                break;
            }
        }
        if (hasImplementedEffect) {
            break;
        }
    }

    if (!hasImplementedEffect) {
        EffectContext context;
        context.owner = &player;
        context.discoveryEvents = discoveryEvents;
        context.encyclopedia = encyclopedia;
        context.position = player.position;
        context.triggerType = EffectTriggerType::NormalUse;
        context.logUnimplementedEffects = true;
        effectDispatcher.dispatchNormalEffects(selected->item, context);
        status_ = "未実装効果";
        return false;
    }

    const int beforeHp = player.hp;
    EffectContext context;
    context.owner = &player;
    context.discoveryEvents = discoveryEvents;
    context.encyclopedia = encyclopedia;
    context.position = player.position;
    context.triggerType = EffectTriggerType::NormalUse;
    context.logUnimplementedEffects = true;
    effectDispatcher.dispatchNormalEffects(selected->item, context);

    if (player.hp == beforeHp && beforeHp >= player.maxHp) {
        bool healOnly = true;
        for (const EffectSpec& spec : selected->item.normalEffects) {
            for (const std::string& effect : spec.effects) {
                if (effect != "heal") {
                    healOnly = false;
                    break;
                }
            }
            if (!healOnly) {
                break;
            }
        }
        if (healOnly) {
            status_ = "体力は満タン";
            return false;
        }
    }

    status_ = "使用: " + selected->item.name;
    --selected->count;
    if (selected->count <= 0) {
        const int objectIndex = stackIndexAtScreenIndex(index);
        removePackedSlotAtPackedIndex(objectIndex);
        objectStacks_.erase(objectStacks_.begin() + objectIndex);
    }
    return true;
}

bool InventorySystem::useObjectInstanceSelection(
    Player& player,
    const EffectDispatcher& effectDispatcher,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    InventoryObjectInstance* selected = selectedObjectInstance();
    if (selected == nullptr) {
        status_ = "個体アイテム未選択";
        return false;
    }
    if (selected->instance.isBroken) {
        status_ = "壊れています";
        return false;
    }
    if (selected->item.normalEffects.empty()) {
        status_ = "通常効果なし";
        return false;
    }

    EffectContext context;
    context.owner = &player;
    context.discoveryEvents = discoveryEvents;
    context.encyclopedia = encyclopedia;
    context.position = player.position;
    context.triggerType = EffectTriggerType::NormalUse;
    context.logUnimplementedEffects = true;
    effectDispatcher.dispatchNormalEffects(selected->item, context);
    effectDispatcher.dispatch(selected->instance.addedEffects, context);
    status_ = "使用: " + selected->item.name;
    return true;
}

bool InventorySystem::useShortcutSelection(
    Player& player,
    SpellRingSystem& spellRing,
    const EffectDispatcher& effectDispatcher,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    (void)spellRing;
    if (selectedObjectStack() != nullptr) {
        return useObjectSelection(player, effectDispatcher, discoveryEvents, encyclopedia);
    }
    if (selectedObjectInstance() != nullptr) {
        return useObjectInstanceSelection(player, effectDispatcher, discoveryEvents, encyclopedia);
    }

    status_ = "ショートカット空き";
    return false;
}

bool InventorySystem::addShortcutSelectionToRing(SpellRingSystem& spellRing)
{
    if (selectedObjectStack() != nullptr) {
        return addObjectSelectionToRing(spellRing);
    }
    if (selectedObjectInstance() != nullptr) {
        return addObjectInstanceSelectionToRing(spellRing);
    }

    status_ = "ショートカット空き";
    return false;
}

void InventorySystem::toggleSelectedProtection()
{
    InventoryObjectInstance* selected = selectedObjectInstance();
    if (selected == nullptr) {
        status_ = "個体アイテムのみ保護できます";
        return;
    }
    selected->instance.protectionEnabled = !selected->instance.protectionEnabled;
    status_ = selected->instance.protectionEnabled ? "保護ON" : "保護OFF";
}

void InventorySystem::updateShortcuts(
    const Input& input,
    Player& player,
    SpellRingSystem& spellRing,
    const EffectDispatcher& effectDispatcher,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    if (input.shortcutCursorDelta() != 0) {
        moveShortcutCursor(input.shortcutCursorDelta());
    }

    selectShortcutSlot(input.shortcutSlotPressed());

    if (input.toggleShortcutRowPressed()) {
        toggleShortcutRow();
    }

    if (input.useItemPressed()) {
        useShortcutSelection(player, spellRing, effectDispatcher, discoveryEvents, encyclopedia);
    }
    if (input.addRingPressed()) {
        addShortcutSelectionToRing(spellRing);
    }
    if (input.pressed(InputAction::ToggleProtection)) {
        toggleSelectedProtection();
    }
}

void InventorySystem::updateScreen(
    const Input& input,
    UiContext& ui,
    Player& player,
    SpellRingSystem& spellRing,
    const EffectDispatcher& effectDispatcher,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    if (!open_) {
        return;
    }

    const bool commandOpenBeforeUpdate = slotCommandMenu_.open;
    const int commandSlotIndex = slotCommandMenuIndex_ >= 0 ? slotCommandMenuIndex_ : selectedShortcutIndex();
    const std::array<UiCommandMenuItem, 3> commandItems = buildSlotCommandItems(commandSlotIndex);
    const int commandSelection = updateUiCommandMenu(
        slotCommandMenu_,
        ui,
        input,
        commandItems.data(),
        static_cast<int>(commandItems.size()));
    if (commandSelection >= 0 && slotCommandMenuIndex_ >= 0) {
        selectShortcutIndex(slotCommandMenuIndex_);
        if (commandSelection == 0) {
            if (grabbedSlotActive_) {
                status_ = "つかみ中は使用できません";
            } else {
                useShortcutSelection(player, spellRing, effectDispatcher, discoveryEvents, encyclopedia);
            }
        } else if (commandSelection == 1) {
            if (grabbedSlotActive_) {
                status_ = "つかみ中はリング移動できません";
            } else {
                addShortcutSelectionToRing(spellRing);
            }
        } else if (commandSelection == 2) {
            if (grabbedSlotActive_) {
                status_ = "つかみ中は保護変更できません";
            } else {
                toggleSelectedProtection();
            }
        }
        slotCommandMenuIndex_ = -1;
        resetSlotPointerPress();
        ui.block(inventoryScreenRect());
        return;
    } else if (!slotCommandMenu_.open) {
        if (commandOpenBeforeUpdate && input.backPressed()) {
            slotCommandMenuIndex_ = -1;
            resetSlotPointerPress();
            ui.block(inventoryScreenRect());
            return;
        }
        slotCommandMenuIndex_ = -1;
    }

    if (input.pausePressed()) {
        if (slotCommandMenu_.open) {
            closeUiCommandMenu(slotCommandMenu_);
            slotCommandMenuIndex_ = -1;
            resetSlotPointerPress();
            return;
        }
        if (grabbedSlotActive_) {
            cancelGrab();
        } else {
            closeUiCommandMenu(slotCommandMenu_);
            slotCommandMenuIndex_ = -1;
            resetSlotPointerPress();
            open_ = false;
        }
        return;
    }

    if (slotCommandMenu_.open) {
        ui.block(inventoryScreenRect());
        return;
    }

    if (input.pressed(InputAction::MoveLeft)) {
        moveShortcutCursorGrid(-1, 0);
    }
    if (input.pressed(InputAction::MoveRight)) {
        moveShortcutCursorGrid(1, 0);
    }
    if (input.pressed(InputAction::MoveUp)) {
        moveShortcutCursorGrid(0, -1);
    }
    if (input.pressed(InputAction::MoveDown)) {
        moveShortcutCursorGrid(0, 1);
    }
    if (input.shortcutCursorDelta() != 0) {
        moveShortcutCursor(input.shortcutCursorDelta());
    }
    selectShortcutSlot(input.shortcutSlotPressed());

    int hoveredSlotIndex = -1;
    for (int i = 0; i < ShortcutSlotCount; ++i) {
        const UiRect rect = inventorySlotRect(i);
        if (rect.contains(ui.mouse())) {
            hoveredSlotIndex = i;
            selectShortcutIndex(i);
            break;
        }
    }

    if (input.mouseLeftPressed() && hoveredSlotIndex >= 0 && !ui.pointerConsumed()) {
        slotPointerPressIndex_ = hoveredSlotIndex;
        slotPointerPressMouse_ = input.mouseScreen();
        slotPointerPressCanOpenMenu_ = hasScreenItem(hoveredSlotIndex);
        slotPointerDragTriggered_ = false;
        ui.consumePointer();
    }

    if (slotPointerPressIndex_ >= 0 &&
        input.mouseLeftHeld() &&
        !slotPointerDragTriggered_ &&
        !grabbedSlotActive_ &&
        hasScreenItem(slotPointerPressIndex_) &&
        lengthSquared(input.mouseScreen() - slotPointerPressMouse_) >= SlotDragStartDistanceSq) {
        selectShortcutIndex(slotPointerPressIndex_);
        grabOrPlaceSelected();
        slotPointerDragTriggered_ = grabbedSlotActive_;
        slotPointerPressCanOpenMenu_ = false;
        closeUiCommandMenu(slotCommandMenu_);
        slotCommandMenuIndex_ = -1;
    }

    if (slotPointerPressIndex_ >= 0 && input.mouseLeftReleased()) {
        if (slotPointerDragTriggered_ && grabbedSlotActive_) {
            if (hoveredSlotIndex >= 0) {
                selectShortcutIndex(hoveredSlotIndex);
            }
            placeGrabbedAtSelected();
        } else if (slotPointerPressCanOpenMenu_ && hoveredSlotIndex == slotPointerPressIndex_) {
            openSlotCommandMenu(slotPointerPressIndex_);
        }
        resetSlotPointerPress();
    }

    if (input.grabOrPlacePressed()) {
        closeUiCommandMenu(slotCommandMenu_);
        slotCommandMenuIndex_ = -1;
        grabOrPlaceSelected();
    }
    if (input.useItemPressed() || input.confirmPressed()) {
        if (grabbedSlotActive_) {
            placeGrabbedAtSelected();
        } else if (hasScreenItem(selectedShortcutIndex())) {
            openSlotCommandMenu(selectedShortcutIndex());
        }
    }
    if (input.addRingPressed()) {
        if (grabbedSlotActive_) {
            status_ = "つかみ中は配置してください";
        } else {
            addShortcutSelectionToRing(spellRing);
        }
    }
    if (input.pressed(InputAction::ToggleProtection)) {
        if (grabbedSlotActive_) {
            status_ = "つかみ中は保護変更できません";
        } else {
            toggleSelectedProtection();
        }
    }

    ui.block(inventoryScreenRect());
}

void InventorySystem::update(
    const Input& input,
    UiContext& ui,
    Player& player,
    SpellRingSystem& spellRing,
    const EffectDispatcher& effectDispatcher,
    bool blocked,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    if (input.inventoryPressed() && !blocked) {
        if (open_) {
            closeUiCommandMenu(slotCommandMenu_);
            slotCommandMenuIndex_ = -1;
            resetSlotPointerPress();
            if (grabbedSlotActive_) {
                cancelGrab();
            }
        }
        open_ = !open_;
    }
    if (!open_ || blocked) {
        return;
    }
    updateScreen(input, ui, player, spellRing, effectDispatcher, discoveryEvents, encyclopedia);
}

void InventorySystem::render(
    Renderer& renderer,
    const Player& player,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& catalog,
    const EncyclopediaSystem& encyclopedia) const
{
    (void)player;
    (void)spellRing;

    renderer.setScreenSpace();

    if (!open_) {
        return;
    }

    UiWindowScope inventoryWindow(
        renderer,
        "inventory.main",
        {{ScreenX, ScreenY}, {ScreenW, ScreenH}},
        "アイテム",
        "F/Enter 決定  R リングへ  P 保護  G つかむ/置く  Esc 戻る");

    char buffer[160];
    for (int i = 0; i < ShortcutSlotCount; ++i) {
        const UiRect rect = inventorySlotRect(i);
        const bool selected = i == selectedShortcutIndex();
        const Color fill = selected ? Color{54, 44, 72, 242} : Color{20, 20, 28, 226};
        const Vec2 slotCenter = uiRectCenter(rect);
        renderer.fillCircle(slotCenter, inventorySlotFrameRadius(rect), fill);

        const InventoryObjectStack* objectStack = objectStackAtScreenIndex(i);
        if (objectStack != nullptr) {
            const Color objectColor = inventoryObjectColor(objectStack->item);
            const Vec2 iconCenter = slotCenter;
            const ObjectImageDrawOptions imageOptions = selected
                ? withSelectedItemOutline()
                : ObjectImageDrawOptions{};
            const bool drewImage = drawObjectImage(
                renderer,
                objectStack->item,
                iconCenter,
                {InventoryObjectImageMaxSize, InventoryObjectImageMaxSize},
                imageOptions);
            if (!drewImage) {
                renderer.fillCircle(iconCenter, 22.0f, objectColor);
                if (selected) {
                    drawSelectedItemCircleOutline(renderer, iconCenter, 22.0f);
                }
            }
        } else if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(i)) {
            const Color objectColor = objectInstance->instance.isBroken ? Color{82, 82, 90, 255} : inventoryObjectColor(objectInstance->item);
            const Vec2 iconCenter = slotCenter;
            ObjectImageDrawOptions imageOptions;
            imageOptions.tint = objectInstance->instance.isBroken ? Color{140, 140, 148, 220} : Color{255, 255, 255, 255};
            if (selected) {
                imageOptions = withSelectedItemOutline(imageOptions);
            }
            const bool drewImage = drawObjectImage(
                renderer,
                objectInstance->item,
                iconCenter,
                {InventoryObjectImageMaxSize, InventoryObjectImageMaxSize},
                imageOptions);
            if (!drewImage) {
                renderer.fillCircle(iconCenter, 22.0f, objectColor);
                if (selected) {
                    drawSelectedItemCircleOutline(renderer, iconCenter, 22.0f);
                }
            }
        }
    }
    const int detailIndex = (slotCommandMenu_.open && slotCommandMenuIndex_ >= 0)
        ? slotCommandMenuIndex_
        : selectedShortcutIndex();
    const InventoryObjectStack* detailStack = objectStackAtScreenIndex(detailIndex);
    const InventoryObjectInstance* detailInstance = objectInstanceAtScreenIndex(detailIndex);

    std::string detailTitle = "Empty";
    if (detailStack != nullptr) {
        const InventoryObjectStack* objectStack = detailStack;
        std::snprintf(buffer, sizeof(buffer), "%s x%d", objectStack->item.name.c_str(), objectStack->count);
        detailTitle = buffer;
    } else if (detailInstance != nullptr) {
        const InventoryObjectInstance* objectInstance = detailInstance;
        detailTitle = objectInstance->item.name;
    }

    const UiRect detailPanel{{DetailX, DetailY}, {DetailW, DetailH}};
    drawUiSubPanel(renderer, detailPanel);
    float detailLineY = drawUiDetailHeader(renderer, detailPanel, detailTitle);

    if (detailStack != nullptr) {
        const InventoryObjectStack* objectStack = detailStack;
        const ItemData& item = objectStack->item;
        const std::vector<std::string> effectLines =
            encyclopedia.getObjectEffectDisplayLines(item.id, catalog, EffectRevealMode::WithUnknown);
        const std::string effectText = joinEffectLines(effectLines);
        drawUiDetailText(renderer, detailPanel, detailLineY, item.description.empty() ? "-" : item.description);
        std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(effectLines.size()));
        drawUiDetailLine(renderer, detailPanel, detailLineY, "効果数", buffer);
        drawUiDetailText(renderer, detailPanel, detailLineY, "効果");
        drawUiDetailText(renderer, detailPanel, detailLineY, effectText);
        std::snprintf(buffer, sizeof(buffer), "%d", item.attackPower);
        drawUiDetailLine(renderer, detailPanel, detailLineY, "攻撃力", buffer);
        const std::string damageTypeText = item.damageType.empty() ? "-" : std::string(damageTypeDisplayName(item.damageType));
        drawUiDetailLine(renderer, detailPanel, detailLineY, "ダメージ", damageTypeText);
        std::snprintf(buffer, sizeof(buffer), "%d", item.digPower);
        drawUiDetailLine(renderer, detailPanel, detailLineY, "掘削力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d", item.durability);
        drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", item.weightKg);
        drawUiDetailLine(renderer, detailPanel, detailLineY, "重さ", buffer);
    } else if (detailInstance != nullptr) {
        const InventoryObjectInstance* objectInstance = detailInstance;
        const ItemData& item = objectInstance->item;
        const ItemInstance& instance = objectInstance->instance;
        const std::vector<std::string> effectLines =
            encyclopedia.getObjectEffectDisplayLines(item.id, catalog, EffectRevealMode::WithUnknown);
        const std::string effectText = joinEffectLines(effectLines);
        drawUiDetailText(renderer, detailPanel, detailLineY, item.description.empty() ? "-" : item.description);
        std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(effectLines.size()));
        drawUiDetailLine(renderer, detailPanel, detailLineY, "効果数", buffer);
        drawUiDetailText(renderer, detailPanel, detailLineY, "効果");
        drawUiDetailText(renderer, detailPanel, detailLineY, effectText);
        std::snprintf(buffer, sizeof(buffer), "%s", instance.instanceId.c_str());
        drawUiDetailLine(renderer, detailPanel, detailLineY, "個体ID", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d", instance.enhanceLevel);
        drawUiDetailLine(renderer, detailPanel, detailLineY, "強化Lv", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d/%d", instance.currentDurability, instance.maxDurability);
        drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
        drawUiDetailLine(renderer, detailPanel, detailLineY, "保護", instance.protectionEnabled ? "ON" : "OFF");
        drawUiDetailLine(renderer, detailPanel, detailLineY, "状態", instance.isBroken ? "破損" : "通常");
        std::snprintf(buffer, sizeof(buffer), "+%d / +%d / +%d", instance.attackBonus, instance.digBonus, instance.durabilityBonus);
        drawUiDetailLine(renderer, detailPanel, detailLineY, "補正", buffer);
        drawUiDetailLine(renderer, detailPanel, detailLineY, "操作", "P 保護ON/OFF");
    } else {
        drawUiDetailText(renderer, detailPanel, detailLineY, "アイテム未配置");
        drawUiDetailText(renderer, detailPanel, detailLineY, "-");
        drawUiDetailLine(renderer, detailPanel, detailLineY, "効果数", "0");
        drawUiDetailText(renderer, detailPanel, detailLineY, "効果");
        drawUiDetailText(renderer, detailPanel, detailLineY, "-");
        drawUiDetailLine(renderer, detailPanel, detailLineY, "攻撃力", "-");
        drawUiDetailLine(renderer, detailPanel, detailLineY, "ダメージ", "-");
        drawUiDetailLine(renderer, detailPanel, detailLineY, "掘削力", "-");
        drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", "-");
        drawUiDetailLine(renderer, detailPanel, detailLineY, "重さ", "-");
    }

    const int commandSlotIndex = slotCommandMenuIndex_ >= 0 ? slotCommandMenuIndex_ : selectedShortcutIndex();
    const std::array<UiCommandMenuItem, 3> commandItems = buildSlotCommandItems(commandSlotIndex);
    drawUiCommandMenu(renderer, slotCommandMenu_, commandItems.data(), static_cast<int>(commandItems.size()));

}

void InventorySystem::renderShortcutHud(Renderer& renderer, const SpellRingSystem& spellRing, int screenWidth, int screenHeight) const
{
    renderer.setScreenSpace();

    const float panelW = std::min(1040.0f, std::max(760.0f, static_cast<float>(screenWidth) - HudMargin * 2.0f));
    const float panelX = (static_cast<float>(screenWidth) - panelW) * 0.5f;
    const float panelY = static_cast<float>(screenHeight) - HudHeight - 12.0f;
    const UiRect hudPanel{{panelX, panelY}, {panelW, HudHeight}};
    const Vec2 hudContent = uiSubPanelContentPos(hudPanel);
    const float innerX = hudContent.x;
    const float slotY = hudContent.y;
    const float slotW = (panelW - ui::SubPanelPadding.x * 2.0f - HudSlotGap * static_cast<float>(ShortcutColumns - 1)) / static_cast<float>(ShortcutColumns);

    drawUiSubPanel(renderer, hudPanel);

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "Row %d/3   Ring %d", shortcutRow_ + 1, spellRing.activeRingIndex() + 1);
    renderer.drawText({innerX, panelY + 74.0f}, buffer, ui::Text, 2);
    renderer.drawText({innerX + 300.0f, panelY + 74.0f}, "Tab 行切替, F 決定, R リングへ, P 保護", ui::TextMuted, 2);

    for (int column = 0; column < ShortcutColumns; ++column) {
        const int slotIndex = shortcutRow_ * ShortcutColumns + column;
        const bool selected = column == selectedShortcutColumn_;
        const Vec2 slotPos{innerX + static_cast<float>(column) * (slotW + HudSlotGap), slotY};
        const Color fill = selected ? Color{54, 44, 72, 242} : Color{20, 20, 28, 226};
        const Color outline = selected ? ui::WindowBorder : Color{78, 72, 94, 255};
        renderer.fillRect(slotPos, {slotW, HudSlotHeight}, fill);
        renderer.drawRect(slotPos, {slotW, HudSlotHeight}, outline);

        std::snprintf(buffer, sizeof(buffer), "%d", column + 1);
        renderer.drawText(slotPos + Vec2{7.0f, 6.0f}, buffer, selected ? ui::Text : ui::TextDisabled, 1);

        const InventoryObjectStack* objectStack = objectStackAtScreenIndex(slotIndex);
        if (objectStack != nullptr) {
            const Vec2 iconCenter = slotPos + Vec2{24.0f, 24.0f};
            const bool drewImage = drawObjectImage(
                renderer,
                objectStack->item,
                iconCenter,
                {ShortcutObjectImageMaxSize, ShortcutObjectImageMaxSize});
            if (!drewImage) {
                renderer.fillCircle(iconCenter, 20.0f, inventoryObjectColor(objectStack->item));
            }
            std::snprintf(buffer, sizeof(buffer), "%s x%d", objectStack->item.name.c_str(), objectStack->count);
            renderer.drawText(slotPos + Vec2{56.0f, 18.0f}, buffer, ui::Text, 2);
        } else if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(slotIndex)) {
            const Vec2 iconCenter = slotPos + Vec2{24.0f, 24.0f};
            ObjectImageDrawOptions imageOptions;
            imageOptions.tint = objectInstance->instance.isBroken ? Color{140, 140, 148, 220} : Color{255, 255, 255, 255};
            const bool drewImage = drawObjectImage(
                renderer,
                objectInstance->item,
                iconCenter,
                {ShortcutObjectImageMaxSize, ShortcutObjectImageMaxSize},
                imageOptions);
            if (!drewImage) {
                const Color objectColor = objectInstance->instance.isBroken ? Color{82, 82, 90, 255} : inventoryObjectColor(objectInstance->item);
                renderer.fillCircle(iconCenter, 20.0f, objectColor);
            }
            std::snprintf(buffer, sizeof(buffer), "%s%s",
                objectInstance->instance.protectionEnabled ? "[P] " : "",
                objectInstance->item.name.c_str());
            renderer.drawText(
                slotPos + Vec2{56.0f, 18.0f},
                buffer,
                objectInstance->instance.isBroken ? ui::TextDisabled : ui::Text,
                2);
        } else {
            renderer.drawText(slotPos + Vec2{20.0f, 18.0f}, "Empty", ui::TextDisabled, 2);
        }

        if (selected) {
            renderer.fillRect({slotPos.x + 8.0f, slotPos.y + HudSlotHeight - 5.0f}, {slotW - 16.0f, 3.0f}, ui::Text);
            renderer.drawText({slotPos.x + slotW * 0.5f - 4.0f, slotPos.y + HudSlotHeight + 5.0f}, "v", ui::Text, 2);
        }
    }

}

}
