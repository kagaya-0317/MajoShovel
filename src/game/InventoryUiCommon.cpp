#include "game/InventoryUiCommon.hpp"

#include "game/EncyclopediaSystem.hpp"
#include "game/ItemImageRenderer.hpp"
#include "game/ObjectImageRenderer.hpp"
#include "game/ObjectVisualPose.hpp"
#include "game/WorldIconRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

namespace majo {

namespace {

enum class InlineIconKind {
    Item,
    World,
};

constexpr std::string_view BrokenItemNamePrefix = "壊れた";
constexpr Color BrokenItemImageTint{140, 140, 148, 220};
constexpr Color BrokenItemFallbackColor{82, 82, 90, 255};

struct InlineIconTag {
    InlineIconKind kind = InlineIconKind::Item;
    std::string_view key;
};

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

float inlineItemIconSize(Renderer& renderer, const InlineItemTextStyle& style)
{
    const Vec2 textSize = renderer.measureText("0", style.scale);
    return std::max(1.0f, textSize.y * std::max(0.1f, style.iconScale));
}

bool inlineIconTagAt(std::string_view text, std::size_t offset, std::size_t& outEnd, InlineIconTag& outTag)
{
    constexpr std::string_view ItemPrefix = "{item:";
    constexpr std::string_view WorldPrefix = "{world:";

    std::string_view prefix;
    InlineIconKind kind = InlineIconKind::Item;
    if (offset + ItemPrefix.size() < text.size() && text.substr(offset, ItemPrefix.size()) == ItemPrefix) {
        prefix = ItemPrefix;
        kind = InlineIconKind::Item;
    } else if (offset + WorldPrefix.size() < text.size() && text.substr(offset, WorldPrefix.size()) == WorldPrefix) {
        prefix = WorldPrefix;
        kind = InlineIconKind::World;
    } else {
        return false;
    }

    const std::size_t close = text.find('}', offset + prefix.size());
    if (close == std::string_view::npos) {
        return false;
    }

    outTag.key = text.substr(offset + prefix.size(), close - offset - prefix.size());
    if (outTag.key.empty()) {
        return false;
    }

    outTag.kind = kind;
    outEnd = close + 1;
    return true;
}

std::size_t findNextInlineIconTag(std::string_view text, std::size_t offset)
{
    const std::size_t item = text.find("{item:", offset);
    const std::size_t world = text.find("{world:", offset);
    if (item == std::string_view::npos) {
        return world;
    }
    if (world == std::string_view::npos) {
        return item;
    }
    return std::min(item, world);
}

std::string popInlineItemTextUnit(std::string text)
{
    if (text.empty()) {
        return text;
    }

    if (text.back() == '}') {
        const std::size_t itemOpen = text.rfind("{item:");
        const std::size_t worldOpen = text.rfind("{world:");
        const std::size_t open = itemOpen == std::string::npos
            ? worldOpen
            : (worldOpen == std::string::npos ? itemOpen : std::max(itemOpen, worldOpen));
        if (open != std::string::npos) {
            std::size_t tagEnd = 0;
            InlineIconTag tag;
            if (inlineIconTagAt(text, open, tagEnd, tag) && tagEnd == text.size()) {
                text.erase(open);
                return text;
            }
        }
    }

    text.pop_back();
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xc0U) == 0x80U) {
        text.pop_back();
    }
    return text;
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

Color multiplyColor(Color color, Color multiplier)
{
    color.r = static_cast<unsigned char>((static_cast<int>(color.r) * static_cast<int>(multiplier.r)) / 255);
    color.g = static_cast<unsigned char>((static_cast<int>(color.g) * static_cast<int>(multiplier.g)) / 255);
    color.b = static_cast<unsigned char>((static_cast<int>(color.b) * static_cast<int>(multiplier.b)) / 255);
    color.a = static_cast<unsigned char>((static_cast<int>(color.a) * static_cast<int>(multiplier.a)) / 255);
    return color;
}

float drawInventoryDetailHeader(Renderer& renderer, UiRect panel, std::string_view text, bool protectedItem)
{
    constexpr float MinHeaderHeight = 50.0f;
    constexpr float HeaderGap = 16.0f;
    constexpr float ProtectionGap = 12.0f;
    constexpr int TitleScale = 3;
    constexpr int ProtectionScale = 2;
    const UiRect content = uiSubPanelContentRect(panel);
    const std::string_view protectionText = "保護";
    const Vec2 protectionSize = protectedItem
        ? renderer.measureText(protectionText, ProtectionScale)
        : Vec2{};
    const float titleMaxWidth = protectedItem
        ? std::max(0.0f, content.size.x - protectionSize.x - ProtectionGap)
        : content.size.x;

    renderer.drawWrappedText(content.pos, text, titleMaxWidth, ui::Text, TitleScale);
    renderer.drawWrappedText({content.pos.x + 1.0f, content.pos.y}, text, titleMaxWidth, ui::Text, TitleScale);
    if (protectedItem) {
        renderer.drawText(
            {content.pos.x + content.size.x - protectionSize.x, content.pos.y + 6.0f},
            protectionText,
            {255, 230, 150, 255},
            ProtectionScale);
    }
    return content.pos.y + std::max(MinHeaderHeight, renderer.measureWrappedText(text, titleMaxWidth, TitleScale).y + HeaderGap);
}

void drawDetailSeparator(Renderer& renderer, UiRect panel, float& y)
{
    constexpr float SeparatorBleed = 10.0f;
    const UiRect content = uiSubPanelContentRect(panel);
    y += 4.0f;
    const UiRect separator{
        Vec2{content.pos.x - SeparatorBleed, y},
        Vec2{content.size.x + SeparatorBleed * 2.0f, ui::SeparatorHeight}};
    drawUiSeparator(renderer, separator);
    y += ui::SeparatorHeight + 2.0f;
}

void drawExtraLines(Renderer& renderer, UiRect panel, float& y, const std::vector<InventoryUiDetailExtraLine>& extraLines)
{
    if (extraLines.empty()) {
        return;
    }
    drawDetailSeparator(renderer, panel, y);
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

std::string itemDisplayName(std::string_view baseName, bool broken)
{
    if (!broken || baseName.empty() || baseName.starts_with(BrokenItemNamePrefix)) {
        return std::string(baseName);
    }
    return std::string(BrokenItemNamePrefix) + std::string(baseName);
}

Color itemFallbackColorForBrokenState(Color color, bool broken)
{
    if (!broken) {
        return color;
    }
    Color brokenColor = BrokenItemFallbackColor;
    brokenColor.a = color.a;
    return brokenColor;
}

ObjectImageDrawOptions itemImageOptionsWithBrokenState(ObjectImageDrawOptions options, bool broken)
{
    if (broken) {
        options.tint = multiplyColor(options.tint, BrokenItemImageTint);
    }
    return options;
}

InventoryUiItemStats inventoryUiStatsFromInstance(const ItemInstance& instance)
{
    return {
        instance.instanceId,
        instance.currentDurability,
        instance.maxDurability,
        instance.enhanceLevel,
        instance.attackBonus,
        instance.digBonus,
        instance.durabilityBonus,
        instance.weightModifier,
        instance.sizeModifier,
        instance.protectionEnabled,
        instance.isBroken,
    };
}

InventoryUiItemStats inventoryUiStatsFromRingItem(const SpellRingItem& item)
{
    return {
        item.instanceId,
        item.durability,
        item.maxDurability,
        item.enhanceLevel,
        item.attackBonus,
        item.digBonus,
        item.durabilityBonus,
        item.weightModifier,
        item.sizeModifier,
        item.protectionEnabled,
        item.broken(),
    };
}

std::optional<InventoryUiItemStats> inventoryUiEntryStats(const InventoryUiEntryView& entry)
{
    if (entry.stats) {
        return entry.stats;
    }
    if (entry.instance != nullptr) {
        return inventoryUiStatsFromInstance(*entry.instance);
    }
    return std::nullopt;
}

std::string joinInventoryUiEffectLines(const std::vector<std::string>& lines)
{
    if (lines.empty()) {
        return "-";
    }
    if (lines.size() == 1 && lines.front() == "\xE3\x81\xAA\xE3\x81\x97") {
        return lines.front();
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

void drawItemEffectDetailSections(
    Renderer& renderer,
    UiRect panel,
    float& y,
    const ItemData& item,
    const ObjectCatalog& catalog,
    const EncyclopediaSystem& encyclopedia)
{
    const ObjectEffectDisplaySections sections =
        encyclopedia.getObjectEffectDisplaySections(item.id, catalog, EffectRevealMode::WithUnknown);
    if (!sections.useLines.empty()) {
        drawUiDetailText(renderer, panel, y, "使用時の効果");
        drawUiDetailText(renderer, panel, y, joinInventoryUiEffectLines(sections.useLines));
    }
    if (!sections.ringLines.empty()) {
        drawUiDetailText(renderer, panel, y, "リングに乗せたときの効果");
        drawUiDetailText(renderer, panel, y, joinInventoryUiEffectLines(sections.ringLines));
    }
}

Vec2 measureInlineItemText(Renderer& renderer, std::string_view text, const InlineItemTextStyle& style)
{
    const float iconSize = inlineItemIconSize(renderer, style);
    const Vec2 lineMeasure = renderer.measureText("0", style.scale);
    float width = 0.0f;
    float height = std::max(lineMeasure.y, iconSize);

    std::size_t cursor = 0;
    while (cursor < text.size()) {
        std::size_t tagEnd = 0;
        InlineIconTag tag;
        if (inlineIconTagAt(text, cursor, tagEnd, tag)) {
            (void)tag;
            width += iconSize + style.iconTextGap;
            cursor = tagEnd;
            continue;
        }

        const std::size_t nextTag = findNextInlineIconTag(text, cursor + 1);
        const std::size_t end = nextTag == std::string_view::npos ? text.size() : nextTag;
        const std::string_view chunk = text.substr(cursor, end - cursor);
        const Vec2 chunkSize = renderer.measureText(chunk, style.scale);
        width += chunkSize.x;
        height = std::max(height, chunkSize.y);
        cursor = end;
    }

    return {width, height};
}

std::string fittedInlineItemText(Renderer& renderer, std::string text, float maxWidth, const InlineItemTextStyle& style)
{
    if (maxWidth <= 0.0f) {
        return "";
    }
    if (measureInlineItemText(renderer, text, style).x <= maxWidth) {
        return text;
    }

    constexpr std::string_view Ellipsis = "...";
    while (!text.empty()) {
        text = popInlineItemTextUnit(std::move(text));
        std::string candidate = text + std::string(Ellipsis);
        if (measureInlineItemText(renderer, candidate, style).x <= maxWidth) {
            return candidate;
        }
    }
    return measureInlineItemText(renderer, Ellipsis, style).x <= maxWidth ? std::string(Ellipsis) : "";
}

std::string inlineItemTag(std::string_view objectId)
{
    if (objectId.empty()) {
        return {};
    }
    return "{item:" + std::string(objectId) + "}";
}

std::string inlineWorldIconTag(std::string_view worldIconKey)
{
    if (worldIconKey.empty()) {
        return {};
    }
    return "{world:" + std::string(worldIconKey) + "}";
}

std::string inlineMaterialIconTag(MaterialType type)
{
    if (type == MaterialType::Count) {
        return {};
    }
    return inlineWorldIconTag(materialTypeSaveName(type));
}

void drawInlineItemText(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    Vec2 pos,
    std::string_view text,
    const InlineItemTextStyle& style)
{
    const float iconSize = inlineItemIconSize(renderer, style);
    const Vec2 lineMeasure = renderer.measureText("0", style.scale);
    constexpr float InlineIconVisualYOffset = -2.0f;
    const float iconTopOffset = (lineMeasure.y - iconSize) * 0.5f + InlineIconVisualYOffset;
    Vec2 cursor = pos;

    std::size_t offset = 0;
    while (offset < text.size()) {
        std::size_t tagEnd = 0;
        InlineIconTag tag;
        if (inlineIconTagAt(text, offset, tagEnd, tag)) {
            const Vec2 center{cursor.x + iconSize * 0.5f, cursor.y + iconTopOffset + iconSize * 0.5f};
            bool drewIcon = false;
            if (tag.kind == InlineIconKind::Item) {
                if (const ObjectDefinition* object = catalog.registry.findById(tag.key)) {
                    ObjectImageDrawOptions options;
                    options.applyScaleOverride = false;
                    drewIcon = drawItemImage(
                        renderer,
                        *object,
                        center,
                        {iconSize, iconSize},
                        objectGroundImageOptions(*object, options));
                }
            } else {
                const WorldIconDefinition* definition = worldIconDefinitionByKey(tag.key);
                WorldIconDrawOptions options;
                options.applyScaleOverride = false;
                drewIcon = definition != nullptr && drawWorldIcon(renderer, definition->iconId, center, {iconSize, iconSize}, options);
            }
            if (drewIcon) {
                cursor.x += iconSize + style.iconTextGap;
                offset = tagEnd;
                continue;
            }
        }

        const std::size_t nextTag = findNextInlineIconTag(text, offset + 1);
        const std::size_t end = nextTag == std::string_view::npos ? text.size() : nextTag;
        const std::string_view chunk = text.substr(offset, end - offset);
        if (style.outlineEnabled) {
            renderer.drawOutlinedText(cursor, chunk, style.text, style.outline, style.outlinePx, style.scale);
        } else {
            renderer.drawText(cursor, chunk, style.text, style.scale);
        }
        cursor.x += renderer.measureText(chunk, style.scale).x;
        offset = end;
    }
}

void drawInlineItemTextRightAligned(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    Vec2 rightTop,
    std::string_view text,
    const InlineItemTextStyle& style)
{
    const Vec2 size = measureInlineItemText(renderer, text, style);
    drawInlineItemText(renderer, catalog, {rightTop.x - size.x, rightTop.y}, text, style);
}

void drawInventoryUiSlotBottomLabel(Renderer& renderer, UiRect rect, std::string_view label, Color color)
{
    if (label.empty()) {
        return;
    }
    constexpr int LabelScale = 2;
    const Vec2 labelSize = renderer.measureText(label, LabelScale);
    constexpr float VisualCenterCorrectionX = 3.0f;
    const Vec2 labelPos{
        rect.pos.x + (rect.size.x - labelSize.x) * 0.5f + VisualCenterCorrectionX,
        rect.pos.y + rect.size.y - labelSize.y - 4.0f,
    };
    renderer.drawOutlinedText(labelPos, label, color, {0, 0, 0, 120}, 6, LabelScale);
}

void drawInventoryUiSlot(
    Renderer& renderer,
    UiRect rect,
    const InventoryUiEntryView& entry,
    const InventoryUiSlotStyle& style)
{
    Color fill = style.selected ? Color{54, 44, 72, 242} : Color{20, 20, 28, 226};
    const Vec2 slotCenter = uiRectCenter(rect);
    const std::optional<InventoryUiItemStats> stats = inventoryUiEntryStats(entry);
    renderer.fillCircle(slotCenter, slotFrameRadius(rect), fill);
    const auto drawBottomLabel = [&]() {
        drawInventoryUiSlotBottomLabel(renderer, rect, style.bottomLabel, style.bottomLabelColor);
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
    const auto drawProtectionLabel = [&]() {
        if (!style.showProtectionLabel || !stats || !stats->protectionEnabled) {
            return;
        }
        constexpr int LabelScale = 2;
        renderer.drawOutlinedText(
            rect.pos + Vec2{5.0f, 3.0f},
            "保護",
            style.protectionLabelColor,
            {0, 0, 0, 120},
            6,
            LabelScale);
    };

    if (entry.item == nullptr) {
        drawBottomLabel();
        drawTopRightCount();
        return;
    }

    const bool broken = stats ? stats->broken : entry.item->durability == 0;

    if (!stats) {
        const Color objectColor = itemFallbackColorForBrokenState(inventoryUiObjectColor(*entry.item), broken);
        ObjectImageDrawOptions imageOptions;
        imageOptions.tint = style.disabled ? Color{128, 128, 128, 255} : Color{255, 255, 255, 255};
        imageOptions = itemImageOptionsWithBrokenState(imageOptions, broken);
        if (style.selected) {
            imageOptions = withSelectedItemOutline(imageOptions);
        }
        const bool drewImage = drawItemImage(
            renderer,
            *entry.item,
            slotCenter,
            {style.imageMaxSize, style.imageMaxSize},
            objectGroundImageOptions(*entry.item, imageOptions));
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

    const Color objectColor = itemFallbackColorForBrokenState(inventoryUiObjectColor(*entry.item), broken);
    ObjectImageDrawOptions imageOptions;
    imageOptions.tint = Color{255, 255, 255, 255};
    if (style.disabled) {
        imageOptions.tint = darkenColor(imageOptions.tint, 0.5f);
    }
    imageOptions = itemImageOptionsWithBrokenState(imageOptions, broken);
    if (style.selected) {
        imageOptions = withSelectedItemOutline(imageOptions);
    }
    const bool drewImage = drawItemImage(
        renderer,
        *entry.item,
        slotCenter,
        {style.imageMaxSize, style.imageMaxSize},
        objectGroundImageOptions(*entry.item, imageOptions));
    if (!drewImage) {
        renderer.fillCircle(slotCenter, 22.0f, style.disabled ? darkenColor(objectColor, 0.5f) : objectColor);
        if (style.selected) {
            drawSelectedItemCircleOutline(renderer, slotCenter, 22.0f);
        }
    }
    drawProtectionLabel();
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
    const InventoryUiDetailOptions& options,
    const std::vector<InventoryUiDetailExtraLine>& extraLines)
{
    char buffer[160];
    const std::optional<InventoryUiItemStats> stats = inventoryUiEntryStats(entry);
    std::string detailTitle = "Empty";
    if (entry.item != nullptr) {
        const bool broken = stats ? stats->broken : entry.item->durability == 0;
        const std::string displayName = itemDisplayName(entry.item->name, broken);
        if (!stats && entry.stackCount > 1) {
            std::snprintf(buffer, sizeof(buffer), "%s x%d", displayName.c_str(), entry.stackCount);
            detailTitle = buffer;
        } else {
            detailTitle = displayName;
        }
    }

    if (entry.item == nullptr) {
        drawUiSubPanel(renderer, panel);
        return;
    }

    drawUiSubPanel(renderer, panel);
    float detailLineY = drawInventoryDetailHeader(renderer, panel, detailTitle, stats && stats->protectionEnabled);

    drawUiDetailText(renderer, panel, detailLineY, entry.item->description.empty() ? "-" : entry.item->description);
    drawItemEffectDetailSections(renderer, panel, detailLineY, *entry.item, catalog, encyclopedia);

    if (stats) {
        if (stats->maxDurability < 0) {
            std::snprintf(buffer, sizeof(buffer), "壊れない");
        } else {
            std::snprintf(buffer, sizeof(buffer), "%d/%d", stats->currentDurability, stats->maxDurability);
        }
    } else if (entry.item->durability < 0) {
        std::snprintf(buffer, sizeof(buffer), "壊れない");
    } else {
        std::snprintf(buffer, sizeof(buffer), "%d", entry.item->durability);
    }
    drawUiDetailLine(renderer, panel, detailLineY, "耐久力", buffer);

    const double weightKg = entry.item->weightKg * (stats ? stats->weightModifier : 1.0);
    std::snprintf(buffer, sizeof(buffer), "%.1fkg", weightKg);
    drawUiDetailLine(renderer, panel, detailLineY, "重さ", buffer);

    if (options.showEnhanceCount && stats && stats->enhanceLevel > 0) {
        std::snprintf(buffer, sizeof(buffer), "%d", stats->enhanceLevel);
        drawUiDetailLine(renderer, panel, detailLineY, "強化回数", buffer);
    }

    drawExtraLines(renderer, panel, detailLineY, extraLines);
}

}
