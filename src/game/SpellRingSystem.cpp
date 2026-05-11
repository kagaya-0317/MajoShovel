#include "game/SpellRingSystem.hpp"

#include <algorithm>
#include <cmath>
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
    const float full = Pi * 2.0f;
    angle = std::fmod(angle, full);
    if (angle < 0.0f) {
        angle += full;
    }
    return angle;
}

float quantizeAngle(float angle)
{
    return normalizeAngle(std::round(normalizeAngle(angle) / PlacementStepRadians) * PlacementStepRadians);
}

float angleDistance(float a, float b)
{
    const float full = Pi * 2.0f;
    float diff = std::fabs(normalizeAngle(a) - normalizeAngle(b));
    if (diff > Pi) {
        diff = full - diff;
    }
    return diff;
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

void SpellRingSystem::initialize(const RuntimeBalance& balance)
{
    items_.clear();
    SpellRingItem shovel = makeShovel();
    shovel.localAngle = 0.0f;
    SpellRingItem torch = makeTorch();
    torch.localAngle = Pi;
    items_.push_back(std::move(shovel));
    items_.push_back(std::move(torch));
    radius_ = balance.spellRingRadius;
    angularSpeed_ = balance.spellRingSpeed;
    baseEquippedWeight_ = totalEquippedWeight();
    baseAngle_ = 0.0f;
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

    baseAngle_ += effectiveAngularSpeed() * dt;
    const Vec2 normalCenter = player.position + player.facing * player.spellRingShift;

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

    if (items_.empty()) {
        return;
    }

    int periodicHealCount = 0;
    for (const SpellRingItem& item : items_) {
        if (!item.broken() && item.hasCapturedBehavior("periodic_heal")) {
            ++periodicHealCount;
        }
    }
    if (periodicHealCount > 0) {
        const float stackedRate = std::min(1.0f, 0.65f + 0.15f * static_cast<float>(periodicHealCount));
        capturedHealTimer_ -= dt * stackedRate;
        if (capturedHealTimer_ <= 0.0f) {
            if (player.hp > 0 && player.hp < player.maxHp) {
                player.hp = std::min(player.maxHp, player.hp + 1);
            }
            capturedHealTimer_ = CapturedPeriodicHealInterval;
        }
    } else {
        capturedHealTimer_ = CapturedPeriodicHealInterval;
    }

    for (std::size_t i = 0; i < items_.size(); ++i) {
        SpellRingItem& item = items_[i];
        const float angle = baseAngle_ + item.localAngle;
        float itemRadius = radius_;
        if (item.hasCapturedBehavior("jump_outward")) {
            item.capturedBehaviorTimer += dt;
            item.capturedJumpTimer = std::max(0.0f, item.capturedJumpTimer - dt);
            if (item.capturedBehaviorTimer >= CapturedJumpInterval) {
                item.capturedBehaviorTimer = 0.0f;
                item.capturedJumpTimer = CapturedJumpDuration;
            }
            if (item.capturedJumpTimer > 0.0f) {
                const float phase = 1.0f - item.capturedJumpTimer / CapturedJumpDuration;
                itemRadius += std::sin(phase * Pi) * CapturedJumpDistance;
            }
        } else {
            item.capturedJumpTimer = 0.0f;
        }
        item.worldPosition = center_ + fromAngle(angle) * itemRadius;
    }
}

void SpellRingSystem::upgradeShovelPower(int amount)
{
    for (auto& item : items_) {
        if (item.type == SpellRingItemType::Shovel) {
            item.damage += amount;
            item.digPower += amount;
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
    for (auto& item : items_) {
        item.damage += amount;
    }
}

bool SpellRingSystem::canAddItem() const
{
    return items_.size() < MaxSpellRingItems;
}

bool SpellRingSystem::canAddItem(const SpellRingItem& item) const
{
    return canAddItem() && totalEquippedWeight() + std::max(0.0f, item.weight) <= MaxSpellRingWeight;
}

bool SpellRingSystem::canPlaceItemAtAngle(int index, float angle) const
{
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return false;
    }
    return canPlaceItemAtAngle(items_[static_cast<std::size_t>(index)], angle, index);
}

std::optional<float> SpellRingSystem::nearestPlaceableAngle(int index, float desiredAngle, float maxDeltaRadians) const
{
    if (index < 0 || index >= static_cast<int>(items_.size()) || maxDeltaRadians < 0.0f) {
        return std::nullopt;
    }

    const SpellRingItem& item = items_[static_cast<std::size_t>(index)];
    const float desired = quantizeAngle(desiredAngle);
    if (canPlaceItemAtAngle(item, desired, index)) {
        return desired;
    }

    const int maxSteps = static_cast<int>(std::floor(maxDeltaRadians / PlacementStepRadians + 0.0001f));
    for (int step = 1; step <= maxSteps; ++step) {
        const float delta = static_cast<float>(step) * PlacementStepRadians;
        const float clockwise = quantizeAngle(desired + delta);
        if (canPlaceItemAtAngle(item, clockwise, index)) {
            return clockwise;
        }
        const float counterClockwise = quantizeAngle(desired - delta);
        if (canPlaceItemAtAngle(item, counterClockwise, index)) {
            return counterClockwise;
        }
    }

    return std::nullopt;
}

bool SpellRingSystem::addItem(SpellRingItemType type)
{
    return addItem(makeSpellRingItem(type));
}

bool SpellRingSystem::addItem(SpellRingItem item)
{
    if (!canAddItem(item)) {
        return false;
    }
    const std::optional<float> angle = findBestPlacementAngle(item);
    if (!angle) {
        return false;
    }
    item.localAngle = *angle;
    items_.push_back(std::move(item));
    return true;
}

bool SpellRingSystem::addObjectItem(const ItemData& item)
{
    if (!canAddItem() || item.id.empty()) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItem(item.id);
    applyObjectDefinition(ringItem, item);
    return addItem(std::move(ringItem));
}

bool SpellRingSystem::addObjectItem(const ItemData& item, const ItemInstance& instance)
{
    if (!canAddItem() || item.id.empty() || instance.objectId != item.id) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItem(item.id);
    applyItemInstance(ringItem, instance);
    applyObjectDefinition(ringItem, item);
    return addItem(std::move(ringItem));
}

void SpellRingSystem::applyObjectParameters(const ObjectCatalog& catalog)
{
    if (catalog.objectsById.empty()) {
        return;
    }

    for (SpellRingItem& item : items_) {
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

void SpellRingSystem::removeBrokenItems()
{
    items_.erase(
        std::remove_if(items_.begin(), items_.end(), [](const SpellRingItem& item) {
            return item.broken() && !item.protectionEnabled;
        }),
        items_.end());
}

bool SpellRingSystem::moveItemAngle(int index, float deltaRadians)
{
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return false;
    }

    SpellRingItem& item = items_[static_cast<std::size_t>(index)];
    const float candidate = quantizeAngle(item.localAngle + deltaRadians);
    if (!canPlaceItemAtAngle(item, candidate, index)) {
        return false;
    }

    item.localAngle = candidate;
    return true;
}

void SpellRingSystem::normalizeItemPlacements()
{
    std::vector<SpellRingItem> original = std::move(items_);
    items_.clear();
    items_.reserve(std::min(original.size(), MaxSpellRingItems));
    for (SpellRingItem& item : original) {
        if (items_.size() >= MaxSpellRingItems) {
            break;
        }
        item.localAngle = quantizeAngle(item.localAngle);
        if (!canPlaceItemAtAngle(item, item.localAngle)) {
            const std::optional<float> angle = findBestPlacementAngle(item);
            if (!angle) {
                continue;
            }
            item.localAngle = *angle;
        }
        items_.push_back(std::move(item));
    }
}

void SpellRingSystem::resetBaseWeightToCurrent()
{
    baseEquippedWeight_ = totalEquippedWeight();
}

void SpellRingSystem::switchActiveRing(int delta)
{
    constexpr int RingCount = 3;
    activeRingIndex_ = (activeRingIndex_ + delta) % RingCount;
    if (activeRingIndex_ < 0) {
        activeRingIndex_ += RingCount;
    }
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

float SpellRingSystem::totalEquippedWeight() const
{
    float total = 0.0f;
    for (const SpellRingItem& item : items_) {
        total += std::max(0.0f, item.weight);
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

bool SpellRingSystem::canPlaceItemAtAngle(const SpellRingItem&, float angle, int ignoreIndex) const
{
    const float candidate = quantizeAngle(angle);
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (static_cast<int>(i) == ignoreIndex) {
            continue;
        }
        if (angleDistance(candidate, items_[i].localAngle) < ItemAngularSizeRadians - 0.0001f) {
            return false;
        }
    }
    return true;
}

std::optional<float> SpellRingSystem::findBestPlacementAngle(const SpellRingItem& item, int ignoreIndex) const
{
    if (items_.empty() || (items_.size() == 1 && ignoreIndex == 0)) {
        return 0.0f;
    }

    std::optional<float> bestAngle;
    float bestDistance = -1.0f;
    constexpr int StepCount = 72;
    for (int step = 0; step < StepCount; ++step) {
        const float candidate = static_cast<float>(step) * PlacementStepRadians;
        if (!canPlaceItemAtAngle(item, candidate, ignoreIndex)) {
            continue;
        }

        float nearestDistance = Pi * 2.0f;
        for (std::size_t i = 0; i < items_.size(); ++i) {
            if (static_cast<int>(i) == ignoreIndex) {
                continue;
            }
            nearestDistance = std::min(nearestDistance, angleDistance(candidate, items_[i].localAngle));
        }

        if (!bestAngle || nearestDistance > bestDistance + 0.0001f) {
            bestAngle = candidate;
            bestDistance = nearestDistance;
        }
    }

    return bestAngle;
}

}
