#include "game/SpellRingSystem.hpp"

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
    state_ = SpellRingState::Normal;
}

void SpellRingSystem::update(Player& player, const Input& input, float dt, float, bool paused, const RuntimeBalance& balance)
{
    if (paused) {
        return;
    }

    baseAngle_ += angularSpeed_ * dt;
    const Vec2 normalCenter = player.position + player.facing * player.spellRingShift;

    if (state_ == SpellRingState::Normal) {
        center_ = normalCenter;
        if (input.throwPressed() && player.throwCooldownRemaining <= 0.0f) {
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

float SpellRingSystem::cooldownRatio(const Player& player, const RuntimeBalance& balance) const
{
    return clamp(player.throwCooldownRemaining / balance.spellRingThrowCooldown, 0.0f, 1.0f);
}

}
