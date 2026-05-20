#include "data/ObjectCatalog.hpp"

#include "data/StageWeight.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace majo {

namespace {

constexpr std::string_view HeaderName = "\xE5\x90\x8D\xE5\x89\x8D";
constexpr std::string_view HeaderCategory = "\xE3\x82\xAB\xE3\x83\x86\xE3\x82\xB4\xE3\x83\xAA";
constexpr std::string_view HeaderDescription = "\xE8\xAA\xAC\xE6\x98\x8E\xE6\x96\x87";
constexpr std::string_view HeaderLegacyDescription = "\xE8\xAA\xAC\xE6\x98\x8E";
constexpr std::string_view HeaderRarity = "\xE3\x83\xAC\xE3\x82\xA2\xE5\xBA\xA6";
constexpr std::string_view HeaderPrice = "\xE4\xBE\xA1\xE6\xA0\xBC";
constexpr std::string_view HeaderLegacyPrice = "\xE5\x80\xA4\xE6\xAE\xB5";
constexpr std::string_view HeaderUseEffects = "\xE9\x80\x9A\xE5\xB8\xB8\xE5\x8A\xB9\xE6\x9E\x9C";
constexpr std::string_view HeaderOrbitEffects = "\xE8\xBB\x8C\xE9\x81\x93\xE5\x8A\xB9\xE6\x9E\x9C";
constexpr std::string_view HeaderAttackPower = "\xE6\x94\xBB\xE6\x92\x83\xE5\x8A\x9B";
constexpr std::string_view HeaderDamageType = "\xE3\x83\x80\xE3\x83\xA1\xE3\x83\xBC\xE3\x82\xB8\xE7\xA8\xAE\xE5\x88\xA5";
constexpr std::string_view HeaderDigPower = "\xE6\x8E\x98\xE5\x89\x8A\xE5\x8A\x9B";
constexpr std::string_view HeaderDurability = "\xE8\x80\x90\xE4\xB9\x85\xE5\x8A\x9B";
constexpr std::string_view HeaderWeight = "\xE9\x87\x8D\xE3\x81\x95";
constexpr std::string_view HeaderImageNumber = "\xE7\x94\xBB\xE5\x83\x8F\xE7\x95\xAA\xE5\x8F\xB7";
constexpr std::string_view HeaderGroundRotation = "\xE5\x9C\xB0\xE9\x9D\xA2\xE8\xA7\x92\xE5\xBA\xA6";
constexpr std::string_view HeaderRingRotation = "\xE3\x83\xAA\xE3\x83\xB3\xE3\x82\xB0\xE8\xA7\x92\xE5\xBA\xA6";
constexpr std::string_view HeaderTags = "\xE7\x89\xB9\xE6\xAE\x8A\xE3\x82\xBF\xE3\x82\xB0";
constexpr std::string_view HeaderEffectText = "\xE5\x8A\xB9\xE6\x9E\x9C\xE3\x83\x86\xE3\x82\xAD\xE3\x82\xB9\xE3\x83\x88";
constexpr std::string_view EffectCodesSheetName = "\xE5\x8A\xB9\xE6\x9E\x9C\xE3\x82\xB3\xE3\x83\xBC\xE3\x83\x89\xE4\xB8\x80\xE8\xA6\xA7";
constexpr std::string_view SpecialTagsSheetName = "\xE7\x89\xB9\xE6\xAE\x8A\xE3\x82\xBF\xE3\x82\xB0\xE4\xB8\x80\xE8\xA6\xA7";
constexpr std::string_view LegacyEffectCodesSheetName = "\x23\xE5\x8A\xB9\xE6\x9E\x9C\xE3\x82\xB3\xE3\x83\xBC\xE3\x83\x89";
constexpr std::string_view LegacySpecialTagsSheetName = "\x23\xE7\x89\xB9\xE6\xAE\x8A\xE3\x82\xBF\xE3\x82\xB0";
constexpr std::string_view HeaderEffectCode = "\xE5\x8A\xB9\xE6\x9E\x9C\xE3\x82\xB3\xE3\x83\xBC\xE3\x83\x89";
constexpr std::string_view HeaderDisplayName = "\xE8\xA1\xA8\xE7\xA4\xBA\xE5\x90\x8D";
constexpr std::string_view HeaderClassification = "\xE5\x88\x86\xE9\xA1\x9E";
constexpr std::string_view HeaderUsableField = "\xE4\xBD\xBF\xE7\x94\xA8\xE5\x8F\xAF\xE8\x83\xBD\xE6\xAC\x84";
constexpr std::string_view HeaderSupportedTargets = "\xE5\xAF\xBE\xE5\xBF\x9C\x74\x61\x72\x67\x65\x74";
constexpr std::string_view HeaderValueMeaning = "\x76\x61\x6C\x75\x65\xE6\x84\x8F\xE5\x91\xB3";
constexpr std::string_view HeaderDurationMeaning = "\x64\x75\x72\x61\x74\x69\x6F\x6E\xE6\x84\x8F\xE5\x91\xB3";
constexpr std::string_view HeaderStackingRule = "\xE9\x87\x8D\xE8\xA4\x87\xE3\x83\xAB\xE3\x83\xBC\xE3\x83\xAB";
constexpr std::string_view HeaderImplementationState = "\xE5\xAE\x9F\xE8\xA3\x85\xE7\x8A\xB6\xE6\x85\x8B";
constexpr std::string_view HeaderNote = "\xE5\x82\x99\xE8\x80\x83";
constexpr std::string_view HeaderSpecialTag = "\xE7\x89\xB9\xE6\xAE\x8A\xE3\x82\xBF\xE3\x82\xB0";
constexpr std::string_view HeaderUsage = "\xE7\x94\xA8\xE9\x80\x94";
constexpr std::string_view HeaderTargetCategories = "\xE5\xAF\xBE\xE8\xB1\xA1\xE3\x82\xAB\xE3\x83\x86\xE3\x82\xB4\xE3\x83\xAA";
constexpr std::string_view HeaderExclusiveGroup = "\xE6\x8E\x92\xE4\xBB\x96\xE3\x82\xB0\xE3\x83\xAB\xE3\x83\xBC\xE3\x83\x97";
constexpr std::string_view HeaderRelatedEffectCodes = "\xE9\x96\xA2\xE9\x80\xA3\xE5\x8A\xB9\xE6\x9E\x9C\xE3\x82\xB3\xE3\x83\xBC\xE3\x83\x89";

constexpr std::array<std::string_view, 10> AllowedCategories = {
    "\xE6\x8E\x98\xE5\x89\x8A",
    "\xE6\x8E\xA2\xE7\xB4\xA2",
    "\xE8\xBB\x8C\xE9\x81\x93",
    "\xE5\xAE\x9D",
    "\xE5\x9B\x9E\xE5\xBE\xA9",
    "\xE5\xBC\xB7\xE5\x8C\x96",
    "\xE5\xBC\xB1\xE4\xBD\x93",
    "\xE9\xAD\x94\xE5\xB0\x8E\xE6\x9B\xB8",
    "\xE6\xAD\xA6\xE5\x99\xA8",
    "\xE7\x9B\xBE",
};

constexpr std::array<std::string_view, 11> AllowedDamageTypes = {
    "none",
    "slash",
    "blunt",
    "pierce",
    "fire",
    "ice",
    "thunder",
    "wind",
    "earth",
    "water",
    "magic",
};

constexpr std::array<std::string_view, 3> LootChestPrefixes = {
    "C",
    "R",
    "S",
};

struct DeprecatedName {
    std::string_view name;
    std::string_view replacement;
    std::string_view note;
};

constexpr std::array<DeprecatedName, 8> DeprecatedEffectCodes = {{
    {"hard_dig", "hard_dig_tool", "old hard dig code/tag name"},
    {"detect_hidden", "hidden_detector", "use the detector tag name in DB metadata; do not make tags fire effects directly"},
    {"detect_treasure", "treasure_detector", "use the detector tag name in DB metadata; do not make tags fire effects directly"},
    {"heal_enemy", "heal with target=enemy", "target should express who receives the effect"},
    {"buff_enemy", "buff_* with target=enemy", "target should express who receives the effect"},
    {"stun_chance", "status_stun_chance", "state effects use the status_* naming family"},
    {"poison_chance", "status_poison_chance", "state effects use the status_* naming family"},
    {"slow_chance", "status_slow_chance", "state effects use the status_* naming family"},
}};

constexpr std::array<DeprecatedName, 8> DeprecatedSpecialTags = {{
    {"hard_dig", "hard_dig_tool", "old hard dig code/tag name"},
    {"detect_hidden", "hidden_detector", "detector labels are special tags"},
    {"detect_treasure", "treasure_detector", "detector labels are special tags"},
    {"orbit_anchor", "orbit effect code orbit_anchor", "orbit_anchor is an EffectSpec effect code, not a special tag"},
    {"orbit_shift", "orbit effect code orbit_shift", "orbit_shift is an EffectSpec effect code, not a special tag"},
    {"heal_enemy", "heal with target=enemy", "target should express who receives the effect"},
    {"buff_enemy", "buff_* with target=enemy", "target should express who receives the effect"},
    {"stun_chance", "status_stun_chance", "state effects use the status_* naming family"},
}};

void addValidationIssue(
    ObjectCatalog& catalog,
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

std::string_view lootChestPrefix(LootChestKind chestKind)
{
    switch (chestKind) {
    case LootChestKind::Common:
        return "C";
    case LootChestKind::Rare:
        return "R";
    case LootChestKind::SuperRare:
        return "S";
    }
    return {};
}

bool parseLootChestKind(std::string_view text, LootChestKind& outKind)
{
    const std::string normalized = trim(text);
    if (equalsIgnoreCase(normalized, "C") ||
        equalsIgnoreCase(normalized, "common") ||
        equalsIgnoreCase(normalized, "normal") ||
        equalsIgnoreCase(normalized, "common_chest") ||
        equalsIgnoreCase(normalized, "normal_chest") ||
        normalized == "\xE9\x80\x9A\xE5\xB8\xB8\xE5\xAE\x9D\xE7\xAE\xB1") {
        outKind = LootChestKind::Common;
        return true;
    }
    if (equalsIgnoreCase(normalized, "R") ||
        equalsIgnoreCase(normalized, "rare") ||
        equalsIgnoreCase(normalized, "rare_chest") ||
        normalized == "\xE3\x83\xAC\xE3\x82\xA2\xE5\xAE\x9D\xE7\xAE\xB1") {
        outKind = LootChestKind::Rare;
        return true;
    }
    if (equalsIgnoreCase(normalized, "S") ||
        equalsIgnoreCase(normalized, "super") ||
        equalsIgnoreCase(normalized, "super_rare") ||
        equalsIgnoreCase(normalized, "super_rare_chest") ||
        normalized == "\xE8\xB6\x85\xE3\x83\xAC\xE3\x82\xA2\xE5\xAE\x9D\xE7\xAE\xB1") {
        outKind = LootChestKind::SuperRare;
        return true;
    }
    return false;
}

std::vector<std::string> expectedLootWeightColumns()
{
    return expectedStageWeightColumnNames(LootChestPrefixes);
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

bool isAllowed(std::string_view value, const auto& allowedValues)
{
    return std::any_of(allowedValues.begin(), allowedValues.end(), [value](std::string_view allowed) {
        return value == allowed;
    });
}

bool isLegacyPhysicalDamageType(std::string_view value)
{
    return equalsIgnoreCase(trim(value), "physical");
}

std::string lowerAscii(std::string_view text)
{
    std::string value(text);
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string formatDiscoveryNumber(double value)
{
    if (std::fabs(value - std::round(value)) < 0.001) {
        return std::to_string(static_cast<int>(std::lround(value)));
    }
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.1f", value);
    return buffer;
}

std::string formatDiscoveryPercent(double value)
{
    if (std::fabs(value - std::round(value)) < 0.001) {
        return std::to_string(static_cast<int>(std::lround(value))) + "%";
    }
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.1f%%", value);
    return buffer;
}

std::string statusNameJa(std::string_view effect)
{
    if (effect.find("paralyze") != std::string_view::npos) {
        return "麻痺";
    }
    if (effect.find("poison") != std::string_view::npos) {
        return "毒";
    }
    if (effect.find("slow") != std::string_view::npos) {
        return "鈍足";
    }
    if (effect.find("bleed") != std::string_view::npos) {
        return "出血";
    }
    if (effect.find("confuse") != std::string_view::npos) {
        return "混乱";
    }
    if (effect.find("blind") != std::string_view::npos) {
        return "盲目";
    }
    if (effect.find("wet") != std::string_view::npos) {
        return "濡れ";
    }
    if (effect.find("hot") != std::string_view::npos) {
        return "熱々";
    }
    if (effect.find("frozen") != std::string_view::npos) {
        return "凍結";
    }
    return "状態異常";
}

std::string normalizeEffectKey(std::string_view effect)
{
    if (effect == "guard") {
        return "guard_projectile";
    }
    return std::string(effect);
}

bool hasEffectCode(const std::vector<EffectSpec>& specs, std::string_view effect)
{
    for (const EffectSpec& spec : specs) {
        for (const std::string& code : spec.effects) {
            if (code == effect) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> splitEffectTextLines(std::string_view text)
{
    std::vector<std::string> lines;
    std::string current;
    const auto flush = [&]() {
        std::string line = trim(current);
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
        current.clear();
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == '\n' || ch == '\r') {
            flush();
            continue;
        }
        if (i + 2 < text.size() &&
            ch == 0xEF &&
            static_cast<unsigned char>(text[i + 1]) == 0xBD &&
            static_cast<unsigned char>(text[i + 2]) == 0x9C) {
            flush();
            i += 2;
            continue;
        }
        current.push_back(text[i]);
    }
    flush();
    return lines;
}

bool isNoDiscoveryEffectText(std::string_view text)
{
    const std::string normalized = trim(text);
    return equalsIgnoreCase(normalized, "none") || normalized == "\xE3\x81\xAA\xE3\x81\x97";
}

void pushDiscoveryLine(
    std::vector<DiscoveryEffectLine>& lines,
    const ObjectDefinition& object,
    std::string effectKey,
    std::string text,
    DiscoveryTrigger trigger)
{
    if (effectKey.empty() || text.empty()) {
        return;
    }
    const auto it = std::find_if(lines.begin(), lines.end(), [&effectKey](const DiscoveryEffectLine& line) {
        return line.effectKey == effectKey;
    });
    if (it != lines.end()) {
        return;
    }
    lines.push_back(DiscoveryEffectLine{
        .objectId = object.id,
        .effectKey = std::move(effectKey),
        .text = std::move(text),
        .trigger = trigger,
    });
}

bool autoTextForEffectCode(
    std::string_view effectCode,
    double value,
    double duration,
    std::string& outText)
{
    if (effectCode == "heal") {
        outText = value > 0.0
            ? "HPを回復する（" + formatDiscoveryNumber(value) + "）"
            : "HPを回復する";
        return true;
    }
    if (effectCode == "light") {
        outText = "周囲を照らす";
        return true;
    }
    if (effectCode == "detect_treasure" || effectCode == "detect") {
        outText = "宝を探知する";
        return true;
    }
    if (effectCode == "detect_hidden") {
        outText = "隠れたものを探知する";
        return true;
    }
    if (effectCode == "guard") {
        outText = "弾を防ぐ";
        return true;
    }
    if (effectCode == "guard_large") {
        outText = "大型弾を防ぐ";
        return true;
    }
    if (effectCode == "coin_drop_chance") {
        outText = "敵に当たると少額のお金を落とす（" + formatDiscoveryPercent(std::max(0.0, value)) + "）";
        return true;
    }
    if (effectCode == "hit_coin_spill") {
        outText = "敵に当たると小銭をこぼさせる（" + formatDiscoveryPercent(std::max(0.0, value)) + "）";
        return true;
    }
    if (effectCode == "break_coin_spill") {
        outText = value > 0.0
            ? "壊れると小銭をばらまく（" + formatDiscoveryNumber(value) + "倍）"
            : "壊れると小銭をばらまく";
        return true;
    }
    if (effectCode == "break_random_item_drop") {
        outText = "壊れるとランダムなアイテムを落とす（" + formatDiscoveryPercent(std::max(0.0, value)) + "）";
        return true;
    }
    if (effectCode == "vacuum_pull_light") {
        outText = "回転中に軽いドロップや小型の敵を引き寄せる";
        return true;
    }
    if (effectCode == "wind_push_light") {
        outText = "回転中に軽いドロップや小型の敵を押し出す";
        return true;
    }
    if (effectCode == "draw_white_line") {
        outText = "地面に白い線を書く";
        return true;
    }
    if (effectCode == "complete_magic_circle") {
        outText = "白線で丸く囲うと魔法陣を発動する";
        return true;
    }
    if (effectCode == "critical_chance") {
        outText = "会心率を上げる（" + formatDiscoveryPercent(std::max(0.0, value)) + "）";
        return true;
    }
    if (effectCode == "critical_power") {
        outText = value > 0.0
            ? "会心ダメージ倍率を変える（" + formatDiscoveryNumber(value) + "倍）"
            : "会心ダメージ倍率を変える";
        return true;
    }
    if (effectCode == "forced_critical_hit") {
        outText = value > 0.0
            ? "敵に当たると必ず会心になる（" + formatDiscoveryNumber(value) + "倍）"
            : "敵に当たると必ず会心になる";
        return true;
    }
    if (effectCode == "break_ceramic_shards") {
        outText = "壊れると陶器片が飛び散り、周囲の敵に小ダメージを与える";
        return true;
    }
    if (effectCode == "break_glass_shards") {
        outText = "壊れるとガラス片が飛び散り、周囲の敵に小ダメージを与える";
        return true;
    }
    if (effectCode == "break_fire_burst") {
        outText = "壊れると火が弾け、周囲の敵に火ダメージを与える";
        return true;
    }
    if (effectCode == "water_spray") {
        outText = "壊れると水をまき散らし、周囲の敵を濡らす";
        return true;
    }
    if (effectCode == "hot_air") {
        outText = "回転中に熱気を出し、周囲の敵を熱々にする";
        return true;
    }
    if (effectCode == "dry_wet_bonus_damage") {
        outText = value > 0.0
            ? "濡れた敵を乾かすと追加ダメージを与える（+" + formatDiscoveryNumber(value) + "）"
            : "濡れた敵を乾かすと追加ダメージを与える";
        return true;
    }
    if (effectCode == "cold_air_aura") {
        outText = "回転中に冷気を吹き出し、浴び続けた敵を凍結させる";
        return true;
    }
    if (effectCode == "slash_power") {
        outText = value > 0.0
            ? "斬撃接触ダメージを強める（" + formatDiscoveryNumber(value) + "倍）"
            : "斬撃接触ダメージを強める";
        return true;
    }
    if (effectCode == "item_orbit_offset") {
        outText = value != 0.0
            ? "このアイテムだけ軌道距離を補正する（" + formatDiscoveryNumber(value) + "px）"
            : "このアイテムだけ軌道距離を補正する";
        return true;
    }
    if (effectCode == "regen") {
        outText = value > 0.0
            ? "少しずつHPを回復する（毎秒" + formatDiscoveryNumber(value) + "）"
            : "少しずつHPを回復する";
        return true;
    }
    if (effectCode == "reflect_physical") {
        outText = "物理弾を反射する";
        return true;
    }
    if (effectCode == "reflect_physical_chance") {
        outText = "物理弾を確率で反射（" + formatDiscoveryPercent(std::max(0.0, value)) + "）";
        return true;
    }
    if (effectCode == "reflect_magic") {
        outText = "元素/魔法弾を反射する";
        return true;
    }
    if (effectCode == "reflect_magic_chance") {
        outText = "元素/魔法弾を確率で反射（" + formatDiscoveryPercent(std::max(0.0, value)) + "）";
        return true;
    }
    if (effectCode == "reflect_water") {
        outText = "水弾を反射する";
        return true;
    }
    if (effectCode == "reflect_water_chance") {
        outText = "水弾を確率で反射（" + formatDiscoveryPercent(std::max(0.0, value)) + "）";
        return true;
    }
    if (effectCode.rfind("status_", 0) == 0) {
        const bool chance = effectCode.size() > 7 && effectCode.rfind("_chance") == effectCode.size() - 7;
        const std::string name = statusNameJa(effectCode);
        if (chance) {
            outText = "敵を" + name + "させる（" + formatDiscoveryPercent(std::max(0.0, value)) + "）";
        } else if (duration > 0.0) {
            outText = "敵を" + name + "させる（" + formatDiscoveryNumber(duration) + "秒）";
        } else {
            outText = "敵を" + name + "させる";
        }
        return true;
    }
    if (effectCode == "buff_attack" || effectCode == "debuff_attack") {
        outText = (effectCode == "buff_attack" ? "攻撃力を上げる" : "攻撃力を下げる");
        if (duration > 0.0) {
            outText += "（" + formatDiscoveryNumber(duration) + "秒）";
        }
        return true;
    }
    if (effectCode == "buff_speed" || effectCode == "debuff_speed") {
        outText = (effectCode == "buff_speed" ? "移動速度を上げる" : "移動速度を下げる");
        if (duration > 0.0) {
            outText += "（" + formatDiscoveryNumber(duration) + "秒）";
        }
        return true;
    }
    if (effectCode == "buff_defense" || effectCode == "debuff_defense") {
        outText = (effectCode == "buff_defense" ? "防御力を上げる" : "防御力を下げる");
        if (duration > 0.0) {
            outText += "（" + formatDiscoveryNumber(duration) + "秒）";
        }
        return true;
    }
    if (effectCode == "knockback") {
        outText = "敵をノックバックさせる";
        return true;
    }
    if (effectCode == "orbit_speed") {
        outText = "リング回転速度を変化させる";
        return true;
    }
    if (effectCode == "orbit_gravity") {
        outText = "リング軌道を内側へ寄せる";
        return true;
    }
    if (effectCode == "orbit_power") {
        outText = "リング出力を変化させる";
        return true;
    }
    if (effectCode == "orbit_antigravity") {
        outText = "リング軌道を外側へ広げる";
        return true;
    }
    if (effectCode == "orbit_anchor") {
        outText = "リング中心を安定させる";
        return true;
    }
    if (effectCode == "orbit_shift") {
        outText = "リングずらし距離を変化させる";
        return true;
    }
    if (effectCode == "damage_speed") {
        outText = "速度ダメージ補正を強める";
        return true;
    }
    return false;
}

const DeprecatedName* findDeprecated(std::string_view name, const auto& deprecatedNames)
{
    const auto it = std::find_if(
        deprecatedNames.begin(),
        deprecatedNames.end(),
        [name](const DeprecatedName& candidate) {
            return candidate.name == name;
        });
    return it != deprecatedNames.end() ? &*it : nullptr;
}

bool splitTopLevel(std::string_view text, char delimiter, std::vector<std::string>& outParts)
{
    outParts.clear();
    int bracketDepth = 0;
    std::size_t partBegin = 0;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '[') {
            ++bracketDepth;
        } else if (ch == ']') {
            --bracketDepth;
            if (bracketDepth < 0) {
                return false;
            }
        } else if (ch == delimiter && bracketDepth == 0) {
            outParts.push_back(trim(text.substr(partBegin, i - partBegin)));
            partBegin = i + 1;
        }
    }

    if (bracketDepth != 0) {
        return false;
    }

    outParts.push_back(trim(text.substr(partBegin)));
    return true;
}

bool isBracketedList(std::string_view text)
{
    return text.size() >= 2 && text.front() == '[' && text.back() == ']';
}

bool containsBracket(std::string_view text)
{
    return text.find('[') != std::string_view::npos || text.find(']') != std::string_view::npos;
}

bool parseStringListToken(std::string_view token, std::vector<std::string>& outValues)
{
    const std::string normalized = trim(token);
    outValues.clear();
    if (normalized.empty()) {
        return false;
    }

    if (!isBracketedList(normalized)) {
        if (containsBracket(normalized) || normalized.find(',') != std::string::npos) {
            return false;
        }
        outValues.push_back(normalized);
        return true;
    }

    const std::string_view inner(normalized.data() + 1, normalized.size() - 2);
    std::vector<std::string> parts;
    if (!splitTopLevel(inner, ',', parts) || parts.empty()) {
        return false;
    }

    for (std::string& part : parts) {
        if (part.empty() || containsBracket(part)) {
            return false;
        }
        outValues.push_back(part);
    }
    return !outValues.empty();
}

bool parseNumberListToken(
    std::string_view token,
    std::vector<double>& outValues,
    std::vector<std::string>* outWarnings,
    std::string_view packet)
{
    const std::string normalized = trim(token);
    outValues.clear();
    if (normalized.empty()) {
        return false;
    }

    if (!isBracketedList(normalized)) {
        if (containsBracket(normalized) || normalized.find(',') != std::string::npos) {
            return false;
        }
        double value = 0.0;
        if (!parseDoubleStrict(normalized, value)) {
            if (outWarnings != nullptr) {
                outWarnings->push_back("effect value is not a number: \"" + normalized +
                    "\" in packet \"" + std::string(packet) + "\"; using 0");
            }
            value = 0.0;
        }
        outValues.push_back(value);
        return true;
    }

    const std::string_view inner(normalized.data() + 1, normalized.size() - 2);
    std::vector<std::string> parts;
    if (!splitTopLevel(inner, ',', parts) || parts.empty()) {
        return false;
    }

    for (const std::string& part : parts) {
        double value = 0.0;
        if (!parseDoubleStrict(part, value)) {
            if (outWarnings != nullptr) {
                outWarnings->push_back("effect value is not a number: \"" + part +
                    "\" in packet \"" + std::string(packet) + "\"; using 0");
            }
            value = 0.0;
        }
        outValues.push_back(value);
    }
    return !outValues.empty();
}

bool parseEffectPacket(
    std::string_view packet,
    EffectSpec& outSpec,
    std::string& outError,
    std::vector<std::string>* outWarnings)
{
    std::vector<std::string> fields;
    if (!splitTopLevel(packet, ':', fields) || fields.size() != 4) {
        outError = "effect packet must have 4 fields";
        return false;
    }

    EffectSpec spec;
    spec.target = trim(fields[0]);
    if (spec.target.empty()) {
        outError = "effect target is empty";
        return false;
    }
    if (!parseStringListToken(fields[1], spec.effects)) {
        outError = "effect list is invalid";
        return false;
    }
    if (!parseNumberListToken(fields[2], spec.values, outWarnings, packet)) {
        outError = "effect value list is invalid";
        return false;
    }
    if (spec.effects.size() != spec.values.size()) {
        outError = "effect and value counts do not match";
        return false;
    }
    if (!parseDoubleStrict(fields[3], spec.duration)) {
        if (outWarnings != nullptr) {
            outWarnings->push_back("effect duration is not a number: \"" + fields[3] +
                "\" in packet \"" + std::string(packet) + "\"; using 0");
        }
        spec.duration = 0.0;
    }

    outSpec = std::move(spec);
    return true;
}

std::vector<std::string> parseCommaSeparatedValues(std::string_view text)
{
    std::vector<std::string> values;
    std::vector<std::string> parts;
    if (!splitTopLevel(text, ',', parts)) {
        return values;
    }
    for (std::string& part : parts) {
        if (!part.empty()) {
            values.push_back(part);
        }
    }
    return values;
}

std::vector<std::string> parseTags(std::string_view text)
{
    std::vector<std::string> tags;
    std::string current;
    int bracketDepth = 0;

    const auto flush = [&tags, &current]() {
        std::string tag = trim(current);
        if (!tag.empty()) {
            tags.push_back(std::move(tag));
        }
        current.clear();
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == '[') {
            ++bracketDepth;
        } else if (ch == ']' && bracketDepth > 0) {
            --bracketDepth;
        }

        if (bracketDepth == 0 && (ch == ',' || ch == ';' || ch == '|')) {
            flush();
            continue;
        }

        // UTF-8 Japanese comma "、".
        if (bracketDepth == 0 &&
            i + 2 < text.size() &&
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
    return tags;
}

std::string rowError(std::size_t rowIndex, std::string_view message)
{
    return "invalid Objects row " + std::to_string(rowIndex + 1) + ": " + std::string(message);
}

void appendRowIssues(
    ObjectCatalog& catalog,
    std::size_t rowIndex,
    std::string_view itemId,
    std::string_view fieldName,
    DbValidationCategory category,
    const std::vector<std::string>& warnings)
{
    for (const std::string& warning : warnings) {
        addValidationIssue(
            catalog,
            DbValidationSeverity::Warning,
            category,
            "Objects row " + std::to_string(rowIndex + 1) +
                " item=\"" + std::string(itemId) + "\" " + std::string(fieldName) + ": " + warning);
    }
}

int parseIntColumnOrDefault(
    std::string_view text,
    int defaultValue,
    std::string_view columnName,
    std::size_t rowIndex,
    std::string_view itemId,
    ObjectCatalog& catalog,
    int minValue,
    int maxValue)
{
    int value = defaultValue;
    if (!parseIntStrict(text, value)) {
        addValidationIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            "Objects row " + std::to_string(rowIndex + 1) +
                " item=\"" + std::string(itemId) + "\" " + std::string(columnName) +
                ": invalid number \"" + trim(text) + "\"; using " + std::to_string(defaultValue));
        return defaultValue;
    }
    if (value < minValue || value > maxValue) {
        addValidationIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            "Objects row " + std::to_string(rowIndex + 1) +
                " item=\"" + std::string(itemId) + "\" " + std::string(columnName) +
                ": out of range " + std::to_string(value) + "; using " + std::to_string(defaultValue));
        return defaultValue;
    }
    return value;
}

double parseDoubleColumnOrDefault(
    std::string_view text,
    double defaultValue,
    std::string_view columnName,
    std::size_t rowIndex,
    std::string_view itemId,
    ObjectCatalog& catalog,
    double minValue)
{
    double value = defaultValue;
    if (!parseDoubleStrict(text, value)) {
        addValidationIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            "Objects row " + std::to_string(rowIndex + 1) +
                " item=\"" + std::string(itemId) + "\" " + std::string(columnName) +
                ": invalid number \"" + trim(text) + "\"; using " + std::to_string(defaultValue));
        return defaultValue;
    }
    if (value < minValue) {
        addValidationIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            "Objects row " + std::to_string(rowIndex + 1) +
                " item=\"" + std::string(itemId) + "\" " + std::string(columnName) +
                ": out of range " + std::to_string(value) + "; using " + std::to_string(defaultValue));
        return defaultValue;
    }
    return value;
}

float parseOptionalFloatColumnOrDefault(
    std::string_view text,
    float defaultValue,
    std::string_view columnName,
    std::size_t rowIndex,
    std::string_view itemId,
    ObjectCatalog& catalog)
{
    const std::string normalized = trim(text);
    if (normalized.empty()) {
        return defaultValue;
    }

    double value = static_cast<double>(defaultValue);
    if (!parseDoubleStrict(normalized, value)) {
        addValidationIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            "Objects row " + std::to_string(rowIndex + 1) +
                " item=\"" + std::string(itemId) + "\" " + std::string(columnName) +
                ": invalid number \"" + normalized + "\"; using " + std::to_string(defaultValue));
        return defaultValue;
    }
    return static_cast<float>(value);
}

std::vector<std::string> splitColonTokens(std::string_view text)
{
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : text) {
        if (ch == ':') {
            tokens.push_back(trim(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    tokens.push_back(trim(current));
    return tokens;
}

bool parseRotationFloatToken(std::string_view text, float& outValue)
{
    double value = 0.0;
    if (!parseDoubleStrict(text, value)) {
        return false;
    }
    outValue = static_cast<float>(value);
    return true;
}

ObjectRingRotation parseObjectRingRotation(
    std::string_view text,
    std::size_t rowIndex,
    std::string_view itemId,
    ObjectCatalog& catalog)
{
    ObjectRingRotation rotation;
    const std::string normalizedText = trim(text);
    if (normalizedText.empty()) {
        return rotation;
    }

    float numericOffset = 0.0f;
    if (parseRotationFloatToken(normalizedText, numericOffset)) {
        rotation.offsetDegrees = numericOffset;
        return rotation;
    }

    const std::vector<std::string> tokens = splitColonTokens(normalizedText);
    const std::string mode = tokens.empty() ? std::string{} : lowerAscii(tokens.front());
    auto addWarning = [&](std::string message) {
        addValidationIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::ObjectField,
            "Objects row " + std::to_string(rowIndex + 1) +
                " item=\"" + std::string(itemId) + "\" ring rotation: " + std::move(message) +
                "; using fixed");
    };

    if (mode == "fixed" || mode == "none" || mode == "0") {
        rotation.mode = ObjectRingRotationMode::Fixed;
    } else if (mode == "outward" || mode == "outer") {
        rotation.mode = ObjectRingRotationMode::Outward;
    } else if (mode == "forward" || mode == "tangent") {
        rotation.mode = ObjectRingRotationMode::Forward;
    } else if (mode == "spin" || mode == "rotate") {
        rotation.mode = ObjectRingRotationMode::Spin;
    } else {
        addWarning("unknown mode \"" + normalizedText + "\"");
        return {};
    }

    if (rotation.mode == ObjectRingRotationMode::Spin) {
        if (tokens.size() >= 2 && !tokens[1].empty()) {
            if (!parseRotationFloatToken(tokens[1], rotation.spinDegreesPerSecond)) {
                addWarning("invalid spin speed \"" + tokens[1] + "\"");
                return {};
            }
        }
        if (tokens.size() >= 3 && !tokens[2].empty()) {
            if (!parseRotationFloatToken(tokens[2], rotation.offsetDegrees)) {
                addWarning("invalid spin offset \"" + tokens[2] + "\"");
                return {};
            }
        }
    } else if (tokens.size() >= 2 && !tokens[1].empty()) {
        if (!parseRotationFloatToken(tokens[1], rotation.offsetDegrees)) {
            addWarning("invalid offset \"" + tokens[1] + "\"");
            return {};
        }
    }

    return rotation;
}

bool isUnsetLootWeightCell(std::string_view text)
{
    const std::string value = trim(text);
    return value.empty() ||
        equalsIgnoreCase(value, "undefined") ||
        equalsIgnoreCase(value, "null") ||
        equalsIgnoreCase(value, "none") ||
        value == "\xE6\x9C\xAA\xE5\xAE\x9A\xE7\xBE\xA9";
}

double parseLootWeightOrZero(
    std::string_view text,
    std::string_view columnName,
    std::size_t rowIndex,
    std::string_view itemId,
    ObjectCatalog& catalog)
{
    if (isUnsetLootWeightCell(text)) {
        return 0.0;
    }

    double value = 0.0;
    if (!parseDoubleStrict(text, value)) {
        ++catalog.lootWeightStats.warningCount;
        addValidationIssue(
            catalog,
            DbValidationSeverity::Warning,
            DbValidationCategory::NumericValue,
            "Objects row " + std::to_string(rowIndex + 1) +
                " item=\"" + std::string(itemId) + "\" loot weight " + std::string(columnName) +
                ": invalid number \"" + trim(text) + "\"; using 0");
        return 0.0;
    }
    return value;
}

struct ObjectColumns {
    int id = -1;
    int name = -1;
    int category = -1;
    int description = -1;
    int rarity = -1;
    int price = -1;
    int useEffects = -1;
    int orbitEffects = -1;
    int attackPower = -1;
    int damageType = -1;
    int digPower = -1;
    int durability = -1;
    int weight = -1;
    int imageNumber = -1;
    int groundRotation = -1;
    int ringRotation = -1;
    int tags = -1;
    int effectText = -1;
    std::vector<std::pair<std::string, int>> lootWeightColumns;
};

bool findObjectColumns(const GoogleSheetRow& headers, ObjectColumns& outColumns, std::string& outError)
{
    ObjectColumns columns;
    columns.id = findColumn(headers, {"ID", "id"});
    columns.name = findColumn(headers, {HeaderName, "name"});
    columns.category = findColumn(headers, {HeaderCategory, "category"});
    columns.description = findColumn(headers, {HeaderDescription, HeaderLegacyDescription, "description", "desc"});
    columns.rarity = findColumn(headers, {HeaderRarity, "rarity"});
    columns.price = findColumn(headers, {HeaderPrice, HeaderLegacyPrice, "price"});
    columns.useEffects = findColumn(headers, {HeaderUseEffects, "use_effects", "useEffects"});
    columns.orbitEffects = findColumn(headers, {HeaderOrbitEffects, "orbit_effects", "orbitEffects"});
    columns.attackPower = findColumn(headers, {HeaderAttackPower, "attack_power", "attackPower", "attack", "power"});
    columns.damageType = findColumn(headers, {HeaderDamageType, "damage_type", "damageType"});
    columns.digPower = findColumn(headers, {HeaderDigPower, "dig_power", "digPower"});
    columns.durability = findColumn(headers, {HeaderDurability, "durability"});
    columns.weight = findColumn(headers, {HeaderWeight, "weight_kg", "weightKg", "weight"});
    columns.imageNumber = findColumn(headers, {HeaderImageNumber, "image_number", "imageNumber"});
    columns.groundRotation = findColumn(headers, {HeaderGroundRotation, "ground_rotation", "ground_rotation_deg", "ground_angle"});
    columns.ringRotation = findColumn(headers, {HeaderRingRotation, "ring_rotation", "orbit_rotation", "ring_angle", "orbit_angle"});
    columns.tags = findColumn(headers, {HeaderTags, "tags"});
    columns.effectText = findColumn(headers, {HeaderEffectText, "effect_text", "effectText", "effect"});
    for (const std::string& columnName : expectedLootWeightColumns()) {
        const int column = findColumn(headers, {std::string_view(columnName)});
        if (column >= 0) {
            columns.lootWeightColumns.emplace_back(columnName, column);
        }
    }

    if (columns.id < 0) {
        outError = "Objects sheet is missing the ID column";
        return false;
    }
    if (columns.name < 0) {
        outError = "Objects sheet is missing the name column";
        return false;
    }
    if (columns.category < 0) {
        outError = "Objects sheet is missing the category column";
        return false;
    }
    if (columns.rarity < 0) {
        outError = "Objects sheet is missing the rarity column";
        return false;
    }
    if (columns.price < 0) {
        outError = "Objects sheet is missing the price column";
        return false;
    }
    if (columns.useEffects < 0) {
        outError = "Objects sheet is missing the use effects column";
        return false;
    }
    if (columns.orbitEffects < 0) {
        outError = "Objects sheet is missing the orbit effects column";
        return false;
    }
    if (columns.attackPower < 0) {
        outError = "Objects sheet is missing the attack power column";
        return false;
    }
    if (columns.damageType < 0) {
        outError = "Objects sheet is missing the damage type column";
        return false;
    }
    if (columns.digPower < 0) {
        outError = "Objects sheet is missing the dig power column";
        return false;
    }
    if (columns.durability < 0) {
        outError = "Objects sheet is missing the durability column";
        return false;
    }
    if (columns.weight < 0) {
        outError = "Objects sheet is missing the weight column";
        return false;
    }
    if (columns.tags < 0) {
        outError = "Objects sheet is missing the tags column";
        return false;
    }
    if (columns.effectText < 0) {
        outError = "Objects sheet is missing the effect text column";
        return false;
    }

    outColumns = columns;
    return true;
}

struct EffectCodeColumns {
    int code = -1;
    int displayName = -1;
    int category = -1;
    int usableField = -1;
    int supportedTargets = -1;
    int valueMeaning = -1;
    int durationMeaning = -1;
    int stackingRule = -1;
    int implementationState = -1;
    int note = -1;
};

bool findEffectCodeColumns(const GoogleSheetRow& headers, EffectCodeColumns& outColumns, std::string& outError)
{
    EffectCodeColumns columns;
    columns.code = findColumn(headers, {HeaderEffectCode, "effect_code", "code"});
    columns.displayName = findColumn(headers, {HeaderDisplayName, "display_name", "displayName", "name"});
    columns.category = findColumn(headers, {HeaderClassification, "category", "classification"});
    columns.usableField = findColumn(headers, {HeaderUsableField, "usable_field", "usableField"});
    columns.supportedTargets = findColumn(headers, {HeaderSupportedTargets, "supported_targets", "targets", "target"});
    columns.valueMeaning = findColumn(headers, {HeaderValueMeaning, "value_meaning", "valueMeaning"});
    columns.durationMeaning = findColumn(headers, {HeaderDurationMeaning, "duration_meaning", "durationMeaning"});
    columns.stackingRule = findColumn(headers, {HeaderStackingRule, "stacking_rule", "stackingRule"});
    columns.implementationState = findColumn(headers, {HeaderImplementationState, "implementation_state", "implementationState", "status"});
    columns.note = findColumn(headers, {HeaderNote, "note", "notes"});

    if (columns.code < 0) {
        outError = "Effect code sheet is missing the effect code column";
        return false;
    }
    if (columns.displayName < 0) {
        outError = "Effect code sheet is missing the display name column";
        return false;
    }

    outColumns = columns;
    return true;
}

struct SpecialTagColumns {
    int tag = -1;
    int displayName = -1;
    int category = -1;
    int usage = -1;
    int targetCategories = -1;
    int exclusiveGroup = -1;
    int relatedEffectCodes = -1;
    int implementationState = -1;
    int note = -1;
};

bool findSpecialTagColumns(const GoogleSheetRow& headers, SpecialTagColumns& outColumns, std::string& outError)
{
    SpecialTagColumns columns;
    columns.tag = findColumn(headers, {HeaderSpecialTag, "special_tag", "tag"});
    columns.displayName = findColumn(headers, {HeaderDisplayName, "display_name", "displayName", "name"});
    columns.category = findColumn(headers, {HeaderClassification, "category", "classification"});
    columns.usage = findColumn(headers, {HeaderUsage, "usage", "purpose"});
    columns.targetCategories = findColumn(headers, {HeaderTargetCategories, "target_categories", "targetCategories"});
    columns.exclusiveGroup = findColumn(headers, {HeaderExclusiveGroup, "exclusive_group", "exclusiveGroup"});
    columns.relatedEffectCodes = findColumn(headers, {HeaderRelatedEffectCodes, "related_effect_codes", "relatedEffectCodes"});
    columns.implementationState = findColumn(headers, {HeaderImplementationState, "implementation_state", "implementationState", "status"});
    columns.note = findColumn(headers, {HeaderNote, "note", "notes"});

    if (columns.tag < 0) {
        outError = "Special tag sheet is missing the special tag column";
        return false;
    }
    if (columns.displayName < 0) {
        outError = "Special tag sheet is missing the display name column";
        return false;
    }

    outColumns = columns;
    return true;
}

}

bool isDamageTypeAllowed(std::string_view value)
{
    const std::string normalized = lowerAscii(trim(value));
    return isAllowed(normalized, AllowedDamageTypes);
}

bool isPhysicalDamageType(std::string_view value)
{
    const std::string normalized = lowerAscii(trim(value));
    return normalized == "slash" ||
        normalized == "blunt" ||
        normalized == "pierce" ||
        normalized == "physical";
}

std::string normalizeDamageType(std::string_view value)
{
    const std::string normalized = lowerAscii(trim(value));
    if (normalized.empty()) {
        return {};
    }
    if (normalized == "physical") {
        return "blunt";
    }
    if (!isAllowed(normalized, AllowedDamageTypes)) {
        return {};
    }
    return normalized;
}

std::string_view damageTypeDisplayName(std::string_view value)
{
    const std::string normalized = lowerAscii(trim(value));
    if (normalized == "slash") {
        return "\xE6\x96\xAC\xE6\x92\x83";
    }
    if (normalized == "blunt") {
        return "\xE6\x89\x93\xE6\x92\x83";
    }
    if (normalized == "pierce") {
        return "\xE5\x88\xBA\xE7\xAA\x81";
    }
    if (normalized == "fire") {
        return "\xE7\x81\xAB";
    }
    if (normalized == "ice") {
        return "\xE6\xB0\xB7";
    }
    if (normalized == "thunder") {
        return "\xE9\x9B\xB7";
    }
    if (normalized == "wind") {
        return "\xE9\xA2\xA8";
    }
    if (normalized == "earth") {
        return "\xE5\x9C\x9F";
    }
    if (normalized == "water") {
        return "\xE6\xB0\xB4";
    }
    if (normalized == "magic") {
        return "\xE9\xAD\x94\xE6\xB3\x95";
    }
    return normalized == "none" ? "\xE3\x81\xAA\xE3\x81\x97" : "\xE3\x81\xAA\xE3\x81\x97";
}

std::vector<DiscoveryEffectLine> buildDiscoveryEffectLines(const ObjectDefinition& object, std::vector<std::string>* debugWarnings)
{
    std::vector<DiscoveryEffectLine> lines;

    if (object.attackPower > 0 && !object.damageType.empty() && object.damageType != "none") {
        pushDiscoveryLine(
            lines,
            object,
            "basic_attack",
            "\xE6\x95\xB5\xE3\x81\xAB" + std::string(damageTypeDisplayName(object.damageType)) +
                "\xE3\x83\x80\xE3\x83\xA1\xE3\x83\xBC\xE3\x82\xB8\xEF\xBC\x88\xE6\x94\xBB\xE6\x92\x83\xE5\x8A\x9B" +
                std::to_string(object.attackPower) + "\xEF\xBC\x89",
            DiscoveryTrigger::Attack);
    }

    const bool hasDig = hasEffectCode(object.normalEffects, "dig") ||
        hasEffectCode(object.normalEffects, "dig_multi") ||
        hasEffectCode(object.normalEffects, "dig_hard") ||
        hasEffectCode(object.orbitEffects, "dig") ||
        hasEffectCode(object.orbitEffects, "dig_multi") ||
        hasEffectCode(object.orbitEffects, "dig_hard");
    const bool hasDigHard = hasEffectCode(object.normalEffects, "dig_hard") ||
        hasEffectCode(object.orbitEffects, "dig_hard");
    if (hasDig && object.digPower > 0) {
        const std::string key = hasDigHard ? "dig_hard" : "dig";
        const std::string text = hasDigHard
            ? "\xE7\xA1\xAC\xE3\x81\x84\xE5\x9C\x9F\xE3\x82\x92\xE6\x8E\x98\xE3\x82\x8C\xE3\x82\x8B\xEF\xBC\x88\xE6\x8E\x98\xE5\x89\x8A\xE5\x8A\x9B" + std::to_string(object.digPower) + "\xEF\xBC\x89"
            : "\xE5\x9C\x9F\xE3\x82\x92\xE6\x8E\x98\xE3\x82\x8C\xE3\x82\x8B\xEF\xBC\x88\xE6\x8E\x98\xE5\x89\x8A\xE5\x8A\x9B" + std::to_string(object.digPower) + "\xEF\xBC\x89";
        pushDiscoveryLine(lines, object, key, text, DiscoveryTrigger::Dig);
    }

    const auto appendFromSpecs = [&](const std::vector<EffectSpec>& specs, DiscoveryTrigger trigger) {
        for (const EffectSpec& spec : specs) {
            const std::size_t count = std::min(spec.effects.size(), spec.values.size());
            for (std::size_t i = 0; i < count; ++i) {
                const std::string& effect = spec.effects[i];
                if (effect.empty() || effect == "none" || effect == "dig" || effect == "dig_hard" || effect == "dig_multi") {
                    continue;
                }
                std::string text;
                if (!autoTextForEffectCode(effect, spec.values[i], spec.duration, text)) {
                    if (debugWarnings != nullptr) {
                        debugWarnings->push_back(
                            "object=\"" + object.id + "\" discovery effect line skipped: unsupported effect code \"" + effect + "\"");
                    }
                    continue;
                }
                pushDiscoveryLine(lines, object, normalizeEffectKey(effect), std::move(text), trigger);
            }
        }
    };

    appendFromSpecs(object.normalEffects, DiscoveryTrigger::NormalEffect);
    appendFromSpecs(object.orbitEffects, DiscoveryTrigger::OrbitEffect);

    const std::vector<std::string> manualLines = splitEffectTextLines(object.effectText);
    if (!manualLines.empty()) {
        if (manualLines.size() == 1 && isNoDiscoveryEffectText(manualLines.front())) {
            return {};
        }
        if (manualLines.size() == lines.size()) {
            for (std::size_t i = 0; i < lines.size(); ++i) {
                lines[i].text = manualLines[i];
            }
        } else if (debugWarnings != nullptr) {
            debugWarnings->push_back(
                "object=\"" + object.id + "\" effectText line count mismatch: generated=" +
                std::to_string(lines.size()) + ", manual=" + std::to_string(manualLines.size()) +
                "; generated text is used");
        }
    }

    return lines;
}

void ItemRegistry::rebuild(std::vector<ItemData> items)
{
    items_ = std::move(items);
    idIndex_.clear();
    categoryIndex_.clear();
    tagIndex_.clear();

    for (std::size_t i = 0; i < items_.size(); ++i) {
        const ItemData& item = items_[i];
        idIndex_.emplace(item.id, i);
        categoryIndex_[item.category].push_back(i);
        for (const std::string& tag : item.tags) {
            tagIndex_[tag].push_back(i);
        }
    }
}

void ItemRegistry::clear()
{
    items_.clear();
    idIndex_.clear();
    categoryIndex_.clear();
    tagIndex_.clear();
}

const std::vector<ItemData>& ItemRegistry::items() const
{
    return items_;
}

const ItemData* ItemRegistry::findById(std::string_view id) const
{
    const auto it = idIndex_.find(std::string(id));
    if (it == idIndex_.end()) {
        return nullptr;
    }
    return &items_[it->second];
}

std::vector<const ItemData*> ItemRegistry::findByCategory(std::string_view category) const
{
    std::vector<const ItemData*> results;
    const auto it = categoryIndex_.find(std::string(category));
    if (it == categoryIndex_.end()) {
        return results;
    }
    results.reserve(it->second.size());
    for (std::size_t index : it->second) {
        results.push_back(&items_[index]);
    }
    return results;
}

std::vector<const ItemData*> ItemRegistry::findByTag(std::string_view tag) const
{
    std::vector<const ItemData*> results;
    const auto it = tagIndex_.find(std::string(tag));
    if (it == tagIndex_.end()) {
        return results;
    }
    results.reserve(it->second.size());
    for (std::size_t index : it->second) {
        results.push_back(&items_[index]);
    }
    return results;
}

bool ItemRegistry::empty() const
{
    return items_.empty();
}

std::size_t ItemRegistry::size() const
{
    return items_.size();
}

double ObjectLootWeights::weightForColumn(std::string_view columnName) const
{
    const auto it = byColumn.find(std::string(columnName));
    return it != byColumn.end() && it->second > 0.0 ? it->second : 0.0;
}

bool ObjectLootWeights::hasPositiveWeight() const
{
    return std::any_of(byColumn.begin(), byColumn.end(), [](const auto& entry) {
        return entry.second > 0.0;
    });
}

std::string effectCodeDisplayName(const ObjectCatalog& catalog, std::string_view code)
{
    const auto it = catalog.effectCodes.find(std::string(code));
    if (it != catalog.effectCodes.end() && !it->second.displayName.empty()) {
        return it->second.displayName;
    }
    return std::string(code);
}

std::string effectSummaryText(const ObjectCatalog& catalog, const std::vector<EffectSpec>& specs)
{
    std::string summary;
    for (const EffectSpec& spec : specs) {
        for (const std::string& effect : spec.effects) {
            if (!summary.empty()) {
                summary += ", ";
            }
            summary += effectCodeDisplayName(catalog, effect);
        }
    }
    return summary.empty() ? "-" : summary;
}

std::string resolveLootWeightColumnName(std::string_view stageId, int depthRank, LootChestKind chestKind)
{
    const std::string_view chestPrefix = lootChestPrefix(chestKind);
    if (chestPrefix.empty()) {
        return {};
    }
    return makeStageWeightColumnName(stageId, depthRank, chestPrefix);
}

std::string resolveLootWeightColumnName(std::string_view stageId, int depthRank, std::string_view chestKind)
{
    LootChestKind parsedKind = LootChestKind::Common;
    if (!parseLootChestKind(chestKind, parsedKind)) {
        return {};
    }
    return resolveLootWeightColumnName(stageId, depthRank, parsedKind);
}

double lootWeightFor(const ObjectDefinition& object, std::string_view stageId, int depthRank, LootChestKind chestKind)
{
    const std::string columnName = resolveLootWeightColumnName(stageId, depthRank, chestKind);
    return columnName.empty() ? 0.0 : object.lootWeights.weightForColumn(columnName);
}

bool parseEffectSpecs(
    std::string_view text,
    std::vector<EffectSpec>& outEffects,
    std::string& outError,
    std::vector<std::string>* outWarnings)
{
    outEffects.clear();
    const std::string normalized = trim(text);
    if (normalized.empty()) {
        outError = "effect is empty";
        return false;
    }
    if (equalsIgnoreCase(normalized, "none")) {
        outError.clear();
        return true;
    }

    std::vector<std::string> packets;
    if (!splitTopLevel(normalized, ';', packets)) {
        outError = "effect packets have invalid brackets";
        return false;
    }

    for (const std::string& packet : packets) {
        if (packet.empty()) {
            outError = "effect packet is empty";
            outEffects.clear();
            return false;
        }

        EffectSpec spec;
        if (!parseEffectPacket(packet, spec, outError, outWarnings)) {
            outEffects.clear();
            return false;
        }
        outEffects.push_back(std::move(spec));
    }

    outError.clear();
    return true;
}

bool parseEffectCodeDefinitions(
    const GoogleSheetTable& table,
    std::unordered_map<std::string, EffectCodeDefinition>& outDefinitions,
    std::string& outError)
{
    std::unordered_map<std::string, EffectCodeDefinition> definitions;
    if (table.rows.empty()) {
        outError = "Effect code sheet is empty";
        outDefinitions = {};
        return false;
    }

    EffectCodeColumns columns;
    if (!findEffectCodeColumns(table.rows.front(), columns, outError)) {
        outDefinitions = {};
        return false;
    }

    for (std::size_t rowIndex = 1; rowIndex < table.rows.size(); ++rowIndex) {
        const GoogleSheetRow& row = table.rows[rowIndex];
        if (!hasRowContent(row)) {
            continue;
        }

        EffectCodeDefinition definition;
        definition.code = cellAt(row, columns.code);
        if (definition.code.empty()) {
            continue;
        }

        definition.displayName = cellAt(row, columns.displayName);
        definition.category = cellAt(row, columns.category);
        definition.usableField = cellAt(row, columns.usableField);
        definition.targets = parseCommaSeparatedValues(cellAt(row, columns.supportedTargets));
        definition.valueMeaning = cellAt(row, columns.valueMeaning);
        definition.durationMeaning = cellAt(row, columns.durationMeaning);
        definition.stackingRule = cellAt(row, columns.stackingRule);
        definition.implementationState = cellAt(row, columns.implementationState);
        definition.note = cellAt(row, columns.note);

        definitions[definition.code] = std::move(definition);
    }

    outDefinitions = std::move(definitions);
    outError.clear();
    return true;
}

bool parseSpecialTagDefinitions(
    const GoogleSheetTable& table,
    std::unordered_map<std::string, SpecialTagDefinition>& outDefinitions,
    std::string& outError)
{
    std::unordered_map<std::string, SpecialTagDefinition> definitions;
    if (table.rows.empty()) {
        outError = "Special tag sheet is empty";
        outDefinitions = {};
        return false;
    }

    SpecialTagColumns columns;
    if (!findSpecialTagColumns(table.rows.front(), columns, outError)) {
        outDefinitions = {};
        return false;
    }

    for (std::size_t rowIndex = 1; rowIndex < table.rows.size(); ++rowIndex) {
        const GoogleSheetRow& row = table.rows[rowIndex];
        if (!hasRowContent(row)) {
            continue;
        }

        SpecialTagDefinition definition;
        definition.tag = cellAt(row, columns.tag);
        if (definition.tag.empty()) {
            continue;
        }

        definition.displayName = cellAt(row, columns.displayName);
        definition.category = cellAt(row, columns.category);
        definition.usage = cellAt(row, columns.usage);
        definition.targetCategories = parseCommaSeparatedValues(cellAt(row, columns.targetCategories));
        definition.exclusiveGroup = cellAt(row, columns.exclusiveGroup);
        definition.relatedEffectCodes = parseCommaSeparatedValues(cellAt(row, columns.relatedEffectCodes));
        definition.implementationState = cellAt(row, columns.implementationState);
        definition.note = cellAt(row, columns.note);

        definitions[definition.tag] = std::move(definition);
    }

    outDefinitions = std::move(definitions);
    outError.clear();
    return true;
}

bool parseObjectCatalog(const GoogleSheetTable& table, ObjectCatalog& outCatalog, std::string& outError)
{
    ObjectCatalog catalog;
    if (table.rows.empty()) {
        outError = "Objects sheet is empty";
        outCatalog = catalog;
        return false;
    }

    ObjectColumns columns;
    if (!findObjectColumns(table.rows.front(), columns, outError)) {
        outCatalog = catalog;
        return false;
    }
    catalog.lootWeightStats.detectedColumnCount = columns.lootWeightColumns.size();

    std::unordered_set<std::string> ids;
    for (std::size_t rowIndex = 1; rowIndex < table.rows.size(); ++rowIndex) {
        const GoogleSheetRow& row = table.rows[rowIndex];
        if (!hasRowContent(row)) {
            continue;
        }

        ObjectDefinition item;
        item.id = cellAt(row, columns.id);
        item.name = cellAt(row, columns.name);
        item.category = cellAt(row, columns.category);
        item.description = cellAt(row, columns.description);
        item.damageType = cellAt(row, columns.damageType);
        item.tags = parseTags(cellAt(row, columns.tags));
        item.effectText = cellAt(row, columns.effectText);

        if (item.id.empty()) {
            addValidationIssue(
                catalog,
                DbValidationSeverity::Error,
                DbValidationCategory::ObjectField,
                rowError(rowIndex, "ID is empty; row skipped"));
            continue;
        }
        if (!ids.insert(item.id).second) {
            addValidationIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::DuplicateId,
                "Objects row " + std::to_string(rowIndex + 1) +
                    " item=\"" + item.id + "\": duplicate ID; first map entry is kept");
        }
        if (item.name.empty()) {
            addValidationIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                rowError(rowIndex, "name is empty"));
        }
        if (!isAllowed(item.category, AllowedCategories)) {
            addValidationIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                rowError(rowIndex, "category is not allowed: " + item.category));
        }
        item.rarity = parseIntColumnOrDefault(cellAt(row, columns.rarity), 1, "rarity", rowIndex, item.id, catalog, 1, 10);
        item.price = parseIntColumnOrDefault(cellAt(row, columns.price), 0, "price", rowIndex, item.id, catalog, 0, 2'147'483'647);
        item.attackPower = parseIntColumnOrDefault(cellAt(row, columns.attackPower), 0, "attack power", rowIndex, item.id, catalog, 0, 2'147'483'647);
        const std::string rawDamageType = item.damageType;
        item.damageType = normalizeDamageType(item.damageType);
        if (item.damageType.empty()) {
            addValidationIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::ObjectField,
                rowError(rowIndex, "damage type is not allowed: " + rawDamageType + "; using none"));
            item.damageType = "none";
        } else if (isLegacyPhysicalDamageType(rawDamageType)) {
            addValidationIssue(
                catalog,
                DbValidationSeverity::Warning,
                DbValidationCategory::DeprecatedEntry,
                rowError(rowIndex, "damage type physical is deprecated; using blunt"));
        }
        item.digPower = parseIntColumnOrDefault(cellAt(row, columns.digPower), 0, "dig power", rowIndex, item.id, catalog, 0, 2'147'483'647);
        item.durability = parseIntColumnOrDefault(cellAt(row, columns.durability), 0, "durability", rowIndex, item.id, catalog, -1, 2'147'483'647);
        item.weightKg = parseDoubleColumnOrDefault(cellAt(row, columns.weight), 0.0, "weight", rowIndex, item.id, catalog, 0.0);
        if (columns.imageNumber >= 0) {
            item.imageNumber = parseIntColumnOrDefault(
                cellAt(row, columns.imageNumber),
                0,
                "image number",
                rowIndex,
                item.id,
                catalog,
                0,
                2'147'483'647);
        } else {
            item.imageNumber = 0;
        }
        if (columns.groundRotation >= 0) {
            item.visualRotation.groundDegrees = parseOptionalFloatColumnOrDefault(
                cellAt(row, columns.groundRotation),
                0.0f,
                "ground rotation",
                rowIndex,
                item.id,
                catalog);
        }
        if (columns.ringRotation >= 0) {
            item.visualRotation.ring = parseObjectRingRotation(
                cellAt(row, columns.ringRotation),
                rowIndex,
                item.id,
                catalog);
        }

        for (const auto& [columnName, columnIndex] : columns.lootWeightColumns) {
            const double weight = parseLootWeightOrZero(cellAt(row, columnIndex), columnName, rowIndex, item.id, catalog);
            if (weight > 0.0) {
                item.lootWeights.byColumn[columnName] = weight;
            }
        }
        if (item.lootWeights.hasPositiveWeight()) {
            ++catalog.lootWeightStats.weightedItemCount;
        }

        std::string effectError;
        std::vector<std::string> effectWarnings;
        if (!parseEffectSpecs(cellAt(row, columns.useEffects), item.normalEffects, effectError, &effectWarnings)) {
            addValidationIssue(
                catalog,
                DbValidationSeverity::Error,
                DbValidationCategory::EffectSpecSyntax,
                rowError(rowIndex, "normal effects are invalid: " + effectError + "; using empty effects"));
            item.normalEffects.clear();
        }
        appendRowIssues(catalog, rowIndex, item.id, "normal effects", DbValidationCategory::EffectSpecSyntax, effectWarnings);

        effectWarnings.clear();
        if (!parseEffectSpecs(cellAt(row, columns.orbitEffects), item.orbitEffects, effectError, &effectWarnings)) {
            addValidationIssue(
                catalog,
                DbValidationSeverity::Error,
                DbValidationCategory::EffectSpecSyntax,
                rowError(rowIndex, "orbit effects are invalid: " + effectError + "; using empty effects"));
            item.orbitEffects.clear();
        }
        appendRowIssues(catalog, rowIndex, item.id, "orbit effects", DbValidationCategory::EffectSpecSyntax, effectWarnings);

        std::vector<std::string> discoveryWarnings;
        item.discoveryEffectLines = buildDiscoveryEffectLines(item, &discoveryWarnings);
        appendRowIssues(catalog, rowIndex, item.id, "discovery effect lines", DbValidationCategory::DeprecatedEntry, discoveryWarnings);

        catalog.objects.push_back(std::move(item));
    }

    for (const ObjectDefinition& object : catalog.objects) {
        catalog.objectsById.emplace(object.id, object);
    }
    catalog.registry.rebuild(catalog.objects);
    outCatalog = std::move(catalog);
    outError.clear();
    return true;
}

bool containsValue(const std::vector<std::string>& values, std::string_view value)
{
    return std::any_of(values.begin(), values.end(), [value](const std::string& candidate) {
        return candidate == value;
    });
}

bool targetAllowedByDefinition(const EffectCodeDefinition& definition, std::string_view target)
{
    if (definition.targets.empty() || containsValue(definition.targets, target)) {
        return true;
    }

    return target == "ground" && containsValue(definition.targets, "terrain");
}

void validateEffectReferencesForField(
    const ItemData& item,
    std::string_view fieldName,
    const std::vector<EffectSpec>& specs,
    const std::unordered_map<std::string, EffectCodeDefinition>& effectCodes,
    std::vector<std::string>& outWarnings)
{
    for (const EffectSpec& spec : specs) {
        for (const std::string& effect : spec.effects) {
            if (const DeprecatedName* deprecated = findDeprecated(effect, DeprecatedEffectCodes)) {
                outWarnings.push_back("item=\"" + item.id + "\" " + std::string(fieldName) +
                    ": deprecated effect code \"" + effect + "\"; migrate to \"" +
                    std::string(deprecated->replacement) + "\" (" + std::string(deprecated->note) + ")");
            }

            const auto effectIt = effectCodes.find(effect);
            if (effectIt == effectCodes.end()) {
                outWarnings.push_back("item=\"" + item.id + "\" " + std::string(fieldName) +
                    ": undefined effect code \"" + effect + "\" for target \"" + spec.target + "\"");
                continue;
            }

            const EffectCodeDefinition& definition = effectIt->second;
            if (!targetAllowedByDefinition(definition, spec.target)) {
                outWarnings.push_back("item=\"" + item.id + "\" " + std::string(fieldName) +
                    ": target \"" + spec.target + "\" is not allowed for effect \"" + effect + "\"");
            } else if (spec.target == "ground" && containsValue(definition.targets, "terrain")) {
                outWarnings.push_back("item=\"" + item.id + "\" " + std::string(fieldName) +
                    ": target \"ground\" is accepted as terrain compatibility for effect \"" + effect + "\"");
            }
        }
    }
}

void validateObjectEffectReferences(ObjectCatalog& catalog)
{
    for (const ObjectDefinition& item : catalog.objects) {
        std::vector<std::string> warnings;
        validateEffectReferencesForField(item, "normal effects", item.normalEffects, catalog.effectCodes, warnings);
        for (const std::string& warning : warnings) {
            const DbValidationCategory category =
                warning.find("undefined effect code") != std::string::npos
                    ? DbValidationCategory::UndefinedEffectCode
                    : warning.find("deprecated effect code") != std::string::npos ||
                            warning.find("terrain compatibility") != std::string::npos
                        ? DbValidationCategory::DeprecatedEntry
                    : DbValidationCategory::TargetMismatch;
            addValidationIssue(catalog, DbValidationSeverity::Warning, category, warning);
        }

        warnings.clear();
        validateEffectReferencesForField(item, "orbit effects", item.orbitEffects, catalog.effectCodes, warnings);
        for (const std::string& warning : warnings) {
            const DbValidationCategory category =
                warning.find("undefined effect code") != std::string::npos
                    ? DbValidationCategory::UndefinedEffectCode
                    : warning.find("deprecated effect code") != std::string::npos ||
                            warning.find("terrain compatibility") != std::string::npos
                        ? DbValidationCategory::DeprecatedEntry
                    : DbValidationCategory::TargetMismatch;
            addValidationIssue(catalog, DbValidationSeverity::Warning, category, warning);
        }
    }
}

void validateObjectTags(ObjectCatalog& catalog)
{
    for (const ObjectDefinition& item : catalog.objects) {
        std::unordered_map<std::string, std::string> exclusiveGroups;
        for (const std::string& tag : item.tags) {
            if (const DeprecatedName* deprecated = findDeprecated(tag, DeprecatedSpecialTags)) {
                addValidationIssue(
                    catalog,
                    DbValidationSeverity::Warning,
                    DbValidationCategory::DeprecatedEntry,
                    "item=\"" + item.id + "\" tags: deprecated special tag \"" + tag +
                        "\"; migrate to \"" + std::string(deprecated->replacement) + "\" (" +
                        std::string(deprecated->note) + ")");
            }

            const auto tagIt = catalog.specialTags.find(tag);
            if (tagIt == catalog.specialTags.end()) {
                addValidationIssue(
                    catalog,
                    DbValidationSeverity::Warning,
                    DbValidationCategory::UndefinedSpecialTag,
                    "item=\"" + item.id + "\" tags: undefined special tag \"" + tag + "\"");
            } else {
                const std::string& group = tagIt->second.exclusiveGroup;
                if (!group.empty()) {
                    const auto groupIt = exclusiveGroups.find(group);
                    if (groupIt != exclusiveGroups.end()) {
                        addValidationIssue(
                            catalog,
                            DbValidationSeverity::Warning,
                            DbValidationCategory::ExclusiveTagConflict,
                            "item=\"" + item.id + "\" tags: exclusive group \"" + group + "\" has both \"" +
                                groupIt->second + "\" and \"" + tag + "\"");
                    } else {
                        exclusiveGroups.emplace(group, tag);
                    }
                }
            }

            if (catalog.effectCodes.find(tag) != catalog.effectCodes.end()) {
                addValidationIssue(
                    catalog,
                    DbValidationSeverity::Warning,
                    DbValidationCategory::TagEffectCodeNameCollision,
                    "item=\"" + item.id + "\" tags: special tag \"" + tag + "\" has the same name as an effect code");
            }
        }
    }
}

bool loadObjectCatalogFromGoogleSheet(const GoogleSheetSourceConfig& config, ObjectCatalog& outCatalog, std::string& outError)
{
    GoogleSheetTable table;
    if (!loadGoogleSheetTableForSheet(config, config.objectsSheet, table, outError)) {
        return false;
    }
    ObjectCatalog catalog;
    if (!parseObjectCatalog(table, catalog, outError)) {
        outCatalog = {};
        return false;
    }

    const auto loadEffectCodes = [&config](std::unordered_map<std::string, EffectCodeDefinition>& definitions, std::string& error) {
        std::string lastError;
        for (std::string_view sheetName : {EffectCodesSheetName, LegacyEffectCodesSheetName}) {
            GoogleSheetTable effectCodeTable;
            std::string sheetError;
            if (!loadGoogleSheetTableForSheet(config, sheetName, effectCodeTable, sheetError)) {
                lastError = "failed to load sheet " + std::string(sheetName) + ": " + sheetError;
                continue;
            }
            if (parseEffectCodeDefinitions(effectCodeTable, definitions, sheetError)) {
                error.clear();
                return true;
            }
            lastError = "failed to parse sheet " + std::string(sheetName) + ": " + sheetError;
        }
        error = lastError;
        return false;
    };

    if (!loadEffectCodes(catalog.effectCodes, outError)) {
        outError = "failed to load Effect code sheet: " + outError;
        outCatalog = {};
        return false;
    }

    const auto loadSpecialTags = [&config](std::unordered_map<std::string, SpecialTagDefinition>& definitions, std::string& error) {
        std::string lastError;
        for (std::string_view sheetName : {SpecialTagsSheetName, LegacySpecialTagsSheetName}) {
            GoogleSheetTable specialTagTable;
            std::string sheetError;
            if (!loadGoogleSheetTableForSheet(config, sheetName, specialTagTable, sheetError)) {
                lastError = "failed to load sheet " + std::string(sheetName) + ": " + sheetError;
                continue;
            }
            if (parseSpecialTagDefinitions(specialTagTable, definitions, sheetError)) {
                error.clear();
                return true;
            }
            lastError = "failed to parse sheet " + std::string(sheetName) + ": " + sheetError;
        }
        error = lastError;
        return false;
    };

    if (!loadSpecialTags(catalog.specialTags, outError)) {
        outError = "failed to load Special tag sheet: " + outError;
        outCatalog = {};
        return false;
    }

    validateObjectEffectReferences(catalog);
    validateObjectTags(catalog);

    outCatalog = std::move(catalog);
    outError.clear();
    return true;
}

std::string_view dbValidationSeverityName(DbValidationSeverity severity)
{
    switch (severity) {
    case DbValidationSeverity::Warning:
        return "warning";
    case DbValidationSeverity::Error:
        return "error";
    }
    return "unknown";
}

std::string_view dbValidationCategoryName(DbValidationCategory category)
{
    switch (category) {
    case DbValidationCategory::UndefinedEffectCode:
        return "undefined_effect_code";
    case DbValidationCategory::UndefinedSpecialTag:
        return "undefined_special_tag";
    case DbValidationCategory::TargetMismatch:
        return "target_mismatch";
    case DbValidationCategory::EffectSpecSyntax:
        return "effect_spec_syntax";
    case DbValidationCategory::ExclusiveTagConflict:
        return "exclusive_tag_conflict";
    case DbValidationCategory::DuplicateId:
        return "duplicate_id";
    case DbValidationCategory::NumericValue:
        return "numeric_value";
    case DbValidationCategory::ObjectField:
        return "object_field";
    case DbValidationCategory::TagEffectCodeNameCollision:
        return "tag_effect_code_name_collision";
    case DbValidationCategory::DeprecatedEntry:
        return "deprecated_entry";
    }
    return "unknown";
}

}
