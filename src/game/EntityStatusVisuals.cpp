#include "game/EntityStatusVisuals.hpp"

#include "engine/Renderer.hpp"
#include "game/EffectSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace majo {

namespace {

struct StatusVisualDefinition {
    std::string_view stateId;
    std::string_view displayName;
    Color tint;
    bool hasTint = true;
    Color popupColor;
};

constexpr std::array<StatusVisualDefinition, 14> StatusVisualDefinitions{{
    {"status_frozen", "氷結", {170, 232, 255, 255}, true, {118, 224, 255, 255}},
    {"status_shocked", "感電", {255, 238, 118, 255}, true, {255, 224, 74, 255}},
    {"status_paralyze", "しびれ", {255, 245, 150, 255}, true, {255, 236, 98, 255}},
    {"status_hot", "熱気", {255, 178, 132, 255}, true, {255, 132, 66, 255}},
    {"status_poison", "毒", {160, 255, 160, 255}, true, {112, 238, 104, 255}},
    {"status_bleed", "出血", {255, 138, 150, 255}, true, {255, 86, 98, 255}},
    {"status_glued", "粘着", {226, 214, 166, 255}, true, {238, 214, 126, 255}},
    {"status_slow", "鈍足", {160, 190, 255, 255}, true, {122, 176, 255, 255}},
    {"status_blind", "暗闇", {205, 195, 255, 255}, true, {194, 170, 255, 255}},
    {"status_wet", "濡れ", {170, 230, 255, 255}, true, {112, 208, 255, 255}},
    {"status_sleep", "睡眠", {205, 224, 255, 255}, true, {176, 214, 255, 255}},
    {"status_giant", "巨大化", {255, 255, 255, 255}, false, {255, 194, 92, 255}},
    {"status_stun", "スタン", {255, 255, 255, 255}, false, {255, 216, 92, 255}},
    {"status_confuse", "混乱", {255, 255, 255, 255}, false, {255, 226, 66, 255}},
}};

const StatusVisualDefinition* findStatusVisualDefinition(std::string_view stateId)
{
    for (const StatusVisualDefinition& definition : StatusVisualDefinitions) {
        if (definition.stateId == stateId) {
            return &definition;
        }
    }
    return nullptr;
}

void fillStar(Renderer& renderer, Vec2 center, float outerRadius, float innerRadius, float rotation, Color color)
{
    std::array<Vec2, 10> points{};
    for (std::size_t i = 0; i < points.size(); ++i) {
        const float radius = i % 2 == 0 ? outerRadius : innerRadius;
        const float angle = rotation - Pi * 0.5f + static_cast<float>(i) * Pi / 5.0f;
        points[i] = center + fromAngle(angle) * radius;
    }
    renderer.fillPolygon(points.data(), points.size(), color);
}

void drawConfuseStar(Renderer& renderer, Vec2 center, float radius, float rotation, float alphaScale)
{
    const auto alpha = [alphaScale](int value) {
        return static_cast<unsigned char>(std::clamp(
            std::lround(static_cast<float>(value) * alphaScale),
            0L,
            255L));
    };

    renderer.fillSoftCircle(center, radius * 1.55f, {255, 214, 80, alpha(48)});
    fillStar(
        renderer,
        center + Vec2{1.5f, 2.0f},
        radius * 1.10f,
        radius * 0.48f,
        rotation,
        {72, 48, 18, alpha(118)});
    fillStar(renderer, center, radius * 1.12f, radius * 0.48f, rotation, {126, 82, 18, alpha(230)});
    fillStar(renderer, center, radius * 0.88f, radius * 0.38f, rotation, {255, 236, 78, alpha(245)});
    fillStar(
        renderer,
        center + fromAngle(rotation - 0.95f) * (radius * 0.22f),
        radius * 0.34f,
        radius * 0.15f,
        rotation,
        {255, 255, 220, alpha(220)});
}

} // namespace

EntityStatusVisualStyle entityStatusVisualStyle(const EntityStatus& status)
{
    EntityStatusVisualStyle style;
    style.flipVertical = status.hasState("status_stun");
    style.scaleMultiplier = static_cast<float>(status.sizeMultiplierFromStates());

    for (const StatusVisualDefinition& definition : StatusVisualDefinitions) {
        if (definition.hasTint && status.hasState(definition.stateId)) {
            style.tint = definition.tint;
            style.hasTint = true;
            break;
        }
    }

    return style;
}

Vec2 entityStatusJitterOffset(const EntityStatus& status, double totalSeconds)
{
    if (!status.hasState("status_paralyze") && !status.hasState("status_shocked")) {
        return {};
    }
    const int phase = static_cast<int>(std::floor(std::max(0.0, totalSeconds) * 36.0));
    return {phase % 2 == 0 ? -1.0f : 1.0f, 0.0f};
}

std::string_view entityStatusDisplayName(std::string_view stateId)
{
    const StatusVisualDefinition* definition = findStatusVisualDefinition(stateId);
    return definition != nullptr ? definition->displayName : std::string_view{};
}

Color entityStatusPopupColor(std::string_view stateId, StatusPopupTarget target)
{
    if (target == StatusPopupTarget::Player) {
        return {255, 72, 64, 255};
    }

    const StatusVisualDefinition* definition = findStatusVisualDefinition(stateId);
    return definition != nullptr ? definition->popupColor : Color{255, 255, 255, 255};
}

bool shouldShowEntityStatusPopup(const EntityStateApplyResult& result)
{
    return result.applied && result.added;
}

void emitEntityStatusAuras(const EntityStatus& status, Vec2 position, EffectSystem& effects)
{
    for (const EntityState& state : status.states()) {
        effects.spawnStatusAura(position, state.stateId);
    }
}

void renderEntityStatusOverlays(
    Renderer& renderer,
    const EntityStatus& status,
    Vec2 footAnchor,
    float visualSize,
    double totalSeconds)
{
    if (!status.hasState("status_confuse")) {
        return;
    }

    const float size = std::max(1.0f, visualSize);
    const float orbitX = std::clamp(size * 0.16f, 7.0f, 18.0f);
    const float orbitY = std::clamp(size * 0.045f, 2.0f, 7.0f);
    const float starRadius = std::clamp(size * 0.075f, 4.2f, 9.0f);
    const Vec2 center = footAnchor + Vec2{0.0f, -size * 0.88f - starRadius * 1.6f};
    const float baseAngle = static_cast<float>(totalSeconds) * 4.25f;
    const float pulse = 0.84f + 0.16f * std::sin(static_cast<float>(totalSeconds) * 6.0f);

    for (int i = 0; i < 2; ++i) {
        const float angle = baseAngle + static_cast<float>(i) * Pi;
        const Vec2 offset{
            std::cos(angle) * orbitX,
            std::sin(angle) * orbitY,
        };
        const float depthScale = i == 0 ? 1.0f : 0.86f;
        drawConfuseStar(
            renderer,
            center + offset,
            starRadius * depthScale,
            -baseAngle * 1.35f + static_cast<float>(i) * 0.72f,
            pulse * (i == 0 ? 1.0f : 0.86f));
    }
}

} // namespace majo
