#pragma once

#include "data/GameBalance.hpp"
#include <filesystem>
#include <string>

namespace majo {

struct RuntimeBalance {
    float playerSpeed = balance::PlayerSpeed;
    float playerRadius = balance::PlayerRadius;
    float lightRadius = balance::LightRadius;

    float orbitRadius = balance::OrbitRadius;
    float orbitSpeed = balance::OrbitSpeed;
    float orbitShiftDistance = balance::OrbitShiftDistance;
    float orbitThrowCooldown = balance::OrbitThrowCooldown;
    float orbitThrowSpeed = balance::OrbitThrowSpeed;
    float orbitThrowDistance = balance::OrbitThrowDistance;
    float orbitThrowMaxTime = balance::OrbitThrowMaxTime;
    float orbitReturnSpeed = balance::OrbitReturnSpeed;

    int dirtHp = balance::DirtHp;
    int rockHp = balance::RockHp;
    int oreHp = balance::OreHp;

    float enemySpeed = balance::EnemySpeed;
    float enemyRadius = balance::EnemyRadius;
    int enemyHp = balance::EnemyHp;
    int enemyXp = balance::EnemyXp;
    int enemyDugSpawnEvery = balance::EnemyDugSpawnEvery;
    int enemySoftCap = balance::EnemySoftCap;

    int xpBase = balance::XpBase;
    int xpPerLevel = balance::XpPerLevel;
};

bool loadRuntimeBalance(const std::filesystem::path& path, RuntimeBalance& outBalance, std::string& outError);

}
