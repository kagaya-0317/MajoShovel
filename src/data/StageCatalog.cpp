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
    int specialRoomCount)
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
        defaultStage("stage_01_stardust", "星くずの浅坑", "ストーリー", 10, "natural_cave", "soft_stardust", 320, 0.30, 0.25, 1.00, 1.00, 3, 1),
        defaultStage("stage_02_junk_magic", "魔導廃棄層", "ストーリー", 20, "junk_layer", "junk_mixed", 420, 0.38, 0.32, 1.12, 1.45, 4, 2),
        defaultStage("stage_03_star_core", "落星核の深層", "ストーリー", 30, "star_core", "hard_star_core", 540, 0.44, 0.28, 0.90, 2.20, 5, 2),
        defaultStage("stage_04_astral_mine", "星間廃坑", "ローグライク", 40, "astral_rogue", "chaos_astral", 640, 0.55, 0.45, 1.25, 1.80, 0, 5),
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
