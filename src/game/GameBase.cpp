#include "game/GameInternal.hpp"

#include "engine/InputHelpGlyph.hpp"
#include "game/CharacterSprite.hpp"
#include "game/EnemyImageRenderer.hpp"
#include "game/ItemSortPolicy.hpp"
#include "game/MenuIconImage.hpp"
#include "game/NpcCharacterVisual.hpp"
#include "game/PlayerEquipmentVisual.hpp"
#include "game/RingDisplayName.hpp"
#include "game/SpellRingItem.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_set>

namespace majo {

namespace {

constexpr std::string_view AudioSeNewItemJingle = "se.item.new.jingle";
constexpr std::string_view AudioSeForgeUpgrade = "se.facility.forge_upgrade";
constexpr std::string_view AudioSeWorkbenchUpgrade = "se.facility.workbench_upgrade";
constexpr std::string_view AudioSeWorkbenchRepair = "se.facility.workbench_repair";
constexpr std::string_view AudioSeRingWorkshopUpgrade = "se.facility.ring_workshop_upgrade";
constexpr std::string_view AudioSeRingWorkshopRespec = "se.facility.ring_workshop_respec";
constexpr std::string_view AudioSeMerchantTransaction = "se.merchant.transaction";
constexpr std::string_view AudioSeDiarySave = "se.facility.diary_save";
constexpr std::string_view BaseFacilityWindowHelpText = "↑/↓ 選択  F/Enter 決定  Esc 戻る";
constexpr float BaseDiaryRecordPanelHeight = 248.0f;
constexpr float BaseDiaryInfoRowHeight = 36.0f;
constexpr std::string_view MiningToolCategory = "\xE6\x8E\x98\xE5\x89\x8A";
constexpr std::string_view RescueShovelObjectId = "item_shovel";
constexpr std::string_view RescueTorchObjectId = "item_torch";
constexpr float BaseMiningRescueDropDurationSeconds = 1.05f;
constexpr float BaseMiningRescueDropEndSeconds = 1.55f;
constexpr std::string_view BaseRandomTalkTriggerPrefix = "base_random_talk:";
constexpr int BookshelfEndingReplayMenuIndex = BookshelfMenuItemCount;
constexpr float BookshelfEndingCommandMinWidth = 240.0f;
constexpr std::string_view BookshelfCodexHelpText = "WASD/矢印 選択  Esc 戻る";
constexpr Vec2 BookshelfRecordCountOffset{32.0f, 86.0f};
constexpr Vec2 BookshelfGridViewportPosition{72.0f, 172.0f};
constexpr float BookshelfGridViewportHeightExtension = 20.0f;
constexpr unsigned char BookshelfUninspectedEnemyImageAlpha = 128;
constexpr float BaseStoryLookaroundSeconds = 0.9f;
constexpr float BaseStoryMinWalkSeconds = 0.18f;
constexpr float BaseReturnSceneWaitSeconds = 1.0f;
constexpr float BaseReturnSceneFirstWalkTilesX = 2.0f;
constexpr float BaseReturnSceneFirstWalkTilesY = -1.0f;
constexpr float BaseReturnSceneFirstWalkSeconds = 1.43f;
constexpr float BaseReturnSceneSecondWalkTilesX = 0.0f;
constexpr float BaseReturnSceneSecondWalkTilesY = -0.5f;
constexpr float BaseReturnSceneSecondWalkSeconds = 0.32f;
constexpr float BaseReturnSceneElderOffsetTilesX = 4.0f;
constexpr float BaseReturnSceneElderOffsetTilesY = 1.0f;
constexpr float BaseReturnSceneMonicaOffsetTilesX = -2.0f;
constexpr float BaseReturnSceneMonicaOffsetTilesY = 2.0f;
constexpr float BaseStoryChicoryFlightSeconds = 2.2f;
constexpr float BaseStoryRingDemoOpenSeconds = 1.15f;
constexpr float BaseStoryRingDemoCloseSeconds = 0.55f;
constexpr std::string_view BaseStoryRingDemoDefaultItemObjectId = "item_apple";
constexpr std::string_view AudioSeRingAppear = "se.ring.appear";
constexpr float BaseFacilityMarkerBobSpeed = 5.2f;
constexpr float BaseFacilityMarkerBobPixels = 4.0f;
constexpr float BaseFacilityMarkerMinTipY = 76.0f;
constexpr float BaseFacilityMarkerStemWidth = 14.0f;
constexpr float BaseFacilityMarkerStemHeight = 34.0f;
constexpr float BaseFacilityMarkerHeadHalfWidth = 26.0f;
constexpr float BaseFacilityMarkerHeadHeight = 32.0f;
constexpr float BaseFacilityMarkerPulseSeconds = 0.75f;
constexpr float BaseFacilityMarkerPulseScale = 0.24f;
constexpr float BaseFacilityMarkerOuterOutlinePixels = 4.0f;
constexpr float BaseFacilityMarkerInnerOutlinePixels = 2.0f;

UiRect baseDiaryPanelRect()
{
    return uiEnsureDecoratedWindowMinSize({{360.0f, 108.0f}, {560.0f, 504.0f}});
}

bool isBaseRandomTalkSpeaker(std::string_view speakerId)
{
    return speakerId == "merchant" || speakerId == "processor";
}

std::string_view processingSuccessAudioCue(bool repair, bool enhancement)
{
    if (repair) {
        return AudioSeWorkbenchRepair;
    }
    return enhancement ? AudioSeWorkbenchUpgrade : std::string_view{};
}

bool isBasePresentationCommand(std::string_view name)
{
    return name == "base_actor_offset" ||
        name == "base_actor_reset" ||
        name == "base_wait" ||
        name == "base_fade" ||
        name == "base_player_place" ||
        name == "base_player_walk" ||
        name == "base_player_lookaround" ||
        name == "base_return_scene" ||
        name == "base_chicory_figure8" ||
        name == "base_ring_demo" ||
        name == "base_facility_marker";
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

float parseStoryCommandFirstFloatFrom(const DialogueCommand& command, std::size_t firstIndex, float fallback)
{
    for (std::size_t index = firstIndex; index < command.args.size(); ++index) {
        if (command.args[index].find('=') != std::string::npos) {
            continue;
        }
        const float value = parseStoryCommandFloat(command, index, fallback);
        if (value != fallback) {
            return value;
        }
    }
    return fallback;
}

int parseStoryCommandNamedInt(const DialogueCommand& command, std::string_view key, int fallback)
{
    const std::string prefix = std::string(key) + "=";
    for (const std::string& arg : command.args) {
        if (arg.rfind(prefix, 0) != 0) {
            continue;
        }
        errno = 0;
        char* end = nullptr;
        const char* valueStart = arg.c_str() + prefix.size();
        const long value = std::strtol(valueStart, &end, 10);
        if (end != valueStart && end != nullptr && *end == '\0' && errno == 0) {
            return static_cast<int>(value);
        }
    }
    return fallback;
}

Vec2 storyTileOffset(float tileX, float tileY)
{
    const float tileSize = static_cast<float>(balance::TileSize);
    return {tileX * tileSize, tileY * tileSize};
}

std::string_view baseReturnSceneMode(const DialogueCommand& command)
{
    return command.args.empty() ? std::string_view("begin") : std::string_view(command.args[0]);
}

bool baseReturnSceneModeIsBegin(std::string_view mode)
{
    return mode == "begin" || mode == "start";
}

bool baseReturnSceneModeIsEnd(std::string_view mode)
{
    return mode == "end" || mode == "finish";
}

bool baseStorySpeakerTurnsPlayer(std::string_view speakerId)
{
    return speakerId == "monica" || speakerId == "elder";
}

bool isBaseReturnSceneBeginStep(const DialogueStep& step)
{
    return step.kind == DialogueStepKind::Command &&
        step.command.name == "base_return_scene" &&
        baseReturnSceneModeIsBegin(baseReturnSceneMode(step.command));
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
    {"アイテムにおいて未発見の「リングに乗せた時の効果」が発動しても、発見したことにならない", "発見したことにならない"},
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
        return "入るたび姿を変える底なしの迷宮\n持ち込み不可で、初期ステータスから奥を目指す";
    }
    return "採掘しながら奥へ進むダンジョン\nワープポイントを見つけると次回以降の出発地点にできる";
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
    drawBaseDetailInlineItemTextRun(renderer, objectCatalog, pos, inlineMaterialIconTag(type) + std::string(materialTypeDisplayName(type)) + " ", ui::Text);
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

std::string_view baseItemSourceDisplayName(int source, int unlockedRingCount)
{
    const int clampedSource = std::clamp(source, 0, BaseItemSourceCount - 1);
    if (clampedSource == BaseBackpackSourceIndex) {
        return "リュック";
    }
    if (clampedSource == BaseWarehouseSourceIndex) {
        return "収納箱";
    }
    return ringDisplayName(clampedSource - BaseRingSourceOffset, unlockedRingCount);
}

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

UiTabItem baseItemSourceTabItem(int source, int unlockedRingCount, bool enabled)
{
    const int clampedSource = std::clamp(source, 0, BaseItemSourceCount - 1);
    return {
        baseItemSourceDisplayName(clampedSource, unlockedRingCount),
        enabled,
        baseItemSourceIconImageNumber(clampedSource),
    };
}

constexpr int StorageDepositSourceCount = 1 + SpellRingCount;
constexpr float MerchantSellSourceYOffset = 44.0f;
constexpr float MerchantSellRingYOffset = MerchantSellSourceYOffset + 40.0f + 40.0f;
constexpr float StorageTransferLayoutYOffset = 24.0f;
constexpr int StorageWithdrawRows = 3;
constexpr int StorageWithdrawSlotCount = StorageColumns * StorageWithdrawRows;
constexpr float MerchantGridY = 170.0f;
constexpr float MerchantSellGridY = 230.0f;
constexpr float ProcessingGridY = 230.0f;
constexpr float StorageTransferGridY = 254.0f;
constexpr float StorageWithdrawGridY = 190.0f;
constexpr float StorageWithdrawBatchModeButtonWidth = 220.0f;
constexpr float BaseRingPreviewScale = 0.9f;
constexpr float BaseProcessingRingYOffset = 64.0f;
constexpr float MerchantSellRingPreviewScale = 0.9f;
constexpr float StorageRingPreviewScale = 1.0f;
constexpr float BaseItemSourceGridYOffset = 44.0f;
constexpr float StoragePageSelectorGridGap = 10.0f;
constexpr float BaseFacilitySpawnGap = 18.0f;
constexpr float BaseMineExitReturnUpOffset = 40.0f;

enum class BaseFacilitySpawnSide {
    Above,
    Below,
};

UiRect merchantGridSlotRect(int index)
{
    return standardInventoryUiGridSlotRect(index, MerchantGridY);
}

UiRect merchantSellGridSlotRect(int index)
{
    return standardInventoryUiGridSlotRect(index, MerchantSellGridY);
}

UiRect baseProcessingGridSlotRect(int index)
{
    return standardInventoryUiGridSlotRect(index, ProcessingGridY);
}

UiRect storageTransferGridSlotRect(int index)
{
    return standardInventoryUiGridSlotRect(index, StorageTransferGridY);
}

UiRect storageWithdrawSlotRect(int index)
{
    return standardInventoryUiGridSlotRect(index, StorageWithdrawGridY);
}

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

bool storyCommandArgIsOff(std::string_view value)
{
    return value == "hide" || value == "off" || value == "clear";
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

float baseFacilityMarkerPulseScale(float markerAge)
{
    if (markerAge < 0.0f || markerAge >= BaseFacilityMarkerPulseSeconds) {
        return 1.0f;
    }
    const float t = markerAge / BaseFacilityMarkerPulseSeconds;
    const float wave = std::sin(t * Pi * 2.0f);
    return 1.0f + wave * wave * (1.0f - t) * BaseFacilityMarkerPulseScale;
}

void fillBaseFacilityMarkerStem(
    Renderer& renderer,
    float centerX,
    float topY,
    float width,
    float height,
    float expand,
    Color color)
{
    renderer.fillRect(
        {centerX - width * 0.5f - expand, topY - expand},
        {width + expand * 2.0f, height + expand * 2.0f},
        color);
}

void fillBaseFacilityMarkerHead(
    Renderer& renderer,
    float centerX,
    float tipY,
    float halfWidth,
    float height,
    float expand,
    Color color)
{
    const Vec2 points[] = {
        {centerX, tipY + expand},
        {centerX - halfWidth - expand, tipY - height - expand},
        {centerX + halfWidth + expand, tipY - height - expand},
    };
    renderer.fillPolygon(points, 3, color);
}

void drawBaseFacilityMarkerArrow(Renderer& renderer, UiRect targetRect, float animationTime, float markerAge)
{
    const float centerX = targetRect.pos.x + targetRect.size.x * 0.5f;
    const float bob = std::sin(animationTime * BaseFacilityMarkerBobSpeed) * BaseFacilityMarkerBobPixels;
    const float tipY = std::max(BaseFacilityMarkerMinTipY, targetRect.pos.y - 10.0f + bob);
    const float pulseScale = baseFacilityMarkerPulseScale(markerAge);
    const float stemWidth = BaseFacilityMarkerStemWidth * pulseScale;
    const float stemHeight = BaseFacilityMarkerStemHeight * pulseScale;
    const float headHalfWidth = BaseFacilityMarkerHeadHalfWidth * pulseScale;
    const float headHeight = BaseFacilityMarkerHeadHeight * pulseScale;
    const float stemTopY = tipY - stemHeight - headHeight * 0.55f;
    const Vec2 shadowOffset{0.0f, 3.0f};

    const Vec2 head[] = {
        {centerX, tipY},
        {centerX - headHalfWidth, tipY - headHeight},
        {centerX + headHalfWidth, tipY - headHeight},
    };
    const Vec2 shadowHead[] = {
        head[0] + shadowOffset,
        head[1] + shadowOffset,
        head[2] + shadowOffset,
    };

    renderer.fillRect(
        {centerX - stemWidth * 0.5f - 2.0f, stemTopY + 3.0f},
        {stemWidth + 4.0f, stemHeight},
        {46, 34, 26, 115});
    renderer.fillPolygon(shadowHead, 3, {46, 34, 26, 115});

    fillBaseFacilityMarkerStem(
        renderer,
        centerX,
        stemTopY,
        stemWidth,
        stemHeight,
        BaseFacilityMarkerOuterOutlinePixels,
        {255, 255, 255, 255});
    fillBaseFacilityMarkerHead(
        renderer,
        centerX,
        tipY,
        headHalfWidth,
        headHeight,
        BaseFacilityMarkerOuterOutlinePixels,
        {255, 255, 255, 255});
    fillBaseFacilityMarkerStem(
        renderer,
        centerX,
        stemTopY,
        stemWidth,
        stemHeight,
        BaseFacilityMarkerInnerOutlinePixels,
        {0, 0, 0, 255});
    fillBaseFacilityMarkerHead(
        renderer,
        centerX,
        tipY,
        headHalfWidth,
        headHeight,
        BaseFacilityMarkerInnerOutlinePixels,
        {0, 0, 0, 255});

    fillBaseFacilityMarkerStem(renderer, centerX, stemTopY, stemWidth, stemHeight, 0.0f, {255, 232, 98, 245});
    renderer.fillPolygon(head, 3, {255, 216, 68, 248});
    renderer.drawLine(head[1], head[0], {128, 84, 36, 180});
    renderer.drawLine(head[2], head[0], {128, 84, 36, 180});
    renderer.drawLine(
        {centerX - stemWidth * 0.35f, stemTopY + 2.0f},
        {centerX + stemWidth * 0.35f, stemTopY + 2.0f},
        {255, 252, 194, 222});
}

void drawBaseFacilityMarkers(
    Renderer& renderer,
    const std::vector<BaseFacility>& facilities,
    BaseArea area,
    bool ringWorkshopUnlocked,
    const std::unordered_map<std::string, float>& markedFacilityIds,
    float animationTime)
{
    if (markedFacilityIds.empty()) {
        return;
    }

    for (const BaseFacility& facility : facilities) {
        const auto marker = markedFacilityIds.find(std::string(facility.facilityId));
        if (marker == markedFacilityIds.end()) {
            continue;
        }
        if (facility.rect.size.x <= 0.0f || facility.rect.size.y <= 0.0f) {
            continue;
        }

        drawBaseFacilityMarkerArrow(
            renderer,
            baseFacilityPointerRect(facility, area, ringWorkshopUnlocked),
            animationTime,
            animationTime - marker->second);
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

            ImageDrawOptions options = artworkImageDrawOptions();
            const bool highlightOutline = showInteractionHints && inInteractionRange && facility.enabled;
            if (highlightOutline) {
                options.outlineColor = hovered ? Color{255, 230, 72, 255} : Color{255, 255, 255, 245};
                options.outlinePx = DungeonInspectableOutlinePx;
            }
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

UiRect storageDepositSourceRect(int tabIndex, int tabCount)
{
    const int count = std::clamp(tabCount, 1, StorageDepositSourceCount);
    const UiRect firstSlot = storageTransferGridSlotRect(0);
    const UiRect lastSlot = storageTransferGridSlotRect(StorageColumns - 1);
    const float left = firstSlot.pos.x;
    const float right = lastSlot.pos.x + lastSlot.size.x;
    const float totalGap = BaseItemSourceTabInnerGap * static_cast<float>(count - 1);
    const float tabWidth = (right - left - totalGap) / static_cast<float>(count);

    UiRect rect = merchantSellSourceRect(tabIndex, count);
    rect.pos.x = left + static_cast<float>(tabIndex) * (tabWidth + BaseItemSourceTabInnerGap);
    rect.pos.y += StorageTransferLayoutYOffset;
    rect.size.x = tabWidth;
    return rect;
}

Vec2 storageTransferCountTextPos()
{
    return {storageItemCircleLeftX(), 116.0f + StorageTransferLayoutYOffset};
}

UiRect baseQuantityDialogRect(Vec2 screenSize)
{
    UiRect panel = uiEnsureDecoratedWindowMinSize({{}, {420.0f, 336.0f}});
    panel.pos = {
        (screenSize.x - panel.size.x) * 0.5f,
        (screenSize.y - panel.size.y) * 0.5f,
    };
    return panel;
}

UiRect storageTransferSortButtonRect()
{
    UiRect rect = uiFooterActionButtonRect(
        merchantPanelRect(),
        {180.0f, ui::ButtonHeight},
        UiFooterActionAlignment::Left);
    rect.pos.x = storageItemCircleLeftX();
    return rect;
}

Vec2 storageWithdrawCountTextPos()
{
    return storageTransferCountTextPos();
}

UiRect storageWithdrawSortButtonRect()
{
    return storageTransferSortButtonRect();
}

UiRect smallActionDialogRect(int choiceCount = 2)
{
    return uiChoiceWindowRect(
        {410.0f, 170.0f},
        460.0f,
        choiceCount,
        BaseFacilityWindowHelpText);
}

UiRect smallActionChoiceRectForDialog(UiRect dialog, int index)
{
    return uiChoiceWindowButtonRect(dialog, index);
}

UiRect smallActionChoiceRect(int index)
{
    return smallActionChoiceRectForDialog(smallActionDialogRect(), index);
}

void drawSmallActionInfoText(
    Renderer& renderer,
    UiRect panel,
    std::string_view title,
    std::string_view text)
{
    drawUiWindowBodyText(
        renderer,
        panel,
        title,
        text,
        smallActionChoiceRectForDialog(panel, 0).pos.y,
        ui::TextMuted);
}

Vec2 smallActionSupplementaryTextPos(UiRect panel)
{
    const UiRect body = uiBodyRect(panel);
    return body.pos + Vec2{8.0f, 106.0f};
}

UiRect storageActionDialogRect()
{
    return smallActionDialogRect(3);
}

UiRect storageBulkDialogRect(int actionCount)
{
    UiRect rect = smallActionDialogRect(actionCount);
    const UiRect screen = baseMapBounds();
    rect.pos.y = screen.pos.y + (screen.size.y - rect.size.y) * 0.5f;
    return rect;
}

UiRect storageActionChoiceRect(int index)
{
    return smallActionChoiceRectForDialog(storageActionDialogRect(), index);
}

UiRect storageBulkChoiceRect(UiRect dialog, int index)
{
    return smallActionChoiceRectForDialog(dialog, index);
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
    return smallActionDialogRect(itemCount);
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
    style.showPowerBadges = false;
    style.scroll.wheelStep = style.slotSize.y + style.slotGap.y;
    style.scroll.scrollbarPaddingX = 2.0f;
    style.scroll.scrollbarPaddingY = 0.0f;
    return style;
}

UiRect bookshelfGridViewport()
{
    UiRect viewport = inventoryUiGridViewport(BookshelfGridViewportPosition, bookshelfGridStyle());
    viewport.size.y += BookshelfGridViewportHeightExtension;
    return viewport;
}

void drawBookshelfUnknownDetail(Renderer& renderer, UiRect panel, std::string_view status)
{
    drawUiSubPanel(renderer, panel);
    (void)drawUiDetailHeader(renderer, panel, status);
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

std::string enemyContactAttackPowerText(const EnemyDefinition& enemy)
{
    return enemy.contactAttackPower > 0
        ? std::to_string(enemy.contactAttackPower)
        : "-";
}

void applyEnemyCodexImageStageStyle(EnemyImageDrawOptions& options, EncyclopediaStage stage)
{
    if (stage == EncyclopediaStage::Complete) {
        return;
    }

    options.tint.a = BookshelfUninspectedEnemyImageAlpha;
    options.outlineColor.a = BookshelfUninspectedEnemyImageAlpha;
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
constexpr float RingWorkshopRadiusSliderStepMeters = 0.01f;

UiSliderState& ringWorkshopRadiusSliderState()
{
    static UiSliderState state;
    return state;
}

UiConfirmDialogState& ringWorkshopRespecConfirmState()
{
    static UiConfirmDialogState state;
    return state;
}

UiSliderSpec ringWorkshopRadiusSliderSpec(float minMeters, float maxMeters)
{
    UiSliderSpec spec;
    spec.minValue = minMeters;
    spec.maxValue = maxMeters > minMeters + 0.001f
        ? maxMeters
        : minMeters + RingWorkshopRadiusSliderStepMeters;
    spec.step = RingWorkshopRadiusSliderStepMeters;
    spec.valueDecimalPlaces = 2;
    spec.valueSuffix = "m";
    return spec;
}

UiSliderStyle ringWorkshopRadiusSliderStyle(bool enabled)
{
    UiSliderStyle style;
    style.trackThickness = 4.0f;
    style.thumbRadius = 8.0f;
    if (!enabled) {
        style.track = {76, 84, 104, 130};
        style.activeTrack = ui::TextDisabled;
        style.thumb = ui::TextDisabled;
        style.thumbOutline = {255, 255, 255, 48};
    }
    return style;
}

UiTabsInput ringWorkshopRingTabsInput(const Input& input, int ringCount)
{
    UiTabsInput tabsInput = makeUiCycleTabsInput(input, ringCount);

    const int directRingFocus = input.shortcutSlotPressed();
    if (directRingFocus >= 0 && directRingFocus < ringCount) {
        tabsInput.directFocusIndex = directRingFocus;
    }
    tabsInput.commit = tabsInput.focusDelta != 0 || tabsInput.directFocusIndex >= 0;
    return tabsInput;
}

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
    const Vec2 size{300.0f, ui::ButtonHeight};
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

UiRect ringWorkshopRadiusSliderRect(const UiScrollAreaLayout& scroll)
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
    case 8: return "リングアイテム数増加";
    default: return "未解禁";
    }
}

const char* ringWorkshopUpgradeDescription(int index)
{
    switch (index) {
    case 0:
        return "リング半径の上限を広げるよ";
    case 1:
        return "リング半径の下限を下げるよ";
    case 2:
        return "リングのアイテムの回転速度をあげるよ";
    case 3:
        return "リングの重量制限を増やすよ";
    case 4:
        return "リングずらしのずらす距離を伸ばすよ";
    case 5:
        return "リング投げの飛距離を伸ばすよ";
    case 6:
        return "リング投げの再使用までの時間を短くするよ";
    case 7:
        return "リングのアイテムが重量制限を超えたときに、回転速度が遅くなるのを軽減するよ";
    case 8:
        return "リングにのせられるアイテム数の上限を増やすよ";
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

UiRect baseBrokenRingDepartureConfirmRect()
{
    return uiEnsureDecoratedWindowMinSize({{410.0f, 230.0f}, {460.0f, 250.0f}});
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
    UiModalNavigationScope navigationScope(panel);
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
        std::vector<UiColoredTextRun> textRuns;
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
        const float textHeight = drawUiWrappedColoredText(renderer, {textX, y}, textRuns, textWidth, TextScale);
        const float lineHeight = std::max(
            renderer.measureText("・", TextScale).y,
            textHeight);
        y += lineHeight + BulletGap;
    };

    drawParagraph("このダンジョンはローグライクダンジョンだよ", ui::Text, ParagraphGap);
    drawParagraph("以下の特殊ルールが設定されています", ui::Text, ParagraphGap);
    y += 3.0f;
    for (const RoguelikeDepartureRuleText& rule : RoguelikeDepartureRules) {
        drawBullet(rule);
    }
    y += 8.0f;
    drawParagraph("出発する？", ui::Text, 0.0f);

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

void drawTextCentered(Renderer& renderer, UiRect rect, std::string_view text, Color color, int scale)
{
    const Vec2 size = renderer.measureText(text, scale);
    renderer.drawText(
        {
            rect.pos.x + (rect.size.x - size.x) * 0.5f,
            rect.pos.y + (rect.size.y - size.y) * 0.5f,
        },
        text,
        color,
        scale);
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

void drawPageSelector(Renderer& renderer, const UiPageSelectorRects& rects, std::string_view text, bool enabled)
{
    drawTextCentered(renderer, rects.text, text, {198, 198, 206, 255}, StoragePageTextScale);
    drawUiArrowButton(renderer, rects.prev, UiArrowDirection::Left, UiArrowButtonVariant::Standard, enabled);
    drawUiArrowButton(renderer, rects.next, UiArrowDirection::Right, UiArrowButtonVariant::Standard, enabled);
}

enum class StoragePageSelectorInputMode {
    CycleShortcutAndArrows,
    ArrowsOnly,
};

bool updateStoragePageSelector(
    UiContext& ui,
    const Input& input,
    const UiPageSelectorRects& rects,
    int& page,
    int pageCount,
    StoragePageSelectorInputMode inputMode)
{
    const int count = std::max(1, pageCount);
    page = std::clamp(page, 0, count - 1);

    int delta = inputMode == StoragePageSelectorInputMode::CycleShortcutAndArrows
        ? uiCycleInputDelta(input, count)
        : 0;
    if (delta == 0 && updateUiArrowButton(
            ui,
            rects.prev,
            UiArrowDirection::Left,
            UiArrowButtonVariant::Standard,
            count > 1)) {
        delta = -1;
    }
    if (delta == 0 && updateUiArrowButton(
            ui,
            rects.next,
            UiArrowDirection::Right,
            UiArrowButtonVariant::Standard,
            count > 1)) {
        delta = 1;
    }
    if (delta == 0) {
        return false;
    }

    const int previousPage = page;
    page = wrapStoragePageIndex(page, delta, count);
    if (page == previousPage) {
        return false;
    }
    ui.emitSound(UiSoundEvent::TabSwitch);
    return true;
}

UiRect merchantSellSortButtonRect()
{
    UiRect rect = uiFooterActionButtonRect(
        merchantPanelRect(),
        {180.0f, ui::ButtonHeight},
        UiFooterActionAlignment::Left);
    rect.pos.x = storageItemCircleLeftX();
    return rect;
}

UiRect batchItemModeButtonRect()
{
    UiRect rect = merchantSellSortButtonRect();
    rect.pos.x += rect.size.x + 12.0f;
    return rect;
}

UiRect batchItemActionButtonRect(int index)
{
    constexpr float ButtonWidth = 150.0f;
    constexpr float ButtonGap = 12.0f;
    UiRect rect = uiFooterActionButtonRect(
        merchantPanelRect(),
        {ButtonWidth, ui::ButtonHeight},
        UiFooterActionAlignment::Left);
    rect.pos.x = storageItemCircleLeftX() + static_cast<float>(index) * (ButtonWidth + ButtonGap);
    return rect;
}

Vec2 batchItemSelectionSummaryPos()
{
    const UiRect actionButton = batchItemActionButtonRect(2);
    return {actionButton.pos.x + actionButton.size.x + 22.0f, actionButton.pos.y + 10.0f};
}

std::string batchItemSelectionCountText(int selectedCount)
{
    return std::to_string(selectedCount) + "個選択中";
}

void openStorageBatchTransferFailureDialog(
    UiResultDialogState& dialog,
    bool withdrawing,
    std::string detail)
{
    std::vector<std::string> lines;
    lines.reserve(detail.empty() ? 1 : 2);
    lines.push_back(withdrawing ? "取り出せなかった" : "しまえなかった");
    if (!detail.empty()) {
        lines.push_back(std::move(detail));
    }
    openUiResultDialog(dialog, {}, std::move(lines));
}

UiRect storageWithdrawBatchModeButtonRect()
{
    UiRect rect = batchItemModeButtonRect();
    rect.size.x = StorageWithdrawBatchModeButtonWidth;
    return rect;
}

UiRect storageWithdrawBatchActionButtonRect(int index)
{
    return batchItemActionButtonRect(index);
}

Vec2 storageWithdrawBatchSelectionSummaryPos()
{
    const UiRect actionButton = storageWithdrawBatchActionButtonRect(2);
    return {actionButton.pos.x + actionButton.size.x + 22.0f, actionButton.pos.y + 10.0f};
}

UiRect batchItemConfirmRect()
{
    return uiEnsureDecoratedWindowMinSize({{410.0f, 230.0f}, {460.0f, 250.0f}});
}

bool batchItemActionPressed(const Input& input)
{
    return input.lastActiveDevice() == InputDeviceKind::Gamepad
        ? input.inventoryPressed()
        : input.addRingPressed();
}

std::string batchItemActionInputTag()
{
    const Input* input = inputHelpContext();
    const InputAction action = input != nullptr && input->lastActiveDevice() == InputDeviceKind::Gamepad
        ? InputAction::OpenInventory
        : InputAction::PutSelectedItemOnRing;
    return inlineInputActionTag(action);
}

std::string batchItemWindowHelpText(
    bool active,
    std::string_view actionLabel,
    std::string_view modeLabel,
    bool arrangeAvailable)
{
    const std::string batchAction = batchItemActionInputTag();
    if (active) {
        return "F/Enter 選択  " + inlineInputActionTag(InputAction::ArrangeItems) +
            " 全選択  " + batchAction + " " + std::string(actionLabel) + "  Esc 全解除/戻る";
    }
    std::string help = "F/Enter 決定  ";
    if (arrangeAvailable) {
        help += inlineInputActionTag(InputAction::ArrangeItems) + " 並び替え  ";
    }
    help += inlineInputActionTag(InputAction::GrabOrPlaceItem) +
        " つかむ/置く  P 保護  ";
    return help + batchAction + " " + std::string(modeLabel) + "  Esc 戻る";
}

std::string withUiCycleHelp(
    std::string help,
    int itemCount,
    std::string_view targetLabel)
{
    if (itemCount <= 1) {
        return help;
    }
    return "Z/X " + std::string(targetLabel) + "  " + help;
}

UiRect baseItemSourceSlotRect(UiRect(*sourceSlotRect)(int), int index)
{
    UiRect rect = sourceSlotRect(index);
    rect.pos.y += BaseItemSourceGridYOffset;
    return rect;
}

UiPageSelectorRects pageSelectorRectsCenteredAboveItemGrid(UiRect firstSlot, UiRect lastSlot)
{
    const float gridCenterX = (firstSlot.pos.x + lastSlot.pos.x + lastSlot.size.x) * 0.5f;
    return uiPageSelectorRectsCentered(
        {gridCenterX, firstSlot.pos.y - StoragePageSelectorGridGap - StoragePageButtonSize * 0.5f},
        StoragePageTextWidth);
}

UiPageSelectorRects baseWarehouseSourcePageSelectorRects(UiRect(*sourceSlotRect)(int))
{
    const UiRect first = baseItemSourceSlotRect(sourceSlotRect, 0);
    const UiRect last = baseItemSourceSlotRect(sourceSlotRect, StorageColumns - 1);
    return pageSelectorRectsCenteredAboveItemGrid(first, last);
}

void drawBaseWarehouseSourcePageSelector(
    Renderer& renderer,
    UiRect(*sourceSlotRect)(int),
    int page,
    int pageCount)
{
    const UiPageSelectorRects pageRects = baseWarehouseSourcePageSelectorRects(sourceSlotRect);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d/%d", page + 1, pageCount);
    drawPageSelector(renderer, pageRects, buffer, pageCount > 1);
}

UiPageSelectorRects storageWithdrawPageSelectorRects()
{
    const UiRect first = storageWithdrawSlotRect(0);
    const UiRect last = storageWithdrawSlotRect(StorageColumns - 1);
    return pageSelectorRectsCenteredAboveItemGrid(first, last);
}

void drawStorageWithdrawHeader(Renderer& renderer, int page, int pageCount)
{
    const UiPageSelectorRects pageRects = storageWithdrawPageSelectorRects();
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d/%d", page + 1, pageCount);
    drawPageSelector(renderer, pageRects, buffer, pageCount > 1);
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

Vec2 baseRingPreviewCenterForRing(Vec2 center, int ringIndex, RingShape shape)
{
    center += ringUiPreviewStyle(ringIndex).centerOffset;
    if (shape == RingShape::Comet) {
        constexpr float CometPreviewYOffset = 120.0f;
        center.y += CometPreviewYOffset;
    }
    return center;
}

Vec2 baseProcessingRingPreviewCenter(const SpellRingSystem& spellRing, int ringIndex)
{
    return baseRingPreviewCenterForRing(
        baseProcessingRingPreviewCenter(),
        ringIndex,
        spellRing.ringShapeForIndex(ringIndex));
}

Vec2 merchantSellRingPreviewCenter(const SpellRingSystem& spellRing, int ringIndex)
{
    return baseRingPreviewCenterForRing(
        merchantSellRingPreviewCenter(),
        ringIndex,
        spellRing.ringShapeForIndex(ringIndex));
}

Vec2 storageRingPreviewCenter(const SpellRingSystem& spellRing, int ringIndex)
{
    return baseRingPreviewCenterForRing(
        storageRingPreviewCenter(),
        ringIndex,
        spellRing.ringShapeForIndex(ringIndex));
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
    context.radius = ringUiShapeRadius(context.shape, ringIndex) * previewScale;
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
    const Vec2 anchor = baseRingPreviewItemAnchor(
        previewCenter,
        item,
        spellRing,
        balance,
        ringIndex,
        itemIndex,
        itemCount,
        previewScale);
    Vec2 outward = normalize(anchor - previewCenter);
    if (lengthSquared(outward) <= 0.0001f) {
        outward = {0.0f, -1.0f};
    }
    const Vec2 center = ringItemActionDrawPosition(
        item,
        baseRingPreviewItemDrawCenter(
            previewCenter,
            item,
            spellRing,
            balance,
            ringIndex,
            itemIndex,
            itemCount,
            previewScale,
            totalSeconds),
        outward);
    return {center - RingItemUiRectSize * 0.5f, RingItemUiRectSize};
}

UiRect baseRingPreviewItemNavigationRect(
    Vec2 previewCenter,
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float previewScale)
{
    const Vec2 center = elevatedDrawPosition(
        baseRingPreviewItemAnchor(
            previewCenter,
            item,
            spellRing,
            balance,
            ringIndex,
            itemIndex,
            itemCount,
            previewScale),
        RingItemBaseAltitude);
    return {center - RingItemUiRectSize * 0.5f, RingItemUiRectSize};
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
        MerchantSellRingPreviewScale,
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
        StorageRingPreviewScale,
        totalSeconds);
}

enum class BaseRingPreviewDisabledMode {
    None,
    Storage,
    MerchantSell,
};

bool baseRingPreviewItemDisabled(
    BaseRingPreviewDisabledMode mode,
    const SpellRingItem& item,
    const ItemData* object)
{
    switch (mode) {
    case BaseRingPreviewDisabledMode::None:
        return false;
    case BaseRingPreviewDisabledMode::Storage:
        return item.objectId.empty();
    case BaseRingPreviewDisabledMode::MerchantSell:
        return item.protectionEnabled || object == nullptr || isImportantItem(*object);
    }
    return false;
}

void drawBaseRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const EncyclopediaSystem& encyclopedia,
    const RuntimeBalance& balance,
    Vec2 center,
    Vec2 weightTextPos,
    int ringIndex,
    int selectedIndex,
    float previewScale,
    float totalSeconds,
    bool showProtectionIcon = true,
    BaseRingPreviewDisabledMode disabledMode = BaseRingPreviewDisabledMode::None)
{
    const std::vector<SpellRingItem>& items = spellRing.itemsForRing(ringIndex);
    const RingShape shape = spellRing.ringShapeForIndex(ringIndex);
    const RingUiPreviewStyle previewStyle = ringUiPreviewStyle(ringIndex);
    drawRingWeightLimitText(renderer, weightTextPos, spellRing, ringIndex);

    RingOrbitContext context = baseRingPreviewOrbitContext(spellRing, balance, ringIndex, 0, static_cast<int>(items.size()), previewScale);
    std::vector<Vec2> orbitPath = getRingPathSamplePoints(center, context, 160);
    for (Vec2& point : orbitPath) {
        point = baseRingPreviewPoint(center, shape, point);
    }
    MagicOrbitDrawOptions orbitOptions{
        shape,
        true,
        false,
        true,
        true,
        ringIndex,
        totalSeconds,
        0.92f,
    };
    orbitOptions.centerDecoration = previewStyle.centerDecoration;
    drawMagicOrbitPath(renderer, orbitPath, center, orbitOptions);

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
        const UiRect itemRect = baseRingPreviewItemRect(
            center,
            item,
            spellRing,
            balance,
            ringIndex,
            i,
            static_cast<int>(items.size()),
            previewScale,
            totalSeconds);
        const UiRect navigationRect = baseRingPreviewItemNavigationRect(
            center,
            item,
            spellRing,
            balance,
            ringIndex,
            i,
            static_cast<int>(items.size()),
            previewScale);
        registerUiNavigationTarget(
            navigationRect,
            UiNavigationRole::Grid,
            i == selectedIndex);
        const bool selected = uiControlVisualState(navigationRect).selected;
        const ItemData* object = objectForRingItem(objectCatalog, item);
        const float contentAlpha = baseRingPreviewItemDisabled(disabledMode, item, object)
            ? InventoryUiDisabledIconAlpha
            : 1.0f;
        if (previewStyle.radialGuides && shape != RingShape::FigureEight) {
            const Color angleLineColor = selected ? Color{255, 230, 150, 120} : Color{94, 102, 128, 85};
            Vec2 tangent = normalize(Vec2{-outward.y, outward.x});
            if (lengthSquared(tangent) <= 0.0001f) {
                tangent = {0.0f, 1.0f};
            }
            constexpr float AngleLineHalfWidthPx = 0.5f;
            renderer.drawLine(center + tangent * AngleLineHalfWidthPx, itemAnchor + tangent * AngleLineHalfWidthPx, angleLineColor);
            renderer.drawLine(center - tangent * AngleLineHalfWidthPx, itemAnchor - tangent * AngleLineHalfWidthPx, angleLineColor);
        }
        {
            UiControlMotionScope motion(renderer, navigationRect, UiControlMotion::PressOnly);
            drawRingItemShape(
                renderer,
                item,
                object,
                itemCenter,
                outward,
                forward,
                totalSeconds,
                selected,
                false,
                false,
                showProtectionIcon,
                contentAlpha,
                &encyclopedia);
            char label[16];
            std::snprintf(label, sizeof(label), "%d", i + 1);
            renderer.drawText(
                itemCenter + Vec2{-5.0f, 22.0f},
                label,
                selected ? Color{255, 230, 150, 255} : Color{174, 182, 198, 255},
                1);
        }
    }
}

void drawBaseProcessingRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const EncyclopediaSystem& encyclopedia,
    const RuntimeBalance& balance,
    int ringIndex,
    int selectedIndex,
    float totalSeconds)
{
    drawBaseRingPreview(
        renderer,
        spellRing,
        objectCatalog,
        encyclopedia,
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
    const EncyclopediaSystem& encyclopedia,
    const RuntimeBalance& balance,
    int ringIndex,
    int selectedIndex,
    float totalSeconds)
{
    drawBaseRingPreview(
        renderer,
        spellRing,
        objectCatalog,
        encyclopedia,
        balance,
        merchantSellRingPreviewCenter(spellRing, ringIndex),
        merchantSellRingPreviewWeightTextPos(),
        ringIndex,
        selectedIndex,
        MerchantSellRingPreviewScale,
        totalSeconds,
        true,
        BaseRingPreviewDisabledMode::MerchantSell);
}

void drawBatchItemSelectionBadge(Renderer& renderer, UiRect rect)
{
    const Vec2 center = rect.pos + Vec2{10.0f, 10.0f};
    renderer.fillCircle(center, 9.0f, {232, 164, 70, 248});
    renderer.drawCircle(center, 10.5f, {255, 232, 166, 248});
    renderer.drawLine(center + Vec2{-4.0f, 0.0f}, center + Vec2{-1.0f, 3.0f}, {30, 22, 26, 255});
    renderer.drawLine(center + Vec2{-1.0f, 3.0f}, center + Vec2{5.0f, -4.0f}, {30, 22, 26, 255});
}

UiButtonStyle batchItemActionButtonStyle(bool enabled)
{
    UiButtonStyle style = uiActionButtonStyle();
    if (!enabled) {
        style.fill = {18, 24, 42, 150};
        style.fillHot = style.fill;
        style.outline = {120, 122, 138, 120};
        style.outlineHot = style.outline;
        style.text = ui::TextDisabled;
    }
    return style;
}

void drawStorageRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const EncyclopediaSystem& encyclopedia,
    const RuntimeBalance& balance,
    int ringIndex,
    int selectedIndex,
    float totalSeconds)
{
    drawBaseRingPreview(
        renderer,
        spellRing,
        objectCatalog,
        encyclopedia,
        balance,
        storageRingPreviewCenter(spellRing, ringIndex),
        storageRingPreviewWeightTextPos(),
        ringIndex,
        selectedIndex,
        StorageRingPreviewScale,
        totalSeconds,
        true,
        BaseRingPreviewDisabledMode::Storage);
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
        std::to_string(required),
        std::to_string(owned),
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
    snapshot.currentDurability = durabilityUnitsToDisplayPoints(entry.instance.currentDurability);
    snapshot.maxDurability = durabilityUnitsToDisplayPoints(entry.instance.maxDurability);
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
    snapshot.currentDurability = durabilityUnitsToDisplayPoints(item.durability);
    snapshot.maxDurability = durabilityUnitsToDisplayPoints(item.maxDurability);
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

struct ConfirmPreviewRow {
    std::string label;
    std::string beforeValue;
    std::string afterValue;
};

std::string formatProcessingInt(int value)
{
    return std::to_string(value);
}

std::string formatProcessingSignedInt(int value)
{
    return "+" + std::to_string(value);
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

void drawConfirmPreviewRow(Renderer& renderer, UiRect content, float& y, const ConfirmPreviewRow& row)
{
    constexpr float ValueX = 184.0f;
    renderer.drawText({content.pos.x, y}, row.label, ui::TextMuted, 2);
    Vec2 valuePos{content.pos.x + ValueX, y};
    drawUiTextRun(renderer, valuePos, row.beforeValue, ui::Text);
    drawUiTextRun(renderer, valuePos, "→", ui::TextMuted);
    drawUiTextRun(renderer, valuePos, row.afterValue, ConfirmAfterValueColor);
    y += 31.0f;
}

UiRect baseActionConfirmBodyRect(UiRect panel)
{
    constexpr float ContentInset = ui::PanelPadding + 12.0f;
    constexpr float BodyTopOffset = -2.0f;
    const float bodyTop = panel.pos.y + ui::HeaderHeight + BodyTopOffset;
    return {{
        panel.pos.x + ContentInset,
        bodyTop,
    }, {
        panel.size.x - ContentInset * 2.0f,
        std::max(0.0f, uiConfirmDialogButtonRect(panel, 0).pos.y - bodyTop - 16.0f),
    }};
}

UiRect baseActionConfirmRequirementRect(
    UiRect panel,
    UiRect body,
    float contentBottom,
    std::size_t requirementCount)
{
    constexpr float RequirementTopPadding = ui::SubPanelPadding.y;
    constexpr float RequirementTitleToRows = 34.0f;
    constexpr float RequirementRowHeight = 31.0f;
    constexpr float RequirementBottomPadding = 18.0f;
    const float materialTop = contentBottom + 7.0f;
    const float requiredRows = static_cast<float>(std::max<std::size_t>(1, requirementCount));
    const float preferredMaterialHeight =
        RequirementTopPadding + RequirementTitleToRows + RequirementRowHeight * requiredRows + RequirementBottomPadding;
    const float buttonTop = uiConfirmDialogButtonRect(panel, 0).pos.y;
    const float materialHeight = std::max(
        86.0f,
        std::min(preferredMaterialHeight, buttonTop - materialTop - 14.0f));
    return {{body.pos.x, materialTop}, {body.size.x, materialHeight}};
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
    lines.push_back(before.name + "を修理したよ");
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
        lines.push_back(before.name + "1個を強化したよ");
    } else {
        lines.push_back(before.name + "を強化したよ");
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
    lines.push_back(before.name + "の強化をリセットしたよ");
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
    const char* verb = lightMode ? "軽量化したよ" : "大型化したよ";
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
    const double totalMultiplier = balance::MerchantSellPriceMultiplier * multiplier;
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
    const double totalMultiplier = balance::MerchantSellPriceMultiplier * multiplier;
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(item.price) * totalMultiplier)));
}

bool Game::isHighValueBuyObject(const ItemData& item) const
{
    if (merchantUpgradeLevel_ < 4 || !isTreasureObject(item)) {
        return false;
    }
    return std::find(highValueBuyObjectIds_.begin(), highValueBuyObjectIds_.end(), item.id) != highValueBuyObjectIds_.end();
}

int Game::merchantProductPurchasableQuantity(const MerchantProduct& product) const
{
    if (product.quantity <= 0 || product.price < 0 ||
        objectCatalog_.registry.findById(product.objectId) == nullptr) {
        return 0;
    }

    const int affordableQuantity = product.price > 0
        ? money_ / product.price
        : product.quantity;
    const int candidateQuantity = std::min(product.quantity, affordableQuantity);
    InventorySystem previewInventory = inventory_;
    int purchasableQuantity = 0;
    while (purchasableQuantity < candidateQuantity &&
        previewInventory.addObjectItem(objectCatalog_, product.objectId)) {
        ++purchasableQuantity;
    }
    return purchasableQuantity;
}

bool Game::canBuyMerchantProduct(const MerchantProduct& product) const
{
    return product.quantity > 0 &&
        product.price >= 0 &&
        money_ >= product.price &&
        inventory_.canAddObjectItem(objectCatalog_, product.objectId);
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
        baseStatus_ = "売却対象がないよ";
        return;
    }

    const SellableEntry entry = sellable[static_cast<std::size_t>(index)];
    if (!entry.sellable) {
        baseStatus_ = entry.blockedReason.empty() ? "売れないよ" : entry.blockedReason;
        return;
    }

    bool sold = false;
    int soldCount = 1;
    if (entry.kind == SellableKind::Stack) {
        const auto& stacks = inventory_.objectStacks();
        if (entry.index < 0 || entry.index >= static_cast<int>(stacks.size())) {
            baseStatus_ = "売却対象がないよ";
            return;
        }
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(entry.index)];
        soldCount = count <= 0 ? stack.count : std::min(count, stack.count);
        sold = inventory_.removeObjectItemCount(stack.objectId, soldCount);
    } else {
        const auto& instances = inventory_.objectInstances();
        if (entry.index < 0 || entry.index >= static_cast<int>(instances.size())) {
            baseStatus_ = "売却対象がないよ";
            return;
        }
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(entry.index)];
        sold = inventory_.removeObjectInstance(instance.instance.instanceId);
    }

    if (sold) {
        money_ += entry.price * std::max(1, soldCount);
        baseStatus_ = "売却したよ";
        playAudioSe(AudioSeMerchantTransaction);
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

int Game::merchantSellTargetQuantity(MerchantSellTarget target) const
{
    if (!target.valid) {
        return 0;
    }
    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            return std::max(0, stack->count);
        }
        return inventory_.screenObjectInstanceAt(target.slotIndex) != nullptr ? 1 : 0;
    }
    if (target.source == BaseItemSource::Warehouse) {
        return target.storageEntry.kind == StorageEntryKind::Stack
            ? std::max(0, storageEntryStackCount(target.storageEntry, true))
            : (storageEntryInstance(target.storageEntry, true) != nullptr ? 1 : 0);
    }
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return 0;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    return target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size()) ? 1 : 0;
}

std::vector<Game::MerchantSellTarget> Game::merchantSellTargetsForSource(int source) const
{
    std::vector<MerchantSellTarget> targets;
    const int clampedSource = std::clamp(source, 0, BaseItemSourceCount - 1);
    const BaseItemSource itemSource = static_cast<BaseItemSource>(clampedSource);
    if (itemSource == BaseItemSource::Backpack) {
        targets.reserve(static_cast<std::size_t>(inventory_.screenSlotCount()));
        for (int slot = 0; slot < inventory_.screenSlotCount(); ++slot) {
            MerchantSellTarget target = merchantSellTargetForSourceSlot(clampedSource, slot);
            if (target.valid) {
                targets.push_back(target);
            }
        }
        return targets;
    }
    if (itemSource == BaseItemSource::Warehouse) {
        const std::vector<StorageEntry> entries = warehouseStorageEntries();
        targets.reserve(entries.size());
        for (const StorageEntry entry : entries) {
            MerchantSellTarget target{};
            target.source = BaseItemSource::Warehouse;
            target.storageEntry = entry;
            target.warehouseEntry = true;
            target.valid = true;
            targets.push_back(target);
        }
        return targets;
    }

    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(clampedSource), 0, SpellRingCount - 1);
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
    targets.reserve(ringItems.size());
    for (int itemIndex = 0; itemIndex < static_cast<int>(ringItems.size()); ++itemIndex) {
        MerchantSellTarget target{};
        target.source = itemSource;
        target.ringIndex = ringIndex;
        target.ringItemIndex = itemIndex;
        target.valid = true;
        targets.push_back(target);
    }
    return targets;
}

std::optional<ItemKey> Game::itemKeyForBaseItemTarget(BaseItemTarget target) const
{
    if (!target.valid) {
        return std::nullopt;
    }

    ItemKey key{};
    if (target.source == BaseItemSource::Backpack) {
        key.container = {ItemContainerKind::Backpack, -1};
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            key.stack = true;
            key.stableId = stack->objectId;
        } else if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            key.stableId = instance->instance.instanceId;
        }
    } else if (target.source == BaseItemSource::Warehouse) {
        key.container = {ItemContainerKind::Warehouse, -1};
        if (target.storageEntry.kind == StorageEntryKind::Stack &&
            target.storageEntry.index >= 0 &&
            target.storageEntry.index < static_cast<int>(warehouseObjectStacks_.size())) {
            key.stack = true;
            key.stableId = warehouseObjectStacks_[static_cast<std::size_t>(target.storageEntry.index)].objectId;
        } else if (target.storageEntry.kind == StorageEntryKind::Instance &&
            target.storageEntry.index >= 0 &&
            target.storageEntry.index < static_cast<int>(warehouseObjectInstances_.size())) {
            key.stableId = warehouseObjectInstances_[static_cast<std::size_t>(target.storageEntry.index)].instance.instanceId;
        }
    } else if (target.ringIndex >= 0 && target.ringIndex < SpellRingCount) {
        key.container = {ItemContainerKind::Ring, target.ringIndex};
        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
        if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size())) {
            const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
            key.stableId = ringItem.instanceId.empty() ? ringItem.objectId : ringItem.instanceId;
            key.fallbackIndex = ringItem.instanceId.empty() ? target.ringItemIndex : -1;
        }
    }
    return key.valid() ? std::optional<ItemKey>{std::move(key)} : std::nullopt;
}

Game::BaseItemTarget Game::baseItemTargetForItemKey(const ItemKey& key) const
{
    if (key.container.kind == ItemContainerKind::Backpack) {
        for (int slot = 0; slot < inventory_.screenSlotCount(); ++slot) {
            if (key.stack) {
                const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot);
                if (stack != nullptr && stack->objectId == key.stableId) {
                    BaseItemTarget target{};
                    target.source = BaseItemSource::Backpack;
                    target.slotIndex = slot;
                    target.valid = true;
                    return target;
                }
            } else {
                const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot);
                if (instance != nullptr && instance->instance.instanceId == key.stableId) {
                    BaseItemTarget target{};
                    target.source = BaseItemSource::Backpack;
                    target.slotIndex = slot;
                    target.valid = true;
                    return target;
                }
            }
        }
        return {};
    }

    if (key.container.kind == ItemContainerKind::Warehouse) {
        if (key.stack) {
            for (int index = 0; index < static_cast<int>(warehouseObjectStacks_.size()); ++index) {
                if (warehouseObjectStacks_[static_cast<std::size_t>(index)].objectId == key.stableId) {
                    BaseItemTarget target{};
                    target.source = BaseItemSource::Warehouse;
                    target.storageEntry = {StorageEntryKind::Stack, index};
                    target.warehouseEntry = true;
                    target.valid = true;
                    return target;
                }
            }
        } else {
            for (int index = 0; index < static_cast<int>(warehouseObjectInstances_.size()); ++index) {
                if (warehouseObjectInstances_[static_cast<std::size_t>(index)].instance.instanceId == key.stableId) {
                    BaseItemTarget target{};
                    target.source = BaseItemSource::Warehouse;
                    target.storageEntry = {StorageEntryKind::Instance, index};
                    target.warehouseEntry = true;
                    target.valid = true;
                    return target;
                }
            }
        }
        return {};
    }

    if (key.container.kind != ItemContainerKind::Ring) {
        return {};
    }
    const int ringIndex = key.container.index;
    if (ringIndex < 0 || ringIndex >= SpellRingCount) {
        return {};
    }
    const BaseItemSource source = static_cast<BaseItemSource>(
        BaseWarehouseSourceIndex + 1 + ringIndex);
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
    if (key.fallbackIndex >= 0) {
        if (key.fallbackIndex >= static_cast<int>(ringItems.size()) ||
            ringItems[static_cast<std::size_t>(key.fallbackIndex)].objectId != key.stableId) {
            return {};
        }
        BaseItemTarget target{};
        target.source = source;
        target.ringIndex = ringIndex;
        target.ringItemIndex = key.fallbackIndex;
        target.valid = true;
        return target;
    }
    for (int itemIndex = 0; itemIndex < static_cast<int>(ringItems.size()); ++itemIndex) {
        if (ringItems[static_cast<std::size_t>(itemIndex)].instanceId == key.stableId) {
            BaseItemTarget target{};
            target.source = source;
            target.ringIndex = ringIndex;
            target.ringItemIndex = itemIndex;
            target.valid = true;
            return target;
        }
    }
    return {};
}

std::optional<ItemKey> Game::itemKeyForProcessingTarget(ProcessingTarget target) const
{
    BaseItemTarget baseTarget{};
    baseTarget.source = target.source;
    baseTarget.slotIndex = target.slotIndex;
    baseTarget.storageEntry = target.backpackEntry;
    baseTarget.warehouseEntry = target.warehouseEntry;
    baseTarget.ringIndex = target.ringIndex;
    baseTarget.ringItemIndex = target.ringItemIndex;
    baseTarget.valid = target.valid;
    return itemKeyForBaseItemTarget(baseTarget);
}

bool Game::moveItemKeyToGridPlacement(const ItemKey& key, int placement)
{
    if (!key.valid() || placement < 0) {
        return false;
    }
    if (key.container.kind == ItemContainerKind::Backpack) {
        return key.stack
            ? inventory_.moveObjectStackToScreenSlot(key.stableId, placement)
            : inventory_.moveObjectInstanceToScreenSlot(key.stableId, placement);
    }
    if (key.container.kind != ItemContainerKind::Warehouse) {
        return false;
    }

    const BaseItemTarget target = baseItemTargetForItemKey(key);
    if (!target.valid || target.source != BaseItemSource::Warehouse) {
        return false;
    }
    const int entryIndex = target.storageEntry.kind == StorageEntryKind::Stack
        ? target.storageEntry.index
        : static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index;
    syncWarehouseDisplaySlots();
    if (!warehouseItemLayout_.moveEntryToSlot(entryIndex, placement, warehouseCapacity())) {
        return false;
    }
    syncWarehouseDisplaySlots();
    return true;
}

std::optional<bool> Game::itemProtectionEnabled(const ItemKey& key) const
{
    if (!key.valid() || key.stack) {
        return std::nullopt;
    }
    if (key.container.kind == ItemContainerKind::Backpack) {
        return inventory_.objectInstanceProtectionEnabled(key.stableId);
    }

    const BaseItemTarget target = baseItemTargetForItemKey(key);
    if (!target.valid) {
        return std::nullopt;
    }
    if (key.container.kind == ItemContainerKind::Warehouse &&
        target.storageEntry.kind == StorageEntryKind::Instance &&
        target.storageEntry.index >= 0 &&
        target.storageEntry.index < static_cast<int>(warehouseObjectInstances_.size())) {
        return warehouseObjectInstances_[static_cast<std::size_t>(target.storageEntry.index)]
            .instance.protectionEnabled;
    }
    if (key.container.kind == ItemContainerKind::Ring &&
        target.ringIndex >= 0 &&
        target.ringIndex < SpellRingCount) {
        const std::vector<SpellRingItem>& items = spellRing_.itemsForRing(target.ringIndex);
        if (target.ringItemIndex >= 0 &&
            target.ringItemIndex < static_cast<int>(items.size()) &&
            !items[static_cast<std::size_t>(target.ringItemIndex)].instanceId.empty()) {
            return items[static_cast<std::size_t>(target.ringItemIndex)].protectionEnabled;
        }
    }
    return std::nullopt;
}

ItemProtectionToggleResult Game::toggleItemProtection(const ItemKey& key)
{
    if (!key.valid()) {
        return ItemProtectionToggleResult::Missing;
    }
    if (key.stack) {
        return ItemProtectionToggleResult::Unsupported;
    }

    if (key.container.kind == ItemContainerKind::Backpack) {
        const std::optional<bool> protectedNow =
            inventory_.objectInstanceProtectionEnabled(key.stableId);
        if (!protectedNow) {
            return ItemProtectionToggleResult::Missing;
        }
        return inventory_.setObjectInstanceProtection(key.stableId, !*protectedNow)
            ? ItemProtectionToggleResult::Changed
            : ItemProtectionToggleResult::Missing;
    }

    const BaseItemTarget target = baseItemTargetForItemKey(key);
    if (!target.valid) {
        return ItemProtectionToggleResult::Missing;
    }
    if (key.container.kind == ItemContainerKind::Warehouse) {
        if (target.storageEntry.kind != StorageEntryKind::Instance ||
            target.storageEntry.index < 0 ||
            target.storageEntry.index >= static_cast<int>(warehouseObjectInstances_.size())) {
            return ItemProtectionToggleResult::Missing;
        }
        ItemInstance& instance =
            warehouseObjectInstances_[static_cast<std::size_t>(target.storageEntry.index)].instance;
        instance.protectionEnabled = !instance.protectionEnabled;
        return ItemProtectionToggleResult::Changed;
    }
    if (key.container.kind == ItemContainerKind::Ring &&
        target.ringIndex >= 0 &&
        target.ringIndex < SpellRingCount) {
        std::vector<SpellRingItem>& items = spellRing_.itemsForRing(target.ringIndex);
        if (target.ringItemIndex < 0 ||
            target.ringItemIndex >= static_cast<int>(items.size()) ||
            items[static_cast<std::size_t>(target.ringItemIndex)].instanceId.empty()) {
            return ItemProtectionToggleResult::Unsupported;
        }
        SpellRingItem& item = items[static_cast<std::size_t>(target.ringItemIndex)];
        item.protectionEnabled = !item.protectionEnabled;
        return ItemProtectionToggleResult::Changed;
    }
    return ItemProtectionToggleResult::Missing;
}

bool Game::sortBaseItemSource(int source)
{
    if (source == BaseBackpackSourceIndex) {
        const bool sorted = inventory_.sortByItemOrder(objectCatalog_);
        baseStatus_ = sorted ? "リュックを並び替えました" : "リュックは空だよ";
        return sorted;
    }
    if (baseItemSourceIsWarehouse(source)) {
        const bool hasItems = warehouseUsedSlots() > 0;
        sortWarehouseByItemOrder();
        return hasItems;
    }
    if (!baseItemSourceIsRing(source)) {
        baseStatus_ = "並び替えるアイテムがないよ";
        return false;
    }

    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(source), 0, SpellRingCount - 1);
    std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
    if (ringItems.empty()) {
        baseStatus_ = "リングは空だよ";
        return false;
    }
    spellRing_.arrangeItemsEvenlyForRing(ringIndex, balance_);
    baseStatus_ = "リングを等間隔に整列したよ";
    return true;
}

bool Game::cancelBaseRingItemInteraction(bool restoreOriginalAngle)
{
    if (!baseRingItemInteraction_.active()) {
        return false;
    }
    if (restoreOriginalAngle) {
        const BaseItemTarget target = baseItemTargetForItemKey(baseRingItemInteraction_.item);
        if (target.valid &&
            target.ringIndex >= 0 &&
            target.ringIndex < SpellRingCount) {
            std::vector<SpellRingItem>& items = spellRing_.itemsForRing(target.ringIndex);
            if (target.ringItemIndex >= 0 &&
                target.ringItemIndex < static_cast<int>(items.size())) {
                items[static_cast<std::size_t>(target.ringItemIndex)].localAngle =
                    baseRingItemInteraction_.originalAngle;
            }
        }
    }
    baseRingItemInteraction_ = {};
    return true;
}

void Game::clearBaseItemInteractions()
{
    baseItemInteraction_.clear();
    (void)cancelBaseRingItemInteraction(true);
}

Game::BaseRingInteractionResult Game::updateBaseRingItemInteraction(
    const Input& input,
    UiContext& ui,
    int source,
    int& selection,
    BaseRingPreviewKind previewKind,
    float animationSeconds,
    BaseRingInteractionMode mode)
{
    BaseRingInteractionResult result{};
    if (!baseItemSourceIsRing(source)) {
        (void)cancelBaseRingItemInteraction(true);
        return result;
    }

    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(source), 0, SpellRingCount - 1);
    std::vector<SpellRingItem>& items = spellRing_.itemsForRing(ringIndex);
    const int itemCount = static_cast<int>(items.size());
    if (itemCount <= 0) {
        selection = 0;
        (void)cancelBaseRingItemInteraction(true);
        if ((mode == BaseRingInteractionMode::Manage &&
                (input.grabOrPlacePressed() || input.pressed(InputAction::ToggleProtection))) ||
            input.confirmPressed() ||
            input.useItemPressed()) {
            baseStatus_ = "アイテム未選択";
            ui.rejectAction();
            result.consumed = true;
        }
        return result;
    }

    const auto targetAt = [source, ringIndex](int index) {
        BaseItemTarget target{};
        target.source = static_cast<BaseItemSource>(source);
        target.ringIndex = ringIndex;
        target.ringItemIndex = index;
        target.valid = index >= 0;
        return target;
    };
    const auto previewCenterAndScale = [this, previewKind, ringIndex]() {
        switch (previewKind) {
        case BaseRingPreviewKind::Storage:
            return std::pair{storageRingPreviewCenter(spellRing_, ringIndex), StorageRingPreviewScale};
        case BaseRingPreviewKind::Processing:
            return std::pair{baseProcessingRingPreviewCenter(spellRing_, ringIndex), BaseRingPreviewScale};
        case BaseRingPreviewKind::Merchant:
            return std::pair{merchantSellRingPreviewCenter(spellRing_, ringIndex), MerchantSellRingPreviewScale};
        }
        return std::pair{Vec2{}, 1.0f};
    };
    const auto rectAt = [this, ringIndex, itemCount, animationSeconds, &previewCenterAndScale](
                            const SpellRingItem& item,
                            int index) {
        const auto [center, previewScale] = previewCenterAndScale();
        return baseRingPreviewItemRect(
            center,
            item,
            spellRing_,
            balance_,
            ringIndex,
            index,
            itemCount,
            previewScale,
            animationSeconds);
    };
    const auto navigationRectAt = [this, ringIndex, itemCount, &previewCenterAndScale](
                                      const SpellRingItem& item,
                                      int index) {
        const auto [center, previewScale] = previewCenterAndScale();
        return baseRingPreviewItemNavigationRect(
            center,
            item,
            spellRing_,
            balance_,
            ringIndex,
            index,
            itemCount,
            previewScale);
    };
    const auto pointerAngle = [this, ringIndex, itemCount, &previewCenterAndScale](Vec2 point) {
        const auto [center, previewScale] = previewCenterAndScale();
        const RingShape shape = spellRing_.ringShapeForIndex(ringIndex);
        if (shape == RingShape::Comet) {
            point = rotateAround(point, center, -RingUiCometArcRotation);
        }
        const RingOrbitContext context = baseRingPreviewOrbitContext(
            spellRing_,
            balance_,
            ringIndex,
            0,
            itemCount,
            previewScale);
        return findNearestRingPathParam(point, center, context, 320);
    };
    const auto resetMoveState = [this]() {
        baseRingItemInteraction_ = {};
    };

    if (baseRingItemInteraction_.active() &&
        baseRingItemInteraction_.item.container != ItemContainerId{ItemContainerKind::Ring, ringIndex}) {
        (void)cancelBaseRingItemInteraction(true);
    }
    if (mode == BaseRingInteractionMode::ActivateOnly && baseRingItemInteraction_.active()) {
        (void)cancelBaseRingItemInteraction(true);
    }

    if (baseRingItemInteraction_.keyboardMoveActive) {
        const BaseItemTarget target = baseItemTargetForItemKey(baseRingItemInteraction_.item);
        if (!target.valid ||
            target.ringIndex != ringIndex ||
            target.ringItemIndex < 0 ||
            target.ringItemIndex >= itemCount) {
            resetMoveState();
            result.consumed = true;
            return result;
        }
        selection = target.ringItemIndex;
        if (input.grabOrPlacePressed() || input.confirmPressed() || input.useItemPressed()) {
            resetMoveState();
            baseStatus_ = "位置を確定したよ";
            ui.emitSound(UiSoundEvent::Confirm);
            result.consumed = true;
            return result;
        }

        Vec2 direction{
            (input.pressed(InputAction::MoveRight) ? 1.0f : 0.0f) -
                (input.pressed(InputAction::MoveLeft) ? 1.0f : 0.0f),
            (input.pressed(InputAction::MoveDown) ? 1.0f : 0.0f) -
                (input.pressed(InputAction::MoveUp) ? 1.0f : 0.0f),
        };
        if (lengthSquared(direction) > 1.0f) {
            direction = normalize(direction);
        }
        if (lengthSquared(direction) > 0.0001f) {
            const SpellRingItem& item = items[static_cast<std::size_t>(selection)];
            const auto [center, previewScale] = previewCenterAndScale();
            const Vec2 current = baseRingPreviewItemAnchor(
                center,
                item,
                spellRing_,
                balance_,
                ringIndex,
                selection,
                itemCount,
                previewScale);
            const Vec2 unitDirection = normalize(direction);
            const auto score = [&](float angle) {
                SpellRingItem candidate = item;
                candidate.localAngle = angle;
                const Vec2 next = baseRingPreviewItemAnchor(
                    center,
                    candidate,
                    spellRing_,
                    balance_,
                    ringIndex,
                    selection,
                    itemCount,
                    previewScale);
                const Vec2 offset = next - current;
                if (lengthSquared(offset) <= 0.0001f) {
                    return -std::numeric_limits<float>::max();
                }
                const Vec2 unitOffset = normalize(offset);
                return unitOffset.x * unitDirection.x +
                    unitOffset.y * unitDirection.y;
            };
            const int sign = score(item.localAngle + RingAngleStep) >=
                    score(item.localAngle - RingAngleStep)
                ? 1
                : -1;
            if (spellRing_.moveItemAngleForRing(
                    ringIndex,
                    selection,
                    RingAngleStep * static_cast<float>(sign))) {
                baseStatus_.clear();
                ui.emitSound(UiSoundEvent::ItemMove);
            } else {
                baseStatus_ = "その位置には移動できないよ";
                ui.rejectAction();
            }
        }
        result.consumed = true;
        return result;
    }

    if (baseRingItemInteraction_.pointerPending ||
        baseRingItemInteraction_.pointerDragging) {
        const BaseItemTarget target = baseItemTargetForItemKey(baseRingItemInteraction_.item);
        if (!target.valid ||
            target.ringIndex != ringIndex ||
            target.ringItemIndex < 0 ||
            target.ringItemIndex >= itemCount) {
            resetMoveState();
            result.consumed = true;
            return result;
        }
        selection = target.ringItemIndex;
        if (input.mouseLeftHeld()) {
            if (baseRingItemInteraction_.pointerPending &&
                lengthSquared(input.mouseScreen() - baseRingItemInteraction_.pointerStart) >=
                    StorageDragStartDistanceSq) {
                baseRingItemInteraction_.pointerPending = false;
                baseRingItemInteraction_.pointerDragging = true;
            }
            if (baseRingItemInteraction_.pointerDragging) {
                const std::optional<float> snapAngle = spellRing_.nearestPlaceableAngleForRing(
                    ringIndex,
                    selection,
                    pointerAngle(input.mouseScreen()),
                    RingDragSnapMaxDelta);
                if (snapAngle) {
                    items[static_cast<std::size_t>(selection)].localAngle = *snapAngle;
                }
            }
        }
        if (input.mouseLeftReleased()) {
            if (baseRingItemInteraction_.pointerDragging) {
                baseStatus_ = "位置を確定したよ";
                ui.emitSound(UiSoundEvent::ItemMove);
            } else {
                result.activateIndex = selection;
            }
            resetMoveState();
            result.consumed = true;
            return result;
        }
        result.consumed = true;
        return result;
    }

    selection = std::clamp(selection, 0, itemCount - 1);
    const auto moveSelection = [&](int delta) {
        if (delta == 0) {
            return;
        }
        selection = (selection + delta) % itemCount;
        if (selection < 0) {
            selection += itemCount;
        }
        ui.setNavigationFocus(navigationRectAt(items[static_cast<std::size_t>(selection)], selection));
    };
    moveSelection(input.shortcutCursorDelta());

    int pointerPressed = -1;
    for (int index = 0; index < itemCount; ++index) {
        const SpellRingItem& item = items[static_cast<std::size_t>(index)];
        const UiRect hitRect = rectAt(item, index);
        const UiRect navigationRect = navigationRectAt(item, index);
        if (ui.selectionFocused(hitRect, navigationRect)) {
            selection = index;
        }
        if (ui.pressed(hitRect, navigationRect) && !ui.navigationActive()) {
            pointerPressed = index;
        }
    }

    if (pointerPressed >= 0) {
        selection = pointerPressed;
        if (mode == BaseRingInteractionMode::ActivateOnly) {
            result.activateIndex = selection;
            result.consumed = true;
            return result;
        }
        const std::optional<ItemKey> key = itemKeyForBaseItemTarget(targetAt(pointerPressed));
        if (key) {
            baseRingItemInteraction_.item = *key;
            baseRingItemInteraction_.originalAngle =
                items[static_cast<std::size_t>(pointerPressed)].localAngle;
            baseRingItemInteraction_.pointerStart = input.mouseScreen();
            baseRingItemInteraction_.pointerPending = true;
            ui.consumePointer();
            result.consumed = true;
            return result;
        }
    }

    if (mode == BaseRingInteractionMode::Manage && input.grabOrPlacePressed()) {
        const std::optional<ItemKey> key = itemKeyForBaseItemTarget(targetAt(selection));
        if (!key) {
            baseStatus_ = "アイテム未選択";
            ui.rejectAction();
        } else {
            baseRingItemInteraction_.item = *key;
            baseRingItemInteraction_.originalAngle =
                items[static_cast<std::size_t>(selection)].localAngle;
            baseRingItemInteraction_.keyboardMoveActive = true;
            baseStatus_ = "位置を移動中だよ";
            ui.emitSound(UiSoundEvent::ItemMove);
        }
        result.consumed = true;
        return result;
    }

    if (mode == BaseRingInteractionMode::Manage && input.pressed(InputAction::ToggleProtection)) {
        const std::optional<ItemKey> key = itemKeyForBaseItemTarget(targetAt(selection));
        const ItemProtectionToggleResult toggleResult =
            key ? toggleItemProtection(*key) : ItemProtectionToggleResult::Missing;
        if (toggleResult == ItemProtectionToggleResult::Changed && key) {
            const bool enabled = itemProtectionEnabled(*key).value_or(false);
            baseStatus_ = enabled ? "アイテムを保護したよ" : "アイテムの保護を解除したよ";
            ui.emitSound(UiSoundEvent::Confirm);
        } else {
            baseStatus_ = toggleResult == ItemProtectionToggleResult::Unsupported
                ? "このアイテムは保護できないよ"
                : "アイテム未選択";
            ui.rejectAction();
        }
        result.consumed = true;
        return result;
    }

    if (input.confirmPressed() || input.useItemPressed()) {
        result.activateIndex = selection;
        result.consumed = true;
    }
    return result;
}

bool Game::batchItemKeySelected(
    const BatchItemSelectionState& state,
    const ItemKey& key) const
{
    return std::find(state.selectedKeys.begin(), state.selectedKeys.end(), key) !=
        state.selectedKeys.end();
}

bool Game::batchItemSelectionTargetSelected(
    const BatchItemSelectionState& state,
    BaseItemTarget target) const
{
    const std::optional<ItemKey> key = itemKeyForBaseItemTarget(target);
    return key && batchItemKeySelected(state, *key);
}

bool Game::toggleBatchItemSelectionTarget(BatchItemSelectionState& state, BaseItemTarget target)
{
    const std::optional<ItemKey> key = itemKeyForBaseItemTarget(target);
    if (!key) {
        return false;
    }
    const auto it = std::find(state.selectedKeys.begin(), state.selectedKeys.end(), *key);
    if (it != state.selectedKeys.end()) {
        state.selectedKeys.erase(it);
    } else {
        state.selectedKeys.push_back(*key);
    }
    return true;
}

bool Game::addBatchItemSelectionTarget(BatchItemSelectionState& state, BaseItemTarget target)
{
    const std::optional<ItemKey> key = itemKeyForBaseItemTarget(target);
    if (!key || batchItemKeySelected(state, *key)) {
        return false;
    }
    state.selectedKeys.push_back(*key);
    return true;
}

void Game::clearBatchItemSelectionState(BatchItemSelectionState& state)
{
    state = {};
}

bool Game::merchantBulkSellTargetSelected(MerchantSellTarget target) const
{
    return batchItemSelectionTargetSelected(baseMerchantBulkSell_, target);
}

bool Game::toggleMerchantBulkSellTarget(MerchantSellTarget target)
{
    return merchantSellTargetAvailable(target) &&
        toggleBatchItemSelectionTarget(baseMerchantBulkSell_, target);
}

void Game::selectAllMerchantBulkSellTargets(int source)
{
    for (const MerchantSellTarget target : merchantSellTargetsForSource(source)) {
        if (!merchantSellTargetAvailable(target)) {
            continue;
        }
        addBatchItemSelectionTarget(baseMerchantBulkSell_, target);
    }
}

void Game::pruneMerchantBulkSellSelection()
{
    baseMerchantBulkSell_.selectedKeys.erase(
        std::remove_if(
            baseMerchantBulkSell_.selectedKeys.begin(),
            baseMerchantBulkSell_.selectedKeys.end(),
            [this](const ItemKey& key) {
                return !merchantSellTargetAvailable(baseItemTargetForItemKey(key));
            }),
        baseMerchantBulkSell_.selectedKeys.end());
}

Game::MerchantBulkSellSummary Game::merchantBulkSellSummary() const
{
    MerchantBulkSellSummary summary{};
    for (const ItemKey& key : baseMerchantBulkSell_.selectedKeys) {
        const MerchantSellTarget target = baseItemTargetForItemKey(key);
        if (!merchantSellTargetAvailable(target)) {
            continue;
        }
        const int quantity = merchantSellTargetQuantity(target);
        summary.itemCount += quantity;
        summary.totalPrice += merchantSellTargetPrice(target) * quantity;
    }
    return summary;
}

bool Game::sellMerchantBulkSelection()
{
    pruneMerchantBulkSellSelection();
    const MerchantBulkSellSummary summary = merchantBulkSellSummary();
    if (summary.itemCount <= 0) {
        baseStatus_ = "売却するアイテムが選択されていないよ";
        return false;
    }

    std::vector<MerchantSellTarget> targets;
    targets.reserve(baseMerchantBulkSell_.selectedKeys.size());
    for (const ItemKey& key : baseMerchantBulkSell_.selectedKeys) {
        const MerchantSellTarget target = baseItemTargetForItemKey(key);
        if (!merchantSellTargetAvailable(target)) {
            baseStatus_ = "売却内容が変わりました";
            return false;
        }
        targets.push_back(target);
    }

    for (std::size_t i = 0; i < baseMerchantBulkSell_.selectedKeys.size(); ++i) {
        const ItemKey& key = baseMerchantBulkSell_.selectedKeys[i];
        const MerchantSellTarget& target = targets[i];
        if (target.source != BaseItemSource::Backpack) {
            continue;
        }
        const int quantity = merchantSellTargetQuantity(target);
        const bool removed = key.stack
            ? inventory_.removeObjectItemCount(key.stableId, quantity)
            : inventory_.removeObjectInstance(key.stableId);
        if (!removed) {
            baseStatus_ = "まとめ売りに失敗したよ";
            return false;
        }
    }

    std::vector<int> warehouseInstanceIndices;
    std::vector<int> warehouseStackIndices;
    std::array<std::vector<int>, SpellRingCount> ringItemIndices;
    for (const MerchantSellTarget& target : targets) {
        if (target.source == BaseItemSource::Warehouse) {
            if (target.storageEntry.kind == StorageEntryKind::Stack) {
                warehouseStackIndices.push_back(target.storageEntry.index);
            } else {
                warehouseInstanceIndices.push_back(target.storageEntry.index);
            }
        } else if (target.source != BaseItemSource::Backpack &&
            target.ringIndex >= 0 &&
            target.ringIndex < SpellRingCount) {
            ringItemIndices[static_cast<std::size_t>(target.ringIndex)].push_back(target.ringItemIndex);
        }
    }
    const auto descending = [](int left, int right) { return left > right; };
    std::sort(warehouseInstanceIndices.begin(), warehouseInstanceIndices.end(), descending);
    for (const int index : warehouseInstanceIndices) {
        removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + index);
        warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + index);
    }
    std::sort(warehouseStackIndices.begin(), warehouseStackIndices.end(), descending);
    for (const int index : warehouseStackIndices) {
        removeWarehouseDisplaySlotAtEntryIndex(index);
        warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + index);
    }

    bool removedRingItem = false;
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        std::vector<int>& indices = ringItemIndices[static_cast<std::size_t>(ringIndex)];
        std::sort(indices.begin(), indices.end(), descending);
        std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
        for (const int index : indices) {
            ringItems.erase(ringItems.begin() + index);
            removedRingItem = true;
        }
    }
    if (removedRingItem) {
        refreshOrbitEffects();
    }

    money_ += summary.totalPrice;
    baseStatus_ = std::to_string(summary.itemCount) + "個を" + std::to_string(summary.totalPrice) + "円で売却した";
    playAudioSe(AudioSeMerchantTransaction);
    clearMerchantBulkSellState();
    return true;
}

void Game::clearMerchantBulkSellState()
{
    clearBatchItemSelectionState(baseMerchantBulkSell_);
}

bool Game::sellMerchantTarget(MerchantSellTarget target, int count)
{
    const auto completeSale = [this]() {
        baseStatus_ = "売却したよ";
        playAudioSe(AudioSeMerchantTransaction);
    };

    if (!target.valid) {
        baseStatus_ = "売却対象がないよ";
        return false;
    }
    if (!merchantSellTargetAvailable(target)) {
        baseStatus_ = "売れないよ";
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
        return false;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            const int soldCount = count <= 0 ? stack->count : std::min(count, stack->count);
            const std::string objectId = stack->objectId;
            const int price = sellPrice(stack->item) * std::max(1, soldCount);
            if (inventory_.removeObjectItemCount(objectId, soldCount)) {
                money_ += price;
                completeSale();
                return true;
            }
            baseStatus_ = "売却できなかったよ";
            return false;
        }

        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            const std::string instanceId = instance->instance.instanceId;
            const int price = sellPrice(instance->item, &instance->instance);
            if (inventory_.removeObjectInstance(instanceId)) {
                money_ += price;
                completeSale();
                return true;
            }
            baseStatus_ = "売却できなかったよ";
            return false;
        }

        baseStatus_ = "売却対象がないよ";
        return false;
    }

    if (target.source == BaseItemSource::Warehouse) {
        if (target.storageEntry.kind == StorageEntryKind::Stack) {
            if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectStacks_.size())) {
                baseStatus_ = "売却対象がないよ";
                return false;
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
            completeSale();
            return true;
        }

        if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectInstances_.size())) {
            baseStatus_ = "売却対象がないよ";
            return false;
        }
        const InventoryObjectInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(target.storageEntry.index)];
        money_ += sellPrice(instance.item, &instance.instance);
        removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index);
        warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + target.storageEntry.index);
        baseSellSelection_ = std::clamp(baseSellSelection_, 0, StoragePaneSlotCount - 1);
        completeSale();
        return true;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        baseStatus_ = "売却対象がないよ";
        return false;
    }
    std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        baseStatus_ = "売却対象がないよ";
        return false;
    }
    const ItemData* item = objectForRingItem(objectCatalog_, ringItems[static_cast<std::size_t>(target.ringItemIndex)]);
    if (item == nullptr) {
        baseStatus_ = "売れないよ";
        return false;
    }

    money_ += sellPrice(*item, &ringItems[static_cast<std::size_t>(target.ringItemIndex)]);
    ringItems.erase(ringItems.begin() + target.ringItemIndex);
    refreshOrbitEffects();
    baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, static_cast<int>(ringItems.size()) - 1));
    completeSale();
    return true;
}

void Game::sellMerchantScreenSlot(int slotIndex, int count)
{
    sellMerchantTarget(merchantSellTargetForSourceSlot(0, slotIndex), count);
}

bool Game::buyMerchantProduct(int index, int count)
{
    refreshMerchantStock(false);
    if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
        baseStatus_ = "購入できる商品がないよ";
        return false;
    }

    MerchantProduct& product = merchantStock_[static_cast<std::size_t>(index)];
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    if (item == nullptr) {
        baseStatus_ = "商品データがないよ";
        return false;
    }
    if (product.quantity <= 0) {
        baseStatus_ = "品切れだよ";
        return false;
    }
    if (money_ < product.price) {
        baseStatus_ = "所持金が足りないよ";
        return false;
    }
    const int purchasableQuantity = merchantProductPurchasableQuantity(product);
    if (purchasableQuantity <= 0) {
        baseStatus_ = "リュックがいっぱいだよ";
        return false;
    }
    const int purchaseCount = std::clamp(count, 1, product.quantity);
    if (purchaseCount > purchasableQuantity) {
        baseStatus_ = "その個数は購入できないよ";
        return false;
    }

    InventorySystem purchasedInventory = inventory_;
    std::vector<InventoryAddResult> addResults;
    addResults.reserve(static_cast<std::size_t>(purchaseCount));
    for (int i = 0; i < purchaseCount; ++i) {
        InventoryAddResult addResult;
        if (!purchasedInventory.addObjectItem(objectCatalog_, product.objectId, &addResult)) {
            baseStatus_ = "リュックがいっぱいだよ";
            return false;
        }
        addResults.push_back(std::move(addResult));
    }

    inventory_ = std::move(purchasedInventory);
    money_ -= product.price * purchaseCount;
    product.quantity -= purchaseCount;
    for (const InventoryAddResult& addResult : addResults) {
        recordObjectObtainedForFirstNotice(
            product.objectId,
            addResult.instanceId,
            addResult.kind == InventoryAddKind::Instance && !addResult.instanceId.empty(),
            basePlayerPosition_);
    }
    baseStatus_ = product.quantity <= 0 ? "購入したよ（品切れ）" : "購入したよ";
    playAudioSe(AudioSeMerchantTransaction);
    return true;
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

bool Game::processingBulkRepairExecutable() const
{
    return processingBulkRepairTargetCount() > 0 &&
        money_ >= processingBulkRepairMoneyCost() &&
        inventory_.materialCount(MaterialType::EnhancementOre) >= processingBulkRepairOreCost();
}

void Game::applyProcessingBulkRepair()
{
    const int targetCount = processingBulkRepairTargetCount();
    if (targetCount <= 0) {
        baseStatus_ = "修理が必要なアイテムはないよ";
        return;
    }

    const int moneyCost = processingBulkRepairMoneyCost();
    const int oreCost = processingBulkRepairOreCost();
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りないよ";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りないよ";
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
        lines.push_back(std::to_string(repairedCount) + "個のアイテムを修理したよ");
        if (moneyCost > 0 || oreCost > 0) {
            lines.push_back(
                "費用: " +
                std::to_string(moneyCost) +
                "G / 強化鉱石 x" +
                std::to_string(oreCost));
        }
        baseStatus_.clear();
        playAudioSe(AudioSeWorkbenchRepair);
        openUiResultDialog(
            baseResultDialog_,
            "一括修理完了",
            lines);
        return;
    }

    baseStatus_ = "修理が必要なアイテムはないよ";
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
    baseProcessingConfirm_.confirmState = uiButtonState(executable);
    baseStatus_.clear();
}

void Game::drawProcessingConfirmDialog(Renderer& renderer, UiRect panel) const
{
    if (!baseProcessingConfirm_.open) {
        return;
    }

    UiModalNavigationScope navigationScope(panel);
    UiWindowScope window(
        renderer,
        "base.processing.confirm",
        panel,
        baseProcessingConfirm_.title,
        uiConfirmDialogHelpText(),
        UiWindowOptions{true, true});

    const UiRect body = baseActionConfirmBodyRect(panel);

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
        renderer.drawText(body.pos, "加工対象がないよ", ui::Text, 2);
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

    std::vector<ConfirmPreviewRow> previewRows;
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
        previewRows.push_back({"攻撃力補正", formatProcessingSignedInt(before.attackBonus), formatProcessingSignedInt(after.attackBonus)});
        previewRows.push_back({"掘削力補正", formatProcessingSignedInt(before.digBonus), formatProcessingSignedInt(after.digBonus)});
        previewRows.push_back({"耐久力補正", formatProcessingSignedInt(before.durabilityBonus), formatProcessingSignedInt(after.durabilityBonus)});
        previewRows.push_back({"合計強化回数", formatProcessingInt(before.enhanceLevel), formatProcessingInt(after.enhanceLevel)});
    } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
        previewRows.push_back({"重量", formatProcessingPercent(before.weightModifier), formatProcessingPercent(after.weightModifier)});
        previewRows.push_back({"大きさ", formatProcessingPercent(before.sizeModifier), formatProcessingPercent(after.sizeModifier)});
    } else {
        if (mode == ProcessingMode::Attack) {
            previewRows.push_back({"攻撃力補正", formatProcessingSignedInt(before.attackBonus), formatProcessingSignedInt(after.attackBonus)});
        } else if (mode == ProcessingMode::Dig) {
            previewRows.push_back({"掘削力補正", formatProcessingSignedInt(before.digBonus), formatProcessingSignedInt(after.digBonus)});
        } else if (mode == ProcessingMode::Durability) {
            previewRows.push_back({"最大耐久力", formatProcessingMaxDurability(before.maxDurability), formatProcessingMaxDurability(after.maxDurability)});
        }
        previewRows.push_back({"合計強化回数", formatProcessingInt(before.enhanceLevel), formatProcessingInt(after.enhanceLevel)});
    }

    const auto confirmQuestion = [&]() -> std::string {
        const std::string itemName = processingInlineItemName(before);
        switch (mode) {
        case ProcessingMode::Repair:
            return itemName + "を修理する？";
        case ProcessingMode::ResetEnhancement:
            return itemName + "の強化をリセットする？";
        case ProcessingMode::Lighten:
            return itemName + "を軽量化する？";
        case ProcessingMode::Enlarge:
            return itemName + "を大型化する？";
        case ProcessingMode::Attack:
        case ProcessingMode::Dig:
        case ProcessingMode::Durability:
            return itemName + "を強化する？";
        }
        return itemName + "に作業を行う？";
    };

    float y = body.pos.y;
    InlineItemTextStyle questionStyle{};
    questionStyle.text = ui::Text;
    questionStyle.scale = 2;
    const std::string question = fittedInlineItemText(renderer, confirmQuestion(), body.size.x, questionStyle);
    drawInlineItemText(renderer, objectCatalog_, {body.pos.x, y}, question, questionStyle);
    y += measureInlineItemText(renderer, question, questionStyle).y + 22.0f;

    for (const ConfirmPreviewRow& row : previewRows) {
        drawConfirmPreviewRow(renderer, body, y, row);
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

    drawRequirementSubWindow(
        renderer,
        objectCatalog_,
        baseActionConfirmRequirementRect(panel, body, y, requirements.size()),
        requirements);

    drawUiConfirmDialogButtons(renderer, baseProcessingConfirm_, panel);
}

void Game::applyProcessing(int entryIndex)
{
    const std::vector<StorageEntry> entries = processingEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "加工対象がないよ";
        return;
    }
    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    applyProcessingEntry(entry);
}

void Game::applyProcessingScreenSlot(int slotIndex)
{
    const ProcessingTarget target = processingTargetForScreenSlot(slotIndex);
    if (!target.valid) {
        baseStatus_ = "加工対象がないよ";
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
            baseStatus_ = "この作業は未解禁だよ";
        } else if ((mode == ProcessingMode::Repair || mode == ProcessingMode::ResetEnhancement) && entry.kind == StorageEntryKind::Stack) {
            baseStatus_ = "この作業はできないよ";
        } else if (mode == ProcessingMode::ResetEnhancement) {
            baseStatus_ = "リセット不要だよ";
        } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
            baseStatus_ = "加工済みだよ";
        } else {
            baseStatus_ = mode == ProcessingMode::Repair ? "修理不要だよ" : "強化上限だよ";
        }
        return;
    }

    const int moneyCost = processingMoneyCost(entry, mode, warehouseEntry);
    const int oreCost = processingOreCost(entry, mode, warehouseEntry);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りないよ";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りないよ";
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
            const int durabilityBonusUnits = durabilityPointsToUnits(enhanceBonuses.durability);
            instance.maxDurability += durabilityBonusUnits;
            instance.currentDurability = std::min(instance.maxDurability, std::max(0, instance.currentDurability + durabilityBonusUnits));
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
        const int baseDurability = item != nullptr
            ? durabilityPointsToUnits(item->durability)
            : std::max(-1, instance.maxDurability - durabilityPointsToUnits(instance.durabilityBonus));
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
        const int originalSlot = warehouseItemLayout_.slotForEntry(entry.index);
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
            warehouseItemLayout_.insertEntry(
                warehouseUsedSlots() - 1,
                stackSlotWillRemain ? -1 : originalSlot);
            syncWarehouseDisplaySlots();
        }
    } else {
        ItemInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance;
        processed = (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge)
            ? applyShapeProcessing(instance, mode)
            : applyEnhancement(instance);
    }
    if (!processed) {
        baseStatus_ = "加工できないよ";
        return;
    }

    money_ -= moneyCost;
    if (oreCost > 0) {
        const bool spentOre = inventory_.materials().spend(MaterialType::EnhancementOre, oreCost);
        (void)spentOre;
    }
    baseStatus_.clear();
    const std::string_view successCue = processingSuccessAudioCue(
        mode == ProcessingMode::Repair,
        mode == ProcessingMode::Attack || mode == ProcessingMode::Dig || mode == ProcessingMode::Durability);
    if (!successCue.empty()) {
        playAudioSe(successCue);
    }
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
        baseStatus_ = "加工対象がないよ";
        return;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        applyProcessingEntry(target.backpackEntry, mode, target.warehouseEntry);
        return;
    }

    if (!processingTargetAvailable(target, mode)) {
        if (!processingModeUnlocked(mode)) {
            baseStatus_ = "この作業は未解禁だよ";
        } else if (mode == ProcessingMode::ResetEnhancement) {
            baseStatus_ = "リセット不要だよ";
        } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
            baseStatus_ = "加工済みだよ";
        } else {
            baseStatus_ = mode == ProcessingMode::Repair ? "修理不要だよ" : "強化上限だよ";
        }
        return;
    }

    const int moneyCost = processingMoneyCost(target, mode);
    const int oreCost = processingOreCost(target, mode);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りないよ";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りないよ";
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
                const int baseDurability = object != nullptr
                    ? durabilityPointsToUnits(object->durability)
                    : std::max(-1, item.maxDurability - durabilityPointsToUnits(item.durabilityBonus));
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
        baseStatus_ = "加工できないよ";
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
    const std::string_view successCue = processingSuccessAudioCue(
        mode == ProcessingMode::Repair,
        mode == ProcessingMode::Attack || mode == ProcessingMode::Dig || mode == ProcessingMode::Durability);
    if (!successCue.empty()) {
        playAudioSe(successCue);
    }
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
    warehouseItemLayout_.sync(warehouseUsedSlots(), warehouseCapacity());
}

void Game::sortWarehouseByItemOrder()
{
    closeUiCommandMenu(baseStorageCommandMenu_);
    baseStorageCommandOperation_ = StorageQuantityOperation::None;
    baseStorageCommandTarget_ = {};
    clearBaseItemInteractions();

    const int totalCount = warehouseUsedSlots();
    if (totalCount <= 0) {
        warehouseItemLayout_.clear();
        baseStorageWarehousePage_ = 0;
        baseStorageWithdrawSelection_ = 0;
        baseStatus_ = "収納箱は空だよ";
        return;
    }

    const ItemSortPolicy sortPolicy(objectCatalog_);
    std::stable_sort(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [&sortPolicy](const InventoryObjectStack& a, const InventoryObjectStack& b) {
        return sortPolicy.less({a.objectId}, {b.objectId});
    });
    std::stable_sort(warehouseObjectInstances_.begin(), warehouseObjectInstances_.end(), [&sortPolicy](const InventoryObjectInstance& a, const InventoryObjectInstance& b) {
        const std::string& idA = objectSortId(a);
        const std::string& idB = objectSortId(b);
        return sortPolicy.less({idA}, {idB});
    });

    std::vector<int> entryIndices;
    entryIndices.reserve(static_cast<std::size_t>(totalCount));
    for (int i = 0; i < totalCount; ++i) {
        entryIndices.push_back(i);
    }
    std::stable_sort(entryIndices.begin(), entryIndices.end(), [this, &sortPolicy](int a, int b) {
        const std::string& idA = warehouseEntrySortId(a, warehouseObjectStacks_, warehouseObjectInstances_);
        const std::string& idB = warehouseEntrySortId(b, warehouseObjectStacks_, warehouseObjectInstances_);
        return sortPolicy.less({idA}, {idB});
    });

    warehouseItemLayout_.assignSequential(entryIndices, warehouseCapacity());

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
    return warehouseItemLayout_.entryAtSlot(slot);
}

void Game::assignWarehouseEntryToStorageSlot(int entryIndex, int slot)
{
    syncWarehouseDisplaySlots();
    (void)warehouseItemLayout_.moveEntryToSlot(entryIndex, slot, warehouseCapacity());
}

void Game::removeWarehouseDisplaySlotAtEntryIndex(int entryIndex)
{
    syncWarehouseDisplaySlots();
    warehouseItemLayout_.eraseEntry(entryIndex);
}

bool Game::canAddWarehouseObjectStack(std::string_view objectId, int count) const
{
    if (objectId.empty() || count <= 0) {
        return false;
    }
    const auto existing = std::find_if(
        warehouseObjectStacks_.begin(),
        warehouseObjectStacks_.end(),
        [objectId](const InventoryObjectStack& stack) {
            return stack.objectId == objectId;
        });
    if (existing != warehouseObjectStacks_.end()) {
        return existing->count <= std::numeric_limits<int>::max() - count;
    }
    return warehouseUsedSlots() < warehouseCapacity();
}

bool Game::addWarehouseObjectStack(const InventoryObjectStack& stack, int count)
{
    if (!canAddWarehouseObjectStack(stack.objectId, count)) {
        return false;
    }

    const auto existing = std::find_if(
        warehouseObjectStacks_.begin(),
        warehouseObjectStacks_.end(),
        [&stack](const InventoryObjectStack& stored) {
            return stored.objectId == stack.objectId;
        });
    if (existing != warehouseObjectStacks_.end()) {
        existing->count += count;
        existing->item = stack.item;
        existing->item.id = stack.objectId;
        existing->objectId = stack.objectId;
        return true;
    }

    InventoryObjectStack added = stack;
    added.objectId = stack.objectId;
    added.item.id = stack.objectId;
    added.count = count;
    syncWarehouseDisplaySlots();
    warehouseItemLayout_.insertEntry(static_cast<int>(warehouseObjectStacks_.size()));
    warehouseObjectStacks_.push_back(std::move(added));
    return true;
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

std::vector<Game::StorageTransferTarget> Game::storageDepositTargetsForSource(int source) const
{
    std::vector<StorageTransferTarget> targets;
    if (baseItemSourceIsRing(source)) {
        const int ringIndex = ringIndexFromBaseItemSource(source);
        if (ringIndex < 0 || ringIndex >= SpellRingCount) {
            return targets;
        }
        const int itemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
        targets.reserve(static_cast<std::size_t>(itemCount));
        for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
            targets.push_back(storageDepositTargetForSourceSlot(source, itemIndex));
        }
        return targets;
    }

    if (source != BaseBackpackSourceIndex) {
        return targets;
    }
    targets.reserve(static_cast<std::size_t>(inventory_.screenSlotCount()));
    for (int slot = 0; slot < inventory_.screenSlotCount(); ++slot) {
        targets.push_back(storageDepositTargetForSourceSlot(source, slot));
    }
    return targets;
}

bool Game::storageBulkDepositTargetSelected(StorageTransferTarget target) const
{
    return batchItemSelectionTargetSelected(baseStorageBatchSelection_, target);
}

bool Game::toggleStorageBulkDepositTarget(StorageTransferTarget target)
{
    return storageTransferTargetAvailable(target) &&
        toggleBatchItemSelectionTarget(baseStorageBatchSelection_, target);
}

void Game::selectAllStorageBulkDepositTargets(int source)
{
    for (const StorageTransferTarget target : storageDepositTargetsForSource(source)) {
        if (storageTransferTargetAvailable(target)) {
            addBatchItemSelectionTarget(baseStorageBatchSelection_, target);
        }
    }
}

void Game::pruneStorageBulkDepositSelection()
{
    baseStorageBatchSelection_.selectedKeys.erase(
        std::remove_if(
            baseStorageBatchSelection_.selectedKeys.begin(),
            baseStorageBatchSelection_.selectedKeys.end(),
            [this](const ItemKey& key) {
                const StorageTransferTarget target = baseItemTargetForItemKey(key);
                return target.source == BaseItemSource::Warehouse ||
                    !storageTransferTargetAvailable(target);
            }),
        baseStorageBatchSelection_.selectedKeys.end());
}

Game::StorageBatchTransferSummary Game::storageBulkDepositSummary() const
{
    StorageBatchTransferSummary summary{};
    summary.freeSlots = std::max(0, warehouseCapacity() - warehouseUsedSlots());

    std::unordered_set<std::string> storedStackIds;
    storedStackIds.reserve(
        warehouseObjectStacks_.size() + baseStorageBatchSelection_.selectedKeys.size());
    for (const InventoryObjectStack& stack : warehouseObjectStacks_) {
        if (!stack.objectId.empty() && stack.count > 0) {
            storedStackIds.insert(stack.objectId);
        }
    }

    for (const ItemKey& key : baseStorageBatchSelection_.selectedKeys) {
        const StorageTransferTarget target = baseItemTargetForItemKey(key);
        if (!storageTransferTargetAvailable(target) ||
            target.source == BaseItemSource::Warehouse) {
            continue;
        }
        ++summary.selectedCount;
        if (storageTransferTargetIsStack(target)) {
            if (storedStackIds.insert(key.stableId).second) {
                ++summary.requiredSlots;
            }
        } else {
            ++summary.requiredSlots;
        }
    }
    return summary;
}

bool Game::depositStorageBulkSelection()
{
    pruneStorageBulkDepositSelection();
    const StorageBatchTransferSummary summary = storageBulkDepositSummary();
    if (summary.selectedCount <= 0) {
        baseStatus_ = "しまうアイテムが選択されていないよ";
        return false;
    }
    if (!summary.fits()) {
        baseStatus_ = "収納箱の空きがあと" +
            std::to_string(summary.requiredSlots - summary.freeSlots) + "枠必要だよ";
        return false;
    }

    struct PendingDeposit {
        ItemKey key;
        StorageTransferTarget target;
    };
    std::vector<PendingDeposit> pending;
    pending.reserve(baseStorageBatchSelection_.selectedKeys.size());
    for (const ItemKey& key : baseStorageBatchSelection_.selectedKeys) {
        const StorageTransferTarget target = baseItemTargetForItemKey(key);
        if (!storageTransferTargetAvailable(target) ||
            target.source == BaseItemSource::Warehouse) {
            baseStatus_ = "収納内容が変わりました";
            return false;
        }
        pending.push_back({key, target});
    }

    std::stable_sort(pending.begin(), pending.end(), [](const PendingDeposit& left, const PendingDeposit& right) {
        if (left.target.source != right.target.source) {
            return static_cast<int>(left.target.source) < static_cast<int>(right.target.source);
        }
        if (left.target.ringIndex != right.target.ringIndex) {
            return left.target.ringIndex < right.target.ringIndex;
        }
        return left.target.ringItemIndex > right.target.ringItemIndex;
    });

    for (const PendingDeposit& entry : pending) {
        const StorageTransferTarget target = baseItemTargetForItemKey(entry.key);
        if (!storageTransferTargetAvailable(target)) {
            baseStatus_ = "収納内容が変わりました";
            return false;
        }
        if (!depositStorageTarget(target, storageTransferTargetStackCount(target))) {
            return false;
        }
    }

    syncWarehouseDisplaySlots();
    syncEncyclopediaFromInventoryAndRing();
    if (baseItemSourceIsRing(baseStorageDepositSource_)) {
        const int ringIndex = ringIndexFromBaseItemSource(baseStorageDepositSource_);
        const int ringItemCount = ringIndex >= 0 && ringIndex < SpellRingCount
            ? static_cast<int>(spellRing_.itemsForRing(ringIndex).size())
            : 0;
        baseStorageDepositSelection_ = std::clamp(
            baseStorageDepositSelection_,
            0,
            std::max(0, ringItemCount - 1));
    } else {
        baseStorageDepositSelection_ = std::clamp(
            baseStorageDepositSelection_,
            0,
            std::max(0, inventory_.screenSlotCount() - 1));
    }
    clearStorageBatchSelectionState();
    baseStatus_ = std::to_string(summary.selectedCount) + "個を収納箱にしまいました";
    return true;
}

void Game::clearStorageBatchSelectionState()
{
    clearBatchItemSelectionState(baseStorageBatchSelection_);
}

bool Game::depositStorageTarget(StorageTransferTarget target, int count)
{
    if (!target.valid) {
        baseStatus_ = "しまうアイテムがないよ";
        return false;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* source = inventory_.screenObjectStackAt(target.slotIndex)) {
            const int moveCount = std::clamp(count, 1, std::max(1, source->count));
            InventoryObjectStack moved = *source;
            moved.item.id = moved.objectId;
            if (!canAddWarehouseObjectStack(moved.objectId, moveCount)) {
                baseStatus_ = "収納箱がいっぱいだよ";
                return false;
            }
            if (!inventory_.removeObjectItemCount(moved.objectId, moveCount)) {
                baseStatus_ = "しまえなかったよ";
                return false;
            }
            if (!addWarehouseObjectStack(moved, moveCount)) {
                (void)inventory_.addObjectStack(moved.item, moveCount);
                baseStatus_ = "しまえなかったよ";
                return false;
            }
            baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
            baseStatus_ = "収納箱にしまいました";
            return true;
        }

        const InventoryObjectInstance* source = inventory_.screenObjectInstanceAt(target.slotIndex);
        if (source == nullptr) {
            baseStatus_ = "しまうアイテムがないよ";
            return false;
        }
        if (inventory_.isStaffEquipped(source->instance.instanceId)) {
            baseStatus_ = "装備中の杖はしまえないよ";
            return false;
        }
        if (warehouseUsedSlots() >= warehouseCapacity()) {
            baseStatus_ = "収納箱がいっぱいだよ";
            return false;
        }
        InventoryObjectInstance moved;
        if (!inventory_.takeObjectInstance(source->instance.instanceId, moved)) {
            baseStatus_ = "しまえなかったよ";
            return false;
        }
        warehouseObjectInstances_.push_back(std::move(moved));
        baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
        baseStatus_ = "収納箱にしまいました";
        return true;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        baseStatus_ = "しまうアイテムがないよ";
        return false;
    }
    std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        baseStatus_ = "しまうアイテムがないよ";
        return false;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    if (ringItem.objectId.empty()) {
        baseStatus_ = "このアイテムはしまえないよ";
        return false;
    }
    if (warehouseUsedSlots() >= warehouseCapacity()) {
        baseStatus_ = "収納箱がいっぱいだよ";
        return false;
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
    return true;
}

bool Game::withdrawStorageTarget(StorageTransferTarget target, int count)
{
    if (!target.valid || target.source != BaseItemSource::Warehouse) {
        baseStatus_ = "取り出すアイテムがないよ";
        return false;
    }

    if (target.storageEntry.kind == StorageEntryKind::Stack) {
        if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectStacks_.size())) {
            baseStatus_ = "取り出すアイテムがないよ";
            return false;
        }
        InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(target.storageEntry.index)];
        const int moveCount = std::clamp(count, 1, std::max(1, stack.count));
        ItemData movedItem = stack.item;
        movedItem.id = stack.objectId;
        if (!inventory_.addObjectStack(movedItem, moveCount)) {
            baseStatus_ = "リュックがいっぱいだよ";
            return false;
        }
        stack.count -= moveCount;
        if (stack.count <= 0) {
            removeWarehouseDisplaySlotAtEntryIndex(target.storageEntry.index);
            warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + target.storageEntry.index);
        }
        baseStorageWithdrawSelection_ = std::clamp(baseStorageWithdrawSelection_, 0, StorageWithdrawSlotCount - 1);
        baseStatus_ = "リュックに取り出したよ";
        return true;
    }

    if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectInstances_.size())) {
        baseStatus_ = "取り出すアイテムがないよ";
        return false;
    }
    InventoryObjectInstance moved = warehouseObjectInstances_[static_cast<std::size_t>(target.storageEntry.index)];
    if (!inventory_.addObjectInstance(objectCatalog_, moved.instance)) {
        baseStatus_ = "リュックがいっぱいだよ";
        return false;
    }
    removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index);
    warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + target.storageEntry.index);
    baseStorageWithdrawSelection_ = std::clamp(baseStorageWithdrawSelection_, 0, StorageWithdrawSlotCount - 1);
    baseStatus_ = "リュックに取り出したよ";
    return true;
}

std::vector<Game::StorageTransferTarget> Game::storageWithdrawTargets() const
{
    const std::vector<StorageEntry> entries = warehouseStorageEntries();
    std::vector<StorageTransferTarget> targets;
    targets.reserve(entries.size());
    for (const StorageEntry entry : entries) {
        StorageTransferTarget target{};
        target.source = BaseItemSource::Warehouse;
        target.storageEntry = entry;
        target.warehouseEntry = true;
        target.valid = true;
        targets.push_back(target);
    }
    return targets;
}

bool Game::storageBulkWithdrawTargetSelected(StorageTransferTarget target) const
{
    return batchItemSelectionTargetSelected(baseStorageBatchSelection_, target);
}

bool Game::toggleStorageBulkWithdrawTarget(StorageTransferTarget target)
{
    return target.source == BaseItemSource::Warehouse &&
        storageTransferTargetAvailable(target) &&
        toggleBatchItemSelectionTarget(baseStorageBatchSelection_, target);
}

void Game::selectAllStorageBulkWithdrawTargets()
{
    for (const StorageTransferTarget target : storageWithdrawTargets()) {
        if (storageTransferTargetAvailable(target)) {
            addBatchItemSelectionTarget(baseStorageBatchSelection_, target);
        }
    }
}

void Game::pruneStorageBulkWithdrawSelection()
{
    baseStorageBatchSelection_.selectedKeys.erase(
        std::remove_if(
            baseStorageBatchSelection_.selectedKeys.begin(),
            baseStorageBatchSelection_.selectedKeys.end(),
            [this](const ItemKey& key) {
                const StorageTransferTarget target = baseItemTargetForItemKey(key);
                return target.source != BaseItemSource::Warehouse ||
                    !storageTransferTargetAvailable(target);
            }),
        baseStorageBatchSelection_.selectedKeys.end());
}

Game::StorageBatchTransferSummary Game::storageBulkWithdrawSummary() const
{
    StorageBatchTransferSummary summary{};
    summary.freeSlots = std::max(0, inventory_.screenSlotCount() - backpackUsedSlots());

    std::unordered_set<std::string> backpackStackIds;
    backpackStackIds.reserve(
        inventory_.objectStacks().size() + baseStorageBatchSelection_.selectedKeys.size());
    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        if (!stack.objectId.empty() && stack.count > 0) {
            backpackStackIds.insert(stack.objectId);
        }
    }

    for (const ItemKey& key : baseStorageBatchSelection_.selectedKeys) {
        const StorageTransferTarget target = baseItemTargetForItemKey(key);
        if (target.source != BaseItemSource::Warehouse ||
            !storageTransferTargetAvailable(target)) {
            continue;
        }
        ++summary.selectedCount;
        if (storageTransferTargetIsStack(target)) {
            if (backpackStackIds.insert(key.stableId).second) {
                ++summary.requiredSlots;
            }
        } else {
            ++summary.requiredSlots;
        }
    }
    return summary;
}

bool Game::withdrawStorageBulkSelection()
{
    pruneStorageBulkWithdrawSelection();
    const StorageBatchTransferSummary summary = storageBulkWithdrawSummary();
    if (summary.selectedCount <= 0) {
        baseStatus_ = "取り出すアイテムが選択されていないよ";
        return false;
    }
    if (!summary.fits()) {
        baseStatus_ = "リュックの空きがあと" +
            std::to_string(summary.requiredSlots - summary.freeSlots) + "枠必要だよ";
        return false;
    }

    std::vector<ItemKey> pending = baseStorageBatchSelection_.selectedKeys;
    for (const ItemKey& key : pending) {
        const StorageTransferTarget target = baseItemTargetForItemKey(key);
        if (target.source != BaseItemSource::Warehouse ||
            !storageTransferTargetAvailable(target)) {
            baseStatus_ = "取り出す内容が変わりました";
            return false;
        }
    }
    std::stable_sort(pending.begin(), pending.end(), [](const ItemKey& left, const ItemKey& right) {
        return left.stack && !right.stack;
    });

    InventorySystem previewInventory = inventory_;
    for (const ItemKey& key : pending) {
        const StorageTransferTarget target = baseItemTargetForItemKey(key);
        bool transferable = true;
        if (storageTransferTargetIsStack(target)) {
            const int itemCount = storageTransferTargetStackCount(target);
            const ItemData* item = storageEntryItem(target.storageEntry, true);
            if (item != nullptr) {
                ItemData movedItem = *item;
                movedItem.id = key.stableId;
                transferable = previewInventory.addObjectStack(movedItem, itemCount);
            } else {
                transferable = false;
            }
        } else {
            const ItemInstance* instance = storageEntryInstance(target.storageEntry, true);
            transferable = instance != nullptr &&
                previewInventory.addObjectInstance(objectCatalog_, *instance);
        }
        if (!transferable) {
            baseStatus_ = "取り出せないアイテムが含まれています";
            return false;
        }
    }

    for (const ItemKey& key : pending) {
        const StorageTransferTarget target = baseItemTargetForItemKey(key);
        if (!storageTransferTargetAvailable(target)) {
            baseStatus_ = "取り出す内容が変わりました";
            return false;
        }
        if (!withdrawStorageTarget(target, storageTransferTargetStackCount(target))) {
            return false;
        }
    }

    syncWarehouseDisplaySlots();
    syncEncyclopediaFromInventoryAndRing();
    baseStorageWithdrawSelection_ = std::clamp(
        baseStorageWithdrawSelection_,
        0,
        StorageWithdrawSlotCount - 1);
    clearStorageBatchSelectionState();
    baseStatus_ = std::to_string(summary.selectedCount) + "個をリュックに取り出したよ";
    return true;
}

int Game::storageBulkActionCount() const
{
    return 1 + std::clamp(unlockedRingPresetSlotCount(), 0, RingPresetSlotCount);
}

UiButtonState Game::storageBulkActionState(int actionIndex) const
{
    if (actionIndex == 0) {
        return uiButtonState(canDepositAnyBackpackItem());
    }

    const int presetIndex = actionIndex - 1;
    const bool available = presetIndex >= 0 &&
        presetIndex < unlockedRingPresetSlotCount() &&
        ringPresets_.registered(presetIndex);
    return uiButtonState(available);
}

bool Game::canDepositAnyBackpackItem() const
{
    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        if (canAddWarehouseObjectStack(stack.objectId, stack.count)) {
            return true;
        }
    }

    if (warehouseUsedSlots() >= warehouseCapacity()) {
        return false;
    }
    return std::any_of(
        inventory_.objectInstances().begin(),
        inventory_.objectInstances().end(),
        [this](const InventoryObjectInstance& instance) {
            return !inventory_.isStaffEquipped(instance.instance.instanceId);
        });
}

void Game::depositAllBackpackItems()
{
    int storedCount = 0;
    int skippedFullCount = 0;
    int skippedStaffCount = 0;

    const std::vector<InventoryObjectStack> backpackStacks = inventory_.objectStacks();
    for (const InventoryObjectStack& stack : backpackStacks) {
        if (stack.objectId.empty() || stack.count <= 0) {
            continue;
        }

        const int moveCount = stack.count;
        if (!canAddWarehouseObjectStack(stack.objectId, moveCount)) {
            skippedFullCount += stack.count;
            continue;
        }

        if (!inventory_.removeObjectItemCount(stack.objectId, moveCount)) {
            continue;
        }
        if (!addWarehouseObjectStack(stack, moveCount)) {
            ItemData restoredItem = stack.item;
            restoredItem.id = stack.objectId;
            (void)inventory_.addObjectStack(restoredItem, moveCount);
            continue;
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

    syncWarehouseDisplaySlots();
    syncEncyclopediaFromInventoryAndRing();

    if (storedCount <= 0) {
        if (skippedFullCount > 0) {
            baseStatus_ = "収納箱がいっぱいだよ";
        } else if (skippedStaffCount > 0) {
            baseStatus_ = "装備中の杖以外にしまう物がないよ";
        } else {
            baseStatus_ = "しまうアイテムがないよ";
        }
        return;
    }

    std::vector<std::string> resultLines{
        std::to_string(storedCount) + "個のアイテムをしまったよ",
    };
    baseStatus_ = resultLines.front();
    if (skippedFullCount > 0) {
        baseStatus_ += " / 満杯で" + std::to_string(skippedFullCount) + "個残りました";
        resultLines.push_back("収納箱がいっぱいで" + std::to_string(skippedFullCount) + "個残ったよ");
    }
    if (skippedStaffCount > 0) {
        baseStatus_ += " / 装備中の杖は残したよ";
        resultLines.push_back("装備中の杖はリュックに残したよ");
    }
    openUiResultDialog(baseResultDialog_, "収納完了", std::move(resultLines));
}

void Game::prepareRingPresetFromWarehouse(int presetIndex)
{
    const int presetSlotCount = unlockedRingPresetSlotCount();
    if (presetIndex < 0 || presetIndex >= presetSlotCount) {
        baseStatus_ = presetSlotCount <= 0
            ? "リングプリセットは未解禁だよ"
            : "プリセット" + std::to_string(presetIndex + 1) + "は未解禁だよ";
        return;
    }
    if (!ringPresets_.registered(presetIndex)) {
        baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "は未登録だよ";
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
        openUiResultDialog(baseResultDialog_, "準備結果", {baseStatus_});
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
        ItemData movedItem = it->item;
        movedItem.id = pick.objectId;
        if (!inventory_.addObjectStack(movedItem, 1)) {
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

    std::vector<std::string> resultLines;
    if (withdrawnCount <= 0) {
        if (fullCount > 0) {
            baseStatus_ = "リュックがいっぱいで取り出せないよ";
        } else {
            baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "の不足分は収納箱にないよ";
        }
    } else {
        baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "ぶんを" + std::to_string(withdrawnCount) + "個取り出したよ";
    }
    resultLines.push_back(baseStatus_);
    if (notFoundCount > 0) {
        baseStatus_ += " / 収納箱になし " + std::to_string(notFoundCount);
        resultLines.push_back("収納箱になし：" + std::to_string(notFoundCount) + "個");
    }
    if (fullCount > 0) {
        baseStatus_ += " / リュック満杯 " + std::to_string(fullCount);
        resultLines.push_back("リュック満杯：" + std::to_string(fullCount) + "個");
    }
    if (vanishedCount > 0) {
        baseStatus_ += " / 取り出せず " + std::to_string(vanishedCount);
        resultLines.push_back("取り出せず：" + std::to_string(vanishedCount) + "個");
    }
    openUiResultDialog(baseResultDialog_, "準備結果", std::move(resultLines));
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
        return "収納箱に保管できるアイテム数を増やすよ";
    case 1:
        return "商人ワゴンの商品枠や買取機能を強化するよ";
    case 2:
        return "作業台で扱える加工の種類を増やしたり、加工費用を割引したりするよ";
    case 3:
        return "リング工房を拠点に建てるよ　リングの個別の調整や強化が可能になるよ";
    case 4:
        return "ルネの最大HPを増やすよ";
    case 5:
        return "すべてのスペルリングの半径を増やすよ";
    case 6:
        return "すべてのスペルリングのアイテム回転速度を増やすよ";
    case 7:
        return "近くのアイテムをルネへ引き寄せる範囲を広げるよ";
    case 8:
        return "リングプリセットを解禁するよ　リングのアイテム配置を保存・呼び出しできるようになるよ";
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

bool Game::upgradeExecutable(int index) const
{
    if (!upgradeImplemented(index) || upgradeMaxed(index)) {
        return false;
    }
    const int cost = upgradeCost(index);
    if (cost <= 0 || money_ < cost) {
        return false;
    }
    const int materialCost = upgradeMaterialCost(index);
    return materialCost <= 0 || inventory_.materialCount(upgradeMaterialType(index)) >= materialCost;
}

void Game::closeBaseFacilityScreens()
{
    clearBaseItemInteractions();
    baseMiningStartChoiceActive_ = false;
    baseWarpPointSelectActive_ = false;
    baseRegenerateConfirm_ = {};
    baseRoguelikeDepartureConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    baseStorageActive_ = false;
    baseStorageMode_ = StorageUiMode::Closed;
    baseQuantityDialog_ = {};
    baseQuantityPending_ = {};
    clearStorageBatchSelectionState();
    closeUiCommandMenu(baseStorageCommandMenu_);
    baseSellActive_ = false;
    baseMerchantMode_ = MerchantUiMode::Closed;
    clearMerchantBulkSellState();
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
    baseStoryMarkedFacilities_.clear();
    baseStoryCommand_ = {};
    baseStoryChicoryFlight_ = {};
    baseStoryRingDemo_ = {};
    baseStoryFadeAlpha_ = 0.0f;
    basePlayerSpriteWalking_ = false;
    updateBasePlayerSpriteFlipFromFacing();
}

void Game::applyBaseReturnSceneBeginPlacement()
{
    placeBasePlayerAtMineExitReturnPoint();
    baseStoryFacilityOffsets_["elder"] = storyTileOffset(
        BaseReturnSceneElderOffsetTilesX,
        BaseReturnSceneElderOffsetTilesY);
    baseStoryFacilityOffsets_["monica"] = storyTileOffset(
        BaseReturnSceneMonicaOffsetTilesX,
        BaseReturnSceneMonicaOffsetTilesY);
    basePlayerSpriteWalking_ = false;
    updateBasePlayerSpriteFlipFromFacing();
}

bool Game::applyBaseReturnSceneBeginPlacementForTrigger(std::string_view trigger)
{
    if (!basePresentationActive()) {
        return false;
    }

    const StoryEvent* event = findStoryEventForTrigger(trigger);
    if (event == nullptr ||
        event->dialogue.steps.empty() ||
        !isBaseReturnSceneBeginStep(event->dialogue.steps.front())) {
        return false;
    }

    applyBaseReturnSceneBeginPlacement();
    return true;
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

void Game::renderBaseStoryFacilityMarkers(Renderer& renderer) const
{
    if (baseStoryMarkedFacilities_.empty() || !basePresentationActive()) {
        return;
    }

    renderer.setScreenSpace();
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
    drawBaseFacilityMarkers(
        renderer,
        facilities,
        baseArea_,
        ringWorkshopUnlocked_,
        baseStoryMarkedFacilities_,
        baseActorIdleAnimationTime_);
}

void Game::renderBaseStoryChicoryFlight(Renderer& renderer) const
{
    if (!baseStoryChicoryFlight_.active || !basePresentationActive()) {
        return;
    }

    const float duration = std::max(0.001f, baseStoryChicoryFlight_.durationSeconds);
    const float progress = clamp(baseStoryChicoryFlight_.elapsedSeconds / duration, 0.0f, 1.0f);
    const float fade = std::min(
        smoothStep01(progress / 0.16f),
        smoothStep01((1.0f - progress) / 0.18f));
    if (fade <= 0.01f) {
        return;
    }

    renderer.setScreenSpace();
    const Vec2 playerFoot = playerSpriteFootAnchor(basePlayerPosition_);
    const float theta = progress * Pi * 2.4f;
    const Vec2 figureEight{
        std::sin(theta) * 58.0f,
        std::sin(theta * 2.0f) * 24.0f,
    };
    const float launch = smoothStep01(progress / 0.22f);
    const Vec2 orbitPosition = baseStoryChicoryFlight_.centerPosition + figureEight + Vec2{0.0f, std::sin(baseActorIdleAnimationTime_ * 8.0f) * 4.0f};
    const Vec2 position = lerp(baseStoryChicoryFlight_.startPosition, orbitPosition, launch);
    const Vec2 shadowAnchor{position.x, playerFoot.y + 5.0f};
    const float shadowScale = 0.46f + 0.22f * (1.0f - clamp((playerFoot.y - position.y) / 112.0f, 0.0f, 1.0f));

    renderer.drawActorShadow(
        shadowAnchor,
        36.0f * shadowScale,
        {0, 0, 0, alphaByte(82.0f * fade)});
    renderer.fillSoftCircle(position, 24.0f, withAlpha({132, 244, 255, 255}, 82.0f * fade));
    renderer.fillSoftCircle(position, 13.0f, withAlpha({255, 246, 168, 255}, 132.0f * fade));
    renderer.fillCircle(position, 5.6f, withAlpha({255, 255, 236, 255}, 255.0f * fade));
    drawMagicStar(
        renderer,
        position + Vec2{0.0f, -1.0f},
        9.0f + std::sin(baseActorIdleAnimationTime_ * 10.0f) * 1.6f,
        withAlpha({255, 252, 204, 255}, 220.0f * fade),
        baseActorIdleAnimationTime_ * 1.8f);
}

void Game::renderBaseStoryRingDemo(Renderer& renderer) const
{
    if (!baseStoryRingDemo_.active || !basePresentationActive()) {
        return;
    }

    const float duration = std::max(0.001f, baseStoryRingDemo_.durationSeconds);
    const float t = smoothStep01(clamp(baseStoryRingDemo_.elapsedSeconds / duration, 0.0f, 1.0f));
    const float alpha = baseStoryRingDemo_.closing ? 1.0f - t : t;
    if (alpha <= 0.01f) {
        return;
    }

    renderer.setScreenSpace();
    const Vec2 center = basePlayerPosition_ + Vec2{0.0f, -2.0f};
    const float radiusScale = baseStoryRingDemo_.closing
        ? lerp(1.0f, 0.18f, t)
        : lerp(0.18f, 1.0f, t);
    const int visibleRingCount = std::clamp(baseStoryRingDemo_.visibleRingCount, 1, SpellRingCount);

    renderer.fillSoftCircle(center, 24.0f + 48.0f * radiusScale, withAlpha({118, 224, 255, 255}, 40.0f * alpha));
    for (RingShape shapePass : MagicRingShapeRenderOrder) {
        for (int ringIndex = 0; ringIndex < visibleRingCount; ++ringIndex) {
            const RingShape shape = spellRing_.ringShapeForIndex(ringIndex);
            if (shape != shapePass) {
                continue;
            }
            RingOrbitContext context = spellRing_.makeOrbitContextForRing(ringIndex, 0, 1, radiusScale, balance_);
            const float demoBaseAngle = normalizeAngle(
                spellRing_.ringBaseAngleForIndex(ringIndex) +
                spellRing_.ringAngularSpeedForIndex(ringIndex, balance_) * baseActorIdleAnimationTime_);
            if (shape == RingShape::FigureEight) {
                context.shapeRotation = normalizeAngle(
                    spellRing_.shapeRotationForRing(ringIndex) +
                    std::max(0.0f, context.tuning.figure8ShapeRotationSpeed) * baseActorIdleAnimationTime_);
            } else if (shape == RingShape::Comet) {
                context.shapeRotation = demoBaseAngle;
            }
            std::vector<Vec2> orbitPath = getRingPathSamplePoints(center, context, 176);
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
                    baseActorIdleAnimationTime_,
                    alpha * (ringIndex == 0 ? 0.86f : 1.0f),
                });
        }
    }

    struct DemoRingItem {
        SpellRingItem item;
        int ringItemCount = 1;
    };

    const auto preparedDemoRingItem = [&](SpellRingItem item, int ringIndex, int itemIndex, int itemCount) {
        item.ringIndex = std::clamp(ringIndex, 0, SpellRingCount - 1);
        const RingOrbitContext context = spellRing_.makeOrbitContextForRing(
            item.ringIndex,
            itemIndex,
            std::max(1, itemCount),
            radiusScale,
            balance_);
        RingOrbitContext demoContext = context;
        const RingShape shape = demoContext.shape;
        const float demoBaseAngle = normalizeAngle(
            spellRing_.ringBaseAngleForIndex(item.ringIndex) +
            spellRing_.ringAngularSpeedForIndex(item.ringIndex, balance_) * baseActorIdleAnimationTime_);
        float shapeRotationSpeed = 0.0f;
        if (shape == RingShape::FigureEight) {
            shapeRotationSpeed = std::max(0.0f, demoContext.tuning.figure8ShapeRotationSpeed);
            demoContext.shapeRotation = normalizeAngle(
                spellRing_.shapeRotationForRing(item.ringIndex) +
                shapeRotationSpeed * baseActorIdleAnimationTime_);
        } else if (shape == RingShape::Comet) {
            shapeRotationSpeed = spellRing_.ringAngularSpeedForIndex(item.ringIndex, balance_);
            demoContext.shapeRotation = demoBaseAngle;
        }
        const float orbitParam = shape == RingShape::Comet
            ? normalizeLocalParam(shape, item.localAngle, demoContext.tuning)
            : normalizeAngle(demoBaseAngle + item.localAngle);
        const float localAngularSpeed = shape == RingShape::Comet
            ? 0.0f
            : spellRing_.ringAngularSpeedForIndex(item.ringIndex, balance_);
        const Vec2 localPosition = getRingItemLocalPosition(orbitParam, demoContext);
        item.worldPosition = getRingItemWorldPosition(center, orbitParam, demoContext);
        item.orbitOutward = ringItemOutwardDirection(item, localPosition);
        item.orbitTangent = getRingItemVelocity(orbitParam, localAngularSpeed, shapeRotationSpeed, {}, demoContext);
        if (lengthSquared(item.orbitTangent) > 0.0001f) {
            item.orbitTangent = normalize(item.orbitTangent);
        } else {
            item.orbitTangent = {-item.orbitOutward.y, item.orbitOutward.x};
        }
        if (lengthSquared(item.orbitTangent) <= 0.0001f) {
            item.orbitTangent = {1.0f, 0.0f};
        }
        item.worldVelocity = item.orbitTangent;
        return item;
    };

    std::vector<DemoRingItem> demoItems;
    if (visibleRingCount >= 1) {
        const auto& equippedRingItems = spellRing_.itemsForRing(0);
        const int equippedItemCount = static_cast<int>(equippedRingItems.size());
        demoItems.reserve(equippedRingItems.size() + 1);
        for (int index = 0; index < equippedItemCount; ++index) {
            demoItems.push_back({
                preparedDemoRingItem(equippedRingItems[static_cast<std::size_t>(index)], 0, index, equippedItemCount),
                equippedItemCount,
            });
        }
    }

    const int itemRingIndex = std::clamp(baseStoryRingDemo_.itemRingIndex, 0, SpellRingCount - 1);
    if (visibleRingCount > itemRingIndex && !baseStoryRingDemo_.itemObjectId.empty()) {
        if (objectCatalog_.registry.findById(baseStoryRingDemo_.itemObjectId) != nullptr) {
            SpellRingItem item = makeObjectRingItem(baseStoryRingDemo_.itemObjectId);
            const RingShape itemRingShape = spellRing_.ringShapeForIndex(itemRingIndex);
            item.localAngle = itemRingShape == RingShape::Comet
                ? normalizeLocalParam(itemRingShape, Pi, makeRingOrbitTuning(balance_))
                : normalizeAngle(baseActorIdleAnimationTime_ * 1.35f);
            demoItems.push_back({
                preparedDemoRingItem(item, itemRingIndex, 0, 1),
                1,
            });
        }
    }

    std::stable_sort(
        demoItems.begin(),
        demoItems.end(),
        [](const DemoRingItem& left, const DemoRingItem& right) {
            return left.item.worldPosition.y < right.item.worldPosition.y;
        });

    for (const DemoRingItem& demoItem : demoItems) {
        const SpellRingItem& item = demoItem.item;
        const float itemScale = spellRing_.ringShapeForIndex(item.ringIndex) == RingShape::Comet
            ? std::clamp(1.0f - std::max(0, demoItem.ringItemCount - 10) * 0.014f, 0.76f, 1.0f)
            : 1.0f;
        const Vec2 drawPosition = ringItemDrawPosition(item, baseActorIdleAnimationTime_);
        renderer.drawActorShadow(
            item.worldPosition + Vec2{0.0f, 18.0f},
            ringItemShadowVisualSize(item, baseActorIdleAnimationTime_) * itemScale * radiusScale,
            {0, 0, 0, alphaByte(68.0f * alpha)});

        ObjectImageDrawOptions options;
        options.tint = withAlpha({255, 255, 255, 255}, 255.0f * alpha);
        options.outlineColor.a = alphaByte(210.0f * alpha);
        options.outlineEnabled = true;
        options.outlinePx = ArtworkOutlinePx;
        const bool drewImage = drawRingItemObjectImage(
            renderer,
            item,
            objectForRingItem(objectCatalog_, item),
            drawPosition,
            {RingObjectImageMaxSize * itemScale, RingObjectImageMaxSize * itemScale},
            item.orbitOutward,
            item.orbitTangent,
            baseActorIdleAnimationTime_,
            options);
        if (drewImage) {
            continue;
        }

        const float fallbackRadius = item.hitRadius * itemScale;
        if (item.type == SpellRingItemType::Shovel) {
            renderer.fillCircle(drawPosition, fallbackRadius, withAlpha({178, 184, 190, 255}, 255.0f * alpha));
            renderer.drawLine(drawPosition, drawPosition + item.orbitOutward * (15.0f * itemScale), withAlpha({90, 96, 102, 255}, 255.0f * alpha));
        } else if (item.type == SpellRingItemType::Torch) {
            renderer.fillCircle(drawPosition, fallbackRadius, withAlpha({242, 122, 25, 255}, 255.0f * alpha));
            renderer.fillCircle(drawPosition + Vec2{2.0f, -2.0f} * itemScale, 4.0f * itemScale, withAlpha({255, 238, 98, 255}, 255.0f * alpha));
        } else {
            renderer.fillCircle(drawPosition, fallbackRadius, withAlpha({96, 122, 210, 255}, 255.0f * alpha));
            renderer.drawCircle(drawPosition, fallbackRadius + 3.0f, withAlpha({160, 202, 255, 255}, 255.0f * alpha));
        }
    }
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

void Game::updateBaseStorySpeakerFacing()
{
    const std::string_view speakerId = dialogue_.currentSpeakerId();
    if (!baseStorySpeakerTurnsPlayer(speakerId) || !basePresentationActive()) {
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

    const BaseFacility* speaker = findBaseFacilityById(facilities, speakerId);
    if (speaker == nullptr || !speaker->enabled || baseFacilityHiddenInNormalView(baseArea_, *speaker)) {
        return;
    }

    const BaseCharacterSpriteVisual* spriteVisual = baseCharacterSpriteVisual(baseArea_, speaker->facilityId);
    if (spriteVisual == nullptr) {
        return;
    }

    const UiRect visualRect = baseCharacterSpriteVisualRect(*speaker);
    Vec2 speakerPosition = visualRect.pos + visualRect.size * 0.5f;
    if (const NpcCharacterVisual* visual = findNpcCharacterVisual(spriteVisual->visualId)) {
        speakerPosition = visualRect.pos + Vec2{
            visualRect.size.x * visual->anchor.x,
            visualRect.size.y * visual->anchor.y,
        };
    }

    const float dx = speakerPosition.x - basePlayerPosition_.x;
    if (std::abs(dx) <= CharacterSpriteHorizontalFacingEpsilon) {
        return;
    }

    basePlayerFacing_ = {dx < 0.0f ? -1.0f : 1.0f, 0.0f};
    updateBasePlayerSpriteFlipFromFacing();
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

        if (command->name == "base_return_scene") {
            const std::string_view mode = baseReturnSceneMode(*command);
            if (baseReturnSceneModeIsBegin(mode)) {
                applyBaseReturnSceneBeginPlacement();
                baseStoryCommand_.startPosition = basePlayerPosition_;
                baseStoryCommand_.startFacing = lengthSquared(basePlayerFacing_) > 0.0001f
                    ? normalize(basePlayerFacing_)
                    : Vec2{0.0f, 1.0f};
                baseStoryCommand_.targetPosition = baseStoryCommand_.startPosition +
                    storyTileOffset(BaseReturnSceneFirstWalkTilesX, BaseReturnSceneFirstWalkTilesY) +
                    storyTileOffset(BaseReturnSceneSecondWalkTilesX, BaseReturnSceneSecondWalkTilesY);
                baseStoryCommand_.targetFacing = {0.0f, -1.0f};
            } else if (baseReturnSceneModeIsEnd(mode)) {
                baseStoryCommand_.targetPosition = baseStoryCommand_.startPosition;
                baseStoryCommand_.targetFacing = baseStoryCommand_.startFacing;
            } else {
                logWarning("[story] unknown base_return_scene mode: " + std::string(mode));
                dialogue_.completeCurrentCommandStep();
                baseStoryCommand_ = {};
                return;
            }
        }

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

        if (command->name == "base_facility_marker") {
            if (command->args.empty() || command->args[0] == "clear") {
                baseStoryMarkedFacilities_.clear();
            } else if (command->args.size() >= 2 && storyCommandArgIsOff(command->args[1])) {
                baseStoryMarkedFacilities_.erase(command->args[0]);
            } else {
                baseStoryMarkedFacilities_[command->args[0]] = baseActorIdleAnimationTime_;
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
        } else if (command->name == "base_chicory_figure8") {
            const Vec2 playerFoot = playerSpriteFootAnchor(basePlayerPosition_);
            baseStoryCommand_.targetPosition = baseStoryCommand_.startPosition;
            baseStoryCommand_.targetFacing = baseStoryCommand_.startFacing;
            baseStoryChicoryFlight_ = {};
            baseStoryChicoryFlight_.active = true;
            baseStoryChicoryFlight_.durationSeconds = std::max(
                0.1f,
                parseStoryCommandFloat(*command, 0, BaseStoryChicoryFlightSeconds));
            baseStoryChicoryFlight_.startPosition = playerFoot + Vec2{0.0f, -96.0f};
            baseStoryChicoryFlight_.centerPosition = basePlayerPosition_ + Vec2{0.0f, -22.0f};
        } else if (command->name == "base_ring_demo") {
            baseStoryCommand_.targetPosition = baseStoryCommand_.startPosition;
            baseStoryCommand_.targetFacing = baseStoryCommand_.startFacing;
            const std::string mode = command->args.empty() ? std::string("open") : command->args[0];
            if (mode == "close") {
                if (!baseStoryRingDemo_.active) {
                    baseStoryRingDemo_.active = true;
                    baseStoryRingDemo_.visibleRingCount = 2;
                    baseStoryRingDemo_.itemObjectId = std::string(BaseStoryRingDemoDefaultItemObjectId);
                }
                baseStoryRingDemo_.closing = true;
                baseStoryRingDemo_.elapsedSeconds = 0.0f;
                baseStoryRingDemo_.durationSeconds = std::max(
                    0.05f,
                    parseStoryCommandFloat(*command, 1, BaseStoryRingDemoCloseSeconds));
            } else {
                baseStoryRingDemo_ = {};
                baseStoryRingDemo_.active = true;
                baseStoryRingDemo_.closing = false;
                baseStoryRingDemo_.durationSeconds = std::max(
                    0.05f,
                    parseStoryCommandFirstFloatFrom(*command, 3, BaseStoryRingDemoOpenSeconds));
                baseStoryRingDemo_.visibleRingCount = std::clamp(
                    static_cast<int>(std::lround(parseStoryCommandFloat(*command, 1, 2.0f))),
                    1,
                    SpellRingCount);
                const int itemRingNumber = parseStoryCommandNamedInt(*command, "item_ring", 2);
                baseStoryRingDemo_.itemRingIndex = std::clamp(itemRingNumber - 1, 0, SpellRingCount - 1);
                baseStoryRingDemo_.itemObjectId = command->args.size() >= 3 && !command->args[2].empty()
                    ? command->args[2]
                    : std::string(BaseStoryRingDemoDefaultItemObjectId);
                playAudioSe(AudioSeRingAppear);
            }
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

    if (command->name == "base_return_scene") {
        const std::string_view mode = baseReturnSceneMode(*command);
        if (baseReturnSceneModeIsBegin(mode)) {
            const float waitSeconds = BaseReturnSceneWaitSeconds;
            const float firstWalkSeconds = BaseReturnSceneFirstWalkSeconds;
            const float secondWalkSeconds = BaseReturnSceneSecondWalkSeconds;
            const float totalSeconds = waitSeconds + firstWalkSeconds + secondWalkSeconds;
            const Vec2 start = baseStoryCommand_.startPosition;
            const Vec2 firstEnd = start + storyTileOffset(BaseReturnSceneFirstWalkTilesX, BaseReturnSceneFirstWalkTilesY);
            const Vec2 finalEnd = firstEnd + storyTileOffset(BaseReturnSceneSecondWalkTilesX, BaseReturnSceneSecondWalkTilesY);

            bool walking = false;
            if (baseStoryCommand_.elapsedSeconds < waitSeconds) {
                basePlayerPosition_ = start;
                basePlayerFacing_ = baseStoryCommand_.startFacing;
            } else if (baseStoryCommand_.elapsedSeconds < waitSeconds + firstWalkSeconds) {
                const float t = clamp((baseStoryCommand_.elapsedSeconds - waitSeconds) / firstWalkSeconds, 0.0f, 1.0f);
                basePlayerPosition_ = lerp(start, firstEnd, t);
                basePlayerFacing_ = normalize(firstEnd - start);
                walking = true;
            } else {
                const float t = clamp(
                    (baseStoryCommand_.elapsedSeconds - waitSeconds - firstWalkSeconds) / secondWalkSeconds,
                    0.0f,
                    1.0f);
                basePlayerPosition_ = lerp(firstEnd, finalEnd, t);
                basePlayerFacing_ = normalize(finalEnd - firstEnd);
                walking = t < 1.0f;
            }

            updateBasePlayerSpriteAnimation(safeDt, walking);
            updateBasePlayerSpriteFlipFromFacing();
            if (baseStoryCommand_.elapsedSeconds >= totalSeconds) {
                basePlayerPosition_ = finalEnd;
                basePlayerFacing_ = normalize(finalEnd - firstEnd);
                basePlayerSpriteWalking_ = false;
                dialogue_.completeCurrentCommandStep();
                baseStoryCommand_ = {};
            }
            return;
        }

        if (baseReturnSceneModeIsEnd(mode)) {
            const float duration = std::max(0.001f, parseStoryCommandFloat(*command, 1, ScreenTransitionFadeOutSeconds));
            const float totalSeconds = duration * 2.0f;
            if (baseStoryCommand_.elapsedSeconds < duration) {
                baseStoryFadeAlpha_ = smoothStep01(baseStoryCommand_.elapsedSeconds / duration);
            } else {
                baseStoryFacilityOffsets_.erase("elder");
                baseStoryFacilityOffsets_.erase("monica");
                baseStoryFadeAlpha_ = 1.0f - smoothStep01((baseStoryCommand_.elapsedSeconds - duration) / duration);
            }

            updateBasePlayerSpriteAnimation(safeDt, false);
            updateBasePlayerSpriteFlipFromFacing();
            if (baseStoryCommand_.elapsedSeconds >= totalSeconds) {
                baseStoryFadeAlpha_ = 0.0f;
                dialogue_.completeCurrentCommandStep();
                baseStoryCommand_ = {};
            }
            return;
        }
    }

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

    if (command->name == "base_chicory_figure8") {
        updateBasePlayerSpriteAnimation(safeDt, false);
        updateBasePlayerSpriteFlipFromFacing();
        baseStoryChicoryFlight_.elapsedSeconds = baseStoryCommand_.elapsedSeconds;
        if (baseStoryCommand_.elapsedSeconds >= baseStoryChicoryFlight_.durationSeconds) {
            baseStoryChicoryFlight_ = {};
            dialogue_.completeCurrentCommandStep();
            baseStoryCommand_ = {};
        }
        return;
    }

    if (command->name == "base_ring_demo") {
        updateBasePlayerSpriteAnimation(safeDt, false);
        updateBasePlayerSpriteFlipFromFacing();
        baseStoryRingDemo_.elapsedSeconds = baseStoryCommand_.elapsedSeconds;
        if (baseStoryCommand_.elapsedSeconds >= baseStoryRingDemo_.durationSeconds) {
            if (baseStoryRingDemo_.closing) {
                baseStoryRingDemo_ = {};
            } else {
                baseStoryRingDemo_.elapsedSeconds = baseStoryRingDemo_.durationSeconds;
            }
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
        clearMerchantBulkSellState();
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
    clearMerchantBulkSellState();
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
        baseStatus_ = "この強化枠は未実装だよ";
        return;
    }
    if (upgradeMaxed(index)) {
        baseStatus_ = "強化上限だよ";
        return;
    }
    const int cost = upgradeCost(index);
    if (cost <= 0) {
        return;
    }
    if (money_ < cost) {
        baseStatus_ = "所持金が足りないよ";
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
    playAudioSe(AudioSeForgeUpgrade);
    if (index == 3 && beforeLevel == 0 && afterLevel > beforeLevel) {
        requestBaseAreaFade(
            BaseArea::Outdoor,
            baseHomeScreenDefaultPosition(balance_.playerRadius),
            {0.0f, 1.0f},
            "リング工房を建設したよ",
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
        baseStatus_ = "リング工房はまだ解禁されていないよ";
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
    ringWorkshopRadiusSliderState().dismissValue();
    ringWorkshopRespecConfirmState() = {};
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
        baseStatus_ = "再調整できるリング強化ポイントがないよ";
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
        baseStatus_ = ringDisplayNameWithSpaceSuffix(
            from.ringIndex,
            unlockedRingCount(),
            ringLevelUpgradeKindName(from.kind)) + "から移せるポイントがないよ";
        return false;
    }
    --fromPoints;
    ++toPoints;
    ringWorkshopRespecSource_.reset();
    baseStatus_ = "配分案を変更したよ";
    return true;
}

bool Game::openRingWorkshopRespecConfirm()
{
    if (!ringWorkshopRespecChanged()) {
        baseStatus_ = "配分は変更されていないよ";
        return false;
    }

    const int moneyCost = ringWorkshopRespecMoneyCost();
    const int moonCost = ringWorkshopRespecMoonCost();
    const bool executable =
        money_ >= moneyCost &&
        inventory_.materialCount(MaterialType::MoonFragment) >= moonCost;
    UiConfirmDialogState& confirm = ringWorkshopRespecConfirmState();
    openUiConfirmDialog(
        confirm,
        "再調整の確認",
        "",
        "再調整する",
        "戻る",
        executable ? 0 : 1);
    confirm.confirmState = uiButtonState(executable);
    ringWorkshopRespecSource_.reset();
    baseStatus_.clear();
    return true;
}

void Game::applyRingWorkshopRespec()
{
    if (!ringWorkshopRespecChanged()) {
        baseStatus_ = "配分は変更されていないよ";
        return;
    }
    const int moneyCost = ringWorkshopRespecMoneyCost();
    const int moonCost = ringWorkshopRespecMoonCost();
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りないよ";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りないよ";
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
    playAudioSe(AudioSeRingWorkshopRespec);
    baseStatus_ = "リング強化の配分を再調整したよ";
}

void Game::drawRingWorkshopRespecConfirmDialog(Renderer& renderer, UiRect panel) const
{
    const UiConfirmDialogState& confirm = ringWorkshopRespecConfirmState();
    if (!confirm.open) {
        return;
    }

    UiModalNavigationScope navigationScope(panel);
    UiWindowScope window(
        renderer,
        "base.ring_workshop.respec.confirm",
        panel,
        confirm.title,
        uiConfirmDialogHelpText(),
        UiWindowOptions{true, true});

    const UiRect body = baseActionConfirmBodyRect(panel);
    float y = body.pos.y;
    renderer.drawText({body.pos.x, y}, "この内容で再調整を実行する？", ui::Text, 2);
    y += renderer.measureText("この内容で再調整を実行する？", 2).y + 22.0f;

    constexpr std::size_t VisibleChangeRows = 2;
    std::vector<ConfirmPreviewRow> previewRows;
    int changedItemCount = 0;
    const int ringCount = unlockedRingCount();
    for (int ringIndex = 0; ringIndex < ringCount; ++ringIndex) {
        const RingLevelUpgradePoints& current = levelRingUpgradePoints_[static_cast<std::size_t>(ringIndex)];
        const RingLevelUpgradePoints& draft = ringWorkshopDraftUpgradePoints_[static_cast<std::size_t>(ringIndex)];
        for (int kindIndex = 0; kindIndex < RingLevelUpgradeKindCount; ++kindIndex) {
            const RingLevelUpgradeKind kind = ringWorkshopKindForIndex(kindIndex);
            const int currentPoints = ringLevelUpgradePoint(current, kind);
            const int draftPoints = ringLevelUpgradePoint(draft, kind);
            if (currentPoints == draftPoints) {
                continue;
            }
            ++changedItemCount;
            if (previewRows.size() < VisibleChangeRows) {
                previewRows.push_back({
                    ringDisplayNameWithSpaceSuffix(ringIndex, ringCount, ringLevelUpgradeKindName(kind)),
                    std::to_string(currentPoints) + "点",
                    std::to_string(draftPoints) + "点",
                });
            }
        }
    }
    for (const ConfirmPreviewRow& row : previewRows) {
        drawConfirmPreviewRow(renderer, body, y, row);
    }
    if (changedItemCount > static_cast<int>(previewRows.size())) {
        renderer.drawText(
            {body.pos.x, y},
            "ほか" + std::to_string(changedItemCount - static_cast<int>(previewRows.size())) + "項目を変更",
            ui::TextMuted,
            2);
        y += 31.0f;
    }

    const std::vector<RequirementRow> requirements{
        moneyRequirementRow(ringWorkshopRespecMoneyCost(), money_),
        materialRequirementRow(
            MaterialType::MoonFragment,
            ringWorkshopRespecMoonCost(),
            inventory_.materialCount(MaterialType::MoonFragment)),
    };
    drawRequirementSubWindow(
        renderer,
        objectCatalog_,
        baseActionConfirmRequirementRect(panel, body, y, requirements.size()),
        requirements);
    drawUiConfirmDialogButtons(renderer, confirm, panel);
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
        return "リングアイテム数増加";
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

bool Game::ringWorkshopUpgradeExecutable(RingWorkshopUpgrade upgrade) const
{
    return upgrade != RingWorkshopUpgrade::RadiusAdjust &&
        ringWorkshopUpgradeLevel(upgrade) < ringWorkshopUpgradeMaxLevel(upgrade) &&
        money_ >= ringWorkshopUpgradeMoneyCost(upgrade) &&
        inventory_.materialCount(MaterialType::MoonFragment) >= ringWorkshopUpgradeMoonCost(upgrade);
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
        ringDisplayNameWithSuffix(ringIndex, unlockedRingCount(), "の") +
        ringWorkshopUpgradeName(upgrade) + "を強化したよ"));
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
        baseStatus_ = "リング半径を調整したよ";
    }
    return changed;
}

void Game::buyRingWorkshopUpgrade(RingWorkshopUpgrade upgrade)
{
    if (upgrade == RingWorkshopUpgrade::RadiusAdjust) {
        baseStatus_ = "半径はゲージで調整してね";
        return;
    }
    if (ringWorkshopUpgradeLevel(upgrade) >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        baseStatus_ = "この強化は上限だよ";
        return;
    }
    const int moneyCost = ringWorkshopUpgradeMoneyCost(upgrade);
    const int moonCost = ringWorkshopUpgradeMoonCost(upgrade);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りないよ";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りないよ";
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
    playAudioSe(AudioSeRingWorkshopUpgrade);
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

    encyclopedia_.noteEffectEvents(discoveries, objectCatalog_);
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
            (void)spellRing_.consumeItemDurability(item, FullPointDurabilityCostUnits);

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
    if (basePresentationActive() &&
        !event->dialogue.steps.empty() &&
        isBaseReturnSceneBeginStep(event->dialogue.steps.front())) {
        applyBaseReturnSceneBeginPlacement();
    }
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
            if (!ui.navigationActive() || ui.navigationFocusRole() == UiNavigationRole::Grid) {
                const int dx =
                    (input.pressed(InputAction::MoveRight) ? 1 : 0) -
                    (input.pressed(InputAction::MoveLeft) ? 1 : 0);
                const int dy =
                    (input.pressed(InputAction::MoveDown) ? 1 : 0) -
                    (input.pressed(InputAction::MoveUp) ? 1 : 0);
                nextSelection = moveUiGridSelection(nextSelection, itemCount, columns, dx, dy);
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
            if (ui.selectionFocused(rect)) {
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
            if (ui.selectionFocused(rect)) {
                bookshelfSelection_ = i;
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
    const UiRect panel = baseDiaryPanelRect();
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
    if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp)) {
        baseDiarySelection_ = 1;
    }
    if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown)) {
        baseDiarySelection_ = 0;
    }

    const auto saveDiary = [this, &ui]() {
        std::string message;
        if (saveSaveData(message)) {
            playAudioSe(AudioSeDiarySave);
            baseDiaryMode_ = BaseDiaryMode::Saved;
            baseDiarySelection_ = 0;
            baseDiaryMessage_ = "保存したよ";
            baseDiarySummary_ = currentDiarySaveSummary();
        } else {
            ui.rejectAction();
            baseDiaryMode_ = BaseDiaryMode::Error;
            baseDiarySelection_ = 0;
            baseDiaryMessage_ = message.empty() ? "セーブに失敗したよ" : message;
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

    if (baseStorageBatchSelection_.confirm.open) {
        const UiRect confirmPanel = batchItemConfirmRect();
        const bool withdrawing = baseStorageMode_ == StorageUiMode::Withdraw;
        const StorageBatchTransferSummary summary = withdrawing
            ? storageBulkWithdrawSummary()
            : storageBulkDepositSummary();
        baseStorageBatchSelection_.confirm.confirmState = uiButtonState(
            summary.selectedCount > 0 && summary.fits());
        const UiConfirmDialogResult result = updateUiConfirmDialog(
            baseStorageBatchSelection_.confirm,
            ui,
            input,
            confirmPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            const bool succeeded = withdrawing
                ? withdrawStorageBulkSelection()
                : depositStorageBulkSelection();
            ui.emitActionResult(succeeded);
            if (!succeeded) {
                openStorageBatchTransferFailureDialog(
                    baseResultDialog_,
                    withdrawing,
                    baseStatus_);
            }
        }
        ui.block(confirmPanel);
        return;
    }

    if (baseMerchantBulkSell_.confirm.open) {
        const UiRect confirmPanel = batchItemConfirmRect();
        baseMerchantBulkSell_.confirm.confirmState = uiButtonState(merchantBulkSellSummary().itemCount > 0);
        const UiConfirmDialogResult result = updateUiConfirmDialog(
            baseMerchantBulkSell_.confirm,
            ui,
            input,
            confirmPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            ui.emitActionResult(sellMerchantBulkSelection());
        }
        ui.block(confirmPanel);
        return;
    }

    if (baseResultDialog_.open) {
        const UiRect resultPanel = baseResultDialogRect();
        updateUiResultDialog(baseResultDialog_, ui, input, resultPanel);
        ui.block(resultPanel);
        return;
    }

    if (baseQuantityDialog_.open) {
        const UiRect quantityPanel = baseQuantityDialogRect({
            static_cast<float>(camera_.width()),
            static_cast<float>(camera_.height()),
        });
        const UiQuantityDialogResult quantityResult = updateUiQuantityDialog(baseQuantityDialog_, ui, input, quantityPanel);
        if (quantityResult == UiQuantityDialogResult::Confirmed) {
            const int quantity = baseQuantityDialog_.value;
            const BaseQuantityPending pending = baseQuantityPending_;
            baseQuantityPending_ = {};
            bool succeeded = false;
            if (pending.operation == BaseQuantityOperation::StorageDeposit) {
                succeeded = depositStorageTarget(pending.target, quantity);
            } else if (pending.operation == BaseQuantityOperation::StorageWithdraw) {
                succeeded = withdrawStorageTarget(pending.target, quantity);
            } else if (pending.operation == BaseQuantityOperation::MerchantBuy) {
                succeeded = buyMerchantProduct(pending.merchantProductIndex, quantity);
            } else if (pending.operation == BaseQuantityOperation::MerchantSell) {
                succeeded = sellMerchantTarget(pending.target, quantity);
            }
            ui.emitActionResult(succeeded);
        } else if (quantityResult == UiQuantityDialogResult::Cancelled) {
            baseQuantityPending_ = {};
        }
        ui.block(quantityPanel);
        return;
    }

    const auto handleBaseItemGridInteraction =
        [this, &ui](const ItemGridInteractionResult& interaction, auto&& activate) {
            switch (interaction.event) {
            case ItemGridInteractionEvent::None:
                break;
            case ItemGridInteractionEvent::Activate:
                activate(interaction.slotIndex);
                break;
            case ItemGridInteractionEvent::GrabStarted:
                baseStatus_ = "つかみ中";
                ui.emitSound(UiSoundEvent::ItemMove);
                break;
            case ItemGridInteractionEvent::MoveRequested:
                if (moveItemKeyToGridPlacement(interaction.item, interaction.placement)) {
                    baseStatus_ =
                        interaction.destinationOccupied &&
                            interaction.originPlacement != interaction.placement
                        ? "入れ替えました"
                        : "配置したよ";
                    ui.emitSound(UiSoundEvent::ItemMove);
                } else {
                    baseStatus_ = "配置できないよ";
                    ui.rejectAction();
                }
                break;
            case ItemGridInteractionEvent::ProtectionRequested:
            {
                const ItemProtectionToggleResult result =
                    toggleItemProtection(interaction.item);
                if (result == ItemProtectionToggleResult::Changed) {
                    baseStatus_ = itemProtectionEnabled(interaction.item).value_or(false)
                        ? "保護ON"
                        : "保護OFF";
                    ui.emitSound(UiSoundEvent::Confirm);
                } else if (result == ItemProtectionToggleResult::Unsupported) {
                    baseStatus_ = "個体アイテムのみ保護できます";
                    ui.rejectAction();
                } else {
                    baseStatus_ = "アイテム未選択";
                    ui.rejectAction();
                }
                break;
            }
            case ItemGridInteractionEvent::ProtectionBlocked:
                baseStatus_ = "つかみ中は保護変更できないよ";
                ui.rejectAction();
                break;
            case ItemGridInteractionEvent::GrabCancelled:
                baseStatus_ = interaction.item.valid()
                    ? "配置をキャンセルしたよ"
                    : "アイテム未選択";
                if (interaction.item.valid()) {
                    ui.emitSound(UiSoundEvent::Cancel);
                } else {
                    ui.rejectAction();
                }
                break;
            }
            return interaction.consumed;
        };

    if (baseProcessingConfirm_.open) {
        const UiRect confirmPanel = baseProcessingConfirmRect();
        baseProcessingConfirm_.confirmState = uiButtonState(processingCommandExecutable(
            baseProcessingConfirmTarget_,
            baseProcessingConfirmMode_));
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

    UiConfirmDialogState& ringWorkshopRespecConfirm = ringWorkshopRespecConfirmState();
    if (ringWorkshopRespecConfirm.open) {
        const UiRect confirmPanel = baseProcessingConfirmRect();
        const bool executable =
            baseRingWorkshopActive_ &&
            baseRingWorkshopMode_ == RingWorkshopMode::Respec &&
            ringWorkshopRespecChanged() &&
            money_ >= ringWorkshopRespecMoneyCost() &&
            inventory_.materialCount(MaterialType::MoonFragment) >= ringWorkshopRespecMoonCost();
        ringWorkshopRespecConfirm.confirmState = uiButtonState(executable);
        const UiConfirmDialogResult result = updateUiConfirmDialog(
            ringWorkshopRespecConfirm,
            ui,
            input,
            confirmPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            applyRingWorkshopRespec();
        } else if (result == UiConfirmDialogResult::Cancelled) {
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
            ringWorkshopRadiusSliderState().dismissValue();
            ringWorkshopRespecConfirmState() = {};
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
            ringWorkshopRadiusSliderState().dismissValue();
            ringWorkshopRespecConfirmState() = {};
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
            if (!ui.navigationActive()) {
                if (input.pressed(InputAction::MoveUp)) {
                    baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + RingWorkshopActionCount - 1) % RingWorkshopActionCount;
                }
                if (input.pressed(InputAction::MoveDown)) {
                    baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + 1) % RingWorkshopActionCount;
                }
            }
            for (int i = 0; i < RingWorkshopActionCount; ++i) {
                const UiRect rect = ringWorkshopActionChoiceRect(i);
                if (ui.navigationFocused(rect)) {
                    baseRingWorkshopSelection_ = i;
                }
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
                ringTabLabels[static_cast<std::size_t>(i)] = ringDisplayName(i, ringCount);
                ringTabs[static_cast<std::size_t>(i)] = {
                    ringTabLabels[static_cast<std::size_t>(i)],
                    true,
                    ringDisplayIconImageNumber(i),
                };
                ringTabRects[static_cast<std::size_t>(i)] = ringWorkshopRingTabRect(i, ringCount);
            }
            const UiTabsInput ringTabsInput = ringWorkshopRingTabsInput(input, ringCount);
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
            if (!ui.navigationActive()) {
                if (input.pressed(InputAction::MoveUp)) {
                    --move;
                }
                if (input.pressed(InputAction::MoveDown)) {
                    ++move;
                }
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
                        ui.rejectAction();
                        baseStatus_ = "移動元にできるポイントがないよ";
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
            if (baseRingWorkshopUpgradeTabs_.navigationFocused) {
                baseRingWorkshopSelection_ = baseRingWorkshopUpgradeTabs_.focusedIndex;
            }
            if (selectedTab >= 0) {
                baseRingWorkshopSelection_ = selectedTab;
                chooseRespecKind(selectedTab);
                ui.block(workshopBounds);
                return;
            }
            const UiRect confirmRect = ringWorkshopRespecConfirmRect();
            if (ui.navigationFocused(confirmRect)) {
                baseRingWorkshopSelection_ = RingLevelUpgradeKindCount;
            }
            if (updateClickSelection(ui, confirmRect, RingLevelUpgradeKindCount, baseRingWorkshopSelection_)) {
                ui.emitActionResult(openRingWorkshopRespecConfirm());
                ui.block(workshopBounds);
                return;
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                if (baseRingWorkshopSelection_ == RingLevelUpgradeKindCount) {
                    ui.emitActionResult(openRingWorkshopRespecConfirm());
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
                ringTabLabels[static_cast<std::size_t>(i)] = ringDisplayName(i, ringCount);
                ringTabs[static_cast<std::size_t>(i)] = {
                    ringTabLabels[static_cast<std::size_t>(i)],
                    true,
                    ringDisplayIconImageNumber(i),
                };
                ringTabRects[static_cast<std::size_t>(i)] = ringWorkshopRingTabRect(i, ringCount);
            }
            const UiTabsInput ringTabsInput = ringWorkshopRingTabsInput(input, ringCount);
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
            if (!ui.navigationActive()) {
                if (input.pressed(InputAction::MoveUp)) {
                    --move;
                }
                if (input.pressed(InputAction::MoveDown)) {
                    ++move;
                }
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
                UiSliderState& radiusSliderState = ringWorkshopRadiusSliderState();
                UiSliderResult radiusResult{
                    ringWorkshopRadiusSettingForRing(ringIndex),
                    false,
                    false,
                };
                if (radiusSliderState.dragging() || scrollViewport.contains(ui.mouse())) {
                    radiusResult = updateUiSlider(
                        ui,
                        input,
                        ringWorkshopRadiusSliderRect(scrollLayout),
                        radiusResult.value,
                        ringWorkshopRadiusSliderSpec(minMeters, maxMeters),
                        radiusSliderState);
                } else if (input.mouseLeftPressed()) {
                    radiusSliderState.dismissValue();
                }
                if (radiusResult.changed) {
                    setRingWorkshopRadiusSettingForRing(ringIndex, radiusResult.value);
                }
                if (radiusResult.interacting) {
                    ui.block(workshopBounds);
                    return;
                }
            } else {
                ringWorkshopRadiusSliderState().dismissValue();
            }

            const auto chooseUpgradeItem = [this, &ui](int item) {
                const bool implemented = item >= 0 && item < RingWorkshopImplementedUpgradeCount;
                const RingWorkshopUpgrade upgrade = ringWorkshopUpgradeForDisplayIndex(item);
                const UiButtonState buttonState = uiButtonState(
                    implemented && ringWorkshopUpgradeExecutable(upgrade));
                if (tryActivateUiButton(ui, buttonState)) {
                    ui.emitSound(UiSoundEvent::Confirm);
                    buyRingWorkshopUpgrade(upgrade);
                    return;
                }
                if (implemented) {
                    buyRingWorkshopUpgrade(upgrade);
                } else {
                    baseStatus_ = "この項目は未解禁だよ";
                }
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
            if (baseRingWorkshopUpgradeTabs_.navigationFocused) {
                baseRingWorkshopSelection_ = baseRingWorkshopUpgradeTabs_.focusedIndex;
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
                ui.setNavigationFocus(ringWorkshopUpgradeItemRect(scrollLayout, baseRingWorkshopSelection_));
            }
            if (selectedTab >= 0) {
                baseRingWorkshopSelection_ = selectedTab;
                if (!ui.navigationActive()) {
                    ui.block(workshopBounds);
                    return;
                }
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
            ? (baseStorageMode_ == StorageUiMode::Bulk
                ? storageBulkDialogRect(storageBulkActionCount())
                : storageActionDialogRect())
            : merchantPanelRect();
        const auto resetStoragePointerPress = [this]() {
            baseItemInteraction_.cancelPointer();
            if (baseRingItemInteraction_.pointerPending ||
                baseRingItemInteraction_.pointerDragging) {
                (void)cancelBaseRingItemInteraction(true);
            }
        };
        const auto closeStorageCommand = [this]() {
            closeUiCommandMenu(baseStorageCommandMenu_);
            baseStorageCommandOperation_ = StorageQuantityOperation::None;
            baseStorageCommandTarget_ = {};
        };
        const auto closeStorage = [this, &closeStorageCommand, &resetStoragePointerPress]() {
            closeStorageCommand();
            resetStoragePointerPress();
            clearBaseItemInteractions();
            baseStorageActive_ = false;
            baseStorageMode_ = StorageUiMode::Closed;
            baseQuantityDialog_ = {};
            baseQuantityPending_ = {};
            clearStorageBatchSelectionState();
            baseStatus_.clear();
        };
        const auto returnToStorageMenu = [this, &closeStorageCommand, &resetStoragePointerPress]() {
            closeStorageCommand();
            resetStoragePointerPress();
            clearBaseItemInteractions();
            baseStorageMode_ = StorageUiMode::ChooseAction;
            baseStorageActionSelection_ = 0;
            baseStorageBulkSelection_ = 0;
            baseQuantityDialog_ = {};
            baseQuantityPending_ = {};
            clearStorageBatchSelectionState();
            baseStatus_.clear();
        };
        const auto openQuantityDialog = [this](StorageQuantityOperation operation, StorageTransferTarget target, int maxCount) {
            baseQuantityPending_.operation = operation == StorageQuantityOperation::Deposit
                ? BaseQuantityOperation::StorageDeposit
                : BaseQuantityOperation::StorageWithdraw;
            baseQuantityPending_.target = target;
            baseQuantityPending_.merchantProductIndex = -1;
            openUiQuantityDialog(
                baseQuantityDialog_,
                operation == StorageQuantityOperation::Deposit ? "しまう個数" : "取り出す個数",
                1,
                std::max(1, maxCount),
                std::max(1, maxCount),
                "個");
            baseStatus_.clear();
        };
        const auto applyStorageTarget = [this, &openQuantityDialog, &ui](StorageQuantityOperation operation, StorageTransferTarget target) {
            if (!storageTransferTargetAvailable(target)) {
                if (operation == StorageQuantityOperation::Deposit &&
                    target.source == BaseItemSource::Backpack) {
                    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                        if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                            baseStatus_ = "装備中の杖はしまえないよ";
                            ui.rejectAction();
                            return;
                        }
                    }
                }
                if (operation == StorageQuantityOperation::Deposit &&
                    target.valid &&
                    target.source != BaseItemSource::Backpack &&
                    target.source != BaseItemSource::Warehouse) {
                    baseStatus_ = "このアイテムはしまえないよ";
                } else {
                    baseStatus_ = operation == StorageQuantityOperation::Deposit
                        ? "しまうアイテムがないよ"
                        : "取り出すアイテムがないよ";
                }
                ui.rejectAction();
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
                ui.emitActionResult(depositStorageTarget(target, 1));
            } else {
                ui.emitActionResult(withdrawStorageTarget(target, 1));
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
                ui.rejectAction();
                if (operation == StorageQuantityOperation::Deposit &&
                    target.source == BaseItemSource::Backpack) {
                    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                        if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                            baseStatus_ = "装備中の杖はしまえないよ";
                            closeStorageCommand();
                            return;
                        }
                    }
                }
                if (operation == StorageQuantityOperation::Deposit &&
                    target.valid &&
                    target.source != BaseItemSource::Backpack &&
                    target.source != BaseItemSource::Warehouse) {
                    baseStatus_ = "このアイテムはしまえないよ";
                } else {
                    baseStatus_ = operation == StorageQuantityOperation::Deposit
                        ? "しまうアイテムがないよ"
                        : "取り出すアイテムがないよ";
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
        const auto moveGridSelection = [&input, &ui](int& selection, int slotCount) {
            const int count = std::max(1, slotCount);
            selection = std::clamp(selection, 0, count - 1);
            if (!ui.navigationActive() || ui.navigationFocusRole() == UiNavigationRole::Grid) {
                const int dx =
                    (input.pressed(InputAction::MoveRight) ? 1 : 0) -
                    (input.pressed(InputAction::MoveLeft) ? 1 : 0);
                const int dy =
                    (input.pressed(InputAction::MoveDown) ? 1 : 0) -
                    (input.pressed(InputAction::MoveUp) ? 1 : 0);
                selection = moveUiGridSelection(selection, count, StorageColumns, dx, dy);
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
            } else if (baseItemInteraction_.grabActive() ||
                baseRingItemInteraction_.active()) {
                clearBaseItemInteractions();
                baseStatus_ = "配置をキャンセルしたよ";
            } else if ((baseStorageMode_ == StorageUiMode::Deposit ||
                    baseStorageMode_ == StorageUiMode::Withdraw) &&
                baseStorageBatchSelection_.active) {
                if (!baseStorageBatchSelection_.selectedKeys.empty()) {
                    baseStorageBatchSelection_.selectedKeys.clear();
                    baseStatus_ = baseStorageMode_ == StorageUiMode::Withdraw
                        ? "まとめ取り出しの選択を解除したよ"
                        : "まとめ収納の選択を解除したよ";
                } else {
                    clearStorageBatchSelectionState();
                    baseStatus_.clear();
                }
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
            ui.emitActionResult(registered);
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
                if (ui.selectionFocused(rect)) {
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
            const int actionCount = storageBulkActionCount();
            baseStorageBulkSelection_ = std::clamp(baseStorageBulkSelection_, 0, actionCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseStorageBulkSelection_ = (baseStorageBulkSelection_ + actionCount - 1) % actionCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseStorageBulkSelection_ = (baseStorageBulkSelection_ + 1) % actionCount;
            }

            const auto executeBulkAction = [&](int selection) {
                if (!tryActivateUiButton(ui, storageBulkActionState(selection))) {
                    baseStatus_ = selection == 0
                        ? "しまえるアイテムがないよ"
                        : "プリセット" + std::to_string(selection) + "は未登録だよ";
                    return;
                }
                if (selection == 0) {
                    depositAllBackpackItems();
                    ui.emitSound(UiSoundEvent::Confirm);
                    return;
                }
                const int presetIndex = selection - 1;
                prepareRingPresetFromWarehouse(presetIndex);
                ui.emitSound(UiSoundEvent::Confirm);
            };

            for (int i = 0; i < actionCount; ++i) {
                const UiRect rect = storageBulkChoiceRect(storageBounds, i);
                if (ui.selectionFocused(rect)) {
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
                sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(source, unlockedRingCount(), true);
                sourceTabRects[static_cast<std::size_t>(i)] = storageDepositSourceRect(i, sourceCount);
            }
            UiTabsInput sourceTabsInput = makeUiCycleTabsInput(input, sourceCount);
            sourceTabsInput.commit = sourceTabsInput.commit ||
                (!ui.navigationActive() && (input.confirmPressed() || input.useItemPressed()));
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
                clearBaseItemInteractions();
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

            if (baseStorageBatchSelection_.active) {
                pruneStorageBulkDepositSelection();
            }
            const bool bulkDepositActionRequested =
                batchItemActionPressed(input) ||
                ui.pressed(baseStorageBatchSelection_.active
                    ? batchItemActionButtonRect(2)
                    : batchItemModeButtonRect());
            if (bulkDepositActionRequested) {
                closeStorageCommand();
                resetStoragePointerPress();
                clearBaseItemInteractions();
                if (!baseStorageBatchSelection_.active) {
                    baseStorageBatchSelection_.active = true;
                    baseStorageBatchSelection_.selectedKeys.clear();
                    baseStatus_.clear();
                    ui.emitSound(UiSoundEvent::Confirm);
                } else {
                    const StorageBatchTransferSummary summary = storageBulkDepositSummary();
                    if (summary.selectedCount <= 0) {
                        baseStatus_ = "しまうアイテムが選択されていないよ";
                        openStorageBatchTransferFailureDialog(
                            baseResultDialog_,
                            false,
                            baseStatus_);
                        ui.rejectAction();
                    } else if (!summary.fits()) {
                        baseStatus_ = "収納箱の空きがあと" +
                            std::to_string(summary.requiredSlots - summary.freeSlots) + "枠必要だよ";
                        openStorageBatchTransferFailureDialog(
                            baseResultDialog_,
                            false,
                            baseStatus_);
                        ui.rejectAction();
                    } else {
                        openUiConfirmDialog(
                            baseStorageBatchSelection_.confirm,
                            "まとめてしまう",
                            std::to_string(summary.selectedCount) + "個のアイテムをまとめてしまいますか？\n" +
                                "収納箱 " + std::to_string(warehouseUsedSlots()) + "/" +
                                std::to_string(warehouseCapacity()) + " → " +
                                std::to_string(warehouseUsedSlots() + summary.requiredSlots) + "/" +
                                std::to_string(warehouseCapacity()),
                            "しまう",
                            "キャンセル",
                            1);
                        ui.emitSound(UiSoundEvent::Confirm);
                    }
                }
                ui.block(storageBounds);
                return;
            }

            if (baseStorageBatchSelection_.active) {
                if (input.arrangeItemsPressed() || ui.pressed(batchItemActionButtonRect(0))) {
                    selectAllStorageBulkDepositTargets(baseStorageDepositSource_);
                    baseStatus_.clear();
                    ui.emitSound(UiSoundEvent::Confirm);
                    ui.block(storageBounds);
                    return;
                }
                if (ui.pressed(batchItemActionButtonRect(1))) {
                    if (baseStorageBatchSelection_.selectedKeys.empty()) {
                        ui.rejectAction();
                    } else {
                        baseStorageBatchSelection_.selectedKeys.clear();
                        baseStatus_ = "まとめ収納の選択を解除したよ";
                        ui.emitSound(UiSoundEvent::Confirm);
                    }
                    ui.block(storageBounds);
                    return;
                }
            }

            if (!baseStorageBatchSelection_.active &&
                (input.arrangeItemsPressed() || ui.pressed(storageTransferSortButtonRect()))) {
                closeStorageCommand();
                resetStoragePointerPress();
                clearBaseItemInteractions();
                const bool sorted = sortBaseItemSource(baseStorageDepositSource_);
                ui.emitActionResult(sorted, UiSoundEvent::ItemMove);
                baseStorageDepositSelection_ = 0;
                ui.block(storageBounds);
                return;
            }

            if (baseItemSourceIsRing(baseStorageDepositSource_)) {
                const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseStorageDepositSource_), 0, SpellRingCount - 1);
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                const int itemCount = static_cast<int>(ringItems.size());
                const BaseRingInteractionResult interaction =
                    updateBaseRingItemInteraction(
                        input,
                        ui,
                        baseStorageDepositSource_,
                        baseStorageDepositSelection_,
                        BaseRingPreviewKind::Storage,
                        ringPreviewSeconds,
                        baseStorageBatchSelection_.active
                            ? BaseRingInteractionMode::ActivateOnly
                            : BaseRingInteractionMode::Manage);
                if (interaction.activateIndex >= 0) {
                    if (baseStorageBatchSelection_.active) {
                        if (toggleStorageBulkDepositTarget(
                                storageDepositTargetForScreenSlot(interaction.activateIndex))) {
                            baseStatus_.clear();
                            ui.emitSound(UiSoundEvent::Confirm);
                        } else {
                            ui.rejectAction();
                        }
                    } else {
                        const UiRect rect = storageRingItemRect(
                            ringItems[static_cast<std::size_t>(interaction.activateIndex)],
                            spellRing_,
                            balance_,
                            ringIndex,
                            interaction.activateIndex,
                            itemCount,
                            ringPreviewSeconds);
                        openStorageCommand(
                            StorageQuantityOperation::Deposit,
                            storageDepositTargetForScreenSlot(interaction.activateIndex),
                            rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                    }
                }
                ui.block(storageBounds);
                return;
            }

            moveGridSelection(baseStorageDepositSelection_, inventory_.screenSlotCount());
            std::vector<ItemGridInteractionSlot> interactionSlots;
            interactionSlots.reserve(static_cast<std::size_t>(inventory_.screenSlotCount()));
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const UiRect rect = storageTransferGridSlotRect(i);
                if (ui.selectionFocused(rect)) {
                    baseStorageDepositSelection_ = i;
                }
                if (baseStorageBatchSelection_.active && ui.pressed(rect)) {
                    baseStorageDepositSelection_ = i;
                    if (toggleStorageBulkDepositTarget(storageDepositTargetForScreenSlot(i))) {
                        baseStatus_.clear();
                        ui.emitSound(UiSoundEvent::Confirm);
                    } else {
                        ui.rejectAction();
                    }
                    ui.block(storageBounds);
                    return;
                }
                const StorageTransferTarget target = storageDepositTargetForScreenSlot(i);
                interactionSlots.push_back({
                    .rect = rect,
                    .key = itemKeyForBaseItemTarget(target).value_or(ItemKey{}),
                    .placement = i,
                });
            }
            if (baseStorageBatchSelection_.active) {
                if (input.confirmPressed() || input.useItemPressed()) {
                    if (toggleStorageBulkDepositTarget(
                            storageDepositTargetForScreenSlot(baseStorageDepositSelection_))) {
                        baseStatus_.clear();
                        ui.emitSound(UiSoundEvent::Confirm);
                    } else {
                        ui.rejectAction();
                    }
                }
                ui.block(storageBounds);
                return;
            }
            const ItemGridInteractionResult interaction = baseItemInteraction_.update(
                ItemGridInteractionInput{
                    .slots = interactionSlots,
                    .selectedSlot = baseStorageDepositSelection_,
                    .activatePressed = input.confirmPressed() || input.useItemPressed(),
                    .grabPressed = input.grabOrPlacePressed(),
                    .protectionPressed = input.pressed(InputAction::ToggleProtection),
                    .pointerEnabled = true,
                    .dragStartDistanceSquared = StorageDragStartDistanceSq,
                },
                input,
                ui);
            if (interaction.slotIndex >= 0) {
                baseStorageDepositSelection_ = interaction.slotIndex;
            }
            if (handleBaseItemGridInteraction(interaction, [&](int slotIndex) {
                    const int slot = std::clamp(
                        slotIndex,
                        0,
                        std::max(0, inventory_.screenSlotCount() - 1));
                    const UiRect rect = storageTransferGridSlotRect(slot);
                    openStorageCommand(
                        StorageQuantityOperation::Deposit,
                        storageDepositTargetForScreenSlot(slot),
                        rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                })) {
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
            if (baseStorageBatchSelection_.active) {
                pruneStorageBulkWithdrawSelection();
            }
            const bool bulkWithdrawActionRequested =
                batchItemActionPressed(input) ||
                ui.pressed(baseStorageBatchSelection_.active
                    ? storageWithdrawBatchActionButtonRect(2)
                    : storageWithdrawBatchModeButtonRect());
            if (bulkWithdrawActionRequested) {
                closeStorageCommand();
                resetStoragePointerPress();
                clearBaseItemInteractions();
                if (!baseStorageBatchSelection_.active) {
                    baseStorageBatchSelection_.active = true;
                    baseStorageBatchSelection_.selectedKeys.clear();
                    baseStatus_.clear();
                    ui.emitSound(UiSoundEvent::Confirm);
                } else {
                    const StorageBatchTransferSummary summary = storageBulkWithdrawSummary();
                    if (summary.selectedCount <= 0) {
                        baseStatus_ = "取り出すアイテムが選択されていないよ";
                        openStorageBatchTransferFailureDialog(
                            baseResultDialog_,
                            true,
                            baseStatus_);
                        ui.rejectAction();
                    } else if (!summary.fits()) {
                        baseStatus_ = "リュックの空きがあと" +
                            std::to_string(summary.requiredSlots - summary.freeSlots) + "枠必要だよ";
                        openStorageBatchTransferFailureDialog(
                            baseResultDialog_,
                            true,
                            baseStatus_);
                        ui.rejectAction();
                    } else {
                        openUiConfirmDialog(
                            baseStorageBatchSelection_.confirm,
                            "まとめて取り出す",
                            std::to_string(summary.selectedCount) + "個のアイテムをまとめて取り出す？\n" +
                                "リュック " + std::to_string(backpackUsedSlots()) + "/" +
                                std::to_string(inventory_.screenSlotCount()) + " → " +
                                std::to_string(backpackUsedSlots() + summary.requiredSlots) + "/" +
                                std::to_string(inventory_.screenSlotCount()),
                            "取り出す",
                            "キャンセル",
                            1);
                        ui.emitSound(UiSoundEvent::Confirm);
                    }
                }
                ui.block(storageBounds);
                return;
            }

            if (baseStorageBatchSelection_.active) {
                if (input.arrangeItemsPressed() || ui.pressed(storageWithdrawBatchActionButtonRect(0))) {
                    selectAllStorageBulkWithdrawTargets();
                    baseStatus_.clear();
                    ui.emitSound(UiSoundEvent::Confirm);
                    ui.block(storageBounds);
                    return;
                }
                if (ui.pressed(storageWithdrawBatchActionButtonRect(1))) {
                    if (baseStorageBatchSelection_.selectedKeys.empty()) {
                        ui.rejectAction();
                    } else {
                        baseStorageBatchSelection_.selectedKeys.clear();
                        baseStatus_ = "まとめ取り出しの選択を解除したよ";
                        ui.emitSound(UiSoundEvent::Confirm);
                    }
                    ui.block(storageBounds);
                    return;
                }
            }

            if (!baseStorageBatchSelection_.active &&
                (input.arrangeItemsPressed() || ui.pressed(storageWithdrawSortButtonRect()))) {
                clearBaseItemInteractions();
                const bool hasItems = warehouseUsedSlots() > 0;
                ui.emitActionResult(hasItems, UiSoundEvent::ItemMove);
                sortWarehouseByItemOrder();
                ui.block(storageBounds);
                return;
            }
            if (updateStoragePageSelector(
                    ui,
                    input,
                    pageRects,
                    baseStorageWarehousePage_,
                    warehousePageCount,
                    StoragePageSelectorInputMode::CycleShortcutAndArrows)) {
                closeStorageCommand();
                resetStoragePointerPress();
                clearBaseItemInteractions();
                ui.block(storageBounds);
                return;
            }

            moveGridSelection(baseStorageWithdrawSelection_, StorageWithdrawSlotCount);
            std::vector<ItemGridInteractionSlot> interactionSlots;
            interactionSlots.reserve(StorageWithdrawSlotCount);
            for (int i = 0; i < StorageWithdrawSlotCount; ++i) {
                const UiRect rect = storageWithdrawSlotRect(i);
                if (ui.selectionFocused(rect)) {
                    baseStorageWithdrawSelection_ = i;
                }
                if (baseStorageBatchSelection_.active && ui.pressed(rect)) {
                    baseStorageWithdrawSelection_ = i;
                    if (toggleStorageBulkWithdrawTarget(storageWithdrawTargetForSlot(i))) {
                        baseStatus_.clear();
                        ui.emitSound(UiSoundEvent::Confirm);
                    } else {
                        ui.rejectAction();
                    }
                    ui.block(storageBounds);
                    return;
                }
                const StorageTransferTarget target = storageWithdrawTargetForSlot(i);
                interactionSlots.push_back({
                    .rect = rect,
                    .key = itemKeyForBaseItemTarget(target).value_or(ItemKey{}),
                    .placement = baseStorageWarehousePage_ * StorageWithdrawSlotCount + i,
                });
            }
            if (baseStorageBatchSelection_.active) {
                if (input.confirmPressed() || input.useItemPressed()) {
                    if (toggleStorageBulkWithdrawTarget(
                            storageWithdrawTargetForSlot(baseStorageWithdrawSelection_))) {
                        baseStatus_.clear();
                        ui.emitSound(UiSoundEvent::Confirm);
                    } else {
                        ui.rejectAction();
                    }
                }
                ui.block(storageBounds);
                return;
            }
            const ItemGridInteractionResult interaction = baseItemInteraction_.update(
                ItemGridInteractionInput{
                    .slots = interactionSlots,
                    .selectedSlot = baseStorageWithdrawSelection_,
                    .activatePressed = input.confirmPressed() || input.useItemPressed(),
                    .grabPressed = input.grabOrPlacePressed(),
                    .protectionPressed = input.pressed(InputAction::ToggleProtection),
                    .pointerEnabled = true,
                    .dragStartDistanceSquared = StorageDragStartDistanceSq,
                },
                input,
                ui);
            if (interaction.slotIndex >= 0) {
                baseStorageWithdrawSelection_ = interaction.slotIndex;
            }
            if (handleBaseItemGridInteraction(interaction, [&](int slotIndex) {
                    const int slot = std::clamp(slotIndex, 0, StorageWithdrawSlotCount - 1);
                    const UiRect rect = storageWithdrawSlotRect(slot);
                    openStorageCommand(
                        StorageQuantityOperation::Withdraw,
                        storageWithdrawTargetForSlot(slot),
                        rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                })) {
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
            } else if (baseItemInteraction_.grabActive() ||
                baseRingItemInteraction_.active()) {
                clearBaseItemInteractions();
                baseStatus_ = "配置をキャンセルしたよ";
            } else if (baseProcessingUiMode_ == ProcessingUiMode::Enhance) {
                clearBaseItemInteractions();
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
                const UiButtonState buttonState = baseProcessingActionSelection_ == 0
                    ? uiButtonState(processingBulkRepairExecutable())
                    : UiButtonState::Enabled;
                if (!tryActivateUiButton(ui, buttonState)) {
                    applyProcessingBulkRepair();
                    return;
                }
                ui.emitSound(UiSoundEvent::Confirm);
                if (baseProcessingActionSelection_ == 0) {
                    applyProcessingBulkRepair();
                } else {
                    clearBaseItemInteractions();
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
                if (ui.selectionFocused(rect)) {
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
                ui.rejectAction();
                baseStatus_ = "加工対象がないよ";
                return false;
            }
            if (!processingTargetHasAvailableCommand(target)) {
                ui.rejectAction();
                baseStatus_ = "このアイテムにできる作業がないよ";
                return false;
            }
            const std::vector<UiCommandMenuItem> items = processingCommandItems(target);
            baseProcessingCommandSlot_ = slotIndex;
            Vec2 commandAnchor = uiCommandMenuAnchorForSlot(
                baseItemSourceSlotRect(baseProcessingGridSlotRect, slotIndex));
            if (target.source != BaseItemSource::Backpack &&
                target.source != BaseItemSource::Warehouse &&
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
            sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(i, unlockedRingCount(), enabled);
            sourceTabRects[static_cast<std::size_t>(i)] = baseProcessingSourceRect(i, sourceCount);
        }
        UiTabsInput sourceTabsInput = makeUiCycleTabsInput(input, sourceCount);
        const int directSourceFocus = input.shortcutSlotPressed();
        if (directSourceFocus >= 0 && directSourceFocus < sourceCount) {
            sourceTabsInput.directFocusIndex = directSourceFocus;
        }
        sourceTabsInput.commit =
            sourceTabsInput.focusDelta != 0 ||
            sourceTabsInput.directFocusIndex >= 0 ||
            (!ui.navigationActive() && (input.confirmPressed() || input.useItemPressed()));
        const int sourceSelection = updateUiTabs(
            baseProcessingSourceTabs_,
            ui,
            sourceTabsInput,
            baseProcessingSource_,
            sourceTabs.data(),
            sourceCount,
            sourceTabRects.data());
        if (sourceSelection >= 0) {
            clearBaseItemInteractions();
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

        if (input.arrangeItemsPressed() ||
            ui.pressed(merchantSellSortButtonRect())) {
            closeProcessingCommand();
            clearBaseItemInteractions();
            baseProcessingSelection_ = 0;
            const bool sorted = sortBaseItemSource(baseProcessingSource_);
            ui.emitActionResult(sorted, UiSoundEvent::ItemMove);
            ui.block(merchantPanelRect());
            return;
        }

        const bool processingWarehouseSource =
            baseItemSourceIsWarehouse(baseProcessingSource_);
        if (processingWarehouseSource) {
            const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
            baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
            const UiPageSelectorRects pageRects = baseWarehouseSourcePageSelectorRects(baseProcessingGridSlotRect);
            if (updateStoragePageSelector(
                    ui,
                    input,
                    pageRects,
                    baseStorageWarehousePage_,
                    warehousePageCount,
                    StoragePageSelectorInputMode::ArrowsOnly)) {
                closeProcessingCommand();
                clearBaseItemInteractions();
                ui.block(merchantPanelRect());
                return;
            }
        }

        if (baseItemSourceIsRing(baseProcessingSource_)) {
            const BaseRingInteractionResult interaction =
                updateBaseRingItemInteraction(
                    input,
                    ui,
                    baseProcessingSource_,
                    baseProcessingSelection_,
                    BaseRingPreviewKind::Processing,
                    ringPreviewSeconds);
            if (interaction.activateIndex >= 0) {
                (void)openProcessingCommand(interaction.activateIndex);
            }
            ui.block(merchantPanelRect());
            return;
        }

        constexpr int Columns = StorageColumns;
        const int slotCount = processingWarehouseSource
            ? StoragePaneSlotCount
            : inventory_.screenSlotCount();
        baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, slotCount - 1));
        if (!ui.navigationActive() || ui.navigationFocusRole() == UiNavigationRole::Grid) {
            const int dx =
                (input.pressed(InputAction::MoveRight) ? 1 : 0) -
                (input.pressed(InputAction::MoveLeft) ? 1 : 0);
            const int dy =
                (input.pressed(InputAction::MoveDown) ? 1 : 0) -
                (input.pressed(InputAction::MoveUp) ? 1 : 0);
            baseProcessingSelection_ =
                moveUiGridSelection(baseProcessingSelection_, slotCount, Columns, dx, dy);
        }
        std::vector<ItemGridInteractionSlot> interactionSlots;
        interactionSlots.reserve(static_cast<std::size_t>(slotCount));
        for (int i = 0; i < slotCount; ++i) {
            const UiRect rect = baseItemSourceSlotRect(baseProcessingGridSlotRect, i);
            if (ui.selectionFocused(rect)) {
                baseProcessingSelection_ = i;
            }
            const ProcessingTarget target = processingTargetForScreenSlot(i);
            interactionSlots.push_back({
                .rect = rect,
                .key = itemKeyForProcessingTarget(target).value_or(ItemKey{}),
                .placement = processingWarehouseSource
                    ? baseStorageWarehousePage_ * StoragePaneSlotCount + i
                    : i,
            });
        }
        const ItemGridInteractionResult interaction = baseItemInteraction_.update(
            ItemGridInteractionInput{
                .slots = interactionSlots,
                .selectedSlot = baseProcessingSelection_,
                .activatePressed = input.confirmPressed() || input.useItemPressed(),
                .grabPressed = input.grabOrPlacePressed(),
                .protectionPressed = input.pressed(InputAction::ToggleProtection),
                .pointerEnabled = true,
                .dragStartDistanceSquared = StorageDragStartDistanceSq,
            },
            input,
            ui);
        if (interaction.slotIndex >= 0) {
            baseProcessingSelection_ = interaction.slotIndex;
        }
        if (handleBaseItemGridInteraction(interaction, [&](int slotIndex) {
                (void)openProcessingCommand(slotIndex);
            })) {
            ui.block(merchantPanelRect());
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
            baseQuantityDialog_ = {};
            baseQuantityPending_ = {};
            clearMerchantBulkSellState();
            clearBaseItemInteractions();
            baseSellActive_ = false;
            baseMerchantMode_ = MerchantUiMode::Closed;
            baseStatus_.clear();
        };
        const auto returnToMerchantMenu = [&]() {
            closeMerchantCommands();
            baseQuantityDialog_ = {};
            baseQuantityPending_ = {};
            clearMerchantBulkSellState();
            clearBaseItemInteractions();
            baseMerchantMode_ = MerchantUiMode::ChooseAction;
            baseMerchantActionSelection_ = 0;
            baseStatus_.clear();
        };
        const auto moveGridSelection = [&input, &ui](int& selection, int count) {
            constexpr int Columns = 8;
            if (count <= 0) {
                selection = 0;
                return;
            }
            selection = std::clamp(selection, 0, count - 1);
            if (!ui.navigationActive() || ui.navigationFocusRole() == UiNavigationRole::Grid) {
                const int dx =
                    (input.pressed(InputAction::MoveRight) ? 1 : 0) -
                    (input.pressed(InputAction::MoveLeft) ? 1 : 0);
                const int dy =
                    (input.pressed(InputAction::MoveDown) ? 1 : 0) -
                    (input.pressed(InputAction::MoveUp) ? 1 : 0);
                selection = moveUiGridSelection(selection, count, Columns, dx, dy);
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
            clearBaseItemInteractions();
            baseSellSelection_ = 0;
            const bool sorted = sortBaseItemSource(baseMerchantSellSource_);
            ui.emitActionResult(sorted, UiSoundEvent::ItemMove);
        };
        const auto openSellCommand = [&](int slotIndex) {
            const MerchantSellTarget target = merchantSellTargetForScreenSlot(slotIndex);
            if (!target.valid) {
                ui.rejectAction();
                baseStatus_ = "売却対象がないよ";
                return;
            }
            if (!merchantSellTargetAvailable(target)) {
                ui.rejectAction();
                sellMerchantTarget(target, 1);
                return;
            }
            baseMerchantSellCommandIndex_ = slotIndex;
            baseMerchantSellCommandSource_ = baseMerchantSellSource_;
            Vec2 commandAnchor = uiCommandMenuAnchorForSlot(
                baseItemSourceSlotRect(merchantSellGridSlotRect, slotIndex));
            if (target.source != BaseItemSource::Backpack &&
                target.source != BaseItemSource::Warehouse &&
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
            const std::array<UiCommandMenuItem, 1> items{{{"売る", true}}};
            openUiCommandMenu(
                baseMerchantSellCommandMenu_,
                commandAnchor,
                merchantPanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                120.0f,
                2);
        };
        const auto activateSellSlot = [&](int slotIndex) {
            if (!baseMerchantBulkSell_.active) {
                openSellCommand(slotIndex);
                return;
            }
            const MerchantSellTarget target = merchantSellTargetForScreenSlot(slotIndex);
            if (toggleMerchantBulkSellTarget(target)) {
                baseStatus_.clear();
                ui.emitSound(UiSoundEvent::Confirm);
            } else {
                sellMerchantTarget(target, 1);
                ui.rejectAction();
            }
        };
        const auto openBuyCommand = [&](int index) {
            if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
                ui.rejectAction();
                baseStatus_ = "購入できる商品がないよ";
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
            } else if (baseItemInteraction_.grabActive() ||
                baseRingItemInteraction_.active()) {
                clearBaseItemInteractions();
                baseStatus_ = "配置をキャンセルしたよ";
            } else if (baseMerchantMode_ == MerchantUiMode::Sell && baseMerchantBulkSell_.active) {
                if (!baseMerchantBulkSell_.selectedKeys.empty()) {
                    baseMerchantBulkSell_.selectedKeys.clear();
                    baseStatus_ = "まとめ売りの選択を解除したよ";
                } else {
                    clearMerchantBulkSellState();
                    baseStatus_.clear();
                }
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
                if (ui.selectionFocused(rect)) {
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
            if (baseMerchantBulkSell_.active) {
                closeMerchantCommands();
                pruneMerchantBulkSellSelection();
            } else {
                const MerchantSellTarget commandTarget = merchantSellTargetForSourceSlot(
                    baseMerchantSellCommandSource_,
                    baseMerchantSellCommandIndex_);
                const std::array<UiCommandMenuItem, 1> commandItems{{{"売る", true}}};
                const bool commandOpenBeforeUpdate = baseMerchantSellCommandMenu_.open;
                const int commandSelection = updateUiCommandMenu(
                    baseMerchantSellCommandMenu_,
                    ui,
                    input,
                    commandItems.data(),
                    static_cast<int>(commandItems.size()));
                if (commandSelection >= 0 && baseMerchantSellCommandIndex_ >= 0) {
                    const int quantity = merchantSellTargetQuantity(commandTarget);
                    closeMerchantCommands();
                    if (quantity > 1) {
                        baseQuantityPending_.operation = BaseQuantityOperation::MerchantSell;
                        baseQuantityPending_.target = commandTarget;
                        baseQuantityPending_.merchantProductIndex = -1;
                        openUiQuantityDialog(
                            baseQuantityDialog_,
                            "売る個数",
                            1,
                            quantity,
                            1,
                            "個");
                    } else {
                        ui.emitActionResult(sellMerchantTarget(commandTarget, 1));
                    }
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
                sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(i, unlockedRingCount(), enabled);
                sourceTabRects[static_cast<std::size_t>(i)] = merchantSellSourceRect(i, sourceCount);
            }
            UiTabsInput sourceTabsInput = makeUiCycleTabsInput(input, sourceCount);
            const int directSourceFocus = input.shortcutSlotPressed();
            if (directSourceFocus >= 0 && directSourceFocus < sourceCount) {
                sourceTabsInput.directFocusIndex = directSourceFocus;
            }
            sourceTabsInput.commit =
                sourceTabsInput.focusDelta != 0 ||
                sourceTabsInput.directFocusIndex >= 0 ||
                (!ui.navigationActive() && (input.confirmPressed() || input.useItemPressed()));
            const int sourceSelection = updateUiTabs(
                baseMerchantSellSourceTabs_,
                ui,
                sourceTabsInput,
                baseMerchantSellSource_,
                sourceTabs.data(),
                sourceCount,
                sourceTabRects.data());
            if (sourceSelection >= 0) {
                clearBaseItemInteractions();
                baseMerchantSellSource_ = sourceSelection;
                baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, merchantSellSourceSlotCount() - 1));
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            }

            const bool bulkSellActionRequested =
                batchItemActionPressed(input) ||
                ui.pressed(baseMerchantBulkSell_.active
                    ? batchItemActionButtonRect(2)
                    : batchItemModeButtonRect());
            if (bulkSellActionRequested) {
                clearBaseItemInteractions();
                if (!baseMerchantBulkSell_.active) {
                    baseMerchantBulkSell_.active = true;
                    baseMerchantBulkSell_.selectedKeys.clear();
                    closeMerchantCommands();
                    baseStatus_.clear();
                    ui.emitSound(UiSoundEvent::Confirm);
                } else {
                    const MerchantBulkSellSummary summary = merchantBulkSellSummary();
                    if (summary.itemCount <= 0) {
                        baseStatus_ = "売却するアイテムが選択されていないよ";
                        ui.rejectAction();
                    } else {
                        openUiConfirmDialog(
                            baseMerchantBulkSell_.confirm,
                            "まとめ売り",
                            std::to_string(summary.itemCount) + "個のアイテムをまとめて売る？\n" +
                                std::to_string(summary.totalPrice) + "円になります",
                            "売る",
                            "キャンセル",
                            1);
                        ui.emitSound(UiSoundEvent::Confirm);
                    }
                }
                ui.block(merchantBounds);
                return;
            }

            if (baseMerchantBulkSell_.active) {
                if (input.arrangeItemsPressed() || ui.pressed(batchItemActionButtonRect(0))) {
                    selectAllMerchantBulkSellTargets(baseMerchantSellSource_);
                    baseStatus_.clear();
                    ui.emitSound(UiSoundEvent::Confirm);
                    ui.block(merchantBounds);
                    return;
                }
                if (ui.pressed(batchItemActionButtonRect(1))) {
                    if (baseMerchantBulkSell_.selectedKeys.empty()) {
                        ui.rejectAction();
                    } else {
                        baseMerchantBulkSell_.selectedKeys.clear();
                        baseStatus_ = "まとめ売りの選択を解除したよ";
                        ui.emitSound(UiSoundEvent::Confirm);
                    }
                    ui.block(merchantBounds);
                    return;
                }
            } else if (input.arrangeItemsPressed() || ui.pressed(merchantSellSortButtonRect())) {
                sortMerchantSellSource();
                ui.block(merchantBounds);
                return;
            }

            const bool merchantWarehouseSource =
                baseItemSourceIsWarehouse(baseMerchantSellSource_);
            if (merchantWarehouseSource) {
                const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
                baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                const UiPageSelectorRects pageRects = baseWarehouseSourcePageSelectorRects(merchantSellGridSlotRect);
                if (updateStoragePageSelector(
                        ui,
                        input,
                        pageRects,
                        baseStorageWarehousePage_,
                        warehousePageCount,
                        StoragePageSelectorInputMode::ArrowsOnly)) {
                    closeMerchantCommands();
                    clearBaseItemInteractions();
                    ui.block(merchantBounds);
                    return;
                }

                moveGridSelection(baseSellSelection_, StoragePaneSlotCount);
                std::vector<ItemGridInteractionSlot> interactionSlots;
                interactionSlots.reserve(StoragePaneSlotCount);
                for (int i = 0; i < StoragePaneSlotCount; ++i) {
                    const UiRect rect = baseItemSourceSlotRect(merchantSellGridSlotRect, i);
                    if (ui.selectionFocused(rect)) {
                        baseSellSelection_ = i;
                    }
                    if (baseMerchantBulkSell_.active && ui.pressed(rect)) {
                        baseSellSelection_ = i;
                        activateSellSlot(i);
                        ui.block(merchantBounds);
                        return;
                    }
                    const MerchantSellTarget target =
                        merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                    interactionSlots.push_back({
                        .rect = rect,
                        .key = itemKeyForBaseItemTarget(target).value_or(ItemKey{}),
                        .placement = baseStorageWarehousePage_ * StoragePaneSlotCount + i,
                    });
                }
                if (baseMerchantBulkSell_.active &&
                    (input.confirmPressed() || input.useItemPressed())) {
                    activateSellSlot(baseSellSelection_);
                    ui.block(merchantBounds);
                    return;
                }
                if (!baseMerchantBulkSell_.active) {
                    const ItemGridInteractionResult interaction = baseItemInteraction_.update(
                        ItemGridInteractionInput{
                            .slots = interactionSlots,
                            .selectedSlot = baseSellSelection_,
                            .activatePressed = input.confirmPressed() || input.useItemPressed(),
                            .grabPressed = input.grabOrPlacePressed(),
                            .protectionPressed = input.pressed(InputAction::ToggleProtection),
                            .pointerEnabled = true,
                            .dragStartDistanceSquared = StorageDragStartDistanceSq,
                        },
                        input,
                        ui);
                    if (interaction.slotIndex >= 0) {
                        baseSellSelection_ = interaction.slotIndex;
                    }
                    if (handleBaseItemGridInteraction(interaction, activateSellSlot)) {
                        ui.block(merchantBounds);
                        return;
                    }
                }
                ui.block(merchantBounds);
                return;
            }

            if (baseItemSourceIsRing(baseMerchantSellSource_)) {
                const BaseRingInteractionResult interaction =
                    updateBaseRingItemInteraction(
                        input,
                        ui,
                        baseMerchantSellSource_,
                        baseSellSelection_,
                        BaseRingPreviewKind::Merchant,
                        ringPreviewSeconds,
                        baseMerchantBulkSell_.active
                            ? BaseRingInteractionMode::ActivateOnly
                            : BaseRingInteractionMode::Manage);
                if (interaction.activateIndex >= 0) {
                    activateSellSlot(interaction.activateIndex);
                }
                ui.block(merchantBounds);
                return;
            }

            moveGridSelection(baseSellSelection_, inventory_.screenSlotCount());
            std::vector<ItemGridInteractionSlot> interactionSlots;
            interactionSlots.reserve(
                static_cast<std::size_t>(inventory_.screenSlotCount()));
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const UiRect rect = baseItemSourceSlotRect(merchantSellGridSlotRect, i);
                if (ui.selectionFocused(rect)) {
                    baseSellSelection_ = i;
                }
                if (baseMerchantBulkSell_.active && ui.pressed(rect)) {
                    baseSellSelection_ = i;
                    activateSellSlot(i);
                    ui.block(merchantBounds);
                    return;
                }
                const MerchantSellTarget target =
                    merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                interactionSlots.push_back({
                    .rect = rect,
                    .key = itemKeyForBaseItemTarget(target).value_or(ItemKey{}),
                    .placement = i,
                });
            }
            if (baseMerchantBulkSell_.active &&
                (input.confirmPressed() || input.useItemPressed())) {
                activateSellSlot(baseSellSelection_);
                ui.block(merchantBounds);
                return;
            }
            if (!baseMerchantBulkSell_.active) {
                const ItemGridInteractionResult interaction = baseItemInteraction_.update(
                    ItemGridInteractionInput{
                        .slots = interactionSlots,
                        .selectedSlot = baseSellSelection_,
                        .activatePressed = input.confirmPressed() || input.useItemPressed(),
                        .grabPressed = input.grabOrPlacePressed(),
                        .protectionPressed = input.pressed(InputAction::ToggleProtection),
                        .pointerEnabled = true,
                        .dragStartDistanceSquared = StorageDragStartDistanceSq,
                    },
                    input,
                    ui);
                if (interaction.slotIndex >= 0) {
                    baseSellSelection_ = interaction.slotIndex;
                }
                if (handleBaseItemGridInteraction(interaction, activateSellSlot)) {
                    ui.block(merchantBounds);
                    return;
                }
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
                const int productIndex = baseMerchantBuyCommandIndex_;
                const int quantity = merchantProductPurchasableQuantity(
                    merchantStock_[static_cast<std::size_t>(productIndex)]);
                closeMerchantCommands();
                if (quantity > 1) {
                    baseQuantityPending_.operation = BaseQuantityOperation::MerchantBuy;
                    baseQuantityPending_.target = {};
                    baseQuantityPending_.merchantProductIndex = productIndex;
                    openUiQuantityDialog(
                        baseQuantityDialog_,
                        "買う個数",
                        1,
                        quantity,
                        1,
                        "個");
                } else {
                    ui.emitActionResult(buyMerchantProduct(productIndex, 1));
                }
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
                if (ui.selectionFocused(rect)) {
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
        if (!ui.navigationActive()) {
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
        if (baseUpgradeTabs_.navigationFocused) {
            displaySelection = baseUpgradeTabs_.focusedIndex;
            baseUpgradeSelection_ = baseUpgradeIndexForDisplay(roguelikeTrainer, displaySelection);
        }
        if (selectedTab >= 0) {
            baseUpgradeSelection_ = baseUpgradeIndexForDisplay(roguelikeTrainer, selectedTab);
            if (!ui.navigationActive()) {
                ui.block(upgradePanel);
                return;
            }
        }
        if (ui.pressed(baseUpgradeConfirmRect()) || input.confirmPressed() || input.useItemPressed()) {
            if (tryActivateUiButton(ui, uiButtonState(upgradeExecutable(baseUpgradeSelection_)))) {
                ui.emitSound(UiSoundEvent::Confirm);
            }
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
                baseStatus_ = "解放済みワープポイントがないよ";
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
                baseStatus_ = "解放済みワープポイントがないよ";
                ui.block(baseMiningWarpPointSelectRect());
                return;
            }

            const int warpPointCount = static_cast<int>(selectableWarpPoints.size());
            baseWarpPointSelection_ = std::clamp(baseWarpPointSelection_, 0, warpPointCount - 1);
            for (int i = 0; i < warpPointCount; ++i) {
                const UiRect rect = baseMiningWarpPointSelectChoiceRect(i);
                if (ui.selectionFocused(rect)) {
                    baseWarpPointSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseWarpPointSelection_ = i;
                    const bool started = startFromSelectedWarpPoint();
                    ui.emitActionResult(started);
                    if (started) {
                        return;
                    }
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                const bool started = startFromSelectedWarpPoint();
                ui.emitActionResult(started);
                if (started) {
                    return;
                }
            }
            ui.block(baseMiningWarpPointSelectRect());
            return;
        }

        const UiPageSelectorRects stageSelector = baseMiningStageSelectorRects();
        int stageDelta = uiCycleInputDelta(input, selectableStageCount);
        if (stageDelta == 0 && updateUiArrowButton(
                ui,
                stageSelector.prev,
                UiArrowDirection::Left,
                UiArrowButtonVariant::Standard,
                selectableStageCount > 1)) {
            stageDelta = -1;
        }
        if (stageDelta == 0 && updateUiArrowButton(
                ui,
                stageSelector.next,
                UiArrowDirection::Right,
                UiArrowButtonVariant::Standard,
                selectableStageCount > 1)) {
            stageDelta = 1;
        }
        if (stageDelta != 0 && changeSelectedStage(stageDelta)) {
            ui.emitSound(UiSoundEvent::TabSwitch);
            ui.block(miningStartPanel);
            return;
        }
        if (selectedStageRoguelike) {
            const UiRect startRect = baseMiningStartChoiceRect(0);
            if (ui.selectionFocused(startRect)) {
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
        const auto miningStartChoiceState = [&](int index) {
            return uiButtonState(
                (index != 1 || !selectableWarpPoints.empty()) &&
                (index != 2 || canRegenerateCurrentStage()));
        };
        const auto chooseMiningStart = [&](int index) {
            if (!tryActivateUiButton(ui, miningStartChoiceState(index))) {
                baseStatus_ = index == 1
                    ? "解放済みワープポイントがないよ"
                    : "全ワープ解放とクリア後に可能";
                return;
            }
            if (index == 1) {
                ui.emitSound(UiSoundEvent::MenuOpen);
                baseWarpPointSelectActive_ = true;
                baseWarpPointSelection_ = std::clamp(
                    baseWarpPointSelection_,
                    0,
                    static_cast<int>(selectableWarpPoints.size()) - 1);
                baseStatus_.clear();
                return;
            }
            if (index == 2) {
                ui.emitSound(UiSoundEvent::MenuOpen);
                openRegenerateConfirm();
                return;
            }
            ui.emitSound(UiSoundEvent::Confirm);
            baseRegenerateConfirm_ = {};
            baseRoguelikeDepartureConfirm_ = {};
            requestMiningStartTransition(false, false);
        };
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            const UiRect rect = baseMiningStartChoiceRect(i);
            if (ui.selectionFocused(rect)) {
                baseMiningStartSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseMiningStartSelection_ = i;
                chooseMiningStart(i);
                ui.block(miningStartPanel);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            chooseMiningStart(baseMiningStartSelection_);
            ui.block(miningStartPanel);
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
                        "壊れたアイテムがリングに乗ってるよ　このまま出発する？",
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
                baseQuantityDialog_ = {};
                baseQuantityPending_ = {};
                closeUiCommandMenu(baseStorageCommandMenu_);
                baseStorageCommandOperation_ = StorageQuantityOperation::None;
                baseStorageCommandTarget_ = {};
                clearBaseItemInteractions();
                clearStorageBatchSelectionState();
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
                clearMerchantBulkSellState();
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
                    baseStatus_ = "リング工房: まだ解禁されていないよ";
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
        drawSmallActionInfoText(renderer, panel, "本棚", "何を見ますか？");
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
    renderer.drawText(panel.pos + BookshelfRecordCountOffset, buffer, {150, 150, 160, 255}, 2);
    std::snprintf(buffer, sizeof(buffer), "完成度 %d%%", codexCompletionPercent(discoveredCount, totalCount));
    const Vec2 completionTextSize = renderer.measureText(buffer, 2);
    renderer.drawText(
        panel.pos + Vec2{panel.size.x - 28.0f - completionTextSize.x, 62.0f},
        buffer,
        {255, 230, 150, 255},
        2);
    if (totalCount <= 0) {
        renderer.drawText(panel.pos + Vec2{28.0f, 154.0f}, "記録対象がないよ", {150, 150, 160, 255}, 2);
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
            applyEnemyCodexImageStageStyle(iconOptions, stage);
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
            if (stage == EncyclopediaStage::Undiscovered) {
                drawBookshelfUnknownDetail(renderer, detailPanel, "未発見");
            } else {
                drawUiSubPanel(renderer, detailPanel);
                const std::string name = enemy.name.empty() ? enemy.id : enemy.name;
                float detailY = drawUiDetailHeader(renderer, detailPanel, name);
                EnemyImageDrawOptions imageOptions;
                imageOptions.allowUpscale = true;
                imageOptions.directionOverrideEnabled = true;
                imageOptions.directionOverride = {0.0f, 1.0f};
                applyEnemyCodexImageStageStyle(imageOptions, stage);
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
                    drawUiDetailText(renderer, detailPanel, detailY, "虫眼鏡で観察すると詳細が記録されるよ");
                } else {
                    drawUiDetailText(renderer, detailPanel, detailY, enemy.description.empty() ? "-" : enemy.description);
                    drawUiDetailLine(renderer, detailPanel, detailY, "HP", std::to_string(enemy.hp));
                    drawUiDetailLine(renderer, detailPanel, detailY, "攻撃力", enemyContactAttackPowerText(enemy));
                    drawUiDetailLine(renderer, detailPanel, detailY, "移動速度", enemyMoveSpeedLabel(enemy.moveSpeed));
                    std::string reward = std::to_string(enemy.xp);
                    reward += "EXP";
                    reward += " / ";
                    reward += std::to_string(enemy.money);
                    reward += "G";
                    drawUiDetailLine(renderer, detailPanel, detailY, "報酬", reward);
                    if (enemy.captureDifficulty > 0) {
                        drawUiDetailLine(renderer, detailPanel, detailY, "捕獲難易度", enemyCaptureDifficultyLabel(enemy.captureDifficulty));
                    }
                }
            }
        } else {
            drawUiSubPanel(renderer, detailPanel);
            float detailY = drawUiDetailHeader(renderer, detailPanel, "敵未選択");
            drawUiDetailText(renderer, detailPanel, detailY, "敵を選択してね");
        }
    } else if (const ObjectDefinition* object = objectAt(bookshelfSelection_)) {
        const bool treasure = object->category == "\xE5\xAE\x9D";
        const EncyclopediaStage stage = encyclopedia_.objectStage(object->id, treasure);
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
            drawBookshelfUnknownDetail(renderer, detailPanel, "未入手");
        }
    } else {
        drawUiSubPanel(renderer, detailPanel);
        float detailY = drawUiDetailHeader(renderer, detailPanel, "アイテム未選択");
        drawUiDetailText(renderer, detailPanel, detailY, "アイテムを選択してね");
    }
}

void Game::renderBaseDiaryScreen(Renderer& renderer, UiRect panel) const
{
    const UiRect body = uiBodyRect(panel);
    const DiarySaveSummary& summary = baseDiarySummary_;

    const UiRect recordPanel{
        body.pos + Vec2{12.0f, -26.0f},
        {body.size.x - 24.0f, BaseDiaryRecordPanelHeight},
    };
    drawUiSubPanel(renderer, recordPanel);
    const UiRect recordContent = uiSubPanelContentRect(recordPanel);

    float y = recordContent.pos.y + 6.0f;
    constexpr float ValueXOffset = 142.0f;
    const float labelX = recordContent.pos.x;
    const float valueX = recordContent.pos.x + ValueXOffset;
    const float rightX = recordContent.pos.x + recordContent.size.x;

    const auto drawTextRow = [&](std::string_view label, std::string_view value, Color valueColor = ui::Text) {
        renderer.drawText({labelX, y}, label, ui::TextMuted, 2);
        renderer.drawText({valueX, y}, value, valueColor, 2);
        y += BaseDiaryInfoRowHeight;
    };

    if (!summary.hasSave && baseDiaryMode_ != BaseDiaryMode::Saved) {
        renderer.drawText({labelX, y}, "記録なし", ui::TextMuted, 2);
        y += BaseDiaryInfoRowHeight;
    } else {
        renderer.drawText({labelX, y}, "進み具合", ui::TextMuted, 2);
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
        y += BaseDiaryInfoRowHeight;
        drawTextRow("ルネのレベル", "Lv." + std::to_string(std::max(1, summary.playerLevel)));
        drawTextRow("アイテム図鑑", std::to_string(std::clamp(summary.itemCodexPercent, 0, 100)) + "%");
        drawTextRow("モンスター図鑑", std::to_string(std::clamp(summary.enemyCodexPercent, 0, 100)) + "%");
        drawTextRow("プレイ時間", formatDiaryPlayTime(summary.playTimeSeconds));
    }

    const Vec2 messagePos{recordPanel.pos.x + 22.0f, recordPanel.pos.y + recordPanel.size.y + 24.0f};
    if (baseDiaryMode_ == BaseDiaryMode::Confirm) {
        renderer.drawText(messagePos, "保存する？", ui::Text, 2);
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 1), "戻る", baseDiarySelection_ == 1, uiCancelButtonStyle());
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 0), "保存", baseDiarySelection_ == 0, uiActionButtonStyle());
    } else if (baseDiaryMode_ == BaseDiaryMode::Error) {
        const std::string message = baseDiaryMessage_.empty() ? std::string("もう一度試すか、戻ってください") : baseDiaryMessage_;
        renderer.drawText(messagePos, message, Color{255, 190, 190, 255}, 2);
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 1), "戻る", baseDiarySelection_ == 1, uiCancelButtonStyle());
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 0), "再試行", baseDiarySelection_ == 0, uiActionButtonStyle());
    } else {
        renderer.drawText(messagePos, "保存したよ", Color{202, 255, 216, 255}, 2);
        drawUiButton(renderer, uiResultDialogOkButtonRect(panel), "閉じる", false, uiActionButtonStyle());
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
                const bool highlightOutline = context.showInteractionHints && inInteractionRange && facilityPtr->enabled;
                options.outlineEnabled = ArtworkOutlineEnabled;
                options.outlineColor = highlightOutline
                    ? (hovered ? Color{255, 230, 72, 255} : Color{255, 255, 255, 245})
                    : ArtworkOutlineColor;
                options.outlinePx = highlightOutline ? DungeonInspectableOutlinePx : ArtworkOutlinePx;

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
                    {PlayerSpriteAnchorX, PlayerSpriteAnchorY},
                    false,
                    artworkImageDrawOptions());
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

bool Game::basePanelUiActive() const
{
    return baseRingWorkshopActive_ ||
        baseDiaryActive_ ||
        baseBookshelfActive_ ||
        baseStorageActive_ ||
        baseProcessingUiMode_ != ProcessingUiMode::Closed ||
        baseSellActive_ ||
        baseUpgradeActive_ ||
        baseMiningStartChoiceActive_;
}

bool Game::baseInteractionHintsVisible() const
{
    const bool dialogActive =
        baseStorageBatchSelection_.confirm.open ||
        baseMerchantBulkSell_.confirm.open ||
        baseResultDialog_.open ||
        baseQuantityDialog_.open ||
        baseProcessingConfirm_.open ||
        baseBrokenRingDepartureConfirm_.open ||
        baseRegenerateConfirm_.open ||
        baseRoguelikeDepartureConfirm_.open;
    const bool globalUiActive =
        dialogue_.active() ||
        pendingStoryTriggerDelayActive() ||
        !pendingStoryTrigger_.empty() ||
        !pendingStoryTriggers_.empty() ||
        firstItemAcquisitionNoticeActive() ||
        debugNamedSaveDialogMode_ != DebugNamedSaveDialogMode::Closed ||
        debugItemPickerActive_ ||
        debugStoryTestActive_ ||
        portraitExpressionPicker_.active;

    return mode_ == ScreenMode::Base &&
        !baseEditEnabled_ &&
        !basePanelUiActive() &&
        !dialogActive &&
        !globalUiActive &&
        !baseMiningRescueDrop_.active &&
        !screenTransition_.active();
}

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
    const bool showInteractionHints = baseInteractionHintsVisible();
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

    renderBaseStoryRingDemo(renderer);
    renderBaseStoryChicoryFlight(renderer);
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
    const auto drawGrabbedGridItem = [this, &renderer, ringPreviewSeconds](
                                         UiRect destination,
                                         float imageMaxSize) {
        const ItemKey* key = baseItemInteraction_.grabbedItem();
        if (key == nullptr) {
            return;
        }
        const BaseItemTarget target = baseItemTargetForItemKey(*key);
        const InventoryUiEntryView entry = storageTransferTargetView(target);
        if (entry.item == nullptr) {
            return;
        }
        const float bob = std::sin(ringPreviewSeconds * 5.4f) * 4.0f;
        drawInventoryUiItemIcon(
            renderer,
            destination.pos + destination.size * 0.5f + Vec2{0.0f, -38.0f + bob},
            entry,
            imageMaxSize,
            false,
            false,
            1.0f);
    };
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
    const bool showInteractionHints = baseInteractionHintsVisible();
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

    renderBaseStoryRingDemo(renderer);
    renderBaseStoryChicoryFlight(renderer);
    renderBaseMiningRescueDropEvent(renderer);
    renderHiddenBaseOrbit(renderer);
    renderTopInfoBar(renderer);
    }

    char buffer[256];
    const bool panelUiActive = basePanelUiActive();
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
    const int bulkStorageActionCount = storageBulkActionCount();
    const UiRect panel = baseDiaryActive_
        ? baseDiaryPanelRect()
        : (baseMiningStartChoiceActive_
        ? baseMiningStartPanelRect()
        : (storageActionDialogActive
        ? (baseStorageMode_ == StorageUiMode::Bulk
            ? storageBulkDialogRect(bulkStorageActionCount)
            : storageActionDialogRect())
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
    const char* panelTitle = "魔女の拠点";
    std::optional<UiWindowScope> panelWindow;
    std::optional<UiCancelControlScope> panelCancelScope;
    if (panelUiActive) {
        std::string panelHelpText(BaseFacilityWindowHelpText);
        if (roguelikeOverlay && baseSellActive_) {
            if (baseMerchantMode_ == MerchantUiMode::Buy) {
                panelTitle = "旅商人 購入";
            } else if (baseMerchantMode_ == MerchantUiMode::Sell) {
                panelTitle = "旅商人 売却";
            } else {
                panelTitle = "旅商人";
            }
        } else if (roguelikeOverlay && baseProcessingUiMode_ != ProcessingUiMode::Closed) {
            panelTitle = "旅の加工職人";
        } else if (roguelikeOverlay && baseUpgradeActive_) {
            panelTitle = "修練者";
        } else if (baseBookshelfActive_) {
            panelTitle = bookshelfPage_ == BookshelfPage::Items
                ? "アイテム図鑑"
                : (bookshelfPage_ == BookshelfPage::Enemies ? "モンスター図鑑" : "本棚");
            if (bookshelfPage_ != BookshelfPage::Menu) {
                panelHelpText = BookshelfCodexHelpText;
            }
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
        if (baseSellActive_ && baseMerchantMode_ == MerchantUiMode::Sell) {
            panelHelpText = withUiCycleHelp(
                batchItemWindowHelpText(
                    baseMerchantBulkSell_.active,
                    "売却",
                    "まとめて売る",
                    true),
                baseItemSourceCountForUnlockedRings(unlockedRingCount()),
                "対象切替");
        } else if (baseStorageActive_ && baseStorageMode_ == StorageUiMode::Deposit) {
            panelHelpText = withUiCycleHelp(
                batchItemWindowHelpText(
                    baseStorageBatchSelection_.active,
                    "しまう",
                    "まとめてしまう",
                    true),
                storageDepositSourceCountForUnlockedRings(unlockedRingCount()),
                "対象切替");
        } else if (baseStorageActive_ && baseStorageMode_ == StorageUiMode::Withdraw) {
            const int warehousePageCount = std::max(
                1,
                (warehouseCapacity() + StorageWithdrawSlotCount - 1) / StorageWithdrawSlotCount);
            panelHelpText = withUiCycleHelp(
                batchItemWindowHelpText(
                    baseStorageBatchSelection_.active,
                    "取り出す",
                    "まとめて取り出す",
                    true),
                warehousePageCount,
                "ページ切替");
        } else if (baseProcessingUiMode_ == ProcessingUiMode::Enhance) {
            panelHelpText = withUiCycleHelp(
                "F/Enter 決定  " +
                    inlineInputActionTag(InputAction::ArrangeItems) +
                    " 並び替え  " +
                    inlineInputActionTag(InputAction::GrabOrPlaceItem) +
                    " つかむ/置く  P 保護  Esc 戻る",
                baseItemSourceCountForUnlockedRings(unlockedRingCount()),
                "対象切替");
        } else if (baseRingWorkshopActive_ && baseRingWorkshopMode_ != RingWorkshopMode::ChooseAction) {
            panelHelpText = withUiCycleHelp(
                "↑/↓ 項目選択  F/Enter 決定  Esc 戻る",
                unlockedRingCount(),
                "リング切替");
        } else if (baseMiningStartChoiceActive_) {
            panelHelpText = withUiCycleHelp(
                std::string(BaseFacilityWindowHelpText),
                static_cast<int>(selectableStageDefinitionsForCurrentUnlockState().size()),
                "行き先切替");
        }
        if (baseRingItemInteraction_.keyboardMoveActive) {
            panelHelpText = "WASD/矢印 位置変更  F/Enter/" +
                inlineInputActionTag(InputAction::GrabOrPlaceItem) +
                " 確定  Esc キャンセル";
        }
        const bool panelCancelButton = true;
        if (panelCancelButton) {
            panelCancelScope.emplace(baseCancelState_);
        }
        panelWindow.emplace(renderer, "base.panel", panel, panelTitle, panelHelpText, UiWindowOptions{true, panelCancelButton});
    }

    if (baseDiaryActive_) {
        renderBaseDiaryScreen(renderer, panel);
    } else if (baseStorageActive_) {
        if (baseStorageMode_ == StorageUiMode::ChooseAction) {
            std::snprintf(buffer, sizeof(buffer), "収納数：%d/%d", warehouseUsedSlots(), warehouseCapacity());
            drawSmallActionInfoText(renderer, panel, panelTitle, buffer);
            constexpr std::array<std::string_view, 3> Choices{"しまう", "取り出す", "一括操作"};
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                drawUiButton(renderer, storageActionChoiceRect(i), Choices[static_cast<std::size_t>(i)], i == baseStorageActionSelection_, uiActionButtonStyle());
            }
        } else if (baseStorageMode_ == StorageUiMode::Bulk) {
            std::snprintf(buffer, sizeof(buffer), "収納数：%d/%d", warehouseUsedSlots(), warehouseCapacity());
            drawSmallActionInfoText(renderer, panel, panelTitle, buffer);
            constexpr std::array<std::string_view, 4> Choices{
                "リュックを全部しまう",
                "プリセット1を準備",
                "プリセット2を準備",
                "プリセット3を準備",
            };
            for (int i = 0; i < bulkStorageActionCount; ++i) {
                drawUiButton(
                    renderer,
                    storageBulkChoiceRect(panel, i),
                    Choices[static_cast<std::size_t>(i)],
                    i == baseStorageBulkSelection_,
                    storageBulkActionState(i),
                    uiActionButtonStyle());
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
                    sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(source, unlockedRingCount(), true);
                    sourceTabRects[static_cast<std::size_t>(i)] = storageDepositSourceRect(i, sourceCount);
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

                if (baseStorageBatchSelection_.active) {
                    const StorageBatchTransferSummary summary = storageBulkDepositSummary();
                    drawUiButton(
                        renderer,
                        batchItemActionButtonRect(0),
                        "全選択",
                        false,
                        batchItemActionButtonStyle(true));
                    drawUiButton(
                        renderer,
                        batchItemActionButtonRect(1),
                        "全解除",
                        false,
                        batchItemActionButtonStyle(summary.selectedCount > 0));
                    drawUiButton(
                        renderer,
                        batchItemActionButtonRect(2),
                        "しまう",
                        false,
                        batchItemActionButtonStyle(summary.selectedCount > 0 && summary.fits()));
                    renderer.drawText(
                        batchItemSelectionSummaryPos(),
                        batchItemSelectionCountText(summary.selectedCount),
                        ui::Text,
                        2);
                } else {
                    drawUiButton(
                        renderer,
                        storageTransferSortButtonRect(),
                        "並び替え",
                        false,
                        uiActionButtonStyle());
                    drawUiButton(
                        renderer,
                        batchItemModeButtonRect(),
                        "まとめてしまう",
                        false,
                        uiActionButtonStyle());
                }

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
                        encyclopedia_,
                        balance_,
                        ringIndex,
                        selectedRingIndex,
                        ringPreviewSeconds);
                    for (int i = 0; i < static_cast<int>(ringItems.size()); ++i) {
                        const SpellRingItem& item = ringItems[static_cast<std::size_t>(i)];
                        UiRect itemRect = storageRingItemRect(
                            item,
                            spellRing_,
                            balance_,
                            ringIndex,
                            i,
                            static_cast<int>(ringItems.size()),
                            ringPreviewSeconds);
                        if (!item.objectId.empty() &&
                            baseStorageBatchSelection_.active &&
                            storageBulkDepositTargetSelected(
                                storageDepositTargetForSourceSlot(baseStorageDepositSource_, i))) {
                            drawBatchItemSelectionBadge(renderer, itemRect);
                        }
                    }
                    if (!ringItems.empty() && selectedRingIndex >= 0) {
                        selectedRingItem = &ringItems[static_cast<std::size_t>(selectedRingIndex)];
                        detailEntry = storageTransferTargetView(storageDepositTargetForSourceSlot(baseStorageDepositSource_, selectedRingIndex));
                    }
                } else {
                    for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                        const StorageTransferTarget target = storageDepositTargetForSourceSlot(baseStorageDepositSource_, i);
                        const InventoryUiEntryView view = storageTransferTargetView(target);
                        const bool disabled = view.item != nullptr &&
                            !storageTransferTargetAvailable(target);
                        InventoryUiSlotStyle style{
                            i == baseStorageDepositSelection_,
                            disabled,
                            48.0f};
                        applyInventoryUiPowerBadgeDiscovery(style, encyclopedia_);
                        style.contentAlpha = itemGridInteractionContentAlpha(
                            baseItemInteraction_,
                            itemKeyForBaseItemTarget(target).value_or(ItemKey{}));
                        applyInventoryUiStackCount(style, view);
                        const UiRect rect = storageTransferGridSlotRect(i);
                        drawInventoryUiSlot(renderer, rect, view, style);
                        if (baseStorageBatchSelection_.active &&
                            storageBulkDepositTargetSelected(target)) {
                            drawBatchItemSelectionBadge(renderer, rect);
                        }
                    }
                    drawGrabbedGridItem(
                        storageTransferGridSlotRect(baseStorageDepositSelection_),
                        48.0f);
                    detailEntry = storageTransferTargetView(storageDepositTargetForScreenSlot(
                        std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1))));
                }
            } else if (baseStorageMode_ == StorageUiMode::Withdraw) {
                const int warehousePageCount = std::max(1, (warehouseCapacity() + StorageWithdrawSlotCount - 1) / StorageWithdrawSlotCount);
                const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                std::snprintf(buffer, sizeof(buffer), "収納数 %d/%d", warehouseUsedSlots(), warehouseCapacity());
                renderer.drawText(storageWithdrawCountTextPos(), buffer, ui::TextMuted, 2);
                drawStorageWithdrawHeader(renderer, warehousePage, warehousePageCount);
                if (baseStorageBatchSelection_.active) {
                    const StorageBatchTransferSummary summary = storageBulkWithdrawSummary();
                    drawUiButton(
                        renderer,
                        storageWithdrawBatchActionButtonRect(0),
                        "全選択",
                        false,
                        batchItemActionButtonStyle(true));
                    drawUiButton(
                        renderer,
                        storageWithdrawBatchActionButtonRect(1),
                        "全解除",
                        false,
                        batchItemActionButtonStyle(summary.selectedCount > 0));
                    drawUiButton(
                        renderer,
                        storageWithdrawBatchActionButtonRect(2),
                        "取り出す",
                        false,
                        batchItemActionButtonStyle(summary.selectedCount > 0 && summary.fits()));
                    renderer.drawText(
                        storageWithdrawBatchSelectionSummaryPos(),
                        batchItemSelectionCountText(summary.selectedCount),
                        ui::Text,
                        2);
                } else {
                    drawUiButton(
                        renderer,
                        storageWithdrawSortButtonRect(),
                        "並び替え",
                        false,
                        uiActionButtonStyle());
                    drawUiButton(
                        renderer,
                        storageWithdrawBatchModeButtonRect(),
                        "まとめて取り出す",
                        false,
                        uiActionButtonStyle());
                }
                for (int i = 0; i < StorageWithdrawSlotCount; ++i) {
                    const StorageTransferTarget target = storageWithdrawTargetForSlot(i);
                    const InventoryUiEntryView view = storageTransferTargetView(target);
                    InventoryUiSlotStyle style{i == baseStorageWithdrawSelection_, false, 48.0f};
                    applyInventoryUiPowerBadgeDiscovery(style, encyclopedia_);
                    style.contentAlpha = itemGridInteractionContentAlpha(
                        baseItemInteraction_,
                        itemKeyForBaseItemTarget(target).value_or(ItemKey{}));
                    applyInventoryUiStackCount(style, view);
                    const UiRect rect = storageWithdrawSlotRect(i);
                    drawInventoryUiSlot(renderer, rect, view, style);
                    if (baseStorageBatchSelection_.active &&
                        storageBulkWithdrawTargetSelected(target)) {
                        drawBatchItemSelectionBadge(renderer, rect);
                    }
                }
                drawGrabbedGridItem(
                    storageWithdrawSlotRect(baseStorageWithdrawSelection_),
                    48.0f);
                detailEntry = storageTransferTargetView(storageWithdrawTargetForSlot(
                    std::clamp(baseStorageWithdrawSelection_, 0, StorageWithdrawSlotCount - 1)));
            }

            if (detailEntry.item == nullptr && selectedRingItem != nullptr) {
                drawUiSubPanel(renderer, detailPanel);
                float detailLineY = drawUiDetailHeader(renderer, detailPanel, ringItemDisplayName(objectCatalog_, *selectedRingItem));
                drawUiDetailText(renderer, detailPanel, detailLineY, "このアイテムは収納箱にしまえないよ");
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
            drawSmallActionInfoText(renderer, panel, panelTitle, "何を調整する？");
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
                ringTabLabels[static_cast<std::size_t>(i)] = ringDisplayName(i, ringCount);
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
            const bool respecListFocused =
                !navigationUiCursorEnabled_ || respecTabsState.navigationFocused;
            const int selectedRespecTab =
                respecListFocused && baseRingWorkshopSelection_ < RingLevelUpgradeKindCount
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
                baseRingWorkshopSelection_ == RingLevelUpgradeKindCount ? "この内容で再調整" : ringLevelUpgradeKindName(selectedKind));
            drawUiDetailLine(renderer, detailPanel, detailY, "対象", ringDisplayName(ringIndex, ringCount));
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
                const std::string sourceName = ringDisplayNameWithSpaceSuffix(
                    ringWorkshopRespecSource_->ringIndex,
                    ringCount,
                    ringLevelUpgradeKindName(ringWorkshopRespecSource_->kind));
                drawUiDetailLine(renderer, detailPanel, detailY, "移動元", sourceName, Color{255, 230, 150, 255});
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
                ringWorkshopRespecChanged() ? "この内容で再調整" : "変更なし",
                baseRingWorkshopSelection_ == RingLevelUpgradeKindCount,
                confirmStyle);
        } else if (baseRingWorkshopMode_ == RingWorkshopMode::Upgrade) {
            const int ringCount = unlockedRingCount();
            const int ringIndex = std::clamp(baseRingWorkshopRingIndex_, 0, ringCount - 1);
            std::array<UiTabItem, SpellRingCount> ringTabs{};
            std::array<UiRect, SpellRingCount> ringTabRects{};
            std::array<std::string, SpellRingCount> ringTabLabels{};
            for (int i = 0; i < ringCount; ++i) {
                ringTabLabels[static_cast<std::size_t>(i)] = ringDisplayName(i, ringCount);
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
            const bool radiusAdjustable = maxMeters > minMeters + 0.001f;
            const UiSliderSpec radiusSliderSpec = ringWorkshopRadiusSliderSpec(minMeters, maxMeters);
            const UiRect radiusSliderRect = ringWorkshopRadiusSliderRect(scrollLayout);
            drawUiSlider(
                renderer,
                radiusSliderRect,
                radiusAdjustable ? currentMeters : radiusSliderSpec.minValue,
                radiusSliderSpec,
                ringWorkshopRadiusSliderState(),
                ringWorkshopRadiusSliderStyle(radiusAdjustable));
            char currentRadiusBuffer[32];
            char radiusRangeBuffer[48];
            std::snprintf(currentRadiusBuffer, sizeof(currentRadiusBuffer), "%.2fm", currentMeters);
            std::snprintf(radiusRangeBuffer, sizeof(radiusRangeBuffer), "（%.2f～%.2fm）", minMeters, maxMeters);
            const int radiusInfoScale = 2;
            const Vec2 currentRadiusSize = renderer.measureText(currentRadiusBuffer, radiusInfoScale);
            const Vec2 radiusRangeSize = renderer.measureText(radiusRangeBuffer, radiusInfoScale);
            Vec2 radiusInfoPos{
                radiusSliderRect.pos.x + (radiusSliderRect.size.x - currentRadiusSize.x - radiusRangeSize.x) * 0.5f,
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
                !navigationUiCursorEnabled_ || baseRingWorkshopUpgradeTabs_.navigationFocused
                    ? baseRingWorkshopSelection_
                    : -1,
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
                drawInlineTextRun(pos, inlineMaterialIconTag(MaterialType::MoonFragment) + std::string(materialTypeDisplayName(MaterialType::MoonFragment)) + " ", ui::Text);
                drawTextRun(pos, std::to_string(cost), numberColor, 2);
                drawTextRun(pos, "（", ui::TextMuted, 2);
                drawTextRun(pos, std::to_string(owned), numberColor, 2);
                drawTextRun(pos, "）", ui::TextMuted, 2);
                y += 31.0f;
            };
            const char* confirmLabel = "強化する";
            UiButtonState confirmState = UiButtonState::Unavailable;
            if (implemented) {
                const auto upgrade = ringWorkshopUpgradeForDisplayIndex(selected);
                confirmState = uiButtonState(ringWorkshopUpgradeExecutable(upgrade));
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
                drawUiDetailLine(renderer, detailPanel, detailY, "対象", ringDisplayName(ringIndex, ringCount));
                if (maxed) {
                    drawUiDetailLine(renderer, detailPanel, detailY, "効果", "上限到達済み", ui::TextMuted);
                    drawUiDetailLine(renderer, detailPanel, detailY, "必要素材", "なし", ui::TextMuted);
                    confirmLabel = "上限";
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
                drawUiDetailText(renderer, detailPanel, detailY, "この項目は現在利用できないよ");
                confirmLabel = "未実装";
            }
            drawUiButton(
                renderer,
                ringWorkshopUpgradeConfirmRect(),
                confirmLabel,
                false,
                confirmState,
                uiActionButtonStyle());
        }
    } else if (baseProcessingUiMode_ == ProcessingUiMode::ChooseAction) {
        drawSmallActionInfoText(renderer, panel, panelTitle, "作業台で何をする？");
        constexpr std::array<std::string_view, 2> Choices{"一括修理", "強化"};
        for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
            const UiButtonState buttonState = i == 0
                ? uiButtonState(processingBulkRepairExecutable())
                : UiButtonState::Enabled;
            drawUiButton(
                renderer,
                merchantActionChoiceRect(i),
                Choices[static_cast<std::size_t>(i)],
                i == baseProcessingActionSelection_,
                buttonState,
                uiActionButtonStyle());
        }
        if (roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan) {
            const int targetCount = processingBulkRepairTargetCount();
            const int moneyCost = processingBulkRepairMoneyCost();
            const int oreCost = processingBulkRepairOreCost();
            std::snprintf(
                buffer,
                sizeof(buffer),
                "一括修理: 対象%d個 / %dG / 強化鉱石%d",
                targetCount,
                moneyCost,
                oreCost);
            const Color textColor =
                money_ >= moneyCost && inventory_.materialCount(MaterialType::EnhancementOre) >= oreCost
                    ? ui::TextMuted
                    : Color{238, 82, 82, 255};
            renderer.drawText(smallActionSupplementaryTextPos(panel), buffer, textColor, 2);
        }
    } else if (baseProcessingUiMode_ == ProcessingUiMode::Enhance) {
        const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount());
        std::array<UiTabItem, BaseProcessingSourceCount> sourceTabs{};
        std::array<UiRect, BaseProcessingSourceCount> sourceTabRects{};
        for (int i = 0; i < sourceCount; ++i) {
            const bool enabled = !(roguelikeFacilityUiMode_ == RoguelikeFacilityUiMode::Artisan &&
                baseItemSourceIsWarehouse(i));
            sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(i, unlockedRingCount(), enabled);
            sourceTabRects[static_cast<std::size_t>(i)] = baseProcessingSourceRect(i, sourceCount);
        }
        drawUiTabs(
            renderer,
            baseProcessingSourceTabs_,
            baseProcessingSource_,
            sourceTabs.data(),
            sourceCount,
            sourceTabRects.data());
        drawUiButton(
            renderer,
            merchantSellSortButtonRect(),
            "並び替え",
            false,
            uiActionButtonStyle());

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
                encyclopedia_,
                balance_,
                ringIndex,
                selectedRingItem,
                ringPreviewSeconds);
        } else if (warehouseSource) {
            const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
            const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
            drawBaseWarehouseSourcePageSelector(
                renderer,
                baseProcessingGridSlotRect,
                warehousePage,
                warehousePageCount);
            for (int i = 0; i < StoragePaneSlotCount; ++i) {
                const InventoryUiEntryView view = entryViewForScreenSlot(i);
                const bool unavailable = view.item != nullptr && !processingTargetHasAvailableCommand(processingTargetForScreenSlot(i));
                InventoryUiSlotStyle style{i == baseProcessingSelection_, unavailable, 48.0f};
                applyInventoryUiPowerBadgeDiscovery(style, encyclopedia_);
                style.contentAlpha = itemGridInteractionContentAlpha(
                    baseItemInteraction_,
                    itemKeyForProcessingTarget(processingTargetForScreenSlot(i)).value_or(ItemKey{}));
                applyInventoryUiStackCount(style, view);
                drawInventoryUiSlot(renderer, baseItemSourceSlotRect(baseProcessingGridSlotRect, i), view, style);
            }
        } else {
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const InventoryUiEntryView view = entryViewForScreenSlot(i);
                const bool unavailable = view.item != nullptr && !processingTargetHasAvailableCommand(processingTargetForScreenSlot(i));
                InventoryUiSlotStyle style{i == baseProcessingSelection_, unavailable, 48.0f};
                applyInventoryUiPowerBadgeDiscovery(style, encyclopedia_);
                style.contentAlpha = itemGridInteractionContentAlpha(
                    baseItemInteraction_,
                    itemKeyForProcessingTarget(processingTargetForScreenSlot(i)).value_or(ItemKey{}));
                applyInventoryUiStackCount(style, view);
                drawInventoryUiSlot(renderer, baseItemSourceSlotRect(baseProcessingGridSlotRect, i), view, style);
            }
        }
        if (!ringSource) {
            const UiRect destination = baseItemSourceSlotRect(
                baseProcessingGridSlotRect,
                baseProcessingSelection_);
            drawGrabbedGridItem(destination, 48.0f);
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
            drawUiDetailText(renderer, detailPanel, detailLineY, "加工するアイテムを選択してね");
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
            drawSmallActionInfoText(renderer, panel, panelTitle, "何を見ていくんだい？");
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
            const auto sellTargetHasPrice = [this, &entryViewForSellTarget](MerchantSellTarget target) {
                if (!target.valid) {
                    return false;
                }
                const InventoryUiEntryView view = entryViewForSellTarget(target);
                return view.item != nullptr && isSellableObject(*view.item);
            };

            const UiRect detailPanel = merchantDetailPanelRect();
            drawMoneySummaryText(renderer, {detailPanel.pos.x, detailPanel.pos.y + 12.0f}, money_);

            InventoryUiEntryView detailEntry{};
            std::vector<InventoryUiDetailExtraLine> extraLines;
            const SpellRingItem* selectedRingItem = nullptr;
            if (buyMode) {
                if (merchantStock_.empty()) {
                    renderer.drawText({92.0f, 210.0f}, "商品がないよ", {198, 198, 206, 255}, 2);
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
                    applyInventoryUiPowerBadgeDiscovery(style, encyclopedia_);
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
                    sourceTabs[static_cast<std::size_t>(i)] = baseItemSourceTabItem(i, unlockedRingCount(), enabled);
                    sourceTabRects[static_cast<std::size_t>(i)] = merchantSellSourceRect(i, sourceCount);
                }
                drawUiTabs(
                    renderer,
                    baseMerchantSellSourceTabs_,
                    baseMerchantSellSource_,
                    sourceTabs.data(),
                    sourceCount,
                    sourceTabRects.data());
                if (baseMerchantBulkSell_.active) {
                    const MerchantBulkSellSummary bulkSummary = merchantBulkSellSummary();
                    drawUiButton(
                        renderer,
                        batchItemActionButtonRect(0),
                        "全選択",
                        false,
                        batchItemActionButtonStyle(true));
                    drawUiButton(
                        renderer,
                        batchItemActionButtonRect(1),
                        "全解除",
                        false,
                        batchItemActionButtonStyle(bulkSummary.itemCount > 0));
                    drawUiButton(
                        renderer,
                        batchItemActionButtonRect(2),
                        "売却",
                        false,
                        batchItemActionButtonStyle(bulkSummary.itemCount > 0));
                    renderer.drawText(
                        batchItemSelectionSummaryPos(),
                        batchItemSelectionCountText(bulkSummary.itemCount),
                        ui::Text,
                        2);
                } else {
                    drawUiButton(renderer, merchantSellSortButtonRect(), "並び替え", false, uiActionButtonStyle());
                    drawUiButton(renderer, batchItemModeButtonRect(), "まとめて売る", false, uiActionButtonStyle());
                }

                const bool warehouseSource = baseItemSourceIsWarehouse(baseMerchantSellSource_);
                const bool ringSource = baseItemSourceIsRing(baseMerchantSellSource_);
                const auto sellTargetBottomLabel = [this, &sellTargetHasPrice](
                    MerchantSellTarget target,
                    std::string& outLabel,
                    Color& outColor) {
                    outLabel.clear();
                    if (!sellTargetHasPrice(target)) {
                        return false;
                    }

                    char priceBuffer[32];
                    std::snprintf(priceBuffer, sizeof(priceBuffer), "%dG", merchantSellTargetPrice(target));
                    outLabel = priceBuffer;
                    outColor = merchantSellTargetAvailable(target) ? ui::Text : ui::TextDisabled;
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
                        encyclopedia_,
                        balance_,
                        ringIndex,
                        selectedRingIndex,
                        ringPreviewSeconds);
                    for (int i = 0; i < static_cast<int>(ringItems.size()); ++i) {
                        const MerchantSellTarget target = merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                        const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(i)];
                        UiRect labelRect = merchantSellRingItemRect(
                            ringItem,
                            spellRing_,
                            balance_,
                            ringIndex,
                            i,
                            static_cast<int>(ringItems.size()),
                            ringPreviewSeconds);
                        labelRect.size.y += RingItemBottomLabelExtraHeight;
                        std::string label;
                        Color labelColor = ui::Text;
                        if (sellTargetBottomLabel(target, label, labelColor)) {
                            drawInventoryUiSlotBottomLabel(renderer, labelRect, label, labelColor);
                        }
                        if (targetHighValue(target)) {
                            drawHighValueLabel(labelRect);
                        }
                        if (baseMerchantBulkSell_.active && merchantBulkSellTargetSelected(target)) {
                            drawBatchItemSelectionBadge(renderer, labelRect);
                        }
                    }
                    if (!ringItems.empty() && selectedRingIndex >= 0) {
                        selectedRingItem = &ringItems[static_cast<std::size_t>(selectedRingIndex)];
                    }
                } else if (warehouseSource) {
                    const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
                    const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                    drawBaseWarehouseSourcePageSelector(
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
                        applyInventoryUiPowerBadgeDiscovery(style, encyclopedia_);
                        style.contentAlpha = itemGridInteractionContentAlpha(
                            baseItemInteraction_,
                            itemKeyForBaseItemTarget(target).value_or(ItemKey{}));
                        if (view.item != nullptr) {
                            applySellTargetBottomLabel(style, target);
                        }
                        applyInventoryUiStackCount(style, view);
                        drawInventoryUiSlot(renderer, baseItemSourceSlotRect(merchantSellGridSlotRect, i), view, style);
                        if (targetHighValue(target)) {
                            drawHighValueLabel(baseItemSourceSlotRect(merchantSellGridSlotRect, i));
                        }
                        if (baseMerchantBulkSell_.active && merchantBulkSellTargetSelected(target)) {
                            drawBatchItemSelectionBadge(
                                renderer,
                                baseItemSourceSlotRect(merchantSellGridSlotRect, i));
                        }
                    }
                } else {
                    for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                        const MerchantSellTarget target = merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                        const InventoryUiEntryView view = entryViewForSellTarget(target);
                        const std::string_view blockedLabel = blockedSellLabel(target);
                    const bool disabled = view.item != nullptr && !blockedLabel.empty();
                    const UiRect rect = baseItemSourceSlotRect(merchantSellGridSlotRect, i);
                    InventoryUiSlotStyle style{i == baseSellSelection_, disabled, 48.0f};
                    applyInventoryUiPowerBadgeDiscovery(style, encyclopedia_);
                    style.contentAlpha = itemGridInteractionContentAlpha(
                            baseItemInteraction_,
                            itemKeyForBaseItemTarget(target).value_or(ItemKey{}));
                        if (view.item != nullptr) {
                            applySellTargetBottomLabel(style, target);
                        }
                        applyInventoryUiStackCount(style, view);
                        drawInventoryUiSlot(renderer, rect, view, style);
                        if (targetHighValue(target)) {
                            drawHighValueLabel(rect);
                        }
                        if (baseMerchantBulkSell_.active && merchantBulkSellTargetSelected(target)) {
                            drawBatchItemSelectionBadge(renderer, rect);
                        }
                    }
                }
                if (!ringSource) {
                    const UiRect destination = baseItemSourceSlotRect(
                        merchantSellGridSlotRect,
                        baseSellSelection_);
                    drawGrabbedGridItem(destination, 48.0f);
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
            }

            if (!buyMode && detailEntry.item == nullptr && selectedRingItem != nullptr) {
                drawUiSubPanel(renderer, detailPanel);
                drawUiDetailHeader(renderer, detailPanel, ringItemDisplayName(objectCatalog_, *selectedRingItem));
            } else {
                drawInventoryUiDetailPanel(
                    renderer,
                    detailPanel,
                    detailEntry,
                    objectCatalog_,
                    encyclopedia_,
                    InventoryUiDetailOptions{
                        .animationSeconds = ringPreviewSeconds,
                    },
                    extraLines);
            }
            if (buyMode) {
                const bool buyCommandEnabled = baseMerchantBuyCommandIndex_ >= 0 &&
                    baseMerchantBuyCommandIndex_ < static_cast<int>(merchantStock_.size()) &&
                    canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(baseMerchantBuyCommandIndex_)]);
                const std::array<UiCommandMenuItem, 1> buyItems{{{"買う", buyCommandEnabled}}};
                drawUiCommandMenu(renderer, baseMerchantBuyCommandMenu_, buyItems.data(), static_cast<int>(buyItems.size()));
            } else if (!baseMerchantBulkSell_.active) {
                const std::array<UiCommandMenuItem, 1> sellItems{{{"売る", true}}};
                drawUiCommandMenu(
                    renderer,
                    baseMerchantSellCommandMenu_,
                    sellItems.data(),
                    static_cast<int>(sellItems.size()));
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
            !navigationUiCursorEnabled_ || baseUpgradeTabs_.navigationFocused
                ? displaySelection
                : -1,
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
            drawInlineTextRun(pos, inlineMaterialIconTag(type) + std::string(materialTypeDisplayName(type)) + " ", ui::Text);
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

        const char* confirmLabel = "強化する";
        if (!implemented) {
            confirmLabel = "未実装";
        } else if (maxed) {
            confirmLabel = "上限";
        }
        drawUiButton(
            renderer,
            baseUpgradeConfirmRect(),
            confirmLabel,
            false,
            uiButtonState(upgradeExecutable(selected)),
            uiActionButtonStyle());
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
        drawUiArrowButton(
            renderer,
            stageSelector.prev,
            UiArrowDirection::Left,
            UiArrowButtonVariant::Standard,
            canSelectDestination);
        drawUiArrowButton(
            renderer,
            stageSelector.next,
            UiArrowDirection::Right,
            UiArrowButtonVariant::Standard,
            canSelectDestination);
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
                const UiButtonState buttonState = uiButtonState(!disabled);
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
                const bool hot = i == baseMiningStartSelection_ && !baseWarpPointSelectActive_;
                if (i == 1) {
                    drawUiButton(renderer, rect, "", hot, buttonState, buttonStyle);
                    InlineItemTextStyle buttonTextStyle;
                    buttonTextStyle.text = uiButtonStyleForState(buttonStyle, buttonState).text;
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
                    drawUiButton(renderer, rect, baseMiningStartChoiceName(i), hot, buttonState, buttonStyle);
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
            UiNavigationLayerScope warpNavigationScope;
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
                "↑/↓ 選択  F/Enter 出発  Esc 戻る",
                UiWindowOptions{true, true});

            renderer.drawText(warpPanel.pos + Vec2{48.0f, 82.0f}, "どのワープポイントにする？", {198, 198, 206, 255}, 2);
            if (selectableWarpPoints.empty()) {
                renderer.drawText(warpPanel.pos + Vec2{48.0f, 142.0f}, "解放済みワープポイントがないよ", ui::TextDisabled, 2);
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
                drawUiButton(renderer, rect, "", hot, buttonStyle, UiNavigationRole::Grid);

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
    if (baseStorageBatchSelection_.confirm.open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});

        drawUiConfirmDialog(
            renderer,
            baseStorageBatchSelection_.confirm,
            batchItemConfirmRect(),
            baseStorageMode_ == StorageUiMode::Withdraw
                ? "base.storage.bulk_withdraw.confirm"
                : "base.storage.bulk_deposit.confirm");
    }
    if (baseMerchantBulkSell_.confirm.open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});

        drawUiConfirmDialog(
            renderer,
            baseMerchantBulkSell_.confirm,
            batchItemConfirmRect(),
            "base.merchant.bulk_sell.confirm");
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
    if (ringWorkshopRespecConfirmState().open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});

        drawRingWorkshopRespecConfirmDialog(renderer, baseProcessingConfirmRect());
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
    if (baseQuantityDialog_.open) {
        panelCancelScope.reset();
        panelWindow.reset();
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});
        drawUiQuantityDialog(
            renderer,
            baseQuantityDialog_,
            baseQuantityDialogRect({
                static_cast<float>(camera_.width()),
                static_cast<float>(camera_.height()),
            }),
            "base.quantity");
    }

}

} // namespace majo
