#include "devtools/autosim/AutoSimulationLevelUpPlanner.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace majo::autosim {

namespace {

constexpr int RadiusOption = 0;
constexpr int SpeedOption = 1;
constexpr int WeightOption = 2;
constexpr int LevelUpOptionCount = 3;

struct LevelUpCandidate {
    int ringIndex = 0;
    int option = RadiusOption;
    float score = -std::numeric_limits<float>::max();
    std::string reason;
};

const char* optionReason(int option)
{
    switch (option) {
    case SpeedOption:
        return "speed";
    case WeightOption:
        return "weight";
    case RadiusOption:
    default:
        return "radius";
    }
}

int upgradePointsForOption(const GameTestRingLoadoutSnapshot& ring, int option)
{
    switch (option) {
    case SpeedOption:
        return ring.speedUpgradePoints;
    case WeightOption:
        return ring.weightLimitUpgradePoints;
    case RadiusOption:
    default:
        return ring.radiusUpgradePoints;
    }
}

int totalUpgradePoints(const GameTestRingLoadoutSnapshot& ring)
{
    return std::max(0, ring.radiusUpgradePoints) +
        std::max(0, ring.speedUpgradePoints) +
        std::max(0, ring.weightLimitUpgradePoints);
}

float weightUsage(const GameTestRingLoadoutSnapshot& ring)
{
    if (ring.maxWeight <= 0.0f) {
        return 0.0f;
    }
    return std::max(0.0f, ring.weight / ring.maxWeight);
}

float usefulBackpackWeightPressure(const GameTestSnapshot& snapshot, const GameTestRingLoadoutSnapshot& ring)
{
    const float remaining = std::max(0.0f, ring.maxWeight - ring.weight);
    float pressure = 0.0f;
    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        if (item.location != GameTestInventoryLocation::Backpack ||
            item.equipped ||
            item.broken ||
            item.category == "杖") {
            continue;
        }

        const int attack = std::max(0, item.attackPower + item.attackBonus);
        const int dig = std::max(0, item.digPower + item.digBonus);
        if (attack <= 0 && dig <= 0) {
            continue;
        }

        const float usefulScore =
            static_cast<float>(dig) * 4.0f +
            static_cast<float>(attack) * 3.0f +
            static_cast<float>(std::max(0, item.rarity)) * 2.0f;
        const float weightGap = static_cast<float>(std::max(0.0, item.weightKg - static_cast<double>(remaining)));
        if (weightGap > 0.0f) {
            pressure += std::min(32.0f, usefulScore) * std::clamp(weightGap / 4.0f, 0.25f, 1.0f);
        }
    }
    return pressure;
}

float ringUseScore(const GameTestSnapshot& snapshot, const GameTestRingLoadoutSnapshot& ring)
{
    float score = 32.0f;
    if (ring.ringIndex == snapshot.ring.activeRingIndex) {
        score += 28.0f;
    }
    score += static_cast<float>(std::min(std::max(0, ring.itemCount), 10)) * 2.5f;
    if (ring.hasDigTool) {
        score += 12.0f;
    }
    if (ring.hasCombatTool) {
        score += 10.0f;
    }
    score += std::min(18.0f, static_cast<float>(std::max(0, ring.bestDigPower)) * 1.8f);
    score += std::min(16.0f, static_cast<float>(std::max(0, ring.bestDamage)) * 1.4f);
    score += std::min(8.0f, std::max(0.0f, ring.bestHitRadius) * 0.12f);

    if (ring.itemCount <= 0) {
        score *= ring.ringIndex == snapshot.ring.activeRingIndex ? 0.75f : 0.45f;
    }
    return score;
}

std::array<float, LevelUpOptionCount> desiredRatios(
    const GameTestSnapshot& snapshot,
    const GameTestRingLoadoutSnapshot& ring,
    float backpackWeightPressure)
{
    std::array<float, LevelUpOptionCount> ratios{0.39f, 0.31f, 0.30f};
    if (ring.hasDigTool) {
        ratios[RadiusOption] += 0.04f;
        ratios[SpeedOption] += 0.02f;
    }
    if (ring.hasCombatTool || !snapshot.enemies.empty()) {
        ratios[SpeedOption] += 0.04f;
    }

    const float usage = weightUsage(ring);
    ratios[WeightOption] += std::clamp((usage - 0.65f) * 0.55f, 0.0f, 0.18f);
    ratios[WeightOption] += std::clamp(backpackWeightPressure / 180.0f, 0.0f, 0.12f);

    const float total = std::max(0.001f, ratios[0] + ratios[1] + ratios[2]);
    for (float& ratio : ratios) {
        ratio /= total;
    }
    return ratios;
}

float optionNeedScore(
    const GameTestRingLoadoutSnapshot& ring,
    int option,
    float backpackWeightPressure)
{
    const float usage = weightUsage(ring);
    const float overweight = std::max(0.0f, usage - 1.0f);
    const float remainingWeight = std::max(0.0f, ring.maxWeight - ring.weight);

    switch (option) {
    case SpeedOption:
        return 31.0f +
            (ring.hasCombatTool ? 12.0f : 0.0f) +
            (ring.hasDigTool ? 8.0f : 0.0f) +
            static_cast<float>(std::min(std::max(0, ring.itemCount), 8)) * 1.5f +
            std::clamp((3.2f - ring.angularSpeed) * 9.0f, -10.0f, 20.0f) -
            overweight * 20.0f;
    case WeightOption:
        return 28.0f +
            std::clamp((usage - 0.68f) * 95.0f, 0.0f, 52.0f) +
            overweight * 95.0f +
            (remainingWeight < 1.5f && ring.itemCount > 0 ? 16.0f : 0.0f) +
            std::min(36.0f, backpackWeightPressure * 0.45f);
    case RadiusOption:
    default:
        return 36.0f +
            (ring.hasDigTool ? 14.0f : 0.0f) +
            (ring.hasCombatTool ? 8.0f : 0.0f) +
            std::clamp((72.0f - ring.radius) * 0.45f, -8.0f, 18.0f) +
            (ring.bestHitRadius > 0.0f && ring.bestHitRadius < 18.0f ? 6.0f : 0.0f);
    }
}

float balanceScore(
    const GameTestSnapshot& snapshot,
    const GameTestRingLoadoutSnapshot& ring,
    int option,
    float backpackWeightPressure,
    float averageRingPoints)
{
    const int ringPoints = totalUpgradePoints(ring);
    const int optionPoints = upgradePointsForOption(ring, option);
    const std::array<float, LevelUpOptionCount> ratios = desiredRatios(snapshot, ring, backpackWeightPressure);
    const float expectedAfterChoice = static_cast<float>(ringPoints + 1) * ratios[static_cast<std::size_t>(option)];

    float score = (expectedAfterChoice - static_cast<float>(optionPoints)) * 18.0f;
    score -= std::max(0.0f, static_cast<float>(optionPoints) - expectedAfterChoice) * 14.0f;
    score -= static_cast<float>(optionPoints) * 2.0f;
    score += (averageRingPoints - static_cast<float>(ringPoints)) * 4.0f;
    return score;
}

LevelUpCandidate scoreCandidate(
    const GameTestSnapshot& snapshot,
    const GameTestRingLoadoutSnapshot& ring,
    int option,
    float averageRingPoints)
{
    const float backpackPressure = usefulBackpackWeightPressure(snapshot, ring);
    LevelUpCandidate candidate;
    candidate.ringIndex = ring.ringIndex;
    candidate.option = option;
    candidate.score =
        ringUseScore(snapshot, ring) +
        optionNeedScore(ring, option, backpackPressure) +
        balanceScore(snapshot, ring, option, backpackPressure, averageRingPoints);
    candidate.score += static_cast<float>(LevelUpOptionCount - option) * 0.01f;
    candidate.score -= static_cast<float>(std::max(0, ring.ringIndex)) * 0.05f;
    candidate.reason = "level_up ring" + std::to_string(ring.ringIndex + 1) + " " + optionReason(option);
    return candidate;
}

GameTestRingLoadoutSnapshot fallbackActiveRing(const GameTestSnapshot& snapshot)
{
    GameTestRingLoadoutSnapshot ring;
    ring.ringIndex = std::max(0, snapshot.ring.activeRingIndex);
    ring.radius = snapshot.ring.activeRadius;
    ring.angularSpeed = snapshot.ring.activeAngularSpeed;
    ring.weight = snapshot.ring.activeWeight;
    ring.maxWeight = snapshot.ring.activeMaxWeight;
    ring.itemCount = snapshot.ring.activeItemCount;
    ring.maxItemCount = snapshot.ring.activeMaxItemCount;
    ring.bestDamage = snapshot.ring.bestDamage;
    ring.bestDigPower = snapshot.ring.bestDigPower;
    ring.bestHitRadius = snapshot.ring.bestHitRadius;
    ring.hasCombatTool = snapshot.ring.hasCombatTool;
    ring.hasDigTool = snapshot.ring.hasDigTool;
    return ring;
}

} // namespace

std::optional<GameTestAction> AutoSimulationLevelUpPlanner::chooseAction(const GameTestSnapshot& snapshot) const
{
    if (snapshot.screenMode != GameTestScreenMode::LevelUp || !snapshot.levelUp.choiceActive) {
        return std::nullopt;
    }

    std::vector<GameTestRingLoadoutSnapshot> rings = snapshot.ring.rings;
    if (rings.empty()) {
        rings.push_back(fallbackActiveRing(snapshot));
    }

    float totalPoints = 0.0f;
    for (const GameTestRingLoadoutSnapshot& ring : rings) {
        totalPoints += static_cast<float>(totalUpgradePoints(ring));
    }
    const float averageRingPoints = totalPoints / static_cast<float>(std::max<std::size_t>(1, rings.size()));

    LevelUpCandidate best;
    for (const GameTestRingLoadoutSnapshot& ring : rings) {
        for (int option = 0; option < LevelUpOptionCount; ++option) {
            const LevelUpCandidate candidate = scoreCandidate(snapshot, ring, option, averageRingPoints);
            if (candidate.score > best.score) {
                best = candidate;
            }
        }
    }

    GameTestAction action;
    action.kind = GameTestActionKind::ChooseLevelUpUpgrade;
    action.ringIndex = best.ringIndex;
    action.upgradeIndex = best.option;
    action.reason = best.reason;
    return action;
}

} // namespace majo::autosim
