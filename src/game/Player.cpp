#include "game/Player.hpp"

#include "game/TileMap.hpp"

namespace majo {

void Player::update(const Input& input, const Camera& camera, TileMap& map, float dt, bool paused, const RuntimeBalance& balance)
{
    if (paused) {
        return;
    }

    status.update(dt);
    const float speed = static_cast<float>(status.applyModifiers(ModifierStat::Speed, balance.playerSpeed));
    velocity = input.moveAxis() * speed;
    const Vec2 delta = velocity * dt;
    Vec2 next = position + Vec2{delta.x, 0.0f};
    if (!map.isCircleBlocked(next, balance.playerRadius)) {
        position = next;
    }
    next = position + Vec2{0.0f, delta.y};
    if (!map.isCircleBlocked(next, balance.playerRadius)) {
        position = next;
    }

    const Vec2 aim = camera.screenToWorld(input.mouseScreen()) - position;
    if (lengthSquared(aim) > 16.0f) {
        facing = normalize(aim);
    } else if (lengthSquared(velocity) > 1.0f) {
        facing = normalize(velocity);
    }

    const float targetShift = input.ringOffsetHeld() ? balance.spellRingShiftDistance : 0.0f;
    spellRingShift = lerp(spellRingShift, targetShift, 1.0f - std::exp(-14.0f * dt));
    throwCooldownRemaining = std::max(0.0f, throwCooldownRemaining - dt);
}

}
