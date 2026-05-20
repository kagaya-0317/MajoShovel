#pragma once

#include "engine/Math.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

class Renderer;
struct DepthRenderEntry;

struct MagicCircleCandidate {
    Vec2 center{};
    float radius = 0.0f;
    float power = 1.0f;
    std::vector<int> segmentIds;
};

class GroundLineSystem {
public:
    bool drawWhiteLine(
        std::string_view sourceKey,
        Vec2 position,
        float width,
        float strength,
        float lifetimeSeconds);
    [[nodiscard]] std::optional<MagicCircleCandidate> findCompletedCircleNear(
        Vec2 position,
        float searchRadius,
        std::string_view sourceKey) const;
    void consumeSegments(std::span<const int> segmentIds);
    void clearSource(std::string_view sourceKey);
    void update(float dt);
    void appendRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer) const;
    void clear();

private:
    struct Segment {
        int id = 0;
        int strokeId = 0;
        std::string sourceKey;
        Vec2 a{};
        Vec2 b{};
        float width = 3.0f;
        float strength = 1.0f;
        float lifetimeSeconds = 0.0f;
        float ageSeconds = 0.0f;
    };

    struct StrokeState {
        Vec2 lastPosition{};
        bool hasLastPosition = false;
        int strokeId = 1;
    };

    [[nodiscard]] StrokeState& stateForSource(std::string_view sourceKey);
    void trimOldestSegments();

    std::vector<Segment> segments_;
    std::vector<StrokeState> strokeStates_;
    std::vector<std::string> strokeStateKeys_;
    int nextSegmentId_ = 1;
};

}
