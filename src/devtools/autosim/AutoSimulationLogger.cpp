#include "devtools/autosim/AutoSimulationLogger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace majo::autosim {

namespace {

std::string csvEscape(const std::string& value)
{
    bool needsQuotes = false;
    for (char ch : value) {
        if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        return value;
    }

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value) {
        if (ch == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

std::string boolText(bool value)
{
    return value ? "true" : "false";
}

} // namespace

AutoSimulationLogger::AutoSimulationLogger()
{
    const std::filesystem::path root = logRoot();
    csvPath_ = root / ("runs_" + timestampForFileName() + ".csv");
    summaryPath_ = root / "latest_summary.txt";
}

std::filesystem::path AutoSimulationLogger::logRoot()
{
    return std::filesystem::path(".local") / "auto_sim";
}

std::string AutoSimulationLogger::timestampForFileName()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local, "%Y%m%d_%H%M%S");
    return stream.str();
}

void AutoSimulationLogger::writeBom(std::ofstream& file)
{
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    file.write(reinterpret_cast<const char*>(bom), sizeof(bom));
}

void AutoSimulationLogger::ensureCsvHeader() const
{
    std::error_code error;
    std::filesystem::create_directories(csvPath_.parent_path(), error);
    if (std::filesystem::exists(csvPath_, error) && std::filesystem::file_size(csvPath_, error) > 0) {
        return;
    }

    std::ofstream file(csvPath_, std::ios::binary | std::ios::trunc);
    if (!file) {
        return;
    }
    writeBom(file);
    file << "run_index,stage_id,stage_name,seed,result,elapsed_seconds,player_level,hp,max_hp,"
        "dug_tiles,defeated_enemies,acquired_items,acquired_object_items,money,total_materials,"
        "warp_discovered,warp_total,boss_defeated,timeout,stuck_count\n";
}

void AutoSimulationLogger::appendCsvRecord(const AutoSimulationRunRecord& record) const
{
    ensureCsvHeader();
    std::ofstream file(csvPath_, std::ios::binary | std::ios::app);
    if (!file) {
        return;
    }

    file << record.runIndex << ','
        << csvEscape(record.stageId) << ','
        << csvEscape(record.stageName) << ','
        << record.seed << ','
        << autoSimulationResultName(record.result) << ','
        << std::fixed << std::setprecision(2) << record.elapsedSeconds << ','
        << record.playerLevel << ','
        << record.hp << ','
        << record.maxHp << ','
        << record.dugTiles << ','
        << record.defeatedEnemies << ','
        << record.acquiredItems << ','
        << record.acquiredObjectItems << ','
        << record.money << ','
        << record.totalMaterials << ','
        << record.discoveredWarpPoints << ','
        << record.totalWarpPoints << ','
        << boolText(record.bossDefeated) << ','
        << boolText(record.timeout) << ','
        << record.stuckCount << '\n';
}

void AutoSimulationLogger::recordRun(const AutoSimulationRunRecord& record)
{
    records_.push_back(record);
    appendCsvRecord(record);
    writeSummary();
}

void AutoSimulationLogger::writeSummary() const
{
    std::error_code error;
    std::filesystem::create_directories(summaryPath_.parent_path(), error);

    std::ofstream file(summaryPath_, std::ios::binary | std::ios::trunc);
    if (!file) {
        return;
    }
    writeBom(file);

    file << "Auto simulation summary\n";
    file << "csv=" << csvPath_.string() << "\n";
    file << "runs=" << records_.size() << "\n";
    if (records_.empty()) {
        return;
    }

    const AutoSimulationRunRecord& last = records_.back();
    file << "last_result=" << autoSimulationResultName(last.result) << "\n";
    file << "last_stage=" << last.stageId << "\n";
    file << "last_seed=" << last.seed << "\n";
    file << "last_elapsed_seconds=" << std::fixed << std::setprecision(2) << last.elapsedSeconds << "\n";
    file << "last_dug_tiles=" << last.dugTiles << "\n";
    file << "last_defeated_enemies=" << last.defeatedEnemies << "\n";
    file << "last_acquired_items=" << last.acquiredItems << "\n";
    file << "last_stuck_count=" << last.stuckCount << "\n";
}

} // namespace majo::autosim
