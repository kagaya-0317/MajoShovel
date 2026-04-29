#include "game/InventorySystem.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

namespace majo {

namespace {
constexpr int MaxPlayerHp = 10;
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
    renderer.drawText({DetailX + 20.0f, y}, label, {168, 172, 184, 255}, 2);
    renderer.drawText({DetailX + 126.0f, y}, value, {235, 235, 240, 255}, 2);
    y += 31.0f;
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
        if (player.hp >= MaxPlayerHp) {
            status_ = "体力は満タン";
            return false;
        }
        player.hp = std::min(MaxPlayerHp, player.hp + 1);
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

bool InventorySystem::useShortcutSelection(Player& player, SpellRingSystem& spellRing)
{
    const ShortcutSlot& slot = selectedShortcutSlot();
    if (!slot.assigned) {
        status_ = "ショートカット空き";
        return false;
    }
    return useItemType(slot.type, player, spellRing);
}

bool InventorySystem::addShortcutSelectionToRing(SpellRingSystem& spellRing)
{
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

void InventorySystem::updateShortcuts(const Input& input, Player& player, SpellRingSystem& spellRing)
{
    if (input.shortcutCursorDelta() != 0) {
        moveShortcutCursor(input.shortcutCursorDelta());
    }

    selectShortcutSlot(input.shortcutSlotPressed());

    if (input.toggleShortcutRowPressed()) {
        toggleShortcutRow();
    }

    if (input.useItemPressed()) {
        useShortcutSelection(player, spellRing);
    }
    if (input.addRingPressed()) {
        addShortcutSelectionToRing(spellRing);
    }
}

void InventorySystem::updateScreen(const Input& input, UiContext& ui, Player& player, SpellRingSystem& spellRing)
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
            useShortcutSelection(player, spellRing);
        }
    }
    if (input.addRingPressed()) {
        if (grabbedSlotActive_) {
            status_ = "つかみ中は配置してください";
        } else {
            addShortcutSelectionToRing(spellRing);
        }
    }

    ui.block(inventoryScreenRect());
}

void InventorySystem::update(const Input& input, UiContext& ui, Player& player, SpellRingSystem& spellRing, bool blocked)
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
    updateScreen(input, ui, player, spellRing);
}

void InventorySystem::render(Renderer& renderer, const Player& player, const SpellRingSystem& spellRing) const
{
    renderer.setScreenSpace();
    drawUiButton(renderer, inventoryToggleRect(), "アイテム", false);

    if (!open_) {
        return;
    }

    renderer.fillRect({ScreenX, ScreenY}, {ScreenW, ScreenH}, {8, 8, 14, 238});
    renderer.drawRect({ScreenX, ScreenY}, {ScreenW, ScreenH}, {210, 184, 255, 255});
    renderer.drawText({ScreenX + 28.0f, ScreenY + 24.0f}, "アイテム", {246, 235, 255, 255}, 4);
    renderer.drawText({ScreenX + 190.0f, ScreenY + 34.0f}, "WASD/矢印 移動  Q/E 左右  F/Enter 使用  R リングへ  G つかむ/置く  Esc 戻る", {198, 198, 206, 255}, 2);

    char buffer[160];
    for (int i = 0; i < ShortcutSlotCount; ++i) {
        const ShortcutSlot& slot = shortcutSlots_[i];
        const UiRect rect = inventorySlotRect(i);
        const bool selected = i == selectedShortcutIndex();
        const Color fill = selected ? Color{54, 44, 72, 242} : Color{20, 20, 28, 226};
        const Color outline = selected ? Color{255, 230, 150, 255} : Color{78, 72, 94, 255};
        renderer.fillRect(rect.pos, rect.size, fill);
        renderer.drawRect(rect.pos, rect.size, outline);

        std::snprintf(buffer, sizeof(buffer), "%d", i % ShortcutColumns + 1);
        renderer.drawText(rect.pos + Vec2{7.0f, 6.0f}, buffer, selected ? Color{255, 230, 150, 255} : Color{150, 150, 160, 255}, 1);

        if (slot.assigned) {
            const InventoryStack& item = stack(slot.type);
            if (renderer.hasIconSheet()) {
                renderer.drawIcon(inventoryItemIcon(slot.type), rect.pos + Vec2{28.0f, 10.0f}, {32.0f, 32.0f});
            } else {
                renderer.fillCircle(rect.pos + Vec2{44.0f, 28.0f}, 13.0f, inventoryItemColor(slot.type));
            }
            std::snprintf(buffer, sizeof(buffer), "%s x%d", inventoryItemName(slot.type), item.count);
            renderer.drawText(rect.pos + Vec2{10.0f, 50.0f}, buffer, {235, 235, 240, 255}, 2);
        } else {
            renderer.drawText(rect.pos + Vec2{22.0f, 30.0f}, "空き", {126, 128, 140, 255}, 2);
        }
    }

    renderer.fillRect({DetailX, DetailY}, {DetailW, DetailH}, {14, 14, 22, 230});
    renderer.drawRect({DetailX, DetailY}, {DetailW, DetailH}, {88, 82, 108, 255});
    renderer.drawText({DetailX + 20.0f, DetailY + 18.0f}, "詳細", {246, 235, 255, 255}, 3);

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
        renderer.fillRect({ScreenGridX, ScreenY + ScreenH - 52.0f}, {720.0f, 32.0f}, {36, 28, 48, 245});
        renderer.drawText({ScreenGridX + 12.0f, ScreenY + ScreenH - 44.0f}, buffer, {255, 230, 150, 255}, 2);
    } else {
        renderer.drawText({ScreenGridX, ScreenY + ScreenH - 42.0f}, status_, {255, 235, 150, 255}, 2);
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

    renderer.fillRect({panelX, panelY}, {panelW, HudHeight}, {8, 8, 14, 218});
    renderer.drawRect({panelX, panelY}, {panelW, HudHeight}, {88, 82, 108, 255});

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "Row %d/3   Ring %d", shortcutRow_ + 1, spellRing.activeRingIndex() + 1);
    renderer.drawText({innerX, panelY + 12.0f}, buffer, {246, 235, 255, 255}, 2);
    renderer.drawText({innerX + 300.0f, panelY + 12.0f}, "Q/E 選択, Tab 行切替, F 使用, R リングへ", {198, 198, 206, 255}, 2);

    for (int column = 0; column < ShortcutColumns; ++column) {
        const int slotIndex = shortcutRow_ * ShortcutColumns + column;
        const ShortcutSlot& slot = shortcutSlots_[slotIndex];
        const bool selected = column == selectedShortcutColumn_;
        const Vec2 slotPos{innerX + static_cast<float>(column) * (slotW + HudSlotGap), slotY};
        const Color fill = selected ? Color{54, 44, 72, 242} : Color{20, 20, 28, 226};
        const Color outline = selected ? Color{255, 230, 150, 255} : Color{78, 72, 94, 255};
        renderer.fillRect(slotPos, {slotW, HudSlotHeight}, fill);
        renderer.drawRect(slotPos, {slotW, HudSlotHeight}, outline);

        std::snprintf(buffer, sizeof(buffer), "%d", column + 1);
        renderer.drawText(slotPos + Vec2{7.0f, 6.0f}, buffer, selected ? Color{255, 230, 150, 255} : Color{150, 150, 160, 255}, 1);

        if (slot.assigned) {
            const InventoryStack& item = stack(slot.type);
            std::snprintf(buffer, sizeof(buffer), "%s x%d", inventoryItemName(slot.type), item.count);
            renderer.drawText(slotPos + Vec2{20.0f, 18.0f}, buffer, {235, 235, 240, 255}, 2);
        } else {
            renderer.drawText(slotPos + Vec2{20.0f, 18.0f}, "空き", {126, 128, 140, 255}, 2);
        }

        if (selected) {
            renderer.fillRect({slotPos.x + 8.0f, slotPos.y + HudSlotHeight - 5.0f}, {slotW - 16.0f, 3.0f}, {255, 230, 150, 255});
            renderer.drawText({slotPos.x + slotW * 0.5f - 4.0f, slotPos.y + HudSlotHeight + 5.0f}, "v", {255, 230, 150, 255}, 2);
        }
    }

}

}
