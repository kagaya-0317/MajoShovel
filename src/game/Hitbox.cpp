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

constexpr std::string_view HitboxHeader = "MAJO_HITBOX_V1";
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

const HitboxProfile* profileFor(const std::unordered_map<std::string, HitboxProfile>& profiles, std::string_view id)
{
    if (id.empty()) {
        return nullptr;
    }

    const auto it = profiles.find(std::string(id));
    if (it == profiles.end() || it->second.circles.empty()) {
        return nullptr;
    }
    return &it->second;
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

} // namespace

HitboxProfile singleCircleHitbox(float radius)
{
    HitboxProfile profile;
    profile.circles.push_back({{}, std::max(HitboxRadiusMin, sanitizedFinite(radius, 10.0f))});
    return profile;
}

const HitboxProfile* enemyHitboxProfileFor(const HitboxCatalog* catalog, const Enemy& enemy)
{
    return catalog == nullptr ? nullptr : profileFor(catalog->enemies, enemy.enemyId);
}

const HitboxProfile* objectHitboxProfileFor(const HitboxCatalog* catalog, std::string_view objectId)
{
    return catalog == nullptr ? nullptr : profileFor(catalog->objects, objectId);
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
    outCatalog.enemies.clear();
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
            if (line == HitboxHeader) {
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
        std::string shape;
        HitCircle circle;
        stream >> kind >> id >> shape >> circle.offset.x >> circle.offset.y >> circle.radius;
        if (stream.fail() || id.empty() || shape != "circle") {
            continue;
        }

        if (kind == "enemy" || legacyEnemyOnly) {
            appendEntry(outCatalog.enemies, id, circle);
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

    std::vector<std::tuple<std::string_view, std::string, const HitboxProfile*>> entries;
    collectEntries(catalog.enemies, "enemy", entries);
    collectEntries(catalog.objects, "object", entries);
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        if (std::get<0>(lhs) != std::get<0>(rhs)) {
            return std::get<0>(lhs) < std::get<0>(rhs);
        }
        return std::get<1>(lhs) < std::get<1>(rhs);
    });

    file << "\xEF\xBB\xBF" << HitboxHeader << "\n";
    for (const auto& [kind, id, profile] : entries) {
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
