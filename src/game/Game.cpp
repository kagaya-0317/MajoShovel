#include "game/Game.hpp"

#include "engine/Log.hpp"
#include "engine/Ui.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

namespace {
constexpr int ShovelIconIndex = 0;
constexpr int TorchIconIndex = 1;
constexpr float IconDrawSize = 32.0f;
constexpr int PauseMenuItemCount = 5;
constexpr int RingCount = 3;
constexpr int RingSlotCount = 8;
constexpr int RingSlotColumns = 4;

UiRect pausePanelRect()
{
    return {{390.0f, 130.0f}, {500.0f, 460.0f}};
}

UiRect pauseMenuItemRect(int index)
{
    return {{450.0f, 235.0f + static_cast<float>(index) * 56.0f}, {380.0f, 44.0f}};
}

UiRect pauseBackButtonRect()
{
    return {{450.0f, 520.0f}, {180.0f, 42.0f}};
}

UiRect quitConfirmRect()
{
    return {{420.0f, 240.0f}, {440.0f, 220.0f}};
}

UiRect quitConfirmButtonRect(int index)
{
    return {{470.0f + static_cast<float>(index) * 180.0f, 380.0f}, {150.0f, 42.0f}};
}

const char* pauseMenuItemName(int index)
{
    switch (index) {
    case 0: return "ステータス";
    case 1: return "アイテム";
    case 2: return "リング";
    case 3: return "オプション";
    case 4: return "ゲーム終了";
    default: return "";
    }
}

UiRect ringPanelRect()
{
    return {{70.0f, 68.0f}, {1140.0f, 590.0f}};
}

UiRect ringTabRect(int index)
{
    return {{116.0f + static_cast<float>(index) * 174.0f, 132.0f}, {152.0f, 40.0f}};
}

UiRect ringSlotRect(int index)
{
    const int column = index % RingSlotColumns;
    const int row = index / RingSlotColumns;
    return {{118.0f + static_cast<float>(column) * 146.0f, 250.0f + static_cast<float>(row) * 92.0f}, {128.0f, 70.0f}};
}

const char* spellRingItemName(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return "スコップ";
    case SpellRingItemType::Torch: return "松明";
    case SpellRingItemType::Stone: return "石";
    case SpellRingItemType::Ore: return "鉱石";
    }
    return "不明";
}

const char* spellRingItemCategory(SpellRingItemType type)
{
    switch (type) {
    case SpellRingItemType::Shovel: return "採掘";
    case SpellRingItemType::Torch: return "照明";
    case SpellRingItemType::Stone: return "打撃";
    case SpellRingItemType::Ore: return "素材";
    }
    return "不明";
}

bool dbDebugLogEnabled()
{
#ifdef _MSC_VER
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, "MAJO_DB_DEBUG") != 0 || rawValue == nullptr) {
        return true;
    }
    const std::string value(rawValue);
    std::free(rawValue);
    const std::string_view text(value);
#else
    const char* value = std::getenv("MAJO_DB_DEBUG");
    if (value == nullptr) {
        return true;
    }
    const std::string_view text(value);
#endif
    return text != "0" && text != "false" && text != "FALSE" && text != "off" && text != "OFF";
}

bool effectDebugLogEnabled()
{
#ifdef _MSC_VER
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, "MAJO_EFFECT_DEBUG") != 0 || rawValue == nullptr) {
        return false;
    }
    const std::string value(rawValue);
    std::free(rawValue);
    const std::string_view text(value);
#else
    const char* value = std::getenv("MAJO_EFFECT_DEBUG");
    if (value == nullptr) {
        return false;
    }
    const std::string_view text(value);
#endif
    return text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON";
}

std::size_t countIssues(
    const ObjectCatalog& catalog,
    DbValidationSeverity severity,
    DbValidationCategory category)
{
    return static_cast<std::size_t>(std::count_if(
        catalog.validationIssues.begin(),
        catalog.validationIssues.end(),
        [severity, category](const DbValidationIssue& issue) {
            return issue.severity == severity && issue.category == category;
        }));
}

std::size_t countSeverity(const ObjectCatalog& catalog, DbValidationSeverity severity)
{
    return static_cast<std::size_t>(std::count_if(
        catalog.validationIssues.begin(),
        catalog.validationIssues.end(),
        [severity](const DbValidationIssue& issue) {
            return issue.severity == severity;
        }));
}

void logIssueList(
    const ObjectCatalog& catalog,
    std::string_view title,
    DbValidationSeverity severity,
    DbValidationCategory category)
{
    const std::size_t count = countIssues(catalog, severity, category);
    logError(std::string(title) + ": " + std::to_string(count));
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        if (issue.severity == severity && issue.category == category) {
            logError("  [" + std::string(dbValidationSeverityName(issue.severity)) + "] " + issue.message);
        }
    }
}

void logDbValidationReport(const ObjectCatalog& catalog)
{
    if (!dbDebugLogEnabled()) {
        return;
    }

    logError("=== Object DB validation report ===");
    logError("Objects: " + std::to_string(catalog.objects.size()));
    logError("Object IDs: " + std::to_string(catalog.objectsById.size()));
    logError("Effect codes: " + std::to_string(catalog.effectCodes.size()));
    logError("Special tags: " + std::to_string(catalog.specialTags.size()));
    logError("Errors: " + std::to_string(countSeverity(catalog, DbValidationSeverity::Error)));
    logError("Warnings: " + std::to_string(countSeverity(catalog, DbValidationSeverity::Warning)));
    logIssueList(catalog, "EffectSpec syntax errors", DbValidationSeverity::Error, DbValidationCategory::EffectSpecSyntax);
    logIssueList(catalog, "Duplicate IDs", DbValidationSeverity::Warning, DbValidationCategory::DuplicateId);
    logIssueList(catalog, "Undefined effect codes", DbValidationSeverity::Warning, DbValidationCategory::UndefinedEffectCode);
    logIssueList(catalog, "Target mismatches", DbValidationSeverity::Warning, DbValidationCategory::TargetMismatch);
    logIssueList(catalog, "Undefined special tags", DbValidationSeverity::Warning, DbValidationCategory::UndefinedSpecialTag);
    logIssueList(catalog, "Exclusive tag conflicts", DbValidationSeverity::Warning, DbValidationCategory::ExclusiveTagConflict);

    std::size_t otherCount = 0;
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        if (issue.category != DbValidationCategory::EffectSpecSyntax &&
            issue.category != DbValidationCategory::DuplicateId &&
            issue.category != DbValidationCategory::UndefinedEffectCode &&
            issue.category != DbValidationCategory::TargetMismatch &&
            issue.category != DbValidationCategory::UndefinedSpecialTag &&
            issue.category != DbValidationCategory::ExclusiveTagConflict) {
            ++otherCount;
        }
    }
    logError("Other DB validation issues: " + std::to_string(otherCount));
    for (const DbValidationIssue& issue : catalog.validationIssues) {
        if (issue.category != DbValidationCategory::EffectSpecSyntax &&
            issue.category != DbValidationCategory::DuplicateId &&
            issue.category != DbValidationCategory::UndefinedEffectCode &&
            issue.category != DbValidationCategory::TargetMismatch &&
            issue.category != DbValidationCategory::UndefinedSpecialTag &&
            issue.category != DbValidationCategory::ExclusiveTagConflict) {
            logError("  [" + std::string(dbValidationSeverityName(issue.severity)) + "][" +
                std::string(dbValidationCategoryName(issue.category)) + "] " + issue.message);
        }
    }
    logError("=== End Object DB validation report ===");
}

void logEffectDispatcherSmoke(const ObjectCatalog& catalog, const EffectDispatcher& dispatcher)
{
    if (!effectDebugLogEnabled()) {
        return;
    }

    logError("=== EffectDispatcher debug smoke ===");
    logError("Registered handlers: " + std::to_string(dispatcher.handlerCount()));
    for (const ObjectDefinition& object : catalog.objects) {
        EffectContext context;
        dispatcher.dispatchNormalEffects(object, context);
        context = EffectContext{};
        dispatcher.dispatchOrbitEffects(object, context);
    }
    logError("=== End EffectDispatcher debug smoke ===");
}
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
    reloadNotice_ = message.empty() ? "データ読込完了" : message;
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
    mode_ = ScreenMode::Playing;
    pausePage_ = PauseMenuPage::Main;
    pauseMenuSelection_ = 0;
    pauseConfirmSelection_ = 1;
    ringSlotSelection_ = 0;
    ringGrabActive_ = false;
    ringGrabOrigin_ = -1;
    ringStatus_.clear();
    inventoryReturnToPause_ = false;
    debugPaused_ = false;
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
        message = "ローカルデータ読込完了";
    } else {
        balance_ = RuntimeBalance{};
        message = "データ読込失敗";
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
        message = "シートデータ読込完了";
        return true;
    }

    std::fprintf(stderr, "Google Sheet balance load failed: %s\n", sheetError.c_str());
    if (loadedLocal) {
        message = "ローカルデータ読込完了";
        return true;
    }

    message = "シートデータ読込失敗";
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

    objectCatalog_ = std::move(loaded);
    effectDispatcher_.clearHandlers();
    effectDispatcher_.registerFoundationHandlers(objectCatalog_);
    logDbValidationReport(objectCatalog_);
    logEffectDispatcherSmoke(objectCatalog_, effectDispatcher_);
    logError("Objects sheet loaded: " + std::to_string(objectCatalog_.registry.size()) + " items");
    const std::vector<const ItemData*> weapons = objectCatalog_.registry.findByCategory("\xE6\xAD\xA6\xE5\x99\xA8");
    const std::vector<const ItemData*> consumables = objectCatalog_.registry.findByTag("consumable");
    logError("Objects registry check: weapons=" + std::to_string(weapons.size()) +
        " consumable=" + std::to_string(consumables.size()));
    logError("Effect code sheet loaded: " + std::to_string(objectCatalog_.effectCodes.size()) + " codes");
    std::size_t effectCodeLogCount = 0;
    for (const auto& [code, definition] : objectCatalog_.effectCodes) {
        if (effectCodeLogCount >= 5) {
            break;
        }
        std::ostringstream line;
        line << "  effect_code=\"" << code
            << "\" name=\"" << definition.displayName
            << "\" category=\"" << definition.category
            << "\" targets=" << definition.targets.size()
            << " status=\"" << definition.implementationState << "\"";
        logError(line.str());
        ++effectCodeLogCount;
    }
    logError("Special tag sheet loaded: " + std::to_string(objectCatalog_.specialTags.size()) + " tags");
    std::size_t specialTagLogCount = 0;
    for (const auto& [tag, definition] : objectCatalog_.specialTags) {
        if (specialTagLogCount >= 5) {
            break;
        }
        std::ostringstream line;
        line << "  special_tag=\"" << tag
            << "\" name=\"" << definition.displayName
            << "\" category=\"" << definition.category
            << "\" target_categories=" << definition.targetCategories.size()
            << " related_effect_codes=" << definition.relatedEffectCodes.size()
            << " status=\"" << definition.implementationState << "\"";
        logError(line.str());
        ++specialTagLogCount;
    }
    logError("Objects by ID map loaded: " + std::to_string(objectCatalog_.objectsById.size()) + " unique IDs");
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        std::ostringstream line;
        line << "  item id=\"" << object.id
            << "\" name=\"" << object.name
            << "\" category=\"" << object.category
            << "\" rarity=" << object.rarity
            << " price=" << object.price
            << " attack_power=" << object.attackPower
            << " damage_type=\"" << object.damageType
            << "\" dig_power=" << object.digPower
            << " durability=" << object.durability
            << " weight_kg=" << object.weightKg
            << " normal_effects=" << object.normalEffects.size()
            << " orbit_effects=" << object.orbitEffects.size()
            << " tags=" << object.tags.size();
        logError(line.str());
    }
    return true;
}

void Game::resize(int width, int height)
{
    camera_.setViewport(width, height);
}

void Game::choosePauseMenuItem(int item)
{
    switch (item) {
    case 0:
        pausePage_ = PauseMenuPage::Status;
        break;
    case 1:
        inventory_.setOpen(true);
        inventory_.cancelGrab();
        inventoryReturnToPause_ = true;
        pausePage_ = PauseMenuPage::Main;
        mode_ = ScreenMode::Inventory;
        break;
    case 2:
        openRingScreen();
        break;
    case 3:
        pausePage_ = PauseMenuPage::Options;
        break;
    case 4:
        pausePage_ = PauseMenuPage::QuitConfirm;
        pauseConfirmSelection_ = 1;
        break;
    default:
        break;
    }
}

void Game::leavePausePage()
{
    if (pausePage_ == PauseMenuPage::Main) {
        mode_ = ScreenMode::Playing;
        return;
    }

    pausePage_ = PauseMenuPage::Main;
}

void Game::openRingScreen()
{
    pausePage_ = PauseMenuPage::Main;
    mode_ = ScreenMode::Ring;
    ringSlotSelection_ = std::clamp(ringSlotSelection_, 0, RingSlotCount - 1);
    cancelRingGrab();
    ringStatus_.clear();
}

void Game::cancelRingGrab()
{
    if (!ringGrabActive_) {
        return;
    }

    auto& items = spellRing_.items();
    const int insertIndex = std::clamp(ringGrabOrigin_, 0, static_cast<int>(items.size()));
    items.insert(items.begin() + insertIndex, ringGrabbedItem_);
    ringGrabActive_ = false;
    ringGrabOrigin_ = -1;
}

void Game::updateRingScreen(const Input& input, UiContext& ui)
{
    for (int i = 0; i < RingCount; ++i) {
        if (ui.pressed(ringTabRect(i))) {
            spellRing_.switchActiveRing(i - spellRing_.activeRingIndex());
            ringStatus_.clear();
            return;
        }
    }

    for (int i = 0; i < RingSlotCount; ++i) {
        if (ui.pressed(ringSlotRect(i))) {
            ringSlotSelection_ = i;
            return;
        }
    }
    ui.block(ringPanelRect());

    const int directRing = input.shortcutSlotPressed();
    if (directRing >= 0 && directRing < RingCount) {
        spellRing_.switchActiveRing(directRing - spellRing_.activeRingIndex());
        ringStatus_.clear();
    }
    if (input.activeRingDelta() != 0) {
        spellRing_.switchActiveRing(input.activeRingDelta());
        ringStatus_.clear();
    }

    int moveX = 0;
    int moveY = 0;
    if (input.pressed(InputAction::MoveLeft) || input.shortcutCursorDelta() < 0) {
        --moveX;
    }
    if (input.pressed(InputAction::MoveRight) || input.shortcutCursorDelta() > 0) {
        ++moveX;
    }
    if (input.pressed(InputAction::MoveUp)) {
        --moveY;
    }
    if (input.pressed(InputAction::MoveDown)) {
        ++moveY;
    }
    if (moveX != 0) {
        ringSlotSelection_ = (ringSlotSelection_ + moveX + RingSlotCount) % RingSlotCount;
    }
    if (moveY != 0) {
        ringSlotSelection_ = (ringSlotSelection_ + moveY * RingSlotColumns + RingSlotCount) % RingSlotCount;
    }

    if (input.pausePressed()) {
        if (ringGrabActive_) {
            cancelRingGrab();
            ringStatus_ = "つかみ操作をキャンセルしました";
        } else {
            mode_ = ScreenMode::PauseMenu;
            pausePage_ = PauseMenuPage::Main;
        }
        return;
    }

    const bool actualRing = spellRing_.activeRingIndex() == 0;
    auto& items = spellRing_.items();
    if (!actualRing) {
        if (input.useItemPressed() || input.confirmPressed() || input.addRingPressed() || input.grabOrPlacePressed()) {
            ringStatus_ = "このリングは未実装です";
        }
        return;
    }

    if (input.addRingPressed()) {
        if (ringGrabActive_) {
            ringStatus_ = "つかみ中は外せません";
        } else if (ringSlotSelection_ < static_cast<int>(items.size()) && items.size() > 1) {
            items.erase(items.begin() + ringSlotSelection_);
            ringSlotSelection_ = std::min(ringSlotSelection_, RingSlotCount - 1);
            ringStatus_ = "選択中スロットのアイテムを外しました";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ringStatus_ = "最後の1個は外せません";
        } else {
            ringStatus_ = "空きスロットです";
        }
        return;
    }

    if (input.grabOrPlacePressed()) {
        if (ringGrabActive_) {
            const int insertIndex = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()));
            items.insert(items.begin() + insertIndex, ringGrabbedItem_);
            ringGrabActive_ = false;
            ringGrabOrigin_ = -1;
            ringStatus_ = "装着順を変更しました";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            if (items.size() <= 1) {
                ringStatus_ = "最後の1個は外せません";
            } else {
                ringGrabbedItem_ = items[ringSlotSelection_];
                ringGrabOrigin_ = ringSlotSelection_;
                items.erase(items.begin() + ringSlotSelection_);
                ringGrabActive_ = true;
                ringStatus_ = "装着アイテムをつかみました";
            }
        } else {
            ringStatus_ = "空きスロットです";
        }
        return;
    }

    if (input.useItemPressed() || input.confirmPressed()) {
        if (ringGrabActive_) {
            const int insertIndex = std::clamp(ringSlotSelection_, 0, static_cast<int>(items.size()));
            items.insert(items.begin() + insertIndex, ringGrabbedItem_);
            ringGrabActive_ = false;
            ringGrabOrigin_ = -1;
            ringStatus_ = "装着順を変更しました";
        } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
            ringStatus_ = "詳細を表示中";
        } else {
            ringStatus_ = "空きスロットです";
        }
    }
}

void Game::updatePauseMenu(const Input& input, UiContext& ui)
{
    if (input.backPressed()) {
        leavePausePage();
        return;
    }

    if (pausePage_ == PauseMenuPage::QuitConfirm) {
        if (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveDown)) {
            pauseConfirmSelection_ = 1 - pauseConfirmSelection_;
        }
        for (int i = 0; i < 2; ++i) {
            if (ui.pressed(quitConfirmButtonRect(i))) {
                pauseConfirmSelection_ = i;
                if (i == 0) {
                    quitRequested_ = true;
                } else {
                    pausePage_ = PauseMenuPage::Main;
                }
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            if (pauseConfirmSelection_ == 0) {
                quitRequested_ = true;
            } else {
                pausePage_ = PauseMenuPage::Main;
            }
            return;
        }
        ui.block(quitConfirmRect());
        return;
    }

    if (pausePage_ != PauseMenuPage::Main) {
        if (ui.pressed(pauseBackButtonRect())) {
            pausePage_ = PauseMenuPage::Main;
            return;
        }
        ui.block(pausePanelRect());
        return;
    }

    if (input.pressed(InputAction::MoveUp)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + PauseMenuItemCount - 1) % PauseMenuItemCount;
    }
    if (input.pressed(InputAction::MoveDown)) {
        pauseMenuSelection_ = (pauseMenuSelection_ + 1) % PauseMenuItemCount;
    }
    for (int i = 0; i < PauseMenuItemCount; ++i) {
        if (ui.pressed(pauseMenuItemRect(i))) {
            pauseMenuSelection_ = i;
            choosePauseMenuItem(i);
            return;
        }
    }
    if (input.confirmPressed() || input.useItemPressed()) {
        choosePauseMenuItem(pauseMenuSelection_);
        return;
    }

    ui.block(pausePanelRect());
}

void Game::updateScreenMode(const Input& input, UiContext& ui)
{
    if (levels_.isChoosing()) {
        inventory_.setOpen(false);
        mode_ = ScreenMode::LevelUp;
    } else if (mode_ == ScreenMode::LevelUp) {
        mode_ = ScreenMode::Playing;
    }

    switch (mode_) {
    case ScreenMode::Playing:
        if (input.pausePressed()) {
            mode_ = ScreenMode::PauseMenu;
            pausePage_ = PauseMenuPage::Main;
            return;
        }
        inventory_.update(input, ui, player_, spellRing_, false);
        if (inventory_.isOpen()) {
            inventoryReturnToPause_ = false;
            mode_ = ScreenMode::Inventory;
            return;
        }
        if (input.activeRingDelta() != 0) {
            spellRing_.switchActiveRing(input.activeRingDelta());
        }
        inventory_.updateShortcuts(input, player_, spellRing_);
        break;
    case ScreenMode::PauseMenu:
        updatePauseMenu(input, ui);
        break;
    case ScreenMode::Inventory:
        inventory_.updateScreen(input, ui, player_, spellRing_);
        if (!inventory_.isOpen()) {
            mode_ = inventoryReturnToPause_ ? ScreenMode::PauseMenu : ScreenMode::Playing;
            pausePage_ = PauseMenuPage::Main;
            inventoryReturnToPause_ = false;
        }
        break;
    case ScreenMode::Ring:
        updateRingScreen(input, ui);
        break;
    case ScreenMode::LevelUp:
        upgrades_.update(input, ui, levels_, spellRing_);
        if (!levels_.isChoosing()) {
            mode_ = ScreenMode::Playing;
        }
        break;
    }
}

bool Game::gameProgressPaused() const
{
    return mode_ != ScreenMode::Playing;
}

void Game::renderRingScreen(Renderer& renderer) const
{
    if (mode_ != ScreenMode::Ring) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = ringPanelRect();
    renderer.fillRect(panel.pos, panel.size, {10, 12, 18, 238});
    renderer.drawRect(panel.pos, panel.size, {184, 210, 255, 255});
    renderer.drawText(panel.pos + Vec2{42.0f, 30.0f}, "リング管理", {246, 248, 255, 255}, 4);
    renderer.drawText(panel.pos + Vec2{312.0f, 34.0f}, "Z/X・1-3 リング選択  WASD/矢印・Q/E スロット", {202, 206, 216, 255}, 2);
    renderer.drawText(panel.pos + Vec2{312.0f, 60.0f}, "F/Enter 詳細  R 外す  G つかむ/置く  Esc 戻る", {202, 206, 216, 255}, 2);

    char buffer[192];
    for (int i = 0; i < RingCount; ++i) {
        std::snprintf(buffer, sizeof(buffer), "Ring %d%s", i + 1, i == 0 ? "" : " 未実装");
        drawUiButton(renderer, ringTabRect(i), buffer, i == spellRing_.activeRingIndex());
    }

    const bool actualRing = spellRing_.activeRingIndex() == 0;
    const auto& items = spellRing_.items();

    renderer.drawText(panel.pos + Vec2{48.0f, 128.0f}, "リング情報", {246, 248, 255, 255}, 3);
    std::snprintf(buffer, sizeof(buffer), "リング番号: %d / 3", spellRing_.activeRingIndex() + 1);
    renderer.drawText(panel.pos + Vec2{48.0f, 174.0f}, buffer, {232, 235, 242, 255}, 2);
    renderer.drawText(panel.pos + Vec2{48.0f, 204.0f}, actualRing ? "リング形状: 円形 (仮)" : "リング形状: 未実装", {232, 235, 242, 255}, 2);
    if (actualRing) {
        std::snprintf(buffer, sizeof(buffer), "半径 %.0f   速度 %.2f   ずらし距離 %.0f   投げクールダウン %.2fs",
            spellRing_.radius(),
            spellRing_.effectiveAngularSpeed(),
            balance_.spellRingShiftDistance,
            balance_.spellRingThrowCooldown);
        renderer.drawText(panel.pos + Vec2{48.0f, 520.0f}, buffer, {202, 206, 216, 255}, 2);
    } else {
        renderer.drawText(panel.pos + Vec2{48.0f, 520.0f}, "半径 -   速度 -   ずらし距離 -   投げクールダウン -", {202, 206, 216, 255}, 2);
    }

    renderer.drawText(panel.pos + Vec2{48.0f, 276.0f}, "装着アイテム一覧", {246, 248, 255, 255}, 3);
    for (int i = 0; i < RingSlotCount; ++i) {
        const UiRect slot = ringSlotRect(i);
        const bool selected = i == ringSlotSelection_;
        renderer.fillRect(slot.pos, slot.size, selected ? Color{54, 56, 84, 245} : Color{22, 24, 34, 232});
        renderer.drawRect(slot.pos, slot.size, selected ? Color{255, 230, 150, 255} : Color{90, 98, 120, 255});
        std::snprintf(buffer, sizeof(buffer), "%d", i + 1);
        renderer.drawText(slot.pos + Vec2{8.0f, 8.0f}, buffer, {174, 182, 198, 255}, 2);
        if (actualRing && i < static_cast<int>(items.size())) {
            std::snprintf(buffer, sizeof(buffer), "%02d %s", i + 1, spellRingItemName(items[i].type));
            renderer.drawText(slot.pos + Vec2{24.0f, 34.0f}, buffer, {238, 240, 246, 255}, 2);
        } else {
            renderer.drawText(slot.pos + Vec2{42.0f, 34.0f}, actualRing ? "空き" : "-", {150, 154, 166, 255}, 2);
        }
    }

    const Vec2 detailPos = panel.pos + Vec2{680.0f, 126.0f};
    renderer.drawText(detailPos, "選択中アイテム詳細", {246, 248, 255, 255}, 3);
    renderer.fillRect(detailPos + Vec2{0.0f, 46.0f}, {410.0f, 310.0f}, {18, 20, 30, 232});
    renderer.drawRect(detailPos + Vec2{0.0f, 46.0f}, {410.0f, 310.0f}, {92, 104, 132, 255});

    if (!actualRing) {
        renderer.drawText(detailPos + Vec2{24.0f, 78.0f}, "このリングは未実装です。", {232, 235, 242, 255}, 2);
        renderer.drawText(detailPos + Vec2{24.0f, 116.0f}, "最大3リング前提の表示枠です。", {202, 206, 216, 255}, 2);
    } else if (ringSlotSelection_ < static_cast<int>(items.size())) {
        const SpellRingItem& item = items[ringSlotSelection_];
        std::snprintf(buffer, sizeof(buffer), "名前: %s", spellRingItemName(item.type));
        renderer.drawText(detailPos + Vec2{24.0f, 72.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "カテゴリ: %s", spellRingItemCategory(item.type));
        renderer.drawText(detailPos + Vec2{24.0f, 104.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "装着順: %d", ringSlotSelection_ + 1);
        renderer.drawText(detailPos + Vec2{24.0f, 136.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "攻撃力: %d   掘削力: %d", item.damage, item.digPower);
        renderer.drawText(detailPos + Vec2{24.0f, 168.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "当たり半径: %.1f   重さ: %.1f", item.hitRadius, item.weight);
        renderer.drawText(detailPos + Vec2{24.0f, 200.0f}, buffer, {232, 235, 242, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "命中間隔: %.2fs", item.hitInterval);
        renderer.drawText(detailPos + Vec2{24.0f, 232.0f}, buffer, {232, 235, 242, 255}, 2);
        renderer.drawText(detailPos + Vec2{24.0f, 264.0f}, "効果: 既存リング処理に接続", {202, 206, 216, 255}, 2);
    } else {
        renderer.drawText(detailPos + Vec2{24.0f, 78.0f}, "空きスロットです。", {202, 206, 216, 255}, 2);
    }

    if (ringGrabActive_) {
        std::snprintf(buffer, sizeof(buffer), "つかみ中: %s   G または F で置く / Esc でキャンセル", spellRingItemName(ringGrabbedItem_.type));
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, buffer, {255, 230, 150, 255}, 2);
    } else if (!ringStatus_.empty()) {
        renderer.drawText(panel.pos + Vec2{48.0f, 556.0f}, ringStatus_, {255, 230, 150, 255}, 2);
    }
}

void Game::update(const Input& input, const Time& time)
{
    checkHotReload();
    reloadNoticeTimer_ = std::max(0.0f, reloadNoticeTimer_ - time.deltaSeconds());

    if (input.debugPressed()) {
        debug_.toggle();
    }
    if (!debug_.visible()) {
        debugPaused_ = false;
    } else if (input.debugPausePressed()) {
        debugPaused_ = !debugPaused_;
    }
    if (debugPaused_) {
        return;
    }

    UiContext ui(input);
    const bool wasPaused = gameProgressPaused();
    updateScreenMode(input, ui);
    const bool paused = gameProgressPaused() || (wasPaused && mode_ == ScreenMode::Playing);

    if (!paused) {
        tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_);
        player_.update(input, camera_, tileMap_, time.deltaSeconds(), false, balance_);
        camera_.follow(player_.position, time.deltaSeconds());

        const SpellRingState previousSpellRingState = spellRing_.state();
        const Vec2 previousRingCenter = spellRing_.center();
        spellRing_.update(player_, input, time.deltaSeconds(), time.totalSeconds(), false, ui.pointerConsumed(), balance_);
        if (previousSpellRingState == SpellRingState::Normal && spellRing_.state() == SpellRingState::Thrown) {
            effects_.spawnThrowStart(previousRingCenter, player_.facing);
        } else if (previousSpellRingState == SpellRingState::Returning && spellRing_.state() == SpellRingState::Normal) {
            effects_.spawnReturn(spellRing_.center());
        }

        tileMap_.updateAround(player_.position, time.deltaSeconds(), balance_);
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

        enemies_.update(player_, spellRing_, tileMap_, time.deltaSeconds(), time.totalSeconds(), false, balance_);
        for (const EnemyEvent& event : enemies_.events()) {
            if (event.type == EnemyEventType::Death) {
                effects_.spawnEnemyDeath(event.position);
            } else {
                effects_.spawnEnemyHit(event.position);
            }
        }
        effects_.update(time.deltaSeconds());
        levels_.addXp(player_, enemies_.consumePendingXp(), balance_);
        if (levels_.isChoosing()) {
            mode_ = ScreenMode::LevelUp;
        }
    }
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
        reloadNotice_ = "再読込: " + changedPath;
    } else {
        reloadNotice_ = message;
    }
    reloadNoticeTimer_ = 3.0f;
}

void Game::renderPauseMenu(Renderer& renderer) const
{
    if (mode_ != ScreenMode::PauseMenu) {
        return;
    }

    renderer.setScreenSpace();
    const UiRect panel = pausePanelRect();
    renderer.fillRect(panel.pos, panel.size, {12, 10, 18, 232});
    renderer.drawRect(panel.pos, panel.size, {210, 184, 255, 255});
    renderer.drawText(panel.pos + Vec2{158.0f, 34.0f}, "PAUSED", {246, 235, 255, 255}, 4);

    char buffer[160];
    if (pausePage_ == PauseMenuPage::Main) {
        renderer.drawText(panel.pos + Vec2{78.0f, 88.0f}, "W/S または ↑↓  F/Enter 決定  Esc/右クリック 戻る", {198, 198, 206, 255}, 2);
        for (int i = 0; i < PauseMenuItemCount; ++i) {
            const bool selected = i == pauseMenuSelection_;
            drawUiButton(renderer, pauseMenuItemRect(i), pauseMenuItemName(i), selected);
        }
        return;
    }

    if (pausePage_ == PauseMenuPage::Status) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "ステータス", {246, 235, 255, 255}, 3);
        std::snprintf(buffer, sizeof(buffer), "HP %02d   Level %02d   XP %02d/%02d", player_.hp, player_.level, player_.xp, player_.xpToNext);
        renderer.drawText(panel.pos + Vec2{58.0f, 160.0f}, buffer, {230, 230, 236, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "Ring %d   Items %02d/08   Radius %.0f   Speed %.1f",
            spellRing_.activeRingIndex() + 1,
            static_cast<int>(spellRing_.items().size()),
            spellRing_.radius(),
            spellRing_.effectiveAngularSpeed());
        renderer.drawText(panel.pos + Vec2{58.0f, 202.0f}, buffer, {230, 230, 236, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Items) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "アイテム", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, "通常画面のショートカットHUDとアイテム画面で管理します。", {230, 230, 236, 255}, 2);
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, "I キーでアイテム画面を開けます。", {198, 198, 206, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Ring) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "リング", {246, 235, 255, 255}, 3);
        std::snprintf(buffer, sizeof(buffer), "アクティブリング %d", spellRing_.activeRingIndex() + 1);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, buffer, {230, 230, 236, 255}, 2);
        std::snprintf(buffer, sizeof(buffer), "装着アイテム %02d/08", static_cast<int>(spellRing_.items().size()));
        renderer.drawText(panel.pos + Vec2{58.0f, 206.0f}, buffer, {230, 230, 236, 255}, 2);
        renderer.drawText(panel.pos + Vec2{58.0f, 248.0f}, "Z/X でアクティブリング切替", {198, 198, 206, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::Options) {
        renderer.drawText(panel.pos + Vec2{48.0f, 102.0f}, "オプション", {246, 235, 255, 255}, 3);
        renderer.drawText(panel.pos + Vec2{58.0f, 164.0f}, "仮画面です。設定項目は後続実装で追加します。", {230, 230, 236, 255}, 2);
    } else if (pausePage_ == PauseMenuPage::QuitConfirm) {
        const UiRect confirm = quitConfirmRect();
        renderer.fillRect(confirm.pos, confirm.size, {8, 8, 14, 245});
        renderer.drawRect(confirm.pos, confirm.size, {255, 230, 150, 255});
        renderer.drawText(confirm.pos + Vec2{58.0f, 42.0f}, "ゲームを終了しますか？", {246, 235, 255, 255}, 3);
        renderer.drawText(confirm.pos + Vec2{62.0f, 92.0f}, "保存処理はまだ実装されていません。", {198, 198, 206, 255}, 2);
        drawUiButton(renderer, quitConfirmButtonRect(0), "終了する", pauseConfirmSelection_ == 0);
        drawUiButton(renderer, quitConfirmButtonRect(1), "キャンセル", pauseConfirmSelection_ == 1);
        return;
    }

    drawUiButton(renderer, pauseBackButtonRect(), "戻る", false);
    renderer.drawText(panel.pos + Vec2{248.0f, 532.0f}, "Esc / 右クリックで戻る", {198, 198, 206, 255}, 2);
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
    if (reloadNoticeTimer_ > 0.0f) {
        renderer.fillRect({18.0f, 170.0f}, {430.0f, 26.0f}, {0, 0, 0, 180});
        renderer.drawText({26.0f, 176.0f}, reloadNotice_, {255, 235, 150, 255}, 2);
    }
    debug_.render(renderer, time, enemies_, tileMap_, spellRing_, player_, balance_);
    if (debugPaused_) {
        renderer.fillRect({18.0f, 202.0f}, {190.0f, 28.0f}, {0, 0, 0, 190});
        renderer.drawText({28.0f, 208.0f}, "DEBUG PAUSED", {255, 230, 150, 255}, 2);
    }
    upgrades_.render(renderer, levels_);
    inventory_.render(renderer, player_, spellRing_);
    if (mode_ == ScreenMode::Playing) {
        inventory_.renderShortcutHud(renderer, spellRing_, camera_.width(), camera_.height());
    }
    renderPauseMenu(renderer);
    renderRingScreen(renderer);
    renderer.present();
}

}
