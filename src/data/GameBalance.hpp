#pragma once

namespace majo::balance {

constexpr int ScreenWidth = 1280;
constexpr int ScreenHeight = 720;
constexpr int TileSize = 48;
constexpr int ChunkTiles = 32;
constexpr int ChunkWorldSize = TileSize * ChunkTiles;
constexpr int ActiveChunkRadius = 2;
constexpr int MaxEffects = 1024;
constexpr int MaxProjectiles = 256;

constexpr float PlayerSpeed = 150.0f;
constexpr float PlayerRadius = 9.0f;
constexpr float PlayerLightRadius = 48.0f;
constexpr float LightRadius = 200.0f;
constexpr float SpellRingRadius = static_cast<float>(TileSize);
constexpr float SpellRingSpeed = 2.72f;
constexpr float SpellRingShiftDistance = 40.0f;
constexpr float SpellRingThrowCooldown = 3.0f;
constexpr float SpellRingThrowSpeed = 430.0f;
constexpr float SpellRingThrowDistance = 250.0f;
constexpr float SpellRingThrowMaxTime = 0.75f;
constexpr float SpellRingReturnSpeed = 540.0f;

constexpr int DirtHp = 8;
constexpr int RockHp = 24;
constexpr int OreHp = 32;

constexpr int MaxEnemies = 192;
constexpr float EnemySpeed = 54.0f;
constexpr float EnemyRadius = 10.0f;
constexpr int EnemyHp = 5;
constexpr int EnemyXp = 5;
constexpr int EnemyDugSpawnEvery = 5;
constexpr int EnemySoftCap = 12;
constexpr float EnemyMinSpawnDistance = 18.0f;
constexpr float EnemySpawnWarmup = 0.65f;
constexpr float EnemySeparationStrength = 85.0f;

constexpr int XpBase = 12;
constexpr int XpPerLevel = 8;

}
