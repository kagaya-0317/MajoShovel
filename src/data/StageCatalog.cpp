#include "data/StageCatalog.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace majo {

namespace {

constexpr int DefaultDisplayOrder = 999;
constexpr int DefaultGoalDistanceTiles = 320;
constexpr double DefaultDetourRate = 0.30;
constexpr double DefaultBranchDensity = 0.25;
constexpr double DefaultCavernWidthMultiplier = 1.00;
constexpr double DefaultTerrainHardnessMultiplier = 1.00;
constexpr int DefaultWarpPointCount = 0;
constexpr int DefaultSpecialRoomCount = 0;

constexpr std::array<std::string_view, 4> KnownGenerationProfiles = {
    "natural_cave",
    "junk_layer",
    "star_core",
    "astral_rogue",
};

constexpr std::array<std::string_view, 4> KnownTerrainProfiles = {
    "soft_stardust",
    "junk_mixed",
    "hard_star_core",
    "chaos_astral",
};

std::string trim(std::string_view text)
{
    auto begin = text.begin();
    auto end = text.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

bool equalsIgnoreCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }
    return true;
}

bool headerMatches(std::string_view header, std::initializer_list<std::string_view> candidates)
{
    const std::string normalized = trim(header);
    for (std::string_view candidate : candidates) {
        if (normalized == candidate || equalsIgnoreCase(normalized, candidate)) {
            return true;
        }
    }
    return false;
}

int findColumn(const GoogleSheetRow& headers, std::initializer_list<std::string_view> names)
{
    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (headerMatches(headers[i], names)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string cellAt(const GoogleSheetRow& row, int column)
{
    if (column < 0 || static_cast<std::size_t>(column) >= row.size()) {
        return {};
    }
    return trim(row[static_cast<std::size_t>(column)]);
}

bool hasRowContent(const GoogleSheetRow& row)
{
    return std::any_of(row.begin(), row.end(), [](const std::string& cell) {
        return !trim(cell).empty();
    });
}

bool parseIntStrict(std::string_view text, int& value)
{
    const std::string copy = trim(text);
    if (copy.empty()) {
        return false;
    }
    const char* begin = copy.data();
    const char* end = copy.data() + copy.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseDoubleStrict(std::string_view text, double& value)
{
    const std::string copy = trim(text);
    if (copy.empty()) {
        return false;
    }
    errno = 0;
    char* parsedEnd = nullptr;
    value = std::strtod(copy.c_str(), &parsedEnd);
    return errno != ERANGE &&
        parsedEnd == copy.c_str() + copy.size() &&
        std::isfinite(value);
}

bool isKnown(std::string_view key, const auto& knownValues)
{
    return std::any_of(knownValues.begin(), knownValues.end(), [key](std::string_view known) {
        return key == known;
    });
}

std::string defaultBossEnemyIdForStage(std::string_view stageId)
{
    if (stageId == "stage_01_stardust") {
        return "stardust_mole";
    }
    if (stageId == "stage_02_junk_magic") {
        return "junk_crab";
    }
    if (stageId == "stage_03_star_core") {
        return "astragna";
    }
    if (stageId == "stage_04_astral_mine") {
        return "star_vein_dragon";
    }
    return {};
}

void addWarning(StageCatalog& catalog, std::string message)
{
    catalog.validationWarnings.push_back(std::move(message));
}

std::string rowPrefix(std::size_t rowIndex, std::string_view stageId)
{
    std::string prefix = "Stages row " + std::to_string(rowIndex + 1);
    if (!stageId.empty()) {
        prefix += " stage=\"" + std::string(stageId) + "\"";
    }
    return prefix + ": ";
}

struct StageColumns {
    int id = -1;
    int name = -1;
    int type = -1;
    int displayOrder = -1;
    int implementationState = -1;
    int generationProfile = -1;
    int terrainProfile = -1;
    int goalDistanceTiles = -1;
    int detourRate = -1;
    int branchDensity = -1;
    int cavernWidthMultiplier = -1;
    int terrainHardnessMultiplier = -1;
    int warpPointCount = -1;
    int specialRoomCount = -1;
    int bossEnemyId = -1;
    int detailImagePath = -1;
    int detailDescription = -1;
    int detailDifficulty = -1;
    int detailSize = -1;
    int detailWallHardness = -1;
    int detailTerrainComplexity = -1;
    int detailEnemyIds = -1;
};

void warnMissingColumn(StageCatalog& catalog, int column, std::string_view columnName)
{
    if (column < 0) {
        addWarning(catalog, "Stages sheet is missing required column \"" + std::string(columnName) + "\"");
    }
}

StageColumns findStageColumns(const GoogleSheetRow& headers, StageCatalog& catalog)
{
    StageColumns columns;
    columns.id = findColumn(headers, {"ステージID", "stage_id", "stageId", "id", "ID"});
    columns.name = findColumn(headers, {"ステージ名", "stage_name", "stageName", "name"});
    columns.type = findColumn(headers, {"種別", "type", "stage_type", "stageType"});
    columns.displayOrder = findColumn(headers, {"表示順", "display_order", "displayOrder", "order"});
    columns.implementationState = findColumn(headers, {"実装状態", "implementation_state", "implementationState", "status"});
    columns.generationProfile = findColumn(headers, {"生成プロファイル", "generation_profile", "generationProfile"});
    columns.terrainProfile = findColumn(headers, {"地形プロファイル", "terrain_profile", "terrainProfile"});
    columns.goalDistanceTiles = findColumn(headers, {"ゴール距離タイル", "goal_distance_tiles", "goalDistanceTiles"});
    columns.detourRate = findColumn(headers, {"迂回度", "detour_rate", "detourRate"});
    columns.branchDensity = findColumn(headers, {"分岐密度", "branch_density", "branchDensity"});
    columns.cavernWidthMultiplier = findColumn(headers, {"空洞幅倍率", "cavern_width_multiplier", "cavernWidthMultiplier"});
    columns.terrainHardnessMultiplier = findColumn(headers, {"地形硬度倍率", "terrain_hardness_multiplier", "terrainHardnessMultiplier"});
    columns.warpPointCount = findColumn(headers, {"ワープポイント数", "warp_point_count", "warpPointCount"});
    columns.specialRoomCount = findColumn(headers, {"特殊部屋数", "special_room_count", "specialRoomCount"});
    columns.bossEnemyId = findColumn(headers, {"ボス敵ID", "boss_enemy_id", "bossEnemyId"});
    columns.detailImagePath = findColumn(headers, {"詳細画像", "詳細イメージ", "detail_image_path", "detailImagePath", "image_path", "imagePath"});
    columns.detailDescription = findColumn(headers, {"詳細説明", "説明文", "detail_description", "detailDescription", "description"});
    columns.detailDifficulty = findColumn(headers, {"難易度", "detail_difficulty", "detailDifficulty", "difficulty"});
    columns.detailSize = findColumn(headers, {"広さ", "detail_size", "detailSize", "size_label", "sizeLabel"});
    columns.detailWallHardness = findColumn(headers, {"壁の固さ", "壁硬度", "detail_wall_hardness", "detailWallHardness", "wall_hardness", "wallHardness"});
    columns.detailTerrainComplexity = findColumn(headers, {"地形の複雑さ", "地形複雑度", "detail_terrain_complexity", "detailTerrainComplexity", "terrain_complexity", "terrainComplexity"});
    columns.detailEnemyIds = findColumn(headers, {"出現する敵", "出現敵", "detail_enemy_ids", "detailEnemyIds", "enemy_ids", "enemyIds"});

    warnMissingColumn(catalog, columns.id, "ステージID");
    warnMissingColumn(catalog, columns.name, "ステージ名");
    warnMissingColumn(catalog, columns.type, "種別");
    warnMissingColumn(catalog, columns.displayOrder, "表示順");
    warnMissingColumn(catalog, columns.implementationState, "実装状態");
    warnMissingColumn(catalog, columns.generationProfile, "生成プロファイル");
    warnMissingColumn(catalog, columns.terrainProfile, "地形プロファイル");
    warnMissingColumn(catalog, columns.goalDistanceTiles, "ゴール距離タイル");
    warnMissingColumn(catalog, columns.detourRate, "迂回度");
    warnMissingColumn(catalog, columns.branchDensity, "分岐密度");
    warnMissingColumn(catalog, columns.cavernWidthMultiplier, "空洞幅倍率");
    warnMissingColumn(catalog, columns.terrainHardnessMultiplier, "地形硬度倍率");
    warnMissingColumn(catalog, columns.warpPointCount, "ワープポイント数");
    warnMissingColumn(catalog, columns.specialRoomCount, "特殊部屋数");
    return columns;
}

int parseIntColumnOrDefault(
    const GoogleSheetRow& row,
    int column,
    int defaultValue,
    std::string_view columnName,
    std::size_t rowIndex,
    std::string_view stageId,
    StageCatalog& catalog)
{
    if (column < 0) {
        return defaultValue;
    }

    const std::string text = cellAt(row, column);
    int value = defaultValue;
    if (!parseIntStrict(text, value)) {
        addWarning(
            catalog,
            rowPrefix(rowIndex, stageId) + std::string(columnName) +
                " is not a number \"" + text + "\"; using " + std::to_string(defaultValue));
        return defaultValue;
    }
    return value;
}

double parseDoubleColumnOrDefault(
    const GoogleSheetRow& row,
    int column,
    double defaultValue,
    std::string_view columnName,
    std::size_t rowIndex,
    std::string_view stageId,
    StageCatalog& catalog)
{
    if (column < 0) {
        return defaultValue;
    }

    const std::string text = cellAt(row, column);
    double value = defaultValue;
    if (!parseDoubleStrict(text, value)) {
        addWarning(
            catalog,
            rowPrefix(rowIndex, stageId) + std::string(columnName) +
                " is not a number \"" + text + "\"; using " + std::to_string(defaultValue));
        return defaultValue;
    }
    return value;
}

std::vector<std::string> parseListColumn(const GoogleSheetRow& row, int column)
{
    std::vector<std::string> values;
    if (column < 0) {
        return values;
    }

    const std::string text = cellAt(row, column);
    std::string token;
    const auto flushToken = [&]() {
        token = trim(token);
        if (!token.empty()) {
            values.push_back(token);
        }
        token.clear();
    };
    for (char ch : text) {
        if (ch == ',' || ch == ';' || ch == '/' || static_cast<unsigned char>(ch) <= ' ') {
            flushToken();
        } else {
            token.push_back(ch);
        }
    }
    flushToken();
    return values;
}

void validateStage(StageDefinition& stage, std::size_t rowIndex, StageCatalog& catalog)
{
    if (stage.name.empty()) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "stage name is empty");
    }
    if (stage.generationProfile.empty()) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "generation profile is empty");
    } else if (!isKnown(stage.generationProfile, KnownGenerationProfiles)) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "unknown generation profile \"" + stage.generationProfile + "\"");
    }
    if (stage.terrainProfile.empty()) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "terrain profile is empty");
    } else if (!isKnown(stage.terrainProfile, KnownTerrainProfiles)) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "unknown terrain profile \"" + stage.terrainProfile + "\"");
    }
    if (stage.goalDistanceTiles < 0) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "goal distance tiles is negative; using " + std::to_string(DefaultGoalDistanceTiles));
        stage.goalDistanceTiles = DefaultGoalDistanceTiles;
    }
    if (stage.detourRate < 0.0) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "detour rate is negative; using " + std::to_string(DefaultDetourRate));
        stage.detourRate = DefaultDetourRate;
    }
    if (stage.branchDensity < 0.0) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "branch density is negative; using " + std::to_string(DefaultBranchDensity));
        stage.branchDensity = DefaultBranchDensity;
    }
    if (stage.cavernWidthMultiplier <= 0.0) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "cavern width multiplier must be greater than 0; using " + std::to_string(DefaultCavernWidthMultiplier));
        stage.cavernWidthMultiplier = DefaultCavernWidthMultiplier;
    }
    if (stage.terrainHardnessMultiplier <= 0.0) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "terrain hardness multiplier must be greater than 0; using " + std::to_string(DefaultTerrainHardnessMultiplier));
        stage.terrainHardnessMultiplier = DefaultTerrainHardnessMultiplier;
    }
    if (stage.warpPointCount < 0) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "warp point count is negative; using " + std::to_string(DefaultWarpPointCount));
        stage.warpPointCount = DefaultWarpPointCount;
    }
    if (stage.specialRoomCount < 0) {
        addWarning(catalog, rowPrefix(rowIndex, stage.id) + "special room count is negative; using " + std::to_string(DefaultSpecialRoomCount));
        stage.specialRoomCount = DefaultSpecialRoomCount;
    }
}

StageDefinition defaultStage(
    std::string id,
    std::string name,
    std::string type,
    int displayOrder,
    std::string generationProfile,
    std::string terrainProfile,
    int goalDistanceTiles,
    double detourRate,
    double branchDensity,
    double cavernWidthMultiplier,
    double terrainHardnessMultiplier,
    int warpPointCount,
    int specialRoomCount,
    std::string bossEnemyId,
    StageDefinition::DisplayDetail detail)
{
    StageDefinition stage;
    stage.id = std::move(id);
    stage.name = std::move(name);
    stage.type = std::move(type);
    stage.displayOrder = displayOrder;
    stage.implementationState = "fallback";
    stage.generationProfile = std::move(generationProfile);
    stage.terrainProfile = std::move(terrainProfile);
    stage.goalDistanceTiles = goalDistanceTiles;
    stage.detourRate = detourRate;
    stage.branchDensity = branchDensity;
    stage.cavernWidthMultiplier = cavernWidthMultiplier;
    stage.terrainHardnessMultiplier = terrainHardnessMultiplier;
    stage.warpPointCount = warpPointCount;
    stage.specialRoomCount = specialRoomCount;
    stage.bossEnemyId = std::move(bossEnemyId);
    stage.detail = std::move(detail);
    return stage;
}

void rebuildStageIndex(StageCatalog& catalog)
{
    catalog.stagesById.clear();
    for (std::size_t i = 0; i < catalog.stages.size(); ++i) {
        catalog.stagesById.emplace(catalog.stages[i].id, i);
    }
}

}

const StageDefinition* StageCatalog::getStageById(std::string_view id) const
{
    const auto it = stagesById.find(std::string(id));
    if (it == stagesById.end()) {
        return nullptr;
    }
    return &stages[it->second];
}

std::vector<StageDefinition> StageCatalog::getStagesSortedByDisplayOrder() const
{
    std::vector<StageDefinition> sorted = stages;
    std::stable_sort(sorted.begin(), sorted.end(), [](const StageDefinition& left, const StageDefinition& right) {
        if (left.displayOrder != right.displayOrder) {
            return left.displayOrder < right.displayOrder;
        }
        return left.id < right.id;
    });
    return sorted;
}

bool StageCatalog::loadFromCsv(std::string_view csv, std::string& outError)
{
    GoogleSheetTable table;
    if (!parseGoogleSheetCsv(csv, table, outError)) {
        *this = StageCatalog{};
        return false;
    }
    return loadFromTable(table, outError);
}

bool StageCatalog::loadFromTable(const GoogleSheetTable& table, std::string& outError)
{
    StageCatalog catalog;
    if (table.rows.empty()) {
        outError = "Stages sheet is empty";
        *this = catalog;
        return false;
    }

    const StageColumns columns = findStageColumns(table.rows.front(), catalog);
    if (columns.name < 0 && columns.generationProfile < 0 && columns.terrainProfile < 0) {
        outError = "Stages sheet is missing core stage columns; possible wrong sheet";
        *this = catalog;
        return false;
    }

    std::unordered_set<std::string> seenIds;
    for (std::size_t rowIndex = 1; rowIndex < table.rows.size(); ++rowIndex) {
        const GoogleSheetRow& row = table.rows[rowIndex];
        if (!hasRowContent(row)) {
            continue;
        }

        StageDefinition stage;
        stage.id = cellAt(row, columns.id);
        if (stage.id.empty()) {
            addWarning(catalog, rowPrefix(rowIndex, stage.id) + "stage ID is empty; row skipped");
            continue;
        }

        stage.name = cellAt(row, columns.name);
        stage.type = cellAt(row, columns.type);
        stage.displayOrder = parseIntColumnOrDefault(row, columns.displayOrder, DefaultDisplayOrder, "display order", rowIndex, stage.id, catalog);
        stage.implementationState = cellAt(row, columns.implementationState);
        stage.generationProfile = cellAt(row, columns.generationProfile);
        stage.terrainProfile = cellAt(row, columns.terrainProfile);
        stage.goalDistanceTiles = parseIntColumnOrDefault(row, columns.goalDistanceTiles, DefaultGoalDistanceTiles, "goal distance tiles", rowIndex, stage.id, catalog);
        stage.detourRate = parseDoubleColumnOrDefault(row, columns.detourRate, DefaultDetourRate, "detour rate", rowIndex, stage.id, catalog);
        stage.branchDensity = parseDoubleColumnOrDefault(row, columns.branchDensity, DefaultBranchDensity, "branch density", rowIndex, stage.id, catalog);
        stage.cavernWidthMultiplier = parseDoubleColumnOrDefault(row, columns.cavernWidthMultiplier, DefaultCavernWidthMultiplier, "cavern width multiplier", rowIndex, stage.id, catalog);
        stage.terrainHardnessMultiplier = parseDoubleColumnOrDefault(row, columns.terrainHardnessMultiplier, DefaultTerrainHardnessMultiplier, "terrain hardness multiplier", rowIndex, stage.id, catalog);
        stage.warpPointCount = parseIntColumnOrDefault(row, columns.warpPointCount, DefaultWarpPointCount, "warp point count", rowIndex, stage.id, catalog);
        stage.specialRoomCount = parseIntColumnOrDefault(row, columns.specialRoomCount, DefaultSpecialRoomCount, "special room count", rowIndex, stage.id, catalog);
        stage.bossEnemyId = cellAt(row, columns.bossEnemyId);
        if (stage.bossEnemyId.empty()) {
            stage.bossEnemyId = defaultBossEnemyIdForStage(stage.id);
        }
        stage.detail.imagePath = cellAt(row, columns.detailImagePath);
        stage.detail.description = cellAt(row, columns.detailDescription);
        stage.detail.difficulty = cellAt(row, columns.detailDifficulty);
        stage.detail.size = cellAt(row, columns.detailSize);
        stage.detail.wallHardness = cellAt(row, columns.detailWallHardness);
        stage.detail.terrainComplexity = cellAt(row, columns.detailTerrainComplexity);
        stage.detail.enemyIds = parseListColumn(row, columns.detailEnemyIds);

        if (!seenIds.insert(stage.id).second) {
            addWarning(catalog, rowPrefix(rowIndex, stage.id) + "duplicate stage ID; first map entry is kept");
        }

        validateStage(stage, rowIndex, catalog);
        catalog.stages.push_back(std::move(stage));
    }

    rebuildStageIndex(catalog);
    if (catalog.stages.empty()) {
        outError = "Stages sheet has no valid stage rows";
        *this = catalog;
        return false;
    }

    *this = std::move(catalog);
    outError.clear();
    return true;
}

void StageCatalog::loadDefaultStages()
{
    StageCatalog catalog;
    catalog.stages = {
        defaultStage(
            "stage_01_stardust",
            "星くずのダンジョン",
            "ストーリー",
            10,
            "natural_cave",
            "soft_stardust",
            320,
            0.30,
            0.25,
            1.00,
            1.00,
            3,
            1,
            "stardust_mole",
            StageDefinition::DisplayDetail{
                .description = "星くずが積もる、最初の坑道。\n小さな空洞が多く、基本の採掘と戦闘を覚えやすい。",
                .difficulty = "やさしい",
                .size = "広くない",
                .wallHardness = "やわらかめ",
                .terrainComplexity = "そこまで",
                .enemyIds = {"slime", "bake_kinoko", "uji_uji", "shield_beetle", "web_spider", "ore_crab", "bomb_tsuchinoko", "stardust_mole"},
            }),
        defaultStage(
            "stage_02_junk_magic",
            "魔導具廃棄層",
            "ストーリー",
            20,
            "junk_layer",
            "junk_mixed",
            420,
            0.38,
            0.32,
            1.12,
            1.45,
            4,
            2,
            "junk_crab",
            StageDefinition::DisplayDetail{
                .description = "古い魔導具が埋まる廃棄層。\n硬い壁と寄り道が増え、敵の攻撃も少し厄介になる。",
                .difficulty = "ふつう",
                .size = "広い",
                .wallHardness = "ふつう",
                .terrainComplexity = "少し",
                .enemyIds = {"shield_beetle", "web_spider", "ore_crab", "bomb_tsuchinoko", "junk_crab"},
            }),
        defaultStage(
            "stage_03_star_core",
            "落星の眠る地底",
            "ストーリー",
            30,
            "star_core",
            "hard_star_core",
            540,
            0.44,
            0.28,
            0.90,
            2.20,
            5,
            2,
            "astragna",
            StageDefinition::DisplayDetail{
                .description = "落ちた星の核へ続く深い地底。\n壁はかなり硬く、少ない通路で強敵と向き合うことになる。",
                .difficulty = "むずかしい",
                .size = "かなり広い",
                .wallHardness = "かため",
                .terrainComplexity = "複雑",
                .enemyIds = {"ore_crab", "bomb_tsuchinoko", "astragna"},
            }),
        defaultStage(
            "stage_04_astral_mine",
            "不可思議の迷宮",
            "ローグライク",
            40,
            "astral_rogue",
            "chaos_astral",
            640,
            0.55,
            0.45,
            1.25,
            1.80,
            0,
            5,
            "star_vein_dragon",
            StageDefinition::DisplayDetail{
                .description = "入るたび姿を変える底なしの迷宮。\n持ち込み不可で、初期ステータスから深層を目指す。",
                .difficulty = "不明",
                .size = "底なし",
                .wallHardness = "不明",
                .terrainComplexity = "不明",
                .enemyIds = {"slime", "bake_kinoko", "uji_uji", "shield_beetle", "web_spider", "ore_crab", "bomb_tsuchinoko", "junk_crab", "astragna", "star_vein_dragon"},
            }),
    };
    rebuildStageIndex(catalog);
    *this = std::move(catalog);
}

bool loadStageCatalogFromGoogleSheet(const GoogleSheetSourceConfig& config, StageCatalog& outCatalog, std::string& outError)
{
    GoogleSheetTable table;
    if (!loadGoogleSheetTableForSheet(config, config.stagesSheet, table, outError)) {
        return false;
    }

    StageCatalog catalog;
    if (!catalog.loadFromTable(table, outError)) {
        outCatalog = std::move(catalog);
        return false;
    }

    outCatalog = std::move(catalog);
    outError.clear();
    return true;
}

}
