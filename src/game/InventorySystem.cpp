#include "game/InventorySystem.hpp"

#include "engine/Log.hpp"
#include "game/EffectDispatcher.hpp"
#include "game/EncyclopediaSystem.hpp"
#include "game/InventoryUiCommon.hpp"
#include "game/ObjectImageRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace majo {

namespace {
constexpr int InventoryColumns = 8;
constexpr int InventoryRows = 3;
constexpr int ShortcutHudColumns = 8;
constexpr float PanelX = 790.0f;
constexpr float PanelY = 70.0f;
constexpr float PanelW = 430.0f;
constexpr float PanelH = 440.0f;
constexpr float RowX = PanelX + 24.0f;
constexpr float RowW = PanelW - 48.0f;
constexpr float RowH = 58.0f;
constexpr float HudMargin = 16.0f;
constexpr float HudHeight = 118.0f;
constexpr float HudSlotGap = 12.0f;
constexpr float HudSlotSize = 64.0f;
constexpr float HudSlotY = 42.0f;
constexpr float HudSelectedNameY = 19.0f;
constexpr float ScreenX = 44.0f;
constexpr float ScreenY = 58.0f;
constexpr float ScreenW = 1192.0f;
constexpr float ScreenH = 610.0f;
constexpr float ScreenGridY = ScreenY + 84.0f;
constexpr float ScreenSlotW = 88.0f;
constexpr float ScreenSlotH = 76.0f;
constexpr float ScreenSlotGap = 8.0f;
constexpr float InventoryObjectImageMaxSize = 48.0f;
constexpr float ShortcutObjectImageMaxSize = 48.0f;
constexpr float SlotDragStartDistanceSq = 36.0f;
constexpr float GrabbedSlotContentAlpha = 0.42f;
constexpr float GrabbedFloatingIconLift = 38.0f;
constexpr float GrabbedFloatingIconBobAmplitude = 4.0f;
constexpr float GrabbedFloatingIconBobSpeed = 5.4f;
constexpr float DetailX = ScreenX + 820.0f;
constexpr float DetailY = ScreenY + 50.0f;
constexpr float DetailW = 330.0f;
constexpr float DetailH = 520.0f;
constexpr float ScreenGridW = static_cast<float>(InventoryColumns) * ScreenSlotW + static_cast<float>(InventoryColumns - 1) * ScreenSlotGap;
constexpr float ScreenGridX = ScreenX + (DetailX - ScreenX - ScreenGridW) * 0.5f;
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

int clampedUnlockedRingCount(int unlockedRingCount)
{
    return std::clamp(unlockedRingCount, 1, SpellRingCount);
}

std::string_view protectionCommandLabel(const InventoryObjectInstance& instance)
{
    return instance.instance.protectionEnabled ? "保護を解除" : "保護";
}

UiRect inventorySortButtonRect()
{
    return uiBottomLeftButtonRect(inventoryScreenRect(), {150.0f, ui::ButtonHeight});
}

UiRect inventoryDiscardConfirmRect()
{
    return {{390.0f, 220.0f}, {500.0f, 280.0f}};
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

UiRect makeShortcutHudPanelRect(int screenWidth, int screenHeight)
{
    const float panelW = std::min(1040.0f, std::max(760.0f, static_cast<float>(screenWidth) - HudMargin * 2.0f));
    const float panelX = (static_cast<float>(screenWidth) - panelW) * 0.5f;
    const float panelY = static_cast<float>(screenHeight) - HudHeight - 12.0f;
    return {{panelX, panelY}, {panelW, HudHeight}};
}

UiRect shortcutHudSlotRect(int column, int screenWidth, int screenHeight)
{
    const UiRect panel = makeShortcutHudPanelRect(screenWidth, screenHeight);
    const float totalW = HudSlotSize * static_cast<float>(ShortcutHudColumns) +
        HudSlotGap * static_cast<float>(ShortcutHudColumns - 1);
    const float startX = panel.pos.x + (panel.size.x - totalW) * 0.5f;
    return {{
        startX + static_cast<float>(column) * (HudSlotSize + HudSlotGap),
        panel.pos.y + HudSlotY,
    }, {HudSlotSize, HudSlotSize}};
}

Vec2 shortcutHudSlotCenter(int column, int screenWidth, int screenHeight)
{
    const UiRect rect = shortcutHudSlotRect(column, screenWidth, screenHeight);
    return rect.pos + rect.size * 0.5f;
}

Vec2 uiRectCenter(const UiRect& rect)
{
    return rect.pos + rect.size * 0.5f;
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

void clearInventoryAddResult(InventoryAddResult* outResult)
{
    if (outResult != nullptr) {
        *outResult = {};
    }
}

void setInventoryAddResult(
    InventoryAddResult* outResult,
    InventoryAddKind kind,
    std::string_view objectId,
    std::string_view instanceId = {},
    int quantity = 1)
{
    if (outResult == nullptr) {
        return;
    }
    outResult->added = true;
    outResult->kind = kind;
    outResult->objectId = std::string(objectId);
    outResult->instanceId = std::string(instanceId);
    outResult->quantity = std::max(0, quantity);
}

std::unordered_map<std::string, int> buildObjectSortOrder(const ObjectCatalog& catalog)
{
    std::unordered_map<std::string, int> order;
    order.reserve(catalog.objects.size());
    for (int i = 0; i < static_cast<int>(catalog.objects.size()); ++i) {
        const ObjectDefinition& object = catalog.objects[static_cast<std::size_t>(i)];
        if (!object.id.empty() && order.find(object.id) == order.end()) {
            order.emplace(object.id, i);
        }
    }
    return order;
}

int objectSortOrder(const std::unordered_map<std::string, int>& order, const std::string& objectId)
{
    constexpr int MissingOrder = 1'000'000'000;
    const auto it = order.find(objectId);
    return it != order.end() ? it->second : MissingOrder;
}

const std::string& objectSortId(const InventoryObjectInstance& instance)
{
    return !instance.item.id.empty() ? instance.item.id : instance.instance.objectId;
}

const std::string& packedObjectSortId(
    int packedIndex,
    const std::vector<InventoryObjectStack>& stacks,
    const std::vector<InventoryObjectInstance>& instances)
{
    const int stackCount = static_cast<int>(stacks.size());
    if (packedIndex >= 0 && packedIndex < stackCount) {
        return stacks[static_cast<std::size_t>(packedIndex)].objectId;
    }
    const int instanceIndex = packedIndex - stackCount;
    if (instanceIndex >= 0 && instanceIndex < static_cast<int>(instances.size())) {
        return objectSortId(instances[static_cast<std::size_t>(instanceIndex)]);
    }
    static const std::string Empty;
    return Empty;
}

bool ringContainsInstanceId(const SpellRingSystem& spellRing, std::string_view instanceId)
{
    if (instanceId.empty()) {
        return false;
    }
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const std::vector<SpellRingItem>& ringItems = spellRing.itemsForRing(ringIndex);
        if (std::any_of(ringItems.begin(), ringItems.end(), [instanceId](const SpellRingItem& item) {
            return item.instanceId == instanceId;
        })) {
            return true;
        }
    }
    return false;
}

}

void InventorySystem::clearObjectStacks()
{
    objectStacks_.clear();
    objectInstances_.clear();
    equippedStaffInstanceId_.clear();
}

bool InventorySystem::isStaffEquipped(std::string_view instanceId) const
{
    return !instanceId.empty() && equippedStaffInstanceId_ == instanceId;
}

void InventorySystem::clearEquippedStaff()
{
    equippedStaffInstanceId_.clear();
}

bool InventorySystem::restoreEquippedStaffInstanceId(std::string_view instanceId, std::string* outWarning)
{
    equippedStaffInstanceId_.clear();
    if (outWarning != nullptr) {
        outWarning->clear();
    }
    if (instanceId.empty() || instanceId == "-") {
        return true;
    }

    const InventoryObjectInstance* entry = objectInstanceById(instanceId);
    if (entry == nullptr) {
        if (outWarning != nullptr) {
            *outWarning = "equipped_staff instance_id=\"" + std::string(instanceId) + "\" is missing; staff unequipped";
        }
        return false;
    }
    if (!isStaffObject(entry->item)) {
        if (outWarning != nullptr) {
            *outWarning = "equipped_staff instance_id=\"" + std::string(instanceId) + "\" is not a staff; staff unequipped";
        }
        return false;
    }
    if (entry->instance.isBroken) {
        if (outWarning != nullptr) {
            *outWarning = "equipped_staff instance_id=\"" + std::string(instanceId) + "\" is broken; staff unequipped";
        }
        return false;
    }

    equippedStaffInstanceId_ = std::string(instanceId);
    return true;
}

bool InventorySystem::sortByCatalogOrder(const ObjectCatalog& catalog)
{
    closeUiCommandMenu(slotCommandMenu_);
    slotCommandMenuIndex_ = -1;
    closeRingTargetCommandMenu();
    resetSlotPointerPress();
    if (grabbedSlotActive_) {
        cancelGrab();
    }

    const int totalCount = static_cast<int>(objectStacks_.size() + objectInstances_.size());
    if (totalCount <= 0) {
        packedItemSlots_.clear();
        selected_ = 0;
        shortcutRow_ = 0;
        selectedShortcutColumn_ = 0;
        status_ = "アイテムなし";
        return false;
    }

    const auto order = buildObjectSortOrder(catalog);
    std::stable_sort(objectStacks_.begin(), objectStacks_.end(), [&order](const InventoryObjectStack& a, const InventoryObjectStack& b) {
        const int orderA = objectSortOrder(order, a.objectId);
        const int orderB = objectSortOrder(order, b.objectId);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        return a.objectId < b.objectId;
    });
    std::stable_sort(objectInstances_.begin(), objectInstances_.end(), [&order](const InventoryObjectInstance& a, const InventoryObjectInstance& b) {
        const std::string& idA = objectSortId(a);
        const std::string& idB = objectSortId(b);
        const int orderA = objectSortOrder(order, idA);
        const int orderB = objectSortOrder(order, idB);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        return idA < idB;
    });

    std::vector<int> packedIndices;
    packedIndices.reserve(static_cast<std::size_t>(totalCount));
    for (int i = 0; i < totalCount; ++i) {
        packedIndices.push_back(i);
    }
    std::stable_sort(packedIndices.begin(), packedIndices.end(), [this, &order](int a, int b) {
        const std::string& idA = packedObjectSortId(a, objectStacks_, objectInstances_);
        const std::string& idB = packedObjectSortId(b, objectStacks_, objectInstances_);
        const int orderA = objectSortOrder(order, idA);
        const int orderB = objectSortOrder(order, idB);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        if (idA != idB) {
            return idA < idB;
        }
        return a < b;
    });

    packedItemSlots_.assign(static_cast<std::size_t>(totalCount), -1);
    for (int slot = 0; slot < static_cast<int>(packedIndices.size()); ++slot) {
        const int packedIndex = packedIndices[static_cast<std::size_t>(slot)];
        packedItemSlots_[static_cast<std::size_t>(packedIndex)] = slot < ShortcutSlotCount ? slot : slot % ShortcutSlotCount;
    }

    selectShortcutIndex(0);
    status_ = "並び替えました";
    return true;
}

std::vector<RingEquipFxRequest> InventorySystem::consumeRingEquipFxRequests()
{
    std::vector<RingEquipFxRequest> requests = std::move(ringEquipFxRequests_);
    ringEquipFxRequests_.clear();
    return requests;
}

std::vector<InventoryDiscardRequest> InventorySystem::consumeDiscardRequests()
{
    std::vector<InventoryDiscardRequest> requests = std::move(discardRequests_);
    discardRequests_.clear();
    return requests;
}

std::vector<InventoryUseEvent> InventorySystem::consumeUseEvents()
{
    std::vector<InventoryUseEvent> events = std::move(useEvents_);
    useEvents_.clear();
    return events;
}

std::vector<StatusPopupEvent> InventorySystem::consumeStatusPopupEvents()
{
    std::vector<StatusPopupEvent> consumed;
    consumed.swap(statusPopupEvents_);
    return consumed;
}

void InventorySystem::setOpen(bool open)
{
    open_ = open;
    if (!open_) {
        closeUiCommandMenu(slotCommandMenu_);
        slotCommandMenuIndex_ = -1;
        closeRingTargetCommandMenu();
        discardConfirm_ = {};
        discardConfirmSlotIndex_ = -1;
        resetSlotPointerPress();
    }
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
    observeObjectInstanceId(objectInstances_.back().instance.instanceId);
    return true;
}

bool InventorySystem::addRuntimeObjectInstance(const ItemData& item, ItemInstance instance)
{
    if (item.id.empty() || instance.instanceId.empty()) {
        return false;
    }
    if (instance.objectId.empty()) {
        instance.objectId = item.id;
    }
    if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
        status_ = "インベントリ満杯";
        return false;
    }

    objectInstances_.push_back(InventoryObjectInstance{
        .item = item,
        .instance = std::move(instance),
    });
    observeObjectInstanceId(objectInstances_.back().instance.instanceId);
    status_ = "Picked up: " + item.name;
    return true;
}

ItemInstance InventorySystem::createDetachedObjectInstance(const ItemData& item)
{
    return makeItemInstanceFromDefinition(allocateInstanceId(), item);
}

void InventorySystem::observeObjectInstanceId(std::string_view instanceId)
{
    constexpr std::string_view Prefix = "iteminst_";
    if (instanceId.rfind(Prefix, 0) != 0) {
        return;
    }

    const std::string suffix(instanceId.substr(Prefix.size()));
    if (suffix.empty()) {
        return;
    }
    char* parseEnd = nullptr;
    const unsigned long long parsed = std::strtoull(suffix.c_str(), &parseEnd, 10);
    if (parseEnd == suffix.c_str() || parseEnd == nullptr || *parseEnd != '\0') {
        return;
    }
    nextInstanceId_ = std::max(nextInstanceId_, parsed + 1);
}

std::optional<bool> InventorySystem::objectInstanceProtectionEnabled(std::string_view instanceId) const
{
    if (instanceId.empty()) {
        return std::nullopt;
    }
    const auto it = std::find_if(objectInstances_.begin(), objectInstances_.end(), [instanceId](const InventoryObjectInstance& entry) {
        return entry.instance.instanceId == instanceId;
    });
    if (it == objectInstances_.end()) {
        return std::nullopt;
    }
    return it->instance.protectionEnabled;
}

bool InventorySystem::setObjectInstanceProtection(std::string_view instanceId, bool enabled)
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
    it->instance.protectionEnabled = enabled;
    status_ = enabled ? "保護ON" : "保護OFF";
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

bool InventorySystem::useObjectStackById(
    std::string_view objectId,
    Player& player,
    const EffectDispatcher& effectDispatcher,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia,
    std::string* outStatus)
{
    if (objectId.empty()) {
        status_ = "アイテム未指定";
        if (outStatus != nullptr) {
            *outStatus = status_;
        }
        return false;
    }

    for (int i = 0; i < static_cast<int>(objectStacks_.size()); ++i) {
        const InventoryObjectStack& stack = objectStacks_[static_cast<std::size_t>(i)];
        if (stack.objectId != objectId || stack.count <= 0) {
            continue;
        }

        const bool used = useObjectStackAtIndex(
            i,
            player,
            effectDispatcher,
            magic,
            discoveryEvents,
            encyclopedia);
        if (outStatus != nullptr) {
            *outStatus = status_;
        }
        return used;
    }

    status_ = "アイテムがありません";
    if (outStatus != nullptr) {
        *outStatus = status_;
    }
    return false;
}

bool InventorySystem::useObjectInstanceById(
    std::string_view instanceId,
    Player& player,
    const EffectDispatcher& effectDispatcher,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia,
    std::string* outStatus)
{
    if (instanceId.empty()) {
        status_ = "アイテム未指定";
        if (outStatus != nullptr) {
            *outStatus = status_;
        }
        return false;
    }

    for (int i = 0; i < static_cast<int>(objectInstances_.size()); ++i) {
        const InventoryObjectInstance& objectInstance = objectInstances_[static_cast<std::size_t>(i)];
        if (objectInstance.instance.instanceId != instanceId) {
            continue;
        }

        const bool used = useObjectInstanceAtIndex(
            i,
            player,
            effectDispatcher,
            magic,
            discoveryEvents,
            encyclopedia);
        if (outStatus != nullptr) {
            *outStatus = status_;
        }
        return used;
    }

    status_ = "アイテムがありません";
    if (outStatus != nullptr) {
        *outStatus = status_;
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
    if (isStaffEquipped(instanceId)) {
        status_ = "装備中の杖は外してください";
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
    if (isStaffEquipped(instanceId)) {
        status_ = "装備中の杖はしまえません";
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

bool InventorySystem::resetObjectInstanceEnhancement(std::string_view instanceId, const ObjectCatalog& catalog)
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
    if (instance.enhanceLevel <= 0 &&
        instance.attackBonus == 0 &&
        instance.digBonus == 0 &&
        instance.durabilityBonus == 0) {
        status_ = "リセット不要";
        return false;
    }

    const ItemData* item = catalog.registry.findById(instance.objectId);
    const int baseDurability = item != nullptr ? item->durability : std::max(-1, instance.maxDurability - instance.durabilityBonus);
    instance.enhanceLevel = 0;
    instance.attackBonus = 0;
    instance.digBonus = 0;
    instance.durabilityBonus = 0;
    instance.maxDurability = baseDurability;
    if (instance.maxDurability >= 0) {
        instance.currentDurability = std::clamp(instance.currentDurability, 0, instance.maxDurability);
        instance.isBroken = instance.currentDurability == 0;
    } else {
        instance.isBroken = false;
    }
    status_ = "強化をリセットしました";
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

bool InventorySystem::modifyObjectInstanceShape(std::string_view instanceId, double weightMultiplier, double sizeMultiplier)
{
    if (instanceId.empty() || weightMultiplier <= 0.0 || sizeMultiplier <= 0.0) {
        return false;
    }
    const auto it = std::find_if(objectInstances_.begin(), objectInstances_.end(), [instanceId](const InventoryObjectInstance& entry) {
        return entry.instance.instanceId == instanceId;
    });
    if (it == objectInstances_.end()) {
        return false;
    }

    ItemInstance& instance = it->instance;
    instance.weightModifier = std::clamp(instance.weightModifier * weightMultiplier, 0.25, 4.0);
    instance.sizeModifier = std::clamp(instance.sizeModifier * sizeMultiplier, 0.50, 3.0);
    status_ = "加工しました";
    return true;
}

bool InventorySystem::modifyObjectStackItemShape(std::string_view objectId, double weightMultiplier, double sizeMultiplier)
{
    if (objectId.empty() || weightMultiplier <= 0.0 || sizeMultiplier <= 0.0) {
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
    ItemInstance& instance = objectInstances_.back().instance;
    instance.weightModifier = std::clamp(instance.weightModifier * weightMultiplier, 0.25, 4.0);
    instance.sizeModifier = std::clamp(instance.sizeModifier * sizeMultiplier, 0.50, 3.0);

    --it->count;
    if (it->count <= 0) {
        const int stackIndex = static_cast<int>(std::distance(objectStacks_.begin(), it));
        removePackedSlotAtPackedIndex(stackIndex);
        objectStacks_.erase(it);
    }
    status_ = "個体化して加工しました";
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

const InventoryObjectInstance* InventorySystem::objectInstanceById(std::string_view instanceId) const
{
    if (instanceId.empty()) {
        return nullptr;
    }
    const auto it = std::find_if(objectInstances_.begin(), objectInstances_.end(), [instanceId](const InventoryObjectInstance& entry) {
        return entry.instance.instanceId == instanceId;
    });
    return it == objectInstances_.end() ? nullptr : &*it;
}

InventoryObjectInstance* InventorySystem::objectInstanceById(std::string_view instanceId)
{
    if (instanceId.empty()) {
        return nullptr;
    }
    const auto it = std::find_if(objectInstances_.begin(), objectInstances_.end(), [instanceId](const InventoryObjectInstance& entry) {
        return entry.instance.instanceId == instanceId;
    });
    return it == objectInstances_.end() ? nullptr : &*it;
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

bool InventorySystem::canAddObjectItem(const ObjectCatalog& catalog, std::string_view objectId) const
{
    if (objectId.empty() || catalog.registry.empty()) {
        return false;
    }

    const ItemData* item = catalog.registry.findById(objectId);
    if (item == nullptr) {
        return false;
    }

    MaterialType materialType = MaterialType::Count;
    if (materialTypeForObject(*item, materialType)) {
        return true;
    }

    if (isStackableObject(*item)) {
        const auto stackIt = std::find_if(objectStacks_.begin(), objectStacks_.end(), [item](const InventoryObjectStack& stack) {
            return stack.objectId == item->id;
        });
        if (stackIt != objectStacks_.end()) {
            return true;
        }
    }

    return static_cast<int>(objectStacks_.size() + objectInstances_.size()) < ShortcutSlotCount;
}

bool InventorySystem::addObjectItem(const ObjectCatalog& catalog, std::string_view objectId, InventoryAddResult* outResult)
{
    clearInventoryAddResult(outResult);
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
        setInventoryAddResult(outResult, InventoryAddKind::Material, item->id);
        return true;
    }

    if (!isStackableObject(*item)) {
        if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
            status_ = "インベントリ満杯";
            return false;
        }
        InventoryObjectInstance objectInstance = createObjectInstance(*item);
        status_ = "Picked up: " + item->name;
        setInventoryAddResult(outResult, InventoryAddKind::Instance, item->id, objectInstance.instance.instanceId);
        return true;
    }

    for (InventoryObjectStack& stack : objectStacks_) {
        if (stack.objectId == item->id) {
            ++stack.count;
            stack.item = *item;
            status_ = "Picked up: " + item->name;
            setInventoryAddResult(outResult, InventoryAddKind::Stack, item->id);
            return true;
        }
    }

    if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
        status_ = "インベントリ満杯";
        return false;
    }

    objectStacks_.push_back(InventoryObjectStack{*item, 1});
    status_ = "Picked up: " + item->name;
    setInventoryAddResult(outResult, InventoryAddKind::Stack, item->id);
    return true;
}

bool InventorySystem::addRuntimeObjectItem(const ItemData& item, InventoryAddResult* outResult)
{
    clearInventoryAddResult(outResult);
    if (item.id.empty()) {
        return false;
    }

    MaterialType materialType = MaterialType::Count;
    if (materialTypeForObject(item, materialType)) {
        materials_.add(materialType, 1);
        status_ = "素材入手: " + std::string(materialTypeDisplayName(materialType));
        setInventoryAddResult(outResult, InventoryAddKind::Material, item.id);
        return true;
    }

    if (!isStackableObject(item)) {
        if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
            status_ = "インベントリ満杯";
            return false;
        }
        InventoryObjectInstance objectInstance = createObjectInstance(item);
        status_ = "Picked up: " + item.name;
        setInventoryAddResult(outResult, InventoryAddKind::Instance, item.id, objectInstance.instance.instanceId);
        return true;
    }

    for (InventoryObjectStack& stack : objectStacks_) {
        if (stack.objectId == item.id) {
            ++stack.count;
            stack.item = item;
            stack.objectId = item.id;
            status_ = "Picked up: " + item.name;
            setInventoryAddResult(outResult, InventoryAddKind::Stack, item.id);
            return true;
        }
    }

    if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
        status_ = "インベントリ満杯";
        return false;
    }

    objectStacks_.push_back(InventoryObjectStack{item, 1});
    status_ = "Picked up: " + item.name;
    setInventoryAddResult(outResult, InventoryAddKind::Stack, item.id);
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
        return !isStaffObject(objectStack->item) && !objectStack->item.normalEffects.empty();
    }
    if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(index)) {
        return !isStaffObject(objectInstance->item) &&
            !objectInstance->instance.isBroken &&
            !objectInstance->item.normalEffects.empty();
    }
    return false;
}

bool InventorySystem::canDiscardScreenItem(int index, bool itemDiscardEnabled) const
{
    (void)itemDiscardEnabled;
    if (const InventoryObjectStack* objectStack = objectStackAtScreenIndex(index)) {
        return objectStack->count > 0 && !isImportantItem(objectStack->item);
    }
    if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(index)) {
        return !isStaffEquipped(objectInstance->instance.instanceId) &&
            !objectInstance->instance.protectionEnabled &&
            !isImportantItem(objectInstance->item);
    }
    return false;
}

bool InventorySystem::discardScreenItem(int index, bool itemDiscardEnabled)
{
    if (InventoryObjectStack* objectStack = objectStackAtScreenIndex(index)) {
        if (objectStack->count <= 0) {
            status_ = "アイテム未選択";
            return false;
        }
        if (isImportantItem(objectStack->item)) {
            status_ = "重要アイテムは捨てられません";
            return false;
        }
        if (itemDiscardEnabled) {
            discardRequests_.push_back(InventoryDiscardRequest{
                .item = objectStack->item,
                .quantity = 1,
            });
        }
        status_ = "捨てた: " + objectStack->item.name;
        --objectStack->count;
        if (objectStack->count <= 0) {
            const int objectIndex = stackIndexAtScreenIndex(index);
            if (objectIndex >= 0) {
                removePackedSlotAtPackedIndex(objectIndex);
                objectStacks_.erase(objectStacks_.begin() + objectIndex);
            }
        }
        return true;
    }
    if (InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(index)) {
        if (isStaffEquipped(objectInstance->instance.instanceId)) {
            status_ = "装備中の杖は捨てられません";
            return false;
        }
        if (objectInstance->instance.protectionEnabled) {
            status_ = "保護中は捨てられません";
            return false;
        }
        if (isImportantItem(objectInstance->item)) {
            status_ = "重要アイテムは捨てられません";
            return false;
        }
        if (itemDiscardEnabled) {
            discardRequests_.push_back(InventoryDiscardRequest{
                .item = objectInstance->item,
                .instance = objectInstance->instance,
                .quantity = 1,
            });
        }
        status_ = "捨てた: " + objectInstance->item.name;
        const int instanceIndex = instanceIndexAtScreenIndex(index);
        if (instanceIndex >= 0) {
            removePackedSlotAtPackedIndex(static_cast<int>(objectStacks_.size()) + instanceIndex);
            objectInstances_.erase(objectInstances_.begin() + instanceIndex);
        }
        return true;
    }

    status_ = "アイテム未選択";
    return false;
}

void InventorySystem::openDiscardConfirmDialog(int slotIndex)
{
    if (!canDiscardScreenItem(slotIndex, false)) {
        status_ = "捨てられません";
        return;
    }

    discardConfirmSlotIndex_ = slotIndex;
    openUiConfirmDialog(
        discardConfirm_,
        "確認",
        "",
        "捨てる",
        "戻る",
        1);
    closeUiCommandMenu(slotCommandMenu_);
    slotCommandMenuIndex_ = -1;
    closeRingTargetCommandMenu();
    resetSlotPointerPress();
}

void InventorySystem::drawDiscardConfirmDialog(Renderer& renderer, const ObjectCatalog& catalog) const
{
    if (!discardConfirm_.open) {
        return;
    }

    const UiRect panel = inventoryDiscardConfirmRect();
    UiWindowScope window(
        renderer,
        "inventory.discard.confirm",
        panel,
        discardConfirm_.title,
        uiConfirmDialogHelpText(),
        UiWindowOptions{true, true});

    const InventoryObjectStack* stack = objectStackAtScreenIndex(discardConfirmSlotIndex_);
    const InventoryObjectInstance* instance = objectInstanceAtScreenIndex(discardConfirmSlotIndex_);
    const ItemData* item = stack != nullptr ? &stack->item : (instance != nullptr ? &instance->item : nullptr);
    const bool broken = stack != nullptr
        ? stack->item.durability == 0
        : (instance != nullptr && instance->instance.isBroken);
    const std::string itemName = item != nullptr
        ? itemDisplayName(item->name.empty() ? item->id : item->name, broken)
        : std::string("アイテム");
    const std::string iconPrefix = item != nullptr && catalog.registry.findById(item->id) != nullptr
        ? inlineItemTag(item->id)
        : "";

    constexpr float ContentInset = 48.0f;
    const float bodyTop = panel.pos.y + ui::HeaderHeight + 6.0f;
    const UiRect body{{
        panel.pos.x + ContentInset,
        bodyTop,
    }, {
        panel.size.x - ContentInset * 2.0f,
        std::max(0.0f, uiConfirmDialogButtonRect(panel, 0).pos.y - bodyTop - 16.0f),
    }};

    InlineItemTextStyle questionStyle;
    questionStyle.text = ui::Text;
    questionStyle.scale = 2;
    const std::string question = fittedInlineItemText(
        renderer,
        iconPrefix + itemName + "を捨てますか？",
        body.size.x,
        questionStyle);
    float y = body.pos.y;
    drawInlineItemText(renderer, catalog, {body.pos.x, y}, question, questionStyle);
    y += measureInlineItemText(renderer, question, questionStyle).y + 18.0f;
    renderer.drawWrappedText(
        {body.pos.x, y},
        "（捨てたアイテムは再入手できません）",
        body.size.x,
        {255, 224, 164, 255},
        2);

    drawUiConfirmDialogButtons(renderer, discardConfirm_, panel);
}

bool InventorySystem::canEquipStaffScreenItem(int index) const
{
    if (const InventoryObjectStack* objectStack = objectStackAtScreenIndex(index)) {
        const bool stackSlotWillRemain = objectStack->count > 1;
        return objectStack->count > 0 &&
            isStaffObject(objectStack->item) &&
            objectStack->item.durability != 0 &&
            (!stackSlotWillRemain || static_cast<int>(objectStacks_.size() + objectInstances_.size()) < ShortcutSlotCount);
    }
    if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(index)) {
        if (isStaffEquipped(objectInstance->instance.instanceId)) {
            return true;
        }
        return isStaffObject(objectInstance->item) && !objectInstance->instance.isBroken;
    }
    return false;
}

bool InventorySystem::unequipStaff()
{
    if (equippedStaffInstanceId_.empty()) {
        status_ = "杖を装備していません";
        return false;
    }
    equippedStaffInstanceId_.clear();
    status_ = "杖を外しました";
    return true;
}

bool InventorySystem::equipStaffScreenItem(int index, const SpellRingSystem& spellRing)
{
    if (InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(index)) {
        if (isStaffEquipped(objectInstance->instance.instanceId)) {
            return unequipStaff();
        }
        if (!isStaffObject(objectInstance->item)) {
            status_ = "杖ではありません";
            return false;
        }
        if (objectInstance->instance.isBroken) {
            status_ = "壊れた杖は装備できません";
            return false;
        }
        if (ringContainsInstanceId(spellRing, objectInstance->instance.instanceId)) {
            status_ = "リングから外してから装備してください";
            return false;
        }
        equippedStaffInstanceId_ = objectInstance->instance.instanceId;
        status_ = "杖を装備しました: " + objectInstance->item.name;
        return true;
    }

    InventoryObjectStack* objectStack = objectStackAtScreenIndex(index);
    if (objectStack == nullptr || objectStack->count <= 0) {
        status_ = "杖がありません";
        return false;
    }
    if (!isStaffObject(objectStack->item)) {
        status_ = "杖ではありません";
        return false;
    }
    if (objectStack->item.durability == 0) {
        status_ = "壊れた杖は装備できません";
        return false;
    }
    const bool stackSlotWillRemain = objectStack->count > 1;
    if (stackSlotWillRemain && static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount) {
        status_ = "インベントリ満杯";
        return false;
    }

    const ItemData item = objectStack->item;
    ItemInstance instance = createDetachedObjectInstance(item);
    const std::string instanceId = instance.instanceId;
    objectInstances_.push_back(InventoryObjectInstance{
        .item = item,
        .instance = std::move(instance),
    });
    if (stackSlotWillRemain) {
        --objectStack->count;
    } else {
        const int stackIndex = stackIndexAtScreenIndex(index);
        if (stackIndex >= 0) {
            removePackedSlotAtPackedIndex(stackIndex);
            objectStacks_.erase(objectStacks_.begin() + stackIndex);
        }
    }
    syncPackedItemSlots();
    moveObjectInstanceToScreenSlot(instanceId, index);
    equippedStaffInstanceId_ = instanceId;
    status_ = "杖を装備しました: " + item.name;
    return true;
}

bool InventorySystem::toggleStaffEquipmentScreenItem(int index, const SpellRingSystem& spellRing)
{
    if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(index)) {
        if (isStaffEquipped(objectInstance->instance.instanceId)) {
            return unequipStaff();
        }
    }
    return equipStaffScreenItem(index, spellRing);
}

bool InventorySystem::equipStaffObject(
    std::string_view objectId,
    std::string_view instanceId,
    const SpellRingSystem& spellRing,
    std::string* outStatus)
{
    const auto finish = [&](bool equipped) {
        if (outStatus != nullptr) {
            *outStatus = status_;
        }
        return equipped;
    };

    if (!instanceId.empty()) {
        for (int slot = 0; slot < screenSlotCount(); ++slot) {
            const InventoryObjectInstance* instance = screenObjectInstanceAt(slot);
            if (instance != nullptr && instance->instance.instanceId == instanceId) {
                return finish(equipStaffScreenItem(slot, spellRing));
            }
        }
        status_ = "杖がありません";
        return finish(false);
    }

    if (objectId.empty()) {
        status_ = "杖がありません";
        return finish(false);
    }

    for (int slot = 0; slot < screenSlotCount(); ++slot) {
        const InventoryObjectStack* stack = screenObjectStackAt(slot);
        if (stack != nullptr && stack->objectId == objectId && stack->count > 0) {
            return finish(equipStaffScreenItem(slot, spellRing));
        }
    }

    status_ = "杖がありません";
    return finish(false);
}

InventorySystem::SlotCommandList InventorySystem::buildSlotCommandItems(
    int slotIndex,
    bool itemUseEnabled,
    bool itemDiscardEnabled) const
{
    SlotCommandList list;
    list.items.reserve(SlotCommandMaxCount);
    list.actions.reserve(SlotCommandMaxCount);

    const bool hasItem = hasScreenItem(slotIndex);
    if (!hasItem) {
        return list;
    }

    const InventoryObjectStack* objectStack = objectStackAtScreenIndex(slotIndex);
    const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(slotIndex);
    const ItemData* item = objectStack != nullptr
        ? &objectStack->item
        : (objectInstance != nullptr ? &objectInstance->item : nullptr);
    const bool staff = item != nullptr && isStaffObject(*item);
    const bool equippedStaff = objectInstance != nullptr && isStaffEquipped(objectInstance->instance.instanceId);

    const auto addCommand = [&list](std::string_view label, bool enabled, SlotCommandAction action) {
        list.items.push_back(UiCommandMenuItem{label, enabled});
        list.actions.push_back(action);
    };

    if (staff) {
        addCommand(equippedStaff ? "外す" : "装備する", canEquipStaffScreenItem(slotIndex), SlotCommandAction::ToggleStaffEquipment);
    } else {
        addCommand("使用する", itemUseEnabled && canUseScreenItem(slotIndex), SlotCommandAction::Use);
    }
    if (!equippedStaff) {
        addCommand("リングへ", hasItem, SlotCommandAction::AddToRing);
    }
    if (objectInstance != nullptr) {
        addCommand(protectionCommandLabel(*objectInstance), true, SlotCommandAction::ToggleProtection);
    }
    addCommand("捨てる", canDiscardScreenItem(slotIndex, itemDiscardEnabled), SlotCommandAction::Discard);

    return list;
}

std::array<UiCommandMenuItem, SpellRingCount> InventorySystem::buildRingTargetCommandItems(
    int slotIndex,
    const SpellRingSystem& spellRing,
    int unlockedRingCount) const
{
    std::array<UiCommandMenuItem, SpellRingCount> items{};
    const int ringCount = clampedUnlockedRingCount(unlockedRingCount);
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        items[static_cast<std::size_t>(ringIndex)] = {
            "リング" + std::to_string(ringIndex + 1),
            ringIndex < ringCount && screenItemCanAddToRingForRing(slotIndex, spellRing, ringIndex),
        };
    }
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

bool InventorySystem::moveObjectStackToScreenSlot(std::string_view objectId, int slotIndex)
{
    if (objectId.empty() || slotIndex < 0 || slotIndex >= ShortcutSlotCount) {
        return false;
    }
    syncPackedItemSlots();
    for (int i = 0; i < static_cast<int>(objectStacks_.size()); ++i) {
        if (objectStacks_[static_cast<std::size_t>(i)].objectId == objectId) {
            return moveScreenItem(packedItemSlots_[static_cast<std::size_t>(i)], slotIndex);
        }
    }
    return false;
}

bool InventorySystem::moveObjectInstanceToScreenSlot(std::string_view instanceId, int slotIndex)
{
    if (instanceId.empty() || slotIndex < 0 || slotIndex >= ShortcutSlotCount) {
        return false;
    }
    syncPackedItemSlots();
    const int stackCount = static_cast<int>(objectStacks_.size());
    for (int i = 0; i < static_cast<int>(objectInstances_.size()); ++i) {
        if (objectInstances_[static_cast<std::size_t>(i)].instance.instanceId == instanceId) {
            const int packedIndex = stackCount + i;
            return moveScreenItem(packedItemSlots_[static_cast<std::size_t>(packedIndex)], slotIndex);
        }
    }
    return false;
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

void InventorySystem::openSlotCommandMenu(int slotIndex, bool itemUseEnabled, bool itemDiscardEnabled)
{
    if (!hasScreenItem(slotIndex)) {
        closeUiCommandMenu(slotCommandMenu_);
        slotCommandMenuIndex_ = -1;
        closeRingTargetCommandMenu();
        return;
    }
    closeRingTargetCommandMenu();
    const SlotCommandList commandItems = buildSlotCommandItems(slotIndex, itemUseEnabled, itemDiscardEnabled);
    slotCommandMenuIndex_ = slotIndex;
    const UiRect slotRect = inventorySlotRect(slotIndex);
    openUiCommandMenu(
        slotCommandMenu_,
        slotRect.pos + Vec2{slotRect.size.x - 20.0f, 0.0f},
        inventoryScreenRect(),
        static_cast<int>(commandItems.items.size()),
        commandItems.items.data(),
        120.0f,
        2);
}

void InventorySystem::closeRingTargetCommandMenu()
{
    closeUiCommandMenu(ringTargetCommandMenu_);
    ringTargetCommandSlotIndex_ = -1;
}

void InventorySystem::openRingTargetCommandMenu(
    int slotIndex,
    Vec2 anchor,
    const SpellRingSystem& spellRing,
    int unlockedRingCount)
{
    if (!hasScreenItem(slotIndex)) {
        closeRingTargetCommandMenu();
        return;
    }

    const int ringCount = clampedUnlockedRingCount(unlockedRingCount);
    const std::array<UiCommandMenuItem, SpellRingCount> items =
        buildRingTargetCommandItems(slotIndex, spellRing, ringCount);
    ringTargetCommandSlotIndex_ = slotIndex;
    openUiCommandMenu(
        ringTargetCommandMenu_,
        anchor,
        inventoryScreenRect(),
        ringCount,
        items.data(),
        132.0f,
        2);
}

void InventorySystem::resetSlotPointerPress()
{
    slotPointerPressIndex_ = -1;
    slotPointerPressMouse_ = {};
    slotPointerPressCanOpenMenu_ = false;
    slotPointerDragTriggered_ = false;
}

bool InventorySystem::screenItemCanAddToRing(
    int index,
    const SpellRingSystem& spellRing,
    std::optional<float> preferredAngle) const
{
    if (const InventoryObjectStack* stack = objectStackAtScreenIndex(index)) {
        if (stack->count <= 0) {
            return false;
        }
        return preferredAngle
            ? spellRing.canAddObjectItemAtAngle(stack->item, *preferredAngle)
            : spellRing.canAddObjectItem(stack->item);
    }
    if (const InventoryObjectInstance* instance = objectInstanceAtScreenIndex(index)) {
        if (isStaffEquipped(instance->instance.instanceId)) {
            return false;
        }
        return preferredAngle
            ? spellRing.canAddObjectItemAtAngle(instance->item, instance->instance, *preferredAngle)
            : spellRing.canAddObjectItem(instance->item, instance->instance);
    }
    return false;
}

bool InventorySystem::screenItemCanAddToRingForRing(int index, const SpellRingSystem& spellRing, int ringIndex) const
{
    if (const InventoryObjectStack* stack = objectStackAtScreenIndex(index)) {
        return stack->count > 0 && spellRing.canAddObjectItemForRing(ringIndex, stack->item);
    }
    if (const InventoryObjectInstance* instance = objectInstanceAtScreenIndex(index)) {
        if (isStaffEquipped(instance->instance.instanceId)) {
            return false;
        }
        return spellRing.canAddObjectItemForRing(ringIndex, instance->item, instance->instance);
    }
    return false;
}

bool InventorySystem::addScreenItemToRing(
    int index,
    SpellRingSystem& spellRing,
    std::optional<float> preferredAngle,
    SpellRingAddResult* outResult)
{
    if (InventoryObjectStack* stack = objectStackAtScreenIndex(index)) {
        if (stack->count <= 0) {
            status_ = "ショートカット空き";
            return false;
        }
        if (!screenItemCanAddToRing(index, spellRing, preferredAngle)) {
            status_ = spellRing.canAddItem() ? "リングへ配置できません" : "リング満杯";
            return false;
        }

        ItemInstance instance = createDetachedObjectInstance(stack->item);
        const bool added = preferredAngle
            ? spellRing.addObjectItemAtAngle(stack->item, instance, *preferredAngle, outResult)
            : spellRing.addObjectItem(stack->item, instance, outResult);
        if (!added) {
            status_ = "リングへ配置できません";
            return false;
        }

        status_ = "リングに追加: " + stack->item.name;
        --stack->count;
        if (stack->count <= 0) {
            const int objectIndex = stackIndexAtScreenIndex(index);
            if (objectIndex >= 0) {
                removePackedSlotAtPackedIndex(objectIndex);
                objectStacks_.erase(objectStacks_.begin() + objectIndex);
            }
        }
        return true;
    }

    if (InventoryObjectInstance* instance = objectInstanceAtScreenIndex(index)) {
        if (isStaffEquipped(instance->instance.instanceId)) {
            status_ = "装備中の杖はリングに乗せられません";
            return false;
        }
        if (!screenItemCanAddToRing(index, spellRing, preferredAngle)) {
            status_ = spellRing.canAddItem() ? "リングへ配置できません" : "リング満杯";
            return false;
        }

        const bool added = preferredAngle
            ? spellRing.addObjectItemAtAngle(instance->item, instance->instance, *preferredAngle, outResult)
            : spellRing.addObjectItem(instance->item, instance->instance, outResult);
        if (!added) {
            status_ = "リングへ配置できません";
            return false;
        }

        status_ = "リングに追加: " + instance->item.name;
        const int instanceIndex = instanceIndexAtScreenIndex(index);
        if (instanceIndex >= 0) {
            removePackedSlotAtPackedIndex(static_cast<int>(objectStacks_.size()) + instanceIndex);
            objectInstances_.erase(objectInstances_.begin() + instanceIndex);
        }
        return true;
    }

    status_ = "ショートカット空き";
    return false;
}

bool InventorySystem::addScreenItemToRingForRing(
    int index,
    SpellRingSystem& spellRing,
    int ringIndex,
    SpellRingAddResult* outResult)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        status_ = "リングへ配置できません";
        return false;
    }

    const std::string ringLabel = "リング" + std::to_string(ringIndex + 1);
    if (InventoryObjectStack* stack = objectStackAtScreenIndex(index)) {
        if (stack->count <= 0) {
            status_ = "ショートカット空き";
            return false;
        }
        if (!screenItemCanAddToRingForRing(index, spellRing, ringIndex)) {
            status_ = spellRing.canAddItemForRing(ringIndex) ? ringLabel + "へ配置できません" : ringLabel + "満杯";
            return false;
        }

        ItemInstance instance = createDetachedObjectInstance(stack->item);
        if (!spellRing.addObjectItemToRing(ringIndex, stack->item, instance, outResult)) {
            status_ = ringLabel + "へ配置できません";
            return false;
        }

        status_ = ringLabel + "に追加: " + stack->item.name;
        --stack->count;
        if (stack->count <= 0) {
            const int objectIndex = stackIndexAtScreenIndex(index);
            if (objectIndex >= 0) {
                removePackedSlotAtPackedIndex(objectIndex);
                objectStacks_.erase(objectStacks_.begin() + objectIndex);
            }
        }
        return true;
    }

    if (InventoryObjectInstance* instance = objectInstanceAtScreenIndex(index)) {
        if (isStaffEquipped(instance->instance.instanceId)) {
            status_ = "装備中の杖はリングに乗せられません";
            return false;
        }
        if (!screenItemCanAddToRingForRing(index, spellRing, ringIndex)) {
            status_ = spellRing.canAddItemForRing(ringIndex) ? ringLabel + "へ配置できません" : ringLabel + "満杯";
            return false;
        }

        if (!spellRing.addObjectItemToRing(ringIndex, instance->item, instance->instance, outResult)) {
            status_ = ringLabel + "へ配置できません";
            return false;
        }

        status_ = ringLabel + "に追加: " + instance->item.name;
        const int instanceIndex = instanceIndexAtScreenIndex(index);
        if (instanceIndex >= 0) {
            removePackedSlotAtPackedIndex(static_cast<int>(objectStacks_.size()) + instanceIndex);
            objectInstances_.erase(objectInstances_.begin() + instanceIndex);
        }
        return true;
    }

    status_ = "ショートカット空き";
    return false;
}

bool InventorySystem::addObjectToRing(
    std::string_view objectId,
    std::string_view instanceId,
    SpellRingSystem& spellRing,
    SpellRingAddResult* outResult,
    std::string* outStatus)
{
    const auto setStatus = [&](std::string message) {
        status_ = std::move(message);
        if (outStatus != nullptr) {
            *outStatus = status_;
        }
    };

    if (!instanceId.empty()) {
        const auto it = std::find_if(objectInstances_.begin(), objectInstances_.end(), [instanceId](const InventoryObjectInstance& entry) {
            return entry.instance.instanceId == instanceId;
        });
        if (it == objectInstances_.end()) {
            setStatus("リュックにありません");
            return false;
        }
        if (isStaffEquipped(instanceId)) {
            setStatus("装備中の杖はリングに乗せられません");
            return false;
        }
        if (it->instance.isBroken) {
            setStatus("壊れています");
            return false;
        }
        if (!spellRing.canAddObjectItem(it->item, it->instance)) {
            setStatus(spellRing.canAddItem() ? "リングへ配置できません" : "リング満杯");
            return false;
        }

        SpellRingAddResult result{};
        if (!spellRing.addObjectItem(it->item, it->instance, &result)) {
            setStatus("リングへ配置できません");
            return false;
        }

        if (outResult != nullptr) {
            *outResult = result;
        }
        const std::string itemName = it->item.name;
        const int instanceIndex = static_cast<int>(std::distance(objectInstances_.begin(), it));
        removePackedSlotAtPackedIndex(static_cast<int>(objectStacks_.size()) + instanceIndex);
        objectInstances_.erase(it);
        setStatus("リングに追加: " + itemName);
        return true;
    }

    if (objectId.empty()) {
        setStatus("リュックにありません");
        return false;
    }

    const auto it = std::find_if(objectStacks_.begin(), objectStacks_.end(), [objectId](const InventoryObjectStack& stack) {
        return stack.objectId == objectId && stack.count > 0;
    });
    if (it == objectStacks_.end()) {
        setStatus("リュックにありません");
        return false;
    }
    if (!spellRing.canAddObjectItem(it->item)) {
        setStatus(spellRing.canAddItem() ? "リングへ配置できません" : "リング満杯");
        return false;
    }

    ItemInstance instance = createDetachedObjectInstance(it->item);
    SpellRingAddResult result{};
    if (!spellRing.addObjectItem(it->item, instance, &result)) {
        setStatus("リングへ配置できません");
        return false;
    }

    if (outResult != nullptr) {
        *outResult = result;
    }
    const std::string itemName = it->item.name;
    --it->count;
    if (it->count <= 0) {
        const int stackIndex = static_cast<int>(std::distance(objectStacks_.begin(), it));
        removePackedSlotAtPackedIndex(stackIndex);
        objectStacks_.erase(it);
    }
    setStatus("リングに追加: " + itemName);
    return true;
}

bool InventorySystem::addObjectSelectionToRing(SpellRingSystem& spellRing, SpellRingAddResult* outResult)
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
    if (!spellRing.addObjectItem(selected->item, objectInstance.instance, outResult)) {
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

bool InventorySystem::addObjectInstanceSelectionToRing(SpellRingSystem& spellRing, SpellRingAddResult* outResult)
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
    if (isStaffEquipped(selected->instance.instanceId)) {
        status_ = "装備中の杖はリングに乗せられません";
        return false;
    }
    if (!spellRing.addObjectItem(selected->item, selected->instance, outResult)) {
        status_ = "リング満杯";
        return false;
    }
    status_ = "リングに追加: " + selected->item.name;
    const int instanceIndex = instanceIndexAtScreenIndex(index);
    removePackedSlotAtPackedIndex(static_cast<int>(objectStacks_.size()) + instanceIndex);
    objectInstances_.erase(objectInstances_.begin() + instanceIndex);
    return true;
}

bool InventorySystem::useObjectStackAtIndex(
    int stackIndex,
    Player& player,
    const EffectDispatcher& effectDispatcher,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    if (stackIndex < 0 || stackIndex >= static_cast<int>(objectStacks_.size())) {
        status_ = "Objects DB item not selected";
        return false;
    }

    InventoryObjectStack& selected = objectStacks_[static_cast<std::size_t>(stackIndex)];
    if (selected.count <= 0) {
        status_ = "Objects DB item not selected";
        return false;
    }
    if (isStaffObject(selected.item)) {
        status_ = "杖は装備用です";
        return false;
    }
    if (selected.item.normalEffects.empty()) {
        status_ = "通常効果なし";
        return false;
    }

    bool hasImplementedEffect = false;
    for (const EffectSpec& spec : selected.item.normalEffects) {
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
        context.magic = magic;
        context.statusPopupEvents = &statusPopupEvents_;
        context.discoveryEvents = discoveryEvents;
        context.encyclopedia = encyclopedia;
        context.position = player.position;
        context.triggerType = EffectTriggerType::NormalUse;
        context.logUnimplementedEffects = true;
        effectDispatcher.dispatchNormalEffects(selected.item, context);
        status_ = "未実装効果";
        return false;
    }

    const int beforeHp = player.hp;
    EffectContext context;
    context.owner = &player;
    context.magic = magic;
    context.statusPopupEvents = &statusPopupEvents_;
    context.discoveryEvents = discoveryEvents;
    context.encyclopedia = encyclopedia;
    context.position = player.position;
    context.triggerType = EffectTriggerType::NormalUse;
    context.logUnimplementedEffects = true;
    effectDispatcher.dispatchNormalEffects(selected.item, context);

    if (player.hp == beforeHp && beforeHp >= player.maxHp) {
        bool healOnly = true;
        for (const EffectSpec& spec : selected.item.normalEffects) {
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

    status_ = "使用: " + selected.item.name;
    const ItemData usedItem = selected.item;
    const int healedAmount = std::max(0, player.hp - beforeHp);
    --selected.count;
    if (selected.count <= 0) {
        removePackedSlotAtPackedIndex(stackIndex);
        objectStacks_.erase(objectStacks_.begin() + stackIndex);
    }
    useEvents_.push_back(InventoryUseEvent{usedItem, std::nullopt, healedAmount});
    return true;
}

bool InventorySystem::useObjectSelection(
    Player& player,
    const EffectDispatcher& effectDispatcher,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    const int index = selectedShortcutIndex();
    const int objectIndex = stackIndexAtScreenIndex(index);
    if (objectIndex < 0) {
        status_ = "Objects DB item not selected";
        return false;
    }

    return useObjectStackAtIndex(
        objectIndex,
        player,
        effectDispatcher,
        magic,
        discoveryEvents,
        encyclopedia);
}

bool InventorySystem::useObjectInstanceAtIndex(
    int instanceIndex,
    Player& player,
    const EffectDispatcher& effectDispatcher,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(objectInstances_.size())) {
        status_ = "個体アイテム未選択";
        return false;
    }

    InventoryObjectInstance& selected = objectInstances_[static_cast<std::size_t>(instanceIndex)];
    if (selected.instance.isBroken) {
        status_ = "壊れています";
        return false;
    }
    if (isStaffObject(selected.item)) {
        status_ = "杖は装備用です";
        return false;
    }
    if (selected.item.normalEffects.empty()) {
        status_ = "通常効果なし";
        return false;
    }

    const int beforeHp = player.hp;
    EffectContext context;
    context.owner = &player;
    context.magic = magic;
    context.statusPopupEvents = &statusPopupEvents_;
    context.discoveryEvents = discoveryEvents;
    context.encyclopedia = encyclopedia;
    context.position = player.position;
    context.triggerType = EffectTriggerType::NormalUse;
    context.logUnimplementedEffects = true;
    effectDispatcher.dispatchNormalEffects(selected.item, context);
    effectDispatcher.dispatch(selected.instance.addedEffects, context);
    status_ = "使用: " + selected.item.name;
    useEvents_.push_back(InventoryUseEvent{
        selected.item,
        std::optional<ItemInstance>{selected.instance},
        std::max(0, player.hp - beforeHp)});
    return true;
}

bool InventorySystem::useObjectInstanceSelection(
    Player& player,
    const EffectDispatcher& effectDispatcher,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    const int index = selectedShortcutIndex();
    const int instanceIndex = instanceIndexAtScreenIndex(index);
    if (instanceIndex < 0) {
        status_ = "個体アイテム未選択";
        return false;
    }

    return useObjectInstanceAtIndex(
        instanceIndex,
        player,
        effectDispatcher,
        magic,
        discoveryEvents,
        encyclopedia);
}

bool InventorySystem::useShortcutSelection(
    Player& player,
    SpellRingSystem& spellRing,
    const EffectDispatcher& effectDispatcher,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    (void)spellRing;
    if (selectedObjectStack() != nullptr) {
        return useObjectSelection(player, effectDispatcher, magic, discoveryEvents, encyclopedia);
    }
    if (selectedObjectInstance() != nullptr) {
        return useObjectInstanceSelection(player, effectDispatcher, magic, discoveryEvents, encyclopedia);
    }

    status_ = "ショートカット空き";
    return false;
}

bool InventorySystem::addShortcutSelectionToRing(SpellRingSystem& spellRing, SpellRingAddResult* outResult)
{
    return addScreenItemToRing(selectedShortcutIndex(), spellRing, std::nullopt, outResult);
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

void InventorySystem::queueRingEquipFx(Vec2 sourceScreen, const SpellRingAddResult& result)
{
    ringEquipFxRequests_.push_back(RingEquipFxRequest{
        .sourceScreen = sourceScreen,
        .ringIndex = result.ringIndex,
        .itemIndex = result.itemIndex,
        .localAngle = result.localAngle,
        .objectId = result.objectId,
        .instanceId = result.instanceId,
    });
}

void InventorySystem::updateShortcuts(
    const Input& input,
    UiContext& ui,
    Player& player,
    SpellRingSystem& spellRing,
    const EffectDispatcher& effectDispatcher,
    int screenWidth,
    int screenHeight,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    int hoveredSlotIndex = -1;
    if (!ui.pointerConsumed()) {
        for (int column = 0; column < ShortcutColumns; ++column) {
            const UiRect rect = shortcutHudSlotRect(column, screenWidth, screenHeight);
            if (rect.contains(ui.mouse())) {
                hoveredSlotIndex = shortcutRow_ * ShortcutColumns + column;
                selectShortcutIndex(hoveredSlotIndex);
                break;
            }
        }
    }

    if (hoveredSlotIndex >= 0 && input.mouseLeftPressed() && !ui.pointerConsumed()) {
        if (canUseScreenItem(hoveredSlotIndex)) {
            if (useShortcutSelection(player, spellRing, effectDispatcher, magic, discoveryEvents, encyclopedia)) {
                ui.emitSound(UiSoundEvent::ItemUse);
            } else {
                ui.emitSound(UiSoundEvent::Cancel);
            }
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
        }
        ui.consumePointer();
        return;
    }

    if (input.shortcutCursorDelta() != 0) {
        moveShortcutCursor(input.shortcutCursorDelta());
    }

    if (input.toggleShortcutRowPressed()) {
        toggleShortcutRow();
    }

    if (input.useItemPressed()) {
        ui.emitSound(useShortcutSelection(player, spellRing, effectDispatcher, magic, discoveryEvents, encyclopedia)
            ? UiSoundEvent::ItemUse
            : UiSoundEvent::Cancel);
    }
    if (input.addRingPressed()) {
        SpellRingAddResult result{};
        if (addShortcutSelectionToRing(spellRing, &result)) {
            ui.emitSound(UiSoundEvent::RingPlace);
            queueRingEquipFx(shortcutHudSlotCenter(selectedShortcutColumn_, screenWidth, screenHeight), result);
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
        }
    }
    if (input.pressed(InputAction::ToggleProtection)) {
        ui.emitSound(selectedObjectInstance() != nullptr ? UiSoundEvent::Confirm : UiSoundEvent::Cancel);
        toggleSelectedProtection();
    }
}

void InventorySystem::updateScreen(
    const Input& input,
    UiContext& ui,
    Player& player,
    SpellRingSystem& spellRing,
    const EffectDispatcher& effectDispatcher,
    const ObjectCatalog& catalog,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia,
    bool itemUseEnabled,
    bool itemDiscardEnabled,
    int unlockedRingCount)
{
    if (!open_) {
        return;
    }
    const int ringTargetCount = clampedUnlockedRingCount(unlockedRingCount);

    if (discardConfirm_.open) {
        const UiRect confirmPanel = inventoryDiscardConfirmRect();
        const UiConfirmDialogResult result = updateUiConfirmDialog(discardConfirm_, ui, input, confirmPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            discardScreenItem(discardConfirmSlotIndex_, false);
            discardConfirmSlotIndex_ = -1;
        } else if (result == UiConfirmDialogResult::Cancelled) {
            discardConfirmSlotIndex_ = -1;
        }
        ui.block(inventoryScreenRect());
        return;
    }

    const int ringTargetSlotIndex = ringTargetCommandSlotIndex_ >= 0
        ? ringTargetCommandSlotIndex_
        : selectedShortcutIndex();
    const std::array<UiCommandMenuItem, SpellRingCount> ringTargetItems =
        buildRingTargetCommandItems(ringTargetSlotIndex, spellRing, ringTargetCount);
    const bool ringTargetOpenBeforeUpdate = ringTargetCommandMenu_.open;
    const int ringTargetSelection = updateUiCommandMenu(
        ringTargetCommandMenu_,
        ui,
        input,
        ringTargetItems.data(),
        ringTargetCount);
    if (ringTargetSelection >= 0 &&
        ringTargetSelection < ringTargetCount &&
        ringTargetCommandSlotIndex_ >= 0) {
        selectShortcutIndex(ringTargetCommandSlotIndex_);
        if (!addScreenItemToRingForRing(ringTargetCommandSlotIndex_, spellRing, ringTargetSelection)) {
            ui.emitSound(UiSoundEvent::Cancel);
        }
        closeRingTargetCommandMenu();
        slotCommandMenuIndex_ = -1;
        resetSlotPointerPress();
        ui.block(inventoryScreenRect());
        return;
    }
    if (ringTargetOpenBeforeUpdate && !ringTargetCommandMenu_.open) {
        ringTargetCommandSlotIndex_ = -1;
        slotCommandMenuIndex_ = -1;
        resetSlotPointerPress();
        ui.block(inventoryScreenRect());
        return;
    }
    if (ringTargetCommandMenu_.open) {
        ui.block(inventoryScreenRect());
        return;
    }

    const bool commandOpenBeforeUpdate = slotCommandMenu_.open;
    const int commandSlotIndex = slotCommandMenuIndex_ >= 0 ? slotCommandMenuIndex_ : selectedShortcutIndex();
    const SlotCommandList commandItems = buildSlotCommandItems(
        commandSlotIndex,
        itemUseEnabled,
        itemDiscardEnabled);
    const int commandSelection = updateUiCommandMenu(
        slotCommandMenu_,
        ui,
        input,
        commandItems.items.data(),
        static_cast<int>(commandItems.items.size()));
    if (commandSelection >= 0 &&
        commandSelection < static_cast<int>(commandItems.actions.size()) &&
        slotCommandMenuIndex_ >= 0) {
        selectShortcutIndex(slotCommandMenuIndex_);
        const SlotCommandAction action = commandItems.actions[static_cast<std::size_t>(commandSelection)];
        if (action == SlotCommandAction::Use) {
            if (grabbedSlotActive_) {
                status_ = "つかみ中は使用できません";
            } else {
                useShortcutSelection(player, spellRing, effectDispatcher, magic, discoveryEvents, encyclopedia);
            }
        } else if (action == SlotCommandAction::AddToRing) {
            if (grabbedSlotActive_) {
                status_ = "つかみ中はリング移動できません";
            } else if (ringTargetCount > 1) {
                const Vec2 submenuAnchor{
                    slotCommandMenu_.panel.pos.x + slotCommandMenu_.panel.size.x,
                    slotCommandMenu_.panel.pos.y - 12.0f,
                };
                openRingTargetCommandMenu(slotCommandMenuIndex_, submenuAnchor, spellRing, ringTargetCount);
            } else {
                addScreenItemToRingForRing(slotCommandMenuIndex_, spellRing, 0);
            }
        } else if (action == SlotCommandAction::ToggleStaffEquipment) {
            if (grabbedSlotActive_) {
                status_ = "つかみ中は装備変更できません";
            } else {
                toggleStaffEquipmentScreenItem(slotCommandMenuIndex_, spellRing);
            }
        } else if (action == SlotCommandAction::ToggleProtection) {
            if (grabbedSlotActive_) {
                status_ = "つかみ中は保護変更できません";
            } else {
                toggleSelectedProtection();
            }
        } else if (action == SlotCommandAction::Discard) {
            if (grabbedSlotActive_) {
                status_ = "つかみ中は捨てられません";
            } else if (!itemDiscardEnabled) {
                openDiscardConfirmDialog(slotCommandMenuIndex_);
            } else {
                discardScreenItem(slotCommandMenuIndex_, itemDiscardEnabled);
            }
        }
        if (!ringTargetCommandMenu_.open) {
            slotCommandMenuIndex_ = -1;
        }
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

    if (uiCancelRequested(cancelState_, input, ui, inventoryScreenRect())) {
        if (slotCommandMenu_.open) {
            closeUiCommandMenu(slotCommandMenu_);
            slotCommandMenuIndex_ = -1;
            closeRingTargetCommandMenu();
            resetSlotPointerPress();
            return;
        }
        if (grabbedSlotActive_) {
            cancelGrab();
        } else {
            closeUiCommandMenu(slotCommandMenu_);
            slotCommandMenuIndex_ = -1;
            closeRingTargetCommandMenu();
            resetSlotPointerPress();
            open_ = false;
        }
        return;
    }

    if (slotCommandMenu_.open) {
        ui.block(inventoryScreenRect());
        return;
    }

    if (input.arrangeItemsPressed() || ui.pressed(inventorySortButtonRect())) {
        ui.emitSound(sortByCatalogOrder(catalog) ? UiSoundEvent::ItemMove : UiSoundEvent::Cancel);
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
        if (slotPointerDragTriggered_) {
            ui.emitSound(UiSoundEvent::ItemMove);
        }
        slotPointerPressCanOpenMenu_ = false;
        closeUiCommandMenu(slotCommandMenu_);
        slotCommandMenuIndex_ = -1;
        closeRingTargetCommandMenu();
    }

    if (slotPointerPressIndex_ >= 0 && input.mouseLeftReleased()) {
        if (slotPointerDragTriggered_ && grabbedSlotActive_) {
            if (hoveredSlotIndex >= 0) {
                selectShortcutIndex(hoveredSlotIndex);
            }
            placeGrabbedAtSelected();
            ui.emitSound(UiSoundEvent::ItemMove);
        } else if (slotPointerPressCanOpenMenu_ && hoveredSlotIndex == slotPointerPressIndex_) {
            openSlotCommandMenu(slotPointerPressIndex_, itemUseEnabled, itemDiscardEnabled);
        }
        resetSlotPointerPress();
    }

    if (input.grabOrPlacePressed()) {
        closeUiCommandMenu(slotCommandMenu_);
        slotCommandMenuIndex_ = -1;
        closeRingTargetCommandMenu();
        ui.emitSound(grabbedSlotActive_ || hasScreenItem(selectedShortcutIndex()) ? UiSoundEvent::ItemMove : UiSoundEvent::Cancel);
        grabOrPlaceSelected();
    }
    if (input.useItemPressed() || input.confirmPressed()) {
        if (grabbedSlotActive_) {
            placeGrabbedAtSelected();
            ui.emitSound(UiSoundEvent::ItemMove);
        } else if (hasScreenItem(selectedShortcutIndex())) {
            openSlotCommandMenu(selectedShortcutIndex(), itemUseEnabled, itemDiscardEnabled);
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
        }
    }
    if (input.addRingPressed()) {
        if (grabbedSlotActive_) {
            ui.emitSound(UiSoundEvent::Cancel);
            status_ = "つかみ中は配置してください";
        } else if (ringTargetCount > 1 && hasScreenItem(selectedShortcutIndex())) {
            const UiRect slotRect = inventorySlotRect(selectedShortcutIndex());
            openRingTargetCommandMenu(
                selectedShortcutIndex(),
                slotRect.pos + Vec2{slotRect.size.x - 20.0f, 0.0f},
                spellRing,
                ringTargetCount);
            ui.block(inventoryScreenRect());
            return;
        } else {
            ui.emitSound(addScreenItemToRingForRing(selectedShortcutIndex(), spellRing, 0) ? UiSoundEvent::RingPlace : UiSoundEvent::Cancel);
        }
    }
    if (input.pressed(InputAction::ToggleProtection)) {
        if (grabbedSlotActive_) {
            ui.emitSound(UiSoundEvent::Cancel);
            status_ = "つかみ中は保護変更できません";
        } else {
            ui.emitSound(selectedObjectInstance() != nullptr ? UiSoundEvent::Confirm : UiSoundEvent::Cancel);
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
    const ObjectCatalog& catalog,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia,
    int unlockedRingCount)
{
    if (input.inventoryPressed() && !blocked) {
        if (open_) {
            closeUiCommandMenu(slotCommandMenu_);
            slotCommandMenuIndex_ = -1;
            closeRingTargetCommandMenu();
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
    updateScreen(
        input,
        ui,
        player,
        spellRing,
        effectDispatcher,
        catalog,
        magic,
        discoveryEvents,
        encyclopedia,
        true,
        true,
        unlockedRingCount);
}

void InventorySystem::render(
    Renderer& renderer,
    const Player& player,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& catalog,
    const EncyclopediaSystem& encyclopedia,
    bool itemUseEnabled,
    bool itemDiscardEnabled,
    float animationSeconds,
    int unlockedRingCount) const
{
    (void)player;
    (void)spellRing;

    renderer.setScreenSpace();

    if (!open_) {
        return;
    }

    UiCancelControlScope cancelScope(cancelState_);
    UiWindowScope inventoryWindow(
        renderer,
        "inventory.main",
        {{ScreenX, ScreenY}, {ScreenW, ScreenH}},
        "アイテム",
        "F/Enter 決定  R リングへ  P 保護  G つかむ/置く  T 並び替え  Esc 戻る",
        UiWindowOptions{true, true});

    const auto entryViewForSlot = [this](int slotIndex) {
        InventoryUiEntryView entry{};
        const InventoryObjectStack* objectStack = objectStackAtScreenIndex(slotIndex);
        if (objectStack != nullptr) {
            entry.item = &objectStack->item;
            entry.stackCount = objectStack->count;
        } else if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(slotIndex)) {
            entry.item = &objectInstance->item;
            entry.instance = &objectInstance->instance;
            entry.stackCount = 1;
            entry.equipped = isStaffEquipped(objectInstance->instance.instanceId);
        }
        return entry;
    };

    for (int i = 0; i < ShortcutSlotCount; ++i) {
        const UiRect rect = inventorySlotRect(i);
        InventoryUiSlotStyle style{i == selectedShortcutIndex(), false, InventoryObjectImageMaxSize};
        if (grabbedSlotActive_ && i == grabbedSlotOrigin_) {
            style.contentAlpha = GrabbedSlotContentAlpha;
        }
        drawInventoryUiSlot(renderer, rect, entryViewForSlot(i), style);
    }

    if (grabbedSlotActive_ && grabbedSlotOrigin_ >= 0 && grabbedSlotOrigin_ < ShortcutSlotCount) {
        const InventoryUiEntryView grabbedEntry = entryViewForSlot(grabbedSlotOrigin_);
        if (grabbedEntry.item != nullptr) {
            const UiRect targetRect = inventorySlotRect(selectedShortcutIndex());
            const float bob = std::sin(animationSeconds * GrabbedFloatingIconBobSpeed) * GrabbedFloatingIconBobAmplitude;
            const Vec2 iconCenter = uiRectCenter(targetRect) + Vec2{0.0f, -GrabbedFloatingIconLift + bob};
            drawInventoryUiItemIcon(
                renderer,
                iconCenter,
                grabbedEntry,
                InventoryObjectImageMaxSize,
                false,
                false,
                1.0f);
        }
    }

    int detailIndex = selectedShortcutIndex();
    if (slotCommandMenu_.open && slotCommandMenuIndex_ >= 0) {
        detailIndex = slotCommandMenuIndex_;
    }
    if (ringTargetCommandMenu_.open && ringTargetCommandSlotIndex_ >= 0) {
        detailIndex = ringTargetCommandSlotIndex_;
    }
    const InventoryObjectStack* detailStack = objectStackAtScreenIndex(detailIndex);
    const InventoryObjectInstance* detailInstance = objectInstanceAtScreenIndex(detailIndex);

    InventoryUiEntryView detailEntry{};
    if (detailStack != nullptr) {
        detailEntry.item = &detailStack->item;
        detailEntry.stackCount = detailStack->count;
    } else if (detailInstance != nullptr) {
        detailEntry.item = &detailInstance->item;
        detailEntry.instance = &detailInstance->instance;
        detailEntry.stackCount = 1;
        detailEntry.equipped = isStaffEquipped(detailInstance->instance.instanceId);
    }

    const UiRect detailPanel{{DetailX, DetailY}, {DetailW, DetailH}};
    drawInventoryUiDetailPanel(
        renderer,
        detailPanel,
        detailEntry,
        catalog,
        encyclopedia,
        InventoryUiDetailOptions{.animationSeconds = animationSeconds});
    drawUiButton(renderer, inventorySortButtonRect(), "並び替え", false, uiActionButtonStyle());

    const int commandSlotIndex = slotCommandMenuIndex_ >= 0 ? slotCommandMenuIndex_ : selectedShortcutIndex();
    const SlotCommandList commandItems = buildSlotCommandItems(
        commandSlotIndex,
        itemUseEnabled,
        itemDiscardEnabled);
    drawUiCommandMenu(
        renderer,
        slotCommandMenu_,
        commandItems.items.data(),
        static_cast<int>(commandItems.items.size()));
    const int ringTargetSlotIndex = ringTargetCommandSlotIndex_ >= 0
        ? ringTargetCommandSlotIndex_
        : selectedShortcutIndex();
    const int ringTargetCount = clampedUnlockedRingCount(unlockedRingCount);
    const std::array<UiCommandMenuItem, SpellRingCount> ringTargetItems =
        buildRingTargetCommandItems(ringTargetSlotIndex, spellRing, ringTargetCount);
    drawUiCommandMenu(
        renderer,
        ringTargetCommandMenu_,
        ringTargetItems.data(),
        ringTargetCount);
    drawDiscardConfirmDialog(renderer, catalog);

}

UiRect InventorySystem::shortcutHudPanelRect(int screenWidth, int screenHeight) const
{
    return makeShortcutHudPanelRect(screenWidth, screenHeight);
}

void InventorySystem::renderShortcutHud(Renderer& renderer, const SpellRingSystem& spellRing, int screenWidth, int screenHeight) const
{
    renderer.setScreenSpace();

    const UiRect hudPanel = shortcutHudPanelRect(screenWidth, screenHeight);

    const auto entryViewForSlot = [this](int slotIndex) {
        InventoryUiEntryView entry{};
        if (const InventoryObjectStack* objectStack = objectStackAtScreenIndex(slotIndex)) {
            entry.item = &objectStack->item;
            entry.stackCount = objectStack->count;
        } else if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(slotIndex)) {
            entry.item = &objectInstance->item;
            entry.instance = &objectInstance->instance;
            entry.stackCount = 1;
            entry.equipped = isStaffEquipped(objectInstance->instance.instanceId);
        }
        return entry;
    };

    const auto selectedItemName = [this]() {
        char buffer[128];
        const int selectedIndex = selectedShortcutIndex();
        if (const InventoryObjectStack* objectStack = objectStackAtScreenIndex(selectedIndex)) {
            const std::string name = itemDisplayName(objectStack->item.name, objectStack->item.durability == 0);
            std::snprintf(buffer, sizeof(buffer), "%s x%d", name.c_str(), objectStack->count);
            return std::string(buffer);
        }
        if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(selectedIndex)) {
            return itemDisplayName(objectInstance->item.name, objectInstance->instance.isBroken);
        }
        return std::string{};
    }();

    if (!selectedItemName.empty()) {
        constexpr int NameScale = 2;
        const Vec2 nameSize = renderer.measureText(selectedItemName, NameScale);
        const UiRect selectedRect = shortcutHudSlotRect(selectedShortcutColumn_, screenWidth, screenHeight);
        const float minX = hudPanel.pos.x + 18.0f;
        const float maxX = hudPanel.pos.x + hudPanel.size.x - 18.0f - nameSize.x;
        const float centeredX = selectedRect.pos.x + (selectedRect.size.x - nameSize.x) * 0.5f;
        const Vec2 namePos{
            std::clamp(centeredX, minX, std::max(minX, maxX)),
            hudPanel.pos.y + HudSelectedNameY,
        };
        renderer.drawOutlinedText(namePos, selectedItemName, ui::Text, {0, 0, 0, 120}, 6, NameScale);
    }

    (void)spellRing;

    for (int column = 0; column < ShortcutColumns; ++column) {
        const int slotIndex = shortcutRow_ * ShortcutColumns + column;
        const bool selected = column == selectedShortcutColumn_;
        const UiRect slotRect = shortcutHudSlotRect(column, screenWidth, screenHeight);
        const InventoryUiEntryView entry = entryViewForSlot(slotIndex);
        InventoryUiSlotStyle style{selected, false, ShortcutObjectImageMaxSize};
        if (entry.item != nullptr && entry.instance == nullptr && entry.stackCount > 1) {
            style.showTopRightCount = true;
            style.topRightCount = entry.stackCount;
        }
        drawInventoryUiSlot(renderer, slotRect, entry, style);
    }

}

}
