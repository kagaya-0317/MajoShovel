#include "game/ProjectileSystem.hpp"

#include "engine/Log.hpp"
#include "game/Collision.hpp"
#include "game/EncyclopediaSystem.hpp"
#include "game/EnemySystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace majo {

namespace {

struct ProjectilePrototype {
    std::string_view id;
    std::string_view displayName;
    float speed = 180.0f;
    float radius = 4.0f;
    float lifetime = 2.0f;
    int damage = 1;
    std::string_view damageType = "blunt";
    bool piercesTargets = false;
    std::initializer_list<std::string_view> tags;
};

constexpr std::array<ProjectilePrototype, 13> Prototypes{{
    {"stone_bullet", "石つぶて", 245.0f, 6.0f, 2.4f, 1, "blunt", false, {"small", "stone"}},
    {"big_stone_bullet", "大岩弾", 205.0f, 10.0f, 2.6f, 3, "blunt", false, {"stone"}},
    {"weapon_throw", "投げ武器", 220.0f, 6.8f, 2.0f, 2, "blunt", false, {"metal", "small"}},
    {"poison_spit", "毒液", 155.0f, 5.0f, 2.8f, 1, "water", false, {"small", "poison"}},
    {"paralyze_shot", "しびれ弾", 170.0f, 4.4f, 2.4f, 0, "none", false, {"small", "paralyze"}},
    {"mud_blob", "泥だんご", 150.0f, 5.8f, 2.6f, 0, "earth", false, {"mud", "poison"}},
    {"cactus_needle", "サボテン針", 260.0f, 2.5f, 1.8f, 1, "pierce", true, {"small", "needle"}},
    {"water_shot", "水弾", 210.0f, 4.0f, 2.2f, 1, "water", false, {"small", "water"}},
    {"fire_breath", "火炎ブレス", 145.0f, 7.0f, 1.2f, 2, "fire", false, {"fire", "short_range"}},
    {"web_thread", "クモ糸", 135.0f, 3.5f, 2.4f, 1, "blunt", false, {"small", "web"}},
    {"wind_wave", "風波", 235.0f, 6.0f, 1.6f, 1, "wind", true, {"wind"}},
    {"explosion_small", "小爆発", 80.0f, 10.0f, 0.55f, 2, "fire", false, {"explosion"}},
    {"junk_chunk", "ガラクタ弾", 235.0f, 9.5f, 2.5f, 2, "blunt", false, {"metal", "heavy"}},
}};

constexpr float CapturedMagnetProjectileRadius = 170.0f;
constexpr float CapturedMagnetProjectileAcceleration = 230.0f;
constexpr int CapturedMagnetProjectileLimit = 6;
constexpr float CapturedWindDeflectRadius = 150.0f;
constexpr float CapturedWindDeflectImpulse = 72.0f;
constexpr int CapturedWindDeflectLimit = 5;
constexpr std::size_t MaxProjectileFxParticles = 520;

enum class ProjectileFxEvent {
    Launch,
    Impact,
    Guard,
    Reflect,
    Expire,
};

struct ProjectileVisualProfile {
    Color core{220, 205, 160, 245};
    Color edge{44, 38, 34, 210};
    Color glow{220, 205, 160, 78};
    Color flash{255, 238, 170, 215};
    Color debris{170, 140, 92, 210};
    ProjectileFxVisual particleVisual = ProjectileFxVisual::Shard;
    float tailLengthScale = 1.25f;
    float tailWidthScale = 0.58f;
    float impactRingScale = 3.4f;
};

struct ProjectileFxEventTuning {
    int count = 6;
    float speedMin = 24.0f;
    float speedMax = 76.0f;
    float lifetimeMin = 0.18f;
    float lifetimeMax = 0.38f;
    float startSize = 2.6f;
    float endSize = 0.0f;
    float spreadRadians = Pi * 2.0f;
    float ringStartScale = 0.85f;
    float ringEndScale = 2.8f;
    bool ring = true;
};

const ProjectilePrototype& prototypeFor(std::string_view id)
{
    const auto it = std::find_if(Prototypes.begin(), Prototypes.end(), [id](const ProjectilePrototype& prototype) {
        return prototype.id == id;
    });
    if (it != Prototypes.end()) {
        return *it;
    }
    return Prototypes.front();
}

bool hasPrototype(std::string_view id)
{
    return std::any_of(Prototypes.begin(), Prototypes.end(), [id](const ProjectilePrototype& prototype) {
        return prototype.id == id;
    });
}

std::vector<ProjectileDefinition> makeProjectileDefinitions()
{
    std::vector<ProjectileDefinition> definitions;
    definitions.reserve(Prototypes.size());
    for (const ProjectilePrototype& prototype : Prototypes) {
        ProjectileDefinition definition;
        definition.id = std::string(prototype.id);
        definition.displayName = std::string(prototype.displayName);
        definition.speed = prototype.speed;
        definition.radius = prototype.radius;
        definition.lifetime = prototype.lifetime;
        definition.damage = prototype.damage;
        definition.damageType = std::string(prototype.damageType);
        definition.piercesTargets = prototype.piercesTargets;
        for (std::string_view tag : prototype.tags) {
            definition.tags.emplace_back(tag);
        }
        definitions.push_back(std::move(definition));
    }
    return definitions;
}

std::mt19937& projectileFxRng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

float sampleFloat(float minValue, float maxValue)
{
    if (maxValue < minValue) {
        std::swap(minValue, maxValue);
    }
    if (std::abs(maxValue - minValue) <= 0.0001f) {
        return minValue;
    }
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(projectileFxRng());
}

int sampleInt(int minValue, int maxValue)
{
    if (maxValue < minValue) {
        std::swap(minValue, maxValue);
    }
    std::uniform_int_distribution<int> dist(minValue, maxValue);
    return dist(projectileFxRng());
}

Vec2 perpendicular(Vec2 value)
{
    return {-value.y, value.x};
}

Color scaleAlpha(Color color, float scale)
{
    color.a = static_cast<unsigned char>(std::clamp(
        std::lround(static_cast<float>(color.a) * scale),
        0L,
        255L));
    return color;
}

Color mixColor(Color a, Color b, float t)
{
    t = clamp(t, 0.0f, 1.0f);
    const auto mixChannel = [t](unsigned char left, unsigned char right) {
        const float value = static_cast<float>(left) + (static_cast<float>(right) - static_cast<float>(left)) * t;
        return static_cast<unsigned char>(std::clamp(std::lround(value), 0L, 255L));
    };
    return {
        mixChannel(a.r, b.r),
        mixChannel(a.g, b.g),
        mixChannel(a.b, b.b),
        mixChannel(a.a, b.a),
    };
}

float angleOf(Vec2 value)
{
    return std::atan2(value.y, value.x);
}

int positiveModulo(int value, int divisor)
{
    if (divisor <= 0) {
        return 0;
    }
    const int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

struct JunkColorSet {
    Color core;
    Color edge;
    Color glow;
    Color flash;
    Color debris;
};

const std::array<JunkColorSet, 6>& junkColorPalette()
{
    static constexpr std::array<JunkColorSet, 6> Palette{{
        {{164, 166, 156, 250}, {46, 48, 48, 230}, {214, 208, 164, 82}, {255, 226, 128, 226}, {112, 118, 116, 218}},
        {{176, 116, 70, 250}, {64, 46, 34, 230}, {232, 144, 84, 78}, {255, 198, 118, 224}, {132, 82, 54, 220}},
        {{94, 132, 170, 248}, {36, 54, 76, 230}, {112, 178, 232, 82}, {184, 228, 255, 224}, {72, 98, 128, 220}},
        {{178, 160, 72, 248}, {70, 62, 30, 230}, {238, 214, 102, 78}, {255, 242, 148, 224}, {132, 116, 56, 220}},
        {{98, 152, 112, 248}, {34, 64, 44, 230}, {116, 214, 138, 76}, {190, 250, 174, 220}, {72, 116, 84, 216}},
        {{150, 104, 162, 248}, {58, 42, 70, 230}, {198, 138, 220, 76}, {238, 196, 255, 220}, {110, 78, 124, 216}},
    }};
    return Palette;
}

const JunkColorSet& junkColorSetFor(int variant)
{
    const auto& palette = junkColorPalette();
    return palette[static_cast<std::size_t>(positiveModulo(variant, static_cast<int>(palette.size())))];
}

Color junkDebrisColor(int variant)
{
    return junkColorSetFor(variant).debris;
}

int projectileVisualVariantFor(std::string_view projectileId)
{
    if (projectileId == "junk_chunk") {
        return sampleInt(0, static_cast<int>(junkColorPalette().size()) - 1);
    }
    return 0;
}

Color colorFor(std::string_view damageType)
{
    if (damageType == "fire") {
        return {255, 112, 48, 245};
    }
    if (damageType == "water") {
        return {78, 166, 255, 245};
    }
    if (damageType == "wind") {
        return {160, 235, 200, 235};
    }
    if (damageType == "magic") {
        return {190, 118, 255, 245};
    }
    if (damageType == "none") {
        return {184, 188, 198, 230};
    }
    return {220, 205, 160, 245};
}

ProjectileVisualProfile visualProfileFor(const Projectile& projectile)
{
    const std::string_view id = projectile.projectileId;
    if (id == "stone_bullet") {
        return {
            {166, 146, 104, 245},
            {58, 48, 40, 220},
            {196, 166, 104, 70},
            {232, 196, 124, 205},
            {126, 102, 72, 220},
            ProjectileFxVisual::Shard,
            1.05f,
            0.48f,
            2.9f,
        };
    }
    if (id == "big_stone_bullet") {
        return {
            {136, 124, 106, 250},
            {42, 40, 42, 230},
            {206, 190, 148, 82},
            {248, 222, 158, 220},
            {104, 92, 80, 235},
            ProjectileFxVisual::Shard,
            0.95f,
            0.66f,
            4.4f,
        };
    }
    if (id == "weapon_throw") {
        return {
            {212, 226, 234, 250},
            {58, 66, 74, 225},
            {178, 228, 255, 90},
            {255, 246, 178, 230},
            {190, 204, 214, 220},
            ProjectileFxVisual::SparkLine,
            2.2f,
            0.36f,
            3.0f,
        };
    }
    if (id == "poison_spit") {
        return {
            {118, 236, 76, 240},
            {82, 40, 110, 225},
            {126, 255, 86, 86},
            {190, 255, 104, 220},
            {176, 72, 214, 198},
            ProjectileFxVisual::SoftCircle,
            1.7f,
            0.72f,
            3.4f,
        };
    }
    if (id == "paralyze_shot") {
        return {
            {255, 238, 86, 245},
            {74, 90, 164, 225},
            {142, 224, 255, 95},
            {255, 255, 202, 235},
            {104, 218, 255, 212},
            ProjectileFxVisual::SparkLine,
            1.95f,
            0.38f,
            3.2f,
        };
    }
    if (id == "mud_blob") {
        return {
            {132, 86, 48, 245},
            {54, 38, 26, 220},
            {116, 78, 48, 70},
            {202, 142, 82, 210},
            {94, 62, 38, 218},
            ProjectileFxVisual::SoftCircle,
            1.05f,
            0.70f,
            3.8f,
        };
    }
    if (id == "cactus_needle") {
        return {
            {168, 236, 108, 245},
            {42, 92, 48, 225},
            {178, 255, 128, 74},
            {230, 255, 164, 220},
            {92, 178, 78, 205},
            ProjectileFxVisual::Needle,
            3.0f,
            0.25f,
            2.4f,
        };
    }
    if (id == "water_shot") {
        return {
            {76, 178, 255, 240},
            {42, 86, 156, 225},
            {106, 218, 255, 88},
            {196, 246, 255, 225},
            {124, 226, 255, 205},
            ProjectileFxVisual::SoftCircle,
            1.85f,
            0.58f,
            3.2f,
        };
    }
    if (id == "fire_breath") {
        return {
            {255, 104, 42, 250},
            {122, 40, 22, 224},
            {255, 136, 46, 112},
            {255, 226, 98, 232},
            {255, 156, 50, 214},
            ProjectileFxVisual::SoftCircle,
            1.9f,
            0.90f,
            3.6f,
        };
    }
    if (id == "web_thread") {
        return {
            {226, 232, 224, 238},
            {90, 96, 102, 210},
            {232, 246, 242, 72},
            {255, 255, 246, 210},
            {204, 214, 210, 190},
            ProjectileFxVisual::Thread,
            4.4f,
            0.24f,
            3.1f,
        };
    }
    if (id == "wind_wave") {
        return {
            {152, 246, 196, 230},
            {70, 144, 112, 200},
            {178, 255, 218, 82},
            {230, 255, 224, 218},
            {152, 250, 200, 192},
            ProjectileFxVisual::WindArc,
            1.7f,
            0.42f,
            4.1f,
        };
    }
    if (id == "explosion_small") {
        return {
            {255, 128, 42, 250},
            {126, 44, 28, 230},
            {255, 120, 48, 130},
            {255, 240, 116, 238},
            {92, 76, 64, 196},
            ProjectileFxVisual::SoftCircle,
            0.75f,
            1.05f,
            5.2f,
        };
    }
    if (id == "junk_chunk") {
        const JunkColorSet& colors = junkColorSetFor(projectile.visualVariant);
        return {
            colors.core,
            colors.edge,
            colors.glow,
            colors.flash,
            colors.debris,
            ProjectileFxVisual::Shard,
            1.30f,
            0.72f,
            4.0f,
        };
    }

    ProjectileVisualProfile profile;
    profile.core = colorFor(projectile.damageType);
    profile.flash = scaleAlpha(profile.core, 0.88f);
    profile.glow = scaleAlpha(profile.core, 0.34f);
    profile.debris = scaleAlpha(profile.core, 0.76f);
    return profile;
}

float projectileFadeScale(const Projectile& projectile)
{
    if (projectile.initialLifetime <= 0.0001f) {
        return 1.0f;
    }
    const float remaining = clamp(projectile.lifetime / projectile.initialLifetime, 0.0f, 1.0f);
    if (remaining >= 0.20f) {
        return 1.0f;
    }
    return clamp(remaining / 0.20f, 0.0f, 1.0f);
}

void drawShardShape(Renderer& renderer, Vec2 center, float radius, float rotation, Color color)
{
    const Vec2 forward = fromAngle(rotation);
    const Vec2 side = perpendicular(forward);
    const std::array<Vec2, 4> points{
        center + forward * (radius * 1.25f),
        center + side * (radius * 0.72f) - forward * (radius * 0.20f),
        center - forward * (radius * 1.05f),
        center - side * (radius * 0.58f) + forward * (radius * 0.10f),
    };
    renderer.fillPolygon(points.data(), points.size(), color);
}

void drawNeedleTriangleShape(Renderer& renderer, Vec2 center, float length, float width, float rotation, Color color)
{
    const Vec2 forward = fromAngle(rotation);
    const Vec2 side = perpendicular(forward);
    const Vec2 tip = center + forward * (length * 0.62f);
    const Vec2 rear = center - forward * (length * 0.48f);
    const std::array<Vec2, 3> points{
        tip,
        rear + side * width,
        rear - side * width,
    };
    renderer.fillPolygon(points.data(), points.size(), color);
}

void drawLightningBolt(Renderer& renderer, Vec2 center, float radius, float rotation, float phase, float stretch, Color color)
{
    const Vec2 forward = fromAngle(rotation);
    const Vec2 side = perpendicular(forward);
    const float length = radius * std::max(1.4f, stretch * 2.2f);
    std::vector<Vec2> points;
    points.reserve(5);
    for (int i = 0; i < 5; ++i) {
        const float t = static_cast<float>(i) / 4.0f;
        const float axis = (t - 0.5f) * length;
        const float endpointScale = (i == 0 || i == 4) ? 0.0f : 1.0f;
        const float offset = std::sin(phase * 24.0f + rotation * 3.7f + static_cast<float>(i) * 2.15f) * radius * 0.74f * endpointScale;
        points.push_back(center + forward * axis + side * offset);
    }
    renderer.drawSoftPolyline(points, std::max(2.2f, radius * 0.34f), scaleAlpha(color, 0.45f));
    renderer.drawSoftPolyline(points, std::max(1.0f, radius * 0.14f), color);
}

void drawProjectileFxParticle(Renderer& renderer, const ProjectileFxParticle& particle)
{
    if (!particle.active || particle.lifetime <= 0.0f) {
        return;
    }
    const float t = clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
    const Color color = mixColor(particle.startColor, particle.endColor, t);
    const float radius = std::max(0.0f, lerp(particle.startRadius, particle.endRadius, t));
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    const Vec2 direction = fromAngle(particle.rotation);
    const float stretch = std::max(0.1f, particle.stretch);
    switch (particle.visual) {
    case ProjectileFxVisual::Ring:
        renderer.drawSoftRing(
            particle.position,
            radius,
            std::max(1.0f, radius * 0.18f),
            color);
        return;
    case ProjectileFxVisual::SparkLine:
        renderer.drawSoftLine(
            particle.position - direction * (radius * stretch),
            particle.position + direction * (radius * stretch),
            std::max(1.0f, radius * 0.28f),
            color);
        return;
    case ProjectileFxVisual::Shard:
        drawShardShape(renderer, particle.position, radius, particle.rotation, color);
        return;
    case ProjectileFxVisual::Needle:
        renderer.drawSoftLine(
            particle.position - direction * (radius * stretch * 0.74f),
            particle.position + direction * (radius * stretch * 0.92f),
            std::max(1.0f, radius * 0.44f),
            scaleAlpha(color, 0.34f));
        drawNeedleTriangleShape(
            renderer,
            particle.position,
            radius * stretch * 2.2f,
            std::max(1.0f, radius * 0.42f),
            particle.rotation,
            color);
        return;
    case ProjectileFxVisual::Droplet:
        renderer.drawSoftLine(
            particle.position - direction * (radius * stretch * 0.35f),
            particle.position + direction * (radius * stretch * 0.90f),
            std::max(1.0f, radius * 0.46f),
            scaleAlpha(color, 0.72f));
        renderer.fillSoftCircle(particle.position + direction * (radius * stretch * 0.92f), radius * 0.70f, color);
        renderer.fillCircle(particle.position - direction * (radius * stretch * 0.18f), radius * 0.34f, scaleAlpha(color, 0.82f));
        return;
    case ProjectileFxVisual::LightningBolt:
        drawLightningBolt(renderer, particle.position, radius, particle.rotation, t, stretch, color);
        return;
    case ProjectileFxVisual::Flame:
        renderer.drawSoftLine(
            particle.position - direction * (radius * 0.28f),
            particle.position + direction * (radius * stretch * 0.96f),
            std::max(1.0f, radius * 0.70f),
            scaleAlpha(color, 0.58f));
        renderer.fillSoftCircle(particle.position + direction * (radius * stretch * 0.44f), radius * 0.86f, color);
        renderer.fillCircle(particle.position + direction * (radius * stretch * 0.62f), radius * 0.34f, scaleAlpha({255, 232, 116, color.a}, 0.86f));
        return;
    case ProjectileFxVisual::StickySplat: {
        renderer.fillSoftCircle(particle.position, radius * 1.22f, scaleAlpha(color, 0.72f));
        renderer.fillCircle(particle.position, radius * 0.48f, color);
        for (int i = 0; i < 4; ++i) {
            const float angle = particle.rotation + static_cast<float>(i) * 1.72f + std::sin(t * 7.0f + static_cast<float>(i)) * 0.18f;
            const Vec2 offset = fromAngle(angle) * radius * (0.42f + 0.20f * static_cast<float>(i % 2));
            renderer.fillSoftCircle(particle.position + offset, radius * (0.22f + 0.07f * static_cast<float>(i % 3)), scaleAlpha(color, 0.82f));
        }
        return;
    }
    case ProjectileFxVisual::Thread: {
        const Vec2 side = perpendicular(direction);
        renderer.drawSoftLine(
            particle.position - direction * (radius * stretch),
            particle.position + direction * (radius * stretch),
            std::max(1.0f, radius * 0.18f),
            color);
        renderer.drawLine(
            particle.position - side * (radius * 0.58f),
            particle.position + side * (radius * 0.58f),
            scaleAlpha(color, 0.62f));
        return;
    }
    case ProjectileFxVisual::WindArc: {
        const Vec2 side = perpendicular(direction);
        renderer.drawSoftRing(
            particle.position,
            radius,
            std::max(1.0f, radius * 0.12f),
            scaleAlpha(color, 0.62f));
        renderer.drawSoftLine(
            particle.position - side * (radius * 0.90f),
            particle.position + side * (radius * 0.90f),
            std::max(1.0f, radius * 0.18f),
            color);
        return;
    }
    case ProjectileFxVisual::SoftCircle:
        renderer.fillSoftCircle(particle.position, radius, color);
        renderer.fillCircle(particle.position, radius * 0.42f, scaleAlpha(color, 0.68f));
        return;
    }
}

float projectileShardSpinSpeed(const Projectile& projectile)
{
    if (projectile.projectileId == "big_stone_bullet") {
        return 11.0f;
    }
    if (projectile.projectileId == "stone_bullet") {
        return 14.0f;
    }
    if (projectile.projectileId == "junk_chunk") {
        return 13.0f;
    }
    return 5.0f;
}

void drawProjectile(Renderer& renderer, const Projectile& projectile)
{
    const ProjectileVisualProfile profile = visualProfileFor(projectile);
    const float fade = projectileFadeScale(projectile);
    const Vec2 direction = lengthSquared(projectile.velocity) > 0.0001f
        ? normalize(projectile.velocity)
        : Vec2{1.0f, 0.0f};
    const Vec2 side = perpendicular(direction);
    const float radius = projectile.radius;
    const float tailLength = radius * profile.tailLengthScale;
    const float tailWidth = std::max(1.0f, radius * profile.tailWidthScale);
    const Color glow = scaleAlpha(profile.glow, fade);
    const Color core = scaleAlpha(profile.core, fade);
    const Color edge = scaleAlpha(profile.edge, fade);

    if (tailLength > 1.0f) {
        renderer.drawSoftLine(
            projectile.position - direction * tailLength,
            projectile.position - direction * (radius * 0.15f),
            tailWidth,
            glow);
    }

    const std::string_view id = projectile.projectileId;
    if (id == "cactus_needle") {
        renderer.drawSoftLine(
            projectile.position - direction * (radius * 2.2f),
            projectile.position + direction * (radius * 3.2f),
            std::max(1.0f, radius * 0.82f),
            scaleAlpha(profile.glow, fade * 0.76f));
        const float rotation = angleOf(direction);
        drawNeedleTriangleShape(renderer, projectile.position, radius * 6.7f, radius * 1.14f, rotation, edge);
        drawNeedleTriangleShape(renderer, projectile.position + direction * (radius * 0.08f), radius * 5.9f, radius * 0.82f, rotation, core);
        renderer.fillCircle(projectile.position + direction * (radius * 3.8f), std::max(1.0f, radius * 0.32f), scaleAlpha(profile.flash, fade));
        return;
    }
    if (id == "weapon_throw") {
        const float spin = projectile.age * 18.0f;
        const Vec2 blade = fromAngle(spin);
        renderer.drawSoftLine(
            projectile.position - blade * (radius * 2.6f),
            projectile.position + blade * (radius * 2.6f),
            std::max(1.0f, radius * 0.46f),
            scaleAlpha(profile.glow, fade));
        renderer.drawLine(projectile.position - blade * (radius * 2.1f), projectile.position + blade * (radius * 2.1f), core);
        renderer.fillCircle(projectile.position, radius * 0.62f, edge);
        return;
    }
    if (id == "web_thread") {
        renderer.drawSoftLine(
            projectile.position - direction * (radius * 5.4f),
            projectile.position + direction * (radius * 2.1f),
            std::max(1.0f, radius * 0.70f),
            scaleAlpha(profile.glow, fade * 0.92f));
        renderer.drawLine(projectile.position - direction * (radius * 4.8f), projectile.position + direction * (radius * 1.8f), core);
        renderer.drawLine(projectile.position - side * (radius * 1.4f), projectile.position + side * (radius * 1.4f), scaleAlpha(profile.flash, fade * 0.55f));
        renderer.fillCircle(projectile.position, radius * 0.62f, scaleAlpha(core, 0.85f));
        return;
    }
    if (id == "wind_wave") {
        const float pulse = 1.0f + 0.12f * std::sin(projectile.age * 18.0f);
        renderer.drawSoftRing(projectile.position, radius * 1.75f * pulse, std::max(1.0f, radius * 0.22f), scaleAlpha(profile.glow, fade));
        renderer.drawSoftLine(
            projectile.position - side * (radius * 1.45f),
            projectile.position + side * (radius * 1.45f),
            std::max(1.0f, radius * 0.32f),
            scaleAlpha(core, fade));
        renderer.drawLine(
            projectile.position - side * (radius * 0.92f) + direction * (radius * 0.45f),
            projectile.position + side * (radius * 0.92f) + direction * (radius * 0.45f),
            scaleAlpha(profile.flash, fade * 0.72f));
        return;
    }
    if (id == "explosion_small") {
        const float pulse = 1.0f + 0.20f * std::sin(projectile.age * 26.0f);
        renderer.fillSoftCircle(projectile.position, radius * 1.85f * pulse, scaleAlpha(profile.glow, fade));
        renderer.fillCircle(projectile.position, radius * pulse, core);
        renderer.fillCircle(projectile.position - direction * (radius * 0.22f), radius * 0.45f, scaleAlpha(profile.flash, fade));
        renderer.drawSoftRing(projectile.position, radius * 1.30f * pulse, std::max(1.0f, radius * 0.14f), scaleAlpha(profile.flash, fade * 0.70f));
        return;
    }

    if (id == "fire_breath" || id == "poison_spit" || id == "water_shot" || id == "paralyze_shot" || id == "mud_blob") {
        renderer.fillSoftCircle(projectile.position, radius * 1.85f, glow);
        renderer.fillCircle(projectile.position, radius, core);
        renderer.fillCircle(projectile.position - direction * (radius * 0.30f) + side * (radius * 0.20f), radius * 0.45f, scaleAlpha(profile.flash, fade * 0.76f));
        renderer.drawCircle(projectile.position, radius + 1.6f, edge);
        return;
    }

    if (id == "stone_bullet" || id == "big_stone_bullet" || id == "junk_chunk") {
        const float spinPhase = static_cast<float>(projectile.visualVariant) * 0.73f;
        drawShardShape(renderer, projectile.position, radius * 0.98f, angleOf(direction) + projectile.age * projectileShardSpinSpeed(projectile) + spinPhase, core);
        renderer.drawCircle(projectile.position, radius + 1.8f, edge);
        renderer.fillCircle(projectile.position - direction * (radius * 0.25f) - side * (radius * 0.15f), radius * 0.32f, scaleAlpha(profile.flash, fade * 0.46f));
        return;
    }

    renderer.fillSoftCircle(projectile.position, radius * 1.55f, glow);
    renderer.fillCircle(projectile.position, radius, core);
    renderer.drawCircle(projectile.position, radius + 2.0f, edge);
}

void addProjectileFxParticle(std::vector<ProjectileFxParticle>& particles, ProjectileFxParticle particle)
{
    if (particle.lifetime <= 0.0f) {
        return;
    }
    particle.active = true;
    if (particles.size() >= MaxProjectileFxParticles) {
        const auto inactive = std::find_if(particles.begin(), particles.end(), [](const ProjectileFxParticle& existing) {
            return !existing.active;
        });
        if (inactive != particles.end()) {
            *inactive = particle;
            return;
        }
        particles.erase(particles.begin());
    }
    particles.push_back(particle);
}

bool projectileExpireUsesRing(const Projectile& projectile)
{
    const std::string_view id = projectile.projectileId;
    return !(id == "stone_bullet" ||
        id == "big_stone_bullet" ||
        id == "weapon_throw" ||
        id == "poison_spit" ||
        id == "paralyze_shot" ||
        id == "mud_blob" ||
        id == "cactus_needle" ||
        id == "water_shot" ||
        id == "fire_breath" ||
        id == "web_thread" ||
        id == "junk_chunk");
}

ProjectileFxEventTuning projectileFxTuning(const Projectile& projectile, ProjectileFxEvent event)
{
    ProjectileFxEventTuning tuning;
    const std::string_view id = projectile.projectileId;
    if (event == ProjectileFxEvent::Launch) {
        tuning.count = 5;
        tuning.speedMin = 18.0f;
        tuning.speedMax = 58.0f;
        tuning.lifetimeMin = 0.14f;
        tuning.lifetimeMax = 0.28f;
        tuning.startSize = std::max(1.5f, projectile.radius * 0.45f);
        tuning.ringStartScale = 0.55f;
        tuning.ringEndScale = 1.9f;
        tuning.spreadRadians = 1.35f;
    } else if (event == ProjectileFxEvent::Expire) {
        tuning.count = 5;
        tuning.speedMin = 12.0f;
        tuning.speedMax = 44.0f;
        tuning.lifetimeMin = 0.22f;
        tuning.lifetimeMax = 0.46f;
        tuning.startSize = std::max(1.4f, projectile.radius * 0.42f);
        tuning.ringStartScale = 0.70f;
        tuning.ringEndScale = 2.6f;
        tuning.ring = projectileExpireUsesRing(projectile);
    } else {
        tuning.count = 10;
        tuning.speedMin = 42.0f;
        tuning.speedMax = 118.0f;
        tuning.lifetimeMin = 0.20f;
        tuning.lifetimeMax = 0.44f;
        tuning.startSize = std::max(1.8f, projectile.radius * 0.55f);
        tuning.ringStartScale = 0.85f;
        tuning.ringEndScale = 3.1f;
    }

    if (id == "big_stone_bullet") {
        tuning.count += event == ProjectileFxEvent::Launch ? 2 : 7;
        tuning.speedMax += 28.0f;
        tuning.startSize *= 1.18f;
        tuning.ringEndScale += 1.0f;
    } else if (id == "weapon_throw") {
        tuning.count += 3;
        tuning.speedMax += 36.0f;
        tuning.startSize *= 0.72f;
    } else if (id == "poison_spit") {
        tuning.count += 4;
        tuning.speedMax *= 0.82f;
        tuning.lifetimeMax += 0.14f;
        tuning.startSize *= 0.92f;
    } else if (id == "paralyze_shot") {
        tuning.count += 5;
        tuning.speedMin += 28.0f;
        tuning.speedMax += 54.0f;
        tuning.lifetimeMax *= 0.82f;
        tuning.startSize *= 0.66f;
    } else if (id == "mud_blob") {
        tuning.count += 5;
        tuning.speedMax *= 0.78f;
        tuning.lifetimeMax += 0.18f;
        tuning.startSize *= 1.10f;
        tuning.ringEndScale += 0.7f;
    } else if (id == "cactus_needle") {
        tuning.count += 4;
        tuning.speedMin += 34.0f;
        tuning.speedMax += 56.0f;
        tuning.startSize *= 0.58f;
        tuning.ringEndScale -= 0.5f;
    } else if (id == "water_shot") {
        tuning.count += 5;
        tuning.speedMax += 16.0f;
        tuning.startSize *= 0.82f;
    } else if (id == "fire_breath") {
        tuning.count += 6;
        tuning.speedMin += 10.0f;
        tuning.speedMax += 36.0f;
        tuning.lifetimeMax += 0.08f;
        tuning.startSize *= 1.08f;
    } else if (id == "web_thread") {
        tuning.count += 3;
        tuning.speedMax *= 0.62f;
        tuning.lifetimeMin += 0.10f;
        tuning.lifetimeMax += 0.20f;
        tuning.startSize *= 0.70f;
    } else if (id == "wind_wave") {
        tuning.count += 6;
        tuning.speedMin += 38.0f;
        tuning.speedMax += 72.0f;
        tuning.lifetimeMax += 0.10f;
        tuning.ringEndScale += 1.0f;
    } else if (id == "explosion_small") {
        tuning.count += 12;
        tuning.speedMin += 18.0f;
        tuning.speedMax += 88.0f;
        tuning.startSize *= 1.16f;
        tuning.ringEndScale += 2.1f;
    } else if (id == "junk_chunk") {
        tuning.count += 6;
        tuning.speedMax += 42.0f;
        tuning.startSize *= 1.04f;
        tuning.ringEndScale += 0.7f;
    }

    if (event == ProjectileFxEvent::Expire) {
        if (id == "stone_bullet") {
            tuning.count += 4;
            tuning.speedMax += 54.0f;
            tuning.startSize *= 1.78f;
            tuning.lifetimeMax += 0.08f;
        } else if (id == "big_stone_bullet") {
            tuning.count += 5;
            tuning.speedMax += 62.0f;
            tuning.startSize *= 1.62f;
            tuning.lifetimeMax += 0.12f;
        } else if (id == "weapon_throw") {
            tuning.count += 4;
            tuning.speedMax += 62.0f;
            tuning.startSize *= 1.18f;
        } else if (id == "poison_spit" || id == "water_shot") {
            tuning.count += 8;
            tuning.speedMin += 40.0f;
            tuning.speedMax += 96.0f;
            tuning.startSize *= 1.16f;
            tuning.lifetimeMax += 0.12f;
        } else if (id == "paralyze_shot") {
            tuning.count += 8;
            tuning.speedMin += 22.0f;
            tuning.speedMax += 84.0f;
            tuning.startSize *= 1.88f;
            tuning.lifetimeMin *= 0.70f;
            tuning.lifetimeMax *= 0.80f;
        } else if (id == "mud_blob") {
            tuning.count += 5;
            tuning.speedMax += 28.0f;
            tuning.startSize *= 1.70f;
            tuning.lifetimeMax += 0.16f;
        } else if (id == "cactus_needle") {
            tuning.count += 4;
            tuning.speedMax += 64.0f;
            tuning.startSize *= 3.15f;
        } else if (id == "fire_breath") {
            tuning.count += 10;
            tuning.speedMin += 28.0f;
            tuning.speedMax += 86.0f;
            tuning.startSize *= 1.34f;
            tuning.lifetimeMax += 0.10f;
        } else if (id == "web_thread") {
            tuning.count += 9;
            tuning.speedMin = 6.0f;
            tuning.speedMax = 46.0f;
            tuning.startSize *= 2.70f;
            tuning.endSize = std::max(tuning.endSize, projectile.radius * 1.32f);
            tuning.lifetimeMax += 0.26f;
        } else if (id == "junk_chunk") {
            tuning.count += 7;
            tuning.speedMax += 72.0f;
            tuning.startSize *= 1.70f;
            tuning.lifetimeMax += 0.10f;
        }
    }

    if (event == ProjectileFxEvent::Guard) {
        tuning.count = std::max(5, tuning.count - 2);
        tuning.ringEndScale *= 0.80f;
    } else if (event == ProjectileFxEvent::Reflect) {
        tuning.count += 5;
        tuning.speedMin += 34.0f;
        tuning.speedMax += 54.0f;
        tuning.ringEndScale += 0.9f;
    }

    return tuning;
}

float projectileTrailInterval(const Projectile& projectile)
{
    const std::string_view id = projectile.projectileId;
    if (id == "fire_breath" || id == "explosion_small") {
        return 0.025f;
    }
    if (id == "water_shot" || id == "poison_spit" || id == "paralyze_shot" || id == "wind_wave") {
        return 0.040f;
    }
    if (id == "weapon_throw" || id == "cactus_needle" || id == "web_thread") {
        return id == "web_thread" ? 0.030f : 0.055f;
    }
    if (id == "big_stone_bullet" || id == "junk_chunk" || id == "mud_blob") {
        return 0.075f;
    }
    return 0.090f;
}

ProjectileFxVisual trailVisualFor(const Projectile& projectile)
{
    const std::string_view id = projectile.projectileId;
    if (id == "weapon_throw" || id == "paralyze_shot") {
        return ProjectileFxVisual::SparkLine;
    }
    if (id == "cactus_needle") {
        return ProjectileFxVisual::Needle;
    }
    if (id == "web_thread") {
        return ProjectileFxVisual::Thread;
    }
    if (id == "wind_wave") {
        return ProjectileFxVisual::WindArc;
    }
    if (id == "stone_bullet" || id == "big_stone_bullet" || id == "junk_chunk") {
        return ProjectileFxVisual::Shard;
    }
    return ProjectileFxVisual::SoftCircle;
}

ProjectileFxVisual projectileFxVisualForEvent(const Projectile& projectile, ProjectileFxEvent event, const ProjectileVisualProfile& profile)
{
    if (event == ProjectileFxEvent::Launch) {
        return trailVisualFor(projectile);
    }
    if (event == ProjectileFxEvent::Expire) {
        const std::string_view id = projectile.projectileId;
        if (id == "poison_spit" || id == "water_shot") {
            return ProjectileFxVisual::Droplet;
        }
        if (id == "paralyze_shot") {
            return ProjectileFxVisual::LightningBolt;
        }
        if (id == "fire_breath") {
            return ProjectileFxVisual::Flame;
        }
        if (id == "web_thread") {
            return ProjectileFxVisual::StickySplat;
        }
    }
    return profile.particleVisual;
}

Color projectileFxParticleStartColor(
    const Projectile& projectile,
    const ProjectileVisualProfile& profile,
    ProjectileFxEvent event,
    int index)
{
    if (projectile.projectileId == "junk_chunk" && event != ProjectileFxEvent::Launch) {
        const Color color = index % 3 == 0
            ? junkColorSetFor(projectile.visualVariant + index).flash
            : junkDebrisColor(projectile.visualVariant + index);
        return scaleAlpha(color, event == ProjectileFxEvent::Expire ? 0.92f : 1.0f);
    }
    if (event == ProjectileFxEvent::Expire && projectile.projectileId == "paralyze_shot") {
        return index % 2 == 0 ? Color{255, 255, 202, 235} : Color{112, 226, 255, 222};
    }
    if (event == ProjectileFxEvent::Expire && projectile.projectileId == "fire_breath") {
        return index % 3 == 0 ? Color{255, 238, 118, 232} : (index % 3 == 1 ? profile.flash : profile.debris);
    }
    if (event == ProjectileFxEvent::Expire && projectile.projectileId == "web_thread") {
        return index % 2 == 0 ? Color{238, 242, 234, 206} : Color{202, 214, 210, 190};
    }
    return index % 3 == 0 ? profile.flash : (index % 3 == 1 ? profile.debris : profile.glow);
}

void emitProjectileFxEvent(std::vector<ProjectileFxParticle>& particles, const Projectile& projectile, ProjectileFxEvent event)
{
    const ProjectileVisualProfile profile = visualProfileFor(projectile);
    const ProjectileFxEventTuning tuning = projectileFxTuning(projectile, event);
    const Vec2 direction = lengthSquared(projectile.velocity) > 0.0001f
        ? normalize(projectile.velocity)
        : Vec2{1.0f, 0.0f};
    const float baseAngle = angleOf(direction);
    const float eventDirection = event == ProjectileFxEvent::Launch ? baseAngle + Pi : baseAngle;
    const float radius = std::max(1.0f, projectile.radius);
    const float eventScale = event == ProjectileFxEvent::Expire ? 0.72f : 1.0f;

    if (tuning.ring) {
        ProjectileFxParticle ring;
        ring.visual = ProjectileFxVisual::Ring;
        ring.position = projectile.position;
        ring.velocity = {};
        ring.startColor = event == ProjectileFxEvent::Guard
            ? scaleAlpha({210, 226, 255, 220}, 0.86f)
            : scaleAlpha(profile.flash, eventScale);
        ring.endColor = scaleAlpha(ring.startColor, 0.0f);
        ring.lifetime = event == ProjectileFxEvent::Launch ? 0.16f : 0.24f;
        ring.startRadius = radius * tuning.ringStartScale;
        const float ringEndScale = event == ProjectileFxEvent::Launch
            ? tuning.ringEndScale
            : std::max(tuning.ringEndScale, profile.impactRingScale);
        ring.endRadius = radius * std::max(tuning.ringStartScale + 0.1f, ringEndScale);
        ring.rotation = sampleFloat(-Pi, Pi);
        addProjectileFxParticle(particles, ring);
    }

    for (int i = 0; i < tuning.count; ++i) {
        const float angle = eventDirection + sampleFloat(-tuning.spreadRadians * 0.5f, tuning.spreadRadians * 0.5f);
        const float speed = sampleFloat(tuning.speedMin, tuning.speedMax);
        const Vec2 radial = fromAngle(angle);
        ProjectileFxParticle particle;
        particle.visual = projectileFxVisualForEvent(projectile, event, profile);
        particle.position = projectile.position + radial * sampleFloat(0.0f, radius * 0.45f);
        particle.velocity = radial * speed + projectile.velocity * (event == ProjectileFxEvent::Launch ? -0.10f : 0.08f);
        particle.startColor = projectileFxParticleStartColor(projectile, profile, event, i);
        if (event == ProjectileFxEvent::Guard) {
            particle.startColor = mixColor(particle.startColor, {206, 226, 255, 220}, 0.55f);
        } else if (event == ProjectileFxEvent::Reflect) {
            particle.startColor = mixColor(particle.startColor, {255, 244, 170, 230}, 0.40f);
        } else if (event == ProjectileFxEvent::Expire &&
            projectile.projectileId != "paralyze_shot" &&
            projectile.projectileId != "fire_breath" &&
            projectile.projectileId != "web_thread") {
            particle.startColor = scaleAlpha(particle.startColor, 0.82f);
        }
        particle.endColor = scaleAlpha(particle.startColor, 0.0f);
        particle.lifetime = sampleFloat(tuning.lifetimeMin, tuning.lifetimeMax);
        particle.startRadius = sampleFloat(tuning.startSize * 0.68f, tuning.startSize * 1.22f);
        particle.endRadius = tuning.endSize;
        particle.rotation = angle + sampleFloat(-0.72f, 0.72f);
        particle.angularVelocity = sampleFloat(-8.0f, 8.0f);
        particle.drag = sampleFloat(2.2f, 5.0f);
        particle.stretch = sampleFloat(0.8f, 1.8f);
        if (event == ProjectileFxEvent::Expire && (projectile.projectileId == "poison_spit" || projectile.projectileId == "water_shot")) {
            particle.stretch = sampleFloat(1.7f, 3.2f);
            particle.drag = sampleFloat(4.0f, 7.0f);
        } else if (event == ProjectileFxEvent::Expire && projectile.projectileId == "paralyze_shot") {
            particle.stretch = sampleFloat(1.9f, 3.0f);
            particle.angularVelocity = sampleFloat(-18.0f, 18.0f);
            particle.drag = sampleFloat(5.0f, 9.0f);
        } else if (event == ProjectileFxEvent::Expire && projectile.projectileId == "fire_breath") {
            particle.stretch = sampleFloat(1.1f, 2.1f);
            particle.drag = sampleFloat(2.6f, 5.2f);
        } else if (event == ProjectileFxEvent::Expire && projectile.projectileId == "web_thread") {
            particle.stretch = sampleFloat(0.9f, 1.5f);
            particle.drag = sampleFloat(7.0f, 12.0f);
        } else if (projectile.projectileId == "web_thread") {
            particle.stretch = sampleFloat(2.6f, 5.0f);
        } else if (projectile.projectileId == "cactus_needle") {
            particle.stretch = sampleFloat(1.5f, 2.8f);
        } else if (projectile.projectileId == "wind_wave") {
            particle.stretch = sampleFloat(1.2f, 2.4f);
        }
        addProjectileFxParticle(particles, particle);
    }
}

void emitProjectileTrail(std::vector<ProjectileFxParticle>& particles, Projectile& projectile, float dt)
{
    if (dt <= 0.0f || !projectile.active) {
        return;
    }
    projectile.trailTimer -= dt;
    if (projectile.trailTimer > 0.0f) {
        return;
    }

    const ProjectileVisualProfile profile = visualProfileFor(projectile);
    const Vec2 direction = lengthSquared(projectile.velocity) > 0.0001f
        ? normalize(projectile.velocity)
        : Vec2{1.0f, 0.0f};
    const Vec2 side = perpendicular(direction);
    const float interval = projectileTrailInterval(projectile);
    const float radius = std::max(1.0f, projectile.radius);
    int spawned = 0;
    do {
        ProjectileFxParticle particle;
        particle.visual = trailVisualFor(projectile);
        particle.position =
            projectile.position -
            direction * sampleFloat(radius * 0.50f, radius * 1.45f) +
            side * sampleFloat(-radius * 0.45f, radius * 0.45f);
        particle.velocity =
            direction * sampleFloat(-42.0f, -8.0f) +
            side * sampleFloat(-22.0f, 22.0f);
        if (projectile.projectileId == "web_thread") {
            particle.position =
                projectile.position -
                direction * sampleFloat(radius * 1.25f, radius * 3.90f) +
                side * sampleFloat(-radius * 0.54f, radius * 0.54f);
            particle.velocity =
                direction * sampleFloat(-58.0f, -14.0f) +
                side * sampleFloat(-10.0f, 10.0f);
        }
        particle.startColor = scaleAlpha(
            spawned % 2 == 0 ? profile.glow : profile.debris,
            projectileFadeScale(projectile));
        particle.endColor = scaleAlpha(particle.startColor, 0.0f);
        particle.lifetime = projectile.projectileId == "web_thread"
            ? sampleFloat(0.56f, 0.92f)
            : sampleFloat(0.14f, 0.30f);
        particle.startRadius = sampleFloat(radius * 0.20f, radius * 0.56f);
        particle.endRadius = 0.0f;
        particle.rotation = angleOf(direction) + sampleFloat(-0.75f, 0.75f);
        particle.angularVelocity = sampleFloat(-4.0f, 4.0f);
        particle.drag = sampleFloat(3.0f, 7.0f);
        particle.stretch = sampleFloat(0.8f, 1.8f);
        if (projectile.projectileId == "fire_breath" || projectile.projectileId == "explosion_small") {
            particle.startRadius *= 1.32f;
            particle.lifetime += 0.08f;
        } else if (projectile.projectileId == "web_thread") {
            particle.startRadius *= 1.12f;
            particle.stretch = sampleFloat(3.5f, 6.4f);
            particle.drag = sampleFloat(1.2f, 2.8f);
        } else if (projectile.projectileId == "cactus_needle") {
            particle.stretch = sampleFloat(1.8f, 3.2f);
        }
        addProjectileFxParticle(particles, particle);
        projectile.trailTimer += interval * sampleFloat(0.82f, 1.18f);
        ++spawned;
    } while (projectile.trailTimer <= 0.0f && spawned < 3);
}

void updateProjectileFxParticles(std::vector<ProjectileFxParticle>& particles, float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    for (ProjectileFxParticle& particle : particles) {
        if (!particle.active) {
            continue;
        }
        particle.age += dt;
        if (particle.age >= particle.lifetime) {
            particle.active = false;
            continue;
        }
        particle.position += particle.velocity * dt;
        particle.velocity = particle.velocity * std::max(0.0f, 1.0f - particle.drag * dt);
        particle.rotation += particle.angularVelocity * dt;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(), [](const ProjectileFxParticle& particle) {
            return !particle.active;
        }),
        particles.end());
}

int projectileDamage(const Projectile& projectile)
{
    if (projectile.damageType == "fire" || projectile.damageType == "magic") {
        return std::max(0, projectile.damage + 1);
    }
    return std::max(0, projectile.damage);
}

int applyDefenseModifier(const EntityStatus& status, int damage)
{
    if (damage <= 0) {
        return 0;
    }

    const double defense = std::max(0.05, status.multiplierFor(ModifierStat::Defense));
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(damage) / defense)));
}

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

bool hasProjectileTag(const Projectile& projectile, std::string_view tag)
{
    return std::any_of(projectile.tags.begin(), projectile.tags.end(), [tag](const std::string& projectileTag) {
        return projectileTag == tag;
    });
}

void queueStatusPopupEvent(
    std::vector<StatusPopupEvent>& events,
    Vec2 position,
    std::string_view stateId,
    StatusPopupTarget target,
    const EntityStateApplyResult& result)
{
    if (!shouldShowEntityStatusPopup(result) || entityStatusDisplayName(stateId).empty()) {
        return;
    }
    events.push_back(StatusPopupEvent{
        .position = position,
        .stateId = std::string(stateId),
        .target = target,
    });
}

bool isHeavyProjectile(const Projectile& projectile)
{
    return projectile.radius >= 6.0f ||
        projectile.damage >= 2 ||
        hasProjectileTag(projectile, "large") ||
        hasProjectileTag(projectile, "heavy") ||
        projectile.projectileId == "big_stone_bullet" ||
        projectile.projectileId == "explosion_small";
}

bool orbitEffectContains(const ObjectDefinition& object, std::string_view effectCode);
double orbitEffectValue(const ObjectDefinition& object, std::string_view effectCode, double fallbackValue);

float projectileGuardRadius(const SpellRingItem& item, double value, float baseBonus, double areaMultiplier)
{
    const float normalizedValue = static_cast<float>(std::max(0.0, value));
    const float valueBonus = std::max(0.0f, normalizedValue - 1.0f) * 10.0f;
    return (item.hitRadius + baseBonus + valueBonus) *
        static_cast<float>(std::max(0.0, areaMultiplier));
}

bool projectileOverlapsItemGuard(const SpellRingItem& item, const Projectile& projectile, float guardRadius)
{
    return circlesOverlap(projectile.position, projectile.radius, item.worldPosition, guardRadius);
}

float equipmentScaledGuardRadius(const SpellRingSystem& spellRing, const SpellRingItem& item, float radius)
{
    const double guardAreaMultiplier = spellRing.equipmentModifiersForRing(item.ringIndex).guardAreaMul;
    return radius * static_cast<float>(std::max(0.0, guardAreaMultiplier));
}

struct GuardBlockResult {
    bool blocked = false;
    std::string effectKey;
};

GuardBlockResult blocksProjectile(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const Projectile& projectile,
    const ObjectDefinition* object)
{
    if (item.broken()) {
        return {};
    }

    if (item.hasCapturedBehavior("magic_guard") && projectile.damageType == "magic" &&
        circlesOverlap(
            projectile.position,
            projectile.radius,
            item.worldPosition,
            equipmentScaledGuardRadius(spellRing, item, item.hitRadius + 12.0f))) {
        return {true, "guard_projectile"};
    }

    if (item.hasCapturedBehavior("heavy_guard") &&
        (isPhysicalDamageType(projectile.damageType) || isHeavyProjectile(projectile)) &&
        circlesOverlap(
            projectile.position,
            projectile.radius,
            item.worldPosition,
            equipmentScaledGuardRadius(spellRing, item, item.hitRadius + 10.0f))) {
        return {true, isHeavyProjectile(projectile) ? "guard_large" : "guard_projectile"};
    }

    if (item.hasCapturedBehavior("outward_guard") &&
        circlesOverlap(
            projectile.position,
            projectile.radius,
            item.worldPosition,
            equipmentScaledGuardRadius(spellRing, item, item.hitRadius + 8.0f))) {
        const Vec2 outward = item.orbitOutward;
        const Vec2 incomingFrom = normalize(projectile.velocity * -1.0f);
        if (dot(outward, incomingFrom) > 0.25f) {
            return {true, "guard_projectile"};
        }
    }

    if (object == nullptr) {
        return {};
    }

    const bool heavy = isHeavyProjectile(projectile);
    if (heavy && orbitEffectContains(*object, "guard_large")) {
        const float guardRadius = projectileGuardRadius(
            item,
            orbitEffectValue(*object, "guard_large", 1.0),
            8.0f,
            spellRing.equipmentModifiersForRing(item.ringIndex).guardAreaMul);
        if (projectileOverlapsItemGuard(item, projectile, guardRadius)) {
            return {true, "guard_large"};
        }
    }
    if (!heavy && orbitEffectContains(*object, "guard")) {
        const float guardRadius = projectileGuardRadius(
            item,
            orbitEffectValue(*object, "guard", 1.0),
            4.0f,
            spellRing.equipmentModifiersForRing(item.ringIndex).guardAreaMul);
        if (projectileOverlapsItemGuard(item, projectile, guardRadius)) {
            return {true, "guard_projectile"};
        }
    }

    return {};
}

bool projectileOverlapsItemCounter(const SpellRingItem& item, const Projectile& projectile)
{
    return !item.broken() && projectileOverlapsItemGuard(item, projectile, item.hitRadius + 4.0f);
}

bool orbitEffectContains(const ObjectDefinition& object, std::string_view effectCode)
{
    for (const EffectSpec& spec : object.orbitEffects) {
        for (const std::string& effect : spec.effects) {
            if (effect == effectCode) {
                return true;
            }
        }
    }
    return false;
}

double orbitEffectValue(const ObjectDefinition& object, std::string_view effectCode, double fallbackValue)
{
    for (const EffectSpec& spec : object.orbitEffects) {
        for (std::size_t i = 0; i < spec.effects.size(); ++i) {
            if (spec.effects[i] != effectCode) {
                continue;
            }
            if (i < spec.values.size()) {
                return spec.values[i];
            }
            return fallbackValue;
        }
    }
    return fallbackValue;
}

bool objectHasEffectLineKey(const ObjectDefinition& object, std::string_view effectKey)
{
    return std::any_of(
        object.discoveryEffectLines.begin(),
        object.discoveryEffectLines.end(),
        [effectKey](const DiscoveryEffectLine& line) {
            return line.effectKey == effectKey;
        });
}

bool discoveryQueueContains(
    const std::vector<EffectDiscoveryEvent>* discoveryEvents,
    std::string_view objectId,
    std::string_view effectKey)
{
    if (discoveryEvents == nullptr || objectId.empty() || effectKey.empty()) {
        return false;
    }
    for (const EffectDiscoveryEvent& event : *discoveryEvents) {
        if (event.objectId == objectId && event.effectKey == effectKey) {
            return true;
        }
    }
    return false;
}

bool isEffectDiscovered(
    const EncyclopediaSystem* encyclopedia,
    const std::vector<EffectDiscoveryEvent>* discoveryEvents,
    std::string_view objectId,
    std::string_view effectKey)
{
    if (objectId.empty() || effectKey.empty()) {
        return false;
    }
    if (encyclopedia != nullptr && encyclopedia->hasObjectEffect(objectId, effectKey)) {
        return true;
    }
    return discoveryQueueContains(discoveryEvents, objectId, effectKey);
}

bool rollChancePercent(double chancePercent)
{
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.0, 100.0);
    return dist(rng) <= std::clamp(chancePercent, 0.0, 100.0);
}

bool projectileIsReflectImmune(const Projectile& projectile)
{
    if (projectile.projectileId == "explosion_small") {
        return true;
    }
    return hasProjectileTag(projectile, "unreflectable");
}

bool isMagicReflectDamageType(std::string_view damageType)
{
    return damageType == "magic" ||
        damageType == "fire" ||
        damageType == "ice" ||
        damageType == "thunder" ||
        damageType == "wind" ||
        damageType == "earth" ||
        damageType == "water";
}

void pushDiscoveryEvent(
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const ObjectDefinition& object,
    std::string_view effectKey,
    Vec2 position,
    std::string_view note = {})
{
    if (discoveryEvents == nullptr || object.id.empty() || effectKey.empty()) {
        return;
    }
    discoveryEvents->push_back(EffectDiscoveryEvent{
        .objectId = object.id,
        .objectName = object.name,
        .effectKey = std::string(effectKey),
        .description = {},
        .note = std::string(note),
        .position = position,
    });
}

std::string chooseGuardEffectKey(const ObjectDefinition* object, const Projectile& projectile)
{
    if (object == nullptr) {
        return "guard_projectile";
    }
    if (isHeavyProjectile(projectile) && objectHasEffectLineKey(*object, "guard_large")) {
        return "guard_large";
    }
    if (objectHasEffectLineKey(*object, "guard_projectile")) {
        return "guard_projectile";
    }
    if (objectHasEffectLineKey(*object, "guard")) {
        return "guard";
    }
    return "guard_projectile";
}

struct ReflectAttemptResult {
    bool reflected = false;
    std::string discoveredEffectKey;
    std::string note;
};

ReflectAttemptResult tryReflectProjectile(
    const Projectile& projectile,
    const ObjectDefinition& object,
    double reflectChanceAdd,
    const EncyclopediaSystem* encyclopedia,
    const std::vector<EffectDiscoveryEvent>* discoveryEvents)
{
    struct ReflectRule {
        std::string_view normalKey;
        std::string_view chanceKey;
    };

    ReflectRule rule{};
    if (isPhysicalDamageType(projectile.damageType)) {
        rule = {"reflect_physical", "reflect_physical_chance"};
    } else if (projectile.damageType == "water") {
        const bool hasWaterReflect =
            orbitEffectContains(object, "reflect_water") ||
            orbitEffectContains(object, "reflect_water_chance");
        if (hasWaterReflect) {
            rule = {"reflect_water", "reflect_water_chance"};
        } else {
            rule = {"reflect_magic", "reflect_magic_chance"};
        }
    } else if (isMagicReflectDamageType(projectile.damageType)) {
        rule = {"reflect_magic", "reflect_magic_chance"};
    } else {
        return {};
    }

    const bool hasNormal = orbitEffectContains(object, rule.normalKey);
    const bool hasChance = orbitEffectContains(object, rule.chanceKey);
    if (!hasNormal && !hasChance) {
        return {};
    }

    ReflectAttemptResult result;
    if (hasChance) {
        const bool firstGuarantee = discoveryEvents != nullptr &&
            !isEffectDiscovered(encyclopedia, discoveryEvents, object.id, rule.chanceKey);
        bool passed = firstGuarantee;
        if (!firstGuarantee) {
            passed = rollChancePercent(orbitEffectValue(object, rule.chanceKey, 0.0) + reflectChanceAdd);
        }
        if (passed) {
            result.discoveredEffectKey = std::string(rule.chanceKey);
            if (projectileIsReflectImmune(projectile)) {
                result.note = "※ただし、この弾には効かなかった";
                return result;
            }
            result.reflected = true;
            return result;
        }
    }

    if (!hasNormal) {
        return result;
    }

    result.discoveredEffectKey = std::string(rule.normalKey);
    if (projectileIsReflectImmune(projectile)) {
        result.note = "※ただし、この弾には効かなかった";
        return result;
    }
    result.reflected = true;
    return result;
}

void pushPlayer(Player& player, TileMap& map, Vec2 direction, float distance)
{
    const Vec2 delta = normalize(direction) * distance;
    const Vec2 next = player.position + delta;
    if (!map.isCircleBlocked(next, player.effectiveRadius(balance::PlayerRadius))) {
        player.position = next;
    }
}

}

std::span<const ProjectileDefinition> projectileDefinitions()
{
    static const std::vector<ProjectileDefinition> Definitions = makeProjectileDefinitions();
    return Definitions;
}

bool ProjectileSystem::spawn(std::string_view projectileId, Vec2 position, Vec2 direction, ProjectileOwnerType ownerType)
{
    static const std::vector<EffectSpec> NoEffects;
    return spawn(projectileId, position, direction, ownerType, NoEffects);
}

bool ProjectileSystem::spawn(std::string_view projectileId, Vec2 position, Vec2 direction, ProjectileOwnerType ownerType, const std::vector<EffectSpec>& effects)
{
    return spawn(projectileId, position, direction, ownerType, effects, ProjectileSpawnTuning{});
}

bool ProjectileSystem::spawn(
    std::string_view projectileId,
    Vec2 position,
    Vec2 direction,
    ProjectileOwnerType ownerType,
    const std::vector<EffectSpec>& effects,
    const ProjectileSpawnTuning& tuning)
{
    if (!hasPrototype(projectileId)) {
        static std::unordered_set<std::string> loggedUnknownProjectileIds;
        if (loggedUnknownProjectileIds.insert(std::string(projectileId)).second) {
            logError("[warning] ProjectileSystem: undefined projectile id \"" + std::string(projectileId) + "\"; spawn ignored");
        }
        return false;
    }

    Projectile* projectile = projectiles_.acquire();
    if (projectile == nullptr) {
        return false;
    }

    const ProjectilePrototype& prototype = prototypeFor(projectileId);
    *projectile = Projectile{};
    projectile->active = true;
    projectile->position = position;
    const float speedMultiplier = std::max(0.05f, tuning.speedMultiplier);
    projectile->velocity = normalize(direction) * (prototype.speed * speedMultiplier);
    projectile->radius = std::max(0.5f, prototype.radius * std::max(0.1f, tuning.radiusScale));
    projectile->age = 0.0f;
    projectile->lifetime = prototype.lifetime;
    projectile->initialLifetime = projectile->lifetime;
    projectile->ownerType = ownerType;
    projectile->projectileId = std::string(prototype.id);
    projectile->trailTimer = sampleFloat(0.0f, projectileTrailInterval(*projectile) * 0.65f);
    projectile->piercesTargets = prototype.piercesTargets;
    projectile->previewTargetHit = false;
    projectile->visualVariant = projectileVisualVariantFor(prototype.id);
    const double baseDamage = static_cast<double>(tuning.damageOverride >= 0 ? tuning.damageOverride : prototype.damage);
    projectile->damage = std::max(0, static_cast<int>(std::ceil(baseDamage * std::max(0.0, tuning.damageMultiplier))));
    projectile->damageType = std::string(prototype.damageType);
    for (std::string_view tag : prototype.tags) {
        projectile->tags.emplace_back(tag);
    }
    projectile->effects = effects;
    emitProjectileFxEvent(projectileFx_, *projectile, ProjectileFxEvent::Launch);
    return true;
}

void ProjectileSystem::updatePreview(float dt)
{
    updatePreview(dt, std::nullopt);
}

void ProjectileSystem::updatePreview(float dt, std::optional<ProjectilePreviewTarget> target)
{
    if (dt <= 0.0f) {
        return;
    }

    const bool hasTarget = target.has_value() && target->enabled && target->radius > 0.0f;
    updateProjectileFxParticles(projectileFx_, dt);
    for (Projectile& projectile : projectiles_.items()) {
        if (!projectile.active) {
            continue;
        }
        projectile.age += dt;
        projectile.lifetime -= dt;
        if (projectile.lifetime <= 0.0f) {
            emitProjectileFxEvent(projectileFx_, projectile, ProjectileFxEvent::Expire);
            projectile.active = false;
            continue;
        }
        projectile.position += projectile.velocity * dt;
        emitProjectileTrail(projectileFx_, projectile, dt);
        if (hasTarget && !projectile.previewTargetHit && circlesOverlap(projectile.position, projectile.radius, target->position, target->radius)) {
            emitProjectileFxEvent(projectileFx_, projectile, ProjectileFxEvent::Impact);
            projectile.previewTargetHit = true;
            if (!projectile.piercesTargets) {
                projectile.active = false;
            }
        }
    }
}

void ProjectileSystem::update(
    Player& player,
    SpellRingSystem& spellRing,
    EnemySystem& enemies,
    TileMap& map,
    float dt,
    const EffectDispatcher& effectDispatcher,
    const ObjectCatalog& objectCatalog,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    updateProjectileFxParticles(projectileFx_, dt);
    for (Projectile& projectile : projectiles_.items()) {
        if (!projectile.active) {
            continue;
        }

        projectile.age += dt;
        projectile.lifetime -= dt;
        if (projectile.lifetime <= 0.0f) {
            emitProjectileFxEvent(projectileFx_, projectile, ProjectileFxEvent::Expire);
            projectile.active = false;
            continue;
        }

        projectile.position += projectile.velocity * dt;
        emitProjectileTrail(projectileFx_, projectile, dt);
        if (map.isCircleBlocked(projectile.position, projectile.radius)) {
            soundEvents_.push_back(ProjectileSoundEvent::Impact);
            emitProjectileFxEvent(projectileFx_, projectile, ProjectileFxEvent::Impact);
            double mudRadius = 0.0;
            double mudSlow = 1.0;
            double mudDamagePerSecond = 0.0;
            double mudDuration = 0.0;
            std::string mudDamageType = "poison";
            for (const EffectSpec& spec : projectile.effects) {
                for (const std::string& effect : spec.effects) {
                    if (effect == "mud_zone") {
                        if (spec.values.size() >= 1) {
                            mudRadius = spec.values[0];
                        }
                        if (spec.values.size() >= 2) {
                            mudSlow = spec.values[1];
                        }
                        if (spec.values.size() >= 3) {
                            mudDamagePerSecond = spec.values[2];
                        }
                        mudDuration = spec.duration;
                    } else if (effect.rfind("mud_damage_type_", 0) == 0) {
                        mudDamageType = effect.substr(std::string("mud_damage_type_").size());
                    }
                }
            }
            if (mudDuration > 0.0 && mudRadius > 0.0) {
                enemies.addMudZone(
                    projectile.position,
                    static_cast<float>(mudRadius),
                    static_cast<float>(mudDuration),
                    static_cast<float>(mudSlow),
                    static_cast<float>(mudDamagePerSecond),
                    mudDamageType);
            }
            projectile.active = false;
            continue;
        }

        if (projectile.ownerType == ProjectileOwnerType::Enemy) {
            bool consumedByRing = false;
            const SpellRingItem* blockingItem = nullptr;
            const ObjectDefinition* blockingObject = nullptr;
            std::string guardEffectKey = "guard_projectile";
            bool reflectedByRing = false;
            const std::vector<const SpellRingItem*> runtimeItems = spellRing.runtimeItems();
            for (const SpellRingItem* itemPtr : runtimeItems) {
                if (itemPtr == nullptr) {
                    continue;
                }
                const SpellRingItem& item = *itemPtr;
                const ObjectDefinition* itemObject = nullptr;
                if (!item.objectId.empty()) {
                    itemObject = objectCatalog.registry.findById(item.objectId);
                }
                const GuardBlockResult guardBlock = blocksProjectile(item, spellRing, projectile, itemObject);
                if (guardBlock.blocked) {
                    blockingItem = itemPtr;
                    consumedByRing = true;
                    blockingObject = itemObject;
                    guardEffectKey = guardBlock.effectKey.empty()
                        ? chooseGuardEffectKey(blockingObject, projectile)
                        : guardBlock.effectKey;
                    if (blockingObject != nullptr) {
                        const ReflectAttemptResult reflect = tryReflectProjectile(
                            projectile,
                            *blockingObject,
                            spellRing.equipmentModifiersForRing(item.ringIndex).reflectChanceAdd,
                            encyclopedia,
                            discoveryEvents);
                        if (!reflect.discoveredEffectKey.empty()) {
                            pushDiscoveryEvent(
                                discoveryEvents,
                                *blockingObject,
                                reflect.discoveredEffectKey,
                                item.worldPosition,
                                reflect.note);
                        }
                        if (reflect.reflected) {
                            reflectedByRing = true;
                        }
                    }
                    break;
                }
                if (itemObject != nullptr && projectileOverlapsItemCounter(item, projectile)) {
                    const ReflectAttemptResult reflect = tryReflectProjectile(
                        projectile,
                        *itemObject,
                        spellRing.equipmentModifiersForRing(item.ringIndex).reflectChanceAdd,
                        encyclopedia,
                        discoveryEvents);
                    if (!reflect.discoveredEffectKey.empty()) {
                        pushDiscoveryEvent(
                            discoveryEvents,
                            *itemObject,
                            reflect.discoveredEffectKey,
                            item.worldPosition,
                            reflect.note);
                    }
                    if (reflect.reflected) {
                        blockingItem = itemPtr;
                        blockingObject = itemObject;
                        consumedByRing = true;
                        reflectedByRing = true;
                        break;
                    }
                }
            }
            if (consumedByRing) {
                if (reflectedByRing && blockingItem != nullptr) {
                    soundEvents_.push_back(ProjectileSoundEvent::Reflect);
                    emitProjectileFxEvent(projectileFx_, projectile, ProjectileFxEvent::Reflect);
                    projectile.ownerType = ProjectileOwnerType::PlayerOrbit;
                    projectile.damage = std::max(
                        0,
                        static_cast<int>(std::ceil(
                            static_cast<double>(projectile.damage) *
                            std::max(
                                0.0,
                                spellRing.equipmentModifiersForRing(blockingItem->ringIndex).reflectPowerMul))));
                    const Vec2 fromRing = projectile.position - blockingItem->worldPosition;
                    const Vec2 reflectDirection = lengthSquared(fromRing) > 0.0001f
                        ? normalize(fromRing)
                        : normalize(projectile.velocity * -1.0f);
                    const float speed = std::max(40.0f, length(projectile.velocity));
                    projectile.velocity = reflectDirection * speed;
                    continue;
                }
                soundEvents_.push_back(ProjectileSoundEvent::Guard);
                emitProjectileFxEvent(projectileFx_, projectile, ProjectileFxEvent::Guard);
                if (blockingItem != nullptr && blockingObject != nullptr) {
                    pushDiscoveryEvent(
                        discoveryEvents,
                        *blockingObject,
                        guardEffectKey,
                        blockingItem->worldPosition);
                }
                projectile.active = false;
                continue;
            }
        }

        if (projectile.ownerType == ProjectileOwnerType::Enemy &&
            circlesOverlap(projectile.position, projectile.radius, player.position, player.effectiveRadius(balance::PlayerRadius))) {
            player.applyDamage(
                applyDefenseModifier(player.status, projectileDamage(projectile)),
                DamageSource::Projectile);
            if (projectile.projectileId == "wind_wave") {
                pushPlayer(player, map, projectile.velocity, 18.0f);
            }
            std::vector<EffectSpec> dispatchEffects;
            dispatchEffects.reserve(projectile.effects.size());
            double mudRadius = 0.0;
            double mudSlow = 1.0;
            double mudDamagePerSecond = 0.0;
            double mudDuration = 0.0;
            std::string mudDamageType = "poison";
            for (const EffectSpec& spec : projectile.effects) {
                bool customOnly = false;
                for (const std::string& effect : spec.effects) {
                    if (effect == "status_paralyze") {
                        const double paralyzeDuration = spec.duration > 0.0 ? spec.duration : 1.5;
                        const EntityStateApplyResult result = player.status.applyState(
                            "status_paralyze",
                            1.0,
                            paralyzeDuration,
                            "enemy:shoot_paralyze",
                            StateApplyMode::KeepLonger);
                        queueStatusPopupEvent(
                            statusPopupEvents_,
                            player.position,
                            "status_paralyze",
                            StatusPopupTarget::Player,
                            result);
                        customOnly = true;
                    } else if (effect == "mud_zone") {
                        if (spec.values.size() >= 1) {
                            mudRadius = spec.values[0];
                        }
                        if (spec.values.size() >= 2) {
                            mudSlow = spec.values[1];
                        }
                        if (spec.values.size() >= 3) {
                            mudDamagePerSecond = spec.values[2];
                        }
                        mudDuration = spec.duration;
                        customOnly = true;
                    } else if (effect.rfind("mud_damage_type_", 0) == 0) {
                        mudDamageType = effect.substr(std::string("mud_damage_type_").size());
                        customOnly = true;
                    }
                }
                if (!customOnly) {
                    dispatchEffects.push_back(spec);
                }
            }
            if (mudDuration > 0.0 && mudRadius > 0.0) {
                enemies.addMudZone(
                    projectile.position,
                    static_cast<float>(mudRadius),
                    static_cast<float>(mudDuration),
                    static_cast<float>(mudSlow),
                    static_cast<float>(mudDamagePerSecond),
                    mudDamageType);
            }
            if (!dispatchEffects.empty()) {
                EffectContext context;
                context.owner = &player;
                context.encyclopedia = encyclopedia;
                context.statusPopupEvents = &statusPopupEvents_;
                context.position = projectile.position;
                context.triggerType = EffectTriggerType::Hit;
                context.logUnimplementedEffects = false;
                effectDispatcher.dispatch(dispatchEffects, context);
            }
            emitProjectileFxEvent(projectileFx_, projectile, ProjectileFxEvent::Impact);
            projectile.active = false;
            continue;
        }

        if (projectile.ownerType == ProjectileOwnerType::PlayerOrbit &&
            enemies.hitByPlayerProjectile(
                projectile,
                player,
                spellRing,
                projectileDamage(projectile),
                effectDispatcher,
                discoveryEvents,
                encyclopedia)) {
            emitProjectileFxEvent(projectileFx_, projectile, ProjectileFxEvent::Impact);
            projectile.active = false;
        }
    }
}

int ProjectileSystem::activeCount(ProjectileOwnerType ownerType) const
{
    int count = 0;
    for (const Projectile& projectile : projectiles_.items()) {
        if (projectile.active && projectile.ownerType == ownerType) {
            ++count;
        }
    }
    return count;
}

int ProjectileSystem::pullMetalProjectiles(Vec2 center, float dt, float radius)
{
    if (dt <= 0.0f) {
        return 0;
    }

    int pulled = 0;
    const float effectiveRadius = std::max(8.0f, radius);
    const float radiusSq = effectiveRadius * effectiveRadius;
    for (Projectile& projectile : projectiles_.items()) {
        if (!projectile.active || !hasProjectileTag(projectile, "metal")) {
            continue;
        }
        const Vec2 toCenter = center - projectile.position;
        const float distanceSq = lengthSquared(toCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }
        const float distance = std::sqrt(distanceSq);
        const float falloff = 1.0f - clamp(distance / effectiveRadius, 0.0f, 1.0f);
        projectile.velocity += normalize(toCenter) * (CapturedMagnetProjectileAcceleration * falloff * dt);
        ++pulled;
        if (pulled >= CapturedMagnetProjectileLimit) {
            break;
        }
    }
    return pulled;
}

int ProjectileSystem::deflectEnemyProjectiles(Vec2 center, float dt, float radius)
{
    if (dt <= 0.0f) {
        return 0;
    }

    int deflected = 0;
    const float effectiveRadius = std::max(8.0f, radius);
    const float radiusSq = effectiveRadius * effectiveRadius;
    for (Projectile& projectile : projectiles_.items()) {
        if (!projectile.active || projectile.ownerType != ProjectileOwnerType::Enemy) {
            continue;
        }
        const Vec2 fromCenter = projectile.position - center;
        const float distanceSq = lengthSquared(fromCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }
        const Vec2 side{-fromCenter.y, fromCenter.x};
        projectile.velocity += normalize(side) * (CapturedWindDeflectImpulse * dt);
        ++deflected;
        if (deflected >= CapturedWindDeflectLimit) {
            break;
        }
    }
    return deflected;
}

void ProjectileSystem::render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights) const
{
    std::vector<DepthRenderEntry> entries;
    appendRenderEntries(entries, renderer, map, playerLight, extraLights);
    std::stable_sort(entries.begin(), entries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : entries) {
        entry.draw();
    }
}

void ProjectileSystem::appendRenderEntries(
    std::vector<DepthRenderEntry>& entries,
    Renderer& renderer,
    const TileMap& map,
    Vec2 playerLight,
    const std::vector<LightSource>& extraLights) const
{
    for (const ProjectileFxParticle& particle : projectileFx_) {
        if (!particle.active || !map.isLit(particle.position, playerLight, extraLights)) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            particle.position.y,
            [&renderer, &particle]() {
                drawProjectileFxParticle(renderer, particle);
            },
        });
    }

    for (const Projectile& projectile : projectiles_.items()) {
        if (!projectile.active || !map.isLit(projectile.position, playerLight, extraLights)) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            projectile.position.y,
            [&renderer, &projectile]() {
                drawProjectile(renderer, projectile);
            },
        });
    }
}

void ProjectileSystem::appendPreviewRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer) const
{
    for (const ProjectileFxParticle& particle : projectileFx_) {
        if (!particle.active) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            particle.position.y,
            [&renderer, &particle]() {
                drawProjectileFxParticle(renderer, particle);
            },
        });
    }

    for (const Projectile& projectile : projectiles_.items()) {
        if (!projectile.active) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            projectile.position.y,
            [&renderer, &projectile]() {
                drawProjectile(renderer, projectile);
            },
        });
    }
}

void ProjectileSystem::clear()
{
    projectiles_ = {};
    projectileFx_.clear();
    soundEvents_.clear();
    statusPopupEvents_.clear();
}

std::vector<ProjectileSoundEvent> ProjectileSystem::consumeSoundEvents()
{
    std::vector<ProjectileSoundEvent> events = soundEvents_;
    soundEvents_.clear();
    return events;
}

std::vector<StatusPopupEvent> ProjectileSystem::consumeStatusPopupEvents()
{
    std::vector<StatusPopupEvent> consumed;
    consumed.swap(statusPopupEvents_);
    return consumed;
}

}
