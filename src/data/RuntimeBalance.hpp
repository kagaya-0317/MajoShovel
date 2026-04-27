#pragma once

#include "data/GameBalance.hpp"
#include <filesystem>
#include <string>
#include <string_view>

namespace majo {

struct RuntimeBalance {
    float playerSpeed = balance::PlayerSpeed;
    float playerRadius = balance::PlayerRadius;
    float lightRadius = balance::LightRadius;

    float spellRingRadius = balance::SpellRingRadius;
    float spellRingSpeed = balance::SpellRingSpeed;
    float spellRingShiftDistance = balance::SpellRingShiftDistance;
    float spellRingThrowCooldown = balance::SpellRingThrowCooldown;
    float spellRingThrowSpeed = balance::SpellRingThrowSpeed;
    float spellRingThrowDistance = balance::SpellRingThrowDistance;
    float spellRingThrowMaxTime = balance::SpellRingThrowMaxTime;
    float spellRingReturnSpeed = balance::SpellRingReturnSpeed;

    int dirtHp = balance::DirtHp;
    int rockHp = balance::RockHp;
    int oreHp = balance::OreHp;

    float enemySpeed = balance::EnemySpeed;
    float enemyRadius = balance::EnemyRadius;
    int enemyHp = balance::EnemyHp;
    int enemyXp = balance::EnemyXp;
    int enemyDugSpawnEvery = balance::EnemyDugSpawnEvery;
    int enemySoftCap = balance::EnemySoftCap;
    float enemyMinSpawnDistance = balance::EnemyMinSpawnDistance;
    float enemySpawnWarmup = balance::EnemySpawnWarmup;
    float enemySeparationStrength = balance::EnemySeparationStrength;

    int xpBase = balance::XpBase;
    int xpPerLevel = balance::XpPerLevel;

    bool operator==(const RuntimeBalance&) const = default;
};

bool loadRuntimeBalanceFromText(std::string_view text, RuntimeBalance& outBalance, std::string& outError);
bool loadRuntimeBalanceFromCsv(std::string_view csv, RuntimeBalance& outBalance, std::string& outError);
bool loadRuntimeBalance(const std::filesystem::path& path, RuntimeBalance& outBalance, std::string& outError);

}
