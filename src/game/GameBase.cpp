#include "game/GameInternal.hpp"

#include "engine/InputHelpGlyph.hpp"
#include "game/EnemyImageRenderer.hpp"

namespace majo {

namespace {

bool isTutorialStoryTrigger(std::string_view trigger)
{
    return trigger.rfind("tutorial:", 0) == 0;
}

void drawBaseControlHelp(Renderer& renderer, int screenWidth, int screenHeight, std::string help)
{
    if (help.empty()) {
        return;
    }

    InputHelpStyle helpStyle;
    helpStyle.text = {232, 232, 238, 235};
    helpStyle.outline = {0, 0, 0, 190};
    helpStyle.scale = 2;
    helpStyle.outlinePx = 4;
    helpStyle.iconHeight = 24.0f;
    helpStyle.outlineEnabled = true;

    const float screenW = static_cast<float>(screenWidth);
    const float screenH = static_cast<float>(screenHeight);
    const float maxWidth = std::max(120.0f, screenW - 32.0f);
    help = fittedInputHelpText(renderer, std::move(help), maxWidth, helpStyle);
    const Vec2 textSize = measureInputHelpText(renderer, help, helpStyle);
    const Vec2 pos{
        (screenW - textSize.x) * 0.5f,
        std::max(TopInfoBarY + TopInfoBarHeight + 8.0f, screenH - textSize.y - 4.0f),
    };
    drawInputHelpText(renderer, pos, help, helpStyle);
}

std::string baseExplorationControlHelp(const BaseFacility* facility)
{
    if (facility == nullptr) {
        return "WASD/方向キー 移動   Enter 近くの施設を調べる   Esc メニュー";
    }

    switch (facility->onInteract) {
    case BaseFacilityAction::MonicaTalk:
        return "Enter モニカと話す   Esc メニュー";
    case BaseFacilityAction::HomeEntrance:
        return "Enter ルネの家に入る   Esc メニュー";
    case BaseFacilityAction::HomeExit:
        return "Enter 屋外へ戻る   Esc メニュー";
    default:
        return std::string("Enter ") + facility->displayName + "を調べる   Esc メニュー";
    }
}

int baseUpgradeWarehouseCapacityForLevel(int level)
{
    constexpr std::array<int, 5> Capacities{{48, 72, 100, 140, 200}};
    const int index = std::clamp(level - 1, 0, static_cast<int>(Capacities.size()) - 1);
    return Capacities[static_cast<std::size_t>(index)];
}

const char* baseUpgradeMerchantFeature(int level)
{
    switch (level) {
    case 1: return "通常売買";
    case 2: return "品揃え5枠";
    case 3: return "品揃え6枠/買取+10%";
    case 4: return "宝の高価買取";
    case 5: return "品揃え8枠/レア増加";
    case 6: return "品揃え9枠/買取+20%";
    case 7: return "品揃え10枠/高レア増加";
    default: return "未解禁";
    }
}

const char* baseUpgradeProcessingFeature(int level)
{
    switch (level) {
    case 1: return "軽量化";
    case 2: return "作業台費用-10%";
    case 3: return "大型化";
    case 4: return "作業台費用-20%";
    case 5: return "作業台費用-30%";
    default: return "未解禁";
    }
}

std::unordered_map<std::string, int> buildObjectSortOrder(const ObjectCatalog& catalog)
{
    std::unordered_map<std::string, int> order;
    order.reserve(catalog.objects.size());
    for (int i = 0; i < static_cast<int>(catalog.objects.size()); ++i) {
        const ObjectDefinition& object = catalog.objects[static_cast<std::size_t>(i)];
        if (!object.id.empty() && order.find(object.id) == order.end()) {
            order.emplace(object.id, i);
        }
    }
    return order;
}

int objectSortOrder(const std::unordered_map<std::string, int>& order, const std::string& objectId)
{
    constexpr int MissingOrder = 1'000'000'000;
    const auto it = order.find(objectId);
    return it != order.end() ? it->second : MissingOrder;
}

const std::string& objectSortId(const InventoryObjectInstance& instance)
{
    return !instance.item.id.empty() ? instance.item.id : instance.instance.objectId;
}

const std::string& warehouseEntrySortId(
    int entryIndex,
    const std::vector<InventoryObjectStack>& stacks,
    const std::vector<InventoryObjectInstance>& instances)
{
    const int stackCount = static_cast<int>(stacks.size());
    if (entryIndex >= 0 && entryIndex < stackCount) {
        return stacks[static_cast<std::size_t>(entryIndex)].objectId;
    }
    const int instanceIndex = entryIndex - stackCount;
    if (instanceIndex >= 0 && instanceIndex < static_cast<int>(instances.size())) {
        return objectSortId(instances[static_cast<std::size_t>(instanceIndex)]);
    }
    static const std::string Empty;
    return Empty;
}

const char* baseUpgradeResultSubject(int index)
{
    switch (index) {
    case 0: return "倉庫容量";
    case 1: return "商人機能";
    case 2: return "作業台機能";
    case 3: return "リング工房";
    case 4: return "最大HP";
    case 5: return "リング半径";
    case 6: return "リング速度";
    case 7: return "収集術式";
    default: return "強化項目";
    }
}

std::string baseUpgradeResultChangeLine(int index, int beforeLevel, int afterLevel)
{
    char buffer[192];
    switch (index) {
    case 0:
        std::snprintf(buffer, sizeof(buffer), "倉庫容量: %d枠 → %d枠",
            baseUpgradeWarehouseCapacityForLevel(beforeLevel),
            baseUpgradeWarehouseCapacityForLevel(afterLevel));
        return buffer;
    case 1:
        std::snprintf(buffer, sizeof(buffer), "商人機能: %s → %s",
            baseUpgradeMerchantFeature(beforeLevel),
            baseUpgradeMerchantFeature(afterLevel));
        return buffer;
    case 2:
        std::snprintf(buffer, sizeof(buffer), "加工解禁: %s → %s",
            baseUpgradeProcessingFeature(beforeLevel),
            baseUpgradeProcessingFeature(afterLevel));
        return buffer;
    case 3:
        return "リング工房: 未解禁 → 解禁";
    case 4:
        std::snprintf(buffer, sizeof(buffer), "最大HP: +%d → +%d", beforeLevel * 2, afterLevel * 2);
        return buffer;
    case 5:
        std::snprintf(buffer, sizeof(buffer), "初期リング半径: +%d%% → +%d%%", beforeLevel * 8, afterLevel * 8);
        return buffer;
    case 6:
        std::snprintf(buffer, sizeof(buffer), "初期リング速度: +%d%% → +%d%%", beforeLevel * 8, afterLevel * 8);
        return buffer;
    case 7:
        std::snprintf(buffer, sizeof(buffer), "収集術式: Lv.%d → Lv.%d", beforeLevel, afterLevel);
        return buffer;
    default:
        return {};
    }
}

constexpr int RingLevelUpgradeKindCount = 3;
constexpr int BaseBackpackSourceIndex = 0;

bool sameRingLevelUpgradeSelection(RingLevelUpgradeSelection left, RingLevelUpgradeSelection right)
{
    return left.ringIndex == right.ringIndex && left.kind == right.kind;
}

constexpr std::array<std::string_view, BaseItemSourceCount> BaseItemSourceLabels{{
    "リュック",
    "収納箱",
    "リング1",
    "リング2",
    "リング3",
}};

constexpr int StorageDepositSourceCount = 1 + SpellRingCount;
constexpr float MerchantSellSourceYOffset = 44.0f;
constexpr float MerchantSellItemYOffset = MerchantSellSourceYOffset + 16.0f;
constexpr float MerchantSellRingYOffset = MerchantSellSourceYOffset + 40.0f + 40.0f;
constexpr float StorageTransferLayoutYOffset = 24.0f;
constexpr int StorageWithdrawRows = 3;
constexpr int StorageWithdrawSlotCount = StorageColumns * StorageWithdrawRows;
constexpr float StorageWithdrawGridY = 190.0f;
constexpr float StorageWithdrawRowGap = 8.0f;
constexpr float StorageWithdrawSortButtonGap = 22.0f;
constexpr float BaseRingPreviewScale = 0.9f;
constexpr float BaseProcessingRingYOffset = 64.0f;
constexpr float MerchantSellRingPreviewScale = 0.9f;
constexpr float StorageRingPreviewScale = 1.0f;
constexpr float MerchantSellRingItemLabelExtraHeight = 30.0f;
constexpr float ExternalWarehouseGridYOffset = 44.0f;
constexpr float ExternalWarehousePageSelectorGap = 10.0f;
constexpr float BaseFacilitySpawnGap = 18.0f;
constexpr float BaseMineExitReturnUpOffset = 40.0f;

enum class BaseFacilitySpawnSide {
    Above,
    Below,
};

UiRect defaultBaseFacilityRect(BaseArea area, bool ringWorkshopUnlocked, std::string_view facilityId)
{
    const std::vector<BaseFacility> facilities = baseFacilities(area, ringWorkshopUnlocked);
    const auto it = std::find_if(facilities.begin(), facilities.end(), [facilityId](const BaseFacility& facility) {
        return facility.facilityId == facilityId;
    });
    return it == facilities.end() ? UiRect{{0.0f, 0.0f}, {0.0f, 0.0f}} : it->rect;
}

UiRect outdoorHomeDoorSpawnRect(UiRect homeRect, bool ringWorkshopUnlocked)
{
    constexpr UiRect DefaultDoor{{265.0f, 222.0f}, {60.0f, 44.0f}};
    const UiRect defaultHome = defaultBaseFacilityRect(BaseArea::Outdoor, ringWorkshopUnlocked, "home");
    if (defaultHome.size.x <= 0.0f || defaultHome.size.y <= 0.0f) {
        return DefaultDoor;
    }

    const float scaleX = homeRect.size.x / defaultHome.size.x;
    const float scaleY = homeRect.size.y / defaultHome.size.y;
    const float scale = std::isfinite(scaleX) && std::isfinite(scaleY)
        ? std::max(0.001f, std::min(scaleX, scaleY))
        : 1.0f;
    return {
        DefaultDoor.pos + (homeRect.pos - defaultHome.pos),
        DefaultDoor.size * scale,
    };
}

struct BaseFacilityVisual {
    const char* facilityId = "";
    const char* imagePath = "";
    UiRect rect{};
};

constexpr std::array<BaseFacilityVisual, 7> OutdoorBaseFacilityVisuals{{
    {"mine_exit", "assets/kyoten/move.png", {{578.0f, 530.0f}, {148.0f, 190.0f}}},
    {"storage_chest", "assets/kyoten/box.png", {{593.0f, 445.0f}, {60.0f, 53.0f}}},
    {"merchant_wagon", "assets/kyoten/wagon.png", {{928.0f, 65.0f}, {206.0f, 185.0f}}},
    {"processing_table", "assets/kyoten/sagyodai.png", {{512.0f, 154.0f}, {183.0f, 106.0f}}},
    {"upgrade_forge", "assets/kyoten/kyokaro.png", {{968.0f, 355.0f}, {217.0f, 226.0f}}},
    {"ring_workshop", "assets/kyoten/ring-kobo.png", {{842.0f, 470.0f}, {115.0f, 93.0f}}},
    {"home", "assets/kyoten/house.png", {{113.0f, 11.0f}, {301.0f, 308.0f}}},
}};

constexpr std::array<BaseFacilityVisual, 3> HomeInteriorBaseFacilityVisuals{{
    {"bookshelf", "assets/kyoten/books.png", {{368.0f, 322.0f}, {127.0f, 213.0f}}},
    {"diary", "assets/kyoten/desk.png", {{760.0f, 416.0f}, {179.0f, 142.0f}}},
    {"bed", "assets/kyoten/bed.png", {{680.0f, 188.0f}, {178.0f, 195.0f}}},
}};

const BaseFacilityVisual* findBaseFacilityVisual(
    std::span<const BaseFacilityVisual> visuals,
    std::string_view facilityId)
{
    const auto it = std::find_if(
        visuals.begin(),
        visuals.end(),
        [facilityId](const BaseFacilityVisual& visual) {
            return std::string_view(visual.facilityId) == facilityId;
        });
    return it == visuals.end() ? nullptr : &*it;
}

const BaseFacilityVisual* baseFacilityVisual(BaseArea area, std::string_view facilityId)
{
    switch (area) {
    case BaseArea::Outdoor:
        return findBaseFacilityVisual(OutdoorBaseFacilityVisuals, facilityId);
    case BaseArea::HomeInterior:
        return findBaseFacilityVisual(HomeInteriorBaseFacilityVisuals, facilityId);
    }
    return nullptr;
}

UiRect baseFacilityVisualRect(
    const BaseFacility& facility,
    BaseArea area,
    bool ringWorkshopUnlocked,
    const BaseFacilityVisual& visual)
{
    const UiRect defaultRect = defaultBaseFacilityRect(area, ringWorkshopUnlocked, facility.facilityId);
    if (defaultRect.size.x <= 0.0f || defaultRect.size.y <= 0.0f) {
        return visual.rect;
    }

    const float scaleX = facility.rect.size.x / defaultRect.size.x;
    const float scaleY = facility.rect.size.y / defaultRect.size.y;
    const float visualScale = std::isfinite(scaleX) && std::isfinite(scaleY)
        ? std::max(0.001f, std::min(scaleX, scaleY))
        : 1.0f;
    return {
        visual.rect.pos + (facility.rect.pos - defaultRect.pos),
        visual.rect.size * visualScale,
    };
}

UiRect baseFacilityPointerRect(const BaseFacility& facility, BaseArea area, bool ringWorkshopUnlocked)
{
    if (const BaseFacilityVisual* visual = baseFacilityVisual(area, facility.facilityId)) {
        return baseFacilityVisualRect(facility, area, ringWorkshopUnlocked, *visual);
    }
    return facility.rect;
}

bool baseFacilityVisualHitTest(
    Renderer& renderer,
    const BaseFacilityVisual& visual,
    UiRect rect,
    Vec2 point)
{
    ImageDrawOptions options;
    const ImageHandle handle = renderer.acquireImage(visual.imagePath, TextureFilter::Nearest);
    if (!handle.valid()) {
        return rect.contains(point);
    }
    return renderer.imageHitTestAlpha(
        handle,
        rect.pos + rect.size * 0.5f,
        rect.size,
        point,
        options,
        12);
}

Vec2 closestPointOnRect(Vec2 point, UiRect rect)
{
    return {
        clamp(point.x, rect.pos.x, rect.pos.x + rect.size.x),
        clamp(point.y, rect.pos.y, rect.pos.y + rect.size.y),
    };
}

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

const BaseFacility* findBaseFacilityById(
    const std::vector<BaseFacility>& facilities,
    std::string_view facilityId)
{
    const auto it = std::find_if(
        facilities.begin(),
        facilities.end(),
        [facilityId](const BaseFacility& facility) {
            return std::string_view(facility.facilityId) == facilityId;
        });
    return it == facilities.end() ? nullptr : &*it;
}

bool pointEnteredRectFromBelow(Vec2 previousPoint, Vec2 currentPoint, UiRect rect)
{
    return rect.contains(currentPoint) &&
        previousPoint.y >= rect.pos.y + rect.size.y &&
        currentPoint.y < previousPoint.y;
}

bool pointEnteredRectFromAbove(Vec2 previousPoint, Vec2 currentPoint, UiRect rect)
{
    return rect.contains(currentPoint) &&
        previousPoint.y <= rect.pos.y &&
        currentPoint.y > previousPoint.y;
}

bool baseFacilityHiddenInNormalView(BaseArea area, const BaseFacility& facility)
{
    const std::string_view facilityId = facility.facilityId;
    if (area == BaseArea::Outdoor && facilityId == "home_entrance") {
        return true;
    }
    if (area == BaseArea::HomeInterior && facilityId == "home_exit") {
        return true;
    }
    return !facility.unlocked && facilityId == "ring_workshop";
}

void drawBaseFacilityNameLabel(
    Renderer& renderer,
    const BaseFacility& facility,
    UiRect labelRect,
    BaseArea area,
    bool ringWorkshopUnlocked)
{
    constexpr int LabelScale = 2;
    constexpr int LabelOutlinePx = 6;
    constexpr float TopPadding = 4.0f;
    constexpr float LabelLift = 16.0f;
    constexpr float HomeDoorLabelGap = 6.0f;
    const Vec2 textSize = renderer.measureText(facility.displayName, LabelScale);
    Vec2 pos{
        labelRect.pos.x + (labelRect.size.x - textSize.x) * 0.5f,
        labelRect.pos.y + TopPadding,
    };

    const std::string_view facilityId = facility.facilityId;
    if (area == BaseArea::Outdoor && facilityId == "home") {
        const UiRect doorRect = outdoorHomeDoorSpawnRect(facility.rect, ringWorkshopUnlocked);
        pos.x = doorRect.pos.x + (doorRect.size.x - textSize.x) * 0.5f;
        pos.y = doorRect.pos.y - textSize.y - HomeDoorLabelGap;
    } else if (area == BaseArea::Outdoor &&
        (facilityId == "storage_chest" || facilityId == "ring_workshop")) {
        pos.y -= LabelLift;
    }

    renderer.drawOutlinedText(
        pos,
        facility.displayName,
        {255, 255, 255, 255},
        {0, 0, 0, 170},
        LabelOutlinePx,
        LabelScale);
}

const BaseFacility* selectBaseInteractionFacility(
    Vec2 playerPosition,
    Vec2 playerFacing,
    BaseArea area,
    const std::vector<BaseFacility>& facilities)
{
    constexpr float DirectionalCandidateDot = 0.45f;
    constexpr float DirectionalTieEpsilon = 0.001f;

    const BaseFacility* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    const BaseFacility* directional = nullptr;
    float directionalDot = DirectionalCandidateDot;
    float directionalDistance = std::numeric_limits<float>::max();
    const bool hasFacing = lengthSquared(playerFacing) > 0.0001f;
    const Vec2 facing = hasFacing ? normalize(playerFacing) : Vec2{};

    for (const BaseFacility& facility : facilities) {
        if (baseFacilityHiddenInNormalView(area, facility)) {
            continue;
        }
        if (!baseInteractionAvailable(playerPosition, facility)) {
            continue;
        }

        const float dist = distanceToRect(playerPosition, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }

        if (!hasFacing) {
            continue;
        }

        Vec2 target = closestPointOnRect(playerPosition, facility.rect);
        Vec2 toFacility = target - playerPosition;
        if (lengthSquared(toFacility) <= 0.0001f) {
            target = facility.rect.pos + facility.rect.size * 0.5f;
            toFacility = target - playerPosition;
        }
        if (lengthSquared(toFacility) <= 0.0001f) {
            continue;
        }

        const float candidateDot = dot(facing, normalize(toFacility));
        if (candidateDot < DirectionalCandidateDot) {
            continue;
        }

        if (candidateDot > directionalDot + DirectionalTieEpsilon ||
            (std::abs(candidateDot - directionalDot) <= DirectionalTieEpsilon && dist < directionalDistance)) {
            directionalDot = candidateDot;
            directionalDistance = dist;
            directional = &facility;
        }
    }

    return directional != nullptr ? directional : nearest;
}

void drawBaseFacilityFallbackRect(
    Renderer& renderer,
    const BaseFacility& facility,
    bool inInteractionRange,
    bool hovered)
{
    Color fill = facility.enabled ? Color{96, 82, 82, 255} : Color{84, 62, 56, 255};
    if (!facility.unlocked) {
        fill = {58, 58, 64, 255};
    }
    Color outline = facility.enabled ? Color{220, 200, 150, 255} : Color{120, 108, 98, 255};
    if (inInteractionRange && facility.enabled) {
        outline = hovered ? Color{255, 230, 72, 255} : Color{255, 255, 255, 245};
        fill.a = std::max<unsigned char>(fill.a, 170);
    }
    renderer.fillRect(facility.rect.pos, facility.rect.size, fill);
    renderer.drawRect(facility.rect.pos, facility.rect.size, outline);
    if (!inInteractionRange || !facility.enabled) {
        renderer.drawText(
            facility.rect.pos + Vec2{8.0f, 8.0f},
            facility.displayName,
            facility.enabled ? Color{248, 238, 214, 255} : Color{154, 146, 138, 255},
            2);
    }
}

void drawBaseFacilities(
    Renderer& renderer,
    const std::vector<BaseFacility>& facilities,
    BaseArea area,
    bool ringWorkshopUnlocked,
    Vec2 playerPosition,
    Vec2 mouse)
{
    struct FacilityNameLabel {
        const BaseFacility* facility = nullptr;
        UiRect rect{};
    };
    std::vector<FacilityNameLabel> labels;

    for (int pass = 0; pass < 2; ++pass) {
        const bool drawEnabled = pass == 1;
        for (const BaseFacility& facility : facilities) {
            if (baseFacilityHiddenInNormalView(area, facility)) {
                continue;
            }
            if (facility.enabled != drawEnabled) {
                continue;
            }

            const bool inInteractionRange = baseInteractionAvailable(playerPosition, facility);
            const bool labelVisible = inInteractionRange && facility.enabled;
            const BaseFacilityVisual* visual = baseFacilityVisual(area, facility.facilityId);

            if (visual == nullptr) {
                const bool hovered = inInteractionRange && facility.enabled && facility.rect.contains(mouse);
                drawBaseFacilityFallbackRect(renderer, facility, inInteractionRange, hovered);
                if (labelVisible) {
                    labels.push_back({&facility, facility.rect});
                }
                continue;
            }

            const UiRect visualRect = baseFacilityVisualRect(facility, area, ringWorkshopUnlocked, *visual);
            const bool hovered = inInteractionRange &&
                facility.enabled &&
                baseFacilityVisualHitTest(renderer, *visual, visualRect, mouse);

            ImageDrawOptions options;
            options.outlineEnabled = inInteractionRange && facility.enabled;
            options.outlineColor = hovered ? Color{255, 230, 72, 255} : Color{255, 255, 255, 245};
            options.outlinePx = 1;
            if (!facility.unlocked) {
                options.tint = {190, 190, 198, 230};
            }
            if (!renderer.drawImage(
                    visual->imagePath,
                    visualRect.pos + visualRect.size * 0.5f,
                    visualRect.size,
                    options,
                    TextureFilter::Nearest)) {
                drawBaseFacilityFallbackRect(renderer, facility, inInteractionRange, hovered);
            }
            if (labelVisible) {
                labels.push_back({&facility, visualRect});
            }
        }
    }

    for (const FacilityNameLabel& label : labels) {
        if (label.facility != nullptr) {
            drawBaseFacilityNameLabel(renderer, *label.facility, label.rect, area, ringWorkshopUnlocked);
        }
    }
}

Vec2 baseFacilitySpawnPosition(UiRect facilityRect, BaseFacilitySpawnSide side, float playerRadius)
{
    Vec2 position{facilityRect.pos.x + facilityRect.size.x * 0.5f, facilityRect.pos.y};
    if (side == BaseFacilitySpawnSide::Above) {
        position.y = facilityRect.pos.y - playerRadius - BaseFacilitySpawnGap;
    } else {
        position.y = facilityRect.pos.y + facilityRect.size.y + playerRadius + BaseFacilitySpawnGap;
    }

    const UiRect bounds = baseMapBounds();
    position.x = std::clamp(
        position.x,
        bounds.pos.x + playerRadius,
        bounds.pos.x + bounds.size.x - playerRadius);
    position.y = std::clamp(
        position.y,
        bounds.pos.y + playerRadius,
        bounds.pos.y + bounds.size.y - playerRadius);
    return position;
}

Vec2 homeInteriorEntryPosition(UiRect homeExitRect, float playerRadius)
{
    Vec2 position = baseFacilitySpawnPosition(homeExitRect, BaseFacilitySpawnSide::Above, playerRadius);
    position.y -= static_cast<float>(balance::TileSize);
    return position;
}

UiRect merchantSellSourceRect(int index, int tabCount = BaseItemSourceCount)
{
    return baseItemSourceTabRect(index, 116.0f + MerchantSellSourceYOffset, tabCount);
}

float storageItemCircleLeftX()
{
    const UiRect first = merchantGridSlotRect(0);
    const float radius = std::min(first.size.x, first.size.y) * 0.5f;
    return first.pos.x + first.size.x * 0.5f - radius;
}

bool baseItemSourceIsWarehouse(int source)
{
    return source == BaseWarehouseSourceIndex;
}

bool baseItemSourceIsRing(int source)
{
    return source >= BaseRingSourceOffset && source < BaseItemSourceCount;
}

int ringIndexFromBaseItemSource(int source)
{
    return source - BaseRingSourceOffset;
}

int baseItemSourceCountForUnlockedRings(int unlockedRingCount)
{
    return BaseRingSourceOffset + std::clamp(unlockedRingCount, 1, SpellRingCount);
}

int storageDepositSourceCountForUnlockedRings(int unlockedRingCount)
{
    return 1 + std::clamp(unlockedRingCount, 1, SpellRingCount);
}

int clampBaseItemSourceForUnlockedRings(int source, int unlockedRingCount)
{
    const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount);
    return source >= 0 && source < sourceCount ? source : BaseBackpackSourceIndex;
}

int clampStorageDepositSourceForUnlockedRings(int source, int unlockedRingCount)
{
    if (source == BaseBackpackSourceIndex) {
        return source;
    }
    if (!baseItemSourceIsRing(source)) {
        return BaseBackpackSourceIndex;
    }
    const int ringIndex = ringIndexFromBaseItemSource(source);
    return ringIndex >= 0 && ringIndex < std::clamp(unlockedRingCount, 1, SpellRingCount)
        ? source
        : BaseBackpackSourceIndex;
}

int storageDepositSourceValue(int tabIndex)
{
    if (tabIndex <= 0) {
        return BaseBackpackSourceIndex;
    }
    return BaseRingSourceOffset + std::clamp(tabIndex - 1, 0, SpellRingCount - 1);
}

int storageDepositSourceTabIndex(int source)
{
    if (source == BaseBackpackSourceIndex) {
        return 0;
    }
    if (baseItemSourceIsRing(source)) {
        return 1 + ringIndexFromBaseItemSource(source);
    }
    return 0;
}

UiRect storageDepositSourceRect(int tabIndex)
{
    constexpr float StorageDepositSourceTabWidth = 180.0f;
    constexpr float StorageDepositSourceTabPitch = 194.0f;
    UiRect rect = merchantSellSourceRect(tabIndex);
    rect.pos.x = storageItemCircleLeftX() + static_cast<float>(tabIndex) * StorageDepositSourceTabPitch;
    rect.pos.y += StorageTransferLayoutYOffset;
    rect.size.x = StorageDepositSourceTabWidth;
    return rect;
}

UiRect storageTransferGridSlotRect(int index)
{
    UiRect rect = merchantGridSlotRect(index);
    rect.pos.y += MerchantSellItemYOffset + StorageTransferLayoutYOffset;
    return rect;
}

Vec2 storageTransferCountTextPos()
{
    return {storageItemCircleLeftX(), 116.0f + StorageTransferLayoutYOffset};
}

UiRect storageQuantityDialogRect()
{
    return {{430.0f, 130.0f}, {420.0f, 396.0f}};
}

UiRect storageTransferSortButtonRect()
{
    UiRect rect = uiBottomLeftButtonRect(merchantPanelRect(), {180.0f, ui::ButtonHeight});
    rect.pos.x = storageItemCircleLeftX();
    return rect;
}

UiRect storageWithdrawSlotRect(int index)
{
    UiRect rect = merchantGridSlotRect(index);
    const int row = index / StorageColumns;
    rect.pos.y = StorageWithdrawGridY + static_cast<float>(row) * (rect.size.y + StorageWithdrawRowGap);
    return rect;
}

Vec2 storageWithdrawCountTextPos()
{
    return storageTransferCountTextPos();
}

UiRect storageWithdrawSortButtonRect()
{
    UiRect rect = storageTransferSortButtonRect();
    const UiRect lastSlot = storageWithdrawSlotRect(StorageWithdrawSlotCount - 1);
    rect.pos.y = lastSlot.pos.y + lastSlot.size.y + StorageWithdrawSortButtonGap;
    return rect;
}

UiRect smallActionDialogRect()
{
    return {{410.0f, 170.0f}, {460.0f, 330.0f}};
}

UiRect smallActionChoiceRectForDialog(UiRect dialog, int index)
{
    constexpr float ChoiceGap = 16.0f;
    constexpr float ButtonHorizontalInset = 22.0f;
    const UiRect body = uiBodyRect(dialog);
    const float width = std::max(0.0f, body.size.x - ButtonHorizontalInset * 2.0f);
    return {
        {
            body.pos.x + (body.size.x - width) * 0.5f,
            body.pos.y + 20.0f + static_cast<float>(index) * (ui::ButtonHeight + ChoiceGap),
        },
        {width, ui::ButtonHeight},
    };
}

UiRect smallActionChoiceRect(int index)
{
    return smallActionChoiceRectForDialog(smallActionDialogRect(), index);
}

Vec2 smallActionInfoTextPos(UiRect panel)
{
    const UiRect body = uiBodyRect(panel);
    return body.pos + Vec2{8.0f, -18.0f};
}

UiRect storageActionDialogRect()
{
    UiRect rect = smallActionDialogRect();
    rect.size.y += 48.0f;
    return rect;
}

UiRect storageBulkDialogRect()
{
    UiRect rect = smallActionDialogRect();
    rect.size.y += 120.0f;
    return rect;
}

UiRect storageActionChoiceRect(int index)
{
    return smallActionChoiceRectForDialog(storageActionDialogRect(), index);
}

UiRect storageBulkChoiceRect(int index)
{
    return smallActionChoiceRectForDialog(storageBulkDialogRect(), index);
}

std::string formatDiaryPlayTime(std::int64_t seconds)
{
    const std::int64_t totalMinutes = std::max<std::int64_t>(0, seconds) / 60;
    const std::int64_t hours = totalMinutes / 60;
    const std::int64_t minutes = totalMinutes % 60;
    return std::to_string(hours) + "時間" + std::to_string(minutes) + "分";
}

UiRect merchantActionDialogRect()
{
    return smallActionDialogRect();
}

UiRect merchantActionChoiceRect(int index)
{
    return smallActionChoiceRect(index);
}

UiRect bookshelfMenuPanelRect()
{
    return merchantActionDialogRect();
}

UiRect bookshelfMenuChoiceRect(int index)
{
    return merchantActionChoiceRect(index);
}

InventoryUiGridStyle bookshelfGridStyle()
{
    InventoryUiGridStyle style;
    style.visibleRows = 5;
    style.scroll.wheelStep = style.slotSize.y + style.slotGap.y;
    style.scroll.scrollbarPaddingX = 2.0f;
    style.scroll.scrollbarPaddingY = 0.0f;
    return style;
}

UiRect bookshelfGridViewport()
{
    return inventoryUiGridViewport({72.0f, 142.0f}, bookshelfGridStyle());
}

const char* enemyMoveSpeedLabel(double speed)
{
    if (!std::isfinite(speed) || speed <= 20.0) {
        return "かなり遅い";
    }
    if (speed <= 35.0) {
        return "遅い";
    }
    if (speed <= 45.0) {
        return "やや遅い";
    }
    if (speed <= 55.0) {
        return "まあまあ";
    }
    if (speed <= 65.0) {
        return "やや速い";
    }
    if (speed <= 80.0) {
        return "速い";
    }
    return "かなり速い";
}

const char* enemyCaptureDifficultyLabel(int difficulty)
{
    if (difficulty <= 1) {
        return "超簡単";
    }
    if (difficulty == 2) {
        return "簡単";
    }
    if (difficulty == 3) {
        return "やや簡単";
    }
    if (difficulty == 4) {
        return "まあまあ";
    }
    if (difficulty == 5) {
        return "ややムズい";
    }
    if (difficulty == 6) {
        return "ムズい";
    }
    return "激ムズ";
}

std::string enemyContactAttackText(const EnemyDefinition& enemy)
{
    if (enemy.contactAttackPower <= 0) {
        return "-";
    }

    std::string text = std::to_string(enemy.contactAttackPower);
    const std::string damageType = normalizeDamageType(enemy.contactDamageType);
    if (!damageType.empty() && damageType != "none") {
        text += "（";
        text += damageTypeDisplayName(damageType);
        text += "ダメージ）";
    }
    return text;
}

constexpr int RingWorkshopActionCount = 2;
constexpr int RingWorkshopUpgradeFutureCount = 4;
constexpr int RingWorkshopUpgradeDisplayCount = RingWorkshopImplementedUpgradeCount + RingWorkshopUpgradeFutureCount;

UiRect homeInteriorMapRect()
{
    return {{290.0f, 100.0f}, {700.0f, 520.0f}};
}

UiRect homeInteriorWalkBounds()
{
    return {{354.0f, 182.0f}, {572.0f, 388.0f}};
}

void drawHomeInteriorBackdrop(Renderer& renderer)
{
    const UiRect room = homeInteriorMapRect();
    if (renderer.drawImage(
            "assets/kyoten/map_house.png",
            room.pos + room.size * 0.5f,
            room.size,
            ImageDrawOptions{},
            TextureFilter::Nearest)) {
        return;
    }

    renderer.fillRect(room.pos, room.size, {46, 36, 38, 255});
    renderer.drawRect(room.pos, room.size, {184, 150, 108, 255});
    renderer.fillRect(room.pos + Vec2{56.0f, 82.0f}, {568.0f, 376.0f}, {118, 92, 66, 255});
    renderer.drawText(room.pos + Vec2{294.0f, 42.0f}, "ルネの家", {246, 235, 255, 255}, 2);
}

UiRect ringWorkshopActionDialogRect()
{
    return smallActionDialogRect();
}

UiRect ringWorkshopActionChoiceRect(int index)
{
    return smallActionChoiceRect(index);
}

UiRect ringWorkshopPanelRect()
{
    return merchantPanelRect();
}

UiRect ringWorkshopDetailPanelRect()
{
    return merchantDetailPanelRect();
}

UiRect ringWorkshopRingTabRect(int index, int unlockedRingCount = SpellRingCount)
{
    constexpr float TabTop = 126.0f;
    constexpr float TabGap = 22.0f;
    const int ringCount = std::clamp(unlockedRingCount, 1, SpellRingCount);
    const UiRect panel = ringWorkshopPanelRect();
    const UiRect detail = ringWorkshopDetailPanelRect();
    const float left = panel.pos.x + 72.0f;
    const float right = detail.pos.x - 24.0f;
    const float totalGap = TabGap * static_cast<float>(std::max(0, ringCount - 1));
    const float width = std::max(1.0f, (right - left - totalGap) / static_cast<float>(ringCount));
    const float pitch = width + TabGap;
    return {{left + static_cast<float>(index) * pitch, TabTop}, {width, ui::ButtonHeight}};
}

UiRect ringWorkshopRespecPanelRect()
{
    const UiRect firstTab = ringWorkshopRingTabRect(0);
    const UiRect lastTab = ringWorkshopRingTabRect(SpellRingCount - 1);
    return {{
        firstTab.pos.x,
        firstTab.pos.y + firstTab.size.y + 30.0f,
    }, {
        lastTab.pos.x + lastTab.size.x - firstTab.pos.x,
        302.0f,
    }};
}

UiRect ringWorkshopRespecKindRect(int index)
{
    constexpr float TopGap = 58.0f;
    constexpr float RowGap = 16.0f;
    constexpr float RowHeight = 54.0f;
    const UiRect panel = ringWorkshopRespecPanelRect();
    return {{
        panel.pos.x + 24.0f,
        panel.pos.y + TopGap + static_cast<float>(index) * (RowHeight + RowGap),
    }, {
        panel.size.x - 48.0f,
        RowHeight,
    }};
}

UiRect ringWorkshopRespecConfirmRect()
{
    const UiRect panel = ringWorkshopPanelRect();
    const UiRect detail = ringWorkshopDetailPanelRect();
    const Vec2 size{220.0f, ui::ButtonHeight};
    const float leftAreaLeft = panel.pos.x + 72.0f;
    const float leftAreaRight = detail.pos.x - 24.0f;
    return {{
        leftAreaLeft + (leftAreaRight - leftAreaLeft - size.x) * 0.5f,
        panel.pos.y + panel.size.y - 74.0f,
    }, size};
}

UiRect ringWorkshopUpgradeListPanelRect()
{
    const UiRect panel = ringWorkshopPanelRect();
    const UiRect detail = ringWorkshopDetailPanelRect();
    return {{
        panel.pos.x + 72.0f,
        panel.pos.y + 96.0f,
    }, {
        detail.pos.x - panel.pos.x - 108.0f,
        420.0f,
    }};
}

UiRect ringWorkshopUpgradeItemRect(int index)
{
    constexpr float RowGap = 10.0f;
    constexpr float RowHeight = 42.0f;
    const UiRect panel = ringWorkshopUpgradeListPanelRect();
    return {{
        panel.pos.x + 24.0f,
        panel.pos.y + 58.0f + static_cast<float>(index) * (RowHeight + RowGap),
    }, {
        panel.size.x - 48.0f,
        RowHeight,
    }};
}

UiRect ringWorkshopUpgradeConfirmRect()
{
    const UiRect detail = ringWorkshopDetailPanelRect();
    const Vec2 size{208.0f, ui::ButtonHeight};
    return {{
        detail.pos.x + (detail.size.x - size.x) * 0.5f,
        ringWorkshopPanelRect().pos.y + ringWorkshopPanelRect().size.y - 74.0f,
    }, size};
}

RingLevelUpgradeKind ringWorkshopKindForIndex(int index)
{
    switch (index) {
    case 1:
        return RingLevelUpgradeKind::Speed;
    case 2:
        return RingLevelUpgradeKind::WeightLimit;
    case 0:
    default:
        return RingLevelUpgradeKind::Radius;
    }
}

const char* ringWorkshopActionLabel(int index)
{
    switch (index) {
    case 0: return "配分再調整";
    case 1: return "工房強化";
    default: return "";
    }
}

const char* ringWorkshopUpgradeShortName(int index)
{
    switch (index) {
    case 0: return "初期半径";
    case 1: return "初期速度";
    case 2: return "ずらし距離";
    case 3: return "投げ距離";
    case 4: return "投げ短縮";
    case 5: return "重量軽減";
    case 6: return "装着枠";
    default: return "未解禁";
    }
}

const char* ringWorkshopFutureUpgradeName(int index)
{
    switch (index) {
    case 3: return "リング投げ距離強化";
    case 4: return "リング投げクールダウン短縮";
    case 5: return "リング重量ペナルティ軽減";
    case 6: return "リング装着枠増加";
    default: return "未解禁項目";
    }
}

std::string formatRingWorkshopValue(RingLevelUpgradeKind kind, float value)
{
    char buffer[64];
    switch (kind) {
    case RingLevelUpgradeKind::Radius:
        std::snprintf(buffer, sizeof(buffer), "%.0fpx", value);
        break;
    case RingLevelUpgradeKind::Speed:
        std::snprintf(buffer, sizeof(buffer), "%.2f", value);
        break;
    case RingLevelUpgradeKind::WeightLimit:
        std::snprintf(buffer, sizeof(buffer), "%.1fkg", value);
        break;
    }
    return buffer;
}

UiRect baseBrokenRingDepartureConfirmRect()
{
    return {{410.0f, 230.0f}, {460.0f, 250.0f}};
}

Vec2 baseSystemMessagePos(
    UiRect panel,
    bool storageActive,
    bool merchantActive,
    bool processingActive,
    bool upgradeActive)
{
    if (upgradeActive) {
        return baseUpgradePanelRect().pos + Vec2{32.0f, 468.0f};
    }
    if (storageActive || merchantActive || processingActive) {
        return {80.0f, 500.0f};
    }
    return panel.pos + Vec2{54.0f, 454.0f};
}

void drawTextCentered(Renderer& renderer, UiRect rect, float y, std::string_view text, Color color, int scale)
{
    const Vec2 size = renderer.measureText(text, scale);
    renderer.drawText({rect.pos.x + (rect.size.x - size.x) * 0.5f, y}, text, color, scale);
}

void drawStorageHeader(Renderer& renderer, float x, float y, std::string_view title, std::string_view count, Color color)
{
    renderer.drawText({x, y}, title, color, 3);
    const Vec2 titleSize = renderer.measureText(title, 3);
    renderer.drawText(
        {x + titleSize.x + StorageHeaderCountGap, y + StorageHeaderCountYOffset},
        count,
        color,
        StorageHeaderCountScale);
}

void drawStoragePageSelector(Renderer& renderer, int page, int pageCount)
{
    char buffer[64];
    const UiRect prevPageRect = storagePrevPageButtonRect();
    const UiRect pageTextRect = storagePageTextRect();
    const UiRect nextPageRect = storageNextPageButtonRect();
    std::snprintf(buffer, sizeof(buffer), "%d/%d", page + 1, pageCount);
    drawTextCentered(renderer, pageTextRect, StorageBottomHeaderY + StoragePageTextYOffset, buffer, {198, 198, 206, 255}, StoragePageTextScale);
    drawUiRectButton(renderer, prevPageRect, "<", false);
    drawUiRectButton(renderer, nextPageRect, ">", false);
}

UiRect merchantSellGridSlotRect(int index)
{
    UiRect rect = merchantGridSlotRect(index);
    rect.pos.y += MerchantSellItemYOffset;
    return rect;
}

UiRect externalWarehouseSourceSlotRect(UiRect(*sourceSlotRect)(int), int index)
{
    UiRect rect = sourceSlotRect(index);
    rect.pos.y += ExternalWarehouseGridYOffset;
    return rect;
}

UiPageSelectorRects externalWarehousePageSelectorRects(UiRect(*sourceSlotRect)(int))
{
    const UiRect first = externalWarehouseSourceSlotRect(sourceSlotRect, 0);
    const UiRect last = externalWarehouseSourceSlotRect(sourceSlotRect, StorageColumns - 1);
    return uiPageSelectorRectsFromNextButton(
        {last.pos.x + last.size.x - StoragePageButtonSize, first.pos.y - StoragePageButtonSize - ExternalWarehousePageSelectorGap},
        StoragePageTextWidth);
}

void drawExternalWarehouseSourceHeader(
    Renderer& renderer,
    UiRect(*sourceSlotRect)(int),
    int page,
    int pageCount)
{
    const UiPageSelectorRects pageRects = externalWarehousePageSelectorRects(sourceSlotRect);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d/%d", page + 1, pageCount);
    drawTextCentered(renderer, pageRects.text, pageRects.text.pos.y + StoragePageTextYOffset, buffer, {198, 198, 206, 255}, StoragePageTextScale);
    drawUiRectButton(renderer, pageRects.prev, "<", false);
    drawUiRectButton(renderer, pageRects.next, ">", false);
}

UiPageSelectorRects storageWithdrawPageSelectorRects()
{
    const UiRect first = storageWithdrawSlotRect(0);
    const UiRect last = storageWithdrawSlotRect(StorageColumns - 1);
    return uiPageSelectorRectsFromNextButton(
        {last.pos.x + last.size.x - StoragePageButtonSize, first.pos.y - StoragePageButtonSize - ExternalWarehousePageSelectorGap},
        StoragePageTextWidth);
}

void drawStorageWithdrawHeader(Renderer& renderer, int page, int pageCount)
{
    const UiPageSelectorRects pageRects = storageWithdrawPageSelectorRects();
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d/%d", page + 1, pageCount);
    drawTextCentered(renderer, pageRects.text, pageRects.text.pos.y + StoragePageTextYOffset, buffer, {198, 198, 206, 255}, StoragePageTextScale);
    drawUiRectButton(renderer, pageRects.prev, "<", false);
    drawUiRectButton(renderer, pageRects.next, ">", false);
}

Vec2 baseRingPreviewCenterFromGrid(UiRect(*slotRect)(int), float yOffset)
{
    const UiRect first = slotRect(0);
    const UiRect last = slotRect(StoragePaneSlotCount - 1);
    return {
        first.pos.x + (slotRect(StorageColumns - 1).pos.x + first.size.x - first.pos.x) * 0.5f,
        first.pos.y + (last.pos.y + last.size.y - first.pos.y) * 0.5f + yOffset,
    };
}

Vec2 baseProcessingRingPreviewCenter()
{
    return baseRingPreviewCenterFromGrid(baseProcessingGridSlotRect, BaseProcessingRingYOffset);
}

Vec2 merchantSellRingPreviewCenter()
{
    return baseRingPreviewCenterFromGrid(merchantGridSlotRect, MerchantSellRingYOffset);
}

Vec2 storageRingPreviewCenter()
{
    return baseRingPreviewCenterFromGrid(storageTransferGridSlotRect, MerchantSellRingYOffset) + Vec2{0.0f, -60.0f};
}

Vec2 baseRingPreviewCenterForShape(Vec2 center, RingShape shape)
{
    if (shape == RingShape::Comet) {
        constexpr float CometPreviewYOffset = 120.0f;
        center.y += CometPreviewYOffset;
    }
    return center;
}

Vec2 baseProcessingRingPreviewCenter(const SpellRingSystem& spellRing, int ringIndex)
{
    return baseRingPreviewCenterForShape(
        baseProcessingRingPreviewCenter(),
        spellRing.ringShapeForIndex(ringIndex));
}

Vec2 merchantSellRingPreviewCenter(const SpellRingSystem& spellRing, int ringIndex)
{
    return baseRingPreviewCenterForShape(
        merchantSellRingPreviewCenter(),
        spellRing.ringShapeForIndex(ringIndex));
}

Vec2 storageRingPreviewCenter(const SpellRingSystem& spellRing, int ringIndex)
{
    return baseRingPreviewCenterForShape(
        storageRingPreviewCenter(),
        spellRing.ringShapeForIndex(ringIndex));
}

float baseRingPreviewRadius(RingShape shape, float previewScale)
{
    return ringUiShapeRadius(shape) * previewScale;
}

Vec2 rotateAround(Vec2 point, Vec2 center, float radians)
{
    const Vec2 local = point - center;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return center + Vec2{
        local.x * c - local.y * s,
        local.x * s + local.y * c,
    };
}

RingOrbitContext baseRingPreviewOrbitContext(
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float previewScale)
{
    RingOrbitContext context;
    context.shape = spellRing.ringShapeForIndex(ringIndex);
    context.radius = baseRingPreviewRadius(context.shape, previewScale);
    context.shapeRotation = 0.0f;
    context.itemIndex = std::max(0, itemIndex);
    context.itemCount = std::max(1, itemCount);
    context.tuning = makeRingOrbitTuning(balance);
    return context;
}

Vec2 baseRingPreviewPoint(Vec2 center, RingShape shape, Vec2 point)
{
    if (shape == RingShape::Comet) {
        return rotateAround(point, center, RingUiCometArcRotation);
    }
    return point;
}

Vec2 baseRingPreviewItemAnchor(
    Vec2 center,
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float previewScale)
{
    const RingOrbitContext context = baseRingPreviewOrbitContext(spellRing, balance, ringIndex, itemIndex, itemCount, previewScale);
    const Vec2 point = getRingItemWorldPosition(center, item.localAngle, context);
    return baseRingPreviewPoint(center, context.shape, point);
}

Vec2 baseRingPreviewItemDrawCenter(
    Vec2 center,
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float previewScale,
    float totalSeconds)
{
    SpellRingItem displayItem = item;
    displayItem.worldPosition = baseRingPreviewItemAnchor(center, item, spellRing, balance, ringIndex, itemIndex, itemCount, previewScale);
    return ringItemDrawPosition(displayItem, totalSeconds);
}

UiRect baseRingPreviewItemRect(
    Vec2 previewCenter,
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float previewScale,
    float totalSeconds)
{
    constexpr Vec2 Size{54.0f, 54.0f};
    const Vec2 center = baseRingPreviewItemDrawCenter(previewCenter, item, spellRing, balance, ringIndex, itemIndex, itemCount, previewScale, totalSeconds);
    return {center - Size * 0.5f, Size};
}

UiRect baseProcessingRingItemRect(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float totalSeconds)
{
    return baseRingPreviewItemRect(
        baseProcessingRingPreviewCenter(spellRing, ringIndex),
        item,
        spellRing,
        balance,
        ringIndex,
        itemIndex,
        itemCount,
        BaseRingPreviewScale,
        totalSeconds);
}

UiRect merchantSellRingItemRect(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float totalSeconds)
{
    return baseRingPreviewItemRect(
        merchantSellRingPreviewCenter(spellRing, ringIndex),
        item,
        spellRing,
        balance,
        ringIndex,
        itemIndex,
        itemCount,
        StorageRingPreviewScale,
        totalSeconds);
}

UiRect storageRingItemRect(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const RuntimeBalance& balance,
    int ringIndex,
    int itemIndex,
    int itemCount,
    float totalSeconds)
{
    return baseRingPreviewItemRect(
        storageRingPreviewCenter(spellRing, ringIndex),
        item,
        spellRing,
        balance,
        ringIndex,
        itemIndex,
        itemCount,
        MerchantSellRingPreviewScale,
        totalSeconds);
}

void drawBaseRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const RuntimeBalance& balance,
    Vec2 center,
    int ringIndex,
    int selectedIndex,
    float previewScale,
    float totalSeconds)
{
    const std::vector<SpellRingItem>& items = spellRing.itemsForRing(ringIndex);
    const RingShape shape = spellRing.ringShapeForIndex(ringIndex);
    RingOrbitContext context = baseRingPreviewOrbitContext(spellRing, balance, ringIndex, 0, static_cast<int>(items.size()), previewScale);
    std::vector<Vec2> orbitPath = getRingPathSamplePoints(center, context, 160);
    for (Vec2& point : orbitPath) {
        point = baseRingPreviewPoint(center, shape, point);
    }
    drawMagicOrbitPath(
        renderer,
        orbitPath,
        center,
        MagicOrbitDrawOptions{
            shape,
            true,
            false,
            true,
            true,
            ringIndex,
            totalSeconds,
            0.92f,
        });

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const SpellRingItem& item = items[static_cast<std::size_t>(i)];
        const Vec2 itemAnchor = baseRingPreviewItemAnchor(center, item, spellRing, balance, ringIndex, i, static_cast<int>(items.size()), previewScale);
        const Vec2 itemCenter = baseRingPreviewItemDrawCenter(center, item, spellRing, balance, ringIndex, i, static_cast<int>(items.size()), previewScale, totalSeconds);
        Vec2 outward = normalize(itemAnchor - center);
        if (lengthSquared(outward) <= 0.0001f) {
            outward = {0.0f, -1.0f};
        }
        Vec2 forward{-outward.y, outward.x};
        if (lengthSquared(forward) <= 0.0001f) {
            forward = {1.0f, 0.0f};
        }
        const bool selected = i == selectedIndex;
        const ItemData* object = objectForRingItem(objectCatalog, item);
        if (shape != RingShape::FigureEight) {
            const Color angleLineColor = selected ? Color{255, 230, 150, 120} : Color{94, 102, 128, 85};
            Vec2 tangent = normalize(Vec2{-outward.y, outward.x});
            if (lengthSquared(tangent) <= 0.0001f) {
                tangent = {0.0f, 1.0f};
            }
            constexpr float AngleLineHalfWidthPx = 0.5f;
            renderer.drawLine(center + tangent * AngleLineHalfWidthPx, itemAnchor + tangent * AngleLineHalfWidthPx, angleLineColor);
            renderer.drawLine(center - tangent * AngleLineHalfWidthPx, itemAnchor - tangent * AngleLineHalfWidthPx, angleLineColor);
        }
        drawRingItemShape(renderer, item, object, itemCenter, outward, forward, totalSeconds, selected);
        char label[16];
        std::snprintf(label, sizeof(label), "%d", i + 1);
        renderer.drawText(itemCenter + Vec2{-5.0f, 22.0f}, label, selected ? Color{255, 230, 150, 255} : Color{174, 182, 198, 255}, 1);
    }
}

void drawBaseProcessingRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const RuntimeBalance& balance,
    int ringIndex,
    int selectedIndex,
    float totalSeconds)
{
    drawBaseRingPreview(
        renderer,
        spellRing,
        objectCatalog,
        balance,
        baseProcessingRingPreviewCenter(spellRing, ringIndex),
        ringIndex,
        selectedIndex,
        BaseRingPreviewScale,
        totalSeconds);
}

void drawMerchantSellRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const RuntimeBalance& balance,
    int ringIndex,
    int selectedIndex,
    float totalSeconds)
{
    drawBaseRingPreview(
        renderer,
        spellRing,
        objectCatalog,
        balance,
        merchantSellRingPreviewCenter(spellRing, ringIndex),
        ringIndex,
        selectedIndex,
        MerchantSellRingPreviewScale,
        totalSeconds);
}

void drawStorageRingPreview(
    Renderer& renderer,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const RuntimeBalance& balance,
    int ringIndex,
    int selectedIndex,
    float totalSeconds)
{
    drawBaseRingPreview(
        renderer,
        spellRing,
        objectCatalog,
        balance,
        storageRingPreviewCenter(spellRing, ringIndex),
        ringIndex,
        selectedIndex,
        StorageRingPreviewScale,
        totalSeconds);
}

struct ProcessingResultSnapshot {
    std::string name;
    std::string objectId;
    int stackCount = 1;
    bool stackSource = false;
    bool isBroken = false;
    int currentDurability = -1;
    int maxDurability = -1;
    int enhanceLevel = 0;
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    double weightModifier = 1.0;
    double sizeModifier = 1.0;
};

std::string nonEmptyItemName(std::string_view name)
{
    return name.empty() ? std::string{"アイテム"} : std::string{name};
}

constexpr Color ConfirmAfterValueColor{255, 230, 150, 255};
constexpr Color RequirementShortageColor{238, 82, 82, 255};

struct RequirementRow {
    std::string label;
    std::string required;
    std::string owned;
    bool enough = true;
};

void drawUiTextRun(Renderer& renderer, Vec2& pos, std::string_view text, Color color, int scale = 2)
{
    renderer.drawText(pos, text, color, scale);
    pos.x += renderer.measureText(text, scale).x;
}

void drawUiInlineRun(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    Vec2& pos,
    std::string_view text,
    Color color = ui::Text,
    int scale = 2)
{
    InlineItemTextStyle style{};
    style.text = color;
    style.scale = scale;
    drawInlineItemText(renderer, catalog, pos, text, style);
    pos.x += measureInlineItemText(renderer, text, style).x;
}

RequirementRow moneyRequirementRow(int required, int owned)
{
    return RequirementRow{
        inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + " お金",
        std::to_string(required) + "G",
        std::to_string(owned) + "G",
        owned >= required,
    };
}

RequirementRow materialRequirementRow(MaterialType type, int required, int owned)
{
    return RequirementRow{
        inlineMaterialIconTag(type) + std::string(materialTypeDisplayName(type)),
        "×" + std::to_string(required),
        "×" + std::to_string(owned),
        owned >= required,
    };
}

void drawRequirementValue(Renderer& renderer, Vec2 pos, const RequirementRow& row)
{
    const Color valueColor = row.enough ? ui::Text : RequirementShortageColor;
    drawUiTextRun(renderer, pos, row.required, valueColor);
    drawUiTextRun(renderer, pos, "（", ui::TextMuted);
    drawUiTextRun(renderer, pos, row.owned, valueColor);
    drawUiTextRun(renderer, pos, "）", ui::TextMuted);
}

void drawRequirementRows(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    UiRect content,
    const std::vector<RequirementRow>& rows)
{
    constexpr float LabelWidth = 140.0f;
    constexpr float LineHeight = 31.0f;
    float y = content.pos.y;
    if (rows.empty()) {
        renderer.drawText({content.pos.x, y}, "なし", ui::TextMuted, 2);
        return;
    }
    for (const RequirementRow& row : rows) {
        Vec2 labelPos{content.pos.x, y};
        drawUiInlineRun(renderer, catalog, labelPos, row.label, ui::Text);
        drawRequirementValue(renderer, {content.pos.x + LabelWidth, y}, row);
        y += LineHeight;
    }
}

void drawRequirementSubWindow(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    UiRect panel,
    const std::vector<RequirementRow>& rows)
{
    drawUiSubPanel(renderer, panel);
    UiRect content = uiSubPanelContentRect(panel);
    const float topPadding = ui::SubPanelPadding.y;
    content.pos.y = panel.pos.y + topPadding;
    content.size.y = std::max(0.0f, panel.size.y - topPadding - ui::SubPanelPadding.y);
    renderer.drawText(content.pos, "必要素材", ui::TextMuted, 2);
    content.pos.y += 34.0f;
    content.size.y = std::max(0.0f, content.size.y - 34.0f);
    drawRequirementRows(renderer, catalog, content, rows);
}

ProcessingResultSnapshot processingSnapshotFromStack(const InventoryObjectStack& stack)
{
    ProcessingResultSnapshot snapshot{};
    snapshot.objectId = stack.objectId;
    snapshot.stackCount = stack.count;
    snapshot.stackSource = true;
    snapshot.currentDurability = stack.item.durability;
    snapshot.maxDurability = stack.item.durability;
    snapshot.isBroken = stack.item.durability == 0;
    snapshot.name = nonEmptyItemName(itemDisplayName(stack.item.name, snapshot.isBroken));
    return snapshot;
}

ProcessingResultSnapshot processingSnapshotFromInstance(const InventoryObjectInstance& entry)
{
    ProcessingResultSnapshot snapshot{};
    snapshot.objectId = entry.instance.objectId.empty() ? entry.item.id : entry.instance.objectId;
    snapshot.stackCount = 1;
    snapshot.currentDurability = entry.instance.currentDurability;
    snapshot.maxDurability = entry.instance.maxDurability;
    snapshot.isBroken = entry.instance.isBroken;
    snapshot.name = nonEmptyItemName(itemDisplayName(entry.item.name, snapshot.isBroken));
    snapshot.enhanceLevel = entry.instance.enhanceLevel;
    snapshot.attackBonus = entry.instance.attackBonus;
    snapshot.digBonus = entry.instance.digBonus;
    snapshot.durabilityBonus = entry.instance.durabilityBonus;
    snapshot.weightModifier = entry.instance.weightModifier;
    snapshot.sizeModifier = entry.instance.sizeModifier;
    return snapshot;
}

ProcessingResultSnapshot processingSnapshotFromRingItem(const ObjectCatalog& catalog, const SpellRingItem& item)
{
    ProcessingResultSnapshot snapshot{};
    snapshot.objectId = item.objectId;
    snapshot.name = nonEmptyItemName(ringItemDisplayName(catalog, item));
    snapshot.stackCount = 1;
    snapshot.currentDurability = item.durability;
    snapshot.maxDurability = item.maxDurability;
    snapshot.isBroken = item.broken();
    snapshot.enhanceLevel = item.enhanceLevel;
    snapshot.attackBonus = item.attackBonus;
    snapshot.digBonus = item.digBonus;
    snapshot.durabilityBonus = item.durabilityBonus;
    snapshot.weightModifier = item.weightModifier;
    snapshot.sizeModifier = item.sizeModifier;
    return snapshot;
}

ProcessingResultSnapshot processingEnhancedSnapshot(
    ProcessingResultSnapshot snapshot,
    int attackBonus,
    int digBonus,
    int durabilityBonus)
{
    snapshot.stackCount = 1;
    snapshot.stackSource = false;
    snapshot.enhanceLevel = std::min(MaxItemEnhanceLevel, snapshot.enhanceLevel + 1);
    snapshot.attackBonus += attackBonus;
    snapshot.digBonus += digBonus;
    snapshot.durabilityBonus += durabilityBonus;
    if (durabilityBonus > 0 && snapshot.maxDurability >= 0) {
        snapshot.maxDurability += durabilityBonus;
        snapshot.currentDurability = std::min(snapshot.maxDurability, std::max(0, snapshot.currentDurability + durabilityBonus));
    }
    return snapshot;
}

ProcessingResultSnapshot processingResetSnapshot(ProcessingResultSnapshot snapshot)
{
    snapshot.enhanceLevel = 0;
    snapshot.attackBonus = 0;
    snapshot.digBonus = 0;
    snapshot.durabilityBonus = 0;
    return snapshot;
}

ProcessingResultSnapshot processingShapeSnapshot(ProcessingResultSnapshot snapshot, double weightMultiplier, double sizeMultiplier)
{
    snapshot.stackCount = 1;
    snapshot.stackSource = false;
    snapshot.weightModifier = std::clamp(snapshot.weightModifier * weightMultiplier, 0.25, 4.0);
    snapshot.sizeModifier = std::clamp(snapshot.sizeModifier * sizeMultiplier, 0.50, 3.0);
    return snapshot;
}

std::string processingChangeLine(std::string_view label, int beforeValue, int afterValue)
{
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "%s: %d → %d", std::string(label).c_str(), beforeValue, afterValue);
    return buffer;
}

std::string processingSignedChangeLine(std::string_view label, int beforeValue, int afterValue)
{
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "%s: +%d → +%d", std::string(label).c_str(), beforeValue, afterValue);
    return buffer;
}

std::string processingDurabilityChangeLine(std::string_view label, int beforeValue, int afterValue)
{
    if (beforeValue < 0 || afterValue < 0) {
        return std::string(label) + ": 壊れない";
    }
    return processingChangeLine(label, beforeValue, afterValue);
}

std::string processingPercentChangeLine(std::string_view label, double beforeValue, double afterValue)
{
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "%s: %.0f%% → %.0f%%",
        std::string(label).c_str(),
        beforeValue * 100.0,
        afterValue * 100.0);
    return buffer;
}

struct ProcessingPreviewRow {
    std::string label;
    std::string beforeValue;
    std::string afterValue;
};

std::string formatProcessingInt(int value)
{
    return std::to_string(value);
}

std::string formatProcessingDurability(int current, int maximum)
{
    if (maximum < 0) {
        return "壊れない";
    }
    return std::to_string(std::max(0, current)) + "/" + std::to_string(maximum);
}

std::string formatProcessingMaxDurability(int maximum)
{
    if (maximum < 0) {
        return "壊れない";
    }
    return std::to_string(maximum);
}

std::string formatProcessingPercent(double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", value * 100.0);
    return buffer;
}

std::string processingInlineItemName(const ProcessingResultSnapshot& snapshot)
{
    std::string text = inlineItemTag(snapshot.objectId);
    if (!text.empty()) {
        text += " ";
    }
    text += snapshot.name;
    return text;
}

void drawProcessingPreviewRow(Renderer& renderer, UiRect content, float& y, const ProcessingPreviewRow& row)
{
    constexpr float ValueX = 184.0f;
    renderer.drawText({content.pos.x, y}, row.label, ui::TextMuted, 2);
    Vec2 valuePos{content.pos.x + ValueX, y};
    drawUiTextRun(renderer, valuePos, row.beforeValue, ui::Text);
    drawUiTextRun(renderer, valuePos, "→", ui::TextMuted);
    drawUiTextRun(renderer, valuePos, row.afterValue, ConfirmAfterValueColor);
    y += 31.0f;
}

std::string processingRepairDurabilityLine(const ProcessingResultSnapshot& before, const ProcessingResultSnapshot& after)
{
    if (before.maxDurability < 0 || after.maxDurability < 0) {
        return "耐久力: 壊れない";
    }
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "耐久力: %d/%d → %d/%d",
        before.currentDurability,
        before.maxDurability,
        after.currentDurability,
        after.maxDurability);
    return buffer;
}

std::vector<std::string> processingRepairResultLines(
    const ProcessingResultSnapshot& before,
    const ProcessingResultSnapshot& after)
{
    std::vector<std::string> lines;
    lines.push_back(before.name + "を修理しました");
    if (before.isBroken && !after.isBroken) {
        lines.push_back("状態: 破損 → 通常");
    }
    lines.push_back(processingRepairDurabilityLine(before, after));
    return lines;
}

std::vector<std::string> processingEnhanceResultLines(
    const ProcessingResultSnapshot& before,
    const ProcessingResultSnapshot& after,
    bool attackMode,
    bool digMode,
    bool durabilityMode)
{
    std::vector<std::string> lines;
    if (before.stackSource && before.stackCount > 1) {
        lines.push_back(before.name + "1個を強化しました");
    } else {
        lines.push_back(before.name + "を強化しました");
    }
    lines.push_back(processingChangeLine("強化Lv", before.enhanceLevel, after.enhanceLevel));
    if (attackMode) {
        lines.push_back(processingSignedChangeLine("攻撃力", before.attackBonus, after.attackBonus));
    } else if (digMode) {
        lines.push_back(processingSignedChangeLine("抑制力", before.digBonus, after.digBonus));
    } else if (durabilityMode) {
        lines.push_back(processingDurabilityChangeLine("最大耐久力", before.maxDurability, after.maxDurability));
    }
    return lines;
}

std::vector<std::string> processingResetResultLines(
    const ProcessingResultSnapshot& before,
    const ProcessingResultSnapshot& after)
{
    std::vector<std::string> lines;
    lines.push_back(before.name + "の強化をリセットしました");
    lines.push_back(processingChangeLine("強化Lv", before.enhanceLevel, after.enhanceLevel));
    lines.push_back(processingSignedChangeLine("攻撃力", before.attackBonus, after.attackBonus));
    lines.push_back(processingSignedChangeLine("抑制力", before.digBonus, after.digBonus));
    lines.push_back(processingSignedChangeLine("耐久補正", before.durabilityBonus, after.durabilityBonus));
    return lines;
}

std::vector<std::string> processingShapeResultLines(
    const ProcessingResultSnapshot& before,
    const ProcessingResultSnapshot& after,
    bool lightMode)
{
    std::vector<std::string> lines;
    const char* verb = lightMode ? "軽量化しました" : "大型化しました";
    if (before.stackSource && before.stackCount > 1) {
        lines.push_back(before.name + "1個を" + verb);
    } else {
        lines.push_back(before.name + "を" + verb);
    }
    lines.push_back(processingPercentChangeLine("重量", before.weightModifier, after.weightModifier));
    lines.push_back(processingPercentChangeLine("大きさ", before.sizeModifier, after.sizeModifier));
    return lines;
}

} // namespace

bool Game::isSellableObject(const ItemData& item) const
{
    return !isStoryObject(item);
}

bool Game::isStoryObject(const ItemData& item) const
{
    return isImportantItem(item);
}

namespace {

constexpr double LightenWeightMultiplier = 0.85;
constexpr double EnlargeWeightMultiplier = 1.15;
constexpr double EnlargeSizeMultiplier = 1.18;

bool isTreasureObject(const ItemData& item)
{
    return item.category == "\xE5\xAE\x9D";
}

double itemInstanceSellValueMultiplier(
    int currentDurability,
    int maxDurability,
    int enhanceLevel,
    int attackBonus,
    int digBonus,
    int durabilityBonus,
    double weightModifier,
    double sizeModifier,
    bool broken)
{
    double multiplier = 1.0;
    multiplier += static_cast<double>(std::max(0, enhanceLevel)) * 0.10;
    multiplier += static_cast<double>(std::max(0, attackBonus) + std::max(0, digBonus)) * 0.035;
    multiplier += static_cast<double>(std::max(0, durabilityBonus)) * 0.018;

    if (weightModifier < 0.999) {
        multiplier += std::min(0.35, (1.0 - weightModifier) * 1.5);
    }
    if (sizeModifier > 1.001) {
        multiplier += std::min(0.35, (sizeModifier - 1.0) * 1.2);
    }

    if (broken || currentDurability == 0) {
        multiplier *= 0.45;
    } else if (maxDurability > 0 && currentDurability >= 0) {
        const double durabilityRatio = std::clamp(
            static_cast<double>(currentDurability) / static_cast<double>(maxDurability),
            0.0,
            1.0);
        multiplier *= 0.75 + durabilityRatio * 0.25;
    }

    return std::max(0.1, multiplier);
}

} // namespace

int Game::sellPrice(const ItemData& item, const ItemInstance* instance) const
{
    double multiplier = 1.0;
    if (merchantUpgradeLevel_ >= 6) {
        multiplier = 1.2;
    } else if (merchantUpgradeLevel_ >= 3) {
        multiplier = 1.1;
    }
    if (isHighValueBuyObject(item)) {
        multiplier *= merchantUpgradeLevel_ >= 6 ? 1.8 : 1.5;
    }
    if (instance != nullptr) {
        multiplier *= itemInstanceSellValueMultiplier(
            instance->currentDurability,
            instance->maxDurability,
            instance->enhanceLevel,
            instance->attackBonus,
            instance->digBonus,
            instance->durabilityBonus,
            instance->weightModifier,
            instance->sizeModifier,
            instance->isBroken);
    }
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(item.price) * multiplier)));
}

int Game::sellPrice(const ItemData& item, const SpellRingItem* ringItem) const
{
    if (ringItem == nullptr) {
        return sellPrice(item, static_cast<const ItemInstance*>(nullptr));
    }
    double multiplier = 1.0;
    if (merchantUpgradeLevel_ >= 6) {
        multiplier = 1.2;
    } else if (merchantUpgradeLevel_ >= 3) {
        multiplier = 1.1;
    }
    if (isHighValueBuyObject(item)) {
        multiplier *= merchantUpgradeLevel_ >= 6 ? 1.8 : 1.5;
    }
    multiplier *= itemInstanceSellValueMultiplier(
        ringItem->durability,
        ringItem->maxDurability,
        ringItem->enhanceLevel,
        ringItem->attackBonus,
        ringItem->digBonus,
        ringItem->durabilityBonus,
        ringItem->weightModifier,
        ringItem->sizeModifier,
        ringItem->broken());
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(item.price) * multiplier)));
}

bool Game::isHighValueBuyObject(const ItemData& item) const
{
    if (merchantUpgradeLevel_ < 4 || !isTreasureObject(item)) {
        return false;
    }
    return std::find(highValueBuyObjectIds_.begin(), highValueBuyObjectIds_.end(), item.id) != highValueBuyObjectIds_.end();
}

bool Game::merchantProductCanFit(const ItemData* item) const
{
    if (item == nullptr) {
        return false;
    }
    const auto& stacks = inventory_.objectStacks();
    const bool existingStack = std::any_of(stacks.begin(), stacks.end(), [&](const InventoryObjectStack& stack) {
        return stack.objectId == item->id;
    });
    return existingStack || backpackUsedSlots() < inventory_.screenSlotCount();
}

bool Game::canBuyMerchantProduct(const MerchantProduct& product) const
{
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    return product.quantity > 0 && item != nullptr && money_ >= product.price && merchantProductCanFit(item);
}

void Game::refreshHighValueBuyObjects(bool force)
{
    if (merchantUpgradeLevel_ < 4) {
        highValueBuyObjectIds_.clear();
        return;
    }
    if (!force && !highValueBuyObjectIds_.empty()) {
        return;
    }

    std::vector<const ItemData*> candidates;
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const ItemData* item = objectCatalog_.registry.findById(object.id);
        if (item == nullptr || item->id.empty() || item->price <= 0 || !isTreasureObject(*item) || !isSellableObject(*item)) {
            continue;
        }
        candidates.push_back(item);
    }

    highValueBuyObjectIds_.clear();
    if (candidates.empty()) {
        return;
    }

    std::mt19937& rng = lootRuntimeRng();
    std::shuffle(candidates.begin(), candidates.end(), rng);
    const int pickCount = std::min(4, static_cast<int>(candidates.size()));
    highValueBuyObjectIds_.reserve(static_cast<std::size_t>(pickCount));
    for (int i = 0; i < pickCount; ++i) {
        highValueBuyObjectIds_.push_back(candidates[static_cast<std::size_t>(i)]->id);
    }
}

std::vector<Game::SellableEntry> Game::sellableObjects() const
{
    std::vector<SellableEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(i)];
        if (stack.count <= 0) {
            continue;
        }
        SellableEntry entry{SellableKind::Stack, i};
        entry.price = sellPrice(stack.item);
        entry.sellable = true;
        entries.push_back(std::move(entry));
    }
    const auto& instances = inventory_.objectInstances();
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(i)];
        SellableEntry entry{SellableKind::Instance, i};
        entry.price = sellPrice(instance.item, &instance.instance);
        entry.sellable = !inventory_.isStaffEquipped(instance.instance.instanceId) &&
            !instance.instance.protectionEnabled;
        if (inventory_.isStaffEquipped(instance.instance.instanceId)) {
            entry.blockedReason = "装備中";
        } else if (!entry.sellable) {
            entry.blockedReason = "保護中";
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

void Game::refreshMerchantStock(bool force)
{
    refreshHighValueBuyObjects(force);
    if (!force && !merchantStock_.empty()) {
        return;
    }

    std::vector<const ItemData*> candidates;
    const int maxRarity = merchantUpgradeLevel_ >= 7 ? 10 :
        (merchantUpgradeLevel_ >= 5 ? 7 :
            (merchantUpgradeLevel_ >= 4 ? 5 :
                (merchantUpgradeLevel_ >= 2 ? 4 : 2)));
    for (const ObjectDefinition& object : objectCatalog_.objects) {
        const ItemData* item = objectCatalog_.registry.findById(object.id);
        if (item == nullptr || item->id.empty() || item->price <= 0 || isStoryObject(*item) || isTreasureObject(*item)) {
            continue;
        }
        if (item->rarity > maxRarity) {
            continue;
        }
        const bool basicCategory =
            item->category == "\xE5\x9B\x9E\xE5\xBE\xA9" ||
            item->category == "\xE5\xBC\xB1\xE4\xBD\x93" ||
            item->category == "\xE6\x8E\xA2\xE7\xB4\xA2" ||
            item->category == "\xE5\xBC\xB7\xE5\x8C\x96";
        const bool equipmentCategory =
            item->category == "\xE6\x8E\x98\xE5\x89\x8A" ||
            item->category == "\xE6\xAD\xA6\xE5\x99\xA8" ||
            item->category == "\xE7\x9B\xBE" ||
            item->category == "\xE9\xAD\x94\xE5\xB0\x8E\xE6\x9B\xB8";
        const bool basicTag = std::any_of(item->tags.begin(), item->tags.end(), [](const std::string& tag) {
            return tag == "consumable" || tag == "potion" || tag == "food";
        });
        if (basicCategory || basicTag || (merchantUpgradeLevel_ >= 4 && equipmentCategory)) {
            candidates.push_back(item);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const ItemData* left, const ItemData* right) {
        if (left->price != right->price) {
            return left->price < right->price;
        }
        return left->id < right->id;
    });

    merchantStock_.clear();
    if (candidates.empty()) {
        return;
    }

    ++merchantStockVersion_;
    const int stockCount = std::min(3 + std::clamp(merchantUpgradeLevel_, 1, 7), static_cast<int>(candidates.size()));
    std::mt19937& rng = lootRuntimeRng();
    std::uniform_int_distribution<int> quantityDistribution(1, 5);
    for (int i = 0; i < stockCount; ++i) {
        std::vector<double> weights;
        weights.reserve(candidates.size());
        for (const ItemData* candidate : candidates) {
            const double commonWeight = static_cast<double>(std::max(1, 12 - std::clamp(candidate->rarity, 1, 10)));
            const double rareWeight = merchantUpgradeLevel_ >= 7
                ? static_cast<double>(std::clamp(candidate->rarity, 1, 10)) * 1.4
                : (merchantUpgradeLevel_ >= 5 ? static_cast<double>(std::clamp(candidate->rarity, 1, 10)) * 0.65 : 0.0);
            weights.push_back(commonWeight + rareWeight);
        }
        std::discrete_distribution<int> distribution(weights.begin(), weights.end());
        const int pickedIndex = distribution(rng);
        const ItemData* item = candidates[static_cast<std::size_t>(pickedIndex)];
        const int quantity = item->rarity >= 6 ? std::uniform_int_distribution<int>(1, 2)(rng) : quantityDistribution(rng);
        merchantStock_.push_back(MerchantProduct{item->id, std::max(1, item->price), quantity});
        candidates.erase(candidates.begin() + pickedIndex);
    }
}

void Game::sellMerchantEntry(int index, int count)
{
    const std::vector<SellableEntry> sellable = sellableObjects();
    if (index < 0 || index >= static_cast<int>(sellable.size())) {
        baseStatus_ = "売却対象がありません";
        return;
    }

    const SellableEntry entry = sellable[static_cast<std::size_t>(index)];
    if (!entry.sellable) {
        baseStatus_ = entry.blockedReason.empty() ? "売れません" : entry.blockedReason;
        return;
    }

    bool sold = false;
    int soldCount = 1;
    if (entry.kind == SellableKind::Stack) {
        const auto& stacks = inventory_.objectStacks();
        if (entry.index < 0 || entry.index >= static_cast<int>(stacks.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectStack& stack = stacks[static_cast<std::size_t>(entry.index)];
        soldCount = count <= 0 ? stack.count : std::min(count, stack.count);
        sold = inventory_.removeObjectItemCount(stack.objectId, soldCount);
    } else {
        const auto& instances = inventory_.objectInstances();
        if (entry.index < 0 || entry.index >= static_cast<int>(instances.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectInstance& instance = instances[static_cast<std::size_t>(entry.index)];
        sold = inventory_.removeObjectInstance(instance.instance.instanceId);
    }

    if (sold) {
        money_ += entry.price * std::max(1, soldCount);
        baseStatus_ = "売却しました";
        baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, static_cast<int>(sellableObjects().size()) - 1));
    }
}

Game::MerchantSellTarget Game::merchantSellTargetForSourceSlot(int source, int slotIndex) const
{
    MerchantSellTarget target{};
    target.slotIndex = slotIndex;
    const int clampedSource = std::clamp(source, 0, BaseItemSourceCount - 1);
    target.source = static_cast<BaseItemSource>(clampedSource);

    if (target.source == BaseItemSource::Backpack) {
        if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
            return target;
        }
        if (inventory_.screenObjectStackAt(slotIndex) != nullptr ||
            inventory_.screenObjectInstanceAt(slotIndex) != nullptr) {
            target.valid = true;
        }
        return target;
    }

    if (target.source == BaseItemSource::Warehouse) {
        const std::optional<StorageEntry> entry = warehouseEntryForPageSlot(slotIndex, baseStorageWarehousePage_);
        if (!entry) {
            return target;
        }
        target.storageEntry = *entry;
        target.warehouseEntry = true;
        target.valid = true;
        return target;
    }

    target.ringIndex = ringIndexFromBaseItemSource(clampedSource);
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return target;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (slotIndex < 0 || slotIndex >= static_cast<int>(ringItems.size())) {
        return target;
    }
    target.ringItemIndex = slotIndex;
    target.valid = true;
    return target;
}

Game::MerchantSellTarget Game::merchantSellTargetForScreenSlot(int slotIndex) const
{
    return merchantSellTargetForSourceSlot(baseMerchantSellSource_, slotIndex);
}

bool Game::merchantSellTargetAvailable(MerchantSellTarget target) const
{
    if (!target.valid) {
        return false;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            return stack->count > 0 && isSellableObject(stack->item);
        }
        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            return !inventory_.isStaffEquipped(instance->instance.instanceId) &&
                !instance->instance.protectionEnabled &&
                isSellableObject(instance->item);
        }
        return false;
    }

    if (target.source == BaseItemSource::Warehouse) {
        const ItemData* item = storageEntryItem(target.storageEntry, true);
        if (item == nullptr || !isSellableObject(*item)) {
            return false;
        }
        if (target.storageEntry.kind == StorageEntryKind::Stack) {
            return storageEntryStackCount(target.storageEntry, true) > 0;
        }
        const ItemInstance* instance = storageEntryInstance(target.storageEntry, true);
        return instance != nullptr && !instance->protectionEnabled;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return false;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return false;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    if (ringItem.protectionEnabled) {
        return false;
    }
    const ItemData* item = objectForRingItem(objectCatalog_, ringItem);
    return item != nullptr && isSellableObject(*item);
}

int Game::merchantSellTargetPrice(MerchantSellTarget target) const
{
    if (!target.valid) {
        return 0;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            return sellPrice(stack->item);
        }
        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            return sellPrice(instance->item, &instance->instance);
        }
        return 0;
    }

    if (target.source == BaseItemSource::Warehouse) {
        const ItemData* item = storageEntryItem(target.storageEntry, true);
        return item != nullptr ? sellPrice(*item, storageEntryInstance(target.storageEntry, true)) : 0;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return 0;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return 0;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    const ItemData* item = objectForRingItem(objectCatalog_, ringItem);
    return item != nullptr ? sellPrice(*item, &ringItem) : 0;
}

void Game::sellMerchantTarget(MerchantSellTarget target, int count)
{
    if (!target.valid) {
        baseStatus_ = "売却対象がありません";
        return;
    }
    if (!merchantSellTargetAvailable(target)) {
        baseStatus_ = "売れません";
        if (target.source == BaseItemSource::Backpack) {
            if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                    baseStatus_ = "装備中";
                } else if (instance->instance.protectionEnabled) {
                    baseStatus_ = "保護中";
                }
            }
        } else if (target.source == BaseItemSource::Warehouse) {
            if (const ItemInstance* instance = storageEntryInstance(target.storageEntry, true)) {
                if (instance->protectionEnabled) {
                    baseStatus_ = "保護中";
                }
            }
        } else if (target.ringIndex >= 0 && target.ringIndex < SpellRingCount) {
            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
            if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size()) &&
                ringItems[static_cast<std::size_t>(target.ringItemIndex)].protectionEnabled) {
                baseStatus_ = "保護中";
            }
        }
        return;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            const int soldCount = count <= 0 ? stack->count : std::min(count, stack->count);
            const std::string objectId = stack->objectId;
            const int price = sellPrice(stack->item) * std::max(1, soldCount);
            if (inventory_.removeObjectItemCount(objectId, soldCount)) {
                money_ += price;
                baseStatus_ = "売却しました";
            }
            return;
        }

        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            const std::string instanceId = instance->instance.instanceId;
            const int price = sellPrice(instance->item, &instance->instance);
            if (inventory_.removeObjectInstance(instanceId)) {
                money_ += price;
                baseStatus_ = "売却しました";
            }
            return;
        }

        baseStatus_ = "売却対象がありません";
        return;
    }

    if (target.source == BaseItemSource::Warehouse) {
        if (target.storageEntry.kind == StorageEntryKind::Stack) {
            if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectStacks_.size())) {
                baseStatus_ = "売却対象がありません";
                return;
            }
            InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(target.storageEntry.index)];
            const int soldCount = count <= 0 ? stack.count : std::min(count, stack.count);
            money_ += sellPrice(stack.item) * std::max(1, soldCount);
            stack.count -= soldCount;
            if (stack.count <= 0) {
                removeWarehouseDisplaySlotAtEntryIndex(target.storageEntry.index);
                warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + target.storageEntry.index);
            }
            baseSellSelection_ = std::clamp(baseSellSelection_, 0, StoragePaneSlotCount - 1);
            baseStatus_ = "売却しました";
            return;
        }

        if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectInstances_.size())) {
            baseStatus_ = "売却対象がありません";
            return;
        }
        const InventoryObjectInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(target.storageEntry.index)];
        money_ += sellPrice(instance.item, &instance.instance);
        removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index);
        warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + target.storageEntry.index);
        baseSellSelection_ = std::clamp(baseSellSelection_, 0, StoragePaneSlotCount - 1);
        baseStatus_ = "売却しました";
        return;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        baseStatus_ = "売却対象がありません";
        return;
    }
    std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        baseStatus_ = "売却対象がありません";
        return;
    }
    const ItemData* item = objectForRingItem(objectCatalog_, ringItems[static_cast<std::size_t>(target.ringItemIndex)]);
    if (item == nullptr) {
        baseStatus_ = "売れません";
        return;
    }

    money_ += sellPrice(*item, &ringItems[static_cast<std::size_t>(target.ringItemIndex)]);
    ringItems.erase(ringItems.begin() + target.ringItemIndex);
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, static_cast<int>(ringItems.size()) - 1));
    baseStatus_ = "売却しました";
}

void Game::sellMerchantScreenSlot(int slotIndex, int count)
{
    sellMerchantTarget(merchantSellTargetForSourceSlot(0, slotIndex), count);
}

void Game::buyMerchantProduct(int index)
{
    refreshMerchantStock(false);
    if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
        baseStatus_ = "購入できる商品がありません";
        return;
    }

    MerchantProduct& product = merchantStock_[static_cast<std::size_t>(index)];
    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
    if (item == nullptr) {
        baseStatus_ = "商品データがありません";
        return;
    }
    if (product.quantity <= 0) {
        baseStatus_ = "品切れです";
        return;
    }
    if (money_ < product.price) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (!merchantProductCanFit(item)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    InventoryAddResult addResult;
    if (!inventory_.addObjectItem(objectCatalog_, product.objectId, &addResult)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    money_ -= product.price;
    --product.quantity;
    recordObjectObtainedForFirstNotice(
        product.objectId,
        addResult.instanceId,
        addResult.kind == InventoryAddKind::Instance && !addResult.instanceId.empty(),
        basePlayerPosition_);
    baseStatus_ = product.quantity <= 0 ? "購入しました（品切れ）" : "購入しました";
}

std::vector<Game::StorageEntry> Game::processingEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

std::optional<Game::StorageEntry> Game::processingEntryForScreenSlot(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
        return std::nullopt;
    }
    if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slotIndex)) {
        const auto& stacks = inventory_.objectStacks();
        for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
            if (stacks[static_cast<std::size_t>(i)].objectId == stack->objectId) {
                return StorageEntry{StorageEntryKind::Stack, i};
            }
        }
    }
    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slotIndex)) {
        const auto& instances = inventory_.objectInstances();
        for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
            if (instances[static_cast<std::size_t>(i)].instance.instanceId == instance->instance.instanceId) {
                return StorageEntry{StorageEntryKind::Instance, i};
            }
        }
    }
    return std::nullopt;
}

std::optional<Game::StorageEntry> Game::warehouseEntryForPageSlot(int slotIndex, int page) const
{
    return warehouseEntryForPageSlot(slotIndex, page, StoragePaneSlotCount);
}

std::optional<Game::StorageEntry> Game::warehouseEntryForPageSlot(int slotIndex, int page, int slotsPerPage) const
{
    const int pageSize = std::max(1, slotsPerPage);
    if (slotIndex < 0 || slotIndex >= pageSize) {
        return std::nullopt;
    }

    const std::vector<StorageEntry> entries = warehouseStorageEntries();
    const int pageCount = std::max(1, (warehouseCapacity() + pageSize - 1) / pageSize);
    const int warehousePage = std::clamp(page, 0, pageCount - 1);
    const int entryIndex = warehouseEntryIndexAtStorageSlot(warehousePage * pageSize + slotIndex);
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        return std::nullopt;
    }
    return entries[static_cast<std::size_t>(entryIndex)];
}

InventoryUiEntryView Game::storageEntryView(StorageEntry entry, bool warehouseEntry) const
{
    InventoryUiEntryView view{};
    view.item = storageEntryItem(entry, warehouseEntry);
    view.instance = storageEntryInstance(entry, warehouseEntry);
    view.stackCount = storageEntryStackCount(entry, warehouseEntry);
    view.equipped = !warehouseEntry &&
        view.instance != nullptr &&
        inventory_.isStaffEquipped(view.instance->instanceId);
    return view;
}

Game::ProcessingTarget Game::processingTargetForScreenSlot(int slotIndex) const
{
    ProcessingTarget target{};
    target.slotIndex = slotIndex;
    if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
        return target;
    }

    const int source = std::clamp(baseProcessingSource_, 0, BaseProcessingSourceCount - 1);
    target.source = static_cast<BaseItemSource>(source);
    if (target.source == BaseItemSource::Backpack) {
        const std::optional<StorageEntry> entry = processingEntryForScreenSlot(slotIndex);
        if (!entry) {
            return target;
        }
        target.backpackEntry = *entry;
        target.valid = true;
        return target;
    }

    if (target.source == BaseItemSource::Warehouse) {
        const std::optional<StorageEntry> entry = warehouseEntryForPageSlot(slotIndex, baseStorageWarehousePage_);
        if (!entry) {
            return target;
        }
        target.backpackEntry = *entry;
        target.warehouseEntry = true;
        target.valid = true;
        return target;
    }

    target.ringIndex = ringIndexFromBaseItemSource(source);
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return target;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (slotIndex < 0 || slotIndex >= static_cast<int>(ringItems.size())) {
        return target;
    }
    target.ringItemIndex = slotIndex;
    target.valid = true;
    return target;
}

const char* Game::processingModeName(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Repair: return "修理";
    case ProcessingMode::Attack: return "攻撃力強化";
    case ProcessingMode::Dig: return "掘削力強化";
    case ProcessingMode::Durability: return "耐久力強化";
    case ProcessingMode::ResetEnhancement: return "強化リセット";
    case ProcessingMode::Lighten: return "軽量化";
    case ProcessingMode::Enlarge: return "大型化";
    }
    return "";
}

const char* Game::processingActionName(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Repair:
        return "修理する";
    case ProcessingMode::ResetEnhancement:
        return "リセットする";
    case ProcessingMode::Lighten:
    case ProcessingMode::Enlarge:
        return "加工する";
    case ProcessingMode::Attack:
    case ProcessingMode::Dig:
    case ProcessingMode::Durability:
        return "強化する";
    }
    return "実行する";
}

bool Game::processingModeUnlocked(ProcessingMode mode) const
{
    switch (mode) {
    case ProcessingMode::Lighten:
        return processingUnlockLevel_ >= 1;
    case ProcessingMode::Enlarge:
        return processingUnlockLevel_ >= 3;
    case ProcessingMode::Repair:
    case ProcessingMode::Attack:
    case ProcessingMode::Dig:
    case ProcessingMode::Durability:
    case ProcessingMode::ResetEnhancement:
        return true;
    }
    return true;
}

bool Game::processingEntryAvailable(StorageEntry entry, bool warehouseEntry) const
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    return processingEntryAvailable(entry, mode, warehouseEntry);
}

bool Game::processingEntryAvailable(StorageEntry entry, ProcessingMode mode, bool warehouseEntry) const
{
    if (!processingModeUnlocked(mode)) {
        return false;
    }
    if (entry.kind == StorageEntryKind::Stack) {
        return mode != ProcessingMode::Repair && mode != ProcessingMode::ResetEnhancement;
    }
    const ItemInstance* instance = storageEntryInstance(entry, warehouseEntry);
    if (instance == nullptr) {
        return false;
    }
    if (mode == ProcessingMode::Repair) {
        return instance->maxDurability >= 0 && (instance->isBroken || instance->currentDurability < instance->maxDurability);
    }
    if (mode == ProcessingMode::ResetEnhancement) {
        return instance->enhanceLevel > 0 ||
            instance->attackBonus != 0 ||
            instance->digBonus != 0 ||
            instance->durabilityBonus != 0;
    }
    if (mode == ProcessingMode::Lighten) {
        return instance->weightModifier >= 0.999;
    }
    if (mode == ProcessingMode::Enlarge) {
        return instance->sizeModifier <= 1.001;
    }
    return instance->enhanceLevel < MaxItemEnhanceLevel;
}

bool Game::processingScreenSlotAvailable(int slotIndex) const
{
    return processingTargetAvailable(processingTargetForScreenSlot(slotIndex));
}

bool Game::processingTargetAvailable(ProcessingTarget target) const
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    return processingTargetAvailable(target, mode);
}

bool Game::processingTargetAvailable(ProcessingTarget target, ProcessingMode mode) const
{
    if (!target.valid) {
        return false;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        return processingEntryAvailable(target.backpackEntry, mode, target.warehouseEntry);
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return false;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return false;
    }

    if (!processingModeUnlocked(mode)) {
        return false;
    }
    const SpellRingItem& item = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    if (mode == ProcessingMode::Repair) {
        return item.maxDurability >= 0 && (item.broken() || item.durability < item.maxDurability);
    }
    if (mode == ProcessingMode::ResetEnhancement) {
        return item.enhanceLevel > 0 ||
            item.attackBonus != 0 ||
            item.digBonus != 0 ||
            item.durabilityBonus != 0;
    }
    if (mode == ProcessingMode::Lighten) {
        return item.weightModifier >= 0.999;
    }
    if (mode == ProcessingMode::Enlarge) {
        return item.sizeModifier <= 1.001;
    }
    return item.enhanceLevel < MaxItemEnhanceLevel;
}

bool Game::processingTargetHasAvailableCommand(ProcessingTarget target) const
{
    for (int i = 0; i < BaseProcessingModeCount; ++i) {
        if (processingTargetAvailable(target, static_cast<ProcessingMode>(i))) {
            return true;
        }
    }
    return false;
}

bool Game::processingCommandExecutable(ProcessingTarget target, ProcessingMode mode) const
{
    if (!processingTargetAvailable(target, mode)) {
        return false;
    }
    return money_ >= processingMoneyCost(target, mode) &&
        inventory_.materialCount(MaterialType::EnhancementOre) >= processingOreCost(target, mode);
}

int Game::processingMoneyCost(StorageEntry entry, ProcessingMode mode, bool warehouseEntry) const
{
    const ItemData* item = storageEntryItem(entry, warehouseEntry);
    const int basePrice = std::max(1, item != nullptr ? item->price : 0);
    const auto discountCost = [this](int rawCost) {
        double multiplier = 1.0;
        if (processingUnlockLevel_ >= 5) {
            multiplier = 0.70;
        } else if (processingUnlockLevel_ >= 4) {
            multiplier = 0.80;
        } else if (processingUnlockLevel_ >= 2) {
            multiplier = 0.90;
        }
        return std::max(1, static_cast<int>(std::ceil(static_cast<double>(std::max(1, rawCost)) * multiplier)));
    };
    if (mode == ProcessingMode::Repair) {
        const ItemInstance* instance = storageEntryInstance(entry, warehouseEntry);
        if (instance == nullptr || instance->maxDurability <= 0) {
            return 0;
        }
        if (instance->isBroken) {
            return discountCost(static_cast<int>(std::ceil(static_cast<double>(basePrice) * 0.6)));
        }
        const int missing = std::max(0, instance->maxDurability - instance->currentDurability);
        if (missing <= 0) {
            return 0;
        }
        const double ratio = static_cast<double>(missing) / static_cast<double>(instance->maxDurability);
        return discountCost(static_cast<int>(std::ceil(static_cast<double>(basePrice) * ratio * 0.4)));
    }
    if (mode == ProcessingMode::ResetEnhancement) {
        return discountCost(std::max(20, basePrice / 4));
    }
    if (mode == ProcessingMode::Lighten) {
        return discountCost(std::max(80, basePrice / 2 + 90));
    }
    if (mode == ProcessingMode::Enlarge) {
        return discountCost(std::max(120, basePrice / 2 + 140));
    }

    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, warehouseEntry)) {
        enhanceLevel = instance->enhanceLevel;
    }
    return discountCost(std::max(20, basePrice / 2 + (enhanceLevel + 1) * 50));
}

int Game::processingOreCost(StorageEntry entry, ProcessingMode mode, bool warehouseEntry) const
{
    if (mode == ProcessingMode::Repair || mode == ProcessingMode::ResetEnhancement) {
        return 0;
    }
    if (mode == ProcessingMode::Lighten) {
        return 2;
    }
    if (mode == ProcessingMode::Enlarge) {
        return 3;
    }
    int enhanceLevel = 0;
    if (const ItemInstance* instance = storageEntryInstance(entry, warehouseEntry)) {
        enhanceLevel = instance->enhanceLevel;
    }
    return enhanceLevel + 1;
}

int Game::processingMoneyCost(ProcessingTarget target, ProcessingMode mode) const
{
    if (!target.valid) {
        return 0;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        return processingMoneyCost(target.backpackEntry, mode, target.warehouseEntry);
    }

    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return 0;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    const ItemData* item = objectForRingItem(objectCatalog_, ringItem);
    const int basePrice = std::max(1, item != nullptr ? item->price : 0);
    const auto discountCost = [this](int rawCost) {
        double multiplier = 1.0;
        if (processingUnlockLevel_ >= 5) {
            multiplier = 0.70;
        } else if (processingUnlockLevel_ >= 4) {
            multiplier = 0.80;
        } else if (processingUnlockLevel_ >= 2) {
            multiplier = 0.90;
        }
        return std::max(1, static_cast<int>(std::ceil(static_cast<double>(std::max(1, rawCost)) * multiplier)));
    };
    if (mode == ProcessingMode::Repair) {
        if (ringItem.maxDurability <= 0) {
            return 0;
        }
        if (ringItem.broken()) {
            return discountCost(static_cast<int>(std::ceil(static_cast<double>(basePrice) * 0.6)));
        }
        const int missing = std::max(0, ringItem.maxDurability - ringItem.durability);
        if (missing <= 0) {
            return 0;
        }
        const double ratio = static_cast<double>(missing) / static_cast<double>(ringItem.maxDurability);
        return discountCost(static_cast<int>(std::ceil(static_cast<double>(basePrice) * ratio * 0.4)));
    }
    if (mode == ProcessingMode::ResetEnhancement) {
        return discountCost(std::max(20, basePrice / 4));
    }
    if (mode == ProcessingMode::Lighten) {
        return discountCost(std::max(80, basePrice / 2 + 90));
    }
    if (mode == ProcessingMode::Enlarge) {
        return discountCost(std::max(120, basePrice / 2 + 140));
    }

    return discountCost(std::max(20, basePrice / 2 + (ringItem.enhanceLevel + 1) * 50));
}

int Game::processingOreCost(ProcessingTarget target, ProcessingMode mode) const
{
    if (mode == ProcessingMode::Repair || mode == ProcessingMode::ResetEnhancement || !target.valid) {
        return 0;
    }
    if (mode == ProcessingMode::Lighten) {
        return 2;
    }
    if (mode == ProcessingMode::Enlarge) {
        return 3;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        return processingOreCost(target.backpackEntry, mode, target.warehouseEntry);
    }

    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return 0;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    return ringItem.enhanceLevel + 1;
}

std::vector<UiCommandMenuItem> Game::processingCommandItems(ProcessingTarget target) const
{
    std::vector<UiCommandMenuItem> items;
    items.reserve(BaseProcessingModeCount);
    for (int i = 0; i < BaseProcessingModeCount; ++i) {
        const ProcessingMode mode = static_cast<ProcessingMode>(i);
        items.push_back(UiCommandMenuItem{
            processingModeName(mode),
            processingTargetAvailable(target, mode),
        });
    }
    return items;
}

void Game::openProcessingConfirm(ProcessingTarget target, ProcessingMode mode)
{
    baseProcessingMode_ = static_cast<int>(mode);
    baseProcessingConfirmTarget_ = target;
    baseProcessingConfirmMode_ = mode;
    const bool executable = processingCommandExecutable(target, mode);
    openUiConfirmDialog(
        baseProcessingConfirm_,
        processingModeName(mode),
        "",
        processingActionName(mode),
        "戻る",
        executable ? 0 : 1);
    baseProcessingConfirm_.confirmEnabled = executable;
    baseStatus_.clear();
}

void Game::drawProcessingConfirmDialog(Renderer& renderer, UiRect panel) const
{
    if (!baseProcessingConfirm_.open) {
        return;
    }

    UiWindowScope window(
        renderer,
        "base.processing.confirm",
        panel,
        baseProcessingConfirm_.title,
        uiConfirmDialogHelpText(),
        UiWindowOptions{true, true});

    constexpr float ContentInset = ui::PanelPadding + 12.0f;
    constexpr float BodyTopOffset = -2.0f;
    const float bodyTop = panel.pos.y + ui::HeaderHeight + BodyTopOffset;
    const UiRect body{{
        panel.pos.x + ContentInset,
        bodyTop,
    }, {
        panel.size.x - ContentInset * 2.0f,
        std::max(0.0f, uiConfirmDialogButtonRect(panel, 0).pos.y - bodyTop - 16.0f),
    }};

    const auto targetSnapshot = [this](ProcessingTarget snapshotTarget) {
        if (snapshotTarget.source == BaseItemSource::Backpack || snapshotTarget.source == BaseItemSource::Warehouse) {
            if (snapshotTarget.backpackEntry.kind == StorageEntryKind::Stack) {
                const InventoryObjectStack& stack = snapshotTarget.warehouseEntry
                    ? warehouseObjectStacks_[static_cast<std::size_t>(snapshotTarget.backpackEntry.index)]
                    : inventory_.objectStacks()[static_cast<std::size_t>(snapshotTarget.backpackEntry.index)];
                return processingSnapshotFromStack(stack);
            }
            const InventoryObjectInstance& instance = snapshotTarget.warehouseEntry
                ? warehouseObjectInstances_[static_cast<std::size_t>(snapshotTarget.backpackEntry.index)]
                : inventory_.objectInstances()[static_cast<std::size_t>(snapshotTarget.backpackEntry.index)];
            return processingSnapshotFromInstance(instance);
        }

        const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(snapshotTarget.ringIndex);
        if (snapshotTarget.ringItemIndex >= 0 && snapshotTarget.ringItemIndex < static_cast<int>(ringItems.size())) {
            return processingSnapshotFromRingItem(objectCatalog_, ringItems[static_cast<std::size_t>(snapshotTarget.ringItemIndex)]);
        }
        ProcessingResultSnapshot snapshot{};
        snapshot.name = "アイテム";
        return snapshot;
    };

    if (!baseProcessingConfirmTarget_.valid) {
        renderer.drawText(body.pos, "加工対象がありません", ui::Text, 2);
        drawUiConfirmDialogButtons(renderer, baseProcessingConfirm_, panel);
        return;
    }

    const ProcessingMode mode = baseProcessingConfirmMode_;
    const ProcessingResultSnapshot before = targetSnapshot(baseProcessingConfirmTarget_);
    ProcessingResultSnapshot after = before;
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    if (mode == ProcessingMode::Attack) {
        attackBonus = 1;
    } else if (mode == ProcessingMode::Dig) {
        digBonus = 1;
    } else if (mode == ProcessingMode::Durability) {
        durabilityBonus = 2;
    }

    if (mode == ProcessingMode::Repair) {
        if (after.maxDurability >= 0) {
            after.currentDurability = after.maxDurability;
            after.isBroken = false;
        }
    } else if (mode == ProcessingMode::ResetEnhancement) {
        after = processingResetSnapshot(after);
    } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
        after = processingShapeSnapshot(
            after,
            mode == ProcessingMode::Lighten ? LightenWeightMultiplier : EnlargeWeightMultiplier,
            mode == ProcessingMode::Lighten ? 1.0 : EnlargeSizeMultiplier);
    } else {
        after = processingEnhancedSnapshot(after, attackBonus, digBonus, durabilityBonus);
    }

    std::vector<ProcessingPreviewRow> previewRows;
    if (mode == ProcessingMode::Repair) {
        previewRows.push_back({
            "耐久力",
            formatProcessingDurability(before.currentDurability, before.maxDurability),
            formatProcessingDurability(after.currentDurability, after.maxDurability),
        });
        if (before.isBroken && !after.isBroken) {
            previewRows.push_back({"状態", "破損", "通常"});
        }
    } else if (mode == ProcessingMode::ResetEnhancement) {
        previewRows.push_back({"攻撃力補正", formatProcessingInt(before.attackBonus), formatProcessingInt(after.attackBonus)});
        previewRows.push_back({"掘削力補正", formatProcessingInt(before.digBonus), formatProcessingInt(after.digBonus)});
        previewRows.push_back({"耐久力補正", formatProcessingInt(before.durabilityBonus), formatProcessingInt(after.durabilityBonus)});
        previewRows.push_back({"合計強化回数", formatProcessingInt(before.enhanceLevel), formatProcessingInt(after.enhanceLevel)});
    } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
        previewRows.push_back({"重量", formatProcessingPercent(before.weightModifier), formatProcessingPercent(after.weightModifier)});
        previewRows.push_back({"大きさ", formatProcessingPercent(before.sizeModifier), formatProcessingPercent(after.sizeModifier)});
    } else {
        if (mode == ProcessingMode::Attack) {
            previewRows.push_back({"攻撃力補正", formatProcessingInt(before.attackBonus), formatProcessingInt(after.attackBonus)});
        } else if (mode == ProcessingMode::Dig) {
            previewRows.push_back({"掘削力補正", formatProcessingInt(before.digBonus), formatProcessingInt(after.digBonus)});
        } else if (mode == ProcessingMode::Durability) {
            previewRows.push_back({"最大耐久力", formatProcessingMaxDurability(before.maxDurability), formatProcessingMaxDurability(after.maxDurability)});
        }
        previewRows.push_back({"合計強化回数", formatProcessingInt(before.enhanceLevel), formatProcessingInt(after.enhanceLevel)});
    }

    const auto confirmQuestion = [&]() -> std::string {
        const std::string itemName = processingInlineItemName(before);
        switch (mode) {
        case ProcessingMode::Repair:
            return itemName + "を修理しますか？";
        case ProcessingMode::ResetEnhancement:
            return itemName + "の強化をリセットしますか？";
        case ProcessingMode::Lighten:
            return itemName + "を軽量化しますか？";
        case ProcessingMode::Enlarge:
            return itemName + "を大型化しますか？";
        case ProcessingMode::Attack:
        case ProcessingMode::Dig:
        case ProcessingMode::Durability:
            return itemName + "を強化しますか？";
        }
        return itemName + "に作業を行いますか？";
    };

    float y = body.pos.y;
    InlineItemTextStyle questionStyle{};
    questionStyle.text = ui::Text;
    questionStyle.scale = 2;
    const std::string question = fittedInlineItemText(renderer, confirmQuestion(), body.size.x, questionStyle);
    drawInlineItemText(renderer, objectCatalog_, {body.pos.x, y}, question, questionStyle);
    y += measureInlineItemText(renderer, question, questionStyle).y + 22.0f;

    for (const ProcessingPreviewRow& row : previewRows) {
        drawProcessingPreviewRow(renderer, body, y, row);
    }

    std::vector<RequirementRow> requirements;
    const int moneyCost = processingMoneyCost(baseProcessingConfirmTarget_, mode);
    const int oreCost = processingOreCost(baseProcessingConfirmTarget_, mode);
    if (moneyCost > 0) {
        requirements.push_back(moneyRequirementRow(moneyCost, money_));
    }
    if (oreCost > 0) {
        requirements.push_back(materialRequirementRow(
            MaterialType::EnhancementOre,
            oreCost,
            inventory_.materialCount(MaterialType::EnhancementOre)));
    }

    const float buttonTop = uiConfirmDialogButtonRect(panel, 0).pos.y;
    constexpr float RequirementTopPadding = ui::SubPanelPadding.y;
    constexpr float RequirementTitleToRows = 34.0f;
    constexpr float RequirementRowHeight = 31.0f;
    constexpr float RequirementBottomPadding = 18.0f;
    const float materialTop = y + 7.0f;
    const float requiredRows = static_cast<float>(std::max<std::size_t>(1, requirements.size()));
    const float preferredMaterialHeight =
        RequirementTopPadding + RequirementTitleToRows + RequirementRowHeight * requiredRows + RequirementBottomPadding;
    const float materialHeight = std::max(86.0f, std::min(preferredMaterialHeight, buttonTop - materialTop - 14.0f));
    drawRequirementSubWindow(
        renderer,
        objectCatalog_,
        {{body.pos.x, materialTop}, {body.size.x, materialHeight}},
        requirements);

    drawUiConfirmDialogButtons(renderer, baseProcessingConfirm_, panel);
}

void Game::applyProcessing(int entryIndex)
{
    const std::vector<StorageEntry> entries = processingEntries();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(entries.size())) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    const StorageEntry entry = entries[static_cast<std::size_t>(entryIndex)];
    applyProcessingEntry(entry);
}

void Game::applyProcessingScreenSlot(int slotIndex)
{
    const ProcessingTarget target = processingTargetForScreenSlot(slotIndex);
    if (!target.valid) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    applyProcessingTarget(target);
}

void Game::applyProcessingEntry(StorageEntry entry, bool warehouseEntry)
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    applyProcessingEntry(entry, mode, warehouseEntry);
}

void Game::applyProcessingEntry(StorageEntry entry, ProcessingMode mode, bool warehouseEntry)
{
    if (!processingEntryAvailable(entry, mode, warehouseEntry)) {
        if (!processingModeUnlocked(mode)) {
            baseStatus_ = "この作業は未解禁です";
        } else if ((mode == ProcessingMode::Repair || mode == ProcessingMode::ResetEnhancement) && entry.kind == StorageEntryKind::Stack) {
            baseStatus_ = "この作業はできません";
        } else if (mode == ProcessingMode::ResetEnhancement) {
            baseStatus_ = "リセット不要です";
        } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
            baseStatus_ = "加工済みです";
        } else {
            baseStatus_ = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
        }
        return;
    }

    const int moneyCost = processingMoneyCost(entry, mode, warehouseEntry);
    const int oreCost = processingOreCost(entry, mode, warehouseEntry);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りません";
        return;
    }

    const auto entrySnapshot = [this, warehouseEntry](StorageEntry snapshotEntry) {
        if (snapshotEntry.kind == StorageEntryKind::Stack) {
            const InventoryObjectStack& stack = warehouseEntry
                ? warehouseObjectStacks_[static_cast<std::size_t>(snapshotEntry.index)]
                : inventory_.objectStacks()[static_cast<std::size_t>(snapshotEntry.index)];
            return processingSnapshotFromStack(stack);
        }
        const InventoryObjectInstance& instance = warehouseEntry
            ? warehouseObjectInstances_[static_cast<std::size_t>(snapshotEntry.index)]
            : inventory_.objectInstances()[static_cast<std::size_t>(snapshotEntry.index)];
        return processingSnapshotFromInstance(instance);
    };
    const ProcessingResultSnapshot beforeSnapshot = entrySnapshot(entry);
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    if (mode == ProcessingMode::Attack) {
        attackBonus = 1;
    } else if (mode == ProcessingMode::Dig) {
        digBonus = 1;
    } else if (mode == ProcessingMode::Durability) {
        durabilityBonus = 2;
    }

    const auto applyEnhancement = [&](ItemInstance& instance) {
        if (instance.enhanceLevel >= MaxItemEnhanceLevel) {
            return false;
        }
        ++instance.enhanceLevel;
        instance.attackBonus += attackBonus;
        instance.digBonus += digBonus;
        instance.durabilityBonus += durabilityBonus;
        if (durabilityBonus > 0 && instance.maxDurability >= 0) {
            instance.maxDurability += durabilityBonus;
            instance.currentDurability = std::min(instance.maxDurability, std::max(0, instance.currentDurability + durabilityBonus));
        }
        return true;
    };
    const auto resetEnhancement = [this](ItemInstance& instance) {
        if (instance.enhanceLevel <= 0 &&
            instance.attackBonus == 0 &&
            instance.digBonus == 0 &&
            instance.durabilityBonus == 0) {
            return false;
        }
        const ItemData* item = objectCatalog_.registry.findById(instance.objectId);
        const int baseDurability = item != nullptr ? item->durability : std::max(-1, instance.maxDurability - instance.durabilityBonus);
        instance.enhanceLevel = 0;
        instance.attackBonus = 0;
        instance.digBonus = 0;
        instance.durabilityBonus = 0;
        instance.maxDurability = baseDurability;
        if (instance.maxDurability >= 0) {
            instance.currentDurability = std::clamp(instance.currentDurability, 0, instance.maxDurability);
            instance.isBroken = instance.currentDurability == 0;
        } else {
            instance.isBroken = false;
        }
        return true;
    };
    const auto applyShapeProcessing = [](ItemInstance& instance, ProcessingMode shapeMode) {
        if (shapeMode == ProcessingMode::Lighten) {
            if (instance.weightModifier < 0.999) {
                return false;
            }
            instance.weightModifier = std::clamp(instance.weightModifier * LightenWeightMultiplier, 0.25, 4.0);
            return true;
        }
        if (shapeMode == ProcessingMode::Enlarge) {
            if (instance.sizeModifier > 1.001) {
                return false;
            }
            instance.weightModifier = std::clamp(instance.weightModifier * EnlargeWeightMultiplier, 0.25, 4.0);
            instance.sizeModifier = std::clamp(instance.sizeModifier * EnlargeSizeMultiplier, 0.50, 3.0);
            return true;
        }
        return false;
    };
    const auto allocateWarehouseInstanceId = [this]() {
        constexpr std::string_view Prefix = "warehouseinst_";
        unsigned long long nextId = 1;
        const auto scanId = [&nextId, Prefix](const std::string& id) {
            if (id.rfind(Prefix, 0) == 0) {
                const unsigned long long parsed = std::strtoull(id.c_str() + Prefix.size(), nullptr, 10);
                nextId = std::max(nextId, parsed + 1);
            }
        };
        for (const InventoryObjectInstance& instance : inventory_.objectInstances()) {
            scanId(instance.instance.instanceId);
        }
        for (const InventoryObjectInstance& instance : warehouseObjectInstances_) {
            scanId(instance.instance.instanceId);
        }
        for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
            for (const SpellRingItem& item : spellRing_.itemsForRing(ringIndex)) {
                scanId(item.instanceId);
            }
        }
        return std::string(Prefix) + std::to_string(nextId);
    };

    bool processed = false;
    if (!warehouseEntry && mode == ProcessingMode::Repair) {
        const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
        processed = inventory_.repairObjectInstance(instance.instance.instanceId);
    } else if (!warehouseEntry && mode == ProcessingMode::ResetEnhancement) {
        const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
        processed = inventory_.resetObjectInstanceEnhancement(instance.instance.instanceId, objectCatalog_);
    } else if (!warehouseEntry && (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge)) {
        if (entry.kind == StorageEntryKind::Stack) {
            const InventoryObjectStack& stack = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.modifyObjectStackItemShape(
                stack.objectId,
                mode == ProcessingMode::Lighten ? LightenWeightMultiplier : EnlargeWeightMultiplier,
                mode == ProcessingMode::Lighten ? 1.0 : EnlargeSizeMultiplier);
        } else {
            const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.modifyObjectInstanceShape(
                instance.instance.instanceId,
                mode == ProcessingMode::Lighten ? LightenWeightMultiplier : EnlargeWeightMultiplier,
                mode == ProcessingMode::Lighten ? 1.0 : EnlargeSizeMultiplier);
        }
    } else if (!warehouseEntry) {
        if (entry.kind == StorageEntryKind::Stack) {
            const InventoryObjectStack& stack = inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectStackItem(stack.objectId, attackBonus, digBonus, durabilityBonus, MaxItemEnhanceLevel);
        } else {
            const InventoryObjectInstance& instance = inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
            processed = inventory_.enhanceObjectInstance(instance.instance.instanceId, attackBonus, digBonus, durabilityBonus, MaxItemEnhanceLevel);
        }
    } else if (mode == ProcessingMode::Repair) {
        ItemInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance;
        if (instance.maxDurability >= 0) {
            instance.currentDurability = instance.maxDurability;
            instance.isBroken = false;
            processed = true;
        }
    } else if (mode == ProcessingMode::ResetEnhancement) {
        ItemInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance;
        processed = resetEnhancement(instance);
    } else if (entry.kind == StorageEntryKind::Stack) {
        InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(entry.index)];
        const bool stackSlotWillRemain = stack.count > 1;
        if (stackSlotWillRemain && warehouseUsedSlots() >= warehouseCapacity()) {
            baseStatus_ = "倉庫がいっぱいです";
            return;
        }
        syncWarehouseDisplaySlots();
        const int originalSlot = entry.index >= 0 && entry.index < static_cast<int>(warehouseDisplaySlots_.size())
            ? warehouseDisplaySlots_[static_cast<std::size_t>(entry.index)]
            : -1;
        const ItemData item = stack.item;
        ItemInstance instance = makeItemInstanceFromDefinition(allocateWarehouseInstanceId(), item);
        processed = (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge)
            ? applyShapeProcessing(instance, mode)
            : applyEnhancement(instance);
        if (processed) {
            --stack.count;
            if (stack.count <= 0) {
                removeWarehouseDisplaySlotAtEntryIndex(entry.index);
                warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + entry.index);
            }
            warehouseObjectInstances_.push_back(InventoryObjectInstance{item, std::move(instance)});
            warehouseDisplaySlots_.push_back(stackSlotWillRemain ? -1 : originalSlot);
            syncWarehouseDisplaySlots();
        }
    } else {
        ItemInstance& instance = warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance;
        processed = (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge)
            ? applyShapeProcessing(instance, mode)
            : applyEnhancement(instance);
    }
    if (!processed) {
        baseStatus_ = "加工できません";
        return;
    }

    money_ -= moneyCost;
    if (oreCost > 0) {
        const bool spentOre = inventory_.materials().spend(MaterialType::EnhancementOre, oreCost);
        (void)spentOre;
    }
    baseStatus_.clear();
    if (mode == ProcessingMode::Repair) {
        const ProcessingResultSnapshot afterSnapshot = entrySnapshot(entry);
        openUiResultDialog(baseResultDialog_, "作業完了", processingRepairResultLines(beforeSnapshot, afterSnapshot));
    } else if (mode == ProcessingMode::ResetEnhancement) {
        const ProcessingResultSnapshot afterSnapshot = entrySnapshot(entry);
        openUiResultDialog(baseResultDialog_, "作業完了", processingResetResultLines(beforeSnapshot, afterSnapshot));
    } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
        const ProcessingResultSnapshot afterSnapshot = processingShapeSnapshot(
            beforeSnapshot,
            mode == ProcessingMode::Lighten ? LightenWeightMultiplier : EnlargeWeightMultiplier,
            mode == ProcessingMode::Lighten ? 1.0 : EnlargeSizeMultiplier);
        openUiResultDialog(baseResultDialog_, "作業完了", processingShapeResultLines(beforeSnapshot, afterSnapshot, mode == ProcessingMode::Lighten));
    } else {
        const ProcessingResultSnapshot afterSnapshot = entry.kind == StorageEntryKind::Stack
            ? processingEnhancedSnapshot(beforeSnapshot, attackBonus, digBonus, durabilityBonus)
            : entrySnapshot(entry);
        openUiResultDialog(
            baseResultDialog_,
            "作業完了",
            processingEnhanceResultLines(
                beforeSnapshot,
                afterSnapshot,
                mode == ProcessingMode::Attack,
                mode == ProcessingMode::Dig,
                mode == ProcessingMode::Durability));
    }
    const int selectionCount = warehouseEntry ? StoragePaneSlotCount : inventory_.screenSlotCount();
    baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, selectionCount - 1));
}

void Game::applyProcessingTarget(ProcessingTarget target)
{
    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
    applyProcessingTarget(target, mode);
}

void Game::applyProcessingTarget(ProcessingTarget target, ProcessingMode mode)
{
    if (!target.valid) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        applyProcessingEntry(target.backpackEntry, mode, target.warehouseEntry);
        return;
    }

    if (!processingTargetAvailable(target, mode)) {
        if (!processingModeUnlocked(mode)) {
            baseStatus_ = "この作業は未解禁です";
        } else if (mode == ProcessingMode::ResetEnhancement) {
            baseStatus_ = "リセット不要です";
        } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
            baseStatus_ = "加工済みです";
        } else {
            baseStatus_ = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
        }
        return;
    }

    const int moneyCost = processingMoneyCost(target, mode);
    const int oreCost = processingOreCost(target, mode);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
        baseStatus_ = "強化鉱石が足りません";
        return;
    }

    const std::vector<SpellRingItem>& ringItemsBefore = spellRing_.itemsForRing(target.ringIndex);
    const ProcessingResultSnapshot beforeSnapshot =
        processingSnapshotFromRingItem(objectCatalog_, ringItemsBefore[static_cast<std::size_t>(target.ringItemIndex)]);
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    if (mode == ProcessingMode::Attack) {
        attackBonus = 1;
    } else if (mode == ProcessingMode::Dig) {
        digBonus = 1;
    } else if (mode == ProcessingMode::Durability) {
        durabilityBonus = 2;
    }

    bool processed = false;
    if (mode == ProcessingMode::Repair) {
        processed = spellRing_.repairItem(target.ringIndex, target.ringItemIndex);
    } else if (mode == ProcessingMode::ResetEnhancement ||
        mode == ProcessingMode::Lighten ||
        mode == ProcessingMode::Enlarge) {
        std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
        if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size())) {
            SpellRingItem& item = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
            if (mode == ProcessingMode::ResetEnhancement) {
                const ItemData* object = objectCatalog_.registry.findById(item.objectId);
                const int baseDurability = object != nullptr ? object->durability : std::max(-1, item.maxDurability - item.durabilityBonus);
                item.enhanceLevel = 0;
                item.attackBonus = 0;
                item.digBonus = 0;
                item.durabilityBonus = 0;
                item.maxDurability = baseDurability;
                if (item.maxDurability >= 0) {
                    item.durability = std::clamp(item.durability, 0, item.maxDurability);
                    item.isBroken = item.durability == 0;
                } else {
                    item.isBroken = false;
                }
                item.objectStatsApplied = false;
                spellRing_.applyObjectParameters(objectCatalog_);
                processed = true;
            } else if (mode == ProcessingMode::Lighten) {
                if (item.weightModifier >= 0.999) {
                    item.weightModifier = std::clamp(item.weightModifier * LightenWeightMultiplier, 0.25, 4.0);
                    item.objectStatsApplied = false;
                    spellRing_.applyObjectParameters(objectCatalog_);
                    processed = true;
                }
            } else if (mode == ProcessingMode::Enlarge) {
                if (item.sizeModifier <= 1.001) {
                    item.weightModifier = std::clamp(item.weightModifier * EnlargeWeightMultiplier, 0.25, 4.0);
                    item.sizeModifier = std::clamp(item.sizeModifier * EnlargeSizeMultiplier, 0.50, 3.0);
                    item.objectStatsApplied = false;
                    spellRing_.applyObjectParameters(objectCatalog_);
                    processed = true;
                }
            }
        }
    } else {
        processed = spellRing_.enhanceItem(
            target.ringIndex,
            target.ringItemIndex,
            attackBonus,
            digBonus,
            durabilityBonus,
            MaxItemEnhanceLevel,
            objectCatalog_);
    }
    if (!processed) {
        baseStatus_ = "加工できません";
        return;
    }

    money_ -= moneyCost;
    if (oreCost > 0) {
        const bool spentOre = inventory_.materials().spend(MaterialType::EnhancementOre, oreCost);
        (void)spentOre;
    }
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    const std::vector<SpellRingItem>& ringItemsAfter = spellRing_.itemsForRing(target.ringIndex);
    const ProcessingResultSnapshot afterSnapshot =
        processingSnapshotFromRingItem(objectCatalog_, ringItemsAfter[static_cast<std::size_t>(target.ringItemIndex)]);
    baseStatus_.clear();
    if (mode == ProcessingMode::Repair) {
        openUiResultDialog(baseResultDialog_, "作業完了", processingRepairResultLines(beforeSnapshot, afterSnapshot));
    } else if (mode == ProcessingMode::ResetEnhancement) {
        openUiResultDialog(baseResultDialog_, "作業完了", processingResetResultLines(beforeSnapshot, afterSnapshot));
    } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
        openUiResultDialog(baseResultDialog_, "作業完了", processingShapeResultLines(beforeSnapshot, afterSnapshot, mode == ProcessingMode::Lighten));
    } else {
        openUiResultDialog(
            baseResultDialog_,
            "作業完了",
            processingEnhanceResultLines(
                beforeSnapshot,
                afterSnapshot,
                mode == ProcessingMode::Attack,
                mode == ProcessingMode::Dig,
                mode == ProcessingMode::Durability));
    }
    baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
}

int Game::warehouseCapacity() const
{
    constexpr std::array<int, 5> Capacities{{48, 72, 100, 140, 200}};
    const int level = std::clamp(warehouseCapacityLevel_, 0, static_cast<int>(Capacities.size()) - 1);
    return Capacities[static_cast<std::size_t>(level)];
}

int Game::warehouseUsedSlots() const
{
    return static_cast<int>(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
}

int Game::backpackUsedSlots() const
{
    return static_cast<int>(inventory_.objectStacks().size() + inventory_.objectInstances().size());
}

std::vector<Game::StorageEntry> Game::backpackStorageEntries() const
{
    std::vector<StorageEntry> entries;
    const auto& stacks = inventory_.objectStacks();
    const auto& instances = inventory_.objectInstances();
    entries.reserve(stacks.size() + instances.size());
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i) {
        if (stacks[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(instances.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

std::vector<Game::StorageEntry> Game::warehouseStorageEntries() const
{
    std::vector<StorageEntry> entries;
    entries.reserve(warehouseObjectStacks_.size() + warehouseObjectInstances_.size());
    for (int i = 0; i < static_cast<int>(warehouseObjectStacks_.size()); ++i) {
        if (warehouseObjectStacks_[static_cast<std::size_t>(i)].count > 0) {
            entries.push_back(StorageEntry{StorageEntryKind::Stack, i});
        }
    }
    for (int i = 0; i < static_cast<int>(warehouseObjectInstances_.size()); ++i) {
        entries.push_back(StorageEntry{StorageEntryKind::Instance, i});
    }
    return entries;
}

void Game::syncWarehouseDisplaySlots() const
{
    const int totalCount = warehouseUsedSlots();
    if (totalCount <= 0) {
        warehouseDisplaySlots_.clear();
        return;
    }

    const int capacity = warehouseCapacity();
    std::vector<int> nextSlots(static_cast<std::size_t>(totalCount), -1);
    std::vector<bool> used(static_cast<std::size_t>(capacity), false);
    const int copyCount = std::min(totalCount, static_cast<int>(warehouseDisplaySlots_.size()));
    for (int i = 0; i < copyCount; ++i) {
        const int slot = warehouseDisplaySlots_[static_cast<std::size_t>(i)];
        if (slot >= 0 && slot < capacity && !used[static_cast<std::size_t>(slot)]) {
            nextSlots[static_cast<std::size_t>(i)] = slot;
            used[static_cast<std::size_t>(slot)] = true;
        }
    }

    int cursor = 0;
    for (int i = 0; i < totalCount; ++i) {
        if (nextSlots[static_cast<std::size_t>(i)] >= 0) {
            continue;
        }
        while (cursor < capacity && used[static_cast<std::size_t>(cursor)]) {
            ++cursor;
        }
        if (cursor >= capacity) {
            nextSlots[static_cast<std::size_t>(i)] = i % capacity;
        } else {
            nextSlots[static_cast<std::size_t>(i)] = cursor;
            used[static_cast<std::size_t>(cursor)] = true;
            ++cursor;
        }
    }
    warehouseDisplaySlots_ = std::move(nextSlots);
}

void Game::sortWarehouseByCatalogOrder()
{
    closeUiCommandMenu(baseStorageCommandMenu_);
    baseStorageCommandOperation_ = StorageQuantityOperation::None;
    baseStorageCommandTarget_ = {};
    baseStoragePointerOperation_ = StorageQuantityOperation::None;
    baseStoragePointerTarget_ = {};
    baseStoragePointerPressMouse_ = {};
    baseStoragePointerPressCanOpenMenu_ = false;
    baseStoragePointerDragTriggered_ = false;

    const int totalCount = warehouseUsedSlots();
    if (totalCount <= 0) {
        warehouseDisplaySlots_.clear();
        baseStorageWarehousePage_ = 0;
        baseStorageWithdrawSelection_ = 0;
        baseStatus_ = "収納箱は空です";
        return;
    }

    const auto order = buildObjectSortOrder(objectCatalog_);
    std::stable_sort(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [&order](const InventoryObjectStack& a, const InventoryObjectStack& b) {
        const int orderA = objectSortOrder(order, a.objectId);
        const int orderB = objectSortOrder(order, b.objectId);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        return a.objectId < b.objectId;
    });
    std::stable_sort(warehouseObjectInstances_.begin(), warehouseObjectInstances_.end(), [&order](const InventoryObjectInstance& a, const InventoryObjectInstance& b) {
        const std::string& idA = objectSortId(a);
        const std::string& idB = objectSortId(b);
        const int orderA = objectSortOrder(order, idA);
        const int orderB = objectSortOrder(order, idB);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        return idA < idB;
    });

    std::vector<int> entryIndices;
    entryIndices.reserve(static_cast<std::size_t>(totalCount));
    for (int i = 0; i < totalCount; ++i) {
        entryIndices.push_back(i);
    }
    std::stable_sort(entryIndices.begin(), entryIndices.end(), [this, &order](int a, int b) {
        const std::string& idA = warehouseEntrySortId(a, warehouseObjectStacks_, warehouseObjectInstances_);
        const std::string& idB = warehouseEntrySortId(b, warehouseObjectStacks_, warehouseObjectInstances_);
        const int orderA = objectSortOrder(order, idA);
        const int orderB = objectSortOrder(order, idB);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        if (idA != idB) {
            return idA < idB;
        }
        return a < b;
    });

    const int capacity = warehouseCapacity();
    warehouseDisplaySlots_.assign(static_cast<std::size_t>(totalCount), -1);
    for (int slot = 0; slot < static_cast<int>(entryIndices.size()); ++slot) {
        const int entryIndex = entryIndices[static_cast<std::size_t>(slot)];
        warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)] = capacity > 0 ? slot % capacity : -1;
    }

    baseStorageWarehousePage_ = 0;
    baseStorageWithdrawSelection_ = 0;
    baseStatus_ = "収納箱を並び替えました";
}

int Game::warehouseEntryIndexAtStorageSlot(int slot) const
{
    syncWarehouseDisplaySlots();
    if (slot < 0 || slot >= warehouseCapacity()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(warehouseDisplaySlots_.size()); ++i) {
        if (warehouseDisplaySlots_[static_cast<std::size_t>(i)] == slot) {
            return i;
        }
    }
    return -1;
}

void Game::assignWarehouseEntryToStorageSlot(int entryIndex, int slot)
{
    syncWarehouseDisplaySlots();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseDisplaySlots_.size()) || slot < 0 || slot >= warehouseCapacity()) {
        return;
    }
    for (int i = 0; i < static_cast<int>(warehouseDisplaySlots_.size()); ++i) {
        if (i != entryIndex && warehouseDisplaySlots_[static_cast<std::size_t>(i)] == slot) {
            std::swap(warehouseDisplaySlots_[static_cast<std::size_t>(i)], warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)]);
            return;
        }
    }
    warehouseDisplaySlots_[static_cast<std::size_t>(entryIndex)] = slot;
}

void Game::removeWarehouseDisplaySlotAtEntryIndex(int entryIndex)
{
    syncWarehouseDisplaySlots();
    if (entryIndex < 0 || entryIndex >= static_cast<int>(warehouseDisplaySlots_.size())) {
        return;
    }
    warehouseDisplaySlots_.erase(warehouseDisplaySlots_.begin() + entryIndex);
}

std::string Game::storageEntryLabel(StorageEntry entry, bool warehouseEntry) const
{
    char buffer[192];
    if (entry.kind == StorageEntryKind::Stack) {
        const InventoryObjectStack& stack = warehouseEntry
            ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)]
            : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)];
        const std::string name = itemDisplayName(stack.item.name, stack.item.durability == 0);
        std::snprintf(buffer, sizeof(buffer), "%s x%d", name.c_str(), stack.count);
        return buffer;
    }

    const InventoryObjectInstance& instance = warehouseEntry
        ? warehouseObjectInstances_[static_cast<std::size_t>(entry.index)]
        : inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    const std::string name = itemDisplayName(instance.item.name, instance.instance.isBroken);
    std::snprintf(buffer, sizeof(buffer), "%s %sLv.%d",
        name.c_str(),
        instance.instance.protectionEnabled ? "[保護] " : "",
        instance.instance.enhanceLevel);
    return buffer;
}

const ItemData* Game::storageEntryItem(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind == StorageEntryKind::Stack) {
        return warehouseEntry
            ? &warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].item
            : &inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].item;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].item
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].item;
}

const ItemInstance* Game::storageEntryInstance(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Instance) {
        return nullptr;
    }
    return warehouseEntry
        ? &warehouseObjectInstances_[static_cast<std::size_t>(entry.index)].instance
        : &inventory_.objectInstances()[static_cast<std::size_t>(entry.index)].instance;
}

int Game::storageEntryStackCount(StorageEntry entry, bool warehouseEntry) const
{
    if (entry.kind != StorageEntryKind::Stack) {
        return 1;
    }
    return warehouseEntry
        ? warehouseObjectStacks_[static_cast<std::size_t>(entry.index)].count
        : inventory_.objectStacks()[static_cast<std::size_t>(entry.index)].count;
}

Game::StorageTransferTarget Game::storageDepositTargetForSourceSlot(int source, int slotIndex) const
{
    StorageTransferTarget target{};
    target.slotIndex = slotIndex;
    const int clampedSource = std::clamp(source, 0, BaseItemSourceCount - 1);
    target.source = static_cast<BaseItemSource>(clampedSource);

    if (target.source == BaseItemSource::Backpack) {
        if (slotIndex < 0 || slotIndex >= inventory_.screenSlotCount()) {
            return target;
        }
        if (inventory_.screenObjectStackAt(slotIndex) != nullptr ||
            inventory_.screenObjectInstanceAt(slotIndex) != nullptr) {
            target.valid = true;
        }
        return target;
    }

    if (!baseItemSourceIsRing(clampedSource)) {
        return target;
    }

    target.ringIndex = ringIndexFromBaseItemSource(clampedSource);
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return target;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (slotIndex < 0 || slotIndex >= static_cast<int>(ringItems.size())) {
        return target;
    }
    target.ringItemIndex = slotIndex;
    target.valid = true;
    return target;
}

Game::StorageTransferTarget Game::storageDepositTargetForScreenSlot(int slotIndex) const
{
    return storageDepositTargetForSourceSlot(baseStorageDepositSource_, slotIndex);
}

Game::StorageTransferTarget Game::storageWithdrawTargetForSlot(int slotIndex) const
{
    StorageTransferTarget target{};
    target.source = BaseItemSource::Warehouse;
    target.slotIndex = slotIndex;
    const std::optional<StorageEntry> entry = warehouseEntryForPageSlot(
        slotIndex,
        baseStorageWarehousePage_,
        StorageWithdrawSlotCount);
    if (!entry) {
        return target;
    }
    target.storageEntry = *entry;
    target.warehouseEntry = true;
    target.valid = true;
    return target;
}

bool Game::storageTransferTargetAvailable(StorageTransferTarget target) const
{
    if (!target.valid) {
        return false;
    }
    if (target.source == BaseItemSource::Backpack) {
        if (inventory_.screenObjectStackAt(target.slotIndex) != nullptr) {
            return true;
        }
        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            return !inventory_.isStaffEquipped(instance->instance.instanceId);
        }
        return false;
    }
    if (target.source == BaseItemSource::Warehouse) {
        if (target.storageEntry.kind == StorageEntryKind::Stack) {
            return storageEntryStackCount(target.storageEntry, true) > 0;
        }
        return storageEntryInstance(target.storageEntry, true) != nullptr;
    }
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return false;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return false;
    }
    return !ringItems[static_cast<std::size_t>(target.ringItemIndex)].objectId.empty();
}

bool Game::storageTransferTargetIsStack(StorageTransferTarget target) const
{
    if (!target.valid) {
        return false;
    }
    if (target.source == BaseItemSource::Backpack) {
        return inventory_.screenObjectStackAt(target.slotIndex) != nullptr;
    }
    if (target.source == BaseItemSource::Warehouse) {
        return target.storageEntry.kind == StorageEntryKind::Stack;
    }
    return false;
}

int Game::storageTransferTargetStackCount(StorageTransferTarget target) const
{
    if (!storageTransferTargetIsStack(target)) {
        return 1;
    }
    if (target.source == BaseItemSource::Backpack) {
        const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex);
        return stack != nullptr ? stack->count : 0;
    }
    if (target.source == BaseItemSource::Warehouse) {
        return storageEntryStackCount(target.storageEntry, true);
    }
    return 1;
}

InventoryUiEntryView Game::storageTransferTargetView(StorageTransferTarget target) const
{
    InventoryUiEntryView view{};
    if (!target.valid) {
        return view;
    }
    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
            view.item = &stack->item;
            view.stackCount = stack->count;
            return view;
        }
        if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
            view.item = &instance->item;
            view.instance = &instance->instance;
            view.stackCount = 1;
            view.equipped = inventory_.isStaffEquipped(instance->instance.instanceId);
        }
        return view;
    }
    if (target.source == BaseItemSource::Warehouse) {
        return storageEntryView(target.storageEntry, true);
    }
    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return view;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return view;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    view.item = objectForRingItem(objectCatalog_, ringItem);
    view.stats = inventoryUiStatsFromRingItem(ringItem);
    view.stackCount = 1;
    return view;
}

void Game::depositStorageTarget(StorageTransferTarget target, int count)
{
    if (!target.valid) {
        baseStatus_ = "しまうアイテムがありません";
        return;
    }

    if (target.source == BaseItemSource::Backpack) {
        if (const InventoryObjectStack* source = inventory_.screenObjectStackAt(target.slotIndex)) {
            const int moveCount = std::clamp(count, 1, std::max(1, source->count));
            auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [source](const InventoryObjectStack& stack) {
                return stack.objectId == source->objectId;
            });
            if (it == warehouseObjectStacks_.end()) {
                if (warehouseUsedSlots() >= warehouseCapacity()) {
                    baseStatus_ = "収納箱がいっぱいです";
                    return;
                }
                syncWarehouseDisplaySlots();
                const int newStackIndex = static_cast<int>(warehouseObjectStacks_.size());
                warehouseDisplaySlots_.insert(warehouseDisplaySlots_.begin() + newStackIndex, -1);
                warehouseObjectStacks_.push_back(InventoryObjectStack{source->item, 0});
                it = warehouseObjectStacks_.end() - 1;
            }
            const std::string objectId = source->objectId;
            if (!inventory_.removeObjectItemCount(objectId, moveCount)) {
                baseStatus_ = "しまえませんでした";
                return;
            }
            it->count += moveCount;
            baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
            baseStatus_ = "収納箱にしまいました";
            return;
        }

        const InventoryObjectInstance* source = inventory_.screenObjectInstanceAt(target.slotIndex);
        if (source == nullptr) {
            baseStatus_ = "しまうアイテムがありません";
            return;
        }
        if (inventory_.isStaffEquipped(source->instance.instanceId)) {
            baseStatus_ = "装備中の杖はしまえません";
            return;
        }
        if (warehouseUsedSlots() >= warehouseCapacity()) {
            baseStatus_ = "収納箱がいっぱいです";
            return;
        }
        InventoryObjectInstance moved;
        if (!inventory_.takeObjectInstance(source->instance.instanceId, moved)) {
            baseStatus_ = "しまえませんでした";
            return;
        }
        warehouseObjectInstances_.push_back(std::move(moved));
        baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
        baseStatus_ = "収納箱にしまいました";
        return;
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        baseStatus_ = "しまうアイテムがありません";
        return;
    }
    std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        baseStatus_ = "しまうアイテムがありません";
        return;
    }
    const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
    if (ringItem.objectId.empty()) {
        baseStatus_ = "このアイテムはしまえません";
        return;
    }
    if (warehouseUsedSlots() >= warehouseCapacity()) {
        baseStatus_ = "収納箱がいっぱいです";
        return;
    }

    const ItemData* object = objectForRingItem(objectCatalog_, ringItem);
    const ItemData missingObject = object == nullptr ? makeMissingItemData(ringItem.objectId) : ItemData{};
    ItemInstance instance = inventoryInstanceFromRingItem(inventory_, objectCatalog_, ringItem);
    warehouseObjectInstances_.push_back(InventoryObjectInstance{
        object != nullptr ? *object : missingObject,
        std::move(instance),
    });
    ringItems.erase(ringItems.begin() + target.ringItemIndex);
    spellRing_.resetBaseWeightToCurrent();
    refreshOrbitEffects();
    baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, static_cast<int>(ringItems.size()) - 1));
    baseStatus_ = "収納箱にしまいました";
}

void Game::withdrawStorageTarget(StorageTransferTarget target, int count)
{
    if (!target.valid || target.source != BaseItemSource::Warehouse) {
        baseStatus_ = "取り出すアイテムがありません";
        return;
    }

    if (target.storageEntry.kind == StorageEntryKind::Stack) {
        if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectStacks_.size())) {
            baseStatus_ = "取り出すアイテムがありません";
            return;
        }
        InventoryObjectStack& stack = warehouseObjectStacks_[static_cast<std::size_t>(target.storageEntry.index)];
        const int moveCount = std::clamp(count, 1, std::max(1, stack.count));
        const std::string objectId = stack.objectId;
        if (!inventory_.canAddObjectItem(objectCatalog_, objectId)) {
            baseStatus_ = "リュックがいっぱいです";
            return;
        }
        for (int i = 0; i < moveCount; ++i) {
            if (!inventory_.addObjectItem(objectCatalog_, objectId)) {
                baseStatus_ = "リュックがいっぱいです";
                return;
            }
        }
        stack.count -= moveCount;
        if (stack.count <= 0) {
            removeWarehouseDisplaySlotAtEntryIndex(target.storageEntry.index);
            warehouseObjectStacks_.erase(warehouseObjectStacks_.begin() + target.storageEntry.index);
        }
        baseStorageWithdrawSelection_ = std::clamp(baseStorageWithdrawSelection_, 0, StorageWithdrawSlotCount - 1);
        baseStatus_ = "リュックに取り出しました";
        return;
    }

    if (target.storageEntry.index < 0 || target.storageEntry.index >= static_cast<int>(warehouseObjectInstances_.size())) {
        baseStatus_ = "取り出すアイテムがありません";
        return;
    }
    InventoryObjectInstance moved = warehouseObjectInstances_[static_cast<std::size_t>(target.storageEntry.index)];
    if (!inventory_.addObjectInstance(objectCatalog_, moved.instance)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index);
    warehouseObjectInstances_.erase(warehouseObjectInstances_.begin() + target.storageEntry.index);
    baseStorageWithdrawSelection_ = std::clamp(baseStorageWithdrawSelection_, 0, StorageWithdrawSlotCount - 1);
    baseStatus_ = "リュックに取り出しました";
}

void Game::depositAllStorageItems()
{
    int storedCount = 0;
    int skippedFullCount = 0;
    int skippedStaffCount = 0;
    bool ringChanged = false;

    const auto findWarehouseStack = [this](std::string_view objectId) {
        return std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [objectId](const InventoryObjectStack& stack) {
            return stack.objectId == objectId;
        });
    };

    const std::vector<InventoryObjectStack> backpackStacks = inventory_.objectStacks();
    for (const InventoryObjectStack& stack : backpackStacks) {
        if (stack.objectId.empty() || stack.count <= 0) {
            continue;
        }

        const bool existingStack = findWarehouseStack(stack.objectId) != warehouseObjectStacks_.end();
        if (!existingStack && warehouseUsedSlots() >= warehouseCapacity()) {
            skippedFullCount += stack.count;
            continue;
        }

        const int moveCount = stack.count;
        if (!inventory_.removeObjectItemCount(stack.objectId, moveCount)) {
            continue;
        }

        auto it = findWarehouseStack(stack.objectId);
        if (it == warehouseObjectStacks_.end()) {
            warehouseObjectStacks_.push_back(InventoryObjectStack{stack.item, moveCount});
        } else {
            it->item = stack.item;
            it->count += moveCount;
        }
        storedCount += moveCount;
    }

    const std::vector<InventoryObjectInstance> backpackInstances = inventory_.objectInstances();
    for (const InventoryObjectInstance& instance : backpackInstances) {
        if (inventory_.isStaffEquipped(instance.instance.instanceId)) {
            ++skippedStaffCount;
            continue;
        }
        if (warehouseUsedSlots() >= warehouseCapacity()) {
            ++skippedFullCount;
            continue;
        }
        InventoryObjectInstance moved;
        if (inventory_.takeObjectInstance(instance.instance.instanceId, moved)) {
            warehouseObjectInstances_.push_back(std::move(moved));
            ++storedCount;
        }
    }

    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
        for (int itemIndex = static_cast<int>(ringItems.size()) - 1; itemIndex >= 0; --itemIndex) {
            const SpellRingItem ringItem = ringItems[static_cast<std::size_t>(itemIndex)];
            if (ringItem.objectId.empty()) {
                continue;
            }
            if (warehouseUsedSlots() >= warehouseCapacity()) {
                ++skippedFullCount;
                continue;
            }

            const ItemData* object = objectForRingItem(objectCatalog_, ringItem);
            const ItemData missingObject = object == nullptr ? makeMissingItemData(ringItem.objectId) : ItemData{};
            ItemInstance instance = inventoryInstanceFromRingItem(inventory_, objectCatalog_, ringItem);
            warehouseObjectInstances_.push_back(InventoryObjectInstance{
                object != nullptr ? *object : missingObject,
                std::move(instance),
            });
            ringItems.erase(ringItems.begin() + itemIndex);
            ++storedCount;
            ringChanged = true;
        }
    }

    syncWarehouseDisplaySlots();
    if (ringChanged) {
        spellRing_.resetBaseWeightToCurrent();
        refreshOrbitEffects();
    }
    syncEncyclopediaFromInventoryAndRing();

    if (storedCount <= 0) {
        if (skippedFullCount > 0) {
            baseStatus_ = "収納箱がいっぱいです";
        } else if (skippedStaffCount > 0) {
            baseStatus_ = "装備中の杖以外にしまう物がありません";
        } else {
            baseStatus_ = "しまうアイテムがありません";
        }
        return;
    }

    baseStatus_ = std::to_string(storedCount) + "個しまいました";
    if (skippedFullCount > 0) {
        baseStatus_ += " / 満杯で" + std::to_string(skippedFullCount) + "個残りました";
    }
    if (skippedStaffCount > 0) {
        baseStatus_ += " / 装備中の杖は残しました";
    }
}

void Game::prepareRingPresetFromWarehouse(int presetIndex)
{
    if (!ringPresets_.registered(presetIndex)) {
        baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "は未登録です";
        return;
    }

    const std::vector<RingPresetItem> missingItems = ringPresets_.missingItemsForPreset(
        presetIndex,
        inventory_,
        spellRing_,
        objectCatalog_);
    if (missingItems.empty()) {
        baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "の必要アイテムは手元にあります";
        return;
    }

    struct WarehousePresetPick {
        bool instance = false;
        std::string objectId;
        std::string instanceId;
    };

    std::vector<WarehousePresetPick> picks;
    std::vector<bool> usedInstances(warehouseObjectInstances_.size(), false);
    std::vector<int> usedStackCounts(warehouseObjectStacks_.size(), 0);
    int notFoundCount = 0;
    constexpr int NoWarehousePresetMatchScore = std::numeric_limits<int>::max() / 8;

    for (const RingPresetItem& missing : missingItems) {
        int bestScore = std::numeric_limits<int>::max();
        WarehousePresetPick bestPick{};
        int bestIndex = -1;

        for (int i = 0; i < static_cast<int>(warehouseObjectInstances_.size()); ++i) {
            if (usedInstances[static_cast<std::size_t>(i)]) {
                continue;
            }
            const InventoryObjectInstance& candidate = warehouseObjectInstances_[static_cast<std::size_t>(i)];
            const int score = ringPresetInstanceMatchScore(missing, candidate.instance);
            if (score >= NoWarehousePresetMatchScore) {
                continue;
            }
            if (score < bestScore) {
                bestScore = score;
                bestIndex = i;
                bestPick = WarehousePresetPick{
                    .instance = true,
                    .objectId = candidate.instance.objectId,
                    .instanceId = candidate.instance.instanceId,
                };
            }
        }

        for (int i = 0; i < static_cast<int>(warehouseObjectStacks_.size()); ++i) {
            const InventoryObjectStack& candidate = warehouseObjectStacks_[static_cast<std::size_t>(i)];
            if (candidate.count <= usedStackCounts[static_cast<std::size_t>(i)]) {
                continue;
            }
            const int score = ringPresetStackMatchScore(missing, candidate.item);
            if (score >= NoWarehousePresetMatchScore) {
                continue;
            }
            if (score < bestScore) {
                bestScore = score;
                bestIndex = i;
                bestPick = WarehousePresetPick{
                    .instance = false,
                    .objectId = candidate.objectId,
                };
            }
        }

        if (bestIndex < 0) {
            ++notFoundCount;
            continue;
        }
        if (bestPick.instance) {
            usedInstances[static_cast<std::size_t>(bestIndex)] = true;
        } else {
            ++usedStackCounts[static_cast<std::size_t>(bestIndex)];
        }
        picks.push_back(std::move(bestPick));
    }

    int withdrawnCount = 0;
    int fullCount = 0;
    int vanishedCount = 0;
    for (const WarehousePresetPick& pick : picks) {
        if (pick.instance) {
            const auto it = std::find_if(warehouseObjectInstances_.begin(), warehouseObjectInstances_.end(), [&pick](const InventoryObjectInstance& entry) {
                return entry.instance.instanceId == pick.instanceId;
            });
            if (it == warehouseObjectInstances_.end()) {
                ++vanishedCount;
                continue;
            }
            if (!inventory_.addObjectInstance(objectCatalog_, it->instance)) {
                ++fullCount;
                continue;
            }
            const int instanceIndex = static_cast<int>(std::distance(warehouseObjectInstances_.begin(), it));
            removeWarehouseDisplaySlotAtEntryIndex(static_cast<int>(warehouseObjectStacks_.size()) + instanceIndex);
            warehouseObjectInstances_.erase(it);
            ++withdrawnCount;
            continue;
        }

        const auto it = std::find_if(warehouseObjectStacks_.begin(), warehouseObjectStacks_.end(), [&pick](const InventoryObjectStack& stack) {
            return stack.objectId == pick.objectId && stack.count > 0;
        });
        if (it == warehouseObjectStacks_.end()) {
            ++vanishedCount;
            continue;
        }
        if (!inventory_.addObjectItem(objectCatalog_, pick.objectId)) {
            ++fullCount;
            continue;
        }
        --it->count;
        if (it->count <= 0) {
            const int stackIndex = static_cast<int>(std::distance(warehouseObjectStacks_.begin(), it));
            removeWarehouseDisplaySlotAtEntryIndex(stackIndex);
            warehouseObjectStacks_.erase(it);
        }
        ++withdrawnCount;
    }

    syncWarehouseDisplaySlots();
    syncEncyclopediaFromInventoryAndRing();

    if (withdrawnCount <= 0) {
        if (fullCount > 0) {
            baseStatus_ = "リュックがいっぱいで取り出せません";
        } else {
            baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "の不足分は収納箱にありません";
        }
    } else {
        baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "ぶんを" + std::to_string(withdrawnCount) + "個取り出しました";
    }
    if (notFoundCount > 0) {
        baseStatus_ += " / 収納箱になし " + std::to_string(notFoundCount);
    }
    if (fullCount > 0) {
        baseStatus_ += " / リュック満杯 " + std::to_string(fullCount);
    }
    if (vanishedCount > 0) {
        baseStatus_ += " / 取り出せず " + std::to_string(vanishedCount);
    }
}

int Game::upgradeCost(int index) const
{
    switch (index) {
    case 0: return 150 + warehouseCapacityLevel_ * 100;
    case 1: return 120 + (merchantUpgradeLevel_ - 1) * 120;
    case 2: return 180 + processingUnlockLevel_ * 140;
    case 3: return 300;
    case 4: return 100 + maxHpUpgradeLevel_ * 50;
    case 5: return 150 + ringRadiusUpgradeLevel_ * 75;
    case 6: return 150 + ringSpeedUpgradeLevel_ * 75;
    case 7: return 120 + collectionRangeUpgradeLevel_ * 80;
    default: return 0;
    }
}

MaterialType Game::upgradeMaterialType(int index) const
{
    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
        return MaterialType::OldWoodBuildingMaterial;
    case 4:
    case 5:
    case 6:
    case 7:
        return MaterialType::ManaDrop;
    default:
        return MaterialType::OldWoodBuildingMaterial;
    }
}

int Game::upgradeMaterialCost(int index) const
{
    switch (index) {
    case 0: return warehouseCapacityLevel_ + 2;
    case 1: return merchantUpgradeLevel_ + 1;
    case 2: return processingUnlockLevel_ + 2;
    case 3: return ringWorkshopUnlocked_ ? 0 : 5;
    case 4: return maxHpUpgradeLevel_ + 1;
    case 5: return ringRadiusUpgradeLevel_ + 1;
    case 6: return ringSpeedUpgradeLevel_ + 1;
    case 7: return collectionRangeUpgradeLevel_ + 1;
    default: return 0;
    }
}

const char* Game::upgradeName(int index) const
{
    switch (index) {
    case 0: return "倉庫容量強化";
    case 1: return "商人機能強化";
    case 2: return "作業台機能解禁";
    case 3: return "リング工房解禁";
    case 4: return "最大HPアップ";
    case 5: return "リング半径アップ";
    case 6: return "リング速度アップ";
    case 7: return "収集術式";
    default: return "";
    }
}

int Game::upgradeLevel(int index) const
{
    switch (index) {
    case 0: return warehouseCapacityLevel_ + 1;
    case 1: return merchantUpgradeLevel_;
    case 2: return processingUnlockLevel_;
    case 3: return ringWorkshopUnlocked_ ? 1 : 0;
    case 4: return maxHpUpgradeLevel_;
    case 5: return ringRadiusUpgradeLevel_;
    case 6: return ringSpeedUpgradeLevel_;
    case 7: return collectionRangeUpgradeLevel_;
    default: return 0;
    }
}

int Game::upgradeMaxLevel(int index) const
{
    switch (index) {
    case 0: return 5;
    case 1: return 7;
    case 2: return 5;
    case 3: return 1;
    case 4:
    case 5:
    case 6:
    case 7:
        return 5;
    default:
        return 0;
    }
}

bool Game::upgradeImplemented(int index) const
{
    return index >= 0 && index <= 7;
}

bool Game::upgradeMaxed(int index) const
{
    const int maxLevel = upgradeMaxLevel(index);
    return maxLevel <= 0 || upgradeLevel(index) >= maxLevel;
}

void Game::buyUpgrade(int index)
{
    if (!upgradeImplemented(index)) {
        baseStatus_ = "この強化枠は未実装です";
        return;
    }
    if (upgradeMaxed(index)) {
        baseStatus_ = "強化上限です";
        return;
    }
    const int cost = upgradeCost(index);
    if (cost <= 0) {
        return;
    }
    if (money_ < cost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    const MaterialType materialType = upgradeMaterialType(index);
    const int materialCost = upgradeMaterialCost(index);
    if (materialCost > 0 && inventory_.materialCount(materialType) < materialCost) {
        baseStatus_ = std::string(materialTypeDisplayName(materialType)) + "が足りません";
        return;
    }

    const int beforeLevel = upgradeLevel(index);
    money_ -= cost;
    if (materialCost > 0) {
        const bool spent = inventory_.materials().spend(materialType, materialCost);
        (void)spent;
    }
    switch (index) {
    case 0:
        ++warehouseCapacityLevel_;
        break;
    case 1:
        ++merchantUpgradeLevel_;
        refreshMerchantStock(true);
        break;
    case 2:
        ++processingUnlockLevel_;
        break;
    case 3:
        ringWorkshopUnlocked_ = true;
        break;
    case 4:
        ++maxHpUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 5:
        ++ringRadiusUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 6:
        ++ringSpeedUpgradeLevel_;
        applyPermanentUpgrades();
        break;
    case 7:
        ++collectionRangeUpgradeLevel_;
        break;
    default:
        break;
    }
    const int afterLevel = upgradeLevel(index);
    baseStatus_.clear();
    openUiResultDialog(
        baseResultDialog_,
        "強化完了",
        {
            std::string(baseUpgradeResultSubject(index)) + "を強化しました",
            baseUpgradeResultChangeLine(index, beforeLevel, afterLevel),
        });
}

void Game::openRingWorkshop()
{
    if (!ringWorkshopUnlocked_) {
        baseStatus_ = "リング工房はまだ解禁されていません";
        return;
    }
    baseRingWorkshopActive_ = true;
    baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
    baseRingWorkshopSelection_ = 0;
    baseRingWorkshopRingIndex_ = std::clamp(spellRing_.activeRingIndex(), 0, SpellRingCount - 1);
    baseRingWorkshopRingTabs_ = {};
    resetRingWorkshopDraft();
    baseStatus_.clear();
}

void Game::resetRingWorkshopDraft()
{
    ringWorkshopDraftUpgradePoints_ = levelRingUpgradePoints_;
    ringWorkshopRespecSource_.reset();
}

int Game::ringLevelUpgradePointTotal() const
{
    return majo::ringLevelUpgradePointTotal(levelRingUpgradePoints_);
}

bool Game::ringWorkshopRespecChanged() const
{
    return ringWorkshopDraftUpgradePoints_ != levelRingUpgradePoints_;
}

int Game::ringWorkshopRespecMoneyCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    return 80 + ringLevelUpgradePointTotal() * 20;
}

int Game::ringWorkshopRespecMoonCost() const
{
    if (!ringWorkshopRespecChanged()) {
        return 0;
    }
    return 1 + ringLevelUpgradePointTotal() / 3;
}

bool Game::adjustRingWorkshopRespec(RingLevelUpgradeSelection from, RingLevelUpgradeSelection to)
{
    if (ringLevelUpgradePointTotal() <= 0) {
        baseStatus_ = "再調整できるリング強化ポイントがありません";
        return false;
    }
    from.ringIndex = std::clamp(from.ringIndex, 0, SpellRingCount - 1);
    to.ringIndex = std::clamp(to.ringIndex, 0, SpellRingCount - 1);
    if (sameRingLevelUpgradeSelection(from, to)) {
        ringWorkshopRespecSource_.reset();
        return false;
    }

    RingLevelUpgradePoints& fromRingPoints = ringWorkshopDraftUpgradePoints_[static_cast<std::size_t>(from.ringIndex)];
    RingLevelUpgradePoints& toRingPoints = ringWorkshopDraftUpgradePoints_[static_cast<std::size_t>(to.ringIndex)];
    int& fromPoints = ringLevelUpgradePointRef(fromRingPoints, from.kind);
    int& toPoints = ringLevelUpgradePointRef(toRingPoints, to.kind);
    if (fromPoints <= 0) {
        baseStatus_ = "リング" + std::to_string(from.ringIndex + 1) + " " +
            ringLevelUpgradeKindName(from.kind) + "から移せるポイントがありません";
        return false;
    }
    --fromPoints;
    ++toPoints;
    ringWorkshopRespecSource_.reset();
    baseStatus_ = "配分案を変更しました。確定で支払います";
    return true;
}

void Game::confirmRingWorkshopRespec()
{
    if (!ringWorkshopRespecChanged()) {
        baseStatus_ = "配分は変更されていません";
        return;
    }
    const int moneyCost = ringWorkshopRespecMoneyCost();
    const int moonCost = ringWorkshopRespecMoonCost();
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    for (RingLevelUpgradePoints& points : ringWorkshopDraftUpgradePoints_) {
        points = clampedRingLevelUpgradePoints(points);
    }
    levelRingUpgradePoints_ = ringWorkshopDraftUpgradePoints_;
    ringWorkshopRespecSource_.reset();
    applyPermanentUpgrades();
    baseStatus_ = "リング強化の配分を再調整しました";
}

const char* Game::ringWorkshopUpgradeName(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return "初期リング半径強化";
    case RingWorkshopUpgrade::InitialSpeed:
        return "初期リング速度強化";
    case RingWorkshopUpgrade::ShiftDistance:
        return "ずらし距離強化";
    }
    return "";
}

int Game::ringWorkshopUpgradeLevel(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return workshopInitialRadiusLevel_;
    case RingWorkshopUpgrade::InitialSpeed:
        return workshopInitialSpeedLevel_;
    case RingWorkshopUpgrade::ShiftDistance:
        return workshopShiftDistanceLevel_;
    }
    return 0;
}

int Game::ringWorkshopUpgradeMaxLevel(RingWorkshopUpgrade) const
{
    return 5;
}

int Game::ringWorkshopUpgradeMoneyCost(RingWorkshopUpgrade upgrade) const
{
    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    return 120 + level * 90;
}

int Game::ringWorkshopUpgradeMoonCost(RingWorkshopUpgrade upgrade) const
{
    const int level = ringWorkshopUpgradeLevel(upgrade);
    if (level >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return 0;
    }
    return level + 1;
}

float Game::ringWorkshopUpgradeCurrentValue(RingWorkshopUpgrade upgrade) const
{
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        return effectiveInitialRingRadiusForRing(0, levelRingUpgradePoints_[0].radius);
    case RingWorkshopUpgrade::InitialSpeed:
        return effectiveInitialRingSpeedForRing(0, levelRingUpgradePoints_[0].speed);
    case RingWorkshopUpgrade::ShiftDistance:
        return effectiveRingShiftDistance();
    }
    return 0.0f;
}

float Game::ringWorkshopUpgradeNextValue(RingWorkshopUpgrade upgrade) const
{
    const int currentLevel = ringWorkshopUpgradeLevel(upgrade);
    if (currentLevel >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        return ringWorkshopUpgradeCurrentValue(upgrade);
    }
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius: {
        const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringRadiusUpgradeLevel_) * 0.08f;
        const float workshopMultiplier = 1.0f + static_cast<float>(currentLevel + 1) * 0.05f;
        const float levelMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(levelRingUpgradePoints_[0].radius);
        return balance_.spellRingRadius * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
    }
    case RingWorkshopUpgrade::InitialSpeed: {
        const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringSpeedUpgradeLevel_) * 0.08f;
        const float workshopMultiplier = 1.0f + static_cast<float>(currentLevel + 1) * 0.05f;
        const float levelMultiplier = SpellRingSystem::levelScaleMultiplierForPoints(levelRingUpgradePoints_[0].speed);
        return balance_.spellRingSpeed * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
    }
    case RingWorkshopUpgrade::ShiftDistance:
        return balance_.spellRingShiftDistance + static_cast<float>(currentLevel + 1) * 8.0f;
    }
    return 0.0f;
}

void Game::buyRingWorkshopUpgrade(RingWorkshopUpgrade upgrade)
{
    if (ringWorkshopUpgradeLevel(upgrade) >= ringWorkshopUpgradeMaxLevel(upgrade)) {
        baseStatus_ = "この強化は上限です";
        return;
    }
    const int moneyCost = ringWorkshopUpgradeMoneyCost(upgrade);
    const int moonCost = ringWorkshopUpgradeMoonCost(upgrade);
    if (money_ < moneyCost) {
        baseStatus_ = "所持金が足りません";
        return;
    }
    if (inventory_.materialCount(MaterialType::MoonFragment) < moonCost) {
        baseStatus_ = "月のカケラが足りません";
        return;
    }
    money_ -= moneyCost;
    const bool spent = inventory_.materials().spend(MaterialType::MoonFragment, moonCost);
    (void)spent;
    switch (upgrade) {
    case RingWorkshopUpgrade::InitialRadius:
        ++workshopInitialRadiusLevel_;
        break;
    case RingWorkshopUpgrade::InitialSpeed:
        ++workshopInitialSpeedLevel_;
        break;
    case RingWorkshopUpgrade::ShiftDistance:
        ++workshopShiftDistanceLevel_;
        break;
    }
    applyPermanentUpgrades();
    resetRingWorkshopDraft();
    baseStatus_ = "リング工房強化を行いました";
}

void Game::openBookshelf()
{
    baseBookshelfActive_ = true;
    bookshelfPage_ = BookshelfPage::Menu;
    bookshelfSelection_ = 0;
    bookshelfScrollOffset_ = 0.0f;
    bookshelfScrollState_ = {};
    baseStatus_.clear();
    syncEncyclopediaFromInventoryAndRing();
}

void Game::syncEncyclopediaFromInventoryAndRing()
{
    std::unordered_map<std::string, int> ownedCounts;
    std::unordered_map<std::string, const ObjectDefinition*> ownedObjects;
    const auto addOwnedObject = [&ownedCounts, &ownedObjects](const ObjectDefinition& object, int count) {
        if (object.id.empty() || count <= 0) {
            return;
        }
        ownedCounts[object.id] += count;
        ownedObjects.try_emplace(object.id, &object);
    };

    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        if (!stack.objectId.empty() && stack.count > 0) {
            addOwnedObject(stack.item, stack.count);
        }
    }
    for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
        if (!objectInstance.item.id.empty()) {
            addOwnedObject(objectInstance.item, 1);
        }
    }
    for (const InventoryObjectStack& stack : warehouseObjectStacks_) {
        if (!stack.objectId.empty() && stack.count > 0) {
            addOwnedObject(stack.item, stack.count);
        }
    }
    for (const InventoryObjectInstance& objectInstance : warehouseObjectInstances_) {
        if (!objectInstance.item.id.empty()) {
            addOwnedObject(objectInstance.item, 1);
        }
    }
    for (const auto& [objectId, count] : ownedCounts) {
        const int suppressCount = [&]() {
            const auto it = encyclopediaOwnedSyncSuppressCounts_.find(objectId);
            return it == encyclopediaOwnedSyncSuppressCounts_.end() ? 0 : it->second;
        }();
        if (count <= suppressCount) {
            continue;
        }
        const auto objectIt = ownedObjects.find(objectId);
        if (objectIt != ownedObjects.end() && objectIt->second != nullptr) {
            encyclopedia_.noteItemObtained(*objectIt->second, player_.position);
        }
    }

    std::unordered_map<std::string, int> ringCounts;
    std::unordered_map<std::string, const ObjectDefinition*> ringObjects;
    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->objectId.empty()) {
            continue;
        }
        const ObjectDefinition* object = objectCatalog_.registry.findById(itemPtr->objectId);
        if (object != nullptr) {
            ringCounts[object->id] += 1;
            ringObjects.try_emplace(object->id, object);
        }
    }
    for (const auto& [objectId, count] : ringCounts) {
        const int suppressCount = [&]() {
            const auto it = encyclopediaRingSyncSuppressCounts_.find(objectId);
            return it == encyclopediaRingSyncSuppressCounts_.end() ? 0 : it->second;
        }();
        if (count <= suppressCount) {
            continue;
        }
        const auto objectIt = ringObjects.find(objectId);
        if (objectIt != ringObjects.end() && objectIt->second != nullptr) {
            encyclopedia_.noteItemEquipped(*objectIt->second, player_.position);
        }
    }
}

void Game::captureEncyclopediaSyncSuppressState()
{
    encyclopediaOwnedSyncSuppressCounts_.clear();
    encyclopediaRingSyncSuppressCounts_.clear();

    const auto addOwnedId = [this](std::string_view objectId, int count) {
        if (objectId.empty() || count <= 0) {
            return;
        }
        encyclopediaOwnedSyncSuppressCounts_[std::string(objectId)] += count;
    };
    for (const InventoryObjectStack& stack : inventory_.objectStacks()) {
        addOwnedId(stack.objectId, stack.count);
    }
    for (const InventoryObjectInstance& objectInstance : inventory_.objectInstances()) {
        addOwnedId(objectInstance.item.id, 1);
    }

    const std::vector<const SpellRingItem*> runtimeItems = spellRing_.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->objectId.empty()) {
            continue;
        }
        encyclopediaRingSyncSuppressCounts_[itemPtr->objectId] += 1;
    }
}

void Game::applyEffectDiscoveries(const std::vector<EffectDiscoveryEvent>& discoveries)
{
    encyclopedia_.noteEffectEvents(discoveries, objectCatalog_);
}

void Game::recordObjectObtainedForFirstNotice(
    std::string_view objectId,
    std::string_view instanceId,
    bool protectable,
    Vec2 position)
{
    if (objectId.empty()) {
        return;
    }
    const ObjectDefinition* object = objectCatalog_.registry.findById(objectId);
    if (object == nullptr) {
        return;
    }
    if (!encyclopedia_.noteItemObtained(*object, position)) {
        return;
    }

    firstItemAcquisitionNotices_.push_back(FirstItemAcquisitionNotice{
        .objectId = std::string(objectId),
        .instanceId = std::string(instanceId),
        .protectable = protectable && !instanceId.empty(),
    });
}

bool Game::firstItemAcquisitionNoticeActive() const
{
    return !firstItemAcquisitionNotices_.empty();
}

void Game::closeFirstItemAcquisitionNotice()
{
    if (!firstItemAcquisitionNotices_.empty()) {
        firstItemAcquisitionNotices_.pop_front();
    }
}

void Game::addStoryFlag(std::string flag)
{
    if (flag.empty()) {
        return;
    }
    if (std::find(storyFlags_.begin(), storyFlags_.end(), flag) == storyFlags_.end()) {
        storyFlags_.push_back(std::move(flag));
    }
}

bool Game::hasStoryFlag(std::string_view flag) const
{
    return std::find(storyFlags_.begin(), storyFlags_.end(), std::string(flag)) != storyFlags_.end();
}

void Game::startBaseMonicaDialogue()
{
    baseStatus_.clear();
    if (hasStoryFlag("ending_seen") && startStoryEventForTrigger("monica_base:post_ending")) {
        return;
    }
    const int progress = std::clamp(unlockedStages_, 1, 4);
    if (startStoryEventForTrigger("monica_base:progress_" + std::to_string(progress))) {
        return;
    }
    dialogue_.start(baseMonicaDialogue());
}

bool Game::hasBrokenRingItemForDeparture() const
{
    for (int ringIndex = 0; ringIndex < SpellRingCount; ++ringIndex) {
        for (const SpellRingItem& item : spellRing_.itemsForRing(ringIndex)) {
            if (item.broken()) {
                return true;
            }
        }
    }
    return false;
}

void Game::openBaseMiningStartChoice()
{
    clampCurrentStageToSelectableStages();
    syncWarpStateForCurrentStage();
    baseMiningStartChoiceActive_ = true;
    baseMiningStartSelection_ = unlockedWarpPointCount_ > 0 ? 1 : 0;
    baseWarpPointSelectActive_ = false;
    baseWarpPointSelection_ = 0;
    baseRegenerateConfirm_ = {};
    baseBrokenRingDepartureConfirm_ = {};
    baseStatus_.clear();
}

void Game::maybeQueueStageStartStory()
{
    if (currentStageId_.empty()) {
        return;
    }
    if (dungeonRingIntroActive()) {
        stageStartStoryPendingAfterRingIntro_ = true;
        return;
    }

    stageStartStoryPendingAfterRingIntro_ = false;
    queueStoryEventForCurrentStage("stage_start");
    queueStoryEventForCurrentStage("monica_radio");
}

void Game::placeBasePlayerAtMineExitReturnPoint()
{
    const UiRect fallback = defaultBaseFacilityRect(BaseArea::Outdoor, ringWorkshopUnlocked_, "mine_exit");
    const UiRect mineExitRect = toUiRect(baseFacilityRectFor(BaseArea::Outdoor, "mine_exit", toBaseEditRect(fallback)));
    baseArea_ = BaseArea::Outdoor;
    basePlayerPosition_ = baseFacilitySpawnPosition(mineExitRect, BaseFacilitySpawnSide::Above, balance_.playerRadius);
    const UiRect bounds = baseMapBounds();
    basePlayerPosition_.y = std::clamp(
        basePlayerPosition_.y - BaseMineExitReturnUpOffset,
        bounds.pos.y + balance_.playerRadius,
        bounds.pos.y + bounds.size.y - balance_.playerRadius);
    baseOutdoorPlayerPosition_ = basePlayerPosition_;
    basePlayerFacing_ = {0.0f, 1.0f};
}

std::vector<Game::WarpPoint> Game::selectableWarpPointsForCurrentStageStart() const
{
    std::vector<WarpPoint> points;
    const std::vector<WarpPoint>* source = nullptr;
    const auto retainedStage = dungeonStates_.find(currentStageId_);
    if (retainedStage != dungeonStates_.end() && retainedStage->second.valid) {
        source = &retainedStage->second.warpPoints;
    } else if (!warpPoints_.empty()) {
        source = &warpPoints_;
    }

    if (source != nullptr) {
        for (const WarpPoint& point : *source) {
            if (point.discovered) {
                points.push_back(point);
            }
        }
    }

    if (points.empty() && unlockedWarpPointCount_ > 0 && hasLatestWarpPointPosition_) {
        WarpPoint fallback;
        fallback.stageId = currentStage_ + 1;
        fallback.index = std::max(0, unlockedWarpPointCount_ - 1);
        fallback.position = latestWarpPointPosition_;
        fallback.tilePosition = {
            tileMap_.worldToTile(latestWarpPointPosition_.x),
            tileMap_.worldToTile(latestWarpPointPosition_.y),
        };
        fallback.discovered = true;
        fallback.unlocked = true;
        fallback.snapshotCaptured = true;
        points.push_back(fallback);
    }

    std::sort(points.begin(), points.end(), [](const WarpPoint& left, const WarpPoint& right) {
        return left.index < right.index;
    });
    return points;
}

void Game::placeBasePlayerAtHomeDoorResumePoint()
{
    const UiRect fallback = defaultBaseFacilityRect(BaseArea::Outdoor, ringWorkshopUnlocked_, "home_entrance");
    const UiRect entranceRect = toUiRect(baseFacilityRectFor(BaseArea::Outdoor, "home_entrance", toBaseEditRect(fallback)));
    baseArea_ = BaseArea::Outdoor;
    basePlayerPosition_ = baseFacilitySpawnPosition(
        entranceRect,
        BaseFacilitySpawnSide::Below,
        balance_.playerRadius);
    baseOutdoorPlayerPosition_ = basePlayerPosition_;
    basePlayerFacing_ = {0.0f, -1.0f};
}

const StoryEvent* Game::findStoryEvent(std::string_view id) const
{
    const auto it = std::find_if(storyEvents_.begin(), storyEvents_.end(), [id](const StoryEvent& event) {
        return event.id == id;
    });
    return it == storyEvents_.end() ? nullptr : &*it;
}

const StoryEvent* Game::findStoryEventForTrigger(std::string_view trigger) const
{
    const auto it = std::find_if(storyEvents_.begin(), storyEvents_.end(), [this, trigger](const StoryEvent& event) {
        if (event.trigger != trigger) {
            return false;
        }
        if (event.repeatable) {
            return true;
        }
        return event.onceFlag.empty() ||
            std::find(storyFlags_.begin(), storyFlags_.end(), event.onceFlag) == storyFlags_.end();
    });
    return it == storyEvents_.end() ? nullptr : &*it;
}

std::string Game::currentStageStoryTrigger(std::string_view triggerName) const
{
    if (currentStageId_.empty()) {
        return {};
    }
    return std::string(triggerName) + ":" + currentStageId_;
}

bool Game::queueStoryEventForTrigger(std::string trigger)
{
    if (trigger.empty()) {
        return false;
    }
    if (isTutorialStoryTrigger(trigger) && !pendingStoryTriggers_.empty()) {
        return false;
    }
    if (findStoryEventForTrigger(trigger) == nullptr) {
        return false;
    }
    if (std::find(pendingStoryTriggers_.begin(), pendingStoryTriggers_.end(), trigger) != pendingStoryTriggers_.end()) {
        return true;
    }
    pendingStoryTriggers_.push_back(std::move(trigger));
    return true;
}

bool Game::queueStoryEventForCurrentStage(std::string_view triggerName)
{
    return queueStoryEventForTrigger(currentStageStoryTrigger(triggerName));
}

void Game::updateQueuedStoryEvents()
{
    if (dialogue_.active() ||
        dungeonFocusActive() ||
        pendingStoryTriggers_.empty() ||
        screenTransition_.active() ||
        worldBuildActive() ||
        dungeonRingIntroActive() ||
        mode_ == ScreenMode::OpeningKamishibai ||
        mode_ == ScreenMode::EndingKamishibai ||
        mode_ == ScreenMode::Title ||
        mode_ == ScreenMode::WorldLoading) {
        return;
    }

    while (!pendingStoryTriggers_.empty()) {
        std::string trigger = std::move(pendingStoryTriggers_.front());
        pendingStoryTriggers_.erase(pendingStoryTriggers_.begin());
        if (startStoryEventForTrigger(trigger)) {
            return;
        }
    }
}

bool Game::startStoryEvent(std::string_view id)
{
    const StoryEvent* event = findStoryEvent(id);
    if (event == nullptr) {
        logWarning("[story] event not found: " + std::string(id));
        return false;
    }

    if (!event->onceFlag.empty()) {
        const bool alreadySeen = std::find(storyFlags_.begin(), storyFlags_.end(), event->onceFlag) != storyFlags_.end();
        if (alreadySeen) {
            return false;
        }
        addStoryFlag(event->onceFlag);
    }

    baseStatus_.clear();
    pendingDialogueCompletion_ = {};
    dialogue_.start(event->dialogue);
    return true;
}

bool Game::startStoryEventWithCompletion(std::string_view id, std::function<void()> onComplete)
{
    if (dialogue_.active()) {
        return false;
    }
    if (!startStoryEvent(id)) {
        return false;
    }
    if (!dialogue_.active()) {
        if (onComplete) {
            onComplete();
        }
        return true;
    }
    pendingDialogueCompletion_ = std::move(onComplete);
    return true;
}

bool Game::startDialogueSequenceWithCompletion(DialogueSequence sequence, std::function<void()> onComplete)
{
    if (dialogue_.active()) {
        return false;
    }
    baseStatus_.clear();
    pendingDialogueCompletion_ = std::move(onComplete);
    dialogue_.start(std::move(sequence));
    if (!dialogue_.active()) {
        std::function<void()> callback = std::move(pendingDialogueCompletion_);
        pendingDialogueCompletion_ = {};
        if (callback) {
            callback();
        }
    }
    return true;
}

bool Game::startStoryEventForDebug(std::string_view id)
{
    const StoryEvent* event = findStoryEvent(id);
    if (event == nullptr) {
        logWarning("[story] debug event not found: " + std::string(id));
        return false;
    }

    pendingStoryTrigger_.clear();
    pendingStoryTriggerDelaySeconds_ = 0.0f;
    pendingStoryTriggers_.clear();
    baseStatus_.clear();
    pendingDialogueCompletion_ = {};
    dialogue_.start(event->dialogue);
    logInfo("[story] debug replay: " + event->id);
    return true;
}

bool Game::startStoryEventForTrigger(std::string_view trigger)
{
    const StoryEvent* event = findStoryEventForTrigger(trigger);
    if (event == nullptr) {
        return false;
    }
    return startStoryEvent(event->id);
}

void Game::maybeStartOpeningBaseIntroEvent()
{
    queueStoryEventForTrigger(std::string(IntroTutorialBaseReturnTrigger));
}

void Game::updateBookshelfScreen(const Input& input, UiContext& ui)
{
    const auto itemCountForPage = [this](BookshelfPage page) {
        switch (page) {
        case BookshelfPage::Menu:
            return BookshelfMenuItemCount;
        case BookshelfPage::Items:
            return static_cast<int>(objectCatalog_.objects.size());
        case BookshelfPage::Enemies:
            return static_cast<int>(enemyCatalog_.enemies.size());
        }
        return 0;
    };
    const auto pageForMenuSelection = [](int selection) {
        return selection == 1 ? BookshelfPage::Enemies : BookshelfPage::Items;
    };
    const auto openSelectedPage = [&]() {
        bookshelfPage_ = pageForMenuSelection(bookshelfSelection_);
        bookshelfSelection_ = 0;
        bookshelfScrollOffset_ = 0.0f;
        bookshelfScrollState_ = {};
    };

    const UiRect panel = bookshelfPage_ == BookshelfPage::Menu
        ? bookshelfMenuPanelRect()
        : merchantPanelRect();
    if (uiCancelRequested(baseCancelState_, input, ui, panel)) {
        if (bookshelfPage_ == BookshelfPage::Menu) {
            baseBookshelfActive_ = false;
            baseStatus_.clear();
        } else {
            bookshelfPage_ = BookshelfPage::Menu;
            bookshelfSelection_ = 0;
            bookshelfScrollOffset_ = 0.0f;
            bookshelfScrollState_ = {};
        }
        return;
    }

    const int itemCount = itemCountForPage(bookshelfPage_);
    if (itemCount <= 0) {
        bookshelfSelection_ = 0;
    } else {
        if (bookshelfPage_ == BookshelfPage::Menu) {
            if (input.pressed(InputAction::MoveUp)) {
                bookshelfSelection_ = (bookshelfSelection_ + itemCount - 1) % itemCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                bookshelfSelection_ = (bookshelfSelection_ + 1) % itemCount;
            }
        } else {
            const InventoryUiGridStyle gridStyle = bookshelfGridStyle();
            const int columns = std::max(1, gridStyle.columns);
            int nextSelection = bookshelfSelection_;
            if (input.pressed(InputAction::MoveLeft)) {
                nextSelection = std::max(0, nextSelection - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                nextSelection = std::min(itemCount - 1, nextSelection + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                nextSelection = std::max(0, nextSelection - columns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                nextSelection = std::min(itemCount - 1, nextSelection + columns);
            }
            if (nextSelection != bookshelfSelection_) {
                bookshelfSelection_ = nextSelection;
                keepInventoryUiGridItemVisible(
                    bookshelfGridViewport(),
                    bookshelfSelection_,
                    itemCount,
                    bookshelfScrollOffset_,
                    gridStyle);
            }
        }
        bookshelfSelection_ = std::clamp(bookshelfSelection_, 0, itemCount - 1);
    }

    if (bookshelfPage_ == BookshelfPage::Menu) {
        const int visibleCount = std::min(BookshelfVisibleRows, itemCount);
        for (int i = 0; i < visibleCount; ++i) {
            const UiRect rect = bookshelfMenuChoiceRect(i);
            if (rect.contains(ui.mouse())) {
                bookshelfSelection_ = i;
            }
            if (ui.pressed(rect)) {
                bookshelfSelection_ = i;
                ui.emitSound(UiSoundEvent::BookOpen);
                openSelectedPage();
                return;
            }
        }
    } else {
        const InventoryUiGridStyle gridStyle = bookshelfGridStyle();
        const UiRect viewport = bookshelfGridViewport();
        const UiScrollAreaLayout layout = updateInventoryUiGrid(
            ui,
            input,
            viewport,
            itemCount,
            bookshelfScrollOffset_,
            gridStyle,
            &bookshelfScrollState_);
        for (int i = 0; i < itemCount; ++i) {
            const UiRect rect = inventoryUiGridSlotRect(layout, i, gridStyle);
            if (!uiScrollAreaRectVisible(layout, rect)) {
                continue;
            }
            if (ui.hovered(rect)) {
                bookshelfSelection_ = i;
            }
            if (ui.pressed(rect)) {
                bookshelfSelection_ = i;
                ui.emitSound(UiSoundEvent::Confirm);
                return;
            }
        }
    }

    if ((input.confirmPressed() || input.useItemPressed()) && bookshelfPage_ == BookshelfPage::Menu) {
        ui.emitSound(UiSoundEvent::BookOpen);
        openSelectedPage();
        return;
    }

    ui.block(panel);
}

void Game::openBaseDiary()
{
    baseDiaryActive_ = true;
    baseDiaryMode_ = BaseDiaryMode::Confirm;
    baseDiarySelection_ = 0;
    baseDiarySummary_ = loadDiarySaveSummaryFromDisk();
    baseDiaryMessage_.clear();
    baseStatus_.clear();
}

void Game::closeBaseDiary()
{
    baseDiaryActive_ = false;
    baseDiaryMode_ = BaseDiaryMode::Confirm;
    baseDiarySelection_ = 0;
    baseDiarySummary_ = {};
    baseDiaryMessage_.clear();
    baseStatus_.clear();
}

void Game::updateBaseDiaryScreen(const Input& input, UiContext& ui)
{
    const UiRect panel = basePanelRect();
    if (uiCancelRequested(baseCancelState_, input, ui, panel)) {
        closeBaseDiary();
        ui.block(panel);
        return;
    }

    if (baseDiaryMode_ == BaseDiaryMode::Saved) {
        const UiRect closeButton = uiResultDialogOkButtonRect(panel);
        if (ui.pressed(closeButton) || input.confirmPressed() || input.useItemPressed()) {
            ui.emitSound(UiSoundEvent::Confirm);
            closeBaseDiary();
            ui.block(panel);
            return;
        }
        ui.block(panel);
        return;
    }

    if (ui.hovered(uiConfirmDialogButtonRect(panel, 0))) {
        baseDiarySelection_ = 0;
    } else if (ui.hovered(uiConfirmDialogButtonRect(panel, 1))) {
        baseDiarySelection_ = 1;
    }
    if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp) || input.activeRingDelta() < 0) {
        baseDiarySelection_ = 1;
    }
    if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown) || input.activeRingDelta() > 0) {
        baseDiarySelection_ = 0;
    }

    const auto saveDiary = [this, &ui]() {
        std::string message;
        if (saveSaveData(message)) {
            ui.emitSound(UiSoundEvent::Confirm);
            baseDiaryMode_ = BaseDiaryMode::Saved;
            baseDiarySelection_ = 0;
            baseDiaryMessage_ = "保存しました。";
            baseDiarySummary_ = currentDiarySaveSummary();
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
            baseDiaryMode_ = BaseDiaryMode::Error;
            baseDiarySelection_ = 0;
            baseDiaryMessage_ = message.empty() ? "セーブに失敗しました。" : message;
        }
    };

    if (ui.pressed(uiConfirmDialogButtonRect(panel, 0))) {
        saveDiary();
        ui.block(panel);
        return;
    }
    if (ui.pressed(uiConfirmDialogButtonRect(panel, 1))) {
        ui.emitSound(UiSoundEvent::Cancel);
        closeBaseDiary();
        ui.block(panel);
        return;
    }
    if (input.confirmPressed() || input.useItemPressed()) {
        if (baseDiarySelection_ == 0) {
            saveDiary();
        } else {
            ui.emitSound(UiSoundEvent::Cancel);
            closeBaseDiary();
        }
        ui.block(panel);
        return;
    }

    ui.block(panel);
}

void Game::updateBaseScreen(const Input& input, UiContext& ui, float dt)
{
    baseRingPreviewAnimationTime_ = std::fmod(baseRingPreviewAnimationTime_ + std::max(0.0f, dt), 3600.0f);
    const float ringPreviewSeconds = baseRingPreviewAnimationTime_;

    updatePlayerFootstepDust(dt);

    if (baseEditEnabled_) {
        updateBaseEditScreen(input, ui, dt);
        return;
    }

    if (baseResultDialog_.open) {
        const UiRect resultPanel = baseResultDialogRect();
        updateUiResultDialog(baseResultDialog_, ui, input, resultPanel);
        ui.block(resultPanel);
        return;
    }

    if (baseStorageQuantityDialog_.open) {
        const UiRect quantityPanel = storageQuantityDialogRect();
        const UiQuantityDialogResult quantityResult = updateUiQuantityDialog(baseStorageQuantityDialog_, ui, input, quantityPanel);
        if (quantityResult == UiQuantityDialogResult::Confirmed) {
            const int quantity = baseStorageQuantityDialog_.value;
            const StorageQuantityPending pending = baseStorageQuantityPending_;
            baseStorageQuantityPending_ = {};
            if (pending.operation == StorageQuantityOperation::Deposit) {
                depositStorageTarget(pending.target, quantity);
            } else if (pending.operation == StorageQuantityOperation::Withdraw) {
                withdrawStorageTarget(pending.target, quantity);
            }
        } else if (quantityResult == UiQuantityDialogResult::Cancelled) {
            baseStorageQuantityPending_ = {};
        }
        ui.block(quantityPanel);
        return;
    }

    if (baseProcessingConfirm_.open) {
        const UiRect confirmPanel = baseProcessingConfirmRect();
        baseProcessingConfirm_.confirmEnabled = processingCommandExecutable(
            baseProcessingConfirmTarget_,
            baseProcessingConfirmMode_);
        const UiConfirmDialogResult result = updateUiConfirmDialog(baseProcessingConfirm_, ui, input, confirmPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            applyProcessingTarget(baseProcessingConfirmTarget_, baseProcessingConfirmMode_);
            baseProcessingConfirmTarget_ = {};
        } else if (result == UiConfirmDialogResult::Cancelled) {
            baseProcessingConfirmTarget_ = {};
            baseStatus_.clear();
        }
        ui.block(confirmPanel);
        return;
    }

    if (baseBrokenRingDepartureConfirm_.open) {
        const UiRect confirmPanel = baseBrokenRingDepartureConfirmRect();
        const UiConfirmDialogResult result = updateUiConfirmDialog(
            baseBrokenRingDepartureConfirm_,
            ui,
            input,
            confirmPanel);
        if (result == UiConfirmDialogResult::Confirmed) {
            openBaseMiningStartChoice();
        } else if (result == UiConfirmDialogResult::Cancelled) {
            baseStatus_.clear();
        }
        ui.block(confirmPanel);
        return;
    }

    if (baseDiaryActive_) {
        updateBaseDiaryScreen(input, ui);
        return;
    }

    if (baseBookshelfActive_) {
        updateBookshelfScreen(input, ui);
        return;
    }

    if (baseRingWorkshopActive_) {
        const UiRect workshopBounds = baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction
            ? ringWorkshopActionDialogRect()
            : ringWorkshopPanelRect();
        const auto closeWorkshop = [this]() {
            baseRingWorkshopActive_ = false;
            baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
            baseRingWorkshopSelection_ = 0;
            resetRingWorkshopDraft();
            baseStatus_.clear();
        };
        const auto returnToWorkshopMenu = [this]() {
            if (baseRingWorkshopMode_ == RingWorkshopMode::Respec) {
                resetRingWorkshopDraft();
            }
            baseRingWorkshopMode_ = RingWorkshopMode::ChooseAction;
            baseRingWorkshopSelection_ = 0;
            baseStatus_.clear();
        };
        if (uiCancelRequested(baseCancelState_, input, ui, workshopBounds)) {
            if (baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction) {
                closeWorkshop();
            } else {
                returnToWorkshopMenu();
            }
            ui.block(workshopBounds);
            return;
        }

        if (baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction) {
            baseRingWorkshopSelection_ = std::clamp(baseRingWorkshopSelection_, 0, RingWorkshopActionCount - 1);
            const auto chooseAction = [this, &ui](int item) {
                ui.emitSound(UiSoundEvent::Confirm);
                if (item == 0) {
                    baseRingWorkshopMode_ = RingWorkshopMode::Respec;
                    baseRingWorkshopSelection_ = 0;
                    baseRingWorkshopRingIndex_ = std::clamp(spellRing_.activeRingIndex(), 0, unlockedRingCount() - 1);
                    baseRingWorkshopRingTabs_ = {};
                    resetRingWorkshopDraft();
                    baseStatus_.clear();
                    return;
                }
                baseRingWorkshopMode_ = RingWorkshopMode::Upgrade;
                baseRingWorkshopSelection_ = 0;
                baseStatus_.clear();
            };
            if (input.pressed(InputAction::MoveUp)) {
                baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + RingWorkshopActionCount - 1) % RingWorkshopActionCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + 1) % RingWorkshopActionCount;
            }
            for (int i = 0; i < RingWorkshopActionCount; ++i) {
                const UiRect rect = ringWorkshopActionChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseRingWorkshopSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseRingWorkshopSelection_ = i;
                    chooseAction(i);
                    ui.block(workshopBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                chooseAction(baseRingWorkshopSelection_);
                ui.block(workshopBounds);
                return;
            }
            ui.block(workshopBounds);
            return;
        }

        if (baseRingWorkshopMode_ == RingWorkshopMode::Respec) {
            constexpr int RespecSelectionCount = RingLevelUpgradeKindCount + 1;
            const int ringCount = unlockedRingCount();
            baseRingWorkshopRingIndex_ = std::clamp(baseRingWorkshopRingIndex_, 0, ringCount - 1);
            baseRingWorkshopSelection_ = std::clamp(baseRingWorkshopSelection_, 0, RespecSelectionCount - 1);

            std::array<UiTabItem, SpellRingCount> ringTabs{};
            std::array<UiRect, SpellRingCount> ringTabRects{};
            std::array<std::string, SpellRingCount> ringTabLabels{};
            for (int i = 0; i < ringCount; ++i) {
                ringTabLabels[static_cast<std::size_t>(i)] = "リング " + std::to_string(i + 1);
                ringTabs[static_cast<std::size_t>(i)] = {ringTabLabels[static_cast<std::size_t>(i)], true};
                ringTabRects[static_cast<std::size_t>(i)] = ringWorkshopRingTabRect(i, ringCount);
            }
            UiTabsInput ringTabsInput{};
            ringTabsInput.focusDelta = input.activeRingDelta();
            const int directRingFocus = input.shortcutSlotPressed();
            if (directRingFocus >= 0 && directRingFocus < ringCount) {
                ringTabsInput.directFocusIndex = directRingFocus;
            }
            ringTabsInput.commit = ringTabsInput.focusDelta != 0 || ringTabsInput.directFocusIndex >= 0;
            const int ringSelection = updateUiTabs(
                baseRingWorkshopRingTabs_,
                ui,
                ringTabsInput,
                baseRingWorkshopRingIndex_,
                ringTabs.data(),
                ringCount,
                ringTabRects.data());
            if (ringSelection >= 0) {
                baseRingWorkshopRingIndex_ = ringSelection;
                ui.block(workshopBounds);
                return;
            }

            int move = 0;
            if (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft) || input.shortcutCursorDelta() < 0) {
                --move;
            }
            if (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight) || input.shortcutCursorDelta() > 0) {
                ++move;
            }
            if (move != 0) {
                baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + move + RespecSelectionCount) % RespecSelectionCount;
            }

            const auto chooseRespecKind = [this, &ui](int kindIndex) {
                const RingLevelUpgradeSelection selection{
                    baseRingWorkshopRingIndex_,
                    ringWorkshopKindForIndex(kindIndex),
                };
                if (!ringWorkshopRespecSource_) {
                    const RingLevelUpgradePoints& points = ringWorkshopDraftUpgradePoints_[static_cast<std::size_t>(selection.ringIndex)];
                    if (ringLevelUpgradePoint(points, selection.kind) <= 0) {
                        ui.emitSound(UiSoundEvent::Cancel);
                        baseStatus_ = "移動元にできるポイントがありません";
                        return;
                    }
                    ringWorkshopRespecSource_ = selection;
                    ui.emitSound(UiSoundEvent::Confirm);
                    baseStatus_ = "移動先を選んでください";
                    return;
                }
                ui.emitSound(UiSoundEvent::Confirm);
                adjustRingWorkshopRespec(*ringWorkshopRespecSource_, selection);
            };

            for (int i = 0; i < RingLevelUpgradeKindCount; ++i) {
                const UiRect rect = ringWorkshopRespecKindRect(i);
                if (rect.contains(ui.mouse())) {
                    baseRingWorkshopSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseRingWorkshopSelection_ = i;
                    chooseRespecKind(i);
                    ui.block(workshopBounds);
                    return;
                }
            }
            const UiRect confirmRect = ringWorkshopRespecConfirmRect();
            if (confirmRect.contains(ui.mouse())) {
                baseRingWorkshopSelection_ = RingLevelUpgradeKindCount;
            }
            if (ui.pressed(confirmRect)) {
                baseRingWorkshopSelection_ = RingLevelUpgradeKindCount;
                ui.emitSound(UiSoundEvent::Confirm);
                confirmRingWorkshopRespec();
                ui.block(workshopBounds);
                return;
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                if (baseRingWorkshopSelection_ == RingLevelUpgradeKindCount) {
                    ui.emitSound(UiSoundEvent::Confirm);
                    confirmRingWorkshopRespec();
                } else {
                    chooseRespecKind(baseRingWorkshopSelection_);
                }
                ui.block(workshopBounds);
                return;
            }
            ui.block(workshopBounds);
            return;
        }

        if (baseRingWorkshopMode_ == RingWorkshopMode::Upgrade) {
            baseRingWorkshopSelection_ = std::clamp(baseRingWorkshopSelection_, 0, RingWorkshopUpgradeDisplayCount - 1);
            int move = 0;
            if (input.pressed(InputAction::MoveUp) || input.pressed(InputAction::MoveLeft) || input.shortcutCursorDelta() < 0) {
                --move;
            }
            if (input.pressed(InputAction::MoveDown) || input.pressed(InputAction::MoveRight) || input.shortcutCursorDelta() > 0) {
                ++move;
            }
            if (move != 0) {
                baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + move + RingWorkshopUpgradeDisplayCount) % RingWorkshopUpgradeDisplayCount;
            }

            const auto chooseUpgradeItem = [this, &ui](int item) {
                if (item >= 0 && item < RingWorkshopImplementedUpgradeCount) {
                    ui.emitSound(UiSoundEvent::Confirm);
                    buyRingWorkshopUpgrade(static_cast<RingWorkshopUpgrade>(item));
                    return;
                }
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "この項目は未解禁です";
            };

            for (int i = 0; i < RingWorkshopUpgradeDisplayCount; ++i) {
                const UiRect rect = ringWorkshopUpgradeItemRect(i);
                if (rect.contains(ui.mouse())) {
                    baseRingWorkshopSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseRingWorkshopSelection_ = i;
                    chooseUpgradeItem(i);
                    ui.block(workshopBounds);
                    return;
                }
            }
            if (ui.pressed(ringWorkshopUpgradeConfirmRect())) {
                chooseUpgradeItem(baseRingWorkshopSelection_);
                ui.block(workshopBounds);
                return;
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                chooseUpgradeItem(baseRingWorkshopSelection_);
                ui.block(workshopBounds);
                return;
            }
            ui.block(workshopBounds);
            return;
        }

        ui.block(workshopBounds);
        return;
    }

    if (baseStorageActive_) {
        const bool storageSmallDialog =
            baseStorageMode_ == StorageUiMode::ChooseAction ||
            baseStorageMode_ == StorageUiMode::Bulk;
        const UiRect storageBounds = storageSmallDialog
            ? (baseStorageMode_ == StorageUiMode::Bulk ? storageBulkDialogRect() : storageActionDialogRect())
            : merchantPanelRect();
        const auto resetStoragePointerPress = [this]() {
            baseStoragePointerOperation_ = StorageQuantityOperation::None;
            baseStoragePointerTarget_ = {};
            baseStoragePointerPressMouse_ = {};
            baseStoragePointerPressCanOpenMenu_ = false;
            baseStoragePointerDragTriggered_ = false;
        };
        const auto closeStorageCommand = [this]() {
            closeUiCommandMenu(baseStorageCommandMenu_);
            baseStorageCommandOperation_ = StorageQuantityOperation::None;
            baseStorageCommandTarget_ = {};
        };
        const auto closeStorage = [this, &closeStorageCommand, &resetStoragePointerPress]() {
            closeStorageCommand();
            resetStoragePointerPress();
            baseStorageActive_ = false;
            baseStorageMode_ = StorageUiMode::Closed;
            baseStorageQuantityDialog_ = {};
            baseStorageQuantityPending_ = {};
            baseStatus_.clear();
        };
        const auto returnToStorageMenu = [this, &closeStorageCommand, &resetStoragePointerPress]() {
            closeStorageCommand();
            resetStoragePointerPress();
            baseStorageMode_ = StorageUiMode::ChooseAction;
            baseStorageActionSelection_ = 0;
            baseStorageBulkSelection_ = 0;
            baseStorageQuantityDialog_ = {};
            baseStorageQuantityPending_ = {};
            baseStatus_.clear();
        };
        const auto openQuantityDialog = [this](StorageQuantityOperation operation, StorageTransferTarget target, int maxCount) {
            InventoryUiEntryView view = storageTransferTargetView(target);
            const std::optional<InventoryUiItemStats> stats = inventoryUiEntryStats(view);
            const bool broken = stats ? stats->broken : (view.item != nullptr && view.item->durability == 0);
            const std::string itemName = view.item != nullptr && !view.item->name.empty()
                ? itemDisplayName(view.item->name, broken)
                : std::string("アイテム");
            baseStorageQuantityPending_ = StorageQuantityPending{operation, target};
            openUiQuantityDialog(
                baseStorageQuantityDialog_,
                operation == StorageQuantityOperation::Deposit ? "しまう個数" : "取り出す個数",
                itemName,
                1,
                std::max(1, maxCount),
                std::max(1, maxCount),
                "個");
            baseStatus_.clear();
        };
        const auto applyStorageTarget = [this, &openQuantityDialog](StorageQuantityOperation operation, StorageTransferTarget target) {
            if (!storageTransferTargetAvailable(target)) {
                if (operation == StorageQuantityOperation::Deposit &&
                    target.source == BaseItemSource::Backpack) {
                    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                        if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                            baseStatus_ = "装備中の杖はしまえません";
                            return;
                        }
                    }
                }
                if (operation == StorageQuantityOperation::Deposit &&
                    target.valid &&
                    target.source != BaseItemSource::Backpack &&
                    target.source != BaseItemSource::Warehouse) {
                    baseStatus_ = "このアイテムはしまえません";
                } else {
                    baseStatus_ = operation == StorageQuantityOperation::Deposit
                        ? "しまうアイテムがありません"
                        : "取り出すアイテムがありません";
                }
                return;
            }
            if (storageTransferTargetIsStack(target)) {
                const int stackCount = storageTransferTargetStackCount(target);
                if (stackCount > 1) {
                    openQuantityDialog(operation, target, stackCount);
                    return;
                }
            }
            if (operation == StorageQuantityOperation::Deposit) {
                depositStorageTarget(target, 1);
            } else {
                withdrawStorageTarget(target, 1);
            }
        };
        const auto storageCommandItems = [this]() {
            const bool available = storageTransferTargetAvailable(baseStorageCommandTarget_);
            const char* label = baseStorageCommandOperation_ == StorageQuantityOperation::Withdraw
                ? "取り出す"
                : "しまう";
            return std::array<UiCommandMenuItem, 1>{{{label, available}}};
        };
        const auto openStorageCommand = [&](StorageQuantityOperation operation, StorageTransferTarget target, Vec2 anchor) {
            if (!storageTransferTargetAvailable(target)) {
                ui.emitSound(UiSoundEvent::Cancel);
                if (operation == StorageQuantityOperation::Deposit &&
                    target.source == BaseItemSource::Backpack) {
                    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                        if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                            baseStatus_ = "装備中の杖はしまえません";
                            closeStorageCommand();
                            return;
                        }
                    }
                }
                if (operation == StorageQuantityOperation::Deposit &&
                    target.valid &&
                    target.source != BaseItemSource::Backpack &&
                    target.source != BaseItemSource::Warehouse) {
                    baseStatus_ = "このアイテムはしまえません";
                } else {
                    baseStatus_ = operation == StorageQuantityOperation::Deposit
                        ? "しまうアイテムがありません"
                        : "取り出すアイテムがありません";
                }
                closeStorageCommand();
                return;
            }

            baseStorageCommandOperation_ = operation;
            baseStorageCommandTarget_ = target;
            const std::array<UiCommandMenuItem, 1> items = storageCommandItems();
            openUiCommandMenu(
                baseStorageCommandMenu_,
                anchor,
                storageBounds,
                static_cast<int>(items.size()),
                items.data(),
                140.0f,
                2);
            baseStatus_.clear();
        };
        const auto moveGridSelection = [&input](int& selection, int slotCount) {
            const int count = std::max(1, slotCount);
            selection = std::clamp(selection, 0, count - 1);
            const int columns = StorageColumns;
            if (input.pressed(InputAction::MoveLeft)) {
                selection = std::max(0, selection - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                selection = std::min(count - 1, selection + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                selection = std::max(0, selection - columns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                selection = std::min(count - 1, selection + columns);
            }
            if (input.shortcutCursorDelta() != 0) {
                selection = std::clamp(selection + input.shortcutCursorDelta(), 0, count - 1);
            }
        };

        const std::array<UiCommandMenuItem, 1> commandItems = storageCommandItems();
        const bool commandOpenBeforeUpdate = baseStorageCommandMenu_.open;
        const int commandSelection = updateUiCommandMenu(
            baseStorageCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0) {
            const StorageQuantityOperation operation = baseStorageCommandOperation_;
            const StorageTransferTarget target = baseStorageCommandTarget_;
            closeStorageCommand();
            resetStoragePointerPress();
            if (operation == StorageQuantityOperation::Deposit ||
                operation == StorageQuantityOperation::Withdraw) {
                applyStorageTarget(operation, target);
            }
            ui.block(storageBounds);
            return;
        } else if (!baseStorageCommandMenu_.open) {
            if (commandOpenBeforeUpdate && input.backPressed()) {
                closeStorageCommand();
                resetStoragePointerPress();
                ui.block(storageBounds);
                return;
            }
            baseStorageCommandOperation_ = StorageQuantityOperation::None;
            baseStorageCommandTarget_ = {};
        }

        if (uiCancelRequested(baseCancelState_, input, ui, storageBounds)) {
            if (baseStorageCommandMenu_.open) {
                closeStorageCommand();
                resetStoragePointerPress();
            } else if (baseStorageMode_ == StorageUiMode::ChooseAction) {
                closeStorage();
            } else {
                returnToStorageMenu();
            }
            ui.block(storageBounds);
            return;
        }

        if (baseStorageCommandMenu_.open) {
            ui.block(storageBounds);
            return;
        }

        const int storagePresetShortcut = input.shortcutSlotPressed();
        if (storagePresetShortcut >= 0 && storagePresetShortcut < RingPresetSlotCount) {
            const bool registered = ringPresets_.registered(storagePresetShortcut);
            prepareRingPresetFromWarehouse(storagePresetShortcut);
            ui.emitSound(registered ? UiSoundEvent::Confirm : UiSoundEvent::Cancel);
            ui.block(storageBounds);
            return;
        }

        if (baseStorageMode_ == StorageUiMode::ChooseAction) {
            constexpr int ChoiceCount = 3;
            baseStorageActionSelection_ = std::clamp(baseStorageActionSelection_, 0, ChoiceCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseStorageActionSelection_ = (baseStorageActionSelection_ + ChoiceCount - 1) % ChoiceCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseStorageActionSelection_ = (baseStorageActionSelection_ + 1) % ChoiceCount;
            }
            for (int i = 0; i < ChoiceCount; ++i) {
                const UiRect rect = storageActionChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseStorageActionSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseStorageActionSelection_ = i;
                    ui.emitSound(UiSoundEvent::Confirm);
                    baseStorageMode_ = i == 0
                        ? StorageUiMode::Deposit
                        : (i == 1 ? StorageUiMode::Withdraw : StorageUiMode::Bulk);
                    ui.block(storageBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                ui.emitSound(UiSoundEvent::Confirm);
                baseStorageMode_ = baseStorageActionSelection_ == 0
                    ? StorageUiMode::Deposit
                    : (baseStorageActionSelection_ == 1 ? StorageUiMode::Withdraw : StorageUiMode::Bulk);
                ui.block(storageBounds);
                return;
            }
            ui.block(storageBounds);
            return;
        }

        if (baseStorageMode_ == StorageUiMode::Bulk) {
            constexpr int ChoiceCount = 4;
            baseStorageBulkSelection_ = std::clamp(baseStorageBulkSelection_, 0, ChoiceCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseStorageBulkSelection_ = (baseStorageBulkSelection_ + ChoiceCount - 1) % ChoiceCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseStorageBulkSelection_ = (baseStorageBulkSelection_ + 1) % ChoiceCount;
            }

            const auto executeBulkAction = [&](int selection) {
                if (selection == 0) {
                    depositAllStorageItems();
                    ui.emitSound(UiSoundEvent::Confirm);
                    return;
                }
                const int presetIndex = selection - 1;
                if (!ringPresets_.registered(presetIndex)) {
                    baseStatus_ = "プリセット" + std::to_string(presetIndex + 1) + "は未登録です";
                    ui.emitSound(UiSoundEvent::Cancel);
                    return;
                }
                prepareRingPresetFromWarehouse(presetIndex);
                ui.emitSound(UiSoundEvent::Confirm);
            };

            for (int i = 0; i < ChoiceCount; ++i) {
                const UiRect rect = storageBulkChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseStorageBulkSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseStorageBulkSelection_ = i;
                    executeBulkAction(i);
                    ui.block(storageBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                executeBulkAction(baseStorageBulkSelection_);
                ui.block(storageBounds);
                return;
            }
            ui.block(storageBounds);
            return;
        }

        if (baseStorageMode_ == StorageUiMode::Deposit) {
            const int sourceCount = storageDepositSourceCountForUnlockedRings(unlockedRingCount());
            baseStorageDepositSource_ = clampStorageDepositSourceForUnlockedRings(baseStorageDepositSource_, unlockedRingCount());
            const int currentTab = storageDepositSourceTabIndex(baseStorageDepositSource_);
            std::array<UiTabItem, StorageDepositSourceCount> sourceTabs{};
            std::array<UiRect, StorageDepositSourceCount> sourceTabRects{};
            for (int i = 0; i < sourceCount; ++i) {
                const int source = storageDepositSourceValue(i);
                sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(source)], true};
                sourceTabRects[static_cast<std::size_t>(i)] = storageDepositSourceRect(i);
            }
            UiTabsInput sourceTabsInput{};
            sourceTabsInput.focusDelta = input.activeRingDelta();
            sourceTabsInput.commit =
                sourceTabsInput.focusDelta != 0 ||
                input.confirmPressed() ||
                input.useItemPressed();
            const int sourceSelection = updateUiTabs(
                baseStorageDepositSourceTabs_,
                ui,
                sourceTabsInput,
                currentTab,
                sourceTabs.data(),
                sourceCount,
                sourceTabRects.data());
            if (sourceSelection >= 0) {
                closeStorageCommand();
                resetStoragePointerPress();
                baseStorageDepositSource_ = storageDepositSourceValue(sourceSelection);
                if (baseItemSourceIsRing(baseStorageDepositSource_)) {
                    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseStorageDepositSource_), 0, SpellRingCount - 1);
                    baseStorageDepositSelection_ = std::clamp(
                        baseStorageDepositSelection_,
                        0,
                        std::max(0, static_cast<int>(spellRing_.itemsForRing(ringIndex).size()) - 1));
                } else {
                    baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1));
                }
                ui.block(storageBounds);
                return;
            }

            if (baseItemSourceIsRing(baseStorageDepositSource_)) {
                const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseStorageDepositSource_), 0, SpellRingCount - 1);
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                const int itemCount = static_cast<int>(ringItems.size());
                if (itemCount <= 0) {
                    baseStorageDepositSelection_ = 0;
                } else {
                    baseStorageDepositSelection_ = std::clamp(baseStorageDepositSelection_, 0, itemCount - 1);
                    const auto moveRingSelection = [&](int delta) {
                        if (delta == 0 || itemCount <= 0) {
                            return;
                        }
                        baseStorageDepositSelection_ = (baseStorageDepositSelection_ + delta) % itemCount;
                        if (baseStorageDepositSelection_ < 0) {
                            baseStorageDepositSelection_ += itemCount;
                        }
                    };
                    moveRingSelection(input.shortcutCursorDelta());
                    if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp)) {
                        moveRingSelection(-1);
                    }
                    if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown)) {
                        moveRingSelection(1);
                    }
                }
                int hoveredRingItem = -1;
                for (int i = 0; i < itemCount; ++i) {
                    const UiRect rect = storageRingItemRect(
                        ringItems[static_cast<std::size_t>(i)],
                        spellRing_,
                        balance_,
                        ringIndex,
                        i,
                        itemCount,
                        ringPreviewSeconds);
                    if (rect.contains(ui.mouse())) {
                        baseStorageDepositSelection_ = i;
                        hoveredRingItem = i;
                    }
                }
                if (input.mouseLeftPressed() && hoveredRingItem >= 0 && !ui.pointerConsumed()) {
                    baseStorageDepositSelection_ = hoveredRingItem;
                    const StorageTransferTarget target = storageDepositTargetForScreenSlot(hoveredRingItem);
                    baseStoragePointerOperation_ = StorageQuantityOperation::Deposit;
                    baseStoragePointerTarget_ = target;
                    baseStoragePointerPressMouse_ = input.mouseScreen();
                    baseStoragePointerPressCanOpenMenu_ = storageTransferTargetAvailable(target);
                    baseStoragePointerDragTriggered_ = false;
                    ui.consumePointer();
                    return;
                }
                if (baseStoragePointerOperation_ == StorageQuantityOperation::Deposit &&
                    baseStoragePointerTarget_.source != BaseItemSource::Backpack &&
                    baseStoragePointerTarget_.source != BaseItemSource::Warehouse &&
                    baseStoragePointerTarget_.ringIndex == ringIndex) {
                    if (input.mouseLeftHeld() &&
                        baseStoragePointerPressCanOpenMenu_ &&
                        !baseStoragePointerDragTriggered_ &&
                        lengthSquared(input.mouseScreen() - baseStoragePointerPressMouse_) >= StorageDragStartDistanceSq) {
                        baseStoragePointerDragTriggered_ = true;
                        baseStoragePointerPressCanOpenMenu_ = false;
                    }
                    if (input.mouseLeftReleased()) {
                        if (!baseStoragePointerDragTriggered_ &&
                            baseStoragePointerPressCanOpenMenu_ &&
                            hoveredRingItem == baseStoragePointerTarget_.ringItemIndex) {
                            baseStorageDepositSelection_ = hoveredRingItem;
                            openStorageCommand(
                                StorageQuantityOperation::Deposit,
                                baseStoragePointerTarget_,
                                input.mouseScreen());
                        }
                        resetStoragePointerPress();
                        ui.block(storageBounds);
                        return;
                    }
                }
                if (input.confirmPressed() || input.useItemPressed()) {
                    const UiRect rect = baseStorageDepositSelection_ >= 0 && baseStorageDepositSelection_ < itemCount
                        ? storageRingItemRect(
                            ringItems[static_cast<std::size_t>(baseStorageDepositSelection_)],
                            spellRing_,
                            balance_,
                            ringIndex,
                            baseStorageDepositSelection_,
                            itemCount,
                            ringPreviewSeconds)
                        : storageTransferGridSlotRect(0);
                    openStorageCommand(
                        StorageQuantityOperation::Deposit,
                        storageDepositTargetForScreenSlot(baseStorageDepositSelection_),
                        rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                    ui.block(storageBounds);
                    return;
                }
                ui.block(storageBounds);
                return;
            }

            if (input.arrangeItemsPressed() || ui.pressed(storageTransferSortButtonRect())) {
                closeStorageCommand();
                resetStoragePointerPress();
                const bool sorted = inventory_.sortByCatalogOrder(objectCatalog_);
                ui.emitSound(sorted ? UiSoundEvent::ItemMove : UiSoundEvent::Cancel);
                baseStorageDepositSelection_ = 0;
                baseStatus_ = sorted ? "リュックを並び替えました" : "リュックは空です";
                ui.block(storageBounds);
                return;
            }

            moveGridSelection(baseStorageDepositSelection_, inventory_.screenSlotCount());
            int hoveredBackpackSlot = -1;
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const UiRect rect = storageTransferGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseStorageDepositSelection_ = i;
                    hoveredBackpackSlot = i;
                }
            }
            const auto moveBackpackStorageItem = [this](StorageTransferTarget target, int toSlot) {
                if (target.source != BaseItemSource::Backpack || toSlot < 0 || toSlot >= inventory_.screenSlotCount()) {
                    return false;
                }
                if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
                    return inventory_.moveObjectStackToScreenSlot(stack->objectId, toSlot);
                }
                if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                    return inventory_.moveObjectInstanceToScreenSlot(instance->instance.instanceId, toSlot);
                }
                return false;
            };
            if (input.mouseLeftPressed() && hoveredBackpackSlot >= 0 && !ui.pointerConsumed()) {
                baseStorageDepositSelection_ = hoveredBackpackSlot;
                const StorageTransferTarget target = storageDepositTargetForScreenSlot(hoveredBackpackSlot);
                baseStoragePointerOperation_ = StorageQuantityOperation::Deposit;
                baseStoragePointerTarget_ = target;
                baseStoragePointerPressMouse_ = input.mouseScreen();
                baseStoragePointerPressCanOpenMenu_ = storageTransferTargetAvailable(target);
                baseStoragePointerDragTriggered_ = false;
                ui.consumePointer();
                return;
            }
            if (baseStoragePointerOperation_ == StorageQuantityOperation::Deposit &&
                baseStoragePointerTarget_.source == BaseItemSource::Backpack) {
                if (input.mouseLeftHeld() &&
                    baseStoragePointerPressCanOpenMenu_ &&
                    !baseStoragePointerDragTriggered_ &&
                    lengthSquared(input.mouseScreen() - baseStoragePointerPressMouse_) >= StorageDragStartDistanceSq) {
                    baseStoragePointerDragTriggered_ = true;
                    baseStoragePointerPressCanOpenMenu_ = false;
                    closeStorageCommand();
                    baseStatus_.clear();
                }
                if (input.mouseLeftReleased()) {
                    if (baseStoragePointerDragTriggered_) {
                        if (hoveredBackpackSlot >= 0 &&
                            moveBackpackStorageItem(baseStoragePointerTarget_, hoveredBackpackSlot)) {
                            ui.emitSound(UiSoundEvent::ItemMove);
                            baseStorageDepositSelection_ = hoveredBackpackSlot;
                            baseStatus_.clear();
                        }
                    } else if (baseStoragePointerPressCanOpenMenu_ &&
                        hoveredBackpackSlot == baseStoragePointerTarget_.slotIndex) {
                        const UiRect rect = storageTransferGridSlotRect(hoveredBackpackSlot);
                        openStorageCommand(
                            StorageQuantityOperation::Deposit,
                            baseStoragePointerTarget_,
                            rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                    }
                    resetStoragePointerPress();
                    ui.block(storageBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                const UiRect rect = storageTransferGridSlotRect(baseStorageDepositSelection_);
                openStorageCommand(
                    StorageQuantityOperation::Deposit,
                    storageDepositTargetForScreenSlot(baseStorageDepositSelection_),
                    rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                ui.block(storageBounds);
                return;
            }
            ui.block(storageBounds);
            return;
        }

        if (baseStorageMode_ == StorageUiMode::Withdraw) {
            const int warehousePageCount = std::max(1, (warehouseCapacity() + StorageWithdrawSlotCount - 1) / StorageWithdrawSlotCount);
            baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
            const UiPageSelectorRects pageRects = storageWithdrawPageSelectorRects();
            if (input.arrangeItemsPressed() || ui.pressed(storageWithdrawSortButtonRect())) {
                const bool hasItems = warehouseUsedSlots() > 0;
                ui.emitSound(hasItems ? UiSoundEvent::ItemMove : UiSoundEvent::Cancel);
                sortWarehouseByCatalogOrder();
                ui.block(storageBounds);
                return;
            }
            if (input.activeRingDelta() != 0) {
                closeStorageCommand();
                resetStoragePointerPress();
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, input.activeRingDelta(), warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }
            if (ui.pressed(pageRects.prev)) {
                closeStorageCommand();
                resetStoragePointerPress();
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, -1, warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }
            if (ui.pressed(pageRects.next)) {
                closeStorageCommand();
                resetStoragePointerPress();
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, 1, warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }

            moveGridSelection(baseStorageWithdrawSelection_, StorageWithdrawSlotCount);
            int hoveredWarehouseSlot = -1;
            for (int i = 0; i < StorageWithdrawSlotCount; ++i) {
                const UiRect rect = storageWithdrawSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseStorageWithdrawSelection_ = i;
                    hoveredWarehouseSlot = i;
                }
            }
            const auto moveWarehouseStorageItem = [this](StorageTransferTarget target, int toSlot) {
                if (target.source != BaseItemSource::Warehouse ||
                    !target.valid ||
                    toSlot < 0 ||
                    toSlot >= StorageWithdrawSlotCount) {
                    return false;
                }
                const int entryIndex = target.storageEntry.kind == StorageEntryKind::Stack
                    ? target.storageEntry.index
                    : static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index;
                const int storageSlot = baseStorageWarehousePage_ * StorageWithdrawSlotCount + toSlot;
                if (storageSlot >= warehouseCapacity()) {
                    return false;
                }
                assignWarehouseEntryToStorageSlot(entryIndex, storageSlot);
                return true;
            };
            if (input.mouseLeftPressed() && hoveredWarehouseSlot >= 0 && !ui.pointerConsumed()) {
                baseStorageWithdrawSelection_ = hoveredWarehouseSlot;
                const StorageTransferTarget target = storageWithdrawTargetForSlot(hoveredWarehouseSlot);
                baseStoragePointerOperation_ = StorageQuantityOperation::Withdraw;
                baseStoragePointerTarget_ = target;
                baseStoragePointerPressMouse_ = input.mouseScreen();
                baseStoragePointerPressCanOpenMenu_ = storageTransferTargetAvailable(target);
                baseStoragePointerDragTriggered_ = false;
                ui.consumePointer();
                return;
            }
            if (baseStoragePointerOperation_ == StorageQuantityOperation::Withdraw &&
                baseStoragePointerTarget_.source == BaseItemSource::Warehouse) {
                if (input.mouseLeftHeld() &&
                    baseStoragePointerPressCanOpenMenu_ &&
                    !baseStoragePointerDragTriggered_ &&
                    lengthSquared(input.mouseScreen() - baseStoragePointerPressMouse_) >= StorageDragStartDistanceSq) {
                    baseStoragePointerDragTriggered_ = true;
                    baseStoragePointerPressCanOpenMenu_ = false;
                    closeStorageCommand();
                    baseStatus_.clear();
                }
                if (input.mouseLeftReleased()) {
                    if (baseStoragePointerDragTriggered_) {
                        if (hoveredWarehouseSlot >= 0 &&
                            moveWarehouseStorageItem(baseStoragePointerTarget_, hoveredWarehouseSlot)) {
                            ui.emitSound(UiSoundEvent::ItemMove);
                            baseStorageWithdrawSelection_ = hoveredWarehouseSlot;
                            baseStatus_.clear();
                        }
                    } else if (baseStoragePointerPressCanOpenMenu_ &&
                        hoveredWarehouseSlot == baseStoragePointerTarget_.slotIndex) {
                        const UiRect rect = storageWithdrawSlotRect(hoveredWarehouseSlot);
                        openStorageCommand(
                            StorageQuantityOperation::Withdraw,
                            baseStoragePointerTarget_,
                            rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                    }
                    resetStoragePointerPress();
                    ui.block(storageBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                const UiRect rect = storageWithdrawSlotRect(baseStorageWithdrawSelection_);
                openStorageCommand(
                    StorageQuantityOperation::Withdraw,
                    storageWithdrawTargetForSlot(baseStorageWithdrawSelection_),
                    rect.pos + Vec2{rect.size.x - 20.0f, 0.0f});
                ui.block(storageBounds);
                return;
            }
            ui.block(storageBounds);
            return;
        }

        returnToStorageMenu();
        ui.block(storageBounds);
        return;
    }

    if (baseProcessingActive_) {
        const auto closeProcessingCommand = [this]() {
            closeUiCommandMenu(baseProcessingCommandMenu_);
            baseProcessingCommandSlot_ = -1;
        };
        const auto openProcessingCommand = [&](int slotIndex) {
            const ProcessingTarget target = processingTargetForScreenSlot(slotIndex);
            if (!target.valid) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "加工対象がありません";
                return false;
            }
            if (!processingTargetHasAvailableCommand(target)) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "このアイテムにできる作業がありません";
                return false;
            }
            const std::vector<UiCommandMenuItem> items = processingCommandItems(target);
            baseProcessingCommandSlot_ = slotIndex;
            Vec2 commandAnchor = baseProcessingGridSlotRect(slotIndex).pos;
            if (target.source == BaseItemSource::Warehouse) {
                commandAnchor = externalWarehouseSourceSlotRect(baseProcessingGridSlotRect, slotIndex).pos;
            } else if (target.source != BaseItemSource::Backpack &&
                target.ringIndex >= 0 &&
                target.ringIndex < SpellRingCount) {
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
                if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size())) {
                    commandAnchor = baseProcessingRingItemRect(
                        ringItems[static_cast<std::size_t>(target.ringItemIndex)],
                        spellRing_,
                        balance_,
                        target.ringIndex,
                        target.ringItemIndex,
                        static_cast<int>(ringItems.size()),
                        ringPreviewSeconds).pos;
                }
            }
            openUiCommandMenu(
                baseProcessingCommandMenu_,
                commandAnchor,
                merchantPanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                184.0f,
                2);
            return true;
        };
        if (uiCancelRequested(baseCancelState_, input, ui, merchantPanelRect())) {
            if (baseProcessingCommandMenu_.open) {
                closeProcessingCommand();
            } else {
                baseProcessingActive_ = false;
                baseProcessingConfirm_ = {};
                baseProcessingConfirmTarget_ = {};
                baseStatus_.clear();
            }
            return;
        }
        const ProcessingTarget commandTarget = baseProcessingCommandSlot_ >= 0
            ? processingTargetForScreenSlot(baseProcessingCommandSlot_)
            : ProcessingTarget{};
        const std::vector<UiCommandMenuItem> commandItems = processingCommandItems(commandTarget);
        const bool commandOpenBeforeUpdate = baseProcessingCommandMenu_.open;
        const int commandSelection = updateUiCommandMenu(
            baseProcessingCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0 && baseProcessingCommandSlot_ >= 0) {
            const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(commandSelection, 0, BaseProcessingModeCount - 1));
            openProcessingConfirm(processingTargetForScreenSlot(baseProcessingCommandSlot_), mode);
            closeProcessingCommand();
            ui.block(merchantPanelRect());
            return;
        } else if (!baseProcessingCommandMenu_.open && commandOpenBeforeUpdate) {
            baseProcessingCommandSlot_ = -1;
        }
        if (baseProcessingCommandMenu_.open) {
            ui.block(merchantPanelRect());
            return;
        }

        const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount());
        baseProcessingSource_ = clampBaseItemSourceForUnlockedRings(baseProcessingSource_, unlockedRingCount());
        std::array<UiTabItem, BaseProcessingSourceCount> sourceTabs{};
        std::array<UiRect, BaseProcessingSourceCount> sourceTabRects{};
        for (int i = 0; i < sourceCount; ++i) {
            sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(i)], true};
            sourceTabRects[static_cast<std::size_t>(i)] = baseProcessingSourceRect(i, sourceCount);
        }
        UiTabsInput sourceTabsInput{};
        sourceTabsInput.focusDelta = baseItemSourceIsWarehouse(baseProcessingSource_) ? 0 : input.activeRingDelta();
        const int directSourceFocus = input.shortcutSlotPressed();
        if (directSourceFocus >= 0 && directSourceFocus < sourceCount) {
            sourceTabsInput.directFocusIndex = directSourceFocus;
        }
        sourceTabsInput.commit =
            sourceTabsInput.focusDelta != 0 ||
            sourceTabsInput.directFocusIndex >= 0 ||
            input.confirmPressed() ||
            input.useItemPressed();
        const int sourceSelection = updateUiTabs(
            baseProcessingSourceTabs_,
            ui,
            sourceTabsInput,
            baseProcessingSource_,
            sourceTabs.data(),
            sourceCount,
            sourceTabRects.data());
        if (sourceSelection >= 0) {
            baseProcessingSource_ = sourceSelection;
            int sourceSlotCount = inventory_.screenSlotCount();
            if (baseItemSourceIsWarehouse(baseProcessingSource_)) {
                sourceSlotCount = StoragePaneSlotCount;
            } else if (baseItemSourceIsRing(baseProcessingSource_)) {
                const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseProcessingSource_), 0, SpellRingCount - 1);
                sourceSlotCount = std::max(1, static_cast<int>(spellRing_.itemsForRing(ringIndex).size()));
            }
            baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, sourceSlotCount - 1));
            closeProcessingCommand();
            return;
        }

        if (baseItemSourceIsWarehouse(baseProcessingSource_)) {
            const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
            baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
            const UiPageSelectorRects pageRects = externalWarehousePageSelectorRects(baseProcessingGridSlotRect);
            if (input.activeRingDelta() != 0) {
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, input.activeRingDelta(), warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }
            if (ui.pressed(pageRects.prev)) {
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, -1, warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }
            if (ui.pressed(pageRects.next)) {
                const int previousPage = baseStorageWarehousePage_;
                baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, 1, warehousePageCount);
                if (baseStorageWarehousePage_ != previousPage) {
                    ui.emitSound(UiSoundEvent::TabSwitch);
                }
            }

            baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, StoragePaneSlotCount - 1);
            if (input.pressed(InputAction::MoveLeft)) {
                baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                baseProcessingSelection_ = std::min(StoragePaneSlotCount - 1, baseProcessingSelection_ + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - StorageColumns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseProcessingSelection_ = std::min(StoragePaneSlotCount - 1, baseProcessingSelection_ + StorageColumns);
            }
            for (int i = 0; i < StoragePaneSlotCount; ++i) {
                const UiRect rect = externalWarehouseSourceSlotRect(baseProcessingGridSlotRect, i);
                if (rect.contains(ui.mouse())) {
                    baseProcessingSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseProcessingSelection_ = i;
                    openProcessingCommand(i);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openProcessingCommand(baseProcessingSelection_);
                return;
            }
            ui.block(merchantPanelRect());
            return;
        }

        if (baseItemSourceIsRing(baseProcessingSource_)) {
            const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseProcessingSource_), 0, SpellRingCount - 1);
            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
            const int itemCount = static_cast<int>(ringItems.size());
            if (itemCount <= 0) {
                baseProcessingSelection_ = 0;
            } else {
                baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, itemCount - 1);
                const auto moveRingSelection = [&](int delta) {
                    if (delta == 0 || itemCount <= 0) {
                        return;
                    }
                    baseProcessingSelection_ = (baseProcessingSelection_ + delta) % itemCount;
                    if (baseProcessingSelection_ < 0) {
                        baseProcessingSelection_ += itemCount;
                    }
                };
                moveRingSelection(input.shortcutCursorDelta());
                if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp)) {
                    moveRingSelection(-1);
                }
                if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown)) {
                    moveRingSelection(1);
                }
            }

            for (int i = 0; i < itemCount; ++i) {
                const UiRect rect = baseProcessingRingItemRect(
                    ringItems[static_cast<std::size_t>(i)],
                    spellRing_,
                    balance_,
                    ringIndex,
                    i,
                    itemCount,
                    ringPreviewSeconds);
                if (rect.contains(ui.mouse())) {
                    baseProcessingSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseProcessingSelection_ = i;
                    openProcessingCommand(i);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openProcessingCommand(baseProcessingSelection_);
                return;
            }
            ui.block(merchantPanelRect());
            return;
        }

        constexpr int Columns = 8;
        const int slotCount = inventory_.screenSlotCount();
        baseProcessingSelection_ = std::clamp(baseProcessingSelection_, 0, std::max(0, slotCount - 1));
        if (input.pressed(InputAction::MoveLeft)) {
            baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - 1);
        }
        if (input.pressed(InputAction::MoveRight)) {
            baseProcessingSelection_ = std::min(slotCount - 1, baseProcessingSelection_ + 1);
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseProcessingSelection_ = std::max(0, baseProcessingSelection_ - Columns);
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseProcessingSelection_ = std::min(slotCount - 1, baseProcessingSelection_ + Columns);
        }
        for (int i = 0; i < slotCount; ++i) {
            const UiRect rect = baseProcessingGridSlotRect(i);
            if (rect.contains(ui.mouse())) {
                baseProcessingSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseProcessingSelection_ = i;
                openProcessingCommand(i);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            openProcessingCommand(baseProcessingSelection_);
            return;
        }
        ui.block(merchantPanelRect());
        return;
    }

    if (baseSellActive_) {
        refreshMerchantStock(false);
        const UiRect merchantBounds = baseMerchantMode_ == MerchantUiMode::ChooseAction ? merchantActionDialogRect() : merchantPanelRect();
        const auto closeMerchantCommands = [this]() {
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandSource_ = 0;
            baseMerchantSellCommandIndex_ = -1;
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
        };
        const auto closeMerchant = [&]() {
            closeMerchantCommands();
            baseSellActive_ = false;
            baseMerchantMode_ = MerchantUiMode::Closed;
            baseStatus_.clear();
        };
        const auto returnToMerchantMenu = [&]() {
            closeMerchantCommands();
            baseMerchantMode_ = MerchantUiMode::ChooseAction;
            baseMerchantActionSelection_ = 0;
            baseStatus_.clear();
        };
        const auto moveGridSelection = [&input](int& selection, int count) {
            constexpr int Columns = 8;
            if (count <= 0) {
                selection = 0;
                return;
            }
            selection = std::clamp(selection, 0, count - 1);
            if (input.pressed(InputAction::MoveLeft)) {
                selection = std::max(0, selection - 1);
            }
            if (input.pressed(InputAction::MoveRight)) {
                selection = std::min(count - 1, selection + 1);
            }
            if (input.pressed(InputAction::MoveUp)) {
                selection = std::max(0, selection - Columns);
            }
            if (input.pressed(InputAction::MoveDown)) {
                selection = std::min(count - 1, selection + Columns);
            }
        };
        const auto merchantSellSourceSlotCount = [&]() {
            if (baseMerchantSellSource_ == 0) {
                return inventory_.screenSlotCount();
            }
            if (baseItemSourceIsWarehouse(baseMerchantSellSource_)) {
                return StoragePaneSlotCount;
            }
            const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseMerchantSellSource_), 0, SpellRingCount - 1);
            return static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
        };
        const auto openSellCommand = [&](int slotIndex) {
            const MerchantSellTarget target = merchantSellTargetForScreenSlot(slotIndex);
            if (!target.valid) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "売却対象がありません";
                return;
            }
            if (!merchantSellTargetAvailable(target)) {
                ui.emitSound(UiSoundEvent::Confirm);
                sellMerchantTarget(target, 1);
                return;
            }
            const bool stackItem =
                (target.source == BaseItemSource::Backpack && inventory_.screenObjectStackAt(slotIndex) != nullptr) ||
                (target.source == BaseItemSource::Warehouse && target.storageEntry.kind == StorageEntryKind::Stack);
            baseMerchantSellCommandIndex_ = slotIndex;
            baseMerchantSellCommandSource_ = baseMerchantSellSource_;
            Vec2 commandAnchor = merchantSellGridSlotRect(slotIndex).pos;
            if (target.source == BaseItemSource::Warehouse) {
                commandAnchor = externalWarehouseSourceSlotRect(merchantSellGridSlotRect, slotIndex).pos;
            } else if (target.source != BaseItemSource::Backpack &&
                target.ringIndex >= 0 &&
                target.ringIndex < SpellRingCount) {
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
                if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size())) {
                    commandAnchor = merchantSellRingItemRect(
                        ringItems[static_cast<std::size_t>(target.ringItemIndex)],
                        spellRing_,
                        balance_,
                        target.ringIndex,
                        target.ringItemIndex,
                        static_cast<int>(ringItems.size()),
                        ringPreviewSeconds).pos;
                }
            }
            const std::array<UiCommandMenuItem, 2> items{{{stackItem ? "1個売る" : "売る", true}, {"すべて売る", stackItem}}};
            openUiCommandMenu(
                baseMerchantSellCommandMenu_,
                commandAnchor,
                merchantPanelRect(),
                stackItem ? 2 : 1,
                items.data(),
                168.0f,
                2);
        };
        const auto openBuyCommand = [&](int index) {
            if (index < 0 || index >= static_cast<int>(merchantStock_.size())) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "購入できる商品がありません";
                return;
            }
            baseMerchantBuyCommandIndex_ = index;
            const std::array<UiCommandMenuItem, 1> items{{{"買う", canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(index)])}}};
            openUiCommandMenu(
                baseMerchantBuyCommandMenu_,
                merchantGridSlotRect(index).pos,
                merchantPanelRect(),
                static_cast<int>(items.size()),
                items.data(),
                120.0f,
                2);
        };

        if (uiCancelRequested(baseCancelState_, input, ui, merchantBounds)) {
            if (baseMerchantSellCommandMenu_.open || baseMerchantBuyCommandMenu_.open) {
                closeMerchantCommands();
            } else if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
                closeMerchant();
            } else {
                returnToMerchantMenu();
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
            closeMerchantCommands();
            constexpr int ChoiceCount = 2;
            baseMerchantActionSelection_ = std::clamp(baseMerchantActionSelection_, 0, ChoiceCount - 1);
            if (input.pressed(InputAction::MoveUp)) {
                baseMerchantActionSelection_ = (baseMerchantActionSelection_ + ChoiceCount - 1) % ChoiceCount;
            }
            if (input.pressed(InputAction::MoveDown)) {
                baseMerchantActionSelection_ = (baseMerchantActionSelection_ + 1) % ChoiceCount;
            }
            for (int i = 0; i < ChoiceCount; ++i) {
                const UiRect rect = merchantActionChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseMerchantActionSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseMerchantActionSelection_ = i;
                    ui.emitSound(UiSoundEvent::Confirm);
                    if (i == 0) {
                        baseMerchantMode_ = MerchantUiMode::Buy;
                    } else if (i == 1) {
                        baseMerchantMode_ = MerchantUiMode::Sell;
                    }
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                ui.emitSound(UiSoundEvent::Confirm);
                if (baseMerchantActionSelection_ == 0) {
                    baseMerchantMode_ = MerchantUiMode::Buy;
                } else if (baseMerchantActionSelection_ == 1) {
                    baseMerchantMode_ = MerchantUiMode::Sell;
                }
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::Sell) {
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
            const MerchantSellTarget commandTarget = merchantSellTargetForSourceSlot(
                baseMerchantSellCommandSource_,
                baseMerchantSellCommandIndex_);
            const bool stackCommand =
                (commandTarget.source == BaseItemSource::Backpack &&
                    baseMerchantSellCommandIndex_ >= 0 &&
                    inventory_.screenObjectStackAt(baseMerchantSellCommandIndex_) != nullptr) ||
                (commandTarget.source == BaseItemSource::Warehouse &&
                    commandTarget.storageEntry.kind == StorageEntryKind::Stack);
            const std::array<UiCommandMenuItem, 2> commandItems{{{stackCommand ? "1個売る" : "売る", true}, {"すべて売る", stackCommand}}};
            const int commandItemCount = stackCommand ? 2 : 1;
            const bool commandOpenBeforeUpdate = baseMerchantSellCommandMenu_.open;
            const int commandSelection = updateUiCommandMenu(
                baseMerchantSellCommandMenu_,
                ui,
                input,
                commandItems.data(),
                commandItemCount);
            if (commandSelection >= 0 && baseMerchantSellCommandIndex_ >= 0) {
                sellMerchantTarget(commandTarget, commandSelection == 1 && stackCommand ? 0 : 1);
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            } else if (!baseMerchantSellCommandMenu_.open && commandOpenBeforeUpdate) {
                baseMerchantSellCommandSource_ = 0;
                baseMerchantSellCommandIndex_ = -1;
            }
            if (baseMerchantSellCommandMenu_.open) {
                ui.block(merchantBounds);
                return;
            }

            const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount());
            baseMerchantSellSource_ = clampBaseItemSourceForUnlockedRings(baseMerchantSellSource_, unlockedRingCount());
            std::array<UiTabItem, BaseItemSourceCount> sourceTabs{};
            std::array<UiRect, BaseItemSourceCount> sourceTabRects{};
            for (int i = 0; i < sourceCount; ++i) {
                sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(i)], true};
                sourceTabRects[static_cast<std::size_t>(i)] = merchantSellSourceRect(i, sourceCount);
            }
            UiTabsInput sourceTabsInput{};
            sourceTabsInput.focusDelta = baseItemSourceIsWarehouse(baseMerchantSellSource_) ? 0 : input.activeRingDelta();
            const int directSourceFocus = input.shortcutSlotPressed();
            if (directSourceFocus >= 0 && directSourceFocus < sourceCount) {
                sourceTabsInput.directFocusIndex = directSourceFocus;
            }
            sourceTabsInput.commit =
                sourceTabsInput.focusDelta != 0 ||
                sourceTabsInput.directFocusIndex >= 0 ||
                input.confirmPressed() ||
                input.useItemPressed();
            const int sourceSelection = updateUiTabs(
                baseMerchantSellSourceTabs_,
                ui,
                sourceTabsInput,
                baseMerchantSellSource_,
                sourceTabs.data(),
                sourceCount,
                sourceTabRects.data());
            if (sourceSelection >= 0) {
                baseMerchantSellSource_ = sourceSelection;
                baseSellSelection_ = std::clamp(baseSellSelection_, 0, std::max(0, merchantSellSourceSlotCount() - 1));
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            }

            if (baseItemSourceIsWarehouse(baseMerchantSellSource_)) {
                const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
                baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                const UiPageSelectorRects pageRects = externalWarehousePageSelectorRects(merchantSellGridSlotRect);
                if (input.activeRingDelta() != 0) {
                    const int previousPage = baseStorageWarehousePage_;
                    baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, input.activeRingDelta(), warehousePageCount);
                    if (baseStorageWarehousePage_ != previousPage) {
                        ui.emitSound(UiSoundEvent::TabSwitch);
                    }
                }
                if (ui.pressed(pageRects.prev)) {
                    const int previousPage = baseStorageWarehousePage_;
                    baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, -1, warehousePageCount);
                    if (baseStorageWarehousePage_ != previousPage) {
                        ui.emitSound(UiSoundEvent::TabSwitch);
                    }
                }
                if (ui.pressed(pageRects.next)) {
                    const int previousPage = baseStorageWarehousePage_;
                    baseStorageWarehousePage_ = wrapStoragePageIndex(baseStorageWarehousePage_, 1, warehousePageCount);
                    if (baseStorageWarehousePage_ != previousPage) {
                        ui.emitSound(UiSoundEvent::TabSwitch);
                    }
                }

                moveGridSelection(baseSellSelection_, StoragePaneSlotCount);
                for (int i = 0; i < StoragePaneSlotCount; ++i) {
                    const UiRect rect = externalWarehouseSourceSlotRect(merchantSellGridSlotRect, i);
                    if (rect.contains(ui.mouse())) {
                        baseSellSelection_ = i;
                    }
                    if (ui.pressed(rect)) {
                        baseSellSelection_ = i;
                        openSellCommand(i);
                        ui.block(merchantBounds);
                        return;
                    }
                }
                if (input.confirmPressed() || input.useItemPressed()) {
                    openSellCommand(baseSellSelection_);
                    ui.block(merchantBounds);
                    return;
                }
                ui.block(merchantBounds);
                return;
            }

            if (baseItemSourceIsRing(baseMerchantSellSource_)) {
                const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseMerchantSellSource_), 0, SpellRingCount - 1);
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                const int itemCount = static_cast<int>(ringItems.size());
                if (itemCount <= 0) {
                    baseSellSelection_ = 0;
                } else {
                    baseSellSelection_ = std::clamp(baseSellSelection_, 0, itemCount - 1);
                    const auto moveRingSelection = [&](int delta) {
                        if (delta == 0 || itemCount <= 0) {
                            return;
                        }
                        baseSellSelection_ = (baseSellSelection_ + delta) % itemCount;
                        if (baseSellSelection_ < 0) {
                            baseSellSelection_ += itemCount;
                        }
                    };
                    moveRingSelection(input.shortcutCursorDelta());
                    if (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp)) {
                        moveRingSelection(-1);
                    }
                    if (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown)) {
                        moveRingSelection(1);
                    }
                }

                for (int i = 0; i < itemCount; ++i) {
                    const UiRect rect = merchantSellRingItemRect(
                        ringItems[static_cast<std::size_t>(i)],
                        spellRing_,
                        balance_,
                        ringIndex,
                        i,
                        itemCount,
                        ringPreviewSeconds);
                    if (rect.contains(ui.mouse())) {
                        baseSellSelection_ = i;
                    }
                    if (ui.pressed(rect)) {
                        baseSellSelection_ = i;
                        openSellCommand(i);
                        ui.block(merchantBounds);
                        return;
                    }
                }
                if (input.confirmPressed() || input.useItemPressed()) {
                    openSellCommand(baseSellSelection_);
                    ui.block(merchantBounds);
                    return;
                }
                ui.block(merchantBounds);
                return;
            }

            moveGridSelection(baseSellSelection_, inventory_.screenSlotCount());
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const UiRect rect = merchantSellGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseSellSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseSellSelection_ = i;
                    openSellCommand(i);
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openSellCommand(baseSellSelection_);
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        if (baseMerchantMode_ == MerchantUiMode::Buy) {
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandSource_ = 0;
            baseMerchantSellCommandIndex_ = -1;
            const bool commandEnabled = baseMerchantBuyCommandIndex_ >= 0 &&
                baseMerchantBuyCommandIndex_ < static_cast<int>(merchantStock_.size()) &&
                canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(baseMerchantBuyCommandIndex_)]);
            const std::array<UiCommandMenuItem, 1> commandItems{{{"買う", commandEnabled}}};
            const bool commandOpenBeforeUpdate = baseMerchantBuyCommandMenu_.open;
            const int commandSelection = updateUiCommandMenu(
                baseMerchantBuyCommandMenu_,
                ui,
                input,
                commandItems.data(),
                static_cast<int>(commandItems.size()));
            if (commandSelection >= 0 && baseMerchantBuyCommandIndex_ >= 0) {
                buyMerchantProduct(baseMerchantBuyCommandIndex_);
                closeMerchantCommands();
                ui.block(merchantBounds);
                return;
            } else if (!baseMerchantBuyCommandMenu_.open && commandOpenBeforeUpdate) {
                baseMerchantBuyCommandIndex_ = -1;
            }
            if (baseMerchantBuyCommandMenu_.open) {
                ui.block(merchantBounds);
                return;
            }

            moveGridSelection(baseMerchantBuySelection_, static_cast<int>(merchantStock_.size()));
            for (int i = 0; i < static_cast<int>(merchantStock_.size()); ++i) {
                const UiRect rect = merchantGridSlotRect(i);
                if (rect.contains(ui.mouse())) {
                    baseMerchantBuySelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseMerchantBuySelection_ = i;
                    openBuyCommand(i);
                    ui.block(merchantBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                openBuyCommand(baseMerchantBuySelection_);
                ui.block(merchantBounds);
                return;
            }
            ui.block(merchantBounds);
            return;
        }

        returnToMerchantMenu();
        ui.block(merchantBounds);
        return;
    }

    if (baseUpgradeActive_) {
        const UiRect upgradePanel = baseUpgradePanelRect();
        if (uiCancelRequested(baseCancelState_, input, ui, upgradePanel)) {
            baseUpgradeActive_ = false;
            baseStatus_.clear();
            return;
        }
        UiTabsInput upgradeTabInput{};
        if (input.pressed(InputAction::MoveUp)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + BaseUpgradeItemCount - 1) % BaseUpgradeItemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + 1) % BaseUpgradeItemCount;
        }
        std::array<UiVerticalTabItem, BaseUpgradeItemCount> upgradeTabs{};
        std::array<UiRect, BaseUpgradeItemCount> upgradeTabRects{};
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            upgradeTabs[static_cast<std::size_t>(i)] = {"", "", upgradeImplemented(i)};
            upgradeTabRects[static_cast<std::size_t>(i)] = baseUpgradeItemRect(i);
        }
        baseUpgradeTabs_.focusedIndex = std::clamp(baseUpgradeSelection_, 0, BaseUpgradeItemCount - 1);
        const int selectedTab = updateUiVerticalTabs(
            baseUpgradeTabs_,
            ui,
            upgradeTabInput,
            baseUpgradeSelection_,
            upgradeTabs.data(),
            static_cast<int>(upgradeTabs.size()),
            upgradeTabRects.data());
        if (selectedTab >= 0) {
            baseUpgradeSelection_ = selectedTab;
            ui.block(upgradePanel);
            return;
        }
        if (ui.pressed(baseUpgradeConfirmRect())) {
            ui.emitSound(UiSoundEvent::Confirm);
            buyUpgrade(baseUpgradeSelection_);
            ui.block(upgradePanel);
            return;
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            ui.emitSound(UiSoundEvent::Confirm);
            buyUpgrade(baseUpgradeSelection_);
            ui.block(upgradePanel);
            return;
        }
        ui.block(upgradePanel);
        return;
    }

    if (baseMiningStartChoiceActive_) {
        const auto openRegenerateConfirm = [this]() {
            openUiConfirmDialog(
                baseRegenerateConfirm_,
                "再生成確認",
                "現在の坑道状態を破棄して、地形・敵・宝箱・ワープポイントを作り直します。\n拾っていないドロップがある場合は消えます。\nボス再戦用に地形も作り直します。",
                "再生成する",
                "戻る",
                1);
            baseStatus_.clear();
        };

        if (baseRegenerateConfirm_.open) {
            const UiRect confirmPanel = baseMiningRegenerateConfirmRect();
            const UiConfirmDialogResult result = updateUiConfirmDialog(baseRegenerateConfirm_, ui, input, confirmPanel);
            if (result == UiConfirmDialogResult::Confirmed) {
                requestMiningStartTransition(false, true);
                ui.block(confirmPanel);
                return;
            }
            if (result == UiConfirmDialogResult::Cancelled) {
                baseStatus_.clear();
                ui.block(confirmPanel);
                return;
            }
            ui.block(confirmPanel);
            return;
        }

        if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
            if (baseWarpPointSelectActive_) {
                baseWarpPointSelectActive_ = false;
            } else {
                baseMiningStartChoiceActive_ = false;
            }
            baseRegenerateConfirm_ = {};
            baseStatus_.clear();
            return;
        }

        const std::vector<StageDefinition> selectableStages = selectableStageDefinitionsForCurrentUnlockState();
        const int selectableStageCount = static_cast<int>(selectableStages.size());
        const auto selectedStageIndex = [&]() {
            for (int i = 0; i < selectableStageCount; ++i) {
                if (selectableStages[static_cast<std::size_t>(i)].id == currentStageId_) {
                    return i;
                }
            }
            return 0;
        };
        const auto stageSelectorHitRect = [](UiRect rect) {
            constexpr float Padding = 12.0f;
            return UiRect{
                {rect.pos.x - Padding, rect.pos.y - Padding},
                {rect.size.x + Padding * 2.0f, rect.size.y + Padding * 2.0f},
            };
        };
        const auto changeSelectedStage = [&](int delta) {
            if (selectableStageCount <= 1) {
                return false;
            }
            const int currentIndex = selectedStageIndex();
            const int nextIndex = wrapStoragePageIndex(currentIndex, delta, selectableStageCount);
            if (nextIndex == currentIndex) {
                return false;
            }
            const StageDefinition& stage = selectableStages[static_cast<std::size_t>(nextIndex)];
            currentStageId_ = stage.id;
            currentStage_ = stageCatalogIndexForId(currentStageId_);
            resolveCurrentStageDefinition();
            syncWarpStateForCurrentStage();
            baseMiningStartSelection_ = unlockedWarpPointCount_ > 0 ? 1 : 0;
            baseWarpPointSelectActive_ = false;
            baseWarpPointSelection_ = 0;
            baseRegenerateConfirm_ = {};
            baseStatus_.clear();
            return true;
        };

        const std::vector<WarpPoint> selectableWarpPoints = selectableWarpPointsForCurrentStageStart();
        const auto startFromSelectedWarpPoint = [&]() {
            if (selectableWarpPoints.empty()) {
                baseStatus_ = "解放済みワープポイントがありません";
                return false;
            }
            baseWarpPointSelection_ = std::clamp(
                baseWarpPointSelection_,
                0,
                static_cast<int>(selectableWarpPoints.size()) - 1);
            requestedWarpPointStartPosition_ = selectableWarpPoints[static_cast<std::size_t>(baseWarpPointSelection_)].position;
            baseWarpPointSelectActive_ = false;
            baseRegenerateConfirm_ = {};
            baseStatus_.clear();
            requestMiningStartTransition(true, false);
            return true;
        };

        if (baseWarpPointSelectActive_) {
            if (selectableWarpPoints.empty()) {
                baseWarpPointSelectActive_ = false;
                baseStatus_ = "解放済みワープポイントがありません";
                ui.block(baseMiningWarpPointSelectRect());
                return;
            }

            const int warpPointCount = static_cast<int>(selectableWarpPoints.size());
            baseWarpPointSelection_ = std::clamp(baseWarpPointSelection_, 0, warpPointCount - 1);
            const int warpDelta =
                (input.pressed(InputAction::MoveLeft) || input.pressed(InputAction::MoveUp) ? -1 : 0) +
                (input.pressed(InputAction::MoveRight) || input.pressed(InputAction::MoveDown) ? 1 : 0) +
                input.activeRingDelta();
            if (warpDelta != 0) {
                baseWarpPointSelection_ = wrapStoragePageIndex(baseWarpPointSelection_, warpDelta, warpPointCount);
            }
            for (int i = 0; i < warpPointCount; ++i) {
                const UiRect rect = baseMiningWarpPointSelectChoiceRect(i);
                if (rect.contains(ui.mouse())) {
                    baseWarpPointSelection_ = i;
                }
                if (ui.pressed(rect)) {
                    baseWarpPointSelection_ = i;
                    ui.emitSound(UiSoundEvent::Confirm);
                    if (startFromSelectedWarpPoint()) {
                        return;
                    }
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                ui.emitSound(UiSoundEvent::Confirm);
                if (startFromSelectedWarpPoint()) {
                    return;
                }
            }
            ui.block(baseMiningWarpPointSelectRect());
            return;
        }

        const UiPageSelectorRects stageSelector = baseMiningStageSelectorRects();
        const int pageDelta = input.activeRingDelta();
        if (pageDelta < 0 && changeSelectedStage(pageDelta)) {
            ui.emitSound(UiSoundEvent::TabSwitch);
            ui.block(basePanelRect());
            return;
        }
        if (pageDelta > 0 && changeSelectedStage(pageDelta)) {
            ui.emitSound(UiSoundEvent::TabSwitch);
            ui.block(basePanelRect());
            return;
        }
        if (input.pressed(InputAction::MoveLeft) || ui.pressed(stageSelectorHitRect(stageSelector.prev))) {
            if (changeSelectedStage(-1)) {
                ui.emitSound(UiSoundEvent::TabSwitch);
                ui.block(basePanelRect());
                return;
            }
        }
        if (input.pressed(InputAction::MoveRight) || ui.pressed(stageSelectorHitRect(stageSelector.next))) {
            if (changeSelectedStage(1)) {
                ui.emitSound(UiSoundEvent::TabSwitch);
                ui.block(basePanelRect());
                return;
            }
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + BaseMiningStartChoiceCount - 1) % BaseMiningStartChoiceCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseMiningStartSelection_ = (baseMiningStartSelection_ + 1) % BaseMiningStartChoiceCount;
        }
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            const UiRect rect = baseMiningStartChoiceRect(i);
            if (rect.contains(ui.mouse())) {
                baseMiningStartSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseMiningStartSelection_ = i;
                if (i == 1 && selectableWarpPoints.empty()) {
                    ui.emitSound(UiSoundEvent::Cancel);
                    baseStatus_ = "解放済みワープポイントがありません";
                    return;
                }
                if (i == 1) {
                    ui.emitSound(UiSoundEvent::MenuOpen);
                    baseWarpPointSelectActive_ = true;
                    baseWarpPointSelection_ = std::clamp(
                        baseWarpPointSelection_,
                        0,
                        static_cast<int>(selectableWarpPoints.size()) - 1);
                    baseStatus_.clear();
                    return;
                }
                if (i == 2) {
                    if (!canRegenerateCurrentStage()) {
                        ui.emitSound(UiSoundEvent::Cancel);
                        baseStatus_ = "全ワープ解放とクリア後に可能";
                        return;
                    }
                    ui.emitSound(UiSoundEvent::MenuOpen);
                    openRegenerateConfirm();
                    return;
                }
                ui.emitSound(UiSoundEvent::Confirm);
                baseRegenerateConfirm_ = {};
                requestMiningStartTransition(false, false);
                return;
            }
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            if (baseMiningStartSelection_ == 1 && selectableWarpPoints.empty()) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "解放済みワープポイントがありません";
                return;
            }
            if (baseMiningStartSelection_ == 1) {
                ui.emitSound(UiSoundEvent::MenuOpen);
                baseWarpPointSelectActive_ = true;
                baseWarpPointSelection_ = std::clamp(
                    baseWarpPointSelection_,
                    0,
                    static_cast<int>(selectableWarpPoints.size()) - 1);
                baseStatus_.clear();
                return;
            }
            if (baseMiningStartSelection_ == 2) {
                if (!canRegenerateCurrentStage()) {
                    ui.emitSound(UiSoundEvent::Cancel);
                    baseStatus_ = "全ワープ解放とクリア後に可能";
                    return;
                }
                ui.emitSound(UiSoundEvent::MenuOpen);
                openRegenerateConfirm();
                return;
            }
            ui.emitSound(UiSoundEvent::Confirm);
            baseRegenerateConfirm_ = {};
            requestMiningStartTransition(false, false);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (input.pausePressed()) {
        ui.emitSound(UiSoundEvent::MenuOpen);
        mode_ = ScreenMode::PauseMenu;
        pauseReturnMode_ = ScreenMode::Base;
        pausePage_ = PauseMenuPage::Main;
        return;
    }

    const auto interact = [this](const BaseFacility& facility) {
        if (!facility.enabled) {
            return;
        }
        switch (facility.onInteract) {
        case BaseFacilityAction::MineExit:
            if (hasBrokenRingItemForDeparture()) {
                openUiConfirmDialog(
                    baseBrokenRingDepartureConfirm_,
                    "出発確認",
                    "壊れたアイテムがリングに乗っています。このまま出発しますか？",
                    "はい",
                    "いいえ",
                    1);
                baseStatus_.clear();
            } else {
                openBaseMiningStartChoice();
            }
            break;
        case BaseFacilityAction::Storage:
            baseStorageActive_ = true;
            baseStorageMode_ = StorageUiMode::ChooseAction;
            baseStorageActionSelection_ = 0;
            baseStorageBulkSelection_ = 0;
            baseStorageDepositSource_ = static_cast<int>(BaseItemSource::Backpack);
            baseStorageDepositSourceTabs_.focusedIndex = 0;
            baseStorageDepositSelection_ = 0;
            baseStorageWithdrawSelection_ = 0;
            baseStorageWarehousePage_ = 0;
            baseStorageQuantityDialog_ = {};
            baseStorageQuantityPending_ = {};
            closeUiCommandMenu(baseStorageCommandMenu_);
            baseStorageCommandOperation_ = StorageQuantityOperation::None;
            baseStorageCommandTarget_ = {};
            baseStoragePointerOperation_ = StorageQuantityOperation::None;
            baseStoragePointerTarget_ = {};
            baseStoragePointerPressMouse_ = {};
            baseStoragePointerPressCanOpenMenu_ = false;
            baseStoragePointerDragTriggered_ = false;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Merchant:
            if (hasStoryFlag("ending_seen") && startStoryEventForTrigger("merchant:post_ending")) {
                break;
            }
            if (merchantRefreshPending_) {
                refreshMerchantStock(true);
                merchantRefreshPending_ = false;
            } else {
                refreshMerchantStock(false);
            }
            baseSellActive_ = true;
            baseMerchantMode_ = MerchantUiMode::ChooseAction;
            baseMerchantActionSelection_ = 0;
            baseMerchantSellSource_ = 0;
            baseMerchantSellSourceTabs_.focusedIndex = baseMerchantSellSource_;
            baseSellSelection_ = 0;
            baseMerchantBuySelection_ = 0;
            closeUiCommandMenu(baseMerchantSellCommandMenu_);
            baseMerchantSellCommandSource_ = 0;
            baseMerchantSellCommandIndex_ = -1;
            closeUiCommandMenu(baseMerchantBuyCommandMenu_);
            baseMerchantBuyCommandIndex_ = -1;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Forge:
            baseUpgradeActive_ = true;
            baseUpgradeSelection_ = 0;
            baseUpgradeTabs_.focusedIndex = baseUpgradeSelection_;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Processing:
            if (hasStoryFlag("ending_seen") && startStoryEventForTrigger("processing:post_ending")) {
                break;
            }
            baseProcessingActive_ = true;
            baseProcessingMode_ = 0;
            baseProcessingTabs_.focusedIndex = baseProcessingMode_;
            baseProcessingSource_ = 0;
            baseProcessingSourceTabs_.focusedIndex = baseProcessingSource_;
            baseProcessingSelection_ = 0;
            closeUiCommandMenu(baseProcessingCommandMenu_);
            baseProcessingCommandSlot_ = -1;
            baseProcessingConfirm_ = {};
            baseProcessingConfirmTarget_ = {};
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Bookshelf:
            openBookshelf();
            break;
        case BaseFacilityAction::Diary:
            openBaseDiary();
            break;
        case BaseFacilityAction::RingWorkshop:
            if (facility.unlocked) {
                openRingWorkshop();
            } else {
                baseStatus_ = "リング工房: まだ解禁されていません";
            }
            break;
        case BaseFacilityAction::HomeEntrance:
            baseOutdoorPlayerPosition_ = basePlayerPosition_;
            {
                const UiRect fallback = defaultBaseFacilityRect(BaseArea::HomeInterior, ringWorkshopUnlocked_, "home_exit");
                const UiRect homeExitRect = toUiRect(baseFacilityRectFor(BaseArea::HomeInterior, "home_exit", toBaseEditRect(fallback)));
                requestBaseAreaCrossfade(
                    BaseArea::HomeInterior,
                    homeInteriorEntryPosition(homeExitRect, balance_.playerRadius),
                    {0.0f, -1.0f},
                    "ルネの家に入りました");
            }
            break;
        case BaseFacilityAction::HomeExit:
            {
                const UiRect fallback = defaultBaseFacilityRect(BaseArea::Outdoor, ringWorkshopUnlocked_, "home_entrance");
                const UiRect homeEntranceRect = toUiRect(baseFacilityRectFor(BaseArea::Outdoor, "home_entrance", toBaseEditRect(fallback)));
                const Vec2 outdoorPosition = baseFacilitySpawnPosition(
                    homeEntranceRect,
                    BaseFacilitySpawnSide::Below,
                    balance_.playerRadius);
                baseOutdoorPlayerPosition_ = outdoorPosition;
                requestBaseAreaCrossfade(
                    BaseArea::Outdoor,
                    outdoorPosition,
                    {0.0f, 1.0f},
                    "魔女の拠点に戻りました");
            }
            break;
        case BaseFacilityAction::MonicaTalk:
            startBaseMonicaDialogue();
            break;
        }
    };

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    const float playerRadius = balance_.playerRadius;
    const auto baseCollision = [&](Vec2 position) {
        const UiRect bounds = baseMapBounds();
        if (position.x - playerRadius < bounds.pos.x ||
            position.y - playerRadius < bounds.pos.y ||
            position.x + playerRadius > bounds.pos.x + bounds.size.x ||
            position.y + playerRadius > bounds.pos.y + bounds.size.y) {
            return true;
        }
        const Vec2 passabilityProbe = playerSpriteFootAnchor(position);
        if (baseArea_ == BaseArea::HomeInterior) {
            const UiRect homeWalk = homeInteriorWalkBounds();
            if (passabilityProbe.x - playerRadius < homeWalk.pos.x ||
                passabilityProbe.y - playerRadius < homeWalk.pos.y ||
                passabilityProbe.x + playerRadius > homeWalk.pos.x + homeWalk.size.x ||
                passabilityProbe.y + playerRadius > homeWalk.pos.y + homeWalk.size.y) {
                return true;
            }
        }
        const int minTileX = static_cast<int>(std::floor((passabilityProbe.x - playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int maxTileX = static_cast<int>(std::floor((passabilityProbe.x + playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int minTileY = static_cast<int>(std::floor((passabilityProbe.y - playerRadius) / static_cast<float>(BaseEditGridSize)));
        const int maxTileY = static_cast<int>(std::floor((passabilityProbe.y + playerRadius) / static_cast<float>(BaseEditGridSize)));
        for (int tileY = minTileY; tileY <= maxTileY; ++tileY) {
            for (int tileX = minTileX; tileX <= maxTileX; ++tileX) {
                if (!isBasePassabilityBlocked(baseArea_, tileX, tileY)) {
                    continue;
                }
                const UiRect tileRect{
                    {static_cast<float>(tileX * BaseEditGridSize), static_cast<float>(tileY * BaseEditGridSize)},
                    {static_cast<float>(BaseEditGridSize), static_cast<float>(BaseEditGridSize)},
                };
                if (circleIntersectsRect(passabilityProbe, playerRadius, tileRect)) {
                    return true;
                }
            }
        }
        return false;
    };

    const Vec2 previousBasePlayerPosition = basePlayerPosition_;
    const Vec2 moveAxis = input.moveAxis();
    const bool walkingNow = lengthSquared(moveAxis) > 0.0001f;
    updateBasePlayerSpriteAnimation(dt, walkingNow);
    const PlayerFootstepSurface baseFootstepSurface = baseArea_ == BaseArea::HomeInterior
        ? PlayerFootstepSurface::HomeInterior
        : PlayerFootstepSurface::BaseOutdoor;
    maybeTriggerPlayerFootstep(
        playerSpriteFootAnchor(basePlayerPosition_),
        lengthSquared(moveAxis) > 0.0001f ? moveAxis : basePlayerFacing_,
        basePlayerSpriteWalking_,
        playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
        previousBasePlayerDustFrame_,
        baseFootstepSurface);
    if (lengthSquared(moveAxis) > 0.0001f) {
        basePlayerFacing_ = normalize(moveAxis);
        const Vec2 delta = moveAxis * balance_.playerSpeed * dt;
        Vec2 next = basePlayerPosition_ + Vec2{delta.x, 0.0f};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
        next = basePlayerPosition_ + Vec2{0.0f, delta.y};
        if (!baseCollision(next)) {
            basePlayerPosition_ = next;
        }
    }

    const Vec2 previousBasePlayerFoot = playerSpriteFootAnchor(previousBasePlayerPosition);
    const Vec2 currentBasePlayerFoot = playerSpriteFootAnchor(basePlayerPosition_);
    const auto editedFacilityRect = [this](BaseArea area, std::string_view facilityId) {
        const UiRect fallback = defaultBaseFacilityRect(area, ringWorkshopUnlocked_, facilityId);
        return toUiRect(baseFacilityRectFor(area, facilityId, toBaseEditRect(fallback)));
    };
    if (baseArea_ == BaseArea::Outdoor) {
        if (const BaseFacility* entrance = findBaseFacilityById(facilities, "home_entrance")) {
            if (pointEnteredRectFromBelow(previousBasePlayerFoot, currentBasePlayerFoot, entrance->rect)) {
                const UiRect homeExitRect = editedFacilityRect(BaseArea::HomeInterior, "home_exit");
                baseOutdoorPlayerPosition_ = basePlayerPosition_;
                requestBaseAreaCrossfade(
                    BaseArea::HomeInterior,
                    homeInteriorEntryPosition(homeExitRect, balance_.playerRadius),
                    {0.0f, -1.0f},
                    "ルネの家に入りました");
                return;
            }
        }
    } else if (baseArea_ == BaseArea::HomeInterior) {
        if (const BaseFacility* exit = findBaseFacilityById(facilities, "home_exit")) {
            if (pointEnteredRectFromAbove(previousBasePlayerFoot, currentBasePlayerFoot, exit->rect)) {
                const UiRect homeEntranceRect = editedFacilityRect(BaseArea::Outdoor, "home_entrance");
                const Vec2 outdoorPosition = baseFacilitySpawnPosition(
                    homeEntranceRect,
                    BaseFacilitySpawnSide::Below,
                    balance_.playerRadius);
                baseOutdoorPlayerPosition_ = outdoorPosition;
                requestBaseAreaCrossfade(
                    BaseArea::Outdoor,
                    outdoorPosition,
                    {0.0f, 1.0f},
                    "魔女の拠点に戻りました");
                return;
            }
        }
    }

    const BaseFacility* interactionFacility = selectBaseInteractionFacility(basePlayerPosition_, basePlayerFacing_, baseArea_, facilities);

    if (input.mouseLeftPressed() && !ui.pointerConsumed()) {
        for (const BaseFacility& facility : facilities) {
            if (baseFacilityHiddenInNormalView(baseArea_, facility)) {
                continue;
            }
            if (!baseFacilityPointerRect(facility, baseArea_, ringWorkshopUnlocked_).contains(ui.mouse())) {
                continue;
            }
            ui.consumePointer();
            if (baseInteractionAvailable(basePlayerPosition_, facility)) {
                ui.emitSound(facility.onInteract == BaseFacilityAction::Bookshelf ? UiSoundEvent::BookOpen : UiSoundEvent::Confirm);
                interact(facility);
            } else if (facility.enabled) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "近くまで移動してください";
            }
            return;
        }
    }

    if (input.confirmPressed() && interactionFacility != nullptr) {
        ui.emitSound(interactionFacility->onInteract == BaseFacilityAction::Bookshelf ? UiSoundEvent::BookOpen : UiSoundEvent::Confirm);
        interact(*interactionFacility);
        return;
    }
}

void Game::renderBookshelfScreen(Renderer& renderer) const
{
    char buffer[256];
    const auto menuName = [](int index) {
        switch (index) {
        case 0:
            return "アイテム図鑑";
        case 1:
            return "モンスター図鑑";
        default:
            return "";
        }
    };
    const auto objectAt = [this](int targetIndex) -> const ObjectDefinition* {
        if (targetIndex < 0 || targetIndex >= static_cast<int>(objectCatalog_.objects.size())) {
            return nullptr;
        }
        return &objectCatalog_.objects[static_cast<std::size_t>(targetIndex)];
    };

    if (bookshelfPage_ == BookshelfPage::Menu) {
        const UiRect panel = bookshelfMenuPanelRect();
        renderer.drawText(smallActionInfoTextPos(panel), "どの図鑑を開きますか？", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BookshelfMenuItemCount; ++i) {
            drawUiButton(renderer, bookshelfMenuChoiceRect(i), menuName(i), i == bookshelfSelection_, uiActionButtonStyle());
        }
        return;
    }

    const UiRect panel = merchantPanelRect();
    const UiRect detailPanel = merchantDetailPanelRect();
    const InventoryUiGridStyle gridStyle = bookshelfGridStyle();
    const UiRect gridViewport = bookshelfGridViewport();
    const int totalCount = bookshelfPage_ == BookshelfPage::Enemies
        ? static_cast<int>(enemyCatalog_.enemies.size())
        : static_cast<int>(objectCatalog_.objects.size());
    const UiScrollAreaLayout gridLayout =
        makeInventoryUiGridLayout(gridViewport, totalCount, bookshelfScrollOffset_, gridStyle);
    int discoveredCount = 0;
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            if (stage != EncyclopediaStage::Undiscovered) {
                ++discoveredCount;
            }
        }
    } else {
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            const EncyclopediaStage stage = encyclopedia_.objectStage(object.id, treasure);
            if (stage != EncyclopediaStage::Undiscovered) {
                ++discoveredCount;
            }
        }
    }

    std::snprintf(buffer, sizeof(buffer), "%d/%d 記録", discoveredCount, totalCount);
    renderer.drawText(panel.pos + Vec2{28.0f, 62.0f}, buffer, {150, 150, 160, 255}, 2);
    if (totalCount <= 0) {
        renderer.drawText(panel.pos + Vec2{28.0f, 154.0f}, "記録対象がありません", {150, 150, 160, 255}, 2);
    } else if (bookshelfPage_ == BookshelfPage::Items) {
        std::vector<InventoryUiEntryView> entries;
        entries.reserve(objectCatalog_.objects.size());
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            InventoryUiEntryView entry{};
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if (encyclopedia_.objectStage(object.id, treasure) != EncyclopediaStage::Undiscovered) {
                entry.item = &object;
                entry.stackCount = 1;
            }
            entries.push_back(entry);
        }
        drawInventoryUiGrid(renderer, gridLayout, entries, bookshelfSelection_, gridStyle);
        renderer.pushClipRect(gridLayout.viewport.pos, gridLayout.viewport.size);
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            if (entries[static_cast<std::size_t>(i)].item != nullptr) {
                continue;
            }
            const UiRect rect = inventoryUiGridSlotRect(gridLayout, i, gridStyle);
            if (!uiScrollAreaRectVisible(gridLayout, rect)) {
                continue;
            }
            const std::string_view unknown = "?";
            const Vec2 textSize = renderer.measureText(unknown, 3);
            renderer.drawOutlinedText(
                rect.pos + (rect.size - textSize) * 0.5f,
                unknown,
                ui::TextMuted,
                {0, 0, 0, 160},
                4,
                3);
        }
        renderer.popClipRect();
    } else {
        renderer.pushClipRect(gridLayout.viewport.pos, gridLayout.viewport.size);
        for (int i = 0; i < static_cast<int>(enemyCatalog_.enemies.size()); ++i) {
            const UiRect rect = inventoryUiGridSlotRect(gridLayout, i, gridStyle);
            if (!uiScrollAreaRectVisible(gridLayout, rect)) {
                continue;
            }
            const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(i)];
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            InventoryUiEntryView emptyEntry{};
            drawInventoryUiSlot(renderer, rect, emptyEntry, InventoryUiSlotStyle{i == bookshelfSelection_, stage == EncyclopediaStage::Undiscovered, gridStyle.imageMaxSize});
            if (stage == EncyclopediaStage::Undiscovered) {
                const std::string_view unknown = "?";
                const Vec2 textSize = renderer.measureText(unknown, 3);
                renderer.drawOutlinedText(
                    rect.pos + (rect.size - textSize) * 0.5f,
                    unknown,
                    ui::TextMuted,
                    {0, 0, 0, 160},
                    4,
                    3);
                continue;
            }
            EnemyImageDrawOptions iconOptions;
            iconOptions.allowUpscale = true;
            iconOptions.outlineColor = {42, 22, 34, 255};
            iconOptions.directionOverrideEnabled = true;
            iconOptions.directionOverride = {0.0f, 1.0f};
            iconOptions.selectedOutlineEnabled = i == bookshelfSelection_;
            (void)drawEnemyImageIcon(
                renderer,
                enemy.imageNumber,
                rect.pos + rect.size * 0.5f,
                {gridStyle.imageMaxSize, gridStyle.imageMaxSize},
                baseRingPreviewAnimationTime_,
                iconOptions);
        }
        renderer.popClipRect();
        drawUiScrollAreaScrollbar(renderer, gridLayout, gridStyle.scroll);
    }

    if (bookshelfPage_ == BookshelfPage::Enemies) {
        if (bookshelfSelection_ >= 0 && bookshelfSelection_ < static_cast<int>(enemyCatalog_.enemies.size())) {
            const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(bookshelfSelection_)];
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            drawUiSubPanel(renderer, detailPanel);
            if (stage == EncyclopediaStage::Undiscovered) {
                float detailY = drawUiDetailHeader(renderer, detailPanel, "未発見");
                drawUiDetailText(renderer, detailPanel, detailY, "まだ記録されていません。ダンジョンで遭遇するとモンスター図鑑に登録されます。");
            } else {
                const std::string name = enemy.name.empty() ? enemy.id : enemy.name;
                float detailY = drawUiDetailHeader(renderer, detailPanel, name);
                EnemyImageDrawOptions imageOptions;
                imageOptions.allowUpscale = true;
                imageOptions.outlineColor = {42, 22, 34, 255};
                imageOptions.directionOverrideEnabled = true;
                imageOptions.directionOverride = {0.0f, 1.0f};
                const Vec2 imageMax{112.0f, 112.0f};
                const Vec2 imageCenter{
                    detailPanel.pos.x + detailPanel.size.x * 0.5f,
                    detailY + imageMax.y * 0.5f,
                };
                if (drawEnemyImageIcon(renderer, enemy.imageNumber, imageCenter, imageMax, baseRingPreviewAnimationTime_, imageOptions)) {
                    detailY += imageMax.y + 12.0f;
                }
                if (stage != EncyclopediaStage::Complete) {
                    drawUiDetailText(renderer, detailPanel, detailY, "？？？");
                    drawUiDetailLine(renderer, detailPanel, detailY, "HP", "？？？");
                    drawUiDetailLine(renderer, detailPanel, detailY, "攻撃力", "？？？");
                    drawUiDetailLine(renderer, detailPanel, detailY, "移動速度", "？？？");
                    drawUiDetailText(renderer, detailPanel, detailY, "虫眼鏡で観察すると詳細が記録されます。");
                } else {
                    drawUiDetailText(renderer, detailPanel, detailY, enemy.description.empty() ? "-" : enemy.description);
                    drawUiDetailLine(renderer, detailPanel, detailY, "HP", std::to_string(enemy.hp));
                    drawUiDetailLine(renderer, detailPanel, detailY, "攻撃力", enemyContactAttackText(enemy));
                    drawUiDetailLine(renderer, detailPanel, detailY, "移動速度", enemyMoveSpeedLabel(enemy.moveSpeed));
                    std::string reward = "EXP ";
                    reward += std::to_string(enemy.xp);
                    reward += " / ";
                    reward += std::to_string(enemy.money);
                    reward += "G";
                    drawUiDetailLine(renderer, detailPanel, detailY, "報酬", reward);
                    if (enemy.captureDifficulty > 0) {
                        drawUiDetailLine(renderer, detailPanel, detailY, "捕獲難度", enemyCaptureDifficultyLabel(enemy.captureDifficulty));
                    }
                    if (!enemy.capturedEffectText.empty()) {
                        drawUiDetailText(renderer, detailPanel, detailY, "捕獲時効果");
                        drawUiDetailText(renderer, detailPanel, detailY, enemy.capturedEffectText);
                    }
                }
            }
        } else {
            drawUiSubPanel(renderer, detailPanel);
            float detailY = drawUiDetailHeader(renderer, detailPanel, "敵未選択");
            drawUiDetailText(renderer, detailPanel, detailY, "敵を選択してください。");
        }
    } else if (const ObjectDefinition* object = objectAt(bookshelfSelection_)) {
        const bool treasure = object->category == "\xE5\xAE\x9D";
        const EncyclopediaStage stage = encyclopedia_.objectStage(object->id, treasure);
        const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object->name.empty() ? object->id : object->name);
        if (stage != EncyclopediaStage::Undiscovered) {
            InventoryUiEntryView detailEntry{};
            detailEntry.item = object;
            detailEntry.stackCount = 1;
            std::vector<InventoryUiDetailExtraLine> extraLines;
            if (isSellableObject(*object)) {
                extraLines.push_back({"売値", std::to_string(sellPrice(*object)) + "G"});
            } else {
                extraLines.push_back({"売値", "売却不可", ui::TextDisabled});
            }
            drawInventoryUiDetailPanel(
                renderer,
                detailPanel,
                detailEntry,
                objectCatalog_,
                encyclopedia_,
                InventoryUiDetailOptions{.animationSeconds = baseRingPreviewAnimationTime_, .showExtraLineSeparator = false},
                extraLines);
        } else {
            drawUiSubPanel(renderer, detailPanel);
            const Vec2 bookshelfDetailContent = uiSubPanelContentPos(detailPanel);
            std::snprintf(buffer, sizeof(buffer), "%s / %s", name.c_str(), encyclopediaStageName(stage));
            renderer.drawText(bookshelfDetailContent, buffer, {255, 230, 150, 255}, 2);
            float detailY = bookshelfDetailContent.y + 36.0f;
            drawUiDetailText(renderer, detailPanel, detailY, "まだ記録されていません。入手するとアイテム図鑑に登録されます。");
        }
    } else {
        drawUiSubPanel(renderer, detailPanel);
        float detailY = drawUiDetailHeader(renderer, detailPanel, "アイテム未選択");
        drawUiDetailText(renderer, detailPanel, detailY, "アイテムを選択してください。");
    }
}

void Game::renderBaseDiaryScreen(Renderer& renderer, UiRect panel) const
{
    const UiRect body = uiBodyRect(panel);
    const DiarySaveSummary& summary = baseDiarySummary_;

    const UiRect recordPanel{
        body.pos + Vec2{12.0f, -26.0f},
        {body.size.x - 24.0f, 280.0f},
    };
    drawUiSubPanel(renderer, recordPanel);
    const UiRect recordContent = uiSubPanelContentRect(recordPanel);

    float y = recordContent.pos.y + 6.0f;
    constexpr float ValueXOffset = 142.0f;
    constexpr float RowHeight = 44.0f;
    const float labelX = recordContent.pos.x;
    const float valueX = recordContent.pos.x + ValueXOffset;
    const float rightX = recordContent.pos.x + recordContent.size.x;

    const auto drawTextRow = [&](std::string_view label, std::string_view value, Color valueColor = ui::Text) {
        renderer.drawText({labelX, y}, label, ui::TextMuted, 2);
        renderer.drawText({valueX, y}, value, valueColor, 2);
        y += RowHeight;
    };

    if (!summary.hasSave && baseDiaryMode_ != BaseDiaryMode::Saved) {
        renderer.drawText({labelX, y}, "記録はありません", ui::TextMuted, 2);
        y += RowHeight;
    } else {
        renderer.drawText({labelX, y}, "進行", ui::TextMuted, 2);
        if (summary.storyCleared) {
            renderer.drawText({valueX, y}, "ストーリークリア", Color{255, 230, 150, 255}, 2);
        } else {
            const std::string stageName = fittedSingleLineText(renderer, summary.latestStageName, 190.0f, 2);
            renderer.drawText({valueX, y}, stageName, ui::Text, 2);

            InlineItemTextStyle warpStyle;
            warpStyle.text = ui::Text;
            warpStyle.scale = 2;
            warpStyle.iconTextGap = 6.0f;
            warpStyle.iconScale = 24.0f / std::max(1.0f, renderer.measureText("0", warpStyle.scale).y);
            const std::string warpText =
                inlineWorldIconTag(worldIconKey(WorldIconId::WarpPoint)) +
                std::to_string(std::max(0, summary.discoveredWarpPoints)) +
                "/" +
                std::to_string(std::max(1, summary.totalWarpPoints));
            drawInlineItemTextRightAligned(renderer, objectCatalog_, {rightX, y}, warpText, warpStyle);
        }
        y += RowHeight;
        drawTextRow("ルネ", "Lv." + std::to_string(std::max(1, summary.playerLevel)));
        drawTextRow("アイテム図鑑", std::to_string(std::clamp(summary.itemCodexPercent, 0, 100)) + "%");
        drawTextRow("モンスター図鑑", std::to_string(std::clamp(summary.enemyCodexPercent, 0, 100)) + "%");
        drawTextRow("プレイ時間", formatDiaryPlayTime(summary.playTimeSeconds));
    }

    const Vec2 messagePos{recordPanel.pos.x + 22.0f, recordPanel.pos.y + recordPanel.size.y + 24.0f};
    if (baseDiaryMode_ == BaseDiaryMode::Confirm) {
        renderer.drawText(messagePos, "保存しますか？", ui::Text, 2);
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 1), "戻る", baseDiarySelection_ == 1, uiCancelButtonStyle());
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 0), "保存する", baseDiarySelection_ == 0, uiActionButtonStyle());
    } else if (baseDiaryMode_ == BaseDiaryMode::Error) {
        const std::string message = baseDiaryMessage_.empty() ? std::string("もう一度試すか、戻ってください。") : baseDiaryMessage_;
        renderer.drawText(messagePos, message, Color{255, 190, 190, 255}, 2);
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 1), "戻る", baseDiarySelection_ == 1, uiCancelButtonStyle());
        drawUiButton(renderer, uiConfirmDialogButtonRect(panel, 0), "再試行", baseDiarySelection_ == 0, uiActionButtonStyle());
    } else {
        renderer.drawText(messagePos, "保存しました。", Color{202, 255, 216, 255}, 2);
        drawUiButton(renderer, uiResultDialogOkButtonRect(panel), "閉じる", true, uiActionButtonStyle());
    }
}

void Game::updateBasePlayerSpriteAnimation(float dt, bool walking)
{
    if (walking != basePlayerSpriteWalking_) {
        basePlayerSpriteWalking_ = walking;
        basePlayerSpriteAnimationTime_ = 0.0f;
    } else {
        basePlayerSpriteAnimationTime_ += std::max(0.0f, dt);
    }
}

void Game::renderBaseBackdrop(Renderer& renderer) const
{
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    if (baseArea_ == BaseArea::HomeInterior) {
        drawHomeInteriorBackdrop(renderer);
    } else {
        if (renderer.hasBaseMapTexture()) {
            renderer.drawBaseMapTexture(map.pos, map.size);
        } else {
            renderer.fillRect(map.pos, map.size, {68, 96, 58, 255});
            renderer.drawRect(map.pos, map.size, {156, 128, 82, 255});
            renderer.fillRect({62.0f, 456.0f}, {1156.0f, 88.0f}, {98, 84, 58, 255});
            renderer.fillRect({566.0f, 130.0f}, {132.0f, 430.0f}, {92, 78, 54, 255});
            renderer.fillRect({330.0f, 72.0f}, {154.0f, 100.0f}, {96, 54, 62, 255});
            renderer.drawRect({330.0f, 72.0f}, {154.0f, 100.0f}, {216, 184, 130, 255});
            renderer.drawText({350.0f, 106.0f}, "ルネの家", {246, 235, 255, 255}, 2);
            renderer.fillRect({600.0f, 586.0f}, {80.0f, 34.0f}, {38, 30, 36, 255});
            renderer.drawCircle({640.0f, 602.0f}, 42.0f, {160, 122, 80, 255});
        }
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    const Vec2 mouse{mouseX, mouseY};
    drawBaseFacilities(renderer, facilities, baseArea_, ringWorkshopUnlocked_, basePlayerPosition_, mouse);
    renderBaseEditOverlay(renderer);

    const Vec2 basePlayerFootAnchor = playerSpriteFootAnchor(basePlayerPosition_);
    renderer.drawActorShadow(basePlayerFootAnchor, PlayerSpriteDrawSize);
    renderPlayerFootstepDust(renderer);
    if (renderer.hasPlayerSheet()) {
        renderer.drawPlayerSprite(
            playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
            basePlayerFootAnchor,
            PlayerSpriteDrawSize,
            basePlayerFacing_.x > 0.0f,
            {255, 255, 255, 255},
            {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
    } else {
        renderer.fillCircle(basePlayerPosition_, balance_.playerRadius, {118, 72, 168, 255});
        renderer.drawLine(basePlayerPosition_, basePlayerPosition_ + basePlayerFacing_ * 22.0f, {235, 210, 255, 255});
    }

    renderTopInfoBar(renderer);
}

void Game::renderBaseScreen(Renderer& renderer) const
{
    if (!basePresentationActive()) {
        return;
    }

    renderer.setScreenSpace();
    const float ringPreviewSeconds = baseRingPreviewAnimationTime_;
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    if (baseArea_ == BaseArea::HomeInterior) {
        drawHomeInteriorBackdrop(renderer);
    } else {
        if (renderer.hasBaseMapTexture()) {
            renderer.drawBaseMapTexture(map.pos, map.size);
        } else {
            renderer.fillRect(map.pos, map.size, {68, 96, 58, 255});
            renderer.drawRect(map.pos, map.size, {156, 128, 82, 255});
        renderer.fillRect({62.0f, 456.0f}, {1156.0f, 88.0f}, {98, 84, 58, 255});
        renderer.fillRect({566.0f, 130.0f}, {132.0f, 430.0f}, {92, 78, 54, 255});
        renderer.fillRect({330.0f, 72.0f}, {154.0f, 100.0f}, {96, 54, 62, 255});
        renderer.drawRect({330.0f, 72.0f}, {154.0f, 100.0f}, {216, 184, 130, 255});
        renderer.drawText({350.0f, 106.0f}, "ルネの家", {246, 235, 255, 255}, 2);
        renderer.fillRect({600.0f, 586.0f}, {80.0f, 34.0f}, {38, 30, 36, 255});
            renderer.drawCircle({640.0f, 602.0f}, 42.0f, {160, 122, 80, 255});
        }
    }

    std::vector<BaseFacility> facilities = baseFacilities(baseArea_, ringWorkshopUnlocked_);
    for (BaseFacility& facility : facilities) {
        facility.rect = toUiRect(baseFacilityRectFor(baseArea_, facility.facilityId, toBaseEditRect(facility.rect)));
    }
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    const Vec2 mouse{mouseX, mouseY};
    const BaseFacility* interactionFacility = selectBaseInteractionFacility(basePlayerPosition_, basePlayerFacing_, baseArea_, facilities);
    drawBaseFacilities(renderer, facilities, baseArea_, ringWorkshopUnlocked_, basePlayerPosition_, mouse);
    renderBaseEditOverlay(renderer);

    const Vec2 basePlayerFootAnchor = playerSpriteFootAnchor(basePlayerPosition_);
    renderer.drawActorShadow(basePlayerFootAnchor, PlayerSpriteDrawSize);
    renderPlayerFootstepDust(renderer);
    if (renderer.hasPlayerSheet()) {
        renderer.drawPlayerSprite(
            playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
            basePlayerFootAnchor,
            PlayerSpriteDrawSize,
            basePlayerFacing_.x > 0.0f,
            {255, 255, 255, 255},
            {PlayerSpriteAnchorX, PlayerSpriteAnchorY});
    } else {
        renderer.fillCircle(basePlayerPosition_, balance_.playerRadius, {118, 72, 168, 255});
        renderer.drawLine(basePlayerPosition_, basePlayerPosition_ + basePlayerFacing_ * 22.0f, {235, 210, 255, 255});
    }

    renderTopInfoBar(renderer);

    char buffer[256];
    const bool panelUiActive = baseRingWorkshopActive_ ||
        baseDiaryActive_ ||
        baseBookshelfActive_ ||
        baseStorageActive_ ||
        baseProcessingActive_ ||
        baseSellActive_ ||
        baseUpgradeActive_ ||
        baseMiningStartChoiceActive_;
    const bool bottomControlHelpBlocked =
        dialogue_.active() ||
        pendingStoryTriggerDelayActive() ||
        !pendingStoryTrigger_.empty() ||
        !pendingStoryTriggers_.empty() ||
        firstItemAcquisitionNoticeActive();
    const bool storageActionDialogActive = baseStorageActive_ &&
        (baseStorageMode_ == StorageUiMode::ChooseAction || baseStorageMode_ == StorageUiMode::Bulk);
    const bool merchantActionDialogActive = baseSellActive_ && baseMerchantMode_ == MerchantUiMode::ChooseAction;
    const bool bookshelfMenuDialogActive = baseBookshelfActive_ && bookshelfPage_ == BookshelfPage::Menu;
    const bool bookshelfWideActive = baseBookshelfActive_ && bookshelfPage_ != BookshelfPage::Menu;
    const bool ringWorkshopActionDialogActive = baseRingWorkshopActive_ && baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction;
    const bool ringWorkshopWideActive = baseRingWorkshopActive_ && baseRingWorkshopMode_ != RingWorkshopMode::ChooseAction;
    const UiRect panel = baseDiaryActive_
        ? basePanelRect()
        : (storageActionDialogActive
        ? (baseStorageMode_ == StorageUiMode::Bulk ? storageBulkDialogRect() : storageActionDialogRect())
        : (merchantActionDialogActive
        ? merchantActionDialogRect()
        : (bookshelfMenuDialogActive
        ? bookshelfMenuPanelRect()
        : (ringWorkshopActionDialogActive
        ? ringWorkshopActionDialogRect()
        : ((baseProcessingActive_ ||
        bookshelfWideActive ||
        ringWorkshopWideActive ||
        (baseStorageActive_ && !storageActionDialogActive) ||
        (baseSellActive_ && baseMerchantMode_ != MerchantUiMode::ChooseAction))
        ? merchantPanelRect()
        : (baseUpgradeActive_ ? baseUpgradePanelRect() : basePanelRect()))))));
    std::optional<UiWindowScope> panelWindow;
    std::optional<UiCancelControlScope> panelCancelScope;
    if (panelUiActive) {
        const char* panelTitle = "魔女の拠点";
        if (baseBookshelfActive_) {
            panelTitle = bookshelfPage_ == BookshelfPage::Items
                ? "アイテム図鑑"
                : (bookshelfPage_ == BookshelfPage::Enemies ? "モンスター図鑑" : "本棚");
        } else if (baseRingWorkshopActive_) {
            panelTitle = "リング工房";
        } else if (baseProcessingActive_) {
            panelTitle = "作業台";
        } else if (baseSellActive_) {
            if (baseMerchantMode_ == MerchantUiMode::Buy) {
                panelTitle = "商人ワゴン 購入";
            } else if (baseMerchantMode_ == MerchantUiMode::Sell) {
                panelTitle = "商人ワゴン 売却";
            } else {
                panelTitle = "商人ワゴン";
            }
        } else if (baseStorageActive_) {
            if (baseStorageMode_ == StorageUiMode::Deposit) {
                panelTitle = "収納箱にしまう";
            } else if (baseStorageMode_ == StorageUiMode::Withdraw) {
                panelTitle = "収納箱から取り出す";
            } else if (baseStorageMode_ == StorageUiMode::Bulk) {
                panelTitle = "収納箱 一括操作";
            } else {
                panelTitle = "収納箱";
            }
        } else if (baseUpgradeActive_) {
            panelTitle = "拠点強化炉";
        } else if (baseDiaryActive_) {
            panelTitle = "日記";
        } else if (baseMiningStartChoiceActive_) {
            panelTitle = "ダンジョン入口";
        }
        const bool panelCancelButton = true;
        if (panelCancelButton) {
            panelCancelScope.emplace(baseCancelState_);
        }
        panelWindow.emplace(renderer, "base.panel", panel, panelTitle, "", UiWindowOptions{true, panelCancelButton});
    }

    if (baseDiaryActive_) {
        renderBaseDiaryScreen(renderer, panel);
    } else if (baseStorageActive_) {
        if (baseStorageMode_ == StorageUiMode::ChooseAction) {
            std::snprintf(buffer, sizeof(buffer), "収納数：%d/%d", warehouseUsedSlots(), warehouseCapacity());
            renderer.drawText(smallActionInfoTextPos(panel), buffer, {198, 198, 206, 255}, 2);
            constexpr std::array<std::string_view, 3> Choices{"しまう", "取り出す", "一括操作"};
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                drawUiButton(renderer, storageActionChoiceRect(i), Choices[static_cast<std::size_t>(i)], i == baseStorageActionSelection_, uiActionButtonStyle());
            }
        } else if (baseStorageMode_ == StorageUiMode::Bulk) {
            std::snprintf(buffer, sizeof(buffer), "収納数：%d/%d", warehouseUsedSlots(), warehouseCapacity());
            renderer.drawText(smallActionInfoTextPos(panel), buffer, {198, 198, 206, 255}, 2);
            constexpr std::array<std::string_view, 4> Choices{
                "全部しまう",
                "プリセット1を準備",
                "プリセット2を準備",
                "プリセット3を準備",
            };
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                UiButtonStyle style = uiActionButtonStyle();
                const bool enabled = i == 0 || ringPresets_.registered(i - 1);
                if (!enabled) {
                    style.fill = {18, 24, 42, 150};
                    style.fillHot = {18, 24, 42, 150};
                    style.outline = {120, 122, 138, 120};
                    style.outlineHot = style.outline;
                    style.text = ui::TextDisabled;
                    style.imageTint = {180, 180, 190, 130};
                    style.imageTintHot = {190, 190, 200, 150};
                }
                drawUiButton(
                    renderer,
                    storageBulkChoiceRect(i),
                    Choices[static_cast<std::size_t>(i)],
                    enabled && i == baseStorageBulkSelection_,
                    style);
            }
        } else {
            const UiRect detailPanel = merchantDetailPanelRect();
            InventoryUiEntryView detailEntry{};
            const SpellRingItem* selectedRingItem = nullptr;
            if (baseStorageMode_ == StorageUiMode::Deposit) {
                const int sourceCount = storageDepositSourceCountForUnlockedRings(unlockedRingCount());
                std::array<UiTabItem, StorageDepositSourceCount> sourceTabs{};
                std::array<UiRect, StorageDepositSourceCount> sourceTabRects{};
                for (int i = 0; i < sourceCount; ++i) {
                    const int source = storageDepositSourceValue(i);
                    sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(source)], true};
                    sourceTabRects[static_cast<std::size_t>(i)] = storageDepositSourceRect(i);
                }
                const int currentTab = storageDepositSourceTabIndex(baseStorageDepositSource_);
                drawUiTabs(
                    renderer,
                    baseStorageDepositSourceTabs_,
                    currentTab,
                    sourceTabs.data(),
                    sourceCount,
                    sourceTabRects.data());

                std::snprintf(buffer, sizeof(buffer), "収納箱 %d/%d", warehouseUsedSlots(), warehouseCapacity());
                renderer.drawText(storageTransferCountTextPos(), buffer, ui::TextMuted, 2);

                if (baseItemSourceIsRing(baseStorageDepositSource_)) {
                    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseStorageDepositSource_), 0, SpellRingCount - 1);
                    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                    const int selectedRingIndex = ringItems.empty()
                        ? -1
                        : std::clamp(baseStorageDepositSelection_, 0, static_cast<int>(ringItems.size()) - 1);
                    drawStorageRingPreview(
                        renderer,
                        spellRing_,
                        objectCatalog_,
                        balance_,
                        ringIndex,
                        selectedRingIndex,
                        ringPreviewSeconds);
                    for (int i = 0; i < static_cast<int>(ringItems.size()); ++i) {
                        const SpellRingItem& item = ringItems[static_cast<std::size_t>(i)];
                        if (!item.objectId.empty()) {
                            continue;
                        }
                        UiRect labelRect = storageRingItemRect(
                            item,
                            spellRing_,
                            balance_,
                            ringIndex,
                            i,
                            static_cast<int>(ringItems.size()),
                            ringPreviewSeconds);
                        labelRect.size.y += MerchantSellRingItemLabelExtraHeight;
                        drawInventoryUiSlotBottomLabel(renderer, labelRect, "収納不可", ui::TextDisabled);
                    }
                    if (ringItems.empty()) {
                        const Vec2 emptySize = renderer.measureText("アイテム未配置", 2);
                        renderer.drawText(
                            storageRingPreviewCenter(spellRing_, ringIndex) - emptySize * 0.5f,
                            "アイテム未配置",
                            ui::TextMuted,
                            2);
                    } else if (selectedRingIndex >= 0) {
                        selectedRingItem = &ringItems[static_cast<std::size_t>(selectedRingIndex)];
                        detailEntry = storageTransferTargetView(storageDepositTargetForSourceSlot(baseStorageDepositSource_, selectedRingIndex));
                    }
                } else {
                    for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                        const StorageTransferTarget target = storageDepositTargetForSourceSlot(baseStorageDepositSource_, i);
                        const InventoryUiEntryView view = storageTransferTargetView(target);
                        const bool draggingThis =
                            baseStoragePointerDragTriggered_ &&
                            baseStoragePointerOperation_ == StorageQuantityOperation::Deposit &&
                            baseStoragePointerTarget_.source == BaseItemSource::Backpack &&
                            baseStoragePointerTarget_.slotIndex == i;
                        InventoryUiSlotStyle style{i == baseStorageDepositSelection_, draggingThis, 48.0f};
                        if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                            style.showTopRightCount = true;
                            style.topRightCount = view.stackCount;
                        }
                        drawInventoryUiSlot(renderer, storageTransferGridSlotRect(i), view, style);
                    }
                    detailEntry = storageTransferTargetView(storageDepositTargetForScreenSlot(
                        std::clamp(baseStorageDepositSelection_, 0, std::max(0, inventory_.screenSlotCount() - 1))));
                    drawUiButton(renderer, storageTransferSortButtonRect(), "並び替え", false, uiActionButtonStyle());
                }
            } else if (baseStorageMode_ == StorageUiMode::Withdraw) {
                const int warehousePageCount = std::max(1, (warehouseCapacity() + StorageWithdrawSlotCount - 1) / StorageWithdrawSlotCount);
                const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                std::snprintf(buffer, sizeof(buffer), "収納箱 %d/%d", warehouseUsedSlots(), warehouseCapacity());
                renderer.drawText(storageWithdrawCountTextPos(), buffer, ui::TextMuted, 2);
                drawStorageWithdrawHeader(renderer, warehousePage, warehousePageCount);
                for (int i = 0; i < StorageWithdrawSlotCount; ++i) {
                    const StorageTransferTarget target = storageWithdrawTargetForSlot(i);
                    const InventoryUiEntryView view = storageTransferTargetView(target);
                    const bool draggingThis =
                        baseStoragePointerDragTriggered_ &&
                        baseStoragePointerOperation_ == StorageQuantityOperation::Withdraw &&
                        baseStoragePointerTarget_.source == BaseItemSource::Warehouse &&
                        baseStoragePointerTarget_.slotIndex == i;
                    InventoryUiSlotStyle style{i == baseStorageWithdrawSelection_, draggingThis, 48.0f};
                    if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                        style.showTopRightCount = true;
                        style.topRightCount = view.stackCount;
                    }
                    drawInventoryUiSlot(renderer, storageWithdrawSlotRect(i), view, style);
                }
                detailEntry = storageTransferTargetView(storageWithdrawTargetForSlot(
                    std::clamp(baseStorageWithdrawSelection_, 0, StorageWithdrawSlotCount - 1)));
                drawUiButton(renderer, storageWithdrawSortButtonRect(), "並び替え", false, uiActionButtonStyle());
            }

            if (detailEntry.item == nullptr && selectedRingItem != nullptr) {
                drawUiSubPanel(renderer, detailPanel);
                float detailLineY = drawUiDetailHeader(renderer, detailPanel, ringItemDisplayName(objectCatalog_, *selectedRingItem));
                drawUiDetailText(renderer, detailPanel, detailLineY, "このアイテムは収納箱にしまえません。");
            } else {
                drawInventoryUiDetailPanel(
                    renderer,
                    detailPanel,
                    detailEntry,
                    objectCatalog_,
                    encyclopedia_,
                    InventoryUiDetailOptions{.animationSeconds = ringPreviewSeconds});
            }
            const char* commandLabel = baseStorageCommandOperation_ == StorageQuantityOperation::Withdraw
                ? "取り出す"
                : "しまう";
            const std::array<UiCommandMenuItem, 1> commandItems{{
                {commandLabel, storageTransferTargetAvailable(baseStorageCommandTarget_)},
            }};
            drawUiCommandMenu(renderer, baseStorageCommandMenu_, commandItems.data(), static_cast<int>(commandItems.size()));
        }
    } else if (baseBookshelfActive_) {
        renderBookshelfScreen(renderer);
    } else if (baseRingWorkshopActive_) {
        if (baseRingWorkshopMode_ == RingWorkshopMode::ChooseAction) {
            renderer.drawText(smallActionInfoTextPos(panel), "何を調整しますか？", {198, 198, 206, 255}, 2);
            for (int i = 0; i < RingWorkshopActionCount; ++i) {
                drawUiButton(
                    renderer,
                    ringWorkshopActionChoiceRect(i),
                    ringWorkshopActionLabel(i),
                    i == baseRingWorkshopSelection_,
                    uiActionButtonStyle());
            }
        } else if (baseRingWorkshopMode_ == RingWorkshopMode::Respec) {
            const int ringCount = unlockedRingCount();
            const int ringIndex = std::clamp(baseRingWorkshopRingIndex_, 0, ringCount - 1);
            std::array<UiTabItem, SpellRingCount> ringTabs{};
            std::array<UiRect, SpellRingCount> ringTabRects{};
            std::array<std::string, SpellRingCount> ringTabLabels{};
            for (int i = 0; i < ringCount; ++i) {
                ringTabLabels[static_cast<std::size_t>(i)] = "リング " + std::to_string(i + 1);
                ringTabs[static_cast<std::size_t>(i)] = {ringTabLabels[static_cast<std::size_t>(i)], true};
                ringTabRects[static_cast<std::size_t>(i)] = ringWorkshopRingTabRect(i, ringCount);
            }
            drawUiTabs(
                renderer,
                baseRingWorkshopRingTabs_,
                ringIndex,
                ringTabs.data(),
                ringCount,
                ringTabRects.data());

            const UiRect respecPanel = ringWorkshopRespecPanelRect();
            drawUiSubPanel(renderer, respecPanel);
            renderer.drawText(respecPanel.pos + Vec2{24.0f, 22.0f}, "配分再調整", ui::Text, 3);
            std::snprintf(buffer, sizeof(buffer), "合計強化点 %d", ringLevelUpgradePointTotal());
            renderer.drawText(respecPanel.pos + Vec2{respecPanel.size.x - 168.0f, 29.0f}, buffer, ui::TextMuted, 2);

            const RingLevelUpgradePoints& currentRingPoints = levelRingUpgradePoints_[static_cast<std::size_t>(ringIndex)];
            const RingLevelUpgradePoints& draftRingPoints = ringWorkshopDraftUpgradePoints_[static_cast<std::size_t>(ringIndex)];
            for (int i = 0; i < RingLevelUpgradeKindCount; ++i) {
                const RingLevelUpgradeKind kind = ringWorkshopKindForIndex(i);
                const int currentPoints = ringLevelUpgradePoint(currentRingPoints, kind);
                const int draftPoints = ringLevelUpgradePoint(draftRingPoints, kind);
                const RingLevelUpgradeSelection selection{ringIndex, kind};
                const bool sourceSelected = ringWorkshopRespecSource_ &&
                    sameRingLevelUpgradeSelection(*ringWorkshopRespecSource_, selection);
                std::snprintf(buffer, sizeof(buffer), "%d -> %d", currentPoints, draftPoints);
                UiSmallSelectButtonStyle style;
                style.valueText = sourceSelected ? Color{255, 230, 150, 255} : ui::TextMuted;
                drawUiSmallSelectButton(
                    renderer,
                    ringWorkshopRespecKindRect(i),
                    ringLevelUpgradeKindName(kind),
                    buffer,
                    i == baseRingWorkshopSelection_ || sourceSelected,
                    false,
                    style);
            }

            const UiRect detailPanel = ringWorkshopDetailPanelRect();
            drawUiSubPanel(renderer, detailPanel);
            const int selectedKindIndex = std::clamp(baseRingWorkshopSelection_, 0, RingLevelUpgradeKindCount - 1);
            const RingLevelUpgradeKind selectedKind = ringWorkshopKindForIndex(selectedKindIndex);
            const auto valueForPoints = [this](int selectedRing, RingLevelUpgradeKind kind, const RingLevelUpgradePoints& points) {
                switch (kind) {
                case RingLevelUpgradeKind::Radius:
                    return effectiveInitialRingRadiusForRing(selectedRing, points.radius);
                case RingLevelUpgradeKind::Speed:
                    return effectiveInitialRingSpeedForRing(selectedRing, points.speed);
                case RingLevelUpgradeKind::WeightLimit:
                    return effectiveInitialRingWeightLimitForRing(selectedRing, points.weightLimit);
                }
                return 0.0f;
            };
            float detailY = drawUiDetailHeader(
                renderer,
                detailPanel,
                baseRingWorkshopSelection_ == RingLevelUpgradeKindCount ? "再調整確定" : ringLevelUpgradeKindName(selectedKind));
            std::snprintf(buffer, sizeof(buffer), "リング %d", ringIndex + 1);
            drawUiDetailLine(renderer, detailPanel, detailY, "対象", buffer);
            const int selectedCurrentPoints = ringLevelUpgradePoint(currentRingPoints, selectedKind);
            const int selectedDraftPoints = ringLevelUpgradePoint(draftRingPoints, selectedKind);
            std::snprintf(buffer, sizeof(buffer), "%d点 / %s",
                selectedCurrentPoints,
                formatRingWorkshopValue(selectedKind, valueForPoints(ringIndex, selectedKind, currentRingPoints)).c_str());
            drawUiDetailLine(renderer, detailPanel, detailY, "現在", buffer);
            std::snprintf(buffer, sizeof(buffer), "%d点 / %s",
                selectedDraftPoints,
                formatRingWorkshopValue(selectedKind, valueForPoints(ringIndex, selectedKind, draftRingPoints)).c_str());
            drawUiDetailLine(
                renderer,
                detailPanel,
                detailY,
                "配分案",
                buffer,
                selectedCurrentPoints == selectedDraftPoints ? ui::Text : Color{255, 230, 150, 255});
            if (ringWorkshopRespecSource_) {
                std::snprintf(buffer, sizeof(buffer), "リング%d %s",
                    ringWorkshopRespecSource_->ringIndex + 1,
                    ringLevelUpgradeKindName(ringWorkshopRespecSource_->kind));
                drawUiDetailLine(renderer, detailPanel, detailY, "移動元", buffer, Color{255, 230, 150, 255});
                drawUiDetailText(renderer, detailPanel, detailY, "次に選んだ項目へ1点移します。");
            } else {
                drawUiDetailLine(renderer, detailPanel, detailY, "移動元", "未選択", ui::TextMuted);
                drawUiDetailText(renderer, detailPanel, detailY, "ポイントがある項目を選ぶと移動元になります。");
            }
            std::snprintf(buffer, sizeof(buffer), "%dG", ringWorkshopRespecMoneyCost());
            drawUiDetailLine(
                renderer,
                detailPanel,
                detailY,
                "費用",
                buffer,
                money_ >= ringWorkshopRespecMoneyCost() ? ui::Text : Color{238, 82, 82, 255});
            std::snprintf(buffer, sizeof(buffer), "%d / 所持 %d",
                ringWorkshopRespecMoonCost(),
                inventory_.materialCount(MaterialType::MoonFragment));
            drawUiDetailLine(
                renderer,
                detailPanel,
                detailY,
                "月のカケラ",
                buffer,
                inventory_.materialCount(MaterialType::MoonFragment) >= ringWorkshopRespecMoonCost() ? ui::Text : Color{238, 82, 82, 255});

            UiButtonStyle confirmStyle = uiActionButtonStyle();
            if (!ringWorkshopRespecChanged()) {
                confirmStyle.text = ui::TextMuted;
            }
            drawUiButton(
                renderer,
                ringWorkshopRespecConfirmRect(),
                ringWorkshopRespecChanged() ? "再調整確定" : "変更なし",
                baseRingWorkshopSelection_ == RingLevelUpgradeKindCount,
                confirmStyle);
        } else if (baseRingWorkshopMode_ == RingWorkshopMode::Upgrade) {
            const UiRect listPanel = ringWorkshopUpgradeListPanelRect();
            drawUiSubPanel(renderer, listPanel);
            renderer.drawText(listPanel.pos + Vec2{24.0f, 22.0f}, "工房強化", ui::Text, 3);
            for (int i = 0; i < RingWorkshopUpgradeDisplayCount; ++i) {
                const bool implemented = i < RingWorkshopImplementedUpgradeCount;
                UiSmallSelectButtonStyle style;
                const char* value = "未解禁";
                if (implemented) {
                    const auto upgrade = static_cast<RingWorkshopUpgrade>(i);
                    const int level = ringWorkshopUpgradeLevel(upgrade);
                    const int maxLevel = ringWorkshopUpgradeMaxLevel(upgrade);
                    if (level >= maxLevel) {
                        value = "上限";
                        style.valueText = Color{160, 220, 190, 255};
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "Lv.%d/%d", level, maxLevel);
                        value = buffer;
                    }
                }
                drawUiSmallSelectButton(
                    renderer,
                    ringWorkshopUpgradeItemRect(i),
                    ringWorkshopUpgradeShortName(i),
                    value,
                    i == baseRingWorkshopSelection_,
                    !implemented,
                    style);
            }

            const UiRect detailPanel = ringWorkshopDetailPanelRect();
            drawUiSubPanel(renderer, detailPanel);
            const int selected = std::clamp(baseRingWorkshopSelection_, 0, RingWorkshopUpgradeDisplayCount - 1);
            const bool implemented = selected < RingWorkshopImplementedUpgradeCount;
            const auto formatUpgradeValue = [](RingWorkshopUpgrade upgrade, float value) {
                char valueBuffer[64];
                switch (upgrade) {
                case RingWorkshopUpgrade::InitialRadius:
                case RingWorkshopUpgrade::ShiftDistance:
                    std::snprintf(valueBuffer, sizeof(valueBuffer), "%.0fpx", value);
                    break;
                case RingWorkshopUpgrade::InitialSpeed:
                    std::snprintf(valueBuffer, sizeof(valueBuffer), "%.2f", value);
                    break;
                }
                return std::string(valueBuffer);
            };
            const char* confirmLabel = "強化する";
            UiButtonStyle confirmStyle = uiActionButtonStyle();
            if (implemented) {
                const auto upgrade = static_cast<RingWorkshopUpgrade>(selected);
                const int level = ringWorkshopUpgradeLevel(upgrade);
                const int maxLevel = ringWorkshopUpgradeMaxLevel(upgrade);
                const bool maxed = level >= maxLevel;
                float detailY = drawUiDetailHeader(renderer, detailPanel, ringWorkshopUpgradeName(upgrade));
                std::snprintf(buffer, sizeof(buffer), "Lv.%d/%d", level, maxLevel);
                drawUiDetailLine(renderer, detailPanel, detailY, "段階", buffer, maxed ? Color{160, 220, 190, 255} : ui::Text);
                if (maxed) {
                    drawUiDetailLine(renderer, detailPanel, detailY, "効果", "上限到達済み", ui::TextMuted);
                    drawUiDetailLine(renderer, detailPanel, detailY, "必要素材", "なし", ui::TextMuted);
                    confirmLabel = "上限";
                    confirmStyle.text = ui::TextMuted;
                } else {
                    const std::string currentValue = formatUpgradeValue(upgrade, ringWorkshopUpgradeCurrentValue(upgrade));
                    const std::string nextValue = formatUpgradeValue(upgrade, ringWorkshopUpgradeNextValue(upgrade));
                    drawUiDetailLine(renderer, detailPanel, detailY, "効果", currentValue + " -> " + nextValue, Color{255, 230, 150, 255});
                    std::snprintf(buffer, sizeof(buffer), "%dG", ringWorkshopUpgradeMoneyCost(upgrade));
                    drawUiDetailLine(
                        renderer,
                        detailPanel,
                        detailY,
                        "費用",
                        buffer,
                        money_ >= ringWorkshopUpgradeMoneyCost(upgrade) ? ui::Text : Color{238, 82, 82, 255});
                    std::snprintf(buffer, sizeof(buffer), "%d / 所持 %d",
                        ringWorkshopUpgradeMoonCost(upgrade),
                        inventory_.materialCount(MaterialType::MoonFragment));
                    drawUiDetailLine(
                        renderer,
                        detailPanel,
                        detailY,
                        "月のカケラ",
                        buffer,
                        inventory_.materialCount(MaterialType::MoonFragment) >= ringWorkshopUpgradeMoonCost(upgrade) ? ui::Text : Color{238, 82, 82, 255});
                }
            } else {
                float detailY = drawUiDetailHeader(renderer, detailPanel, ringWorkshopFutureUpgradeName(selected));
                drawUiDetailLine(renderer, detailPanel, detailY, "状態", "未解禁", ui::TextMuted);
                drawUiDetailText(renderer, detailPanel, detailY, "今後の工房拡張で利用予定です。");
                confirmLabel = "未解禁";
                confirmStyle.text = ui::TextDisabled;
            }
            drawUiButton(
                renderer,
                ringWorkshopUpgradeConfirmRect(),
                confirmLabel,
                false,
                confirmStyle);
        }
    } else if (baseProcessingActive_) {
        const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount());
        std::array<UiTabItem, BaseProcessingSourceCount> sourceTabs{};
        std::array<UiRect, BaseProcessingSourceCount> sourceTabRects{};
        for (int i = 0; i < sourceCount; ++i) {
            sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(i)], true};
            sourceTabRects[static_cast<std::size_t>(i)] = baseProcessingSourceRect(i, sourceCount);
        }
        drawUiTabs(
            renderer,
            baseProcessingSourceTabs_,
            baseProcessingSource_,
            sourceTabs.data(),
            sourceCount,
            sourceTabRects.data());

        const auto entryViewForScreenSlot = [this](int slot) {
            InventoryUiEntryView view{};
            const ProcessingTarget target = processingTargetForScreenSlot(slot);
            if (!target.valid) {
                return view;
            }
            if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
                return storageEntryView(target.backpackEntry, target.warehouseEntry);
            }

            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
            if (target.ringItemIndex >= 0 && target.ringItemIndex < static_cast<int>(ringItems.size())) {
                const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
                view.item = objectForRingItem(objectCatalog_, ringItem);
                view.stats = inventoryUiStatsFromRingItem(ringItem);
                view.stackCount = 1;
            }
            return view;
        };

        const bool warehouseSource = baseItemSourceIsWarehouse(baseProcessingSource_);
        const bool ringSource = baseItemSourceIsRing(baseProcessingSource_);
        if (ringSource) {
            const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseProcessingSource_), 0, SpellRingCount - 1);
            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
            const int selectedRingItem = ringItems.empty()
                ? -1
                : std::clamp(baseProcessingSelection_, 0, static_cast<int>(ringItems.size()) - 1);
            drawBaseProcessingRingPreview(
                renderer,
                spellRing_,
                objectCatalog_,
                balance_,
                ringIndex,
                selectedRingItem,
                ringPreviewSeconds);
            if (ringItems.empty()) {
                const Vec2 emptySize = renderer.measureText("アイテム未配置", 2);
                renderer.drawText(
                    baseProcessingRingPreviewCenter(spellRing_, ringIndex) - emptySize * 0.5f,
                    "アイテム未配置",
                    ui::TextMuted,
                    2);
            }
        } else if (warehouseSource) {
            const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
            const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
            drawExternalWarehouseSourceHeader(
                renderer,
                baseProcessingGridSlotRect,
                warehousePage,
                warehousePageCount);
            for (int i = 0; i < StoragePaneSlotCount; ++i) {
                const InventoryUiEntryView view = entryViewForScreenSlot(i);
                const bool unavailable = view.item != nullptr && !processingTargetHasAvailableCommand(processingTargetForScreenSlot(i));
                InventoryUiSlotStyle style{i == baseProcessingSelection_, unavailable, 48.0f};
                if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                    style.showTopRightCount = true;
                    style.topRightCount = view.stackCount;
                }
                drawInventoryUiSlot(renderer, externalWarehouseSourceSlotRect(baseProcessingGridSlotRect, i), view, style);
            }
        } else {
            for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                const InventoryUiEntryView view = entryViewForScreenSlot(i);
                const bool unavailable = view.item != nullptr && !processingTargetHasAvailableCommand(processingTargetForScreenSlot(i));
                InventoryUiSlotStyle style{i == baseProcessingSelection_, unavailable, 48.0f};
                if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                    style.showTopRightCount = true;
                    style.topRightCount = view.stackCount;
                }
                drawInventoryUiSlot(renderer, baseProcessingGridSlotRect(i), view, style);
            }
        }

        const UiRect detailPanel = merchantDetailPanelRect();
        const float moneyRight = detailPanel.pos.x;
        drawMoneySummaryText(renderer, {moneyRight, detailPanel.pos.y + 12.0f}, money_);

        int selected = std::clamp(baseProcessingSelection_, 0, inventory_.screenSlotCount() - 1);
        if (warehouseSource) {
            selected = std::clamp(baseProcessingSelection_, 0, StoragePaneSlotCount - 1);
        } else if (ringSource) {
            const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseProcessingSource_), 0, SpellRingCount - 1);
            const int itemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
            selected = itemCount <= 0 ? 0 : std::clamp(baseProcessingSelection_, 0, itemCount - 1);
        }
        const InventoryUiEntryView detailEntry = entryViewForScreenSlot(selected);
        const ProcessingTarget selectedTarget = processingTargetForScreenSlot(selected);

        const bool selectedRingTarget = selectedTarget.valid && baseItemSourceIsRing(static_cast<int>(selectedTarget.source));
        const SpellRingItem* selectedRingItem = nullptr;
        if (selectedRingTarget &&
            selectedTarget.ringIndex >= 0 &&
            selectedTarget.ringIndex < SpellRingCount) {
            const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(selectedTarget.ringIndex);
            if (selectedTarget.ringItemIndex >= 0 && selectedTarget.ringItemIndex < static_cast<int>(ringItems.size())) {
                selectedRingItem = &ringItems[static_cast<std::size_t>(selectedTarget.ringItemIndex)];
            }
        }

        if (!selectedTarget.valid || (detailEntry.item == nullptr && selectedRingItem == nullptr)) {
            drawUiSubPanel(renderer, detailPanel);
            float detailLineY = drawUiDetailHeader(renderer, detailPanel, "アイテム未選択");
            drawUiDetailText(renderer, detailPanel, detailLineY, "加工するアイテムを選択してください。");
        } else {
            std::optional<InventoryUiItemStats> stats = inventoryUiEntryStats(detailEntry);
            if (!stats && selectedRingItem != nullptr) {
                stats = inventoryUiStatsFromRingItem(*selectedRingItem);
            }
            const int attackBonus = stats ? stats->attackBonus : 0;
            const int digBonus = stats ? stats->digBonus : 0;
            const int durabilityBonus = stats ? stats->durabilityBonus : 0;
            const double weightModifier = stats ? stats->weightModifier : 1.0;
            const double sizeModifier = stats ? stats->sizeModifier : 1.0;

            std::vector<InventoryUiDetailExtraLine> extraLines;
            std::snprintf(buffer, sizeof(buffer), "攻撃+%d / 掘削+%d / 耐久+%d", attackBonus, digBonus, durabilityBonus);
            extraLines.push_back({"強化", buffer, ui::Text});

            std::string processingText;
            if (std::abs(weightModifier - 1.0) > 0.001) {
                std::snprintf(buffer, sizeof(buffer), "重量%.0f%%", weightModifier * 100.0);
                processingText = buffer;
            }
            if (std::abs(sizeModifier - 1.0) > 0.001) {
                std::snprintf(buffer, sizeof(buffer), "大きさ%.0f%%", sizeModifier * 100.0);
                if (!processingText.empty()) {
                    processingText += " / ";
                }
                processingText += buffer;
            }
            if (!processingText.empty()) {
                extraLines.push_back({"加工", processingText, ui::Text});
            }

            if (detailEntry.item != nullptr) {
                drawInventoryUiDetailPanel(
                    renderer,
                    detailPanel,
                    detailEntry,
                    objectCatalog_,
                    encyclopedia_,
                    InventoryUiDetailOptions{.showEnhanceCount = false, .animationSeconds = ringPreviewSeconds},
                    extraLines);
            } else {
                drawUiSubPanel(renderer, detailPanel);
                float detailLineY = drawUiDetailHeader(renderer, detailPanel, ringItemDisplayName(objectCatalog_, *selectedRingItem));
                drawUiDetailText(renderer, detailPanel, detailLineY, "-");
            }
        }
        const ProcessingTarget commandTarget = baseProcessingCommandSlot_ >= 0
            ? processingTargetForScreenSlot(baseProcessingCommandSlot_)
            : ProcessingTarget{};
        const std::vector<UiCommandMenuItem> processingCommandItems = this->processingCommandItems(commandTarget);
        drawUiCommandMenu(
            renderer,
            baseProcessingCommandMenu_,
            processingCommandItems.data(),
            static_cast<int>(processingCommandItems.size()));
    } else if (baseSellActive_) {
        if (baseMerchantMode_ == MerchantUiMode::ChooseAction) {
            renderer.drawText(smallActionInfoTextPos(panel), "何を見ていくんだい？", {198, 198, 206, 255}, 2);
            constexpr std::array<std::string_view, 2> Choices{"買う", "売る"};
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                drawUiButton(renderer, merchantActionChoiceRect(i), Choices[static_cast<std::size_t>(i)], i == baseMerchantActionSelection_, uiActionButtonStyle());
            }
        } else {
            const bool buyMode = baseMerchantMode_ == MerchantUiMode::Buy;
            const auto entryViewForScreenSlot = [this](int slot) {
                InventoryUiEntryView view{};
                if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(slot)) {
                    view.item = &stack->item;
                    view.stackCount = stack->count;
                    return view;
                }
                if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(slot)) {
                    view.item = &instance->item;
                    view.instance = &instance->instance;
                    view.stackCount = 1;
                    view.equipped = inventory_.isStaffEquipped(instance->instance.instanceId);
                }
                return view;
            };
            const auto entryViewForSellTarget = [this, &entryViewForScreenSlot](MerchantSellTarget target) {
                if (!target.valid) {
                    return InventoryUiEntryView{};
                }
                if (target.source == BaseItemSource::Backpack) {
                    return entryViewForScreenSlot(target.slotIndex);
                }
                if (target.source == BaseItemSource::Warehouse) {
                    return storageEntryView(target.storageEntry, true);
                }

                InventoryUiEntryView view{};
                if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
                    return view;
                }
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
                if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
                    return view;
                }
                const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
                view.item = objectForRingItem(objectCatalog_, ringItem);
                view.stats = inventoryUiStatsFromRingItem(ringItem);
                view.stackCount = 1;
                return view;
            };
            const auto blockedSellLabel = [this](MerchantSellTarget target) -> std::string_view {
                if (!target.valid) {
                    return {};
                }
                if (target.source == BaseItemSource::Backpack) {
                    if (const InventoryObjectInstance* instance = inventory_.screenObjectInstanceAt(target.slotIndex)) {
                        if (inventory_.isStaffEquipped(instance->instance.instanceId)) {
                            return "装備中";
                        }
                        if (instance->instance.protectionEnabled) {
                            return "保護中";
                        }
                        if (!isSellableObject(instance->item)) {
                            return "売却不可";
                        }
                    }
                    if (const InventoryObjectStack* stack = inventory_.screenObjectStackAt(target.slotIndex)) {
                        if (!isSellableObject(stack->item)) {
                            return "売却不可";
                        }
                    }
                    return {};
                }

                if (target.source == BaseItemSource::Warehouse) {
                    const ItemData* item = storageEntryItem(target.storageEntry, true);
                    if (item == nullptr || !isSellableObject(*item)) {
                        return "売却不可";
                    }
                    if (const ItemInstance* instance = storageEntryInstance(target.storageEntry, true)) {
                        if (instance->protectionEnabled) {
                            return "保護中";
                        }
                    }
                    return {};
                }

                if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
                    return "売却不可";
                }
                const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
                if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
                    return "売却不可";
                }
                const SpellRingItem& ringItem = ringItems[static_cast<std::size_t>(target.ringItemIndex)];
                if (ringItem.protectionEnabled) {
                    return "保護中";
                }
                const ItemData* item = objectForRingItem(objectCatalog_, ringItem);
                return item != nullptr && isSellableObject(*item) ? std::string_view{} : std::string_view{"売却不可"};
            };

            const UiRect detailPanel = merchantDetailPanelRect();
            drawMoneySummaryText(renderer, {detailPanel.pos.x, detailPanel.pos.y + 12.0f}, money_);

            InventoryUiEntryView detailEntry{};
            std::vector<InventoryUiDetailExtraLine> extraLines;
            const SpellRingItem* selectedRingItem = nullptr;
            if (buyMode) {
                if (merchantStock_.empty()) {
                    renderer.drawText({92.0f, 210.0f}, "商品がありません", {198, 198, 206, 255}, 2);
                }
                for (int i = 0; i < static_cast<int>(merchantStock_.size()); ++i) {
                    const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(i)];
                    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
                    InventoryUiEntryView entry{};
                    entry.item = item;
                    entry.stackCount = 1;
                    const bool disabled = !canBuyMerchantProduct(product);
                    std::snprintf(buffer, sizeof(buffer), "%dG", product.price);
                    InventoryUiSlotStyle style{i == baseMerchantBuySelection_, disabled, 48.0f};
                    style.bottomLabel = buffer;
                    style.bottomLabelColor = disabled ? Color{238, 82, 82, 255} : ui::Text;
                    style.showTopRightCount = true;
                    style.topRightCount = product.quantity;
                    drawInventoryUiSlot(renderer, merchantGridSlotRect(i), entry, style);
                }
                if (!merchantStock_.empty()) {
                    const int selected = std::clamp(baseMerchantBuySelection_, 0, static_cast<int>(merchantStock_.size()) - 1);
                    const MerchantProduct& product = merchantStock_[static_cast<std::size_t>(selected)];
                    const ItemData* item = objectCatalog_.registry.findById(product.objectId);
                    detailEntry.item = item;
                    detailEntry.stackCount = 1;
                    std::snprintf(buffer, sizeof(buffer), "%dG", product.price);
                    extraLines.push_back({"価格", buffer, canBuyMerchantProduct(product) ? ui::Text : Color{238, 82, 82, 255}});
                    std::snprintf(buffer, sizeof(buffer), "%d", product.quantity);
                    extraLines.push_back({"在庫", buffer, product.quantity > 0 ? ui::Text : ui::TextDisabled});
                }
            } else {
                const int sourceCount = baseItemSourceCountForUnlockedRings(unlockedRingCount());
                std::array<UiTabItem, BaseItemSourceCount> sourceTabs{};
                std::array<UiRect, BaseItemSourceCount> sourceTabRects{};
                for (int i = 0; i < sourceCount; ++i) {
                    sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(i)], true};
                    sourceTabRects[static_cast<std::size_t>(i)] = merchantSellSourceRect(i, sourceCount);
                }
                drawUiTabs(
                    renderer,
                    baseMerchantSellSourceTabs_,
                    baseMerchantSellSource_,
                    sourceTabs.data(),
                    sourceCount,
                    sourceTabRects.data());

                const bool warehouseSource = baseItemSourceIsWarehouse(baseMerchantSellSource_);
                const bool ringSource = baseItemSourceIsRing(baseMerchantSellSource_);
                const auto sellTargetBottomLabel = [this, &blockedSellLabel](
                    MerchantSellTarget target,
                    std::string& outLabel,
                    Color& outColor) {
                    outLabel.clear();
                    if (!target.valid) {
                        return false;
                    }
                    const std::string_view blockedLabel = blockedSellLabel(target);
                    if (!blockedLabel.empty()) {
                        outLabel = std::string(blockedLabel);
                        outColor = ui::TextDisabled;
                        return true;
                    }

                    char priceBuffer[32];
                    std::snprintf(priceBuffer, sizeof(priceBuffer), "%dG", merchantSellTargetPrice(target));
                    outLabel = priceBuffer;
                    outColor = ui::Text;
                    return true;
                };
                const auto applySellTargetBottomLabel = [&sellTargetBottomLabel](
                    InventoryUiSlotStyle& style,
                    MerchantSellTarget target) {
                    std::string label;
                    Color labelColor = ui::Text;
                    if (sellTargetBottomLabel(target, label, labelColor)) {
                        style.bottomLabel = label;
                        style.bottomLabelColor = labelColor;
                    }
                };
                const auto targetHighValue = [this, &entryViewForSellTarget](MerchantSellTarget target) {
                    const InventoryUiEntryView view = entryViewForSellTarget(target);
                    return view.item != nullptr && isHighValueBuyObject(*view.item);
                };
                const auto drawHighValueLabel = [&renderer](UiRect rect) {
                    const std::string label = "高価買取中!";
                    const Vec2 size = renderer.measureText(label, 1, TextStyle::Italic);
                    const Vec2 pos{
                        rect.pos.x + (rect.size.x - size.x) * 0.5f,
                        rect.pos.y + rect.size.y - 40.0f,
                    };
                    renderer.drawOutlinedText(pos, label, Color{255, 64, 64, 255}, Color{44, 0, 0, 210}, 2, 1, TextStyle::Italic);
                };
                if (ringSource) {
                    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseMerchantSellSource_), 0, SpellRingCount - 1);
                    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(ringIndex);
                    const int selectedRingIndex = ringItems.empty()
                        ? -1
                        : std::clamp(baseSellSelection_, 0, static_cast<int>(ringItems.size()) - 1);
                    drawMerchantSellRingPreview(
                        renderer,
                        spellRing_,
                        objectCatalog_,
                        balance_,
                        ringIndex,
                        selectedRingIndex,
                        ringPreviewSeconds);
                    for (int i = 0; i < static_cast<int>(ringItems.size()); ++i) {
                        const MerchantSellTarget target = merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                        std::string label;
                        Color labelColor = ui::Text;
                        if (!sellTargetBottomLabel(target, label, labelColor)) {
                            continue;
                        }
                        UiRect labelRect = merchantSellRingItemRect(
                            ringItems[static_cast<std::size_t>(i)],
                            spellRing_,
                            balance_,
                            ringIndex,
                            i,
                            static_cast<int>(ringItems.size()),
                            ringPreviewSeconds);
                        labelRect.size.y += MerchantSellRingItemLabelExtraHeight;
                        drawInventoryUiSlotBottomLabel(renderer, labelRect, label, labelColor);
                        if (targetHighValue(target)) {
                            drawHighValueLabel(labelRect);
                        }
                    }
                    if (ringItems.empty()) {
                        const Vec2 emptySize = renderer.measureText("アイテム未配置", 2);
                        renderer.drawText(
                            merchantSellRingPreviewCenter(spellRing_, ringIndex) - emptySize * 0.5f,
                            "アイテム未配置",
                            ui::TextMuted,
                            2);
                    } else if (selectedRingIndex >= 0) {
                        selectedRingItem = &ringItems[static_cast<std::size_t>(selectedRingIndex)];
                    }
                } else if (warehouseSource) {
                    const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
                    const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                    drawExternalWarehouseSourceHeader(
                        renderer,
                        merchantSellGridSlotRect,
                        warehousePage,
                        warehousePageCount);
                    for (int i = 0; i < StoragePaneSlotCount; ++i) {
                        const MerchantSellTarget target = merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                        const InventoryUiEntryView view = entryViewForSellTarget(target);
                        const std::string_view blockedLabel = blockedSellLabel(target);
                        const bool disabled = view.item != nullptr && !blockedLabel.empty();
                        InventoryUiSlotStyle style{i == baseSellSelection_, disabled, 48.0f};
                        if (view.item != nullptr) {
                            applySellTargetBottomLabel(style, target);
                        }
                        if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                            style.showTopRightCount = true;
                            style.topRightCount = view.stackCount;
                        }
                        drawInventoryUiSlot(renderer, externalWarehouseSourceSlotRect(merchantSellGridSlotRect, i), view, style);
                        if (targetHighValue(target)) {
                            drawHighValueLabel(externalWarehouseSourceSlotRect(merchantSellGridSlotRect, i));
                        }
                    }
                } else {
                    for (int i = 0; i < inventory_.screenSlotCount(); ++i) {
                        const MerchantSellTarget target = merchantSellTargetForSourceSlot(baseMerchantSellSource_, i);
                        const InventoryUiEntryView view = entryViewForSellTarget(target);
                        const std::string_view blockedLabel = blockedSellLabel(target);
                        const bool disabled = view.item != nullptr && !blockedLabel.empty();
                        const UiRect rect = merchantSellGridSlotRect(i);
                        InventoryUiSlotStyle style{i == baseSellSelection_, disabled, 48.0f};
                        if (view.item != nullptr) {
                            applySellTargetBottomLabel(style, target);
                        }
                        if (view.item != nullptr && view.instance == nullptr && view.stackCount > 1) {
                            style.showTopRightCount = true;
                            style.topRightCount = view.stackCount;
                        }
                        drawInventoryUiSlot(renderer, rect, view, style);
                        if (targetHighValue(target)) {
                            drawHighValueLabel(rect);
                        }
                    }
                }

                int selected = std::clamp(baseSellSelection_, 0, inventory_.screenSlotCount() - 1);
                if (warehouseSource) {
                    selected = std::clamp(baseSellSelection_, 0, StoragePaneSlotCount - 1);
                } else if (ringSource) {
                    const int ringIndex = std::clamp(ringIndexFromBaseItemSource(baseMerchantSellSource_), 0, SpellRingCount - 1);
                    const int itemCount = static_cast<int>(spellRing_.itemsForRing(ringIndex).size());
                    selected = itemCount <= 0 ? 0 : std::clamp(baseSellSelection_, 0, itemCount - 1);
                }
                const MerchantSellTarget selectedTarget = merchantSellTargetForScreenSlot(selected);
                detailEntry = entryViewForSellTarget(selectedTarget);
                if (selectedTarget.valid) {
                    const std::string_view blockedLabel = blockedSellLabel(selectedTarget);
                    if (!blockedLabel.empty()) {
                        extraLines.push_back({"売値", std::string(blockedLabel), ui::TextDisabled});
                    } else {
                        if (detailEntry.item != nullptr && isHighValueBuyObject(*detailEntry.item)) {
                            extraLines.push_back({"", "高価買取中!", Color{255, 64, 64, 255}});
                        }
                        std::snprintf(buffer, sizeof(buffer), "%dG", merchantSellTargetPrice(selectedTarget));
                        extraLines.push_back({"売値", buffer, ui::Text});
                    }
                }
            }

            if (!buyMode && detailEntry.item == nullptr && selectedRingItem != nullptr) {
                drawUiSubPanel(renderer, detailPanel);
                float detailLineY = drawUiDetailHeader(renderer, detailPanel, ringItemDisplayName(objectCatalog_, *selectedRingItem));
                drawUiDetailText(renderer, detailPanel, detailLineY, "売却できません。");
                drawUiDetailLine(renderer, detailPanel, detailLineY, "売値", "売却不可");
            } else {
                drawInventoryUiDetailPanel(
                    renderer,
                    detailPanel,
                    detailEntry,
                    objectCatalog_,
                    encyclopedia_,
                    InventoryUiDetailOptions{.animationSeconds = ringPreviewSeconds},
                    extraLines);
            }
            if (buyMode) {
                const bool buyCommandEnabled = baseMerchantBuyCommandIndex_ >= 0 &&
                    baseMerchantBuyCommandIndex_ < static_cast<int>(merchantStock_.size()) &&
                    canBuyMerchantProduct(merchantStock_[static_cast<std::size_t>(baseMerchantBuyCommandIndex_)]);
                const std::array<UiCommandMenuItem, 1> buyItems{{{"買う", buyCommandEnabled}}};
                drawUiCommandMenu(renderer, baseMerchantBuyCommandMenu_, buyItems.data(), static_cast<int>(buyItems.size()));
            } else {
                const MerchantSellTarget commandTarget = merchantSellTargetForSourceSlot(
                    baseMerchantSellCommandSource_,
                    baseMerchantSellCommandIndex_);
                const bool stackCommand =
                    (commandTarget.source == BaseItemSource::Backpack &&
                        baseMerchantSellCommandIndex_ >= 0 &&
                        inventory_.screenObjectStackAt(baseMerchantSellCommandIndex_) != nullptr) ||
                    (commandTarget.source == BaseItemSource::Warehouse &&
                        commandTarget.storageEntry.kind == StorageEntryKind::Stack);
                const std::array<UiCommandMenuItem, 2> sellItems{{{stackCommand ? "1個売る" : "売る", true}, {"すべて売る", stackCommand}}};
                drawUiCommandMenu(renderer, baseMerchantSellCommandMenu_, sellItems.data(), stackCommand ? 2 : 1);
            }
        }
    } else if (baseUpgradeActive_) {
        const int selected = std::clamp(baseUpgradeSelection_, 0, BaseUpgradeItemCount - 1);
        const auto shortName = [](int index) -> const char* {
            switch (index) {
            case 0: return "倉庫容量";
            case 1: return "商人機能";
            case 2: return "作業台機能";
            case 3: return "リング工房";
            case 4: return "最大HP";
            case 5: return "リング半径";
            case 6: return "リング速度";
            case 7: return "収集術式";
            default: return "";
            }
        };
        const auto warehouseCapacityForUiLevel = [](int level) {
            constexpr std::array<int, 5> Capacities{{48, 72, 100, 140, 200}};
            const int index = std::clamp(level - 1, 0, static_cast<int>(Capacities.size()) - 1);
            return Capacities[static_cast<std::size_t>(index)];
        };
        const auto merchantFeature = [](int level) -> const char* {
            switch (level) {
            case 1: return "通常売買";
            case 2: return "品揃え5枠";
            case 3: return "品揃え6枠/買取+10%";
            case 4: return "宝の高価買取";
            case 5: return "品揃え8枠/レア増加";
            case 6: return "品揃え9枠/買取+20%";
            case 7: return "品揃え10枠/高レア増加";
            default: return "未解禁";
            }
        };
        const auto processingFeature = [](int level) -> const char* {
            switch (level) {
            case 1: return "軽量化";
            case 2: return "作業台費用-10%";
            case 3: return "大型化";
            case 4: return "作業台費用-20%";
            case 5: return "作業台費用-30%";
            default: return "未解禁";
            }
        };
        const float listLabelX = panel.pos.x + 40.0f;
        renderer.drawText({listLabelX, 148.0f}, "拠点機能", {198, 198, 206, 255}, 2);
        renderer.drawText({listLabelX, 270.0f}, "施設解禁", {198, 198, 206, 255}, 2);
        renderer.drawText({listLabelX, 392.0f}, "プレイ性能", {198, 198, 206, 255}, 2);
        std::array<UiVerticalTabItem, BaseUpgradeItemCount> upgradeTabs{};
        std::array<UiRect, BaseUpgradeItemCount> upgradeTabRects{};
        std::array<std::string, BaseUpgradeItemCount> upgradeTabValues{};
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            const bool implemented = upgradeImplemented(i);
            const bool maxed = implemented && upgradeMaxed(i);
            if (!implemented) {
                upgradeTabValues[static_cast<std::size_t>(i)] = "未実装";
            } else if (maxed) {
                upgradeTabValues[static_cast<std::size_t>(i)] = "上限";
            } else {
                std::snprintf(buffer, sizeof(buffer), "Lv.%d/%d", upgradeLevel(i), upgradeMaxLevel(i));
                upgradeTabValues[static_cast<std::size_t>(i)] = buffer;
            }
            upgradeTabs[static_cast<std::size_t>(i)] = {
                shortName(i),
                upgradeTabValues[static_cast<std::size_t>(i)],
                implemented,
                maxed ? Color{160, 220, 190, 255} : ui::TextMuted,
            };
            upgradeTabRects[static_cast<std::size_t>(i)] = baseUpgradeItemRect(i);
        }
        drawUiVerticalTabs(
            renderer,
            baseUpgradeTabs_,
            selected,
            upgradeTabs.data(),
            static_cast<int>(upgradeTabs.size()),
            upgradeTabRects.data());

        const UiRect detailPanel = baseUpgradeDetailPanelRect();
        drawUiSubPanel(renderer, detailPanel);

        const auto drawTextRun = [&renderer](Vec2& pos, std::string_view text, Color color, int scale) {
            renderer.drawText(pos, text, color, scale);
            pos.x += renderer.measureText(text, scale).x;
        };
        InlineItemTextStyle inlineStyle;
        inlineStyle.scale = 2;
        inlineStyle.iconTextGap = 4.0f;
        inlineStyle.iconScale = 1.15f;
        const auto drawInlineTextRun = [&](Vec2& pos, std::string_view text, Color color) {
            inlineStyle.text = color;
            drawInlineItemText(renderer, objectCatalog_, pos, text, inlineStyle);
            pos.x += measureInlineItemText(renderer, text, inlineStyle).x;
        };
        const auto beginDetailRow = [&renderer, detailPanel](float& y, std::string_view label) {
            constexpr float LabelWidth = 96.0f;
            const UiRect content = uiSubPanelContentRect(detailPanel);
            renderer.drawText({content.pos.x, y}, label, ui::TextMuted, 2);
            return Vec2{content.pos.x + LabelWidth, y};
        };
        const auto drawDetailTextRow = [&](float& y, std::string_view label, std::string_view text, Color color) {
            constexpr float LabelWidth = 96.0f;
            constexpr float MinLineHeight = 31.0f;
            constexpr float LineGap = 4.0f;
            const UiRect content = uiSubPanelContentRect(detailPanel);
            const Vec2 pos = beginDetailRow(y, label);
            const float valueMaxWidth = std::max(0.0f, content.size.x - LabelWidth);
            renderer.drawWrappedText(pos, text, valueMaxWidth, color, 2);
            y += std::max(MinLineHeight, renderer.measureWrappedText(text, valueMaxWidth, 2).y + LineGap);
        };
        const auto drawMoneyCostLine = [&](float& y, std::string_view label, int cost) {
            Vec2 pos = beginDetailRow(y, label);
            const Color numberColor = money_ >= cost ? ui::Text : Color{238, 82, 82, 255};
            drawInlineTextRun(pos, inlineWorldIconTag(worldIconKey(WorldIconId::MoneyLarge)) + " ", ui::Text);
            drawTextRun(pos, std::to_string(cost), numberColor, 2);
            drawTextRun(pos, "G", ui::Text, 2);
            y += 31.0f;
        };
        const auto drawMaterialCostLine = [&](float& y, std::string_view label, MaterialType type, int cost) {
            const int owned = inventory_.materialCount(type);
            const bool enough = owned >= cost;
            const Color numberColor = enough ? ui::Text : Color{238, 82, 82, 255};
            Vec2 pos = beginDetailRow(y, label);
            drawInlineTextRun(pos, inlineMaterialIconTag(type) + std::string(materialTypeDisplayName(type)) + " ×", ui::Text);
            drawTextRun(pos, std::to_string(cost), numberColor, 2);
            drawTextRun(pos, " (", ui::Text, 2);
            drawTextRun(pos, std::to_string(owned), numberColor, 2);
            drawTextRun(pos, ")", ui::Text, 2);
            y += 31.0f;
        };
        const auto drawEffectChangeLine = [&](float& y, std::string_view label, std::string_view prefix, std::string_view current, std::string_view next) {
            constexpr Color UpgradeValueColor{255, 230, 150, 255};
            Vec2 pos = beginDetailRow(y, label);
            drawTextRun(pos, prefix, ui::Text, 2);
            drawTextRun(pos, current, ui::Text, 2);
            drawTextRun(pos, " → ", ui::TextMuted, 2);
            drawTextRun(pos, next, UpgradeValueColor, 2);
            y += 31.0f;
        };

        float detailY = drawUiDetailHeader(renderer, detailPanel, upgradeName(selected));

        const bool implemented = upgradeImplemented(selected);
        const bool maxed = implemented && upgradeMaxed(selected);
        const MaterialType materialType = upgradeMaterialType(selected);
        const int materialCost = upgradeMaterialCost(selected);
        const int moneyCost = upgradeCost(selected);

        if (implemented && !maxed) {
            drawMoneyCostLine(detailY, "必要素材", moneyCost);
            if (materialCost > 0) {
                drawMaterialCostLine(detailY, "", materialType, materialCost);
            }
        } else {
            drawDetailTextRow(detailY, "必要素材", "なし", ui::Text);
        }

        char currentValue[64];
        char nextValue[64];
        const int level = upgradeLevel(selected);
        const int maxLevel = upgradeMaxLevel(selected);
        const int nextLevel = std::min(maxLevel, level + 1);
        if (maxed) {
            drawDetailTextRow(detailY, "効果", "上限到達済み", ui::TextMuted);
        } else {
            switch (selected) {
            case 0:
                std::snprintf(currentValue, sizeof(currentValue), "%d枠", warehouseCapacityForUiLevel(level));
                std::snprintf(nextValue, sizeof(nextValue), "%d枠", warehouseCapacityForUiLevel(nextLevel));
                drawEffectChangeLine(detailY, "効果", "倉庫容量: ", currentValue, nextValue);
                break;
            case 1:
                drawEffectChangeLine(detailY, "効果", "商人機能: ", merchantFeature(level), merchantFeature(nextLevel));
                break;
            case 2:
                drawEffectChangeLine(detailY, "効果", "加工解禁: ", processingFeature(level), processingFeature(nextLevel));
                break;
            case 3:
                drawDetailTextRow(detailY, "効果", "リング工房を解禁", Color{255, 230, 150, 255});
                break;
            case 4:
                std::snprintf(currentValue, sizeof(currentValue), "+%d", level * 2);
                std::snprintf(nextValue, sizeof(nextValue), "+%d", nextLevel * 2);
                drawEffectChangeLine(detailY, "効果", "最大HP: ", currentValue, nextValue);
                break;
            case 5:
                std::snprintf(currentValue, sizeof(currentValue), "+%d%%", level * 8);
                std::snprintf(nextValue, sizeof(nextValue), "+%d%%", nextLevel * 8);
                drawEffectChangeLine(detailY, "効果", "初期リング半径: ", currentValue, nextValue);
                break;
            case 6:
                std::snprintf(currentValue, sizeof(currentValue), "+%d%%", level * 8);
                std::snprintf(nextValue, sizeof(nextValue), "+%d%%", nextLevel * 8);
                drawEffectChangeLine(detailY, "効果", "初期リング速度: ", currentValue, nextValue);
                break;
            case 7:
                std::snprintf(currentValue, sizeof(currentValue), "%.0fpx", effectiveCollectionPullRadius(level));
                std::snprintf(nextValue, sizeof(nextValue), "%.0fpx", effectiveCollectionPullRadius(nextLevel));
                drawEffectChangeLine(detailY, "効果", "吸引半径: ", currentValue, nextValue);
                drawDetailTextRow(detailY, "", "近くのドロップをプレイヤーへ吸い寄せます。", ui::TextMuted);
                break;
            default:
                break;
            }
        }

        UiButtonStyle confirmStyle = uiActionButtonStyle();
        const char* confirmLabel = "強化する";
        if (!implemented) {
            confirmLabel = "未実装";
            confirmStyle.fill = {20, 24, 38, 190};
            confirmStyle.fillHot = confirmStyle.fill;
            confirmStyle.text = ui::TextDisabled;
        } else if (maxed) {
            confirmLabel = "上限";
            confirmStyle.fill = {26, 42, 62, 204};
            confirmStyle.fillHot = confirmStyle.fill;
            confirmStyle.text = ui::TextMuted;
        }
        drawUiButton(renderer, baseUpgradeConfirmRect(), confirmLabel, false, confirmStyle);
    } else if (baseMiningStartChoiceActive_) {
        const UiRect body = uiBodyRect(panel);
        const float contentLeft = baseMiningContentLeft();
        const std::string stageName = currentStageDefinition().name.empty()
            ? currentStageId_
            : currentStageDefinition().name;
        const auto retainedStage = dungeonStates_.find(currentStageId_);
        const bool hasRetainedDungeon = retainedStage != dungeonStates_.end() && retainedStage->second.valid;
        int totalWarpPoints = MaxWarpPointsPerRun;
        if (hasRetainedDungeon && !retainedStage->second.warpPoints.empty()) {
            totalWarpPoints = static_cast<int>(retainedStage->second.warpPoints.size());
        } else if (!warpPoints_.empty()) {
            totalWarpPoints = static_cast<int>(warpPoints_.size());
        }
        totalWarpPoints = std::max(1, totalWarpPoints);
        const int discoveredWarpPoints = std::clamp(unlockedWarpPointCount_, 0, totalWarpPoints);

        const auto drawCenteredTextInRect = [&](UiRect rect, std::string_view text, Color color, int scale) {
            const Vec2 textSize = renderer.measureText(text, scale);
            renderer.drawText(
                rect.pos + Vec2{
                    std::max(0.0f, (rect.size.x - textSize.x) * 0.5f),
                    std::max(0.0f, (rect.size.y - textSize.y) * 0.5f),
                },
                text,
                color,
                scale);
        };

        const std::vector<StageDefinition> selectableStages = selectableStageDefinitionsForCurrentUnlockState();
        const int selectableStageCount = static_cast<int>(selectableStages.size());
        const bool canSelectDestination = selectableStageCount > 1;
        const UiPageSelectorRects stageSelector = baseMiningStageSelectorRects();
        const std::vector<WarpPoint> selectableWarpPoints = selectableWarpPointsForCurrentStageStart();

        renderer.drawText({contentLeft, body.pos.y}, "行き先", {198, 198, 206, 255}, 2);
        drawUiRectButton(renderer, stageSelector.prev, "<", false);
        drawUiRectButton(renderer, stageSelector.next, ">", false);
        if (!canSelectDestination) {
            renderer.fillRect(stageSelector.prev.pos, stageSelector.prev.size, {0, 0, 0, 118});
            renderer.fillRect(stageSelector.next.pos, stageSelector.next.size, {0, 0, 0, 118});
        }
        const int stageNameScale = renderer.measureText(stageName, 3).x <= stageSelector.text.size.x ? 3 : 2;
        drawCenteredTextInRect(stageSelector.text, stageName, {246, 235, 255, 255}, stageNameScale);

        std::snprintf(buffer, sizeof(buffer), "発見済みワープポイント  %d/%d", discoveredWarpPoints, totalWarpPoints);
        InlineItemTextStyle warpProgressStyle;
        warpProgressStyle.text = {198, 198, 206, 255};
        warpProgressStyle.scale = 2;
        warpProgressStyle.iconTextGap = 8.0f;
        warpProgressStyle.iconScale = 28.0f / std::max(1.0f, renderer.measureText("0", warpProgressStyle.scale).y);
        const std::string warpProgressText = inlineWorldIconTag(worldIconKey(WorldIconId::WarpPoint)) + std::string(buffer);
        const Vec2 warpProgressSize = measureInlineItemText(renderer, warpProgressText, warpProgressStyle);
        drawInlineItemText(
            renderer,
            objectCatalog_,
            {
                panel.pos.x + std::max(0.0f, (panel.size.x - warpProgressSize.x) * 0.5f),
                body.pos.y + 44.0f,
            },
            warpProgressText,
            warpProgressStyle);

        renderer.drawText({contentLeft, body.pos.y + 78.0f}, "出発地点を選んでください", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BaseMiningStartChoiceCount; ++i) {
            const bool disabled = (i == 1 && selectableWarpPoints.empty()) || (i == 2 && !canRegenerateCurrentStage());
            const char* description = "";
            switch (i) {
            case 0:
                description = "入口から出発";
                break;
            case 1:
                description = disabled ? "ワープポイントを解放すると可能" : "解放済み地点から選んで出発";
                break;
            case 2:
                description = disabled ? "全ワープ解放とクリア後に可能" : "地形・敵・宝箱・ワープ配置を作り直す";
                break;
            default:
                break;
            }

            const UiRect rect = baseMiningStartChoiceRect(i);
            UiButtonStyle buttonStyle = uiActionButtonStyle();
            if (disabled) {
                buttonStyle.text = ui::TextDisabled;
                buttonStyle.imageTint = {128, 128, 140, 210};
                buttonStyle.imageTintHot = buttonStyle.imageTint;
                buttonStyle.fill = {18, 22, 34, 190};
                buttonStyle.fillHot = buttonStyle.fill;
                buttonStyle.outline = {98, 88, 112, 190};
                buttonStyle.outlineHot = buttonStyle.outline;
            }
            const bool hot = i == baseMiningStartSelection_ && !disabled && !baseWarpPointSelectActive_;
            if (i == 1) {
                drawUiButton(renderer, rect, "", hot, buttonStyle);
                InlineItemTextStyle buttonTextStyle;
                buttonTextStyle.text = buttonStyle.text;
                buttonTextStyle.scale = 2;
                buttonTextStyle.iconTextGap = 6.0f;
                buttonTextStyle.iconScale = 26.0f / std::max(1.0f, renderer.measureText("0", buttonTextStyle.scale).y);
                const std::string buttonText = inlineWorldIconTag(worldIconKey(WorldIconId::WarpPoint)) + std::string(baseMiningStartChoiceName(i));
                const Vec2 buttonTextSize = measureInlineItemText(renderer, buttonText, buttonTextStyle);
                drawInlineItemText(
                    renderer,
                    objectCatalog_,
                    rect.pos + Vec2{
                        std::max(0.0f, (rect.size.x - buttonTextSize.x) * 0.5f),
                        std::max(0.0f, (rect.size.y - buttonTextSize.y) * 0.5f),
                    },
                    buttonText,
                    buttonTextStyle);
            } else {
                drawUiButton(renderer, rect, baseMiningStartChoiceName(i), hot, buttonStyle);
            }
            const Vec2 descriptionSize = renderer.measureText(description, 2);
            renderer.drawText(
                rect.pos + Vec2{
                    std::max(0.0f, (rect.size.x - descriptionSize.x) * 0.5f),
                    ui::ButtonHeight + 4.0f,
                },
                description,
                disabled ? Color{150, 150, 160, 255} : Color{198, 198, 206, 255},
                2);
        }

        if (baseWarpPointSelectActive_) {
            panelCancelScope.reset();
            panelWindow.reset();

            renderer.fillRect(
                {0.0f, 0.0f},
                {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
                {0, 0, 0, 82});

            const UiRect warpPanel = baseMiningWarpPointSelectRect();
            UiWindowScope warpWindow(
                renderer,
                "base.warp_select",
                warpPanel,
                "ワープポイント選択",
                "F/Enter 出発  Z/X・方向キー 選択  Esc 戻る",
                UiWindowOptions{true, false});

            renderer.drawText(warpPanel.pos + Vec2{48.0f, 74.0f}, "出発地点を選んでください", {198, 198, 206, 255}, 2);
            if (selectableWarpPoints.empty()) {
                renderer.drawText(warpPanel.pos + Vec2{48.0f, 142.0f}, "解放済みワープポイントがありません", ui::TextDisabled, 2);
            }
            for (int i = 0; i < static_cast<int>(selectableWarpPoints.size()); ++i) {
                const WarpPoint& point = selectableWarpPoints[static_cast<std::size_t>(i)];
                const UiRect rect = baseMiningWarpPointSelectChoiceRect(i);
                const bool hot = i == baseWarpPointSelection_;
                UiButtonStyle buttonStyle = uiActionButtonStyle();
                drawUiButton(renderer, rect, "", hot, buttonStyle);

                InlineItemTextStyle pointTextStyle;
                pointTextStyle.text = buttonStyle.text;
                pointTextStyle.scale = 2;
                pointTextStyle.iconTextGap = 6.0f;
                pointTextStyle.iconScale = 24.0f / std::max(1.0f, renderer.measureText("0", pointTextStyle.scale).y);
                const std::string pointText =
                    inlineWorldIconTag(worldIconKey(WorldIconId::WarpPoint)) +
                    "ワープ " + std::to_string(point.index + 1);
                const Vec2 pointTextSize = measureInlineItemText(renderer, pointText, pointTextStyle);
                drawInlineItemText(
                    renderer,
                    objectCatalog_,
                    {
                        rect.pos.x + 14.0f,
                        rect.pos.y + std::max(0.0f, (rect.size.y - pointTextSize.y) * 0.5f),
                    },
                    pointText,
                    pointTextStyle);

                std::snprintf(buffer, sizeof(buffer), "%d/%d", point.index + 1, totalWarpPoints);
                const Vec2 progressSize = renderer.measureText(buffer, 2);
                renderer.drawText(
                    {
                        rect.pos.x + rect.size.x - progressSize.x - 14.0f,
                        rect.pos.y + std::max(0.0f, (rect.size.y - progressSize.y) * 0.5f),
                    },
                    buffer,
                    hot ? Color{255, 230, 150, 255} : Color{198, 198, 206, 255},
                    2);
            }
        }
    } else {
        const bool modalOpen = baseBrokenRingDepartureConfirm_.open;
        if (!modalOpen && !bottomControlHelpBlocked) {
            drawBaseControlHelp(
                renderer,
                camera_.width(),
                camera_.height(),
                baseExplorationControlHelp(interactionFacility));
        }
        if (!baseStatus_.empty()) {
            UiSystemMessageStyle statusStyle;
            statusStyle.fill = {0, 0, 0, 160};
            statusStyle.padding = {20.0f, 3.0f};
            drawUiSystemMessage(renderer, baseStatus_, {300.0f, 612.0f}, statusStyle);
        }
        if (baseBrokenRingDepartureConfirm_.open) {
            drawUiConfirmDialog(
                renderer,
                baseBrokenRingDepartureConfirm_,
                baseBrokenRingDepartureConfirmRect(),
                "base.broken_ring_departure.confirm");
        }
        return;
    }

    if (!baseStatus_.empty()) {
        drawUiSystemMessage(
            renderer,
            baseStatus_,
            baseSystemMessagePos(
                panel,
                baseStorageActive_,
                baseSellActive_,
                baseProcessingActive_ || (baseRingWorkshopActive_ && baseRingWorkshopMode_ != RingWorkshopMode::ChooseAction),
                baseUpgradeActive_));
    }
    if (baseBrokenRingDepartureConfirm_.open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiConfirmDialog(
            renderer,
            baseBrokenRingDepartureConfirm_,
            baseBrokenRingDepartureConfirmRect(),
            "base.broken_ring_departure.confirm");
    }
    if (baseRegenerateConfirm_.open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});

        drawUiConfirmDialog(
            renderer,
            baseRegenerateConfirm_,
            baseMiningRegenerateConfirmRect(),
            "base.mining.regenerate.confirm");
    }
    if (baseProcessingConfirm_.open) {
        panelCancelScope.reset();
        panelWindow.reset();

        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});

        drawProcessingConfirmDialog(renderer, baseProcessingConfirmRect());
    }
    if (baseResultDialog_.open) {
        panelCancelScope.reset();
        panelWindow.reset();
        drawUiResultDialog(renderer, baseResultDialog_, baseResultDialogRect(), "base.result");
    }
    if (baseStorageQuantityDialog_.open) {
        panelCancelScope.reset();
        panelWindow.reset();
        drawUiModalBackdrop(
            renderer,
            {{0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}},
            {0, 0, 0, 96});
        drawUiQuantityDialog(renderer, baseStorageQuantityDialog_, storageQuantityDialogRect(), "base.storage.quantity");
    }

}

} // namespace majo
