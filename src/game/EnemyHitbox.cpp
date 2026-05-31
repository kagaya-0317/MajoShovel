#include "game/EnemyHitbox.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace majo {

namespace {

constexpr std::string_view EnemyHitboxHeader = "MAJO_ENEMY_HITBOX_V1";
constexpr float EnemyHitboxRadiusMin = 1.0f;
constexpr float EnemyHitboxRadiusMax = 512.0f;
constexpr float EnemyHitboxOffsetMax = 512.0f;

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

EnemyHitCircle sanitizeCircle(EnemyHitCircle circle)
{
    circle.offset.x = std::clamp(sanitizedFinite(circle.offset.x, 0.0f), -EnemyHitboxOffsetMax, EnemyHitboxOffsetMax);
    circle.offset.y = std::clamp(sanitizedFinite(circle.offset.y, 0.0f), -EnemyHitboxOffsetMax, EnemyHitboxOffsetMax);
    circle.radius = std::clamp(sanitizedFinite(circle.radius, 10.0f), EnemyHitboxRadiusMin, EnemyHitboxRadiusMax);
    return circle;
}

float enemyHitboxScale(const Enemy& enemy)
{
    return std::max(0.0f, static_cast<float>(enemy.status.sizeMultiplierFromStates()));
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

} // namespace

const EnemyHitboxProfile* enemyHitboxProfileFor(const EnemyHitboxCatalog* catalog, const Enemy& enemy)
{
    if (catalog == nullptr || enemy.enemyId.empty()) {
        return nullptr;
    }

    const auto it = catalog->profiles.find(enemy.enemyId);
    if (it == catalog->profiles.end() || it->second.circles.empty()) {
        return nullptr;
    }
    return &it->second;
}

EnemyHitCircle fallbackEnemyHitCircle(const Enemy& enemy)
{
    return {
        {},
        std::max(1.0f, enemy.radius),
    };
}

float enemyHitboxBoundsRadius(const Enemy& enemy, const EnemyHitboxCatalog* catalog)
{
    const float scale = enemyHitboxScale(enemy);
    if (const EnemyHitboxProfile* profile = enemyHitboxProfileFor(catalog, enemy)) {
        float boundsRadius = 0.0f;
        for (EnemyHitCircle circle : profile->circles) {
            circle = sanitizeCircle(circle);
            boundsRadius = std::max(boundsRadius, length(circle.offset) + circle.radius);
        }
        return std::max(1.0f, boundsRadius * scale);
    }

    return std::max(1.0f, fallbackEnemyHitCircle(enemy).radius * scale);
}

bool enemyHitboxOverlapsCircle(
    const Enemy& enemy,
    const EnemyHitboxCatalog* catalog,
    Vec2 circleCenter,
    float circleRadius)
{
    const float otherRadius = std::max(0.0f, circleRadius);
    const float scale = enemyHitboxScale(enemy);
    const float broadRadius = enemyHitboxBoundsRadius(enemy, catalog) + otherRadius;
    if (distanceSquared(enemy.position, circleCenter) > broadRadius * broadRadius) {
        return false;
    }

    const auto testCircle = [&](EnemyHitCircle hitCircle) {
        hitCircle = sanitizeCircle(hitCircle);
        const Vec2 center = enemy.position + hitCircle.offset * scale;
        const float radius = hitCircle.radius * scale + otherRadius;
        return distanceSquared(center, circleCenter) <= radius * radius;
    };

    if (const EnemyHitboxProfile* profile = enemyHitboxProfileFor(catalog, enemy)) {
        for (EnemyHitCircle hitCircle : profile->circles) {
            if (testCircle(hitCircle)) {
                return true;
            }
        }
        return false;
    }

    return testCircle(fallbackEnemyHitCircle(enemy));
}

bool loadEnemyHitboxCatalog(
    const std::filesystem::path& path,
    EnemyHitboxCatalog& outCatalog,
    std::string& outMessage)
{
    outCatalog.profiles.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outMessage = "Enemy hitbox data not found";
        return false;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (firstLine) {
            firstLine = false;
            stripUtf8Bom(line);
            if (line == EnemyHitboxHeader) {
                continue;
            }
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream stream(line);
        std::string kind;
        std::string enemyId;
        std::string shape;
        EnemyHitCircle circle;
        stream >> kind >> enemyId >> shape >> circle.offset.x >> circle.offset.y >> circle.radius;
        if (stream.fail() || kind != "enemy" || enemyId.empty() || shape != "circle") {
            continue;
        }

        EnemyHitboxProfile& profile = outCatalog.profiles[enemyId];
        if (static_cast<int>(profile.circles.size()) >= EnemyHitboxMaxCircles) {
            continue;
        }
        profile.circles.push_back(sanitizeCircle(circle));
    }

    outMessage = "Enemy hitbox data loaded";
    return true;
}

bool saveEnemyHitboxCatalog(
    const std::filesystem::path& path,
    const EnemyHitboxCatalog& catalog,
    std::string& outMessage)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        outMessage = "Enemy hitbox save failed: could not create data directory";
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        outMessage = "Enemy hitbox save failed: could not open " + path.string();
        return false;
    }

    std::vector<std::pair<std::string, const EnemyHitboxProfile*>> entries;
    entries.reserve(catalog.profiles.size());
    for (const auto& [enemyId, profile] : catalog.profiles) {
        if (!enemyId.empty() && !profile.circles.empty()) {
            entries.push_back({enemyId, &profile});
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    file << "\xEF\xBB\xBF" << EnemyHitboxHeader << "\n";
    for (const auto& [enemyId, profile] : entries) {
        int written = 0;
        for (EnemyHitCircle circle : profile->circles) {
            if (written >= EnemyHitboxMaxCircles) {
                break;
            }
            circle = sanitizeCircle(circle);
            file << "enemy " << enemyId
                << " circle "
                << formatHitboxFloat(circle.offset.x) << " "
                << formatHitboxFloat(circle.offset.y) << " "
                << formatHitboxFloat(circle.radius) << "\n";
            ++written;
        }
    }

    if (!file) {
        outMessage = "Enemy hitbox save failed while writing " + path.string();
        return false;
    }

    outMessage = "Enemy hitbox saved";
    return true;
}

}
