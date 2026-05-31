#include "game/GameInternal.hpp"

#include "data/GameBalance.hpp"
#include "engine/Audio.hpp"
#include "game/RingImpactSound.hpp"

#include <cmath>
#include <fstream>
#include <iterator>

namespace majo {

namespace {

constexpr float DungeonRingIntroDuration = 1.18f;
constexpr float HotReloadPollIntervalSeconds = 0.50f;
constexpr std::string_view DefaultShovelObjectId = "item_shovel";
constexpr std::string_view DefaultTorchObjectId = "item_torch";
constexpr std::string_view MagnifyingGlassObjectId = "item_magnifying_glass";
constexpr std::string_view EndingSeenFlag = "ending_seen";
constexpr std::string_view AudioBgmTitle = "bgm.title";
constexpr std::string_view AudioBgmBase = "bgm.base";
constexpr std::string_view AudioBgmDungeon = "bgm.dungeon";
constexpr std::string_view AudioSeTransition = "se.transition";
constexpr std::string_view AudioSeDigHit = "se.dig.hit";
constexpr std::string_view AudioSeDigBreak = "se.dig.break";
constexpr std::string_view AudioSeDigOreBreak = "se.dig.ore_break";
constexpr std::string_view AudioSeAttackHit = "se.attack.hit";
constexpr std::string_view AudioSePickup = "se.pickup";
constexpr std::string_view AudioSePlayerDamage = "se.player.damage";
constexpr std::string_view AudioSePlayerPinch = "se.player.pinch";
constexpr std::string_view AudioSeRingThrow = "se.ring.throw";
constexpr std::string_view AudioSeEnemyDefeat = "se.enemy.defeat";
constexpr std::string_view AudioSeEnemySpawn = "se.enemy.spawn";
constexpr std::string_view AudioSeEnemyAlert = "se.enemy.alert";
constexpr std::string_view AudioSeEnemyAttack = "se.enemy.attack";
constexpr std::string_view AudioSeEnemyShoot = "se.enemy.shoot";
constexpr std::string_view AudioSeEnemyHeal = "se.enemy.heal";
constexpr std::string_view AudioSeProjectileImpact = "se.projectile.impact";
constexpr std::string_view AudioSeRingGuard = "se.ring.guard";
constexpr std::string_view AudioSeRingReflect = "se.ring.reflect";
constexpr std::string_view AudioSeMagicCast = "se.magic.cast";
constexpr std::string_view AudioSeMagicImpact = "se.magic.impact";
constexpr std::string_view AudioSeCaptureSuccess = "se.capture.success";
constexpr std::string_view AudioSeCaptureFail = "se.capture.fail";
constexpr std::string_view AudioSeExplosion = "se.explosion";
constexpr std::string_view AudioSeDiscovery = "se.discovery";
constexpr std::string_view AudioSeUiConfirm = "se.ui.confirm";
constexpr std::string_view AudioSeUiCancel = "se.ui.cancel";
constexpr std::string_view AudioSeUiMenuOpen = "se.ui.menu_open";
constexpr std::string_view AudioSeUiTabSwitch = "se.ui.tab_switch";
constexpr std::string_view AudioSeUiBookOpen = "se.ui.book_open";
constexpr std::string_view AudioSeUiItemMove = "se.ui.item_move";
constexpr std::string_view AudioSeUiItemUse = "se.ui.item_use";
constexpr std::string_view AudioSeUiRingPlace = "se.ui.ring_place";
constexpr std::string_view AudioSeUiUpgradeSelect = "se.ui.upgrade_select";
constexpr std::string_view AudioSeLevelUpJingle = "se.level_up.jingle";
constexpr std::string_view IntroTutorialChestLootInventoryTrigger = "intro_tutorial:chest_loot_inventory";
constexpr std::string_view IntroTutorialChestLootRingTrigger = "intro_tutorial:chest_loot_ring";
constexpr std::string_view LocalObjectsSnapshotPath = "Objects_with_rotation.tsv";
constexpr std::string_view LocalEnemiesSnapshotPath = ".tmp_enemies.csv";
constexpr std::string_view LocalEnemyBehaviorsSnapshotPath = ".tmp_behaviors.csv";
constexpr float PlayerDamageVignetteStartHpRatio = 0.70f;
constexpr float PlayerDamageVignetteMaxHpRatio = 0.20f;
constexpr float PlayerDamageVignetteFlashDecayPerSecond = 3.4f;
constexpr float LevelUpPresentationMinSeconds = 1.22f;
constexpr float LevelUpPresentationSparkleIntervalSeconds = 0.22f;
constexpr float LevelUpJingleFallbackSeconds = 1.18f;

void stripUtf8Bom(std::string& text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

bool readTextFile(const std::filesystem::path& path, std::string& outText, std::string& outError)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outError = "file not found: " + path.generic_string();
        return false;
    }
    outText.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (!file.eof() && file.bad()) {
        outError = "failed to read: " + path.generic_string();
        outText.clear();
        return false;
    }
    stripUtf8Bom(outText);
    outError.clear();
    return true;
}

bool parseTsvTable(std::string_view text, GoogleSheetTable& outTable, std::string& outError)
{
    GoogleSheetTable table;
    GoogleSheetRow row;
    std::string cell;
    auto flushCell = [&]() {
        if (!cell.empty() && cell.back() == '\r') {
            cell.pop_back();
        }
        row.push_back(std::move(cell));
        cell.clear();
    };
    auto flushRow = [&]() {
        flushCell();
        bool hasContent = false;
        for (const std::string& value : row) {
            if (!value.empty()) {
                hasContent = true;
                break;
            }
        }
        if (hasContent) {
            table.rows.push_back(std::move(row));
        }
        row.clear();
    };

    for (char ch : text) {
        if (ch == '\t') {
            flushCell();
        } else if (ch == '\n') {
            flushRow();
        } else {
            cell.push_back(ch);
        }
    }
    if (!cell.empty() || !row.empty()) {
        flushRow();
    }

    outTable = std::move(table);
    outError.clear();
    return true;
}

bool loadTsvTableFromDisk(const std::filesystem::path& path, GoogleSheetTable& outTable, std::string& outError)
{
    std::string text;
    if (!readTextFile(path, text, outError)) {
        return false;
    }
    return parseTsvTable(text, outTable, outError);
}

bool loadCsvTableFromDisk(const std::filesystem::path& path, GoogleSheetTable& outTable, std::string& outError)
{
    std::string text;
    if (!readTextFile(path, text, outError)) {
        return false;
    }
    return parseGoogleSheetCsv(text, outTable, outError);
}

void synthesizeLocalObjectDefinitions(ObjectCatalog& catalog)
{
    const auto ensureEffect = [&catalog](const std::string& code) {
        if (code.empty() || code == "none" || catalog.effectCodes.find(code) != catalog.effectCodes.end()) {
            return;
        }
        EffectCodeDefinition definition;
        definition.code = code;
        definition.displayName = code;
        definition.implementationState = "local_snapshot";
        catalog.effectCodes.emplace(code, std::move(definition));
    };
    const auto ensureTag = [&catalog](const std::string& tag) {
        if (tag.empty() || catalog.specialTags.find(tag) != catalog.specialTags.end()) {
            return;
        }
        SpecialTagDefinition definition;
        definition.tag = tag;
        definition.displayName = tag;
        definition.implementationState = "local_snapshot";
        catalog.specialTags.emplace(tag, std::move(definition));
    };

    for (const ObjectDefinition& object : catalog.objects) {
        for (const EffectSpec& spec : object.normalEffects) {
            for (const std::string& effect : spec.effects) {
                ensureEffect(effect);
            }
        }
        for (const EffectSpec& spec : object.orbitEffects) {
            for (const std::string& effect : spec.effects) {
                ensureEffect(effect);
            }
        }
        for (const std::string& tag : object.tags) {
            ensureTag(tag);
        }
    }
}

bool loadLocalObjectCatalog(ObjectCatalog& outCatalog, std::string& outError)
{
    GoogleSheetTable table;
    if (!loadTsvTableFromDisk(std::filesystem::path(LocalObjectsSnapshotPath), table, outError)) {
        return false;
    }
    ObjectCatalog catalog;
    if (!parseObjectCatalog(table, catalog, outError)) {
        outCatalog = {};
        return false;
    }
    synthesizeLocalObjectDefinitions(catalog);
    outCatalog = std::move(catalog);
    outError.clear();
    return true;
}

bool loadLocalEnemyCatalog(
    const std::unordered_map<std::string, SpecialTagDefinition>& specialTags,
    EnemyCatalog& outCatalog,
    std::string& outError)
{
    GoogleSheetTable enemiesTable;
    if (!loadCsvTableFromDisk(std::filesystem::path(LocalEnemiesSnapshotPath), enemiesTable, outError)) {
        return false;
    }
    GoogleSheetTable behaviorsTable;
    if (!loadCsvTableFromDisk(std::filesystem::path(LocalEnemyBehaviorsSnapshotPath), behaviorsTable, outError)) {
        return false;
    }
    return parseEnemyCatalog(enemiesTable, behaviorsTable, specialTags, outCatalog, outError);
}

int chunkCoordForWorld(float world)
{
    return static_cast<int>(std::floor(world / static_cast<float>(balance::ChunkWorldSize)));
}

const InventoryObjectInstance* findInventoryObjectInstanceById(
    const InventorySystem& inventory,
    std::string_view instanceId)
{
    if (instanceId.empty()) {
        return nullptr;
    }
    const auto& instances = inventory.objectInstances();
    const auto it = std::find_if(
        instances.begin(),
        instances.end(),
        [instanceId](const InventoryObjectInstance& entry) {
            return entry.instance.instanceId == instanceId;
        });
    return it == instances.end() ? nullptr : &*it;
}

bool spellRingContainsInstanceId(const SpellRingSystem& spellRing, std::string_view instanceId)
{
    if (instanceId.empty()) {
        return false;
    }
    for (const auto& ringItems : spellRing.ringItems()) {
        for (const SpellRingItem& item : ringItems) {
            if (item.instanceId == instanceId) {
                return true;
            }
        }
    }
    return false;
}

bool spellRingContainsObjectId(const SpellRingSystem& spellRing, std::string_view objectId)
{
    if (objectId.empty()) {
        return false;
    }
    for (const SpellRingItem* item : spellRing.runtimeItems()) {
        if (item != nullptr && item->objectId == objectId) {
            return true;
        }
    }
    return false;
}

bool effectSpecsContainEffectForTarget(
    const std::vector<EffectSpec>& specs,
    std::string_view target,
    std::string_view effect)
{
    for (const EffectSpec& spec : specs) {
        if (spec.target != target) {
            continue;
        }
        for (const std::string& effectId : spec.effects) {
            if (effectId == effect) {
                return true;
            }
        }
    }
    return false;
}

bool objectHasCaptureNetOrbitEffect(const ItemData* item)
{
    return item != nullptr && (
        effectSpecsContainEffectForTarget(item->orbitEffects, "enemy", "capture_net") ||
        effectSpecsContainEffectForTarget(item->orbitEffects, "target", "capture_net"));
}

bool objectIdHasCaptureNetOrbitEffect(const ObjectCatalog& catalog, std::string_view objectId)
{
    if (objectId.empty()) {
        return false;
    }
    const ItemData* item = catalog.registry.findById(objectId);
    return objectHasCaptureNetOrbitEffect(item);
}

UiResultDialogLine levelUpResultTextLine(std::string text)
{
    UiResultDialogLine line;
    line.segments.push_back({std::move(text), ui::Text});
    return line;
}

UiResultDialogLine levelUpResultChangeLine(std::string prefix, std::string afterValueText)
{
    UiResultDialogLine line;
    line.segments.push_back({std::move(prefix), ui::Text});
    line.segments.push_back({std::move(afterValueText), Color{255, 230, 150, 255}});
    return line;
}

std::vector<UiResultDialogLine> levelUpResultLines(RingLevelUpgradeSelection selection, float beforeValue, float afterValue)
{
    std::vector<UiResultDialogLine> lines;
    char prefix[128];
    char after[64];
    const int displayRingIndex = std::clamp(selection.ringIndex, 0, SpellRingCount - 1) + 1;
    switch (selection.kind) {
    case RingLevelUpgradeKind::Radius:
        lines.push_back(levelUpResultTextLine("リング" + std::to_string(displayRingIndex) + "のサイズが大きくなった！"));
        std::snprintf(prefix, sizeof(prefix), "リング%d 半径: %.0f → ", displayRingIndex, beforeValue);
        std::snprintf(after, sizeof(after), "%.0f", afterValue);
        lines.push_back(levelUpResultChangeLine(prefix, after));
        break;
    case RingLevelUpgradeKind::Speed:
        lines.push_back(levelUpResultTextLine("リング" + std::to_string(displayRingIndex) + "の回転速度が速くなった！"));
        std::snprintf(prefix, sizeof(prefix), "リング%d 回転速度: %.2f → ", displayRingIndex, beforeValue);
        std::snprintf(after, sizeof(after), "%.2f", afterValue);
        lines.push_back(levelUpResultChangeLine(prefix, after));
        break;
    case RingLevelUpgradeKind::WeightLimit:
        lines.push_back(levelUpResultTextLine("リング" + std::to_string(displayRingIndex) + "の重量上限が拡張された！"));
        std::snprintf(prefix, sizeof(prefix), "リング%d 重量上限: %.1fkg → ", displayRingIndex, beforeValue);
        std::snprintf(after, sizeof(after), "%.1fkg", afterValue);
        lines.push_back(levelUpResultChangeLine(prefix, after));
        break;
    }
    return lines;
}

bool isRoguelikeStageDefinition(const StageDefinition& stage)
{
    return stage.id == "stage_04_astral_mine" ||
        stage.type == "ローグライク" ||
        stage.generationProfile == "astral_rogue";
}

bool isStageClearStoryFlag(const std::string& flag)
{
    return flag.rfind("stage_clear_", 0) == 0;
}

bool playTimeCountsForMode(ScreenMode mode)
{
    switch (mode) {
    case ScreenMode::OpeningKamishibai:
    case ScreenMode::Title:
    case ScreenMode::ObjectImageScaleEdit:
    case ScreenMode::EnemyHitboxEdit:
    case ScreenMode::AudioCueEdit:
        return false;
    default:
        return true;
    }
}

float playerDamageVignetteTarget(int hp, int maxHp)
{
    if (maxHp <= 0 || hp <= 0) {
        return 1.0f;
    }
    const float hpRatio = clamp(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f);
    const float range = std::max(0.001f, PlayerDamageVignetteStartHpRatio - PlayerDamageVignetteMaxHpRatio);
    return smoothStep01((PlayerDamageVignetteStartHpRatio - hpRatio) / range);
}

bool isPlayerPinchHp(int hp, int maxHp)
{
    if (maxHp <= 0 || hp <= 0) {
        return false;
    }
    return static_cast<float>(hp) / static_cast<float>(maxHp) < PlayerDamageVignetteStartHpRatio;
}

bool shouldPlayPlayerPinchDamageSe(int hpAfter, int maxHp, int damageTaken)
{
    if (damageTaken <= 0 || maxHp <= 0 || hpAfter <= 0) {
        return false;
    }

    const int hpBefore = std::min(maxHp, hpAfter + damageTaken);
    return isPlayerPinchHp(hpBefore, maxHp) || isPlayerPinchHp(hpAfter, maxHp);
}

}

void Game::setAudioEngine(AudioEngine* audio)
{
    audio_ = audio;
    activeAudioBgmCue_.clear();
    audioJingle_ = {};
}

void Game::setSettingsAccessors(
    std::function<GameSettings()> getter,
    std::function<void(const GameSettings&)> applier)
{
    settingsGetter_ = std::move(getter);
    settingsApplier_ = std::move(applier);
    if (settingsGetter_) {
        const GameSettings settings = settingsGetter_();
        lightweightModeActive_ = settings.performance.lightweight;
        presentationSettingsActive_ = settings.presentation;
    } else {
        lightweightModeActive_ = false;
        presentationSettingsActive_ = {};
    }
    optionsSettingsLoaded_ = false;
    operationSettingsLoaded_ = false;
}

bool Game::lightweightModeEnabled() const
{
    return lightweightModeActive_;
}

float Game::screenShakeScale() const
{
    switch (presentationSettingsActive_.screenShake) {
    case ScreenShakeSetting::Off:
        return 0.0f;
    case ScreenShakeSetting::Low:
        return 0.5f;
    case ScreenShakeSetting::Standard:
        return 1.0f;
    }
    return 1.0f;
}

void Game::addScreenShake(float amplitude, float duration)
{
    if (amplitude <= 0.0f || duration <= 0.0f || screenShakeScale() <= 0.0f) {
        return;
    }
    screenShakeAmplitude_ = std::max(screenShakeAmplitude_, amplitude);
    screenShakeDuration_ = std::max(screenShakeDuration_, duration);
    screenShakeTimer_ = std::max(screenShakeTimer_, duration);
    ++screenShakeSeed_;
}

void Game::updateScreenShake(float dt)
{
    if (screenShakeTimer_ <= 0.0f) {
        return;
    }
    screenShakeTimer_ = std::max(0.0f, screenShakeTimer_ - std::max(0.0f, dt));
    if (screenShakeTimer_ <= 0.0f) {
        screenShakeDuration_ = 0.0f;
        screenShakeAmplitude_ = 0.0f;
    }
}

Vec2 Game::screenShakeOffset(double totalSeconds) const
{
    if (screenShakeTimer_ <= 0.0f || screenShakeDuration_ <= 0.0f) {
        return {};
    }
    const float scale = screenShakeScale();
    if (scale <= 0.0f) {
        return {};
    }
    const float remaining = clamp(screenShakeTimer_ / screenShakeDuration_, 0.0f, 1.0f);
    const float amplitude = screenShakeAmplitude_ * remaining * remaining * scale;
    const float t = static_cast<float>(totalSeconds);
    const float seed = static_cast<float>(screenShakeSeed_ % 997U);
    return {
        std::sin(t * 83.0f + seed * 1.37f) * amplitude,
        std::cos(t * 97.0f + seed * 1.91f) * amplitude,
    };
}

void Game::addPlayerDamageVignetteFlash(int damageAmount)
{
    if (damageAmount <= 0 || player_.maxHp <= 0) {
        return;
    }

    const float damageRatio = static_cast<float>(damageAmount) / static_cast<float>(std::max(1, player_.maxHp));
    const float flash = std::clamp(0.22f + damageRatio * 1.35f, 0.22f, 0.82f);
    playerDamageVignetteFlash_ = std::max(playerDamageVignetteFlash_, flash);
}

void Game::updatePlayerDamageVignette(float dt)
{
    const float safeDt = std::max(0.0f, dt);
    const float target = playerDamageVignetteTarget(player_.hp, player_.maxHp);
    const float response = target > playerDamageVignetteDanger_ ? 7.5f : 2.6f;
    const float blend = 1.0f - std::exp(-response * safeDt);
    playerDamageVignetteDanger_ += (target - playerDamageVignetteDanger_) * blend;
    playerDamageVignetteDanger_ = clamp(playerDamageVignetteDanger_, 0.0f, 1.0f);

    playerDamageVignetteFlash_ = std::max(
        0.0f,
        playerDamageVignetteFlash_ - PlayerDamageVignetteFlashDecayPerSecond * safeDt);
}

void Game::setInputBindingAccessors(
    std::function<InputBindingMap()> getter,
    std::function<void(const InputBindingMap&)> applier)
{
    inputBindingGetter_ = std::move(getter);
    inputBindingApplier_ = std::move(applier);
    operationSettingsLoaded_ = false;
}

bool Game::handleEvent(const SDL_Event& event)
{
    if (handleDebugItemPickerEvent(event)) {
        return true;
    }
    if (handleObjectImageScaleEditEvent(event)) {
        return true;
    }
    if (handleEnemyHitboxEditEvent(event)) {
        return true;
    }
    if (handleOperationSettingsEvent(event)) {
        return true;
    }
    return false;
}

void Game::setAutoReloadBlocked(bool blocked)
{
    if (autoReloadBlocked_ == blocked) {
        return;
    }

    autoReloadBlocked_ = blocked;
    hotReloadPollTimer_ = 0.0f;
    if (autoReloadBlocked_) {
        watcher_ = FileWatcher{};
    } else if (hotReloadEnabled_) {
        configureWatcher();
    }
}

void Game::setHotReloadEnabled(bool enabled)
{
    if (hotReloadEnabled_ == enabled) {
        return;
    }

    hotReloadEnabled_ = enabled;
    hotReloadPollTimer_ = 0.0f;
    if (hotReloadEnabled_ && !autoReloadBlocked_) {
        configureWatcher();
    } else {
        watcher_ = FileWatcher{};
    }
}

void Game::playAudioBgm(std::string_view id, float fadeSeconds, bool restart)
{
    if (id.empty()) {
        return;
    }

    const std::string cueId(id);
    if (audioJingle_.active) {
        audioJingle_.resumeBgmCue = cueId;
        activeAudioBgmCue_.clear();
        return;
    }

    if (audio_ == nullptr) {
        return;
    }

    if (!restart && activeAudioBgmCue_ == cueId) {
        return;
    }
    audio_->playBgm(cueId, fadeSeconds, restart);
    activeAudioBgmCue_ = cueId;
}

void Game::stopAudioBgm(float fadeSeconds)
{
    if (audio_ != nullptr) {
        audio_->stopBgm(fadeSeconds);
    }
    activeAudioBgmCue_.clear();
}

void Game::playAudioSe(std::string_view id, float volumeScale, float pitchScale)
{
    if (audio_ == nullptr || id.empty()) {
        return;
    }
    audio_->playSe(id, volumeScale, pitchScale);
}

float Game::playAudioJingle(
    std::string_view id,
    float fallbackDurationSeconds,
    float bgmFadeOutSeconds,
    float bgmFadeInSeconds,
    float volumeScale,
    float pitchScale)
{
    const float safePitch = std::max(0.01f, pitchScale);
    float duration = std::max(0.05f, fallbackDurationSeconds);
    if (audio_ != nullptr && !id.empty()) {
        const float cueDuration = audio_->cueDurationSeconds(id, AudioCueType::Se);
        if (cueDuration > 0.0f) {
            duration = cueDuration / safePitch;
        }
    }

    const std::string resumeCue = audioJingle_.active
        ? audioJingle_.resumeBgmCue
        : activeAudioBgmCue_;
    stopAudioBgm(bgmFadeOutSeconds);
    audioJingle_.active = true;
    audioJingle_.remainingSeconds = duration;
    audioJingle_.resumeFadeSeconds = std::max(0.0f, bgmFadeInSeconds);
    audioJingle_.resumeBgmCue = resumeCue;
    playAudioSe(id, volumeScale, safePitch);
    return duration;
}

void Game::updateAudioJingle(float dt)
{
    if (!audioJingle_.active) {
        return;
    }

    audioJingle_.remainingSeconds -= std::max(0.0f, dt);
    if (audioJingle_.remainingSeconds > 0.0f) {
        return;
    }

    const std::string resumeCue = audioJingle_.resumeBgmCue;
    const float fadeSeconds = audioJingle_.resumeFadeSeconds;
    audioJingle_ = {};
    if (!resumeCue.empty()) {
        playAudioBgm(resumeCue, fadeSeconds, true);
    }
}

void Game::playUiSoundEvents(const UiContext& ui)
{
    if (!ui.hasSoundEvents()) {
        return;
    }
    if (ui.soundEventCount(UiSoundEvent::MenuOpen) > 0) {
        playAudioSe(AudioSeUiMenuOpen);
    }
    if (ui.soundEventCount(UiSoundEvent::TabSwitch) > 0) {
        playAudioSe(AudioSeUiTabSwitch);
    }
    if (ui.soundEventCount(UiSoundEvent::BookOpen) > 0) {
        playAudioSe(AudioSeUiBookOpen);
    }
    if (ui.soundEventCount(UiSoundEvent::ItemMove) > 0) {
        playAudioSe(AudioSeUiItemMove);
    }
    if (ui.soundEventCount(UiSoundEvent::ItemUse) > 0) {
        playAudioSe(AudioSeUiItemUse);
    }
    if (ui.soundEventCount(UiSoundEvent::RingPlace) > 0) {
        playAudioSe(AudioSeUiRingPlace);
    }
    if (ui.soundEventCount(UiSoundEvent::UpgradeSelect) > 0) {
        playAudioSe(AudioSeUiUpgradeSelect);
    }
    if (ui.soundEventCount(UiSoundEvent::Confirm) > 0) {
        playAudioSe(AudioSeUiConfirm);
    }
    if (ui.soundEventCount(UiSoundEvent::Cancel) > 0) {
        playAudioSe(AudioSeUiCancel);
    }
}

void Game::initialize(int width, int height, bool testPlayMode)
{
    beginInitialize(width, height, testPlayMode);
    while (!advanceInitialize()) {
    }
}

void Game::beginInitialize(int width, int height, bool testPlayMode)
{
    (void)width;
    (void)height;
    camera_.setViewport(balance::ScreenWidth, balance::ScreenHeight);
    testPlayMode_ = testPlayMode;
    initializeJob_ = InitializeJob{};
    initializeJob_.active = true;
    initializeJob_.allowSheetSource = testPlayMode;
    initializeJob_.step = InitializeStep::LoadSheetSourceConfig;
}

bool Game::advanceInitialize()
{
    if (!initializeJob_.active) {
        return initializeJob_.step == InitializeStep::Done;
    }

    switch (initializeJob_.step) {
    case InitializeStep::None:
        initializeJob_.step = InitializeStep::LoadSheetSourceConfig;
        break;
    case InitializeStep::LoadSheetSourceConfig:
        loadSheetSourceConfig();
        initializeJob_.step = InitializeStep::LoadBalance;
        break;
    case InitializeStep::LoadBalance:
        loadBalanceFromSources(initializeJob_.loadMessage);
        initializeJob_.step = InitializeStep::LoadObjects;
        break;
    case InitializeStep::LoadObjects:
        loadObjectsFromSheet();
        initializeJob_.step = InitializeStep::LoadStages;
        break;
    case InitializeStep::LoadStages:
        loadStagesFromSheet();
        initializeJob_.step = InitializeStep::ResolveCurrentStage;
        break;
    case InitializeStep::ResolveCurrentStage:
        resolveCurrentStageDefinition();
        initializeJob_.step = InitializeStep::LoadEnemies;
        break;
    case InitializeStep::LoadEnemies:
        loadEnemiesFromSheet();
        initializeJob_.step = InitializeStep::ConfigureWatcher;
        break;
    case InitializeStep::ConfigureWatcher:
        configureWatcher();
        initializeJob_.step = InitializeStep::ResetState;
        break;
    case InitializeStep::ResetState:
        resetWorldSimulationState();
        resetWorldUiState();
        resetWorldRunState();
        initializeJob_.step = InitializeStep::InitializeRing;
        break;
    case InitializeStep::InitializeRing:
        initializeDefaultSpellRing();
        refreshEquipmentModifiers();
        applyPermanentUpgrades();
        spellRing_.applyObjectParameters(objectCatalog_);
        spellRing_.resetBaseWeightToCurrent();
        refreshOrbitEffects();
        initializeJob_.step = InitializeStep::LoadSave;
        break;
    case InitializeStep::LoadSave:
        saveDataLoaded_ = loadSaveData();
        initializeJob_.saveDataLoaded = saveDataLoaded_;
        if (saveDataLoaded_) {
            reloadNotice_ = "セーブ読込完了";
        } else {
            reloadNotice_ = initializeJob_.loadMessage.empty() ? "データ読込完了" : initializeJob_.loadMessage;
        }
        initializeJob_.step = InitializeStep::LoadBaseEdit;
        break;
    case InitializeStep::LoadBaseEdit:
        loadBaseEditData();
        if (initializeJob_.saveDataLoaded) {
            placeBasePlayerAtHomeDoorResumePoint();
        } else {
            baseArea_ = BaseArea::Outdoor;
            basePlayerPosition_ = {640.0f, 360.0f};
            baseOutdoorPlayerPosition_ = basePlayerPosition_;
            basePlayerFacing_ = {0.0f, 1.0f};
        }
        initializeJob_.step = InitializeStep::LoadImageScale;
        break;
    case InitializeStep::LoadImageScale:
        loadObjectImageScaleData();
        setObjectImageScaleOverrides(&objectImageScaleById_);
        setWorldIconScaleOverrides(&otherImageScaleByKey_);
        initializeJob_.step = InitializeStep::LoadEnemyHitboxes;
        break;
    case InitializeStep::LoadEnemyHitboxes:
        loadEnemyHitboxData();
        enemies_.setHitboxCatalog(&enemyHitboxes_);
        initializeJob_.step = InitializeStep::LoadOpening;
        break;
    case InitializeStep::LoadOpening:
        loadOpeningKamishibaiData();
        initializeJob_.step = InitializeStep::LoadStoryEvents;
        break;
    case InitializeStep::LoadStoryEvents:
        loadStoryEvents();
        initializeJob_.step = InitializeStep::LoadOpeningMeta;
        break;
    case InitializeStep::LoadOpeningMeta:
        openingMeta_ = openingMetaSave_.load(&initializeJob_.openingMetaMessage);
        logInfo(
            "[opening] " + initializeJob_.openingMetaMessage +
            " openingEverWatched=" + (openingMeta_.openingEverWatched ? std::string("true") : std::string("false")));
        initializeJob_.step = InitializeStep::EnterInitialScreen;
        break;
    case InitializeStep::EnterInitialScreen:
        enterBase();
        startOpeningKamishibai();
        reloadNoticeTimer_ = 2.0f;
        if (sheetSource_.enabled) {
            sheetSource_.enabled = false;
            logInfo("Google Sheet source disabled after startup load; runtime reload uses local data.");
        }
        initializeJob_.step = InitializeStep::Done;
        initializeJob_.active = false;
        break;
    case InitializeStep::Done:
        initializeJob_.active = false;
        break;
    }

    return initializeJob_.step == InitializeStep::Done;
}

int Game::initializeStepCount() const
{
    return 17;
}

int Game::initializeStepIndex() const
{
    switch (initializeJob_.step) {
    case InitializeStep::None: return 0;
    case InitializeStep::LoadSheetSourceConfig: return 0;
    case InitializeStep::LoadBalance: return 1;
    case InitializeStep::LoadObjects: return 2;
    case InitializeStep::LoadStages: return 3;
    case InitializeStep::ResolveCurrentStage: return 4;
    case InitializeStep::LoadEnemies: return 5;
    case InitializeStep::ConfigureWatcher: return 6;
    case InitializeStep::ResetState: return 7;
    case InitializeStep::InitializeRing: return 8;
    case InitializeStep::LoadSave: return 9;
    case InitializeStep::LoadBaseEdit: return 10;
    case InitializeStep::LoadImageScale: return 11;
    case InitializeStep::LoadEnemyHitboxes: return 12;
    case InitializeStep::LoadOpening: return 13;
    case InitializeStep::LoadStoryEvents: return 14;
    case InitializeStep::LoadOpeningMeta: return 15;
    case InitializeStep::EnterInitialScreen: return 16;
    case InitializeStep::Done: return initializeStepCount();
    }
    return 0;
}

float Game::initializeProgress() const
{
    const int count = std::max(1, initializeStepCount());
    return std::clamp(static_cast<float>(initializeStepIndex()) / static_cast<float>(count), 0.0f, 1.0f);
}

std::string Game::initializeStatusText() const
{
    switch (initializeJob_.step) {
    case InitializeStep::None:
    case InitializeStep::LoadSheetSourceConfig:
        return "Loading data source settings";
    case InitializeStep::LoadBalance:
        return "Loading balance data";
    case InitializeStep::LoadObjects:
        return sheetSource_.enabled ? "Loading Objects sheet" : "Preparing local Objects data";
    case InitializeStep::LoadStages:
        return sheetSource_.enabled ? "Loading Stages sheet" : "Preparing local stage data";
    case InitializeStep::ResolveCurrentStage:
        return "Resolving current stage";
    case InitializeStep::LoadEnemies:
        return sheetSource_.enabled ? "Loading Enemies sheet" : "Preparing local enemy data";
    case InitializeStep::ConfigureWatcher:
        return "Configuring data watchers";
    case InitializeStep::ResetState:
        return "Resetting game state";
    case InitializeStep::InitializeRing:
        return "Preparing spell rings";
    case InitializeStep::LoadSave:
        return "Loading save data";
    case InitializeStep::LoadBaseEdit:
        return "Loading base layout";
    case InitializeStep::LoadImageScale:
        return "Loading image scale settings";
    case InitializeStep::LoadEnemyHitboxes:
        return "Loading enemy hitboxes";
    case InitializeStep::LoadOpening:
        return "Loading opening data";
    case InitializeStep::LoadStoryEvents:
        return "Loading story events";
    case InitializeStep::LoadOpeningMeta:
        return "Loading opening progress";
    case InitializeStep::EnterInitialScreen:
        return "Starting title flow";
    case InitializeStep::Done:
        return "Ready";
    }
    return "Loading";
}

void Game::initializeWorld(bool captureRunStartInventory)
{
    resetWorldSimulationState();
    resetWorldUiState();
    resetWorldRunState();
    buildWorldForRun(captureRunStartInventory);
}

void Game::resetWorldSimulationState()
{
    resetWorldPlayerState();
    resetWorldMapAndRingState();
    resetWorldActionSystems();
    resetWorldInventoryState();
}

void Game::resetWorldPlayerState()
{
    resetInPlace(player_);
    player_.position = {0.0f, 0.0f};
    player_.xpToNext = playerXpToNextForLevel(player_.level, balance_);
}

void Game::resetWorldMapAndRingState()
{
    resetInPlace(tileMap_);
    resetInPlace(spellRing_);
    resetDungeonMinimap();
}

void Game::resetWorldActionSystems()
{
    resetWorldEffectState();
    resetWorldEnemyState();
    resetWorldProjectileState();
    resetInPlace(magic_);
    resetInPlace(magicFx_);
    resetWorldDropState();
    resetWorldProgressionState();
}

void Game::resetWorldEffectState()
{
    resetInPlace(effects_);
    resetInPlace(groundLines_);
    resetInPlace(wetGround_);
    captureAbsorbAnimations_.clear();
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
}

void Game::resetWorldEnemyState()
{
    resetInPlace(enemies_);
    enemies_.setHitboxCatalog(&enemyHitboxes_);
}

void Game::resetWorldProjectileState()
{
    resetInPlace(projectiles_);
}

void Game::resetWorldDropState()
{
    resetInPlace(worldDrops_);
    worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
}

void Game::resetWorldProgressionState()
{
    resetInPlace(levels_);
    resetInPlace(upgrades_);
    levelUpPresentation_ = {};
}

void Game::resetWorldInventoryState()
{
    resetInPlace(inventory_);
}

void Game::resetWorldUiState()
{
    mode_ = ScreenMode::Playing;
    baseMenuSelection_ = 0;
    baseMiningStartChoiceActive_ = false;
    baseMiningStartSelection_ = 0;
    baseRegenerateConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    baseWarpPointSelectActive_ = false;
    baseWarpPointSelection_ = 0;
    warpReturnConfirm_ = {};
    focusedWarpReturnPointIndex_ = -1;
    resetDungeonFocus();
    baseStorageActive_ = false;
    baseStorageMode_ = StorageUiMode::Closed;
    baseStorageActionSelection_ = 0;
    baseStorageBulkSelection_ = 0;
    baseStorageDepositSource_ = static_cast<int>(BaseItemSource::Backpack);
    baseStorageDepositSourceTabs_ = {};
    baseStorageDepositSelection_ = 0;
    baseStorageWithdrawSelection_ = 0;
    baseStorageWarehousePage_ = 0;
    baseStorageQuantityDialog_ = {};
    baseStorageQuantityPending_ = {};
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    baseMerchantActionSelection_ = 0;
    baseMerchantSellSource_ = 0;
    baseMerchantSellSourceTabs_ = {};
    baseSellSelection_ = 0;
    baseMerchantBuySelection_ = 0;
    closeUiCommandMenu(baseMerchantSellCommandMenu_);
    baseMerchantSellCommandSource_ = 0;
    baseMerchantSellCommandIndex_ = -1;
    closeUiCommandMenu(baseMerchantBuyCommandMenu_);
    baseMerchantBuyCommandIndex_ = -1;
    baseUpgradeActive_ = false;
    baseUpgradeSelection_ = 0;
    baseUpgradeTabs_ = {};
    baseProcessingActive_ = false;
    baseProcessingMode_ = 0;
    baseProcessingTabs_ = {};
    baseProcessingSource_ = 0;
    baseProcessingSourceTabs_ = {};
    baseProcessingSelection_ = 0;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingCommandSlot_ = -1;
    closeUiCommandMenu(ringCommandMenu_);
    ringCommandItemIndex_ = -1;
    ringCommandPlaceActive_ = false;
    ringPlaceModeActive_ = false;
    ringEmptyPressActive_ = false;
    ringItemMoveModeActive_ = false;
    ringItemMoveIndex_ = -1;
    levelUpPresentation_ = {};
    levelUpResultDialog_ = {};
    levelUpReturnMode_ = ScreenMode::Playing;
    baseRingWorkshopActive_ = false;
    baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
    baseRingWorkshopSelection_ = 0;
    baseRingWorkshopRingIndex_ = 0;
    baseRingWorkshopRingTabs_ = {};
    ringWorkshopDraftUpgradePoints_ = levelRingUpgradePoints_;
    baseBookshelfActive_ = false;
    bookshelfPage_ = BookshelfPage::Menu;
    bookshelfSelection_ = 0;
    bookshelfScrollOffset_ = 0.0f;
    bookshelfScrollState_ = {};
    baseEditEnabled_ = false;
    baseEditMode_ = BaseEditMode::None;
    baseEditDirty_ = false;
    baseEditUndoStack_.clear();
    baseEditRedoStack_.clear();
    resetBaseEditDragState();
    objectImageScaleReturnMode_ = ScreenMode::Playing;
    imageScaleEditTab_ = ImageScaleEditTab::Objects;
    objectImageScaleAllObjectIds_.clear();
    objectImageScaleObjectIds_.clear();
    objectImageScaleSearchInput_ = {};
    objectImageScaleSelectedIndex_ = -1;
    otherImageScaleSelectedIndex_ = -1;
    objectImageScaleScrollOffset_ = 0.0f;
    otherImageScaleScrollOffset_ = 0.0f;
    objectImageScaleDirty_ = false;
    objectImageScaleStatus_.clear();
    enemyHitboxEditReturnMode_ = ScreenMode::Playing;
    enemyHitboxAllEnemyIds_.clear();
    enemyHitboxEnemyIds_.clear();
    enemyHitboxSearchInput_ = {};
    enemyHitboxSelectedEnemyIndex_ = -1;
    enemyHitboxSelectedCircleIndex_ = -1;
    enemyHitboxScrollOffset_ = 0.0f;
    enemyHitboxDirty_ = false;
    enemyHitboxDraggingCircle_ = false;
    enemyHitboxDragStartMouse_ = {};
    enemyHitboxDragStartOffset_ = {};
    enemyHitboxClipboard_.clear();
    enemyHitboxStatus_.clear();
    debugItemPickerActive_ = false;
    debugItemPickerAllObjectIds_.clear();
    debugItemPickerObjectIds_.clear();
    debugItemPickerSearchInput_ = {};
    debugItemPickerSelectedIndex_ = -1;
    debugItemPickerScrollOffset_ = 0.0f;
    debugItemPickerStatus_.clear();
    debugItemPickerCancelState_ = {};
    debugStoryTestActive_ = false;
    debugStoryTestEntries_.clear();
    debugStoryTestSelectedIndex_ = -1;
    debugStoryTestScrollOffset_ = 0.0f;
    debugStoryTestStatus_.clear();
    debugStoryTestLoadedRevision_ = -1;
    debugStoryTestReturnAfterDialogue_ = false;
    debugStoryTestCancelState_ = {};
    debugPreviewBackgroundIndex_ = 0;
    effectTestActive_ = false;
    effectTestEntries_.clear();
    effectTestVisibleEntries_.clear();
    effectTestTabKeys_.clear();
    effectTestTabLabels_.clear();
    effectTestTabsState_ = {};
    effectTestTabIndex_ = 0;
    effectTestSelectedIndex_ = 0;
    effectTestFrame_ = 0;
    effectTestScrollOffset_ = 0.0f;
    effectTestScrollState_ = {};
    effectTestEmitter_ = {};
    effectTestStatus_.clear();
    projectileTestActive_ = false;
    projectileTestEntries_.clear();
    projectileTestSelectedIndex_ = 0;
    projectileTestFrame_ = 0;
    projectileTestScrollOffset_ = 0.0f;
    projectileTestScrollState_ = {};
    projectileTestStatus_.clear();
    enemyTestActive_ = false;
    enemyTestUiVisible_ = true;
    enemyTestDropdown_ = {};
    enemyTestSelectedIndex_ = 0;
    enemyTestStatus_.clear();
    baseStatus_.clear();
}

void Game::resetWorldRunState()
{
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    pauseMenuSelection_ = 0;
    pauseQuitConfirm_ = {};
    ringTabs_ = {};
    ringSlotSelection_ = 0;
    ringItemMoveModeActive_ = false;
    ringItemMoveIndex_ = -1;
    ringGrabActive_ = false;
    ringGrabOrigin_ = -1;
    ringStatus_.clear();
    runStats_ = RunStats{};
    gameOverSelection_ = 0;
    gameOverStatus_.clear();
    bossSpawned_ = false;
    hasBossSpawnPoint_ = false;
    resetBossEncounter();
    dungeonEvents_.clear();
    stageClearSelection_ = 0;
    stageClearStatus_.clear();
    astralResult_ = AstralRunSummary{};
    astralResultSelection_ = 0;
    inventoryReturnToPause_ = false;
    debugPaused_ = false;
    digToolFailsafeSpawnCooldown_ = 0.0f;
    clampCurrentStageToSelectableStages();
    roguelikeDungeon_ = currentStageIsRoguelike();
    restoreRunStartInventoryOnDeath_ = roguelikeDungeon_;
    roguelikeCarryInRestricted_ = roguelikeDungeon_;
    roguelikeCarryOutRestricted_ = roguelikeDungeon_;
    resetAstralRunState();
}

void Game::buildWorldForRun(bool captureRunStartInventory)
{
    generateDungeonLayoutForRun();
    resetWarpPointRunState();
    initializeMoonFragmentNodesFromWarpPoints();
    initializeRewardNodesFromLayout();
    initializeChestNodesFromLayout();
    initializeCrateNodesFromLayout();
    initializeEnemyNodesFromLayout();
    initializeDungeonEventInstancesFromLayout();
    applyPlacementTerrainOverrides();
    initializeDefaultSpellRing();
    refreshEquipmentModifiers();
    applyPermanentUpgrades();
    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    // Future connection: TileMap chunk initialization will consult
    // currentStageDefinition().terrainProfile and terrainHardnessMultiplier.
    tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
    normalizeOpenBuriedPlacementNodes();
    updateDungeonMinimap(0.0);
    logDungeonGenerationAudit();
    logSpellRingShapeExtensionAudit();
    if (captureRunStartInventory) {
        captureRunStartInventoryState();
    }
}

void Game::beginWorldBuildFromBase(
    bool useLatestWarpPoint,
    InventoryCarryState retainedInventory,
    int retainedLevel,
    int retainedXp,
    int retainedXpToNext)
{
    worldBuildJob_ = WorldBuildJob{};
    worldBuildJob_.active = true;
    worldBuildJob_.useLatestWarpPoint = useLatestWarpPoint;
    worldBuildJob_.retainedInventory = std::move(retainedInventory);
    worldBuildJob_.retainedLevel = retainedLevel;
    worldBuildJob_.retainedXp = retainedXp;
    worldBuildJob_.retainedXpToNext = retainedXpToNext;
    worldBuildJob_.step = WorldBuildStep::ResetSimulation;

    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    baseMiningStartChoiceActive_ = false;
    baseWarpPointSelectActive_ = false;
    baseRegenerateConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    warpReturnConfirm_ = {};
    focusedWarpReturnPointIndex_ = -1;
    baseStatus_.clear();
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
    mode_ = ScreenMode::WorldLoading;
}

void Game::updateWorldBuild(float dt)
{
    if (!worldBuildJob_.active) {
        return;
    }

    worldBuildJob_.elapsedSeconds += dt;
    advanceWorldBuildOneStep();
}

void Game::advanceWorldBuildOneStep()
{
    switch (worldBuildJob_.step) {
    case WorldBuildStep::None:
        worldBuildJob_.step = WorldBuildStep::ResetSimulation;
        break;
    case WorldBuildStep::ResetSimulation:
        resetWorldSimulationState();
        worldBuildJob_.step = WorldBuildStep::ResetUi;
        break;
    case WorldBuildStep::ResetUi:
        resetWorldUiState();
        mode_ = ScreenMode::WorldLoading;
        pauseReturnMode_ = ScreenMode::Base;
        worldBuildJob_.step = WorldBuildStep::ResetRun;
        break;
    case WorldBuildStep::ResetRun:
        resetWorldRunState();
        worldBuildJob_.step = WorldBuildStep::GenerateLayout;
        break;
    case WorldBuildStep::GenerateLayout:
        generateDungeonLayoutForRun();
        worldBuildJob_.step = WorldBuildStep::ResetWarpPoints;
        break;
    case WorldBuildStep::ResetWarpPoints:
        resetWarpPointRunState();
        worldBuildJob_.step = WorldBuildStep::InitializeMoonFragments;
        break;
    case WorldBuildStep::InitializeMoonFragments:
        initializeMoonFragmentNodesFromWarpPoints();
        worldBuildJob_.step = WorldBuildStep::InitializeRewards;
        break;
    case WorldBuildStep::InitializeRewards:
        initializeRewardNodesFromLayout();
        worldBuildJob_.step = WorldBuildStep::InitializeChests;
        break;
    case WorldBuildStep::InitializeChests:
        initializeChestNodesFromLayout();
        worldBuildJob_.step = WorldBuildStep::InitializeCrates;
        break;
    case WorldBuildStep::InitializeCrates:
        initializeCrateNodesFromLayout();
        worldBuildJob_.step = WorldBuildStep::InitializeEnemies;
        break;
    case WorldBuildStep::InitializeEnemies:
        initializeEnemyNodesFromLayout();
        initializeDungeonEventInstancesFromLayout();
        applyPlacementTerrainOverrides();
        worldBuildJob_.step = WorldBuildStep::InitializeRing;
        break;
    case WorldBuildStep::InitializeRing:
        initializeDefaultSpellRing();
        refreshEquipmentModifiers();
        applyPermanentUpgrades();
        spellRing_.applyObjectParameters(objectCatalog_);
        spellRing_.resetBaseWeightToCurrent();
        refreshOrbitEffects();
        worldBuildJob_.step = WorldBuildStep::WarmInitialTiles;
        break;
    case WorldBuildStep::WarmInitialTiles:
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        updateDungeonMinimap(0.0);
        logDungeonGenerationAudit();
        logSpellRingShapeExtensionAudit();
        worldBuildJob_.step = WorldBuildStep::Finalize;
        break;
    case WorldBuildStep::Finalize:
        finishWorldBuild();
        break;
    case WorldBuildStep::Done:
        worldBuildJob_.active = false;
        break;
    }
}

void Game::finishWorldBuild()
{
    WorldBuildJob job = std::move(worldBuildJob_);
    worldBuildJob_ = WorldBuildJob{};

    restoreInventoryCarryState(job.retainedInventory);
    player_.level = job.retainedLevel;
    player_.xp = job.retainedXp;
    player_.xpToNext = job.retainedXpToNext;
    refreshEquipmentModifiers();
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    captureRunStartInventoryState();

    resetWarpPointRunState();
    initializeMoonFragmentNodesFromWarpPoints();
    applyPlacementTerrainOverrides();
    if (job.useLatestWarpPoint) {
        const Vec2 warpStartPosition = warpPointStartPositionForCurrentRequest();
        rebuildUnlockedWarpPointsForStart(warpStartPosition);
        player_.position = safePlayerStartPosition(warpStartPosition);
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        updateDungeonMinimap(0.0);
    }
    if (job.useLatestWarpPoint) {
        captureRetrySnapshotAtWarpPoint();
    }
    requestedWarpPointStartPosition_.reset();

    baseEditEnabled_ = false;
    baseEditMode_ = BaseEditMode::None;
    resetBaseEditDragState();
    mode_ = ScreenMode::Playing;
    playAudioBgm(AudioBgmDungeon, 0.45f);
    pauseReturnMode_ = ScreenMode::Playing;
    resetPlayerFootstepDust();
    camera_.follow(player_.position, 1.0f);
    beginDungeonRingIntro();
    maybeQueueStageStartStory();
    flushPendingDebugDungeonEventPlacement();
}

int Game::worldBuildStepIndex() const
{
    switch (worldBuildJob_.step) {
    case WorldBuildStep::None: return 0;
    case WorldBuildStep::ResetSimulation: return 0;
    case WorldBuildStep::ResetUi: return 1;
    case WorldBuildStep::ResetRun: return 2;
    case WorldBuildStep::GenerateLayout: return 3;
    case WorldBuildStep::ResetWarpPoints: return 4;
    case WorldBuildStep::InitializeMoonFragments: return 5;
    case WorldBuildStep::InitializeRewards: return 6;
    case WorldBuildStep::InitializeChests: return 7;
    case WorldBuildStep::InitializeCrates: return 8;
    case WorldBuildStep::InitializeEnemies: return 9;
    case WorldBuildStep::InitializeRing: return 10;
    case WorldBuildStep::WarmInitialTiles: return 11;
    case WorldBuildStep::Finalize: return 12;
    case WorldBuildStep::Done: return worldBuildStepCount();
    }
    return 0;
}

int Game::worldBuildStepCount() const
{
    return 13;
}

float Game::worldBuildProgress() const
{
    if (!worldBuildJob_.active) {
        return 1.0f;
    }
    return std::clamp(
        static_cast<float>(worldBuildStepIndex()) / static_cast<float>(std::max(1, worldBuildStepCount())),
        0.0f,
        1.0f);
}

std::string Game::worldBuildStatusText() const
{
    switch (worldBuildJob_.step) {
    case WorldBuildStep::None:
    case WorldBuildStep::ResetSimulation:
        return "採掘状態を初期化中";
    case WorldBuildStep::ResetUi:
        return "画面状態を整理中";
    case WorldBuildStep::ResetRun:
        return "ラン情報を初期化中";
    case WorldBuildStep::GenerateLayout:
        return "ダンジョン形状を生成中";
    case WorldBuildStep::ResetWarpPoints:
        return "ワープポイントを配置中";
    case WorldBuildStep::InitializeMoonFragments:
        return "月のカケラを配置中";
    case WorldBuildStep::InitializeRewards:
        return "報酬を配置中";
    case WorldBuildStep::InitializeChests:
        return "宝箱を配置中";
    case WorldBuildStep::InitializeCrates:
        return "木箱を配置中";
    case WorldBuildStep::InitializeEnemies:
        return "敵の配置を準備中";
    case WorldBuildStep::InitializeRing:
        return "スペルリングを準備中";
    case WorldBuildStep::WarmInitialTiles:
        return "入口を読み込み中";
    case WorldBuildStep::Finalize:
        return "採掘へ移行中";
    case WorldBuildStep::Done:
        return "完了";
    }
    return "採掘準備中";
}

void Game::enterBase()
{
    worldBuildJob_ = WorldBuildJob{};
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    closeDebugItemPicker();
    closeDebugStoryTest();
    debugStoryTestReturnAfterDialogue_ = false;
    mode_ = ScreenMode::Base;
    playAudioBgm(AudioBgmBase, 0.35f);
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
    baseMiningStartChoiceActive_ = false;
    baseWarpPointSelectActive_ = false;
    baseRegenerateConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    warpReturnConfirm_ = {};
    focusedWarpReturnPointIndex_ = -1;
    baseStorageActive_ = false;
    baseStorageMode_ = StorageUiMode::Closed;
    baseStorageActionSelection_ = 0;
    baseStorageBulkSelection_ = 0;
    baseStorageDepositSource_ = static_cast<int>(BaseItemSource::Backpack);
    baseStorageDepositSourceTabs_ = {};
    baseStorageDepositSelection_ = 0;
    baseStorageWithdrawSelection_ = 0;
    baseStorageWarehousePage_ = 0;
    baseStorageQuantityDialog_ = {};
    baseStorageQuantityPending_ = {};
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    baseMerchantSellSource_ = 0;
    baseMerchantSellSourceTabs_ = {};
    closeUiCommandMenu(baseMerchantSellCommandMenu_);
    baseMerchantSellCommandSource_ = 0;
    baseMerchantSellCommandIndex_ = -1;
    closeUiCommandMenu(baseMerchantBuyCommandMenu_);
    baseMerchantBuyCommandIndex_ = -1;
    baseUpgradeActive_ = false;
    baseUpgradeTabs_ = {};
    baseProcessingActive_ = false;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingCommandSlot_ = -1;
    baseRingWorkshopActive_ = false;
    baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
    baseRingWorkshopSelection_ = 0;
    baseRingWorkshopRingTabs_ = {};
    baseBookshelfActive_ = false;
    bookshelfScrollOffset_ = 0.0f;
    bookshelfScrollState_ = {};
    baseMenuSelection_ = std::clamp(baseMenuSelection_, 0, BaseMenuItemCount - 1);
    clearTemporaryPlayerState(true);
    resetPlayerFootstepDust();
    if (baseEditEnabled_) {
        baseEditMode_ = BaseEditMode::Facility;
        resetBaseEditDragState();
    }
}

void Game::loadOpeningKamishibaiData()
{
    KamishibaiLoader loader;
    KamishibaiLoadResult result = loader.load(openingKamishibaiDataPath());
    openingPages_ = std::move(result.pages);
    logInfo("[opening] kamishibai pages loaded: " + std::to_string(openingPages_.size()));
    for (const std::string& warning : result.warnings) {
        logWarning("[opening] " + warning);
    }
}

void Game::loadEndingKamishibaiData()
{
    KamishibaiLoader loader;
    KamishibaiLoadResult result = loader.load(endingKamishibaiDataPath());
    endingPages_ = std::move(result.pages);
    logInfo("[ending] kamishibai pages loaded: " + std::to_string(endingPages_.size()));
    for (const std::string& warning : result.warnings) {
        logWarning("[ending] " + warning);
    }
}

void Game::loadStoryEvents()
{
    StoryEventLoader loader;
    StoryEventLoadResult result = loader.loadDirectory(storyEventDataDirectory());
    storyEvents_ = std::move(result.events);
    ++storyEventsRevision_;
    logInfo("[story] events loaded: " + std::to_string(storyEvents_.size()));
    for (const std::string& warning : result.warnings) {
        logWarning("[story] " + warning);
    }
}

void Game::startOpeningKamishibai()
{
    if (openingPages_.empty()) {
        loadOpeningKamishibaiData();
    }
    openingPlayer_.start(openingPages_, openingMeta_.openingEverWatched);
    mode_ = ScreenMode::OpeningKamishibai;
    playAudioBgm(AudioBgmTitle, 0.45f);
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
}

void Game::finishOpeningKamishibai(bool completedPlayback)
{
    if (completedPlayback && !openingMeta_.openingEverWatched) {
        openingMeta_.openingEverWatched = true;
        std::string error;
        if (openingMetaSave_.save(openingMeta_, &error)) {
            logInfo("[opening] openingEverWatched saved: " + openingMetaSave_.path().generic_string());
        } else {
            logError("[opening] " + error);
        }
    }
    mode_ = ScreenMode::Title;
    playAudioBgm(AudioBgmTitle, 0.25f);
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
}

void Game::updateOpeningKamishibai(const Input& input, float dt)
{
    bool skipped = false;
    if (openingPlayer_.canSkipImmediately() &&
        (input.mouseLeftPressed() || input.confirmPressed() || input.useItemPressed())) {
        openingPlayer_.finishImmediately();
        skipped = true;
    }

    openingPlayer_.update(dt);
    if (openingPlayer_.finished()) {
        finishOpeningKamishibai(!skipped);
    }
}

void Game::startEndingKamishibai()
{
    if (endingPages_.empty()) {
        loadEndingKamishibaiData();
    }
    endingPlayer_.start(endingPages_, hasStoryFlag(EndingSeenFlag));
    mode_ = ScreenMode::EndingKamishibai;
    playAudioBgm(AudioBgmTitle, 0.65f);
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
}

void Game::finishEndingKamishibai(bool)
{
    endingKamishibaiPending_ = false;
    addStoryFlag(std::string(EndingSeenFlag));
    addStoryFlag("story_ending_main");
    addStoryFlag("story_stage_03_clear");
    requestReturnToBaseTransition(true, false);
}

void Game::updateEndingKamishibai(const Input& input, float dt)
{
    bool skipped = false;
    if (endingPlayer_.canSkipImmediately() &&
        (input.mouseLeftPressed() || input.confirmPressed() || input.useItemPressed())) {
        endingPlayer_.finishImmediately();
        skipped = true;
    }

    endingPlayer_.update(dt);
    if (endingPlayer_.finished()) {
        finishEndingKamishibai(!skipped);
    }
}

void Game::updateTitleScreen(const Input& input, UiContext& ui)
{
    if (input.mouseLeftPressed() || input.confirmPressed() || input.useItemPressed()) {
        if (input.mouseLeftPressed()) {
            ui.consumePointer();
        }
        ui.emitSound(UiSoundEvent::Confirm);
        const bool needsIntroTutorial = !hasStoryFlag(IntroTutorialCompletedFlag);
        requestScreenTransition(needsIntroTutorial
            ? ScreenTransitionTarget::TitleToIntroTutorial
            : ScreenTransitionTarget::TitleToBase);
    }
}

Game::ScreenTransitionFadeColor Game::fadeColorForScreenTransitionTarget(ScreenTransitionTarget target)
{
    switch (target) {
    case ScreenTransitionTarget::Base:
    case ScreenTransitionTarget::ReturnToBase:
    case ScreenTransitionTarget::IntroTutorialToBase:
        return ScreenTransitionFadeColor::White;
    case ScreenTransitionTarget::None:
    case ScreenTransitionTarget::TitleToBase:
    case ScreenTransitionTarget::TitleToIntroTutorial:
    case ScreenTransitionTarget::MiningStart:
    case ScreenTransitionTarget::BaseArea:
        return ScreenTransitionFadeColor::Black;
    }
    return ScreenTransitionFadeColor::Black;
}

float Game::holdSecondsForScreenTransitionTarget(ScreenTransitionTarget target)
{
    switch (target) {
    case ScreenTransitionTarget::TitleToIntroTutorial:
        return IntroTutorialStartTransitionHoldSeconds;
    case ScreenTransitionTarget::IntroTutorialToBase:
        return IntroTutorialReturnTransitionHoldSeconds;
    case ScreenTransitionTarget::None:
    case ScreenTransitionTarget::Base:
    case ScreenTransitionTarget::TitleToBase:
    case ScreenTransitionTarget::MiningStart:
    case ScreenTransitionTarget::ReturnToBase:
    case ScreenTransitionTarget::BaseArea:
        return ScreenTransitionHoldSeconds;
    }
    return ScreenTransitionHoldSeconds;
}

float Game::fadeInSecondsForScreenTransitionTarget(ScreenTransitionTarget target)
{
    switch (target) {
    case ScreenTransitionTarget::TitleToIntroTutorial:
        return IntroTutorialStartTransitionFadeInSeconds;
    case ScreenTransitionTarget::IntroTutorialToBase:
        return IntroTutorialReturnTransitionFadeInSeconds;
    case ScreenTransitionTarget::None:
    case ScreenTransitionTarget::Base:
    case ScreenTransitionTarget::TitleToBase:
    case ScreenTransitionTarget::MiningStart:
    case ScreenTransitionTarget::ReturnToBase:
    case ScreenTransitionTarget::BaseArea:
        return ScreenTransitionFadeInSeconds;
    }
    return ScreenTransitionFadeInSeconds;
}

float Game::postTransitionStoryDelaySecondsForScreenTransitionTarget(ScreenTransitionTarget target)
{
    switch (target) {
    case ScreenTransitionTarget::TitleToIntroTutorial:
        return IntroTutorialStartEventDelaySeconds;
    case ScreenTransitionTarget::IntroTutorialToBase:
        return IntroTutorialReturnBaseEventDelaySeconds;
    case ScreenTransitionTarget::None:
    case ScreenTransitionTarget::Base:
    case ScreenTransitionTarget::TitleToBase:
    case ScreenTransitionTarget::MiningStart:
    case ScreenTransitionTarget::ReturnToBase:
    case ScreenTransitionTarget::BaseArea:
        return 0.0f;
    }
    return 0.0f;
}

void Game::startScreenTransition(ScreenTransitionTarget target, ScreenTransitionPhase phase)
{
    screenTransition_.target = target;
    screenTransition_.phase = phase;
    screenTransition_.fadeColor = fadeColorForScreenTransitionTarget(target);
    screenTransition_.holdSeconds = holdSecondsForScreenTransitionTarget(target);
    screenTransition_.fadeInSeconds = fadeInSecondsForScreenTransitionTarget(target);
    screenTransition_.postTransitionStoryDelaySeconds = postTransitionStoryDelaySecondsForScreenTransitionTarget(target);
    screenTransition_.elapsed = 0.0f;
    screenTransition_.applied = false;
}

void Game::requestScreenTransition(ScreenTransitionTarget target)
{
    if (target == ScreenTransitionTarget::None || screenTransition_.active()) {
        return;
    }

    startScreenTransition(target, ScreenTransitionPhase::FadingOut);
    playAudioSe(AudioSeTransition);
}

void Game::requestMiningStartTransition(bool useLatestWarpPoint, bool forceRegenerate)
{
    if (!useLatestWarpPoint || forceRegenerate) {
        requestedWarpPointStartPosition_.reset();
    }
    if (screenTransition_.active()) {
        return;
    }

    startScreenTransition(ScreenTransitionTarget::MiningStart, ScreenTransitionPhase::FadingOut);
    screenTransition_.useLatestWarpPoint = useLatestWarpPoint;
    screenTransition_.forceRegenerate = forceRegenerate;
    playAudioSe(AudioSeTransition);
}

void Game::requestReturnToBaseTransition(bool stageCleared, bool died)
{
    if (screenTransition_.active()) {
        return;
    }

    startScreenTransition(ScreenTransitionTarget::ReturnToBase, ScreenTransitionPhase::FadingOut);
    screenTransition_.returnStageCleared = stageCleared;
    screenTransition_.returnDied = died;
    playAudioSe(AudioSeTransition);
}

void Game::requestBaseAreaCrossfade(BaseArea targetArea, Vec2 playerPosition, Vec2 playerFacing, std::string status)
{
    if (screenTransition_.active()) {
        return;
    }

    startScreenTransition(ScreenTransitionTarget::BaseArea, ScreenTransitionPhase::CrossFadeCapture);
    screenTransition_.targetBaseArea = targetArea;
    screenTransition_.targetBasePlayerPosition = playerPosition;
    screenTransition_.targetBasePlayerFacing = playerFacing;
    screenTransition_.targetBaseStatus = testPlayMode_ ? std::move(status) : std::string{};
    playAudioSe(AudioSeTransition);
}

void Game::updateScreenTransition(float dt)
{
    if (!screenTransition_.active()) {
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    switch (screenTransition_.phase) {
    case ScreenTransitionPhase::Idle:
        break;
    case ScreenTransitionPhase::CrossFadeCapture:
        break;
    case ScreenTransitionPhase::CrossFading:
        screenTransition_.elapsed += safeDt;
        if (screenTransition_.elapsed >= ScreenTransitionCrossFadeSeconds) {
            screenTransition_ = ScreenTransitionState{};
        }
        break;
    case ScreenTransitionPhase::FadingOut:
        screenTransition_.elapsed += safeDt;
        if (screenTransition_.elapsed >= ScreenTransitionFadeOutSeconds) {
            screenTransition_.elapsed = 0.0f;
            if (!screenTransition_.applied) {
                applyScreenTransitionTarget(screenTransition_.target);
                screenTransition_.applied = true;
            }
            screenTransition_.phase = ScreenTransitionPhase::Hold;
        }
        break;
    case ScreenTransitionPhase::Hold:
    {
        screenTransition_.elapsed += safeDt;
        if (screenTransition_.target == ScreenTransitionTarget::MiningStart && worldBuildActive()) {
            updateWorldBuild(safeDt);
        }
        const float holdSeconds = screenTransition_.holdSeconds > 0.0f
            ? screenTransition_.holdSeconds
            : ScreenTransitionHoldSeconds;
        if (screenTransition_.elapsed >= holdSeconds && !worldBuildActive()) {
            screenTransition_.elapsed = 0.0f;
            screenTransition_.phase = ScreenTransitionPhase::FadingIn;
        }
        break;
    }
    case ScreenTransitionPhase::FadingIn:
    {
        screenTransition_.elapsed += safeDt;
        const float fadeInSeconds = screenTransition_.fadeInSeconds > 0.0f
            ? screenTransition_.fadeInSeconds
            : ScreenTransitionFadeInSeconds;
        if (screenTransition_.elapsed >= fadeInSeconds) {
            const bool startDungeonRingIntro = dungeonRingIntroStartPending_ &&
                screenTransition_.target == ScreenTransitionTarget::MiningStart;
            const float postTransitionStoryDelaySeconds =
                screenTransition_.postTransitionStoryDelaySeconds;
            screenTransition_ = ScreenTransitionState{};
            if (startDungeonRingIntro) {
                dungeonRingIntroStartPending_ = false;
                dungeonRingIntroTimer_ = DungeonRingIntroDuration;
            }
            if (!pendingStoryTrigger_.empty()) {
                pendingStoryTriggerDelaySeconds_ = std::max(0.0f, postTransitionStoryDelaySeconds);
                queuePendingStoryTriggerIfReady();
            }
        }
        break;
    }
    }
}

void Game::applyScreenTransitionTarget(ScreenTransitionTarget target)
{
    switch (target) {
    case ScreenTransitionTarget::None:
        break;
    case ScreenTransitionTarget::Base:
        enterBase();
        break;
    case ScreenTransitionTarget::TitleToBase:
        enterBase();
        break;
    case ScreenTransitionTarget::TitleToIntroTutorial:
        startIntroTutorialDungeon();
        break;
    case ScreenTransitionTarget::MiningStart:
        startMiningFromBase(screenTransition_.useLatestWarpPoint, screenTransition_.forceRegenerate);
        break;
    case ScreenTransitionTarget::ReturnToBase:
        returnToBaseFromNormalStage(screenTransition_.returnStageCleared, screenTransition_.returnDied);
        break;
    case ScreenTransitionTarget::IntroTutorialToBase:
        completeIntroTutorialAndReturnToBase();
        break;
    case ScreenTransitionTarget::BaseArea:
        baseArea_ = screenTransition_.targetBaseArea;
        basePlayerPosition_ = screenTransition_.targetBasePlayerPosition;
        basePlayerFacing_ = screenTransition_.targetBasePlayerFacing;
        resetPlayerFootstepDust();
        baseStatus_ = std::move(screenTransition_.targetBaseStatus);
        break;
    }
}

void Game::queuePendingStoryTriggerIfReady()
{
    if (pendingStoryTrigger_.empty() || pendingStoryTriggerDelaySeconds_ > 0.0f) {
        return;
    }

    std::string trigger = std::move(pendingStoryTrigger_);
    pendingStoryTrigger_.clear();
    pendingStoryTriggerDelaySeconds_ = 0.0f;
    queueStoryEventForTrigger(std::move(trigger));
}

void Game::updatePendingStoryTriggerDelay(float dt)
{
    if (pendingStoryTrigger_.empty()) {
        pendingStoryTriggerDelaySeconds_ = 0.0f;
        return;
    }

    pendingStoryTriggerDelaySeconds_ = std::max(0.0f, pendingStoryTriggerDelaySeconds_ - std::max(0.0f, dt));
    queuePendingStoryTriggerIfReady();
}

bool Game::pendingStoryTriggerDelayActive() const
{
    return !pendingStoryTrigger_.empty() && pendingStoryTriggerDelaySeconds_ > 0.0f;
}

void Game::startMiningFromBase(bool useLatestWarpPoint, bool forceRegenerate)
{
    const bool roguelikeStage = currentStageIsRoguelike();
    if (roguelikeStage) {
        useLatestWarpPoint = false;
        forceRegenerate = true;
    }
    useLatestWarpPoint = useLatestWarpPoint && unlockedWarpPointCount_ > 0;
    if (!useLatestWarpPoint || forceRegenerate) {
        requestedWarpPointStartPosition_.reset();
    }
    InventoryCarryState retained = captureInventoryCarryState();
    int retainedLevel = player_.level;
    int retainedXp = player_.xp;
    int retainedXpToNext = player_.xpToNext;
    if (roguelikeStage) {
        retainedLevel = 1;
        retainedXp = 0;
        retainedXpToNext = playerXpToNextForLevel(retainedLevel, balance_);
    }
    const bool restoredDungeon = !forceRegenerate && restoreDungeonState(useLatestWarpPoint);
    if (!restoredDungeon) {
        if (forceRegenerate) {
            dungeonStates_.erase(currentStageId_);
            unlockedWarpPointCount_ = 0;
            hasLatestWarpPointPosition_ = false;
            latestWarpPointPosition_ = {};
        }
        beginWorldBuildFromBase(
            useLatestWarpPoint,
            std::move(retained),
            retainedLevel,
            retainedXp,
            retainedXpToNext);
        return;
    }
    restoreInventoryCarryState(retained);
    player_.level = retainedLevel;
    player_.xp = retainedXp;
    player_.xpToNext = retainedXpToNext;
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    captureRunStartInventoryState();
    if (useLatestWarpPoint) {
        const Vec2 startPosition = warpPointStartPositionForCurrentRequest();
        clearKnownWarpPointTerrain();
        player_.position = safePlayerStartPosition(startPosition);
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        updateDungeonMinimap(0.0);
        captureRetrySnapshotAtWarpPoint();
    }
    requestedWarpPointStartPosition_.reset();
    baseEditEnabled_ = false;
    baseEditMode_ = BaseEditMode::None;
    resetBaseEditDragState();
    mode_ = ScreenMode::Playing;
    playAudioBgm(AudioBgmDungeon, 0.45f);
    pauseReturnMode_ = ScreenMode::Playing;
    resetPlayerFootstepDust();
    camera_.follow(player_.position, 1.0f);
    beginDungeonRingIntro();
    maybeQueueStageStartStory();
    flushPendingDebugDungeonEventPlacement();
}

void Game::applyPermanentUpgrades()
{
    spellRing_.setEquipmentModifiers(equipmentModifiers_);
    player_.level = std::clamp(player_.level, 1, PlayerMaxLevel);
    player_.xpToNext = playerXpToNextForLevel(player_.level, balance_);
    if (playerAtMaxLevel(player_)) {
        player_.xp = 0;
    }
    player_.maxHp = playerMaxHpForLevel(player_.level) + maxHpUpgradeLevel_ * 2;
    player_.hp = std::min(player_.hp, player_.maxHp);
    player_.spellRingShiftDistanceBonus = effectiveRingShiftDistance() - balance_.spellRingShiftDistance;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const RingLevelUpgradePoints points = clampedRingLevelUpgradePoints(
            levelRingUpgradePoints_[static_cast<std::size_t>(ringIndex)]);
        spellRing_.setRadiusForRing(ringIndex, effectiveInitialRingRadiusForRing(ringIndex, points.radius));
        spellRing_.setAngularSpeedForRing(ringIndex, effectiveInitialRingSpeedForRing(ringIndex, points.speed));
        spellRing_.setMaxEquippedWeightForRing(ringIndex, effectiveInitialRingWeightLimitForRing(ringIndex, points.weightLimit));
    }
}

void Game::refreshEquipmentModifiers()
{
    EquipmentModifiers nextModifiers;
    const std::string equippedStaffId = inventory_.equippedStaffInstanceId();
    if (!equippedStaffId.empty()) {
        const InventoryObjectInstance* staffInstance = findInventoryObjectInstanceById(inventory_, equippedStaffId);
        if (staffInstance == nullptr) {
            logError("[warning] Staff equipment: instance_id=\"" + equippedStaffId +
                "\" is missing from inventory; staff unequipped");
            inventory_.clearEquippedStaff();
        } else if (staffInstance->instance.isBroken) {
            logError("[warning] Staff equipment: instance_id=\"" + equippedStaffId +
                "\" is broken; staff unequipped");
            inventory_.clearEquippedStaff();
        } else if (spellRingContainsInstanceId(spellRing_, equippedStaffId)) {
            logError("[warning] Staff equipment: instance_id=\"" + equippedStaffId +
                "\" is also mounted on a ring; staff unequipped");
            inventory_.clearEquippedStaff();
        } else {
            const ObjectDefinition* staffObject = &staffInstance->item;
            const auto objectIt = objectCatalog_.objectsById.find(staffInstance->item.id);
            if (objectIt != objectCatalog_.objectsById.end()) {
                staffObject = &objectIt->second;
            }
            if (!isStaffObject(*staffObject)) {
                logError("[warning] Staff equipment: instance_id=\"" + equippedStaffId +
                    "\" object_id=\"" + staffInstance->item.id + "\" is not category staff; staff unequipped");
                inventory_.clearEquippedStaff();
            } else {
                nextModifiers = collectStaffEquipmentModifiers(*staffObject, equippedStaffId);
            }
        }
    }

    equipmentModifiers_ = std::move(nextModifiers);
    spellRing_.setEquipmentModifiers(equipmentModifiers_);
    observedEquippedStaffInstanceId_ = inventory_.equippedStaffInstanceId();

    const std::string summary = equipmentModifiersDebugSummary(equipmentModifiers_);
    if (summary != equipmentModifierLogKey_) {
        equipmentModifierLogKey_ = summary;
        logError("Staff equipment modifiers: " + summary);
    }
}

LevelGainResult Game::gainPlayerXp(int amount)
{
    const int beforeMaxHp = player_.maxHp;
    const LevelGainResult result = levels_.addXp(player_, amount, balance_);
    if (result.levelsGained <= 0) {
        return result;
    }

    applyPermanentUpgrades();
    const int maxHpGain = player_.maxHp - beforeMaxHp;
    if (maxHpGain > 0 && player_.hp > 0) {
        const int beforeHp = player_.hp;
        player_.hp = std::min(player_.maxHp, player_.hp + maxHpGain);
        const int healed = player_.hp - beforeHp;
        if (healed > 0) {
            player_.healEvents.push_back({healed, player_.position});
        }
    }
    return result;
}

Vec2 Game::levelUpPresentationAnchor() const
{
    if (levelUpReturnMode_ == ScreenMode::Base || basePresentationActive()) {
        return basePlayerPosition_;
    }
    return player_.position;
}

void Game::startLevelUpPresentation()
{
    levelUpPresentation_ = {};
    levelUpPresentation_.active = true;
    levelUpPresentation_.durationSeconds = LevelUpPresentationMinSeconds;
    levelUpPresentation_.sparkleTimer = LevelUpPresentationSparkleIntervalSeconds;

    const Vec2 anchor = levelUpPresentationAnchor();
    effects_.spawnLevelUpPopup(anchor);
    effects_.spawnLevelUpSparkles(anchor);
    addScreenShake(2.2f, 0.18f);

    const float jingleDuration = playAudioJingle(
        AudioSeLevelUpJingle,
        LevelUpJingleFallbackSeconds,
        0.08f,
        0.26f,
        1.0f,
        1.0f);
    levelUpPresentation_.durationSeconds = std::max(levelUpPresentation_.durationSeconds, jingleDuration);
}

void Game::updateLevelUpPresentation(float dt)
{
    if (!levelUpPresentation_.active) {
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    levelUpPresentation_.elapsedSeconds += safeDt;
    levelUpPresentation_.sparkleTimer -= safeDt;
    if (levelUpPresentation_.sparkleTimer <= 0.0f) {
        effects_.spawnLevelUpSparkles(levelUpPresentationAnchor());
        levelUpPresentation_.sparkleTimer = LevelUpPresentationSparkleIntervalSeconds;
    }
    effects_.update(safeDt);

    if (levelUpPresentation_.elapsedSeconds >= levelUpPresentation_.durationSeconds) {
        levelUpPresentation_.active = false;
    }
}

void Game::openLevelUpChoice(ScreenMode returnMode)
{
    if (!levels_.isChoosing()) {
        return;
    }
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();

    levelUpReturnMode_ = returnMode == ScreenMode::Base ? ScreenMode::Base : ScreenMode::Playing;
    if (levelUpReturnMode_ == ScreenMode::Base) {
        pauseReturnMode_ = ScreenMode::Base;
        inventoryReturnToPause_ = false;
    }
    mode_ = ScreenMode::LevelUp;
    startLevelUpPresentation();
}

bool Game::applyLevelUpSelection(RingLevelUpgradeSelection selection)
{
    if (!levels_.isChoosing() || levelUpResultDialog_.open || levelUpPresentation_.active) {
        return false;
    }

    const int ringIndex = std::clamp(selection.ringIndex, 0, SpellRingCount - 1);
    if (ringIndex >= unlockedRingCount()) {
        return false;
    }
    const RingLevelUpgradeKind kind = selection.kind;
    float beforeValue = spellRing_.radiusForRing(ringIndex);
    if (kind == RingLevelUpgradeKind::Speed) {
        beforeValue = spellRing_.angularSpeedForRing(ringIndex);
    } else if (kind == RingLevelUpgradeKind::WeightLimit) {
        beforeValue = spellRing_.maxEquippedWeightForRing(ringIndex);
    }

    RingLevelUpgradePoints& points = levelRingUpgradePoints_[static_cast<std::size_t>(ringIndex)];
    ++ringLevelUpgradePointRef(points, kind);
    levels_.finishChoice();
    applyPermanentUpgrades();

    float afterValue = spellRing_.radiusForRing(ringIndex);
    if (kind == RingLevelUpgradeKind::Speed) {
        afterValue = spellRing_.angularSpeedForRing(ringIndex);
    } else if (kind == RingLevelUpgradeKind::WeightLimit) {
        afterValue = spellRing_.maxEquippedWeightForRing(ringIndex);
    }

    openUiResultDialog(
        levelUpResultDialog_,
        "レベルアップ",
        levelUpResultLines(RingLevelUpgradeSelection{ringIndex, kind}, beforeValue, afterValue));
    return true;
}

void Game::updateLevelUpScreen(const Input& input, UiContext& ui, float dt)
{
    const auto returnFromLevelUp = [this]() {
        levelUpPresentation_ = {};
        levelUpResultDialog_ = {};
        if (levelUpReturnMode_ == ScreenMode::Base) {
            mode_ = ScreenMode::Base;
            pauseReturnMode_ = ScreenMode::Base;
            inventoryReturnToPause_ = false;
            return;
        }
        mode_ = ScreenMode::Playing;
        pauseReturnMode_ = ScreenMode::Playing;
        inventoryReturnToPause_ = false;
    };

    if (levelUpPresentation_.active) {
        updateLevelUpPresentation(dt);
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        return;
    }

    effects_.update(std::max(0.0f, dt));

    if (levelUpResultDialog_.open) {
        const UiRect resultPanel = levelUpResultDialogRect();
        if (updateUiResultDialog(levelUpResultDialog_, ui, input, resultPanel) && !levels_.isChoosing()) {
            returnFromLevelUp();
        }
        ui.block(resultPanel);
        return;
    }

    if (!levels_.isChoosing()) {
        returnFromLevelUp();
        return;
    }

    const std::optional<RingLevelUpgradeSelection> selection = upgrades_.update(
        input,
        ui,
        spellRing_,
        dt,
        unlockedRingCount());
    if (!selection) {
        return;
    }

    applyLevelUpSelection(*selection);
}

float Game::effectiveInitialRingRadiusForRing(int ringIndex, int levelRadiusPoints) const
{
    const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringRadiusUpgradeLevel_) * 0.08f;
    const float workshopMultiplier = 1.0f + static_cast<float>(workshopInitialRadiusLevel_) * 0.05f;
    const float levelMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(levelRadiusPoints);
    const double staffMultiplier = std::max(
        0.0,
        ringEquipmentModifiersForRing(equipmentModifiers_, ringIndex).ringRadiusMul);
    return balance_.spellRingRadius *
        baseUpgradeMultiplier *
        workshopMultiplier *
        levelMultiplier *
        static_cast<float>(staffMultiplier);
}

float Game::effectiveInitialRingSpeedForRing(int ringIndex, int levelSpeedPoints) const
{
    const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringSpeedUpgradeLevel_) * 0.08f;
    const float workshopMultiplier = 1.0f + static_cast<float>(workshopInitialSpeedLevel_) * 0.05f;
    const float levelMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(levelSpeedPoints);
    const double staffMultiplier = std::max(
        0.0,
        ringEquipmentModifiersForRing(equipmentModifiers_, ringIndex).ringSpeedMul);
    return balance_.spellRingSpeed *
        baseUpgradeMultiplier *
        workshopMultiplier *
        levelMultiplier *
        static_cast<float>(staffMultiplier);
}

float Game::effectiveInitialRingWeightLimitForRing(int ringIndex, int levelWeightLimitPoints) const
{
    const double staffWeightAdd = std::max(
        0.0,
        ringEquipmentModifiersForRing(equipmentModifiers_, ringIndex).ringWeightLimitAdd);
    return SpellRingSystem::InitialMaxEquippedWeight +
        SpellRingSystem::LevelWeightLimitUpgradeAmount * static_cast<float>(std::max(0, levelWeightLimitPoints)) +
        static_cast<float>(staffWeightAdd);
}

float Game::effectiveRingShiftDistance() const
{
    return balance_.spellRingShiftDistance + static_cast<float>(workshopShiftDistanceLevel_) * 8.0f;
}

void Game::initializeDefaultSpellRing()
{
    spellRing_.initialize(balance_);

    const ItemData* shovel = objectCatalog_.registry.findById(DefaultShovelObjectId);
    const ItemData* torch = objectCatalog_.registry.findById(DefaultTorchObjectId);
    if (shovel == nullptr || torch == nullptr) {
        return;
    }

    std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(0);
    ringItems.clear();

    const ItemInstance shovelInstance = inventory_.createDetachedObjectInstance(*shovel);
    const ItemInstance torchInstance = inventory_.createDetachedObjectInstance(*torch);
    if (!spellRing_.addObjectItem(*shovel, shovelInstance) ||
        !spellRing_.addObjectItem(*torch, torchInstance)) {
        spellRing_.initialize(balance_);
        return;
    }

    if (!ringItems.empty()) {
        ringItems[0].localAngle = 0.0f;
    }
    if (ringItems.size() >= 2) {
        ringItems[1].localAngle = Pi;
    }
}

void Game::observeRingItemInstanceIds()
{
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        for (const SpellRingItem& item : spellRing_.itemsForRing(ringIndex)) {
            inventory_.observeObjectInstanceId(item.instanceId);
        }
    }
}

bool Game::registerRingPresetShortcut(int presetIndex)
{
    if (!ringPresets_.capturePreset(presetIndex, spellRing_)) {
        ringStatus_ = "プリセット登録に失敗しました";
        return false;
    }
    ringStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "に登録しました";
    baseStatus_ = ringStatus_;
    return true;
}

bool Game::applyRingPresetShortcut(int presetIndex)
{
    RingPresetApplyResult result = ringPresets_.applyPreset(
        presetIndex,
        inventory_,
        spellRing_,
        objectCatalog_);
    if (!result.status.empty()) {
        ringStatus_ = result.status;
        baseStatus_ = result.status;
    }
    if (!result.applied) {
        return false;
    }

    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.normalizeItemPlacements();
    spellRing_.resetBaseWeightToCurrent();
    observeRingItemInstanceIds();
    refreshEquipmentModifiers();
    refreshOrbitEffects();
    syncEncyclopediaFromInventoryAndRing();
    ringSlotSelection_ = std::clamp(
        ringSlotSelection_,
        0,
        std::max(0, static_cast<int>(spellRing_.items().size()) - 1));
    return true;
}

float Game::effectiveCollectionPullRadius(int collectionLevel) const
{
    return balance_.collectionPullRadiusBase +
        balance_.collectionPullRadiusPerLevel * static_cast<float>(std::max(0, collectionLevel));
}

void Game::configureWatcher()
{
    watcher_ = FileWatcher{};
    if (!hotReloadEnabled_ || autoReloadBlocked_) {
        return;
    }

    watcher_.watchPath("data");
    watcher_.reset();
}

void Game::loadSheetSourceConfig()
{
    std::string error;
    if (!loadGoogleSheetSourceConfig("data/google_sheet_source.cfg", sheetSource_, error)) {
        sheetSource_ = GoogleSheetSourceConfig{};
        logWarning("Google Sheet source disabled: " + error);
        return;
    }
    const bool allowSheetSourceNow = initializeJob_.active && initializeJob_.allowSheetSource;
    if (!allowSheetSourceNow && sheetSource_.enabled) {
        sheetSource_.enabled = false;
        logInfo("Google Sheet source skipped outside test-play startup load; using local/fallback data.");
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
        logError("Runtime balance load failed: " + error);
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

    logError("Google Sheet balance load failed: " + sheetError);
    if (loadedLocal) {
        message = "ローカルデータ読込完了";
        return true;
    }

    message = "シートデータ読込失敗";
    return false;
}

bool Game::loadObjectsFromSheet()
{
    ObjectCatalog loaded;
    bool loadedFromLocalSnapshot = false;
    if (!sheetSource_.enabled) {
        std::string error;
        if (!loadLocalObjectCatalog(loaded, error)) {
            objectCatalog_ = ObjectCatalog{};
            rebuildObjectImageScaleList();
            logError("Local Objects snapshot load failed: " + error);
            return false;
        }
        loadedFromLocalSnapshot = true;
    } else {
        std::string error;
        if (!loadObjectCatalogFromGoogleSheet(sheetSource_, loaded, error)) {
            logError("Objects sheet load failed: " + error);
            return false;
        }
    }

    objectCatalog_ = std::move(loaded);
    effectDispatcher_.clearHandlers();
    effectDispatcher_.registerFoundationHandlers(objectCatalog_);
    logDbValidationReport(objectCatalog_);
    logEffectDispatcherSmoke(objectCatalog_, effectDispatcher_);
    logError(std::string(loadedFromLocalSnapshot ? "Local Objects snapshot loaded: " : "Objects sheet loaded: ") +
        std::to_string(objectCatalog_.registry.size()) + " items");
    logError("Objects loot weight columns detected: " + std::to_string(objectCatalog_.lootWeightStats.detectedColumnCount));
    logError("Objects loot weighted items: " + std::to_string(objectCatalog_.lootWeightStats.weightedItemCount));
    logError("Objects loot weight warnings: " + std::to_string(objectCatalog_.lootWeightStats.warningCount));
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
            << " image_number=" << object.imageNumber
            << " normal_effects=" << object.normalEffects.size()
            << " orbit_effects=" << object.orbitEffects.size()
            << " tags=" << object.tags.size()
            << " loot_weights=" << object.lootWeights.byColumn.size();
        logError(line.str());
    }
    rebuildObjectImageScaleList();
    return true;
}

bool Game::loadStagesFromSheet()
{
    if (!sheetSource_.enabled) {
        stageCatalog_.loadDefaultStages();
        stageCatalog_.validationWarnings.push_back("Google Sheet source is disabled; using default stages");
        logStageCatalogReport(stageCatalog_, false, true, "Google Sheet source is disabled");
        return false;
    }

    StageCatalog loaded;
    std::string error;
    if (!loadStageCatalogFromGoogleSheet(sheetSource_, loaded, error)) {
        std::vector<std::string> warnings = std::move(loaded.validationWarnings);
        stageCatalog_.loadDefaultStages();
        stageCatalog_.validationWarnings = std::move(warnings);
        stageCatalog_.validationWarnings.push_back("Stages sheet fallback used: " + error);
        logError("Stages sheet load failed: " + error);
        logStageCatalogReport(stageCatalog_, false, true, error);
        return false;
    }

    stageCatalog_ = std::move(loaded);
    logStageCatalogReport(stageCatalog_, true, false, {});
    return true;
}

const StageDefinition& Game::currentStageDefinition() const
{
    return currentStageDefinition_;
}

std::vector<StageDefinition> Game::selectableStageDefinitionsForCurrentUnlockState() const
{
    const std::vector<StageDefinition> sorted = stageCatalog_.getStagesSortedByDisplayOrder();
    std::vector<StageDefinition> selectable;
    const int unlockedStoryStages = std::max(0, unlockedStages_);
    int storyStageIndex = 0;

    for (const StageDefinition& stage : sorted) {
        if (isRoguelikeStageDefinition(stage)) {
            if (unlockedStoryStages >= 2) {
                selectable.push_back(stage);
            }
            continue;
        }

        ++storyStageIndex;
        if (storyStageIndex <= unlockedStoryStages) {
            selectable.push_back(stage);
        }
    }

    if (selectable.empty() && !sorted.empty()) {
        selectable.push_back(sorted.front());
    }
    return selectable;
}

int Game::stageCatalogIndexForId(std::string_view stageId) const
{
    const std::vector<StageDefinition> sorted = stageCatalog_.getStagesSortedByDisplayOrder();
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        if (sorted[i].id == stageId) {
            return static_cast<int>(i);
        }
    }
    return std::clamp(currentStage_, 0, std::max(0, static_cast<int>(sorted.size()) - 1));
}

void Game::clampCurrentStageToSelectableStages()
{
    const std::vector<StageDefinition> selectable = selectableStageDefinitionsForCurrentUnlockState();
    if (selectable.empty()) {
        currentStage_ = 0;
        resolveCurrentStageDefinition();
        return;
    }

    bool currentStageSelectable = false;
    for (const StageDefinition& stage : selectable) {
        if (stage.id == currentStageId_) {
            currentStageSelectable = true;
            break;
        }
    }
    if (!currentStageSelectable) {
        currentStageId_ = selectable.front().id;
    }

    currentStage_ = stageCatalogIndexForId(currentStageId_);
    resolveCurrentStageDefinition();
}

void Game::syncWarpStateForCurrentStage()
{
    auto stateIt = dungeonStates_.find(currentStageId_);
    if (stateIt == dungeonStates_.end() || !stateIt->second.valid) {
        unlockedWarpPointCount_ = 0;
        hasLatestWarpPointPosition_ = false;
        latestWarpPointPosition_ = {};
        return;
    }

    int discoveredCount = 0;
    Vec2 latestPosition{};
    bool hasLatest = false;
    for (const WarpPoint& point : stateIt->second.warpPoints) {
        if (!point.discovered) {
            continue;
        }
        ++discoveredCount;
        latestPosition = point.position;
        hasLatest = true;
    }

    const DungeonState& state = stateIt->second;
    unlockedWarpPointCount_ = std::max(std::max(0, state.unlockedWarpPointCount), discoveredCount);
    hasLatestWarpPointPosition_ = state.hasLatestWarpPointPosition || hasLatest;
    latestWarpPointPosition_ = state.hasLatestWarpPointPosition
        ? state.latestWarpPointPosition
        : (hasLatest ? latestPosition : Vec2{});
}

std::string Game::stageClearFlagForStage(std::string_view stageId) const
{
    if (stageId.empty()) {
        return {};
    }

    int storyStageNumber = 0;
    for (const StageDefinition& stage : stageCatalog_.getStagesSortedByDisplayOrder()) {
        if (isRoguelikeStageDefinition(stage)) {
            continue;
        }
        ++storyStageNumber;
        if (stage.id == stageId) {
            return "stage_clear_" + std::to_string(storyStageNumber);
        }
    }

    if (stageId == currentStageId_) {
        return "stage_clear_" + std::to_string(currentStage_ + 1);
    }
    return {};
}

bool Game::stageCleared(std::string_view stageId) const
{
    const std::string flag = stageClearFlagForStage(stageId);
    return !flag.empty() && hasStoryFlag(flag);
}

bool Game::currentStageCleared() const
{
    return stageCleared(currentStageId_);
}

bool Game::currentStageIsRoguelike() const
{
    return isRoguelikeStageDefinition(currentStageDefinition_);
}

int Game::unlockedRingCount() const
{
    return std::clamp(unlockedRingCount_, 1, SpellRingCount);
}

void Game::clampActiveRingToUnlocked()
{
    const int ringCount = unlockedRingCount();
    const int activeRing = spellRing_.activeRingIndex();
    if (activeRing < ringCount) {
        return;
    }

    const int targetRing = std::clamp(activeRing, 0, ringCount - 1);
    spellRing_.switchActiveRing(targetRing - activeRing);
}

void Game::setUnlockedRingCount(int count)
{
    unlockedRingCount_ = std::clamp(count, 1, SpellRingCount);
    clampActiveRingToUnlocked();
}

bool Game::unlockRingsForCurrentStageClear()
{
    int storyStageNumber = currentStage_ + 1;
    int sortedStoryStageNumber = 0;
    for (const StageDefinition& stage : stageCatalog_.getStagesSortedByDisplayOrder()) {
        if (isRoguelikeStageDefinition(stage)) {
            continue;
        }
        ++sortedStoryStageNumber;
        if (stage.id == currentStageId_) {
            storyStageNumber = sortedStoryStageNumber;
            break;
        }
    }

    const int before = unlockedRingCount();
    setUnlockedRingCount(std::max(before, storyStageNumber + 1));
    return unlockedRingCount() > before;
}

std::string Game::currentStageBossCaptureObjectId() const
{
    if (currentStageId_.empty()) {
        return {};
    }
    return "captured_boss_" + currentStageId_;
}

bool Game::hasCapturedBossForCurrentStage() const
{
    const std::string objectId = currentStageBossCaptureObjectId();
    if (objectId.empty()) {
        return false;
    }

    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        if (stack.count > 0 && stack.objectId == objectId) {
            return true;
        }
    }
    for (const InventoryObjectInstance& instance : inventory_.objectInstances()) {
        if (instance.instance.objectId == objectId || instance.item.id == objectId) {
            return true;
        }
    }
    for (const InventoryObjectStack& stack : warehouseObjectStacks_) {
        if (stack.count > 0 && stack.objectId == objectId) {
            return true;
        }
    }
    for (const InventoryObjectInstance& instance : warehouseObjectInstances_) {
        if (instance.instance.objectId == objectId || instance.item.id == objectId) {
            return true;
        }
    }
    for (const SpellRingItem* item : spellRing_.runtimeItems()) {
        if (item != nullptr && item->objectId == objectId) {
            return true;
        }
    }
    return false;
}

void Game::applyDebugStageUnlockState(int unlockedStoryStages)
{
    int maxStoryStageCount = 0;
    for (const StageDefinition& stage : stageCatalog_.getStagesSortedByDisplayOrder()) {
        if (!isRoguelikeStageDefinition(stage)) {
            ++maxStoryStageCount;
        }
    }
    if (maxStoryStageCount <= 0) {
        maxStoryStageCount = 3;
    }

    unlockedStages_ = std::clamp(unlockedStoryStages, 1, maxStoryStageCount);
    setUnlockedRingCount(unlockedStages_);
    storyFlags_.erase(
        std::remove_if(storyFlags_.begin(), storyFlags_.end(), isStageClearStoryFlag),
        storyFlags_.end());

    const int clearedStoryStages = std::max(0, unlockedStages_ - 1);
    storyFlags_.erase(
        std::remove_if(storyFlags_.begin(), storyFlags_.end(), [](const std::string& flag) {
            const std::string_view flagView(flag.data(), flag.size());
            return flagView == IntroTutorialCompletedFlag ||
                flagView == "story_intro_tutorial_fall" ||
                flagView == "story_intro_tutorial_shovel_ready" ||
                flagView == "story_intro_tutorial_torch_found" ||
                flagView == "story_intro_tutorial_torch_ready" ||
                flagView == "story_intro_tutorial_enemy_encounter" ||
                flagView == "story_intro_tutorial_enemy_defeated" ||
                flagView == "story_intro_tutorial_chest_found" ||
                flagView == "story_intro_tutorial_chest_loot_inventory" ||
                flagView == "story_intro_tutorial_chest_loot_ring" ||
                flagView == "story_intro_tutorial_midway" ||
                flagView == "story_intro_tutorial_exit_found" ||
                flagView == "story_opening_base_intro";
        }),
        storyFlags_.end());
    if (clearedStoryStages > 0) {
        addStoryFlag(std::string(IntroTutorialCompletedFlag));
        addStoryFlag("story_intro_tutorial_fall");
        addStoryFlag("story_intro_tutorial_shovel_ready");
        addStoryFlag("story_intro_tutorial_torch_found");
        addStoryFlag("story_intro_tutorial_torch_ready");
        addStoryFlag("story_intro_tutorial_enemy_encounter");
        addStoryFlag("story_intro_tutorial_enemy_defeated");
        addStoryFlag("story_intro_tutorial_chest_found");
        addStoryFlag("story_intro_tutorial_chest_loot_inventory");
        addStoryFlag("story_intro_tutorial_chest_loot_ring");
        addStoryFlag("story_intro_tutorial_midway");
        addStoryFlag("story_intro_tutorial_exit_found");
        addStoryFlag("story_opening_base_intro");
    }

    for (int stage = 1; stage <= clearedStoryStages; ++stage) {
        addStoryFlag("stage_clear_" + std::to_string(stage));
    }

    clampCurrentStageToSelectableStages();
    syncWarpStateForCurrentStage();
    baseMiningStartSelection_ = unlockedWarpPointCount_ > 0 ? 1 : 0;
    baseWarpPointSelectActive_ = false;
    baseWarpPointSelection_ = 0;
    baseRegenerateConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    baseStatus_.clear();
}

void Game::resolveCurrentStageDefinition()
{
    if (const StageDefinition* stage = stageCatalog_.getStageById(currentStageId_)) {
        currentStageDefinition_ = *stage;
        currentStage_ = stageCatalogIndexForId(currentStageId_);
        logCurrentStageDefinition(currentStageDefinition_, {});
        return;
    }

    if (!stageCatalog_.stages.empty()) {
        std::vector<StageDefinition> sorted = stageCatalog_.getStagesSortedByDisplayOrder();
        currentStageDefinition_ = sorted.front();
        logError("[warning] currentStageId=\"" + currentStageId_ +
            "\" was not found in StageCatalog; using first display-order stage \"" +
            currentStageDefinition_.id + "\"");
        currentStageId_ = currentStageDefinition_.id;
        currentStage_ = 0;
        logCurrentStageDefinition(currentStageDefinition_, "fallback_to_catalog_first");
        return;
    }

    logError("[warning] StageCatalog is empty; using code default stage_01_stardust");
    currentStageDefinition_ = makeCodeDefaultStageDefinition();
    currentStageId_ = currentStageDefinition_.id;
    currentStage_ = 0;
    logCurrentStageDefinition(currentStageDefinition_, "fallback_to_code_default");
}

bool Game::loadEnemiesFromSheet()
{
    EnemyCatalog loaded;
    bool loadedFromLocalSnapshot = false;
    if (!sheetSource_.enabled) {
        std::string error;
        if (!loadLocalEnemyCatalog(objectCatalog_.specialTags, loaded, error)) {
            enemyCatalog_ = EnemyCatalog{};
            logError("Local Enemies snapshot load failed: " + error);
            return false;
        }
        loadedFromLocalSnapshot = true;
    } else {
        std::string error;
        if (!loadEnemyCatalogFromGoogleSheet(sheetSource_, objectCatalog_.specialTags, loaded, error)) {
            logError("Enemies sheet load failed: " + error);
            return false;
        }
    }

    enemyCatalog_ = std::move(loaded);
    for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
        upsertObjectDefinition(objectCatalog_, makeCapturedObjectDefinition(enemy));
    }
    logEnemyDbValidationReport(enemyCatalog_);
    logError(std::string(loadedFromLocalSnapshot ? "Local Enemies snapshot loaded: " : "Enemies sheet loaded: ") +
        std::to_string(enemyCatalog_.enemies.size()) +
        " enemies, " + std::to_string(enemyCatalog_.enemiesById.size()) + " unique IDs");
    logError("Enemies spawn weight columns detected: " + std::to_string(enemyCatalog_.spawnWeightStats.detectedColumnCount));
    logError("Enemies spawn weighted enemies: " + std::to_string(enemyCatalog_.spawnWeightStats.weightedEnemyCount));
    logError("Enemies spawn weight warnings: " + std::to_string(enemyCatalog_.spawnWeightStats.warningCount));
    logError("Behavior sheet loaded: " + std::to_string(enemyCatalog_.behaviors.size()) +
        " behaviors, " + std::to_string(enemyCatalog_.behaviorsById.size()) + " unique IDs");

    std::size_t enemyLogCount = 0;
    for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
        if (enemyLogCount >= 5) {
            break;
        }
        std::ostringstream line;
        line << "  enemy id=\"" << enemy.id
            << "\" name=\"" << enemy.name
            << "\" hp=" << enemy.hp
            << " contact_attack=" << enemy.contactAttackPower
            << " damage_type=\"" << enemy.contactDamageType
            << "\" move_speed=" << enemy.moveSpeed
            << " radius=" << enemy.radius
            << " xp=" << enemy.xp
            << " enemy_behaviors=" << enemy.enemyBehaviorIds.size()
            << " captured_behaviors=" << enemy.capturedBehaviorIds.size()
            << " tags=" << enemy.enemyTags.size()
            << " captured_tags=" << enemy.capturedTags.size()
            << " spawn_weights=" << enemy.spawnWeights.size();
        logError(line.str());
        ++enemyLogCount;
    }

    if (!enemyCatalog_.validationWarnings.empty()) {
        logError("Enemy DB warnings: " + std::to_string(enemyCatalog_.validationWarnings.size()));
        for (const std::string& warning : enemyCatalog_.validationWarnings) {
            logError("  [warning] " + warning);
        }
    }
    return true;
}

void Game::enterGameOver()
{
    if (mode_ == ScreenMode::GameOver || mode_ == ScreenMode::StageClear || mode_ == ScreenMode::AstralResult) {
        return;
    }
    if (currentStageIsRoguelike()) {
        enterAstralResult(AstralRunResult::Died);
        return;
    }

    resetBossEncounter();
    player_.hp = 0;
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    closeDebugItemPicker();
    closeDebugStoryTest();
    debugStoryTestReturnAfterDialogue_ = false;
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    levelUpPresentation_ = {};
    levelUpResultDialog_ = {};
    mode_ = ScreenMode::GameOver;
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    gameOverSelection_ = 0;
    gameOverStatus_.clear();
}

void Game::markCurrentStageCleared()
{
    unlockedStages_ = std::max(unlockedStages_, currentStage_ + 2);
    unlockRingsForCurrentStageClear();
    addStoryFlag("stage_clear_" + std::to_string(currentStage_ + 1));
}

void Game::enterStageClear()
{
    if (mode_ == ScreenMode::StageClear) {
        return;
    }

    markCurrentStageCleared();
    clearTemporaryPlayerState(true);
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    closeDebugItemPicker();
    closeDebugStoryTest();
    debugStoryTestReturnAfterDialogue_ = false;
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    levelUpPresentation_ = {};
    levelUpResultDialog_ = {};
    mode_ = ScreenMode::StageClear;
    playAudioBgm(AudioBgmDungeon, 0.55f);
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    stageClearSelection_ = 0;
    stageClearStatus_.clear();
}

void Game::beginFinalBossEndingSequence()
{
    if (endingKamishibaiPending_ || mode_ == ScreenMode::EndingKamishibai) {
        return;
    }

    markCurrentStageCleared();
    clearTemporaryPlayerState(true);
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    closeDebugItemPicker();
    closeDebugStoryTest();
    debugStoryTestReturnAfterDialogue_ = false;
    if (levels_.isChoosing()) {
        levels_ = LevelSystem{};
    }
    levelUpPresentation_ = {};
    levelUpResultDialog_ = {};
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    endingKamishibaiPending_ = true;
}

void Game::updateScreenMode(
    const Input& input,
    UiContext& ui,
    float dt,
    std::vector<EffectDiscoveryEvent>* discoveryEvents)
{
    if (mode_ == ScreenMode::OpeningKamishibai) {
        updateOpeningKamishibai(input, dt);
        return;
    }

    if (mode_ == ScreenMode::EndingKamishibai) {
        updateEndingKamishibai(input, dt);
        return;
    }

    if (mode_ == ScreenMode::Title) {
        updateTitleScreen(input, ui);
        return;
    }

    if (mode_ == ScreenMode::WorldLoading) {
        return;
    }

    if (firstItemAcquisitionNoticeActive()) {
        updateFirstItemAcquisitionNotice(input, ui);
        return;
    }

    if (debugItemPickerActive_) {
        updateDebugItemPicker(input, ui);
        return;
    }

    if (debugStoryTestActive_) {
        updateDebugStoryTest(input, ui);
        return;
    }

    if (mode_ == ScreenMode::ObjectImageScaleEdit) {
        updateObjectImageScaleEditScreen(input, ui);
        return;
    }

    if (mode_ == ScreenMode::EnemyHitboxEdit) {
        updateEnemyHitboxEditScreen(input, ui);
        return;
    }

    if (mode_ == ScreenMode::AudioCueEdit) {
        updateAudioCueEditScreen(input, ui);
        return;
    }

    if (debugStoryTestReturnAfterDialogue_ && !dialogue_.active()) {
        debugStoryTestReturnAfterDialogue_ = false;
        openDebugStoryTest(debugStoryTestMode_);
        if (debugStoryTestActive_) {
            return;
        }
    }

    updateQueuedStoryEvents();
    if (dialogue_.active()) {
        updateDialoguePlayerIdleAnimation(dt);
        const bool dialogueWasActive = dialogue_.active();
        dialogue_.update(input, dt);
        runDialogueCompletionCallbackIfFinished(dialogueWasActive);
        ui.consumePointer();
        return;
    }

    if (updateDungeonFocus(dt)) {
        ui.consumePointer();
        return;
    }

    if (updateBossEncounterFlow(dt)) {
        return;
    }

    if (endingKamishibaiPending_ && pendingStoryTriggers_.empty()) {
        startEndingKamishibai();
        return;
    }

    if (levels_.isChoosing() && mode_ != ScreenMode::LevelUp && mode_ != ScreenMode::WorldLoading) {
        openLevelUpChoice(basePresentationActive() ? ScreenMode::Base : ScreenMode::Playing);
    }

    if (mode_ == ScreenMode::Base) {
        updateBaseScreen(input, ui, dt);
        return;
    }

    if (player_.hp <= 0 &&
        mode_ != ScreenMode::GameOver &&
        mode_ != ScreenMode::StageClear &&
        mode_ != ScreenMode::AstralResult) {
        enterGameOver();
    }
    if (mode_ == ScreenMode::GameOver) {
        updateGameOverScreen(input, ui);
        return;
    }
    if (mode_ == ScreenMode::StageClear) {
        updateStageClearScreen(input, ui);
        return;
    }
    if (mode_ == ScreenMode::AstralResult) {
        updateAstralResultScreen(input, ui);
        return;
    }

    if (levels_.isChoosing() && mode_ != ScreenMode::LevelUp) {
        openLevelUpChoice(ScreenMode::Playing);
    }

    switch (mode_) {
    case ScreenMode::OpeningKamishibai:
        updateOpeningKamishibai(input, dt);
        break;
    case ScreenMode::EndingKamishibai:
        updateEndingKamishibai(input, dt);
        break;
    case ScreenMode::Title:
        updateTitleScreen(input, ui);
        break;
    case ScreenMode::Base:
        updateBaseScreen(input, ui, dt);
        break;
    case ScreenMode::WorldLoading:
        break;
    case ScreenMode::Playing:
        if (effectTestActive_) {
            updateEffectTestScreen(input, ui, dt);
            return;
        }
        if (projectileTestActive_) {
            updateProjectileTestScreen(input, ui, dt);
            return;
        }
        if (enemyTestActive_) {
            updateEnemyTestUi(input, ui);
            if (mode_ != ScreenMode::Playing || (enemyTestUiVisible_ && input.pausePressed())) {
                return;
            }
        }
        if (updateWarpReturnUi(input, ui)) {
            return;
        }
        if (updateDungeonEventNpcInteraction(input, ui)) {
            return;
        }
        if (input.pausePressed()) {
            ui.emitSound(UiSoundEvent::MenuOpen);
            mode_ = ScreenMode::PauseMenu;
            pauseReturnMode_ = ScreenMode::Playing;
            pausePage_ = PauseMenuPage::Main;
            return;
        }
        updateRingStatusHud(ui);
        inventory_.update(
            input,
            ui,
            player_,
            spellRing_,
            effectDispatcher_,
            false,
            objectCatalog_,
            &magic_,
            discoveryEvents,
            &encyclopedia_,
            unlockedRingCount());
        if (inventory_.isOpen()) {
            inventoryReturnToPause_ = false;
            mode_ = ScreenMode::Inventory;
            return;
        }
        if (input.shortcutSlotPressed() >= 0 && input.shortcutSlotPressed() < RingPresetSlotCount) {
            ui.emitSound(applyRingPresetShortcut(input.shortcutSlotPressed())
                ? UiSoundEvent::Confirm
                : UiSoundEvent::Cancel);
            return;
        }
        if (input.activeRingDelta() != 0) {
            switchActiveRingWithLog(input.activeRingDelta());
        }
        inventory_.updateShortcuts(
            input,
            ui,
            player_,
            spellRing_,
            effectDispatcher_,
            camera_.width(),
            camera_.height(),
            &magic_,
            discoveryEvents,
            &encyclopedia_);
        break;
    case ScreenMode::PauseMenu:
        updatePauseMenu(input, ui);
        break;
    case ScreenMode::Inventory:
    {
        const bool inventoryWorldActionsEnabled = pauseReturnMode_ != ScreenMode::Base;
        inventory_.updateScreen(
            input,
            ui,
            player_,
            spellRing_,
            effectDispatcher_,
            objectCatalog_,
            &magic_,
            discoveryEvents,
            &encyclopedia_,
            inventoryWorldActionsEnabled,
            inventoryWorldActionsEnabled,
            unlockedRingCount());
        std::vector<InventoryDiscardRequest> discardRequests = inventory_.consumeDiscardRequests();
        if (!discardRequests.empty()) {
            spawnInventoryDiscardRequests(std::move(discardRequests));
        }
        if (!inventory_.isOpen()) {
            mode_ = inventoryReturnToPause_ ? ScreenMode::PauseMenu : ScreenMode::Playing;
            inventoryReturnToPause_ = false;
            pausePage_ = PauseMenuPage::Main;
        }
        break;
    }
    case ScreenMode::Ring:
        updateRingScreen(input, ui, dt);
        break;
    case ScreenMode::ObjectImageScaleEdit:
        updateObjectImageScaleEditScreen(input, ui);
        break;
    case ScreenMode::EnemyHitboxEdit:
        updateEnemyHitboxEditScreen(input, ui);
        break;
    case ScreenMode::AudioCueEdit:
        updateAudioCueEditScreen(input, ui);
        break;
    case ScreenMode::LevelUp:
        updateLevelUpScreen(input, ui, dt);
        break;
    case ScreenMode::GameOver:
        break;
    case ScreenMode::StageClear:
        break;
    case ScreenMode::AstralResult:
        break;
    }
}

void Game::updateDialoguePlayerIdleAnimation(float dt)
{
    if (basePresentationActive()) {
        updateBasePlayerSpriteAnimation(dt, false);
        return;
    }
    if (mode_ == ScreenMode::Playing) {
        player_.updateSpriteAnimation(dt, false);
    }
}

void Game::runDialogueCompletionCallbackIfFinished(bool dialogueWasActive)
{
    if (!dialogueWasActive || dialogue_.active() || !pendingDialogueCompletion_) {
        return;
    }

    std::function<void()> onComplete = std::move(pendingDialogueCompletion_);
    pendingDialogueCompletion_ = {};
    onComplete();
}

bool Game::gameProgressPaused() const
{
    return effectTestActive_ ||
        projectileTestActive_ ||
        debugItemPickerActive_ ||
        debugStoryTestActive_ ||
        pendingStoryTriggerDelayActive() ||
        firstItemAcquisitionNoticeActive() ||
        dungeonFocusActive() ||
        dialogue_.active() ||
        (introTutorialActive() && dungeonRingIntroActive()) ||
        bossEncounterBlocksProgress() ||
        endingKamishibaiPending_ ||
        warpReturnConfirm_.open ||
        mode_ != ScreenMode::Playing;
}

bool Game::dungeonEventUiSuppressed() const
{
    return mode_ == ScreenMode::Playing &&
        (!pendingStoryTrigger_.empty() ||
            !pendingStoryTriggers_.empty() ||
            pendingStoryTriggerDelayActive() ||
            dungeonFocusActive() ||
            dialogue_.active());
}

void Game::updatePausedDungeonPresentation(float dt)
{
    if (mode_ != ScreenMode::Playing) {
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    spellRing_.updatePresentation(player_, safeDt, balance_);
    worldDrops_.updatePresentation(safeDt);
    for (ChestNode& node : chestNodes_) {
        updateChestSpawnJump(node, safeDt);
    }
    wetGround_.update(safeDt);
    wetGround_.erasePendingGroundLines(groundLines_);
    groundLines_.update(safeDt);
    magicFx_.update(safeDt);
    effects_.update(safeDt);
}

bool Game::basePresentationActive() const
{
    if (mode_ == ScreenMode::Base || mode_ == ScreenMode::WorldLoading) {
        return true;
    }
    if (mode_ == ScreenMode::LevelUp && levelUpReturnMode_ == ScreenMode::Base) {
        return true;
    }
    if (pauseReturnMode_ != ScreenMode::Base) {
        return false;
    }
    return mode_ == ScreenMode::PauseMenu ||
        mode_ == ScreenMode::Inventory ||
        mode_ == ScreenMode::Ring;
}

void Game::beginDungeonRingIntro()
{
    spellRing_.resetRuntimeStateAtPlayer(player_, balance_);

    if (screenTransition_.active()) {
        dungeonRingIntroStartPending_ = true;
        dungeonRingIntroTimer_ = 0.0f;
        return;
    }

    dungeonRingIntroStartPending_ = false;
    dungeonRingIntroTimer_ = DungeonRingIntroDuration;
}

void Game::updateDungeonRingIntro(float dt)
{
    if (mode_ != ScreenMode::Playing || dialogue_.active() || dungeonRingIntroTimer_ <= 0.0f) {
        return;
    }
    const bool wasActive = dungeonRingIntroTimer_ > 0.0f;
    dungeonRingIntroTimer_ = std::max(0.0f, dungeonRingIntroTimer_ - std::max(0.0f, dt));
    if (wasActive && dungeonRingIntroTimer_ <= 0.0f && stageStartStoryPendingAfterRingIntro_) {
        stageStartStoryPendingAfterRingIntro_ = false;
        maybeQueueStageStartStory();
    }
}

bool Game::dungeonRingIntroActive() const
{
    return mode_ == ScreenMode::Playing && (dungeonRingIntroStartPending_ || dungeonRingIntroTimer_ > 0.0f);
}

float Game::dungeonRingIntroProgress() const
{
    if (dungeonRingIntroStartPending_) {
        return 0.0f;
    }
    if (dungeonRingIntroTimer_ <= 0.0f) {
        return 1.0f;
    }
    return clamp(1.0f - dungeonRingIntroTimer_ / DungeonRingIntroDuration, 0.0f, 1.0f);
}

void Game::queueIntroTutorialChestLootDialogueIfReady()
{
    if (!introTutorialActive() ||
        !introTutorialChestLootPending_ ||
        introTutorialChestLootDialogueQueued_ ||
        firstItemAcquisitionNoticeActive()) {
        return;
    }

    const bool objectAlreadyOnRing =
        spellRingContainsInstanceId(spellRing_, introTutorialChestLootInstanceId_) ||
        spellRingContainsObjectId(spellRing_, introTutorialChestLootObjectId_);

    introTutorialChestLootPending_ = false;
    introTutorialChestLootDialogueQueued_ = true;
    queueStoryEventForTrigger(std::string(
        objectAlreadyOnRing
            ? IntroTutorialChestLootRingTrigger
            : IntroTutorialChestLootInventoryTrigger));
}

void Game::switchActiveRingWithLog(int delta)
{
    if (delta == 0) {
        return;
    }

    clampActiveRingToUnlocked();
    const int ringCount = unlockedRingCount();
    if (ringCount <= 1) {
        return;
    }

    const int previousRing = spellRing_.activeRingIndex();
    int targetRing = (previousRing + delta) % ringCount;
    if (targetRing < 0) {
        targetRing += ringCount;
    }
    spellRing_.switchActiveRing(targetRing - previousRing);
    const int currentRing = spellRing_.activeRingIndex();
    if (currentRing != previousRing && mode_ == ScreenMode::Playing) {
        pushDungeonLog("Ring " + std::to_string(currentRing + 1) + " に切替", "ring_switch");
    }
}

void Game::update(const Input& input, const Time& time)
{
    const GameSettings currentSettings = settingsGetter_ ? settingsGetter_() : optionsSettings_;
    lightweightModeActive_ = currentSettings.performance.lightweight;
    presentationSettingsActive_ = currentSettings.presentation;
    const bool lightweight = lightweightModeEnabled();
    effects_.setLightweightMode(lightweight);
    magicFx_.setLightweightMode(lightweight);
    wetGround_.setLightweightMode(lightweight);
    updateScreenShake(time.deltaSeconds());
    updatePlayerDamageVignette(time.deltaSeconds());
    updateAudioJingle(time.deltaSeconds());

    checkHotReload(time.deltaSeconds());
    reloadNoticeTimer_ = std::max(0.0f, reloadNoticeTimer_ - time.deltaSeconds());
    encyclopedia_.update(time.deltaSeconds());
    updateDungeonLogs(time.deltaSeconds());

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
    if (playTimeCountsForMode(mode_) && !effectTestActive_ && !projectileTestActive_) {
        playTimeSeconds_ += std::max(0.0f, time.deltaSeconds());
    }
    const bool transitionWasActive = screenTransition_.active();
    updateScreenTransition(time.deltaSeconds());
    if (transitionWasActive || screenTransition_.active()) {
        return;
    }
    const bool pendingStoryDelayWasActive = pendingStoryTriggerDelayActive();
    updatePendingStoryTriggerDelay(time.deltaSeconds());
    if (pendingStoryDelayWasActive && pendingStoryTriggerDelayActive()) {
        updateDialoguePlayerIdleAnimation(time.deltaSeconds());
        updatePausedDungeonPresentation(time.deltaSeconds());
        return;
    }
    if (worldBuildActive()) {
        updateWorldBuild(time.deltaSeconds());
        return;
    }
    updateDungeonRingIntro(time.deltaSeconds());
    magic_.setFxSystem(&magicFx_);

    std::vector<EffectDiscoveryEvent> effectDiscoveries;
    UiContext ui(input);
    struct UiSoundFlush {
        Game& game;
        const UiContext& ui;
        ~UiSoundFlush() { game.playUiSoundEvents(ui); }
    } uiSoundFlush{*this, ui};
    const bool wasPaused = gameProgressPaused();
    updateScreenMode(input, ui, time.deltaSeconds(), &effectDiscoveries);
    effects_.setLightweightMode(lightweight);
    magicFx_.setLightweightMode(lightweight);
    wetGround_.setLightweightMode(lightweight);
    queueIntroTutorialChestLootDialogueIfReady();
    if (inventory_.equippedStaffInstanceId() != observedEquippedStaffInstanceId_) {
        refreshEquipmentModifiers();
        applyPermanentUpgrades();
    }
    consumeInventoryUseEvents();
    for (const RingEquipFxRequest& request : inventory_.consumeRingEquipFxRequests()) {
        spawnRingEquipFx(request);
    }
    updateRingEquipFx(time.deltaSeconds());
    refreshOrbitEffects();
    const bool paused = gameProgressPaused() || (wasPaused && mode_ == ScreenMode::Playing);
    if (paused && !effectDiscoveries.empty()) {
        applyEffectDiscoveries(effectDiscoveries);
    }
    syncIntroTutorialTerrainDamageLocks();
    if (dungeonEventUiSuppressed()) {
        updatePausedDungeonPresentation(time.deltaSeconds());
    }

    if (!paused) {
        runStats_.elapsedSeconds += time.deltaSeconds();
        updateAstralRunProgress();
        updatePlayerFootstepDust(time.deltaSeconds());
        const RuntimeBalance dungeonBalance = runtimeBalanceForDungeon();
        const int playerChunkBeforeX = chunkCoordForWorld(player_.position.x);
        const int playerChunkBeforeY = chunkCoordForWorld(player_.position.y);
        tileMap_.updateAround(player_.position, time.deltaSeconds(), dungeonBalance, dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        std::vector<CollisionRect> objectBlockers;
        if (!enemyTestActive_) {
            objectBlockers = solidObjectCollisionRects();
        }
        const RingEquipmentModifiers& activeEquipment =
            spellRing_.equipmentModifiersForRing(spellRing_.activeRingIndex());
        player_.spellRingShiftDistanceMultiplier = static_cast<float>(
            std::max(0.0, spellRing_.orbitShiftMultiplier()) *
            std::max(0.0, activeEquipment.ringShiftDistanceMul));
        player_.update(
            input,
            camera_,
            tileMap_,
            time.deltaSeconds(),
            false,
            balance_,
            std::span<const CollisionRect>{objectBlockers.data(), objectBlockers.size()});
        maybeTriggerPlayerFootstep(
            player_.position,
            lengthSquared(player_.velocity) > 0.0001f ? player_.velocity : player_.facing,
            player_.spriteWalking,
            player_.spriteFrameIndex(),
            previousPlayerDustFrame_,
            PlayerFootstepSurface::Dungeon);
        updatePlayerRegen(time.deltaSeconds(), effectDiscoveries);
        if (player_.hp <= 0) {
            enterGameOver();
            refreshOrbitEffects();
            return;
        }
        if (!enemyTestActive_) {
            updateWarpPoints(time.deltaSeconds());
            updateExposedRewardNodes();
            updateExposedMoonFragmentNodes();
            const int enemyCountBeforeExposedSpawn = enemies_.activeCount();
            updateExposedEnemyNodes();
            if (enemies_.activeCount() > enemyCountBeforeExposedSpawn) {
                playAudioSe(AudioSeEnemySpawn);
            }
        }
        updateRingEffectDiscoveries(effectDiscoveries);
        normalizeOpenBuriedPlacementNodes();
        camera_.follow(player_.position, time.deltaSeconds());
        if (updateDungeonEventDiscovery(time.deltaSeconds())) {
            return;
        }

        spellRing_.update(player_, input, time.deltaSeconds(), time.totalSeconds(), false, ui.pointerConsumed(), balance_);
        for (const RingMotionEvent& event : spellRing_.consumeMotionEvents()) {
            if (event.kind == RingMotionEventKind::ThrowStart) {
                playAudioSe(AudioSeRingThrow);
                effects_.spawnThrowStart(event.position, event.direction);
            } else if (event.kind == RingMotionEventKind::ReturnEnd) {
                effects_.spawnReturn(event.position);
            }
        }
        if (!introTutorialActive() && input.ringOffsetHeld()) {
            queueStoryEventForTrigger("tutorial:ring_shift");
        }
        updateDungeonEvents(time.deltaSeconds(), time.totalSeconds());
        updateChestNodes(time.deltaSeconds(), input);
        if (!enemyTestActive_) {
            updateCrateNodes();
        }

        const bool playerChunkChanged =
            chunkCoordForWorld(player_.position.x) != playerChunkBeforeX ||
            chunkCoordForWorld(player_.position.y) != playerChunkBeforeY;
        if (playerChunkChanged) {
            tileMap_.updateAround(player_.position, time.deltaSeconds(), dungeonBalance, dungeonLayout_);
        }
        digging_.update(
            tileMap_,
            spellRing_,
            player_,
            time.totalSeconds(),
            objectCatalog_,
            effectDispatcher_,
            &magic_,
            &effectDiscoveries,
            &encyclopedia_);
        const std::vector<RingImpactSoundPlayback> terrainImpactSounds =
            resolveRingImpactSoundEvents(digging_.impactSoundEvents(), lootRuntimeRng(), 3);
        if (!terrainImpactSounds.empty()) {
            for (const RingImpactSoundPlayback& sound : terrainImpactSounds) {
                playAudioSe(sound.cueId, sound.volumeScale, sound.pitchScale);
            }
        } else {
            if (!digging_.dugTiles().empty()) {
                bool brokeOre = false;
                for (const DugTile& tile : digging_.dugTiles()) {
                    if (tile.type == TileType::Ore) {
                        brokeOre = true;
                        break;
                    }
                }
                playAudioSe(brokeOre ? AudioSeDigOreBreak : AudioSeDigBreak);
            }
            if (!digging_.hitTiles().empty()) {
                playAudioSe(AudioSeDigHit);
            }
        }
        for (const TerrainHitTile& tile : digging_.hitTiles()) {
            effects_.spawnDigHit(tile.center, tile.center - spellRing_.center(), tile.color);
        }
        if (digging_.dugTiles().empty()) {
            for (Vec2 tile : digging_.openedTiles()) {
                effects_.spawnTileBreak(tile, TileType::Dirt, tileMap_.tileColorAtWorld(tile));
            }
        }
        if (!enemyTestActive_) {
            revealRewardNodesFromOpenedTiles(digging_.openedTiles());
            revealMoonFragmentNodesFromOpenedTiles(digging_.openedTiles());
            revealChestNodesFromOpenedTiles(digging_.openedTiles());
        }
        revealDungeonMinimapOpenedTiles(digging_.openedTiles());
        for (const DugTile& tile : digging_.dugTiles()) {
            effects_.spawnTileBreak(tile.center, tile.type, tile.color);
            ++runStats_.dugTiles;

            std::mt19937& rng = lootRuntimeRng();
            if (digEventDue(
                    runStats_.dugTilesSinceMoneyDrop,
                    balance_.digMoneyMinDugTiles,
                    balance_.digMoneyGuaranteeDugTiles,
                    rng)) {
                const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, {
                    static_cast<float>(tileMap_.worldToTile(tile.center.x)),
                    static_cast<float>(tileMap_.worldToTile(tile.center.y)),
                });
                const int depthRank = lootDepthRankForProgress(currentStageId_, metrics.pathProgress);
                const float multiplier =
                    lootStageMultiplier(balance_, currentStageId_) *
                    lootDepthMultiplier(balance_, currentStageId_, depthRank);
                std::uniform_int_distribution<int> moneyDistribution(2, 6);
                const int amount = scaledLootAmount(moneyDistribution(rng), multiplier);
                if (worldDrops_.spawnMoneyDrop(amount, scatterLootPosition(tile.center, rng), runStats_.elapsedSeconds)) {
                    runStats_.dugTilesSinceMoneyDrop = 0;
                }
            }

            if (digEventDue(
                    runStats_.dugTilesSinceItemDrop,
                    balance_.digItemMinDugTiles,
                    balance_.digItemGuaranteeDugTiles,
                    rng)) {
                const int depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, tile.center);
                if (spawnWeightedObjectLoot(
                        LootChestKind::Common,
                        depthRank,
                        tile.center,
                        rng,
                        "DigItemLoot",
                        false,
                        LootSourceKind::DigItem)) {
                    runStats_.dugTilesSinceItemDrop = 0;
                }
            }
        }
        for (const CapturedRewardDropRequest& rewardRequest : digging_.rewardDropRequests()) {
            std::mt19937& rng = lootRuntimeRng();
            WeightedObjectLootProfile lootProfile;
            if (!weightedObjectLootProfileForDropProfile(rewardRequest.profile, lootProfile)) {
                logError("[warning] CapturedRewardLoot: unknown reward profile \"" + rewardRequest.profile + "\"; no item drop");
                continue;
            }
            const int depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, rewardRequest.position);
            spawnWeightedObjectLoot(
                lootProfile.chestKind,
                depthRank,
                rewardRequest.position,
                rng,
                "CapturedRewardLoot",
                true,
                LootSourceKind::CapturedReward,
                lootProfile.requiredTag);
        }
        for (const DugTile& tile : digging_.dugTiles()) {
            if (tile.type != TileType::Ore) {
                continue;
            }
            const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(dungeonLayout_, {
                static_cast<float>(tileMap_.worldToTile(tile.center.x)),
                static_cast<float>(tileMap_.worldToTile(tile.center.y)),
            });
            const int depthRank = lootDepthRankForProgress(currentStageId_, metrics.pathProgress);
            const float multiplier =
                lootStageMultiplier(balance_, currentStageId_) *
                lootDepthMultiplier(balance_, currentStageId_, depthRank);
            std::uniform_int_distribution<int> oreAmountDistribution(balance_.oreMaterialMin, balance_.oreMaterialMax);
            const int amount = scaledLootAmount(oreAmountDistribution(lootRuntimeRng()), multiplier);
            worldDrops_.spawnMaterialDrop(MaterialType::EnhancementOre, amount, tile.center, runStats_.elapsedSeconds);
        }
        for (const DugTile& tile : digging_.dugTiles()) {
            if (trySpawnFailsafeShovelDropFromWall(tile.center)) {
                break;
            }
        }
        std::vector<WorldDropPickupEvent> pickupEvents;
        int blockedObjectPickupCount = 0;
        const float collectionPullRadius = effectiveCollectionPullRadius(collectionRangeUpgradeLevel_);
        if (collectionPullRadius > 0.0f) {
            worldDrops_.pullNearbyDrops(
                player_.position,
                time.deltaSeconds(),
                collectionPullRadius,
                balance_.collectionPullAcceleration,
                balance_.collectionPullLimit,
                &inventory_,
                &objectCatalog_);
        }
        runStats_.acquiredItems += worldDrops_.update(
            time.deltaSeconds(),
            player_,
            inventory_,
            money_,
            objectCatalog_,
            &effects_,
            &pickupEvents,
            &blockedObjectPickupCount);
        for (const WorldDropPickupEvent& event : pickupEvents) {
            if (event.kind == WorldDropKind::Object) {
                runStats_.acquiredObjectItems += std::max(1, event.quantity);
                recordObjectObtainedForFirstNotice(event.id, event.instanceId, event.protectable, player_.position);
                if (std::string_view(event.id) == MagnifyingGlassObjectId) {
                    queueStoryEventForTrigger("tutorial:magnifying_glass");
                }
                if (!introTutorialActive() && objectIdHasCaptureNetOrbitEffect(objectCatalog_, event.id)) {
                    queueStoryEventForTrigger("tutorial:capture_net");
                }
            }
        }
        if (introTutorialActive() &&
            introTutorialChestOpened_ &&
            !introTutorialChestLootDialogueQueued_ &&
            !pickupEvents.empty()) {
            bool objectPickedUp = false;
            for (const WorldDropPickupEvent& event : pickupEvents) {
                if (event.kind != WorldDropKind::Object) {
                    continue;
                }
                objectPickedUp = true;
                introTutorialChestLootObjectId_ = event.id;
                introTutorialChestLootInstanceId_ = event.instanceId;
            }
            if (objectPickedUp) {
                introTutorialChestLootPending_ = true;
                queueIntroTutorialChestLootDialogueIfReady();
            }
        }
        appendPickupLogs(pickupEvents);
        if (!pickupEvents.empty()) {
            playAudioSe(AudioSePickup);
        }
        if (blockedObjectPickupCount > 0) {
            pushDungeonLog("リュックがいっぱいで拾えません", "pickup_inventory_full");
        }
        updateDigToolFailsafe(time.deltaSeconds());
        if (!enemyTestActive_) {
            const std::vector<Vec2> randomEnemySpawnTiles = spawnHiddenEnemyNodesFromOpenedTiles(digging_.openedTiles());
            std::vector<DugEnemySpawnPoint> randomEnemySpawnPoints;
            randomEnemySpawnPoints.reserve(randomEnemySpawnTiles.size());
            for (Vec2 spawnTile : randomEnemySpawnTiles) {
                randomEnemySpawnPoints.push_back(DugEnemySpawnPoint{
                    .tileCenter = spawnTile,
                    .depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, spawnTile),
                });
            }
            const int enemyCountBeforeDugSpawn = enemies_.activeCount();
            enemies_.spawnFromDugTiles(randomEnemySpawnPoints, tileMap_, player_.position, dungeonBalance, enemyCatalog_, currentStageId_);
            if (enemies_.activeCount() > enemyCountBeforeDugSpawn) {
                playAudioSe(AudioSeEnemySpawn);
            }
            updateBossSpawn();
        }

        updateOrbitAreaEffects(time.deltaSeconds(), effectDiscoveries);
        updateOrbitGroundEffects(time.deltaSeconds(), effectDiscoveries);

        const bool capturedBossOwned = hasCapturedBossForCurrentStage();
        const bool allowBossCapture = currentStageCleared() && !capturedBossOwned;
        const std::string bossCaptureObjectId = currentStageCleared()
            ? currentStageBossCaptureObjectId()
            : std::string{};
        enemies_.update(
            player_,
            spellRing_,
            inventory_,
            tileMap_,
            time.deltaSeconds(),
            time.totalSeconds(),
            false,
            balance_,
            objectCatalog_,
            worldDrops_,
            witchSelfLightCenter(player_.position),
            std::vector<LightSource>{},
            effectDispatcher_,
            projectiles_,
            magic_,
            allowBossCapture,
            bossCaptureObjectId,
            &effectDiscoveries,
            &encyclopedia_);
        for (Vec2 explosionPosition : digging_.capturedExplosionRequests()) {
            handleCapturedExplosion(explosionPosition);
        }
        updateCapturedUtilityBehaviors(time.deltaSeconds());
        updateCapturedProjectileBehaviors(time.deltaSeconds());
        projectiles_.update(
            player_,
            spellRing_,
            enemies_,
            tileMap_,
            time.deltaSeconds(),
            effectDispatcher_,
            objectCatalog_,
            &effectDiscoveries,
            &encyclopedia_);
        magic_.update(player_, spellRing_, enemies_, tileMap_, time.deltaSeconds());
        bool projectileImpactSound = false;
        bool ringGuardSound = false;
        bool ringReflectSound = false;
        for (ProjectileSoundEvent event : projectiles_.consumeSoundEvents()) {
            switch (event) {
            case ProjectileSoundEvent::Impact:
                projectileImpactSound = true;
                break;
            case ProjectileSoundEvent::Guard:
                ringGuardSound = true;
                break;
            case ProjectileSoundEvent::Reflect:
                ringReflectSound = true;
                break;
            }
        }
        if (projectileImpactSound) {
            playAudioSe(AudioSeProjectileImpact);
        }
        if (ringGuardSound) {
            playAudioSe(AudioSeRingGuard);
        }
        if (ringReflectSound) {
            playAudioSe(AudioSeRingReflect);
        }
        bool magicCastSound = false;
        bool magicImpactSound = false;
        for (MagicSoundEvent event : magic_.consumeSoundEvents()) {
            switch (event) {
            case MagicSoundEvent::Cast:
                magicCastSound = true;
                break;
            case MagicSoundEvent::Impact:
                magicImpactSound = true;
                break;
            }
        }
        if (magicCastSound) {
            playAudioSe(AudioSeMagicCast);
        }
        if (magicImpactSound) {
            playAudioSe(AudioSeMagicImpact);
        }
        bool capturedEnemyThisFrame = false;
        for (const CaptureResult& capture : enemies_.consumeCaptureResults()) {
            capturedEnemyThisFrame = handleCaptureResult(capture) || capturedEnemyThisFrame;
        }
        updateCaptureAbsorbAnimations(time.deltaSeconds());
        updateDungeonMinimap(time.totalSeconds());
        handleRingItemBreakEvents(&effectDiscoveries);

        std::vector<Vec2> capturedExplosionPositions;
        for (const EnemyEvent& event : enemies_.events()) {
            if (!event.enemyId.empty()) {
                encyclopedia_.noteEnemyDiscovered(event.enemyId, event.enemyName, event.position);
            }
            if (event.type == EnemyEventType::CapturedExplosion) {
                capturedExplosionPositions.push_back(event.position);
            }
        }
        for (Vec2 explosionPosition : capturedExplosionPositions) {
            handleCapturedExplosion(explosionPosition);
        }

        bool bossDefeated = false;
        Vec2 bossDefeatPosition{};
        const std::vector<RingImpactSoundPlayback> enemyImpactSounds =
            resolveRingImpactSoundEvents(enemies_.impactSoundEvents(), lootRuntimeRng(), 4);
        for (const RingImpactSoundPlayback& sound : enemyImpactSounds) {
            playAudioSe(sound.cueId, sound.volumeScale, sound.pitchScale);
        }
        for (const EnemyEvent& event : enemies_.events()) {
            if (event.type == EnemyEventType::Spawn) {
                playAudioSe(AudioSeEnemySpawn);
            } else if (event.type == EnemyEventType::Alert) {
                playAudioSe(AudioSeEnemyAlert);
            } else if (event.type == EnemyEventType::Attack) {
                playAudioSe(AudioSeEnemyAttack);
            } else if (event.type == EnemyEventType::Shoot) {
                playAudioSe(AudioSeEnemyShoot);
            } else if (event.type == EnemyEventType::HealCast) {
                playAudioSe(AudioSeEnemyHeal);
                magicFx_.playHealPulse(event.position, 24.0f);
            } else if (event.type == EnemyEventType::Heal) {
                if (event.healAmount > 0) {
                    effects_.spawnDamagePopup(event.position, event.healAmount, DamagePopupStyle::Heal);
                }
                magicFx_.playHealPulse(event.position, 18.0f);
            } else if (event.type == EnemyEventType::Explode) {
                playAudioSe(AudioSeExplosion);
                addScreenShake(5.0f, 0.18f);
            } else if (event.type == EnemyEventType::BossTelegraph) {
                SmokeBurstOptions smoke;
                smoke.count = 18;
                smoke.size = 26.0f;
                smoke.spreadRadius = 18.0f;
                smoke.speed = 34.0f;
                smoke.riseSpeed = 22.0f;
                smoke.duration = 0.72f;
                smoke.colorA = {176, 128, 72, 205};
                smoke.colorB = {92, 68, 50, 172};
                effects_.spawnSmokeBurst(event.position, smoke);
            } else if (event.type == EnemyEventType::BossImpact) {
                addScreenShake(event.effectId == "wall_stun" ? 5.0f : 3.0f, 0.16f);
                const bool wallStunImpact = event.effectId == "wall_stun";
                const bool burrowImpact = event.effectId == "burrow";
                SmokeBurstOptions smoke;
                smoke.count = wallStunImpact ? 14 : (burrowImpact ? 16 : 10);
                smoke.size = wallStunImpact ? 24.0f : (burrowImpact ? 22.0f : 18.0f);
                smoke.spreadRadius = wallStunImpact ? 16.0f : (burrowImpact ? 14.0f : 10.0f);
                smoke.colorA = {192, 144, 82, 192};
                smoke.colorB = {110, 82, 58, 150};
                effects_.spawnSmokeBurst(event.position, smoke);
                if (wallStunImpact) {
                    playAudioSe(AudioSeAttackHit);
                }
            } else if (event.type == EnemyEventType::TerrainBreak) {
                effects_.spawnTileBreak(event.position, TileType::Dirt);
            } else if (event.type == EnemyEventType::Inspected) {
                const auto enemyIt = enemyCatalog_.enemiesById.find(event.enemyId);
                if (enemyIt != enemyCatalog_.enemiesById.end() &&
                    encyclopedia_.noteEnemyInspected(enemyIt->second, event.position)) {
                    playAudioSe(AudioSeDiscovery);
                }
            } else if (event.type == EnemyEventType::Death || event.type == EnemyEventType::BossDeath) {
                handleDungeonEventEnemyEvent(event);
                ++runStats_.defeatedEnemies;
                effects_.spawnEnemyDeath(event.position);
                addScreenShake(event.type == EnemyEventType::BossDeath ? 8.0f : 1.5f, event.type == EnemyEventType::BossDeath ? 0.28f : 0.08f);
                if (event.type == EnemyEventType::Death && !capturedEnemyThisFrame) {
                    playAudioSe(AudioSeEnemyDefeat);
                }
                std::mt19937& rng = lootRuntimeRng();
                if (event.moneyDrop > 0) {
                    worldDrops_.spawnMoneyDrop(
                        event.moneyDrop,
                        scatterLootPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng));
                }
                const bool bossDeath = event.type == EnemyEventType::BossDeath;
                const float manaChance = bossDeath ? balance_.bossManaDropChance : balance_.enemyManaDropChance;
                const float moonChance = bossDeath ? balance_.bossMoonFragmentChance : balance_.enemyMoonFragmentChance;
                if (rollChance(manaChance, rng)) {
                    const int amount = bossDeath ? scaledLootAmount(std::uniform_int_distribution<int>(1, 3)(rng), 1.0f) : 1;
                    worldDrops_.spawnMaterialDrop(
                        MaterialType::ManaDrop,
                        amount,
                        scatterLootPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng));
                }
                if (rollChance(moonChance, rng)) {
                    const int amount = bossDeath ? scaledLootAmount(std::uniform_int_distribution<int>(1, 3)(rng), 1.0f) : 1;
                    worldDrops_.spawnMaterialDrop(
                        MaterialType::MoonFragment,
                        amount,
                        scatterLootPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng));
                }
                if (!event.enemyId.empty()) {
                    encyclopedia_.noteEnemyDefeated(event.enemyId, event.enemyName, event.position);
                }
                if (event.type == EnemyEventType::BossDeath) {
                    bossDefeated = true;
                    bossDefeatPosition = event.position;
                }
            } else if (event.type == EnemyEventType::RewardDrop) {
                std::mt19937& rng = lootRuntimeRng();
                WeightedObjectLootProfile lootProfile;
                if (!weightedObjectLootProfileForDropProfile(event.objectDropProfile, lootProfile)) {
                    logError("[warning] CapturedRewardLoot: unknown reward profile \"" + event.objectDropProfile + "\"; no item drop");
                    continue;
                }
                const int depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, event.position);
                spawnWeightedObjectLoot(
                    lootProfile.chestKind,
                    depthRank,
                    event.position,
                    rng,
                    "CapturedRewardLoot",
                    true,
                    LootSourceKind::CapturedReward,
                    lootProfile.requiredTag);
            } else if (event.type == EnemyEventType::MoneyDrop) {
                if (event.moneyDrop <= 0) {
                    continue;
                }
                std::mt19937& rng = lootRuntimeRng();
                worldDrops_.spawnMoneyDrop(
                    event.moneyDrop,
                    scatterLootPosition(event.position, rng),
                    runStats_.elapsedSeconds,
                    makeWorldLootJumpMotion(event.position, rng));
            } else if (event.type == EnemyEventType::MaterialDrop) {
                if (event.materialDropType == MaterialType::Count || event.materialDropCount <= 0) {
                    logError("[warning] EnemyMaterialDrop: invalid material drop event; no material drop");
                    continue;
                }
                std::mt19937& rng = lootRuntimeRng();
                worldDrops_.spawnMaterialDrop(
                    event.materialDropType,
                    event.materialDropCount,
                    event.position,
                    runStats_.elapsedSeconds,
                    makeWorldLootJumpMotion(event.position, rng));
            } else if (event.type == EnemyEventType::ObjectDrop) {
                std::mt19937& rng = lootRuntimeRng();
                const int dropCount = std::max(1, event.objectDropCount);
                if (!event.objectDropProfile.empty()) {
                    WeightedObjectLootProfile lootProfile;
                    if (!weightedObjectLootProfileForDropProfile(event.objectDropProfile, lootProfile)) {
                        logError("[warning] EnemyDropLoot: unknown drop profile \"" + event.objectDropProfile + "\"; no item drop");
                    } else {
                        const int depthRank = lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, event.position);
                        for (int i = 0; i < dropCount; ++i) {
                            spawnWeightedObjectLoot(
                                lootProfile.chestKind,
                                depthRank,
                                event.position,
                                rng,
                                "EnemyDropLoot",
                                true,
                                LootSourceKind::EnemyDrop,
                                lootProfile.requiredTag);
                        }
                    }
                } else if (!event.objectDropId.empty()) {
                    if (event.objectDropInstance) {
                        worldDrops_.spawnObjectInstanceDrop(
                            objectCatalog_,
                            *event.objectDropInstance,
                            scatterLootPosition(event.position, rng),
                            runStats_.elapsedSeconds,
                            makeWorldLootJumpMotion(event.position, rng));
                    } else {
                        for (int i = 0; i < dropCount; ++i) {
                            worldDrops_.spawnObjectDrop(
                                objectCatalog_,
                                event.objectDropId,
                                scatterLootPosition(event.position, rng),
                                runStats_.elapsedSeconds,
                                makeWorldLootJumpMotion(event.position, rng));
                        }
                    }
                } else if (event.objectDropInstance) {
                    worldDrops_.spawnObjectInstanceDrop(
                        objectCatalog_,
                        *event.objectDropInstance,
                        scatterLootPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng));
                }
            } else if (event.type == EnemyEventType::CapturedExplosion) {
                continue;
            } else if (event.type == EnemyEventType::AttackHit) {
                if (!event.ringItemImpact) {
                    playAudioSe(AudioSeAttackHit);
                }
                effects_.spawnEnemyHit(event.position, event.effectId);
                if (event.damageAmount >= 0) {
                    effects_.spawnDamagePopup(
                        event.position,
                        event.damageAmount,
                        event.critical ? DamagePopupStyle::Critical : DamagePopupStyle::Enemy);
                }
            } else if (event.damageAmount >= 0) {
                effects_.spawnDamagePopup(event.position, event.damageAmount, DamagePopupStyle::Enemy);
            }
        }
        int playerDamageTotal = 0;
        for (const PlayerDamageEvent& event : player_.damageEvents) {
            effects_.spawnDamagePopup(event.position, event.amount, DamagePopupStyle::Player);
            playerDamageTotal += std::max(0, event.amount);
        }
        if (!player_.damageEvents.empty()) {
            playAudioSe(AudioSePlayerDamage);
            if (shouldPlayPlayerPinchDamageSe(player_.hp, player_.maxHp, playerDamageTotal)) {
                playAudioSe(AudioSePlayerPinch);
            }
            addScreenShake(4.5f, 0.16f);
            addPlayerDamageVignetteFlash(playerDamageTotal);
        }
        player_.damageEvents.clear();
        for (const PlayerHealEvent& event : player_.healEvents) {
            effects_.spawnDamagePopup(event.position, event.amount, DamagePopupStyle::Heal);
        }
        player_.healEvents.clear();
        for (const StatusPopupEvent& event : enemies_.consumeStatusPopupEvents()) {
            effects_.spawnStatusPopup(event.position, event.stateId, event.target);
        }
        for (const StatusPopupEvent& event : projectiles_.consumeStatusPopupEvents()) {
            effects_.spawnStatusPopup(event.position, event.stateId, event.target);
        }
        for (const StatusPopupEvent& event : inventory_.consumeStatusPopupEvents()) {
            effects_.spawnStatusPopup(event.position, event.stateId, event.target);
        }
        applyEffectDiscoveries(effectDiscoveries);
        syncEncyclopediaFromInventoryAndRing();
        updateAmbientParticleEffects(time.deltaSeconds());
        wetGround_.update(time.deltaSeconds());
        wetGround_.erasePendingGroundLines(groundLines_);
        groundLines_.update(time.deltaSeconds());
        magicFx_.update(time.deltaSeconds());
        effects_.update(time.deltaSeconds());
        gainPlayerXp(enemies_.consumePendingXp());
        if (updateIntroTutorial(input, time.deltaSeconds())) {
            return;
        }
        if (bossDefeated) {
            beginBossDefeatSequence(bossDefeatPosition);
            return;
        }
        if (player_.hp <= 0) {
            enterGameOver();
            return;
        }
        if (levels_.isChoosing()) {
            openLevelUpChoice(ScreenMode::Playing);
        }
    }
}

void Game::checkHotReload(float dt)
{
    if (!hotReloadEnabled_ || autoReloadBlocked_) {
        return;
    }

    hotReloadPollTimer_ = std::max(0.0f, hotReloadPollTimer_ - std::max(0.0f, dt));
    if (hotReloadPollTimer_ > 0.0f) {
        return;
    }
    hotReloadPollTimer_ = HotReloadPollIntervalSeconds;

    std::string changedPath;
    if (!watcher_.poll(changedPath)) {
        return;
    }

    std::string message;
    bool reloaded = false;
    const std::string fileName = filenameOf(changedPath);

    if (fileName == "google_sheet_source.cfg") {
        loadSheetSourceConfig();
        reloaded = loadBalanceFromSources(message);
    } else if (fileName == "game_balance.cfg") {
        reloaded = loadBalanceFromDisk(message);
    } else if (fileName == "opening_kamishibai.tsv") {
        loadOpeningKamishibaiData();
        if (mode_ == ScreenMode::OpeningKamishibai) {
            openingPlayer_.start(openingPages_, openingMeta_.openingEverWatched);
        }
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        configureWatcher();
        return;
    } else if (fileName == "ending_kamishibai.tsv") {
        loadEndingKamishibaiData();
        if (mode_ == ScreenMode::EndingKamishibai) {
            endingPlayer_.start(endingPages_, hasStoryFlag(EndingSeenFlag));
        }
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        configureWatcher();
        return;
    } else if (fileName == "enemy_hitboxes.cfg") {
        loadEnemyHitboxData();
        enemies_.setHitboxCatalog(&enemyHitboxes_);
        rebuildEnemyHitboxEditList();
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        configureWatcher();
        return;
    } else if (std::filesystem::path(changedPath).extension() == ".story") {
        loadStoryEvents();
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        configureWatcher();
        return;
    } else {
        reloaded = loadBalanceFromSources(message);
    }

    if (reloaded) {
        player_.xpToNext = playerXpToNextForLevel(player_.level, balance_);
        if (playerAtMaxLevel(player_)) {
            player_.xp = 0;
        }
        worldDrops_.setDropLimit(balance_.worldDropLimitPerStage);
        applyPermanentUpgrades();
        refreshOrbitEffects();
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        configureWatcher();
        reloadNotice_ = "Hot reload: " + changedPath;
    } else {
        reloadNotice_ = message;
    }
    reloadNoticeTimer_ = 3.0f;
}

} // namespace majo
