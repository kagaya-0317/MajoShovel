#include "game/Game.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace majo {

namespace {
constexpr int ShovelIconIndex = 0;
constexpr int TorchIconIndex = 1;
constexpr float IconDrawSize = 32.0f;
}

void Game::initialize(int width, int height)
{
    camera_.setViewport(width, height);
    loadSheetSourceConfig();
    std::string message;
    loadBalanceFromSources(message);
    loadObjectsFromSheet();
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
    spellRing_ = SpellRingSystem{};
    effects_ = EffectSystem{};
    enemies_ = EnemySystem{};
    inventory_ = InventorySystem{};
    levels_ = LevelSystem{};
    upgrades_ = UpgradeSystem{};
    spellRing_.initialize(balance_);
    tileMap_.updateAround(player_.position, 0.0f, balance_);
}

void Game::configureWatcher()
{
    watcher_ = FileWatcher{};
    watcher_.watchPath("data");
    watcher_.watchPath("assets");
    watcher_.reset();
}

void Game::loadSheetSourceConfig()
{
    std::string error;
    if (!loadGoogleSheetSourceConfig("data/google_sheet_source.cfg", sheetSource_, error)) {
        sheetSource_ = GoogleSheetSourceConfig{};
        std::fprintf(stderr, "Google Sheet source disabled: %s\n", error.c_str());
        return;
    }
}

bool Game::loadBalanceFromDisk(std::string& message)
{
    RuntimeBalance loaded;
    std::string error;
    const std::filesystem::path primary = "data/game_balance.cfg";
    const bool loadedLocal = loadRuntimeBalance(primary, loaded, error);
    if (loadedLocal) {
        balance_ = loaded;
        message = "LOCAL DATA LOADED";
    } else {
        balance_ = RuntimeBalance{};
        message = "DATA LOAD FAILED";
        std::fprintf(stderr, "Runtime balance load failed: %s\n", error.c_str());
    }

    return loadedLocal;
}

bool Game::loadBalanceFromSources(std::string& message)
{
    const bool loadedLocal = loadBalanceFromDisk(message);

    if (!sheetSource_.enabled) {
        return loadedLocal;
    }

    RuntimeBalance sheetBalance;
    std::string sheetError;
    if (loadRuntimeBalanceFromGoogleSheet(sheetSource_, sheetBalance, sheetError)) {
        balance_ = sheetBalance;
        message = "SHEET DATA LOADED";
        return true;
    }

    std::fprintf(stderr, "Google Sheet balance load failed: %s\n", sheetError.c_str());
    if (loadedLocal) {
        message = "LOCAL DATA LOADED";
        return true;
    }

    message = "SHEET DATA FAILED";
    return false;
}

bool Game::loadObjectsFromSheet()
{
    if (!sheetSource_.enabled) {
        objectCatalog_ = ObjectCatalog{};
        return false;
    }

    ObjectCatalog loaded;
    std::string error;
    if (!loadObjectCatalogFromGoogleSheet(sheetSource_, loaded, error)) {
        std::fprintf(stderr, "Objects sheet load failed: %s\n", error.c_str());
        return false;
    }

    objectCatalog_ = loaded;
    std::fprintf(stderr, "Objects sheet loaded: %zu rows\n", objectCatalog_.objects.size());
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        std::fprintf(
            stderr,
            "  object name=\"%s\" description=\"%s\" price=%d attack_power=%d effect=\"%s\"\n",
            object.name.c_str(),
            object.description.c_str(),
            object.price,
            object.attackPower,
            object.effect.c_str());
    }
    return true;
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

    inventory_.update(input, player_, spellRing_, levels_.isChoosing());
    upgrades_.update(input, levels_, spellRing_);
    const bool paused = levels_.isChoosing() || inventory_.isOpen();
    tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_);
    player_.update(input, camera_, tileMap_, time.deltaSeconds(), paused, balance_);
    camera_.follow(player_.position, time.deltaSeconds());
    const SpellRingState previousSpellRingState = spellRing_.state();
    const Vec2 previousRingCenter = spellRing_.center();
    spellRing_.update(player_, input, time.deltaSeconds(), time.totalSeconds(), paused, balance_);
    if (!paused && previousSpellRingState == SpellRingState::Normal && spellRing_.state() == SpellRingState::Thrown) {
        effects_.spawnThrowStart(previousRingCenter, player_.facing);
    } else if (!paused && previousSpellRingState == SpellRingState::Returning && spellRing_.state() == SpellRingState::Normal) {
        effects_.spawnReturn(spellRing_.center());
    }
    tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_);
    if (!paused) {
        digging_.update(tileMap_, spellRing_, time.totalSeconds());
        for (Vec2 tile : digging_.hitTiles()) {
            effects_.spawnDigHit(tile);
        }
        for (Vec2 tile : digging_.openedTiles()) {
            effects_.spawnTileBreak(tile);
        }
        for (const DugTile& tile : digging_.dugTiles()) {
            inventory_.addFromDugTile(tile.type);
        }
        enemies_.spawnFromDugTiles(digging_.openedTiles(), tileMap_, player_.position, balance_);
    }
    enemies_.update(player_, spellRing_, tileMap_, time.deltaSeconds(), time.totalSeconds(), paused, balance_);
    for (const EnemyEvent& event : enemies_.events()) {
        if (event.type == EnemyEventType::Death) {
            effects_.spawnEnemyDeath(event.position);
        } else {
            effects_.spawnEnemyHit(event.position);
        }
    }
    effects_.update(time.deltaSeconds());
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
    for (const auto& item : spellRing_.items()) {
        if (item.type == SpellRingItemType::Torch) {
            itemLights.push_back(item.worldPosition);
        }
    }
    tileMap_.render(renderer, camera_, player_.position, itemLights);

    const bool ringCenterVisible = tileMap_.isLit(spellRing_.center(), player_.position, itemLights);
    if (ringCenterVisible) {
        renderer.drawCircle(spellRing_.center(), spellRing_.radius(), spellRing_.state() == SpellRingState::Normal ? Color{130, 125, 160, 255} : Color{220, 185, 90, 255});
    }
    if (spellRing_.state() != SpellRingState::Normal && ringCenterVisible) {
        renderer.drawLine(player_.position, spellRing_.center(), {150, 110, 80, 100});
    }

    renderer.fillCircle(player_.position, balance_.playerRadius, {118, 72, 168, 255});
    renderer.drawLine(player_.position, player_.position + player_.facing * 22.0f, {235, 210, 255, 255});

    for (const auto& item : spellRing_.items()) {
        if (!tileMap_.isLit(item.worldPosition, player_.position, itemLights)) {
            continue;
        }
        if (item.type == SpellRingItemType::Shovel) {
            if (renderer.hasIconSheet()) {
                renderer.drawIcon(ShovelIconIndex, item.worldPosition - Vec2{IconDrawSize * 0.5f, IconDrawSize * 0.5f});
            } else {
                renderer.fillCircle(item.worldPosition, item.hitRadius, {178, 184, 190, 255});
                renderer.drawLine(item.worldPosition, item.worldPosition + normalize(item.worldPosition - spellRing_.center()) * 15.0f, {90, 96, 102, 255});
            }
        } else if (item.type == SpellRingItemType::Torch) {
            if (renderer.hasIconSheet()) {
                renderer.drawIcon(TorchIconIndex, item.worldPosition - Vec2{IconDrawSize * 0.5f, IconDrawSize * 0.5f});
            } else {
                renderer.fillCircle(item.worldPosition, item.hitRadius, {242, 122, 25, 255});
                renderer.fillCircle(item.worldPosition + Vec2{2.0f, -2.0f}, 4.0f, {255, 238, 98, 255});
            }
        } else if (item.type == SpellRingItemType::Stone) {
            renderer.fillCircle(item.worldPosition, item.hitRadius, {118, 122, 132, 255});
            renderer.drawCircle(item.worldPosition, item.hitRadius + 2.0f, {62, 64, 72, 255});
        } else {
            renderer.fillCircle(item.worldPosition, item.hitRadius, {96, 122, 210, 255});
            renderer.drawCircle(item.worldPosition, item.hitRadius + 3.0f, {160, 202, 255, 255});
        }
    }

    enemies_.render(renderer, tileMap_, player_.position, itemLights);
    effects_.render(renderer);

    renderer.setScreenSpace();
    renderer.drawText({18.0f, static_cast<float>(camera_.height() - 34)}, "WASD MOVE  SPACE RING SHIFT  J/CLICK THROW  I ITEMS  F1 DEBUG", {190, 190, 198, 255}, 2);
    if (reloadNoticeTimer_ > 0.0f) {
        renderer.fillRect({18.0f, 170.0f}, {430.0f, 26.0f}, {0, 0, 0, 180});
        renderer.drawText({26.0f, 176.0f}, reloadNotice_, {255, 235, 150, 255}, 2);
    }
    debug_.render(renderer, time, enemies_, tileMap_, spellRing_, player_, balance_);
    upgrades_.render(renderer, levels_);
    inventory_.render(renderer, player_, spellRing_);
    renderer.present();
}

}
