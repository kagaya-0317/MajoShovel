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

    loaded.playerSpeed = std::max(1.0f, loaded.playerSpeed);
    loaded.playerRadius = std::max(1.0f, loaded.playerRadius);
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
