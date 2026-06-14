#include "game/SpellRingSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace majo {

namespace {
constexpr std::size_t MaxSpellRingItems = 24;
constexpr float PlacementStepRadians = Pi / 36.0f;
constexpr float ItemAngularSizeRadians = Pi / 12.0f;
constexpr float CapturedJumpInterval = 2.0f;
constexpr float CapturedJumpDuration = 0.24f;
constexpr float CapturedJumpDistance = 18.0f;
constexpr float CapturedPeriodicHealInterval = 8.0f;
constexpr int CapturedPeriodicHealMaxPerPulse = 2;
constexpr std::string_view BreakCountdownExplodeBehaviorId = "break_countdown_explode";
constexpr float FullCircleRadians = Pi * 2.0f;
constexpr double MinSlashDamageMultiplier = 0.05;
constexpr double MaxSlashDamageMultiplier = 10.0;
constexpr float MinItemOrbitDistanceOffset = -48.0f;
constexpr float MaxItemOrbitDistanceOffset = 96.0f;
constexpr std::array<float, SpellRingCount> RingBaseRadiusMultipliers{{
    1.2f,
    1.8f,
    2.4f,
}};
constexpr std::array<float, SpellRingCount> RingBaseSpeedMultipliers{{
    1.0f,
    1.0f,
    0.5f,
}};
constexpr int ThrowSourceLoopSamples = 192;
constexpr int ThrowFlightPathSamples = 128;
constexpr int ThrowAnchorPathSamples = 256;
constexpr float ThrowMasterPathPointMergeDistanceSq = 0.01f;
constexpr float ThrowMasterPathTangentSampleDistance = 3.0f;
constexpr float ThrowMasterPathVisibleSampleSpacing = 4.0f;
constexpr float ThrowMasterPathEaseOutFraction = 0.28f;
constexpr float ThrowCometReachBehindCompensation = 0.65f;
constexpr float ThrowCometReachAheadCompensation = 0.30f;
constexpr float ThrowCometReachMinScale = 0.75f;
constexpr float ThrowCometReachMaxScale = 1.40f;
constexpr float ThrowReturnSettleBackDuration = 0.30f;
constexpr float ThrowReturnSettleOscillationDuration = 0.96f;
constexpr float ThrowReturnSettleDuration = ThrowReturnSettleBackDuration + ThrowReturnSettleOscillationDuration;
constexpr float ThrowReturnSettleDistance = 12.0f;
constexpr float ThrowReturnSettleOscillationCycles = 1.5f;
constexpr float ThrowVisualEnergyFadeDuration = 0.55f;
constexpr float RingWindAcceleration = 360.0f;
constexpr float RingWindVelocityDamping = 8.5f;
constexpr float RingWindReturnRate = 7.0f;
constexpr float RingWindMaxOffset = 92.0f;
constexpr float RingWindMaxVelocity = 260.0f;

float smoothStep01(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float linearThenEaseOut01(float t, float easeOutFraction)
{
    const float clampedT = clamp(t, 0.0f, 1.0f);
    const float easeFraction = clamp(easeOutFraction, 0.001f, 0.95f);
    const float easeStart = 1.0f - easeFraction;
    if (clampedT <= easeStart) {
        return clampedT;
    }

    const float x = (clampedT - easeStart) / easeFraction;
    const float eased = x + x * x - x * x * x;
    return easeStart + easeFraction * eased;
}

void startBreakCountdownExplosion(SpellRingItem& item)
{
    RingItemBreakExplosionState state;
    state.active = true;
    state.delay = static_cast<float>(std::max(
        0.0,
        item.capturedBehaviorParamDouble(BreakCountdownExplodeBehaviorId, "delay", 4.0)));
    state.initialDelay = std::max(0.001f, state.delay);
    state.warningTickIndex = -1;
    state.radius = static_cast<float>(std::max(
        8.0,
        item.capturedBehaviorParamDouble(BreakCountdownExplodeBehaviorId, "radius", state.radius)));
    state.damage = std::max(
        0,
        item.capturedBehaviorParamInt(BreakCountdownExplodeBehaviorId, "damage", state.damage));
    state.terrainRadius = static_cast<float>(std::max(
        0.0,
        item.capturedBehaviorParamDouble(BreakCountdownExplodeBehaviorId, "terrainRadius", state.terrainRadius)));
    state.terrainDamage = std::max(
        0,
        item.capturedBehaviorParamInt(BreakCountdownExplodeBehaviorId, "terrainDamage", state.terrainDamage));
    item.breakExplosion = state;
    item.capturedExplodeCharge = 0;
    item.capturedExplodeSleepTimer = 0.0f;
    item.actionFlashTimer = SpellRingItemActionFlashSeconds;
}

float dotVec2(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

float wrap01(float value)
{
    value = std::fmod(value, 1.0f);
    if (value < 0.0f) {
        value += 1.0f;
    }
    return value;
}

Vec2 safeNormalize(Vec2 value, Vec2 fallback = {1.0f, 0.0f})
{
    if (lengthSquared(value) <= 0.0001f) {
        return fallback;
    }
    return normalize(value);
}

double finiteEquipmentMultiplier(double value)
{
    if (!std::isfinite(value)) {
        return 1.0;
    }
    return value;
}

double nonNegativeEquipmentMultiplier(double value)
{
    return std::max(0.0, finiteEquipmentMultiplier(value));
}

float scaledNonNegative(float base, double multiplier)
{
    return std::max(0.0f, base * static_cast<float>(nonNegativeEquipmentMultiplier(multiplier)));
}

float scaledAtLeast(float base, double multiplier, float minimum)
{
    return std::max(minimum, base * static_cast<float>(nonNegativeEquipmentMultiplier(multiplier)));
}

SpellRingItem makeSpellRingItem(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return makeShovel();
    case SpellRingItemType::Torch: return makeTorch();
    case SpellRingItemType::Object: return makeObjectRingItem({});
    }
    return makeObjectRingItem({});
}

struct ItemOrbitParameters {
    double slashDamageMultiplier = 1.0;
    float orbitDistanceOffset = 0.0f;
};

ItemOrbitParameters collectItemOrbitParameters(const ItemData& object)
{
    ItemOrbitParameters parameters;
    for (const EffectSpec& spec : object.orbitEffects) {
        if (spec.target != "item") {
            continue;
        }
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            const double value = index < spec.values.size() ? spec.values[index] : 0.0;
            if (spec.effects[index] == "slash_power") {
                const double multiplier = value > 0.0 && std::isfinite(value) ? value : 1.0;
                parameters.slashDamageMultiplier *= std::clamp(multiplier, MinSlashDamageMultiplier, MaxSlashDamageMultiplier);
            } else if (spec.effects[index] == "item_orbit_offset" && std::isfinite(value)) {
                parameters.orbitDistanceOffset += static_cast<float>(value);
            }
        }
    }
    parameters.orbitDistanceOffset = std::clamp(
        parameters.orbitDistanceOffset,
        MinItemOrbitDistanceOffset,
        MaxItemOrbitDistanceOffset);
    return parameters;
}

} // namespace

float normalizeAngle(float angle)
{
    angle = std::fmod(angle, FullCircleRadians);
    if (angle < 0.0f) {
        angle += FullCircleRadians;
    }
    return angle;
}

float clampCometArcRadians(const RingOrbitTuning& tuning)
{
    const float maxArcDegrees = std::clamp(tuning.cometMaxArcDegrees, 10.0f, 360.0f);
    return std::clamp(std::abs(tuning.cometArcDegrees), 10.0f, maxArcDegrees) * (Pi / 180.0f);
}

float normalizeLocalParam(RingShape shape, float param, const RingOrbitTuning& tuning)
{
    if (shape == RingShape::Comet) {
        const float halfArc = clampCometArcRadians(tuning) * 0.5f;
        return std::clamp(param, -halfArc, halfArc);
    }
    return normalizeAngle(param);
}

namespace {

float quantizeLocalParam(RingShape shape, float param, const RingOrbitTuning& tuning)
{
    if (shape == RingShape::Comet) {
        const float arc = clampCometArcRadians(tuning);
        const float halfArc = arc * 0.5f;
        const float step = std::max(Pi / 180.0f, arc / static_cast<float>(MaxSpellRingItems * 2));
        return std::clamp(std::round(param / step) * step, -halfArc, halfArc);
    }

    return normalizeAngle(std::round(normalizeAngle(param) / PlacementStepRadians) * PlacementStepRadians);
}

float pathParamDistance(RingShape shape, float a, float b, const RingOrbitTuning& tuning)
{
    if (shape == RingShape::Comet) {
        return std::fabs(normalizeLocalParam(shape, a, tuning) - normalizeLocalParam(shape, b, tuning));
    }

    float diff = std::fabs(normalizeAngle(a) - normalizeAngle(b));
    if (diff > Pi) {
        diff = FullCircleRadians - diff;
    }
    return diff;
}

float sampleParamForShape(RingShape shape, float t01, const RingOrbitTuning& tuning)
{
    if (shape == RingShape::Comet) {
        const float arc = clampCometArcRadians(tuning);
        return -arc * 0.5f + arc * clamp(t01, 0.0f, 1.0f);
    }
    return clamp(t01, 0.0f, 1.0f) * FullCircleRadians;
}

Vec2 rotateVec(Vec2 value, float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {
        value.x * c - value.y * s,
        value.x * s + value.y * c,
    };
}

int cometLaneCount(int itemCount)
{
    const int count = std::max(1, itemCount);
    return std::clamp(static_cast<int>(std::ceil(static_cast<float>(count) / 8.0f)), 1, 4);
}

float cometLaneOffset(int itemIndex, int itemCount, float laneSpacing)
{
    const int lanes = cometLaneCount(itemCount);
    const int index = std::clamp(itemIndex, 0, std::max(0, itemCount - 1));
    const int lane = lanes <= 0 ? 0 : (index % lanes);
    const float center = (static_cast<float>(lanes) - 1.0f) * 0.5f;
    return (static_cast<float>(lane) - center) * laneSpacing;
}

void applyObjectDefinition(SpellRingItem& item, const ItemData& object)
{
    const int previousDurability = item.durability;
    const ItemOrbitParameters itemOrbitParameters = collectItemOrbitParameters(object);
    item.damage = object.attackPower + item.attackBonus;
    item.damageType = object.damageType.empty() ? "none" : object.damageType;
    item.digPower = object.digPower + item.digBonus;
    item.maxDurability = object.durability < 0 ? object.durability : object.durability + item.durabilityBonus;
    item.durability = previousDurability >= 0 ? std::min(previousDurability, std::max(0, item.maxDurability)) : item.maxDurability;
    item.weight = static_cast<float>(std::max(0.0, object.weightKg * item.weightModifier));
    const SpellRingItem baseItem = makeSpellRingItem(item.type);
    item.hitRadius = static_cast<float>(std::max(1.0, static_cast<double>(baseItem.hitRadius) * item.sizeModifier));
    item.slashDamageMultiplier = itemOrbitParameters.slashDamageMultiplier;
    item.orbitDistanceOffset = itemOrbitParameters.orbitDistanceOffset;
    item.capturedBehaviorIds = object.capturedBehaviorIds;
    item.capturedBehaviorSpecs = object.capturedBehaviorSpecs;
    item.capturedBehaviorId = item.capturedBehaviorIds.empty() ? std::string{} : item.capturedBehaviorIds.front();
    item.objectVisual = effectiveItemVisualRef(object);
    item.isBroken = item.durability == 0;
    item.objectStatsApplied = true;
}

void applyItemInstance(SpellRingItem& item, const ItemInstance& instance)
{
    item.instanceId = instance.instanceId;
    item.enhanceLevel = instance.enhanceLevel;
    item.attackEnhanceLevel = instance.attackEnhanceLevel;
    item.digEnhanceLevel = instance.digEnhanceLevel;
    item.durabilityEnhanceLevel = instance.durabilityEnhanceLevel;
    item.attackBonus = instance.attackBonus;
    item.digBonus = instance.digBonus;
    item.durabilityBonus = instance.durabilityBonus;
    item.weightModifier = instance.weightModifier;
    item.sizeModifier = instance.sizeModifier;
    item.protectionEnabled = instance.protectionEnabled;
    item.addedEffects = instance.addedEffects;
    item.addedTags = instance.addedTags;
    item.maxDurability = instance.maxDurability < 0 ? instance.maxDurability : instance.maxDurability + instance.durabilityBonus;
    item.durability = instance.currentDurability;
    item.isBroken = instance.isBroken || item.durability == 0;
}

SpellRingItem makeObjectRingItemForAdd(const ItemData& item, const ItemInstance* instance)
{
    SpellRingItem ringItem = makeObjectRingItem(item.id);
    if (instance != nullptr) {
        applyItemInstance(ringItem, *instance);
    }
    applyObjectDefinition(ringItem, item);
    return ringItem;
}

}

RingShape defaultRingShapeForIndex(int ringIndex)
{
    switch (ringIndex) {
    case 0: return RingShape::Circle;
    case 1: return RingShape::FigureEight;
    case 2: return RingShape::Comet;
    default: return RingShape::Circle;
    }
}

const char* ringShapeName(RingShape shape)
{
    switch (shape) {
    case RingShape::Circle: return "Circle";
    case RingShape::FigureEight: return "FigureEight";
    case RingShape::Comet: return "Comet";
    }
    return "Circle";
}

float ringShapeOrbitSpeedMultiplier(RingShape shape, const RuntimeBalance& balance)
{
    if (shape == RingShape::Comet) {
        return std::max(0.05f, balance.cometSpeedMultiplier);
    }
    return 1.0f;
}

float ringBaseRadiusMultiplierForIndex(int ringIndex)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return 1.0f;
    }
    return std::max(0.1f, RingBaseRadiusMultipliers[static_cast<std::size_t>(ringIndex)]);
}

float ringBaseSpeedMultiplierForIndex(int ringIndex)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return 1.0f;
    }
    return std::max(0.05f, RingBaseSpeedMultipliers[static_cast<std::size_t>(ringIndex)]);
}

float levelRingScaleMultiplierForPoints(int points)
{
    return 1.0f + SpellRingSystem::LevelRingScaleUpgradeAmount * static_cast<float>(std::max(0, points));
}

RingOrbitTuning makeRingOrbitTuning(const RuntimeBalance& balance)
{
    RingOrbitTuning tuning;
    tuning.figure8WidthMultiplier = std::max(0.1f, balance.figure8WidthMultiplier);
    tuning.figure8HeightMultiplier = std::max(0.1f, balance.figure8HeightMultiplier);
    tuning.figure8ShapeRotationSpeed = std::max(0.0f, balance.figure8ShapeRotationSpeed);
    tuning.cometRadiusMultiplier = std::max(0.1f, balance.cometRadiusMultiplier);
    tuning.cometArcDegrees = std::max(10.0f, balance.cometArcDegrees);
    tuning.cometSpeedMultiplier = std::max(0.05f, balance.cometSpeedMultiplier);
    tuning.cometTrailLength = std::clamp(balance.cometTrailLength, 0.0f, 1.0f);
    tuning.cometLaneSpacing = std::max(0.0f, balance.cometLaneSpacing);
    tuning.cometMaxArcDegrees = std::clamp(balance.cometMaxArcDegrees, 10.0f, 360.0f);
    if (tuning.cometArcDegrees > tuning.cometMaxArcDegrees) {
        tuning.cometArcDegrees = tuning.cometMaxArcDegrees;
    }
    return tuning;
}

Vec2 getRingCenterWorldPosition(Vec2 playerPosition, Vec2 shiftDirection, float spellRingShift)
{
    const Vec2 baseCenter = witchSelfLightCenter(playerPosition);
    if (std::abs(spellRingShift) <= 0.001f) {
        return baseCenter;
    }
    return baseCenter + safeNormalize(shiftDirection) * spellRingShift;
}

namespace {

Vec2 getRingCenterWorldPositionForFocus(
    Vec2 playerPosition,
    Vec2 shiftDirection,
    float spellRingShift,
    int ringIndex,
    int focusedRingIndex,
    bool shiftAllowed)
{
    const float focusedShift = shiftAllowed && ringIndex == focusedRingIndex ? spellRingShift : 0.0f;
    return getRingCenterWorldPosition(playerPosition, shiftDirection, focusedShift);
}

}

Vec2 getRingItemLocalPosition(float localAngle, const RingOrbitContext& context)
{
    const RingOrbitTuning tuning = context.tuning;
    if (context.shape == RingShape::FigureEight) {
        const float t = normalizeAngle(localAngle);
        const float width = context.radius * std::max(0.1f, tuning.figure8WidthMultiplier);
        const float height = context.radius * std::max(0.1f, tuning.figure8HeightMultiplier);
        const Vec2 figureEight{
            std::sin(t) * width,
            std::sin(t) * std::cos(t) * height,
        };
        return rotateVec(figureEight, context.shapeRotation);
    }

    if (context.shape == RingShape::Comet) {
        const float arc = clampCometArcRadians(tuning);
        const float halfArc = arc * 0.5f;
        const float clamped = std::clamp(localAngle, -halfArc, halfArc);
        const float trail01 = halfArc > 0.0f ? (clamped + halfArc) / arc : 0.5f;
        const float tailFactor = 1.0f - trail01;
        const float trailInset = context.radius * std::clamp(tuning.cometTrailLength, 0.0f, 1.0f) * 0.35f * tailFactor;
        const float laneOffset = cometLaneOffset(context.itemIndex, context.itemCount, tuning.cometLaneSpacing);
        const float radial = context.radius * std::max(0.1f, tuning.cometRadiusMultiplier) + laneOffset - trailInset;
        return fromAngle(context.shapeRotation + clamped) * radial;
    }

    return fromAngle(localAngle) * context.radius;
}

Vec2 applyItemOrbitDistanceOffset(Vec2 localPosition, float localAngle, float distanceOffset)
{
    if (std::abs(distanceOffset) < 0.001f) {
        return localPosition;
    }
    const Vec2 outward = lengthSquared(localPosition) > 0.0001f
        ? normalize(localPosition)
        : fromAngle(localAngle);
    return localPosition + outward * distanceOffset;
}

Vec2 getRingItemLocalPositionWithDistanceOffset(
    float localAngle,
    const RingOrbitContext& context,
    float distanceOffset)
{
    return applyItemOrbitDistanceOffset(getRingItemLocalPosition(localAngle, context), localAngle, distanceOffset);
}

Vec2 getRingItemWorldPosition(Vec2 center, float localAngle, const RingOrbitContext& context)
{
    return center + getRingItemLocalPosition(localAngle, context);
}

Vec2 getRingItemWorldPositionWithDistanceOffset(
    Vec2 center,
    float localAngle,
    const RingOrbitContext& context,
    float distanceOffset)
{
    return center + getRingItemLocalPositionWithDistanceOffset(localAngle, context, distanceOffset);
}

Vec2 getRingItemVelocity(
    float localAngle,
    float localAngularSpeed,
    float shapeRotationSpeed,
    Vec2 centerVelocity,
    const RingOrbitContext& context)
{
    constexpr float SampleDt = 1.0f / 240.0f;
    const Vec2 before = getRingItemLocalPosition(localAngle, context);
    RingOrbitContext nextContext = context;
    nextContext.shapeRotation += shapeRotationSpeed * SampleDt;
    const Vec2 after = getRingItemLocalPosition(localAngle + localAngularSpeed * SampleDt, nextContext);
    return centerVelocity + (after - before) / SampleDt;
}

Vec2 getRingItemVelocityWithDistanceOffset(
    float localAngle,
    float localAngularSpeed,
    float shapeRotationSpeed,
    Vec2 centerVelocity,
    const RingOrbitContext& context,
    float distanceOffset)
{
    constexpr float SampleDt = 1.0f / 240.0f;
    const Vec2 before = getRingItemLocalPositionWithDistanceOffset(localAngle, context, distanceOffset);
    RingOrbitContext nextContext = context;
    nextContext.shapeRotation += shapeRotationSpeed * SampleDt;
    const Vec2 after = getRingItemLocalPositionWithDistanceOffset(
        localAngle + localAngularSpeed * SampleDt,
        nextContext,
        distanceOffset);
    return centerVelocity + (after - before) / SampleDt;
}

std::vector<Vec2> getRingPathSamplePoints(Vec2 center, const RingOrbitContext& context, int sampleCount)
{
    const int count = std::max(8, sampleCount);
    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float t01 = static_cast<float>(i) / static_cast<float>(count - 1);
        const float param = sampleParamForShape(context.shape, t01, context.tuning);
        points.push_back(getRingItemWorldPosition(center, param, context));
    }
    return points;
}

float findNearestRingPathParam(Vec2 worldPoint, Vec2 center, const RingOrbitContext& context, int sampleCount)
{
    const int count = std::max(32, sampleCount);
    float nearestParam = 0.0f;
    float nearestDistanceSq = std::numeric_limits<float>::max();
    for (int i = 0; i < count; ++i) {
        const float t01 = static_cast<float>(i) / static_cast<float>(count - 1);
        const float param = sampleParamForShape(context.shape, t01, context.tuning);
        const Vec2 sample = getRingItemWorldPosition(center, param, context);
        const float distSq = distanceSquared(worldPoint, sample);
        if (distSq < nearestDistanceSq) {
            nearestDistanceSq = distSq;
            nearestParam = param;
        }
    }
    return nearestParam;
}

float pathTForParam(RingShape shape, float param, const RingOrbitTuning& tuning)
{
    if (shape == RingShape::Comet) {
        const float arc = clampCometArcRadians(tuning);
        const float halfArc = arc * 0.5f;
        return arc > 0.0001f ? clamp((std::clamp(param, -halfArc, halfArc) + halfArc) / arc, 0.0f, 1.0f) : 0.0f;
    }
    return normalizeAngle(param) / FullCircleRadians;
}

float cometPathParamForT(float t, const RingOrbitTuning& tuning)
{
    const float arc = clampCometArcRadians(tuning);
    return -arc * 0.5f + arc * clamp(t, 0.0f, 1.0f);
}

float pathParamForT(RingShape shape, float t, const RingOrbitTuning& tuning)
{
    if (shape == RingShape::Comet) {
        return cometPathParamForT(t, tuning);
    }
    return wrap01(t) * FullCircleRadians;
}

float pathTClosestToDirection(Vec2 center, Vec2 direction, const RingOrbitContext& context)
{
    if (context.shape == RingShape::Comet) {
        return 1.0f;
    }

    const Vec2 unitDirection = safeNormalize(direction);
    constexpr int SampleCount = 160;
    float bestT = 0.0f;
    float bestScore = -std::numeric_limits<float>::max();
    for (int i = 0; i < SampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(SampleCount);
        const float param = t * FullCircleRadians;
        const Vec2 point = getRingItemWorldPosition(center, param, context);
        const float score = dotVec2(safeNormalize(point - center, unitDirection), unitDirection);
        if (score > bestScore) {
            bestScore = score;
            bestT = t;
        }
    }
    return bestT;
}

Vec2 screenCounterClockwise(Vec2 direction)
{
    return safeNormalize(Vec2{direction.y, -direction.x}, Vec2{0.0f, -1.0f});
}

struct ThrowAnchorPathTs {
    float launchPathT = 0.0f;
    float returnPathT = 0.5f;
};

float pathTAtProjectionExtreme(
    Vec2 center,
    Vec2 axis,
    Vec2 tieBreakDirection,
    const RingOrbitContext& context,
    bool maximize)
{
    const Vec2 unitAxis = safeNormalize(axis);
    const Vec2 unitTieBreak = safeNormalize(tieBreakDirection);
    constexpr float ProjectionTieEpsilon = 0.01f;
    float bestT = 0.0f;
    float bestProjection = maximize
        ? -std::numeric_limits<float>::max()
        : std::numeric_limits<float>::max();
    float bestTieBreak = -std::numeric_limits<float>::max();

    for (int i = 0; i < ThrowAnchorPathSamples; ++i) {
        const float pathT = static_cast<float>(i) / static_cast<float>(ThrowAnchorPathSamples);
        const float param = pathParamForT(context.shape, pathT, context.tuning);
        const Vec2 offset = getRingItemWorldPosition(center, param, context) - center;
        const float projection = dotVec2(offset, unitAxis);
        const float tieBreak = dotVec2(offset, unitTieBreak);
        const bool projectionWins = maximize
            ? projection > bestProjection + ProjectionTieEpsilon
            : projection < bestProjection - ProjectionTieEpsilon;
        const bool tieBreakWins = std::abs(projection - bestProjection) <= ProjectionTieEpsilon &&
            tieBreak > bestTieBreak;
        if (projectionWins || tieBreakWins) {
            bestProjection = projection;
            bestTieBreak = tieBreak;
            bestT = pathT;
        }
    }

    return bestT;
}

ThrowAnchorPathTs throwAnchorPathTs(Vec2 center, Vec2 throwDirection, const RingOrbitContext& context)
{
    const Vec2 launchAxis = screenCounterClockwise(throwDirection);
    if (context.shape == RingShape::Comet) {
        return {1.0f, 0.0f};
    }
    if (context.shape == RingShape::FigureEight) {
        return {
            pathTAtProjectionExtreme(center, launchAxis, throwDirection, context, true),
            pathTAtProjectionExtreme(center, launchAxis, throwDirection, context, false),
        };
    }

    const float launchPathT = pathTClosestToDirection(center, launchAxis, context);
    return {launchPathT, wrap01(launchPathT + 0.5f)};
}

Vec2 reflectedCometReturnOffset(Vec2 launchOffset, Vec2 throwDirection)
{
    const Vec2 axis = safeNormalize(throwDirection);
    const Vec2 alongAxis = axis * dotVec2(launchOffset, axis);
    const Vec2 normal = launchOffset - alongAxis;
    return alongAxis - normal;
}

float cometThrowReachDistance(Vec2 launchOffset, Vec2 throwDirection, float reachDistance)
{
    if (reachDistance <= 0.001f) {
        return 0.0f;
    }

    const Vec2 unitDirection = safeNormalize(throwDirection);
    const float launchForward = dotVec2(launchOffset, unitDirection);
    const float compensation = launchForward < 0.0f
        ? ThrowCometReachBehindCompensation
        : ThrowCometReachAheadCompensation;
    const float adjustedReach = reachDistance - launchForward * compensation;
    return std::clamp(
        adjustedReach,
        reachDistance * ThrowCometReachMinScale,
        reachDistance * ThrowCometReachMaxScale);
}

Vec2 throwFlightPathPoint(
    Vec2 homeCenter,
    Vec2 launchOffset,
    Vec2 returnOffset,
    Vec2 direction,
    float reachDistance,
    float pathT)
{
    const float t = clamp(pathT, 0.0f, 1.0f);
    const Vec2 anchor = homeCenter + lerp(launchOffset, returnOffset, t);
    return anchor + direction * (4.0f * t * (1.0f - t) * reachDistance);
}

struct SourceMaterialPath {
    std::vector<Vec2> points;
    std::vector<float> sourceU;
    std::vector<float> cumulativeLengths;
    float totalLength = 0.0f;
};

struct ThrowMasterPath {
    std::vector<Vec2> points;
    std::vector<float> cumulativeLengths;
    SourceMaterialPath source;
    float totalLength = 0.0f;
    float sourceLoopLength = 0.0f;
    float flightLength = 0.0f;
    float visibleLength = 0.0f;
    float finishTailDistance = 0.0f;
};

struct ThrowVisibleWindow {
    float tailDistance = 0.0f;
    float visibleLength = 0.0f;
};

void appendMasterPathPoint(ThrowMasterPath& path, Vec2 point)
{
    if (path.points.empty()) {
        path.points.push_back(point);
        path.cumulativeLengths.push_back(0.0f);
        return;
    }

    const float segmentLength = length(point - path.points.back());
    if (segmentLength * segmentLength <= ThrowMasterPathPointMergeDistanceSq) {
        path.points.back() = point;
        return;
    }

    path.totalLength += segmentLength;
    path.points.push_back(point);
    path.cumulativeLengths.push_back(path.totalLength);
}

void appendSourceMaterialPoint(SourceMaterialPath& path, float sourceU, Vec2 point)
{
    const float clampedU = clamp(sourceU, 0.0f, 1.0f);
    if (path.points.empty()) {
        path.points.push_back(point);
        path.sourceU.push_back(clampedU);
        path.cumulativeLengths.push_back(0.0f);
        return;
    }

    path.totalLength += length(point - path.points.back());
    path.points.push_back(point);
    path.sourceU.push_back(clampedU);
    path.cumulativeLengths.push_back(path.totalLength);
}

float closedThrowSourceLoopPathT(float launchPathT, float u)
{
    return wrap01(launchPathT + clamp(u, 0.0f, 1.0f));
}

float closedThrowReturnLoopPathT(float returnPathT, float u)
{
    return wrap01(returnPathT + clamp(u, 0.0f, 1.0f));
}

float cometThrowLoopPathT(float startPathT, float oppositePathT, float u)
{
    const float clampedU = clamp(u, 0.0f, 1.0f);
    if (clampedU <= 0.5f) {
        return lerp(startPathT, oppositePathT, clampedU * 2.0f);
    }
    return lerp(oppositePathT, startPathT, (clampedU - 0.5f) * 2.0f);
}

float throwSourceLoopPathTForU(RingShape shape, float launchPathT, float returnPathT, float u)
{
    if (shape == RingShape::Comet) {
        return clamp(u, 0.0f, 1.0f);
    }
    return closedThrowSourceLoopPathT(launchPathT, u);
}

float throwReturnLoopPathTForU(RingShape shape, float launchPathT, float returnPathT, float u)
{
    if (shape == RingShape::Comet) {
        return cometThrowLoopPathT(returnPathT, launchPathT, u);
    }
    return closedThrowReturnLoopPathT(returnPathT, u);
}

float throwSourceUForPathT(RingShape shape, float pathT, float launchPathT, float returnPathT)
{
    if (shape == RingShape::Comet) {
        return clamp(pathT, 0.0f, 1.0f);
    }

    return wrap01(pathT - launchPathT);
}

Vec2 ringPathPointAtT(Vec2 center, const RingOrbitContext& context, float pathT)
{
    return getRingItemWorldPosition(center, pathParamForT(context.shape, pathT, context.tuning), context);
}

float sampleSourceMaterialDistance(const ThrowMasterPath& path, RingShape shape, float pathT, float launchPathT, float returnPathT)
{
    if (path.source.sourceU.empty() || path.source.cumulativeLengths.empty()) {
        return 0.0f;
    }

    const float sourceU = throwSourceUForPathT(shape, pathT, launchPathT, returnPathT);
    const auto upper = std::lower_bound(path.source.sourceU.begin(), path.source.sourceU.end(), sourceU);
    if (upper == path.source.sourceU.begin()) {
        return path.source.cumulativeLengths.front();
    }
    if (upper == path.source.sourceU.end()) {
        return path.source.cumulativeLengths.back();
    }

    const std::size_t index = static_cast<std::size_t>(upper - path.source.sourceU.begin());
    const float u0 = path.source.sourceU[index - 1];
    const float u1 = path.source.sourceU[index];
    const float d0 = path.source.cumulativeLengths[index - 1];
    const float d1 = path.source.cumulativeLengths[index];
    if (std::abs(u1 - u0) <= 0.0001f) {
        return d1;
    }

    return lerp(d0, d1, clamp((sourceU - u0) / (u1 - u0), 0.0f, 1.0f));
}

Vec2 sampleMasterPathByDistance(const ThrowMasterPath& path, float distance)
{
    if (path.points.empty()) {
        return {};
    }
    if (path.points.size() == 1 || path.totalLength <= 0.0001f) {
        return path.points.front();
    }

    const float clampedDistance = clamp(distance, 0.0f, path.totalLength);
    if (clampedDistance >= path.totalLength) {
        return path.points.back();
    }

    const auto upper = std::lower_bound(path.cumulativeLengths.begin(), path.cumulativeLengths.end(), clampedDistance);
    if (upper == path.cumulativeLengths.begin()) {
        return path.points.front();
    }
    if (upper == path.cumulativeLengths.end()) {
        return path.points.back();
    }

    const std::size_t index = static_cast<std::size_t>(upper - path.cumulativeLengths.begin());
    const float d0 = path.cumulativeLengths[index - 1];
    const float d1 = path.cumulativeLengths[index];
    if (d1 - d0 <= 0.0001f) {
        return path.points[index];
    }

    return lerp(path.points[index - 1], path.points[index], (clampedDistance - d0) / (d1 - d0));
}

float throwFixedTailDistanceForElapsed(const ThrowMasterPath& path, float elapsed, float flightTime, float chaseTime)
{
    if (path.finishTailDistance <= 0.0001f) {
        return 0.0f;
    }

    const float safeFlightTime = std::max(0.02f, flightTime);
    if (elapsed <= safeFlightTime) {
        return path.flightLength * clamp(elapsed / safeFlightTime, 0.0f, 1.0f);
    }

    const float safeChaseTime = std::max(0.02f, chaseTime);
    const float chasePhase = linearThenEaseOut01(
        (elapsed - safeFlightTime) / safeChaseTime,
        ThrowMasterPathEaseOutFraction);
    return path.flightLength + path.sourceLoopLength * chasePhase;
}

float throwCometReturnProgress01(const ThrowMasterPath& path, float elapsed, float flightTime, float chaseTime)
{
    if (path.visibleLength <= 0.0001f) {
        return 0.0f;
    }

    const float safeFlightTime = std::max(0.02f, flightTime);
    const float safeChaseTime = std::max(0.02f, chaseTime);
    if (elapsed <= safeFlightTime) {
        return 0.0f;
    }

    const float flightSpeed = path.flightLength / safeFlightTime;
    const float averageSpeed = path.visibleLength / safeChaseTime;
    const float normalSpeed = std::max(0.0f, averageSpeed * 2.0f - flightSpeed);
    const float denominator = (flightSpeed + normalSpeed) * 0.5f;
    const float u = clamp((elapsed - safeFlightTime) / safeChaseTime, 0.0f, 1.0f);
    if (denominator <= 0.001f) {
        return u;
    }

    const float easeIntegral = u * u * u - 0.5f * u * u * u * u;
    return clamp((flightSpeed * u + (normalSpeed - flightSpeed) * easeIntegral) / denominator, 0.0f, 1.0f);
}

float throwCometTailDistanceForElapsed(const ThrowMasterPath& path, float elapsed, float flightTime, float chaseTime)
{
    if (path.finishTailDistance <= 0.0001f) {
        return 0.0f;
    }

    const float safeFlightTime = std::max(0.02f, flightTime);
    if (elapsed <= safeFlightTime) {
        return path.flightLength * clamp(elapsed / safeFlightTime, 0.0f, 1.0f);
    }

    return path.flightLength + path.visibleLength * throwCometReturnProgress01(path, elapsed, flightTime, chaseTime);
}

ThrowVisibleWindow throwVisibleWindowForElapsed(
    const ThrowMasterPath& path,
    RingShape shape,
    float elapsed,
    float flightTime,
    float chaseTime)
{
    ThrowVisibleWindow window;
    window.tailDistance = shape == RingShape::Comet
        ? throwCometTailDistanceForElapsed(path, elapsed, flightTime, chaseTime)
        : throwFixedTailDistanceForElapsed(path, elapsed, flightTime, chaseTime);
    window.visibleLength = path.visibleLength;
    return window;
}

float throwMasterPathDistanceForMaterialDistance(
    const ThrowMasterPath& path,
    const ThrowVisibleWindow& window,
    float materialDistance)
{
    if (path.totalLength <= 0.0001f || path.visibleLength <= 0.0001f) {
        return 0.0f;
    }

    const float materialT = clamp(materialDistance / path.visibleLength, 0.0f, 1.0f);
    return clamp(window.tailDistance + std::max(0.0f, window.visibleLength) * materialT, 0.0f, path.totalLength);
}

Vec2 sampleThrowMasterPathPoint(
    const ThrowMasterPath& path,
    const ThrowVisibleWindow& window,
    float materialDistance,
    Vec2 homeCenter,
    Vec2 fallbackOutward,
    float distanceOffset)
{
    const Vec2 point = sampleMasterPathByDistance(
        path,
        throwMasterPathDistanceForMaterialDistance(path, window, materialDistance));
    if (std::abs(distanceOffset) <= 0.001f) {
        return point;
    }

    return point + safeNormalize(point - homeCenter, fallbackOutward) * distanceOffset;
}

std::vector<Vec2> sampleThrowMasterVisiblePoints(
    const ThrowMasterPath& path,
    const ThrowVisibleWindow& window,
    Vec2 homeCenter,
    Vec2 fallbackOutward,
    int sampleCount)
{
    if (path.points.size() < 2 || path.visibleLength <= 0.0001f) {
        return path.points;
    }

    const int count = std::max(
        8,
        std::max(
            sampleCount,
            static_cast<int>(std::ceil(std::max(0.0f, window.visibleLength) / ThrowMasterPathVisibleSampleSpacing)) + 1));
    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(count - 1);
        points.push_back(sampleThrowMasterPathPoint(path, window, path.visibleLength * t, homeCenter, fallbackOutward, 0.0f));
    }
    return points;
}

ThrowMasterPath makeThrowMasterPath(
    Vec2 homeCenter,
    const RingOrbitContext& context,
    Vec2 direction,
    Vec2 launchOffset,
    Vec2 returnOffset,
    float launchPathT,
    float returnPathT,
    float reachDistance)
{
    ThrowMasterPath path;
    const Vec2 unitDirection = safeNormalize(direction);
    const float flightReachDistance = context.shape == RingShape::Comet
        ? cometThrowReachDistance(launchOffset, unitDirection, reachDistance)
        : reachDistance;

    for (int i = 0; i <= ThrowSourceLoopSamples; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(ThrowSourceLoopSamples);
        const float pathT = throwSourceLoopPathTForU(context.shape, launchPathT, returnPathT, u);
        const Vec2 sourcePoint = ringPathPointAtT(homeCenter, context, pathT);
        appendSourceMaterialPoint(path.source, u, sourcePoint);
        appendMasterPathPoint(path, sourcePoint);
    }
    path.sourceLoopLength = path.source.totalLength;
    path.visibleLength = path.source.totalLength;

    const float flightStartLength = path.totalLength;
    for (int i = 1; i <= ThrowFlightPathSamples; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(ThrowFlightPathSamples);
        appendMasterPathPoint(
            path,
            throwFlightPathPoint(
                homeCenter,
                launchOffset,
                returnOffset,
                unitDirection,
                flightReachDistance,
                u));
    }
    path.flightLength = path.totalLength - flightStartLength;
    path.finishTailDistance = path.sourceLoopLength + path.flightLength;

    if (context.shape == RingShape::Comet) {
        const float returnRadius = std::max(1.0f, length(returnOffset));
        const float returnStartAngle = std::atan2(returnOffset.y, returnOffset.x);
        const float returnArcRadians = path.visibleLength / returnRadius;
        for (int i = 1; i <= ThrowSourceLoopSamples; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(ThrowSourceLoopSamples);
            appendMasterPathPoint(
                path,
                homeCenter + fromAngle(returnStartAngle + returnArcRadians * u) * returnRadius);
        }
    } else {
        for (int i = 1; i <= ThrowSourceLoopSamples; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(ThrowSourceLoopSamples);
            const float pathT = throwReturnLoopPathTForU(context.shape, launchPathT, returnPathT, u);
            appendMasterPathPoint(path, ringPathPointAtT(homeCenter, context, pathT));
        }
    }

    return path;
}

float throwFlightTimeForPath(const ThrowMasterPath& path, float throwSpeed, float maxTime)
{
    const float cappedMaxTime = std::max(0.02f, maxTime);
    if (throwSpeed <= 0.001f) {
        return cappedMaxTime;
    }
    return std::max(0.02f, std::min(cappedMaxTime, path.flightLength / throwSpeed));
}

float throwChaseTimeForPath(const ThrowMasterPath& path, float returnSpeed, float fallbackTime)
{
    if (returnSpeed <= 0.001f) {
        return std::max(0.02f, fallbackTime);
    }
    return std::max(0.02f, path.sourceLoopLength / returnSpeed);
}

float throwCometChaseTimeForPath(
    const ThrowMasterPath& path,
    float flightTime,
    float normalAngularSpeed,
    float returnRadius,
    float fallbackTime)
{
    const float flightSpeed = path.flightLength / std::max(0.02f, flightTime);
    const float normalSpeed = std::abs(normalAngularSpeed) * std::max(1.0f, returnRadius);
    const float averageSpeed = (flightSpeed + normalSpeed) * 0.5f;
    if (averageSpeed <= 0.001f) {
        return std::max(0.02f, fallbackTime);
    }
    return std::max(0.02f, path.visibleLength / averageSpeed);
}

float throwTotalTime(float flightTime, float chaseTime)
{
    return std::max(0.04f, std::max(0.02f, flightTime) + std::max(0.02f, chaseTime));
}

float throwReturnSettleStartTime(float flightTime)
{
    return std::max(0.02f, flightTime);
}

float throwCompleteTime(RingShape shape, float flightTime, float chaseTime, float settleDuration)
{
    const float tailFinishTime = throwTotalTime(flightTime, chaseTime);
    if (shape == RingShape::Comet) {
        return tailFinishTime;
    }

    const float settleFinishTime = throwReturnSettleStartTime(flightTime) + std::max(0.0f, settleDuration);
    return std::max(tailFinishTime, settleFinishTime);
}

Vec2 throwReturnSettleOffset(Vec2 direction, float elapsed, float flightTime, float settleDuration)
{
    const float safeSettleDuration = std::max(0.0f, settleDuration);
    if (safeSettleDuration <= 0.001f) {
        return {};
    }

    const float settleElapsed = elapsed - throwReturnSettleStartTime(flightTime);
    if (settleElapsed <= 0.0f || settleElapsed >= safeSettleDuration) {
        return {};
    }

    const float backDuration = std::min(ThrowReturnSettleBackDuration, safeSettleDuration);
    float recoilDistance = 0.0f;
    if (settleElapsed < backDuration) {
        const float t = clamp(settleElapsed / std::max(0.001f, backDuration), 0.0f, 1.0f);
        recoilDistance = -ThrowReturnSettleDistance * smoothStep01(t);
    } else {
        const float oscillationDuration = std::max(0.001f, safeSettleDuration - backDuration);
        const float t = clamp((settleElapsed - backDuration) / oscillationDuration, 0.0f, 1.0f);
        const float envelope = 1.0f - smoothStep01(t);
        recoilDistance = -ThrowReturnSettleDistance
            * envelope
            * std::cos(t * FullCircleRadians * ThrowReturnSettleOscillationCycles);
    }
    return safeNormalize(direction) * recoilDistance;
}

Vec2 throwPresentationOrigin(Vec2 homeCenter, Vec2 direction, float elapsed, float flightTime, float settleDuration)
{
    return homeCenter + throwReturnSettleOffset(direction, elapsed, flightTime, settleDuration);
}

std::vector<SpellRingItem>& SpellRingSystem::activeItems()
{
    return itemsByRing_[static_cast<std::size_t>(activeRingIndex_)];
}

const std::vector<SpellRingItem>& SpellRingSystem::activeItems() const
{
    return itemsByRing_[static_cast<std::size_t>(activeRingIndex_)];
}

void SpellRingSystem::initialize(const RuntimeBalance& balance)
{
    for (auto& ringItems : itemsByRing_) {
        ringItems.clear();
    }
    SpellRingItem shovel = makeShovel();
    shovel.ringIndex = 0;
    shovel.localAngle = 0.0f;
    SpellRingItem torch = makeTorch();
    torch.ringIndex = 0;
    torch.localAngle = Pi;
    itemsByRing_[0].push_back(std::move(shovel));
    itemsByRing_[0].push_back(std::move(torch));

    for (int i = 0; i < SpellRingCount; ++i) {
        ringShapes_[static_cast<std::size_t>(i)] = defaultRingShapeForIndex(i);
    }

    radii_.fill(balance.spellRingRadius);
    angularSpeeds_.fill(balance.spellRingSpeed);
    orbitTuning_ = makeRingOrbitTuning(balance);
    maxEquippedWeights_ = SpellRingSystem::InitialMaxEquippedWeightsByRing;
    baseAngles_.fill(0.0f);
    shapeRotations_.fill(0.0f);
    ringRuntime_ = {};
    orbitModifiers_ = OrbitModifiers{};
    equipmentModifiers_ = EquipmentModifiers{};
    workshopModifiersByRing_.fill(RingWorkshopModifiers{});
    capturedHealTimer_ = CapturedPeriodicHealInterval;
    enemyOrbitSpeedDebuffMultiplier_ = 1.0f;
    enemyOrbitSpeedDebuffTimer_ = 0.0f;
    activeRingIndex_ = 0;
    throwingRingIndex_ = -1;
    itemBreakEvents_.clear();
    motionEvents_.clear();
}

float SpellRingSystem::levelScaleMultiplierForPoints(int points)
{
    return levelRingScaleMultiplierForPoints(points);
}

float SpellRingSystem::initialMaxEquippedWeightForRing(int ringIndex)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return InitialMaxEquippedWeight;
    }
    return InitialMaxEquippedWeightsByRing[static_cast<std::size_t>(ringIndex)];
}

float SpellRingSystem::weightStopRatioForPenaltyMultiplier(double penaltyMultiplier)
{
    penaltyMultiplier = nonNegativeEquipmentMultiplier(penaltyMultiplier);
    if (penaltyMultiplier <= 0.0) {
        return MaxWeightStopRatio;
    }

    const float penaltyRange = BaseWeightStopRatio - 1.0f;
    const float adjustedStopRatio = 1.0f + penaltyRange / static_cast<float>(penaltyMultiplier);
    return std::clamp(adjustedStopRatio, 1.0f, MaxWeightStopRatio);
}

float SpellRingSystem::baseRadiusMultiplierForRing(int ringIndex)
{
    return ringBaseRadiusMultiplierForIndex(ringIndex);
}

float SpellRingSystem::baseSpeedMultiplierForRing(int ringIndex)
{
    return ringBaseSpeedMultiplierForIndex(ringIndex);
}

void SpellRingSystem::upgradeRadius(float factor)
{
    upgradeRadiusForRing(activeRingIndex_, factor);
}

void SpellRingSystem::upgradeSpeed(float factor)
{
    upgradeSpeedForRing(activeRingIndex_, factor);
}

void SpellRingSystem::upgradeRadiusForRing(int ringIndex, float factor)
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    radii_[static_cast<std::size_t>(clampedRingIndex)] =
        std::max(0.0f, radii_[static_cast<std::size_t>(clampedRingIndex)] * factor);
}

void SpellRingSystem::upgradeSpeedForRing(int ringIndex, float factor)
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    angularSpeeds_[static_cast<std::size_t>(clampedRingIndex)] =
        std::max(0.0f, angularSpeeds_[static_cast<std::size_t>(clampedRingIndex)] * factor);
}

void SpellRingSystem::setRadius(float radius)
{
    radii_.fill(std::max(0.0f, radius));
}

void SpellRingSystem::setAngularSpeed(float angularSpeed)
{
    angularSpeeds_.fill(std::max(0.0f, angularSpeed));
}

void SpellRingSystem::setRadiusForRing(int ringIndex, float radius)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return;
    }
    radii_[static_cast<std::size_t>(ringIndex)] = std::max(0.0f, radius);
}

void SpellRingSystem::setAngularSpeedForRing(int ringIndex, float angularSpeed)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return;
    }
    angularSpeeds_[static_cast<std::size_t>(ringIndex)] = std::max(0.0f, angularSpeed);
}

void SpellRingSystem::advanceOrbitAngles(float dt, const RuntimeBalance& balance)
{
    const float safeDt = std::max(0.0f, dt);
    const RingOrbitTuning tuning = makeRingOrbitTuning(balance);
    orbitTuning_ = tuning;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const RingShape ringShape = ringShapeForIndex(ringIndex);
        baseAngles_[static_cast<std::size_t>(ringIndex)] = normalizeAngle(
            baseAngles_[static_cast<std::size_t>(ringIndex)] + ringAngularSpeedForIndex(ringIndex, balance) * safeDt);
        if (ringShape == RingShape::FigureEight) {
            shapeRotations_[static_cast<std::size_t>(ringIndex)] = normalizeAngle(
                shapeRotations_[static_cast<std::size_t>(ringIndex)] + std::max(0.0f, tuning.figure8ShapeRotationSpeed) * safeDt);
        }
    }
}

void SpellRingSystem::refreshItemWorldPositions(float dt, const RuntimeBalance& balance, bool advanceCapturedBehaviors)
{
    const float safeDt = std::max(0.0f, dt);
    const RingOrbitTuning tuning = orbitTuning_;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const RingRuntimeState& runtime = ringRuntime_[static_cast<std::size_t>(ringIndex)];
        const Vec2 centerVelocity = safeDt > 0.0f ? (runtime.center - runtime.previousCenter) / safeDt : Vec2{};
        std::vector<SpellRingItem>& ringItems = itemsByRing_[static_cast<std::size_t>(ringIndex)];
        const RingShape ringShape = ringShapeForIndex(ringIndex);
        const float ringAngularSpeed = ringAngularSpeedForIndex(ringIndex, balance);
        const float shapeRotationSpeed = ringShape == RingShape::FigureEight
            ? std::max(0.0f, tuning.figure8ShapeRotationSpeed)
            : (ringShape == RingShape::Comet ? ringAngularSpeed : 0.0f);
        const int ringItemCount = static_cast<int>(ringItems.size());
        for (int itemIndex = 0; itemIndex < ringItemCount; ++itemIndex) {
            SpellRingItem& item = ringItems[static_cast<std::size_t>(itemIndex)];
            item.ringIndex = ringIndex;
            float itemRadiusScale = 1.0f;
            if (item.hasCapturedBehavior("jump_outward")) {
                const float jumpInterval = static_cast<float>(std::max(0.2, item.capturedBehaviorInterval("jump_outward", CapturedJumpInterval)));
                const float jumpDuration = static_cast<float>(std::max(0.05, item.capturedBehaviorParamDouble("jump_outward", "duration", CapturedJumpDuration)));
                const float jumpDistance = static_cast<float>(std::max(2.0, item.capturedBehaviorParamDouble("jump_outward", "distance", CapturedJumpDistance)));
                if (advanceCapturedBehaviors) {
                    item.capturedBehaviorTimer += safeDt;
                    item.capturedJumpTimer = std::max(0.0f, item.capturedJumpTimer - safeDt);
                    if (item.capturedBehaviorTimer >= jumpInterval) {
                        item.capturedBehaviorTimer = 0.0f;
                        item.capturedJumpTimer = jumpDuration;
                    }
                }
                if (item.capturedJumpTimer > 0.0f) {
                    const float phase = 1.0f - item.capturedJumpTimer / jumpDuration;
                    itemRadiusScale += (std::sin(phase * Pi) * jumpDistance) / std::max(1.0f, radiusForRing(ringIndex));
                }
            } else if (advanceCapturedBehaviors) {
                item.capturedJumpTimer = 0.0f;
            }

            RingOrbitContext context = makeOrbitContextForRing(ringIndex, itemIndex, ringItemCount, itemRadiusScale, balance);
            const float param = ringShape == RingShape::Comet
                ? normalizeLocalParam(ringShape, item.localAngle, context.tuning)
                : normalizeAngle(baseAngles_[static_cast<std::size_t>(ringIndex)] + item.localAngle);
            const Vec2 previousWorldPosition = item.worldPosition;
            if (runtime.state == SpellRingState::Normal) {
                const Vec2 localPosition = getRingItemLocalPosition(param, context);
                item.orbitOutward = safeNormalize(localPosition, fromAngle(param));
                item.orbitTangent = safeNormalize(
                    getRingItemVelocity(
                        param,
                        ringShape == RingShape::Comet ? 0.0f : ringAngularSpeed,
                        shapeRotationSpeed,
                        {},
                        context),
                    {-item.orbitOutward.y, item.orbitOutward.x});
                item.worldPosition = getRingItemWorldPositionWithDistanceOffset(
                    runtime.center,
                    param,
                    context,
                    item.orbitDistanceOffset);
                item.worldVelocity = getRingItemVelocityWithDistanceOffset(
                    param,
                    ringShape == RingShape::Comet ? 0.0f : ringAngularSpeed,
                    shapeRotationSpeed,
                    centerVelocity,
                    context,
                    item.orbitDistanceOffset);
            } else {
                RingOrbitContext throwContext = context;
                throwContext.shapeRotation = runtime.throwShapeRotation;
                const Vec2 throwOrigin = throwPresentationOrigin(
                    runtime.homeCenter,
                    runtime.throwDirection,
                    runtime.throwElapsed,
                    runtime.throwPeakTime,
                    runtime.throwSettleTime);
                const ThrowMasterPath throwPath = makeThrowMasterPath(
                    throwOrigin,
                    throwContext,
                    runtime.throwDirection,
                    runtime.throwLaunchOffset,
                    runtime.throwReturnOffset,
                    runtime.throwCutPathT,
                    runtime.throwReturnPathT,
                    runtime.throwDistance);
                const ThrowVisibleWindow throwWindow = throwVisibleWindowForElapsed(
                    throwPath,
                    ringShape,
                    runtime.throwElapsed,
                    runtime.throwPeakTime,
                    runtime.throwReturnTime);
                const float pathT = pathTForParam(ringShape, param, context.tuning);
                const float materialDistance = sampleSourceMaterialDistance(
                    throwPath,
                    ringShape,
                    pathT,
                    runtime.throwCutPathT,
                    runtime.throwReturnPathT);
                item.worldPosition = sampleThrowMasterPathPoint(
                    throwPath,
                    throwWindow,
                    materialDistance,
                    throwOrigin,
                    runtime.throwDirection,
                    item.orbitDistanceOffset);
                const Vec2 before = sampleThrowMasterPathPoint(
                    throwPath,
                    throwWindow,
                    materialDistance - ThrowMasterPathTangentSampleDistance,
                    throwOrigin,
                    runtime.throwDirection,
                    item.orbitDistanceOffset);
                const Vec2 after = sampleThrowMasterPathPoint(
                    throwPath,
                    throwWindow,
                    materialDistance + ThrowMasterPathTangentSampleDistance,
                    throwOrigin,
                    runtime.throwDirection,
                    item.orbitDistanceOffset);
                item.orbitTangent = safeNormalize(after - before, runtime.throwDirection);
                item.orbitOutward = safeNormalize(item.worldPosition - throwOrigin, runtime.throwDirection);
                item.worldVelocity = safeDt > 0.0f ? (item.worldPosition - previousWorldPosition) / safeDt : centerVelocity;
            }
            item.orbitMotionSpeed = length(item.worldVelocity) / std::max(1.0f, radiusForRing(ringIndex));
        }
    }
}

void SpellRingSystem::updatePresentation(const Player& player, float dt, const RuntimeBalance& balance)
{
    updateActionFlashTimers(dt);
    updateThrowVisualEnergyTimers(dt);
    const float safeDt = std::max(0.0f, dt);
    advanceOrbitAngles(safeDt, balance);
    const bool ringShiftAllowed = !anyRingInFlight();
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        RingRuntimeState& runtime = ringRuntime_[static_cast<std::size_t>(ringIndex)];
        runtime.previousCenter = runtime.center;
        if (safeDt > 0.0f) {
            runtime.externalWindOffset += runtime.externalWindVelocity * safeDt;
            runtime.externalWindVelocity = runtime.externalWindVelocity * std::exp(-RingWindVelocityDamping * safeDt);
            runtime.externalWindOffset = runtime.externalWindOffset * std::exp(-RingWindReturnRate * safeDt);
            const float offsetLength = length(runtime.externalWindOffset);
            if (offsetLength > RingWindMaxOffset) {
                runtime.externalWindOffset = runtime.externalWindOffset / offsetLength * RingWindMaxOffset;
            } else if (offsetLength <= 0.05f && lengthSquared(runtime.externalWindVelocity) <= 0.05f) {
                runtime.externalWindOffset = {};
                runtime.externalWindVelocity = {};
            }
        }
        const Vec2 targetCenter = getRingCenterWorldPositionForFocus(
            player.position,
            player.spellRingShiftDirection,
            player.spellRingShift,
            ringIndex,
            activeRingIndex_,
            ringShiftAllowed);
        const Vec2 windCenter = targetCenter + runtime.externalWindOffset;
        runtime.homeCenter = windCenter;
        if (runtime.state == SpellRingState::Normal) {
            runtime.center = windCenter;
        } else {
            runtime.center = windCenter
                + runtime.throwDirection * (runtime.throwDistance * throwReachForRing(ringIndex))
                + throwReturnSettleOffset(
                    runtime.throwDirection,
                    runtime.throwElapsed,
                    runtime.throwPeakTime,
                    runtime.throwSettleTime);
        }
    }
    refreshItemWorldPositions(safeDt, balance, false);
}

void SpellRingSystem::resetRuntimeStateAtPlayer(const Player& player, const RuntimeBalance& balance)
{
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        RingRuntimeState& runtime = ringRuntime_[static_cast<std::size_t>(ringIndex)];
        const Vec2 normalCenter = getRingCenterWorldPositionForFocus(
            player.position,
            player.spellRingShiftDirection,
            player.spellRingShift,
            ringIndex,
            activeRingIndex_,
            true);
        runtime = {};
        runtime.homeCenter = normalCenter;
        runtime.center = normalCenter;
        runtime.previousCenter = normalCenter;
        runtime.throwDirection = player.facing;
    }
    throwingRingIndex_ = -1;
    enemyOrbitSpeedDebuffMultiplier_ = 1.0f;
    enemyOrbitSpeedDebuffTimer_ = 0.0f;
    motionEvents_.clear();
    clearActionFlashTimers();
    refreshItemWorldPositions(0.0f, balance, false);
}

void SpellRingSystem::clearActionFlashTimers()
{
    for (auto& ringItems : itemsByRing_) {
        for (SpellRingItem& item : ringItems) {
            item.actionFlashTimer = 0.0f;
        }
    }
}

void SpellRingSystem::setWorkshopModifiers(const RingWorkshopModifiers& modifiers)
{
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        setWorkshopModifiersForRing(ringIndex, modifiers);
    }
}

void SpellRingSystem::setWorkshopModifiersForRing(int ringIndex, const RingWorkshopModifiers& modifiers)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return;
    }
    RingWorkshopModifiers sanitized{};
    sanitized.throwDistanceMultiplier = std::max(0.0f, modifiers.throwDistanceMultiplier);
    sanitized.throwCooldownMultiplier = std::max(0.02f, modifiers.throwCooldownMultiplier);
    sanitized.weightStopRatioBonus = std::clamp(
        modifiers.weightStopRatioBonus,
        0.0f,
        MaxWeightStopRatio - BaseWeightStopRatio);
    sanitized.extraEquipSlots = std::clamp(modifiers.extraEquipSlots, 0, 10);
    workshopModifiersByRing_[static_cast<std::size_t>(ringIndex)] = sanitized;
}

void SpellRingSystem::updateActionFlashTimers(float dt)
{
    const float safeDt = std::max(0.0f, dt);
    if (safeDt <= 0.0f) {
        return;
    }

    for (auto& ringItems : itemsByRing_) {
        for (SpellRingItem& item : ringItems) {
            if (item.actionFlashTimer > 0.0f) {
                item.actionFlashTimer = std::max(0.0f, item.actionFlashTimer - safeDt);
            }
        }
    }
}

void SpellRingSystem::updateThrowVisualEnergyTimers(float dt)
{
    const float safeDt = std::max(0.0f, dt);
    if (safeDt <= 0.0f) {
        return;
    }

    for (RingRuntimeState& runtime : ringRuntime_) {
        if (runtime.throwVisualEnergyFadeTimer > 0.0f) {
            runtime.throwVisualEnergyFadeTimer = std::max(0.0f, runtime.throwVisualEnergyFadeTimer - safeDt);
        }
    }
}

void SpellRingSystem::updateThrowCooldowns(float dt)
{
    const float safeDt = std::max(0.0f, dt);
    if (safeDt <= 0.0f) {
        return;
    }

    for (RingRuntimeState& runtime : ringRuntime_) {
        runtime.throwCooldownRemaining = std::max(0.0f, runtime.throwCooldownRemaining - safeDt);
    }
}

float SpellRingSystem::throwCooldownForRing(int ringIndex, const RuntimeBalance& balance) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const RingWorkshopModifiers& workshop = workshopModifiersByRing_[static_cast<std::size_t>(clampedRingIndex)];
    return scaledAtLeast(
        balance.spellRingThrowCooldown,
        equipmentModifiersForRing(clampedRingIndex).ringThrowCooldownMul * workshop.throwCooldownMultiplier,
        0.02f);
}

void SpellRingSystem::update(Player& player, const Input& input, float dt, float, bool paused, bool blockPointerThrow, const RuntimeBalance& balance)
{
    updateActionFlashTimers(dt);
    updateThrowVisualEnergyTimers(dt);

    if (paused) {
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    updateThrowCooldowns(safeDt);
    enemyOrbitSpeedDebuffTimer_ = std::max(0.0f, enemyOrbitSpeedDebuffTimer_ - safeDt);
    if (enemyOrbitSpeedDebuffTimer_ <= 0.0f) {
        enemyOrbitSpeedDebuffMultiplier_ = 1.0f;
    }

    advanceOrbitAngles(safeDt, balance);

    const RingEquipmentModifiers& activeEquipment = equipmentModifiersForRing(activeRingIndex_);
    const RingWorkshopModifiers& activeWorkshop = workshopModifiersByRing_[static_cast<std::size_t>(activeRingIndex_)];
    const float throwCooldown = throwCooldownForRing(activeRingIndex_, balance);
    const float throwSpeed = scaledNonNegative(balance.spellRingThrowSpeed, activeEquipment.ringThrowSpeedMul);
    const float throwDistance = scaledNonNegative(
        balance.spellRingThrowDistance,
        activeEquipment.ringThrowDistanceMul * activeWorkshop.throwDistanceMultiplier);
    const float returnSpeed = scaledNonNegative(balance.spellRingReturnSpeed, activeEquipment.ringReturnSpeedMul);

    const double anchorStrength = std::max(
        0.0,
        orbitModifiers_.anchorStrength * finiteEquipmentMultiplier(activeEquipment.ringAnchorMul));
    const bool throwPressed = input.throwPressed() && !(blockPointerThrow && input.mouseLeftPressed());
    const bool throwCanStart =
        throwPressed &&
        ringRuntime_[static_cast<std::size_t>(activeRingIndex_)].throwCooldownRemaining <= 0.0f &&
        throwingRingIndex_ < 0 &&
        ringRuntime_[static_cast<std::size_t>(activeRingIndex_)].state == SpellRingState::Normal;
    const bool ringShiftAllowed = !anyRingInFlight() && !throwCanStart;
    if (!ringShiftAllowed) {
        player.spellRingShift = 0.0f;
        player.spellRingShiftDragActive = false;
    }
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        RingRuntimeState& runtime = ringRuntime_[static_cast<std::size_t>(ringIndex)];
        runtime.previousCenter = runtime.center;
        if (safeDt > 0.0f) {
            runtime.externalWindOffset += runtime.externalWindVelocity * safeDt;
            runtime.externalWindVelocity = runtime.externalWindVelocity * std::exp(-RingWindVelocityDamping * safeDt);
            runtime.externalWindOffset = runtime.externalWindOffset * std::exp(-RingWindReturnRate * safeDt);
            const float offsetLength = length(runtime.externalWindOffset);
            if (offsetLength > RingWindMaxOffset) {
                runtime.externalWindOffset = runtime.externalWindOffset / offsetLength * RingWindMaxOffset;
            } else if (offsetLength <= 0.05f && lengthSquared(runtime.externalWindVelocity) <= 0.05f) {
                runtime.externalWindOffset = {};
                runtime.externalWindVelocity = {};
            }
        }
        const Vec2 targetCenter = getRingCenterWorldPositionForFocus(
            player.position,
            player.spellRingShiftDirection,
            player.spellRingShift,
            ringIndex,
            activeRingIndex_,
            ringShiftAllowed);
        const Vec2 windCenter = targetCenter + runtime.externalWindOffset;
        runtime.homeCenter = windCenter;

        if (runtime.state == SpellRingState::Normal) {
            if (input.ringOffsetHeld() && anchorStrength > 0.0 && safeDt > 0.0f) {
                const float clampedAnchor = clamp(static_cast<float>(anchorStrength), 0.0f, 5.0f);
                const float followRate = 14.0f / (1.0f + clampedAnchor * 2.5f);
                runtime.center = lerp(runtime.center, windCenter, 1.0f - std::exp(-followRate * safeDt));
                const Vec2 toNormalCenter = windCenter - runtime.center;
                const float lag = length(toNormalCenter);
                const float maxLag = 16.0f + clampedAnchor * 32.0f;
                if (lag > maxLag) {
                    runtime.center = windCenter - normalize(toNormalCenter) * maxLag;
                }
            } else {
                runtime.center = windCenter;
            }
            continue;
        }

        runtime.throwElapsed += safeDt;
        const float peakTime = std::max(0.02f, runtime.throwPeakTime);
        const RingShape ringShape = ringShapeForIndex(ringIndex);
        const float completeTime = throwCompleteTime(
            ringShape,
            runtime.throwPeakTime,
            runtime.throwReturnTime,
            runtime.throwSettleTime);
        if (runtime.state == SpellRingState::Thrown && runtime.throwElapsed >= peakTime) {
            runtime.state = SpellRingState::Returning;
            motionEvents_.push_back({
                RingMotionEventKind::ReturnStart,
                ringIndex,
                runtime.homeCenter + runtime.throwReturnOffset,
                runtime.throwDirection});
        }
        if (runtime.throwElapsed >= completeTime) {
            const Vec2 returnEndPosition = runtime.homeCenter + runtime.throwReturnOffset;
            if (ringShape == RingShape::Comet) {
                RingOrbitContext throwContext = makeOrbitContextForRing(ringIndex, 0, 1, 1.0f, balance);
                throwContext.shapeRotation = runtime.throwShapeRotation;
                const ThrowMasterPath throwPath = makeThrowMasterPath(
                    runtime.homeCenter,
                    throwContext,
                    runtime.throwDirection,
                    runtime.throwLaunchOffset,
                    runtime.throwReturnOffset,
                    runtime.throwCutPathT,
                    runtime.throwReturnPathT,
                    runtime.throwDistance);
                const Vec2 finishHeadOffset = sampleMasterPathByDistance(
                    throwPath,
                    throwPath.finishTailDistance + throwPath.visibleLength) - runtime.homeCenter;
                if (lengthSquared(finishHeadOffset) > 0.0001f) {
                    const float halfArc = clampCometArcRadians(throwContext.tuning) * 0.5f;
                    baseAngles_[static_cast<std::size_t>(ringIndex)] = normalizeAngle(
                        std::atan2(finishHeadOffset.y, finishHeadOffset.x) - halfArc);
                }
            } else {
                baseAngles_[static_cast<std::size_t>(ringIndex)] = normalizeAngle(
                    baseAngles_[static_cast<std::size_t>(ringIndex)] +
                    wrap01(runtime.throwReturnPathT - runtime.throwCutPathT) * FullCircleRadians);
            }
            if (ringShape == RingShape::FigureEight) {
                shapeRotations_[static_cast<std::size_t>(ringIndex)] = runtime.throwShapeRotation;
            }
            runtime.state = SpellRingState::Normal;
            runtime.center = windCenter;
            runtime.throwElapsed = 0.0f;
            runtime.throwPeakTime = 0.0f;
            runtime.throwReturnTime = 0.0f;
            runtime.throwSettleTime = 0.0f;
            runtime.throwDistance = 0.0f;
            runtime.throwLaunchOffset = {};
            runtime.throwReturnOffset = {};
            runtime.throwCutPathT = 0.0f;
            runtime.throwReturnPathT = 0.5f;
            runtime.throwShapeRotation = 0.0f;
            runtime.throwVisualEnergyFadeTimer = ThrowVisualEnergyFadeDuration;
            if (throwingRingIndex_ == ringIndex) {
                throwingRingIndex_ = -1;
            }
            motionEvents_.push_back({RingMotionEventKind::ReturnEnd, ringIndex, returnEndPosition, runtime.throwDirection});
        } else {
            runtime.center = windCenter
                + runtime.throwDirection * (runtime.throwDistance * throwReachForRing(ringIndex))
                + throwReturnSettleOffset(
                    runtime.throwDirection,
                    runtime.throwElapsed,
                    runtime.throwPeakTime,
                    runtime.throwSettleTime);
        }
    }

    if (throwPressed &&
        ringRuntime_[static_cast<std::size_t>(activeRingIndex_)].throwCooldownRemaining <= 0.0f &&
        throwingRingIndex_ < 0) {
        RingRuntimeState& runtime = ringRuntime_[static_cast<std::size_t>(activeRingIndex_)];
        if (runtime.state == SpellRingState::Normal) {
            runtime.throwDirection = safeNormalize(player.facing);
            RingOrbitContext throwContext = makeOrbitContextForRing(activeRingIndex_, 0, 1, 1.0f, balance);
            const ThrowAnchorPathTs anchors = throwAnchorPathTs(
                runtime.homeCenter,
                runtime.throwDirection,
                throwContext);
            const float launchPathT = anchors.launchPathT;
            const float returnPathT = anchors.returnPathT;
            const float launchParam = pathParamForT(throwContext.shape, launchPathT, throwContext.tuning);
            const float returnParam = pathParamForT(throwContext.shape, returnPathT, throwContext.tuning);
            runtime.throwLaunchOffset = getRingItemWorldPosition(runtime.homeCenter, launchParam, throwContext) - runtime.homeCenter;
            runtime.throwReturnOffset = throwContext.shape == RingShape::Comet
                ? reflectedCometReturnOffset(runtime.throwLaunchOffset, runtime.throwDirection)
                : getRingItemWorldPosition(runtime.homeCenter, returnParam, throwContext) - runtime.homeCenter;
            const ThrowMasterPath throwPath = makeThrowMasterPath(
                runtime.homeCenter,
                throwContext,
                runtime.throwDirection,
                runtime.throwLaunchOffset,
                runtime.throwReturnOffset,
                launchPathT,
                returnPathT,
                throwDistance);
            const float peakTime = throwFlightTimeForPath(
                throwPath,
                throwSpeed,
                balance.spellRingThrowMaxTime);
            const float returnTime = throwContext.shape == RingShape::Comet
                ? throwCometChaseTimeForPath(
                    throwPath,
                    peakTime,
                    ringAngularSpeedForIndex(activeRingIndex_, balance),
                    length(runtime.throwReturnOffset),
                    peakTime)
                : throwChaseTimeForPath(
                    throwPath,
                    returnSpeed,
                    peakTime);
            runtime.state = SpellRingState::Thrown;
            runtime.throwCutPathT = launchPathT;
            runtime.throwReturnPathT = returnPathT;
            runtime.throwShapeRotation = throwContext.shapeRotation;
            runtime.throwElapsed = 0.0f;
            runtime.throwPeakTime = peakTime;
            runtime.throwReturnTime = returnTime;
            runtime.throwSettleTime = throwContext.shape == RingShape::Comet ? 0.0f : ThrowReturnSettleDuration;
            runtime.throwDistance = throwDistance;
            runtime.throwVisualEnergyFadeTimer = 0.0f;
            throwingRingIndex_ = activeRingIndex_;
            runtime.throwCooldownRemaining = throwCooldown;
            motionEvents_.push_back({
                RingMotionEventKind::ThrowStart,
                activeRingIndex_,
                runtime.homeCenter + runtime.throwLaunchOffset,
                runtime.throwDirection});
        }
    }

    std::vector<SpellRingItem*> allItems = runtimeItemsMutable();
    if (allItems.empty()) {
        return;
    }

    int periodicHealCount = 0;
    float periodicHealAmountAccumulator = 0.0f;
    float periodicHealInterval = CapturedPeriodicHealInterval;
    for (const SpellRingItem* item : allItems) {
        if (item == nullptr) {
            continue;
        }
        if (!item->broken() && item->hasCapturedBehavior("periodic_heal")) {
            ++periodicHealCount;
            periodicHealAmountAccumulator += static_cast<float>(std::max(0.0, item->capturedBehaviorParamDouble("periodic_heal", "amount", 1.0)));
            periodicHealInterval = std::min(periodicHealInterval, static_cast<float>(std::max(0.25, item->capturedBehaviorInterval("periodic_heal", CapturedPeriodicHealInterval))));
        }
    }
    if (periodicHealCount > 0) {
        const float stackedRate = std::min(1.0f, 0.65f + 0.12f * static_cast<float>(periodicHealCount));
        capturedHealTimer_ -= safeDt * stackedRate;
        if (capturedHealTimer_ <= 0.0f) {
            if (player.hp > 0 && player.hp < player.maxHp) {
                const int pulseHeal = std::clamp(static_cast<int>(std::round(std::max(1.0f, periodicHealAmountAccumulator))), 1, CapturedPeriodicHealMaxPerPulse);
                player.hp = std::min(player.maxHp, player.hp + pulseHeal);
            }
            capturedHealTimer_ = periodicHealInterval;
        }
    } else {
        capturedHealTimer_ = CapturedPeriodicHealInterval;
    }

    refreshItemWorldPositions(safeDt, balance, true);
}

float SpellRingSystem::throwReachForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return 0.0f;
    }

    const RingRuntimeState& runtime = ringRuntime_[static_cast<std::size_t>(ringIndex)];
    if (runtime.state == SpellRingState::Normal) {
        return 0.0f;
    }

    const float peakTime = std::max(0.02f, runtime.throwPeakTime);
    if (runtime.throwElapsed <= peakTime) {
        return smoothStep01(runtime.throwElapsed / peakTime);
    }

    const float returnTime = std::max(0.02f, runtime.throwReturnTime);
    return 1.0f - smoothStep01((runtime.throwElapsed - peakTime) / returnTime);
}

void SpellRingSystem::clearOrbitModifiers()
{
    orbitModifiers_ = OrbitModifiers{};
}

void SpellRingSystem::setOrbitModifiers(OrbitModifiers modifiers)
{
    orbitModifiers_ = std::move(modifiers);
}

void SpellRingSystem::setEquipmentModifiers(EquipmentModifiers modifiers)
{
    equipmentModifiers_ = std::move(modifiers);
}

void SpellRingSystem::applyOrbitModifierEffect(std::string_view effect, double value, std::string_view source)
{
    OrbitModifierAccumulator accumulator;
    accumulator.clear();
    accumulator.applyEffect(effect, value, source);
    const OrbitModifiers& incoming = accumulator.modifiers();
    orbitModifiers_.speedMultiplier *= incoming.speedMultiplier;
    orbitModifiers_.powerMultiplier *= incoming.powerMultiplier;
    orbitModifiers_.gravityMultiplier *= incoming.gravityMultiplier;
    orbitModifiers_.antigravityMultiplier *= incoming.antigravityMultiplier;
    orbitModifiers_.anchorStrength += incoming.anchorStrength;
    orbitModifiers_.shiftMultiplier *= incoming.shiftMultiplier;
    orbitModifiers_.speedDamageMultiplier *= incoming.speedDamageMultiplier;
    orbitModifiers_.sources.insert(orbitModifiers_.sources.end(), incoming.sources.begin(), incoming.sources.end());
}

void SpellRingSystem::applyEnemyOrbitSpeedDebuff(float multiplier, float durationSeconds)
{
    if (durationSeconds <= 0.0f) {
        return;
    }

    const float clampedMultiplier = clamp(multiplier, 0.05f, 1.0f);
    enemyOrbitSpeedDebuffMultiplier_ = std::min(enemyOrbitSpeedDebuffMultiplier_, clampedMultiplier);
    enemyOrbitSpeedDebuffTimer_ = std::max(enemyOrbitSpeedDebuffTimer_, durationSeconds);
}

void SpellRingSystem::upgradeItemDamage(int amount)
{
    for (auto& ringItems : itemsByRing_) {
        for (auto& item : ringItems) {
            item.damage += amount;
        }
    }
}

void SpellRingSystem::upgradeMaxEquippedWeightForAllRings(float amount)
{
    for (float& maxWeight : maxEquippedWeights_) {
        maxWeight = std::max(0.0f, maxWeight + amount);
    }
}

void SpellRingSystem::setMaxEquippedWeightForAllRings(float maxWeight)
{
    maxEquippedWeights_.fill(std::max(0.0f, maxWeight));
}

void SpellRingSystem::upgradeMaxEquippedWeightForRing(int ringIndex, float amount)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return;
    }
    float& maxWeight = maxEquippedWeights_[static_cast<std::size_t>(ringIndex)];
    maxWeight = std::max(0.0f, maxWeight + amount);
}

void SpellRingSystem::setMaxEquippedWeightForRing(int ringIndex, float maxWeight)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return;
    }
    maxEquippedWeights_[static_cast<std::size_t>(ringIndex)] = std::max(0.0f, maxWeight);
}

bool SpellRingSystem::canAddItem() const
{
    return canAddItemForRing(activeRingIndex_);
}

bool SpellRingSystem::canAddItem(const SpellRingItem& item) const
{
    return canAddItemForRing(activeRingIndex_, item);
}

bool SpellRingSystem::canAddItemForRing(int ringIndex) const
{
    return ringIndex >= 0 &&
        ringIndex < SpellRingCount &&
        itemsByRing_[static_cast<std::size_t>(ringIndex)].size() < static_cast<std::size_t>(maxItemCountForRing(ringIndex));
}

bool SpellRingSystem::canAddItemForRing(int ringIndex, const SpellRingItem& item) const
{
    return canAddItemForRing(ringIndex) &&
        totalEquippedWeightForRing(ringIndex) + std::max(0.0f, item.weight) <= overweightEquipLimitForRing(ringIndex);
}

bool SpellRingSystem::canPlaceItemAtAngle(int index, float angle) const
{
    const auto& ringItems = activeItems();
    if (index < 0 || index >= static_cast<int>(ringItems.size())) {
        return false;
    }
    return canPlaceItemAtAngle(ringItems[static_cast<std::size_t>(index)], angle, index, orbitTuning_);
}

std::optional<float> SpellRingSystem::nearestPlaceableAngle(int index, float desiredAngle, float maxDeltaRadians) const
{
    const auto& ringItems = activeItems();
    if (index < 0 || index >= static_cast<int>(ringItems.size()) || maxDeltaRadians < 0.0f) {
        return std::nullopt;
    }

    const RingOrbitTuning& tuning = orbitTuning_;
    const RingShape shape = runtimeRingShape();
    const float desired = quantizeLocalParam(shape, desiredAngle, tuning);
    const SpellRingItem& item = ringItems[static_cast<std::size_t>(index)];
    if (canPlaceItemAtAngle(item, desired, index, tuning)) {
        return desired;
    }

    const float stepRadians = shape == RingShape::Comet
        ? std::max(Pi / 180.0f, clampCometArcRadians(tuning) / static_cast<float>(maxItemCount() * 2))
        : PlacementStepRadians;
    const int maxSteps = static_cast<int>(std::floor(maxDeltaRadians / stepRadians + 0.0001f));
    for (int step = 1; step <= maxSteps; ++step) {
        const float delta = static_cast<float>(step) * stepRadians;
        const float clockwise = quantizeLocalParam(shape, desired + delta, tuning);
        if (canPlaceItemAtAngle(item, clockwise, index, tuning)) {
            return clockwise;
        }
        const float counterClockwise = quantizeLocalParam(shape, desired - delta, tuning);
        if (canPlaceItemAtAngle(item, counterClockwise, index, tuning)) {
            return counterClockwise;
        }
    }

    return std::nullopt;
}

bool SpellRingSystem::addItem(SpellRingItemType type)
{
    return addItem(makeSpellRingItem(type));
}

bool SpellRingSystem::addItem(SpellRingItem item, SpellRingAddResult* outResult)
{
    return addItemToRing(activeRingIndex_, std::move(item), outResult);
}

bool SpellRingSystem::addItemToRing(int ringIndex, SpellRingItem item, SpellRingAddResult* outResult)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount || !canAddItemForRing(ringIndex, item)) {
        return false;
    }
    const std::optional<float> angle = findBestPlacementAngleForRing(ringIndex, item, -1, orbitTuning_);
    if (!angle) {
        return false;
    }
    std::vector<SpellRingItem>& ringItems = itemsByRing_[static_cast<std::size_t>(ringIndex)];
    item.ringIndex = ringIndex;
    item.localAngle = *angle;
    const SpellRingAddResult result{
        .ringIndex = ringIndex,
        .itemIndex = static_cast<int>(ringItems.size()),
        .localAngle = item.localAngle,
        .objectId = item.objectId,
        .instanceId = item.instanceId,
    };
    ringItems.push_back(std::move(item));
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool SpellRingSystem::addObjectItem(const ItemData& item, SpellRingAddResult* outResult)
{
    return addObjectItemToRing(activeRingIndex_, item, outResult);
}

bool SpellRingSystem::addObjectItem(const ItemData& item, const ItemInstance& instance, SpellRingAddResult* outResult)
{
    return addObjectItemToRing(activeRingIndex_, item, instance, outResult);
}

bool SpellRingSystem::addObjectItemToRing(int ringIndex, const ItemData& item, SpellRingAddResult* outResult)
{
    if (item.id.empty()) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItem(item.id);
    applyObjectDefinition(ringItem, item);
    return addItemToRing(ringIndex, std::move(ringItem), outResult);
}

bool SpellRingSystem::addObjectItemToRing(
    int ringIndex,
    const ItemData& item,
    const ItemInstance& instance,
    SpellRingAddResult* outResult)
{
    if (item.id.empty() || instance.objectId != item.id) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItem(item.id);
    applyItemInstance(ringItem, instance);
    applyObjectDefinition(ringItem, item);
    return addItemToRing(ringIndex, std::move(ringItem), outResult);
}

bool SpellRingSystem::canAddObjectItem(const ItemData& item) const
{
    return canAddObjectItemForRing(activeRingIndex_, item);
}

bool SpellRingSystem::canAddObjectItem(const ItemData& item, const ItemInstance& instance) const
{
    return canAddObjectItemForRing(activeRingIndex_, item, instance);
}

bool SpellRingSystem::canAddObjectItemForRing(int ringIndex, const ItemData& item) const
{
    if (item.id.empty()) {
        return false;
    }

    const SpellRingItem ringItem = makeObjectRingItemForAdd(item, nullptr);
    return canAddItemForRing(ringIndex, ringItem) &&
        findBestPlacementAngleForRing(ringIndex, ringItem, -1, orbitTuning_).has_value();
}

bool SpellRingSystem::canAddObjectItemForRing(int ringIndex, const ItemData& item, const ItemInstance& instance) const
{
    if (item.id.empty() || instance.objectId != item.id) {
        return false;
    }

    const SpellRingItem ringItem = makeObjectRingItemForAdd(item, &instance);
    return canAddItemForRing(ringIndex, ringItem) &&
        findBestPlacementAngleForRing(ringIndex, ringItem, -1, orbitTuning_).has_value();
}

bool SpellRingSystem::canAddObjectItemAtAngle(const ItemData& item, float localAngle) const
{
    if (item.id.empty()) {
        return false;
    }

    const SpellRingItem ringItem = makeObjectRingItemForAdd(item, nullptr);
    if (!canAddItem(ringItem)) {
        return false;
    }

    const float angle = quantizeLocalParam(runtimeRingShape(), localAngle, orbitTuning_);
    return canPlaceItemAtAngle(ringItem, angle, -1, orbitTuning_);
}

bool SpellRingSystem::canAddObjectItemAtAngle(const ItemData& item, const ItemInstance& instance, float localAngle) const
{
    if (item.id.empty() || instance.objectId != item.id) {
        return false;
    }

    const SpellRingItem ringItem = makeObjectRingItemForAdd(item, &instance);
    if (!canAddItem(ringItem)) {
        return false;
    }

    const float angle = quantizeLocalParam(runtimeRingShape(), localAngle, orbitTuning_);
    return canPlaceItemAtAngle(ringItem, angle, -1, orbitTuning_);
}

bool SpellRingSystem::addObjectItemAtAngle(const ItemData& item, float localAngle, SpellRingAddResult* outResult)
{
    if (item.id.empty()) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItemForAdd(item, nullptr);
    if (!canAddItem(ringItem)) {
        return false;
    }

    const float angle = quantizeLocalParam(runtimeRingShape(), localAngle, orbitTuning_);
    if (!canPlaceItemAtAngle(ringItem, angle, -1, orbitTuning_)) {
        return false;
    }

    ringItem.ringIndex = activeRingIndex_;
    ringItem.localAngle = angle;
    const SpellRingAddResult result{
        .ringIndex = activeRingIndex_,
        .itemIndex = static_cast<int>(activeItems().size()),
        .localAngle = ringItem.localAngle,
        .objectId = ringItem.objectId,
        .instanceId = ringItem.instanceId,
    };
    activeItems().push_back(std::move(ringItem));
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool SpellRingSystem::addObjectItemAtAngle(
    const ItemData& item,
    const ItemInstance& instance,
    float localAngle,
    SpellRingAddResult* outResult)
{
    if (item.id.empty() || (!instance.objectId.empty() && instance.objectId != item.id)) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItemForAdd(item, instance.objectId.empty() ? nullptr : &instance);
    if (!canAddItem(ringItem)) {
        return false;
    }

    const float angle = quantizeLocalParam(runtimeRingShape(), localAngle, orbitTuning_);
    if (!canPlaceItemAtAngle(ringItem, angle, -1, orbitTuning_)) {
        return false;
    }

    ringItem.ringIndex = activeRingIndex_;
    ringItem.localAngle = angle;
    const SpellRingAddResult result{
        .ringIndex = activeRingIndex_,
        .itemIndex = static_cast<int>(activeItems().size()),
        .localAngle = ringItem.localAngle,
        .objectId = ringItem.objectId,
        .instanceId = ringItem.instanceId,
    };
    activeItems().push_back(std::move(ringItem));
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool SpellRingSystem::consumeItemDurability(SpellRingItem& item, int amount)
{
    const bool wasBroken = item.broken();
    const bool becameBroken = item.consumeDurability(amount);
    if (!wasBroken && becameBroken) {
        if (item.hasCapturedBehavior(BreakCountdownExplodeBehaviorId)) {
            startBreakCountdownExplosion(item);
            return true;
        }
        itemBreakEvents_.push_back(RingItemBreakEvent{
            .position = item.worldPosition,
            .type = item.type,
            .objectId = item.objectId,
            .instanceId = item.instanceId,
            .protectionEnabled = item.protectionEnabled,
        });
    }
    return becameBroken;
}

int SpellRingSystem::applyExplosionDamageToItems(Vec2 position, float radius, int damage)
{
    const float safeRadius = std::max(0.0f, radius);
    const int durabilityDamage = std::max(0, damage);
    if (safeRadius <= 0.0f || durabilityDamage <= 0) {
        return 0;
    }

    int damaged = 0;
    for (SpellRingItem* itemPtr : runtimeItemsMutable()) {
        if (itemPtr == nullptr || itemPtr->broken()) {
            continue;
        }

        SpellRingItem& item = *itemPtr;
        const float itemRadius = std::max(0.0f, item.hitRadius);
        const float hitRadius = safeRadius + itemRadius;
        if (distanceSquared(item.worldPosition, position) > hitRadius * hitRadius) {
            continue;
        }

        item.actionFlashTimer = SpellRingItemActionFlashSeconds;
        consumeItemDurability(item, durabilityDamage);
        ++damaged;
    }
    return damaged;
}

std::vector<RingItemBreakEvent> SpellRingSystem::consumeItemBreakEvents()
{
    std::vector<RingItemBreakEvent> events = std::move(itemBreakEvents_);
    itemBreakEvents_.clear();
    return events;
}

bool SpellRingSystem::repairItem(int ringIndex, int itemIndex)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return false;
    }
    std::vector<SpellRingItem>& ringItems = itemsByRing_[static_cast<std::size_t>(ringIndex)];
    if (itemIndex < 0 || itemIndex >= static_cast<int>(ringItems.size())) {
        return false;
    }

    SpellRingItem& item = ringItems[static_cast<std::size_t>(itemIndex)];
    if (item.maxDurability < 0 || (!item.broken() && item.durability >= item.maxDurability)) {
        return false;
    }

    item.durability = item.maxDurability;
    item.isBroken = false;
    item.breakExplosion = {};
    return true;
}

bool SpellRingSystem::enhanceItem(
    int ringIndex,
    int itemIndex,
    int attackBonus,
    int digBonus,
    int durabilityBonus,
    int attackLevelDelta,
    int digLevelDelta,
    int durabilityLevelDelta,
    int maxEnhanceLevel,
    const ObjectCatalog& catalog)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return false;
    }
    std::vector<SpellRingItem>& ringItems = itemsByRing_[static_cast<std::size_t>(ringIndex)];
    if (itemIndex < 0 || itemIndex >= static_cast<int>(ringItems.size())) {
        return false;
    }

    SpellRingItem& item = ringItems[static_cast<std::size_t>(itemIndex)];
    int& modeEnhanceLevel =
        attackLevelDelta > 0 ? item.attackEnhanceLevel :
        digLevelDelta > 0 ? item.digEnhanceLevel :
        item.durabilityEnhanceLevel;
    if (modeEnhanceLevel >= maxEnhanceLevel) {
        return false;
    }

    ++item.enhanceLevel;
    item.attackEnhanceLevel += attackLevelDelta;
    item.digEnhanceLevel += digLevelDelta;
    item.durabilityEnhanceLevel += durabilityLevelDelta;
    item.attackBonus += attackBonus;
    item.digBonus += digBonus;
    item.durabilityBonus += durabilityBonus;
    if (durabilityBonus > 0 && item.maxDurability >= 0) {
        item.durability = std::min(item.maxDurability + durabilityBonus, std::max(0, item.durability + durabilityBonus));
    }

    if (const ItemData* object = catalog.registry.findById(item.objectId)) {
        item.objectStatsApplied = false;
        applyObjectDefinition(item, *object);
    } else {
        item.damage += attackBonus;
        item.digPower += digBonus;
        if (durabilityBonus > 0 && item.maxDurability >= 0) {
            item.maxDurability += durabilityBonus;
        }
    }
    item.isBroken = item.durability == 0;
    if (!item.isBroken) {
        item.breakExplosion = {};
    }
    return true;
}

void SpellRingSystem::applyObjectParameters(const ObjectCatalog& catalog)
{
    if (catalog.objectsById.empty()) {
        return;
    }

    for (auto& ringItems : itemsByRing_) {
        for (SpellRingItem& item : ringItems) {
            if (item.objectStatsApplied || item.objectId.empty()) {
                continue;
            }

            const auto objectIt = catalog.objectsById.find(item.objectId);
            if (objectIt == catalog.objectsById.end()) {
                continue;
            }
            applyObjectDefinition(item, objectIt->second);
        }
    }
}

void SpellRingSystem::removeBrokenItems()
{
    for (auto& ringItems : itemsByRing_) {
        ringItems.erase(
            std::remove_if(ringItems.begin(), ringItems.end(), [](const SpellRingItem& item) {
                return item.broken() && !item.protectionEnabled && !item.breakExplosion.active;
            }),
            ringItems.end());
    }
}

bool SpellRingSystem::moveItemAngle(int index, float deltaRadians)
{
    auto& ringItems = activeItems();
    if (index < 0 || index >= static_cast<int>(ringItems.size())) {
        return false;
    }

    const RingOrbitTuning& tuning = orbitTuning_;
    const RingShape shape = runtimeRingShape();
    SpellRingItem& item = ringItems[static_cast<std::size_t>(index)];
    const float candidate = quantizeLocalParam(shape, item.localAngle + deltaRadians, tuning);
    if (!canPlaceItemAtAngle(item, candidate, index, tuning)) {
        return false;
    }

    item.localAngle = candidate;
    return true;
}

void SpellRingSystem::normalizeItemPlacements()
{
    const int previousActiveRing = activeRingIndex_;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        activeRingIndex_ = ringIndex;
        std::vector<SpellRingItem> original = std::move(activeItems());
        std::vector<SpellRingItem>& normalized = activeItems();
        normalized.clear();
        normalized.reserve(std::min(original.size(), static_cast<std::size_t>(maxItemCountForRing(ringIndex))));
        for (SpellRingItem& item : original) {
            if (normalized.size() >= static_cast<std::size_t>(maxItemCountForRing(ringIndex))) {
                break;
            }
            item.ringIndex = ringIndex;
            item.localAngle = quantizeLocalParam(runtimeRingShape(), item.localAngle, orbitTuning_);
            if (!canPlaceItemAtAngle(item, item.localAngle, -1, orbitTuning_)) {
                const std::optional<float> angle = findBestPlacementAngle(item, -1, orbitTuning_);
                if (!angle) {
                    continue;
                }
                item.localAngle = *angle;
            }
            normalized.push_back(std::move(item));
        }
    }
    activeRingIndex_ = std::clamp(previousActiveRing, 0, SpellRingCount - 1);
}

void SpellRingSystem::switchActiveRing(int delta)
{
    if (anyRingInFlight()) {
        return;
    }
    activeRingIndex_ = (activeRingIndex_ + delta) % SpellRingCount;
    if (activeRingIndex_ < 0) {
        activeRingIndex_ += SpellRingCount;
    }
}

void SpellRingSystem::setRingShapeForIndex(int ringIndex, RingShape shape)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return;
    }
    ringShapes_[static_cast<std::size_t>(ringIndex)] = shape;
}

RingShape SpellRingSystem::ringShapeForIndex(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return RingShape::Circle;
    }
    return ringShapes_[static_cast<std::size_t>(ringIndex)];
}

RingShape SpellRingSystem::activeRingShape() const
{
    return ringShapeForIndex(activeRingIndex_);
}

RingShape SpellRingSystem::runtimeRingShape() const
{
    return activeRingShape();
}

float SpellRingSystem::ringBaseAngleForIndex(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return 0.0f;
    }
    return baseAngles_[static_cast<std::size_t>(ringIndex)];
}

float SpellRingSystem::shapeRotationForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return 0.0f;
    }
    return shapeRotations_[static_cast<std::size_t>(ringIndex)];
}

float SpellRingSystem::ringAngularSpeedForIndex(int ringIndex, const RuntimeBalance& balance) const
{
    return effectiveAngularSpeedForRing(ringIndex) *
        ringShapeOrbitSpeedMultiplier(ringShapeForIndex(ringIndex), balance) *
        ringBaseSpeedMultiplierForIndex(ringIndex);
}

RingOrbitContext SpellRingSystem::makeOrbitContext(int itemIndex, int itemCount, float radiusScale, const RuntimeBalance& balance) const
{
    return makeOrbitContextForRing(activeRingIndex_, itemIndex, itemCount, radiusScale, balance);
}

RingOrbitContext SpellRingSystem::makeOrbitContextForRing(int ringIndex, int itemIndex, int itemCount, float radiusScale, const RuntimeBalance& balance) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const double orbitRadiusMultiplier = std::clamp(
        orbitModifiers_.gravityMultiplier * orbitModifiers_.antigravityMultiplier,
        0.25,
        3.0);
    RingOrbitContext context;
    context.shape = ringShapeForIndex(clampedRingIndex);
    context.radius = std::max(
        1.0f,
        radiusForRing(clampedRingIndex) * ringBaseRadiusMultiplierForIndex(clampedRingIndex) *
            std::max(0.1f, radiusScale) *
            static_cast<float>(orbitRadiusMultiplier));
    context.shapeRotation = context.shape == RingShape::FigureEight
        ? shapeRotations_[static_cast<std::size_t>(clampedRingIndex)]
        : (context.shape == RingShape::Comet ? baseAngles_[static_cast<std::size_t>(clampedRingIndex)] : 0.0f);
    context.itemIndex = std::max(0, itemIndex);
    context.itemCount = std::max(1, itemCount);
    context.tuning = makeRingOrbitTuning(balance);
    return context;
}

Vec2 SpellRingSystem::sampleItemWorldPosition(float localAngle, int itemIndex, int itemCount, float radiusScale, const RuntimeBalance& balance) const
{
    return sampleItemWorldPositionForRing(activeRingIndex_, localAngle, itemIndex, itemCount, radiusScale, balance);
}

Vec2 SpellRingSystem::sampleItemWorldPositionForRing(
    int ringIndex,
    float localAngle,
    int itemIndex,
    int itemCount,
    float radiusScale,
    const RuntimeBalance& balance) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    RingOrbitContext context = makeOrbitContextForRing(clampedRingIndex, itemIndex, itemCount, radiusScale, balance);
    const float param = context.shape == RingShape::Comet
        ? normalizeLocalParam(context.shape, localAngle, context.tuning)
        : normalizeAngle(baseAngles_[static_cast<std::size_t>(clampedRingIndex)] + localAngle);
    return getRingItemWorldPosition(centerForRing(clampedRingIndex), param, context);
}

float SpellRingSystem::nearestPathParam(Vec2 worldPoint, Vec2 center, float radiusScale, const RuntimeBalance& balance, int sampleCount) const
{
    return nearestPathParamForRing(activeRingIndex_, worldPoint, center, radiusScale, balance, sampleCount);
}

float SpellRingSystem::nearestPathParamForRing(
    int ringIndex,
    Vec2 worldPoint,
    Vec2 center,
    float radiusScale,
    const RuntimeBalance& balance,
    int sampleCount) const
{
    RingOrbitContext context = makeOrbitContextForRing(ringIndex, 0, 1, radiusScale, balance);
    context.shapeRotation = context.shape == RingShape::Comet ? 0.0f : context.shapeRotation;
    return findNearestRingPathParam(worldPoint, center, context, sampleCount);
}

std::vector<Vec2> SpellRingSystem::pathSamplePoints(Vec2 center, float radiusScale, const RuntimeBalance& balance, int sampleCount) const
{
    return pathSamplePointsForRing(activeRingIndex_, center, radiusScale, balance, sampleCount);
}

std::vector<Vec2> SpellRingSystem::pathSamplePointsForRing(
    int ringIndex,
    Vec2 center,
    float radiusScale,
    const RuntimeBalance& balance,
    int sampleCount) const
{
    RingOrbitContext context = makeOrbitContextForRing(ringIndex, 0, 1, radiusScale, balance);
    return getRingPathSamplePoints(center, context, sampleCount);
}

std::vector<Vec2> SpellRingSystem::runtimePathSamplePointsForRing(
    int ringIndex,
    const RuntimeBalance& balance,
    int sampleCount) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const RingRuntimeState& runtime = ringRuntime_[static_cast<std::size_t>(clampedRingIndex)];
    RingOrbitContext context = makeOrbitContextForRing(clampedRingIndex, 0, 1, 1.0f, balance);
    if (runtime.state == SpellRingState::Normal) {
        return getRingPathSamplePoints(runtime.center, context, sampleCount);
    }

    context.shapeRotation = runtime.throwShapeRotation;
    const Vec2 throwOrigin = throwPresentationOrigin(
        runtime.homeCenter,
        runtime.throwDirection,
        runtime.throwElapsed,
        runtime.throwPeakTime,
        runtime.throwSettleTime);
    const ThrowMasterPath throwPath = makeThrowMasterPath(
        throwOrigin,
        context,
        runtime.throwDirection,
        runtime.throwLaunchOffset,
        runtime.throwReturnOffset,
        runtime.throwCutPathT,
        runtime.throwReturnPathT,
        runtime.throwDistance);
    const ThrowVisibleWindow throwWindow = throwVisibleWindowForElapsed(
        throwPath,
        context.shape,
        runtime.throwElapsed,
        runtime.throwPeakTime,
        runtime.throwReturnTime);
    return sampleThrowMasterVisiblePoints(
        throwPath,
        throwWindow,
        throwOrigin,
        runtime.throwDirection,
        sampleCount);
}

float SpellRingSystem::normalizeLocalAngle(float angle, const RuntimeBalance& balance) const
{
    const RingOrbitTuning tuning = makeRingOrbitTuning(balance);
    return normalizeLocalParam(runtimeRingShape(), angle, tuning);
}

float SpellRingSystem::quantizeLocalAngle(float angle, const RuntimeBalance& balance) const
{
    const RingOrbitTuning tuning = makeRingOrbitTuning(balance);
    return quantizeLocalParam(runtimeRingShape(), angle, tuning);
}

float SpellRingSystem::cooldownRatio(const RuntimeBalance& balance) const
{
    return cooldownRatioForRing(activeRingIndex_, balance);
}

float SpellRingSystem::cooldownRatioForRing(int ringIndex, const RuntimeBalance& balance) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return 0.0f;
    }

    const float cooldown = throwCooldownForRing(ringIndex, balance);
    return clamp(ringRuntime_[static_cast<std::size_t>(ringIndex)].throwCooldownRemaining / cooldown, 0.0f, 1.0f);
}

float SpellRingSystem::effectiveAngularSpeed() const
{
    return effectiveAngularSpeedForRing(activeRingIndex_);
}

float SpellRingSystem::effectiveAngularSpeedForRing(int ringIndex) const
{
    return static_cast<float>(
        static_cast<double>(angularSpeedForRing(ringIndex)) *
        static_cast<double>(weightSpeedMultiplierForRing(ringIndex)) *
        orbitModifiers_.speedMultiplier *
        static_cast<double>(enemyOrbitSpeedDebuffMultiplier_));
}

const RingEquipmentModifiers& SpellRingSystem::equipmentModifiersForRing(int ringIndex) const
{
    return ringEquipmentModifiersForRing(equipmentModifiers_, ringIndex);
}

double SpellRingSystem::ringOutputMultiplierForRing(int ringIndex) const
{
    return nonNegativeEquipmentMultiplier(equipmentModifiersForRing(ringIndex).ringOutputMul);
}

double SpellRingSystem::ringDamageSpeedMultiplierForRing(int ringIndex) const
{
    return nonNegativeEquipmentMultiplier(equipmentModifiersForRing(ringIndex).ringDamageSpeedMul);
}

double SpellRingSystem::digPowerMultiplierForRing(int ringIndex) const
{
    return nonNegativeEquipmentMultiplier(equipmentModifiersForRing(ringIndex).digPowerMul);
}

int SpellRingSystem::applyDirectionalWind(Vec2 center, Vec2 direction, float dt, float radius, float strength)
{
    const float safeDt = std::max(0.0f, dt);
    const float effectiveRadius = std::max(1.0f, radius);
    const float clampedStrength = std::clamp(strength, 0.1f, 4.0f);
    if (safeDt <= 0.0f || clampedStrength <= 0.0f || lengthSquared(direction) <= 0.0001f) {
        return 0;
    }

    const Vec2 windDirection = safeNormalize(direction);
    const float radiusSq = effectiveRadius * effectiveRadius;
    int affected = 0;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        float bestFalloff = 0.0f;
        const auto considerPosition = [&](Vec2 position) {
            const float distanceSq = distanceSquared(position, center);
            if (distanceSq > radiusSq) {
                return;
            }
            const float distance = std::sqrt(std::max(0.0f, distanceSq));
            const float falloff = 0.35f + (1.0f - clamp(distance / effectiveRadius, 0.0f, 1.0f)) * 0.65f;
            bestFalloff = std::max(bestFalloff, falloff);
        };

        RingRuntimeState& runtime = ringRuntime_[static_cast<std::size_t>(ringIndex)];
        considerPosition(runtime.center);
        for (const SpellRingItem& item : itemsByRing_[static_cast<std::size_t>(ringIndex)]) {
            if (!item.broken()) {
                considerPosition(item.worldPosition);
            }
        }

        if (bestFalloff <= 0.0f) {
            continue;
        }

        runtime.externalWindVelocity += windDirection * (RingWindAcceleration * clampedStrength * bestFalloff * safeDt);
        const float velocityLength = length(runtime.externalWindVelocity);
        if (velocityLength > RingWindMaxVelocity) {
            runtime.externalWindVelocity = runtime.externalWindVelocity / velocityLength * RingWindMaxVelocity;
        }
        ++affected;
    }
    return affected;
}

Vec2 SpellRingSystem::centerForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return ringRuntime_[0].center;
    }
    return ringRuntime_[static_cast<std::size_t>(ringIndex)].center;
}

SpellRingState SpellRingSystem::stateForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return SpellRingState::Normal;
    }
    return ringRuntime_[static_cast<std::size_t>(ringIndex)].state;
}

float SpellRingSystem::throwVisualEnergyForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return 0.0f;
    }

    const RingRuntimeState& runtime = ringRuntime_[static_cast<std::size_t>(ringIndex)];
    if (runtime.state != SpellRingState::Normal) {
        return 1.0f;
    }

    if (ThrowVisualEnergyFadeDuration <= 0.0f) {
        return 0.0f;
    }
    return clamp(runtime.throwVisualEnergyFadeTimer / ThrowVisualEnergyFadeDuration, 0.0f, 1.0f);
}

bool SpellRingSystem::anyRingInFlight() const
{
    for (const RingRuntimeState& runtime : ringRuntime_) {
        if (runtime.state != SpellRingState::Normal) {
            return true;
        }
    }
    return false;
}

std::vector<RingMotionEvent> SpellRingSystem::consumeMotionEvents()
{
    std::vector<RingMotionEvent> events = std::move(motionEvents_);
    motionEvents_.clear();
    return events;
}

float SpellRingSystem::radiusForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return radii_[0];
    }
    return radii_[static_cast<std::size_t>(ringIndex)];
}

float SpellRingSystem::orbitRadiusForRing(int ringIndex) const
{
    return radiusForRing(ringIndex) * ringBaseRadiusMultiplierForIndex(ringIndex);
}

float SpellRingSystem::angularSpeedForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return angularSpeeds_[0];
    }
    return angularSpeeds_[static_cast<std::size_t>(ringIndex)];
}

const std::vector<SpellRingItem>& SpellRingSystem::itemsForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return itemsByRing_[0];
    }
    return itemsByRing_[static_cast<std::size_t>(ringIndex)];
}

std::vector<SpellRingItem>& SpellRingSystem::itemsForRing(int ringIndex)
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return itemsByRing_[0];
    }
    return itemsByRing_[static_cast<std::size_t>(ringIndex)];
}

std::vector<const SpellRingItem*> SpellRingSystem::runtimeItems() const
{
    std::vector<const SpellRingItem*> result;
    std::size_t total = 0;
    for (const auto& ringItems : itemsByRing_) {
        total += ringItems.size();
    }
    result.reserve(total);
    for (const auto& ringItems : itemsByRing_) {
        for (const SpellRingItem& item : ringItems) {
            result.push_back(&item);
        }
    }
    return result;
}

std::vector<SpellRingItem*> SpellRingSystem::runtimeItemsMutable()
{
    std::vector<SpellRingItem*> result;
    std::size_t total = 0;
    for (const auto& ringItems : itemsByRing_) {
        total += ringItems.size();
    }
    result.reserve(total);
    for (auto& ringItems : itemsByRing_) {
        for (SpellRingItem& item : ringItems) {
            result.push_back(&item);
        }
    }
    return result;
}

float SpellRingSystem::totalEquippedWeight() const
{
    return totalEquippedWeightForRing(activeRingIndex_);
}

float SpellRingSystem::totalEquippedWeightForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return 0.0f;
    }

    float total = 0.0f;
    const auto& ringItems = itemsByRing_[static_cast<std::size_t>(ringIndex)];
    for (const SpellRingItem& item : ringItems) {
        total += std::max(0.0f, item.weight);
    }
    return total;
}

float SpellRingSystem::maxEquippedWeight() const
{
    return maxEquippedWeightForRing(activeRingIndex_);
}

float SpellRingSystem::maxEquippedWeightForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return SpellRingSystem::InitialMaxEquippedWeight;
    }
    return maxEquippedWeights_[static_cast<std::size_t>(ringIndex)];
}

float SpellRingSystem::overweightEquipLimitForRing(int ringIndex) const
{
    return maxEquippedWeightForRing(ringIndex) * OverweightEquipLimitRatio;
}

int SpellRingSystem::maxItemCount() const
{
    return maxItemCountForRing(activeRingIndex_);
}

int SpellRingSystem::maxItemCountForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return static_cast<int>(MaxSpellRingItems);
    }
    const RingWorkshopModifiers& modifiers = workshopModifiersByRing_[static_cast<std::size_t>(ringIndex)];
    return static_cast<int>(MaxSpellRingItems) + std::clamp(modifiers.extraEquipSlots, 0, 10);
}

float SpellRingSystem::weightSpeedMultiplier() const
{
    return weightSpeedMultiplierForRing(activeRingIndex_);
}

float SpellRingSystem::weightSpeedMultiplierForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return 1.0f;
    }

    const float limit = maxEquippedWeightForRing(ringIndex);
    if (limit <= 0.0f) {
        return totalEquippedWeightForRing(ringIndex) <= 0.0f ? 1.0f : 0.0f;
    }

    const float weightRatio = totalEquippedWeightForRing(ringIndex) / limit;
    if (weightRatio <= 1.0f) {
        return 1.0f;
    }

    const float stopRatio = weightStopRatioForRing(ringIndex);
    if (stopRatio <= 1.0f) {
        return 0.0f;
    }

    const float progress = (weightRatio - 1.0f) / (stopRatio - 1.0f);
    return std::clamp(1.0f - progress, 0.0f, 1.0f);
}

float SpellRingSystem::weightStopRatioForRing(int ringIndex) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return BaseWeightStopRatio;
    }

    const float equipmentStopRatio = weightStopRatioForPenaltyMultiplier(
        equipmentModifiersForRing(ringIndex).metalWeightPenaltyMul);
    const float workshopStopRatio = BaseWeightStopRatio +
        workshopModifiersByRing_[static_cast<std::size_t>(ringIndex)].weightStopRatioBonus;
    return std::clamp(std::max(equipmentStopRatio, workshopStopRatio), 1.0f, MaxWeightStopRatio);
}

bool SpellRingSystem::canPlaceItemAtAngle(const SpellRingItem&, float angle, int ignoreIndex, const RingOrbitTuning& tuning) const
{
    return canPlaceItemAtAngleForRing(activeRingIndex_, SpellRingItem{}, angle, ignoreIndex, tuning);
}

bool SpellRingSystem::canPlaceItemAtAngleForRing(
    int ringIndex,
    const SpellRingItem&,
    float angle,
    int ignoreIndex,
    const RingOrbitTuning& tuning) const
{
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return false;
    }

    const auto& ringItems = itemsByRing_[static_cast<std::size_t>(ringIndex)];
    const RingShape shape = ringShapeForIndex(ringIndex);
    const float candidate = quantizeLocalParam(shape, angle, tuning);
    if (shape == RingShape::Circle) {
        for (std::size_t i = 0; i < ringItems.size(); ++i) {
            if (static_cast<int>(i) == ignoreIndex) {
                continue;
            }
            if (pathParamDistance(shape, candidate, ringItems[i].localAngle, tuning) < ItemAngularSizeRadians - 0.0001f) {
                return false;
            }
        }
        return true;
    }

    RuntimeBalance defaultBalance{};
    defaultBalance.figure8WidthMultiplier = tuning.figure8WidthMultiplier;
    defaultBalance.figure8HeightMultiplier = tuning.figure8HeightMultiplier;
    defaultBalance.figure8ShapeRotationSpeed = tuning.figure8ShapeRotationSpeed;
    defaultBalance.cometRadiusMultiplier = tuning.cometRadiusMultiplier;
    defaultBalance.cometArcDegrees = tuning.cometArcDegrees;
    defaultBalance.cometSpeedMultiplier = tuning.cometSpeedMultiplier;
    defaultBalance.cometTrailLength = tuning.cometTrailLength;
    defaultBalance.cometLaneSpacing = tuning.cometLaneSpacing;
    defaultBalance.cometMaxArcDegrees = tuning.cometMaxArcDegrees;
    const int itemCount = static_cast<int>(ringItems.size());
    const Vec2 candidatePos = sampleItemWorldPositionForRing(
        ringIndex,
        candidate,
        ignoreIndex < 0 ? itemCount : ignoreIndex,
        itemCount,
        1.0f,
        defaultBalance);
    for (std::size_t i = 0; i < ringItems.size(); ++i) {
        if (static_cast<int>(i) == ignoreIndex) {
            continue;
        }
        const Vec2 otherPos = sampleItemWorldPositionForRing(
            ringIndex,
            ringItems[i].localAngle,
            static_cast<int>(i),
            itemCount,
            1.0f,
            defaultBalance);
        if (distanceSquared(candidatePos, otherPos) < 13.0f * 13.0f) {
            return false;
        }
    }
    return true;
}

std::optional<float> SpellRingSystem::findBestPlacementAngle(const SpellRingItem& item, int ignoreIndex, const RingOrbitTuning& tuning) const
{
    return findBestPlacementAngleForRing(activeRingIndex_, item, ignoreIndex, tuning);
}

std::optional<float> SpellRingSystem::findBestPlacementAngleForRing(
    int ringIndex,
    const SpellRingItem& item,
    int ignoreIndex,
    const RingOrbitTuning& tuning) const
{
    (void)item;
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return std::nullopt;
    }

    const auto& ringItems = itemsByRing_[static_cast<std::size_t>(ringIndex)];
    if (ringItems.empty() || (ringItems.size() == 1 && ignoreIndex == 0)) {
        return ringShapeForIndex(ringIndex) == RingShape::Comet ? 0.0f : 0.0f;
    }

    const RingShape shape = ringShapeForIndex(ringIndex);
    const int stepCount = shape == RingShape::Comet ? 96 : 72;
    std::optional<float> bestAngle;
    float bestDistance = -1.0f;
    for (int step = 0; step < stepCount; ++step) {
        const float t01 = static_cast<float>(step) / static_cast<float>(stepCount - 1);
        const float candidate = quantizeLocalParam(shape, sampleParamForShape(shape, t01, tuning), tuning);
        if (!canPlaceItemAtAngleForRing(ringIndex, item, candidate, ignoreIndex, tuning)) {
            continue;
        }

        float nearestDistance = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < ringItems.size(); ++i) {
            if (static_cast<int>(i) == ignoreIndex) {
                continue;
            }
            nearestDistance = std::min(nearestDistance, pathParamDistance(shape, candidate, ringItems[i].localAngle, tuning));
        }

        if (!bestAngle || nearestDistance > bestDistance + 0.0001f) {
            bestAngle = candidate;
            bestDistance = nearestDistance;
        }
    }

    return bestAngle;
}

}
