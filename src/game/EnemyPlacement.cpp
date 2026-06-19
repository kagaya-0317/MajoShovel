#include "game/EnemyPlacement.hpp"

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

constexpr std::string_view EnemyPlacementHeader = "MAJO_ENEMY_PLACEMENT_V1";
constexpr float EnemyPlacementRadiusMin = 1.0f;
constexpr float EnemyPlacementRadiusMax = 512.0f;
constexpr float EnemyPlacementOffsetMax = 512.0f;

void stripUtf8Bom(std::string& text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

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

bool parsePlacementDirection(std::string_view token, HitboxDirection& outDirection)
{
    if (token == "default") {
        outDirection = HitboxDirection::Default;
        return true;
    }
    if (token == "down") {
        outDirection = HitboxDirection::Down;
        return true;
    }
    if (token == "left") {
        outDirection = HitboxDirection::Left;
        return true;
    }
    if (token == "right") {
        outDirection = HitboxDirection::Right;
        return true;
    }
    if (token == "up") {
        outDirection = HitboxDirection::Up;
        return true;
    }
    return false;
}

std::string formatPlacementFloat(float value)
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

bool offsetIsDefault(Vec2 offset)
{
    return std::abs(offset.x) <= 0.0001f && std::abs(offset.y) <= 0.0001f;
}

} // namespace

float sanitizeEnemyPlacementRadius(float radius)
{
    return std::clamp(sanitizedFinite(radius, 10.0f), EnemyPlacementRadiusMin, EnemyPlacementRadiusMax);
}

Vec2 sanitizeEnemyPlacementOffset(Vec2 offset)
{
    offset.x = std::clamp(sanitizedFinite(offset.x, 0.0f), -EnemyPlacementOffsetMax, EnemyPlacementOffsetMax);
    offset.y = std::clamp(sanitizedFinite(offset.y, 0.0f), -EnemyPlacementOffsetMax, EnemyPlacementOffsetMax);
    return offset;
}

EnemyPlacementEntry sanitizeEnemyPlacementEntry(EnemyPlacementEntry entry)
{
    if (entry.passageRadius) {
        entry.passageRadius = sanitizeEnemyPlacementRadius(*entry.passageRadius);
    }
    for (std::optional<Vec2>& offset : entry.visualOffsets) {
        if (offset) {
            offset = sanitizeEnemyPlacementOffset(*offset);
        }
    }
    return entry;
}

bool enemyPlacementEntryHasAny(const EnemyPlacementEntry& entry)
{
    if (entry.passageRadius) {
        return true;
    }
    return std::any_of(entry.visualOffsets.begin(), entry.visualOffsets.end(), [](const std::optional<Vec2>& offset) {
        return offset.has_value() && !offsetIsDefault(*offset);
    });
}

const EnemyPlacementEntry* enemyPlacementEntryFor(
    const EnemyPlacementCatalog* catalog,
    std::string_view enemyId)
{
    if (catalog == nullptr || enemyId.empty()) {
        return nullptr;
    }
    const auto it = catalog->enemies.find(std::string(enemyId));
    return it != catalog->enemies.end() ? &it->second : nullptr;
}

bool enemyPlacementHasAny(
    const EnemyPlacementCatalog& catalog,
    std::string_view enemyId)
{
    const EnemyPlacementEntry* entry = enemyPlacementEntryFor(&catalog, enemyId);
    return entry != nullptr && enemyPlacementEntryHasAny(*entry);
}

bool enemyPlacementHasPassageRadius(
    const EnemyPlacementCatalog& catalog,
    std::string_view enemyId)
{
    const EnemyPlacementEntry* entry = enemyPlacementEntryFor(&catalog, enemyId);
    return entry != nullptr && entry->passageRadius.has_value();
}

bool enemyPlacementHasVisualOffset(
    const EnemyPlacementCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    const EnemyPlacementEntry* entry = enemyPlacementEntryFor(&catalog, enemyId);
    if (entry == nullptr) {
        return false;
    }
    const std::optional<Vec2>& offset = entry->visualOffsets[static_cast<std::size_t>(hitboxDirectionIndex(direction))];
    return offset.has_value() && !offsetIsDefault(*offset);
}

std::optional<float> enemyPlacementPassageRadiusFor(
    const EnemyPlacementCatalog* catalog,
    std::string_view enemyId)
{
    const EnemyPlacementEntry* entry = enemyPlacementEntryFor(catalog, enemyId);
    if (entry == nullptr || !entry->passageRadius) {
        return std::nullopt;
    }
    return sanitizeEnemyPlacementRadius(*entry->passageRadius);
}

Vec2 resolvedEnemyVisualOffset(
    const EnemyPlacementCatalog* catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    const EnemyPlacementEntry* entry = enemyPlacementEntryFor(catalog, enemyId);
    if (entry == nullptr) {
        return {};
    }
    const std::optional<Vec2>& directional = entry->visualOffsets[static_cast<std::size_t>(hitboxDirectionIndex(direction))];
    if (directional) {
        return sanitizeEnemyPlacementOffset(*directional);
    }
    const std::optional<Vec2>& fallback = entry->visualOffsets[static_cast<std::size_t>(hitboxDirectionIndex(HitboxDirection::Default))];
    return fallback ? sanitizeEnemyPlacementOffset(*fallback) : Vec2{};
}

Vec2 resolvedEnemyVisualOffset(
    const EnemyPlacementCatalog* catalog,
    const Enemy& enemy)
{
    return resolvedEnemyVisualOffset(catalog, enemy.enemyId, enemyHitboxDirectionForFacing(enemy.facingAngle));
}

EnemyPlacementEntry& mutableEnemyPlacementEntry(
    EnemyPlacementCatalog& catalog,
    std::string_view enemyId)
{
    return catalog.enemies[std::string(enemyId)];
}

bool eraseEnemyPlacementEntry(
    EnemyPlacementCatalog& catalog,
    std::string_view enemyId)
{
    return catalog.enemies.erase(std::string(enemyId)) > 0;
}

bool eraseEnemyPlacementPassageRadius(
    EnemyPlacementCatalog& catalog,
    std::string_view enemyId)
{
    if (enemyId.empty()) {
        return false;
    }
    const auto it = catalog.enemies.find(std::string(enemyId));
    if (it == catalog.enemies.end()) {
        return false;
    }
    const bool hadRadius = it->second.passageRadius.has_value();
    it->second.passageRadius.reset();
    if (!enemyPlacementEntryHasAny(it->second)) {
        catalog.enemies.erase(it);
    }
    return hadRadius;
}

bool eraseEnemyPlacementVisualOffset(
    EnemyPlacementCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    if (enemyId.empty()) {
        return false;
    }
    const auto it = catalog.enemies.find(std::string(enemyId));
    if (it == catalog.enemies.end()) {
        return false;
    }
    std::optional<Vec2>& offset = it->second.visualOffsets[static_cast<std::size_t>(hitboxDirectionIndex(direction))];
    const bool hadOffset = offset.has_value();
    offset.reset();
    if (!enemyPlacementEntryHasAny(it->second)) {
        catalog.enemies.erase(it);
    }
    return hadOffset;
}

bool loadEnemyPlacementCatalog(
    const std::filesystem::path& path,
    EnemyPlacementCatalog& outCatalog,
    std::string& outMessage)
{
    EnemyPlacementCatalog catalog;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outMessage = "Enemy placement data not found";
        outCatalog = {};
        return false;
    }

    bool headerRead = false;
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (lineNumber == 1) {
            stripUtf8Bom(line);
        }
        line = trimAscii(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!headerRead) {
            if (line != EnemyPlacementHeader) {
                outMessage = "Enemy placement load failed: unsupported header";
                outCatalog = {};
                return false;
            }
            headerRead = true;
            continue;
        }

        const std::vector<std::string> words = splitAsciiWords(line);
        if (words.size() < 4 || words[0] != "enemy" || words[1].empty()) {
            outMessage = "Enemy placement load failed at line " + std::to_string(lineNumber);
            outCatalog = {};
            return false;
        }

        EnemyPlacementEntry& entry = catalog.enemies[words[1]];
        if (words.size() == 4 && words[2] == "radius") {
            float radius = 0.0f;
            if (!parseFloatToken(words[3], radius)) {
                outMessage = "Enemy placement load failed at line " + std::to_string(lineNumber);
                outCatalog = {};
                return false;
            }
            entry.passageRadius = sanitizeEnemyPlacementRadius(radius);
            continue;
        }

        HitboxDirection direction = HitboxDirection::Default;
        std::size_t offsetTokenIndex = 2;
        if (words[2] != "offset") {
            if (!parsePlacementDirection(words[2], direction)) {
                outMessage = "Enemy placement load failed at line " + std::to_string(lineNumber);
                outCatalog = {};
                return false;
            }
            offsetTokenIndex = 3;
        }
        if (words.size() != offsetTokenIndex + 3 || words[offsetTokenIndex] != "offset") {
            outMessage = "Enemy placement load failed at line " + std::to_string(lineNumber);
            outCatalog = {};
            return false;
        }

        Vec2 offset{};
        if (!parseFloatToken(words[offsetTokenIndex + 1], offset.x) ||
            !parseFloatToken(words[offsetTokenIndex + 2], offset.y)) {
            outMessage = "Enemy placement load failed at line " + std::to_string(lineNumber);
            outCatalog = {};
            return false;
        }
        entry.visualOffsets[static_cast<std::size_t>(hitboxDirectionIndex(direction))] = sanitizeEnemyPlacementOffset(offset);
    }

    if (!headerRead) {
        outMessage = "Enemy placement load failed: empty file";
        outCatalog = {};
        return false;
    }

    for (auto it = catalog.enemies.begin(); it != catalog.enemies.end();) {
        it->second = sanitizeEnemyPlacementEntry(it->second);
        if (it->first.empty() || !enemyPlacementEntryHasAny(it->second)) {
            it = catalog.enemies.erase(it);
        } else {
            ++it;
        }
    }

    outCatalog = std::move(catalog);
    outMessage = "Enemy placement data loaded";
    return true;
}

bool saveEnemyPlacementCatalog(
    const std::filesystem::path& path,
    const EnemyPlacementCatalog& catalog,
    std::string& outMessage)
{
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            outMessage = "Enemy placement save failed: could not create data directory";
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        outMessage = "Enemy placement save failed: could not open " + path.string();
        return false;
    }

    std::vector<std::pair<std::string, EnemyPlacementEntry>> entries;
    entries.reserve(catalog.enemies.size());
    for (const auto& [id, entry] : catalog.enemies) {
        EnemyPlacementEntry sanitized = sanitizeEnemyPlacementEntry(entry);
        if (!id.empty() && enemyPlacementEntryHasAny(sanitized)) {
            entries.emplace_back(id, std::move(sanitized));
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    file << "\xEF\xBB\xBF" << EnemyPlacementHeader << "\n";
    for (const auto& [id, entry] : entries) {
        if (entry.passageRadius) {
            file << "enemy " << id << " radius "
                << formatPlacementFloat(sanitizeEnemyPlacementRadius(*entry.passageRadius)) << "\n";
        }
        for (int i = 0; i < HitboxDirectionCount; ++i) {
            const HitboxDirection direction = static_cast<HitboxDirection>(i);
            const std::optional<Vec2>& offset = entry.visualOffsets[static_cast<std::size_t>(i)];
            if (!offset || offsetIsDefault(*offset)) {
                continue;
            }
            const Vec2 sanitized = sanitizeEnemyPlacementOffset(*offset);
            file << "enemy " << id << " ";
            if (direction != HitboxDirection::Default) {
                file << hitboxDirectionId(direction) << " ";
            }
            file << "offset "
                << formatPlacementFloat(sanitized.x) << " "
                << formatPlacementFloat(sanitized.y) << "\n";
        }
    }

    if (!file) {
        outMessage = "Enemy placement save failed while writing " + path.string();
        return false;
    }

    outMessage = "Enemy placement saved";
    return true;
}

}
