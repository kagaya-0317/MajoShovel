#include "game/SaveFileStorage.hpp"

#include "game/SaveDataValidation.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("MajoShovelSaveFileTests-" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void require(bool condition, std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void writeRawFile(const std::filesystem::path& path, std::string_view bytes)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(file), "failed to prepare a test file");
}

std::string readRawFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

majo::save_file::SaveFileDocument readValidSave(const std::filesystem::path& path)
{
    majo::save_file::SaveFileDocument document;
    std::string error;
    require(majo::save_file::readSaveFile(path, document, error), error);
    return document;
}

bool validatesSemantically(
    std::string_view payload,
    std::string* outError = nullptr,
    majo::save_data::ValidationLimits limits = {})
{
    std::string error;
    const bool valid = majo::save_data::validatePayload(payload, limits, error);
    if (outError != nullptr) {
        *outError = std::move(error);
    }
    return valid;
}

void testLegacyCompatibility(const std::filesystem::path& directory)
{
    const std::filesystem::path legacyPath = directory / "legacy.dat";
    writeRawFile(legacyPath, "MAJO_SHOVEL_SAVE_V1\nmoney 12\n");

    const auto legacy = readValidSave(legacyPath);
    require(legacy.format == majo::save_file::SaveFileFormat::LegacyV1, "V1 format was not recognized");
    require(legacy.payload == "money 12\n", "V1 payload changed while reading");

    writeRawFile(legacyPath, "MAJO_SHOVEL_SAVE_V1\n");
    majo::save_file::SaveFileDocument incomplete;
    std::string error;
    require(!majo::save_file::readSaveFile(legacyPath, incomplete, error), "incomplete V1 file was accepted");

    writeRawFile(legacyPath, "MAJO_SHOVEL_SAVE_V1\nmoney 12\n");
    require(majo::save_file::writeSaveFileAtomically(legacyPath, "money 13\n", error), error);
    require(readValidSave(legacyPath).format == majo::save_file::SaveFileFormat::ProtectedV2, "V1 was not upgraded to V2");
    require(readValidSave(legacyPath).payload == "money 13\n", "upgraded V2 payload is incorrect");
    require(
        readValidSave(majo::save_file::backupPathFor(legacyPath)).payload == "money 12\n",
        "V1 migration did not retain a recoverable backup");
}

void testProtectedRoundTripAndCorruptionDetection(const std::filesystem::path& directory)
{
    const std::filesystem::path savePath = directory / "protected.dat";
    std::string error;
    require(
        majo::save_file::writeSaveFileAtomically(savePath, "money 24\nplayer_level 3\n", error),
        error);

    const auto document = readValidSave(savePath);
    require(document.format == majo::save_file::SaveFileFormat::ProtectedV2, "new save was not written as V2");
    require(document.payload == "money 24\nplayer_level 3\n", "V2 payload changed during round trip");

    std::string raw = readRawFile(savePath);
    require(raw.starts_with("MAJO_SHOVEL_SAVE_V2\n"), "V2 header is missing");
    require(raw.find("MAJO_SHOVEL_SAVE_END_V2 ") != std::string::npos, "V2 footer is missing");

    const std::size_t moneyValue = raw.find("24");
    require(moneyValue != std::string::npos, "test payload was not found");
    raw[moneyValue] = '9';
    writeRawFile(savePath, raw);
    majo::save_file::SaveFileDocument corrupt;
    require(!majo::save_file::readSaveFile(savePath, corrupt, error), "checksum mismatch was accepted");

    raw.pop_back();
    writeRawFile(savePath, raw);
    require(!majo::save_file::readSaveFile(savePath, corrupt, error), "truncated V2 file was accepted");
}

void testBackupRotationAndRecovery(const std::filesystem::path& directory)
{
    const std::filesystem::path savePath = directory / "rotation.dat";
    const std::filesystem::path backupPath = majo::save_file::backupPathFor(savePath);
    std::string error;

    require(majo::save_file::writeSaveFileAtomically(savePath, "value first\n", error), error);
    require(!std::filesystem::exists(backupPath), "first save unexpectedly created a backup");
    require(majo::save_file::writeSaveFileAtomically(savePath, "value second\n", error), error);
    require(readValidSave(savePath).payload == "value second\n", "primary did not receive the second save");
    require(readValidSave(backupPath).payload == "value first\n", "backup did not preserve the previous save");

    std::string corruptPrimary = readRawFile(savePath);
    const std::size_t secondValue = corruptPrimary.find("second");
    require(secondValue != std::string::npos, "second payload was not found");
    corruptPrimary[secondValue] = 'X';
    writeRawFile(savePath, corruptPrimary);

    require(majo::save_file::restoreSaveFileFromBackup(savePath, error), error);
    require(readValidSave(savePath).payload == "value first\n", "backup recovery did not restore the primary");

    writeRawFile(savePath, corruptPrimary);
    require(majo::save_file::writeSaveFileAtomically(savePath, "value third\n", error), error);
    require(readValidSave(savePath).payload == "value third\n", "primary did not receive the third save");
    require(
        readValidSave(backupPath).payload == "value first\n",
        "a corrupt primary overwrote the last valid backup");
}

void testSemanticResourceLimits()
{
    std::string error;
    require(
        validatesSemantically("warehouse_object ore 3800\nwarehouse_capacity_level 4\n", &error),
        error);
    require(
        !validatesSemantically("warehouse_object ore 3801\nwarehouse_capacity_level 4\n", &error),
        "warehouse contents beyond 200 slots were accepted");
    require(
        validatesSemantically("warehouse_object ore 912\nwarehouse_capacity_level 0\n", &error),
        error);
    require(
        !validatesSemantically("warehouse_object ore 913\nwarehouse_capacity_level 0\n", &error),
        "warehouse contents beyond the saved level were accepted");
    require(validatesSemantically("object ore 570\n", &error), error);
    require(
        !validatesSemantically("object ore 571\n", &error),
        "backpack contents beyond 30 slots were accepted");
    require(
        !validatesSemantically("warehouse_capacity_level 4\nwarehouse_object ore 2147483647\n", &error),
        "an expansion-sized warehouse count was accepted");

    std::string excessiveStackRecords;
    for (int index = 0; index <= 30; ++index) {
        excessiveStackRecords += "object item_" + std::to_string(index) + " 1\n";
    }
    require(
        !validatesSemantically(excessiveStackRecords, &error),
        "too many backpack stack records were accepted");

    std::string excessiveMerchantRecords;
    for (int index = 0; index <= 64; ++index) {
        excessiveMerchantRecords += "merchant_stock item_" + std::to_string(index) + " 1 1\n";
    }
    require(
        !validatesSemantically(excessiveMerchantRecords, &error),
        "too many merchant records were accepted");
}

void testSemanticSyntaxAndIdentityChecks()
{
    std::string error;
    require(
        !validatesSemantically(
            "object_instance instance_1 item_a 1 1 0 0 0 0 1 1 0 0\n"
            "warehouse_object_instance instance_1 item_b 1 1 0 0 0 0 1 1 0 0\n",
            &error),
        "a duplicate live item instance ID was accepted");
    require(error.find("duplicated") != std::string::npos, "duplicate ID failed for the wrong reason");
    require(
        !validatesSemantically("play_time_seconds nan\n", &error),
        "a NaN token was accepted");

    std::string invalidUtf8 = "story_flag ";
    invalidUtf8.push_back(static_cast<char>(0xc0));
    invalidUtf8 += "\n";
    require(!validatesSemantically(invalidUtf8, &error), "invalid UTF-8 was accepted");

    majo::save_data::ValidationLimits tightLimits;
    tightLimits.maxLineBytes = 8;
    require(
        !validatesSemantically("money 123\n", &error, tightLimits),
        "a line beyond the configured byte limit was accepted");
}

void testSemanticInvalidPrimaryDoesNotRotateBackup(const std::filesystem::path& directory)
{
    const std::filesystem::path savePath = directory / "semantic-rotation.dat";
    const std::filesystem::path backupPath = majo::save_file::backupPathFor(savePath);
    const majo::save_file::ExistingSaveValidator validator =
        [](const majo::save_file::SaveFileDocument& document) {
            return validatesSemantically(document.payload);
        };
    std::string error;

    require(majo::save_file::writeSaveFileAtomically(savePath, "money 1\n", validator, error), error);
    require(majo::save_file::writeSaveFileAtomically(savePath, "money 2\n", validator, error), error);
    require(readValidSave(backupPath).payload == "money 1\n", "initial semantic backup is incorrect");

    require(
        majo::save_file::writeSaveFileAtomically(
            savePath,
            "warehouse_capacity_level 4\nwarehouse_object ore 3801\n",
            error),
        error);
    require(readValidSave(backupPath).payload == "money 2\n", "test setup did not retain the valid backup");

    require(majo::save_file::writeSaveFileAtomically(savePath, "money 3\n", validator, error), error);
    require(readValidSave(savePath).payload == "money 3\n", "new primary was not installed");
    require(
        readValidSave(backupPath).payload == "money 2\n",
        "a semantically invalid primary overwrote the last valid backup");
}

}

int main()
{
    try {
        TemporaryDirectory directory;
        testLegacyCompatibility(directory.path());
        testProtectedRoundTripAndCorruptionDetection(directory.path());
        testBackupRotationAndRecovery(directory.path());
        testSemanticResourceLimits();
        testSemanticSyntaxAndIdentityChecks();
        testSemanticInvalidPrimaryDoesNotRotateBackup(directory.path());
        std::cout << "Save file storage tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Save file storage tests failed: " << error.what() << '\n';
        return 1;
    }
}
