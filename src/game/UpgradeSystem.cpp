#include "game/UpgradeSystem.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

namespace majo {

namespace {

UiRect panelRect()
{
    return {{160.0f, 150.0f}, {960.0f, 390.0f}};
}

UiRect optionRect(int index)
{
    constexpr float CardWidth = 270.0f;
    constexpr float CardHeight = 218.0f;
    constexpr float CardGap = 32.0f;
    constexpr float CardY = 260.0f;
    const UiRect panel = panelRect();
    const float totalWidth = CardWidth * 3.0f + CardGap * 2.0f;
    const float startX = panel.pos.x + (panel.size.x - totalWidth) * 0.5f;
    return {{startX + static_cast<float>(index) * (CardWidth + CardGap), CardY}, {CardWidth, CardHeight}};
}

const char* upgradeName(int option)
{
    switch (option) {
    case 0: return "リング拡張";
    case 1: return "加速刻印";
    case 2: return "重量拡張";
    default: return "";
    }
}

const char* upgradeDescription(int option)
{
    switch (option) {
    case 0: return "リングの半径が広がる";
    case 1: return "リングの回転速度が上がる";
    case 2: return "全リングの重量上限が上がる";
    default: return "";
    }
}

std::string upgradeCurrentValueText(int option, const SpellRingSystem& spellRing)
{
    char buffer[64];
    switch (option) {
    case 0:
        std::snprintf(buffer, sizeof(buffer), "半径 %.0f", spellRing.radius());
        break;
    case 1:
        std::snprintf(buffer, sizeof(buffer), "速度 %.2f", spellRing.angularSpeed());
        break;
    case 2:
        std::snprintf(buffer, sizeof(buffer), "重量 %.1fkg", spellRing.maxEquippedWeight());
        break;
    default:
        buffer[0] = '\0';
        break;
    }
    return buffer;
}

std::string upgradeNextValueText(int option, const SpellRingSystem& spellRing)
{
    char buffer[64];
    switch (option) {
    case 0:
        std::snprintf(buffer, sizeof(buffer), "%.0f", spellRing.radius() * 1.1f);
        break;
    case 1:
        std::snprintf(buffer, sizeof(buffer), "%.2f", spellRing.angularSpeed() * 1.1f);
        break;
    case 2:
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", spellRing.maxEquippedWeight() + SpellRingSystem::LevelWeightLimitUpgradeAmount);
        break;
    default:
        buffer[0] = '\0';
        break;
    }
    return buffer;
}

int upgradeStageForOption(int option, int levelRingRadiusPoints, int levelRingSpeedPoints, int levelRingWeightLimitPoints)
{
    switch (option) {
    case 0:
        return std::max(0, levelRingRadiusPoints);
    case 1:
        return std::max(0, levelRingSpeedPoints);
    case 2:
        return std::max(0, levelRingWeightLimitPoints);
    default:
        return 0;
    }
}

UpgradeChoice upgradeChoiceForOption(int option)
{
    switch (option) {
    case 0:
        return UpgradeChoice::Radius;
    case 1:
        return UpgradeChoice::Speed;
    case 2:
    default:
        return UpgradeChoice::WeightLimit;
    }
}

std::optional<UpgradeChoice> applyUpgrade(
    int option,
    LevelSystem& level,
    SpellRingSystem& spellRing,
    int& levelRingRadiusPoints,
    int& levelRingSpeedPoints,
    int& levelRingWeightLimitPoints)
{
    if (option == 0) {
        spellRing.upgradeRadius(1.1f);
        ++levelRingRadiusPoints;
    } else if (option == 1) {
        spellRing.upgradeSpeed(1.1f);
        ++levelRingSpeedPoints;
    } else {
        spellRing.upgradeMaxEquippedWeightForAllRings(SpellRingSystem::LevelWeightLimitUpgradeAmount);
        ++levelRingWeightLimitPoints;
    }
    level.finishChoice();
    return upgradeChoiceForOption(option);
}

void drawCenteredText(Renderer& renderer, UiRect rect, float y, std::string_view text, Color color, int scale)
{
    const Vec2 size = renderer.measureText(text, scale);
    renderer.drawText({rect.pos.x + (rect.size.x - size.x) * 0.5f, y}, text, color, scale);
}

void drawLevelUpSubtitle(Renderer& renderer, UiRect panel)
{
    const UiRect header = uiHeaderRect(panel);
    const Vec2 titlePadding = renderer.hasUiWindowTexture()
        ? ui::ImageWindowHeaderTitlePadding
        : ui::HeaderTitlePadding;
    renderer.drawText(header.pos + titlePadding + Vec2{0.0f, 34.0f}, "リングの強化を選ぼう", ui::TextMuted, 2);
}

void drawUpgradeValueLine(
    Renderer& renderer,
    UiRect rect,
    float y,
    std::string_view currentText,
    std::string_view nextText,
    Color baseColor,
    int scale)
{
    constexpr Color UpgradeValueColor{255, 230, 150, 255};
    const std::string arrow = " → ";
    const float currentWidth = renderer.measureText(currentText, scale).x;
    const float arrowWidth = renderer.measureText(arrow, scale).x;
    const float nextWidth = renderer.measureText(nextText, scale).x;
    Vec2 pos{rect.pos.x + (rect.size.x - currentWidth - arrowWidth - nextWidth) * 0.5f, y};
    renderer.drawText(pos, currentText, baseColor, scale);
    pos.x += currentWidth;
    renderer.drawText(pos, arrow, ui::TextMuted, scale);
    pos.x += arrowWidth;
    renderer.drawText(pos, nextText, UpgradeValueColor, scale);
}

}

std::optional<UpgradeChoice> UpgradeSystem::update(
    const Input& input,
    UiContext& ui,
    LevelSystem& level,
    SpellRingSystem& spellRing,
    int& levelRingRadiusPoints,
    int& levelRingSpeedPoints,
    int& levelRingWeightLimitPoints)
{
    if (!level.isChoosing()) {
        return std::nullopt;
    }

    for (int i = 0; i < 3; ++i) {
        const UiRect rect = optionRect(i);
        if (rect.contains(ui.mouse())) {
            selectedOption_ = i;
        }
        if (ui.pressed(rect)) {
            selectedOption_ = i;
            return applyUpgrade(i, level, spellRing, levelRingRadiusPoints, levelRingSpeedPoints, levelRingWeightLimitPoints);
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
        return applyUpgrade(0, level, spellRing, levelRingRadiusPoints, levelRingSpeedPoints, levelRingWeightLimitPoints);
    } else if (input.upgradePressed(1)) {
        selectedOption_ = 1;
        return applyUpgrade(1, level, spellRing, levelRingRadiusPoints, levelRingSpeedPoints, levelRingWeightLimitPoints);
    } else if (input.upgradePressed(2)) {
        selectedOption_ = 2;
        return applyUpgrade(2, level, spellRing, levelRingRadiusPoints, levelRingSpeedPoints, levelRingWeightLimitPoints);
    } else if (input.useItemPressed() || input.confirmPressed()) {
        return applyUpgrade(selectedOption_, level, spellRing, levelRingRadiusPoints, levelRingSpeedPoints, levelRingWeightLimitPoints);
    }
    return std::nullopt;
}

void UpgradeSystem::render(
    Renderer& renderer,
    const LevelSystem& level,
    const SpellRingSystem& spellRing,
    int levelRingRadiusPoints,
    int levelRingSpeedPoints,
    int levelRingWeightLimitPoints)
{
    if (!level.isChoosing()) {
        return;
    }
    renderer.setScreenSpace();
    const UiRect panel = panelRect();
    UiWindowScope levelUpWindow(renderer, "level_up", panel, "レベルアップ", "Q/E で選択   F/Enter で決定");
    drawLevelUpSubtitle(renderer, panel);

    for (int i = 0; i < 3; ++i) {
        const UiRect card = optionRect(i);
        const bool selected = i == selectedOption_;
        renderer.fillRect(card.pos, card.size, selected ? Color{54, 46, 76, 245} : Color{22, 22, 32, 232});
        renderer.drawRect(card.pos, card.size, selected ? ui::WindowBorder : Color{104, 94, 128, 255});
        drawCenteredText(renderer, card, card.pos.y + 40.0f, upgradeName(i), ui::Text, 3);
        drawCenteredText(renderer, card, card.pos.y + 96.0f, upgradeDescription(i), ui::Text, 2);
        drawUpgradeValueLine(
            renderer,
            card,
            card.pos.y + 146.0f,
            upgradeCurrentValueText(i, spellRing),
            upgradeNextValueText(i, spellRing),
            selected ? ui::Text : ui::TextMuted,
            2);
        const int currentStage = upgradeStageForOption(i, levelRingRadiusPoints, levelRingSpeedPoints, levelRingWeightLimitPoints);
        drawCenteredText(
            renderer,
            card,
            card.pos.y + 178.0f,
            "強化回数：" + std::to_string(currentStage),
            selected ? ui::Text : ui::TextMuted,
            2);
    }
}

}
