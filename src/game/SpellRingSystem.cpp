#include "game/SpellRingSystem.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace majo {

namespace {
constexpr std::size_t MaxSpellRingItems = 8;
constexpr float CapturedJumpInterval = 2.0f;
constexpr float CapturedJumpDuration = 0.24f;
constexpr float CapturedJumpDistance = 18.0f;
constexpr float CapturedPeriodicHealInterval = 8.0f;

SpellRingItem makeSpellRingItem(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return makeShovel();
    case SpellRingItemType::Torch: return makeTorch();
    case SpellRingItemType::Stone: return makeStone();
    case SpellRingItemType::Ore: return makeOre();
    case SpellRingItemType::Object: return makeObjectRingItem({});
    }
    return makeStone();
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
    items_.push_back(makeShovel());
    items_.push_back(makeTorch());
    radius_ = balance.spellRingRadius;
    angularSpeed_ = balance.spellRingSpeed;
    baseEquippedWeight_ = totalEquippedWeight();
    baseAngle_ = 0.0f;
    center_ = {};
    orbitModifiers_ = OrbitModifiers{};
    state_ = SpellRingState::Normal;
    capturedHealTimer_ = CapturedPeriodicHealInterval;
    activeRingIndex_ = 0;
}

void SpellRingSystem::update(Player& player, const Input& input, float dt, float, bool paused, bool blockPointerThrow, const RuntimeBalance& balance)
{
    if (paused) {
        return;
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

    const float spacing = Pi * 2.0f / static_cast<float>(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) {
        SpellRingItem& item = items_[i];
        const float angle = baseAngle_ + item.localAngle + spacing * static_cast<float>(i);
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

bool SpellRingSystem::addItem(SpellRingItemType type)
{
    if (!canAddItem()) {
        return false;
    }
    items_.push_back(makeSpellRingItem(type));
    return true;
}

bool SpellRingSystem::addObjectItem(const ItemData& item)
{
    if (!canAddItem() || item.id.empty()) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItem(item.id);
    applyObjectDefinition(ringItem, item);
    items_.push_back(std::move(ringItem));
    return true;
}

bool SpellRingSystem::addObjectItem(const ItemData& item, const ItemInstance& instance)
{
    if (!canAddItem() || item.id.empty() || instance.objectId != item.id) {
        return false;
    }

    SpellRingItem ringItem = makeObjectRingItem(item.id);
    applyItemInstance(ringItem, instance);
    applyObjectDefinition(ringItem, item);
    items_.push_back(std::move(ringItem));
    return true;
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
        orbitModifiers_.speedMultiplier);
}

float SpellRingSystem::totalEquippedWeight() const
{
    float total = 0.0f;
    for (const SpellRingItem& item : items_) {
        total += std::max(0.0f, item.weight);
    }
    return total;
}

float SpellRingSystem::weightSpeedMultiplier() const
{
    const float excessWeight = std::max(0.0f, totalEquippedWeight() - baseEquippedWeight_);
    return 1.0f / (1.0f + excessWeight * 0.035f);
}

}
