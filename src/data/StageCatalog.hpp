#pragma once

#include "data/GoogleSheetSource.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

struct StageDefinition {
    std::string id;
    std::string name;
    std::string type;
    int displayOrder = 999;
    std::string implementationState;
    std::string generationProfile;
    std::string terrainProfile;
    int goalDistanceTiles = 320;
    double detourRate = 0.30;
    double branchDensity = 0.25;
    double cavernWidthMultiplier = 1.00;
    double terrainHardnessMultiplier = 1.00;
    int warpPointCount = 0;
    int specialRoomCount = 0;

    bool operator==(const StageDefinition&) const = default;
};

struct StageCatalog {
    std::vector<StageDefinition> stages;
    std::unordered_map<std::string, std::size_t> stagesById;
    std::vector<std::string> validationWarnings;

    [[nodiscard]] const StageDefinition* getStageById(std::string_view id) const;
    [[nodiscard]] std::vector<StageDefinition> getStagesSortedByDisplayOrder() const;
    bool loadFromCsv(std::string_view csv, std::string& outError);
    bool loadFromTable(const GoogleSheetTable& table, std::string& outError);
    void loadDefaultStages();

    bool operator==(const StageCatalog&) const = default;
};

bool loadStageCatalogFromGoogleSheet(const GoogleSheetSourceConfig& config, StageCatalog& outCatalog, std::string& outError);

}
