#pragma once

#include "data/GoogleSheetSource.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

struct EffectSpec {
    std::string target;
    std::vector<std::string> effects;
    std::vector<double> values;
    double duration = 0.0;

    bool operator==(const EffectSpec&) const = default;
};

struct ObjectDefinition {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    int rarity = 1;
    int price = 0;
    std::vector<EffectSpec> normalEffects;
    std::vector<EffectSpec> orbitEffects;
    int attackPower = 0;
    std::string damageType;
    int digPower = 0;
    int durability = 0;
    double weightKg = 0.0;
    std::vector<std::string> tags;
    std::string effectText;
    std::vector<std::string> capturedBehaviorIds;

    bool operator==(const ObjectDefinition&) const = default;
};

using ItemData = ObjectDefinition;

struct EffectCodeDefinition {
    std::string code;
    std::string displayName;
    std::string category;
    std::string usableField;
    std::vector<std::string> targets;
    std::string valueMeaning;
    std::string durationMeaning;
    std::string stackingRule;
    std::string implementationState;
    std::string note;

    bool operator==(const EffectCodeDefinition&) const = default;
};

struct SpecialTagDefinition {
    std::string tag;
    std::string displayName;
    std::string category;
    std::string usage;
    std::vector<std::string> targetCategories;
    std::string exclusiveGroup;
    std::vector<std::string> relatedEffectCodes;
    std::string implementationState;
    std::string note;

    bool operator==(const SpecialTagDefinition&) const = default;
};

enum class DbValidationSeverity {
    Warning,
    Error,
};

enum class DbValidationCategory {
    UndefinedEffectCode,
    UndefinedSpecialTag,
    TargetMismatch,
    EffectSpecSyntax,
    ExclusiveTagConflict,
    DuplicateId,
    NumericValue,
    ObjectField,
    TagEffectCodeNameCollision,
    DeprecatedEntry,
};

struct DbValidationIssue {
    DbValidationSeverity severity = DbValidationSeverity::Warning;
    DbValidationCategory category = DbValidationCategory::ObjectField;
    std::string message;

    bool operator==(const DbValidationIssue&) const = default;
};

class ItemRegistry {
public:
    void rebuild(std::vector<ItemData> items);
    void clear();

    [[nodiscard]] const std::vector<ItemData>& items() const;
    [[nodiscard]] const ItemData* findById(std::string_view id) const;
    [[nodiscard]] std::vector<const ItemData*> findByCategory(std::string_view category) const;
    [[nodiscard]] std::vector<const ItemData*> findByTag(std::string_view tag) const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;

    bool operator==(const ItemRegistry&) const = default;

private:
    std::vector<ItemData> items_;
    std::unordered_map<std::string, std::size_t> idIndex_;
    std::unordered_map<std::string, std::vector<std::size_t>> categoryIndex_;
    std::unordered_map<std::string, std::vector<std::size_t>> tagIndex_;
};

struct ObjectCatalog {
    std::vector<ObjectDefinition> objects;
    std::unordered_map<std::string, ObjectDefinition> objectsById;
    std::unordered_map<std::string, EffectCodeDefinition> effectCodes;
    std::unordered_map<std::string, SpecialTagDefinition> specialTags;
    std::vector<DbValidationIssue> validationIssues;
    std::vector<std::string> validationWarnings;
    ItemRegistry registry;

    bool operator==(const ObjectCatalog&) const = default;
};

bool parseEffectSpecs(
    std::string_view text,
    std::vector<EffectSpec>& outEffects,
    std::string& outError,
    std::vector<std::string>* outWarnings = nullptr);
bool parseObjectCatalog(const GoogleSheetTable& table, ObjectCatalog& outCatalog, std::string& outError);
bool parseEffectCodeDefinitions(const GoogleSheetTable& table, std::unordered_map<std::string, EffectCodeDefinition>& outDefinitions, std::string& outError);
bool parseSpecialTagDefinitions(const GoogleSheetTable& table, std::unordered_map<std::string, SpecialTagDefinition>& outDefinitions, std::string& outError);
bool loadObjectCatalogFromGoogleSheet(const GoogleSheetSourceConfig& config, ObjectCatalog& outCatalog, std::string& outError);
std::string effectCodeDisplayName(const ObjectCatalog& catalog, std::string_view code);
std::string effectSummaryText(const ObjectCatalog& catalog, const std::vector<EffectSpec>& specs);
std::string_view dbValidationSeverityName(DbValidationSeverity severity);
std::string_view dbValidationCategoryName(DbValidationCategory category);

}
