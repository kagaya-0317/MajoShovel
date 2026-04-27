#pragma once

#include "engine/Input.hpp"
#include "engine/Math.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/SpellRingItem.hpp"
#include "game/Player.hpp"
#include <vector>

namespace majo {

enum class SpellRingState {
    Normal,
    Thrown,
    Returning
};

class SpellRingSystem {
public:
    void initialize(const RuntimeBalance& balance);
    void update(Player& player, const Input& input, float dt, float totalTime, bool paused, const RuntimeBalance& balance);
    void upgradeRadius(float factor) { radius_ *= factor; }
    void upgradeSpeed(float factor) { angularSpeed_ *= factor; }
    void upgradeShovelPower(int amount);
    void upgradeItemDamage(int amount);
    bool addItem(SpellRingItemType type);
    bool canAddItem() const;

    const std::vector<SpellRingItem>& items() const { return items_; }
    std::vector<SpellRingItem>& items() { return items_; }
    Vec2 center() const { return center_; }
    float radius() const { return radius_; }
    float angularSpeed() const { return angularSpeed_; }
    SpellRingState state() const { return state_; }
    float cooldownRatio(const Player& player, const RuntimeBalance& balance) const;

private:
    std::vector<SpellRingItem> items_;
    Vec2 center_{};
    Vec2 throwDirection_{1.0f, 0.0f};
    Vec2 throwStart_{};
    float radius_ = 54.0f;
    float angularSpeed_ = 3.4f;
    float baseAngle_ = 0.0f;
    float throwTime_ = 0.0f;
    SpellRingState state_ = SpellRingState::Normal;
};

}
