#include "game/GameInternal.hpp"

#include "engine/InputHelpGlyph.hpp"
#include "game/EnemyImageRenderer.hpp"
#include "game/EntityStatusVisuals.hpp"
#include "game/ExplosionWarning.hpp"
#include "game/PlayerEquipmentVisual.hpp"
#include "game/RingDisplayName.hpp"

#include <cctype>
#include <vector>

namespace majo {

namespace {

float easeOutCubic(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float easeOutBack(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    constexpr float C1 = 1.70158f;
    constexpr float C3 = C1 + 1.0f;
    const float shifted = t - 1.0f;
    return 1.0f + C3 * shifted * shifted * shifted + C1 * shifted * shifted;
}

float smootherStep(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

constexpr float PlayerDamageVignetteMinAlpha = 1.0f;
constexpr float PlayerDamageVignetteMaxAlpha = 132.0f;
constexpr float PlayerDamageVignetteMinEdgeWidth = 86.0f;
constexpr float PlayerDamageVignetteMaxEdgeWidth = 198.0f;
constexpr std::string_view DungeonStatusHudImagePath = "assets/UI_status.png";
constexpr std::string_view DungeonStatusHudGaugeMaskPath = "assets/UI_status_gaugeMask.png";
constexpr Vec2 DungeonStatusHudImageSize{376.0f, 344.0f};
constexpr float DungeonStatusHudPinchDanger = 0.75f;
constexpr Vec2 DungeonStatusHudHpGaugeCenter{230.0f, 108.0f};
constexpr Vec2 DungeonStatusHudHpGaugeRadius{96.0f, 92.0f};
constexpr float DungeonStatusHudHpGaugeStartAngle = -2.70f;
constexpr float DungeonStatusHudHpGaugeSweepAngle = 4.20f;
constexpr RectF DungeonStatusHudExpGaugeSource{105.0f, 240.0f, 168.0f, 68.0f};
constexpr RectF DungeonStatusHudHpLabelRect{190.0f, 62.0f, 54.0f, 28.0f};
constexpr RectF DungeonStatusHudHpValueRect{164.0f, 92.0f, 98.0f, 28.0f};
constexpr RectF DungeonStatusHudLevelTextRect{24.0f, 118.0f, 116.0f, 82.0f};
constexpr RectF DungeonStatusHudExpTextRect{122.0f, 196.0f, 136.0f, 88.0f};
constexpr Vec2 DungeonStatusHudTextOffset{8.0f, 7.0f};
constexpr Color DungeonStatusHudLabelColor{255, 230, 122, 255};
constexpr Color DungeonStatusHudTextColor{255, 255, 255, 255};
constexpr Color DungeonStatusHudPinchTextColor{255, 226, 78, 255};
constexpr Color DungeonStatusHudHpDangerTint{255, 82, 74, 255};

Vec2 dungeonStatusHudImageToScreen(Vec2 origin, float scale, Vec2 imagePoint)
{
    return origin + imagePoint * scale;
}

Vec2 dungeonStatusHudTexCoord(Vec2 imagePoint)
{
    return {
        imagePoint.x / DungeonStatusHudImageSize.x,
        imagePoint.y / DungeonStatusHudImageSize.y,
    };
}

Vec2 dungeonStatusHudRectCenter(RectF rect)
{
    return {rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f};
}

void drawCenteredDungeonStatusText(
    Renderer& renderer,
    Vec2 origin,
    float imageScale,
    RectF imageRect,
    std::string_view text,
    Color color,
    int textScale)
{
    const Vec2 screenCenter = dungeonStatusHudImageToScreen(
        origin,
        imageScale,
        dungeonStatusHudRectCenter(imageRect) + DungeonStatusHudTextOffset);
    const Vec2 textSize = renderer.measureText(text, textScale);
    renderer.drawText(screenCenter - textSize * 0.5f, text, color, textScale);
}

void drawTwoLineDungeonStatusText(
    Renderer& renderer,
    Vec2 origin,
    float imageScale,
    RectF imageRect,
    std::string_view label,
    std::string_view value,
    int labelScale,
    int valueScale)
{
    constexpr float LineGap = 2.0f;
    const Vec2 labelSize = renderer.measureText(label, labelScale);
    const Vec2 valueSize = renderer.measureText(value, valueScale);
    const Vec2 screenCenter = dungeonStatusHudImageToScreen(
        origin,
        imageScale,
        dungeonStatusHudRectCenter(imageRect) + DungeonStatusHudTextOffset);
    const float totalHeight = labelSize.y + LineGap + valueSize.y;
    const float labelY = screenCenter.y - totalHeight * 0.5f;
    const float valueY = labelY + labelSize.y + LineGap;
    renderer.drawText({screenCenter.x - labelSize.x * 0.5f, labelY}, label, DungeonStatusHudLabelColor, labelScale);
    renderer.drawText({screenCenter.x - valueSize.x * 0.5f, valueY}, value, DungeonStatusHudTextColor, valueScale);
}

void drawDungeonStatusHudHpGauge(
    Renderer& renderer,
    ImageHandle mask,
    Vec2 origin,
    float imageScale,
    float progress,
    Color tint)
{
    progress = clamp(progress, 0.0f, 1.0f);
    if (!mask.valid() || progress <= 0.0f) {
        return;
    }

    const float missingAngle = (1.0f - progress) * DungeonStatusHudHpGaugeSweepAngle;
    const float sweepAngle = progress * DungeonStatusHudHpGaugeSweepAngle;
    const float startAngle = DungeonStatusHudHpGaugeStartAngle + missingAngle;
    const int segments = std::max(3, static_cast<int>(std::ceil(sweepAngle / (Pi / 36.0f))));

    std::vector<ImageTriangleVertex> vertices;
    std::vector<int> indices;
    vertices.reserve(static_cast<std::size_t>(segments) + 2);
    indices.reserve(static_cast<std::size_t>(segments) * 3);

    vertices.push_back({
        dungeonStatusHudImageToScreen(origin, imageScale, DungeonStatusHudHpGaugeCenter),
        dungeonStatusHudTexCoord(DungeonStatusHudHpGaugeCenter),
    });
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = startAngle + sweepAngle * t;
        const Vec2 imagePoint{
            DungeonStatusHudHpGaugeCenter.x + std::cos(angle) * DungeonStatusHudHpGaugeRadius.x,
            DungeonStatusHudHpGaugeCenter.y + std::sin(angle) * DungeonStatusHudHpGaugeRadius.y,
        };
        vertices.push_back({
            dungeonStatusHudImageToScreen(origin, imageScale, imagePoint),
            dungeonStatusHudTexCoord(imagePoint),
        });
    }
    for (int i = 1; i <= segments; ++i) {
        indices.push_back(0);
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    renderer.drawImageTriangleList(mask, vertices.data(), vertices.size(), indices.data(), indices.size(), tint);
}

void drawDungeonStatusHudExpGauge(
    Renderer& renderer,
    ImageHandle mask,
    Vec2 origin,
    float imageScale,
    float progress)
{
    progress = clamp(progress, 0.0f, 1.0f);
    if (!mask.valid() || progress <= 0.0f) {
        return;
    }

    RectF source = DungeonStatusHudExpGaugeSource;
    source.w *= progress;
    const Vec2 drawSize{source.w * imageScale, source.h * imageScale};
    const Vec2 center = dungeonStatusHudImageToScreen(
        origin,
        imageScale,
        {source.x + source.w * 0.5f, source.y + source.h * 0.5f});
    ImageDrawOptions options;
    options.tint = {255, 255, 255, 255};
    renderer.drawImageRegion(mask, source, center, drawSize, options);
}

float dotVec2(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

float stunWakeHopOffset(float stunWakeTimer)
{
    if (stunWakeTimer <= 0.0f) {
        return 0.0f;
    }
    constexpr float StunWakeHopSeconds = 0.18f;
    constexpr float StunWakeHopPixels = 8.0f;
    const float t = clamp(stunWakeTimer / StunWakeHopSeconds, 0.0f, 1.0f);
    return std::sin(t * Pi) * StunWakeHopPixels;
}

Color magicAuraColor(std::string_view damageType)
{
    if (damageType == "fire") {
        return {255, 116, 32, 210};
    }
    if (damageType == "ice") {
        return {116, 214, 255, 210};
    }
    if (damageType == "thunder") {
        return {255, 232, 80, 220};
    }
    if (damageType == "wind") {
        return {138, 238, 178, 190};
    }
    if (damageType == "earth") {
        return {164, 120, 70, 215};
    }
    return {220, 220, 255, 190};
}

std::string signedPercentShort(double multiplier)
{
    const int percent = static_cast<int>(std::round((multiplier - 1.0) * 100.0));
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%+d%%", percent);
    return buffer;
}

std::string signedWeightShort(double value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%+.1fkg", value);
    return buffer;
}

float worldDistanceToMeters(float distance)
{
    return distance / static_cast<float>(balance::TileSize);
}

float linearMetersPerSecondForAngularSpeed(float angularSpeed, float radius)
{
    return angularSpeed * worldDistanceToMeters(radius);
}

std::string dungeonDepthTopInfoEntry(const DungeonLayout& layout, Vec2 tilePosition)
{
    char buffer[32];
    const int meters = std::max(0, static_cast<int>(std::lround(projectedDungeonRouteDistanceTiles(layout, tilePosition))));
    std::snprintf(buffer, sizeof(buffer), "深度 %dm", meters);
    return buffer;
}

std::string dungeonWarpTopInfoEntry(int discovered, int total)
{
    if (total <= 0) {
        return {};
    }

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d/%d", std::clamp(discovered, 0, total), total);
    return inlineWorldIconTag(worldIconKey(WorldIconId::WarpPoint)) + std::string(buffer);
}

bool equipmentTargetAppliesToRing(std::string_view target, int ringIndex)
{
    if (target == "equip_all") {
        return true;
    }
    if (target == "equip_ring1") {
        return ringIndex == 0;
    }
    if (target == "equip_ring2") {
        return ringIndex == 1;
    }
    if (target == "equip_ring3") {
        return ringIndex == 2;
    }
    return false;
}

bool staffEquipmentHasRingTarget(const EquipmentModifiers& modifiers, int ringIndex)
{
    return std::any_of(
        modifiers.sources.begin(),
        modifiers.sources.end(),
        [ringIndex](const EquipmentModifierSource& source) {
            return equipmentTargetAppliesToRing(source.target, ringIndex);
        });
}

std::string percentModifierSuffix(double multiplier)
{
    if (std::fabs(multiplier - 1.0) < 0.005) {
        return {};
    }
    return "（" + signedPercentShort(multiplier) + "）";
}

std::string weightModifierSuffix(double value)
{
    if (std::fabs(value) < 0.05) {
        return {};
    }
    return "（" + signedWeightShort(value) + "）";
}

std::string ringWeightStateLabel(float weightRatio, float stopRatio)
{
    if (weightRatio < 0.5f) {
        return "重量余裕あり";
    }
    if (weightRatio < 0.85f) {
        return "重量あと少し";
    }
    if (weightRatio < 1.0f) {
        return "重量いっぱい";
    }
    if (weightRatio < stopRatio) {
        return "重量オーバー";
    }
    return "超重量オーバー";
}

std::string ringWeightSpeedStateLine(float speedMultiplier)
{
    if (speedMultiplier <= 0.005f) {
        return "リング停止中";
    }

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "リング速度 ×%.2f倍", speedMultiplier);
    return buffer;
}

void drawRingDetailLineWithModifier(
    Renderer& renderer,
    UiRect panel,
    float& y,
    std::string_view label,
    std::string value,
    std::string_view modifier = {},
    Color valueColor = ui::Text,
    Color labelColor = ui::TextMuted)
{
    constexpr float LabelWidth = 106.0f;
    constexpr float MinLineHeight = 31.0f;
    constexpr float LineGap = 4.0f;
    constexpr float ModifierGap = 7.0f;
    constexpr int TextScale = 2;
    constexpr Color ModifierColor{255, 230, 150, 255};

    const float labelX = panel.pos.x + ui::SubPanelPadding.x;
    const float valueX = labelX + LabelWidth;
    const float right = panel.pos.x + panel.size.x - ui::SubPanelPadding.x;
    const float modifierWidth = modifier.empty() ? 0.0f : renderer.measureText(modifier, TextScale).x + ModifierGap;
    const float valueMaxWidth = std::max(0.0f, right - valueX - modifierWidth);
    const std::string fittedValue = fittedSingleLineText(renderer, std::move(value), valueMaxWidth, TextScale);

    renderer.drawText({labelX, y}, label, labelColor, TextScale);
    renderer.drawText({valueX, y}, fittedValue, valueColor, TextScale);
    if (!modifier.empty()) {
        const float suffixX = valueX + renderer.measureText(fittedValue, TextScale).x + ModifierGap;
        renderer.drawText({suffixX, y}, modifier, ModifierColor, TextScale);
    }

    const float lineWidth = renderer.measureText(fittedValue, TextScale).x + modifierWidth;
    y += std::max(
        MinLineHeight,
        renderer.measureWrappedText(fittedValue, std::max(1.0f, std::max(valueMaxWidth, lineWidth)), TextScale).y + LineGap);
}

void drawRingDetailPanel(
    Renderer& renderer,
    UiRect panel,
    const SpellRingSystem& spellRing,
    const EquipmentModifiers& modifiers,
    const RuntimeBalance& balance,
    int ringIndex)
{
    ringIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    drawUiSubPanel(renderer, panel);

    const RingEquipmentModifiers& ringModifiers = ringEquipmentModifiersForRing(modifiers, ringIndex);
    const std::vector<SpellRingItem>& items = spellRing.itemsForRing(ringIndex);
    float y = drawUiDetailHeader(renderer, panel, "リング " + std::to_string(ringIndex + 1));

    char buffer[96];
    drawRingDetailLineWithModifier(
        renderer,
        panel,
        y,
        "形状",
        ringShapeDisplayName(spellRing.ringShapeForIndex(ringIndex)));

    std::snprintf(buffer, sizeof(buffer), "%02d/%02d", static_cast<int>(items.size()), spellRing.maxItemCountForRing(ringIndex));
    drawRingDetailLineWithModifier(renderer, panel, y, "装着", buffer);

    const float orbitRadius = spellRing.orbitRadiusForRing(ringIndex);
    std::snprintf(buffer, sizeof(buffer), "%.2fm", worldDistanceToMeters(orbitRadius));
    drawRingDetailLineWithModifier(
        renderer,
        panel,
        y,
        "半径",
        buffer,
        percentModifierSuffix(ringModifiers.ringRadiusMul));

    std::snprintf(
        buffer,
        sizeof(buffer),
        "%.2fm/s",
        linearMetersPerSecondForAngularSpeed(
            spellRing.ringAngularSpeedForIndex(ringIndex, balance),
            orbitRadius));
    drawRingDetailLineWithModifier(
        renderer,
        panel,
        y,
        "速度",
        buffer,
        percentModifierSuffix(ringModifiers.ringSpeedMul));

    std::snprintf(
        buffer,
        sizeof(buffer),
        "%.1f/%.1fkg",
        spellRing.totalEquippedWeightForRing(ringIndex),
        spellRing.maxEquippedWeightForRing(ringIndex));
    drawRingDetailLineWithModifier(
        renderer,
        panel,
        y,
        "重量",
        buffer,
        weightModifierSuffix(ringModifiers.ringWeightLimitAdd));

    const float totalWeight = spellRing.totalEquippedWeightForRing(ringIndex);
    const float maxWeight = spellRing.maxEquippedWeightForRing(ringIndex);
    const float weightRatio = maxWeight > 0.0f
        ? totalWeight / maxWeight
        : (totalWeight > 0.0f ? SpellRingSystem::MaxWeightStopRatio : 0.0f);
    const float stopRatio = spellRing.weightStopRatioForRing(ringIndex);
    const float weightSpeedMultiplier = spellRing.weightSpeedMultiplierForRing(ringIndex);
    constexpr Color OverloadedWeightColor{255, 112, 112, 255};
    const bool overloaded = weightRatio >= 1.0f;
    const Color stateColor = overloaded ? OverloadedWeightColor : ui::Text;
    drawRingDetailLineWithModifier(
        renderer,
        panel,
        y,
        "状態",
        ringWeightStateLabel(weightRatio, stopRatio),
        {},
        stateColor,
        overloaded ? OverloadedWeightColor : ui::TextMuted);
    if (overloaded) {
        drawRingDetailLineWithModifier(
            renderer,
            panel,
            y,
            "",
            ringWeightSpeedStateLine(weightSpeedMultiplier),
            {},
            OverloadedWeightColor,
            OverloadedWeightColor);
    }

}

void drawMagicAura(Renderer& renderer, Vec2 center, float radius, std::string_view damageType, float totalSeconds)
{
    const Color color = magicAuraColor(damageType);
    const float pulse = 0.5f + 0.5f * std::sin(totalSeconds * 16.0f);
    renderer.drawCircle(center, radius + 4.0f + pulse * 2.0f, withAlpha(color, 125.0f));
    if (damageType == "fire") {
        renderer.fillSoftCircle(center, radius + 8.0f, withAlpha(color, 54.0f));
        renderer.fillCircle(center + Vec2{2.0f, -4.0f}, std::max(2.0f, radius * 0.28f), {255, 224, 84, 180});
    } else if (damageType == "ice") {
        renderer.drawLine(center + Vec2{-radius, 0.0f}, center + Vec2{radius, 0.0f}, withAlpha(color, 170.0f));
        renderer.drawLine(center + Vec2{0.0f, -radius}, center + Vec2{0.0f, radius}, withAlpha(color, 140.0f));
    } else if (damageType == "thunder") {
        renderer.drawLine(center + Vec2{-radius, -radius * 0.5f}, center + Vec2{0.0f, radius * 0.1f}, {255, 250, 180, 210});
        renderer.drawLine(center + Vec2{0.0f, radius * 0.1f}, center + Vec2{radius, radius * 0.45f}, {255, 250, 180, 210});
    } else if (damageType == "wind") {
        renderer.drawSoftRing(center, radius + 6.0f, 3.0f, withAlpha(color, 72.0f));
    } else if (damageType == "earth") {
        for (int i = 0; i < 3; ++i) {
            const float angle = totalSeconds * 3.2f + static_cast<float>(i) * (Pi * 2.0f / 3.0f);
            renderer.fillCircle(center + fromAngle(angle) * (radius + 7.0f), 2.4f, withAlpha(color, 190.0f));
        }
    }
}

float magicAuraLightRadius(std::string_view damageType, float hitRadius)
{
    const float base = std::max(8.0f, hitRadius) * 2.0f;
    if (damageType == "fire") {
        return 42.0f + base * 0.75f;
    }
    if (damageType == "thunder") {
        return 92.0f + base;
    }
    if (damageType == "ice") {
        return 62.0f + base;
    }
    if (damageType == "wind") {
        return 68.0f + base;
    }
    if (damageType == "earth") {
        return 70.0f + base;
    }
    return 58.0f + base;
}

std::string firstItemAcquisitionUseEffectSummary(const ObjectDefinition& object)
{
    std::vector<std::string> lines;
    for (const DiscoveryEffectLine& line : object.discoveryEffectLines) {
        if (line.trigger == DiscoveryTrigger::NormalEffect && !line.text.empty()) {
            lines.push_back(line.text);
        }
    }
    if (lines.empty()) {
        return {};
    }

    std::string text;
    constexpr std::size_t MaxLines = 3;
    const std::size_t count = std::min(lines.size(), MaxLines);
    for (std::size_t i = 0; i < count; ++i) {
        if (!text.empty()) {
            text += " / ";
        }
        text += lines[i];
    }
    if (lines.size() > MaxLines) {
        text += " / ...";
    }
    return text;
}

UiRect firstItemAcquisitionNoticeBodyRect(UiRect panel)
{
    constexpr float BodyMarginX = 30.0f;
    constexpr float BodyTopOffset = 94.0f;
    constexpr float BodyBottomGap = 20.0f;
    const float y = panel.pos.y + BodyTopOffset;
    return {{
        panel.pos.x + BodyMarginX,
        y,
    }, {
        panel.size.x - BodyMarginX * 2.0f,
        std::max(0.0f, firstItemAcquisitionOkButtonRect(panel).pos.y - y - BodyBottomGap),
    }};
}

UiRect firstItemAcquisitionNoticeImagePanelRect(UiRect panel)
{
    constexpr Vec2 ImagePanelSize{136.0f, 136.0f};
    return {firstItemAcquisitionNoticeBodyRect(panel).pos, ImagePanelSize};
}

Vec2 firstItemAcquisitionNoticeImageCenter(UiRect imagePanel)
{
    return imagePanel.pos + imagePanel.size * 0.5f + Vec2{0.0f, -3.0f};
}

void drawDetectionBadge(Renderer& renderer, Vec2 anchor, Color color, int index, float visualScale, float alphaScale)
{
    const float scale = std::max(0.85f, visualScale);
    const Vec2 center = anchor + Vec2{12.0f + static_cast<float>(index) * 17.0f, -18.0f} * scale;
    const Color fill = withAlpha(color, 220.0f * alphaScale);
    const Vec2 tail[] = {
        center + Vec2{-4.5f, 7.0f} * scale,
        center + Vec2{5.5f, 6.0f} * scale,
        anchor + Vec2{3.0f + static_cast<float>(index) * 2.0f, -4.0f} * scale,
    };
    renderer.fillPolygon(tail, 3, fill);
    renderer.fillCircle(center, 10.5f * scale, fill);

    constexpr std::string_view Mark = "!";
    constexpr int TextScale = 2;
    const Vec2 textSize = renderer.measureText(Mark, TextScale, TextStyle::Italic);
    renderer.drawText(
        center - textSize * 0.5f + Vec2{-0.5f, -1.0f} * scale,
        Mark,
        withAlpha({255, 255, 255, 255}, 255.0f * alphaScale),
        TextScale,
        TextStyle::Italic);
}

void drawDetectionBadges(Renderer& renderer, const SpellRingItem& item, Vec2 anchor, float visualScale = 1.0f, float alphaScale = 1.0f)
{
    int index = 0;
    if (item.hiddenDetectionRadius > 0.0f) {
        drawDetectionBadge(renderer, anchor, {126, 208, 255, 255}, index, visualScale, alphaScale);
    }
}

ExplosionWarningVisual ringItemBreakExplosionWarningVisual(const SpellRingItem& item)
{
    if (!item.breakExplosion.active) {
        return {};
    }
    return explosionWarningVisual(item.breakExplosion.initialDelay, item.breakExplosion.delay);
}

void drawRingItemBreakExplosionWarningAura(
    Renderer& renderer,
    Vec2 position,
    float visualRadius,
    const ExplosionWarningVisual& warning)
{
    if (!warning.active) {
        return;
    }

    const float auraRadius = visualRadius * (1.70f + warning.urgency * 0.48f + warning.pulse * 0.24f);
    const float outerRadius = visualRadius * (1.22f + warning.urgency * 0.22f + warning.pulse * 0.15f);
    renderer.fillSoftCircle(position, auraRadius, withAlpha({255, 36, 28, 255}, 78.0f * warning.intensity));
    renderer.fillSoftCircle(position, outerRadius, withAlpha({255, 86, 42, 255}, 54.0f + 88.0f * warning.intensity));

    const float ringRadius = visualRadius + 5.0f + warning.urgency * 5.0f + warning.pulse * (3.0f + warning.urgency * 3.5f);
    renderer.drawCircle(position, ringRadius, withAlpha({255, 54, 38, 255}, 58.0f + 116.0f * warning.intensity));
    renderer.drawCircle(position, ringRadius + 3.5f + warning.pulse * 2.5f, withAlpha({255, 132, 76, 255}, 40.0f + 76.0f * warning.intensity));
}

void drawRingItemBreakExplosionWarningOverlay(
    Renderer& renderer,
    Vec2 position,
    float visualRadius,
    const ExplosionWarningVisual& warning)
{
    if (!warning.active) {
        return;
    }

    const float coreRadius = visualRadius * (0.82f + warning.pulse * 0.12f);
    renderer.fillSoftCircle(position, coreRadius, withAlpha({255, 24, 24, 255}, 44.0f + 118.0f * warning.intensity));
    renderer.drawCircle(position, visualRadius + 2.0f + warning.pulse * 2.5f, withAlpha({255, 40, 32, 255}, 86.0f + 126.0f * warning.intensity));
    renderer.drawCircle(position, visualRadius * (0.54f + warning.pulse * 0.10f), withAlpha({255, 214, 178, 255}, 48.0f + 82.0f * warning.intensity));

    const float rayAngle = warning.elapsed * (2.8f + warning.urgency * 5.0f);
    const float rayInner = visualRadius * (0.42f + warning.pulse * 0.08f);
    const float rayOuter = visualRadius * (1.14f + warning.urgency * 0.24f + warning.pulse * 0.16f);
    const Color rayColor = withAlpha({255, 76, 52, 255}, 46.0f + 92.0f * warning.intensity);
    for (int i = 0; i < 4; ++i) {
        const Vec2 ray = fromAngle(rayAngle + static_cast<float>(i) * Pi * 0.5f);
        renderer.drawLine(position + ray * rayInner, position + ray * rayOuter, rayColor);
    }
}

void drawWarpGuideMinimapIcon(Renderer& renderer, Vec2 center, Vec2 direction, float pulse)
{
    direction = normalize(direction);
    const Vec2 tangent{-direction.y, direction.x};
    const float glowScale = 0.72f + 0.28f * pulse;

    renderer.fillSoftCircle(center, 20.0f + 3.0f * pulse, withAlpha({255, 202, 70, 255}, 76.0f * glowScale));
    renderer.drawSoftRing(center, 11.0f + 1.6f * pulse, 6.4f, withAlpha({255, 224, 92, 255}, 118.0f * glowScale));
    renderer.fillCircle(center, 10.5f, {14, 18, 28, 205});
    renderer.drawCircle(center, 11.2f, {255, 228, 106, 245});
    renderer.drawCircle(center, 8.0f, {118, 214, 255, 108});

    const Vec2 tip = center + direction * 8.2f;
    const Vec2 shoulder = center - direction * 2.4f;
    const Vec2 tail = center - direction * 7.0f;
    const Vec2 arrow[] = {
        tip,
        shoulder + tangent * 5.3f,
        tail,
        shoulder - tangent * 5.3f,
    };
    renderer.fillPolygon(arrow, 4, {255, 216, 68, 248});

    const Vec2 highlight[] = {
        tip - direction * 1.6f,
        center + tangent * 2.0f,
        center + direction * 2.8f - tangent * 0.8f,
    };
    renderer.fillPolygon(highlight, 3, {255, 252, 194, 222});
    renderer.drawLine(tail + tangent * 3.2f, shoulder + tangent * 5.3f, {86, 58, 32, 155});
    renderer.drawLine(tail - tangent * 3.2f, shoulder - tangent * 5.3f, {86, 58, 32, 155});
    renderer.fillCircle(center + direction * 4.0f, 2.0f, {255, 255, 226, 235});
}

constexpr Color DungeonMapExitLadderColor{242, 236, 188, 250};
constexpr Color DungeonMapDiscoveredWarpColor{86, 238, 218, 235};
constexpr Color DungeonMapUndiscoveredWarpColor{34, 64, 126, 220};
constexpr Color DungeonMapEnemyColor{238, 72, 82, 238};
constexpr Color DungeonMapBossEnemyColor{255, 108, 64, 246};
constexpr Color DungeonMapChestColor{246, 198, 50, 238};
constexpr Color DungeonMapCrateColor{144, 86, 42, 238};
constexpr Color DungeonMapEventColor{246, 98, 206, 238};
constexpr Color DungeonMapItemColor{248, 214, 64, 238};
constexpr Color DungeonMapMaterialColor{72, 210, 188, 238};

Vec2 dungeonMapDropPosition(const WorldDropItem& drop)
{
    return drop.jumpActive ? drop.jumpTargetPosition : drop.position;
}

bool dungeonEventVisibleOnMap(const Game::DungeonEventInstance& event)
{
    return event.discovered && !event.completed;
}

void drawDungeonMapCircleMarker(Renderer& renderer, Vec2 center, float radius, Color fill)
{
    renderer.fillCircle(center, radius, fill);
}

void drawDungeonMapSquareMarker(Renderer& renderer, Vec2 center, float size, Color fill)
{
    const Vec2 markerSize{size, size};
    renderer.fillRect(center - markerSize * 0.5f, markerSize, fill);
}

void drawDungeonMapPlayerArrow(Renderer& renderer, Vec2 center, Vec2 facing, float scale)
{
    const Vec2 direction = lengthSquared(facing) > 0.0001f ? normalize(facing) : Vec2{1.0f, 0.0f};
    const Vec2 tangent{-direction.y, direction.x};
    const Vec2 tip = center + direction * (8.2f * scale);
    const Vec2 tail = center - direction * (5.8f * scale);
    const Vec2 notch = center - direction * (1.4f * scale);
    const Vec2 leftWing = center - direction * (2.2f * scale) + tangent * (6.2f * scale);
    const Vec2 rightWing = center - direction * (2.2f * scale) - tangent * (6.2f * scale);

    const Vec2 left[] = {tip, leftWing, notch, tail};
    const Vec2 right[] = {tip, tail, notch, rightWing};
    renderer.fillPolygon(left, 4, {248, 244, 212, 255});
    renderer.fillPolygon(right, 4, {248, 244, 212, 255});
    renderer.drawLine(tail, tip, {72, 94, 122, 210});
}

void drawDungeonMapLadderMarker(Renderer& renderer, Vec2 center, float scale)
{
    const float railHalfGap = 3.2f * scale;
    const float halfHeight = 7.0f * scale;
    const float rungHalfWidth = 4.8f * scale;
    const Color shadow{8, 18, 36, 185};
    const Color rail = DungeonMapExitLadderColor;
    renderer.drawLine(center + Vec2{-railHalfGap + 0.8f * scale, -halfHeight + 0.8f * scale}, center + Vec2{-railHalfGap + 0.8f * scale, halfHeight + 0.8f * scale}, shadow);
    renderer.drawLine(center + Vec2{railHalfGap + 0.8f * scale, -halfHeight + 0.8f * scale}, center + Vec2{railHalfGap + 0.8f * scale, halfHeight + 0.8f * scale}, shadow);
    renderer.drawLine(center + Vec2{-railHalfGap, -halfHeight}, center + Vec2{-railHalfGap, halfHeight}, rail);
    renderer.drawLine(center + Vec2{railHalfGap, -halfHeight}, center + Vec2{railHalfGap, halfHeight}, rail);
    for (float offset : {-4.0f, 0.0f, 4.0f}) {
        renderer.drawLine(center + Vec2{-rungHalfWidth, offset * scale}, center + Vec2{rungHalfWidth, offset * scale}, rail);
    }
}

float dungeonRingIntroItemLocalProgress(float introProgress, int itemIndex, int ringIndex)
{
    const float delay = std::min(0.38f, static_cast<float>(itemIndex) * 0.045f + static_cast<float>(ringIndex) * 0.07f);
    return clamp((introProgress - delay) / 0.46f, 0.0f, 1.0f);
}

float dungeonRingIntroItemReveal(float introProgress, int itemIndex, int ringIndex)
{
    return smootherStep(dungeonRingIntroItemLocalProgress(introProgress, itemIndex, ringIndex));
}

float dungeonRingIntroOrbitScale(float introProgress)
{
    if (introProgress < 0.56f) {
        return lerp(0.12f, 1.16f, easeOutBack(introProgress / 0.56f));
    }
    return lerp(1.16f, 1.0f, easeOutCubic((introProgress - 0.56f) / 0.44f));
}

Vec2 dungeonRingIntroItemGroundPosition(
    const SpellRingSystem& spellRing,
    const SpellRingItem& item,
    float introProgress,
    int itemIndex,
    int ringIndex)
{
    const Vec2 targetOffset = item.worldPosition - spellRing.center();
    const float targetDistance = length(targetOffset);
    const Vec2 direction = targetDistance > 0.001f
        ? targetOffset * (1.0f / targetDistance)
        : fromAngle(item.localAngle);
    const float local = dungeonRingIntroItemLocalProgress(introProgress, itemIndex, ringIndex);
    const float radial = targetDistance * lerp(0.08f, 1.0f, easeOutBack(local));
    return spellRing.center() + direction * radial;
}

constexpr int RingPlaceColumns = 8;
constexpr int RingPlaceRows = 3;
constexpr int RingPlaceSlotCount = RingPlaceColumns * RingPlaceRows;
constexpr float RingPlaceScreenX = 44.0f;
constexpr float RingPlaceScreenY = 58.0f;
constexpr float RingPlaceScreenW = 1192.0f;
constexpr float RingPlaceScreenH = 610.0f;
constexpr float RingPlaceGridY = RingPlaceScreenY + 84.0f;
constexpr float RingPlaceSlotW = 88.0f;
constexpr float RingPlaceSlotH = 76.0f;
constexpr float RingPlaceSlotGap = 8.0f;
constexpr float RingPlaceSlotImageMaxSize = 48.0f;
constexpr float RingPlaceDetailX = RingPlaceScreenX + 820.0f;
constexpr float RingPlaceDetailY = RingPlaceScreenY + 50.0f;
constexpr float RingPlaceDetailW = 330.0f;
constexpr float RingPlaceDetailH = 520.0f;
constexpr float RingPlaceGridW =
    static_cast<float>(RingPlaceColumns) * RingPlaceSlotW +
    static_cast<float>(RingPlaceColumns - 1) * RingPlaceSlotGap;
constexpr float RingPlaceGridX = RingPlaceScreenX + (RingPlaceDetailX - RingPlaceScreenX - RingPlaceGridW) * 0.5f;

enum class RingCommandAction {
    Place,
    Remove,
    ToggleProtection,
    Discard,
};

struct RingCommandMenuItems {
    std::vector<UiCommandMenuItem> items;
    std::vector<RingCommandAction> actions;
};

RingCommandMenuItems ringCommandItems(
    bool placeCommand,
    const SpellRingItem* item,
    const ObjectCatalog& objectCatalog,
    bool commandCanPlace,
    bool commandCanRemove)
{
    RingCommandMenuItems result;
    if (placeCommand) {
        result.items.push_back({"アイテムを配置", commandCanPlace});
        result.actions.push_back(RingCommandAction::Place);
        return result;
    }

    if (item == nullptr) {
        result.items.push_back({"リングから外す", false});
        result.actions.push_back(RingCommandAction::Remove);
        result.items.push_back({"保護", false});
        result.actions.push_back(RingCommandAction::ToggleProtection);
        result.items.push_back({"捨てる", false});
        result.actions.push_back(RingCommandAction::Discard);
        return result;
    }

    const bool hasObject = !item->objectId.empty();
    const bool protectable = hasObject && !item->instanceId.empty();
    const ItemData* itemData = objectForRingItem(objectCatalog, *item);
    const bool discardable = hasObject && itemData != nullptr && !item->protectionEnabled && !isImportantItem(*itemData);
    result.items.push_back({"リングから外す", commandCanRemove});
    result.actions.push_back(RingCommandAction::Remove);
    result.items.push_back({item->protectionEnabled ? "保護を解除" : "保護", protectable});
    result.actions.push_back(RingCommandAction::ToggleProtection);
    result.items.push_back({"捨てる", discardable});
    result.actions.push_back(RingCommandAction::Discard);
    return result;
}

Vec2 ringPressedDirection(const Input& input)
{
    Vec2 direction{};
    if (input.pressed(InputAction::MoveLeft)) {
        direction.x -= 1.0f;
    }
    if (input.pressed(InputAction::MoveRight)) {
        direction.x += 1.0f;
    }
    if (input.pressed(InputAction::MoveUp)) {
        direction.y -= 1.0f;
    }
    if (input.pressed(InputAction::MoveDown)) {
        direction.y += 1.0f;
    }
    return lengthSquared(direction) > 1.0f ? normalize(direction) : direction;
}

int ringItemSelectionByDirection(
    const std::vector<SpellRingItem>& items,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int current,
    Vec2 direction)
{
    const int count = static_cast<int>(items.size());
    if (count <= 0) {
        return 0;
    }
    if (count == 1 || lengthSquared(direction) <= 0.0001f) {
        return std::clamp(current, 0, count - 1);
    }

    const Vec2 unitDirection = normalize(direction);
    const int currentIndex = std::clamp(current, 0, count - 1);
    const Vec2 currentCenter = ringItemUiCenter(
        items[static_cast<std::size_t>(currentIndex)],
        spellRing,
        balance,
        currentIndex,
        count);

    int bestIndex = currentIndex;
    bool bestForward = false;
    float bestScore = -std::numeric_limits<float>::max();
    float bestDistanceSq = std::numeric_limits<float>::max();
    for (int i = 0; i < count; ++i) {
        if (i == currentIndex) {
            continue;
        }
        const Vec2 itemCenter = ringItemUiCenter(
            items[static_cast<std::size_t>(i)],
            spellRing,
            balance,
            i,
            count);
        const Vec2 offset = itemCenter - currentCenter;
        const float distanceSq = lengthSquared(offset);
        if (distanceSq <= 0.0001f) {
            continue;
        }

        const float alignment = dotVec2(normalize(offset), unitDirection);
        const bool forward = alignment > 0.15f;
        const float score = forward
            ? alignment * 256.0f - std::sqrt(distanceSq) * 0.25f
            : alignment;
        if ((forward && !bestForward) ||
            (forward == bestForward && (score > bestScore + 0.001f ||
                (std::fabs(score - bestScore) <= 0.001f && distanceSq < bestDistanceSq)))) {
            bestIndex = i;
            bestForward = forward;
            bestScore = score;
            bestDistanceSq = distanceSq;
        }
    }

    return bestForward ? bestIndex : currentIndex;
}

int ringMoveStepSignForDirection(
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int itemIndex,
    int itemCount,
    float currentAngle,
    Vec2 direction)
{
    if (lengthSquared(direction) <= 0.0001f) {
        return 0;
    }

    const Vec2 unitDirection = normalize(direction);
    const Vec2 currentCenter = ringItemUiCenterAtAngle(currentAngle, spellRing, balance, itemIndex, itemCount);
    const auto directionScore = [&](float angle) {
        const Vec2 nextCenter = ringItemUiCenterAtAngle(
            spellRing.quantizeLocalAngle(angle, balance),
            spellRing,
            balance,
            itemIndex,
            itemCount);
        const Vec2 offset = nextCenter - currentCenter;
        if (lengthSquared(offset) <= 0.0001f) {
            return -std::numeric_limits<float>::max();
        }
        return dotVec2(normalize(offset), unitDirection);
    };

    const float clockwiseScore = directionScore(currentAngle + RingAngleStep);
    const float counterClockwiseScore = directionScore(currentAngle - RingAngleStep);
    return clockwiseScore >= counterClockwiseScore ? 1 : -1;
}

bool moveRingItemByDirection(
    SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int itemIndex,
    int itemCount,
    Vec2 direction)
{
    std::vector<SpellRingItem>& items = spellRing.items();
    if (itemIndex < 0 || itemIndex >= static_cast<int>(items.size())) {
        return false;
    }

    const int sign = ringMoveStepSignForDirection(
        spellRing,
        balance,
        itemIndex,
        itemCount,
        items[static_cast<std::size_t>(itemIndex)].localAngle,
        direction);
    return sign != 0 && spellRing.moveItemAngle(itemIndex, RingAngleStep * static_cast<float>(sign));
}

UiRect ringPlaceWindowRect()
{
    return {{RingPlaceScreenX, RingPlaceScreenY}, {RingPlaceScreenW, RingPlaceScreenH}};
}

UiRect ringDiscardConfirmRect()
{
    return {{390.0f, 220.0f}, {500.0f, 280.0f}};
}

UiRect ringPlaceSlotRect(int index)
{
    const int row = index / RingPlaceColumns;
    const int column = index % RingPlaceColumns;
    return {{
        RingPlaceGridX + static_cast<float>(column) * (RingPlaceSlotW + RingPlaceSlotGap),
        RingPlaceGridY + static_cast<float>(row) * (RingPlaceSlotH + RingPlaceSlotGap),
    }, {RingPlaceSlotW, RingPlaceSlotH}};
}

UiRect ringPlaceDetailRect()
{
    return {{RingPlaceDetailX, RingPlaceDetailY}, {RingPlaceDetailW, RingPlaceDetailH}};
}

InventoryUiEntryView ringPlaceEntryView(const InventorySystem& inventory, int slotIndex)
{
    InventoryUiEntryView entry{};
    if (const InventoryObjectStack* stack = inventory.screenObjectStackAt(slotIndex)) {
        entry.item = &stack->item;
        entry.stackCount = stack->count;
    } else if (const InventoryObjectInstance* instance = inventory.screenObjectInstanceAt(slotIndex)) {
        entry.item = &instance->item;
        entry.instance = &instance->instance;
        entry.stackCount = 1;
        entry.equipped = inventory.isStaffEquipped(instance->instance.instanceId);
    }
    return entry;
}

bool ringPlaceSlotEnabled(
    const InventorySystem& inventory,
    const SpellRingSystem& spellRing,
    int slotIndex,
    float localAngle)
{
    return inventory.hasScreenItemAt(slotIndex) &&
        inventory.screenItemCanAddToRing(slotIndex, spellRing, localAngle);
}

int firstRingPlaceableSlot(
    const InventorySystem& inventory,
    const SpellRingSystem& spellRing,
    float localAngle)
{
    const int count = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    for (int i = 0; i < count; ++i) {
        if (ringPlaceSlotEnabled(inventory, spellRing, i, localAngle)) {
            return i;
        }
    }
    return -1;
}

int movedRingPlaceSelection(
    const InventorySystem& inventory,
    const SpellRingSystem& spellRing,
    float localAngle,
    int current,
    int delta)
{
    const int count = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    if (count <= 0 || delta == 0) {
        return current;
    }

    int cursor = std::clamp(current, 0, count - 1);
    for (int step = 0; step < count; ++step) {
        cursor = (cursor + delta) % count;
        if (cursor < 0) {
            cursor += count;
        }
        if (ringPlaceSlotEnabled(inventory, spellRing, cursor, localAngle)) {
            return cursor;
        }
    }
    return current;
}

bool ringInventoryHasAnyItem(const InventorySystem& inventory)
{
    const int count = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    for (int i = 0; i < count; ++i) {
        if (inventory.hasScreenItemAt(i)) {
            return true;
        }
    }
    return false;
}

bool ringInventoryHasAnyAutoPlaceableItem(const InventorySystem& inventory, const SpellRingSystem& spellRing)
{
    const int count = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    for (int i = 0; i < count; ++i) {
        if (inventory.screenItemCanAddToRing(i, spellRing, std::nullopt)) {
            return true;
        }
    }
    return false;
}

std::string ringPlacementUnavailableStatus(const InventorySystem& inventory, const SpellRingSystem& spellRing)
{
    if (!ringInventoryHasAnyItem(inventory)) {
        return "配置できるアイテムがありません";
    }
    if (!spellRing.canAddItem()) {
        return "リング満杯です";
    }
    if (!ringInventoryHasAnyAutoPlaceableItem(inventory, spellRing)) {
        return "過積載限界のため配置できません";
    }
    return "この位置には配置できません";
}

bool ringPlacementHitAreaContains(Vec2 point, const SpellRingSystem& spellRing, const RuntimeBalance& balance)
{
    if (!ringPanelRect().contains(point) || ringDetailRect().contains(point)) {
        return false;
    }

    std::vector<Vec2> orbitPath = getRingPathSamplePoints(
        spellRing.center(),
        ringUiOrbitContext(spellRing, balance, 0, 1),
        160);
    float nearestDistanceSq = std::numeric_limits<float>::max();
    for (Vec2 pathPoint : orbitPath) {
        pathPoint = applyRingUiShapeRotation(spellRing, ringWorldToUi(spellRing, pathPoint));
        nearestDistanceSq = std::min(nearestDistanceSq, distanceSquared(point, pathPoint));
    }
    constexpr float HitDistance = 72.0f;
    return nearestDistanceSq <= HitDistance * HitDistance;
}

void drawRingPlaceWindow(
    Renderer& renderer,
    const InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    const EncyclopediaSystem& encyclopedia,
    const SpellRingSystem& spellRing,
    int selection,
    float localAngle,
    std::string_view status,
    float totalTime)
{
    const UiRect panel = ringPlaceWindowRect();
    UiWindowScope placeWindow(
        renderer,
        "ring.place",
        panel,
        "アイテム配置",
        "WASD/矢印 選択  F/Enter 配置  Esc 戻る",
        UiWindowOptions{true, true});

    const int slotCount = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    for (int i = 0; i < slotCount; ++i) {
        const InventoryUiEntryView entry = ringPlaceEntryView(inventory, i);
        const bool hasItem = entry.item != nullptr;
        const bool enabled = ringPlaceSlotEnabled(inventory, spellRing, i, localAngle);
        InventoryUiSlotStyle style{i == selection && enabled, hasItem && !enabled, RingPlaceSlotImageMaxSize};
        if (entry.item != nullptr && entry.instance == nullptr && entry.stackCount > 1) {
            style.showTopRightCount = true;
            style.topRightCount = entry.stackCount;
        }
        drawInventoryUiSlot(renderer, ringPlaceSlotRect(i), entry, style);
    }

    InventoryUiEntryView detailEntry{};
    if (selection >= 0 && selection < slotCount) {
        detailEntry = ringPlaceEntryView(inventory, selection);
    }
    drawInventoryUiDetailPanel(
        renderer,
        ringPlaceDetailRect(),
        detailEntry,
        objectCatalog,
        encyclopedia,
        InventoryUiDetailOptions{
            .animationSeconds = totalTime,
            .unlockedRingCount = unlockedRingCount(),
        });

    if (!status.empty()) {
        renderer.drawText(panel.pos + Vec2{32.0f, panel.size.y - 66.0f}, status, {255, 230, 150, 255}, 2);
    }
}

void drawRingDiscardConfirmDialog(
    Renderer& renderer,
    const UiConfirmDialogState& state,
    const ObjectCatalog& objectCatalog,
    const SpellRingItem* item)
{
    if (!state.open) {
        return;
    }

    const UiRect panel = ringDiscardConfirmRect();
    UiWindowScope window(
        renderer,
        "ring.discard.confirm",
        panel,
        state.title,
        uiConfirmDialogHelpText(),
        UiWindowOptions{true, true});

    const std::string itemName = item != nullptr
        ? ringItemDisplayName(objectCatalog, *item)
        : std::string("アイテム");
    const bool hasObject = item != nullptr && !item->objectId.empty() &&
        objectCatalog.registry.findById(item->objectId) != nullptr;
    const std::string iconPrefix = hasObject ? inlineItemTag(item->objectId) : "";

    constexpr float ContentInset = 48.0f;
    const float bodyTop = panel.pos.y + ui::HeaderHeight + 6.0f;
    const UiRect body{{
        panel.pos.x + ContentInset,
        bodyTop,
    }, {
        panel.size.x - ContentInset * 2.0f,
        std::max(0.0f, uiConfirmDialogButtonRect(panel, 0).pos.y - bodyTop - 16.0f),
    }};

    InlineItemTextStyle questionStyle;
    questionStyle.text = ui::Text;
    questionStyle.scale = 2;
    const std::string question = fittedInlineItemText(
        renderer,
        iconPrefix + itemName + "を捨てますか？",
        body.size.x,
        questionStyle);
    float y = body.pos.y;
    drawInlineItemText(renderer, objectCatalog, {body.pos.x, y}, question, questionStyle);
    y += measureInlineItemText(renderer, question, questionStyle).y + 18.0f;
    renderer.drawWrappedText(
        {body.pos.x, y},
        "（捨てたアイテムは再入手できません）",
        body.size.x,
        {255, 224, 164, 255},
        2);

    drawUiConfirmDialogButtons(renderer, state, panel);
}

float cometArrangeArcRadians(const RingOrbitTuning& tuning)
{
    const float maxArcDegrees = std::clamp(tuning.cometMaxArcDegrees, 10.0f, 360.0f);
    return std::clamp(std::abs(tuning.cometArcDegrees), 10.0f, maxArcDegrees) * (Pi / 180.0f);
}

struct RingArrangeEntry {
    int itemIndex = 0;
    float pathParam = 0.0f;
};

float ringArrangePathParam(RingShape shape, const RingOrbitTuning& tuning, float localAngle)
{
    if (shape == RingShape::Comet) {
        const float halfArc = cometArrangeArcRadians(tuning) * 0.5f;
        return std::clamp(localAngle, -halfArc, halfArc);
    }
    return normalizeRingAngle(localAngle);
}

bool arrangeActiveRingItemsEvenly(SpellRingSystem& spellRing, const RuntimeBalance& balance)
{
    std::vector<SpellRingItem>& items = spellRing.items();
    const int count = static_cast<int>(items.size());
    if (count <= 0) {
        return false;
    }

    const RingShape shape = spellRing.activeRingShape();
    const RingOrbitTuning tuning = makeRingOrbitTuning(balance);
    const int ringIndex = spellRing.activeRingIndex();
    if (count == 1) {
        items.front().ringIndex = ringIndex;
        items.front().localAngle = spellRing.normalizeLocalAngle(items.front().localAngle, balance);
        return true;
    }

    std::vector<RingArrangeEntry> order;
    order.reserve(items.size());
    for (int i = 0; i < count; ++i) {
        order.push_back(RingArrangeEntry{
            i,
            ringArrangePathParam(shape, tuning, items[static_cast<std::size_t>(i)].localAngle),
        });
    }
    std::stable_sort(order.begin(), order.end(), [](const RingArrangeEntry& left, const RingArrangeEntry& right) {
        return left.pathParam < right.pathParam;
    });

    if (shape == RingShape::Comet) {
        const float arc = cometArrangeArcRadians(tuning);
        for (int i = 0; i < count; ++i) {
            const int itemIndex = order[static_cast<std::size_t>(i)].itemIndex;
            const float angle = -arc * 0.5f + arc * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
            items[static_cast<std::size_t>(itemIndex)].ringIndex = ringIndex;
            items[static_cast<std::size_t>(itemIndex)].localAngle = spellRing.quantizeLocalAngle(angle, balance);
        }
        return true;
    }

    const float full = Pi * 2.0f;
    const float step = full / static_cast<float>(count);
    int bestStart = 0;
    float bestPhase = 0.0f;
    float bestCost = std::numeric_limits<float>::max();
    for (int start = 0; start < count; ++start) {
        const float firstParam = order[static_cast<std::size_t>(start)].pathParam;
        float phaseSum = 0.0f;
        for (int offset = 0; offset < count; ++offset) {
            const RingArrangeEntry& entry = order[static_cast<std::size_t>((start + offset) % count)];
            float param = entry.pathParam;
            if (param + 0.0001f < firstParam) {
                param += full;
            }
            phaseSum += param - step * static_cast<float>(offset);
        }
        const float phase = phaseSum / static_cast<float>(count);
        float cost = 0.0f;
        for (int offset = 0; offset < count; ++offset) {
            const RingArrangeEntry& entry = order[static_cast<std::size_t>((start + offset) % count)];
            const float target = phase + step * static_cast<float>(offset);
            const float delta = shortestRingAngleDelta(entry.pathParam, target, shape, balance);
            cost += delta * delta;
        }
        if (cost < bestCost) {
            bestCost = cost;
            bestStart = start;
            bestPhase = phase;
        }
    }

    for (int offset = 0; offset < count; ++offset) {
        const int itemIndex = order[static_cast<std::size_t>((bestStart + offset) % count)].itemIndex;
        const float angle = bestPhase + step * static_cast<float>(offset);
        items[static_cast<std::size_t>(itemIndex)].ringIndex = ringIndex;
        items[static_cast<std::size_t>(itemIndex)].localAngle = spellRing.quantizeLocalAngle(angle, balance);
    }

    return true;
}

bool returnRingItemToInventory(
    InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    const SpellRingItem& item)
{
    if (item.objectId.empty()) {
        return false;
    }

    return inventory.addObjectInstance(
        objectCatalog,
        inventoryInstanceFromRingItem(inventory, objectCatalog, item));
}

int ringInventoryFreeObjectSlots(const InventorySystem& inventory)
{
    const int usedSlots = static_cast<int>(inventory.objectStacks().size() + inventory.objectInstances().size());
    return std::max(0, inventory.screenSlotCount() - usedSlots);
}

struct RingRemoveCandidate {
    int itemIndex = -1;
    float weight = 0.0f;
};

std::vector<RingRemoveCandidate> removableRingItemsByWeight(const std::vector<SpellRingItem>& items)
{
    std::vector<RingRemoveCandidate> candidates;
    candidates.reserve(items.size());
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const SpellRingItem& item = items[static_cast<std::size_t>(i)];
        if (item.objectId.empty()) {
            continue;
        }
        candidates.push_back(RingRemoveCandidate{i, std::max(0.0f, item.weight)});
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const RingRemoveCandidate& left, const RingRemoveCandidate& right) {
        if (std::fabs(left.weight - right.weight) > 0.0001f) {
            return left.weight > right.weight;
        }
        return left.itemIndex < right.itemIndex;
    });
    return candidates;
}

bool removeAllRingItemsToInventory(
    std::vector<SpellRingItem>& items,
    int& selection,
    InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    std::string& status)
{
    std::vector<RingRemoveCandidate> candidates = removableRingItemsByWeight(items);
    if (candidates.empty()) {
        status = "外せるアイテムがありません";
        return false;
    }

    const int removableCount = static_cast<int>(candidates.size());
    const int removeLimit = std::min(removableCount, ringInventoryFreeObjectSlots(inventory));
    if (removeLimit <= 0) {
        status = "インベントリ満杯のため外せません";
        return false;
    }

    std::vector<int> removedIndices;
    removedIndices.reserve(static_cast<std::size_t>(removeLimit));
    for (int i = 0; i < removeLimit; ++i) {
        const int itemIndex = candidates[static_cast<std::size_t>(i)].itemIndex;
        if (itemIndex < 0 || itemIndex >= static_cast<int>(items.size()) ||
            !returnRingItemToInventory(inventory, objectCatalog, items[static_cast<std::size_t>(itemIndex)])) {
            break;
        }
        removedIndices.push_back(itemIndex);
    }

    if (removedIndices.empty()) {
        status = "インベントリ満杯のため外せません";
        return false;
    }

    const int removedBeforeSelection = static_cast<int>(std::count_if(
        removedIndices.begin(),
        removedIndices.end(),
        [selection](int itemIndex) {
            return itemIndex < selection;
        }));

    std::sort(removedIndices.begin(), removedIndices.end(), [](int left, int right) {
        return left > right;
    });
    for (int itemIndex : removedIndices) {
        if (itemIndex >= 0 && itemIndex < static_cast<int>(items.size())) {
            items.erase(items.begin() + itemIndex);
        }
    }

    selection -= removedBeforeSelection;
    selection = items.empty() ? 0 : std::clamp(selection, 0, static_cast<int>(items.size()) - 1);

    const int removedCount = static_cast<int>(removedIndices.size());
    if (removedCount >= removableCount) {
        status = "外せるアイテムをすべて戻しました";
    } else {
        status = "空き枠分だけ重い順に" + std::to_string(removedCount) + "個戻しました";
    }
    return true;
}

bool removeRingItemToInventory(
    std::vector<SpellRingItem>& items,
    int& selection,
    InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    std::string& status)
{
    if (selection < 0 || selection >= static_cast<int>(items.size())) {
        status = "アイテム未選択です";
        return false;
    }

    const SpellRingItem& selectedItem = items[static_cast<std::size_t>(selection)];
    if (selectedItem.objectId.empty()) {
        status = "このアイテムは外せません";
        return false;
    }
    if (!returnRingItemToInventory(inventory, objectCatalog, selectedItem)) {
        status = "インベントリ満杯のため外せません";
        return false;
    }

    items.erase(items.begin() + selection);
    selection = std::min(selection, std::max(0, static_cast<int>(items.size()) - 1));
    status = "インベントリへ戻しました";
    return true;
}

bool toggleRingItemProtection(std::vector<SpellRingItem>& items, int selection, std::string& status)
{
    if (selection < 0 || selection >= static_cast<int>(items.size())) {
        status = "アイテム未選択です";
        return false;
    }

    SpellRingItem& item = items[static_cast<std::size_t>(selection)];
    if (item.objectId.empty() || item.instanceId.empty()) {
        status = "個体アイテムのみ保護できます";
        return false;
    }

    item.protectionEnabled = !item.protectionEnabled;
    status = item.protectionEnabled ? "保護ON" : "保護OFF";
    return true;
}

bool discardRingItem(
    std::vector<SpellRingItem>& items,
    int& selection,
    InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    std::vector<InventoryDiscardRequest>& outRequests,
    std::string& status)
{
    if (selection < 0 || selection >= static_cast<int>(items.size())) {
        status = "アイテム未選択です";
        return false;
    }

    const SpellRingItem& selectedItem = items[static_cast<std::size_t>(selection)];
    const ItemData* itemData = objectForRingItem(objectCatalog, selectedItem);
    if (selectedItem.objectId.empty() || itemData == nullptr) {
        status = "このアイテムは捨てられません";
        return false;
    }
    if (selectedItem.protectionEnabled) {
        status = "保護中は捨てられません";
        return false;
    }
    if (isImportantItem(*itemData)) {
        status = "重要アイテムは捨てられません";
        return false;
    }

    outRequests.push_back(InventoryDiscardRequest{
        .item = *itemData,
        .instance = inventoryInstanceFromRingItem(inventory, objectCatalog, selectedItem),
        .quantity = 1,
    });
    items.erase(items.begin() + selection);
    selection = std::min(selection, std::max(0, static_cast<int>(items.size()) - 1));
    status = "捨てた: " + itemData->name;
    return true;
}

void drawDungeonRingIntroOrbit(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int visibleRingCount,
    float introProgress,
    float totalSeconds)
{
    const Vec2 center = spellRing.center();
    const float appear = smootherStep(introProgress / 0.24f);
    const float burstFade = 1.0f - smootherStep((introProgress - 0.08f) / 0.62f);
    const float orbitScale = dungeonRingIntroOrbitScale(introProgress);

    renderer.fillSoftCircle(center, 26.0f + 54.0f * appear, withAlpha({150, 220, 255, 255}, 86.0f * burstFade));
    renderer.drawSoftRing(
        center,
        spellRing.radius() * lerp(0.24f, 1.34f, easeOutCubic(introProgress)),
        18.0f * burstFade,
        withAlpha({128, 224, 255, 255}, 118.0f * burstFade));
    renderer.drawSoftRing(
        center,
        spellRing.radius() * lerp(0.10f, 0.94f, easeOutBack(clamp(introProgress / 0.48f, 0.0f, 1.0f))),
        8.0f + 10.0f * burstFade,
        withAlpha({214, 240, 255, 255}, 92.0f * burstFade));

    const int ringCount = std::clamp(visibleRingCount, 0, SpellRingCount);
    for (RingShape shapePass : MagicRingShapeRenderOrder) {
        for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
            const auto& ringItems = spellRing.itemsForRing(ringIndex);
            if (ringItems.empty() || spellRing.ringShapeForIndex(ringIndex) != shapePass) {
                continue;
            }

            std::vector<Vec2> orbitPath = spellRing.pathSamplePointsForRing(ringIndex, center, orbitScale, balance, 176);
            drawMagicOrbitPath(
                renderer,
                orbitPath,
                center,
                MagicOrbitDrawOptions{
                    shapePass,
                    ringIndex == spellRing.activeRingIndex(),
                    false,
                    true,
                    false,
                    ringIndex,
                    totalSeconds + introProgress * 2.6f,
                    (0.16f + 1.18f * appear) * (ringIndex == spellRing.activeRingIndex() ? 1.0f : 0.72f),
                });
        }
    }

    drawMagicStar(
        renderer,
        center,
        11.0f + 8.0f * burstFade,
        withAlpha({214, 240, 255, 255}, 168.0f * std::max(appear, burstFade)),
        totalSeconds * 1.2f);
}

void drawDungeonRingIntroItem(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const SpellRingItem& item,
    int itemIndex,
    float introProgress,
    float totalSeconds)
{
    const int ringIndex = std::clamp(item.ringIndex, 0, SpellRingCount - 1);
    const float local = dungeonRingIntroItemLocalProgress(introProgress, itemIndex, ringIndex);
    if (local <= 0.0f) {
        return;
    }

    const float reveal = smootherStep(local);
    const Vec2 introGround = dungeonRingIntroItemGroundPosition(spellRing, item, introProgress, itemIndex, ringIndex);
    const Vec2 toItem = introGround - spellRing.center();
    Vec2 outward = lengthSquared(toItem) > 0.0001f ? normalize(toItem) : fromAngle(item.localAngle);
    Vec2 forward{-outward.y, outward.x};
    if (lengthSquared(forward) <= 0.0001f) {
        forward = {1.0f, 0.0f};
    }
    const float lift = (1.0f - reveal) * 22.0f + std::sin(local * Pi) * 10.0f;
    const Vec2 drawPosition = elevatedDrawPosition(introGround, ringItemAltitude(item, totalSeconds) + lift);
    const RingShape ringShape = spellRing.ringShapeForIndex(ringIndex);
    const int ringItemCount = static_cast<int>(spellRing.itemsForRing(ringIndex).size());
    const float cometVisualScale = ringShape == RingShape::Comet
        ? std::clamp(1.0f - std::max(0, ringItemCount - 10) * 0.014f, 0.76f, 1.0f)
        : 1.0f;
    const float popScale = cometVisualScale * (lerp(0.56f, 1.0f, reveal) + std::sin(local * Pi) * 0.16f * (1.0f - local));
    const unsigned char alpha = alphaByte(255.0f * reveal);
    const Color tint{255, 255, 255, alpha};

    renderer.drawActorShadow(
        actorShadowAnchor(introGround, ItemShadowGroundOffsetY),
        ringItemShadowVisualSize(item, totalSeconds) * popScale,
        {0, 0, 0, static_cast<unsigned char>(std::clamp(std::lround(74.0f * reveal), 0L, 255L))});

    const ItemData* object = objectForRingItem(objectCatalog, item);
    const auto drawObject = [&]() {
        ObjectImageDrawOptions options;
        options.tint = tint;
        options.outlineColor.a = alpha;
        options.scaleMultiplier = popScale / std::max(0.001f, cometVisualScale);
        return drawRingItemObjectImage(
            renderer,
            item,
            object,
            drawPosition,
            {RingObjectImageMaxSize * cometVisualScale, RingObjectImageMaxSize * cometVisualScale},
            outward,
            forward,
            totalSeconds,
            options);
    };

    if (item.type == SpellRingItemType::Shovel) {
        if (!drawObject()) {
            renderer.fillCircle(drawPosition, item.hitRadius * popScale, withAlpha({178, 184, 190, 255}, 255.0f * reveal));
            renderer.drawLine(drawPosition, drawPosition + outward * (15.0f * popScale), withAlpha({90, 96, 102, 255}, 255.0f * reveal));
        }
    } else if (item.type == SpellRingItemType::Torch) {
        if (!drawObject()) {
            renderer.fillCircle(drawPosition, item.hitRadius * popScale, withAlpha({242, 122, 25, 255}, 255.0f * reveal));
            renderer.fillCircle(drawPosition + Vec2{2.0f, -2.0f} * popScale, 4.0f * popScale, withAlpha({255, 238, 98, 255}, 255.0f * reveal));
        }
    } else if (!drawObject()) {
        renderer.fillCircle(drawPosition, item.hitRadius * popScale, withAlpha({96, 122, 210, 255}, 255.0f * reveal));
        renderer.drawCircle(drawPosition, item.hitRadius * popScale + 3.0f, withAlpha({160, 202, 255, 255}, 255.0f * reveal));
    }

    const float sparkle = std::sin(local * Pi);
    if (sparkle > 0.05f) {
        const Vec2 introOffset = introGround - spellRing.center();
        const Vec2 sparkleOutward = lengthSquared(introOffset) > 0.0001f ? normalize(introOffset) : Vec2{0.0f, -1.0f};
        drawMagicStar(
            renderer,
            drawPosition - sparkleOutward * (10.0f + 12.0f * sparkle),
            3.0f + sparkle * 5.0f,
            withAlpha({255, 246, 190, 255}, 160.0f * sparkle * reveal),
            totalSeconds * 2.0f + static_cast<float>(itemIndex));
    }

    drawDetectionBadges(renderer, item, drawPosition, popScale, reveal);
}

struct RingItemRenderRef {
    const SpellRingItem* item = nullptr;
    int sequenceIndex = 0;
};

struct RingItemDepthRenderSnapshot {
    float sortY = 0.0f;
    SpellRingItem item;
};

std::vector<RingItemRenderRef> sortedRingItemRenderRefs(const std::vector<const SpellRingItem*>& runtimeItems)
{
    std::vector<RingItemRenderRef> result;
    result.reserve(runtimeItems.size());

    int sequenceIndex = 0;
    for (const SpellRingItem* item : runtimeItems) {
        if (item != nullptr) {
            result.push_back({item, sequenceIndex});
            ++sequenceIndex;
        }
    }

    std::stable_sort(result.begin(), result.end(), [](const RingItemRenderRef& left, const RingItemRenderRef& right) {
        return left.item->worldPosition.y < right.item->worldPosition.y;
    });
    return result;
}

std::vector<const SpellRingItem*> visibleRuntimeRingItems(
    const SpellRingSystem& spellRing,
    int visibleRingCount)
{
    const int ringCount = std::clamp(visibleRingCount, 0, SpellRingCount);
    std::vector<const SpellRingItem*> result;
    std::size_t total = 0;
    for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
        total += spellRing.itemsForRing(ringIndex).size();
    }
    result.reserve(total);
    for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
        for (const SpellRingItem& item : spellRing.itemsForRing(ringIndex)) {
            result.push_back(&item);
        }
    }
    return result;
}

float ringItemCometVisualScale(const SpellRingSystem& spellRing, const SpellRingItem& item)
{
    const int ringIndex = std::clamp(item.ringIndex, 0, SpellRingCount - 1);
    const RingShape ringShape = spellRing.ringShapeForIndex(ringIndex);
    const int ringItemCount = static_cast<int>(spellRing.itemsForRing(ringIndex).size());
    return ringShape == RingShape::Comet
        ? std::clamp(1.0f - std::max(0, ringItemCount - 10) * 0.014f, 0.76f, 1.0f)
        : 1.0f;
}

void drawSpellRingItemWorldVisual(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const SpellRingItem& item,
    float totalSeconds,
    bool drawShadow,
    bool drawMagicAuraOverlay)
{
    const float cometVisualScale = ringItemCometVisualScale(spellRing, item);
    const Vec2 drawPosition = ringItemDrawPosition(item, totalSeconds);

    if (drawShadow) {
        renderer.drawActorShadow(
            actorShadowAnchor(item.worldPosition, ItemShadowGroundOffsetY),
            ringItemShadowVisualSize(item, totalSeconds) * cometVisualScale);
    }

    const ItemData* object = objectForRingItem(objectCatalog, item);
    const Vec2 outward = item.orbitOutward;
    const Vec2 maxImageSize{
        RingObjectImageMaxSize * cometVisualScale,
        RingObjectImageMaxSize * cometVisualScale};
    const ExplosionWarningVisual breakExplosionWarning = ringItemBreakExplosionWarningVisual(item);
    const float breakExplosionVisualRadius = std::max(10.0f, item.hitRadius * cometVisualScale);
    drawRingItemBreakExplosionWarningAura(renderer, drawPosition, breakExplosionVisualRadius, breakExplosionWarning);

    const bool drewImage = drawRingItemObjectImage(
        renderer,
        item,
        object,
        drawPosition,
        maxImageSize,
        outward,
        item.worldVelocity,
        totalSeconds);
    if (!drewImage) {
        if (item.type == SpellRingItemType::Shovel) {
            renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {178, 184, 190, 255});
            renderer.drawLine(drawPosition, drawPosition + outward * (15.0f * cometVisualScale), {90, 96, 102, 255});
        } else if (item.type == SpellRingItemType::Torch) {
            renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {242, 122, 25, 255});
            renderer.fillCircle(drawPosition + Vec2{2.0f, -2.0f} * cometVisualScale, 4.0f * cometVisualScale, {255, 238, 98, 255});
        } else {
            renderer.fillCircle(drawPosition, item.hitRadius * cometVisualScale, {96, 122, 210, 255});
            renderer.drawCircle(drawPosition, item.hitRadius * cometVisualScale + 3.0f, {160, 202, 255, 255});
        }
    }

    drawRingItemBreakExplosionWarningOverlay(renderer, drawPosition, breakExplosionVisualRadius, breakExplosionWarning);
    drawDetectionBadges(renderer, item, drawPosition, cometVisualScale);

    if (drawMagicAuraOverlay && item.magicAuraTimer > 0.0f && !item.magicAuraDamageType.empty() && item.magicAuraFxEmitterId == 0) {
        drawMagicAura(
            renderer,
            drawPosition,
            std::max(8.0f, item.hitRadius * cometVisualScale),
            item.magicAuraDamageType,
            totalSeconds);
    }
}

void appendRingItemDepthRenderEntry(
    std::vector<DepthRenderEntry>& entries,
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const RingItemDepthRenderSnapshot& snapshot,
    float totalSeconds)
{
    entries.push_back(DepthRenderEntry{
        snapshot.sortY,
        [&renderer, &spellRing, &objectCatalog, item = snapshot.item, totalSeconds]() {
            drawSpellRingItemWorldVisual(
                renderer,
                spellRing,
                objectCatalog,
                item,
                totalSeconds,
                true,
                true);
        },
    });
}

constexpr float DungeonMinimapX = 18.0f;
constexpr float DungeonMinimapYGap = 8.0f;
constexpr float DungeonMinimapDiameter = 178.0f;
constexpr float DungeonMinimapEdgeInset = 5.0f;
constexpr float DungeonMinimapTilePx = 2.5f;
constexpr float DungeonMapOverlayMargin = 46.0f;
constexpr float DungeonMapOverlayHeaderHeight = 74.0f;
constexpr float DungeonMapOverlayFooterHeight = 52.0f;
constexpr float DungeonMapOverlayPadding = 28.0f;
constexpr float DungeonMapOverlayScrollbarThickness = 6.0f;
constexpr float DungeonMapOverlayScrollbarInset = 6.0f;
constexpr float DungeonMapOverlayMinTilePx = 4.0f;
constexpr float DungeonMapOverlayMaxTilePx = 12.0f;

struct DungeonMinimapBounds {
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    bool valid = false;
};

Color dungeonMinimapTileColor(TileType type, bool lit)
{
    if (lit) {
        switch (type) {
        case TileType::Empty: return {132, 154, 178, 225};
        case TileType::Dirt: return {92, 70, 54, 205};
        case TileType::Rock: return {86, 90, 104, 205};
        case TileType::Ore: return {92, 108, 188, 220};
        case TileType::HardRock: return {62, 66, 82, 205};
        }
    }

    switch (type) {
    case TileType::Empty: return {58, 72, 92, 178};
    case TileType::Dirt: return {48, 38, 34, 148};
    case TileType::Rock: return {45, 48, 56, 152};
    case TileType::Ore: return {48, 58, 102, 165};
    case TileType::HardRock: return {30, 32, 40, 150};
    }
    return {58, 72, 92, 178};
}

template <typename Cells>
DungeonMinimapBounds dungeonMinimapBounds(const Cells& cells)
{
    DungeonMinimapBounds bounds{};
    const auto tileFromKey = [](std::int64_t key) {
        const std::uint64_t raw = static_cast<std::uint64_t>(key);
        const auto signedFromU32 = [](std::uint32_t value) {
            if (value <= static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
                return static_cast<int>(value);
            }
            return -1 - static_cast<int>(~value);
        };
        return DungeonTile{
            signedFromU32(static_cast<std::uint32_t>(raw >> 32)),
            signedFromU32(static_cast<std::uint32_t>(raw & 0xFFFFFFFFull)),
        };
    };
    for (const auto& [key, cell] : cells) {
        (void)cell;
        const DungeonTile tile = tileFromKey(key);
        if (!bounds.valid) {
            bounds.minX = bounds.maxX = tile.x;
            bounds.minY = bounds.maxY = tile.y;
            bounds.valid = true;
            continue;
        }
        bounds.minX = std::min(bounds.minX, tile.x);
        bounds.minY = std::min(bounds.minY, tile.y);
        bounds.maxX = std::max(bounds.maxX, tile.x);
        bounds.maxY = std::max(bounds.maxY, tile.y);
    }
    return bounds;
}

constexpr int OperationSettingsColumnAction = 0;
constexpr int OperationSettingsColumnKeyboardMouse = 1;
constexpr int OperationSettingsColumnGamepad = 2;
constexpr int OperationSettingsColumnCount = 3;
constexpr int OptionsPageOperation = 0;
constexpr int OptionsPageAudio = 1;
constexpr int OptionsPageVideo = 2;
constexpr int OptionsPageCount = 3;
constexpr int OperationSettingsCategoryCount = 4;
constexpr int AudioSettingsRowCount = 3;
constexpr int VideoSettingsRowCount = 7;
constexpr float OptionsContentYOffset = -16.0f;
constexpr float OptionsStandardContentYOffset = OptionsContentYOffset + 8.0f;
constexpr float OptionsStandardContentHeightBonus = 16.0f;

struct OperationSettingsActionRow {
    InputAction action;
    const char* label;
    int category;
};

struct VideoResolutionPreset {
    int width;
    int height;
};

constexpr const char* OptionsPageLabels[OptionsPageCount] = {
    "操作",
    "音量",
    "画面",
};

constexpr const char* OperationSettingsCategoryLabels[OperationSettingsCategoryCount] = {
    "基本",
    "リング/アイテム",
    "ショートカット",
    "開発",
};

constexpr VideoResolutionPreset VideoResolutionPresets[] = {
    {1280, 720},
    {1600, 900},
    {1920, 1080},
    {2560, 1440},
};
constexpr int VideoResolutionPresetCount = static_cast<int>(sizeof(VideoResolutionPresets) / sizeof(VideoResolutionPresets[0]));

constexpr OperationSettingsActionRow OperationSettingsActionRows[] = {
    {InputAction::MoveLeft, "左へ移動", 0},
    {InputAction::MoveRight, "右へ移動", 0},
    {InputAction::MoveUp, "上へ移動", 0},
    {InputAction::MoveDown, "下へ移動", 0},
    {InputAction::Confirm, "決定", 0},
    {InputAction::Cancel, "戻る", 0},
    {InputAction::Pause, "ポーズ", 0},
    {InputAction::OpenInventory, "アイテム画面", 0},
    {InputAction::ThrowActiveRing, "リングを投げる", 1},
    {InputAction::OffsetRingCenter, "リングずらし", 1},
    {InputAction::UseSelectedItem, "選択アイテム使用", 1},
    {InputAction::PutSelectedItemOnRing, "リングへ入れる", 1},
    {InputAction::GrabOrPlaceItem, "つかむ/置く", 1},
    {InputAction::ArrangeItems, "整列/並び替え", 1},
    {InputAction::PreviousActiveRing, "前のリング", 1},
    {InputAction::NextActiveRing, "次のリング", 1},
    {InputAction::ToggleProtection, "保護切替", 1},
    {InputAction::ShortcutCursorLeft, "ショートカット左", 2},
    {InputAction::ShortcutCursorRight, "ショートカット右", 2},
    {InputAction::DirectShortcut1, "ショートカット1", 2},
    {InputAction::DirectShortcut2, "ショートカット2", 2},
    {InputAction::DirectShortcut3, "ショートカット3", 2},
    {InputAction::DirectShortcut4, "ショートカット4", 2},
    {InputAction::DirectShortcut5, "ショートカット5", 2},
    {InputAction::DirectShortcut6, "ショートカット6", 2},
    {InputAction::DirectShortcut7, "ショートカット7", 2},
    {InputAction::DirectShortcut8, "ショートカット8", 2},
    {InputAction::ToggleShortcutRow, "ショートカット列切替", 2},
    {InputAction::ToggleDebug, "デバッグ表示", 3},
    {InputAction::ToggleDebugPause, "デバッグ停止", 3},
    {InputAction::TestRestart, "テスト再起動", 3},
    {InputAction::ToggleTestFreeze, "テスト停止", 3},
    {InputAction::OpenConsole, "コンソール", 3},
    {InputAction::ToggleAutoReloadBlock, "自動リロード停止", 3},
};

UiRect optionsPanelRect()
{
    return {{150.0f, 35.0f}, {980.0f, 650.0f}};
}

UiRect statusPanelRect()
{
    return {{120.0f, 70.0f}, {1040.0f, 580.0f}};
}

UiRect pausePanelForPage(PauseMenuPage page)
{
    if (page == PauseMenuPage::Options) {
        return optionsPanelRect();
    }
    if (page == PauseMenuPage::Status) {
        return statusPanelRect();
    }
    return pausePanelRect();
}

UiRect optionsPageTabRect(int index)
{
    const UiRect panel = optionsPanelRect();
    constexpr float Gap = 12.0f;
    const float totalWidth = panel.size.x - 84.0f;
    const float width = (totalWidth - Gap * static_cast<float>(OptionsPageCount - 1)) /
        static_cast<float>(OptionsPageCount);
    return {{
        panel.pos.x + 42.0f + static_cast<float>(index) * (width + Gap),
        panel.pos.y + 104.0f + OptionsContentYOffset,
    }, {width, ui::ButtonHeight}};
}

UiRect optionsLeftContentRect()
{
    const UiRect panel = optionsPanelRect();
    return {{
        panel.pos.x + 46.0f,
        panel.pos.y + 166.0f + OptionsStandardContentYOffset,
    }, {570.0f, 332.0f + OptionsStandardContentHeightBonus}};
}

UiRect optionsRightHelpWindowRect()
{
    const UiRect panel = optionsPanelRect();
    return {{
        panel.pos.x + 650.0f,
        panel.pos.y + 166.0f + OptionsStandardContentYOffset,
    }, {284.0f, 332.0f + OptionsStandardContentHeightBonus}};
}

UiRect operationSettingsTableRect()
{
    const UiRect panel = optionsPanelRect();
    return {{
        panel.pos.x + 46.0f,
        panel.pos.y + 220.0f + OptionsContentYOffset,
    }, {580.0f, 278.0f + 24.0f}};
}

UiRect operationSettingsTabRect(int index)
{
    const UiRect panel = optionsPanelRect();
    constexpr float Gap = 8.0f;
    const float totalWidth = panel.size.x - 92.0f;
    const float width = (totalWidth - Gap * static_cast<float>(OperationSettingsCategoryCount - 1)) /
        static_cast<float>(OperationSettingsCategoryCount);
    return {{
        panel.pos.x + 46.0f + static_cast<float>(index) * (width + Gap),
        panel.pos.y + 166.0f + OptionsContentYOffset,
    }, {width, 42.0f}};
}

UiRect operationSettingsHelpWindowRect()
{
    const UiRect panel = optionsPanelRect();
    return {{
        panel.pos.x + 650.0f,
        panel.pos.y + 220.0f + OptionsContentYOffset,
    }, {284.0f, 278.0f + 24.0f}};
}

UiRect optionsFooterButtonRect(int index, int count, float width = 225.0f)
{
    const UiRect panel = optionsPanelRect();
    constexpr float Gap = 10.0f;
    const float totalWidth = width * static_cast<float>(count) + Gap * static_cast<float>(std::max(0, count - 1));
    return {{
        panel.pos.x + (panel.size.x - totalWidth) * 0.5f + static_cast<float>(index) * (width + Gap),
        panel.pos.y + panel.size.y - uiFooterHeight("x") - ui::ButtonHeight - 12.0f,
    }, {width, ui::ButtonHeight}};
}

UiRect operationSettingsDialogRect()
{
    return {{390.0f, 226.0f}, {500.0f, 270.0f}};
}

UiRect optionSettingsContentRect()
{
    return optionsLeftContentRect();
}

UiRect audioSettingsRowRect(int index)
{
    const UiRect content = optionSettingsContentRect();
    return {{content.pos.x, content.pos.y + static_cast<float>(index) * 60.0f}, {content.size.x, 50.0f}};
}

UiRect audioSettingsSliderRect(int index)
{
    const UiRect row = audioSettingsRowRect(index);
    return {{row.pos.x + 202.0f, row.pos.y + 15.0f}, {280.0f, 20.0f}};
}

UiRect videoSettingsRowRect(int index)
{
    const UiRect content = optionSettingsContentRect();
    return {{content.pos.x, content.pos.y + static_cast<float>(index) * 42.0f}, {content.size.x, 38.0f}};
}

UiRect videoBrightnessSliderRect()
{
    const UiRect row = videoSettingsRowRect(4);
    return {{row.pos.x + 202.0f, row.pos.y + 11.0f}, {250.0f, 16.0f}};
}

UiRect optionsHelpWindowRect()
{
    return optionsRightHelpWindowRect();
}

UiTabItem optionsPageTabItem(int index)
{
    return {OptionsPageLabels[std::clamp(index, 0, OptionsPageCount - 1)], true};
}

std::array<UiTabItem, OptionsPageCount> optionsPageTabItems()
{
    std::array<UiTabItem, OptionsPageCount> items{};
    for (int i = 0; i < OptionsPageCount; ++i) {
        items[static_cast<std::size_t>(i)] = optionsPageTabItem(i);
    }
    return items;
}

std::array<UiRect, OptionsPageCount> optionsPageTabRects()
{
    std::array<UiRect, OptionsPageCount> rects{};
    for (int i = 0; i < OptionsPageCount; ++i) {
        rects[static_cast<std::size_t>(i)] = optionsPageTabRect(i);
    }
    return rects;
}

UiSelectableTableStyle operationSettingsTableStyle()
{
    UiSelectableTableStyle style;
    style.headerHeight = 34.0f;
    style.rowHeight = 42.0f;
    style.rowGap = 4.0f;
    style.columnGap = 8.0f;
    style.cellTextScale = 2;
    return style;
}

std::vector<OperationSettingsActionRow> operationSettingsRowsForCategory(int category)
{
    std::vector<OperationSettingsActionRow> rows;
    for (const OperationSettingsActionRow& row : OperationSettingsActionRows) {
        if (row.category == category) {
            rows.push_back(row);
        }
    }
    return rows;
}

std::array<UiRect, OperationSettingsCategoryCount> operationSettingsTabRects()
{
    std::array<UiRect, OperationSettingsCategoryCount> rects{};
    for (int i = 0; i < OperationSettingsCategoryCount; ++i) {
        rects[static_cast<std::size_t>(i)] = operationSettingsTabRect(i);
    }
    return rects;
}

UiVerticalTabsStyle optionsVerticalTabStyle()
{
    UiVerticalTabsStyle style;
    style.labelScale = 2;
    style.valueScale = 2;
    style.textPaddingX = 18.0f;
    style.valuePaddingX = 18.0f;
    style.valueGap = 12.0f;
    style.tabs.activeScale = 1.0f;
    style.tabs.imageOutset = 16.0f;
    return style;
}

int updateClickedVerticalTabSelection(UiContext& ui, const UiVerticalTabItem* items, const UiRect* rects, int itemCount)
{
    if (items == nullptr || rects == nullptr || itemCount <= 0) {
        return -1;
    }
    for (int i = 0; i < itemCount; ++i) {
        if (items[i].enabled && ui.pressed(rects[i])) {
            return i;
        }
    }
    return -1;
}

std::array<UiRect, AudioSettingsRowCount> audioSettingsRowRects()
{
    std::array<UiRect, AudioSettingsRowCount> rects{};
    for (int i = 0; i < AudioSettingsRowCount; ++i) {
        rects[static_cast<std::size_t>(i)] = audioSettingsRowRect(i);
    }
    return rects;
}

std::array<UiRect, VideoSettingsRowCount> videoSettingsRowRects()
{
    std::array<UiRect, VideoSettingsRowCount> rects{};
    for (int i = 0; i < VideoSettingsRowCount; ++i) {
        rects[static_cast<std::size_t>(i)] = videoSettingsRowRect(i);
    }
    return rects;
}

const char* audioSettingsRowLabel(int row)
{
    switch (row) {
    case 0: return "Master";
    case 1: return "BGM";
    case 2: return "SE";
    default: return "";
    }
}

float audioSettingsRowValue(const GameSettings& settings, int row)
{
    switch (row) {
    case 0: return settings.audio.masterVolume;
    case 1: return settings.audio.bgmVolume;
    case 2: return settings.audio.seVolume;
    default: return 0.0f;
    }
}

void setAudioSettingsRowValue(GameSettings& settings, int row, float value)
{
    value = clamp(value, 0.0f, 1.0f);
    switch (row) {
    case 0:
        settings.audio.masterVolume = value;
        break;
    case 1:
        settings.audio.bgmVolume = value;
        break;
    case 2:
        settings.audio.seVolume = value;
        break;
    default:
        break;
    }
}

std::string volumePercentText(float value)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%3d%%", static_cast<int>(std::lround(clamp(value, 0.0f, 1.0f) * 100.0f)));
    return buffer;
}

std::array<std::string, AudioSettingsRowCount> audioSettingsRowValueTexts(const GameSettings& settings)
{
    std::array<std::string, AudioSettingsRowCount> values{};
    for (int i = 0; i < AudioSettingsRowCount; ++i) {
        values[static_cast<std::size_t>(i)] = volumePercentText(audioSettingsRowValue(settings, i));
    }
    return values;
}

std::array<UiVerticalTabItem, AudioSettingsRowCount> audioSettingsTabItems(
    const std::array<std::string, AudioSettingsRowCount>& values)
{
    std::array<UiVerticalTabItem, AudioSettingsRowCount> items{};
    for (int i = 0; i < AudioSettingsRowCount; ++i) {
        items[static_cast<std::size_t>(i)] = {audioSettingsRowLabel(i), values[static_cast<std::size_t>(i)], true};
    }
    return items;
}

const char* videoSettingsRowLabel(int row)
{
    switch (row) {
    case 0: return "表示モード";
    case 1: return "ウィンドウサイズ";
    case 2: return "VSync";
    case 3: return "軽量化";
    case 4: return "明るさ";
    case 5: return "画面揺れ";
    case 6: return "入力アイコン";
    default: return "";
    }
}

std::string windowModeDisplayName(WindowMode mode)
{
    switch (mode) {
    case WindowMode::Windowed:
        return "ウィンドウ";
    case WindowMode::BorderlessFullscreen:
        return "フルスクリーン";
    }
    return "ウィンドウ";
}

std::string screenShakeDisplayName(ScreenShakeSetting setting)
{
    switch (setting) {
    case ScreenShakeSetting::Off:
        return "OFF";
    case ScreenShakeSetting::Low:
        return "弱";
    case ScreenShakeSetting::Standard:
        return "標準";
    }
    return "標準";
}

std::string inputIconSettingDisplayName(InputIconSetting setting)
{
    switch (setting) {
    case InputIconSetting::Auto:
        return "自動";
    case InputIconSetting::KeyboardMouse:
        return "キーボード";
    case InputIconSetting::Gamepad:
        return "ゲームパッド";
    }
    return "自動";
}

float clampedScreenBrightness(float brightness)
{
    return clamp(brightness, MinScreenBrightness, MaxScreenBrightness);
}

float screenBrightnessGaugeValue(float brightness)
{
    const float range = std::max(0.001f, MaxScreenBrightness - MinScreenBrightness);
    return (clampedScreenBrightness(brightness) - MinScreenBrightness) / range;
}

std::string screenBrightnessPercentText(float brightness)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%3d%%", static_cast<int>(std::lround(clampedScreenBrightness(brightness) * 100.0f)));
    return buffer;
}

void setScreenBrightnessValue(GameSettings& settings, float brightness)
{
    settings.presentation.brightness = clampedScreenBrightness(brightness);
}

std::string videoResolutionText(const VideoSettings& video)
{
    return std::to_string(video.windowWidth) + " x " + std::to_string(video.windowHeight);
}

std::string videoSettingsRowValueText(const GameSettings& settings, int row)
{
    switch (row) {
    case 0:
        return windowModeDisplayName(settings.video.windowMode);
    case 1:
        return videoResolutionText(settings.video);
    case 2:
        return settings.video.vsync ? "ON" : "OFF";
    case 3:
        return settings.performance.lightweight ? "ON" : "OFF";
    case 4:
        return screenBrightnessPercentText(settings.presentation.brightness);
    case 5:
        return screenShakeDisplayName(settings.presentation.screenShake);
    case 6:
        return inputIconSettingDisplayName(settings.presentation.inputIcons);
    default:
        return "";
    }
}

std::array<std::string, VideoSettingsRowCount> videoSettingsRowValueTexts(const GameSettings& settings)
{
    std::array<std::string, VideoSettingsRowCount> values{};
    for (int i = 0; i < VideoSettingsRowCount; ++i) {
        values[static_cast<std::size_t>(i)] = videoSettingsRowValueText(settings, i);
    }
    return values;
}

std::array<UiVerticalTabItem, VideoSettingsRowCount> videoSettingsTabItems(
    const std::array<std::string, VideoSettingsRowCount>& values)
{
    std::array<UiVerticalTabItem, VideoSettingsRowCount> items{};
    for (int i = 0; i < VideoSettingsRowCount; ++i) {
        items[static_cast<std::size_t>(i)] = {videoSettingsRowLabel(i), values[static_cast<std::size_t>(i)], true};
    }
    return items;
}

int videoResolutionPresetIndex(const VideoSettings& video)
{
    for (int i = 0; i < VideoResolutionPresetCount; ++i) {
        const VideoResolutionPreset preset = VideoResolutionPresets[i];
        if (video.windowWidth == preset.width && video.windowHeight == preset.height) {
            return i;
        }
    }
    return 0;
}

void cycleVideoResolution(GameSettings& settings, int delta)
{
    const int count = VideoResolutionPresetCount;
    if (count <= 0 || delta == 0) {
        return;
    }
    const int current = videoResolutionPresetIndex(settings.video);
    const int next = (current + delta + count) % count;
    settings.video.windowWidth = VideoResolutionPresets[next].width;
    settings.video.windowHeight = VideoResolutionPresets[next].height;
}

void adjustScreenBrightness(GameSettings& settings, int delta)
{
    setScreenBrightnessValue(settings, settings.presentation.brightness + 0.05f * static_cast<float>(delta));
}

void cycleScreenShakeSetting(GameSettings& settings, int delta)
{
    constexpr std::array<ScreenShakeSetting, 3> Values{
        ScreenShakeSetting::Off,
        ScreenShakeSetting::Low,
        ScreenShakeSetting::Standard,
    };
    auto it = std::find(Values.begin(), Values.end(), settings.presentation.screenShake);
    const int current = it == Values.end() ? 2 : static_cast<int>(std::distance(Values.begin(), it));
    const int next = (current + delta + static_cast<int>(Values.size())) % static_cast<int>(Values.size());
    settings.presentation.screenShake = Values[static_cast<std::size_t>(next)];
}

void cycleInputIconSetting(GameSettings& settings, int delta)
{
    constexpr std::array<InputIconSetting, 3> Values{
        InputIconSetting::Auto,
        InputIconSetting::KeyboardMouse,
        InputIconSetting::Gamepad,
    };
    auto it = std::find(Values.begin(), Values.end(), settings.presentation.inputIcons);
    const int current = it == Values.end() ? 0 : static_cast<int>(std::distance(Values.begin(), it));
    const int next = (current + delta + static_cast<int>(Values.size())) % static_cast<int>(Values.size());
    settings.presentation.inputIcons = Values[static_cast<std::size_t>(next)];
}

void cycleVideoSetting(GameSettings& settings, int row, int delta)
{
    if (delta == 0) {
        return;
    }
    switch (row) {
    case 0:
        settings.video.windowMode = settings.video.windowMode == WindowMode::Windowed
            ? WindowMode::BorderlessFullscreen
            : WindowMode::Windowed;
        break;
    case 1:
        cycleVideoResolution(settings, delta);
        break;
    case 2:
        settings.video.vsync = !settings.video.vsync;
        break;
    case 3:
        settings.performance.lightweight = !settings.performance.lightweight;
        break;
    case 4:
        adjustScreenBrightness(settings, delta);
        break;
    case 5:
        cycleScreenShakeSetting(settings, delta);
        break;
    case 6:
        cycleInputIconSetting(settings, delta);
        break;
    default:
        break;
    }
}

const char* audioSettingsHelpText(int row)
{
    switch (row) {
    case 0:
        return "ゲーム全体の音量です。BGMとSEの両方にまとめて反映されます。";
    case 1:
        return "音楽の音量です。拠点、ダンジョン、ボス戦などのBGMに反映されます。";
    case 2:
        return "効果音の音量です。UI、採掘、攻撃、拾得などのSEに反映されます。";
    default:
        return "";
    }
}

const char* videoSettingsHelpText(int row)
{
    switch (row) {
    case 0:
        return "ウィンドウ表示とフルスクリーン表示を切り替えます。フルスクリーンはボーダーレス全画面です。";
    case 1:
        return "ウィンドウ時の画面サイズです。フルスクリーン中も、次にウィンドウへ戻した時のサイズとして保存します。";
    case 2:
        return "VSyncはモニターの更新タイミングに描画を合わせる設定です。画面の裂けを抑えますが、入力遅延やFPSに影響することがあります。";
    case 3:
        return "ONにすると追加ライト、暗幕、影、常時エフェクト、粒子数を抑えて、ダンジョン中の負荷を下げます。";
    case 4:
        return "ゲージで画面全体の明るさを調整します。暗闇だけでなく、拠点、UI、紙芝居など画面全体に反映されます。";
    case 5:
        return "被弾やボス演出などの画面揺れの強さです。酔いやすい場合は弱またはOFFにしてください。";
    case 6:
        return "操作ヘルプに出す入力アイコンを選びます。自動では最後に使った入力機器に合わせて切り替えます。";
    default:
        return "";
    }
}

const char* operationSettingsActionHelpText(InputAction action)
{
    switch (action) {
    case InputAction::MoveLeft:
        return "左方向の移動と、左右を使う画面でのカーソル移動に使います。";
    case InputAction::MoveRight:
        return "右方向の移動と、左右を使う画面でのカーソル移動に使います。";
    case InputAction::MoveUp:
        return "上方向の移動と、項目一覧でのカーソル移動に使います。";
    case InputAction::MoveDown:
        return "下方向の移動と、項目一覧でのカーソル移動に使います。";
    case InputAction::Confirm:
        return "メニュー項目や確認ダイアログを決定します。";
    case InputAction::Cancel:
        return "現在の画面を戻る、または操作をキャンセルします。";
    case InputAction::Pause:
        return "通常画面から一時停止メニューを開きます。";
    case InputAction::OpenInventory:
        return "アイテム画面を直接開きます。";
    case InputAction::ThrowActiveRing:
        return "選択中のアクティブリングを投げます。";
    case InputAction::OffsetRingCenter:
        return "押している間、リング中心をプレイヤーからずらします。";
    case InputAction::UseSelectedItem:
        return "ショートカットやアイテム画面で選択中のアイテムを使います。";
    case InputAction::PutSelectedItemOnRing:
        return "選択中のアイテムをアクティブリングへ入れます。";
    case InputAction::GrabOrPlaceItem:
        return "アイテム画面やリング画面で、アイテムをつかむ/置く操作に使います。";
    case InputAction::ArrangeItems:
        return "アイテムやリング上の配置を整列、または並び替えします。";
    case InputAction::PreviousActiveRing:
        return "前のアクティブリングへ切り替えます。";
    case InputAction::NextActiveRing:
        return "次のアクティブリングへ切り替えます。";
    case InputAction::ToggleProtection:
        return "アイテムやリング装備の保護ON/OFFを切り替えます。";
    case InputAction::ShortcutCursorLeft:
        return "ショートカット選択を左へ移動します。";
    case InputAction::ShortcutCursorRight:
        return "ショートカット選択を右へ移動します。";
    case InputAction::DirectShortcut1:
        return "ショートカット1番を直接選びます。";
    case InputAction::DirectShortcut2:
        return "ショートカット2番を直接選びます。";
    case InputAction::DirectShortcut3:
        return "ショートカット3番を直接選びます。";
    case InputAction::DirectShortcut4:
        return "ショートカット4番を直接選びます。";
    case InputAction::DirectShortcut5:
        return "ショートカット5番を直接選びます。";
    case InputAction::DirectShortcut6:
        return "ショートカット6番を直接選びます。";
    case InputAction::DirectShortcut7:
        return "ショートカット7番を直接選びます。";
    case InputAction::DirectShortcut8:
        return "ショートカット8番を直接選びます。";
    case InputAction::ToggleShortcutRow:
        return "ショートカットの表示行を切り替えます。";
    case InputAction::ToggleDebug:
        return "開発用のデバッグ表示を切り替えます。";
    case InputAction::ToggleDebugPause:
        return "開発用のデバッグ停止を切り替えます。";
    case InputAction::TestRestart:
        return "テストプレイ中にゲームを再起動します。通常起動では無効です。";
    case InputAction::ToggleTestFreeze:
        return "テスト用の自動進行停止を切り替えます。";
    case InputAction::OpenConsole:
        return "開発用のコマンドコンソールを開きます。";
    case InputAction::ToggleAutoReloadBlock:
        return "開発中の自動リロード停止を切り替えます。";
    default:
        return "この操作の割当を変更します。";
    }
}

std::string operationSettingsHelpTitle(const OperationSettingsActionRow& row, int column)
{
    (void)column;
    return row.label;
}

std::string operationSettingsHelpDescription(InputAction action, int column)
{
    std::string description = operationSettingsActionHelpText(action);
    description += column == OperationSettingsColumnGamepad
        ? " 決定するとゲームパッド入力の割当を変更します。"
        : " 決定するとキー/マウス入力の割当を変更します。";
    return description;
}

void drawOptionsHelpWindow(
    Renderer& renderer,
    UiRect rect,
    std::string_view title,
    std::string_view description,
    std::string_view status)
{
    drawUiSubPanel(renderer, rect);

    const std::string header(title);
    const Vec2 headerPos = rect.pos + Vec2{20.0f, 12.0f};
    renderer.drawText(headerPos, header, ui::Text, 2);

    if (!status.empty()) {
        const Vec2 headerSize = renderer.measureText(header, 2);
        const Vec2 statusSize = renderer.measureText(status, 2);
        const float statusX = rect.pos.x + rect.size.x - statusSize.x - 20.0f;
        if (statusX > headerPos.x + headerSize.x + 18.0f) {
            renderer.drawText({statusX, headerPos.y}, status, {255, 230, 150, 255}, 2);
        }
    }

    renderer.drawWrappedText(
        rect.pos + Vec2{20.0f, 42.0f},
        description,
        rect.size.x - 40.0f,
        ui::TextMuted,
        2);
}

void drawOptionsHelpWindow(
    Renderer& renderer,
    std::string_view title,
    std::string_view description,
    std::string_view status)
{
    drawOptionsHelpWindow(renderer, optionsHelpWindowRect(), title, description, status);
}

std::array<UiSelectableTableColumn, OperationSettingsColumnCount> operationSettingsTableColumns()
{
    const UiRect table = operationSettingsTableRect();
    constexpr float ScrollbarReserve = 20.0f;
    constexpr float ActionWidth = 188.0f;
    constexpr float GapTotal = 8.0f * static_cast<float>(OperationSettingsColumnCount - 1);
    const float contentWidth = std::max(1.0f, table.size.x - ScrollbarReserve);
    const float bindingWidth = std::max(1.0f, (contentWidth - ActionWidth - GapTotal) * 0.5f);
    return {{
        UiSelectableTableColumn{"操作", ActionWidth, false},
        UiSelectableTableColumn{"キーボード/マウス", bindingWidth, true},
        UiSelectableTableColumn{"ゲームパッド", bindingWidth, true},
    }};
}

bool operationSettingsColumnMatchesBinding(int column, const InputBinding& binding)
{
    if (column == OperationSettingsColumnKeyboardMouse) {
        return binding.device == InputBindingDevice::Keyboard || binding.device == InputBindingDevice::MouseButton;
    }
    if (column == OperationSettingsColumnGamepad) {
        return binding.device == InputBindingDevice::GamepadButton || binding.device == InputBindingDevice::GamepadAxis;
    }
    return false;
}

InputRemapCaptureDeviceGroup operationSettingsCaptureGroupForColumn(int column)
{
    return column == OperationSettingsColumnGamepad
        ? InputRemapCaptureDeviceGroup::Gamepad
        : InputRemapCaptureDeviceGroup::KeyboardMouse;
}

std::string operationSettingsBindingDisplayName(const InputBinding& binding)
{
    switch (binding.device) {
    case InputBindingDevice::Keyboard:
        return keyboardScancodeName(binding.code);
    case InputBindingDevice::MouseButton:
        return "Mouse " + mouseButtonName(binding.code);
    case InputBindingDevice::GamepadButton:
        return "Pad " + gamepadButtonName(binding.code);
    case InputBindingDevice::GamepadAxis:
        return "Pad " + gamepadAxisName(binding.code) + (binding.direction < 0 ? "-" : "+");
    }
    return "Unknown";
}

std::string operationSettingsKeyboardGlyphLabel(int scancode)
{
    switch (static_cast<SDL_Scancode>(scancode)) {
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        return "Enter";
    case SDL_SCANCODE_ESCAPE:
        return "Esc";
    case SDL_SCANCODE_BACKSPACE:
        return "Back";
    case SDL_SCANCODE_DELETE:
        return "Del";
    case SDL_SCANCODE_SPACE:
        return "Space";
    case SDL_SCANCODE_TAB:
        return "Tab";
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
        return "Shift";
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
        return "Ctrl";
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
        return "Alt";
    default:
        break;
    }

    std::string label = keyboardScancodeName(scancode);
    if (label.size() == 1) {
        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    }
    return label;
}

std::string operationSettingsBindingGlyphToken(const InputBinding& binding)
{
    switch (binding.device) {
    case InputBindingDevice::Keyboard:
        return "{key:" + operationSettingsKeyboardGlyphLabel(binding.code) + "}";
    case InputBindingDevice::MouseButton:
        if (binding.code == SDL_BUTTON_RIGHT) {
            return "{mouse:right}";
        }
        if (binding.code == SDL_BUTTON_MIDDLE) {
            return "{mouse:middle}";
        }
        return "{mouse:left}";
    case InputBindingDevice::GamepadButton:
        return "{pad:" + gamepadButtonName(binding.code) + "}";
    case InputBindingDevice::GamepadAxis:
        return "{axis:" + gamepadAxisName(binding.code) + ":" + (binding.direction < 0 ? "-" : "+") + "}";
    }
    return "";
}

std::string operationSettingsBindingGlyphText(const InputBindingMap& bindings, InputAction action, int column)
{
    std::string text;
    const std::vector<InputBinding>& actionBindings = bindings[inputActionIndex(action)];
    for (const InputBinding& binding : actionBindings) {
        if (!operationSettingsColumnMatchesBinding(column, binding)) {
            continue;
        }
        const std::string token = operationSettingsBindingGlyphToken(binding);
        if (token.empty()) {
            continue;
        }
        if (!text.empty()) {
            text += " / ";
        }
        text += token;
    }
    return text.empty() ? "未設定" : text;
}

bool operationSettingsBindingSamePhysicalInput(const InputBinding& lhs, const InputBinding& rhs)
{
    return lhs.device == rhs.device &&
        lhs.code == rhs.code &&
        lhs.direction == rhs.direction;
}

void removeOperationSettingsColumnBindings(std::vector<InputBinding>& bindings, int column)
{
    bindings.erase(
        std::remove_if(bindings.begin(), bindings.end(), [column](const InputBinding& binding) {
            return operationSettingsColumnMatchesBinding(column, binding);
        }),
        bindings.end());
}

void removeOperationSettingsBinding(std::vector<InputBinding>& bindings, const InputBinding& target)
{
    bindings.erase(
        std::remove_if(bindings.begin(), bindings.end(), [&target](const InputBinding& binding) {
            return operationSettingsBindingSamePhysicalInput(binding, target);
        }),
        bindings.end());
}

std::vector<InputAction> operationSettingsConflictingActions(
    const InputBindingMap& bindings,
    InputAction targetAction,
    const InputBinding& targetBinding)
{
    std::vector<InputAction> conflicts;
    for (int actionIndex = 0; actionIndex < InputActionCount; ++actionIndex) {
        const InputAction action = static_cast<InputAction>(actionIndex);
        if (action == targetAction) {
            continue;
        }
        for (const InputBinding& binding : bindings[actionIndex]) {
            if (operationSettingsBindingSamePhysicalInput(binding, targetBinding)) {
                conflicts.push_back(action);
                break;
            }
        }
    }
    return conflicts;
}

std::string operationSettingsConflictMessage(const std::vector<InputAction>& actions)
{
    std::string message = "この入力は別の操作に割り当て済みです。\n既存の割当を外して変更しますか？";
    if (!actions.empty()) {
        message += "\n対象: ";
        for (std::size_t i = 0; i < actions.size(); ++i) {
            if (i > 0) {
                message += ", ";
            }
            message += std::string(inputActionName(actions[i]));
        }
    }
    return message;
}

InputAction operationSettingsSelectedAction(const UiSelectableTableState& state, int category)
{
    const std::vector<OperationSettingsActionRow> rows = operationSettingsRowsForCategory(category);
    if (rows.empty()) {
        return InputAction::Count;
    }
    const int row = std::clamp(state.selectedRow, 0, static_cast<int>(rows.size()) - 1);
    return rows[static_cast<std::size_t>(row)].action;
}

void drawOperationSettingsCellText(
    Renderer& renderer,
    UiRect cell,
    std::string_view text,
    Color color,
    int scale,
    float paddingX)
{
    const Vec2 textSize = renderer.measureText(text, scale);
    const Vec2 textPos{
        cell.pos.x + paddingX,
        cell.pos.y + std::max(0.0f, (cell.size.y - textSize.y) * 0.5f),
    };
    renderer.drawText(textPos, text, color, scale);
}

void drawOperationSettingsBindingCell(
    Renderer& renderer,
    UiRect cell,
    std::string text,
    Color color,
    float paddingX)
{
    if (text == "未設定") {
        drawOperationSettingsCellText(renderer, cell, text, color, 2, paddingX);
        return;
    }

    InputHelpStyle helpStyle;
    helpStyle.text = color;
    helpStyle.scale = 2;
    helpStyle.iconHeight = 24.0f;
    const float maxWidth = std::max(1.0f, cell.size.x - paddingX * 2.0f);
    text = fittedInputHelpText(renderer, std::move(text), maxWidth, helpStyle);
    const Vec2 size = measureInputHelpText(renderer, text, helpStyle);
    const Vec2 pos{
        cell.pos.x + std::max(paddingX, (cell.size.x - size.x) * 0.5f),
        cell.pos.y + std::max(0.0f, (cell.size.y - size.y) * 0.5f),
    };
    drawInputHelpText(renderer, pos, text, helpStyle);
}

bool moveOperationSettingsTableColumn(UiSelectableTableState& state, int delta)
{
    if (delta == 0) {
        return false;
    }
    const int previous = std::clamp(
        state.selectedColumn,
        OperationSettingsColumnKeyboardMouse,
        OperationSettingsColumnGamepad);
    state.selectedColumn = delta > 0
        ? OperationSettingsColumnGamepad
        : OperationSettingsColumnKeyboardMouse;
    return state.selectedColumn != previous;
}

UiSelectableTableResult updateOperationSettingsTableClickSelection(
    UiSelectableTableState& state,
    UiContext& ui,
    const Input& input,
    UiRect rect,
    int rowCount,
    const UiSelectableTableColumn* columns,
    int columnCount,
    const UiSelectableTableStyle& style)
{
    UiSelectableTableResult result;
    if (rowCount <= 0 || columns == nullptr || columnCount <= 0) {
        state.selectedRow = 0;
        state.selectedColumn = OperationSettingsColumnKeyboardMouse;
        state.scrollOffset = 0.0f;
        return result;
    }

    state.selectedRow = std::clamp(state.selectedRow, 0, rowCount - 1);
    if (state.selectedColumn < OperationSettingsColumnKeyboardMouse ||
        state.selectedColumn > OperationSettingsColumnGamepad) {
        state.selectedColumn = OperationSettingsColumnKeyboardMouse;
    }

    UiSelectableTableLayout layout = makeUiSelectableTableLayout(rect, rowCount, state.scrollOffset, style);
    layout.scroll = updateUiScrollArea(
        ui,
        input,
        layout.scroll.viewport,
        uiSelectableTableContentHeight(rowCount, style),
        state.scrollOffset,
        style.scroll,
        &state.scroll);
    layout = makeUiSelectableTableLayout(rect, rowCount, state.scrollOffset, style);

    const int previousRow = state.selectedRow;
    const int previousColumn = state.selectedColumn;
    bool keyboardSelectionChanged = false;
    if (input.pressed(InputAction::MoveUp)) {
        state.selectedRow = (state.selectedRow + rowCount - 1) % rowCount;
        keyboardSelectionChanged = true;
    }
    if (input.pressed(InputAction::MoveDown)) {
        state.selectedRow = (state.selectedRow + 1) % rowCount;
        keyboardSelectionChanged = true;
    }
    if (input.pressed(InputAction::MoveLeft)) {
        keyboardSelectionChanged = moveOperationSettingsTableColumn(state, -1) || keyboardSelectionChanged;
    }
    if (input.pressed(InputAction::MoveRight)) {
        keyboardSelectionChanged = moveOperationSettingsTableColumn(state, 1) || keyboardSelectionChanged;
    }

    for (int row = 0; row < rowCount; ++row) {
        const UiRect rowRect = uiSelectableTableRowRect(layout, row, style);
        if (!uiScrollAreaRectVisible(layout.scroll, rowRect)) {
            continue;
        }
        for (int column = OperationSettingsColumnAction; column <= OperationSettingsColumnGamepad; ++column) {
            const UiRect cellRect = uiSelectableTableCellRect(layout, columns, columnCount, row, column, style);
            if (ui.pressed(cellRect)) {
                const bool sameRow = state.selectedRow == row;
                state.selectedRow = row;
                result.pressedRow = row;
                if (column >= OperationSettingsColumnKeyboardMouse) {
                    if (sameRow && state.selectedColumn == column) {
                        result.pressedColumn = column;
                    }
                    state.selectedColumn = column;
                } else {
                    result.pressedColumn = OperationSettingsColumnAction;
                }
            }
        }
    }

    result.selectionChanged = state.selectedRow != previousRow || state.selectedColumn != previousColumn;
    ui.emitCursorMoveIfChanged(
        previousRow * columnCount + previousColumn,
        state.selectedRow * columnCount + state.selectedColumn);
    if (keyboardSelectionChanged || result.pressedRow >= 0) {
        keepUiSelectableTableCellVisible(rect, state.selectedRow, rowCount, state.scrollOffset, style);
    }
    return result;
}

} // namespace

bool Game::rewardNodeVisibleOnDungeonMap(const RewardNode& node) const
{
    if (node.collected) {
        return false;
    }
    return node.visibility == PlacementVisibility::Exposed ||
        node.revealed ||
        node.detectorRevealed;
}

bool Game::moneyNodeVisibleOnDungeonMap(const MoneyNode& node) const
{
    if (node.collected) {
        return false;
    }
    return node.visibility == PlacementVisibility::Exposed ||
        node.detectorRevealed;
}

bool Game::moonFragmentNodeVisibleOnDungeonMap(const MoonFragmentNode& node) const
{
    return !node.collected && node.visibility == PlacementVisibility::Exposed;
}

bool Game::chestNodeVisibleOnDungeonMap(const ChestNode& node) const
{
    if (node.opened || node.mimicTriggered) {
        return false;
    }
    return node.visibility == PlacementVisibility::Exposed ||
        node.revealed ||
        node.detectorRevealed;
}

void Game::updateRingScreen(const Input& input, UiContext& ui, float dt)
{
    if (!introTutorialActive()) {
        queueStoryEventForTrigger("tutorial:ring_equip");
    }

    const int ringCount = unlockedRingCount();
    if (spellRing_.activeRingIndex() >= ringCount) {
        clampActiveRingToUnlocked();
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ringPlaceModeActive_ = false;
        ringEmptyPressActive_ = false;
        ringItemMoveModeActive_ = false;
        ringItemMoveIndex_ = -1;
        ringSnapActive_ = false;
        ringDragItemIndex_ = -1;
        ringStatus_.clear();
    }

    const auto clearRingTransientUi = [this]() {
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ringDiscardConfirm_ = {};
        ringDiscardConfirmItemIndex_ = -1;
        ringPlaceModeActive_ = false;
        ringEmptyPressActive_ = false;
        ringItemMoveModeActive_ = false;
        ringItemMoveIndex_ = -1;
        ringSnapActive_ = false;
        ringDragItemIndex_ = -1;
    };
    const int registerPreset = input.ringPresetRegisterSlotPressed();
    if (registerPreset >= 0 && registerPreset < RingPresetSlotCount) {
        clearRingTransientUi();
        ui.emitSound(registerRingPresetShortcut(registerPreset)
            ? UiSoundEvent::Confirm
            : UiSoundEvent::Cancel);
        ui.block(ringPanelRect());
        return;
    }
    const int applyPreset = input.shortcutSlotPressed();
    if (applyPreset >= 0 && applyPreset < RingPresetSlotCount) {
        clearRingTransientUi();
        ui.emitSound(applyRingPresetShortcut(applyPreset)
            ? UiSoundEvent::Confirm
            : UiSoundEvent::Cancel);
        ui.block(ringPanelRect());
        return;
    }

    auto& items = spellRing_.items();
    if (ringSnapActive_) {
        if (ringDragItemIndex_ < 0 || ringDragItemIndex_ >= static_cast<int>(items.size())) {
            ringSnapActive_ = false;
            ringDragItemIndex_ = -1;
        } else {
            ringSnapElapsed_ = std::min(RingSnapDuration, ringSnapElapsed_ + dt);
            const float t = RingSnapDuration <= 0.0f ? 1.0f : clamp(ringSnapElapsed_ / RingSnapDuration, 0.0f, 1.0f);
            const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
            ringDragDisplayAngle_ = spellRing_.normalizeLocalAngle(
                ringSnapStartAngle_ + shortestRingAngleDelta(ringSnapStartAngle_, ringSnapTargetAngle_, spellRing_.runtimeRingShape(), balance_) * eased,
                balance_);
            if (t >= 1.0f) {
                items[static_cast<std::size_t>(ringDragItemIndex_)].localAngle = ringSnapTargetAngle_;
                ringSnapActive_ = false;
                ringDragItemIndex_ = -1;
            }
        }
        ui.block(ringPanelRect());
        return;
    }

    if (!items.empty()) {
        ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()) - 1);
    } else {
        ringSlotSelection_ = 0;
    }

    if (ringDiscardConfirm_.open) {
        if (pauseReturnMode_ != ScreenMode::Base ||
            ringDiscardConfirmItemIndex_ < 0 ||
            ringDiscardConfirmItemIndex_ >= static_cast<int>(items.size())) {
            ringDiscardConfirm_ = {};
            ringDiscardConfirmItemIndex_ = -1;
            ui.block(ringPanelRect());
            return;
        }

        const UiConfirmDialogResult result = updateUiConfirmDialog(
            ringDiscardConfirm_,
            ui,
            input,
            ringDiscardConfirmRect());
        if (result == UiConfirmDialogResult::Confirmed) {
            ringSlotSelection_ = ringDiscardConfirmItemIndex_;
            std::vector<InventoryDiscardRequest> discardRequests;
            const bool discarded = discardRingItem(
                items,
                ringSlotSelection_,
                inventory_,
                objectCatalog_,
                discardRequests,
                ringStatus_);
            (void)discardRequests;
            ui.emitSound(discarded ? UiSoundEvent::ItemUse : UiSoundEvent::Cancel);
            ringDiscardConfirmItemIndex_ = -1;
        } else if (result == UiConfirmDialogResult::Cancelled) {
            ringDiscardConfirmItemIndex_ = -1;
        }
        ui.block(ringPanelRect());
        return;
    }

    if (ringCommandMenu_.open && uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ui.block(ringPanelRect());
        return;
    }

    const int commandItemIndex = ringCommandItemIndex_ >= 0 ? ringCommandItemIndex_ : ringSlotSelection_;
    const bool commandCanRemove = commandItemIndex >= 0 &&
        commandItemIndex < static_cast<int>(items.size()) &&
        !items[static_cast<std::size_t>(commandItemIndex)].objectId.empty();
    const bool commandCanPlace = ringCommandPlaceActive_ &&
        firstRingPlaceableSlot(inventory_, spellRing_, ringCommandPlaceAngle_) >= 0;
    const SpellRingItem* commandItem = !ringCommandPlaceActive_ && commandItemIndex >= 0 && commandItemIndex < static_cast<int>(items.size())
        ? &items[static_cast<std::size_t>(commandItemIndex)]
        : nullptr;
    const RingCommandMenuItems commandMenuItems = ringCommandItems(
        ringCommandPlaceActive_,
        commandItem,
        objectCatalog_,
        commandCanPlace,
        commandCanRemove);
    const int commandSelection = updateUiCommandMenu(
        ringCommandMenu_,
        ui,
        input,
        commandMenuItems.items.data(),
        static_cast<int>(commandMenuItems.items.size()));
    if (commandSelection >= 0) {
        const RingCommandAction action = commandSelection < static_cast<int>(commandMenuItems.actions.size())
            ? commandMenuItems.actions[static_cast<std::size_t>(commandSelection)]
            : RingCommandAction::Remove;
        if (action == RingCommandAction::Place) {
            const int firstSlot = firstRingPlaceableSlot(inventory_, spellRing_, ringCommandPlaceAngle_);
            if (firstSlot >= 0) {
                ringPlaceModeActive_ = true;
                ringPlaceTargetAngle_ = ringCommandPlaceAngle_;
                ringPlaceSelection_ = firstSlot;
                ringStatus_.clear();
            } else {
                ringStatus_ = ringPlacementUnavailableStatus(inventory_, spellRing_);
            }
        } else if (ringCommandItemIndex_ >= 0) {
            ringSlotSelection_ = ringCommandItemIndex_;
            ringDetailShowsRing_ = false;
            bool commandSucceeded = false;
            switch (action) {
            case RingCommandAction::Place:
                break;
            case RingCommandAction::Remove:
                commandSucceeded = removeRingItemToInventory(items, ringSlotSelection_, inventory_, objectCatalog_, ringStatus_);
                ui.emitSound(commandSucceeded ? UiSoundEvent::ItemMove : UiSoundEvent::Cancel);
                break;
            case RingCommandAction::ToggleProtection:
                commandSucceeded = toggleRingItemProtection(items, ringSlotSelection_, ringStatus_);
                ui.emitSound(commandSucceeded ? UiSoundEvent::Confirm : UiSoundEvent::Cancel);
                break;
            case RingCommandAction::Discard:
            {
                if (pauseReturnMode_ == ScreenMode::Base) {
                    openUiConfirmDialog(
                        ringDiscardConfirm_,
                        "確認",
                        "",
                        "捨てる",
                        "戻る",
                        1);
                    ringDiscardConfirmItemIndex_ = ringSlotSelection_;
                    closeUiCommandMenu(ringCommandMenu_);
                    commandSucceeded = true;
                } else {
                    std::vector<InventoryDiscardRequest> discardRequests;
                    commandSucceeded = discardRingItem(
                        items,
                        ringSlotSelection_,
                        inventory_,
                        objectCatalog_,
                        discardRequests,
                        ringStatus_);
                    if (commandSucceeded) {
                        spawnInventoryDiscardRequests(std::move(discardRequests));
                    }
                    ui.emitSound(commandSucceeded ? UiSoundEvent::ItemUse : UiSoundEvent::Cancel);
                }
                break;
            }
            }
        }
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ui.block(ringPanelRect());
        return;
    }
    if (ringCommandMenu_.open) {
        ui.block(ringPanelRect());
        return;
    }
    if (!ringCommandMenu_.visible) {
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
    }

    if (ringPlaceModeActive_) {
        const int slotCount = std::min(inventory_.screenSlotCount(), RingPlaceSlotCount);
        const int firstSlot = firstRingPlaceableSlot(inventory_, spellRing_, ringPlaceTargetAngle_);
        if (firstSlot < 0) {
            ringPlaceModeActive_ = false;
            ringStatus_ = ringPlacementUnavailableStatus(inventory_, spellRing_);
            ui.block(ringPanelRect());
            return;
        }
        ringPlaceSelection_ = std::clamp(ringPlaceSelection_, 0, std::max(0, slotCount - 1));
        if (!ringPlaceSlotEnabled(inventory_, spellRing_, ringPlaceSelection_, ringPlaceTargetAngle_)) {
            ringPlaceSelection_ = firstSlot;
        }

        if (uiCancelRequested(ringCancelState_, input, ui, ringPlaceWindowRect())) {
            ringPlaceModeActive_ = false;
            ringStatus_ = "配置をキャンセルしました";
            ui.block(ringPanelRect());
            return;
        }

        const int previousRingPlaceSelection = ringPlaceSelection_;
        const auto tryPlaceSelection = [&]() {
            if (!ringPlaceSlotEnabled(inventory_, spellRing_, ringPlaceSelection_, ringPlaceTargetAngle_)) {
                ui.emitSound(UiSoundEvent::Cancel);
                ringStatus_ = "このアイテムは配置できません";
                return;
            }

            SpellRingAddResult result{};
            if (inventory_.addScreenItemToRing(
                    ringPlaceSelection_,
                    spellRing_,
                    std::optional<float>{ringPlaceTargetAngle_},
                    &result)) {
                ringSlotSelection_ = std::max(0, result.itemIndex);
                ringDetailShowsRing_ = false;
                ringPlaceModeActive_ = false;
                ringStatus_ = "リングに配置しました";
                ui.emitSound(UiSoundEvent::Equip);
            } else {
                ui.emitSound(UiSoundEvent::Cancel);
                ringStatus_ = ringPlacementUnavailableStatus(inventory_, spellRing_);
            }
        };

        if (input.pressed(InputAction::MoveLeft)) {
            ringPlaceSelection_ = movedRingPlaceSelection(inventory_, spellRing_, ringPlaceTargetAngle_, ringPlaceSelection_, -1);
        }
        if (input.pressed(InputAction::MoveRight)) {
            ringPlaceSelection_ = movedRingPlaceSelection(inventory_, spellRing_, ringPlaceTargetAngle_, ringPlaceSelection_, 1);
        }
        if (input.pressed(InputAction::MoveUp)) {
            ringPlaceSelection_ = movedRingPlaceSelection(
                inventory_,
                spellRing_,
                ringPlaceTargetAngle_,
                ringPlaceSelection_,
                -RingPlaceColumns);
        }
        if (input.pressed(InputAction::MoveDown)) {
            ringPlaceSelection_ = movedRingPlaceSelection(
                inventory_,
                spellRing_,
                ringPlaceTargetAngle_,
                ringPlaceSelection_,
                RingPlaceColumns);
        }
        if (input.shortcutCursorDelta() != 0) {
            ringPlaceSelection_ = movedRingPlaceSelection(
                inventory_,
                spellRing_,
                ringPlaceTargetAngle_,
                ringPlaceSelection_,
                input.shortcutCursorDelta());
        }

        for (int i = 0; i < slotCount; ++i) {
            const UiRect rect = ringPlaceSlotRect(i);
            const bool enabled = ringPlaceSlotEnabled(inventory_, spellRing_, i, ringPlaceTargetAngle_);
            if (enabled && rect.contains(ui.mouse())) {
                ringPlaceSelection_ = i;
            }
            if (input.mouseLeftPressed() && rect.contains(ui.mouse()) && !ui.pointerConsumed()) {
                ui.consumePointer();
                if (enabled) {
                    ringPlaceSelection_ = i;
                    ui.emitCursorMoveIfChanged(previousRingPlaceSelection, ringPlaceSelection_);
                    tryPlaceSelection();
                } else if (inventory_.hasScreenItemAt(i)) {
                    ui.emitSound(UiSoundEvent::Cancel);
                    ringStatus_ = "このアイテムは配置できません";
                }
                ui.block(ringPanelRect());
                return;
            }
        }

        if (input.confirmPressed() || input.useItemPressed()) {
            ui.emitCursorMoveIfChanged(previousRingPlaceSelection, ringPlaceSelection_);
            tryPlaceSelection();
            ui.block(ringPanelRect());
            return;
        }

        ui.emitCursorMoveIfChanged(previousRingPlaceSelection, ringPlaceSelection_);
        ui.block(ringPanelRect());
        return;
    }

    if (ringItemMoveModeActive_) {
        if (ringItemMoveIndex_ < 0 || ringItemMoveIndex_ >= static_cast<int>(items.size())) {
            ringItemMoveModeActive_ = false;
            ringItemMoveIndex_ = -1;
            ui.block(ringPanelRect());
            return;
        }

        ringSlotSelection_ = ringItemMoveIndex_;
        ringDetailShowsRing_ = false;

        if (uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
            items[static_cast<std::size_t>(ringItemMoveIndex_)].localAngle = ringItemMoveOriginalAngle_;
            ringItemMoveModeActive_ = false;
            ringItemMoveIndex_ = -1;
            ringStatus_ = "移動をキャンセルしました";
            ui.emitSound(UiSoundEvent::Cancel);
            ui.block(ringPanelRect());
            return;
        }

        if (input.confirmPressed() || input.useItemPressed()) {
            ringItemMoveModeActive_ = false;
            ringItemMoveIndex_ = -1;
            ringStatus_ = "位置を確定しました";
            ui.emitSound(UiSoundEvent::Confirm);
            ui.block(ringPanelRect());
            return;
        }

        const Vec2 moveDirection = ringPressedDirection(input);
        if (lengthSquared(moveDirection) > 0.0001f) {
            if (moveRingItemByDirection(spellRing_, balance_, ringItemMoveIndex_, static_cast<int>(items.size()), moveDirection)) {
                ui.emitSound(UiSoundEvent::ItemMove);
                ringStatus_.clear();
            } else {
                ui.emitSound(UiSoundEvent::Cancel);
                ringStatus_ = "その位置には移動できません";
            }
        }

        ui.block(ringPanelRect());
        return;
    }

    std::array<UiTabItem, SpellRingCount> ringTabs{};
    std::array<UiRect, SpellRingCount> ringTabRects{};
    std::array<std::string, SpellRingCount> ringTabLabels{};
    for (int i = 0; i < ringCount; ++i) {
        ringTabLabels[static_cast<std::size_t>(i)] = ringDisplayName(i, ringCount);
        ringTabs[static_cast<std::size_t>(i)] = {ringTabLabels[static_cast<std::size_t>(i)], true};
        ringTabRects[static_cast<std::size_t>(i)] = ringTabRect(i, ringCount);
    }
    UiTabsInput ringTabsInput{};
    ringTabsInput.focusDelta = input.activeRingDelta();
    ringTabsInput.commit = input.confirmPressed() || input.useItemPressed();
    const int ringTabSelection = updateUiTabs(
        ringTabs_,
        ui,
        ringTabsInput,
        spellRing_.activeRingIndex(),
        ringTabs.data(),
        ringCount,
        ringTabRects.data());
    if (ringTabSelection >= 0) {
        ringDetailShowsRing_ = true;
        if (ringTabSelection != spellRing_.activeRingIndex()) {
            spellRing_.switchActiveRing(ringTabSelection - spellRing_.activeRingIndex());
            player_.spellRingShiftDistanceBonus = effectiveRingShiftDistanceForRing(spellRing_.activeRingIndex()) -
                balance_.spellRingShiftDistance;
            closeUiCommandMenu(ringCommandMenu_);
            ringCommandItemIndex_ = -1;
            ringCommandPlaceActive_ = false;
            ringPlaceModeActive_ = false;
            ringEmptyPressActive_ = false;
            ringItemMoveModeActive_ = false;
            ringItemMoveIndex_ = -1;
            ringStatus_.clear();
        }
        return;
    }

    const bool actualRing = true;

    if (input.arrangeItemsPressed()) {
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        if (arrangeActiveRingItemsEvenly(spellRing_, balance_)) {
            ui.emitSound(UiSoundEvent::ItemMove);
            ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()) - 1);
            ringStatus_ = "等間隔に整列しました";
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
            ringStatus_ = "アイテム未配置です";
        }
        ui.block(ringPanelRect());
        return;
    }

    if (input.removeAllRingItemsPressed()) {
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ringPlaceModeActive_ = false;
        ringEmptyPressActive_ = false;
        ringItemMoveModeActive_ = false;
        ringItemMoveIndex_ = -1;
        ringDetailShowsRing_ = false;
        ui.emitSound(removeAllRingItemsToInventory(items, ringSlotSelection_, inventory_, objectCatalog_, ringStatus_)
            ? UiSoundEvent::ItemMove
            : UiSoundEvent::Cancel);
        ui.block(ringPanelRect());
        return;
    }

    if ((ringDragPending_ || ringDragActive_) && uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
        ringDragPending_ = false;
        ringDragActive_ = false;
        ringDragItemIndex_ = -1;
        ringStatus_ = "ドラッグをキャンセルしました";
        ui.block(ringPanelRect());
        return;
    }

    if (ringDragPending_ || ringDragActive_) {
        if (ringDragItemIndex_ < 0 || ringDragItemIndex_ >= static_cast<int>(items.size())) {
            ringDragPending_ = false;
            ringDragActive_ = false;
            ringDragItemIndex_ = -1;
            ui.block(ringPanelRect());
            return;
        }

        if (input.mouseLeftHeld()) {
            if (!ringDragActive_ && distanceSquared(input.mouseScreen(), ringDragStartMouse_) >= 36.0f) {
                ringDragActive_ = true;
                ringDragPending_ = false;
            }
            if (ringDragActive_) {
                ringDragDisplayAngle_ = ringAngleFromPoint(input.mouseScreen(), spellRing_, balance_);
            }
        }

        if (input.mouseLeftReleased()) {
            if (ringDragActive_) {
                ui.emitSound(UiSoundEvent::ItemMove);
                const float desired = ringAngleFromPoint(input.mouseScreen(), spellRing_, balance_);
                const std::optional<float> snapAngle = spellRing_.nearestPlaceableAngle(ringDragItemIndex_, desired, RingDragSnapMaxDelta);
                ringSnapStartAngle_ = desired;
                ringSnapTargetAngle_ = snapAngle.value_or(ringDragOriginalAngle_);
                ringDragDisplayAngle_ = ringSnapStartAngle_;
                ringSnapElapsed_ = 0.0f;
                ringSnapActive_ = true;
                ringStatus_ = snapAngle ? "近い空き位置へ補正しました" : "近くに空きがないため元の位置へ戻しました";
            } else {
                ringSlotSelection_ = ringDragItemIndex_;
                ringCommandItemIndex_ = ringDragItemIndex_;
                ringCommandPlaceActive_ = false;
                const bool canRemove = !items[static_cast<std::size_t>(ringDragItemIndex_)].objectId.empty();
                const RingCommandMenuItems menuItems = ringCommandItems(
                    false,
                    &items[static_cast<std::size_t>(ringDragItemIndex_)],
                    objectCatalog_,
                    false,
                    canRemove);
                openUiCommandMenu(
                    ringCommandMenu_,
                    input.mouseScreen(),
                    ringPanelRect(),
                    static_cast<int>(menuItems.items.size()),
                    menuItems.items.data(),
                    180.0f,
                    2);
                ringStatus_.clear();
            }
            ringDragPending_ = false;
            ringDragActive_ = false;
            ui.block(ringPanelRect());
            return;
        }

        ui.block(ringPanelRect());
        return;
    }

    if (ringEmptyPressActive_) {
        if (input.mouseLeftHeld() &&
            distanceSquared(input.mouseScreen(), ringEmptyPressMouse_) >= 36.0f) {
            ringEmptyPressActive_ = false;
        }

        if (input.mouseLeftReleased()) {
            if (ringEmptyPressActive_) {
                const float placeAngle = ringEmptyPressAngle_;
                if (firstRingPlaceableSlot(inventory_, spellRing_, placeAngle) < 0) {
                    ringStatus_ = ringPlacementUnavailableStatus(inventory_, spellRing_);
                } else {
                    ringCommandItemIndex_ = -1;
                    ringCommandPlaceActive_ = true;
                    ringCommandPlaceAngle_ = placeAngle;
                    const RingCommandMenuItems menuItems = ringCommandItems(true, nullptr, objectCatalog_, true, false);
                    openUiCommandMenu(
                        ringCommandMenu_,
                        input.mouseScreen(),
                        ringPanelRect(),
                        static_cast<int>(menuItems.items.size()),
                        menuItems.items.data(),
                        190.0f,
                        2);
                    ringStatus_.clear();
                }
            }
            ringEmptyPressActive_ = false;
            ui.block(ringPanelRect());
            return;
        }
    }

    if (uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
        mode_ = ScreenMode::PauseMenu;
        pausePage_ = PauseMenuPage::Main;
        return;
    }

    const int previousRingSlotSelection = ringSlotSelection_;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const UiRect rect = ringItemUiRect(items[static_cast<std::size_t>(i)], spellRing_, balance_, i, static_cast<int>(items.size()));
        if (rect.contains(ui.mouse())) {
            ringSlotSelection_ = i;
            ringDetailShowsRing_ = false;
        }
        if (ui.pressed(rect)) {
            closeUiCommandMenu(ringCommandMenu_);
            ringCommandItemIndex_ = -1;
            ringCommandPlaceActive_ = false;
            ringEmptyPressActive_ = false;
            ringSlotSelection_ = i;
            ringDetailShowsRing_ = false;
            ringDragPending_ = true;
            ringDragActive_ = false;
            ringDragItemIndex_ = i;
            ringDragOriginalAngle_ = items[static_cast<std::size_t>(i)].localAngle;
            ringDragDisplayAngle_ = ringDragOriginalAngle_;
            ringDragStartMouse_ = input.mouseScreen();
            ui.emitCursorMoveIfChanged(previousRingSlotSelection, ringSlotSelection_);
            return;
        }
    }

    if (input.mouseLeftPressed() &&
        !ui.pointerConsumed() &&
        ringPlacementHitAreaContains(ui.mouse(), spellRing_, balance_)) {
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ringEmptyPressActive_ = true;
        ringEmptyPressMouse_ = input.mouseScreen();
        ringEmptyPressAngle_ = ringAngleFromPoint(input.mouseScreen(), spellRing_, balance_);
        ui.consumePointer();
        return;
    }

    ui.block(ringPanelRect());

    const Vec2 selectionDirection = ringPressedDirection(input);
    if (!items.empty() && lengthSquared(selectionDirection) > 0.0001f) {
        ringSlotSelection_ = ringItemSelectionByDirection(
            items,
            spellRing_,
            balance_,
            ringSlotSelection_,
            selectionDirection);
        ringDetailShowsRing_ = false;
        ringStatus_.clear();
    }
    ui.emitCursorMoveIfChanged(previousRingSlotSelection, ringSlotSelection_);

    (void)actualRing;

    if (input.addRingPressed()) {
        ringDetailShowsRing_ = false;
        if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ui.emitSound(removeRingItemToInventory(items, ringSlotSelection_, inventory_, objectCatalog_, ringStatus_)
                ? UiSoundEvent::ItemMove
                : UiSoundEvent::Cancel);
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
            ringStatus_ = "アイテム未選択です";
        }
        return;
    }

    if (input.pressed(InputAction::ToggleProtection)) {
        ringDetailShowsRing_ = false;
        if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ui.emitSound(toggleRingItemProtection(items, ringSlotSelection_, ringStatus_)
                ? UiSoundEvent::Confirm
                : UiSoundEvent::Cancel);
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
            ringStatus_ = "アイテム未選択です";
        }
        return;
    }

    if (input.useItemPressed() || input.confirmPressed()) {
        ringDetailShowsRing_ = false;
        if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ui.emitSound(UiSoundEvent::Confirm);
            ringItemMoveModeActive_ = true;
            ringItemMoveIndex_ = ringSlotSelection_;
            ringItemMoveOriginalAngle_ = items[static_cast<std::size_t>(ringSlotSelection_)].localAngle;
            ringStatus_ = "移動モード";
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
            ringStatus_ = "アイテム未選択です";
        }
    }
}

void Game::openOptionsMenu()
{
    pausePage_ = PauseMenuPage::Options;
    operationSettingsCapture_.cancel();
    operationSettingsConflictConfirm_ = {};
    operationSettingsResetAllConfirm_ = {};
    operationSettingsPendingAction_ = InputAction::Count;
    operationSettingsConflictActions_.clear();
    optionsStatus_.clear();
    operationSettingsStatus_.clear();
    loadOptionsSettings();
}

void Game::loadOptionsSettings()
{
    optionsSettings_ = settingsGetter_
        ? sanitizeSettings(settingsGetter_())
        : GameSettings{};
    if (!settingsGetter_ && inputBindingGetter_) {
        optionsSettings_.input.bindings = sanitizeInputBindings(inputBindingGetter_());
    }
    lightweightModeActive_ = optionsSettings_.performance.lightweight;
    presentationSettingsActive_ = optionsSettings_.presentation;
    operationSettingsBindings_ = optionsSettings_.input.bindings;
    optionsSettingsLoaded_ = true;
    operationSettingsLoaded_ = true;
    optionsPage_ = std::clamp(optionsPage_, 0, OptionsPageCount - 1);
    operationSettingsCategory_ = std::clamp(operationSettingsCategory_, 0, OperationSettingsCategoryCount - 1);
    operationSettingsTable_.selectedColumn = OperationSettingsColumnKeyboardMouse;
    operationSettingsTable_.selectedRow = 0;
    operationSettingsTable_.scrollOffset = 0.0f;
    audioSettingsSelection_ = std::clamp(audioSettingsSelection_, 0, AudioSettingsRowCount - 1);
    videoSettingsSelection_ = std::clamp(videoSettingsSelection_, 0, VideoSettingsRowCount - 1);
}

void Game::applyOptionsSettings(std::string status)
{
    optionsSettings_ = sanitizeSettings(optionsSettings_);
    lightweightModeActive_ = optionsSettings_.performance.lightweight;
    presentationSettingsActive_ = optionsSettings_.presentation;
    operationSettingsBindings_ = optionsSettings_.input.bindings;
    if (settingsApplier_) {
        settingsApplier_(optionsSettings_);
    } else if (inputBindingApplier_) {
        inputBindingApplier_(operationSettingsBindings_);
    }
    optionsStatus_ = std::move(status);
}

void Game::queueOperationSettingsBinding(InputAction action, int column, const InputBinding& binding)
{
    operationSettingsPendingAction_ = action;
    operationSettingsPendingColumn_ = column;
    operationSettingsPendingBinding_ = binding;
    operationSettingsConflictActions_ = operationSettingsConflictingActions(operationSettingsBindings_, action, binding);
    if (!operationSettingsConflictActions_.empty()) {
        openUiConfirmDialog(
            operationSettingsConflictConfirm_,
            "割当の確認",
            operationSettingsConflictMessage(operationSettingsConflictActions_),
            "変更する",
            "戻る",
            1);
        return;
    }
    applyOperationSettingsBinding(action, column, binding, false);
}

void Game::applyOperationSettingsBinding(InputAction action, int column, const InputBinding& binding, bool removeConflicts)
{
    if (action == InputAction::Count) {
        return;
    }

    InputBindingMap candidate = operationSettingsBindings_;
    if (removeConflicts) {
        for (InputAction conflict : operationSettingsConflictActions_) {
            removeOperationSettingsBinding(candidate[inputActionIndex(conflict)], binding);
        }
    }
    auto& target = candidate[inputActionIndex(action)];
    removeOperationSettingsColumnBindings(target, column);
    target.push_back(binding);
    candidate = sanitizeInputBindings(candidate);

    operationSettingsBindings_ = candidate;
    optionsSettings_.input.bindings = operationSettingsBindings_;
    applyOptionsSettings("");
    operationSettingsStatus_ = "割当を変更しました: " + operationSettingsBindingDisplayName(binding);
    operationSettingsPendingAction_ = InputAction::Count;
    operationSettingsConflictActions_.clear();
}

void Game::clearOperationSettingsBinding(InputAction action, int column)
{
    if (action == InputAction::Count) {
        return;
    }

    InputBindingMap candidate = operationSettingsBindings_;
    auto& target = candidate[inputActionIndex(action)];
    removeOperationSettingsColumnBindings(target, column);
    if (inputActionRequiresBinding(action) && target.empty()) {
        operationSettingsStatus_ = "必須操作はすべて未設定にはできません";
        return;
    }
    candidate = sanitizeInputBindings(candidate);
    operationSettingsBindings_ = candidate;
    optionsSettings_.input.bindings = operationSettingsBindings_;
    applyOptionsSettings("");
    operationSettingsStatus_ = "割当を削除しました";
}

void Game::resetOperationSettingsAction(InputAction action)
{
    if (action == InputAction::Count) {
        return;
    }
    InputBindingMap candidate = operationSettingsBindings_;
    const InputBindingMap defaults = defaultInputBindings();
    candidate[inputActionIndex(action)] = defaults[inputActionIndex(action)];
    operationSettingsBindings_ = sanitizeInputBindings(candidate);
    optionsSettings_.input.bindings = operationSettingsBindings_;
    applyOptionsSettings("");
    operationSettingsStatus_ = "選択中の操作を初期化しました";
}

void Game::resetOperationSettingsCategory()
{
    InputBindingMap candidate = operationSettingsBindings_;
    const InputBindingMap defaults = defaultInputBindings();
    for (const OperationSettingsActionRow& row : operationSettingsRowsForCategory(operationSettingsCategory_)) {
        candidate[inputActionIndex(row.action)] = defaults[inputActionIndex(row.action)];
    }
    operationSettingsBindings_ = sanitizeInputBindings(candidate);
    optionsSettings_.input.bindings = operationSettingsBindings_;
    applyOptionsSettings("");
    operationSettingsStatus_ = "現在の分類を初期化しました";
}

void Game::resetOperationSettingsAll()
{
    operationSettingsBindings_ = defaultInputBindings();
    optionsSettings_.input.bindings = operationSettingsBindings_;
    applyOptionsSettings("");
    operationSettingsStatus_ = "すべての操作を初期化しました";
}

bool Game::handleOperationSettingsEvent(const SDL_Event& event)
{
    if (mode_ != ScreenMode::PauseMenu ||
        pausePage_ != PauseMenuPage::Options ||
        optionsPage_ != OptionsPageOperation ||
        !operationSettingsCapture_.active() ||
        !operationSettingsCapture_.shouldConsumeEvent(event)) {
        return false;
    }

    optionsSuppressCancelThisFrame_ = true;
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event.button.button == SDL_BUTTON_LEFT &&
        uiCancelButtonRect(operationSettingsDialogRect()).contains(Vec2{
            static_cast<float>(event.button.x),
            static_cast<float>(event.button.y),
        })) {
        operationSettingsCapture_.cancel();
        operationSettingsStatus_ = "変更をキャンセルしました";
        return true;
    }

    const InputAction action = operationSettingsCapture_.action();
    const int column = operationSettingsPendingColumn_;
    InputBinding binding{};
    const InputRemapCaptureResult result = operationSettingsCapture_.handleEvent(event, binding);
    if (result == InputRemapCaptureResult::Captured) {
        queueOperationSettingsBinding(action, column, binding);
    } else if (result == InputRemapCaptureResult::ClearRequested) {
        clearOperationSettingsBinding(action, column);
    } else if (result == InputRemapCaptureResult::Cancelled) {
        operationSettingsStatus_ = "変更をキャンセルしました";
    }
    return true;
}

void Game::updateOperationSettings(const Input& input, UiContext& ui)
{
    if (!optionsSettingsLoaded_ || !operationSettingsLoaded_) {
        loadOptionsSettings();
    }

    const UiRect panel = optionsPanelRect();
    const UiRect table = operationSettingsTableRect();
    const UiRect dialog = operationSettingsDialogRect();

    if (operationSettingsConflictConfirm_.open) {
        const UiConfirmDialogResult result = updateUiConfirmDialog(operationSettingsConflictConfirm_, ui, input, dialog);
        if (result == UiConfirmDialogResult::Confirmed) {
            applyOperationSettingsBinding(
                operationSettingsPendingAction_,
                operationSettingsPendingColumn_,
                operationSettingsPendingBinding_,
                true);
        } else if (result == UiConfirmDialogResult::Cancelled) {
            operationSettingsPendingAction_ = InputAction::Count;
            operationSettingsConflictActions_.clear();
            operationSettingsStatus_ = "変更をキャンセルしました";
        }
        ui.block(panel);
        return;
    }

    if (operationSettingsResetAllConfirm_.open) {
        const UiConfirmDialogResult result = updateUiConfirmDialog(operationSettingsResetAllConfirm_, ui, input, dialog);
        if (result == UiConfirmDialogResult::Confirmed) {
            resetOperationSettingsAll();
        } else if (result == UiConfirmDialogResult::Cancelled) {
            operationSettingsStatus_ = "初期化をキャンセルしました";
        }
        ui.block(panel);
        return;
    }

    if (operationSettingsCapture_.active()) {
        if (ui.pressed(uiCancelButtonRect(operationSettingsDialogRect()))) {
            operationSettingsCapture_.cancel();
            operationSettingsStatus_ = "変更をキャンセルしました";
            ui.emitSound(UiSoundEvent::Cancel);
            ui.block(panel);
            return;
        }
        ui.block(panel);
        return;
    }

    const int categoryDelta = input.mouseWheelDelta() == 0 ? input.shortcutCursorDelta() : 0;
    if (categoryDelta != 0) {
        operationSettingsCategory_ =
            (operationSettingsCategory_ + categoryDelta + OperationSettingsCategoryCount) % OperationSettingsCategoryCount;
        operationSettingsTable_.selectedRow = 0;
        operationSettingsTable_.scrollOffset = 0.0f;
        ui.emitSound(UiSoundEvent::TabSwitch);
    }

    std::array<UiTabItem, OperationSettingsCategoryCount> tabItems{};
    for (int i = 0; i < OperationSettingsCategoryCount; ++i) {
        tabItems[static_cast<std::size_t>(i)] = {OperationSettingsCategoryLabels[i], true};
    }
    const auto tabRects = operationSettingsTabRects();
    UiTabsInput tabsInput{};
    const int selectedTab = updateUiSubTabs(
        operationSettingsTabs_,
        ui,
        tabsInput,
        operationSettingsCategory_,
        tabItems.data(),
        static_cast<int>(tabItems.size()),
        tabRects.data());
    if (selectedTab >= 0 && selectedTab != operationSettingsCategory_) {
        operationSettingsCategory_ = selectedTab;
        operationSettingsTable_.selectedRow = 0;
        operationSettingsTable_.scrollOffset = 0.0f;
    }

    const std::vector<OperationSettingsActionRow> rows = operationSettingsRowsForCategory(operationSettingsCategory_);
    operationSettingsTable_.selectedRow = std::clamp(
        operationSettingsTable_.selectedRow,
        0,
        std::max(0, static_cast<int>(rows.size()) - 1));
    if (operationSettingsTable_.selectedColumn <= OperationSettingsColumnAction) {
        operationSettingsTable_.selectedColumn = OperationSettingsColumnKeyboardMouse;
    }

    const auto columns = operationSettingsTableColumns();
    const UiSelectableTableResult tableResult = updateOperationSettingsTableClickSelection(
        operationSettingsTable_,
        ui,
        input,
        table,
        static_cast<int>(rows.size()),
        columns.data(),
        static_cast<int>(columns.size()),
        operationSettingsTableStyle());

    const bool tableCommitted =
        tableResult.pressedColumn >= OperationSettingsColumnKeyboardMouse ||
        input.confirmPressed() ||
        input.useItemPressed();
    if (tableCommitted && !rows.empty()) {
        const int row = std::clamp(operationSettingsTable_.selectedRow, 0, static_cast<int>(rows.size()) - 1);
        const int column = std::clamp(
            operationSettingsTable_.selectedColumn,
            OperationSettingsColumnKeyboardMouse,
            OperationSettingsColumnGamepad);
        const InputAction action = rows[static_cast<std::size_t>(row)].action;
        operationSettingsPendingAction_ = action;
        operationSettingsPendingColumn_ = column;
        operationSettingsCapture_.begin(action, operationSettingsCaptureGroupForColumn(column));
        operationSettingsStatus_ = column == OperationSettingsColumnGamepad
            ? "ゲームパッド入力を押してください。Esc で中止、Backspace/Delete で削除"
            : "キーまたはマウスボタンを押してください。Esc で中止、Backspace/Delete で削除";
        ui.emitSound(UiSoundEvent::Confirm);
        ui.block(panel);
        return;
    }

    constexpr int ButtonCount = 3;
    const InputAction selectedAction = operationSettingsSelectedAction(operationSettingsTable_, operationSettingsCategory_);
    for (int i = 0; i < ButtonCount; ++i) {
        const UiRect button = optionsFooterButtonRect(i, ButtonCount);
        if (ui.pressed(button)) {
            ui.emitSound(UiSoundEvent::Confirm);
            if (i == 0) {
                clearOperationSettingsBinding(selectedAction, operationSettingsTable_.selectedColumn);
            } else if (i == 1) {
                resetOperationSettingsAction(selectedAction);
            } else {
                openUiConfirmDialog(
                    operationSettingsResetAllConfirm_,
                    "初期化の確認",
                    "すべての操作割当を初期状態に戻しますか？",
                    "初期化する",
                    "戻る",
                    1);
            }
            ui.block(panel);
            return;
        }
    }

    ui.block(panel);
}

void Game::updateAudioSettings(const Input& input, UiContext& ui)
{
    const UiRect panel = optionsPanelRect();
    audioSettingsSelection_ = std::clamp(audioSettingsSelection_, 0, AudioSettingsRowCount - 1);
    const int previousSelection = audioSettingsSelection_;

    if (input.pressed(InputAction::MoveUp)) {
        audioSettingsSelection_ = (audioSettingsSelection_ + AudioSettingsRowCount - 1) % AudioSettingsRowCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        audioSettingsSelection_ = (audioSettingsSelection_ + 1) % AudioSettingsRowCount;
    }

    const auto applyAudioRow = [&](int row, float value) {
        setAudioSettingsRowValue(optionsSettings_, row, value);
        applyOptionsSettings(std::string(audioSettingsRowLabel(row)) + " 音量 " + volumePercentText(audioSettingsRowValue(optionsSettings_, row)));
    };

    if (input.pressed(InputAction::MoveLeft)) {
        applyAudioRow(audioSettingsSelection_, audioSettingsRowValue(optionsSettings_, audioSettingsSelection_) - 0.05f);
        ui.emitSound(UiSoundEvent::Confirm);
    }
    if (input.pressed(InputAction::MoveRight)) {
        applyAudioRow(audioSettingsSelection_, audioSettingsRowValue(optionsSettings_, audioSettingsSelection_) + 0.05f);
        ui.emitSound(UiSoundEvent::Confirm);
    }

    const auto values = audioSettingsRowValueTexts(optionsSettings_);
    const auto tabItems = audioSettingsTabItems(values);
    const auto tabRects = audioSettingsRowRects();
    const int clickedRow = updateClickedVerticalTabSelection(
        ui,
        tabItems.data(),
        tabRects.data(),
        static_cast<int>(tabItems.size()));
    if (clickedRow >= 0) {
        audioSettingsSelection_ = clickedRow;
        ui.emitCursorMoveIfChanged(previousSelection, audioSettingsSelection_);
        return;
    }

    for (int row = 0; row < AudioSettingsRowCount; ++row) {
        const UiRect sliderRect = audioSettingsSliderRect(row);
        if (input.mouseLeftHeld() && sliderRect.contains(ui.mouse()) && !ui.pointerConsumed()) {
            audioSettingsSelection_ = row;
            const float value = clamp((ui.mouse().x - sliderRect.pos.x) / std::max(1.0f, sliderRect.size.x), 0.0f, 1.0f);
            applyAudioRow(row, value);
            ui.consumePointer();
            ui.emitCursorMoveIfChanged(previousSelection, audioSettingsSelection_);
            return;
        }
    }

    ui.emitCursorMoveIfChanged(previousSelection, audioSettingsSelection_);
    constexpr int ButtonCount = 1;
    for (int i = 0; i < ButtonCount; ++i) {
        if (ui.pressed(optionsFooterButtonRect(i, ButtonCount))) {
            ui.emitSound(UiSoundEvent::Confirm);
            optionsSettings_.audio = AudioSettings{};
            applyOptionsSettings("音量を初期化しました");
            ui.block(panel);
            return;
        }
    }

    ui.block(panel);
}

void Game::updateVideoSettings(const Input& input, UiContext& ui)
{
    const UiRect panel = optionsPanelRect();
    videoSettingsSelection_ = std::clamp(videoSettingsSelection_, 0, VideoSettingsRowCount - 1);
    const int previousSelection = videoSettingsSelection_;

    if (input.pressed(InputAction::MoveUp)) {
        videoSettingsSelection_ = (videoSettingsSelection_ + VideoSettingsRowCount - 1) % VideoSettingsRowCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        videoSettingsSelection_ = (videoSettingsSelection_ + 1) % VideoSettingsRowCount;
    }

    const auto applyVideoRow = [&](int row, int delta) {
        cycleVideoSetting(optionsSettings_, row, delta);
        applyOptionsSettings(std::string(videoSettingsRowLabel(row)) + " " + videoSettingsRowValueText(optionsSettings_, row));
    };
    const auto applyBrightnessSlider = [&](float normalizedValue) {
        const float value = MinScreenBrightness +
            (MaxScreenBrightness - MinScreenBrightness) * clamp(normalizedValue, 0.0f, 1.0f);
        setScreenBrightnessValue(optionsSettings_, value);
        applyOptionsSettings(std::string(videoSettingsRowLabel(4)) + " " + videoSettingsRowValueText(optionsSettings_, 4));
    };

    if (input.pressed(InputAction::MoveLeft)) {
        applyVideoRow(videoSettingsSelection_, -1);
        ui.emitSound(UiSoundEvent::Confirm);
    }
    if (input.pressed(InputAction::MoveRight) || input.confirmPressed() || input.useItemPressed()) {
        applyVideoRow(videoSettingsSelection_, 1);
        ui.emitSound(UiSoundEvent::Confirm);
    }

    const auto values = videoSettingsRowValueTexts(optionsSettings_);
    const auto tabItems = videoSettingsTabItems(values);
    const auto tabRects = videoSettingsRowRects();
    const int clickedRow = updateClickedVerticalTabSelection(
        ui,
        tabItems.data(),
        tabRects.data(),
        static_cast<int>(tabItems.size()));
    if (clickedRow >= 0) {
        videoSettingsSelection_ = clickedRow;
        if (clickedRow != 4) {
            applyVideoRow(clickedRow, 1);
            ui.emitSound(UiSoundEvent::Confirm);
        }
        ui.emitCursorMoveIfChanged(previousSelection, videoSettingsSelection_);
        return;
    }

    for (int row = 0; row < VideoSettingsRowCount; ++row) {
        if (row == 4) {
            const UiRect sliderRect = videoBrightnessSliderRect();
            if (input.mouseLeftHeld() && sliderRect.contains(ui.mouse()) && !ui.pointerConsumed()) {
                videoSettingsSelection_ = row;
                applyBrightnessSlider((ui.mouse().x - sliderRect.pos.x) / std::max(1.0f, sliderRect.size.x));
                ui.consumePointer();
                ui.emitCursorMoveIfChanged(previousSelection, videoSettingsSelection_);
                return;
            }
            continue;
        }
    }

    ui.emitCursorMoveIfChanged(previousSelection, videoSettingsSelection_);
    constexpr int ButtonCount = 1;
    for (int i = 0; i < ButtonCount; ++i) {
        if (ui.pressed(optionsFooterButtonRect(i, ButtonCount))) {
            ui.emitSound(UiSoundEvent::Confirm);
            optionsSettings_.video = VideoSettings{};
            optionsSettings_.performance = PerformanceSettings{};
            optionsSettings_.presentation = PresentationSettings{};
            applyOptionsSettings("画面設定を初期化しました");
            ui.block(panel);
            return;
        }
    }

    ui.block(panel);
}

void Game::updateOptionsMenu(const Input& input, UiContext& ui)
{
    if (!optionsSettingsLoaded_) {
        loadOptionsSettings();
    }

    const bool operationModalOpen = optionsPage_ == OptionsPageOperation &&
        (operationSettingsCapture_.active() ||
            operationSettingsConflictConfirm_.open ||
            operationSettingsResetAllConfirm_.open);
    if (!operationModalOpen) {
        int pageDelta = 0;
        if (input.pressed(InputAction::PreviousActiveRing)) {
            --pageDelta;
        }
        if (input.pressed(InputAction::NextActiveRing)) {
            ++pageDelta;
        }
        if (pageDelta != 0) {
            optionsPage_ = (optionsPage_ + pageDelta + OptionsPageCount) % OptionsPageCount;
            optionsStatus_.clear();
            ui.emitSound(UiSoundEvent::TabSwitch);
        }

        const auto tabItems = optionsPageTabItems();
        const auto tabRects = optionsPageTabRects();
        UiTabsInput tabsInput{};
        const int selectedTab = updateUiTabs(
            optionsTabs_,
            ui,
            tabsInput,
            optionsPage_,
            tabItems.data(),
            static_cast<int>(tabItems.size()),
            tabRects.data());
        if (selectedTab >= 0 && selectedTab != optionsPage_) {
            optionsPage_ = selectedTab;
            optionsStatus_.clear();
        }
    }

    if (optionsPage_ == OptionsPageAudio) {
        updateAudioSettings(input, ui);
    } else if (optionsPage_ == OptionsPageVideo) {
        updateVideoSettings(input, ui);
    } else {
        updateOperationSettings(input, ui);
    }
}

void Game::updatePauseMenu(const Input& input, UiContext& ui)
{
    const bool suppressCancelThisFrame = optionsSuppressCancelThisFrame_;
    optionsSuppressCancelThisFrame_ = false;
    const UiRect cancelPanel = pausePage_ == PauseMenuPage::QuitConfirm
        ? quitConfirmRect()
        : pausePanelForPage(pausePage_);
    const bool operationModalOpen = pausePage_ == PauseMenuPage::Options &&
        (operationSettingsCapture_.active() ||
            operationSettingsConflictConfirm_.open ||
            operationSettingsResetAllConfirm_.open);
    if (pausePage_ != PauseMenuPage::QuitConfirm &&
        !operationModalOpen &&
        !suppressCancelThisFrame &&
        uiCancelRequested(pauseCancelState_, input, ui, cancelPanel)) {
        leavePausePage();
        return;
    }

    if (pausePage_ == PauseMenuPage::QuitConfirm) {
        const UiConfirmDialogResult result = updateUiConfirmDialog(pauseQuitConfirm_, ui, input, cancelPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            quitRequested_ = true;
            return;
        }
        if (result == UiConfirmDialogResult::Cancelled) {
            leavePausePage();
            return;
        }
        ui.block(quitConfirmRect());
        return;
    }

    if (pausePage_ == PauseMenuPage::Options) {
        updateOptionsMenu(input, ui);
        return;
    }

    if (pausePage_ != PauseMenuPage::Main) {
        ui.block(pausePanelForPage(pausePage_));
        return;
    }

    const int previousSelection = pauseMenuSelection_;
    if (input.pressed(InputAction::MoveUp)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + PauseMenuItemCount - 1) % PauseMenuItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + 1) % PauseMenuItemCount;
    }
    for (int i = 0; i < PauseMenuItemCount; ++i) {
        const UiRect rect = pauseMenuItemRect(i);
        if (rect.contains(ui.mouse())) {
            pauseMenuSelection_ = i;
        }
        if (ui.pressed(rect)) {
            pauseMenuSelection_ = i;
            ui.emitSound(UiSoundEvent::Confirm);
            choosePauseMenuItem(i);
            return;
        }
    }
    ui.emitCursorMoveIfChanged(previousSelection, pauseMenuSelection_);
    if (input.confirmPressed() || input.useItemPressed()) {
        ui.emitSound(UiSoundEvent::Confirm);
        choosePauseMenuItem(pauseMenuSelection_);
        return;
    }

    ui.block(pausePanelRect());
}

void Game::updateGameOverScreen(const Input& input, UiContext& ui)
{
    const int previousSelection = gameOverSelection_;
    if (input.pressed(InputAction::MoveUp)) {
        gameOverSelection_ = (gameOverSelection_ + GameOverItemCount - 1) % GameOverItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        gameOverSelection_ = (gameOverSelection_ + 1) % GameOverItemCount;
    }

    for (int i = 0; i < GameOverItemCount; ++i) {
        const UiRect rect = gameOverItemRect(i);
        if (rect.contains(ui.mouse())) {
            gameOverSelection_ = i;
        }
        if (ui.pressed(rect)) {
            gameOverSelection_ = i;
            ui.emitSound(UiSoundEvent::Confirm);
            if (i == 0) {
                retryAfterGameOver();
            } else {
                returnToBaseAfterGameOver();
            }
            return;
        }
    }
    ui.emitCursorMoveIfChanged(previousSelection, gameOverSelection_);

    if (input.confirmPressed() || input.useItemPressed()) {
        ui.emitSound(UiSoundEvent::Confirm);
        if (gameOverSelection_ == 0) {
            retryAfterGameOver();
        } else {
            returnToBaseAfterGameOver();
        }
        return;
    }

    ui.block(gameOverPanelRect());
}

void Game::updateStageClearScreen(const Input& input, UiContext& ui)
{
    const int previousSelection = stageClearSelection_;
    if (input.pressed(InputAction::MoveUp)) {
        stageClearSelection_ = (stageClearSelection_ + StageClearItemCount - 1) % StageClearItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        stageClearSelection_ = (stageClearSelection_ + 1) % StageClearItemCount;
    }

    for (int i = 0; i < StageClearItemCount; ++i) {
        const UiRect rect = stageClearItemRect(i);
        if (rect.contains(ui.mouse())) {
            stageClearSelection_ = i;
        }
        if (ui.pressed(rect)) {
            stageClearSelection_ = i;
            ui.emitSound(UiSoundEvent::Confirm);
            requestReturnToBaseTransition(true, false);
            return;
        }
    }
    ui.emitCursorMoveIfChanged(previousSelection, stageClearSelection_);

    if (input.confirmPressed() || input.useItemPressed()) {
        ui.emitSound(UiSoundEvent::Confirm);
        requestReturnToBaseTransition(true, false);
        return;
    }

    ui.block(stageClearPanelRect());
}

void Game::updateAstralResultScreen(const Input& input, UiContext& ui)
{
    const UiRect button = stageClearItemRect(0);
    if (button.contains(ui.mouse())) {
        astralResultSelection_ = 0;
    }
    if (ui.pressed(button) || input.confirmPressed() || input.useItemPressed()) {
        ui.emitSound(UiSoundEvent::Confirm);
        returnToBaseAfterAstralResult();
        return;
    }

    ui.block(stageClearPanelRect());
}

void Game::updateFirstItemAcquisitionNotice(const Input& input, UiContext& ui)
{
    if (firstItemAcquisitionNotices_.empty()) {
        return;
    }

    AcquisitionNotice& notice = firstItemAcquisitionNotices_.front();
    const UiRect panel = firstItemAcquisitionNoticeRect(camera_.width(), camera_.height());
    const UiRect okButton = firstItemAcquisitionOkButtonRect(panel);
    const bool objectNotice = notice.kind == AcquisitionNoticeKind::Object;
    const bool instanceProtectable =
        objectNotice &&
        notice.protectable &&
        inventory_.objectInstanceProtectionEnabled(notice.instanceId).has_value();

    if (objectNotice && input.pressed(InputAction::ToggleProtection)) {
        if (instanceProtectable) {
            const bool protectedNow = inventory_.objectInstanceProtectionEnabled(notice.instanceId).value_or(false);
            if (inventory_.setObjectInstanceProtection(notice.instanceId, !protectedNow)) {
                ui.emitSound(UiSoundEvent::Confirm);
                notice.statusText.clear();
            }
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
            notice.statusText = "個体アイテムのみ保護できます";
        }
    }

    if (objectNotice && input.addRingPressed()) {
        SpellRingAddResult result{};
        std::string status;
        if (inventory_.addObjectToRing(notice.objectId, notice.instanceId, spellRing_, &result, &status)) {
            ui.emitSound(UiSoundEvent::Equip);
            const UiRect imagePanel = firstItemAcquisitionNoticeImagePanelRect(panel);
            spawnRingEquipFx(RingEquipFxRequest{
                .sourceScreen = firstItemAcquisitionNoticeImageCenter(imagePanel),
                .ringIndex = result.ringIndex,
                .itemIndex = result.itemIndex,
                .localAngle = result.localAngle,
                .objectId = result.objectId,
                .instanceId = result.instanceId,
            });
            closeFirstItemAcquisitionNotice();
            return;
        }
        ui.emitSound(UiSoundEvent::Cancel);
        notice.statusText = status.empty() ? "リングへ配置できません" : status;
    }

    if (ui.pressed(okButton) || input.confirmPressed() || input.useItemPressed()) {
        ui.emitSound(UiSoundEvent::Confirm);
        closeFirstItemAcquisitionNotice();
    }

    ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
}

int Game::unlockedRingHudCount() const
{
    return unlockedRingCount();
}

UiRect Game::ringStatusHudRect(int ringIndex, int unlockedRingCount) const
{
    const float screenHeight = static_cast<float>(camera_.height());
    const float totalHeight = static_cast<float>(unlockedRingCount) * RingStatusHudHeight +
        static_cast<float>(std::max(0, unlockedRingCount - 1)) * RingStatusHudGap;
    const float startY = std::max(
        TopInfoBarY + TopInfoBarHeight + 8.0f,
        screenHeight - RingStatusHudBottomMargin - totalHeight);
    return {{
        RingStatusHudLeftMargin,
        startY + static_cast<float>(ringIndex) * (RingStatusHudHeight + RingStatusHudGap),
    }, {RingStatusHudWidth, RingStatusHudHeight}};
}

void Game::updateRingStatusHud(UiContext& ui)
{
    if (introTutorialActive()) {
        return;
    }

    const int unlockedRingCount = unlockedRingHudCount();
    for (int ringIndex = 0; ringIndex < unlockedRingCount; ++ringIndex) {
        if (!ui.pressed(ringStatusHudRect(ringIndex, unlockedRingCount))) {
            continue;
        }
        if (ringIndex != spellRing_.activeRingIndex()) {
            ui.emitSound(UiSoundEvent::TabSwitch);
        }
        switchActiveRingWithLog(ringIndex - spellRing_.activeRingIndex());
        return;
    }
}

std::string Game::currentMapDisplayName() const
{
    if (basePresentationActive()) {
        return baseAreaName(baseArea_);
    }
    if (enemyTestActive_) {
        return "敵テスト";
    }
    if (effectTestActive_) {
        return "エフェクトテスト";
    }
    if (projectileTestActive_) {
        return "弾テスト";
    }
    if (!currentStageDefinition_.name.empty()) {
        return currentStageDefinition_.name;
    }
    return currentStageId_.empty() ? std::string{"Unknown Map"} : currentStageId_;
}

void Game::renderTopInfoBar(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float barWidth = std::max(1.0f, screenWidth - TopInfoBarX * 2.0f);
    renderer.fillRect({TopInfoBarX, TopInfoBarY}, {barWidth, TopInfoBarHeight}, {8, 10, 18, 210});
    renderer.drawRect({TopInfoBarX, TopInfoBarY}, {barWidth, TopInfoBarHeight}, {96, 104, 132, 170});

    const int textScale = 2;
    const Vec2 textMeasure = renderer.measureText("0", textScale);
    const float textY = TopInfoBarY + std::max(0.0f, (TopInfoBarHeight - textMeasure.y) * 0.5f) + 6.0f;
    InlineItemTextStyle moneyStyle;
    moneyStyle.text = {246, 230, 174, 255};
    moneyStyle.scale = textScale;
    moneyStyle.iconTextGap = TopInfoBarIconTextGap;
    moneyStyle.iconScale = TopInfoBarIconSize / std::max(1.0f, textMeasure.y);

    InlineItemTextStyle materialStyle = moneyStyle;
    materialStyle.text = {232, 236, 244, 255};

    const std::string moneyEntry = inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + std::to_string(money_) + "G";
    const Vec2 moneyEntrySize = measureInlineItemText(renderer, moneyEntry, moneyStyle);
    constexpr std::array<TopInfoMaterial, 4> Materials{{
        {MaterialType::OldWoodBuildingMaterial},
        {MaterialType::EnhancementOre},
        {MaterialType::MoonFragment},
        {MaterialType::ManaDrop},
    }};

    std::array<std::string, Materials.size()> materialEntries{};
    std::array<Vec2, Materials.size()> materialEntrySizes{};
    float rightGroupWidth = moneyEntrySize.x;
    for (std::size_t i = 0; i < Materials.size(); ++i) {
        materialEntries[i] = inlineMaterialIconTag(Materials[i].type) + std::to_string(inventory_.materialCount(Materials[i].type));
        materialEntrySizes[i] = measureInlineItemText(renderer, materialEntries[i], materialStyle);
        rightGroupWidth += TopInfoBarGroupGap + materialEntrySizes[i].x;
    }

    const float rightEdge = TopInfoBarX + barWidth - TopInfoBarPaddingX;
    float rightX = rightEdge - rightGroupWidth;
    rightX = std::max(TopInfoBarX + TopInfoBarPaddingX, rightX);

    const float mapX = TopInfoBarX + TopInfoBarPaddingX;
    const float mapAreaMaxWidth = std::max(0.0f, rightX - mapX - 18.0f);
    std::string dungeonInfoEntry;
    if (!basePresentationActive() && !enemyTestActive_) {
        const Vec2 playerTilePosition{
            static_cast<float>(tileMap_.worldToTile(player_.position.x)),
            static_cast<float>(tileMap_.worldToTile(player_.position.y)),
        };
        dungeonInfoEntry = dungeonDepthTopInfoEntry(dungeonLayout_, playerTilePosition);

        const int totalWarpPoints = std::max(0, static_cast<int>(warpPoints_.size()));
        if (warpPointsEnabled_ && totalWarpPoints > 0) {
            dungeonInfoEntry += "   " + dungeonWarpTopInfoEntry(discoveredWarpPointCount(), totalWarpPoints);
        }
    }

    InlineItemTextStyle dungeonInfoStyle = materialStyle;
    dungeonInfoStyle.text = {206, 218, 238, 255};
    dungeonInfoStyle.iconTextGap = 5.0f;
    dungeonInfoStyle.iconScale = 28.0f / std::max(1.0f, textMeasure.y);
    if (!dungeonInfoEntry.empty()) {
        dungeonInfoEntry = fittedInlineItemText(renderer, std::move(dungeonInfoEntry), mapAreaMaxWidth, dungeonInfoStyle);
    }

    const Vec2 dungeonInfoSize = measureInlineItemText(renderer, dungeonInfoEntry, dungeonInfoStyle);
    constexpr float MapDungeonInfoGap = 18.0f;
    const float reservedMapInfoGap = !dungeonInfoEntry.empty() ? MapDungeonInfoGap : 0.0f;
    const float mapMaxWidth = std::max(0.0f, mapAreaMaxWidth - reservedMapInfoGap - dungeonInfoSize.x);
    const std::string mapName = fittedSingleLineText(renderer, currentMapDisplayName(), mapMaxWidth, textScale);
    renderer.drawText({mapX, textY}, mapName, {246, 246, 252, 255}, textScale);
    if (!dungeonInfoEntry.empty()) {
        const float mapInfoGap = !mapName.empty() ? MapDungeonInfoGap : 0.0f;
        const float dungeonInfoX = mapX + renderer.measureText(mapName, textScale).x + mapInfoGap;
        drawInlineItemText(renderer, objectCatalog_, {dungeonInfoX, textY}, dungeonInfoEntry, dungeonInfoStyle);
    }

    float x = rightX;
    drawInlineItemText(renderer, objectCatalog_, {x, textY}, moneyEntry, moneyStyle);
    x += moneyEntrySize.x;

    for (std::size_t i = 0; i < Materials.size(); ++i) {
        x += TopInfoBarGroupGap;
        drawInlineItemText(renderer, objectCatalog_, {x, textY}, materialEntries[i], materialStyle);
        x += materialEntrySizes[i].x;
    }
}

void Game::renderOpeningKamishibai(Renderer& renderer) const
{
    openingRenderer_.render(renderer, openingPlayer_, camera_.width(), camera_.height(), screenShakeScale());
}

void Game::renderEndingKamishibai(Renderer& renderer) const
{
    openingRenderer_.render(renderer, endingPlayer_, camera_.width(), camera_.height(), screenShakeScale());
}

void Game::renderTitleScreen(Renderer& renderer) const
{
    openingRenderer_.renderTitleScreen(renderer, openingTitleImagePath(openingPages_), camera_.width(), camera_.height());
}

void Game::renderScreenTransitionOverlay(Renderer& renderer)
{
    if (!screenTransition_.active()) {
        renderer.destroyFrameSnapshot(screenTransitionSnapshot_);
        return;
    }

    if (screenTransition_.phase == ScreenTransitionPhase::CrossFadeCapture) {
        renderer.destroyFrameSnapshot(screenTransitionSnapshot_);
        screenTransitionSnapshot_ = renderer.captureFrameSnapshot();
        if (!screenTransition_.applied) {
            applyScreenTransitionTarget(screenTransition_.target);
            screenTransition_.applied = true;
        }
        screenTransition_.elapsed = 0.0f;
        screenTransition_.phase = ScreenTransitionPhase::CrossFading;
    }

    float alpha = 0.0f;
    switch (screenTransition_.phase) {
    case ScreenTransitionPhase::Idle:
        break;
    case ScreenTransitionPhase::CrossFadeCapture:
        break;
    case ScreenTransitionPhase::CrossFading:
        alpha = 1.0f - smoothStep01(screenTransition_.elapsed / std::max(0.001f, ScreenTransitionCrossFadeSeconds));
        if (alpha > 0.0f) {
            renderer.setScreenSpace();
            renderer.drawFrameSnapshot(
                screenTransitionSnapshot_,
                {0.0f, 0.0f},
                {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
                {255, 255, 255, alphaByte(255.0f * alpha)});
        }
        return;
    case ScreenTransitionPhase::FadingOut:
        alpha = smoothStep01(screenTransition_.elapsed / std::max(0.001f, ScreenTransitionFadeOutSeconds));
        break;
    case ScreenTransitionPhase::Hold:
        alpha = 1.0f;
        break;
    case ScreenTransitionPhase::FadingIn:
    {
        const float fadeInSeconds = screenTransition_.fadeInSeconds > 0.0f
            ? screenTransition_.fadeInSeconds
            : ScreenTransitionFadeInSeconds;
        alpha = 1.0f - smoothStep01(
            screenTransition_.elapsed / std::max(0.001f, fadeInSeconds));
        break;
    }
    }

    if (alpha <= 0.0f) {
        return;
    }

    renderer.setScreenSpace();
    const unsigned char overlayAlpha = alphaByte(255.0f * alpha);
    const Color overlayColor = screenTransition_.fadeColor == ScreenTransitionFadeColor::White
        ? Color{255, 255, 255, overlayAlpha}
        : Color{0, 0, 0, overlayAlpha};
    renderer.fillRect(
        {0.0f, 0.0f},
        {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
        overlayColor);
}

void Game::renderPlayerDamageVignette(Renderer& renderer, double totalSeconds) const
{
    if (player_.maxHp <= 0 || player_.hp <= 0) {
        return;
    }
    if (mode_ != ScreenMode::Playing &&
        mode_ != ScreenMode::Inventory &&
        mode_ != ScreenMode::PauseMenu &&
        mode_ != ScreenMode::Ring) {
        return;
    }

    const float danger = clamp(playerDamageVignetteDanger_, 0.0f, 1.0f);
    const float flash = clamp(playerDamageVignetteFlash_, 0.0f, 1.0f);
    if (danger <= 0.005f && flash <= 0.005f) {
        return;
    }

    renderer.setScreenSpace();
    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());
    const float shortSide = std::max(1.0f, std::min(screenWidth, screenHeight));
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(totalSeconds) * (3.1f + danger * 2.4f));
    const float dangerAlpha = danger * 34.0f + danger * danger * 74.0f + pulse * danger * danger * 20.0f;
    const float flashAlpha = flash * (58.0f + 20.0f * (1.0f - flash));
    const float edgeAlpha = std::clamp(dangerAlpha + flashAlpha, 0.0f, PlayerDamageVignetteMaxAlpha);
    if (edgeAlpha < PlayerDamageVignetteMinAlpha) {
        return;
    }

    const float edgeWidth = std::clamp(
        shortSide * (0.15f + danger * 0.10f + flash * 0.025f),
        PlayerDamageVignetteMinEdgeWidth,
        PlayerDamageVignetteMaxEdgeWidth);
    const Color edgeColor{188, 18, 36, alphaByte(edgeAlpha)};
    const Color innerColor{188, 18, 36, 0};

    renderer.fillGradientRect(
        {0.0f, 0.0f},
        {screenWidth, edgeWidth},
        edgeColor,
        innerColor,
        GradientDirection::TopToBottom);
    renderer.fillGradientRect(
        {0.0f, screenHeight - edgeWidth},
        {screenWidth, edgeWidth},
        innerColor,
        edgeColor,
        GradientDirection::TopToBottom);
    renderer.fillGradientRect(
        {0.0f, 0.0f},
        {edgeWidth, screenHeight},
        edgeColor,
        innerColor,
        GradientDirection::LeftToRight);
    renderer.fillGradientRect(
        {screenWidth - edgeWidth, 0.0f},
        {edgeWidth, screenHeight},
        innerColor,
        edgeColor,
        GradientDirection::LeftToRight);

    const float cornerAlpha = edgeAlpha * 0.42f;
    const Color cornerColor{215, 24, 42, alphaByte(cornerAlpha)};
    renderer.fillGradientRect(
        {0.0f, 0.0f},
        {edgeWidth, edgeWidth},
        cornerColor,
        innerColor,
        GradientDirection::TopLeftToBottomRight);
    renderer.fillGradientRect(
        {screenWidth - edgeWidth, 0.0f},
        {edgeWidth, edgeWidth},
        innerColor,
        cornerColor,
        GradientDirection::BottomLeftToTopRight);
    renderer.fillGradientRect(
        {0.0f, screenHeight - edgeWidth},
        {edgeWidth, edgeWidth},
        cornerColor,
        innerColor,
        GradientDirection::BottomLeftToTopRight);
    renderer.fillGradientRect(
        {screenWidth - edgeWidth, screenHeight - edgeWidth},
        {edgeWidth, edgeWidth},
        innerColor,
        cornerColor,
        GradientDirection::TopLeftToBottomRight);

    if (flash > 0.001f) {
        renderer.fillRect(
            {0.0f, 0.0f},
            {screenWidth, screenHeight},
            {160, 12, 24, alphaByte(10.0f * flash)});
    }
}

void Game::renderDungeonStatusHud(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());
    const UiRect panel{{
        std::max(8.0f, screenWidth - DungeonStatusHudRightMargin - DungeonStatusHudWidth),
        std::max(TopInfoBarY + TopInfoBarHeight + 8.0f, screenHeight - DungeonStatusHudBottomMargin - DungeonStatusHudHeight),
    }, {DungeonStatusHudWidth, DungeonStatusHudHeight}};

    const float imageScale = std::min(
        panel.size.x / DungeonStatusHudImageSize.x,
        panel.size.y / DungeonStatusHudImageSize.y);
    const Vec2 imageSize = DungeonStatusHudImageSize * imageScale;
    const Vec2 imageOrigin = panel.pos + (panel.size - imageSize) * 0.5f;

    char buffer[64];

    const int hpMax = std::max(1, player_.maxHp);
    const int hp = std::clamp(player_.hp, 0, hpMax);
    const float hpRatio = static_cast<float>(hp) / static_cast<float>(hpMax);
    const bool pinch = playerDamageVignetteDanger_ >= DungeonStatusHudPinchDanger;

    float expRatio = 1.0f;
    std::string expValue = "MAX";
    if (playerAtMaxLevel(player_)) {
        expRatio = 1.0f;
    } else {
        const int xpToNext = std::max(1, player_.xpToNext);
        const int xp = std::clamp(player_.xp, 0, xpToNext);
        expRatio = static_cast<float>(xp) / static_cast<float>(xpToNext);
        std::snprintf(buffer, sizeof(buffer), "%d/%d", xp, xpToNext);
        expValue = buffer;
    }

    ImageDrawOptions imageOptions;
    imageOptions.anchor = {0.0f, 0.0f};
    renderer.drawImage(
        DungeonStatusHudImagePath,
        imageOrigin,
        imageSize,
        imageOptions,
        TextureFilter::Linear);

    const ImageHandle gaugeMask = renderer.acquireImage(DungeonStatusHudGaugeMaskPath, TextureFilter::Linear);
    drawDungeonStatusHudHpGauge(
        renderer,
        gaugeMask,
        imageOrigin,
        imageScale,
        hpRatio,
        pinch ? DungeonStatusHudHpDangerTint : Color{255, 255, 255, 255});
    drawDungeonStatusHudExpGauge(renderer, gaugeMask, imageOrigin, imageScale, expRatio);

    drawCenteredDungeonStatusText(renderer, imageOrigin, imageScale, DungeonStatusHudHpLabelRect, "HP", DungeonStatusHudTextColor, 2);
    std::snprintf(buffer, sizeof(buffer), "%d/%d", hp, hpMax);
    drawCenteredDungeonStatusText(
        renderer,
        imageOrigin,
        imageScale,
        DungeonStatusHudHpValueRect,
        buffer,
        pinch ? DungeonStatusHudPinchTextColor : DungeonStatusHudTextColor,
        2);

    std::snprintf(buffer, sizeof(buffer), "%d", std::max(1, player_.level));
    drawTwoLineDungeonStatusText(renderer, imageOrigin, imageScale, DungeonStatusHudLevelTextRect, "Lv", buffer, 2, 2);
    drawTwoLineDungeonStatusText(renderer, imageOrigin, imageScale, DungeonStatusHudExpTextRect, "EXP", expValue, 2, 2);
}

void Game::renderRingStatusHud(Renderer& renderer) const
{
    renderer.setScreenSpace();

    const int unlockedRingCount = unlockedRingHudCount();

    char buffer[96];
    for (int ringIndex = 0; ringIndex < unlockedRingCount; ++ringIndex) {
        const UiRect panel = ringStatusHudRect(ringIndex, unlockedRingCount);
        const bool active = ringIndex == spellRing_.activeRingIndex();

        drawUiSubPanel(renderer, panel);
        if (active) {
            renderer.drawRect(panel.pos, panel.size, {255, 236, 158, 255});
            renderer.drawRect(panel.pos + Vec2{1.0f, 1.0f}, panel.size - Vec2{2.0f, 2.0f}, {255, 236, 158, 190});
        }

        const Vec2 content = panel.pos + Vec2{RingStatusHudPadding, RingStatusHudPadding};
        const auto& items = spellRing_.itemsForRing(ringIndex);

        std::snprintf(buffer, sizeof(buffer), "%sRing %d", active ? "> " : "", ringIndex + 1);
        renderer.drawText(content, buffer, active ? Color{255, 236, 158, 255} : ui::Text, 2);

        std::snprintf(buffer, sizeof(buffer), "アイテム数 %02d / %02d", static_cast<int>(items.size()), spellRing_.maxItemCount());
        renderer.drawText(content + Vec2{0.0f, 24.0f}, buffer, {232, 236, 244, 255}, 2);

        std::snprintf(
            buffer,
            sizeof(buffer),
            "重量 %.1f / %.1f",
            spellRing_.totalEquippedWeightForRing(ringIndex),
            spellRing_.maxEquippedWeightForRing(ringIndex));
        renderer.drawText(content + Vec2{0.0f, 48.0f}, buffer, {222, 236, 255, 255}, 2);
    }
}

void Game::renderFirstItemAcquisitionNotice(Renderer& renderer) const
{
    if (firstItemAcquisitionNotices_.empty()) {
        return;
    }

    const AcquisitionNotice& notice = firstItemAcquisitionNotices_.front();
    const ObjectDefinition* object = objectCatalog_.registry.findById(notice.objectId);
    const bool objectNotice = notice.kind == AcquisitionNoticeKind::Object;
    if (objectNotice && object == nullptr) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect screen{{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}};
    const UiRect panel = firstItemAcquisitionNoticeRect(camera_.width(), camera_.height());
    const std::optional<bool> protection =
        objectNotice && notice.protectable ? inventory_.objectInstanceProtectionEnabled(notice.instanceId) : std::nullopt;
    const bool canProtect = protection.has_value();
    const char* helpText = objectNotice
        ? (canProtect
            ? "F/Enter OK   R リングへ   P 保護ON/OFF"
            : "F/Enter OK   R リングへ   P 保護不可")
        : "F/Enter OK";

    const bool baseUiActive = mode_ == ScreenMode::Base && (
        baseStorageActive_ ||
        baseSellActive_ ||
        baseUpgradeActive_ ||
        baseProcessingUiMode_ != ProcessingUiMode::Closed ||
        baseRingWorkshopActive_ ||
        baseBookshelfActive_ ||
        baseMiningStartChoiceActive_ ||
        baseResultDialog_.open ||
        baseStorageQuantityDialog_.open ||
        baseRegenerateConfirm_.open ||
        baseBrokenRingDepartureConfirm_.open);
    const bool noticeOverlapsUi =
        baseUiActive ||
        debugItemPickerActive_ ||
        debugStoryTestActive_ ||
        dialogue_.active() ||
        mode_ == ScreenMode::PauseMenu ||
        mode_ == ScreenMode::Inventory ||
        mode_ == ScreenMode::Ring ||
        mode_ == ScreenMode::LevelUp;
    if (noticeOverlapsUi) {
        drawUiModalBackdrop(renderer, screen, {0, 0, 0, 132});
    }
    UiWindowScope window(
        renderer,
        "first_item_acquisition",
        panel,
        notice.title.empty() ? "入手した！" : notice.title,
        helpText,
        UiWindowOptions{true, false});

    const UiRect body = firstItemAcquisitionNoticeBodyRect(panel);
    constexpr float TopPanelGap = 16.0f;
    const UiRect imagePanel = firstItemAcquisitionNoticeImagePanelRect(panel);
    const UiRect detailPanel{{
        imagePanel.pos.x + imagePanel.size.x + TopPanelGap,
        body.pos.y,
    }, {
        std::max(240.0f, body.size.x - imagePanel.size.x - TopPanelGap),
        imagePanel.size.y,
    }};
    drawUiSubPanel(renderer, imagePanel);

    const Vec2 imageCenter = firstItemAcquisitionNoticeImageCenter(imagePanel);
    if (objectNotice) {
        ObjectImageDrawOptions imageOptions;
        imageOptions.allowUpscale = true;
        imageOptions.selectedOutlineEnabled = protection.value_or(false);
        if (!drawItemImage(renderer, *object, imageCenter, {100.0f, 100.0f}, imageOptions)) {
            renderer.fillCircle(imageCenter, 38.0f, inventoryUiObjectColor(*object));
            renderer.drawCircle(imageCenter, 42.0f, {255, 255, 255, 210});
        }
    } else if (notice.kind == AcquisitionNoticeKind::Material) {
        if (!drawWorldIcon(renderer, materialWorldIcon(notice.materialType), imageCenter, {92.0f, 92.0f})) {
            renderer.fillCircle(imageCenter, 38.0f, {120, 220, 255, 230});
            renderer.drawCircle(imageCenter, 42.0f, {255, 255, 255, 210});
        }
    } else {
        if (!drawWorldIcon(renderer, moneyWorldIconForAmount(notice.amount), imageCenter, {92.0f, 92.0f})) {
            renderer.fillCircle(imageCenter, 38.0f, {246, 210, 90, 230});
            renderer.drawCircle(imageCenter, 42.0f, {255, 255, 255, 210});
        }
    }

    const float detailX = detailPanel.pos.x + 4.0f;
    const float detailWidth = detailPanel.size.x - 8.0f;
    std::string rawNameText;
    std::string descriptionText;
    std::string subText;
    if (objectNotice) {
        rawNameText = object->name;
        descriptionText = object->description.empty() ? "-" : object->description;
        char buffer[192];
        std::snprintf(buffer, sizeof(buffer), "レア度%d", object->rarity);
        subText = buffer;
    } else if (notice.kind == AcquisitionNoticeKind::Material) {
        rawNameText = std::string(materialTypeDisplayName(notice.materialType)) + " x" + std::to_string(std::max(1, notice.amount));
        descriptionText = "魔女からのお礼";
        subText = "素材";
    } else {
        rawNameText = std::to_string(std::max(1, notice.amount)) + "G";
        descriptionText = "魔女からのお礼";
        subText = "お金";
    }
    const std::string nameText = fittedSingleLineText(renderer, rawNameText, detailWidth, 3);
    const Vec2 nameSize = renderer.measureText(nameText, 3);

    const std::string rarityText = fittedSingleLineText(renderer, subText, detailWidth, 2);
    const Vec2 raritySize = renderer.measureText(rarityText, 2);
    const Vec2 descriptionSize = renderer.measureWrappedText(descriptionText, detailWidth, 2);

    constexpr float NameRarityGap = 8.0f;
    constexpr float RarityDescriptionGap = 12.0f;
    const float detailBlockHeight =
        nameSize.y + NameRarityGap + raritySize.y + RarityDescriptionGap + descriptionSize.y;
    const float detailTop = imagePanel.pos.y + std::max(0.0f, (imagePanel.size.y - detailBlockHeight) * 0.5f);
    const Vec2 detail{detailX, detailTop};
    const Vec2 rarityPos{detailX, detail.y + nameSize.y + NameRarityGap};
    const Vec2 descriptionPos{detailX, rarityPos.y + raritySize.y + RarityDescriptionGap};

    renderer.drawText(detail, nameText, ui::Text, 3);
    renderer.drawText(rarityPos, rarityText, ui::TextMuted, 2);
    renderer.drawWrappedText(
        descriptionPos,
        descriptionText,
        detailWidth,
        {232, 236, 244, 255},
        2);

    const std::string useEffectText = objectNotice ? firstItemAcquisitionUseEffectSummary(*object) : std::string{};
    if (!useEffectText.empty()) {
        const float effectY = descriptionPos.y + descriptionSize.y + 10.0f;
        renderer.drawText(
            {detailX, effectY},
            fittedSingleLineText(renderer, useEffectText, detailWidth, 2),
            {232, 236, 244, 255},
            2);
    }

    const UiRect okButton = firstItemAcquisitionOkButtonRect(panel);
    if (!notice.statusText.empty()) {
        const std::string statusText = fittedSingleLineText(renderer, notice.statusText, body.size.x, 2);
        const Vec2 statusSize = renderer.measureText(statusText, 2);
        renderer.drawText(
            {
                panel.pos.x + std::max(0.0f, (panel.size.x - statusSize.x) * 0.5f),
                okButton.pos.y - 28.0f,
            },
            statusText,
            {255, 210, 160, 255},
            2);
    }
    drawUiButton(renderer, okButton, "OK", true, uiActionButtonStyle());
}

void Game::appendCaptureAbsorbRenderEntries(
    std::vector<DepthRenderEntry>& entries,
    Renderer& renderer,
    float totalSeconds) const
{
    for (const CaptureAbsorbAnimation& animation : captureAbsorbAnimations_) {
        const Vec2 drawPosition = animation.lastPosition;
        entries.push_back(DepthRenderEntry{
            drawPosition.y + 2.0f,
            [this, &renderer, &animation, drawPosition, totalSeconds]() {
                const float flySeconds = std::max(0.05f, animation.durationSeconds - animation.flyDelaySeconds);
                const float flyProgress = smooth01((animation.elapsedSeconds - animation.flyDelaySeconds) / flySeconds);
                const float whiteProgress = smooth01(animation.elapsedSeconds / std::max(0.05f, animation.flyDelaySeconds));
                const float fadeProgress = smooth01((animation.elapsedSeconds - (animation.durationSeconds - 0.16f)) / 0.16f);
                const float alphaScale = 1.0f - fadeProgress;
                if (alphaScale <= 0.0f) {
                    return;
                }

                const float pulse = 0.5f + 0.5f * std::sin(totalSeconds * 18.0f + animation.elapsedSeconds * 12.0f);
                const float pop = 1.0f + std::sin(std::min(1.0f, animation.elapsedSeconds / std::max(0.05f, animation.flyDelaySeconds)) * Pi) * 0.18f;
                const float shrink = lerp(1.0f, 0.22f, flyProgress);
                const float visualScale = pop * shrink;
                const Color glowColor = withAlpha({164, 224, 255, 255}, (96.0f + pulse * 42.0f) * alphaScale);
                renderer.fillSoftCircle(drawPosition, 36.0f * visualScale + pulse * 8.0f, glowColor);
                renderer.drawSoftRing(
                    drawPosition,
                    20.0f * visualScale + flyProgress * 10.0f,
                    std::max(2.0f, 5.0f * visualScale),
                    withAlpha({255, 248, 210, 255}, 150.0f * alphaScale));
                if (flyProgress > 0.04f && flyProgress < 0.98f) {
                    renderer.drawSoftLine(
                        drawPosition,
                        player_.position,
                        std::max(1.2f, 4.2f * (1.0f - flyProgress) * alphaScale),
                        withAlpha({204, 238, 255, 255}, 92.0f * alphaScale));
                }

                renderer.drawActorShadow(
                    drawPosition + Vec2{0.0f, 11.0f * visualScale},
                    std::max(10.0f, 54.0f * visualScale),
                    withAlpha({0, 0, 0, 82}, 72.0f * alphaScale));

                Enemy visualEnemy = animation.enemy;
                visualEnemy.position = drawPosition;
                EnemyImageDrawOptions imageOptions;
                imageOptions.tint = withAlpha({255, 255, 255, 255}, 255.0f * alphaScale);
                imageOptions.scaleMultiplier = visualScale;
                imageOptions.outlineEnabled = true;
                imageOptions.outlineColor = withAlpha({144, 224, 255, 255}, 210.0f * alphaScale);
                imageOptions.outlinePx = 2;
                imageOptions.selectedOutlineEnabled = true;
                imageOptions.selectedOutlineColor = withAlpha({255, 255, 255, 255}, 170.0f * alphaScale);
                imageOptions.selectedOutlinePx = 5;
                imageOptions.maskOverlayColor = withAlpha({255, 255, 255, 255}, 255.0f * whiteProgress * alphaScale);
                Vec2 drawSize{};
                if (!drawEnemyImage(
                        renderer,
                        visualEnemy,
                        drawPosition,
                        animation.enemy.behaviorTimer + animation.elapsedSeconds,
                        imageOptions,
                        &drawSize)) {
                    const float radius = std::max(8.0f, animation.enemy.radius * 1.35f) * visualScale;
                    renderer.fillCircle(drawPosition, radius, withAlpha({255, 255, 255, 255}, 255.0f * alphaScale));
                    renderer.drawCircle(drawPosition, radius + 3.0f, withAlpha({144, 224, 255, 255}, 210.0f * alphaScale));
                }
            },
        });
    }
}

UiRect Game::dungeonMinimapRect() const
{
    const float screenHeight = static_cast<float>(camera_.height());
    const float minimapY = TopInfoBarY + TopInfoBarHeight + DungeonMinimapYGap;
    const float minimapDiameter = std::min(DungeonMinimapDiameter, std::max(96.0f, screenHeight - minimapY - 8.0f));
    return {{DungeonMinimapX, minimapY}, {minimapDiameter, minimapDiameter}};
}

UiRect Game::dungeonMapOverlayPanelRect() const
{
    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());
    return {{
        DungeonMapOverlayMargin,
        DungeonMapOverlayMargin,
    }, {
        std::max(240.0f, screenWidth - DungeonMapOverlayMargin * 2.0f),
        std::max(180.0f, screenHeight - DungeonMapOverlayMargin * 2.0f),
    }};
}

UiRect Game::dungeonMapOverlayViewportRect() const
{
    const UiRect panel = dungeonMapOverlayPanelRect();
    return {{
        panel.pos.x + DungeonMapOverlayPadding,
        panel.pos.y + DungeonMapOverlayHeaderHeight,
    }, {
        std::max(1.0f, panel.size.x - DungeonMapOverlayPadding * 2.0f),
        std::max(1.0f, panel.size.y - DungeonMapOverlayHeaderHeight - DungeonMapOverlayFooterHeight),
    }};
}

Vec2 Game::dungeonMapOverlayMapSize(UiRect viewport) const
{
    const DungeonMinimapBounds bounds = dungeonMinimapBounds(dungeonMinimapCells_);
    if (!bounds.valid) {
        return viewport.size;
    }

    const int tileCountX = std::max(1, bounds.maxX - bounds.minX + 1);
    const int tileCountY = std::max(1, bounds.maxY - bounds.minY + 1);
    const float fitTilePx = std::min(
        viewport.size.x / static_cast<float>(tileCountX),
        viewport.size.y / static_cast<float>(tileCountY));
    const float tilePx = std::clamp(fitTilePx, DungeonMapOverlayMinTilePx, DungeonMapOverlayMaxTilePx);
    return {
        static_cast<float>(tileCountX) * tilePx,
        static_cast<float>(tileCountY) * tilePx,
    };
}

Vec2 Game::dungeonMapOverlayMaxScroll() const
{
    const UiRect viewport = dungeonMapOverlayViewportRect();
    const Vec2 mapSize = dungeonMapOverlayMapSize(viewport);
    return {
        std::max(0.0f, mapSize.x - viewport.size.x),
        std::max(0.0f, mapSize.y - viewport.size.y),
    };
}

UiRect Game::dungeonMapOverlayVerticalScrollTrackRect() const
{
    const UiRect viewport = dungeonMapOverlayViewportRect();
    return {{
        viewport.pos.x + viewport.size.x - DungeonMapOverlayScrollbarThickness - DungeonMapOverlayScrollbarInset,
        viewport.pos.y + DungeonMapOverlayScrollbarInset,
    }, {
        DungeonMapOverlayScrollbarThickness,
        std::max(1.0f, viewport.size.y - DungeonMapOverlayScrollbarInset * 2.0f),
    }};
}

UiRect Game::dungeonMapOverlayVerticalScrollThumbRect() const
{
    const UiRect viewport = dungeonMapOverlayViewportRect();
    const UiRect track = dungeonMapOverlayVerticalScrollTrackRect();
    const Vec2 mapSize = dungeonMapOverlayMapSize(viewport);
    const Vec2 maxScroll = dungeonMapOverlayMaxScroll();
    if (maxScroll.y <= 0.0f || mapSize.y <= 0.0f) {
        return {{track.pos.x, track.pos.y}, {track.size.x, track.size.y}};
    }
    const float thumbH = std::max(28.0f, track.size.y * std::min(1.0f, viewport.size.y / mapSize.y));
    const float scrollY = std::clamp(dungeonMapOverlayScroll_.y, 0.0f, maxScroll.y);
    return {{
        track.pos.x,
        track.pos.y + (track.size.y - thumbH) * (scrollY / maxScroll.y),
    }, {
        track.size.x,
        thumbH,
    }};
}

UiRect Game::dungeonMapOverlayHorizontalScrollTrackRect() const
{
    const UiRect viewport = dungeonMapOverlayViewportRect();
    return {{
        viewport.pos.x + DungeonMapOverlayScrollbarInset,
        viewport.pos.y + viewport.size.y - DungeonMapOverlayScrollbarThickness - DungeonMapOverlayScrollbarInset,
    }, {
        std::max(1.0f, viewport.size.x - DungeonMapOverlayScrollbarInset * 2.0f),
        DungeonMapOverlayScrollbarThickness,
    }};
}

UiRect Game::dungeonMapOverlayHorizontalScrollThumbRect() const
{
    const UiRect viewport = dungeonMapOverlayViewportRect();
    const UiRect track = dungeonMapOverlayHorizontalScrollTrackRect();
    const Vec2 mapSize = dungeonMapOverlayMapSize(viewport);
    const Vec2 maxScroll = dungeonMapOverlayMaxScroll();
    if (maxScroll.x <= 0.0f || mapSize.x <= 0.0f) {
        return {{track.pos.x, track.pos.y}, {track.size.x, track.size.y}};
    }
    const float thumbW = std::max(28.0f, track.size.x * std::min(1.0f, viewport.size.x / mapSize.x));
    const float scrollX = std::clamp(dungeonMapOverlayScroll_.x, 0.0f, maxScroll.x);
    return {{
        track.pos.x + (track.size.x - thumbW) * (scrollX / maxScroll.x),
        track.pos.y,
    }, {
        thumbW,
        track.size.y,
    }};
}

void Game::renderDungeonMinimap(Renderer& renderer, const std::vector<LightSource>& itemLights) const
{
    if (enemyTestActive_ || dungeonMinimapCells_.empty()) {
        return;
    }

    renderer.setScreenSpace();

    const UiRect minimapRect = dungeonMinimapRect();
    const float minimapDiameter = minimapRect.size.x;
    const float minimapRadius = minimapDiameter * 0.5f;
    const float contentRadius = std::max(32.0f, minimapRadius - DungeonMinimapEdgeInset);
    const Vec2 minimapCenter = minimapRect.pos + Vec2{minimapRadius, minimapRadius};
    const int playerTileX = tileMap_.worldToTile(player_.position.x);
    const int playerTileY = tileMap_.worldToTile(player_.position.y);
    const Vec2 playerLightCenter = witchSelfLightCenter(player_.position);
    const int viewRadiusTiles = static_cast<int>(std::ceil(contentRadius / DungeonMinimapTilePx)) + 2;
    const auto pointInMinimap = [&](Vec2 point, float margin) {
        const float radius = std::max(0.0f, contentRadius - margin);
        return lengthSquared(point - minimapCenter) <= radius * radius;
    };
    const auto tileToMini = [&](int tx, int ty) {
        return minimapCenter + Vec2{
            static_cast<float>(tx - playerTileX) * DungeonMinimapTilePx,
            static_cast<float>(ty - playerTileY) * DungeonMinimapTilePx,
        };
    };

    renderer.fillCircle(minimapCenter, contentRadius, {0, 0, 0, 255});

    const int minTileX = playerTileX - viewRadiusTiles;
    const int maxTileX = playerTileX + viewRadiusTiles;
    const int minTileY = playerTileY - viewRadiusTiles;
    const int maxTileY = playerTileY + viewRadiusTiles;
    for (int ty = minTileY; ty <= maxTileY; ++ty) {
        for (int tx = minTileX; tx <= maxTileX; ++tx) {
            const auto cellIt = dungeonMinimapCells_.find(dungeonMinimapKey(tx, ty));
            if (cellIt == dungeonMinimapCells_.end()) {
                continue;
            }
            const Vec2 cellCenter = tileToMini(tx, ty);
            if (!pointInMinimap(cellCenter, DungeonMinimapTilePx)) {
                continue;
            }
            const bool lit = tileMap_.isLit(tileMap_.tileCenter(tx, ty), playerLightCenter, itemLights);
            const Color color = dungeonMinimapTileColor(cellIt->second.type, lit);
            const Vec2 drawPos = cellCenter - Vec2{DungeonMinimapTilePx, DungeonMinimapTilePx} * 0.5f;
            renderer.fillRect(drawPos, {DungeonMinimapTilePx + 0.4f, DungeonMinimapTilePx + 0.4f}, color);
        }
    }

    const auto drawMiniCircleOnTile = [&](int tx, int ty, float radius, Color color) {
        if (!dungeonMinimapTileSeen(tx, ty)) {
            return;
        }
        const Vec2 marker = tileToMini(tx, ty);
        if (!pointInMinimap(marker, radius + 1.0f)) {
            return;
        }
        drawDungeonMapCircleMarker(renderer, marker, radius, color);
    };
    const auto drawMiniSquareOnTile = [&](int tx, int ty, float size, Color color) {
        if (!dungeonMinimapTileSeen(tx, ty)) {
            return;
        }
        const Vec2 marker = tileToMini(tx, ty);
        if (!pointInMinimap(marker, size * 0.5f + 1.0f)) {
            return;
        }
        drawDungeonMapSquareMarker(renderer, marker, size, color);
    };

    if (warpPointsEnabled_) {
        for (const WarpPoint& point : warpPoints_) {
            if (!dungeonMinimapTileSeen(point.tilePosition.x, point.tilePosition.y)) {
                continue;
            }
            const Vec2 marker = tileToMini(point.tilePosition.x, point.tilePosition.y);
            if (!pointInMinimap(marker, 7.0f)) {
                continue;
            }
            drawDungeonMapCircleMarker(
                renderer,
                marker,
                point.discovered ? 3.2f : 3.0f,
                point.discovered ? DungeonMapDiscoveredWarpColor : DungeonMapUndiscoveredWarpColor);
        }
    }
    {
        const Vec2 entrance = dungeonEntrancePosition();
        const int entranceTileX = tileMap_.worldToTile(entrance.x);
        const int entranceTileY = tileMap_.worldToTile(entrance.y);
        if (dungeonMinimapTileSeen(entranceTileX, entranceTileY)) {
            const Vec2 marker = tileToMini(entranceTileX, entranceTileY);
            if (pointInMinimap(marker, 7.0f)) {
                drawDungeonMapLadderMarker(renderer, marker, 0.68f);
            }
        }
    }

    for (const RewardNode& node : rewardNodes_) {
        if (rewardNodeVisibleOnDungeonMap(node)) {
            drawMiniCircleOnTile(node.tile.x, node.tile.y, 3.0f, DungeonMapItemColor);
        }
    }
    for (const MoneyNode& node : moneyNodes_) {
        if (moneyNodeVisibleOnDungeonMap(node)) {
            drawMiniCircleOnTile(node.tile.x, node.tile.y, 2.1f, DungeonMapItemColor);
        }
    }
    for (const MoonFragmentNode& node : moonFragmentNodes_) {
        if (moonFragmentNodeVisibleOnDungeonMap(node)) {
            drawMiniCircleOnTile(node.tile.x, node.tile.y, 2.2f, DungeonMapMaterialColor);
        }
    }
    for (const CrateNode& node : crateNodes_) {
        if (!node.destroyed) {
            drawMiniSquareOnTile(node.tile.x, node.tile.y, 4.8f, DungeonMapCrateColor);
        }
    }
    for (const ChestNode& node : chestNodes_) {
        if (chestNodeVisibleOnDungeonMap(node)) {
            drawMiniSquareOnTile(node.tile.x, node.tile.y, 5.0f, DungeonMapChestColor);
        }
    }
    for (const DungeonEventInstance& event : dungeonEvents_.all()) {
        if (!dungeonEventVisibleOnMap(event)) {
            continue;
        }
        bool drewObject = false;
        for (const DungeonEventObject& object : event.eventObjects) {
            if (object.destroyed) {
                continue;
            }
            drawMiniCircleOnTile(object.tile.x, object.tile.y, 3.0f, DungeonMapEventColor);
            drewObject = true;
        }
        for (const DungeonEventNestHole& hole : event.nestHoles) {
            if (hole.destroyed) {
                continue;
            }
            drawMiniCircleOnTile(hole.tile.x, hole.tile.y, 3.0f, DungeonMapEventColor);
            drewObject = true;
        }
        if (!drewObject) {
            drawMiniCircleOnTile(event.focusTile.x, event.focusTile.y, 3.0f, DungeonMapEventColor);
        }
    }
    for (const WorldDropItem& drop : worldDrops_.drops()) {
        const Vec2 dropPosition = dungeonMapDropPosition(drop);
        const int dropTileX = tileMap_.worldToTile(dropPosition.x);
        const int dropTileY = tileMap_.worldToTile(dropPosition.y);
        switch (drop.kind) {
        case WorldDropKind::Object:
            drawMiniCircleOnTile(dropTileX, dropTileY, 3.0f, DungeonMapItemColor);
            break;
        case WorldDropKind::Money:
            drawMiniCircleOnTile(dropTileX, dropTileY, 2.1f, DungeonMapItemColor);
            break;
        case WorldDropKind::Material:
            drawMiniCircleOnTile(dropTileX, dropTileY, 2.2f, DungeonMapMaterialColor);
            break;
        }
    }

    bool hasWarpGuideMarker = false;
    Vec2 warpGuideDirection{};
    float warpGuidePulse = 0.0f;
    if (warpPointsEnabled_) {
        for (const DungeonEventInstance& event : dungeonEvents_.all()) {
            if (event.kind != DungeonEventKind::WarpGuideMap ||
                !event.activated ||
                event.completed ||
                event.guideRemainingSeconds <= 0.0f ||
                event.guideTargetWarpPointIndex < 0 ||
                event.guideTargetWarpPointIndex >= static_cast<int>(warpPoints_.size())) {
                continue;
            }
            const WarpPoint& target = warpPoints_[static_cast<std::size_t>(event.guideTargetWarpPointIndex)];
            if (target.discovered) {
                continue;
            }
            const Vec2 toTarget = target.position - player_.position;
            if (lengthSquared(toTarget) <= 0.0001f) {
                continue;
            }
            warpGuideDirection = normalize(toTarget);
            warpGuidePulse = 0.5f + 0.5f * std::sin(std::max(0.0f, event.guideRemainingSeconds) * 7.4f);
            hasWarpGuideMarker = true;
            break;
        }
    }

    std::vector<EnemyMinimapMarker> enemyMarkers;
    enemies_.appendMinimapMarkers(enemyMarkers);
    for (const EnemyMinimapMarker& enemy : enemyMarkers) {
        const int enemyTileX = tileMap_.worldToTile(enemy.position.x);
        const int enemyTileY = tileMap_.worldToTile(enemy.position.y);
        if (!dungeonMinimapTileSeen(enemyTileX, enemyTileY)) {
            continue;
        }
        const Vec2 marker = tileToMini(enemyTileX, enemyTileY);
        if (!pointInMinimap(marker, enemy.boss ? 6.0f : 4.5f)) {
            continue;
        }
        const Color fill = enemy.boss ? DungeonMapBossEnemyColor : DungeonMapEnemyColor;
        renderer.fillCircle(marker, enemy.boss ? 4.0f : 3.0f, fill);
    }

    const Vec2 facing = lengthSquared(player_.facing) > 0.0001f ? normalize(player_.facing) : Vec2{1.0f, 0.0f};
    drawDungeonMapPlayerArrow(renderer, minimapCenter, facing, 0.72f);
    renderer.drawCircle(minimapCenter, contentRadius, {88, 108, 132, 145});
    if (hasWarpGuideMarker) {
        drawWarpGuideMinimapIcon(renderer, minimapCenter + warpGuideDirection * (contentRadius + 0.5f), warpGuideDirection, warpGuidePulse);
    }
}

void Game::renderDungeonMapOverlay(Renderer& renderer, const std::vector<LightSource>& itemLights) const
{
    if (!dungeonMapOverlayOpen_ || enemyTestActive_ || dungeonMinimapCells_.empty()) {
        return;
    }

    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());
    drawUiModalBackdrop(renderer, {{0.0f, 0.0f}, {screenWidth, screenHeight}}, {0, 0, 0, 176});

    const UiRect panel = dungeonMapOverlayPanelRect();
    UiWindowScope window(
        renderer,
        "dungeon.map_overlay",
        panel,
        "探索地図",
        "Esc 閉じる   ホイール/WASD/矢印 スクロール",
        UiWindowOptions{true, true});

    const UiRect mapRect = dungeonMapOverlayViewportRect();
    renderer.fillRect(mapRect.pos, mapRect.size, {4, 7, 12, 212});

    const DungeonMinimapBounds bounds = dungeonMinimapBounds(dungeonMinimapCells_);
    if (!bounds.valid) {
        renderer.drawText(mapRect.pos + Vec2{20.0f, 20.0f}, "地図情報がありません", {170, 178, 190, 255}, 2);
        return;
    }

    const int tileCountX = std::max(1, bounds.maxX - bounds.minX + 1);
    const int tileCountY = std::max(1, bounds.maxY - bounds.minY + 1);
    const Vec2 mapSize = dungeonMapOverlayMapSize(mapRect);
    const float tilePx = std::min(
        mapSize.x / static_cast<float>(tileCountX),
        mapSize.y / static_cast<float>(tileCountY));
    const Vec2 maxScroll = dungeonMapOverlayMaxScroll();
    const Vec2 scroll{
        std::clamp(dungeonMapOverlayScroll_.x, 0.0f, maxScroll.x),
        std::clamp(dungeonMapOverlayScroll_.y, 0.0f, maxScroll.y),
    };
    const Vec2 origin = mapRect.pos + Vec2{
        maxScroll.x <= 0.0f ? (mapRect.size.x - mapSize.x) * 0.5f : -scroll.x,
        maxScroll.y <= 0.0f ? (mapRect.size.y - mapSize.y) * 0.5f : -scroll.y,
    };
    const auto tileToMap = [&](int tx, int ty) {
        return origin + Vec2{
            (static_cast<float>(tx - bounds.minX) + 0.5f) * tilePx,
            (static_cast<float>(ty - bounds.minY) + 0.5f) * tilePx,
        };
    };

    renderer.pushClipRect(mapRect.pos, mapRect.size);
    const Vec2 playerLightCenter = witchSelfLightCenter(player_.position);
    for (int ty = bounds.minY; ty <= bounds.maxY; ++ty) {
        for (int tx = bounds.minX; tx <= bounds.maxX; ++tx) {
            const auto cellIt = dungeonMinimapCells_.find(dungeonMinimapKey(tx, ty));
            if (cellIt == dungeonMinimapCells_.end()) {
                continue;
            }
            const bool lit = tileMap_.isLit(tileMap_.tileCenter(tx, ty), playerLightCenter, itemLights);
            const Color color = dungeonMinimapTileColor(cellIt->second.type, lit);
            const Vec2 drawPos = tileToMap(tx, ty) - Vec2{tilePx, tilePx} * 0.5f;
            if (drawPos.x + tilePx < mapRect.pos.x ||
                drawPos.y + tilePx < mapRect.pos.y ||
                drawPos.x > mapRect.pos.x + mapRect.size.x ||
                drawPos.y > mapRect.pos.y + mapRect.size.y) {
                continue;
            }
            renderer.fillRect(drawPos, {tilePx + 0.35f, tilePx + 0.35f}, color);
        }
    }

    const auto drawMapCircleOnTile = [&](int tx, int ty, float radius, Color color) {
        if (!dungeonMinimapTileSeen(tx, ty)) {
            return;
        }
        drawDungeonMapCircleMarker(renderer, tileToMap(tx, ty), radius, color);
    };
    const auto drawMapSquareOnTile = [&](int tx, int ty, float size, Color color) {
        if (!dungeonMinimapTileSeen(tx, ty)) {
            return;
        }
        drawDungeonMapSquareMarker(renderer, tileToMap(tx, ty), size, color);
    };

    const float itemMarkerRadius = std::max(3.0f, tilePx * 0.88f);
    const float smallMarkerRadius = std::max(2.2f, tilePx * 0.62f);
    const float squareMarkerSize = std::max(5.2f, tilePx * 1.18f);

    if (warpPointsEnabled_) {
        for (const WarpPoint& point : warpPoints_) {
            if (!dungeonMinimapTileSeen(point.tilePosition.x, point.tilePosition.y)) {
                continue;
            }
            const Vec2 marker = tileToMap(point.tilePosition.x, point.tilePosition.y);
            drawDungeonMapCircleMarker(
                renderer,
                marker,
                std::max(3.0f, tilePx * 0.92f),
                point.discovered ? DungeonMapDiscoveredWarpColor : DungeonMapUndiscoveredWarpColor);
        }
    }
    {
        const Vec2 entrance = dungeonEntrancePosition();
        const int entranceTileX = tileMap_.worldToTile(entrance.x);
        const int entranceTileY = tileMap_.worldToTile(entrance.y);
        if (dungeonMinimapTileSeen(entranceTileX, entranceTileY)) {
            drawDungeonMapLadderMarker(
                renderer,
                tileToMap(entranceTileX, entranceTileY),
                std::clamp(tilePx / 6.0f, 0.75f, 1.65f));
        }
    }

    for (const RewardNode& node : rewardNodes_) {
        if (rewardNodeVisibleOnDungeonMap(node)) {
            drawMapCircleOnTile(node.tile.x, node.tile.y, itemMarkerRadius, DungeonMapItemColor);
        }
    }
    for (const MoneyNode& node : moneyNodes_) {
        if (moneyNodeVisibleOnDungeonMap(node)) {
            drawMapCircleOnTile(node.tile.x, node.tile.y, smallMarkerRadius, DungeonMapItemColor);
        }
    }
    for (const MoonFragmentNode& node : moonFragmentNodes_) {
        if (moonFragmentNodeVisibleOnDungeonMap(node)) {
            drawMapCircleOnTile(node.tile.x, node.tile.y, smallMarkerRadius, DungeonMapMaterialColor);
        }
    }
    for (const CrateNode& node : crateNodes_) {
        if (!node.destroyed) {
            drawMapSquareOnTile(node.tile.x, node.tile.y, squareMarkerSize, DungeonMapCrateColor);
        }
    }
    for (const ChestNode& node : chestNodes_) {
        if (chestNodeVisibleOnDungeonMap(node)) {
            drawMapSquareOnTile(node.tile.x, node.tile.y, squareMarkerSize, DungeonMapChestColor);
        }
    }
    for (const DungeonEventInstance& event : dungeonEvents_.all()) {
        if (!dungeonEventVisibleOnMap(event)) {
            continue;
        }
        bool drewObject = false;
        for (const DungeonEventObject& object : event.eventObjects) {
            if (object.destroyed) {
                continue;
            }
            drawMapCircleOnTile(object.tile.x, object.tile.y, itemMarkerRadius, DungeonMapEventColor);
            drewObject = true;
        }
        for (const DungeonEventNestHole& hole : event.nestHoles) {
            if (hole.destroyed) {
                continue;
            }
            drawMapCircleOnTile(hole.tile.x, hole.tile.y, itemMarkerRadius, DungeonMapEventColor);
            drewObject = true;
        }
        if (!drewObject) {
            drawMapCircleOnTile(event.focusTile.x, event.focusTile.y, itemMarkerRadius, DungeonMapEventColor);
        }
    }
    for (const WorldDropItem& drop : worldDrops_.drops()) {
        const Vec2 dropPosition = dungeonMapDropPosition(drop);
        const int dropTileX = tileMap_.worldToTile(dropPosition.x);
        const int dropTileY = tileMap_.worldToTile(dropPosition.y);
        switch (drop.kind) {
        case WorldDropKind::Object:
            drawMapCircleOnTile(dropTileX, dropTileY, itemMarkerRadius, DungeonMapItemColor);
            break;
        case WorldDropKind::Money:
            drawMapCircleOnTile(dropTileX, dropTileY, smallMarkerRadius, DungeonMapItemColor);
            break;
        case WorldDropKind::Material:
            drawMapCircleOnTile(dropTileX, dropTileY, smallMarkerRadius, DungeonMapMaterialColor);
            break;
        }
    }

    std::vector<EnemyMinimapMarker> enemyMarkers;
    enemies_.appendMinimapMarkers(enemyMarkers);
    for (const EnemyMinimapMarker& enemy : enemyMarkers) {
        const int enemyTileX = tileMap_.worldToTile(enemy.position.x);
        const int enemyTileY = tileMap_.worldToTile(enemy.position.y);
        if (!dungeonMinimapTileSeen(enemyTileX, enemyTileY)) {
            continue;
        }
        const Vec2 marker = tileToMap(enemyTileX, enemyTileY);
        const Color fill = enemy.boss ? DungeonMapBossEnemyColor : DungeonMapEnemyColor;
        renderer.fillCircle(marker, enemy.boss ? std::max(4.0f, tilePx * 1.25f) : std::max(3.0f, tilePx), fill);
    }

    const int playerTileX = tileMap_.worldToTile(player_.position.x);
    const int playerTileY = tileMap_.worldToTile(player_.position.y);
    const Vec2 playerMarker = tileToMap(playerTileX, playerTileY);
    const Vec2 facing = lengthSquared(player_.facing) > 0.0001f ? normalize(player_.facing) : Vec2{1.0f, 0.0f};
    drawDungeonMapPlayerArrow(renderer, playerMarker, facing, std::clamp(tilePx / 5.0f, 0.9f, 2.0f));

    if (warpPointsEnabled_) {
        for (const DungeonEventInstance& event : dungeonEvents_.all()) {
            if (event.kind != DungeonEventKind::WarpGuideMap ||
                !event.activated ||
                event.completed ||
                event.guideRemainingSeconds <= 0.0f ||
                event.guideTargetWarpPointIndex < 0 ||
                event.guideTargetWarpPointIndex >= static_cast<int>(warpPoints_.size())) {
                continue;
            }
            const WarpPoint& target = warpPoints_[static_cast<std::size_t>(event.guideTargetWarpPointIndex)];
            if (target.discovered) {
                continue;
            }
            const Vec2 toTarget = target.position - player_.position;
            if (lengthSquared(toTarget) <= 0.0001f) {
                continue;
            }
            const Vec2 direction = normalize(toTarget);
            const float pulse = 0.5f + 0.5f * std::sin(std::max(0.0f, event.guideRemainingSeconds) * 7.4f);
            drawWarpGuideMinimapIcon(renderer, playerMarker + direction * std::max(20.0f, tilePx * 5.0f), direction, pulse);
            break;
        }
    }

    renderer.popClipRect();

    if (maxScroll.y > 0.0f) {
        const UiRect track = dungeonMapOverlayVerticalScrollTrackRect();
        const UiRect thumb = dungeonMapOverlayVerticalScrollThumbRect();
        renderer.fillRect(track.pos, track.size, {0, 0, 0, 96});
        renderer.fillRect(thumb.pos, thumb.size, {154, 178, 208, 190});
    }
    if (maxScroll.x > 0.0f) {
        const UiRect track = dungeonMapOverlayHorizontalScrollTrackRect();
        const UiRect thumb = dungeonMapOverlayHorizontalScrollThumbRect();
        renderer.fillRect(track.pos, track.size, {0, 0, 0, 96});
        renderer.fillRect(thumb.pos, thumb.size, {154, 178, 208, 190});
    }
}

void Game::renderDungeonLogs(Renderer& renderer) const
{
    if (dungeonLogs_.empty()) {
        return;
    }

    renderer.setScreenSpace();

    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());
    const float topLimit = TopInfoBarY + TopInfoBarHeight + 8.0f;
    const float statusTopY = screenHeight - DungeonStatusHudBottomMargin - DungeonStatusHudHeight;
    const float maxBottomY = std::max(topLimit + DungeonLogRowHeight, statusTopY - DungeonLogStatusGap);

    int visibleCount = std::min(static_cast<int>(dungeonLogs_.size()), DungeonLogMaxVisible);
    const auto blockHeight = [](int count) {
        return static_cast<float>(count) * DungeonLogRowHeight +
            static_cast<float>(std::max(0, count - 1)) * DungeonLogGap;
    };
    while (visibleCount > 0 && blockHeight(visibleCount) > maxBottomY - topLimit) {
        --visibleCount;
    }
    if (visibleCount <= 0) {
        return;
    }

    const float totalHeight = blockHeight(visibleCount);
    const float x = std::max(8.0f, screenWidth - DungeonLogRightMargin - DungeonLogWidth);
    const float y = std::clamp(screenHeight * DungeonLogTargetYRatio, topLimit, maxBottomY - totalHeight);
    const int firstIndex = static_cast<int>(dungeonLogs_.size()) - visibleCount;

    constexpr int TextScale = 2;
    const Vec2 textMeasure = renderer.measureText("0", TextScale);
    for (int i = 0; i < visibleCount; ++i) {
        const DungeonLogEntry& entry = dungeonLogs_[static_cast<std::size_t>(firstIndex + i)];
        const float rowY = y + static_cast<float>(i) * (DungeonLogRowHeight + DungeonLogGap);
        const float fadeDenom = std::max(0.01f, entry.lifetime - DungeonLogFadeStart);
        const float fade = entry.age <= DungeonLogFadeStart
            ? 1.0f
            : std::clamp(1.0f - (entry.age - DungeonLogFadeStart) / fadeDenom, 0.0f, 1.0f);

        const unsigned char rightAlpha = static_cast<unsigned char>(std::clamp(210.0f * fade, 0.0f, 210.0f));
        renderer.fillGradientRect(
            {x, rowY},
            {DungeonLogWidth, DungeonLogRowHeight},
            {0, 0, 0, 0},
            {0, 0, 0, rightAlpha},
            GradientDirection::LeftToRight);

        std::string message = entry.message;
        if (entry.count > 0) {
            message = entry.label + " x" + std::to_string(entry.count) + entry.suffix;
        }
        const unsigned char textAlpha = static_cast<unsigned char>(std::clamp(255.0f * fade, 0.0f, 255.0f));
        InlineItemTextStyle textStyle;
        textStyle.text = {246, 246, 252, textAlpha};
        textStyle.scale = TextScale;
        textStyle.iconTextGap = 4.0f;
        textStyle.iconScale = TopInfoBarIconSize / std::max(1.0f, textMeasure.y);
        textStyle.outlineEnabled = true;
        textStyle.outline = {0, 0, 0, textAlpha};
        textStyle.outlinePx = 2;
        message = fittedInlineItemText(renderer, std::move(message), DungeonLogWidth - 26.0f, textStyle);
        const Vec2 textPos{
            x + DungeonLogWidth - 14.0f,
            rowY + std::max(0.0f, (DungeonLogRowHeight - textMeasure.y) * 0.5f) + 6.0f,
        };
        drawInlineItemTextRightAligned(renderer, objectCatalog_, textPos, message, textStyle);
    }
}

void Game::renderDungeonControlHelp(Renderer& renderer) const
{
    if (mode_ != ScreenMode::Playing ||
        gameProgressPaused() ||
        screenTransition_.active() ||
        dungeonRingIntroActive() ||
        dungeonEventUiSuppressed() ||
        levels_.isChoosing()) {
        return;
    }

    renderer.setScreenSpace();
    const float screenWidth = static_cast<float>(camera_.width());
    const float screenHeight = static_cast<float>(camera_.height());

    std::string help =
        "WASD/方向キー 移動   Q/E アイテム選択   Tab アイテム行切替   F 使用   R リングへ   右長押し 中心ずらし   C リング投げ　Esc メニュー";
    bool promptFocused = false;
    if (introTutorialActive()) {
        help = "WASD/方向キー 移動   Esc メニュー";
        if (introTutorialPhase_ == IntroTutorialPhase::FreeToExit &&
            dungeonInspectableInRange(
                introTutorialExitPosition(),
                {DungeonEntranceImageMaxWidth, DungeonEntranceImageMaxHeight})) {
            help = "出口   F/Enter 拠点へ帰還";
            promptFocused = true;
        }
    } else if (warpPointsEnabled_) {
        if (focusedWarpReturnPointIndex_ == DungeonEntranceReturnFocusIndex) {
            help = "ダンジョン入口   F/Enter 拠点へ帰還";
            promptFocused = true;
        } else if (focusedWarpReturnPointIndex_ >= 0 &&
            focusedWarpReturnPointIndex_ < static_cast<int>(warpPoints_.size())) {
            const WarpPoint& point = warpPoints_[static_cast<std::size_t>(focusedWarpReturnPointIndex_)];
            if (point.discovered) {
                help = "ワープポイント " + std::to_string(point.index + 1) + "   F/Enter 拠点へ帰還";
                promptFocused = true;
            }
        }
    }
    if (!promptFocused && currentStageIsRoguelike() && roguelikeBigHole_.active && focusedRoguelikeBigHole_ != 0) {
        help = roguelikeBigHole_.unlocked
            ? "大穴   F/Enter 調べる"
            : "大穴   星脈竜を倒すと進める";
        promptFocused = true;
    }
    if (!promptFocused) {
        const std::string facilityPrompt = roguelikeFacilityPromptText();
        if (!facilityPrompt.empty()) {
            help = facilityPrompt;
            promptFocused = true;
        }
    }
    if (!promptFocused) {
        const std::string npcPrompt = dungeonEventNpcPromptText();
        if (!npcPrompt.empty()) {
            help = npcPrompt;
        }
    }

    const bool introTutorialHelpLayout = introTutorialActive();
    const UiRect shortcutHud = introTutorialHelpLayout
        ? inventory_.shortcutHudPanelRect(camera_.width(), camera_.height())
        : UiRect{};
    InputHelpStyle helpStyle;
    helpStyle.text = {232, 232, 238, 235};
    helpStyle.outline = {0, 0, 0, 190};
    helpStyle.scale = 2;
    helpStyle.outlinePx = 4;
    helpStyle.iconHeight = 24.0f;
    helpStyle.outlineEnabled = true;
    const float maxWidth = introTutorialHelpLayout
        ? std::max(120.0f, shortcutHud.size.x - 32.0f)
        : std::max(120.0f, screenWidth - 32.0f);
    help = fittedInputHelpText(renderer, std::move(help), maxWidth, helpStyle);
    const Vec2 textSize = measureInputHelpText(renderer, help, helpStyle);
    Vec2 pos{
        (screenWidth - textSize.x) * 0.5f,
        std::max(TopInfoBarY + TopInfoBarHeight + 8.0f, screenHeight - textSize.y - 4.0f),
    };
    if (introTutorialHelpLayout) {
        pos.y = std::max(
            TopInfoBarY + TopInfoBarHeight + 8.0f,
            shortcutHud.pos.y - textSize.y - 10.0f);
    }
    drawInputHelpText(renderer, pos, help, helpStyle);
}

void Game::renderWarpReturnUi(Renderer& renderer) const
{
    if (mode_ != ScreenMode::Playing || enemyTestActive_ || !warpPointsEnabled_) {
        return;
    }

    if (warpReturnConfirm_.open) {
        renderer.setScreenSpace();
        drawUiConfirmDialog(renderer, warpReturnConfirm_, warpReturnConfirmRect(), "warp.return_confirm");
    }
}

void Game::renderWorldLoadingScreen(Renderer& renderer, float totalSeconds) const
{
    if (mode_ != ScreenMode::WorldLoading) {
        return;
    }

    renderer.setScreenSpace();
    const float screenW = static_cast<float>(camera_.width());
    const float screenH = static_cast<float>(camera_.height());

    constexpr float BarW = 280.0f;
    constexpr float TrackH = 16.0f;
    constexpr float MarginRight = 28.0f;
    constexpr float MarginBottom = 28.0f;
    const float barLeft = std::max(12.0f, screenW - MarginRight - BarW);
    const float barCenterY = std::max(20.0f, screenH - MarginBottom - TrackH * 0.5f);
    const UiRect bar{{barLeft, barCenterY - TrackH * 0.5f}, {BarW, TrackH}};
    const float progress = std::clamp(worldBuildProgress(), 0.0f, 1.0f);
    const float pulse = 0.76f + 0.24f * std::sin(totalSeconds * 5.0f);
    UiGaugeStyle loadingGaugeStyle;
    loadingGaugeStyle.fill.start = {static_cast<unsigned char>(108.0f + 48.0f * pulse), 206, 236, 230};
    loadingGaugeStyle.fill.end = {132, 230, 250, 230};
    loadingGaugeStyle.track = {12, 16, 24, 190};
    loadingGaugeStyle.trackInner = {30, 38, 52, 220};
    loadingGaugeStyle.trackOuter = {218, 228, 244, 78};
    loadingGaugeStyle.shadow = {0, 0, 0, 105};
    loadingGaugeStyle.tick = {255, 255, 255, 32};
    loadingGaugeStyle.highlight = {255, 255, 255, 118};
    loadingGaugeStyle.capGlow = {132, 230, 250, 78};
    loadingGaugeStyle.capCore = {246, 252, 255, 225};
    loadingGaugeStyle.tickCount = 8;
    loadingGaugeStyle.shimmer = {255, 255, 255, 76};
    loadingGaugeStyle.shimmerPhase =
        std::fmod(std::max(0.0f, totalSeconds) * 116.0f, BarW + loadingGaugeStyle.shimmerWidth) /
        (BarW + loadingGaugeStyle.shimmerWidth);
    drawUiGauge(renderer, bar, progress, loadingGaugeStyle);

    const std::string label = "LOADING";
    const Vec2 labelSize = renderer.measureText(label, 2);
    const Vec2 labelPos{bar.pos.x + bar.size.x - labelSize.x, bar.pos.y - labelSize.y - 9.0f};
    renderer.drawText(labelPos + Vec2{1.0f, 1.0f}, label, {0, 0, 0, 170}, 2);
    renderer.drawText(labelPos, label, {246, 246, 252, 230}, 2);
}

void Game::renderRingScreen(Renderer& renderer, float totalTime) const
{
    if (mode_ != ScreenMode::Ring) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = ringPanelRect();
    UiCancelControlScope cancelScope(ringCancelState_);
    const int ringCount = unlockedRingCount();
    const int presetSlotCount = unlockedRingPresetSlotCount();
    std::string RingHelpText;
    if (ringItemMoveModeActive_) {
        RingHelpText = "WASD/矢印 位置変更  F/Enter 確定  Esc キャンセル";
    } else {
        if (presetSlotCount > 0) {
            const std::string slotRange = presetSlotCount == 1 ? "1" : "1-" + std::to_string(presetSlotCount);
            RingHelpText = slotRange + " 呼出  Shift+" + slotRange + " 登録  ";
        }
        RingHelpText += ringCount > 1
            ? "Z/X リング  F/Enter 移動  T 整列  R 外す  Shift+R 全部取る  P 保護"
            : "WASD/矢印 選択  F/Enter 移動  T 整列  R 外す  Shift+R 全部取る  P 保護";
    }
    UiWindowScope ringWindow(renderer, "ring.manage", panel, "リング", RingHelpText, UiWindowOptions{true, true});

    char buffer[192];
    std::array<UiTabItem, SpellRingCount> ringTabs{};
    std::array<UiRect, SpellRingCount> ringTabRects{};
    std::array<std::string, SpellRingCount> ringTabLabels{};
    for (int i = 0; i < ringCount; ++i) {
        ringTabLabels[static_cast<std::size_t>(i)] = ringDisplayName(i, ringCount);
        ringTabs[static_cast<std::size_t>(i)] = {ringTabLabels[static_cast<std::size_t>(i)], true};
        ringTabRects[static_cast<std::size_t>(i)] = ringTabRect(i, ringCount);
    }
    drawUiTabs(
        renderer,
        ringTabs_,
        spellRing_.activeRingIndex(),
        ringTabs.data(),
        ringCount,
        ringTabRects.data());
    const bool actualRing = true;
    const auto& items = spellRing_.items();
    (void)actualRing;
    const RingShape activeShape = spellRing_.activeRingShape();

    renderer.drawText(panel.pos + Vec2{48.0f, 160.0f}, "リング配置", {246, 248, 255, 255}, 3);
    const int activeRingIndex = spellRing_.activeRingIndex();
    drawRingWeightLimitText(renderer, panel.pos + Vec2{50.0f, 198.0f}, spellRing_, activeRingIndex);
    const Vec2 orbitCenter = ringUiOrbitCenter(spellRing_);
    std::vector<Vec2> orbitPath = getRingPathSamplePoints(
        spellRing_.center(),
        ringUiOrbitContext(spellRing_, balance_, 0, 1),
        160);
    for (Vec2& point : orbitPath) {
        point = applyRingUiShapeRotation(spellRing_, ringWorldToUi(spellRing_, point));
    }
    drawMagicOrbitPath(
        renderer,
        orbitPath,
        orbitCenter,
        MagicOrbitDrawOptions{
            activeShape,
            true,
            false,
            true,
            true,
            spellRing_.activeRingIndex(),
            totalTime,
            1.0f,
        });
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const SpellRingItem& item = items[static_cast<std::size_t>(i)];
        float displayAngle = item.localAngle;
        if (i == ringDragItemIndex_ && (ringDragActive_ || ringSnapActive_)) {
            displayAngle = ringDragDisplayAngle_;
        }
        const Vec2 itemAnchor = ringItemUiCenterAtAngle(
            displayAngle,
            spellRing_,
            balance_,
            i,
            static_cast<int>(items.size()));
        SpellRingItem displayItem = item;
        displayItem.worldPosition = itemAnchor;
        const Vec2 itemCenter = ringItemDrawPosition(displayItem, totalTime);
        const Vec2 outward = normalize(itemAnchor - orbitCenter);
        Vec2 forward{-outward.y, outward.x};
        if (lengthSquared(forward) <= 0.0001f) {
            forward = {1.0f, 0.0f};
        }
        const bool selected = i == ringSlotSelection_;
        const bool moveMode = ringItemMoveModeActive_ && i == ringItemMoveIndex_;
        const bool invalidDragPosition = selected && ringDragActive_ && !spellRing_.canPlaceItemAtAngle(i, displayAngle);
        const ItemData* object = objectForRingItem(objectCatalog_, item);
        if (activeShape != RingShape::FigureEight) {
            const Color angleLineColor = moveMode
                ? Color{255, 142, 42, 150}
                : (selected ? Color{255, 230, 150, 120} : Color{94, 102, 128, 85});
            const Vec2 radial = itemAnchor - orbitCenter;
            Vec2 tangent = normalize(Vec2{-radial.y, radial.x});
            if (lengthSquared(tangent) <= 0.0001f) {
                tangent = {0.0f, 1.0f};
            }
            constexpr float AngleLineHalfWidthPx = 0.5f;
            renderer.drawLine(orbitCenter + tangent * AngleLineHalfWidthPx, itemAnchor + tangent * AngleLineHalfWidthPx, angleLineColor);
            renderer.drawLine(orbitCenter - tangent * AngleLineHalfWidthPx, itemAnchor - tangent * AngleLineHalfWidthPx, angleLineColor);
        }
        drawRingItemShape(renderer, item, object, itemCenter, outward, forward, totalTime, selected, invalidDragPosition, moveMode);
        std::snprintf(buffer, sizeof(buffer), "%d", i + 1);
        renderer.drawText(
            itemCenter + Vec2{-5.0f, 22.0f},
            buffer,
            moveMode ? Color{255, 170, 82, 255} : (selected ? Color{255, 230, 150, 255} : Color{174, 182, 198, 255}),
            1);
    }

    const UiRect ringDetailPanel = ringDetailRect();
    if (ringDetailShowsRing_ || ringSlotSelection_ >= static_cast<int>(items.size())) {
        drawRingDetailPanel(renderer, ringDetailPanel, spellRing_, equipmentModifiers_, balance_, spellRing_.activeRingIndex());
    } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
        const SpellRingItem& item = items[ringSlotSelection_];
        const ItemData* object = objectForRingItem(objectCatalog_, item);
        if (object != nullptr) {
            InventoryUiEntryView detailEntry{};
            detailEntry.item = object;
            detailEntry.stats = inventoryUiStatsFromRingItem(item);
            detailEntry.stackCount = 1;
            drawInventoryUiDetailPanel(
                renderer,
                ringDetailPanel,
                detailEntry,
                objectCatalog_,
                encyclopedia_,
                InventoryUiDetailOptions{
                    .animationSeconds = totalTime,
                    .unlockedRingCount = unlockedRingCount(),
                });
        } else {
            drawUiSubPanel(renderer, ringDetailPanel);
            float detailLineY = drawUiDetailHeader(renderer, ringDetailPanel, ringItemDisplayName(objectCatalog_, item));
            drawUiDetailText(renderer, ringDetailPanel, detailLineY, "-");
        }
    }

    if (ringItemMoveModeActive_ &&
        ringItemMoveIndex_ >= 0 &&
        ringItemMoveIndex_ < static_cast<int>(items.size())) {
        const std::string itemName = ringItemDisplayName(
            objectCatalog_,
            items[static_cast<std::size_t>(ringItemMoveIndex_)]);
        std::snprintf(buffer, sizeof(buffer), "移動中: %s", itemName.c_str());
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, buffer, {255, 170, 82, 255}, 2);
    } else if (!ringStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, ringStatus_, {255, 230, 150, 255}, 2);
    }

    const int commandItemIndex = ringCommandItemIndex_ >= 0 ? ringCommandItemIndex_ : ringSlotSelection_;
    const bool commandCanRemove = commandItemIndex >= 0 &&
        commandItemIndex < static_cast<int>(items.size()) &&
        !items[static_cast<std::size_t>(commandItemIndex)].objectId.empty();
    const bool commandCanPlace = ringCommandPlaceActive_ &&
        firstRingPlaceableSlot(inventory_, spellRing_, ringCommandPlaceAngle_) >= 0;
    const SpellRingItem* commandItem = !ringCommandPlaceActive_ && commandItemIndex >= 0 && commandItemIndex < static_cast<int>(items.size())
        ? &items[static_cast<std::size_t>(commandItemIndex)]
        : nullptr;
    const RingCommandMenuItems commandItems = ringCommandItems(
        ringCommandPlaceActive_,
        commandItem,
        objectCatalog_,
        commandCanPlace,
        commandCanRemove);
    drawUiCommandMenu(renderer, ringCommandMenu_, commandItems.items.data(), static_cast<int>(commandItems.items.size()));

    if (ringPlaceModeActive_) {
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        drawRingPlaceWindow(
            renderer,
            inventory_,
            objectCatalog_,
            encyclopedia_,
            spellRing_,
            ringPlaceSelection_,
            ringPlaceTargetAngle_,
            ringStatus_,
            totalTime);
    }

    if (ringDiscardConfirm_.open) {
        const SpellRingItem* confirmItem =
            ringDiscardConfirmItemIndex_ >= 0 &&
                ringDiscardConfirmItemIndex_ < static_cast<int>(items.size())
            ? &items[static_cast<std::size_t>(ringDiscardConfirmItemIndex_)]
            : nullptr;
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});
        drawRingDiscardConfirmDialog(renderer, ringDiscardConfirm_, objectCatalog_, confirmItem);
    }
}

void Game::renderRoguelikeBigHoleUi(Renderer& renderer) const
{
    if (mode_ != ScreenMode::Playing || enemyTestActive_ || !currentStageIsRoguelike()) {
        return;
    }
    if (!roguelikeBigHoleMenu_.visible) {
        return;
    }

    const std::array<UiCommandMenuItem, 2> items{{
        {"さらに進む", roguelikeBigHole_.unlocked},
        {"地上に戻る", true},
    }};
    renderer.setScreenSpace();
    drawUiCommandMenu(renderer, roguelikeBigHoleMenu_, items.data(), static_cast<int>(items.size()));
}

void Game::renderOperationSettings(Renderer& renderer) const
{
    const UiRect tableRect = operationSettingsTableRect();
    std::array<UiTabItem, OperationSettingsCategoryCount> tabItems{};
    for (int i = 0; i < OperationSettingsCategoryCount; ++i) {
        tabItems[static_cast<std::size_t>(i)] = {OperationSettingsCategoryLabels[i], true};
    }
    const auto tabRects = operationSettingsTabRects();
    drawUiSubTabs(
        renderer,
        operationSettingsTabs_,
        operationSettingsCategory_,
        tabItems.data(),
        static_cast<int>(tabItems.size()),
        tabRects.data());

    const std::vector<OperationSettingsActionRow> rows = operationSettingsRowsForCategory(operationSettingsCategory_);
    const auto columns = operationSettingsTableColumns();
    const UiSelectableTableStyle tableStyle = operationSettingsTableStyle();
    const UiSelectableTableLayout tableLayout = makeUiSelectableTableLayout(
        tableRect,
        static_cast<int>(rows.size()),
        operationSettingsTable_.scrollOffset,
        tableStyle);
    renderer.fillRect(tableLayout.header.pos, tableLayout.header.size, tableStyle.headerFill);
    for (int column = 0; column < OperationSettingsColumnCount; ++column) {
        const UiRect cell{
            {
                tableLayout.scroll.content.pos.x +
                    (column == 0 ? 0.0f :
                        columns[0].width + tableStyle.columnGap +
                            (column == 1 ? 0.0f : columns[1].width + tableStyle.columnGap)),
                tableLayout.header.pos.y,
            },
            {columns[static_cast<std::size_t>(column)].width, tableLayout.header.size.y},
        };
        const Vec2 textSize = renderer.measureText(columns[static_cast<std::size_t>(column)].label, tableStyle.headerTextScale);
        const Vec2 textPos{
            cell.pos.x + std::max(0.0f, (cell.size.x - textSize.x) * 0.5f),
            cell.pos.y + std::max(0.0f, (cell.size.y - textSize.y) * 0.5f),
        };
        renderer.drawText(textPos, columns[static_cast<std::size_t>(column)].label, tableStyle.headerText, tableStyle.headerTextScale);
    }

    renderer.pushClipRect(tableLayout.scroll.viewport.pos, tableLayout.scroll.viewport.size);
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        const UiRect rowRect = uiSelectableTableRowRect(tableLayout, row, tableStyle);
        if (!uiScrollAreaRectVisible(tableLayout.scroll, rowRect)) {
            continue;
        }
        const bool selectedRow = row == operationSettingsTable_.selectedRow;
        if (selectedRow) {
            renderer.fillRect(rowRect.pos, rowRect.size, tableStyle.rowFillHot);
        }
        for (int column = 0; column < OperationSettingsColumnCount; ++column) {
            const UiRect cell = uiSelectableTableCellRect(
                tableLayout,
                columns.data(),
                static_cast<int>(columns.size()),
                row,
                column,
                tableStyle);
            const bool selectedCell = selectedRow && column == operationSettingsTable_.selectedColumn;
            if (selectedCell) {
                renderer.fillRect(cell.pos, cell.size, Color{56, 76, 154, 190});
            }
            if (column == OperationSettingsColumnAction) {
                drawOperationSettingsCellText(
                    renderer,
                    cell,
                    rows[static_cast<std::size_t>(row)].label,
                    tableStyle.text,
                    tableStyle.cellTextScale,
                    tableStyle.cellPaddingX);
            } else {
                const std::string text = operationSettingsBindingGlyphText(
                    operationSettingsBindings_,
                    rows[static_cast<std::size_t>(row)].action,
                    column);
                const Color color = text == "未設定" ? tableStyle.disabledText : tableStyle.text;
                drawOperationSettingsBindingCell(
                    renderer,
                    cell,
                    text,
                    color,
                    tableStyle.cellPaddingX);
            }
        }
    }
    renderer.popClipRect();
    drawUiScrollAreaScrollbar(renderer, tableLayout.scroll, tableStyle.scroll);

    if (!rows.empty()) {
        const int row = std::clamp(operationSettingsTable_.selectedRow, 0, static_cast<int>(rows.size()) - 1);
        const int column = std::clamp(
            operationSettingsTable_.selectedColumn,
            OperationSettingsColumnKeyboardMouse,
            OperationSettingsColumnGamepad);
        const OperationSettingsActionRow& selectedRow = rows[static_cast<std::size_t>(row)];
        drawOptionsHelpWindow(
            renderer,
            operationSettingsHelpWindowRect(),
            operationSettingsHelpTitle(selectedRow, column),
            operationSettingsHelpDescription(selectedRow.action, column),
            operationSettingsStatus_);
    }

    constexpr int ButtonCount = 3;
    constexpr const char* ButtonLabels[ButtonCount] = {
        "削除",
        "選択中の操作を初期化",
        "操作設定を全初期化",
    };
    for (int i = 0; i < ButtonCount; ++i) {
        const bool hot = false;
        drawUiButton(
            renderer,
            optionsFooterButtonRect(i, ButtonCount),
            ButtonLabels[i],
            hot,
            uiActionButtonStyle());
    }

    if (operationSettingsCapture_.active()) {
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 120});
        const UiRect dialog = operationSettingsDialogRect();
        UiWindowScope captureWindow(
            renderer,
            "operation_settings.capture",
            dialog,
            "入力待ち",
            "Esc 中止  Backspace/Delete 削除",
            UiWindowOptions{true, true});
        const std::string line = operationSettingsPendingColumn_ == OperationSettingsColumnGamepad
            ? "ゲームパッドのボタンまたはスティック/トリガーを入力してください。"
            : "キーまたはマウスボタンを入力してください。";
        const float textMaxWidth = std::max(1.0f, dialog.size.x - 84.0f);
        const Vec2 linePos = dialog.pos + Vec2{42.0f, 104.0f};
        renderer.drawWrappedText(linePos, line, textMaxWidth, ui::Text, 2);
        const Vec2 lineSize = renderer.measureWrappedText(line, textMaxWidth, 2);
        renderer.drawText(
            linePos + Vec2{0.0f, lineSize.y + 16.0f},
            "最初に入った入力で上書きします。",
            ui::TextMuted,
            2);
    }

    if (operationSettingsConflictConfirm_.open) {
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 120});
        drawUiConfirmDialog(
            renderer,
            operationSettingsConflictConfirm_,
            operationSettingsDialogRect(),
            "operation_settings.conflict");
    } else if (operationSettingsResetAllConfirm_.open) {
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 120});
        drawUiConfirmDialog(
            renderer,
            operationSettingsResetAllConfirm_,
            operationSettingsDialogRect(),
            "operation_settings.reset_all");
    }
}

void Game::renderAudioSettings(Renderer& renderer) const
{
    const auto values = audioSettingsRowValueTexts(optionsSettings_);
    const auto tabItems = audioSettingsTabItems(values);
    const auto tabRects = audioSettingsRowRects();
    drawUiVerticalTabs(
        renderer,
        UiTabsState{},
        audioSettingsSelection_,
        tabItems.data(),
        static_cast<int>(tabItems.size()),
        tabRects.data(),
        optionsVerticalTabStyle());

    for (int row = 0; row < AudioSettingsRowCount; ++row) {
        const float value = audioSettingsRowValue(optionsSettings_, row);
        const UiRect slider = audioSettingsSliderRect(row);
        UiGaugeStyle gaugeStyle;
        gaugeStyle.tickCount = 10;
        gaugeStyle.fill.start = row == 0 ? Color{132, 230, 250, 230} : (row == 1 ? Color{160, 206, 255, 230} : Color{255, 206, 132, 230});
        gaugeStyle.fill.end = row == 0 ? Color{190, 246, 220, 230} : (row == 1 ? Color{132, 230, 250, 230} : Color{255, 230, 150, 230});
        drawUiGauge(renderer, slider, value, gaugeStyle);
    }

    drawOptionsHelpWindow(
        renderer,
        audioSettingsRowLabel(audioSettingsSelection_),
        audioSettingsHelpText(audioSettingsSelection_),
        optionsStatus_);

    constexpr int ButtonCount = 1;
    constexpr const char* ButtonLabels[ButtonCount] = {"音量設定を全初期化"};
    for (int i = 0; i < ButtonCount; ++i) {
        drawUiButton(
            renderer,
            optionsFooterButtonRect(i, ButtonCount),
            ButtonLabels[i],
            false,
            uiActionButtonStyle());
    }
}

void Game::renderVideoSettings(Renderer& renderer) const
{
    const auto values = videoSettingsRowValueTexts(optionsSettings_);
    const auto tabItems = videoSettingsTabItems(values);
    const auto tabRects = videoSettingsRowRects();
    drawUiVerticalTabs(
        renderer,
        UiTabsState{},
        videoSettingsSelection_,
        tabItems.data(),
        static_cast<int>(tabItems.size()),
        tabRects.data(),
        optionsVerticalTabStyle());

    UiGaugeStyle brightnessGaugeStyle;
    brightnessGaugeStyle.tickCount = 6;
    brightnessGaugeStyle.fill.start = Color{236, 220, 150, 230};
    brightnessGaugeStyle.fill.end = Color{255, 246, 210, 230};
    drawUiGauge(
        renderer,
        videoBrightnessSliderRect(),
        screenBrightnessGaugeValue(optionsSettings_.presentation.brightness),
        brightnessGaugeStyle);

    drawOptionsHelpWindow(
        renderer,
        videoSettingsRowLabel(videoSettingsSelection_),
        videoSettingsHelpText(videoSettingsSelection_),
        optionsStatus_);

    constexpr int ButtonCount = 1;
    constexpr const char* ButtonLabels[ButtonCount] = {"画面設定を全初期化"};
    for (int i = 0; i < ButtonCount; ++i) {
        drawUiButton(
            renderer,
            optionsFooterButtonRect(i, ButtonCount),
            ButtonLabels[i],
            false,
            uiActionButtonStyle());
    }
}

void Game::renderOptionsMenu(Renderer& renderer) const
{
    const auto tabItems = optionsPageTabItems();
    const auto tabRects = optionsPageTabRects();
    drawUiTabs(
        renderer,
        optionsTabs_,
        optionsPage_,
        tabItems.data(),
        static_cast<int>(tabItems.size()),
        tabRects.data());

    if (optionsPage_ == OptionsPageAudio) {
        renderAudioSettings(renderer);
    } else if (optionsPage_ == OptionsPageVideo) {
        renderVideoSettings(renderer);
    } else {
        renderOperationSettings(renderer);
    }
}

void Game::renderPauseMenu(Renderer& renderer) const
{
    if (mode_ != ScreenMode::PauseMenu) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = pausePanelForPage(pausePage_);
    UiCancelControlScope cancelScope(pauseCancelState_);
    const char* pauseTitle = pausePage_ == PauseMenuPage::Status
        ? "ステータス"
        : (pausePage_ == PauseMenuPage::Options ? "オプション" : "PAUSED");
    const char* pauseHelp = pausePage_ == PauseMenuPage::Status
        ? "Esc 戻る"
        : (pausePage_ == PauseMenuPage::Ring
            ? "Z/X でアクティブリング切替  Esc 戻る"
            : (pausePage_ == PauseMenuPage::Options
                ? (optionsPage_ == OptionsPageOperation
                    ? "Z/X 設定切替  Q/E 分類切替  ↑/↓ 行選択  ←/→ 列選択  F/Enter 変更  Esc 戻る"
                    : (optionsPage_ == OptionsPageAudio
                        ? "Z/X 設定切替  ↑/↓ 項目選択  ←/→ 音量変更  Esc 戻る"
                        : "Z/X 設定切替  ↑/↓ 項目選択  ←/→ 変更  F/Enter 切替  Esc 戻る"))
                : "F/Enter 決定  Esc 戻る"));
    UiWindowScope pauseWindow(
        renderer,
        "pause.main",
        panel,
        pauseTitle,
        pauseHelp,
        UiWindowOptions{true, pausePage_ != PauseMenuPage::QuitConfirm});

    char buffer[160];
    if (pausePage_ == PauseMenuPage::Main) {
        for (int i = 0; i < PauseMenuItemCount; ++i) {
            const bool selected = i == pauseMenuSelection_;
            drawUiButton(renderer, pauseMenuItemRect(i), pauseMenuItemName(i), selected);
        }
        return;
    }

    if (pausePage_ == PauseMenuPage::Status) {
        constexpr float LeftWidth = 528.0f;
        const UiRect ringPanel{{panel.pos.x + 46.0f, panel.pos.y + 334.0f}, {LeftWidth, 172.0f}};
        const Vec2 profilePos{ringPanel.pos.x, panel.pos.y + 112.0f};
        const UiRect portraitRect{{panel.pos.x + 636.0f, panel.pos.y + 36.0f}, {340.0f, 500.0f}};

        drawUiSubPanel(renderer, ringPanel);

        renderer.drawText(profilePos, "見習い魔女 ルネ", {246, 235, 255, 255}, 3);
        renderer.drawText(profilePos + Vec2{1.0f, 0.0f}, "見習い魔女 ルネ", {246, 235, 255, 255}, 3);

        const int hpMax = std::max(1, player_.maxHp);
        const int hp = std::clamp(player_.hp, 0, hpMax);
        std::snprintf(buffer, sizeof(buffer), "HP  %02d / %02d", hp, hpMax);
        renderer.drawText(profilePos + Vec2{0.0f, 42.0f}, buffer, {255, 232, 232, 255}, 2);

        UiGaugeStyle hpGaugeStyle;
        hpGaugeStyle.fill.start = {224, 74, 84, 255};
        hpGaugeStyle.fill.end = {255, 126, 116, 255};
        hpGaugeStyle.track = {42, 18, 24, 230};
        hpGaugeStyle.trackInner = {58, 24, 32, 220};
        hpGaugeStyle.trackOuter = {255, 220, 224, 82};
        hpGaugeStyle.highlight = {255, 244, 244, 92};
        hpGaugeStyle.capGlow = {255, 116, 128, 58};
        hpGaugeStyle.trackInnerInset = 4.0f;
        hpGaugeStyle.shadowOffsetY = 2.0f;
        hpGaugeStyle.shadowExtra = 5.0f;
        constexpr float StatusGaugeWidth = 228.0f;
        drawUiGauge(
            renderer,
            {profilePos + Vec2{0.0f, 68.0f}, {StatusGaugeWidth, 12.0f}},
            static_cast<float>(hp) / static_cast<float>(hpMax),
            hpGaugeStyle);

        const float expGaugeX = ringPanel.pos.x + ringPanel.size.x - StatusGaugeWidth;
        const Vec2 expLabelPos{expGaugeX, profilePos.y + 42.0f};
        std::snprintf(buffer, sizeof(buffer), "Lv.%d", std::max(1, player_.level));
        renderer.drawText(expLabelPos, buffer, {222, 236, 255, 255}, 2);
        if (playerAtMaxLevel(player_)) {
            std::snprintf(buffer, sizeof(buffer), "MAX");
        } else {
            std::snprintf(buffer, sizeof(buffer), "%d/%d", player_.xp, player_.xpToNext);
        }
        const std::string expText = std::string("EXP ") + buffer;
        const Vec2 expTextSize = renderer.measureText(expText, 2);
        renderer.drawText({expLabelPos.x + StatusGaugeWidth - expTextSize.x, expLabelPos.y}, expText, {222, 236, 255, 255}, 2);

        UiGaugeStyle expGaugeStyle;
        expGaugeStyle.fill.start = {86, 158, 255, 255};
        expGaugeStyle.fill.end = {132, 230, 250, 255};
        expGaugeStyle.track = {18, 28, 54, 230};
        expGaugeStyle.trackInner = {24, 40, 74, 220};
        expGaugeStyle.trackOuter = {206, 222, 255, 82};
        expGaugeStyle.highlight = {236, 248, 255, 92};
        expGaugeStyle.capGlow = {132, 230, 250, 58};
        expGaugeStyle.trackInnerInset = 4.0f;
        expGaugeStyle.shadowOffsetY = 2.0f;
        expGaugeStyle.shadowExtra = 5.0f;
        const int xpToNext = std::max(1, player_.xpToNext);
        const float expProgress = playerAtMaxLevel(player_)
            ? 1.0f
            : static_cast<float>(std::clamp(player_.xp, 0, xpToNext)) / static_cast<float>(xpToNext);
        drawUiGauge(
            renderer,
            {{expGaugeX, profilePos.y + 68.0f}, {StatusGaugeWidth, 12.0f}},
            expProgress,
            expGaugeStyle);

        constexpr std::array<MaterialType, 4> StatusMaterials{{
            MaterialType::OldWoodBuildingMaterial,
            MaterialType::EnhancementOre,
            MaterialType::MoonFragment,
            MaterialType::ManaDrop,
        }};
        InlineItemTextStyle materialStyle;
        materialStyle.text = {232, 236, 244, 255};
        materialStyle.scale = 2;
        materialStyle.iconScale = 1.0f;
        materialStyle.iconTextGap = 6.0f;
        InlineItemTextStyle moneyStyle = materialStyle;
        moneyStyle.text = {246, 230, 174, 255};
        drawInlineItemText(
            renderer,
            objectCatalog_,
            profilePos + Vec2{0.0f, 98.0f},
            inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + "所持金 " + std::to_string(std::max(0, money_)) + "G",
            moneyStyle);

        renderer.drawText(profilePos + Vec2{0.0f, 132.0f}, "強化素材", {246, 246, 252, 255}, 2);
        constexpr float MaterialColumnGap = 32.0f;
        constexpr float MaterialColumnWidth = (LeftWidth - MaterialColumnGap) * 0.5f;
        constexpr float MaterialValueGap = 8.0f;
        for (int i = 0; i < static_cast<int>(StatusMaterials.size()); ++i) {
            const MaterialType type = StatusMaterials[static_cast<std::size_t>(i)];
            std::string name = inlineMaterialIconTag(type);
            name += materialTypeDisplayName(type);
            const std::string count = "×" + std::to_string(inventory_.materialCount(type));
            const int column = i % 2;
            const int row = i / 2;
            const Vec2 pos{
                profilePos.x + static_cast<float>(column) * (MaterialColumnWidth + MaterialColumnGap),
                profilePos.y + 162.0f + static_cast<float>(row) * 30.0f,
            };
            const Vec2 countSize = renderer.measureText(count, materialStyle.scale);
            name = fittedInlineItemText(
                renderer,
                std::move(name),
                std::max(0.0f, MaterialColumnWidth - countSize.x - MaterialValueGap),
                materialStyle);
            drawInlineItemText(renderer, objectCatalog_, pos, name, materialStyle);
            renderer.drawText(
                {pos.x + MaterialColumnWidth - countSize.x, pos.y},
                count,
                materialStyle.text,
                materialStyle.scale);
        }

        const UiRect ringContent = uiSubPanelContentRect(ringPanel);
        renderer.drawText(ringContent.pos, "スペルリング", {246, 246, 252, 255}, 2);
        const int unlockedRings = unlockedRingCount();
        for (int ringIndex = 0; ringIndex < unlockedRings; ++ringIndex) {
            const float y = ringContent.pos.y + 34.0f + static_cast<float>(ringIndex) * 34.0f;
            renderer.drawText({ringContent.pos.x, y}, ringDisplayName(ringIndex, unlockedRings), ui::Text, 2);

            const std::vector<SpellRingItem>& items = spellRing_.itemsForRing(ringIndex);
            std::snprintf(
                buffer,
                sizeof(buffer),
                "装着 %02d/%02d",
                static_cast<int>(items.size()),
                spellRing_.maxItemCountForRing(ringIndex));
            renderer.drawText({ringContent.pos.x + 104.0f, y}, buffer, ui::TextMuted, 2);

            std::snprintf(
                buffer,
                sizeof(buffer),
                "重量 %.1f/%.1fkg",
                spellRing_.totalEquippedWeightForRing(ringIndex),
                spellRing_.maxEquippedWeightForRing(ringIndex));
            renderer.drawText({ringContent.pos.x + 284.0f, y}, buffer, ui::TextMuted, 2);
        }

        Vec2 portraitSourceSize;
        const bool portraitSizeLoaded =
            renderer.getImageSize("assets/taties/tatie_1.png", portraitSourceSize, TextureFilter::Linear) &&
            portraitSourceSize.x > 0.0f &&
            portraitSourceSize.y > 0.0f;
        constexpr float PortraitScale = 0.65f;
        const Vec2 portraitDrawSize = (portraitSizeLoaded ? portraitSourceSize : portraitRect.size) * PortraitScale;
        ImageDrawOptions portraitOptions;
        portraitOptions.anchor = {0.5f, 0.06f};
        portraitOptions.flipX = true;
        if (!renderer.drawImage(
                "assets/taties/tatie_1.png",
                {panel.pos.x + panel.size.x - 190.0f, panel.pos.y - 36.0f},
                portraitDrawSize,
                portraitOptions,
                TextureFilter::Linear)) {
            renderer.drawPlayerSprite(
                0,
                {portraitRect.pos.x + portraitRect.size.x * 0.5f, panel.pos.y + panel.size.y - 94.0f},
                180.0f,
                false,
                {255, 255, 255, 255},
                {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
        }
    } else if (pausePage_ == PauseMenuPage::Items) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "アイテム", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, "通常画面のショートカットHUDとアイテム画面で管理します。", {230, 230, 236, 255}, 2);
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, "I キーでアイテム画面を開けます。", {198, 198, 206, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Ring) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "リング", {246, 235, 255, 255}, 3);
        renderer.drawText(
            panel.pos + Vec2{58.0f, 164.0f},
            "アクティブ " + std::string(ringDisplayName(spellRing_.activeRingIndex(), unlockedRingCount())),
            {230, 230, 236, 255},
            2);
        std::snprintf(buffer, sizeof(buffer), "装着アイテム %02d/%02d", static_cast<int>(spellRing_.items().size()), spellRing_.maxItemCount());
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Options) {
        renderOptionsMenu(renderer);
    } else if (pausePage_ == PauseMenuPage::QuitConfirm) {
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});
        drawUiConfirmDialog(renderer, pauseQuitConfirm_, quitConfirmRect(), "pause.quit_confirm");
        return;
    }
}

void Game::renderGameOverScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::GameOver) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = gameOverPanelRect();
    UiWindowScope gameOverWindow(renderer, "game_over", panel, "GAME OVER", "F/Enter 決定");
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "リザルト", ui::Text, 3);

    char buffer[160];
    const std::string deathCause = playerDeathCauseText(player_);
    std::snprintf(buffer, sizeof(buffer), "死因      %s", deathCause.c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, 130.0f}, buffer, {255, 214, 220, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "到達時間  %s", formatRunTime(runStats_.elapsedSeconds).c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, 168.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "レベル    %d", player_.level);
    renderer.drawText(panel.pos + Vec2{136.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "撃破数    %d", runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 244.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "掘削数    %d", runStats_.dugTiles);
    renderer.drawText(panel.pos + Vec2{136.0f, 282.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "入手アイテム数  %d", runStats_.acquiredItems);
    renderer.drawText(panel.pos + Vec2{136.0f, 320.0f}, buffer, {230, 230, 236, 255}, 2);

    for (int i = 0; i < GameOverItemCount; ++i) {
        drawUiButton(renderer, gameOverItemRect(i), gameOverItemName(i), i == gameOverSelection_, i == 0 ? uiActionButtonStyle() : uiCancelButtonStyle());
    }
    if (!gameOverStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{152.0f, 474.0f}, gameOverStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::renderStageClearScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::StageClear) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = stageClearPanelRect();
    UiWindowScope stageClearWindow(renderer, "stage_clear", panel, "STAGE CLEAR", "F/Enter 決定");
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "クリア結果", ui::Text, 3);

    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "深部の主を退け、探索記録を拠点へ持ち帰ります。");
    renderer.drawText(panel.pos + Vec2{136.0f, 148.0f}, buffer, {230, 238, 232, 255}, 2);
    renderer.drawText(panel.pos + Vec2{136.0f, 186.0f}, "ボス後の会話は完了しました。次は拠点で状況を確認できます。", {230, 238, 232, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "次ステージ解禁: Stage %d", unlockedStages_);
    renderer.drawText(panel.pos + Vec2{136.0f, 238.0f}, buffer, {255, 230, 150, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "クリア時間 %s   掘削数 %d   撃破数 %d",
        formatRunTime(runStats_.elapsedSeconds).c_str(),
        runStats_.dugTiles,
        runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 292.0f}, buffer, {198, 208, 202, 255}, 2);

    for (int i = 0; i < StageClearItemCount; ++i) {
        drawUiButton(renderer, stageClearItemRect(i), stageClearItemName(i), i == stageClearSelection_, uiActionButtonStyle());
    }
    if (!stageClearStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{152.0f, 474.0f}, stageClearStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::renderAstralResultScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::AstralResult) {
        return;
    }

    std::string resultText = "記録なし";
    switch (astralResult_.result) {
    case AstralRunResult::Returned:
        resultText = "帰還成功";
        break;
    case AstralRunResult::Died:
        resultText = astralResult_.deathCauseText.empty() ? "死亡" : astralResult_.deathCauseText;
        break;
    case AstralRunResult::DragonDefeated:
        resultText = "星脈竜撃破";
        break;
    case AstralRunResult::Completed:
        resultText = "10000m到達";
        break;
    case AstralRunResult::None:
        break;
    }

    renderer.setScreenSpace();
    const UiRect panel = stageClearPanelRect();
    UiWindowScope astralWindow(renderer, "astral_result", panel, "ASTRAL RECORD", "F/Enter 決定");
    renderer.drawText(panel.pos + Vec2{118.0f, 82.0f}, "星間記録", ui::Text, 3);

    char buffer[192];
    float y = panel.pos.y + 128.0f;
    const float lineStep = 34.0f;
    std::snprintf(buffer, sizeof(buffer), "結果      %s", resultText.c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, y}, buffer, {255, 230, 150, 255}, 2);
    y += lineStep;
    std::snprintf(buffer, sizeof(buffer), "到達深度  深度 %d/%d   到達距離 %d",
        astralResult_.reachedDepth,
        astralResult_.maxDepth,
        astralResult_.reachedDistanceTiles);
    renderer.drawText(panel.pos + Vec2{136.0f, y}, buffer, {230, 238, 232, 255}, 2);
    y += lineStep;
    std::snprintf(buffer, sizeof(buffer), "撃破数    %d", astralResult_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, y}, buffer, {230, 238, 232, 255}, 2);
    y += lineStep;
    std::snprintf(buffer, sizeof(buffer), "掘削数    %d", astralResult_.dugTiles);
    renderer.drawText(panel.pos + Vec2{136.0f, y}, buffer, {230, 238, 232, 255}, 2);
    y += lineStep;
    std::snprintf(buffer, sizeof(buffer), "入手      アイテム %d   素材 %d   お金 %dG",
        astralResult_.acquiredItems,
        astralResult_.acquiredMaterials,
        astralResult_.acquiredMoney);
    renderer.drawText(panel.pos + Vec2{136.0f, y}, buffer, {230, 238, 232, 255}, 2);
    y += lineStep;
    renderer.drawText(
        panel.pos + Vec2{136.0f, y},
        astralResult_.carriedOut ? "持ち帰り  すべて持ち帰り" : "持ち帰り  ラン取得品は星間に消失",
        astralResult_.carriedOut ? Color{210, 238, 210, 255} : Color{255, 214, 220, 255},
        2);
    y += lineStep;
    if (astralResult_.carriedOut) {
        std::snprintf(buffer, sizeof(buffer), "スコア    %d   ハイスコア %d%s",
            astralResult_.score,
            astralResult_.highScore,
            astralResult_.highScoreUpdated ? " 更新" : "");
    } else {
        std::snprintf(buffer, sizeof(buffer), "スコア    -   ハイスコア %d", astralResult_.highScore);
    }
    renderer.drawText(panel.pos + Vec2{136.0f, y}, buffer, {255, 230, 150, 255}, 2);

    drawUiButton(renderer, stageClearItemRect(0), "拠点へ戻る", astralResultSelection_ == 0, uiActionButtonStyle());
}

void Game::renderBossDefeatPresentation(Renderer& renderer) const
{
    if (bossEncounter_.phase != BossEncounterPhase::DefeatPresentation) {
        return;
    }

    const float progress = bossDefeatPresentationProgress();
    const float reveal = smoothStep01(std::min(progress / 0.35f, 1.0f));
    const float fade = progress > 0.78f ? 1.0f - smoothStep01((progress - 0.78f) / 0.22f) : 1.0f;
    const float alpha = reveal * fade;
    if (alpha <= 0.001f) {
        return;
    }

    renderer.setScreenSpace();
    const float width = static_cast<float>(camera_.width());
    const float height = static_cast<float>(camera_.height());
    renderer.fillRect({0.0f, 0.0f}, {width, height}, {0, 0, 0, alphaByte(118.0f * alpha)});

    const Vec2 center{width * 0.5f, height * 0.43f};
    const float ringPulse = 1.0f + std::sin(progress * Pi * 3.0f) * 0.08f;
    renderer.drawCircle(center, 92.0f * ringPulse, {255, 220, 118, alphaByte(170.0f * alpha)});
    renderer.drawCircle(center, 122.0f * ringPulse, {255, 156, 94, alphaByte(88.0f * alpha)});
    renderer.drawText(center + Vec2{-174.0f, -20.0f}, "BOSS DEFEATED", {255, 235, 150, alphaByte(255.0f * alpha)}, 4);
    renderer.drawText(center + Vec2{-110.0f, 42.0f}, "深部の主を退けた", {238, 242, 236, alphaByte(230.0f * alpha)}, 2);
}

void Game::renderSpellRingForeground(
    Renderer& renderer,
    const std::vector<const SpellRingItem*>& runtimeItems,
    const std::vector<LightSource>&,
    float totalSeconds) const
{
    if (playerDeathSequenceActive()) {
        return;
    }

    if (liveSpellRingHiddenForDeath() || liveSpellRingHiddenForBossEncounter()) {
        return;
    }

    if (dungeonRingIntroActive()) {
        const float introProgress = dungeonRingIntroProgress();
        drawDungeonRingIntroOrbit(renderer, spellRing_, balance_, unlockedRingCount(), introProgress, totalSeconds);
        const std::vector<RingItemRenderRef> sortedItems = sortedRingItemRenderRefs(runtimeItems);
        for (const RingItemRenderRef& itemRef : sortedItems) {
            drawDungeonRingIntroItem(
                renderer,
                spellRing_,
                objectCatalog_,
                *itemRef.item,
                itemRef.sequenceIndex,
                introProgress,
                totalSeconds);
        }
        return;
    }

    (void)runtimeItems;
    (void)totalSeconds;
}

void Game::renderPlayerDeathRingPresentation(Renderer& renderer, float totalSeconds) const
{
    for (RingShape shapePass : MagicRingShapeRenderOrder) {
        for (const PlayerDeathRingPresentation& presentation : playerDeathSequence_.ringPresentations) {
            if (!presentation.active || presentation.shape != shapePass) {
                continue;
            }

            bool hasVisibleItems = false;
            for (const PlayerDeathRingItemPresentation& itemPresentation : presentation.items) {
                if (!itemPresentation.dropped) {
                    hasVisibleItems = true;
                    break;
                }
            }
            if (!hasVisibleItems) {
                continue;
            }

            RingOrbitContext context;
            context.shape = presentation.shape;
            context.radius = std::max(1.0f, presentation.orbitRadius);
            context.shapeRotation = presentation.shape == RingShape::FigureEight
                ? presentation.shapeRotation
                : (presentation.shape == RingShape::Comet ? presentation.pathPhase : 0.0f);
            context.tuning = makeRingOrbitTuning(balance_);
            const std::vector<Vec2> orbitPath = getRingPathSamplePoints(presentation.center, context, 160);
            drawMagicOrbitPath(
                renderer,
                orbitPath,
                presentation.center,
                MagicOrbitDrawOptions{
                    presentation.shape,
                    presentation.ringIndex == spellRing_.activeRingIndex(),
                    false,
                    false,
                    false,
                    presentation.ringIndex,
                    totalSeconds,
                    0.46f,
                });
        }
    }
}

namespace {

bool playerDeathRingPresentationHasVisibleItems(const PlayerDeathRingPresentation& presentation)
{
    for (const PlayerDeathRingItemPresentation& itemPresentation : presentation.items) {
        if (!itemPresentation.dropped) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace {

constexpr int LightweightDungeonExtraLightLimit = 8;
constexpr int DungeonLightPriorityRing = 20;
constexpr int DungeonLightPriorityDrop = 18;
constexpr int DungeonLightPriorityRingAura = 24;
constexpr int DungeonLightPriorityEntrance = 26;
constexpr int DungeonLightPriorityMagic = 32;
constexpr int DungeonLightPriorityWarp = 38;
constexpr int DungeonLightPriorityEvent = 40;
constexpr int DungeonLightPriorityTutorial = 42;
constexpr int DungeonLightPriorityBoss = 44;

struct DungeonLightCandidate {
    LightSource light;
    int priority = 0;
    int order = 0;
    bool preserveInLightweight = false;
};

std::vector<LightSource> finalizeDungeonLightSources(
    std::vector<DungeonLightCandidate> candidates,
    Vec2 focus,
    bool lightweight)
{
    if (lightweight && static_cast<int>(candidates.size()) > LightweightDungeonExtraLightLimit) {
        const auto score = [focus](const DungeonLightCandidate& candidate) {
            const Vec2 delta = candidate.light.position - focus;
            const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            return static_cast<float>(candidate.priority) * 1000.0f +
                candidate.light.radius * 3.0f -
                distance * 0.45f;
        };
        const auto byScore = [&](const DungeonLightCandidate& a, const DungeonLightCandidate& b) {
            const float scoreA = score(a);
            const float scoreB = score(b);
            if (std::abs(scoreA - scoreB) > 0.001f) {
                return scoreA > scoreB;
            }
            return a.order < b.order;
        };

        std::vector<DungeonLightCandidate> preserved;
        std::vector<DungeonLightCandidate> optional;
        preserved.reserve(candidates.size());
        optional.reserve(candidates.size());
        for (const DungeonLightCandidate& candidate : candidates) {
            if (candidate.preserveInLightweight) {
                preserved.push_back(candidate);
            } else {
                optional.push_back(candidate);
            }
        }

        const int optionalLimit = std::max(0, LightweightDungeonExtraLightLimit - static_cast<int>(preserved.size()));
        if (static_cast<int>(optional.size()) > optionalLimit) {
            std::stable_sort(optional.begin(), optional.end(), byScore);
            optional.resize(static_cast<std::size_t>(optionalLimit));
        }

        candidates = std::move(preserved);
        candidates.insert(candidates.end(), optional.begin(), optional.end());
        std::sort(candidates.begin(), candidates.end(), [](const DungeonLightCandidate& a, const DungeonLightCandidate& b) {
            return a.order < b.order;
        });
    }

    std::vector<LightSource> lights;
    lights.reserve(candidates.size());
    for (const DungeonLightCandidate& candidate : candidates) {
        lights.push_back(candidate.light);
    }
    return lights;
}

} // namespace

std::vector<LightSource> Game::collectDungeonLightSources(double totalSeconds) const
{
    const bool liveRingHidden = liveSpellRingHiddenForDeath() || liveSpellRingHiddenForBossEncounter();
    const std::vector<const SpellRingItem*> runtimeItems = liveRingHidden
        ? std::vector<const SpellRingItem*>{}
        : visibleRuntimeRingItems(spellRing_, unlockedRingCount());
    const bool ringIntroActive = dungeonRingIntroActive();
    const bool miningStartTransitionInDungeon =
        mode_ == ScreenMode::Playing &&
        screenTransition_.active() &&
        screenTransition_.target == ScreenTransitionTarget::MiningStart;
    const float ringIntroProgress = dungeonRingIntroProgress();

    std::vector<DungeonLightCandidate> lights;
    int lightOrder = 0;
    const auto addLight = [&](LightSource light, int priority, bool preserveInLightweight = false) {
        lights.push_back({light, priority, lightOrder++, preserveInLightweight});
    };

    int runtimeItemIndex = 0;
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        const float introLightScale = ringIntroActive
            ? dungeonRingIntroItemReveal(ringIntroProgress, runtimeItemIndex, itemPtr->ringIndex)
            : (miningStartTransitionInDungeon ? 0.0f : 1.0f);
        if (introLightScale <= 0.001f) {
            ++runtimeItemIndex;
            continue;
        }
        const int ringIndex = std::clamp(itemPtr->ringIndex, 0, SpellRingCount - 1);
        const Vec2 lightPosition = ringIntroActive
            ? dungeonRingIntroItemGroundPosition(spellRing_, *itemPtr, ringIntroProgress, runtimeItemIndex, ringIndex)
            : itemPtr->worldPosition;
        const float phase = itemPtr->localAngle * 1.7f + static_cast<float>(itemPtr->ringIndex) * 2.3f;
        if (itemPtr->lightRadius > 0.0f) {
            addLight({
                flickeredLightPosition(lightPosition, static_cast<float>(totalSeconds), phase),
                flickeredLightRadius(itemPtr->lightRadius, static_cast<float>(totalSeconds), phase) * introLightScale,
            }, DungeonLightPriorityRing, true);
        } else if (itemPtr->type == SpellRingItemType::Torch) {
            const float torchPhase = phase + 0.47f;
            addLight({
                flickeredLightPosition(lightPosition, static_cast<float>(totalSeconds), torchPhase),
                flickeredLightRadius(balance_.lightRadius, static_cast<float>(totalSeconds), torchPhase) * introLightScale,
            }, DungeonLightPriorityRing, true);
        }
        if (itemPtr->magicAuraTimer > 0.0f && !itemPtr->magicAuraDamageType.empty()) {
            const float auraPhase = phase + 0.83f;
            addLight({
                flickeredLightPosition(lightPosition, static_cast<float>(totalSeconds), auraPhase),
                flickeredLightRadius(
                    magicAuraLightRadius(itemPtr->magicAuraDamageType, itemPtr->hitRadius),
                    static_cast<float>(totalSeconds),
                    auraPhase) * introLightScale,
            }, DungeonLightPriorityRingAura, true);
        }
        ++runtimeItemIndex;
    }
    if (playerDeathSequenceActive()) {
        for (const PlayerDeathRingPresentation& presentation : playerDeathSequence_.ringPresentations) {
            if (!presentation.active) {
                continue;
            }
            for (const PlayerDeathRingItemPresentation& itemPresentation : presentation.items) {
                if (itemPresentation.dropped) {
                    continue;
                }
                const SpellRingItem& item = itemPresentation.item;
                const float phase = item.localAngle * 1.7f + static_cast<float>(item.ringIndex) * 2.3f;
                if (item.lightRadius > 0.0f) {
                    addLight({
                        flickeredLightPosition(item.worldPosition, static_cast<float>(totalSeconds), phase),
                        flickeredLightRadius(item.lightRadius, static_cast<float>(totalSeconds), phase),
                    }, DungeonLightPriorityRing, true);
                } else if (item.type == SpellRingItemType::Torch) {
                    const float torchPhase = phase + 0.47f;
                    addLight({
                        flickeredLightPosition(item.worldPosition, static_cast<float>(totalSeconds), torchPhase),
                        flickeredLightRadius(balance_.lightRadius, static_cast<float>(totalSeconds), torchPhase),
                    }, DungeonLightPriorityRing, true);
                }
            }
        }
    }
    std::vector<LightSource> dropLights;
    worldDrops_.appendLightSources(dropLights, objectCatalog_, static_cast<float>(totalSeconds));
    for (const LightSource& light : dropLights) {
        addLight(light, DungeonLightPriorityDrop);
    }
    if (warpPointsEnabled_) {
        for (const WarpPoint& point : warpPoints_) {
            float radiusTiles = point.discovered ? point.discoveredLightRadiusTiles : point.undiscoveredLightRadiusTiles;
            if (point.lightRevealAnimating && point.lightRevealDuration > 0.0f) {
                const float t = std::clamp(point.lightRevealTimer / point.lightRevealDuration, 0.0f, 1.0f);
                const float inv = 1.0f - t;
                const float eased = 1.0f - inv * inv * inv;
                radiusTiles = point.undiscoveredLightRadiusTiles +
                    (point.discoveredLightRadiusTiles - point.undiscoveredLightRadiusTiles) * eased;
            }
            const float radiusPx = radiusTiles * static_cast<float>(balance::TileSize);
            const float phase = static_cast<float>(point.index) * 1.73f;
            addLight({
                flickeredLightPosition(point.position, static_cast<float>(totalSeconds), phase),
                flickeredLightRadius(radiusPx, static_cast<float>(totalSeconds), phase),
            }, DungeonLightPriorityWarp);
        }
        if (hasBossSpawnPoint_ && !bossSpawned_ && !hasCapturedBossForCurrentStage()) {
            addLight({
                flickeredLightPosition(bossSpawnPoint_, static_cast<float>(totalSeconds), 4.8f),
                flickeredLightRadius(120.0f, static_cast<float>(totalSeconds), 4.8f),
            }, DungeonLightPriorityBoss);
        }
    }
    if (mode_ == ScreenMode::Playing && !enemyTestActive_) {
        const float entranceLightRadius = DungeonEntranceLightRadiusTiles * static_cast<float>(balance::TileSize);
        addLight({
            flickeredLightPosition(dungeonEntrancePosition(), static_cast<float>(totalSeconds), 2.9f),
            flickeredLightRadius(entranceLightRadius, static_cast<float>(totalSeconds), 2.9f),
        }, DungeonLightPriorityEntrance);
    }
    const std::vector<LightSource> introLights = introTutorialLightSources(totalSeconds);
    for (const LightSource& light : introLights) {
        addLight(light, DungeonLightPriorityTutorial);
    }
    std::vector<LightSource> eventLights;
    dungeonEvents_.appendLightSources(eventLights, totalSeconds);
    for (const LightSource& light : eventLights) {
        addLight(light, DungeonLightPriorityEvent);
    }
    const auto facilityLightPhaseIndex = [](RoguelikeFacilityKind kind) {
        switch (kind) {
        case RoguelikeFacilityKind::Merchant: return 0;
        case RoguelikeFacilityKind::Artisan: return 1;
        case RoguelikeFacilityKind::Trainer: return 2;
        }
        return 0;
    };
    for (const RoguelikeFacilityInstance& facility : roguelikeFacilities_) {
        if (hiddenRouteNpcAttackActive()) {
            continue;
        }
        const float phase = static_cast<float>(facilityLightPhaseIndex(facility.kind)) * 1.37f +
            static_cast<float>(facility.depthMeters) * 0.001f;
        addLight({
            flickeredLightPosition(facility.centerPosition, static_cast<float>(totalSeconds), phase),
            flickeredLightRadius(
                facility.lightRadiusTiles * static_cast<float>(balance::TileSize),
                static_cast<float>(totalSeconds),
                phase),
        }, DungeonLightPriorityEvent);
    }
    std::vector<LightSource> magicLights;
    magic_.appendLightSources(magicLights);
    for (const LightSource& light : magicLights) {
        addLight(light, DungeonLightPriorityMagic);
    }
    const float lightMultiplier = astralLightRadiusMultiplier();
    if (std::abs(lightMultiplier - 1.0f) > 0.001f) {
        for (DungeonLightCandidate& candidate : lights) {
            candidate.light.radius *= lightMultiplier;
        }
    }
    return finalizeDungeonLightSources(std::move(lights), witchSelfLightCenter(player_.position), lightweightModeEnabled());
}

void Game::renderLevelUpOverlay(Renderer& renderer)
{
    if (levelUpPresentation_.active) {
        return;
    }

    upgrades_.render(renderer, levels_, spellRing_, levelRingUpgradePoints_, balance_, unlockedRingCount());
    if (!levelUpResultDialog_.open) {
        return;
    }
    renderer.fillRect(
        {0.0f, 0.0f},
        {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
        {0, 0, 0, 96});
    drawUiResultDialog(renderer, levelUpResultDialog_, levelUpResultDialogRect(), "level_up.result");
}

void Game::render(Renderer& renderer, const Time& time)
{
    renderer.clear({5, 5, 8, 255});
    beginUiFrame(time.deltaSeconds(), gamepadUiCursorEnabled_);
    if (mode_ == ScreenMode::OpeningKamishibai) {
        renderOpeningKamishibai(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::EndingKamishibai) {
        renderEndingKamishibai(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::Title) {
        renderTitleScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        renderObjectImageScaleEditScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        renderEnemyHitboxEditScreen(renderer, time.totalSeconds());
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::EnemyShadowEdit) {
        renderEnemyShadowEditScreen(renderer, time.totalSeconds());
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        renderAudioCueEditScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (effectTestActive_) {
        renderEffectTestScreen(renderer, time.totalSeconds());
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (projectileTestActive_) {
        renderProjectileTestScreen(renderer, time.totalSeconds());
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderer.present();
        return;
    }
    if (basePresentationActive()) {
        renderBaseScreen(renderer);
        if (levelUpPresentation_.active) {
            effects_.renderForeground(renderer);
            effects_.renderDamagePopups(renderer);
        }
        inventory_.render(
            renderer,
            player_,
            spellRing_,
            objectCatalog_,
            encyclopedia_,
            pauseReturnMode_ != ScreenMode::Base,
            pauseReturnMode_ != ScreenMode::Base,
            time.totalSeconds(),
            unlockedRingCount());
        renderPauseMenu(renderer);
        renderRingScreen(renderer, time.totalSeconds());
        dialogue_.render(renderer, camera_.width(), camera_.height());
        renderDebugNamedSaveUi(renderer);
        renderDebugItemPicker(renderer);
        renderDebugStoryTest(renderer);
        renderFirstItemAcquisitionNotice(renderer);
        renderLevelUpOverlay(renderer);
        finishUiFrame(renderer);
        renderBaseDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderWorldLoadingScreen(renderer, time.totalSeconds());
        renderer.present();
        return;
    }

    renderer.setWorldSpace(&camera_, screenShakeOffset(time.totalSeconds()));

    const bool liveRingHidden = liveSpellRingHiddenForDeath() || liveSpellRingHiddenForBossEncounter();
    const std::vector<const SpellRingItem*> runtimeItems = liveRingHidden
        ? std::vector<const SpellRingItem*>{}
        : visibleRuntimeRingItems(spellRing_, unlockedRingCount());
    const bool ringIntroActive = dungeonRingIntroActive();
    const bool lightweight = lightweightModeEnabled();
    const float totalSeconds = static_cast<float>(time.totalSeconds());
    const std::vector<LightSource> itemLights = collectDungeonLightSources(time.totalSeconds());
    const Vec2 playerLightCenter = witchSelfLightCenter(player_.position);
    tileMap_.render(renderer, camera_, playerLightCenter, itemLights);
    std::vector<DepthRenderEntry> worldDepthEntries;
    appendRewardNodeRenderEntries(worldDepthEntries, renderer, itemLights);
    if (!enemyTestActive_) {
        appendDungeonEventRenderEntries(worldDepthEntries, renderer, itemLights, time.totalSeconds());
    }
    wetGround_.appendRenderEntries(worldDepthEntries, renderer);
    groundLines_.appendRenderEntries(worldDepthEntries, renderer);
    worldDrops_.appendRenderEntries(worldDepthEntries, renderer, tileMap_, objectCatalog_, playerLightCenter, itemLights);
    effects_.appendRenderEntries(worldDepthEntries, renderer);
    magicFx_.appendRenderEntries(worldDepthEntries, renderer);
    if (!enemyTestActive_) {
        renderDungeonEntrance(renderer);
        renderWarpPoints(renderer);
        renderRoguelikeBigHole(renderer);
        renderRoguelikeFacilities(renderer);
    }

    bool ringCenterVisible = false;
    if (playerDeathSequenceActive()) {
        for (const PlayerDeathRingPresentation& presentation : playerDeathSequence_.ringPresentations) {
            if (presentation.active &&
                playerDeathRingPresentationHasVisibleItems(presentation) &&
                tileMap_.isLit(presentation.center, playerLightCenter, itemLights)) {
                ringCenterVisible = true;
                break;
            }
        }
    } else {
        const int ringCount = unlockedRingCount();
        for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
            if (!liveRingHidden &&
                !spellRing_.itemsForRing(ringIndex).empty() &&
                tileMap_.isLit(spellRing_.centerForRing(ringIndex), playerLightCenter, itemLights)) {
                ringCenterVisible = true;
                break;
            }
        }
    }
    if (ringCenterVisible && !ringIntroActive) {
        if (playerDeathSequenceActive()) {
            renderPlayerDeathRingPresentation(renderer, static_cast<float>(time.totalSeconds()));
        } else {
            drawSpellRingOrbitLayer(renderer, spellRing_, balance_, unlockedRingCount(), time.totalSeconds(), 0.46f);
        }
    }
    const Vec2 playerFootAnchor = player_.position;
    const EntityStatusVisualStyle playerStatusVisual = entityStatusVisualStyle(player_.status);
    const Vec2 playerVisualFootAnchor = playerFootAnchor +
        entityStatusJitterOffset(player_.status, time.totalSeconds()) +
        Vec2{0.0f, -stunWakeHopOffset(player_.stunWakeTimer)};
    const float playerSizeMultiplier = playerStatusVisual.scaleMultiplier;
    const float playerSpriteVisualSize = playerSpriteNaturalVisualSize(renderer, playerSizeMultiplier);
    const Color playerStatusTint = playerStatusVisual.hasTint ? playerStatusVisual.tint : Color{255, 255, 255, 255};
    renderer.drawActorShadow(playerFootAnchor, playerSpriteVisualSize);
    if (!lightweight) {
        worldDrops_.renderShadows(renderer, tileMap_, objectCatalog_, playerLightCenter, itemLights);
        enemies_.renderShadows(renderer, tileMap_, playerLightCenter, itemLights);
        effects_.renderShadows(renderer);
    }
    renderPlayerFootstepDust(renderer);
    const bool playerDeathActive = playerDeathSequenceActive();
    const auto drawPlayerVisual = [&]() {
        if (renderer.hasPlayerSheet()) {
            const CharacterSpriteMotion playerMotion = playerDeathActive
                ? CharacterSpriteMotion::Death
                : (player_.spriteWalking ? CharacterSpriteMotion::Walk : CharacterSpriteMotion::Idle);
            const float playerAnimationTime = playerDeathActive
                ? playerDeathSequence_.elapsedSeconds
                : player_.spriteAnimationTime;
            const int playerFrame = playerSpriteFrameIndex(playerAnimationTime, playerMotion);
            const bool playerFlip = player_.spriteFlipHorizontal;
            renderer.drawPlayerSpriteNaturalSize(
                playerFrame,
                playerVisualFootAnchor,
                playerSizeMultiplier,
                playerFlip,
                playerStatusTint,
                {PlayerSpriteAnchorX, PlayerSpriteAnchorY},
                playerStatusVisual.flipVertical);
            if (!playerDeathActive && !playerStatusVisual.flipVertical) {
                if (const InventoryObjectInstance* staffInstance = inventory_.equippedStaffInstance()) {
                    const PlayerHeldStaffDrawContext staffContext{
                        .footAnchor = playerVisualFootAnchor,
                        .spriteFrame = playerFrame,
                        .flipHorizontal = playerFlip,
                        .scale = playerSizeMultiplier,
                        .handTint = playerStatusTint,
                        .spriteAnchor = {PlayerSpriteAnchorX, PlayerSpriteAnchorY},
                    };
                    if (drawPlayerHeldStaff(renderer, staffInstance->item, staffContext)) {
                        drawPlayerHeldStaffHandOverlay(renderer, staffContext);
                    }
                }
            }
            if (player_.damageFlash > 0.0f) {
                const float flash = clamp(player_.damageFlash / 0.16f, 0.0f, 1.0f);
                const unsigned char alpha = static_cast<unsigned char>(std::round(185.0f * flash));
                renderer.drawPlayerSpriteNaturalSize(
                    playerFrame,
                    playerVisualFootAnchor,
                    playerSizeMultiplier,
                    playerFlip,
                    {255, 52, 52, alpha},
                    {PlayerSpriteAnchorX, PlayerSpriteAnchorY},
                    playerStatusVisual.flipVertical);
            }
        } else {
            const Color playerColor = player_.damageFlash > 0.0f
                ? Color{255, 72, 72, 255}
                : (playerStatusVisual.hasTint ? playerStatusVisual.tint : Color{118, 72, 168, 255});
            renderer.fillCircle(playerVisualFootAnchor, player_.effectiveRadius(balance_.playerRadius), playerColor);
            renderer.drawLine(playerVisualFootAnchor, playerVisualFootAnchor + player_.facing * (22.0f * playerSizeMultiplier), {235, 210, 255, 255});
        }
        renderEntityStatusOverlays(renderer, player_.status, playerVisualFootAnchor, playerSpriteVisualSize, time.totalSeconds());
    };
    worldDepthEntries.push_back(DepthRenderEntry{
        player_.position.y,
        [&]() {
            if (!playerDeathActive) {
                drawPlayerVisual();
            }
        },
    });
    if (!ringIntroActive) {
        if (playerDeathActive) {
            for (std::size_t presentationIndex = 0; presentationIndex < playerDeathSequence_.ringPresentations.size(); ++presentationIndex) {
                const PlayerDeathRingPresentation& presentation = playerDeathSequence_.ringPresentations[presentationIndex];
                if (!presentation.active) {
                    continue;
                }
                for (std::size_t itemIndex = 0; itemIndex < presentation.items.size(); ++itemIndex) {
                    const PlayerDeathRingItemPresentation& itemPresentation = presentation.items[itemIndex];
                    if (itemPresentation.dropped) {
                        continue;
                    }
                    const SpellRingItem& item = itemPresentation.item;
                    if (!tileMap_.isLit(item.worldPosition, playerLightCenter, itemLights)) {
                        continue;
                    }
                    appendRingItemDepthRenderEntry(
                        worldDepthEntries,
                        renderer,
                        spellRing_,
                        objectCatalog_,
                        RingItemDepthRenderSnapshot{
                            item.worldPosition.y,
                            item,
                        },
                        totalSeconds);
                }
            }
        } else if (!liveRingHidden) {
            const int ringCount = unlockedRingCount();
            for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                for (std::size_t itemIndex = 0; itemIndex < ringItems.size(); ++itemIndex) {
                    const SpellRingItem& item = ringItems[itemIndex];
                    if (!tileMap_.isLit(item.worldPosition, playerLightCenter, itemLights)) {
                        continue;
                    }
                    appendRingItemDepthRenderEntry(
                        worldDepthEntries,
                        renderer,
                        spellRing_,
                        objectCatalog_,
                        RingItemDepthRenderSnapshot{
                            item.worldPosition.y,
                            item,
                        },
                        totalSeconds);
                }
            }
        }
    }
    enemies_.appendRenderEntries(worldDepthEntries, renderer, tileMap_, playerLightCenter, itemLights, 0, &encyclopedia_);
    appendCaptureAbsorbRenderEntries(worldDepthEntries, renderer, time.totalSeconds());
    std::stable_sort(worldDepthEntries.begin(), worldDepthEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : worldDepthEntries) {
        entry.draw();
    }

    std::vector<DepthRenderEntry> projectileDepthEntries;
    projectiles_.appendRenderEntries(projectileDepthEntries, renderer, tileMap_, playerLightCenter, itemLights);
    std::stable_sort(projectileDepthEntries.begin(), projectileDepthEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : projectileDepthEntries) {
        entry.draw();
    }
    effects_.render(renderer);
    tileMap_.renderDarknessOverlay(renderer, camera_, playerLightCenter, itemLights, lightweight);
    std::vector<DepthRenderEntry> magicForegroundEntries;
    magicFx_.appendForegroundRenderEntries(magicForegroundEntries, renderer);
    std::stable_sort(magicForegroundEntries.begin(), magicForegroundEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : magicForegroundEntries) {
        entry.draw();
    }
    renderSpellRingForeground(renderer, runtimeItems, itemLights, time.totalSeconds());
    effects_.renderForeground(renderer);
    effects_.renderDamagePopups(renderer);
    if (playerDeathActive) {
        drawPlayerVisual();
    }

    renderer.setScreenSpace();
    const bool suppressDungeonUi = dungeonEventUiSuppressed();
    if (!suppressDungeonUi) {
        renderPlayerDamageVignette(renderer, time.totalSeconds());
    }
    renderTopInfoBar(renderer);
    if (mode_ == ScreenMode::Playing && !suppressDungeonUi && dungeonMapOverlayOpen_) {
        renderDungeonMapOverlay(renderer, itemLights);
    } else if (mode_ == ScreenMode::Playing && !suppressDungeonUi) {
        renderDungeonMinimap(renderer, itemLights);
        if (!introTutorialActive()) {
            renderRingStatusHud(renderer);
        }
        renderDungeonLogs(renderer);
        renderDungeonStatusHud(renderer);
    }
    if (debugPaused_) {
        renderer.fillRect({18.0f, 202.0f}, {190.0f, 28.0f}, {0, 0, 0, 190});
        renderer.drawText({28.0f, 208.0f}, "DEBUG PAUSED", {255, 230, 150, 255}, 2);
    }
    if (!suppressDungeonUi && !dungeonMapOverlayOpen_) {
        inventory_.render(
            renderer,
            player_,
            spellRing_,
            objectCatalog_,
            encyclopedia_,
            true,
            true,
            time.totalSeconds(),
            unlockedRingCount());
        renderLevelUpOverlay(renderer);
        if (mode_ == ScreenMode::Playing) {
            inventory_.renderShortcutHud(renderer, spellRing_, camera_.width(), camera_.height());
            renderRingEquipFx(renderer);
            renderDungeonControlHelp(renderer);
        } else if (mode_ == ScreenMode::Inventory && pauseReturnMode_ != ScreenMode::Base) {
            renderDungeonLogs(renderer);
        }
        renderWarpReturnUi(renderer);
        renderRoguelikeBigHoleUi(renderer);
        if (roguelikeFacilityUiActive()) {
            renderBaseScreen(renderer);
        }
        renderPauseMenu(renderer);
        renderRingScreen(renderer, time.totalSeconds());
        renderBossDefeatPresentation(renderer);
        renderGameOverScreen(renderer);
        renderStageClearScreen(renderer);
        renderAstralResultScreen(renderer);
        renderEnemyTestUi(renderer);
    }
    if (!suppressDungeonUi && !dungeonMapOverlayOpen_ && reloadNoticeTimer_ > 0.0f) {
        renderer.fillRect({18.0f, 170.0f}, {430.0f, 26.0f}, {0, 0, 0, 180});
        InlineItemTextStyle noticeStyle;
        noticeStyle.text = {255, 235, 150, 255};
        noticeStyle.scale = 2;
        noticeStyle.iconTextGap = 4.0f;
        noticeStyle.iconScale = 1.15f;
        drawInlineItemText(renderer, objectCatalog_, {26.0f, 176.0f}, reloadNotice_, noticeStyle);
    }
    if (!suppressDungeonUi && !dungeonMapOverlayOpen_ &&
        (mode_ == ScreenMode::Playing || mode_ == ScreenMode::Inventory || mode_ == ScreenMode::PauseMenu || mode_ == ScreenMode::Ring)) {
        std::vector<UiRect> encyclopediaAvoidRects;
        const float screenWidth = static_cast<float>(camera_.width());
        const float screenHeight = static_cast<float>(camera_.height());
        encyclopediaAvoidRects.push_back({{TopInfoBarX, TopInfoBarY}, {screenWidth - TopInfoBarX * 2.0f, TopInfoBarHeight + 8.0f}});
        if (reloadNoticeTimer_ > 0.0f) {
            encyclopediaAvoidRects.push_back({{18.0f, 170.0f}, {430.0f, 26.0f}});
        }
        if (mode_ == ScreenMode::Playing) {
            if (!enemyTestActive_ && !dungeonMinimapCells_.empty()) {
                encyclopediaAvoidRects.push_back(dungeonMinimapRect());
            }

            if (!introTutorialActive()) {
                const int unlockedRingCount = unlockedRingHudCount();
                for (int ringIndex = 0; ringIndex < unlockedRingCount; ++ringIndex) {
                    encyclopediaAvoidRects.push_back(ringStatusHudRect(ringIndex, unlockedRingCount));
                }
            }

            const UiRect statusPanel{{
                std::max(8.0f, screenWidth - DungeonStatusHudRightMargin - DungeonStatusHudWidth),
                std::max(TopInfoBarY + TopInfoBarHeight + 8.0f, screenHeight - DungeonStatusHudBottomMargin - DungeonStatusHudHeight),
            }, {DungeonStatusHudWidth, DungeonStatusHudHeight}};
            encyclopediaAvoidRects.push_back(statusPanel);

            int visibleLogCount = std::min(static_cast<int>(dungeonLogs_.size()), DungeonLogMaxVisible);
            const auto logBlockHeight = [](int count) {
                return static_cast<float>(count) * DungeonLogRowHeight +
                    static_cast<float>(std::max(0, count - 1)) * DungeonLogGap;
            };
            const float logTopLimit = TopInfoBarY + TopInfoBarHeight + 8.0f;
            const float statusTopY = screenHeight - DungeonStatusHudBottomMargin - DungeonStatusHudHeight;
            const float maxLogBottomY = std::max(logTopLimit + DungeonLogRowHeight, statusTopY - DungeonLogStatusGap);
            while (visibleLogCount > 0 && logBlockHeight(visibleLogCount) > maxLogBottomY - logTopLimit) {
                --visibleLogCount;
            }
            if (visibleLogCount > 0) {
                const float totalLogHeight = logBlockHeight(visibleLogCount);
                const float logX = std::max(8.0f, screenWidth - DungeonLogRightMargin - DungeonLogWidth);
                const float logY = std::clamp(screenHeight * DungeonLogTargetYRatio, logTopLimit, maxLogBottomY - totalLogHeight);
                encyclopediaAvoidRects.push_back({{logX, logY}, {DungeonLogWidth, totalLogHeight}});
            }
        }

        encyclopedia_.renderPopups(renderer, camera_, objectCatalog_, encyclopediaAvoidRects);
    }
    dialogue_.render(renderer, camera_.width(), camera_.height());
    if (!suppressDungeonUi) {
        renderDebugNamedSaveUi(renderer);
        renderDebugItemPicker(renderer);
        renderDebugStoryTest(renderer);
    }
    renderFirstItemAcquisitionNotice(renderer);
    renderAutoSimulationIntentOverlay(renderer);
    finishUiFrame(renderer);
    renderDebugOverlay(renderer, time);
    renderScreenTransitionOverlay(renderer);
    renderer.present();
}

} // namespace majo
