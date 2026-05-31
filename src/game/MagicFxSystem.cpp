#include "game/MagicFxSystem.hpp"

#include "engine/Renderer.hpp"
#include "game/DepthRender.hpp"
#include "game/EffectPreviewCatalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace majo {

namespace {

constexpr std::size_t MaxParticles = 2800;
constexpr std::size_t LightweightMaxParticles = 1400;
constexpr std::size_t MaxEmitters = 220;
constexpr std::size_t MaxLightningStrikes = 32;
constexpr std::size_t MaxThunderImpactArcs = 220;
constexpr float MagicFxBounceStopSpeed = 32.0f;

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

Vec2 rotateVec(Vec2 value, float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {value.x * c - value.y * s, value.x * s + value.y * c};
}

float wrapRadians(float angle)
{
    while (angle > Pi) {
        angle -= Pi * 2.0f;
    }
    while (angle < -Pi) {
        angle += Pi * 2.0f;
    }
    return angle;
}

float steerRadians(float current, float target, float amount)
{
    return current + wrapRadians(target - current) * clamp(amount, 0.0f, 1.0f);
}

float hash01(float value)
{
    const float x = std::sin(value * 12.9898f + 78.233f) * 43758.5453f;
    return x - std::floor(x);
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

int electricFrameIndex(float age, float phase = 0.0f, float framesPerSecond = 18.0f)
{
    return static_cast<int>(std::floor(age * framesPerSecond + phase * 11.0f));
}

float electricFrameAlpha(float age, float lifetime, float phase, bool strong)
{
    if (age < 0.0f || age >= lifetime || lifetime <= 0.0f) {
        return 0.0f;
    }

    static constexpr std::array<float, 6> Pattern{{1.0f, 0.88f, 0.96f, 0.82f, 0.94f, 0.86f}};
    const int frame = electricFrameIndex(age, phase);
    const float frameAlpha = Pattern[static_cast<std::size_t>(std::abs(frame) % static_cast<int>(Pattern.size()))];
    if (frameAlpha <= 0.0f) {
        return 0.0f;
    }

    const float progress = clamp(age / lifetime, 0.0f, 1.0f);
    const float step = std::floor(progress * 4.0f) / 3.0f;
    const float steppedDecay = lerp(strong ? 1.0f : 0.90f, strong ? 0.56f : 0.46f, clamp(step, 0.0f, 1.0f));
    return frameAlpha * steppedDecay;
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

void drawDirtClod(Renderer& renderer, Vec2 center, float size, float rotation, Color color)
{
    const Vec2 forward = fromAngle(rotation);
    const Vec2 side = perpendicular(forward);
    const float seed = rotation * 9.17f + size * 0.31f + center.x * 0.013f + center.y * 0.019f;
    constexpr std::size_t Count = 8;
    std::array<Vec2, Count> points{};
    for (std::size_t i = 0; i < Count; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(Count)) * Pi * 2.0f;
        const float radius = size * lerp(0.72f, 1.16f, hash01(seed + static_cast<float>(i) * 1.43f));
        points[i] = center + forward * (std::cos(angle) * radius * 0.88f) + side * (std::sin(angle) * radius * 0.70f);
    }

    const Color outline = scaleAlpha({48, 34, 24, color.a}, 0.72f);
    renderer.fillPolygon(points.data(), points.size(), outline);

    std::array<Vec2, Count> inner = points;
    for (Vec2& p : inner) {
        p = center + (p - center) * 0.86f;
    }
    const Color body = mixColor({122, 82, 48, color.a}, color, 0.35f);
    renderer.fillPolygon(inner.data(), inner.size(), body);

    const Vec2 highlightA = center + forward * (size * 0.42f) - side * (size * 0.30f);
    const Vec2 highlightB = center + forward * (size * 0.05f) + side * (size * 0.42f);
    renderer.drawLine(highlightA, highlightB, scaleAlpha({210, 156, 92, color.a}, 0.42f));
    renderer.drawLine(center - forward * (size * 0.18f) - side * (size * 0.48f), center - forward * (size * 0.46f), scaleAlpha({62, 42, 30, color.a}, 0.62f));
}

void drawEarthCrack(Renderer& renderer, const MagicFxSystem::Particle& particle, Color color, float length)
{
    if (length <= 0.0f || color.a == 0) {
        return;
    }
    const Vec2 root = particle.position;
    const float seed = particle.rotation * 7.7f + particle.startSize * 0.11f + root.x * 0.021f + root.y * 0.017f;
    const Vec2 forward = fromAngle(particle.rotation);
    const Vec2 side = perpendicular(forward);
    const float startDistance = length * lerp(0.02f, 0.08f, hash01(seed + 0.77f));
    const int pointCount = 7 + static_cast<int>(hash01(seed + 1.31f) * 3.0f);
    const float baseSign = hash01(seed + 1.83f) < 0.5f ? -1.0f : 1.0f;
    const Color dark = scaleAlpha({26, 20, 17, color.a}, 0.98f);
    const Color deep = scaleAlpha({12, 10, 9, color.a}, 0.78f);
    const Color warmEdge = scaleAlpha({190, 132, 74, color.a}, 0.22f);

    std::array<Vec2, 10> points{};
    float alongAccumulator = startDistance;
    float sideAccumulator = length * (hash01(seed + 12.0f) - 0.5f) * 0.08f;
    float sideDirection = baseSign;
    for (int i = 0; i < pointCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(pointCount - 1);
        if (i > 0) {
            const float segmentT = static_cast<float>(i - 1) / static_cast<float>(pointCount - 1);
            const float step = length * lerp(0.055f, 0.205f, hash01(seed + 4.0f + static_cast<float>(i) * 5.63f)) * lerp(1.12f, 0.68f, segmentT);
            alongAccumulator += step;
            const float turnRoll = hash01(seed + 9.0f + static_cast<float>(i) * 4.17f);
            if (turnRoll < 0.30f) {
                sideDirection *= -1.0f;
            } else if (turnRoll > 0.78f) {
                sideDirection = hash01(seed + 13.0f + static_cast<float>(i)) < 0.5f ? -1.0f : 1.0f;
            }
            const float targetOffset = length *
                lerp(0.035f, 0.235f, hash01(seed + 20.0f + static_cast<float>(i) * 2.19f)) *
                sideDirection *
                lerp(1.0f, 0.36f, t);
            sideAccumulator = sideAccumulator * lerp(0.12f, 0.46f, hash01(seed + 26.0f + static_cast<float>(i))) + targetOffset;
            sideAccumulator += length * (hash01(seed + 30.0f + static_cast<float>(i) * 3.71f) - 0.5f) * 0.05f;
        }
        points[static_cast<std::size_t>(i)] = root + forward * alongAccumulator + side * sideAccumulator;
    }

    for (int i = 1; i < pointCount; ++i) {
        const float t1 = static_cast<float>(i) / static_cast<float>(pointCount - 1);
        const float widthNoise = lerp(0.58f, 1.55f, hash01(seed + 42.0f + static_cast<float>(i) * 4.31f));
        const float width = std::max(0.62f, length * lerp(0.095f, 0.010f, t1) * widthNoise);
        const Vec2 current = points[static_cast<std::size_t>(i - 1)];
        const Vec2 next = points[static_cast<std::size_t>(i)];
        const Vec2 segment = normalize(next - current);
        const Vec2 edgeOffset = perpendicular(segment) * std::max(0.42f, width * 0.30f);
        renderer.drawSoftLine(current + edgeOffset, next + edgeOffset, width * 0.68f, warmEdge);
        renderer.drawSoftLine(current, next, width, dark);
        renderer.drawLine(current, next, deep);
    }

    for (int i = 1; i < pointCount - 2; ++i) {
        if (hash01(seed + 70.0f + static_cast<float>(i) * 4.23f) <= 0.24f) {
            continue;
        }
        const float t0 = static_cast<float>(i) / static_cast<float>(pointCount - 1);
        const float branchSign = hash01(seed + 80.0f + static_cast<float>(i) * 1.79f) < 0.5f ? -1.0f : 1.0f;
        const Vec2 branchRoot = points[static_cast<std::size_t>(i)] + (points[static_cast<std::size_t>(i + 1)] - points[static_cast<std::size_t>(i)]) * lerp(0.18f, 0.52f, hash01(seed + 84.0f + static_cast<float>(i)));
        const Vec2 branchForward = normalize(forward * lerp(0.25f, 0.58f, hash01(seed + 88.0f + static_cast<float>(i))) + side * branchSign * lerp(0.90f, 1.55f, hash01(seed + 91.0f + static_cast<float>(i))));
        const Vec2 branchSide = perpendicular(branchForward);
        const int branchPointCount = 3 + static_cast<int>(hash01(seed + 95.0f + static_cast<float>(i)) * 3.0f);
        const float branchLength = length * lerp(0.14f, 0.34f, hash01(seed + 100.0f + static_cast<float>(i))) * (1.0f - t0 * 0.46f);
        Vec2 previous = branchRoot;
        float branchAlong = 0.0f;
        float branchOffset = 0.0f;
        float branchDirectionSign = branchSign;
        for (int b = 1; b < branchPointCount; ++b) {
            const float bt = static_cast<float>(b) / static_cast<float>(branchPointCount - 1);
            branchAlong += branchLength * lerp(0.24f, 0.54f, hash01(seed + 104.0f + static_cast<float>(i) * 3.0f + static_cast<float>(b)));
            const float branchTurnRoll = hash01(seed + 108.0f + static_cast<float>(i) * 3.0f + static_cast<float>(b));
            if (branchTurnRoll < 0.32f) {
                branchDirectionSign *= -1.0f;
            } else if (branchTurnRoll > 0.78f) {
                branchDirectionSign = hash01(seed + 112.0f + static_cast<float>(i) * 2.0f + static_cast<float>(b)) < 0.5f ? -1.0f : 1.0f;
            }
            branchOffset += branchLength * branchDirectionSign * lerp(0.04f, 0.20f, hash01(seed + 110.0f + static_cast<float>(i) * 3.0f + static_cast<float>(b))) * (1.0f - bt * 0.42f);
            const Vec2 branchNext = branchRoot +
                branchForward * branchAlong +
                branchSide * branchOffset;
            const Vec2 segmentStart = previous;
            const float branchWidth = std::max(0.36f, length * lerp(0.036f, 0.008f, bt) * (1.0f - t0 * 0.42f));
            renderer.drawSoftLine(segmentStart, branchNext, branchWidth, scaleAlpha(dark, 0.80f));
            renderer.drawLine(segmentStart, branchNext, scaleAlpha(deep, 0.74f));
            if (hash01(seed + 124.0f + static_cast<float>(i) * 5.0f + static_cast<float>(b) * 2.37f) > 0.52f) {
                const float subSign = hash01(seed + 128.0f + static_cast<float>(i) * 7.0f + static_cast<float>(b)) < 0.5f ? -1.0f : 1.0f;
                const Vec2 subRoot = segmentStart + (branchNext - segmentStart) * lerp(0.36f, 0.74f, hash01(seed + 132.0f + static_cast<float>(i) + static_cast<float>(b)));
                const Vec2 subForward = normalize(branchForward * lerp(0.10f, 0.34f, hash01(seed + 136.0f + static_cast<float>(i))) + branchSide * subSign * lerp(0.84f, 1.42f, hash01(seed + 140.0f + static_cast<float>(b))));
                const Vec2 subSide = perpendicular(subForward);
                const int subPointCount = 3 + static_cast<int>(hash01(seed + 144.0f + static_cast<float>(i) * 2.0f + static_cast<float>(b)) * 2.0f);
                const float subLength = branchLength * lerp(0.22f, 0.48f, hash01(seed + 148.0f + static_cast<float>(i) + static_cast<float>(b) * 4.0f)) * (1.0f - bt * 0.28f);
                Vec2 subPrevious = subRoot;
                float subAlong = 0.0f;
                float subOffset = 0.0f;
                float subDirectionSign = subSign;
                for (int s = 1; s < subPointCount; ++s) {
                    const float st = static_cast<float>(s) / static_cast<float>(subPointCount - 1);
                    subAlong += subLength * lerp(0.26f, 0.56f, hash01(seed + 152.0f + static_cast<float>(i) * 3.0f + static_cast<float>(b) * 5.0f + static_cast<float>(s)));
                    if (hash01(seed + 156.0f + static_cast<float>(i) + static_cast<float>(b) * 3.0f + static_cast<float>(s)) < 0.38f) {
                        subDirectionSign *= -1.0f;
                    }
                    subOffset += subLength * subDirectionSign * lerp(0.04f, 0.17f, hash01(seed + 160.0f + static_cast<float>(i) + static_cast<float>(b) + static_cast<float>(s))) * (1.0f - st * 0.48f);
                    const Vec2 subNext = subRoot + subForward * subAlong + subSide * subOffset;
                    const float subWidth = std::max(0.26f, branchWidth * lerp(0.58f, 0.30f, st));
                    renderer.drawSoftLine(subPrevious, subNext, subWidth, scaleAlpha(dark, 0.58f));
                    renderer.drawLine(subPrevious, subNext, scaleAlpha(deep, 0.56f));
                    subPrevious = subNext;
                }
            }
            previous = branchNext;
        }
    }
}

void drawEarthSpike(Renderer& renderer, const MagicFxSystem::Particle& particle, Color color, float size)
{
    if (size <= 0.0f || color.a == 0) {
        return;
    }
    const float progress = particle.lifetime > 0.0f ? clamp(particle.age / particle.lifetime, 0.0f, 1.0f) : 1.0f;
    const float rise = progress < 0.18f
        ? 1.0f - (1.0f - progress / 0.18f) * (1.0f - progress / 0.18f) * (1.0f - progress / 0.18f)
        : 1.0f;
    const Vec2 base = particle.position;
    const Vec2 up = fromAngle(particle.rotation);
    const Vec2 side = perpendicular(up);
    const float seed = particle.rotation * 11.3f + particle.startSize * 0.27f + base.x * 0.015f + base.y * 0.018f;
    const float spikeHeight = size * lerp(1.18f, 1.50f, hash01(seed + 1.0f)) * rise;
    const float baseWidth = size * lerp(0.68f, 0.94f, hash01(seed + 2.0f));

    auto drift = [&](float salt, float scale) {
        return (hash01(seed + salt) - 0.5f) * scale;
    };
    auto centerAt = [&](float t, float salt, float scale) {
        const float bend = drift(salt, scale) * std::sin(t * Pi * 0.95f);
        return base + up * (spikeHeight * t) + side * (baseWidth * bend);
    };

    const Vec2 c0 = centerAt(0.00f, 10.0f, 0.08f);
    const Vec2 c1 = centerAt(0.22f, 11.0f, 0.28f);
    const Vec2 c2 = centerAt(0.47f, 12.0f, 0.36f);
    const Vec2 c3 = centerAt(0.72f, 13.0f, 0.30f);
    const Vec2 c4 = centerAt(0.96f, 14.0f, 0.22f);
    const float topWidth = baseWidth * lerp(0.16f, 0.30f, hash01(seed + 15.0f));

    const Vec2 leftBase = c0 - side * (baseWidth * lerp(0.62f, 0.90f, hash01(seed + 20.0f))) + up * (baseWidth * drift(21.0f, 0.10f));
    const Vec2 leftLow = c1 - side * (baseWidth * lerp(0.46f, 0.78f, hash01(seed + 22.0f))) + up * (baseWidth * drift(23.0f, 0.14f));
    const Vec2 leftMid = c2 - side * (baseWidth * lerp(0.34f, 0.72f, hash01(seed + 24.0f))) + up * (baseWidth * drift(25.0f, 0.12f));
    const Vec2 leftHigh = c3 - side * (baseWidth * lerp(0.20f, 0.48f, hash01(seed + 26.0f))) + up * (baseWidth * drift(27.0f, 0.10f));
    const Vec2 topLeft = c4 - side * (topWidth * lerp(0.72f, 1.36f, hash01(seed + 28.0f))) - up * (baseWidth * lerp(0.02f, 0.12f, hash01(seed + 29.0f)));

    const Vec2 rightBase = c0 + side * (baseWidth * lerp(0.56f, 0.84f, hash01(seed + 30.0f))) + up * (baseWidth * drift(31.0f, 0.08f));
    const Vec2 rightLow = c1 + side * (baseWidth * lerp(0.50f, 0.82f, hash01(seed + 32.0f))) + up * (baseWidth * drift(33.0f, 0.13f));
    const Vec2 rightMid = c2 + side * (baseWidth * lerp(0.30f, 0.64f, hash01(seed + 34.0f))) + up * (baseWidth * drift(35.0f, 0.11f));
    const Vec2 rightHigh = c3 + side * (baseWidth * lerp(0.16f, 0.44f, hash01(seed + 36.0f))) + up * (baseWidth * drift(37.0f, 0.12f));
    const Vec2 topRight = c4 + side * (topWidth * lerp(0.48f, 1.18f, hash01(seed + 38.0f))) - up * (baseWidth * lerp(0.04f, 0.16f, hash01(seed + 39.0f)));
    const Vec2 chippedTip = c4 + side * (topWidth * drift(40.0f, 1.10f)) + up * (baseWidth * lerp(0.02f, 0.13f, hash01(seed + 41.0f)));

    const Vec2 rightShelf = rightBase + side * (baseWidth * lerp(0.04f, 0.16f, hash01(seed + 42.0f))) + up * (baseWidth * lerp(0.02f, 0.13f, hash01(seed + 43.0f)));
    const Vec2 rightRoot = c0 + side * (baseWidth * lerp(0.40f, 0.74f, hash01(seed + 44.0f))) + up * (-baseWidth * lerp(0.04f, 0.18f, hash01(seed + 45.0f)));
    const Vec2 rootDip = c0 + side * (baseWidth * drift(46.0f, 0.22f)) + up * (-baseWidth * lerp(0.08f, 0.24f, hash01(seed + 47.0f)));
    const Vec2 leftRoot = c0 - side * (baseWidth * lerp(0.44f, 0.82f, hash01(seed + 48.0f))) + up * (-baseWidth * lerp(0.02f, 0.16f, hash01(seed + 49.0f)));
    const Vec2 leftShelf = leftBase - side * (baseWidth * lerp(0.04f, 0.18f, hash01(seed + 55.0f))) + up * (baseWidth * lerp(0.00f, 0.12f, hash01(seed + 56.0f)));

    const std::array<Vec2, 14> outline{
        chippedTip,
        topRight,
        rightHigh,
        rightMid,
        rightLow,
        rightShelf,
        rightRoot,
        rootDip,
        leftRoot,
        leftShelf,
        leftLow,
        leftMid,
        leftHigh,
        topLeft};
    renderer.fillPolygon(outline.data(), outline.size(), scaleAlpha({54, 38, 28, color.a}, 0.96f));

    const Vec2 ridgeLow = c1 + side * (baseWidth * drift(50.0f, 0.26f));
    const Vec2 ridgeMid = c2 + side * (baseWidth * drift(51.0f, 0.30f));
    const Vec2 ridgeHigh = c3 + side * (baseWidth * drift(52.0f, 0.26f));
    const Vec2 ridgeTip = c4 + side * (baseWidth * drift(53.0f, 0.18f)) - up * (baseWidth * lerp(0.02f, 0.10f, hash01(seed + 54.0f)));
    const Vec2 rootRidge = c0 + side * (baseWidth * drift(57.0f, 0.18f)) + up * (-baseWidth * lerp(0.02f, 0.10f, hash01(seed + 58.0f)));
    const Vec2 lowerLeftBreak = c0 - side * (baseWidth * lerp(0.18f, 0.42f, hash01(seed + 59.0f))) + up * (baseWidth * lerp(0.02f, 0.14f, hash01(seed + 60.0f)));
    const Vec2 lowerRightBreak = c0 + side * (baseWidth * lerp(0.16f, 0.46f, hash01(seed + 61.0f))) + up * (baseWidth * lerp(0.00f, 0.13f, hash01(seed + 62.0f)));

    const std::array<Vec2, 5> leftFace{chippedTip, topLeft, leftHigh, leftMid, ridgeMid};
    const std::array<Vec2, 6> rightFace{chippedTip, topRight, rightHigh, rightMid, ridgeMid, ridgeHigh};
    const std::array<Vec2, 5> centerRidgeFace{ridgeMid, rightMid, rightLow, ridgeLow, lowerLeftBreak};
    const std::array<Vec2, 5> lowerRidgeFace{ridgeLow, lowerRightBreak, rootRidge, lowerLeftBreak, leftMid};
    const std::array<Vec2, 4> lowerLeftFace{leftMid, lowerLeftBreak, leftRoot, leftShelf};
    const std::array<Vec2, 4> lowerCenterFace{lowerLeftBreak, rootRidge, rootDip, leftRoot};
    const std::array<Vec2, 5> lowerRightFace{rightLow, rightShelf, rightRoot, lowerRightBreak, ridgeLow};
    const std::array<Vec2, 4> rightBaseFace{lowerRightBreak, rightRoot, rootDip, rootRidge};
    const std::array<Vec2, 4> topFace{topLeft, chippedTip, topRight, ridgeTip};
    renderer.fillPolygon(leftFace.data(), leftFace.size(), scaleAlpha({150, 104, 64, color.a}, 0.92f));
    renderer.fillPolygon(rightFace.data(), rightFace.size(), scaleAlpha({86, 60, 44, color.a}, 0.96f));
    renderer.fillPolygon(centerRidgeFace.data(), centerRidgeFace.size(), scaleAlpha({124, 84, 54, color.a}, 0.82f));
    renderer.fillPolygon(lowerRidgeFace.data(), lowerRidgeFace.size(), scaleAlpha({116, 78, 52, color.a}, 0.78f));
    renderer.fillPolygon(lowerLeftFace.data(), lowerLeftFace.size(), scaleAlpha({132, 90, 56, color.a}, 0.74f));
    renderer.fillPolygon(lowerCenterFace.data(), lowerCenterFace.size(), scaleAlpha({104, 70, 48, color.a}, 0.72f));
    renderer.fillPolygon(lowerRightFace.data(), lowerRightFace.size(), scaleAlpha({72, 50, 38, color.a}, 0.82f));
    renderer.fillPolygon(rightBaseFace.data(), rightBaseFace.size(), scaleAlpha({84, 58, 42, color.a}, 0.72f));
    renderer.fillPolygon(topFace.data(), topFace.size(), scaleAlpha({178, 124, 74, color.a}, 0.68f));

    renderer.drawSoftLine(chippedTip + (ridgeHigh - chippedTip) * 0.22f, chippedTip + (ridgeHigh - chippedTip) * 0.48f, 1.1f, scaleAlpha({222, 166, 102, color.a}, 0.18f));
    renderer.drawSoftLine(ridgeHigh + (ridgeMid - ridgeHigh) * 0.16f, ridgeHigh + (ridgeMid - ridgeHigh) * 0.42f, 1.0f, scaleAlpha({210, 148, 88, color.a}, 0.15f));
    renderer.drawLine(ridgeMid, ridgeLow, scaleAlpha({76, 52, 38, color.a}, 0.20f));
    renderer.drawLine(ridgeLow, rootRidge, scaleAlpha({64, 44, 32, color.a}, 0.20f));
    renderer.drawSoftLine(topLeft + (leftHigh - topLeft) * 0.20f, topLeft + (leftHigh - topLeft) * 0.54f, 1.0f, scaleAlpha({210, 148, 84, color.a}, 0.14f));
    renderer.drawLine(rightHigh, rightLow, scaleAlpha({38, 28, 22, color.a}, 0.38f));
    renderer.drawLine(leftMid, lowerLeftBreak, scaleAlpha({54, 36, 28, color.a}, 0.16f));
    renderer.drawLine(rightLow, ridgeLow, scaleAlpha({42, 30, 24, color.a}, 0.18f));
    renderer.drawSoftLine(leftShelf, lowerLeftBreak, 1.0f, scaleAlpha({206, 142, 78, color.a}, 0.13f));
    renderer.drawLine(lowerRightBreak, rightRoot, scaleAlpha({36, 26, 22, color.a}, 0.30f));
}

void drawSharpLightningSegment(Renderer& renderer, Vec2 a, Vec2 b, float width, Color color)
{
    const Vec2 delta = b - a;
    if (lengthSquared(delta) <= 0.0001f || width <= 0.0f || color.a == 0) {
        return;
    }
    const Vec2 direction = normalize(delta);
    const Vec2 side = perpendicular(direction);
    const float startWidth = width * 0.54f;
    const float endWidth = width * 0.42f;
    const std::array<Vec2, 6> points{
        a - direction * (width * 0.32f),
        a + side * startWidth,
        b + side * endWidth,
        b + direction * (width * 0.58f),
        b - side * endWidth,
        a - side * startWidth,
    };
    renderer.fillPolygon(points.data(), points.size(), color);
}

void buildElectricFramePath(
    const Vec2* points,
    int pointCount,
    int frameIndex,
    bool strong,
    float jitterScale,
    std::array<Vec2, 12>& outPoints)
{
    if (points == nullptr || pointCount <= 0) {
        return;
    }

    const int safeCount = std::clamp(pointCount, 1, static_cast<int>(outPoints.size()));
    for (int i = 0; i < safeCount; ++i) {
        outPoints[static_cast<std::size_t>(i)] = points[i];
    }
    if (safeCount <= 2) {
        return;
    }

    float totalLength = 0.0f;
    for (int i = 1; i < safeCount; ++i) {
        totalLength += length(points[i] - points[i - 1]);
    }
    const float averageStep = totalLength / static_cast<float>(std::max(1, safeCount - 1));
    const float baseAmplitude = std::max(
        jitterScale * (strong ? 2.80f : 2.20f),
        averageStep * (strong ? 0.72f : 0.56f));
    for (int i = 1; i < safeCount - 1; ++i) {
        const Vec2 tangentSource = points[i + 1] - points[i - 1];
        if (lengthSquared(tangentSource) <= 0.0001f) {
            continue;
        }

        const Vec2 tangent = normalize(tangentSource);
        const Vec2 side = perpendicular(tangent);
        const float t = static_cast<float>(i) / static_cast<float>(safeCount - 1);
        const float centerWeight = std::sin(t * Pi);
        const float seed = static_cast<float>(frameIndex * 191 + i * 271 + safeCount * 43);
        const float zigzagSign = hash01(seed + 5.0f) < 0.5f ? -1.0f : 1.0f;
        const float sideJitter =
            ((hash01(seed) * 2.0f - 1.0f) * baseAmplitude +
             zigzagSign * baseAmplitude * lerp(0.36f, 1.05f, hash01(seed + 9.0f))) *
            centerWeight;
        const float forwardJitter = (hash01(seed + 17.0f) * 2.0f - 1.0f) * baseAmplitude * 0.62f * centerWeight;
        outPoints[static_cast<std::size_t>(i)] = points[i] + side * sideJitter + tangent * forwardJitter;
    }
}

void drawLightningPath(Renderer& renderer, const Vec2* points, int pointCount, float outerWidth, float coreWidth, float alpha, bool strong, int frameIndex)
{
    if (points == nullptr || pointCount < 2 || alpha <= 0.0f) {
        return;
    }

    std::array<Vec2, 12> framePoints{};
    const int safeCount = std::clamp(pointCount, 2, static_cast<int>(framePoints.size()));
    buildElectricFramePath(points, safeCount, frameIndex, strong, outerWidth * 0.95f, framePoints);

    const Color shell = scaleAlpha(strong ? Color{255, 242, 170, 226} : Color{255, 238, 188, 194}, alpha);
    const Color core = scaleAlpha(strong ? Color{255, 252, 214, 255} : Color{255, 250, 222, 232}, alpha);
    for (int i = 1; i < safeCount; ++i) {
        const float segmentT = static_cast<float>(i - 1) / static_cast<float>(std::max(1, safeCount - 2));
        const float pulse = 0.72f + 0.48f * (static_cast<float>((frameIndex + i * 37) % 5) / 4.0f);
        const float taper = lerp(1.36f, 0.54f, segmentT);
        const float width = outerWidth * pulse * taper;
        drawSharpLightningSegment(renderer, framePoints[static_cast<std::size_t>(i - 1)], framePoints[static_cast<std::size_t>(i)], width * 0.78f, shell);
        drawSharpLightningSegment(renderer, framePoints[static_cast<std::size_t>(i - 1)], framePoints[static_cast<std::size_t>(i)], std::max(1.0f, coreWidth * pulse * taper), core);
    }
}

void drawFireballCore(Renderer& renderer, const MagicFxSystem::Particle& particle, Color color, float size)
{
    if (color.a == 0 || size <= 0.0f) {
        return;
    }

    const Vec2 center = particleDrawPosition(particle);
    const float seed = particle.rotation * 13.7f + particle.startSize * 0.41f + center.x * 0.015f + center.y * 0.019f;
    const float pulse =
        1.0f +
        std::sin(particle.age * Pi * 10.0f + seed) * 0.10f +
        std::sin(particle.age * Pi * 6.0f + seed * 0.37f) * 0.055f;
    const float coreSize = size * clamp(pulse, 0.84f, 1.18f);
    renderer.fillSoftCircle(center, coreSize * 1.72f, scaleAlpha({255, 92, 20, color.a}, 0.44f));
    renderer.fillSoftCircle(center, coreSize * 1.12f, scaleAlpha({255, 122, 28, color.a}, 0.78f));
    renderer.fillCircle(center, coreSize * 0.78f, color);
    renderer.fillSoftCircle(center + Vec2{-coreSize * 0.18f, -coreSize * 0.20f}, coreSize * 0.34f, scaleAlpha({255, 218, 88, color.a}, 0.72f));
}

void drawThunderArcParticle(Renderer& renderer, const MagicFxSystem::Particle& particle, Color color, float size)
{
    if (color.a == 0 || size <= 0.0f) {
        return;
    }

    const Vec2 center = particleDrawPosition(particle);
    const Vec2 forward = lengthSquared(particle.velocity) > 0.0001f ? normalize(particle.velocity) : fromAngle(particle.rotation);
    const Vec2 side = perpendicular(forward);
    const float seed = particle.rotation * 18.31f + particle.startSize * 0.37f + center.x * 0.019f + center.y * 0.023f;
    const int pointCount = 6 + static_cast<int>(hash01(seed + 0.71f) * 4.0f);
    const float length = std::max(7.0f, size * std::max(1.0f, particle.stretch) * 1.88f);
    const float bendSign = hash01(seed + 1.37f) < 0.5f ? -1.0f : 1.0f;
    std::array<Vec2, 12> points{};
    for (int i = 0; i < pointCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(pointCount - 1);
        const float along = (t - 0.5f) * length;
        const float centerWeight = std::sin(t * Pi);
        const float alternating = (i % 2 == 0 ? -1.0f : 1.0f) * bendSign;
        const float bow = centerWeight * length * lerp(0.16f, 0.34f, hash01(seed + 2.0f)) * bendSign;
        const float notch =
            alternating * centerWeight * length * lerp(0.13f, 0.30f, hash01(seed + 3.0f + static_cast<float>(i) * 1.9f)) +
            (hash01(seed + 11.0f + static_cast<float>(i) * 2.3f) - 0.5f) * length * 0.12f * centerWeight;
        points[static_cast<std::size_t>(i)] = center + forward * along + side * (bow + notch);
    }

    const int frameIndex = electricFrameIndex(particle.age, hash01(seed + 5.0f), 6.0f);
    static constexpr std::array<float, 6> FrameAlpha{{1.0f, 0.72f, 0.96f, 0.64f, 0.90f, 0.78f}};
    const float frameAlpha = FrameAlpha[static_cast<std::size_t>(std::abs(frameIndex) % static_cast<int>(FrameAlpha.size()))];
    drawLightningPath(
        renderer,
        points.data(),
        pointCount,
        std::max(1.8f, size * 0.34f),
        std::max(0.8f, size * 0.12f),
        (static_cast<float>(color.a) / 255.0f) * frameAlpha,
        false,
        frameIndex);
}

void drawLightningStrike(Renderer& renderer, const MagicFxSystem::LightningStrike& strike)
{
    const float progress = strike.lifetime > 0.0f ? clamp(strike.age / strike.lifetime, 0.0f, 1.0f) : 1.0f;
    const float alpha = electricFrameAlpha(strike.age, strike.lifetime, strike.strong ? 0.18f : 0.43f, strike.strong);
    if (progress >= 1.0f || alpha <= 0.0f) {
        return;
    }

    drawLightningPath(
        renderer,
        strike.points.data(),
        strike.pointCount,
        strike.outerWidth,
        strike.coreWidth,
        alpha,
        strike.strong,
        electricFrameIndex(strike.age, strike.strong ? 0.18f : 0.43f));
}

void drawThunderSigilArc(Renderer& renderer, const MagicFxSystem::ThunderImpactArc& arc, float alpha)
{
    const int frameIndex = electricFrameIndex(arc.age - arc.startDelay, arc.phase);
    std::array<Vec2, 12> framePoints{};
    const int safeCount = std::clamp(arc.pointCount, 2, static_cast<int>(framePoints.size()));
    buildElectricFramePath(arc.points.data(), safeCount, frameIndex, arc.strong, arc.outerWidth * 1.20f, framePoints);

    const Color line = scaleAlpha(arc.strong ? Color{255, 242, 176, 192} : Color{255, 238, 196, 156}, alpha);
    const Color core = scaleAlpha(Color{255, 252, 218, 230}, alpha);
    for (int i = 1; i < safeCount; ++i) {
        const float t = static_cast<float>(i - 1) / static_cast<float>(std::max(1, safeCount - 2));
        const float width = arc.outerWidth * lerp(1.18f, 0.72f, t);
        drawSharpLightningSegment(renderer, framePoints[static_cast<std::size_t>(i - 1)], framePoints[static_cast<std::size_t>(i)], width * 0.82f, line);
        drawSharpLightningSegment(renderer, framePoints[static_cast<std::size_t>(i - 1)], framePoints[static_cast<std::size_t>(i)], std::max(1.0f, arc.coreWidth * 0.62f), core);
    }
}

void drawThunderImpactArc(Renderer& renderer, const MagicFxSystem::ThunderImpactArc& arc)
{
    if (!arc.active || arc.pointCount < 2 || arc.age < arc.startDelay) {
        return;
    }
    const float localAge = arc.age - arc.startDelay;
    const float progress = arc.lifetime > 0.0f ? clamp(localAge / arc.lifetime, 0.0f, 1.0f) : 1.0f;
    float alpha = electricFrameAlpha(localAge, arc.lifetime, arc.phase, arc.strong);
    if (arc.style == MagicFxSystem::ThunderImpactArcStyle::Vertical) {
        alpha *= 0.92f;
    } else if (arc.style == MagicFxSystem::ThunderImpactArcStyle::Sigil) {
        alpha *= 0.78f;
    }
    if (progress >= 1.0f || alpha <= 0.0f) {
        return;
    }

    if (arc.style == MagicFxSystem::ThunderImpactArcStyle::Sigil) {
        drawThunderSigilArc(renderer, arc, alpha);
        return;
    }

    drawLightningPath(
        renderer,
        arc.points.data(),
        arc.pointCount,
        arc.outerWidth,
        arc.coreWidth,
        alpha,
        arc.strong,
        electricFrameIndex(localAge, arc.phase));
}

void drawWindBlade(Renderer& renderer, const MagicFxSystem::Particle& particle, Color color, float size)
{
    if (color.a == 0 || size <= 0.0f) {
        return;
    }

    const Vec2 center = particleDrawPosition(particle);
    const Vec2 forward = lengthSquared(particle.velocity) > 0.0001f ? normalize(particle.velocity) : fromAngle(particle.rotation);
    drawWindCrescent(renderer, center, size, std::atan2(forward.y, forward.x), particle.stretch, color);
}

void drawParticleGroundShadow(Renderer& renderer, const MagicFxSystem::Particle& particle, float size)
{
    if (particle.bouncesRemaining <= 0 && particle.height <= 0.5f) {
        return;
    }
    if (particle.bounceRestitution <= 0.0f) {
        return;
    }

    const float heightFade = clamp(1.0f - particle.height / 64.0f, 0.18f, 0.54f);
    const float radius = std::max(1.4f, size * lerp(0.95f, 0.52f, clamp(particle.height / 54.0f, 0.0f, 1.0f)));
    renderer.fillSoftCircle(
        particle.position + Vec2{0.0f, 2.0f},
        radius,
        scaleAlpha({20, 28, 34, 82}, heightFade));
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
    drawParticleGroundShadow(renderer, particle, size);
    Vec2 center = particleDrawPosition(particle);
    if (particle.shape == MagicFxParticleShape::DirtClod && particle.lifetime > 3.0f) {
        const float seed = particle.rotation * 4.37f + particle.startSize * 0.83f + particle.lifetime * 0.19f;
        const float swayAmount = particle.startSize * (1.95f + 0.46f * std::sin(particle.age * 0.78f + seed));
        center += fromAngle(seed + std::sin(particle.age * 0.72f + seed) * 1.28f) * swayAmount;
    }
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
    case MagicFxParticleShape::FireballCore:
        drawFireballCore(renderer, particle, color, size);
        break;
    case MagicFxParticleShape::ThunderArc:
        drawThunderArcParticle(renderer, particle, color, size);
        break;
    case MagicFxParticleShape::Shard:
        drawShard(renderer, center, size, particle.rotation, color);
        break;
    case MagicFxParticleShape::Crystal:
        drawCrystal(renderer, center, size, particle.rotation, color);
        break;
    case MagicFxParticleShape::WindCrescent:
        drawWindCrescent(renderer, center, size, particle.rotation, particle.stretch, color);
        break;
    case MagicFxParticleShape::WindBlade:
        drawWindBlade(renderer, particle, color, size);
        break;
    case MagicFxParticleShape::WindSparkle:
        drawWindSparkle(renderer, center, size, particle.rotation, color);
        break;
    case MagicFxParticleShape::EarthSpike:
        drawEarthSpike(renderer, particle, color, size);
        break;
    case MagicFxParticleShape::EarthCrack:
        drawEarthCrack(renderer, particle, color, particle.startSize);
        break;
    case MagicFxParticleShape::DirtClod:
        drawDirtClod(renderer, center, size, particle.rotation, color);
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

void MagicFxSystem::playHealPulse(Vec2 position, float radius)
{
    radius = std::max(8.0f, radius);

    MagicFxEmitterConfig ring;
    ring.position = position;
    ring.particleShape = MagicFxParticleShape::Ring;
    ring.spawnShape = MagicFxSpawnShape::Point;
    ring.startColor = {118, 255, 168, 220};
    ring.endColor = {220, 255, 218, 0};
    ring.alphaScale = {0.82f, 1.0f};
    ring.lifetime = {0.62f, 0.82f};
    ring.startSize = {radius * 0.34f, radius * 0.44f};
    ring.endSize = {radius * 1.16f, radius * 1.36f};
    ring.fadeInFraction = 0.02f;
    ring.fadeOutFraction = 0.78f;
    ring.burstCount = 1;
    ring.depthSorted = true;
    emitBurst(ring);

    MagicFxEmitterConfig glow;
    glow.position = position;
    glow.particleShape = MagicFxParticleShape::SoftCircle;
    glow.spawnShape = MagicFxSpawnShape::Circle;
    glow.startColor = {112, 255, 184, 146};
    glow.endColor = {226, 255, 214, 0};
    glow.alphaScale = {0.72f, 1.0f};
    glow.speed = {4.0f, 18.0f};
    glow.lifetime = {0.56f, 1.02f};
    glow.startSize = {radius * 0.14f, radius * 0.30f};
    glow.endSize = {0.0f, radius * 0.08f};
    glow.height = {0.0f, 4.0f};
    glow.verticalVelocity = {16.0f, 42.0f};
    glow.gravity = 14.0f;
    glow.drag = 2.2f;
    glow.spawnRadius = radius * 0.58f;
    glow.spreadRadians = Pi * 1.35f;
    glow.fadeInFraction = 0.04f;
    glow.fadeOutFraction = 0.76f;
    glow.emissionRate = 44.0f;
    glow.duration = 0.44f;
    glow.startDelay = 0.03f;
    glow.depthSorted = true;
    glow.foreground = true;
    addEmitter(glow);

    MagicFxEmitterConfig motes;
    motes.position = position;
    motes.particleShape = MagicFxParticleShape::Circle;
    motes.spawnShape = MagicFxSpawnShape::Ring;
    motes.startColor = {198, 255, 146, 230};
    motes.endColor = {102, 255, 202, 0};
    motes.alphaScale = {0.76f, 1.0f};
    motes.speed = {16.0f, 54.0f};
    motes.lifetime = {0.62f, 1.18f};
    motes.startSize = {1.8f, 4.2f};
    motes.endSize = {0.0f, 0.8f};
    motes.height = {0.0f, 6.0f};
    motes.verticalVelocity = {24.0f, 58.0f};
    motes.gravity = 18.0f;
    motes.drag = 1.7f;
    motes.spawnRadius = radius * 0.72f;
    motes.spreadRadians = Pi * 1.50f;
    motes.fadeInFraction = 0.05f;
    motes.fadeOutFraction = 0.82f;
    motes.emissionRate = 34.0f;
    motes.duration = 0.42f;
    motes.startDelay = 0.12f;
    motes.depthSorted = true;
    motes.foreground = true;
    addEmitter(motes);

    MagicFxEmitterConfig glints;
    glints.position = position;
    glints.particleShape = MagicFxParticleShape::SparkLine;
    glints.spawnShape = MagicFxSpawnShape::Ring;
    glints.startColor = {230, 255, 190, 238};
    glints.endColor = {116, 255, 188, 0};
    glints.alphaScale = {0.78f, 1.0f};
    glints.speed = {24.0f, 76.0f};
    glints.lifetime = {0.34f, 0.72f};
    glints.startSize = {0.7f, 1.8f};
    glints.endSize = {0.0f, 0.3f};
    glints.height = {0.0f, 5.0f};
    glints.verticalVelocity = {10.0f, 34.0f};
    glints.gravity = 8.0f;
    glints.drag = 1.2f;
    glints.spawnRadius = radius * 0.78f;
    glints.spreadRadians = Pi * 1.65f;
    glints.stretch = 2.2f;
    glints.fadeOutFraction = 0.78f;
    glints.emissionRate = 30.0f;
    glints.duration = 0.34f;
    glints.startDelay = 0.20f;
    glints.depthSorted = true;
    glints.foreground = true;
    addEmitter(glints);
}

MagicFxEmitterHandle MagicFxSystem::startFireAura(Vec2 position, float radius)
{
    radius = std::max(4.0f, radius);

    MagicFxEmitterConfig outerFlame;
    outerFlame.position = position;
    outerFlame.direction = {0.0f, -1.0f};
    outerFlame.particleShape = MagicFxParticleShape::SoftCircle;
    outerFlame.spawnShape = MagicFxSpawnShape::Circle;
    outerFlame.startColor = {255, 118, 24, 188};
    outerFlame.endColor = {255, 154, 42, 0};
    outerFlame.speed = {6.0f, 22.0f};
    outerFlame.lifetime = {0.82f, 1.35f};
    outerFlame.startSize = {std::max(0.8f, radius * 0.055f), std::max(1.8f, radius * 0.125f)};
    outerFlame.endSize = {0.0f, std::max(0.35f, radius * 0.028f)};
    outerFlame.height = {0.0f, 4.0f};
    outerFlame.verticalVelocity = {12.0f, 30.0f};
    outerFlame.gravity = 12.0f;
    outerFlame.drag = 1.45f;
    outerFlame.spawnRadius = radius * 0.68f;
    outerFlame.spreadRadians = Pi * 0.92f;
    outerFlame.fadeInFraction = 0.04f;
    outerFlame.fadeOutFraction = 0.70f;
    outerFlame.emissionRate = 84.0f;
    outerFlame.loop = true;
    outerFlame.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(outerFlame);

    MagicFxEmitterConfig core;
    core.position = position;
    core.direction = {0.0f, -1.0f};
    core.particleShape = MagicFxParticleShape::Circle;
    core.spawnShape = MagicFxSpawnShape::Circle;
    core.startColor = {255, 238, 104, 224};
    core.endColor = {255, 184, 52, 0};
    core.speed = {4.0f, 18.0f};
    core.lifetime = {0.68f, 1.12f};
    core.startSize = {std::max(0.7f, radius * 0.040f), std::max(1.5f, radius * 0.100f)};
    core.endSize = {0.0f, std::max(0.28f, radius * 0.022f)};
    core.height = {0.0f, 2.0f};
    core.verticalVelocity = {14.0f, 34.0f};
    core.gravity = 11.0f;
    core.drag = 1.65f;
    core.spawnRadius = radius * 0.36f;
    core.spreadRadians = Pi * 0.62f;
    core.fadeOutFraction = 0.72f;
    core.emissionRate = 68.0f;
    core.loop = true;
    core.depthSorted = true;
    addEmitterWithParent(core, handle.id);

    MagicFxEmitterConfig embers;
    embers.position = position;
    embers.direction = {0.0f, -1.0f};
    embers.particleShape = MagicFxParticleShape::Circle;
    embers.spawnShape = MagicFxSpawnShape::Circle;
    embers.startColor = {255, 216, 86, 190};
    embers.endColor = {255, 88, 18, 0};
    embers.speed = {14.0f, 44.0f};
    embers.lifetime = {0.90f, 1.55f};
    embers.startSize = {0.35f, 0.95f};
    embers.endSize = {0.0f, 0.16f};
    embers.height = {0.0f, 3.0f};
    embers.verticalVelocity = {16.0f, 42.0f};
    embers.gravity = 16.0f;
    embers.drag = 1.15f;
    embers.spawnRadius = radius * 0.58f;
    embers.spreadRadians = Pi * 1.20f;
    embers.fadeOutFraction = 0.76f;
    embers.emissionRate = 24.0f;
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
    smoke.emissionRate = 16.0f;
    smoke.loop = true;
    smoke.depthSorted = true;
    addEmitterWithParent(smoke, handle.id);

    return handle;
}

MagicFxEmitterHandle MagicFxSystem::startFireballLoop(Vec2 position, Vec2 direction, float radius)
{
    direction = normalize(direction);
    radius = std::max(3.0f, radius * 0.72f);

    MagicFxEmitterConfig body;
    body.position = position;
    body.direction = direction;
    body.particleShape = MagicFxParticleShape::FireballCore;
    body.spawnShape = MagicFxSpawnShape::Point;
    body.startColor = {255, 126, 28, 240};
    body.endColor = {255, 96, 20, 222};
    body.speed = {0.0f, 0.0f};
    body.lifetime = {0.075f, 0.115f};
    body.startSize = {std::max(1.0f, radius * 0.20f), std::max(1.6f, radius * 0.30f)};
    body.endSize = body.startSize;
    body.rotation = {0.0f, Pi * 2.0f};
    body.fadeOutFraction = 0.10f;
    body.emissionRate = 58.0f;
    body.loop = true;
    body.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(body);

    MagicFxEmitterConfig core;
    core.position = position;
    core.direction = direction * -1.0f;
    core.baseVelocity = direction * -28.0f;
    core.particleShape = MagicFxParticleShape::Circle;
    core.spawnShape = MagicFxSpawnShape::Circle;
    core.startColor = {255, 238, 104, 238};
    core.endColor = {255, 178, 48, 0};
    core.speed = {7.0f, 26.0f};
    core.lifetime = {0.44f, 0.82f};
    core.startSize = {std::max(0.8f, radius * 0.075f), std::max(1.8f, radius * 0.180f)};
    core.endSize = {0.0f, std::max(0.28f, radius * 0.028f)};
    core.height = {0.0f, 3.0f};
    core.verticalVelocity = {3.0f, 18.0f};
    core.gravity = 8.0f;
    core.drag = 1.7f;
    core.spawnRadius = std::max(1.0f, radius * 0.28f);
    core.spreadRadians = Pi * 1.25f;
    core.fadeOutFraction = 0.76f;
    core.emissionRate = 66.0f;
    core.loop = true;
    core.depthSorted = true;
    addEmitterWithParent(core, handle.id);

    MagicFxEmitterConfig outerFlame;
    outerFlame.position = position;
    outerFlame.direction = direction * -1.0f;
    outerFlame.baseVelocity = direction * -52.0f;
    outerFlame.particleShape = MagicFxParticleShape::SoftCircle;
    outerFlame.spawnShape = MagicFxSpawnShape::Circle;
    outerFlame.startColor = {255, 120, 26, 214};
    outerFlame.endColor = {255, 154, 42, 0};
    outerFlame.speed = {16.0f, 54.0f};
    outerFlame.lifetime = {0.58f, 1.08f};
    outerFlame.startSize = {std::max(0.9f, radius * 0.090f), std::max(2.2f, radius * 0.250f)};
    outerFlame.endSize = {0.0f, std::max(0.32f, radius * 0.035f)};
    outerFlame.height = {0.0f, 5.0f};
    outerFlame.verticalVelocity = {0.0f, 22.0f};
    outerFlame.gravity = 9.0f;
    outerFlame.drag = 1.25f;
    outerFlame.spawnRadius = std::max(1.4f, radius * 0.48f);
    outerFlame.spreadRadians = Pi * 1.55f;
    outerFlame.fadeOutFraction = 0.80f;
    outerFlame.emissionRate = 52.0f;
    outerFlame.loop = true;
    outerFlame.depthSorted = true;
    addEmitterWithParent(outerFlame, handle.id);

    MagicFxEmitterConfig sparks;
    sparks.position = position;
    sparks.direction = direction * -1.0f;
    sparks.baseVelocity = direction * -66.0f;
    sparks.particleShape = MagicFxParticleShape::Circle;
    sparks.spawnShape = MagicFxSpawnShape::Circle;
    sparks.startColor = {255, 236, 112, 230};
    sparks.endColor = {255, 64, 16, 0};
    sparks.speed = {26.0f, 84.0f};
    sparks.lifetime = {0.52f, 1.02f};
    sparks.startSize = {0.28f, 0.85f};
    sparks.endSize = {0.0f, 0.12f};
    sparks.height = {0.0f, 4.0f};
    sparks.verticalVelocity = {4.0f, 24.0f};
    sparks.gravity = 12.0f;
    sparks.drag = 0.9f;
    sparks.spawnRadius = std::max(1.0f, radius * 0.34f);
    sparks.spreadRadians = Pi * 1.75f;
    sparks.fadeOutFraction = 0.82f;
    sparks.emissionRate = 24.0f;
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
    smoke.lifetime = {0.46f, 0.92f};
    smoke.startSize = {std::max(1.0f, radius * 0.12f), std::max(2.4f, radius * 0.26f)};
    smoke.endSize = {std::max(1.8f, radius * 0.26f), std::max(4.2f, radius * 0.50f)};
    smoke.height = {2.0f, 7.0f};
    smoke.verticalVelocity = {4.0f, 16.0f};
    smoke.gravity = 2.0f;
    smoke.drag = 1.35f;
    smoke.spawnRadius = std::max(1.0f, radius * 0.34f);
    smoke.spreadRadians = Pi * 1.10f;
    smoke.fadeInFraction = 0.16f;
    smoke.fadeOutFraction = 0.78f;
    smoke.emissionRate = 12.0f;
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
    ignition.particleShape = MagicFxParticleShape::Circle;
    ignition.spawnShape = MagicFxSpawnShape::Ring;
    ignition.startColor = {255, 222, 92, 238};
    ignition.endColor = {255, 58, 18, 0};
    ignition.speed = {18.0f, 64.0f};
    ignition.lifetime = {0.22f, 0.48f};
    ignition.startSize = {0.45f, 1.20f};
    ignition.endSize = {0.0f, 0.22f};
    ignition.height = {0.0f, 2.5f};
    ignition.verticalVelocity = {8.0f, 24.0f};
    ignition.gravity = 14.0f;
    ignition.drag = 1.4f;
    ignition.spawnRadius = radius * 0.56f;
    ignition.spreadRadians = Pi * 2.0f;
    ignition.fadeOutFraction = 0.76f;
    ignition.burstCount = 82;
    ignition.depthSorted = true;
    emitBurst(ignition);

    MagicFxEmitterConfig flames;
    flames.position = position;
    flames.direction = {0.0f, -1.0f};
    flames.particleShape = MagicFxParticleShape::SoftCircle;
    flames.spawnShape = MagicFxSpawnShape::Circle;
    flames.startColor = {255, 82, 22, 190};
    flames.endColor = {255, 184, 58, 0};
    flames.speed = {10.0f, 38.0f};
    flames.lifetime = {0.34f, 0.72f};
    flames.startSize = {radius * 0.045f, radius * 0.115f};
    flames.endSize = {0.0f, radius * 0.026f};
    flames.height = {0.0f, 3.0f};
    flames.verticalVelocity = {14.0f, 42.0f};
    flames.gravity = 24.0f;
    flames.drag = 2.8f;
    flames.spawnRadius = radius * 0.78f;
    flames.spreadRadians = Pi * 1.05f;
    flames.fadeInFraction = 0.03f;
    flames.fadeOutFraction = 0.74f;
    flames.emissionRate = 86.0f;
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
    coreFlames.lifetime = {0.20f, 0.46f};
    coreFlames.startSize = {radius * 0.026f, radius * 0.065f};
    coreFlames.endSize = {0.0f, radius * 0.018f};
    coreFlames.height = {0.0f, 2.5f};
    coreFlames.verticalVelocity = {12.0f, 36.0f};
    coreFlames.gravity = 20.0f;
    coreFlames.drag = 3.4f;
    coreFlames.spawnRadius = radius * 0.48f;
    coreFlames.spreadRadians = Pi * 0.75f;
    coreFlames.fadeOutFraction = 0.76f;
    coreFlames.emissionRate = 56.0f;
    coreFlames.duration = duration * 0.92f;
    coreFlames.loop = false;
    coreFlames.depthSorted = true;
    addEmitter(coreFlames);

    MagicFxEmitterConfig embers;
    embers.position = position;
    embers.direction = {0.0f, -1.0f};
    embers.particleShape = MagicFxParticleShape::Circle;
    embers.spawnShape = MagicFxSpawnShape::Circle;
    embers.startColor = {255, 226, 92, 220};
    embers.endColor = {255, 54, 16, 0};
    embers.speed = {14.0f, 58.0f};
    embers.lifetime = {0.32f, 0.72f};
    embers.startSize = {0.45f, 1.15f};
    embers.endSize = {0.0f, 0.18f};
    embers.height = {0.0f, 4.0f};
    embers.verticalVelocity = {8.0f, 26.0f};
    embers.gravity = 12.0f;
    embers.drag = 1.35f;
    embers.spawnRadius = radius * 0.70f;
    embers.spreadRadians = Pi * 1.55f;
    embers.fadeOutFraction = 0.78f;
    embers.emissionRate = 22.0f;
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
    frost.startColor = {170, 232, 255, 190};
    frost.endColor = {228, 252, 255, 0};
    frost.speed = {1.5f, 7.5f};
    frost.lifetime = {1.10f, 1.85f};
    frost.startSize = {std::max(1.4f, radius * 0.085f), std::max(2.5f, radius * 0.155f)};
    frost.endSize = {0.0f, 0.55f};
    frost.rotation = {0.0f, Pi * 2.0f};
    frost.angularVelocity = {-0.65f, 0.65f};
    frost.height = {0.0f, 5.0f};
    frost.verticalVelocity = {0.0f, 4.0f};
    frost.gravity = 0.9f;
    frost.drag = 1.0f;
    frost.spawnRadius = std::max(3.0f, radius * 0.82f);
    frost.spreadRadians = Pi * 1.05f;
    frost.fadeInFraction = 0.12f;
    frost.fadeOutFraction = 0.74f;
    frost.emissionRate = 10.0f;
    frost.loop = true;
    frost.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(frost);

    MagicFxEmitterConfig mist;
    mist.position = position;
    mist.direction = {0.0f, -1.0f};
    mist.particleShape = MagicFxParticleShape::SoftCircle;
    mist.spawnShape = MagicFxSpawnShape::Circle;
    mist.startColor = {176, 236, 255, 78};
    mist.endColor = {234, 252, 255, 0};
    mist.speed = {1.0f, 5.5f};
    mist.lifetime = {1.15f, 2.10f};
    mist.startSize = {radius * 0.16f, radius * 0.32f};
    mist.endSize = {radius * 0.38f, radius * 0.72f};
    mist.height = {0.0f, 4.0f};
    mist.verticalVelocity = {1.0f, 6.0f};
    mist.gravity = 0.35f;
    mist.drag = 0.75f;
    mist.spawnRadius = radius * 0.66f;
    mist.spreadRadians = Pi * 0.82f;
    mist.fadeInFraction = 0.24f;
    mist.fadeOutFraction = 0.76f;
    mist.emissionRate = 24.0f;
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
    glints.speed = {0.0f, 4.0f};
    glints.lifetime = {0.48f, 0.95f};
    glints.startSize = {0.55f, 1.25f};
    glints.endSize = {0.0f, 0.18f};
    glints.rotation = {0.0f, Pi * 2.0f};
    glints.height = {1.0f, 6.0f};
    glints.verticalVelocity = {0.0f, 2.0f};
    glints.gravity = 0.2f;
    glints.drag = 0.7f;
    glints.spawnRadius = radius * 0.86f;
    glints.spreadRadians = Pi * 2.0f;
    glints.stretch = 1.35f;
    glints.fadeInFraction = 0.18f;
    glints.fadeOutFraction = 0.70f;
    glints.emissionRate = 6.0f;
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
    crystal.baseVelocity = direction * -4.0f;
    crystal.particleShape = MagicFxParticleShape::Crystal;
    crystal.spawnShape = MagicFxSpawnShape::Circle;
    crystal.startColor = {210, 246, 255, 232};
    crystal.endColor = {110, 204, 255, 0};
    crystal.speed = {0.0f, 0.6f};
    crystal.lifetime = {0.42f, 0.42f};
    crystal.startSize = {std::max(5.0f, radius * 0.62f), std::max(5.0f, radius * 0.62f)};
    crystal.endSize = crystal.startSize;
    crystal.rotation = {forwardAngle, forwardAngle};
    crystal.angularVelocity = {0.0f, 0.0f};
    crystal.height = {0.0f, 0.8f};
    crystal.verticalVelocity = {0.0f, 0.0f};
    crystal.drag = 1.8f;
    crystal.spawnRadius = std::max(0.2f, radius * 0.035f);
    crystal.spreadRadians = Pi * 0.04f;
    crystal.fadeOutFraction = 0.66f;
    crystal.emissionRate = 34.0f;
    crystal.loop = true;
    crystal.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(crystal);

    MagicFxEmitterConfig facets;
    facets.position = position;
    facets.direction = direction * -1.0f;
    facets.baseVelocity = direction * -5.0f;
    facets.particleShape = MagicFxParticleShape::Shard;
    facets.spawnShape = MagicFxSpawnShape::Circle;
    facets.startColor = {246, 255, 255, 190};
    facets.endColor = {138, 222, 255, 0};
    facets.speed = {0.0f, 2.0f};
    facets.lifetime = {0.42f, 0.42f};
    facets.startSize = {std::max(1.2f, radius * 0.16f), std::max(1.2f, radius * 0.16f)};
    facets.endSize = facets.startSize;
    facets.rotation = {forwardAngle, forwardAngle};
    facets.angularVelocity = {0.0f, 0.0f};
    facets.height = {0.0f, 1.0f};
    facets.verticalVelocity = {0.0f, 0.0f};
    facets.drag = 1.8f;
    facets.spawnRadius = std::max(0.4f, radius * 0.10f);
    facets.spreadRadians = Pi * 0.10f;
    facets.fadeOutFraction = 0.76f;
    facets.emissionRate = 2.0f;
    facets.loop = true;
    facets.depthSorted = true;
    addEmitterWithParent(facets, handle.id);

    MagicFxEmitterConfig mist;
    mist.position = position;
    mist.direction = direction * -1.0f;
    mist.baseVelocity = direction * -10.0f;
    mist.particleShape = MagicFxParticleShape::SoftCircle;
    mist.spawnShape = MagicFxSpawnShape::Point;
    mist.startColor = {174, 232, 255, 158};
    mist.endColor = {224, 252, 255, 0};
    mist.speed = {1.0f, 6.0f};
    mist.lifetime = {0.34f, 0.74f};
    mist.startSize = {std::max(0.8f, radius * 0.10f), std::max(1.8f, radius * 0.20f)};
    mist.endSize = {std::max(0.8f, radius * 0.10f), std::max(2.8f, radius * 0.30f)};
    mist.height = {0.0f, 2.0f};
    mist.verticalVelocity = {0.0f, 1.2f};
    mist.drag = 1.5f;
    mist.spreadRadians = Pi * 0.20f;
    mist.fadeOutFraction = 0.78f;
    mist.emissionRate = 10.0f;
    mist.loop = true;
    mist.depthSorted = true;
    addEmitterWithParent(mist, handle.id);

    MagicFxEmitterConfig powder;
    powder.position = position;
    powder.direction = direction * -1.0f;
    powder.baseVelocity = direction * -8.0f;
    powder.particleShape = MagicFxParticleShape::Circle;
    powder.spawnShape = MagicFxSpawnShape::Circle;
    powder.startColor = {244, 255, 255, 190};
    powder.endColor = {170, 230, 255, 0};
    powder.speed = {1.0f, 7.0f};
    powder.lifetime = {0.42f, 0.42f};
    powder.startSize = {0.9f, 0.9f};
    powder.endSize = powder.startSize;
    powder.height = {0.0f, 3.0f};
    powder.verticalVelocity = {0.0f, 1.0f};
    powder.gravity = 0.2f;
    powder.drag = 1.6f;
    powder.spawnRadius = std::max(0.6f, radius * 0.14f);
    powder.spreadRadians = Pi * 0.24f;
    powder.fadeOutFraction = 0.82f;
    powder.emissionRate = 4.0f;
    powder.loop = true;
    powder.depthSorted = true;
    addEmitterWithParent(powder, handle.id);

    MagicFxEmitterConfig glints;
    glints.position = position;
    glints.direction = direction * -1.0f;
    glints.baseVelocity = direction * -8.0f;
    glints.particleShape = MagicFxParticleShape::SparkLine;
    glints.spawnShape = MagicFxSpawnShape::Circle;
    glints.startColor = {255, 255, 255, 235};
    glints.endColor = {150, 224, 255, 0};
    glints.speed = {2.0f, 10.0f};
    glints.lifetime = {0.28f, 0.28f};
    glints.startSize = {0.65f, 0.65f};
    glints.endSize = glints.startSize;
    glints.rotation = {forwardAngle, forwardAngle};
    glints.height = {0.0f, 3.0f};
    glints.verticalVelocity = {0.0f, 0.6f};
    glints.drag = 1.4f;
    glints.spawnRadius = std::max(0.4f, radius * 0.08f);
    glints.spreadRadians = Pi * 0.12f;
    glints.stretch = 1.5f;
    glints.fadeOutFraction = 0.72f;
    glints.emissionRate = 4.0f;
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
    largeShards.speed = {46.0f, 142.0f};
    largeShards.lifetime = {0.72f, 1.22f};
    largeShards.startSize = {3.2f, 7.2f};
    largeShards.endSize = {0.0f, 1.0f};
    largeShards.rotation = {0.0f, Pi * 2.0f};
    largeShards.angularVelocity = {-8.0f, 8.0f};
    largeShards.height = {8.0f, 22.0f};
    largeShards.verticalVelocity = {118.0f, 260.0f};
    largeShards.gravity = 620.0f;
    largeShards.drag = 1.15f;
    largeShards.spawnRadius = radius * 0.22f;
    largeShards.spreadRadians = Pi * 2.0f;
    largeShards.fadeOutFraction = 0.78f;
    largeShards.burstCount = 24;
    largeShards.bounceRestitution = 0.42f;
    largeShards.groundFriction = 8.6f;
    largeShards.maxBounces = 2;
    largeShards.depthSorted = true;
    emitBurst(largeShards);

    MagicFxEmitterConfig smallShards = largeShards;
    smallShards.startColor = {232, 252, 255, 220};
    smallShards.endColor = {142, 222, 255, 0};
    smallShards.speed = {58.0f, 176.0f};
    smallShards.lifetime = {0.54f, 0.98f};
    smallShards.startSize = {1.0f, 2.8f};
    smallShards.endSize = {0.0f, 0.35f};
    smallShards.height = {5.0f, 15.0f};
    smallShards.verticalVelocity = {86.0f, 210.0f};
    smallShards.gravity = 560.0f;
    smallShards.drag = 1.0f;
    smallShards.spawnRadius = radius * 0.30f;
    smallShards.burstCount = 58;
    smallShards.bounceRestitution = 0.36f;
    smallShards.groundFriction = 10.0f;
    smallShards.maxBounces = 1;
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
    radius = std::max(4.0f, radius);

    MagicFxEmitterConfig arcs;
    arcs.position = position;
    arcs.direction = {1.0f, 0.0f};
    arcs.particleShape = MagicFxParticleShape::ThunderArc;
    arcs.spawnShape = MagicFxSpawnShape::Ring;
    arcs.startColor = {255, 244, 176, 232};
    arcs.endColor = {255, 232, 146, 0};
    arcs.speed = {10.0f, 22.0f};
    arcs.lifetime = {0.32f, 0.52f};
    arcs.startSize = {std::max(2.4f, radius * 0.16f), std::max(4.2f, radius * 0.28f)};
    arcs.endSize = {std::max(1.2f, radius * 0.08f), std::max(2.4f, radius * 0.16f)};
    arcs.rotation = {0.0f, Pi * 2.0f};
    arcs.spawnRadius = std::max(2.0f, radius * 0.56f);
    arcs.spreadRadians = Pi * 0.30f;
    arcs.stretch = 2.55f;
    arcs.fadeInFraction = 0.02f;
    arcs.fadeOutFraction = 0.68f;
    arcs.emissionRate = 7.0f;
    arcs.loop = true;
    arcs.depthSorted = true;
    return addEmitter(arcs);
}

void MagicFxSystem::playThunderStrike(Vec2 origin, Vec2 target, bool strong)
{
    LightningStrike strike;
    strike.active = true;
    strike.strong = strong;
    strike.lifetime = strong ? sampleRange({0.20f, 0.28f}) : sampleRange({0.15f, 0.21f});
    strike.outerWidth = strong ? sampleRange({10.0f, 15.5f}) : sampleRange({5.8f, 9.0f});
    strike.coreWidth = strong ? sampleRange({2.6f, 4.2f}) : sampleRange({1.6f, 2.6f});

    Vec2 sourceBias = origin - target;
    if (lengthSquared(sourceBias) <= 0.0001f) {
        sourceBias = {random01() - 0.5f, random01() - 0.5f};
    }
    const float skyHeight = strong ? sampleRange({148.0f, 190.0f}) : sampleRange({108.0f, 146.0f});
    const float startOffsetX = std::clamp(sourceBias.x * 0.18f + (random01() - 0.5f) * (strong ? 52.0f : 34.0f), strong ? -58.0f : -38.0f, strong ? 58.0f : 38.0f);
    const Vec2 start = target + Vec2{startOffsetX, -skyHeight};
    const int segmentCount = strong ? 9 : 7;
    strike.pointCount = segmentCount + 1;
    strike.points[0] = start;
    for (int i = 1; i < segmentCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segmentCount);
        const float centerWeight = 1.0f - std::abs(t - 0.5f) * 1.15f;
        const float amplitude = (strong ? 62.0f : 36.0f) * std::max(0.18f, centerWeight);
        const float previousX = strike.points[static_cast<std::size_t>(i - 1)].x;
        const float baseX = lerp(start.x, target.x, t);
        const float kinkX = baseX + (random01() - 0.5f) * amplitude;
        const float minStep = strong ? 14.0f : 8.0f;
        const float clampedX = std::clamp(kinkX, previousX - amplitude - minStep, previousX + amplitude + minStep);
        const float y = lerp(start.y, target.y, t);
        strike.points[static_cast<std::size_t>(i)] = {clampedX, y};
    }
    strike.points[static_cast<std::size_t>(segmentCount)] = target;

    if (lightningStrikes_.size() >= MaxLightningStrikes) {
        lightningStrikes_.erase(lightningStrikes_.begin());
    }
    lightningStrikes_.push_back(strike);

    const int groundArcCount = strong ? 10 : 7;
    for (int i = 0; i < groundArcCount; ++i) {
        ThunderImpactArc arc;
        arc.active = true;
        arc.style = ThunderImpactArcStyle::Ground;
        arc.origin = target;
        arc.strong = strong;
        arc.startDelay = sampleRange(strong ? MagicFxRange{0.010f, 0.105f} : MagicFxRange{0.015f, 0.13f});
        arc.lifetime = sampleRange(strong ? MagicFxRange{0.20f, 0.42f} : MagicFxRange{0.17f, 0.36f});
        arc.outerWidth = sampleRange(strong ? MagicFxRange{2.6f, 5.4f} : MagicFxRange{2.0f, 4.1f});
        arc.coreWidth = sampleRange(strong ? MagicFxRange{0.9f, 1.8f} : MagicFxRange{0.7f, 1.4f});
        arc.phase = random01();
        arc.pointCount = 5 + static_cast<int>(random01() * 4.0f);
        arc.pointCount = std::clamp(arc.pointCount, 5, static_cast<int>(arc.points.size()));
        const float baseAngle = random01() * Pi * 2.0f;
        const float totalLength = sampleRange(strong ? MagicFxRange{42.0f, 104.0f} : MagicFxRange{30.0f, 76.0f});
        const float startRadius = sampleRange({2.0f, 8.0f});
        Vec2 current = target + fromAngle(baseAngle + sampleRange({-0.48f, 0.48f})) * startRadius;
        float angle = baseAngle + sampleRange({-0.96f, 0.96f});
        arc.points[0] = current;
        float traveled = 0.0f;
        for (int p = 1; p < arc.pointCount; ++p) {
            const float t = static_cast<float>(p) / static_cast<float>(arc.pointCount - 1);
            const float remaining = std::max(0.0f, totalLength - traveled);
            const float baseStep = remaining / static_cast<float>(arc.pointCount - p);
            angle = steerRadians(angle + sampleRange({-1.45f, 1.45f}) * lerp(1.18f, 0.70f, t), baseAngle, 0.07f + 0.08f * t);
            const float step = baseStep * sampleRange({0.48f, 1.62f});
            const Vec2 forward = fromAngle(angle);
            const Vec2 side = perpendicular(forward);
            current += forward * step + side * sampleRange({-0.52f, 0.52f}) * step;
            traveled += step;
            arc.points[static_cast<std::size_t>(p)] = current;
        }
        addThunderImpactArc(arc);
    }

    const int verticalArcCount = strong ? 5 : 3;
    for (int i = 0; i < verticalArcCount; ++i) {
        ThunderImpactArc arc;
        arc.active = true;
        arc.style = ThunderImpactArcStyle::Vertical;
        arc.origin = target;
        arc.strong = strong;
        arc.startDelay = sampleRange(strong ? MagicFxRange{0.0f, 0.07f} : MagicFxRange{0.0f, 0.09f});
        arc.lifetime = sampleRange(strong ? MagicFxRange{0.13f, 0.28f} : MagicFxRange{0.11f, 0.24f});
        arc.outerWidth = sampleRange(strong ? MagicFxRange{2.1f, 4.2f} : MagicFxRange{1.5f, 3.2f});
        arc.coreWidth = sampleRange(strong ? MagicFxRange{0.8f, 1.6f} : MagicFxRange{0.6f, 1.2f});
        arc.phase = random01();
        arc.pointCount = 4 + static_cast<int>(random01() * 3.0f);
        arc.pointCount = std::clamp(arc.pointCount, 4, static_cast<int>(arc.points.size()));
        const float totalLength = sampleRange(strong ? MagicFxRange{32.0f, 74.0f} : MagicFxRange{22.0f, 54.0f});
        const float horizontalDrift = sampleRange(strong ? MagicFxRange{-34.0f, 34.0f} : MagicFxRange{-22.0f, 22.0f});
        Vec2 current = target + Vec2{sampleRange({-8.0f, 8.0f}), sampleRange({-2.0f, 5.0f})};
        arc.points[0] = current;
        for (int p = 1; p < arc.pointCount; ++p) {
            const float t = static_cast<float>(p) / static_cast<float>(arc.pointCount - 1);
            const float y = target.y - totalLength * t + sampleRange({-9.0f, 7.0f});
            const float x = target.x + horizontalDrift * t + sampleRange({-17.0f, 17.0f}) * (1.0f - t * 0.25f);
            current = {x, y};
            arc.points[static_cast<std::size_t>(p)] = current;
        }
        addThunderImpactArc(arc);
    }

    const int sigilArcCount = strong ? 4 : 2;
    for (int i = 0; i < sigilArcCount; ++i) {
        ThunderImpactArc arc;
        arc.active = true;
        arc.style = ThunderImpactArcStyle::Sigil;
        arc.origin = target;
        arc.strong = strong;
        arc.startDelay = sampleRange(strong ? MagicFxRange{0.04f, 0.16f} : MagicFxRange{0.05f, 0.18f});
        arc.lifetime = sampleRange(strong ? MagicFxRange{0.24f, 0.48f} : MagicFxRange{0.20f, 0.40f});
        arc.outerWidth = sampleRange(strong ? MagicFxRange{1.1f, 2.2f} : MagicFxRange{0.9f, 1.7f});
        arc.coreWidth = sampleRange({0.8f, 1.2f});
        arc.phase = random01();
        arc.pointCount = 5 + static_cast<int>(random01() * 4.0f);
        arc.pointCount = std::clamp(arc.pointCount, 5, static_cast<int>(arc.points.size()));
        const float radius = sampleRange(strong ? MagicFxRange{18.0f, 58.0f} : MagicFxRange{16.0f, 44.0f});
        const float startAngle = random01() * Pi * 2.0f;
        const float sweep = sampleRange({0.65f, 1.85f}) * (random01() < 0.5f ? -1.0f : 1.0f);
        for (int p = 0; p < arc.pointCount; ++p) {
            const float t = static_cast<float>(p) / static_cast<float>(arc.pointCount - 1);
            const float angle = startAngle + sweep * t + sampleRange({-0.035f, 0.035f});
            const float pointRadius = radius * sampleRange({0.92f, 1.08f});
            arc.points[static_cast<std::size_t>(p)] = target + fromAngle(angle) * pointRadius;
        }
        addThunderImpactArc(arc);
    }

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

    MagicFxEmitterConfig blades;
    blades.position = position;
    blades.direction = {1.0f, 0.0f};
    blades.particleShape = MagicFxParticleShape::WindBlade;
    blades.spawnShape = MagicFxSpawnShape::Ring;
    blades.startColor = {190, 255, 210, 176};
    blades.endColor = {232, 255, 238, 0};
    blades.speed = {24.0f, 52.0f};
    blades.lifetime = {0.46f, 0.82f};
    blades.startSize = {std::max(2.4f, radius * 0.16f), std::max(4.6f, radius * 0.30f)};
    blades.endSize = {std::max(1.0f, radius * 0.070f), std::max(2.2f, radius * 0.15f)};
    blades.rotation = {0.0f, Pi * 2.0f};
    blades.angularVelocity = {-7.4f, 7.4f};
    blades.drag = 1.20f;
    blades.spawnRadius = radius * 0.40f;
    blades.spreadRadians = Pi * 1.25f;
    blades.stretch = 2.35f;
    blades.fadeInFraction = 0.06f;
    blades.fadeOutFraction = 0.76f;
    blades.emissionRate = 16.0f;
    blades.loop = true;
    blades.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(blades);

    MagicFxEmitterConfig smallBlades = blades;
    smallBlades.startColor = {224, 255, 218, 132};
    smallBlades.endColor = {150, 246, 190, 0};
    smallBlades.speed = {30.0f, 64.0f};
    smallBlades.lifetime = {0.34f, 0.62f};
    smallBlades.startSize = {std::max(1.4f, radius * 0.090f), std::max(2.8f, radius * 0.180f)};
    smallBlades.endSize = {0.0f, std::max(1.0f, radius * 0.070f)};
    smallBlades.angularVelocity = {-8.8f, 8.8f};
    smallBlades.drag = 1.35f;
    smallBlades.spawnRadius = radius * 0.48f;
    smallBlades.spreadRadians = Pi * 1.32f;
    smallBlades.stretch = 2.05f;
    smallBlades.emissionRate = 9.0f;
    addEmitterWithParent(smallBlades, handle.id);

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
    crescent.baseVelocity = direction * -14.0f;
    crescent.particleShape = MagicFxParticleShape::WindCrescent;
    crescent.spawnShape = MagicFxSpawnShape::Circle;
    crescent.startColor = {190, 255, 210, 166};
    crescent.endColor = {235, 255, 238, 0};
    crescent.speed = {0.0f, 3.0f};
    crescent.lifetime = {0.32f, 0.52f};
    crescent.startSize = {radius * 0.42f, radius * 0.60f};
    crescent.endSize = {radius * 0.24f, radius * 0.40f};
    crescent.rotation = {angle, angle};
    crescent.angularVelocity = {0.0f, 0.0f};
    crescent.spawnRadius = radius * 0.035f;
    crescent.spreadRadians = Pi * 0.08f;
    crescent.stretch = 2.70f;
    crescent.fadeInFraction = 0.08f;
    crescent.fadeOutFraction = 0.84f;
    crescent.emissionRate = 16.0f;
    crescent.loop = true;
    crescent.depthSorted = true;
    return addEmitter(crescent);
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
    rocks.particleShape = MagicFxParticleShape::DirtClod;
    rocks.spawnShape = MagicFxSpawnShape::Ring;
    rocks.startColor = {178, 128, 78, 232};
    rocks.endColor = {138, 96, 62, 170};
    rocks.alphaScale = {0.62f, 1.0f};
    rocks.speed = {0.2f, 1.6f};
    rocks.lifetime = {10.0f, 15.0f};
    rocks.startSize = {4.0f, 8.8f};
    rocks.endSize = {3.8f, 8.4f};
    rocks.rotation = {0.0f, Pi * 2.0f};
    rocks.angularVelocity = {-0.28f, 0.28f};
    rocks.height = {11.0f, 30.0f};
    rocks.verticalVelocity = {-0.75f, 0.95f};
    rocks.gravity = 0.06f;
    rocks.drag = 0.16f;
    rocks.spawnRadius = std::max(4.0f, radius * 0.62f);
    rocks.spreadRadians = Pi * 2.0f;
    rocks.fadeInFraction = 0.03f;
    rocks.fadeOutFraction = 0.06f;
    rocks.emissionRate = 0.45f;
    rocks.burstCount = 10;
    rocks.loop = true;
    rocks.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(rocks);

    MagicFxEmitterConfig dustCloud;
    dustCloud.position = position;
    dustCloud.direction = {0.0f, -1.0f};
    dustCloud.particleShape = MagicFxParticleShape::SoftCircle;
    dustCloud.spawnShape = MagicFxSpawnShape::Circle;
    dustCloud.startColor = {150, 112, 74, 82};
    dustCloud.endColor = {104, 82, 62, 0};
    dustCloud.alphaScale = {0.50f, 0.92f};
    dustCloud.speed = {0.6f, 4.8f};
    dustCloud.lifetime = {1.35f, 2.50f};
    dustCloud.startSize = {radius * 0.14f, radius * 0.32f};
    dustCloud.endSize = {radius * 0.36f, radius * 0.76f};
    dustCloud.height = {0.0f, 7.0f};
    dustCloud.verticalVelocity = {0.4f, 4.4f};
    dustCloud.gravity = 0.18f;
    dustCloud.drag = 0.75f;
    dustCloud.spawnRadius = std::max(2.0f, radius * 0.46f);
    dustCloud.spreadRadians = Pi * 0.90f;
    dustCloud.fadeInFraction = 0.24f;
    dustCloud.fadeOutFraction = 0.76f;
    dustCloud.emissionRate = 22.0f;
    dustCloud.loop = true;
    dustCloud.depthSorted = true;
    addEmitterWithParent(dustCloud, handle.id);

    MagicFxEmitterConfig sand;
    sand.position = position;
    sand.direction = {0.0f, 1.0f};
    sand.particleShape = MagicFxParticleShape::SoftCircle;
    sand.spawnShape = MagicFxSpawnShape::Ring;
    sand.startColor = {178, 134, 82, 46};
    sand.endColor = {84, 64, 48, 0};
    sand.alphaScale = {0.35f, 0.86f};
    sand.speed = {0.4f, 4.0f};
    sand.lifetime = {0.82f, 1.42f};
    sand.startSize = {0.8f, 2.2f};
    sand.endSize = {0.0f, 0.6f};
    sand.height = {0.0f, 5.0f};
    sand.verticalVelocity = {-2.0f, 2.0f};
    sand.gravity = 2.6f;
    sand.spawnRadius = std::max(3.0f, radius * 0.76f);
    sand.spreadRadians = Pi * 2.0f;
    sand.fadeInFraction = 0.12f;
    sand.fadeOutFraction = 0.74f;
    sand.emissionRate = 5.0f;
    sand.loop = true;
    sand.depthSorted = true;
    addEmitterWithParent(sand, handle.id);

    return handle;
}

MagicFxEmitterHandle MagicFxSystem::startDirtClodLoop(Vec2 position, Vec2 direction, float radius)
{
    direction = normalize(direction);

    MagicFxEmitterConfig rock;
    rock.position = position;
    rock.direction = direction;
    rock.particleShape = MagicFxParticleShape::DirtClod;
    rock.spawnShape = MagicFxSpawnShape::Circle;
    rock.startColor = {166, 112, 66, 238};
    rock.endColor = {82, 58, 42, 0};
    rock.alphaScale = {0.70f, 1.0f};
    rock.speed = {0.0f, 1.6f};
    rock.lifetime = {0.24f, 0.42f};
    rock.startSize = {radius * 0.78f, radius * 1.32f};
    rock.endSize = {radius * 0.70f, radius * 1.12f};
    rock.rotation = {0.0f, Pi * 2.0f};
    rock.angularVelocity = {-0.65f, 0.65f};
    rock.spawnRadius = std::max(0.8f, radius * 0.12f);
    rock.spreadRadians = Pi * 2.0f;
    rock.fadeInFraction = 0.04f;
    rock.fadeOutFraction = 0.62f;
    rock.emissionRate = 22.0f;
    rock.loop = true;
    rock.depthSorted = true;
    const MagicFxEmitterHandle handle = addEmitter(rock);

    MagicFxEmitterConfig dust;
    dust.position = position;
    dust.direction = direction * -1.0f;
    dust.baseVelocity = direction * -18.0f;
    dust.particleShape = MagicFxParticleShape::SoftCircle;
    dust.spawnShape = MagicFxSpawnShape::Circle;
    dust.startColor = {150, 104, 66, 98};
    dust.endColor = {92, 70, 54, 0};
    dust.speed = {2.0f, 12.0f};
    dust.lifetime = {0.34f, 0.64f};
    dust.startSize = {radius * 0.42f, radius * 0.82f};
    dust.endSize = {radius * 0.72f, radius * 1.30f};
    dust.height = {0.0f, 2.0f};
    dust.verticalVelocity = {1.0f, 6.0f};
    dust.gravity = 4.0f;
    dust.drag = 1.6f;
    dust.spawnRadius = std::max(1.0f, radius * 0.45f);
    dust.spreadRadians = Pi * 0.95f;
    dust.fadeInFraction = 0.12f;
    dust.fadeOutFraction = 0.72f;
    dust.emissionRate = 18.0f;
    dust.loop = true;
    dust.depthSorted = true;
    addEmitterWithParent(dust, handle.id);

    MagicFxEmitterConfig grit;
    grit.position = position;
    grit.direction = direction * -1.0f;
    grit.baseVelocity = direction * -26.0f;
    grit.particleShape = MagicFxParticleShape::DirtClod;
    grit.spawnShape = MagicFxSpawnShape::Cone;
    grit.startColor = {176, 124, 72, 205};
    grit.endColor = {80, 58, 42, 0};
    grit.alphaScale = {0.42f, 0.86f};
    grit.speed = {4.0f, 20.0f};
    grit.lifetime = {0.22f, 0.44f};
    grit.startSize = {1.0f, 3.2f};
    grit.endSize = {0.0f, 0.8f};
    grit.rotation = {0.0f, Pi * 2.0f};
    grit.angularVelocity = {-1.4f, 1.4f};
    grit.height = {0.0f, 3.0f};
    grit.verticalVelocity = {1.0f, 14.0f};
    grit.gravity = 14.0f;
    grit.drag = 1.3f;
    grit.spawnRadius = std::max(1.0f, radius * 0.26f);
    grit.spreadRadians = Pi * 0.86f;
    grit.fadeOutFraction = 0.78f;
    grit.emissionRate = 10.0f;
    grit.loop = true;
    grit.depthSorted = true;
    addEmitterWithParent(grit, handle.id);

    return handle;
}

void MagicFxSystem::playEarthSpikeRise(Vec2 position, float radius)
{
    radius = std::max(8.0f, radius);
    const float spikeLifetime = sampleRange({1.02f, 1.10f});
    constexpr float SpikeFadeOutFraction = 0.14f;

    MagicFxEmitterConfig spikeBody;
    spikeBody.position = position;
    spikeBody.direction = {0.0f, -1.0f};
    spikeBody.particleShape = MagicFxParticleShape::EarthSpike;
    spikeBody.spawnShape = MagicFxSpawnShape::Point;
    spikeBody.startColor = {148, 102, 64, 245};
    spikeBody.endColor = {110, 78, 56, 220};
    spikeBody.alphaScale = {0.86f, 1.0f};
    spikeBody.speed = {0.0f, 0.0f};
    spikeBody.lifetime = {spikeLifetime, spikeLifetime};
    spikeBody.startSize = {radius * 1.02f, radius * 1.02f};
    spikeBody.endSize = {radius * 0.98f, radius * 0.98f};
    spikeBody.rotation = {-Pi * 0.50f, -Pi * 0.50f};
    spikeBody.angularVelocity = {0.0f, 0.0f};
    spikeBody.height = {0.0f, 0.0f};
    spikeBody.verticalVelocity = {0.0f, 0.0f};
    spikeBody.gravity = 0.0f;
    spikeBody.drag = 0.0f;
    spikeBody.spawnRadius = 0.0f;
    spikeBody.spreadRadians = 0.0f;
    spikeBody.fadeInFraction = 0.04f;
    spikeBody.fadeOutFraction = SpikeFadeOutFraction;
    spikeBody.burstCount = 1;
    spikeBody.depthSorted = true;
    emitBurst(spikeBody);

    MagicFxEmitterConfig cracks;
    cracks.position = position;
    cracks.direction = {1.0f, 0.0f};
    cracks.particleShape = MagicFxParticleShape::EarthCrack;
    cracks.spawnShape = MagicFxSpawnShape::Point;
    cracks.startColor = {34, 24, 18, 228};
    cracks.endColor = {34, 24, 18, 218};
    cracks.alphaScale = {0.62f, 1.0f};
    cracks.speed = {0.0f, 0.0f};
    cracks.lifetime = {spikeLifetime, spikeLifetime};
    cracks.startSize = {radius * 0.50f, radius * 1.18f};
    cracks.endSize = {radius * 0.50f, radius * 1.18f};
    cracks.rotation = {0.0f, 0.0f};
    cracks.spawnRadius = 0.0f;
    cracks.spreadRadians = Pi * 2.0f;
    cracks.fadeInFraction = 0.0f;
    cracks.fadeOutFraction = SpikeFadeOutFraction;
    cracks.burstCount = 1;
    cracks.depthSorted = false;
    constexpr int CrackCount = 7;
    const float crackSector = (Pi * 2.0f) / static_cast<float>(CrackCount);
    const float crackRotationOffset = random01() * Pi * 2.0f;
    for (int i = 0; i < CrackCount; ++i) {
        const float angle = crackRotationOffset + crackSector * (static_cast<float>(i) + (random01() - 0.5f) * 0.46f);
        const float rootAngle = angle + (random01() - 0.5f) * 0.34f;
        const float rootDistance = radius * lerp(0.02f, 0.14f, random01());
        const float length = radius * lerp(0.56f, 1.22f, random01());
        cracks.position = position + fromAngle(rootAngle) * rootDistance;
        cracks.rotation = {angle, angle};
        cracks.startSize = {length * 0.82f, length};
        cracks.endSize = cracks.startSize;
        cracks.alphaScale = {0.52f, 0.92f};
        emitBurst(cracks);
    }

    MagicFxEmitterConfig motes;
    motes.position = position;
    motes.direction = {0.0f, -1.0f};
    motes.particleShape = MagicFxParticleShape::Circle;
    motes.spawnShape = MagicFxSpawnShape::Circle;
    motes.startColor = {248, 218, 132, 172};
    motes.endColor = {172, 122, 68, 0};
    motes.alphaScale = {0.42f, 0.88f};
    motes.speed = {2.0f, 18.0f};
    motes.lifetime = {0.72f, 1.48f};
    motes.startSize = {1.0f, 2.6f};
    motes.endSize = {0.0f, 0.8f};
    motes.height = {4.0f, 34.0f};
    motes.verticalVelocity = {5.0f, 20.0f};
    motes.gravity = 4.0f;
    motes.drag = 1.1f;
    motes.spawnRadius = radius * 0.72f;
    motes.spreadRadians = Pi * 2.0f;
    motes.fadeInFraction = 0.16f;
    motes.fadeOutFraction = 0.72f;
    motes.emissionRate = 82.0f;
    motes.duration = 0.58f;
    motes.loop = false;
    motes.depthSorted = true;
    addEmitter(motes);

    MagicFxEmitterConfig glints;
    glints.position = position;
    glints.direction = {0.0f, -1.0f};
    glints.particleShape = MagicFxParticleShape::SparkLine;
    glints.spawnShape = MagicFxSpawnShape::Circle;
    glints.startColor = {255, 236, 154, 210};
    glints.endColor = {196, 144, 78, 0};
    glints.alphaScale = {0.36f, 0.86f};
    glints.speed = {0.0f, 12.0f};
    glints.lifetime = {0.30f, 0.78f};
    glints.startSize = {0.8f, 1.9f};
    glints.endSize = {0.0f, 0.3f};
    glints.rotation = {0.0f, Pi * 2.0f};
    glints.angularVelocity = {-2.2f, 2.2f};
    glints.height = {10.0f, 46.0f};
    glints.verticalVelocity = {1.0f, 11.0f};
    glints.gravity = 2.0f;
    glints.drag = 1.35f;
    glints.spawnRadius = radius * 0.58f;
    glints.spreadRadians = Pi * 2.0f;
    glints.stretch = 2.1f;
    glints.fadeInFraction = 0.12f;
    glints.fadeOutFraction = 0.72f;
    glints.emissionRate = 46.0f;
    glints.duration = 0.52f;
    glints.loop = false;
    glints.depthSorted = true;
    addEmitter(glints);

    MagicFxEmitterConfig dust;
    dust.position = position;
    dust.direction = {0.0f, -1.0f};
    dust.particleShape = MagicFxParticleShape::SoftCircle;
    dust.spawnShape = MagicFxSpawnShape::Ring;
    dust.startColor = {132, 92, 58, 118};
    dust.endColor = {80, 62, 48, 0};
    dust.alphaScale = {0.42f, 0.86f};
    dust.speed = {16.0f, 86.0f};
    dust.lifetime = {0.30f, 0.72f};
    dust.startSize = {radius * 0.10f, radius * 0.28f};
    dust.endSize = {radius * 0.36f, radius * 0.78f};
    dust.height = {0.0f, 2.0f};
    dust.verticalVelocity = {8.0f, 28.0f};
    dust.gravity = 22.0f;
    dust.drag = 1.6f;
    dust.spawnRadius = radius * 0.50f;
    dust.spreadRadians = Pi * 1.35f;
    dust.fadeOutFraction = 0.76f;
    dust.burstCount = 44;
    dust.depthSorted = true;
    emitBurst(dust);

    MagicFxEmitterConfig sand;
    sand.position = position;
    sand.direction = {0.0f, -1.0f};
    sand.particleShape = MagicFxParticleShape::SoftCircle;
    sand.spawnShape = MagicFxSpawnShape::Circle;
    sand.startColor = {190, 144, 92, 76};
    sand.endColor = {116, 86, 62, 0};
    sand.alphaScale = {0.28f, 0.78f};
    sand.speed = {10.0f, 54.0f};
    sand.lifetime = {0.48f, 1.10f};
    sand.startSize = {0.8f, 2.8f};
    sand.endSize = {0.0f, 0.8f};
    sand.height = {0.0f, 5.0f};
    sand.verticalVelocity = {12.0f, 42.0f};
    sand.gravity = 26.0f;
    sand.drag = 1.7f;
    sand.spawnRadius = radius * 0.52f;
    sand.spreadRadians = Pi * 1.7f;
    sand.fadeInFraction = 0.06f;
    sand.fadeOutFraction = 0.78f;
    sand.burstCount = 62;
    sand.depthSorted = true;
    emitBurst(sand);

    MagicFxEmitterConfig shards;
    shards.position = position;
    shards.direction = {0.0f, -1.0f};
    shards.particleShape = MagicFxParticleShape::DirtClod;
    shards.spawnShape = MagicFxSpawnShape::Circle;
    shards.startColor = {170, 122, 74, 220};
    shards.endColor = {82, 58, 38, 0};
    shards.alphaScale = {0.55f, 1.0f};
    shards.speed = {16.0f, 58.0f};
    shards.lifetime = {0.26f, 0.54f};
    shards.startSize = {2.8f, 9.8f};
    shards.endSize = {0.0f, 1.2f};
    shards.rotation = {0.0f, Pi * 2.0f};
    shards.angularVelocity = {-7.0f, 7.0f};
    shards.height = {0.0f, 5.0f};
    shards.verticalVelocity = {14.0f, 48.0f};
    shards.gravity = 170.0f;
    shards.drag = 1.6f;
    shards.spawnRadius = radius * 0.18f;
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
    debris.particleShape = MagicFxParticleShape::DirtClod;
    debris.spawnShape = MagicFxSpawnShape::Circle;
    debris.startColor = {172, 118, 70, 220};
    debris.endColor = {78, 58, 42, 0};
    debris.alphaScale = {0.48f, 1.0f};
    debris.speed = {8.0f, 48.0f};
    debris.lifetime = {0.26f, 0.62f};
    debris.startSize = {2.8f, 10.8f};
    debris.endSize = {0.0f, 1.4f};
    debris.rotation = {0.0f, Pi * 2.0f};
    debris.angularVelocity = {-6.5f, 6.5f};
    debris.height = {0.0f, 5.0f};
    debris.verticalVelocity = {6.0f, 34.0f};
    debris.gravity = 174.0f;
    debris.drag = 2.2f;
    debris.spawnRadius = radius * 0.20f;
    debris.spreadRadians = Pi * 2.0f;
    debris.fadeOutFraction = 0.66f;
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
    dust.alphaScale = {0.38f, 0.86f};
    dust.speed = {3.0f, 20.0f};
    dust.lifetime = {0.34f, 0.72f};
    dust.startSize = {radius * 0.20f, radius * 0.48f};
    dust.endSize = {radius * 0.44f, radius * 0.88f};
    dust.height = {0.0f, 3.0f};
    dust.verticalVelocity = {3.0f, 14.0f};
    dust.gravity = 8.0f;
    dust.drag = 2.2f;
    dust.spawnRadius = radius * 0.28f;
    dust.spreadRadians = Pi * 1.2f;
    dust.fadeInFraction = 0.12f;
    dust.fadeOutFraction = 0.72f;
    dust.burstCount = 18;
    dust.depthSorted = true;
    emitBurst(dust);

    MagicFxEmitterConfig grit;
    grit.position = position;
    grit.direction = {1.0f, 0.0f};
    grit.particleShape = MagicFxParticleShape::DirtClod;
    grit.spawnShape = MagicFxSpawnShape::Circle;
    grit.startColor = {190, 136, 82, 190};
    grit.endColor = {82, 60, 42, 0};
    grit.alphaScale = {0.38f, 0.88f};
    grit.speed = {14.0f, 62.0f};
    grit.lifetime = {0.16f, 0.36f};
    grit.startSize = {0.9f, 3.6f};
    grit.endSize = {0.0f, 0.5f};
    grit.rotation = {0.0f, Pi * 2.0f};
    grit.angularVelocity = {-10.0f, 10.0f};
    grit.height = {0.0f, 2.0f};
    grit.verticalVelocity = {4.0f, 20.0f};
    grit.gravity = 160.0f;
    grit.drag = 2.8f;
    grit.spawnRadius = radius * 0.18f;
    grit.spreadRadians = Pi * 2.0f;
    grit.fadeOutFraction = 0.68f;
    grit.burstCount = 26;
    grit.depthSorted = true;
    emitBurst(grit);
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
    updateThunderImpactArcs(dt);
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

    for (const ThunderImpactArc& arc : thunderImpactArcs_) {
        if (!arc.active || arc.pointCount < 2) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            arc.origin.y,
            [&renderer, &arc]() {
                drawThunderImpactArc(renderer, arc);
            },
        });
    }

    for (const Particle& particle : particles_) {
        if (!particle.active || particle.foreground) {
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

void MagicFxSystem::appendForegroundRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer) const
{
    for (const Particle& particle : particles_) {
        if (!particle.active || !particle.foreground) {
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
    thunderImpactArcs_.clear();
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

    const int effectiveCount = lightweightMode_ ? std::max(1, (count + 1) / 2) : count;
    for (int i = 0; i < effectiveCount; ++i) {
        Particle particle;
        particle.active = true;
        particle.shape = config.particleShape;
        particle.position = config.position + sampleSpawnOffset(config);
        particle.velocity = sampleVelocity(config);
        particle.acceleration = config.acceleration;
        particle.startColor = config.startColor;
        particle.endColor = config.endColor;
        const float alphaScale = clamp(sample(config.alphaScale), 0.0f, 1.0f);
        particle.startColor.a = static_cast<unsigned char>(std::clamp(
            std::lround(static_cast<float>(particle.startColor.a) * alphaScale),
            0L,
            255L));
        particle.endColor.a = static_cast<unsigned char>(std::clamp(
            std::lround(static_cast<float>(particle.endColor.a) * alphaScale),
            0L,
            255L));
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
        particle.bounceRestitution = clamp(config.bounceRestitution, 0.0f, 1.0f);
        particle.groundFriction = std::max(0.0f, config.groundFriction);
        particle.bouncesRemaining = std::max(0, config.maxBounces);
        particle.depthSorted = config.depthSorted;
        particle.foreground = config.foreground;
        if (particle.shape == MagicFxParticleShape::ThunderArc) {
            Vec2 radial = particle.position - config.position;
            if (lengthSquared(radial) <= 0.0001f) {
                radial = fromAngle(particle.rotation);
            } else {
                radial = normalize(radial);
            }
            const float spinSign = random01() < 0.5f ? -1.0f : 1.0f;
            const float orbitalSpeed = std::max(4.0f, length(particle.velocity));
            particle.velocity = perpendicular(radial) * (spinSign * orbitalSpeed) + radial * sampleRange({-1.6f, 1.6f});
            particle.rotation = std::atan2(particle.velocity.y, particle.velocity.x);
            particle.angularVelocity = spinSign * sampleRange({1.2f, 2.2f});
        }
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
        const float startDelay = std::max(0.0f, emitter.config.startDelay);
        const float localAge = emitter.age - startDelay;
        const bool started = localAge >= 0.0f;
        const bool durationElapsed = started && emitter.config.duration > 0.0f && localAge >= emitter.config.duration;
        const bool shouldEmit = started && (emitter.config.loop || !durationElapsed);
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
        if (started && !emitter.config.loop && emitter.config.duration <= 0.0f && emitter.config.emissionRate <= 0.0f) {
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
        if (particle.shape == MagicFxParticleShape::WindBlade && lengthSquared(particle.velocity) > 0.0001f) {
            particle.velocity = rotateVec(particle.velocity, particle.angularVelocity * dt * 1.55f);
        } else if (particle.shape == MagicFxParticleShape::ThunderArc && lengthSquared(particle.velocity) > 0.0001f) {
            particle.velocity = rotateVec(particle.velocity, particle.angularVelocity * dt * 0.58f);
        }
        particle.position += particle.velocity * dt;
        particle.velocity = particle.velocity * std::max(0.0f, 1.0f - particle.drag * dt);
        particle.verticalVelocity -= particle.gravity * dt;
        particle.height += particle.verticalVelocity * dt;
        if (particle.height <= 0.0f) {
            particle.height = 0.0f;
            if (particle.verticalVelocity < 0.0f && particle.bouncesRemaining > 0 && particle.bounceRestitution > 0.0f) {
                particle.verticalVelocity = -particle.verticalVelocity * particle.bounceRestitution;
                --particle.bouncesRemaining;
                particle.velocity = particle.velocity * 0.62f;
                particle.angularVelocity *= -0.58f;
                if (particle.verticalVelocity < MagicFxBounceStopSpeed) {
                    particle.verticalVelocity = 0.0f;
                    particle.bouncesRemaining = 0;
                }
            } else {
                particle.verticalVelocity = 0.0f;
                particle.bouncesRemaining = 0;
            }
            if (particle.groundFriction > 0.0f) {
                particle.velocity = particle.velocity * std::max(0.0f, 1.0f - particle.groundFriction * dt);
            }
        }
        if ((particle.shape == MagicFxParticleShape::WindBlade || particle.shape == MagicFxParticleShape::ThunderArc) && lengthSquared(particle.velocity) > 0.0001f) {
            particle.rotation = std::atan2(particle.velocity.y, particle.velocity.x);
        } else {
            particle.rotation += particle.angularVelocity * dt;
        }
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

void MagicFxSystem::updateThunderImpactArcs(float dt)
{
    for (ThunderImpactArc& arc : thunderImpactArcs_) {
        if (!arc.active) {
            continue;
        }
        arc.age += dt;
        if (arc.age >= arc.startDelay + arc.lifetime) {
            arc.active = false;
        }
    }

    thunderImpactArcs_.erase(
        std::remove_if(thunderImpactArcs_.begin(), thunderImpactArcs_.end(), [](const ThunderImpactArc& arc) {
            return !arc.active;
        }),
        thunderImpactArcs_.end());
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
    const std::size_t maxParticles = lightweightMode_ ? LightweightMaxParticles : MaxParticles;
    if (particles_.size() >= maxParticles) {
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

void MagicFxSystem::addThunderImpactArc(ThunderImpactArc arc)
{
    if (thunderImpactArcs_.size() >= MaxThunderImpactArcs) {
        const auto inactive = std::find_if(thunderImpactArcs_.begin(), thunderImpactArcs_.end(), [](const ThunderImpactArc& existing) {
            return !existing.active;
        });
        if (inactive != thunderImpactArcs_.end()) {
            *inactive = arc;
            return;
        }
        thunderImpactArcs_.erase(thunderImpactArcs_.begin());
    }
    thunderImpactArcs_.push_back(arc);
}

std::span<const EffectPreviewEntry> magicFxPreviewEntries()
{
    static constexpr std::array<EffectPreviewEntry, 18> Entries{{
        {.id = "fire_aura", .label = "火オーラ", .group = "MagicFx / 常駐", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::Player, .playback = EffectPreviewPlayback::PersistentEmitter, .radius = 34.0f},
        {.id = "fireball_loop", .label = "火球ループ", .group = "MagicFx / 常駐", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::EnemySlime, .playback = EffectPreviewPlayback::PersistentEmitter, .direction = {1.0f, 0.0f}, .radius = 28.0f},
        {.id = "fire_ground_burn", .label = "火の地面燃焼", .group = "MagicFx / 単発", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::WallTile, .radius = 38.0f},
        {.id = "ice_aura", .label = "氷オーラ", .group = "MagicFx / 常駐", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::Player, .playback = EffectPreviewPlayback::PersistentEmitter, .radius = 34.0f},
        {.id = "ice_shard_loop", .label = "氷片ループ", .group = "MagicFx / 常駐", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::EnemySlime, .playback = EffectPreviewPlayback::PersistentEmitter, .direction = {1.0f, 0.0f}, .radius = 28.0f},
        {.id = "ice_shatter", .label = "氷砕け", .group = "MagicFx / 単発", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::EnemySlime, .radius = 34.0f},
        {.id = "thunder_aura", .label = "雷オーラ", .group = "MagicFx / 常駐", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::Player, .playback = EffectPreviewPlayback::PersistentEmitter, .radius = 36.0f},
        {.id = "thunder_strike", .label = "落雷", .group = "MagicFx / 単発", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::EnemySlime, .radius = 40.0f},
        {.id = "thunder_bolt", .label = "雷ボルト", .group = "MagicFx / 単発", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::EnemySlime, .radius = 40.0f},
        {.id = "thunder_spark_burst", .label = "雷スパーク", .group = "MagicFx / 単発", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::EnemySlime, .radius = 36.0f},
        {.id = "heal_pulse", .label = "回復パルス", .group = "MagicFx / 単発", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::Player, .radius = 32.0f},
        {.id = "wind_aura", .label = "風オーラ", .group = "MagicFx / 常駐", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::Player, .playback = EffectPreviewPlayback::PersistentEmitter, .radius = 38.0f},
        {.id = "wind_wave_loop", .label = "風波ループ", .group = "MagicFx / 常駐", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::EnemySlime, .playback = EffectPreviewPlayback::PersistentEmitter, .direction = {1.0f, 0.0f}, .radius = 34.0f},
        {.id = "wind_impact", .label = "風ヒット", .group = "MagicFx / 単発", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::EnemySlime, .direction = {1.0f, 0.0f}, .radius = 36.0f},
        {.id = "earth_aura", .label = "土オーラ", .group = "MagicFx / 常駐", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::Player, .playback = EffectPreviewPlayback::PersistentEmitter, .radius = 36.0f},
        {.id = "dirt_clod_loop", .label = "土塊ループ", .group = "MagicFx / 常駐", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::EnemySlime, .playback = EffectPreviewPlayback::PersistentEmitter, .direction = {1.0f, 0.0f}, .radius = 32.0f},
        {.id = "earth_spike_rise", .label = "土の棘", .group = "MagicFx / 単発", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::WallTile, .radius = 38.0f},
        {.id = "earth_debris_burst", .label = "土破片バースト", .group = "MagicFx / 単発", .source = EffectPreviewSource::MagicFx, .target = EffectPreviewTarget::WallTile, .radius = 38.0f},
    }};
    return {Entries.data(), Entries.size()};
}

MagicFxEmitterHandle startMagicFxPreview(
    MagicFxSystem& magicFx,
    const EffectPreviewEntry& entry,
    Vec2 position,
    Vec2 direction)
{
    const Vec2 dir = lengthSquared(direction) > 0.0001f ? normalize(direction) : Vec2{1.0f, 0.0f};
    const float radius = std::max(1.0f, entry.radius);
    if (entry.id == "fire_aura") {
        return magicFx.startFireAura(position, radius);
    }
    if (entry.id == "fireball_loop") {
        return magicFx.startFireballLoop(position, dir, radius);
    }
    if (entry.id == "ice_aura") {
        return magicFx.startIceAura(position, radius);
    }
    if (entry.id == "ice_shard_loop") {
        return magicFx.startIceShardLoop(position, dir, radius);
    }
    if (entry.id == "thunder_aura") {
        return magicFx.startThunderAura(position, radius);
    }
    if (entry.id == "wind_aura") {
        return magicFx.startWindAura(position, radius);
    }
    if (entry.id == "wind_wave_loop") {
        return magicFx.startWindWaveLoop(position, dir, radius);
    }
    if (entry.id == "earth_aura") {
        return magicFx.startEarthAura(position, radius);
    }
    if (entry.id == "dirt_clod_loop") {
        return magicFx.startDirtClodLoop(position, dir, radius);
    }
    return {};
}

void playMagicFxPreview(
    MagicFxSystem& magicFx,
    const EffectPreviewEntry& entry,
    Vec2 position,
    Vec2 direction)
{
    const Vec2 dir = lengthSquared(direction) > 0.0001f ? normalize(direction) : Vec2{1.0f, 0.0f};
    const float radius = std::max(1.0f, entry.radius);
    if (entry.id == "fire_ground_burn") {
        magicFx.playFireGroundBurn(position, radius, 0.9f);
    } else if (entry.id == "ice_shatter") {
        magicFx.playIceShatter(position, radius);
    } else if (entry.id == "thunder_strike") {
        magicFx.playThunderStrike(position + Vec2{0.0f, -150.0f}, position, true);
    } else if (entry.id == "thunder_bolt") {
        magicFx.playThunderBolt(position - dir * 130.0f + Vec2{0.0f, -24.0f}, position + dir * 24.0f);
    } else if (entry.id == "thunder_spark_burst") {
        magicFx.playThunderSparkBurst(position, radius);
    } else if (entry.id == "heal_pulse") {
        magicFx.playHealPulse(position, radius);
    } else if (entry.id == "wind_impact") {
        magicFx.playWindImpact(position, dir, radius);
    } else if (entry.id == "earth_spike_rise") {
        magicFx.playEarthSpikeRise(position, radius);
    } else if (entry.id == "earth_debris_burst") {
        magicFx.playEarthDebrisBurst(position, radius);
    }
}

}
