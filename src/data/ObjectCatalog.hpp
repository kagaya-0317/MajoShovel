#pragma once

#include "data/GoogleSheetSource.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

enum class LootChestKind {
    Common,
    Rare,
    SuperRare,
};

struct ObjectLootWeights {
    std::unordered_map<std::string, double> byColumn;

    [[nodiscard]] double weightForColumn(std::string_view columnName) const;
    [[nodiscard]] bool hasPositiveWeight() const;

    bool operator==(const ObjectLootWeights&) const = default;
};

struct EffectSpec {
    std::string target;
    std::vector<std::string> effects;
    std::vector<double> values;
    double duration = 0.0;

    bool operator==(const EffectSpec&) const = default;
};

enum class DiscoveryTrigger {
    Attack,
    Dig,
    NormalEffect,
    OrbitEffect,
};

struct DiscoveryEffectLine {
    std::string objectId;
    std::string effectKey;
    std::string text;
    DiscoveryTrigger trigger = DiscoveryTrigger::NormalEffect;

    bool operator==(const DiscoveryEffectLine&) const = default;
};

struct CapturedBehaviorSpec {
    std::string trigger;
    std::string behavior;
    std::unordered_map<std::string, std::string> params;
    double intervalSeconds = 0.0;

    bool operator==(const CapturedBehaviorSpec&) const = default;
};

enum class ObjectRingRotationMode {
    Fixed,
    Outward,
    Forward,
    Spin,
};

struct ObjectRingRotation {
    ObjectRingRotationMode mode = ObjectRingRotationMode::Fixed;
    float offsetDegrees = 0.0f;
    float spinDegreesPerSecond = 0.0f;

    bool operator==(const ObjectRingRotation&) const = default;
};

struct ObjectVisualRotation {
    float groundDegrees = 0.0f;
    ObjectRingRotation ring;

    bool operator==(const ObjectVisualRotation&) const = default;
};

enum class ItemVisualSource {
    Object,
    Enemy,
};

struct ItemVisualRef {
    ItemVisualSource source = ItemVisualSource::Object;
    int imageNumber = 0;
    std::string sourceId;

    bool operator==(const ItemVisualRef&) const = default;
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
    int imageNumber = 0;
    ItemVisualRef visual;
    ObjectVisualRotation visualRotation;
    std::vector<std::string> tags;
    std::string effectText;
    std::vector<DiscoveryEffectLine> discoveryEffectLines;
    std::vector<std::string> capturedBehaviorIds;
    std::vector<CapturedBehaviorSpec> capturedBehaviorSpecs;
    ObjectLootWeights lootWeights;

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

struct LootWeightLoadStats {
    std::size_t detectedColumnCount = 0;
    std::size_t weightedItemCount = 0;
    std::size_t warningCount = 0;

    bool operator==(const LootWeightLoadStats&) const = default;
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
    LootWeightLoadStats lootWeightStats;

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
std::string resolveLootWeightColumnName(std::string_view stageId, int depthRank, LootChestKind chestKind);
std::string resolveLootWeightColumnName(std::string_view stageId, int depthRank, std::string_view chestKind);
double lootWeightFor(const ObjectDefinition& object, std::string_view stageId, int depthRank, LootChestKind chestKind);
bool isDamageTypeAllowed(std::string_view value);
bool isPhysicalDamageType(std::string_view value);
std::string normalizeDamageType(std::string_view value);
std::string_view damageTypeDisplayName(std::string_view value);
std::vector<DiscoveryEffectLine> buildDiscoveryEffectLines(const ObjectDefinition& object, std::vector<std::string>* debugWarnings = nullptr);
std::string_view dbValidationSeverityName(DbValidationSeverity severity);
std::string_view dbValidationCategoryName(DbValidationCategory category);

}
