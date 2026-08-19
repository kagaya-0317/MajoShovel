#include "game/SaveFileStorage.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace majo::save_file {
namespace {

constexpr std::string_view LegacyHeader = "MAJO_SHOVEL_SAVE_V1";
constexpr std::string_view ProtectedHeader = "MAJO_SHOVEL_SAVE_V2";
constexpr std::string_view ProtectedFooterPrefix = "MAJO_SHOVEL_SAVE_END_V2 ";
constexpr std::uintmax_t MaxSaveFileBytes = 16ULL * 1024ULL * 1024ULL;

std::atomic<std::uint64_t> TemporaryFileSequence{0};

std::string pathForError(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::string systemErrorMessage(unsigned long error)
{
    return std::error_code(static_cast<int>(error), std::system_category()).message();
}

std::uint32_t crc32(std::string_view bytes)
{
    std::uint32_t crc = 0xffffffffU;
    for (const unsigned char byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return crc ^ 0xffffffffU;
}

std::string fixedWidthHex(std::uint32_t value)
{
    constexpr char Digits[] = "0123456789abcdef";
    std::string result(8, '0');
    for (int index = 7; index >= 0; --index) {
        result[static_cast<std::size_t>(index)] = Digits[value & 0x0fU];
        value >>= 4U;
    }
    return result;
}

bool parseUnsignedDecimal(std::string_view text, std::uint64_t& outValue)
{
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, outValue, 10);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseChecksum(std::string_view text, std::uint32_t& outValue)
{
    if (text.size() != 8) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, outValue, 16);
    return result.ec == std::errc{} && result.ptr == end;
}

bool readRawFile(const std::filesystem::path& path, std::string& outBytes, std::string& outError)
{
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        outError = "failed to inspect save file " + pathForError(path) + ": " + error.message();
        return false;
    }
    if (size > MaxSaveFileBytes) {
        outError = "save file exceeds the 16 MiB safety limit: " + pathForError(path);
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outError = "failed to open save file: " + pathForError(path);
        return false;
    }

    outBytes.assign(static_cast<std::size_t>(size), '\0');
    if (size > 0) {
        file.read(outBytes.data(), static_cast<std::streamsize>(size));
        if (file.gcount() != static_cast<std::streamsize>(size)) {
            outError = "save file changed or ended while being read: " + pathForError(path);
            outBytes.clear();
            return false;
        }
    }
    if (file.peek() != std::char_traits<char>::eof()) {
        outError = "save file changed while being read: " + pathForError(path);
        outBytes.clear();
        return false;
    }
    return true;
}

std::string_view lineWithoutCarriageReturn(std::string_view line)
{
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    return line;
}

bool parseSaveFileContent(std::string_view bytes, SaveFileDocument& outDocument, std::string& outError)
{
    const std::size_t headerEnd = bytes.find('\n');
    if (headerEnd == std::string_view::npos) {
        outError = "save file is incomplete (missing header terminator)";
        return false;
    }

    const std::string_view header = lineWithoutCarriageReturn(bytes.substr(0, headerEnd));
    const std::size_t payloadStart = headerEnd + 1;
    if (header == LegacyHeader) {
        const std::string_view payload = bytes.substr(payloadStart);
        if (payload.empty() || payload.back() != '\n') {
            outError = "legacy V1 save file is incomplete";
            return false;
        }
        outDocument.format = SaveFileFormat::LegacyV1;
        outDocument.payload.assign(payload);
        return true;
    }

    if (header != ProtectedHeader) {
        outError = "save file has an unsupported header";
        return false;
    }
    if (bytes.empty() || bytes.back() != '\n' || bytes.size() <= payloadStart) {
        outError = "protected V2 save file is incomplete";
        return false;
    }

    const std::size_t footerSeparator = bytes.rfind('\n', bytes.size() - 2);
    if (footerSeparator == std::string_view::npos || footerSeparator < headerEnd) {
        outError = "protected V2 save file has no integrity footer";
        return false;
    }
    const std::size_t footerStart = footerSeparator + 1;
    std::string_view footer = bytes.substr(footerStart, bytes.size() - footerStart - 1);
    footer = lineWithoutCarriageReturn(footer);
    if (!footer.starts_with(ProtectedFooterPrefix)) {
        outError = "protected V2 save file has an invalid integrity footer";
        return false;
    }

    const std::string_view footerValues = footer.substr(ProtectedFooterPrefix.size());
    const std::size_t valueSeparator = footerValues.find(' ');
    if (valueSeparator == std::string_view::npos ||
        footerValues.find(' ', valueSeparator + 1) != std::string_view::npos) {
        outError = "protected V2 save file has malformed integrity values";
        return false;
    }

    std::uint64_t expectedSize = 0;
    std::uint32_t expectedChecksum = 0;
    if (!parseUnsignedDecimal(footerValues.substr(0, valueSeparator), expectedSize) ||
        !parseChecksum(footerValues.substr(valueSeparator + 1), expectedChecksum)) {
        outError = "protected V2 save file has invalid integrity values";
        return false;
    }

    const std::string_view payload = bytes.substr(payloadStart, footerStart - payloadStart);
    if ((!payload.empty() && payload.back() != '\n') || expectedSize != payload.size()) {
        outError = "protected V2 save file payload size does not match";
        return false;
    }
    if (crc32(payload) != expectedChecksum) {
        outError = "protected V2 save file checksum does not match";
        return false;
    }

    outDocument.format = SaveFileFormat::ProtectedV2;
    outDocument.payload.assign(payload);
    return true;
}

std::string protectedSaveFileContent(std::string payload)
{
    if (!payload.empty() && payload.back() != '\n') {
        payload.push_back('\n');
    }

    std::string content;
    content.reserve(
        ProtectedHeader.size() + 1 + payload.size() + ProtectedFooterPrefix.size() + 32);
    content.append(ProtectedHeader);
    content.push_back('\n');
    content.append(payload);
    content.append(ProtectedFooterPrefix);
    content.append(std::to_string(payload.size()));
    content.push_back(' ');
    content.append(fixedWidthHex(crc32(payload)));
    content.push_back('\n');
    return content;
}

std::filesystem::path uniqueTemporaryPathFor(const std::filesystem::path& destination)
{
    std::filesystem::path result = destination;
    result += ".tmp.";
#ifdef _WIN32
    result += std::to_string(GetCurrentProcessId());
#else
    result += "process";
#endif
    result += ".";
    result += std::to_string(TemporaryFileSequence.fetch_add(1, std::memory_order_relaxed));
    return result;
}

class TemporaryFileGuard {
public:
    explicit TemporaryFileGuard(std::filesystem::path path) : path_(std::move(path)) {}
    TemporaryFileGuard(const TemporaryFileGuard&) = delete;
    TemporaryFileGuard& operator=(const TemporaryFileGuard&) = delete;

    ~TemporaryFileGuard()
    {
        if (!committed_) {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }
    }

    const std::filesystem::path& path() const { return path_; }
    void commit() { committed_ = true; }

private:
    std::filesystem::path path_;
    bool committed_ = false;
};

bool writeRawFileAndFlush(const std::filesystem::path& path, std::string_view bytes, std::string& outError)
{
#ifdef _WIN32
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const unsigned long error = GetLastError();
        outError = "failed to create temporary save file " + pathForError(path) + ": " + systemErrorMessage(error);
        return false;
    }

    std::size_t offset = 0;
    bool ok = true;
    while (offset < bytes.size()) {
        const std::size_t remaining = bytes.size() - offset;
        const DWORD chunkSize = static_cast<DWORD>(std::min<std::size_t>(
            remaining,
            static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD written = 0;
        const bool writeSucceeded = WriteFile(file, bytes.data() + offset, chunkSize, &written, nullptr) != FALSE;
        if (!writeSucceeded || written != chunkSize) {
            const unsigned long error = writeSucceeded ? ERROR_WRITE_FAULT : GetLastError();
            outError = "failed to write temporary save file " + pathForError(path) + ": " + systemErrorMessage(error);
            ok = false;
            break;
        }
        offset += written;
    }
    if (ok && !FlushFileBuffers(file)) {
        const unsigned long error = GetLastError();
        outError = "failed to flush temporary save file " + pathForError(path) + ": " + systemErrorMessage(error);
        ok = false;
    }
    CloseHandle(file);
    return ok;
#else
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        outError = "failed to create temporary save file: " + pathForError(path);
        return false;
    }
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.flush();
    if (!file) {
        outError = "failed to write temporary save file: " + pathForError(path);
        return false;
    }
    file.close();
    return static_cast<bool>(file);
#endif
}

bool replaceFileAtomically(
    const std::filesystem::path& temporaryPath,
    const std::filesystem::path& destinationPath,
    std::string& outError)
{
#ifdef _WIN32
    std::error_code existsError;
    const bool destinationExists = std::filesystem::exists(destinationPath, existsError) && !existsError;
    if (destinationExists && ReplaceFileW(
            destinationPath.c_str(),
            temporaryPath.c_str(),
            nullptr,
            REPLACEFILE_WRITE_THROUGH,
            nullptr,
            nullptr)) {
        return true;
    }
    if (MoveFileExW(
            temporaryPath.c_str(),
            destinationPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    const unsigned long error = GetLastError();
    outError = "failed to replace save file " + pathForError(destinationPath) + ": " + systemErrorMessage(error);
    return false;
#else
    std::error_code error;
    std::filesystem::rename(temporaryPath, destinationPath, error);
    if (!error) {
        return true;
    }
    outError = "failed to replace save file " + pathForError(destinationPath) + ": " + error.message();
    return false;
#endif
}

bool writeValidatedTemporaryFile(
    const std::filesystem::path& destinationPath,
    std::string_view bytes,
    TemporaryFileGuard& temporary,
    std::string& outError)
{
    if (!writeRawFileAndFlush(temporary.path(), bytes, outError)) {
        return false;
    }

    std::string writtenBytes;
    if (!readRawFile(temporary.path(), writtenBytes, outError)) {
        outError = "temporary save reread failed for " + pathForError(destinationPath) + ": " + outError;
        return false;
    }
    if (writtenBytes != bytes) {
        outError = "temporary save content changed after writing: " + pathForError(destinationPath);
        return false;
    }

    SaveFileDocument verification;
    std::string verificationError;
    if (!parseSaveFileContent(writtenBytes, verification, verificationError)) {
        outError = "temporary save verification failed for " + pathForError(destinationPath) + ": " + verificationError;
        return false;
    }
    return true;
}

bool installValidatedContent(
    const std::filesystem::path& destinationPath,
    std::string_view bytes,
    std::string& outError)
{
    TemporaryFileGuard temporary(uniqueTemporaryPathFor(destinationPath));
    if (!writeValidatedTemporaryFile(destinationPath, bytes, temporary, outError)) {
        return false;
    }
    if (!replaceFileAtomically(temporary.path(), destinationPath, outError)) {
        return false;
    }
    temporary.commit();
    return true;
}

}

std::filesystem::path backupPathFor(const std::filesystem::path& primaryPath)
{
    std::filesystem::path backupPath = primaryPath;
    backupPath += ".bak";
    return backupPath;
}

bool readSaveFile(
    const std::filesystem::path& path,
    SaveFileDocument& outDocument,
    std::string& outError)
{
    std::string bytes;
    if (!readRawFile(path, bytes, outError)) {
        return false;
    }

    SaveFileDocument document;
    if (!parseSaveFileContent(bytes, document, outError)) {
        outError += ": " + pathForError(path);
        return false;
    }

    outDocument = std::move(document);
    outError.clear();
    return true;
}

bool writeSaveFileAtomically(
    const std::filesystem::path& path,
    std::string payload,
    std::string& outError)
{
    return writeSaveFileAtomically(path, std::move(payload), {}, outError);
}

bool writeSaveFileAtomically(
    const std::filesystem::path& path,
    std::string payload,
    const ExistingSaveValidator& existingSaveValidator,
    std::string& outError)
{
    std::error_code directoryError;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), directoryError);
        if (directoryError) {
            outError = "failed to create save directory " + pathForError(path.parent_path()) + ": " + directoryError.message();
            return false;
        }
    }

    const std::string protectedContent = protectedSaveFileContent(std::move(payload));
    if (protectedContent.size() > MaxSaveFileBytes) {
        outError = "save data exceeds the 16 MiB safety limit";
        return false;
    }

    std::string existingBytes;
    SaveFileDocument existingDocument;
    std::string existingError;
    const bool existingIsValid =
        readRawFile(path, existingBytes, existingError) &&
        parseSaveFileContent(existingBytes, existingDocument, existingError) &&
        (!existingSaveValidator || existingSaveValidator(existingDocument));
    if (existingIsValid) {
        if (!installValidatedContent(backupPathFor(path), existingBytes, outError)) {
            return false;
        }
    }

    if (!installValidatedContent(path, protectedContent, outError)) {
        return false;
    }

    outError.clear();
    return true;
}

bool restoreSaveFileFromBackup(
    const std::filesystem::path& primaryPath,
    std::string& outError)
{
    const std::filesystem::path backupPath = backupPathFor(primaryPath);
    std::string backupBytes;
    SaveFileDocument backupDocument;
    if (!readRawFile(backupPath, backupBytes, outError) ||
        !parseSaveFileContent(backupBytes, backupDocument, outError)) {
        outError = "backup save is not recoverable: " + outError;
        return false;
    }

    if (!installValidatedContent(primaryPath, backupBytes, outError)) {
        return false;
    }
    outError.clear();
    return true;
}

}
