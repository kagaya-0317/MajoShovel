#include "devtools/autosim/AutoSimulationTypes.hpp"

namespace majo::autosim {

const char* autoSimulationStateName(AutoSimulationState state)
{
    switch (state) {
    case AutoSimulationState::Idle: return "idle";
    case AutoSimulationState::Running: return "running";
    case AutoSimulationState::Paused: return "paused";
    }
    return "unknown";
}

const char* autoSimulationResultName(AutoSimulationResult result)
{
    switch (result) {
    case AutoSimulationResult::None: return "none";
    case AutoSimulationResult::StageClear: return "stage_clear";
    case AutoSimulationResult::GameOver: return "game_over";
    case AutoSimulationResult::AstralResult: return "astral_result";
    case AutoSimulationResult::Timeout: return "timeout";
    case AutoSimulationResult::Stopped: return "stopped";
    }
    return "unknown";
}

const char* autoSimulationGoalName(AutoSimulationGoal goal)
{
    switch (goal) {
    case AutoSimulationGoal::None: return "none";
    case AutoSimulationGoal::DismissUi: return "dismiss_ui";
    case AutoSimulationGoal::EquipLoadout: return "equip_loadout";
    case AutoSimulationGoal::UseItem: return "use_item";
    case AutoSimulationGoal::MineWall: return "mine_wall";
    case AutoSimulationGoal::Combat: return "combat";
    case AutoSimulationGoal::CollectDrop: return "collect_drop";
    case AutoSimulationGoal::OpenChest: return "open_chest";
    case AutoSimulationGoal::DiscoverWarp: return "discover_warp";
    case AutoSimulationGoal::ReturnToBase: return "return_to_base";
    case AutoSimulationGoal::ApproachBoss: return "approach_boss";
    case AutoSimulationGoal::FollowMainPath: return "follow_main_path";
    case AutoSimulationGoal::EscapeStuck: return "escape_stuck";
    }
    return "unknown";
}

} // namespace majo::autosim
