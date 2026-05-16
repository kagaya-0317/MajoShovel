#pragma once

#include "data/GoogleSheetSource.hpp"
#include "data/ObjectCatalog.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

struct EnemyBehaviorSpec {
    std::string trigger;
    std::string behavior;
    std::unordered_map<std::string, std::string> params;
    double intervalSeconds = 0.0;

    bool operator==(const EnemyBehaviorSpec&) const = default;
};

struct EnemyDefinition {
    std::string id;
    std::string name;
    std::string description;
    int imageNumber = 0;
    int hp = 1;
    int contactAttackPower = 0;
    std::string contactDamageType = "none";
    double moveSpeed = 0.0;
    double radius = 0.0;
    int xp = 0;
    int money = 0;
    std::string enemyAi;
    std::string unawareAiId;
    double visionDistance = 120.0;
    double visionAngle = 100.0;
    double loseSightSeconds = 1.5;
    std::string enemyBehaviorCode;
    std::string capturedBehaviorCode;
    std::vector<std::string> enemyBehaviorIds;
    std::vector<std::string> capturedBehaviorIds;
    std::vector<std::string> enemyTags;
    int captureDifficulty = 0;
    std::string capturedDescription;
    std::string capturedEffectText;
    int capturedAttackPower = 0;
    std::string capturedDamageType = "none";
    int capturedDigPower = 0;
    int capturedDurability = 0;
    double capturedWeight = 0.0;
    std::vector<EffectSpec> capturedNormalEffects;
    std::vector<EffectSpec> capturedOrbitEffects;
    std::vector<std::string> capturedTags;
    std::unordered_map<std::string, double> spawnWeights;
    std::vector<EnemyBehaviorSpec> enemyBehaviorSpecs;
    std::vector<EnemyBehaviorSpec> capturedBehaviorSpecs;
    std::string note;

    bool operator==(const EnemyDefinition&) const = default;
};

struct BehaviorDefinition {
    std::string id;
    std::string displayName;
    std::string classification;
    std::string usableField;
    std::vector<std::string> supportedTriggers;
    std::string primaryParams;
    std::string defaultParams;
    std::string defaultProjectileId;
    std::string baseProcess;
    std::vector<std::string> relatedTags;
    std::string facingControl;
    std::string movementControl;
    std::string duplicateRule;
    std::string implementationState;
    std::string note;

    // Backward compatibility fields for old behavior sheets and existing runtime usage.
    double defaultIntervalSeconds = 0.0;
    std::string enemyTarget;
    std::string capturedTarget;
    std::vector<EffectSpec> enemyDefaultEffects;
    std::vector<EffectSpec> capturedDefaultEffects;
    std::string enemyFacingControl;
    std::string capturedFacingControl;

    bool operator==(const BehaviorDefinition&) const = default;
};

struct EnemySpawnWeightLoadStats {
    std::size_t detectedColumnCount = 0;
    std::size_t weightedEnemyCount = 0;
    std::size_t warningCount = 0;

    bool operator==(const EnemySpawnWeightLoadStats&) const = default;
};

struct EnemyCatalog {
    std::vector<EnemyDefinition> enemies;
    std::unordered_map<std::string, EnemyDefinition> enemiesById;
    std::vector<BehaviorDefinition> behaviors;
    std::unordered_map<std::string, BehaviorDefinition> behaviorsById;
    std::vector<DbValidationIssue> validationIssues;
    std::vector<std::string> validationWarnings;
    EnemySpawnWeightLoadStats spawnWeightStats;

    bool operator==(const EnemyCatalog&) const = default;
};

bool parseEnemyCatalog(
    const GoogleSheetTable& enemiesTable,
    const GoogleSheetTable& behaviorsTable,
    const std::unordered_map<std::string, SpecialTagDefinition>& specialTags,
    EnemyCatalog& outCatalog,
    std::string& outError);
bool loadEnemyCatalogFromGoogleSheet(
    const GoogleSheetSourceConfig& config,
    const std::unordered_map<std::string, SpecialTagDefinition>& specialTags,
    EnemyCatalog& outCatalog,
    std::string& outError);
std::string resolveEnemySpawnWeightColumnName(std::string_view stageId, int depthRank);
double enemySpawnWeightFor(const EnemyDefinition& enemy, std::string_view stageId, int depthRank);

}
