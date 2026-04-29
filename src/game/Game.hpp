#pragma once

#include "engine/Camera.hpp"
#include "engine/FileWatcher.hpp"
#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "data/GoogleSheetSource.hpp"
#include "data/ObjectCatalog.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/DebugOverlay.hpp"
#include "game/DiggingSystem.hpp"
#include "game/EffectDispatcher.hpp"
#include "game/EffectSystem.hpp"
#include "game/EnemySystem.hpp"
#include "game/InventorySystem.hpp"
#include "game/LevelSystem.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"
#include "game/UpgradeSystem.hpp"

#include <string>

namespace majo {

class UiContext;

enum class ScreenMode {
    Playing,
    PauseMenu,
    Inventory,
    Ring,
    LevelUp
};

enum class PauseMenuPage {
    Main,
    Status,
    Items,
    Ring,
    Options,
    QuitConfirm
};

class Game {
public:
    void initialize(int width, int height);
    void resize(int width, int height);
    void update(const Input& input, const Time& time);
    void render(Renderer& renderer, const Time& time);
    bool quitRequested() const { return quitRequested_; }

private:
    void initializeWorld();
    void configureWatcher();
    void checkHotReload();
    void loadSheetSourceConfig();
    bool loadBalanceFromSources(std::string& message);
    bool loadBalanceFromDisk(std::string& message);
    bool loadObjectsFromSheet();
    void updateScreenMode(const Input& input, UiContext& ui);
    void updatePauseMenu(const Input& input, UiContext& ui);
    void choosePauseMenuItem(int item);
    void leavePausePage();
    void openRingScreen();
    void updateRingScreen(const Input& input, UiContext& ui);
    void cancelRingGrab();
    bool gameProgressPaused() const;
    void renderPauseMenu(Renderer& renderer) const;
    void renderRingScreen(Renderer& renderer) const;

    Camera camera_;
    RuntimeBalance balance_;
    GoogleSheetSourceConfig sheetSource_;
    ObjectCatalog objectCatalog_;
    FileWatcher watcher_;
    Player player_;
    TileMap tileMap_;
    SpellRingSystem spellRing_;
    DiggingSystem digging_;
    EffectDispatcher effectDispatcher_;
    EffectSystem effects_;
    EnemySystem enemies_;
    InventorySystem inventory_;
    LevelSystem levels_;
    UpgradeSystem upgrades_;
    DebugOverlay debug_;
    ScreenMode mode_ = ScreenMode::Playing;
    PauseMenuPage pausePage_ = PauseMenuPage::Main;
    int pauseMenuSelection_ = 0;
    int pauseConfirmSelection_ = 1;
    int ringSlotSelection_ = 0;
    bool ringGrabActive_ = false;
    int ringGrabOrigin_ = -1;
    SpellRingItem ringGrabbedItem_{};
    std::string ringStatus_;
    bool inventoryReturnToPause_ = false;
    bool quitRequested_ = false;
    bool debugPaused_ = false;
    float reloadNoticeTimer_ = 0.0f;
    std::string reloadNotice_;
};

}
