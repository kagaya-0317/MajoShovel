#pragma once

#include <string>

namespace majo {

enum class GameTestActionKind {
    None,
    ReturnToBaseViaWarp,
    ReturnToBaseAfterGameOver,
    StartMiningFromBase,
    SyncEncyclopedia,
    EquipBackpackStaff,
    EquipBackpackItemToRing,
    RemoveRingItemToBackpack,
    DepositBackpackStack,
    DepositBackpackInstance,
    SellBackpackStack,
    SellBackpackInstance,
    ProtectBackpackInstance,
    RepairBackpackInstance,
    EnhanceBackpackStackAttack,
    EnhanceBackpackStackDig,
    EnhanceBackpackInstanceAttack,
    EnhanceBackpackInstanceDig,
    BuyBaseUpgrade,
};

struct GameTestAction {
    GameTestActionKind kind = GameTestActionKind::None;
    std::string objectId;
    std::string instanceId;
    int count = 1;
    int upgradeIndex = -1;
    int ringIndex = -1;
    int ringItemIndex = -1;
    std::string reason;
};

struct GameTestActionResult {
    bool applied = false;
    std::string message;
};

inline const char* gameTestActionKindName(GameTestActionKind kind)
{
    switch (kind) {
    case GameTestActionKind::None: return "none";
    case GameTestActionKind::ReturnToBaseViaWarp: return "return_to_base_via_warp";
    case GameTestActionKind::ReturnToBaseAfterGameOver: return "return_to_base_after_game_over";
    case GameTestActionKind::StartMiningFromBase: return "start_mining_from_base";
    case GameTestActionKind::SyncEncyclopedia: return "sync_encyclopedia";
    case GameTestActionKind::EquipBackpackStaff: return "equip_backpack_staff";
    case GameTestActionKind::EquipBackpackItemToRing: return "equip_backpack_item_to_ring";
    case GameTestActionKind::RemoveRingItemToBackpack: return "remove_ring_item_to_backpack";
    case GameTestActionKind::DepositBackpackStack: return "deposit_backpack_stack";
    case GameTestActionKind::DepositBackpackInstance: return "deposit_backpack_instance";
    case GameTestActionKind::SellBackpackStack: return "sell_backpack_stack";
    case GameTestActionKind::SellBackpackInstance: return "sell_backpack_instance";
    case GameTestActionKind::ProtectBackpackInstance: return "protect_backpack_instance";
    case GameTestActionKind::RepairBackpackInstance: return "repair_backpack_instance";
    case GameTestActionKind::EnhanceBackpackStackAttack: return "enhance_backpack_stack_attack";
    case GameTestActionKind::EnhanceBackpackStackDig: return "enhance_backpack_stack_dig";
    case GameTestActionKind::EnhanceBackpackInstanceAttack: return "enhance_backpack_instance_attack";
    case GameTestActionKind::EnhanceBackpackInstanceDig: return "enhance_backpack_instance_dig";
    case GameTestActionKind::BuyBaseUpgrade: return "buy_base_upgrade";
    }
    return "unknown";
}

} // namespace majo
