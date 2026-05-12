#pragma once

#include "data/RuntimeBalance.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

struct GoogleSheetSourceConfig {
    bool enabled = false;
    std::string spreadsheetId;
    std::string gid = "0";
    std::string objectsSheet = "Objects";
    std::string stagesSheet = "Stages";
    std::string enemiesSheet = "Enemies";
    std::string behaviorSheet = "#敵挙動コード";
    bool behaviorSheetExplicit = false;
};

using GoogleSheetRow = std::vector<std::string>;

struct GoogleSheetTable {
    std::vector<GoogleSheetRow> rows;
};

bool loadGoogleSheetSourceConfig(const std::filesystem::path& path, GoogleSheetSourceConfig& outConfig, std::string& outError);
bool parseGoogleSheetCsv(std::string_view csv, GoogleSheetTable& outTable, std::string& outError);
bool loadGoogleSheetCsv(const GoogleSheetSourceConfig& config, std::string& outCsv, std::string& outError);
bool loadGoogleSheetCsvForSheet(const GoogleSheetSourceConfig& config, std::string_view sheetName, std::string& outCsv, std::string& outError);
bool loadGoogleSheetTable(const GoogleSheetSourceConfig& config, GoogleSheetTable& outTable, std::string& outError);
bool loadGoogleSheetTableForSheet(const GoogleSheetSourceConfig& config, std::string_view sheetName, GoogleSheetTable& outTable, std::string& outError);
bool loadRuntimeBalanceFromGoogleSheet(const GoogleSheetSourceConfig& config, RuntimeBalance& outBalance, std::string& outError);
std::string googleSheetCsvUrl(const GoogleSheetSourceConfig& config);
std::string googleSheetCsvUrlForSheet(const GoogleSheetSourceConfig& config, std::string_view sheetName);

}
