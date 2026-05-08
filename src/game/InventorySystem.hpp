#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Ui.hpp"
#include "data/ObjectCatalog.hpp"
#include "game/ItemModel.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

class EffectDispatcher;

enum class InventoryItemType {
    Dirt,
    Stone,
    Ore,
    Count
};

struct InventoryStack {
    InventoryItemType type = InventoryItemType::Dirt;
    int count = 0;
};

struct ShortcutSlot {
    bool assigned = false;
    InventoryItemType type = InventoryItemType::Dirt;
};

struct InventoryObjectStack : StackItem {
    ItemData item;

    InventoryObjectStack() = default;
    InventoryObjectStack(const ItemData& itemData, int itemCount)
        : StackItem{itemData.id, itemCount}
        , item(itemData)
    {
    }
};

struct InventoryObjectInstance {
    ItemData item;
    ItemInstance instance;
};

class InventorySystem {
public:
    void addFromDugTile(TileType type);
    bool addObjectItem(const ObjectCatalog& catalog, std::string_view objectId);
    bool addRuntimeObjectItem(const ItemData& item);
    void updateShortcuts(const Input& input, Player& player, SpellRingSystem& spellRing, const EffectDispatcher& effectDispatcher);
    void updateScreen(const Input& input, UiContext& ui, Player& player, SpellRingSystem& spellRing, const EffectDispatcher& effectDispatcher);
    void update(const Input& input, UiContext& ui, Player& player, SpellRingSystem& spellRing, const EffectDispatcher& effectDispatcher, bool blocked);
    void render(Renderer& renderer, const Player& player, const SpellRingSystem& spellRing, const ObjectCatalog& catalog) const;
    void renderShortcutHud(Renderer& renderer, const SpellRingSystem& spellRing, int screenWidth, int screenHeight) const;
    bool isOpen() const { return open_; }
    void setOpen(bool open) { open_ = open; }
    void cancelGrab();
    int itemCount(InventoryItemType type) const;
    void setItemCount(InventoryItemType type, int count);
    const std::vector<InventoryObjectStack>& objectStacks() const { return objectStacks_; }
    std::vector<StackItem> stackItemsForSave() const;
    const std::vector<InventoryObjectInstance>& objectInstances() const { return objectInstances_; }
    const MaterialInventory& materials() const { return materials_; }
    MaterialInventory& materials() { return materials_; }
    void clearObjectStacks();
    bool setObjectItemCount(const ObjectCatalog& catalog, std::string_view objectId, int count);
    bool addObjectInstance(const ObjectCatalog& catalog, ItemInstance instance);
    bool removeObjectItemCount(std::string_view objectId, int count);
    bool removeObjectInstance(std::string_view instanceId);
    bool takeObjectInstance(std::string_view instanceId, InventoryObjectInstance& outInstance);
    bool repairObjectInstance(std::string_view instanceId);
    bool enhanceObjectInstance(std::string_view instanceId, int attackBonus, int digBonus, int durabilityBonus, int maxEnhanceLevel);
    bool enhanceObjectStackItem(std::string_view objectId, int attackBonus, int digBonus, int durabilityBonus, int maxEnhanceLevel);
    bool enhanceSelectedObjectInstance(int attackBonus, int digBonus, int durabilityBonus);
    void addMaterial(MaterialType type, int count);
    void setMaterialCount(MaterialType type, int count);
    int materialCount(MaterialType type) const;

private:
    static constexpr int ItemCount = static_cast<int>(InventoryItemType::Count);
    static constexpr int ShortcutColumns = 8;
    static constexpr int ShortcutRows = 3;
    static constexpr int ShortcutSlotCount = ShortcutColumns * ShortcutRows;

    InventoryStack& stack(InventoryItemType type);
    const InventoryStack& stack(InventoryItemType type) const;
    InventoryItemType selectedType() const;
    const ShortcutSlot& selectedShortcutSlot() const;
    ShortcutSlot& selectedShortcutSlot();
    const InventoryObjectStack* objectStackAtScreenIndex(int index) const;
    InventoryObjectStack* objectStackAtScreenIndex(int index);
    const InventoryObjectInstance* objectInstanceAtScreenIndex(int index) const;
    InventoryObjectInstance* objectInstanceAtScreenIndex(int index);
    const InventoryObjectStack* selectedObjectStack() const;
    InventoryObjectInstance* selectedObjectInstance();
    const InventoryObjectInstance* selectedObjectInstance() const;
    int selectedShortcutIndex() const;
    void moveShortcutCursor(int delta);
    void moveShortcutCursorGrid(int dx, int dy);
    void selectShortcutSlot(int slot);
    void selectShortcutIndex(int index);
    void toggleShortcutRow();
    void grabOrPlaceSelected();
    void placeGrabbedAtSelected();
    bool useItemType(InventoryItemType type, Player& player, SpellRingSystem& spellRing);
    bool addItemTypeToRing(InventoryItemType type, SpellRingSystem& spellRing);
    bool addObjectSelectionToRing(SpellRingSystem& spellRing);
    bool addObjectInstanceSelectionToRing(SpellRingSystem& spellRing);
    bool useObjectSelection(Player& player, const EffectDispatcher& effectDispatcher);
    bool useObjectInstanceSelection(Player& player, const EffectDispatcher& effectDispatcher);
    bool useShortcutSelection(Player& player, SpellRingSystem& spellRing, const EffectDispatcher& effectDispatcher);
    bool addShortcutSelectionToRing(SpellRingSystem& spellRing);
    bool useSelected(Player& player, SpellRingSystem& spellRing);
    bool addSelectedToRing(SpellRingSystem& spellRing);
    std::string allocateInstanceId();
    InventoryObjectInstance createObjectInstance(const ItemData& item);
    void toggleSelectedProtection();

    std::array<InventoryStack, ItemCount> stacks_{{
        {InventoryItemType::Dirt, 0},
        {InventoryItemType::Stone, 0},
        {InventoryItemType::Ore, 0},
    }};
    std::array<ShortcutSlot, ShortcutSlotCount> shortcutSlots_{{
        {true, InventoryItemType::Dirt},
        {true, InventoryItemType::Stone},
        {true, InventoryItemType::Ore},
    }};
    std::vector<InventoryObjectStack> objectStacks_;
    std::vector<InventoryObjectInstance> objectInstances_;
    MaterialInventory materials_;
    unsigned long long nextInstanceId_ = 1;
    int selected_ = 0;
    int shortcutRow_ = 0;
    int selectedShortcutColumn_ = 0;
    bool grabbedSlotActive_ = false;
    ShortcutSlot grabbedSlot_{};
    int grabbedSlotOrigin_ = -1;
    bool open_ = false;
    std::string status_ = "アイテムなし";
};

const char* inventoryItemName(InventoryItemType type);
const char* inventoryItemUseText(InventoryItemType type);
int inventoryItemIcon(InventoryItemType type);
Color inventoryItemColor(InventoryItemType type);

}
