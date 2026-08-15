#include "devtools/autosim/AutoSimulationConsumablePlanner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace majo::autosim {

namespace {

constexpr float NormalHealHpRatio = 0.45f;
constexpr float DangerHealHpRatio = 0.60f;
constexpr float EmergencyHealHpRatio = 0.25f;
constexpr float NormalDesiredHpRatio = 0.65f;
constexpr float DangerDesiredHpRatio = 0.72f;
constexpr float EnemyDangerRadius = 260.0f;
constexpr float BossDangerRadius = 520.0f;
constexpr float BuffEnemyRadius = 340.0f;
constexpr float BuffBossRadius = 620.0f;
constexpr float ActiveBuffRefreshSeconds = 3.0f;

enum class ConsumableUseKind {
    Stack,
    Instance,
};

struct CombatPressure {
    bool dangerNearby = false;
    bool buffWorthyNearby = false;
    bool bossNearby = false;
    int nearbyEnemies = 0;
};

struct ConsumableChoice {
    const GameTestObjectEntrySnapshot* item = nullptr;
    ConsumableUseKind kind = ConsumableUseKind::Stack;
    double heal = 0.0;
    float score = std::numeric_limits<float>::max();
    std::string reasonPrefix;
};

bool selfTarget(std::string_view target)
{
    return target == "player" || target == "owner" || target == "self";
}

bool hasTag(const GameTestObjectEntrySnapshot& item, std::string_view tag)
{
    return std::any_of(item.tags.begin(), item.tags.end(), [tag](const std::string& itemTag) {
        return std::string_view(itemTag.data(), itemTag.size()) == tag;
    });
}

bool consumableLike(const GameTestObjectEntrySnapshot& item)
{
    return hasTag(item, "consumable") ||
        hasTag(item, "food") ||
        hasTag(item, "apple") ||
        hasTag(item, "potion") ||
        hasTag(item, "candy") ||
        hasTag(item, "cookie");
}

CombatPressure combatPressure(const GameTestSnapshot& snapshot)
{
    CombatPressure pressure;
    for (const GameTestEnemySnapshot& enemy : snapshot.enemies) {
        const float dangerRadius = enemy.boss ? BossDangerRadius : EnemyDangerRadius;
        const float buffRadius = enemy.boss ? BuffBossRadius : BuffEnemyRadius;
        const float distSq = distanceSquared(snapshot.player.position, enemy.position);
        if (distSq <= dangerRadius * dangerRadius) {
            pressure.dangerNearby = true;
        }
        if (distSq <= buffRadius * buffRadius) {
            pressure.buffWorthyNearby = true;
            ++pressure.nearbyEnemies;
            if (enemy.boss) {
                pressure.bossNearby = true;
            }
        }
    }
    return pressure;
}

ConsumableUseKind useKindFor(const GameTestObjectEntrySnapshot& item)
{
    return item.kind == GameTestObjectEntryKind::Instance ? ConsumableUseKind::Instance : ConsumableUseKind::Stack;
}

AutoSimulationConsumableProfile buildConsumableProfile(const GameTestObjectEntrySnapshot& item)
{
    AutoSimulationConsumableProfile profile;
    for (const GameTestUseEffectSnapshot& effect : item.useEffects) {
        if (!selfTarget(effect.target)) {
            continue;
        }

        if (effect.effect == "heal") {
            if (effect.duration == 0.0 && effect.value > 0.0) {
                profile.heal += effect.value;
            } else {
                profile.unsafeSelfEffect = true;
            }
            continue;
        }

        if (effect.effect == "buff_attack") {
            if (effect.value > 1.0) {
                profile.attackMultiplier = std::max(profile.attackMultiplier, effect.value);
            } else {
                profile.unsafeSelfEffect = true;
            }
            continue;
        }

        if (effect.effect == "buff_speed") {
            if (effect.value > 1.0) {
                profile.speedMultiplier = std::max(profile.speedMultiplier, effect.value);
            } else {
                profile.unsafeSelfEffect = true;
            }
            continue;
        }

        if (effect.effect == "buff_defense") {
            if (effect.value > 1.0) {
                profile.defenseMultiplier = std::max(profile.defenseMultiplier, effect.value);
            } else {
                profile.unsafeSelfEffect = true;
            }
            continue;
        }

        if (effect.effect == "status_giant") {
            if (effect.value > 0.0) {
                profile.giantValue = std::max(profile.giantValue, effect.value);
            } else {
                profile.unsafeSelfEffect = true;
            }
            continue;
        }

        profile.unsafeSelfEffect = true;
    }
    return profile;
}

bool usableConsumableItem(const GameTestObjectEntrySnapshot& item, const AutoSimulationConsumableProfile& profile)
{
    if (item.location != GameTestInventoryLocation::Backpack ||
        item.objectId.empty() ||
        item.broken ||
        item.important ||
        profile.unsafeSelfEffect ||
        !consumableLike(item)) {
        return false;
    }

    if (item.kind == GameTestObjectEntryKind::Stack) {
        return item.count > 0;
    }
    if (item.kind == GameTestObjectEntryKind::Instance) {
        return !item.instanceId.empty() &&
            !item.equipped &&
            !item.protectionEnabled;
    }
    return false;
}

bool hasActiveModifier(const GameTestSnapshot& snapshot, std::string_view modifierId)
{
    for (const GameTestPlayerModifierSnapshot& modifier : snapshot.player.modifiers) {
        if (std::string_view(modifier.id.data(), modifier.id.size()) == modifierId &&
            modifier.duration > ActiveBuffRefreshSeconds) {
            return true;
        }
    }
    return false;
}

bool hasActiveState(const GameTestSnapshot& snapshot, std::string_view stateId)
{
    for (const GameTestPlayerStateSnapshot& state : snapshot.player.states) {
        if (std::string_view(state.id.data(), state.id.size()) == stateId &&
            state.duration > ActiveBuffRefreshSeconds) {
            return true;
        }
    }
    return false;
}

float itemCostPenalty(const GameTestObjectEntrySnapshot& item, ConsumableUseKind kind)
{
    float penalty =
        static_cast<float>(std::max(0, item.rarity)) * 4.0f +
        static_cast<float>(std::max(0, item.price)) * 0.025f;
    if (kind == ConsumableUseKind::Instance) {
        penalty += 18.0f;
    } else if (item.count <= 1) {
        penalty += 3.0f;
    }
    return penalty;
}

float healChoiceScore(
    const GameTestObjectEntrySnapshot& item,
    ConsumableUseKind kind,
    double healAmount,
    float desiredHeal,
    int missingHp,
    bool emergency)
{
    const float heal = static_cast<float>(healAmount);
    const float underDesired = std::max(0.0f, desiredHeal - heal);
    const float overMissing = std::max(0.0f, heal - static_cast<float>(missingHp));
    const float overDesired = std::max(0.0f, heal - desiredHeal);
    float score =
        underDesired * (emergency ? 4.0f : 2.3f) +
        overMissing * 1.4f +
        overDesired * 0.25f +
        static_cast<float>(std::max(0, item.rarity)) * 1.5f +
        static_cast<float>(std::max(0, item.price)) * 0.01f;
    if (kind == ConsumableUseKind::Instance) {
        score += 9.0f;
    }
    if (emergency && heal >= desiredHeal) {
        score -= 32.0f;
    }
    return score;
}

float buffBenefitScore(
    const GameTestSnapshot& snapshot,
    const CombatPressure& pressure,
    const AutoSimulationConsumableProfile& profile,
    float hpRatio)
{
    if (!pressure.buffWorthyNearby) {
        return 0.0f;
    }

    float benefit = 0.0f;
    if (profile.attackMultiplier > 1.0 && !hasActiveModifier(snapshot, "buff_attack")) {
        benefit += static_cast<float>((profile.attackMultiplier - 1.0) * 145.0);
        if (snapshot.ring.hasCombatTool) {
            benefit += 18.0f;
        }
        if (pressure.bossNearby) {
            benefit += 38.0f;
        }
        if (pressure.nearbyEnemies >= 2) {
            benefit += 14.0f;
        }
    }

    if (profile.speedMultiplier > 1.0 && !hasActiveModifier(snapshot, "buff_speed")) {
        benefit += static_cast<float>((profile.speedMultiplier - 1.0) * 135.0);
        if (hpRatio <= 0.55f) {
            benefit += 18.0f;
        }
        if (pressure.bossNearby) {
            benefit += 22.0f;
        }
        if (pressure.nearbyEnemies >= 2) {
            benefit += 12.0f;
        }
    }

    if (profile.defenseMultiplier > 1.0 && !hasActiveModifier(snapshot, "buff_defense")) {
        benefit += static_cast<float>((profile.defenseMultiplier - 1.0) * 155.0);
        if (hpRatio <= 0.75f) {
            benefit += 30.0f;
        }
        if (pressure.bossNearby) {
            benefit += 35.0f;
        }
        if (pressure.nearbyEnemies >= 2) {
            benefit += 18.0f;
        }
    }

    if (profile.giantValue > 0.0 && !hasActiveState(snapshot, "status_giant")) {
        if (pressure.bossNearby || hpRatio <= 0.50f || pressure.nearbyEnemies >= 3) {
            benefit += 50.0f;
            if (pressure.bossNearby) {
                benefit += 35.0f;
            }
        }
    }

    return benefit;
}

std::optional<ConsumableChoice> chooseHeal(
    const GameTestSnapshot& snapshot,
    bool dangerNearby,
    bool emergency)
{
    const int missingHp = snapshot.player.maxHp - snapshot.player.hp;
    const float desiredHp = static_cast<float>(snapshot.player.maxHp) *
        (dangerNearby ? DangerDesiredHpRatio : NormalDesiredHpRatio);
    const float desiredHeal = std::clamp(
        desiredHp - static_cast<float>(snapshot.player.hp),
        1.0f,
        static_cast<float>(missingHp));

    ConsumableChoice best;
    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        const AutoSimulationConsumableProfile profile = autoSimulationConsumableProfile(item);
        if (!usableConsumableItem(item, profile) || profile.heal <= 0.0) {
            continue;
        }

        const ConsumableUseKind kind = useKindFor(item);
        const float score = healChoiceScore(item, kind, profile.heal, desiredHeal, missingHp, emergency);
        if (score < best.score) {
            best.item = &item;
            best.kind = kind;
            best.heal = profile.heal;
            best.score = score;
            best.reasonPrefix = emergency ? "emergency_heal" : "low_hp_heal";
        }
    }

    if (best.item == nullptr) {
        return std::nullopt;
    }
    return best;
}

std::optional<ConsumableChoice> chooseBuff(
    const GameTestSnapshot& snapshot,
    const CombatPressure& pressure,
    float hpRatio)
{
    ConsumableChoice best;
    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        const AutoSimulationConsumableProfile profile = autoSimulationConsumableProfile(item);
        if (!usableConsumableItem(item, profile)) {
            continue;
        }

        const float benefit = buffBenefitScore(snapshot, pressure, profile, hpRatio);
        if (benefit <= 0.0f) {
            continue;
        }

        const ConsumableUseKind kind = useKindFor(item);
        float score = 175.0f - benefit + itemCostPenalty(item, kind);
        if (profile.giantValue > 0.0) {
            score += 42.0f;
        }
        if (profile.heal > 0.0 && snapshot.player.hp >= snapshot.player.maxHp) {
            score += 8.0f;
        }

        if (score < best.score) {
            best.item = &item;
            best.kind = kind;
            best.score = score;
            best.reasonPrefix = "combat_buff";
        }
    }

    if (best.item == nullptr) {
        return std::nullopt;
    }
    return best;
}

GameTestAction makeAction(const ConsumableChoice& choice, const GameTestSnapshot& snapshot)
{
    GameTestAction action;
    action.kind = choice.kind == ConsumableUseKind::Stack
        ? GameTestActionKind::UseBackpackStackItem
        : GameTestActionKind::UseBackpackInstanceItem;
    action.objectId = choice.item->objectId;
    action.instanceId = choice.item->instanceId;
    action.count = 1;
    action.reason =
        choice.reasonPrefix +
        " hp=" + std::to_string(snapshot.player.hp) +
        "/" + std::to_string(snapshot.player.maxHp);
    if (choice.heal > 0.0) {
        action.reason += " heal=" + std::to_string(static_cast<int>(std::round(choice.heal)));
    }
    action.reason += " " + choice.item->objectId;
    return action;
}

} // namespace

AutoSimulationConsumableProfile autoSimulationConsumableProfile(
    const GameTestObjectEntrySnapshot& item)
{
    return buildConsumableProfile(item);
}

std::optional<GameTestAction> AutoSimulationConsumablePlanner::chooseAction(const GameTestSnapshot& snapshot) const
{
    if (snapshot.worldLoading || snapshot.transitionActive || snapshot.dialogueActive) {
        return std::nullopt;
    }
    if (snapshot.firstItemNoticeActive || snapshot.pendingStoryDelayActive) {
        return std::nullopt;
    }
    if (snapshot.screenMode != GameTestScreenMode::Playing ||
        snapshot.player.maxHp <= 0 ||
        snapshot.player.hp <= 0) {
        return std::nullopt;
    }

    const float hpRatio = static_cast<float>(snapshot.player.hp) / static_cast<float>(snapshot.player.maxHp);
    const CombatPressure pressure = combatPressure(snapshot);
    const bool emergency = hpRatio <= EmergencyHealHpRatio;
    const float triggerRatio = pressure.dangerNearby ? DangerHealHpRatio : NormalHealHpRatio;
    if (snapshot.player.hp < snapshot.player.maxHp && (emergency || hpRatio <= triggerRatio)) {
        if (std::optional<ConsumableChoice> heal = chooseHeal(snapshot, pressure.dangerNearby, emergency)) {
            return makeAction(*heal, snapshot);
        }
    }

    if (std::optional<ConsumableChoice> buff = chooseBuff(snapshot, pressure, hpRatio)) {
        return makeAction(*buff, snapshot);
    }

    return std::nullopt;
}

} // namespace majo::autosim
