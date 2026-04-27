#pragma once

#include "engine/Camera.hpp"
#include "engine/FileWatcher.hpp"
#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/DebugOverlay.hpp"
#include "game/DiggingSystem.hpp"
#include "game/EnemySystem.hpp"
#include "game/LevelSystem.hpp"
#include "game/OrbitSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"
#include "game/UpgradeSystem.hpp"

namespace majo {

class Game {
public:
    void initialize(int width, int height);
    void resize(int width, int height);
    void update(const Input& input, const Time& time);
    void render(Renderer& renderer, const Time& time);

private:
    void initializeWorld();
    void configureWatcher();
    void checkHotReload();
    bool loadBalanceFromDisk(std::string& message);

    Camera camera_;
    RuntimeBalance balance_;
    FileWatcher watcher_;
    Player player_;
    TileMap tileMap_;
    OrbitSystem orbit_;
    DiggingSystem digging_;
    EnemySystem enemies_;
    LevelSystem levels_;
    UpgradeSystem upgrades_;
    DebugOverlay debug_;
    float reloadNoticeTimer_ = 0.0f;
    std::string reloadNotice_;
};

}
