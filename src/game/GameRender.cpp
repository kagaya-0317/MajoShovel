#include "game/GameInternal.hpp"

#include "engine/FrameProfiler.hpp"
#include "engine/InputHelpGlyph.hpp"
#include "game/EnemyImageRenderer.hpp"
#include "game/EntityStatusVisuals.hpp"
#include "game/ExplosionWarning.hpp"
#include "game/MenuIconImage.hpp"
#include "game/PlayerEquipmentVisual.hpp"
#include "game/RingDisplayName.hpp"

#include <array>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace majo {

namespace {

constexpr std::string_view StoryBossSpritePath = "assets/enemies/boss_1.png";
constexpr int StoryBossSpriteColumns = 6;
constexpr int StoryBossSpriteRows = 6;
constexpr float StoryBossSpriteScale = 0.84f;
constexpr std::string_view StorySmallMoleSpritePath = "assets/enemies/story_small_mole_walk.png";
constexpr int StorySmallMoleSpriteColumns = 2;
constexpr int StorySmallMoleSpriteRows = 4;
constexpr float StorySmallMoleFrameSeconds = 0.12f;
constexpr std::string_view StoryCrabDishSpritePath = "assets/enemies/story_crab_dish.png";
constexpr Vec2 StoryCrabDishDrawSize{76.0f, 50.0f};
constexpr float StoryBossExplodeEscapeWarmupSeconds = 0.48f;
constexpr float StoryBossExplodeEscapePostExplosionHoldSeconds = 60.0f / 60.0f;
constexpr float StoryBossExplodeEscapeFadeInSeconds = 80.0f / 60.0f;
constexpr float StoryBossExplodeEscapePostFadeHoldSeconds = 20.0f / 60.0f;

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

std::uint32_t astralEchoHash(std::uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float astralEchoUnit(int index, int salt)
{
    const std::uint32_t hashed = astralEchoHash(static_cast<std::uint32_t>(index) * 0x9e3779b9u + static_cast<std::uint32_t>(salt));
    return static_cast<float>(hashed & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

Vec2 astralEchoStarPosition(UiRect area, int index)
{
    return {
        area.pos.x + astralEchoUnit(index, 11) * area.size.x,
        area.pos.y + astralEchoUnit(index, 37) * area.size.y,
    };
}

Color astralEchoStarColor(bool recent, float brightness)
{
    if (recent) {
        return {
            255,
            static_cast<unsigned char>(std::clamp(218.0f + brightness * 37.0f, 0.0f, 255.0f)),
            static_cast<unsigned char>(std::clamp(96.0f + brightness * 68.0f, 0.0f, 255.0f)),
            255,
        };
    }
    return {
        static_cast<unsigned char>(std::clamp(188.0f + brightness * 67.0f, 0.0f, 255.0f)),
        static_cast<unsigned char>(std::clamp(206.0f + brightness * 49.0f, 0.0f, 255.0f)),
        255,
        255,
    };
}

constexpr float PlayerDamageVignetteMinAlpha = 1.0f;
constexpr float PlayerDamageVignetteMaxAlpha = 132.0f;
constexpr float PlayerDamageVignetteMinEdgeWidth = 86.0f;
constexpr float PlayerDamageVignetteMaxEdgeWidth = 198.0f;
constexpr std::string_view DungeonStatusHudImagePath = "assets/system/UI_status.png";
constexpr std::string_view DungeonStatusHudGaugeMaskPath = "assets/system/UI_status_gaugeMask.png";
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
constexpr std::string_view RingStatusHudImagePath = "assets/system/UI_rings.png";
constexpr int RingStatusHudImageColumns = 2;
constexpr int RingStatusHudImageRows = 3;
constexpr Vec2 RingStatusHudFrameSize{229.0f, 104.0f};
constexpr float RingStatusHudSliceLeftWidth = 104.0f;
constexpr float RingStatusHudSliceRightWidth = 24.0f;
constexpr Vec2 RingStatusHudCooldownCenter{52.0f, 52.0f};
constexpr float RingStatusHudCooldownInnerRadius = 38.0f;
constexpr float RingStatusHudCooldownOuterRadius = 43.0f;
constexpr Vec2 RingStatusHudTextPos{104.0f, 20.0f};
constexpr float RingStatusHudLineGap = 22.0f;
constexpr UiRect RingStatusHudWeightGaugeRect{{104.0f, 80.0f}, {144.0f, 3.0f}};
constexpr Vec2 RingManagementWeightGaugeSize{220.0f, 4.0f};
constexpr float RingManagementWeightGaugeOffsetY = 26.0f;
constexpr float RingManagementWeightStateLineOffsetY = 40.0f;
constexpr float RingDetailLabelWidth = 106.0f;
constexpr float RingDetailWeightGaugeOffsetY = 26.0f;
constexpr float RingDetailWeightGaugeHeight = 4.0f;
constexpr float RingDetailWeightGaugeBottomGap = 10.0f;
constexpr Color RingStatusHudNameColor{255, 239, 172, 255};
constexpr Color RingStatusHudInactiveNameColor{246, 248, 255, 255};
constexpr Color RingStatusHudItemColor{232, 236, 244, 255};
constexpr Color RingStatusHudWeightColor{222, 236, 255, 255};
constexpr Color RingStatusHudCooldownCoolStartColor{255, 202, 64, 248};
constexpr Color RingStatusHudCooldownCoolEndColor{255, 255, 246, 252};
constexpr Color RingStatusHudCooldownReadyStartColor{142, 232, 255, 248};
constexpr Color RingStatusHudCooldownReadyEndColor{255, 255, 255, 252};
constexpr Color RingWeightGaugeBackColor{6, 15, 35, 168};
constexpr Color RingWeightGaugeFillColor{84, 218, 255, 238};
constexpr Color RingWeightGaugeOverColor{255, 72, 84, 238};
constexpr Color RingWeightGaugeLimitColor{255, 248, 190, 245};
constexpr float RingWeightOverloadEpsilon = 0.0001f;
constexpr Color RingWeightOverloadedTextColor{255, 112, 112, 255};
constexpr std::string_view RingStatusHudChargeReadySe = "se.ring.charge_ready";
constexpr float RingStatusHudReadyHoldSeconds = 1.0f;
constexpr float RingStatusHudFadeSeconds = 20.0f / 60.0f;
constexpr float RingStatusHudPulseSeconds = 0.56f;

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

RectF ringStatusHudSpriteSource(int ringIndex, bool active)
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, RingStatusHudImageRows - 1);
    const int column = active ? 1 : 0;
    return {
        RingStatusHudFrameSize.x * static_cast<float>(column),
        RingStatusHudFrameSize.y * static_cast<float>(clampedRingIndex),
        RingStatusHudFrameSize.x,
        RingStatusHudFrameSize.y,
    };
}

UiRect dungeonStatusHudRect(float screenWidth, float screenHeight)
{
    return {{
        std::max(8.0f, screenWidth - DungeonStatusHudRightMargin - DungeonStatusHudWidth),
        std::max(
            TopInfoBarY + TopInfoBarHeight + 8.0f,
            screenHeight - DungeonStatusHudBottomMargin - DungeonStatusHudHeight - DungeonHudUpShift),
    }, {DungeonStatusHudWidth, DungeonStatusHudHeight}};
}

void drawRingStatusHudCooldownPulse(Renderer& renderer, Vec2 panelPos, float pulseTimer, float alphaScale)
{
    if (pulseTimer <= 0.0f || alphaScale <= 0.001f) {
        return;
    }

    const float remaining = clamp(pulseTimer / RingStatusHudPulseSeconds, 0.0f, 1.0f);
    const float t = 1.0f - remaining;
    const Vec2 center = panelPos + RingStatusHudCooldownCenter;
    renderer.drawSoftRing(
        center,
        RingStatusHudCooldownOuterRadius + 2.0f + 14.0f * t,
        8.0f,
        withAlpha(RingStatusHudCooldownReadyStartColor, 154.0f * remaining * alphaScale));
    renderer.fillSoftCircle(
        center,
        RingStatusHudCooldownOuterRadius + 8.0f * t,
        withAlpha(RingStatusHudCooldownReadyEndColor, 36.0f * remaining * alphaScale));
}

void drawRingStatusHudCooldown(Renderer& renderer, Vec2 panelPos, float cooldownRatio, float alphaScale)
{
    const float readyRatio = 1.0f - clamp(cooldownRatio, 0.0f, 1.0f);
    if (readyRatio <= 0.001f || alphaScale <= 0.001f) {
        return;
    }

    const bool ready = cooldownRatio <= 0.001f;
    renderer.fillSoftRingArc(
        panelPos + RingStatusHudCooldownCenter,
        RingStatusHudCooldownInnerRadius,
        RingStatusHudCooldownOuterRadius,
        -Pi * 0.5f,
        Pi * 2.0f * readyRatio,
        withAlpha(ready ? RingStatusHudCooldownReadyStartColor : RingStatusHudCooldownCoolStartColor, 255.0f * alphaScale),
        withAlpha(ready ? RingStatusHudCooldownReadyEndColor : RingStatusHudCooldownCoolEndColor, 255.0f * alphaScale));
}

void drawRingWeightGauge(Renderer& renderer, UiRect gauge, float weight, float limit)
{
    if (gauge.size.x <= 0.0f || gauge.size.y <= 0.0f) {
        return;
    }

    renderer.fillRect(gauge.pos, gauge.size, RingWeightGaugeBackColor);

    const float safeLimit = std::max(0.001f, limit);
    const float ratio = std::max(0.0f, weight / safeLimit);
    constexpr float GaugeMaxRatio = SpellRingSystem::OverweightEquipLimitRatio;
    const float limitX = gauge.pos.x + gauge.size.x / GaugeMaxRatio;
    const float blueWidth = gauge.size.x * clamp(std::min(ratio, 1.0f) / GaugeMaxRatio, 0.0f, 1.0f);
    if (blueWidth > 0.0f) {
        renderer.fillRect(gauge.pos, {blueWidth, gauge.size.y}, RingWeightGaugeFillColor);
    }

    const float overRatio = std::max(0.0f, ratio - (1.0f + RingWeightOverloadEpsilon));
    const float overWidth = gauge.size.x * clamp(overRatio / GaugeMaxRatio, 0.0f, 1.0f);
    if (overWidth > 0.0f) {
        renderer.fillRect({limitX, gauge.pos.y}, {overWidth, gauge.size.y}, RingWeightGaugeOverColor);
    }

    renderer.fillRect({limitX - 0.5f, gauge.pos.y - 2.0f}, {1.0f, gauge.size.y + 4.0f}, RingWeightGaugeLimitColor);
}

bool ringStatusHudLeftCircleContains(UiRect panel, Vec2 point)
{
    constexpr float Radius = RingStatusHudFrameSize.y * 0.5f;
    return distanceSquared(point, panel.pos + RingStatusHudCooldownCenter) <= Radius * Radius;
}

bool ringStatusHudRightWindowContains(UiRect panel, Vec2 point)
{
    const UiRect rightWindow{
        panel.pos + Vec2{RingStatusHudSliceLeftWidth, 0.0f},
        {std::max(0.0f, panel.size.x - RingStatusHudSliceLeftWidth), panel.size.y},
    };
    return rightWindow.contains(point);
}

bool inputHelpUsesGamepad()
{
    switch (inputHelpDeviceMode()) {
    case InputHelpDeviceMode::KeyboardMouse:
        return false;
    case InputHelpDeviceMode::Gamepad:
        return true;
    case InputHelpDeviceMode::Auto:
        break;
    }
    const Input* input = inputHelpContext();
    return input != nullptr && input->lastActiveDevice() == InputDeviceKind::Gamepad;
}

UiScrollAreaLayout makeTitleCreditsLayout(
    Renderer& renderer,
    std::string_view text,
    float scrollOffset,
    const UiScrollAreaStyle& style)
{
    constexpr float TextPaddingY = 12.0f;
    const UiRect viewport = titleCreditsViewportRect();
    const auto contentHeightForWidth = [&](float width) {
        return renderer.measureWrappedText(text, std::max(1.0f, width), 2).y +
            TextPaddingY * 2.0f;
    };

    UiScrollAreaLayout layout = makeUiScrollAreaLayout(
        viewport,
        contentHeightForWidth(viewport.size.x),
        scrollOffset,
        style);
    if (layout.scrollable) {
        layout = makeUiScrollAreaLayout(
            viewport,
            contentHeightForWidth(layout.content.size.x),
            scrollOffset,
            style);
    }
    return layout;
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

std::string dungeonDepthTopInfoEntry(
    const DungeonLayout& layout,
    Vec2 tilePosition,
    bool offMainRoute)
{
    char buffer[64];
    const int meters = std::max(0, static_cast<int>(std::lround(projectedDungeonRouteDistanceTiles(layout, tilePosition))));
    std::snprintf(
        buffer,
        sizeof(buffer),
        offMainRoute ? "深度 %dm (寄り道中)" : "深度 %dm",
        meters);
    return buffer;
}

std::string roguelikeDepthTopInfoEntry(
    const DungeonLayout& layout,
    Vec2 tilePosition,
    int areaStartMeters,
    int areaEndMeters,
    int completionMeters,
    bool offMainRoute)
{
    const int start = std::max(0, areaStartMeters);
    const int end = std::max(start + 1, areaEndMeters);
    const int meters = std::clamp(
        start + static_cast<int>(std::lround(projectedDungeonRouteDistanceTiles(layout, tilePosition))),
        0,
        std::min(end, std::max(1, completionMeters)));
    char buffer[64];
    std::snprintf(
        buffer,
        sizeof(buffer),
        offMainRoute ? "深度 %dm (寄り道中)" : "深度 %dm",
        meters);
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

struct RingWeightUiState {
    float totalWeight = 0.0f;
    float maxWeight = 0.0f;
    float weightRatio = 0.0f;
    float stopRatio = SpellRingSystem::MaxWeightStopRatio;
    bool overloaded = false;
};

RingWeightUiState ringWeightUiStateForRing(const SpellRingSystem& spellRing, int ringIndex)
{
    ringIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);

    RingWeightUiState state;
    state.totalWeight = spellRing.totalEquippedWeightForRing(ringIndex);
    state.maxWeight = spellRing.maxEquippedWeightForRing(ringIndex);
    state.weightRatio = state.maxWeight > 0.0f
        ? state.totalWeight / state.maxWeight
        : (state.totalWeight > 0.0f ? SpellRingSystem::MaxWeightStopRatio : 0.0f);
    state.stopRatio = spellRing.weightStopRatioForRing(ringIndex);
    state.overloaded = state.weightRatio > 1.0f + RingWeightOverloadEpsilon;
    return state;
}

std::string ringWeightStateLabel(const RingWeightUiState& state)
{
    if (state.weightRatio < 0.5f) {
        return "重量余裕あり";
    }
    if (state.weightRatio < 0.85f) {
        return "重量あと少し";
    }
    if (!state.overloaded) {
        return "重量いっぱい";
    }
    if (state.weightRatio < state.stopRatio) {
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
    std::snprintf(buffer, sizeof(buffer), "リング速度 %.2f倍", speedMultiplier);
    return buffer;
}

void drawRingManagementWeightSummary(
    Renderer& renderer,
    Vec2 pos,
    const SpellRingSystem& spellRing,
    int ringIndex)
{
    constexpr Color NormalColor{188, 202, 224, 255};
    constexpr int TextScale = 2;

    const RingWeightUiState state = ringWeightUiStateForRing(spellRing, ringIndex);
    const Color color = state.overloaded ? RingWeightOverloadedTextColor : NormalColor;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "重量 %.1f / %.1fkg", state.totalWeight, state.maxWeight);
    renderer.drawText(pos, buffer, color, TextScale);
    drawRingWeightGauge(
        renderer,
        {pos + Vec2{0.0f, RingManagementWeightGaugeOffsetY}, RingManagementWeightGaugeSize},
        state.totalWeight,
        state.maxWeight);
    renderer.drawText(
        pos + Vec2{0.0f, RingManagementWeightStateLineOffsetY},
        ringWeightStateLabel(state),
        color,
        TextScale);
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
    constexpr float MinLineHeight = 31.0f;
    constexpr float LineGap = 4.0f;
    constexpr float ModifierGap = 7.0f;
    constexpr int TextScale = 2;
    constexpr Color ModifierColor{255, 230, 150, 255};

    const float labelX = panel.pos.x + ui::SubPanelPadding.x;
    const float valueX = labelX + RingDetailLabelWidth;
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
    int ringIndex,
    int unlockedRingCount)
{
    ringIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    drawUiSubPanel(renderer, panel);

    const RingEquipmentModifiers& ringModifiers = ringEquipmentModifiersForRing(modifiers, ringIndex);
    const std::vector<SpellRingItem>& items = spellRing.itemsForRing(ringIndex);
    float y = drawUiDetailHeaderWithIcon(
        renderer,
        panel,
        ringDisplayName(ringIndex, unlockedRingCount),
        ringDisplayIconImageNumber(ringIndex));

    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%02d/%02d", static_cast<int>(items.size()), spellRing.maxItemCountForRing(ringIndex));
    drawRingDetailLineWithModifier(renderer, panel, y, "アイテム", buffer);

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

    const RingWeightUiState weightState = ringWeightUiStateForRing(spellRing, ringIndex);
    const float weightLineY = y;
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%.1f/%.1fkg",
        weightState.totalWeight,
        weightState.maxWeight);
    drawRingDetailLineWithModifier(
        renderer,
        panel,
        y,
        "重量",
        buffer,
        weightModifierSuffix(ringModifiers.ringWeightLimitAdd));

    const float weightGaugeLeft = panel.pos.x + ui::SubPanelPadding.x + RingDetailLabelWidth;
    const float weightGaugeRight = panel.pos.x + panel.size.x - ui::SubPanelPadding.x;
    drawRingWeightGauge(
        renderer,
        {{weightGaugeLeft, weightLineY + RingDetailWeightGaugeOffsetY},
         {std::max(0.0f, weightGaugeRight - weightGaugeLeft), RingDetailWeightGaugeHeight}},
        weightState.totalWeight,
        weightState.maxWeight);
    y += RingDetailWeightGaugeBottomGap;

    const Color stateColor = weightState.overloaded ? RingWeightOverloadedTextColor : ui::Text;
    drawRingDetailLineWithModifier(
        renderer,
        panel,
        y,
        "",
        ringWeightStateLabel(weightState),
        {},
        stateColor,
        weightState.overloaded ? RingWeightOverloadedTextColor : ui::TextMuted);
    if (weightState.overloaded) {
        drawRingDetailLineWithModifier(
            renderer,
            panel,
            y,
            "",
            ringWeightSpeedStateLine(spellRing.weightSpeedMultiplierForRing(ringIndex)),
            {},
            RingWeightOverloadedTextColor,
            RingWeightOverloadedTextColor);
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

constexpr float ItemAcquisitionWindowWidth = 720.0f;
constexpr float ItemAcquisitionWindowMargin = 16.0f;
constexpr float ItemAcquisitionBodyMarginX = 30.0f;
constexpr float ItemAcquisitionBodyTopOffset = 82.0f;
constexpr float ItemAcquisitionContentButtonGap = 20.0f;
constexpr float ItemAcquisitionStatusAreaHeight = 28.0f;
constexpr float ItemAcquisitionButtonBottomGap = 52.0f;
constexpr Vec2 ItemAcquisitionImagePanelSize{136.0f, 136.0f};
constexpr float ItemAcquisitionImageDetailGap = 16.0f;

enum class ItemAcquisitionPrimaryAction {
    None,
    Use,
    EquipStaff,
};

ItemAcquisitionPrimaryAction itemAcquisitionPrimaryAction(
    const InventorySystem& inventory,
    const SpellRingSystem& spellRing,
    const ItemData* item,
    std::string_view objectId,
    std::string_view instanceId)
{
    if (item == nullptr) {
        return ItemAcquisitionPrimaryAction::None;
    }
    if (isStaffObject(*item)) {
        return inventory.canEquipStaffObjectById(objectId, instanceId, spellRing)
            ? ItemAcquisitionPrimaryAction::EquipStaff
            : ItemAcquisitionPrimaryAction::None;
    }
    return inventory.canUseObjectById(objectId, instanceId)
        ? ItemAcquisitionPrimaryAction::Use
        : ItemAcquisitionPrimaryAction::None;
}

float itemAcquisitionNoticeWidth(int screenWidth)
{
    return std::min(
        ItemAcquisitionWindowWidth,
        std::max(1.0f, static_cast<float>(screenWidth) - ItemAcquisitionWindowMargin * 2.0f));
}

UiRect itemAcquisitionNoticeRect(
    int screenWidth,
    int screenHeight,
    float contentHeight,
    bool statusVisible)
{
    const float statusHeight = statusVisible ? ItemAcquisitionStatusAreaHeight : 0.0f;
    const float desiredHeight = ItemAcquisitionBodyTopOffset +
        std::max(ItemAcquisitionImagePanelSize.y, contentHeight) +
        ItemAcquisitionContentButtonGap + statusHeight +
        ui::ButtonHeight + ItemAcquisitionButtonBottomGap;
    const Vec2 size{
        itemAcquisitionNoticeWidth(screenWidth),
        std::min(
            desiredHeight,
            std::max(1.0f, static_cast<float>(screenHeight) - ItemAcquisitionWindowMargin * 2.0f)),
    };
    return {{
        (static_cast<float>(screenWidth) - size.x) * 0.5f,
        (static_cast<float>(screenHeight) - size.y) * 0.5f,
    }, size};
}

UiRect itemAcquisitionOkButtonRect(UiRect panel)
{
    constexpr Vec2 Size{180.0f, ui::ButtonHeight};
    return {{
        panel.pos.x + (panel.size.x - Size.x) * 0.5f,
        panel.pos.y + panel.size.y - ItemAcquisitionButtonBottomGap - Size.y,
    }, Size};
}

UiRect itemAcquisitionNoticeBodyRect(UiRect panel, bool statusVisible)
{
    const float statusHeight = statusVisible ? ItemAcquisitionStatusAreaHeight : 0.0f;
    const float y = panel.pos.y + ItemAcquisitionBodyTopOffset;
    return {{
        panel.pos.x + ItemAcquisitionBodyMarginX,
        y,
    }, {
        panel.size.x - ItemAcquisitionBodyMarginX * 2.0f,
        std::max(
            0.0f,
            itemAcquisitionOkButtonRect(panel).pos.y - y -
                ItemAcquisitionContentButtonGap - statusHeight),
    }};
}

UiRect itemAcquisitionNoticeImagePanelRect(UiRect panel, bool statusVisible)
{
    return {itemAcquisitionNoticeBodyRect(panel, statusVisible).pos, ItemAcquisitionImagePanelSize};
}

UiRect itemAcquisitionNoticeDetailLayoutRect(UiRect panel, bool statusVisible)
{
    const UiRect body = itemAcquisitionNoticeBodyRect(panel, statusVisible);
    return {{
        body.pos.x + ItemAcquisitionImagePanelSize.x + ItemAcquisitionImageDetailGap,
        body.pos.y - ui::SubPanelPadding.y,
    }, {
        std::max(
            1.0f,
            body.size.x - ItemAcquisitionImagePanelSize.x - ItemAcquisitionImageDetailGap),
        body.size.y + ui::SubPanelPadding.y * 2.0f,
    }};
}

UiRect itemAcquisitionNoticeDetailMeasureRect(int screenWidth)
{
    const float bodyWidth = itemAcquisitionNoticeWidth(screenWidth) - ItemAcquisitionBodyMarginX * 2.0f;
    return {{0.0f, -ui::SubPanelPadding.y}, {
        std::max(
            1.0f,
            bodyWidth - ItemAcquisitionImagePanelSize.x - ItemAcquisitionImageDetailGap),
        1.0f,
    }};
}

std::string itemAcquisitionObjectDisplayName(const ItemData& item, int amount)
{
    std::string name = item.name;
    if (amount > 1) {
        name += " x" + std::to_string(amount);
    }
    return name;
}

Vec2 itemAcquisitionNoticeImageCenter(UiRect imagePanel)
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
constexpr Color DungeonMapChestColor{82, 158, 236, 238};
constexpr Color DungeonMapCrateColor{144, 86, 42, 238};
constexpr Color DungeonMapEventColor{246, 98, 206, 238};
constexpr Color DungeonMapItemColor{104, 210, 116, 238};
constexpr Color DungeonMapMoneyColor{248, 214, 64, 238};
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

void drawDungeonMapWarpMarker(Renderer& renderer, Vec2 center, float radius, Color fill)
{
    renderer.fillCircle(center, radius, fill);
    renderer.drawCircle(center, radius + 2.0f, fill);
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
    const float tipLength = 8.8f * scale;
    const float backLength = 4.4f * scale;
    const float halfWidth = 5.8f * scale;

    const Vec2 points[] = {
        center + direction * tipLength,
        center - direction * backLength + tangent * halfWidth,
        center - direction * backLength - tangent * halfWidth,
    };
    renderer.fillPolygon(points, 3, {248, 244, 212, 255});
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
    Move,
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
        result.items.push_back({"位置を移動", false});
        result.actions.push_back(RingCommandAction::Move);
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
    result.items.push_back({"位置を移動", true});
    result.actions.push_back(RingCommandAction::Move);
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

UiRect ringArrangeButtonRect()
{
    return uiFooterActionButtonRect(
        ringPanelRect(),
        {180.0f, ui::ButtonHeight},
        UiFooterActionAlignment::Left);
}

UiRect ringPresetButtonRect(bool registerPreset)
{
    constexpr float ButtonWidth = 196.0f;
    UiRect rect = uiFooterActionButtonRect(
        ringMainContentRect(),
        {ButtonWidth, ui::ButtonHeight},
        UiFooterActionAlignment::Right);
    if (!registerPreset) {
        rect.pos.x -= ButtonWidth + ui::ButtonGap;
    }
    return rect;
}

constexpr std::array<UiCommandMenuItem, RingPresetSlotCount> RingPresetMenuItems{{
    {"プリセット1", true},
    {"プリセット2", true},
    {"プリセット3", true},
}};

UiRect ringDiscardConfirmRect()
{
    return uiEnsureDecoratedWindowMinSize({{390.0f, 220.0f}, {500.0f, 280.0f}});
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
        return "配置できるアイテムがないよ";
    }
    if (!spellRing.canAddItem()) {
        return "リング満杯だよ";
    }
    if (!ringInventoryHasAnyAutoPlaceableItem(inventory, spellRing)) {
        return "過積載限界のため配置できないよ";
    }
    return "この位置には配置できないよ";
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
    float totalTime,
    int unlockedRingCount)
{
    const UiRect panel = ringPlaceWindowRect();
    UiModalNavigationScope navigationScope(panel);
    UiWindowScope placeWindow(
        renderer,
        "ring.place",
        panel,
        "アイテム配置",
        buildInputHelpText({
            {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, "配置"},
            {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "戻る"},
        }),
        UiWindowOptions{true, true});

    const int slotCount = std::min(inventory.screenSlotCount(), RingPlaceSlotCount);
    for (int i = 0; i < slotCount; ++i) {
        const InventoryUiEntryView entry = ringPlaceEntryView(inventory, i);
        const bool hasItem = entry.item != nullptr;
        const bool enabled = ringPlaceSlotEnabled(inventory, spellRing, i, localAngle);
        InventoryUiSlotStyle style{i == selection && enabled, hasItem && !enabled, RingPlaceSlotImageMaxSize};
        applyInventoryUiPowerBadgeDiscovery(style, encyclopedia);
        applyInventoryUiStackCount(style, entry);
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
            .unlockedRingCount = unlockedRingCount,
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
    UiModalNavigationScope navigationScope(panel);
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
        iconPrefix + itemName + "を捨てる？",
        body.size.x,
        questionStyle);
    float y = body.pos.y;
    drawInlineItemText(renderer, objectCatalog, {body.pos.x, y}, question, questionStyle);
    y += measureInlineItemText(renderer, question, questionStyle).y;
    renderer.drawWrappedText(
        {body.pos.x, y},
        "（捨てたアイテムは再入手できないよ）",
        body.size.x,
        ui::Text,
        2);

    drawUiConfirmDialogButtons(renderer, state, panel);
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
        status = "外せるアイテムがないよ";
        return false;
    }

    const int removableCount = static_cast<int>(candidates.size());
    const int removeLimit = std::min(removableCount, ringInventoryFreeObjectSlots(inventory));
    if (removeLimit <= 0) {
        status = "インベントリ満杯のため外せないよ";
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
        status = "インベントリ満杯のため外せないよ";
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
        status = "外せるアイテムをすべて戻したよ";
    } else {
        status = "空き枠分だけ重い順に" + std::to_string(removedCount) + "個戻したよ";
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
        status = "アイテム未選択";
        return false;
    }

    const SpellRingItem& selectedItem = items[static_cast<std::size_t>(selection)];
    if (selectedItem.objectId.empty()) {
        status = "このアイテムは外せないよ";
        return false;
    }
    if (!returnRingItemToInventory(inventory, objectCatalog, selectedItem)) {
        status = "インベントリ満杯のため外せないよ";
        return false;
    }

    items.erase(items.begin() + selection);
    selection = std::min(selection, std::max(0, static_cast<int>(items.size()) - 1));
    status = "インベントリへ戻したよ";
    return true;
}

bool toggleRingItemProtection(std::vector<SpellRingItem>& items, int selection, std::string& status)
{
    if (selection < 0 || selection >= static_cast<int>(items.size())) {
        status = "アイテム未選択";
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
        status = "アイテム未選択";
        return false;
    }

    const SpellRingItem& selectedItem = items[static_cast<std::size_t>(selection)];
    const ItemData* itemData = objectForRingItem(objectCatalog, selectedItem);
    if (selectedItem.objectId.empty() || itemData == nullptr) {
        status = "このアイテムは捨てられないよ";
        return false;
    }
    if (selectedItem.protectionEnabled) {
        status = "保護中のアイテムは捨てられないよ";
        return false;
    }
    if (isImportantItem(*itemData)) {
        status = "重要アイテムは捨てられないよ";
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
    const float worldVisualScale = spellRing.worldItemVisualScale(item);
    const float popScale = worldVisualScale * (lerp(0.56f, 1.0f, reveal) + std::sin(local * Pi) * 0.16f * (1.0f - local));
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
        options.scaleMultiplier = popScale / std::max(0.001f, worldVisualScale);
        return drawRingItemObjectImage(
            renderer,
            item,
            object,
            drawPosition,
            {RingObjectImageMaxSize * worldVisualScale, RingObjectImageMaxSize * worldVisualScale},
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

void drawSpellRingItemWorldVisual(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const SpellRingItem& item,
    float totalSeconds,
    bool drawShadow,
    bool drawMagicAuraOverlay)
{
    const float worldVisualScale = spellRing.worldItemVisualScale(item);
    const Vec2 drawPosition = ringItemDrawPosition(item, totalSeconds);

    if (drawShadow) {
        renderer.drawActorShadow(
            actorShadowAnchor(item.worldPosition, ItemShadowGroundOffsetY),
            ringItemShadowVisualSize(item, totalSeconds) * worldVisualScale);
    }

    const ItemData* object = objectForRingItem(objectCatalog, item);
    const Vec2 outward = item.orbitOutward;
    const Vec2 maxImageSize{
        RingObjectImageMaxSize * worldVisualScale,
        RingObjectImageMaxSize * worldVisualScale};
    const ExplosionWarningVisual breakExplosionWarning = ringItemBreakExplosionWarningVisual(item);
    const float breakExplosionVisualRadius = std::max(10.0f, item.hitRadius * worldVisualScale);
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
            renderer.fillCircle(drawPosition, item.hitRadius * worldVisualScale, {178, 184, 190, 255});
            renderer.drawLine(drawPosition, drawPosition + outward * (15.0f * worldVisualScale), {90, 96, 102, 255});
        } else if (item.type == SpellRingItemType::Torch) {
            renderer.fillCircle(drawPosition, item.hitRadius * worldVisualScale, {242, 122, 25, 255});
            renderer.fillCircle(drawPosition + Vec2{2.0f, -2.0f} * worldVisualScale, 4.0f * worldVisualScale, {255, 238, 98, 255});
        } else {
            renderer.fillCircle(drawPosition, item.hitRadius * worldVisualScale, {96, 122, 210, 255});
            renderer.drawCircle(drawPosition, item.hitRadius * worldVisualScale + 3.0f, {160, 202, 255, 255});
        }
    }

    drawRingItemBreakExplosionWarningOverlay(renderer, drawPosition, breakExplosionVisualRadius, breakExplosionWarning);
    drawDetectionBadges(renderer, item, drawPosition, worldVisualScale);

    if (drawMagicAuraOverlay && item.magicAuraTimer > 0.0f && !item.magicAuraDamageType.empty() && item.magicAuraFxEmitterId == 0) {
        drawMagicAura(
            renderer,
            drawPosition,
            std::max(8.0f, item.hitRadius * worldVisualScale),
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

void tagDepthRenderEntries(std::vector<DepthRenderEntry>& entries, std::size_t firstIndex, const char* profileName)
{
    for (std::size_t index = firstIndex; index < entries.size(); ++index) {
        entries[index].profileName = profileName;
    }
}

constexpr std::string_view DungeonMinimapFramePath = "assets/system/UI_mapFrame.png";
constexpr Vec2 DungeonMinimapFrameImageSize{200.0f, 200.0f};
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
constexpr float DungeonMapOverlayMinTilePx = 8.0f;
constexpr float DungeonMapOverlayMaxTilePx = 12.0f;

Renderer::ColorRect dungeonMapOverlayTileRect(Vec2 origin, int offsetX, int offsetY, float tilePx, Color color)
{
    const float left = std::floor(origin.x + static_cast<float>(offsetX) * tilePx);
    const float top = std::floor(origin.y + static_cast<float>(offsetY) * tilePx);
    const float right = std::ceil(origin.x + static_cast<float>(offsetX + 1) * tilePx);
    const float bottom = std::ceil(origin.y + static_cast<float>(offsetY + 1) * tilePx);
    return Renderer::ColorRect{
        {left, top},
        {
            std::max(1.0f, right - left),
            std::max(1.0f, bottom - top),
        },
        color};
}

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
constexpr int OptionsPageVideo = 0;
constexpr int OptionsPageAudio = 1;
constexpr int OptionsPageOperation = 2;
constexpr int OptionsPageCount = 3;
constexpr int OperationSettingsCategoryCount = 2;
constexpr int AudioSettingsRowCount = 3;
constexpr int VideoSettingsRowCount = 7;
constexpr int VideoSettingsRowWindowMode = 0;
constexpr int VideoSettingsRowWindowSize = 1;
constexpr int VideoSettingsRowBrightness = 2;
constexpr int VideoSettingsRowInputIcons = 3;
constexpr int VideoSettingsRowScreenShake = 4;
constexpr int VideoSettingsRowLightweight = 5;
constexpr int VideoSettingsRowVSync = 6;

std::string standardMenuHelpText(std::string_view primaryLabel = "決定", std::string_view backLabel = "戻る")
{
    return buildInputHelpText({
        {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, std::string(primaryLabel)},
        {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, std::string(backLabel)},
    });
}

std::string optionsMenuHelpText(int page)
{
    std::vector<InputHelpEntry> entries;
    if (page != OptionsPageAudio) {
        entries.push_back({
            InputHelpGroup::Primary,
            {InputAction::Confirm, InputAction::UseSelectedItem},
            page == OptionsPageOperation ? "割当操作" : "切替",
        });
    }
    entries.push_back({InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "戻る"});
    entries.push_back({
        InputHelpGroup::Cycle,
        {InputAction::CyclePrevious, InputAction::CycleNext},
        "設定切替",
    });
    return buildInputHelpText(entries);
}

struct OptionsSliderUiState {
    std::array<UiSliderState, AudioSettingsRowCount> audio{};
    UiSliderState brightness{};
};

OptionsSliderUiState& optionsSliderUiState()
{
    static OptionsSliderUiState state;
    return state;
}

constexpr float OptionsContentYOffset = -16.0f;
constexpr float OptionsStandardContentYOffset = OptionsContentYOffset + 8.0f;
constexpr float OptionsStandardContentHeightBonus = 16.0f;
constexpr float OptionsDetailWindowHeightExtension = 30.0f;
constexpr float OptionsSettingsRowHeight = 38.0f * 1.2f;
constexpr float OptionsSettingsRowGap = 4.0f;
constexpr float OperationSettingsTextOffsetY = 2.0f;
constexpr Color OperationSettingsHoveredCellFill{94, 94, 102, 190};
constexpr Color OperationSettingsSelectedCellFill{56, 76, 154, 190};

struct OperationSettingsActionRow {
    InputAction action;
    const char* label;
    int category;
};

enum class OperationSettingsCommand {
    Replace,
    Append,
    Clear,
    Reset,
};

struct VideoResolutionPreset {
    int width;
    int height;
};

constexpr const char* OptionsPageLabels[OptionsPageCount] = {
    "画面",
    "音量",
    "操作",
};

constexpr std::array<MenuIconImage, OptionsPageCount> OptionsPageIcons{{
    MenuIconImage::ScreenSettings,
    MenuIconImage::Volume,
    MenuIconImage::Gamepad,
}};

constexpr const char* OperationSettingsCategoryLabels[OperationSettingsCategoryCount] = {
    "基本",
    "リング/アイテム",
};

constexpr std::array<UiCommandMenuItem, 4> OperationSettingsCommandItems{{
    {"変更", true},
    {"追加", true},
    {"削除", true},
    {"初期化", true},
}};

constexpr VideoResolutionPreset VideoResolutionPresets[] = {
    {1280, 720},
    {1600, 900},
    {1920, 1080},
    {2560, 1440},
};
constexpr int VideoResolutionPresetCount = static_cast<int>(sizeof(VideoResolutionPresets) / sizeof(VideoResolutionPresets[0]));

constexpr OperationSettingsActionRow OperationSettingsActionRows[] = {
    {InputAction::MoveLeft, "左へ移動／選択", 0},
    {InputAction::MoveRight, "右へ移動／選択", 0},
    {InputAction::MoveUp, "上へ移動／選択", 0},
    {InputAction::MoveDown, "下へ移動／選択", 0},
    {InputAction::Confirm, "決定", 0},
    {InputAction::Cancel, "戻る", 0},
    {InputAction::Pause, "メニュー", 0},
    {InputAction::OpenInventory, "アイテム画面を開く", 0},
    {InputAction::OpenOptions, "オプション（タイトル画面）", 0},
    {InputAction::OpenCredits, "クレジット（タイトル画面）", 0},
    {InputAction::ToggleFullscreen, "フルスクリーン切替", 0},
    {InputAction::ThrowActiveRing, "リングを投げる", 1},
    {InputAction::OffsetRingCenter, "リングずらし", 1},
    {InputAction::ShiftRingLeft, "リングずらし：左", 1},
    {InputAction::ShiftRingRight, "リングずらし：右", 1},
    {InputAction::ShiftRingUp, "リングずらし：上", 1},
    {InputAction::ShiftRingDown, "リングずらし：下", 1},
    {InputAction::UseSelectedItem, "選択アイテム使用", 1},
    {InputAction::DiscardSelectedItem, "選択アイテムを捨てる", 1},
    {InputAction::PutSelectedItemOnRing, "リングへ入れる", 1},
    {InputAction::GrabOrPlaceItem, "つかむ/置く", 1},
    {InputAction::ArrangeItems, "並び替え", 1},
    {InputAction::SecondaryActionModifier, "サブ操作", 1},
    {InputAction::CyclePrevious, "前へ切替", 1},
    {InputAction::CycleNext, "次へ切替", 1},
    {InputAction::ToggleProtection, "アイテム保護切替", 1},
};

UiRect optionsPanelRect()
{
    return optionsMenuPanelRect();
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
    }, {
        284.0f,
        332.0f + OptionsStandardContentHeightBonus + OptionsDetailWindowHeightExtension,
    }};
}

float optionsDetailWindowBottomY()
{
    const UiRect detail = optionsRightHelpWindowRect();
    return detail.pos.y + detail.size.y;
}

UiRect operationSettingsTableRect()
{
    const UiRect panel = optionsPanelRect();
    const Vec2 position{
        panel.pos.x + 46.0f,
        panel.pos.y + 220.0f + OptionsContentYOffset,
    };
    return {position, {580.0f, optionsDetailWindowBottomY() - position.y}};
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
    const Vec2 position{
        panel.pos.x + 650.0f,
        panel.pos.y + 220.0f + OptionsContentYOffset,
    };
    return {position, {284.0f, optionsDetailWindowBottomY() - position.y}};
}

UiRect optionsFooterButtonRect(int index, int count, float width = 225.0f)
{
    return uiFooterActionGroupButtonRect(
        optionsPanelRect(),
        {width, ui::ButtonHeight},
        index,
        count);
}

UiRect operationSettingsDialogRect()
{
    return uiEnsureDecoratedWindowMinSize({{390.0f, 226.0f}, {500.0f, 270.0f}});
}

UiRect optionSettingsContentRect()
{
    return optionsLeftContentRect();
}

UiRect optionSettingsRowRect(int index)
{
    const UiRect content = optionSettingsContentRect();
    const float pitch = OptionsSettingsRowHeight + OptionsSettingsRowGap;
    return {{
        content.pos.x,
        content.pos.y + static_cast<float>(index) * pitch,
    }, {content.size.x, OptionsSettingsRowHeight}};
}

UiRect audioSettingsRowRect(int index)
{
    return optionSettingsRowRect(index);
}

UiRect audioSettingsSliderRect(int index)
{
    const UiRect row = audioSettingsRowRect(index);
    constexpr float SliderHeight = 20.0f;
    return {{
        row.pos.x + 202.0f,
        row.pos.y + (row.size.y - SliderHeight) * 0.5f,
    }, {280.0f, SliderHeight}};
}

UiRect videoSettingsRowRect(int index)
{
    return optionSettingsRowRect(index);
}

UiRect videoBrightnessSliderRect()
{
    const UiRect row = videoSettingsRowRect(VideoSettingsRowBrightness);
    constexpr float SliderHeight = 16.0f;
    return {{
        row.pos.x + 202.0f,
        row.pos.y + (row.size.y - SliderHeight) * 0.5f,
    }, {250.0f, SliderHeight}};
}

UiRect optionsHelpWindowRect()
{
    return optionsRightHelpWindowRect();
}

UiTabItem optionsPageTabItem(int index)
{
    const int clampedIndex = std::clamp(index, 0, OptionsPageCount - 1);
    return {
        OptionsPageLabels[clampedIndex],
        true,
        menuIconImageNumber(OptionsPageIcons[static_cast<std::size_t>(clampedIndex)]),
    };
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

int pauseMenuItemIconImageNumber(int index)
{
    switch (index) {
    case 0: return menuIconImageNumber(MenuIconImage::Status);
    case 1: return menuIconImageNumber(MenuIconImage::Backpack);
    case 2: return menuIconImageNumber(MenuIconImage::Ring0);
    case 3: return menuIconImageNumber(MenuIconImage::Options);
    case 4: return menuIconImageNumber(MenuIconImage::QuitGame);
    default: return 0;
    }
}

UiSelectableTableStyle operationSettingsTableStyle()
{
    UiSelectableTableStyle style;
    style.headerHeight = 34.0f;
    style.rowHeight = 48.0f;
    style.rowGap = 0.0f;
    style.columnGap = 0.0f;
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
    style.tabs.imageOutset = 16.0f;
    return style;
}

struct UiVerticalTabPressResult {
    int index = -1;
    bool wasSelected = false;
};

UiVerticalTabPressResult updateOptionsVerticalTab(
    UiTabsState& state,
    UiContext& ui,
    const UiVerticalTabItem* items,
    const UiRect* rects,
    int itemCount,
    int selectedIndex)
{
    if (items == nullptr || rects == nullptr || itemCount <= 0) {
        state = {};
        return {};
    }

    const int clampedSelection = std::clamp(selectedIndex, 0, itemCount - 1);
    state.focusedIndex = clampedSelection;
    UiTabsInput input{};
    const int pressedIndex = updateUiVerticalTabs(
        state,
        ui,
        input,
        clampedSelection,
        items,
        itemCount,
        rects,
        optionsVerticalTabStyle());
    return {pressedIndex, pressedIndex == clampedSelection};
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
    case 0: return "マスター音量";
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

UiSliderSpec audioSettingsSliderSpec()
{
    return {
        .minValue = 0.0f,
        .maxValue = 100.0f,
        .step = 5.0f,
    };
}

UiSliderStyle audioSettingsSliderStyle(int row)
{
    UiSliderStyle style;
    style.activeTrack = row == 0
        ? Color{132, 230, 250, 255}
        : (row == 1 ? Color{160, 206, 255, 255} : Color{255, 206, 132, 255});
    style.thumb = style.activeTrack;
    return style;
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
    case VideoSettingsRowWindowMode: return "表示モード";
    case VideoSettingsRowWindowSize: return "ウィンドウサイズ";
    case VideoSettingsRowBrightness: return "明るさ";
    case VideoSettingsRowInputIcons: return "操作アイコン表示";
    case VideoSettingsRowScreenShake: return "画面揺れ";
    case VideoSettingsRowLightweight: return "軽量化";
    case VideoSettingsRowVSync: return "VSync";
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
        return "なし";
    case ScreenShakeSetting::Low:
        return "弱め";
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

UiSliderSpec screenBrightnessSliderSpec()
{
    return {
        .minValue = MinScreenBrightness * 100.0f,
        .maxValue = MaxScreenBrightness * 100.0f,
        .step = 5.0f,
        .showReference = true,
        .referenceValue = 100.0f,
    };
}

UiSliderStyle screenBrightnessSliderStyle()
{
    UiSliderStyle style;
    style.activeTrack = {236, 220, 150, 255};
    style.thumb = style.activeTrack;
    return style;
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
    case VideoSettingsRowWindowMode:
        return windowModeDisplayName(settings.video.windowMode);
    case VideoSettingsRowWindowSize:
        return videoResolutionText(settings.video);
    case VideoSettingsRowBrightness:
        return screenBrightnessPercentText(settings.presentation.brightness);
    case VideoSettingsRowInputIcons:
        return inputIconSettingDisplayName(settings.presentation.inputIcons);
    case VideoSettingsRowScreenShake:
        return screenShakeDisplayName(settings.presentation.screenShake);
    case VideoSettingsRowLightweight:
        return settings.performance.lightweight ? "ON" : "OFF";
    case VideoSettingsRowVSync:
        return settings.video.vsync ? "ON" : "OFF";
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
    case VideoSettingsRowWindowMode:
        settings.video.windowMode = settings.video.windowMode == WindowMode::Windowed
            ? WindowMode::BorderlessFullscreen
            : WindowMode::Windowed;
        break;
    case VideoSettingsRowWindowSize:
        cycleVideoResolution(settings, delta);
        break;
    case VideoSettingsRowBrightness:
        adjustScreenBrightness(settings, delta);
        break;
    case VideoSettingsRowInputIcons:
        cycleInputIconSetting(settings, delta);
        break;
    case VideoSettingsRowScreenShake:
        cycleScreenShakeSetting(settings, delta);
        break;
    case VideoSettingsRowLightweight:
        settings.performance.lightweight = !settings.performance.lightweight;
        break;
    case VideoSettingsRowVSync:
        settings.video.vsync = !settings.video.vsync;
        break;
    default:
        break;
    }
}

const char* audioSettingsHelpText(int row)
{
    switch (row) {
    case 0:
        return "ゲーム全体の音量だよ　BGMとSEの両方にまとめて反映されるよ";
    case 1:
        return "BGMの音量";
    case 2:
        return "効果音の音量";
    default:
        return "";
    }
}

const char* videoSettingsHelpText(int row)
{
    switch (row) {
    case VideoSettingsRowWindowMode:
        return "ウィンドウ表示とフルスクリーン表示を切り替えるよ";
    case VideoSettingsRowWindowSize:
        return "ウィンドウ時の画面サイズだよ";
    case VideoSettingsRowBrightness:
        return "画面全体の明るさを調整できるよ";
    case VideoSettingsRowInputIcons:
        return "操作アイコンの表示形式をキーボード・ゲームパッドから選ぼう　「自動」では最後に使った入力機器に合わせて切り替えるよ";
    case VideoSettingsRowScreenShake:
        return "被弾やボス演出などの画面揺れ演出の強さだよ";
    case VideoSettingsRowLightweight:
        return "ONにすると光・暗幕・影・エフェクト・粒子数などを制限して、画面負荷を下げるよ";
    case VideoSettingsRowVSync:
        return "モニターの更新タイミングに描画を合わせる設定　ONにすると画面の裂けを抑えるけど、入力遅延やFPSに影響することがあるよ";
    default:
        return "";
    }
}

const char* operationSettingsActionHelpText(InputAction action)
{
    switch (action) {
    case InputAction::MoveLeft:
        return "プレイヤーの移動やカーソル移動";
    case InputAction::MoveRight:
        return "プレイヤーの移動やカーソル移動";
    case InputAction::MoveUp:
        return "プレイヤーの移動やカーソル移動";
    case InputAction::MoveDown:
        return "プレイヤーの移動やカーソル移動";
    case InputAction::Confirm:
        return "決定したり、調べたり、話しかけたり";
    case InputAction::Cancel:
        return "現在の画面を戻る、または操作をキャンセル";
    case InputAction::Pause:
        return "メニューを開く";
    case InputAction::OpenInventory:
        return "アイテム画面を直接開く";
    case InputAction::OpenOptions:
        return "タイトル画面でオプションを開く";
    case InputAction::OpenCredits:
        return "タイトル画面でクレジットを開く";
    case InputAction::ToggleFullscreen:
        return "ウィンドウ表示とフルスクリーン表示を切り替える";
    case InputAction::ThrowActiveRing:
        return "リング投げを発動する";
    case InputAction::OffsetRingCenter:
        return "マウス右ボタンのドラッグまたはゲームパッドの右スティックで、リング中心をずらす固定操作";
    case InputAction::ShiftRingLeft:
        return "リング中心を左へずらす";
    case InputAction::ShiftRingRight:
        return "リング中心を右へずらす";
    case InputAction::ShiftRingUp:
        return "リング中心を上へずらす";
    case InputAction::ShiftRingDown:
        return "リング中心を下へずらす";
    case InputAction::UseSelectedItem:
        return "ショートカットやアイテム画面で、選択中のアイテムを使う";
    case InputAction::DiscardSelectedItem:
        return "選択中のアイテムを捨てる（拠点では実行前に確認）";
    case InputAction::PutSelectedItemOnRing:
        return "選択中のアイテムをリングへ入れる";
    case InputAction::GrabOrPlaceItem:
        return "アイテム画面やリング画面で、アイテムをつかむ/置く";
    case InputAction::ArrangeItems:
        return "アイテムやリング上の配置を整列・並び替えする";
    case InputAction::SecondaryActionModifier:
        return "方向入力との組み合わせでアイテムショートカットを操作し、リング操作では全部外すなどのサブ操作を行う";
    case InputAction::CyclePrevious:
        return "リング・分類・対象・行き先・ページなどを前へ切り替える";
    case InputAction::CycleNext:
        return "リング・分類・対象・行き先・ページなどを次へ切り替える";
    case InputAction::ToggleProtection:
        return "アイテムの保護ON/OFFを切り替える";
    default:
        return "";
    }
}

std::string_view operationSettingsHelpTitle(const OperationSettingsActionRow& row)
{
    return row.label;
}

std::string_view operationSettingsHelpDescription(InputAction action)
{
    return operationSettingsActionHelpText(action);
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
    const float gapTotal = operationSettingsTableStyle().columnGap
        * static_cast<float>(OperationSettingsColumnCount - 1);
    const float contentWidth = std::max(1.0f, table.size.x - ScrollbarReserve);
    const float bindingWidth = std::max(1.0f, (contentWidth - ActionWidth - gapTotal) * 0.5f);
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
    case SDL_SCANCODE_LEFT:
        return "←";
    case SDL_SCANCODE_RIGHT:
        return "→";
    case SDL_SCANCODE_UP:
        return "↑";
    case SDL_SCANCODE_DOWN:
        return "↓";
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
    case InputBindingDevice::Keyboard: {
        std::string result;
        const auto appendModifier = [&](InputModifiers modifier, std::string_view label) {
            if (!inputModifiersContain(binding.modifiers, modifier)) {
                return;
            }
            if (!result.empty()) {
                result += '+';
            }
            result += "{key:";
            result += label;
            result += '}';
        };
        appendModifier(InputModifiers::Ctrl, "Ctrl");
        appendModifier(InputModifiers::Alt, "Alt");
        appendModifier(InputModifiers::Shift, "Shift");
        appendModifier(InputModifiers::Gui, "Gui");
        if (!result.empty()) {
            result += '+';
        }
        result += "{key:" + operationSettingsKeyboardGlyphLabel(binding.code) + "}";
        return result;
    }
    case InputBindingDevice::MouseButton:
        if (binding.code == SDL_BUTTON_RIGHT) {
            return "{mouse:right}";
        }
        if (binding.code == SDL_BUTTON_MIDDLE) {
            return "{mouse:middle}";
        }
        if (binding.code == SDL_BUTTON_LEFT) {
            return "{mouse:left}";
        }
        if (binding.code == SDL_BUTTON_X1) {
            return "{key:Mouse4}";
        }
        if (binding.code == SDL_BUTTON_X2) {
            return "{key:Mouse5}";
        }
        return "{key:Mouse" + std::to_string(binding.code) + "}";
    case InputBindingDevice::GamepadButton:
        return "{pad:" + gamepadButtonName(binding.code) + "}";
    case InputBindingDevice::GamepadAxis:
        return "{axis:" + gamepadAxisName(binding.code) + ":" + (binding.direction < 0 ? "-" : "+") + "}";
    }
    return "";
}

std::string operationSettingsBindingGlyphText(const InputBindingMap& bindings, InputAction action, int column)
{
    if (action == InputAction::OffsetRingCenter && column == OperationSettingsColumnGamepad) {
        return "{stick:R}";
    }

    std::vector<std::string> tokens;
    const std::vector<InputBinding>& actionBindings = bindings[inputActionIndex(action)];
    for (const InputBinding& binding : actionBindings) {
        if (!operationSettingsColumnMatchesBinding(column, binding)) {
            continue;
        }
        const std::string token = operationSettingsBindingGlyphToken(binding);
        if (token.empty()) {
            continue;
        }
        if (std::find(tokens.begin(), tokens.end(), token) == tokens.end()) {
            tokens.push_back(token);
        }
    }

    std::string text;
    for (const std::string& token : tokens) {
        if (!text.empty()) {
            text += " / ";
        }
        text += token;
    }
    return text.empty() ? "未設定" : text;
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
            return inputBindingSamePhysicalInput(binding, target);
        }),
        bindings.end());
}

bool hasOperationSettingsBinding(const std::vector<InputBinding>& bindings, const InputBinding& target)
{
    return std::any_of(bindings.begin(), bindings.end(), [&target](const InputBinding& binding) {
        return inputBindingSamePhysicalInput(binding, target);
    });
}

std::vector<InputAction> operationSettingsConflictingActions(
    const InputBindingMap& bindings,
    InputAction targetAction,
    const InputBinding& targetBinding)
{
    std::vector<InputAction> conflicts;
    for (int actionIndex = 0; actionIndex < InputActionCount; ++actionIndex) {
        const InputAction action = static_cast<InputAction>(actionIndex);
        if (!inputActionsConflict(targetAction, action)) {
            continue;
        }
        for (const InputBinding& binding : bindings[actionIndex]) {
            if (inputBindingSamePhysicalInput(binding, targetBinding)) {
                conflicts.push_back(action);
                break;
            }
        }
    }
    return conflicts;
}

std::string_view operationSettingsActionLabel(InputAction action)
{
    const auto row = std::find_if(
        std::begin(OperationSettingsActionRows),
        std::end(OperationSettingsActionRows),
        [action](const OperationSettingsActionRow& candidate) {
            return candidate.action == action;
        });
    return row != std::end(OperationSettingsActionRows)
        ? std::string_view(row->label)
        : inputActionName(action);
}

std::string operationSettingsConflictMessage(
    const std::vector<InputAction>& actions,
    std::string_view editVerb)
{
    std::string message = "この入力は同じ場面で使う別の操作に割り当て済みだよ\n既存の割当を外して";
    message += editVerb;
    message += "する？";
    if (!actions.empty()) {
        message += "\n競合する操作: ";
        for (std::size_t i = 0; i < actions.size(); ++i) {
            if (i > 0) {
                message += ", ";
            }
            message += operationSettingsActionLabel(actions[i]);
        }
    }
    return message;
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
        cell.pos.y + std::max(0.0f, (cell.size.y - textSize.y) * 0.5f) +
            OperationSettingsTextOffsetY,
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
        const float centeredPaddingX = std::max(
            paddingX,
            (cell.size.x - renderer.measureText(text, 2).x) * 0.5f);
        drawOperationSettingsCellText(renderer, cell, text, color, 2, centeredPaddingX);
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
        cell.pos.y + std::max(0.0f, (cell.size.y - size.y) * 0.5f) +
            OperationSettingsTextOffsetY,
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

struct OperationSettingsTableUpdateResult {
    UiSelectableTableResult selection{};
    int categoryDelta = 0;
};

OperationSettingsTableUpdateResult updateOperationSettingsTableClickSelection(
    UiSelectableTableState& state,
    UiContext& ui,
    const Input& input,
    UiRect rect,
    int rowCount,
    const UiSelectableTableColumn* columns,
    int columnCount,
    const UiSelectableTableStyle& style,
    int& hoveredRow,
    int& hoveredColumn)
{
    OperationSettingsTableUpdateResult result;
    hoveredRow = -1;
    hoveredColumn = -1;
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
    bool navigationFocusedTableCell = false;
    for (int row = 0; row < rowCount; ++row) {
        const UiRect rowRect = uiSelectableTableRowRect(layout, row, style);
        if (!uiScrollAreaRectVisible(layout.scroll, rowRect)) {
            continue;
        }
        for (int column = OperationSettingsColumnAction; column <= OperationSettingsColumnGamepad; ++column) {
            const UiRect cellRect = uiSelectableTableCellRect(layout, columns, columnCount, row, column, style);
            if (!columns[column].enabled) {
                continue;
            }
            if (ui.hovered(cellRect)) {
                hoveredRow = row;
                hoveredColumn = column;
            }
            if (ui.navigationFocused(cellRect)) {
                state.selectedRow = row;
                state.selectedColumn = std::max(OperationSettingsColumnKeyboardMouse, column);
                keyboardSelectionChanged = true;
                navigationFocusedTableCell = true;
            }
        }
    }
    if (!ui.navigationActive() && input.pressed(InputAction::MoveUp)) {
        state.selectedRow = (state.selectedRow + rowCount - 1) % rowCount;
        keyboardSelectionChanged = true;
    }
    if (!ui.navigationActive() && input.pressed(InputAction::MoveDown)) {
        state.selectedRow = (state.selectedRow + 1) % rowCount;
        keyboardSelectionChanged = true;
    }
    const int horizontalDelta =
        (input.pressed(InputAction::MoveRight) ? 1 : 0) -
        (input.pressed(InputAction::MoveLeft) ? 1 : 0);
    if (horizontalDelta != 0) {
        if (ui.navigationActive()) {
            const bool stayedAtBoundary = state.selectedColumn == previousColumn &&
                ((horizontalDelta < 0 && previousColumn == OperationSettingsColumnKeyboardMouse) ||
                    (horizontalDelta > 0 && previousColumn == OperationSettingsColumnGamepad));
            if (navigationFocusedTableCell && stayedAtBoundary) {
                result.categoryDelta = horizontalDelta;
            }
        } else if (!moveOperationSettingsTableColumn(state, horizontalDelta)) {
            result.categoryDelta = horizontalDelta;
        } else {
            keyboardSelectionChanged = true;
        }
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
                result.selection.pressedRow = row;
                if (column >= OperationSettingsColumnKeyboardMouse) {
                    if (sameRow && state.selectedColumn == column) {
                        result.selection.pressedColumn = column;
                    }
                    state.selectedColumn = column;
                } else {
                    result.selection.pressedColumn = OperationSettingsColumnAction;
                }
            }
        }
    }

    result.selection.selectionChanged = state.selectedRow != previousRow || state.selectedColumn != previousColumn;
    ui.emitCursorMoveIfChanged(
        previousRow * columnCount + previousColumn,
        state.selectedRow * columnCount + state.selectedColumn);
    if (keyboardSelectionChanged || result.selection.pressedRow >= 0) {
        keepUiSelectableTableCellVisible(rect, state.selectedRow, rowCount, state.scrollOffset, style);
    }
    return result;
}

float importantDungeonNoticeBlockHeight(int count)
{
    return static_cast<float>(count) * ImportantDungeonNoticeRowHeight +
        static_cast<float>(std::max(0, count - 1)) * ImportantDungeonNoticeGap;
}

UiRect importantDungeonNoticeBlockRect(float screenWidth, float screenHeight, int count)
{
    const float height = importantDungeonNoticeBlockHeight(count);
    const float bottomY = screenHeight * ImportantDungeonNoticeBottomYRatio;
    return {{
        (screenWidth - ImportantDungeonNoticeWidth) * 0.5f,
        bottomY - height,
    }, {
        ImportantDungeonNoticeWidth,
        height,
    }};
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
        closeUiCommandMenu(ringPresetMenu_);
        ringPresetMenuAction_ = RingPresetMenuAction::None;
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
    const auto performRingPresetAction = [this, &ui, &clearRingTransientUi](
                                             RingPresetMenuAction action,
                                             int presetIndex) {
        clearRingTransientUi();
        closeUiCommandMenu(ringPresetMenu_);
        ringPresetMenuAction_ = RingPresetMenuAction::None;

        bool succeeded = false;
        switch (action) {
        case RingPresetMenuAction::Apply:
            succeeded = applyRingPreset(presetIndex);
            break;
        case RingPresetMenuAction::Register:
            succeeded = registerRingPresetShortcut(presetIndex);
            break;
        case RingPresetMenuAction::None:
            break;
        }
        ui.emitActionResult(succeeded);
    };
    const int presetSlotCount = unlockedRingPresetSlotCount();
    const auto activateRingPresetButton = [this,
                                           &ui,
                                           &clearRingTransientUi,
                                           &performRingPresetAction,
                                           presetSlotCount](RingPresetMenuAction action, UiRect buttonRect) {
        if (presetSlotCount <= 0) {
            ui.rejectAction();
            return;
        }
        if (presetSlotCount == 1) {
            performRingPresetAction(action, 0);
            return;
        }

        clearRingTransientUi();
        ringPresetMenuAction_ = action;
        openUiCommandMenu(
            ringPresetMenu_,
            uiCommandMenuAnchorForSlot(buttonRect),
            ringPanelRect(),
            presetSlotCount,
            RingPresetMenuItems.data(),
            188.0f,
            2);
        ringStatus_.clear();
    };

    if (ringPresetMenu_.visible) {
        const RingPresetMenuAction action = ringPresetMenuAction_;
        const int presetSelection = updateUiCommandMenu(
            ringPresetMenu_,
            ui,
            input,
            RingPresetMenuItems.data(),
            presetSlotCount);
        if (presetSelection >= 0) {
            performRingPresetAction(action, presetSelection);
        } else if (!ringPresetMenu_.visible) {
            ringPresetMenuAction_ = RingPresetMenuAction::None;
        }
        ui.block(ringPanelRect());
        return;
    }

    auto& items = spellRing_.items();
    const auto beginRingItemMove = [this, &items](int itemIndex) {
        if (itemIndex < 0 || itemIndex >= static_cast<int>(items.size())) {
            ringStatus_ = "アイテム未選択";
            return false;
        }

        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        ringEmptyPressActive_ = false;
        ringSlotSelection_ = itemIndex;
        ringDetailShowsRing_ = false;
        ringItemMoveModeActive_ = true;
        ringItemMoveIndex_ = itemIndex;
        ringItemMoveOriginalAngle_ = items[static_cast<std::size_t>(itemIndex)].localAngle;
        ringStatus_ = "移動モード";
        return true;
    };
    const auto openRingItemCommandMenu = [this, &items](int itemIndex, Vec2 anchor) {
        if (itemIndex < 0 || itemIndex >= static_cast<int>(items.size())) {
            ringStatus_ = "アイテム未選択";
            return false;
        }

        ringSlotSelection_ = itemIndex;
        ringDetailShowsRing_ = false;
        ringCommandItemIndex_ = itemIndex;
        ringCommandPlaceActive_ = false;
        ringEmptyPressActive_ = false;
        const SpellRingItem& item = items[static_cast<std::size_t>(itemIndex)];
        const RingCommandMenuItems menuItems = ringCommandItems(
            false,
            &item,
            objectCatalog_,
            false,
            !item.objectId.empty());
        openUiCommandMenu(
            ringCommandMenu_,
            anchor,
            ringPanelRect(),
            static_cast<int>(menuItems.items.size()),
            menuItems.items.data(),
            180.0f,
            2);
        ringStatus_.clear();
        return true;
    };
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
            ui.emitActionResult(discarded, UiSoundEvent::ItemUse);
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
                ui.rejectAction();
            }
        } else if (ringCommandItemIndex_ >= 0) {
            ringSlotSelection_ = ringCommandItemIndex_;
            ringDetailShowsRing_ = false;
            bool commandSucceeded = false;
            switch (action) {
            case RingCommandAction::Place:
                break;
            case RingCommandAction::Move:
                commandSucceeded = beginRingItemMove(ringSlotSelection_);
                if (!commandSucceeded) {
                    ui.rejectAction();
                }
                break;
            case RingCommandAction::Remove:
                commandSucceeded = removeRingItemToInventory(items, ringSlotSelection_, inventory_, objectCatalog_, ringStatus_);
                ui.emitActionResult(commandSucceeded, UiSoundEvent::ItemMove);
                break;
            case RingCommandAction::ToggleProtection:
                commandSucceeded = toggleRingItemProtection(items, ringSlotSelection_, ringStatus_);
                ui.emitActionResult(commandSucceeded);
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
                    ui.emitActionResult(commandSucceeded, UiSoundEvent::ItemUse);
                }
                break;
            }
            }
        } else {
            ui.rejectAction();
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
            ui.rejectAction();
            ui.block(ringPanelRect());
            return;
        }
        ringPlaceSelection_ = std::clamp(ringPlaceSelection_, 0, std::max(0, slotCount - 1));
        if (!ringPlaceSlotEnabled(inventory_, spellRing_, ringPlaceSelection_, ringPlaceTargetAngle_)) {
            ringPlaceSelection_ = firstSlot;
        }

        if (uiCancelRequested(ringCancelState_, input, ui, ringPlaceWindowRect())) {
            ringPlaceModeActive_ = false;
            ringStatus_ = "配置をキャンセルしたよ";
            ui.block(ringPanelRect());
            return;
        }

        const int previousRingPlaceSelection = ringPlaceSelection_;
        const auto tryPlaceSelection = [&]() {
            if (!ringPlaceSlotEnabled(inventory_, spellRing_, ringPlaceSelection_, ringPlaceTargetAngle_)) {
                ui.rejectAction();
                ringStatus_ = "このアイテムは配置できないよ";
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
                ringStatus_ = "リングに配置したよ";
                ui.emitSound(UiSoundEvent::Equip);
            } else {
                ui.rejectAction();
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
            if (enabled && ui.selectionFocused(rect)) {
                ringPlaceSelection_ = i;
            }
            if (ui.pressed(rect)) {
                if (enabled) {
                    ringPlaceSelection_ = i;
                    ui.emitCursorMoveIfChanged(previousRingPlaceSelection, ringPlaceSelection_);
                    tryPlaceSelection();
                } else if (inventory_.hasScreenItemAt(i)) {
                    ui.rejectAction();
                    ringStatus_ = "このアイテムは配置できないよ";
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
            ringStatus_ = "移動をキャンセルしたよ";
            ui.emitSound(UiSoundEvent::Cancel);
            ui.block(ringPanelRect());
            return;
        }

        if (input.confirmPressed() || input.useItemPressed()) {
            ringItemMoveModeActive_ = false;
            ringItemMoveIndex_ = -1;
            ringStatus_ = "位置を確定したよ";
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
                ui.rejectAction();
                ringStatus_ = "その位置には移動できないよ";
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
        ringTabs[static_cast<std::size_t>(i)] = {
            ringTabLabels[static_cast<std::size_t>(i)],
            true,
            ringDisplayIconImageNumber(i),
        };
        ringTabRects[static_cast<std::size_t>(i)] = ringTabRect(i, ringCount);
    }
    UiTabsInput ringTabsInput = makeUiCycleTabsInput(input, ringCount);
    if (ringTabsInput.focusDelta != 0) {
        ringTabsInput.directFocusIndex = spellRing_.activeRingIndex();
    }
    ringTabsInput.commit = ringTabsInput.commit ||
        (!ui.navigationActive() && (input.confirmPressed() || input.useItemPressed()));
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

    if (presetSlotCount > 0 && ui.pressed(ringPresetButtonRect(false))) {
        activateRingPresetButton(RingPresetMenuAction::Apply, ringPresetButtonRect(false));
        ui.block(ringPanelRect());
        return;
    }
    if (presetSlotCount > 0 && ui.pressed(ringPresetButtonRect(true))) {
        activateRingPresetButton(RingPresetMenuAction::Register, ringPresetButtonRect(true));
        ui.block(ringPanelRect());
        return;
    }
    if (input.arrangeItemsPressed() || ui.pressed(ringArrangeButtonRect())) {
        closeUiCommandMenu(ringCommandMenu_);
        ringCommandItemIndex_ = -1;
        ringCommandPlaceActive_ = false;
        if (spellRing_.arrangeItemsEvenlyForRing(spellRing_.activeRingIndex(), balance_)) {
            ui.emitSound(UiSoundEvent::ItemMove);
            ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()) - 1);
            ringStatus_ = "等間隔に整列したよ";
        } else {
            ui.rejectAction();
            ringStatus_ = "アイテム未配置だよ";
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
        ui.emitActionResult(
            removeAllRingItemsToInventory(items, ringSlotSelection_, inventory_, objectCatalog_, ringStatus_),
            UiSoundEvent::ItemMove);
        ui.block(ringPanelRect());
        return;
    }

    if ((ringDragPending_ || ringDragActive_) && uiCancelRequested(ringCancelState_, input, ui, ringPanelRect())) {
        ringDragPending_ = false;
        ringDragActive_ = false;
        ringDragItemIndex_ = -1;
        ringStatus_ = "ドラッグをキャンセルしたよ";
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
                ringStatus_ = snapAngle ? "近い空き位置へ補正したよ" : "近くに空きがないため元の位置へ戻したよ";
            } else {
                openRingItemCommandMenu(ringDragItemIndex_, input.mouseScreen());
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
        mode_ = ringReturnToPause_ ? ScreenMode::PauseMenu : pauseReturnMode_;
        ringReturnToPause_ = false;
        pausePage_ = PauseMenuPage::Main;
        return;
    }

    const int previousRingSlotSelection = ringSlotSelection_;
    std::vector<RingUiItemInteractionTarget> interactionTargets;
    interactionTargets.reserve(items.size());
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const SpellRingItem& item = items[static_cast<std::size_t>(i)];
        const UiRect rect = ringItemUiRect(item, spellRing_, balance_, i, static_cast<int>(items.size()));
        interactionTargets.push_back({
            {i, ringItemUiCenter(item, spellRing_, balance_, i, static_cast<int>(items.size()))},
            rect,
            rect,
        });
    }
    const int interactionIndex = frontmostRingUiItemInteractionIndex(interactionTargets, ui);
    if (interactionIndex >= 0) {
        const SpellRingItem& interactionItem = items[static_cast<std::size_t>(interactionIndex)];
        const UiRect rect = ringItemUiRect(
            interactionItem,
            spellRing_,
            balance_,
            interactionIndex,
            static_cast<int>(items.size()));
        if (ui.selectionFocused(rect)) {
            ringSlotSelection_ = interactionIndex;
            ringDetailShowsRing_ = false;
        }
        if (ui.pressed(rect)) {
            closeUiCommandMenu(ringCommandMenu_);
            ringCommandItemIndex_ = -1;
            ringCommandPlaceActive_ = false;
            ringEmptyPressActive_ = false;
            ringSlotSelection_ = interactionIndex;
            ringDetailShowsRing_ = false;
            if (ui.navigationActive()) {
                openRingItemCommandMenu(interactionIndex, uiCommandMenuAnchorForSlot(rect));
                ui.emitCursorMoveIfChanged(previousRingSlotSelection, ringSlotSelection_);
                return;
            }
            ringDragPending_ = true;
            ringDragActive_ = false;
            ringDragItemIndex_ = interactionIndex;
            ringDragOriginalAngle_ = items[static_cast<std::size_t>(interactionIndex)].localAngle;
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
    if (!ui.navigationActive() && !items.empty() && lengthSquared(selectionDirection) > 0.0001f) {
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

    if (input.grabOrPlacePressed()) {
        ui.emitActionResult(beginRingItemMove(ringSlotSelection_), UiSoundEvent::ItemMove);
        return;
    }

    if (input.addRingPressed()) {
        ringDetailShowsRing_ = false;
        if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ui.emitActionResult(
                removeRingItemToInventory(items, ringSlotSelection_, inventory_, objectCatalog_, ringStatus_),
                UiSoundEvent::ItemMove);
        } else {
            ui.rejectAction();
            ringStatus_ = "アイテム未選択";
        }
        return;
    }

    if (input.pressed(InputAction::ToggleProtection)) {
        ringDetailShowsRing_ = false;
        if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ui.emitActionResult(toggleRingItemProtection(items, ringSlotSelection_, ringStatus_));
        } else {
            ui.rejectAction();
            ringStatus_ = "アイテム未選択";
        }
        return;
    }

    if (input.useItemPressed() || input.confirmPressed()) {
        if (ringSlotSelection_ < static_cast<int>(items.size())) {
            const UiRect selectedItemRect = ringItemUiRect(
                items[static_cast<std::size_t>(ringSlotSelection_)],
                spellRing_,
                balance_,
                ringSlotSelection_,
                static_cast<int>(items.size()));
            openRingItemCommandMenu(
                ringSlotSelection_,
                uiCommandMenuAnchorForSlot(selectedItemRect));
        } else {
            ui.rejectAction();
            ringStatus_ = "アイテム未選択";
        }
    }
}

void Game::prepareOptionsMenu()
{
    optionsPage_ = OptionsPageVideo;
    audioSettingsTabs_ = {};
    videoSettingsTabs_ = {};
    optionsSliderUiState() = {};
    operationSettingsHoveredRow_ = -1;
    operationSettingsHoveredColumn_ = -1;
    closeUiCommandMenu(operationSettingsCommandMenu_);
    operationSettingsCapture_.cancel();
    operationSettingsConflictConfirm_ = {};
    operationSettingsResetAllConfirm_ = {};
    operationSettingsReadOnlyDialog_ = {};
    clearOperationSettingsPendingEdit();
    optionsStatus_.clear();
    loadOptionsSettings();
}

void Game::openOptionsMenu()
{
    pausePage_ = PauseMenuPage::Options;
    prepareOptionsMenu();
}

bool Game::optionsMenuActive() const
{
    return (mode_ == ScreenMode::PauseMenu && pausePage_ == PauseMenuPage::Options) ||
        (mode_ == ScreenMode::Title && titleMenuPage_ == TitleMenuPage::Options);
}

bool Game::operationSettingsModalVisible() const
{
    return optionsPage_ == OptionsPageOperation &&
        (operationSettingsCommandMenu_.visible ||
            operationSettingsCapture_.active() ||
            operationSettingsConflictConfirm_.open ||
            operationSettingsResetAllConfirm_.open ||
            operationSettingsReadOnlyDialog_.open);
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

void Game::beginOperationSettingsBindingCapture(OperationSettingsBindingEditMode mode)
{
    optionsStatus_.clear();
    operationSettingsPendingEditMode_ = mode;
    operationSettingsCapture_.begin(
        operationSettingsPendingAction_,
        operationSettingsCaptureGroupForColumn(operationSettingsPendingColumn_));
}

bool Game::handleOperationSettingsCaptureResult(
    InputRemapCaptureResult result,
    InputAction action,
    int column,
    const InputBinding& binding)
{
    if (result == InputRemapCaptureResult::None) {
        return false;
    }
    if (result == InputRemapCaptureResult::Captured) {
        queueOperationSettingsBinding(action, column, binding);
    } else if (result == InputRemapCaptureResult::Cancelled) {
        clearOperationSettingsPendingEdit();
    }
    return true;
}

void Game::clearOperationSettingsPendingEdit()
{
    operationSettingsPendingAction_ = InputAction::Count;
    operationSettingsPendingColumn_ = OperationSettingsColumnKeyboardMouse;
    operationSettingsPendingBinding_ = {};
    operationSettingsPendingEditMode_ = OperationSettingsBindingEditMode::Replace;
    operationSettingsConflictActions_.clear();
}

void Game::queueOperationSettingsBinding(InputAction action, int column, const InputBinding& binding)
{
    if (operationSettingsPendingEditMode_ == OperationSettingsBindingEditMode::Append &&
        hasOperationSettingsBinding(operationSettingsBindings_[inputActionIndex(action)], binding)) {
        optionsStatus_ = "この入力はすでに割り当て済みだよ";
        clearOperationSettingsPendingEdit();
        return;
    }

    operationSettingsPendingAction_ = action;
    operationSettingsPendingColumn_ = column;
    operationSettingsPendingBinding_ = binding;
    operationSettingsConflictActions_ = operationSettingsConflictingActions(operationSettingsBindings_, action, binding);
    if (!operationSettingsConflictActions_.empty()) {
        const std::string editVerb =
            operationSettingsPendingEditMode_ == OperationSettingsBindingEditMode::Append
            ? "追加"
            : "変更";
        openUiConfirmDialog(
            operationSettingsConflictConfirm_,
            "操作割当の確認",
            operationSettingsConflictMessage(operationSettingsConflictActions_, editVerb),
            editVerb,
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
    // 開発用ショートカットはプレイヤー操作の割当を予約しない。
    // 同じ入力が割り当てられた場合は、確認を増やさずプレイヤー操作を優先する。
    for (int actionIndex = 0; actionIndex < InputActionCount; ++actionIndex) {
        const InputAction existingAction = static_cast<InputAction>(actionIndex);
        if (inputActionIsDeveloperOnly(existingAction)) {
            removeOperationSettingsBinding(candidate[actionIndex], binding);
        }
    }
    auto& target = candidate[inputActionIndex(action)];
    if (operationSettingsPendingEditMode_ == OperationSettingsBindingEditMode::Replace) {
        removeOperationSettingsColumnBindings(target, column);
    } else if (hasOperationSettingsBinding(target, binding)) {
        optionsStatus_ = "この入力はすでに割り当て済みだよ";
        clearOperationSettingsPendingEdit();
        return;
    }
    target.push_back(binding);
    candidate = sanitizeInputBindings(candidate);

    operationSettingsBindings_ = candidate;
    optionsSettings_.input.bindings = operationSettingsBindings_;
    applyOptionsSettings("");
    clearOperationSettingsPendingEdit();
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
        return;
    }
    candidate = sanitizeInputBindings(candidate);
    operationSettingsBindings_ = candidate;
    optionsSettings_.input.bindings = operationSettingsBindings_;
    applyOptionsSettings("");
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
}

void Game::resetOperationSettingsAll()
{
    operationSettingsBindings_ = defaultInputBindings();
    optionsSettings_.input.bindings = operationSettingsBindings_;
    applyOptionsSettings("");
}

bool Game::handleOperationSettingsEvent(const SDL_Event& event)
{
    if (!optionsMenuActive() ||
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
        handleOperationSettingsCaptureResult(
            InputRemapCaptureResult::Cancelled,
            InputAction::Count,
            operationSettingsPendingColumn_,
            {});
        return true;
    }

    const InputAction action = operationSettingsCapture_.action();
    const int column = operationSettingsPendingColumn_;
    InputBinding binding{};
    const InputRemapCaptureResult result = operationSettingsCapture_.handleEvent(event, binding);
    handleOperationSettingsCaptureResult(result, action, column, binding);
    return true;
}

void Game::updateOperationSettings(const Input& input, UiContext& ui)
{
    const int previousHoveredCell =
        operationSettingsHoveredRow_ >= 0 && operationSettingsHoveredColumn_ >= 0
        ? operationSettingsHoveredRow_ * OperationSettingsColumnCount + operationSettingsHoveredColumn_
        : -1;
    operationSettingsHoveredRow_ = -1;
    operationSettingsHoveredColumn_ = -1;
    if (!optionsSettingsLoaded_ || !operationSettingsLoaded_) {
        loadOptionsSettings();
    }

    const UiRect panel = optionsPanelRect();
    const UiRect table = operationSettingsTableRect();
    const UiRect dialog = operationSettingsDialogRect();

    if (operationSettingsCapture_.active()) {
        const InputAction action = operationSettingsCapture_.action();
        if (handleOperationSettingsCaptureResult(
                operationSettingsCapture_.update(),
                action,
                operationSettingsPendingColumn_,
                {})) {
            ui.block(panel);
            return;
        }
    }

    if (operationSettingsConflictConfirm_.open) {
        const UiConfirmDialogResult result = updateUiConfirmDialog(operationSettingsConflictConfirm_, ui, input, dialog);
        if (result == UiConfirmDialogResult::Confirmed) {
            applyOperationSettingsBinding(
                operationSettingsPendingAction_,
                operationSettingsPendingColumn_,
                operationSettingsPendingBinding_,
                true);
        } else if (result == UiConfirmDialogResult::Cancelled) {
            clearOperationSettingsPendingEdit();
        }
        ui.block(panel);
        return;
    }

    if (operationSettingsReadOnlyDialog_.open) {
        updateUiResultDialog(operationSettingsReadOnlyDialog_, ui, input, dialog);
        ui.block(panel);
        return;
    }

    if (operationSettingsResetAllConfirm_.open) {
        const UiConfirmDialogResult result = updateUiConfirmDialog(operationSettingsResetAllConfirm_, ui, input, dialog);
        if (result == UiConfirmDialogResult::Confirmed) {
            resetOperationSettingsAll();
        }
        ui.block(panel);
        return;
    }

    if (operationSettingsCapture_.active()) {
        ui.block(panel);
        return;
    }

    const int commandSelection = updateUiCommandMenu(
        operationSettingsCommandMenu_,
        ui,
        input,
        OperationSettingsCommandItems.data(),
        static_cast<int>(OperationSettingsCommandItems.size()));
    if (commandSelection >= 0) {
        switch (static_cast<OperationSettingsCommand>(commandSelection)) {
        case OperationSettingsCommand::Replace:
            beginOperationSettingsBindingCapture(OperationSettingsBindingEditMode::Replace);
            break;
        case OperationSettingsCommand::Append:
            beginOperationSettingsBindingCapture(OperationSettingsBindingEditMode::Append);
            break;
        case OperationSettingsCommand::Clear:
            clearOperationSettingsBinding(operationSettingsPendingAction_, operationSettingsPendingColumn_);
            clearOperationSettingsPendingEdit();
            break;
        case OperationSettingsCommand::Reset:
            resetOperationSettingsAction(operationSettingsPendingAction_);
            clearOperationSettingsPendingEdit();
            break;
        }
    }
    if (operationSettingsCommandMenu_.visible) {
        ui.block(panel);
        return;
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
    const OperationSettingsTableUpdateResult tableUpdate = updateOperationSettingsTableClickSelection(
        operationSettingsTable_,
        ui,
        input,
        table,
        static_cast<int>(rows.size()),
        columns.data(),
        static_cast<int>(columns.size()),
        operationSettingsTableStyle(),
        operationSettingsHoveredRow_,
        operationSettingsHoveredColumn_);
    const UiSelectableTableResult& tableResult = tableUpdate.selection;
    if (tableUpdate.categoryDelta != 0) {
        operationSettingsCategory_ =
            (operationSettingsCategory_ + tableUpdate.categoryDelta + OperationSettingsCategoryCount) %
            OperationSettingsCategoryCount;
        operationSettingsTable_.selectedRow = 0;
        operationSettingsTable_.scrollOffset = 0.0f;
        if (ui.navigationActive()) {
            const UiSelectableTableLayout firstRowLayout = makeUiSelectableTableLayout(
                table,
                1,
                operationSettingsTable_.scrollOffset,
                operationSettingsTableStyle());
            ui.setNavigationFocus(uiSelectableTableCellRect(
                firstRowLayout,
                columns.data(),
                static_cast<int>(columns.size()),
                operationSettingsTable_.selectedRow,
                operationSettingsTable_.selectedColumn,
                operationSettingsTableStyle()));
        }
        ui.emitSound(UiSoundEvent::TabSwitch);
        ui.block(panel);
        return;
    }
    const int hoveredCell =
        operationSettingsHoveredRow_ >= 0 && operationSettingsHoveredColumn_ >= 0
        ? operationSettingsHoveredRow_ * OperationSettingsColumnCount + operationSettingsHoveredColumn_
        : -1;
    if (hoveredCell >= 0 && hoveredCell != previousHoveredCell) {
        ui.emitSound(UiSoundEvent::CursorMove);
    }

    const bool tableCommitted =
        tableResult.pressedColumn >= OperationSettingsColumnKeyboardMouse ||
        (!ui.navigationActive() && (input.confirmPressed() || input.useItemPressed()));
    if (tableCommitted && !rows.empty()) {
        const int row = std::clamp(operationSettingsTable_.selectedRow, 0, static_cast<int>(rows.size()) - 1);
        const int column = std::clamp(
            operationSettingsTable_.selectedColumn,
            OperationSettingsColumnKeyboardMouse,
            OperationSettingsColumnGamepad);
        const InputAction action = rows[static_cast<std::size_t>(row)].action;
        if (!inputActionCanBeRemapped(action)) {
            ui.rejectAction();
            openUiResultDialog(
                operationSettingsReadOnlyDialog_,
                "",
                {"この項目は変更できません"});
            ui.block(panel);
            return;
        }
        operationSettingsPendingAction_ = action;
        operationSettingsPendingColumn_ = column;
        const UiRect selectedCell = uiSelectableTableCellRect(
            makeUiSelectableTableLayout(
                table,
                static_cast<int>(rows.size()),
                operationSettingsTable_.scrollOffset,
                operationSettingsTableStyle()),
            columns.data(),
            static_cast<int>(columns.size()),
            row,
            column,
            operationSettingsTableStyle());
        openUiCommandMenu(
            operationSettingsCommandMenu_,
            uiCommandMenuAnchorForSlot(selectedCell),
            panel,
            static_cast<int>(OperationSettingsCommandItems.size()),
            OperationSettingsCommandItems.data(),
            160.0f);
        ui.block(panel);
        return;
    }

    constexpr int ButtonCount = 1;
    for (int i = 0; i < ButtonCount; ++i) {
        const UiRect button = optionsFooterButtonRect(i, ButtonCount);
        if (ui.pressed(button)) {
            ui.emitSound(UiSoundEvent::Confirm);
            openUiConfirmDialog(
                operationSettingsResetAllConfirm_,
                "初期化の確認",
                "すべての操作割当を初期状態に戻す？",
                "初期化",
                "戻る",
                1);
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
    const auto values = audioSettingsRowValueTexts(optionsSettings_);
    const auto tabItems = audioSettingsTabItems(values);
    const auto tabRects = audioSettingsRowRects();
    bool settingsRowFocused = !ui.navigationActive();
    for (int row = 0; row < AudioSettingsRowCount; ++row) {
        if (ui.navigationFocused(tabRects[static_cast<std::size_t>(row)])) {
            audioSettingsSelection_ = row;
            settingsRowFocused = true;
        }
    }

    if (!ui.navigationActive() && input.pressed(InputAction::MoveUp)) {
        audioSettingsSelection_ = (audioSettingsSelection_ + AudioSettingsRowCount - 1) % AudioSettingsRowCount;
    }
    if (!ui.navigationActive() && input.pressed(InputAction::MoveDown)) {
        audioSettingsSelection_ = (audioSettingsSelection_ + 1) % AudioSettingsRowCount;
    }

    const auto applyAudioRow = [&](int row, float value) {
        setAudioSettingsRowValue(optionsSettings_, row, value);
        applyOptionsSettings(std::string(audioSettingsRowLabel(row)) + " 音量 " + volumePercentText(audioSettingsRowValue(optionsSettings_, row)));
    };

    bool keyboardAdjusted = false;
    if (settingsRowFocused && input.pressed(InputAction::MoveLeft)) {
        applyAudioRow(audioSettingsSelection_, audioSettingsRowValue(optionsSettings_, audioSettingsSelection_) - 0.05f);
        ui.emitSound(UiSoundEvent::Confirm);
        keyboardAdjusted = true;
    }
    if (settingsRowFocused && input.pressed(InputAction::MoveRight)) {
        applyAudioRow(audioSettingsSelection_, audioSettingsRowValue(optionsSettings_, audioSettingsSelection_) + 0.05f);
        ui.emitSound(UiSoundEvent::Confirm);
        keyboardAdjusted = true;
    }

    int interactingSlider = -1;
    OptionsSliderUiState& sliderState = optionsSliderUiState();
    for (int row = 0; row < AudioSettingsRowCount; ++row) {
        UiSliderState& state = sliderState.audio[static_cast<std::size_t>(row)];
        const UiSliderResult result = updateUiSlider(
            ui,
            input,
            audioSettingsSliderRect(row),
            audioSettingsRowValue(optionsSettings_, row) * 100.0f,
            audioSettingsSliderSpec(),
            state);
        if (result.changed) {
            applyAudioRow(row, result.value / 100.0f);
        }
        if (result.interacting) {
            audioSettingsSelection_ = row;
            interactingSlider = row;
        }
    }
    if (keyboardAdjusted) {
        sliderState.audio[static_cast<std::size_t>(audioSettingsSelection_)].showValue();
    }
    if (interactingSlider >= 0) {
        ui.emitCursorMoveIfChanged(previousSelection, audioSettingsSelection_);
        return;
    }

    const UiVerticalTabPressResult pressedTab = updateOptionsVerticalTab(
        audioSettingsTabs_,
        ui,
        tabItems.data(),
        tabRects.data(),
        static_cast<int>(tabItems.size()),
        audioSettingsSelection_);
    if (pressedTab.index >= 0) {
        audioSettingsSelection_ = pressedTab.index;
        ui.emitCursorMoveIfChanged(previousSelection, audioSettingsSelection_);
        return;
    }

    ui.emitCursorMoveIfChanged(previousSelection, audioSettingsSelection_);
    constexpr int ButtonCount = 1;
    for (int i = 0; i < ButtonCount; ++i) {
        if (ui.pressed(optionsFooterButtonRect(i, ButtonCount))) {
            ui.emitSound(UiSoundEvent::Confirm);
            optionsSettings_.audio = AudioSettings{};
            applyOptionsSettings("");
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
    const auto values = videoSettingsRowValueTexts(optionsSettings_);
    const auto tabItems = videoSettingsTabItems(values);
    const auto tabRects = videoSettingsRowRects();
    bool settingsRowFocused = !ui.navigationActive();
    for (int row = 0; row < VideoSettingsRowCount; ++row) {
        if (ui.navigationFocused(tabRects[static_cast<std::size_t>(row)])) {
            videoSettingsSelection_ = row;
            settingsRowFocused = true;
        }
    }

    if (!ui.navigationActive() && input.pressed(InputAction::MoveUp)) {
        videoSettingsSelection_ = (videoSettingsSelection_ + VideoSettingsRowCount - 1) % VideoSettingsRowCount;
    }
    if (!ui.navigationActive() && input.pressed(InputAction::MoveDown)) {
        videoSettingsSelection_ = (videoSettingsSelection_ + 1) % VideoSettingsRowCount;
    }

    const auto applyVideoRow = [&](int row, int delta) {
        cycleVideoSetting(optionsSettings_, row, delta);
        applyOptionsSettings(std::string(videoSettingsRowLabel(row)) + " " + videoSettingsRowValueText(optionsSettings_, row));
    };
    const auto applyBrightnessSlider = [&](float percent) {
        setScreenBrightnessValue(optionsSettings_, percent / 100.0f);
        applyOptionsSettings(
            std::string(videoSettingsRowLabel(VideoSettingsRowBrightness)) + " " +
            videoSettingsRowValueText(optionsSettings_, VideoSettingsRowBrightness));
    };

    bool keyboardAdjustedBrightness = false;
    if (settingsRowFocused && input.pressed(InputAction::MoveLeft)) {
        applyVideoRow(videoSettingsSelection_, -1);
        ui.emitSound(UiSoundEvent::Confirm);
        keyboardAdjustedBrightness = videoSettingsSelection_ == VideoSettingsRowBrightness;
    }
    if (settingsRowFocused &&
        (input.pressed(InputAction::MoveRight) ||
            (!ui.navigationActive() && (input.confirmPressed() || input.useItemPressed())))) {
        applyVideoRow(videoSettingsSelection_, 1);
        ui.emitSound(UiSoundEvent::Confirm);
        keyboardAdjustedBrightness = videoSettingsSelection_ == VideoSettingsRowBrightness;
    }

    const UiRect brightnessSlider = videoBrightnessSliderRect();
    OptionsSliderUiState& sliderState = optionsSliderUiState();
    const UiSliderResult brightnessResult = updateUiSlider(
        ui,
        input,
        brightnessSlider,
        clampedScreenBrightness(optionsSettings_.presentation.brightness) * 100.0f,
        screenBrightnessSliderSpec(),
        sliderState.brightness);
    if (brightnessResult.changed) {
        applyBrightnessSlider(brightnessResult.value);
    }
    if (keyboardAdjustedBrightness) {
        sliderState.brightness.showValue();
    }
    if (brightnessResult.interacting) {
        videoSettingsSelection_ = VideoSettingsRowBrightness;
        ui.emitCursorMoveIfChanged(previousSelection, videoSettingsSelection_);
        return;
    }

    const UiVerticalTabPressResult pressedTab = updateOptionsVerticalTab(
        videoSettingsTabs_,
        ui,
        tabItems.data(),
        tabRects.data(),
        static_cast<int>(tabItems.size()),
        videoSettingsSelection_);
    if (pressedTab.index >= 0) {
        videoSettingsSelection_ = pressedTab.index;
        if (pressedTab.wasSelected &&
            pressedTab.index != VideoSettingsRowBrightness) {
            applyVideoRow(pressedTab.index, 1);
            ui.emitSound(UiSoundEvent::Confirm);
        }
        ui.emitCursorMoveIfChanged(previousSelection, videoSettingsSelection_);
        return;
    }

    ui.emitCursorMoveIfChanged(previousSelection, videoSettingsSelection_);
    constexpr int ButtonCount = 1;
    for (int i = 0; i < ButtonCount; ++i) {
        if (ui.pressed(optionsFooterButtonRect(i, ButtonCount))) {
            ui.emitSound(UiSoundEvent::Confirm);
            optionsSettings_.video = VideoSettings{};
            optionsSettings_.performance = PerformanceSettings{};
            optionsSettings_.presentation = PresentationSettings{};
            applyOptionsSettings("");
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

    const auto dismissSliderValueBubbles = [] {
        optionsSliderUiState() = {};
    };
    const bool operationModalOpen = operationSettingsModalVisible();
    if (!operationModalOpen) {
        const auto tabItems = optionsPageTabItems();
        const auto tabRects = optionsPageTabRects();
        const UiTabsInput pageTabsInput = makeUiCycleTabsInput(input, OptionsPageCount);
        const int selectedTab = updateUiTabs(
            optionsTabs_,
            ui,
            pageTabsInput,
            optionsPage_,
            tabItems.data(),
            static_cast<int>(tabItems.size()),
            tabRects.data());
        if (selectedTab >= 0 && selectedTab != optionsPage_) {
            optionsPage_ = selectedTab;
            dismissSliderValueBubbles();
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
        operationSettingsModalVisible();
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
            handleApplicationQuitRequested();
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
        if (ui.selectionFocused(rect)) {
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

void Game::updateGameOverScreen(const Input& input, UiContext& ui, float dt)
{
    if (updateDeathResultPrelude(dt, ui)) {
        return;
    }

    const int previousSelection = gameOverSelection_;
    if (input.pressed(InputAction::MoveUp)) {
        gameOverSelection_ = (gameOverSelection_ + GameOverItemCount - 1) % GameOverItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        gameOverSelection_ = (gameOverSelection_ + 1) % GameOverItemCount;
    }

    for (int i = 0; i < GameOverItemCount; ++i) {
        const UiRect rect = gameOverItemRect(i);
        if (ui.selectionFocused(rect)) {
            gameOverSelection_ = i;
        }
        if (ui.pressed(rect)) {
            gameOverSelection_ = i;
            ui.emitSound(UiSoundEvent::Confirm);
            if (i == 0) {
                requestDeathResultExitTransition(ScreenTransitionTarget::GameOverRetry);
            } else {
                requestDeathResultExitTransition(ScreenTransitionTarget::GameOverReturnToBase);
            }
            return;
        }
    }
    ui.emitCursorMoveIfChanged(previousSelection, gameOverSelection_);

    if (input.confirmPressed() || input.useItemPressed()) {
        ui.emitSound(UiSoundEvent::Confirm);
        if (gameOverSelection_ == 0) {
            requestDeathResultExitTransition(ScreenTransitionTarget::GameOverRetry);
        } else {
            requestDeathResultExitTransition(ScreenTransitionTarget::GameOverReturnToBase);
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
        if (ui.selectionFocused(rect)) {
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

void Game::updateAstralResultScreen(const Input& input, UiContext& ui, float dt)
{
    if (updateDeathResultPrelude(dt, ui)) {
        return;
    }

    const UiRect button = stageClearItemRect(0);
    if (ui.selectionFocused(button)) {
        astralResultSelection_ = 0;
    }
    if (ui.pressed(button) || input.confirmPressed() || input.useItemPressed()) {
        ui.emitSound(UiSoundEvent::Confirm);
        if (astralResult_.result == AstralRunResult::Died) {
            requestDeathResultExitTransition(ScreenTransitionTarget::AstralDeathReturnToBase);
            return;
        }
        returnToBaseAfterAstralResult();
        return;
    }

    ui.block(stageClearPanelRect());
}

float Game::measureItemAcquisitionNoticeContentHeight(
    Renderer& renderer,
    const AcquisitionNotice& notice) const
{
    const UiRect detailLayout = itemAcquisitionNoticeDetailMeasureRect(camera_.width());
    const UiRect detailContent = uiSubPanelContentRect(detailLayout);
    const bool objectNotice = notice.kind == AcquisitionNoticeKind::Object;
    const ObjectDefinition* object = objectNotice
        ? objectCatalog_.registry.findById(notice.objectId)
        : nullptr;

    std::string title;
    std::string category;
    std::string description;
    int rarity = 0;
    std::optional<InventoryUiItemStats> itemStats;
    if (object != nullptr) {
        title = itemAcquisitionObjectDisplayName(*object, notice.amount);
        category = object->category;
        description = object->description.empty() ? "-" : object->description;
        rarity = object->rarity;
        if (const InventoryObjectInstance* instance = inventory_.objectInstanceById(notice.instanceId)) {
            itemStats = inventoryUiStatsFromInstance(instance->instance);
        }
    } else if (notice.kind == AcquisitionNoticeKind::Material) {
        title = std::string(materialTypeDisplayName(notice.materialType)) +
            " x" + std::to_string(std::max(1, notice.amount));
        category = "素材";
        description = "魔女からのお礼";
    } else {
        title = std::to_string(std::max(1, notice.amount)) + "G";
        category = "お金";
        description = "魔女からのお礼";
    }

    float height = measureInventoryUiDetailHeaderHeight(
        renderer,
        detailLayout,
        title,
        category,
        rarity);
    height += renderer.measureWrappedText(description, detailContent.size.x, 2).y + 8.0f;
    if (object != nullptr) {
        height += measureInventoryUiItemEffectSections(
            renderer,
            detailLayout,
            *object,
            objectCatalog_,
            encyclopedia_,
            itemStats,
            unlockedRingCount());
        height += measureInventoryUiWeightLineHeight(renderer, detailLayout, *object, itemStats);
    }
    return height;
}

void Game::updateItemAcquisitionNotice(
    const Input& input,
    UiContext& ui,
    Renderer& renderer,
    float dt)
{
    if (itemAcquisitionNotices_.empty()) {
        return;
    }

    AcquisitionNotice& notice = itemAcquisitionNotices_.front();
    if (!notice.jinglePlayed) {
        notice.jinglePlayed = true;
        if (notice.jingleOnShow) {
            playItemAcquisitionNoticeJingle();
        }
    }
    const float animationSpeed = notice.presentation == AcquisitionNoticePresentation::Standard ? 2.0f : 1.0f;
    const float animationStep = std::max(0.0f, dt) * animationSpeed / ui::WindowAnimationSeconds;
    if (notice.animationPhase == AcquisitionNoticeAnimationPhase::Opening) {
        notice.animationProgress = std::min(1.0f, notice.animationProgress + animationStep);
        if (notice.animationProgress >= 1.0f) {
            notice.animationPhase = AcquisitionNoticeAnimationPhase::Visible;
        }
    } else if (notice.animationPhase == AcquisitionNoticeAnimationPhase::Closing) {
        notice.animationProgress = std::max(0.0f, notice.animationProgress - animationStep);
        if (notice.animationProgress <= 0.0f) {
            itemAcquisitionNotices_.pop_front();
        }
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        return;
    }

    const UiRect panel = itemAcquisitionNoticeRect(
        camera_.width(),
        camera_.height(),
        measureItemAcquisitionNoticeContentHeight(renderer, notice),
        !notice.statusText.empty());
    const UiRect okButton = itemAcquisitionOkButtonRect(panel);
    const bool objectNotice = notice.kind == AcquisitionNoticeKind::Object;
    const bool dungeonActionsEnabled = mode_ != ScreenMode::Base;
    const bool instanceProtectable =
        objectNotice &&
        notice.protectable &&
        inventory_.objectInstanceProtectionEnabled(notice.instanceId).has_value();
    const ObjectDefinition* noticeObject = objectNotice
        ? objectCatalog_.registry.findById(notice.objectId)
        : nullptr;
    const ItemAcquisitionPrimaryAction primaryAction = dungeonActionsEnabled
        ? itemAcquisitionPrimaryAction(
            inventory_,
            spellRing_,
            noticeObject,
            notice.objectId,
            notice.instanceId)
        : ItemAcquisitionPrimaryAction::None;
    const bool discardable =
        dungeonActionsEnabled &&
        noticeObject != nullptr &&
        !isImportantItem(*noticeObject) &&
        (notice.instanceId.empty() ||
            (!inventory_.objectInstanceProtectionEnabled(notice.instanceId).value_or(false) &&
                !inventory_.isStaffEquipped(notice.instanceId)));

    if (notice.animationPhase != AcquisitionNoticeAnimationPhase::Visible) {
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        return;
    }

    const int unlockedRingCount = unlockedRingHudCount();
    if (dungeonActionsEnabled && unlockedRingCount > 1 && input.cycleDelta() != 0) {
        const int previousRingIndex = spellRing_.activeRingIndex();
        switchActiveRing(input.cycleDelta());
        if (spellRing_.activeRingIndex() != previousRingIndex) {
            ui.emitSound(UiSoundEvent::TabSwitch);
        }
    }
    for (int ringIndex = 0; dungeonActionsEnabled && ringIndex < unlockedRingCount; ++ringIndex) {
        if (!ui.pressed(ringStatusHudRect(ringIndex, unlockedRingCount))) {
            continue;
        }
        if (ringIndex != spellRing_.activeRingIndex()) {
            ui.emitSound(UiSoundEvent::TabSwitch);
            switchActiveRing(ringIndex - spellRing_.activeRingIndex());
        }
        return;
    }

    if (objectNotice && input.pressed(InputAction::ToggleProtection)) {
        if (instanceProtectable) {
            const bool protectedNow = inventory_.objectInstanceProtectionEnabled(notice.instanceId).value_or(false);
            const bool changed = inventory_.setObjectInstanceProtection(notice.instanceId, !protectedNow);
            ui.emitActionResult(changed);
            if (changed) {
                notice.statusText.clear();
            }
        } else {
            ui.rejectAction();
            notice.statusText = "個体アイテムのみ保護できます";
        }
    }

    if (primaryAction != ItemAcquisitionPrimaryAction::None && input.useItemPressed()) {
        std::string status;
        std::vector<EffectDiscoveryEvent> discoveries;
        const bool actionSucceeded = primaryAction == ItemAcquisitionPrimaryAction::EquipStaff
            ? inventory_.equipStaffObject(
                notice.objectId,
                notice.instanceId,
                spellRing_,
                &status)
            : (notice.instanceId.empty()
                ? inventory_.useObjectStackById(
                    notice.objectId,
                    player_,
                    effectDispatcher_,
                    &magic_,
                    &discoveries,
                    &encyclopedia_,
                    &status)
                : inventory_.useObjectInstanceById(
                    notice.instanceId,
                    player_,
                    effectDispatcher_,
                    &magic_,
                    &discoveries,
                    &encyclopedia_,
                    &status));
        ui.emitActionResult(
            actionSucceeded,
            primaryAction == ItemAcquisitionPrimaryAction::EquipStaff
                ? UiSoundEvent::Equip
                : UiSoundEvent::ItemUse);
        notice.statusText = status;
        if (actionSucceeded) {
            applyEffectDiscoveries(discoveries);
            closeItemAcquisitionNotice();
            return;
        }
    }

    if (dungeonActionsEnabled && objectNotice && input.addRingPressed()) {
        SpellRingAddResult result{};
        std::string status;
        if (inventory_.addObjectToRing(notice.objectId, notice.instanceId, spellRing_, &result, &status)) {
            ui.emitSound(UiSoundEvent::Equip);
            const UiRect imagePanel = itemAcquisitionNoticeImagePanelRect(
                panel,
                !notice.statusText.empty());
            spawnRingEquipFx(RingEquipFxRequest{
                .sourceScreen = itemAcquisitionNoticeImageCenter(imagePanel),
                .ringIndex = result.ringIndex,
                .itemIndex = result.itemIndex,
                .localAngle = result.localAngle,
                .objectId = result.objectId,
                .instanceId = result.instanceId,
            });
            closeItemAcquisitionNotice();
            return;
        }
        ui.rejectAction();
        notice.statusText = status.empty() ? "リングへ配置できないよ" : status;
    }

    if (dungeonActionsEnabled && objectNotice && input.discardItemPressed()) {
        if (!discardable) {
            ui.rejectAction();
            notice.statusText = "このアイテムは捨てられないよ";
            ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
            return;
        }
        std::string status;
        const bool discarded = notice.instanceId.empty()
            ? inventory_.discardObjectStackById(notice.objectId, true, &status)
            : inventory_.discardObjectInstanceById(notice.instanceId, true, &status);
        ui.emitActionResult(discarded, UiSoundEvent::ItemUse);
        notice.statusText = status;
        if (discarded) {
            spawnInventoryDiscardRequests(inventory_.consumeDiscardRequests());
            closeItemAcquisitionNotice();
            return;
        }
    }

    const bool okButtonPressed = ui.pressed(okButton) &&
        !input.useItemPressed() &&
        !input.addRingPressed() &&
        !input.discardItemPressed() &&
        !input.pressed(InputAction::ToggleProtection);
    if (okButtonPressed ||
        input.confirmPressed() ||
        input.pressed(InputAction::Cancel) ||
        input.pressed(InputAction::Pause)) {
        ui.emitSound(UiSoundEvent::Confirm);
        closeItemAcquisitionNotice();
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
        screenHeight - RingStatusHudBottomMargin - totalHeight - RingStatusHudUpOffset - DungeonHudUpShift);
    return {{
        RingStatusHudLeftMargin + static_cast<float>(ringIndex) * RingStatusHudIndentStep,
        startY + static_cast<float>(ringIndex) * (RingStatusHudHeight + RingStatusHudGap),
    }, {RingStatusHudWidth, RingStatusHudHeight}};
}

bool Game::updateRingStatusHud(UiContext& ui, float dt)
{
    if (introTutorialActive()) {
        return false;
    }

    const float safeDt = std::max(0.0f, dt);
    const int unlockedRingCount = unlockedRingHudCount();
    bool handledPointer = false;
    for (int ringIndex = 0; ringIndex < unlockedRingCount; ++ringIndex) {
        const UiRect panel = ringStatusHudRect(ringIndex, unlockedRingCount);
        const Vec2 pointer = ui.mouse();
        if (!ui.pressed(panel)) {
            continue;
        }

        handledPointer = true;
        const bool active = ringIndex == spellRing_.activeRingIndex();
        if (ringStatusHudRightWindowContains(panel, pointer)) {
            if (!active) {
                ui.emitSound(UiSoundEvent::TabSwitch);
                switchActiveRing(ringIndex - spellRing_.activeRingIndex());
                break;
            }
            pauseReturnMode_ = ScreenMode::Playing;
            inventoryReturnToPause_ = false;
            ringReturnToPause_ = false;
            ui.emitSound(UiSoundEvent::MenuOpen);
            openRingScreen();
            break;
        }

        if (!active) {
            ui.emitSound(UiSoundEvent::TabSwitch);
            switchActiveRing(ringIndex - spellRing_.activeRingIndex());
        } else if (ringStatusHudLeftCircleContains(panel, pointer)) {
            if (!spellRing_.tryThrowActiveRing(player_, balance_)) {
                ui.rejectAction();
            }
        }
        break;
    }

    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        RingStatusHudAnimation& animation = ringStatusHudAnimations_[static_cast<std::size_t>(ringIndex)];
        if (ringIndex >= unlockedRingCount) {
            animation = {};
            continue;
        }

        const float cooldownRatio = spellRing_.cooldownRatioForRing(ringIndex, balance_);
        if (!animation.initialized) {
            animation.previousCooldownRatio = cooldownRatio;
            animation.visibility = ringIndex == spellRing_.activeRingIndex() || cooldownRatio > 0.001f ? 1.0f : 0.0f;
            animation.initialized = true;
        } else if (animation.previousCooldownRatio > 0.001f && cooldownRatio <= 0.001f) {
            animation.readyHoldTimer = RingStatusHudReadyHoldSeconds;
            animation.pulseTimer = RingStatusHudPulseSeconds;
            playAudioSe(RingStatusHudChargeReadySe);
        }

        animation.previousCooldownRatio = cooldownRatio;
        animation.readyHoldTimer = std::max(0.0f, animation.readyHoldTimer - safeDt);
        animation.pulseTimer = std::max(0.0f, animation.pulseTimer - safeDt);

        const bool active = ringIndex == spellRing_.activeRingIndex();
        const bool shouldShow = active || cooldownRatio > 0.001f || animation.readyHoldTimer > 0.0f;
        const float targetVisibility = shouldShow ? 1.0f : 0.0f;
        if constexpr (RingStatusHudFadeSeconds <= 0.001f) {
            animation.visibility = targetVisibility;
        } else {
            const float step = safeDt / RingStatusHudFadeSeconds;
            if (animation.visibility < targetVisibility) {
                animation.visibility = std::min(targetVisibility, animation.visibility + step);
            } else {
                animation.visibility = std::max(targetVisibility, animation.visibility - step);
            }
        }
    }

    return handledPointer;
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

    const int textScale = 2;
    const Vec2 textMeasure = renderer.measureText("0", textScale);
    const auto centeredEntryY = [](float entryHeight) {
        return TopInfoBarY + (TopInfoBarHeight - entryHeight) * 0.5f;
    };
    const float textY = centeredEntryY(textMeasure.y);
    InlineItemTextStyle moneyStyle;
    moneyStyle.text = {246, 230, 174, 255};
    moneyStyle.scale = textScale;
    moneyStyle.iconTextGap = TopInfoBarIconTextGap;
    moneyStyle.iconScale = TopInfoBarIconSize / std::max(1.0f, textMeasure.y);
    const float moneyPulse = moneyGainFx_.hudPulseStrength();
    const auto pulseChannel = [moneyPulse](unsigned char base, unsigned char highlight) {
        return static_cast<unsigned char>(std::clamp(
            std::lround(lerp(static_cast<float>(base), static_cast<float>(highlight), moneyPulse)),
            0L,
            255L));
    };
    moneyStyle.text = {
        pulseChannel(moneyStyle.text.r, 255),
        pulseChannel(moneyStyle.text.g, 252),
        pulseChannel(moneyStyle.text.b, 222),
        255,
    };

    InlineItemTextStyle materialStyle = moneyStyle;
    materialStyle.text = {232, 236, 244, 255};

    const std::string moneyEntry =
        inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) +
        std::to_string(moneyGainFx_.displayedMoney(money_)) +
        "G";
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
        dungeonInfoEntry = astralRunActive()
            ? roguelikeDepthTopInfoEntry(
                dungeonLayout_,
                playerTilePosition,
                astralRun_.currentDepthMeters,
                astralRun_.nextHoleDepthMeters,
                astralRun_.completionDepthMeters,
                dungeonRouteDeviation_.offMainRoute)
            : dungeonDepthTopInfoEntry(
                dungeonLayout_,
                playerTilePosition,
                dungeonRouteDeviation_.offMainRoute);

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
        drawInlineItemText(
            renderer,
            objectCatalog_,
            {dungeonInfoX, centeredEntryY(dungeonInfoSize.y)},
            dungeonInfoEntry,
            dungeonInfoStyle);
    }

    float x = rightX;
    drawInlineItemText(
        renderer,
        objectCatalog_,
        {x, centeredEntryY(moneyEntrySize.y)},
        moneyEntry,
        moneyStyle);
    x += moneyEntrySize.x;

    for (std::size_t i = 0; i < Materials.size(); ++i) {
        x += TopInfoBarGroupGap;
        drawInlineItemText(
            renderer,
            objectCatalog_,
            {x, centeredEntryY(materialEntrySizes[i].y)},
            materialEntries[i],
            materialStyle);
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
    const KamishibaiPage* page = endingPlayer_.currentPage();
    if (endingKamishibaiKind_ != EndingKind::AstralClear || page == nullptr || page->effectName != "astral_constellation") {
        return;
    }

    renderer.setScreenSpace();
    const float width = static_cast<float>(camera_.width());
    const float height = static_cast<float>(camera_.height());
    const float bandHeight = std::clamp(height * 0.28f, 150.0f, 230.0f);
    const UiRect starArea{{0.0f, 0.0f}, {width, std::max(1.0f, height - bandHeight)}};
    renderer.pushClipRect(starArea.pos, starArea.size);
    renderAstralEchoStarfield(renderer, starArea, true);
    renderer.popClipRect();
}

void Game::renderTitleScreen(Renderer& renderer) const
{
    openingRenderer_.renderTitleScreen(renderer, openingTitleImagePath(openingPages_), camera_.width(), camera_.height());
    renderer.setScreenSpace();
    renderer.drawOutlinedText(
        titleVersionTextPos(),
        GameVersionText,
        {224, 230, 244, 218},
        {0, 0, 0, 150},
        2,
        2);

    if (titleMenuPage_ == TitleMenuPage::Options) {
        UiCancelControlScope cancelScope(titleCancelState_);
        const std::string help = optionsMenuHelpText(optionsPage_);
        UiWindowScope window(
            renderer,
            "title.options",
            optionsPanelRect(),
            "オプション",
            help,
            UiWindowOptions{true, true});
        renderOptionsMenu(renderer);
        return;
    }

    if (titleMenuPage_ == TitleMenuPage::Credits) {
        UiCancelControlScope cancelScope(titleCancelState_);
        const UiRect panel = titleCreditsPanelRect();
        UiScrollAreaStyle scrollStyle;
        const UiScrollAreaLayout layout = makeTitleCreditsLayout(
            renderer,
            titleCreditsText_,
            titleCreditsScrollOffset_,
            scrollStyle);
        titleCreditsContentHeight_ = layout.contentHeight;
        const std::string help = buildInputHelpText({
            {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "戻る"},
        });
        UiWindowScope window(
            renderer,
            "title.credits",
            panel,
            "クレジット",
            help,
            UiWindowOptions{true, true});

        renderer.pushClipRect(layout.viewport.pos, layout.viewport.size);
        renderer.drawWrappedText(
            layout.content.pos + Vec2{0.0f, 12.0f - layout.scrollOffset},
            titleCreditsText_,
            std::max(1.0f, layout.content.size.x),
            ui::Text,
            2);
        renderer.popClipRect();
        drawUiScrollAreaScrollbar(renderer, layout, scrollStyle);
        return;
    }

    drawUiButton(renderer, titleTopButtonRect(0), "オプション", false, uiActionButtonStyle());
    drawUiButton(renderer, titleTopButtonRect(1), "クレジット", false, uiActionButtonStyle());

    const std::string prompt = inputHelpUsesGamepad()
        ? "Press {act:Confirm} to Start"
        : "Press {act:Confirm} / Click to Start";
    InputHelpStyle promptStyle;
    promptStyle.text = {255, 255, 255, 245};
    promptStyle.outline = {0, 0, 0, 190};
    promptStyle.outlineEnabled = true;
    promptStyle.outlinePx = 4;
    promptStyle.scale = 3;
    promptStyle.iconHeight = 36.0f;
    const UiRect promptRect = titleStartPromptRect();
    registerUiNavigationTarget(promptRect, UiNavigationRole::Control, true);
    UiControlMotionScope promptMotion(
        renderer,
        promptRect,
        UiControlMotion::HoverAndPress);
    const Vec2 promptSize = measureInputHelpText(renderer, prompt, promptStyle);
    drawInputHelpText(
        renderer,
        {
            promptRect.pos.x + (promptRect.size.x - promptSize.x) * 0.5f,
            promptRect.pos.y + (promptRect.size.y - promptSize.y) * 0.5f,
        },
        prompt,
        promptStyle);

    drawUiBottomInputHelp(
        renderer,
        "title.control-help",
        {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
        buildInputHelpText({
            {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, "ゲーム開始"},
            {InputHelpGroup::Other, {InputAction::OpenOptions}, "オプション"},
            {InputHelpGroup::Other, {InputAction::OpenCredits}, "クレジット"},
        }));
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

void Game::renderDevBuildNotice(Renderer& renderer) const
{
    if (devBuildNoticeState_ == DevBuildNoticeState::None) {
        return;
    }

    constexpr std::string_view ReadyMessage = "ビルド完了　F5で更新を適用";
    constexpr std::string_view FailedMessage = "ビルド失敗　コンソールを確認";
    constexpr float HorizontalPadding = 24.0f;
    constexpr float VerticalPadding = 10.0f;
    constexpr float TopMargin = 18.0f;
    constexpr float LineGap = 8.0f;
    constexpr float PreferredTextWidth = 820.0f;
    constexpr int TextScale = 2;

    renderer.setScreenSpace();
    const bool failed = devBuildNoticeState_ == DevBuildNoticeState::Failed;
    const std::string_view message = failed ? FailedMessage : ReadyMessage;
    std::string summaryText;
    for (const std::string& summary : devBuildNoticeChangeSummaries_) {
        if (summary.empty()) {
            continue;
        }
        if (!summaryText.empty()) {
            summaryText += '\n';
        }
        summaryText += "・";
        summaryText += summary;
    }
    const float textMaxWidth = std::max(
        1.0f,
        std::min(
            PreferredTextWidth,
            static_cast<float>(camera_.width()) - TopMargin * 2.0f - HorizontalPadding * 2.0f));
    const Vec2 headingSize = renderer.measureText(message, TextScale);
    const Vec2 summarySize = summaryText.empty()
        ? Vec2{}
        : renderer.measureWrappedText(summaryText, textMaxWidth, TextScale);
    const float contentWidth = std::max(headingSize.x, summarySize.x);
    const float contentHeight = headingSize.y +
        (summaryText.empty() ? 0.0f : LineGap + summarySize.y);
    const Vec2 panelSize{
        contentWidth + HorizontalPadding * 2.0f,
        contentHeight + VerticalPadding * 2.0f,
    };
    const Vec2 panelPos{
        (static_cast<float>(camera_.width()) - panelSize.x) * 0.5f,
        TopMargin,
    };
    const Color panelColor = failed
        ? Color{42, 12, 16, 236}
        : Color{12, 15, 22, 232};
    const Color borderColor = failed
        ? Color{255, 112, 124, 240}
        : Color{255, 220, 112, 230};
    const Color textColor = failed
        ? Color{255, 210, 214, 255}
        : Color{255, 241, 188, 255};
    renderer.fillRect(panelPos, panelSize, panelColor);
    renderer.drawRect(panelPos, panelSize, borderColor);
    renderer.drawText(
        panelPos + Vec2{HorizontalPadding, VerticalPadding},
        message,
        textColor,
        TextScale);
    if (!summaryText.empty()) {
        renderer.drawWrappedText(
            panelPos + Vec2{HorizontalPadding, VerticalPadding + headingSize.y + LineGap},
            summaryText,
            textMaxWidth,
            textColor,
            TextScale);
    }
}

void Game::renderFinalScreenOverlays(Renderer& renderer)
{
    renderScreenTransitionOverlay(renderer);
    renderDevBuildNotice(renderer);
    renderBossDefeatFlash(renderer);
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
    const UiRect panel = dungeonStatusHudRect(screenWidth, screenHeight);

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
    const ImageHandle ringHudImage = renderer.acquireImage(RingStatusHudImagePath, TextureFilter::Linear);
    Vec2 ringHudImageSize{};
    const bool canDrawImage =
        ringHudImage.valid() &&
        renderer.getImageSize(ringHudImage, ringHudImageSize) &&
        ringHudImageSize.x >= RingStatusHudFrameSize.x * static_cast<float>(RingStatusHudImageColumns) &&
        ringHudImageSize.y >= RingStatusHudFrameSize.y * static_cast<float>(RingStatusHudImageRows);

    char buffer[96];
    for (int ringIndex = 0; ringIndex < unlockedRingCount; ++ringIndex) {
        const UiRect panel = ringStatusHudRect(ringIndex, unlockedRingCount);
        UiControlMotionScope panelMotion(
            renderer,
            panel,
            UiControlMotion::HoverAndPress);
        const bool active = ringIndex == spellRing_.activeRingIndex();

        if (canDrawImage) {
            renderer.drawImageHorizontalSlices(
                ringHudImage,
                ringStatusHudSpriteSource(ringIndex, active),
                panel.pos,
                panel.size,
                RingStatusHudSliceLeftWidth,
                RingStatusHudSliceRightWidth);
        } else {
            drawUiSubPanel(renderer, panel);
        }

        const auto& items = spellRing_.itemsForRing(ringIndex);
        const RingStatusHudAnimation& cooldownAnimation = ringStatusHudAnimations_[static_cast<std::size_t>(ringIndex)];
        const float cooldownAlpha = clamp(cooldownAnimation.visibility, 0.0f, 1.0f);
        drawRingStatusHudCooldownPulse(renderer, panel.pos, cooldownAnimation.pulseTimer, cooldownAlpha);
        drawRingStatusHudCooldown(renderer, panel.pos, spellRing_.cooldownRatioForRing(ringIndex, balance_), cooldownAlpha);

        const Vec2 textPos = panel.pos + RingStatusHudTextPos;
        renderer.drawText(textPos, ringDisplayName(ringIndex, unlockedRingCount), active ? RingStatusHudNameColor : RingStatusHudInactiveNameColor, 2);

        std::snprintf(buffer, sizeof(buffer), "所持数 %d/%d", static_cast<int>(items.size()), spellRing_.maxItemCountForRing(ringIndex));
        renderer.drawText(
            textPos + Vec2{0.0f, RingStatusHudLineGap},
            buffer,
            RingStatusHudItemColor,
            2);

        const float weight = spellRing_.totalEquippedWeightForRing(ringIndex);
        const float weightLimit = spellRing_.maxEquippedWeightForRing(ringIndex);
        std::snprintf(
            buffer,
            sizeof(buffer),
            "重量 %.1f/%.1fkg",
            weight,
            weightLimit);
        renderer.drawText(
            textPos + Vec2{0.0f, RingStatusHudLineGap * 2.0f},
            buffer,
            RingStatusHudWeightColor,
            2);
        drawRingWeightGauge(
            renderer,
            {panel.pos + RingStatusHudWeightGaugeRect.pos, RingStatusHudWeightGaugeRect.size},
            weight,
            weightLimit);
    }
}

void Game::renderItemAcquisitionNotice(Renderer& renderer, float animationSeconds) const
{
    if (itemAcquisitionNotices_.empty()) {
        return;
    }

    const AcquisitionNotice& notice = itemAcquisitionNotices_.front();
    const ObjectDefinition* object = objectCatalog_.registry.findById(notice.objectId);
    const bool objectNotice = notice.kind == AcquisitionNoticeKind::Object;
    if (objectNotice && object == nullptr) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect screen{{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}};
    const bool statusVisible = !notice.statusText.empty();
    const UiRect panel = itemAcquisitionNoticeRect(
        camera_.width(),
        camera_.height(),
        measureItemAcquisitionNoticeContentHeight(renderer, notice),
        statusVisible);
    UiModalNavigationScope navigationScope(panel);
    const std::optional<bool> protection =
        objectNotice && notice.protectable ? inventory_.objectInstanceProtectionEnabled(notice.instanceId) : std::nullopt;
    std::optional<InventoryUiItemStats> itemStats;
    if (objectNotice) {
        if (const InventoryObjectInstance* instance = inventory_.objectInstanceById(notice.instanceId)) {
            itemStats = inventoryUiStatsFromInstance(instance->instance);
        }
    }
    const bool canProtect = protection.has_value();
    const bool dungeonActionsEnabled = mode_ != ScreenMode::Base;
    const ItemAcquisitionPrimaryAction primaryAction = dungeonActionsEnabled
        ? itemAcquisitionPrimaryAction(
            inventory_,
            spellRing_,
            object,
            notice.objectId,
            notice.instanceId)
        : ItemAcquisitionPrimaryAction::None;
    const bool discardable =
        dungeonActionsEnabled &&
        objectNotice &&
        object != nullptr &&
        !isImportantItem(*object) &&
        (notice.instanceId.empty() ||
            (!protection.value_or(false) && !inventory_.isStaffEquipped(notice.instanceId)));
    std::vector<InputHelpEntry> helpEntries{
        {InputHelpGroup::Primary, {InputAction::Confirm}, "OK"},
    };
    if (dungeonActionsEnabled && unlockedRingHudCount() > 1) {
        helpEntries.push_back({
            InputHelpGroup::Cycle,
            {InputAction::CyclePrevious, InputAction::CycleNext},
            "リング切替",
        });
    }
    if (objectNotice) {
        if (dungeonActionsEnabled) {
            if (primaryAction != ItemAcquisitionPrimaryAction::None) {
                helpEntries.push_back({
                    InputHelpGroup::Other,
                    {InputAction::UseSelectedItem},
                    primaryAction == ItemAcquisitionPrimaryAction::EquipStaff ? "装備する" : "使用する",
                });
            }
            helpEntries.push_back({InputHelpGroup::Other, {InputAction::PutSelectedItemOnRing}, "リングへ"});
            helpEntries.push_back({
                InputHelpGroup::Other,
                {InputAction::DiscardSelectedItem},
                discardable ? "捨てる" : "捨てる不可",
            });
        }
        helpEntries.push_back({
            InputHelpGroup::Other,
            {InputAction::ToggleProtection},
            canProtect ? "保護ON/OFF" : "保護不可",
        });
    }
    const std::string helpText = buildInputHelpText(helpEntries);

    const bool baseUiActive = mode_ == ScreenMode::Base && (
        baseStorageActive_ ||
        baseSellActive_ ||
        baseUpgradeActive_ ||
        baseProcessingUiMode_ != ProcessingUiMode::Closed ||
        baseRingWorkshopActive_ ||
        baseBookshelfActive_ ||
        baseMiningStartChoiceActive_ ||
        baseResultDialog_.open ||
        baseQuantityDialog_.open ||
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
    const float noticeAnimation = smootherStep(std::clamp(notice.animationProgress, 0.0f, 1.0f));
    renderer.pushScreenTransform(
        panel.pos + panel.size * 0.5f,
        lerp(0.9f, 1.0f, noticeAnimation),
        noticeAnimation);
    {
        UiWindowScope window(
            renderer,
            "item_acquisition",
            panel,
            notice.title.empty() ? "アイテムを入手した" : notice.title,
            helpText,
            UiWindowOptions{false, false});

    const UiRect body = itemAcquisitionNoticeBodyRect(panel, statusVisible);
    const UiRect imagePanel = itemAcquisitionNoticeImagePanelRect(panel, statusVisible);
    const UiRect detailLayout = itemAcquisitionNoticeDetailLayoutRect(panel, statusVisible);
    drawUiSubPanel(renderer, imagePanel);

    const Vec2 imageCenter = itemAcquisitionNoticeImageCenter(imagePanel);
    if (objectNotice) {
        ObjectImageDrawOptions imageOptions;
        imageOptions.allowUpscale = true;
        if (!drawItemImage(renderer, *object, imageCenter, {100.0f, 100.0f}, imageOptions)) {
            renderer.fillCircle(imageCenter, 38.0f, inventoryUiObjectColor(*object));
            renderer.drawCircle(imageCenter, 42.0f, {255, 255, 255, 210});
        }
        if (protection.value_or(false)) {
            constexpr Vec2 ProtectionIconAreaSize{100.0f, 100.0f};
            drawInventoryUiProtectionIcon(
                renderer,
                {imageCenter - ProtectionIconAreaSize * 0.5f, ProtectionIconAreaSize},
                InventoryUiProtectionIconStyle{.size = 24.0f});
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

    std::string rawNameText;
    std::string descriptionText;
    std::string categoryText;
    int rarity = 0;
    if (objectNotice) {
        rawNameText = itemAcquisitionObjectDisplayName(*object, notice.amount);
        descriptionText = object->description.empty() ? "-" : object->description;
        categoryText = object->category;
        rarity = object->rarity;
    } else if (notice.kind == AcquisitionNoticeKind::Material) {
        rawNameText = std::string(materialTypeDisplayName(notice.materialType)) + " x" + std::to_string(std::max(1, notice.amount));
        descriptionText = "魔女からのお礼";
        categoryText = "素材";
    } else {
        rawNameText = std::to_string(std::max(1, notice.amount)) + "G";
        descriptionText = "魔女からのお礼";
        categoryText = "お金";
    }
    float detailLineY = drawInventoryUiDetailHeader(
        renderer,
        detailLayout,
        rawNameText,
        categoryText,
        rarity,
        animationSeconds);
    drawUiDetailText(renderer, detailLayout, detailLineY, descriptionText);
    if (objectNotice) {
        drawInventoryUiItemEffectSections(
            renderer,
            detailLayout,
            detailLineY,
            *object,
            objectCatalog_,
            encyclopedia_,
            itemStats,
            unlockedRingCount());
        drawInventoryUiWeightLine(renderer, detailLayout, detailLineY, *object, itemStats);
    }

    const UiRect okButton = itemAcquisitionOkButtonRect(panel);
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
    drawUiButton(renderer, okButton, "OK", false, uiActionButtonStyle());
    }
    renderer.popScreenTransform();

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

Vec2 Game::dungeonMapOverlayPlayerCenteredScroll() const
{
    const UiRect viewport = dungeonMapOverlayViewportRect();
    const DungeonMinimapBounds bounds = dungeonMinimapBounds(dungeonMinimapCells_);
    if (!bounds.valid) {
        return {};
    }

    const Vec2 mapSize = dungeonMapOverlayMapSize(viewport);
    const Vec2 maxScroll = dungeonMapOverlayMaxScroll();
    if (maxScroll.x <= 0.0f && maxScroll.y <= 0.0f) {
        return {};
    }

    const int tileCountX = std::max(1, bounds.maxX - bounds.minX + 1);
    const int tileCountY = std::max(1, bounds.maxY - bounds.minY + 1);
    const float tilePx = std::min(
        mapSize.x / static_cast<float>(tileCountX),
        mapSize.y / static_cast<float>(tileCountY));
    const int playerTileX = tileMap_.worldToTile(player_.position.x);
    const int playerTileY = tileMap_.worldToTile(player_.position.y);
    const Vec2 playerOnMap{
        (static_cast<float>(playerTileX - bounds.minX) + 0.5f) * tilePx,
        (static_cast<float>(playerTileY - bounds.minY) + 0.5f) * tilePx,
    };
    return {
        std::clamp(playerOnMap.x - viewport.size.x * 0.5f, 0.0f, maxScroll.x),
        std::clamp(playerOnMap.y - viewport.size.y * 0.5f, 0.0f, maxScroll.y),
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
    UiControlMotionScope minimapMotion(
        renderer,
        minimapRect,
        UiControlMotion::HoverAndPress);
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

    {
        FrameProfileScope minimapTilesProfile("UI.minimap.tiles");
        renderer.fillCircle(minimapCenter, contentRadius, {0, 0, 0, 255});

        const int minTileX = playerTileX - viewRadiusTiles;
        const int maxTileX = playerTileX + viewRadiusTiles;
        const int minTileY = playerTileY - viewRadiusTiles;
        const int maxTileY = playerTileY + viewRadiusTiles;
        static thread_local std::vector<Renderer::ColorRect> minimapTileRects;
        minimapTileRects.clear();
        const std::size_t maxVisibleTileCount =
            static_cast<std::size_t>(std::max(0, maxTileX - minTileX + 1)) *
            static_cast<std::size_t>(std::max(0, maxTileY - minTileY + 1));
        const std::size_t reserveTileCount = std::min(maxVisibleTileCount, dungeonMinimapCells_.size());
        if (minimapTileRects.capacity() < reserveTileCount) {
            minimapTileRects.reserve(reserveTileCount);
        }
        for (const auto& [key, cell] : dungeonMinimapCells_) {
            const DungeonTile tile = dungeonMinimapTileFromKey(key);
            if (tile.x < minTileX || tile.x > maxTileX || tile.y < minTileY || tile.y > maxTileY) {
                continue;
            }
            const Vec2 cellCenter = tileToMini(tile.x, tile.y);
            if (!pointInMinimap(cellCenter, DungeonMinimapTilePx)) {
                continue;
            }
            const bool lit = tileMap_.isLit(tileMap_.tileCenter(tile.x, tile.y), playerLightCenter, itemLights);
            const Color color = dungeonMinimapTileColor(cell.type, lit);
            const Vec2 drawPos = cellCenter - Vec2{DungeonMinimapTilePx, DungeonMinimapTilePx} * 0.5f;
            minimapTileRects.push_back(Renderer::ColorRect{
                drawPos,
                {DungeonMinimapTilePx + 0.4f, DungeonMinimapTilePx + 0.4f},
                color});
        }
        renderer.fillColorRects(minimapTileRects);
    }

    bool hasWarpGuideMarker = false;
    Vec2 warpGuideDirection{};
    float warpGuidePulse = 0.0f;
    {
        FrameProfileScope minimapMarkersProfile("UI.minimap.markers");
        const auto drawMiniCircleOnTile = [&](int tx, int ty, float radius, Color color) {
            const Vec2 marker = tileToMini(tx, ty);
            if (!pointInMinimap(marker, radius + 1.0f)) {
                return;
            }
            if (!dungeonMinimapTileSeen(tx, ty)) {
                return;
            }
            drawDungeonMapCircleMarker(renderer, marker, radius, color);
        };
        const auto drawMiniSquareOnTile = [&](int tx, int ty, float size, Color color) {
            const Vec2 marker = tileToMini(tx, ty);
            if (!pointInMinimap(marker, size * 0.5f + 1.0f)) {
                return;
            }
            if (!dungeonMinimapTileSeen(tx, ty)) {
                return;
            }
            drawDungeonMapSquareMarker(renderer, marker, size, color);
        };

        if (warpPointsEnabled_) {
            for (const WarpPoint& point : warpPoints_) {
                const Vec2 marker = tileToMini(point.tilePosition.x, point.tilePosition.y);
                if (!pointInMinimap(marker, 7.0f)) {
                    continue;
                }
                if (!dungeonMinimapTileSeen(point.tilePosition.x, point.tilePosition.y)) {
                    continue;
                }
                drawDungeonMapWarpMarker(
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
            const Vec2 marker = tileToMini(entranceTileX, entranceTileY);
            if (pointInMinimap(marker, 7.0f) && dungeonMinimapTileSeen(entranceTileX, entranceTileY)) {
                drawDungeonMapLadderMarker(renderer, marker, 0.68f);
            }
        }

        for (const RewardNode& node : rewardNodes_) {
            if (rewardNodeVisibleOnDungeonMap(node)) {
                drawMiniCircleOnTile(node.tile.x, node.tile.y, 3.0f, DungeonMapItemColor);
            }
        }
        for (const MoneyNode& node : moneyNodes_) {
            if (moneyNodeVisibleOnDungeonMap(node)) {
                drawMiniCircleOnTile(node.tile.x, node.tile.y, 2.1f, DungeonMapMoneyColor);
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
                drawMiniCircleOnTile(dropTileX, dropTileY, 2.1f, DungeonMapMoneyColor);
                break;
            case WorldDropKind::Material:
                drawMiniCircleOnTile(dropTileX, dropTileY, 2.2f, DungeonMapMaterialColor);
                break;
            }
        }

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
    }

    {
        FrameProfileScope minimapEnemiesProfile("UI.minimap.enemies");
        static thread_local std::vector<EnemyMinimapMarker> enemyMarkers;
        enemyMarkers.clear();
        enemies_.appendMinimapMarkers(enemyMarkers);
        for (const EnemyMinimapMarker& enemy : enemyMarkers) {
            const int enemyTileX = tileMap_.worldToTile(enemy.position.x);
            const int enemyTileY = tileMap_.worldToTile(enemy.position.y);
            const Vec2 marker = tileToMini(enemyTileX, enemyTileY);
            if (!pointInMinimap(marker, enemy.boss ? 6.0f : 4.5f)) {
                continue;
            }
            if (!dungeonMinimapTileSeen(enemyTileX, enemyTileY)) {
                continue;
            }
            const Color fill = enemy.boss ? DungeonMapBossEnemyColor : DungeonMapEnemyColor;
            renderer.fillCircle(marker, enemy.boss ? 4.0f : 3.0f, fill);
        }
    }

    {
        FrameProfileScope minimapFrameProfile("UI.minimap.frame");
        const Vec2 facing = lengthSquared(player_.facing) > 0.0001f ? normalize(player_.facing) : Vec2{1.0f, 0.0f};
        drawDungeonMapPlayerArrow(renderer, minimapCenter, facing, 0.72f);
        const float frameScale = minimapDiameter / DungeonMinimapDiameter;
        ImageDrawOptions frameOptions;
        frameOptions.anchor = {0.5f, 0.5f};
        if (!renderer.drawImage(
                DungeonMinimapFramePath,
                minimapCenter,
                DungeonMinimapFrameImageSize * frameScale,
                frameOptions,
                TextureFilter::Linear)) {
            renderer.drawCircle(minimapCenter, contentRadius, {88, 108, 132, 145});
        }
        if (hasWarpGuideMarker) {
            drawWarpGuideMinimapIcon(renderer, minimapCenter + warpGuideDirection * (contentRadius + 0.5f), warpGuideDirection, warpGuidePulse);
        }
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
    UiExclusiveNavigationScope navigationScope(panel);
    UiWindowScope window(
        renderer,
        "dungeon.map_overlay",
        panel,
        "探索地図",
        buildInputHelpText({
            {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "閉じる"},
        }),
        UiWindowOptions{true, true});

    const UiRect mapRect = dungeonMapOverlayViewportRect();
    renderer.fillRect(mapRect.pos, mapRect.size, {4, 7, 12, 255});

    const DungeonMinimapBounds bounds = dungeonMinimapBounds(dungeonMinimapCells_);
    if (!bounds.valid) {
        renderer.drawText(mapRect.pos + Vec2{20.0f, 20.0f}, "地図情報がないよ", {170, 178, 190, 255}, 2);
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
    static thread_local std::vector<Renderer::ColorRect> mapTileRects;
    mapTileRects.clear();
    const float safeTilePx = std::max(0.001f, tilePx);
    const std::size_t visibleTileEstimate =
        static_cast<std::size_t>(std::max(1, static_cast<int>(std::ceil(mapRect.size.x / safeTilePx)) + 2)) *
        static_cast<std::size_t>(std::max(1, static_cast<int>(std::ceil(mapRect.size.y / safeTilePx)) + 2));
    if (mapTileRects.capacity() < visibleTileEstimate) {
        mapTileRects.reserve(visibleTileEstimate);
    }
    for (int ty = bounds.minY; ty <= bounds.maxY; ++ty) {
        for (int tx = bounds.minX; tx <= bounds.maxX; ++tx) {
            const auto cellIt = dungeonMinimapCells_.find(dungeonMinimapKey(tx, ty));
            if (cellIt == dungeonMinimapCells_.end()) {
                continue;
            }
            const bool lit = tileMap_.isLit(tileMap_.tileCenter(tx, ty), playerLightCenter, itemLights);
            const Renderer::ColorRect tileRect = dungeonMapOverlayTileRect(
                origin,
                tx - bounds.minX,
                ty - bounds.minY,
                tilePx,
                dungeonMinimapTileColor(cellIt->second.type, lit));
            if (tileRect.pos.x + tileRect.size.x < mapRect.pos.x ||
                tileRect.pos.y + tileRect.size.y < mapRect.pos.y ||
                tileRect.pos.x > mapRect.pos.x + mapRect.size.x ||
                tileRect.pos.y > mapRect.pos.y + mapRect.size.y) {
                continue;
            }
            mapTileRects.push_back(tileRect);
        }
    }
    renderer.fillColorRects(mapTileRects);

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
            drawDungeonMapWarpMarker(
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
            drawMapCircleOnTile(node.tile.x, node.tile.y, smallMarkerRadius, DungeonMapMoneyColor);
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
            drawMapCircleOnTile(dropTileX, dropTileY, smallMarkerRadius, DungeonMapMoneyColor);
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
        UiControlMotionScope motion(renderer, track, UiControlMotion::HoverAndPress);
        renderer.fillRect(track.pos, track.size, {0, 0, 0, 96});
        renderer.fillRect(thumb.pos, thumb.size, {154, 178, 208, 190});
    }
    if (maxScroll.x > 0.0f) {
        const UiRect track = dungeonMapOverlayHorizontalScrollTrackRect();
        const UiRect thumb = dungeonMapOverlayHorizontalScrollThumbRect();
        UiControlMotionScope motion(renderer, track, UiControlMotion::HoverAndPress);
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
    const float statusTopY = dungeonStatusHudRect(screenWidth, screenHeight).pos.y;
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
    const Vec2 fontMeasure = renderer.measureText("0", TextScale);
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
        textStyle.iconScale = TopInfoBarIconSize / std::max(1.0f, fontMeasure.y);
        textStyle.outlineEnabled = true;
        textStyle.outline = {0, 0, 0, textAlpha};
        textStyle.outlinePx = 2;
        message = fittedInlineItemText(renderer, std::move(message), DungeonLogWidth - 26.0f, textStyle);
        const Vec2 textPos{
            x + DungeonLogWidth - 14.0f,
            rowY + std::max(0.0f, (DungeonLogRowHeight - fontMeasure.y) * 0.5f),
        };
        drawInlineItemTextRightAligned(renderer, objectCatalog_, textPos, message, textStyle);
    }
}

void Game::renderImportantDungeonNotices(Renderer& renderer) const
{
    const int visibleCount = std::min(
        static_cast<int>(importantDungeonNotices_.size()),
        ImportantDungeonNoticeMaxVisible);
    if (visibleCount <= 0) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect block = importantDungeonNoticeBlockRect(
        static_cast<float>(camera_.width()),
        static_cast<float>(camera_.height()),
        visibleCount);
    const int firstIndex = static_cast<int>(importantDungeonNotices_.size()) - visibleCount;

    constexpr int TextScale = 2;
    for (int i = 0; i < visibleCount; ++i) {
        const DungeonLogEntry& entry =
            importantDungeonNotices_[static_cast<std::size_t>(firstIndex + i)];
        const float fadeDenom = std::max(0.01f, entry.lifetime - ImportantDungeonNoticeFadeStart);
        const float fade = entry.age <= ImportantDungeonNoticeFadeStart
            ? 1.0f
            : std::clamp(
                1.0f - (entry.age - ImportantDungeonNoticeFadeStart) / fadeDenom,
                0.0f,
                1.0f);
        const float rowY = block.pos.y +
            static_cast<float>(i) * (ImportantDungeonNoticeRowHeight + ImportantDungeonNoticeGap);
        const float halfWidth = block.size.x * 0.5f;
        const unsigned char centerAlpha = static_cast<unsigned char>(
            std::clamp(205.0f * fade, 0.0f, 205.0f));
        renderer.fillGradientRect(
            {block.pos.x, rowY},
            {halfWidth, ImportantDungeonNoticeRowHeight},
            {0, 0, 0, 0},
            {12, 10, 18, centerAlpha},
            GradientDirection::LeftToRight);
        renderer.fillGradientRect(
            {block.pos.x + halfWidth, rowY},
            {halfWidth, ImportantDungeonNoticeRowHeight},
            {12, 10, 18, centerAlpha},
            {0, 0, 0, 0},
            GradientDirection::LeftToRight);

        InlineItemTextStyle textStyle;
        textStyle.text = {
            255,
            236,
            166,
            static_cast<unsigned char>(std::clamp(255.0f * fade, 0.0f, 255.0f)),
        };
        textStyle.scale = TextScale;
        textStyle.iconTextGap = 5.0f;
        textStyle.iconScale = 1.15f;
        textStyle.outlineEnabled = true;
        textStyle.outline = {
            0,
            0,
            0,
            static_cast<unsigned char>(std::clamp(255.0f * fade, 0.0f, 255.0f)),
        };
        textStyle.outlinePx = 2;

        const std::string message = fittedInlineItemText(
            renderer,
            entry.message,
            block.size.x - 72.0f,
            textStyle);
        const Vec2 textSize = measureInlineItemText(renderer, message, textStyle);
        const Vec2 textPos{
            block.pos.x + std::max(0.0f, (block.size.x - textSize.x) * 0.5f),
            rowY + std::max(0.0f, (ImportantDungeonNoticeRowHeight - textSize.y) * 0.5f),
        };
        drawInlineItemText(renderer, objectCatalog_, textPos, message, textStyle);
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

    std::string help = buildInputHelpText({
        {InputHelpGroup::Back, {InputAction::Pause}, "メニュー"},
        {InputHelpGroup::Other, {}, "アイテム選択", inlineShortcutCursorInputTag()},
        {InputHelpGroup::Other, {}, "アイテム行切替", inlineShortcutRowInputTag()},
        {InputHelpGroup::Other, {InputAction::UseSelectedItem}, "使用"},
        {InputHelpGroup::Other, {InputAction::PutSelectedItemOnRing}, "リングへ"},
        {InputHelpGroup::Other, {InputAction::OffsetRingCenter}, "中心ずらし"},
        {InputHelpGroup::Other, {InputAction::ThrowActiveRing}, "リング投げ"},
    });
    bool promptFocused = false;
    if (introTutorialActive()) {
        help = buildInputHelpText({
            {InputHelpGroup::Back, {InputAction::Pause}, "メニュー"},
        });
        if (introTutorialPhase_ == IntroTutorialPhase::FreeToExit &&
            dungeonInspectableInRange(
                introTutorialExitPosition(),
                {DungeonEntranceImageMaxWidth, DungeonEntranceImageMaxHeight})) {
            help = buildInputHelpText({
                {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, "出口から拠点へ帰還"},
            });
            promptFocused = true;
        }
    } else if (focusedWarpReturnPointIndex_ == DungeonEntranceReturnFocusIndex) {
        help = buildInputHelpText({
            {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, "ダンジョン入口から拠点へ帰還"},
        });
        promptFocused = true;
    } else if (warpPointsEnabled_) {
        if (focusedWarpReturnPointIndex_ >= 0 &&
            focusedWarpReturnPointIndex_ < static_cast<int>(warpPoints_.size())) {
            const WarpPoint& point = warpPoints_[static_cast<std::size_t>(focusedWarpReturnPointIndex_)];
            if (point.discovered) {
                help = buildInputHelpText({{
                    InputHelpGroup::Primary,
                    {InputAction::Confirm, InputAction::UseSelectedItem},
                    "ワープポイント " + std::to_string(point.index + 1) + " から拠点へ帰還",
                }});
                promptFocused = true;
            }
        }
    }
    if (!promptFocused && currentStageIsRoguelike() && roguelikeBigHole_.active && focusedRoguelikeBigHole_ != 0) {
        help = roguelikeBigHole_.unlocked
            ? buildInputHelpText({
                {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, "大穴を調べる"},
            })
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
    const float safeTop = TopInfoBarY + TopInfoBarHeight + 8.0f;
    UiRect safeArea{{0.0f, safeTop}, {screenWidth, std::max(0.0f, screenHeight - safeTop)}};
    if (introTutorialHelpLayout) {
        safeArea.pos.x = (screenWidth - shortcutHud.size.x) * 0.5f;
        safeArea.size.x = shortcutHud.size.x;
        safeArea.size.y = std::max(0.0f, shortcutHud.pos.y - 10.0f - safeTop);
    }
    drawUiBottomInputHelp(renderer, "dungeon.control-help", safeArea, std::move(help));
}

void Game::renderWarpReturnUi(Renderer& renderer) const
{
    if (mode_ != ScreenMode::Playing || enemyTestActive_ || (!warpPointsEnabled_ && !warpReturnConfirm_.open)) {
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
    loadingGaugeStyle.highlight = {255, 255, 255, 118};
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
    std::string ringHelpText;
    if (ringItemMoveModeActive_) {
        ringHelpText = buildInputHelpText({
            {
                InputHelpGroup::Primary,
                {InputAction::Confirm, InputAction::UseSelectedItem, InputAction::GrabOrPlaceItem},
                "確定",
            },
            {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "キャンセル"},
        });
    } else {
        std::vector<InputHelpEntry> entries{
            {InputHelpGroup::Primary, {InputAction::Confirm, InputAction::UseSelectedItem}, "コマンド"},
            {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "戻る"},
        };
        if (ringCount > 1) {
            entries.push_back({
                InputHelpGroup::Cycle,
                {InputAction::CyclePrevious, InputAction::CycleNext},
                "リング切替",
            });
        }
        entries.push_back({InputHelpGroup::Other, {InputAction::GrabOrPlaceItem}, "移動"});
        entries.push_back({InputHelpGroup::Other, {InputAction::ArrangeItems}, "並び替え"});
        entries.push_back({InputHelpGroup::Other, {InputAction::PutSelectedItemOnRing}, "外す"});
        entries.push_back({InputHelpGroup::Other, {}, "全部外す", inlineRingRemoveAllInputTag()});
        entries.push_back({InputHelpGroup::Other, {InputAction::ToggleProtection}, "保護"});
        ringHelpText = buildInputHelpText(entries);
        if (presetSlotCount > 0 && !inputHelpUsesGamepad()) {
            const std::string slotRange = presetSlotCount == 1 ? "1" : "1-" + std::to_string(presetSlotCount);
            const std::string recallInput = inlineInputKeyChordTag({slotRange});
            const std::string registerInput = inlineInputKeyChordTag({"Shift", slotRange});
            ringHelpText += "  " + recallInput + " 呼出  " + registerInput + " 登録";
        }
    }
    UiWindowScope ringWindow(renderer, "ring.manage", panel, "リング", ringHelpText, UiWindowOptions{true, true});

    char buffer[192];
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
        spellRing_.activeRingIndex(),
        ringTabs.data(),
        ringCount,
        ringTabRects.data());
    if (!ringItemMoveModeActive_) {
        drawUiButton(
            renderer,
            ringArrangeButtonRect(),
            "並び替え",
            false,
            uiActionButtonStyle());
        if (presetSlotCount > 0) {
            drawUiButton(
                renderer,
                ringPresetButtonRect(false),
                "プリセット呼出",
                false,
                uiActionButtonStyle());
            drawUiButton(
                renderer,
                ringPresetButtonRect(true),
                "プリセット登録",
                false,
                uiActionButtonStyle());
        }
    }
    const bool actualRing = true;
    const auto& items = spellRing_.items();
    (void)actualRing;
    const RingShape activeShape = spellRing_.activeRingShape();

    const int activeRingIndex = spellRing_.activeRingIndex();
    const RingUiPreviewStyle previewStyle = ringUiPreviewStyle(activeRingIndex);
    drawRingManagementWeightSummary(renderer, panel.pos + Vec2{50.0f, 160.0f}, spellRing_, activeRingIndex);
    const Vec2 orbitCenter = ringUiOrbitCenter(spellRing_);
    std::vector<Vec2> orbitPath = getRingPathSamplePoints(
        spellRing_.center(),
        ringUiOrbitContext(spellRing_, balance_, 0, 1),
        160);
    for (Vec2& point : orbitPath) {
        point = applyRingUiShapeRotation(spellRing_, ringWorldToUi(spellRing_, point));
    }
    MagicOrbitDrawOptions orbitOptions{
        activeShape,
        true,
        false,
        true,
        true,
        activeRingIndex,
        totalTime,
        1.0f,
    };
    orbitOptions.centerDecoration = previewStyle.centerDecoration;
    drawMagicOrbitPath(renderer, orbitPath, orbitCenter, orbitOptions);

    std::vector<RingUiItemDrawOrderEntry> drawOrder;
    drawOrder.reserve(items.size());
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const SpellRingItem& item = items[static_cast<std::size_t>(i)];
        const UiRect itemRect = ringItemUiRect(
            item,
            spellRing_,
            balance_,
            i,
            static_cast<int>(items.size()));
        registerUiNavigationTarget(
            itemRect,
            UiNavigationRole::Control,
            i == ringSlotSelection_);
        float displayAngle = item.localAngle;
        const bool pointerMoving =
            i == ringDragItemIndex_ && (ringDragActive_ || ringSnapActive_);
        if (pointerMoving) {
            displayAngle = ringDragDisplayAngle_;
        }
        const Vec2 itemAnchor = ringItemUiCenterAtAngle(
            displayAngle,
            spellRing_,
            balance_,
            i,
            static_cast<int>(items.size()));
        const bool selected = uiControlVisualState(itemRect).selected;
        const bool keyboardMoving = ringItemMoveModeActive_ && i == ringItemMoveIndex_;
        drawOrder.push_back({
            i,
            itemAnchor,
            pointerMoving || keyboardMoving
                ? RingUiItemFrontPriority::Moving
                : (selected ? RingUiItemFrontPriority::Selected : RingUiItemFrontPriority::Normal),
        });
    }
    sortRingUiItemsBackToFront(drawOrder);

    for (const RingUiItemDrawOrderEntry& drawEntry : drawOrder) {
        const int i = drawEntry.itemIndex;
        const SpellRingItem& item = items[static_cast<std::size_t>(i)];
        const UiRect itemRect = ringItemUiRect(
            item,
            spellRing_,
            balance_,
            i,
            static_cast<int>(items.size()));
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
        // Combat feedback belongs to the live world, not the ring management preview.
        displayItem.actionFlashTimer = 0.0f;
        const Vec2 itemCenter = ringItemDrawPosition(displayItem, totalTime);
        const Vec2 outward = normalize(itemAnchor - orbitCenter);
        Vec2 forward{-outward.y, outward.x};
        if (lengthSquared(forward) <= 0.0001f) {
            forward = {1.0f, 0.0f};
        }
        const bool current = i == ringSlotSelection_;
        const bool selected = uiControlVisualState(itemRect).selected;
        const bool moveMode = ringItemMoveModeActive_ && i == ringItemMoveIndex_;
        const bool emphasized = selected || moveMode;
        const bool invalidDragPosition = current && ringDragActive_ && !spellRing_.canPlaceItemAtAngle(i, displayAngle);
        const ItemData* object = objectForRingItem(objectCatalog_, item);
        if (previewStyle.radialGuides && activeShape != RingShape::FigureEight) {
            const Color angleLineColor = moveMode
                ? Color{255, 142, 42, 150}
                : (emphasized ? Color{255, 230, 150, 120} : Color{94, 102, 128, 85});
            const Vec2 radial = itemAnchor - orbitCenter;
            Vec2 tangent = normalize(Vec2{-radial.y, radial.x});
            if (lengthSquared(tangent) <= 0.0001f) {
                tangent = {0.0f, 1.0f};
            }
            constexpr float AngleLineHalfWidthPx = 0.5f;
            renderer.drawLine(orbitCenter + tangent * AngleLineHalfWidthPx, itemAnchor + tangent * AngleLineHalfWidthPx, angleLineColor);
            renderer.drawLine(orbitCenter - tangent * AngleLineHalfWidthPx, itemAnchor - tangent * AngleLineHalfWidthPx, angleLineColor);
        }
        {
            UiControlMotionScope motion(renderer, itemRect, UiControlMotion::PressOnly);
            drawRingItemShape(
                renderer,
                displayItem,
                object,
                itemCenter,
                outward,
                forward,
                totalTime,
                emphasized,
                invalidDragPosition,
                moveMode,
                true,
                1.0f,
                &encyclopedia_);
        }
    }

    if (ringItemMoveModeActive_ &&
        ringItemMoveIndex_ >= 0 &&
        ringItemMoveIndex_ < static_cast<int>(items.size())) {
        suppressUiSelectionCursor();
        UiNavigationLayerScope moveNavigationScope;
        registerUiNavigationTarget(
            ringItemUiRect(
                items[static_cast<std::size_t>(ringItemMoveIndex_)],
                spellRing_,
                balance_,
                ringItemMoveIndex_,
                static_cast<int>(items.size())),
            UiNavigationRole::Control,
            true);
    }

    const UiRect ringDetailPanel = ringDetailRect();
    const int ringTabPreviewIndex = ringTabs_.hoveredIndex >= 0
        ? ringTabs_.hoveredIndex
        : (ringTabs_.navigationFocused ? ringTabs_.focusedIndex : -1);
    if (ringTabPreviewIndex >= 0 || ringDetailShowsRing_ || ringSlotSelection_ >= static_cast<int>(items.size())) {
        const int detailRingIndex = ringTabPreviewIndex >= 0
            ? ringTabPreviewIndex
            : spellRing_.activeRingIndex();
        drawRingDetailPanel(
            renderer,
            ringDetailPanel,
            spellRing_,
            equipmentModifiers_,
            balance_,
            detailRingIndex,
            unlockedRingCount());
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
    drawUiCommandMenu(
        renderer,
        ringPresetMenu_,
        RingPresetMenuItems.data(),
        presetSlotCount);

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
            totalTime,
            unlockedRingCount());
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
            cell.pos.y + std::max(0.0f, (cell.size.y - textSize.y) * 0.5f) +
                OperationSettingsTextOffsetY,
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
        const bool hoveredRow = row == operationSettingsHoveredRow_;
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
            const bool preferredCell = selectedRow && column == operationSettingsTable_.selectedColumn;
            const bool hoveredCell = columns[static_cast<std::size_t>(column)].enabled
                && hoveredRow
                && column == operationSettingsHoveredColumn_;
            if (columns[static_cast<std::size_t>(column)].enabled) {
                registerUiNavigationTarget(
                    cell,
                    UiNavigationRole::Control,
                    preferredCell);
            }
            UiControlMotionScope motion(
                renderer,
                cell,
                UiControlMotion::PressOnly,
                columns[static_cast<std::size_t>(column)].enabled);
            if (preferredCell) {
                renderer.fillRect(cell.pos, cell.size, OperationSettingsSelectedCellFill);
            } else if (hoveredCell) {
                renderer.fillRect(cell.pos, cell.size, OperationSettingsHoveredCellFill);
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
        const OperationSettingsActionRow& selectedRow = rows[static_cast<std::size_t>(row)];
        drawOptionsHelpWindow(
            renderer,
            operationSettingsHelpWindowRect(),
            operationSettingsHelpTitle(selectedRow),
            operationSettingsHelpDescription(selectedRow.action),
            optionsStatus_);
    }

    constexpr int ButtonCount = 1;
    constexpr const char* ButtonLabels[ButtonCount] = {
        "全初期化",
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

    drawUiCommandMenu(
        renderer,
        operationSettingsCommandMenu_,
        OperationSettingsCommandItems.data(),
        static_cast<int>(OperationSettingsCommandItems.size()));

    if (operationSettingsCapture_.active()) {
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 120});
        const UiRect dialog = operationSettingsDialogRect();
        const bool gamepadBinding = operationSettingsPendingColumn_ == OperationSettingsColumnGamepad;
        const bool gamepadOperationDisplay = inputHelpUsesGamepad();
        const std::string_view captureHelpText = gamepadOperationDisplay
            ? "B / ○を１秒長押しで中止"
            : "×クリックで中止";
        UiExclusiveNavigationScope navigationScope(dialog);
        UiWindowScope captureWindow(
            renderer,
            "operation_settings.capture",
            dialog,
            operationSettingsPendingEditMode_ == OperationSettingsBindingEditMode::Append
                ? "操作割当の追加"
                : "操作割当の変更",
            captureHelpText,
            UiWindowOptions{true, true});
        const std::string line = gamepadBinding
            ? "ゲームパッドのボタンまたはスティック/トリガーを入力してね"
            : "キーまたはマウスボタンを入力してね";
        const float textMaxWidth = std::max(1.0f, dialog.size.x - 84.0f);
        const Vec2 linePos = dialog.pos + Vec2{42.0f, 104.0f};
        renderer.drawWrappedText(linePos, line, textMaxWidth, ui::Text, 2);

        if (gamepadOperationDisplay) {
            InputHelpStyle footerHelpStyle;
            footerHelpStyle.text = ui::TextMuted;
            footerHelpStyle.scale = 2;
            footerHelpStyle.iconHeight = 23.0f;
            const UiRect footer = uiFooterRect(dialog, captureHelpText);
            const Vec2 footerTextPadding = renderer.hasUiWindowTexture()
                ? ui::ImageWindowFooterTextPadding
                : ui::FooterTextPadding;
            const Vec2 footerTextPos = footer.pos + footerTextPadding + ui::FooterHelpTextOffset;
            const Vec2 footerTextSize = measureInputHelpText(renderer, captureHelpText, footerHelpStyle);
            constexpr Vec2 HoldTrackSize{40.0f, 5.0f};
            constexpr float HoldTrackGap = 12.0f;
            const UiRect holdTrack{
                {
                    footerTextPos.x + footerTextSize.x + HoldTrackGap,
                    footerTextPos.y + (footerTextSize.y - HoldTrackSize.y) * 0.5f,
                },
                HoldTrackSize,
            };
            const float holdProgress = operationSettingsCapture_.gamepadCancelHoldProgress();
            renderer.fillRect(holdTrack.pos, holdTrack.size, {18, 20, 32, 210});
            if (holdProgress > 0.0f) {
                renderer.fillRect(
                    holdTrack.pos,
                    {holdTrack.size.x * holdProgress, holdTrack.size.y},
                    {212, 200, 126, 235});
            }
            renderer.drawRect(holdTrack.pos, holdTrack.size, {112, 116, 138, 200});
        }
    }

    if (operationSettingsReadOnlyDialog_.open) {
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 120});
        drawUiResultDialog(
            renderer,
            operationSettingsReadOnlyDialog_,
            operationSettingsDialogRect(),
            "operation_settings.read_only");
    } else if (operationSettingsConflictConfirm_.open) {
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
    const OptionsSliderUiState& sliderState = optionsSliderUiState();
    const auto values = audioSettingsRowValueTexts(optionsSettings_);
    const auto tabItems = audioSettingsTabItems(values);
    const auto tabRects = audioSettingsRowRects();
    drawUiVerticalTabs(
        renderer,
        audioSettingsTabs_,
        audioSettingsSelection_,
        tabItems.data(),
        static_cast<int>(tabItems.size()),
        tabRects.data(),
        optionsVerticalTabStyle());

    for (int row = 0; row < AudioSettingsRowCount; ++row) {
        const float value = audioSettingsRowValue(optionsSettings_, row) * 100.0f;
        const UiRect slider = audioSettingsSliderRect(row);
        drawUiSlider(
            renderer,
            slider,
            value,
            audioSettingsSliderSpec(),
            sliderState.audio[static_cast<std::size_t>(row)],
            audioSettingsSliderStyle(row));
    }

    drawOptionsHelpWindow(
        renderer,
        audioSettingsRowLabel(audioSettingsSelection_),
        audioSettingsHelpText(audioSettingsSelection_),
        optionsStatus_);

    constexpr int ButtonCount = 1;
    constexpr const char* ButtonLabels[ButtonCount] = {"全初期化"};
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
    const OptionsSliderUiState& sliderState = optionsSliderUiState();
    const auto values = videoSettingsRowValueTexts(optionsSettings_);
    const auto tabItems = videoSettingsTabItems(values);
    const auto tabRects = videoSettingsRowRects();
    drawUiVerticalTabs(
        renderer,
        videoSettingsTabs_,
        videoSettingsSelection_,
        tabItems.data(),
        static_cast<int>(tabItems.size()),
        tabRects.data(),
        optionsVerticalTabStyle());

    drawUiSlider(
        renderer,
        videoBrightnessSliderRect(),
        clampedScreenBrightness(optionsSettings_.presentation.brightness) * 100.0f,
        screenBrightnessSliderSpec(),
        sliderState.brightness,
        screenBrightnessSliderStyle());

    drawOptionsHelpWindow(
        renderer,
        videoSettingsRowLabel(videoSettingsSelection_),
        videoSettingsHelpText(videoSettingsSelection_),
        optionsStatus_);

    constexpr int ButtonCount = 1;
    constexpr const char* ButtonLabels[ButtonCount] = {"全初期化"};
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
        : (pausePage_ == PauseMenuPage::Options ? "オプション" : "MENU");
    std::string pauseHelp;
    if (pausePage_ == PauseMenuPage::Options) {
        pauseHelp = optionsMenuHelpText(optionsPage_);
    } else if (pausePage_ == PauseMenuPage::Ring) {
        pauseHelp = buildInputHelpText({
            {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "戻る"},
            {
                InputHelpGroup::Cycle,
                {InputAction::CyclePrevious, InputAction::CycleNext},
                "リング切替",
            },
        });
    } else if (pausePage_ == PauseMenuPage::Status) {
        pauseHelp = buildInputHelpText({
            {InputHelpGroup::Back, {InputAction::Cancel, InputAction::Pause}, "戻る"},
        });
    } else {
        pauseHelp = standardMenuHelpText();
    }
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
            drawUiButton(renderer, pauseMenuItemRect(i), pauseMenuItemName(i), pauseMenuItemIconImageNumber(i), selected);
        }
        return;
    }

    if (pausePage_ == PauseMenuPage::Status) {
        constexpr float LeftWidth = 528.0f;
        const UiRect ringPanel{{panel.pos.x + 46.0f, panel.pos.y + 340.0f}, {LeftWidth, 172.0f}};
        const Vec2 profilePos{ringPanel.pos.x, panel.pos.y + 112.0f};
        const Vec2 profileTitlePos = profilePos + Vec2{0.0f, -12.0f};
        const UiRect portraitRect{{panel.pos.x + 636.0f, panel.pos.y + 36.0f}, {340.0f, 500.0f}};

        drawUiSubPanel(renderer, ringPanel);

        renderer.drawText(profileTitlePos, "見習い魔女 ルネ", {246, 235, 255, 255}, 3);
        renderer.drawText(profileTitlePos + Vec2{1.0f, 0.0f}, "見習い魔女 ルネ", {246, 235, 255, 255}, 3);

        const int hpMax = std::max(1, player_.maxHp);
        const int hp = std::clamp(player_.hp, 0, hpMax);
        std::snprintf(buffer, sizeof(buffer), "HP  %02d / %02d", hp, hpMax);
        renderer.drawText(profilePos + Vec2{0.0f, 34.0f}, buffer, {255, 232, 232, 255}, 2);

        UiGaugeStyle hpGaugeStyle;
        hpGaugeStyle.fill.start = {224, 74, 84, 255};
        hpGaugeStyle.fill.end = {255, 126, 116, 255};
        hpGaugeStyle.track = {42, 18, 24, 230};
        hpGaugeStyle.trackInner = {58, 24, 32, 220};
        hpGaugeStyle.trackOuter = {255, 220, 224, 82};
        hpGaugeStyle.highlight = {255, 244, 244, 92};
        hpGaugeStyle.trackInnerInset = 4.0f;
        hpGaugeStyle.shadowOffsetY = 2.0f;
        hpGaugeStyle.shadowExtra = 5.0f;
        constexpr float StatusGaugeWidth = 228.0f;
        drawUiGauge(
            renderer,
            {profilePos + Vec2{0.0f, 60.0f}, {StatusGaugeWidth, 12.0f}},
            static_cast<float>(hp) / static_cast<float>(hpMax),
            hpGaugeStyle);

        const float expGaugeX = ringPanel.pos.x + ringPanel.size.x - StatusGaugeWidth;
        const Vec2 expLabelPos{expGaugeX, profilePos.y + 34.0f};
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
        expGaugeStyle.trackInnerInset = 4.0f;
        expGaugeStyle.shadowOffsetY = 2.0f;
        expGaugeStyle.shadowExtra = 5.0f;
        const int xpToNext = std::max(1, player_.xpToNext);
        const float expProgress = playerAtMaxLevel(player_)
            ? 1.0f
            : static_cast<float>(std::clamp(player_.xp, 0, xpToNext)) / static_cast<float>(xpToNext);
        drawUiGauge(
            renderer,
            {{expGaugeX, profilePos.y + 60.0f}, {StatusGaugeWidth, 12.0f}},
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
            profilePos + Vec2{0.0f, 94.0f},
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
            const std::string count = std::to_string(inventory_.materialCount(type));
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
            drawUiTextWithIcon(
                renderer,
                {ringContent.pos.x, y},
                ringDisplayName(ringIndex, unlockedRings),
                ringDisplayIconImageNumber(ringIndex));

            const std::vector<SpellRingItem>& items = spellRing_.itemsForRing(ringIndex);
            std::snprintf(
                buffer,
                sizeof(buffer),
                "アイテム %02d/%02d",
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

        const std::string statusPortraitPath = portraitPathForSpeaker("player", defaultPortraitVariant("player"));
        Vec2 portraitSourceSize;
        const bool portraitSizeLoaded =
            renderer.getImageSize(statusPortraitPath, portraitSourceSize, TextureFilter::Nearest) &&
            portraitSourceSize.x > 0.0f &&
            portraitSourceSize.y > 0.0f;
        constexpr float PortraitScale = 0.65f;
        const float sourceScale = portraitPathUsesScaledSource(statusPortraitPath) ? 3.0f : 1.0f;
        const Vec2 portraitDrawSize = (portraitSizeLoaded ? portraitSourceSize * sourceScale : portraitRect.size) * PortraitScale;
        ImageDrawOptions portraitOptions;
        portraitOptions.anchor = {0.5f, 0.06f};
        portraitOptions.flipX = true;
        if (!renderer.drawImage(
                statusPortraitPath,
                {panel.pos.x + panel.size.x - 190.0f, panel.pos.y - 36.0f},
                portraitDrawSize,
                portraitOptions,
                TextureFilter::Nearest)) {
            renderer.drawPlayerSprite(
                0,
                {portraitRect.pos.x + portraitRect.size.x * 0.5f, panel.pos.y + panel.size.y - 94.0f},
                180.0f,
                false,
                {255, 255, 255, 255},
                {PlayerSpriteAnchorX, PlayerSpriteAnchorY},
                false,
                artworkImageDrawOptions());
        }
    } else if (pausePage_ == PauseMenuPage::Items) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "アイテム", {246, 235, 255, 255}, 3);
    } else if (pausePage_ == PauseMenuPage::Ring) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "リング", {246, 235, 255, 255}, 3);
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

bool Game::renderDeathResultPrelude(Renderer& renderer) const
{
    if (!deathResultPrelude_.active) {
        return false;
    }

    renderer.setScreenSpace();
    const UiRect screen{{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}};
    renderer.fillRect(screen.pos, screen.size, {0, 0, 0, alphaByte(255.0f * deathResultPreludeBlackAlpha())});
    const float starAlpha = deathResultPreludeStarAlpha();
    if (starAlpha > 0.0f) {
        renderAstralEchoStarfield(renderer, screen, false, starAlpha);
    }
    return deathResultPreludeBlocksWindow();
}

void Game::renderAstralEchoStarfield(Renderer& renderer, UiRect area, bool showConstellation, float alpha) const
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha <= 0.0f) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect(area.pos, area.size, {2, 3, 10, alphaByte(255.0f * alpha)});
    renderer.fillGradientRect(
        area.pos,
        area.size,
        {6, 8, 20, alphaByte(255.0f * alpha)},
        {2, 3, 10, alphaByte(255.0f * alpha)},
        GradientDirection::TopToBottom);

    const int starCount = std::max(0, astralEchoStarCount_);
    for (int i = 0; i < starCount; ++i) {
        const bool recent = astralEchoRecentStarVisible_ && i == astralEchoRecentStarIndex_;
        const float brightness = astralEchoUnit(i, 73);
        const float radius = 0.8f + astralEchoUnit(i, 101) * 1.0f;
        const Vec2 pos = astralEchoStarPosition(area, i);
        const Color core = astralEchoStarColor(recent, brightness);
        const unsigned char glowAlpha = recent
            ? alphaByte(64.0f * alpha)
            : alphaByte(std::clamp(36.0f + brightness * 72.0f, 0.0f, 255.0f) * alpha);
        const Color fadedCore{core.r, core.g, core.b, alphaByte(static_cast<float>(core.a) * alpha)};
        renderer.fillSoftCircle(pos, radius * 3.8f, {core.r, core.g, core.b, glowAlpha});
        renderer.fillCircle(pos, radius, fadedCore);
    }

    if (!showConstellation) {
        return;
    }

    const Vec2 center = area.pos + area.size * 0.5f;
    const float constellationScale = std::min(area.size.x, area.size.y) * 0.32f;
    const std::array<Vec2, 10> points = {
        center + Vec2{-0.46f, -0.20f} * constellationScale,
        center + Vec2{-0.24f, -0.38f} * constellationScale,
        center + Vec2{0.02f, -0.28f} * constellationScale,
        center + Vec2{0.26f, -0.46f} * constellationScale,
        center + Vec2{0.42f, -0.16f} * constellationScale,
        center + Vec2{0.18f, 0.04f} * constellationScale,
        center + Vec2{0.34f, 0.34f} * constellationScale,
        center + Vec2{-0.02f, 0.26f} * constellationScale,
        center + Vec2{-0.30f, 0.42f} * constellationScale,
        center + Vec2{-0.18f, 0.02f} * constellationScale,
    };
    constexpr std::array<std::pair<int, int>, 11> lines = {{
        {0, 1}, {1, 2}, {2, 3}, {2, 5}, {3, 4}, {4, 5},
        {5, 6}, {5, 7}, {7, 8}, {7, 9}, {9, 0},
    }};
    for (const auto& line : lines) {
        renderer.drawSoftLine(points[static_cast<std::size_t>(line.first)], points[static_cast<std::size_t>(line.second)], 8.0f, {106, 212, 255, alphaByte(58.0f * alpha)});
        renderer.drawSoftLine(points[static_cast<std::size_t>(line.first)], points[static_cast<std::size_t>(line.second)], 2.4f, {214, 242, 255, alphaByte(156.0f * alpha)});
        renderer.drawLine(points[static_cast<std::size_t>(line.first)], points[static_cast<std::size_t>(line.second)], {255, 252, 218, alphaByte(210.0f * alpha)});
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
        const float radius = i == 5 ? 7.0f : 4.6f;
        renderer.fillSoftCircle(points[i], radius * 5.0f, {116, 224, 255, alphaByte(124.0f * alpha)});
        renderer.fillCircle(points[i], radius, {255, 249, 202, alphaByte(255.0f * alpha)});
        renderer.fillCircle(points[i] + Vec2{-radius * 0.28f, -radius * 0.30f}, radius * 0.38f, {255, 255, 255, alphaByte(255.0f * alpha)});
    }
}

void Game::renderGameOverScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::GameOver) {
        return;
    }

    renderer.setScreenSpace();
    const bool preludeActive = deathResultPrelude_.active;
    if (renderDeathResultPrelude(renderer)) {
        return;
    }
    if (!preludeActive) {
        renderAstralEchoStarfield(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            false);
    }
    if (deathResultExitTransitionActive()) {
        return;
    }
    const UiRect panel = gameOverPanelRect();
    UiWindowScope gameOverWindow(renderer, "game_over", panel, "GAME OVER", standardMenuHelpText("決定", ""));
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "リザルト", ui::Text, 3);

    char buffer[160];
    const std::string deathCause = playerDeathCauseText(player_);
    std::snprintf(buffer, sizeof(buffer), "死因      %s", deathCause.c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, 130.0f}, buffer, {255, 214, 220, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "レベル    %d", player_.level);
    renderer.drawText(panel.pos + Vec2{136.0f, 168.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "撃破数    %d", runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "掘削数    %d", runStats_.dugTiles);
    renderer.drawText(panel.pos + Vec2{136.0f, 244.0f}, buffer, {230, 230, 236, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "入手アイテム数  %d", runStats_.acquiredItems);
    renderer.drawText(panel.pos + Vec2{136.0f, 282.0f}, buffer, {230, 230, 236, 255}, 2);

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
    UiWindowScope stageClearWindow(renderer, "stage_clear", panel, "STAGE CLEAR", standardMenuHelpText("決定", ""));
    renderer.drawText(panel.pos + Vec2{118.0f, 92.0f}, "クリア結果", ui::Text, 3);

    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "次ステージ解禁: Stage %d", unlockedStages_);
    renderer.drawText(panel.pos + Vec2{136.0f, 148.0f}, buffer, {255, 230, 150, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "クリア時間 %s   掘削数 %d   撃破数 %d",
        formatRunTime(runStats_.elapsedSeconds).c_str(),
        runStats_.dugTiles,
        runStats_.defeatedEnemies);
    renderer.drawText(panel.pos + Vec2{136.0f, 202.0f}, buffer, {198, 208, 202, 255}, 2);

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
    if (astralResult_.result == AstralRunResult::Died) {
        const bool preludeActive = deathResultPrelude_.active;
        if (renderDeathResultPrelude(renderer)) {
            return;
        }
        if (!preludeActive) {
            renderAstralEchoStarfield(
                renderer,
                {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
                false);
        }
        if (deathResultExitTransitionActive()) {
            return;
        }
    }
    const UiRect panel = stageClearPanelRect();
    UiWindowScope astralWindow(renderer, "astral_result", panel, "ASTRAL RECORD", standardMenuHelpText("決定", ""));
    renderer.drawText(panel.pos + Vec2{118.0f, 82.0f}, "星間記録", ui::Text, 3);

    char buffer[192];
    float y = panel.pos.y + 128.0f;
    const float lineStep = 34.0f;
    std::snprintf(buffer, sizeof(buffer), "結果      %s", resultText.c_str());
    renderer.drawText(panel.pos + Vec2{136.0f, y}, buffer, {255, 230, 150, 255}, 2);
    y += lineStep;
    std::snprintf(buffer, sizeof(buffer), "到達深度  %dm/%dm",
        astralResult_.reachedDepthMeters,
        astralResult_.maxDepthMeters);
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

    drawUiButton(renderer, stageClearItemRect(0), "拠点へ戻る", false, uiActionButtonStyle());
}

void Game::renderBossDefeatFlash(Renderer& renderer) const
{
    if (bossEncounter_.phase != BossEncounterPhase::DefeatFlash) {
        return;
    }

    const float alpha = bossDefeatFlashAlpha();
    if (alpha <= 0.001f) {
        return;
    }

    renderer.setScreenSpace();
    const float width = static_cast<float>(camera_.width());
    const float height = static_cast<float>(camera_.height());
    renderer.fillRect({0.0f, 0.0f}, {width, height}, {255, 255, 255, alphaByte(255.0f * alpha)});
}

bool drawStoryBossEnemyImage(
    Renderer& renderer,
    const EnemyCatalog& enemyCatalog,
    std::string_view bossEnemyId,
    Vec2 position,
    float animationTimeSeconds,
    float alpha,
    Vec2 stretchScale = {1.0f, 1.0f})
{
    const auto it = enemyCatalog.enemiesById.find(std::string(bossEnemyId));
    const EnemyDefinition* definition = it != enemyCatalog.enemiesById.end() ? &it->second : nullptr;
    if (definition == nullptr) {
        renderer.drawActorShadow(position, 112.0f, {1.0f, 0.42f}, {0, 0, 0, alphaByte(76.0f * alpha)});
        renderer.fillCircle(position + Vec2{0.0f, -32.0f}, 42.0f, {190, 96, 76, alphaByte(220.0f * alpha)});
        renderer.drawCircle(position + Vec2{0.0f, -32.0f}, 46.0f, {255, 210, 130, alphaByte(180.0f * alpha)});
        return false;
    }

    Enemy enemy;
    enemy.active = true;
    enemy.isBoss = true;
    enemy.enemyId = definition->id;
    enemy.enemyName = definition->name;
    enemy.definition = definition;
    enemy.enemyTags = definition->enemyTags;
    enemy.aiId = definition->enemyAi;
    enemy.unawareAiId = definition->unawareAiId;
    enemy.position = position;
    enemy.radius = definition->radius > 0.0 ? static_cast<float>(definition->radius) : 32.0f;
    enemy.hp = std::max(1, definition->hp);
    enemy.maxHp = enemy.hp;
    enemy.facingAngle = Pi * 0.5f;

    renderer.drawActorShadow(
        position,
        std::max(72.0f, enemy.radius * 2.25f),
        {1.0f, 0.42f},
        {0, 0, 0, alphaByte(82.0f * alpha)});

    EnemyImageDrawOptions options;
    options.tint = {255, 255, 255, alphaByte(255.0f * alpha)};
    options.stretchScale = stretchScale;
    options.directionOverrideEnabled = true;
    options.directionOverride = {0.0f, 1.0f};
    Vec2 drawSize{};
    if (!drawEnemyImage(renderer, enemy, position, animationTimeSeconds, options, &drawSize)) {
        renderer.fillCircle(position + Vec2{0.0f, -32.0f}, enemy.radius, {190, 96, 76, alphaByte(220.0f * alpha)});
        renderer.drawCircle(position + Vec2{0.0f, -32.0f}, enemy.radius + 4.0f, {255, 210, 130, alphaByte(180.0f * alpha)});
        return false;
    }
    return true;
}

void Game::appendDungeonStoryPresentationRenderEntries(
    std::vector<DepthRenderEntry>& entries,
    Renderer& renderer,
    float totalSeconds) const
{
    if (!dungeonStoryPresentation_.started ||
        dungeonStoryPresentation_.kind == DungeonStoryPresentationKind::None) {
        return;
    }

    const DungeonStoryPresentationState state = dungeonStoryPresentation_;
    entries.push_back(DepthRenderEntry{
        state.position.y,
        [this, state, &renderer, totalSeconds]() {
            if (state.kind == DungeonStoryPresentationKind::BossAfterIdle) {
                drawStoryBossEnemyImage(
                    renderer,
                    enemyCatalog_,
                    state.bossEnemyId,
                    state.position,
                    totalSeconds,
                    1.0f);
                return;
            }

            if (state.kind == DungeonStoryPresentationKind::BossAfterDefeat) {
                const ImageHandle handle = renderer.acquireImage(StoryBossSpritePath, TextureFilter::Nearest);
                Vec2 imageSize{};
                if (!handle.valid() || !renderer.getImageSize(handle, imageSize)) {
                    return;
                }

                const float frameWidth = imageSize.x / static_cast<float>(StoryBossSpriteColumns);
                const float frameHeight = imageSize.y / static_cast<float>(StoryBossSpriteRows);
                const float progress = clamp(
                    state.elapsedSeconds / std::max(0.001f, state.durationSeconds),
                    0.0f,
                    1.0f);
                const int frame = std::clamp(
                    static_cast<int>(progress * static_cast<float>(StoryBossSpriteColumns)),
                    0,
                    StoryBossSpriteColumns - 1);
                const float collapse = smoothStep01(progress);
                const float fade = progress > 0.62f ? 1.0f - smoothStep01((progress - 0.62f) / 0.38f) : 1.0f;
                const float shake = std::sin(progress * Pi * 8.0f) * 6.0f * (1.0f - progress);
                const RectF source{
                    frameWidth * static_cast<float>(frame),
                    0.0f,
                    frameWidth,
                    frameHeight,
                };
                const Vec2 drawSize{
                    frameWidth * StoryBossSpriteScale * (1.0f + 0.05f * (1.0f - collapse)),
                    frameHeight * StoryBossSpriteScale * (1.0f - 0.36f * collapse),
                };
                const Vec2 center = state.position + Vec2{shake, -34.0f + 22.0f * collapse};
                const unsigned char alpha = alphaByte(255.0f * fade);
                const unsigned char shade = alphaByte(255.0f - 92.0f * collapse);
                renderer.drawActorShadow(
                    state.position,
                    132.0f * (1.0f - 0.45f * collapse),
                    {1.0f, 0.42f},
                    {0, 0, 0, alphaByte(86.0f * fade)});
                ImageDrawOptions options = artworkImageDrawOptions();
                options.tint = {shade, shade, shade, alpha};
                options.outlineColor.a = alpha;
                renderer.drawImageRegion(handle, source, center, drawSize, options);
                return;
            }

            if (state.kind == DungeonStoryPresentationKind::SmallMoleEscape) {
                const ImageHandle handle = renderer.acquireImage(StorySmallMoleSpritePath, TextureFilter::Nearest);
                Vec2 imageSize{};
                if (!handle.valid() || !renderer.getImageSize(handle, imageSize)) {
                    return;
                }

                const Vec2 travel = state.targetPosition - state.startPosition;
                const int row = std::abs(travel.x) >= std::abs(travel.y)
                    ? (travel.x < 0.0f ? 1 : 2)
                    : (travel.y < 0.0f ? 3 : 0);
                const int frame = static_cast<int>(
                    std::floor(state.elapsedSeconds / StorySmallMoleFrameSeconds)) %
                    StorySmallMoleSpriteColumns;
                const float frameWidth = imageSize.x / static_cast<float>(StorySmallMoleSpriteColumns);
                const float frameHeight = imageSize.y / static_cast<float>(StorySmallMoleSpriteRows);
                const float progress = clamp(
                    state.elapsedSeconds / std::max(0.001f, state.durationSeconds),
                    0.0f,
                    1.0f);
                const float fade = progress > 0.82f ? 1.0f - smoothStep01((progress - 0.82f) / 0.18f) : 1.0f;
                const RectF source{
                    frameWidth * static_cast<float>(frame),
                    frameHeight * static_cast<float>(row),
                    frameWidth,
                    frameHeight,
                };
                const Vec2 drawSize{frameWidth, frameHeight};
                renderer.drawActorShadow(
                    state.position + Vec2{0.0f, 8.0f},
                    42.0f,
                    {1.0f, 0.34f},
                    {0, 0, 0, alphaByte(64.0f * fade)});
                ImageDrawOptions options = artworkImageDrawOptions();
                const unsigned char alpha = alphaByte(255.0f * fade);
                options.tint = {255, 255, 255, alpha};
                options.outlineColor.a = alpha;
                renderer.drawImageRegion(handle, source, state.position + Vec2{0.0f, -14.0f}, drawSize, options);
                return;
            }

            if (state.kind == DungeonStoryPresentationKind::BossExplodeEscape) {
                const bool beforeExplosion = state.elapsedSeconds < StoryBossExplodeEscapeWarmupSeconds;
                if (beforeExplosion) {
                    const float warmup = clamp(state.elapsedSeconds / StoryBossExplodeEscapeWarmupSeconds, 0.0f, 1.0f);
                    const float shakeAmp = 2.0f + 7.0f * warmup;
                    const Vec2 shake{
                        std::sin(state.elapsedSeconds * 38.0f) * shakeAmp,
                        std::cos(state.elapsedSeconds * 31.0f) * shakeAmp * 0.34f,
                    };
                    renderer.fillSoftCircle(
                        state.startPosition + Vec2{0.0f, -28.0f},
                        46.0f + 42.0f * warmup,
                        {255, 126, 64, alphaByte(72.0f * warmup)});
                    drawStoryBossEnemyImage(
                        renderer,
                        enemyCatalog_,
                        state.bossEnemyId,
                        state.startPosition + shake,
                        state.elapsedSeconds,
                        1.0f,
                        {1.0f + warmup * 0.06f, 1.0f - warmup * 0.05f});
                    return;
                }

                const ImageHandle handle = renderer.acquireImage(StoryCrabDishSpritePath, TextureFilter::Nearest);
                Vec2 imageSize{};
                const float dishAppearStart =
                    StoryBossExplodeEscapeWarmupSeconds +
                    StoryBossExplodeEscapePostExplosionHoldSeconds;
                if (state.elapsedSeconds < dishAppearStart) {
                    return;
                }

                const float escapeStart =
                    dishAppearStart +
                    StoryBossExplodeEscapeFadeInSeconds +
                    StoryBossExplodeEscapePostFadeHoldSeconds;
                const float fadeIn = smoothStep01(
                    (state.elapsedSeconds - dishAppearStart) / StoryBossExplodeEscapeFadeInSeconds);
                const float escapeSeconds = std::max(0.001f, state.durationSeconds - escapeStart);
                const float escapeT = clamp((state.elapsedSeconds - escapeStart) / escapeSeconds, 0.0f, 1.0f);
                const float fadeOut = escapeT > 0.82f ? 1.0f - smoothStep01((escapeT - 0.82f) / 0.18f) : 1.0f;
                const float alpha = fadeIn * fadeOut;
                if (!handle.valid() || !renderer.getImageSize(handle, imageSize) || imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
                    renderer.fillCircle(state.position + Vec2{0.0f, -13.0f}, 16.0f, {240, 86, 56, alphaByte(240.0f * alpha)});
                    renderer.drawCircle(state.position + Vec2{0.0f, -13.0f}, 19.0f, {255, 236, 196, alphaByte(210.0f * alpha)});
                    return;
                }

                const float wobble = std::sin(state.elapsedSeconds * 28.0f) * 2.0f * (1.0f - escapeT);
                renderer.drawActorShadow(
                    state.position + Vec2{0.0f, 8.0f},
                    42.0f * (1.0f - 0.18f * escapeT),
                    {1.0f, 0.34f},
                    {0, 0, 0, alphaByte(62.0f * alpha)});
                ImageDrawOptions options = artworkImageDrawOptions();
                const unsigned char imageAlpha = alphaByte(255.0f * alpha);
                options.tint = {255, 255, 255, imageAlpha};
                options.outlineColor.a = imageAlpha;
                options.rotationDegrees = wobble;
                renderer.drawImage(handle, state.position + Vec2{0.0f, -13.0f}, StoryCrabDishDrawSize, options);
            }
        },
        "WorldDepth.draw.story",
    });
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
constexpr std::string_view StardustMoleBossEnemyId = "stardust_mole";
constexpr std::string_view JunkCrabBossEnemyId = "junk_crab";
constexpr std::string_view AstragnaBossEnemyId = "astragna";
constexpr float PersistentBossArenaLightRadius =
    (static_cast<float>(BossArenaRadiusXTiles) + 0.75f) * static_cast<float>(balance::TileSize);
constexpr float AstragnaBossArenaLightRadius = (15.0f + 0.75f) * static_cast<float>(balance::TileSize);

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

bool bossUsesPersistentArenaLight(std::string_view bossEnemyId)
{
    return bossEnemyId == StardustMoleBossEnemyId ||
        bossEnemyId == JunkCrabBossEnemyId ||
        bossEnemyId == AstragnaBossEnemyId;
}

float persistentBossArenaLightRadius(std::string_view bossEnemyId)
{
    if (bossEnemyId == AstragnaBossEnemyId) {
        return AstragnaBossArenaLightRadius;
    }
    return PersistentBossArenaLightRadius;
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
        const bool bossLightAvailable = hasBossSpawnPoint_ && !hasCapturedBossForCurrentStage();
        if (bossLightAvailable && bossUsesPersistentArenaLight(currentStageDefinition_.bossEnemyId)) {
            addLight({
                bossSpawnPoint_,
                persistentBossArenaLightRadius(currentStageDefinition_.bossEnemyId),
            }, DungeonLightPriorityBoss, true);
        } else if (bossLightAvailable && !bossSpawned_) {
            addLight({
                flickeredLightPosition(bossSpawnPoint_, static_cast<float>(totalSeconds), 4.8f),
                flickeredLightRadius(120.0f, static_cast<float>(totalSeconds), 4.8f),
            }, DungeonLightPriorityBoss);
        }
        if (dungeonStoryPresentation_.started &&
            dungeonStoryPresentation_.kind != DungeonStoryPresentationKind::None) {
            addLight({
                flickeredLightPosition(dungeonStoryPresentation_.position, static_cast<float>(totalSeconds), 5.6f),
                flickeredLightRadius(168.0f, static_cast<float>(totalSeconds), 5.6f),
            }, DungeonLightPriorityBoss, true);
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
    FrameProfileScope renderProfile("Game.render");
    renderer.clear({5, 5, 8, 255});
    beginUiFrame(time.deltaSeconds(), navigationUiCursorEnabled_);
    if (mode_ == ScreenMode::OpeningKamishibai) {
        renderOpeningKamishibai(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::EndingKamishibai) {
        renderEndingKamishibai(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::Title) {
        renderTitleScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        renderObjectImageScaleEditScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        renderEnemyHitboxEditScreen(renderer, time.totalSeconds());
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::EnemyPlacementEdit) {
        renderEnemyPlacementEditScreen(renderer, time.totalSeconds());
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::EnemyShadowEdit) {
        renderEnemyShadowEditScreen(renderer, time.totalSeconds());
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
        renderer.present();
        return;
    }
    if (mode_ == ScreenMode::AudioCueEdit) {
        renderAudioCueEditScreen(renderer);
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
        renderer.present();
        return;
    }
    if (effectTestActive_) {
        renderEffectTestScreen(renderer, time.totalSeconds());
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
        renderer.present();
        return;
    }
    if (projectileTestActive_) {
        renderProjectileTestScreen(renderer, time.totalSeconds());
        finishUiFrame(renderer);
        renderDebugOverlay(renderer, time);
        renderFinalScreenOverlays(renderer);
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
        renderBaseStoryFacilityMarkers(renderer);
        renderBaseStoryFadeOverlay(renderer);
        renderDebugNamedSaveUi(renderer);
        renderDebugItemPicker(renderer);
        renderDebugStoryTest(renderer);
        renderPortraitExpressionPicker(renderer);
        renderItemAcquisitionNotice(renderer, static_cast<float>(time.totalSeconds()));
        renderLevelUpOverlay(renderer);
        renderAutoSimulationIntentOverlay(renderer);
        finishUiFrame(renderer);
        renderBaseDebugOverlay(renderer, time);
        renderScreenTransitionOverlay(renderer);
        renderWorldLoadingScreen(renderer, time.totalSeconds());
        renderDevBuildNotice(renderer);
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
    const CollisionRect worldSpriteCullBounds = expandedCollisionRect(cameraWorldRect(camera_), screenDormantMarginWorld());
    std::vector<LightSource> itemLights;
    {
        FrameProfileScope lightsCollectProfile("Lights.collect");
        itemLights = collectDungeonLightSources(time.totalSeconds());
    }
    const Vec2 playerLightCenter = witchSelfLightCenter(player_.position);
    {
        FrameProfileScope tileMapRenderProfile("TileMap.render");
        tileMap_.render(renderer, camera_, playerLightCenter, itemLights);
    }
    std::vector<DepthRenderEntry> worldDepthEntries;
    {
        FrameProfileScope worldDepthPrepareNodesProfile("WorldDepth.prepare_nodes");
        std::size_t firstEntry = worldDepthEntries.size();
        appendRewardNodeRenderEntries(worldDepthEntries, renderer, itemLights);
        tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.reward");
        if (!enemyTestActive_) {
            firstEntry = worldDepthEntries.size();
            appendDungeonEventRenderEntries(worldDepthEntries, renderer, itemLights, time.totalSeconds());
            tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.event");
        }
        firstEntry = worldDepthEntries.size();
        wetGround_.appendRenderEntries(worldDepthEntries, renderer);
        tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.wet");
        firstEntry = worldDepthEntries.size();
        groundLines_.appendRenderEntries(worldDepthEntries, renderer);
        tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.ground_line");
        firstEntry = worldDepthEntries.size();
        worldDrops_.appendRenderEntries(worldDepthEntries, renderer, tileMap_, objectCatalog_, playerLightCenter, itemLights, &worldSpriteCullBounds);
        tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.drop");
        firstEntry = worldDepthEntries.size();
        effects_.appendRenderEntries(worldDepthEntries, renderer);
        tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.fx_depth");
        firstEntry = worldDepthEntries.size();
        magicFx_.appendRenderEntries(worldDepthEntries, renderer);
        tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.magic_depth");
    }
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
        worldDrops_.renderShadows(renderer, tileMap_, objectCatalog_, playerLightCenter, itemLights, &worldSpriteCullBounds);
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
                playerStatusVisual.flipVertical,
                artworkImageDrawOptions());
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
    {
        FrameProfileScope worldDepthPrepareActorsProfile("WorldDepth.prepare_actors");
        worldDepthEntries.push_back(DepthRenderEntry{
            player_.position.y,
            [&]() {
                if (!playerDeathActive) {
                    drawPlayerVisual();
                }
            },
            "WorldDepth.draw.player",
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
                        worldDepthEntries.back().profileName = "WorldDepth.draw.ring_item";
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
                        worldDepthEntries.back().profileName = "WorldDepth.draw.ring_item";
                    }
                }
            }
        }
        std::size_t firstEntry = worldDepthEntries.size();
        enemies_.appendRenderEntries(worldDepthEntries, renderer, tileMap_, objectCatalog_, playerLightCenter, itemLights, 0, &encyclopedia_);
        tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.enemy");
        firstEntry = worldDepthEntries.size();
        appendDungeonStoryPresentationRenderEntries(worldDepthEntries, renderer, totalSeconds);
        tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.story");
        firstEntry = worldDepthEntries.size();
        appendCaptureAbsorbRenderEntries(worldDepthEntries, renderer, time.totalSeconds());
        tagDepthRenderEntries(worldDepthEntries, firstEntry, "WorldDepth.draw.capture");
    }
    {
        FrameProfileScope worldDepthDrawProfile("WorldDepth.draw");
        {
            FrameProfileScope worldDepthSortProfile("WorldDepth.sort");
            std::stable_sort(worldDepthEntries.begin(), worldDepthEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
                return left.sortY < right.sortY;
            });
        }
        {
            FrameProfileScope worldDepthEntriesDrawProfile("WorldDepth.entries_draw");
            for (const DepthRenderEntry& entry : worldDepthEntries) {
                FrameProfileScope entryProfile(entry.profileName);
                entry.draw();
            }
        }
    }

    std::vector<DepthRenderEntry> projectileDepthEntries;
    {
        FrameProfileScope projectilesRenderProfile("Projectiles.render");
        projectiles_.appendRenderEntries(projectileDepthEntries, renderer, tileMap_, playerLightCenter, itemLights);
        std::stable_sort(projectileDepthEntries.begin(), projectileDepthEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
            return left.sortY < right.sortY;
        });
        for (const DepthRenderEntry& entry : projectileDepthEntries) {
            entry.draw();
        }
    }
    {
        FrameProfileScope effectsRenderProfile("Effects.render");
        effects_.render(renderer);
    }
    {
        FrameProfileScope darknessRenderProfile("Darkness.render");
        tileMap_.renderDarknessOverlay(renderer, camera_, playerLightCenter, itemLights, lightweight);
    }
    {
        FrameProfileScope foregroundRenderProfile("Foreground.render");
        std::vector<DepthRenderEntry> magicForegroundEntries;
        magicFx_.appendForegroundRenderEntries(magicForegroundEntries, renderer);
        std::stable_sort(magicForegroundEntries.begin(), magicForegroundEntries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
            return left.sortY < right.sortY;
        });
        for (const DepthRenderEntry& entry : magicForegroundEntries) {
            entry.draw();
        }
        renderSpellRingForeground(renderer, runtimeItems, itemLights, time.totalSeconds());
        moneyGainFx_.renderForeground(renderer);
        effects_.renderForeground(renderer);
        effects_.renderDamagePopups(renderer);
        if (playerDeathActive) {
            drawPlayerVisual();
        }
        renderPendingBuriedEnemySpawnWarnings(renderer);
    }

    renderDungeonHitboxOverlay(renderer, time);

    {
        FrameProfileScope dungeonUiProfile("DungeonUI.render");
        renderer.setScreenSpace();
        const bool suppressDungeonUi = dungeonEventUiSuppressed();
        {
            FrameProfileScope hudProfile("UI.hud");
            {
                FrameProfileScope vignetteProfile("UI.vignette");
                if (!suppressDungeonUi) {
                    renderPlayerDamageVignette(renderer, time.totalSeconds());
                }
            }
            {
                FrameProfileScope topbarProfile("UI.topbar");
                renderTopInfoBar(renderer);
            }
            if (mode_ == ScreenMode::Playing && !suppressDungeonUi && dungeonMapOverlayOpen_) {
                FrameProfileScope mapOverlayProfile("UI.map_overlay");
                renderDungeonMapOverlay(renderer, itemLights);
            } else if (mode_ == ScreenMode::Playing && !suppressDungeonUi) {
                {
                    FrameProfileScope minimapProfile("UI.minimap");
                    renderDungeonMinimap(renderer, itemLights);
                }
                if (!introTutorialActive()) {
                    FrameProfileScope ringStatusProfile("UI.ring_status");
                    renderRingStatusHud(renderer);
                }
                {
                    FrameProfileScope logsProfile("UI.logs");
                    renderDungeonLogs(renderer);
                }
                {
                    FrameProfileScope playerStatusProfile("UI.player_status");
                    renderDungeonStatusHud(renderer);
                }
            }
            {
                FrameProfileScope debugPauseProfile("UI.debug_pause");
                if (debugPaused_) {
                    renderer.fillRect({18.0f, 202.0f}, {190.0f, 28.0f}, {0, 0, 0, 190});
                    renderer.drawText({28.0f, 208.0f}, "DEBUG PAUSED", {255, 230, 150, 255}, 2);
                }
            }
        }
        {
            FrameProfileScope inventoryProfile("UI.inventory");
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
                    inventory_.renderShortcutHud(
                        renderer,
                        spellRing_,
                        encyclopedia_,
                        camera_.width(),
                        camera_.height());
                    renderRingEquipFx(renderer);
                    renderDungeonControlHelp(renderer);
                } else if (mode_ == ScreenMode::Inventory && pauseReturnMode_ != ScreenMode::Base) {
                    renderDungeonLogs(renderer);
                }
            }
        }
        {
            FrameProfileScope menusProfile("UI.menus");
            if (!suppressDungeonUi && !dungeonMapOverlayOpen_) {
                renderWarpReturnUi(renderer);
                renderRoguelikeBigHoleUi(renderer);
                if (roguelikeFacilityUiActive()) {
                    renderBaseScreen(renderer);
                }
                renderPauseMenu(renderer);
                renderRingScreen(renderer, time.totalSeconds());
                renderGameOverScreen(renderer);
                renderStageClearScreen(renderer);
                renderAstralResultScreen(renderer);
                renderEnemyTestUi(renderer);
            }
        }
        {
            FrameProfileScope noticeProfile("UI.notice");
            if (!suppressDungeonUi && !dungeonMapOverlayOpen_ && reloadNoticeTimer_ > 0.0f) {
                renderer.fillRect({18.0f, 170.0f}, {430.0f, 26.0f}, {0, 0, 0, 180});
                InlineItemTextStyle noticeStyle;
                noticeStyle.text = {255, 235, 150, 255};
                noticeStyle.scale = 2;
                noticeStyle.iconTextGap = 4.0f;
                noticeStyle.iconScale = 1.15f;
                drawInlineItemText(renderer, objectCatalog_, {26.0f, 176.0f}, reloadNotice_, noticeStyle);
            }
            const bool importantNoticeMode =
                mode_ == ScreenMode::Playing ||
                ((mode_ == ScreenMode::Inventory || mode_ == ScreenMode::Ring) &&
                    pauseReturnMode_ != ScreenMode::Base);
            if (!suppressDungeonUi && !dungeonMapOverlayOpen_ && importantNoticeMode) {
                renderImportantDungeonNotices(renderer);
            }
        }
        {
            FrameProfileScope popupsProfile("UI.popups");
            if (!suppressDungeonUi && !dungeonMapOverlayOpen_ &&
                (mode_ == ScreenMode::Playing || mode_ == ScreenMode::Inventory || mode_ == ScreenMode::PauseMenu || mode_ == ScreenMode::Ring)) {
                std::vector<UiRect> encyclopediaAvoidRects;
                const float screenWidth = static_cast<float>(camera_.width());
                const float screenHeight = static_cast<float>(camera_.height());
                encyclopediaAvoidRects.push_back({{TopInfoBarX, TopInfoBarY}, {screenWidth - TopInfoBarX * 2.0f, TopInfoBarHeight + 8.0f}});
                if (reloadNoticeTimer_ > 0.0f) {
                    encyclopediaAvoidRects.push_back({{18.0f, 170.0f}, {430.0f, 26.0f}});
                }
                const bool importantNoticeMode =
                    mode_ == ScreenMode::Playing ||
                    ((mode_ == ScreenMode::Inventory || mode_ == ScreenMode::Ring) &&
                        pauseReturnMode_ != ScreenMode::Base);
                const int importantNoticeCount = std::min(
                    static_cast<int>(importantDungeonNotices_.size()),
                    ImportantDungeonNoticeMaxVisible);
                if (importantNoticeMode && importantNoticeCount > 0) {
                    encyclopediaAvoidRects.push_back(importantDungeonNoticeBlockRect(
                        screenWidth,
                        screenHeight,
                        importantNoticeCount));
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

                    encyclopediaAvoidRects.push_back(dungeonStatusHudRect(screenWidth, screenHeight));

                    int visibleLogCount = std::min(static_cast<int>(dungeonLogs_.size()), DungeonLogMaxVisible);
                    const auto logBlockHeight = [](int count) {
                        return static_cast<float>(count) * DungeonLogRowHeight +
                            static_cast<float>(std::max(0, count - 1)) * DungeonLogGap;
                    };
                    const float logTopLimit = TopInfoBarY + TopInfoBarHeight + 8.0f;
                    const float statusTopY = dungeonStatusHudRect(screenWidth, screenHeight).pos.y;
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

                encyclopedia_.renderPopups(
                    renderer,
                    camera_,
                    objectCatalog_,
                    player_.position,
                    encyclopediaAvoidRects);
            }
        }
        {
            FrameProfileScope overlaysProfile("UI.overlays");
            renderDungeonEventItemRequestUi(renderer, time.totalSeconds());
            dialogue_.render(renderer, camera_.width(), camera_.height());
            if (!suppressDungeonUi) {
                renderDebugNamedSaveUi(renderer);
                renderDebugItemPicker(renderer);
                renderDebugStoryTest(renderer);
            }
            renderPortraitExpressionPicker(renderer);
            renderItemAcquisitionNotice(renderer, static_cast<float>(time.totalSeconds()));
            renderAutoSimulationIntentOverlay(renderer);
        }
    }
    finishUiFrame(renderer);
    renderDebugOverlay(renderer, time);
    renderFinalScreenOverlays(renderer);
    renderer.present();
}

} // namespace majo
