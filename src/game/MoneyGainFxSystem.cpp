#include "game/MoneyGainFxSystem.hpp"

#include "engine/Renderer.hpp"
#include "game/WorldIconRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>

namespace majo {

namespace {

constexpr std::size_t MaxActiveCoins = 96;
constexpr int MaxCoinsPerGrant = 20;
constexpr float Pi = 3.14159265358979323846f;
constexpr float MoneyCoinShadowSize = 20.0f;
constexpr float ArrivalPulseDurationSeconds = 0.24f;
constexpr float HudPulseDurationSeconds = 0.22f;
constexpr Vec2 MoneyCoinTargetOffset{0.0f, -16.0f};

float smooth01(float value)
{
    value = clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float easeOutCubic(float value)
{
    value = clamp(value, 0.0f, 1.0f);
    const float inverse = 1.0f - value;
    return 1.0f - inverse * inverse * inverse;
}

int desiredCoinCount(int amount)
{
    return std::clamp(amount, 1, MaxCoinsPerGrant);
}

std::uint32_t mixSeed(std::uint32_t seed, std::uint32_t value)
{
    seed ^= value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    return seed;
}

std::uint32_t spawnSeed(std::uint32_t serial, int amount, Vec2 origin)
{
    std::uint32_t seed = mixSeed(0x4d414a4fU, serial);
    seed = mixSeed(seed, static_cast<std::uint32_t>(std::max(0, amount)));
    seed = mixSeed(seed, static_cast<std::uint32_t>(std::lround(origin.x * 8.0f)));
    return mixSeed(seed, static_cast<std::uint32_t>(std::lround(origin.y * 8.0f)));
}

float randomRange(std::mt19937& rng, float minValue, float maxValue)
{
    return std::uniform_real_distribution<float>(minValue, maxValue)(rng);
}

float arcHeight(float progress, float height)
{
    return std::sin(clamp(progress, 0.0f, 1.0f) * Pi) * height;
}

template <typename CoinType>
float absorbStartSeconds(const CoinType& coin)
{
    return coin.jumpDurationSeconds +
        coin.bounceDurationSeconds +
        coin.settleDurationSeconds;
}

Vec2 bezierPoint(Vec2 start, Vec2 control, Vec2 end, float t)
{
    const float inverse = 1.0f - t;
    return start * (inverse * inverse) +
        control * (2.0f * inverse * t) +
        end * (t * t);
}

Color withScaledAlpha(Color color, float alphaScale)
{
    color.a = static_cast<unsigned char>(std::clamp(
        std::lround(static_cast<float>(color.a) * clamp(alphaScale, 0.0f, 1.0f)),
        0L,
        255L));
    return color;
}

}

void MoneyGainFxSystem::spawn(int amount, Vec2 origin)
{
    amount = std::max(0, amount);
    if (amount <= 0) {
        return;
    }

    std::mt19937 rng{spawnSeed(++spawnSerial_, amount, origin)};
    spawnEvents_.push_back(MoneyGainSpawnEvent{
        .position = origin,
        .pitchScale = randomRange(rng, 0.96f, 1.04f),
    });

    const int desiredCount = desiredCoinCount(amount);
    const std::size_t availableSlots = MaxActiveCoins > coins_.size()
        ? MaxActiveCoins - coins_.size()
        : 0;
    if (availableSlots == 0) {
        coins_.back().displayAmount = static_cast<int>(std::min<std::int64_t>(
            std::numeric_limits<int>::max(),
            static_cast<std::int64_t>(coins_.back().displayAmount) + amount));
        return;
    }

    const int coinCount = std::min(desiredCount, static_cast<int>(availableSlots));
    int remainingAmount = amount;
    for (int index = 0; index < coinCount; ++index) {
        const int remainingCoins = coinCount - index;
        const int displayAmount = std::max(1, remainingAmount / remainingCoins);
        remainingAmount -= displayAmount;

        Coin coin;
        const float spreadProgress =
            std::sqrt((static_cast<float>(index) + 0.5f) / static_cast<float>(coinCount));
        const float spreadAngle =
            static_cast<float>(index) * 2.39996323f +
            randomRange(rng, -0.20f, 0.20f);
        const float spreadRadius = lerp(5.0f, 31.0f, spreadProgress);
        coin.origin = origin + Vec2{randomRange(rng, -2.0f, 2.0f), randomRange(rng, -5.0f, -2.0f)};
        coin.groundPosition = origin + Vec2{
            std::cos(spreadAngle) * spreadRadius,
            std::sin(spreadAngle) * spreadRadius * 0.42f,
        };
        coin.position = coin.origin;
        coin.previousPosition = coin.origin;
        coin.startDelaySeconds = static_cast<float>(index) * 0.014f + randomRange(rng, 0.0f, 0.008f);
        coin.jumpDurationSeconds = randomRange(rng, 0.24f, 0.30f);
        coin.jumpHeight = randomRange(rng, 27.0f, 39.0f);
        coin.bounceDurationSeconds = randomRange(rng, 0.11f, 0.15f);
        coin.bounceHeight = randomRange(rng, 5.5f, 9.5f);
        coin.settleDurationSeconds = randomRange(rng, 0.08f, 0.13f);
        coin.flightDurationSeconds = randomRange(rng, 0.34f, 0.45f);
        coin.curveSideOffset = randomRange(rng, -28.0f, 28.0f);
        coin.curveLift = randomRange(rng, 20.0f, 36.0f);
        coin.initialRotationDegrees = randomRange(rng, 0.0f, 360.0f);
        coin.angularVelocityDegrees = randomRange(rng, 480.0f, 820.0f) *
            (std::bernoulli_distribution(0.5)(rng) ? 1.0f : -1.0f);
        coin.displayAmount = displayAmount;
        coin.sequenceIndex = index;
        coin.sequenceCount = coinCount;
        coins_.push_back(coin);
    }

    if (remainingAmount > 0) {
        coins_.back().displayAmount = static_cast<int>(std::min<std::int64_t>(
            std::numeric_limits<int>::max(),
            static_cast<std::int64_t>(coins_.back().displayAmount) + remainingAmount));
    }
}

void MoneyGainFxSystem::update(float dt, Vec2 targetPosition)
{
    const float safeDt = std::max(0.0f, dt);
    hudPulseSeconds_ = std::max(0.0f, hudPulseSeconds_ - safeDt);
    const Vec2 target = targetPosition + MoneyCoinTargetOffset;

    for (Coin& coin : coins_) {
        coin.previousPosition = coin.position;
        coin.ageSeconds += safeDt;
        const float activeSeconds = coin.ageSeconds - coin.startDelaySeconds;
        if (activeSeconds <= 0.0f) {
            coin.position = coin.origin;
            continue;
        }

        if (activeSeconds < coin.jumpDurationSeconds) {
            const float progress = activeSeconds / std::max(0.05f, coin.jumpDurationSeconds);
            coin.position =
                lerp(coin.origin, coin.groundPosition, easeOutCubic(progress)) +
                Vec2{0.0f, -arcHeight(progress, coin.jumpHeight)};
            continue;
        }

        const float bounceSeconds = activeSeconds - coin.jumpDurationSeconds;
        if (bounceSeconds < coin.bounceDurationSeconds) {
            const float progress = bounceSeconds / std::max(0.05f, coin.bounceDurationSeconds);
            coin.position = coin.groundPosition + Vec2{0.0f, -arcHeight(progress, coin.bounceHeight)};
            continue;
        }

        const float groundSeconds = absorbStartSeconds(coin);
        if (activeSeconds < groundSeconds) {
            coin.position = coin.groundPosition;
            continue;
        }

        const float rawProgress =
            (activeSeconds - groundSeconds) /
            std::max(0.05f, coin.flightDurationSeconds);
        const float progress = clamp(rawProgress, 0.0f, 1.0f);
        const Vec2 delta = target - coin.groundPosition;
        const Vec2 side = lengthSquared(delta) > 0.01f
            ? normalize(Vec2{-delta.y, delta.x})
            : Vec2{1.0f, 0.0f};
        const Vec2 control =
            coin.groundPosition +
            delta * 0.42f +
            side * coin.curveSideOffset +
            Vec2{0.0f, -coin.curveLift};
        coin.position = bezierPoint(coin.groundPosition, control, target, smooth01(progress));
    }

    auto removeBegin = std::remove_if(coins_.begin(), coins_.end(), [&](const Coin& coin) {
        const float activeSeconds = coin.ageSeconds - coin.startDelaySeconds;
        if (activeSeconds < absorbStartSeconds(coin) + coin.flightDurationSeconds) {
            return false;
        }

        arrivalPulses_.push_back(ArrivalPulse{
            .position = target,
            .ageSeconds = 0.0f,
            .durationSeconds = ArrivalPulseDurationSeconds,
        });
        arrivalEvents_.push_back(MoneyGainArrivalEvent{
            .position = target,
            .amount = coin.displayAmount,
            .pitchScale = lerp(
                0.94f,
                1.18f,
                static_cast<float>(coin.sequenceIndex) /
                    static_cast<float>(std::max(1, coin.sequenceCount - 1))),
        });
        hudPulseSeconds_ = HudPulseDurationSeconds;
        return true;
    });
    coins_.erase(removeBegin, coins_.end());

    for (ArrivalPulse& pulse : arrivalPulses_) {
        pulse.ageSeconds += safeDt;
    }
    arrivalPulses_.erase(
        std::remove_if(arrivalPulses_.begin(), arrivalPulses_.end(), [](const ArrivalPulse& pulse) {
            return pulse.ageSeconds >= pulse.durationSeconds;
        }),
        arrivalPulses_.end());
}

void MoneyGainFxSystem::renderForeground(Renderer& renderer) const
{
    for (const ArrivalPulse& pulse : arrivalPulses_) {
        const float progress = clamp(
            pulse.ageSeconds / std::max(0.01f, pulse.durationSeconds),
            0.0f,
            1.0f);
        const float alpha = 1.0f - smooth01(progress);
        renderer.fillSoftCircle(
            pulse.position,
            lerp(14.0f, 32.0f, easeOutCubic(progress)),
            withScaledAlpha({255, 210, 70, 110}, alpha));
        renderer.drawSoftRing(
            pulse.position,
            lerp(8.0f, 25.0f, easeOutCubic(progress)),
            lerp(4.5f, 1.5f, progress),
            withScaledAlpha({255, 242, 166, 210}, alpha));
    }

    for (const Coin& coin : coins_) {
        const float activeSeconds = coin.ageSeconds - coin.startDelaySeconds;
        if (activeSeconds < 0.0f) {
            continue;
        }

        const float groundSeconds = absorbStartSeconds(coin);
        const float flightProgress = clamp(
            (activeSeconds - groundSeconds) /
                std::max(0.05f, coin.flightDurationSeconds),
            0.0f,
            1.0f);
        const float heightAboveGround = std::max(0.0f, coin.groundPosition.y - coin.position.y);
        const float shadowLift = clamp(
            heightAboveGround / std::max(1.0f, coin.jumpHeight),
            0.0f,
            1.0f);
        const float shadowAlpha = (1.0f - flightProgress) * lerp(1.0f, 0.42f, shadowLift);
        const float shadowScale = lerp(1.0f, 0.58f, shadowLift);
        renderer.fillEllipse(
            coin.groundPosition + Vec2{0.0f, MoneyCoinShadowSize * 0.34f},
            {
                MoneyCoinShadowSize * 0.43f * shadowScale,
                MoneyCoinShadowSize * 0.15f * shadowScale,
            },
            withScaledAlpha({14, 10, 20, 78}, shadowAlpha));

        const float trailAlpha = smooth01(flightProgress) * (1.0f - flightProgress);
        if (trailAlpha > 0.001f) {
            renderer.drawSoftLine(
                coin.previousPosition,
                coin.position,
                lerp(3.2f, 1.2f, flightProgress),
                withScaledAlpha({255, 206, 72, 170}, trailAlpha * 2.2f));
        }

        const float popProgress = clamp(
            activeSeconds / std::max(0.04f, coin.jumpDurationSeconds),
            0.0f,
            1.0f);
        const float landingImpact = smooth01(clamp(
            1.0f - std::abs(activeSeconds - coin.jumpDurationSeconds) / 0.055f,
            0.0f,
            1.0f));
        const float arrivalShrink = lerp(1.0f, 0.62f, smooth01(flightProgress));
        const float popScale = 0.78f + 0.22f * easeOutCubic(popProgress);
        const float visualScale = popScale * arrivalShrink;

        WorldIconDrawOptions options;
        options.filter = TextureFilter::Linear;
        options.allowUpscale = true;
        options.outlineEnabled = false;
        options.rotationDegrees =
            coin.initialRotationDegrees +
            coin.angularVelocityDegrees * activeSeconds;
        options.scaleMultiplier = visualScale;
        options.tint = withScaledAlpha({255, 255, 255, 255}, 1.0f - smooth01((flightProgress - 0.90f) / 0.10f));
        (void)drawWorldIcon(
            renderer,
            WorldIconId::MoneyCoin,
            coin.position,
            {
                WorldIconScaleReferenceSize * (1.0f + landingImpact * 0.10f),
                WorldIconScaleReferenceSize * (1.0f - landingImpact * 0.12f),
            },
            options);
    }
}

void MoneyGainFxSystem::clear()
{
    coins_.clear();
    arrivalPulses_.clear();
    spawnEvents_.clear();
    arrivalEvents_.clear();
    spawnSerial_ = 0;
    hudPulseSeconds_ = 0.0f;
}

int MoneyGainFxSystem::displayedMoney(int authoritativeMoney) const
{
    return std::max(0, authoritativeMoney - pendingDisplayAmount());
}

int MoneyGainFxSystem::pendingDisplayAmount() const
{
    std::int64_t total = 0;
    for (const Coin& coin : coins_) {
        total += std::max(0, coin.displayAmount);
    }
    return static_cast<int>(std::min<std::int64_t>(std::numeric_limits<int>::max(), total));
}

float MoneyGainFxSystem::hudPulseStrength() const
{
    return smooth01(hudPulseSeconds_ / HudPulseDurationSeconds);
}

std::vector<MoneyGainSpawnEvent> MoneyGainFxSystem::consumeSpawnEvents()
{
    std::vector<MoneyGainSpawnEvent> events;
    events.swap(spawnEvents_);
    return events;
}

std::vector<MoneyGainArrivalEvent> MoneyGainFxSystem::consumeArrivalEvents()
{
    std::vector<MoneyGainArrivalEvent> events;
    events.swap(arrivalEvents_);
    return events;
}

}
