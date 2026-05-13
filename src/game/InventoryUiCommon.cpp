#include "game/InventoryUiCommon.hpp"

#include "game/EncyclopediaSystem.hpp"
#include "game/ObjectImageRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

namespace majo {

namespace {

bool objectCategoryEquals(const ItemData& item, std::string_view category)
{
    return item.category == category;
}

float slotFrameRadius(const UiRect& rect)
{
    return std::min(rect.size.x, rect.size.y) * 0.5f;
}

Vec2 uiRectCenter(const UiRect& rect)
{
    return rect.pos + rect.size * 0.5f;
}

void drawSelectedItemCircleOutline(Renderer& renderer, Vec2 center, float radius)
{
    const Color outline = selectedItemOutlineColor();
    for (int i = 0; i < 6; ++i) {
        renderer.drawCircle(center, radius + static_cast<float>(i), outline);
    }
}

Color darkenColor(Color color, float factor)
{
    factor = std::clamp(factor, 0.0f, 1.0f);
    color.r = static_cast<unsigned char>(std::lround(static_cast<float>(color.r) * factor));
    color.g = static_cast<unsigned char>(std::lround(static_cast<float>(color.g) * factor));
    color.b = static_cast<unsigned char>(std::lround(static_cast<float>(color.b) * factor));
    return color;
}

void drawExtraLines(Renderer& renderer, UiRect panel, float& y, const std::vector<InventoryUiDetailExtraLine>& extraLines)
{
    for (const InventoryUiDetailExtraLine& line : extraLines) {
        drawUiDetailLine(renderer, panel, y, line.label, line.value, line.valueColor);
    }
}

}

Color inventoryUiObjectColor(const ItemData& item)
{
    if (objectCategoryEquals(item, "回復")) {
        return {116, 220, 144, 255};
    }
    if (objectCategoryEquals(item, "武器")) {
        return {224, 96, 86, 255};
    }
    if (objectCategoryEquals(item, "盾")) {
        return {104, 168, 226, 255};
    }
    if (objectCategoryEquals(item, "宝")) {
        return {244, 206, 78, 255};
    }
    if (objectCategoryEquals(item, "探索")) {
        return {136, 214, 214, 255};
    }
    return {188, 152, 236, 255};
}

std::string joinInventoryUiEffectLines(const std::vector<std::string>& lines)
{
    if (lines.empty()) {
        return "-";
    }
    std::string text;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (!text.empty()) {
            text += '\n';
        }
        text += "・";
        text += lines[i];
    }
    return text;
}

void drawInventoryUiSlot(
    Renderer& renderer,
    UiRect rect,
    const InventoryUiEntryView& entry,
    const InventoryUiSlotStyle& style)
{
    Color fill = style.selected ? Color{54, 44, 72, 242} : Color{20, 20, 28, 226};
    const Vec2 slotCenter = uiRectCenter(rect);
    renderer.fillCircle(slotCenter, slotFrameRadius(rect), fill);
    const auto drawBottomLabel = [&]() {
        if (style.bottomLabel.empty()) {
            return;
        }
        constexpr int LabelScale = 2;
        const Vec2 labelSize = renderer.measureText(style.bottomLabel, LabelScale);
        // Native text measurement includes a small right-side safety margin.
        constexpr float VisualCenterCorrectionX = 3.0f;
        const Vec2 labelPos{
            rect.pos.x + (rect.size.x - labelSize.x) * 0.5f + VisualCenterCorrectionX,
            rect.pos.y + rect.size.y - labelSize.y - 4.0f,
        };
        renderer.drawOutlinedText(labelPos, style.bottomLabel, style.bottomLabelColor, {0, 0, 0, 120}, 6, LabelScale);
    };
    const auto drawTopRightCount = [&]() {
        if (!style.showTopRightCount) {
            return;
        }
        constexpr int CountScale = 2;
        const std::string text = "\xC3\x97" + std::to_string(style.topRightCount);
        const Vec2 textSize = renderer.measureText(text, CountScale);
        const Vec2 textPos{
            rect.pos.x + rect.size.x - textSize.x - 5.0f,
            rect.pos.y + 3.0f,
        };
        renderer.drawOutlinedText(textPos, text, style.topRightCountColor, {0, 0, 0, 120}, 6, CountScale);
    };

    if (entry.item == nullptr) {
        drawBottomLabel();
        drawTopRightCount();
        return;
    }

    if (entry.instance == nullptr) {
        const Color objectColor = inventoryUiObjectColor(*entry.item);
        ObjectImageDrawOptions imageOptions;
        imageOptions.tint = style.disabled ? Color{128, 128, 128, 255} : Color{255, 255, 255, 255};
        if (style.selected) {
            imageOptions = withSelectedItemOutline(imageOptions);
        }
        const bool drewImage = drawObjectImage(
            renderer,
            *entry.item,
            slotCenter,
            {style.imageMaxSize, style.imageMaxSize},
            imageOptions);
        if (!drewImage) {
            renderer.fillCircle(slotCenter, 22.0f, style.disabled ? darkenColor(objectColor, 0.5f) : objectColor);
            if (style.selected) {
                drawSelectedItemCircleOutline(renderer, slotCenter, 22.0f);
            }
        }
        drawBottomLabel();
        drawTopRightCount();
        return;
    }

    const Color objectColor = entry.instance->isBroken ? Color{82, 82, 90, 255} : inventoryUiObjectColor(*entry.item);
    ObjectImageDrawOptions imageOptions;
    imageOptions.tint = entry.instance->isBroken ? Color{140, 140, 148, 220} : Color{255, 255, 255, 255};
    if (style.disabled) {
        imageOptions.tint = darkenColor(imageOptions.tint, 0.5f);
    }
    if (style.selected) {
        imageOptions = withSelectedItemOutline(imageOptions);
    }
    const bool drewImage = drawObjectImage(
        renderer,
        *entry.item,
        slotCenter,
            {style.imageMaxSize, style.imageMaxSize},
            imageOptions);
    if (!drewImage) {
        renderer.fillCircle(slotCenter, 22.0f, style.disabled ? darkenColor(objectColor, 0.5f) : objectColor);
        if (style.selected) {
            drawSelectedItemCircleOutline(renderer, slotCenter, 22.0f);
        }
    }
    drawBottomLabel();
    drawTopRightCount();
}

void drawInventoryUiSlot(
    Renderer& renderer,
    UiRect rect,
    const InventoryUiEntryView& entry,
    bool selected,
    float imageMaxSize)
{
    drawInventoryUiSlot(renderer, rect, entry, InventoryUiSlotStyle{selected, false, imageMaxSize});
}

void drawInventoryUiDetailPanel(
    Renderer& renderer,
    UiRect panel,
    const InventoryUiEntryView& entry,
    const ObjectCatalog& catalog,
    const EncyclopediaSystem& encyclopedia,
    bool showProtectionOperation,
    const std::vector<InventoryUiDetailExtraLine>& extraLines)
{
    char buffer[160];
    std::string detailTitle = "Empty";
    if (entry.item != nullptr) {
        if (entry.instance == nullptr) {
            std::snprintf(buffer, sizeof(buffer), "%s x%d", entry.item->name.c_str(), entry.stackCount);
            detailTitle = buffer;
        } else {
            detailTitle = entry.item->name;
        }
    }

    if (entry.item == nullptr) {
        drawUiSubPanel(renderer, panel);
        return;
    }

    drawUiSubPanel(renderer, panel);
    float detailLineY = drawUiDetailHeader(renderer, panel, detailTitle);

    const std::vector<std::string> effectLines =
        encyclopedia.getObjectEffectDisplayLines(entry.item->id, catalog, EffectRevealMode::WithUnknown);
    const std::string effectText = joinInventoryUiEffectLines(effectLines);
    drawUiDetailText(renderer, panel, detailLineY, entry.item->description.empty() ? "-" : entry.item->description);
    std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(effectLines.size()));
    drawUiDetailLine(renderer, panel, detailLineY, "効果数", buffer);
    drawUiDetailText(renderer, panel, detailLineY, "効果");
    drawUiDetailText(renderer, panel, detailLineY, effectText);

    if (entry.instance == nullptr) {
        std::snprintf(buffer, sizeof(buffer), "%d", entry.item->attackPower);
        drawUiDetailLine(renderer, panel, detailLineY, "攻撃力", buffer);
        const std::string damageTypeText = entry.item->damageType.empty() ? "-" : std::string(damageTypeDisplayName(entry.item->damageType));
        drawUiDetailLine(renderer, panel, detailLineY, "ダメージ", damageTypeText);
        std::snprintf(buffer, sizeof(buffer), "%d", entry.item->digPower);
        drawUiDetailLine(renderer, panel, detailLineY, "掘削力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%d", entry.item->durability);
        drawUiDetailLine(renderer, panel, detailLineY, "耐久力", buffer);
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", entry.item->weightKg);
        drawUiDetailLine(renderer, panel, detailLineY, "重さ", buffer);
        drawExtraLines(renderer, panel, detailLineY, extraLines);
        return;
    }

    const ItemInstance& instance = *entry.instance;
    std::snprintf(buffer, sizeof(buffer), "%s", instance.instanceId.c_str());
    drawUiDetailLine(renderer, panel, detailLineY, "個体ID", buffer);
    std::snprintf(buffer, sizeof(buffer), "%d", instance.enhanceLevel);
    drawUiDetailLine(renderer, panel, detailLineY, "強化Lv", buffer);
    std::snprintf(buffer, sizeof(buffer), "%d/%d", instance.currentDurability, instance.maxDurability);
    drawUiDetailLine(renderer, panel, detailLineY, "耐久力", buffer);
    drawUiDetailLine(renderer, panel, detailLineY, "保護", instance.protectionEnabled ? "ON" : "OFF");
    drawUiDetailLine(renderer, panel, detailLineY, "状態", instance.isBroken ? "破損" : "通常");
    std::snprintf(buffer, sizeof(buffer), "+%d / +%d / +%d", instance.attackBonus, instance.digBonus, instance.durabilityBonus);
    drawUiDetailLine(renderer, panel, detailLineY, "補正", buffer);
    if (showProtectionOperation) {
        drawUiDetailLine(renderer, panel, detailLineY, "操作", "P 保護ON/OFF");
    }
    drawExtraLines(renderer, panel, detailLineY, extraLines);
}

}
