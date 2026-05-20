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

UiRect ringTabRect(int index)
{
    constexpr float TabWidth = 164.0f;
    constexpr float TabHeight = 38.0f;
    constexpr float TabGap = 18.0f;
    constexpr float TabY = 208.0f;
    const UiRect panel = panelRect();
    const float totalWidth = TabWidth * static_cast<float>(SpellRingCount) + TabGap * static_cast<float>(SpellRingCount - 1);
    const float startX = panel.pos.x + (panel.size.x - totalWidth) * 0.5f;
    return {{startX + static_cast<float>(index) * (TabWidth + TabGap), TabY}, {TabWidth, TabHeight}};
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
    case 2: return "リングの重量上限が上がる";
    default: return "";
    }
}

RingLevelUpgradeKind upgradeKindForOption(int option)
{
    switch (option) {
    case 0:
        return RingLevelUpgradeKind::Radius;
    case 1:
        return RingLevelUpgradeKind::Speed;
    case 2:
    default:
        return RingLevelUpgradeKind::WeightLimit;
    }
}

std::string upgradeCurrentValueText(int option, const SpellRingSystem& spellRing, int ringIndex)
{
    char buffer[64];
    switch (option) {
    case 0:
        std::snprintf(buffer, sizeof(buffer), "半径 %.0f", spellRing.radiusForRing(ringIndex));
        break;
    case 1:
        std::snprintf(buffer, sizeof(buffer), "速度 %.2f", spellRing.angularSpeedForRing(ringIndex));
        break;
    case 2:
        std::snprintf(buffer, sizeof(buffer), "重量 %.1fkg", spellRing.maxEquippedWeightForRing(ringIndex));
        break;
    default:
        buffer[0] = '\0';
        break;
    }
    return buffer;
}

float nextLevelScaleFactor(int currentPoints)
{
    const float currentMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(currentPoints);
    const float nextMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(currentPoints + 1);
    return nextMultiplier / std::max(0.0001f, currentMultiplier);
}

std::string upgradeNextValueText(
    int option,
    const SpellRingSystem& spellRing,
    int ringIndex,
    const RingLevelUpgradePoints& points)
{
    char buffer[64];
    switch (option) {
    case 0:
        std::snprintf(buffer, sizeof(buffer), "%.0f", spellRing.radiusForRing(ringIndex) * nextLevelScaleFactor(points.radius));
        break;
    case 1:
        std::snprintf(buffer, sizeof(buffer), "%.2f", spellRing.angularSpeedForRing(ringIndex) * nextLevelScaleFactor(points.speed));
        break;
    case 2:
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", spellRing.maxEquippedWeightForRing(ringIndex) + SpellRingSystem::LevelWeightLimitUpgradeAmount);
        break;
    default:
        buffer[0] = '\0';
        break;
    }
    return buffer;
}

int upgradeStageForOption(int option, const RingLevelUpgradePoints& points)
{
    return std::max(0, ringLevelUpgradePoint(points, upgradeKindForOption(option)));
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

std::optional<RingLevelUpgradeSelection> UpgradeSystem::update(
    const Input& input,
    UiContext& ui,
    SpellRingSystem& spellRing)
{
    if (!ringSelectionInitialized_) {
        selectedRingIndex_ = std::clamp(spellRing.activeRingIndex(), 0, SpellRingCount - 1);
        ringSelectionInitialized_ = true;
    }
    selectedRingIndex_ = std::clamp(selectedRingIndex_, 0, SpellRingCount - 1);
    selectedOption_ = std::clamp(selectedOption_, 0, 2);

    const auto chooseUpgrade = [&](int option) {
        ui.emitSound(UiSoundEvent::UpgradeSelect);
        return RingLevelUpgradeSelection{selectedRingIndex_, upgradeKindForOption(option)};
    };

    for (int i = 0; i < SpellRingCount; ++i) {
        const UiRect rect = ringTabRect(i);
        if (ui.pressed(rect)) {
            if (selectedRingIndex_ != i) {
                ui.emitSound(UiSoundEvent::TabSwitch);
            }
            selectedRingIndex_ = i;
            ui.block(panelRect());
            return std::nullopt;
        }
    }

    if (input.activeRingDelta() != 0) {
        selectedRingIndex_ = (selectedRingIndex_ + input.activeRingDelta()) % SpellRingCount;
        if (selectedRingIndex_ < 0) {
            selectedRingIndex_ += SpellRingCount;
        }
        ui.emitSound(UiSoundEvent::TabSwitch);
    }

    for (int i = 0; i < 3; ++i) {
        const UiRect rect = optionRect(i);
        if (rect.contains(ui.mouse())) {
            selectedOption_ = i;
        }
        if (ui.pressed(rect)) {
            selectedOption_ = i;
            return chooseUpgrade(i);
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
        return chooseUpgrade(0);
    } else if (input.upgradePressed(1)) {
        selectedOption_ = 1;
        return chooseUpgrade(1);
    } else if (input.upgradePressed(2)) {
        selectedOption_ = 2;
        return chooseUpgrade(2);
    } else if (input.useItemPressed() || input.confirmPressed()) {
        return chooseUpgrade(selectedOption_);
    }
    return std::nullopt;
}

void UpgradeSystem::render(
    Renderer& renderer,
    const LevelSystem& level,
    const SpellRingSystem& spellRing,
    const RingLevelUpgradePointTable& levelRingUpgradePoints)
{
    if (!level.isChoosing()) {
        ringSelectionInitialized_ = false;
        return;
    }
    renderer.setScreenSpace();
    const UiRect panel = panelRect();
    if (!ringSelectionInitialized_) {
        selectedRingIndex_ = std::clamp(spellRing.activeRingIndex(), 0, SpellRingCount - 1);
        ringSelectionInitialized_ = true;
    }
    const int ringIndex = std::clamp(selectedRingIndex_, 0, SpellRingCount - 1);
    const RingLevelUpgradePoints& points = levelRingUpgradePoints[static_cast<std::size_t>(ringIndex)];
    UiWindowScope levelUpWindow(renderer, "level_up", panel, "レベルアップ", "Z/X リング   Q/E で選択   F/Enter で決定");
    drawLevelUpSubtitle(renderer, panel);

    for (int i = 0; i < SpellRingCount; ++i) {
        const UiRect tab = ringTabRect(i);
        const bool selected = i == ringIndex;
        renderer.fillRect(tab.pos, tab.size, selected ? Color{54, 46, 76, 245} : Color{22, 22, 32, 232});
        renderer.drawRect(tab.pos, tab.size, selected ? ui::WindowBorder : Color{104, 94, 128, 255});
        drawCenteredText(renderer, tab, tab.pos.y + 10.0f, "リング" + std::to_string(i + 1), selected ? ui::Text : ui::TextMuted, 2);
    }

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
            upgradeCurrentValueText(i, spellRing, ringIndex),
            upgradeNextValueText(i, spellRing, ringIndex, points),
            selected ? ui::Text : ui::TextMuted,
            2);
        const int currentStage = upgradeStageForOption(i, points);
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
