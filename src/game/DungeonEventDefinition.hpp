#pragma once

#include "game/DungeonLayout.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace majo {

enum class DungeonEventKind {
    SleepingEnemyTreasure,
    MonsterSwarmRoom,
    NestRoom,
    BossMonsterRoom,
    GlowingRockRoom,
    ElectricCircuitRoom,
    CoinRoom,
    WarpGuideMap,
    BuriedWitch,
    LostBaggageWitch,
    ItemRequestWitch,
    SurroundedWitch,
    ColdWitchCampfire,
    HeavyRockWitch,
};

enum class DungeonEventCategory {
    Combat,
    Mining,
    Attribute,
    Exploration,
    Witch,
    SpecialRoom,
};

struct DungeonEventCavityProfile {
    int minRadiusTiles = 3;
    int maxRadiusTiles = 5;
    float fillRatio = 0.62f;
    int innerRadiusTiles = 1;
    float specialRoomRadiusScale = 0.72f;
};

struct DungeonEventDefinition {
    DungeonEventKind kind = DungeonEventKind::SleepingEnemyTreasure;
    std::string_view id;
    std::string_view displayName;
    DungeonEventCategory category = DungeonEventCategory::Exploration;
    bool debugPlaceable = true;
    bool discoveryDialogue = true;
    bool stageCandidate = true;
    bool requiresUndiscoveredWarpPoint = false;
    bool highDanger = false;
    float discoveryRadiusTiles = 5.0f;
    float selfLightRadiusTiles = 4.0f;
    DungeonEventCavityProfile cavity{};
    std::string_view npcVisualId;
};

struct DungeonEventFixedPlacement {
    DungeonEventKind kind = DungeonEventKind::SleepingEnemyTreasure;
    int sectionIndex = 0;
    float sectionT = 0.5f;
};

std::span<const DungeonEventDefinition> dungeonEventDefinitions();
const DungeonEventDefinition& dungeonEventDefinition(DungeonEventKind kind);
const DungeonEventDefinition* findDungeonEventDefinitionById(std::string_view id);
std::string_view dungeonEventKindId(DungeonEventKind kind);
std::string_view dungeonEventKindDisplayName(DungeonEventKind kind);
bool dungeonEventKindFromId(std::string_view id, DungeonEventKind& outKind);
bool dungeonEventKindIsWitch(DungeonEventKind kind);
bool dungeonEventKindIsCombat(DungeonEventKind kind);
bool dungeonEventKindIsHighDanger(DungeonEventKind kind);
bool dungeonEventKindHasDiscoveryDialogue(DungeonEventKind kind);
float dungeonEventDiscoveryRadiusTiles(DungeonEventKind kind);
float dungeonEventLightRadiusTiles(DungeonEventKind kind);
DungeonEventCavityProfile dungeonEventCavityProfile(DungeonEventKind kind);
std::string_view dungeonEventNpcVisualId(DungeonEventKind kind);
std::optional<DungeonEventKind> dungeonEventKindForSpecialRoom(SpecialRoomType type, int index);
std::span<const DungeonEventKind> dungeonEventStageCandidateKinds(std::string_view stageId);
bool dungeonEventKindAllowedForStage(DungeonEventKind kind, std::string_view stageId);
std::span<const DungeonEventFixedPlacement> dungeonEventFixedPlacements(std::string_view stageId);

}
