#pragma once

#include "game/SpellRingSystem.hpp"

#include <algorithm>
#include <array>

namespace majo {

enum class RingLevelUpgradeKind {
    Radius,
    Speed,
    WeightLimit,
};

struct RingLevelUpgradePoints {
    int radius = 0;
    int speed = 0;
    int weightLimit = 0;

    bool operator==(const RingLevelUpgradePoints&) const = default;
};

struct RingLevelUpgradeSelection {
    int ringIndex = 0;
    RingLevelUpgradeKind kind = RingLevelUpgradeKind::Radius;

    bool operator==(const RingLevelUpgradeSelection&) const = default;
};

using RingLevelUpgradePointTable = std::array<RingLevelUpgradePoints, SpellRingCount>;

inline int& ringLevelUpgradePointRef(RingLevelUpgradePoints& points, RingLevelUpgradeKind kind)
{
    switch (kind) {
    case RingLevelUpgradeKind::Speed:
        return points.speed;
    case RingLevelUpgradeKind::WeightLimit:
        return points.weightLimit;
    case RingLevelUpgradeKind::Radius:
    default:
        return points.radius;
    }
}

inline int ringLevelUpgradePoint(const RingLevelUpgradePoints& points, RingLevelUpgradeKind kind)
{
    switch (kind) {
    case RingLevelUpgradeKind::Speed:
        return points.speed;
    case RingLevelUpgradeKind::WeightLimit:
        return points.weightLimit;
    case RingLevelUpgradeKind::Radius:
    default:
        return points.radius;
    }
}

inline RingLevelUpgradePoints clampedRingLevelUpgradePoints(RingLevelUpgradePoints points)
{
    points.radius = std::max(0, points.radius);
    points.speed = std::max(0, points.speed);
    points.weightLimit = std::max(0, points.weightLimit);
    return points;
}

inline int ringLevelUpgradePointTotal(const RingLevelUpgradePoints& points)
{
    return std::max(0, points.radius) +
        std::max(0, points.speed) +
        std::max(0, points.weightLimit);
}

inline int ringLevelUpgradePointTotal(const RingLevelUpgradePointTable& table)
{
    int total = 0;
    for (const RingLevelUpgradePoints& points : table) {
        total += ringLevelUpgradePointTotal(points);
    }
    return total;
}

inline const char* ringLevelUpgradeKindName(RingLevelUpgradeKind kind)
{
    switch (kind) {
    case RingLevelUpgradeKind::Speed:
        return "速度";
    case RingLevelUpgradeKind::WeightLimit:
        return "重量";
    case RingLevelUpgradeKind::Radius:
    default:
        return "半径";
    }
}

}
