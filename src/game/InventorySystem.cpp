#include "game/InventorySystem.hpp"

#include <algorithm>
#include <cstdio>

namespace majo {

namespace {
constexpr int MaxPlayerHp = 10;
constexpr float PanelX = 790.0f;
constexpr float PanelY = 70.0f;
constexpr float PanelW = 430.0f;
constexpr float PanelH = 440.0f;

bool canAddToRing(InventoryItemType type)
{
    return type == InventoryItemType::Stone || type == InventoryItemType::Ore;
}

SpellRingItemType ringTypeFor(InventoryItemType type)
{
    return type == InventoryItemType::Ore ? SpellRingItemType::Ore : SpellRingItemType::Stone;
}
}

const char* inventoryItemName(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "DIRT";
    case InventoryItemType::Stone: return "STONE";
    case InventoryItemType::Ore: return "ORE";
    case InventoryItemType::Count: break;
    }
    return "ITEM";
}

const char* inventoryItemUseText(InventoryItemType type)
{
    switch (type) {
    case InventoryItemType::Dirt: return "USE HEAL 1";
    case InventoryItemType::Stone: return "USE SHOVEL +1";
    case InventoryItemType::Ore: return "USE DAMAGE +1";
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

InventoryItemType InventorySystem::selectedType() const
{
    return stacks_[selected_].type;
}

void InventorySystem::addFromDugTile(TileType type)
{
    switch (type) {
    case TileType::Dirt:
        ++stack(InventoryItemType::Dirt).count;
        status_ = "GOT DIRT";
        break;
    case TileType::Rock:
        ++stack(InventoryItemType::Stone).count;
        status_ = "GOT STONE";
        break;
    case TileType::Ore:
        ++stack(InventoryItemType::Ore).count;
        status_ = "GOT ORE";
        break;
    case TileType::Empty:
        break;
    }
}

void InventorySystem::moveSelection(int delta)
{
    selected_ += delta;
    if (selected_ < 0) {
        selected_ = ItemCount - 1;
    } else if (selected_ >= ItemCount) {
        selected_ = 0;
    }
}

bool InventorySystem::useSelected(Player& player, SpellRingSystem& spellRing)
{
    InventoryStack& selected = stacks_[selected_];
    if (selected.count <= 0) {
        status_ = "NO ITEM";
        return false;
    }

    switch (selected.type) {
    case InventoryItemType::Dirt:
        if (player.hp >= MaxPlayerHp) {
            status_ = "HP FULL";
            return false;
        }
        player.hp = std::min(MaxPlayerHp, player.hp + 1);
        status_ = "HP +1";
        break;
    case InventoryItemType::Stone:
        spellRing.upgradeShovelPower(1);
        status_ = "SHOVEL +1";
        break;
    case InventoryItemType::Ore:
        spellRing.upgradeItemDamage(1);
        status_ = "DAMAGE +1";
        break;
    case InventoryItemType::Count:
        return false;
    }

    --selected.count;
    return true;
}

bool InventorySystem::addSelectedToRing(SpellRingSystem& spellRing)
{
    InventoryStack& selected = stacks_[selected_];
    if (selected.count <= 0) {
        status_ = "NO ITEM";
        return false;
    }
    if (!canAddToRing(selected.type)) {
        status_ = "NOT FOR RING";
        return false;
    }
    if (!spellRing.addItem(ringTypeFor(selected.type))) {
        status_ = "RING FULL";
        return false;
    }
    --selected.count;
    status_ = "ADDED RING";
    return true;
}

void InventorySystem::update(const Input& input, Player& player, SpellRingSystem& spellRing, bool blocked)
{
    if (input.inventoryPressed() && !blocked) {
        open_ = !open_;
    }
    if (!open_ || blocked) {
        return;
    }
    if (input.menuUpPressed()) {
        moveSelection(-1);
    }
    if (input.menuDownPressed()) {
        moveSelection(1);
    }
    if (input.useItemPressed()) {
        useSelected(player, spellRing);
    }
    if (input.addRingPressed()) {
        addSelectedToRing(spellRing);
    }
}

void InventorySystem::render(Renderer& renderer, const Player& player, const SpellRingSystem& spellRing) const
{
    if (!open_) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect({PanelX, PanelY}, {PanelW, PanelH}, {10, 10, 16, 235});
    renderer.drawRect({PanelX, PanelY}, {PanelW, PanelH}, {210, 184, 255, 255});
    renderer.drawText({PanelX + 26.0f, PanelY + 24.0f}, "ITEMS", {246, 235, 255, 255}, 4);

    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "HP %02d/%02d   SPELL RING %02d/08", player.hp, MaxPlayerHp, static_cast<int>(spellRing.items().size()));
    renderer.drawText({PanelX + 28.0f, PanelY + 78.0f}, buffer, {190, 190, 198, 255}, 2);

    for (int i = 0; i < ItemCount; ++i) {
        const InventoryStack& item = stacks_[i];
        const float y = PanelY + 122.0f + static_cast<float>(i) * 74.0f;
        const bool selected = i == selected_;
        renderer.fillRect({PanelX + 24.0f, y}, {PanelW - 48.0f, 58.0f}, selected ? Color{42, 34, 62, 245} : Color{22, 22, 30, 220});
        renderer.drawRect({PanelX + 24.0f, y}, {PanelW - 48.0f, 58.0f}, selected ? Color{255, 230, 150, 255} : Color{80, 74, 96, 255});

        const Vec2 iconPos{PanelX + 42.0f, y + 13.0f};
        if (renderer.hasIconSheet()) {
            renderer.drawIcon(inventoryItemIcon(item.type), iconPos, {32.0f, 32.0f});
        } else {
            renderer.fillCircle(iconPos + Vec2{16.0f, 16.0f}, 13.0f, inventoryItemColor(item.type));
        }

        std::snprintf(buffer, sizeof(buffer), "%s X%03d", inventoryItemName(item.type), item.count);
        renderer.drawText({PanelX + 88.0f, y + 10.0f}, buffer, {235, 235, 240, 255}, 2);
        renderer.drawText({PanelX + 88.0f, y + 34.0f}, inventoryItemUseText(item.type), {168, 172, 184, 255}, 2);
        if (canAddToRing(item.type)) {
            renderer.drawText({PanelX + 278.0f, y + 34.0f}, "E RING", {190, 214, 255, 255}, 2);
        }
    }

    renderer.drawText({PanelX + 28.0f, PanelY + 362.0f}, "UP DOWN  ENTER USE  E RING", {198, 198, 206, 255}, 2);
    renderer.drawText({PanelX + 28.0f, PanelY + 392.0f}, status_, {255, 235, 150, 255}, 2);
}

}
