#include "game/OpeningMetaSave.hpp"

#include <cstdlib>
#include <fstream>
#include <string_view>
#include <system_error>

namespace majo {
namespace {

std::string trimAscii(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() && static_cast<unsigned char>(value[begin]) <= 0x20U) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && static_cast<unsigned char>(value[end - 1]) <= 0x20U) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

std::string lowerAscii(std::string_view value)
{
    std::string lowered;
    lowered.reserve(value.size());
    for (char ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            lowered.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else {
            lowered.push_back(ch);
        }
    }
    return lowered;
}

void stripUtf8Bom(std::string& line)
{
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xefU &&
        static_cast<unsigned char>(line[1]) == 0xbbU &&
        static_cast<unsigned char>(line[2]) == 0xbfU) {
        line.erase(0, 3);
    }
}

bool parseBool(std::string_view value)
{
    const std::string normalized = lowerAscii(trimAscii(value));
    return normalized == "true" || normalized == "1" || normalized == "yes";
}

}

std::filesystem::path OpeningMetaSave::path() const
{
#if defined(_MSC_VER)
    char* localAppData = nullptr;
    std::size_t localAppDataLength = 0;
    if (_dupenv_s(&localAppData, &localAppDataLength, "LOCALAPPDATA") == 0 &&
        localAppData != nullptr &&
        localAppDataLength > 1) {
        std::filesystem::path result = std::filesystem::path(localAppData) / "MajoShovel" / "meta.dat";
        std::free(localAppData);
        return result;
    }
    if (localAppData != nullptr) {
        std::free(localAppData);
    }
#else
    if (const char* localAppData = std::getenv("LOCALAPPDATA")) {
        if (localAppData[0] != '\0') {
            return std::filesystem::path(localAppData) / "MajoShovel" / "meta.dat";
        }
    }
#endif
    return std::filesystem::path("meta.dat");
}

OpeningMetaData OpeningMetaSave::load(std::string* outMessage) const
{
    OpeningMetaData data;
    const std::filesystem::path metaPath = path();
    std::ifstream file(metaPath);
    if (!file) {
        if (outMessage != nullptr) {
            *outMessage = "opening meta not found: " + metaPath.generic_string();
        }
        return data;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (firstLine) {
            stripUtf8Bom(line);
            firstLine = false;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = trimAscii(std::string_view(line).substr(0, equals));
        const std::string value = trimAscii(std::string_view(line).substr(equals + 1));
        if (key == "openingEverWatched") {
            data.openingEverWatched = parseBool(value);
        }
    }
    if (outMessage != nullptr) {
        *outMessage = "opening meta loaded: " + metaPath.generic_string();
    }
    return data;
}

bool OpeningMetaSave::save(const OpeningMetaData& data, std::string* outError) const
{
    const std::filesystem::path metaPath = path();
    const std::filesystem::path parent = metaPath.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            if (outError != nullptr) {
                *outError = "failed to create opening meta directory: " + parent.generic_string() + " (" + ec.message() + ")";
            }
            return false;
        }
    }

    std::ofstream file(metaPath, std::ios::binary | std::ios::trunc);
    if (!file) {
        if (outError != nullptr) {
            *outError = "failed to open opening meta for write: " + metaPath.generic_string();
        }
        return false;
    }
    file << "openingEverWatched=" << (data.openingEverWatched ? "true" : "false") << "\n";
    if (!file) {
        if (outError != nullptr) {
            *outError = "failed to write opening meta: " + metaPath.generic_string();
        }
        return false;
    }
    return true;
}

}
