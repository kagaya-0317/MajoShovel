#pragma once

#include <filesystem>
#include <string>

namespace majo {

struct OpeningMetaData {
    bool openingEverWatched = false;
};

class OpeningMetaSave {
public:
    [[nodiscard]] std::filesystem::path path() const;
    [[nodiscard]] OpeningMetaData load(std::string* outMessage = nullptr) const;
    [[nodiscard]] bool save(const OpeningMetaData& data, std::string* outError = nullptr) const;
};

}
