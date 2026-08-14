#pragma once

#include "engine/Math.hpp"
#include "engine/RendererTypes.hpp"

#include <cstdint>
#include <vector>

namespace majo {

class Renderer;

struct MoneyGainLandingEvent {
    Vec2 position{};
    float pitchScale = 1.0f;
};

struct MoneyGainArrivalEvent {
    Vec2 position{};
    int amount = 0;
    float pitchScale = 1.0f;
};

class MoneyGainFxSystem {
public:
    void spawn(int amount, Vec2 origin);
    void update(float dt, Vec2 targetPosition);
    void renderForeground(Renderer& renderer) const;
    void clear();

    [[nodiscard]] int displayedMoney(int authoritativeMoney) const;
    [[nodiscard]] int pendingDisplayAmount() const;
    [[nodiscard]] float hudPulseStrength() const;
    [[nodiscard]] bool active() const { return !coins_.empty() || !sparkles_.empty() || !arrivalPulses_.empty(); }
    std::vector<MoneyGainLandingEvent> consumeLandingEvents();
    std::vector<MoneyGainArrivalEvent> consumeArrivalEvents();

private:
    struct Coin {
        Vec2 origin{};
        Vec2 groundPosition{};
        Vec2 position{};
        Vec2 previousPosition{};
        float ageSeconds = 0.0f;
        float startDelaySeconds = 0.0f;
        float jumpDurationSeconds = 0.26f;
        float jumpHeight = 30.0f;
        float bounceDurationSeconds = 0.14f;
        float bounceHeight = 8.0f;
        float settleDurationSeconds = 0.1f;
        float flightDurationSeconds = 0.38f;
        float curveSideOffset = 0.0f;
        float curveLift = 24.0f;
        float initialRotationDegrees = 0.0f;
        float airborneRotationDegrees = 0.0f;
        float absorbRotationDegrees = 0.0f;
        float initialDepthRotationDegrees = 0.0f;
        float airborneDepthRotationDegrees = 0.0f;
        float initialDepthAxisDegrees = 90.0f;
        float absorbDepthRotationDegrees = 0.0f;
        float absorbDepthAxisDegrees = 90.0f;
        float sparkleTimerSeconds = 0.0f;
        float trailIntensity = 1.0f;
        float landingPitchScale = 1.0f;
        std::uint32_t sparkleRandomState = 1;
        int displayAmount = 0;
        int sequenceIndex = 0;
        int sequenceCount = 1;
        bool shouldEmitLandingEvent = false;
    };

    struct Sparkle {
        Vec2 position{};
        Color color{};
        float ageSeconds = 0.0f;
        float durationSeconds = 0.18f;
        float size = 4.0f;
        float rotationRadians = 0.0f;
    };

    struct ArrivalPulse {
        Vec2 position{};
        float ageSeconds = 0.0f;
        float durationSeconds = 0.24f;
    };

    std::vector<Coin> coins_;
    std::vector<Sparkle> sparkles_;
    std::vector<ArrivalPulse> arrivalPulses_;
    std::vector<MoneyGainLandingEvent> landingEvents_;
    std::vector<MoneyGainArrivalEvent> arrivalEvents_;
    std::uint32_t spawnSerial_ = 0;
    float hudPulseSeconds_ = 0.0f;
};

}
