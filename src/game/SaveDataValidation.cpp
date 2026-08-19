#include "game/SaveDataValidation.hpp"

#include "game/ItemModel.hpp"
#include "game/StorageRules.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace majo::save_data {
namespace {

constexpr std::size_t MaxTokensPerLine = 64;
constexpr std::size_t MaxIdentifierBytes = 256;
constexpr std::size_t MaxStoryFlagRecords = 8'192;
constexpr std::size_t MaxKnowledgeRecords = 32'768;
constexpr std::size_t MaxCodexRecords = 65'536;
constexpr std::size_t MaxDungeonTileRecords = 131'072;
constexpr std::size_t MaxDungeonNodeRecords = 4'096;
constexpr std::size_t MaxDungeonEventRecords = 1'024;
constexpr std::size_t MaxDungeonEventNestedRecords = 8'192;
constexpr std::size_t MaxWarpPointRecords = 64;
constexpr std::size_t MaxHighValueBuyRecords = 256;
constexpr std::int64_t MaxSavedItemCount = 10'000'000;

struct Counters {
    std::size_t storyFlags = 0;
    std::size_t knowledge = 0;
    std::size_t codex = 0;
    std::size_t merchantStock = 0;
    std::size_t highValueBuyObjects = 0;
    std::size_t backpackStackRecords = 0;
    std::size_t warehouseStackRecords = 0;
    std::size_t backpackInstances = 0;
    std::size_t warehouseInstances = 0;
    std::size_t ringItems = 0;
    std::size_t ringPresetItems = 0;
    std::size_t warpPoints = 0;
    std::size_t dungeonTileRecords = 0;
    std::size_t dungeonNodeRecords = 0;
    std::size_t dungeonEvents = 0;
    std::size_t dungeonEventNestedRecords = 0;
    std::size_t worldDrops = 0;
};

bool isWhitespace(unsigned char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r';
}

bool isValidUtf8(std::string_view text)
{
    std::size_t index = 0;
    while (index < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        if (lead <= 0x7fU) {
            ++index;
            continue;
        }

        std::size_t continuationCount = 0;
        std::uint32_t codePoint = 0;
        if (lead >= 0xc2U && lead <= 0xdfU) {
            continuationCount = 1;
            codePoint = lead & 0x1fU;
        } else if (lead >= 0xe0U && lead <= 0xefU) {
            continuationCount = 2;
            codePoint = lead & 0x0fU;
        } else if (lead >= 0xf0U && lead <= 0xf4U) {
            continuationCount = 3;
            codePoint = lead & 0x07U;
        } else {
            return false;
        }
        if (index + continuationCount >= text.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset <= continuationCount; ++offset) {
            const unsigned char continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xc0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6U) | (continuation & 0x3fU);
        }
        if ((continuationCount == 2 && codePoint < 0x800U) ||
            (continuationCount == 3 && codePoint < 0x10000U) ||
            (codePoint >= 0xd800U && codePoint <= 0xdfffU) ||
            codePoint > 0x10ffffU) {
            return false;
        }
        index += continuationCount + 1;
    }
    return true;
}

bool parseInteger(std::string_view token, std::int64_t& outValue)
{
    if (token.empty()) {
        return false;
    }
    if (token.front() == '+') {
        token.remove_prefix(1);
        if (token.empty()) {
            return false;
        }
    }
    const char* begin = token.data();
    const char* end = begin + token.size();
    const auto result = std::from_chars(begin, end, outValue, 10);
    return result.ec == std::errc{} && result.ptr == end;
}

std::string lineError(std::size_t lineNumber, std::string_view key, std::string_view message)
{
    std::string result = "semantic validation failed at line " + std::to_string(lineNumber);
    if (!key.empty()) {
        result += " (";
        result.append(key.substr(0, MaxIdentifierBytes));
        result += ")";
    }
    result += ": ";
    result += message;
    return result;
}

bool tokenize(
    std::string_view line,
    const ValidationLimits& limits,
    std::size_t lineNumber,
    std::vector<std::string_view>& outTokens,
    std::string& outError)
{
    outTokens.clear();
    if (line.size() > limits.maxLineBytes) {
        outError = lineError(lineNumber, {}, "line exceeds the byte limit");
        return false;
    }
    for (const unsigned char ch : line) {
        if (ch == 0 || (ch < 0x20U && !isWhitespace(ch))) {
            outError = lineError(lineNumber, {}, "line contains a forbidden control character");
            return false;
        }
    }

    std::size_t index = 0;
    while (index < line.size()) {
        while (index < line.size() && isWhitespace(static_cast<unsigned char>(line[index]))) {
            ++index;
        }
        if (index >= line.size()) {
            break;
        }
        const std::size_t begin = index;
        while (index < line.size() && !isWhitespace(static_cast<unsigned char>(line[index]))) {
            ++index;
        }
        const std::string_view token = line.substr(begin, index - begin);
        if (token.size() > limits.maxTokenBytes) {
            outError = lineError(lineNumber, outTokens.empty() ? std::string_view{} : outTokens.front(), "token exceeds the byte limit");
            return false;
        }
        if (outTokens.size() >= MaxTokensPerLine) {
            outError = lineError(lineNumber, outTokens.empty() ? std::string_view{} : outTokens.front(), "line has too many fields");
            return false;
        }
        outTokens.push_back(token);
    }
    if (!outTokens.empty() && outTokens.front().size() > MaxIdentifierBytes) {
        outError = lineError(lineNumber, {}, "key exceeds the identifier byte limit");
        return false;
    }
    return true;
}

bool requireIdentifier(
    const std::vector<std::string_view>& tokens,
    std::size_t index,
    std::size_t lineNumber,
    std::string& outError)
{
    if (index >= tokens.size() || tokens[index].empty() || tokens[index].size() > MaxIdentifierBytes) {
        outError = lineError(lineNumber, tokens.empty() ? std::string_view{} : tokens.front(), "identifier is missing or too long");
        return false;
    }
    return true;
}

bool requireMinimumFields(
    const std::vector<std::string_view>& tokens,
    std::size_t minimumCount,
    std::size_t lineNumber,
    std::string& outError)
{
    if (tokens.size() < minimumCount) {
        outError = lineError(lineNumber, tokens.empty() ? std::string_view{} : tokens.front(), "required fields are missing");
        return false;
    }
    return true;
}

bool requireInteger(
    const std::vector<std::string_view>& tokens,
    std::size_t index,
    std::int64_t minimum,
    std::int64_t maximum,
    std::size_t lineNumber,
    std::int64_t& outValue,
    std::string& outError)
{
    if (index >= tokens.size() || !parseInteger(tokens[index], outValue)) {
        outError = lineError(lineNumber, tokens.empty() ? std::string_view{} : tokens.front(), "integer field is missing or malformed");
        return false;
    }
    if (outValue < minimum || outValue > maximum) {
        outError = lineError(lineNumber, tokens.front(), "integer field is outside its safety range");
        return false;
    }
    return true;
}

bool requireReal(
    const std::vector<std::string_view>& tokens,
    std::size_t index,
    double minimum,
    double maximum,
    std::size_t lineNumber,
    std::string& outError)
{
    if (index >= tokens.size()) {
        outError = lineError(lineNumber, tokens.empty() ? std::string_view{} : tokens.front(), "numeric field is missing");
        return false;
    }
    double value = 0.0;
    const char* begin = tokens[index].data();
    const char* end = begin + tokens[index].size();
    const auto result = std::from_chars(begin, end, value, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(value)) {
        outError = lineError(lineNumber, tokens.front(), "numeric field is malformed or non-finite");
        return false;
    }
    if (value < minimum || value > maximum) {
        outError = lineError(lineNumber, tokens.front(), "numeric field is outside its safety range");
        return false;
    }
    return true;
}

bool requireIntegerFields(
    const std::vector<std::string_view>& tokens,
    std::initializer_list<std::size_t> indexes,
    std::size_t lineNumber,
    std::string& outError)
{
    std::int64_t ignored = 0;
    for (const std::size_t index : indexes) {
        if (!requireInteger(
                tokens,
                index,
                std::numeric_limits<int>::lowest(),
                std::numeric_limits<int>::max(),
                lineNumber,
                ignored,
                outError)) {
            return false;
        }
    }
    return true;
}

bool requireRealFields(
    const std::vector<std::string_view>& tokens,
    std::initializer_list<std::size_t> indexes,
    std::size_t lineNumber,
    std::string& outError)
{
    for (const std::size_t index : indexes) {
        if (!requireReal(tokens, index, -1.0e15, 1.0e15, lineNumber, outError)) {
            return false;
        }
    }
    return true;
}

bool validateItemInstanceFields(
    const std::vector<std::string_view>& tokens,
    std::size_t lineNumber,
    std::string& outError)
{
    if (!requireMinimumFields(tokens, 13, lineNumber, outError) ||
        !requireIntegerFields(tokens, {3, 4, 5, 6, 7, 8, 11, 12}, lineNumber, outError) ||
        !requireRealFields(tokens, {9, 10}, lineNumber, outError)) {
        return false;
    }
    if (tokens.size() > 13 && tokens.size() < 16) {
        outError = lineError(lineNumber, tokens.front(), "optional enhancement fields are incomplete");
        return false;
    }
    return tokens.size() < 16 || requireIntegerFields(tokens, {13, 14, 15}, lineNumber, outError);
}

bool validateRingFields(
    const std::vector<std::string_view>& tokens,
    std::size_t lineNumber,
    std::string& outError)
{
    if (!requireMinimumFields(tokens, 12, lineNumber, outError) ||
        !requireIdentifier(tokens, 2, lineNumber, outError) ||
        !requireIdentifier(tokens, 4, lineNumber, outError) ||
        !requireIntegerFields(tokens, {1, 3, 5, 6, 7}, lineNumber, outError) ||
        !requireRealFields(tokens, {8, 9, 10, 11}, lineNumber, outError)) {
        return false;
    }
    for (const std::size_t index : {13U, 14U, 15U, 16U, 19U, 20U, 21U, 22U, 23U, 24U}) {
        if (index < tokens.size() && !requireIntegerFields(tokens, {index}, lineNumber, outError)) {
            return false;
        }
    }
    for (const std::size_t index : {17U, 18U}) {
        if (index < tokens.size() && !requireRealFields(tokens, {index}, lineNumber, outError)) {
            return false;
        }
    }
    return true;
}

bool validateRingPresetItemFields(
    const std::vector<std::string_view>& tokens,
    std::size_t lineNumber,
    std::string& outError)
{
    if (!requireMinimumFields(tokens, 17, lineNumber, outError) ||
        !requireIdentifier(tokens, 4, lineNumber, outError) ||
        !requireIdentifier(tokens, 5, lineNumber, outError) ||
        !requireIntegerFields(tokens, {1, 2, 3, 7, 8, 9, 10, 11, 12, 15, 16}, lineNumber, outError) ||
        !requireRealFields(tokens, {6, 13, 14}, lineNumber, outError)) {
        return false;
    }
    if (tokens.size() > 17 && tokens.size() < 20) {
        outError = lineError(lineNumber, tokens.front(), "optional enhancement fields are incomplete");
        return false;
    }
    return tokens.size() < 20 || requireIntegerFields(tokens, {17, 18, 19}, lineNumber, outError);
}

bool consume(std::size_t& value, std::size_t maximum, std::size_t lineNumber, std::string_view key, std::string& outError)
{
    if (value >= maximum) {
        outError = lineError(lineNumber, key, "record count exceeds its safety limit");
        return false;
    }
    ++value;
    return true;
}

std::size_t delimiterEntryCount(std::string_view token, char delimiter)
{
    if (token.empty() || token == "-") {
        return 0;
    }
    return 1 + static_cast<std::size_t>(std::count(token.begin(), token.end(), delimiter));
}

bool addStackCount(
    std::unordered_map<std::string, std::int64_t>& counts,
    std::string_view objectId,
    std::int64_t count,
    std::size_t lineNumber,
    std::string_view key,
    std::string& outError)
{
    std::int64_t& total = counts[std::string(objectId)];
    if (count > MaxSavedItemCount - total) {
        outError = lineError(lineNumber, key, "aggregate item count exceeds its safety limit");
        return false;
    }
    total += count;
    return true;
}

std::int64_t requiredStackSlots(const std::unordered_map<std::string, std::int64_t>& counts)
{
    std::int64_t result = 0;
    for (const auto& [objectId, count] : counts) {
        (void)objectId;
        result += storage_rules::requiredStackSlots(static_cast<int>(count), ObjectStackMaxCount);
    }
    return result;
}

bool registerLiveInstanceId(
    const std::vector<std::string_view>& tokens,
    std::size_t index,
    std::size_t lineNumber,
    std::unordered_set<std::string>& instanceIds,
    std::string& outError)
{
    if (!requireIdentifier(tokens, index, lineNumber, outError)) {
        return false;
    }
    if (tokens[index] == "-") {
        return true;
    }
    if (!instanceIds.insert(std::string(tokens[index])).second) {
        outError = lineError(lineNumber, tokens.front(), "live item instance ID is duplicated");
        return false;
    }
    return true;
}

bool validateKnownRecord(
    const std::vector<std::string_view>& tokens,
    const ValidationLimits& limits,
    std::size_t lineNumber,
    Counters& counters,
    int& warehouseCapacityLevel,
    std::unordered_map<std::string, std::int64_t>& backpackCounts,
    std::unordered_map<std::string, std::int64_t>& warehouseCounts,
    std::unordered_set<std::string>& liveInstanceIds,
    std::string& outError)
{
    if (tokens.empty()) {
        return true;
    }
    const std::string_view key = tokens.front();
    std::int64_t value = 0;
    if (key == "money") {
        if (!requireInteger(tokens, 1, 0, std::numeric_limits<int>::max(), lineNumber, value, outError)) {
            return false;
        }
    } else if (key == "play_time_seconds") {
        if (!requireReal(tokens, 1, 0.0, 1.0e15, lineNumber, outError)) {
            return false;
        }
    } else if (key == "durability_unit_scale") {
        if (!requireInteger(tokens, 1, 1, 1'000, lineNumber, value, outError)) {
            return false;
        }
    } else if (key == "player_level") {
        if (!requireInteger(tokens, 1, 1, 1'000'000, lineNumber, value, outError)) {
            return false;
        }
    } else if (key == "player_xp" || key == "player_xp_to_next" ||
        key == "astral_high_score" || key == "astral_echo_star_count" ||
        key == "merchant_stock_version") {
        if (!requireInteger(tokens, 1, 0, std::numeric_limits<int>::max(), lineNumber, value, outError)) {
            return false;
        }
    } else if (key == "merchant_upgrade_level") {
        if (!requireInteger(tokens, 1, 1, 7, lineNumber, value, outError)) {
            return false;
        }
    } else if (key == "warehouse_capacity_level") {
        if (!requireInteger(tokens, 1, 0, storage_rules::MaxWarehouseCapacityLevel, lineNumber, value, outError)) {
            return false;
        }
        warehouseCapacityLevel = static_cast<int>(value);
    } else if (key == "object" || key == "warehouse_object") {
        if (!requireIdentifier(tokens, 1, lineNumber, outError) ||
            !requireInteger(tokens, 2, 1, MaxSavedItemCount, lineNumber, value, outError)) {
            return false;
        }
        std::size_t& recordCount = key == "object"
            ? counters.backpackStackRecords
            : counters.warehouseStackRecords;
        const std::size_t recordLimit = key == "object"
            ? static_cast<std::size_t>(std::max(0, limits.maxBackpackSlots))
            : static_cast<std::size_t>(storage_rules::MaxWarehouseCapacity);
        if (!consume(recordCount, recordLimit, lineNumber, key, outError)) {
            return false;
        }
        auto& counts = key == "object" ? backpackCounts : warehouseCounts;
        if (!addStackCount(counts, tokens[1], value, lineNumber, key, outError)) {
            return false;
        }
    } else if (key == "object_instance") {
        if (!consume(counters.backpackInstances, static_cast<std::size_t>(std::max(0, limits.maxBackpackSlots)), lineNumber, key, outError) ||
            !validateItemInstanceFields(tokens, lineNumber, outError) ||
            !registerLiveInstanceId(tokens, 1, lineNumber, liveInstanceIds, outError) ||
            tokens[1] == "-" ||
            !requireIdentifier(tokens, 2, lineNumber, outError) ||
            tokens[2] == "-") {
            if (outError.empty()) {
                outError = lineError(lineNumber, key, "live item identifiers must not be '-'");
            }
            return false;
        }
    } else if (key == "warehouse_object_instance") {
        if (!consume(counters.warehouseInstances, storage_rules::MaxWarehouseCapacity, lineNumber, key, outError) ||
            !validateItemInstanceFields(tokens, lineNumber, outError) ||
            !registerLiveInstanceId(tokens, 1, lineNumber, liveInstanceIds, outError) ||
            tokens[1] == "-" ||
            !requireIdentifier(tokens, 2, lineNumber, outError) ||
            tokens[2] == "-") {
            if (outError.empty()) {
                outError = lineError(lineNumber, key, "live item identifiers must not be '-'");
            }
            return false;
        }
    } else if (key == "ring") {
        if (!consume(counters.ringItems, static_cast<std::size_t>(std::max(0, limits.maxRingItemRecords)), lineNumber, key, outError) ||
            !validateRingFields(tokens, lineNumber, outError)) {
            return false;
        }
        if (tokens.size() > 12 && !registerLiveInstanceId(tokens, 12, lineNumber, liveInstanceIds, outError)) {
            return false;
        }
    } else if (key == "ring_preset_item") {
        if (!consume(counters.ringPresetItems, static_cast<std::size_t>(std::max(0, limits.maxRingPresetItemRecords)), lineNumber, key, outError) ||
            !validateRingPresetItemFields(tokens, lineNumber, outError)) {
            return false;
        }
    } else if (key == "merchant_stock") {
        if (!consume(counters.merchantStock, static_cast<std::size_t>(std::max(0, limits.maxMerchantStockRecords)), lineNumber, key, outError) ||
            !requireIdentifier(tokens, 1, lineNumber, outError) ||
            !requireInteger(tokens, 2, 1, 100'000'000, lineNumber, value, outError)) {
            return false;
        }
        if (tokens.size() >= 4 && !requireInteger(tokens, 3, 0, MaxSavedItemCount, lineNumber, value, outError)) {
            return false;
        }
    } else if (key == "high_value_buy_object") {
        if (!consume(counters.highValueBuyObjects, MaxHighValueBuyRecords, lineNumber, key, outError) ||
            !requireIdentifier(tokens, 1, lineNumber, outError)) {
            return false;
        }
    } else if (key == "story_flag") {
        if (!consume(counters.storyFlags, MaxStoryFlagRecords, lineNumber, key, outError) ||
            !requireIdentifier(tokens, 1, lineNumber, outError)) {
            return false;
        }
    } else if (key == "main_obtained_object" || key == "main_captured_enemy") {
        if (!consume(counters.knowledge, MaxKnowledgeRecords, lineNumber, key, outError) ||
            !requireIdentifier(tokens, 1, lineNumber, outError)) {
            return false;
        }
    } else if (key == "codex_entry" || key == "codex_effect" ||
        key == "codex_sync_suppress_owned" || key == "codex_sync_suppress_ring") {
        if (!consume(counters.codex, MaxCodexRecords, lineNumber, key, outError) ||
            !requireIdentifier(tokens, 1, lineNumber, outError)) {
            return false;
        }
        if (key == "codex_entry") {
            if (!requireIdentifier(tokens, 2, lineNumber, outError) ||
                !requireInteger(tokens, 3, 0, 10, lineNumber, value, outError)) {
                return false;
            }
        } else if (key == "codex_effect") {
            if (!requireIdentifier(tokens, 2, lineNumber, outError)) {
                return false;
            }
        } else if (!requireInteger(tokens, 2, 0, MaxSavedItemCount, lineNumber, value, outError)) {
            return false;
        }
    } else if (key == "dungeon_warp_point") {
        if (!consume(counters.warpPoints, MaxWarpPointRecords, lineNumber, key, outError) ||
            !requireMinimumFields(tokens, 8, lineNumber, outError) ||
            !requireIdentifier(tokens, 1, lineNumber, outError) ||
            !requireIntegerFields(tokens, {2, 5, 6, 7}, lineNumber, outError) ||
            !requireRealFields(tokens, {3, 4}, lineNumber, outError)) {
            return false;
        }
    } else if (key == "dungeon_minimap_cell" || key == "dungeon_tile_edit") {
        if (!consume(counters.dungeonTileRecords, MaxDungeonTileRecords, lineNumber, key, outError)) {
            return false;
        }
    } else if (key == "dungeon_reward_node" || key == "dungeon_money_node" ||
        key == "dungeon_moon_fragment_node" || key == "dungeon_chest_node" ||
        key == "dungeon_crate_node" || key == "dungeon_enemy_node") {
        if (!consume(counters.dungeonNodeRecords, MaxDungeonNodeRecords, lineNumber, key, outError)) {
            return false;
        }
    } else if (key == "dungeon_event_instance") {
        if (!consume(counters.dungeonEvents, MaxDungeonEventRecords, lineNumber, key, outError)) {
            return false;
        }
        std::size_t nested = 0;
        if (tokens.size() > 14) {
            nested += delimiterEntryCount(tokens[14], ',');
        }
        if (tokens.size() > 15) {
            nested += delimiterEntryCount(tokens[15], '|');
        }
        if (nested > MaxDungeonEventNestedRecords - std::min(counters.dungeonEventNestedRecords, MaxDungeonEventNestedRecords)) {
            outError = lineError(lineNumber, key, "nested event record count exceeds its safety limit");
            return false;
        }
        counters.dungeonEventNestedRecords += nested;
    } else if (key == "dungeon_world_drop") {
        if (!consume(counters.worldDrops, static_cast<std::size_t>(std::max(0, limits.maxWorldDropRecords)), lineNumber, key, outError) ||
            !requireMinimumFields(tokens, 9, lineNumber, outError) ||
            !requireIdentifier(tokens, 1, lineNumber, outError) ||
            !requireIdentifier(tokens, 2, lineNumber, outError) ||
            !requireIdentifier(tokens, 3, lineNumber, outError) ||
            !requireInteger(tokens, 4, 1, MaxSavedItemCount, lineNumber, value, outError) ||
            !requireRealFields(tokens, {5, 6, 7, 8}, lineNumber, outError) ||
            (tokens.size() > 9 && !requireIntegerFields(tokens, {9}, lineNumber, outError))) {
            return false;
        }
    } else if (key == "material") {
        if (!requireIdentifier(tokens, 1, lineNumber, outError) ||
            !requireInteger(tokens, 2, 0, MaxSavedItemCount, lineNumber, value, outError)) {
            return false;
        }
    } else if (key == "pending_level_bonus_choices") {
        if (!requireInteger(tokens, 1, 0, 100, lineNumber, value, outError)) {
            return false;
        }
    }
    return true;
}

}

bool validatePayload(
    std::string_view payload,
    const ValidationLimits& limits,
    std::string& outError)
{
    if (!isValidUtf8(payload)) {
        outError = "semantic validation failed: payload is not valid UTF-8";
        return false;
    }
    if (limits.maxBackpackSlots <= 0 || limits.maxWorldDropRecords <= 0) {
        outError = "semantic validation configuration is invalid";
        return false;
    }

    Counters counters;
    int warehouseCapacityLevel = 0;
    std::unordered_map<std::string, std::int64_t> backpackCounts;
    std::unordered_map<std::string, std::int64_t> warehouseCounts;
    std::unordered_set<std::string> liveInstanceIds;
    std::vector<std::string_view> tokens;
    tokens.reserve(MaxTokensPerLine);

    bool sawRecord = false;
    std::size_t lineNumber = 0;
    std::size_t begin = 0;
    while (begin < payload.size()) {
        if (lineNumber >= limits.maxLines) {
            outError = "semantic validation failed: line count exceeds the safety limit";
            return false;
        }
        const std::size_t newline = payload.find('\n', begin);
        const std::size_t end = newline == std::string_view::npos ? payload.size() : newline;
        ++lineNumber;
        if (!tokenize(payload.substr(begin, end - begin), limits, lineNumber, tokens, outError) ||
            !validateKnownRecord(
                tokens,
                limits,
                lineNumber,
                counters,
                warehouseCapacityLevel,
                backpackCounts,
                warehouseCounts,
                liveInstanceIds,
                outError)) {
            return false;
        }
        sawRecord = sawRecord || !tokens.empty();
        if (newline == std::string_view::npos) {
            break;
        }
        begin = newline + 1;
    }

    if (!sawRecord) {
        outError = "semantic validation failed: payload contains no records";
        return false;
    }

    const std::int64_t backpackSlots =
        requiredStackSlots(backpackCounts) + static_cast<std::int64_t>(counters.backpackInstances);
    if (backpackSlots > limits.maxBackpackSlots) {
        outError = "semantic validation failed: backpack contents exceed the slot capacity";
        return false;
    }

    const int warehouseCapacity = storage_rules::warehouseCapacityForLevel(warehouseCapacityLevel);
    const std::int64_t warehouseSlots =
        requiredStackSlots(warehouseCounts) + static_cast<std::int64_t>(counters.warehouseInstances);
    if (warehouseSlots > warehouseCapacity) {
        outError = "semantic validation failed: warehouse contents exceed the saved slot capacity";
        return false;
    }

    outError.clear();
    return true;
}

}
