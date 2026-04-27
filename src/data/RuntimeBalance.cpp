#include "data/RuntimeBalance.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <string_view>
#include <system_error>
#include <unordered_map>

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

    std::unordered_map<std::string, std::string> values;
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
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
    if (!setFloat("orbit_radius", loaded.orbitRadius)) return false;
    if (!setFloat("orbit_speed", loaded.orbitSpeed)) return false;
    if (!setFloat("orbit_shift_distance", loaded.orbitShiftDistance)) return false;
    if (!setFloat("orbit_throw_cooldown", loaded.orbitThrowCooldown)) return false;
    if (!setFloat("orbit_throw_speed", loaded.orbitThrowSpeed)) return false;
    if (!setFloat("orbit_throw_distance", loaded.orbitThrowDistance)) return false;
    if (!setFloat("orbit_throw_max_time", loaded.orbitThrowMaxTime)) return false;
    if (!setFloat("orbit_return_speed", loaded.orbitReturnSpeed)) return false;
    if (!setInt("dirt_hp", loaded.dirtHp)) return false;
    if (!setInt("rock_hp", loaded.rockHp)) return false;
    if (!setInt("ore_hp", loaded.oreHp)) return false;
    if (!setFloat("enemy_speed", loaded.enemySpeed)) return false;
    if (!setFloat("enemy_radius", loaded.enemyRadius)) return false;
    if (!setInt("enemy_hp", loaded.enemyHp)) return false;
    if (!setInt("enemy_xp", loaded.enemyXp)) return false;
    if (!setInt("enemy_dug_spawn_every", loaded.enemyDugSpawnEvery)) return false;
    if (!setInt("enemy_soft_cap", loaded.enemySoftCap)) return false;
    if (!setInt("xp_base", loaded.xpBase)) return false;
    if (!setInt("xp_per_level", loaded.xpPerLevel)) return false;

    loaded.playerSpeed = std::max(1.0f, loaded.playerSpeed);
    loaded.playerRadius = std::max(1.0f, loaded.playerRadius);
    loaded.lightRadius = std::max(16.0f, loaded.lightRadius);
    loaded.orbitRadius = std::max(1.0f, loaded.orbitRadius);
    loaded.orbitThrowCooldown = std::max(0.1f, loaded.orbitThrowCooldown);
    loaded.dirtHp = std::max(1, loaded.dirtHp);
    loaded.rockHp = std::max(1, loaded.rockHp);
    loaded.oreHp = std::max(1, loaded.oreHp);
    loaded.enemyRadius = std::max(1.0f, loaded.enemyRadius);
    loaded.enemyHp = std::max(1, loaded.enemyHp);
    loaded.enemyXp = std::max(0, loaded.enemyXp);
    loaded.enemyDugSpawnEvery = std::max(1, loaded.enemyDugSpawnEvery);
    loaded.enemySoftCap = std::max(1, loaded.enemySoftCap);
    loaded.xpBase = std::max(1, loaded.xpBase);
    loaded.xpPerLevel = std::max(0, loaded.xpPerLevel);

    outBalance = loaded;
    outError.clear();
    return true;
}

}
