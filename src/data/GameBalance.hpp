#pragma once

namespace majo::balance {

constexpr int ScreenWidth = 1280;
constexpr int ScreenHeight = 720;
constexpr int TileSize = 16;
constexpr int ChunkTiles = 32;
constexpr int ChunkWorldSize = TileSize * ChunkTiles;
constexpr int ActiveChunkRadius = 2;

constexpr float PlayerSpeed = 150.0f;
constexpr float PlayerRadius = 9.0f;
constexpr float LightRadius = 200.0f;
constexpr float OrbitRadius = static_cast<float>(TileSize);
constexpr float OrbitSpeed = 3.4f;
constexpr float OrbitShiftDistance = 40.0f;
constexpr float OrbitThrowCooldown = 3.0f;
constexpr float OrbitThrowSpeed = 430.0f;
constexpr float OrbitThrowDistance = 250.0f;
constexpr float OrbitThrowMaxTime = 0.75f;
constexpr float OrbitReturnSpeed = 540.0f;

constexpr int DirtHp = 2;
constexpr int RockHp = 6;
constexpr int OreHp = 8;

constexpr int MaxEnemies = 192;
constexpr float EnemySpeed = 54.0f;
constexpr float EnemyRadius = 10.0f;
constexpr int EnemyHp = 5;
constexpr int EnemyXp = 5;
constexpr int EnemyDugSpawnEvery = 10;
constexpr int EnemySoftCap = 12;

constexpr int XpBase = 12;
constexpr int XpPerLevel = 8;

}
