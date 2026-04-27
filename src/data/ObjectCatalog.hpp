#pragma once

#include "data/GoogleSheetSource.hpp"

#include <string>
#include <vector>

namespace majo {

struct ObjectDefinition {
    std::string name;
    std::string description;
    int price = 0;
    int attackPower = 0;
    std::string effect;

    bool operator==(const ObjectDefinition&) const = default;
};

struct ObjectCatalog {
    std::vector<ObjectDefinition> objects;

    bool operator==(const ObjectCatalog&) const = default;
};

bool parseObjectCatalog(const GoogleSheetTable& table, ObjectCatalog& outCatalog, std::string& outError);
bool loadObjectCatalogFromGoogleSheet(const GoogleSheetSourceConfig& config, ObjectCatalog& outCatalog, std::string& outError);

}
