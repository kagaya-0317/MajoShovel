#include "data/ObjectCatalog.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>

namespace majo {

namespace {

constexpr std::string_view HeaderName = "\xE5\x90\x8D\xE5\x89\x8D";
constexpr std::string_view HeaderDescription = "\xE8\xAA\xAC\xE6\x98\x8E";
constexpr std::string_view HeaderPrice = "\xE5\x80\xA4\xE6\xAE\xB5";
constexpr std::string_view HeaderAttackPower = "\xE6\x94\xBB\xE6\x92\x83\xE5\x8A\x9B";
constexpr std::string_view HeaderEffect = "\xE5\x8A\xB9\xE6\x9E\x9C";

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

bool equalsIgnoreCase(std::string_view left, std::string_view right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }
    return true;
}

bool parseInt(std::string_view text, int& value)
{
    const std::string copy = trim(text);
    if (copy.empty()) {
        value = 0;
        return true;
    }

    const char* begin = copy.data();
    const char* end = copy.data() + copy.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool headerMatches(std::string_view header, std::initializer_list<std::string_view> candidates)
{
    const std::string normalized = trim(header);
    for (std::string_view candidate : candidates) {
        if (normalized == candidate || equalsIgnoreCase(normalized, candidate)) {
            return true;
        }
    }
    return false;
}

int findColumn(const GoogleSheetRow& headers, std::initializer_list<std::string_view> names)
{
    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (headerMatches(headers[i], names)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string cellAt(const GoogleSheetRow& row, int column)
{
    if (column < 0 || static_cast<std::size_t>(column) >= row.size()) {
        return {};
    }
    return trim(row[static_cast<std::size_t>(column)]);
}

}

bool parseObjectCatalog(const GoogleSheetTable& table, ObjectCatalog& outCatalog, std::string& outError)
{
    ObjectCatalog catalog;
    if (table.rows.empty()) {
        outError = "Objects sheet is empty";
        outCatalog = catalog;
        return false;
    }

    const GoogleSheetRow& headers = table.rows.front();
    const int nameColumn = findColumn(headers, {HeaderName, "name"});
    const int descriptionColumn = findColumn(headers, {HeaderDescription, "description", "desc"});
    const int priceColumn = findColumn(headers, {HeaderPrice, "price"});
    const int attackPowerColumn = findColumn(headers, {HeaderAttackPower, "attack_power", "attack", "power"});
    const int effectColumn = findColumn(headers, {HeaderEffect, "effect"});

    if (nameColumn < 0) {
        outError = "Objects sheet is missing the name column";
        outCatalog = catalog;
        return false;
    }

    for (std::size_t rowIndex = 1; rowIndex < table.rows.size(); ++rowIndex) {
        const GoogleSheetRow& row = table.rows[rowIndex];
        ObjectDefinition object;
        object.name = cellAt(row, nameColumn);
        if (object.name.empty()) {
            continue;
        }

        object.description = cellAt(row, descriptionColumn);
        object.effect = cellAt(row, effectColumn);

        if (!parseInt(cellAt(row, priceColumn), object.price)) {
            outError = "invalid price in Objects row " + std::to_string(rowIndex + 1);
            outCatalog = catalog;
            return false;
        }
        if (!parseInt(cellAt(row, attackPowerColumn), object.attackPower)) {
            outError = "invalid attack power in Objects row " + std::to_string(rowIndex + 1);
            outCatalog = catalog;
            return false;
        }

        catalog.objects.push_back(object);
    }

    outCatalog = catalog;
    outError.clear();
    return true;
}

bool loadObjectCatalogFromGoogleSheet(const GoogleSheetSourceConfig& config, ObjectCatalog& outCatalog, std::string& outError)
{
    GoogleSheetTable table;
    if (!loadGoogleSheetTableForSheet(config, config.objectsSheet, table, outError)) {
        return false;
    }
    return parseObjectCatalog(table, outCatalog, outError);
}

}
