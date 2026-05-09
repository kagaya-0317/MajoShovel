#include "data/EnemyCatalog.hpp"

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

constexpr std::array<std::string_view, 9> AllowedDamageTypes = {
    "none",
    "physical",
    "fire",
    "ice",
    "thunder",
    "wind",
    "earth",
    "water",
    "magic",
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

bool isNone(std::string_view text)
{
    const std::string value = trim(text);
    return value.empty() || equalsIgnoreCase(value, "none") || value == "-";
}

bool isAllowedDamageType(std::string_view value)
{
    return std::any_of(AllowedDamageTypes.begin(), AllowedDamageTypes.end(), [value](std::string_view allowed) {
        return value == allowed;
    });
}

void addIssue(
    EnemyCatalog& catalog,
    DbValidationSeverity severity,
    DbValidationCategory category,
    std::string message)
{
    if (severity == DbValidationSeverity::Warning) {
        catalog.validationWarnings.push_back(message);
    }
    catalog.validationIssues.push_back(DbValidationIssue{
        .severity = severity,
        .category = category,
        .message = std::move(message),
    });
}

std::vector<std::string> parseDelimitedValues(std::string_view text)
{
    std::vector<std::string> values;
    std::string current;
    const auto flush = [&]() {
        std::string value = trim(current);
        if (!value.empty() && !isNone(value)) {
            values.push_back(std::move(value));
        }
        current.clear();
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == ',' || ch == ';' || ch == '|') {
            flush();
            continue;
        }
        if (i + 2 < text.size() &&
            ch == 0xE3 &&
            static_cast<unsigned char>(text[i + 1]) == 0x80 &&
            static_cast<unsigned char>(text[i + 2]) == 0x81) {
            flush();
            i += 2;
            continue;
        }
        current.push_back(text[i]);
    }
    flush();
    return values;
}

int parseIntColumnOrDefault(
    std::string_view text,
    int defaultValue,
    std::string_view sheet,
    std::size_t rowIndex,
    std::string_view id,
    std::string_view columnName,
    EnemyCatalog& catalog)
{
    if (trim(text).empty()) {
        return defaultValue;
    }
    int value = defaultValue;
    if (!parseIntStrict(text, value)) {
        addIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                " id=\"" + std::string(id) + "\" " + std::string(columnName) +
                ": invalid number \"" + trim(text) + "\"; using " + std::to_string(defaultValue));
        return defaultValue;
    }
    return value;
}

double parseDoubleColumnOrDefault(
    std::string_view text,
    double defaultValue,
    std::string_view sheet,
    std::size_t rowIndex,
    std::string_view id,
    std::string_view columnName,
    EnemyCatalog& catalog)
{
    if (trim(text).empty()) {
        return defaultValue;
    }
    double value = defaultValue;
    if (!parseDoubleStrict(text, value)) {
        addIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                " id=\"" + std::string(id) + "\" " + std::string(columnName) +
                ": invalid number \"" + trim(text) + "\"; using " + std::to_string(defaultValue));
        return defaultValue;
    }
    return value;
}

void validateDamageType(
    std::string& value,
    std::string_view sheet,
    std::size_t rowIndex,
    std::string_view id,
    std::string_view columnName,
    EnemyCatalog& catalog)
{
    if (value.empty()) {
        value = "none";
        return;
    }
    if (!isAllowedDamageType(value)) {
        addIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::ObjectField,
            std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                " id=\"" + std::string(id) + "\" " + std::string(columnName) +
                ": invalid damage type \"" + value + "\"; using none");
        value = "none";
    }
}

void parseEffectField(
    std::string_view text,
    std::vector<EffectSpec>& outEffects,
    std::string_view sheet,
    std::size_t rowIndex,
    std::string_view id,
    std::string_view fieldName,
    EnemyCatalog& catalog)
{
    outEffects.clear();
    if (isNone(text)) {
        return;
    }

    std::string error;
    std::vector<std::string> warnings;
    if (!parseEffectSpecs(text, outEffects, error, &warnings)) {
        addIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::EffectSpecSyntax,
            std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                " id=\"" + std::string(id) + "\" " + std::string(fieldName) +
                ": invalid effect spec: " + error + "; using empty effects");
        outEffects.clear();
    }
    for (const std::string& warning : warnings) {
        addIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::EffectSpecSyntax,
            std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                " id=\"" + std::string(id) + "\" " + std::string(fieldName) + ": " + warning);
    }
}

struct EnemyColumns {
    int id = -1;
    int name = -1;
    int description = -1;
    int hp = -1;
    int contactAttackPower = -1;
    int contactDamageType = -1;
    int moveSpeed = -1;
    int radius = -1;
    int xp = -1;
    int money = -1;
    int enemyAi = -1;
    int enemyBehaviorId = -1;
    int enemyTags = -1;
    int captureDifficulty = -1;
    int capturedDescription = -1;
    int capturedEffectText = -1;
    int capturedAttackPower = -1;
    int capturedDamageType = -1;
    int capturedDigPower = -1;
    int capturedDurability = -1;
    int capturedWeight = -1;
    int capturedNormalEffects = -1;
    int capturedOrbitEffects = -1;
    int capturedTags = -1;
    int capturedBehaviorId = -1;
    int note = -1;
};

bool findEnemyColumns(const GoogleSheetRow& headers, EnemyColumns& outColumns, std::string& outError)
{
    EnemyColumns columns;
    columns.id = findColumn(headers, {"ID", "id"});
    columns.name = findColumn(headers, {"名前", "name"});
    columns.description = findColumn(headers, {"説明文", "description"});
    columns.hp = findColumn(headers, {"HP", "hp"});
    columns.contactAttackPower = findColumn(headers, {"接触攻撃力", "contact_attack_power"});
    columns.contactDamageType = findColumn(headers, {"接触ダメージ種別", "contact_damage_type"});
    columns.moveSpeed = findColumn(headers, {"移動速度", "move_speed"});
    columns.radius = findColumn(headers, {"半径", "radius"});
    columns.xp = findColumn(headers, {"経験値", "xp"});
    columns.money = findColumn(headers, {"お金", "money"});
    columns.enemyAi = findColumn(headers, {"敵AI", "enemy_ai"});
    columns.enemyBehaviorId = findColumn(headers, {"敵挙動ID", "enemy_behavior_id"});
    columns.enemyTags = findColumn(headers, {"敵特殊タグ", "enemy_tags"});
    columns.captureDifficulty = findColumn(headers, {"捕獲難度", "capture_difficulty"});
    columns.capturedDescription = findColumn(headers, {"捕獲後説明文", "captured_description"});
    columns.capturedEffectText = findColumn(headers, {"捕獲後効果テキスト", "captured_effect_text"});
    columns.capturedAttackPower = findColumn(headers, {"捕獲後攻撃力", "captured_attack_power"});
    columns.capturedDamageType = findColumn(headers, {"捕獲後ダメージ種別", "captured_damage_type"});
    columns.capturedDigPower = findColumn(headers, {"捕獲後掘削力", "captured_dig_power"});
    columns.capturedDurability = findColumn(headers, {"捕獲後耐久力", "captured_durability"});
    columns.capturedWeight = findColumn(headers, {"捕獲後重さ", "captured_weight"});
    columns.capturedNormalEffects = findColumn(headers, {"捕獲後通常効果", "captured_normal_effects"});
    columns.capturedOrbitEffects = findColumn(headers, {"捕獲後軌道効果", "captured_orbit_effects"});
    columns.capturedTags = findColumn(headers, {"捕獲後特殊タグ", "captured_tags"});
    columns.capturedBehaviorId = findColumn(headers, {"捕獲後挙動ID", "captured_behavior_id"});
    columns.note = findColumn(headers, {"備考", "note"});

    if (columns.id < 0) {
        outError = "Enemies sheet is missing the ID column";
        return false;
    }
    if (columns.name < 0) {
        outError = "Enemies sheet is missing the name column";
        return false;
    }
    outColumns = columns;
    return true;
}

struct BehaviorColumns {
    int id = -1;
    int displayName = -1;
    int usableField = -1;
    int classification = -1;
    int triggerCondition = -1;
    int baseProcess = -1;
    int defaultIntervalSeconds = -1;
    int defaultProjectileId = -1;
    int enemyTarget = -1;
    int capturedTarget = -1;
    int enemyDefaultEffects = -1;
    int capturedDefaultEffects = -1;
    int relatedTags = -1;
    int enemyFacingControl = -1;
    int capturedFacingControl = -1;
    int movementControl = -1;
    int duplicateRule = -1;
    int implementationState = -1;
    int note = -1;
};

bool findBehaviorColumns(const GoogleSheetRow& headers, BehaviorColumns& outColumns, std::string& outError)
{
    BehaviorColumns columns;
    columns.id = findColumn(headers, {"挙動ID", "behavior_id"});
    columns.displayName = findColumn(headers, {"表示名", "display_name"});
    columns.usableField = findColumn(headers, {"使用欄", "usable_field"});
    columns.classification = findColumn(headers, {"分類", "classification"});
    columns.triggerCondition = findColumn(headers, {"起動条件", "trigger_condition"});
    columns.baseProcess = findColumn(headers, {"基本処理", "base_process"});
    columns.defaultIntervalSeconds = findColumn(headers, {"既定間隔秒", "default_interval_seconds"});
    columns.defaultProjectileId = findColumn(headers, {"既定発射物ID", "default_projectile_id"});
    columns.enemyTarget = findColumn(headers, {"敵側対象", "enemy_target"});
    columns.capturedTarget = findColumn(headers, {"捕獲後対象", "captured_target"});
    columns.enemyDefaultEffects = findColumn(headers, {"敵側既定効果", "enemy_default_effects"});
    columns.capturedDefaultEffects = findColumn(headers, {"捕獲後既定効果", "captured_default_effects"});
    columns.relatedTags = findColumn(headers, {"関連特殊タグ", "related_tags"});
    columns.enemyFacingControl = findColumn(headers, {"敵側向き制御", "enemy_facing_control"});
    columns.capturedFacingControl = findColumn(headers, {"捕獲後向き制御", "captured_facing_control"});
    columns.movementControl = findColumn(headers, {"移動制御", "movement_control"});
    columns.duplicateRule = findColumn(headers, {"重複ルール", "duplicate_rule"});
    columns.implementationState = findColumn(headers, {"実装状態", "implementation_state"});
    columns.note = findColumn(headers, {"備考", "note"});

    if (columns.id < 0) {
        outError = "Behavior sheet is missing the behavior ID column";
        return false;
    }
    if (columns.displayName < 0) {
        outError = "Behavior sheet is missing the display name column";
        return false;
    }
    outColumns = columns;
    return true;
}

void validateBehaviorReference(
    const std::vector<std::string>& behaviorIds,
    const std::unordered_map<std::string, BehaviorDefinition>& behaviorsById,
    std::string_view sheet,
    std::size_t rowIndex,
    std::string_view id,
    std::string_view fieldName,
    EnemyCatalog& catalog)
{
    for (const std::string& behaviorId : behaviorIds) {
        if (behaviorsById.find(behaviorId) == behaviorsById.end()) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                    " id=\"" + std::string(id) + "\" " + std::string(fieldName) +
                    ": undefined behavior ID \"" + behaviorId + "\"");
        }
    }
}

void validateTags(
    const std::vector<std::string>& tags,
    const std::unordered_map<std::string, SpecialTagDefinition>& specialTags,
    std::string_view sheet,
    std::size_t rowIndex,
    std::string_view id,
    std::string_view fieldName,
    EnemyCatalog& catalog)
{
    for (const std::string& tag : tags) {
        if (specialTags.find(tag) == specialTags.end()) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::UndefinedSpecialTag,
                std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                    " id=\"" + std::string(id) + "\" " + std::string(fieldName) +
                    ": undefined special tag \"" + tag + "\"");
        }
    }
}

bool parseBehaviors(const GoogleSheetTable& table, EnemyCatalog& catalog, std::string& outError)
{
    if (table.rows.empty()) {
        outError = "Behavior sheet is empty";
        return false;
    }

    BehaviorColumns columns;
    if (!findBehaviorColumns(table.rows.front(), columns, outError)) {
        return false;
    }

    std::unordered_set<std::string> ids;
    for (std::size_t rowIndex = 1; rowIndex < table.rows.size(); ++rowIndex) {
        const GoogleSheetRow& row = table.rows[rowIndex];
        if (!hasRowContent(row)) {
            continue;
        }

        BehaviorDefinition behavior;
        behavior.id = cellAt(row, columns.id);
        behavior.displayName = cellAt(row, columns.displayName);
        behavior.usableField = cellAt(row, columns.usableField);
        behavior.classification = cellAt(row, columns.classification);
        behavior.triggerCondition = cellAt(row, columns.triggerCondition);
        behavior.baseProcess = cellAt(row, columns.baseProcess);
        behavior.defaultIntervalSeconds = parseDoubleColumnOrDefault(
            cellAt(row, columns.defaultIntervalSeconds), 0.0, "Behavior", rowIndex, behavior.id, "既定間隔秒", catalog);
        behavior.defaultProjectileId = cellAt(row, columns.defaultProjectileId);
        behavior.enemyTarget = cellAt(row, columns.enemyTarget);
        behavior.capturedTarget = cellAt(row, columns.capturedTarget);
        parseEffectField(cellAt(row, columns.enemyDefaultEffects), behavior.enemyDefaultEffects, "Behavior", rowIndex, behavior.id, "敵側既定効果", catalog);
        parseEffectField(cellAt(row, columns.capturedDefaultEffects), behavior.capturedDefaultEffects, "Behavior", rowIndex, behavior.id, "捕獲後既定効果", catalog);
        behavior.relatedTags = parseDelimitedValues(cellAt(row, columns.relatedTags));
        behavior.enemyFacingControl = cellAt(row, columns.enemyFacingControl);
        behavior.capturedFacingControl = cellAt(row, columns.capturedFacingControl);
        behavior.movementControl = cellAt(row, columns.movementControl);
        behavior.duplicateRule = cellAt(row, columns.duplicateRule);
        behavior.implementationState = cellAt(row, columns.implementationState);
        behavior.note = cellAt(row, columns.note);

        if (behavior.id.empty()) {
            addIssue(catalog, DbValidationSeverity::Warning, DbValidationCategory::ObjectField,
                "Behavior row " + std::to_string(rowIndex + 1) + ": ID is empty; row skipped");
            continue;
        }
        if (!ids.insert(behavior.id).second) {
            addIssue(catalog, DbValidationSeverity::Warning, DbValidationCategory::DuplicateId,
                "Behavior row " + std::to_string(rowIndex + 1) + " id=\"" + behavior.id + "\": duplicate ID; first map entry is kept");
        }
        if (behavior.displayName.empty()) {
            addIssue(catalog, DbValidationSeverity::Warning, DbValidationCategory::ObjectField,
                "Behavior row " + std::to_string(rowIndex + 1) + " id=\"" + behavior.id + "\": display name is empty");
        }

        catalog.behaviorsById.emplace(behavior.id, behavior);
        catalog.behaviors.push_back(std::move(behavior));
    }
    return true;
}

bool parseEnemies(
    const GoogleSheetTable& table,
    const std::unordered_map<std::string, SpecialTagDefinition>& specialTags,
    EnemyCatalog& catalog,
    std::string& outError)
{
    if (table.rows.empty()) {
        outError = "Enemies sheet is empty";
        return false;
    }

    EnemyColumns columns;
    if (!findEnemyColumns(table.rows.front(), columns, outError)) {
        return false;
    }

    std::unordered_set<std::string> ids;
    for (std::size_t rowIndex = 1; rowIndex < table.rows.size(); ++rowIndex) {
        const GoogleSheetRow& row = table.rows[rowIndex];
        if (!hasRowContent(row)) {
            continue;
        }

        EnemyDefinition enemy;
        enemy.id = cellAt(row, columns.id);
        enemy.name = cellAt(row, columns.name);
        enemy.description = cellAt(row, columns.description);
        enemy.hp = parseIntColumnOrDefault(cellAt(row, columns.hp), 1, "Enemies", rowIndex, enemy.id, "HP", catalog);
        enemy.contactAttackPower = parseIntColumnOrDefault(cellAt(row, columns.contactAttackPower), 0, "Enemies", rowIndex, enemy.id, "接触攻撃力", catalog);
        enemy.contactDamageType = cellAt(row, columns.contactDamageType);
        validateDamageType(enemy.contactDamageType, "Enemies", rowIndex, enemy.id, "接触ダメージ種別", catalog);
        enemy.moveSpeed = parseDoubleColumnOrDefault(cellAt(row, columns.moveSpeed), 0.0, "Enemies", rowIndex, enemy.id, "移動速度", catalog);
        enemy.radius = parseDoubleColumnOrDefault(cellAt(row, columns.radius), 0.0, "Enemies", rowIndex, enemy.id, "半径", catalog);
        enemy.xp = parseIntColumnOrDefault(cellAt(row, columns.xp), 0, "Enemies", rowIndex, enemy.id, "経験値", catalog);
        enemy.money = parseIntColumnOrDefault(cellAt(row, columns.money), 0, "Enemies", rowIndex, enemy.id, "お金", catalog);
        enemy.enemyAi = cellAt(row, columns.enemyAi);
        enemy.enemyBehaviorIds = parseDelimitedValues(cellAt(row, columns.enemyBehaviorId));
        enemy.enemyTags = parseDelimitedValues(cellAt(row, columns.enemyTags));
        enemy.captureDifficulty = parseIntColumnOrDefault(cellAt(row, columns.captureDifficulty), 0, "Enemies", rowIndex, enemy.id, "捕獲難度", catalog);
        enemy.capturedDescription = cellAt(row, columns.capturedDescription);
        enemy.capturedEffectText = cellAt(row, columns.capturedEffectText);
        enemy.capturedAttackPower = parseIntColumnOrDefault(cellAt(row, columns.capturedAttackPower), 0, "Enemies", rowIndex, enemy.id, "捕獲後攻撃力", catalog);
        enemy.capturedDamageType = cellAt(row, columns.capturedDamageType);
        validateDamageType(enemy.capturedDamageType, "Enemies", rowIndex, enemy.id, "捕獲後ダメージ種別", catalog);
        enemy.capturedDigPower = parseIntColumnOrDefault(cellAt(row, columns.capturedDigPower), 0, "Enemies", rowIndex, enemy.id, "捕獲後掘削力", catalog);
        enemy.capturedDurability = parseIntColumnOrDefault(cellAt(row, columns.capturedDurability), 0, "Enemies", rowIndex, enemy.id, "捕獲後耐久力", catalog);
        enemy.capturedWeight = parseDoubleColumnOrDefault(cellAt(row, columns.capturedWeight), 0.0, "Enemies", rowIndex, enemy.id, "捕獲後重さ", catalog);
        parseEffectField(cellAt(row, columns.capturedNormalEffects), enemy.capturedNormalEffects, "Enemies", rowIndex, enemy.id, "捕獲後通常効果", catalog);
        parseEffectField(cellAt(row, columns.capturedOrbitEffects), enemy.capturedOrbitEffects, "Enemies", rowIndex, enemy.id, "捕獲後軌道効果", catalog);
        enemy.capturedTags = parseDelimitedValues(cellAt(row, columns.capturedTags));
        enemy.capturedBehaviorIds = parseDelimitedValues(cellAt(row, columns.capturedBehaviorId));
        enemy.note = cellAt(row, columns.note);

        if (enemy.id.empty()) {
            addIssue(catalog, DbValidationSeverity::Warning, DbValidationCategory::ObjectField,
                "Enemies row " + std::to_string(rowIndex + 1) + ": ID is empty; row skipped");
            continue;
        }
        if (!ids.insert(enemy.id).second) {
            addIssue(catalog, DbValidationSeverity::Warning, DbValidationCategory::DuplicateId,
                "Enemies row " + std::to_string(rowIndex + 1) + " id=\"" + enemy.id + "\": duplicate ID; first map entry is kept");
        }
        if (enemy.name.empty()) {
            addIssue(catalog, DbValidationSeverity::Warning, DbValidationCategory::ObjectField,
                "Enemies row " + std::to_string(rowIndex + 1) + " id=\"" + enemy.id + "\": name is empty");
        }

        validateBehaviorReference(enemy.enemyBehaviorIds, catalog.behaviorsById, "Enemies", rowIndex, enemy.id, "敵挙動ID", catalog);
        validateBehaviorReference(enemy.capturedBehaviorIds, catalog.behaviorsById, "Enemies", rowIndex, enemy.id, "捕獲後挙動ID", catalog);
        validateTags(enemy.enemyTags, specialTags, "Enemies", rowIndex, enemy.id, "敵特殊タグ", catalog);
        validateTags(enemy.capturedTags, specialTags, "Enemies", rowIndex, enemy.id, "捕獲後特殊タグ", catalog);

        catalog.enemiesById.emplace(enemy.id, enemy);
        catalog.enemies.push_back(std::move(enemy));
    }
    return true;
}

}

bool parseEnemyCatalog(
    const GoogleSheetTable& enemiesTable,
    const GoogleSheetTable& behaviorsTable,
    const std::unordered_map<std::string, SpecialTagDefinition>& specialTags,
    EnemyCatalog& outCatalog,
    std::string& outError)
{
    EnemyCatalog catalog;
    if (!parseBehaviors(behaviorsTable, catalog, outError)) {
        outCatalog = {};
        return false;
    }
    for (std::size_t i = 0; i < catalog.behaviors.size(); ++i) {
        validateTags(catalog.behaviors[i].relatedTags, specialTags, "Behavior", i + 1, catalog.behaviors[i].id, "関連特殊タグ", catalog);
    }
    if (!parseEnemies(enemiesTable, specialTags, catalog, outError)) {
        outCatalog = {};
        return false;
    }

    outCatalog = std::move(catalog);
    outError.clear();
    return true;
}

bool loadEnemyCatalogFromGoogleSheet(
    const GoogleSheetSourceConfig& config,
    const std::unordered_map<std::string, SpecialTagDefinition>& specialTags,
    EnemyCatalog& outCatalog,
    std::string& outError)
{
    GoogleSheetTable enemiesTable;
    if (!loadGoogleSheetTableForSheet(config, config.enemiesSheet, enemiesTable, outError)) {
        return false;
    }

    GoogleSheetTable behaviorsTable;
    if (!loadGoogleSheetTableForSheet(config, config.behaviorSheet, behaviorsTable, outError)) {
        return false;
    }

    return parseEnemyCatalog(enemiesTable, behaviorsTable, specialTags, outCatalog, outError);
}

}
