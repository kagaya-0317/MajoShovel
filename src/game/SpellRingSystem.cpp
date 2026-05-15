#include "game/SpellRingSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace majo {

namespace {
constexpr std::size_t MaxSpellRingItems = 24;
constexpr float MaxSpellRingWeight = 24.0f;
constexpr float PlacementStepRadians = Pi / 36.0f;
constexpr float ItemAngularSizeRadians = Pi / 12.0f;
constexpr float CapturedJumpInterval = 2.0f;
constexpr float CapturedJumpDuration = 0.24f;
constexpr float CapturedJumpDistance = 18.0f;
constexpr float CapturedPeriodicHealInterval = 8.0f;
constexpr int CapturedPeriodicHealMaxPerPulse = 2;
constexpr float FullCircleRadians = Pi * 2.0f;
constexpr std::array<float, SpellRingCount> RingBaseRadiusMultipliers{{
    1.2f,
    1.8f,
    2.4f,
}};
constexpr std::array<float, SpellRingCount> RingBaseSpeedMultipliers{{
    1.0f,
    1.0f,
    0.8f,
}};

SpellRingItem makeSpellRingItem(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return makeShovel();
    case SpellRingItemType::Torch: return makeTorch();
    case SpellRingItemType::Object: return makeObjectRingItem({});
    }
    return makeObjectRingItem({});
}

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
    item.damage = object.attackPower + item.attackBonus;
    item.damageType = object.damageType.empty() ? "none" : object.damageType;
    item.digPower = object.digPower + item.digBonus;
    item.maxDurability = object.durability < 0 ? object.durability : object.durability + item.durabilityBonus;
    item.durability = previousDurability >= 0 ? std::min(previousDurability, std::max(0, item.maxDurability)) : item.maxDurability;
    item.weight = static_cast<float>(std::max(0.0, object.weightKg * item.weightModifier));
    item.hitRadius = static_cast<float>(std::max(1.0, static_cast<double>(item.hitRadius) * item.sizeModifier));
    item.capturedBehaviorIds = object.capturedBehaviorIds;
    item.capturedBehaviorSpecs = object.capturedBehaviorSpecs;
    item.capturedBehaviorId = item.capturedBehaviorIds.empty() ? std::string{} : item.capturedBehaviorIds.front();
    item.isBroken = item.durability == 0;
    item.objectStatsApplied = true;
}

void applyItemInstance(SpellRingItem& item, const ItemInstance& instance)
{
    item.instanceId = instance.instanceId;
    item.enhanceLevel = instance.enhanceLevel;
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

Vec2 getRingCenterWorldPosition(Vec2 playerPosition, Vec2 playerFacing, float spellRingShift)
{
    return playerPosition + playerFacing * spellRingShift;
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

Vec2 getRingItemWorldPosition(Vec2 center, float localAngle, const RingOrbitContext& context)
{
    return center + getRingItemLocalPosition(localAngle, context);
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

    radius_ = balance.spellRingRadius;
    angularSpeed_ = balance.spellRingSpeed;
    orbitTuning_ = makeRingOrbitTuning(balance);
    baseEquippedWeight_ = totalEquippedWeight();
    baseAngles_.fill(0.0f);
    shapeRotations_.fill(0.0f);
    center_ = {};
    orbitModifiers_ = OrbitModifiers{};
    state_ = SpellRingState::Normal;
    capturedHealTimer_ = CapturedPeriodicHealInterval;
    enemyOrbitSpeedDebuffMultiplier_ = 1.0f;
    enemyOrbitSpeedDebuffTimer_ = 0.0f;
    activeRingIndex_ = 0;
}

void SpellRingSystem::update(Player& player, const Input& input, float dt, float, bool paused, bool blockPointerThrow, const RuntimeBalance& balance)
{
    if (paused) {
        return;
    }

    enemyOrbitSpeedDebuffTimer_ = std::max(0.0f, enemyOrbitSpeedDebuffTimer_ - dt);
    if (enemyOrbitSpeedDebuffTimer_ <= 0.0f) {
        enemyOrbitSpeedDebuffMultiplier_ = 1.0f;
    }

    const RingOrbitTuning tuning = makeRingOrbitTuning(balance);
    orbitTuning_ = tuning;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const RingShape ringShape = ringShapeForIndex(ringIndex);
        baseAngles_[static_cast<std::size_t>(ringIndex)] = normalizeAngle(
            baseAngles_[static_cast<std::size_t>(ringIndex)] + ringAngularSpeedForIndex(ringIndex, balance) * dt);
        if (ringShape == RingShape::FigureEight) {
            shapeRotations_[static_cast<std::size_t>(ringIndex)] = normalizeAngle(
                shapeRotations_[static_cast<std::size_t>(ringIndex)] + std::max(0.0f, tuning.figure8ShapeRotationSpeed) * dt);
        }
    }

    const Vec2 normalCenter = getRingCenterWorldPosition(player.position, player.facing, player.spellRingShift);
    const Vec2 previousCenter = center_;

    if (state_ == SpellRingState::Normal) {
        center_ = normalCenter;
        const bool throwPressed = input.throwPressed() && !(blockPointerThrow && input.mouseLeftPressed());
        if (throwPressed && player.throwCooldownRemaining <= 0.0f) {
            state_ = SpellRingState::Thrown;
            throwDirection_ = player.facing;
            throwStart_ = center_;
            throwTime_ = 0.0f;
            player.throwCooldownRemaining = balance.spellRingThrowCooldown;
        }
    } else if (state_ == SpellRingState::Thrown) {
        throwTime_ += dt;
        center_ += throwDirection_ * balance.spellRingThrowSpeed * dt;
        if (distanceSquared(center_, throwStart_) >= balance.spellRingThrowDistance * balance.spellRingThrowDistance ||
            throwTime_ >= balance.spellRingThrowMaxTime) {
            state_ = SpellRingState::Returning;
        }
    } else {
        const Vec2 toPlayer = normalCenter - center_;
        const float dist = length(toPlayer);
        if (dist < 14.0f) {
            center_ = normalCenter;
            state_ = SpellRingState::Normal;
        } else {
            center_ += normalize(toPlayer) * balance.spellRingReturnSpeed * dt;
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
        capturedHealTimer_ -= dt * stackedRate;
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

    const Vec2 centerVelocity = dt > 0.0f ? (center_ - previousCenter) / dt : Vec2{};
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
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
                item.capturedBehaviorTimer += dt;
                item.capturedJumpTimer = std::max(0.0f, item.capturedJumpTimer - dt);
                if (item.capturedBehaviorTimer >= jumpInterval) {
                    item.capturedBehaviorTimer = 0.0f;
                    item.capturedJumpTimer = jumpDuration;
                }
                if (item.capturedJumpTimer > 0.0f) {
                    const float phase = 1.0f - item.capturedJumpTimer / jumpDuration;
                    itemRadiusScale += (std::sin(phase * Pi) * jumpDistance) / std::max(1.0f, radius_);
                }
            } else {
                item.capturedJumpTimer = 0.0f;
            }

            RingOrbitContext context = makeOrbitContextForRing(ringIndex, itemIndex, ringItemCount, itemRadiusScale, balance);
            const float param = ringShape == RingShape::Comet
                ? normalizeLocalParam(ringShape, item.localAngle, context.tuning)
                : normalizeAngle(baseAngles_[static_cast<std::size_t>(ringIndex)] + item.localAngle);
            item.worldPosition = getRingItemWorldPosition(center_, param, context);
            item.worldVelocity = getRingItemVelocity(
                param,
                ringShape == RingShape::Comet ? 0.0f : ringAngularSpeed,
                shapeRotationSpeed,
                centerVelocity,
                context);
            item.orbitMotionSpeed = length(item.worldVelocity) / std::max(1.0f, radius_);
        }
    }
}

void SpellRingSystem::upgradeShovelPower(int amount)
{
    for (auto& ringItems : itemsByRing_) {
        for (auto& item : ringItems) {
            if (item.type == SpellRingItemType::Shovel) {
                item.damage += amount;
                item.digPower += amount;
            }
        }
    }
}

void SpellRingSystem::clearOrbitModifiers()
{
    orbitModifiers_ = OrbitModifiers{};
}

void SpellRingSystem::setOrbitModifiers(OrbitModifiers modifiers)
{
    orbitModifiers_ = std::move(modifiers);
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

bool SpellRingSystem::canAddItem() const
{
    return activeItems().size() < MaxSpellRingItems;
}

bool SpellRingSystem::canAddItem(const SpellRingItem& item) const
{
    return canAddItem() && totalEquippedWeight() + std::max(0.0f, item.weight) <= MaxSpellRingWeight;
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
        ? std::max(Pi / 180.0f, clampCometArcRadians(tuning) / static_cast<float>(MaxSpellRingItems * 2))
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
    if (!canAddItem(item)) {
        return false;
    }
    const std::optional<float> angle = findBestPlacementAngle(item, -1, orbitTuning_);
    if (!angle) {
        return false;
    }
    item.ringIndex = activeRingIndex_;
    item.localAngle = *angle;
    const SpellRingAddResult result{
        .ringIndex = activeRingIndex_,
        .itemIndex = static_cast<int>(activeItems().size()),
        .localAngle = item.localAngle,
        .objectId = item.objectId,
        .instanceId = item.instanceId,
    };
    activeItems().push_back(std::move(item));
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool SpellRingSystem::addObjectItem(const ItemData& item, SpellRingAddResult* outResult)
{
    if (!canAddItem() || item.id.empty()) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItem(item.id);
    applyObjectDefinition(ringItem, item);
    return addItem(std::move(ringItem), outResult);
}

bool SpellRingSystem::addObjectItem(const ItemData& item, const ItemInstance& instance, SpellRingAddResult* outResult)
{
    if (!canAddItem() || item.id.empty() || instance.objectId != item.id) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItem(item.id);
    applyItemInstance(ringItem, instance);
    applyObjectDefinition(ringItem, item);
    return addItem(std::move(ringItem), outResult);
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
                return item.broken() && !item.protectionEnabled;
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
        normalized.reserve(std::min(original.size(), MaxSpellRingItems));
        for (SpellRingItem& item : original) {
            if (normalized.size() >= MaxSpellRingItems) {
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

void SpellRingSystem::resetBaseWeightToCurrent()
{
    baseEquippedWeight_ = totalEquippedWeight();
}

void SpellRingSystem::switchActiveRing(int delta)
{
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
    return effectiveAngularSpeed() *
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
    RingOrbitContext context;
    context.shape = ringShapeForIndex(clampedRingIndex);
    context.radius = std::max(1.0f, radius_ * ringBaseRadiusMultiplierForIndex(clampedRingIndex) * std::max(0.1f, radiusScale));
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
    return getRingItemWorldPosition(center_, param, context);
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

float SpellRingSystem::cooldownRatio(const Player& player, const RuntimeBalance& balance) const
{
    return clamp(player.throwCooldownRemaining / balance.spellRingThrowCooldown, 0.0f, 1.0f);
}

float SpellRingSystem::effectiveAngularSpeed() const
{
    return static_cast<float>(
        static_cast<double>(angularSpeed_) *
        static_cast<double>(weightSpeedMultiplier()) *
        orbitModifiers_.speedMultiplier *
        static_cast<double>(enemyOrbitSpeedDebuffMultiplier_));
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
    float total = 0.0f;
    for (const auto& ringItems : itemsByRing_) {
        for (const SpellRingItem& item : ringItems) {
            total += std::max(0.0f, item.weight);
        }
    }
    return total;
}

float SpellRingSystem::maxEquippedWeight() const
{
    return MaxSpellRingWeight;
}

int SpellRingSystem::maxItemCount() const
{
    return static_cast<int>(MaxSpellRingItems);
}

float SpellRingSystem::weightSpeedMultiplier() const
{
    const float excessWeight = std::max(0.0f, totalEquippedWeight() - baseEquippedWeight_);
    return 1.0f / (1.0f + excessWeight * 0.035f);
}

bool SpellRingSystem::canPlaceItemAtAngle(const SpellRingItem&, float angle, int ignoreIndex, const RingOrbitTuning& tuning) const
{
    const auto& ringItems = activeItems();
    const RingShape shape = runtimeRingShape();
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
    const Vec2 candidatePos = sampleItemWorldPosition(candidate, ignoreIndex < 0 ? itemCount : ignoreIndex, itemCount, 1.0f, defaultBalance);
    for (std::size_t i = 0; i < ringItems.size(); ++i) {
        if (static_cast<int>(i) == ignoreIndex) {
            continue;
        }
        const Vec2 otherPos = sampleItemWorldPosition(ringItems[i].localAngle, static_cast<int>(i), itemCount, 1.0f, defaultBalance);
        if (distanceSquared(candidatePos, otherPos) < 13.0f * 13.0f) {
            return false;
        }
    }
    return true;
}

std::optional<float> SpellRingSystem::findBestPlacementAngle(const SpellRingItem& item, int ignoreIndex, const RingOrbitTuning& tuning) const
{
    (void)item;
    const auto& ringItems = activeItems();
    if (ringItems.empty() || (ringItems.size() == 1 && ignoreIndex == 0)) {
        return runtimeRingShape() == RingShape::Comet ? 0.0f : 0.0f;
    }

    const RingShape shape = runtimeRingShape();
    const int stepCount = shape == RingShape::Comet ? 96 : 72;
    std::optional<float> bestAngle;
    float bestDistance = -1.0f;
    for (int step = 0; step < stepCount; ++step) {
        const float t01 = static_cast<float>(step) / static_cast<float>(stepCount - 1);
        const float candidate = quantizeLocalParam(shape, sampleParamForShape(shape, t01, tuning), tuning);
        if (!canPlaceItemAtAngle(item, candidate, ignoreIndex, tuning)) {
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
