#include "game/Hitbox.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <tuple>
#include <utility>

namespace majo {

namespace {

constexpr std::string_view HitboxHeader = "MAJO_HITBOX_V3";
constexpr std::string_view HitboxV2Header = "MAJO_HITBOX_V2";
constexpr std::string_view LegacyHitboxHeader = "MAJO_HITBOX_V1";
constexpr std::string_view LegacyEnemyHitboxHeader = "MAJO_ENEMY_HITBOX_V1";
constexpr float HitboxRadiusMin = 1.0f;
constexpr float HitboxRadiusMax = 512.0f;
constexpr float HitboxOffsetMax = 512.0f;

void stripUtf8Bom(std::string& text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
}

float sanitizedFinite(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

HitCircle sanitizeCircle(HitCircle circle)
{
    circle.offset.x = std::clamp(sanitizedFinite(circle.offset.x, 0.0f), -HitboxOffsetMax, HitboxOffsetMax);
    circle.offset.y = std::clamp(sanitizedFinite(circle.offset.y, 0.0f), -HitboxOffsetMax, HitboxOffsetMax);
    circle.radius = std::clamp(sanitizedFinite(circle.radius, 10.0f), HitboxRadiusMin, HitboxRadiusMax);
    return circle;
}

float sanitizedScale(float scale)
{
    return std::max(0.0f, sanitizedFinite(scale, 1.0f));
}

float sanitizedPadding(float padding)
{
    return std::max(0.0f, sanitizedFinite(padding, 0.0f));
}

float enemyHitboxScale(const Enemy& enemy)
{
    return std::max(0.0f, static_cast<float>(enemy.status.sizeMultiplierFromStates()));
}

Vec2 rotateOffset(Vec2 offset, float radians)
{
    if (std::abs(radians) <= 0.00001f) {
        return offset;
    }
    const float s = std::sin(radians);
    const float c = std::cos(radians);
    return {
        offset.x * c - offset.y * s,
        offset.x * s + offset.y * c,
    };
}

std::string formatHitboxFloat(float value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    std::string text = stream.str();
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text.empty() ? std::string("0") : text;
}

bool profileHasCircles(const HitboxProfile& profile)
{
    return !profile.circles.empty();
}

const HitboxProfile* profileFor(const std::unordered_map<std::string, HitboxProfile>& profiles, std::string_view id)
{
    if (id.empty()) {
        return nullptr;
    }

    const auto it = profiles.find(std::string(id));
    if (it == profiles.end() || !profileHasCircles(it->second)) {
        return nullptr;
    }
    return &it->second;
}

bool parseHitboxDirection(std::string_view token, HitboxDirection& outDirection)
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

const HitboxProfile* enemyProfileSlot(
    const std::unordered_map<std::string, EnemyHitboxProfiles>& enemies,
    std::string_view id,
    HitboxDirection direction)
{
    if (id.empty()) {
        return nullptr;
    }

    const auto it = enemies.find(std::string(id));
    if (it == enemies.end()) {
        return nullptr;
    }

    const HitboxProfile& profile = it->second.directions[static_cast<std::size_t>(hitboxDirectionIndex(direction))];
    return profileHasCircles(profile) ? &profile : nullptr;
}

void appendEntry(
    std::unordered_map<std::string, HitboxProfile>& profiles,
    const std::string& id,
    HitCircle circle)
{
    if (id.empty()) {
        return;
    }

    HitboxProfile& profile = profiles[id];
    if (static_cast<int>(profile.circles.size()) >= HitboxMaxCircles) {
        return;
    }
    profile.circles.push_back(sanitizeCircle(circle));
}

void appendProfileEntry(HitboxProfile& profile, HitCircle circle)
{
    if (static_cast<int>(profile.circles.size()) >= HitboxMaxCircles) {
        return;
    }
    profile.circles.push_back(sanitizeCircle(circle));
}

void appendEnemyEntry(
    std::unordered_map<std::string, EnemyHitboxProfiles>& profileMap,
    const std::string& id,
    HitboxDirection direction,
    HitCircle circle)
{
    if (id.empty()) {
        return;
    }

    HitboxProfile& profile = profileMap[id].directions[static_cast<std::size_t>(hitboxDirectionIndex(direction))];
    if (static_cast<int>(profile.circles.size()) >= HitboxMaxCircles) {
        return;
    }
    profile.circles.push_back(sanitizeCircle(circle));
}

void collectEntries(
    const std::unordered_map<std::string, HitboxProfile>& profiles,
    std::string_view kind,
    std::vector<std::tuple<std::string_view, std::string, const HitboxProfile*>>& entries)
{
    entries.reserve(entries.size() + profiles.size());
    for (const auto& [id, profile] : profiles) {
        if (!id.empty() && !profile.circles.empty()) {
            entries.push_back({kind, id, &profile});
        }
    }
}

struct EnemyHitboxSaveEntry {
    std::string id;
    HitboxDirection direction = HitboxDirection::Default;
    const HitboxProfile* profile = nullptr;
};

void collectEnemyEntries(
    const std::unordered_map<std::string, EnemyHitboxProfiles>& enemies,
    std::vector<EnemyHitboxSaveEntry>& entries)
{
    entries.reserve(entries.size() + enemies.size());
    for (const auto& [id, profileSet] : enemies) {
        if (id.empty()) {
            continue;
        }
        for (int i = 0; i < HitboxDirectionCount; ++i) {
            const HitboxProfile& profile = profileSet.directions[static_cast<std::size_t>(i)];
            if (!profileHasCircles(profile)) {
                continue;
            }
            entries.push_back({
                id,
                static_cast<HitboxDirection>(i),
                &profile,
            });
        }
    }
}

} // namespace

int hitboxDirectionIndex(HitboxDirection direction)
{
    switch (direction) {
    case HitboxDirection::Default: return 0;
    case HitboxDirection::Down: return 1;
    case HitboxDirection::Left: return 2;
    case HitboxDirection::Right: return 3;
    case HitboxDirection::Up: return 4;
    }
    return 0;
}

std::string_view hitboxDirectionId(HitboxDirection direction)
{
    switch (direction) {
    case HitboxDirection::Default: return "default";
    case HitboxDirection::Down: return "down";
    case HitboxDirection::Left: return "left";
    case HitboxDirection::Right: return "right";
    case HitboxDirection::Up: return "up";
    }
    return "default";
}

std::string_view hitboxDirectionDisplayName(HitboxDirection direction)
{
    switch (direction) {
    case HitboxDirection::Default: return "共通";
    case HitboxDirection::Down: return "下";
    case HitboxDirection::Left: return "左";
    case HitboxDirection::Right: return "右";
    case HitboxDirection::Up: return "上";
    }
    return "共通";
}

Vec2 hitboxDirectionVector(HitboxDirection direction)
{
    switch (direction) {
    case HitboxDirection::Left: return {-1.0f, 0.0f};
    case HitboxDirection::Right: return {1.0f, 0.0f};
    case HitboxDirection::Up: return {0.0f, -1.0f};
    case HitboxDirection::Default:
    case HitboxDirection::Down:
        return {0.0f, 1.0f};
    }
    return {0.0f, 1.0f};
}

HitboxDirection enemyHitboxDirectionForFacing(float facingAngle)
{
    const Vec2 direction{std::cos(facingAngle), std::sin(facingAngle)};
    if (std::abs(direction.x) > std::abs(direction.y)) {
        return direction.x >= 0.0f ? HitboxDirection::Right : HitboxDirection::Left;
    }
    return direction.y >= 0.0f ? HitboxDirection::Down : HitboxDirection::Up;
}

HitboxProfile singleCircleHitbox(float radius)
{
    HitboxProfile profile;
    profile.circles.push_back({{}, std::max(HitboxRadiusMin, sanitizedFinite(radius, 10.0f))});
    return profile;
}

const HitboxProfile* enemyHitboxProfileFor(const HitboxCatalog* catalog, const Enemy& enemy)
{
    if (catalog == nullptr) {
        return nullptr;
    }

    const HitboxDirection direction = enemyHitboxDirectionForFacing(enemy.facingAngle);
    if (const HitboxProfile* profile = enemyHitboxProfileFor(catalog, enemy.enemyId, direction)) {
        return profile;
    }
    return enemyHitboxProfileFor(catalog, enemy.enemyId, HitboxDirection::Default);
}

const HitboxProfile* enemyHitboxProfileFor(
    const HitboxCatalog* catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    return catalog == nullptr ? nullptr : enemyProfileSlot(catalog->enemies, enemyId, direction);
}

bool enemyHitboxHasProfile(
    const HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    return enemyHitboxProfileFor(&catalog, enemyId, direction) != nullptr;
}

bool enemyHitboxHasAnyProfile(const HitboxCatalog& catalog, std::string_view enemyId)
{
    if (enemyId.empty()) {
        return false;
    }

    const auto it = catalog.enemies.find(std::string(enemyId));
    if (it == catalog.enemies.end()) {
        return false;
    }

    return std::any_of(it->second.directions.begin(), it->second.directions.end(), [](const HitboxProfile& profile) {
        return profileHasCircles(profile);
    });
}

HitboxProfile& mutableEnemyHitboxProfile(
    HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    return catalog.enemies[std::string(enemyId)].directions[static_cast<std::size_t>(hitboxDirectionIndex(direction))];
}

bool eraseEnemyHitboxProfile(
    HitboxCatalog& catalog,
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

    HitboxProfile& profile = it->second.directions[static_cast<std::size_t>(hitboxDirectionIndex(direction))];
    const bool hadProfile = profileHasCircles(profile);
    profile.circles.clear();
    if (std::none_of(it->second.directions.begin(), it->second.directions.end(), [](const HitboxProfile& candidate) {
        return profileHasCircles(candidate);
    })) {
        catalog.enemies.erase(it);
    }
    return hadProfile;
}

const HitboxProfile* bossWeakPointProfileFor(const HitboxCatalog* catalog, const Enemy& enemy)
{
    if (catalog == nullptr) {
        return nullptr;
    }

    const HitboxDirection direction = enemyHitboxDirectionForFacing(enemy.facingAngle);
    if (const HitboxProfile* profile = bossWeakPointProfileFor(catalog, enemy.enemyId, direction)) {
        return profile;
    }
    return bossWeakPointProfileFor(catalog, enemy.enemyId, HitboxDirection::Default);
}

const HitboxProfile* bossWeakPointProfileFor(
    const HitboxCatalog* catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    return catalog == nullptr ? nullptr : enemyProfileSlot(catalog->bossWeakPoints, enemyId, direction);
}

bool bossWeakPointHasProfile(
    const HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    return bossWeakPointProfileFor(&catalog, enemyId, direction) != nullptr;
}

bool bossWeakPointHasAnyProfile(const HitboxCatalog& catalog, std::string_view enemyId)
{
    if (enemyId.empty()) {
        return false;
    }

    const auto it = catalog.bossWeakPoints.find(std::string(enemyId));
    if (it == catalog.bossWeakPoints.end()) {
        return false;
    }

    return std::any_of(it->second.directions.begin(), it->second.directions.end(), [](const HitboxProfile& profile) {
        return profileHasCircles(profile);
    });
}

HitboxProfile& mutableBossWeakPointProfile(
    HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    return catalog.bossWeakPoints[std::string(enemyId)].directions[static_cast<std::size_t>(hitboxDirectionIndex(direction))];
}

bool eraseBossWeakPointProfile(
    HitboxCatalog& catalog,
    std::string_view enemyId,
    HitboxDirection direction)
{
    if (enemyId.empty()) {
        return false;
    }

    const auto it = catalog.bossWeakPoints.find(std::string(enemyId));
    if (it == catalog.bossWeakPoints.end()) {
        return false;
    }

    HitboxProfile& profile = it->second.directions[static_cast<std::size_t>(hitboxDirectionIndex(direction))];
    const bool hadProfile = profileHasCircles(profile);
    profile.circles.clear();
    if (std::none_of(it->second.directions.begin(), it->second.directions.end(), [](const HitboxProfile& candidate) {
        return profileHasCircles(candidate);
    })) {
        catalog.bossWeakPoints.erase(it);
    }
    return hadProfile;
}

const HitboxProfile* objectHitboxProfileFor(const HitboxCatalog* catalog, std::string_view objectId)
{
    return catalog == nullptr ? nullptr : profileFor(catalog->objects, objectId);
}

const HitboxProfile* playerHitboxProfileFor(const HitboxCatalog* catalog)
{
    return catalog != nullptr && profileHasCircles(catalog->player) ? &catalog->player : nullptr;
}

bool playerHitboxHasProfile(const HitboxCatalog& catalog)
{
    return profileHasCircles(catalog.player);
}

HitboxProfile& mutablePlayerHitboxProfile(HitboxCatalog& catalog)
{
    return catalog.player;
}

bool erasePlayerHitboxProfile(HitboxCatalog& catalog)
{
    const bool hadProfile = profileHasCircles(catalog.player);
    catalog.player.circles.clear();
    return hadProfile;
}

HitCircle fallbackEnemyHitCircle(const Enemy& enemy)
{
    return {
        {},
        std::max(1.0f, enemy.radius),
    };
}

float hitboxProfileBoundsRadius(const HitboxProfile& profile, float scale, float radiusPadding)
{
    const float safeScale = sanitizedScale(scale);
    const float safePadding = sanitizedPadding(radiusPadding);
    float boundsRadius = 0.0f;
    for (HitCircle circle : profile.circles) {
        circle = sanitizeCircle(circle);
        boundsRadius = std::max(boundsRadius, length(circle.offset) * safeScale + circle.radius * safeScale + safePadding);
    }
    return std::max(1.0f, boundsRadius);
}

float enemyHitboxBoundsRadius(const Enemy& enemy, const HitboxCatalog* catalog)
{
    const float scale = enemyHitboxScale(enemy);
    if (const HitboxProfile* profile = enemyHitboxProfileFor(catalog, enemy)) {
        return hitboxProfileBoundsRadius(*profile, scale);
    }

    return std::max(1.0f, fallbackEnemyHitCircle(enemy).radius * scale);
}

bool hitboxProfileOverlapsCircle(
    const HitboxProfile& profile,
    Vec2 center,
    float rotationRadians,
    float scale,
    float radiusPadding,
    Vec2 circleCenter,
    float circleRadius)
{
    const float otherRadius = std::max(0.0f, circleRadius);
    const float safeScale = sanitizedScale(scale);
    const float safePadding = sanitizedPadding(radiusPadding);
    const float broadRadius = hitboxProfileBoundsRadius(profile, safeScale, safePadding) + otherRadius;
    if (distanceSquared(center, circleCenter) > broadRadius * broadRadius) {
        return false;
    }

    for (HitCircle hitCircle : profile.circles) {
        hitCircle = sanitizeCircle(hitCircle);
        const Vec2 hitCenter = center + rotateOffset(hitCircle.offset * safeScale, rotationRadians);
        const float radius = hitCircle.radius * safeScale + safePadding + otherRadius;
        if (distanceSquared(hitCenter, circleCenter) <= radius * radius) {
            return true;
        }
    }
    return false;
}

bool hitboxProfilesOverlap(
    const HitboxProfile& lhs,
    Vec2 lhsCenter,
    float lhsRotationRadians,
    float lhsScale,
    float lhsRadiusPadding,
    const HitboxProfile& rhs,
    Vec2 rhsCenter,
    float rhsRotationRadians,
    float rhsScale,
    float rhsRadiusPadding)
{
    const float lhsSafeScale = sanitizedScale(lhsScale);
    const float rhsSafeScale = sanitizedScale(rhsScale);
    const float lhsSafePadding = sanitizedPadding(lhsRadiusPadding);
    const float rhsSafePadding = sanitizedPadding(rhsRadiusPadding);
    const float broadRadius =
        hitboxProfileBoundsRadius(lhs, lhsSafeScale, lhsSafePadding) +
        hitboxProfileBoundsRadius(rhs, rhsSafeScale, rhsSafePadding);
    if (distanceSquared(lhsCenter, rhsCenter) > broadRadius * broadRadius) {
        return false;
    }

    for (HitCircle lhsCircle : lhs.circles) {
        lhsCircle = sanitizeCircle(lhsCircle);
        const Vec2 lhsCircleCenter = lhsCenter + rotateOffset(lhsCircle.offset * lhsSafeScale, lhsRotationRadians);
        const float lhsRadius = lhsCircle.radius * lhsSafeScale + lhsSafePadding;
        for (HitCircle rhsCircle : rhs.circles) {
            rhsCircle = sanitizeCircle(rhsCircle);
            const Vec2 rhsCircleCenter = rhsCenter + rotateOffset(rhsCircle.offset * rhsSafeScale, rhsRotationRadians);
            const float radius = lhsRadius + rhsCircle.radius * rhsSafeScale + rhsSafePadding;
            if (distanceSquared(lhsCircleCenter, rhsCircleCenter) <= radius * radius) {
                return true;
            }
        }
    }
    return false;
}

bool enemyHitboxOverlapsCircle(
    const Enemy& enemy,
    const HitboxCatalog* catalog,
    Vec2 circleCenter,
    float circleRadius)
{
    const float scale = enemyHitboxScale(enemy);
    if (const HitboxProfile* profile = enemyHitboxProfileFor(catalog, enemy)) {
        return hitboxProfileOverlapsCircle(*profile, enemy.position, 0.0f, scale, 0.0f, circleCenter, circleRadius);
    }

    const float radius = fallbackEnemyHitCircle(enemy).radius * scale + std::max(0.0f, circleRadius);
    return distanceSquared(enemy.position, circleCenter) <= radius * radius;
}

bool enemyHitboxOverlapsProfile(
    const Enemy& enemy,
    const HitboxCatalog* catalog,
    const HitboxProfile& profile,
    Vec2 profileCenter,
    float profileRotationRadians,
    float profileScale,
    float profileRadiusPadding)
{
    const float enemyScale = enemyHitboxScale(enemy);
    if (const HitboxProfile* enemyProfile = enemyHitboxProfileFor(catalog, enemy)) {
        return hitboxProfilesOverlap(
            *enemyProfile,
            enemy.position,
            0.0f,
            enemyScale,
            0.0f,
            profile,
            profileCenter,
            profileRotationRadians,
            profileScale,
            profileRadiusPadding);
    }

    return hitboxProfileOverlapsCircle(
        profile,
        profileCenter,
        profileRotationRadians,
        profileScale,
        profileRadiusPadding,
        enemy.position,
        fallbackEnemyHitCircle(enemy).radius * enemyScale);
}

bool loadHitboxCatalog(
    const std::filesystem::path& path,
    HitboxCatalog& outCatalog,
    std::string& outMessage)
{
    outCatalog.player.circles.clear();
    outCatalog.enemies.clear();
    outCatalog.bossWeakPoints.clear();
    outCatalog.objects.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outMessage = "Hitbox data not found";
        return false;
    }

    std::string line;
    bool firstLine = true;
    bool legacyEnemyOnly = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (firstLine) {
            firstLine = false;
            stripUtf8Bom(line);
            if (line == HitboxHeader || line == HitboxV2Header || line == LegacyHitboxHeader) {
                continue;
            }
            if (line == LegacyEnemyHitboxHeader) {
                legacyEnemyOnly = true;
                continue;
            }
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream stream(line);
        std::string kind;
        std::string id;
        std::string token;
        HitCircle circle;
        stream >> kind;
        if (stream.fail()) {
            continue;
        }
        if (kind == "player") {
            std::string shape;
            stream >> shape >> circle.offset.x >> circle.offset.y >> circle.radius;
            if (!stream.fail() && shape == "circle") {
                appendProfileEntry(outCatalog.player, circle);
            }
            continue;
        }

        stream >> id >> token;
        if (stream.fail() || id.empty()) {
            continue;
        }

        HitboxDirection direction = HitboxDirection::Default;
        std::string shape = token;
        if ((kind == "enemy" || kind == "boss_weakpoint" || legacyEnemyOnly) && token != "circle") {
            if (!parseHitboxDirection(token, direction)) {
                continue;
            }
            stream >> shape;
        }

        stream >> circle.offset.x >> circle.offset.y >> circle.radius;
        if (stream.fail() || shape != "circle") {
            continue;
        }

        if (kind == "enemy" || legacyEnemyOnly) {
            appendEnemyEntry(outCatalog.enemies, id, legacyEnemyOnly ? HitboxDirection::Default : direction, circle);
        } else if (kind == "boss_weakpoint") {
            appendEnemyEntry(outCatalog.bossWeakPoints, id, direction, circle);
        } else if (kind == "object") {
            appendEntry(outCatalog.objects, id, circle);
        }
    }

    outMessage = legacyEnemyOnly ? "Legacy enemy hitbox data loaded" : "Hitbox data loaded";
    return true;
}

bool saveHitboxCatalog(
    const std::filesystem::path& path,
    const HitboxCatalog& catalog,
    std::string& outMessage)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        outMessage = "Hitbox save failed: could not create data directory";
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        outMessage = "Hitbox save failed: could not open " + path.string();
        return false;
    }

    std::vector<EnemyHitboxSaveEntry> enemyEntries;
    collectEnemyEntries(catalog.enemies, enemyEntries);
    std::sort(enemyEntries.begin(), enemyEntries.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.id != rhs.id) {
            return lhs.id < rhs.id;
        }
        return hitboxDirectionIndex(lhs.direction) < hitboxDirectionIndex(rhs.direction);
    });

    std::vector<EnemyHitboxSaveEntry> bossWeakPointEntries;
    collectEnemyEntries(catalog.bossWeakPoints, bossWeakPointEntries);
    std::sort(bossWeakPointEntries.begin(), bossWeakPointEntries.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.id != rhs.id) {
            return lhs.id < rhs.id;
        }
        return hitboxDirectionIndex(lhs.direction) < hitboxDirectionIndex(rhs.direction);
    });

    std::vector<std::tuple<std::string_view, std::string, const HitboxProfile*>> objectEntries;
    collectEntries(catalog.objects, "object", objectEntries);
    std::sort(objectEntries.begin(), objectEntries.end(), [](const auto& lhs, const auto& rhs) {
        if (std::get<0>(lhs) != std::get<0>(rhs)) {
            return std::get<0>(lhs) < std::get<0>(rhs);
        }
        return std::get<1>(lhs) < std::get<1>(rhs);
    });

    file << "\xEF\xBB\xBF" << HitboxHeader << "\n";
    {
        int written = 0;
        for (HitCircle circle : catalog.player.circles) {
            if (written >= HitboxMaxCircles) {
                break;
            }
            circle = sanitizeCircle(circle);
            file << "player circle "
                << formatHitboxFloat(circle.offset.x) << " "
                << formatHitboxFloat(circle.offset.y) << " "
                << formatHitboxFloat(circle.radius) << "\n";
            ++written;
        }
    }
    for (const EnemyHitboxSaveEntry& entry : enemyEntries) {
        int written = 0;
        for (HitCircle circle : entry.profile->circles) {
            if (written >= HitboxMaxCircles) {
                break;
            }
            circle = sanitizeCircle(circle);
            file << "enemy " << entry.id << " ";
            if (entry.direction != HitboxDirection::Default) {
                file << hitboxDirectionId(entry.direction) << " ";
            }
            file << "circle "
                << formatHitboxFloat(circle.offset.x) << " "
                << formatHitboxFloat(circle.offset.y) << " "
                << formatHitboxFloat(circle.radius) << "\n";
            ++written;
        }
    }
    for (const EnemyHitboxSaveEntry& entry : bossWeakPointEntries) {
        int written = 0;
        for (HitCircle circle : entry.profile->circles) {
            if (written >= 1) {
                break;
            }
            circle = sanitizeCircle(circle);
            file << "boss_weakpoint " << entry.id << " ";
            if (entry.direction != HitboxDirection::Default) {
                file << hitboxDirectionId(entry.direction) << " ";
            }
            file << "circle "
                << formatHitboxFloat(circle.offset.x) << " "
                << formatHitboxFloat(circle.offset.y) << " "
                << formatHitboxFloat(circle.radius) << "\n";
            ++written;
        }
    }
    for (const auto& [kind, id, profile] : objectEntries) {
        int written = 0;
        for (HitCircle circle : profile->circles) {
            if (written >= HitboxMaxCircles) {
                break;
            }
            circle = sanitizeCircle(circle);
            file << kind << " " << id
                << " circle "
                << formatHitboxFloat(circle.offset.x) << " "
                << formatHitboxFloat(circle.offset.y) << " "
                << formatHitboxFloat(circle.radius) << "\n";
            ++written;
        }
    }

    if (!file) {
        outMessage = "Hitbox save failed while writing " + path.string();
        return false;
    }

    outMessage = "Hitbox saved";
    return true;
}

}
