#include "data/GoogleSheetSource.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace majo {

namespace {

std::string trim(std::string_view text)
{
    auto begin = text.begin();
    auto end = text.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

bool parseBool(std::string_view text, bool& value)
{
    std::string copy = trim(text);
    std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (copy == "true" || copy == "1" || copy == "yes" || copy == "on") {
        value = true;
        return true;
    }
    if (copy == "false" || copy == "0" || copy == "no" || copy == "off") {
        value = false;
        return true;
    }
    return false;
}

std::string urlEncode(std::string_view text)
{
    constexpr char Hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char ch : text) {
        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded.push_back(static_cast<char>(ch));
        } else {
            encoded.push_back('%');
            encoded.push_back(Hex[ch >> 4]);
            encoded.push_back(Hex[ch & 0x0F]);
        }
    }
    return encoded;
}

std::unordered_map<std::string, std::string> parseKeyValueFile(const std::filesystem::path& path, std::string& outError)
{
    std::unordered_map<std::string, std::string> values;
    std::ifstream file(path);
    if (!file) {
        outError = "sheet source config not found: " + path.string();
        return values;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            outError = "invalid sheet source line " + std::to_string(lineNumber);
            values.clear();
            return values;
        }

        values[trim(std::string_view(line).substr(0, equals))] = trim(std::string_view(line).substr(equals + 1));
    }

    outError.clear();
    return values;
}

#ifdef _WIN32

struct WinHttpHandle {
    HINTERNET handle = nullptr;

    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET value) : handle(value) {}
    ~WinHttpHandle()
    {
        if (handle) {
            WinHttpCloseHandle(handle);
        }
    }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    explicit operator bool() const { return handle != nullptr; }
};

std::wstring toWide(std::string_view text)
{
    if (text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(std::max(0, size)), L'\0');
    if (size > 0) {
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    }
    return result;
}

std::string toUtf8(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(std::max(0, size)), '\0');
    if (size > 0) {
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    }
    return result;
}

bool crackUrl(const std::string& url, URL_COMPONENTS& parts, std::wstring& host, std::wstring& path, std::string& outError)
{
    std::wstring wideUrl = toWide(url);
    host.assign(256, L'\0');
    path.assign(4096, L'\0');
    std::wstring extra(2048, L'\0');

    ZeroMemory(&parts, sizeof(parts));
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host.data();
    parts.dwHostNameLength = static_cast<DWORD>(host.size());
    parts.lpszUrlPath = path.data();
    parts.dwUrlPathLength = static_cast<DWORD>(path.size());
    parts.lpszExtraInfo = extra.data();
    parts.dwExtraInfoLength = static_cast<DWORD>(extra.size());

    if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &parts)) {
        outError = "invalid sheet URL";
        return false;
    }

    host.resize(parts.dwHostNameLength);
    path.resize(parts.dwUrlPathLength);
    extra.resize(parts.dwExtraInfoLength);
    path += extra;
    return true;
}

bool readResponseBody(HINTERNET request, std::string& outBody, std::string& outError)
{
    outBody.clear();
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            outError = "failed to read sheet response";
            return false;
        }
        if (available == 0) {
            break;
        }

        const std::size_t start = outBody.size();
        outBody.resize(start + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, outBody.data() + start, available, &read)) {
            outError = "failed to read sheet data";
            return false;
        }
        outBody.resize(start + read);
    }
    return true;
}

bool queryStatusCode(HINTERNET request, DWORD& outStatus)
{
    DWORD size = sizeof(outStatus);
    return WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &outStatus,
        &size,
        WINHTTP_NO_HEADER_INDEX) == TRUE;
}

bool queryLocation(HINTERNET request, std::string& outLocation)
{
    DWORD size = 0;
    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_LOCATION,
        WINHTTP_HEADER_NAME_BY_INDEX,
        WINHTTP_NO_OUTPUT_BUFFER,
        &size,
        WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) {
        return false;
    }

    std::wstring location(size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_LOCATION,
            WINHTTP_HEADER_NAME_BY_INDEX,
            location.data(),
            &size,
            WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }

    if (!location.empty() && location.back() == L'\0') {
        location.pop_back();
    }
    outLocation = toUtf8(location);
    return !outLocation.empty();
}

bool downloadTextFromUrl(const std::string& initialUrl, std::string& outText, std::string& outError)
{
    WinHttpHandle session(WinHttpOpen(
        L"MajoShovel/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0));
    if (!session) {
        outError = "failed to open HTTP session";
        return false;
    }

    std::string url = initialUrl;
    for (int redirect = 0; redirect < 6; ++redirect) {
        URL_COMPONENTS parts{};
        std::wstring host;
        std::wstring path;
        if (!crackUrl(url, parts, host, path, outError)) {
            return false;
        }

        if (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) {
            outError = "unsupported sheet URL scheme";
            return false;
        }

        WinHttpHandle connection(WinHttpConnect(session.handle, host.c_str(), parts.nPort, 0));
        if (!connection) {
            outError = "failed to connect to sheet host";
            return false;
        }

        const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
        WinHttpHandle request(WinHttpOpenRequest(
            connection.handle,
            L"GET",
            path.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags));
        if (!request) {
            outError = "failed to create sheet request";
            return false;
        }

        if (!WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request.handle, nullptr)) {
            outError = "failed to fetch sheet data";
            return false;
        }

        DWORD status = 0;
        if (!queryStatusCode(request.handle, status)) {
            outError = "failed to read sheet status";
            return false;
        }

        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            std::string location;
            if (!queryLocation(request.handle, location)) {
                outError = "sheet redirect had no location";
                return false;
            }
            url = location;
            continue;
        }

        if (status != 200) {
            outError = "sheet request returned HTTP " + std::to_string(status);
            return false;
        }

        return readResponseBody(request.handle, outText, outError);
    }

    outError = "too many sheet redirects";
    return false;
}

#else

bool downloadTextFromUrl(const std::string&, std::string&, std::string& outError)
{
    outError = "Google Sheet download is only implemented on Windows";
    return false;
}

#endif

bool hasRowContent(const GoogleSheetRow& row)
{
    for (const std::string& cell : row) {
        if (!trim(cell).empty()) {
            return true;
        }
    }
    return false;
}

}

bool loadGoogleSheetSourceConfig(const std::filesystem::path& path, GoogleSheetSourceConfig& outConfig, std::string& outError)
{
    GoogleSheetSourceConfig config;
    const std::unordered_map<std::string, std::string> values = parseKeyValueFile(path, outError);
    if (!outError.empty()) {
        outConfig = config;
        return false;
    }

    auto it = values.find("enabled");
    if (it != values.end() && !parseBool(it->second, config.enabled)) {
        outError = "invalid bool for enabled";
        outConfig = config;
        return false;
    }

    it = values.find("spreadsheet_id");
    if (it != values.end()) {
        config.spreadsheetId = trim(it->second);
    }

    it = values.find("gid");
    if (it != values.end()) {
        config.gid = trim(it->second);
    }

    it = values.find("objects_sheet");
    if (it != values.end()) {
        config.objectsSheet = trim(it->second);
    }

    if (config.enabled && config.spreadsheetId.empty()) {
        outError = "spreadsheet_id is required when sheet source is enabled";
        outConfig = GoogleSheetSourceConfig{};
        return false;
    }
    if (config.objectsSheet.empty()) {
        config.objectsSheet = "Objects";
    }

    outConfig = config;
    outError.clear();
    return true;
}

std::string googleSheetCsvUrl(const GoogleSheetSourceConfig& config)
{
    return "https://docs.google.com/spreadsheets/d/" + config.spreadsheetId + "/export?format=csv&gid=" + config.gid;
}

std::string googleSheetCsvUrlForSheet(const GoogleSheetSourceConfig& config, std::string_view sheetName)
{
    return "https://docs.google.com/spreadsheets/d/" + config.spreadsheetId + "/gviz/tq?tqx=out:csv&sheet=" + urlEncode(sheetName);
}

bool parseGoogleSheetCsv(std::string_view csv, GoogleSheetTable& outTable, std::string& outError)
{
    GoogleSheetTable table;
    GoogleSheetRow row;
    std::string cell;
    bool inQuotes = false;

    auto flushRow = [&]() {
        if (cell.ends_with('\r')) {
            cell.pop_back();
        }
        row.push_back(cell);
        cell.clear();

        if (hasRowContent(row)) {
            table.rows.push_back(row);
        }
        row.clear();
    };

    for (std::size_t i = 0; i < csv.size(); ++i) {
        const char ch = csv[i];
        if (inQuotes) {
            if (ch == '"' && i + 1 < csv.size() && csv[i + 1] == '"') {
                cell.push_back('"');
                ++i;
            } else if (ch == '"') {
                inQuotes = false;
            } else {
                cell.push_back(ch);
            }
            continue;
        }

        if (ch == '"') {
            inQuotes = true;
        } else if (ch == ',') {
            if (cell.ends_with('\r')) {
                cell.pop_back();
            }
            row.push_back(cell);
            cell.clear();
        } else if (ch == '\n') {
            flushRow();
        } else {
            cell.push_back(ch);
        }
    }

    if (inQuotes) {
        outError = "unterminated quoted CSV cell";
        outTable = GoogleSheetTable{};
        return false;
    }

    if (!cell.empty() || !row.empty()) {
        flushRow();
    }

    outTable = table;
    outError.clear();
    return true;
}

bool loadGoogleSheetCsv(const GoogleSheetSourceConfig& config, std::string& outCsv, std::string& outError)
{
    if (!config.enabled) {
        outError = "Google Sheet source is disabled";
        return false;
    }
    if (config.spreadsheetId.empty()) {
        outError = "spreadsheet_id is empty";
        return false;
    }

    return downloadTextFromUrl(googleSheetCsvUrl(config), outCsv, outError);
}

bool loadGoogleSheetCsvForSheet(const GoogleSheetSourceConfig& config, std::string_view sheetName, std::string& outCsv, std::string& outError)
{
    if (!config.enabled) {
        outError = "Google Sheet source is disabled";
        return false;
    }
    if (config.spreadsheetId.empty()) {
        outError = "spreadsheet_id is empty";
        return false;
    }
    if (trim(sheetName).empty()) {
        outError = "sheet name is empty";
        return false;
    }

    return downloadTextFromUrl(googleSheetCsvUrlForSheet(config, sheetName), outCsv, outError);
}

bool loadGoogleSheetTable(const GoogleSheetSourceConfig& config, GoogleSheetTable& outTable, std::string& outError)
{
    std::string csv;
    if (!loadGoogleSheetCsv(config, csv, outError)) {
        return false;
    }
    return parseGoogleSheetCsv(csv, outTable, outError);
}

bool loadGoogleSheetTableForSheet(const GoogleSheetSourceConfig& config, std::string_view sheetName, GoogleSheetTable& outTable, std::string& outError)
{
    std::string csv;
    if (!loadGoogleSheetCsvForSheet(config, sheetName, csv, outError)) {
        return false;
    }
    return parseGoogleSheetCsv(csv, outTable, outError);
}

bool loadRuntimeBalanceFromGoogleSheet(const GoogleSheetSourceConfig& config, RuntimeBalance& outBalance, std::string& outError)
{
    std::string csv;
    if (!loadGoogleSheetCsv(config, csv, outError)) {
        return false;
    }

    return loadRuntimeBalanceFromCsv(csv, outBalance, outError);
}

}
