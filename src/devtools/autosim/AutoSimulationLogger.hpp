#pragma once

#include "devtools/autosim/AutoSimulationTypes.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace majo::autosim {

class AutoSimulationLogger {
public:
    AutoSimulationLogger();

    void recordRun(const AutoSimulationRunRecord& record);
    void writeSummary() const;
    std::filesystem::path writeCheckpointReport(
        const GameTestCheckpointMeasurementSnapshot& measurement,
        AutoSimulationResult result) const;
    const std::vector<AutoSimulationRunRecord>& records() const { return records_; }
    const std::filesystem::path& csvPath() const { return csvPath_; }

private:
    static std::filesystem::path logRoot();
    static std::string timestampForFileName();
    static void writeBom(std::ofstream& file);
    void ensureCsvHeader() const;
    void appendCsvRecord(const AutoSimulationRunRecord& record) const;

    std::filesystem::path csvPath_;
    std::filesystem::path summaryPath_;
    std::vector<AutoSimulationRunRecord> records_;
};

} // namespace majo::autosim
