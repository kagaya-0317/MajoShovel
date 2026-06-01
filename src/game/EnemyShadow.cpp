#include "game/EnemyShadow.hpp"

#include "game/ActorVisual.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <vector>

namespace majo {

namespace {

constexpr std::string_view EnemyShadowHeader = "MAJO_ENEMY_SHADOW_V1";
constexpr float EnemyShadowOffsetMax = 512.0f;
constexpr float EnemyShadowScaleMin = 0.05f;
constexpr float EnemyShadowScaleMax = 4.0f;

std::string trimAscii(std::string_view text)
{
    std::size_t start = 0;
    while (start < text.size() && static_cast<unsigned char>(text[start]) <= ' ') {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && static_cast<unsigned char>(text[end - 1]) <= ' ') {
        --end;
    }
    return std::string(text.substr(start, end - start));
}

std::vector<std::string> splitAsciiWords(std::string_view text)
{
    std::vector<std::string> words;
    std::size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && static_cast<unsigned char>(text[pos]) <= ' ') {
            ++pos;
        }
        const std::size_t start = pos;
        while (pos < text.size() && static_cast<unsigned char>(text[pos]) > ' ') {
            ++pos;
        }
        if (start < pos) {
            words.emplace_back(text.substr(start, pos - start));
        }
    }
    return words;
}

bool parseFloatToken(const std::string& token, float& outValue)
{
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    float value = 0.0f;
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(value)) {
        return false;
    }
    outValue = value;
    return true;
}

float sanitizedFinite(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

std::string formatEnemyShadowFloat(float value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    std::string text = stream.str();
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    if (text == "-0") {
        return "0";
    }
    return text;
}

bool specIsDefault(const EnemyShadowSpec& spec)
{
    return spec == defaultEnemyShadowSpec();
}

}

EnemyShadowSpec sanitizeEnemyShadowSpec(EnemyShadowSpec spec)
{
    spec.offset.x = std::clamp(sanitizedFinite(spec.offset.x, 0.0f), -EnemyShadowOffsetMax, EnemyShadowOffsetMax);
    spec.offset.y = std::clamp(sanitizedFinite(spec.offset.y, EnemyShadowGroundOffsetY), -EnemyShadowOffsetMax, EnemyShadowOffsetMax);
    spec.scale.x = std::clamp(sanitizedFinite(spec.scale.x, 1.0f), EnemyShadowScaleMin, EnemyShadowScaleMax);
    spec.scale.y = std::clamp(sanitizedFinite(spec.scale.y, 1.0f), EnemyShadowScaleMin, EnemyShadowScaleMax);
    return spec;
}

EnemyShadowSpec defaultEnemyShadowSpec()
{
    return sanitizeEnemyShadowSpec({.offset = {0.0f, EnemyShadowGroundOffsetY}, .scale = {1.0f, 1.0f}});
}

const EnemyShadowSpec* enemyShadowSpecFor(const EnemyShadowCatalog* catalog, std::string_view enemyId)
{
    if (catalog == nullptr || enemyId.empty()) {
        return nullptr;
    }
    const auto it = catalog->enemies.find(std::string(enemyId));
    return it != catalog->enemies.end() ? &it->second : nullptr;
}

EnemyShadowSpec resolvedEnemyShadowSpec(const EnemyShadowCatalog* catalog, std::string_view enemyId)
{
    if (const EnemyShadowSpec* spec = enemyShadowSpecFor(catalog, enemyId)) {
        return sanitizeEnemyShadowSpec(*spec);
    }
    return defaultEnemyShadowSpec();
}

bool eraseEnemyShadowSpec(EnemyShadowCatalog& catalog, std::string_view enemyId)
{
    return catalog.enemies.erase(std::string(enemyId)) > 0;
}

bool loadEnemyShadowCatalog(
    const std::filesystem::path& path,
    EnemyShadowCatalog& outCatalog,
    std::string& outMessage)
{
    EnemyShadowCatalog catalog;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outMessage = "Enemy shadow data not found";
        outCatalog = {};
        return false;
    }

    bool headerRead = false;
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (lineNumber == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        line = trimAscii(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!headerRead) {
            if (line != EnemyShadowHeader) {
                outMessage = "Enemy shadow load failed: unsupported header";
                outCatalog = {};
                return false;
            }
            headerRead = true;
            continue;
        }

        const std::vector<std::string> words = splitAsciiWords(line);
        if (words.size() != 8 ||
            words[0] != "enemy" ||
            words[2] != "offset" ||
            words[5] != "scale") {
            outMessage = "Enemy shadow load failed at line " + std::to_string(lineNumber);
            outCatalog = {};
            return false;
        }

        EnemyShadowSpec spec;
        if (!parseFloatToken(words[3], spec.offset.x) ||
            !parseFloatToken(words[4], spec.offset.y) ||
            !parseFloatToken(words[6], spec.scale.x) ||
            !parseFloatToken(words[7], spec.scale.y)) {
            outMessage = "Enemy shadow load failed at line " + std::to_string(lineNumber);
            outCatalog = {};
            return false;
        }

        spec = sanitizeEnemyShadowSpec(spec);
        if (!words[1].empty() && !specIsDefault(spec)) {
            catalog.enemies[words[1]] = spec;
        }
    }

    if (!headerRead) {
        outMessage = "Enemy shadow load failed: empty file";
        outCatalog = {};
        return false;
    }

    outCatalog = std::move(catalog);
    outMessage = "Enemy shadow data loaded";
    return true;
}

bool saveEnemyShadowCatalog(
    const std::filesystem::path& path,
    const EnemyShadowCatalog& catalog,
    std::string& outMessage)
{
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            outMessage = "Enemy shadow save failed: could not create data directory";
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        outMessage = "Enemy shadow save failed: could not open " + path.string();
        return false;
    }

    std::vector<std::pair<std::string, EnemyShadowSpec>> entries;
    entries.reserve(catalog.enemies.size());
    for (const auto& [id, spec] : catalog.enemies) {
        const EnemyShadowSpec sanitized = sanitizeEnemyShadowSpec(spec);
        if (!id.empty() && !specIsDefault(sanitized)) {
            entries.emplace_back(id, sanitized);
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    file << "\xEF\xBB\xBF" << EnemyShadowHeader << "\n";
    for (const auto& [id, spec] : entries) {
        file << "enemy " << id << " offset "
            << formatEnemyShadowFloat(spec.offset.x) << " "
            << formatEnemyShadowFloat(spec.offset.y) << " scale "
            << formatEnemyShadowFloat(spec.scale.x) << " "
            << formatEnemyShadowFloat(spec.scale.y) << "\n";
    }

    if (!file) {
        outMessage = "Enemy shadow save failed while writing " + path.string();
        return false;
    }

    outMessage = "Enemy shadow saved";
    return true;
}

}
