#include "devtools/autosim/AutoSimulationCombatModel.hpp"

#include <algorithm>
#include <limits>

namespace majo::autosim {

namespace {

constexpr float CombatAcquireRadius = 520.0f;
constexpr float EnemyBodyRadiusFallback = 14.0f;
constexpr float EmergencyDistance = 42.0f;

const GameTestEnemySnapshot* nearestEnemy(const GameTestSnapshot& snapshot)
{
    const GameTestEnemySnapshot* best = nullptr;
    float bestDistanceSq = CombatAcquireRadius * CombatAcquireRadius;
    for (const GameTestEnemySnapshot& enemy : snapshot.enemies) {
        const float distSq = distanceSquared(snapshot.player.position, enemy.position);
        if (distSq < bestDistanceSq) {
            bestDistanceSq = distSq;
            best = &enemy;
        }
    }
    return best;
}

} // namespace

std::optional<AutoSimulationPlan> AutoSimulationCombatModel::makePlan(const GameTestSnapshot& snapshot) const
{
    const GameTestEnemySnapshot* enemy = nearestEnemy(snapshot);
    if (enemy == nullptr) {
        return std::nullopt;
    }

    const float ringRadius = std::max(36.0f, snapshot.ring.activeRadius);
    const float hitRadius = std::max(8.0f, snapshot.ring.bestHitRadius);
    const float enemyRadius = enemy->boss ? 28.0f : EnemyBodyRadiusFallback;
    const float desiredMin = std::max(EmergencyDistance, ringRadius - hitRadius - enemyRadius * 0.25f);
    const float desiredMax = std::max(desiredMin + 18.0f, ringRadius + hitRadius + enemyRadius + 18.0f);
    const float throwRange = desiredMax + ringRadius * 1.7f;
    const float distance = length(enemy->position - snapshot.player.position);

    AutoSimulationPlan plan;
    plan.goal = AutoSimulationGoal::Combat;
    plan.targetWorld = enemy->position;
    plan.moveTargetWorld = enemy->position;
    plan.aimTargetWorld = enemy->position;
    plan.hasTarget = true;
    plan.hasMoveTarget = true;
    plan.hasAimTarget = true;
    plan.rangeControl = true;
    plan.desiredRangeMin = desiredMin;
    plan.desiredRangeMax = desiredMax;
    plan.strafe = distance >= desiredMin && distance <= desiredMax;
    plan.throwRing = snapshot.ring.hasCombatTool && distance <= throwRange;
    plan.ringOffset = false;
    plan.moveAwayFromTarget = false;
    plan.reason = enemy->boss ? "boss_effective_range" : "enemy_effective_range";
    return plan;
}

} // namespace majo::autosim
