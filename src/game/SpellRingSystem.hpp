#pragma once

#include "engine/Input.hpp"
#include "engine/Math.hpp"
#include "data/ObjectCatalog.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/OrbitModifiers.hpp"
#include "game/SpellRingItem.hpp"
#include "game/Player.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace majo {

enum class RingShape {
    Circle = 0,
    FigureEight = 1,
    Comet = 2
};

enum class SpellRingState {
    Normal,
    Thrown,
    Returning
};

enum class RingMotionEventKind {
    ThrowStart,
    ReturnStart,
    ReturnEnd
};

constexpr int SpellRingCount = 3;

struct RingMotionEvent {
    RingMotionEventKind kind = RingMotionEventKind::ThrowStart;
    int ringIndex = 0;
    Vec2 position{};
    Vec2 direction{1.0f, 0.0f};
};

RingShape defaultRingShapeForIndex(int ringIndex);
const char* ringShapeName(RingShape shape);
float ringShapeOrbitSpeedMultiplier(RingShape shape, const RuntimeBalance& balance);

struct RingOrbitTuning {
    float figure8WidthMultiplier = 1.20f;
    float figure8HeightMultiplier = 0.70f;
    float figure8ShapeRotationSpeed = 0.25f;

    float cometRadiusMultiplier = 1.25f;
    float cometArcDegrees = 100.0f;
    float cometSpeedMultiplier = 1.10f;
    float cometTrailLength = 0.20f;
    float cometLaneSpacing = 10.0f;
    float cometMaxArcDegrees = 130.0f;
};

RingOrbitTuning makeRingOrbitTuning(const RuntimeBalance& balance);

struct RingOrbitContext {
    RingShape shape = RingShape::Circle;
    float radius = 0.0f;
    float shapeRotation = 0.0f;
    int itemIndex = 0;
    int itemCount = 1;
    RingOrbitTuning tuning{};
};

struct SpellRingAddResult {
    int ringIndex = 0;
    int itemIndex = -1;
    float localAngle = 0.0f;
    std::string objectId;
    std::string instanceId;
};

struct RingItemBreakEvent {
    Vec2 position{};
    SpellRingItemType type = SpellRingItemType::Object;
    std::string objectId;
    std::string instanceId;
    bool protectionEnabled = false;
};

Vec2 getRingCenterWorldPosition(Vec2 playerPosition, Vec2 shiftDirection, float spellRingShift);
Vec2 getRingItemLocalPosition(float localAngle, const RingOrbitContext& context);
Vec2 getRingItemWorldPosition(Vec2 center, float localAngle, const RingOrbitContext& context);
Vec2 getRingItemVelocity(
    float localAngle,
    float localAngularSpeed,
    float shapeRotationSpeed,
    Vec2 centerVelocity,
    const RingOrbitContext& context);
std::vector<Vec2> getRingPathSamplePoints(Vec2 center, const RingOrbitContext& context, int sampleCount = 96);
float findNearestRingPathParam(Vec2 worldPoint, Vec2 center, const RingOrbitContext& context, int sampleCount = 256);

class SpellRingSystem {
public:
    static constexpr float InitialMaxEquippedWeight = 10.0f;
    static constexpr float LevelRingScaleUpgradeAmount = 0.1f;
    static constexpr float LevelWeightLimitUpgradeAmount = 1.0f;
    static float levelScaleMultiplierForPoints(int points);

    void initialize(const RuntimeBalance& balance);
    void update(Player& player, const Input& input, float dt, float totalTime, bool paused, bool blockPointerThrow, const RuntimeBalance& balance);
    void updatePresentation(const Player& player, float dt, const RuntimeBalance& balance);
    void resetRuntimeStateAtPlayer(const Player& player, const RuntimeBalance& balance);
    void upgradeRadius(float factor);
    void upgradeSpeed(float factor);
    void upgradeRadiusForRing(int ringIndex, float factor);
    void upgradeSpeedForRing(int ringIndex, float factor);
    void setRadius(float radius);
    void setAngularSpeed(float angularSpeed);
    void setRadiusForRing(int ringIndex, float radius);
    void setAngularSpeedForRing(int ringIndex, float angularSpeed);
    void clearOrbitModifiers();
    void setOrbitModifiers(OrbitModifiers modifiers);
    void setEquipmentModifiers(EquipmentModifiers modifiers);
    void applyOrbitModifierEffect(std::string_view effect, double value, std::string_view source);
    void applyEnemyOrbitSpeedDebuff(float multiplier, float durationSeconds);
    void upgradeItemDamage(int amount);
    void upgradeMaxEquippedWeightForAllRings(float amount);
    void setMaxEquippedWeightForAllRings(float maxWeight);
    void upgradeMaxEquippedWeightForRing(int ringIndex, float amount);
    void setMaxEquippedWeightForRing(int ringIndex, float maxWeight);
    bool addItem(SpellRingItemType type);
    bool addItem(SpellRingItem item, SpellRingAddResult* outResult = nullptr);
    bool addObjectItem(const ItemData& item, SpellRingAddResult* outResult = nullptr);
    bool addObjectItem(const ItemData& item, const ItemInstance& instance, SpellRingAddResult* outResult = nullptr);
    bool addObjectItemToRing(int ringIndex, const ItemData& item, SpellRingAddResult* outResult = nullptr);
    bool addObjectItemToRing(
        int ringIndex,
        const ItemData& item,
        const ItemInstance& instance,
        SpellRingAddResult* outResult = nullptr);
    bool canAddObjectItem(const ItemData& item) const;
    bool canAddObjectItem(const ItemData& item, const ItemInstance& instance) const;
    bool canAddObjectItemForRing(int ringIndex, const ItemData& item) const;
    bool canAddObjectItemForRing(int ringIndex, const ItemData& item, const ItemInstance& instance) const;
    bool canAddObjectItemAtAngle(const ItemData& item, float localAngle) const;
    bool canAddObjectItemAtAngle(const ItemData& item, const ItemInstance& instance, float localAngle) const;
    bool addObjectItemAtAngle(const ItemData& item, float localAngle, SpellRingAddResult* outResult = nullptr);
    bool addObjectItemAtAngle(
        const ItemData& item,
        const ItemInstance& instance,
        float localAngle,
        SpellRingAddResult* outResult = nullptr);
    bool repairItem(int ringIndex, int itemIndex);
    bool enhanceItem(
        int ringIndex,
        int itemIndex,
        int attackBonus,
        int digBonus,
        int durabilityBonus,
        int maxEnhanceLevel,
        const ObjectCatalog& catalog);
    bool canAddItem() const;
    bool consumeItemDurability(SpellRingItem& item, int amount = 1);
    std::vector<RingItemBreakEvent> consumeItemBreakEvents();
    bool canAddItem(const SpellRingItem& item) const;
    bool canAddItemForRing(int ringIndex) const;
    bool canAddItemForRing(int ringIndex, const SpellRingItem& item) const;
    bool canPlaceItemAtAngle(int index, float angle) const;
    std::optional<float> nearestPlaceableAngle(int index, float desiredAngle, float maxDeltaRadians) const;
    bool moveItemAngle(int index, float deltaRadians);
    void normalizeItemPlacements();
    void switchActiveRing(int delta);
    void applyObjectParameters(const ObjectCatalog& catalog);
    void removeBrokenItems();
    void resetBaseWeightToCurrent();
    void setRingShapeForIndex(int ringIndex, RingShape shape);
    RingShape ringShapeForIndex(int ringIndex) const;
    RingShape activeRingShape() const;
    RingShape runtimeRingShape() const;
    float ringBaseAngleForIndex(int ringIndex) const;
    float shapeRotationForRing(int ringIndex) const;
    float ringAngularSpeedForIndex(int ringIndex, const RuntimeBalance& balance) const;
    RingOrbitContext makeOrbitContext(int itemIndex, int itemCount, float radiusScale, const RuntimeBalance& balance) const;
    RingOrbitContext makeOrbitContextForRing(int ringIndex, int itemIndex, int itemCount, float radiusScale, const RuntimeBalance& balance) const;
    Vec2 sampleItemWorldPosition(float localAngle, int itemIndex, int itemCount, float radiusScale, const RuntimeBalance& balance) const;
    Vec2 sampleItemWorldPositionForRing(int ringIndex, float localAngle, int itemIndex, int itemCount, float radiusScale, const RuntimeBalance& balance) const;
    float nearestPathParam(Vec2 worldPoint, Vec2 center, float radiusScale, const RuntimeBalance& balance, int sampleCount = 256) const;
    float nearestPathParamForRing(int ringIndex, Vec2 worldPoint, Vec2 center, float radiusScale, const RuntimeBalance& balance, int sampleCount = 256) const;
    std::vector<Vec2> pathSamplePoints(Vec2 center, float radiusScale, const RuntimeBalance& balance, int sampleCount = 96) const;
    std::vector<Vec2> pathSamplePointsForRing(int ringIndex, Vec2 center, float radiusScale, const RuntimeBalance& balance, int sampleCount = 96) const;
    std::vector<Vec2> runtimePathSamplePointsForRing(int ringIndex, const RuntimeBalance& balance, int sampleCount = 96) const;
    float normalizeLocalAngle(float angle, const RuntimeBalance& balance) const;
    float quantizeLocalAngle(float angle, const RuntimeBalance& balance) const;

    const std::vector<SpellRingItem>& items() const { return itemsByRing_[static_cast<std::size_t>(activeRingIndex_)]; }
    std::vector<SpellRingItem>& items() { return itemsByRing_[static_cast<std::size_t>(activeRingIndex_)]; }
    const std::vector<SpellRingItem>& itemsForRing(int ringIndex) const;
    std::vector<SpellRingItem>& itemsForRing(int ringIndex);
    const std::array<std::vector<SpellRingItem>, SpellRingCount>& ringItems() const { return itemsByRing_; }
    std::array<std::vector<SpellRingItem>, SpellRingCount>& ringItems() { return itemsByRing_; }
    std::vector<const SpellRingItem*> runtimeItems() const;
    std::vector<SpellRingItem*> runtimeItemsMutable();
    int runtimeRingCount() const { return SpellRingCount; }
    Vec2 center() const { return centerForRing(activeRingIndex_); }
    Vec2 centerForRing(int ringIndex) const;
    float radius() const { return radiusForRing(activeRingIndex_); }
    float radiusForRing(int ringIndex) const;
    float angularSpeed() const { return angularSpeedForRing(activeRingIndex_); }
    float angularSpeedForRing(int ringIndex) const;
    float effectiveAngularSpeed() const;
    float effectiveAngularSpeedForRing(int ringIndex) const;
    float totalEquippedWeight() const;
    float totalEquippedWeightForRing(int ringIndex) const;
    float maxEquippedWeight() const;
    float maxEquippedWeightForRing(int ringIndex) const;
    int maxItemCount() const;
    float weightSpeedMultiplier() const;
    float weightSpeedMultiplierForRing(int ringIndex) const;
    double effectivePowerMultiplier() const { return orbitModifiers_.powerMultiplier; }
    double effectiveGravityMultiplier() const { return orbitModifiers_.gravityMultiplier; }
    double effectiveAntigravityMultiplier() const { return orbitModifiers_.antigravityMultiplier; }
    double orbitAnchorStrength() const { return orbitModifiers_.anchorStrength; }
    double orbitShiftMultiplier() const { return orbitModifiers_.shiftMultiplier; }
    double speedDamageMultiplier() const { return orbitModifiers_.speedDamageMultiplier; }
    const OrbitModifiers& orbitModifiers() const { return orbitModifiers_; }
    const EquipmentModifiers& equipmentModifiers() const { return equipmentModifiers_; }
    const RingEquipmentModifiers& equipmentModifiersForRing(int ringIndex) const;
    double ringOutputMultiplierForRing(int ringIndex) const;
    double ringDamageSpeedMultiplierForRing(int ringIndex) const;
    double digPowerMultiplierForRing(int ringIndex) const;
    SpellRingState state() const { return stateForRing(activeRingIndex_); }
    SpellRingState stateForRing(int ringIndex) const;
    bool anyRingInFlight() const;
    int throwingRingIndex() const { return throwingRingIndex_; }
    int activeRingIndex() const { return activeRingIndex_; }
    float shapeRotation() const { return shapeRotationForRing(activeRingIndex_); }
    float cooldownRatio(const Player& player, const RuntimeBalance& balance) const;
    std::vector<RingMotionEvent> consumeMotionEvents();

private:
    struct RingRuntimeState {
        Vec2 homeCenter{};
        Vec2 center{};
        Vec2 previousCenter{};
        Vec2 throwDirection{1.0f, 0.0f};
        Vec2 throwLaunchOffset{};
        Vec2 throwReturnOffset{};
        float throwElapsed = 0.0f;
        float throwPeakTime = 0.0f;
        float throwReturnTime = 0.0f;
        float throwDistance = 0.0f;
        SpellRingState state = SpellRingState::Normal;
    };

    std::array<std::vector<SpellRingItem>, SpellRingCount> itemsByRing_{};
    std::array<RingShape, SpellRingCount> ringShapes_{
        RingShape::Circle,
        RingShape::FigureEight,
        RingShape::Comet,
    };
    std::array<RingRuntimeState, SpellRingCount> ringRuntime_{};
    std::array<float, SpellRingCount> radii_{{
        54.0f,
        54.0f,
        54.0f,
    }};
    std::array<float, SpellRingCount> angularSpeeds_{{
        2.176f,
        2.176f,
        2.176f,
    }};
    std::array<float, SpellRingCount> maxEquippedWeights_{{
        InitialMaxEquippedWeight,
        InitialMaxEquippedWeight,
        InitialMaxEquippedWeight,
    }};
    std::array<float, SpellRingCount> baseEquippedWeights_{};
    std::array<float, SpellRingCount> baseAngles_{};
    std::array<float, SpellRingCount> shapeRotations_{};
    float capturedHealTimer_ = 0.0f;
    float enemyOrbitSpeedDebuffMultiplier_ = 1.0f;
    float enemyOrbitSpeedDebuffTimer_ = 0.0f;
    int activeRingIndex_ = 0;
    int throwingRingIndex_ = -1;
    OrbitModifiers orbitModifiers_{};
    EquipmentModifiers equipmentModifiers_{};
    RingOrbitTuning orbitTuning_{};
    std::vector<RingItemBreakEvent> itemBreakEvents_;
    std::vector<RingMotionEvent> motionEvents_;

    std::vector<SpellRingItem>& activeItems();
    const std::vector<SpellRingItem>& activeItems() const;
    bool addItemToRing(int ringIndex, SpellRingItem item, SpellRingAddResult* outResult = nullptr);
    void advanceOrbitAngles(float dt, const RuntimeBalance& balance);
    void refreshItemWorldPositions(float dt, const RuntimeBalance& balance, bool advanceCapturedBehaviors);
    float throwTotalTimeForRing(int ringIndex) const;
    float throwProgressForRing(int ringIndex) const;
    float throwReachForRing(int ringIndex) const;
    float throwLineMixForRing(int ringIndex) const;
    float throwPathTForItem(int ringIndex, float pathParam, const RingOrbitContext& context) const;
    Vec2 throwMorphPathPointForRing(
        int ringIndex,
        float pathT,
        const RingOrbitContext& context,
        float distanceOffset) const;
    bool canPlaceItemAtAngle(const SpellRingItem& item, float angle, int ignoreIndex, const RingOrbitTuning& tuning) const;
    bool canPlaceItemAtAngleForRing(
        int ringIndex,
        const SpellRingItem& item,
        float angle,
        int ignoreIndex,
        const RingOrbitTuning& tuning) const;
    std::optional<float> findBestPlacementAngle(const SpellRingItem& item, int ignoreIndex, const RingOrbitTuning& tuning) const;
    std::optional<float> findBestPlacementAngleForRing(
        int ringIndex,
        const SpellRingItem& item,
        int ignoreIndex,
        const RingOrbitTuning& tuning) const;
};

}
