#include "game/OrbitSystem.hpp"

namespace majo {

void OrbitSystem::initialize(const RuntimeBalance& balance)
{
    items_.clear();
    items_.push_back(makeShovel());
    items_.push_back(makeTorch());
    radius_ = balance.orbitRadius;
    angularSpeed_ = balance.orbitSpeed;
    baseAngle_ = 0.0f;
    center_ = {};
    state_ = OrbitState::Normal;
}

void OrbitSystem::update(Player& player, const Input& input, float dt, float, bool paused, const RuntimeBalance& balance)
{
    if (paused) {
        return;
    }

    baseAngle_ += angularSpeed_ * dt;
    const Vec2 normalCenter = player.position + player.facing * player.orbitShift;

    if (state_ == OrbitState::Normal) {
        center_ = normalCenter;
        if (input.throwPressed() && player.throwCooldownRemaining <= 0.0f) {
            state_ = OrbitState::Thrown;
            throwDirection_ = player.facing;
            throwStart_ = center_;
            throwTime_ = 0.0f;
            player.throwCooldownRemaining = balance.orbitThrowCooldown;
        }
    } else if (state_ == OrbitState::Thrown) {
        throwTime_ += dt;
        center_ += throwDirection_ * balance.orbitThrowSpeed * dt;
        if (distanceSquared(center_, throwStart_) >= balance.orbitThrowDistance * balance.orbitThrowDistance ||
            throwTime_ >= balance.orbitThrowMaxTime) {
            state_ = OrbitState::Returning;
        }
    } else {
        const Vec2 toPlayer = normalCenter - center_;
        const float dist = length(toPlayer);
        if (dist < 14.0f) {
            center_ = normalCenter;
            state_ = OrbitState::Normal;
        } else {
            center_ += normalize(toPlayer) * balance.orbitReturnSpeed * dt;
        }
    }

    const float spacing = Pi * 2.0f / static_cast<float>(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const float angle = baseAngle_ + items_[i].localAngle + spacing * static_cast<float>(i);
        items_[i].worldPosition = center_ + fromAngle(angle) * radius_;
    }
}

void OrbitSystem::upgradeShovelPower(int amount)
{
    for (auto& item : items_) {
        if (item.type == OrbitItemType::Shovel) {
            item.damage += amount;
            item.digPower += amount;
        }
    }
}

float OrbitSystem::cooldownRatio(const Player& player, const RuntimeBalance& balance) const
{
    return clamp(player.throwCooldownRemaining / balance.orbitThrowCooldown, 0.0f, 1.0f);
}

}
