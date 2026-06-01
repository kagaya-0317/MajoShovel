#include "game/DungeonEventDefinition.hpp"

#include <algorithm>
#include <array>

namespace majo {

namespace {

constexpr DungeonEventCavityProfile CombatTreasureCavity{6, 8, 0.58f, 3, 0.88f};
constexpr DungeonEventCavityProfile CombatRoomCavity{6, 8, 0.60f, 3, 0.88f};
constexpr DungeonEventCavityProfile BossRoomCavity{7, 9, 0.58f, 3, 0.92f};
constexpr DungeonEventCavityProfile PuzzleRoomCavity{5, 7, 0.56f, 2, 0.76f};
constexpr DungeonEventCavityProfile CoinRoomCavity{4, 6, 0.62f, 2, 0.66f};
constexpr DungeonEventCavityProfile GuideCavity{3, 4, 0.68f, 1, 0.58f};
constexpr DungeonEventCavityProfile BuriedWitchCavity{2, 3, 0.72f, 1, 0.45f};
constexpr DungeonEventCavityProfile SurroundedWitchCavity{4, 5, 0.62f, 2, 0.65f};
constexpr DungeonEventCavityProfile WitchCavity{3, 4, 0.66f, 1, 0.56f};

constexpr std::array<DungeonEventDefinition, 14> Definitions{{
    {
        DungeonEventKind::SleepingEnemyTreasure,
        "sleeping_enemy_treasure",
        "眠る敵の宝物庫",
        DungeonEventCategory::Combat,
        true,
        true,
        true,
        false,
        false,
        5.0f,
        4.0f,
        CombatTreasureCavity,
    },
    {
        DungeonEventKind::MonsterSwarmRoom,
        "monster_swarm_room",
        "大量発生の部屋",
        DungeonEventCategory::Combat,
        true,
        true,
        true,
        false,
        true,
        5.0f,
        4.0f,
        CombatRoomCavity,
    },
    {
        DungeonEventKind::NestRoom,
        "nest_room",
        "巣穴つぶし",
        DungeonEventCategory::Combat,
        true,
        true,
        true,
        false,
        true,
        5.0f,
        4.0f,
        CombatRoomCavity,
    },
    {
        DungeonEventKind::BossMonsterRoom,
        "boss_monster_room",
        "親分モンスター部屋",
        DungeonEventCategory::Combat,
        true,
        true,
        true,
        false,
        true,
        7.0f,
        4.5f,
        BossRoomCavity,
    },
    {
        DungeonEventKind::GlowingRockRoom,
        "glowing_rock_room",
        "光る岩",
        DungeonEventCategory::Mining,
        true,
        true,
        true,
        false,
        false,
        5.0f,
        4.0f,
        PuzzleRoomCavity,
    },
    {
        DungeonEventKind::ElectricCircuitRoom,
        "electric_circuit_room",
        "電気回路",
        DungeonEventCategory::Attribute,
        true,
        true,
        true,
        false,
        false,
        5.0f,
        4.5f,
        PuzzleRoomCavity,
    },
    {
        DungeonEventKind::CoinRoom,
        "coin_room",
        "金貨部屋",
        DungeonEventCategory::SpecialRoom,
        true,
        false,
        false,
        false,
        false,
        4.0f,
        3.6f,
        CoinRoomCavity,
    },
    {
        DungeonEventKind::WarpGuideMap,
        "warp_guide_map",
        "ワープ案内図",
        DungeonEventCategory::Exploration,
        true,
        true,
        true,
        true,
        false,
        4.0f,
        3.2f,
        GuideCavity,
    },
    {
        DungeonEventKind::BuriedWitch,
        "buried_witch",
        "埋まった魔女",
        DungeonEventCategory::Witch,
        true,
        true,
        true,
        false,
        false,
        5.0f,
        3.0f,
        BuriedWitchCavity,
    },
    {
        DungeonEventKind::LostBaggageWitch,
        "lost_baggage_witch",
        "荷物を落とした魔女",
        DungeonEventCategory::Witch,
        true,
        true,
        true,
        false,
        false,
        5.0f,
        3.0f,
        WitchCavity,
    },
    {
        DungeonEventKind::ItemRequestWitch,
        "item_request_witch",
        "アイテム要求魔女",
        DungeonEventCategory::Witch,
        true,
        true,
        true,
        false,
        false,
        5.0f,
        3.0f,
        WitchCavity,
    },
    {
        DungeonEventKind::SurroundedWitch,
        "surrounded_witch",
        "敵に囲まれた魔女",
        DungeonEventCategory::Witch,
        true,
        true,
        true,
        false,
        true,
        5.0f,
        3.0f,
        SurroundedWitchCavity,
    },
    {
        DungeonEventKind::ColdWitchCampfire,
        "cold_witch_campfire",
        "寒がっている魔女",
        DungeonEventCategory::Witch,
        true,
        true,
        true,
        false,
        false,
        5.0f,
        3.0f,
        WitchCavity,
    },
    {
        DungeonEventKind::HeavyRockWitch,
        "heavy_rock_witch",
        "重いもの魔女",
        DungeonEventCategory::Witch,
        true,
        true,
        true,
        false,
        false,
        5.0f,
        3.0f,
        WitchCavity,
    },
}};

constexpr std::array<DungeonEventKind, 11> FallbackCandidates{{
    DungeonEventKind::SleepingEnemyTreasure,
    DungeonEventKind::MonsterSwarmRoom,
    DungeonEventKind::NestRoom,
    DungeonEventKind::GlowingRockRoom,
    DungeonEventKind::ElectricCircuitRoom,
    DungeonEventKind::BuriedWitch,
    DungeonEventKind::LostBaggageWitch,
    DungeonEventKind::ItemRequestWitch,
    DungeonEventKind::SurroundedWitch,
    DungeonEventKind::ColdWitchCampfire,
    DungeonEventKind::HeavyRockWitch,
}};

constexpr std::array<DungeonEventKind, 6> Stage01Candidates{{
    DungeonEventKind::GlowingRockRoom,
    DungeonEventKind::BuriedWitch,
    DungeonEventKind::LostBaggageWitch,
    DungeonEventKind::MonsterSwarmRoom,
    DungeonEventKind::ItemRequestWitch,
    DungeonEventKind::NestRoom,
}};

constexpr std::array<DungeonEventKind, 6> Stage02Candidates{{
    DungeonEventKind::HeavyRockWitch,
    DungeonEventKind::ColdWitchCampfire,
    DungeonEventKind::ElectricCircuitRoom,
    DungeonEventKind::ItemRequestWitch,
    DungeonEventKind::SurroundedWitch,
    DungeonEventKind::BossMonsterRoom,
}};

constexpr std::array<DungeonEventKind, 4> Stage03Candidates{{
    DungeonEventKind::SleepingEnemyTreasure,
    DungeonEventKind::MonsterSwarmRoom,
    DungeonEventKind::NestRoom,
    DungeonEventKind::BossMonsterRoom,
}};

constexpr std::array<DungeonEventKind, 9> Stage04Candidates{{
    DungeonEventKind::MonsterSwarmRoom,
    DungeonEventKind::NestRoom,
    DungeonEventKind::ElectricCircuitRoom,
    DungeonEventKind::HeavyRockWitch,
    DungeonEventKind::GlowingRockRoom,
    DungeonEventKind::BossMonsterRoom,
    DungeonEventKind::ColdWitchCampfire,
    DungeonEventKind::SurroundedWitch,
    DungeonEventKind::SleepingEnemyTreasure,
}};

constexpr std::array<DungeonEventFixedPlacement, 6> Stage01FixedPlacements{{
    {DungeonEventKind::GlowingRockRoom, 0, 0.32f},
    {DungeonEventKind::BuriedWitch, 0, 0.68f},
    {DungeonEventKind::LostBaggageWitch, 1, 0.32f},
    {DungeonEventKind::MonsterSwarmRoom, 1, 0.68f},
    {DungeonEventKind::ItemRequestWitch, 2, 0.32f},
    {DungeonEventKind::NestRoom, 2, 0.68f},
}};

constexpr std::array<DungeonEventFixedPlacement, 6> Stage02FixedPlacements{{
    {DungeonEventKind::HeavyRockWitch, 0, 0.32f},
    {DungeonEventKind::ColdWitchCampfire, 0, 0.68f},
    {DungeonEventKind::ElectricCircuitRoom, 1, 0.32f},
    {DungeonEventKind::ItemRequestWitch, 1, 0.68f},
    {DungeonEventKind::SurroundedWitch, 2, 0.32f},
    {DungeonEventKind::BossMonsterRoom, 2, 0.68f},
}};

constexpr std::array<DungeonEventFixedPlacement, 4> Stage03FixedPlacements{{
    {DungeonEventKind::SleepingEnemyTreasure, 0, 0.32f},
    {DungeonEventKind::MonsterSwarmRoom, 0, 0.68f},
    {DungeonEventKind::NestRoom, 1, 0.50f},
    {DungeonEventKind::BossMonsterRoom, 2, 0.50f},
}};

} // namespace

std::span<const DungeonEventDefinition> dungeonEventDefinitions()
{
    return Definitions;
}

const DungeonEventDefinition& dungeonEventDefinition(DungeonEventKind kind)
{
    const auto it = std::find_if(Definitions.begin(), Definitions.end(), [kind](const DungeonEventDefinition& definition) {
        return definition.kind == kind;
    });
    return it != Definitions.end() ? *it : Definitions.front();
}

const DungeonEventDefinition* findDungeonEventDefinitionById(std::string_view id)
{
    const auto it = std::find_if(Definitions.begin(), Definitions.end(), [id](const DungeonEventDefinition& definition) {
        return definition.id == id;
    });
    return it != Definitions.end() ? &*it : nullptr;
}

std::string_view dungeonEventKindId(DungeonEventKind kind)
{
    return dungeonEventDefinition(kind).id;
}

std::string_view dungeonEventKindDisplayName(DungeonEventKind kind)
{
    return dungeonEventDefinition(kind).displayName;
}

bool dungeonEventKindFromId(std::string_view id, DungeonEventKind& outKind)
{
    const DungeonEventDefinition* definition = findDungeonEventDefinitionById(id);
    if (definition == nullptr) {
        return false;
    }
    outKind = definition->kind;
    return true;
}

bool dungeonEventKindIsWitch(DungeonEventKind kind)
{
    return dungeonEventDefinition(kind).category == DungeonEventCategory::Witch;
}

bool dungeonEventKindIsCombat(DungeonEventKind kind)
{
    return dungeonEventDefinition(kind).category == DungeonEventCategory::Combat;
}

bool dungeonEventKindIsHighDanger(DungeonEventKind kind)
{
    return dungeonEventDefinition(kind).highDanger;
}

bool dungeonEventKindHasDiscoveryDialogue(DungeonEventKind kind)
{
    return dungeonEventDefinition(kind).discoveryDialogue;
}

float dungeonEventDiscoveryRadiusTiles(DungeonEventKind kind)
{
    return dungeonEventDefinition(kind).discoveryRadiusTiles;
}

float dungeonEventLightRadiusTiles(DungeonEventKind kind)
{
    return dungeonEventDefinition(kind).selfLightRadiusTiles;
}

DungeonEventCavityProfile dungeonEventCavityProfile(DungeonEventKind kind)
{
    return dungeonEventDefinition(kind).cavity;
}

std::optional<DungeonEventKind> dungeonEventKindForSpecialRoom(SpecialRoomType type, int index)
{
    switch (type) {
    case SpecialRoomType::TreasureRoom:
        return DungeonEventKind::SleepingEnemyTreasure;
    case SpecialRoomType::EnemyRoom:
        return index % 2 == 0 ? DungeonEventKind::MonsterSwarmRoom : DungeonEventKind::NestRoom;
    case SpecialRoomType::OreRoom:
        return index % 2 == 0 ? DungeonEventKind::GlowingRockRoom : DungeonEventKind::ElectricCircuitRoom;
    case SpecialRoomType::CoinRoom:
        return DungeonEventKind::CoinRoom;
    case SpecialRoomType::SafeCavern:
        break;
    case SpecialRoomType::None:
        break;
    }
    return std::nullopt;
}

std::span<const DungeonEventKind> dungeonEventStageCandidateKinds(std::string_view stageId)
{
    if (stageId == "stage_01_stardust") {
        return Stage01Candidates;
    }
    if (stageId == "stage_02_junk_magic") {
        return Stage02Candidates;
    }
    if (stageId == "stage_03_star_core") {
        return Stage03Candidates;
    }
    if (stageId == "stage_04_astral_mine") {
        return Stage04Candidates;
    }
    return FallbackCandidates;
}

std::span<const DungeonEventFixedPlacement> dungeonEventFixedPlacements(std::string_view stageId)
{
    if (stageId == "stage_01_stardust") {
        return Stage01FixedPlacements;
    }
    if (stageId == "stage_02_junk_magic") {
        return Stage02FixedPlacements;
    }
    if (stageId == "stage_03_star_core") {
        return Stage03FixedPlacements;
    }
    return {};
}

bool dungeonEventKindAllowedForStage(DungeonEventKind kind, std::string_view stageId)
{
    const DungeonEventDefinition& definition = dungeonEventDefinition(kind);
    if (definition.category == DungeonEventCategory::SpecialRoom) {
        return true;
    }
    if (!definition.stageCandidate) {
        return false;
    }
    if (definition.requiresUndiscoveredWarpPoint && stageId == "stage_04_astral_mine") {
        return false;
    }
    const std::span<const DungeonEventKind> candidates = dungeonEventStageCandidateKinds(stageId);
    return std::find(candidates.begin(), candidates.end(), kind) != candidates.end();
}

}
