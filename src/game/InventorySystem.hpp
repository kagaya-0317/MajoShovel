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
class EncyclopediaSystem;
struct EffectDiscoveryEvent;

struct ShortcutSlot {
    bool assigned = false;
};

struct RingEquipFxRequest {
    Vec2 sourceScreen{};
    int ringIndex = 0;
    int itemIndex = -1;
    float localAngle = 0.0f;
    std::string objectId;
    std::string instanceId;
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
    bool addObjectItem(const ObjectCatalog& catalog, std::string_view objectId);
    bool addRuntimeObjectItem(const ItemData& item);
    void updateShortcuts(
        const Input& input,
        UiContext& ui,
        Player& player,
        SpellRingSystem& spellRing,
        const EffectDispatcher& effectDispatcher,
        int screenWidth,
        int screenHeight,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    void updateScreen(
        const Input& input,
        UiContext& ui,
        Player& player,
        SpellRingSystem& spellRing,
        const EffectDispatcher& effectDispatcher,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    void update(
        const Input& input,
        UiContext& ui,
        Player& player,
        SpellRingSystem& spellRing,
        const EffectDispatcher& effectDispatcher,
        bool blocked,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    void render(
        Renderer& renderer,
        const Player& player,
        const SpellRingSystem& spellRing,
        const ObjectCatalog& catalog,
        const EncyclopediaSystem& encyclopedia) const;
    void renderShortcutHud(Renderer& renderer, const SpellRingSystem& spellRing, int screenWidth, int screenHeight) const;
    bool isOpen() const { return open_; }
    void setOpen(bool open) { open_ = open; }
    void cancelGrab();
    const std::vector<InventoryObjectStack>& objectStacks() const { return objectStacks_; }
    std::vector<StackItem> stackItemsForSave() const;
    const std::vector<InventoryObjectInstance>& objectInstances() const { return objectInstances_; }
    const MaterialInventory& materials() const { return materials_; }
    MaterialInventory& materials() { return materials_; }
    std::vector<RingEquipFxRequest> consumeRingEquipFxRequests();
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
    int screenSlotCount() const { return ShortcutSlotCount; }
    const InventoryObjectStack* screenObjectStackAt(int index) const { return objectStackAtScreenIndex(index); }
    const InventoryObjectInstance* screenObjectInstanceAt(int index) const { return objectInstanceAtScreenIndex(index); }
    bool hasScreenItemAt(int index) const { return hasScreenItem(index); }
    bool moveObjectStackToScreenSlot(std::string_view objectId, int slotIndex);
    bool moveObjectInstanceToScreenSlot(std::string_view instanceId, int slotIndex);

private:
    static constexpr int ShortcutColumns = 8;
    static constexpr int ShortcutRows = 3;
    static constexpr int ShortcutSlotCount = ShortcutColumns * ShortcutRows;

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
    bool addObjectSelectionToRing(SpellRingSystem& spellRing, SpellRingAddResult* outResult = nullptr);
    bool addObjectInstanceSelectionToRing(SpellRingSystem& spellRing, SpellRingAddResult* outResult = nullptr);
    bool useObjectSelection(
        Player& player,
        const EffectDispatcher& effectDispatcher,
        std::vector<EffectDiscoveryEvent>* discoveryEvents,
        const EncyclopediaSystem* encyclopedia);
    bool useObjectInstanceSelection(
        Player& player,
        const EffectDispatcher& effectDispatcher,
        std::vector<EffectDiscoveryEvent>* discoveryEvents,
        const EncyclopediaSystem* encyclopedia);
    bool useShortcutSelection(
        Player& player,
        SpellRingSystem& spellRing,
        const EffectDispatcher& effectDispatcher,
        std::vector<EffectDiscoveryEvent>* discoveryEvents,
        const EncyclopediaSystem* encyclopedia);
    bool addShortcutSelectionToRing(SpellRingSystem& spellRing, SpellRingAddResult* outResult = nullptr);
    bool canUseScreenItem(int index) const;
    std::array<UiCommandMenuItem, 3> buildSlotCommandItems(int slotIndex) const;
    bool hasScreenItem(int index) const;
    bool moveScreenItem(int fromIndex, int toIndex);
    void syncPackedItemSlots() const;
    int packedItemIndexAtScreenIndex(int index) const;
    int stackIndexAtScreenIndex(int index) const;
    int instanceIndexAtScreenIndex(int index) const;
    void removePackedSlotAtPackedIndex(int packedIndex) const;
    std::string allocateInstanceId();
    InventoryObjectInstance createObjectInstance(const ItemData& item);
    void toggleSelectedProtection();
    void openSlotCommandMenu(int slotIndex);
    void resetSlotPointerPress();
    void queueRingEquipFx(Vec2 sourceScreen, const SpellRingAddResult& result);

    std::array<ShortcutSlot, ShortcutSlotCount> shortcutSlots_{};
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
    UiCommandMenuState slotCommandMenu_{};
    int slotCommandMenuIndex_ = -1;
    int slotPointerPressIndex_ = -1;
    Vec2 slotPointerPressMouse_{};
    bool slotPointerPressCanOpenMenu_ = false;
    bool slotPointerDragTriggered_ = false;
    mutable std::vector<int> packedItemSlots_;
    std::vector<RingEquipFxRequest> ringEquipFxRequests_;
    mutable UiCancelControlState cancelState_{};
    bool open_ = false;
    std::string status_ = "アイテムなし";
};

}
