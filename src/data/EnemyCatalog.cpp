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
    return isDamageTypeAllowed(value);
}

bool isLegacyPhysicalDamageType(std::string_view value)
{
    return equalsIgnoreCase(trim(value), "physical");
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

double parseRequiredDoubleColumnOrDefault(
    std::string_view text,
    double defaultValue,
    std::string_view sheet,
    std::size_t rowIndex,
    std::string_view id,
    std::string_view columnName,
    EnemyCatalog& catalog)
{
    const std::string normalized = trim(text);
    if (normalized.empty()) {
        addIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                " id=\"" + std::string(id) + "\" " + std::string(columnName) +
                ": empty; using " + std::to_string(defaultValue));
        return defaultValue;
    }

    double value = defaultValue;
    if (!parseDoubleStrict(normalized, value)) {
        addIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                " id=\"" + std::string(id) + "\" " + std::string(columnName) +
                ": invalid number \"" + normalized + "\"; using " + std::to_string(defaultValue));
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
    if (trim(value).empty()) {
        value = "none";
        return;
    }
    const std::string rawValue = value;
    value = normalizeDamageType(value);
    if (value.empty()) {
        addIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::ObjectField,
            std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                " id=\"" + std::string(id) + "\" " + std::string(columnName) +
                ": invalid damage type \"" + rawValue + "\"; using none");
        value = "none";
    } else if (isLegacyPhysicalDamageType(rawValue)) {
        addIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::DeprecatedEntry,
            std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                " id=\"" + std::string(id) + "\" " + std::string(columnName) +
                ": damage type physical is deprecated; using blunt");
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

constexpr std::array<std::string_view, 6> DefinedBehaviorTriggers = {
    "always",
    "attack",
    "timer",
    "contact",
    "detect",
    "hit",
};

constexpr std::array<std::string_view, 12> KnownProjectileIds = {
    "stone_bullet",
    "big_stone_bullet",
    "weapon_throw",
    "poison_spit",
    "paralyze_shot",
    "mud_blob",
    "cactus_needle",
    "water_shot",
    "fire_breath",
    "web_thread",
    "wind_wave",
    "explosion_small",
};

constexpr std::array<std::string_view, 5> KnownDropProfiles = {
    "money",
    "treasure",
    "box_common",
    "box_rare",
    "box_super",
};

enum class BehaviorUsageField {
    Enemy,
    Captured,
};

std::string toLowerAscii(std::string_view text)
{
    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::vector<std::string> splitBehaviorEntries(std::string_view text)
{
    std::vector<std::string> entries;
    std::string current;
    for (char ch : text) {
        if (ch == ';') {
            const std::string entry = trim(current);
            if (!entry.empty()) {
                entries.push_back(entry);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    const std::string last = trim(current);
    if (!last.empty()) {
        entries.push_back(last);
    }
    return entries;
}

std::vector<std::string> splitBehaviorParamTokens(std::string_view text)
{
    std::vector<std::string> tokens;
    std::string current;
    const auto flush = [&]() {
        const std::string value = trim(current);
        if (!value.empty()) {
            tokens.push_back(value);
        }
        current.clear();
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == ',') {
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
    return tokens;
}

std::vector<std::string> splitPipeValues(std::string_view text)
{
    std::vector<std::string> values;
    std::string current;
    for (char ch : text) {
        if (ch == '|') {
            const std::string value = trim(current);
            if (!value.empty()) {
                values.push_back(value);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    const std::string last = trim(current);
    if (!last.empty()) {
        values.push_back(last);
    }
    return values;
}

bool isKnownProjectileId(std::string_view projectileId)
{
    return std::any_of(KnownProjectileIds.begin(), KnownProjectileIds.end(), [projectileId](std::string_view value) {
        return value == projectileId;
    });
}

bool isKnownDropProfile(std::string_view profile)
{
    return std::any_of(KnownDropProfiles.begin(), KnownDropProfiles.end(), [profile](std::string_view value) {
        return value == profile;
    });
}

bool isDefinedBehaviorTrigger(std::string_view trigger)
{
    return std::any_of(DefinedBehaviorTriggers.begin(), DefinedBehaviorTriggers.end(), [trigger](std::string_view value) {
        return value == trigger;
    });
}

std::string mapLegacyBehaviorIdToCode(std::string_view legacyId)
{
    const std::string id = trim(legacyId);
    if (id.empty() || equalsIgnoreCase(id, "none")) {
        return "none";
    }
    if (id == "front_guard") {
        return "always:front_guard:none:0";
    }
    if (id == "shoot_fire") {
        return "attack:shoot_fire:none:2.0";
    }
    if (id == "shoot_water") {
        return "attack:shoot_water:none:2.0";
    }
    if (id == "shoot_poison") {
        return "attack:shoot_poison:none:2.3";
    }
    if (id == "shoot_web") {
        return "attack:shoot_web:none:2.3";
    }
    if (id == "throw_stone") {
        return "attack:throw_object:projectile=stone_bullet:2.0";
    }
    if (id == "radial_spike") {
        return "attack:radial_spike:none:2.4";
    }
    if (id == "wind_blow") {
        return "attack:wind_blow:none:2.5";
    }
    if (id == "countdown_explode") {
        return "timer:countdown_explode:none:0";
    }
    if (id == "dig_move") {
        return "always:dig_move:none:0";
    }
    if (id == "ring_slow_bite") {
        return "contact:ring_slow_bite:none:1.0";
    }
    if (id == "swarm_alert") {
        return "detect:swarm_alert:none:0";
    }
    if (id == "dig_contact") {
        return "hit:dig_contact:none:0.2";
    }
    return {};
}

std::string convertLegacyBehaviorFieldToCode(
    std::string_view legacyField,
    std::string_view sheet,
    std::size_t rowIndex,
    std::string_view enemyId,
    std::string_view fieldName,
    EnemyCatalog& catalog)
{
    const std::vector<std::string> legacyIds = parseDelimitedValues(legacyField);
    if (legacyIds.empty()) {
        return "none";
    }

    std::vector<std::string> converted;
    for (const std::string& legacyId : legacyIds) {
        const std::string code = mapLegacyBehaviorIdToCode(legacyId);
        if (code.empty()) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                    " id=\"" + std::string(enemyId) + "\" " + std::string(fieldName) +
                    ": unknown legacy behavior ID \"" + legacyId + "\"; ignored");
            continue;
        }
        if (!equalsIgnoreCase(code, "none")) {
            converted.push_back(code);
        }
    }

    if (converted.empty()) {
        return "none";
    }
    std::string joined;
    for (std::size_t i = 0; i < converted.size(); ++i) {
        if (i > 0) {
            joined += ';';
        }
        joined += converted[i];
    }
    return joined;
}

bool parseBehaviorParams(
    std::string_view text,
    std::unordered_map<std::string, std::string>& outParams,
    std::string& outError)
{
    outParams.clear();
    if (isNone(text)) {
        outError.clear();
        return true;
    }

    for (const std::string& token : splitBehaviorParamTokens(text)) {
        const std::size_t equals = token.find('=');
        if (equals == std::string::npos || equals == 0 || equals + 1 >= token.size()) {
            outError = "invalid param \"" + token + "\" (expected key=value)";
            outParams.clear();
            return false;
        }

        const std::string key = trim(std::string_view(token).substr(0, equals));
        const std::string value = trim(std::string_view(token).substr(equals + 1));
        if (key.empty() || value.empty()) {
            outError = "invalid param \"" + token + "\" (empty key/value)";
            outParams.clear();
            return false;
        }
        if (outParams.find(key) != outParams.end()) {
            outError = "duplicate param key \"" + key + "\"";
            outParams.clear();
            return false;
        }
        outParams.emplace(key, value);
    }

    outError.clear();
    return true;
}

bool parseEnemyBehaviorSpecEntry(std::string_view entry, EnemyBehaviorSpec& outSpec, std::string& outError)
{
    const std::size_t p1 = entry.find(':');
    const std::size_t p2 = p1 == std::string_view::npos ? std::string_view::npos : entry.find(':', p1 + 1);
    const std::size_t p3 = p2 == std::string_view::npos ? std::string_view::npos : entry.find(':', p2 + 1);
    if (p1 == std::string_view::npos || p2 == std::string_view::npos || p3 == std::string_view::npos) {
        outError = "expected trigger:behavior:params:interval";
        return false;
    }

    EnemyBehaviorSpec spec;
    spec.trigger = toLowerAscii(trim(entry.substr(0, p1)));
    spec.behavior = trim(entry.substr(p1 + 1, p2 - p1 - 1));
    const std::string paramsText = trim(entry.substr(p2 + 1, p3 - p2 - 1));
    const std::string intervalText = trim(entry.substr(p3 + 1));

    if (spec.trigger.empty()) {
        outError = "trigger is empty";
        return false;
    }
    if (spec.behavior.empty()) {
        outError = "behavior is empty";
        return false;
    }
    if (!parseDoubleStrict(intervalText, spec.intervalSeconds)) {
        outError = "interval is not numeric";
        return false;
    }
    if (!parseBehaviorParams(paramsText, spec.params, outError)) {
        return false;
    }

    outSpec = std::move(spec);
    outError.clear();
    return true;
}

bool behaviorUsableFieldMatches(std::string_view usableField, BehaviorUsageField usage)
{
    const std::string trimmed = trim(usableField);
    if (trimmed.empty()) {
        return true;
    }

    const std::string lower = toLowerAscii(trimmed);
    const bool mentionsEnemy = trimmed.find("敵") != std::string::npos ||
        lower.find("enemy") != std::string::npos;
    const bool mentionsCaptured = trimmed.find("捕獲") != std::string::npos ||
        lower.find("captured") != std::string::npos;
    const bool mentionsBoth = trimmed.find("両方") != std::string::npos ||
        trimmed.find("共通") != std::string::npos ||
        lower.find("both") != std::string::npos ||
        lower.find("all") != std::string::npos;

    if (mentionsBoth) {
        return true;
    }
    if (usage == BehaviorUsageField::Enemy) {
        return !mentionsCaptured || mentionsEnemy;
    }
    return !mentionsEnemy || mentionsCaptured;
}

bool behaviorSupportedTriggerMatches(const BehaviorDefinition& behavior, std::string_view trigger)
{
    if (behavior.supportedTriggers.empty()) {
        return true;
    }
    return std::any_of(behavior.supportedTriggers.begin(), behavior.supportedTriggers.end(), [trigger](const std::string& supported) {
        return toLowerAscii(supported) == trigger;
    });
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

bool validateBehaviorParamsForEntry(
    const EnemyBehaviorSpec& spec,
    const std::unordered_map<std::string, SpecialTagDefinition>& specialTags,
    std::string& outError)
{
    for (const auto& [key, value] : spec.params) {
        if (key == "projectile") {
            for (const std::string& projectileId : splitPipeValues(value)) {
                if (!isKnownProjectileId(projectileId)) {
                    outError = "unknown projectile \"" + projectileId + "\"";
                    return false;
                }
            }
        } else if (key == "targetTag") {
            for (const std::string& targetTag : splitPipeValues(value)) {
                if (specialTags.find(targetTag) == specialTags.end()) {
                    outError = "unknown targetTag \"" + targetTag + "\"";
                    return false;
                }
            }
        } else if (key == "profile") {
            for (const std::string& profile : splitPipeValues(value)) {
                if (!isKnownDropProfile(profile)) {
                    outError = "unknown drop profile \"" + profile + "\"";
                    return false;
                }
            }
        }
    }
    outError.clear();
    return true;
}

std::vector<std::string> extractBehaviorIds(const std::vector<EnemyBehaviorSpec>& specs, bool normalizeThrowObjectToStone)
{
    std::vector<std::string> ids;
    std::unordered_set<std::string> seen;
    for (const EnemyBehaviorSpec& spec : specs) {
        const std::string behaviorId = normalizeThrowObjectToStone && spec.behavior == "throw_object"
            ? "throw_stone"
            : spec.behavior;
        if (!seen.insert(behaviorId).second) {
            continue;
        }
        ids.push_back(behaviorId);
    }
    return ids;
}

void parseBehaviorCodeField(
    std::string_view behaviorCodeText,
    BehaviorUsageField usage,
    std::string_view sheet,
    std::size_t rowIndex,
    std::string_view enemyId,
    std::string_view fieldName,
    const std::unordered_map<std::string, BehaviorDefinition>& behaviorsById,
    const std::unordered_map<std::string, SpecialTagDefinition>& specialTags,
    EnemyCatalog& catalog,
    std::vector<EnemyBehaviorSpec>& outSpecs)
{
    outSpecs.clear();
    if (isNone(behaviorCodeText)) {
        return;
    }

    for (const std::string& entry : splitBehaviorEntries(behaviorCodeText)) {
        if (isNone(entry)) {
            continue;
        }

        EnemyBehaviorSpec spec;
        std::string parseError;
        if (!parseEnemyBehaviorSpecEntry(entry, spec, parseError)) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                    " id=\"" + std::string(enemyId) + "\" " + std::string(fieldName) +
                    ": invalid behavior entry \"" + entry + "\" (" + parseError + "); entry disabled");
            continue;
        }

        if (!isDefinedBehaviorTrigger(spec.trigger)) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                    " id=\"" + std::string(enemyId) + "\" " + std::string(fieldName) +
                    ": undefined trigger \"" + spec.trigger + "\"; entry disabled");
            continue;
        }

        const auto behaviorIt = behaviorsById.find(spec.behavior);
        if (behaviorIt == behaviorsById.end()) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                    " id=\"" + std::string(enemyId) + "\" " + std::string(fieldName) +
                    ": undefined behavior \"" + spec.behavior + "\"; entry disabled");
            continue;
        }

        const BehaviorDefinition& behavior = behaviorIt->second;
        if (!behaviorUsableFieldMatches(behavior.usableField, usage)) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                    " id=\"" + std::string(enemyId) + "\" " + std::string(fieldName) +
                    ": behavior \"" + spec.behavior + "\" is not usable in this column; entry disabled");
            continue;
        }

        if (!behaviorSupportedTriggerMatches(behavior, spec.trigger)) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                    " id=\"" + std::string(enemyId) + "\" " + std::string(fieldName) +
                    ": trigger \"" + spec.trigger + "\" is not allowed for behavior \"" + spec.behavior + "\"; entry disabled");
            continue;
        }

        std::string paramError;
        if (!validateBehaviorParamsForEntry(spec, specialTags, paramError)) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                std::string(sheet) + " row " + std::to_string(rowIndex + 1) +
                    " id=\"" + std::string(enemyId) + "\" " + std::string(fieldName) +
                    ": " + paramError + "; entry disabled");
            continue;
        }

        outSpecs.push_back(std::move(spec));
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
    int unawareAi = -1;
    int visionDistance = -1;
    int visionAngle = -1;
    int loseSightSeconds = -1;
    int enemyBehaviorCode = -1;
    int enemyBehaviorIdLegacy = -1;
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
    int capturedBehaviorCode = -1;
    int capturedBehaviorIdLegacy = -1;
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
    columns.unawareAi = findColumn(headers, {"未発見AI", "unaware_ai"});
    columns.visionDistance = findColumn(headers, {"視野距離", "vision_distance"});
    columns.visionAngle = findColumn(headers, {"視野角", "vision_angle"});
    columns.loseSightSeconds = findColumn(headers, {"見失い秒数", "lose_sight_seconds"});
    columns.enemyBehaviorCode = findColumn(headers, {"敵挙動コード", "enemy_behavior_code"});
    columns.enemyBehaviorIdLegacy = findColumn(headers, {"敵挙動ID", "enemy_behavior_id"});
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
    columns.capturedBehaviorCode = findColumn(headers, {"捕獲後挙動コード", "captured_behavior_code"});
    columns.capturedBehaviorIdLegacy = findColumn(headers, {"捕獲後挙動ID", "captured_behavior_id"});
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
    int classification = -1;
    int usableField = -1;
    int supportedTriggers = -1;
    int primaryParams = -1;
    int defaultParams = -1;
    int defaultProjectileId = -1;
    int baseProcess = -1;
    int relatedTags = -1;
    int facingControl = -1;
    int movementControl = -1;
    int duplicateRule = -1;
    int implementationState = -1;
    int note = -1;

    // Legacy columns.
    int triggerCondition = -1;
    int defaultIntervalSeconds = -1;
    int enemyTarget = -1;
    int capturedTarget = -1;
    int enemyDefaultEffects = -1;
    int capturedDefaultEffects = -1;
    int enemyFacingControl = -1;
    int capturedFacingControl = -1;
};

bool findBehaviorColumns(const GoogleSheetRow& headers, BehaviorColumns& outColumns, std::string& outError)
{
    BehaviorColumns columns;
    columns.id = findColumn(headers, {"敵挙動コード", "挙動ID", "behavior_id", "enemy_behavior_code"});
    columns.displayName = findColumn(headers, {"表示名", "display_name"});
    columns.classification = findColumn(headers, {"分類", "classification"});
    columns.usableField = findColumn(headers, {"使用欄", "usable_field"});
    columns.supportedTriggers = findColumn(headers, {"使用可能trigger", "supported_triggers", "trigger_condition"});
    columns.primaryParams = findColumn(headers, {"主なparams", "primary_params"});
    columns.defaultParams = findColumn(headers, {"デフォルトparams", "default_params"});
    columns.defaultProjectileId = findColumn(headers, {"既定発射物ID", "default_projectile_id"});
    columns.baseProcess = findColumn(headers, {"基本処理", "base_process"});
    columns.relatedTags = findColumn(headers, {"関連特殊タグ", "related_tags"});
    columns.facingControl = findColumn(headers, {"向き制御", "facing_control"});
    columns.movementControl = findColumn(headers, {"移動制御", "movement_control"});
    columns.duplicateRule = findColumn(headers, {"重複ルール", "duplicate_rule"});
    columns.implementationState = findColumn(headers, {"実装状態", "implementation_state"});
    columns.note = findColumn(headers, {"備考", "note"});

    columns.triggerCondition = findColumn(headers, {"起動条件", "trigger_condition"});
    columns.defaultIntervalSeconds = findColumn(headers, {"既定間隔秒", "default_interval_seconds"});
    columns.enemyTarget = findColumn(headers, {"敵側対象", "enemy_target"});
    columns.capturedTarget = findColumn(headers, {"捕獲後対象", "captured_target"});
    columns.enemyDefaultEffects = findColumn(headers, {"敵側既定効果", "enemy_default_effects"});
    columns.capturedDefaultEffects = findColumn(headers, {"捕獲後既定効果", "captured_default_effects"});
    columns.enemyFacingControl = findColumn(headers, {"敵側向き制御", "enemy_facing_control"});
    columns.capturedFacingControl = findColumn(headers, {"捕獲後向き制御", "captured_facing_control"});

    if (columns.id < 0) {
        outError = "Behavior sheet is missing the behavior code column";
        return false;
    }
    if (columns.displayName < 0) {
        outError = "Behavior sheet is missing the display name column";
        return false;
    }
    outColumns = columns;
    return true;
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
        behavior.classification = cellAt(row, columns.classification);
        behavior.usableField = cellAt(row, columns.usableField);
        behavior.supportedTriggers = parseDelimitedValues(cellAt(row, columns.supportedTriggers));
        behavior.primaryParams = cellAt(row, columns.primaryParams);
        behavior.defaultParams = cellAt(row, columns.defaultParams);
        behavior.defaultProjectileId = cellAt(row, columns.defaultProjectileId);
        behavior.baseProcess = cellAt(row, columns.baseProcess);
        behavior.relatedTags = parseDelimitedValues(cellAt(row, columns.relatedTags));
        behavior.facingControl = cellAt(row, columns.facingControl);
        behavior.movementControl = cellAt(row, columns.movementControl);
        behavior.duplicateRule = cellAt(row, columns.duplicateRule);
        behavior.implementationState = cellAt(row, columns.implementationState);
        behavior.note = cellAt(row, columns.note);

        if (behavior.facingControl.empty()) {
            const std::string enemyFacing = cellAt(row, columns.enemyFacingControl);
            const std::string capturedFacing = cellAt(row, columns.capturedFacingControl);
            if (!enemyFacing.empty() || !capturedFacing.empty()) {
                behavior.facingControl = enemyFacing;
                if (!capturedFacing.empty()) {
                    if (!behavior.facingControl.empty()) {
                        behavior.facingControl += " / ";
                    }
                    behavior.facingControl += capturedFacing;
                }
            }
        }

        behavior.defaultIntervalSeconds = parseDoubleColumnOrDefault(
            cellAt(row, columns.defaultIntervalSeconds), 0.0, "Behavior", rowIndex, behavior.id, "既定間隔秒", catalog);
        behavior.enemyTarget = cellAt(row, columns.enemyTarget);
        behavior.capturedTarget = cellAt(row, columns.capturedTarget);
        parseEffectField(cellAt(row, columns.enemyDefaultEffects), behavior.enemyDefaultEffects, "Behavior", rowIndex, behavior.id, "敵側既定効果", catalog);
        parseEffectField(cellAt(row, columns.capturedDefaultEffects), behavior.capturedDefaultEffects, "Behavior", rowIndex, behavior.id, "捕獲後既定効果", catalog);
        behavior.enemyFacingControl = cellAt(row, columns.enemyFacingControl);
        behavior.capturedFacingControl = cellAt(row, columns.capturedFacingControl);

        if (behavior.supportedTriggers.empty() && columns.triggerCondition >= 0) {
            behavior.supportedTriggers = parseDelimitedValues(cellAt(row, columns.triggerCondition));
        }

        if (behavior.id.empty()) {
            addIssue(catalog, DbValidationSeverity::Warning, DbValidationCategory::ObjectField,
                "Behavior row " + std::to_string(rowIndex + 1) + ": code is empty; row skipped");
            continue;
        }
        if (!ids.insert(behavior.id).second) {
            addIssue(catalog, DbValidationSeverity::Warning, DbValidationCategory::DuplicateId,
                "Behavior row " + std::to_string(rowIndex + 1) + " id=\"" + behavior.id + "\": duplicate code; first map entry is kept");
        }
        if (behavior.displayName.empty()) {
            addIssue(catalog, DbValidationSeverity::Warning, DbValidationCategory::ObjectField,
                "Behavior row " + std::to_string(rowIndex + 1) + " id=\"" + behavior.id + "\": display name is empty");
        }
        for (const std::string& trigger : behavior.supportedTriggers) {
            const std::string normalized = toLowerAscii(trigger);
            if (!isDefinedBehaviorTrigger(normalized)) {
                addIssue(
                    catalog,
                    DbValidationSeverity::Warning,
                    DbValidationCategory::ObjectField,
                    "Behavior row " + std::to_string(rowIndex + 1) + " id=\"" + behavior.id +
                        "\": unknown supported trigger \"" + trigger + "\"");
            }
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
        enemy.unawareAiId = "idle";
        if (columns.unawareAi >= 0) {
            const std::string unawareAi = cellAt(row, columns.unawareAi);
            if (unawareAi.empty()) {
                addIssue(
                    catalog,
                    DbValidationSeverity::Warning,
                    DbValidationCategory::ObjectField,
                    "Enemies row " + std::to_string(rowIndex + 1) + " id=\"" + enemy.id +
                        "\" 未発見AI: empty; using idle");
            } else {
                enemy.unawareAiId = unawareAi;
            }
        }
        enemy.visionDistance = columns.visionDistance >= 0
            ? parseRequiredDoubleColumnOrDefault(
                cellAt(row, columns.visionDistance), 120.0, "Enemies", rowIndex, enemy.id, "視野距離", catalog)
            : 120.0;
        enemy.visionAngle = columns.visionAngle >= 0
            ? parseRequiredDoubleColumnOrDefault(
                cellAt(row, columns.visionAngle), 100.0, "Enemies", rowIndex, enemy.id, "視野角", catalog)
            : 100.0;
        enemy.loseSightSeconds = columns.loseSightSeconds >= 0
            ? parseRequiredDoubleColumnOrDefault(
                cellAt(row, columns.loseSightSeconds), 1.5, "Enemies", rowIndex, enemy.id, "見失い秒数", catalog)
            : 1.5;
        if (!(enemy.visionDistance > 0.0)) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::NumericValue,
                "Enemies row " + std::to_string(rowIndex + 1) + " id=\"" + enemy.id +
                    "\" 視野距離: must be > 0; using 120");
            enemy.visionDistance = 120.0;
        }
        if (!(enemy.visionAngle > 0.0)) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::NumericValue,
                "Enemies row " + std::to_string(rowIndex + 1) + " id=\"" + enemy.id +
                    "\" 視野角: must be > 0; using 100");
            enemy.visionAngle = 100.0;
        }
        if (enemy.loseSightSeconds < 0.0) {
            addIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::NumericValue,
                "Enemies row " + std::to_string(rowIndex + 1) + " id=\"" + enemy.id +
                    "\" 見失い秒数: must be >= 0; using 1.5");
            enemy.loseSightSeconds = 1.5;
        }

        const std::string enemyBehaviorCodeValue = cellAt(row, columns.enemyBehaviorCode);
        const std::string enemyBehaviorLegacy = cellAt(row, columns.enemyBehaviorIdLegacy);
        enemy.enemyBehaviorCode = !enemyBehaviorCodeValue.empty()
            ? enemyBehaviorCodeValue
            : convertLegacyBehaviorFieldToCode(enemyBehaviorLegacy, "Enemies", rowIndex, enemy.id, "敵挙動ID", catalog);
        parseBehaviorCodeField(
            enemy.enemyBehaviorCode,
            BehaviorUsageField::Enemy,
            "Enemies",
            rowIndex,
            enemy.id,
            "敵挙動コード",
            catalog.behaviorsById,
            specialTags,
            catalog,
            enemy.enemyBehaviorSpecs);
        enemy.enemyBehaviorIds = extractBehaviorIds(enemy.enemyBehaviorSpecs, false);

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

        const std::string capturedBehaviorCodeValue = cellAt(row, columns.capturedBehaviorCode);
        const std::string capturedBehaviorLegacy = cellAt(row, columns.capturedBehaviorIdLegacy);
        enemy.capturedBehaviorCode = !capturedBehaviorCodeValue.empty()
            ? capturedBehaviorCodeValue
            : convertLegacyBehaviorFieldToCode(capturedBehaviorLegacy, "Enemies", rowIndex, enemy.id, "捕獲後挙動ID", catalog);
        parseBehaviorCodeField(
            enemy.capturedBehaviorCode,
            BehaviorUsageField::Captured,
            "Enemies",
            rowIndex,
            enemy.id,
            "捕獲後挙動コード",
            catalog.behaviorsById,
            specialTags,
            catalog,
            enemy.capturedBehaviorSpecs);
        enemy.capturedBehaviorIds = extractBehaviorIds(enemy.capturedBehaviorSpecs, true);

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

    std::vector<std::string> behaviorSheetCandidates;
    if (config.behaviorSheetExplicit && !trim(config.behaviorSheet).empty()) {
        behaviorSheetCandidates.push_back(config.behaviorSheet);
    } else {
        behaviorSheetCandidates = {"#敵挙動コード", "#挙動ID", "挙動ID一覧"};
    }

    GoogleSheetTable behaviorsTable;
    std::string lastError;
    bool loadedBehaviorSheet = false;
    std::string loadedBehaviorSheetName;
    for (const std::string& sheetName : behaviorSheetCandidates) {
        std::string behaviorLoadError;
        if (!loadGoogleSheetTableForSheet(config, sheetName, behaviorsTable, behaviorLoadError)) {
            lastError = "failed to load sheet " + sheetName + ": " + behaviorLoadError;
            continue;
        }
        loadedBehaviorSheet = true;
        loadedBehaviorSheetName = sheetName;
        break;
    }
    if (!loadedBehaviorSheet) {
        outError = lastError.empty() ? "failed to load behavior sheet" : lastError;
        return false;
    }

    if (!parseEnemyCatalog(enemiesTable, behaviorsTable, specialTags, outCatalog, outError)) {
        return false;
    }

    if (!loadedBehaviorSheetName.empty() &&
        !config.behaviorSheetExplicit &&
        loadedBehaviorSheetName != "#敵挙動コード") {
        outCatalog.validationWarnings.push_back(
            "Behavior sheet fallback used: " + loadedBehaviorSheetName +
            " (preferred: #敵挙動コード)");
    }

    return true;
}

}
