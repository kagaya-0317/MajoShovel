#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace majo::save_file {

enum class SaveFileFormat {
    LegacyV1,
    ProtectedV2,
};

struct SaveFileDocument {
    SaveFileFormat format = SaveFileFormat::LegacyV1;
    std::string payload;
};

using ExistingSaveValidator = std::function<bool(const SaveFileDocument&)>;

[[nodiscard]] std::filesystem::path backupPathFor(const std::filesystem::path& primaryPath);

bool readSaveFile(
    const std::filesystem::path& path,
    SaveFileDocument& outDocument,
    std::string& outError);

bool writeSaveFileAtomically(
    const std::filesystem::path& path,
    std::string payload,
    std::string& outError);

bool writeSaveFileAtomically(
    const std::filesystem::path& path,
    std::string payload,
    const ExistingSaveValidator& existingSaveValidator,
    std::string& outError);

bool restoreSaveFileFromBackup(
    const std::filesystem::path& primaryPath,
    std::string& outError);

}
