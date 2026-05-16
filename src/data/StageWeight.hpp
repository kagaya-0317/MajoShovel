#pragma once

#include <cstddef>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

struct StageWeightSpec {
    std::string_view stageId;
    std::string_view columnPrefix;
    int maxDepthRank = 0;
};

std::span<const StageWeightSpec> stageWeightSpecs();
std::string_view stageWeightColumnPrefix(std::string_view stageId);
int clampStageWeightDepthRank(std::string_view stageId, int depthRank);
std::string makeStageWeightColumnName(std::string_view stageId, int depthRank, std::string_view weightKindCode);
std::vector<std::string> expectedStageWeightColumnNames(std::span<const std::string_view> weightKindCodes);
std::optional<std::size_t> selectWeightedIndex(std::span<const double> weights, std::mt19937& rng);

}
