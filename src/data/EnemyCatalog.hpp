#pragma once

#include "data/GoogleSheetSource.hpp"
#include "data/ObjectCatalog.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

struct EnemyDefinition {
    std::string id;
    std::string name;
    std::string description;
    int hp = 1;
    int contactAttackPower = 0;
    std::string contactDamageType = "none";
    double moveSpeed = 0.0;
    double radius = 0.0;
    int xp = 0;
    std::string enemyAi;
    std::vector<std::string> enemyBehaviorIds;
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
    std::vector<std::string> capturedBehaviorIds;
    std::string note;

    bool operator==(const EnemyDefinition&) const = default;
};

struct BehaviorDefinition {
    std::string id;
    std::string displayName;
    std::string usableField;
    std::string classification;
    std::string triggerCondition;
    std::string baseProcess;
    double defaultIntervalSeconds = 0.0;
    std::string defaultProjectileId;
    std::string enemyTarget;
    std::string capturedTarget;
    std::vector<EffectSpec> enemyDefaultEffects;
    std::vector<EffectSpec> capturedDefaultEffects;
    std::vector<std::string> relatedTags;
    std::string enemyFacingControl;
    std::string capturedFacingControl;
    std::string movementControl;
    std::string duplicateRule;
    std::string implementationState;
    std::string note;

    bool operator==(const BehaviorDefinition&) const = default;
};

struct EnemyCatalog {
    std::vector<EnemyDefinition> enemies;
    std::unordered_map<std::string, EnemyDefinition> enemiesById;
    std::vector<BehaviorDefinition> behaviors;
    std::unordered_map<std::string, BehaviorDefinition> behaviorsById;
    std::vector<DbValidationIssue> validationIssues;
    std::vector<std::string> validationWarnings;

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

}
