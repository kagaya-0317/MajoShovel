#include "game/Game.hpp"

#include <algorithm>
#include <filesystem>
#include <vector>

namespace majo {

void Game::initialize(int width, int height)
{
    camera_.setViewport(width, height);
    std::string message;
    loadBalanceFromDisk(message);
    configureWatcher();
    initializeWorld();
    reloadNotice_ = message.empty() ? "DATA LOADED" : message;
    reloadNoticeTimer_ = 2.0f;
}

void Game::initializeWorld()
{
    player_ = Player{};
    player_.position = {0.0f, 0.0f};
    player_.xpToNext = balance_.xpBase + player_.level * balance_.xpPerLevel;
    tileMap_ = TileMap{};
    orbit_ = OrbitSystem{};
    enemies_ = EnemySystem{};
    levels_ = LevelSystem{};
    upgrades_ = UpgradeSystem{};
    orbit_.initialize(balance_);
    tileMap_.updateAround(player_.position, 0.0f, balance_);
}

void Game::configureWatcher()
{
    watcher_ = FileWatcher{};
    watcher_.watchPath("data");
    watcher_.watchPath("assets");
    watcher_.reset();
}

bool Game::loadBalanceFromDisk(std::string& message)
{
    RuntimeBalance loaded;
    std::string error;
    const std::filesystem::path primary = "data/game_balance.cfg";
    if (loadRuntimeBalance(primary, loaded, error)) {
        balance_ = loaded;
        message = "DATA RELOADED";
        return true;
    }
    message = "DATA LOAD FAILED";
    return false;
}

void Game::resize(int width, int height)
{
    camera_.setViewport(width, height);
}

void Game::update(const Input& input, const Time& time)
{
    checkHotReload();
    reloadNoticeTimer_ = std::max(0.0f, reloadNoticeTimer_ - time.deltaSeconds());

    if (input.debugPressed()) {
        debug_.toggle();
    }

    upgrades_.update(input, levels_, orbit_);
    const bool paused = levels_.isChoosing();
    tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_);
    player_.update(input, camera_, tileMap_, time.deltaSeconds(), paused, balance_);
    camera_.follow(player_.position, time.deltaSeconds());
    orbit_.update(player_, input, time.deltaSeconds(), time.totalSeconds(), paused, balance_);
    tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_);
    if (!paused) {
        digging_.update(tileMap_, orbit_, time.totalSeconds());
        enemies_.spawnFromDugTiles(digging_.openedTiles(), player_.position, balance_);
    }
    enemies_.update(player_, orbit_, tileMap_, time.deltaSeconds(), time.totalSeconds(), paused, balance_);
    levels_.addXp(player_, enemies_.consumePendingXp(), balance_);
}

void Game::checkHotReload()
{
    std::string changedPath;
    if (!watcher_.poll(changedPath)) {
        return;
    }

    std::string message;
    if (loadBalanceFromDisk(message)) {
        initializeWorld();
        configureWatcher();
        reloadNotice_ = "RELOADED: " + changedPath;
    } else {
        reloadNotice_ = message;
    }
    reloadNoticeTimer_ = 3.0f;
}

void Game::render(Renderer& renderer, const Time& time)
{
    renderer.clear({5, 5, 8, 255});
    renderer.setWorldSpace(&camera_);

    std::vector<Vec2> itemLights;
    for (const auto& item : orbit_.items()) {
        if (item.type == OrbitItemType::Torch) {
            itemLights.push_back(item.worldPosition);
        }
    }
    tileMap_.render(renderer, camera_, player_.position, itemLights);

    const bool orbitCenterVisible = tileMap_.isLit(orbit_.center(), player_.position, itemLights);
    if (orbitCenterVisible) {
        renderer.drawCircle(orbit_.center(), orbit_.radius(), orbit_.state() == OrbitState::Normal ? Color{130, 125, 160, 255} : Color{220, 185, 90, 255});
    }
    if (orbit_.state() != OrbitState::Normal && orbitCenterVisible) {
        renderer.drawLine(player_.position, orbit_.center(), {150, 110, 80, 100});
    }

    renderer.fillCircle(player_.position, balance_.playerRadius, {118, 72, 168, 255});
    renderer.drawLine(player_.position, player_.position + player_.facing * 22.0f, {235, 210, 255, 255});

    for (const auto& item : orbit_.items()) {
        if (!tileMap_.isLit(item.worldPosition, player_.position, itemLights)) {
            continue;
        }
        if (item.type == OrbitItemType::Shovel) {
            renderer.fillCircle(item.worldPosition, item.hitRadius, {178, 184, 190, 255});
            renderer.drawLine(item.worldPosition, item.worldPosition + normalize(item.worldPosition - orbit_.center()) * 15.0f, {90, 96, 102, 255});
        } else {
            renderer.fillCircle(item.worldPosition, item.hitRadius, {242, 122, 25, 255});
            renderer.fillCircle(item.worldPosition + Vec2{2.0f, -2.0f}, 4.0f, {255, 238, 98, 255});
        }
    }

    enemies_.render(renderer, tileMap_, player_.position, itemLights);

    renderer.setScreenSpace();
    renderer.drawText({18.0f, static_cast<float>(camera_.height() - 34)}, "WASD MOVE  SPACE SHIFT  J/CLICK THROW  F1 DEBUG", {190, 190, 198, 255}, 2);
    if (reloadNoticeTimer_ > 0.0f) {
        renderer.fillRect({18.0f, 170.0f}, {430.0f, 26.0f}, {0, 0, 0, 180});
        renderer.drawText({26.0f, 176.0f}, reloadNotice_, {255, 235, 150, 255}, 2);
    }
    debug_.render(renderer, time, enemies_, tileMap_, orbit_, player_, balance_);
    upgrades_.render(renderer, levels_);
    renderer.present();
}

}
