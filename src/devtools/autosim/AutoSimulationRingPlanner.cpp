#include "devtools/autosim/AutoSimulationRingPlanner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace majo::autosim {

namespace {

constexpr float SwitchCooldownSeconds = 1.45f;
constexpr float RegularSwitchGain = 34.0f;
constexpr float MissingRoleSwitchGain = 9.0f;
constexpr float LightSwitchGain = 24.0f;
constexpr float UtilitySwitchGain = 42.0f;
constexpr float MeaningfulLightRadius = 140.0f;

struct RingUseScore {
    int ringIndex = -1;
    float combat = 0.0f;
    float dig = 0.0f;
    float light = 0.0f;
    float utility = 0.0f;
    float roleScore = 0.0f;
    float lightRadius = 0.0f;
    bool hasCombatTool = false;
    bool hasDigTool = false;
    bool hasLightTool = false;
};

bool busy(const GameTestSnapshot& snapshot)
{
    return snapshot.worldLoading ||
        snapshot.transitionActive ||
        snapshot.dialogueActive ||
        snapshot.pendingStoryDelayActive ||
        snapshot.firstItemNoticeActive ||
        snapshot.bossPresentationActive;
}

const char* roleName(AutoSimulationRingRole role)
{
    switch (role) {
    case AutoSimulationRingRole::Combat: return "combat";
    case AutoSimulationRingRole::Dig: return "dig";
    case AutoSimulationRingRole::Light: return "light";
    case AutoSimulationRingRole::Utility: return "utility";
    case AutoSimulationRingRole::None: break;
    }
    return "none";
}

AutoSimulationRingRole effectiveRole(const AutoSimulationPlan& plan)
{
    if (plan.preferredRingRole != AutoSimulationRingRole::None) {
        return plan.preferredRingRole;
    }

    switch (plan.goal) {
    case AutoSimulationGoal::Combat:
        return AutoSimulationRingRole::Combat;
    case AutoSimulationGoal::MineWall:
        return AutoSimulationRingRole::Dig;
    case AutoSimulationGoal::CollectDrop:
    case AutoSimulationGoal::OpenChest:
    case AutoSimulationGoal::DiscoverWarp:
    case AutoSimulationGoal::ResumeFrontier:
    case AutoSimulationGoal::ApproachBoss:
    case AutoSimulationGoal::FollowMainPath:
        return AutoSimulationRingRole::Light;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::UseItem:
    case AutoSimulationGoal::ReturnToBase:
    case AutoSimulationGoal::EscapeStuck:
        return AutoSimulationRingRole::None;
    }
    return AutoSimulationRingRole::None;
}

float lightValue(float lightRadius)
{
    return lightRadius > 0.0f ? 34.0f + lightRadius * 0.42f : 0.0f;
}

RingUseScore scoreRing(
    const GameTestSnapshot& snapshot,
    const GameTestRingLoadoutSnapshot& ring,
    const AutoSimulationItemEvaluator& itemEvaluator)
{
    RingUseScore score;
    score.ringIndex = ring.ringIndex;
    score.hasCombatTool = ring.hasCombatTool;
    score.hasDigTool = ring.hasDigTool;
    score.hasLightTool = ring.hasLightTool;
    score.lightRadius = std::max(0.0f, ring.bestLightRadius);
    score.combat =
        static_cast<float>(std::max(0, ring.bestDamage)) * 8.5f +
        std::max(0.0f, ring.bestHitRadius) * 0.25f +
        (ring.hasCombatTool ? 18.0f : 0.0f);
    score.dig =
        static_cast<float>(std::max(0, ring.bestDigPower)) * 9.5f +
        std::max(0.0f, ring.bestHitRadius) * 0.35f +
        (ring.hasDigTool ? 22.0f : 0.0f);
    score.light = lightValue(ring.bestLightRadius);
    score.utility =
        static_cast<float>(std::max(0, ring.itemCount)) * 3.0f +
        static_cast<float>(std::max(0, ring.maxItemCount)) * 1.2f +
        std::max(0.0f, ring.radius) * 0.08f +
        std::max(0.0f, ring.angularSpeed) * 3.0f +
        std::max(0.0f, ring.maxWeight) * 0.55f;

    for (const GameTestRingItemSnapshot& item : snapshot.ring.items) {
        if (item.ringIndex != ring.ringIndex || item.objectId.empty() || item.broken) {
            continue;
        }

        const AutoSimulationItemScore itemScore = itemEvaluator.evaluate(item);
        score.combat += itemScore.combat;
        score.dig += itemScore.dig;
        score.light = std::max(score.light, itemScore.light);
        score.utility += itemScore.utility + itemScore.loadout * 0.06f;
        score.lightRadius = std::max(score.lightRadius, item.lightRadius);
        score.hasCombatTool = score.hasCombatTool || itemScore.combat > 0.0f;
        score.hasDigTool = score.hasDigTool || itemScore.dig > 0.0f;
        score.hasLightTool = score.hasLightTool || item.lightRadius > 0.0f;
    }

    return score;
}

float roleScore(const RingUseScore& score, AutoSimulationRingRole role)
{
    switch (role) {
    case AutoSimulationRingRole::Combat:
        return score.combat * 1.20f + score.dig * 0.16f + score.light * 0.08f + score.utility * 0.12f;
    case AutoSimulationRingRole::Dig:
        return score.dig * 1.20f + score.combat * 0.14f + score.light * 0.10f + score.utility * 0.12f;
    case AutoSimulationRingRole::Light:
        return score.light * 1.15f + std::max(score.combat, score.dig) * 0.18f + score.utility * 0.18f;
    case AutoSimulationRingRole::Utility:
        return std::max(score.combat, score.dig) * 0.45f + score.light * 0.65f + score.utility * 0.28f;
    case AutoSimulationRingRole::None:
        break;
    }
    return 0.0f;
}

bool hasRoleTool(const RingUseScore& score, AutoSimulationRingRole role)
{
    switch (role) {
    case AutoSimulationRingRole::Combat:
        return score.hasCombatTool;
    case AutoSimulationRingRole::Dig:
        return score.hasDigTool;
    case AutoSimulationRingRole::Light:
        return score.hasLightTool;
    case AutoSimulationRingRole::Utility:
        return score.hasCombatTool || score.hasDigTool || score.hasLightTool;
    case AutoSimulationRingRole::None:
        break;
    }
    return false;
}

float minimumReadyScore(AutoSimulationRingRole role)
{
    switch (role) {
    case AutoSimulationRingRole::Combat:
    case AutoSimulationRingRole::Dig:
        return 58.0f;
    case AutoSimulationRingRole::Light:
        return 72.0f;
    case AutoSimulationRingRole::Utility:
        return 84.0f;
    case AutoSimulationRingRole::None:
        break;
    }
    return std::numeric_limits<float>::max();
}

float switchGainThreshold(
    AutoSimulationRingRole role,
    const RingUseScore& current,
    const RingUseScore& target)
{
    if (!hasRoleTool(current, role) && hasRoleTool(target, role)) {
        return MissingRoleSwitchGain;
    }
    if (role == AutoSimulationRingRole::Light) {
        if (current.lightRadius < MeaningfulLightRadius && target.lightRadius >= MeaningfulLightRadius) {
            return MissingRoleSwitchGain;
        }
        return LightSwitchGain;
    }
    if (role == AutoSimulationRingRole::Utility) {
        return UtilitySwitchGain;
    }
    return RegularSwitchGain;
}

std::string scoreReason(float gain)
{
    return " gain=" + std::to_string(static_cast<int>(std::round(gain)));
}

} // namespace

void AutoSimulationRingPlanner::reset()
{
    switchCooldownSeconds_ = 0.0f;
}

std::optional<GameTestAction> AutoSimulationRingPlanner::chooseAction(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPlan& plan,
    float dt)
{
    switchCooldownSeconds_ = std::max(0.0f, switchCooldownSeconds_ - std::max(0.0f, dt));
    if (switchCooldownSeconds_ > 0.0f ||
        snapshot.screenMode != GameTestScreenMode::Playing ||
        snapshot.ringState != GameTestRingState::Normal ||
        busy(snapshot)) {
        return std::nullopt;
    }

    const AutoSimulationRingRole role = effectiveRole(plan);
    if (role == AutoSimulationRingRole::None || snapshot.ring.rings.size() <= 1) {
        return std::nullopt;
    }

    std::optional<RingUseScore> current;
    std::optional<RingUseScore> best;
    for (const GameTestRingLoadoutSnapshot& ring : snapshot.ring.rings) {
        RingUseScore score = scoreRing(snapshot, ring, itemEvaluator_);
        score.roleScore = roleScore(score, role);
        if (ring.ringIndex == snapshot.ring.activeRingIndex) {
            current = score;
        }
        if (!best || score.roleScore > best->roleScore) {
            best = score;
        }
    }

    if (!current || !best ||
        best->ringIndex == snapshot.ring.activeRingIndex ||
        best->roleScore < minimumReadyScore(role) ||
        !hasRoleTool(*best, role)) {
        return std::nullopt;
    }

    const float gain = best->roleScore - current->roleScore;
    if (gain < switchGainThreshold(role, *current, *best)) {
        return std::nullopt;
    }

    GameTestAction action;
    action.kind = GameTestActionKind::SwitchActiveRing;
    action.ringIndex = best->ringIndex;
    action.reason =
        std::string("ring_select ") +
        roleName(role) +
        " ring" +
        std::to_string(best->ringIndex + 1) +
        scoreReason(gain);
    switchCooldownSeconds_ = SwitchCooldownSeconds;
    return action;
}

} // namespace majo::autosim
