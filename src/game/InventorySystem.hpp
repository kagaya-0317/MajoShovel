#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Ui.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"
#include <array>
#include <string>

namespace majo {

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

class InventorySystem {
public:
    void addFromDugTile(TileType type);
    void updateShortcuts(const Input& input, Player& player, SpellRingSystem& spellRing);
    void updateScreen(const Input& input, UiContext& ui, Player& player, SpellRingSystem& spellRing);
    void update(const Input& input, UiContext& ui, Player& player, SpellRingSystem& spellRing, bool blocked);
    void render(Renderer& renderer, const Player& player, const SpellRingSystem& spellRing) const;
    void renderShortcutHud(Renderer& renderer, const SpellRingSystem& spellRing, int screenWidth, int screenHeight) const;
    bool isOpen() const { return open_; }
    void setOpen(bool open) { open_ = open; }
    void cancelGrab();

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
    bool useShortcutSelection(Player& player, SpellRingSystem& spellRing);
    bool addShortcutSelectionToRing(SpellRingSystem& spellRing);
    bool useSelected(Player& player, SpellRingSystem& spellRing);
    bool addSelectedToRing(SpellRingSystem& spellRing);

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
