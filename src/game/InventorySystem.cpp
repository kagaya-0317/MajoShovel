#include "game/InventorySystem.hpp"

#include "engine/Log.hpp"
#include "game/EffectDispatcher.hpp"

#include <algorithm>
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
constexpr float DetailX = ScreenX + 820.0f;
constexpr float DetailY = ScreenY + 84.0f;
constexpr float DetailW = 330.0f;
constexpr float DetailH = 480.0f;

UiRect inventoryToggleRect()
{
    return {{1122.0f, 18.0f}, {124.0f, 38.0f}};
}

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
    return {{PanelX + 26.0f, PanelY + 350.0f}, {130.0f, 38.0f}};
}

UiRect ringButtonRect()
{
    return {{PanelX + 166.0f, PanelY + 350.0f}, {130.0f, 38.0f}};
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

bool canAddToRing(InventoryItemType type)
{
    return type == InventoryItemType::Stone || type == InventoryItemType::Ore;
}

SpellRingItemType ringTypeFor(InventoryItemType type)
{
    return type == InventoryItemType::Ore ? SpellRingItemType::Ore : SpellRingItemType::Stone;
}

const char* inventoryItemCategory(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "素材";
    case InventoryItemType::Stone: return "素材";
    case InventoryItemType::Ore: return "素材";
    case InventoryItemType::Count: break;
    }
    return "";
}

const char* inventoryItemDescription(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "掘削で手に入る土。簡易回復に使える。";
    case InventoryItemType::Stone: return "硬い石。シャベル強化やリング追加に使える。";
    case InventoryItemType::Ore: return "魔力を含む鉱石。攻撃強化やリング追加に使える。";
    case InventoryItemType::Count: break;
    }
    return "";
}

const char* inventoryNormalEffect(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "heal +1 (仮)";
    case InventoryItemType::Stone: return "shovel_power +1 (仮)";
    case InventoryItemType::Ore: return "damage +1 (仮)";
    case InventoryItemType::Count: break;
    }
    return "";
}

const char* inventoryOrbitEffect(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "なし";
    case InventoryItemType::Stone: return "stone item";
    case InventoryItemType::Ore: return "ore item";
    case InventoryItemType::Count: break;
    }
    return "";
}

const char* inventoryDamageType(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "なし";
    case InventoryItemType::Stone: return "打撃";
    case InventoryItemType::Ore: return "打撃";
    case InventoryItemType::Count: break;
    }
    return "";
}

const char* inventoryTags(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "material, earth";
    case InventoryItemType::Stone: return "material, ring_add";
    case InventoryItemType::Ore: return "material, ring_add";
    case InventoryItemType::Count: break;
    }
    return "";
}

int inventoryAttackPower(InventoryItemType type)
{
    return type == InventoryItemType::Ore ? 1 : 0;
}

int inventoryDigPower(InventoryItemType type)
{
    return type == InventoryItemType::Stone ? 1 : 0;
}

int inventoryDurability(InventoryItemType type)
{
    return type == InventoryItemType::Dirt ? 1 : 3;
}

double inventoryWeightKg(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return 0.2;
    case InventoryItemType::Stone: return 1.4;
    case InventoryItemType::Ore: return 2.0;
    case InventoryItemType::Count: break;
    }
    return 0.0;
}

void drawDetailLine(Renderer& renderer, float& y, const char* label, std::string_view value)
{
    constexpr float ValueX = DetailX + 126.0f;
    constexpr float ValueMaxW = DetailX + DetailW - ValueX - 18.0f;
    renderer.drawText({DetailX + 20.0f, y}, label, ui::TextMuted, 2);
    renderer.drawWrappedText({ValueX, y}, value, ValueMaxW, ui::Text, 2);
    y += std::max(31.0f, renderer.measureWrappedText(value, ValueMaxW, 2).y + 4.0f);
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

std::string joinValues(const std::vector<std::string>& values)
{
    if (values.empty()) {
        return "-";
    }

    std::string joined;
    for (const std::string& value : values) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += value;
    }
    return joined;
}

std::string effectSummary(const std::vector<EffectSpec>& specs)
{
    std::string summary;
    for (const EffectSpec& spec : specs) {
        for (const std::string& effect : spec.effects) {
            if (!summary.empty()) {
                summary += ", ";
            }
            summary += effect;
        }
    }
    return summary.empty() ? "-" : summary;
}
}

const char* inventoryItemName(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "土";
    case InventoryItemType::Stone: return "石";
    case InventoryItemType::Ore: return "鉱石";
    case InventoryItemType::Count: break;
    }
    return "ITEM";
}

const char* inventoryItemUseText(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "使う 体力 +1";
    case InventoryItemType::Stone: return "使う シャベル +1";
    case InventoryItemType::Ore: return "使う ダメージ +1";
    case InventoryItemType::Count: break;
    }
    return "";
}

int inventoryItemIcon(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return 4;
    case InventoryItemType::Stone: return 5;
    case InventoryItemType::Ore: return 6;
    case InventoryItemType::Count: break;
    }
    return 0;
}

Color inventoryItemColor(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return {128, 82, 44, 255};
    case InventoryItemType::Stone: return {118, 122, 132, 255};
    case InventoryItemType::Ore: return {96, 122, 210, 255};
    case InventoryItemType::Count: break;
    }
    return {255, 255, 255, 255};
}

InventoryStack& InventorySystem::stack(InventoryItemType type)
{
    return stacks_[static_cast<int>(type)];
}

const InventoryStack& InventorySystem::stack(InventoryItemType type) const
{
    return stacks_[static_cast<int>(type)];
}

int InventorySystem::itemCount(InventoryItemType type) const
{
    return stack(type).count;
}

void InventorySystem::setItemCount(InventoryItemType type, int count)
{
    stack(type).count = std::max(0, count);
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
    if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount - ItemCount) {
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
    if (stackSlotWillRemain && static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount - ItemCount) {
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
        if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount - ItemCount) {
            status_ = "インベントリ満杯";
            return false;
        }
        createObjectInstance(selectedStack->item);
        --selectedStack->count;
        if (selectedStack->count <= 0) {
            const int objectIndex = selectedShortcutIndex() - ItemCount;
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

InventoryItemType InventorySystem::selectedType() const
{
    return stacks_[selected_].type;
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
    const int objectIndex = index - ItemCount;
    if (objectIndex < 0 || objectIndex >= static_cast<int>(objectStacks_.size())) {
        return nullptr;
    }
    return &objectStacks_[static_cast<std::size_t>(objectIndex)];
}

InventoryObjectStack* InventorySystem::objectStackAtScreenIndex(int index)
{
    const int objectIndex = index - ItemCount;
    if (objectIndex < 0 || objectIndex >= static_cast<int>(objectStacks_.size())) {
        return nullptr;
    }
    return &objectStacks_[static_cast<std::size_t>(objectIndex)];
}

const InventoryObjectInstance* InventorySystem::objectInstanceAtScreenIndex(int index) const
{
    const int instanceIndex = index - ItemCount - static_cast<int>(objectStacks_.size());
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(objectInstances_.size())) {
        return nullptr;
    }
    return &objectInstances_[static_cast<std::size_t>(instanceIndex)];
}

InventoryObjectInstance* InventorySystem::objectInstanceAtScreenIndex(int index)
{
    const int instanceIndex = index - ItemCount - static_cast<int>(objectStacks_.size());
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(objectInstances_.size())) {
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

void InventorySystem::addFromDugTile(TileType type)
{
    switch (type) {
    case TileType::Dirt:
        ++stack(InventoryItemType::Dirt).count;
        status_ = "土を入手";
        break;
    case TileType::Rock:
    case TileType::HardRock:
        ++stack(InventoryItemType::Stone).count;
        status_ = "石を入手";
        break;
    case TileType::Ore:
        ++stack(InventoryItemType::Ore).count;
        status_ = "鉱石を入手";
        break;
    case TileType::Empty:
        break;
    }
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
        if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount - ItemCount) {
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

    if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount - ItemCount) {
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
        if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount - ItemCount) {
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

    if (static_cast<int>(objectStacks_.size() + objectInstances_.size()) >= ShortcutSlotCount - ItemCount) {
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

void InventorySystem::toggleShortcutRow()
{
    shortcutRow_ = (shortcutRow_ + 1) % ShortcutRows;
}

void InventorySystem::grabOrPlaceSelected()
{
    ShortcutSlot& selected = selectedShortcutSlot();
    if (grabbedSlotActive_) {
        placeGrabbedAtSelected();
        return;
    }
    if (!selected.assigned) {
        status_ = "空き枠";
        return;
    }

    grabbedSlot_ = selected;
    grabbedSlotOrigin_ = selectedShortcutIndex();
    grabbedSlotActive_ = true;
    selected = ShortcutSlot{};
    status_ = "つかみ中";
}

void InventorySystem::placeGrabbedAtSelected()
{
    if (!grabbedSlotActive_) {
        return;
    }

    ShortcutSlot& selected = selectedShortcutSlot();
    const int targetIndex = selectedShortcutIndex();
    const ShortcutSlot displaced = selected;
    selected = grabbedSlot_;
    if (displaced.assigned && grabbedSlotOrigin_ >= 0 && grabbedSlotOrigin_ < ShortcutSlotCount && grabbedSlotOrigin_ != targetIndex) {
        shortcutSlots_[grabbedSlotOrigin_] = displaced;
    }
    grabbedSlotActive_ = false;
    grabbedSlot_ = ShortcutSlot{};
    grabbedSlotOrigin_ = -1;
    status_ = displaced.assigned ? "入れ替え" : "配置";
}

void InventorySystem::cancelGrab()
{
    if (!grabbedSlotActive_) {
        return;
    }

    if (grabbedSlotOrigin_ >= 0 && grabbedSlotOrigin_ < ShortcutSlotCount && !shortcutSlots_[grabbedSlotOrigin_].assigned) {
        shortcutSlots_[grabbedSlotOrigin_] = grabbedSlot_;
        grabbedSlot_ = ShortcutSlot{};
        grabbedSlotOrigin_ = -1;
        grabbedSlotActive_ = false;
        status_ = "キャンセル";
        return;
    }

    for (ShortcutSlot& slot : shortcutSlots_) {
        if (!slot.assigned) {
            slot = grabbedSlot_;
            grabbedSlot_ = ShortcutSlot{};
            grabbedSlotOrigin_ = -1;
            grabbedSlotActive_ = false;
            status_ = "キャンセル";
            return;
        }
    }

    ShortcutSlot& selected = selectedShortcutSlot();
    std::swap(selected, grabbedSlot_);
    grabbedSlot_ = ShortcutSlot{};
    grabbedSlotOrigin_ = -1;
    grabbedSlotActive_ = false;
    status_ = "キャンセル";
}

bool InventorySystem::useItemType(InventoryItemType type, Player& player, SpellRingSystem& spellRing)
{
    InventoryStack& selected = stack(type);
    if (selected.count <= 0) {
        status_ = "アイテムなし";
        return false;
    }

    switch (type) {
    case InventoryItemType::Dirt:
        if (player.hp >= player.maxHp) {
            status_ = "体力は満タン";
            return false;
        }
        player.hp = std::min(player.maxHp, player.hp + 1);
        status_ = "体力 +1";
        break;
    case InventoryItemType::Stone:
        spellRing.upgradeShovelPower(1);
        status_ = "シャベル +1";
        break;
    case InventoryItemType::Ore:
        spellRing.upgradeItemDamage(1);
        status_ = "ダメージ +1";
        break;
    case InventoryItemType::Count:
        return false;
    }

    --selected.count;
    return true;
}

bool InventorySystem::addItemTypeToRing(InventoryItemType type, SpellRingSystem& spellRing)
{
    InventoryStack& selected = stack(type);
    if (selected.count <= 0) {
        status_ = "アイテムなし";
        return false;
    }
    if (!canAddToRing(type)) {
        status_ = "リング不可";
        return false;
    }
    if (!spellRing.addItem(ringTypeFor(type))) {
        status_ = "リング満杯";
        return false;
    }
    --selected.count;
    status_ = "リングに追加";
    return true;
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
        const int objectIndex = index - ItemCount;
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
    const int instanceIndex = index - ItemCount - static_cast<int>(objectStacks_.size());
    objectInstances_.erase(objectInstances_.begin() + instanceIndex);
    return true;
}

bool InventorySystem::useObjectSelection(Player& player, const EffectDispatcher& effectDispatcher)
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
        const int objectIndex = index - ItemCount;
        objectStacks_.erase(objectStacks_.begin() + objectIndex);
    }
    return true;
}

bool InventorySystem::useObjectInstanceSelection(Player& player, const EffectDispatcher& effectDispatcher)
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
    const EffectDispatcher& effectDispatcher)
{
    if (selectedObjectStack() != nullptr) {
        return useObjectSelection(player, effectDispatcher);
    }
    if (selectedObjectInstance() != nullptr) {
        return useObjectInstanceSelection(player, effectDispatcher);
    }

    const ShortcutSlot& slot = selectedShortcutSlot();
    if (!slot.assigned) {
        status_ = "ショートカット空き";
        return false;
    }
    return useItemType(slot.type, player, spellRing);
}

bool InventorySystem::addShortcutSelectionToRing(SpellRingSystem& spellRing)
{
    if (selectedObjectStack() != nullptr) {
        return addObjectSelectionToRing(spellRing);
    }
    if (selectedObjectInstance() != nullptr) {
        return addObjectInstanceSelectionToRing(spellRing);
    }

    const ShortcutSlot& slot = selectedShortcutSlot();
    if (!slot.assigned) {
        status_ = "ショートカット空き";
        return false;
    }
    return addItemTypeToRing(slot.type, spellRing);
}

bool InventorySystem::useSelected(Player& player, SpellRingSystem& spellRing)
{
    return useItemType(selectedType(), player, spellRing);
}

bool InventorySystem::addSelectedToRing(SpellRingSystem& spellRing)
{
    return addItemTypeToRing(selectedType(), spellRing);
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
    const EffectDispatcher& effectDispatcher)
{
    if (input.shortcutCursorDelta() != 0) {
        moveShortcutCursor(input.shortcutCursorDelta());
    }

    selectShortcutSlot(input.shortcutSlotPressed());

    if (input.toggleShortcutRowPressed()) {
        toggleShortcutRow();
    }

    if (input.useItemPressed()) {
        useShortcutSelection(player, spellRing, effectDispatcher);
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
    const EffectDispatcher& effectDispatcher)
{
    if (!open_) {
        return;
    }

    if (input.pausePressed()) {
        if (grabbedSlotActive_) {
            cancelGrab();
        } else {
            open_ = false;
        }
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

    for (int i = 0; i < ShortcutSlotCount; ++i) {
        if (ui.pressed(inventorySlotRect(i))) {
            selectShortcutIndex(i);
            return;
        }
    }

    if (input.grabOrPlacePressed()) {
        grabOrPlaceSelected();
    }
    if (input.useItemPressed() || input.confirmPressed()) {
        if (grabbedSlotActive_) {
            placeGrabbedAtSelected();
        } else {
            useShortcutSelection(player, spellRing, effectDispatcher);
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
    bool blocked)
{
    if (!blocked && ui.pressed(inventoryToggleRect())) {
        open_ = !open_;
        return;
    }
    if (input.inventoryPressed() && !blocked) {
        open_ = !open_;
    }
    if (!open_ || blocked) {
        return;
    }
    updateScreen(input, ui, player, spellRing, effectDispatcher);
}

void InventorySystem::render(Renderer& renderer, const Player& player, const SpellRingSystem& spellRing) const
{
    (void)player;
    (void)spellRing;

    renderer.setScreenSpace();
    drawUiButton(renderer, inventoryToggleRect(), "アイテム", false);

    if (!open_) {
        return;
    }

    drawUiWindow(
        renderer,
        {{ScreenX, ScreenY}, {ScreenW, ScreenH}},
        "アイテム",
        "WASD/矢印 移動  Q/E 左右  F/Enter 使用  R リングへ  P 保護  G つかむ/置く  Esc 戻る");

    char buffer[160];
    for (int i = 0; i < ShortcutSlotCount; ++i) {
        const ShortcutSlot& slot = shortcutSlots_[i];
        const UiRect rect = inventorySlotRect(i);
        const bool selected = i == selectedShortcutIndex();
        const Color fill = selected ? Color{54, 44, 72, 242} : Color{20, 20, 28, 226};
        const Color outline = selected ? ui::WindowBorder : Color{78, 72, 94, 255};
        renderer.fillRect(rect.pos, rect.size, fill);
        renderer.drawRect(rect.pos, rect.size, outline);

        std::snprintf(buffer, sizeof(buffer), "%d", i % ShortcutColumns + 1);
        renderer.drawText(rect.pos + Vec2{7.0f, 6.0f}, buffer, selected ? ui::Text : ui::TextDisabled, 1);

        if (slot.assigned) {
            const InventoryStack& item = stack(slot.type);
            if (renderer.hasIconSheet()) {
                renderer.drawIcon(inventoryItemIcon(slot.type), rect.pos + Vec2{28.0f, 10.0f}, {32.0f, 32.0f});
            } else {
                renderer.fillCircle(rect.pos + Vec2{44.0f, 28.0f}, 13.0f, inventoryItemColor(slot.type));
            }
            std::snprintf(buffer, sizeof(buffer), "%s x%d", inventoryItemName(slot.type), item.count);
            renderer.drawText(rect.pos + Vec2{10.0f, 50.0f}, buffer, ui::Text, 2);
        } else {
            const InventoryObjectStack* objectStack = objectStackAtScreenIndex(i);
            if (objectStack != nullptr) {
                const Color objectColor = inventoryObjectColor(objectStack->item);
                renderer.fillCircle(rect.pos + Vec2{44.0f, 28.0f}, 13.0f, objectColor);
                renderer.drawCircle(rect.pos + Vec2{44.0f, 28.0f}, 16.0f, {255, 230, 150, 180});
                std::snprintf(buffer, sizeof(buffer), "%s x%d", objectStack->item.name.c_str(), objectStack->count);
                renderer.drawText(rect.pos + Vec2{10.0f, 50.0f}, buffer, ui::Text, 2);
            } else if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(i)) {
                const Color objectColor = objectInstance->instance.isBroken ? Color{82, 82, 90, 255} : inventoryObjectColor(objectInstance->item);
                renderer.fillCircle(rect.pos + Vec2{44.0f, 28.0f}, 13.0f, objectColor);
                renderer.drawCircle(
                    rect.pos + Vec2{44.0f, 28.0f},
                    16.0f,
                    objectInstance->instance.protectionEnabled ? Color{116, 220, 255, 220} : Color{255, 230, 150, 180});
                std::snprintf(buffer, sizeof(buffer), "%s", objectInstance->item.name.c_str());
                renderer.drawText(rect.pos + Vec2{10.0f, 50.0f}, buffer, ui::Text, 2);
            } else {
                renderer.drawText(rect.pos + Vec2{22.0f, 30.0f}, "空き", ui::TextDisabled, 2);
            }
        }
    }

    renderer.fillRect({DetailX, DetailY}, {DetailW, DetailH}, ui::WindowFill);
    renderer.drawRect({DetailX, DetailY}, {DetailW, DetailH}, ui::WindowBorder);
    renderer.drawText({DetailX + 20.0f, DetailY + 18.0f}, "詳細", ui::Text, 3);

    const ShortcutSlot& selectedSlot = selectedShortcutSlot();
    float detailLineY = DetailY + 68.0f;
    if (selectedSlot.assigned) {
        const InventoryStack& selectedStack = stack(selectedSlot.type);
        std::snprintf(buffer, sizeof(buffer), "%s x%d", inventoryItemName(selectedSlot.type), selectedStack.count);
        drawDetailLine(renderer, detailLineY, "名前", buffer);
        drawDetailLine(renderer, detailLineY, "カテゴリ", inventoryItemCategory(selectedSlot.type));
        drawDetailLine(renderer, detailLineY, "説明文", inventoryItemDescription(selectedSlot.type));
        drawDetailLine(renderer, detailLineY, "通常効果", inventoryNormalEffect(selectedSlot.type));
        drawDetailLine(renderer, detailLineY, "軌道効果", inventoryOrbitEffect(selectedSlot.type));
        std::snprintf(buffer, sizeof(buffer), "%d", inventoryAttackPower(selectedSlot.type));
        drawDetailLine(renderer, detailLineY, "攻撃力", buffer);
        drawDetailLine(renderer, detailLineY, "ダメージ", inventoryDamageType(selectedSlot.type));
        std::snprintf(buffer, sizeof(buffer), "%d", inventoryDigPower(selectedSlot.type));
        drawDetailLine(renderer, detailLineY, "掘削力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d", inventoryDurability(selectedSlot.type));
        drawDetailLine(renderer, detailLineY, "耐久力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", inventoryWeightKg(selectedSlot.type));
        drawDetailLine(renderer, detailLineY, "重さ", buffer);
        drawDetailLine(renderer, detailLineY, "特殊タグ", inventoryTags(selectedSlot.type));
        drawDetailLine(renderer, detailLineY, "効果テキスト", inventoryItemUseText(selectedSlot.type));
    } else if (const InventoryObjectStack* objectStack = selectedObjectStack()) {
        const ItemData& item = objectStack->item;
        std::snprintf(buffer, sizeof(buffer), "%s x%d", item.name.c_str(), objectStack->count);
        drawDetailLine(renderer, detailLineY, "名前", buffer);
        drawDetailLine(renderer, detailLineY, "カテゴリ", item.category);
        drawDetailLine(renderer, detailLineY, "説明文", item.description.empty() ? "-" : item.description);
        drawDetailLine(renderer, detailLineY, "通常効果", effectSummary(item.normalEffects));
        drawDetailLine(renderer, detailLineY, "軌道効果", effectSummary(item.orbitEffects));
        std::snprintf(buffer, sizeof(buffer), "%d", item.attackPower);
        drawDetailLine(renderer, detailLineY, "攻撃力", buffer);
        drawDetailLine(renderer, detailLineY, "ダメージ", item.damageType.empty() ? "-" : item.damageType);
        std::snprintf(buffer, sizeof(buffer), "%d", item.digPower);
        drawDetailLine(renderer, detailLineY, "掘削力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d", item.durability);
        drawDetailLine(renderer, detailLineY, "耐久力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", item.weightKg);
        drawDetailLine(renderer, detailLineY, "重さ", buffer);
        drawDetailLine(renderer, detailLineY, "特殊タグ", joinValues(item.tags));
        drawDetailLine(renderer, detailLineY, "効果テキスト", item.effectText.empty() ? "-" : item.effectText);
    } else if (const InventoryObjectInstance* objectInstance = selectedObjectInstance()) {
        const ItemData& item = objectInstance->item;
        const ItemInstance& instance = objectInstance->instance;
        std::snprintf(buffer, sizeof(buffer), "%s", item.name.c_str());
        drawDetailLine(renderer, detailLineY, "名前", buffer);
        drawDetailLine(renderer, detailLineY, "カテゴリ", item.category);
        std::snprintf(buffer, sizeof(buffer), "%s", instance.instanceId.c_str());
        drawDetailLine(renderer, detailLineY, "個体ID", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d", instance.enhanceLevel);
        drawDetailLine(renderer, detailLineY, "強化Lv", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d/%d", instance.currentDurability, instance.maxDurability);
        drawDetailLine(renderer, detailLineY, "耐久力", buffer);
        drawDetailLine(renderer, detailLineY, "保護", instance.protectionEnabled ? "ON" : "OFF");
        drawDetailLine(renderer, detailLineY, "状態", instance.isBroken ? "破損" : "通常");
        std::snprintf(buffer, sizeof(buffer), "+%d / +%d / +%d", instance.attackBonus, instance.digBonus, instance.durabilityBonus);
        drawDetailLine(renderer, detailLineY, "補正", buffer);
        drawDetailLine(renderer, detailLineY, "通常効果", effectSummary(item.normalEffects));
        drawDetailLine(renderer, detailLineY, "追加効果", effectSummary(instance.addedEffects));
        drawDetailLine(renderer, detailLineY, "特殊タグ", joinValues(item.tags));
        drawDetailLine(renderer, detailLineY, "操作", "P 保護ON/OFF");
    } else {
        drawDetailLine(renderer, detailLineY, "名前", "空き");
        drawDetailLine(renderer, detailLineY, "カテゴリ", "-");
        drawDetailLine(renderer, detailLineY, "説明文", "アイテム未配置");
        drawDetailLine(renderer, detailLineY, "通常効果", "-");
        drawDetailLine(renderer, detailLineY, "軌道効果", "-");
        drawDetailLine(renderer, detailLineY, "攻撃力", "-");
        drawDetailLine(renderer, detailLineY, "ダメージ", "-");
        drawDetailLine(renderer, detailLineY, "掘削力", "-");
        drawDetailLine(renderer, detailLineY, "耐久力", "-");
        drawDetailLine(renderer, detailLineY, "重さ", "-");
        drawDetailLine(renderer, detailLineY, "特殊タグ", "-");
        drawDetailLine(renderer, detailLineY, "効果テキスト", "-");
    }

    if (grabbedSlotActive_) {
        std::snprintf(buffer, sizeof(buffer), "つかみ中: %s  移動先で G または F / Escでキャンセル",
            grabbedSlot_.assigned ? inventoryItemName(grabbedSlot_.type) : "空き");
        renderer.fillRect({ScreenGridX, ScreenY + ScreenH - 52.0f}, {720.0f, 32.0f}, ui::WindowFillStrong);
        renderer.drawText({ScreenGridX + 12.0f, ScreenY + ScreenH - 44.0f}, buffer, ui::Text, 2);
    } else {
        renderer.drawText({ScreenGridX, ScreenY + ScreenH - 42.0f}, status_, ui::Text, 2);
    }
}

void InventorySystem::renderShortcutHud(Renderer& renderer, const SpellRingSystem& spellRing, int screenWidth, int screenHeight) const
{
    renderer.setScreenSpace();

    const float panelW = std::min(1040.0f, std::max(760.0f, static_cast<float>(screenWidth) - HudMargin * 2.0f));
    const float panelX = (static_cast<float>(screenWidth) - panelW) * 0.5f;
    const float panelY = static_cast<float>(screenHeight) - HudHeight - 12.0f;
    const float innerX = panelX + 16.0f;
    const float slotY = panelY + 42.0f;
    const float slotW = (panelW - 32.0f - HudSlotGap * static_cast<float>(ShortcutColumns - 1)) / static_cast<float>(ShortcutColumns);

    renderer.fillRect({panelX, panelY}, {panelW, HudHeight}, ui::WindowFill);
    renderer.drawRect({panelX, panelY}, {panelW, HudHeight}, ui::WindowBorder);

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "Row %d/3   Ring %d", shortcutRow_ + 1, spellRing.activeRingIndex() + 1);
    renderer.drawText({innerX, panelY + 12.0f}, buffer, ui::Text, 2);
    renderer.drawText({innerX + 300.0f, panelY + 12.0f}, "Q/E 選択, Tab 行切替, F 使用, R リングへ, P 保護", ui::TextMuted, 2);

    for (int column = 0; column < ShortcutColumns; ++column) {
        const int slotIndex = shortcutRow_ * ShortcutColumns + column;
        const ShortcutSlot& slot = shortcutSlots_[slotIndex];
        const bool selected = column == selectedShortcutColumn_;
        const Vec2 slotPos{innerX + static_cast<float>(column) * (slotW + HudSlotGap), slotY};
        const Color fill = selected ? Color{54, 44, 72, 242} : Color{20, 20, 28, 226};
        const Color outline = selected ? ui::WindowBorder : Color{78, 72, 94, 255};
        renderer.fillRect(slotPos, {slotW, HudSlotHeight}, fill);
        renderer.drawRect(slotPos, {slotW, HudSlotHeight}, outline);

        std::snprintf(buffer, sizeof(buffer), "%d", column + 1);
        renderer.drawText(slotPos + Vec2{7.0f, 6.0f}, buffer, selected ? ui::Text : ui::TextDisabled, 1);

        if (slot.assigned) {
            const InventoryStack& item = stack(slot.type);
            std::snprintf(buffer, sizeof(buffer), "%s x%d", inventoryItemName(slot.type), item.count);
            renderer.drawText(slotPos + Vec2{20.0f, 18.0f}, buffer, ui::Text, 2);
        } else {
            const InventoryObjectStack* objectStack = objectStackAtScreenIndex(slotIndex);
            if (objectStack != nullptr) {
                std::snprintf(buffer, sizeof(buffer), "%s x%d", objectStack->item.name.c_str(), objectStack->count);
                renderer.drawText(slotPos + Vec2{20.0f, 18.0f}, buffer, ui::Text, 2);
            } else if (const InventoryObjectInstance* objectInstance = objectInstanceAtScreenIndex(slotIndex)) {
                std::snprintf(buffer, sizeof(buffer), "%s%s",
                    objectInstance->instance.protectionEnabled ? "[P] " : "",
                    objectInstance->item.name.c_str());
                renderer.drawText(
                    slotPos + Vec2{20.0f, 18.0f},
                    buffer,
                    objectInstance->instance.isBroken ? ui::TextDisabled : ui::Text,
                    2);
            } else {
                renderer.drawText(slotPos + Vec2{20.0f, 18.0f}, "空き", ui::TextDisabled, 2);
            }
        }

        if (selected) {
            renderer.fillRect({slotPos.x + 8.0f, slotPos.y + HudSlotHeight - 5.0f}, {slotW - 16.0f, 3.0f}, ui::Text);
            renderer.drawText({slotPos.x + slotW * 0.5f - 4.0f, slotPos.y + HudSlotHeight + 5.0f}, "v", ui::Text, 2);
        }
    }

}

}
