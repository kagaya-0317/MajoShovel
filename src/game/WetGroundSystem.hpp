#pragma once

#include "engine/Math.hpp"

#include <array>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

class GroundLineSystem;
class Renderer;
struct DepthRenderEntry;

struct WetGroundEmitter {
    std::string sourceKey;
    Vec2 position{};
    float radius = 16.0f;
    float strength = 1.0f;
};

class WetGroundSystem {
public:
    bool touchSource(
        std::string_view sourceKey,
        Vec2 position,
        float radius,
        float strength = 1.0f);
    bool spawn(Vec2 position, float radius, float strength = 1.0f);
    void update(float dt);
    int erasePendingGroundLines(GroundLineSystem& groundLines);
    void appendRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer) const;
    void clear();
    void setLightweightMode(bool enabled);

private:
    static constexpr int MarkPointCount = 16;

    struct Mark {
        int id = 0;
        Vec2 center{};
        float baseRadius = 16.0f;
        float yScale = 0.58f;
        float rotation = 0.0f;
        float lifetimeSeconds = 5.0f;
        float ageSeconds = 0.0f;
        unsigned char alpha = 58;
        bool erasePending = true;
        std::array<Vec2, MarkPointCount> baseOffsets{};
    };

    struct SourceState {
        std::string key;
        Vec2 lastEmitPosition{};
        bool hasLastEmitPosition = false;
        float cooldownSeconds = 0.0f;
        float idleSeconds = 0.0f;
    };

    [[nodiscard]] SourceState& stateForSource(std::string_view sourceKey);
    [[nodiscard]] std::size_t maxMarks() const;
    void trimOldestMarks();
    void trimIdleSources();

    std::vector<Mark> marks_;
    std::vector<SourceState> sourceStates_;
    std::mt19937 rng_{std::random_device{}()};
    int nextMarkId_ = 1;
    bool lightweightMode_ = false;
};

}
