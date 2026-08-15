#include "devtools/autosim/AutoSimulationLogger.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <map>
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

std::filesystem::path checkpointReportRoot()
{
#ifdef _WIN32
    char* localAppData = nullptr;
    std::size_t localAppDataLength = 0;
    if (_dupenv_s(&localAppData, &localAppDataLength, "LOCALAPPDATA") == 0 &&
        localAppData != nullptr && localAppDataLength > 1) {
        const std::filesystem::path root =
            std::filesystem::path(localAppData) / "MajoShovel" / "auto-sim-reports";
        std::free(localAppData);
        return root;
    }
    std::free(localAppData);
#endif
    return std::filesystem::path(".local") / "auto_sim" / "reports";
}

std::string reportResultText(AutoSimulationResult result, bool completed)
{
    if (completed) {
        return "完了（ボス直前ワープ到達）";
    }
    switch (result) {
    case AutoSimulationResult::GameOver: return "中断（ゲームオーバー）";
    case AutoSimulationResult::Timeout: return "中断（タイムアウト）";
    case AutoSimulationResult::Stopped: return "中断（手動停止）";
    default: return "中断（未完了）";
    }
}

GameTestCheckpointMeasurementTotals subtractTotals(
    const GameTestCheckpointMeasurementTotals& value,
    const GameTestCheckpointMeasurementTotals& baseline)
{
    GameTestCheckpointMeasurementTotals result;
    result.elapsedSeconds = std::max(0.0f, value.elapsedSeconds - baseline.elapsedSeconds);
    result.defeatedEnemies = std::max(0, value.defeatedEnemies - baseline.defeatedEnemies);
    result.combatSeconds = std::max(0.0f, value.combatSeconds - baseline.combatSeconds);
    result.defeatedEnemyHitCount = std::max(
        0,
        value.defeatedEnemyHitCount - baseline.defeatedEnemyHitCount);
    result.playerDamagedCount = std::max(0, value.playerDamagedCount - baseline.playerDamagedCount);
    result.playerDamageTotal = std::max(0, value.playerDamageTotal - baseline.playerDamageTotal);
    result.recoveryUseCount = std::max(0, value.recoveryUseCount - baseline.recoveryUseCount);
    result.acquiredItemCount = std::max(0, value.acquiredItemCount - baseline.acquiredItemCount);
    result.brokenItemCount = std::max(0, value.brokenItemCount - baseline.brokenItemCount);

    std::map<std::string, GameTestEnemyMeasurementSnapshot> baselineById;
    for (const GameTestEnemyMeasurementSnapshot& enemy : baseline.enemies) {
        baselineById[enemy.enemyId] = enemy;
    }
    for (const GameTestEnemyMeasurementSnapshot& enemy : value.enemies) {
        const auto beforeIt = baselineById.find(enemy.enemyId);
        const GameTestEnemyMeasurementSnapshot before =
            beforeIt == baselineById.end() ? GameTestEnemyMeasurementSnapshot{} : beforeIt->second;
        GameTestEnemyMeasurementSnapshot delta{
            .enemyId = enemy.enemyId,
            .enemyName = enemy.enemyName,
            .defeatedCount = std::max(0, enemy.defeatedCount - before.defeatedCount),
            .combatSeconds = std::max(0.0f, enemy.combatSeconds - before.combatSeconds),
            .defeatedEnemyHitCount = std::max(
                0,
                enemy.defeatedEnemyHitCount - before.defeatedEnemyHitCount),
            .playerDamagedCount = std::max(0, enemy.playerDamagedCount - before.playerDamagedCount),
            .playerDamageTotal = std::max(0, enemy.playerDamageTotal - before.playerDamageTotal),
        };
        if (delta.defeatedCount > 0 || delta.defeatedEnemyHitCount > 0 ||
            delta.playerDamagedCount > 0 || delta.playerDamageTotal > 0) {
            result.enemies.push_back(std::move(delta));
        }
    }
    return result;
}

double perMinute(int value, float seconds)
{
    return seconds > 0.001f ? static_cast<double>(value) * 60.0 / static_cast<double>(seconds) : 0.0;
}

void writeMetricBlock(std::ofstream& file, const GameTestCheckpointMeasurementTotals& totals)
{
    const auto averageOrZero = [&](double total) {
        return totals.defeatedEnemies > 0 ? total / static_cast<double>(totals.defeatedEnemies) : 0.0;
    };
    file << std::fixed << std::setprecision(2);
    file << "攻略時間: " << totals.elapsedSeconds << " 秒\r\n";
    file << "敵との平均戦闘時間: " << averageOrZero(totals.combatSeconds) << " 秒\r\n";
    file << "敵撃破までの平均ヒット数: " <<
        averageOrZero(totals.defeatedEnemyHitCount) << " 回\r\n";
    file << "敵からの平均被ダメージ回数: " << averageOrZero(totals.playerDamagedCount) << " 回\r\n";
    file << "敵撃破数: " << totals.defeatedEnemies << " / " << perMinute(totals.defeatedEnemies, totals.elapsedSeconds) << "（1分平均）\r\n";
    file << "被ダメージ: " << totals.playerDamageTotal << " / " << perMinute(totals.playerDamageTotal, totals.elapsedSeconds) << "（1分平均）\r\n";
    file << "回復使用回数: " << totals.recoveryUseCount << " / " << perMinute(totals.recoveryUseCount, totals.elapsedSeconds) << "（1分平均）\r\n";
    file << "アイテム入手数: " << totals.acquiredItemCount << " / " << perMinute(totals.acquiredItemCount, totals.elapsedSeconds) << "（1分平均）\r\n";
    file << "アイテム破損回数: " << totals.brokenItemCount << " / " << perMinute(totals.brokenItemCount, totals.elapsedSeconds) << "（1分平均）\r\n";
}

void writeEnemyBreakdown(std::ofstream& file, const GameTestCheckpointMeasurementTotals& totals)
{
    file << "\r\n敵の内訳:\r\n";
    if (totals.enemies.empty()) {
        file << "  （対象なし）\r\n";
        return;
    }
    std::vector<GameTestEnemyMeasurementSnapshot> enemies = totals.enemies;
    std::sort(enemies.begin(), enemies.end(), [](const auto& left, const auto& right) {
        if (left.combatSeconds != right.combatSeconds) {
            return left.combatSeconds > right.combatSeconds;
        }
        return left.enemyId < right.enemyId;
    });
    for (const GameTestEnemyMeasurementSnapshot& enemy : enemies) {
        const double defeated = static_cast<double>(enemy.defeatedCount);
        const std::string name = enemy.enemyName.empty() ? enemy.enemyId : enemy.enemyName;
        file << "  " << name << " [" << enemy.enemyId << "]\r\n";
        file << "    撃破数: " << enemy.defeatedCount << "\r\n";
        if (enemy.defeatedCount > 0) {
            file << std::fixed << std::setprecision(2);
            file << "    平均戦闘時間: " << enemy.combatSeconds / defeated << " 秒\r\n";
            file << "    撃破までの平均ヒット数: " <<
                static_cast<double>(enemy.defeatedEnemyHitCount) / defeated << " 回\r\n";
            file << "    平均被ダメージ回数: " << static_cast<double>(enemy.playerDamagedCount) / defeated << " 回\r\n";
        } else {
            file << "    平均戦闘時間 / 撃破までの平均ヒット数 / 平均被ダメージ回数: ―（撃破サンプルなし）\r\n";
        }
        file << "    被ダメージ合計: " << enemy.playerDamageTotal << "\r\n";
    }
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

std::filesystem::path AutoSimulationLogger::writeCheckpointReport(
    const GameTestCheckpointMeasurementSnapshot& measurement,
    AutoSimulationResult result) const
{
    const std::filesystem::path root = checkpointReportRoot();
    std::error_code error;
    std::filesystem::create_directories(root, error);

    std::string stageToken = measurement.stageId.empty() ? "unknown_stage" : measurement.stageId;
    std::replace_if(stageToken.begin(), stageToken.end(), [](unsigned char ch) {
        return !(std::isalnum(ch) || ch == '_' || ch == '-');
    }, '_');
    const std::string timestamp = timestampForFileName();
    std::filesystem::path path = root / ("checkpoint_" + stageToken + "_" + timestamp + ".txt");
    for (int suffix = 2; std::filesystem::exists(path, error); ++suffix) {
        path = root / ("checkpoint_" + stageToken + "_" + timestamp + "_" + std::to_string(suffix) + ".txt");
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return {};
    }
    writeBom(file);

    file << "オートシミュ ボス直前ワープ計測レポート\r\n";
    file << "========================================\r\n";
    file << "結果: " << reportResultText(result, measurement.completed) << "\r\n";
    file << "ステージ: " << measurement.stageName << " [" << measurement.stageId << "]\r\n";
    file << "シード: " << measurement.seed << "\r\n";
    file << "到達ワープ: " << measurement.checkpoints.size() << "/" << measurement.totalWarpPoints << "\r\n";
    file << "\r\n開始時リング:\r\n";
    if (measurement.ringLoadout.empty()) {
        file << "  （装備なし）\r\n";
    } else {
        for (const std::string& item : measurement.ringLoadout) {
            file << "  " << item << "\r\n";
        }
    }
    file << "開始時リュック:\r\n";
    if (measurement.backpackLoadout.empty()) {
        file << "  （空）\r\n";
    } else {
        for (const std::string& item : measurement.backpackLoadout) {
            file << "  " << item << "\r\n";
        }
    }

    file << "\r\n全体\r\n";
    file << "----------------------------------------\r\n";
    writeMetricBlock(file, measurement.totals);
    const GameTestEnemyMeasurementSnapshot* slowest = nullptr;
    const GameTestEnemyMeasurementSnapshot* mostDamaging = nullptr;
    for (const GameTestEnemyMeasurementSnapshot& enemy : measurement.totals.enemies) {
        if (enemy.defeatedCount > 0 &&
            (slowest == nullptr ||
                enemy.combatSeconds / static_cast<float>(enemy.defeatedCount) >
                    slowest->combatSeconds / static_cast<float>(slowest->defeatedCount))) {
            slowest = &enemy;
        }
        if (mostDamaging == nullptr || enemy.playerDamageTotal > mostDamaging->playerDamageTotal) {
            mostDamaging = &enemy;
        }
    }
    file << "\r\n注目:\r\n";
    if (slowest != nullptr) {
        file << "  最も平均戦闘時間が長い敵: " <<
            (slowest->enemyName.empty() ? slowest->enemyId : slowest->enemyName) << " (" <<
            std::fixed << std::setprecision(2) <<
            slowest->combatSeconds / static_cast<float>(slowest->defeatedCount) << " 秒)\r\n";
    } else {
        file << "  最も平均戦闘時間が長い敵: ―\r\n";
    }
    if (mostDamaging != nullptr && mostDamaging->playerDamageTotal > 0) {
        file << "  最もダメージを受けた敵: " <<
            (mostDamaging->enemyName.empty() ? mostDamaging->enemyId : mostDamaging->enemyName) <<
            " (" << mostDamaging->playerDamageTotal << ")\r\n";
    } else {
        file << "  最もダメージを受けた敵: ―\r\n";
    }
    writeEnemyBreakdown(file, measurement.totals);

    GameTestCheckpointMeasurementTotals previous;
    for (std::size_t i = 0; i < measurement.checkpoints.size(); ++i) {
        const GameTestCheckpointMeasurementPoint& checkpoint = measurement.checkpoints[i];
        const GameTestCheckpointMeasurementTotals interval = subtractTotals(checkpoint.totals, previous);
        file << "\r\n区間 " << (i + 1) << ": " <<
            (i == 0 ? "入口" : "ワープ" + std::to_string(i)) << " → ワープ" <<
            (checkpoint.warpIndex + 1) << "\r\n";
        file << "----------------------------------------\r\n";
        file << std::fixed << std::setprecision(2)
             << "ワープ到達時の累計時間: " << checkpoint.totals.elapsedSeconds << " 秒\r\n";
        writeMetricBlock(file, interval);
        writeEnemyBreakdown(file, interval);
        previous = checkpoint.totals;
    }

    if (!measurement.completed) {
        const GameTestCheckpointMeasurementTotals unfinished = subtractTotals(measurement.totals, previous);
        if (unfinished.elapsedSeconds > 0.0f || unfinished.defeatedEnemyHitCount > 0 ||
            unfinished.playerDamagedCount > 0 || unfinished.acquiredItemCount > 0) {
            file << "\r\n未到達区間（最後の到達ワープ以降）\r\n";
            file << "----------------------------------------\r\n";
            writeMetricBlock(file, unfinished);
            writeEnemyBreakdown(file, unfinished);
        }
    }
    file.flush();
    return file ? path : std::filesystem::path{};
}

} // namespace majo::autosim
