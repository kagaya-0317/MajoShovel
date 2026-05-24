#include "game/DungeonEventDefinition.hpp"

#include <algorithm>
#include <array>

namespace majo {

namespace {

constexpr DungeonEventCavityProfile CombatTreasureCavity{6, 8, 0.58f, 3, 0.88f};
constexpr DungeonEventCavityProfile CombatRoomCavity{6, 8, 0.60f, 3, 0.88f};
constexpr DungeonEventCavityProfile BossRoomCavity{7, 9, 0.58f, 3, 0.92f};
constexpr DungeonEventCavityProfile PuzzleRoomCavity{5, 7, 0.56f, 2, 0.76f};
constexpr DungeonEventCavityProfile SafeCavernCavity{4, 6, 0.68f, 2, 0.70f};
constexpr DungeonEventCavityProfile CoinRoomCavity{4, 6, 0.62f, 2, 0.66f};
constexpr DungeonEventCavityProfile GuideCavity{3, 4, 0.68f, 1, 0.58f};
constexpr DungeonEventCavityProfile BuriedWitchCavity{2, 3, 0.72f, 1, 0.45f};
constexpr DungeonEventCavityProfile SurroundedWitchCavity{4, 5, 0.62f, 2, 0.65f};
constexpr DungeonEventCavityProfile WitchCavity{3, 4, 0.66f, 1, 0.56f};

constexpr std::array<DungeonEventDefinition, 15> Definitions{{
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
        DungeonEventKind::SafeCavern,
        "safe_cavern",
        "休憩空洞",
        DungeonEventCategory::SpecialRoom,
        true,
        false,
        false,
        false,
        false,
        4.0f,
        3.6f,
        SafeCavernCavity,
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

constexpr std::array<DungeonEventKind, 12> FallbackCandidates{{
    DungeonEventKind::SleepingEnemyTreasure,
    DungeonEventKind::MonsterSwarmRoom,
    DungeonEventKind::NestRoom,
    DungeonEventKind::GlowingRockRoom,
    DungeonEventKind::ElectricCircuitRoom,
    DungeonEventKind::WarpGuideMap,
    DungeonEventKind::BuriedWitch,
    DungeonEventKind::LostBaggageWitch,
    DungeonEventKind::ItemRequestWitch,
    DungeonEventKind::SurroundedWitch,
    DungeonEventKind::ColdWitchCampfire,
    DungeonEventKind::HeavyRockWitch,
}};

constexpr std::array<DungeonEventKind, 6> Stage01Candidates{{
    DungeonEventKind::BuriedWitch,
    DungeonEventKind::MonsterSwarmRoom,
    DungeonEventKind::GlowingRockRoom,
    DungeonEventKind::LostBaggageWitch,
    DungeonEventKind::SleepingEnemyTreasure,
    DungeonEventKind::SurroundedWitch,
}};

constexpr std::array<DungeonEventKind, 7> Stage02Candidates{{
    DungeonEventKind::ItemRequestWitch,
    DungeonEventKind::NestRoom,
    DungeonEventKind::ElectricCircuitRoom,
    DungeonEventKind::HeavyRockWitch,
    DungeonEventKind::SleepingEnemyTreasure,
    DungeonEventKind::ColdWitchCampfire,
    DungeonEventKind::BossMonsterRoom,
}};

constexpr std::array<DungeonEventKind, 6> Stage03Candidates{{
    DungeonEventKind::GlowingRockRoom,
    DungeonEventKind::ElectricCircuitRoom,
    DungeonEventKind::SleepingEnemyTreasure,
    DungeonEventKind::ColdWitchCampfire,
    DungeonEventKind::HeavyRockWitch,
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
        return DungeonEventKind::SafeCavern;
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
