#include "game/GameInternal.hpp"

#include "engine/InputHelpGlyph.hpp"
#include "game/CharacterSprite.hpp"
#include "game/EnemyImageRenderer.hpp"
#include "game/MenuIconImage.hpp"
#include "game/NpcCharacterVisual.hpp"
#include "game/PlayerEquipmentVisual.hpp"
#include "game/RingDisplayName.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <unordered_set>

namespace majo {

namespace {

constexpr std::string_view AudioSeNewItemJingle = "se.item.new.jingle";
constexpr std::string_view AudioSeEffectDiscovery = "se.discovery.effect";
constexpr std::string_view BaseFacilityWindowHelpText = "↑/↓ 選択  F/Enter 決定  Esc 戻る";
constexpr std::string_view MiningToolCategory = "\xE6\x8E\x98\xE5\x89\x8A";
constexpr std::string_view RescueShovelObjectId = "item_shovel";
constexpr std::string_view RescueTorchObjectId = "item_torch";
constexpr float BaseMiningRescueDropDurationSeconds = 1.05f;
constexpr float BaseMiningRescueDropEndSeconds = 1.55f;
constexpr std::string_view BaseRandomTalkTriggerPrefix = "base_random_talk:";
constexpr int BookshelfEndingReplayMenuIndex = BookshelfMenuItemCount;
constexpr float BookshelfMenuChoiceGap = 16.0f;
constexpr float BookshelfEndingCommandMinWidth = 240.0f;
constexpr float BaseStoryLookaroundSeconds = 0.9f;
constexpr float BaseStoryMinWalkSeconds = 0.18f;

bool isBaseRandomTalkSpeaker(std::string_view speakerId)
{
    return speakerId == "merchant" || speakerId == "processor";
}

bool isBasePresentationCommand(std::string_view name)
{
    return name == "base_actor_offset" ||
        name == "base_actor_reset" ||
        name == "base_wait" ||
        name == "base_fade" ||
        name == "base_player_place" ||
        name == "base_player_walk" ||
        name == "base_player_lookaround";
}

float parseStoryCommandFloat(const DialogueCommand& command, std::size_t index, float fallback)
{
    if (index >= command.args.size()) {
        return fallback;
    }

    errno = 0;
    char* end = nullptr;
    const float value = std::strtof(command.args[index].c_str(), &end);
    if (end != command.args[index].c_str() && end != nullptr && *end == '\0' && errno == 0 && std::isfinite(value)) {
        return value;
    }
    return fallback;
}

Vec2 storyTileOffset(float tileX, float tileY)
{
    const float tileSize = static_cast<float>(balance::TileSize);
    return {tileX * tileSize, tileY * tileSize};
}

void applyBaseStoryFacilityOffsets(
    std::vector<BaseFacility>& facilities,
    const std::unordered_map<std::string, Vec2>& offsets)
{
    if (offsets.empty()) {
        return;
    }

    for (BaseFacility& facility : facilities) {
        const auto it = offsets.find(std::string(facility.facilityId));
        if (it != offsets.end()) {
            facility.rect.pos += it->second;
        }
    }
}

std::string baseRandomTalkTrigger(std::string_view speakerId)
{
    if (!isBaseRandomTalkSpeaker(speakerId)) {
        return {};
    }
    std::string trigger{BaseRandomTalkTriggerPrefix};
    trigger += speakerId;
    return trigger;
}

std::mt19937& baseTalkSessionRng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

void drawEquippedStaffOnPlayer(
    Renderer& renderer,
    const InventorySystem& inventory,
    Vec2 footAnchor,
    int playerFrame,
    bool flipHorizontal)
{
    if (const InventoryObjectInstance* staffInstance = inventory.equippedStaffInstance()) {
        const PlayerHeldStaffDrawContext context{
            .footAnchor = footAnchor,
            .spriteFrame = playerFrame,
            .flipHorizontal = flipHorizontal,
            .spriteAnchor = {PlayerSpriteAnchorX, PlayerSpriteAnchorY},
        };
        if (drawPlayerHeldStaff(renderer, staffInstance->item, context)) {
            drawPlayerHeldStaffHandOverlay(renderer, context);
        }
    }
}

constexpr float NewItemJingleFallbackSeconds = 0.92f;
constexpr float HiddenBaseNpcHitCooldownSeconds = 0.24f;
constexpr std::array<float, SpellRingCount> RingWorkshopRadiusMaxMetersPerLevel{{0.12f, 0.18f, 0.24f}};
constexpr std::array<float, SpellRingCount> RingWorkshopRadiusMinMetersPerLevel{{0.08f, 0.12f, 0.16f}};
constexpr std::array<float, SpellRingCount> RingWorkshopSpeedMetersPerSecondPerLevel{{0.20f, 0.30f, 0.25f}};
constexpr float RingWorkshopWeightLimitKgPerLevel = 1.0f;
constexpr float RingWorkshopShiftDistanceMetersPerLevel = 0.50f;
constexpr float RingWorkshopThrowDistanceMetersPerLevel = 0.40f;
constexpr float RingWorkshopThrowCooldownSecondsPerLevel = 0.18f;
constexpr float DungeonDetailImageHeight = 96.0f;
constexpr float DungeonDetailEnemyIconSize = 42.5f;
constexpr float DungeonDetailEnemyIconGap = 3.0f;
struct RoguelikeDepartureRuleText {
    std::string_view text;
    std::string_view emphasis;
};

constexpr std::array<RoguelikeDepartureRuleText, 7> RoguelikeDepartureRules{{
    {"アイテムや装備の持ち込み不可", "持ち込み不可"},
    {"ルネのレベルは1からスタート", "レベルは1から"},
    {"ダンジョン内には様々なアイテムが出現するが、今まで入手したことのないアイテムは出現しない", "今まで入手したことのないアイテムは出現しない"},
    {"本ダンジョン内では、アイテムにおいて未発見の「リングに乗せた時の効果」が発動しても、発見したことにならない", "発見したことにならない"},
    {"アイテム「虫眼鏡」は登場しない", "登場しない"},
    {"今までアイテム「虫取りアミ」で捕獲したことがない敵は、本ダンジョン内で捕獲することができない", "捕獲することができない"},
    {"本ダンジョン内で得たものは、帰還に成功しない限りすべて失う", "帰還に成功しない限りすべて失う"},
}};

float metersToWorldDistance(float meters)
{
    return meters * static_cast<float>(balance::TileSize);
}

float worldDistanceToMeters(float distance)
{
    return distance / static_cast<float>(balance::TileSize);
}

float linearMetersPerSecondForAngularSpeed(float angularSpeed, float radius)
{
    return angularSpeed * worldDistanceToMeters(radius);
}

float ringWeightPenaltyReliefPercentForLevel(int level)
{
    return static_cast<float>(std::max(0, level)) * 10.0f;
}

bool isTutorialStoryTrigger(std::string_view trigger)
{
    return trigger.rfind("tutorial:", 0) == 0;
}

bool isMiningToolObject(const ItemData& item)
{
    return item.category == MiningToolCategory;
}

bool stageLooksRoguelike(const StageDefinition& stage)
{
    return stage.id == "stage_04_astral_mine" ||
        stage.type == "ローグライク" ||
        stage.generationProfile == "astral_rogue";
}

std::string stageDetailDescription(const StageDefinition& stage)
{
    if (!stage.detail.description.empty()) {
        return stage.detail.description;
    }
    if (stageLooksRoguelike(stage)) {
        return "入るたび姿を変える底なしの迷宮。\n持ち込み不可で、初期ステータスから深層を目指す。";
    }
    return "採掘しながら奥へ進むダンジョン。\nワープポイントを見つけると次回以降の出発地点にできる。";
}

std::string stageDetailDifficulty(const StageDefinition& stage)
{
    if (!stage.detail.difficulty.empty()) {
        return stage.detail.difficulty;
    }
    if (stageLooksRoguelike(stage)) {
        return "不明";
    }
    if (stage.terrainHardnessMultiplier >= 2.0 || stage.goalDistanceTiles >= 520) {
        return "むずかしい";
    }
    if (stage.goalDistanceTiles >= 400 || stage.terrainHardnessMultiplier >= 1.3) {
        return "ふつう";
    }
    return "やさしい";
}

std::string stageDetailSize(const StageDefinition& stage)
{
    if (!stage.detail.size.empty()) {
        return stage.detail.size;
    }
    if (stageLooksRoguelike(stage)) {
        return "底なし";
    }
    if (stage.goalDistanceTiles >= 520) {
        return "かなり広い";
    }
    if (stage.goalDistanceTiles >= 400) {
        return "広い";
    }
    return "広くない";
}

std::string stageDetailWallHardness(const StageDefinition& stage)
{
    if (!stage.detail.wallHardness.empty()) {
        return stage.detail.wallHardness;
    }
    if (stageLooksRoguelike(stage)) {
        return "不明";
    }
    if (stage.terrainHardnessMultiplier >= 1.8) {
        return "かため";
    }
    if (stage.terrainHardnessMultiplier >= 1.2) {
        return "ふつう";
    }
    return "やわらかめ";
}

std::string stageDetailTerrainComplexity(const StageDefinition& stage)
{
    if (!stage.detail.terrainComplexity.empty()) {
        return stage.detail.terrainComplexity;
    }
    if (stageLooksRoguelike(stage)) {
        return "不明";
    }
    if (stage.detourRate >= 0.42 || stage.branchDensity >= 0.38) {
        return "複雑";
    }
    if (stage.detourRate >= 0.34 || stage.branchDensity >= 0.30) {
        return "少し";
    }
    return "そこまで";
}

std::vector<const EnemyDefinition*> stageDetailEnemies(const StageDefinition& stage, const EnemyCatalog& catalog)
{
    std::vector<const EnemyDefinition*> enemies;
    std::unordered_set<std::string> seen;
    const auto addEnemy = [&](std::string_view id) {
        if (id.empty()) {
            return;
        }
        auto it = catalog.enemiesById.find(std::string(id));
        if (it == catalog.enemiesById.end() || isCodexHiddenEnemy(it->second)) {
            return;
        }
        if (seen.insert(it->second.id).second) {
            enemies.push_back(&it->second);
        }
    };

    for (const std::string& enemyId : stage.detail.enemyIds) {
        addEnemy(enemyId);
    }
    if (!enemies.empty()) {
        return enemies;
    }

    struct WeightedEnemy {
        const EnemyDefinition* enemy = nullptr;
        int firstDepth = 99;
        double totalWeight = 0.0;
    };
    std::vector<WeightedEnemy> weighted;
    for (const EnemyDefinition& enemy : catalog.enemies) {
        if (enemy.id.empty() || isCodexHiddenEnemy(enemy)) {
            continue;
        }
        WeightedEnemy candidate{.enemy = &enemy};
        for (int depth = 1; depth <= 9; ++depth) {
            const double weight = enemySpawnWeightFor(enemy, stage.id, depth);
            if (weight <= 0.0) {
                continue;
            }
            candidate.firstDepth = std::min(candidate.firstDepth, depth);
            candidate.totalWeight += weight;
        }
        if (candidate.totalWeight > 0.0) {
            weighted.push_back(candidate);
        }
    }
    std::stable_sort(weighted.begin(), weighted.end(), [](const WeightedEnemy& left, const WeightedEnemy& right) {
        if (left.firstDepth != right.firstDepth) {
            return left.firstDepth < right.firstDepth;
        }
        if (left.enemy->baseLevel != right.enemy->baseLevel) {
            return left.enemy->baseLevel < right.enemy->baseLevel;
        }
        return left.enemy->id < right.enemy->id;
    });
    enemies.reserve(weighted.size());
    for (const WeightedEnemy& entry : weighted) {
        enemies.push_back(entry.enemy);
    }
    return enemies;
}

void drawDungeonDetailFallbackImage(Renderer& renderer, UiRect rect, const StageDefinition& stage)
{
    const Color frame{102, 92, 120, 210};
    const Color path{218, 194, 128, 190};
    const Color star{255, 236, 170, 230};
    Color top{20, 28, 44, 235};
    Color bottom{42, 34, 54, 235};
    Color wall{78, 76, 94, 230};
    if (stage.terrainProfile == "soft_stardust") {
        top = {28, 34, 58, 235};
        bottom = {54, 48, 82, 235};
        wall = {92, 86, 118, 230};
    } else if (stage.terrainProfile == "junk_mixed") {
        top = {34, 34, 42, 235};
        bottom = {66, 56, 50, 235};
        wall = {104, 88, 74, 230};
    } else if (stage.terrainProfile == "hard_star_core") {
        top = {24, 32, 56, 235};
        bottom = {62, 42, 76, 235};
        wall = {90, 76, 112, 230};
    } else if (stage.terrainProfile == "chaos_astral") {
        top = {20, 24, 46, 235};
        bottom = {54, 30, 74, 235};
        wall = {68, 52, 110, 230};
    }

    renderer.fillGradientRect(rect.pos, rect.size, top, bottom, GradientDirection::TopToBottom);
    renderer.drawRect(rect.pos, rect.size, frame);

    const float cell = 16.0f;
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 18; ++x) {
            const int noise = (x * 37 + y * 53 + static_cast<int>(stage.displayOrder)) % 11;
            if (noise <= 3) {
                renderer.fillRect(
                    rect.pos + Vec2{8.0f + static_cast<float>(x) * cell, 7.0f + static_cast<float>(y) * cell},
                    {cell - 3.0f, cell - 3.0f},
                    wall);
            }
        }
    }

    Vec2 previous{rect.pos.x + 22.0f, rect.pos.y + rect.size.y - 22.0f};
    for (int i = 1; i <= 7; ++i) {
        const float t = static_cast<float>(i) / 7.0f;
        const float wobble = std::sin(t * 8.4f + static_cast<float>(stage.displayOrder)) * 18.0f;
        const Vec2 next{
            rect.pos.x + 22.0f + t * (rect.size.x - 44.0f),
            rect.pos.y + rect.size.y - 22.0f - t * (rect.size.y - 38.0f) + wobble,
        };
        renderer.drawLine(previous + Vec2{0.0f, 1.0f}, next + Vec2{0.0f, 1.0f}, {0, 0, 0, 120});
        renderer.drawLine(previous, next, path);
        previous = next;
    }

    for (int i = 0; i < 8; ++i) {
        const float x = rect.pos.x + 18.0f + static_cast<float>((i * 41 + stage.displayOrder) % 260);
        const float y = rect.pos.y + 12.0f + static_cast<float>((i * 23 + stage.displayOrder) % 70);
        renderer.fillCircle({x, y}, i % 3 == 0 ? 2.5f : 1.7f, star);
    }
}

void drawDungeonDetailImage(Renderer& renderer, UiRect panel, float& y, const StageDefinition& stage)
{
    constexpr float ImageBottomGap = 10.0f;
    const UiRect content = uiSubPanelContentRect(panel);
    const UiRect imageRect{{content.pos.x, y}, {content.size.x, DungeonDetailImageHeight}};
    bool drewImage = false;
    if (!stage.detail.imagePath.empty()) {
        Vec2 imageSize{};
        if (renderer.getImageSize(stage.detail.imagePath, imageSize, TextureFilter::Linear) &&
            imageSize.x > 0.0f &&
            imageSize.y > 0.0f) {
            const float scale = std::min(imageRect.size.x / imageSize.x, imageRect.size.y / imageSize.y);
            const Vec2 drawSize{imageSize.x * scale, imageSize.y * scale};
            ImageDrawOptions options;
            drewImage = renderer.drawImage(
                stage.detail.imagePath,
                imageRect.pos + imageRect.size * 0.5f,
                drawSize,
                options,
                TextureFilter::Linear);
        }
    }
    if (!drewImage) {
        drawDungeonDetailFallbackImage(renderer, imageRect, stage);
    }
    y += DungeonDetailImageHeight + ImageBottomGap;
}

void drawDungeonWarpProgressLine(
    Renderer& renderer,
    const ObjectCatalog& objectCatalog,
    UiRect panel,
    float& y,
    int discoveredWarpPoints,
    int totalWarpPoints)
{
    constexpr float LabelWidth = 106.0f;
    constexpr float MinLineHeight = 31.0f;
    const float labelX = panel.pos.x + ui::SubPanelPadding.x;
    const float valueX = labelX + LabelWidth;
    renderer.drawText({labelX, y}, "ワープ", ui::TextMuted, 2);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d/%d", discoveredWarpPoints, totalWarpPoints);
    InlineItemTextStyle style;
    style.text = ui::Text;
    style.scale = 2;
    style.iconTextGap = 6.0f;
    style.iconScale = 23.0f / std::max(1.0f, renderer.measureText("0", style.scale).y);
    drawInlineItemText(
        renderer,
        objectCatalog,
        {valueX, y - 2.0f},
        inlineWorldIconTag(worldIconKey(WorldIconId::WarpPoint)) + std::string(buffer),
        style);
    y += MinLineHeight;
}

void drawDungeonDetailEnemyIcons(
    Renderer& renderer,
    UiRect panel,
    float& y,
    const StageDefinition& stage,
    const EnemyCatalog& enemyCatalog,
    const EncyclopediaSystem& encyclopedia,
    float animationSeconds)
{
    const UiRect content = uiSubPanelContentRect(panel);
    renderer.drawText({content.pos.x, y}, "出現する敵", ui::TextMuted, 2);
    y += 22.0f;

    const std::vector<const EnemyDefinition*> enemies = stageDetailEnemies(stage, enemyCatalog);
    if (enemies.empty()) {
        renderer.drawText({content.pos.x, y}, "不明", ui::TextMuted, 2);
        y += 30.0f;
        return;
    }

    const int columns = std::max(1, static_cast<int>((content.size.x + DungeonDetailEnemyIconGap) / (DungeonDetailEnemyIconSize + DungeonDetailEnemyIconGap)));
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i) {
        const int column = i % columns;
        const int row = i / columns;
        const Vec2 slotPos{
            content.pos.x + static_cast<float>(column) * (DungeonDetailEnemyIconSize + DungeonDetailEnemyIconGap),
            y + static_cast<float>(row) * (DungeonDetailEnemyIconSize + DungeonDetailEnemyIconGap),
        };
        const Vec2 iconCenter = slotPos + Vec2{DungeonDetailEnemyIconSize * 0.5f, DungeonDetailEnemyIconSize * 0.5f};

        const EnemyDefinition& enemy = *enemies[static_cast<std::size_t>(i)];
        EnemyImageDrawOptions options;
        options.allowUpscale = true;
        options.directionOverrideEnabled = true;
        options.directionOverride = {0.0f, 1.0f};
        if (encyclopedia.enemyStage(enemy.id) == EncyclopediaStage::Undiscovered) {
            options.tint = {36, 38, 48, 255};
            options.maskOverlayColor = {0, 0, 0, 150};
        }
        if (enemy.imageNumber <= 0 ||
            !drawEnemyImageIcon(
                renderer,
                enemy.imageNumber,
                iconCenter,
                {DungeonDetailEnemyIconSize, DungeonDetailEnemyIconSize},
                animationSeconds,
                options)) {
            renderer.fillCircle(iconCenter, DungeonDetailEnemyIconSize * 0.32f, {34, 34, 44, 230});
        }
    }

    const int rows = (static_cast<int>(enemies.size()) + columns - 1) / columns;
    y += static_cast<float>(rows) * DungeonDetailEnemyIconSize + static_cast<float>(std::max(0, rows - 1)) * DungeonDetailEnemyIconGap + 4.0f;
}

void drawDungeonStartDetailPanel(
    Renderer& renderer,
    const ObjectCatalog& objectCatalog,
    const EnemyCatalog& enemyCatalog,
    const EncyclopediaSystem& encyclopedia,
    UiRect panel,
    const StageDefinition& stage,
    int discoveredWarpPoints,
    int totalWarpPoints,
    float animationSeconds)
{
    drawUiSubPanel(renderer, panel);
    float y = uiSubPanelContentRect(panel).pos.y;
    drawDungeonDetailImage(renderer, panel, y, stage);
    drawUiDetailText(renderer, panel, y, stageDetailDescription(stage));
    drawDungeonWarpProgressLine(renderer, objectCatalog, panel, y, discoveredWarpPoints, totalWarpPoints);
    drawUiDetailLine(renderer, panel, y, "難易度", stageDetailDifficulty(stage));
    drawUiDetailLine(renderer, panel, y, "広さ", stageDetailSize(stage));
    drawUiDetailLine(renderer, panel, y, "壁の固さ", stageDetailWallHardness(stage));
    drawUiDetailLine(renderer, panel, y, "地形", stageDetailTerrainComplexity(stage));
    y += 2.0f;
    drawDungeonDetailEnemyIcons(renderer, panel, y, stage, enemyCatalog, encyclopedia, animationSeconds);
}

bool isMerchantMiningCandidate(const ItemData& item, int merchantUpgradeLevel)
{
    if (item.id.empty() || item.price <= 0 || item.category != MiningToolCategory) {
        return false;
    }
    const int maxRarity = merchantUpgradeLevel >= 7 ? 10 :
        (merchantUpgradeLevel >= 5 ? 7 :
            (merchantUpgradeLevel >= 4 ? 5 :
            (merchantUpgradeLevel >= 2 ? 4 : 2)));
    return item.rarity <= maxRarity;
}

constexpr std::string_view BaseReturnCount1Flag = "base_return_count_1";
constexpr std::string_view BaseReturnCount2Flag = "base_return_count_2";
constexpr std::string_view BaseStorageTutorialFlag = "story_tutorial_base_storage";
constexpr std::string_view BaseMerchantTutorialFlag = "story_tutorial_base_merchant";
constexpr std::string_view BaseProcessingTutorialFlag = "story_tutorial_base_processing";
constexpr std::string_view BaseForgeTutorialFlag = "story_tutorial_base_forge";
constexpr std::string_view BaseBookshelfTutorialFlag = "story_tutorial_base_bookshelf";
constexpr std::string_view BaseDiaryTutorialFlag = "story_tutorial_base_diary";
constexpr int BaseHintMoneyThreshold = 5000;
constexpr int BaseHintMaterialThreshold = 10;
constexpr int BaseUpgradeRingWorkshopIndex = 3;
constexpr int BaseUpgradeRingPresetIndex = 8;
constexpr std::string_view BaseHintStorageFullTrigger = "base_hint:storage_full";
constexpr std::string_view BaseHintMerchantFullTrigger = "base_hint:merchant_full";
constexpr std::string_view BaseHintProcessingReadyTrigger = "base_hint:processing_ready";
constexpr std::string_view BaseHintForgeReadyTrigger = "base_hint:forge_ready";
constexpr std::string_view BaseHintRingWorkshopBuildableTrigger = "base_hint:ring_workshop_buildable";
constexpr std::string_view BaseHintRingPresetReadyTrigger = "base_hint:ring_preset_ready";
constexpr std::string_view BaseHintDiarySaveTrigger = "base_hint:diary_save";
constexpr std::string_view BaseHintBookshelfStage1Trigger = "base_hint:bookshelf_stage1";

void drawBaseControlHelp(Renderer& renderer, int screenWidth, int screenHeight, std::string help)
{
    if (help.empty()) {
        return;
    }

    InputHelpStyle helpStyle;
    helpStyle.text = {232, 232, 238, 235};
    helpStyle.outline = {0, 0, 0, 190};
    helpStyle.scale = 2;
    helpStyle.outlinePx = 4;
    helpStyle.iconHeight = 24.0f;
    helpStyle.outlineEnabled = true;

    const float screenW = static_cast<float>(screenWidth);
    const float screenH = static_cast<float>(screenHeight);
    const float maxWidth = std::max(120.0f, screenW - 32.0f);
    help = fittedInputHelpText(renderer, std::move(help), maxWidth, helpStyle);
    const Vec2 textSize = measureInputHelpText(renderer, help, helpStyle);
    const Vec2 pos{
        (screenW - textSize.x) * 0.5f,
        std::max(TopInfoBarY + TopInfoBarHeight + 8.0f, screenH - textSize.y - 4.0f),
    };
    drawInputHelpText(renderer, pos, help, helpStyle);
}

std::string baseExplorationControlHelp(const BaseFacility* facility)
{
    if (facility == nullptr) {
        return "WASD/方向キー 移動   Enter 近くの施設を調べる   Esc メニュー";
    }

    switch (facility->verb) {
    case BaseInteractionVerb::Inspect:
        return std::string("Enter ") + facility->displayName + "を調べる   Esc メニュー";
    case BaseInteractionVerb::Talk:
        return std::string("Enter ") + facility->displayName + "と話す   Esc メニュー";
    case BaseInteractionVerb::Enter:
        return std::string("Enter ") + facility->displayName + "に入る   Esc メニュー";
    case BaseInteractionVerb::Exit:
        return "Enter 屋外へ戻る   Esc メニュー";
    }
    return std::string("Enter ") + facility->displayName + "を調べる   Esc メニュー";
}

const char* baseFacilityTutorialTrigger(BaseFacilityAction action)
{
    switch (action) {
    case BaseFacilityAction::MineExit:
        return nullptr;
    case BaseFacilityAction::Storage:
        return "tutorial:base_storage";
    case BaseFacilityAction::Bookshelf:
        return "tutorial:base_bookshelf";
    case BaseFacilityAction::Merchant:
        return "tutorial:base_merchant";
    case BaseFacilityAction::Processing:
        return "tutorial:base_processing";
    case BaseFacilityAction::Forge:
        return "tutorial:base_forge";
    case BaseFacilityAction::Diary:
        return "tutorial:base_diary";
    case BaseFacilityAction::RingWorkshop:
        return "tutorial:base_ring_workshop";
    case BaseFacilityAction::HomeEntrance:
    case BaseFacilityAction::HomeExit:
    case BaseFacilityAction::MonicaTalk:
    case BaseFacilityAction::ElderTalk:
        return nullptr;
    }
    return nullptr;
}

int baseUpgradeWarehouseCapacityForLevel(int level)
{
    constexpr std::array<int, 5> Capacities{{48, 72, 100, 140, 200}};
    const int index = std::clamp(level, 0, static_cast<int>(Capacities.size()) - 1);
    return Capacities[static_cast<std::size_t>(index)];
}

template <std::size_t N>
int baseUpgradeCostForStep(int step, const std::array<int, N>& costs)
{
    if (step < 0 || step >= static_cast<int>(costs.size())) {
        return 0;
    }
    return costs[static_cast<std::size_t>(step)];
}

int merchantStockCountForLevel(int level)
{
    const int clampedLevel = std::clamp(level, 1, 7);
    return 6 + (clampedLevel - 1) * 3;
}

int baseUpgradeMerchantStockCountForUiLevel(int level)
{
    return 6 + std::clamp(level, 0, 6) * 3;
}

const char* baseUpgradeMerchantBuyPriceFeature(int level)
{
    if (level >= 5) {
        return "+20%";
    }
    if (level >= 2) {
        return "+10%";
    }
    return "通常";
}

const char* baseUpgradeMerchantTreasureFeature(int level)
{
    return level >= 3 ? "解禁" : "未解禁";
}

const char* baseUpgradeMerchantRareFeature(int level)
{
    if (level >= 6) {
        return "高レア増加";
    }
    if (level >= 4) {
        return "レア増加";
    }
    return "通常";
}

const char* baseUpgradeProcessingUnlockFeature(int level)
{
    if (level >= 3) {
        return "大型化";
    }
    if (level >= 1) {
        return "軽量化";
    }
    return "未解禁";
}

const char* baseUpgradeProcessingDiscountFeature(int level)
{
    if (level >= 5) {
        return "-30%";
    }
    if (level >= 4) {
        return "-20%";
    }
    if (level >= 2) {
        return "-10%";
    }
    return "通常";
}

const char* baseUpgradeRingPresetFeature(int level)
{
    switch (level) {
    case 1: return "1枠";
    case 2: return "2枠";
    case 3: return "3枠";
    default: return "未解禁";
    }
}

enum class MerchantStockGroup {
    Mining,
    Exploration,
    Recovery,
    WeaponShield,
    EnhanceDebuff,
};

constexpr std::array<MerchantStockGroup, 5> RequiredMerchantStockGroups{{
    MerchantStockGroup::Mining,
    MerchantStockGroup::Exploration,
    MerchantStockGroup::Recovery,
    MerchantStockGroup::WeaponShield,
    MerchantStockGroup::EnhanceDebuff,
}};

const char* merchantStockGroupName(MerchantStockGroup group)
{
    switch (group) {
    case MerchantStockGroup::Mining: return "mining";
    case MerchantStockGroup::Exploration: return "exploration";
    case MerchantStockGroup::Recovery: return "recovery";
    case MerchantStockGroup::WeaponShield: return "weapon_shield";
    case MerchantStockGroup::EnhanceDebuff: return "enhance_debuff";
    }
    return "unknown";
}

std::optional<MerchantStockGroup> merchantStockGroupForItem(const ItemData& item)
{
    if (item.category == "\xE6\x8E\x98\xE5\x89\x8A") {
        return MerchantStockGroup::Mining;
    }
    if (item.category == "\xE6\x8E\xA2\xE7\xB4\xA2") {
        return MerchantStockGroup::Exploration;
    }
    if (item.category == "\xE5\x9B\x9E\xE5\xBE\xA9") {
        return MerchantStockGroup::Recovery;
    }
    if (item.category == "\xE6\xAD\xA6\xE5\x99\xA8" || item.category == "\xE7\x9B\xBE") {
        return MerchantStockGroup::WeaponShield;
    }
    if (item.category == "\xE5\xBC\xB7\xE5\x8C\x96" || item.category == "\xE5\xBC\xB1\xE4\xBD\x93") {
        return MerchantStockGroup::EnhanceDebuff;
    }
    return std::nullopt;
}

double merchantCandidateWeight(const ItemData& item, int merchantUpgradeLevel)
{
    const double commonWeight = static_cast<double>(std::max(1, 12 - std::clamp(item.rarity, 1, 10)));
    const double rareWeight = merchantUpgradeLevel >= 7
        ? static_cast<double>(std::clamp(item.rarity, 1, 10)) * 1.4
        : (merchantUpgradeLevel >= 5 ? static_cast<double>(std::clamp(item.rarity, 1, 10)) * 0.65 : 0.0);
    return commonWeight + rareWeight;
}

const ItemData* pickMerchantCandidate(
    std::vector<const ItemData*>& pool,
    int merchantUpgradeLevel,
    std::mt19937& rng,
    std::optional<MerchantStockGroup> requiredGroup = std::nullopt)
{
    std::vector<std::size_t> indexes;
    indexes.reserve(pool.size());
    for (std::size_t i = 0; i < pool.size(); ++i) {
        if (requiredGroup.has_value()) {
            const std::optional<MerchantStockGroup> group = merchantStockGroupForItem(*pool[i]);
            if (!group.has_value() || *group != *requiredGroup) {
                continue;
            }
        }
        indexes.push_back(i);
    }
    if (indexes.empty()) {
        return nullptr;
    }

    std::vector<double> weights;
    weights.reserve(indexes.size());
    for (std::size_t index : indexes) {
        weights.push_back(merchantCandidateWeight(*pool[index], merchantUpgradeLevel));
    }
    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    const std::size_t pickedPoolIndex = indexes[static_cast<std::size_t>(distribution(rng))];
    const ItemData* item = pool[pickedPoolIndex];
    pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(pickedPoolIndex));
    return item;
}

std::unordered_map<std::string, int> buildObjectSortOrder(const ObjectCatalog& catalog)
{
    std::unordered_map<std::string, int> order;
    order.reserve(catalog.objects.size());
    for (int i = 0; i < static_cast<int>(catalog.objects.size()); ++i) {
        const ObjectDefinition& object = catalog.objects[static_cast<std::size_t>(i)];
        if (!object.id.empty() && order.find(object.id) == order.end()) {
            order.emplace(object.id, i);
        }
    }
    return order;
}

int objectSortOrder(const std::unordered_map<std::string, int>& order, const std::string& objectId)
{
    constexpr int MissingOrder = 1'000'000'000;
    const auto it = order.find(objectId);
    return it != order.end() ? it->second : MissingOrder;
}

const std::string& objectSortId(const InventoryObjectInstance& instance)
{
    return !instance.item.id.empty() ? instance.item.id : instance.instance.objectId;
}

const std::string& warehouseEntrySortId(
    int entryIndex,
    const std::vector<InventoryObjectStack>& stacks,
    const std::vector<InventoryObjectInstance>& instances)
{
    const int stackCount = static_cast<int>(stacks.size());
    if (entryIndex >= 0 && entryIndex < stackCount) {
        return stacks[static_cast<std::size_t>(entryIndex)].objectId;
    }
    const int instanceIndex = entryIndex - stackCount;
    if (instanceIndex >= 0 && instanceIndex < static_cast<int>(instances.size())) {
        return objectSortId(instances[static_cast<std::size_t>(instanceIndex)]);
    }
    static const std::string Empty;
    return Empty;
}

const char* baseUpgradeResultSubject(int index)
{
    switch (index) {
    case 0: return "収納箱容量";
    case 1: return "商人機能";
    case 2: return "作業台機能";
    case 3: return "リング工房";
    case 4: return "最大HP";
    case 5: return "リング半径";
    case 6: return "リング速度";
    case 7: return "吸引強化";
    case 8: return "リングプリセット解禁";
    default: return "強化項目";
    }
}

UiResultDialogLine baseUpgradeResultTextLine(std::string text)
{
    UiResultDialogLine line;
    line.segments.push_back({std::move(text), ui::Text});
    return line;
}

UiResultDialogLine baseUpgradeResultTextLine(std::string prefix, std::string text, Color textColor)
{
    UiResultDialogLine line;
    line.segments.push_back({std::move(prefix), ui::Text});
    line.segments.push_back({std::move(text), textColor});
    return line;
}

void appendBaseUpgradeResultChangeLine(
    std::vector<UiResultDialogLine>& lines,
    std::string prefix,
    std::string current,
    std::string next,
    bool showUnchanged = false)
{
    constexpr Color UpgradeValueColor{255, 230, 150, 255};
    const bool changed = current != next;
    if (!changed && !showUnchanged) {
        return;
    }

    UiResultDialogLine line;
    line.segments.push_back({std::move(prefix), ui::Text});
    line.segments.push_back({std::move(current), ui::Text});
    line.segments.push_back({" → ", ui::TextMuted});
    line.segments.push_back({std::move(next), changed ? UpgradeValueColor : ui::Text});
    lines.push_back(std::move(line));
}

std::vector<UiResultDialogLine> baseUpgradeResultLines(int index, int beforeLevel, int afterLevel)
{
    std::vector<UiResultDialogLine> lines;
    lines.push_back(baseUpgradeResultTextLine(std::string(baseUpgradeResultSubject(index)) + "を強化しました"));

    char currentValue[64];
    char nextValue[64];
    switch (index) {
    case 0:
        std::snprintf(currentValue, sizeof(currentValue), "%d枠", baseUpgradeWarehouseCapacityForLevel(beforeLevel));
        std::snprintf(nextValue, sizeof(nextValue), "%d枠", baseUpgradeWarehouseCapacityForLevel(afterLevel));
        appendBaseUpgradeResultChangeLine(lines, "収納箱容量: ", currentValue, nextValue);
        break;
    case 1:
        std::snprintf(currentValue, sizeof(currentValue), "%d枠", baseUpgradeMerchantStockCountForUiLevel(beforeLevel));
        std::snprintf(nextValue, sizeof(nextValue), "%d枠", baseUpgradeMerchantStockCountForUiLevel(afterLevel));
        appendBaseUpgradeResultChangeLine(lines, "品揃え: ", currentValue, nextValue);
        appendBaseUpgradeResultChangeLine(
            lines,
            "買取価格: ",
            baseUpgradeMerchantBuyPriceFeature(beforeLevel),
            baseUpgradeMerchantBuyPriceFeature(afterLevel),
            std::string_view(baseUpgradeMerchantBuyPriceFeature(beforeLevel)) != "通常");
        appendBaseUpgradeResultChangeLine(
            lines,
            "宝の高価買取: ",
            baseUpgradeMerchantTreasureFeature(beforeLevel),
            baseUpgradeMerchantTreasureFeature(afterLevel),
            std::string_view(baseUpgradeMerchantTreasureFeature(beforeLevel)) != "未解禁");
        appendBaseUpgradeResultChangeLine(
            lines,
            "レア商品: ",
            baseUpgradeMerchantRareFeature(beforeLevel),
            baseUpgradeMerchantRareFeature(afterLevel),
            std::string_view(baseUpgradeMerchantRareFeature(beforeLevel)) != "通常");
        break;
    case 2:
        appendBaseUpgradeResultChangeLine(
            lines,
            "加工機能: ",
            baseUpgradeProcessingUnlockFeature(beforeLevel),
            baseUpgradeProcessingUnlockFeature(afterLevel),
            std::string_view(baseUpgradeProcessingUnlockFeature(beforeLevel)) != "未解禁");
        appendBaseUpgradeResultChangeLine(
            lines,
            "作業台費用: ",
            baseUpgradeProcessingDiscountFeature(beforeLevel),
            baseUpgradeProcessingDiscountFeature(afterLevel),
            std::string_view(baseUpgradeProcessingDiscountFeature(beforeLevel)) != "通常");
        break;
    case 3:
        appendBaseUpgradeResultChangeLine(lines, "リング工房: ", "未解禁", "解禁");
        break;
    case 4:
        std::snprintf(currentValue, sizeof(currentValue), "+%d", beforeLevel * 2);
        std::snprintf(nextValue, sizeof(nextValue), "+%d", afterLevel * 2);
        appendBaseUpgradeResultChangeLine(lines, "最大HP: ", currentValue, nextValue);
        break;
    case 5:
        std::snprintf(
            currentValue,
            sizeof(currentValue),
            "%.2fm",
            static_cast<float>(balance::SpellRingRadius) *
                (1.0f + static_cast<float>(beforeLevel) * 0.08f) /
                static_cast<float>(balance::TileSize));
        std::snprintf(
            nextValue,
            sizeof(nextValue),
            "%.2fm",
            static_cast<float>(balance::SpellRingRadius) *
                (1.0f + static_cast<float>(afterLevel) * 0.08f) /
                static_cast<float>(balance::TileSize));
        appendBaseUpgradeResultChangeLine(lines, "リング半径: ", currentValue, nextValue);
        break;
    case 6:
        std::snprintf(
            currentValue,
            sizeof(currentValue),
            "%.2fm/s",
            balance::SpellRingSpeed * (1.0f + static_cast<float>(beforeLevel) * 0.08f));
        std::snprintf(
            nextValue,
            sizeof(nextValue),
            "%.2fm/s",
            balance::SpellRingSpeed * (1.0f + static_cast<float>(afterLevel) * 0.08f));
        appendBaseUpgradeResultChangeLine(lines, "リング速度: ", currentValue, nextValue);
        break;
    case 7:
        std::snprintf(currentValue, sizeof(currentValue), "Lv.%d", beforeLevel);
        std::snprintf(nextValue, sizeof(nextValue), "Lv.%d", afterLevel);
        appendBaseUpgradeResultChangeLine(lines, "吸引強化: ", currentValue, nextValue);
        lines.push_back(baseUpgradeResultTextLine("対象: ", "近くのドロップ", ui::TextMuted));
        break;
    case 8:
        appendBaseUpgradeResultChangeLine(
            lines,
            "プリセット枠: ",
            baseUpgradeRingPresetFeature(beforeLevel),
            baseUpgradeRingPresetFeature(afterLevel));
        lines.push_back(baseUpgradeResultTextLine("用途: ", "リング編成登録/呼び出し", ui::TextMuted));
        break;
    default:
        break;
    }
    return lines;
}

constexpr int RingLevelUpgradeKindCount = 3;
constexpr int RingWorkshopRespecBaseMoneyCost = 120;
constexpr int RingWorkshopRespecMoneyCostPerTotalPoint = 20;
constexpr int RingWorkshopRespecMoneyCostPerMovedPoint = 80;
constexpr int RingWorkshopRespecBaseMoonCost = 1;
constexpr int RingWorkshopRespecMovedPointsPerExtraMoon = 4;
constexpr int BaseBackpackSourceIndex = 0;

bool sameRingLevelUpgradeSelection(RingLevelUpgradeSelection left, RingLevelUpgradeSelection right)
{
    return left.ringIndex == right.ringIndex && left.kind == right.kind;
}

Vec2 beginBaseDetailRow(Renderer& renderer, UiRect detailPanel, float y, std::string_view label)
{
    constexpr float LabelWidth = 96.0f;
    const UiRect content = uiSubPanelContentRect(detailPanel);
    renderer.drawText({content.pos.x, y}, label, ui::TextMuted, 2);
    return {content.pos.x + LabelWidth, y};
}

void drawBaseDetailTextRun(Renderer& renderer, Vec2& pos, std::string_view text, Color color, int scale)
{
    renderer.drawText(pos, text, color, scale);
    pos.x += renderer.measureText(text, scale).x;
}

void drawBaseDetailInlineItemTextRun(
    Renderer& renderer,
    const ObjectCatalog& objectCatalog,
    Vec2& pos,
    std::string_view text,
    Color color)
{
    InlineItemTextStyle inlineStyle;
    inlineStyle.scale = 2;
    inlineStyle.iconTextGap = 4.0f;
    inlineStyle.iconScale = 1.15f;
    inlineStyle.text = color;
    drawInlineItemText(renderer, objectCatalog, pos, text, inlineStyle);
    pos.x += measureInlineItemText(renderer, text, inlineStyle).x;
}

void drawBaseDetailMoneyCostLine(
    Renderer& renderer,
    const ObjectCatalog& objectCatalog,
    UiRect detailPanel,
    float& y,
    std::string_view label,
    int cost,
    int ownedMoney)
{
    Vec2 pos = beginBaseDetailRow(renderer, detailPanel, y, label);
    const Color numberColor = ownedMoney >= cost ? ui::Text : Color{238, 82, 82, 255};
    drawBaseDetailInlineItemTextRun(renderer, objectCatalog, pos, inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + " ", ui::Text);
    drawBaseDetailTextRun(renderer, pos, std::to_string(cost), numberColor, 2);
    drawBaseDetailTextRun(renderer, pos, "G", ui::Text, 2);
    y += 31.0f;
}

void drawBaseDetailMaterialCostLine(
    Renderer& renderer,
    const ObjectCatalog& objectCatalog,
    UiRect detailPanel,
    float& y,
    std::string_view label,
    MaterialType type,
    int cost,
    int owned)
{
    const Color numberColor = owned >= cost ? ui::Text : Color{238, 82, 82, 255};
    Vec2 pos = beginBaseDetailRow(renderer, detailPanel, y, label);
    drawBaseDetailInlineItemTextRun(renderer, objectCatalog, pos, inlineMaterialIconTag(type) + std::string(materialTypeDisplayName(type)) + " ×", ui::Text);
    drawBaseDetailTextRun(renderer, pos, std::to_string(cost), numberColor, 2);
    drawBaseDetailTextRun(renderer, pos, "（", ui::TextMuted, 2);
    drawBaseDetailTextRun(renderer, pos, std::to_string(owned), numberColor, 2);
    drawBaseDetailTextRun(renderer, pos, "）", ui::TextMuted, 2);
    y += 31.0f;
}

int ringWorkshopRespecMovedPointCount(
    const RingLevelUpgradePointTable& current,
    const RingLevelUpgradePointTable& draft)
{
    const auto absoluteDifference = [](int left, int right) {
        return left >= right ? left - right : right - left;
    };

    int changedPointSides = 0;
    for (std::size_t i = 0; i < current.size(); ++i) {
        const RingLevelUpgradePoints before = clampedRingLevelUpgradePoints(current[i]);
        const RingLevelUpgradePoints after = clampedRingLevelUpgradePoints(draft[i]);
        changedPointSides += absoluteDifference(before.radius, after.radius);
        changedPointSides += absoluteDifference(before.speed, after.speed);
        changedPointSides += absoluteDifference(before.weightLimit, after.weightLimit);
    }
    return changedPointSides / 2;
}

constexpr std::array<int, 4> RoguelikeTrainerUpgradeIndices{{4, 5, 6, 7}};

int baseUpgradeDisplayCount(bool roguelikeTrainer)
{
    return roguelikeTrainer
        ? static_cast<int>(RoguelikeTrainerUpgradeIndices.size())
        : BaseUpgradeItemCount;
}

int baseUpgradeIndexForDisplay(bool roguelikeTrainer, int displayIndex)
{
    if (!roguelikeTrainer) {
        return std::clamp(displayIndex, 0, BaseUpgradeItemCount - 1);
    }
    const int clamped = std::clamp(displayIndex, 0, static_cast<int>(RoguelikeTrainerUpgradeIndices.size()) - 1);
    return RoguelikeTrainerUpgradeIndices[static_cast<std::size_t>(clamped)];
}

int baseUpgradeDisplayForIndex(bool roguelikeTrainer, int upgradeIndex)
{
    if (!roguelikeTrainer) {
        return std::clamp(upgradeIndex, 0, BaseUpgradeItemCount - 1);
    }
    for (int i = 0; i < static_cast<int>(RoguelikeTrainerUpgradeIndices.size()); ++i) {
        if (RoguelikeTrainerUpgradeIndices[static_cast<std::size_t>(i)] == upgradeIndex) {
            return i;
        }
    }
    return 0;
}

constexpr std::array<std::string_view, BaseItemSourceCount> BaseItemSourceLabels{{
    "リュック",
    "収納箱",
    "リング1",
    "リング2",
    "リング3",
}};

int baseItemSourceIconImageNumber(int source)
{
    if (source == BaseBackpackSourceIndex) {
        return menuIconImageNumber(MenuIconImage::Backpack);
    }
    if (source == BaseWarehouseSourceIndex) {
        return menuIconImageNumber(MenuIconImage::StorageChest);
    }
    if (source >= BaseRingSourceOffset && source < BaseItemSourceCount) {
        return ringDisplayIconImageNumber(source - BaseRingSourceOffset);
    }
    return 0;
}

UiTabItem baseItemSourceTabItem(int source, bool enabled)
{
    const int clampedSource = std::clamp(source, 0, BaseItemSourceCount - 1);
    return {
        BaseItemSourceLabels[static_cast<std::size_t>(clampedSource)],
        enabled,
        baseItemSourceIconImageNumber(clampedSource),
    };
}

constexpr int StorageDepositSourceCount = 1 + SpellRingCount;
constexpr float MerchantSellSourceYOffset = 44.0f;
constexpr float MerchantSellItemYOffset = MerchantSellSourceYOffset + 16.0f;
constexpr float MerchantSellRingYOffset = MerchantSellSourceYOffset + 40.0f + 40.0f;
constexpr float StorageTransferLayoutYOffset = 24.0f;
constexpr int StorageWithdrawRows = 3;
constexpr int StorageWithdrawSlotCount = StorageColumns * StorageWithdrawRows;
constexpr float StorageWithdrawGridY = 190.0f;
constexpr float StorageWithdrawRowGap = 8.0f;
constexpr float StorageWithdrawSortButtonGap = 22.0f;
constexpr float BaseRingPreviewScale = 0.9f;
constexpr float BaseProcessingRingYOffset = 64.0f;
constexpr float MerchantSellRingPreviewScale = 0.9f;
constexpr float StorageRingPreviewScale = 1.0f;
constexpr float MerchantSellRingItemLabelExtraHeight = 30.0f;
constexpr float ExternalWarehouseGridYOffset = 44.0f;
constexpr float ExternalWarehousePageSelectorGap = 10.0f;
constexpr float BaseFacilitySpawnGap = 18.0f;
constexpr float BaseMineExitReturnUpOffset = 40.0f;

enum class BaseFacilitySpawnSide {
    Above,
    Below,
};

UiRect defaultBaseFacilityRect(BaseArea area, bool ringWorkshopUnlocked, std::string_view facilityId)
{
    const std::vector<BaseFacility> facilities = baseFacilities(area, ringWorkshopUnlocked);
    const auto it = std::find_if(facilities.begin(), facilities.end(), [facilityId](const BaseFacility& facility) {
        return facility.facilityId == facilityId;
    });
    return it == facilities.end() ? UiRect{{0.0f, 0.0f}, {0.0f, 0.0f}} : it->rect;
}

UiRect outdoorHomeDoorSpawnRect(UiRect homeRect, bool ringWorkshopUnlocked)
{
    constexpr UiRect DefaultDoor{{265.0f, 222.0f}, {60.0f, 44.0f}};
    const UiRect defaultHome = defaultBaseFacilityRect(BaseArea::Outdoor, ringWorkshopUnlocked, "home");
    if (defaultHome.size.x <= 0.0f || defaultHome.size.y <= 0.0f) {
        return DefaultDoor;
    }

    const float scaleX = homeRect.size.x / defaultHome.size.x;
    const float scaleY = homeRect.size.y / defaultHome.size.y;
    const float scale = std::isfinite(scaleX) && std::isfinite(scaleY)
        ? std::max(0.001f, std::min(scaleX, scaleY))
        : 1.0f;
    return {
        DefaultDoor.pos + (homeRect.pos - defaultHome.pos),
        DefaultDoor.size * scale,
    };
}

struct BaseFacilityVisual {
    const char* facilityId = "";
    const char* imagePath = "";
    UiRect rect{};
};

constexpr std::array<BaseFacilityVisual, 7> OutdoorBaseFacilityVisuals{{
    {"mine_exit", "assets/kyoten/move.png", {{578.0f, 530.0f}, {148.0f, 190.0f}}},
    {"storage_chest", "assets/kyoten/box.png", {{593.0f, 445.0f}, {60.0f, 53.0f}}},
    {"merchant_wagon", "assets/kyoten/wagon.png", {{928.0f, 65.0f}, {206.0f, 185.0f}}},
    {"processing_table", "assets/kyoten/sagyodai.png", {{512.0f, 154.0f}, {183.0f, 106.0f}}},
    {"upgrade_forge", "assets/kyoten/kyokaro.png", {{968.0f, 355.0f}, {217.0f, 226.0f}}},
    {"ring_workshop", "assets/kyoten/ring-kobo.png", {{842.0f, 470.0f}, {115.0f, 93.0f}}},
    {"home", "assets/kyoten/house.png", {{113.0f, 11.0f}, {301.0f, 308.0f}}},
}};

struct BaseCharacterSpriteVisual {
    const char* facilityId = "";
    const char* visualId = "";
};

constexpr std::array<BaseCharacterSpriteVisual, 4> OutdoorBaseCharacterSprites{{
    {"merchant_npc", "base_merchant"},
    {"processor_npc", "base_processor"},
    {"monica", "base_monica"},
    {"elder", "base_elder"},
}};

constexpr std::array<BaseFacilityVisual, 3> HomeInteriorBaseFacilityVisuals{{
    {"bookshelf", "assets/kyoten/books.png", {{368.0f, 322.0f}, {127.0f, 213.0f}}},
    {"diary", "assets/kyoten/desk.png", {{760.0f, 416.0f}, {179.0f, 142.0f}}},
    {"bed", "assets/kyoten/bed.png", {{680.0f, 188.0f}, {178.0f, 195.0f}}},
}};

const BaseFacilityVisual* findBaseFacilityVisual(
    std::span<const BaseFacilityVisual> visuals,
    std::string_view facilityId)
{
    const auto it = std::find_if(
        visuals.begin(),
        visuals.end(),
        [facilityId](const BaseFacilityVisual& visual) {
            return std::string_view(visual.facilityId) == facilityId;
        });
    return it == visuals.end() ? nullptr : &*it;
}

const BaseFacilityVisual* baseFacilityVisual(BaseArea area, std::string_view facilityId)
{
    switch (area) {
    case BaseArea::Outdoor:
        return findBaseFacilityVisual(OutdoorBaseFacilityVisuals, facilityId);
    case BaseArea::HomeInterior:
        return findBaseFacilityVisual(HomeInteriorBaseFacilityVisuals, facilityId);
    }
    return nullptr;
}

const BaseCharacterSpriteVisual* baseCharacterSpriteVisual(BaseArea area, std::string_view facilityId)
{
    if (area != BaseArea::Outdoor) {
        return nullptr;
    }

    const auto it = std::find_if(
        OutdoorBaseCharacterSprites.begin(),
        OutdoorBaseCharacterSprites.end(),
        [facilityId](const BaseCharacterSpriteVisual& visual) {
            return std::string_view(visual.facilityId) == facilityId;
        });
    return it == OutdoorBaseCharacterSprites.end() ? nullptr : &*it;
}

UiRect baseFacilityVisualRect(
    const BaseFacility& facility,
    BaseArea area,
    bool ringWorkshopUnlocked,
    const BaseFacilityVisual& visual)
{
    const UiRect defaultRect = defaultBaseFacilityRect(area, ringWorkshopUnlocked, facility.facilityId);
    if (defaultRect.size.x <= 0.0f || defaultRect.size.y <= 0.0f) {
        return visual.rect;
    }

    const float scaleX = facility.rect.size.x / defaultRect.size.x;
    const float scaleY = facility.rect.size.y / defaultRect.size.y;
    const float visualScale = std::isfinite(scaleX) && std::isfinite(scaleY)
        ? std::max(0.001f, std::min(scaleX, scaleY))
        : 1.0f;
    return {
        visual.rect.pos + (facility.rect.pos - defaultRect.pos),
        visual.rect.size * visualScale,
    };
}

UiRect baseCharacterSpriteVisualRect(const BaseFacility& facility)
{
    return facility.rect;
}

UiRect baseFacilityPointerRect(const BaseFacility& facility, BaseArea area, bool ringWorkshopUnlocked)
{
    if (const BaseFacilityVisual* visual = baseFacilityVisual(area, facility.facilityId)) {
        return baseFacilityVisualRect(facility, area, ringWorkshopUnlocked, *visual);
    }
    if (baseCharacterSpriteVisual(area, facility.facilityId) != nullptr) {
        return baseCharacterSpriteVisualRect(facility);
    }
    return facility.rect;
}

bool baseFacilityVisualHitTest(
    Renderer& renderer,
    const BaseFacilityVisual& visual,
    UiRect rect,
    Vec2 point)
{
    ImageDrawOptions options;
    const ImageHandle handle = renderer.acquireImage(visual.imagePath, TextureFilter::Nearest);
    if (!handle.valid()) {
        return rect.contains(point);
    }
    return renderer.imageHitTestAlpha(
        handle,
        rect.pos + rect.size * 0.5f,
        rect.size,
        point,
        options,
        12);
}

Vec2 currentRenderMousePosition(const Renderer& renderer)
{
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    return renderer.windowToRenderCoordinates({mouseX, mouseY});
}

Vec2 closestPointOnRect(Vec2 point, UiRect rect)
{
    return {
        clamp(point.x, rect.pos.x, rect.pos.x + rect.size.x),
        clamp(point.y, rect.pos.y, rect.pos.y + rect.size.y),
    };
}

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

const BaseFacility* findBaseFacilityById(
    const std::vector<BaseFacility>& facilities,
    std::string_view facilityId)
{
    const auto it = std::find_if(
        facilities.begin(),
        facilities.end(),
        [facilityId](const BaseFacility& facility) {
            return std::string_view(facility.facilityId) == facilityId;
        });
    return it == facilities.end() ? nullptr : &*it;
}

bool pointEnteredRectFromBelow(Vec2 previousPoint, Vec2 currentPoint, UiRect rect)
{
    return rect.contains(currentPoint) &&
        previousPoint.y >= rect.pos.y + rect.size.y &&
        currentPoint.y < previousPoint.y;
}

bool pointEnteredRectFromAbove(Vec2 previousPoint, Vec2 currentPoint, UiRect rect)
{
    return rect.contains(currentPoint) &&
        previousPoint.y <= rect.pos.y &&
        currentPoint.y > previousPoint.y;
}

bool baseFacilityHiddenInNormalView(BaseArea area, const BaseFacility& facility)
{
    const std::string_view facilityId = facility.facilityId;
    if (area == BaseArea::Outdoor && facilityId == "home_entrance") {
        return true;
    }
    if (area == BaseArea::HomeInterior && facilityId == "home_exit") {
        return true;
    }
    return !facility.unlocked && facilityId == "ring_workshop";
}

std::string_view baseFacilityInteractionGroupId(const BaseFacility& facility)
{
    const std::string_view groupId = facility.interactionGroupId;
    return groupId.empty() ? std::string_view(facility.facilityId) : groupId;
}

bool baseInteractionGroupAvailable(
    Vec2 playerPosition,
    BaseArea area,
    const std::vector<BaseFacility>& facilities,
    const BaseFacility& facility)
{
    if (!facility.enabled) {
        return false;
    }

    const std::string_view groupId = baseFacilityInteractionGroupId(facility);
    for (const BaseFacility& candidate : facilities) {
        if (!candidate.enabled) {
            continue;
        }
        if (baseFacilityHiddenInNormalView(area, candidate)) {
            continue;
        }
        if (baseFacilityInteractionGroupId(candidate) != groupId) {
            continue;
        }
        if (baseInteractionAvailable(playerPosition, candidate)) {
            return true;
        }
    }
    return false;
}

void drawBaseFacilityNameLabel(
    Renderer& renderer,
    const BaseFacility& facility,
    UiRect labelRect,
    BaseArea area,
    bool ringWorkshopUnlocked)
{
    constexpr int LabelScale = 2;
    constexpr int LabelOutlinePx = 6;
    constexpr float TopPadding = 4.0f;
    constexpr float LabelLift = 16.0f;
    constexpr float HomeDoorLabelGap = 6.0f;
    const Vec2 textSize = renderer.measureText(facility.displayName, LabelScale);
    Vec2 pos{
        labelRect.pos.x + (labelRect.size.x - textSize.x) * 0.5f,
        labelRect.pos.y + TopPadding,
    };

    const std::string_view facilityId = facility.facilityId;
    if (area == BaseArea::Outdoor && facilityId == "home") {
        const UiRect doorRect = outdoorHomeDoorSpawnRect(facility.rect, ringWorkshopUnlocked);
        pos.x = doorRect.pos.x + (doorRect.size.x - textSize.x) * 0.5f;
        pos.y = doorRect.pos.y - textSize.y - HomeDoorLabelGap;
    } else if (area == BaseArea::Outdoor &&
        (facilityId == "storage_chest" || facilityId == "ring_workshop")) {
        pos.y -= LabelLift;
    }

    renderer.drawOutlinedText(
        pos,
        facility.displayName,
        {255, 255, 255, 255},
        {0, 0, 0, 170},
        LabelOutlinePx,
        LabelScale);
}

const BaseFacility* selectBaseInteractionFacility(
    Vec2 playerPosition,
    Vec2 playerFacing,
    BaseArea area,
    const std::vector<BaseFacility>& facilities)
{
    constexpr float DirectionalCandidateDot = 0.45f;
    constexpr float DirectionalTieEpsilon = 0.001f;

    const BaseFacility* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    const BaseFacility* directional = nullptr;
    float directionalDot = DirectionalCandidateDot;
    float directionalDistance = std::numeric_limits<float>::max();
    const bool hasFacing = lengthSquared(playerFacing) > 0.0001f;
    const Vec2 facing = hasFacing ? normalize(playerFacing) : Vec2{};

    for (const BaseFacility& facility : facilities) {
        if (baseFacilityHiddenInNormalView(area, facility)) {
            continue;
        }
        if (!baseInteractionGroupAvailable(playerPosition, area, facilities, facility)) {
            continue;
        }

        const float dist = distanceToRect(playerPosition, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }

        if (!hasFacing) {
            continue;
        }

        Vec2 target = closestPointOnRect(playerPosition, facility.rect);
        Vec2 toFacility = target - playerPosition;
        if (lengthSquared(toFacility) <= 0.0001f) {
            target = facility.rect.pos + facility.rect.size * 0.5f;
            toFacility = target - playerPosition;
        }
        if (lengthSquared(toFacility) <= 0.0001f) {
            continue;
        }

        const float candidateDot = dot(facing, normalize(toFacility));
        if (candidateDot < DirectionalCandidateDot) {
            continue;
        }

        if (candidateDot > directionalDot + DirectionalTieEpsilon ||
            (std::abs(candidateDot - directionalDot) <= DirectionalTieEpsilon && dist < directionalDistance)) {
            directionalDot = candidateDot;
            directionalDistance = dist;
            directional = &facility;
        }
    }

    return directional != nullptr ? directional : nearest;
}

void drawBaseFacilityFallbackRect(
    Renderer& renderer,
    const BaseFacility& facility,
    bool inInteractionRange,
    bool hovered,
    bool showInteractionHints)
{
    Color fill = facility.enabled ? Color{96, 82, 82, 255} : Color{84, 62, 56, 255};
    if (!facility.unlocked) {
        fill = {58, 58, 64, 255};
    }
    Color outline = facility.enabled ? Color{220, 200, 150, 255} : Color{120, 108, 98, 255};
    if (showInteractionHints && inInteractionRange && facility.enabled) {
        outline = hovered ? Color{255, 230, 72, 255} : Color{255, 255, 255, 245};
        fill.a = std::max<unsigned char>(fill.a, 170);
    }
    renderer.fillRect(facility.rect.pos, facility.rect.size, fill);
    renderer.drawRect(facility.rect.pos, facility.rect.size, outline);
    if (showInteractionHints && (!inInteractionRange || !facility.enabled)) {
        if (facility.showNameLabel) {
            renderer.drawText(
                facility.rect.pos + Vec2{8.0f, 8.0f},
                facility.displayName,
                facility.enabled ? Color{248, 238, 214, 255} : Color{154, 146, 138, 255},
                2);
        }
    }
}

void drawBaseFacilities(
    Renderer& renderer,
    const std::vector<BaseFacility>& facilities,
    BaseArea area,
    bool ringWorkshopUnlocked,
    Vec2 playerPosition,
    Vec2 mouse,
    bool showInteractionHints)
{
    struct FacilityNameLabel {
        const BaseFacility* facility = nullptr;
        UiRect rect{};
    };
    std::vector<FacilityNameLabel> labels;

    for (int pass = 0; pass < 2; ++pass) {
        const bool drawEnabled = pass == 1;
        for (const BaseFacility& facility : facilities) {
            if (baseFacilityHiddenInNormalView(area, facility)) {
                continue;
            }
            if (facility.enabled != drawEnabled) {
                continue;
            }

            const bool inInteractionRange = showInteractionHints &&
                baseInteractionGroupAvailable(playerPosition, area, facilities, facility);
            const bool labelVisible = showInteractionHints && inInteractionRange && facility.enabled && facility.showNameLabel;
            const BaseFacilityVisual* visual = baseFacilityVisual(area, facility.facilityId);

            if (visual == nullptr) {
                if (baseCharacterSpriteVisual(area, facility.facilityId) != nullptr) {
                    continue;
                }
                const bool hovered = showInteractionHints && inInteractionRange && facility.enabled && facility.rect.contains(mouse);
                drawBaseFacilityFallbackRect(renderer, facility, inInteractionRange, hovered, showInteractionHints);
                if (labelVisible) {
                    labels.push_back({&facility, facility.rect});
                }
                continue;
            }

            const UiRect visualRect = baseFacilityVisualRect(facility, area, ringWorkshopUnlocked, *visual);
            const bool hovered = showInteractionHints &&
                inInteractionRange &&
                facility.enabled &&
                baseFacilityVisualHitTest(renderer, *visual, visualRect, mouse);

            ImageDrawOptions options;
            options.outlineEnabled = showInteractionHints && inInteractionRange && facility.enabled;
            options.outlineColor = hovered ? Color{255, 230, 72, 255} : Color{255, 255, 255, 245};
            options.outlinePx = 1;
            if (!facility.unlocked) {
                options.tint = {190, 190, 198, 230};
            }
            if (!renderer.drawImage(
                    visual->imagePath,
                    visualRect.pos + visualRect.size * 0.5f,
                    visualRect.size,
                    options,
                    TextureFilter::Nearest)) {
                drawBaseFacilityFallbackRect(renderer, facility, inInteractionRange, hovered, showInteractionHints);
            }
            if (labelVisible) {
                labels.push_back({&facility, visualRect});
            }
        }
    }

    for (const FacilityNameLabel& label : labels) {
        if (label.facility != nullptr) {
            drawBaseFacilityNameLabel(renderer, *label.facility, label.rect, area, ringWorkshopUnlocked);
        }
    }
}

Vec2 baseFacilitySpawnPosition(UiRect facilityRect, BaseFacilitySpawnSide side, float playerRadius)
{
    Vec2 position{facilityRect.pos.x + facilityRect.size.x * 0.5f, facilityRect.pos.y};
    if (side == BaseFacilitySpawnSide::Above) {
        position.y = facilityRect.pos.y - playerRadius - BaseFacilitySpawnGap;
    } else {
        position.y = facilityRect.pos.y + facilityRect.size.y + playerRadius + BaseFacilitySpawnGap;
    }

    const UiRect bounds = baseMapBounds();
    position.x = std::clamp(
        position.x,
        bounds.pos.x + playerRadius,
        bounds.pos.x + bounds.size.x - playerRadius);
    position.y = std::clamp(
        position.y,
        bounds.pos.y + playerRadius,
        bounds.pos.y + bounds.size.y - playerRadius);
    return position;
}

Vec2 homeInteriorEntryPosition(UiRect homeExitRect, float playerRadius)
{
    Vec2 position = baseFacilitySpawnPosition(homeExitRect, BaseFacilitySpawnSide::Above, playerRadius);
    position.y -= static_cast<float>(balance::TileSize);
    return position;
}

Vec2 baseHomeScreenDefaultPosition(float playerRadius)
{
    constexpr Vec2 DefaultPosition{640.0f, 360.0f};
    const UiRect bounds = baseMapBounds();
    return {
        std::clamp(DefaultPosition.x, bounds.pos.x + playerRadius, bounds.pos.x + bounds.size.x - playerRadius),
        std::clamp(DefaultPosition.y, bounds.pos.y + playerRadius, bounds.pos.y + bounds.size.y - playerRadius),
    };
}

UiRect merchantSellSourceRect(int index, int tabCount = BaseItemSourceCount)
{
    return baseItemSourceTabRect(index, 116.0f + MerchantSellSourceYOffset, tabCount);
}

float storageItemCircleLeftX()
{
    const UiRect first = merchantGridSlotRect(0);
    const float radius = std::min(first.size.x, first.size.y) * 0.5f;
    return first.pos.x + first.size.x * 0.5f - radius;
}

bool baseItemSourceIsWarehouse(int source)
{
    return source == BaseWarehouseSourceIndex;
}

bool baseItemSourceIsRing(int source)
{
    return source >= BaseRingSourceOffset && source < BaseItemSourceCount;
}

int ringIndexFromBaseItemSource(int source)
{
    return source - BaseRingSourceOffset;
}

int baseItemSourceCountForUnlockedRings(int unlockedRingCount)
{
    return BaseRingSourceOffset + std::clamp(unlockedRingCount, 1, SpellRingCount);
}

int storageDepositSourceCountForUnlockedRings(int unlockedRingCount)
{
    return 1 + std::clamp(unlockedRingCount, 1, SpellRingCount);
}

int clampBaseItemSourceForUnlockedRings(int source, int unlockedRingCount)
{
    const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount);
    return source >= 0 && source < sourceCount ? source : BaseBackpackSourceIndex;
}

int clampStorageDepositSourceForUnlockedRings(int source, int unlockedRingCount)
{
    if (source == BaseBackpackSourceIndex) {
        return source;
    }
    if (!baseItemSourceIsRing(source)) {
        return BaseBackpackSourceIndex;
    }
    const int ringIndex = ringIndexFromBaseItemSource(source);
    return ringIndex >= 0 && ringIndex < std::clamp(unlockedRingCount, 1, SpellRingCount)
        ? source
        : BaseBackpackSourceIndex;
}

int storageDepositSourceValue(int tabIndex)
{
    if (tabIndex <= 0) {
        return BaseBackpackSourceIndex;
    }
    return BaseRingSourceOffset + std::clamp(tabIndex - 1, 0, SpellRingCount - 1);
}

int storageDepositSourceTabIndex(int source)
{
    if (source == BaseBackpackSourceIndex) {
        return 0;
    }
    if (baseItemSourceIsRing(source)) {
        return 1 + ringIndexFromBaseItemSource(source);
    }
    return 0;
}

UiRect storageDepositSourceRect(int tabIndex)
{
    constexpr float StorageDepositSourceTabWidth = 180.0f;
    constexpr float StorageDepositSourceTabPitch = 194.0f;
    UiRect rect = merchantSellSourceRect(tabIndex);
    rect.pos.x = storageItemCircleLeftX() + static_cast<float>(tabIndex) * StorageDepositSourceTabPitch;
    rect.pos.y += StorageTransferLayoutYOffset;
    rect.size.x = StorageDepositSourceTabWidth;
    return rect;
}

UiRect storageTransferGridSlotRect(int index)
{
    UiRect rect = merchantGridSlotRect(index);
    rect.pos.y += MerchantSellItemYOffset + StorageTransferLayoutYOffset;
    return rect;
}

Vec2 storageTransferCountTextPos()
{
    return {storageItemCircleLeftX(), 116.0f + StorageTransferLayoutYOffset};
}

UiRect storageQuantityDialogRect()
{
    return {{430.0f, 130.0f}, {420.0f, 396.0f}};
}

UiRect storageTransferSortButtonRect()
{
    UiRect rect = uiBottomLeftButtonRect(merchantPanelRect(), {180.0f, ui::ButtonHeight});
    rect.pos.x = storageItemCircleLeftX();
    return rect;
}

UiRect storageWithdrawSlotRect(int index)
{
    UiRect rect = merchantGridSlotRect(index);
    const int row = index / StorageColumns;
    rect.pos.y = StorageWithdrawGridY + static_cast<float>(row) * (rect.size.y + StorageWithdrawRowGap);
    return rect;
}

Vec2 storageWithdrawCountTextPos()
{
    return storageTransferCountTextPos();
}

UiRect storageWithdrawSortButtonRect()
{
    UiRect rect = storageTransferSortButtonRect();
    const UiRect lastSlot = storageWithdrawSlotRect(StorageWithdrawSlotCount - 1);
    rect.pos.y = lastSlot.pos.y + lastSlot.size.y + StorageWithdrawSortButtonGap;
    return rect;
}

UiRect smallActionDialogRect()
{
    return {{410.0f, 170.0f}, {460.0f, 330.0f}};
}

UiRect smallActionChoiceRectForDialog(UiRect dialog, int index)
{
    constexpr float ChoiceGap = 16.0f;
    constexpr float ButtonHorizontalInset = 22.0f;
    const UiRect body = uiBodyRect(dialog);
    const float width = std::max(0.0f, body.size.x - ButtonHorizontalInset * 2.0f);
    return {
        {
            body.pos.x + (body.size.x - width) * 0.5f,
            body.pos.y + 20.0f + static_cast<float>(index) * (ui::ButtonHeight + ChoiceGap),
        },
        {width, ui::ButtonHeight},
    };
}

UiRect smallActionChoiceRect(int index)
{
    return smallActionChoiceRectForDialog(smallActionDialogRect(), index);
}

Vec2 smallActionInfoTextPos(UiRect panel)
{
    const UiRect body = uiBodyRect(panel);
    return body.pos + Vec2{8.0f, -18.0f};
}

UiRect storageActionDialogRect()
{
    UiRect rect = smallActionDialogRect();
    rect.size.y += 48.0f;
    return rect;
}

UiRect storageBulkDialogRect()
{
    UiRect rect = smallActionDialogRect();
    rect.size.y += 120.0f;
    return rect;
}

UiRect storageActionChoiceRect(int index)
{
    return smallActionChoiceRectForDialog(storageActionDialogRect(), index);
}

UiRect storageBulkChoiceRect(int index)
{
    return smallActionChoiceRectForDialog(storageBulkDialogRect(), index);
}

std::string formatDiaryPlayTime(std::int64_t seconds)
{
    const std::int64_t totalMinutes = std::max<std::int64_t>(0, seconds) / 60;
    const std::int64_t hours = totalMinutes / 60;
    const std::int64_t minutes = totalMinutes % 60;
    return std::to_string(hours) + "時間" + std::to_string(minutes) + "分";
}

int codexCompletionPercent(int discoveredCount, int totalCount)
{
    if (totalCount <= 0) {
        return 0;
    }
    return std::clamp(discoveredCount * 100 / totalCount, 0, 100);
}

int itemCodexObjectCount(const ObjectCatalog& catalog)
{
    return static_cast<int>(std::count_if(
        catalog.objects.begin(),
        catalog.objects.end(),
        [](const ObjectDefinition& object) {
            return !isCodexHiddenObject(object);
        }));
}

int enemyCodexEnemyCount(const EnemyCatalog& catalog)
{
    return static_cast<int>(std::count_if(
        catalog.enemies.begin(),
        catalog.enemies.end(),
        [](const EnemyDefinition& enemy) {
            return !isCodexHiddenEnemy(enemy);
        }));
}

std::vector<const EnemyDefinition*> enemyCodexEnemies(const EnemyCatalog& catalog)
{
    std::vector<const EnemyDefinition*> enemies;
    enemies.reserve(catalog.enemies.size());
    for (const EnemyDefinition& enemy : catalog.enemies) {
        if (!isCodexHiddenEnemy(enemy)) {
            enemies.push_back(&enemy);
        }
    }
    return enemies;
}

bool baseFacilityIsPerson(std::string_view facilityId)
{
    return facilityId == "monica" ||
        facilityId == "merchant_npc" ||
        facilityId == "processor_npc" ||
        facilityId == "elder";
}

std::string hiddenBaseNpcRemovedFlag(std::string_view facilityId)
{
    return "hidden_npc_removed:" + std::string(facilityId);
}

struct HiddenBaseNpcDefinition {
    std::string_view facilityId;
    std::string_view enemyId;
    bool monica = false;
};

constexpr std::array<HiddenBaseNpcDefinition, 4> HiddenBaseNpcDefinitions{{
    {"merchant_npc", "base_npc_merchant", false},
    {"processor_npc", "base_npc_processor", false},
    {"elder", "base_npc_elder", false},
    {"monica", "base_npc_monica", true},
}};

const HiddenBaseNpcDefinition* hiddenBaseNpcDefinitionForFacility(std::string_view facilityId)
{
    const auto it = std::find_if(
        HiddenBaseNpcDefinitions.begin(),
        HiddenBaseNpcDefinitions.end(),
        [facilityId](const HiddenBaseNpcDefinition& definition) {
            return definition.facilityId == facilityId;
        });
    return it == HiddenBaseNpcDefinitions.end() ? nullptr : &*it;
}

bool hiddenBaseNpcTargetFacility(std::string_view facilityId)
{
    const HiddenBaseNpcDefinition* definition = hiddenBaseNpcDefinitionForFacility(facilityId);
    return definition != nullptr && !definition->monica;
}

void applyHiddenRouteFacilityAvailability(
    std::vector<BaseFacility>& facilities,
    bool peopleGone,
    bool orbitCorruptionUnlocked,
    bool merchantRemoved,
    bool processorRemoved,
    bool elderRemoved)
{
    if (peopleGone || merchantRemoved) {
        for (BaseFacility& facility : facilities) {
            if (std::string_view(facility.facilityId) == "merchant_wagon") {
                facility.enabled = false;
                facility.showNameLabel = false;
            }
        }
    }
    if (peopleGone || processorRemoved) {
        for (BaseFacility& facility : facilities) {
            if (std::string_view(facility.facilityId) == "processing_table") {
                facility.enabled = false;
                facility.showNameLabel = false;
            }
        }
    }
    facilities.erase(
        std::remove_if(
            facilities.begin(),
            facilities.end(),
            [peopleGone, orbitCorruptionUnlocked, merchantRemoved, processorRemoved, elderRemoved](const BaseFacility& facility) {
                const std::string_view facilityId = facility.facilityId;
                if (peopleGone && baseFacilityIsPerson(facilityId)) {
                    return true;
                }
                if (orbitCorruptionUnlocked && facilityId == "monica") {
                    return true;
                }
                return (merchantRemoved && facilityId == "merchant_npc") ||
                    (processorRemoved && facilityId == "processor_npc") ||
                    (elderRemoved && facilityId == "elder");
            }),
        facilities.end());
}

const char* bookshelfMenuLabel(int index)
{
    switch (index) {
    case 0:
        return "アイテム図鑑";
    case 1:
        return "モンスター図鑑";
    case BookshelfEndingReplayMenuIndex:
        return "エンディングを見る";
    default:
        return "";
    }
}

const char* bookshelfEndingReplayLabel(EndingKind kind)
{
    switch (kind) {
    case EndingKind::Main:
        return "エンド1";
    case EndingKind::EncyclopediaComplete:
        return "図鑑コンプリートエンド";
    case EndingKind::AstralClear:
        return "不可思議の迷宮踏破エンド";
    case EndingKind::HiddenBad:
    case EndingKind::MainFailedTrust:
    case EndingKind::MainFailedMonicaMissing:
    case EndingKind::EncyclopediaFailedTrust:
    case EndingKind::AstralFailedTrust:
        break;
    }
    return "エンディング";
}

std::vector<UiCommandMenuItem> bookshelfEndingReplayCommandItems(const std::vector<EndingKind>& choices)
{
    std::vector<UiCommandMenuItem> items;
    items.reserve(choices.size());
    for (EndingKind kind : choices) {
        items.push_back({bookshelfEndingReplayLabel(kind), true});
    }
    return items;
}

UiRect merchantActionDialogRect()
{
    return smallActionDialogRect();
}

UiRect merchantActionChoiceRect(int index)
{
    return smallActionChoiceRect(index);
}

UiRect bookshelfMenuPanelRect(int itemCount)
{
    UiRect rect = merchantActionDialogRect();
    const int extraItems = std::max(0, itemCount - BookshelfMenuItemCount);
    rect.size.y += static_cast<float>(extraItems) * (ui::ButtonHeight + BookshelfMenuChoiceGap);
    return rect;
}

UiRect bookshelfMenuPanelRect()
{
    return bookshelfMenuPanelRect(BookshelfMenuItemCount);
}

UiRect bookshelfMenuChoiceRect(UiRect panel, int index)
{
    return smallActionChoiceRectForDialog(panel, index);
}

UiRect bookshelfMenuChoiceRect(int index)
{
    return bookshelfMenuChoiceRect(bookshelfMenuPanelRect(), index);
}

InventoryUiGridStyle bookshelfGridStyle()
{
    InventoryUiGridStyle style;
    style.visibleRows = 5;
    style.scroll.wheelStep = style.slotSize.y + style.slotGap.y;
    style.scroll.scrollbarPaddingX = 2.0f;
    style.scroll.scrollbarPaddingY = 0.0f;
    return style;
}

UiRect bookshelfGridViewport()
{
    return inventoryUiGridViewport({72.0f, 142.0f}, bookshelfGridStyle());
}

const char* enemyMoveSpeedLabel(double speed)
{
    if (!std::isfinite(speed) || speed <= 20.0) {
        return "かなり遅い";
    }
    if (speed <= 35.0) {
        return "遅い";
    }
    if (speed <= 45.0) {
        return "やや遅い";
    }
    if (speed <= 55.0) {
        return "まあまあ";
    }
    if (speed <= 65.0) {
        return "やや速い";
    }
    if (speed <= 80.0) {
        return "速い";
    }
    return "かなり速い";
}

const char* enemyCaptureDifficultyLabel(int difficulty)
{
    if (difficulty <= 1) {
        return "超簡単";
    }
    if (difficulty == 2) {
        return "簡単";
    }
    if (difficulty == 3) {
        return "やや簡単";
    }
    if (difficulty == 4) {
        return "まあまあ";
    }
    if (difficulty == 5) {
        return "ややムズい";
    }
    if (difficulty == 6) {
        return "ムズい";
    }
    return "激ムズ";
}

std::string enemyContactAttackText(const EnemyDefinition& enemy)
{
    if (enemy.contactAttackPower <= 0) {
        return "-";
    }

    std::string text = std::to_string(enemy.contactAttackPower);
    const std::string damageType = normalizeDamageType(enemy.contactDamageType);
    if (!damageType.empty() && damageType != "none") {
        text += "（";
        text += damageTypeDisplayName(damageType);
        text += "ダメージ）";
    }
    return text;
}

constexpr int RingWorkshopActionCount = 2;
constexpr int RingWorkshopUpgradeDisplayCount = RingWorkshopImplementedUpgradeCount;
constexpr float RingWorkshopScrollGaugeTop = 34.0f;
constexpr float RingWorkshopScrollRadiusInfoTop = 56.0f;
constexpr float RingWorkshopScrollSectionTop = 96.0f;
constexpr float RingWorkshopScrollButtonTop = 126.0f;
constexpr float RingWorkshopScrollButtonHeight = 42.0f;
constexpr float RingWorkshopScrollButtonPitch = 46.0f;
constexpr float RingWorkshopScrollBottomPadding = 12.0f;
constexpr float RingWorkshopScrollButtonInsetX = 14.0f;
constexpr float RingWorkshopScrollButtonWidth = 338.0f;

template <std::size_t N>
int updateVerticalTabClickSelection(
    UiTabsState& state,
    UiContext& ui,
    int selectedIndex,
    const std::array<UiVerticalTabItem, N>& items,
    const std::array<UiRect, N>& rects)
{
    UiTabsInput input{};
    state.focusedIndex = std::clamp(selectedIndex, 0, static_cast<int>(N) - 1);
    return updateUiVerticalTabs(
        state,
        ui,
        input,
        selectedIndex,
        items.data(),
        static_cast<int>(items.size()),
        rects.data());
}

template <std::size_t N>
int updateVerticalTabClickSelection(
    UiTabsState& state,
    UiContext& ui,
    int selectedIndex,
    const std::array<UiVerticalTabItem, N>& items,
    const std::array<UiRect, N>& rects,
    int itemCount)
{
    UiTabsInput input{};
    const int clampedCount = std::clamp(itemCount, 0, static_cast<int>(N));
    if (clampedCount <= 0) {
        state.focusedIndex = -1;
        return -1;
    }
    state.focusedIndex = std::clamp(selectedIndex, 0, clampedCount - 1);
    return updateUiVerticalTabs(
        state,
        ui,
        input,
        selectedIndex,
        items.data(),
        clampedCount,
        rects.data());
}

bool updateClickSelection(UiContext& ui, UiRect rect, int index, int& selectedIndex)
{
    if (!ui.pressed(rect)) {
        return false;
    }
    selectedIndex = index;
    return true;
}

UiScrollAreaStyle ringWorkshopScrollAreaStyle()
{
    UiScrollAreaStyle style;
    style.wheelStep = RingWorkshopScrollButtonPitch;
    style.scrollbarWidth = 8.0f;
    style.scrollbarGap = 6.0f;
    style.scrollbarPaddingX = 4.0f;
    style.scrollbarPaddingY = 2.0f;
    style.outline = {255, 255, 255, 0};
    return style;
}

float ringWorkshopUpgradeScrollContentHeight()
{
    return RingWorkshopScrollButtonTop +
        static_cast<float>(RingWorkshopUpgradeDisplayCount) * RingWorkshopScrollButtonPitch -
        (RingWorkshopScrollButtonPitch - RingWorkshopScrollButtonHeight) +
        RingWorkshopScrollBottomPadding;
}

UiRect homeInteriorMapRect()
{
    return {{290.0f, 100.0f}, {700.0f, 520.0f}};
}

UiRect homeInteriorWalkBounds()
{
    return {{354.0f, 182.0f}, {572.0f, 388.0f}};
}

void drawHomeInteriorBackdrop(Renderer& renderer)
{
    const UiRect room = homeInteriorMapRect();
    if (renderer.drawImage(
            "assets/kyoten/map_house.png",
            room.pos + room.size * 0.5f,
            room.size,
            ImageDrawOptions{},
            TextureFilter::Nearest)) {
        return;
    }

    renderer.fillRect(room.pos, room.size, {46, 36, 38, 255});
    renderer.drawRect(room.pos, room.size, {184, 150, 108, 255});
    renderer.fillRect(room.pos + Vec2{56.0f, 82.0f}, {568.0f, 376.0f}, {118, 92, 66, 255});
    renderer.drawText(room.pos + Vec2{294.0f, 42.0f}, "ルネの家", {246, 235, 255, 255}, 2);
}

UiRect ringWorkshopActionDialogRect()
{
    return smallActionDialogRect();
}

UiRect ringWorkshopActionChoiceRect(int index)
{
    return smallActionChoiceRect(index);
}

UiRect ringWorkshopPanelRect()
{
    return baseUpgradePanelRect();
}

UiRect ringWorkshopDetailPanelRect()
{
    return baseUpgradeDetailPanelRect();
}

UiRect ringWorkshopRingTabRect(int index, int unlockedRingCount = SpellRingCount)
{
    constexpr float TabTop = 126.0f;
    constexpr float TabGap = 22.0f;
    const int ringCount = std::clamp(unlockedRingCount, 1, SpellRingCount);
    const UiRect panel = ringWorkshopPanelRect();
    const float left = panel.pos.x + 38.0f;
    const float right = panel.pos.x + panel.size.x - 38.0f;
    const float totalGap = TabGap * static_cast<float>(std::max(0, ringCount - 1));
    const float width = std::max(1.0f, (right - left - totalGap) / static_cast<float>(ringCount));
    const float pitch = width + TabGap;
    return {{left + static_cast<float>(index) * pitch, TabTop}, {width, ui::ButtonHeight}};
}

UiRect ringWorkshopRespecPanelRect()
{
    const UiRect firstTab = ringWorkshopRingTabRect(0);
    const UiRect lastTab = ringWorkshopRingTabRect(SpellRingCount - 1);
    return {{
        firstTab.pos.x,
        firstTab.pos.y + firstTab.size.y + 30.0f,
    }, {
        lastTab.pos.x + lastTab.size.x - firstTab.pos.x,
        302.0f,
    }};
}

UiRect ringWorkshopRespecDetailPanelRect()
{
    UiRect detail = ringWorkshopDetailPanelRect();
    detail.pos.y = ringWorkshopRespecPanelRect().pos.y;
    return detail;
}

UiRect ringWorkshopRespecKindRect(int index)
{
    constexpr float TopGap = 42.0f;
    constexpr float RowGap = 16.0f;
    constexpr float RowHeight = 60.0f;
    const UiRect panel = ringWorkshopRespecPanelRect();
    const UiRect detail = ringWorkshopRespecDetailPanelRect();
    const float left = panel.pos.x + 24.0f;
    const float right = detail.pos.x - 28.0f;
    return {{
        left,
        panel.pos.y + TopGap + static_cast<float>(index) * (RowHeight + RowGap),
    }, {
        std::max(1.0f, right - left),
        RowHeight,
    }};
}

UiRect ringWorkshopRespecConfirmRect()
{
    const UiRect detail = ringWorkshopRespecDetailPanelRect();
    const Vec2 size{220.0f, ui::ButtonHeight};
    const float leftAreaLeft = ringWorkshopPanelRect().pos.x + 72.0f;
    const float leftAreaRight = detail.pos.x - 24.0f;
    return {{
        leftAreaLeft + (leftAreaRight - leftAreaLeft - size.x) * 0.5f,
        baseUpgradeConfirmRect().pos.y,
    }, size};
}

UiRect ringWorkshopUpgradeScrollViewportRect()
{
    const UiRect panel = ringWorkshopPanelRect();
    const UiRect firstTab = ringWorkshopRingTabRect(0);
    return {{
        panel.pos.x + 24.0f,
        firstTab.pos.y + firstTab.size.y + 18.0f,
    }, {
        394.0f,
        392.0f,
    }};
}

UiRect ringWorkshopUpgradeItemRect(const UiScrollAreaLayout& scroll, int index)
{
    return {{
        scroll.content.pos.x + RingWorkshopScrollButtonInsetX,
        scroll.content.pos.y + RingWorkshopScrollButtonTop +
            static_cast<float>(index) * RingWorkshopScrollButtonPitch -
            scroll.scrollOffset,
    }, {
        RingWorkshopScrollButtonWidth,
        RingWorkshopScrollButtonHeight,
    }};
}

UiRect ringWorkshopUpgradeDetailPanelRect()
{
    const UiRect list = ringWorkshopUpgradeScrollViewportRect();
    const UiRect panel = ringWorkshopPanelRect();
    return {{
        list.pos.x + list.size.x + 28.0f,
        list.pos.y,
    }, {
        panel.pos.x + panel.size.x - 38.0f - (list.pos.x + list.size.x + 28.0f),
        354.0f,
    }};
}

UiRect ringWorkshopUpgradeConfirmRect()
{
    const UiRect detail = ringWorkshopUpgradeDetailPanelRect();
    const Vec2 size{208.0f, ui::ButtonHeight};
    return {{detail.pos.x + (detail.size.x - size.x) * 0.5f, 572.0f}, size};
}

UiRect ringWorkshopRadiusGaugeRect(const UiScrollAreaLayout& scroll)
{
    return {{
        scroll.content.pos.x + RingWorkshopScrollButtonInsetX,
        scroll.content.pos.y + RingWorkshopScrollGaugeTop - scroll.scrollOffset,
    }, {
        RingWorkshopScrollButtonWidth,
        14.0f,
    }};
}

RingLevelUpgradeKind ringWorkshopKindForIndex(int index)
{
    switch (index) {
    case 1:
        return RingLevelUpgradeKind::Speed;
    case 2:
        return RingLevelUpgradeKind::WeightLimit;
    case 0:
    default:
        return RingLevelUpgradeKind::Radius;
    }
}

const char* ringWorkshopActionLabel(int index)
{
    switch (index) {
    case 0: return "リング強化";
    case 1: return "レベルアップ配分調整";
    default: return "";
    }
}

const char* ringWorkshopUpgradeShortName(int index)
{
    switch (index) {
    case 0: return "リング半径の上限強化";
    case 1: return "リング半径の下限強化";
    case 2: return "リング速度強化";
    case 3: return "リング重量制限強化";
    case 4: return "ずらし距離強化";
    case 5: return "リング投げ距離強化";
    case 6: return "リング投げクールダウン短縮";
    case 7: return "リング重量ペナルティ軽減";
    case 8: return "リング装着枠増加";
    default: return "未解禁";
    }
}

const char* ringWorkshopUpgradeDescription(int index)
{
    switch (index) {
    case 0:
        return "リング半径の上限を広げます。広い範囲にアイテムを配置しやすくなります。";
    case 1:
        return "リング半径の下限を下げます。小さく締めたリングを使いやすくなります。";
    case 2:
        return "リング速度を上げます。配置したアイテムの発動機会が増えます。";
    case 3:
        return "リングの重量制限を増やします。重いアイテムを装着しやすくなります。";
    case 4:
        return "リング位置をずらす距離を伸ばします。状況に合わせてリングを動かしやすくなります。";
    case 5:
        return "リングを投げられる距離を伸ばします。離れた位置へリングを展開しやすくなります。";
    case 6:
        return "リング投げの再使用時間を短縮します。投げ直しの隙が小さくなります。";
    case 7:
        return "リングが重いときの停止ペナルティを軽減します。重い構成でも動きを保ちやすくなります。";
    case 8:
        return "リングに装着できるアイテム枠を増やします。より多くのアイテムを組み込めます。";
    }
    return "";
}

std::string formatRingWorkshopValue(RingLevelUpgradeKind kind, float value)
{
    char buffer[64];
    switch (kind) {
    case RingLevelUpgradeKind::Radius:
        std::snprintf(buffer, sizeof(buffer), "%.2fm", worldDistanceToMeters(value));
        break;
    case RingLevelUpgradeKind::Speed:
        std::snprintf(buffer, sizeof(buffer), "%.2fm/s", value);
        break;
    case RingLevelUpgradeKind::WeightLimit:
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", value);
        break;
    }
    return buffer;
}

struct ColoredTextSourceRun {
    std::string_view text;
    Color color;
};

struct WrappedColoredTextRun {
    std::string text;
    Color color;
};

struct WrappedColoredTextLine {
    std::string text;
    std::vector<WrappedColoredTextRun> runs;
};

bool sameColor(Color lhs, Color rhs)
{
    return lhs.r == rhs.r
        && lhs.g == rhs.g
        && lhs.b == rhs.b
        && lhs.a == rhs.a;
}

std::size_t utf8CodepointByteLength(unsigned char lead)
{
    if ((lead & 0x80U) == 0U) {
        return 1;
    }
    if ((lead & 0xE0U) == 0xC0U) {
        return 2;
    }
    if ((lead & 0xF0U) == 0xE0U) {
        return 3;
    }
    if ((lead & 0xF8U) == 0xF0U) {
        return 4;
    }
    return 1;
}

void appendColoredTextRun(std::vector<WrappedColoredTextRun>& runs, std::string_view text, Color color)
{
    if (text.empty()) {
        return;
    }
    if (!runs.empty() && sameColor(runs.back().color, color)) {
        runs.back().text.append(text.data(), text.size());
        return;
    }
    runs.push_back({std::string{text.data(), text.size()}, color});
}

void appendWrappedColoredTextToken(WrappedColoredTextLine& line, std::string_view token, Color color)
{
    if (token.empty()) {
        return;
    }
    line.text.append(token.data(), token.size());
    appendColoredTextRun(line.runs, token, color);
}

float coloredTextAdvance(Renderer& renderer, std::string_view text, int scale)
{
    if (text.empty()) {
        return 0.0f;
    }
#ifdef _WIN32
    constexpr float NativeTextTexturePaddingX = 4.0f;
#else
    constexpr float NativeTextTexturePaddingX = 0.0f;
#endif
    return std::max(0.0f, renderer.measureText(text, scale).x - NativeTextTexturePaddingX);
}

float wrappedColoredTextLineAdvance(Renderer& renderer, int scale)
{
    const float singleLineHeight = renderer.measureText("あ", scale).y;
    const float twoLineHeight = renderer.measureText("あ\nあ", scale).y;
    return std::max(1.0f, twoLineHeight - singleLineHeight);
}

float drawWrappedColoredText(
    Renderer& renderer,
    Vec2 pos,
    const std::vector<ColoredTextSourceRun>& sourceRuns,
    float maxWidth,
    int scale)
{
    std::vector<WrappedColoredTextLine> lines;
    WrappedColoredTextLine line;
    const float wrapWidth = std::max(1.0f, maxWidth);

    for (const ColoredTextSourceRun& run : sourceRuns) {
        for (std::size_t i = 0; i < run.text.size();) {
            const char c = run.text[i];
            if (c == '\n') {
                lines.push_back(std::move(line));
                line = {};
                ++i;
                continue;
            }

            const std::size_t charLength = std::min(
                utf8CodepointByteLength(static_cast<unsigned char>(c)),
                run.text.size() - i);
            const std::string_view token{run.text.data() + i, charLength};
            std::string candidate = line.text;
            candidate.append(token.data(), token.size());
            if (!line.text.empty() && renderer.measureText(candidate, scale).x > wrapWidth) {
                lines.push_back(std::move(line));
                line = {};
            }
            appendWrappedColoredTextToken(line, token, run.color);
            i += charLength;
        }
    }

    if (!line.text.empty()) {
        lines.push_back(std::move(line));
    }
    if (lines.empty()) {
        return 0.0f;
    }

    const float singleLineHeight = renderer.measureText("あ", scale).y;
    const float lineAdvance = wrappedColoredTextLineAdvance(renderer, scale);
    float y = pos.y;
    for (const WrappedColoredTextLine& wrappedLine : lines) {
        float x = pos.x;
        for (const WrappedColoredTextRun& run : wrappedLine.runs) {
            renderer.drawText({x, y}, run.text, run.color, scale);
            x += coloredTextAdvance(renderer, run.text, scale);
        }
        y += lineAdvance;
    }
    return singleLineHeight + lineAdvance * static_cast<float>(lines.size() - 1);
}

UiRect baseBrokenRingDepartureConfirmRect()
{
    return {{410.0f, 230.0f}, {460.0f, 250.0f}};
}

UiRect baseRoguelikeDepartureConfirmRect()
{
    return {{260.0f, 54.0f}, {760.0f, 610.0f}};
}

void drawRoguelikeDepartureConfirmDialog(Renderer& renderer, const UiConfirmDialogState& state)
{
    if (!state.open) {
        return;
    }

    const UiRect panel = baseRoguelikeDepartureConfirmRect();
    UiWindowScope window(
        renderer,
        "base.roguelike_departure.confirm",
        panel,
        state.title,
        uiConfirmDialogHelpText(),
        UiWindowOptions{true, true});

    constexpr int TextScale = 2;
    constexpr float ContentInset = 54.0f;
    constexpr float BodyTopOffset = 8.0f;
    constexpr float ParagraphGap = 9.0f;
    constexpr float BulletGap = 6.0f;
    constexpr float BulletIndent = 30.0f;
    constexpr Color BulletColor{255, 230, 150, 255};
    constexpr Color EmphasisColor{255, 230, 150, 255};
    const float bodyTop = panel.pos.y + ui::HeaderHeight + BodyTopOffset;
    const UiRect body{{
        panel.pos.x + ContentInset,
        bodyTop,
    }, {
        panel.size.x - ContentInset * 2.0f,
        std::max(0.0f, uiConfirmDialogButtonRect(panel, 0).pos.y - bodyTop - 18.0f),
    }};

    float y = body.pos.y;
    const auto drawParagraph = [&](std::string_view text, Color color, float gap) {
        renderer.drawWrappedText({body.pos.x, y}, text, body.size.x, color, TextScale);
        y += renderer.measureWrappedText(text, body.size.x, TextScale).y + gap;
    };
    const auto drawBullet = [&](const RoguelikeDepartureRuleText& rule) {
        const float textX = body.pos.x + BulletIndent;
        const float textWidth = std::max(1.0f, body.size.x - BulletIndent);
        std::vector<ColoredTextSourceRun> textRuns;
        textRuns.reserve(3);
        const auto appendTextRun = [&](std::string_view text, Color color) {
            if (!text.empty()) {
                textRuns.push_back({text, color});
            }
        };
        const std::size_t emphasisPos = rule.emphasis.empty() ? std::string_view::npos : rule.text.find(rule.emphasis);
        if (emphasisPos == std::string_view::npos) {
            appendTextRun(rule.text, ui::Text);
        } else {
            appendTextRun(rule.text.substr(0, emphasisPos), ui::Text);
            appendTextRun(rule.text.substr(emphasisPos, rule.emphasis.size()), EmphasisColor);
            appendTextRun(rule.text.substr(emphasisPos + rule.emphasis.size()), ui::Text);
        }

        renderer.drawText({body.pos.x, y}, "・", BulletColor, TextScale);
        const float textHeight = drawWrappedColoredText(renderer, {textX, y}, textRuns, textWidth, TextScale);
        const float lineHeight = std::max(
            renderer.measureText("・", TextScale).y,
            textHeight);
        y += lineHeight + BulletGap;
    };

    drawParagraph("このダンジョンはローグライクダンジョンです", ui::Text, ParagraphGap);
    drawParagraph("以下の特殊ルールが設定されています", ui::Text, ParagraphGap);
    y += 3.0f;
    for (const RoguelikeDepartureRuleText& rule : RoguelikeDepartureRules) {
        drawBullet(rule);
    }
    y += 8.0f;
    drawParagraph("出発しますか？", ui::Text, 0.0f);

    drawUiConfirmDialogButtons(renderer, state, panel);
}

Vec2 baseSystemMessagePos(
    UiRect panel,
    bool storageActive,
    bool merchantActive,
    bool processingActive,
    bool upgradeActive)
{
    if (upgradeActive) {
        return baseUpgradePanelRect().pos + Vec2{32.0f, 468.0f};
    }
    if (storageActive || merchantActive || processingActive) {
        return {80.0f, 500.0f};
    }
    return panel.pos + Vec2{54.0f, 454.0f};
}

void drawTextCentered(Renderer& renderer, UiRect rect, float y, std::string_view text, Color color, int scale)
{
    const Vec2 size = renderer.measureText(text, scale);
    renderer.drawText({rect.pos.x + (rect.size.x - size.x) * 0.5f, y}, text, color, scale);
}

void drawStorageHeader(Renderer& renderer, float x, float y, std::string_view title, std::string_view count, Color color)
{
    renderer.drawText({x, y}, title, color, 3);
    const Vec2 titleSize = renderer.measureText(title, 3);
    renderer.drawText(
        {x + titleSize.x + StorageHeaderCountGap, y + StorageHeaderCountYOffset},
        count,
        color,
        StorageHeaderCountScale);
}

void drawStoragePageSelector(Renderer& renderer, int page, int pageCount)
{
    char buffer[64];
    const UiRect prevPageRect = storagePrevPageButtonRect();
    const UiRect pageTextRect = storagePageTextRect();
    const UiRect nextPageRect = storageNextPageButtonRect();
    std::snprintf(buffer, sizeof(buffer), "%d/%d", page + 1, pageCount);
    drawTextCentered(renderer, pageTextRect, StorageBottomHeaderY + StoragePageTextYOffset, buffer, {198, 198, 206, 255}, StoragePageTextScale);
    drawUiRectButton(renderer, prevPageRect, "<", false);
    drawUiRectButton(renderer, nextPageRect, ">", false);
}

UiRect merchantSellGridSlotRect(int index)
{
    UiRect rect = merchantGridSlotRect(index);
    rect.pos.y += MerchantSellItemYOffset;
    return rect;
}

UiRect merchantSellSortButtonRect()
{
    UiRect rect = uiBottomLeftButtonRect(merchantPanelRect(), {180.0f, ui::ButtonHeight});
    rect.pos.x = storageItemCircleLeftX();
    return rect;
}

UiRect externalWarehouseSourceSlotRect(UiRect(*sourceSlotRect)(int), int index)
{
    UiRect rect = sourceSlotRect(index);
    rect.pos.y += ExternalWarehouseGridYOffset;
    return rect;
}

UiPageSelectorRects externalWarehousePageSelectorRects(UiRect(*sourceSlotRect)(int))
{
    const UiRect first = externalWarehouseSourceSlotRect(sourceSlotRect, 0);
    const UiRect last = externalWarehouseSourceSlotRect(sourceSlotRect, StorageColumns - 1);
    return uiPageSelectorRectsFromNextButton(
        {last.pos.x + last.size.x - StoragePageButtonSize, first.pos.y - StoragePageButtonSize - ExternalWarehousePageSelectorGap},
        StoragePageTextWidth);
}

void drawExternalWarehouseSourceHeader(
    Renderer& renderer,
    UiRect(*sourceSlotRect)(int),
    int page,
    int pageCount)
{
    const UiPageSelectorRects pageRects = externalWarehousePageSelectorRects(sourceSlotRect);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d/%d", page + 1, pageCount);
    drawTextCentered(renderer, pageRects.text, pageRects.text.pos.y + StoragePageTextYOffset, buffer, {198, 198, 206, 255}, StoragePageTextScale);
    drawUiRectButton(renderer, pageRects.prev, "<", false);
    drawUiRectButton(renderer, pageRects.next, ">", false);
}

UiPageSelectorRects storageWithdrawPageSelectorRects()
{
    const UiRect first = storageWithdrawSlotRect(0);
    const UiRect last = storageWithdrawSlotRect(StorageColumns - 1);
    return uiPageSelectorRectsFromNextButton(
        {last.pos.x + last.size.x - StoragePageButtonSize, first.pos.y - StoragePageButtonSize - ExternalWarehousePageSelectorGap},
        StoragePageTextWidth);
}

void drawStorageWithdrawHeader(Renderer& renderer, int page, int pageCount)
{
    const UiPageSelectorRects pageRects = storageWithdrawPageSelectorRects();
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d/%d", page + 1, pageCount);
    drawTextCentered(renderer, pageRects.text, pageRects.text.pos.y + StoragePageTextYOffset, buffer, {198, 198, 206, 255}, StoragePageTextScale);
    drawUiRectButton(renderer, pageRects.prev, "<", false);
    drawUiRectButton(renderer, pageRects.next, ">", false);
}

Vec2 baseRingPreviewCenterFromGrid(UiRect(*slotRect)(int), float yOffset)
{
    const UiRect first = slotRect(0);
    const UiRect last = slotRect(StoragePaneSlotCount - 1);
    return {
        first.pos.x + (slotRect(StorageColumns - 1).pos.x + first.size.x - first.pos.x) * 0.5f,
        first.pos.y + (last.pos.y + last.size.y - first.pos.y) * 0.5f + yOffset,
    };
}

Vec2 baseRingPreviewWeightTextPosFromGrid(UiRect(*slotRect)(int), float yOffset)
{
    const UiRect first = slotRect(0);
    return first.pos + Vec2{18.0f, yOffset - 52.0f};
}

Vec2 baseProcessingRingPreviewCenter()
{
    return baseRingPreviewCenterFromGrid(baseProcessingGridSlotRect, BaseProcessingRingYOffset);
}

Vec2 baseProcessingRingPreviewWeightTextPos()
{
    return baseRingPreviewWeightTextPosFromGrid(baseProcessingGridSlotRect, BaseProcessingRingYOffset);
}

Vec2 merchantSellRingPreviewCenter()
{
    return baseRingPreviewCenterFromGrid(merchantGridSlotRect, MerchantSellRingYOffset);
}

Vec2 merchantSellRingPreviewWeightTextPos()
{
    return baseRingPreviewWeightTextPosFromGrid(merchantGridSlotRect, MerchantSellRingYOffset);
}

Vec2 storageRingPreviewCenter()
{
    return baseRingPreviewCenterFromGrid(storageTransferGridSlotRect, MerchantSellRingYOffset) + Vec2{0.0f, -60.0f};
}

Vec2 storageRingPreviewWeightTextPos()
{
    return baseRingPreviewWeightTextPosFromGrid(storageTransferGridSlotRect, MerchantSellRingYOffset - 60.0f);
}

Vec2 baseRingPreviewCenterForShape(Vec2 center, RingShape shape)
{
    if (shape == RingShape::Comet) {
        constexpr float CometPreviewYOffset = 120.0f;
        center.y += CometPreviewYOffset;
    }
    return center;
}

Vec2 baseProcessingRingPreviewCenter(const SpellRingSystem& spellRing, int ringIndex)
{
    return baseRingPreviewCenterForShape(
        baseProcessingRingPreviewCenter(),
        spellRing.ringShapeForIndex(ringIndex));
}

Vec2 merchantSellRingPreviewCenter(const SpellRingSystem& spellRing, int ringIndex)
{
    return baseRingPreviewCenterForShape(
        merchantSellRingPreviewCenter(),
        spellRing.ringShapeForIndex(ringIndex));
}

Vec2 storageRingPreviewCenter(const SpellRingSystem& spellRing, int ringIndex)
{
    return baseRingPreviewCenterForShape(
        storageRingPreviewCenter(),
        spellRing.ringShapeForIndex(ringIndex));
}

float baseRingPreviewRadius(RingShape shape, float previewScale)
{
    return ringUiShapeRadius(shape) * previewScale;
}

Vec2 rotateAround(Vec2 point, Vec2 center, float radians)
{
    const Vec2 local = point - center;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return center + Vec2{
        local.x * c - local.y * s,
        local.x * s + local.y * c,
    };
}

RingOrbitContext baseRingPreviewOrbitContext(
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float previewScale)
{
    RingOrbitContext context;
    context.shape = spellRing.ringShapeForIndex(ringIndex);
    context.radius = baseRingPreviewRadius(context.shape, previewScale);
    context.shapeRotation = 0.0f;
    context.itemIndex = std::max(0, itemIndex);
    context.itemCount = std::max(1, itemCount);
    context.tuning = makeRingOrbitTuning(balance);
    return context;
}

Vec2 baseRingPreviewPoint(Vec2 center, RingShape shape, Vec2 point)
{
    if (shape == RingShape::Comet) {
        return rotateAround(point, center, RingUiCometArcRotation);
    }
    return point;
}

Vec2 baseRingPreviewItemAnchor(
    Vec2 center,
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float previewScale)
{
    const RingOrbitContext context = baseRingPreviewOrbitContext(spellRing, balance, ringIndex, itemIndex, itemCount, previewScale);
    const Vec2 point = getRingItemWorldPosition(center, item.localAngle, context);
    return baseRingPreviewPoint(center, context.shape, point);
}

Vec2 baseRingPreviewItemDrawCenter(
    Vec2 center,
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float previewScale,
    float totalSeconds)
{
    SpellRingItem displayItem = item;
    displayItem.worldPosition = baseRingPreviewItemAnchor(center, item, spellRing, balance, ringIndex, itemIndex, itemCount, previewScale);
    return ringItemDrawPosition(displayItem, totalSeconds);
}

UiRect baseRingPreviewItemRect(
    Vec2 previewCenter,
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float previewScale,
    float totalSeconds)
{
    constexpr Vec2 Size{54.0f, 54.0f};
    const Vec2 center = baseRingPreviewItemDrawCenter(previewCenter, item, spellRing, balance, ringIndex, itemIndex, itemCount, previewScale, totalSeconds);
    return {center - Size * 0.5f, Size};
}

UiRect baseProcessingRingItemRect(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float totalSeconds)
{
    return baseRingPreviewItemRect(
        baseProcessingRingPreviewCenter(spellRing, ringIndex),
        item,
        spellRing,
        balance,
        ringIndex,
        itemIndex,
        itemCount,
        BaseRingPreviewScale,
        totalSeconds);
}

UiRect merchantSellRingItemRect(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float totalSeconds)
{
    return baseRingPreviewItemRect(
        merchantSellRingPreviewCenter(spellRing, ringIndex),
        item,
        spellRing,
        balance,
        ringIndex,
        itemIndex,
        itemCount,
        StorageRingPreviewScale,
        totalSeconds);
}

UiRect storageRingItemRect(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float totalSeconds)
{
    return baseRingPreviewItemRect(
        storageRingPreviewCenter(spellRing, ringIndex),
        item,
        spellRing,
        balance,
        ringIndex,
        itemIndex,
        itemCount,
        MerchantSellRingPreviewScale,
        totalSeconds);
}

void drawBaseRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const RuntimeBalance& balance,
    Vec2 center,
    Vec2 weightTextPos,
    int ringIndex,
    int selectedIndex,
    float previewScale,
    float totalSeconds)
{
    const std::vector<SpellRingItem>& items = spellRing.itemsForRing(ringIndex);
    const RingShape shape = spellRing.ringShapeForIndex(ringIndex);
    drawRingWeightLimitText(renderer, weightTextPos, spellRing, ringIndex);

    RingOrbitContext context = baseRingPreviewOrbitContext(spellRing, balance, ringIndex, 0, static_cast<int>(items.size()), previewScale);
    std::vector<Vec2> orbitPath = getRingPathSamplePoints(center, context, 160);
    for (Vec2& point : orbitPath) {
        point = baseRingPreviewPoint(center, shape, point);
    }
    drawMagicOrbitPath(
        renderer,
        orbitPath,
        center,
        MagicOrbitDrawOptions{
            shape,
            true,
            false,
            true,
            true,
            ringIndex,
            totalSeconds,
            0.92f,
        });

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const SpellRingItem& item = items[static_cast<std::size_t>(i)];
        const Vec2 itemAnchor = baseRingPreviewItemAnchor(center, item, spellRing, balance, ringIndex, i, static_cast<int>(items.size()), previewScale);
        const Vec2 itemCenter = baseRingPreviewItemDrawCenter(center, item, spellRing, balance, ringIndex, i, static_cast<int>(items.size()), previewScale, totalSeconds);
        Vec2 outward = normalize(itemAnchor - center);
        if (lengthSquared(outward) <= 0.0001f) {
            outward = {0.0f, -1.0f};
        }
        Vec2 forward{-outward.y, outward.x};
        if (lengthSquared(forward) <= 0.0001f) {
            forward = {1.0f, 0.0f};
        }
        const bool selected = i == selectedIndex;
        const ItemData* object = objectForRingItem(objectCatalog, item);
        if (shape != RingShape::FigureEight) {
            const Color angleLineColor = selected ? Color{255, 230, 150, 120} : Color{94, 102, 128, 85};
            Vec2 tangent = normalize(Vec2{-outward.y, outward.x});
            if (lengthSquared(tangent) <= 0.0001f) {
                tangent = {0.0f, 1.0f};
            }
            constexpr float AngleLineHalfWidthPx = 0.5f;
            renderer.drawLine(center + tangent * AngleLineHalfWidthPx, itemAnchor + tangent * AngleLineHalfWidthPx, angleLineColor);
            renderer.drawLine(center - tangent * AngleLineHalfWidthPx, itemAnchor - tangent * AngleLineHalfWidthPx, angleLineColor);
        }
        drawRingItemShape(renderer, item, object, itemCenter, outward, forward, totalSeconds, selected);
        char label[16];
        std::snprintf(label, sizeof(label), "%d", i + 1);
        renderer.drawText(itemCenter + Vec2{-5.0f, 22.0f}, label, selected ? Color{255, 230, 150, 255} : Color{174, 182, 198, 255}, 1);
    }
}

void drawBaseProcessingRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const RuntimeBalance& balance,
    int ringIndex,
    int selectedIndex,
    float totalSeconds)
{
    drawBaseRingPreview(
        renderer,
        spellRing,
        objectCatalog,
        balance,
        baseProcessingRingPreviewCenter(spellRing, ringIndex),
        baseProcessingRingPreviewWeightTextPos(),
        ringIndex,
        selectedIndex,
        BaseRingPreviewScale,
        totalSeconds);
}

void drawMerchantSellRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const RuntimeBalance& balance,
    int ringIndex,
    int selectedIndex,
    float totalSeconds)
{
    drawBaseRingPreview(
        renderer,
        spellRing,
        objectCatalog,
        balance,
        merchantSellRingPreviewCenter(spellRing, ringIndex),
        merchantSellRingPreviewWeightTextPos(),
        ringIndex,
        selectedIndex,
        MerchantSellRingPreviewScale,
        totalSeconds);
}

void drawStorageRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const RuntimeBalance& balance,
    int ringIndex,
    int selectedIndex,
    float totalSeconds)
{
    drawBaseRingPreview(
        renderer,
        spellRing,
        objectCatalog,
        balance,
        storageRingPreviewCenter(spellRing, ringIndex),
        storageRingPreviewWeightTextPos(),
        ringIndex,
        selectedIndex,
        StorageRingPreviewScale,
        totalSeconds);
}

struct ProcessingResultSnapshot {
    std::string name;
    std::string objectId;
    int stackCount = 1;
    bool stackSource = false;
    bool isBroken = false;
    int currentDurability = -1;
    int maxDurability = -1;
    int baseDurability = -1;
    int rarity = 1;
    int enhanceLevel = 0;
    int attackEnhanceLevel = 0;
    int digEnhanceLevel = 0;
    int durabilityEnhanceLevel = 0;
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    double weightModifier = 1.0;
    double sizeModifier = 1.0;
};

std::string nonEmptyItemName(std::string_view name)
{
    return name.empty() ? std::string{"アイテム"} : std::string{name};
}

constexpr Color ConfirmAfterValueColor{255, 230, 150, 255};
constexpr Color RequirementShortageColor{238, 82, 82, 255};

struct RequirementRow {
    std::string label;
    std::string required;
    std::string owned;
    bool enough = true;
};

void drawUiTextRun(Renderer& renderer, Vec2& pos, std::string_view text, Color color, int scale = 2)
{
    renderer.drawText(pos, text, color, scale);
    pos.x += renderer.measureText(text, scale).x;
}

void drawUiInlineRun(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    Vec2& pos,
    std::string_view text,
    Color color = ui::Text,
    int scale = 2)
{
    InlineItemTextStyle style{};
    style.text = color;
    style.scale = scale;
    drawInlineItemText(renderer, catalog, pos, text, style);
    pos.x += measureInlineItemText(renderer, text, style).x;
}

RequirementRow moneyRequirementRow(int required, int owned)
{
    return RequirementRow{
        inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + " お金",
        std::to_string(required) + "G",
        std::to_string(owned) + "G",
        owned >= required,
    };
}

RequirementRow materialRequirementRow(MaterialType type, int required, int owned)
{
    return RequirementRow{
        inlineMaterialIconTag(type) + std::string(materialTypeDisplayName(type)),
        "×" + std::to_string(required),
        "×" + std::to_string(owned),
        owned >= required,
    };
}

void drawRequirementValue(Renderer& renderer, Vec2 pos, const RequirementRow& row)
{
    const Color valueColor = row.enough ? ui::Text : RequirementShortageColor;
    drawUiTextRun(renderer, pos, row.required, valueColor);
    drawUiTextRun(renderer, pos, "（", ui::TextMuted);
    drawUiTextRun(renderer, pos, row.owned, valueColor);
    drawUiTextRun(renderer, pos, "）", ui::TextMuted);
}

void drawRequirementRows(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    UiRect content,
    const std::vector<RequirementRow>& rows)
{
    constexpr float LabelWidth = 140.0f;
    constexpr float LineHeight = 31.0f;
    float y = content.pos.y;
    if (rows.empty()) {
        renderer.drawText({content.pos.x, y}, "なし", ui::TextMuted, 2);
        return;
    }
    for (const RequirementRow& row : rows) {
        Vec2 labelPos{content.pos.x, y};
        drawUiInlineRun(renderer, catalog, labelPos, row.label, ui::Text);
        drawRequirementValue(renderer, {content.pos.x + LabelWidth, y}, row);
        y += LineHeight;
    }
}

void drawRequirementSubWindow(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    UiRect panel,
    const std::vector<RequirementRow>& rows)
{
    drawUiSubPanel(renderer, panel);
    UiRect content = uiSubPanelContentRect(panel);
    const float topPadding = ui::SubPanelPadding.y;
    content.pos.y = panel.pos.y + topPadding;
    content.size.y = std::max(0.0f, panel.size.y - topPadding - ui::SubPanelPadding.y);
    renderer.drawText(content.pos, "必要素材", ui::TextMuted, 2);
    content.pos.y += 34.0f;
    content.size.y = std::max(0.0f, content.size.y - 34.0f);
    drawRequirementRows(renderer, catalog, content, rows);
}

ProcessingResultSnapshot processingSnapshotFromStack(const InventoryObjectStack& stack)
{
    ProcessingResultSnapshot snapshot{};
    snapshot.objectId = stack.objectId;
    snapshot.stackCount = stack.count;
    snapshot.stackSource = true;
    snapshot.currentDurability = stack.item.durability;
    snapshot.maxDurability = stack.item.durability;
    snapshot.baseDurability = stack.item.durability;
    snapshot.rarity = std::clamp(stack.item.rarity, 1, 10);
    snapshot.isBroken = stack.item.durability == 0;
    snapshot.name = nonEmptyItemName(itemDisplayName(stack.item.name, snapshot.isBroken));
    return snapshot;
}

ProcessingResultSnapshot processingSnapshotFromInstance(const InventoryObjectInstance& entry)
{
    ProcessingResultSnapshot snapshot{};
    snapshot.objectId = entry.instance.objectId.empty() ? entry.item.id : entry.instance.objectId;
    snapshot.stackCount = 1;
    snapshot.currentDurability = entry.instance.currentDurability;
    snapshot.maxDurability = entry.instance.maxDurability;
    snapshot.baseDurability = entry.item.durability;
    snapshot.rarity = std::clamp(entry.item.rarity, 1, 10);
    snapshot.isBroken = entry.instance.isBroken;
    snapshot.name = nonEmptyItemName(itemDisplayName(entry.item.name, snapshot.isBroken));
    snapshot.enhanceLevel = entry.instance.enhanceLevel;
    snapshot.attackEnhanceLevel = entry.instance.attackEnhanceLevel;
    snapshot.digEnhanceLevel = entry.instance.digEnhanceLevel;
    snapshot.durabilityEnhanceLevel = entry.instance.durabilityEnhanceLevel;
    snapshot.attackBonus = entry.instance.attackBonus;
    snapshot.digBonus = entry.instance.digBonus;
    snapshot.durabilityBonus = entry.instance.durabilityBonus;
    snapshot.weightModifier = entry.instance.weightModifier;
    snapshot.sizeModifier = entry.instance.sizeModifier;
    return snapshot;
}

ProcessingResultSnapshot processingSnapshotFromRingItem(const ObjectCatalog& catalog, const SpellRingItem& item)
{
    ProcessingResultSnapshot snapshot{};
    snapshot.objectId = item.objectId;
    snapshot.name = nonEmptyItemName(ringItemDisplayName(catalog, item));
    snapshot.stackCount = 1;
    snapshot.currentDurability = item.durability;
    snapshot.maxDurability = item.maxDurability;
    if (const ItemData* object = catalog.registry.findById(item.objectId)) {
        snapshot.baseDurability = object->durability;
        snapshot.rarity = std::clamp(object->rarity, 1, 10);
    }
    snapshot.isBroken = item.broken();
    snapshot.enhanceLevel = item.enhanceLevel;
    snapshot.attackEnhanceLevel = item.attackEnhanceLevel;
    snapshot.digEnhanceLevel = item.digEnhanceLevel;
    snapshot.durabilityEnhanceLevel = item.durabilityEnhanceLevel;
    snapshot.attackBonus = item.attackBonus;
    snapshot.digBonus = item.digBonus;
    snapshot.durabilityBonus = item.durabilityBonus;
    snapshot.weightModifier = item.weightModifier;
    snapshot.sizeModifier = item.sizeModifier;
    return snapshot;
}

ProcessingResultSnapshot processingEnhancedSnapshot(
    ProcessingResultSnapshot snapshot,
    int attackBonus,
    int digBonus,
    int durabilityBonus)
{
    snapshot.stackCount = 1;
    snapshot.stackSource = false;
    ++snapshot.enhanceLevel;
    if (attackBonus > 0) {
        ++snapshot.attackEnhanceLevel;
    }
    if (digBonus > 0) {
        ++snapshot.digEnhanceLevel;
    }
    if (durabilityBonus > 0) {
        ++snapshot.durabilityEnhanceLevel;
    }
    snapshot.attackBonus += attackBonus;
    snapshot.digBonus += digBonus;
    snapshot.durabilityBonus += durabilityBonus;
    if (durabilityBonus > 0 && snapshot.maxDurability >= 0) {
        snapshot.maxDurability += durabilityBonus;
        snapshot.currentDurability = std::min(snapshot.maxDurability, std::max(0, snapshot.currentDurability + durabilityBonus));
    }
    return snapshot;
}

ProcessingResultSnapshot processingResetSnapshot(ProcessingResultSnapshot snapshot)
{
    snapshot.enhanceLevel = 0;
    snapshot.attackEnhanceLevel = 0;
    snapshot.digEnhanceLevel = 0;
    snapshot.durabilityEnhanceLevel = 0;
    snapshot.attackBonus = 0;
    snapshot.digBonus = 0;
    snapshot.durabilityBonus = 0;
    return snapshot;
}

ProcessingResultSnapshot processingShapeSnapshot(ProcessingResultSnapshot snapshot, double weightMultiplier, double sizeMultiplier)
{
    snapshot.stackCount = 1;
    snapshot.stackSource = false;
    snapshot.weightModifier = std::clamp(snapshot.weightModifier * weightMultiplier, 0.25, 4.0);
    snapshot.sizeModifier = std::clamp(snapshot.sizeModifier * sizeMultiplier, 0.50, 3.0);
    return snapshot;
}

std::string processingChangeLine(std::string_view label, int beforeValue, int afterValue)
{
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "%s: %d → %d", std::string(label).c_str(), beforeValue, afterValue);
    return buffer;
}

std::string processingSignedChangeLine(std::string_view label, int beforeValue, int afterValue)
{
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "%s: +%d → +%d", std::string(label).c_str(), beforeValue, afterValue);
    return buffer;
}

std::string processingDurabilityChangeLine(std::string_view label, int beforeValue, int afterValue)
{
    if (beforeValue < 0 || afterValue < 0) {
        return std::string(label) + ": 壊れない";
    }
    return processingChangeLine(label, beforeValue, afterValue);
}

std::string processingPercentChangeLine(std::string_view label, double beforeValue, double afterValue)
{
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "%s: %.0f%% → %.0f%%",
        std::string(label).c_str(),
        beforeValue * 100.0,
        afterValue * 100.0);
    return buffer;
}

struct ProcessingPreviewRow {
    std::string label;
    std::string beforeValue;
    std::string afterValue;
};

std::string formatProcessingInt(int value)
{
    return std::to_string(value);
}

std::string formatProcessingDurability(int current, int maximum)
{
    if (maximum < 0) {
        return "壊れない";
    }
    return std::to_string(std::max(0, current)) + "/" + std::to_string(maximum);
}

std::string formatProcessingMaxDurability(int maximum)
{
    if (maximum < 0) {
        return "壊れない";
    }
    return std::to_string(maximum);
}

std::string formatProcessingPercent(double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", value * 100.0);
    return buffer;
}

std::string processingInlineItemName(const ProcessingResultSnapshot& snapshot)
{
    std::string text = inlineItemTag(snapshot.objectId);
    if (!text.empty()) {
        text += " ";
    }
    text += snapshot.name;
    return text;
}

void drawProcessingPreviewRow(Renderer& renderer, UiRect content, float& y, const ProcessingPreviewRow& row)
{
    constexpr float ValueX = 184.0f;
    renderer.drawText({content.pos.x, y}, row.label, ui::TextMuted, 2);
    Vec2 valuePos{content.pos.x + ValueX, y};
    drawUiTextRun(renderer, valuePos, row.beforeValue, ui::Text);
    drawUiTextRun(renderer, valuePos, "→", ui::TextMuted);
    drawUiTextRun(renderer, valuePos, row.afterValue, ConfirmAfterValueColor);
    y += 31.0f;
}

std::string processingRepairDurabilityLine(const ProcessingResultSnapshot& before, const ProcessingResultSnapshot& after)
{
    if (before.maxDurability < 0 || after.maxDurability < 0) {
        return "耐久力: 壊れない";
    }
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "耐久力: %d/%d → %d/%d",
        before.currentDurability,
        before.maxDurability,
        after.currentDurability,
        after.maxDurability);
    return buffer;
}

std::vector<std::string> processingRepairResultLines(
    const ProcessingResultSnapshot& before,
    const ProcessingResultSnapshot& after)
{
    std::vector<std::string> lines;
    lines.push_back(before.name + "を修理しました");
    if (before.isBroken && !after.isBroken) {
        lines.push_back("状態: 破損 → 通常");
    }
    lines.push_back(processingRepairDurabilityLine(before, after));
    return lines;
}

std::vector<std::string> processingEnhanceResultLines(
    const ProcessingResultSnapshot& before,
    const ProcessingResultSnapshot& after,
    bool attackMode,
    bool digMode,
    bool durabilityMode)
{
    std::vector<std::string> lines;
    if (before.stackSource && before.stackCount > 1) {
        lines.push_back(before.name + "1個を強化しました");
    } else {
        lines.push_back(before.name + "を強化しました");
    }
    lines.push_back(processingChangeLine("強化Lv", before.enhanceLevel, after.enhanceLevel));
    if (attackMode) {
        lines.push_back(processingSignedChangeLine("攻撃力", before.attackBonus, after.attackBonus));
    } else if (digMode) {
        lines.push_back(processingSignedChangeLine("抑制力", before.digBonus, after.digBonus));
    } else if (durabilityMode) {
        lines.push_back(processingDurabilityChangeLine("最大耐久力", before.maxDurability, after.maxDurability));
    }
    return lines;
}

std::vector<std::string> processingResetResultLines(
    const ProcessingResultSnapshot& before,
    const ProcessingResultSnapshot& after)
{
    std::vector<std::string> lines;
    lines.push_back(before.name + "の強化をリセットしました");
    lines.push_back(processingChangeLine("強化Lv", before.enhanceLevel, after.enhanceLevel));
    lines.push_back(processingSignedChangeLine("攻撃力", before.attackBonus, after.attackBonus));
    lines.push_back(processingSignedChangeLine("抑制力", before.digBonus, after.digBonus));
    lines.push_back(processingSignedChangeLine("耐久補正", before.durabilityBonus, after.durabilityBonus));
    return lines;
}

std::vector<std::string> processingShapeResultLines(
    const ProcessingResultSnapshot& before,
    const ProcessingResultSnapshot& after,
    bool lightMode)
{
    std::vector<std::string> lines;
    const char* verb = lightMode ? "軽量化しました" : "大型化しました";
    if (before.stackSource && before.stackCount > 1) {
        lines.push_back(before.name + "1個を" + verb);
    } else {
        lines.push_back(before.name + "を" + verb);
    }
    lines.push_back(processingPercentChangeLine("重量", before.weightModifier, after.weightModifier));
    lines.push_back(processingPercentChangeLine("大きさ", before.sizeModifier, after.sizeModifier));
    return lines;
}

} // namespace

bool Game::isSellableObject(const ItemData& item) const
{
    return !isStoryObject(item);
}

bool Game::isStoryObject(const ItemData& item) const
{
    return isImportantItem(item);
}

namespace {

constexpr double LightenWeightMultiplier = 0.85;
constexpr double EnlargeWeightMultiplier = 1.15;
constexpr double EnlargeSizeMultiplier = 1.18;
constexpr double SellPriceBaseMultiplier = 0.5;

int processingDiscountCost(int rawCost, int processingUnlockLevel)
{
    double multiplier = 1.0;
    if (processingUnlockLevel >= 5) {
        multiplier = 0.70;
    } else if (processingUnlockLevel >= 4) {
        multiplier = 0.80;
    } else if (processingUnlockLevel >= 2) {
        multiplier = 0.90;
    }
    return std::max(1, static_cast<int>(std::ceil(static_cast<double>(std::max(1, rawCost)) * multiplier)));
}

int processingRarity(const ItemData* item)
{
    return std::clamp(item != nullptr ? item->rarity : 1, 1, 10);
}

int processingEnhanceMoneyCost(int rarity, int nextEnhanceLevel)
{
    constexpr std::array<int, 10> RarityBaseCosts{{80, 100, 120, 145, 170, 200, 230, 265, 305, 350}};
    rarity = std::clamp(rarity, 1, 10);
    nextEnhanceLevel = std::clamp(nextEnhanceLevel, 1, MaxItemEnhanceLevel);
    return RarityBaseCosts[static_cast<std::size_t>(rarity - 1)] + nextEnhanceLevel * 60;
}

int processingEnhanceOreCost(int rarity, int nextEnhanceLevel)
{
    constexpr std::array<int, 10> RarityOreBonus{{0, 1, 1, 2, 2, 3, 3, 4, 5, 6}};
    rarity = std::clamp(rarity, 1, 10);
    nextEnhanceLevel = std::clamp(nextEnhanceLevel, 1, MaxItemEnhanceLevel);
    return nextEnhanceLevel + RarityOreBonus[static_cast<std::size_t>(rarity - 1)];
}

int processingLightenMoneyCost(int rarity)
{
    rarity = std::clamp(rarity, 1, 10);
    return 140 + rarity * 30;
}

int processingLightenOreCost(int rarity)
{
    rarity = std::clamp(rarity, 1, 10);
    return 2 + (rarity + 1) / 2;
}

int processingEnlargeMoneyCost(int rarity)
{
    rarity = std::clamp(rarity, 1, 10);
    return 190 + rarity * 45;
}

int processingEnlargeOreCost(int rarity)
{
    rarity = std::clamp(rarity, 1, 10);
    return 3 + (rarity * 2 + 2) / 3;
}

int processingPowerEnhanceBonus(int rarity, int nextEnhanceLevel)
{
    constexpr std::array<int, 5> RarityOneBonuses{{1, 2, 2, 2, 3}};
    constexpr std::array<int, 5> RarityTenBonuses{{5, 6, 6, 7, 7}};
    rarity = std::clamp(rarity, 1, 10);
    nextEnhanceLevel = std::clamp(nextEnhanceLevel, 1, MaxItemEnhanceLevel);
    const int index = nextEnhanceLevel - 1;
    const double t = static_cast<double>(rarity - 1) / 9.0;
    const double value = static_cast<double>(RarityOneBonuses[static_cast<std::size_t>(index)]) +
        static_cast<double>(RarityTenBonuses[static_cast<std::size_t>(index)] - RarityOneBonuses[static_cast<std::size_t>(index)]) * t;
    return std::max(1, static_cast<int>(std::lround(value)));
}

int processingDurabilityEnhanceBonus(int baseDurability, int nextEnhanceLevel)
{
    if (baseDurability <= 0) {
        return 0;
    }
    constexpr std::array<int, 5> PercentByLevel{{20, 20, 30, 40, 40}};
    nextEnhanceLevel = std::clamp(nextEnhanceLevel, 1, MaxItemEnhanceLevel);
    const int percent = PercentByLevel[static_cast<std::size_t>(nextEnhanceLevel - 1)];
    return std::max(0, static_cast<int>(std::lround(static_cast<double>(baseDurability) * static_cast<double>(percent) / 100.0)));
}

bool processingDurabilityEnhanceAvailable(const ItemData* item)
{
    return item != nullptr && processingDurabilityEnhanceBonus(item->durability, 1) > 0;
}

bool processingDurabilityEnhanceAvailable(int baseDurability)
{
    return processingDurabilityEnhanceBonus(baseDurability, 1) > 0;
}

struct ProcessingEnhanceBonuses {
    int attack = 0;
    int dig = 0;
    int durability = 0;
};

ProcessingEnhanceBonuses processingEnhanceBonuses(
    bool attackMode,
    bool digMode,
    bool durabilityMode,
    int rarity,
    int baseDurability,
    int currentModeEnhanceLevel)
{
    ProcessingEnhanceBonuses bonuses{};
    const int nextEnhanceLevel = std::clamp(currentModeEnhanceLevel + 1, 1, MaxItemEnhanceLevel);
    if (attackMode) {
        bonuses.attack = processingPowerEnhanceBonus(rarity, nextEnhanceLevel);
    } else if (digMode) {
        bonuses.dig = processingPowerEnhanceBonus(rarity, nextEnhanceLevel);
    } else if (durabilityMode) {
        bonuses.durability = processingDurabilityEnhanceBonus(baseDurability, nextEnhanceLevel);
    }
    return bonuses;
}

bool isTreasureObject(const ItemData& item)
{
    return item.category == "\xE5\xAE\x9D";
}

double itemInstanceSellValueMultiplier(
    int currentDurability,
    int maxDurability,
    int enhanceLevel,
    int attackBonus,
    int digBonus,
    int durabilityBonus,
    double weightModifier,
    double sizeModifier,
    bool broken)
{
    double multiplier = 1.0;
    multiplier += static_cast<double>(std::max(0, enhanceLevel)) * 0.10;
    multiplier += static_cast<double>(std::max(0, attackBonus) + std::max(0, digBonus)) * 0.035;
    multiplier += static_cast<double>(std::max(0, durabilityBonus)) * 0.018;

    if (weightModifier < 0.999) {
        multiplier += std::min(0.35, (1.0 - weightModifier) * 1.5);
    }
    if (sizeModifier > 1.001) {
        multiplier += std::min(0.35, (sizeModifier - 1.0) * 1.2);
    }

    if (broken || currentDurability == 0) {
        multiplier *= 0.45;
    } else if (maxDurability > 0 && currentDurability >= 0) {
        const double durabilityRatio = std::clamp(
            static_cast<double>(currentDurability) / static_cast<double>(maxDurability),
            0.0,
            1.0);
        multiplier *= 0.75 + durabilityRatio * 0.25;
    }

    return std::max(0.1, multiplier);
}

} // namespace

int Game::sellPrice(const ItemData& item, const ItemInstance* instance) const
{
    double multiplier = 1.0;
    if (merchantUpgradeLevel_ >= 6) {
        multiplier = 1.2;
    } else if (merchantUpgradeLevel_ >= 3) {
        multiplier = 1.1;
    }
    if (isHighValueBuyObject(item)) {
        multiplier *= merchantUpgradeLevel_ >= 6 ? 1.8 : 1.5;
    }
    if (instance != nullptr) {
        multiplier *= itemInstanceSellValueMultiplier(
            instance->currentDurability,
            instance->maxDurability,
            instance->enhanceLevel,
            instance->attackBonus,
            instance->digBonus,
            instance->durabilityBonus,
            instance->weightModifier,
            instance->sizeModifier,
            instance->isBroken);
    }
    const double totalMultiplier = SellPriceBaseMultiplier * multiplier;
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(item.price) * totalMultiplier)));
}

int Game::sellPrice(const ItemData& item, const SpellRingItem* ringItem) const
{
    if (ringItem == nullptr) {
        return sellPrice(item, static_cast<const ItemInstance*>(nullptr));
    }
    double multiplier = 1.0;
    if (merchantUpgradeLevel_ >= 6) {
        multiplier = 1.2;
    } else if (merchantUpgradeLevel_ >= 3) {
        multiplier = 1.1;
    }
    if (isHighValueBuyObject(item)) {
        multiplier *= merchantUpgradeLevel_ >= 6 ? 1.8 : 1.5;
    }
    multiplier *= itemInstanceSellValueMultiplier(
        ringItem->durability,
        ringItem->maxDurability,
        ringItem->enhanceLevel,
        ringItem->attackBonus,
        ringItem->digBonus,
        ringItem->durabilityBonus,
        ringItem->weightModifier,
        ringItem->sizeModifier,
        ringItem->broken());
    const double totalMultiplier = SellPriceBaseMultiplier * multiplier;
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(item.price) * totalMultiplier)));
}

bool Game::isHighValueBuyObject(const ItemData& item) const
{
    if (merchantUpgradeLevel_ < 4 || !isTreasureObject(item)) {
        return false;
    }
    return std::find(highValueBuyObjectIds_.begin(), highValueBuyObjectIds_.end(), item.id) != highValueBuyObjectIds_.end();
}

bool Game::merchantProductCanFit(const ItemData* item) const
{
    if (item == nullptr) {
        return false;
    }
    const auto& stacks = inventory_.objectStacks();
    const bool existingStack = std::any_of(stacks.begin(), stacks.end(), [&](const InventoryObjectStack& stack) {
        return stack.objectId == item->id;
    });
    return existingStack || backpackUsedSlots() < inventory_.screenSlotCount();
}

bool Game::canBuyMerchantProduct(const MerchantProduct& product) const
{
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    return product.quantity > 0 && item != nullptr && money_ >= product.price && merchantProductCanFit(item);
}

void Game::refreshHighValueBuyObjects(bool force)
{
    if (merchantUpgradeLevel_ < 4) {
        highValueBuyObjectIds_.clear();
        return;
    }
    if (!force && !highValueBuyObjectIds_.empty()) {
        return;
    }

    std::vector<const ItemData*> candidates;
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const ItemData* item = objectCatalog_.registry.findById(object.id);
        if (item == nullptr || item->id.empty() || item->price <= 0 || !isTreasureObject(*item) || !isSellableObject(*item)) {
            continue;
        }
        candidates.push_back(item);
    }

    highValueBuyObjectIds_.clear();
    if (candidates.empty()) {
        return;
    }

    std::mt19937& rng = lootRuntimeRng();
    std::shuffle(candidates.begin(), candidates.end(), rng);
    const int pickCount = std::min(4, static_cast<int>(candidates.size()));
    highValueBuyObjectIds_.reserve(static_cast<std::size_t>(pickCount));
    for (int i = 0; i < pickCount; ++i) {
        highValueBuyObjectIds_.push_back(candidates[static_cast<std::size_t>(i)]->id);
    }
}

std::vector<Game::SellableEntry> Game::sellableObjects() const
{
    std::vector<SellableEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(i)];
        if (stack.count <= 0) {
            continue;
        }
        SellableEntry entry{SellableKind::Stack, i};
        entry.price = sellPrice(stack.item);
        entry.sellable = true;
        entries.push_back(std::move(entry));
    }
    const auto& instances = inventory_.objectInstances();
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(i)];
        SellableEntry entry{SellableKind::Instance, i};
        entry.price = sellPrice(instance.item, &instance.instance);
        entry.sellable = !inventory_.isStaffEquipped(instance.instance.instanceId) &&
            !instance.instance.protectionEnabled;
        if (inventory_.isStaffEquipped(instance.instance.instanceId)) {
            entry.blockedReason = "装備中";
        } else if (!entry.sellable) {
            entry.blockedReason = "保護中";
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

void Game::refreshMerchantStock(bool force)
{
    refreshHighValueBuyObjects(force);
    if (!force && !merchantStock_.empty()) {
        return;
    }

    std::vector<const ItemData*> candidates;
    const int maxRarity = merchantUpgradeLevel_ >= 7 ? 10 :
        (merchantUpgradeLevel_ >= 5 ? 7 :
            (merchantUpgradeLevel_ >= 4 ? 5 :
                (merchantUpgradeLevel_ >= 2 ? 4 : 2)));
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const ItemData* item = objectCatalog_.registry.findById(object.id);
        if (item == nullptr || item->id.empty() || item->price <= 0 || isStoryObject(*item)) {
            continue;
        }
        if (isTreasureObject(*item)) {
            continue;
        }
        if (item->rarity > maxRarity) {
            continue;
        }
        const bool requiredCategory = merchantStockGroupForItem(*item).has_value();
        const bool advancedEquipmentCategory = item->category == "\xE9\xAD\x94\xE5\xB0\x8E\xE6\x9B\xB8";
        const bool basicTag = std::any_of(item->tags.begin(), item->tags.end(), [](const std::string& tag) {
            return tag == "consumable" || tag == "potion" || tag == "food";
        });
        if (requiredCategory || basicTag || (merchantUpgradeLevel_ >= 4 && advancedEquipmentCategory)) {
            candidates.push_back(item);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const ItemData* left, const ItemData* right) {
        if (left->price != right->price) {
            return left->price < right->price;
        }
        return left->id < right->id;
    });

    merchantStock_.clear();
    if (candidates.empty()) {
        return;
    }

    ++merchantStockVersion_;
    const int stockCount = merchantStockCountForLevel(merchantUpgradeLevel_);
    const int minimumPerRequiredGroup = std::max(1, (stockCount + 9) / 10);
    std::mt19937& rng = lootRuntimeRng();
    std::uniform_int_distribution<int> quantityDistribution(1, 5);
    const auto addProduct = [&](const ItemData& item) {
        const int quantity = item.rarity >= 6 ? std::uniform_int_distribution<int>(1, 2)(rng) : quantityDistribution(rng);
        merchantStock_.push_back(MerchantProduct{item.id, std::max(1, item.price), quantity});
    };

    std::vector<const ItemData*> uniquePool = candidates;
    for (MerchantStockGroup group : RequiredMerchantStockGroups) {
        for (int i = 0; i < minimumPerRequiredGroup && static_cast<int>(merchantStock_.size()) < stockCount; ++i) {
            const ItemData* item = pickMerchantCandidate(uniquePool, merchantUpgradeLevel_, rng, group);
            if (item == nullptr) {
                std::vector<const ItemData*> duplicatePool = candidates;
                item = pickMerchantCandidate(duplicatePool, merchantUpgradeLevel_, rng, group);
            }
            if (item == nullptr) {
                logError("[warning] Merchant stock: no Objects candidates for required group \"" +
                    std::string(merchantStockGroupName(group)) + "\"");
                break;
            }
            addProduct(*item);
        }
    }

    while (static_cast<int>(merchantStock_.size()) < stockCount) {
        if (uniquePool.empty()) {
            uniquePool = candidates;
        }
        const ItemData* item = pickMerchantCandidate(uniquePool, merchantUpgradeLevel_, rng);
        if (item == nullptr) {
            break;
        }
        addProduct(*item);
    }
}

void Game::sellMerchantEntry(int index, int count)
{
    const std::vector<SellableEntry> sellable = sellableObjects();
    if (index < 0 || index >= static_cast<int>(sellable.size())) {
        baseStatus_ = "売却対象がありません";
        return;
    }

    const SellableEntry entry = sellable[static_cast<std::size_t>(index)];
    if (!entry.sellable) {
        baseStatus_ = entry.blockedReason.empty() ? "売れません" : entry.blockedReason;
        return;
    }

    bool sold = false;
    int soldCount = 1;
    if (entry.kind == SellableKind::Stack) {
        const auto& stacks = inventory_.objectStacks();
        if (entry.index < 0 || entry.index >= static_cast<int>(stacks.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(entry.index)];
        soldCount = count <= 0 ? stack.count : std::min(count, stack.count);
        sold = inventory_.removeObjectItemCount(stack.objectId, soldCount);
    } else {
        const auto& instances = inventory_.objectInstances();
        if (entry.index < 0 || entry.index >= static_cast<int>(instances.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(entry.index)];
        sold = inventory_.removeObjectInstance(instance.instance.instanceId);
    }

    if (sold) {
        money_ += entry.price * std::max(1, soldCount);
        baseStatus_ = "売却しました";
        baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, static_cast<int>(sellableObjects().size()) - 1));
    }
}

Game::MerchantSellTarget Game::merchantSellTargetForSourceSlot(int source, int slotIndex) const
{
    MerchantSellTarget target{};
    target.slotIndex = slotIndex;
    const int clampedSource = std::clamp(source, 0, BaseItemSourceCount - 1);
    target.source = static_cast<BaseItemSource>(clampedSource);

    if (target.source == BaseItemSource::Backpack) {
        if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
            return target;
        }
        if (inventory_.screenObjectStackAt(slotIndex) != nullptr ||
            inventory_.screenObjectInstanceAt(slotIndex) != nullptr) {
            target.valid = true;
        }
        return target;
    }

    if (target.source == BaseItemSource::Warehouse) {
        const std::optional<StorageEntry> entry = warehouseEntryForPageSlot(slotIndex, baseStorageWarehousePage_);
        if (!entry) {
            return target;
        }
        target.storageEntry = *entry;
        target.warehouseEntry = true;
        target.valid = true;
        return target;
    }

    target.ringIndex = ringIndexFromBaseItemSource(clampedSource);
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return target;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (slotIndex < 0 || slotIndex >= static_cast<int>(ringItems.size())) {
        return target;
    }
    target.ringItemIndex = slotIndex;
    target.valid = true;
    return target;
}

Game::MerchantSellTarget Game::merchantSellTargetForScreenSlot(int slotIndex) const
{
    return merchantSellTargetForSourceSlot(baseMerchantSellSource_, slotIndex);
}

bool Game::merchantSellTargetAvailable(MerchantSellTarget target) const
{
    if (!target.valid) {
        return false;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            return stack->count > 0 && isSellableObject(stack->item);
        }
        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            return !inventory_.isStaffEquipped(instance->instance.instanceId) &&
                !instance->instance.protectionEnabled &&
                isSellableObject(instance->item);
        }
        return false;
    }

    if (target.source == BaseItemSource::Warehouse) {
        const ItemData* item = storageEntryItem(target.storageEntry, true);
        if (item == nullptr || !isSellableObject(*item)) {
            return false;
        }
        if (target.storageEntry.kind == StorageEntryKind::Stack) {
            return storageEntryStackCount(target.storageEntry, true) > 0;
        }
        const ItemInstance* instance = storageEntryInstance(target.storageEntry, true);
        return instance != nullptr && !instance->protectionEnabled;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return false;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return false;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    if (ringItem.protectionEnabled) {
        return false;
    }
    const ItemData* item = objectForRingItem(objectCatalog_, ringItem);
    return item != nullptr && isSellableObject(*item);
}

int Game::merchantSellTargetPrice(MerchantSellTarget target) const
{
    if (!target.valid) {
        return 0;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            return sellPrice(stack->item);
        }
        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            return sellPrice(instance->item, &instance->instance);
        }
        return 0;
    }

    if (target.source == BaseItemSource::Warehouse) {
        const ItemData* item = storageEntryItem(target.storageEntry, true);
        return item != nullptr ? sellPrice(*item, storageEntryInstance(target.storageEntry, true)) : 0;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return 0;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return 0;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    const ItemData* item = objectForRingItem(objectCatalog_, ringItem);
    return item != nullptr ? sellPrice(*item, &ringItem) : 0;
}

void Game::sellMerchantTarget(MerchantSellTarget target, int count)
{
    if (!target.valid) {
        baseStatus_ = "売却対象がありません";
        return;
    }
    if (!merchantSellTargetAvailable(target)) {
        baseStatus_ = "売れません";
        if (target.source == BaseItemSource::Backpack) {
            if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                    baseStatus_ = "装備中";
                } else if (instance->instance.protectionEnabled) {
                    baseStatus_ = "保護中";
                }
            }
        } else if (target.source == BaseItemSource::Warehouse) {
            if (const ItemInstance* instance = storageEntryInstance(target.storageEntry, true)) {
                if (instance->protectionEnabled) {
                    baseStatus_ = "保護中";
                }
            }
        } else if (target.ringIndex >= 0 && target.ringIndex < SpellRingCount) {
            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
            if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size()) &&
                ringItems[static_cast<std::size_t>(target.ringItemIndex)].protectionEnabled) {
                baseStatus_ = "保護中";
            }
        }
        return;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            const int soldCount = count <= 0 ? stack->count : std::min(count, stack->count);
            const std::string objectId = stack->objectId;
            const int price = sellPrice(stack->item) * std::max(1, soldCount);
            if (inventory_.removeObjectItemCount(objectId, soldCount)) {
                money_ += price;
                baseStatus_ = "売却しました";
            }
            return;
        }

        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            const std::string instanceId = instance->instance.instanceId;
            const int price = sellPrice(instance->item, &instance->instance);
            if (inventory_.removeObjectInstance(instanceId)) {
                money_ += price;
                baseStatus_ = "売却しました";
            }
            return;
        }

        baseStatus_ = "売却対象がありません";
        return;
    }

    if (target.source == BaseItemSource::Warehouse) {
        if (target.storageEntry.kind == StorageEntryKind::Stack) {
            if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectStacks_.size())) {
                baseStatus_ = "売却対象がありません";
                return;
            }
            InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(target.storageEntry.index)];
            const int soldCount = count <= 0 ? stack.count : std::min(count, stack.count);
            money_ += sellPrice(stack.item) * std::max(1, soldCount);
            stack.count -= soldCount;
            if (stack.count <= 0) {
                removeWarehouseDisplaySlotAtEntryIndex(target.storageEntry.index);
                warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + target.storageEntry.index);
            }
            baseSellSelection_ = std::clamp(baseSellSelection_, 0, StoragePaneSlotCount - 1);
            baseStatus_ = "売却しました";
            return;
        }

        if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectInstances_.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(target.storageEntry.index)];
        money_ += sellPrice(instance.item, &instance.instance);
        removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index);
        warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + target.storageEntry.index);
        baseSellSelection_ = std::clamp(baseSellSelection_, 0, StoragePaneSlotCount - 1);
        baseStatus_ = "売却しました";
        return;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        baseStatus_ = "売却対象がありません";
        return;
    }
    std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        baseStatus_ = "売却対象がありません";
        return;
    }
    const ItemData* item = objectForRingItem(objectCatalog_, ringItems[static_cast<std::size_t>(target.ringItemIndex)]);
    if (item == nullptr) {
        baseStatus_ = "売れません";
        return;
    }

    money_ += sellPrice(*item, &ringItems[static_cast<std::size_t>(target.ringItemIndex)]);
    ringItems.erase(ringItems.begin() + target.ringItemIndex);
    refreshOrbitEffects();
    baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, static_cast<int>(ringItems.size()) - 1));
    baseStatus_ = "売却しました";
}

void Game::sellMerchantScreenSlot(int slotIndex, int count)
{
    sellMerchantTarget(merchantSellTargetForSourceSlot(0, slotIndex), count);
}

void Game::buyMerchantProduct(int index)
{
    refreshMerchantStock(false);
    if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
        baseStatus_ = "購入できる商品がありません";
        return;
    }

    MerchantProduct& product = merchantStock_[static_cast<std::size_t>(index)];
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    if (item == nullptr) {
        baseStatus_ = "商品データがありません";
        return;
    }
    if (product.quantity <= 0) {
        baseStatus_ = "品切れです";
        return;
    }
    if (money_ < product.price) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (!merchantProductCanFit(item)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    InventoryAddResult addResult;
    if (!inventory_.addObjectItem(objectCatalog_, product.objectId, &addResult)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    money_ -= product.price;
    --product.quantity;
    recordObjectObtainedForFirstNotice(
        product.objectId,
        addResult.instanceId,
        addResult.kind == InventoryAddKind::Instance && !addResult.instanceId.empty(),
        basePlayerPosition_);
    baseStatus_ = product.quantity <= 0 ? "購入しました（品切れ）" : "購入しました";
}

std::vector<Game::StorageEntry> Game::processingEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

std::optional<Game::StorageEntry> Game::processingEntryForScreenSlot(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
        return std::nullopt;
    }
    if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slotIndex)) {
        const auto& stacks = inventory_.objectStacks();
        for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
            if (stacks[static_cast<std::size_t>(i)].objectId == stack->objectId) {
                return StorageEntry{StorageEntryKind::Stack, i};
            }
        }
    }
    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slotIndex)) {
        const auto& instances = inventory_.objectInstances();
        for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
            if (instances[static_cast<std::size_t>(i)].instance.instanceId == instance->instance.instanceId) {
                return StorageEntry{StorageEntryKind::Instance, i};
            }
        }
    }
    return std::nullopt;
}

std::optional<Game::StorageEntry> Game::warehouseEntryForPageSlot(int slotIndex, int page) const
{
    return warehouseEntryForPageSlot(slotIndex, page, StoragePaneSlotCount);
}

std::optional<Game::StorageEntry> Game::warehouseEntryForPageSlot(int slotIndex, int page, int slotsPerPage) const
{
    const int pageSize = std::max(1, slotsPerPage);
    if (slotIndex < 0 || slotIndex >= pageSize) {
        return std::nullopt;
    }

    const std::vector<StorageEntry> entries = warehouseStorageEntries();
    const int pageCount = std::max(1, (warehouseCapacity() + pageSize - 1) / pageSize);
    const int warehousePage = std::clamp(page, 0, pageCount - 1);
    const int entryIndex = warehouseEntryIndexAtStorageSlot(warehousePage * pageSize + slotIndex);
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        return std::nullopt;
    }
    return entries[static_cast<std::size_t>(entryIndex)];
}

InventoryUiEntryView Game::storageEntryView(StorageEntry entry, bool warehouseEntry) const
{
    InventoryUiEntryView view{};
    view.item = storageEntryItem(entry, warehouseEntry);
    view.instance = storageEntryInstance(entry, warehouseEntry);
    view.stackCount = storageEntryStackCount(entry, warehouseEntry);
    view.equipped = !warehouseEntry &&
        view.instance != nullptr &&
        inventory_.isStaffEquipped(view.instance->instanceId);
    return view;
}

Game::ProcessingTarget Game::processingTargetForScreenSlot(int slotIndex) const
{
    ProcessingTarget target{};
    target.slotIndex = slotIndex;
    if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
        return target;
    }

    const int source = std::clamp(baseProcessingSource_, 0, BaseProcessingSourceCount - 1);
    target.source = static_cast<BaseItemSource>(source);
    if (target.source == BaseItemSource::Backpack) {
        const std::optional<StorageEntry> entry = processingEntryForScreenSlot(slotIndex);
        if (!entry) {
            return target;
        }
        target.backpackEntry = *entry;
        target.valid = true;
        return target;
    }

    if (target.source == BaseItemSource::Warehouse) {
        const std::optional<StorageEntry> entry = warehouseEntryForPageSlot(slotIndex, baseStorageWarehousePage_);
        if (!entry) {
            return target;
        }
        target.backpackEntry = *entry;
        target.warehouseEntry = true;
        target.valid = true;
        return target;
    }

    target.ringIndex = ringIndexFromBaseItemSource(source);
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return target;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (slotIndex < 0 || slotIndex >= static_cast<int>(ringItems.size())) {
        return target;
    }
    target.ringItemIndex = slotIndex;
    target.valid = true;
    return target;
}

const char* Game::processingModeName(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Repair: return "修理";
    case ProcessingMode::Attack: return "攻撃力強化";
    case ProcessingMode::Dig: return "掘削力強化";
    case ProcessingMode::Durability: return "耐久力強化";
    case ProcessingMode::ResetEnhancement: return "強化リセット";
    case ProcessingMode::Lighten: return "軽量化";
    case ProcessingMode::Enlarge: return "大型化";
    }
    return "";
}

const char* Game::processingActionName(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Repair:
        return "修理する";
    case ProcessingMode::ResetEnhancement:
        return "リセットする";
    case ProcessingMode::Lighten:
    case ProcessingMode::Enlarge:
        return "加工する";
    case ProcessingMode::Attack:
    case ProcessingMode::Dig:
    case ProcessingMode::Durability:
        return "強化する";
    }
    return "実行する";
}

bool Game::processingModeUnlocked(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Lighten:
        return processingUnlockLevel_ >= 1;
    case ProcessingMode::Enlarge:
        return processingUnlockLevel_ >= 3;
    case ProcessingMode::Repair:
    case ProcessingMode::Attack:
    case ProcessingMode::Dig:
    case ProcessingMode::Durability:
    case ProcessingMode::ResetEnhancement:
        return true;
    }
    return true;
}

bool Game::processingEntryAvailable(StorageEntry entry, bool warehouseEntry) const
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    return processingEntryAvailable(entry, mode, warehouseEntry);
}

bool Game::processingEntryAvailable(StorageEntry entry, ProcessingMode mode, bool warehouseEntry) const
{
    if (!processingModeUnlocked(mode)) {
        return false;
    }
    if (entry.kind == StorageEntryKind::Stack) {
        if (mode == ProcessingMode::Durability) {
            const ItemData* item = storageEntryItem(entry, warehouseEntry);
            return processingDurabilityEnhanceAvailable(item);
        }
        return mode != ProcessingMode::Repair && mode != ProcessingMode::ResetEnhancement;
    }
    const ItemInstance* instance = storageEntryInstance(entry, warehouseEntry);
    if (instance == nullptr) {
        return false;
    }
    if (mode == ProcessingMode::Repair) {
        return instance->maxDurability >= 0 && (instance->isBroken || instance->currentDurability < instance->maxDurability);
    }
    if (instance->isBroken) {
        return false;
    }
    if (mode == ProcessingMode::ResetEnhancement) {
        return instance->enhanceLevel > 0 ||
            instance->attackEnhanceLevel > 0 ||
            instance->digEnhanceLevel > 0 ||
            instance->durabilityEnhanceLevel > 0 ||
            instance->attackBonus != 0 ||
            instance->digBonus != 0 ||
            instance->durabilityBonus != 0;
    }
    if (mode == ProcessingMode::Lighten) {
        return instance->weightModifier >= 0.999;
    }
    if (mode == ProcessingMode::Enlarge) {
        return instance->sizeModifier <= 1.001;
    }
    if (mode == ProcessingMode::Durability) {
        const ItemData* item = storageEntryItem(entry, warehouseEntry);
        return instance->durabilityEnhanceLevel < MaxItemEnhanceLevel && processingDurabilityEnhanceAvailable(item);
    }
    if (mode == ProcessingMode::Attack) {
        return instance->attackEnhanceLevel < MaxItemEnhanceLevel;
    }
    if (mode == ProcessingMode::Dig) {
        return instance->digEnhanceLevel < MaxItemEnhanceLevel;
    }
    return false;
}

bool Game::processingScreenSlotAvailable(int slotIndex) const
{
    return processingTargetAvailable(processingTargetForScreenSlot(slotIndex));
}

bool Game::processingTargetAvailable(ProcessingTarget target) const
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    return processingTargetAvailable(target, mode);
}

bool Game::processingTargetAvailable(ProcessingTarget target, ProcessingMode mode) const
{
    if (!target.valid) {
        return false;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        return processingEntryAvailable(target.backpackEntry, mode, target.warehouseEntry);
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return false;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return false;
    }

    if (!processingModeUnlocked(mode)) {
        return false;
    }
    const SpellRingItem& item = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    if (mode == ProcessingMode::Repair) {
        return item.maxDurability >= 0 && (item.broken() || item.durability < item.maxDurability);
    }
    if (item.broken()) {
        return false;
    }
    if (mode == ProcessingMode::ResetEnhancement) {
        return item.enhanceLevel > 0 ||
            item.attackEnhanceLevel > 0 ||
            item.digEnhanceLevel > 0 ||
            item.durabilityEnhanceLevel > 0 ||
            item.attackBonus != 0 ||
            item.digBonus != 0 ||
            item.durabilityBonus != 0;
    }
    if (mode == ProcessingMode::Lighten) {
        return item.weightModifier >= 0.999;
    }
    if (mode == ProcessingMode::Enlarge) {
        return item.sizeModifier <= 1.001;
    }
    if (mode == ProcessingMode::Durability) {
        const ItemData* object = objectCatalog_.registry.findById(item.objectId);
        return item.durabilityEnhanceLevel < MaxItemEnhanceLevel && processingDurabilityEnhanceAvailable(object);
    }
    if (mode == ProcessingMode::Attack) {
        return item.attackEnhanceLevel < MaxItemEnhanceLevel;
    }
    if (mode == ProcessingMode::Dig) {
        return item.digEnhanceLevel < MaxItemEnhanceLevel;
    }
    return false;
}

bool Game::processingTargetHasAvailableCommand(ProcessingTarget target) const
{
    for (int i = 0; i < BaseProcessingModeCount; ++i) {
        const ProcessingMode mode = static_cast<ProcessingMode>(i);
        if (mode != ProcessingMode::Repair && processingTargetAvailable(target, mode)) {
            return true;
        }
    }
    return false;
}

bool Game::processingCommandExecutable(ProcessingTarget target, ProcessingMode mode) const
{
    if (!processingTargetAvailable(target, mode)) {
        return false;
    }
    return money_ >= processingMoneyCost(target, mode) &&
        inventory_.materialCount(MaterialType::EnhancementOre) >= processingOreCost(target, mode);
}

int Game::processingMoneyCost(StorageEntry entry, ProcessingMode mode, bool warehouseEntry) const
{
    const ItemData* item = storageEntryItem(entry, warehouseEntry);
    const int basePrice = std::max(1, item != nullptr ? item->price : 0);
    const int rarity = processingRarity(item);
    const auto discountCost = [this](int rawCost) {
        return processingDiscountCost(rawCost, processingUnlockLevel_);
    };
    const auto facilityCost = [this, mode](int cost) {
        return roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan && mode != ProcessingMode::Repair
            ? roguelikeAdjustedFacilityMoneyCost(cost)
            : cost;
    };
    if (mode == ProcessingMode::Repair) {
        const ItemInstance* instance = storageEntryInstance(entry, warehouseEntry);
        if (instance == nullptr || instance->maxDurability <= 0) {
            return 0;
        }
        if (instance->isBroken) {
            return discountCost(static_cast<int>(std::ceil(static_cast<double>(basePrice) * 0.6)));
        }
        const int missing = std::max(0, instance->maxDurability - instance->currentDurability);
        if (missing <= 0) {
            return 0;
        }
        const double ratio = static_cast<double>(missing) / static_cast<double>(instance->maxDurability);
        return discountCost(static_cast<int>(std::ceil(static_cast<double>(basePrice) * ratio * 0.4)));
    }
    if (mode == ProcessingMode::ResetEnhancement) {
        return facilityCost(discountCost(std::max(20, basePrice / 4)));
    }
    if (mode == ProcessingMode::Lighten) {
        return facilityCost(discountCost(processingLightenMoneyCost(rarity)));
    }
    if (mode == ProcessingMode::Enlarge) {
        return facilityCost(discountCost(processingEnlargeMoneyCost(rarity)));
    }

    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, warehouseEntry)) {
        if (mode == ProcessingMode::Attack) {
            enhanceLevel = instance->attackEnhanceLevel;
        } else if (mode == ProcessingMode::Dig) {
            enhanceLevel = instance->digEnhanceLevel;
        } else if (mode == ProcessingMode::Durability) {
            enhanceLevel = instance->durabilityEnhanceLevel;
        }
    }
    return facilityCost(discountCost(processingEnhanceMoneyCost(rarity, enhanceLevel + 1)));
}

int Game::processingOreCost(StorageEntry entry, ProcessingMode mode, bool warehouseEntry) const
{
    const ItemData* item = storageEntryItem(entry, warehouseEntry);
    const int rarity = processingRarity(item);
    const auto facilityCost = [this, mode](int cost) {
        return roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan && mode != ProcessingMode::Repair
            ? roguelikeAdjustedFacilityMaterialCost(cost)
            : cost;
    };
    if (mode == ProcessingMode::Repair || mode == ProcessingMode::ResetEnhancement) {
        return 0;
    }
    if (mode == ProcessingMode::Lighten) {
        return facilityCost(processingLightenOreCost(rarity));
    }
    if (mode == ProcessingMode::Enlarge) {
        return facilityCost(processingEnlargeOreCost(rarity));
    }
    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, warehouseEntry)) {
        if (mode == ProcessingMode::Attack) {
            enhanceLevel = instance->attackEnhanceLevel;
        } else if (mode == ProcessingMode::Dig) {
            enhanceLevel = instance->digEnhanceLevel;
        } else if (mode == ProcessingMode::Durability) {
            enhanceLevel = instance->durabilityEnhanceLevel;
        }
    }
    return facilityCost(processingEnhanceOreCost(rarity, enhanceLevel + 1));
}

int Game::processingMoneyCost(ProcessingTarget target, ProcessingMode mode) const
{
    if (!target.valid) {
        return 0;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        return processingMoneyCost(target.backpackEntry, mode, target.warehouseEntry);
    }

    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return 0;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    const ItemData* item = objectForRingItem(objectCatalog_, ringItem);
    const int basePrice = std::max(1, item != nullptr ? item->price : 0);
    const int rarity = processingRarity(item);
    const auto discountCost = [this](int rawCost) {
        return processingDiscountCost(rawCost, processingUnlockLevel_);
    };
    const auto facilityCost = [this, mode](int cost) {
        return roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan && mode != ProcessingMode::Repair
            ? roguelikeAdjustedFacilityMoneyCost(cost)
            : cost;
    };
    if (mode == ProcessingMode::Repair) {
        if (ringItem.maxDurability <= 0) {
            return 0;
        }
        if (ringItem.broken()) {
            return discountCost(static_cast<int>(std::ceil(static_cast<double>(basePrice) * 0.6)));
        }
        const int missing = std::max(0, ringItem.maxDurability - ringItem.durability);
        if (missing <= 0) {
            return 0;
        }
        const double ratio = static_cast<double>(missing) / static_cast<double>(ringItem.maxDurability);
        return discountCost(static_cast<int>(std::ceil(static_cast<double>(basePrice) * ratio * 0.4)));
    }
    if (mode == ProcessingMode::ResetEnhancement) {
        return facilityCost(discountCost(std::max(20, basePrice / 4)));
    }
    if (mode == ProcessingMode::Lighten) {
        return facilityCost(discountCost(processingLightenMoneyCost(rarity)));
    }
    if (mode == ProcessingMode::Enlarge) {
        return facilityCost(discountCost(processingEnlargeMoneyCost(rarity)));
    }

    const int enhanceLevel =
        mode == ProcessingMode::Attack ? ringItem.attackEnhanceLevel :
        mode == ProcessingMode::Dig ? ringItem.digEnhanceLevel :
        mode == ProcessingMode::Durability ? ringItem.durabilityEnhanceLevel :
        ringItem.enhanceLevel;
    return facilityCost(discountCost(processingEnhanceMoneyCost(rarity, enhanceLevel + 1)));
}

int Game::processingOreCost(ProcessingTarget target, ProcessingMode mode) const
{
    if (mode == ProcessingMode::Repair || mode == ProcessingMode::ResetEnhancement || !target.valid) {
        return 0;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        return processingOreCost(target.backpackEntry, mode, target.warehouseEntry);
    }

    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return 0;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    const ItemData* item = objectForRingItem(objectCatalog_, ringItem);
    const int rarity = processingRarity(item);
    const auto facilityCost = [this, mode](int cost) {
        return roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan && mode != ProcessingMode::Repair
            ? roguelikeAdjustedFacilityMaterialCost(cost)
            : cost;
    };
    if (mode == ProcessingMode::Lighten) {
        return facilityCost(processingLightenOreCost(rarity));
    }
    if (mode == ProcessingMode::Enlarge) {
        return facilityCost(processingEnlargeOreCost(rarity));
    }
    const int enhanceLevel =
        mode == ProcessingMode::Attack ? ringItem.attackEnhanceLevel :
        mode == ProcessingMode::Dig ? ringItem.digEnhanceLevel :
        mode == ProcessingMode::Durability ? ringItem.durabilityEnhanceLevel :
        ringItem.enhanceLevel;
    return facilityCost(processingEnhanceOreCost(rarity, enhanceLevel + 1));
}

std::vector<Game::ProcessingMode> Game::processingCommandModes(ProcessingTarget target) const
{
    (void)target;
    constexpr std::array<ProcessingMode, BaseProcessingModeCount - 1> CommandOrder{{
        ProcessingMode::Attack,
        ProcessingMode::Dig,
        ProcessingMode::Durability,
        ProcessingMode::Lighten,
        ProcessingMode::Enlarge,
        ProcessingMode::ResetEnhancement,
    }};
    std::vector<ProcessingMode> modes;
    modes.reserve(CommandOrder.size());
    for (ProcessingMode mode : CommandOrder) {
        if (!processingModeUnlocked(mode)) {
            continue;
        }
        modes.push_back(mode);
    }
    return modes;
}

std::vector<UiCommandMenuItem> Game::processingCommandItems(ProcessingTarget target) const
{
    std::vector<UiCommandMenuItem> items;
    const std::vector<ProcessingMode> modes = processingCommandModes(target);
    items.reserve(modes.size());
    for (ProcessingMode mode : modes) {
        items.push_back(UiCommandMenuItem{
            processingModeName(mode),
            processingTargetAvailable(target, mode),
        });
    }
    return items;
}

int Game::processingBulkRepairTargetCount() const
{
    int count = 0;
    for (const InventoryObjectInstance& entry : inventory_.objectInstances()) {
        const ItemInstance& instance = entry.instance;
        if (instance.maxDurability >= 0 && (instance.isBroken || instance.currentDurability < instance.maxDurability)) {
            ++count;
        }
    }
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
        for (const SpellRingItem& item : ringItems) {
            if (item.maxDurability >= 0 && (item.broken() || item.durability < item.maxDurability)) {
                ++count;
            }
        }
    }
    return count;
}

int Game::processingBulkRepairMoneyCost() const
{
    if (roguelikeFacilityUiMode_ != RoguelikeFacilityUiMode::Artisan) {
        return 0;
    }

    int rawCost = 0;
    const auto addRepairCost = [&](ProcessingTarget target) {
        if (!target.valid) {
            return;
        }
        const int cost = processingMoneyCost(target, ProcessingMode::Repair);
        rawCost += std::max(10, cost);
    };

    const auto& instances = inventory_.objectInstances();
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        const ItemInstance& instance = instances[static_cast<std::size_t>(i)].instance;
        if (instance.maxDurability < 0 || (!instance.isBroken && instance.currentDurability >= instance.maxDurability)) {
            continue;
        }
        ProcessingTarget target{};
        target.source = BaseItemSource::Backpack;
        target.slotIndex = i;
        target.backpackEntry = StorageEntry{StorageEntryKind::Instance, i};
        target.valid = true;
        addRepairCost(target);
    }

    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
        for (int itemIndex = 0; itemIndex < static_cast<int>(ringItems.size()); ++itemIndex) {
            const SpellRingItem& item = ringItems[static_cast<std::size_t>(itemIndex)];
            if (item.maxDurability < 0 || (!item.broken() && item.durability >= item.maxDurability)) {
                continue;
            }
            ProcessingTarget target{};
            target.source = static_cast<BaseItemSource>(BaseRingSourceOffset + ringIndex);
            target.slotIndex = itemIndex;
            target.ringIndex = ringIndex;
            target.ringItemIndex = itemIndex;
            target.valid = true;
            addRepairCost(target);
        }
    }

    return roguelikeAdjustedFacilityMoneyCost(rawCost);
}

int Game::processingBulkRepairOreCost() const
{
    if (roguelikeFacilityUiMode_ != RoguelikeFacilityUiMode::Artisan) {
        return 0;
    }
    const int count = processingBulkRepairTargetCount();
    if (count <= 0) {
        return 0;
    }
    return std::max(1, (count + 1) / 2 + roguelikeFacilityCostStep() / 2);
}

void Game::applyProcessingBulkRepair()
{
    const int targetCount = processingBulkRepairTargetCount();
    if (targetCount <= 0) {
        baseStatus_ = "修理が必要なアイテムはありません";
        return;
    }

    const int moneyCost = processingBulkRepairMoneyCost();
    const int oreCost = processingBulkRepairOreCost();
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りません";
        return;
    }

    int repairedCount = 0;

    std::vector<std::string> backpackInstanceIds;
    for (const InventoryObjectInstance& entry : inventory_.objectInstances()) {
        const ItemInstance& instance = entry.instance;
        if (instance.maxDurability >= 0 && (instance.isBroken || instance.currentDurability < instance.maxDurability)) {
            backpackInstanceIds.push_back(instance.instanceId);
        }
    }
    for (const std::string& instanceId : backpackInstanceIds) {
        if (inventory_.repairObjectInstance(instanceId)) {
            ++repairedCount;
        }
    }

    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
        for (int itemIndex = 0; itemIndex < static_cast<int>(ringItems.size()); ++itemIndex) {
            const SpellRingItem& item = ringItems[static_cast<std::size_t>(itemIndex)];
            if (item.maxDurability >= 0 && (item.broken() || item.durability < item.maxDurability) &&
                spellRing_.repairItem(ringIndex, itemIndex)) {
                ++repairedCount;
            }
        }
    }

    if (repairedCount > 0) {
        if (moneyCost > 0) {
            money_ -= moneyCost;
        }
        if (oreCost > 0) {
            const bool spent = inventory_.materials().spend(MaterialType::EnhancementOre, oreCost);
            (void)spent;
        }
        std::vector<std::string> lines;
        lines.push_back(std::to_string(repairedCount) + "個のアイテムを修理しました");
        if (moneyCost > 0 || oreCost > 0) {
            lines.push_back(
                "費用: " +
                std::to_string(moneyCost) +
                "G / 強化鉱石 x" +
                std::to_string(oreCost));
        }
        baseStatus_.clear();
        openUiResultDialog(
            baseResultDialog_,
            "一括修理完了",
            lines);
        return;
    }

    baseStatus_ = "修理が必要なアイテムはありません";
}

void Game::openProcessingConfirm(ProcessingTarget target, ProcessingMode mode)
{
    baseProcessingMode_ = static_cast<int>(mode);
    baseProcessingConfirmTarget_ = target;
    baseProcessingConfirmMode_ = mode;
    const bool executable = processingCommandExecutable(target, mode);
    openUiConfirmDialog(
        baseProcessingConfirm_,
        processingModeName(mode),
        "",
        processingActionName(mode),
        "戻る",
        executable ? 0 : 1);
    baseProcessingConfirm_.confirmEnabled = executable;
    baseStatus_.clear();
}

void Game::drawProcessingConfirmDialog(Renderer& renderer, UiRect panel) const
{
    if (!baseProcessingConfirm_.open) {
        return;
    }

    UiWindowScope window(
        renderer,
        "base.processing.confirm",
        panel,
        baseProcessingConfirm_.title,
        uiConfirmDialogHelpText(),
        UiWindowOptions{true, true});

    constexpr float ContentInset = ui::PanelPadding + 12.0f;
    constexpr float BodyTopOffset = -2.0f;
    const float bodyTop = panel.pos.y + ui::HeaderHeight + BodyTopOffset;
    const UiRect body{{
        panel.pos.x + ContentInset,
        bodyTop,
    }, {
        panel.size.x - ContentInset * 2.0f,
        std::max(0.0f, uiConfirmDialogButtonRect(panel, 0).pos.y - bodyTop - 16.0f),
    }};

    const auto targetSnapshot = [this](ProcessingTarget snapshotTarget) {
        if (snapshotTarget.source == BaseItemSource::Backpack || snapshotTarget.source == BaseItemSource::Warehouse) {
            if (snapshotTarget.backpackEntry.kind == StorageEntryKind::Stack) {
                const InventoryObjectStack& stack = snapshotTarget.warehouseEntry
                    ? warehouseObjectStacks_[static_cast<std::size_t>(snapshotTarget.backpackEntry.index)]
                    : inventory_.objectStacks()[static_cast<std::size_t>(snapshotTarget.backpackEntry.index)];
                return processingSnapshotFromStack(stack);
            }
            const InventoryObjectInstance& instance = snapshotTarget.warehouseEntry
                ? warehouseObjectInstances_[static_cast<std::size_t>(snapshotTarget.backpackEntry.index)]
                : inventory_.objectInstances()[static_cast<std::size_t>(snapshotTarget.backpackEntry.index)];
            return processingSnapshotFromInstance(instance);
        }

        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(snapshotTarget.ringIndex);
        if (snapshotTarget.ringItemIndex >= 0 && snapshotTarget.ringItemIndex < static_cast<int>(ringItems.size())) {
            return processingSnapshotFromRingItem(objectCatalog_, ringItems[static_cast<std::size_t>(snapshotTarget.ringItemIndex)]);
        }
        ProcessingResultSnapshot snapshot{};
        snapshot.name = "アイテム";
        return snapshot;
    };

    if (!baseProcessingConfirmTarget_.valid) {
        renderer.drawText(body.pos, "加工対象がありません", ui::Text, 2);
        drawUiConfirmDialogButtons(renderer, baseProcessingConfirm_, panel);
        return;
    }

    const ProcessingMode mode = baseProcessingConfirmMode_;
    const ProcessingResultSnapshot before = targetSnapshot(baseProcessingConfirmTarget_);
    ProcessingResultSnapshot after = before;
    const int beforeModeEnhanceLevel =
        mode == ProcessingMode::Attack ? before.attackEnhanceLevel :
        mode == ProcessingMode::Dig ? before.digEnhanceLevel :
        mode == ProcessingMode::Durability ? before.durabilityEnhanceLevel :
        before.enhanceLevel;
    const ProcessingEnhanceBonuses enhanceBonuses =
        processingEnhanceBonuses(
            mode == ProcessingMode::Attack,
            mode == ProcessingMode::Dig,
            mode == ProcessingMode::Durability,
            before.rarity,
            before.baseDurability,
            beforeModeEnhanceLevel);

    if (mode == ProcessingMode::Repair) {
        if (after.maxDurability >= 0) {
            after.currentDurability = after.maxDurability;
            after.isBroken = false;
        }
    } else if (mode == ProcessingMode::ResetEnhancement) {
        after = processingResetSnapshot(after);
    } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
        after = processingShapeSnapshot(
            after,
            mode == ProcessingMode::Lighten ? LightenWeightMultiplier : EnlargeWeightMultiplier,
            mode == ProcessingMode::Lighten ? 1.0 : EnlargeSizeMultiplier);
    } else {
        after = processingEnhancedSnapshot(after, enhanceBonuses.attack, enhanceBonuses.dig, enhanceBonuses.durability);
    }

    std::vector<ProcessingPreviewRow> previewRows;
    if (mode == ProcessingMode::Repair) {
        previewRows.push_back({
            "耐久力",
            formatProcessingDurability(before.currentDurability, before.maxDurability),
            formatProcessingDurability(after.currentDurability, after.maxDurability),
        });
        if (before.isBroken && !after.isBroken) {
            previewRows.push_back({"状態", "破損", "通常"});
        }
    } else if (mode == ProcessingMode::ResetEnhancement) {
        previewRows.push_back({"攻撃力補正", formatProcessingInt(before.attackBonus), formatProcessingInt(after.attackBonus)});
        previewRows.push_back({"掘削力補正", formatProcessingInt(before.digBonus), formatProcessingInt(after.digBonus)});
        previewRows.push_back({"耐久力補正", formatProcessingInt(before.durabilityBonus), formatProcessingInt(after.durabilityBonus)});
        previewRows.push_back({"合計強化回数", formatProcessingInt(before.enhanceLevel), formatProcessingInt(after.enhanceLevel)});
    } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
        previewRows.push_back({"重量", formatProcessingPercent(before.weightModifier), formatProcessingPercent(after.weightModifier)});
        previewRows.push_back({"大きさ", formatProcessingPercent(before.sizeModifier), formatProcessingPercent(after.sizeModifier)});
    } else {
        if (mode == ProcessingMode::Attack) {
            previewRows.push_back({"攻撃力補正", formatProcessingInt(before.attackBonus), formatProcessingInt(after.attackBonus)});
        } else if (mode == ProcessingMode::Dig) {
            previewRows.push_back({"掘削力補正", formatProcessingInt(before.digBonus), formatProcessingInt(after.digBonus)});
        } else if (mode == ProcessingMode::Durability) {
            previewRows.push_back({"最大耐久力", formatProcessingMaxDurability(before.maxDurability), formatProcessingMaxDurability(after.maxDurability)});
        }
        previewRows.push_back({"合計強化回数", formatProcessingInt(before.enhanceLevel), formatProcessingInt(after.enhanceLevel)});
    }

    const auto confirmQuestion = [&]() -> std::string {
        const std::string itemName = processingInlineItemName(before);
        switch (mode) {
        case ProcessingMode::Repair:
            return itemName + "を修理しますか？";
        case ProcessingMode::ResetEnhancement:
            return itemName + "の強化をリセットしますか？";
        case ProcessingMode::Lighten:
            return itemName + "を軽量化しますか？";
        case ProcessingMode::Enlarge:
            return itemName + "を大型化しますか？";
        case ProcessingMode::Attack:
        case ProcessingMode::Dig:
        case ProcessingMode::Durability:
            return itemName + "を強化しますか？";
        }
        return itemName + "に作業を行いますか？";
    };

    float y = body.pos.y;
    InlineItemTextStyle questionStyle{};
    questionStyle.text = ui::Text;
    questionStyle.scale = 2;
    const std::string question = fittedInlineItemText(renderer, confirmQuestion(), body.size.x, questionStyle);
    drawInlineItemText(renderer, objectCatalog_, {body.pos.x, y}, question, questionStyle);
    y += measureInlineItemText(renderer, question, questionStyle).y + 22.0f;

    for (const ProcessingPreviewRow& row : previewRows) {
        drawProcessingPreviewRow(renderer, body, y, row);
    }

    std::vector<RequirementRow> requirements;
    const int moneyCost = processingMoneyCost(baseProcessingConfirmTarget_, mode);
    const int oreCost = processingOreCost(baseProcessingConfirmTarget_, mode);
    if (moneyCost > 0) {
        requirements.push_back(moneyRequirementRow(moneyCost, money_));
    }
    if (oreCost > 0) {
        requirements.push_back(materialRequirementRow(
            MaterialType::EnhancementOre,
            oreCost,
            inventory_.materialCount(MaterialType::EnhancementOre)));
    }

    const float buttonTop = uiConfirmDialogButtonRect(panel, 0).pos.y;
    constexpr float RequirementTopPadding = ui::SubPanelPadding.y;
    constexpr float RequirementTitleToRows = 34.0f;
    constexpr float RequirementRowHeight = 31.0f;
    constexpr float RequirementBottomPadding = 18.0f;
    const float materialTop = y + 7.0f;
    const float requiredRows = static_cast<float>(std::max<std::size_t>(1, requirements.size()));
    const float preferredMaterialHeight =
        RequirementTopPadding + RequirementTitleToRows + RequirementRowHeight * requiredRows + RequirementBottomPadding;
    const float materialHeight = std::max(86.0f, std::min(preferredMaterialHeight, buttonTop - materialTop - 14.0f));
    drawRequirementSubWindow(
        renderer,
        objectCatalog_,
        {{body.pos.x, materialTop}, {body.size.x, materialHeight}},
        requirements);

    drawUiConfirmDialogButtons(renderer, baseProcessingConfirm_, panel);
}

void Game::applyProcessing(int entryIndex)
{
    const std::vector<StorageEntry> entries = processingEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    applyProcessingEntry(entry);
}

void Game::applyProcessingScreenSlot(int slotIndex)
{
    const ProcessingTarget target = processingTargetForScreenSlot(slotIndex);
    if (!target.valid) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    applyProcessingTarget(target);
}

void Game::applyProcessingEntry(StorageEntry entry, bool warehouseEntry)
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    applyProcessingEntry(entry, mode, warehouseEntry);
}

void Game::applyProcessingEntry(StorageEntry entry, ProcessingMode mode, bool warehouseEntry)
{
    if (!processingEntryAvailable(entry, mode, warehouseEntry)) {
        if (!processingModeUnlocked(mode)) {
            baseStatus_ = "この作業は未解禁です";
        } else if ((mode == ProcessingMode::Repair || mode == ProcessingMode::ResetEnhancement) && entry.kind == StorageEntryKind::Stack) {
            baseStatus_ = "この作業はできません";
        } else if (mode == ProcessingMode::ResetEnhancement) {
            baseStatus_ = "リセット不要です";
        } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
            baseStatus_ = "加工済みです";
        } else {
            baseStatus_ = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
        }
        return;
    }

    const int moneyCost = processingMoneyCost(entry, mode, warehouseEntry);
    const int oreCost = processingOreCost(entry, mode, warehouseEntry);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りません";
        return;
    }

    const auto entrySnapshot = [this, warehouseEntry](StorageEntry snapshotEntry) {
        if (snapshotEntry.kind == StorageEntryKind::Stack) {
            const InventoryObjectStack& stack = warehouseEntry
                ? warehouseObjectStacks_[static_cast<std::size_t>(snapshotEntry.index)]
                : inventory_.objectStacks()[static_cast<std::size_t>(snapshotEntry.index)];
            return processingSnapshotFromStack(stack);
        }
        const InventoryObjectInstance& instance = warehouseEntry
            ? warehouseObjectInstances_[static_cast<std::size_t>(snapshotEntry.index)]
            : inventory_.objectInstances()[static_cast<std::size_t>(snapshotEntry.index)];
        return processingSnapshotFromInstance(instance);
    };
    const ProcessingResultSnapshot beforeSnapshot = entrySnapshot(entry);
    const int beforeModeEnhanceLevel =
        mode == ProcessingMode::Attack ? beforeSnapshot.attackEnhanceLevel :
        mode == ProcessingMode::Dig ? beforeSnapshot.digEnhanceLevel :
        mode == ProcessingMode::Durability ? beforeSnapshot.durabilityEnhanceLevel :
        beforeSnapshot.enhanceLevel;
    const ProcessingEnhanceBonuses enhanceBonuses =
        processingEnhanceBonuses(
            mode == ProcessingMode::Attack,
            mode == ProcessingMode::Dig,
            mode == ProcessingMode::Durability,
            beforeSnapshot.rarity,
            beforeSnapshot.baseDurability,
            beforeModeEnhanceLevel);

    const auto applyEnhancement = [&](ItemInstance& instance) {
        int& modeEnhanceLevel =
            mode == ProcessingMode::Attack ? instance.attackEnhanceLevel :
            mode == ProcessingMode::Dig ? instance.digEnhanceLevel :
            instance.durabilityEnhanceLevel;
        if (modeEnhanceLevel >= MaxItemEnhanceLevel) {
            return false;
        }
        ++instance.enhanceLevel;
        ++modeEnhanceLevel;
        instance.attackBonus += enhanceBonuses.attack;
        instance.digBonus += enhanceBonuses.dig;
        instance.durabilityBonus += enhanceBonuses.durability;
        if (enhanceBonuses.durability > 0 && instance.maxDurability >= 0) {
            instance.maxDurability += enhanceBonuses.durability;
            instance.currentDurability = std::min(instance.maxDurability, std::max(0, instance.currentDurability + enhanceBonuses.durability));
        }
        return true;
    };
    const auto resetEnhancement = [this](ItemInstance& instance) {
        if (instance.enhanceLevel <= 0 &&
            instance.attackEnhanceLevel <= 0 &&
            instance.digEnhanceLevel <= 0 &&
            instance.durabilityEnhanceLevel <= 0 &&
            instance.attackBonus == 0 &&
            instance.digBonus == 0 &&
            instance.durabilityBonus == 0) {
            return false;
        }
        const ItemData* item = objectCatalog_.registry.findById(instance.objectId);
        const int baseDurability = item != nullptr ? item->durability : std::max(-1, instance.maxDurability - instance.durabilityBonus);
        instance.enhanceLevel = 0;
        instance.attackEnhanceLevel = 0;
        instance.digEnhanceLevel = 0;
        instance.durabilityEnhanceLevel = 0;
        instance.attackBonus = 0;
        instance.digBonus = 0;
        instance.durabilityBonus = 0;
        instance.maxDurability = baseDurability;
        if (instance.maxDurability >= 0) {
            instance.currentDurability = std::clamp(instance.currentDurability, 0, instance.maxDurability);
            instance.isBroken = instance.currentDurability == 0;
        } else {
            instance.isBroken = false;
        }
        return true;
    };
    const auto applyShapeProcessing = [](ItemInstance& instance, ProcessingMode shapeMode) {
        if (shapeMode == ProcessingMode::Lighten) {
            if (instance.weightModifier < 0.999) {
                return false;
            }
            instance.weightModifier = std::clamp(instance.weightModifier * LightenWeightMultiplier, 0.25, 4.0);
            return true;
        }
        if (shapeMode == ProcessingMode::Enlarge) {
            if (instance.sizeModifier > 1.001) {
                return false;
            }
            instance.weightModifier = std::clamp(instance.weightModifier * EnlargeWeightMultiplier, 0.25, 4.0);
            instance.sizeModifier = std::clamp(instance.sizeModifier * EnlargeSizeMultiplier, 0.50, 3.0);
            return true;
        }
        return false;
    };
    const auto allocateWarehouseInstanceId = [this]() {
        constexpr std::string_view Prefix = "warehouseinst_";
        unsigned long long nextId = 1;
        const auto scanId = [&nextId, Prefix](const std::string& id) {
            if (id.rfind(Prefix, 0) == 0) {
                const unsigned long long parsed = std::strtoull(id.c_str() + Prefix.size(), nullptr, 10);
                nextId = std::max(nextId, parsed + 1);
            }
        };
        for (const InventoryObjectInstance& instance : inventory_.objectInstances()) {
            scanId(instance.instance.instanceId);
        }
        for (const InventoryObjectInstance& instance : warehouseObjectInstances_) {
            scanId(instance.instance.instanceId);
        }
        for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
            for (const SpellRingItem& item : spellRing_.itemsForRing(ringIndex)) {
                scanId(item.instanceId);
            }
        }
        return std::string(Prefix) + std::to_string(nextId);
    };

    bool processed = false;
    if (!warehouseEntry && mode == ProcessingMode::Repair) {
        const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
        processed = inventory_.repairObjectInstance(instance.instance.instanceId);
    } else if (!warehouseEntry && mode == ProcessingMode::ResetEnhancement) {
        const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
        processed = inventory_.resetObjectInstanceEnhancement(instance.instance.instanceId, objectCatalog_);
    } else if (!warehouseEntry && (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge)) {
        if (entry.kind == StorageEntryKind::Stack) {
            const InventoryObjectStack& stack = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.modifyObjectStackItemShape(
                stack.objectId,
                mode == ProcessingMode::Lighten ? LightenWeightMultiplier : EnlargeWeightMultiplier,
                mode == ProcessingMode::Lighten ? 1.0 : EnlargeSizeMultiplier);
        } else {
            const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.modifyObjectInstanceShape(
                instance.instance.instanceId,
                mode == ProcessingMode::Lighten ? LightenWeightMultiplier : EnlargeWeightMultiplier,
                mode == ProcessingMode::Lighten ? 1.0 : EnlargeSizeMultiplier);
        }
    } else if (!warehouseEntry) {
        if (entry.kind == StorageEntryKind::Stack) {
            const InventoryObjectStack& stack = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectStackItem(
                stack.objectId,
                enhanceBonuses.attack,
                enhanceBonuses.dig,
                enhanceBonuses.durability,
                mode == ProcessingMode::Attack ? 1 : 0,
                mode == ProcessingMode::Dig ? 1 : 0,
                mode == ProcessingMode::Durability ? 1 : 0,
                MaxItemEnhanceLevel);
        } else {
            const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectInstance(
                instance.instance.instanceId,
                enhanceBonuses.attack,
                enhanceBonuses.dig,
                enhanceBonuses.durability,
                mode == ProcessingMode::Attack ? 1 : 0,
                mode == ProcessingMode::Dig ? 1 : 0,
                mode == ProcessingMode::Durability ? 1 : 0,
                MaxItemEnhanceLevel);
        }
    } else if (mode == ProcessingMode::Repair) {
        ItemInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance;
        if (instance.maxDurability >= 0) {
            instance.currentDurability = instance.maxDurability;
            instance.isBroken = false;
            processed = true;
        }
    } else if (mode == ProcessingMode::ResetEnhancement) {
        ItemInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance;
        processed = resetEnhancement(instance);
    } else if (entry.kind == StorageEntryKind::Stack) {
        InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(entry.index)];
        const bool stackSlotWillRemain = stack.count > 1;
        if (stackSlotWillRemain && warehouseUsedSlots() >= warehouseCapacity()) {
            baseStatus_ = "倉庫がいっぱいです";
            return;
        }
        syncWarehouseDisplaySlots();
        const int originalSlot = entry.index >= 0 && entry.index < static_cast<int>(warehouseDisplaySlots_.size())
            ? warehouseDisplaySlots_[static_cast<std::size_t>(entry.index)]
            : -1;
        const ItemData item = stack.item;
        ItemInstance instance = makeItemInstanceFromDefinition(allocateWarehouseInstanceId(), item);
        processed = (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge)
            ? applyShapeProcessing(instance, mode)
            : applyEnhancement(instance);
        if (processed) {
            --stack.count;
            if (stack.count <= 0) {
                removeWarehouseDisplaySlotAtEntryIndex(entry.index);
                warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + entry.index);
            }
            warehouseObjectInstances_.push_back(InventoryObjectInstance{item, std::move(instance)});
            warehouseDisplaySlots_.push_back(stackSlotWillRemain ? -1 : originalSlot);
            syncWarehouseDisplaySlots();
        }
    } else {
        ItemInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance;
        processed = (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge)
            ? applyShapeProcessing(instance, mode)
            : applyEnhancement(instance);
    }
    if (!processed) {
        baseStatus_ = "加工できません";
        return;
    }

    money_ -= moneyCost;
    if (oreCost > 0) {
        const bool spentOre = inventory_.materials().spend(MaterialType::EnhancementOre, oreCost);
        (void)spentOre;
    }
    baseStatus_.clear();
    if (mode == ProcessingMode::Repair) {
        const ProcessingResultSnapshot afterSnapshot = entrySnapshot(entry);
        openUiResultDialog(baseResultDialog_, "作業完了", processingRepairResultLines(beforeSnapshot, afterSnapshot));
    } else if (mode == ProcessingMode::ResetEnhancement) {
        const ProcessingResultSnapshot afterSnapshot = entrySnapshot(entry);
        openUiResultDialog(baseResultDialog_, "作業完了", processingResetResultLines(beforeSnapshot, afterSnapshot));
    } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
        const ProcessingResultSnapshot afterSnapshot = processingShapeSnapshot(
            beforeSnapshot,
            mode == ProcessingMode::Lighten ? LightenWeightMultiplier : EnlargeWeightMultiplier,
            mode == ProcessingMode::Lighten ? 1.0 : EnlargeSizeMultiplier);
        openUiResultDialog(baseResultDialog_, "作業完了", processingShapeResultLines(beforeSnapshot, afterSnapshot, mode == ProcessingMode::Lighten));
    } else {
        const ProcessingResultSnapshot afterSnapshot = entry.kind == StorageEntryKind::Stack
            ? processingEnhancedSnapshot(
                beforeSnapshot,
                enhanceBonuses.attack,
                enhanceBonuses.dig,
                enhanceBonuses.durability)
            : entrySnapshot(entry);
        openUiResultDialog(
            baseResultDialog_,
            "作業完了",
            processingEnhanceResultLines(
                beforeSnapshot,
                afterSnapshot,
                mode == ProcessingMode::Attack,
                mode == ProcessingMode::Dig,
                mode == ProcessingMode::Durability));
    }
    const int selectionCount = warehouseEntry ? StoragePaneSlotCount : inventory_.screenSlotCount();
    baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, selectionCount - 1));
}

void Game::applyProcessingTarget(ProcessingTarget target)
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    applyProcessingTarget(target, mode);
}

void Game::applyProcessingTarget(ProcessingTarget target, ProcessingMode mode)
{
    if (!target.valid) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        applyProcessingEntry(target.backpackEntry, mode, target.warehouseEntry);
        return;
    }

    if (!processingTargetAvailable(target, mode)) {
        if (!processingModeUnlocked(mode)) {
            baseStatus_ = "この作業は未解禁です";
        } else if (mode == ProcessingMode::ResetEnhancement) {
            baseStatus_ = "リセット不要です";
        } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
            baseStatus_ = "加工済みです";
        } else {
            baseStatus_ = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
        }
        return;
    }

    const int moneyCost = processingMoneyCost(target, mode);
    const int oreCost = processingOreCost(target, mode);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りません";
        return;
    }

    const std::vector<SpellRingItem>& ringItemsBefore = spellRing_.itemsForRing(target.ringIndex);
    const ProcessingResultSnapshot beforeSnapshot =
        processingSnapshotFromRingItem(objectCatalog_, ringItemsBefore[static_cast<std::size_t>(target.ringItemIndex)]);
    const int beforeModeEnhanceLevel =
        mode == ProcessingMode::Attack ? beforeSnapshot.attackEnhanceLevel :
        mode == ProcessingMode::Dig ? beforeSnapshot.digEnhanceLevel :
        mode == ProcessingMode::Durability ? beforeSnapshot.durabilityEnhanceLevel :
        beforeSnapshot.enhanceLevel;
    const ProcessingEnhanceBonuses enhanceBonuses =
        processingEnhanceBonuses(
            mode == ProcessingMode::Attack,
            mode == ProcessingMode::Dig,
            mode == ProcessingMode::Durability,
            beforeSnapshot.rarity,
            beforeSnapshot.baseDurability,
            beforeModeEnhanceLevel);

    bool processed = false;
    if (mode == ProcessingMode::Repair) {
        processed = spellRing_.repairItem(target.ringIndex, target.ringItemIndex);
    } else if (mode == ProcessingMode::ResetEnhancement ||
        mode == ProcessingMode::Lighten ||
        mode == ProcessingMode::Enlarge) {
        std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
        if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size())) {
            SpellRingItem& item = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
            if (mode == ProcessingMode::ResetEnhancement) {
                const ItemData* object = objectCatalog_.registry.findById(item.objectId);
                const int baseDurability = object != nullptr ? object->durability : std::max(-1, item.maxDurability - item.durabilityBonus);
                item.enhanceLevel = 0;
                item.attackEnhanceLevel = 0;
                item.digEnhanceLevel = 0;
                item.durabilityEnhanceLevel = 0;
                item.attackBonus = 0;
                item.digBonus = 0;
                item.durabilityBonus = 0;
                item.maxDurability = baseDurability;
                if (item.maxDurability >= 0) {
                    item.durability = std::clamp(item.durability, 0, item.maxDurability);
                    item.isBroken = item.durability == 0;
                } else {
                    item.isBroken = false;
                }
                item.objectStatsApplied = false;
                spellRing_.applyObjectParameters(objectCatalog_);
                processed = true;
            } else if (mode == ProcessingMode::Lighten) {
                if (item.weightModifier >= 0.999) {
                    item.weightModifier = std::clamp(item.weightModifier * LightenWeightMultiplier, 0.25, 4.0);
                    item.objectStatsApplied = false;
                    spellRing_.applyObjectParameters(objectCatalog_);
                    processed = true;
                }
            } else if (mode == ProcessingMode::Enlarge) {
                if (item.sizeModifier <= 1.001) {
                    item.weightModifier = std::clamp(item.weightModifier * EnlargeWeightMultiplier, 0.25, 4.0);
                    item.sizeModifier = std::clamp(item.sizeModifier * EnlargeSizeMultiplier, 0.50, 3.0);
                    item.objectStatsApplied = false;
                    spellRing_.applyObjectParameters(objectCatalog_);
                    processed = true;
                }
            }
        }
    } else {
        processed = spellRing_.enhanceItem(
            target.ringIndex,
            target.ringItemIndex,
            enhanceBonuses.attack,
            enhanceBonuses.dig,
            enhanceBonuses.durability,
            mode == ProcessingMode::Attack ? 1 : 0,
            mode == ProcessingMode::Dig ? 1 : 0,
            mode == ProcessingMode::Durability ? 1 : 0,
            MaxItemEnhanceLevel,
            objectCatalog_);
    }
    if (!processed) {
        baseStatus_ = "加工できません";
        return;
    }

    money_ -= moneyCost;
    if (oreCost > 0) {
        const bool spentOre = inventory_.materials().spend(MaterialType::EnhancementOre, oreCost);
        (void)spentOre;
    }
    refreshOrbitEffects();
    const std::vector<SpellRingItem>& ringItemsAfter = spellRing_.itemsForRing(target.ringIndex);
    const ProcessingResultSnapshot afterSnapshot =
        processingSnapshotFromRingItem(objectCatalog_, ringItemsAfter[static_cast<std::size_t>(target.ringItemIndex)]);
    baseStatus_.clear();
    if (mode == ProcessingMode::Repair) {
        openUiResultDialog(baseResultDialog_, "作業完了", processingRepairResultLines(beforeSnapshot, afterSnapshot));
    } else if (mode == ProcessingMode::ResetEnhancement) {
        openUiResultDialog(baseResultDialog_, "作業完了", processingResetResultLines(beforeSnapshot, afterSnapshot));
    } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
        openUiResultDialog(baseResultDialog_, "作業完了", processingShapeResultLines(beforeSnapshot, afterSnapshot, mode == ProcessingMode::Lighten));
    } else {
        openUiResultDialog(
            baseResultDialog_,
            "作業完了",
            processingEnhanceResultLines(
                beforeSnapshot,
                afterSnapshot,
                mode == ProcessingMode::Attack,
                mode == ProcessingMode::Dig,
                mode == ProcessingMode::Durability));
    }
    baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
}

int Game::warehouseCapacity() const
{
    constexpr std::array<int, 5> Capacities{{48, 72, 100, 140, 200}};
    const int level = std::clamp(warehouseCapacityLevel_, 0, static_cast<int>(Capacities.size()) - 1);
    return Capacities[static_cast<std::size_t>(level)];
}

int Game::warehouseUsedSlots() const
{
    return static_cast<int>(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
}

int Game::backpackUsedSlots() const
{
    return static_cast<int>(inventory_.objectStacks().size() + inventory_.objectInstances().size());
}

std::vector<Game::StorageEntry> Game::backpackStorageEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

std::vector<Game::StorageEntry> Game::warehouseStorageEntries() const
{
    std::vector<StorageEntry> entries;
    entries.reserve(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
    for (int i = 0; i < static_cast<int>(warehouseObjectStacks_.size()); ++i) {
        if (warehouseObjectStacks_[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(warehouseObjectInstances_.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

void Game::syncWarehouseDisplaySlots() const
{
    const int totalCount = warehouseUsedSlots();
    if (totalCount <= 0) {
        warehouseDisplaySlots_.clear();
        return;
    }

    const int capacity = warehouseCapacity();
    std::vector<int> nextSlots(static_cast<std::size_t>(totalCount), -1);
    std::vector<bool> used(static_cast<std::size_t>(capacity), false);
    const int copyCount = std::min(totalCount, static_cast<int>(warehouseDisplaySlots_.size()));
    for (int i = 0; i < copyCount; ++i) {
        const int slot = warehouseDisplaySlots_[static_cast<std::size_t>(i)];
        if (slot >= 0 && slot < capacity && !used[static_cast<std::size_t>(slot)]) {
            nextSlots[static_cast<std::size_t>(i)] = slot;
            used[static_cast<std::size_t>(slot)] = true;
        }
    }

    int cursor = 0;
    for (int i = 0; i < totalCount; ++i) {
        if (nextSlots[static_cast<std::size_t>(i)] >= 0) {
            continue;
        }
        while (cursor < capacity && used[static_cast<std::size_t>(cursor)]) {
            ++cursor;
        }
        if (cursor >= capacity) {
            nextSlots[static_cast<std::size_t>(i)] = i % capacity;
        } else {
            nextSlots[static_cast<std::size_t>(i)] = cursor;
            used[static_cast<std::size_t>(cursor)] = true;
            ++cursor;
        }
    }
    warehouseDisplaySlots_ = std::move(nextSlots);
}

void Game::sortWarehouseByCatalogOrder()
{
    closeUiCommandMenu(baseStorageCommandMenu_);
    baseStorageCommandOperation_ = StorageQuantityOperation::None;
    baseStorageCommandTarget_ = {};
    baseStoragePointerOperation_ = StorageQuantityOperation::None;
    baseStoragePointerTarget_ = {};
    baseStoragePointerPressMouse_ = {};
    baseStoragePointerPressCanOpenMenu_ = false;
    baseStoragePointerDragTriggered_ = false;

    const int totalCount = warehouseUsedSlots();
    if (totalCount <= 0) {
        warehouseDisplaySlots_.clear();
        baseStorageWarehousePage_ = 0;
        baseStorageWithdrawSelection_ = 0;
        baseStatus_ = "収納箱は空です";
        return;
    }

    const auto order = buildObjectSortOrder(objectCatalog_);
    std::stable_sort(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [&order](const InventoryObjectStack& a, const InventoryObjectStack& b) {
        const int orderA = objectSortOrder(order, a.objectId);
        const int orderB = objectSortOrder(order, b.objectId);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        return a.objectId < b.objectId;
    });
    std::stable_sort(warehouseObjectInstances_.begin(), warehouseObjectInstances_.end(), [&order](const InventoryObjectInstance& a, const InventoryObjectInstance& b) {
        const std::string& idA = objectSortId(a);
        const std::string& idB = objectSortId(b);
        const int orderA = objectSortOrder(order, idA);
        const int orderB = objectSortOrder(order, idB);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        return idA < idB;
    });

    std::vector<int> entryIndices;
    entryIndices.reserve(static_cast<std::size_t>(totalCount));
    for (int i = 0; i < totalCount; ++i) {
        entryIndices.push_back(i);
    }
    std::stable_sort(entryIndices.begin(), entryIndices.end(), [this, &order](int a, int b) {
        const std::string& idA = warehouseEntrySortId(a, warehouseObjectStacks_, warehouseObjectInstances_);
        const std::string& idB = warehouseEntrySortId(b, warehouseObjectStacks_, warehouseObjectInstances_);
        const int orderA = objectSortOrder(order, idA);
        const int orderB = objectSortOrder(order, idB);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        if (idA != idB) {
            return idA < idB;
        }
        return a < b;
    });

    const int capacity = warehouseCapacity();
    warehouseDisplaySlots_.assign(static_cast<std::size_t>(totalCount), -1);
    for (int slot = 0; slot < static_cast<int>(entryIndices.size()); ++slot) {
        const int entryIndex = entryIndices[static_cast<std::size_t>(slot)];
        warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)] = capacity > 0 ? slot % capacity : -1;
    }

    baseStorageWarehousePage_ = 0;
    baseStorageWithdrawSelection_ = 0;
    baseStatus_ = "収納箱を並び替えました";
}

int Game::warehouseEntryIndexAtStorageSlot(int slot) const
{
    syncWarehouseDisplaySlots();
    if (slot < 0 || slot >= warehouseCapacity()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(warehouseDisplaySlots_.size()); ++i) {
        if (warehouseDisplaySlots_[static_cast<std::size_t>(i)] == slot) {
            return i;
        }
    }
    return -1;
}

void Game::assignWarehouseEntryToStorageSlot(int entryIndex, int slot)
{
    syncWarehouseDisplaySlots();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseDisplaySlots_.size()) || slot < 0 || slot >= warehouseCapacity()) {
        return;
    }
    for (int i = 0; i < static_cast<int>(warehouseDisplaySlots_.size()); ++i) {
        if (i != entryIndex && warehouseDisplaySlots_[static_cast<std::size_t>(i)] == slot) {
            std::swap(warehouseDisplaySlots_[static_cast<std::size_t>(i)], warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)]);
            return;
        }
    }
    warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)] = slot;
}

void Game::removeWarehouseDisplaySlotAtEntryIndex(int entryIndex)
{
    syncWarehouseDisplaySlots();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseDisplaySlots_.size())) {
        return;
    }
    warehouseDisplaySlots_.erase(warehouseDisplaySlots_.begin() + entryIndex);
}

std::string Game::storageEntryLabel(StorageEntry entry, bool warehouseEntry) const
{
    char buffer[192];
    if (entry.kind == StorageEntryKind::Stack) {
        const InventoryObjectStack& stack = warehouseEntry
            ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)]
            : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
        const std::string name = itemDisplayName(stack.item.name, stack.item.durability == 0);
        std::snprintf(buffer, sizeof(buffer), "%s x%d", name.c_str(), stack.count);
        return buffer;
    }

    const InventoryObjectInstance& instance = warehouseEntry
        ? warehouseObjectInstances_[static_cast<std::size_t>(entry.index)]
        : inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    const std::string name = itemDisplayName(instance.item.name, instance.instance.isBroken);
    std::snprintf(buffer, sizeof(buffer), "%s %sLv.%d",
        name.c_str(),
        instance.instance.protectionEnabled ? "[保護] " : "",
        instance.instance.enhanceLevel);
    return buffer;
}

const ItemData* Game::storageEntryItem(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind == StorageEntryKind::Stack) {
        return warehouseEntry
            ? &warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].item
            : &inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].item;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].item
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].item;
}

const ItemInstance* Game::storageEntryInstance(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Instance) {
        return nullptr;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].instance;
}

int Game::storageEntryStackCount(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Stack) {
        return 1;
    }
    return warehouseEntry
        ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].count
        : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].count;
}

Game::StorageTransferTarget Game::storageDepositTargetForSourceSlot(int source, int slotIndex) const
{
    StorageTransferTarget target{};
    target.slotIndex = slotIndex;
    const int clampedSource = std::clamp(source, 0, BaseItemSourceCount - 1);
    target.source = static_cast<BaseItemSource>(clampedSource);

    if (target.source == BaseItemSource::Backpack) {
        if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
            return target;
        }
        if (inventory_.screenObjectStackAt(slotIndex) != nullptr ||
            inventory_.screenObjectInstanceAt(slotIndex) != nullptr) {
            target.valid = true;
        }
        return target;
    }

    if (!baseItemSourceIsRing(clampedSource)) {
        return target;
    }

    target.ringIndex = ringIndexFromBaseItemSource(clampedSource);
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return target;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (slotIndex < 0 || slotIndex >= static_cast<int>(ringItems.size())) {
        return target;
    }
    target.ringItemIndex = slotIndex;
    target.valid = true;
    return target;
}

Game::StorageTransferTarget Game::storageDepositTargetForScreenSlot(int slotIndex) const
{
    return storageDepositTargetForSourceSlot(baseStorageDepositSource_, slotIndex);
}

Game::StorageTransferTarget Game::storageWithdrawTargetForSlot(int slotIndex) const
{
    StorageTransferTarget target{};
    target.source = BaseItemSource::Warehouse;
    target.slotIndex = slotIndex;
    const std::optional<StorageEntry> entry = warehouseEntryForPageSlot(
        slotIndex,
        baseStorageWarehousePage_,
        StorageWithdrawSlotCount);
    if (!entry) {
        return target;
    }
    target.storageEntry = *entry;
    target.warehouseEntry = true;
    target.valid = true;
    return target;
}

bool Game::storageTransferTargetAvailable(StorageTransferTarget target) const
{
    if (!target.valid) {
        return false;
    }
    if (target.source == BaseItemSource::Backpack) {
        if (inventory_.screenObjectStackAt(target.slotIndex) != nullptr) {
            return true;
        }
        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            return !inventory_.isStaffEquipped(instance->instance.instanceId);
        }
        return false;
    }
    if (target.source == BaseItemSource::Warehouse) {
        if (target.storageEntry.kind == StorageEntryKind::Stack) {
            return storageEntryStackCount(target.storageEntry, true) > 0;
        }
        return storageEntryInstance(target.storageEntry, true) != nullptr;
    }
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return false;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return false;
    }
    return !ringItems[static_cast<std::size_t>(target.ringItemIndex)].objectId.empty();
}

bool Game::storageTransferTargetIsStack(StorageTransferTarget target) const
{
    if (!target.valid) {
        return false;
    }
    if (target.source == BaseItemSource::Backpack) {
        return inventory_.screenObjectStackAt(target.slotIndex) != nullptr;
    }
    if (target.source == BaseItemSource::Warehouse) {
        return target.storageEntry.kind == StorageEntryKind::Stack;
    }
    return false;
}

int Game::storageTransferTargetStackCount(StorageTransferTarget target) const
{
    if (!storageTransferTargetIsStack(target)) {
        return 1;
    }
    if (target.source == BaseItemSource::Backpack) {
        const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex);
        return stack != nullptr ? stack->count : 0;
    }
    if (target.source == BaseItemSource::Warehouse) {
        return storageEntryStackCount(target.storageEntry, true);
    }
    return 1;
}

InventoryUiEntryView Game::storageTransferTargetView(StorageTransferTarget target) const
{
    InventoryUiEntryView view{};
    if (!target.valid) {
        return view;
    }
    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            view.item = &stack->item;
            view.stackCount = stack->count;
            return view;
        }
        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            view.item = &instance->item;
            view.instance = &instance->instance;
            view.stackCount = 1;
            view.equipped = inventory_.isStaffEquipped(instance->instance.instanceId);
        }
        return view;
    }
    if (target.source == BaseItemSource::Warehouse) {
        return storageEntryView(target.storageEntry, true);
    }
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return view;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return view;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    view.item = objectForRingItem(objectCatalog_, ringItem);
    view.stats = inventoryUiStatsFromRingItem(ringItem);
    view.stackCount = 1;
    return view;
}

void Game::depositStorageTarget(StorageTransferTarget target, int count)
{
    if (!target.valid) {
        baseStatus_ = "しまうアイテムがありません";
        return;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* source = inventory_.screenObjectStackAt(target.slotIndex)) {
            const int moveCount = std::clamp(count, 1, std::max(1, source->count));
            auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [source](const InventoryObjectStack& stack) {
                return stack.objectId == source->objectId;
            });
            if (it == warehouseObjectStacks_.end()) {
                if (warehouseUsedSlots() >= warehouseCapacity()) {
                    baseStatus_ = "収納箱がいっぱいです";
                    return;
                }
                syncWarehouseDisplaySlots();
                const int newStackIndex = static_cast<int>(warehouseObjectStacks_.size());
                warehouseDisplaySlots_.insert(warehouseDisplaySlots_.begin() + newStackIndex, -1);
                warehouseObjectStacks_.push_back(InventoryObjectStack{source->item, 0});
                it = warehouseObjectStacks_.end() - 1;
            }
            const std::string objectId = source->objectId;
            if (!inventory_.removeObjectItemCount(objectId, moveCount)) {
                baseStatus_ = "しまえませんでした";
                return;
            }
            it->count += moveCount;
            baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
            baseStatus_ = "収納箱にしまいました";
            return;
        }

        const InventoryObjectInstance* source = inventory_.screenObjectInstanceAt(target.slotIndex);
        if (source == nullptr) {
            baseStatus_ = "しまうアイテムがありません";
            return;
        }
        if (inventory_.isStaffEquipped(source->instance.instanceId)) {
            baseStatus_ = "装備中の杖はしまえません";
            return;
        }
        if (warehouseUsedSlots() >= warehouseCapacity()) {
            baseStatus_ = "収納箱がいっぱいです";
            return;
        }
        InventoryObjectInstance moved;
        if (!inventory_.takeObjectInstance(source->instance.instanceId, moved)) {
            baseStatus_ = "しまえませんでした";
            return;
        }
        warehouseObjectInstances_.push_back(std::move(moved));
        baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
        baseStatus_ = "収納箱にしまいました";
        return;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        baseStatus_ = "しまうアイテムがありません";
        return;
    }
    std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        baseStatus_ = "しまうアイテムがありません";
        return;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    if (ringItem.objectId.empty()) {
        baseStatus_ = "このアイテムはしまえません";
        return;
    }
    if (warehouseUsedSlots() >= warehouseCapacity()) {
        baseStatus_ = "収納箱がいっぱいです";
        return;
    }

    const ItemData* object = objectForRingItem(objectCatalog_, ringItem);
    const ItemData missingObject = object == nullptr ? makeMissingItemData(ringItem.objectId) : ItemData{};
    ItemInstance instance = inventoryInstanceFromRingItem(inventory_, objectCatalog_, ringItem);
    warehouseObjectInstances_.push_back(InventoryObjectInstance{
        object != nullptr ? *object : missingObject,
        std::move(instance),
    });
    ringItems.erase(ringItems.begin() + target.ringItemIndex);
    refreshOrbitEffects();
    baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, static_cast<int>(ringItems.size()) - 1));
    baseStatus_ = "収納箱にしまいました";
}

void Game::withdrawStorageTarget(StorageTransferTarget target, int count)
{
    if (!target.valid || target.source != BaseItemSource::Warehouse) {
        baseStatus_ = "取り出すアイテムがありません";
        return;
    }

    if (target.storageEntry.kind == StorageEntryKind::Stack) {
        if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectStacks_.size())) {
            baseStatus_ = "取り出すアイテムがありません";
            return;
        }
        InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(target.storageEntry.index)];
        const int moveCount = std::clamp(count, 1, std::max(1, stack.count));
        const std::string objectId = stack.objectId;
        if (!inventory_.canAddObjectItem(objectCatalog_, objectId)) {
            baseStatus_ = "リュックがいっぱいです";
            return;
        }
        for (int i = 0; i < moveCount; ++i) {
            if (!inventory_.addObjectItem(objectCatalog_, objectId)) {
                baseStatus_ = "リュックがいっぱいです";
                return;
            }
        }
        stack.count -= moveCount;
        if (stack.count <= 0) {
            removeWarehouseDisplaySlotAtEntryIndex(target.storageEntry.index);
            warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + target.storageEntry.index);
        }
        baseStorageWithdrawSelection_ = std::clamp(baseStorageWithdrawSelection_, 0, StorageWithdrawSlotCount - 1);
        baseStatus_ = "リュックに取り出しました";
        return;
    }

    if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectInstances_.size())) {
        baseStatus_ = "取り出すアイテムがありません";
        return;
    }
    InventoryObjectInstance moved = warehouseObjectInstances_[static_cast<std::size_t>(target.storageEntry.index)];
    if (!inventory_.addObjectInstance(objectCatalog_, moved.instance)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index);
    warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + target.storageEntry.index);
    baseStorageWithdrawSelection_ = std::clamp(baseStorageWithdrawSelection_, 0, StorageWithdrawSlotCount - 1);
    baseStatus_ = "リュックに取り出しました";
}

void Game::depositAllStorageItems()
{
    int storedCount = 0;
    int skippedFullCount = 0;
    int skippedStaffCount = 0;
    bool ringChanged = false;

    const auto findWarehouseStack = [this](std::string_view objectId) {
        return std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [objectId](const InventoryObjectStack& stack) {
            return stack.objectId == objectId;
        });
    };

    const std::vector<InventoryObjectStack> backpackStacks = inventory_.objectStacks();
    for (const InventoryObjectStack& stack : backpackStacks) {
        if (stack.objectId.empty() || stack.count <= 0) {
            continue;
        }

        const bool existingStack = findWarehouseStack(stack.objectId) != warehouseObjectStacks_.end();
        if (!existingStack && warehouseUsedSlots() >= warehouseCapacity()) {
            skippedFullCount += stack.count;
            continue;
        }

        const int moveCount = stack.count;
        if (!inventory_.removeObjectItemCount(stack.objectId, moveCount)) {
            continue;
        }

        auto it = findWarehouseStack(stack.objectId);
        if (it == warehouseObjectStacks_.end()) {
            warehouseObjectStacks_.push_back(InventoryObjectStack{stack.item, moveCount});
        } else {
            it->item = stack.item;
            it->count += moveCount;
        }
        storedCount += moveCount;
    }

    const std::vector<InventoryObjectInstance> backpackInstances = inventory_.objectInstances();
    for (const InventoryObjectInstance& instance : backpackInstances) {
        if (inventory_.isStaffEquipped(instance.instance.instanceId)) {
            ++skippedStaffCount;
            continue;
        }
        if (warehouseUsedSlots() >= warehouseCapacity()) {
            ++skippedFullCount;
            continue;
        }
        InventoryObjectInstance moved;
        if (inventory_.takeObjectInstance(instance.instance.instanceId, moved)) {
            warehouseObjectInstances_.push_back(std::move(moved));
            ++storedCount;
        }
    }

    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
        for (int itemIndex = static_cast<int>(ringItems.size()) - 1; itemIndex >= 0; --itemIndex) {
            const SpellRingItem ringItem = ringItems[static_cast<std::size_t>(itemIndex)];
            if (ringItem.objectId.empty()) {
                continue;
            }
            if (warehouseUsedSlots() >= warehouseCapacity()) {
                ++skippedFullCount;
                continue;
            }

            const ItemData* object = objectForRingItem(objectCatalog_, ringItem);
            const ItemData missingObject = object == nullptr ? makeMissingItemData(ringItem.objectId) : ItemData{};
            ItemInstance instance = inventoryInstanceFromRingItem(inventory_, objectCatalog_, ringItem);
            warehouseObjectInstances_.push_back(InventoryObjectInstance{
                object != nullptr ? *object : missingObject,
                std::move(instance),
            });
            ringItems.erase(ringItems.begin() + itemIndex);
            ++storedCount;
            ringChanged = true;
        }
    }

    syncWarehouseDisplaySlots();
    if (ringChanged) {
        refreshOrbitEffects();
    }
    syncEncyclopediaFromInventoryAndRing();

    if (storedCount <= 0) {
        if (skippedFullCount > 0) {
            baseStatus_ = "収納箱がいっぱいです";
        } else if (skippedStaffCount > 0) {
            baseStatus_ = "装備中の杖以外にしまう物がありません";
        } else {
            baseStatus_ = "しまうアイテムがありません";
        }
        return;
    }

    baseStatus_ = std::to_string(storedCount) + "個しまいました";
    if (skippedFullCount > 0) {
        baseStatus_ += " / 満杯で" + std::to_string(skippedFullCount) + "個残りました";
    }
    if (skippedStaffCount > 0) {
        baseStatus_ += " / 装備中の杖は残しました";
    }
}

void Game::prepareRingPresetFromWarehouse(int presetIndex)
{
    const int presetSlotCount = unlockedRingPresetSlotCount();
    if (presetIndex < 0 || presetIndex >= presetSlotCount) {
        baseStatus_ = presetSlotCount <= 0
            ? "リングプリセットは未解禁です"
            : "プリセット" + std::to_string(presetIndex + 1) + "は未解禁です";
        return;
    }
    if (!ringPresets_.registered(presetIndex)) {
        baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "は未登録です";
        return;
    }

    const std::vector<RingPresetItem> missingItems = ringPresets_.missingItemsForPreset(
        presetIndex,
        inventory_,
        spellRing_,
        objectCatalog_,
        unlockedRingCount());
    if (missingItems.empty()) {
        baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "の必要アイテムは手元にあります";
        return;
    }

    struct WarehousePresetPick {
        bool instance = false;
        std::string objectId;
        std::string instanceId;
    };

    std::vector<WarehousePresetPick> picks;
    std::vector<bool> usedInstances(warehouseObjectInstances_.size(), false);
    std::vector<int> usedStackCounts(warehouseObjectStacks_.size(), 0);
    int notFoundCount = 0;
    constexpr int NoWarehousePresetMatchScore = std::numeric_limits<int>::max() / 8;

    for (const RingPresetItem& missing : missingItems) {
        int bestScore = std::numeric_limits<int>::max();
        WarehousePresetPick bestPick{};
        int bestIndex = -1;

        for (int i = 0; i < static_cast<int>(warehouseObjectInstances_.size()); ++i) {
            if (usedInstances[static_cast<std::size_t>(i)]) {
                continue;
            }
            const InventoryObjectInstance& candidate = warehouseObjectInstances_[static_cast<std::size_t>(i)];
            const int score = ringPresetInstanceMatchScore(missing, candidate.instance);
            if (score >= NoWarehousePresetMatchScore) {
                continue;
            }
            if (score < bestScore) {
                bestScore = score;
                bestIndex = i;
                bestPick = WarehousePresetPick{
                    .instance = true,
                    .objectId = candidate.instance.objectId,
                    .instanceId = candidate.instance.instanceId,
                };
            }
        }

        for (int i = 0; i < static_cast<int>(warehouseObjectStacks_.size()); ++i) {
            const InventoryObjectStack& candidate = warehouseObjectStacks_[static_cast<std::size_t>(i)];
            if (candidate.count <= usedStackCounts[static_cast<std::size_t>(i)]) {
                continue;
            }
            const int score = ringPresetStackMatchScore(missing, candidate.item);
            if (score >= NoWarehousePresetMatchScore) {
                continue;
            }
            if (score < bestScore) {
                bestScore = score;
                bestIndex = i;
                bestPick = WarehousePresetPick{
                    .instance = false,
                    .objectId = candidate.objectId,
                };
            }
        }

        if (bestIndex < 0) {
            ++notFoundCount;
            continue;
        }
        if (bestPick.instance) {
            usedInstances[static_cast<std::size_t>(bestIndex)] = true;
        } else {
            ++usedStackCounts[static_cast<std::size_t>(bestIndex)];
        }
        picks.push_back(std::move(bestPick));
    }

    int withdrawnCount = 0;
    int fullCount = 0;
    int vanishedCount = 0;
    for (const WarehousePresetPick& pick : picks) {
        if (pick.instance) {
            const auto it = std::find_if(warehouseObjectInstances_.begin(), warehouseObjectInstances_.end(), [&pick](const InventoryObjectInstance& entry) {
                return entry.instance.instanceId == pick.instanceId;
            });
            if (it == warehouseObjectInstances_.end()) {
                ++vanishedCount;
                continue;
            }
            if (!inventory_.addObjectInstance(objectCatalog_, it->instance)) {
                ++fullCount;
                continue;
            }
            const int instanceIndex = static_cast<int>(std::distance(warehouseObjectInstances_.begin(), it));
            removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + instanceIndex);
            warehouseObjectInstances_.erase(it);
            ++withdrawnCount;
            continue;
        }

        const auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [&pick](const InventoryObjectStack& stack) {
            return stack.objectId == pick.objectId && stack.count > 0;
        });
        if (it == warehouseObjectStacks_.end()) {
            ++vanishedCount;
            continue;
        }
        if (!inventory_.addObjectItem(objectCatalog_, pick.objectId)) {
            ++fullCount;
            continue;
        }
        --it->count;
        if (it->count <= 0) {
            const int stackIndex = static_cast<int>(std::distance(warehouseObjectStacks_.begin(), it));
            removeWarehouseDisplaySlotAtEntryIndex(stackIndex);
            warehouseObjectStacks_.erase(it);
        }
        ++withdrawnCount;
    }

    syncWarehouseDisplaySlots();
    syncEncyclopediaFromInventoryAndRing();

    if (withdrawnCount <= 0) {
        if (fullCount > 0) {
            baseStatus_ = "リュックがいっぱいで取り出せません";
        } else {
            baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "の不足分は収納箱にありません";
        }
    } else {
        baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "ぶんを" + std::to_string(withdrawnCount) + "個取り出しました";
    }
    if (notFoundCount > 0) {
        baseStatus_ += " / 収納箱になし " + std::to_string(notFoundCount);
    }
    if (fullCount > 0) {
        baseStatus_ += " / リュック満杯 " + std::to_string(fullCount);
    }
    if (vanishedCount > 0) {
        baseStatus_ += " / 取り出せず " + std::to_string(vanishedCount);
    }
}

int Game::upgradeCost(int index) const
{
    constexpr std::array<int, 4> StorageCosts{{500, 1000, 2000, 4000}};
    constexpr std::array<int, 6> MerchantCosts{{400, 800, 1600, 3200, 6400, 12000}};
    constexpr std::array<int, 5> ProcessingCosts{{300, 700, 1500, 3000, 5000}};
    constexpr std::array<int, 5> MaxHpCosts{{600, 1200, 2400, 4800, 8000}};
    constexpr std::array<int, 5> RingRadiusCosts{{1200, 2400, 4800, 9000, 15000}};
    constexpr std::array<int, 5> RingSpeedCosts{{1200, 2400, 4800, 9000, 15000}};
    constexpr std::array<int, 5> CollectionRangeCosts{{750, 1500, 3000, 5500, 9000}};
    constexpr std::array<int, 3> RingPresetCosts{{2000, 4000, 8000}};

    int cost = 0;
    switch (index) {
    case 0: cost = baseUpgradeCostForStep(warehouseCapacityLevel_, StorageCosts); break;
    case 1: cost = baseUpgradeCostForStep(std::max(0, merchantUpgradeLevel_ - 1), MerchantCosts); break;
    case 2: cost = baseUpgradeCostForStep(processingUnlockLevel_, ProcessingCosts); break;
    case 3: cost = ringWorkshopUnlocked_ ? 0 : 10000; break;
    case 4: cost = baseUpgradeCostForStep(maxHpUpgradeLevel_, MaxHpCosts); break;
    case 5: cost = baseUpgradeCostForStep(ringRadiusUpgradeLevel_, RingRadiusCosts); break;
    case 6: cost = baseUpgradeCostForStep(ringSpeedUpgradeLevel_, RingSpeedCosts); break;
    case 7: cost = baseUpgradeCostForStep(collectionRangeUpgradeLevel_, CollectionRangeCosts); break;
    case 8: cost = baseUpgradeCostForStep(ringPresetSlotLevel_, RingPresetCosts); break;
    default: cost = 0; break;
    }
    return roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Trainer
        ? roguelikeAdjustedFacilityMoneyCost(cost)
        : cost;
}

MaterialType Game::upgradeMaterialType(int index) const
{
    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
        return MaterialType::OldWoodBuildingMaterial;
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        return MaterialType::ManaDrop;
    default:
        return MaterialType::OldWoodBuildingMaterial;
    }
}

int Game::upgradeMaterialCost(int index) const
{
    constexpr std::array<int, 4> StorageMaterialCosts{{9, 15, 24, 36}};
    constexpr std::array<int, 6> MerchantMaterialCosts{{6, 12, 21, 30, 42, 54}};
    constexpr std::array<int, 5> ProcessingMaterialCosts{{6, 12, 18, 27, 36}};
    constexpr std::array<int, 5> MaxHpMaterialCosts{{4, 8, 14, 20, 28}};
    constexpr std::array<int, 5> RingRadiusMaterialCosts{{8, 14, 22, 32, 44}};
    constexpr std::array<int, 5> RingSpeedMaterialCosts{{8, 14, 22, 32, 44}};
    constexpr std::array<int, 5> CollectionRangeMaterialCosts{{6, 10, 16, 24, 34}};
    constexpr std::array<int, 3> RingPresetMaterialCosts{{12, 20, 30}};

    int cost = 0;
    switch (index) {
    case 0: cost = baseUpgradeCostForStep(warehouseCapacityLevel_, StorageMaterialCosts); break;
    case 1: cost = baseUpgradeCostForStep(std::max(0, merchantUpgradeLevel_ - 1), MerchantMaterialCosts); break;
    case 2: cost = baseUpgradeCostForStep(processingUnlockLevel_, ProcessingMaterialCosts); break;
    case 3: cost = ringWorkshopUnlocked_ ? 0 : 60; break;
    case 4: cost = baseUpgradeCostForStep(maxHpUpgradeLevel_, MaxHpMaterialCosts); break;
    case 5: cost = baseUpgradeCostForStep(ringRadiusUpgradeLevel_, RingRadiusMaterialCosts); break;
    case 6: cost = baseUpgradeCostForStep(ringSpeedUpgradeLevel_, RingSpeedMaterialCosts); break;
    case 7: cost = baseUpgradeCostForStep(collectionRangeUpgradeLevel_, CollectionRangeMaterialCosts); break;
    case 8: cost = baseUpgradeCostForStep(ringPresetSlotLevel_, RingPresetMaterialCosts); break;
    default: cost = 0; break;
    }
    return roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Trainer
        ? roguelikeAdjustedFacilityMaterialCost(cost)
        : cost;
}

const char* Game::upgradeName(int index) const
{
    switch (index) {
    case 0: return "収納箱容量強化";
    case 1: return "商人機能強化";
    case 2: return "作業台機能解禁";
    case 3: return "リング工房解禁";
    case 4: return "最大HPアップ";
    case 5: return "リング半径アップ";
    case 6: return "リング速度アップ";
    case 7: return "吸引強化";
    case 8: return "リングプリセット解禁";
    default: return "";
    }
}

const char* baseUpgradeDescription(int index)
{
    switch (index) {
    case 0:
        return "収納箱に保管できるアイテム数を増やします。";
    case 1:
        return "商人ワゴンの商品枠と買取機能を強化します。";
    case 2:
        return "作業台で扱える加工と加工費用の割引を増やします。";
    case 3:
        return "リング工房を拠点に開き、リング専用の調整を可能にします。";
    case 4:
        return "ルネの最大HPを増やします。";
    case 5:
        return "リング半径を広げます。";
    case 6:
        return "リング速度を上げます。";
    case 7:
        return "近くのドロップをルネへ引き寄せる範囲を広げます。";
    case 8:
        return "リング編成を保存して呼び出せる枠を解禁します。";
    default:
        return "";
    }
}

int Game::upgradeLevel(int index) const
{
    switch (index) {
    case 0: return warehouseCapacityLevel_;
    case 1: return std::max(0, merchantUpgradeLevel_ - 1);
    case 2: return processingUnlockLevel_;
    case 3: return ringWorkshopUnlocked_ ? 1 : 0;
    case 4: return maxHpUpgradeLevel_;
    case 5: return ringRadiusUpgradeLevel_;
    case 6: return ringSpeedUpgradeLevel_;
    case 7: return collectionRangeUpgradeLevel_;
    case 8: return ringPresetSlotLevel_;
    default: return 0;
    }
}

int Game::upgradeMaxLevel(int index) const
{
    switch (index) {
    case 0: return 4;
    case 1: return 6;
    case 2: return 5;
    case 3: return 1;
    case 4:
    case 5:
    case 6:
    case 7:
        return 5;
    case 8:
        return RingPresetSlotCount;
    default:
        return 0;
    }
}

bool Game::upgradeImplemented(int index) const
{
    return index >= 0 && index <= 8;
}

bool Game::upgradeMaxed(int index) const
{
    const int maxLevel = upgradeMaxLevel(index);
    return maxLevel <= 0 || upgradeLevel(index) >= maxLevel;
}

void Game::closeBaseFacilityScreens()
{
    baseMiningStartChoiceActive_ = false;
    baseWarpPointSelectActive_ = false;
    baseRegenerateConfirm_ = {};
    baseRoguelikeDepartureConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    baseStorageActive_ = false;
    baseStorageMode_ = StorageUiMode::Closed;
    baseStorageQuantityDialog_ = {};
    baseStorageQuantityPending_ = {};
    closeUiCommandMenu(baseStorageCommandMenu_);
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    closeUiCommandMenu(baseMerchantSellCommandMenu_);
    closeUiCommandMenu(baseMerchantBuyCommandMenu_);
    baseUpgradeActive_ = false;
    baseResultDialog_ = {};
    baseProcessingUiMode_ = ProcessingUiMode::Closed;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingConfirm_ = {};
    baseRingWorkshopActive_ = false;
    baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
    baseBookshelfActive_ = false;
    closeUiCommandMenu(bookshelfEndingCommandMenu_);
    baseDiaryActive_ = false;
}

void Game::clearBaseStoryPresentation()
{
    baseStoryFacilityOffsets_.clear();
    baseStoryCommand_ = {};
    baseStoryFadeAlpha_ = 0.0f;
    basePlayerSpriteWalking_ = false;
    updateBasePlayerSpriteFlipFromFacing();
}

void Game::renderBaseStoryFadeOverlay(Renderer& renderer) const
{
    if (baseStoryFadeAlpha_ <= 0.0f) {
        return;
    }

    renderer.setScreenSpace();
    renderer.fillRect(
        {0.0f, 0.0f},
        {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
        {0, 0, 0, alphaByte(255.0f * baseStoryFadeAlpha_)});
}

bool Game::storyEventUsesBasePresentation(std::string_view id) const
{
    const StoryEvent* event = findStoryEvent(id);
    if (event == nullptr) {
        return false;
    }

    return std::any_of(
        event->dialogue.steps.begin(),
        event->dialogue.steps.end(),
        [](const DialogueStep& step) {
            return step.kind == DialogueStepKind::Command && isBasePresentationCommand(step.command.name);
        });
}

void Game::updateBaseStoryPresentationCommand(float dt)
{
    const DialogueCommand* command = dialogue_.currentCommand();
    if (command == nullptr) {
        baseStoryCommand_ = {};
        return;
    }

    if (!isBasePresentationCommand(command->name) || !basePresentationActive()) {
        dialogue_.completeCurrentCommandStep();
        baseStoryCommand_ = {};
        return;
    }

    const int stepIndex = dialogue_.currentStepIndex();
    const bool newCommand = baseStoryCommand_.stepIndex != stepIndex || baseStoryCommand_.name != command->name;
    if (newCommand) {
        baseStoryCommand_ = {};
        baseStoryCommand_.stepIndex = stepIndex;
        baseStoryCommand_.name = command->name;
        baseStoryCommand_.startPosition = basePlayerPosition_;
        baseStoryCommand_.startFacing = lengthSquared(basePlayerFacing_) > 0.0001f
            ? normalize(basePlayerFacing_)
            : Vec2{0.0f, 1.0f};

        if (command->name == "base_actor_offset") {
            if (!command->args.empty()) {
                const float dx = parseStoryCommandFloat(*command, 1, 0.0f);
                const float dy = parseStoryCommandFloat(*command, 2, 0.0f);
                baseStoryFacilityOffsets_[command->args[0]] = storyTileOffset(dx, dy);
            }
            dialogue_.completeCurrentCommandStep();
            baseStoryCommand_ = {};
            return;
        }

        if (command->name == "base_actor_reset") {
            if (!command->args.empty()) {
                baseStoryFacilityOffsets_.erase(command->args[0]);
            }
            dialogue_.completeCurrentCommandStep();
            baseStoryCommand_ = {};
            return;
        }

        if (command->name == "base_player_place") {
            if (!command->args.empty()) {
                const std::string& placement = command->args[0];
                if (placement == "mine_exit_return") {
                    placeBasePlayerAtMineExitReturnPoint();
                } else if (placement == "home_door_resume") {
                    placeBasePlayerAtHomeDoorResumePoint();
                } else if (placement == "outdoor") {
                    baseArea_ = BaseArea::Outdoor;
                    basePlayerPosition_ = {
                        parseStoryCommandFloat(*command, 1, basePlayerPosition_.x),
                        parseStoryCommandFloat(*command, 2, basePlayerPosition_.y),
                    };
                    baseOutdoorPlayerPosition_ = basePlayerPosition_;
                    basePlayerFacing_ = {
                        parseStoryCommandFloat(*command, 3, basePlayerFacing_.x),
                        parseStoryCommandFloat(*command, 4, basePlayerFacing_.y),
                    };
                    if (lengthSquared(basePlayerFacing_) <= 0.0001f) {
                        basePlayerFacing_ = {0.0f, 1.0f};
                    }
                    updateBasePlayerSpriteFlipFromFacing();
                }
            }
            dialogue_.completeCurrentCommandStep();
            baseStoryCommand_ = {};
            return;
        }

        if (command->name == "base_fade") {
            baseStoryCommand_.targetPosition = baseStoryCommand_.startPosition;
            baseStoryCommand_.targetFacing = baseStoryCommand_.startFacing;
        } else if (command->name == "base_wait") {
            baseStoryCommand_.targetPosition = baseStoryCommand_.startPosition;
            baseStoryCommand_.targetFacing = baseStoryCommand_.startFacing;
        } else if (command->name == "base_player_walk") {
            const float dx = parseStoryCommandFloat(*command, 0, 0.0f);
            const float dy = parseStoryCommandFloat(*command, 1, 0.0f);
            baseStoryCommand_.targetPosition = baseStoryCommand_.startPosition + storyTileOffset(dx, dy);
            const Vec2 delta = baseStoryCommand_.targetPosition - baseStoryCommand_.startPosition;
            baseStoryCommand_.targetFacing = lengthSquared(delta) > 0.0001f
                ? normalize(delta)
                : baseStoryCommand_.startFacing;
        } else if (command->name == "base_player_lookaround") {
            baseStoryCommand_.targetPosition = baseStoryCommand_.startPosition;
            baseStoryCommand_.targetFacing = baseStoryCommand_.startFacing;
        }
    }

    const float safeDt = std::max(0.0f, dt);
    baseStoryCommand_.elapsedSeconds += safeDt;

    if (command->name == "base_fade") {
        const std::string direction = !command->args.empty() ? command->args[0] : "out";
        const float duration = std::max(0.001f, parseStoryCommandFloat(*command, 1, ScreenTransitionFadeOutSeconds));
        const float t = smoothStep01(baseStoryCommand_.elapsedSeconds / duration);
        if (direction == "in") {
            baseStoryFadeAlpha_ = 1.0f - t;
        } else {
            baseStoryFadeAlpha_ = t;
        }
        updateBasePlayerSpriteAnimation(safeDt, false);
        updateBasePlayerSpriteFlipFromFacing();
        if (baseStoryCommand_.elapsedSeconds >= duration) {
            baseStoryFadeAlpha_ = direction == "in" ? 0.0f : 1.0f;
            dialogue_.completeCurrentCommandStep();
            baseStoryCommand_ = {};
        }
        return;
    }

    if (command->name == "base_wait") {
        const float duration = std::max(0.0f, parseStoryCommandFloat(*command, 0, 0.0f));
        updateBasePlayerSpriteAnimation(safeDt, false);
        updateBasePlayerSpriteFlipFromFacing();
        if (baseStoryCommand_.elapsedSeconds >= duration) {
            dialogue_.completeCurrentCommandStep();
            baseStoryCommand_ = {};
        }
        return;
    }

    if (command->name == "base_player_walk") {
        const float explicitDuration = parseStoryCommandFloat(*command, 2, -1.0f);
        const float distance = length(baseStoryCommand_.targetPosition - baseStoryCommand_.startPosition);
        const float duration = explicitDuration > 0.0f
            ? explicitDuration
            : std::max(BaseStoryMinWalkSeconds, distance / std::max(1.0f, balance_.playerSpeed));
        const float t = duration > 0.0f ? clamp(baseStoryCommand_.elapsedSeconds / duration, 0.0f, 1.0f) : 1.0f;
        basePlayerPosition_ = baseStoryCommand_.startPosition +
            (baseStoryCommand_.targetPosition - baseStoryCommand_.startPosition) * t;
        basePlayerFacing_ = baseStoryCommand_.targetFacing;
        updateBasePlayerSpriteAnimation(safeDt, distance > 0.001f && t < 1.0f);
        updateBasePlayerSpriteFlipFromFacing();
        if (t >= 1.0f) {
            basePlayerPosition_ = baseStoryCommand_.targetPosition;
            basePlayerSpriteWalking_ = false;
            dialogue_.completeCurrentCommandStep();
            baseStoryCommand_ = {};
        }
        return;
    }

    if (command->name == "base_player_lookaround") {
        const float duration = std::max(0.1f, parseStoryCommandFloat(*command, 0, BaseStoryLookaroundSeconds));
        const float t = clamp(baseStoryCommand_.elapsedSeconds / duration, 0.0f, 1.0f);
        if (t < 0.25f) {
            basePlayerFacing_ = {-1.0f, 0.0f};
        } else if (t < 0.5f) {
            basePlayerFacing_ = {1.0f, 0.0f};
        } else if (t < 0.75f) {
            basePlayerFacing_ = {-1.0f, 0.0f};
        } else {
            basePlayerFacing_ = baseStoryCommand_.startFacing;
        }
        updateBasePlayerSpriteAnimation(safeDt, false);
        updateBasePlayerSpriteFlipFromFacing();
        if (t >= 1.0f) {
            basePlayerFacing_ = baseStoryCommand_.startFacing;
            updateBasePlayerSpriteFlipFromFacing();
            dialogue_.completeCurrentCommandStep();
            baseStoryCommand_ = {};
        }
    }
}

bool Game::roguelikeFacilityUiActive() const
{
    return roguelikeFacilityUiMode_ != RoguelikeFacilityUiMode::None;
}

int Game::roguelikeFacilityCostStep() const
{
    return astralRunActive() ? std::max(0, astralRun_.areaIndex) : 0;
}

int Game::roguelikeAdjustedFacilityMoneyCost(int baseCost) const
{
    if (baseCost <= 0) {
        return 0;
    }
    const double multiplier = 1.0 + static_cast<double>(roguelikeFacilityCostStep()) * 0.10;
    return std::max(1, static_cast<int>(std::ceil(static_cast<double>(baseCost) * multiplier)));
}

int Game::roguelikeAdjustedFacilityMaterialCost(int baseCost) const
{
    if (baseCost <= 0) {
        return 0;
    }
    const int step = roguelikeFacilityCostStep();
    return std::max(1, baseCost + (baseCost * step + 5) / 10);
}

int Game::roguelikeMerchantStockLimit() const
{
    return std::clamp(4 + roguelikeFacilityCostStep() / 2, 4, 8);
}

void Game::prepareRoguelikeMerchantStock()
{
    if (!roguelikeMerchantStockSuspended_) {
        suspendedMerchantStock_ = merchantStock_;
        suspendedMerchantStockVersion_ = merchantStockVersion_;
        roguelikeMerchantStockSuspended_ = true;
    }
    refreshMerchantStock(true);
    const int limit = roguelikeMerchantStockLimit();
    if (static_cast<int>(merchantStock_.size()) > limit) {
        merchantStock_.resize(static_cast<std::size_t>(limit));
    }
    baseMerchantBuySelection_ = std::clamp(baseMerchantBuySelection_, 0, std::max(0, static_cast<int>(merchantStock_.size()) - 1));
}

void Game::restoreRoguelikeMerchantStock()
{
    if (!roguelikeMerchantStockSuspended_) {
        return;
    }
    merchantStock_ = std::move(suspendedMerchantStock_);
    suspendedMerchantStock_.clear();
    merchantStockVersion_ = suspendedMerchantStockVersion_;
    suspendedMerchantStockVersion_ = 0;
    roguelikeMerchantStockSuspended_ = false;
}

void Game::openRoguelikeFacility(RoguelikeFacilityKind kind, std::string_view facilityId)
{
    closeBaseFacilityScreens();
    activeRoguelikeFacilityId_ = std::string(facilityId);
    switch (kind) {
    case RoguelikeFacilityKind::Merchant:
        roguelikeFacilityUiMode_ = RoguelikeFacilityUiMode::Merchant;
        prepareRoguelikeMerchantStock();
        baseSellActive_ = true;
        baseMerchantMode_ = MerchantUiMode::ChooseAction;
        baseMerchantActionSelection_ = 0;
        baseMerchantSellSource_ = 0;
        baseMerchantSellSourceTabs_.focusedIndex = baseMerchantSellSource_;
        baseSellSelection_ = 0;
        baseMerchantBuySelection_ = 0;
        closeUiCommandMenu(baseMerchantSellCommandMenu_);
        baseMerchantSellCommandSource_ = 0;
        baseMerchantSellCommandIndex_ = -1;
        closeUiCommandMenu(baseMerchantBuyCommandMenu_);
        baseMerchantBuyCommandIndex_ = -1;
        break;
    case RoguelikeFacilityKind::Artisan:
        roguelikeFacilityUiMode_ = RoguelikeFacilityUiMode::Artisan;
        baseProcessingUiMode_ = ProcessingUiMode::ChooseAction;
        baseProcessingActionSelection_ = 0;
        baseProcessingMode_ = static_cast<int>(ProcessingMode::Attack);
        baseProcessingTabs_.focusedIndex = baseProcessingMode_;
        baseProcessingSource_ = 0;
        baseProcessingSourceTabs_.focusedIndex = baseProcessingSource_;
        baseProcessingSelection_ = 0;
        closeUiCommandMenu(baseProcessingCommandMenu_);
        baseProcessingCommandSlot_ = -1;
        baseProcessingConfirm_ = {};
        baseProcessingConfirmTarget_ = {};
        break;
    case RoguelikeFacilityKind::Trainer:
        roguelikeFacilityUiMode_ = RoguelikeFacilityUiMode::Trainer;
        baseUpgradeActive_ = true;
        baseUpgradeSelection_ = baseUpgradeIndexForDisplay(true, 0);
        baseUpgradeTabs_ = {};
        baseUpgradeTabs_.focusedIndex = 0;
        break;
    }
    baseStatus_.clear();
}

void Game::closeRoguelikeFacilityUi()
{
    restoreRoguelikeMerchantStock();
    roguelikeFacilityUiMode_ = RoguelikeFacilityUiMode::None;
    activeRoguelikeFacilityId_.clear();
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    closeUiCommandMenu(baseMerchantSellCommandMenu_);
    closeUiCommandMenu(baseMerchantBuyCommandMenu_);
    baseProcessingUiMode_ = ProcessingUiMode::Closed;
    closeUiCommandMenu(baseProcessingCommandMenu_);
    baseProcessingConfirm_ = {};
    baseProcessingConfirmTarget_ = {};
    baseUpgradeActive_ = false;
}

bool Game::updateRoguelikeFacilityUi(const Input& input, UiContext& ui, float dt)
{
    if (!roguelikeFacilityUiActive()) {
        return false;
    }

    if (roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Merchant &&
        baseItemSourceIsWarehouse(baseMerchantSellSource_)) {
        baseMerchantSellSource_ = BaseBackpackSourceIndex;
        baseMerchantSellSourceTabs_.focusedIndex = baseMerchantSellSource_;
    }
    if (roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan &&
        baseItemSourceIsWarehouse(baseProcessingSource_)) {
        baseProcessingSource_ = BaseBackpackSourceIndex;
        baseProcessingSourceTabs_.focusedIndex = baseProcessingSource_;
    }

    updateBaseScreen(input, ui, dt);

    const bool panelStillOpen =
        baseSellActive_ ||
        baseProcessingUiMode_ != ProcessingUiMode::Closed ||
        baseUpgradeActive_ ||
        baseProcessingConfirm_.open ||
        baseResultDialog_.open;
    if (!panelStillOpen) {
        closeRoguelikeFacilityUi();
        baseStatus_.clear();
    }
    return true;
}

void Game::buyUpgrade(int index)
{
    if (!upgradeImplemented(index)) {
        baseStatus_ = "この強化枠は未実装です";
        return;
    }
    if (upgradeMaxed(index)) {
        baseStatus_ = "強化上限です";
        return;
    }
    const int cost = upgradeCost(index);
    if (cost <= 0) {
        return;
    }
    if (money_ < cost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    const MaterialType materialType = upgradeMaterialType(index);
    const int materialCost = upgradeMaterialCost(index);
    if (materialCost > 0 && inventory_.materialCount(materialType) < materialCost) {
        baseStatus_ = std::string(materialTypeDisplayName(materialType)) + "が足りません";
        return;
    }

    const int beforeLevel = upgradeLevel(index);
    money_ -= cost;
    if (materialCost > 0) {
        const bool spent = inventory_.materials().spend(materialType, materialCost);
        (void)spent;
    }
    switch (index) {
    case 0:
        ++warehouseCapacityLevel_;
        break;
    case 1:
        ++merchantUpgradeLevel_;
        refreshMerchantStock(true);
        break;
    case 2:
        ++processingUnlockLevel_;
        break;
    case 3:
        ringWorkshopUnlocked_ = true;
        break;
    case 4:
        ++maxHpUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 5:
        ++ringRadiusUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 6:
        ++ringSpeedUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 7:
        ++collectionRangeUpgradeLevel_;
        break;
    case 8:
        ringPresetSlotLevel_ = std::min(RingPresetSlotCount, ringPresetSlotLevel_ + 1);
        break;
    default:
        break;
    }
    const int afterLevel = upgradeLevel(index);
    baseStatus_.clear();
    if (index == 3 && beforeLevel == 0 && afterLevel > beforeLevel) {
        requestBaseAreaFade(
            BaseArea::Outdoor,
            baseHomeScreenDefaultPosition(balance_.playerRadius),
            {0.0f, 1.0f},
            "リング工房を建設しました",
            true);
        return;
    }
    openUiResultDialog(
        baseResultDialog_,
        "強化完了",
        baseUpgradeResultLines(index, beforeLevel, afterLevel));
}

void Game::openRingWorkshop()
{
    if (!ringWorkshopUnlocked_) {
        baseStatus_ = "リング工房はまだ解禁されていません";
        return;
    }
    baseRingWorkshopActive_ = true;
    baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
    baseRingWorkshopSelection_ = 0;
    baseRingWorkshopRingIndex_ = std::clamp(spellRing_.activeRingIndex(), 0, SpellRingCount - 1);
    baseRingWorkshopRingTabs_ = {};
    baseRingWorkshopUpgradeTabs_ = {};
    baseRingWorkshopUpgradeScrollOffset_ = 0.0f;
    baseRingWorkshopUpgradeScroll_ = {};
    resetRingWorkshopDraft();
    baseStatus_.clear();
}

void Game::resetRingWorkshopDraft()
{
    ringWorkshopDraftUpgradePoints_ = levelRingUpgradePoints_;
    ringWorkshopRespecSource_.reset();
}

int Game::ringLevelUpgradePointTotal() const
{
    return majo::ringLevelUpgradePointTotal(levelRingUpgradePoints_);
}

bool Game::ringWorkshopRespecChanged() const
{
    return ringWorkshopDraftUpgradePoints_ != levelRingUpgradePoints_;
}

int Game::ringWorkshopRespecMoneyCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    const int movedPoints = ringWorkshopRespecMovedPointCount(levelRingUpgradePoints_, ringWorkshopDraftUpgradePoints_);
    return RingWorkshopRespecBaseMoneyCost +
        ringLevelUpgradePointTotal() * RingWorkshopRespecMoneyCostPerTotalPoint +
        movedPoints * RingWorkshopRespecMoneyCostPerMovedPoint;
}

int Game::ringWorkshopRespecMoonCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    const int movedPoints = ringWorkshopRespecMovedPointCount(levelRingUpgradePoints_, ringWorkshopDraftUpgradePoints_);
    return RingWorkshopRespecBaseMoonCost +
        std::max(0, movedPoints - 1) / RingWorkshopRespecMovedPointsPerExtraMoon;
}

bool Game::adjustRingWorkshopRespec(RingLevelUpgradeSelection from, RingLevelUpgradeSelection to)
{
    if (ringLevelUpgradePointTotal() <= 0) {
        baseStatus_ = "再調整できるリング強化ポイントがありません";
        return false;
    }
    from.ringIndex = std::clamp(from.ringIndex, 0, SpellRingCount - 1);
    to.ringIndex = std::clamp(to.ringIndex, 0, SpellRingCount - 1);
    if (sameRingLevelUpgradeSelection(from, to)) {
        ringWorkshopRespecSource_.reset();
        return false;
    }

    RingLevelUpgradePoints& fromRingPoints = ringWorkshopDraftUpgradePoints_[static_cast<std::size_t>(from.ringIndex)];
    RingLevelUpgradePoints& toRingPoints = ringWorkshopDraftUpgradePoints_[static_cast<std::size_t>(to.ringIndex)];
    int& fromPoints = ringLevelUpgradePointRef(fromRingPoints, from.kind);
    int& toPoints = ringLevelUpgradePointRef(toRingPoints, to.kind);
    if (fromPoints <= 0) {
        baseStatus_ = "リング" + std::to_string(from.ringIndex + 1) + " " +
            ringLevelUpgradeKindName(from.kind) + "から移せるポイントがありません";
        return false;
    }
    --fromPoints;
    ++toPoints;
    ringWorkshopRespecSource_.reset();
    baseStatus_ = "配分案を変更しました。確定で支払います";
    return true;
}

void Game::confirmRingWorkshopRespec()
{
    if (!ringWorkshopRespecChanged()) {
        baseStatus_ = "配分は変更されていません";
        return;
    }
    const int moneyCost = ringWorkshopRespecMoneyCost();
    const int moonCost = ringWorkshopRespecMoonCost();
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    for (RingLevelUpgradePoints& points : ringWorkshopDraftUpgradePoints_) {
        points = clampedRingLevelUpgradePoints(points);
    }
    levelRingUpgradePoints_ = ringWorkshopDraftUpgradePoints_;
    ringWorkshopRespecSource_.reset();
    applyPermanentUpgrades();
    baseStatus_ = "リング強化の配分を再調整しました";
}

const char* Game::ringWorkshopUpgradeName(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::RadiusAdjust:
        return "半径調整";
    case RingWorkshopUpgrade::RadiusMax:
        return "リング半径の上限強化";
    case RingWorkshopUpgrade::RadiusMin:
        return "リング半径の下限強化";
    case RingWorkshopUpgrade::Speed:
        return "リング速度強化";
    case RingWorkshopUpgrade::WeightLimit:
        return "リング重量制限強化";
    case RingWorkshopUpgrade::ShiftDistance:
        return "ずらし距離強化";
    case RingWorkshopUpgrade::ThrowDistance:
        return "リング投げ距離強化";
    case RingWorkshopUpgrade::ThrowCooldown:
        return "リング投げクールダウン短縮";
    case RingWorkshopUpgrade::WeightPenalty:
        return "リング重量ペナルティ軽減";
    case RingWorkshopUpgrade::EquipSlot:
        return "リング装着枠増加";
    }
    return "";
}

int Game::ringWorkshopUpgradeLevel(RingWorkshopUpgrade upgrade) const
{
    const int ringIndex = std::clamp(baseRingWorkshopRingIndex_, 0, SpellRingCount - 1);
    const RingWorkshopRingUpgrades& upgrades = workshopRingUpgrades_[static_cast<std::size_t>(ringIndex)];
    switch (upgrade) {
    case RingWorkshopUpgrade::RadiusAdjust:
        return 0;
    case RingWorkshopUpgrade::RadiusMax:
        return upgrades.radiusMaxLevel;
    case RingWorkshopUpgrade::RadiusMin:
        return upgrades.radiusMinLevel;
    case RingWorkshopUpgrade::Speed:
        return upgrades.speedLevel;
    case RingWorkshopUpgrade::WeightLimit:
        return upgrades.weightLimitLevel;
    case RingWorkshopUpgrade::ShiftDistance:
        return upgrades.shiftDistanceLevel;
    case RingWorkshopUpgrade::ThrowDistance:
        return upgrades.throwDistanceLevel;
    case RingWorkshopUpgrade::ThrowCooldown:
        return upgrades.throwCooldownLevel;
    case RingWorkshopUpgrade::WeightPenalty:
        return upgrades.weightPenaltyLevel;
    case RingWorkshopUpgrade::EquipSlot:
        return upgrades.equipSlotLevel;
    }
    return 0;
}

int Game::ringWorkshopUpgradeMaxLevel(RingWorkshopUpgrade upgrade) const
{
    if (upgrade == RingWorkshopUpgrade::RadiusAdjust) {
        return 0;
    }
    return 5;
}

int Game::ringWorkshopUpgradeMoneyCost(RingWorkshopUpgrade upgrade) const
{
    constexpr std::array<int, 5> PremiumCosts{{500, 900, 1600, 2600, 4000}};
    constexpr std::array<int, 5> StandardCosts{{400, 750, 1300, 2100, 3300}};
    constexpr std::array<int, 5> UtilityCosts{{300, 550, 950, 1550, 2400}};

    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    switch (upgrade) {
    case RingWorkshopUpgrade::RadiusMax:
    case RingWorkshopUpgrade::RadiusMin:
    case RingWorkshopUpgrade::Speed:
    case RingWorkshopUpgrade::EquipSlot:
        return PremiumCosts[static_cast<std::size_t>(level)];
    case RingWorkshopUpgrade::WeightLimit:
    case RingWorkshopUpgrade::ThrowDistance:
    case RingWorkshopUpgrade::ThrowCooldown:
    case RingWorkshopUpgrade::WeightPenalty:
        return StandardCosts[static_cast<std::size_t>(level)];
    case RingWorkshopUpgrade::ShiftDistance:
        return UtilityCosts[static_cast<std::size_t>(level)];
    case RingWorkshopUpgrade::RadiusAdjust:
        return 0;
    }
    return 0;
}

int Game::ringWorkshopUpgradeMoonCost(RingWorkshopUpgrade upgrade) const
{
    constexpr std::array<int, 5> PremiumCosts{{3, 5, 8, 12, 17}};
    constexpr std::array<int, 5> StandardCosts{{2, 4, 6, 9, 13}};
    constexpr std::array<int, 5> UtilityCosts{{2, 3, 5, 7, 10}};

    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    switch (upgrade) {
    case RingWorkshopUpgrade::RadiusMax:
    case RingWorkshopUpgrade::RadiusMin:
    case RingWorkshopUpgrade::Speed:
    case RingWorkshopUpgrade::EquipSlot:
        return PremiumCosts[static_cast<std::size_t>(level)];
    case RingWorkshopUpgrade::WeightLimit:
    case RingWorkshopUpgrade::ThrowDistance:
    case RingWorkshopUpgrade::ThrowCooldown:
    case RingWorkshopUpgrade::WeightPenalty:
        return StandardCosts[static_cast<std::size_t>(level)];
    case RingWorkshopUpgrade::ShiftDistance:
        return UtilityCosts[static_cast<std::size_t>(level)];
    case RingWorkshopUpgrade::RadiusAdjust:
        return 0;
    }
    return 0;
}

float Game::ringWorkshopUpgradeCurrentValue(RingWorkshopUpgrade upgrade) const
{
    const int ringIndex = std::clamp(baseRingWorkshopRingIndex_, 0, SpellRingCount - 1);
    const RingWorkshopRingUpgrades& upgrades = workshopRingUpgrades_[static_cast<std::size_t>(ringIndex)];
    const RingLevelUpgradePoints points = clampedRingLevelUpgradePoints(
        levelRingUpgradePoints_[static_cast<std::size_t>(ringIndex)]);
    switch (upgrade) {
    case RingWorkshopUpgrade::RadiusAdjust:
        return metersToWorldDistance(ringWorkshopRadiusSettingForRing(ringIndex));
    case RingWorkshopUpgrade::RadiusMax:
        return metersToWorldDistance(ringWorkshopRadiusMaxForRing(ringIndex));
    case RingWorkshopUpgrade::RadiusMin:
        return metersToWorldDistance(ringWorkshopRadiusMinForRing(ringIndex));
    case RingWorkshopUpgrade::Speed:
        return linearMetersPerSecondForAngularSpeed(
            effectiveInitialRingSpeedForRing(ringIndex, points.speed) *
                SpellRingSystem::baseSpeedMultiplierForRing(ringIndex),
            effectiveInitialRingRadiusForRing(ringIndex, points.radius) *
                SpellRingSystem::baseRadiusMultiplierForRing(ringIndex));
    case RingWorkshopUpgrade::WeightLimit:
        return effectiveInitialRingWeightLimitForRing(ringIndex, points.weightLimit);
    case RingWorkshopUpgrade::ShiftDistance:
        return effectiveRingShiftDistanceForRing(ringIndex);
    case RingWorkshopUpgrade::ThrowDistance:
        return balance_.spellRingThrowDistance +
            metersToWorldDistance(static_cast<float>(upgrades.throwDistanceLevel) * RingWorkshopThrowDistanceMetersPerLevel);
    case RingWorkshopUpgrade::ThrowCooldown:
        return std::max(
            0.02f,
            balance_.spellRingThrowCooldown -
                static_cast<float>(upgrades.throwCooldownLevel) * RingWorkshopThrowCooldownSecondsPerLevel);
    case RingWorkshopUpgrade::WeightPenalty:
        return ringWeightPenaltyReliefPercentForLevel(upgrades.weightPenaltyLevel);
    case RingWorkshopUpgrade::EquipSlot:
        return static_cast<float>(24 + upgrades.equipSlotLevel * 2);
    }
    return 0.0f;
}

float Game::ringWorkshopUpgradeNextValue(RingWorkshopUpgrade upgrade) const
{
    const int currentLevel = ringWorkshopUpgradeLevel(upgrade);
    if (currentLevel >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return ringWorkshopUpgradeCurrentValue(upgrade);
    }
    switch (upgrade) {
    case RingWorkshopUpgrade::RadiusAdjust:
        return ringWorkshopUpgradeCurrentValue(upgrade);
    case RingWorkshopUpgrade::RadiusMax:
        return metersToWorldDistance(
            ringWorkshopRadiusMaxForRing(baseRingWorkshopRingIndex_) +
            RingWorkshopRadiusMaxMetersPerLevel[static_cast<std::size_t>(std::clamp(baseRingWorkshopRingIndex_, 0, SpellRingCount - 1))]);
    case RingWorkshopUpgrade::RadiusMin:
        return metersToWorldDistance(
            std::max(
                0.10f,
                ringWorkshopRadiusMinForRing(baseRingWorkshopRingIndex_) -
                    RingWorkshopRadiusMinMetersPerLevel[static_cast<std::size_t>(std::clamp(baseRingWorkshopRingIndex_, 0, SpellRingCount - 1))]));
    case RingWorkshopUpgrade::Speed: {
        const int ringIndex = std::clamp(baseRingWorkshopRingIndex_, 0, SpellRingCount - 1);
        return ringWorkshopUpgradeCurrentValue(upgrade) +
            RingWorkshopSpeedMetersPerSecondPerLevel[static_cast<std::size_t>(ringIndex)] *
                static_cast<float>(std::max(0.0, ringEquipmentModifiersForRing(equipmentModifiers_, ringIndex).ringSpeedMul));
    }
    case RingWorkshopUpgrade::WeightLimit:
        return ringWorkshopUpgradeCurrentValue(upgrade) + RingWorkshopWeightLimitKgPerLevel;
    case RingWorkshopUpgrade::ShiftDistance:
        return balance_.spellRingShiftDistance +
            metersToWorldDistance(static_cast<float>(currentLevel + 1) * RingWorkshopShiftDistanceMetersPerLevel);
    case RingWorkshopUpgrade::ThrowDistance:
        return balance_.spellRingThrowDistance +
            metersToWorldDistance(static_cast<float>(currentLevel + 1) * RingWorkshopThrowDistanceMetersPerLevel);
    case RingWorkshopUpgrade::ThrowCooldown:
        return std::max(
            0.02f,
            balance_.spellRingThrowCooldown -
                static_cast<float>(currentLevel + 1) * RingWorkshopThrowCooldownSecondsPerLevel);
    case RingWorkshopUpgrade::WeightPenalty:
        return ringWeightPenaltyReliefPercentForLevel(currentLevel + 1);
    case RingWorkshopUpgrade::EquipSlot:
        return static_cast<float>(24 + (currentLevel + 1) * 2);
    }
    return 0.0f;
}

std::string Game::ringWorkshopUpgradeValueText(RingWorkshopUpgrade upgrade, float value) const
{
    char valueBuffer[64];
    switch (upgrade) {
    case RingWorkshopUpgrade::RadiusAdjust:
    case RingWorkshopUpgrade::RadiusMax:
    case RingWorkshopUpgrade::RadiusMin:
    case RingWorkshopUpgrade::ShiftDistance:
    case RingWorkshopUpgrade::ThrowDistance:
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.2fm", worldDistanceToMeters(value));
        break;
    case RingWorkshopUpgrade::Speed:
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.2fm/s", value);
        break;
    case RingWorkshopUpgrade::WeightLimit:
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.1fkg", value);
        break;
    case RingWorkshopUpgrade::ThrowCooldown:
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.2fs", value);
        break;
    case RingWorkshopUpgrade::WeightPenalty:
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.0f%%", value);
        break;
    case RingWorkshopUpgrade::EquipSlot:
        std::snprintf(valueBuffer, sizeof(valueBuffer), "%.0f枠", value);
        break;
    }
    return valueBuffer;
}

std::vector<UiResultDialogLine> Game::ringWorkshopUpgradeResultLines(
    RingWorkshopUpgrade upgrade,
    int ringIndex,
    float beforeValue,
    float afterValue) const
{
    std::vector<UiResultDialogLine> lines;
    lines.push_back(baseUpgradeResultTextLine(
        "リング" + std::to_string(ringIndex + 1) + "の" + ringWorkshopUpgradeName(upgrade) + "を強化しました"));
    appendBaseUpgradeResultChangeLine(
        lines,
        "効果: ",
        ringWorkshopUpgradeValueText(upgrade, beforeValue),
        ringWorkshopUpgradeValueText(upgrade, afterValue),
        true);
    return lines;
}

Game::RingWorkshopUpgrade Game::ringWorkshopUpgradeForDisplayIndex(int index) const
{
    switch (index) {
    case 0:
        return RingWorkshopUpgrade::RadiusMax;
    case 1:
        return RingWorkshopUpgrade::RadiusMin;
    case 2:
        return RingWorkshopUpgrade::Speed;
    case 3:
        return RingWorkshopUpgrade::WeightLimit;
    case 4:
        return RingWorkshopUpgrade::ShiftDistance;
    case 5:
        return RingWorkshopUpgrade::ThrowDistance;
    case 6:
        return RingWorkshopUpgrade::ThrowCooldown;
    case 7:
        return RingWorkshopUpgrade::WeightPenalty;
    case 8:
    default:
        return RingWorkshopUpgrade::EquipSlot;
    }
}

float Game::ringWorkshopRadiusMinForRing(int ringIndex) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const RingLevelUpgradePoints points = clampedRingLevelUpgradePoints(
        levelRingUpgradePoints_[static_cast<std::size_t>(clampedRingIndex)]);
    const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringRadiusUpgradeLevel_) * 0.08f;
    const float levelMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(points.radius);
    const float staffMultiplier = static_cast<float>(std::max(
        0.0,
        ringEquipmentModifiersForRing(equipmentModifiers_, clampedRingIndex).ringRadiusMul));
    const float defaultOrbitMeters = worldDistanceToMeters(
        balance_.spellRingRadius *
        baseUpgradeMultiplier *
        levelMultiplier *
        SpellRingSystem::baseRadiusMultiplierForRing(clampedRingIndex) *
        staffMultiplier);
    return std::max(
        0.10f,
        defaultOrbitMeters -
            static_cast<float>(workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].radiusMinLevel) *
                RingWorkshopRadiusMinMetersPerLevel[static_cast<std::size_t>(clampedRingIndex)]);
}

float Game::ringWorkshopRadiusMaxForRing(int ringIndex) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const RingLevelUpgradePoints points = clampedRingLevelUpgradePoints(
        levelRingUpgradePoints_[static_cast<std::size_t>(clampedRingIndex)]);
    const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringRadiusUpgradeLevel_) * 0.08f;
    const float levelMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(points.radius);
    const float staffMultiplier = static_cast<float>(std::max(
        0.0,
        ringEquipmentModifiersForRing(equipmentModifiers_, clampedRingIndex).ringRadiusMul));
    const float defaultOrbitMeters = worldDistanceToMeters(
        balance_.spellRingRadius *
        baseUpgradeMultiplier *
        levelMultiplier *
        SpellRingSystem::baseRadiusMultiplierForRing(clampedRingIndex) *
        staffMultiplier);
    return std::max(
        ringWorkshopRadiusMinForRing(clampedRingIndex),
        defaultOrbitMeters +
            static_cast<float>(workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].radiusMaxLevel) *
                RingWorkshopRadiusMaxMetersPerLevel[static_cast<std::size_t>(clampedRingIndex)]);
}

float Game::ringWorkshopRadiusSettingForRing(int ringIndex) const
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const float minMeters = ringWorkshopRadiusMinForRing(clampedRingIndex);
    const float maxMeters = ringWorkshopRadiusMaxForRing(clampedRingIndex);
    float setting = workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].radiusSettingMeters;
    if (setting <= 0.0f) {
        setting = minMeters;
    }
    return std::clamp(setting, minMeters, maxMeters);
}

bool Game::setRingWorkshopRadiusSettingForRing(int ringIndex, float meters)
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
    const float minMeters = ringWorkshopRadiusMinForRing(clampedRingIndex);
    const float maxMeters = ringWorkshopRadiusMaxForRing(clampedRingIndex);
    if (maxMeters <= minMeters + 0.001f) {
        workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].radiusSettingMeters = minMeters;
        return false;
    }
    const float clamped = std::clamp(meters, minMeters, maxMeters);
    float& setting = workshopRingUpgrades_[static_cast<std::size_t>(clampedRingIndex)].radiusSettingMeters;
    const bool changed = std::abs(setting - clamped) > 0.001f;
    setting = clamped;
    if (changed) {
        applyPermanentUpgrades();
        baseStatus_ = "リング半径を調整しました";
    }
    return changed;
}

void Game::buyRingWorkshopUpgrade(RingWorkshopUpgrade upgrade)
{
    if (upgrade == RingWorkshopUpgrade::RadiusAdjust) {
        baseStatus_ = "半径はゲージで調整してください";
        return;
    }
    if (ringWorkshopUpgradeLevel(upgrade) >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        baseStatus_ = "この強化は上限です";
        return;
    }
    const int moneyCost = ringWorkshopUpgradeMoneyCost(upgrade);
    const int moonCost = ringWorkshopUpgradeMoonCost(upgrade);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    const int ringIndex = std::clamp(baseRingWorkshopRingIndex_, 0, SpellRingCount - 1);
    const float beforeValue = ringWorkshopUpgradeCurrentValue(upgrade);
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    RingWorkshopRingUpgrades& upgrades = workshopRingUpgrades_[static_cast<std::size_t>(ringIndex)];
    switch (upgrade) {
    case RingWorkshopUpgrade::RadiusMax:
        ++upgrades.radiusMaxLevel;
        upgrades.radiusSettingMeters = ringWorkshopRadiusMaxForRing(ringIndex);
        break;
    case RingWorkshopUpgrade::RadiusMin:
        ++upgrades.radiusMinLevel;
        upgrades.radiusSettingMeters = ringWorkshopRadiusSettingForRing(ringIndex);
        break;
    case RingWorkshopUpgrade::Speed:
        ++upgrades.speedLevel;
        break;
    case RingWorkshopUpgrade::WeightLimit:
        ++upgrades.weightLimitLevel;
        break;
    case RingWorkshopUpgrade::ShiftDistance:
        ++upgrades.shiftDistanceLevel;
        break;
    case RingWorkshopUpgrade::ThrowDistance:
        ++upgrades.throwDistanceLevel;
        break;
    case RingWorkshopUpgrade::ThrowCooldown:
        ++upgrades.throwCooldownLevel;
        break;
    case RingWorkshopUpgrade::WeightPenalty:
        ++upgrades.weightPenaltyLevel;
        break;
    case RingWorkshopUpgrade::EquipSlot:
        ++upgrades.equipSlotLevel;
        break;
    case RingWorkshopUpgrade::RadiusAdjust:
        break;
    }
    applyPermanentUpgrades();
    resetRingWorkshopDraft();
    const float afterValue = ringWorkshopUpgradeCurrentValue(upgrade);
    baseStatus_.clear();
    openUiResultDialog(
        baseResultDialog_,
        "強化完了",
        ringWorkshopUpgradeResultLines(upgrade, ringIndex, beforeValue, afterValue));
}

void Game::openBookshelf()
{
    syncEncyclopediaFromInventoryAndRing();
    if (encyclopediaComplete() && !hasStoryFlag(StoryEndingEncyclopediaCompleteFlag)) {
        const EndingKind encyclopediaEndingKind = resolveEndingKamishibaiKind(EndingKind::EncyclopediaComplete);
        if (encyclopediaEndingKind == EndingKind::EncyclopediaComplete ||
            !hasStoryFlag(StoryEndingEncyclopediaFailedTrustFlag)) {
            requestEndingKamishibai(encyclopediaEndingKind);
            return;
        }
    }

    baseBookshelfActive_ = true;
    bookshelfPage_ = BookshelfPage::Menu;
    bookshelfSelection_ = 0;
    bookshelfScrollOffset_ = 0.0f;
    bookshelfScrollState_ = {};
    closeUiCommandMenu(bookshelfEndingCommandMenu_);
    baseStatus_.clear();
}

std::vector<EndingKind> Game::bookshelfEndingReplayChoices() const
{
    std::vector<EndingKind> choices;
    if (hasStoryFlag(StoryTrustBrokenFlag) || hasStoryFlag(HiddenEndingPeopleGoneFlag)) {
        return choices;
    }
    if (hasStoryFlag(StoryEndingMainFlag) || hasStoryFlag(StoryEndingSeenFlag)) {
        choices.push_back(EndingKind::Main);
    }
    if (hasStoryFlag(StoryEndingEncyclopediaCompleteFlag)) {
        choices.push_back(EndingKind::EncyclopediaComplete);
    }
    if (hasStoryFlag(StoryEndingAstralClearFlag)) {
        choices.push_back(EndingKind::AstralClear);
    }
    return choices;
}

int Game::bookshelfMenuItemCount() const
{
    return BookshelfMenuItemCount + (bookshelfEndingReplayChoices().empty() ? 0 : 1);
}

bool Game::encyclopediaComplete() const
{
    int targetCount = 0;
    int discoveredCount = 0;
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        if (isCodexHiddenObject(object)) {
            continue;
        }
        ++targetCount;
        const bool treasure = object.category == "\xE5\xAE\x9D";
        if (encyclopedia_.objectStage(object.id, treasure) != EncyclopediaStage::Undiscovered) {
            ++discoveredCount;
        }
    }
    for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
        if (isCodexHiddenEnemy(enemy)) {
            continue;
        }
        ++targetCount;
        if (encyclopedia_.enemyStage(enemy.id) != EncyclopediaStage::Undiscovered) {
            ++discoveredCount;
        }
    }
    return targetCount > 0 && discoveredCount >= targetCount;
}

void Game::syncEncyclopediaFromInventoryAndRing()
{
    if (!gameplayRewardsEnabled()) {
        return;
    }

    std::unordered_map<std::string, int> ownedCounts;
    std::unordered_map<std::string, const ObjectDefinition*> ownedObjects;
    const auto addOwnedObject = [&ownedCounts, &ownedObjects](const ObjectDefinition& object, int count) {
        if (object.id.empty() || count <= 0 || isCodexHiddenObject(object)) {
            return;
        }
        ownedCounts[object.id] += count;
        ownedObjects.try_emplace(object.id, &object);
    };

    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        if (!stack.objectId.empty() && stack.count > 0) {
            addOwnedObject(stack.item, stack.count);
        }
    }
    for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
        if (!objectInstance.item.id.empty()) {
            addOwnedObject(objectInstance.item, 1);
        }
    }
    for (const InventoryObjectStack& stack : warehouseObjectStacks_) {
        if (!stack.objectId.empty() && stack.count > 0) {
            addOwnedObject(stack.item, stack.count);
        }
    }
    for (const InventoryObjectInstance& objectInstance : warehouseObjectInstances_) {
        if (!objectInstance.item.id.empty()) {
            addOwnedObject(objectInstance.item, 1);
        }
    }
    for (const auto& [objectId, count] : ownedCounts) {
        const int suppressCount = [&]() {
            const auto it = encyclopediaOwnedSyncSuppressCounts_.find(objectId);
            return it == encyclopediaOwnedSyncSuppressCounts_.end() ? 0 : it->second;
        }();
        if (count <= suppressCount) {
            continue;
        }
        const auto objectIt = ownedObjects.find(objectId);
        if (objectIt != ownedObjects.end() && objectIt->second != nullptr) {
            encyclopedia_.noteItemObtained(*objectIt->second, player_.position);
        }
    }

    std::unordered_map<std::string, int> ringCounts;
    std::unordered_map<std::string, const ObjectDefinition*> ringObjects;
    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->objectId.empty()) {
            continue;
        }
        const ObjectDefinition* object = objectCatalog_.registry.findById(itemPtr->objectId);
        if (object != nullptr && !isCodexHiddenObject(*object)) {
            ringCounts[object->id] += 1;
            ringObjects.try_emplace(object->id, object);
        }
    }
    for (const auto& [objectId, count] : ringCounts) {
        const int suppressCount = [&]() {
            const auto it = encyclopediaRingSyncSuppressCounts_.find(objectId);
            return it == encyclopediaRingSyncSuppressCounts_.end() ? 0 : it->second;
        }();
        if (count <= suppressCount) {
            continue;
        }
        const auto objectIt = ringObjects.find(objectId);
        if (objectIt != ringObjects.end() && objectIt->second != nullptr) {
            encyclopedia_.noteItemEquipped(*objectIt->second, player_.position);
        }
    }
}

void Game::captureEncyclopediaSyncSuppressState()
{
    encyclopediaOwnedSyncSuppressCounts_.clear();
    encyclopediaRingSyncSuppressCounts_.clear();

    const auto addOwnedId = [this](std::string_view objectId, int count) {
        if (objectId.empty() || count <= 0) {
            return;
        }
        const ObjectDefinition* object = objectCatalog_.registry.findById(objectId);
        if (object != nullptr && isCodexHiddenObject(*object)) {
            return;
        }
        encyclopediaOwnedSyncSuppressCounts_[std::string(objectId)] += count;
    };
    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        addOwnedId(stack.objectId, stack.count);
    }
    for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
        addOwnedId(objectInstance.item.id, 1);
    }

    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->objectId.empty()) {
            continue;
        }
        const ObjectDefinition* object = objectCatalog_.registry.findById(itemPtr->objectId);
        if (object != nullptr && isCodexHiddenObject(*object)) {
            continue;
        }
        encyclopediaRingSyncSuppressCounts_[itemPtr->objectId] += 1;
    }
}

void Game::applyEffectDiscoveries(const std::vector<EffectDiscoveryEvent>& discoveries)
{
    if (!shouldRecordEffectDiscoveries()) {
        return;
    }

    if (encyclopedia_.noteEffectEvents(discoveries, objectCatalog_) > 0) {
        playAudioSe(AudioSeEffectDiscovery);
    }
}

void Game::recordMainObjectObtained(std::string_view objectId)
{
    if (!shouldRecordMainProgressKnowledge() || objectId.empty()) {
        return;
    }
    const ObjectDefinition* object = objectCatalog_.registry.findById(objectId);
    if (object == nullptr || isCodexHiddenObject(*object)) {
        return;
    }
    mainObtainedObjectIds_.insert(std::string(objectId));
}

void Game::recordMainCapturedEnemy(std::string_view enemyId)
{
    if (!shouldRecordMainProgressKnowledge() || enemyId.empty()) {
        return;
    }
    const auto enemyIt = enemyCatalog_.enemiesById.find(std::string(enemyId));
    if (enemyIt == enemyCatalog_.enemiesById.end() || isCodexHiddenEnemy(enemyIt->second)) {
        return;
    }
    mainCapturedEnemyIds_.insert(std::string(enemyId));
}

void Game::recordObjectObtainedForFirstNotice(
    std::string_view objectId,
    std::string_view instanceId,
    bool protectable,
    Vec2 position)
{
    if (!gameplayRewardsEnabled() || objectId.empty()) {
        return;
    }
    const ObjectDefinition* object = objectCatalog_.registry.findById(objectId);
    if (object == nullptr) {
        return;
    }
    recordMainObjectObtained(objectId);
    const bool codexHidden = isCodexHiddenObject(*object);
    if (!codexHidden && !encyclopedia_.noteItemObtained(*object, position)) {
        return;
    }

    const bool playJingle = firstItemAcquisitionNotices_.empty();
    firstItemAcquisitionNotices_.push_back(AcquisitionNotice{
        .kind = AcquisitionNoticeKind::Object,
        .title = codexHidden ? "入手した！" : "はじめて入手した！",
        .objectId = std::string(objectId),
        .instanceId = std::string(instanceId),
        .protectable = protectable && !instanceId.empty(),
    });
    if (playJingle) {
        playAudioJingle(
            AudioSeNewItemJingle,
            NewItemJingleFallbackSeconds,
            0.06f,
            0.22f,
            1.0f,
            1.0f);
    }
}

void Game::recordRewardObjectAcquisitionNotice(
    std::string_view objectId,
    std::string_view instanceId,
    bool protectable,
    Vec2 position)
{
    if (!gameplayRewardsEnabled() || objectId.empty()) {
        return;
    }
    const ObjectDefinition* object = objectCatalog_.registry.findById(objectId);
    if (object == nullptr) {
        return;
    }
    recordMainObjectObtained(objectId);
    (void)encyclopedia_.noteItemObtained(*object, position);

    const bool playJingle = firstItemAcquisitionNotices_.empty();
    firstItemAcquisitionNotices_.push_back(AcquisitionNotice{
        .kind = AcquisitionNoticeKind::Object,
        .title = "お礼をもらった！",
        .objectId = std::string(objectId),
        .instanceId = std::string(instanceId),
        .protectable = protectable && !instanceId.empty(),
    });
    if (playJingle) {
        playAudioJingle(
            AudioSeNewItemJingle,
            NewItemJingleFallbackSeconds,
            0.06f,
            0.22f,
            1.0f,
            1.0f);
    }
}

void Game::recordRewardMaterialAcquisitionNotice(MaterialType materialType, int amount)
{
    if (!gameplayRewardsEnabled() || amount <= 0 || materialType == MaterialType::Count) {
        return;
    }

    const bool playJingle = firstItemAcquisitionNotices_.empty();
    firstItemAcquisitionNotices_.push_back(AcquisitionNotice{
        .kind = AcquisitionNoticeKind::Material,
        .title = "お礼をもらった！",
        .materialType = materialType,
        .amount = amount,
    });
    if (playJingle) {
        playAudioJingle(
            AudioSeNewItemJingle,
            NewItemJingleFallbackSeconds,
            0.06f,
            0.22f,
            1.0f,
            1.0f);
    }
}

void Game::recordRewardMoneyAcquisitionNotice(int amount)
{
    if (!gameplayRewardsEnabled() || amount <= 0) {
        return;
    }

    const bool playJingle = firstItemAcquisitionNotices_.empty();
    firstItemAcquisitionNotices_.push_back(AcquisitionNotice{
        .kind = AcquisitionNoticeKind::Money,
        .title = "お礼をもらった！",
        .amount = amount,
    });
    if (playJingle) {
        playAudioJingle(
            AudioSeNewItemJingle,
            NewItemJingleFallbackSeconds,
            0.06f,
            0.22f,
            1.0f,
            1.0f);
    }
}

bool Game::firstItemAcquisitionNoticeActive() const
{
    return !firstItemAcquisitionNotices_.empty();
}

void Game::closeFirstItemAcquisitionNotice()
{
    if (!firstItemAcquisitionNotices_.empty()) {
        firstItemAcquisitionNotices_.pop_front();
    }
}

void Game::addStoryFlag(std::string flag)
{
    if (flag.empty()) {
        return;
    }
    if (std::find(storyFlags_.begin(), storyFlags_.end(), flag) == storyFlags_.end()) {
        storyFlags_.push_back(std::move(flag));
    }
}

bool Game::hasStoryFlag(std::string_view flag) const
{
    return std::find(storyFlags_.begin(), storyFlags_.end(), std::string(flag)) != storyFlags_.end();
}

void Game::unlockHiddenBaseOrbitCorruption()
{
    if (!hasStoryFlag(StoryHiddenOrbitCorruptionUnlockedFlag)) {
        addStoryFlag(std::string(StoryHiddenOrbitCorruptionUnlockedFlag));
        baseStatus_ = "リングが、拠点でも止まらなくなった";
    }
    player_.position = basePlayerPosition_;
    player_.facing = lengthSquared(basePlayerFacing_) > 0.0001f
        ? normalize(basePlayerFacing_)
        : Vec2{0.0f, 1.0f};
    player_.spellRingShift = 0.0f;
    player_.spellRingShiftDirection = player_.facing;
    spellRing_.resetRuntimeStateAtPlayer(player_, balance_);
}

bool Game::hiddenBaseOrbitActive() const
{
    return hasStoryFlag(StoryHiddenOrbitCorruptionUnlockedFlag) &&
        !hasStoryFlag(StoryHiddenEndingEverythingOrbitsFlag) &&
        !hasStoryFlag(HiddenEndingPeopleGoneFlag);
}

bool Game::hiddenBaseNpcRemoved(std::string_view facilityId) const
{
    return hasStoryFlag(hiddenBaseNpcRemovedFlag(facilityId));
}

bool Game::hiddenBaseAllNonMonicaNpcsRemoved() const
{
    return std::all_of(
        HiddenBaseNpcDefinitions.begin(),
        HiddenBaseNpcDefinitions.end(),
        [this](const HiddenBaseNpcDefinition& definition) {
            return definition.monica || hiddenBaseNpcRemoved(definition.facilityId);
        });
}

bool Game::storeCapturedEnemyDefinition(const EnemyDefinition& enemy)
{
    ObjectDefinition captured = makeCapturedObjectDefinition(enemy);
    upsertObjectDefinition(objectCatalog_, captured);

    SpellRingAddResult ringAdd{};
    if (spellRing_.addObjectItem(captured, &ringAdd)) {
        refreshOrbitEffects();
        syncEncyclopediaFromInventoryAndRing();
        return true;
    }

    InventoryAddResult inventoryAdd{};
    if (inventory_.addRuntimeObjectItem(captured, &inventoryAdd)) {
        syncEncyclopediaFromInventoryAndRing();
        return true;
    }

    if (warehouseUsedSlots() < warehouseCapacity()) {
        ItemInstance instance = inventory_.createDetachedObjectInstance(captured);
        warehouseObjectInstances_.push_back(InventoryObjectInstance{captured, std::move(instance)});
        syncWarehouseDisplaySlots();
        syncEncyclopediaFromInventoryAndRing();
        return true;
    }

    return false;
}

void Game::updateHiddenBaseOrbit(const Input& input, UiContext& ui, float dt, bool interactionsEnabled)
{
    if (!hiddenBaseOrbitActive()) {
        return;
    }

    player_.position = basePlayerPosition_;
    player_.facing = lengthSquared(basePlayerFacing_) > 0.0001f
        ? normalize(basePlayerFacing_)
        : Vec2{0.0f, 1.0f};
    player_.spellRingShift = 0.0f;
    player_.spellRingShiftDirection = player_.facing;

    if (interactionsEnabled) {
        spellRing_.update(player_, input, dt, baseRingPreviewAnimationTime_, false, ui.pointerConsumed(), balance_);
    } else {
        spellRing_.updatePresentation(player_, dt, balance_);
        return;
    }

    for (auto& [facilityId, cooldown] : hiddenBaseNpcHitCooldowns_) {
        cooldown = std::max(0.0f, cooldown - std::max(0.0f, dt));
    }

    if (baseArea_ != BaseArea::Outdoor) {
        return;
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    applyHiddenRouteFacilityAvailability(
        facilities,
        hasStoryFlag(HiddenEndingPeopleGoneFlag),
        hasStoryFlag(StoryHiddenOrbitCorruptionUnlockedFlag),
        hiddenBaseNpcRemoved("merchant_npc"),
        hiddenBaseNpcRemoved("processor_npc"),
        hiddenBaseNpcRemoved("elder"));
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    applyBaseStoryFacilityOffsets(facilities, baseStoryFacilityOffsets_);

    auto markTrustBroken = [this]() {
        if (hasStoryFlag(StoryTrustBrokenFlag)) {
            return;
        }
        addStoryFlag(std::string(StoryTrustBrokenFlag));
        std::string message;
        if (!saveSaveData(message)) {
            logError("[hidden] trust break save failed: " + message);
        }
    };

    auto removeNpc = [this, &markTrustBroken](std::string_view facilityId, const EnemyDefinition& enemy, bool captured) {
        markTrustBroken();
        if (captured) {
            const bool stored = storeCapturedEnemyDefinition(enemy);
            baseStatus_ = stored
                ? ((enemy.name.empty() ? enemy.id : enemy.name) + "を捕獲した")
                : ((enemy.name.empty() ? enemy.id : enemy.name) + "はリングに吸い込まれた");
        } else {
            baseStatus_ = (enemy.name.empty() ? enemy.id : enemy.name) + "を撃破した";
        }

        addStoryFlag(hiddenBaseNpcRemovedFlag(facilityId));
        hiddenBaseNpcHp_.erase(std::string(facilityId));
        hiddenBaseNpcHitCooldowns_.erase(std::string(facilityId));

        if (hiddenBaseAllNonMonicaNpcsRemoved() && !hasStoryFlag(StoryHiddenMonicaDuelUnlockedFlag)) {
            addStoryFlag(std::string(StoryHiddenMonicaDuelUnlockedFlag));
            queueStoryEventForTrigger(std::string(HiddenMonicaDuelIntroTrigger));
        }

        std::string message;
        if (!saveSaveData(message)) {
            logError("[hidden] npc removal save failed: " + message);
        }
    };

    std::vector<SpellRingItem*> runtimeItems = spellRing_.runtimeItemsMutable();
    for (const BaseFacility& facility : facilities) {
        const std::string_view facilityId = facility.facilityId;
        if (!hiddenBaseNpcTargetFacility(facilityId)) {
            continue;
        }
        const HiddenBaseNpcDefinition* npcDefinition = hiddenBaseNpcDefinitionForFacility(facilityId);
        if (npcDefinition == nullptr) {
            continue;
        }
        const auto enemyIt = enemyCatalog_.enemiesById.find(std::string(npcDefinition->enemyId));
        if (enemyIt == enemyCatalog_.enemiesById.end()) {
            continue;
        }

        const EnemyDefinition& enemy = enemyIt->second;
        int& hp = hiddenBaseNpcHp_[std::string(facilityId)];
        if (hp <= 0) {
            hp = std::max(1, enemy.hp);
        }
        float& cooldown = hiddenBaseNpcHitCooldowns_[std::string(facilityId)];
        if (cooldown > 0.0f) {
            continue;
        }

        const UiRect targetRect = baseFacilityPointerRect(facility, baseArea_, ringWorkshopUnlocked_);
        for (SpellRingItem* itemPtr : runtimeItems) {
            if (itemPtr == nullptr || itemPtr->broken() || itemPtr->objectId.empty()) {
                continue;
            }
            SpellRingItem& item = *itemPtr;
            const float hitRadius = std::max(6.0f, item.hitRadius);
            if (!circleIntersectsRect(item.worldPosition, hitRadius, targetRect)) {
                continue;
            }

            cooldown = std::max(HiddenBaseNpcHitCooldownSeconds, item.hitInterval);
            item.actionFlashTimer = SpellRingItemActionFlashSeconds;
            (void)spellRing_.consumeItemDurability(item, 1);

            if (hiddenRouteCaptureNetObject(item.objectId)) {
                removeNpc(facilityId, enemy, true);
            } else {
                markTrustBroken();
                hp -= std::max(1, item.damage);
                baseStatus_ = (enemy.name.empty() ? enemy.id : enemy.name) + "に攻撃した";
                if (hp <= 0) {
                    removeNpc(facilityId, enemy, false);
                }
            }
            (void)spellRing_.consumeItemBreakEvents();
            break;
        }
    }
}

void Game::renderHiddenBaseOrbit(Renderer& renderer) const
{
    if (!hiddenBaseOrbitActive()) {
        return;
    }
    renderSpellRingForeground(renderer, spellRing_.runtimeItems(), {}, baseRingPreviewAnimationTime_);
}

void Game::recordBaseHintDungeonReturn()
{
    if (!hasStoryFlag(BaseReturnCount1Flag)) {
        addStoryFlag(std::string(BaseReturnCount1Flag));
    } else if (!hasStoryFlag(BaseReturnCount2Flag)) {
        addStoryFlag(std::string(BaseReturnCount2Flag));
    }
}

bool Game::queueBaseHintEventOnReturn(std::string_view returnedStageId, bool stageCleared)
{
    const auto canBuyUpgrade = [this](int index) {
        if (!upgradeImplemented(index) || upgradeMaxed(index)) {
            return false;
        }

        const int moneyCost = upgradeCost(index);
        const int materialCost = upgradeMaterialCost(index);
        const MaterialType materialType = upgradeMaterialType(index);
        return moneyCost > 0 &&
            money_ >= moneyCost &&
            (materialCost <= 0 || inventory_.materialCount(materialType) >= materialCost);
    };

    const auto queueHint = [this](std::string_view trigger) {
        return queueStoryEventForTrigger(std::string(trigger));
    };

    const bool backpackFull = backpackUsedSlots() >= inventory_.screenSlotCount();
    if (backpackFull) {
        if (!hasStoryFlag(BaseStorageTutorialFlag) && queueHint(BaseHintStorageFullTrigger)) {
            return true;
        }
        if (!hasStoryFlag(BaseMerchantTutorialFlag) && queueHint(BaseHintMerchantFullTrigger)) {
            return true;
        }
    }

    if (canBuyUpgrade(BaseUpgradeRingWorkshopIndex) && queueHint(BaseHintRingWorkshopBuildableTrigger)) {
        return true;
    }
    if (canBuyUpgrade(BaseUpgradeRingPresetIndex) && queueHint(BaseHintRingPresetReadyTrigger)) {
        return true;
    }
    if (!hasStoryFlag(BaseProcessingTutorialFlag) &&
        money_ >= BaseHintMoneyThreshold &&
        inventory_.materialCount(MaterialType::EnhancementOre) >= BaseHintMaterialThreshold &&
        queueHint(BaseHintProcessingReadyTrigger)) {
        return true;
    }
    if (!hasStoryFlag(BaseForgeTutorialFlag) &&
        money_ >= BaseHintMoneyThreshold &&
        (inventory_.materialCount(MaterialType::OldWoodBuildingMaterial) >= BaseHintMaterialThreshold ||
            inventory_.materialCount(MaterialType::ManaDrop) >= BaseHintMaterialThreshold) &&
        queueHint(BaseHintForgeReadyTrigger)) {
        return true;
    }
    if (!hasStoryFlag(BaseDiaryTutorialFlag) &&
        hasStoryFlag(BaseReturnCount2Flag) &&
        queueHint(BaseHintDiarySaveTrigger)) {
        return true;
    }

    const bool stage1ClearedNow = stageCleared && stageClearFlagForStage(returnedStageId) == "stage_clear_1";
    return stage1ClearedNow &&
        !hasStoryFlag(BaseBookshelfTutorialFlag) &&
        queueHint(BaseHintBookshelfStage1Trigger);
}

void Game::startBaseMonicaDialogue()
{
    baseStatus_.clear();
    if (startBaseTalkStoryEvent("monica", {})) {
        return;
    }
    dialogue_.start(baseMonicaDialogue());
}

void Game::startBaseElderDialogue()
{
    baseStatus_.clear();
    if (startBaseTalkStoryEvent("elder", {})) {
        return;
    }
    dialogue_.start(baseElderDialogue());
}

void Game::clearBaseTalkSessionSelections()
{
    baseTalkSessionSelections_.clear();
}

std::string Game::selectBaseRandomTalkEventId(std::string_view speakerId)
{
    const std::string trigger = baseRandomTalkTrigger(speakerId);
    if (trigger.empty()) {
        return {};
    }

    const std::string speakerKey{speakerId};
    auto cached = baseTalkSessionSelections_.find(speakerKey);
    if (cached != baseTalkSessionSelections_.end()) {
        if (findStoryEvent(cached->second) != nullptr) {
            return cached->second;
        }
        baseTalkSessionSelections_.erase(cached);
    }

    std::vector<std::string> eventIds;
    for (const StoryEvent& event : storyEvents_) {
        if (event.trigger == trigger) {
            eventIds.push_back(event.id);
        }
    }
    if (eventIds.empty()) {
        return {};
    }

    std::uniform_int_distribution<std::size_t> distribution(0, eventIds.size() - 1);
    std::string selected = eventIds[distribution(baseTalkSessionRng())];
    baseTalkSessionSelections_[speakerKey] = selected;
    return selected;
}

std::string Game::baseTalkStoryTrigger(std::string_view speakerId) const
{
    if (speakerId.empty()) {
        return {};
    }

    const std::string speaker{speakerId};
    if (hasStoryFlag("ending_seen")) {
        const std::string postEndingTrigger = "base_talk:" + speaker + ":post_ending";
        if (findStoryEventForTrigger(postEndingTrigger) != nullptr) {
            return postEndingTrigger;
        }
    }

    const int progress = std::clamp(unlockedStages_, 1, 4);
    return "base_talk:" + speaker + ":progress_" + std::to_string(progress);
}

bool Game::startBaseTalkStoryEvent(std::string_view speakerId, std::function<void()> onComplete)
{
    if (isBaseRandomTalkSpeaker(speakerId)) {
        const std::string randomEventId = selectBaseRandomTalkEventId(speakerId);
        if (!randomEventId.empty()) {
            return startStoryEventWithCompletion(randomEventId, std::move(onComplete));
        }
    }

    const std::string trigger = baseTalkStoryTrigger(speakerId);
    if (trigger.empty()) {
        return false;
    }

    const StoryEvent* event = findStoryEventForTrigger(trigger);
    if (event == nullptr) {
        return false;
    }

    return startStoryEventWithCompletion(event->id, std::move(onComplete));
}

bool Game::hasBrokenRingItemForDeparture() const
{
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        for (const SpellRingItem& item : spellRing_.itemsForRing(ringIndex)) {
            if (item.broken()) {
                return true;
            }
        }
    }
    return false;
}

void Game::openBaseMiningStartChoice()
{
    clampCurrentStageToSelectableStages();
    syncWarpStateForCurrentStage();
    baseMiningStartChoiceActive_ = true;
    baseMiningStartSelection_ = stageLooksRoguelike(currentStageDefinition()) ? 0 : (unlockedWarpPointCount_ > 0 ? 1 : 0);
    baseWarpPointSelectActive_ = false;
    baseWarpPointSelection_ = 0;
    baseRegenerateConfirm_ = {};
    baseRoguelikeDepartureConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    baseStatus_.clear();
}

bool Game::hasAnyMiningToolForBaseRescue() const
{
    const auto hasMiningStack = [](const InventoryObjectStack& stack) {
        return stack.count > 0 && isMiningToolObject(stack.item);
    };
    const auto hasMiningInstance = [](const InventoryObjectInstance& instance) {
        return isMiningToolObject(instance.item);
    };

    if (std::any_of(inventory_.objectStacks().begin(), inventory_.objectStacks().end(), hasMiningStack) ||
        std::any_of(inventory_.objectInstances().begin(), inventory_.objectInstances().end(), hasMiningInstance) ||
        std::any_of(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), hasMiningStack) ||
        std::any_of(warehouseObjectInstances_.begin(), warehouseObjectInstances_.end(), hasMiningInstance)) {
        return true;
    }

    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        for (const SpellRingItem& item : spellRing_.itemsForRing(ringIndex)) {
            if (item.type == SpellRingItemType::Shovel) {
                return true;
            }
            const ItemData* object = objectForRingItem(objectCatalog_, item);
            if (object != nullptr && isMiningToolObject(*object)) {
                return true;
            }
        }
    }

    return false;
}

bool Game::canAffordMerchantMiningToolForBaseRescue() const
{
    for (const MerchantProduct& product : merchantStock_) {
        if (product.quantity <= 0 || product.price <= 0) {
            continue;
        }
        const ItemData* item = objectCatalog_.registry.findById(product.objectId);
        if (item != nullptr && isMiningToolObject(*item) && money_ >= product.price) {
            return true;
        }
    }

    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const ItemData* item = objectCatalog_.registry.findById(object.id);
        if (item != nullptr &&
            isMerchantMiningCandidate(*item, merchantUpgradeLevel_) &&
            !isStoryObject(*item) &&
            money_ >= std::max(1, item->price)) {
            return true;
        }
    }

    return false;
}

bool Game::shouldStartBaseMiningRescueDropEvent() const
{
    return mode_ == ScreenMode::Base &&
        baseArea_ == BaseArea::Outdoor &&
        !baseMiningRescueDrop_.active &&
        !dialogue_.active() &&
        !pendingStoryTriggerDelayActive() &&
        pendingStoryTrigger_.empty() &&
        pendingStoryTriggers_.empty() &&
        !firstItemAcquisitionNoticeActive() &&
        !screenTransition_.active() &&
        !hasAnyMiningToolForBaseRescue() &&
        !canAffordMerchantMiningToolForBaseRescue();
}

void Game::startBaseMiningRescueDropEvent()
{
    const UiRect bounds = baseMapBounds();
    const Vec2 baseTarget = {
        std::clamp(basePlayerPosition_.x, bounds.pos.x + 90.0f, bounds.pos.x + bounds.size.x - 90.0f),
        std::clamp(basePlayerPosition_.y + 58.0f, bounds.pos.y + 100.0f, bounds.pos.y + bounds.size.y - 50.0f),
    };
    const std::array<Vec2, 2> offsets{{
        {-34.0f, 0.0f},
        {34.0f, 12.0f},
    }};

    baseMiningRescueDrop_ = {};
    baseMiningRescueDrop_.active = true;
    baseMiningRescueDrop_.items[0] = BaseMiningRescueDropItem{
        .objectId = std::string(RescueShovelObjectId),
        .startPosition = baseTarget + offsets[0] + Vec2{-36.0f, -430.0f},
        .targetPosition = baseTarget + offsets[0],
        .delaySeconds = 0.0f,
    };
    baseMiningRescueDrop_.items[1] = BaseMiningRescueDropItem{
        .objectId = std::string(RescueTorchObjectId),
        .startPosition = baseTarget + offsets[1] + Vec2{42.0f, -450.0f},
        .targetPosition = baseTarget + offsets[1],
        .delaySeconds = 0.12f,
    };
    baseStatus_ = "空からスコップと松明が降ってきた！";
}

bool Game::grantBaseMiningRescueTool(std::string_view objectId)
{
    const ItemData* item = objectCatalog_.registry.findById(objectId);
    SpellRingItem fallbackRingItem = objectId == RescueTorchObjectId ? makeTorch() : makeShovel();
    fallbackRingItem.objectId = std::string(objectId);

    const int ringCount = std::clamp(unlockedRingCount(), 1, SpellRingCount);
    if (item != nullptr) {
        for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
            SpellRingAddResult addResult{};
            if (spellRing_.addObjectItemToRing(ringIndex, *item, &addResult)) {
                refreshOrbitEffects();
                syncEncyclopediaFromInventoryAndRing();
                return true;
            }
        }

        if (inventory_.addObjectItem(objectCatalog_, objectId)) {
            syncEncyclopediaFromInventoryAndRing();
            return true;
        }

        if (warehouseUsedSlots() < warehouseCapacity()) {
            ItemInstance instance = inventory_.createDetachedObjectInstance(*item);
            warehouseObjectInstances_.push_back(InventoryObjectInstance{*item, std::move(instance)});
            syncWarehouseDisplaySlots();
            syncEncyclopediaFromInventoryAndRing();
            return true;
        }
    }

    std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(0);
    fallbackRingItem.ringIndex = 0;
    fallbackRingItem.localAngle = static_cast<float>(ringItems.size()) * Pi * 0.618f;
    ringItems.push_back(std::move(fallbackRingItem));
    refreshOrbitEffects();
    syncEncyclopediaFromInventoryAndRing();
    return true;
}

void Game::updateBaseMiningRescueDropEvent(float dt, UiContext& ui)
{
    if (!baseMiningRescueDrop_.active) {
        return;
    }

    baseMiningRescueDrop_.elapsedSeconds += std::max(0.0f, dt);
    bool allGranted = true;
    for (BaseMiningRescueDropItem& item : baseMiningRescueDrop_.items) {
        const float landSeconds = item.delaySeconds + BaseMiningRescueDropDurationSeconds;
        if (!item.granted && baseMiningRescueDrop_.elapsedSeconds >= landSeconds) {
            item.granted = grantBaseMiningRescueTool(item.objectId);
            ui.emitSound(UiSoundEvent::ItemUse);
        }
        allGranted = allGranted && item.granted;
    }

    if (allGranted && baseMiningRescueDrop_.elapsedSeconds >= BaseMiningRescueDropEndSeconds) {
        baseMiningRescueDrop_ = {};
        baseStatus_ = "スコップと松明を受け取りました";
    }
}

void Game::maybeQueueStageStartStory()
{
    if (currentStageId_.empty()) {
        return;
    }
    if (dungeonRingIntroActive()) {
        stageStartStoryPendingAfterRingIntro_ = true;
        return;
    }

    stageStartStoryPendingAfterRingIntro_ = false;
    queueStoryEventForCurrentStage("stage_start");
    queueStoryEventForCurrentStage("monica_radio");
}

void Game::placeBasePlayerAtMineExitReturnPoint()
{
    const UiRect fallback = defaultBaseFacilityRect(BaseArea::Outdoor, ringWorkshopUnlocked_, "mine_exit");
    const UiRect mineExitRect = toUiRect(baseFacilityRectFor(BaseArea::Outdoor, "mine_exit", toBaseEditRect(fallback)));
    baseArea_ = BaseArea::Outdoor;
    basePlayerPosition_ = baseFacilitySpawnPosition(mineExitRect, BaseFacilitySpawnSide::Above, balance_.playerRadius);
    const UiRect bounds = baseMapBounds();
    basePlayerPosition_.y = std::clamp(
        basePlayerPosition_.y - BaseMineExitReturnUpOffset,
        bounds.pos.y + balance_.playerRadius,
        bounds.pos.y + bounds.size.y - balance_.playerRadius);
    baseOutdoorPlayerPosition_ = basePlayerPosition_;
    basePlayerFacing_ = {0.0f, 1.0f};
    updateBasePlayerSpriteFlipFromFacing();
}

std::vector<Game::WarpPoint> Game::selectableWarpPointsForCurrentStageStart() const
{
    std::vector<WarpPoint> points;
    const std::vector<WarpPoint>* source = nullptr;
    const auto retainedStage = dungeonStates_.find(currentStageId_);
    if (retainedStage != dungeonStates_.end() && retainedStage->second.valid) {
        source = &retainedStage->second.warpPoints;
    } else if (!warpPoints_.empty()) {
        source = &warpPoints_;
    }

    if (source != nullptr) {
        for (const WarpPoint& point : *source) {
            if (point.discovered) {
                points.push_back(point);
            }
        }
    }

    if (points.empty() && unlockedWarpPointCount_ > 0 && hasLatestWarpPointPosition_) {
        WarpPoint fallback;
        fallback.stageId = currentStage_ + 1;
        fallback.index = std::max(0, unlockedWarpPointCount_ - 1);
        fallback.position = latestWarpPointPosition_;
        fallback.tilePosition = {
            tileMap_.worldToTile(latestWarpPointPosition_.x),
            tileMap_.worldToTile(latestWarpPointPosition_.y),
        };
        fallback.discovered = true;
        fallback.unlocked = true;
        fallback.snapshotCaptured = true;
        points.push_back(fallback);
    }

    std::sort(points.begin(), points.end(), [](const WarpPoint& left, const WarpPoint& right) {
        return left.index < right.index;
    });
    return points;
}

void Game::placeBasePlayerAtHomeDoorResumePoint()
{
    const UiRect fallback = defaultBaseFacilityRect(BaseArea::Outdoor, ringWorkshopUnlocked_, "home_entrance");
    const UiRect entranceRect = toUiRect(baseFacilityRectFor(BaseArea::Outdoor, "home_entrance", toBaseEditRect(fallback)));
    baseArea_ = BaseArea::Outdoor;
    basePlayerPosition_ = baseFacilitySpawnPosition(
        entranceRect,
        BaseFacilitySpawnSide::Below,
        balance_.playerRadius);
    baseOutdoorPlayerPosition_ = basePlayerPosition_;
    basePlayerFacing_ = {0.0f, -1.0f};
    updateBasePlayerSpriteFlipFromFacing();
}

const StoryEvent* Game::findStoryEvent(std::string_view id) const
{
    const auto it = std::find_if(storyEvents_.begin(), storyEvents_.end(), [id](const StoryEvent& event) {
        return event.id == id;
    });
    return it == storyEvents_.end() ? nullptr : &*it;
}

const StoryEvent* Game::findStoryEventForTrigger(std::string_view trigger) const
{
    const auto it = std::find_if(storyEvents_.begin(), storyEvents_.end(), [this, trigger](const StoryEvent& event) {
        if (event.trigger != trigger) {
            return false;
        }
        if (event.repeatable) {
            return true;
        }
        return event.onceFlag.empty() ||
            std::find(storyFlags_.begin(), storyFlags_.end(), event.onceFlag) == storyFlags_.end();
    });
    return it == storyEvents_.end() ? nullptr : &*it;
}

std::string Game::currentStageStoryTrigger(std::string_view triggerName) const
{
    if (currentStageId_.empty()) {
        return {};
    }
    return std::string(triggerName) + ":" + currentStageId_;
}

bool Game::queueStoryEventForTrigger(std::string trigger)
{
    if (playerDeathSequenceActive()) {
        return false;
    }
    if (trigger.empty()) {
        return false;
    }
    if (isTutorialStoryTrigger(trigger) && !pendingStoryTriggers_.empty()) {
        return false;
    }
    if (findStoryEventForTrigger(trigger) == nullptr) {
        return false;
    }
    if (std::find(pendingStoryTriggers_.begin(), pendingStoryTriggers_.end(), trigger) != pendingStoryTriggers_.end()) {
        return true;
    }
    pendingStoryTriggers_.push_back(std::move(trigger));
    return true;
}

bool Game::queueStoryEventForCurrentStage(std::string_view triggerName)
{
    return queueStoryEventForTrigger(currentStageStoryTrigger(triggerName));
}

void Game::updateQueuedStoryEvents()
{
    if (dialogue_.active() ||
        dungeonFocusActive() ||
        pendingStoryTriggers_.empty() ||
        screenTransition_.active() ||
        worldBuildActive() ||
        dungeonRingIntroActive() ||
        mode_ == ScreenMode::OpeningKamishibai ||
        mode_ == ScreenMode::EndingKamishibai ||
        mode_ == ScreenMode::Title ||
        mode_ == ScreenMode::WorldLoading) {
        return;
    }

    while (!pendingStoryTriggers_.empty()) {
        std::string trigger = std::move(pendingStoryTriggers_.front());
        pendingStoryTriggers_.erase(pendingStoryTriggers_.begin());
        if (startStoryEventForTrigger(trigger)) {
            return;
        }
    }
}

bool Game::startStoryEventInternal(std::string_view id, StoryEventStartOptions options)
{
    if (!options.ignorePlayerDeath && playerDeathSequenceActive()) {
        return false;
    }
    const StoryEvent* event = findStoryEvent(id);
    if (event == nullptr) {
        logWarning(options.logDebugReplay
            ? "[story] debug event not found: " + std::string(id)
            : "[story] event not found: " + std::string(id));
        return false;
    }

    if (options.respectOnceFlag && !event->onceFlag.empty()) {
        const bool alreadySeen = std::find(storyFlags_.begin(), storyFlags_.end(), event->onceFlag) != storyFlags_.end();
        if (alreadySeen) {
            return false;
        }
        addStoryFlag(event->onceFlag);
    }

    if (options.clearPendingStoryQueues) {
        pendingStoryTrigger_.clear();
        pendingStoryTriggerDelaySeconds_ = 0.0f;
        pendingStoryTriggers_.clear();
    }
    baseStatus_.clear();
    pendingDialogueCompletion_ = {};
    clearBaseStoryPresentation();
    dialogue_.start(event->dialogue);
    if (options.logDebugReplay) {
        logInfo("[story] debug replay: " + event->id);
    }
    if (options.onComplete) {
        if (!dialogue_.active()) {
            std::function<void()> onComplete = std::move(options.onComplete);
            onComplete();
        } else {
            pendingDialogueCompletion_ = std::move(options.onComplete);
        }
    }
    return true;
}

bool Game::startStoryEvent(std::string_view id)
{
    return startStoryEventInternal(id, {});
}

bool Game::startStoryEventWithCompletion(std::string_view id, std::function<void()> onComplete)
{
    if (playerDeathSequenceActive()) {
        return false;
    }
    if (dialogue_.active()) {
        return false;
    }
    StoryEventStartOptions options;
    options.onComplete = std::move(onComplete);
    return startStoryEventInternal(id, std::move(options));
}

bool Game::startDialogueSequenceWithCompletion(DialogueSequence sequence, std::function<void()> onComplete)
{
    if (playerDeathSequenceActive()) {
        return false;
    }
    if (dialogue_.active()) {
        return false;
    }
    baseStatus_.clear();
    pendingDialogueCompletion_ = std::move(onComplete);
    dialogue_.start(std::move(sequence));
    if (!dialogue_.active()) {
        std::function<void()> callback = std::move(pendingDialogueCompletion_);
        pendingDialogueCompletion_ = {};
        if (callback) {
            callback();
        }
    }
    return true;
}

bool Game::startStoryEventForDebug(std::string_view id)
{
    return startStoryEventForDebugWithCompletion(id, {});
}

bool Game::startStoryEventForDebugWithCompletion(std::string_view id, std::function<void()> onComplete)
{
    StoryEventStartOptions options;
    options.respectOnceFlag = false;
    options.clearPendingStoryQueues = true;
    options.ignorePlayerDeath = true;
    options.logDebugReplay = true;
    options.onComplete = std::move(onComplete);
    return startStoryEventInternal(id, std::move(options));
}

bool Game::startStoryEventForTrigger(std::string_view trigger)
{
    const StoryEvent* event = findStoryEventForTrigger(trigger);
    if (event == nullptr) {
        return false;
    }
    return startStoryEvent(event->id);
}

void Game::maybeStartOpeningBaseIntroEvent()
{
    queueStoryEventForTrigger(std::string(IntroTutorialBaseReturnTrigger));
}

void Game::updateBookshelfScreen(const Input& input, UiContext& ui)
{
    const std::vector<EndingKind> replayChoices = bookshelfEndingReplayChoices();
    const int menuItemCount = BookshelfMenuItemCount + (replayChoices.empty() ? 0 : 1);
    const auto itemCountForPage = [menuItemCount, this](BookshelfPage page) {
        switch (page) {
        case BookshelfPage::Menu:
            return menuItemCount;
        case BookshelfPage::Items:
            return itemCodexObjectCount(objectCatalog_);
        case BookshelfPage::Enemies:
            return enemyCodexEnemyCount(enemyCatalog_);
        }
        return 0;
    };
    const auto pageForMenuSelection = [](int selection) {
        return selection == 1 ? BookshelfPage::Enemies : BookshelfPage::Items;
    };
    const auto openSelectedPage = [&]() {
        closeUiCommandMenu(bookshelfEndingCommandMenu_);
        bookshelfPage_ = pageForMenuSelection(bookshelfSelection_);
        bookshelfSelection_ = 0;
        bookshelfScrollOffset_ = 0.0f;
        bookshelfScrollState_ = {};
    };

    const UiRect panel = bookshelfPage_ == BookshelfPage::Menu
        ? bookshelfMenuPanelRect(menuItemCount)
        : merchantPanelRect();
    const auto startReplay = [&](EndingKind kind) {
        closeUiCommandMenu(bookshelfEndingCommandMenu_);
        baseBookshelfActive_ = false;
        bookshelfPage_ = BookshelfPage::Menu;
        bookshelfSelection_ = 0;
        bookshelfScrollOffset_ = 0.0f;
        bookshelfScrollState_ = {};
        baseStatus_.clear();
        startEndingReplayKamishibai(kind);
    };
    const auto openEndingReplay = [&]() {
        if (replayChoices.empty()) {
            closeUiCommandMenu(bookshelfEndingCommandMenu_);
            return;
        }
        if (replayChoices.size() == 1) {
            ui.emitSound(UiSoundEvent::BookOpen);
            startReplay(replayChoices.front());
            return;
        }
        const std::vector<UiCommandMenuItem> commandItems = bookshelfEndingReplayCommandItems(replayChoices);
        openUiCommandMenu(
            bookshelfEndingCommandMenu_,
            uiCommandMenuAnchorForSlot(bookshelfMenuChoiceRect(panel, BookshelfEndingReplayMenuIndex)),
            panel,
            static_cast<int>(commandItems.size()),
            commandItems.data(),
            BookshelfEndingCommandMinWidth,
            2);
        baseStatus_.clear();
    };
    const auto activateMenuSelection = [&]() {
        if (bookshelfSelection_ == BookshelfEndingReplayMenuIndex) {
            openEndingReplay();
            return;
        }
        ui.emitSound(UiSoundEvent::BookOpen);
        openSelectedPage();
    };

    if (bookshelfPage_ == BookshelfPage::Menu && bookshelfEndingCommandMenu_.visible) {
        const std::vector<UiCommandMenuItem> commandItems = bookshelfEndingReplayCommandItems(replayChoices);
        if (replayChoices.size() < 2) {
            closeUiCommandMenu(bookshelfEndingCommandMenu_);
        }
        const int commandSelection = updateUiCommandMenu(
            bookshelfEndingCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0 && commandSelection < static_cast<int>(replayChoices.size())) {
            startReplay(replayChoices[static_cast<std::size_t>(commandSelection)]);
            ui.block(panel);
            return;
        }
        ui.block(panel);
        return;
    }

    if (uiCancelRequested(baseCancelState_, input, ui, panel)) {
        if (bookshelfPage_ == BookshelfPage::Menu) {
            baseBookshelfActive_ = false;
            closeUiCommandMenu(bookshelfEndingCommandMenu_);
            baseStatus_.clear();
        } else {
            bookshelfPage_ = BookshelfPage::Menu;
            bookshelfSelection_ = 0;
            bookshelfScrollOffset_ = 0.0f;
            bookshelfScrollState_ = {};
            closeUiCommandMenu(bookshelfEndingCommandMenu_);
        }
        return;
    }

    const int itemCount = itemCountForPage(bookshelfPage_);
    if (itemCount <= 0) {
        bookshelfSelection_ = 0;
    } else {
        if (bookshelfPage_ == BookshelfPage::Menu) {
            if (input.pressed(InputAction::MoveUp)) {
                bookshelfSelection_ = (bookshelfSelection_ + itemCount - 1) % itemCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                bookshelfSelection_ = (bookshelfSelection_ + 1) % itemCount;
            }
        } else {
            const InventoryUiGridStyle gridStyle = bookshelfGridStyle();
            const int columns = std::max(1, gridStyle.columns);
            int nextSelection = bookshelfSelection_;
            if (input.pressed(InputAction::MoveLeft)) {
                nextSelection = std::max(0, nextSelection - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                nextSelection = std::min(itemCount - 1, nextSelection + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                nextSelection = std::max(0, nextSelection - columns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                nextSelection = std::min(itemCount - 1, nextSelection + columns);
            }
            if (nextSelection != bookshelfSelection_) {
                bookshelfSelection_ = nextSelection;
                keepInventoryUiGridItemVisible(
                    bookshelfGridViewport(),
                    bookshelfSelection_,
                    itemCount,
                    bookshelfScrollOffset_,
                    gridStyle);
            }
        }
        bookshelfSelection_ = std::clamp(bookshelfSelection_, 0, itemCount - 1);
    }

    if (bookshelfPage_ == BookshelfPage::Menu) {
        const int visibleCount = std::min(BookshelfVisibleRows, itemCount);
        for (int i = 0; i < visibleCount; ++i) {
            const UiRect rect = bookshelfMenuChoiceRect(panel, i);
            if (rect.contains(ui.mouse())) {
                bookshelfSelection_ = i;
            }
            if (ui.pressed(rect)) {
                bookshelfSelection_ = i;
                activateMenuSelection();
                return;
            }
        }
    } else {
        const InventoryUiGridStyle gridStyle = bookshelfGridStyle();
        const UiRect viewport = bookshelfGridViewport();
        const UiScrollAreaLayout layout = updateInventoryUiGrid(
            ui,
            input,
            viewport,
            itemCount,
            bookshelfScrollOffset_,
            gridStyle,
            &bookshelfScrollState_);
        for (int i = 0; i < itemCount; ++i) {
            const UiRect rect = inventoryUiGridSlotRect(layout, i, gridStyle);
            if (!uiScrollAreaRectVisible(layout, rect)) {
                continue;
            }
            if (ui.hovered(rect)) {
                bookshelfSelection_ = i;
            }
            if (ui.pressed(rect)) {
                bookshelfSelection_ = i;
                ui.emitSound(UiSoundEvent::Confirm);
                return;
            }
        }
    }

    if ((input.confirmPressed() || input.useItemPressed()) && bookshelfPage_ == BookshelfPage::Menu) {
        activateMenuSelection();
        return;
    }

    ui.block(panel);
}

void Game::openBaseDiary()
{
    baseDiaryActive_ = true;
    baseDiaryMode_ = BaseDiaryMode::Confirm;
    baseDiarySelection_ = 0;
    baseDiarySummary_ = loadDiarySaveSummaryFromDisk();
    baseDiaryMessage_.clear();
    baseStatus_.clear();
}

void Game::closeBaseDiary()
{
    baseDiaryActive_ = false;
    baseDiaryMode_ = BaseDiaryMode::Confirm;
    baseDiarySelection_ = 0;
    baseDiarySummary_ = {};
    baseDiaryMessage_.clear();
    baseStatus_.clear();
}

void Game::updateBaseDiaryScreen(const Input& input, UiContext& ui)
{
    const UiRect panel = basePanelRect();
    if (uiCancelRequested(baseCancelState_, input, ui, panel)) {
        closeBaseDiary();
        ui.block(panel);
        return;
    }

    if (baseDiaryMode_ == BaseDiaryMode::Saved) {
        const UiRect closeButton = uiResultDialogOkButtonRect(panel);
        if (ui.pressed(closeButton) || input.confirmPressed() || input.useItemPressed()) {
            ui.emitSound(UiSoundEvent::Confirm);
            closeBaseDiary();
            ui.block(panel);
            return;
        }
        ui.block(panel);
        return;
    }

    if (ui.hovered(uiConfirmDialogButtonRect(panel, 0))) {
        baseDiarySelection_ = 0;
    } else if (ui.hovered(uiConfirmDialogButtonRect(panel, 1))) {
        baseDiarySelection_ = 1;
    }
    if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp) || input.activeRingDelta() < 0) {
        baseDiarySelection_ = 1;
    }
    if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown) || input.activeRingDelta() > 0) {
        baseDiarySelection_ = 0;
    }

    const auto saveDiary = [this, &ui]() {
        std::string message;
        if (saveSaveData(message)) {
            ui.emitSound(UiSoundEvent::Confirm);
            baseDiaryMode_ = BaseDiaryMode::Saved;
            baseDiarySelection_ = 0;
            baseDiaryMessage_ = "保存しました。";
            baseDiarySummary_ = currentDiarySaveSummary();
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
            baseDiaryMode_ = BaseDiaryMode::Error;
            baseDiarySelection_ = 0;
            baseDiaryMessage_ = message.empty() ? "セーブに失敗しました。" : message;
        }
    };

    if (ui.pressed(uiConfirmDialogButtonRect(panel, 0))) {
        saveDiary();
        ui.block(panel);
        return;
    }
    if (ui.pressed(uiConfirmDialogButtonRect(panel, 1))) {
        ui.emitSound(UiSoundEvent::Cancel);
        closeBaseDiary();
        ui.block(panel);
        return;
    }
    if (input.confirmPressed() || input.useItemPressed()) {
        if (baseDiarySelection_ == 0) {
            saveDiary();
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
            closeBaseDiary();
        }
        ui.block(panel);
        return;
    }

    ui.block(panel);
}

void Game::updateBaseScreen(const Input& input, UiContext& ui, float dt)
{
    baseRingPreviewAnimationTime_ = std::fmod(baseRingPreviewAnimationTime_ + std::max(0.0f, dt), 3600.0f);
    updateBaseActorIdleAnimation(dt);
    const float ringPreviewSeconds = baseRingPreviewAnimationTime_;

    updatePlayerFootstepDust(dt);

    if (baseEditEnabled_) {
        updateBaseEditScreen(input, ui, dt);
        return;
    }

    const auto baseManualSelectionSnapshot = [this]() {
        return std::array<int, 25>{{
            baseMenuSelection_,
            baseMiningStartSelection_,
            baseWarpPointSelection_,
            baseStorageActionSelection_,
            baseStorageBulkSelection_,
            baseStorageDepositSource_,
            baseStorageDepositSelection_,
            baseStorageWithdrawSelection_,
            baseMerchantActionSelection_,
            baseMerchantSellSource_,
            baseSellSelection_,
            baseMerchantBuySelection_,
            baseMerchantSellCommandSource_,
            baseMerchantSellCommandIndex_,
            baseMerchantBuyCommandIndex_,
            baseUpgradeSelection_,
            baseProcessingActionSelection_,
            baseProcessingMode_,
            baseProcessingSource_,
            baseProcessingSelection_,
            baseRingWorkshopSelection_,
            baseRingWorkshopRingIndex_,
            bookshelfSelection_,
            baseDiarySelection_,
            baseEditSelectedFacilityIndex_,
        }};
    };
    const auto previousBaseManualSelection = baseManualSelectionSnapshot();
    struct BaseCursorMoveSoundGuard {
        std::function<void()> emit;
        ~BaseCursorMoveSoundGuard()
        {
            emit();
        }
    };
    BaseCursorMoveSoundGuard baseCursorMoveSoundGuard{[this, &ui, previousBaseManualSelection, baseManualSelectionSnapshot]() {
        if (ui.soundEventCount(UiSoundEvent::CursorMove) > 0) {
            return;
        }
        if (baseManualSelectionSnapshot() != previousBaseManualSelection) {
            ui.emitSound(UiSoundEvent::CursorMove);
        }
    }};

    if (baseResultDialog_.open) {
        const UiRect resultPanel = baseResultDialogRect();
        updateUiResultDialog(baseResultDialog_, ui, input, resultPanel);
        ui.block(resultPanel);
        return;
    }

    if (baseStorageQuantityDialog_.open) {
        const UiRect quantityPanel = storageQuantityDialogRect();
        const UiQuantityDialogResult quantityResult = updateUiQuantityDialog(baseStorageQuantityDialog_, ui, input, quantityPanel);
        if (quantityResult == UiQuantityDialogResult::Confirmed) {
            const int quantity = baseStorageQuantityDialog_.value;
            const StorageQuantityPending pending = baseStorageQuantityPending_;
            baseStorageQuantityPending_ = {};
            if (pending.operation == StorageQuantityOperation::Deposit) {
                depositStorageTarget(pending.target, quantity);
            } else if (pending.operation == StorageQuantityOperation::Withdraw) {
                withdrawStorageTarget(pending.target, quantity);
            }
        } else if (quantityResult == UiQuantityDialogResult::Cancelled) {
            baseStorageQuantityPending_ = {};
        }
        ui.block(quantityPanel);
        return;
    }

    if (baseProcessingConfirm_.open) {
        const UiRect confirmPanel = baseProcessingConfirmRect();
        baseProcessingConfirm_.confirmEnabled = processingCommandExecutable(
            baseProcessingConfirmTarget_,
            baseProcessingConfirmMode_);
        const UiConfirmDialogResult result = updateUiConfirmDialog(baseProcessingConfirm_, ui, input, confirmPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            applyProcessingTarget(baseProcessingConfirmTarget_, baseProcessingConfirmMode_);
            baseProcessingConfirmTarget_ = {};
        } else if (result == UiConfirmDialogResult::Cancelled) {
            baseProcessingConfirmTarget_ = {};
            baseStatus_.clear();
        }
        ui.block(confirmPanel);
        return;
    }

    if (baseBrokenRingDepartureConfirm_.open) {
        const UiRect confirmPanel = baseBrokenRingDepartureConfirmRect();
        const UiConfirmDialogResult result = updateUiConfirmDialog(
            baseBrokenRingDepartureConfirm_,
            ui,
            input,
            confirmPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            openBaseMiningStartChoice();
        } else if (result == UiConfirmDialogResult::Cancelled) {
            baseStatus_.clear();
        }
        ui.block(confirmPanel);
        return;
    }

    if (baseDiaryActive_) {
        updateBaseDiaryScreen(input, ui);
        return;
    }

    if (baseBookshelfActive_) {
        updateBookshelfScreen(input, ui);
        return;
    }

    if (baseRingWorkshopActive_) {
        const UiRect workshopBounds = baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction
            ? ringWorkshopActionDialogRect()
            : ringWorkshopPanelRect();
        const auto closeWorkshop = [this]() {
            baseRingWorkshopActive_ = false;
            baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
            baseRingWorkshopSelection_ = 0;
            baseRingWorkshopUpgradeTabs_ = {};
            baseRingWorkshopUpgradeScrollOffset_ = 0.0f;
            baseRingWorkshopUpgradeScroll_ = {};
            resetRingWorkshopDraft();
            baseStatus_.clear();
        };
        const auto returnToWorkshopMenu = [this]() {
            if (baseRingWorkshopMode_ == RingWorkshopMode::Respec) {
                resetRingWorkshopDraft();
            }
            baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
            baseRingWorkshopSelection_ = 0;
            baseRingWorkshopUpgradeTabs_ = {};
            baseRingWorkshopUpgradeScrollOffset_ = 0.0f;
            baseRingWorkshopUpgradeScroll_ = {};
            baseStatus_.clear();
        };
        if (uiCancelRequested(baseCancelState_, input, ui, workshopBounds)) {
            if (baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction) {
                closeWorkshop();
            } else {
                returnToWorkshopMenu();
            }
            ui.block(workshopBounds);
            return;
        }

        if (baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction) {
            baseRingWorkshopSelection_ = std::clamp(baseRingWorkshopSelection_, 0, RingWorkshopActionCount - 1);
            const auto chooseAction = [this, &ui](int item) {
                ui.emitSound(UiSoundEvent::Confirm);
                if (item == 0) {
                    baseRingWorkshopMode_ = RingWorkshopMode::Upgrade;
                    baseRingWorkshopSelection_ = 0;
                    baseRingWorkshopRingIndex_ = std::clamp(spellRing_.activeRingIndex(), 0, unlockedRingCount() - 1);
                    baseRingWorkshopRingTabs_ = {};
                    baseRingWorkshopUpgradeTabs_ = {};
                    baseRingWorkshopUpgradeScrollOffset_ = 0.0f;
                    baseRingWorkshopUpgradeScroll_ = {};
                    baseStatus_.clear();
                    return;
                }
                if (item == 1) {
                    baseRingWorkshopMode_ = RingWorkshopMode::Respec;
                    baseRingWorkshopSelection_ = 0;
                    baseRingWorkshopRingIndex_ = std::clamp(spellRing_.activeRingIndex(), 0, unlockedRingCount() - 1);
                    baseRingWorkshopRingTabs_ = {};
                    baseRingWorkshopUpgradeTabs_ = {};
                    baseRingWorkshopUpgradeScrollOffset_ = 0.0f;
                    baseRingWorkshopUpgradeScroll_ = {};
                    resetRingWorkshopDraft();
                    baseStatus_.clear();
                    return;
                }
            };
            if (input.pressed(InputAction::MoveUp)) {
                baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + RingWorkshopActionCount - 1) % RingWorkshopActionCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + 1) % RingWorkshopActionCount;
            }
            for (int i = 0; i < RingWorkshopActionCount; ++i) {
                const UiRect rect = ringWorkshopActionChoiceRect(i);
                if (updateClickSelection(ui, rect, i, baseRingWorkshopSelection_)) {
                    chooseAction(i);
                    ui.block(workshopBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                chooseAction(baseRingWorkshopSelection_);
                ui.block(workshopBounds);
                return;
            }
            ui.block(workshopBounds);
            return;
        }

        if (baseRingWorkshopMode_ == RingWorkshopMode::Respec) {
            constexpr int RespecSelectionCount = RingLevelUpgradeKindCount + 1;
            const int ringCount = unlockedRingCount();
            baseRingWorkshopRingIndex_ = std::clamp(baseRingWorkshopRingIndex_, 0, ringCount - 1);
            baseRingWorkshopSelection_ = std::clamp(baseRingWorkshopSelection_, 0, RespecSelectionCount - 1);

            std::array<UiTabItem, SpellRingCount> ringTabs{};
            std::array<UiRect, SpellRingCount> ringTabRects{};
            std::array<std::string, SpellRingCount> ringTabLabels{};
            for (int i = 0; i < ringCount; ++i) {
                ringTabLabels[static_cast<std::size_t>(i)] = "リング " + std::to_string(i + 1);
                ringTabs[static_cast<std::size_t>(i)] = {
                    ringTabLabels[static_cast<std::size_t>(i)],
                    true,
                    ringDisplayIconImageNumber(i),
                };
                ringTabRects[static_cast<std::size_t>(i)] = ringWorkshopRingTabRect(i, ringCount);
            }
            UiTabsInput ringTabsInput{};
            ringTabsInput.focusDelta = input.activeRingDelta();
            const int directRingFocus = input.shortcutSlotPressed();
            if (directRingFocus >= 0 && directRingFocus < ringCount) {
                ringTabsInput.directFocusIndex = directRingFocus;
            }
            ringTabsInput.commit = ringTabsInput.focusDelta != 0 || ringTabsInput.directFocusIndex >= 0;
            const int ringSelection = updateUiTabs(
                baseRingWorkshopRingTabs_,
                ui,
                ringTabsInput,
                baseRingWorkshopRingIndex_,
                ringTabs.data(),
                ringCount,
                ringTabRects.data());
            if (ringSelection >= 0) {
                baseRingWorkshopRingIndex_ = ringSelection;
                ui.block(workshopBounds);
                return;
            }

            int move = 0;
            if (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft) || input.shortcutCursorDelta() < 0) {
                --move;
            }
            if (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight) || input.shortcutCursorDelta() > 0) {
                ++move;
            }
            if (move != 0) {
                baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + move + RespecSelectionCount) % RespecSelectionCount;
            }

            const auto chooseRespecKind = [this, &ui](int kindIndex) {
                const RingLevelUpgradeSelection selection{
                    baseRingWorkshopRingIndex_,
                    ringWorkshopKindForIndex(kindIndex),
                };
                if (!ringWorkshopRespecSource_) {
                    const RingLevelUpgradePoints& points = ringWorkshopDraftUpgradePoints_[static_cast<std::size_t>(selection.ringIndex)];
                    if (ringLevelUpgradePoint(points, selection.kind) <= 0) {
                        ui.emitSound(UiSoundEvent::Cancel);
                        baseStatus_ = "移動元にできるポイントがありません";
                        return;
                    }
                    ringWorkshopRespecSource_ = selection;
                    ui.emitSound(UiSoundEvent::Confirm);
                    baseStatus_ = "移動先を選んでください";
                    return;
                }
                ui.emitSound(UiSoundEvent::Confirm);
                adjustRingWorkshopRespec(*ringWorkshopRespecSource_, selection);
            };

            std::array<UiVerticalTabItem, RingLevelUpgradeKindCount> respecTabs{};
            std::array<UiRect, RingLevelUpgradeKindCount> respecTabRects{};
            for (int i = 0; i < RingLevelUpgradeKindCount; ++i) {
                respecTabs[static_cast<std::size_t>(i)] = {ringLevelUpgradeKindName(ringWorkshopKindForIndex(i)), "", true};
                respecTabRects[static_cast<std::size_t>(i)] = ringWorkshopRespecKindRect(i);
            }
            const int selectedTab = updateVerticalTabClickSelection(
                baseRingWorkshopUpgradeTabs_,
                ui,
                std::clamp(baseRingWorkshopSelection_, 0, RingLevelUpgradeKindCount - 1),
                respecTabs,
                respecTabRects);
            if (selectedTab >= 0) {
                baseRingWorkshopSelection_ = selectedTab;
                chooseRespecKind(selectedTab);
                ui.block(workshopBounds);
                return;
            }
            const UiRect confirmRect = ringWorkshopRespecConfirmRect();
            if (updateClickSelection(ui, confirmRect, RingLevelUpgradeKindCount, baseRingWorkshopSelection_)) {
                ui.emitSound(UiSoundEvent::Confirm);
                confirmRingWorkshopRespec();
                ui.block(workshopBounds);
                return;
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                if (baseRingWorkshopSelection_ == RingLevelUpgradeKindCount) {
                    ui.emitSound(UiSoundEvent::Confirm);
                    confirmRingWorkshopRespec();
                } else {
                    chooseRespecKind(baseRingWorkshopSelection_);
                }
                ui.block(workshopBounds);
                return;
            }
            ui.block(workshopBounds);
            return;
        }

        if (baseRingWorkshopMode_ == RingWorkshopMode::Upgrade) {
            const int ringCount = unlockedRingCount();
            baseRingWorkshopRingIndex_ = std::clamp(baseRingWorkshopRingIndex_, 0, ringCount - 1);
            baseRingWorkshopSelection_ = std::clamp(baseRingWorkshopSelection_, 0, RingWorkshopUpgradeDisplayCount - 1);

            std::array<UiTabItem, SpellRingCount> ringTabs{};
            std::array<UiRect, SpellRingCount> ringTabRects{};
            std::array<std::string, SpellRingCount> ringTabLabels{};
            for (int i = 0; i < ringCount; ++i) {
                ringTabLabels[static_cast<std::size_t>(i)] = "リング " + std::to_string(i + 1);
                ringTabs[static_cast<std::size_t>(i)] = {
                    ringTabLabels[static_cast<std::size_t>(i)],
                    true,
                    ringDisplayIconImageNumber(i),
                };
                ringTabRects[static_cast<std::size_t>(i)] = ringWorkshopRingTabRect(i, ringCount);
            }
            UiTabsInput ringTabsInput{};
            ringTabsInput.focusDelta = input.activeRingDelta();
            const int directRingFocus = input.shortcutSlotPressed();
            if (directRingFocus >= 0 && directRingFocus < ringCount) {
                ringTabsInput.directFocusIndex = directRingFocus;
            }
            ringTabsInput.commit = ringTabsInput.focusDelta != 0 || ringTabsInput.directFocusIndex >= 0;
            const int ringSelection = updateUiTabs(
                baseRingWorkshopRingTabs_,
                ui,
                ringTabsInput,
                baseRingWorkshopRingIndex_,
                ringTabs.data(),
                ringCount,
                ringTabRects.data());
            if (ringSelection >= 0) {
                baseRingWorkshopRingIndex_ = ringSelection;
                ui.block(workshopBounds);
                return;
            }

            int move = 0;
            if (input.pressed(InputAction::MoveUp)) {
                --move;
            }
            if (input.pressed(InputAction::MoveDown)) {
                ++move;
            }
            if (move != 0) {
                baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + move + RingWorkshopUpgradeDisplayCount) % RingWorkshopUpgradeDisplayCount;
            }

            const UiScrollAreaStyle scrollStyle = ringWorkshopScrollAreaStyle();
            const float scrollContentHeight = ringWorkshopUpgradeScrollContentHeight();
            const UiRect scrollViewport = ringWorkshopUpgradeScrollViewportRect();
            UiScrollAreaLayout scrollLayout = updateUiScrollArea(
                ui,
                input,
                scrollViewport,
                scrollContentHeight,
                baseRingWorkshopUpgradeScrollOffset_,
                scrollStyle,
                &baseRingWorkshopUpgradeScroll_);
            if (move != 0) {
                keepUiScrollAreaRectVisible(
                    scrollViewport,
                    ringWorkshopUpgradeItemRect(scrollLayout, baseRingWorkshopSelection_),
                    scrollContentHeight,
                    baseRingWorkshopUpgradeScrollOffset_,
                    scrollStyle);
                scrollLayout = makeUiScrollAreaLayout(
                    scrollViewport,
                    scrollContentHeight,
                    baseRingWorkshopUpgradeScrollOffset_,
                    scrollStyle);
            }

            const int ringIndex = std::clamp(baseRingWorkshopRingIndex_, 0, SpellRingCount - 1);
            const float minMeters = ringWorkshopRadiusMinForRing(ringIndex);
            const float maxMeters = ringWorkshopRadiusMaxForRing(ringIndex);
            if (maxMeters > minMeters + 0.001f) {
                const UiRect gaugeRect = ringWorkshopRadiusGaugeRect(scrollLayout);
                if (input.mouseLeftHeld() && scrollViewport.contains(ui.mouse()) && gaugeRect.contains(ui.mouse()) && !ui.pointerConsumed()) {
                    const float ratio = clamp((ui.mouse().x - gaugeRect.pos.x) / std::max(1.0f, gaugeRect.size.x), 0.0f, 1.0f);
                    setRingWorkshopRadiusSettingForRing(ringIndex, minMeters + (maxMeters - minMeters) * ratio);
                    ui.block(gaugeRect);
                    ui.block(workshopBounds);
                    return;
                }
            }

            const auto chooseUpgradeItem = [this, &ui](int item) {
                if (item >= 0 && item < RingWorkshopImplementedUpgradeCount) {
                    ui.emitSound(UiSoundEvent::Confirm);
                    buyRingWorkshopUpgrade(ringWorkshopUpgradeForDisplayIndex(item));
                    return;
                }
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "この項目は未解禁です";
            };

            std::array<UiVerticalTabItem, RingWorkshopUpgradeDisplayCount> upgradeTabs{};
            std::array<UiRect, RingWorkshopUpgradeDisplayCount> upgradeTabRects{};
            for (int i = 0; i < RingWorkshopUpgradeDisplayCount; ++i) {
                upgradeTabs[static_cast<std::size_t>(i)] = {"", "", i < RingWorkshopImplementedUpgradeCount};
                upgradeTabRects[static_cast<std::size_t>(i)] = ringWorkshopUpgradeItemRect(scrollLayout, i);
            }
            const int selectedTab = updateVerticalTabClickSelection(
                baseRingWorkshopUpgradeTabs_,
                ui,
                baseRingWorkshopSelection_,
                upgradeTabs,
                upgradeTabRects);
            if (selectedTab >= 0) {
                baseRingWorkshopSelection_ = selectedTab;
                ui.block(workshopBounds);
                return;
            }
            if (ui.pressed(ringWorkshopUpgradeConfirmRect())) {
                chooseUpgradeItem(baseRingWorkshopSelection_);
                ui.block(workshopBounds);
                return;
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                chooseUpgradeItem(baseRingWorkshopSelection_);
                ui.block(workshopBounds);
                return;
            }
            ui.block(workshopBounds);
            return;
        }

        ui.block(workshopBounds);
        return;
    }

    if (baseStorageActive_) {
        const bool storageSmallDialog =
            baseStorageMode_ == StorageUiMode::ChooseAction ||
            baseStorageMode_ == StorageUiMode::Bulk;
        const UiRect storageBounds = storageSmallDialog
            ? (baseStorageMode_ == StorageUiMode::Bulk ? storageBulkDialogRect() : storageActionDialogRect())
            : merchantPanelRect();
        const auto resetStoragePointerPress = [this]() {
            baseStoragePointerOperation_ = StorageQuantityOperation::None;
            baseStoragePointerTarget_ = {};
            baseStoragePointerPressMouse_ = {};
            baseStoragePointerPressCanOpenMenu_ = false;
            baseStoragePointerDragTriggered_ = false;
        };
        const auto closeStorageCommand = [this]() {
            closeUiCommandMenu(baseStorageCommandMenu_);
            baseStorageCommandOperation_ = StorageQuantityOperation::None;
            baseStorageCommandTarget_ = {};
        };
        const auto closeStorage = [this, &closeStorageCommand, &resetStoragePointerPress]() {
            closeStorageCommand();
            resetStoragePointerPress();
            baseStorageActive_ = false;
            baseStorageMode_ = StorageUiMode::Closed;
            baseStorageQuantityDialog_ = {};
            baseStorageQuantityPending_ = {};
            baseStatus_.clear();
        };
        const auto returnToStorageMenu = [this, &closeStorageCommand, &resetStoragePointerPress]() {
            closeStorageCommand();
            resetStoragePointerPress();
            baseStorageMode_ = StorageUiMode::ChooseAction;
            baseStorageActionSelection_ = 0;
            baseStorageBulkSelection_ = 0;
            baseStorageQuantityDialog_ = {};
            baseStorageQuantityPending_ = {};
            baseStatus_.clear();
        };
        const auto openQuantityDialog = [this](StorageQuantityOperation operation, StorageTransferTarget target, int maxCount) {
            InventoryUiEntryView view = storageTransferTargetView(target);
            const std::optional<InventoryUiItemStats> stats = inventoryUiEntryStats(view);
            const bool broken = stats ? stats->broken : (view.item != nullptr && view.item->durability == 0);
            const std::string itemName = view.item != nullptr && !view.item->name.empty()
                ? itemDisplayName(view.item->name, broken)
                : std::string("アイテム");
            baseStorageQuantityPending_ = StorageQuantityPending{operation, target};
            openUiQuantityDialog(
                baseStorageQuantityDialog_,
                operation == StorageQuantityOperation::Deposit ? "しまう個数" : "取り出す個数",
                itemName,
                1,
                std::max(1, maxCount),
                std::max(1, maxCount),
                "個");
            baseStatus_.clear();
        };
        const auto applyStorageTarget = [this, &openQuantityDialog](StorageQuantityOperation operation, StorageTransferTarget target) {
            if (!storageTransferTargetAvailable(target)) {
                if (operation == StorageQuantityOperation::Deposit &&
                    target.source == BaseItemSource::Backpack) {
                    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                        if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                            baseStatus_ = "装備中の杖はしまえません";
                            return;
                        }
                    }
                }
                if (operation == StorageQuantityOperation::Deposit &&
                    target.valid &&
                    target.source != BaseItemSource::Backpack &&
                    target.source != BaseItemSource::Warehouse) {
                    baseStatus_ = "このアイテムはしまえません";
                } else {
                    baseStatus_ = operation == StorageQuantityOperation::Deposit
                        ? "しまうアイテムがありません"
                        : "取り出すアイテムがありません";
                }
                return;
            }
            if (storageTransferTargetIsStack(target)) {
                const int stackCount = storageTransferTargetStackCount(target);
                if (stackCount > 1) {
                    openQuantityDialog(operation, target, stackCount);
                    return;
                }
            }
            if (operation == StorageQuantityOperation::Deposit) {
                depositStorageTarget(target, 1);
            } else {
                withdrawStorageTarget(target, 1);
            }
        };
        const auto storageCommandItems = [this]() {
            const bool available = storageTransferTargetAvailable(baseStorageCommandTarget_);
            const char* label = baseStorageCommandOperation_ == StorageQuantityOperation::Withdraw
                ? "取り出す"
                : "しまう";
            return std::array<UiCommandMenuItem, 1>{{{label, available}}};
        };
        const auto openStorageCommand = [&](StorageQuantityOperation operation, StorageTransferTarget target, Vec2 anchor) {
            if (!storageTransferTargetAvailable(target)) {
                ui.emitSound(UiSoundEvent::Cancel);
                if (operation == StorageQuantityOperation::Deposit &&
                    target.source == BaseItemSource::Backpack) {
                    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                        if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                            baseStatus_ = "装備中の杖はしまえません";
                            closeStorageCommand();
                            return;
                        }
                    }
                }
                if (operation == StorageQuantityOperation::Deposit &&
                    target.valid &&
                    target.source != BaseItemSource::Backpack &&
                    target.source != BaseItemSource::Warehouse) {
                    baseStatus_ = "このアイテムはしまえません";
                } else {
                    baseStatus_ = operation == StorageQuantityOperation::Deposit
                        ? "しまうアイテムがありません"
                        : "取り出すアイテムがありません";
                }
                closeStorageCommand();
                return;
            }

            baseStorageCommandOperation_ = operation;
            baseStorageCommandTarget_ = target;
            const std::array<UiCommandMenuItem, 1> items = storageCommandItems();
            openUiCommandMenu(
                baseStorageCommandMenu_,
                anchor,
                storageBounds,
                static_cast<int>(items.size()),
                items.data(),
                140.0f,
                2);
            baseStatus_.clear();
        };
        const auto moveGridSelection = [&input](int& selection, int slotCount) {
            const int count = std::max(1, slotCount);
            selection = std::clamp(selection, 0, count - 1);
            const int columns = StorageColumns;
            if (input.pressed(InputAction::MoveLeft)) {
                selection = std::max(0, selection - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                selection = std::min(count - 1, selection + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                selection = std::max(0, selection - columns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                selection = std::min(count - 1, selection + columns);
            }
            if (input.shortcutCursorDelta() != 0) {
                selection = std::clamp(selection + input.shortcutCursorDelta(), 0, count - 1);
            }
        };

        const std::array<UiCommandMenuItem, 1> commandItems = storageCommandItems();
        const bool commandOpenBeforeUpdate = baseStorageCommandMenu_.open;
        const int commandSelection = updateUiCommandMenu(
            baseStorageCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0) {
            const StorageQuantityOperation operation = baseStorageCommandOperation_;
            const StorageTransferTarget target = baseStorageCommandTarget_;
            closeStorageCommand();
            resetStoragePointerPress();
            if (operation == StorageQuantityOperation::Deposit ||
                operation == StorageQuantityOperation::Withdraw) {
                applyStorageTarget(operation, target);
            }
            ui.block(storageBounds);
            return;
        } else if (!baseStorageCommandMenu_.open) {
            if (commandOpenBeforeUpdate && input.backPressed()) {
                closeStorageCommand();
                resetStoragePointerPress();
                ui.block(storageBounds);
                return;
            }
            baseStorageCommandOperation_ = StorageQuantityOperation::None;
            baseStorageCommandTarget_ = {};
        }

        if (uiCancelRequested(baseCancelState_, input, ui, storageBounds)) {
            if (baseStorageCommandMenu_.open) {
                closeStorageCommand();
                resetStoragePointerPress();
            } else if (baseStorageMode_ == StorageUiMode::ChooseAction) {
                closeStorage();
            } else {
                returnToStorageMenu();
            }
            ui.block(storageBounds);
            return;
        }

        if (baseStorageCommandMenu_.open) {
            ui.block(storageBounds);
            return;
        }

        const int storagePresetShortcut = input.shortcutSlotPressed();
        if (storagePresetShortcut >= 0 && storagePresetShortcut < RingPresetSlotCount) {
            const bool registered = storagePresetShortcut < unlockedRingPresetSlotCount() &&
                ringPresets_.registered(storagePresetShortcut);
            prepareRingPresetFromWarehouse(storagePresetShortcut);
            ui.emitSound(registered ? UiSoundEvent::Confirm : UiSoundEvent::Cancel);
            ui.block(storageBounds);
            return;
        }

        if (baseStorageMode_ == StorageUiMode::ChooseAction) {
            constexpr int ChoiceCount = 3;
            baseStorageActionSelection_ = std::clamp(baseStorageActionSelection_, 0, ChoiceCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseStorageActionSelection_ = (baseStorageActionSelection_ + ChoiceCount - 1) % ChoiceCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseStorageActionSelection_ = (baseStorageActionSelection_ + 1) % ChoiceCount;
            }
            for (int i = 0; i < ChoiceCount; ++i) {
                const UiRect rect = storageActionChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseStorageActionSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseStorageActionSelection_ = i;
                    ui.emitSound(UiSoundEvent::Confirm);
                    baseStorageMode_ = i == 0
                        ? StorageUiMode::Deposit
                        : (i == 1 ? StorageUiMode::Withdraw : StorageUiMode::Bulk);
                    ui.block(storageBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                ui.emitSound(UiSoundEvent::Confirm);
                baseStorageMode_ = baseStorageActionSelection_ == 0
                    ? StorageUiMode::Deposit
                    : (baseStorageActionSelection_ == 1 ? StorageUiMode::Withdraw : StorageUiMode::Bulk);
                ui.block(storageBounds);
                return;
            }
            ui.block(storageBounds);
            return;
        }

        if (baseStorageMode_ == StorageUiMode::Bulk) {
            constexpr int ChoiceCount = 4;
            baseStorageBulkSelection_ = std::clamp(baseStorageBulkSelection_, 0, ChoiceCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseStorageBulkSelection_ = (baseStorageBulkSelection_ + ChoiceCount - 1) % ChoiceCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseStorageBulkSelection_ = (baseStorageBulkSelection_ + 1) % ChoiceCount;
            }

            const auto executeBulkAction = [&](int selection) {
                if (selection == 0) {
                    depositAllStorageItems();
                    ui.emitSound(UiSoundEvent::Confirm);
                    return;
                }
                const int presetIndex = selection - 1;
                if (presetIndex >= unlockedRingPresetSlotCount()) {
                    baseStatus_ = unlockedRingPresetSlotCount() <= 0
                        ? "リングプリセットは未解禁です"
                        : "プリセット" + std::to_string(presetIndex + 1) + "は未解禁です";
                    ui.emitSound(UiSoundEvent::Cancel);
                    return;
                }
                if (!ringPresets_.registered(presetIndex)) {
                    baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "は未登録です";
                    ui.emitSound(UiSoundEvent::Cancel);
                    return;
                }
                prepareRingPresetFromWarehouse(presetIndex);
                ui.emitSound(UiSoundEvent::Confirm);
            };

            for (int i = 0; i < ChoiceCount; ++i) {
                const UiRect rect = storageBulkChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseStorageBulkSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseStorageBulkSelection_ = i;
                    executeBulkAction(i);
                    ui.block(storageBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                executeBulkAction(baseStorageBulkSelection_);
                ui.block(storageBounds);
                return;
            }
            ui.block(storageBounds);
            return;
        }

        if (baseStorageMode_ == StorageUiMode::Deposit) {
            const int sourceCount = storageDepositSourceCountForUnlockedRings(unlockedRingCount());
            baseStorageDepositSource_ = clampStorageDepositSourceForUnlockedRings(baseStorageDepositSource_, unlockedRingCount());
            const int currentTab = storageDepositSourceTabIndex(baseStorageDepositSource_);
            std::array<UiTabItem, StorageDepositSourceCount> sourceTabs{};
            std::array<UiRect, StorageDepositSourceCount> sourceTabRects{};
            for (int i = 0; i < sourceCount; ++i) {
                const int source = storageDepositSourceValue(i);
                sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(source, true);
                sourceTabRects[static_cast<std::size_t>(i)] = storageDepositSourceRect(i);
            }
            UiTabsInput sourceTabsInput{};
            sourceTabsInput.focusDelta = input.activeRingDelta();
            sourceTabsInput.commit =
                sourceTabsInput.focusDelta != 0 ||
                input.confirmPressed() ||
                input.useItemPressed();
            const int sourceSelection = updateUiTabs(
                baseStorageDepositSourceTabs_,
                ui,
                sourceTabsInput,
                currentTab,
                sourceTabs.data(),
                sourceCount,
                sourceTabRects.data());
            if (sourceSelection >= 0) {
                closeStorageCommand();
                resetStoragePointerPress();
                baseStorageDepositSource_ = storageDepositSourceValue(sourceSelection);
                if (baseItemSourceIsRing(baseStorageDepositSource_)) {
                    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseStorageDepositSource_), 0, SpellRingCount - 1);
                    baseStorageDepositSelection_ = std::clamp(
                        baseStorageDepositSelection_,
                        0,
                        std::max(0, static_cast<int>(spellRing_.itemsForRing(ringIndex).size()) - 1));
                } else {
                    baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
                }
                ui.block(storageBounds);
                return;
            }

            if (baseItemSourceIsRing(baseStorageDepositSource_)) {
                const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseStorageDepositSource_), 0, SpellRingCount - 1);
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                const int itemCount = static_cast<int>(ringItems.size());
                if (itemCount <= 0) {
                    baseStorageDepositSelection_ = 0;
                } else {
                    baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, itemCount - 1);
                    const auto moveRingSelection = [&](int delta) {
                        if (delta == 0 || itemCount <= 0) {
                            return;
                        }
                        baseStorageDepositSelection_ = (baseStorageDepositSelection_ + delta) % itemCount;
                        if (baseStorageDepositSelection_ < 0) {
                            baseStorageDepositSelection_ += itemCount;
                        }
                    };
                    moveRingSelection(input.shortcutCursorDelta());
                    if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp)) {
                        moveRingSelection(-1);
                    }
                    if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown)) {
                        moveRingSelection(1);
                    }
                }
                int hoveredRingItem = -1;
                for (int i = 0; i < itemCount; ++i) {
                    const UiRect rect = storageRingItemRect(
                        ringItems[static_cast<std::size_t>(i)],
                        spellRing_,
                        balance_,
                        ringIndex,
                        i,
                        itemCount,
                        ringPreviewSeconds);
                    if (rect.contains(ui.mouse())) {
                        baseStorageDepositSelection_ = i;
                        hoveredRingItem = i;
                    }
                }
                if (input.mouseLeftPressed() && hoveredRingItem >= 0 && !ui.pointerConsumed()) {
                    baseStorageDepositSelection_ = hoveredRingItem;
                    const StorageTransferTarget target = storageDepositTargetForScreenSlot(hoveredRingItem);
                    baseStoragePointerOperation_ = StorageQuantityOperation::Deposit;
                    baseStoragePointerTarget_ = target;
                    baseStoragePointerPressMouse_ = input.mouseScreen();
                    baseStoragePointerPressCanOpenMenu_ = storageTransferTargetAvailable(target);
                    baseStoragePointerDragTriggered_ = false;
                    ui.consumePointer();
                    return;
                }
                if (baseStoragePointerOperation_ == StorageQuantityOperation::Deposit &&
                    baseStoragePointerTarget_.source != BaseItemSource::Backpack &&
                    baseStoragePointerTarget_.source != BaseItemSource::Warehouse &&
                    baseStoragePointerTarget_.ringIndex == ringIndex) {
                    if (input.mouseLeftHeld() &&
                        baseStoragePointerPressCanOpenMenu_ &&
                        !baseStoragePointerDragTriggered_ &&
                        lengthSquared(input.mouseScreen() - baseStoragePointerPressMouse_) >= StorageDragStartDistanceSq) {
                        baseStoragePointerDragTriggered_ = true;
                        baseStoragePointerPressCanOpenMenu_ = false;
                    }
                    if (input.mouseLeftReleased()) {
                        if (!baseStoragePointerDragTriggered_ &&
                            baseStoragePointerPressCanOpenMenu_ &&
                            hoveredRingItem == baseStoragePointerTarget_.ringItemIndex) {
                            baseStorageDepositSelection_ = hoveredRingItem;
                            openStorageCommand(
                                StorageQuantityOperation::Deposit,
                                baseStoragePointerTarget_,
                                input.mouseScreen());
                        }
                        resetStoragePointerPress();
                        ui.block(storageBounds);
                        return;
                    }
                }
                if (input.confirmPressed() || input.useItemPressed()) {
                    const UiRect rect = baseStorageDepositSelection_ >= 0 && baseStorageDepositSelection_ < itemCount
                        ? storageRingItemRect(
                            ringItems[static_cast<std::size_t>(baseStorageDepositSelection_)],
                            spellRing_,
                            balance_,
                            ringIndex,
                            baseStorageDepositSelection_,
                            itemCount,
                            ringPreviewSeconds)
                        : storageTransferGridSlotRect(0);
                    openStorageCommand(
                        StorageQuantityOperation::Deposit,
                        storageDepositTargetForScreenSlot(baseStorageDepositSelection_),
                        rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                    ui.block(storageBounds);
                    return;
                }
                ui.block(storageBounds);
                return;
            }

            if (input.arrangeItemsPressed() || ui.pressed(storageTransferSortButtonRect())) {
                closeStorageCommand();
                resetStoragePointerPress();
                const bool sorted = inventory_.sortByCatalogOrder(objectCatalog_);
                ui.emitSound(sorted ? UiSoundEvent::ItemMove : UiSoundEvent::Cancel);
                baseStorageDepositSelection_ = 0;
                baseStatus_ = sorted ? "リュックを並び替えました" : "リュックは空です";
                ui.block(storageBounds);
                return;
            }

            moveGridSelection(baseStorageDepositSelection_, inventory_.screenSlotCount());
            int hoveredBackpackSlot = -1;
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const UiRect rect = storageTransferGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseStorageDepositSelection_ = i;
                    hoveredBackpackSlot = i;
                }
            }
            const auto moveBackpackStorageItem = [this](StorageTransferTarget target, int toSlot) {
                if (target.source != BaseItemSource::Backpack || toSlot < 0 || toSlot >= inventory_.screenSlotCount()) {
                    return false;
                }
                if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
                    return inventory_.moveObjectStackToScreenSlot(stack->objectId, toSlot);
                }
                if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                    return inventory_.moveObjectInstanceToScreenSlot(instance->instance.instanceId, toSlot);
                }
                return false;
            };
            if (input.mouseLeftPressed() && hoveredBackpackSlot >= 0 && !ui.pointerConsumed()) {
                baseStorageDepositSelection_ = hoveredBackpackSlot;
                const StorageTransferTarget target = storageDepositTargetForScreenSlot(hoveredBackpackSlot);
                baseStoragePointerOperation_ = StorageQuantityOperation::Deposit;
                baseStoragePointerTarget_ = target;
                baseStoragePointerPressMouse_ = input.mouseScreen();
                baseStoragePointerPressCanOpenMenu_ = storageTransferTargetAvailable(target);
                baseStoragePointerDragTriggered_ = false;
                ui.consumePointer();
                return;
            }
            if (baseStoragePointerOperation_ == StorageQuantityOperation::Deposit &&
                baseStoragePointerTarget_.source == BaseItemSource::Backpack) {
                if (input.mouseLeftHeld() &&
                    baseStoragePointerPressCanOpenMenu_ &&
                    !baseStoragePointerDragTriggered_ &&
                    lengthSquared(input.mouseScreen() - baseStoragePointerPressMouse_) >= StorageDragStartDistanceSq) {
                    baseStoragePointerDragTriggered_ = true;
                    baseStoragePointerPressCanOpenMenu_ = false;
                    closeStorageCommand();
                    baseStatus_.clear();
                }
                if (input.mouseLeftReleased()) {
                    if (baseStoragePointerDragTriggered_) {
                        if (hoveredBackpackSlot >= 0 &&
                            moveBackpackStorageItem(baseStoragePointerTarget_, hoveredBackpackSlot)) {
                            ui.emitSound(UiSoundEvent::ItemMove);
                            baseStorageDepositSelection_ = hoveredBackpackSlot;
                            baseStatus_.clear();
                        }
                    } else if (baseStoragePointerPressCanOpenMenu_ &&
                        hoveredBackpackSlot == baseStoragePointerTarget_.slotIndex) {
                        const UiRect rect = storageTransferGridSlotRect(hoveredBackpackSlot);
                        openStorageCommand(
                            StorageQuantityOperation::Deposit,
                            baseStoragePointerTarget_,
                            rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                    }
                    resetStoragePointerPress();
                    ui.block(storageBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                const UiRect rect = storageTransferGridSlotRect(baseStorageDepositSelection_);
                openStorageCommand(
                    StorageQuantityOperation::Deposit,
                    storageDepositTargetForScreenSlot(baseStorageDepositSelection_),
                    rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                ui.block(storageBounds);
                return;
            }
            ui.block(storageBounds);
            return;
        }

        if (baseStorageMode_ == StorageUiMode::Withdraw) {
            const int warehousePageCount = std::max(1, (warehouseCapacity() + StorageWithdrawSlotCount - 1) / StorageWithdrawSlotCount);
            baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
            const UiPageSelectorRects pageRects = storageWithdrawPageSelectorRects();
            if (input.arrangeItemsPressed() || ui.pressed(storageWithdrawSortButtonRect())) {
                const bool hasItems = warehouseUsedSlots() > 0;
                ui.emitSound(hasItems ? UiSoundEvent::ItemMove : UiSoundEvent::Cancel);
                sortWarehouseByCatalogOrder();
                ui.block(storageBounds);
                return;
            }
            if (input.activeRingDelta() != 0) {
                closeStorageCommand();
                resetStoragePointerPress();
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, input.activeRingDelta(), warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }
            if (ui.pressed(pageRects.prev)) {
                closeStorageCommand();
                resetStoragePointerPress();
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, -1, warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }
            if (ui.pressed(pageRects.next)) {
                closeStorageCommand();
                resetStoragePointerPress();
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, 1, warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }

            moveGridSelection(baseStorageWithdrawSelection_, StorageWithdrawSlotCount);
            int hoveredWarehouseSlot = -1;
            for (int i = 0; i < StorageWithdrawSlotCount; ++i) {
                const UiRect rect = storageWithdrawSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseStorageWithdrawSelection_ = i;
                    hoveredWarehouseSlot = i;
                }
            }
            const auto moveWarehouseStorageItem = [this](StorageTransferTarget target, int toSlot) {
                if (target.source != BaseItemSource::Warehouse ||
                    !target.valid ||
                    toSlot < 0 ||
                    toSlot >= StorageWithdrawSlotCount) {
                    return false;
                }
                const int entryIndex = target.storageEntry.kind == StorageEntryKind::Stack
                    ? target.storageEntry.index
                    : static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index;
                const int storageSlot = baseStorageWarehousePage_ * StorageWithdrawSlotCount + toSlot;
                if (storageSlot >= warehouseCapacity()) {
                    return false;
                }
                assignWarehouseEntryToStorageSlot(entryIndex, storageSlot);
                return true;
            };
            if (input.mouseLeftPressed() && hoveredWarehouseSlot >= 0 && !ui.pointerConsumed()) {
                baseStorageWithdrawSelection_ = hoveredWarehouseSlot;
                const StorageTransferTarget target = storageWithdrawTargetForSlot(hoveredWarehouseSlot);
                baseStoragePointerOperation_ = StorageQuantityOperation::Withdraw;
                baseStoragePointerTarget_ = target;
                baseStoragePointerPressMouse_ = input.mouseScreen();
                baseStoragePointerPressCanOpenMenu_ = storageTransferTargetAvailable(target);
                baseStoragePointerDragTriggered_ = false;
                ui.consumePointer();
                return;
            }
            if (baseStoragePointerOperation_ == StorageQuantityOperation::Withdraw &&
                baseStoragePointerTarget_.source == BaseItemSource::Warehouse) {
                if (input.mouseLeftHeld() &&
                    baseStoragePointerPressCanOpenMenu_ &&
                    !baseStoragePointerDragTriggered_ &&
                    lengthSquared(input.mouseScreen() - baseStoragePointerPressMouse_) >= StorageDragStartDistanceSq) {
                    baseStoragePointerDragTriggered_ = true;
                    baseStoragePointerPressCanOpenMenu_ = false;
                    closeStorageCommand();
                    baseStatus_.clear();
                }
                if (input.mouseLeftReleased()) {
                    if (baseStoragePointerDragTriggered_) {
                        if (hoveredWarehouseSlot >= 0 &&
                            moveWarehouseStorageItem(baseStoragePointerTarget_, hoveredWarehouseSlot)) {
                            ui.emitSound(UiSoundEvent::ItemMove);
                            baseStorageWithdrawSelection_ = hoveredWarehouseSlot;
                            baseStatus_.clear();
                        }
                    } else if (baseStoragePointerPressCanOpenMenu_ &&
                        hoveredWarehouseSlot == baseStoragePointerTarget_.slotIndex) {
                        const UiRect rect = storageWithdrawSlotRect(hoveredWarehouseSlot);
                        openStorageCommand(
                            StorageQuantityOperation::Withdraw,
                            baseStoragePointerTarget_,
                            rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                    }
                    resetStoragePointerPress();
                    ui.block(storageBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                const UiRect rect = storageWithdrawSlotRect(baseStorageWithdrawSelection_);
                openStorageCommand(
                    StorageQuantityOperation::Withdraw,
                    storageWithdrawTargetForSlot(baseStorageWithdrawSelection_),
                    rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                ui.block(storageBounds);
                return;
            }
            ui.block(storageBounds);
            return;
        }

        returnToStorageMenu();
        ui.block(storageBounds);
        return;
    }

    if (baseProcessingUiMode_ != ProcessingUiMode::Closed) {
        const UiRect processingBounds = baseProcessingUiMode_ == ProcessingUiMode::ChooseAction
            ? merchantActionDialogRect()
            : merchantPanelRect();
        if (uiCancelRequested(baseCancelState_, input, ui, processingBounds)) {
            if (baseProcessingUiMode_ == ProcessingUiMode::Enhance && baseProcessingCommandMenu_.open) {
                closeUiCommandMenu(baseProcessingCommandMenu_);
                baseProcessingCommandSlot_ = -1;
            } else if (baseProcessingUiMode_ == ProcessingUiMode::Enhance) {
                baseProcessingUiMode_ = ProcessingUiMode::ChooseAction;
                baseProcessingConfirm_ = {};
                baseProcessingConfirmTarget_ = {};
                baseStatus_.clear();
            } else {
                baseProcessingUiMode_ = ProcessingUiMode::Closed;
                baseStatus_.clear();
            }
            ui.block(processingBounds);
            return;
        }

        if (baseProcessingUiMode_ == ProcessingUiMode::ChooseAction) {
            closeUiCommandMenu(baseProcessingCommandMenu_);
            baseProcessingCommandSlot_ = -1;
            constexpr int ChoiceCount = 2;
            baseProcessingActionSelection_ = std::clamp(baseProcessingActionSelection_, 0, ChoiceCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseProcessingActionSelection_ = (baseProcessingActionSelection_ + ChoiceCount - 1) % ChoiceCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseProcessingActionSelection_ = (baseProcessingActionSelection_ + 1) % ChoiceCount;
            }
            const auto chooseProcessingAction = [this, &ui]() {
                ui.emitSound(UiSoundEvent::Confirm);
                if (baseProcessingActionSelection_ == 0) {
                    applyProcessingBulkRepair();
                } else {
                    baseProcessingUiMode_ = ProcessingUiMode::Enhance;
                    baseProcessingMode_ = static_cast<int>(ProcessingMode::Attack);
                    baseProcessingTabs_.focusedIndex = baseProcessingMode_;
                    baseProcessingSource_ = 0;
                    baseProcessingSourceTabs_.focusedIndex = baseProcessingSource_;
                    baseProcessingSelection_ = 0;
                    baseStatus_.clear();
                }
            };
            for (int i = 0; i < ChoiceCount; ++i) {
                const UiRect rect = merchantActionChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseProcessingActionSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseProcessingActionSelection_ = i;
                    chooseProcessingAction();
                    ui.block(processingBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                chooseProcessingAction();
                ui.block(processingBounds);
                return;
            }
            ui.block(processingBounds);
            return;
        }

        const auto closeProcessingCommand = [this]() {
            closeUiCommandMenu(baseProcessingCommandMenu_);
            baseProcessingCommandSlot_ = -1;
        };
        const auto openProcessingCommand = [&](int slotIndex) {
            const ProcessingTarget target = processingTargetForScreenSlot(slotIndex);
            if (!target.valid) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "加工対象がありません";
                return false;
            }
            if (!processingTargetHasAvailableCommand(target)) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "このアイテムにできる作業がありません";
                return false;
            }
            const std::vector<UiCommandMenuItem> items = processingCommandItems(target);
            baseProcessingCommandSlot_ = slotIndex;
            Vec2 commandAnchor = uiCommandMenuAnchorForSlot(baseProcessingGridSlotRect(slotIndex));
            if (target.source == BaseItemSource::Warehouse) {
                commandAnchor = uiCommandMenuAnchorForSlot(
                    externalWarehouseSourceSlotRect(baseProcessingGridSlotRect, slotIndex));
            } else if (target.source != BaseItemSource::Backpack &&
                target.ringIndex >= 0 &&
                target.ringIndex < SpellRingCount) {
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
                if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size())) {
                    commandAnchor = baseProcessingRingItemRect(
                        ringItems[static_cast<std::size_t>(target.ringItemIndex)],
                        spellRing_,
                        balance_,
                        target.ringIndex,
                        target.ringItemIndex,
                        static_cast<int>(ringItems.size()),
                        ringPreviewSeconds).pos;
                }
            }
            openUiCommandMenu(
                baseProcessingCommandMenu_,
                commandAnchor,
                merchantPanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                184.0f,
                2);
            return true;
        };
        const ProcessingTarget commandTarget = baseProcessingCommandSlot_ >= 0
            ? processingTargetForScreenSlot(baseProcessingCommandSlot_)
            : ProcessingTarget{};
        const std::vector<UiCommandMenuItem> commandItems = processingCommandItems(commandTarget);
        const bool commandOpenBeforeUpdate = baseProcessingCommandMenu_.open;
        const int commandSelection = updateUiCommandMenu(
            baseProcessingCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0 && baseProcessingCommandSlot_ >= 0) {
            const std::vector<ProcessingMode> commandModes = processingCommandModes(commandTarget);
            if (commandSelection < static_cast<int>(commandModes.size())) {
                openProcessingConfirm(
                    processingTargetForScreenSlot(baseProcessingCommandSlot_),
                    commandModes[static_cast<std::size_t>(commandSelection)]);
            }
            closeProcessingCommand();
            ui.block(merchantPanelRect());
            return;
        } else if (!baseProcessingCommandMenu_.open && commandOpenBeforeUpdate) {
            baseProcessingCommandSlot_ = -1;
        }
        if (baseProcessingCommandMenu_.open) {
            ui.block(merchantPanelRect());
            return;
        }

        const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount());
        baseProcessingSource_ = clampBaseItemSourceForUnlockedRings(baseProcessingSource_, unlockedRingCount());
        if (roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan &&
            baseItemSourceIsWarehouse(baseProcessingSource_)) {
            baseProcessingSource_ = BaseBackpackSourceIndex;
        }
        std::array<UiTabItem, BaseProcessingSourceCount> sourceTabs{};
        std::array<UiRect, BaseProcessingSourceCount> sourceTabRects{};
        for (int i = 0; i < sourceCount; ++i) {
            const bool enabled = !(roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan &&
                baseItemSourceIsWarehouse(i));
            sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(i, enabled);
            sourceTabRects[static_cast<std::size_t>(i)] = baseProcessingSourceRect(i, sourceCount);
        }
        UiTabsInput sourceTabsInput{};
        sourceTabsInput.focusDelta = baseItemSourceIsWarehouse(baseProcessingSource_) ? 0 : input.activeRingDelta();
        const int directSourceFocus = input.shortcutSlotPressed();
        if (directSourceFocus >= 0 && directSourceFocus < sourceCount) {
            sourceTabsInput.directFocusIndex = directSourceFocus;
        }
        sourceTabsInput.commit =
            sourceTabsInput.focusDelta != 0 ||
            sourceTabsInput.directFocusIndex >= 0 ||
            input.confirmPressed() ||
            input.useItemPressed();
        const int sourceSelection = updateUiTabs(
            baseProcessingSourceTabs_,
            ui,
            sourceTabsInput,
            baseProcessingSource_,
            sourceTabs.data(),
            sourceCount,
            sourceTabRects.data());
        if (sourceSelection >= 0) {
            baseProcessingSource_ = sourceSelection;
            int sourceSlotCount = inventory_.screenSlotCount();
            if (baseItemSourceIsWarehouse(baseProcessingSource_)) {
                sourceSlotCount = StoragePaneSlotCount;
            } else if (baseItemSourceIsRing(baseProcessingSource_)) {
                const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseProcessingSource_), 0, SpellRingCount - 1);
                sourceSlotCount = std::max(1, static_cast<int>(spellRing_.itemsForRing(ringIndex).size()));
            }
            baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, sourceSlotCount - 1));
            closeProcessingCommand();
            return;
        }

        if (baseItemSourceIsWarehouse(baseProcessingSource_)) {
            const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
            baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
            const UiPageSelectorRects pageRects = externalWarehousePageSelectorRects(baseProcessingGridSlotRect);
            if (input.activeRingDelta() != 0) {
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, input.activeRingDelta(), warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }
            if (ui.pressed(pageRects.prev)) {
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, -1, warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }
            if (ui.pressed(pageRects.next)) {
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, 1, warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }

            baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, StoragePaneSlotCount - 1);
            if (input.pressed(InputAction::MoveLeft)) {
                baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                baseProcessingSelection_ = std::min(StoragePaneSlotCount - 1, baseProcessingSelection_ + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - StorageColumns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseProcessingSelection_ = std::min(StoragePaneSlotCount - 1, baseProcessingSelection_ + StorageColumns);
            }
            for (int i = 0; i < StoragePaneSlotCount; ++i) {
                const UiRect rect = externalWarehouseSourceSlotRect(baseProcessingGridSlotRect, i);
                if (rect.contains(ui.mouse())) {
                    baseProcessingSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseProcessingSelection_ = i;
                    openProcessingCommand(i);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openProcessingCommand(baseProcessingSelection_);
                return;
            }
            ui.block(merchantPanelRect());
            return;
        }

        if (baseItemSourceIsRing(baseProcessingSource_)) {
            const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseProcessingSource_), 0, SpellRingCount - 1);
            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
            const int itemCount = static_cast<int>(ringItems.size());
            if (itemCount <= 0) {
                baseProcessingSelection_ = 0;
            } else {
                baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, itemCount - 1);
                const auto moveRingSelection = [&](int delta) {
                    if (delta == 0 || itemCount <= 0) {
                        return;
                    }
                    baseProcessingSelection_ = (baseProcessingSelection_ + delta) % itemCount;
                    if (baseProcessingSelection_ < 0) {
                        baseProcessingSelection_ += itemCount;
                    }
                };
                moveRingSelection(input.shortcutCursorDelta());
                if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp)) {
                    moveRingSelection(-1);
                }
                if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown)) {
                    moveRingSelection(1);
                }
            }

            for (int i = 0; i < itemCount; ++i) {
                const UiRect rect = baseProcessingRingItemRect(
                    ringItems[static_cast<std::size_t>(i)],
                    spellRing_,
                    balance_,
                    ringIndex,
                    i,
                    itemCount,
                    ringPreviewSeconds);
                if (rect.contains(ui.mouse())) {
                    baseProcessingSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseProcessingSelection_ = i;
                    openProcessingCommand(i);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openProcessingCommand(baseProcessingSelection_);
                return;
            }
            ui.block(merchantPanelRect());
            return;
        }

        constexpr int Columns = 8;
        const int slotCount = inventory_.screenSlotCount();
        baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, slotCount - 1));
        if (input.pressed(InputAction::MoveLeft)) {
            baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - 1);
        }
        if (input.pressed(InputAction::MoveRight)) {
            baseProcessingSelection_ = std::min(slotCount - 1, baseProcessingSelection_ + 1);
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - Columns);
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseProcessingSelection_ = std::min(slotCount - 1, baseProcessingSelection_ + Columns);
        }
        for (int i = 0; i < slotCount; ++i) {
            const UiRect rect = baseProcessingGridSlotRect(i);
            if (rect.contains(ui.mouse())) {
                baseProcessingSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseProcessingSelection_ = i;
                openProcessingCommand(i);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            openProcessingCommand(baseProcessingSelection_);
            return;
        }
        ui.block(merchantPanelRect());
        return;
    }

    if (baseSellActive_) {
        refreshMerchantStock(false);
        const UiRect merchantBounds = baseMerchantMode_ == MerchantUiMode::ChooseAction ? merchantActionDialogRect() : merchantPanelRect();
        const auto closeMerchantCommands = [this]() {
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandSource_ = 0;
            baseMerchantSellCommandIndex_ = -1;
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
        };
        const auto closeMerchant = [&]() {
            closeMerchantCommands();
            baseSellActive_ = false;
            baseMerchantMode_ = MerchantUiMode::Closed;
            baseStatus_.clear();
        };
        const auto returnToMerchantMenu = [&]() {
            closeMerchantCommands();
            baseMerchantMode_ = MerchantUiMode::ChooseAction;
            baseMerchantActionSelection_ = 0;
            baseStatus_.clear();
        };
        const auto moveGridSelection = [&input](int& selection, int count) {
            constexpr int Columns = 8;
            if (count <= 0) {
                selection = 0;
                return;
            }
            selection = std::clamp(selection, 0, count - 1);
            if (input.pressed(InputAction::MoveLeft)) {
                selection = std::max(0, selection - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                selection = std::min(count - 1, selection + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                selection = std::max(0, selection - Columns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                selection = std::min(count - 1, selection + Columns);
            }
        };
        const auto merchantSellSourceSlotCount = [&]() {
            if (baseMerchantSellSource_ == 0) {
                return inventory_.screenSlotCount();
            }
            if (baseItemSourceIsWarehouse(baseMerchantSellSource_)) {
                return StoragePaneSlotCount;
            }
            const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseMerchantSellSource_), 0, SpellRingCount - 1);
            return static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
        };
        const auto sortMerchantSellSource = [&]() {
            closeMerchantCommands();
            baseSellSelection_ = 0;
            if (baseMerchantSellSource_ == BaseBackpackSourceIndex) {
                const bool sorted = inventory_.sortByCatalogOrder(objectCatalog_);
                baseStatus_ = sorted ? "リュックを並び替えました" : "リュックは空です";
                ui.emitSound(sorted ? UiSoundEvent::ItemMove : UiSoundEvent::Cancel);
                return;
            }
            if (baseItemSourceIsWarehouse(baseMerchantSellSource_)) {
                const bool hasItems = warehouseUsedSlots() > 0;
                sortWarehouseByCatalogOrder();
                ui.emitSound(hasItems ? UiSoundEvent::ItemMove : UiSoundEvent::Cancel);
                return;
            }
            if (baseItemSourceIsRing(baseMerchantSellSource_)) {
                const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseMerchantSellSource_), 0, SpellRingCount - 1);
                std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                if (ringItems.empty()) {
                    baseStatus_ = "リングは空です";
                    ui.emitSound(UiSoundEvent::Cancel);
                    return;
                }
                const auto order = buildObjectSortOrder(objectCatalog_);
                std::stable_sort(ringItems.begin(), ringItems.end(), [&order](const SpellRingItem& a, const SpellRingItem& b) {
                    const int orderA = objectSortOrder(order, a.objectId);
                    const int orderB = objectSortOrder(order, b.objectId);
                    if (orderA != orderB) {
                        return orderA < orderB;
                    }
                    if (a.objectId != b.objectId) {
                        return a.objectId < b.objectId;
                    }
                    return a.instanceId < b.instanceId;
                });
                baseStatus_ = "リングを並び替えました";
                ui.emitSound(UiSoundEvent::ItemMove);
            }
        };
        const auto openSellCommand = [&](int slotIndex) {
            const MerchantSellTarget target = merchantSellTargetForScreenSlot(slotIndex);
            if (!target.valid) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "売却対象がありません";
                return;
            }
            if (!merchantSellTargetAvailable(target)) {
                ui.emitSound(UiSoundEvent::Confirm);
                sellMerchantTarget(target, 1);
                return;
            }
            const bool stackItem =
                (target.source == BaseItemSource::Backpack && inventory_.screenObjectStackAt(slotIndex) != nullptr) ||
                (target.source == BaseItemSource::Warehouse && target.storageEntry.kind == StorageEntryKind::Stack);
            baseMerchantSellCommandIndex_ = slotIndex;
            baseMerchantSellCommandSource_ = baseMerchantSellSource_;
            Vec2 commandAnchor = uiCommandMenuAnchorForSlot(merchantSellGridSlotRect(slotIndex));
            if (target.source == BaseItemSource::Warehouse) {
                commandAnchor = uiCommandMenuAnchorForSlot(
                    externalWarehouseSourceSlotRect(merchantSellGridSlotRect, slotIndex));
            } else if (target.source != BaseItemSource::Backpack &&
                target.ringIndex >= 0 &&
                target.ringIndex < SpellRingCount) {
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
                if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size())) {
                    commandAnchor = merchantSellRingItemRect(
                        ringItems[static_cast<std::size_t>(target.ringItemIndex)],
                        spellRing_,
                        balance_,
                        target.ringIndex,
                        target.ringItemIndex,
                        static_cast<int>(ringItems.size()),
                        ringPreviewSeconds).pos;
                }
            }
            const std::array<UiCommandMenuItem, 2> items{{{stackItem ? "1個売る" : "売る", true}, {"すべて売る", stackItem}}};
            openUiCommandMenu(
                baseMerchantSellCommandMenu_,
                commandAnchor,
                merchantPanelRect(),
                stackItem ? 2 : 1,
                items.data(),
                168.0f,
                2);
        };
        const auto openBuyCommand = [&](int index) {
            if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "購入できる商品がありません";
                return;
            }
            baseMerchantBuyCommandIndex_ = index;
            const std::array<UiCommandMenuItem, 1> items{{{"買う", canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(index)])}}};
            openUiCommandMenu(
                baseMerchantBuyCommandMenu_,
                uiCommandMenuAnchorForSlot(merchantGridSlotRect(index)),
                merchantPanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                120.0f,
                2);
        };

        if (uiCancelRequested(baseCancelState_, input, ui, merchantBounds)) {
            if (baseMerchantSellCommandMenu_.open || baseMerchantBuyCommandMenu_.open) {
                closeMerchantCommands();
            } else if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
                closeMerchant();
            } else {
                returnToMerchantMenu();
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
            closeMerchantCommands();
            constexpr int ChoiceCount = 2;
            baseMerchantActionSelection_ = std::clamp(baseMerchantActionSelection_, 0, ChoiceCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseMerchantActionSelection_ = (baseMerchantActionSelection_ + ChoiceCount - 1) % ChoiceCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseMerchantActionSelection_ = (baseMerchantActionSelection_ + 1) % ChoiceCount;
            }
            for (int i = 0; i < ChoiceCount; ++i) {
                const UiRect rect = merchantActionChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseMerchantActionSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseMerchantActionSelection_ = i;
                    ui.emitSound(UiSoundEvent::Confirm);
                    if (i == 0) {
                        baseMerchantMode_ = MerchantUiMode::Buy;
                    } else if (i == 1) {
                        baseMerchantMode_ = MerchantUiMode::Sell;
                    }
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                ui.emitSound(UiSoundEvent::Confirm);
                if (baseMerchantActionSelection_ == 0) {
                    baseMerchantMode_ = MerchantUiMode::Buy;
                } else if (baseMerchantActionSelection_ == 1) {
                    baseMerchantMode_ = MerchantUiMode::Sell;
                }
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::Sell) {
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
            const MerchantSellTarget commandTarget = merchantSellTargetForSourceSlot(
                baseMerchantSellCommandSource_,
                baseMerchantSellCommandIndex_);
            const bool stackCommand =
                (commandTarget.source == BaseItemSource::Backpack &&
                    baseMerchantSellCommandIndex_ >= 0 &&
                    inventory_.screenObjectStackAt(baseMerchantSellCommandIndex_) != nullptr) ||
                (commandTarget.source == BaseItemSource::Warehouse &&
                    commandTarget.storageEntry.kind == StorageEntryKind::Stack);
            const std::array<UiCommandMenuItem, 2> commandItems{{{stackCommand ? "1個売る" : "売る", true}, {"すべて売る", stackCommand}}};
            const int commandItemCount = stackCommand ? 2 : 1;
            const bool commandOpenBeforeUpdate = baseMerchantSellCommandMenu_.open;
            const int commandSelection = updateUiCommandMenu(
                baseMerchantSellCommandMenu_,
                ui,
                input,
                commandItems.data(),
                commandItemCount);
            if (commandSelection >= 0 && baseMerchantSellCommandIndex_ >= 0) {
                sellMerchantTarget(commandTarget, commandSelection == 1 && stackCommand ? 0 : 1);
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            } else if (!baseMerchantSellCommandMenu_.open && commandOpenBeforeUpdate) {
                baseMerchantSellCommandSource_ = 0;
                baseMerchantSellCommandIndex_ = -1;
            }
            if (baseMerchantSellCommandMenu_.open) {
                ui.block(merchantBounds);
                return;
            }

            const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount());
            baseMerchantSellSource_ = clampBaseItemSourceForUnlockedRings(baseMerchantSellSource_, unlockedRingCount());
            if (roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Merchant &&
                baseItemSourceIsWarehouse(baseMerchantSellSource_)) {
                baseMerchantSellSource_ = BaseBackpackSourceIndex;
            }
            std::array<UiTabItem, BaseItemSourceCount> sourceTabs{};
            std::array<UiRect, BaseItemSourceCount> sourceTabRects{};
            for (int i = 0; i < sourceCount; ++i) {
                const bool enabled = !(roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Merchant &&
                    baseItemSourceIsWarehouse(i));
                sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(i, enabled);
                sourceTabRects[static_cast<std::size_t>(i)] = merchantSellSourceRect(i, sourceCount);
            }
            UiTabsInput sourceTabsInput{};
            sourceTabsInput.focusDelta = baseItemSourceIsWarehouse(baseMerchantSellSource_) ? 0 : input.activeRingDelta();
            const int directSourceFocus = input.shortcutSlotPressed();
            if (directSourceFocus >= 0 && directSourceFocus < sourceCount) {
                sourceTabsInput.directFocusIndex = directSourceFocus;
            }
            sourceTabsInput.commit =
                sourceTabsInput.focusDelta != 0 ||
                sourceTabsInput.directFocusIndex >= 0 ||
                input.confirmPressed() ||
                input.useItemPressed();
            const int sourceSelection = updateUiTabs(
                baseMerchantSellSourceTabs_,
                ui,
                sourceTabsInput,
                baseMerchantSellSource_,
                sourceTabs.data(),
                sourceCount,
                sourceTabRects.data());
            if (sourceSelection >= 0) {
                baseMerchantSellSource_ = sourceSelection;
                baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, merchantSellSourceSlotCount() - 1));
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            }

            if (input.arrangeItemsPressed() || ui.pressed(merchantSellSortButtonRect())) {
                sortMerchantSellSource();
                ui.block(merchantBounds);
                return;
            }

            if (baseItemSourceIsWarehouse(baseMerchantSellSource_)) {
                const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
                baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                const UiPageSelectorRects pageRects = externalWarehousePageSelectorRects(merchantSellGridSlotRect);
                if (input.activeRingDelta() != 0) {
                    const int previousPage = baseStorageWarehousePage_;
                    baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, input.activeRingDelta(), warehousePageCount);
                    if (baseStorageWarehousePage_ != previousPage) {
                        ui.emitSound(UiSoundEvent::TabSwitch);
                    }
                }
                if (ui.pressed(pageRects.prev)) {
                    const int previousPage = baseStorageWarehousePage_;
                    baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, -1, warehousePageCount);
                    if (baseStorageWarehousePage_ != previousPage) {
                        ui.emitSound(UiSoundEvent::TabSwitch);
                    }
                }
                if (ui.pressed(pageRects.next)) {
                    const int previousPage = baseStorageWarehousePage_;
                    baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, 1, warehousePageCount);
                    if (baseStorageWarehousePage_ != previousPage) {
                        ui.emitSound(UiSoundEvent::TabSwitch);
                    }
                }

                moveGridSelection(baseSellSelection_, StoragePaneSlotCount);
                for (int i = 0; i < StoragePaneSlotCount; ++i) {
                    const UiRect rect = externalWarehouseSourceSlotRect(merchantSellGridSlotRect, i);
                    if (rect.contains(ui.mouse())) {
                        baseSellSelection_ = i;
                    }
                    if (ui.pressed(rect)) {
                        baseSellSelection_ = i;
                        openSellCommand(i);
                        ui.block(merchantBounds);
                        return;
                    }
                }
                if (input.confirmPressed() || input.useItemPressed()) {
                    openSellCommand(baseSellSelection_);
                    ui.block(merchantBounds);
                    return;
                }
                ui.block(merchantBounds);
                return;
            }

            if (baseItemSourceIsRing(baseMerchantSellSource_)) {
                const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseMerchantSellSource_), 0, SpellRingCount - 1);
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                const int itemCount = static_cast<int>(ringItems.size());
                if (itemCount <= 0) {
                    baseSellSelection_ = 0;
                } else {
                    baseSellSelection_ = std::clamp(baseSellSelection_, 0, itemCount - 1);
                    const auto moveRingSelection = [&](int delta) {
                        if (delta == 0 || itemCount <= 0) {
                            return;
                        }
                        baseSellSelection_ = (baseSellSelection_ + delta) % itemCount;
                        if (baseSellSelection_ < 0) {
                            baseSellSelection_ += itemCount;
                        }
                    };
                    moveRingSelection(input.shortcutCursorDelta());
                    if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp)) {
                        moveRingSelection(-1);
                    }
                    if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown)) {
                        moveRingSelection(1);
                    }
                }

                for (int i = 0; i < itemCount; ++i) {
                    const UiRect rect = merchantSellRingItemRect(
                        ringItems[static_cast<std::size_t>(i)],
                        spellRing_,
                        balance_,
                        ringIndex,
                        i,
                        itemCount,
                        ringPreviewSeconds);
                    if (rect.contains(ui.mouse())) {
                        baseSellSelection_ = i;
                    }
                    if (ui.pressed(rect)) {
                        baseSellSelection_ = i;
                        openSellCommand(i);
                        ui.block(merchantBounds);
                        return;
                    }
                }
                if (input.confirmPressed() || input.useItemPressed()) {
                    openSellCommand(baseSellSelection_);
                    ui.block(merchantBounds);
                    return;
                }
                ui.block(merchantBounds);
                return;
            }

            moveGridSelection(baseSellSelection_, inventory_.screenSlotCount());
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const UiRect rect = merchantSellGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseSellSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseSellSelection_ = i;
                    openSellCommand(i);
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openSellCommand(baseSellSelection_);
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::Buy) {
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandSource_ = 0;
            baseMerchantSellCommandIndex_ = -1;
            const bool commandEnabled = baseMerchantBuyCommandIndex_ >= 0 &&
                baseMerchantBuyCommandIndex_ < static_cast<int>(merchantStock_.size()) &&
                canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(baseMerchantBuyCommandIndex_)]);
            const std::array<UiCommandMenuItem, 1> commandItems{{{"買う", commandEnabled}}};
            const bool commandOpenBeforeUpdate = baseMerchantBuyCommandMenu_.open;
            const int commandSelection = updateUiCommandMenu(
                baseMerchantBuyCommandMenu_,
                ui,
                input,
                commandItems.data(),
                static_cast<int>(commandItems.size()));
            if (commandSelection >= 0 && baseMerchantBuyCommandIndex_ >= 0) {
                buyMerchantProduct(baseMerchantBuyCommandIndex_);
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            } else if (!baseMerchantBuyCommandMenu_.open && commandOpenBeforeUpdate) {
                baseMerchantBuyCommandIndex_ = -1;
            }
            if (baseMerchantBuyCommandMenu_.open) {
                ui.block(merchantBounds);
                return;
            }

            moveGridSelection(baseMerchantBuySelection_, static_cast<int>(merchantStock_.size()));
            for (int i = 0; i < static_cast<int>(merchantStock_.size()); ++i) {
                const UiRect rect = merchantGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseMerchantBuySelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseMerchantBuySelection_ = i;
                    openBuyCommand(i);
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openBuyCommand(baseMerchantBuySelection_);
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        returnToMerchantMenu();
        ui.block(merchantBounds);
        return;
    }

    if (baseUpgradeActive_) {
        const UiRect upgradePanel = baseUpgradePanelRect();
        const bool roguelikeTrainer = roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Trainer;
        const int upgradeDisplayCount = baseUpgradeDisplayCount(roguelikeTrainer);
        int displaySelection = baseUpgradeDisplayForIndex(roguelikeTrainer, baseUpgradeSelection_);
        baseUpgradeSelection_ = baseUpgradeIndexForDisplay(roguelikeTrainer, displaySelection);
        baseUpgradeTabs_.focusedIndex = displaySelection;
        if (uiCancelRequested(baseCancelState_, input, ui, upgradePanel)) {
            baseUpgradeActive_ = false;
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            displaySelection = (displaySelection + upgradeDisplayCount - 1) % upgradeDisplayCount;
            baseUpgradeSelection_ = baseUpgradeIndexForDisplay(roguelikeTrainer, displaySelection);
            baseUpgradeTabs_.focusedIndex = displaySelection;
        }
        if (input.pressed(InputAction::MoveDown)) {
            displaySelection = (displaySelection + 1) % upgradeDisplayCount;
            baseUpgradeSelection_ = baseUpgradeIndexForDisplay(roguelikeTrainer, displaySelection);
            baseUpgradeTabs_.focusedIndex = displaySelection;
        }
        std::array<UiVerticalTabItem, BaseUpgradeItemCount> upgradeTabs{};
        std::array<UiRect, BaseUpgradeItemCount> upgradeTabRects{};
        for (int i = 0; i < upgradeDisplayCount; ++i) {
            const int upgradeIndex = baseUpgradeIndexForDisplay(roguelikeTrainer, i);
            upgradeTabs[static_cast<std::size_t>(i)] = {"", "", upgradeImplemented(upgradeIndex)};
            upgradeTabRects[static_cast<std::size_t>(i)] = baseUpgradeItemRect(i);
        }
        const int selectedTab = updateVerticalTabClickSelection(
            baseUpgradeTabs_,
            ui,
            displaySelection,
            upgradeTabs,
            upgradeTabRects,
            upgradeDisplayCount);
        if (selectedTab >= 0) {
            baseUpgradeSelection_ = baseUpgradeIndexForDisplay(roguelikeTrainer, selectedTab);
            ui.block(upgradePanel);
            return;
        }
        if (ui.pressed(baseUpgradeConfirmRect())) {
            ui.emitSound(UiSoundEvent::Confirm);
            buyUpgrade(baseUpgradeSelection_);
            ui.block(upgradePanel);
            return;
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            ui.emitSound(UiSoundEvent::Confirm);
            buyUpgrade(baseUpgradeSelection_);
            ui.block(upgradePanel);
            return;
        }
        ui.block(upgradePanel);
        return;
    }

    if (baseMiningStartChoiceActive_) {
        const UiRect miningStartPanel = baseMiningStartPanelRect();
        const auto openRegenerateConfirm = [this]() {
            openUiConfirmDialog(
                baseRegenerateConfirm_,
                "再生成確認",
                "現在の坑道状態を破棄して、地形・敵・宝箱・ワープポイントを作り直します。\n拾っていないドロップがある場合は消えます。\nボス再戦用に地形も作り直します。",
                "再生成する",
                "戻る",
                1);
            baseStatus_.clear();
        };
        const auto openRoguelikeDepartureConfirm = [this]() {
            openUiConfirmDialog(
                baseRoguelikeDepartureConfirm_,
                "出発確認",
                "",
                "はい",
                "いいえ",
                1);
            baseStatus_.clear();
        };

        if (baseRoguelikeDepartureConfirm_.open) {
            const UiRect confirmPanel = baseRoguelikeDepartureConfirmRect();
            const UiConfirmDialogResult result = updateUiConfirmDialog(
                baseRoguelikeDepartureConfirm_,
                ui,
                input,
                confirmPanel);
            if (result == UiConfirmDialogResult::Confirmed) {
                requestMiningStartTransition(false, false);
                ui.block(confirmPanel);
                return;
            }
            if (result == UiConfirmDialogResult::Cancelled) {
                baseStatus_.clear();
                ui.block(confirmPanel);
                return;
            }
            ui.block(confirmPanel);
            return;
        }
        if (baseRegenerateConfirm_.open) {
            const UiRect confirmPanel = baseMiningRegenerateConfirmRect();
            const UiConfirmDialogResult result = updateUiConfirmDialog(baseRegenerateConfirm_, ui, input, confirmPanel);
            if (result == UiConfirmDialogResult::Confirmed) {
                requestMiningStartTransition(false, true);
                ui.block(confirmPanel);
                return;
            }
            if (result == UiConfirmDialogResult::Cancelled) {
                baseStatus_.clear();
                ui.block(confirmPanel);
                return;
            }
            ui.block(confirmPanel);
            return;
        }

        if (uiCancelRequested(baseCancelState_, input, ui, miningStartPanel)) {
            if (baseWarpPointSelectActive_) {
                baseWarpPointSelectActive_ = false;
            } else {
                baseMiningStartChoiceActive_ = false;
            }
            baseRegenerateConfirm_ = {};
            baseRoguelikeDepartureConfirm_ = {};
            baseStatus_.clear();
            return;
        }

        const std::vector<StageDefinition> selectableStages = selectableStageDefinitionsForCurrentUnlockState();
        const int selectableStageCount = static_cast<int>(selectableStages.size());
        const auto selectedStageIndex = [&]() {
            for (int i = 0; i < selectableStageCount; ++i) {
                if (selectableStages[static_cast<std::size_t>(i)].id == currentStageId_) {
                    return i;
                }
            }
            return 0;
        };
        const auto stageSelectorHitRect = [](UiRect rect) {
            constexpr float Padding = 12.0f;
            return UiRect{
                {rect.pos.x - Padding, rect.pos.y - Padding},
                {rect.size.x + Padding * 2.0f, rect.size.y + Padding * 2.0f},
            };
        };
        const auto changeSelectedStage = [&](int delta) {
            if (selectableStageCount <= 1) {
                return false;
            }
            const int currentIndex = selectedStageIndex();
            const int nextIndex = wrapStoragePageIndex(currentIndex, delta, selectableStageCount);
            if (nextIndex == currentIndex) {
                return false;
            }
            const StageDefinition& stage = selectableStages[static_cast<std::size_t>(nextIndex)];
            currentStageId_ = stage.id;
            currentStage_ = stageCatalogIndexForId(currentStageId_);
            resolveCurrentStageDefinition();
            syncWarpStateForCurrentStage();
            baseMiningStartSelection_ = stageLooksRoguelike(stage) ? 0 : (unlockedWarpPointCount_ > 0 ? 1 : 0);
            baseWarpPointSelectActive_ = false;
            baseWarpPointSelection_ = 0;
            baseRegenerateConfirm_ = {};
            baseRoguelikeDepartureConfirm_ = {};
            baseStatus_.clear();
            return true;
        };

        const std::vector<WarpPoint> selectableWarpPoints = selectableWarpPointsForCurrentStageStart();
        const bool selectedStageRoguelike = stageLooksRoguelike(currentStageDefinition());
        if (selectedStageRoguelike) {
            baseMiningStartSelection_ = 0;
            baseWarpPointSelectActive_ = false;
            baseWarpPointSelection_ = 0;
            baseRegenerateConfirm_ = {};
        }
        const auto startFromSelectedWarpPoint = [&]() {
            if (selectableWarpPoints.empty()) {
                baseStatus_ = "解放済みワープポイントがありません";
                return false;
            }
            baseWarpPointSelection_ = std::clamp(
                baseWarpPointSelection_,
                0,
                static_cast<int>(selectableWarpPoints.size()) - 1);
            requestedWarpPointStartPosition_ = selectableWarpPoints[static_cast<std::size_t>(baseWarpPointSelection_)].position;
            baseWarpPointSelectActive_ = false;
            baseRegenerateConfirm_ = {};
            baseRoguelikeDepartureConfirm_ = {};
            baseStatus_.clear();
            requestMiningStartTransition(true, false);
            return true;
        };

        if (baseWarpPointSelectActive_) {
            if (selectableWarpPoints.empty()) {
                baseWarpPointSelectActive_ = false;
                baseStatus_ = "解放済みワープポイントがありません";
                ui.block(baseMiningWarpPointSelectRect());
                return;
            }

            const int warpPointCount = static_cast<int>(selectableWarpPoints.size());
            baseWarpPointSelection_ = std::clamp(baseWarpPointSelection_, 0, warpPointCount - 1);
            const int warpDelta =
                (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp) ? -1 : 0) +
                (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown) ? 1 : 0) +
                input.activeRingDelta();
            if (warpDelta != 0) {
                baseWarpPointSelection_ = wrapStoragePageIndex(baseWarpPointSelection_, warpDelta, warpPointCount);
            }
            for (int i = 0; i < warpPointCount; ++i) {
                const UiRect rect = baseMiningWarpPointSelectChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseWarpPointSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseWarpPointSelection_ = i;
                    ui.emitSound(UiSoundEvent::Confirm);
                    if (startFromSelectedWarpPoint()) {
                        return;
                    }
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                ui.emitSound(UiSoundEvent::Confirm);
                if (startFromSelectedWarpPoint()) {
                    return;
                }
            }
            ui.block(baseMiningWarpPointSelectRect());
            return;
        }

        const UiPageSelectorRects stageSelector = baseMiningStageSelectorRects();
        const int pageDelta = input.activeRingDelta();
        if (pageDelta < 0 && changeSelectedStage(pageDelta)) {
            ui.emitSound(UiSoundEvent::TabSwitch);
            ui.block(miningStartPanel);
            return;
        }
        if (pageDelta > 0 && changeSelectedStage(pageDelta)) {
            ui.emitSound(UiSoundEvent::TabSwitch);
            ui.block(miningStartPanel);
            return;
        }
        if (input.pressed(InputAction::MoveLeft) || ui.pressed(stageSelectorHitRect(stageSelector.prev))) {
            if (changeSelectedStage(-1)) {
                ui.emitSound(UiSoundEvent::TabSwitch);
                ui.block(miningStartPanel);
                return;
            }
        }
        if (input.pressed(InputAction::MoveRight) || ui.pressed(stageSelectorHitRect(stageSelector.next))) {
            if (changeSelectedStage(1)) {
                ui.emitSound(UiSoundEvent::TabSwitch);
                ui.block(miningStartPanel);
                return;
            }
        }
        if (selectedStageRoguelike) {
            const UiRect startRect = baseMiningStartChoiceRect(0);
            if (startRect.contains(ui.mouse())) {
                baseMiningStartSelection_ = 0;
            }
            if (ui.pressed(startRect) || input.confirmPressed() || input.useItemPressed()) {
                ui.emitSound(UiSoundEvent::MenuOpen);
                openRoguelikeDepartureConfirm();
                ui.block(miningStartPanel);
                return;
            }
            ui.block(miningStartPanel);
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + BaseMiningStartChoiceCount - 1) % BaseMiningStartChoiceCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + 1) % BaseMiningStartChoiceCount;
        }
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            const UiRect rect = baseMiningStartChoiceRect(i);
            if (rect.contains(ui.mouse())) {
                baseMiningStartSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseMiningStartSelection_ = i;
                if (i == 1 && selectableWarpPoints.empty()) {
                    ui.emitSound(UiSoundEvent::Cancel);
                    baseStatus_ = "解放済みワープポイントがありません";
                    return;
                }
                if (i == 1) {
                    ui.emitSound(UiSoundEvent::MenuOpen);
                    baseWarpPointSelectActive_ = true;
                    baseWarpPointSelection_ = std::clamp(
                        baseWarpPointSelection_,
                        0,
                        static_cast<int>(selectableWarpPoints.size()) - 1);
                    baseStatus_.clear();
                    return;
                }
                if (i == 2) {
                    if (!canRegenerateCurrentStage()) {
                        ui.emitSound(UiSoundEvent::Cancel);
                        baseStatus_ = "全ワープ解放とクリア後に可能";
                        return;
                    }
                    ui.emitSound(UiSoundEvent::MenuOpen);
                    openRegenerateConfirm();
                    return;
                }
                ui.emitSound(UiSoundEvent::Confirm);
                baseRegenerateConfirm_ = {};
                baseRoguelikeDepartureConfirm_ = {};
                requestMiningStartTransition(false, false);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            if (baseMiningStartSelection_ == 1 && selectableWarpPoints.empty()) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "解放済みワープポイントがありません";
                return;
            }
            if (baseMiningStartSelection_ == 1) {
                ui.emitSound(UiSoundEvent::MenuOpen);
                baseWarpPointSelectActive_ = true;
                baseWarpPointSelection_ = std::clamp(
                    baseWarpPointSelection_,
                    0,
                    static_cast<int>(selectableWarpPoints.size()) - 1);
                baseStatus_.clear();
                return;
            }
            if (baseMiningStartSelection_ == 2) {
                if (!canRegenerateCurrentStage()) {
                    ui.emitSound(UiSoundEvent::Cancel);
                    baseStatus_ = "全ワープ解放とクリア後に可能";
                    return;
                }
                ui.emitSound(UiSoundEvent::MenuOpen);
                openRegenerateConfirm();
                return;
            }
            ui.emitSound(UiSoundEvent::Confirm);
            baseRegenerateConfirm_ = {};
            baseRoguelikeDepartureConfirm_ = {};
            requestMiningStartTransition(false, false);
            return;
        }
        ui.block(miningStartPanel);
        return;
    }

    if (baseMiningRescueDrop_.active) {
        updateBaseMiningRescueDropEvent(dt, ui);
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        return;
    }
    if (shouldStartBaseMiningRescueDropEvent()) {
        ui.emitSound(UiSoundEvent::MenuOpen);
        startBaseMiningRescueDropEvent();
        updateBaseMiningRescueDropEvent(dt, ui);
        ui.block({{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}});
        return;
    }

    if (input.pausePressed()) {
        ui.emitSound(UiSoundEvent::MenuOpen);
        mode_ = ScreenMode::PauseMenu;
        pauseReturnMode_ = ScreenMode::Base;
        pausePage_ = PauseMenuPage::Main;
        return;
    }

    const auto faceBaseCharacterSpriteTowardPlayer = [this](const BaseFacility& facility) {
        const BaseCharacterSpriteVisual* spriteVisual = baseCharacterSpriteVisual(baseArea_, facility.facilityId);
        if (spriteVisual == nullptr) {
            return;
        }
        const NpcCharacterVisual* visual = findNpcCharacterVisual(spriteVisual->visualId);
        if (visual == nullptr) {
            return;
        }

        const UiRect visualRect = baseCharacterSpriteVisualRect(facility);
        const Vec2 anchorPosition = visualRect.pos + Vec2{
            visualRect.size.x * visual->anchor.x,
            visualRect.size.y * visual->anchor.y,
        };
        const std::string facilityId(facility.facilityId);
        const auto it = baseNpcSpriteFlipHorizontal_.find(facilityId);
        const bool currentFlip = it != baseNpcSpriteFlipHorizontal_.end()
            ? it->second
            : visual->defaultFlipHorizontal;
        baseNpcSpriteFlipHorizontal_[facilityId] =
            characterSpriteFlipHorizontalFromFacing(basePlayerPosition_ - anchorPosition, currentFlip);
    };

    const auto startFacilityInteractionSequence = [this](const BaseFacility& facility, const std::function<void()>& openAction) {
        if (!openAction) {
            return;
        }
        if (const char* trigger = baseFacilityTutorialTrigger(facility.onInteract)) {
            if (const StoryEvent* event = findStoryEventForTrigger(trigger)) {
                std::function<void()> afterTutorial = openAction;
                if (startStoryEventWithCompletion(event->id, std::move(afterTutorial))) {
                    return;
                }
            }
        }
        if (startBaseTalkStoryEvent(facility.speakerId, openAction)) {
            return;
        }
        openAction();
    };

    const auto interact = [this, &startFacilityInteractionSequence, &faceBaseCharacterSpriteTowardPlayer](const BaseFacility& facility) {
        if (!facility.enabled) {
            return;
        }
        if (facility.verb == BaseInteractionVerb::Talk) {
            faceBaseCharacterSpriteTowardPlayer(facility);
        }
        std::function<void()> openAction;
        switch (facility.onInteract) {
        case BaseFacilityAction::MineExit:
            openAction = [this]() {
                if (hasBrokenRingItemForDeparture()) {
                    openUiConfirmDialog(
                        baseBrokenRingDepartureConfirm_,
                        "出発確認",
                        "壊れたアイテムがリングに乗っています。このまま出発しますか？",
                        "はい",
                        "いいえ",
                        1);
                    baseStatus_.clear();
                } else {
                    openBaseMiningStartChoice();
                }
            };
            break;
        case BaseFacilityAction::Storage:
            openAction = [this]() {
                baseStorageActive_ = true;
                baseStorageMode_ = StorageUiMode::ChooseAction;
                baseStorageActionSelection_ = 0;
                baseStorageBulkSelection_ = 0;
                baseStorageDepositSource_ = static_cast<int>(BaseItemSource::Backpack);
                baseStorageDepositSourceTabs_.focusedIndex = 0;
                baseStorageDepositSelection_ = 0;
                baseStorageWithdrawSelection_ = 0;
                baseStorageWarehousePage_ = 0;
                baseStorageQuantityDialog_ = {};
                baseStorageQuantityPending_ = {};
                closeUiCommandMenu(baseStorageCommandMenu_);
                baseStorageCommandOperation_ = StorageQuantityOperation::None;
                baseStorageCommandTarget_ = {};
                baseStoragePointerOperation_ = StorageQuantityOperation::None;
                baseStoragePointerTarget_ = {};
                baseStoragePointerPressMouse_ = {};
                baseStoragePointerPressCanOpenMenu_ = false;
                baseStoragePointerDragTriggered_ = false;
                baseStatus_.clear();
            };
            break;
        case BaseFacilityAction::Merchant:
            openAction = [this]() {
                if (merchantRefreshPending_) {
                    refreshMerchantStock(true);
                    merchantRefreshPending_ = false;
                } else {
                    refreshMerchantStock(false);
                }
                baseSellActive_ = true;
                baseMerchantMode_ = MerchantUiMode::ChooseAction;
                baseMerchantActionSelection_ = 0;
                baseMerchantSellSource_ = 0;
                baseMerchantSellSourceTabs_.focusedIndex = baseMerchantSellSource_;
                baseSellSelection_ = 0;
                baseMerchantBuySelection_ = 0;
                closeUiCommandMenu(baseMerchantSellCommandMenu_);
                baseMerchantSellCommandSource_ = 0;
                baseMerchantSellCommandIndex_ = -1;
                closeUiCommandMenu(baseMerchantBuyCommandMenu_);
                baseMerchantBuyCommandIndex_ = -1;
                baseStatus_.clear();
            };
            break;
        case BaseFacilityAction::Forge:
            openAction = [this]() {
                baseUpgradeActive_ = true;
                baseUpgradeSelection_ = 0;
                baseUpgradeTabs_.focusedIndex = baseUpgradeSelection_;
                baseStatus_.clear();
            };
            break;
        case BaseFacilityAction::Processing:
            openAction = [this]() {
                baseProcessingUiMode_ = ProcessingUiMode::ChooseAction;
                baseProcessingActionSelection_ = 0;
                baseProcessingMode_ = static_cast<int>(ProcessingMode::Attack);
                baseProcessingTabs_.focusedIndex = baseProcessingMode_;
                baseProcessingSource_ = 0;
                baseProcessingSourceTabs_.focusedIndex = baseProcessingSource_;
                baseProcessingSelection_ = 0;
                closeUiCommandMenu(baseProcessingCommandMenu_);
                baseProcessingCommandSlot_ = -1;
                baseProcessingConfirm_ = {};
                baseProcessingConfirmTarget_ = {};
                baseStatus_.clear();
            };
            break;
        case BaseFacilityAction::Bookshelf:
            openAction = [this]() {
                openBookshelf();
            };
            break;
        case BaseFacilityAction::Diary:
            openAction = [this]() {
                openBaseDiary();
            };
            break;
        case BaseFacilityAction::RingWorkshop:
            openAction = [this, unlocked = facility.unlocked]() {
                if (unlocked) {
                    openRingWorkshop();
                } else {
                    baseStatus_ = "リング工房: まだ解禁されていません";
                }
            };
            break;
        case BaseFacilityAction::HomeEntrance:
            baseOutdoorPlayerPosition_ = basePlayerPosition_;
            {
                const UiRect fallback = defaultBaseFacilityRect(BaseArea::HomeInterior, ringWorkshopUnlocked_, "home_exit");
                const UiRect homeExitRect = toUiRect(baseFacilityRectFor(BaseArea::HomeInterior, "home_exit", toBaseEditRect(fallback)));
                requestBaseAreaCrossfade(
                    BaseArea::HomeInterior,
                    homeInteriorEntryPosition(homeExitRect, balance_.playerRadius),
                    {0.0f, -1.0f},
                    "ルネの家に入りました");
            }
            break;
        case BaseFacilityAction::HomeExit:
            {
                const UiRect fallback = defaultBaseFacilityRect(BaseArea::Outdoor, ringWorkshopUnlocked_, "home_entrance");
                const UiRect homeEntranceRect = toUiRect(baseFacilityRectFor(BaseArea::Outdoor, "home_entrance", toBaseEditRect(fallback)));
                const Vec2 outdoorPosition = baseFacilitySpawnPosition(
                    homeEntranceRect,
                    BaseFacilitySpawnSide::Below,
                    balance_.playerRadius);
                baseOutdoorPlayerPosition_ = outdoorPosition;
                requestBaseAreaCrossfade(
                    BaseArea::Outdoor,
                    outdoorPosition,
                    {0.0f, 1.0f},
                    "魔女の拠点に戻りました");
            }
            break;
        case BaseFacilityAction::MonicaTalk:
            startBaseMonicaDialogue();
            break;
        case BaseFacilityAction::ElderTalk:
            startBaseElderDialogue();
            break;
        }
        startFacilityInteractionSequence(facility, openAction);
    };

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    applyHiddenRouteFacilityAvailability(
        facilities,
        hasStoryFlag(HiddenEndingPeopleGoneFlag),
        hasStoryFlag(StoryHiddenOrbitCorruptionUnlockedFlag),
        hiddenBaseNpcRemoved("merchant_npc"),
        hiddenBaseNpcRemoved("processor_npc"),
        hiddenBaseNpcRemoved("elder"));
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    applyBaseStoryFacilityOffsets(facilities, baseStoryFacilityOffsets_);
    const float playerRadius = balance_.playerRadius;
    const auto baseCollision = [&](Vec2 position) {
        const UiRect bounds = baseMapBounds();
        if (position.x - playerRadius < bounds.pos.x ||
            position.y - playerRadius < bounds.pos.y ||
            position.x + playerRadius > bounds.pos.x + bounds.size.x ||
            position.y + playerRadius > bounds.pos.y + bounds.size.y) {
            return true;
        }
        const Vec2 passabilityProbe = playerSpriteFootAnchor(position);
        if (baseArea_ == BaseArea::HomeInterior) {
            const UiRect homeWalk = homeInteriorWalkBounds();
            if (passabilityProbe.x - playerRadius < homeWalk.pos.x ||
                passabilityProbe.y - playerRadius < homeWalk.pos.y ||
                passabilityProbe.x + playerRadius > homeWalk.pos.x + homeWalk.size.x ||
                passabilityProbe.y + playerRadius > homeWalk.pos.y + homeWalk.size.y) {
                return true;
            }
        }
        const int minTileX = static_cast<int>(std::floor((passabilityProbe.x - playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int maxTileX = static_cast<int>(std::floor((passabilityProbe.x + playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int minTileY = static_cast<int>(std::floor((passabilityProbe.y - playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int maxTileY = static_cast<int>(std::floor((passabilityProbe.y + playerRadius) / static_cast<float>(BaseEditGridSize)));
        for (int tileY = minTileY; tileY <= maxTileY; ++tileY) {
            for (int tileX = minTileX; tileX <= maxTileX; ++tileX) {
                if (!isBasePassabilityBlocked(baseArea_, tileX, tileY)) {
                    continue;
                }
                const UiRect tileRect{
                    {static_cast<float>(tileX * BaseEditGridSize), static_cast<float>(tileY * BaseEditGridSize)},
                    {static_cast<float>(BaseEditGridSize), static_cast<float>(BaseEditGridSize)},
                };
                if (circleIntersectsRect(passabilityProbe, playerRadius, tileRect)) {
                    return true;
                }
            }
        }
        return false;
    };

    const Vec2 previousBasePlayerPosition = basePlayerPosition_;
    const Vec2 moveAxis = input.moveAxis();
    const bool walkingNow = lengthSquared(moveAxis) > 0.0001f;
    updateBasePlayerSpriteAnimation(dt, walkingNow);
    const PlayerFootstepSurface baseFootstepSurface = baseArea_ == BaseArea::HomeInterior
        ? PlayerFootstepSurface::HomeInterior
        : PlayerFootstepSurface::BaseOutdoor;
    maybeTriggerPlayerFootstep(
        playerSpriteFootAnchor(basePlayerPosition_),
        lengthSquared(moveAxis) > 0.0001f ? moveAxis : basePlayerFacing_,
        basePlayerSpriteWalking_,
        characterSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
        previousBasePlayerDustFrame_,
        baseFootstepSurface);
    if (lengthSquared(moveAxis) > 0.0001f) {
        basePlayerFacing_ = normalize(moveAxis);
        updateBasePlayerSpriteFlipFromFacing();
        const Vec2 delta = moveAxis * balance_.playerSpeed * dt;
        Vec2 next = basePlayerPosition_ + Vec2{delta.x, 0.0f};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
        next = basePlayerPosition_ + Vec2{0.0f, delta.y};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
    }

    const Vec2 previousBasePlayerFoot = playerSpriteFootAnchor(previousBasePlayerPosition);
    const Vec2 currentBasePlayerFoot = playerSpriteFootAnchor(basePlayerPosition_);
    const auto editedFacilityRect = [this](BaseArea area, std::string_view facilityId) {
        const UiRect fallback = defaultBaseFacilityRect(area, ringWorkshopUnlocked_, facilityId);
        return toUiRect(baseFacilityRectFor(area, facilityId, toBaseEditRect(fallback)));
    };
    if (baseArea_ == BaseArea::Outdoor) {
        if (const BaseFacility* entrance = findBaseFacilityById(facilities, "home_entrance")) {
            if (pointEnteredRectFromBelow(previousBasePlayerFoot, currentBasePlayerFoot, entrance->rect)) {
                const UiRect homeExitRect = editedFacilityRect(BaseArea::HomeInterior, "home_exit");
                baseOutdoorPlayerPosition_ = basePlayerPosition_;
                requestBaseAreaCrossfade(
                    BaseArea::HomeInterior,
                    homeInteriorEntryPosition(homeExitRect, balance_.playerRadius),
                    {0.0f, -1.0f},
                    "ルネの家に入りました");
                return;
            }
        }
    } else if (baseArea_ == BaseArea::HomeInterior) {
        if (const BaseFacility* exit = findBaseFacilityById(facilities, "home_exit")) {
            if (pointEnteredRectFromAbove(previousBasePlayerFoot, currentBasePlayerFoot, exit->rect)) {
                const UiRect homeEntranceRect = editedFacilityRect(BaseArea::Outdoor, "home_entrance");
                const Vec2 outdoorPosition = baseFacilitySpawnPosition(
                    homeEntranceRect,
                    BaseFacilitySpawnSide::Below,
                    balance_.playerRadius);
                baseOutdoorPlayerPosition_ = outdoorPosition;
                requestBaseAreaCrossfade(
                    BaseArea::Outdoor,
                    outdoorPosition,
                    {0.0f, 1.0f},
                    "魔女の拠点に戻りました");
                return;
            }
        }
    }

    const bool hiddenBaseRingInteractionsEnabled =
        !dialogue_.active() &&
        !pendingStoryTriggerDelayActive() &&
        pendingStoryTrigger_.empty() &&
        pendingStoryTriggers_.empty() &&
        !firstItemAcquisitionNoticeActive() &&
        !screenTransition_.active();
    updateHiddenBaseOrbit(input, ui, dt, hiddenBaseRingInteractionsEnabled);

    const BaseFacility* interactionFacility = selectBaseInteractionFacility(basePlayerPosition_, basePlayerFacing_, baseArea_, facilities);

    if (input.mouseLeftPressed() && !ui.pointerConsumed()) {
        const SDL_Keymod mods = SDL_GetModState();
        const bool testClickAnywhere = (mods & SDL_KMOD_CTRL) != 0;
        for (const BaseFacility& facility : facilities) {
            if (baseFacilityHiddenInNormalView(baseArea_, facility)) {
                continue;
            }
            if (!baseFacilityPointerRect(facility, baseArea_, ringWorkshopUnlocked_).contains(ui.mouse())) {
                continue;
            }
            ui.consumePointer();
            if (baseInteractionGroupAvailable(basePlayerPosition_, baseArea_, facilities, facility) ||
                (testClickAnywhere && facility.enabled)) {
                ui.emitSound(facility.onInteract == BaseFacilityAction::Bookshelf ? UiSoundEvent::BookOpen : UiSoundEvent::Confirm);
                interact(facility);
            } else if (facility.enabled) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "近くまで移動してください";
            }
            return;
        }
    }

    if (input.confirmPressed() && interactionFacility != nullptr) {
        ui.emitSound(interactionFacility->onInteract == BaseFacilityAction::Bookshelf ? UiSoundEvent::BookOpen : UiSoundEvent::Confirm);
        interact(*interactionFacility);
        return;
    }
}

void Game::renderBookshelfScreen(Renderer& renderer) const
{
    char buffer[256];
    std::vector<const ObjectDefinition*> itemCodexObjects;
    itemCodexObjects.reserve(objectCatalog_.objects.size());
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        if (!isCodexHiddenObject(object)) {
            itemCodexObjects.push_back(&object);
        }
    }
    const std::vector<const EnemyDefinition*> enemyCodexObjects = enemyCodexEnemies(enemyCatalog_);
    const std::vector<EndingKind> replayChoices = bookshelfEndingReplayChoices();
    const int menuItemCount = BookshelfMenuItemCount + (replayChoices.empty() ? 0 : 1);

    const auto objectAt = [&itemCodexObjects](int targetIndex) -> const ObjectDefinition* {
        if (targetIndex < 0 || targetIndex >= static_cast<int>(itemCodexObjects.size())) {
            return nullptr;
        }
        return itemCodexObjects[static_cast<std::size_t>(targetIndex)];
    };

    if (bookshelfPage_ == BookshelfPage::Menu) {
        const UiRect panel = bookshelfMenuPanelRect(menuItemCount);
        renderer.drawText(smallActionInfoTextPos(panel), "何を見ますか？", {198, 198, 206, 255}, 2);
        for (int i = 0; i < menuItemCount; ++i) {
            drawUiButton(renderer, bookshelfMenuChoiceRect(panel, i), bookshelfMenuLabel(i), i == bookshelfSelection_, uiActionButtonStyle());
        }
        if (replayChoices.size() >= 2) {
            const std::vector<UiCommandMenuItem> commandItems = bookshelfEndingReplayCommandItems(replayChoices);
            drawUiCommandMenu(
                renderer,
                bookshelfEndingCommandMenu_,
                commandItems.data(),
                static_cast<int>(commandItems.size()));
        }
        return;
    }

    const UiRect panel = merchantPanelRect();
    const UiRect detailPanel = merchantDetailPanelRect();
    const InventoryUiGridStyle gridStyle = bookshelfGridStyle();
    const UiRect gridViewport = bookshelfGridViewport();
    const int totalCount = bookshelfPage_ == BookshelfPage::Enemies
        ? static_cast<int>(enemyCodexObjects.size())
        : static_cast<int>(itemCodexObjects.size());
    const UiScrollAreaLayout gridLayout =
        makeInventoryUiGridLayout(gridViewport, totalCount, bookshelfScrollOffset_, gridStyle);
    int discoveredCount = 0;
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        for (const EnemyDefinition* enemy : enemyCodexObjects) {
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy->id);
            if (stage != EncyclopediaStage::Undiscovered) {
                ++discoveredCount;
            }
        }
    } else {
        for (const ObjectDefinition* object : itemCodexObjects) {
            const bool treasure = object->category == "\xE5\xAE\x9D";
            const EncyclopediaStage stage = encyclopedia_.objectStage(object->id, treasure);
            if (stage != EncyclopediaStage::Undiscovered) {
                ++discoveredCount;
            }
        }
    }

    std::snprintf(buffer, sizeof(buffer), "%d/%d 記録", discoveredCount, totalCount);
    renderer.drawText(panel.pos + Vec2{28.0f, 62.0f}, buffer, {150, 150, 160, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "完成度 %d%%", codexCompletionPercent(discoveredCount, totalCount));
    const Vec2 completionTextSize = renderer.measureText(buffer, 2);
    renderer.drawText(
        panel.pos + Vec2{panel.size.x - 28.0f - completionTextSize.x, 62.0f},
        buffer,
        {255, 230, 150, 255},
        2);
    if (totalCount <= 0) {
        renderer.drawText(panel.pos + Vec2{28.0f, 154.0f}, "記録対象がありません", {150, 150, 160, 255}, 2);
    } else if (bookshelfPage_ == BookshelfPage::Items) {
        std::vector<InventoryUiEntryView> entries;
        entries.reserve(itemCodexObjects.size());
        for (const ObjectDefinition* object : itemCodexObjects) {
            InventoryUiEntryView entry{};
            const bool treasure = object->category == "\xE5\xAE\x9D";
            if (encyclopedia_.objectStage(object->id, treasure) != EncyclopediaStage::Undiscovered) {
                entry.item = object;
                entry.stackCount = 1;
            }
            entries.push_back(entry);
        }
        drawInventoryUiGrid(renderer, gridLayout, entries, bookshelfSelection_, gridStyle);
        renderer.pushClipRect(gridLayout.viewport.pos, gridLayout.viewport.size);
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            if (entries[static_cast<std::size_t>(i)].item != nullptr) {
                continue;
            }
            const UiRect rect = inventoryUiGridSlotRect(gridLayout, i, gridStyle);
            if (!uiScrollAreaRectVisible(gridLayout, rect)) {
                continue;
            }
            const std::string_view unknown = "?";
            const Vec2 textSize = renderer.measureText(unknown, 3);
            renderer.drawOutlinedText(
                rect.pos + (rect.size - textSize) * 0.5f,
                unknown,
                ui::TextMuted,
                {0, 0, 0, 160},
                4,
                3);
        }
        renderer.popClipRect();
    } else {
        renderer.pushClipRect(gridLayout.viewport.pos, gridLayout.viewport.size);
        for (int i = 0; i < static_cast<int>(enemyCodexObjects.size()); ++i) {
            const UiRect rect = inventoryUiGridSlotRect(gridLayout, i, gridStyle);
            if (!uiScrollAreaRectVisible(gridLayout, rect)) {
                continue;
            }
            const EnemyDefinition& enemy = *enemyCodexObjects[static_cast<std::size_t>(i)];
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            InventoryUiEntryView emptyEntry{};
            drawInventoryUiSlot(renderer, rect, emptyEntry, InventoryUiSlotStyle{i == bookshelfSelection_, stage == EncyclopediaStage::Undiscovered, gridStyle.imageMaxSize});
            if (stage == EncyclopediaStage::Undiscovered) {
                const std::string_view unknown = "?";
                const Vec2 textSize = renderer.measureText(unknown, 3);
                renderer.drawOutlinedText(
                    rect.pos + (rect.size - textSize) * 0.5f,
                    unknown,
                    ui::TextMuted,
                    {0, 0, 0, 160},
                    4,
                    3);
                continue;
            }
            EnemyImageDrawOptions iconOptions;
            iconOptions.allowUpscale = true;
            iconOptions.directionOverrideEnabled = true;
            iconOptions.directionOverride = {0.0f, 1.0f};
            (void)drawEnemyImageIcon(
                renderer,
                enemy.imageNumber,
                rect.pos + rect.size * 0.5f,
                {gridStyle.imageMaxSize, gridStyle.imageMaxSize},
                baseRingPreviewAnimationTime_,
                iconOptions);
        }
        renderer.popClipRect();
        drawUiScrollAreaScrollbar(renderer, gridLayout, gridStyle.scroll);
    }

    if (bookshelfPage_ == BookshelfPage::Enemies) {
        if (bookshelfSelection_ >= 0 && bookshelfSelection_ < static_cast<int>(enemyCodexObjects.size())) {
            const EnemyDefinition& enemy = *enemyCodexObjects[static_cast<std::size_t>(bookshelfSelection_)];
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            drawUiSubPanel(renderer, detailPanel);
            if (stage == EncyclopediaStage::Undiscovered) {
                float detailY = drawUiDetailHeader(renderer, detailPanel, "未発見");
                drawUiDetailText(renderer, detailPanel, detailY, "まだ記録されていません。ダンジョンで遭遇するとモンスター図鑑に登録されます。");
            } else {
                const std::string name = enemy.name.empty() ? enemy.id : enemy.name;
                float detailY = drawUiDetailHeader(renderer, detailPanel, name);
                EnemyImageDrawOptions imageOptions;
                imageOptions.allowUpscale = true;
                imageOptions.directionOverrideEnabled = true;
                imageOptions.directionOverride = {0.0f, 1.0f};
                const Vec2 imageMax{112.0f, 112.0f};
                const Vec2 imageCenter{
                    detailPanel.pos.x + detailPanel.size.x * 0.5f,
                    detailY + imageMax.y * 0.5f,
                };
                if (drawEnemyImageIcon(renderer, enemy.imageNumber, imageCenter, imageMax, baseRingPreviewAnimationTime_, imageOptions)) {
                    detailY += imageMax.y + 12.0f;
                }
                if (stage != EncyclopediaStage::Complete) {
                    drawUiDetailText(renderer, detailPanel, detailY, "？？？");
                    drawUiDetailLine(renderer, detailPanel, detailY, "HP", "？？？");
                    drawUiDetailLine(renderer, detailPanel, detailY, "攻撃力", "？？？");
                    drawUiDetailLine(renderer, detailPanel, detailY, "移動速度", "？？？");
                    drawUiDetailText(renderer, detailPanel, detailY, "虫眼鏡で観察すると詳細が記録されます。");
                } else {
                    drawUiDetailText(renderer, detailPanel, detailY, enemy.description.empty() ? "-" : enemy.description);
                    drawUiDetailLine(renderer, detailPanel, detailY, "HP", std::to_string(enemy.hp));
                    drawUiDetailLine(renderer, detailPanel, detailY, "攻撃力", enemyContactAttackText(enemy));
                    drawUiDetailLine(renderer, detailPanel, detailY, "移動速度", enemyMoveSpeedLabel(enemy.moveSpeed));
                    std::string reward = "EXP ";
                    reward += std::to_string(enemy.xp);
                    reward += " / ";
                    reward += std::to_string(enemy.money);
                    reward += "G";
                    drawUiDetailLine(renderer, detailPanel, detailY, "報酬", reward);
                    if (enemy.captureDifficulty > 0) {
                        drawUiDetailLine(renderer, detailPanel, detailY, "捕獲難度", enemyCaptureDifficultyLabel(enemy.captureDifficulty));
                    }
                    if (!enemy.capturedEffectText.empty()) {
                        drawUiDetailText(renderer, detailPanel, detailY, "捕獲時効果");
                        drawUiDetailText(renderer, detailPanel, detailY, enemy.capturedEffectText);
                    }
                }
            }
        } else {
            drawUiSubPanel(renderer, detailPanel);
            float detailY = drawUiDetailHeader(renderer, detailPanel, "敵未選択");
            drawUiDetailText(renderer, detailPanel, detailY, "敵を選択してください。");
        }
    } else if (const ObjectDefinition* object = objectAt(bookshelfSelection_)) {
        const bool treasure = object->category == "\xE5\xAE\x9D";
        const EncyclopediaStage stage = encyclopedia_.objectStage(object->id, treasure);
        const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object->name.empty() ? object->id : object->name);
        if (stage != EncyclopediaStage::Undiscovered) {
            InventoryUiEntryView detailEntry{};
            detailEntry.item = object;
            detailEntry.stackCount = 1;
            std::vector<InventoryUiDetailExtraLine> extraLines;
            if (isSellableObject(*object)) {
                extraLines.push_back({"売値", std::to_string(sellPrice(*object)) + "G"});
            } else {
                extraLines.push_back({"売値", "売却不可", ui::TextDisabled});
            }
            drawInventoryUiDetailPanel(
                renderer,
                detailPanel,
                detailEntry,
                objectCatalog_,
                encyclopedia_,
                InventoryUiDetailOptions{.animationSeconds = baseRingPreviewAnimationTime_, .showExtraLineSeparator = false},
                extraLines);
        } else {
            drawUiSubPanel(renderer, detailPanel);
            const Vec2 bookshelfDetailContent = uiSubPanelContentPos(detailPanel);
            std::snprintf(buffer, sizeof(buffer), "%s / %s", name.c_str(), encyclopediaStageName(stage));
            renderer.drawText(bookshelfDetailContent, buffer, {255, 230, 150, 255}, 2);
            float detailY = bookshelfDetailContent.y + 36.0f;
            drawUiDetailText(renderer, detailPanel, detailY, "まだ記録されていません。入手するとアイテム図鑑に登録されます。");
        }
    } else {
        drawUiSubPanel(renderer, detailPanel);
        float detailY = drawUiDetailHeader(renderer, detailPanel, "アイテム未選択");
        drawUiDetailText(renderer, detailPanel, detailY, "アイテムを選択してください。");
    }
}

void Game::renderBaseDiaryScreen(Renderer& renderer, UiRect panel) const
{
    const UiRect body = uiBodyRect(panel);
    const DiarySaveSummary& summary = baseDiarySummary_;

    const UiRect recordPanel{
        body.pos + Vec2{12.0f, -26.0f},
        {body.size.x - 24.0f, 280.0f},
    };
    drawUiSubPanel(renderer, recordPanel);
    const UiRect recordContent = uiSubPanelContentRect(recordPanel);

    float y = recordContent.pos.y + 6.0f;
    constexpr float ValueXOffset = 142.0f;
    constexpr float RowHeight = 44.0f;
    const float labelX = recordContent.pos.x;
    const float valueX = recordContent.pos.x + ValueXOffset;
    const float rightX = recordContent.pos.x + recordContent.size.x;

    const auto drawTextRow = [&](std::string_view label, std::string_view value, Color valueColor = ui::Text) {
        renderer.drawText({labelX, y}, label, ui::TextMuted, 2);
        renderer.drawText({valueX, y}, value, valueColor, 2);
        y += RowHeight;
    };

    if (!summary.hasSave && baseDiaryMode_ != BaseDiaryMode::Saved) {
        renderer.drawText({labelX, y}, "記録はありません", ui::TextMuted, 2);
        y += RowHeight;
    } else {
        renderer.drawText({labelX, y}, "進行", ui::TextMuted, 2);
        if (summary.storyCleared) {
            renderer.drawText({valueX, y}, "ストーリークリア", Color{255, 230, 150, 255}, 2);
        } else {
            const std::string stageName = fittedSingleLineText(renderer, summary.latestStageName, 190.0f, 2);
            renderer.drawText({valueX, y}, stageName, ui::Text, 2);

            InlineItemTextStyle warpStyle;
            warpStyle.text = ui::Text;
            warpStyle.scale = 2;
            warpStyle.iconTextGap = 6.0f;
            warpStyle.iconScale = 24.0f / std::max(1.0f, renderer.measureText("0", warpStyle.scale).y);
            const std::string warpText =
                inlineWorldIconTag(worldIconKey(WorldIconId::WarpPoint)) +
                std::to_string(std::max(0, summary.discoveredWarpPoints)) +
                "/" +
                std::to_string(std::max(1, summary.totalWarpPoints));
            drawInlineItemTextRightAligned(renderer, objectCatalog_, {rightX, y}, warpText, warpStyle);
        }
        y += RowHeight;
        drawTextRow("ルネ", "Lv." + std::to_string(std::max(1, summary.playerLevel)));
        drawTextRow("アイテム図鑑", std::to_string(std::clamp(summary.itemCodexPercent, 0, 100)) + "%");
        drawTextRow("モンスター図鑑", std::to_string(std::clamp(summary.enemyCodexPercent, 0, 100)) + "%");
        drawTextRow("プレイ時間", formatDiaryPlayTime(summary.playTimeSeconds));
    }

    const Vec2 messagePos{recordPanel.pos.x + 22.0f, recordPanel.pos.y + recordPanel.size.y + 24.0f};
    if (baseDiaryMode_ == BaseDiaryMode::Confirm) {
        renderer.drawText(messagePos, "保存しますか？", ui::Text, 2);
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 1), "戻る", baseDiarySelection_ == 1, uiCancelButtonStyle());
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 0), "保存する", baseDiarySelection_ == 0, uiActionButtonStyle());
    } else if (baseDiaryMode_ == BaseDiaryMode::Error) {
        const std::string message = baseDiaryMessage_.empty() ? std::string("もう一度試すか、戻ってください。") : baseDiaryMessage_;
        renderer.drawText(messagePos, message, Color{255, 190, 190, 255}, 2);
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 1), "戻る", baseDiarySelection_ == 1, uiCancelButtonStyle());
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 0), "再試行", baseDiarySelection_ == 0, uiActionButtonStyle());
    } else {
        renderer.drawText(messagePos, "保存しました。", Color{202, 255, 216, 255}, 2);
        drawUiButton(renderer, uiResultDialogOkButtonRect(panel), "閉じる", true, uiActionButtonStyle());
    }
}

void Game::renderBaseMiningRescueDropEvent(Renderer& renderer) const
{
    if (!baseMiningRescueDrop_.active) {
        return;
    }

    for (const BaseMiningRescueDropItem& item : baseMiningRescueDrop_.items) {
        const float rawProgress =
            (baseMiningRescueDrop_.elapsedSeconds - item.delaySeconds) / BaseMiningRescueDropDurationSeconds;
        const float progress = clamp(rawProgress, 0.0f, 1.0f);
        const float eased = smooth01(progress);
        const Vec2 center = lerp(item.startPosition, item.targetPosition, eased) +
            Vec2{std::sin(eased * Pi * 2.0f) * 8.0f, -std::sin(eased * Pi) * 22.0f};

        const float shadowAlpha = 34.0f + eased * 88.0f;
        renderer.fillCircle(
            item.targetPosition + Vec2{0.0f, 14.0f},
            8.0f + eased * 16.0f,
            {0, 0, 0, static_cast<unsigned char>(std::clamp(shadowAlpha, 0.0f, 130.0f))});

        ObjectImageDrawOptions imageOptions;
        imageOptions.allowUpscale = true;
        imageOptions.rotationDegrees = (item.objectId == RescueTorchObjectId ? -9.0f : 12.0f) +
            std::sin(baseMiningRescueDrop_.elapsedSeconds * 10.0f + item.delaySeconds * 18.0f) * 4.0f;
        const ItemData* object = objectCatalog_.registry.findById(item.objectId);
        if (object == nullptr || !drawItemImage(renderer, *object, center, {58.0f, 58.0f}, imageOptions)) {
            const Color core = item.objectId == RescueTorchObjectId
                ? Color{255, 176, 64, 255}
                : Color{178, 184, 190, 255};
            renderer.fillCircle(center, 17.0f, core);
            renderer.drawCircle(center, 21.0f, {255, 246, 190, 220});
        }

        const float glint = 0.5f + 0.5f * std::sin(baseMiningRescueDrop_.elapsedSeconds * 16.0f + item.delaySeconds * 31.0f);
        renderer.fillCircle(
            center + Vec2{-18.0f, -24.0f},
            2.0f + glint * 2.2f,
            {255, 248, 188, static_cast<unsigned char>(150.0f + glint * 80.0f)});
        renderer.fillCircle(
            center + Vec2{20.0f, -12.0f},
            1.5f + (1.0f - glint) * 2.0f,
            {184, 232, 255, static_cast<unsigned char>(130.0f + (1.0f - glint) * 70.0f)});
    }
}

void Game::updateBasePlayerSpriteAnimation(float dt, bool walking)
{
    updateCharacterSpriteAnimation(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_, dt, walking);
}

void Game::updateBaseActorIdleAnimation(float dt)
{
    baseActorIdleAnimationTime_ = std::fmod(
        baseActorIdleAnimationTime_ + std::max(0.0f, dt),
        3600.0f);
}

void Game::updateBasePlayerSpriteFlipFromFacing()
{
    basePlayerSpriteFlipHorizontal_ =
        characterSpriteFlipHorizontalFromFacing(basePlayerFacing_, basePlayerSpriteFlipHorizontal_);
}

namespace {

struct BaseActorRenderContext {
    BaseArea area = BaseArea::Outdoor;
    bool ringWorkshopUnlocked = false;
    Vec2 playerPosition{};
    Vec2 playerFacing{};
    float playerRadius = 0.0f;
    float actorIdleAnimationTime = 0.0f;
    float playerSpriteAnimationTime = 0.0f;
    bool playerSpriteWalking = false;
    bool playerSpriteFlipHorizontal = false;
    bool showInteractionHints = true;
    const InventorySystem* inventory = nullptr;
    const std::unordered_map<std::string, bool>* npcSpriteFlipHorizontal = nullptr;
};

bool baseNpcSpriteFlipHorizontal(
    std::string_view facilityId,
    bool fallback,
    const std::unordered_map<std::string, bool>* npcSpriteFlipHorizontal)
{
    if (npcSpriteFlipHorizontal == nullptr) {
        return fallback;
    }
    const auto it = npcSpriteFlipHorizontal->find(std::string(facilityId));
    return it != npcSpriteFlipHorizontal->end() ? it->second : fallback;
}

void drawBaseActors(
    Renderer& renderer,
    const std::vector<BaseFacility>& facilities,
    Vec2 mouse,
    const BaseActorRenderContext& context,
    const std::function<void()>& drawPlayerFootstepDust)
{
    struct ActorDrawEntry {
        float depthY = 0.0f;
        std::function<void()> draw;
    };

    std::vector<ActorDrawEntry> drawEntries;
    drawEntries.reserve(facilities.size() + 1);

    const int baseNpcFrame = characterSpriteIdleFrameIndex(context.actorIdleAnimationTime);
    for (const BaseFacility& facility : facilities) {
        if (!facility.enabled || baseFacilityHiddenInNormalView(context.area, facility)) {
            continue;
        }

        const BaseCharacterSpriteVisual* spriteVisual = baseCharacterSpriteVisual(context.area, facility.facilityId);
        if (spriteVisual == nullptr) {
            continue;
        }

        const UiRect visualRect = baseCharacterSpriteVisualRect(facility);
        const bool inInteractionRange = context.showInteractionHints &&
            baseInteractionGroupAvailable(context.playerPosition, context.area, facilities, facility);
        const bool hovered = context.showInteractionHints && inInteractionRange && facility.enabled && visualRect.contains(mouse);
        const NpcCharacterVisual* visual = findNpcCharacterVisual(spriteVisual->visualId);
        if (visual == nullptr) {
            drawEntries.push_back(ActorDrawEntry{
                facility.rect.pos.y + facility.rect.size.y,
                [&, facilityPtr = &facility, inInteractionRange, hovered, showInteractionHints = context.showInteractionHints]() {
                    drawBaseFacilityFallbackRect(renderer, *facilityPtr, inInteractionRange, hovered, showInteractionHints);
                },
            });
            continue;
        }

        const float drawScale = npcCharacterScaleToFit(renderer, *visual, visualRect.size);
        Vec2 drawSize = npcCharacterDrawSize(renderer, *visual, drawScale);
        if (drawSize.x <= 0.0f || drawSize.y <= 0.0f) {
            drawSize = visualRect.size;
        }
        const Vec2 anchorPosition = visualRect.pos + Vec2{
            visualRect.size.x * visual->anchor.x,
            visualRect.size.y * visual->anchor.y,
        };
        renderer.drawActorShadow(anchorPosition, std::max(drawSize.x, drawSize.y));
        const bool flipHorizontal = baseNpcSpriteFlipHorizontal(
            facility.facilityId,
            visual->defaultFlipHorizontal,
            context.npcSpriteFlipHorizontal);
        drawEntries.push_back(ActorDrawEntry{
            anchorPosition.y,
            [&, visual, facilityPtr = &facility, anchorPosition, drawScale, inInteractionRange, hovered, baseNpcFrame, flipHorizontal]() {
                NpcCharacterDrawOptions options;
                options.frameIndex = baseNpcFrame;
                options.anchorPosition = anchorPosition;
                options.scale = drawScale;
                options.flipHorizontal = flipHorizontal;
                options.outlineEnabled = context.showInteractionHints && inInteractionRange && facilityPtr->enabled;
                options.outlineColor = hovered ? Color{255, 230, 72, 255} : Color{255, 255, 255, 245};
                options.outlinePx = 1;

                if (!drawNpcCharacterSprite(renderer, *visual, options)) {
                    drawBaseFacilityFallbackRect(
                        renderer,
                        *facilityPtr,
                        inInteractionRange,
                        hovered,
                        context.showInteractionHints);
                }
            },
        });
    }

    const Vec2 basePlayerFootAnchor = playerSpriteFootAnchor(context.playerPosition);
    const float basePlayerSpriteVisualSize = playerSpriteNaturalVisualSize(renderer);
    renderer.drawActorShadow(basePlayerFootAnchor, basePlayerSpriteVisualSize);
    if (drawPlayerFootstepDust) {
        drawPlayerFootstepDust();
    }
    drawEntries.push_back(ActorDrawEntry{
        basePlayerFootAnchor.y,
        [&]() {
            if (renderer.hasPlayerSheet()) {
                const int playerFrame = characterSpriteFrameIndex(
                    context.playerSpriteAnimationTime,
                    context.playerSpriteWalking);
                renderer.drawPlayerSpriteNaturalSize(
                    playerFrame,
                    basePlayerFootAnchor,
                    1.0f,
                    context.playerSpriteFlipHorizontal,
                    {255, 255, 255, 255},
                    {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
                if (context.inventory != nullptr) {
                    drawEquippedStaffOnPlayer(
                        renderer,
                        *context.inventory,
                        basePlayerFootAnchor,
                        playerFrame,
                        context.playerSpriteFlipHorizontal);
                }
            } else {
                renderer.fillCircle(context.playerPosition, context.playerRadius, {118, 72, 168, 255});
                renderer.drawLine(
                    context.playerPosition,
                    context.playerPosition + context.playerFacing * 22.0f,
                    {235, 210, 255, 255});
            }
        },
    });

    std::stable_sort(
        drawEntries.begin(),
        drawEntries.end(),
        [](const ActorDrawEntry& left, const ActorDrawEntry& right) {
            return left.depthY < right.depthY;
        });
    for (const ActorDrawEntry& entry : drawEntries) {
        if (entry.draw) {
            entry.draw();
        }
    }
}

} // namespace

void Game::renderBaseBackdrop(Renderer& renderer) const
{
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    if (baseArea_ == BaseArea::HomeInterior) {
        drawHomeInteriorBackdrop(renderer);
    } else {
        if (renderer.hasBaseMapTexture()) {
            renderer.drawBaseMapTexture(map.pos, map.size);
        } else {
            renderer.fillRect(map.pos, map.size, {68, 96, 58, 255});
            renderer.drawRect(map.pos, map.size, {156, 128, 82, 255});
            renderer.fillRect({62.0f, 456.0f}, {1156.0f, 88.0f}, {98, 84, 58, 255});
            renderer.fillRect({566.0f, 130.0f}, {132.0f, 430.0f}, {92, 78, 54, 255});
            renderer.fillRect({330.0f, 72.0f}, {154.0f, 100.0f}, {96, 54, 62, 255});
            renderer.drawRect({330.0f, 72.0f}, {154.0f, 100.0f}, {216, 184, 130, 255});
            renderer.drawText({350.0f, 106.0f}, "ルネの家", {246, 235, 255, 255}, 2);
            renderer.fillRect({600.0f, 586.0f}, {80.0f, 34.0f}, {38, 30, 36, 255});
            renderer.drawCircle({640.0f, 602.0f}, 42.0f, {160, 122, 80, 255});
        }
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    applyHiddenRouteFacilityAvailability(
        facilities,
        hasStoryFlag(HiddenEndingPeopleGoneFlag),
        hasStoryFlag(StoryHiddenOrbitCorruptionUnlockedFlag),
        hiddenBaseNpcRemoved("merchant_npc"),
        hiddenBaseNpcRemoved("processor_npc"),
        hiddenBaseNpcRemoved("elder"));
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    applyBaseStoryFacilityOffsets(facilities, baseStoryFacilityOffsets_);
    const Vec2 mouse = currentRenderMousePosition(renderer);
    const bool showInteractionHints = !dialogue_.active();
    drawBaseFacilities(renderer, facilities, baseArea_, ringWorkshopUnlocked_, basePlayerPosition_, mouse, showInteractionHints);
    renderBaseEditOverlay(renderer);
    drawBaseActors(
        renderer,
        facilities,
        mouse,
        BaseActorRenderContext{
            baseArea_,
            ringWorkshopUnlocked_,
            basePlayerPosition_,
            basePlayerFacing_,
            balance_.playerRadius,
            baseActorIdleAnimationTime_,
            basePlayerSpriteAnimationTime_,
            basePlayerSpriteWalking_,
            basePlayerSpriteFlipHorizontal_,
            showInteractionHints,
            &inventory_,
            &baseNpcSpriteFlipHorizontal_,
        },
        [&]() {
            renderPlayerFootstepDust(renderer);
        });

    renderBaseMiningRescueDropEvent(renderer);
    renderHiddenBaseOrbit(renderer);
    renderTopInfoBar(renderer);
}

void Game::renderBaseScreen(Renderer& renderer) const
{
    const bool roguelikeOverlay = roguelikeFacilityUiActive();
    if (!basePresentationActive() && !roguelikeOverlay) {
        return;
    }

    renderer.setScreenSpace();
    std::optional<BaseFacility> interactionFacility;
    const float ringPreviewSeconds = baseRingPreviewAnimationTime_;
    if (!roguelikeOverlay) {
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    if (baseArea_ == BaseArea::HomeInterior) {
        drawHomeInteriorBackdrop(renderer);
    } else {
        if (renderer.hasBaseMapTexture()) {
            renderer.drawBaseMapTexture(map.pos, map.size);
        } else {
            renderer.fillRect(map.pos, map.size, {68, 96, 58, 255});
            renderer.drawRect(map.pos, map.size, {156, 128, 82, 255});
        renderer.fillRect({62.0f, 456.0f}, {1156.0f, 88.0f}, {98, 84, 58, 255});
        renderer.fillRect({566.0f, 130.0f}, {132.0f, 430.0f}, {92, 78, 54, 255});
        renderer.fillRect({330.0f, 72.0f}, {154.0f, 100.0f}, {96, 54, 62, 255});
        renderer.drawRect({330.0f, 72.0f}, {154.0f, 100.0f}, {216, 184, 130, 255});
        renderer.drawText({350.0f, 106.0f}, "ルネの家", {246, 235, 255, 255}, 2);
        renderer.fillRect({600.0f, 586.0f}, {80.0f, 34.0f}, {38, 30, 36, 255});
            renderer.drawCircle({640.0f, 602.0f}, 42.0f, {160, 122, 80, 255});
        }
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    applyHiddenRouteFacilityAvailability(
        facilities,
        hasStoryFlag(HiddenEndingPeopleGoneFlag),
        hasStoryFlag(StoryHiddenOrbitCorruptionUnlockedFlag),
        hiddenBaseNpcRemoved("merchant_npc"),
        hiddenBaseNpcRemoved("processor_npc"),
        hiddenBaseNpcRemoved("elder"));
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    applyBaseStoryFacilityOffsets(facilities, baseStoryFacilityOffsets_);
    const Vec2 mouse = currentRenderMousePosition(renderer);
    const bool showInteractionHints = !dialogue_.active();
    if (showInteractionHints) {
    if (const BaseFacility* selectedFacility = selectBaseInteractionFacility(basePlayerPosition_, basePlayerFacing_, baseArea_, facilities)) {
        interactionFacility = *selectedFacility;
    }
    }
    drawBaseFacilities(renderer, facilities, baseArea_, ringWorkshopUnlocked_, basePlayerPosition_, mouse, showInteractionHints);
    renderBaseEditOverlay(renderer);
    drawBaseActors(
        renderer,
        facilities,
        mouse,
        BaseActorRenderContext{
            baseArea_,
            ringWorkshopUnlocked_,
            basePlayerPosition_,
            basePlayerFacing_,
            balance_.playerRadius,
            baseActorIdleAnimationTime_,
            basePlayerSpriteAnimationTime_,
            basePlayerSpriteWalking_,
            basePlayerSpriteFlipHorizontal_,
            showInteractionHints,
            &inventory_,
            &baseNpcSpriteFlipHorizontal_,
        },
        [&]() {
            renderPlayerFootstepDust(renderer);
        });

    renderBaseMiningRescueDropEvent(renderer);
    renderHiddenBaseOrbit(renderer);
    renderTopInfoBar(renderer);
    }

    char buffer[256];
    const bool panelUiActive = baseRingWorkshopActive_ ||
        baseDiaryActive_ ||
        baseBookshelfActive_ ||
        baseStorageActive_ ||
        baseProcessingUiMode_ != ProcessingUiMode::Closed ||
        baseSellActive_ ||
        baseUpgradeActive_ ||
        baseMiningStartChoiceActive_;
    const bool bottomControlHelpBlocked =
        dialogue_.active() ||
        pendingStoryTriggerDelayActive() ||
        !pendingStoryTrigger_.empty() ||
        !pendingStoryTriggers_.empty() ||
        firstItemAcquisitionNoticeActive();
    const bool rescueDropActive = baseMiningRescueDrop_.active;
    const bool storageActionDialogActive = baseStorageActive_ &&
        (baseStorageMode_ == StorageUiMode::ChooseAction || baseStorageMode_ == StorageUiMode::Bulk);
    const bool merchantActionDialogActive = baseSellActive_ && baseMerchantMode_ == MerchantUiMode::ChooseAction;
    const bool processingActionDialogActive = baseProcessingUiMode_ == ProcessingUiMode::ChooseAction;
    const bool bookshelfMenuDialogActive = baseBookshelfActive_ && bookshelfPage_ == BookshelfPage::Menu;
    const bool bookshelfWideActive = baseBookshelfActive_ && bookshelfPage_ != BookshelfPage::Menu;
    const bool ringWorkshopActionDialogActive = baseRingWorkshopActive_ && baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction;
    const bool ringWorkshopWideActive = baseRingWorkshopActive_ && baseRingWorkshopMode_ != RingWorkshopMode::ChooseAction;
    const UiRect panel = baseDiaryActive_
        ? basePanelRect()
        : (baseMiningStartChoiceActive_
        ? baseMiningStartPanelRect()
        : (storageActionDialogActive
        ? (baseStorageMode_ == StorageUiMode::Bulk ? storageBulkDialogRect() : storageActionDialogRect())
        : (merchantActionDialogActive
        ? merchantActionDialogRect()
        : (processingActionDialogActive
        ? merchantActionDialogRect()
        : (bookshelfMenuDialogActive
        ? bookshelfMenuPanelRect(bookshelfMenuItemCount())
        : (ringWorkshopActionDialogActive
        ? ringWorkshopActionDialogRect()
        : (ringWorkshopWideActive
        ? ringWorkshopPanelRect()
        : ((baseProcessingUiMode_ == ProcessingUiMode::Enhance ||
        bookshelfWideActive ||
        (baseStorageActive_ && !storageActionDialogActive) ||
        (baseSellActive_ && baseMerchantMode_ != MerchantUiMode::ChooseAction))
        ? merchantPanelRect()
        : (baseUpgradeActive_ ? baseUpgradePanelRect() : basePanelRect())))))))));
    std::optional<UiWindowScope> panelWindow;
    std::optional<UiCancelControlScope> panelCancelScope;
    if (panelUiActive) {
        const char* panelTitle = "魔女の拠点";
        if (roguelikeOverlay && baseSellActive_) {
            if (baseMerchantMode_ == MerchantUiMode::Buy) {
                panelTitle = "野良商人 購入";
            } else if (baseMerchantMode_ == MerchantUiMode::Sell) {
                panelTitle = "野良商人 売却";
            } else {
                panelTitle = "野良商人";
            }
        } else if (roguelikeOverlay && baseProcessingUiMode_ != ProcessingUiMode::Closed) {
            panelTitle = "野良加工職人";
        } else if (roguelikeOverlay && baseUpgradeActive_) {
            panelTitle = "修練者";
        } else if (baseBookshelfActive_) {
            panelTitle = bookshelfPage_ == BookshelfPage::Items
                ? "アイテム図鑑"
                : (bookshelfPage_ == BookshelfPage::Enemies ? "モンスター図鑑" : "本棚");
        } else if (baseRingWorkshopActive_) {
            panelTitle = baseRingWorkshopMode_ == RingWorkshopMode::Respec
                ? "レベルアップ配分調整"
                : "リング工房";
        } else if (baseProcessingUiMode_ != ProcessingUiMode::Closed) {
            panelTitle = "作業台";
        } else if (baseSellActive_) {
            if (baseMerchantMode_ == MerchantUiMode::Buy) {
                panelTitle = "商人ワゴン 購入";
            } else if (baseMerchantMode_ == MerchantUiMode::Sell) {
                panelTitle = "商人ワゴン 売却";
            } else {
                panelTitle = "商人ワゴン";
            }
        } else if (baseStorageActive_) {
            if (baseStorageMode_ == StorageUiMode::Deposit) {
                panelTitle = "収納箱にしまう";
            } else if (baseStorageMode_ == StorageUiMode::Withdraw) {
                panelTitle = "収納箱から取り出す";
            } else if (baseStorageMode_ == StorageUiMode::Bulk) {
                panelTitle = "収納箱 一括操作";
            } else {
                panelTitle = "収納箱";
            }
        } else if (baseUpgradeActive_) {
            panelTitle = "拠点強化炉";
        } else if (baseDiaryActive_) {
            panelTitle = "日記";
        } else if (baseMiningStartChoiceActive_) {
            panelTitle = "ダンジョン入口";
        }
        const bool panelCancelButton = true;
        if (panelCancelButton) {
            panelCancelScope.emplace(baseCancelState_);
        }
        panelWindow.emplace(renderer, "base.panel", panel, panelTitle, BaseFacilityWindowHelpText, UiWindowOptions{true, panelCancelButton});
    }

    if (baseDiaryActive_) {
        renderBaseDiaryScreen(renderer, panel);
    } else if (baseStorageActive_) {
        if (baseStorageMode_ == StorageUiMode::ChooseAction) {
            std::snprintf(buffer, sizeof(buffer), "収納数：%d/%d", warehouseUsedSlots(), warehouseCapacity());
            renderer.drawText(smallActionInfoTextPos(panel), buffer, {198, 198, 206, 255}, 2);
            constexpr std::array<std::string_view, 3> Choices{"しまう", "取り出す", "一括操作"};
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                drawUiButton(renderer, storageActionChoiceRect(i), Choices[static_cast<std::size_t>(i)], i == baseStorageActionSelection_, uiActionButtonStyle());
            }
        } else if (baseStorageMode_ == StorageUiMode::Bulk) {
            std::snprintf(buffer, sizeof(buffer), "収納数：%d/%d", warehouseUsedSlots(), warehouseCapacity());
            renderer.drawText(smallActionInfoTextPos(panel), buffer, {198, 198, 206, 255}, 2);
            constexpr std::array<std::string_view, 4> Choices{
                "全部しまう",
                "プリセット1を準備",
                "プリセット2を準備",
                "プリセット3を準備",
            };
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                UiButtonStyle style = uiActionButtonStyle();
                const bool enabled = i == 0 ||
                    (i - 1 < unlockedRingPresetSlotCount() && ringPresets_.registered(i - 1));
                if (!enabled) {
                    style.fill = {18, 24, 42, 150};
                    style.fillHot = {18, 24, 42, 150};
                    style.outline = {120, 122, 138, 120};
                    style.outlineHot = style.outline;
                    style.text = ui::TextDisabled;
                    style.imageTint = {180, 180, 190, 130};
                    style.imageTintHot = {190, 190, 200, 150};
                }
                drawUiButton(
                    renderer,
                    storageBulkChoiceRect(i),
                    Choices[static_cast<std::size_t>(i)],
                    enabled && i == baseStorageBulkSelection_,
                    style);
            }
        } else {
            const UiRect detailPanel = merchantDetailPanelRect();
            InventoryUiEntryView detailEntry{};
            const SpellRingItem* selectedRingItem = nullptr;
            if (baseStorageMode_ == StorageUiMode::Deposit) {
                const int sourceCount = storageDepositSourceCountForUnlockedRings(unlockedRingCount());
                std::array<UiTabItem, StorageDepositSourceCount> sourceTabs{};
                std::array<UiRect, StorageDepositSourceCount> sourceTabRects{};
                for (int i = 0; i < sourceCount; ++i) {
                    const int source = storageDepositSourceValue(i);
                    sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(source, true);
                    sourceTabRects[static_cast<std::size_t>(i)] = storageDepositSourceRect(i);
                }
                const int currentTab = storageDepositSourceTabIndex(baseStorageDepositSource_);
                drawUiTabs(
                    renderer,
                    baseStorageDepositSourceTabs_,
                    currentTab,
                    sourceTabs.data(),
                    sourceCount,
                    sourceTabRects.data());

                std::snprintf(buffer, sizeof(buffer), "収納箱 %d/%d", warehouseUsedSlots(), warehouseCapacity());
                renderer.drawText(storageTransferCountTextPos(), buffer, ui::TextMuted, 2);

                if (baseItemSourceIsRing(baseStorageDepositSource_)) {
                    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseStorageDepositSource_), 0, SpellRingCount - 1);
                    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                    const int selectedRingIndex = ringItems.empty()
                        ? -1
                        : std::clamp(baseStorageDepositSelection_, 0, static_cast<int>(ringItems.size()) - 1);
                    drawStorageRingPreview(
                        renderer,
                        spellRing_,
                        objectCatalog_,
                        balance_,
                        ringIndex,
                        selectedRingIndex,
                        ringPreviewSeconds);
                    for (int i = 0; i < static_cast<int>(ringItems.size()); ++i) {
                        const SpellRingItem& item = ringItems[static_cast<std::size_t>(i)];
                        if (!item.objectId.empty()) {
                            continue;
                        }
                        UiRect labelRect = storageRingItemRect(
                            item,
                            spellRing_,
                            balance_,
                            ringIndex,
                            i,
                            static_cast<int>(ringItems.size()),
                            ringPreviewSeconds);
                        labelRect.size.y += MerchantSellRingItemLabelExtraHeight;
                        drawInventoryUiSlotBottomLabel(renderer, labelRect, "収納不可", ui::TextDisabled);
                    }
                    if (!ringItems.empty() && selectedRingIndex >= 0) {
                        selectedRingItem = &ringItems[static_cast<std::size_t>(selectedRingIndex)];
                        detailEntry = storageTransferTargetView(storageDepositTargetForSourceSlot(baseStorageDepositSource_, selectedRingIndex));
                    }
                } else {
                    for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                        const StorageTransferTarget target = storageDepositTargetForSourceSlot(baseStorageDepositSource_, i);
                        const InventoryUiEntryView view = storageTransferTargetView(target);
                        const bool draggingThis =
                            baseStoragePointerDragTriggered_ &&
                            baseStoragePointerOperation_ == StorageQuantityOperation::Deposit &&
                            baseStoragePointerTarget_.source == BaseItemSource::Backpack &&
                            baseStoragePointerTarget_.slotIndex == i;
                        InventoryUiSlotStyle style{i == baseStorageDepositSelection_, draggingThis, 48.0f};
                        if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                            style.showTopRightCount = true;
                            style.topRightCount = view.stackCount;
                        }
                        drawInventoryUiSlot(renderer, storageTransferGridSlotRect(i), view, style);
                    }
                    detailEntry = storageTransferTargetView(storageDepositTargetForScreenSlot(
                        std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1))));
                    drawUiButton(renderer, storageTransferSortButtonRect(), "並び替え", false, uiActionButtonStyle());
                }
            } else if (baseStorageMode_ == StorageUiMode::Withdraw) {
                const int warehousePageCount = std::max(1, (warehouseCapacity() + StorageWithdrawSlotCount - 1) / StorageWithdrawSlotCount);
                const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                std::snprintf(buffer, sizeof(buffer), "収納箱 %d/%d", warehouseUsedSlots(), warehouseCapacity());
                renderer.drawText(storageWithdrawCountTextPos(), buffer, ui::TextMuted, 2);
                drawStorageWithdrawHeader(renderer, warehousePage, warehousePageCount);
                for (int i = 0; i < StorageWithdrawSlotCount; ++i) {
                    const StorageTransferTarget target = storageWithdrawTargetForSlot(i);
                    const InventoryUiEntryView view = storageTransferTargetView(target);
                    const bool draggingThis =
                        baseStoragePointerDragTriggered_ &&
                        baseStoragePointerOperation_ == StorageQuantityOperation::Withdraw &&
                        baseStoragePointerTarget_.source == BaseItemSource::Warehouse &&
                        baseStoragePointerTarget_.slotIndex == i;
                    InventoryUiSlotStyle style{i == baseStorageWithdrawSelection_, draggingThis, 48.0f};
                    if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                        style.showTopRightCount = true;
                        style.topRightCount = view.stackCount;
                    }
                    drawInventoryUiSlot(renderer, storageWithdrawSlotRect(i), view, style);
                }
                detailEntry = storageTransferTargetView(storageWithdrawTargetForSlot(
                    std::clamp(baseStorageWithdrawSelection_, 0, StorageWithdrawSlotCount - 1)));
                drawUiButton(renderer, storageWithdrawSortButtonRect(), "並び替え", false, uiActionButtonStyle());
            }

            if (detailEntry.item == nullptr && selectedRingItem != nullptr) {
                drawUiSubPanel(renderer, detailPanel);
                float detailLineY = drawUiDetailHeader(renderer, detailPanel, ringItemDisplayName(objectCatalog_, *selectedRingItem));
                drawUiDetailText(renderer, detailPanel, detailLineY, "このアイテムは収納箱にしまえません。");
            } else {
                drawInventoryUiDetailPanel(
                    renderer,
                    detailPanel,
                    detailEntry,
                    objectCatalog_,
                    encyclopedia_,
                    InventoryUiDetailOptions{.animationSeconds = ringPreviewSeconds});
            }
            const char* commandLabel = baseStorageCommandOperation_ == StorageQuantityOperation::Withdraw
                ? "取り出す"
                : "しまう";
            const std::array<UiCommandMenuItem, 1> commandItems{{
                {commandLabel, storageTransferTargetAvailable(baseStorageCommandTarget_)},
            }};
            drawUiCommandMenu(renderer, baseStorageCommandMenu_, commandItems.data(), static_cast<int>(commandItems.size()));
        }
    } else if (baseBookshelfActive_) {
        renderBookshelfScreen(renderer);
    } else if (baseRingWorkshopActive_) {
        if (baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction) {
            renderer.drawText(smallActionInfoTextPos(panel), "何を調整しますか？", {198, 198, 206, 255}, 2);
            for (int i = 0; i < RingWorkshopActionCount; ++i) {
                drawUiButton(
                    renderer,
                    ringWorkshopActionChoiceRect(i),
                    ringWorkshopActionLabel(i),
                    i == baseRingWorkshopSelection_,
                    uiActionButtonStyle());
            }
        } else if (baseRingWorkshopMode_ == RingWorkshopMode::Respec) {
            const int ringCount = unlockedRingCount();
            const int ringIndex = std::clamp(baseRingWorkshopRingIndex_, 0, ringCount - 1);
            std::array<UiTabItem, SpellRingCount> ringTabs{};
            std::array<UiRect, SpellRingCount> ringTabRects{};
            std::array<std::string, SpellRingCount> ringTabLabels{};
            for (int i = 0; i < ringCount; ++i) {
                ringTabLabels[static_cast<std::size_t>(i)] = "リング " + std::to_string(i + 1);
                ringTabs[static_cast<std::size_t>(i)] = {
                    ringTabLabels[static_cast<std::size_t>(i)],
                    true,
                    ringDisplayIconImageNumber(i),
                };
                ringTabRects[static_cast<std::size_t>(i)] = ringWorkshopRingTabRect(i, ringCount);
            }
            drawUiTabs(
                renderer,
                baseRingWorkshopRingTabs_,
                ringIndex,
                ringTabs.data(),
                ringCount,
                ringTabRects.data());

            const UiRect respecPanel = ringWorkshopRespecPanelRect();
            std::snprintf(buffer, sizeof(buffer), "合計強化点 %d", ringLevelUpgradePointTotal());
            renderer.drawText(respecPanel.pos + Vec2{24.0f, 6.0f}, buffer, ui::TextMuted, 2);

            const RingLevelUpgradePoints& currentRingPoints = levelRingUpgradePoints_[static_cast<std::size_t>(ringIndex)];
            const RingLevelUpgradePoints& draftRingPoints = ringWorkshopDraftUpgradePoints_[static_cast<std::size_t>(ringIndex)];
            std::array<UiVerticalTabItem, RingLevelUpgradeKindCount> respecTabs{};
            std::array<UiRect, RingLevelUpgradeKindCount> respecTabRects{};
            std::array<std::string, RingLevelUpgradeKindCount> respecTabValues{};
            for (int i = 0; i < RingLevelUpgradeKindCount; ++i) {
                const RingLevelUpgradeKind kind = ringWorkshopKindForIndex(i);
                const int currentPoints = ringLevelUpgradePoint(currentRingPoints, kind);
                const int draftPoints = ringLevelUpgradePoint(draftRingPoints, kind);
                const RingLevelUpgradeSelection selection{ringIndex, kind};
                const bool sourceSelected = ringWorkshopRespecSource_ &&
                    sameRingLevelUpgradeSelection(*ringWorkshopRespecSource_, selection);
                std::snprintf(buffer, sizeof(buffer), "%d -> %d", currentPoints, draftPoints);
                respecTabValues[static_cast<std::size_t>(i)] = buffer;
                respecTabs[static_cast<std::size_t>(i)] = {
                    ringLevelUpgradeKindName(kind),
                    respecTabValues[static_cast<std::size_t>(i)],
                    true,
                    sourceSelected ? Color{255, 230, 150, 255} : ui::TextMuted,
                };
                respecTabRects[static_cast<std::size_t>(i)] = ringWorkshopRespecKindRect(i);
            }
            UiVerticalTabsStyle respecTabStyle;
            respecTabStyle.labelScale = 3;
            respecTabStyle.valueScale = 2;
            UiTabsState respecTabsState = baseRingWorkshopUpgradeTabs_;
            const int selectedRespecTab = baseRingWorkshopSelection_ < RingLevelUpgradeKindCount
                ? baseRingWorkshopSelection_
                : -1;
            if (selectedRespecTab < 0) {
                respecTabsState.focusedIndex = -1;
            }
            drawUiVerticalTabs(
                renderer,
                respecTabsState,
                selectedRespecTab,
                respecTabs.data(),
                static_cast<int>(respecTabs.size()),
                respecTabRects.data(),
                respecTabStyle);

            const UiRect detailPanel = ringWorkshopRespecDetailPanelRect();
            drawUiSubPanel(renderer, detailPanel);
            const int selectedKindIndex = std::clamp(baseRingWorkshopSelection_, 0, RingLevelUpgradeKindCount - 1);
            const RingLevelUpgradeKind selectedKind = ringWorkshopKindForIndex(selectedKindIndex);
            const auto valueForPoints = [this](int selectedRing, RingLevelUpgradeKind kind, const RingLevelUpgradePoints& points) {
                switch (kind) {
                case RingLevelUpgradeKind::Radius:
                    return effectiveInitialRingRadiusForRing(selectedRing, points.radius) *
                        SpellRingSystem::baseRadiusMultiplierForRing(selectedRing);
                case RingLevelUpgradeKind::Speed:
                    return linearMetersPerSecondForAngularSpeed(
                        effectiveInitialRingSpeedForRing(selectedRing, points.speed) *
                            SpellRingSystem::baseSpeedMultiplierForRing(selectedRing),
                        effectiveInitialRingRadiusForRing(selectedRing, points.radius) *
                            SpellRingSystem::baseRadiusMultiplierForRing(selectedRing));
                case RingLevelUpgradeKind::WeightLimit:
                    return effectiveInitialRingWeightLimitForRing(selectedRing, points.weightLimit);
                }
                return 0.0f;
            };
            float detailY = drawUiDetailHeader(
                renderer,
                detailPanel,
                baseRingWorkshopSelection_ == RingLevelUpgradeKindCount ? "再調整確定" : ringLevelUpgradeKindName(selectedKind));
            std::snprintf(buffer, sizeof(buffer), "リング %d", ringIndex + 1);
            drawUiDetailLine(renderer, detailPanel, detailY, "対象", buffer);
            const int selectedCurrentPoints = ringLevelUpgradePoint(currentRingPoints, selectedKind);
            const int selectedDraftPoints = ringLevelUpgradePoint(draftRingPoints, selectedKind);
            std::snprintf(buffer, sizeof(buffer), "%d点 / %s",
                selectedCurrentPoints,
                formatRingWorkshopValue(selectedKind, valueForPoints(ringIndex, selectedKind, currentRingPoints)).c_str());
            drawUiDetailLine(renderer, detailPanel, detailY, "現在", buffer);
            std::snprintf(buffer, sizeof(buffer), "%d点 / %s",
                selectedDraftPoints,
                formatRingWorkshopValue(selectedKind, valueForPoints(ringIndex, selectedKind, draftRingPoints)).c_str());
            drawUiDetailLine(
                renderer,
                detailPanel,
                detailY,
                "配分案",
                buffer,
                selectedCurrentPoints == selectedDraftPoints ? ui::Text : Color{255, 230, 150, 255});
            if (ringWorkshopRespecSource_) {
                std::snprintf(buffer, sizeof(buffer), "リング%d %s",
                    ringWorkshopRespecSource_->ringIndex + 1,
                    ringLevelUpgradeKindName(ringWorkshopRespecSource_->kind));
                drawUiDetailLine(renderer, detailPanel, detailY, "移動元", buffer, Color{255, 230, 150, 255});
                drawUiDetailText(renderer, detailPanel, detailY, "次に選んだ項目へ1点移します。");
            } else {
                drawUiDetailLine(renderer, detailPanel, detailY, "移動元", "未選択", ui::TextMuted);
                drawUiDetailText(renderer, detailPanel, detailY, "ポイントがある項目を選ぶと移動元になります。");
            }
            drawBaseDetailMoneyCostLine(
                renderer,
                objectCatalog_,
                detailPanel,
                detailY,
                "必要素材",
                ringWorkshopRespecMoneyCost(),
                money_);
            drawBaseDetailMaterialCostLine(
                renderer,
                objectCatalog_,
                detailPanel,
                detailY,
                "",
                MaterialType::MoonFragment,
                ringWorkshopRespecMoonCost(),
                inventory_.materialCount(MaterialType::MoonFragment));

            UiButtonStyle confirmStyle = uiActionButtonStyle();
            if (!ringWorkshopRespecChanged()) {
                confirmStyle.text = ui::TextMuted;
            }
            drawUiButton(
                renderer,
                ringWorkshopRespecConfirmRect(),
                ringWorkshopRespecChanged() ? "再調整確定" : "変更なし",
                baseRingWorkshopSelection_ == RingLevelUpgradeKindCount,
                confirmStyle);
        } else if (baseRingWorkshopMode_ == RingWorkshopMode::Upgrade) {
            const int ringCount = unlockedRingCount();
            const int ringIndex = std::clamp(baseRingWorkshopRingIndex_, 0, ringCount - 1);
            std::array<UiTabItem, SpellRingCount> ringTabs{};
            std::array<UiRect, SpellRingCount> ringTabRects{};
            std::array<std::string, SpellRingCount> ringTabLabels{};
            for (int i = 0; i < ringCount; ++i) {
                ringTabLabels[static_cast<std::size_t>(i)] = "リング " + std::to_string(i + 1);
                ringTabs[static_cast<std::size_t>(i)] = {
                    ringTabLabels[static_cast<std::size_t>(i)],
                    true,
                    ringDisplayIconImageNumber(i),
                };
                ringTabRects[static_cast<std::size_t>(i)] = ringWorkshopRingTabRect(i, ringCount);
            }
            drawUiTabs(
                renderer,
                baseRingWorkshopRingTabs_,
                ringIndex,
                ringTabs.data(),
                ringCount,
                ringTabRects.data());

            const UiScrollAreaStyle scrollStyle = ringWorkshopScrollAreaStyle();
            const float scrollContentHeight = ringWorkshopUpgradeScrollContentHeight();
            const UiScrollAreaLayout scrollLayout = makeUiScrollAreaLayout(
                ringWorkshopUpgradeScrollViewportRect(),
                scrollContentHeight,
                baseRingWorkshopUpgradeScrollOffset_,
                scrollStyle);
            const float minMeters = ringWorkshopRadiusMinForRing(ringIndex);
            const float maxMeters = ringWorkshopRadiusMaxForRing(ringIndex);
            const float currentMeters = ringWorkshopRadiusSettingForRing(ringIndex);
            std::array<UiVerticalTabItem, RingWorkshopUpgradeDisplayCount> upgradeTabs{};
            std::array<UiRect, RingWorkshopUpgradeDisplayCount> upgradeTabRects{};
            std::array<std::string, RingWorkshopUpgradeDisplayCount> upgradeTabValues{};
            for (int i = 0; i < RingWorkshopUpgradeDisplayCount; ++i) {
                const bool implemented = i < RingWorkshopImplementedUpgradeCount;
                if (implemented) {
                    const auto upgrade = ringWorkshopUpgradeForDisplayIndex(i);
                    const int level = ringWorkshopUpgradeLevel(upgrade);
                    const int maxLevel = ringWorkshopUpgradeMaxLevel(upgrade);
                    if (level >= maxLevel) {
                        upgradeTabValues[static_cast<std::size_t>(i)] = "上限";
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "Lv.%d/%d", level, maxLevel);
                        upgradeTabValues[static_cast<std::size_t>(i)] = buffer;
                    }
                } else {
                    upgradeTabValues[static_cast<std::size_t>(i)] = "未実装";
                }
                const bool maxed = implemented &&
                    ringWorkshopUpgradeLevel(ringWorkshopUpgradeForDisplayIndex(i)) >= ringWorkshopUpgradeMaxLevel(ringWorkshopUpgradeForDisplayIndex(i));
                upgradeTabs[static_cast<std::size_t>(i)] = {
                    ringWorkshopUpgradeShortName(i),
                    upgradeTabValues[static_cast<std::size_t>(i)],
                    implemented,
                    maxed ? Color{160, 220, 190, 255} : ui::TextMuted,
                };
                upgradeTabRects[static_cast<std::size_t>(i)] = ringWorkshopUpgradeItemRect(scrollLayout, i);
            }
            UiVerticalTabsStyle upgradeTabStyle;
            upgradeTabStyle.labelScale = 2;
            renderer.pushClipRect(scrollLayout.viewport.pos, scrollLayout.viewport.size);
            const float contentTop = scrollLayout.content.pos.y - scrollLayout.scrollOffset;
            renderer.drawText(
                {scrollLayout.content.pos.x, contentTop},
                "半径調整",
                {198, 198, 206, 255},
                2);
            UiGaugeStyle radiusGaugeStyle;
            radiusGaugeStyle.tickCount = 6;
            radiusGaugeStyle.fill.start = maxMeters > minMeters + 0.001f ? Color{132, 230, 250, 230} : ui::TextDisabled;
            radiusGaugeStyle.fill.end = maxMeters > minMeters + 0.001f ? Color{190, 246, 220, 230} : ui::TextDisabled;
            const float radiusRatio = maxMeters > minMeters + 0.001f
                ? (currentMeters - minMeters) / std::max(0.001f, maxMeters - minMeters)
                : 0.0f;
            const UiRect radiusGaugeRect = ringWorkshopRadiusGaugeRect(scrollLayout);
            drawUiGauge(renderer, radiusGaugeRect, radiusRatio, radiusGaugeStyle);
            char currentRadiusBuffer[32];
            char radiusRangeBuffer[48];
            std::snprintf(currentRadiusBuffer, sizeof(currentRadiusBuffer), "%.2fm", currentMeters);
            std::snprintf(radiusRangeBuffer, sizeof(radiusRangeBuffer), "（%.2f～%.2fm）", minMeters, maxMeters);
            const int radiusInfoScale = 2;
            const Vec2 currentRadiusSize = renderer.measureText(currentRadiusBuffer, radiusInfoScale);
            const Vec2 radiusRangeSize = renderer.measureText(radiusRangeBuffer, radiusInfoScale);
            Vec2 radiusInfoPos{
                radiusGaugeRect.pos.x + (radiusGaugeRect.size.x - currentRadiusSize.x - radiusRangeSize.x) * 0.5f,
                contentTop + RingWorkshopScrollRadiusInfoTop,
            };
            renderer.drawText(radiusInfoPos, currentRadiusBuffer, ui::Text, radiusInfoScale);
            radiusInfoPos.x += currentRadiusSize.x;
            renderer.drawText(radiusInfoPos, radiusRangeBuffer, Color{218, 218, 226, 255}, radiusInfoScale);
            renderer.drawText(
                {scrollLayout.content.pos.x, contentTop + RingWorkshopScrollSectionTop},
                "リング強化",
                {198, 198, 206, 255},
                2);
            drawUiVerticalTabs(
                renderer,
                baseRingWorkshopUpgradeTabs_,
                baseRingWorkshopSelection_,
                upgradeTabs.data(),
                static_cast<int>(upgradeTabs.size()),
                upgradeTabRects.data(),
                upgradeTabStyle);
            renderer.popClipRect();
            drawUiScrollAreaScrollbar(renderer, scrollLayout, scrollStyle);

            const UiRect detailPanel = ringWorkshopUpgradeDetailPanelRect();
            drawUiSubPanel(renderer, detailPanel);
            const int selected = std::clamp(baseRingWorkshopSelection_, 0, RingWorkshopUpgradeDisplayCount - 1);
            const bool implemented = selected < RingWorkshopImplementedUpgradeCount;
            const auto drawTextRun = [&renderer](Vec2& pos, std::string_view text, Color color, int scale) {
                renderer.drawText(pos, text, color, scale);
                pos.x += renderer.measureText(text, scale).x;
            };
            InlineItemTextStyle inlineStyle;
            inlineStyle.scale = 2;
            inlineStyle.iconTextGap = 4.0f;
            inlineStyle.iconScale = 1.15f;
            const auto drawInlineTextRun = [&](Vec2& pos, std::string_view text, Color color) {
                inlineStyle.text = color;
                drawInlineItemText(renderer, objectCatalog_, pos, text, inlineStyle);
                pos.x += measureInlineItemText(renderer, text, inlineStyle).x;
            };
            const auto beginDetailRow = [&renderer, detailPanel](float& y, std::string_view label) {
                constexpr float LabelWidth = 96.0f;
                const UiRect content = uiSubPanelContentRect(detailPanel);
                renderer.drawText({content.pos.x, y}, label, ui::TextMuted, 2);
                return Vec2{content.pos.x + LabelWidth, y};
            };
            const auto drawMoneyCostLine = [&](float& y, std::string_view label, int cost) {
                Vec2 pos = beginDetailRow(y, label);
                const Color numberColor = money_ >= cost ? ui::Text : Color{238, 82, 82, 255};
                drawInlineTextRun(pos, inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + " ", ui::Text);
                drawTextRun(pos, std::to_string(cost), numberColor, 2);
                drawTextRun(pos, "G", ui::Text, 2);
                y += 31.0f;
            };
            const auto drawMoonCostLine = [&](float& y, std::string_view label, int cost) {
                const int owned = inventory_.materialCount(MaterialType::MoonFragment);
                const Color numberColor = owned >= cost ? ui::Text : Color{238, 82, 82, 255};
                Vec2 pos = beginDetailRow(y, label);
                drawInlineTextRun(pos, inlineMaterialIconTag(MaterialType::MoonFragment) + std::string(materialTypeDisplayName(MaterialType::MoonFragment)) + " ×", ui::Text);
                drawTextRun(pos, std::to_string(cost), numberColor, 2);
                drawTextRun(pos, "（", ui::TextMuted, 2);
                drawTextRun(pos, std::to_string(owned), numberColor, 2);
                drawTextRun(pos, "）", ui::TextMuted, 2);
                y += 31.0f;
            };
            const char* confirmLabel = "強化する";
            UiButtonStyle confirmStyle = uiActionButtonStyle();
            if (implemented) {
                const auto upgrade = ringWorkshopUpgradeForDisplayIndex(selected);
                const int level = ringWorkshopUpgradeLevel(upgrade);
                const int maxLevel = ringWorkshopUpgradeMaxLevel(upgrade);
                const bool maxed = level >= maxLevel;
                float detailY = drawUiDetailHeader(renderer, detailPanel, ringWorkshopUpgradeName(upgrade));
                const UiRect detailContent = uiSubPanelContentRect(detailPanel);
                renderer.drawWrappedText(
                    {detailContent.pos.x, detailY},
                    ringWorkshopUpgradeDescription(selected),
                    detailContent.size.x,
                    ui::TextMuted,
                    2);
                detailY += renderer.measureWrappedText(ringWorkshopUpgradeDescription(selected), detailContent.size.x, 2).y + 8.0f;
                std::snprintf(buffer, sizeof(buffer), "リング %d", ringIndex + 1);
                drawUiDetailLine(renderer, detailPanel, detailY, "対象", buffer);
                if (maxed) {
                    drawUiDetailLine(renderer, detailPanel, detailY, "効果", "上限到達済み", ui::TextMuted);
                    drawUiDetailLine(renderer, detailPanel, detailY, "必要素材", "なし", ui::TextMuted);
                    confirmLabel = "上限";
                    confirmStyle.text = ui::TextMuted;
                } else {
                    const std::string currentValue = ringWorkshopUpgradeValueText(upgrade, ringWorkshopUpgradeCurrentValue(upgrade));
                    const std::string nextValue = ringWorkshopUpgradeValueText(upgrade, ringWorkshopUpgradeNextValue(upgrade));
                    drawUiDetailLine(renderer, detailPanel, detailY, "効果", currentValue + " -> " + nextValue, Color{255, 230, 150, 255});
                    drawMoneyCostLine(detailY, "必要素材", ringWorkshopUpgradeMoneyCost(upgrade));
                    drawMoonCostLine(detailY, "", ringWorkshopUpgradeMoonCost(upgrade));
                }
            } else {
                float detailY = drawUiDetailHeader(renderer, detailPanel, "リング強化");
                drawUiDetailLine(renderer, detailPanel, detailY, "状態", "未実装", ui::TextMuted);
                drawUiDetailText(renderer, detailPanel, detailY, "この項目は現在利用できません。");
                confirmLabel = "未実装";
                confirmStyle.text = ui::TextDisabled;
            }
            drawUiButton(
                renderer,
                ringWorkshopUpgradeConfirmRect(),
                confirmLabel,
                false,
                confirmStyle);
        }
    } else if (baseProcessingUiMode_ == ProcessingUiMode::ChooseAction) {
        renderer.drawText(smallActionInfoTextPos(panel), "作業台で何をしますか？", {198, 198, 206, 255}, 2);
        constexpr std::array<std::string_view, 2> Choices{"一括修理", "強化"};
        for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
            drawUiButton(
                renderer,
                merchantActionChoiceRect(i),
                Choices[static_cast<std::size_t>(i)],
                i == baseProcessingActionSelection_,
                uiActionButtonStyle());
        }
        if (roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan) {
            const int targetCount = processingBulkRepairTargetCount();
            const int moneyCost = processingBulkRepairMoneyCost();
            const int oreCost = processingBulkRepairOreCost();
            std::snprintf(
                buffer,
                sizeof(buffer),
                "一括修理: 対象%d個 / %dG / 強化鉱石×%d",
                targetCount,
                moneyCost,
                oreCost);
            const Color textColor =
                money_ >= moneyCost && inventory_.materialCount(MaterialType::EnhancementOre) >= oreCost
                    ? ui::TextMuted
                    : Color{238, 82, 82, 255};
            renderer.drawText(smallActionInfoTextPos(panel) + Vec2{0.0f, 124.0f}, buffer, textColor, 2);
        }
    } else if (baseProcessingUiMode_ == ProcessingUiMode::Enhance) {
        const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount());
        std::array<UiTabItem, BaseProcessingSourceCount> sourceTabs{};
        std::array<UiRect, BaseProcessingSourceCount> sourceTabRects{};
        for (int i = 0; i < sourceCount; ++i) {
            const bool enabled = !(roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan &&
                baseItemSourceIsWarehouse(i));
            sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(i, enabled);
            sourceTabRects[static_cast<std::size_t>(i)] = baseProcessingSourceRect(i, sourceCount);
        }
        drawUiTabs(
            renderer,
            baseProcessingSourceTabs_,
            baseProcessingSource_,
            sourceTabs.data(),
            sourceCount,
            sourceTabRects.data());

        const auto entryViewForScreenSlot = [this](int slot) {
            InventoryUiEntryView view{};
            const ProcessingTarget target = processingTargetForScreenSlot(slot);
            if (!target.valid) {
                return view;
            }
            if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
                return storageEntryView(target.backpackEntry, target.warehouseEntry);
            }

            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
            if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size())) {
                const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
                view.item = objectForRingItem(objectCatalog_, ringItem);
                view.stats = inventoryUiStatsFromRingItem(ringItem);
                view.stackCount = 1;
            }
            return view;
        };

        const bool warehouseSource = baseItemSourceIsWarehouse(baseProcessingSource_);
        const bool ringSource = baseItemSourceIsRing(baseProcessingSource_);
        if (ringSource) {
            const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseProcessingSource_), 0, SpellRingCount - 1);
            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
            const int selectedRingItem = ringItems.empty()
                ? -1
                : std::clamp(baseProcessingSelection_, 0, static_cast<int>(ringItems.size()) - 1);
            drawBaseProcessingRingPreview(
                renderer,
                spellRing_,
                objectCatalog_,
                balance_,
                ringIndex,
                selectedRingItem,
                ringPreviewSeconds);
        } else if (warehouseSource) {
            const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
            const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
            drawExternalWarehouseSourceHeader(
                renderer,
                baseProcessingGridSlotRect,
                warehousePage,
                warehousePageCount);
            for (int i = 0; i < StoragePaneSlotCount; ++i) {
                const InventoryUiEntryView view = entryViewForScreenSlot(i);
                const bool unavailable = view.item != nullptr && !processingTargetHasAvailableCommand(processingTargetForScreenSlot(i));
                InventoryUiSlotStyle style{i == baseProcessingSelection_, unavailable, 48.0f};
                if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                    style.showTopRightCount = true;
                    style.topRightCount = view.stackCount;
                }
                drawInventoryUiSlot(renderer, externalWarehouseSourceSlotRect(baseProcessingGridSlotRect, i), view, style);
            }
        } else {
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const InventoryUiEntryView view = entryViewForScreenSlot(i);
                const bool unavailable = view.item != nullptr && !processingTargetHasAvailableCommand(processingTargetForScreenSlot(i));
                InventoryUiSlotStyle style{i == baseProcessingSelection_, unavailable, 48.0f};
                if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                    style.showTopRightCount = true;
                    style.topRightCount = view.stackCount;
                }
                drawInventoryUiSlot(renderer, baseProcessingGridSlotRect(i), view, style);
            }
        }

        const UiRect detailPanel = merchantDetailPanelRect();
        const float moneyRight = detailPanel.pos.x;
        drawMoneySummaryText(renderer, {moneyRight, detailPanel.pos.y + 12.0f}, money_);

        int selected = std::clamp(baseProcessingSelection_, 0, inventory_.screenSlotCount() - 1);
        if (warehouseSource) {
            selected = std::clamp(baseProcessingSelection_, 0, StoragePaneSlotCount - 1);
        } else if (ringSource) {
            const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseProcessingSource_), 0, SpellRingCount - 1);
            const int itemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
            selected = itemCount <= 0 ? 0 : std::clamp(baseProcessingSelection_, 0, itemCount - 1);
        }
        const InventoryUiEntryView detailEntry = entryViewForScreenSlot(selected);
        const ProcessingTarget selectedTarget = processingTargetForScreenSlot(selected);

        const bool selectedRingTarget = selectedTarget.valid && baseItemSourceIsRing(static_cast<int>(selectedTarget.source));
        const SpellRingItem* selectedRingItem = nullptr;
        if (selectedRingTarget &&
            selectedTarget.ringIndex >= 0 &&
            selectedTarget.ringIndex < SpellRingCount) {
            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(selectedTarget.ringIndex);
            if (selectedTarget.ringItemIndex >= 0 && selectedTarget.ringItemIndex < static_cast<int>(ringItems.size())) {
                selectedRingItem = &ringItems[static_cast<std::size_t>(selectedTarget.ringItemIndex)];
            }
        }

        if (!selectedTarget.valid || (detailEntry.item == nullptr && selectedRingItem == nullptr)) {
            drawUiSubPanel(renderer, detailPanel);
            float detailLineY = drawUiDetailHeader(renderer, detailPanel, "アイテム未選択");
            drawUiDetailText(renderer, detailPanel, detailLineY, "加工するアイテムを選択してください。");
        } else {
            if (detailEntry.item != nullptr) {
                drawInventoryUiDetailPanel(
                    renderer,
                    detailPanel,
                    detailEntry,
                    objectCatalog_,
                    encyclopedia_,
                    InventoryUiDetailOptions{.animationSeconds = ringPreviewSeconds});
            } else {
                drawUiSubPanel(renderer, detailPanel);
                float detailLineY = drawUiDetailHeader(renderer, detailPanel, ringItemDisplayName(objectCatalog_, *selectedRingItem));
                drawUiDetailText(renderer, detailPanel, detailLineY, "-");
            }
        }
        const ProcessingTarget commandTarget = baseProcessingCommandSlot_ >= 0
            ? processingTargetForScreenSlot(baseProcessingCommandSlot_)
            : ProcessingTarget{};
        const std::vector<UiCommandMenuItem> processingCommandItems = this->processingCommandItems(commandTarget);
        drawUiCommandMenu(
            renderer,
            baseProcessingCommandMenu_,
            processingCommandItems.data(),
            static_cast<int>(processingCommandItems.size()));
    } else if (baseSellActive_) {
        if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
            renderer.drawText(smallActionInfoTextPos(panel), "何を見ていくんだい？", {198, 198, 206, 255}, 2);
            constexpr std::array<std::string_view, 2> Choices{"買う", "売る"};
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                drawUiButton(renderer, merchantActionChoiceRect(i), Choices[static_cast<std::size_t>(i)], i == baseMerchantActionSelection_, uiActionButtonStyle());
            }
        } else {
            const bool buyMode = baseMerchantMode_ == MerchantUiMode::Buy;
            const auto entryViewForScreenSlot = [this](int slot) {
                InventoryUiEntryView view{};
                if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot)) {
                    view.item = &stack->item;
                    view.stackCount = stack->count;
                    return view;
                }
                if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot)) {
                    view.item = &instance->item;
                    view.instance = &instance->instance;
                    view.stackCount = 1;
                    view.equipped = inventory_.isStaffEquipped(instance->instance.instanceId);
                }
                return view;
            };
            const auto entryViewForSellTarget = [this, &entryViewForScreenSlot](MerchantSellTarget target) {
                if (!target.valid) {
                    return InventoryUiEntryView{};
                }
                if (target.source == BaseItemSource::Backpack) {
                    return entryViewForScreenSlot(target.slotIndex);
                }
                if (target.source == BaseItemSource::Warehouse) {
                    return storageEntryView(target.storageEntry, true);
                }

                InventoryUiEntryView view{};
                if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
                    return view;
                }
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
                if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
                    return view;
                }
                const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
                view.item = objectForRingItem(objectCatalog_, ringItem);
                view.stats = inventoryUiStatsFromRingItem(ringItem);
                view.stackCount = 1;
                return view;
            };
            const auto blockedSellLabel = [this](MerchantSellTarget target) -> std::string_view {
                if (!target.valid) {
                    return {};
                }
                if (target.source == BaseItemSource::Backpack) {
                    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                        if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                            return "装備中";
                        }
                        if (instance->instance.protectionEnabled) {
                            return "保護中";
                        }
                        if (!isSellableObject(instance->item)) {
                            return "売却不可";
                        }
                    }
                    if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
                        if (!isSellableObject(stack->item)) {
                            return "売却不可";
                        }
                    }
                    return {};
                }

                if (target.source == BaseItemSource::Warehouse) {
                    const ItemData* item = storageEntryItem(target.storageEntry, true);
                    if (item == nullptr || !isSellableObject(*item)) {
                        return "売却不可";
                    }
                    if (const ItemInstance* instance = storageEntryInstance(target.storageEntry, true)) {
                        if (instance->protectionEnabled) {
                            return "保護中";
                        }
                    }
                    return {};
                }

                if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
                    return "売却不可";
                }
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
                if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
                    return "売却不可";
                }
                const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
                if (ringItem.protectionEnabled) {
                    return "保護中";
                }
                const ItemData* item = objectForRingItem(objectCatalog_, ringItem);
                return item != nullptr && isSellableObject(*item) ? std::string_view{} : std::string_view{"売却不可"};
            };

            const UiRect detailPanel = merchantDetailPanelRect();
            drawMoneySummaryText(renderer, {detailPanel.pos.x, detailPanel.pos.y + 12.0f}, money_);

            InventoryUiEntryView detailEntry{};
            std::vector<InventoryUiDetailExtraLine> extraLines;
            const SpellRingItem* selectedRingItem = nullptr;
            if (buyMode) {
                if (merchantStock_.empty()) {
                    renderer.drawText({92.0f, 210.0f}, "商品がありません", {198, 198, 206, 255}, 2);
                }
                for (int i = 0; i < static_cast<int>(merchantStock_.size()); ++i) {
                    const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(i)];
                    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
                    InventoryUiEntryView entry{};
                    entry.item = item;
                    entry.stackCount = 1;
                    const bool disabled = !canBuyMerchantProduct(product);
                    std::snprintf(buffer, sizeof(buffer), "%dG", product.price);
                    InventoryUiSlotStyle style{i == baseMerchantBuySelection_, disabled, 48.0f};
                    style.bottomLabel = buffer;
                    style.bottomLabelColor = disabled ? Color{238, 82, 82, 255} : ui::Text;
                    style.showTopRightCount = true;
                    style.topRightCount = product.quantity;
                    drawInventoryUiSlot(renderer, merchantGridSlotRect(i), entry, style);
                }
                if (!merchantStock_.empty()) {
                    const int selected = std::clamp(baseMerchantBuySelection_, 0, static_cast<int>(merchantStock_.size()) - 1);
                    const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(selected)];
                    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
                    detailEntry.item = item;
                    detailEntry.stackCount = 1;
                    std::snprintf(buffer, sizeof(buffer), "%dG", product.price);
                    extraLines.push_back({"価格", buffer, canBuyMerchantProduct(product) ? ui::Text : Color{238, 82, 82, 255}});
                    std::snprintf(buffer, sizeof(buffer), "%d", product.quantity);
                    extraLines.push_back({"在庫", buffer, product.quantity > 0 ? ui::Text : ui::TextDisabled});
                }
            } else {
                const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount());
                std::array<UiTabItem, BaseItemSourceCount> sourceTabs{};
                std::array<UiRect, BaseItemSourceCount> sourceTabRects{};
                for (int i = 0; i < sourceCount; ++i) {
                    const bool enabled = !(roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Merchant &&
                        baseItemSourceIsWarehouse(i));
                    sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(i, enabled);
                    sourceTabRects[static_cast<std::size_t>(i)] = merchantSellSourceRect(i, sourceCount);
                }
                drawUiTabs(
                    renderer,
                    baseMerchantSellSourceTabs_,
                    baseMerchantSellSource_,
                    sourceTabs.data(),
                    sourceCount,
                    sourceTabRects.data());
                drawUiButton(renderer, merchantSellSortButtonRect(), "並び替え", false, uiActionButtonStyle());

                const bool warehouseSource = baseItemSourceIsWarehouse(baseMerchantSellSource_);
                const bool ringSource = baseItemSourceIsRing(baseMerchantSellSource_);
                const auto sellTargetBottomLabel = [this, &blockedSellLabel](
                    MerchantSellTarget target,
                    std::string& outLabel,
                    Color& outColor) {
                    outLabel.clear();
                    if (!target.valid) {
                        return false;
                    }
                    const std::string_view blockedLabel = blockedSellLabel(target);
                    if (!blockedLabel.empty()) {
                        outLabel = std::string(blockedLabel);
                        outColor = ui::TextDisabled;
                        return true;
                    }

                    char priceBuffer[32];
                    std::snprintf(priceBuffer, sizeof(priceBuffer), "%dG", merchantSellTargetPrice(target));
                    outLabel = priceBuffer;
                    outColor = ui::Text;
                    return true;
                };
                const auto applySellTargetBottomLabel = [&sellTargetBottomLabel](
                    InventoryUiSlotStyle& style,
                    MerchantSellTarget target) {
                    std::string label;
                    Color labelColor = ui::Text;
                    if (sellTargetBottomLabel(target, label, labelColor)) {
                        style.bottomLabel = label;
                        style.bottomLabelColor = labelColor;
                    }
                };
                const auto targetHighValue = [this, &entryViewForSellTarget](MerchantSellTarget target) {
                    const InventoryUiEntryView view = entryViewForSellTarget(target);
                    return view.item != nullptr && isHighValueBuyObject(*view.item);
                };
                const auto drawHighValueLabel = [&renderer](UiRect rect) {
                    const std::string label = "高価買取中!";
                    const Vec2 size = renderer.measureText(label, 1, TextStyle::Italic);
                    const Vec2 pos{
                        rect.pos.x + (rect.size.x - size.x) * 0.5f,
                        rect.pos.y + rect.size.y - 40.0f,
                    };
                    renderer.drawOutlinedText(pos, label, Color{255, 64, 64, 255}, Color{44, 0, 0, 210}, 2, 1, TextStyle::Italic);
                };
                if (ringSource) {
                    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseMerchantSellSource_), 0, SpellRingCount - 1);
                    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                    const int selectedRingIndex = ringItems.empty()
                        ? -1
                        : std::clamp(baseSellSelection_, 0, static_cast<int>(ringItems.size()) - 1);
                    drawMerchantSellRingPreview(
                        renderer,
                        spellRing_,
                        objectCatalog_,
                        balance_,
                        ringIndex,
                        selectedRingIndex,
                        ringPreviewSeconds);
                    for (int i = 0; i < static_cast<int>(ringItems.size()); ++i) {
                        const MerchantSellTarget target = merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                        std::string label;
                        Color labelColor = ui::Text;
                        if (!sellTargetBottomLabel(target, label, labelColor)) {
                            continue;
                        }
                        UiRect labelRect = merchantSellRingItemRect(
                            ringItems[static_cast<std::size_t>(i)],
                            spellRing_,
                            balance_,
                            ringIndex,
                            i,
                            static_cast<int>(ringItems.size()),
                            ringPreviewSeconds);
                        labelRect.size.y += MerchantSellRingItemLabelExtraHeight;
                        drawInventoryUiSlotBottomLabel(renderer, labelRect, label, labelColor);
                        if (targetHighValue(target)) {
                            drawHighValueLabel(labelRect);
                        }
                    }
                    if (!ringItems.empty() && selectedRingIndex >= 0) {
                        selectedRingItem = &ringItems[static_cast<std::size_t>(selectedRingIndex)];
                    }
                } else if (warehouseSource) {
                    const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
                    const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                    drawExternalWarehouseSourceHeader(
                        renderer,
                        merchantSellGridSlotRect,
                        warehousePage,
                        warehousePageCount);
                    for (int i = 0; i < StoragePaneSlotCount; ++i) {
                        const MerchantSellTarget target = merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                        const InventoryUiEntryView view = entryViewForSellTarget(target);
                        const std::string_view blockedLabel = blockedSellLabel(target);
                        const bool disabled = view.item != nullptr && !blockedLabel.empty();
                        InventoryUiSlotStyle style{i == baseSellSelection_, disabled, 48.0f};
                        if (view.item != nullptr) {
                            applySellTargetBottomLabel(style, target);
                        }
                        if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                            style.showTopRightCount = true;
                            style.topRightCount = view.stackCount;
                        }
                        drawInventoryUiSlot(renderer, externalWarehouseSourceSlotRect(merchantSellGridSlotRect, i), view, style);
                        if (targetHighValue(target)) {
                            drawHighValueLabel(externalWarehouseSourceSlotRect(merchantSellGridSlotRect, i));
                        }
                    }
                } else {
                    for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                        const MerchantSellTarget target = merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                        const InventoryUiEntryView view = entryViewForSellTarget(target);
                        const std::string_view blockedLabel = blockedSellLabel(target);
                        const bool disabled = view.item != nullptr && !blockedLabel.empty();
                        const UiRect rect = merchantSellGridSlotRect(i);
                        InventoryUiSlotStyle style{i == baseSellSelection_, disabled, 48.0f};
                        if (view.item != nullptr) {
                            applySellTargetBottomLabel(style, target);
                        }
                        if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                            style.showTopRightCount = true;
                            style.topRightCount = view.stackCount;
                        }
                        drawInventoryUiSlot(renderer, rect, view, style);
                        if (targetHighValue(target)) {
                            drawHighValueLabel(rect);
                        }
                    }
                }

                int selected = std::clamp(baseSellSelection_, 0, inventory_.screenSlotCount() - 1);
                if (warehouseSource) {
                    selected = std::clamp(baseSellSelection_, 0, StoragePaneSlotCount - 1);
                } else if (ringSource) {
                    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseMerchantSellSource_), 0, SpellRingCount - 1);
                    const int itemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
                    selected = itemCount <= 0 ? 0 : std::clamp(baseSellSelection_, 0, itemCount - 1);
                }
                const MerchantSellTarget selectedTarget = merchantSellTargetForScreenSlot(selected);
                detailEntry = entryViewForSellTarget(selectedTarget);
                if (selectedTarget.valid) {
                    const std::string_view blockedLabel = blockedSellLabel(selectedTarget);
                    if (!blockedLabel.empty()) {
                        extraLines.push_back({"売値", std::string(blockedLabel), ui::TextDisabled});
                    } else {
                        if (detailEntry.item != nullptr && isHighValueBuyObject(*detailEntry.item)) {
                            extraLines.push_back({"", "高価買取中!", Color{255, 64, 64, 255}});
                        }
                        std::snprintf(buffer, sizeof(buffer), "%dG", merchantSellTargetPrice(selectedTarget));
                        extraLines.push_back({"売値", buffer, ui::Text});
                    }
                }
            }

            if (!buyMode && detailEntry.item == nullptr && selectedRingItem != nullptr) {
                drawUiSubPanel(renderer, detailPanel);
                float detailLineY = drawUiDetailHeader(renderer, detailPanel, ringItemDisplayName(objectCatalog_, *selectedRingItem));
                drawUiDetailText(renderer, detailPanel, detailLineY, "売却できません。");
                drawUiDetailLine(renderer, detailPanel, detailLineY, "売値", "売却不可");
            } else {
                drawInventoryUiDetailPanel(
                    renderer,
                    detailPanel,
                    detailEntry,
                    objectCatalog_,
                    encyclopedia_,
                    InventoryUiDetailOptions{.animationSeconds = ringPreviewSeconds},
                    extraLines);
            }
            if (buyMode) {
                const bool buyCommandEnabled = baseMerchantBuyCommandIndex_ >= 0 &&
                    baseMerchantBuyCommandIndex_ < static_cast<int>(merchantStock_.size()) &&
                    canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(baseMerchantBuyCommandIndex_)]);
                const std::array<UiCommandMenuItem, 1> buyItems{{{"買う", buyCommandEnabled}}};
                drawUiCommandMenu(renderer, baseMerchantBuyCommandMenu_, buyItems.data(), static_cast<int>(buyItems.size()));
            } else {
                const MerchantSellTarget commandTarget = merchantSellTargetForSourceSlot(
                    baseMerchantSellCommandSource_,
                    baseMerchantSellCommandIndex_);
                const bool stackCommand =
                    (commandTarget.source == BaseItemSource::Backpack &&
                        baseMerchantSellCommandIndex_ >= 0 &&
                        inventory_.screenObjectStackAt(baseMerchantSellCommandIndex_) != nullptr) ||
                    (commandTarget.source == BaseItemSource::Warehouse &&
                        commandTarget.storageEntry.kind == StorageEntryKind::Stack);
                const std::array<UiCommandMenuItem, 2> sellItems{{{stackCommand ? "1個売る" : "売る", true}, {"すべて売る", stackCommand}}};
                drawUiCommandMenu(renderer, baseMerchantSellCommandMenu_, sellItems.data(), stackCommand ? 2 : 1);
            }
        }
    } else if (baseUpgradeActive_) {
        const bool roguelikeTrainer = roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Trainer;
        const int upgradeDisplayCount = baseUpgradeDisplayCount(roguelikeTrainer);
        const int displaySelection = baseUpgradeDisplayForIndex(roguelikeTrainer, baseUpgradeSelection_);
        const int selected = baseUpgradeIndexForDisplay(roguelikeTrainer, displaySelection);
        const auto warehouseCapacityForUiLevel = [](int level) {
            constexpr std::array<int, 5> Capacities{{48, 72, 100, 140, 200}};
            const int index = std::clamp(level, 0, static_cast<int>(Capacities.size()) - 1);
            return Capacities[static_cast<std::size_t>(index)];
        };
        const auto merchantStockCountForUiLevel = [](int level) {
            return 6 + std::clamp(level, 0, 6) * 3;
        };
        const auto merchantBuyPriceFeature = [](int level) -> const char* {
            if (level >= 5) {
                return "+20%";
            }
            if (level >= 2) {
                return "+10%";
            }
            return "通常";
        };
        const auto merchantTreasureFeature = [](int level) -> const char* {
            return level >= 3 ? "解禁" : "未解禁";
        };
        const auto merchantRareFeature = [](int level) -> const char* {
            if (level >= 6) {
                return "高レア増加";
            }
            if (level >= 4) {
                return "レア増加";
            }
            return "通常";
        };
        const auto processingUnlockFeature = [](int level) -> const char* {
            if (level >= 3) {
                return "大型化";
            }
            if (level >= 1) {
                return "軽量化";
            }
            return "未解禁";
        };
        const auto processingDiscountFeature = [](int level) -> const char* {
            if (level >= 5) {
                return "-30%";
            }
            if (level >= 4) {
                return "-20%";
            }
            if (level >= 2) {
                return "-10%";
            }
            return "通常";
        };
        const float listLabelX = panel.pos.x + 38.0f;
        if (roguelikeTrainer) {
            renderer.drawText({listLabelX, 128.0f}, "ルネの強化", {198, 198, 206, 255}, 2);
        } else {
            renderer.drawText({listLabelX, 128.0f}, "拠点の強化", {198, 198, 206, 255}, 2);
            renderer.drawText({listLabelX, 350.0f}, "ルネの強化", {198, 198, 206, 255}, 2);
        }
        std::array<UiVerticalTabItem, BaseUpgradeItemCount> upgradeTabs{};
        std::array<UiRect, BaseUpgradeItemCount> upgradeTabRects{};
        std::array<std::string, BaseUpgradeItemCount> upgradeTabValues{};
        for (int i = 0; i < upgradeDisplayCount; ++i) {
            const int upgradeIndex = baseUpgradeIndexForDisplay(roguelikeTrainer, i);
            const bool implemented = upgradeImplemented(upgradeIndex);
            const bool maxed = implemented && upgradeMaxed(upgradeIndex);
            if (!implemented) {
                upgradeTabValues[static_cast<std::size_t>(i)] = "未実装";
            } else if (maxed) {
                upgradeTabValues[static_cast<std::size_t>(i)] = "上限";
            } else {
                std::snprintf(buffer, sizeof(buffer), "Lv.%d/%d", upgradeLevel(upgradeIndex), upgradeMaxLevel(upgradeIndex));
                upgradeTabValues[static_cast<std::size_t>(i)] = buffer;
            }
            upgradeTabs[static_cast<std::size_t>(i)] = {
                upgradeName(upgradeIndex),
                upgradeTabValues[static_cast<std::size_t>(i)],
                implemented,
                maxed ? Color{160, 220, 190, 255} : ui::TextMuted,
            };
            upgradeTabRects[static_cast<std::size_t>(i)] = baseUpgradeItemRect(i);
        }
        UiVerticalTabsStyle upgradeTabStyle;
        upgradeTabStyle.labelScale = 2;
        drawUiVerticalTabs(
            renderer,
            baseUpgradeTabs_,
            displaySelection,
            upgradeTabs.data(),
            upgradeDisplayCount,
            upgradeTabRects.data(),
            upgradeTabStyle);

        const UiRect detailPanel = baseUpgradeDetailPanelRect();
        drawUiSubPanel(renderer, detailPanel);

        const auto drawTextRun = [&renderer](Vec2& pos, std::string_view text, Color color, int scale) {
            renderer.drawText(pos, text, color, scale);
            pos.x += renderer.measureText(text, scale).x;
        };
        InlineItemTextStyle inlineStyle;
        inlineStyle.scale = 2;
        inlineStyle.iconTextGap = 4.0f;
        inlineStyle.iconScale = 1.15f;
        const auto drawInlineTextRun = [&](Vec2& pos, std::string_view text, Color color) {
            inlineStyle.text = color;
            drawInlineItemText(renderer, objectCatalog_, pos, text, inlineStyle);
            pos.x += measureInlineItemText(renderer, text, inlineStyle).x;
        };
        const auto beginDetailRow = [&renderer, detailPanel](float& y, std::string_view label) {
            constexpr float LabelWidth = 96.0f;
            const UiRect content = uiSubPanelContentRect(detailPanel);
            renderer.drawText({content.pos.x, y}, label, ui::TextMuted, 2);
            return Vec2{content.pos.x + LabelWidth, y};
        };
        const auto drawDetailTextRow = [&](float& y, std::string_view label, std::string_view text, Color color) {
            constexpr float LabelWidth = 96.0f;
            constexpr float MinLineHeight = 31.0f;
            constexpr float LineGap = 4.0f;
            const UiRect content = uiSubPanelContentRect(detailPanel);
            const Vec2 pos = beginDetailRow(y, label);
            const float valueMaxWidth = std::max(0.0f, content.size.x - LabelWidth);
            renderer.drawWrappedText(pos, text, valueMaxWidth, color, 2);
            y += std::max(MinLineHeight, renderer.measureWrappedText(text, valueMaxWidth, 2).y + LineGap);
        };
        const auto drawDetailDescription = [&](float& y, std::string_view text) {
            constexpr float LineGap = 8.0f;
            const UiRect content = uiSubPanelContentRect(detailPanel);
            renderer.drawWrappedText(content.pos + Vec2{0.0f, y - content.pos.y}, text, content.size.x, ui::TextMuted, 2);
            y += renderer.measureWrappedText(text, content.size.x, 2).y + LineGap;
        };
        const auto drawMoneyCostLine = [&](float& y, std::string_view label, int cost) {
            Vec2 pos = beginDetailRow(y, label);
            const Color numberColor = money_ >= cost ? ui::Text : Color{238, 82, 82, 255};
            drawInlineTextRun(pos, inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + " ", ui::Text);
            drawTextRun(pos, std::to_string(cost), numberColor, 2);
            drawTextRun(pos, "G", ui::Text, 2);
            y += 31.0f;
        };
        const auto drawMaterialCostLine = [&](float& y, std::string_view label, MaterialType type, int cost) {
            const int owned = inventory_.materialCount(type);
            const bool enough = owned >= cost;
            const Color numberColor = enough ? ui::Text : Color{238, 82, 82, 255};
            Vec2 pos = beginDetailRow(y, label);
            drawInlineTextRun(pos, inlineMaterialIconTag(type) + std::string(materialTypeDisplayName(type)) + " ×", ui::Text);
            drawTextRun(pos, std::to_string(cost), numberColor, 2);
            drawTextRun(pos, "（", ui::TextMuted, 2);
            drawTextRun(pos, std::to_string(owned), numberColor, 2);
            drawTextRun(pos, "）", ui::TextMuted, 2);
            y += 31.0f;
        };
        const auto drawEffectChangeLine = [&](float& y, std::string_view label, std::string_view prefix, std::string_view current, std::string_view next, bool showUnchanged = false) {
            constexpr Color UpgradeValueColor{255, 230, 150, 255};
            const bool changed = current != next;
            if (!changed && !showUnchanged) {
                return;
            }
            Vec2 pos = beginDetailRow(y, label);
            drawTextRun(pos, prefix, ui::Text, 2);
            drawTextRun(pos, current, ui::Text, 2);
            drawTextRun(pos, " → ", ui::TextMuted, 2);
            drawTextRun(pos, next, changed ? UpgradeValueColor : ui::Text, 2);
            y += 31.0f;
        };
        const auto drawEffectTextLine = [&](float& y, std::string_view label, std::string_view prefix, std::string_view text, Color color) {
            Vec2 pos = beginDetailRow(y, label);
            drawTextRun(pos, prefix, ui::Text, 2);
            drawTextRun(pos, text, color, 2);
            y += 31.0f;
        };

        float detailY = drawUiDetailHeader(renderer, detailPanel, upgradeName(selected));
        drawDetailDescription(detailY, baseUpgradeDescription(selected));

        const bool implemented = upgradeImplemented(selected);
        const bool maxed = implemented && upgradeMaxed(selected);
        const MaterialType materialType = upgradeMaterialType(selected);
        const int materialCost = upgradeMaterialCost(selected);
        const int moneyCost = upgradeCost(selected);

        if (implemented && !maxed) {
            drawMoneyCostLine(detailY, "必要素材", moneyCost);
            if (materialCost > 0) {
                drawMaterialCostLine(detailY, "", materialType, materialCost);
            }
        } else {
            drawDetailTextRow(detailY, "必要素材", "なし", ui::Text);
        }

        char currentValue[64];
        char nextValue[64];
        const int level = upgradeLevel(selected);
        const int maxLevel = upgradeMaxLevel(selected);
        const int nextLevel = std::min(maxLevel, level + 1);
        if (maxed) {
            drawDetailTextRow(detailY, "効果", "上限到達済み", ui::TextMuted);
        } else {
            switch (selected) {
            case 0:
                std::snprintf(currentValue, sizeof(currentValue), "%d枠", warehouseCapacityForUiLevel(level));
                std::snprintf(nextValue, sizeof(nextValue), "%d枠", warehouseCapacityForUiLevel(nextLevel));
                drawEffectChangeLine(detailY, "効果", "収納箱容量: ", currentValue, nextValue);
                break;
            case 1:
                std::snprintf(currentValue, sizeof(currentValue), "%d枠", merchantStockCountForUiLevel(level));
                std::snprintf(nextValue, sizeof(nextValue), "%d枠", merchantStockCountForUiLevel(nextLevel));
                drawEffectChangeLine(detailY, "効果", "品揃え: ", currentValue, nextValue);
                drawEffectChangeLine(
                    detailY,
                    "",
                    "買取価格: ",
                    merchantBuyPriceFeature(level),
                    merchantBuyPriceFeature(nextLevel),
                    std::string_view(merchantBuyPriceFeature(level)) != "通常");
                drawEffectChangeLine(
                    detailY,
                    "",
                    "宝の高価買取: ",
                    merchantTreasureFeature(level),
                    merchantTreasureFeature(nextLevel),
                    std::string_view(merchantTreasureFeature(level)) != "未解禁");
                drawEffectChangeLine(
                    detailY,
                    "",
                    "レア商品: ",
                    merchantRareFeature(level),
                    merchantRareFeature(nextLevel),
                    std::string_view(merchantRareFeature(level)) != "通常");
                break;
            case 2:
                drawEffectChangeLine(
                    detailY,
                    "効果",
                    "加工機能: ",
                    processingUnlockFeature(level),
                    processingUnlockFeature(nextLevel),
                    std::string_view(processingUnlockFeature(level)) != "未解禁");
                drawEffectChangeLine(
                    detailY,
                    "",
                    "作業台費用: ",
                    processingDiscountFeature(level),
                    processingDiscountFeature(nextLevel),
                    std::string_view(processingDiscountFeature(level)) != "通常");
                break;
            case 3:
                drawEffectChangeLine(detailY, "効果", "リング工房: ", "未解禁", "解禁");
                break;
            case 4:
                std::snprintf(currentValue, sizeof(currentValue), "+%d", level * 2);
                std::snprintf(nextValue, sizeof(nextValue), "+%d", nextLevel * 2);
                drawEffectChangeLine(detailY, "効果", "最大HP: ", currentValue, nextValue);
                break;
            case 5:
                std::snprintf(
                    currentValue,
                    sizeof(currentValue),
                    "%.2fm",
                    worldDistanceToMeters(balance_.spellRingRadius * (1.0f + static_cast<float>(level) * 0.08f)));
                std::snprintf(
                    nextValue,
                    sizeof(nextValue),
                    "%.2fm",
                    worldDistanceToMeters(balance_.spellRingRadius * (1.0f + static_cast<float>(nextLevel) * 0.08f)));
                drawEffectChangeLine(detailY, "効果", "リング半径: ", currentValue, nextValue);
                break;
            case 6:
                std::snprintf(
                    currentValue,
                    sizeof(currentValue),
                    "%.2fm/s",
                    balance_.spellRingSpeed * (1.0f + static_cast<float>(level) * 0.08f));
                std::snprintf(
                    nextValue,
                    sizeof(nextValue),
                    "%.2fm/s",
                    balance_.spellRingSpeed * (1.0f + static_cast<float>(nextLevel) * 0.08f));
                drawEffectChangeLine(detailY, "効果", "リング速度: ", currentValue, nextValue);
                break;
            case 7:
                std::snprintf(currentValue, sizeof(currentValue), "%.0fpx", effectiveCollectionPullRadius(level));
                std::snprintf(nextValue, sizeof(nextValue), "%.0fpx", effectiveCollectionPullRadius(nextLevel));
                drawEffectChangeLine(detailY, "効果", "吸引半径: ", currentValue, nextValue);
                drawEffectTextLine(detailY, "", "対象: ", "近くのドロップ", ui::TextMuted);
                break;
            case 8:
                drawEffectChangeLine(
                    detailY,
                    "効果",
                    "プリセット枠: ",
                    baseUpgradeRingPresetFeature(level),
                    baseUpgradeRingPresetFeature(nextLevel));
                drawEffectTextLine(detailY, "", "用途: ", "リング編成登録/呼び出し", ui::TextMuted);
                break;
            default:
                break;
            }
        }

        UiButtonStyle confirmStyle = uiActionButtonStyle();
        const char* confirmLabel = "強化する";
        if (!implemented) {
            confirmLabel = "未実装";
            confirmStyle.fill = {20, 24, 38, 190};
            confirmStyle.fillHot = confirmStyle.fill;
            confirmStyle.text = ui::TextDisabled;
        } else if (maxed) {
            confirmLabel = "上限";
            confirmStyle.fill = {26, 42, 62, 204};
            confirmStyle.fillHot = confirmStyle.fill;
            confirmStyle.text = ui::TextMuted;
        }
        drawUiButton(renderer, baseUpgradeConfirmRect(), confirmLabel, false, confirmStyle);
    } else if (baseMiningStartChoiceActive_) {
        const UiRect body = uiBodyRect(panel);
        const float contentLeft = baseMiningContentLeft();
        const float contentRight = baseMiningContentRight();
        const std::string stageName = currentStageDefinition().name.empty()
            ? currentStageId_
            : currentStageDefinition().name;
        const auto retainedStage = dungeonStates_.find(currentStageId_);
        const bool hasRetainedDungeon = retainedStage != dungeonStates_.end() && retainedStage->second.valid;
        int totalWarpPoints = std::max(0, currentStageDefinition().warpPointCount);
        if (hasRetainedDungeon && !retainedStage->second.warpPoints.empty()) {
            totalWarpPoints = static_cast<int>(retainedStage->second.warpPoints.size());
        } else if (!warpPoints_.empty()) {
            totalWarpPoints = static_cast<int>(warpPoints_.size());
        }
        totalWarpPoints = std::max(0, totalWarpPoints);
        const int discoveredWarpPoints = std::clamp(unlockedWarpPointCount_, 0, totalWarpPoints);

        const auto drawCenteredTextInRect = [&](UiRect rect, std::string_view text, Color color, int scale) {
            const Vec2 textSize = renderer.measureText(text, scale);
            renderer.drawText(
                rect.pos + Vec2{
                    std::max(0.0f, (rect.size.x - textSize.x) * 0.5f),
                    std::max(0.0f, (rect.size.y - textSize.y) * 0.5f),
                },
                text,
                color,
                scale);
        };

        const std::vector<StageDefinition> selectableStages = selectableStageDefinitionsForCurrentUnlockState();
        const int selectableStageCount = static_cast<int>(selectableStages.size());
        const bool canSelectDestination = selectableStageCount > 1;
        const UiPageSelectorRects stageSelector = baseMiningStageSelectorRects();
        const std::vector<WarpPoint> selectableWarpPoints = selectableWarpPointsForCurrentStageStart();
        const bool selectedStageRoguelike = stageLooksRoguelike(currentStageDefinition());

        renderer.drawText({contentLeft, body.pos.y}, "行き先", {198, 198, 206, 255}, 2);
        drawUiRectButton(renderer, stageSelector.prev, "<", false);
        drawUiRectButton(renderer, stageSelector.next, ">", false);
        if (!canSelectDestination) {
            renderer.fillRect(stageSelector.prev.pos, stageSelector.prev.size, {0, 0, 0, 118});
            renderer.fillRect(stageSelector.next.pos, stageSelector.next.size, {0, 0, 0, 118});
        }
        const int stageNameScale = renderer.measureText(stageName, 3).x <= stageSelector.text.size.x ? 3 : 2;
        drawCenteredTextInRect(stageSelector.text, stageName, {246, 235, 255, 255}, stageNameScale);
        constexpr float DestinationSeparatorBleedX = 16.0f;
        drawUiSeparator(
            renderer,
            {{
                contentLeft - DestinationSeparatorBleedX,
                body.pos.y + 42.0f,
            }, {
                contentRight - contentLeft + DestinationSeparatorBleedX * 2.0f,
                ui::SeparatorHeight,
            }});

        if (selectedStageRoguelike) {
            drawUiButton(
                renderer,
                baseMiningStartChoiceRect(0),
                "出発",
                baseMiningStartSelection_ == 0,
                uiActionButtonStyle());
        } else {
            renderer.drawText({contentLeft, body.pos.y + 78.0f}, "どこから採掘を開始する？", {198, 198, 206, 255}, 2);
            for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
                const bool disabled = (i == 1 && selectableWarpPoints.empty()) || (i == 2 && !canRegenerateCurrentStage());
                const char* description = "";
                switch (i) {
                case 0:
                    description = "入口から出発";
                    break;
                case 1:
                    description = disabled ? "ワープポイントを解放すると可能" : "解放済み地点から選んで出発";
                    break;
                case 2:
                    description = disabled ? "全ワープ解放とクリア後に可能" : "地形・敵・宝箱・ワープ配置を作り直す";
                    break;
                default:
                    break;
                }

                const UiRect rect = baseMiningStartChoiceRect(i);
                UiButtonStyle buttonStyle = uiActionButtonStyle();
                if (disabled) {
                    buttonStyle.text = ui::TextDisabled;
                    buttonStyle.imageTint = {128, 128, 140, 210};
                    buttonStyle.imageTintHot = buttonStyle.imageTint;
                    buttonStyle.fill = {18, 22, 34, 190};
                    buttonStyle.fillHot = buttonStyle.fill;
                    buttonStyle.outline = {98, 88, 112, 190};
                    buttonStyle.outlineHot = buttonStyle.outline;
                }
                const bool hot = i == baseMiningStartSelection_ && !disabled && !baseWarpPointSelectActive_;
                if (i == 1) {
                    drawUiButton(renderer, rect, "", hot, buttonStyle);
                    InlineItemTextStyle buttonTextStyle;
                    buttonTextStyle.text = buttonStyle.text;
                    buttonTextStyle.scale = 2;
                    buttonTextStyle.iconTextGap = 6.0f;
                    buttonTextStyle.iconScale = 26.0f / std::max(1.0f, renderer.measureText("0", buttonTextStyle.scale).y);
                    const std::string buttonText = inlineWorldIconTag(worldIconKey(WorldIconId::WarpPoint)) + std::string(baseMiningStartChoiceName(i));
                    const Vec2 buttonTextSize = measureInlineItemText(renderer, buttonText, buttonTextStyle);
                    drawInlineItemText(
                        renderer,
                        objectCatalog_,
                        rect.pos + Vec2{
                            std::max(0.0f, (rect.size.x - buttonTextSize.x) * 0.5f),
                            std::max(0.0f, (rect.size.y - buttonTextSize.y) * 0.5f),
                        },
                        buttonText,
                        buttonTextStyle);
                } else {
                    drawUiButton(renderer, rect, baseMiningStartChoiceName(i), hot, buttonStyle);
                }
                const Vec2 descriptionSize = renderer.measureText(description, 2);
                renderer.drawText(
                    rect.pos + Vec2{
                        std::max(0.0f, (rect.size.x - descriptionSize.x) * 0.5f),
                        ui::ButtonHeight + 4.0f,
                    },
                    description,
                    disabled ? Color{150, 150, 160, 255} : Color{198, 198, 206, 255},
                    2);
            }
        }

        drawDungeonStartDetailPanel(
            renderer,
            objectCatalog_,
            enemyCatalog_,
            encyclopedia_,
            baseMiningStartDetailPanelRect(),
            currentStageDefinition(),
            discoveredWarpPoints,
            totalWarpPoints,
            baseRingPreviewAnimationTime_);

        if (!selectedStageRoguelike && baseWarpPointSelectActive_) {
            panelCancelScope.reset();
            panelWindow.reset();

            renderer.fillRect(
                {0.0f, 0.0f},
                {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
                {0, 0, 0, 82});

            const UiRect warpPanel = baseMiningWarpPointSelectRect();
            UiWindowScope warpWindow(
                renderer,
                "base.warp_select",
                warpPanel,
                "ワープポイント選択",
                "F/Enter 出発  Z/X・方向キー 選択  Esc 戻る",
                UiWindowOptions{true, true});

            renderer.drawText(warpPanel.pos + Vec2{48.0f, 82.0f}, "どのワープポイントにする？", {198, 198, 206, 255}, 2);
            if (selectableWarpPoints.empty()) {
                renderer.drawText(warpPanel.pos + Vec2{48.0f, 142.0f}, "解放済みワープポイントがありません", ui::TextDisabled, 2);
            }
            const DungeonLayout* warpPointDepthLayout = &dungeonLayout_;
            if (retainedStage != dungeonStates_.end() && retainedStage->second.valid) {
                warpPointDepthLayout = &retainedStage->second.dungeonLayout;
            }
            const auto warpPointDepthTilePosition = [](const WarpPoint& point) {
                if (lengthSquared(point.position) > 0.0001f) {
                    return point.position / static_cast<float>(balance::TileSize);
                }
                return Vec2{
                    static_cast<float>(point.tilePosition.x),
                    static_cast<float>(point.tilePosition.y),
                };
            };
            const auto warpPointDepthMeters = [&](const WarpPoint& point) {
                return std::max(
                    0,
                    static_cast<int>(std::lround(projectedDungeonRouteDistanceTiles(
                        *warpPointDepthLayout,
                        warpPointDepthTilePosition(point)))));
            };
            const auto formatWarpPointStartLabel = [&](const WarpPoint& point) {
                char labelBuffer[64];
                std::snprintf(
                    labelBuffer,
                    sizeof(labelBuffer),
                    "ワープ%d（%dm）",
                    std::max(0, point.index) + 1,
                    warpPointDepthMeters(point));
                return std::string(labelBuffer);
            };
            for (int i = 0; i < static_cast<int>(selectableWarpPoints.size()); ++i) {
                const WarpPoint& point = selectableWarpPoints[static_cast<std::size_t>(i)];
                const UiRect rect = baseMiningWarpPointSelectChoiceRect(i);
                const bool hot = i == baseWarpPointSelection_;
                UiButtonStyle buttonStyle = uiActionButtonStyle();
                drawUiButton(renderer, rect, "", hot, buttonStyle);

                InlineItemTextStyle pointTextStyle;
                pointTextStyle.text = buttonStyle.text;
                pointTextStyle.scale = 2;
                pointTextStyle.iconTextGap = 6.0f;
                pointTextStyle.iconScale = 24.0f / std::max(1.0f, renderer.measureText("0", pointTextStyle.scale).y);
                const std::string rawPointText =
                    inlineWorldIconTag(worldIconKey(WorldIconId::WarpPoint)) +
                    formatWarpPointStartLabel(point);
                const std::string pointText = fittedInlineItemText(
                    renderer,
                    rawPointText,
                    rect.size.x - 28.0f,
                    pointTextStyle);
                const Vec2 pointTextSize = measureInlineItemText(renderer, pointText, pointTextStyle);
                drawInlineItemText(
                    renderer,
                    objectCatalog_,
                    {
                        rect.pos.x + std::max(0.0f, (rect.size.x - pointTextSize.x) * 0.5f),
                        rect.pos.y + std::max(0.0f, (rect.size.y - pointTextSize.y) * 0.5f),
                    },
                    pointText,
                    pointTextStyle);
            }
        }
    } else {
        const bool modalOpen = baseBrokenRingDepartureConfirm_.open;
        if (!modalOpen && !bottomControlHelpBlocked && !rescueDropActive) {
            const std::string controlHelp = baseExplorationControlHelp(interactionFacility.has_value() ? &*interactionFacility : nullptr);
            drawBaseControlHelp(
                renderer,
                camera_.width(),
                camera_.height(),
                controlHelp);
        }
        if (!baseStatus_.empty()) {
            UiSystemMessageStyle statusStyle;
            statusStyle.fill = {0, 0, 0, 160};
            statusStyle.padding = {20.0f, 3.0f};
            drawUiSystemMessage(renderer, baseStatus_, {300.0f, 612.0f}, statusStyle);
        }
        if (baseBrokenRingDepartureConfirm_.open) {
            drawUiConfirmDialog(
                renderer,
                baseBrokenRingDepartureConfirm_,
                baseBrokenRingDepartureConfirmRect(),
                "base.broken_ring_departure.confirm");
        }
        return;
    }

    if (!baseStatus_.empty()) {
        drawUiSystemMessage(
            renderer,
            baseStatus_,
            baseSystemMessagePos(
                panel,
                baseStorageActive_,
                baseSellActive_,
                baseProcessingUiMode_ == ProcessingUiMode::Enhance ||
                    (baseRingWorkshopActive_ && baseRingWorkshopMode_ != RingWorkshopMode::ChooseAction),
                baseUpgradeActive_));
    }
    if (baseBrokenRingDepartureConfirm_.open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiConfirmDialog(
            renderer,
            baseBrokenRingDepartureConfirm_,
            baseBrokenRingDepartureConfirmRect(),
            "base.broken_ring_departure.confirm");
    }
    if (baseRoguelikeDepartureConfirm_.open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});

        drawRoguelikeDepartureConfirmDialog(renderer, baseRoguelikeDepartureConfirm_);
    }
    if (baseRegenerateConfirm_.open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});

        drawUiConfirmDialog(
            renderer,
            baseRegenerateConfirm_,
            baseMiningRegenerateConfirmRect(),
            "base.mining.regenerate.confirm");
    }
    if (baseProcessingConfirm_.open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});

        drawProcessingConfirmDialog(renderer, baseProcessingConfirmRect());
    }
    if (baseResultDialog_.open) {
        panelCancelScope.reset();
        panelWindow.reset();
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});
        drawUiResultDialog(renderer, baseResultDialog_, baseResultDialogRect(), "base.result");
    }
    if (baseStorageQuantityDialog_.open) {
        panelCancelScope.reset();
        panelWindow.reset();
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});
        drawUiQuantityDialog(renderer, baseStorageQuantityDialog_, storageQuantityDialogRect(), "base.storage.quantity");
    }

}

} // namespace majo
