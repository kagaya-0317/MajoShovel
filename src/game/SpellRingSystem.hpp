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

constexpr int SpellRingCount = 3;

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

Vec2 getRingCenterWorldPosition(Vec2 playerPosition, Vec2 playerFacing, float spellRingShift);
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
    void initialize(const RuntimeBalance& balance);
    void update(Player& player, const Input& input, float dt, float totalTime, bool paused, bool blockPointerThrow, const RuntimeBalance& balance);
    void upgradeRadius(float factor) { radius_ *= factor; }
    void upgradeSpeed(float factor) { angularSpeed_ *= factor; }
    void setRadius(float radius) { radius_ = radius; }
    void setAngularSpeed(float angularSpeed) { angularSpeed_ = angularSpeed; }
    void clearOrbitModifiers();
    void setOrbitModifiers(OrbitModifiers modifiers);
    void applyOrbitModifierEffect(std::string_view effect, double value, std::string_view source);
    void applyEnemyOrbitSpeedDebuff(float multiplier, float durationSeconds);
    void upgradeShovelPower(int amount);
    void upgradeItemDamage(int amount);
    bool addItem(SpellRingItemType type);
    bool addItem(SpellRingItem item, SpellRingAddResult* outResult = nullptr);
    bool addObjectItem(const ItemData& item, SpellRingAddResult* outResult = nullptr);
    bool addObjectItem(const ItemData& item, const ItemInstance& instance, SpellRingAddResult* outResult = nullptr);
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
    Vec2 center() const { return center_; }
    float radius() const { return radius_; }
    float angularSpeed() const { return angularSpeed_; }
    float effectiveAngularSpeed() const;
    float totalEquippedWeight() const;
    float maxEquippedWeight() const;
    int maxItemCount() const;
    float weightSpeedMultiplier() const;
    double effectivePowerMultiplier() const { return orbitModifiers_.powerMultiplier; }
    double effectiveGravityMultiplier() const { return orbitModifiers_.gravityMultiplier; }
    double effectiveAntigravityMultiplier() const { return orbitModifiers_.antigravityMultiplier; }
    double orbitAnchorStrength() const { return orbitModifiers_.anchorStrength; }
    double orbitShiftMultiplier() const { return orbitModifiers_.shiftMultiplier; }
    double speedDamageMultiplier() const { return orbitModifiers_.speedDamageMultiplier; }
    const OrbitModifiers& orbitModifiers() const { return orbitModifiers_; }
    SpellRingState state() const { return state_; }
    int activeRingIndex() const { return activeRingIndex_; }
    float shapeRotation() const { return shapeRotationForRing(activeRingIndex_); }
    float cooldownRatio(const Player& player, const RuntimeBalance& balance) const;

private:
    std::array<std::vector<SpellRingItem>, SpellRingCount> itemsByRing_{};
    std::array<RingShape, SpellRingCount> ringShapes_{
        RingShape::Circle,
        RingShape::FigureEight,
        RingShape::Comet,
    };
    Vec2 center_{};
    Vec2 throwDirection_{1.0f, 0.0f};
    Vec2 throwStart_{};
    float radius_ = 54.0f;
    float angularSpeed_ = 2.72f;
    float baseEquippedWeight_ = 0.0f;
    std::array<float, SpellRingCount> baseAngles_{};
    std::array<float, SpellRingCount> shapeRotations_{};
    float throwTime_ = 0.0f;
    float capturedHealTimer_ = 0.0f;
    float enemyOrbitSpeedDebuffMultiplier_ = 1.0f;
    float enemyOrbitSpeedDebuffTimer_ = 0.0f;
    int activeRingIndex_ = 0;
    OrbitModifiers orbitModifiers_{};
    RingOrbitTuning orbitTuning_{};
    SpellRingState state_ = SpellRingState::Normal;
    std::vector<RingItemBreakEvent> itemBreakEvents_;

    std::vector<SpellRingItem>& activeItems();
    const std::vector<SpellRingItem>& activeItems() const;
    bool canPlaceItemAtAngle(const SpellRingItem& item, float angle, int ignoreIndex, const RingOrbitTuning& tuning) const;
    std::optional<float> findBestPlacementAngle(const SpellRingItem& item, int ignoreIndex, const RingOrbitTuning& tuning) const;
};

}
