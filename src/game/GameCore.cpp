#include "game/GameInternal.hpp"

#include "data/GameBalance.hpp"
#include "engine/Audio.hpp"
#include "engine/FrameProfiler.hpp"
#include "game/RingImpactSound.hpp"
#include "game/RingDisplayName.hpp"
#include "game/SpecialObjectRules.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <random>
#include <sstream>

namespace majo {

namespace {

constexpr float DungeonRingIntroDuration = 1.18f;
constexpr float DeathResultPreludeFadeOutSeconds = 0.45f;
constexpr float DeathResultPreludeBlackHoldSeconds = 1.0f;
constexpr float DeathResultPreludeStarFadeInSeconds = 0.85f;
constexpr float DeathResultWindowLeadSeconds = 20.0f / 60.0f;
constexpr float DeathResultExitTransitionHoldSeconds = 1.5f;
constexpr float HotReloadPollIntervalSeconds = 0.50f;
constexpr std::string_view DefaultShovelObjectId = "item_shovel";
constexpr std::string_view DefaultTorchObjectId = "item_torch";
constexpr std::string_view MagnifyingGlassObjectId = "item_magnifying_glass";
constexpr std::string_view TutorialAppleObjectId = "item_apple";
constexpr std::string_view MagicBookCategory = "魔導書";
constexpr std::string_view AudioBgmTitle = "bgm.title";
constexpr std::string_view AudioBgmBase = "bgm.base";
constexpr std::string_view AudioBgmDungeon = "bgm.dungeon";
constexpr std::string_view AudioSeTransition = "se.transition";
constexpr std::string_view AudioSePlayerDamage = "se.player.damage";
constexpr std::string_view AudioSePlayerPinch = "se.player.pinch";
constexpr std::string_view AudioSeRingThrow = "se.ring.throw";
constexpr std::string_view AudioSeEnemyAlert = "se.enemy.alert";
constexpr std::string_view AudioSeEnemyAttack = "se.enemy.attack";
constexpr std::string_view AudioSeEnemyShoot = "se.enemy.shoot";
constexpr std::string_view AudioSeJunkCrabThrow = "se.projectile.metal.launch";
constexpr std::string_view AudioSeEnemyHeal = "se.enemy.heal";
constexpr std::string_view AudioSeEnemyMimicBite = "se.enemy.mimic_bite";
constexpr std::string_view AudioSeRingSlowBite = "se.ring.slow_bite";
constexpr std::string_view AudioSeMagicCast = "se.magic.cast";
constexpr std::string_view AudioSeMagicImpact = "se.magic.impact";
constexpr std::string_view AudioSeCaptureFail = "se.capture.fail";
constexpr std::string_view AudioSeExplosionTick = "se.explosion.tick";
constexpr std::string_view AudioSeDiscovery = "se.discovery";
constexpr std::string_view AudioSeMoneyDrop = "se.money.drop";
constexpr std::string_view AudioSeMoneyArrive = "se.money.arrive";
constexpr std::string_view AudioSeEffectDiscovery = "se.discovery.effect";
constexpr std::string_view AudioSeMonsterDiscovery = "se.discovery.monster";
constexpr std::string_view AudioSeUiConfirm = "se.ui.confirm";
constexpr std::string_view AudioSeUiCancel = "se.ui.cancel";
constexpr std::string_view AudioSeUiError = "se.ui.error";
constexpr std::string_view AudioSeUiMenuOpen = "se.ui.menu_open";
constexpr std::string_view AudioSeUiTabSwitch = "se.ui.tab_switch";
constexpr std::string_view AudioSeUiBookOpen = "se.ui.book_open";
constexpr std::string_view AudioSeUiCursorMove = "se.ui.cursor_move";
constexpr std::string_view AudioSeUiItemMove = "se.ui.item_move";
constexpr std::string_view AudioSeUiItemUse = "se.ui.item_use";
constexpr std::string_view AudioSeUiEquip = "se.ui.equip";
constexpr std::string_view AudioSeUiRingPlace = "se.ui.ring_place";
constexpr std::string_view AudioSeUiUpgradeSelect = "se.ui.upgrade_select";
constexpr std::string_view AudioSeRingAppear = "se.ring.appear";
constexpr std::string_view AudioSeRingUnlockJingle = "se.ring.unlock.jingle";
constexpr std::string_view AudioSeDialogueAdvance = "se.dialogue.advance";
constexpr std::string_view AudioSeDialogueTextPlayer = "se.dialogue.text.player";
constexpr std::string_view AudioSeDialogueTextMonica = "se.dialogue.text.monica";
constexpr std::string_view AudioSeDialogueTextChicory = "se.dialogue.text.chicory";
constexpr std::string_view AudioSeDialogueTextAstragna = "se.dialogue.text.astragna";
constexpr std::string_view AudioSeDialogueTextLow = "se.dialogue.text.low";
constexpr std::string_view AudioSeDialogueTextMid = "se.dialogue.text.mid";
constexpr std::string_view AudioSeDialogueTextHigh = "se.dialogue.text.high";
constexpr std::string_view AudioSeStoryPhoneIncoming = "se.story.phone.incoming";
constexpr std::string_view AudioSeStoryPhoneOutgoing = "se.story.phone.outgoing";
constexpr std::string_view AudioSeStoryPhoneHangup = "se.story.phone.hangup";
constexpr std::string_view AudioSeStoryRumble = "se.story.rumble";
constexpr std::string_view AudioSeLevelUpJingle = "se.level_up.jingle";
constexpr std::string_view AudioSeGameOverJingle = "se.game_over.jingle";
constexpr float StoryPhoneIncomingSeconds = 0.92f;
constexpr float StoryPhoneOutgoingSeconds = 0.68f;
constexpr float StoryPhoneHangupSeconds = 0.20f;
constexpr float ImpactSePitchJitterRatio = 0.10f;
constexpr float ExplosionRadiusScale = 1.5f;
constexpr std::string_view IntroTutorialChestLootInventoryTrigger = "intro_tutorial:chest_loot_inventory";
constexpr std::string_view IntroTutorialChestLootRingTrigger = "intro_tutorial:chest_loot_ring";
constexpr std::string_view LocalObjectsSnapshotPath = "Objects_with_rotation.tsv";
constexpr std::string_view LocalEnemiesSnapshotPath = ".tmp_enemies.csv";
constexpr std::string_view LocalEnemyBehaviorsSnapshotPath = ".tmp_behaviors.csv";
constexpr float PlayerDamageVignetteStartHpRatio = 0.70f;
constexpr float PlayerDamageVignetteMaxHpRatio = 0.20f;
constexpr float PlayerDamageVignetteFlashDecayPerSecond = 3.4f;
constexpr float PlayerPinchSeHpRatio = 1.0f / 3.0f;
constexpr float LevelUpPresentationMinSeconds = 90.0f / 60.0f;
constexpr float LevelUpJingleFallbackSeconds = 1.18f;
constexpr float GameOverJingleFallbackSeconds = 1.35f;
constexpr float StoryRingUnlockJingleFallbackSeconds = 1.28f;
constexpr std::array<float, SpellRingCount> RingWorkshopRadiusMaxMetersPerLevel{{0.12f, 0.18f, 0.24f}};
constexpr std::array<float, SpellRingCount> RingWorkshopRadiusMinMetersPerLevel{{0.08f, 0.12f, 0.16f}};
constexpr std::array<float, SpellRingCount> RingWorkshopSpeedMetersPerSecondPerLevel{{0.20f, 0.30f, 0.25f}};
constexpr float RingWorkshopWeightLimitKgPerLevel = 1.0f;
constexpr float RingWorkshopShiftDistanceMetersPerLevel = 0.50f;
constexpr float RingWorkshopThrowDistanceMetersPerLevel = 0.40f;
constexpr float RingWorkshopThrowCooldownSecondsPerLevel = 0.18f;

std::string_view dialogueTextSoundCueForSpeaker(std::string_view speakerId)
{
    if (speakerId == "player") {
        return AudioSeDialogueTextPlayer;
    }
    if (speakerId == "monica") {
        return AudioSeDialogueTextMonica;
    }
    if (speakerId == "chicory") {
        return AudioSeDialogueTextChicory;
    }
    if (speakerId == "astragna") {
        return AudioSeDialogueTextAstragna;
    }
    if (speakerId == "slime" || speakerId == "stardust_mole") {
        return AudioSeDialogueTextLow;
    }
    if (speakerId == "elder" || speakerId == "witch") {
        return AudioSeDialogueTextHigh;
    }
    return AudioSeDialogueTextMid;
}

float metersToWorldDistance(float meters)
{
    return meters * static_cast<float>(balance::TileSize);
}

float worldDistanceToMeters(float distance)
{
    return distance / static_cast<float>(balance::TileSize);
}

bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool impactSePitchJitterTarget(std::string_view id)
{
    if (startsWith(id, "se.impact.")) {
        return true;
    }
    if (id == "se.dig.hit" ||
        id == "se.dig.break" ||
        id == "se.dig.ore_break" ||
        id == "se.attack.hit" ||
        id == "se.player.damage" ||
        id == "se.enemy.mimic_bite" ||
        id == "se.enemy.guard" ||
        id == "se.ring.guard" ||
        id == "se.ring.reflect" ||
        id == "se.ring.slow_bite" ||
        id == "se.magic.impact" ||
        id == "se.projectile.impact" ||
        id == "se.projectile.bubble.pop" ||
        id == "se.crate.break" ||
        id == "se.item.break" ||
        id == "se.item.break.ceramic" ||
        id == "se.item.break.glass") {
        return true;
    }
    return startsWith(id, "se.projectile.") && endsWith(id, ".destroy");
}

float storyCommandFloatArg(const DialogueCommand& command, std::size_t index, float fallback)
{
    if (index >= command.args.size()) {
        return fallback;
    }
    try {
        std::size_t consumed = 0;
        const float value = std::stof(command.args[index], &consumed);
        return consumed == command.args[index].size() ? value : fallback;
    } catch (...) {
        return fallback;
    }
}

bool applyStoryShakeProfile(const DialogueCommand& command, float& amplitude, float& duration, std::string_view& soundId)
{
    if (command.args.empty()) {
        return false;
    }

    const std::string& profile = command.args[0];
    if (profile == "small") {
        amplitude = 3.0f;
        duration = 0.14f;
        soundId = {};
        return true;
    } else if (profile == "strong") {
        amplitude = 7.0f;
        duration = 0.72f;
        soundId = AudioSeStoryRumble;
        return true;
    } else if (profile == "boss") {
        amplitude = 8.0f;
        duration = 0.90f;
        soundId = AudioSeStoryRumble;
        return true;
    }
    return false;
}

bool storyJingleCueForCommand(const DialogueCommand& command, std::string_view& cueId, float& fallbackSeconds)
{
    if (command.args.empty()) {
        return false;
    }

    const std::string& profile = command.args[0];
    if (profile == "ring_unlock") {
        cueId = AudioSeRingUnlockJingle;
        fallbackSeconds = StoryRingUnlockJingleFallbackSeconds;
        return true;
    }
    if (profile == "level_up") {
        cueId = AudioSeLevelUpJingle;
        fallbackSeconds = LevelUpJingleFallbackSeconds;
        return true;
    }
    if (profile == "game_over") {
        cueId = AudioSeGameOverJingle;
        fallbackSeconds = GameOverJingleFallbackSeconds;
        return true;
    }
    if (profile.rfind("se.", 0) == 0) {
        cueId = profile;
        fallbackSeconds = storyCommandFloatArg(command, 1, StoryRingUnlockJingleFallbackSeconds);
        return true;
    }
    return false;
}

float applyImpactSePitchJitter(std::string_view id, float pitchScale)
{
    if (!impactSePitchJitterTarget(id)) {
        return pitchScale;
    }

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> distribution(
        1.0f - ImpactSePitchJitterRatio,
        1.0f + ImpactSePitchJitterRatio);
    return std::clamp(pitchScale * distribution(rng), 0.25f, 4.0f);
}

float angularSpeedForLinearMetersPerSecond(float speedMetersPerSecond, float radius)
{
    if (radius <= 0.001f) {
        return 0.0f;
    }
    return speedMetersPerSecond * static_cast<float>(balance::TileSize) / radius;
}

bool ensureInitialStaffEquipped(
    InventorySystem& inventory,
    const ObjectCatalog& objectCatalog,
    const SpellRingSystem& spellRing)
{
    if (objectCatalog.registry.findById(ApprenticeWitchStaffObjectId) == nullptr) {
        return false;
    }

    if (const InventoryObjectInstance* staff = inventory.equippedStaffInstance()) {
        if (isApprenticeWitchStaffObjectId(staff->item.id)) {
            return true;
        }
    }

    std::string equipStatus;
    if (inventory.equipStaffObject(ApprenticeWitchStaffObjectId, "", spellRing, &equipStatus)) {
        return true;
    }

    InventoryAddResult addResult;
    if (!inventory.addObjectItem(objectCatalog, ApprenticeWitchStaffObjectId, &addResult)) {
        return false;
    }

    const std::string_view instanceId =
        addResult.kind == InventoryAddKind::Instance ? std::string_view(addResult.instanceId) : std::string_view{};
    return inventory.equipStaffObject(ApprenticeWitchStaffObjectId, instanceId, spellRing, &equipStatus);
}

float linearMetersPerSecondForAngularSpeed(float angularSpeed, float radius)
{
    return angularSpeed * worldDistanceToMeters(radius);
}

void stripUtf8Bom(std::string& text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

std::string enemyStealLogLabel(const EnemyEvent& event, const ObjectCatalog& objectCatalog)
{
    if (event.moneyDrop > 0) {
        return inlineWorldIconTag(worldIconKey(moneyWorldIconForAmount(event.moneyDrop))) +
            std::to_string(event.moneyDrop) + "G";
    }
    if (!event.objectDropId.empty()) {
        const ObjectDefinition* object = objectCatalog.registry.findById(event.objectDropId);
        const ItemData* item = event.objectDropRuntimeItem ? &*event.objectDropRuntimeItem : object;
        const std::string name = item != nullptr && !item->name.empty() ? item->name : event.objectDropId;
        const std::string icon = object != nullptr ? inlineItemTag(event.objectDropId) : "";
        return icon + name;
    }
    return {};
}

DamagePopupStyle enemyDamagePopupStyle(const EnemyEvent& event)
{
    if (event.frontGuarded) {
        return DamagePopupStyle::Guard;
    }
    if (event.weakPointHit) {
        return DamagePopupStyle::WeakPoint;
    }
    if (event.critical) {
        return DamagePopupStyle::Critical;
    }
    return DamagePopupStyle::Enemy;
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

std::string normalizeTitleCreditsText(std::string text)
{
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    while (!text.empty() && (text.back() == '\n' || text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }
    if (!text.empty()) {
        return text;
    }
    return "ダンジョンを掘る魔女\n\n制作\nkagaya\n\n使用ライブラリ\nSDL3";
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

bool objectIdIsMagicBook(const ObjectCatalog& catalog, std::string_view objectId)
{
    if (objectId.empty()) {
        return false;
    }
    const ItemData* item = catalog.registry.findById(objectId);
    return item != nullptr && std::string_view(item->category) == MagicBookCategory;
}

bool objectIdIsEquippableStaff(const ObjectCatalog& catalog, std::string_view objectId)
{
    if (objectId.empty()) {
        return false;
    }
    const ItemData* item = catalog.registry.findById(objectId);
    return item != nullptr && isStaffObject(*item) && item->durability != 0;
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

std::vector<UiResultDialogLine> levelUpResultLines(
    RingLevelUpgradeSelection selection,
    float beforeValue,
    float afterValue,
    int unlockedRingCount)
{
    std::vector<UiResultDialogLine> lines;
    char prefix[128];
    char after[64];
    const std::string ringName = std::string(ringDisplayName(selection.ringIndex, unlockedRingCount));
    switch (selection.kind) {
    case RingLevelUpgradeKind::Radius:
        lines.push_back(levelUpResultTextLine(ringName + "のサイズが大きくなった！"));
        std::snprintf(prefix, sizeof(prefix), "%s 半径: %.2fm → ", ringName.c_str(), beforeValue);
        std::snprintf(after, sizeof(after), "%.2fm", afterValue);
        lines.push_back(levelUpResultChangeLine(prefix, after));
        break;
    case RingLevelUpgradeKind::Speed:
        lines.push_back(levelUpResultTextLine(ringName + "の速度が速くなった！"));
        std::snprintf(prefix, sizeof(prefix), "%s 速度: %.2fm/s → ", ringName.c_str(), beforeValue);
        std::snprintf(after, sizeof(after), "%.2fm/s", afterValue);
        lines.push_back(levelUpResultChangeLine(prefix, after));
        break;
    case RingLevelUpgradeKind::WeightLimit:
        lines.push_back(levelUpResultTextLine(ringName + "の重量上限が拡張された！"));
        std::snprintf(prefix, sizeof(prefix), "%s 重量上限: %.1fkg → ", ringName.c_str(), beforeValue);
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
    case ScreenMode::EnemyPlacementEdit:
    case ScreenMode::EnemyShadowEdit:
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
    return static_cast<float>(hp) / static_cast<float>(maxHp) <= PlayerPinchSeHpRatio;
}

bool shouldPlayPlayerPinchDamageSe(int hpAfter, int maxHp, int damageTaken)
{
    if (damageTaken <= 0 || maxHp <= 0 || hpAfter <= 0) {
        return false;
    }

    const int hpBefore = std::min(maxHp, hpAfter + damageTaken);
    return !isPlayerPinchHp(hpBefore, maxHp) && isPlayerPinchHp(hpAfter, maxHp);
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

void Game::beginStoryPhoneSound(StoryPhoneSoundKind kind)
{
    storyPhoneSound_ = {};
    storyPhoneSound_.kind = kind;
    switch (kind) {
    case StoryPhoneSoundKind::Incoming:
        storyPhoneSound_.durationSeconds = StoryPhoneIncomingSeconds;
        playAudioSe(AudioSeStoryPhoneIncoming);
        break;
    case StoryPhoneSoundKind::Outgoing:
        storyPhoneSound_.durationSeconds = StoryPhoneOutgoingSeconds;
        playAudioSe(AudioSeStoryPhoneOutgoing);
        break;
    case StoryPhoneSoundKind::Hangup:
        storyPhoneSound_.durationSeconds = StoryPhoneHangupSeconds;
        playAudioSe(AudioSeStoryPhoneHangup);
        break;
    case StoryPhoneSoundKind::None:
        storyPhoneSound_ = {};
        break;
    }
}

void Game::updateStoryPhoneSound(float dt)
{
    if (!storyPhoneSoundActive()) {
        return;
    }
    storyPhoneSound_.elapsedSeconds += std::max(0.0f, dt);
    if (storyPhoneSound_.elapsedSeconds >= storyPhoneSound_.durationSeconds) {
        storyPhoneSound_ = {};
    }
}

bool Game::storyPhoneSoundActive() const
{
    return storyPhoneSound_.kind != StoryPhoneSoundKind::None;
}

void Game::beginStoryShakeCommand(float amplitude, float duration, std::string_view soundId)
{
    storyShakeCommand_ = {};
    storyShakeCommand_.active = duration > 0.0f;
    storyShakeCommand_.durationSeconds = std::max(0.0f, duration);
    addScreenShake(amplitude, duration);
    if (!soundId.empty()) {
        playAudioSe(soundId);
    }
}

void Game::updateStoryShakeCommand(float dt)
{
    if (!storyShakeCommandActive()) {
        return;
    }
    storyShakeCommand_.elapsedSeconds += std::max(0.0f, dt);
    if (storyShakeCommand_.elapsedSeconds >= storyShakeCommand_.durationSeconds) {
        storyShakeCommand_ = {};
    }
}

bool Game::storyShakeCommandActive() const
{
    return storyShakeCommand_.active;
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
    if (handleOperationSettingsEvent(event)) {
        return true;
    }
    if (handleAudioCueEditEvent(event)) {
        return true;
    }
    if (handleDebugNamedSaveEvent(event)) {
        return true;
    }
    if (handleDebugItemPickerEvent(event)) {
        return true;
    }
    if (handleObjectImageScaleEditEvent(event)) {
        return true;
    }
    if (handlePortraitExpressionEditEvent(event)) {
        return true;
    }
    if (handleEnemyHitboxEditEvent(event)) {
        return true;
    }
    if (handleEnemyPlacementEditEvent(event)) {
        return true;
    }
    if (handleEnemyShadowEditEvent(event)) {
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
    audio_->playSe(id, volumeScale, applyImpactSePitchJitter(id, pitchScale));
}

void Game::playAudioSeAt(std::string_view id, Vec2 worldPosition, float volumeScale, float pitchScale)
{
    if (audio_ == nullptr || id.empty()) {
        return;
    }
    audio_->playSe(
        id,
        AudioSeParams{
            .volumeScale = volumeScale,
            .pitchScale = applyImpactSePitchJitter(id, pitchScale),
            .pan = audioPanForWorldPosition(worldPosition),
        });
}

float Game::audioPanForWorldPosition(Vec2 worldPosition) const
{
    const int viewportWidth = std::max(1, camera_.width());
    const Vec2 screenPosition = camera_.worldToScreen(worldPosition);
    const float halfWidth = std::max(1.0f, static_cast<float>(viewportWidth) * 0.5f);
    constexpr float MaxPan = 0.78f;
    return std::clamp((screenPosition.x - halfWidth) / halfWidth, -1.0f, 1.0f) * MaxPan;
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
    if (ui.soundEventCount(UiSoundEvent::CursorMove) > 0) {
        playAudioSe(AudioSeUiCursorMove);
    }
    if (ui.soundEventCount(UiSoundEvent::ItemMove) > 0) {
        playAudioSe(AudioSeUiItemMove);
    }
    if (ui.soundEventCount(UiSoundEvent::ItemUse) > 0) {
        playAudioSe(AudioSeUiItemUse);
    }
    if (ui.soundEventCount(UiSoundEvent::Equip) > 0) {
        playAudioSe(AudioSeUiEquip);
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
    if (ui.soundEventCount(UiSoundEvent::Error) > 0) {
        playAudioSe(AudioSeUiError);
    }
}

std::string Game::crashContextSummary() const
{
    std::ostringstream out;
    out << "game_initialize_active=" << (initializeJob_.active ? "true" : "false") << "\n";
    out << "game_initialize_step=" << initializeStepIndex() << "/" << initializeStepCount() << "\n";
    out << "game_initialize_status=" << initializeStatusText() << "\n";
    out << "screen_mode=" << screenModeName(mode_) << "\n";
    out << "pause_return_mode=" << screenModeName(pauseReturnMode_) << "\n";
    out << "current_stage_id=" << currentStageId_ << "\n";
    out << "current_stage_index=" << currentStage_ << "\n";
    out << "player_position=" << player_.position.x << "," << player_.position.y << "\n";
    out << "player_hp=" << player_.hp << "/" << player_.maxHp << "\n";
    out << "player_level=" << player_.level << "\n";
    out << "roguelike_dungeon=" << (roguelikeDungeon_ ? "true" : "false") << "\n";
    out << "roguelike_depth_meters=" << astralRun_.currentDepthMeters << "-" << astralRun_.nextHoleDepthMeters << "\n";
    out << "roguelike_max_reached_meters=" << astralRun_.maxReachedDepthMeters << "\n";
    out << "roguelike_depth_rank=" << astralRun_.currentDepth << "\n";
    out << "world_build_active=" << (worldBuildJob_.active ? "true" : "false") << "\n";
    out << "world_build_step=" << worldBuildStepIndex() << "/" << worldBuildStepCount() << "\n";
    out << "world_build_status=" << worldBuildStatusText() << "\n";
    out << "run_elapsed_seconds=" << runStats_.elapsedSeconds << "\n";
    out << "run_defeated_enemies=" << runStats_.defeatedEnemies << "\n";
    out << "run_dug_tiles=" << runStats_.dugTiles << "\n";
    out << "quit_requested=" << (quitRequested_ ? "true" : "false") << "\n";
    return out.str();
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
            updateBasePlayerSpriteFlipFromFacing();
        }
        initializeJob_.step = InitializeStep::LoadImageScale;
        break;
    case InitializeStep::LoadImageScale:
        loadObjectImageScaleData();
        setObjectImageScaleOverrides(&objectImageScaleById_);
        setWorldIconScaleOverrides(&otherImageScaleByKey_);
        setUiMenuIconScaleOverrides(&otherImageScaleByKey_);
        initializeJob_.step = InitializeStep::LoadHitboxes;
        break;
    case InitializeStep::LoadHitboxes:
        loadHitboxData();
        loadEnemyPlacementData();
        loadEnemyShadowData();
        bindWorldEnemyCatalogs();
        initializeJob_.step = InitializeStep::LoadOpening;
        break;
    case InitializeStep::LoadOpening:
        loadOpeningKamishibaiData();
        loadTitleCreditsData();
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
    case InitializeStep::LoadHitboxes: return 12;
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
    case InitializeStep::LoadHitboxes:
        return "Loading hitboxes";
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
    moneyGainFx_.clear();
    ringTrailEffectTimer_ = 0.0f;
    ambientParticleTimer_ = 0.0f;
}

void Game::bindWorldEnemyCatalogs()
{
    enemies_.setHitboxCatalog(&hitboxes_);
    enemies_.setPlacementCatalog(&enemyPlacements_);
    enemies_.setShadowCatalog(&enemyShadows_);
}

void Game::resetWorldEnemyState()
{
    resetInPlace(enemies_);
    bindWorldEnemyCatalogs();
    pendingBuriedEnemySpawns_.clear();
}

void Game::restoreWorldEnemyState(const EnemySystem& state)
{
    enemies_ = state;
    bindWorldEnemyCatalogs();
    enemies_.clearTemporaryState();
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
    playerDeathSequence_ = {};
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
    baseRoguelikeDepartureConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    baseMiningRescueDrop_ = {};
    baseWarpPointSelectActive_ = false;
    baseWarpPointSelection_ = 0;
    warpReturnConfirm_ = {};
    focusedWarpReturnPointIndex_ = -1;
    hoveredWarpReturnPointIndex_ = -1;
    focusedRoguelikeFacilityIndex_ = -1;
    hoveredRoguelikeFacilityIndex_ = -1;
    closeRoguelikeFacilityUi();
    introTutorialExitHovered_ = false;
    resetDungeonFocus();
    clearDungeonStoryPresentation();
    storyPhoneSound_ = {};
    storyShakeCommand_ = {};
    baseStorageActive_ = false;
    baseStorageMode_ = StorageUiMode::Closed;
    baseStorageActionSelection_ = 0;
    baseStorageBulkSelection_ = 0;
    baseStorageDepositSource_ = static_cast<int>(BaseItemSource::Backpack);
    baseStorageDepositSourceTabs_ = {};
    baseStorageDepositSelection_ = 0;
    baseStorageWithdrawSelection_ = 0;
    baseStorageWarehousePage_ = 0;
    baseQuantityDialog_ = {};
    baseQuantityPending_ = {};
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
    dungeonMapOverlayOpen_ = false;
    dungeonMapOverlayScroll_ = {};
    baseProcessingUiMode_ = ProcessingUiMode::Closed;
    baseProcessingActionSelection_ = 0;
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
    ringDiscardConfirm_ = {};
    ringDiscardConfirmItemIndex_ = -1;
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
    baseRingWorkshopUpgradeTabs_ = {};
    baseRingWorkshopUpgradeScrollOffset_ = 0.0f;
    baseRingWorkshopUpgradeScroll_ = {};
    ringWorkshopDraftUpgradePoints_ = levelRingUpgradePoints_;
    baseBookshelfActive_ = false;
    bookshelfPage_ = BookshelfPage::Menu;
    bookshelfSelection_ = 0;
    bookshelfScrollOffset_ = 0.0f;
    bookshelfScrollState_ = {};
    closeUiCommandMenu(bookshelfEndingCommandMenu_);
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
    hitboxEditTab_ = HitboxEditTab::Enemies;
    enemyHitboxDirection_ = HitboxDirection::Default;
    enemyHitboxAllEnemyIds_.clear();
    enemyHitboxEnemyIds_.clear();
    objectHitboxAllObjectIds_.clear();
    objectHitboxObjectIds_.clear();
    playerHitboxAllIds_.clear();
    playerHitboxIds_.clear();
    enemyHitboxSearchInput_ = {};
    enemyHitboxSelectedEnemyIndex_ = -1;
    objectHitboxSelectedObjectIndex_ = -1;
    playerHitboxSelectedIndex_ = 0;
    enemyHitboxSelectedCircleIndex_ = -1;
    enemyHitboxScrollOffset_ = 0.0f;
    objectHitboxScrollOffset_ = 0.0f;
    playerHitboxScrollOffset_ = 0.0f;
    enemyHitboxDirty_ = false;
    enemyHitboxDraggingCircle_ = false;
    enemyHitboxDragUndoSnapshotPushed_ = false;
    enemyHitboxDragStartMouse_ = {};
    enemyHitboxDragStartOffset_ = {};
    enemyHitboxClipboard_.clear();
    hitboxEditUndoStack_.clear();
    hitboxEditRedoStack_.clear();
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
    effectTestReplayTimerSeconds_ = 0.0f;
    effectTestScrollOffset_ = 0.0f;
    effectTestScrollState_ = {};
    effectTestEmitter_ = {};
    effectTestStatus_.clear();
    projectileTestActive_ = false;
    projectileTestEntries_.clear();
    projectileTestSelectedIndex_ = 0;
    projectileTestReplayTimerSeconds_ = 0.0f;
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
    resetDungeonRouteDeviation();
    gameOverSelection_ = 0;
    gameOverStatus_.clear();
    bossSpawned_ = false;
    bossPreviewSpawned_ = false;
    hasBossSpawnPoint_ = false;
    resetBossEncounter();
    clearRoguelikeBigHoleState();
    clearRoguelikeFacilities();
    clearHiddenDungeonNpcTargets();
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
    if (roguelikeDungeon_) {
        resetLevelRingUpgradePointsForRun();
    }
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
    initializeRoguelikeBigHoleFromLayout();
    initializeRoguelikeFacilitiesFromLayout();
    applyPlacementTerrainOverrides();
    initializeDefaultSpellRing();
    refreshEquipmentModifiers();
    applyPermanentUpgrades();
    spellRing_.applyObjectParameters(objectCatalog_);
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
    bool restoreRetainedInventory,
    int retainedLevel,
    int retainedXp,
    int retainedXpToNext)
{
    worldBuildJob_ = WorldBuildJob{};
    worldBuildJob_.active = true;
    worldBuildJob_.useLatestWarpPoint = useLatestWarpPoint;
    worldBuildJob_.restoreRetainedInventory = restoreRetainedInventory;
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
    baseRoguelikeDepartureConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    baseMiningRescueDrop_ = {};
    warpReturnConfirm_ = {};
    closeUiCommandMenu(roguelikeBigHoleMenu_);
    focusedWarpReturnPointIndex_ = -1;
    hoveredWarpReturnPointIndex_ = -1;
    focusedRoguelikeBigHole_ = 0;
    hoveredRoguelikeBigHole_ = false;
    focusedRoguelikeFacilityIndex_ = -1;
    hoveredRoguelikeFacilityIndex_ = -1;
    closeRoguelikeFacilityUi();
    introTutorialExitHovered_ = false;
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
        initializeRoguelikeBigHoleFromLayout();
        initializeRoguelikeFacilitiesFromLayout();
        applyPlacementTerrainOverrides();
        worldBuildJob_.step = WorldBuildStep::InitializeRing;
        break;
    case WorldBuildStep::InitializeRing:
        initializeDefaultSpellRing();
        refreshEquipmentModifiers();
        applyPermanentUpgrades();
        spellRing_.applyObjectParameters(objectCatalog_);
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

    if (job.restoreRetainedInventory) {
        restoreInventoryCarryState(job.retainedInventory);
    } else {
        roguelikeReturnInventoryState_ = job.retainedInventory;
        money_ = 0;
        inventory_.setOpen(false);
        inventory_.cancelGrab();
        spellRing_.applyObjectParameters(objectCatalog_);
        spellRing_.normalizeItemPlacements();
        observeRingItemInstanceIds();
        refreshOrbitEffects();
    }
    player_.level = job.retainedLevel;
    player_.xp = job.retainedXp;
    player_.xpToNext = job.retainedXpToNext;
    refreshEquipmentModifiers();
    applyPermanentUpgrades();
    clearTemporaryPlayerState(true);
    captureRunStartInventoryState();

    initializeRoguelikeBigHoleFromLayout();
    initializeRoguelikeFacilitiesFromLayout();
    applyPlacementTerrainOverrides();
    if (job.useLatestWarpPoint) {
        const Vec2 warpStartPosition = warpPointStartPositionForCurrentRequest();
        rebuildUnlockedWarpPointsForStart(warpStartPosition);
        player_.position = safePlayerStartPosition(warpStartPosition);
        disarmWarpReturnInteractionAt(warpStartPosition);
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        updateDungeonMinimap(0.0);
    }
    if (job.useLatestWarpPoint) {
        captureRetrySnapshotAtWarpPoint();
    } else {
        disarmedWarpReturnPointIndex_ = -1;
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
    pendingStoryTrigger_.clear();
    pendingStoryTriggerDelaySeconds_ = 0.0f;
    pendingStoryTriggers_.clear();
    clearBaseTalkSessionSelections();
    pendingDialogueCompletion_ = {};
    dialogue_.clear();
    clearBaseStoryPresentation();
    storyPhoneSound_ = {};
    storyShakeCommand_ = {};
    inventory_.setOpen(false);
    inventory_.cancelGrab();
    cancelRingGrab();
    spellRing_.clearActionFlashTimers();
    closeDebugNamedSaveDialog();
    closeDebugNamedLoadDialog();
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
    baseRoguelikeDepartureConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    warpReturnConfirm_ = {};
    focusedWarpReturnPointIndex_ = -1;
    hoveredWarpReturnPointIndex_ = -1;
    introTutorialExitHovered_ = false;
    baseStorageActive_ = false;
    baseStorageMode_ = StorageUiMode::Closed;
    baseStorageActionSelection_ = 0;
    baseStorageBulkSelection_ = 0;
    baseStorageDepositSource_ = static_cast<int>(BaseItemSource::Backpack);
    baseStorageDepositSourceTabs_ = {};
    baseStorageDepositSelection_ = 0;
    baseStorageWithdrawSelection_ = 0;
    baseStorageWarehousePage_ = 0;
    baseQuantityDialog_ = {};
    baseQuantityPending_ = {};
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
    dungeonMapOverlayOpen_ = false;
    dungeonMapOverlayScroll_ = {};
    baseProcessingUiMode_ = ProcessingUiMode::Closed;
    baseProcessingActionSelection_ = 0;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingCommandSlot_ = -1;
    baseRingWorkshopActive_ = false;
    baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
    baseRingWorkshopSelection_ = 0;
    baseRingWorkshopRingTabs_ = {};
    baseRingWorkshopUpgradeTabs_ = {};
    baseRingWorkshopUpgradeScrollOffset_ = 0.0f;
    baseRingWorkshopUpgradeScroll_ = {};
    baseBookshelfActive_ = false;
    bookshelfScrollOffset_ = 0.0f;
    bookshelfScrollState_ = {};
    closeUiCommandMenu(bookshelfEndingCommandMenu_);
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
    KamishibaiLoadResult result = loader.load(endingKamishibaiDataPath(endingKamishibaiKind_));
    endingPages_ = std::move(result.pages);
    logInfo("[ending] kamishibai pages loaded: " + std::to_string(endingPages_.size()));
    for (const std::string& warning : result.warnings) {
        logWarning("[ending] " + warning);
    }
}

void Game::loadTitleCreditsData()
{
    std::string text;
    std::string error;
    if (!readTextFile(titleCreditsDataPath(), text, error)) {
        logWarning("[title] credits load failed: " + error);
        titleCreditsText_ = normalizeTitleCreditsText({});
        titleCreditsScrollOffset_ = 0.0f;
        titleCreditsContentHeight_ = 0.0f;
        titleCreditsScrollState_ = {};
        return;
    }
    titleCreditsText_ = normalizeTitleCreditsText(std::move(text));
    titleCreditsScrollOffset_ = 0.0f;
    titleCreditsContentHeight_ = 0.0f;
    titleCreditsScrollState_ = {};
    logInfo("[title] credits loaded: " + titleCreditsDataPath().generic_string());
}

void Game::loadStoryEvents()
{
    StoryEventLoader loader;
    StoryEventLoadResult result = loader.loadDirectory(storyEventDataDirectory());
    storyEvents_ = std::move(result.events);
    clearBaseTalkSessionSelections();
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
    titleMenuPage_ = TitleMenuPage::Main;
    titleCreditsScrollOffset_ = 0.0f;
    titleCreditsScrollState_ = {};
    playAudioBgm(AudioBgmTitle, 0.25f);
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
}

void Game::updateOpeningKamishibai(const Input& input, float dt)
{
    bool skipped = false;
    if (openingPlayer_.canSkipImmediately() && input.useItemPressed()) {
        openingPlayer_.finishImmediately();
        skipped = true;
    } else if (input.mouseLeftPressed() || input.confirmPressed() || input.useItemPressed()) {
        openingPlayer_.advance();
    }

    openingPlayer_.update(dt);
    if (openingPlayer_.finished()) {
        finishOpeningKamishibai(!skipped);
    }
}

EndingKind Game::resolveEndingKamishibaiKind(EndingKind kind) const
{
    switch (kind) {
    case EndingKind::Main:
        if (hasStoryFlag(StoryTrustBrokenFlag)) {
            return EndingKind::MainFailedTrust;
        }
        if (hasStoryFlag(StoryHiddenOrbitCorruptionUnlockedFlag) || hasStoryFlag(HiddenEndingPeopleGoneFlag)) {
            return EndingKind::MainFailedMonicaMissing;
        }
        return EndingKind::Main;
    case EndingKind::EncyclopediaComplete:
        return hasStoryFlag(StoryTrustBrokenFlag)
            ? EndingKind::EncyclopediaFailedTrust
            : EndingKind::EncyclopediaComplete;
    case EndingKind::AstralClear:
        return EndingKind::AstralClear;
    case EndingKind::HiddenBad:
    case EndingKind::MainFailedTrust:
    case EndingKind::MainFailedMonicaMissing:
    case EndingKind::EncyclopediaFailedTrust:
    case EndingKind::AstralFailedTrust:
        return kind;
    }
    return kind;
}

void Game::requestEndingKamishibai(EndingKind kind)
{
    endingKamishibaiKind_ = resolveEndingKamishibaiKind(kind);
    endingKamishibaiPending_ = true;
    endingKamishibaiReplay_ = false;
}

void Game::startEndingKamishibai(EndingKind kind)
{
    startEndingKamishibaiPlayback(resolveEndingKamishibaiKind(kind), false);
}

void Game::startEndingReplayKamishibai(EndingKind kind)
{
    startEndingKamishibaiPlayback(kind, true);
}

void Game::startEndingKamishibaiPlayback(EndingKind kind, bool replay)
{
    endingKamishibaiKind_ = kind;
    endingKamishibaiReplay_ = replay;
    loadEndingKamishibaiData();
    const bool canSkipImmediately = [&]() {
        if (replay) {
            return true;
        }
        switch (kind) {
        case EndingKind::Main:
            return hasStoryFlag(StoryEndingSeenFlag);
        case EndingKind::EncyclopediaComplete:
            return hasStoryFlag(StoryEndingEncyclopediaCompleteFlag);
        case EndingKind::AstralClear:
            return hasStoryFlag(StoryEndingAstralClearFlag);
        case EndingKind::HiddenBad:
            return hasStoryFlag(StoryHiddenEndingEverythingOrbitsFlag);
        case EndingKind::MainFailedTrust:
            return hasStoryFlag(StoryEndingMainFailedTrustFlag);
        case EndingKind::MainFailedMonicaMissing:
            return hasStoryFlag(StoryEndingMainFailedMonicaMissingFlag);
        case EndingKind::EncyclopediaFailedTrust:
            return hasStoryFlag(StoryEndingEncyclopediaFailedTrustFlag);
        case EndingKind::AstralFailedTrust:
            return hasStoryFlag(StoryEndingAstralFailedTrustFlag);
        }
        return false;
    }();
    endingPlayer_.start(endingPages_, canSkipImmediately);
    mode_ = ScreenMode::EndingKamishibai;
    playAudioBgm(AudioBgmTitle, 0.65f);
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = replay
        ? ScreenMode::Base
        : ((kind == EndingKind::Main ||
                kind == EndingKind::MainFailedTrust ||
                kind == EndingKind::MainFailedMonicaMissing)
            ? ScreenMode::Playing
            : ScreenMode::Base);
    inventoryReturnToPause_ = false;
}

void Game::finishEndingKamishibai(bool)
{
    const EndingKind finishedKind = endingKamishibaiKind_;
    const bool replay = endingKamishibaiReplay_;
    endingKamishibaiPending_ = false;
    endingKamishibaiReplay_ = false;
    if (replay) {
        mode_ = ScreenMode::Base;
        playAudioBgm(AudioBgmBase, 0.45f);
        pausePage_ = PauseMenuPage::Main;
        pauseReturnMode_ = ScreenMode::Base;
        inventoryReturnToPause_ = false;
        closeBaseFacilityScreens();
        return;
    }
    switch (finishedKind) {
    case EndingKind::Main:
        addStoryFlag(std::string(StoryEndingSeenFlag));
        addStoryFlag(std::string(StoryEndingMainFlag));
        addStoryFlag("story_stage_03_clear");
        requestReturnToBaseTransition(true, false);
        return;
    case EndingKind::MainFailedTrust:
        addStoryFlag(std::string(StoryEndingMainFailedTrustFlag));
        addStoryFlag("story_stage_03_clear");
        requestReturnToBaseTransition(true, false);
        return;
    case EndingKind::MainFailedMonicaMissing:
        addStoryFlag(std::string(StoryEndingMainFailedMonicaMissingFlag));
        addStoryFlag("story_stage_03_clear");
        requestReturnToBaseTransition(true, false);
        return;
    case EndingKind::EncyclopediaComplete:
        addStoryFlag(std::string(StoryEndingEncyclopediaCompleteFlag));
        break;
    case EndingKind::AstralClear:
        addStoryFlag(std::string(StoryEndingAstralClearFlag));
        break;
    case EndingKind::HiddenBad:
        addStoryFlag(std::string(StoryHiddenEndingEverythingOrbitsFlag));
        addStoryFlag(std::string(HiddenEndingPeopleGoneFlag));
        break;
    case EndingKind::EncyclopediaFailedTrust:
        addStoryFlag(std::string(StoryEndingEncyclopediaFailedTrustFlag));
        break;
    case EndingKind::AstralFailedTrust:
        addStoryFlag(std::string(StoryEndingAstralFailedTrustFlag));
        break;
    }

    mode_ = ScreenMode::Base;
    playAudioBgm(AudioBgmBase, 0.45f);
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Base;
    inventoryReturnToPause_ = false;
    closeBaseFacilityScreens();
    if (finishedKind == EndingKind::HiddenBad) {
        std::string message;
        if (!saveSaveData(message)) {
            logError("[ending] hidden ending save failed: " + message);
        }
    }
}

void Game::updateEndingKamishibai(const Input& input, float dt)
{
    bool skipped = false;
    if (endingPlayer_.canSkipImmediately() && input.useItemPressed()) {
        endingPlayer_.finishImmediately();
        skipped = true;
    } else if (input.mouseLeftPressed() || input.confirmPressed() || input.useItemPressed()) {
        endingPlayer_.advance();
    }

    endingPlayer_.update(dt);
    if (endingPlayer_.finished()) {
        finishEndingKamishibai(!skipped);
    }
}

void Game::updateTitleScreen(const Input& input, UiContext& ui)
{
    if (screenTransition_.active()) {
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        return;
    }

    if (titleMenuPage_ == TitleMenuPage::Options) {
        const bool suppressCancelThisFrame = optionsSuppressCancelThisFrame_;
        optionsSuppressCancelThisFrame_ = false;
        const bool operationModalOpen = operationSettingsModalVisible();
        if (!operationModalOpen &&
            !suppressCancelThisFrame &&
            uiCancelRequested(titleCancelState_, input, ui, optionsMenuPanelRect())) {
            returnToTitleMain();
            return;
        }
        updateOptionsMenu(input, ui);
        return;
    }

    if (titleMenuPage_ == TitleMenuPage::Credits) {
        const UiRect panel = titleCreditsPanelRect();
        if (uiCancelRequested(titleCancelState_, input, ui, panel)) {
            returnToTitleMain();
            return;
        }

        const UiRect viewport = titleCreditsViewportRect();
        UiScrollAreaStyle scrollStyle;
        const UiScrollAreaLayout layout = updateUiScrollArea(
            ui,
            input,
            viewport,
            titleCreditsContentHeight_,
            titleCreditsScrollOffset_,
            scrollStyle,
            &titleCreditsScrollState_);

        const auto scrollBy = [&](float delta) {
            const float previousOffset = titleCreditsScrollOffset_;
            titleCreditsScrollOffset_ = clamp(
                titleCreditsScrollOffset_ + delta,
                0.0f,
                layout.maxScroll);
            if (titleCreditsScrollOffset_ != previousOffset) {
                ui.emitSound(UiSoundEvent::CursorMove);
            }
        };
        if (layout.scrollable) {
            if (input.pressed(InputAction::MoveUp)) {
                scrollBy(-36.0f);
            }
            if (input.pressed(InputAction::MoveDown)) {
                scrollBy(36.0f);
            }
            if (input.pressed(InputAction::MoveLeft)) {
                scrollBy(-viewport.size.y * 0.8f);
            }
            if (input.pressed(InputAction::MoveRight)) {
                scrollBy(viewport.size.y * 0.8f);
            }
        }
        ui.block(panel);
        return;
    }

    if (input.pressed(InputAction::OpenOptions) || ui.pressed(titleTopButtonRect(0))) {
        ui.emitSound(UiSoundEvent::Confirm);
        openTitleOptions();
        return;
    }
    if (input.pressed(InputAction::OpenCredits) || ui.pressed(titleTopButtonRect(1))) {
        ui.emitSound(UiSoundEvent::Confirm);
        openTitleCredits();
        return;
    }

    if (ui.pressed(titleStartPromptRect()) ||
        (!ui.navigationActive() && (input.confirmPressed() || input.useItemPressed())) ||
        (input.mouseLeftPressed() && !ui.pointerConsumed())) {
        if (input.mouseLeftPressed()) {
            ui.consumePointer();
        }
        ui.emitSound(UiSoundEvent::Confirm);
        startTitleGame();
        return;
    }

    ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
}

void Game::openTitleOptions()
{
    titleMenuPage_ = TitleMenuPage::Options;
    titleCancelState_ = {};
    prepareOptionsMenu();
}

void Game::openTitleCredits()
{
    titleMenuPage_ = TitleMenuPage::Credits;
    titleCreditsScrollOffset_ = 0.0f;
    titleCreditsScrollState_ = {};
    titleCancelState_ = {};
    if (titleCreditsText_.empty()) {
        loadTitleCreditsData();
    }
}

void Game::returnToTitleMain()
{
    titleMenuPage_ = TitleMenuPage::Main;
    titleCancelState_ = {};
    operationSettingsCapture_.cancel();
    operationSettingsConflictConfirm_ = {};
    operationSettingsResetAllConfirm_ = {};
}

void Game::startTitleGame()
{
    const bool needsIntroTutorial = !hasStoryFlag(IntroTutorialCompletedFlag);
    requestScreenTransition(needsIntroTutorial
        ? ScreenTransitionTarget::TitleToIntroTutorial
        : ScreenTransitionTarget::TitleToBase);
}

Game::ScreenTransitionFadeColor Game::fadeColorForScreenTransitionTarget(ScreenTransitionTarget target)
{
    switch (target) {
    case ScreenTransitionTarget::Base:
    case ScreenTransitionTarget::ReturnToBase:
    case ScreenTransitionTarget::IntroTutorialToBase:
    case ScreenTransitionTarget::FinalBossEndingKamishibai:
        return ScreenTransitionFadeColor::White;
    case ScreenTransitionTarget::None:
    case ScreenTransitionTarget::TitleToBase:
    case ScreenTransitionTarget::TitleToIntroTutorial:
    case ScreenTransitionTarget::MiningStart:
    case ScreenTransitionTarget::BaseArea:
    case ScreenTransitionTarget::BossEncounterIntro:
    case ScreenTransitionTarget::BossEncounterAfterDialogue:
    case ScreenTransitionTarget::GameOverRetry:
    case ScreenTransitionTarget::GameOverReturnToBase:
    case ScreenTransitionTarget::AstralDeathReturnToBase:
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
    case ScreenTransitionTarget::BossEncounterIntro:
    case ScreenTransitionTarget::BossEncounterAfterDialogue:
        return BossEncounterIntroTransitionHoldSeconds;
    case ScreenTransitionTarget::GameOverRetry:
    case ScreenTransitionTarget::GameOverReturnToBase:
    case ScreenTransitionTarget::AstralDeathReturnToBase:
        return DeathResultExitTransitionHoldSeconds;
    case ScreenTransitionTarget::None:
    case ScreenTransitionTarget::Base:
    case ScreenTransitionTarget::TitleToBase:
    case ScreenTransitionTarget::MiningStart:
    case ScreenTransitionTarget::ReturnToBase:
    case ScreenTransitionTarget::BaseArea:
    case ScreenTransitionTarget::FinalBossEndingKamishibai:
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
    case ScreenTransitionTarget::FinalBossEndingKamishibai:
    case ScreenTransitionTarget::BaseArea:
    case ScreenTransitionTarget::BossEncounterIntro:
    case ScreenTransitionTarget::BossEncounterAfterDialogue:
    case ScreenTransitionTarget::GameOverRetry:
    case ScreenTransitionTarget::GameOverReturnToBase:
    case ScreenTransitionTarget::AstralDeathReturnToBase:
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
    case ScreenTransitionTarget::FinalBossEndingKamishibai:
    case ScreenTransitionTarget::BaseArea:
    case ScreenTransitionTarget::BossEncounterIntro:
    case ScreenTransitionTarget::BossEncounterAfterDialogue:
    case ScreenTransitionTarget::GameOverRetry:
    case ScreenTransitionTarget::GameOverReturnToBase:
    case ScreenTransitionTarget::AstralDeathReturnToBase:
        return 0.0f;
    }
    return 0.0f;
}

void Game::startScreenTransition(
    ScreenTransitionTarget target,
    ScreenTransitionPhase phase,
    ScreenTransitionSound sound)
{
    screenTransition_.target = target;
    screenTransition_.phase = phase;
    screenTransition_.fadeColor = fadeColorForScreenTransitionTarget(target);
    screenTransition_.holdSeconds = holdSecondsForScreenTransitionTarget(target);
    screenTransition_.fadeInSeconds = fadeInSecondsForScreenTransitionTarget(target);
    screenTransition_.postTransitionStoryDelaySeconds = postTransitionStoryDelaySecondsForScreenTransitionTarget(target);
    screenTransition_.elapsed = 0.0f;
    screenTransition_.applied = false;
    playScreenTransitionSound(sound);
}

void Game::playScreenTransitionSound(ScreenTransitionSound sound)
{
    switch (sound) {
    case ScreenTransitionSound::Generic:
    case ScreenTransitionSound::DungeonLadder:
    case ScreenTransitionSound::WarpPoint:
    case ScreenTransitionSound::Home:
        playAudioSe(AudioSeTransition);
        return;
    }
}

void Game::requestScreenTransition(ScreenTransitionTarget target, ScreenTransitionSound sound)
{
    if (target == ScreenTransitionTarget::None || screenTransition_.active()) {
        return;
    }

    startScreenTransition(target, ScreenTransitionPhase::FadingOut, sound);
}

void Game::requestDeathResultExitTransition(ScreenTransitionTarget target)
{
    if (target != ScreenTransitionTarget::GameOverRetry &&
        target != ScreenTransitionTarget::GameOverReturnToBase &&
        target != ScreenTransitionTarget::AstralDeathReturnToBase) {
        return;
    }
    requestScreenTransition(target);
}

bool Game::deathResultExitTransitionActive() const
{
    return screenTransition_.active() &&
        (screenTransition_.target == ScreenTransitionTarget::GameOverRetry ||
        screenTransition_.target == ScreenTransitionTarget::GameOverReturnToBase ||
        screenTransition_.target == ScreenTransitionTarget::AstralDeathReturnToBase);
}

void Game::requestMiningStartTransition(bool useLatestWarpPoint, bool forceRegenerate)
{
    if (!useLatestWarpPoint || forceRegenerate) {
        requestedWarpPointStartPosition_.reset();
    }
    if (screenTransition_.active()) {
        return;
    }

    startScreenTransition(
        ScreenTransitionTarget::MiningStart,
        ScreenTransitionPhase::FadingOut,
        ScreenTransitionSound::Generic);
    screenTransition_.useLatestWarpPoint = useLatestWarpPoint;
    screenTransition_.forceRegenerate = forceRegenerate;
}

void Game::requestReturnToBaseTransition(
    bool stageCleared,
    bool died,
    ScreenTransitionSound sound)
{
    if (screenTransition_.active()) {
        return;
    }

    startScreenTransition(ScreenTransitionTarget::ReturnToBase, ScreenTransitionPhase::FadingOut, sound);
    screenTransition_.returnStageCleared = stageCleared;
    screenTransition_.returnDied = died;
}

void Game::requestBaseAreaCrossfade(BaseArea targetArea, Vec2 playerPosition, Vec2 playerFacing, std::string status)
{
    if (screenTransition_.active()) {
        return;
    }

    startScreenTransition(
        ScreenTransitionTarget::BaseArea,
        ScreenTransitionPhase::CrossFadeCapture,
        ScreenTransitionSound::Generic);
    screenTransition_.targetBaseArea = targetArea;
    screenTransition_.targetBasePlayerPosition = playerPosition;
    screenTransition_.targetBasePlayerFacing = playerFacing;
    screenTransition_.targetBaseStatus = testPlayMode_ ? std::move(status) : std::string{};
}

void Game::requestBaseAreaFade(BaseArea targetArea, Vec2 playerPosition, Vec2 playerFacing, std::string status, bool closeBaseUi)
{
    if (screenTransition_.active()) {
        return;
    }

    startScreenTransition(
        ScreenTransitionTarget::BaseArea,
        ScreenTransitionPhase::FadingOut,
        ScreenTransitionSound::Generic);
    screenTransition_.targetBaseArea = targetArea;
    screenTransition_.targetBasePlayerPosition = playerPosition;
    screenTransition_.targetBasePlayerFacing = playerFacing;
    screenTransition_.targetBaseStatus = testPlayMode_ ? std::move(status) : std::string{};
    screenTransition_.closeBaseUi = closeBaseUi;
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
            const ScreenTransitionTarget completedTarget = screenTransition_.target;
            const bool startDungeonRingIntro = dungeonRingIntroStartPending_;
            const float postTransitionStoryDelaySeconds =
                screenTransition_.postTransitionStoryDelaySeconds;
            screenTransition_ = ScreenTransitionState{};
            if (startDungeonRingIntro) {
                if (mode_ == ScreenMode::Playing) {
                    startDungeonRingIntroTimer();
                } else {
                    dungeonRingIntroStartPending_ = false;
                    dungeonRingIntroTimer_ = 0.0f;
                }
            }
            if (completedTarget == ScreenTransitionTarget::BossEncounterIntro) {
                finishBossEncounterIntroTransition();
            } else if (completedTarget == ScreenTransitionTarget::BossEncounterAfterDialogue) {
                finishBossEncounterAfterDialogueTransition();
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
    case ScreenTransitionTarget::FinalBossEndingKamishibai:
        startFinalBossEndingKamishibaiAfterTransition();
        break;
    case ScreenTransitionTarget::BaseArea:
        baseArea_ = screenTransition_.targetBaseArea;
        basePlayerPosition_ = screenTransition_.targetBasePlayerPosition;
        basePlayerFacing_ = screenTransition_.targetBasePlayerFacing;
        updateBasePlayerSpriteFlipFromFacing();
        if (baseArea_ == BaseArea::Outdoor) {
            baseOutdoorPlayerPosition_ = basePlayerPosition_;
        }
        if (screenTransition_.closeBaseUi) {
            closeBaseFacilityScreens();
        }
        resetPlayerFootstepDust();
        baseStatus_ = std::move(screenTransition_.targetBaseStatus);
        break;
    case ScreenTransitionTarget::BossEncounterIntro:
        applyBossStoryPlayerPlacement();
        break;
    case ScreenTransitionTarget::BossEncounterAfterDialogue:
        applyBossStoryPlayerPlacement();
        beginBossAfterStoryPresentation();
        camera_.setPosition(bossAfterStoryPresentationPosition());
        break;
    case ScreenTransitionTarget::GameOverRetry:
        retryAfterGameOver();
        break;
    case ScreenTransitionTarget::GameOverReturnToBase:
        returnToBaseAfterGameOver();
        break;
    case ScreenTransitionTarget::AstralDeathReturnToBase:
        clearAstralEchoRecentStar();
        deathResultPrelude_ = {};
        returnToBaseFromNormalStage(false, true);
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
    const bool restoreRetainedInventory = !roguelikeStage;
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
            restoreRetainedInventory,
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
        disarmWarpReturnInteractionAt(startPosition);
        tileMap_.updateAround(player_.position, 0.0f, runtimeBalanceForDungeon(), dungeonLayout_);
        normalizeOpenBuriedPlacementNodes();
        updateDungeonMinimap(0.0);
        captureRetrySnapshotAtWarpPoint();
    } else {
        disarmedWarpReturnPointIndex_ = -1;
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
    player_.spellRingShiftDistanceBonus = effectiveRingShiftDistanceForRing(spellRing_.activeRingIndex()) -
        balance_.spellRingShiftDistance;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const RingWorkshopRingUpgrades& upgrades = workshopRingUpgrades_[static_cast<std::size_t>(ringIndex)];
        const float workshopThrowDistance = balance_.spellRingThrowDistance +
            metersToWorldDistance(static_cast<float>(upgrades.throwDistanceLevel) * RingWorkshopThrowDistanceMetersPerLevel);
        const float workshopThrowCooldown = std::max(
            0.02f,
            balance_.spellRingThrowCooldown -
                static_cast<float>(upgrades.throwCooldownLevel) * RingWorkshopThrowCooldownSecondsPerLevel);
        spellRing_.setWorkshopModifiersForRing(ringIndex, RingWorkshopModifiers{
            workshopThrowDistance / std::max(0.001f, balance_.spellRingThrowDistance),
            workshopThrowCooldown / std::max(0.001f, balance_.spellRingThrowCooldown),
            static_cast<float>(upgrades.weightPenaltyLevel) * 0.10f,
            upgrades.equipSlotLevel * 2,
        });

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
        const InventoryObjectInstance* staffInstance = inventory_.equippedStaffInstance();
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
    if (amount <= 0 || !gameplayRewardsEnabled()) {
        return {};
    }

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

    effects_.spawnLevelUpPopup(levelUpPresentationAnchor());
    const Vec2 effectAnchor = levelUpReturnMode_ == ScreenMode::Base || basePresentationActive()
        ? basePlayerPosition_
        : player_.position - Vec2{0.0f, PlayerSpriteDrawSize * (PlayerSpriteAnchorY - 0.5f)};
    effects_.spawnLevelUpEffects(effectAnchor);

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
    float beforeValue = worldDistanceToMeters(spellRing_.orbitRadiusForRing(ringIndex));
    if (kind == RingLevelUpgradeKind::Speed) {
        beforeValue = linearMetersPerSecondForAngularSpeed(
            spellRing_.ringAngularSpeedForIndex(ringIndex, balance_),
            spellRing_.orbitRadiusForRing(ringIndex));
    } else if (kind == RingLevelUpgradeKind::WeightLimit) {
        beforeValue = spellRing_.maxEquippedWeightForRing(ringIndex);
    }

    RingLevelUpgradePoints& points = levelRingUpgradePoints_[static_cast<std::size_t>(ringIndex)];
    ++ringLevelUpgradePointRef(points, kind);
    levels_.finishChoice();
    applyPermanentUpgrades();

    float afterValue = worldDistanceToMeters(spellRing_.orbitRadiusForRing(ringIndex));
    if (kind == RingLevelUpgradeKind::Speed) {
        afterValue = linearMetersPerSecondForAngularSpeed(
            spellRing_.ringAngularSpeedForIndex(ringIndex, balance_),
            spellRing_.orbitRadiusForRing(ringIndex));
    } else if (kind == RingLevelUpgradeKind::WeightLimit) {
        afterValue = spellRing_.maxEquippedWeightForRing(ringIndex);
    }

    openUiResultDialog(
        levelUpResultDialog_,
        "レベルアップ",
        levelUpResultLines(RingLevelUpgradeSelection{ringIndex, kind}, beforeValue, afterValue, unlockedRingCount()));
    return true;
}

void Game::resetLevelRingUpgradePointsForRun()
{
    levelRingUpgradePoints_ = {};
    ringWorkshopDraftUpgradePoints_ = levelRingUpgradePoints_;
    ringWorkshopRespecSource_.reset();
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
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringRadiusUpgradeLevel_) * 0.08f;
    const float levelMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(levelRadiusPoints);
    const double staffMultiplier = std::max(
        0.0,
        ringEquipmentModifiersForRing(equipmentModifiers_, clampedRingIndex).ringRadiusMul);
    const float defaultRadiusBeforeStaff = balance_.spellRingRadius *
        baseUpgradeMultiplier *
        levelMultiplier;
    const float baseRadiusMultiplier = SpellRingSystem::baseRadiusMultiplierForRing(clampedRingIndex);
    const float defaultOrbitRadiusMeters = worldDistanceToMeters(
        defaultRadiusBeforeStaff * baseRadiusMultiplier * static_cast<float>(staffMultiplier));
    const float radiusMaxMeters = defaultOrbitRadiusMeters +
        static_cast<float>(workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].radiusMaxLevel) *
            RingWorkshopRadiusMaxMetersPerLevel[static_cast<std::size_t>(clampedRingIndex)];
    const float radiusMinMeters = std::max(
        0.10f,
        defaultOrbitRadiusMeters -
            static_cast<float>(workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].radiusMinLevel) *
                RingWorkshopRadiusMinMetersPerLevel[static_cast<std::size_t>(clampedRingIndex)]);
    const float radiusSettingMeters = ringWorkshopRadiusSettingForRing(clampedRingIndex);
    const float adjustedMeters = std::clamp(radiusSettingMeters, radiusMinMeters, std::max(radiusMinMeters, radiusMaxMeters));
    return metersToWorldDistance(adjustedMeters) / std::max(0.001f, baseRadiusMultiplier);
}

float Game::effectiveInitialRingSpeedForRing(int ringIndex, int levelSpeedPoints) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringSpeedUpgradeLevel_) * 0.08f;
    const float levelMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(levelSpeedPoints);
    const double staffMultiplier = std::max(
        0.0,
        ringEquipmentModifiersForRing(equipmentModifiers_, clampedRingIndex).ringSpeedMul);
    const int radiusPoints = clampedRingIndex >= 0 && clampedRingIndex < SpellRingCount
        ? clampedRingLevelUpgradePoints(levelRingUpgradePoints_[static_cast<std::size_t>(clampedRingIndex)]).radius
        : 0;
    const float orbitRadius = effectiveInitialRingRadiusForRing(clampedRingIndex, radiusPoints) *
        SpellRingSystem::baseRadiusMultiplierForRing(clampedRingIndex);
    const float baseSpeedMultiplier = SpellRingSystem::baseSpeedMultiplierForRing(clampedRingIndex);
    const float speedBeforeWorkshop = balance_.spellRingSpeed * baseUpgradeMultiplier * levelMultiplier;
    const float linearSpeedBeforeStaff =
        linearMetersPerSecondForAngularSpeed(speedBeforeWorkshop * baseSpeedMultiplier, orbitRadius) +
        static_cast<float>(workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].speedLevel) *
            RingWorkshopSpeedMetersPerSecondPerLevel[static_cast<std::size_t>(clampedRingIndex)];
    return angularSpeedForLinearMetersPerSecond(
        linearSpeedBeforeStaff / std::max(0.001f, baseSpeedMultiplier),
        orbitRadius) *
        static_cast<float>(staffMultiplier);
}

float Game::effectiveInitialRingWeightLimitForRing(int ringIndex, int levelWeightLimitPoints) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const double staffWeightAdd = std::max(
        0.0,
        ringEquipmentModifiersForRing(equipmentModifiers_, clampedRingIndex).ringWeightLimitAdd);
    return SpellRingSystem::initialMaxEquippedWeightForRing(clampedRingIndex) +
        SpellRingSystem::LevelWeightLimitUpgradeAmount * static_cast<float>(std::max(0, levelWeightLimitPoints)) +
        static_cast<float>(workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].weightLimitLevel) *
            RingWorkshopWeightLimitKgPerLevel +
        static_cast<float>(staffWeightAdd);
}

float Game::effectiveRingShiftDistanceForRing(int ringIndex) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    return balance_.spellRingShiftDistance +
        metersToWorldDistance(
            static_cast<float>(workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].shiftDistanceLevel) *
            RingWorkshopShiftDistanceMetersPerLevel);
}

float Game::effectiveRingShiftDistance() const
{
    return effectiveRingShiftDistanceForRing(spellRing_.activeRingIndex());
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

    recordMainObjectObtained(DefaultShovelObjectId);
    recordMainObjectObtained(DefaultTorchObjectId);
    ensureInitialStaffEquipped(inventory_, objectCatalog_, spellRing_);
}

void Game::observeRingItemInstanceIds()
{
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        for (const SpellRingItem& item : spellRing_.itemsForRing(ringIndex)) {
            inventory_.observeObjectInstanceId(item.instanceId);
        }
    }
}

int Game::unlockedRingPresetSlotCount() const
{
    return std::clamp(ringPresetSlotLevel_, 0, RingPresetSlotCount);
}

bool Game::registerRingPresetShortcut(int presetIndex)
{
    const int presetSlotCount = unlockedRingPresetSlotCount();
    if (presetIndex < 0 || presetIndex >= presetSlotCount) {
        ringStatus_ = presetSlotCount <= 0
            ? "リングプリセットは未解禁だよ"
            : "プリセット" + std::to_string(presetIndex + 1) + "は未解禁だよ";
        baseStatus_ = ringStatus_;
        return false;
    }
    if (!ringPresets_.capturePreset(presetIndex, spellRing_, unlockedRingCount())) {
        ringStatus_ = "プリセット登録に失敗したよ";
        return false;
    }
    ringStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "に登録したよ";
    baseStatus_ = ringStatus_;
    return true;
}

bool Game::applyRingPreset(int presetIndex)
{
    const int presetSlotCount = unlockedRingPresetSlotCount();
    if (presetIndex < 0 || presetIndex >= presetSlotCount) {
        ringStatus_ = presetSlotCount <= 0
            ? "リングプリセットは未解禁だよ"
            : "プリセット" + std::to_string(presetIndex + 1) + "は未解禁だよ";
        baseStatus_ = ringStatus_;
        return false;
    }
    RingPresetApplyResult result = ringPresets_.applyPreset(
        presetIndex,
        inventory_,
        spellRing_,
        objectCatalog_,
        unlockedRingCount());
    if (!result.status.empty()) {
        ringStatus_ = result.status;
        baseStatus_ = result.status;
    }
    if (!result.applied) {
        return false;
    }

    spellRing_.applyObjectParameters(objectCatalog_);
    spellRing_.normalizeItemPlacements();
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

bool Game::shouldRecordMainProgressKnowledge() const
{
    return !(currentStageIsRoguelike() && mode_ == ScreenMode::Playing);
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
    player_.spellRingShiftDistanceBonus = effectiveRingShiftDistanceForRing(spellRing_.activeRingIndex()) -
        balance_.spellRingShiftDistance;
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
    baseMiningStartSelection_ = currentStageIsRoguelike() ? 0 : (unlockedWarpPointCount_ > 0 ? 1 : 0);
    baseWarpPointSelectActive_ = false;
    baseWarpPointSelection_ = 0;
    baseRegenerateConfirm_ = {};
    baseRoguelikeDepartureConfirm_ = {};
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
        upsertObjectDefinition(objectCatalog_, makeCapturedObjectDefinition(enemy, EnemyVariantTier::Deep));
        upsertObjectDefinition(objectCatalog_, makeCapturedObjectDefinition(enemy, EnemyVariantTier::Abyss));
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
            << "\" base_level=" << enemy.baseLevel
            << " hp=" << enemy.hp
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
    if (!playerDeathSequenceActive()) {
        playAudioJingle(
            AudioSeGameOverJingle,
            GameOverJingleFallbackSeconds,
            0.12f,
            0.36f,
            1.0f,
            1.0f);
    }
    if (currentStageIsRoguelike()) {
        enterAstralResult(AstralRunResult::Died);
        return;
    }

    recordAstralEchoStar(true);
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
    freezePlayerDeathPoseForResult();
    beginDeathResultPrelude();
    pausePage_ = PauseMenuPage::Main;
    pauseReturnMode_ = ScreenMode::Playing;
    inventoryReturnToPause_ = false;
    gameOverSelection_ = 0;
    gameOverStatus_.clear();
}

void Game::beginDeathResultPrelude()
{
    deathResultPrelude_ = {};
    deathResultPrelude_.active = true;
}

bool Game::updateDeathResultPrelude(float dt, UiContext& ui)
{
    if (!deathResultPrelude_.active) {
        return false;
    }

    constexpr float TotalSeconds =
        DeathResultPreludeFadeOutSeconds +
        DeathResultPreludeBlackHoldSeconds +
        DeathResultPreludeStarFadeInSeconds;
    deathResultPrelude_.elapsedSeconds += std::max(0.0f, dt);
    if (deathResultPrelude_.elapsedSeconds >= TotalSeconds) {
        deathResultPrelude_.active = false;
    }

    const bool blocksWindow = deathResultPreludeBlocksWindow();
    if (blocksWindow) {
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
    }
    return blocksWindow;
}

bool Game::deathResultPreludeBlocksWindow() const
{
    if (!deathResultPrelude_.active) {
        return false;
    }
    constexpr float TotalSeconds =
        DeathResultPreludeFadeOutSeconds +
        DeathResultPreludeBlackHoldSeconds +
        DeathResultPreludeStarFadeInSeconds;
    return deathResultPrelude_.elapsedSeconds <
        std::max(0.0f, TotalSeconds - DeathResultWindowLeadSeconds);
}

float Game::deathResultPreludeBlackAlpha() const
{
    return smoothStep01(std::clamp(deathResultPrelude_.elapsedSeconds / DeathResultPreludeFadeOutSeconds, 0.0f, 1.0f));
}

float Game::deathResultPreludeStarAlpha() const
{
    const float starElapsed =
        deathResultPrelude_.elapsedSeconds -
        DeathResultPreludeFadeOutSeconds -
        DeathResultPreludeBlackHoldSeconds;
    return smoothStep01(std::clamp(starElapsed / DeathResultPreludeStarFadeInSeconds, 0.0f, 1.0f));
}

void Game::handleApplicationQuitRequested()
{
    if (astralEchoQuitRecordable()) {
        recordAstralEchoStar(false);
    }
}

void Game::recordAstralEchoStar(bool markRecent)
{
    astralEchoStarCount_ = std::clamp(astralEchoStarCount_ + 1, 0, 9999999);
    if (markRecent) {
        astralEchoRecentStarIndex_ = astralEchoStarCount_ - 1;
        astralEchoRecentStarVisible_ = true;
    } else {
        clearAstralEchoRecentStar();
    }
    saveAstralEchoMeta();
}

void Game::clearAstralEchoRecentStar()
{
    astralEchoRecentStarIndex_ = -1;
    astralEchoRecentStarVisible_ = false;
}

bool Game::astralEchoQuitRecordable() const
{
    switch (mode_) {
    case ScreenMode::Playing:
        return !introTutorialActive();
    case ScreenMode::PauseMenu:
    case ScreenMode::Inventory:
    case ScreenMode::Ring:
        return !introTutorialActive() && pauseReturnMode_ == ScreenMode::Playing;
    case ScreenMode::LevelUp:
        return !introTutorialActive() && levelUpReturnMode_ == ScreenMode::Playing;
    default:
        return false;
    }
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
    if (endingKamishibaiPending_ ||
        mode_ == ScreenMode::EndingKamishibai ||
        screenTransition_.active()) {
        return;
    }

    markCurrentStageCleared();
    requestScreenTransition(ScreenTransitionTarget::FinalBossEndingKamishibai);
}

void Game::startFinalBossEndingKamishibaiAfterTransition()
{
    bossEncounterRingHidden_ = false;
    clearDungeonStoryPresentation();
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
    startEndingKamishibai(EndingKind::Main);
}

void Game::updateScreenMode(
    const Input& input,
    UiContext& ui,
    Renderer& renderer,
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

    if (player_.hp <= 0 &&
        !playerDeathSequenceActive() &&
        mode_ != ScreenMode::GameOver &&
        mode_ != ScreenMode::StageClear &&
        mode_ != ScreenMode::AstralResult) {
        beginPlayerDeathSequence();
        return;
    }

    if (itemAcquisitionNoticeActive()) {
        updateItemAcquisitionNotice(input, ui, renderer, dt);
        return;
    }

    if (debugNamedSaveDialogMode_ != DebugNamedSaveDialogMode::Closed) {
        updateDebugNamedSaveUi(input, ui);
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

    if (portraitExpressionPicker_.active) {
        updatePortraitExpressionPicker(input, ui);
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

    if (mode_ == ScreenMode::EnemyPlacementEdit) {
        updateEnemyPlacementEditScreen(input, ui);
        return;
    }

    if (mode_ == ScreenMode::EnemyShadowEdit) {
        updateEnemyShadowEditScreen(input, ui);
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
        if (dialogue_.currentCommand() != nullptr) {
            updateStoryEventCommand(dt);
        } else {
            updateDialoguePlayerIdleAnimation(dt);
        }
        const bool dialogueWasActive = dialogue_.active();
        dialogue_.update(input, dt);
        if (const std::optional<std::string> speakerId = dialogue_.consumeTextSoundSpeakerId()) {
            playAudioSe(dialogueTextSoundCueForSpeaker(*speakerId));
        }
        if (dialogue_.consumeAdvanceSoundRequests() > 0) {
            playAudioSe(AudioSeDialogueAdvance);
        }
        runDialogueCompletionCallbackIfFinished(dialogueWasActive);
        ui.consumePointer();
        return;
    }

    if (dungeonEventItemRequestUiActive()) {
        updateDungeonEventItemRequestUi(input, ui);
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
        startEndingKamishibai(endingKamishibaiKind_);
        return;
    }

    if (mode_ == ScreenMode::Base && maybeStartHiddenMonicaDuel()) {
        return;
    }

    if (player_.hp > 0 && levels_.isChoosing() && mode_ != ScreenMode::LevelUp && mode_ != ScreenMode::WorldLoading) {
        openLevelUpChoice(basePresentationActive() ? ScreenMode::Base : ScreenMode::Playing);
    }

    if (mode_ == ScreenMode::Base) {
        updateBaseScreen(input, ui, dt);
        return;
    }

    if (player_.hp <= 0 &&
        !playerDeathSequenceActive() &&
        mode_ != ScreenMode::GameOver &&
        mode_ != ScreenMode::StageClear &&
        mode_ != ScreenMode::AstralResult) {
        beginPlayerDeathSequence();
        return;
    }
    if (mode_ == ScreenMode::GameOver) {
        updateGameOverScreen(input, ui, dt);
        return;
    }
    if (mode_ == ScreenMode::StageClear) {
        updateStageClearScreen(input, ui);
        return;
    }
    if (mode_ == ScreenMode::AstralResult) {
        updateAstralResultScreen(input, ui, dt);
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
        if (updateRoguelikeFacilityUi(input, ui, dt)) {
            return;
        }
        if (updateWarpReturnUi(input, ui)) {
            return;
        }
        if (updateRoguelikeBigHoleUi(input, ui)) {
            return;
        }
        if (updateRoguelikeFacilityInteraction(input, ui)) {
            return;
        }
        if (updateDungeonEventNpcInteraction(input, ui)) {
            return;
        }
        if (dungeonMapOverlayOpen_) {
            const UiRect mapPanel = dungeonMapOverlayPanelRect();
            if (dungeonEventUiSuppressed() || input.pausePressed() ||
                uiCancelRequested(pauseCancelState_, input, ui, mapPanel)) {
                dungeonMapOverlayOpen_ = false;
                dungeonMapOverlayScrollbarDragAxis_ = 0;
                dungeonMapOverlayScrollbarDragOffset_ = 0.0f;
                ui.emitSound(UiSoundEvent::Cancel);
            }
            const Vec2 maxScroll = dungeonMapOverlayMaxScroll();
            if (input.mouseLeftReleased()) {
                dungeonMapOverlayScrollbarDragAxis_ = 0;
                dungeonMapOverlayScrollbarDragOffset_ = 0.0f;
            }
            if (dungeonMapOverlayScrollbarDragAxis_ != 0 && input.mouseLeftHeld()) {
                if (dungeonMapOverlayScrollbarDragAxis_ == 1 && maxScroll.y > 0.0f) {
                    const UiRect track = dungeonMapOverlayVerticalScrollTrackRect();
                    (void)ui.selectionFocused(track);
                    const UiRect thumb = dungeonMapOverlayVerticalScrollThumbRect();
                    const float movable = std::max(1.0f, track.size.y - thumb.size.y);
                    const float thumbY = std::clamp(ui.mouse().y - dungeonMapOverlayScrollbarDragOffset_, track.pos.y, track.pos.y + movable);
                    dungeonMapOverlayScroll_.y = ((thumbY - track.pos.y) / movable) * maxScroll.y;
                } else if (dungeonMapOverlayScrollbarDragAxis_ == 2 && maxScroll.x > 0.0f) {
                    const UiRect track = dungeonMapOverlayHorizontalScrollTrackRect();
                    (void)ui.selectionFocused(track);
                    const UiRect thumb = dungeonMapOverlayHorizontalScrollThumbRect();
                    const float movable = std::max(1.0f, track.size.x - thumb.size.x);
                    const float thumbX = std::clamp(ui.mouse().x - dungeonMapOverlayScrollbarDragOffset_, track.pos.x, track.pos.x + movable);
                    dungeonMapOverlayScroll_.x = ((thumbX - track.pos.x) / movable) * maxScroll.x;
                }
                ui.consumePointer();
            } else {
                const UiRect verticalTrack = dungeonMapOverlayVerticalScrollTrackRect();
                const UiRect verticalThumb = dungeonMapOverlayVerticalScrollThumbRect();
                const UiRect horizontalTrack = dungeonMapOverlayHorizontalScrollTrackRect();
                const UiRect horizontalThumb = dungeonMapOverlayHorizontalScrollThumbRect();
                const bool verticalPressed = maxScroll.y > 0.0f &&
                    ui.pressed(verticalTrack) &&
                    !ui.navigationActive();
                const bool horizontalPressed = maxScroll.x > 0.0f &&
                    ui.pressed(horizontalTrack) &&
                    !ui.navigationActive();
                if (verticalPressed) {
                    dungeonMapOverlayScrollbarDragAxis_ = 1;
                    dungeonMapOverlayScrollbarDragOffset_ = verticalThumb.contains(ui.mouse())
                        ? ui.mouse().y - verticalThumb.pos.y
                        : verticalThumb.size.y * 0.5f;
                    const float movable = std::max(1.0f, verticalTrack.size.y - verticalThumb.size.y);
                    const float thumbY = std::clamp(ui.mouse().y - dungeonMapOverlayScrollbarDragOffset_, verticalTrack.pos.y, verticalTrack.pos.y + movable);
                    dungeonMapOverlayScroll_.y = ((thumbY - verticalTrack.pos.y) / movable) * maxScroll.y;
                    ui.consumePointer();
                } else if (horizontalPressed) {
                    dungeonMapOverlayScrollbarDragAxis_ = 2;
                    dungeonMapOverlayScrollbarDragOffset_ = horizontalThumb.contains(ui.mouse())
                        ? ui.mouse().x - horizontalThumb.pos.x
                        : horizontalThumb.size.x * 0.5f;
                    const float movable = std::max(1.0f, horizontalTrack.size.x - horizontalThumb.size.x);
                    const float thumbX = std::clamp(ui.mouse().x - dungeonMapOverlayScrollbarDragOffset_, horizontalTrack.pos.x, horizontalTrack.pos.x + movable);
                    dungeonMapOverlayScroll_.x = ((thumbX - horizontalTrack.pos.x) / movable) * maxScroll.x;
                    ui.consumePointer();
                }
            }
            if (dungeonMapOverlayViewportRect().contains(ui.mouse())) {
                dungeonMapOverlayScroll_.y += static_cast<float>(input.mouseWheelDelta()) * 42.0f;
            }
            if (input.pressed(InputAction::MoveLeft)) {
                dungeonMapOverlayScroll_.x -= 42.0f;
            }
            if (input.pressed(InputAction::MoveRight)) {
                dungeonMapOverlayScroll_.x += 42.0f;
            }
            if (input.pressed(InputAction::MoveUp)) {
                dungeonMapOverlayScroll_.y -= 42.0f;
            }
            if (input.pressed(InputAction::MoveDown)) {
                dungeonMapOverlayScroll_.y += 42.0f;
            }
            dungeonMapOverlayScroll_.x = std::clamp(dungeonMapOverlayScroll_.x, 0.0f, maxScroll.x);
            dungeonMapOverlayScroll_.y = std::clamp(dungeonMapOverlayScroll_.y, 0.0f, maxScroll.y);
            ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
            return;
        }
        if (!dungeonEventUiSuppressed() &&
            !enemyTestActive_ &&
            !dungeonMinimapCells_.empty() &&
            ui.pressed(dungeonMinimapRect())) {
            dungeonMapOverlayOpen_ = true;
            dungeonMapOverlayScroll_ = dungeonMapOverlayPlayerCenteredScroll();
            dungeonMapOverlayScrollbarDragAxis_ = 0;
            dungeonMapOverlayScrollbarDragOffset_ = 0.0f;
            ui.emitSound(UiSoundEvent::BookOpen);
            return;
        }
        // 右クリックは固定のリングドラッグを優先し、同じ既定割当のメニューを開かない。
        if (input.pausePressed() && !input.ringOffsetPointerHeld()) {
            ui.emitSound(UiSoundEvent::MenuOpen);
            mode_ = ScreenMode::PauseMenu;
            pauseReturnMode_ = ScreenMode::Playing;
            pausePage_ = PauseMenuPage::Main;
            return;
        }
        if (updateRingStatusHud(ui, dt)) {
            return;
        }
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
        if (input.cycleDelta() != 0) {
            switchActiveRingWithLog(input.cycleDelta());
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
    case ScreenMode::EnemyPlacementEdit:
        updateEnemyPlacementEditScreen(input, ui);
        break;
    case ScreenMode::EnemyShadowEdit:
        updateEnemyShadowEditScreen(input, ui);
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
        updateBaseStorySpeakerFacing();
        updateBasePlayerSpriteAnimation(dt, false);
        updateBaseActorIdleAnimation(dt);
        return;
    }
    if (mode_ == ScreenMode::Playing) {
        player_.updateSpriteAnimation(dt, false);
        enemies_.updateBossStoryVisuals(dt);
    }
}

void Game::updateStoryEventCommand(float dt)
{
    const DialogueCommand* command = dialogue_.currentCommand();
    if (command == nullptr) {
        return;
    }

    if (command->name == "story_shake") {
        float amplitude = 5.0f;
        float duration = 0.22f;
        std::string_view soundId;
        const bool hasProfile = applyStoryShakeProfile(*command, amplitude, duration, soundId);
        amplitude = storyCommandFloatArg(*command, hasProfile ? 1 : 0, amplitude);
        duration = storyCommandFloatArg(*command, hasProfile ? 2 : 1, duration);
        if (!storyShakeCommandActive()) {
            beginStoryShakeCommand(amplitude, duration, soundId);
        }
        updateStoryShakeCommand(dt);
        if (!storyShakeCommandActive()) {
            dialogue_.completeCurrentCommandStep();
        }
        return;
    }

    if (command->name == "story_phone") {
        StoryPhoneSoundKind kind = StoryPhoneSoundKind::None;
        const std::string mode = command->args.empty() ? std::string{} : command->args[0];
        if (mode == "incoming" || mode == "ring" || mode == "ring_incoming") {
            kind = StoryPhoneSoundKind::Incoming;
        } else if (mode == "outgoing" || mode == "call" || mode == "call_outgoing") {
            kind = StoryPhoneSoundKind::Outgoing;
        } else if (mode == "hangup" || mode == "end" || mode == "cut") {
            kind = StoryPhoneSoundKind::Hangup;
        }

        if (kind == StoryPhoneSoundKind::None) {
            logWarning("[story] unknown story_phone mode: " + mode);
            dialogue_.completeCurrentCommandStep();
            return;
        }

        if (!storyPhoneSoundActive()) {
            beginStoryPhoneSound(kind);
        }
        updateStoryPhoneSound(dt);
        if (!storyPhoneSoundActive()) {
            dialogue_.completeCurrentCommandStep();
        }
        return;
    }

    if (command->name == "story_jingle") {
        std::string_view cueId;
        float fallbackSeconds = StoryRingUnlockJingleFallbackSeconds;
        if (storyJingleCueForCommand(*command, cueId, fallbackSeconds)) {
            playAudioJingle(cueId, fallbackSeconds, 0.08f, 0.24f, 1.0f, 1.0f);
        } else {
            const std::string profile = command->args.empty() ? std::string{} : command->args[0];
            logWarning("[story] unknown story_jingle profile: " + profile);
        }
        dialogue_.completeCurrentCommandStep();
        return;
    }

    if (command->name.rfind("dungeon_", 0) == 0) {
        updateDungeonStoryCommand(*command, dt);
        return;
    }

    updateBaseStoryPresentationCommand(dt);
}

void Game::runDialogueCompletionCallbackIfFinished(bool dialogueWasActive)
{
    if (!dialogueWasActive || dialogue_.active() || !pendingDialogueCompletion_) {
        return;
    }

    std::function<void()> onComplete = std::move(pendingDialogueCompletion_);
    pendingDialogueCompletion_ = {};
    clearBaseStoryPresentation();
    onComplete();
}

bool Game::gameProgressPaused() const
{
    return effectTestActive_ ||
        projectileTestActive_ ||
        debugItemPickerActive_ ||
        debugStoryTestActive_ ||
        pendingStoryTriggerDelayActive() ||
        itemAcquisitionNoticeActive() ||
        dungeonEventItemRequestUiActive() ||
        dungeonFocusActive() ||
        dialogue_.active() ||
        (introTutorialActive() && dungeonRingIntroActive()) ||
        bossEncounterBlocksProgress() ||
        endingKamishibaiPending_ ||
        warpReturnConfirm_.open ||
        roguelikeBigHoleMenu_.open ||
        roguelikeFacilityUiActive() ||
        mode_ != ScreenMode::Playing;
}

bool Game::dungeonEventUiSuppressed() const
{
    return mode_ == ScreenMode::Playing &&
        (!pendingStoryTrigger_.empty() ||
            !pendingStoryTriggers_.empty() ||
            pendingStoryTriggerDelayActive() ||
            dungeonEventItemRequestUiActive() ||
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

void Game::startDungeonRingIntroTimer()
{
    dungeonRingIntroStartPending_ = false;
    dungeonRingIntroTimer_ = DungeonRingIntroDuration;
    playAudioSe(AudioSeRingAppear);
}

void Game::beginDungeonRingIntro()
{
    spellRing_.resetRuntimeStateAtPlayer(player_, balance_);

    if (screenTransition_.active()) {
        dungeonRingIntroStartPending_ = true;
        dungeonRingIntroTimer_ = 0.0f;
        return;
    }

    startDungeonRingIntroTimer();
}

void Game::updateDungeonRingIntro(float dt)
{
    if (mode_ != ScreenMode::Playing || dialogue_.active()) {
        return;
    }
    if (dungeonRingIntroStartPending_ && !screenTransition_.active()) {
        startDungeonRingIntroTimer();
    }
    if (dungeonRingIntroTimer_ <= 0.0f) {
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
        itemAcquisitionNoticeActive()) {
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
    player_.spellRingShiftDistanceBonus = effectiveRingShiftDistanceForRing(currentRing) -
        balance_.spellRingShiftDistance;
    if (currentRing != previousRing && mode_ == ScreenMode::Playing) {
        pushDungeonLog(
            ringDisplayNameWithSpaceSuffix(currentRing, ringCount, "に切替"),
            "ring_switch");
    }
}

void Game::update(const Input& input, const Time& time, Renderer& renderer)
{
    FrameProfileScope updateProfile("Game.update");
    navigationUiCursorEnabled_ = input.uiNavigationCursorActive();
    const GameSettings currentSettings = settingsGetter_ ? settingsGetter_() : optionsSettings_;
    lightweightModeActive_ = currentSettings.performance.lightweight;
    presentationSettingsActive_ = currentSettings.presentation;
    const bool lightweight = lightweightModeEnabled();
    effects_.setLightweightMode(lightweight);
    magicFx_.setLightweightMode(lightweight);
    wetGround_.setLightweightMode(lightweight);
    updateScreenShake(time.deltaSeconds());
    updatePlayerDamageVignette(time.deltaSeconds());
    spellRing_.updateTransientPresentation(time.deltaSeconds());
    updateAudioJingle(time.deltaSeconds());

    checkHotReload(time.deltaSeconds());
    reloadNoticeTimer_ = std::max(0.0f, reloadNoticeTimer_ - time.deltaSeconds());
    const bool discoveryPopupStartAllowed =
        mode_ == ScreenMode::Playing &&
        player_.hp > 0 &&
        !playerDeathSequenceActive() &&
        !levels_.isChoosing() &&
        !gameProgressPaused() &&
        !dungeonEventUiSuppressed() &&
        !dungeonMapOverlayOpen_ &&
        !screenTransition_.active() &&
        !worldBuildActive() &&
        !debugPaused_;
    if (const std::optional<EncyclopediaPopupStartedEvent> popupStarted =
            encyclopedia_.update(time.deltaSeconds(), discoveryPopupStartAllowed)) {
        switch (popupStarted->cue) {
        case EncyclopediaPopupCue::EffectDiscovery:
            playAudioSe(AudioSeEffectDiscovery);
            break;
        case EncyclopediaPopupCue::MonsterDiscovery:
            playAudioSeAt(AudioSeMonsterDiscovery, popupStarted->position);
            break;
        case EncyclopediaPopupCue::None:
            break;
        }
    }
    updateDungeonLogs(time.deltaSeconds());
    handleRingItemAddedEvents();

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
    std::vector<EffectDiscoveryEvent>* effectDiscoveryEvents =
        shouldRecordEffectDiscoveries() ? &effectDiscoveries : nullptr;
    UiContext ui(input, renderer);
    struct UiSoundFlush {
        Game& game;
        const UiContext& ui;
        ~UiSoundFlush() { game.playUiSoundEvents(ui); }
    } uiSoundFlush{*this, ui};
    const bool wasPaused = gameProgressPaused();
    updateScreenMode(input, ui, renderer, time.deltaSeconds(), effectDiscoveryEvents);
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
    const bool paused = gameProgressPaused() || dungeonMapOverlayOpen_ || (wasPaused && mode_ == ScreenMode::Playing);
    if (paused && gameplayRewardsEnabled() && !effectDiscoveries.empty()) {
        applyEffectDiscoveries(effectDiscoveries);
    }
    syncIntroTutorialTerrainDamageLocks();
    if (dungeonEventUiSuppressed()) {
        updatePausedDungeonPresentation(time.deltaSeconds());
    }

    if (!paused) {
        runStats_.elapsedSeconds += time.deltaSeconds();
        updateAutoSimulationCheckpointMeasurement(time.deltaSeconds());
        updateAstralRunProgress();
        updatePlayerFootstepDust(time.deltaSeconds());
        bool deathActive = playerDeathSequenceActive();
        const RuntimeBalance dungeonBalance = runtimeBalanceForDungeon();
        const int playerChunkBeforeX = chunkCoordForWorld(player_.position.x);
        const int playerChunkBeforeY = chunkCoordForWorld(player_.position.y);
        {
            FrameProfileScope profile("TileMap.update");
            tileMap_.updateAround(player_.position, time.deltaSeconds(), dungeonBalance, dungeonLayout_);
        }
        normalizeOpenBuriedPlacementNodes();
        std::vector<CollisionRect> objectBlockers;
        if (!enemyTestActive_) {
            FrameProfileScope profile("Collision.objects");
            objectBlockers = solidObjectCollisionRects();
        }
        const RingEquipmentModifiers& activeEquipment =
            spellRing_.equipmentModifiersForRing(spellRing_.activeRingIndex());
        player_.spellRingShiftDistanceMultiplier = static_cast<float>(
            std::max(0.0, spellRing_.orbitShiftMultiplier()) *
            std::max(0.0, activeEquipment.ringShiftDistanceMul));
        if (!deathActive) {
            {
                FrameProfileScope profile("Player.update");
                player_.update(
                    input,
                    tileMap_,
                    time.deltaSeconds(),
                    false,
                    balance_,
                    std::span<const CollisionRect>{objectBlockers.data(), objectBlockers.size()});
            }
            maybeTriggerPlayerFootstep(
                player_.position,
                lengthSquared(player_.velocity) > 0.0001f ? player_.velocity : player_.facing,
                player_.spriteWalking,
                player_.spriteFrameIndex(),
                previousPlayerDustFrame_,
                PlayerFootstepSurface::Dungeon);
            updatePlayerRegen(time.deltaSeconds(), effectDiscoveries);
        } else {
            player_.velocity = {};
        }
        if (player_.hp <= 0) {
            beginPlayerDeathSequence();
            deathActive = true;
        }
        if (!deathActive) {
            updateDungeonRouteDeviation(time.deltaSeconds());
            updateDungeonDepthTutorials();
        }
        if (!enemyTestActive_) {
            updateWarpPoints(time.deltaSeconds());
            updateExposedEnemyNodes();
        }
        if (gameplayRewardsEnabled()) {
            updateRingEffectDiscoveries(effectDiscoveries);
        }
        normalizeOpenBuriedPlacementNodes();
        camera_.follow(player_.position, time.deltaSeconds());
        if (!deathActive && gameplayRewardsEnabled()) {
            prepareDungeonEventEncountersForView();
        }
        if (!deathActive && updateDungeonEventDiscovery(time.deltaSeconds())) {
            return;
        }

        {
            FrameProfileScope profile("SpellRing.update");
            if (!deathActive) {
                spellRing_.update(player_, input, time.deltaSeconds(), time.totalSeconds(), false, ui.pointerConsumed(), balance_);
            } else {
                spellRing_.updatePresentation(player_, time.deltaSeconds(), balance_);
            }
        }
        for (const RingMotionEvent& event : spellRing_.consumeMotionEvents()) {
            if (event.kind == RingMotionEventKind::ThrowStart) {
                playAudioSeAt(AudioSeRingThrow, event.position);
                effects_.spawnThrowStart(event.position, event.direction);
            } else if (event.kind == RingMotionEventKind::ReturnEnd) {
                effects_.spawnReturn(event.position);
            }
        }
        if (gameplayRewardsEnabled()) {
            updateDungeonEvents(time.deltaSeconds(), time.totalSeconds());
        }
        if (!deathActive && gameplayRewardsEnabled()) {
            updateHiddenDungeonNpcTargets();
        }
        if (gameplayRewardsEnabled()) {
            updateChestNodes(time.deltaSeconds(), input);
        }
        if (!enemyTestActive_ && gameplayRewardsEnabled()) {
            updateCrateNodes();
        }

        const bool playerChunkChanged =
            chunkCoordForWorld(player_.position.x) != playerChunkBeforeX ||
            chunkCoordForWorld(player_.position.y) != playerChunkBeforeY;
        if (playerChunkChanged) {
            FrameProfileScope profile("TileMap.update");
            tileMap_.updateAround(player_.position, time.deltaSeconds(), dungeonBalance, dungeonLayout_);
        }
        {
            FrameProfileScope profile("Digging.update");
            digging_.update(
                tileMap_,
                spellRing_,
                player_,
                time.totalSeconds(),
                time.deltaSeconds(),
                objectCatalog_,
                &hitboxes_,
                effectDispatcher_,
                &magic_,
                effectDiscoveryEvents,
                gameplayRewardsEnabled() ? &encyclopedia_ : nullptr);
        }
        const std::vector<RingImpactSoundPlayback> terrainImpactSounds =
            resolveRingImpactSoundEvents(digging_.impactSoundEvents(), 3);
        if (!terrainImpactSounds.empty()) {
            for (const RingImpactSoundPlayback& sound : terrainImpactSounds) {
                playAudioSeAt(sound.cueId, sound.position, sound.volumeScale, sound.pitchScale);
            }
        }
        const bool playDigEffectSounds = terrainImpactSounds.empty();
        for (const TerrainHitTile& tile : digging_.hitTiles()) {
            effects_.spawnDigHit(tile.center, tile.center - spellRing_.center(), tile.color, playDigEffectSounds);
        }
        if (digging_.dugTiles().empty()) {
            for (Vec2 tile : digging_.openedTiles()) {
                effects_.spawnTileBreak(tile, TileType::Dirt, tileMap_.tileColorAtWorld(tile), playDigEffectSounds);
            }
        }
        if (!enemyTestActive_ && gameplayRewardsEnabled()) {
            revealRewardNodesFromOpenedTiles(digging_.openedTiles());
            revealMoonFragmentNodesFromOpenedTiles(digging_.openedTiles());
            revealChestNodesFromOpenedTiles(digging_.openedTiles());
        }
        revealDungeonMinimapOpenedTiles(digging_.openedTiles());
        for (const DugTile& tile : digging_.dugTiles()) {
            effects_.spawnTileBreak(tile.center, tile.type, tile.color, playDigEffectSounds);
            ++runStats_.dugTiles;

            std::mt19937& rng = lootRuntimeRng();
            if (gameplayRewardsEnabled() && digEventDue(
                    runStats_.dugTilesSinceMoneyDrop,
                    balance_.digMoneyMinDugTiles,
                    balance_.digMoneyGuaranteeDugTiles,
                    rng)) {
                const int depthRank = currentStageIsRoguelike()
                    ? roguelikeDepthRankForWorldPosition(tile.center)
                    : lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, tile.center);
                const float multiplier =
                    lootStageMultiplier(balance_, currentStageId_) *
                    lootDepthMultiplier(balance_, currentStageId_, depthRank);
                std::uniform_int_distribution<int> moneyDistribution(2, 6);
                const int amount = scaledLootAmount(moneyDistribution(rng), multiplier);
                if (grantDungeonMoney(
                        amount,
                        safeLootLandingPosition(tile.center, rng))) {
                    runStats_.dugTilesSinceMoneyDrop = 0;
                }
            }

            if (gameplayRewardsEnabled() && digEventDue(
                    runStats_.dugTilesSinceItemDrop,
                    balance_.digItemMinDugTiles,
                    balance_.digItemGuaranteeDugTiles,
                    rng)) {
                const int depthRank = currentStageIsRoguelike()
                    ? roguelikeDepthRankForWorldPosition(tile.center)
                    : lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, tile.center);
                if (spawnWeightedObjectLoot(
                        LootChestKind::Common,
                        depthRank,
                        tile.center,
                        rng,
                        "DigItemLoot",
                        true,
                        LootSourceKind::DigItem)) {
                    runStats_.dugTilesSinceItemDrop = 0;
                }
            }
        }
        for (const CapturedRewardDropRequest& rewardRequest : digging_.rewardDropRequests()) {
            if (!gameplayRewardsEnabled()) {
                break;
            }
            std::mt19937& rng = lootRuntimeRng();
            WeightedObjectLootProfile lootProfile;
            if (!weightedObjectLootProfileForDropProfile(rewardRequest.profile, lootProfile)) {
                logError("[warning] CapturedRewardLoot: unknown reward profile \"" + rewardRequest.profile + "\"; no item drop");
                continue;
            }
            const int depthRank = currentStageIsRoguelike()
                ? roguelikeDepthRankForWorldPosition(rewardRequest.position)
                : lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, rewardRequest.position);
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
            if (!gameplayRewardsEnabled()) {
                break;
            }
            if (tile.type != TileType::Ore) {
                continue;
            }
            const int depthRank = currentStageIsRoguelike()
                ? roguelikeDepthRankForWorldPosition(tile.center)
                : lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, tile.center);
            const float multiplier =
                lootStageMultiplier(balance_, currentStageId_) *
                lootDepthMultiplier(balance_, currentStageId_, depthRank);
            std::uniform_int_distribution<int> oreAmountDistribution(balance_.oreMaterialMin, balance_.oreMaterialMax);
            std::mt19937& rng = lootRuntimeRng();
            const int amount = scaledLootAmount(oreAmountDistribution(rng), multiplier);
            worldDrops_.spawnMaterialDrop(
                MaterialType::EnhancementOre,
                amount,
                safeLootLandingPosition(tile.center, rng),
                runStats_.elapsedSeconds,
                makeWorldLootJumpMotion(tile.center, rng));
        }
        if (gameplayRewardsEnabled()) {
            for (const DugTile& tile : digging_.dugTiles()) {
                if (trySpawnFailsafeShovelDropFromWall(tile.center)) {
                    break;
                }
            }
        }
        std::vector<WorldDropPickupEvent> pickupEvents;
        int blockedObjectPickupCount = 0;
        const float collectionPullRadius = effectiveCollectionPullRadius(collectionRangeUpgradeLevel_);
        if (!deathActive && collectionPullRadius > 0.0f) {
            FrameProfileScope profile("WorldDrops.update");
            worldDrops_.pullNearbyDrops(
                player_.position,
                time.deltaSeconds(),
                collectionPullRadius,
                balance_.collectionPullAcceleration,
                balance_.collectionPullLimit,
                &inventory_,
                &objectCatalog_);
        }
        if (deathActive) {
            FrameProfileScope profile("WorldDrops.update");
            worldDrops_.updatePresentation(time.deltaSeconds());
        } else {
            FrameProfileScope profile("WorldDrops.update");
            const CollisionRect dropEffectBounds = expandedCollisionRect(cameraWorldRect(camera_), screenDormantMarginWorld());
            runStats_.acquiredItems += worldDrops_.update(
                time.deltaSeconds(),
                player_,
                inventory_,
                money_,
                objectCatalog_,
                &effects_,
                &pickupEvents,
                &blockedObjectPickupCount,
                &dropEffectBounds);
        }
        for (const WorldDropPickupEvent& event : pickupEvents) {
            if (!gameplayRewardsEnabled()) {
                break;
            }
            if (event.kind == WorldDropKind::Object) {
                runStats_.acquiredObjectItems += std::max(1, event.quantity);
                recordObjectAcquisitionNotice(
                    event.id,
                    event.instanceId,
                    event.protectable,
                    player_.position,
                    std::max(1, event.quantity));
                if (std::string_view(event.id) == MagnifyingGlassObjectId) {
                    queueStoryEventForTrigger("tutorial:magnifying_glass");
                }
                if (!introTutorialActive() && objectIdHasCaptureNetOrbitEffect(objectCatalog_, event.id)) {
                    queueStoryEventForTrigger("tutorial:capture_net");
                }
                if (!introTutorialActive() && objectIdIsMagicBook(objectCatalog_, event.id)) {
                    queueStoryEventForTrigger("tutorial:magic_book");
                }
                if (!introTutorialActive() && objectIdIsEquippableStaff(objectCatalog_, event.id)) {
                    queueStoryEventForTrigger("tutorial:staff_equip");
                }
                if (!introTutorialActive() && std::string_view(event.id) == TutorialAppleObjectId) {
                    queueStoryEventForTrigger("tutorial:item_use");
                }
            } else if (event.kind == WorldDropKind::Money) {
                moneyGainFx_.spawn(std::max(0, event.quantity), event.position);
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
        if (blockedObjectPickupCount > 0) {
            pushImportantDungeonNotice("リュックがいっぱいで拾えないよ", "pickup_inventory_full");
        }
        if (gameplayRewardsEnabled()) {
            updateDigToolFailsafe(time.deltaSeconds());
        }
        if (!enemyTestActive_) {
            updatePendingBuriedEnemySpawns(time.deltaSeconds());
            enemies_.syncScreenDormantEnemies(
                expandedCollisionRect(cameraWorldRect(camera_), screenDormantMarginWorld()),
                spellRing_);
            const std::vector<Vec2> randomEnemySpawnTiles = spawnHiddenEnemyNodesFromOpenedTiles(digging_.openedTiles());
            std::vector<DugEnemySpawnPoint> randomEnemySpawnPoints;
            randomEnemySpawnPoints.reserve(randomEnemySpawnTiles.size());
            for (Vec2 spawnTile : randomEnemySpawnTiles) {
                randomEnemySpawnPoints.push_back(DugEnemySpawnPoint{
                    .tileCenter = spawnTile,
                    .depthRank = roguelikeDepthRankForWorldPosition(spawnTile),
                });
            }
            const std::vector<DugEnemySpawnRequest> dugEnemySpawnRequests =
                enemies_.collectDugSpawnRequests(
                    randomEnemySpawnPoints,
                    tileMap_,
                    player_.position,
                    dungeonBalance,
                    static_cast<int>(pendingBuriedEnemySpawns_.size()));
            for (const DugEnemySpawnRequest& request : dugEnemySpawnRequests) {
                schedulePendingBuriedEnemySpawn(
                    DungeonTile{
                        tileMap_.worldToTile(request.position.x),
                        tileMap_.worldToTile(request.position.y),
                    },
                    request.position,
                    request.depthRank);
            }
            updateBossSpawn();
        }

        if (gameplayRewardsEnabled()) {
            updateOrbitAreaEffects(time.deltaSeconds(), effectDiscoveries);
            updateOrbitGroundEffects(time.deltaSeconds(), effectDiscoveries);
        }

        const bool hiddenMonicaDuel = currentStageIsHiddenMonicaDuel();
        const bool capturedBossOwned = hasCapturedBossForCurrentStage();
        const bool allowBossCapture = (currentStageCleared() || hiddenMonicaDuel) && !capturedBossOwned;
        const std::string bossCaptureObjectId = (currentStageCleared() || hiddenMonicaDuel)
            ? currentStageBossCaptureObjectId()
            : std::string{};
        const CollisionRect stealViewBounds = cameraWorldRect(camera_);
        {
            FrameProfileScope profile("Enemies.update");
            enemies_.update(
                player_,
                spellRing_,
                inventory_,
                tileMap_,
                time.deltaSeconds(),
                time.totalSeconds(),
                false,
                balance_,
                enemyCatalog_,
                objectCatalog_,
                worldDrops_,
                [this](int amount, Vec2 origin) {
                    return grantDungeonMoney(amount, origin);
                },
                [this](int amount, Vec2 origin) {
                    return takeDungeonMoney(amount, origin);
                },
                witchSelfLightCenter(player_.position),
                std::vector<LightSource>{},
                effectDispatcher_,
                projectiles_,
                magic_,
                stealViewBounds,
                allowBossCapture,
                bossCaptureObjectId,
                currentStageIsRoguelike() ? &mainCapturedEnemyIds_ : nullptr,
                effectDiscoveryEvents,
                gameplayRewardsEnabled() ? &encyclopedia_ : nullptr);
        }
        for (const CapturedExplosionRequest& explosionRequest : digging_.capturedExplosionRequests()) {
            handleCapturedExplosion(explosionRequest);
        }
        updateCapturedUtilityBehaviors(time.deltaSeconds());
        updateCapturedProjectileBehaviors(time.deltaSeconds());
        {
            FrameProfileScope profile("Projectiles.update");
            projectiles_.update(
                player_,
                spellRing_,
                enemies_,
                tileMap_,
                time.deltaSeconds(),
                effectDispatcher_,
                objectCatalog_,
                effectDiscoveryEvents,
                gameplayRewardsEnabled() ? &encyclopedia_ : nullptr);
        }
        {
            FrameProfileScope profile("Magic.update");
            magic_.update(player_, spellRing_, enemies_, tileMap_, time.deltaSeconds());
        }
        for (const ProjectileSoundEvent& event : projectiles_.consumeSoundEvents()) {
            playAudioSeAt(event.cueId, event.position, event.volumeScale, event.pitchScale);
        }
        bool magicCastSound = false;
        bool magicImpactSound = false;
        Vec2 magicCastSoundPosition{};
        Vec2 magicImpactSoundPosition{};
        for (MagicSoundEvent event : magic_.consumeSoundEvents()) {
            switch (event.kind) {
            case MagicSoundKind::Cast:
                magicCastSound = true;
                magicCastSoundPosition = event.position;
                break;
            case MagicSoundKind::Impact:
                magicImpactSound = true;
                magicImpactSoundPosition = event.position;
                break;
            }
        }
        if (magicCastSound) {
            playAudioSeAt(AudioSeMagicCast, magicCastSoundPosition);
        }
        if (magicImpactSound) {
            playAudioSeAt(AudioSeMagicImpact, magicImpactSoundPosition);
        }
        for (const MagicFxSoundEvent& event : magicFx_.consumeSoundEvents()) {
            playAudioSeAt(event.cueId, event.position, event.volumeScale, event.pitchScale);
        }
        bool capturedEnemyThisFrame = false;
        for (const CaptureResult& capture : enemies_.consumeCaptureResults()) {
            if (!gameplayRewardsEnabled()) {
                break;
            }
            const bool captured = handleCaptureResult(capture);
            if (captured) {
                handleHiddenDungeonNpcCaptureResult(capture);
            }
            capturedEnemyThisFrame = captured || capturedEnemyThisFrame;
        }
        updateCaptureAbsorbAnimations(time.deltaSeconds());
        updateDungeonMinimap(time.totalSeconds());
        if (gameplayRewardsEnabled()) {
            handleRingItemBreakEvents(effectDiscoveryEvents);
        }

        std::vector<CapturedExplosionRequest> capturedExplosionRequests;
        const auto codexEnemyNameForEvent = [this](const EnemyEvent& event) {
            const auto enemyIt = enemyCatalog_.enemiesById.find(event.enemyId);
            if (enemyIt == enemyCatalog_.enemiesById.end()) {
                return event.enemyName;
            }
            return enemyIt->second.name.empty() ? enemyIt->second.id : enemyIt->second.name;
        };
        for (const EnemyEvent& event : enemies_.events()) {
            if (gameplayRewardsEnabled() && !event.enemyId.empty()) {
                encyclopedia_.noteEnemyDiscovered(event.enemyId, codexEnemyNameForEvent(event), event.position);
            }
            if (event.type == EnemyEventType::CapturedExplosion) {
                CapturedExplosionRequest request;
                request.position = event.position;
                if (event.effectRadius > 0.0f) {
                    request.radius = event.effectRadius;
                }
                if (event.damageAmount >= 0) {
                    request.damage = event.damageAmount;
                }
                if (event.terrainRadius > 0.0f) {
                    request.terrainRadius = event.terrainRadius;
                }
                if (event.terrainDamage >= 0) {
                    request.terrainDamage = event.terrainDamage;
                }
                capturedExplosionRequests.push_back(request);
            }
        }
        for (const CapturedExplosionRequest& explosionRequest : capturedExplosionRequests) {
            handleCapturedExplosion(explosionRequest);
        }
        if (gameplayRewardsEnabled()) {
            handleRingItemBreakEvents(effectDiscoveryEvents);
        }

        bool bossDefeated = false;
        Vec2 bossDefeatPosition{};
        const std::vector<RingImpactSoundPlayback> enemyImpactSounds =
            resolveRingImpactSoundEvents(enemies_.impactSoundEvents(), 4);
        for (const RingImpactSoundPlayback& sound : enemyImpactSounds) {
            playAudioSeAt(sound.cueId, sound.position, sound.volumeScale, sound.pitchScale);
        }
        for (const EnemySoundEvent& sound : enemies_.consumeSoundEvents()) {
            playAudioSeAt(sound.cueId, sound.position, sound.volumeScale, sound.pitchScale);
        }
        for (const EnemyEvent& event : enemies_.events()) {
            recordAutoSimulationEnemyEvent(event);
            if (event.type == EnemyEventType::Alert) {
                playAudioSeAt(AudioSeEnemyAlert, event.position);
            } else if (event.type == EnemyEventType::Attack) {
                if (event.effectId == "ring_slow_bite") {
                    playAudioSeAt(AudioSeRingSlowBite, event.position);
                } else if (event.effectId == "chest_bite_lunge") {
                    playAudioSeAt(AudioSeEnemyMimicBite, event.position);
                } else {
                    playAudioSeAt(AudioSeEnemyAttack, event.position);
                }
            } else if (event.type == EnemyEventType::Shoot) {
                if (event.effectId == "junk_throw") {
                    playAudioSeAt(AudioSeJunkCrabThrow, event.position);
                } else if (event.effectId == "wind_blow") {
                    playAudioSeAt(AudioSeEnemyShoot, event.position);
                }
            } else if (event.type == EnemyEventType::HealCast) {
                playAudioSeAt(AudioSeEnemyHeal, event.position);
                magicFx_.playHealPulse(event.position, 24.0f);
            } else if (event.type == EnemyEventType::Heal) {
                if (event.healAmount > 0) {
                    effects_.spawnDamagePopup(event.position, event.healAmount, DamagePopupStyle::Heal);
                }
                magicFx_.playHealPulse(event.position, 18.0f);
            } else if (event.type == EnemyEventType::ExplosionWarningTick) {
                playAudioSeAt(AudioSeExplosionTick, event.position);
            } else if (event.type == EnemyEventType::Explode) {
                const float radius = event.effectRadius > 0.0f ? event.effectRadius : 48.0f * ExplosionRadiusScale;
                effects_.spawnExplosion(event.position, radius);
                addScreenShake(std::clamp(3.5f + radius * 0.035f, 4.5f, 8.0f), 0.20f);
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
                const bool emergeImpact = event.effectId == "jump_start";
                const bool landingImpact = event.effectId == "landing";
                const bool moleBurrowDebrisImpact = burrowImpact || emergeImpact;
                SmokeBurstOptions smoke;
                smoke.count = wallStunImpact ? 14 : ((moleBurrowDebrisImpact || landingImpact) ? 16 : 10);
                smoke.size = wallStunImpact ? 24.0f : ((moleBurrowDebrisImpact || landingImpact) ? 22.0f : 18.0f);
                smoke.spreadRadius = wallStunImpact ? 16.0f : ((moleBurrowDebrisImpact || landingImpact) ? 14.0f : 10.0f);
                smoke.colorA = {192, 144, 82, 192};
                smoke.colorB = {110, 82, 58, 150};
                effects_.spawnAttackImpactBurst(event.position, smoke, wallStunImpact);
                if (moleBurrowDebrisImpact) {
                    effects_.spawnTileBreak(event.position, TileType::Dirt, {154, 110, 66, 235}, false, 2.0f, 2);
                }
            } else if (event.type == EnemyEventType::TerrainHit) {
                const Vec2 direction = lengthSquared(event.effectDirection) > 0.0001f
                    ? event.effectDirection
                    : event.position - player_.position;
                effects_.spawnDigHit(event.position, direction, event.terrainColor);
            } else if (event.type == EnemyEventType::TerrainBreak) {
                effects_.spawnTileBreak(event.position, event.terrainTileType, event.terrainColor);
            } else if (event.type == EnemyEventType::Inspected) {
                const auto enemyIt = enemyCatalog_.enemiesById.find(event.enemyId);
                if (gameplayRewardsEnabled() && enemyIt != enemyCatalog_.enemiesById.end()) {
                    encyclopedia_.noteEnemyInspected(enemyIt->second, event.position);
                }
            } else if (event.type == EnemyEventType::BossResolved) {
                handleDungeonEventEnemyEvent(event);
                if (!isFinalBossFirstClearEncounter(bossEncounter_.purpose)) {
                    effects_.spawnAreaPulse(event.position, 92.0f, {255, 214, 110, 210});
                    addScreenShake(6.0f, 0.24f);
                }
                bossDefeated = true;
                bossDefeatPosition = event.position;
            } else if (event.type == EnemyEventType::Death || event.type == EnemyEventType::BossDeath) {
                const bool hiddenNpcEvent = handleHiddenDungeonNpcEnemyEvent(event);
                handleDungeonEventEnemyEvent(event);
                const bool eventRewardsEnabled = gameplayRewardsEnabled() && !event.suppressRewards && !hiddenNpcEvent;
                if (eventRewardsEnabled) {
                    ++runStats_.defeatedEnemies;
                }
                const bool playEnemyDefeatSound =
                    event.type == EnemyEventType::Death && !capturedEnemyThisFrame && eventRewardsEnabled;
                effects_.spawnEnemyDeath(event.position, playEnemyDefeatSound);
                addScreenShake(event.type == EnemyEventType::BossDeath ? 8.0f : 1.5f, event.type == EnemyEventType::BossDeath ? 0.28f : 0.08f);
                std::mt19937& rng = lootRuntimeRng();
                if (eventRewardsEnabled && event.moneyDrop > 0) {
                    grantDungeonMoney(
                        event.moneyDrop,
                        safeLootLandingPosition(event.position, rng));
                }
                const bool bossDeath = event.type == EnemyEventType::BossDeath;
                const float manaChance = bossDeath ? balance_.bossManaDropChance : balance_.enemyManaDropChance;
                const float moonChance = bossDeath ? balance_.bossMoonFragmentChance : balance_.enemyMoonFragmentChance;
                if (eventRewardsEnabled && rollChance(manaChance, rng)) {
                    const int amount = bossDeath ? scaledLootAmount(std::uniform_int_distribution<int>(1, 3)(rng), 1.0f) : 1;
                    worldDrops_.spawnMaterialDrop(
                        MaterialType::ManaDrop,
                        amount,
                        safeLootLandingPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng));
                }
                if (eventRewardsEnabled && !currentStageIsRoguelike() && rollChance(moonChance, rng)) {
                    const int amount = bossDeath ? scaledLootAmount(std::uniform_int_distribution<int>(1, 3)(rng), 1.0f) : 1;
                    worldDrops_.spawnMaterialDrop(
                        MaterialType::MoonFragment,
                        amount,
                        safeLootLandingPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng));
                }
                if (eventRewardsEnabled && !event.enemyId.empty()) {
                    encyclopedia_.noteEnemyDefeated(event.enemyId, codexEnemyNameForEvent(event), event.position);
                }
                if (eventRewardsEnabled && event.type == EnemyEventType::BossDeath) {
                    bossDefeated = true;
                    bossDefeatPosition = event.position;
                }
            } else if (event.type == EnemyEventType::Steal) {
                const std::string stolenLabel = enemyStealLogLabel(event, objectCatalog_);
                if (!stolenLabel.empty()) {
                    const std::string enemyName = !event.enemyName.empty()
                        ? event.enemyName
                        : (event.enemyId.empty() ? std::string("敵") : event.enemyId);
                    pushDungeonLog(enemyName + "は" + stolenLabel + "を盗んだ");
                }
            } else if (event.type == EnemyEventType::RewardDrop) {
                if (!gameplayRewardsEnabled()) {
                    continue;
                }
                std::mt19937& rng = lootRuntimeRng();
                WeightedObjectLootProfile lootProfile;
                if (!weightedObjectLootProfileForDropProfile(event.objectDropProfile, lootProfile)) {
                    logError("[warning] CapturedRewardLoot: unknown reward profile \"" + event.objectDropProfile + "\"; no item drop");
                    continue;
                }
                const int depthRank = currentStageIsRoguelike()
                    ? roguelikeDepthRankForWorldPosition(event.position)
                    : lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, event.position);
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
                if (!gameplayRewardsEnabled() || event.moneyDrop <= 0) {
                    continue;
                }
                std::mt19937& rng = lootRuntimeRng();
                grantDungeonMoney(
                    event.moneyDrop,
                    safeLootLandingPosition(event.position, rng));
            } else if (event.type == EnemyEventType::MaterialDrop) {
                if (!gameplayRewardsEnabled()) {
                    continue;
                }
                if (event.materialDropType == MaterialType::Count || event.materialDropCount <= 0) {
                    logError("[warning] EnemyMaterialDrop: invalid material drop event; no material drop");
                    continue;
                }
                std::mt19937& rng = lootRuntimeRng();
                const MaterialType materialType = currentStageIsRoguelike()
                    ? normalizeRoguelikeMaterialDrop(event.materialDropType, rng)
                    : event.materialDropType;
                worldDrops_.spawnMaterialDrop(
                    materialType,
                    event.materialDropCount,
                    safeLootLandingPosition(event.position, rng),
                    runStats_.elapsedSeconds,
                    makeWorldLootJumpMotion(event.position, rng));
            } else if (event.type == EnemyEventType::ObjectDrop) {
                if (!gameplayRewardsEnabled()) {
                    continue;
                }
                std::mt19937& rng = lootRuntimeRng();
                const int dropCount = std::max(1, event.objectDropCount);
                if (!event.objectDropProfile.empty()) {
                    WeightedObjectLootProfile lootProfile;
                    if (!weightedObjectLootProfileForDropProfile(event.objectDropProfile, lootProfile)) {
                        logError("[warning] EnemyDropLoot: unknown drop profile \"" + event.objectDropProfile + "\"; no item drop");
                    } else {
                        const int depthRank = currentStageIsRoguelike()
                            ? roguelikeDepthRankForWorldPosition(event.position)
                            : lootDepthRankForWorldPosition(tileMap_, dungeonLayout_, currentStageId_, event.position);
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
                    if (objectExcludedFromDungeonDrops(event.objectDropId) ||
                        (currentStageIsRoguelike() && !roguelikeObjectAllowed(event.objectDropId))) {
                        continue;
                    }
                    if (event.objectDropInstance) {
                        worldDrops_.spawnObjectInstanceDrop(
                            objectCatalog_,
                            *event.objectDropInstance,
                            safeLootLandingPosition(event.position, rng),
                            runStats_.elapsedSeconds,
                            makeWorldLootJumpMotion(event.position, rng),
                            false,
                            event.objectDropRuntimeItem ? &*event.objectDropRuntimeItem : nullptr);
                    } else {
                        for (int i = 0; i < dropCount; ++i) {
                            worldDrops_.spawnObjectDrop(
                                objectCatalog_,
                                event.objectDropId,
                                safeLootLandingPosition(event.position, rng),
                                runStats_.elapsedSeconds,
                                makeWorldLootJumpMotion(event.position, rng));
                        }
                    }
                } else if (event.objectDropInstance) {
                    if (objectExcludedFromDungeonDrops(event.objectDropInstance->objectId) ||
                        (currentStageIsRoguelike() && !roguelikeObjectAllowed(event.objectDropInstance->objectId))) {
                        continue;
                    }
                    worldDrops_.spawnObjectInstanceDrop(
                        objectCatalog_,
                        *event.objectDropInstance,
                        safeLootLandingPosition(event.position, rng),
                        runStats_.elapsedSeconds,
                        makeWorldLootJumpMotion(event.position, rng),
                        false,
                        event.objectDropRuntimeItem ? &*event.objectDropRuntimeItem : nullptr);
                }
            } else if (event.type == EnemyEventType::CapturedExplosion) {
                continue;
            } else if (event.type == EnemyEventType::AttackHit) {
                effects_.spawnEnemyHit(event.position, event.effectId, !event.ringItemImpact);
                if (event.damageAmount >= 0) {
                    effects_.spawnDamagePopup(
                        event.position,
                        event.damageAmount,
                        enemyDamagePopupStyle(event));
                }
            } else if (event.damageAmount >= 0) {
                effects_.spawnDamagePopup(event.position, event.damageAmount, DamagePopupStyle::Enemy);
            }
        }
        int playerDamageTotal = 0;
        for (const PlayerDamageEvent& event : player_.damageEvents) {
            recordAutoSimulationPlayerDamage(event);
            effects_.spawnDamagePopup(event.position, event.amount, DamagePopupStyle::Player);
            playerDamageTotal += std::max(0, event.amount);
        }
        if (!player_.damageEvents.empty()) {
            playAudioSeAt(AudioSePlayerDamage, player_.position);
            if (shouldPlayPlayerPinchDamageSe(player_.hp, player_.maxHp, playerDamageTotal)) {
                playAudioSeAt(AudioSePlayerPinch, player_.position);
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
        for (const EffectSoundEvent& event : effects_.consumeSoundEvents()) {
            playAudioSeAt(event.cueId, event.position, event.volumeScale, event.pitchScale);
        }
        for (const MagicFxSoundEvent& event : magicFx_.consumeSoundEvents()) {
            playAudioSeAt(event.cueId, event.position, event.volumeScale, event.pitchScale);
        }
        if (gameplayRewardsEnabled()) {
            applyEffectDiscoveries(effectDiscoveries);
            syncEncyclopediaFromInventoryAndRing();
        }
        {
            FrameProfileScope profile("Fx.update");
            updateAmbientParticleEffects(time.deltaSeconds());
            wetGround_.update(time.deltaSeconds());
            wetGround_.erasePendingGroundLines(groundLines_);
            groundLines_.update(time.deltaSeconds());
            moneyGainFx_.update(time.deltaSeconds(), player_.position);
            magicFx_.update(time.deltaSeconds());
            effects_.update(time.deltaSeconds());
        }
        for (const MoneyGainLandingEvent& event : moneyGainFx_.consumeLandingEvents()) {
            playAudioSeAt(AudioSeMoneyDrop, event.position, 1.0f, event.pitchScale);
        }
        for (const MoneyGainArrivalEvent& event : moneyGainFx_.consumeArrivalEvents()) {
            playAudioSeAt(AudioSeMoneyArrive, event.position, 0.56f, event.pitchScale);
        }
        const int pendingXp = enemies_.consumePendingXp();
        if (gameplayRewardsEnabled()) {
            gainPlayerXp(pendingXp);
        }
        if (!deathActive && updateIntroTutorial(input, time.deltaSeconds())) {
            return;
        }
        if (!deathActive && bossDefeated) {
            beginBossDefeatSequence(bossDefeatPosition);
            return;
        }
        if (player_.hp <= 0) {
            beginPlayerDeathSequence();
            deathActive = true;
        }
        updatePlayerDeathSequence(time.deltaSeconds());
        if (!deathActive && levels_.isChoosing()) {
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
        return;
    } else if (fileName == "credits.txt") {
        loadTitleCreditsData();
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        return;
    } else if (isEndingKamishibaiDataFileName(fileName)) {
        if (mode_ == ScreenMode::EndingKamishibai) {
            if (endingKamishibaiReplay_) {
                startEndingReplayKamishibai(endingKamishibaiKind_);
            } else {
                startEndingKamishibai(endingKamishibaiKind_);
            }
        } else {
            loadEndingKamishibaiData();
        }
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        return;
    } else if (fileName == "hitboxes.cfg" || fileName == "enemy_hitboxes.cfg") {
        loadHitboxData();
        enemies_.setHitboxCatalog(&hitboxes_);
        rebuildEnemyHitboxEditList();
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        return;
    } else if (fileName == "enemy_placements.cfg") {
        loadEnemyPlacementData();
        enemies_.setPlacementCatalog(&enemyPlacements_);
        rebuildEnemyPlacementEditList();
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        return;
    } else if (fileName == "enemy_shadows.cfg") {
        loadEnemyShadowData();
        enemies_.setShadowCatalog(&enemyShadows_);
        rebuildEnemyShadowEditList();
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
        return;
    } else if (std::filesystem::path(changedPath).extension() == ".story") {
        loadStoryEvents();
        reloadNotice_ = "Hot reload: " + changedPath;
        reloadNoticeTimer_ = 3.0f;
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
        reloadNotice_ = "Hot reload: " + changedPath;
    } else {
        reloadNotice_ = message;
    }
    reloadNoticeTimer_ = 3.0f;
}

} // namespace majo
