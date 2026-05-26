#include "devtools/autosim/AutoSimulationNavigator.hpp"

#include <algorithm>

namespace majo::autosim {

namespace {

constexpr float ThrowPulseIntervalSeconds = 0.54f;
constexpr float ConfirmPulseIntervalSeconds = 0.18f;
constexpr float ArriveDistance = 22.0f;

Vec2 worldToScreen(const GameTestSnapshot& snapshot, Vec2 world)
{
    return {
        world.x - snapshot.cameraPosition.x + static_cast<float>(snapshot.viewportWidth) * 0.5f,
        world.y - snapshot.cameraPosition.y + static_cast<float>(snapshot.viewportHeight) * 0.5f,
    };
}

Vec2 viewportCenter(const GameTestSnapshot& snapshot)
{
    return {
        static_cast<float>(snapshot.viewportWidth) * 0.5f,
        static_cast<float>(snapshot.viewportHeight) * 0.5f,
    };
}

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

} // namespace

void AutoSimulationNavigator::reset()
{
    throwCooldownSeconds_ = 0.0f;
    confirmCooldownSeconds_ = 0.0f;
}

InputAutomationFrame AutoSimulationNavigator::makeInput(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPlan& plan,
    float dt)
{
    throwCooldownSeconds_ = std::max(0.0f, throwCooldownSeconds_ - std::max(0.0f, dt));
    confirmCooldownSeconds_ = std::max(0.0f, confirmCooldownSeconds_ - std::max(0.0f, dt));

    InputAutomationFrame frame;
    if (plan.goal == AutoSimulationGoal::None) {
        return frame;
    }

    frame.active = true;
    frame.exclusive = true;
    frame.aimScreen = viewportCenter(snapshot);

    if (plan.confirm) {
        if (confirmCooldownSeconds_ <= 0.0f) {
            frame.confirmPressed = true;
            frame.useItemPressed = true;
            confirmCooldownSeconds_ = ConfirmPulseIntervalSeconds;
        }
        return frame;
    }

    if (!plan.hasTarget) {
        return frame;
    }

    const Vec2 moveTarget = plan.hasMoveTarget ? plan.moveTargetWorld : plan.targetWorld;
    const Vec2 aimTarget = plan.hasAimTarget ? plan.aimTargetWorld : plan.targetWorld;
    const Vec2 toTarget = moveTarget - snapshot.player.position;
    const Vec2 toAim = aimTarget - snapshot.player.position;
    const float targetDistanceSq = lengthSquared(toTarget);
    const float aimDistance = length(toAim);
    const float arriveDistance = std::max(1.0f, plan.moveTargetArriveDistance);
    Vec2 moveDirection = targetDistanceSq > arriveDistance * arriveDistance ? normalize(toTarget) : Vec2{};
    if (plan.rangeControl && aimDistance > 0.0001f) {
        const Vec2 aimDirection = toAim * (1.0f / aimDistance);
        if (aimDistance < plan.desiredRangeMin) {
            if (plan.goal == AutoSimulationGoal::Combat &&
                plan.alignMoveTargetInRange &&
                targetDistanceSq > arriveDistance * arriveDistance) {
                const Vec2 routeDirection = normalize(toTarget);
                moveDirection = dot(routeDirection, aimDirection) <= 0.18f
                    ? routeDirection
                    : aimDirection * -1.0f;
            } else {
                moveDirection = aimDirection * -1.0f;
            }
        } else if (aimDistance > plan.desiredRangeMax) {
            moveDirection = targetDistanceSq > arriveDistance * arriveDistance ? normalize(toTarget) : aimDirection;
        } else if (plan.strafe) {
            moveDirection = Vec2{-aimDirection.y, aimDirection.x};
        } else if (plan.alignMoveTargetInRange && targetDistanceSq > arriveDistance * arriveDistance) {
            moveDirection = normalize(toTarget);
        } else {
            moveDirection = Vec2{};
        }
    }
    if (plan.moveAwayFromTarget) {
        moveDirection = targetDistanceSq > 0.0001f ? normalize(snapshot.player.position - plan.targetWorld) : normalize(snapshot.player.facing);
    }
    frame.moveAxis = moveDirection;
    frame.aimScreen = worldToScreen(snapshot, aimTarget);
    frame.ringOffsetHeld = plan.ringOffset;
    if (frame.ringOffsetHeld && plan.ringOffsetRequiresMoveTarget) {
        const float offsetDistance = std::max(1.0f, plan.ringOffsetMoveTargetDistance);
        frame.ringOffsetHeld = targetDistanceSq <= offsetDistance * offsetDistance;
    }

    if (plan.throwRing &&
        snapshot.ringState == GameTestRingState::Normal &&
        throwCooldownSeconds_ <= 0.0f) {
        frame.throwPressed = true;
        throwCooldownSeconds_ = ThrowPulseIntervalSeconds;
    }

    return frame;
}

} // namespace majo::autosim
