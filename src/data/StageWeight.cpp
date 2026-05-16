#include "data/StageWeight.hpp"

#include <algorithm>
#include <array>

namespace majo {

namespace {

constexpr std::array<StageWeightSpec, 4> StageWeightSpecs = {{
    {"stage_01_stardust", "ST", 3},
    {"stage_02_junk_magic", "JK", 3},
    {"stage_03_star_core", "SC", 3},
    {"stage_04_astral_mine", "AS", 9},
}};

const StageWeightSpec* findStageWeightSpec(std::string_view stageId)
{
    const auto it = std::find_if(StageWeightSpecs.begin(), StageWeightSpecs.end(), [stageId](const StageWeightSpec& spec) {
        return spec.stageId == stageId;
    });
    return it != StageWeightSpecs.end() ? &*it : nullptr;
}

}

std::span<const StageWeightSpec> stageWeightSpecs()
{
    return StageWeightSpecs;
}

std::string_view stageWeightColumnPrefix(std::string_view stageId)
{
    const StageWeightSpec* spec = findStageWeightSpec(stageId);
    return spec != nullptr ? spec->columnPrefix : std::string_view{};
}

int clampStageWeightDepthRank(std::string_view stageId, int depthRank)
{
    const StageWeightSpec* spec = findStageWeightSpec(stageId);
    if (spec == nullptr) {
        return 0;
    }
    return std::clamp(depthRank, 1, spec->maxDepthRank);
}

std::string makeStageWeightColumnName(std::string_view stageId, int depthRank, std::string_view weightKindCode)
{
    const StageWeightSpec* spec = findStageWeightSpec(stageId);
    if (spec == nullptr || weightKindCode.empty()) {
        return {};
    }

    const int clampedDepth = std::clamp(depthRank, 1, spec->maxDepthRank);
    return std::string(spec->columnPrefix) + "_" + std::string(weightKindCode) + std::to_string(clampedDepth);
}

std::vector<std::string> expectedStageWeightColumnNames(std::span<const std::string_view> weightKindCodes)
{
    std::size_t columnCount = 0;
    for (const StageWeightSpec& spec : StageWeightSpecs) {
        columnCount += static_cast<std::size_t>(spec.maxDepthRank) * weightKindCodes.size();
    }

    std::vector<std::string> columns;
    columns.reserve(columnCount);
    for (const StageWeightSpec& spec : StageWeightSpecs) {
        for (int depth = 1; depth <= spec.maxDepthRank; ++depth) {
            for (std::string_view kindCode : weightKindCodes) {
                if (!kindCode.empty()) {
                    columns.push_back(std::string(spec.columnPrefix) + "_" + std::string(kindCode) + std::to_string(depth));
                }
            }
        }
    }
    return columns;
}

std::optional<std::size_t> selectWeightedIndex(std::span<const double> weights, std::mt19937& rng)
{
    if (weights.empty()) {
        return std::nullopt;
    }
    const bool hasPositiveWeight = std::any_of(weights.begin(), weights.end(), [](double weight) {
        return weight > 0.0;
    });
    if (!hasPositiveWeight) {
        return std::nullopt;
    }

    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    const int selected = distribution(rng);
    if (selected < 0 || static_cast<std::size_t>(selected) >= weights.size()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(selected);
}

}
