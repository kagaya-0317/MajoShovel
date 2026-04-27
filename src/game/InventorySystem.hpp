#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
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

class InventorySystem {
public:
    void addFromDugTile(TileType type);
    void update(const Input& input, Player& player, SpellRingSystem& spellRing, bool blocked);
    void render(Renderer& renderer, const Player& player, const SpellRingSystem& spellRing) const;
    bool isOpen() const { return open_; }

private:
    static constexpr int ItemCount = static_cast<int>(InventoryItemType::Count);

    InventoryStack& stack(InventoryItemType type);
    const InventoryStack& stack(InventoryItemType type) const;
    InventoryItemType selectedType() const;
    void moveSelection(int delta);
    bool useSelected(Player& player, SpellRingSystem& spellRing);
    bool addSelectedToRing(SpellRingSystem& spellRing);

    std::array<InventoryStack, ItemCount> stacks_{{
        {InventoryItemType::Dirt, 0},
        {InventoryItemType::Stone, 0},
        {InventoryItemType::Ore, 0},
    }};
    int selected_ = 0;
    bool open_ = false;
    std::string status_ = "NO ITEM";
};

const char* inventoryItemName(InventoryItemType type);
const char* inventoryItemUseText(InventoryItemType type);
int inventoryItemIcon(InventoryItemType type);
Color inventoryItemColor(InventoryItemType type);

}
