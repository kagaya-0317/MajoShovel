#include "game/UpgradeSystem.hpp"

#include "data/GameBalance.hpp"
#include "game/RingDisplayName.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace majo {

namespace {

UiRect panelRect()
{
    return {{192.0f, 70.0f}, {896.0f, 524.0f}};
}

constexpr Vec2 LevelUpCardSize{310.0f, 278.0f};
constexpr float LevelUpCardGap = -38.0f;
constexpr float LevelUpCardContentOffsetY = 20.0f;

int clampedUnlockedRingCount(int unlockedRingCount)
{
    return std::clamp(unlockedRingCount, 1, SpellRingCount);
}

float worldDistanceToMeters(float distance)
{
    return distance / static_cast<float>(balance::TileSize);
}

float linearMetersPerSecondForAngularSpeed(float angularSpeed, float radius)
{
    return angularSpeed * worldDistanceToMeters(radius);
}

float ringPromptY(int unlockedRingCount)
{
    return clampedUnlockedRingCount(unlockedRingCount) <= 1 ? 208.0f : 244.0f;
}

UiRect optionRect(int index, int unlockedRingCount)
{
    constexpr float PromptCardGap = 4.0f;
    const UiRect panel = panelRect();
    const float totalWidth = LevelUpCardSize.x * 3.0f + LevelUpCardGap * 2.0f;
    const float startX = panel.pos.x + (panel.size.x - totalWidth) * 0.5f;
    return {{
        startX + static_cast<float>(index) * (LevelUpCardSize.x + LevelUpCardGap),
        ringPromptY(unlockedRingCount) + PromptCardGap,
    }, LevelUpCardSize};
}

UiRect ringTabRect(int index, int unlockedRingCount)
{
    constexpr float TabWidth = 210.0f;
    constexpr float TabGap = 14.0f;
    constexpr float TabY = 178.0f;
    const UiRect panel = panelRect();
    const int tabCount = clampedUnlockedRingCount(unlockedRingCount);
    const float totalWidth = TabWidth * static_cast<float>(tabCount) + TabGap * static_cast<float>(std::max(0, tabCount - 1));
    const float startX = panel.pos.x + (panel.size.x - totalWidth) * 0.5f;
    return {{startX + static_cast<float>(index) * (TabWidth + TabGap), TabY}, {TabWidth, ui::ButtonHeight}};
}

UiRect okButtonRect(int unlockedRingCount)
{
    constexpr Vec2 Size{180.0f, ui::ButtonHeight};
    constexpr float CardGap = -8.0f;
    const UiRect panel = panelRect();
    const UiRect card = optionRect(0, unlockedRingCount);
    return {{
        panel.pos.x + (panel.size.x - Size.x) * 0.5f,
        card.pos.y + card.size.y + CardGap,
    }, Size};
}

const char* upgradeName(int option)
{
    switch (option) {
    case 0: return "リング拡張";
    case 1: return "リング加速";
    case 2: return "重量拡張";
    default: return "";
    }
}

const char* upgradeDescription(int option)
{
    switch (option) {
    case 0: return "リングの半径が広がるよ";
    case 1: return "リングのアイテムの回転速度が上がるよ";
    case 2: return "リングのアイテムの重量上限が上がるよ";
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

std::string upgradeCurrentValueText(int option, const SpellRingSystem& spellRing, int ringIndex, const RuntimeBalance& balance)
{
    char buffer[64];
    switch (option) {
    case 0:
        std::snprintf(buffer, sizeof(buffer), "半径 %.2fm", worldDistanceToMeters(spellRing.orbitRadiusForRing(ringIndex)));
        break;
    case 1:
        std::snprintf(
            buffer,
            sizeof(buffer),
            "速度 %.2fm/s",
            linearMetersPerSecondForAngularSpeed(
                spellRing.ringAngularSpeedForIndex(ringIndex, balance),
                spellRing.orbitRadiusForRing(ringIndex)));
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
    const RingLevelUpgradePoints& points,
    const RuntimeBalance& balance)
{
    char buffer[64];
    switch (option) {
    case 0:
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%.2fm",
            worldDistanceToMeters(spellRing.orbitRadiusForRing(ringIndex) * nextLevelScaleFactor(points.radius)));
        break;
    case 1:
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%.2fm/s",
            linearMetersPerSecondForAngularSpeed(
                spellRing.ringAngularSpeedForIndex(ringIndex, balance) * nextLevelScaleFactor(points.speed),
                spellRing.orbitRadiusForRing(ringIndex)));
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
    renderer.drawText(header.pos + titlePadding + Vec2{0.0f, 42.0f}, "強化するリングを選ぼう", ui::TextMuted, 2);
}

void drawUpgradePrompt(Renderer& renderer, UiRect panel, int unlockedRingCount)
{
    const Vec2 titlePadding = renderer.hasUiWindowTexture()
        ? ui::ImageWindowHeaderTitlePadding
        : ui::HeaderTitlePadding;
    renderer.drawText(
        {panel.pos.x + titlePadding.x, ringPromptY(unlockedRingCount)},
        "リングの強化内容を選ぼう",
        ui::TextMuted,
        2);
}

std::string levelUpHelpText(int unlockedRingCount, bool ringSelected)
{
    if (clampedUnlockedRingCount(unlockedRingCount) <= 1) {
        return "←/→ カード選択  F/Enter OK";
    }
    if (!ringSelected) {
        return "Z/X リング選択  F/Enter 決定";
    }
    return "Z/X リング選択  ←/→ カード選択  F/Enter OK";
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
    SpellRingSystem& spellRing,
    float dt,
    int unlockedRingCount)
{
    const int ringCount = clampedUnlockedRingCount(unlockedRingCount);
    if (!ringSelectionInitialized_) {
        const int activeRing = std::clamp(spellRing.activeRingIndex(), 0, ringCount - 1);
        selectedRingIndex_ = ringCount <= 1 ? 0 : -1;
        selectedOption_ = 0;
        cardFade_ = ringCount <= 1 ? 1.0f : 0.0f;
        ringTabs_.focusedIndex = activeRing;
        ringSelectionInitialized_ = true;
    }
    if (ringCount <= 1) {
        selectedRingIndex_ = 0;
    } else if (selectedRingIndex_ >= ringCount) {
        selectedRingIndex_ = -1;
    }
    selectedOption_ = std::clamp(selectedOption_, 0, 2);
    const bool ringSelected = selectedRingIndex_ >= 0;
    cardFade_ = ringSelected
        ? clamp(cardFade_ + std::max(0.0f, dt) * 6.0f, 0.0f, 1.0f)
        : 0.0f;

    const auto chooseUpgrade = [&](int option) -> std::optional<RingLevelUpgradeSelection> {
        if (selectedRingIndex_ < 0) {
            return std::nullopt;
        }
        ui.emitSound(UiSoundEvent::UpgradeSelect);
        return RingLevelUpgradeSelection{selectedRingIndex_, upgradeKindForOption(option)};
    };

    if (ringCount > 1) {
        std::array<UiTabItem, SpellRingCount> ringTabs{};
        std::array<UiRect, SpellRingCount> ringTabRects{};
        std::array<std::string, SpellRingCount> ringTabLabels{};
        for (int i = 0; i < ringCount; ++i) {
            ringTabLabels[static_cast<std::size_t>(i)] = ringDisplayName(i, ringCount);
            ringTabs[static_cast<std::size_t>(i)] = {
                ringTabLabels[static_cast<std::size_t>(i)],
                true,
                ringDisplayIconImageNumber(i),
            };
            ringTabRects[static_cast<std::size_t>(i)] = ringTabRect(i, ringCount);
        }

        UiTabsInput tabsInput = makeUiCycleTabsInput(input, ringCount);
        const int directRingFocus = input.shortcutSlotPressed();
        if (!ringSelected && directRingFocus >= 0 && directRingFocus < ringCount) {
            selectedRingIndex_ = directRingFocus;
            ringTabs_.focusedIndex = directRingFocus;
            ui.emitSound(UiSoundEvent::TabSwitch);
            ui.block(panelRect());
            return std::nullopt;
        }
        tabsInput.commit = tabsInput.commit ||
            (!ringSelected &&
                !ui.navigationActive() &&
                (input.confirmPressed() || input.useItemPressed()));

        const int ringSelection = updateUiTabs(
            ringTabs_,
            ui,
            tabsInput,
            selectedRingIndex_,
            ringTabs.data(),
            ringCount,
            ringTabRects.data());
        if (ringSelection >= 0) {
            selectedRingIndex_ = ringSelection;
            ui.block(panelRect());
            return std::nullopt;
        }
    }

    if (selectedRingIndex_ < 0) {
        ui.block(panelRect());
        return std::nullopt;
    }

    for (int i = 0; i < 3; ++i) {
        const UiRect rect = optionRect(i, ringCount);
        if (ui.navigationFocused(rect)) {
            selectedOption_ = i;
        }
        if (ui.pressed(rect)) {
            if (selectedOption_ != i) {
                ui.emitSound(UiSoundEvent::TabSwitch);
            }
            selectedOption_ = i;
        }
    }

    int move = 0;
    if (!ui.navigationActive() && input.pressed(InputAction::MoveLeft)) {
        --move;
    }
    if (!ui.navigationActive() && input.pressed(InputAction::MoveRight)) {
        ++move;
    }
    if (move != 0) {
        selectedOption_ = (selectedOption_ + move + 3) % 3;
        ui.emitSound(UiSoundEvent::TabSwitch);
    }

    if (input.upgradePressed(0)) {
        selectedOption_ = 0;
    } else if (input.upgradePressed(1)) {
        selectedOption_ = 1;
    } else if (input.upgradePressed(2)) {
        selectedOption_ = 2;
    }

    if (ui.pressed(okButtonRect(ringCount)) || input.useItemPressed() || input.confirmPressed()) {
        ui.block(panelRect());
        return chooseUpgrade(selectedOption_);
    }

    ui.block(panelRect());
    return std::nullopt;
}

void UpgradeSystem::render(
    Renderer& renderer,
    const LevelSystem& level,
    const SpellRingSystem& spellRing,
    const RingLevelUpgradePointTable& levelRingUpgradePoints,
    const RuntimeBalance& balance,
    int unlockedRingCount)
{
    if (!level.isChoosing()) {
        selectedRingIndex_ = -1;
        cardFade_ = 0.0f;
        ringTabs_ = {};
        ringSelectionInitialized_ = false;
        return;
    }
    renderer.setScreenSpace();
    const UiRect panel = panelRect();
    const int ringCount = clampedUnlockedRingCount(unlockedRingCount);
    if (!ringSelectionInitialized_) {
        const int activeRing = std::clamp(spellRing.activeRingIndex(), 0, ringCount - 1);
        selectedRingIndex_ = ringCount <= 1 ? 0 : -1;
        selectedOption_ = 0;
        cardFade_ = ringCount <= 1 ? 1.0f : 0.0f;
        ringTabs_.focusedIndex = activeRing;
        ringSelectionInitialized_ = true;
    }
    if (ringCount <= 1) {
        selectedRingIndex_ = 0;
        cardFade_ = 1.0f;
    } else if (selectedRingIndex_ >= ringCount) {
        selectedRingIndex_ = -1;
        cardFade_ = 0.0f;
    }
    const bool ringSelected = selectedRingIndex_ >= 0;
    const int ringIndex = std::clamp(ringSelected ? selectedRingIndex_ : 0, 0, ringCount - 1);
    const RingLevelUpgradePoints& points = levelRingUpgradePoints[static_cast<std::size_t>(ringIndex)];
    UiWindowScope levelUpWindow(renderer, "level_up", panel, "レベルアップ", levelUpHelpText(ringCount, ringSelected));
    if (ringCount > 1) {
        drawLevelUpSubtitle(renderer, panel);
    }

    if (ringCount > 1) {
        std::array<UiTabItem, SpellRingCount> ringTabs{};
        std::array<UiRect, SpellRingCount> ringTabRects{};
        std::array<std::string, SpellRingCount> ringTabLabels{};
        for (int i = 0; i < ringCount; ++i) {
            ringTabLabels[static_cast<std::size_t>(i)] = ringDisplayName(i, ringCount);
            ringTabs[static_cast<std::size_t>(i)] = {
                ringTabLabels[static_cast<std::size_t>(i)],
                true,
                ringDisplayIconImageNumber(i),
            };
            ringTabRects[static_cast<std::size_t>(i)] = ringTabRect(i, ringCount);
        }
        drawUiTabs(
            renderer,
            ringTabs_,
            selectedRingIndex_,
            ringTabs.data(),
            ringCount,
            ringTabRects.data());
    }
    drawUpgradePrompt(renderer, panel, ringCount);

    if (cardFade_ <= 0.0f) {
        return;
    }

    renderer.pushScreenTransform({0.0f, 0.0f}, 1.0f, clamp(cardFade_, 0.0f, 1.0f));
    for (int i = 0; i < 3; ++i) {
        const UiRect card = optionRect(i, ringCount);
        UiRect content = card;
        content.pos.y += LevelUpCardContentOffsetY;
        const bool preferred = i == selectedOption_;
        const bool selected = uiControlVisualState(card).selected;
        UiButtonStyle cardStyle;
        cardStyle.imageTint = {232, 232, 238, 245};
        cardStyle.imageTintHot = {255, 255, 235, 255};
        cardStyle.fill = {22, 22, 32, 232};
        cardStyle.fillHot = {54, 46, 76, 245};
        cardStyle.outline = {104, 94, 128, 255};
        cardStyle.outlineHot = ui::WindowBorder;
        drawUiFlexibleButtonFrame(renderer, card, preferred, cardStyle);
        drawCenteredText(renderer, content, content.pos.y + 42.0f, upgradeName(i), ui::Text, 3);
        drawCenteredText(renderer, content, content.pos.y + 90.0f, upgradeDescription(i), ui::Text, 2);
        drawUpgradeValueLine(
            renderer,
            content,
            content.pos.y + 134.0f,
            upgradeCurrentValueText(i, spellRing, ringIndex, balance),
            upgradeNextValueText(i, spellRing, ringIndex, points, balance),
            selected ? ui::Text : ui::TextMuted,
            2);
        const int currentStage = upgradeStageForOption(i, points);
        drawCenteredText(
            renderer,
            content,
            content.pos.y + 182.0f,
            "強化回数：" + std::to_string(currentStage),
            selected ? ui::Text : ui::TextMuted,
            2);
    }
    drawUiButton(renderer, okButtonRect(ringCount), "OK", false, uiActionButtonStyle());
    renderer.popScreenTransform();
}

}
