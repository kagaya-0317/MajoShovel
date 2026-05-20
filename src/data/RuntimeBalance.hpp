#pragma once

#include "data/GameBalance.hpp"
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace majo {

struct RuntimeBalance {
    float playerSpeed = balance::PlayerSpeed;
    float playerRadius = balance::PlayerRadius;
    float playerLightRadius = balance::PlayerLightRadius;
    float lightRadius = balance::LightRadius;

    float spellRingRadius = balance::SpellRingRadius;
    float spellRingSpeed = balance::SpellRingSpeed;
    float spellRingShiftDistance = balance::SpellRingShiftDistance;
    float spellRingThrowCooldown = balance::SpellRingThrowCooldown;
    float spellRingThrowSpeed = balance::SpellRingThrowSpeed;
    float spellRingThrowDistance = balance::SpellRingThrowDistance;
    float spellRingThrowMaxTime = balance::SpellRingThrowMaxTime;
    float spellRingReturnSpeed = balance::SpellRingReturnSpeed;
    float figure8WidthMultiplier = 1.20f;
    float figure8HeightMultiplier = 0.70f;
    float figure8ShapeRotationSpeed = 0.25f;
    float cometRadiusMultiplier = 1.25f;
    float cometArcDegrees = 100.0f;
    float cometSpeedMultiplier = 1.10f;
    float cometTrailLength = 0.20f;
    float cometLaneSpacing = 10.0f;
    float cometMaxArcDegrees = 130.0f;

    int dirtHp = balance::DirtHp;
    int rockHp = balance::RockHp;
    int oreHp = balance::OreHp;

    float enemySpeed = balance::EnemySpeed;
    float enemyRadius = balance::EnemyRadius;
    int enemyHp = balance::EnemyHp;
    int enemyXp = balance::EnemyXp;
    int enemyDugSpawnEvery = balance::EnemyDugSpawnEvery;
    int enemyMinDugTiles = 11;
    int enemyGuaranteeDugTiles = 20;
    int enemySoftCap = balance::EnemySoftCap;
    float enemyMinSpawnDistance = balance::EnemyMinSpawnDistance;
    float enemySpawnWarmup = balance::EnemySpawnWarmup;
    float enemySeparationStrength = balance::EnemySeparationStrength;
    float enemyDetectedVisionMultiplier = 1.8f;
    float enemyRingSlowBiteMultiplier = 0.65f;
    float enemyRingSlowBiteDuration = 4.0f;

    int xpBase = balance::XpBase;
    int xpPerLevel = balance::XpPerLevel;
    int worldDropLimitPerStage = 300;
    float collectionPullRadiusBase = 56.0f;
    float collectionPullRadiusPerLevel = 32.0f;
    float collectionPullAcceleration = 1800.0f;
    int collectionPullLimit = 24;
    int digMoneyMinDugTiles = 6;
    int digMoneyGuaranteeDugTiles = 12;
    int digItemMinDugTiles = 10;
    int digItemGuaranteeDugTiles = 30;
    float lootMoneyChance = 0.30f;
    float lootMaterialChance = 0.30f;
    float lootStageMultiplierST = 1.0f;
    float lootStageMultiplierJK = 1.2f;
    float lootStageMultiplierSC = 1.5f;
    float lootStageMultiplierAS = 1.0f;
    std::array<float, 3> lootDepthMultiplier{1.0f, 1.3f, 1.7f};
    std::array<float, 9> lootDepthMultiplierAS{1.0f, 1.1f, 1.2f, 1.4f, 1.7f, 2.0f, 2.4f, 2.9f, 3.5f};
    float lootGradeMultiplierC = 1.0f;
    float lootGradeMultiplierR = 1.8f;
    float lootGradeMultiplierS = 3.0f;
    float crateMoneyChance = 0.30f;
    float crateBonusChance = 0.30f;
    float enemyManaDropChance = 0.10f;
    float enemyMoonFragmentChance = 0.01f;
    float bossManaDropChance = 0.80f;
    float bossMoonFragmentChance = 0.15f;
    int oreMaterialMin = 1;
    int oreMaterialMax = 3;

    bool operator==(const RuntimeBalance&) const = default;
};

bool loadRuntimeBalanceFromText(std::string_view text, RuntimeBalance& outBalance, std::string& outError);
bool loadRuntimeBalanceFromCsv(std::string_view csv, RuntimeBalance& outBalance, std::string& outError);
bool loadRuntimeBalance(const std::filesystem::path& path, RuntimeBalance& outBalance, std::string& outError);

}
