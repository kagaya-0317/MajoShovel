#include "game/SpellRingSystem.hpp"

#include <utility>

namespace majo {

namespace {
constexpr std::size_t MaxSpellRingItems = 8;

SpellRingItem makeSpellRingItem(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return makeShovel();
    case SpellRingItemType::Torch: return makeTorch();
    case SpellRingItemType::Stone: return makeStone();
    case SpellRingItemType::Ore: return makeOre();
    }
    return makeStone();
}
}

void SpellRingSystem::initialize(const RuntimeBalance& balance)
{
    items_.clear();
    items_.push_back(makeShovel());
    items_.push_back(makeTorch());
    radius_ = balance.spellRingRadius;
    angularSpeed_ = balance.spellRingSpeed;
    baseAngle_ = 0.0f;
    center_ = {};
    orbitModifiers_ = OrbitModifiers{};
    state_ = SpellRingState::Normal;
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

    const float spacing = Pi * 2.0f / static_cast<float>(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const float angle = baseAngle_ + items_[i].localAngle + spacing * static_cast<float>(i);
        items_[i].worldPosition = center_ + fromAngle(angle) * radius_;
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
    return static_cast<float>(static_cast<double>(angularSpeed_) * orbitModifiers_.speedMultiplier);
}

}
