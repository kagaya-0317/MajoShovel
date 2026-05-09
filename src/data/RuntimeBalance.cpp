#include "data/RuntimeBalance.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

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

bool parseFloat(std::string_view text, float& value)
{
    const std::string copy = trim(text);
    const char* begin = copy.data();
    const char* end = copy.data() + copy.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseInt(std::string_view text, int& value)
{
    const std::string copy = trim(text);
    const char* begin = copy.data();
    const char* end = copy.data() + copy.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool applyRuntimeBalanceValues(const std::unordered_map<std::string, std::string>& values, RuntimeBalance& outBalance, std::string& outError)
{
    RuntimeBalance loaded;

    auto setFloat = [&](const char* key, float& target) {
        auto it = values.find(key);
        if (it == values.end()) {
            return true;
        }
        float parsed = target;
        if (!parseFloat(it->second, parsed)) {
            outError = std::string("invalid float for ") + key;
            return false;
        }
        target = parsed;
        return true;
    };
    auto setFloatAlias = [&](const char* primaryKey, const char* legacyKey, float& target) {
        if (values.find(primaryKey) != values.end()) {
            return setFloat(primaryKey, target);
        }
        return setFloat(legacyKey, target);
    };
    auto setInt = [&](const char* key, int& target) {
        auto it = values.find(key);
        if (it == values.end()) {
            return true;
        }
        int parsed = target;
        if (!parseInt(it->second, parsed)) {
            outError = std::string("invalid int for ") + key;
            return false;
        }
        target = parsed;
        return true;
    };

    if (!setFloat("player_speed", loaded.playerSpeed)) return false;
    if (!setFloat("player_radius", loaded.playerRadius)) return false;
    if (!setFloat("player_light_radius", loaded.playerLightRadius)) return false;
    if (!setFloat("light_radius", loaded.lightRadius)) return false;
    if (!setFloatAlias("spell_ring_radius", "orbit_radius", loaded.spellRingRadius)) return false;
    if (!setFloatAlias("spell_ring_speed", "orbit_speed", loaded.spellRingSpeed)) return false;
    if (!setFloatAlias("spell_ring_shift_distance", "orbit_shift_distance", loaded.spellRingShiftDistance)) return false;
    if (!setFloatAlias("spell_ring_throw_cooldown", "orbit_throw_cooldown", loaded.spellRingThrowCooldown)) return false;
    if (!setFloatAlias("spell_ring_throw_speed", "orbit_throw_speed", loaded.spellRingThrowSpeed)) return false;
    if (!setFloatAlias("spell_ring_throw_distance", "orbit_throw_distance", loaded.spellRingThrowDistance)) return false;
    if (!setFloatAlias("spell_ring_throw_max_time", "orbit_throw_max_time", loaded.spellRingThrowMaxTime)) return false;
    if (!setFloatAlias("spell_ring_return_speed", "orbit_return_speed", loaded.spellRingReturnSpeed)) return false;
    if (!setInt("dirt_hp", loaded.dirtHp)) return false;
    if (!setInt("rock_hp", loaded.rockHp)) return false;
    if (!setInt("ore_hp", loaded.oreHp)) return false;
    if (!setFloat("enemy_speed", loaded.enemySpeed)) return false;
    if (!setFloat("enemy_radius", loaded.enemyRadius)) return false;
    if (!setInt("enemy_hp", loaded.enemyHp)) return false;
    if (!setInt("enemy_xp", loaded.enemyXp)) return false;
    if (!setInt("enemy_dug_spawn_every", loaded.enemyDugSpawnEvery)) return false;
    if (!setInt("enemy_soft_cap", loaded.enemySoftCap)) return false;
    if (!setFloat("enemy_min_spawn_distance", loaded.enemyMinSpawnDistance)) return false;
    if (!setFloat("enemy_spawn_warmup", loaded.enemySpawnWarmup)) return false;
    if (!setFloat("enemy_separation_strength", loaded.enemySeparationStrength)) return false;
    if (!setInt("xp_base", loaded.xpBase)) return false;
    if (!setInt("xp_per_level", loaded.xpPerLevel)) return false;
    if (!setInt("world_drop_limit_per_stage", loaded.worldDropLimitPerStage)) return false;
    if (!setFloat("loot_money_chance", loaded.lootMoneyChance)) return false;
    if (!setFloat("loot_material_chance", loaded.lootMaterialChance)) return false;
    if (!setFloat("loot_stage_multiplier_ST", loaded.lootStageMultiplierST)) return false;
    if (!setFloat("loot_stage_multiplier_JK", loaded.lootStageMultiplierJK)) return false;
    if (!setFloat("loot_stage_multiplier_SC", loaded.lootStageMultiplierSC)) return false;
    if (!setFloat("loot_stage_multiplier_AS", loaded.lootStageMultiplierAS)) return false;
    if (!setFloat("loot_depth_multiplier_1", loaded.lootDepthMultiplier[0])) return false;
    if (!setFloat("loot_depth_multiplier_2", loaded.lootDepthMultiplier[1])) return false;
    if (!setFloat("loot_depth_multiplier_3", loaded.lootDepthMultiplier[2])) return false;
    if (!setFloat("loot_depth_multiplier_AS_1", loaded.lootDepthMultiplierAS[0])) return false;
    if (!setFloat("loot_depth_multiplier_AS_2", loaded.lootDepthMultiplierAS[1])) return false;
    if (!setFloat("loot_depth_multiplier_AS_3", loaded.lootDepthMultiplierAS[2])) return false;
    if (!setFloat("loot_depth_multiplier_AS_4", loaded.lootDepthMultiplierAS[3])) return false;
    if (!setFloat("loot_depth_multiplier_AS_5", loaded.lootDepthMultiplierAS[4])) return false;
    if (!setFloat("loot_depth_multiplier_AS_6", loaded.lootDepthMultiplierAS[5])) return false;
    if (!setFloat("loot_depth_multiplier_AS_7", loaded.lootDepthMultiplierAS[6])) return false;
    if (!setFloat("loot_depth_multiplier_AS_8", loaded.lootDepthMultiplierAS[7])) return false;
    if (!setFloat("loot_depth_multiplier_AS_9", loaded.lootDepthMultiplierAS[8])) return false;
    if (!setFloat("loot_grade_multiplier_C", loaded.lootGradeMultiplierC)) return false;
    if (!setFloat("loot_grade_multiplier_R", loaded.lootGradeMultiplierR)) return false;
    if (!setFloat("loot_grade_multiplier_S", loaded.lootGradeMultiplierS)) return false;
    if (!setFloat("crate_money_chance", loaded.crateMoneyChance)) return false;
    if (!setFloat("crate_bonus_chance", loaded.crateBonusChance)) return false;
    if (!setFloat("enemy_mana_drop_chance", loaded.enemyManaDropChance)) return false;
    if (!setFloat("enemy_moon_fragment_chance", loaded.enemyMoonFragmentChance)) return false;
    if (!setFloat("boss_mana_drop_chance", loaded.bossManaDropChance)) return false;
    if (!setFloat("boss_moon_fragment_chance", loaded.bossMoonFragmentChance)) return false;
    if (!setInt("ore_material_min", loaded.oreMaterialMin)) return false;
    if (!setInt("ore_material_max", loaded.oreMaterialMax)) return false;

    loaded.playerSpeed = std::max(1.0f, loaded.playerSpeed);
    loaded.playerRadius = std::max(1.0f, loaded.playerRadius);
    loaded.playerLightRadius = std::max(0.0f, loaded.playerLightRadius);
    loaded.lightRadius = std::max(16.0f, loaded.lightRadius);
    loaded.spellRingRadius = std::max(1.0f, loaded.spellRingRadius);
    loaded.spellRingThrowCooldown = std::max(0.1f, loaded.spellRingThrowCooldown);
    loaded.dirtHp = std::max(1, loaded.dirtHp);
    loaded.rockHp = std::max(1, loaded.rockHp);
    loaded.oreHp = std::max(1, loaded.oreHp);
    loaded.enemyRadius = std::max(1.0f, loaded.enemyRadius);
    loaded.enemyHp = std::max(1, loaded.enemyHp);
    loaded.enemyXp = std::max(0, loaded.enemyXp);
    loaded.enemyDugSpawnEvery = std::max(1, loaded.enemyDugSpawnEvery);
    loaded.enemySoftCap = std::max(1, loaded.enemySoftCap);
    loaded.enemyMinSpawnDistance = std::max(0.0f, loaded.enemyMinSpawnDistance);
    loaded.enemySpawnWarmup = std::max(0.0f, loaded.enemySpawnWarmup);
    loaded.enemySeparationStrength = std::max(0.0f, loaded.enemySeparationStrength);
    loaded.xpBase = std::max(1, loaded.xpBase);
    loaded.xpPerLevel = std::max(0, loaded.xpPerLevel);
    loaded.worldDropLimitPerStage = std::max(0, loaded.worldDropLimitPerStage);
    loaded.lootMoneyChance = std::clamp(loaded.lootMoneyChance, 0.0f, 1.0f);
    loaded.lootMaterialChance = std::clamp(loaded.lootMaterialChance, 0.0f, 1.0f);
    loaded.lootStageMultiplierST = std::max(0.0f, loaded.lootStageMultiplierST);
    loaded.lootStageMultiplierJK = std::max(0.0f, loaded.lootStageMultiplierJK);
    loaded.lootStageMultiplierSC = std::max(0.0f, loaded.lootStageMultiplierSC);
    loaded.lootStageMultiplierAS = std::max(0.0f, loaded.lootStageMultiplierAS);
    for (float& value : loaded.lootDepthMultiplier) {
        value = std::max(0.0f, value);
    }
    for (float& value : loaded.lootDepthMultiplierAS) {
        value = std::max(0.0f, value);
    }
    loaded.lootGradeMultiplierC = std::max(0.0f, loaded.lootGradeMultiplierC);
    loaded.lootGradeMultiplierR = std::max(0.0f, loaded.lootGradeMultiplierR);
    loaded.lootGradeMultiplierS = std::max(0.0f, loaded.lootGradeMultiplierS);
    loaded.crateMoneyChance = std::clamp(loaded.crateMoneyChance, 0.0f, 1.0f);
    loaded.crateBonusChance = std::clamp(loaded.crateBonusChance, 0.0f, 1.0f);
    loaded.enemyManaDropChance = std::clamp(loaded.enemyManaDropChance, 0.0f, 1.0f);
    loaded.enemyMoonFragmentChance = std::clamp(loaded.enemyMoonFragmentChance, 0.0f, 1.0f);
    loaded.bossManaDropChance = std::clamp(loaded.bossManaDropChance, 0.0f, 1.0f);
    loaded.bossMoonFragmentChance = std::clamp(loaded.bossMoonFragmentChance, 0.0f, 1.0f);
    loaded.oreMaterialMin = std::max(1, loaded.oreMaterialMin);
    loaded.oreMaterialMax = std::max(loaded.oreMaterialMin, loaded.oreMaterialMax);

    outBalance = loaded;
    outError.clear();
    return true;
}

std::unordered_map<std::string, std::string> parseCsvRows(std::string_view csv)
{
    std::unordered_map<std::string, std::string> values;
    std::vector<std::string> row;
    std::string cell;
    bool inQuotes = false;

    auto flushRow = [&]() {
        if (cell.ends_with('\r')) {
            cell.pop_back();
        }
        row.push_back(cell);
        cell.clear();

        bool hasContent = false;
        for (const std::string& value : row) {
            if (!trim(value).empty()) {
                hasContent = true;
                break;
            }
        }
        if (!hasContent) {
            row.clear();
            return;
        }

        if (row.size() >= 2) {
            const std::string key = trim(row[0]);
            const std::string value = trim(row[1]);
            if (!key.empty() && !(key == "key" && value == "value")) {
                values[key] = value;
            }
        } else if (row.size() == 1) {
            const std::string line = trim(row[0]);
            const std::size_t equals = line.find('=');
            if (equals != std::string::npos) {
                values[trim(std::string_view(line).substr(0, equals))] = trim(std::string_view(line).substr(equals + 1));
            }
        }
        row.clear();
    };

    for (std::size_t i = 0; i < csv.size(); ++i) {
        const char ch = csv[i];
        if (inQuotes) {
            if (ch == '"' && i + 1 < csv.size() && csv[i + 1] == '"') {
                cell.push_back('"');
                ++i;
            } else if (ch == '"') {
                inQuotes = false;
            } else {
                cell.push_back(ch);
            }
            continue;
        }

        if (ch == '"') {
            inQuotes = true;
        } else if (ch == ',') {
            if (cell.ends_with('\r')) {
                cell.pop_back();
            }
            row.push_back(cell);
            cell.clear();
        } else if (ch == '\n') {
            flushRow();
        } else {
            cell.push_back(ch);
        }
    }

    if (!cell.empty() || !row.empty()) {
        flushRow();
    }
    return values;
}

}

bool loadRuntimeBalanceFromText(std::string_view text, RuntimeBalance& outBalance, std::string& outError)
{
    std::unordered_map<std::string, std::string> values;
    std::istringstream stream{std::string(text)};
    std::string line;
    int lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            outError = "invalid balance line " + std::to_string(lineNumber);
            return false;
        }
        values[trim(std::string_view(line).substr(0, equals))] = trim(std::string_view(line).substr(equals + 1));
    }

    return applyRuntimeBalanceValues(values, outBalance, outError);
}

bool loadRuntimeBalanceFromCsv(std::string_view csv, RuntimeBalance& outBalance, std::string& outError)
{
    const std::unordered_map<std::string, std::string> values = parseCsvRows(csv);
    if (values.empty()) {
        outError = "sheet has no balance values";
        return false;
    }

    return applyRuntimeBalanceValues(values, outBalance, outError);
}

bool loadRuntimeBalance(const std::filesystem::path& path, RuntimeBalance& outBalance, std::string& outError)
{
    RuntimeBalance loaded;
    std::ifstream file(path);
    if (!file) {
        outBalance = loaded;
        outError = "balance file not found: " + path.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return loadRuntimeBalanceFromText(buffer.str(), outBalance, outError);
}

}
