#include "devtools/autosim/AutoSimulationMapClueModel.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace majo::autosim {

namespace {

constexpr float MaxClueDistance = 1900.0f;
constexpr float MaxClueScore = 5200.0f;

struct MapClueCandidate {
    Vec2 world{};
    std::string reason;
    float score = std::numeric_limits<float>::max();
};

std::string reasonForClue(const GameTestMapClueSnapshot& clue)
{
    switch (clue.kind) {
    case GameTestMapClueKind::WarpGlow:
        return "map_clue_warp_light";
    case GameTestMapClueKind::UnknownLight:
        break;
    }
    return "map_clue_light";
}

float kindBias(const GameTestMapClueSnapshot& clue)
{
    switch (clue.kind) {
    case GameTestMapClueKind::WarpGlow:
        return -180.0f;
    case GameTestMapClueKind::UnknownLight:
        break;
    }
    return 0.0f;
}

std::optional<MapClueCandidate> makeCandidate(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathfinder& pathfinder,
    const AutoSimulationPathField& pathField,
    const GameTestMapClueSnapshot& clue)
{
    if (!clue.visibleOnMinimap || clue.alreadyVisited) {
        return std::nullopt;
    }

    const float directDistance = length(clue.position - snapshot.player.position);
    if (directDistance > MaxClueDistance) {
        return std::nullopt;
    }

    const std::optional<AutoSimulationRoute> route = pathfinder.findRoute(pathField, clue.position);
    if (!route) {
        return std::nullopt;
    }

    const float confidence = std::clamp(clue.confidence, 0.0f, 1.0f);
    const float score =
        route->totalCost +
        route->hardTileCount * 260.0f +
        route->digTileCount * 42.0f +
        directDistance * 0.035f -
        confidence * 360.0f +
        kindBias(clue);
    if (score > MaxClueScore) {
        return std::nullopt;
    }

    return MapClueCandidate{
        .world = clue.position,
        .reason = reasonForClue(clue),
        .score = score,
    };
}

void keepBetter(std::optional<MapClueCandidate>& best, std::optional<MapClueCandidate> candidate)
{
    if (!candidate) {
        return;
    }
    if (!best || candidate->score < best->score) {
        best = std::move(candidate);
    }
}

} // namespace

std::optional<AutoSimulationMapClueTarget> AutoSimulationMapClueModel::chooseTarget(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathfinder& pathfinder,
    const AutoSimulationPathField& pathField) const
{
    if (!pathField.valid()) {
        return std::nullopt;
    }

    std::optional<MapClueCandidate> best;
    for (const GameTestMapClueSnapshot& clue : snapshot.dungeon.mapClues) {
        keepBetter(best, makeCandidate(snapshot, pathfinder, pathField, clue));
    }

    if (!best) {
        return std::nullopt;
    }

    return AutoSimulationMapClueTarget{
        .world = best->world,
        .reason = best->reason,
    };
}

} // namespace majo::autosim
