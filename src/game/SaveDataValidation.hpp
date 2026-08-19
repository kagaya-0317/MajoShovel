#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace majo::save_data {

struct ValidationLimits {
    std::size_t maxLineBytes = 64 * 1024;
    std::size_t maxTokenBytes = 16 * 1024;
    std::size_t maxLines = 200'000;
    int maxBackpackSlots = 30;
    int maxMerchantStockRecords = 64;
    int maxRingItemRecords = 192;
    int maxRingPresetItemRecords = 576;
    int maxWorldDropRecords = 2'048;
};

bool validatePayload(
    std::string_view payload,
    const ValidationLimits& limits,
    std::string& outError);

}
