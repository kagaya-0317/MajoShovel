#include "devtools/autosim/AutoSimulationIntentFormatter.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace majo::autosim {

namespace {

constexpr float SameTargetDistance = 34.0f;

AutoSimulationIntent makeIntent(
    AutoSimulationGoal goal,
    std::string prefix,
    std::string subject,
    std::string suffix,
    AutoSimulationIntentIconKind iconKind = AutoSimulationIntentIconKind::None,
    std::string iconKey = {})
{
    AutoSimulationIntent intent;
    intent.visible = true;
    intent.goal = goal;
    intent.iconKind = iconKind;
    intent.iconKey = std::move(iconKey);
    intent.prefix = std::move(prefix);
    intent.subject = std::move(subject);
    intent.suffix = std::move(suffix);
    return intent;
}

void attachTarget(AutoSimulationIntent& intent, const AutoSimulationPlan& plan)
{
    intent.hasTarget = plan.hasTarget;
    intent.targetWorld = plan.targetWorld;
}

const GameTestDropSnapshot* matchingDrop(const GameTestSnapshot& snapshot, Vec2 target)
{
    const GameTestDropSnapshot* best = nullptr;
    float bestDistanceSq = SameTargetDistance * SameTargetDistance;
    for (const GameTestDropSnapshot& drop : snapshot.drops) {
        const float distSq = distanceSquared(drop.position, target);
        if (distSq < bestDistanceSq) {
            bestDistanceSq = distSq;
            best = &drop;
        }
    }
    return best;
}

AutoSimulationIntentIconKind intentIconKind(GameTestIconKind kind)
{
    switch (kind) {
    case GameTestIconKind::Object:
        return AutoSimulationIntentIconKind::Object;
    case GameTestIconKind::World:
        return AutoSimulationIntentIconKind::World;
    case GameTestIconKind::None:
        break;
    }
    return AutoSimulationIntentIconKind::None;
}

std::string terrainName(GameTestTerrainKind kind)
{
    switch (kind) {
    case GameTestTerrainKind::Dirt:
        return "土壁";
    case GameTestTerrainKind::Rock:
        return "岩壁";
    case GameTestTerrainKind::Ore:
        return "鉱石壁";
    case GameTestTerrainKind::HardRock:
        return "硬い岩壁";
    case GameTestTerrainKind::Empty:
        break;
    }
    return "壁";
}

bool reasonContains(const AutoSimulationPlan& plan, std::string_view text)
{
    return plan.reason.find(text) != std::string::npos;
}

} // namespace

AutoSimulationIntent AutoSimulationIntentFormatter::format(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPlan& plan) const
{
    AutoSimulationIntent intent;
    if (plan.goal == AutoSimulationGoal::None) {
        return intent;
    }

    if (plan.routeAvoidingHardWall &&
        plan.goal != AutoSimulationGoal::MineWall &&
        (plan.goal == AutoSimulationGoal::FollowMainPath ||
            plan.goal == AutoSimulationGoal::CollectDrop ||
            plan.goal == AutoSimulationGoal::OpenChest ||
            plan.goal == AutoSimulationGoal::DiscoverWarp ||
            plan.goal == AutoSimulationGoal::ResumeFrontier ||
            plan.goal == AutoSimulationGoal::ApproachBoss)) {
        intent = makeIntent(plan.goal, "", "硬い壁があるから遠回りしていこう", "", AutoSimulationIntentIconKind::Path);
        attachTarget(intent, plan);
        return intent;
    }

    switch (plan.goal) {
    case AutoSimulationGoal::DismissUi:
        intent = makeIntent(plan.goal, "", "画面を進めたい", "");
        break;
    case AutoSimulationGoal::EquipLoadout:
        intent = makeIntent(plan.goal, "", "装備を整えたい", "");
        break;
    case AutoSimulationGoal::UseItem:
        intent = makeIntent(plan.goal, "", "消耗アイテムを使いたい", "", AutoSimulationIntentIconKind::Object);
        break;
    case AutoSimulationGoal::MineWall: {
        const std::string wallName = terrainName(plan.hasTargetTerrainKind ? plan.targetTerrainKind : GameTestTerrainKind::Empty);
        if (reasonContains(plan, "map_clue") && plan.targetTerrainKind == GameTestTerrainKind::Dirt) {
            intent = makeIntent(plan.goal, "", "地図の光へ向けて土壁を掘りたい", "", AutoSimulationIntentIconKind::Dig);
        } else if (reasonContains(plan, "map_clue")) {
            intent = makeIntent(plan.goal, "", "地図の光へ向けて進みたい", "", AutoSimulationIntentIconKind::Dig);
        } else if (reasonContains(plan, "explore_soft_dirt")) {
            intent = makeIntent(plan.goal, "", "やわらかい土壁を掘って奥を探したい", "", AutoSimulationIntentIconKind::Dig);
        } else if (reasonContains(plan, "explore") && plan.targetTerrainKind == GameTestTerrainKind::Dirt) {
            intent = makeIntent(plan.goal, "", "土壁を掘って奥を探したい", "", AutoSimulationIntentIconKind::Dig);
        } else if (plan.routeAvoidingHardWall && plan.targetTerrainKind == GameTestTerrainKind::Dirt) {
            intent = makeIntent(plan.goal, "", "硬い壁を避けて土壁を掘りたい", "", AutoSimulationIntentIconKind::Dig);
        } else if (plan.targetTerrainKind == GameTestTerrainKind::Dirt) {
            intent = makeIntent(plan.goal, "そこの", wallName, "を掘って進みたい", AutoSimulationIntentIconKind::Dig);
        } else {
            intent = makeIntent(plan.goal, "そこの", wallName, "を掘りたい", AutoSimulationIntentIconKind::Dig);
        }
        break;
    }
    case AutoSimulationGoal::Combat:
        intent = makeIntent(plan.goal, "近くの", "敵", "を倒したい", AutoSimulationIntentIconKind::Enemy);
        break;
    case AutoSimulationGoal::CollectDrop:
        if (const GameTestDropSnapshot* drop = matchingDrop(snapshot, plan.targetWorld)) {
            const std::string name = drop->displayName.empty() ? std::string("アイテム") : drop->displayName;
            intent = makeIntent(
                plan.goal,
                "そこの",
                name,
                "を取りにいきたい",
                intentIconKind(drop->iconKind),
                drop->iconKey);
        } else {
            intent = makeIntent(plan.goal, "そこの", "アイテム", "を取りにいきたい", AutoSimulationIntentIconKind::Object);
        }
        break;
    case AutoSimulationGoal::OpenChest:
        intent = makeIntent(plan.goal, "そこの", "宝箱", "を開けたい", AutoSimulationIntentIconKind::Chest);
        break;
    case AutoSimulationGoal::DiscoverWarp:
        if (reasonContains(plan, "visible_warp")) {
            intent = makeIntent(plan.goal, "見えている", "ワープ", "に触れたい", AutoSimulationIntentIconKind::Warp);
        } else if (reasonContains(plan, "map_clue")) {
            intent = makeIntent(plan.goal, "", "地図の光を調べに行きたい", "", AutoSimulationIntentIconKind::Path);
        } else if (reasonContains(plan, "explore_open")) {
            intent = makeIntent(plan.goal, "", "まだ調べていない通路を進みたい", "", AutoSimulationIntentIconKind::Path);
        } else if (reasonContains(plan, "explore")) {
            intent = makeIntent(plan.goal, "", "土壁が多い方を探したい", "", AutoSimulationIntentIconKind::Path);
        } else {
            intent = makeIntent(plan.goal, "", "次のワープ", "を見つけたい", AutoSimulationIntentIconKind::Warp);
        }
        break;
    case AutoSimulationGoal::ReturnToBase:
        if (reasonContains(plan, "backpack_full")) {
            intent = makeIntent(plan.goal, "", "リュックがいっぱいなので拠点へ戻りたい", "", AutoSimulationIntentIconKind::Base);
        } else if (reasonContains(plan, "low_hp")) {
            intent = makeIntent(plan.goal, "", "HPが少なく回復手段もないので拠点へ戻りたい", "", AutoSimulationIntentIconKind::Base);
        } else if (reasonContains(plan, "checkpoint")) {
            intent = makeIntent(plan.goal, "", "拠点で準備したい", "", AutoSimulationIntentIconKind::Base);
        } else {
            intent = makeIntent(plan.goal, "", "拠点へ戻りたい", "", AutoSimulationIntentIconKind::Base);
        }
        break;
    case AutoSimulationGoal::ResumeFrontier:
        intent = makeIntent(plan.goal, "", "前に進んだ地点へ戻りたい", "", AutoSimulationIntentIconKind::Path);
        break;
    case AutoSimulationGoal::ApproachBoss:
        intent = makeIntent(plan.goal, "", "奥へ進んでボスを探したい", "", AutoSimulationIntentIconKind::Path);
        break;
    case AutoSimulationGoal::FollowMainPath:
        intent = makeIntent(plan.goal, "", "先に進みたい", "", AutoSimulationIntentIconKind::Path);
        break;
    case AutoSimulationGoal::EscapeStuck:
        intent = makeIntent(plan.goal, "", "引っかかったので抜け出したい", "", AutoSimulationIntentIconKind::Path);
        break;
    case AutoSimulationGoal::None:
        break;
    }

    attachTarget(intent, plan);
    (void)snapshot;
    return intent;
}

} // namespace majo::autosim
