#include "game/InventoryUiCommon.hpp"

#include "engine/Log.hpp"
#include "game/EncyclopediaSystem.hpp"
#include "game/ItemImageRenderer.hpp"
#include "game/ObjectImageRenderer.hpp"
#include "game/ObjectVisualPose.hpp"
#include "game/OrbitModifiers.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/WorldIconRenderer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_set>

namespace majo {

namespace {

enum class InlineIconKind {
    Item,
    World,
};

enum class EnhancementBadgeKind {
    Attack,
    Dig,
    Durability,
    Lighten,
    Enlarge,
};

struct EnhancementBadge {
    EnhancementBadgeKind kind = EnhancementBadgeKind::Attack;
    int count = 1;
};

constexpr std::string_view BrokenItemNamePrefix = "壊れた";
constexpr Color BrokenItemImageTint{72, 72, 80, 238};
constexpr Color BrokenItemFallbackColor{42, 42, 48, 255};
constexpr Color InventoryEffectTextColor{255, 230, 150, 255};
constexpr float TwoPi = 6.283185307f;
constexpr float DetailLabelWidth = 106.0f;
constexpr float DetailMinLineHeight = 31.0f;
constexpr float DetailLineGap = 4.0f;

struct InlineIconTag {
    InlineIconKind kind = InlineIconKind::Item;
    std::string_view key;
};

bool objectCategoryEquals(const ItemData& item, std::string_view category)
{
    return item.category == category;
}

void trimInPlace(std::string& text)
{
    const auto isSpace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.front()))) {
        text.erase(text.begin());
    }
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
}

std::vector<std::string> splitInventoryEffectTextLines(std::string_view text)
{
    std::vector<std::string> lines;
    std::string current;
    const auto flush = [&]() {
        trimInPlace(current);
        if (!current.empty()) {
            lines.push_back(std::move(current));
        }
        current.clear();
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == '\n' || ch == '\r') {
            flush();
            continue;
        }
        if (i + 2 < text.size() &&
            ch == 0xEF &&
            static_cast<unsigned char>(text[i + 1]) == 0xBD &&
            static_cast<unsigned char>(text[i + 2]) == 0x9C) {
            flush();
            i += 2;
            continue;
        }
        current.push_back(text[i]);
    }
    flush();
    return lines;
}

bool noInventoryEffectText(std::string_view text)
{
    std::string normalized(text);
    trimInPlace(normalized);
    return normalized == "none" || normalized == "なし";
}

std::string signedPercentText(double multiplier)
{
    const int percent = static_cast<int>(std::round((multiplier - 1.0) * 100.0));
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%+d%%", percent);
    return buffer;
}

std::string signedWeightStopRatioText(double multiplier)
{
    const double delta = static_cast<double>(SpellRingSystem::weightStopRatioForPenaltyMultiplier(multiplier)) -
        static_cast<double>(SpellRingSystem::BaseWeightStopRatio);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%+.2f倍", delta);
    return buffer;
}

std::string signedAddText(double value, std::string_view unit)
{
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%+.1f%s", value, std::string(unit).c_str());
    return buffer;
}

std::string equipmentTargetDisplayPrefix(std::string_view target)
{
    if (target == "equip_ring1") {
        return "リング1: ";
    }
    if (target == "equip_ring2") {
        return "リング2: ";
    }
    if (target == "equip_ring3") {
        return "リング3: ";
    }
    return {};
}

void logUnsupportedStaffEquipmentEffectOnce(std::string_view objectId, std::string_view effect)
{
    static std::unordered_set<std::string> logged;
    std::string key(objectId);
    key += ':';
    key += effect;
    if (logged.insert(key).second) {
        logError("[warning] Staff equipment UI: unsupported equipment effect \"" +
            std::string(effect) + "\" on object \"" + std::string(objectId) + "\"");
    }
}

std::string staffEquipmentEffectLine(
    const ItemData& item,
    const ObjectCatalog& catalog,
    std::string_view target,
    std::string_view effect,
    double value)
{
    const double multiplier = value == 0.0 ? 1.0 : value;
    std::string line = equipmentTargetDisplayPrefix(target);
    if (effect == "ring_speed_mul") {
        line += "速度";
        line += signedPercentText(multiplier);
    } else if (effect == "ring_radius_mul") {
        line += "半径";
        line += signedPercentText(multiplier);
    } else if (effect == "ring_weight_limit_add") {
        line += "重量上限";
        line += signedAddText(value, "kg");
    } else if (effect == "ring_shift_distance_mul") {
        line += "ずらし距離";
        line += signedPercentText(multiplier);
    } else if (effect == "ring_throw_distance_mul") {
        line += "投げ距離";
        line += signedPercentText(multiplier);
    } else if (effect == "ring_throw_speed_mul") {
        line += "投げ速度";
        line += signedPercentText(multiplier);
    } else if (effect == "ring_throw_cooldown_mul") {
        line += "投げクールダウン";
        line += signedPercentText(multiplier);
    } else if (effect == "ring_return_speed_mul") {
        line += "戻り速度";
        line += signedPercentText(multiplier);
    } else if (effect == "ring_output_mul") {
        line += "最終出力";
        line += signedPercentText(multiplier);
    } else if (effect == "ring_anchor_mul") {
        line += "アンカー";
        line += signedPercentText(multiplier);
    } else if (effect == "ring_damage_speed_mul") {
        line += "速度ダメージ";
        line += signedPercentText(multiplier);
    } else if (effect == "light_radius_mul") {
        line += "照明半径";
        line += signedPercentText(multiplier);
    } else if (effect == "detect_range_mul") {
        line += "探知範囲";
        line += signedPercentText(multiplier);
    } else if (effect == "guard_area_mul") {
        line += "防御範囲";
        line += signedPercentText(multiplier);
    } else if (effect == "reflect_power_mul") {
        line += "反射威力";
        line += signedPercentText(multiplier);
    } else if (effect == "reflect_chance_add") {
        line += "反射確率";
        line += signedAddText(value, "%");
    } else if (effect == "metal_weight_penalty_mul") {
        line += "過積載停止";
        line += signedWeightStopRatioText(multiplier);
    } else if (effect == "dig_power_mul") {
        line += "掘削力";
        line += signedPercentText(multiplier);
    } else if (effect == "durability_cost_mul") {
        line += "耐久消費";
        line += signedPercentText(multiplier);
    } else if (effect == "sell_price_mul") {
        line += "売却価格";
        line += signedPercentText(multiplier);
    } else if (effect == "money_visible_level") {
        line += "お金表示Lv";
        line += std::to_string(std::max(0, static_cast<int>(std::round(value))));
    } else if (effect == "danger_hint_level") {
        line += "危険ヒントLv";
        line += std::to_string(std::max(0, static_cast<int>(std::round(value))));
    } else {
        logUnsupportedStaffEquipmentEffectOnce(item.id, effect);
        line += "未実装の装備効果: ";
        line += effectCodeDisplayName(catalog, effect);
    }
    return line;
}

std::vector<std::string> staffEquipmentEffectLines(const ItemData& item, const ObjectCatalog& catalog)
{
    std::vector<std::string> lines;
    for (const EffectSpec& spec : item.normalEffects) {
        if (!isStaffEquipTarget(spec.target)) {
            continue;
        }
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            const std::string& effect = spec.effects[index];
            if (effect.empty() || effect == "none") {
                continue;
            }
            const double value = index < spec.values.size() ? spec.values[index] : 0.0;
            lines.push_back(staffEquipmentEffectLine(item, catalog, spec.target, effect, value));
        }
    }
    return lines;
}

void applyStaffManualEquipmentEffectText(
    const ItemData& item,
    std::vector<std::string>& equipmentLines,
    std::size_t ringLineCount)
{
    std::vector<std::string> manualLines = splitInventoryEffectTextLines(item.effectText);
    if (manualLines.empty() || (manualLines.size() == 1 && noInventoryEffectText(manualLines.front()))) {
        return;
    }

    const std::size_t equipmentCount = equipmentLines.size();
    if (equipmentCount == 0) {
        return;
    }

    const bool equipmentOnlyText = manualLines.size() == equipmentCount;
    const bool equipmentAndRingText = ringLineCount > 0 && manualLines.size() == equipmentCount + ringLineCount;
    if (equipmentOnlyText || equipmentAndRingText) {
        equipmentLines.assign(manualLines.begin(), manualLines.begin() + static_cast<std::ptrdiff_t>(equipmentCount));
    }
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

unsigned char colorByte(float value)
{
    return static_cast<unsigned char>(std::clamp(std::lround(value), 0L, 255L));
}

Color mixColor(Color from, Color to, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return {
        colorByte(static_cast<float>(from.r) + (static_cast<float>(to.r) - static_cast<float>(from.r)) * t),
        colorByte(static_cast<float>(from.g) + (static_cast<float>(to.g) - static_cast<float>(from.g)) * t),
        colorByte(static_cast<float>(from.b) + (static_cast<float>(to.b) - static_cast<float>(from.b)) * t),
        colorByte(static_cast<float>(from.a) + (static_cast<float>(to.a) - static_cast<float>(from.a)) * t),
    };
}

float smootherStep01(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * value * (value * (value * 6.0f - 15.0f) + 10.0f);
}

Color withAlpha(Color color, float alpha)
{
    color.a = colorByte(alpha);
    return color;
}

Color scaleAlpha(Color color, float alphaScale)
{
    color.a = colorByte(static_cast<float>(color.a) * std::clamp(alphaScale, 0.0f, 1.0f));
    return color;
}

Color hsvColor(float hue, float saturation, float value)
{
    hue = hue - std::floor(hue);
    saturation = std::clamp(saturation, 0.0f, 1.0f);
    value = std::clamp(value, 0.0f, 1.0f);

    const float sector = hue * 6.0f;
    const int index = static_cast<int>(std::floor(sector));
    const float fraction = sector - static_cast<float>(index);
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - saturation * fraction);
    const float r = value * (1.0f - saturation * (1.0f - fraction));

    float red = value;
    float green = r;
    float blue = p;
    switch (index % 6) {
    case 0:
        red = value;
        green = r;
        blue = p;
        break;
    case 1:
        red = q;
        green = value;
        blue = p;
        break;
    case 2:
        red = p;
        green = value;
        blue = r;
        break;
    case 3:
        red = p;
        green = q;
        blue = value;
        break;
    case 4:
        red = r;
        green = p;
        blue = value;
        break;
    default:
        red = value;
        green = p;
        blue = q;
        break;
    }

    return {colorByte(red * 255.0f), colorByte(green * 255.0f), colorByte(blue * 255.0f), 255};
}

Color rarityBaseColor(int rarity, int starIndex, float animationSeconds)
{
    rarity = std::clamp(rarity, 1, 10);
    constexpr Color Brown{205, 124, 66, 255};
    constexpr Color Gray{168, 170, 176, 255};
    constexpr Color Yellow{255, 226, 88, 255};
    constexpr Color YellowGreen{174, 226, 76, 255};
    constexpr Color Aqua{86, 220, 232, 255};
    constexpr Color Sky{104, 184, 255, 255};
    constexpr Color Copper{205, 124, 66, 255};
    constexpr Color Silver{153, 178, 188, 255};
    constexpr Color Gold{255, 216, 82, 255};

    switch (rarity) {
    case 1:
        return Brown;
    case 2:
        return Gray;
    case 3:
        return Yellow;
    case 4:
        return YellowGreen;
    case 5:
        return Aqua;
    case 6:
        return Sky;
    case 7:
        return Copper;
    case 8:
        return Silver;
    case 9:
        return Gold;
    default:
        break;
    }

    const float hue = animationSeconds * 0.16f + static_cast<float>(starIndex) * 0.095f;
    return hsvColor(hue, 0.58f, 1.0f);
}

float rarityShineScale(int rarity)
{
    if (rarity < 4) {
        return 0.0f;
    }
    if (rarity >= 7) {
        return 1.0f;
    }
    return static_cast<float>(rarity - 3) / 4.0f;
}

float rarityShineAmount(int rarity, int starIndex, float animationSeconds)
{
    const float scale = rarityShineScale(rarity);
    if (scale <= 0.0f) {
        return 0.0f;
    }
    const float primary =
        0.5f + 0.5f * std::sin(animationSeconds * TwoPi * 0.95f + static_cast<float>(starIndex) * 0.82f);
    const float secondary =
        0.5f + 0.5f * std::sin(animationSeconds * TwoPi * 0.37f + static_cast<float>(starIndex) * 1.31f + 1.7f);
    const float wave = std::clamp(primary * 0.82f + secondary * 0.18f, 0.0f, 1.0f);
    return scale * (0.18f + 0.82f * smootherStep01(wave));
}

void drawRarityStarGlow(
    Renderer& renderer,
    Vec2 pos,
    std::string_view star,
    Color baseColor,
    float shine,
    int scale)
{
    if (shine <= 0.0f) {
        return;
    }

    constexpr Vec2 OuterOffsets[] = {
        {-2.0f, 0.0f},
        {2.0f, 0.0f},
        {0.0f, -2.0f},
        {0.0f, 2.0f},
        {-1.5f, -1.5f},
        {1.5f, -1.5f},
        {-1.5f, 1.5f},
        {1.5f, 1.5f},
    };
    constexpr Vec2 InnerOffsets[] = {
        {-1.0f, 0.0f},
        {1.0f, 0.0f},
        {0.0f, -1.0f},
        {0.0f, 1.0f},
    };

    const Color outerGlow = withAlpha(mixColor(baseColor, {255, 255, 255, 255}, 0.62f), 34.0f + 112.0f * shine);
    const Color innerGlow = withAlpha({255, 255, 255, 255}, 46.0f + 154.0f * shine);

    for (const Vec2 offset : OuterOffsets) {
        renderer.drawText(pos + offset, star, outerGlow, scale);
    }
    for (const Vec2 offset : InnerOffsets) {
        renderer.drawText(pos + offset, star, innerGlow, scale);
    }
}

Vec2 drawRarityStars(Renderer& renderer, Vec2 pos, int rarity, float animationSeconds)
{
    constexpr std::string_view Star = "★";
    constexpr int StarScale = 2;
    constexpr float StarGap = 2.0f;
    const int clampedRarity = std::clamp(rarity, 1, 10);
    const Vec2 starSize = renderer.measureText(Star, StarScale);
    Vec2 cursor = pos;
    for (int i = 0; i < clampedRarity; ++i) {
        const float shine = rarityShineAmount(clampedRarity, i, animationSeconds);
        const Color baseColor = rarityBaseColor(clampedRarity, i, animationSeconds);
        const Color color = mixColor(baseColor, {255, 255, 255, 255}, shine * 0.88f);
        drawRarityStarGlow(renderer, cursor, Star, baseColor, shine, StarScale);
        if (clampedRarity >= 7) {
            renderer.drawOutlinedText(cursor, Star, color, {20, 16, 24, 150}, 2, StarScale);
        } else {
            renderer.drawText(cursor, Star, color, StarScale);
        }
        if (shine > 0.0f) {
            renderer.drawText(cursor + Vec2{-1.0f, -1.0f}, Star, withAlpha({255, 255, 255, 255}, 58.0f + 150.0f * shine), StarScale);
        }
        cursor.x += starSize.x + StarGap;
    }
    return {cursor.x - pos.x - StarGap, starSize.y};
}

float drawInventoryDetailHeader(
    Renderer& renderer,
    UiRect panel,
    std::string_view text,
    bool protectedItem,
    int rarity,
    float animationSeconds)
{
    constexpr float NameRarityGap = 3.0f;
    constexpr float RarityImageGap = 8.0f;
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
    const Vec2 titleSize = renderer.measureWrappedText(text, titleMaxWidth, TitleScale);
    if (protectedItem) {
        renderer.drawText(
            {content.pos.x + content.size.x - protectionSize.x, content.pos.y + 6.0f},
            protectionText,
            {255, 230, 150, 255},
            ProtectionScale);
    }

    const Vec2 raritySize = drawRarityStars(
        renderer,
        {content.pos.x, content.pos.y + titleSize.y + NameRarityGap},
        rarity,
        animationSeconds);
    return content.pos.y + titleSize.y + NameRarityGap + raritySize.y + RarityImageGap;
}

ObjectImageDrawOptions inventoryUiObjectImageOptions(
    const ItemData& item,
    bool broken,
    ObjectImageDrawOptions options = {})
{
    return objectGroundImageOptions(item, itemImageOptionsWithBrokenState(options, broken));
}

void drawInventoryDetailImage(Renderer& renderer, UiRect panel, float& y, const ItemData& item, bool broken)
{
    constexpr Vec2 ImageMaxSize{96.0f, 96.0f};
    constexpr float ImageBottomGap = 8.0f;
    const UiRect content = uiSubPanelContentRect(panel);
    const Vec2 center{
        content.pos.x + content.size.x * 0.5f,
        y + ImageMaxSize.y * 0.5f,
    };

    ObjectImageDrawOptions objectOptions;
    objectOptions.allowUpscale = true;
    objectOptions.applyScaleOverride = false;
    const bool drewImage = drawItemImage(
        renderer,
        item,
        center,
        ImageMaxSize,
        inventoryUiObjectImageOptions(item, broken, objectOptions));
    if (!drewImage) {
        renderer.fillCircle(center, ImageMaxSize.x * 0.35f, itemFallbackColorForBrokenState(inventoryUiObjectColor(item), broken));
    }
    y += ImageMaxSize.y + ImageBottomGap;
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

void drawExtraLines(
    Renderer& renderer,
    UiRect panel,
    float& y,
    const std::vector<InventoryUiDetailExtraLine>& extraLines,
    bool showSeparator)
{
    if (extraLines.empty()) {
        return;
    }
    if (showSeparator) {
        drawDetailSeparator(renderer, panel, y);
    }
    for (const InventoryUiDetailExtraLine& line : extraLines) {
        drawUiDetailLine(renderer, panel, y, line.label, line.value, line.valueColor);
    }
}

Color enhancementBadgeColor(EnhancementBadgeKind kind)
{
    switch (kind) {
    case EnhancementBadgeKind::Attack:
        return {238, 76, 72, 255};
    case EnhancementBadgeKind::Dig:
        return {76, 154, 246, 255};
    case EnhancementBadgeKind::Durability:
        return {238, 204, 72, 255};
    case EnhancementBadgeKind::Lighten:
        return {72, 174, 96, 255};
    case EnhancementBadgeKind::Enlarge:
        return {142, 92, 214, 255};
    }
    return ui::Text;
}

std::vector<EnhancementBadge> enhancementBadgesFor(const InventoryUiItemStats& stats)
{
    std::vector<EnhancementBadge> badges;
    if (stats.attackEnhanceLevel > 0) {
        badges.push_back({EnhancementBadgeKind::Attack, stats.attackEnhanceLevel});
    }
    if (stats.digEnhanceLevel > 0) {
        badges.push_back({EnhancementBadgeKind::Dig, stats.digEnhanceLevel});
    }
    if (stats.durabilityEnhanceLevel > 0) {
        badges.push_back({EnhancementBadgeKind::Durability, stats.durabilityEnhanceLevel});
    }
    if (stats.weightModifier < 0.999) {
        badges.push_back({EnhancementBadgeKind::Lighten, 1});
    }
    if (stats.sizeModifier > 1.001) {
        badges.push_back({EnhancementBadgeKind::Enlarge, 1});
    }
    return badges;
}

float enhancementBadgeWidth(const EnhancementBadge& badge)
{
    if (badge.kind == EnhancementBadgeKind::Lighten || badge.kind == EnhancementBadgeKind::Enlarge) {
        return 24.0f;
    }
    return badge.count >= 2 ? 36.0f : 24.0f;
}

void drawArrowEnhancementBadge(Renderer& renderer, UiRect rect, Color color, int count)
{
    const Vec2 center = rect.pos + rect.size * 0.5f;
    const float arrowX = count >= 2 ? rect.pos.x + 10.0f : center.x;
    const Color shadow{0, 0, 0, 120};
    const Color stemColor = mixColor(color, {255, 255, 255, color.a}, 0.08f);
    const std::array<Vec2, 3> shadowHead{{
        {arrowX, rect.pos.y + 4.0f},
        {arrowX - 6.0f, rect.pos.y + 12.0f},
        {arrowX + 6.0f, rect.pos.y + 12.0f},
    }};
    const std::array<Vec2, 3> head{{
        {arrowX, rect.pos.y + 3.0f},
        {arrowX - 6.0f, rect.pos.y + 11.0f},
        {arrowX + 6.0f, rect.pos.y + 11.0f},
    }};
    renderer.fillPolygon(shadowHead.data(), shadowHead.size(), shadow);
    renderer.fillRect({arrowX - 3.0f, rect.pos.y + 10.0f}, {6.0f, 11.0f}, shadow);
    renderer.fillPolygon(head.data(), head.size(), stemColor);
    renderer.fillRect({arrowX - 3.0f, rect.pos.y + 9.0f}, {6.0f, 11.0f}, stemColor);
    if (count >= 2) {
        const std::string countText = std::to_string(count);
        const Vec2 textSize = renderer.measureText(countText, 2);
        const Vec2 textPos{
            rect.pos.x + rect.size.x - textSize.x - 3.0f,
            rect.pos.y + (rect.size.y - textSize.y) * 0.5f,
        };
        renderer.drawText(textPos + Vec2{1.0f, 1.0f}, countText, {0, 0, 0, 170}, 2);
        renderer.drawText(textPos, countText, ui::Text, 2);
    }
}

void drawSquareEnhancementBadge(Renderer& renderer, UiRect rect, Color color, std::string_view text)
{
    renderer.fillRect(rect.pos, rect.size, color);
    const Vec2 textSize = renderer.measureText(text, 2);
    const Vec2 textPos{
        rect.pos.x + (rect.size.x - textSize.x) * 0.5f,
        rect.pos.y + (rect.size.y - textSize.y) * 0.5f,
    };
    renderer.drawText(textPos + Vec2{1.0f, 1.0f}, text, {0, 0, 0, 150}, 2);
    renderer.drawText(textPos, text, ui::Text, 2);
}

void drawEnhancementBadge(Renderer& renderer, UiRect rect, const EnhancementBadge& badge)
{
    const Color color = enhancementBadgeColor(badge.kind);
    switch (badge.kind) {
    case EnhancementBadgeKind::Attack:
    case EnhancementBadgeKind::Dig:
    case EnhancementBadgeKind::Durability:
        drawArrowEnhancementBadge(renderer, rect, color, badge.count);
        break;
    case EnhancementBadgeKind::Lighten:
        drawSquareEnhancementBadge(renderer, rect, color, "軽");
        break;
    case EnhancementBadgeKind::Enlarge:
        drawSquareEnhancementBadge(renderer, rect, color, "大");
        break;
    }
}

void drawInventoryEnhancementLine(Renderer& renderer, UiRect panel, float& y, const InventoryUiItemStats& stats)
{
    constexpr float BadgeHeight = 24.0f;
    constexpr float BadgeGap = 5.0f;
    constexpr float RowGap = 5.0f;
    const float labelX = panel.pos.x + ui::SubPanelPadding.x;
    const float valueX = labelX + DetailLabelWidth;
    const float valueMaxX = panel.pos.x + panel.size.x - ui::SubPanelPadding.x;
    renderer.drawText({labelX, y}, "強化", ui::TextMuted, 2);

    const std::vector<EnhancementBadge> badges = enhancementBadgesFor(stats);
    if (badges.empty()) {
        renderer.drawText({valueX, y}, "なし", ui::TextMuted, 2);
        y += DetailMinLineHeight;
        return;
    }

    float x = valueX;
    float rowY = y;
    float bottomY = y + BadgeHeight;
    for (const EnhancementBadge& badge : badges) {
        const float badgeWidth = enhancementBadgeWidth(badge);
        if (x > valueX && x + badgeWidth > valueMaxX) {
            x = valueX;
            rowY += BadgeHeight + RowGap;
        }
        drawEnhancementBadge(renderer, {{x, rowY - 3.0f}, {badgeWidth, BadgeHeight}}, badge);
        x += badgeWidth + BadgeGap;
        bottomY = std::max(bottomY, rowY + BadgeHeight);
    }
    y += std::max(DetailMinLineHeight, bottomY - y + DetailLineGap);
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
        instance.attackEnhanceLevel,
        instance.digEnhanceLevel,
        instance.durabilityEnhanceLevel,
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
        item.attackEnhanceLevel,
        item.digEnhanceLevel,
        item.durabilityEnhanceLevel,
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

std::string formatInventoryUiWeightText(const ItemData& item, const std::optional<InventoryUiItemStats>& stats)
{
    const double baseWeightKg = std::max(0.0, item.weightKg);
    const double weightModifier = stats ? stats->weightModifier : 1.0;
    const double effectiveWeightKg = std::max(0.0, baseWeightKg * weightModifier);

    char buffer[96];
    if (!stats || std::abs(weightModifier - 1.0) <= 0.001) {
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", effectiveWeightKg);
        return buffer;
    }

    std::snprintf(
        buffer,
        sizeof(buffer),
        "%.1fkg（元%.1fkg x%.0f%%）",
        effectiveWeightKg,
        baseWeightKg,
        weightModifier * 100.0);
    return buffer;
}

void drawInventoryUiEffectText(Renderer& renderer, UiRect panel, float& y, std::string_view text)
{
    drawUiDetailText(renderer, panel, y, text, InventoryEffectTextColor);
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
    if (isStaffObject(item)) {
        std::vector<std::string> equipmentLines = staffEquipmentEffectLines(item, catalog);
        std::vector<std::string> ringLines = sections.ringLines;
        applyStaffManualEquipmentEffectText(item, equipmentLines, ringLines.size());
        if (!equipmentLines.empty()) {
            drawInventoryUiEffectText(renderer, panel, y, "装備時効果");
            drawInventoryUiEffectText(renderer, panel, y, joinInventoryUiEffectLines(equipmentLines));
        }
        if (!ringLines.empty()) {
            drawInventoryUiEffectText(renderer, panel, y, "リングに乗せたときの効果");
            drawInventoryUiEffectText(renderer, panel, y, joinInventoryUiEffectLines(ringLines));
        }
        return;
    }
    if (!sections.useLines.empty()) {
        drawInventoryUiEffectText(renderer, panel, y, "使用時の効果");
        drawInventoryUiEffectText(renderer, panel, y, joinInventoryUiEffectLines(sections.useLines));
    }
    if (!sections.ringLines.empty()) {
        drawInventoryUiEffectText(renderer, panel, y, "リングに乗せたときの効果");
        drawInventoryUiEffectText(renderer, panel, y, joinInventoryUiEffectLines(sections.ringLines));
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
                    options.tint.a = style.text.a;
                    options.outlineColor.a = style.text.a;
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
                options.tint.a = style.text.a;
                options.outlineColor.a = style.text.a;
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

void drawInventoryUiItemIcon(
    Renderer& renderer,
    Vec2 center,
    const InventoryUiEntryView& entry,
    float imageMaxSize,
    bool selected,
    bool disabled,
    float alphaScale)
{
    if (entry.item == nullptr) {
        return;
    }

    alphaScale = std::clamp(alphaScale, 0.0f, 1.0f);
    const std::optional<InventoryUiItemStats> stats = inventoryUiEntryStats(entry);
    const bool broken = stats ? stats->broken : entry.item->durability == 0;
    const Color objectColor = itemFallbackColorForBrokenState(inventoryUiObjectColor(*entry.item), broken);

    ObjectImageDrawOptions imageOptions;
    imageOptions.tint = disabled ? Color{128, 128, 128, 255} : Color{255, 255, 255, 255};
    imageOptions.tint = scaleAlpha(imageOptions.tint, alphaScale);
    if (selected) {
        imageOptions = withSelectedItemOutline(imageOptions);
        imageOptions.outlineColor = scaleAlpha(imageOptions.outlineColor, alphaScale);
    }

    const bool drewImage = drawItemImage(
        renderer,
        *entry.item,
        center,
        {imageMaxSize, imageMaxSize},
        inventoryUiObjectImageOptions(*entry.item, broken, imageOptions));
    if (!drewImage) {
        Color fallback = disabled ? darkenColor(objectColor, 0.5f) : objectColor;
        fallback = scaleAlpha(fallback, alphaScale);
        renderer.fillCircle(center, 22.0f, fallback);
        if (selected) {
            drawSelectedItemCircleOutline(renderer, center, 22.0f);
        }
    }
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
    const float contentAlpha = std::clamp(style.contentAlpha, 0.0f, 1.0f);
    if (style.showFrame) {
        renderer.fillCircle(slotCenter, slotFrameRadius(rect), fill);
    }
    const auto drawBottomLabel = [&]() {
        drawInventoryUiSlotBottomLabel(renderer, rect, style.bottomLabel, scaleAlpha(style.bottomLabelColor, contentAlpha));
    };
    const auto drawTopRightCount = [&]() {
        if (entry.equipped || !style.showTopRightCount) {
            return;
        }
        constexpr int CountScale = 2;
        const std::string text = "\xC3\x97" + std::to_string(style.topRightCount);
        const Vec2 textSize = renderer.measureText(text, CountScale);
        const Vec2 textPos{
            rect.pos.x + rect.size.x - textSize.x - 5.0f,
            rect.pos.y + 3.0f,
        };
        renderer.drawOutlinedText(
            textPos,
            text,
            scaleAlpha(style.topRightCountColor, contentAlpha),
            scaleAlpha({0, 0, 0, 120}, contentAlpha),
            6,
            CountScale);
    };
    const auto drawEquippedLabel = [&]() {
        if (!entry.equipped) {
            return;
        }
        constexpr int LabelScale = 3;
        const std::string_view text = "E";
        const Vec2 textSize = renderer.measureText(text, LabelScale);
        const Vec2 textPos{
            rect.pos.x + rect.size.x - textSize.x - 5.0f,
            rect.pos.y + 3.0f,
        };
        renderer.drawOutlinedText(
            textPos,
            text,
            scaleAlpha(Color{90, 230, 120, 255}, contentAlpha),
            scaleAlpha({0, 0, 0, 140}, contentAlpha),
            6,
            LabelScale);
    };
    const auto drawProtectionLabel = [&]() {
        if (!style.showProtectionLabel || !stats || !stats->protectionEnabled) {
            return;
        }
        constexpr int LabelScale = 2;
        renderer.drawOutlinedText(
            rect.pos + Vec2{5.0f, 3.0f},
            "保護",
            scaleAlpha(style.protectionLabelColor, contentAlpha),
            scaleAlpha({0, 0, 0, 120}, contentAlpha),
            6,
            LabelScale);
    };

    if (entry.item == nullptr) {
        drawBottomLabel();
        drawTopRightCount();
        drawEquippedLabel();
        return;
    }

    const bool broken = stats ? stats->broken : entry.item->durability == 0;

    if (!stats) {
        drawInventoryUiItemIcon(renderer, slotCenter, entry, style.imageMaxSize, style.selected, style.disabled, contentAlpha);
        drawBottomLabel();
        drawTopRightCount();
        drawEquippedLabel();
        return;
    }

    (void)broken;
    drawInventoryUiItemIcon(renderer, slotCenter, entry, style.imageMaxSize, style.selected, style.disabled, contentAlpha);
    drawProtectionLabel();
    drawBottomLabel();
    drawTopRightCount();
    drawEquippedLabel();
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

int inventoryUiGridRowCount(int itemCount, const InventoryUiGridStyle& style)
{
    const int columns = std::max(1, style.columns);
    return itemCount <= 0 ? 0 : (itemCount + columns - 1) / columns;
}

float inventoryUiGridWidth(const InventoryUiGridStyle& style)
{
    const int columns = std::max(1, style.columns);
    return static_cast<float>(columns) * style.slotSize.x + static_cast<float>(columns - 1) * style.slotGap.x;
}

float inventoryUiGridVisibleHeight(const InventoryUiGridStyle& style)
{
    const int rows = std::max(1, style.visibleRows);
    return static_cast<float>(rows) * style.slotSize.y + static_cast<float>(rows - 1) * style.slotGap.y;
}

UiRect inventoryUiGridViewport(Vec2 pos, const InventoryUiGridStyle& style)
{
    const float scrollbarReserve = std::max(
        0.0f,
        style.scroll.scrollbarWidth + style.scroll.scrollbarGap + style.scroll.scrollbarPaddingX);
    return {pos, {inventoryUiGridWidth(style) + scrollbarReserve, inventoryUiGridVisibleHeight(style)}};
}

float inventoryUiGridContentHeight(int itemCount, const InventoryUiGridStyle& style)
{
    const int rows = inventoryUiGridRowCount(itemCount, style);
    if (rows <= 0) {
        return 0.0f;
    }
    return static_cast<float>(rows) * style.slotSize.y + static_cast<float>(rows - 1) * style.slotGap.y;
}

UiScrollAreaLayout makeInventoryUiGridLayout(
    UiRect viewport,
    int itemCount,
    float scrollOffset,
    const InventoryUiGridStyle& style)
{
    return makeUiScrollAreaLayout(viewport, inventoryUiGridContentHeight(itemCount, style), scrollOffset, style.scroll);
}

UiScrollAreaLayout updateInventoryUiGrid(
    UiContext& ui,
    const Input& input,
    UiRect viewport,
    int itemCount,
    float& scrollOffset,
    const InventoryUiGridStyle& style,
    UiScrollAreaState* state)
{
    return updateUiScrollArea(
        ui,
        input,
        viewport,
        inventoryUiGridContentHeight(itemCount, style),
        scrollOffset,
        style.scroll,
        state);
}

UiRect inventoryUiGridSlotRect(const UiScrollAreaLayout& layout, int index, const InventoryUiGridStyle& style)
{
    const int columns = std::max(1, style.columns);
    const int clampedIndex = std::max(0, index);
    const int row = clampedIndex / columns;
    const int column = clampedIndex % columns;
    return {{
        layout.content.pos.x + static_cast<float>(column) * (style.slotSize.x + style.slotGap.x),
        layout.content.pos.y + static_cast<float>(row) * (style.slotSize.y + style.slotGap.y) - layout.scrollOffset,
    }, style.slotSize};
}

void keepInventoryUiGridItemVisible(
    UiRect viewport,
    int selectedIndex,
    int itemCount,
    float& scrollOffset,
    const InventoryUiGridStyle& style)
{
    if (selectedIndex < 0 || selectedIndex >= itemCount) {
        scrollOffset = makeInventoryUiGridLayout(viewport, itemCount, scrollOffset, style).scrollOffset;
        return;
    }
    const UiScrollAreaLayout layout = makeInventoryUiGridLayout(viewport, itemCount, scrollOffset, style);
    keepUiScrollAreaRectVisible(
        viewport,
        inventoryUiGridSlotRect(layout, selectedIndex, style),
        inventoryUiGridContentHeight(itemCount, style),
        scrollOffset,
        style.scroll);
}

void drawInventoryUiGrid(
    Renderer& renderer,
    const UiScrollAreaLayout& layout,
    std::span<const InventoryUiEntryView> entries,
    int selectedIndex,
    const InventoryUiGridStyle& style)
{
    renderer.pushClipRect(layout.viewport.pos, layout.viewport.size);
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const UiRect rect = inventoryUiGridSlotRect(layout, i, style);
        if (!uiScrollAreaRectVisible(layout, rect)) {
            continue;
        }
        drawInventoryUiSlot(renderer, rect, entries[static_cast<std::size_t>(i)], i == selectedIndex, style.imageMaxSize);
    }
    renderer.popClipRect();
    drawUiScrollAreaScrollbar(renderer, layout, style.scroll);
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
    float detailLineY = drawInventoryDetailHeader(
        renderer,
        panel,
        detailTitle,
        stats && stats->protectionEnabled,
        entry.item->rarity,
        options.animationSeconds);

    const bool broken = stats ? stats->broken : entry.item->durability == 0;
    drawInventoryDetailImage(renderer, panel, detailLineY, *entry.item, broken);
    drawUiDetailText(renderer, panel, detailLineY, entry.item->description.empty() ? "-" : entry.item->description);
    drawItemEffectDetailSections(renderer, panel, detailLineY, *entry.item, catalog, encyclopedia);

    if (!entry.item->category.empty()) {
        drawUiDetailLine(renderer, panel, detailLineY, "分類", entry.item->category);
    }

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

    drawUiDetailLine(renderer, panel, detailLineY, "重量", formatInventoryUiWeightText(*entry.item, stats));

    if (options.showEnhanceCount && stats) {
        drawInventoryEnhancementLine(renderer, panel, detailLineY, *stats);
    }

    drawExtraLines(renderer, panel, detailLineY, extraLines, options.showExtraLineSeparator);
}

}
