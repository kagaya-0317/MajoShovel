#pragma once

#include "engine/Math.hpp"
#include "data/EnemyCatalog.hpp"
#include "game/EntityStatus.hpp"

#include <string>
#include <vector>

namespace majo {

enum class EnemyAwarenessState {
    Unaware,
    Detected,
};

enum class EnemyAwarenessIcon {
    None,
    Exclamation,
    Question,
};

struct Enemy {
    bool active = false;
    bool isBoss = false;
    int id = 0;
    std::string enemyId;
    std::string enemyName;
    const EnemyDefinition* definition = nullptr;
    std::string behaviorId;
    std::vector<std::string> behaviorIds;
    std::string projectileId;
    std::string rangedBehaviorId;
    float projectileInterval = 0.0f;
    std::vector<EffectSpec> projectileEffects;
    std::string aiId;
    std::string unawareAiId;
    float behaviorTimer = 0.0f;
    float projectileTimer = 0.0f;
    std::vector<std::string> enemyTags;
    Vec2 position{};
    Vec2 velocity{};
    float radius = 10.0f;
    int hp = 5;
    int maxHp = 5;
    int xp = 5;
    int moneyDrop = 0;
    int contactAttackPower = 1;
    std::string contactDamageType = "physical";
    float contactTimer = 0.0f;
    float hitFlash = 0.0f;
    float facingAngle = 0.0f;
    EnemyAwarenessState awareness = EnemyAwarenessState::Unaware;
    float loseSightTimer = 0.0f;
    float visionDistance = 120.0f;
    float visionAngle = 100.0f;
    float loseSightSeconds = 1.5f;
    EnemyAwarenessIcon awarenessIcon = EnemyAwarenessIcon::None;
    float awarenessIconTimer = 0.0f;
    Vec2 aiMoveDirection{1.0f, 0.0f};
    Vec2 patrolAnchor{};
    bool patrolAnchorInitialized = false;
    float aiDecisionTimer = 0.0f;
    float aiDigTimer = 0.0f;
    float repathTimer = 0.0f;
    float spawnTimer = 0.0f;
    float spawnDuration = 0.0f;
    Vec2 knockbackVelocity{};
    float knockbackTimer = 0.0f;
    double poisonDamageAccumulator = 0.0;
    EntityStatus status;
};

}
