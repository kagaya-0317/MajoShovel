#include "game/MagicFxSystem.hpp"

#include "engine/Renderer.hpp"
#include "game/DepthRender.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace majo {

namespace {

constexpr std::size_t MaxParticles = 2400;
constexpr std::size_t MaxEmitters = 220;
constexpr std::size_t MaxLightningStrikes = 32;

std::mt19937& magicFxRng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

float random01()
{
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(magicFxRng());
}

float sampleRange(MagicFxRange range)
{
    if (range.max < range.min) {
        std::swap(range.min, range.max);
    }
    if (std::abs(range.max - range.min) <= 0.0001f) {
        return range.min;
    }
    std::uniform_real_distribution<float> dist(range.min, range.max);
    return dist(magicFxRng());
}

Vec2 perpendicular(Vec2 value)
{
    return {-value.y, value.x};
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

Color scaleAlpha(Color color, float scale)
{
    color.a = static_cast<unsigned char>(std::clamp(
        std::lround(static_cast<float>(color.a) * scale),
        0L,
        255L));
    return color;
}

Color particleColor(const MagicFxSystem::Particle& particle)
{
    const float t = particle.lifetime > 0.0f ? clamp(particle.age / particle.lifetime, 0.0f, 1.0f) : 1.0f;
    Color color = mixColor(particle.startColor, particle.endColor, t);
    float alphaScale = 1.0f;
    if (particle.fadeInFraction > 0.0f && t < particle.fadeInFraction) {
        alphaScale *= clamp(t / particle.fadeInFraction, 0.0f, 1.0f);
    }
    if (particle.fadeOutFraction > 0.0f && t > 1.0f - particle.fadeOutFraction) {
        alphaScale *= clamp((1.0f - t) / particle.fadeOutFraction, 0.0f, 1.0f);
    }
    color.a = static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.a) * alphaScale), 0L, 255L));
    return color;
}

float particleSize(const MagicFxSystem::Particle& particle)
{
    const float t = particle.lifetime > 0.0f ? clamp(particle.age / particle.lifetime, 0.0f, 1.0f) : 1.0f;
    return std::max(0.0f, lerp(particle.startSize, particle.endSize, t));
}

Vec2 particleDrawPosition(const MagicFxSystem::Particle& particle)
{
    return particle.position + Vec2{0.0f, -std::max(0.0f, particle.height)};
}

void drawShard(Renderer& renderer, Vec2 center, float size, float rotation, Color color)
{
    const Vec2 forward = fromAngle(rotation);
    const Vec2 side = perpendicular(forward);
    const std::array<Vec2, 3> points{
        center + forward * (size * 1.35f),
        center - forward * (size * 0.75f) + side * (size * 0.55f),
        center - forward * (size * 0.75f) - side * (size * 0.55f),
    };
    renderer.fillPolygon(points.data(), points.size(), color);
}

void drawCrystal(Renderer& renderer, Vec2 center, float size, float rotation, Color color)
{
    const Vec2 forward = fromAngle(rotation);
    const Vec2 side = perpendicular(forward);
    const std::array<Vec2, 8> points{
        center + forward * (size * 1.55f),
        center + forward * (size * 0.62f) + side * (size * 0.48f),
        center + forward * (size * 0.12f) + side * (size * 0.72f),
        center - forward * (size * 0.52f) + side * (size * 0.50f),
        center - forward * (size * 1.10f),
        center - forward * (size * 0.42f) - side * (size * 0.58f),
        center + forward * (size * 0.18f) - side * (size * 0.70f),
        center + forward * (size * 0.74f) - side * (size * 0.42f),
    };
    renderer.fillPolygon(points.data(), points.size(), color);
    const Color facet = scaleAlpha({246, 255, 255, color.a}, 0.58f);
    renderer.drawLine(points[0], center + side * (size * 0.30f), facet);
    renderer.drawLine(points[0], center - side * (size * 0.28f), scaleAlpha(facet, 0.72f));
    renderer.drawLine(points[2], points[5], scaleAlpha(facet, 0.52f));
}

void drawWindCrescent(Renderer& renderer, Vec2 center, float size, float rotation, float stretch, Color color)
{
    const Vec2 forward = fromAngle(rotation);
    const Vec2 side = perpendicular(forward);
    constexpr std::size_t Segments = 24;
    const float halfHeight = size * 1.06f;
    const float xScale = std::max(1.0f, stretch * 0.52f);
    const float outerRadius = halfHeight * 1.18f;
    const float innerRadius = halfHeight * 1.50f;
    const float outerShift = outerRadius * 0.28f;
    const float innerShift = innerRadius * 0.63f;

    std::array<Vec2, (Segments + 1) * 2> vertices{};
    std::array<int, Segments * 6> indices{};
    std::array<Vec2, Segments + 1> outerArc{};
    std::array<Vec2, Segments + 1> innerArc{};

    for (std::size_t i = 0; i <= Segments; ++i) {
        const float u = -1.0f + 2.0f * (static_cast<float>(i) / static_cast<float>(Segments));
        const float y = u * halfHeight;
        const float outerX = (std::sqrt(std::max(0.0f, outerRadius * outerRadius - y * y)) - outerShift) * xScale;
        float innerX = (std::sqrt(std::max(0.0f, innerRadius * innerRadius - y * y)) - innerShift) * xScale;
        const float tipBlend = std::pow(std::abs(u), 3.2f);
        innerX = lerp(innerX, outerX - halfHeight * 0.035f * xScale, tipBlend);

        outerArc[i] = center + forward * outerX + side * y;
        innerArc[i] = center + forward * innerX + side * y;
        vertices[i * 2] = outerArc[i];
        vertices[i * 2 + 1] = innerArc[i];
    }

    int index = 0;
    for (std::size_t i = 0; i < Segments; ++i) {
        const int outerA = static_cast<int>(i * 2);
        const int innerA = outerA + 1;
        const int outerB = static_cast<int>((i + 1) * 2);
        const int innerB = outerB + 1;
        indices[static_cast<std::size_t>(index++)] = outerA;
        indices[static_cast<std::size_t>(index++)] = outerB;
        indices[static_cast<std::size_t>(index++)] = innerA;
        indices[static_cast<std::size_t>(index++)] = innerA;
        indices[static_cast<std::size_t>(index++)] = outerB;
        indices[static_cast<std::size_t>(index++)] = innerB;
    }

    renderer.fillTriangleList(vertices.data(), vertices.size(), indices.data(), indices.size(), scaleAlpha(color, 0.74f));

    const float edgeWidth = std::max(1.0f, size * 0.13f);
    const Color softEdge = scaleAlpha(color, 0.32f);
    const Color highlight = scaleAlpha({238, 255, 238, color.a}, 0.62f);
    for (std::size_t i = 1; i < outerArc.size(); ++i) {
        renderer.drawSoftLine(outerArc[i - 1], outerArc[i], edgeWidth * 2.8f, softEdge);
        renderer.drawSoftLine(outerArc[i - 1], outerArc[i], edgeWidth, color);
    }
    for (std::size_t i = 1; i < innerArc.size(); ++i) {
        renderer.drawSoftLine(innerArc[i - 1], innerArc[i], std::max(1.0f, edgeWidth * 0.68f), highlight);
    }
}

void drawWindSparkle(Renderer& renderer, Vec2 center, float size, float rotation, Color color)
{
    const Vec2 forward = fromAngle(rotation);
    const Vec2 side = perpendicular(forward);
    const float coreSize = std::max(0.8f, size * 0.72f);
    const std::array<Vec2, 4> core{
        center + forward * (coreSize * 0.72f),
        center + side * (coreSize * 0.42f),
        center - forward * (coreSize * 0.72f),
        center - side * (coreSize * 0.42f),
    };
    renderer.fillSoftCircle(center, std::max(0.6f, size * 0.45f), scaleAlpha(color, 0.34f));
    renderer.fillPolygon(core.data(), core.size(), scaleAlpha({246, 255, 236, color.a}, 0.78f));

    const float rayLength = std::max(1.2f, size * 1.72f);
    const Color ray = scaleAlpha({236, 255, 226, color.a}, 0.58f);
    renderer.drawSoftLine(center - forward * rayLength, center + forward * rayLength, std::max(1.0f, size * 0.24f), ray);
    renderer.drawSoftLine(center - side * (rayLength * 0.62f), center + side * (rayLength * 0.62f), std::max(1.0f, size * 0.18f), scaleAlpha(ray, 0.62f));
}

void drawLightningPath(Renderer& renderer, const Vec2* points, int pointCount, float outerWidth, float coreWidth, float alpha, bool strong)
{
    if (points == nullptr || pointCount < 2 || alpha <= 0.0f) {
        return;
    }

    const Color outer = scaleAlpha(strong ? Color{92, 150, 255, 150} : Color{120, 176, 255, 108}, alpha);
    const Color mid = scaleAlpha(strong ? Color{255, 232, 118, 214} : Color{218, 240, 255, 172}, alpha);
    const Color core = scaleAlpha(Color{255, 255, 255, 255}, alpha);
    for (int i = 1; i < pointCount; ++i) {
        const float segmentT = static_cast<float>(i - 1) / static_cast<float>(std::max(1, pointCount - 2));
        const float pulse = 0.82f + 0.30f * (static_cast<float>((i * 37) % 5) / 4.0f);
        const float taper = lerp(1.10f, 0.76f, segmentT);
        const float width = outerWidth * pulse * taper;
        renderer.drawSoftLine(points[i - 1], points[i], width * 2.10f, scaleAlpha(outer, 0.72f));
        renderer.drawSoftLine(points[i - 1], points[i], width, mid);
        renderer.drawSoftLine(points[i - 1], points[i], std::max(1.0f, coreWidth * taper), core);
    }
}

void drawLightningStrike(Renderer& renderer, const MagicFxSystem::LightningStrike& strike)
{
    const float progress = strike.lifetime > 0.0f ? clamp(strike.age / strike.lifetime, 0.0f, 1.0f) : 1.0f;
    const float fade = progress < 0.18f
        ? clamp(progress / 0.18f, 0.0f, 1.0f)
        : std::pow(clamp((1.0f - progress) / 0.82f, 0.0f, 1.0f), 0.62f);
    const float flicker = 0.86f + 0.14f * std::sin(progress * Pi * 18.0f);
    const float alpha = fade * flicker;

    drawLightningPath(
        renderer,
        strike.points.data(),
        strike.pointCount,
        strike.outerWidth,
        strike.coreWidth,
        alpha,
        strike.strong);
    for (int i = 0; i < strike.branchCount; ++i) {
        const MagicFxSystem::LightningBranch& branch = strike.branches[static_cast<std::size_t>(i)];
        drawLightningPath(
            renderer,
            branch.points.data(),
            branch.pointCount,
            branch.width,
            std::max(1.0f, branch.width * 0.30f),
            alpha * 0.72f,
            strike.strong);
    }
}

void drawParticle(Renderer& renderer, const MagicFxSystem::Particle& particle)
{
    const Color color = particleColor(particle);
    if (color.a == 0) {
        return;
    }
    const float size = particleSize(particle);
    if (size <= 0.0f) {
        return;
    }
    const Vec2 center = particleDrawPosition(particle);
    switch (particle.shape) {
    case MagicFxParticleShape::SoftCircle:
        renderer.fillSoftCircle(center, size, color);
        break;
    case MagicFxParticleShape::Circle:
        renderer.fillCircle(center, size, color);
        break;
    case MagicFxParticleShape::Ring:
        renderer.drawSoftRing(center, size, std::max(1.0f, size * 0.18f), color);
        break;
    case MagicFxParticleShape::SparkLine: {
        Vec2 direction = lengthSquared(particle.velocity) > 0.0001f ? normalize(particle.velocity) : fromAngle(particle.rotation);
        const Vec2 half = direction * (size * std::max(1.0f, particle.stretch));
        renderer.drawSoftLine(center - half, center + half, std::max(1.0f, size * 0.32f), color);
        break;
    }
    case MagicFxParticleShape::Shard:
        drawShard(renderer, center, size, particle.rotation, color);
        break;
    case MagicFxParticleShape::Crystal:
        drawCrystal(renderer, center, size, particle.rotation, color);
        break;
    case MagicFxParticleShape::WindCrescent:
        drawWindCrescent(renderer, center, size, particle.rotation, particle.stretch, color);
        break;
    case MagicFxParticleShape::WindSparkle:
        drawWindSparkle(renderer, center, size, particle.rotation, color);
        break;
    }
}

} // namespace

MagicFxEmitterHandle MagicFxSystem::addEmitter(const MagicFxEmitterConfig& config)
{
    return addEmitterWithParent(config, 0);
}

MagicFxEmitterHandle MagicFxSystem::addEmitterWithParent(const MagicFxEmitterConfig& config, std::uint32_t parentId)
{
    if (emitters_.size() >= MaxEmitters) {
        emitters_.erase(emitters_.begin());
    }

    const std::uint32_t id = nextEmitterId_++;
    if (nextEmitterId_ == 0) {
        nextEmitterId_ = 1;
    }

    emitters_.push_back(Emitter{
        .active = true,
        .id = id,
        .parentId = parentId,
        .config = config,
        .age = 0.0f,
        .emitAccumulator = 0.0f,
    });

    if (config.burstCount > 0) {
        spawnParticles(config, config.burstCount);
    }

    return {id};
}

void MagicFxSystem::emitBurst(const MagicFxEmitterConfig& config)
{
    spawnParticles(config, std::max(0, config.burstCount));
}

MagicFxEmitterHandle MagicFxSystem::startFireAura(Vec2 position, float radius)
{
    radius = std::max(4.0f, radius);

    MagicFxEmitterConfig outerFlame;
    outerFlame.position = position;
    outerFlame.direction = {0.0f, -1.0f};
    outerFlame.particleShape = MagicFxParticleShape::SoftCircle;
    outerFlame.spawnShape = MagicFxSpawnShape::Circle;
    outerFlame.startColor = {255, 92, 24, 176};
    outerFlame.endColor = {255, 178, 60, 0};
    outerFlame.speed = {10.0f, 34.0f};
    outerFlame.lifetime = {0.18f, 0.36f};
    outerFlame.startSize = {std::max(1.8f, radius * 0.16f), std::max(3.2f, radius * 0.30f)};
    outerFlame.endSize = {0.0f, std::max(0.8f, radius * 0.07f)};
    outerFlame.height = {0.0f, 4.0f};
    outerFlame.verticalVelocity = {12.0f, 34.0f};
    outerFlame.gravity = 20.0f;
    outerFlame.drag = 2.9f;
    outerFlame.spawnRadius = radius * 0.62f;
    outerFlame.spreadRadians = Pi * 0.92f;
    outerFlame.fadeInFraction = 0.04f;
    outerFlame.fadeOutFraction = 0.72f;
    outerFlame.emissionRate = 58.0f;
    outerFlame.loop = true;
    outerFlame.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(outerFlame);

    MagicFxEmitterConfig core;
    core.position = position;
    core.direction = {0.0f, -1.0f};
    core.particleShape = MagicFxParticleShape::Circle;
    core.spawnShape = MagicFxSpawnShape::Circle;
    core.startColor = {255, 238, 118, 220};
    core.endColor = {255, 112, 28, 0};
    core.speed = {6.0f, 24.0f};
    core.lifetime = {0.10f, 0.22f};
    core.startSize = {std::max(1.2f, radius * 0.08f), std::max(2.2f, radius * 0.16f)};
    core.endSize = {0.0f, std::max(0.6f, radius * 0.04f)};
    core.height = {0.0f, 2.0f};
    core.verticalVelocity = {10.0f, 28.0f};
    core.gravity = 16.0f;
    core.drag = 3.4f;
    core.spawnRadius = radius * 0.38f;
    core.spreadRadians = Pi * 0.62f;
    core.fadeOutFraction = 0.72f;
    core.emissionRate = 44.0f;
    core.loop = true;
    core.depthSorted = true;
    addEmitterWithParent(core, handle.id);

    MagicFxEmitterConfig embers;
    embers.position = position;
    embers.direction = {0.0f, -1.0f};
    embers.particleShape = MagicFxParticleShape::SparkLine;
    embers.spawnShape = MagicFxSpawnShape::Circle;
    embers.startColor = {255, 224, 92, 205};
    embers.endColor = {255, 64, 18, 0};
    embers.speed = {24.0f, 70.0f};
    embers.lifetime = {0.14f, 0.34f};
    embers.startSize = {0.8f, 1.8f};
    embers.endSize = {0.0f, 0.4f};
    embers.height = {0.0f, 3.0f};
    embers.verticalVelocity = {6.0f, 20.0f};
    embers.gravity = 10.0f;
    embers.drag = 1.7f;
    embers.spawnRadius = radius * 0.58f;
    embers.spreadRadians = Pi * 1.20f;
    embers.stretch = 2.1f;
    embers.fadeOutFraction = 0.76f;
    embers.emissionRate = 22.0f;
    embers.loop = true;
    embers.depthSorted = true;
    addEmitterWithParent(embers, handle.id);

    MagicFxEmitterConfig smoke;
    smoke.position = position;
    smoke.direction = {0.0f, -1.0f};
    smoke.particleShape = MagicFxParticleShape::SoftCircle;
    smoke.spawnShape = MagicFxSpawnShape::Circle;
    smoke.startColor = {64, 52, 46, 54};
    smoke.endColor = {40, 36, 36, 0};
    smoke.speed = {3.0f, 12.0f};
    smoke.lifetime = {0.32f, 0.62f};
    smoke.startSize = {radius * 0.10f, radius * 0.20f};
    smoke.endSize = {radius * 0.18f, radius * 0.36f};
    smoke.height = {2.0f, 5.0f};
    smoke.verticalVelocity = {7.0f, 20.0f};
    smoke.gravity = 2.0f;
    smoke.drag = 1.5f;
    smoke.spawnRadius = radius * 0.42f;
    smoke.spreadRadians = Pi * 0.66f;
    smoke.fadeInFraction = 0.18f;
    smoke.fadeOutFraction = 0.74f;
    smoke.emissionRate = 8.0f;
    smoke.loop = true;
    smoke.depthSorted = true;
    addEmitterWithParent(smoke, handle.id);

    return handle;
}

MagicFxEmitterHandle MagicFxSystem::startFireballLoop(Vec2 position, Vec2 direction, float radius)
{
    direction = normalize(direction);
    radius = std::max(3.0f, radius * 0.72f);

    MagicFxEmitterConfig core;
    core.position = position;
    core.direction = direction * -1.0f;
    core.baseVelocity = direction * -28.0f;
    core.particleShape = MagicFxParticleShape::Circle;
    core.spawnShape = MagicFxSpawnShape::Circle;
    core.startColor = {255, 246, 132, 238};
    core.endColor = {255, 114, 30, 0};
    core.speed = {10.0f, 36.0f};
    core.lifetime = {0.09f, 0.20f};
    core.startSize = {std::max(1.2f, radius * 0.16f), std::max(2.4f, radius * 0.30f)};
    core.endSize = {0.0f, std::max(0.45f, radius * 0.05f)};
    core.height = {0.0f, 3.0f};
    core.verticalVelocity = {-2.0f, 8.0f};
    core.gravity = 8.0f;
    core.drag = 3.2f;
    core.spawnRadius = std::max(1.0f, radius * 0.22f);
    core.spreadRadians = Pi * 1.25f;
    core.fadeOutFraction = 0.76f;
    core.emissionRate = 118.0f;
    core.loop = true;
    core.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(core);

    MagicFxEmitterConfig outerFlame;
    outerFlame.position = position;
    outerFlame.direction = direction * -1.0f;
    outerFlame.baseVelocity = direction * -52.0f;
    outerFlame.particleShape = MagicFxParticleShape::SoftCircle;
    outerFlame.spawnShape = MagicFxSpawnShape::Circle;
    outerFlame.startColor = {255, 104, 24, 205};
    outerFlame.endColor = {255, 190, 66, 0};
    outerFlame.speed = {22.0f, 78.0f};
    outerFlame.lifetime = {0.16f, 0.34f};
    outerFlame.startSize = {std::max(1.6f, radius * 0.20f), std::max(3.8f, radius * 0.46f)};
    outerFlame.endSize = {0.0f, std::max(0.55f, radius * 0.08f)};
    outerFlame.height = {0.0f, 5.0f};
    outerFlame.verticalVelocity = {-5.0f, 12.0f};
    outerFlame.gravity = 9.0f;
    outerFlame.drag = 2.0f;
    outerFlame.spawnRadius = std::max(1.4f, radius * 0.42f);
    outerFlame.spreadRadians = Pi * 1.55f;
    outerFlame.fadeOutFraction = 0.80f;
    outerFlame.emissionRate = 96.0f;
    outerFlame.loop = true;
    outerFlame.depthSorted = true;
    addEmitterWithParent(outerFlame, handle.id);

    MagicFxEmitterConfig sparks;
    sparks.position = position;
    sparks.direction = direction * -1.0f;
    sparks.baseVelocity = direction * -66.0f;
    sparks.particleShape = MagicFxParticleShape::SparkLine;
    sparks.spawnShape = MagicFxSpawnShape::Circle;
    sparks.startColor = {255, 236, 112, 230};
    sparks.endColor = {255, 64, 16, 0};
    sparks.speed = {46.0f, 128.0f};
    sparks.lifetime = {0.10f, 0.26f};
    sparks.startSize = {0.55f, 1.35f};
    sparks.endSize = {0.0f, 0.2f};
    sparks.height = {0.0f, 4.0f};
    sparks.verticalVelocity = {-4.0f, 10.0f};
    sparks.gravity = 12.0f;
    sparks.drag = 0.9f;
    sparks.spawnRadius = std::max(1.0f, radius * 0.34f);
    sparks.spreadRadians = Pi * 1.75f;
    sparks.stretch = 2.8f;
    sparks.fadeOutFraction = 0.82f;
    sparks.emissionRate = 42.0f;
    sparks.loop = true;
    sparks.depthSorted = true;
    addEmitterWithParent(sparks, handle.id);

    MagicFxEmitterConfig smoke;
    smoke.position = position;
    smoke.direction = direction * -1.0f;
    smoke.baseVelocity = direction * -28.0f;
    smoke.particleShape = MagicFxParticleShape::SoftCircle;
    smoke.spawnShape = MagicFxSpawnShape::Circle;
    smoke.startColor = {72, 58, 50, 52};
    smoke.endColor = {38, 34, 34, 0};
    smoke.speed = {6.0f, 20.0f};
    smoke.lifetime = {0.28f, 0.56f};
    smoke.startSize = {std::max(1.4f, radius * 0.16f), std::max(3.0f, radius * 0.32f)};
    smoke.endSize = {std::max(2.2f, radius * 0.32f), std::max(4.8f, radius * 0.58f)};
    smoke.height = {2.0f, 7.0f};
    smoke.verticalVelocity = {4.0f, 16.0f};
    smoke.gravity = 2.0f;
    smoke.drag = 1.35f;
    smoke.spawnRadius = std::max(1.0f, radius * 0.34f);
    smoke.spreadRadians = Pi * 1.10f;
    smoke.fadeInFraction = 0.16f;
    smoke.fadeOutFraction = 0.78f;
    smoke.emissionRate = 20.0f;
    smoke.loop = true;
    smoke.depthSorted = true;
    addEmitterWithParent(smoke, handle.id);

    return handle;
}

void MagicFxSystem::playFireGroundBurn(Vec2 position, float radius, float duration)
{
    radius = std::max(4.0f, radius);
    duration = std::max(0.05f, duration);

    MagicFxEmitterConfig ignition;
    ignition.position = position;
    ignition.direction = {0.0f, -1.0f};
    ignition.particleShape = MagicFxParticleShape::SparkLine;
    ignition.spawnShape = MagicFxSpawnShape::Ring;
    ignition.startColor = {255, 222, 92, 238};
    ignition.endColor = {255, 58, 18, 0};
    ignition.speed = {36.0f, 118.0f};
    ignition.lifetime = {0.12f, 0.26f};
    ignition.startSize = {0.9f, 2.6f};
    ignition.endSize = {0.0f, 0.5f};
    ignition.spawnRadius = radius * 0.48f;
    ignition.spreadRadians = Pi * 2.0f;
    ignition.stretch = 3.4f;
    ignition.fadeOutFraction = 0.82f;
    ignition.burstCount = 54;
    ignition.depthSorted = true;
    emitBurst(ignition);

    MagicFxEmitterConfig flash;
    flash.position = position;
    flash.particleShape = MagicFxParticleShape::Ring;
    flash.spawnShape = MagicFxSpawnShape::Point;
    flash.startColor = {255, 190, 70, 150};
    flash.endColor = {255, 84, 20, 0};
    flash.lifetime = {0.14f, 0.18f};
    flash.startSize = {radius * 0.42f, radius * 0.56f};
    flash.endSize = {radius * 0.78f, radius * 0.96f};
    flash.fadeOutFraction = 0.86f;
    flash.burstCount = 1;
    flash.depthSorted = true;
    emitBurst(flash);

    MagicFxEmitterConfig flames;
    flames.position = position;
    flames.direction = {0.0f, -1.0f};
    flames.particleShape = MagicFxParticleShape::SoftCircle;
    flames.spawnShape = MagicFxSpawnShape::Circle;
    flames.startColor = {255, 82, 22, 190};
    flames.endColor = {255, 184, 58, 0};
    flames.speed = {10.0f, 38.0f};
    flames.lifetime = {0.18f, 0.42f};
    flames.startSize = {radius * 0.07f, radius * 0.18f};
    flames.endSize = {0.0f, radius * 0.04f};
    flames.height = {0.0f, 3.0f};
    flames.verticalVelocity = {14.0f, 42.0f};
    flames.gravity = 24.0f;
    flames.drag = 2.8f;
    flames.spawnRadius = radius * 0.78f;
    flames.spreadRadians = Pi * 1.05f;
    flames.fadeInFraction = 0.03f;
    flames.fadeOutFraction = 0.74f;
    flames.emissionRate = 118.0f;
    flames.duration = duration;
    flames.loop = false;
    flames.depthSorted = true;
    addEmitter(flames);

    MagicFxEmitterConfig coreFlames;
    coreFlames.position = position;
    coreFlames.direction = {0.0f, -1.0f};
    coreFlames.particleShape = MagicFxParticleShape::Circle;
    coreFlames.spawnShape = MagicFxSpawnShape::Circle;
    coreFlames.startColor = {255, 240, 118, 220};
    coreFlames.endColor = {255, 102, 26, 0};
    coreFlames.speed = {8.0f, 28.0f};
    coreFlames.lifetime = {0.10f, 0.24f};
    coreFlames.startSize = {radius * 0.04f, radius * 0.10f};
    coreFlames.endSize = {0.0f, radius * 0.025f};
    coreFlames.height = {0.0f, 2.5f};
    coreFlames.verticalVelocity = {12.0f, 36.0f};
    coreFlames.gravity = 20.0f;
    coreFlames.drag = 3.4f;
    coreFlames.spawnRadius = radius * 0.48f;
    coreFlames.spreadRadians = Pi * 0.75f;
    coreFlames.fadeOutFraction = 0.76f;
    coreFlames.emissionRate = 76.0f;
    coreFlames.duration = duration * 0.92f;
    coreFlames.loop = false;
    coreFlames.depthSorted = true;
    addEmitter(coreFlames);

    MagicFxEmitterConfig embers;
    embers.position = position;
    embers.direction = {0.0f, -1.0f};
    embers.particleShape = MagicFxParticleShape::SparkLine;
    embers.spawnShape = MagicFxSpawnShape::Circle;
    embers.startColor = {255, 226, 92, 220};
    embers.endColor = {255, 54, 16, 0};
    embers.speed = {26.0f, 92.0f};
    embers.lifetime = {0.14f, 0.38f};
    embers.startSize = {0.7f, 1.8f};
    embers.endSize = {0.0f, 0.4f};
    embers.height = {0.0f, 4.0f};
    embers.verticalVelocity = {10.0f, 30.0f};
    embers.gravity = 14.0f;
    embers.drag = 1.15f;
    embers.spawnRadius = radius * 0.70f;
    embers.spreadRadians = Pi * 1.55f;
    embers.stretch = 2.6f;
    embers.fadeOutFraction = 0.82f;
    embers.emissionRate = 30.0f;
    embers.duration = duration * 0.86f;
    embers.loop = false;
    embers.depthSorted = true;
    addEmitter(embers);

    MagicFxEmitterConfig smoke;
    smoke.position = position;
    smoke.direction = {0.0f, -1.0f};
    smoke.particleShape = MagicFxParticleShape::SoftCircle;
    smoke.spawnShape = MagicFxSpawnShape::Circle;
    smoke.startColor = {72, 58, 50, 58};
    smoke.endColor = {46, 40, 38, 0};
    smoke.speed = {3.0f, 13.0f};
    smoke.lifetime = {0.40f, 0.86f};
    smoke.startSize = {radius * 0.10f, radius * 0.24f};
    smoke.endSize = {radius * 0.24f, radius * 0.52f};
    smoke.height = {2.0f, 6.0f};
    smoke.verticalVelocity = {8.0f, 22.0f};
    smoke.gravity = 2.0f;
    smoke.drag = 1.6f;
    smoke.spawnRadius = radius * 0.62f;
    smoke.spreadRadians = Pi * 0.82f;
    smoke.fadeInFraction = 0.18f;
    smoke.fadeOutFraction = 0.70f;
    smoke.emissionRate = 24.0f;
    smoke.duration = duration * 0.88f;
    smoke.loop = false;
    smoke.depthSorted = true;
    addEmitter(smoke);
}

MagicFxEmitterHandle MagicFxSystem::startIceAura(Vec2 position, float radius)
{
    radius = std::max(4.0f, radius);

    MagicFxEmitterConfig frost;
    frost.position = position;
    frost.direction = {0.0f, -1.0f};
    frost.particleShape = MagicFxParticleShape::Shard;
    frost.spawnShape = MagicFxSpawnShape::Ring;
    frost.startColor = {156, 226, 255, 176};
    frost.endColor = {228, 252, 255, 0};
    frost.speed = {3.0f, 12.0f};
    frost.lifetime = {0.40f, 0.78f};
    frost.startSize = {std::max(1.6f, radius * 0.10f), std::max(2.8f, radius * 0.18f)};
    frost.endSize = {0.0f, 0.8f};
    frost.rotation = {0.0f, Pi * 2.0f};
    frost.angularVelocity = {-1.6f, 1.6f};
    frost.height = {0.0f, 5.0f};
    frost.verticalVelocity = {1.0f, 7.0f};
    frost.gravity = 2.0f;
    frost.drag = 1.8f;
    frost.spawnRadius = std::max(3.0f, radius * 0.82f);
    frost.spreadRadians = Pi * 1.05f;
    frost.fadeInFraction = 0.12f;
    frost.fadeOutFraction = 0.74f;
    frost.emissionRate = 28.0f;
    frost.loop = true;
    frost.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(frost);

    MagicFxEmitterConfig mist;
    mist.position = position;
    mist.direction = {0.0f, -1.0f};
    mist.particleShape = MagicFxParticleShape::SoftCircle;
    mist.spawnShape = MagicFxSpawnShape::Circle;
    mist.startColor = {180, 236, 255, 48};
    mist.endColor = {234, 252, 255, 0};
    mist.speed = {2.0f, 8.0f};
    mist.lifetime = {0.42f, 0.86f};
    mist.startSize = {radius * 0.12f, radius * 0.24f};
    mist.endSize = {radius * 0.22f, radius * 0.44f};
    mist.height = {0.0f, 4.0f};
    mist.verticalVelocity = {2.0f, 8.0f};
    mist.gravity = 0.8f;
    mist.drag = 1.2f;
    mist.spawnRadius = radius * 0.56f;
    mist.spreadRadians = Pi * 0.82f;
    mist.fadeInFraction = 0.24f;
    mist.fadeOutFraction = 0.76f;
    mist.emissionRate = 16.0f;
    mist.loop = true;
    mist.depthSorted = true;
    addEmitterWithParent(mist, handle.id);

    MagicFxEmitterConfig glints;
    glints.position = position;
    glints.direction = {1.0f, 0.0f};
    glints.particleShape = MagicFxParticleShape::SparkLine;
    glints.spawnShape = MagicFxSpawnShape::Ring;
    glints.startColor = {248, 255, 255, 230};
    glints.endColor = {162, 226, 255, 0};
    glints.speed = {0.0f, 8.0f};
    glints.lifetime = {0.08f, 0.18f};
    glints.startSize = {0.8f, 1.8f};
    glints.endSize = {0.0f, 0.3f};
    glints.rotation = {0.0f, Pi * 2.0f};
    glints.height = {1.0f, 6.0f};
    glints.verticalVelocity = {0.0f, 4.0f};
    glints.gravity = 0.6f;
    glints.drag = 1.0f;
    glints.spawnRadius = radius * 0.86f;
    glints.spreadRadians = Pi * 2.0f;
    glints.stretch = 1.35f;
    glints.fadeInFraction = 0.18f;
    glints.fadeOutFraction = 0.70f;
    glints.emissionRate = 14.0f;
    glints.loop = true;
    glints.depthSorted = true;
    addEmitterWithParent(glints, handle.id);

    return handle;
}

MagicFxEmitterHandle MagicFxSystem::startIceShardLoop(Vec2 position, Vec2 direction, float radius)
{
    direction = normalize(direction);
    const float forwardAngle = std::atan2(direction.y, direction.x);
    radius = std::max(7.0f, radius * 1.52f);

    MagicFxEmitterConfig crystal;
    crystal.position = position;
    crystal.direction = direction * -1.0f;
    crystal.baseVelocity = direction * -12.0f;
    crystal.particleShape = MagicFxParticleShape::Crystal;
    crystal.spawnShape = MagicFxSpawnShape::Circle;
    crystal.startColor = {210, 246, 255, 232};
    crystal.endColor = {110, 204, 255, 0};
    crystal.speed = {2.0f, 10.0f};
    crystal.lifetime = {0.12f, 0.22f};
    crystal.startSize = {std::max(4.4f, radius * 0.54f), std::max(7.0f, radius * 0.84f)};
    crystal.endSize = {std::max(1.0f, radius * 0.12f), std::max(1.8f, radius * 0.20f)};
    crystal.rotation = {forwardAngle - 0.08f, forwardAngle + 0.08f};
    crystal.angularVelocity = {-0.45f, 0.45f};
    crystal.height = {0.0f, 2.0f};
    crystal.verticalVelocity = {-1.0f, 2.0f};
    crystal.drag = 2.4f;
    crystal.spawnRadius = std::max(0.9f, radius * 0.12f);
    crystal.spreadRadians = Pi * 0.22f;
    crystal.fadeOutFraction = 0.66f;
    crystal.emissionRate = 42.0f;
    crystal.loop = true;
    crystal.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(crystal);

    MagicFxEmitterConfig facets;
    facets.position = position;
    facets.direction = direction * -1.0f;
    facets.baseVelocity = direction * -10.0f;
    facets.particleShape = MagicFxParticleShape::Shard;
    facets.spawnShape = MagicFxSpawnShape::Circle;
    facets.startColor = {246, 255, 255, 190};
    facets.endColor = {138, 222, 255, 0};
    facets.speed = {4.0f, 20.0f};
    facets.lifetime = {0.10f, 0.20f};
    facets.startSize = {std::max(1.4f, radius * 0.18f), std::max(2.8f, radius * 0.30f)};
    facets.endSize = {0.0f, 0.4f};
    facets.rotation = {forwardAngle - 0.45f, forwardAngle + 0.45f};
    facets.angularVelocity = {-1.4f, 1.4f};
    facets.height = {0.0f, 3.0f};
    facets.verticalVelocity = {-1.0f, 4.0f};
    facets.drag = 1.8f;
    facets.spawnRadius = std::max(1.4f, radius * 0.36f);
    facets.spreadRadians = Pi * 0.78f;
    facets.fadeOutFraction = 0.76f;
    facets.emissionRate = 32.0f;
    facets.loop = true;
    facets.depthSorted = true;
    addEmitterWithParent(facets, handle.id);

    MagicFxEmitterConfig mist;
    mist.position = position;
    mist.direction = direction * -1.0f;
    mist.baseVelocity = direction * -28.0f;
    mist.particleShape = MagicFxParticleShape::SparkLine;
    mist.spawnShape = MagicFxSpawnShape::Point;
    mist.startColor = {174, 232, 255, 158};
    mist.endColor = {224, 252, 255, 0};
    mist.speed = {16.0f, 46.0f};
    mist.lifetime = {0.16f, 0.32f};
    mist.startSize = {std::max(1.0f, radius * 0.12f), std::max(2.0f, radius * 0.22f)};
    mist.endSize = {0.0f, 0.55f};
    mist.height = {0.0f, 2.0f};
    mist.verticalVelocity = {-2.0f, 4.0f};
    mist.drag = 1.6f;
    mist.spreadRadians = Pi * 0.68f;
    mist.stretch = 3.0f;
    mist.fadeOutFraction = 0.78f;
    mist.emissionRate = 40.0f;
    mist.loop = true;
    mist.depthSorted = true;
    addEmitterWithParent(mist, handle.id);

    MagicFxEmitterConfig powder;
    powder.position = position;
    powder.direction = direction * -1.0f;
    powder.baseVelocity = direction * -18.0f;
    powder.particleShape = MagicFxParticleShape::Circle;
    powder.spawnShape = MagicFxSpawnShape::Circle;
    powder.startColor = {244, 255, 255, 190};
    powder.endColor = {170, 230, 255, 0};
    powder.speed = {8.0f, 30.0f};
    powder.lifetime = {0.18f, 0.38f};
    powder.startSize = {0.8f, 1.8f};
    powder.endSize = {0.0f, 0.35f};
    powder.height = {0.0f, 3.0f};
    powder.verticalVelocity = {-1.0f, 5.0f};
    powder.gravity = 1.0f;
    powder.drag = 1.6f;
    powder.spawnRadius = std::max(1.4f, radius * 0.42f);
    powder.spreadRadians = Pi * 1.10f;
    powder.fadeOutFraction = 0.82f;
    powder.emissionRate = 26.0f;
    powder.loop = true;
    powder.depthSorted = true;
    addEmitterWithParent(powder, handle.id);

    MagicFxEmitterConfig glints;
    glints.position = position;
    glints.direction = direction * -1.0f;
    glints.baseVelocity = direction * -24.0f;
    glints.particleShape = MagicFxParticleShape::SparkLine;
    glints.spawnShape = MagicFxSpawnShape::Circle;
    glints.startColor = {255, 255, 255, 235};
    glints.endColor = {150, 224, 255, 0};
    glints.speed = {18.0f, 58.0f};
    glints.lifetime = {0.06f, 0.14f};
    glints.startSize = {0.55f, 1.3f};
    glints.endSize = {0.0f, 0.25f};
    glints.rotation = {forwardAngle - 0.4f, forwardAngle + 0.4f};
    glints.height = {0.0f, 3.0f};
    glints.verticalVelocity = {-1.0f, 4.0f};
    glints.drag = 1.2f;
    glints.spawnRadius = std::max(0.8f, radius * 0.24f);
    glints.spreadRadians = Pi * 0.75f;
    glints.stretch = 1.5f;
    glints.fadeOutFraction = 0.72f;
    glints.emissionRate = 18.0f;
    glints.loop = true;
    glints.depthSorted = true;
    addEmitterWithParent(glints, handle.id);

    return handle;
}

void MagicFxSystem::playIceShatter(Vec2 position, float radius)
{
    radius = std::max(8.0f, radius * 1.22f);

    MagicFxEmitterConfig largeShards;
    largeShards.position = position;
    largeShards.direction = {1.0f, 0.0f};
    largeShards.particleShape = MagicFxParticleShape::Shard;
    largeShards.spawnShape = MagicFxSpawnShape::Circle;
    largeShards.startColor = {176, 236, 255, 235};
    largeShards.endColor = {235, 252, 255, 0};
    largeShards.speed = {50.0f, 154.0f};
    largeShards.lifetime = {0.42f, 0.94f};
    largeShards.startSize = {3.2f, 7.2f};
    largeShards.endSize = {0.0f, 1.0f};
    largeShards.rotation = {0.0f, Pi * 2.0f};
    largeShards.angularVelocity = {-8.0f, 8.0f};
    largeShards.height = {0.0f, 5.0f};
    largeShards.verticalVelocity = {5.0f, 26.0f};
    largeShards.gravity = 34.0f;
    largeShards.drag = 1.25f;
    largeShards.spawnRadius = radius * 0.22f;
    largeShards.spreadRadians = Pi * 2.0f;
    largeShards.fadeOutFraction = 0.78f;
    largeShards.burstCount = 24;
    largeShards.depthSorted = true;
    emitBurst(largeShards);

    MagicFxEmitterConfig smallShards = largeShards;
    smallShards.startColor = {232, 252, 255, 220};
    smallShards.endColor = {142, 222, 255, 0};
    smallShards.speed = {66.0f, 196.0f};
    smallShards.lifetime = {0.24f, 0.64f};
    smallShards.startSize = {1.0f, 2.8f};
    smallShards.endSize = {0.0f, 0.35f};
    smallShards.verticalVelocity = {2.0f, 18.0f};
    smallShards.gravity = 28.0f;
    smallShards.drag = 1.0f;
    smallShards.spawnRadius = radius * 0.30f;
    smallShards.burstCount = 58;
    emitBurst(smallShards);

    MagicFxEmitterConfig flash;
    flash.position = position;
    flash.particleShape = MagicFxParticleShape::Ring;
    flash.spawnShape = MagicFxSpawnShape::Point;
    flash.startColor = {226, 252, 255, 188};
    flash.endColor = {210, 246, 255, 0};
    flash.lifetime = {0.18f, 0.34f};
    flash.startSize = {radius * 0.52f, radius * 0.72f};
    flash.endSize = {radius * 1.35f, radius * 1.68f};
    flash.fadeOutFraction = 0.9f;
    flash.burstCount = 1;
    flash.depthSorted = true;
    emitBurst(flash);

    MagicFxEmitterConfig glints;
    glints.position = position;
    glints.direction = {1.0f, 0.0f};
    glints.particleShape = MagicFxParticleShape::SparkLine;
    glints.spawnShape = MagicFxSpawnShape::Ring;
    glints.startColor = {255, 255, 255, 240};
    glints.endColor = {156, 226, 255, 0};
    glints.speed = {8.0f, 42.0f};
    glints.lifetime = {0.16f, 0.42f};
    glints.startSize = {0.8f, 2.0f};
    glints.endSize = {0.0f, 0.3f};
    glints.rotation = {0.0f, Pi * 2.0f};
    glints.height = {1.0f, 6.0f};
    glints.verticalVelocity = {2.0f, 16.0f};
    glints.gravity = 10.0f;
    glints.drag = 0.8f;
    glints.spawnRadius = radius * 0.70f;
    glints.spreadRadians = Pi * 2.0f;
    glints.stretch = 1.5f;
    glints.fadeInFraction = 0.08f;
    glints.fadeOutFraction = 0.76f;
    glints.burstCount = 34;
    glints.depthSorted = true;
    emitBurst(glints);

    MagicFxEmitterConfig mist;
    mist.position = position;
    mist.direction = {0.0f, -1.0f};
    mist.particleShape = MagicFxParticleShape::SoftCircle;
    mist.spawnShape = MagicFxSpawnShape::Circle;
    mist.startColor = {190, 238, 255, 72};
    mist.endColor = {236, 252, 255, 0};
    mist.speed = {4.0f, 18.0f};
    mist.lifetime = {0.62f, 1.24f};
    mist.startSize = {radius * 0.14f, radius * 0.30f};
    mist.endSize = {radius * 0.34f, radius * 0.66f};
    mist.height = {0.0f, 4.0f};
    mist.verticalVelocity = {3.0f, 14.0f};
    mist.gravity = 1.0f;
    mist.drag = 1.3f;
    mist.spawnRadius = radius * 0.48f;
    mist.spreadRadians = Pi * 0.92f;
    mist.fadeInFraction = 0.20f;
    mist.fadeOutFraction = 0.78f;
    mist.burstCount = 26;
    mist.depthSorted = true;
    emitBurst(mist);
}

MagicFxEmitterHandle MagicFxSystem::startThunderAura(Vec2 position, float radius)
{
    MagicFxEmitterConfig sparks;
    sparks.position = position;
    sparks.direction = {1.0f, 0.0f};
    sparks.particleShape = MagicFxParticleShape::SparkLine;
    sparks.spawnShape = MagicFxSpawnShape::Ring;
    sparks.startColor = {255, 248, 148, 230};
    sparks.endColor = {136, 202, 255, 0};
    sparks.speed = {10.0f, 34.0f};
    sparks.lifetime = {0.08f, 0.18f};
    sparks.startSize = {1.8f, 3.2f};
    sparks.endSize = {0.0f, 0.6f};
    sparks.rotation = {0.0f, Pi * 2.0f};
    sparks.spawnRadius = std::max(3.0f, radius * 0.82f);
    sparks.spreadRadians = Pi * 2.0f;
    sparks.stretch = 3.2f;
    sparks.fadeOutFraction = 0.82f;
    sparks.emissionRate = 42.0f;
    sparks.loop = true;
    sparks.depthSorted = true;
    return addEmitter(sparks);
}

void MagicFxSystem::playThunderStrike(Vec2 origin, Vec2 target, bool strong)
{
    LightningStrike strike;
    strike.active = true;
    strike.strong = strong;
    strike.lifetime = strong ? sampleRange({0.16f, 0.22f}) : sampleRange({0.12f, 0.17f});
    strike.outerWidth = strong ? sampleRange({5.4f, 8.4f}) : sampleRange({3.2f, 5.2f});
    strike.coreWidth = strong ? sampleRange({1.4f, 2.4f}) : sampleRange({1.0f, 1.6f});

    Vec2 sourceBias = origin - target;
    if (lengthSquared(sourceBias) <= 0.0001f) {
        sourceBias = {random01() - 0.5f, random01() - 0.5f};
    }
    const float skyHeight = strong ? sampleRange({148.0f, 190.0f}) : sampleRange({108.0f, 146.0f});
    const float startOffsetX = std::clamp(sourceBias.x * 0.18f + (random01() - 0.5f) * (strong ? 52.0f : 34.0f), strong ? -58.0f : -38.0f, strong ? 58.0f : 38.0f);
    const Vec2 start = target + Vec2{startOffsetX, -skyHeight};
    const Vec2 delta = target - start;
    const Vec2 direction = normalize(delta);
    const Vec2 side = perpendicular(direction);
    const int segmentCount = strong ? 8 : 6;
    strike.pointCount = segmentCount + 1;
    strike.points[0] = start;
    for (int i = 1; i < segmentCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segmentCount);
        const float centerWeight = 1.0f - std::abs(t - 0.5f) * 0.72f;
        const float amplitude = (strong ? 38.0f : 22.0f) * centerWeight;
        const float offset = (random01() - 0.5f) * amplitude;
        const float alongJitter = (random01() - 0.5f) * (strong ? 10.0f : 6.0f);
        strike.points[static_cast<std::size_t>(i)] = start + delta * t + side * offset + direction * alongJitter;
    }
    strike.points[static_cast<std::size_t>(segmentCount)] = target;

    strike.branchCount = strong ? 6 : 3;
    for (int i = 0; i < strike.branchCount; ++i) {
        LightningBranch& branch = strike.branches[static_cast<std::size_t>(i)];
        const int baseIndex = 1 + static_cast<int>(random01() * static_cast<float>(std::max(1, segmentCount - 2)));
        const Vec2 base = strike.points[static_cast<std::size_t>(std::min(baseIndex, segmentCount - 1))];
        const float sign = random01() < 0.5f ? -1.0f : 1.0f;
        const float lengthValue = strong ? sampleRange({24.0f, 58.0f}) : sampleRange({14.0f, 34.0f});
        const Vec2 branchDirection = normalize(direction * sampleRange({0.08f, 0.38f}) + side * sign * sampleRange({0.72f, 1.12f}));
        branch.pointCount = 3;
        branch.width = strike.outerWidth * sampleRange({0.28f, 0.46f});
        branch.points[0] = base;
        branch.points[1] = base + branchDirection * (lengthValue * 0.52f) + side * ((random01() - 0.5f) * lengthValue * 0.22f);
        branch.points[2] = base + branchDirection * lengthValue + direction * ((random01() - 0.5f) * lengthValue * 0.20f);
    }

    if (lightningStrikes_.size() >= MaxLightningStrikes) {
        lightningStrikes_.erase(lightningStrikes_.begin());
    }
    lightningStrikes_.push_back(strike);

    MagicFxEmitterConfig flash;
    flash.position = target;
    flash.particleShape = MagicFxParticleShape::Ring;
    flash.spawnShape = MagicFxSpawnShape::Point;
    flash.startColor = strong ? Color{255, 244, 118, 180} : Color{210, 232, 255, 118};
    flash.endColor = {132, 198, 255, 0};
    flash.lifetime = strong ? MagicFxRange{0.16f, 0.26f} : MagicFxRange{0.12f, 0.20f};
    flash.startSize = strong ? MagicFxRange{8.0f, 14.0f} : MagicFxRange{5.0f, 9.0f};
    flash.endSize = strong ? MagicFxRange{30.0f, 46.0f} : MagicFxRange{18.0f, 28.0f};
    flash.fadeOutFraction = 0.88f;
    flash.burstCount = 2;
    flash.depthSorted = true;
    emitBurst(flash);

    MagicFxEmitterConfig sparks;
    sparks.position = target;
    sparks.direction = {1.0f, 0.0f};
    sparks.particleShape = MagicFxParticleShape::SparkLine;
    sparks.spawnShape = MagicFxSpawnShape::Circle;
    sparks.startColor = strong ? Color{255, 252, 168, 238} : Color{208, 232, 255, 196};
    sparks.endColor = {112, 190, 255, 0};
    sparks.speed = strong ? MagicFxRange{34.0f, 126.0f} : MagicFxRange{18.0f, 72.0f};
    sparks.lifetime = strong ? MagicFxRange{0.14f, 0.34f} : MagicFxRange{0.10f, 0.24f};
    sparks.startSize = strong ? MagicFxRange{1.0f, 2.6f} : MagicFxRange{0.7f, 1.8f};
    sparks.endSize = {0.0f, 0.35f};
    sparks.spawnRadius = strong ? 7.0f : 4.0f;
    sparks.spreadRadians = Pi * 2.0f;
    sparks.stretch = strong ? 3.2f : 2.3f;
    sparks.fadeOutFraction = 0.82f;
    sparks.burstCount = strong ? 28 : 14;
    sparks.depthSorted = true;
    emitBurst(sparks);
}

void MagicFxSystem::playThunderBolt(Vec2 origin, Vec2 target)
{
    playThunderStrike(origin, target, true);
}

void MagicFxSystem::playThunderSparkBurst(Vec2 position, float radius)
{
    MagicFxEmitterConfig burst;
    burst.position = position;
    burst.direction = {1.0f, 0.0f};
    burst.particleShape = MagicFxParticleShape::SparkLine;
    burst.spawnShape = MagicFxSpawnShape::Circle;
    burst.startColor = {255, 252, 168, 235};
    burst.endColor = {138, 206, 255, 0};
    burst.speed = {32.0f, 110.0f};
    burst.lifetime = {0.10f, 0.24f};
    burst.startSize = {1.8f, 3.8f};
    burst.endSize = {0.0f, 0.8f};
    burst.spawnRadius = std::max(1.0f, radius * 0.22f);
    burst.spreadRadians = Pi * 2.0f;
    burst.stretch = 3.6f;
    burst.fadeOutFraction = 0.86f;
    burst.burstCount = 18;
    burst.depthSorted = true;
    emitBurst(burst);
}

MagicFxEmitterHandle MagicFxSystem::startWindAura(Vec2 position, float radius)
{
    radius = std::max(4.0f, radius);

    MagicFxEmitterConfig crescents;
    crescents.position = position;
    crescents.direction = {1.0f, 0.0f};
    crescents.particleShape = MagicFxParticleShape::WindCrescent;
    crescents.spawnShape = MagicFxSpawnShape::Ring;
    crescents.startColor = {172, 252, 204, 118};
    crescents.endColor = {232, 255, 238, 0};
    crescents.speed = {6.0f, 20.0f};
    crescents.lifetime = {0.28f, 0.56f};
    crescents.startSize = {std::max(2.2f, radius * 0.18f), std::max(3.8f, radius * 0.32f)};
    crescents.endSize = {0.0f, std::max(1.2f, radius * 0.12f)};
    crescents.rotation = {0.0f, Pi * 2.0f};
    crescents.angularVelocity = {-2.2f, 2.2f};
    crescents.spawnRadius = radius * 0.90f;
    crescents.spreadRadians = Pi * 2.0f;
    crescents.stretch = 2.1f;
    crescents.fadeInFraction = 0.14f;
    crescents.fadeOutFraction = 0.70f;
    crescents.emissionRate = 22.0f;
    crescents.loop = true;
    crescents.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(crescents);

    MagicFxEmitterConfig mist;
    mist.position = position;
    mist.direction = {1.0f, 0.0f};
    mist.particleShape = MagicFxParticleShape::SoftCircle;
    mist.spawnShape = MagicFxSpawnShape::Circle;
    mist.startColor = {174, 246, 204, 38};
    mist.endColor = {232, 255, 236, 0};
    mist.speed = {4.0f, 18.0f};
    mist.lifetime = {0.22f, 0.52f};
    mist.startSize = {0.55f, 1.25f};
    mist.endSize = {0.9f, 2.4f};
    mist.spawnRadius = radius * 0.82f;
    mist.spreadRadians = Pi * 2.0f;
    mist.fadeInFraction = 0.18f;
    mist.fadeOutFraction = 0.78f;
    mist.emissionRate = 58.0f;
    mist.loop = true;
    mist.depthSorted = true;
    addEmitterWithParent(mist, handle.id);

    MagicFxEmitterConfig sparkle;
    sparkle.position = position;
    sparkle.direction = {1.0f, 0.0f};
    sparkle.particleShape = MagicFxParticleShape::WindSparkle;
    sparkle.spawnShape = MagicFxSpawnShape::Ring;
    sparkle.startColor = {244, 255, 228, 210};
    sparkle.endColor = {146, 246, 190, 0};
    sparkle.speed = {2.0f, 12.0f};
    sparkle.lifetime = {0.10f, 0.24f};
    sparkle.startSize = {0.8f, 1.7f};
    sparkle.endSize = {0.0f, 0.25f};
    sparkle.rotation = {0.0f, Pi * 2.0f};
    sparkle.angularVelocity = {-1.4f, 1.4f};
    sparkle.spawnRadius = radius * 0.96f;
    sparkle.spreadRadians = Pi * 2.0f;
    sparkle.fadeInFraction = 0.16f;
    sparkle.fadeOutFraction = 0.72f;
    sparkle.emissionRate = 14.0f;
    sparkle.loop = true;
    sparkle.depthSorted = true;
    addEmitterWithParent(sparkle, handle.id);

    return handle;
}

MagicFxEmitterHandle MagicFxSystem::startWindWaveLoop(Vec2 position, Vec2 direction, float radius)
{
    direction = normalize(direction);
    radius = std::max(8.0f, radius);
    const float angle = std::atan2(direction.y, direction.x);

    MagicFxEmitterConfig crescent;
    crescent.position = position;
    crescent.direction = direction * -1.0f;
    crescent.baseVelocity = direction * -18.0f;
    crescent.particleShape = MagicFxParticleShape::WindCrescent;
    crescent.spawnShape = MagicFxSpawnShape::Circle;
    crescent.startColor = {180, 255, 206, 152};
    crescent.endColor = {235, 255, 238, 0};
    crescent.speed = {0.0f, 6.0f};
    crescent.lifetime = {0.26f, 0.42f};
    crescent.startSize = {radius * 0.46f, radius * 0.66f};
    crescent.endSize = {radius * 0.26f, radius * 0.46f};
    crescent.rotation = {angle, angle};
    crescent.angularVelocity = {0.0f, 0.0f};
    crescent.spawnRadius = radius * 0.06f;
    crescent.spreadRadians = Pi * 0.16f;
    crescent.stretch = 2.85f;
    crescent.fadeInFraction = 0.06f;
    crescent.fadeOutFraction = 0.82f;
    crescent.emissionRate = 18.0f;
    crescent.loop = true;
    crescent.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(crescent);

    MagicFxEmitterConfig mist;
    mist.position = position;
    mist.direction = direction * -1.0f;
    mist.baseVelocity = direction * -22.0f;
    mist.particleShape = MagicFxParticleShape::SoftCircle;
    mist.spawnShape = MagicFxSpawnShape::Cone;
    mist.startColor = {170, 246, 204, 44};
    mist.endColor = {232, 255, 236, 0};
    mist.speed = {10.0f, 34.0f};
    mist.lifetime = {0.18f, 0.42f};
    mist.startSize = {0.65f, 1.55f};
    mist.endSize = {1.0f, 2.8f};
    mist.spawnRadius = radius * 1.02f;
    mist.spreadRadians = Pi * 0.95f;
    mist.fadeInFraction = 0.18f;
    mist.fadeOutFraction = 0.78f;
    mist.emissionRate = 118.0f;
    mist.loop = true;
    mist.depthSorted = true;
    addEmitterWithParent(mist, handle.id);

    MagicFxEmitterConfig sparkle;
    sparkle.position = position;
    sparkle.direction = direction * -1.0f;
    sparkle.baseVelocity = direction * -10.0f;
    sparkle.particleShape = MagicFxParticleShape::WindSparkle;
    sparkle.spawnShape = MagicFxSpawnShape::Cone;
    sparkle.startColor = {246, 255, 230, 230};
    sparkle.endColor = {132, 244, 184, 0};
    sparkle.speed = {4.0f, 20.0f};
    sparkle.lifetime = {0.08f, 0.20f};
    sparkle.startSize = {0.75f, 1.85f};
    sparkle.endSize = {0.0f, 0.22f};
    sparkle.rotation = {angle - 0.35f, angle + 0.35f};
    sparkle.angularVelocity = {-1.0f, 1.0f};
    sparkle.spawnRadius = radius * 0.88f;
    sparkle.spreadRadians = Pi * 0.70f;
    sparkle.fadeInFraction = 0.12f;
    sparkle.fadeOutFraction = 0.78f;
    sparkle.emissionRate = 38.0f;
    sparkle.loop = true;
    sparkle.depthSorted = true;
    addEmitterWithParent(sparkle, handle.id);

    return handle;
}

void MagicFxSystem::playWindImpact(Vec2 position, Vec2 direction, float radius)
{
    direction = normalize(direction);
    radius = std::max(6.0f, radius);
    const float angle = std::atan2(direction.y, direction.x);

    MagicFxEmitterConfig crescents;
    crescents.position = position;
    crescents.direction = direction;
    crescents.baseVelocity = direction * 26.0f;
    crescents.particleShape = MagicFxParticleShape::WindCrescent;
    crescents.spawnShape = MagicFxSpawnShape::Cone;
    crescents.startColor = {205, 255, 218, 172};
    crescents.endColor = {232, 255, 236, 0};
    crescents.speed = {18.0f, 76.0f};
    crescents.lifetime = {0.18f, 0.42f};
    crescents.startSize = {radius * 0.38f, radius * 0.70f};
    crescents.endSize = {radius * 0.10f, radius * 0.34f};
    crescents.rotation = {angle - 0.12f, angle + 0.12f};
    crescents.angularVelocity = {-0.8f, 0.8f};
    crescents.spawnRadius = radius * 0.48f;
    crescents.spreadRadians = Pi * 0.82f;
    crescents.stretch = 2.45f;
    crescents.fadeOutFraction = 0.86f;
    crescents.burstCount = 8;
    crescents.depthSorted = true;
    emitBurst(crescents);

    MagicFxEmitterConfig mist;
    mist.position = position;
    mist.direction = direction;
    mist.particleShape = MagicFxParticleShape::SoftCircle;
    mist.spawnShape = MagicFxSpawnShape::Cone;
    mist.startColor = {176, 246, 204, 50};
    mist.endColor = {232, 255, 236, 0};
    mist.speed = {22.0f, 78.0f};
    mist.lifetime = {0.18f, 0.46f};
    mist.startSize = {0.75f, 1.8f};
    mist.endSize = {1.2f, 3.2f};
    mist.spawnRadius = radius * 0.86f;
    mist.spreadRadians = Pi * 0.88f;
    mist.fadeInFraction = 0.12f;
    mist.fadeOutFraction = 0.76f;
    mist.burstCount = 64;
    mist.depthSorted = true;
    emitBurst(mist);

    MagicFxEmitterConfig ring;
    ring.position = position;
    ring.particleShape = MagicFxParticleShape::Ring;
    ring.spawnShape = MagicFxSpawnShape::Point;
    ring.startColor = {220, 255, 226, 70};
    ring.endColor = {180, 244, 204, 0};
    ring.lifetime = {0.16f, 0.26f};
    ring.startSize = {radius * 0.34f, radius * 0.46f};
    ring.endSize = {radius * 0.82f, radius * 1.08f};
    ring.fadeOutFraction = 0.9f;
    ring.burstCount = 1;
    ring.depthSorted = true;
    emitBurst(ring);

    MagicFxEmitterConfig sparkle;
    sparkle.position = position;
    sparkle.direction = direction;
    sparkle.baseVelocity = direction * 18.0f;
    sparkle.particleShape = MagicFxParticleShape::WindSparkle;
    sparkle.spawnShape = MagicFxSpawnShape::Cone;
    sparkle.startColor = {248, 255, 232, 235};
    sparkle.endColor = {142, 244, 188, 0};
    sparkle.speed = {12.0f, 56.0f};
    sparkle.lifetime = {0.10f, 0.26f};
    sparkle.startSize = {0.8f, 2.1f};
    sparkle.endSize = {0.0f, 0.24f};
    sparkle.rotation = {angle - 0.55f, angle + 0.55f};
    sparkle.angularVelocity = {-2.4f, 2.4f};
    sparkle.spawnRadius = radius * 0.64f;
    sparkle.spreadRadians = Pi * 0.86f;
    sparkle.fadeInFraction = 0.10f;
    sparkle.fadeOutFraction = 0.80f;
    sparkle.burstCount = 22;
    sparkle.depthSorted = true;
    emitBurst(sparkle);
}

MagicFxEmitterHandle MagicFxSystem::startEarthAura(Vec2 position, float radius)
{
    MagicFxEmitterConfig rocks;
    rocks.position = position;
    rocks.direction = {0.0f, -1.0f};
    rocks.particleShape = MagicFxParticleShape::Shard;
    rocks.spawnShape = MagicFxSpawnShape::Ring;
    rocks.startColor = {180, 126, 76, 210};
    rocks.endColor = {92, 66, 46, 0};
    rocks.speed = {2.0f, 10.0f};
    rocks.lifetime = {0.42f, 0.82f};
    rocks.startSize = {2.0f, 4.0f};
    rocks.endSize = {1.0f, 2.4f};
    rocks.rotation = {0.0f, Pi * 2.0f};
    rocks.angularVelocity = {-2.8f, 2.8f};
    rocks.height = {2.0f, 8.0f};
    rocks.verticalVelocity = {-3.0f, 6.0f};
    rocks.gravity = 4.0f;
    rocks.drag = 1.2f;
    rocks.spawnRadius = std::max(4.0f, radius * 0.84f);
    rocks.spreadRadians = Pi * 2.0f;
    rocks.fadeInFraction = 0.12f;
    rocks.fadeOutFraction = 0.62f;
    rocks.emissionRate = 18.0f;
    rocks.loop = true;
    rocks.depthSorted = true;
    return addEmitter(rocks);
}

MagicFxEmitterHandle MagicFxSystem::startDirtClodLoop(Vec2 position, Vec2 direction, float radius)
{
    direction = normalize(direction);

    MagicFxEmitterConfig dust;
    dust.position = position;
    dust.direction = direction * -1.0f;
    dust.baseVelocity = direction * -18.0f;
    dust.particleShape = MagicFxParticleShape::SoftCircle;
    dust.spawnShape = MagicFxSpawnShape::Circle;
    dust.startColor = {150, 104, 66, 98};
    dust.endColor = {92, 70, 54, 0};
    dust.speed = {4.0f, 24.0f};
    dust.lifetime = {0.24f, 0.46f};
    dust.startSize = {radius * 0.42f, radius * 0.82f};
    dust.endSize = {radius * 0.72f, radius * 1.30f};
    dust.height = {0.0f, 2.0f};
    dust.verticalVelocity = {2.0f, 10.0f};
    dust.gravity = 8.0f;
    dust.drag = 2.0f;
    dust.spawnRadius = std::max(1.0f, radius * 0.45f);
    dust.spreadRadians = Pi * 0.95f;
    dust.fadeInFraction = 0.12f;
    dust.fadeOutFraction = 0.72f;
    dust.emissionRate = 32.0f;
    dust.loop = true;
    dust.depthSorted = true;
    return addEmitter(dust);
}

void MagicFxSystem::playEarthSpikeRise(Vec2 position, float radius)
{
    radius = std::max(8.0f, radius);

    MagicFxEmitterConfig spikeBody;
    spikeBody.position = position;
    spikeBody.direction = {0.0f, -1.0f};
    spikeBody.particleShape = MagicFxParticleShape::Shard;
    spikeBody.spawnShape = MagicFxSpawnShape::Circle;
    spikeBody.startColor = {132, 92, 58, 235};
    spikeBody.endColor = {84, 60, 42, 0};
    spikeBody.speed = {0.0f, 8.0f};
    spikeBody.lifetime = {0.42f, 0.58f};
    spikeBody.startSize = {radius * 0.62f, radius * 0.92f};
    spikeBody.endSize = {radius * 0.42f, radius * 0.70f};
    spikeBody.rotation = {-Pi * 0.62f, -Pi * 0.38f};
    spikeBody.angularVelocity = {-0.8f, 0.8f};
    spikeBody.height = {14.0f, 30.0f};
    spikeBody.verticalVelocity = {0.0f, 14.0f};
    spikeBody.gravity = 28.0f;
    spikeBody.drag = 1.4f;
    spikeBody.spawnRadius = radius * 0.20f;
    spikeBody.spreadRadians = Pi * 0.50f;
    spikeBody.fadeInFraction = 0.04f;
    spikeBody.fadeOutFraction = 0.48f;
    spikeBody.burstCount = 5;
    spikeBody.depthSorted = true;
    emitBurst(spikeBody);

    MagicFxEmitterConfig dust;
    dust.position = position;
    dust.direction = {0.0f, -1.0f};
    dust.particleShape = MagicFxParticleShape::SoftCircle;
    dust.spawnShape = MagicFxSpawnShape::Ring;
    dust.startColor = {132, 92, 58, 135};
    dust.endColor = {80, 62, 48, 0};
    dust.speed = {18.0f, 72.0f};
    dust.lifetime = {0.24f, 0.52f};
    dust.startSize = {radius * 0.10f, radius * 0.24f};
    dust.endSize = {radius * 0.24f, radius * 0.52f};
    dust.height = {0.0f, 2.0f};
    dust.verticalVelocity = {8.0f, 28.0f};
    dust.gravity = 22.0f;
    dust.drag = 1.6f;
    dust.spawnRadius = radius * 0.42f;
    dust.spreadRadians = Pi * 1.0f;
    dust.fadeOutFraction = 0.76f;
    dust.burstCount = 30;
    dust.depthSorted = true;
    emitBurst(dust);

    MagicFxEmitterConfig shards;
    shards.position = position;
    shards.direction = {0.0f, -1.0f};
    shards.particleShape = MagicFxParticleShape::Shard;
    shards.spawnShape = MagicFxSpawnShape::Circle;
    shards.startColor = {170, 122, 74, 220};
    shards.endColor = {82, 58, 38, 0};
    shards.speed = {22.0f, 90.0f};
    shards.lifetime = {0.22f, 0.48f};
    shards.startSize = {2.8f, 6.4f};
    shards.endSize = {0.0f, 1.4f};
    shards.rotation = {0.0f, Pi * 2.0f};
    shards.angularVelocity = {-7.0f, 7.0f};
    shards.height = {0.0f, 5.0f};
    shards.verticalVelocity = {20.0f, 70.0f};
    shards.gravity = 150.0f;
    shards.drag = 0.8f;
    shards.spawnRadius = radius * 0.24f;
    shards.spreadRadians = Pi * 1.2f;
    shards.fadeOutFraction = 0.72f;
    shards.burstCount = 16;
    shards.depthSorted = true;
    emitBurst(shards);
}

void MagicFxSystem::playEarthDebrisBurst(Vec2 position, float radius)
{
    radius = std::max(5.0f, radius);

    MagicFxEmitterConfig debris;
    debris.position = position;
    debris.direction = {1.0f, 0.0f};
    debris.particleShape = MagicFxParticleShape::Shard;
    debris.spawnShape = MagicFxSpawnShape::Circle;
    debris.startColor = {172, 118, 70, 220};
    debris.endColor = {78, 58, 42, 0};
    debris.speed = {26.0f, 96.0f};
    debris.lifetime = {0.22f, 0.46f};
    debris.startSize = {2.4f, 5.2f};
    debris.endSize = {0.0f, 1.0f};
    debris.rotation = {0.0f, Pi * 2.0f};
    debris.angularVelocity = {-8.0f, 8.0f};
    debris.height = {0.0f, 5.0f};
    debris.verticalVelocity = {8.0f, 42.0f};
    debris.gravity = 90.0f;
    debris.drag = 1.0f;
    debris.spawnRadius = radius * 0.30f;
    debris.spreadRadians = Pi * 2.0f;
    debris.fadeOutFraction = 0.76f;
    debris.burstCount = 20;
    debris.depthSorted = true;
    emitBurst(debris);

    MagicFxEmitterConfig dust;
    dust.position = position;
    dust.direction = {0.0f, -1.0f};
    dust.particleShape = MagicFxParticleShape::SoftCircle;
    dust.spawnShape = MagicFxSpawnShape::Circle;
    dust.startColor = {126, 96, 70, 92};
    dust.endColor = {78, 62, 48, 0};
    dust.speed = {4.0f, 28.0f};
    dust.lifetime = {0.28f, 0.58f};
    dust.startSize = {radius * 0.28f, radius * 0.62f};
    dust.endSize = {radius * 0.58f, radius * 1.05f};
    dust.height = {0.0f, 3.0f};
    dust.verticalVelocity = {4.0f, 18.0f};
    dust.gravity = 8.0f;
    dust.drag = 1.8f;
    dust.spawnRadius = radius * 0.38f;
    dust.spreadRadians = Pi * 1.2f;
    dust.fadeInFraction = 0.12f;
    dust.fadeOutFraction = 0.72f;
    dust.burstCount = 12;
    dust.depthSorted = true;
    emitBurst(dust);
}

bool MagicFxSystem::setEmitterPosition(MagicFxEmitterHandle handle, Vec2 position)
{
    if (!handle.valid()) {
        return false;
    }
    bool updated = false;
    for (Emitter& emitter : emitters_) {
        if (emitter.active && (emitter.id == handle.id || emitter.parentId == handle.id)) {
            emitter.config.position = position;
            updated = true;
        }
    }
    return updated;
}

bool MagicFxSystem::stopEmitter(MagicFxEmitterHandle handle)
{
    if (!handle.valid()) {
        return false;
    }
    bool stopped = false;
    for (Emitter& emitter : emitters_) {
        if (emitter.active && (emitter.id == handle.id || emitter.parentId == handle.id)) {
            emitter.active = false;
            stopped = true;
        }
    }
    return stopped;
}

void MagicFxSystem::update(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    updateEmitters(dt);
    updateParticles(dt);
    updateLightningStrikes(dt);
}

void MagicFxSystem::appendRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer) const
{
    for (const LightningStrike& strike : lightningStrikes_) {
        if (!strike.active || strike.pointCount < 2) {
            continue;
        }
        const Vec2 target = strike.points[static_cast<std::size_t>(strike.pointCount - 1)];
        entries.push_back(DepthRenderEntry{
            target.y,
            [&renderer, &strike]() {
                drawLightningStrike(renderer, strike);
            },
        });
    }

    for (const Particle& particle : particles_) {
        if (!particle.active) {
            continue;
        }
        const float sortY = particle.depthSorted ? particle.position.y : -100000.0f;
        entries.push_back(DepthRenderEntry{
            sortY,
            [&renderer, &particle]() {
                drawParticle(renderer, particle);
            },
        });
    }
}

void MagicFxSystem::clear()
{
    particles_.clear();
    emitters_.clear();
    lightningStrikes_.clear();
}

int MagicFxSystem::activeParticleCount() const
{
    return static_cast<int>(std::count_if(particles_.begin(), particles_.end(), [](const Particle& particle) {
        return particle.active;
    }));
}

int MagicFxSystem::activeEmitterCount() const
{
    return static_cast<int>(std::count_if(emitters_.begin(), emitters_.end(), [](const Emitter& emitter) {
        return emitter.active;
    }));
}

void MagicFxSystem::spawnParticles(const MagicFxEmitterConfig& config, int count)
{
    if (count <= 0) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        Particle particle;
        particle.active = true;
        particle.shape = config.particleShape;
        particle.position = config.position + sampleSpawnOffset(config);
        particle.velocity = sampleVelocity(config);
        particle.acceleration = config.acceleration;
        particle.startColor = config.startColor;
        particle.endColor = config.endColor;
        particle.lifetime = std::max(0.02f, sample(config.lifetime));
        particle.startSize = std::max(0.0f, sample(config.startSize));
        particle.endSize = std::max(0.0f, sample(config.endSize));
        particle.rotation = sample(config.rotation);
        particle.angularVelocity = sample(config.angularVelocity);
        particle.height = std::max(0.0f, sample(config.height));
        particle.verticalVelocity = sample(config.verticalVelocity);
        particle.gravity = std::max(0.0f, config.gravity);
        particle.drag = std::max(0.0f, config.drag);
        particle.stretch = std::max(0.1f, config.stretch);
        particle.fadeInFraction = clamp(config.fadeInFraction, 0.0f, 1.0f);
        particle.fadeOutFraction = clamp(config.fadeOutFraction, 0.0f, 1.0f);
        particle.depthSorted = config.depthSorted;
        addParticle(particle);
    }
}

void MagicFxSystem::updateEmitters(float dt)
{
    for (Emitter& emitter : emitters_) {
        if (!emitter.active) {
            continue;
        }

        emitter.age += dt;
        const bool durationElapsed = emitter.config.duration > 0.0f && emitter.age >= emitter.config.duration;
        const bool shouldEmit = emitter.config.loop || !durationElapsed;
        if (shouldEmit && emitter.config.emissionRate > 0.0f) {
            emitter.emitAccumulator += emitter.config.emissionRate * dt;
            const int emitCount = static_cast<int>(std::floor(emitter.emitAccumulator));
            if (emitCount > 0) {
                emitter.emitAccumulator -= static_cast<float>(emitCount);
                spawnParticles(emitter.config, emitCount);
            }
        }

        if (!emitter.config.loop && durationElapsed) {
            emitter.active = false;
        }
        if (!emitter.config.loop && emitter.config.duration <= 0.0f && emitter.config.emissionRate <= 0.0f) {
            emitter.active = false;
        }
    }

    emitters_.erase(
        std::remove_if(emitters_.begin(), emitters_.end(), [](const Emitter& emitter) {
            return !emitter.active;
        }),
        emitters_.end());
}

void MagicFxSystem::updateParticles(float dt)
{
    for (Particle& particle : particles_) {
        if (!particle.active) {
            continue;
        }

        particle.age += dt;
        if (particle.age >= particle.lifetime) {
            particle.active = false;
            continue;
        }

        particle.velocity += particle.acceleration * dt;
        particle.position += particle.velocity * dt;
        particle.velocity = particle.velocity * std::max(0.0f, 1.0f - particle.drag * dt);
        particle.height = std::max(0.0f, particle.height + particle.verticalVelocity * dt);
        particle.verticalVelocity -= particle.gravity * dt;
        particle.rotation += particle.angularVelocity * dt;
    }

    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(), [](const Particle& particle) {
            return !particle.active;
        }),
        particles_.end());
}

void MagicFxSystem::updateLightningStrikes(float dt)
{
    for (LightningStrike& strike : lightningStrikes_) {
        if (!strike.active) {
            continue;
        }
        strike.age += dt;
        if (strike.age >= strike.lifetime) {
            strike.active = false;
        }
    }

    lightningStrikes_.erase(
        std::remove_if(lightningStrikes_.begin(), lightningStrikes_.end(), [](const LightningStrike& strike) {
            return !strike.active;
        }),
        lightningStrikes_.end());
}

float MagicFxSystem::sample(MagicFxRange range)
{
    return sampleRange(range);
}

Vec2 MagicFxSystem::sampleSpawnOffset(const MagicFxEmitterConfig& config)
{
    const Vec2 direction = normalize(config.direction);
    const Vec2 side = perpendicular(direction);
    switch (config.spawnShape) {
    case MagicFxSpawnShape::Point:
        return {};
    case MagicFxSpawnShape::Circle: {
        const float angle = random01() * Pi * 2.0f;
        const float radius = std::sqrt(random01()) * std::max(0.0f, config.spawnRadius);
        return fromAngle(angle) * radius;
    }
    case MagicFxSpawnShape::Ring: {
        const float angle = random01() * Pi * 2.0f;
        return fromAngle(angle) * std::max(0.0f, config.spawnRadius);
    }
    case MagicFxSpawnShape::Line: {
        const float offset = (random01() - 0.5f) * std::max(0.0f, config.spawnLineLength);
        return direction * offset;
    }
    case MagicFxSpawnShape::Cone: {
        const float along = random01() * std::max(0.0f, config.spawnRadius);
        const float width = std::tan(std::max(0.0f, config.spreadRadians) * 0.5f) * along;
        return direction * along + side * ((random01() - 0.5f) * width * 2.0f);
    }
    }
    return {};
}

Vec2 MagicFxSystem::sampleVelocity(const MagicFxEmitterConfig& config)
{
    const Vec2 direction = normalize(config.direction);
    const float baseAngle = std::atan2(direction.y, direction.x);
    const float spread = std::max(0.0f, config.spreadRadians);
    const float angle = baseAngle + (random01() - 0.5f) * spread;
    const float speed = sample(config.speed);
    return config.baseVelocity + fromAngle(angle) * speed;
}

void MagicFxSystem::addParticle(Particle particle)
{
    if (particles_.size() >= MaxParticles) {
        const auto inactive = std::find_if(particles_.begin(), particles_.end(), [](const Particle& existing) {
            return !existing.active;
        });
        if (inactive != particles_.end()) {
            *inactive = particle;
            return;
        }
        particles_.erase(particles_.begin());
    }
    particles_.push_back(particle);
}

}
