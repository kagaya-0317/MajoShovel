#pragma once

#include "engine/Input.hpp"
#include "engine/Math.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/OrbitModifiers.hpp"
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
    void update(Player& player, const Input& input, float dt, float totalTime, bool paused, bool blockPointerThrow, const RuntimeBalance& balance);
    void upgradeRadius(float factor) { radius_ *= factor; }
    void upgradeSpeed(float factor) { angularSpeed_ *= factor; }
    void clearOrbitModifiers();
    void setOrbitModifiers(OrbitModifiers modifiers);
    void applyOrbitModifierEffect(std::string_view effect, double value, std::string_view source);
    void upgradeShovelPower(int amount);
    void upgradeItemDamage(int amount);
    bool addItem(SpellRingItemType type);
    bool canAddItem() const;
    void switchActiveRing(int delta);

    const std::vector<SpellRingItem>& items() const { return items_; }
    std::vector<SpellRingItem>& items() { return items_; }
    Vec2 center() const { return center_; }
    float radius() const { return radius_; }
    float angularSpeed() const { return angularSpeed_; }
    float effectiveAngularSpeed() const;
    double effectivePowerMultiplier() const { return orbitModifiers_.powerMultiplier; }
    double effectiveGravityMultiplier() const { return orbitModifiers_.gravityMultiplier; }
    double effectiveAntigravityMultiplier() const { return orbitModifiers_.antigravityMultiplier; }
    double orbitAnchorStrength() const { return orbitModifiers_.anchorStrength; }
    double orbitShiftMultiplier() const { return orbitModifiers_.shiftMultiplier; }
    const OrbitModifiers& orbitModifiers() const { return orbitModifiers_; }
    SpellRingState state() const { return state_; }
    int activeRingIndex() const { return activeRingIndex_; }
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
    int activeRingIndex_ = 0;
    OrbitModifiers orbitModifiers_{};
    SpellRingState state_ = SpellRingState::Normal;
};

}
