#include "game/UpgradeSystem.hpp"

#include <string>

namespace majo {

namespace {

UiRect panelRect()
{
    return {{130.0f, 150.0f}, {1020.0f, 390.0f}};
}

UiRect optionRect(int index)
{
    return {{178.0f + static_cast<float>(index) * 318.0f, 260.0f}, {286.0f, 218.0f}};
}

const char* upgradeName(int option)
{
    switch (option) {
    case 0: return "リング拡張";
    case 1: return "加速刻印";
    case 2: return "シャベル強化";
    default: return "";
    }
}

const char* upgradeDescription(int option)
{
    switch (option) {
    case 0: return "リングの半径が広がる";
    case 1: return "リングの回転速度が上がる";
    case 2: return "攻撃と掘削が強くなる";
    default: return "";
    }
}

const char* upgradeValueText(int option)
{
    switch (option) {
    case 0: return "半径 x1.10";
    case 1: return "速度 x1.10";
    case 2: return "攻撃力 +1 / 掘削力 +1";
    default: return "";
    }
}

void applyUpgrade(int option, LevelSystem& level, SpellRingSystem& spellRing)
{
    if (option == 0) {
        spellRing.upgradeRadius(1.1f);
    } else if (option == 1) {
        spellRing.upgradeSpeed(1.1f);
    } else {
        spellRing.upgradeShovelPower(1);
    }
    level.finishChoice();
}

}

void UpgradeSystem::update(const Input& input, UiContext& ui, LevelSystem& level, SpellRingSystem& spellRing)
{
    if (!level.isChoosing()) {
        return;
    }

    for (int i = 0; i < 3; ++i) {
        if (ui.pressed(optionRect(i))) {
            selectedOption_ = i;
            applyUpgrade(i, level, spellRing);
            return;
        }
    }
    ui.block(panelRect());

    int move = 0;
    if (input.pressed(InputAction::MoveLeft) || input.shortcutCursorDelta() < 0) {
        --move;
    }
    if (input.pressed(InputAction::MoveRight) || input.shortcutCursorDelta() > 0) {
        ++move;
    }
    if (move != 0) {
        selectedOption_ = (selectedOption_ + move + 3) % 3;
    }

    if (input.upgradePressed(0)) {
        selectedOption_ = 0;
        applyUpgrade(0, level, spellRing);
    } else if (input.upgradePressed(1)) {
        selectedOption_ = 1;
        applyUpgrade(1, level, spellRing);
    } else if (input.upgradePressed(2)) {
        selectedOption_ = 2;
        applyUpgrade(2, level, spellRing);
    } else if (input.useItemPressed() || input.confirmPressed()) {
        applyUpgrade(selectedOption_, level, spellRing);
    }
}

void UpgradeSystem::render(Renderer& renderer, const LevelSystem& level)
{
    if (!level.isChoosing()) {
        return;
    }
    renderer.setScreenSpace();
    const UiRect panel = panelRect();
    renderer.fillRect(panel.pos, panel.size, {12, 10, 18, 238});
    renderer.drawRect(panel.pos, panel.size, {210, 184, 255, 255});
    renderer.drawText(panel.pos + Vec2{354.0f, 36.0f}, "レベルアップ", {246, 235, 255, 255}, 4);
    renderer.drawText(panel.pos + Vec2{238.0f, 88.0f}, "A/D・左右・Q/E で選択   F/Enter で決定   1/2/3 で直接決定", {202, 206, 216, 255}, 2);

    for (int i = 0; i < 3; ++i) {
        const UiRect card = optionRect(i);
        const bool selected = i == selectedOption_;
        renderer.fillRect(card.pos, card.size, selected ? Color{54, 46, 76, 245} : Color{22, 22, 32, 232});
        renderer.drawRect(card.pos, card.size, selected ? Color{255, 230, 150, 255} : Color{104, 94, 128, 255});
        renderer.drawText(card.pos + Vec2{18.0f, 18.0f}, std::to_string(i + 1), {174, 182, 198, 255}, 2);
        renderer.drawText(card.pos + Vec2{50.0f, 40.0f}, upgradeName(i), {246, 235, 255, 255}, 3);
        renderer.drawText(card.pos + Vec2{24.0f, 96.0f}, upgradeDescription(i), {226, 228, 236, 255}, 2);
        renderer.drawText(card.pos + Vec2{24.0f, 146.0f}, upgradeValueText(i), selected ? Color{255, 230, 150, 255} : Color{190, 210, 255, 255}, 2);
        if (selected) {
            renderer.drawText(card.pos + Vec2{88.0f, 182.0f}, "選択中", {255, 230, 150, 255}, 2);
        }
    }
}

}
