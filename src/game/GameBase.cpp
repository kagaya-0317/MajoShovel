#include "game/GameInternal.hpp"

namespace majo {

namespace {

bool isTutorialStoryTrigger(std::string_view trigger)
{
    return trigger.rfind("tutorial:", 0) == 0;
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

enum RingLevelUpgradePointIndex {
    RingLevelUpgradeRadius = 0,
    RingLevelUpgradeSpeed = 1,
    RingLevelUpgradeWeightLimit = 2,
};

struct RingWorkshopRespecTransfer {
    int from = 0;
    int to = 0;
};

constexpr int RingWorkshopRespecTransferCount = 6;
constexpr int RingWorkshopConfirmItemIndex = RingWorkshopRespecTransferCount;
constexpr int RingWorkshopUpgradeStartIndex = RingWorkshopConfirmItemIndex + 1;
constexpr int BaseBackpackSourceIndex = 0;
constexpr std::array<RingWorkshopRespecTransfer, RingWorkshopRespecTransferCount> RingWorkshopRespecTransfers{{
    {RingLevelUpgradeSpeed, RingLevelUpgradeRadius},
    {RingLevelUpgradeWeightLimit, RingLevelUpgradeRadius},
    {RingLevelUpgradeRadius, RingLevelUpgradeSpeed},
    {RingLevelUpgradeWeightLimit, RingLevelUpgradeSpeed},
    {RingLevelUpgradeRadius, RingLevelUpgradeWeightLimit},
    {RingLevelUpgradeSpeed, RingLevelUpgradeWeightLimit},
}};

const char* ringLevelUpgradePointName(int index)
{
    switch (index) {
    case RingLevelUpgradeRadius: return "半径";
    case RingLevelUpgradeSpeed: return "速度";
    case RingLevelUpgradeWeightLimit: return "重量";
    default: return "強化";
    }
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
constexpr float BaseRingPreviewScale = 0.8f;
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

UiRect merchantSellSourceRect(int index)
{
    return {{BaseItemSourceTabX + static_cast<float>(index) * BaseItemSourceTabPitch, 116.0f + MerchantSellSourceYOffset}, {BaseItemSourceTabWidth, ui::ButtonHeight}};
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
    UiRect rect = merchantSellSourceRect(tabIndex);
    rect.pos.x = storageItemCircleLeftX() + static_cast<float>(tabIndex) * BaseItemSourceTabPitch;
    rect.pos.y += StorageTransferLayoutYOffset;
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

UiRect smallActionDialogRect()
{
    return {{410.0f, 170.0f}, {460.0f, 330.0f}};
}

UiRect smallActionChoiceRect(int index)
{
    constexpr float ChoiceGap = 16.0f;
    const UiRect body = uiBodyRect(smallActionDialogRect());
    const float left = body.pos.x + 8.0f;
    const float right = body.pos.x + body.size.x - 36.0f;
    return {
        {left, body.pos.y + 20.0f + static_cast<float>(index) * (ui::ButtonHeight + ChoiceGap)},
        {right - left, ui::ButtonHeight},
    };
}

Vec2 smallActionInfoTextPos(UiRect panel)
{
    const UiRect body = uiBodyRect(panel);
    return body.pos + Vec2{8.0f, -18.0f};
}

UiRect storageActionDialogRect()
{
    return smallActionDialogRect();
}

UiRect storageActionChoiceRect(int index)
{
    return smallActionChoiceRect(index);
}

UiRect merchantActionDialogRect()
{
    return smallActionDialogRect();
}

UiRect merchantActionChoiceRect(int index)
{
    return smallActionChoiceRect(index);
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
    if (storageActive || merchantActive) {
        return {80.0f, 500.0f};
    }
    if (processingActive) {
        return panel.pos + Vec2{54.0f, 504.0f};
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
    return baseRingPreviewCenterFromGrid(baseProcessingGridSlotRect, 28.0f);
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

ProcessingResultSnapshot processingSnapshotFromStack(const InventoryObjectStack& stack)
{
    ProcessingResultSnapshot snapshot{};
    snapshot.name = nonEmptyItemName(stack.item.name);
    snapshot.stackCount = stack.count;
    snapshot.stackSource = true;
    snapshot.currentDurability = stack.item.durability;
    snapshot.maxDurability = stack.item.durability;
    snapshot.isBroken = stack.item.durability == 0;
    return snapshot;
}

ProcessingResultSnapshot processingSnapshotFromInstance(const InventoryObjectInstance& entry)
{
    ProcessingResultSnapshot snapshot{};
    snapshot.name = nonEmptyItemName(entry.item.name);
    snapshot.stackCount = 1;
    snapshot.currentDurability = entry.instance.currentDurability;
    snapshot.maxDurability = entry.instance.maxDurability;
    snapshot.isBroken = entry.instance.isBroken;
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
    if (item.category == "\xE3\x82\xB9\xE3\x83\x88\xE3\x83\xBC\xE3\x83\xAA\xE3\x83\xBC") {
        return true;
    }
    for (const std::string& tag : item.tags) {
        if (tag == "story" || tag == "story_item" || tag == "key_item" || tag == "unsellable" || tag == "no_sell") {
            return true;
        }
    }
    return false;
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
        entry.sellable = !instance.instance.protectionEnabled;
        if (!entry.sellable) {
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
            return !instance->instance.protectionEnabled && isSellableObject(instance->item);
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
                if (instance->instance.protectionEnabled) {
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
    if (!inventory_.addObjectItem(objectCatalog_, product.objectId)) {
        baseStatus_ = "リュックがいっぱいです";
        return;
    }
    money_ -= product.price;
    --product.quantity;
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
    if (slotIndex < 0 || slotIndex >= StoragePaneSlotCount) {
        return std::nullopt;
    }

    const std::vector<StorageEntry> entries = warehouseStorageEntries();
    const int pageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
    const int warehousePage = std::clamp(page, 0, pageCount - 1);
    const int entryIndex = warehouseEntryIndexAtStorageSlot(warehousePage * StoragePaneSlotCount + slotIndex);
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
    if (!target.valid) {
        return false;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        return processingEntryAvailable(target.backpackEntry, target.warehouseEntry);
    }

    if (target.ringIndex < 0 || target.ringIndex >= SpellRingCount) {
        return false;
    }
    const std::vector<SpellRingItem>& ringItems = spellRing_.itemsForRing(target.ringIndex);
    if (target.ringItemIndex < 0 || target.ringItemIndex >= static_cast<int>(ringItems.size())) {
        return false;
    }

    const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
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
    if (!processingEntryAvailable(entry, warehouseEntry)) {
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
    if (!target.valid) {
        baseStatus_ = "加工対象がありません";
        return;
    }
    if (target.source == BaseItemSource::Backpack || target.source == BaseItemSource::Warehouse) {
        applyProcessingEntry(target.backpackEntry, target.warehouseEntry);
        return;
    }

    if (!processingTargetAvailable(target)) {
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
        std::snprintf(buffer, sizeof(buffer), "%s x%d", stack.item.name.c_str(), stack.count);
        return buffer;
    }

    const InventoryObjectInstance& instance = warehouseEntry
        ? warehouseObjectInstances_[static_cast<std::size_t>(entry.index)]
        : inventory_.objectInstances()[static_cast<std::size_t>(entry.index)];
    std::snprintf(buffer, sizeof(buffer), "%s %s%s Lv.%d",
        instance.item.name.c_str(),
        instance.instance.protectionEnabled ? "[保護] " : "",
        instance.instance.isBroken ? "[破損] " : "",
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
    const std::optional<StorageEntry> entry = warehouseEntryForPageSlot(slotIndex, baseStorageWarehousePage_);
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
        return inventory_.screenObjectStackAt(target.slotIndex) != nullptr ||
            inventory_.screenObjectInstanceAt(target.slotIndex) != nullptr;
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
        baseStorageWithdrawSelection_ = std::clamp(baseStorageWithdrawSelection_, 0, StoragePaneSlotCount - 1);
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
    baseStorageWithdrawSelection_ = std::clamp(baseStorageWithdrawSelection_, 0, StoragePaneSlotCount - 1);
    baseStatus_ = "リュックに取り出しました";
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
    baseRingWorkshopSelection_ = 0;
    resetRingWorkshopDraft();
    baseStatus_.clear();
}

void Game::resetRingWorkshopDraft()
{
    ringWorkshopDraftRadiusPoints_ = levelRingRadiusPoints_;
    ringWorkshopDraftSpeedPoints_ = levelRingSpeedPoints_;
    ringWorkshopDraftWeightLimitPoints_ = levelRingWeightLimitPoints_;
}

int Game::ringLevelUpgradePointTotal() const
{
    return std::max(0, levelRingRadiusPoints_) +
        std::max(0, levelRingSpeedPoints_) +
        std::max(0, levelRingWeightLimitPoints_);
}

bool Game::ringWorkshopRespecChanged() const
{
    return ringWorkshopDraftRadiusPoints_ != levelRingRadiusPoints_ ||
        ringWorkshopDraftSpeedPoints_ != levelRingSpeedPoints_ ||
        ringWorkshopDraftWeightLimitPoints_ != levelRingWeightLimitPoints_;
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

bool Game::adjustRingWorkshopRespec(int fromIndex, int toIndex)
{
    if (ringLevelUpgradePointTotal() <= 0) {
        baseStatus_ = "再調整できるリング強化ポイントがありません";
        return false;
    }
    if (fromIndex == toIndex) {
        return false;
    }

    const auto pointRef = [this](int index) -> int& {
        switch (index) {
        case RingLevelUpgradeSpeed:
            return ringWorkshopDraftSpeedPoints_;
        case RingLevelUpgradeWeightLimit:
            return ringWorkshopDraftWeightLimitPoints_;
        case RingLevelUpgradeRadius:
        default:
            return ringWorkshopDraftRadiusPoints_;
        }
    };

    int& fromPoints = pointRef(fromIndex);
    int& toPoints = pointRef(toIndex);
    if (fromPoints <= 0) {
        baseStatus_ = std::string(ringLevelUpgradePointName(fromIndex)) + "から移せるポイントがありません";
        return false;
    }
    --fromPoints;
    ++toPoints;
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
    levelRingRadiusPoints_ = std::max(0, ringWorkshopDraftRadiusPoints_);
    levelRingSpeedPoints_ = std::max(0, ringWorkshopDraftSpeedPoints_);
    levelRingWeightLimitPoints_ = std::max(0, ringWorkshopDraftWeightLimitPoints_);
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
        return effectiveInitialRingRadius(levelRingRadiusPoints_);
    case RingWorkshopUpgrade::InitialSpeed:
        return effectiveInitialRingSpeed(levelRingSpeedPoints_);
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
        const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelRingRadiusPoints_)));
        return balance_.spellRingRadius * baseUpgradeMultiplier * workshopMultiplier * levelMultiplier;
    }
    case RingWorkshopUpgrade::InitialSpeed: {
        const float baseUpgradeMultiplier = 1.0f + static_cast<float>(ringSpeedUpgradeLevel_) * 0.08f;
        const float workshopMultiplier = 1.0f + static_cast<float>(currentLevel + 1) * 0.05f;
        const float levelMultiplier = static_cast<float>(std::pow(1.1, std::max(0, levelRingSpeedPoints_)));
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
    const UiRect homeEntranceRect = toUiRect(baseFacilityRectFor(BaseArea::Outdoor, "home_entrance", toBaseEditRect(fallback)));
    baseArea_ = BaseArea::Outdoor;
    basePlayerPosition_ = baseFacilitySpawnPosition(homeEntranceRect, BaseFacilitySpawnSide::Below, balance_.playerRadius);
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
    dialogue_.start(event->dialogue);
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
    pendingStoryTriggers_.clear();
    baseStatus_.clear();
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
    queueStoryEventForTrigger("title_base_enter");
}

void Game::updateBookshelfScreen(const Input& input, UiContext& ui)
{
    const auto objectCountForPage = [this](BookshelfPage page) {
        int count = 0;
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((page == BookshelfPage::Treasures && treasure) ||
                (page == BookshelfPage::Items && !treasure)) {
                ++count;
            }
        }
        return count;
    };

    if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
        if (bookshelfPage_ == BookshelfPage::Menu) {
            baseBookshelfActive_ = false;
            baseStatus_.clear();
        } else {
            bookshelfPage_ = BookshelfPage::Menu;
            bookshelfSelection_ = 0;
        }
        return;
    }

    const int itemCount = bookshelfPage_ == BookshelfPage::Menu
        ? BookshelfMenuItemCount
        : (bookshelfPage_ == BookshelfPage::Enemies ? static_cast<int>(enemyCatalog_.enemies.size()) : objectCountForPage(bookshelfPage_));
    if (itemCount <= 0) {
        bookshelfSelection_ = 0;
    } else {
        if (input.pressed(InputAction::MoveUp)) {
            bookshelfSelection_ = (bookshelfSelection_ + itemCount - 1) % itemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            bookshelfSelection_ = (bookshelfSelection_ + 1) % itemCount;
        }
        bookshelfSelection_ = std::clamp(bookshelfSelection_, 0, itemCount - 1);
    }

    const int visibleCount = std::min(BookshelfVisibleRows, itemCount);
    const int firstVisibleIndex = std::clamp(bookshelfSelection_ - visibleCount / 2, 0, std::max(0, itemCount - visibleCount));
    for (int i = 0; i < visibleCount; ++i) {
        const UiRect rect = bookshelfItemRect(i);
        const int itemIndex = firstVisibleIndex + i;
        if (rect.contains(ui.mouse())) {
            bookshelfSelection_ = itemIndex;
        }
        if (ui.pressed(rect)) {
            bookshelfSelection_ = itemIndex;
            if (bookshelfPage_ == BookshelfPage::Menu) {
                ui.emitSound(UiSoundEvent::BookOpen);
                bookshelfPage_ = static_cast<BookshelfPage>(bookshelfSelection_ + 1);
                bookshelfSelection_ = 0;
            } else {
                ui.emitSound(UiSoundEvent::Confirm);
            }
            return;
        }
    }

    if ((input.confirmPressed() || input.useItemPressed()) && bookshelfPage_ == BookshelfPage::Menu) {
        ui.emitSound(UiSoundEvent::BookOpen);
        bookshelfPage_ = static_cast<BookshelfPage>(bookshelfSelection_ + 1);
        bookshelfSelection_ = 0;
        return;
    }

    ui.block(basePanelRect());
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

    if (baseBookshelfActive_) {
        updateBookshelfScreen(input, ui);
        return;
    }

    if (baseRingWorkshopActive_) {
        if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
            baseRingWorkshopActive_ = false;
            resetRingWorkshopDraft();
            baseStatus_.clear();
            return;
        }
        if (input.pressed(InputAction::MoveUp)) {
            baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + BaseRingWorkshopItemCount - 1) % BaseRingWorkshopItemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseRingWorkshopSelection_ = (baseRingWorkshopSelection_ + 1) % BaseRingWorkshopItemCount;
        }
        const auto chooseWorkshopItem = [this, &ui](int item) {
            if (item >= 0 && item < RingWorkshopRespecTransferCount) {
                ui.emitSound(UiSoundEvent::Confirm);
                const RingWorkshopRespecTransfer transfer = RingWorkshopRespecTransfers[static_cast<std::size_t>(item)];
                adjustRingWorkshopRespec(transfer.from, transfer.to);
                return;
            }
            if (item == RingWorkshopConfirmItemIndex) {
                ui.emitSound(UiSoundEvent::Confirm);
                confirmRingWorkshopRespec();
                return;
            }
            if (item >= RingWorkshopUpgradeStartIndex &&
                item < RingWorkshopUpgradeStartIndex + RingWorkshopImplementedUpgradeCount) {
                ui.emitSound(UiSoundEvent::Confirm);
                buyRingWorkshopUpgrade(static_cast<RingWorkshopUpgrade>(item - RingWorkshopUpgradeStartIndex));
                return;
            }
            ui.emitSound(UiSoundEvent::Cancel);
            baseStatus_ = "この項目は未解禁です";
        };
        for (int i = 0; i < BaseRingWorkshopItemCount; ++i) {
            const UiRect rect = baseRingWorkshopItemRect(i);
            if (rect.contains(ui.mouse())) {
                baseRingWorkshopSelection_ = i;
            }
            if (ui.pressed(rect)) {
                baseRingWorkshopSelection_ = i;
                chooseWorkshopItem(i);
                return;
            }
        }
        if (ui.pressed(ringWorkshopConfirmRect())) {
            ui.emitSound(UiSoundEvent::Confirm);
            confirmRingWorkshopRespec();
            return;
        }
        if (input.confirmPressed() || input.useItemPressed()) {
            chooseWorkshopItem(baseRingWorkshopSelection_);
            return;
        }
        ui.block(basePanelRect());
        return;
    }

    if (baseStorageActive_) {
        const UiRect storageBounds = baseStorageMode_ == StorageUiMode::ChooseAction
            ? storageActionDialogRect()
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
            baseStorageQuantityDialog_ = {};
            baseStorageQuantityPending_ = {};
            baseStatus_.clear();
        };
        const auto openQuantityDialog = [this](StorageQuantityOperation operation, StorageTransferTarget target, int maxCount) {
            InventoryUiEntryView view = storageTransferTargetView(target);
            const std::string itemName = view.item != nullptr && !view.item->name.empty() ? view.item->name : std::string("アイテム");
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
                    if (i == 2) {
                        ui.emitSound(UiSoundEvent::ItemMove);
                        sortWarehouseByCatalogOrder();
                    } else {
                        ui.emitSound(UiSoundEvent::Confirm);
                        baseStorageMode_ = i == 0 ? StorageUiMode::Deposit : StorageUiMode::Withdraw;
                    }
                    ui.block(storageBounds);
                    return;
                }
            }
            if (input.confirmPressed() || input.useItemPressed()) {
                if (baseStorageActionSelection_ == 2) {
                    ui.emitSound(UiSoundEvent::ItemMove);
                    sortWarehouseByCatalogOrder();
                } else {
                    ui.emitSound(UiSoundEvent::Confirm);
                    baseStorageMode_ = baseStorageActionSelection_ == 0 ? StorageUiMode::Deposit : StorageUiMode::Withdraw;
                }
                ui.block(storageBounds);
                return;
            }
            ui.block(storageBounds);
            return;
        }

        if (baseStorageMode_ == StorageUiMode::Deposit) {
            const int currentTab = storageDepositSourceTabIndex(baseStorageDepositSource_);
            std::array<UiTabItem, StorageDepositSourceCount> sourceTabs{};
            std::array<UiRect, StorageDepositSourceCount> sourceTabRects{};
            for (int i = 0; i < StorageDepositSourceCount; ++i) {
                const int source = storageDepositSourceValue(i);
                sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(source)], true};
                sourceTabRects[static_cast<std::size_t>(i)] = storageDepositSourceRect(i);
            }
            UiTabsInput sourceTabsInput{};
            sourceTabsInput.focusDelta = input.activeRingDelta();
            const int directSourceFocus = input.shortcutSlotPressed();
            if (directSourceFocus >= 0 && directSourceFocus < StorageDepositSourceCount) {
                sourceTabsInput.directFocusIndex = directSourceFocus;
            }
            sourceTabsInput.commit =
                sourceTabsInput.focusDelta != 0 ||
                sourceTabsInput.directFocusIndex >= 0 ||
                input.confirmPressed() ||
                input.useItemPressed();
            const int sourceSelection = updateUiTabs(
                baseStorageDepositSourceTabs_,
                ui,
                sourceTabsInput,
                currentTab,
                sourceTabs.data(),
                static_cast<int>(sourceTabs.size()),
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
            const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
            baseStorageWarehousePage_ = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
            const UiPageSelectorRects pageRects = externalWarehousePageSelectorRects(storageTransferGridSlotRect);
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

            moveGridSelection(baseStorageWithdrawSelection_, StoragePaneSlotCount);
            int hoveredWarehouseSlot = -1;
            for (int i = 0; i < StoragePaneSlotCount; ++i) {
                const UiRect rect = externalWarehouseSourceSlotRect(storageTransferGridSlotRect, i);
                if (rect.contains(ui.mouse())) {
                    baseStorageWithdrawSelection_ = i;
                    hoveredWarehouseSlot = i;
                }
            }
            const auto moveWarehouseStorageItem = [this](StorageTransferTarget target, int toSlot) {
                if (target.source != BaseItemSource::Warehouse ||
                    !target.valid ||
                    toSlot < 0 ||
                    toSlot >= StoragePaneSlotCount) {
                    return false;
                }
                const int entryIndex = target.storageEntry.kind == StorageEntryKind::Stack
                    ? target.storageEntry.index
                    : static_cast<int>(warehouseObjectStacks_.size()) + target.storageEntry.index;
                const int storageSlot = baseStorageWarehousePage_ * StoragePaneSlotCount + toSlot;
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
                        const UiRect rect = externalWarehouseSourceSlotRect(storageTransferGridSlotRect, hoveredWarehouseSlot);
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
                const UiRect rect = externalWarehouseSourceSlotRect(storageTransferGridSlotRect, baseStorageWithdrawSelection_);
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
            if (!processingTargetAvailable(target)) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseStatus_ = "この作業はできません";
                return false;
            }
            const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
            const std::array<UiCommandMenuItem, 1> items{{{processingActionName(mode), true}}};
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
                144.0f,
                2);
            return true;
        };
        if (uiCancelRequested(baseCancelState_, input, ui, merchantPanelRect())) {
            if (baseProcessingCommandMenu_.open) {
                closeProcessingCommand();
            } else {
                baseProcessingActive_ = false;
                baseStatus_.clear();
            }
            return;
        }
        const ProcessingMode currentProcessingMode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
        const std::array<UiCommandMenuItem, 1> commandItems{{{processingActionName(currentProcessingMode), true}}};
        const bool commandOpenBeforeUpdate = baseProcessingCommandMenu_.open;
        const int commandSelection = updateUiCommandMenu(
            baseProcessingCommandMenu_,
            ui,
            input,
            commandItems.data(),
            static_cast<int>(commandItems.size()));
        if (commandSelection >= 0 && baseProcessingCommandSlot_ >= 0) {
            applyProcessingTarget(processingTargetForScreenSlot(baseProcessingCommandSlot_));
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

        std::array<UiTabItem, BaseProcessingModeCount> processingTabs{};
        std::array<UiRect, BaseProcessingModeCount> processingTabRects{};
        for (int i = 0; i < BaseProcessingModeCount; ++i) {
            const auto mode = static_cast<ProcessingMode>(i);
            processingTabs[static_cast<std::size_t>(i)] = {processingModeName(mode), processingModeUnlocked(mode)};
            processingTabRects[static_cast<std::size_t>(i)] = baseProcessingModeRect(i);
        }
        UiTabsInput processingTabsInput{};
        processingTabsInput.focusDelta = input.toggleShortcutRowPressed() ? 1 : 0;
        processingTabsInput.commit = input.confirmPressed() || input.useItemPressed();
        const int processingTabSelection = updateUiTabs(
            baseProcessingTabs_,
            ui,
            processingTabsInput,
            baseProcessingMode_,
            processingTabs.data(),
            static_cast<int>(processingTabs.size()),
            processingTabRects.data());
        if (processingTabSelection >= 0) {
            baseProcessingMode_ = processingTabSelection;
            return;
        }

        std::array<UiTabItem, BaseProcessingSourceCount> sourceTabs{};
        std::array<UiRect, BaseProcessingSourceCount> sourceTabRects{};
        for (int i = 0; i < BaseProcessingSourceCount; ++i) {
            sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(i)], true};
            sourceTabRects[static_cast<std::size_t>(i)] = baseProcessingSourceRect(i);
        }
        UiTabsInput sourceTabsInput{};
        sourceTabsInput.focusDelta = baseItemSourceIsWarehouse(baseProcessingSource_) ? 0 : input.activeRingDelta();
        const int directSourceFocus = input.shortcutSlotPressed();
        if (directSourceFocus >= 0 && directSourceFocus < BaseProcessingSourceCount) {
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
                static_cast<int>(sourceTabs.size()),
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

            std::array<UiTabItem, BaseItemSourceCount> sourceTabs{};
            std::array<UiRect, BaseItemSourceCount> sourceTabRects{};
            for (int i = 0; i < BaseItemSourceCount; ++i) {
                sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(i)], true};
                sourceTabRects[static_cast<std::size_t>(i)] = merchantSellSourceRect(i);
            }
            UiTabsInput sourceTabsInput{};
            sourceTabsInput.focusDelta = baseItemSourceIsWarehouse(baseMerchantSellSource_) ? 0 : input.activeRingDelta();
            const int directSourceFocus = input.shortcutSlotPressed();
            if (directSourceFocus >= 0 && directSourceFocus < BaseItemSourceCount) {
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
                static_cast<int>(sourceTabs.size()),
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
        if (input.pressed(InputAction::MoveUp)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + BaseUpgradeItemCount - 1) % BaseUpgradeItemCount;
        }
        if (input.pressed(InputAction::MoveDown)) {
            baseUpgradeSelection_ = (baseUpgradeSelection_ + 1) % BaseUpgradeItemCount;
        }
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            const UiRect rect = baseUpgradeItemRect(i);
            if (rect.contains(ui.mouse())) {
                baseUpgradeSelection_ = i;
            }
            if (ui.pressed(rect)) {
                ui.emitSound(UiSoundEvent::Confirm);
                baseUpgradeSelection_ = i;
                ui.block(upgradePanel);
                return;
            }
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
            baseRegenerateConfirmActive_ = true;
            baseStatus_.clear();
        };
        const auto confirmRegenerate = [this]() {
            baseRegenerateConfirmActive_ = false;
            requestMiningStartTransition(false, true);
        };

        if (baseRegenerateConfirmActive_) {
            if (ui.pressed(baseMiningRegenerateConfirmButtonRect(0)) || input.confirmPressed() || input.useItemPressed()) {
                ui.emitSound(UiSoundEvent::Confirm);
                confirmRegenerate();
                ui.block(baseMiningRegenerateConfirmRect());
                return;
            }
            if (ui.pressed(baseMiningRegenerateConfirmButtonRect(1)) || input.backPressed()) {
                ui.emitSound(UiSoundEvent::Cancel);
                baseRegenerateConfirmActive_ = false;
                baseStatus_.clear();
                ui.block(baseMiningRegenerateConfirmRect());
                return;
            }
            ui.block(baseMiningRegenerateConfirmRect());
            return;
        }

        if (uiCancelRequested(baseCancelState_, input, ui, basePanelRect())) {
            if (baseWarpPointSelectActive_) {
                baseWarpPointSelectActive_ = false;
            } else {
                baseMiningStartChoiceActive_ = false;
            }
            baseRegenerateConfirmActive_ = false;
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
            baseRegenerateConfirmActive_ = false;
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
            baseRegenerateConfirmActive_ = false;
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
                baseRegenerateConfirmActive_ = false;
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
            baseRegenerateConfirmActive_ = false;
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
            clampCurrentStageToSelectableStages();
            syncWarpStateForCurrentStage();
            baseMiningStartChoiceActive_ = true;
            baseMiningStartSelection_ = unlockedWarpPointCount_ > 0 ? 1 : 0;
            baseWarpPointSelectActive_ = false;
            baseWarpPointSelection_ = 0;
            baseRegenerateConfirmActive_ = false;
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Storage:
            baseStorageActive_ = true;
            baseStorageMode_ = StorageUiMode::ChooseAction;
            baseStorageActionSelection_ = 0;
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
            baseStatus_.clear();
            break;
        case BaseFacilityAction::Bookshelf:
            openBookshelf();
            break;
        case BaseFacilityAction::Diary: {
            std::string message;
            saveSaveData(message);
            baseStatus_ = message;
            break;
        }
        case BaseFacilityAction::RingWorkshop:
            if (facility.unlocked) {
                openRingWorkshop();
            } else {
                baseStatus_ = "リング工房用スペース: まだ解禁されていません";
            }
            break;
        case BaseFacilityAction::HomeEntrance:
            baseOutdoorPlayerPosition_ = basePlayerPosition_;
            requestBaseAreaCrossfade(
                BaseArea::HomeInterior,
                {640.0f, 542.0f},
                {0.0f, -1.0f},
                "ルネの家に入りました");
            break;
        case BaseFacilityAction::HomeExit:
            requestBaseAreaCrossfade(
                BaseArea::Outdoor,
                baseOutdoorPlayerPosition_,
                {0.0f, 1.0f},
                "屋外拠点に戻りました");
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

    const Vec2 moveAxis = input.moveAxis();
    const bool walkingNow = lengthSquared(moveAxis) > 0.0001f;
    if (walkingNow != basePlayerSpriteWalking_) {
        basePlayerSpriteWalking_ = walkingNow;
        basePlayerSpriteAnimationTime_ = 0.0f;
    } else {
        basePlayerSpriteAnimationTime_ += dt;
    }
    maybeSpawnPlayerFootstepDust(
        playerSpriteFootAnchor(basePlayerPosition_),
        lengthSquared(moveAxis) > 0.0001f ? moveAxis : basePlayerFacing_,
        basePlayerSpriteWalking_,
        playerSpriteFrameIndex(basePlayerSpriteAnimationTime_, basePlayerSpriteWalking_),
        previousBasePlayerDustFrame_);
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

    const BaseFacility* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const BaseFacility& facility : facilities) {
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }

    if (input.mouseLeftPressed() && !ui.pointerConsumed()) {
        for (const BaseFacility& facility : facilities) {
            if (!facility.rect.contains(ui.mouse())) {
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

    if (input.confirmPressed() && nearest != nullptr) {
        ui.emitSound(nearest->onInteract == BaseFacilityAction::Bookshelf ? UiSoundEvent::BookOpen : UiSoundEvent::Confirm);
        interact(*nearest);
        return;
    }
}

void Game::renderBookshelfScreen(Renderer& renderer) const
{
    const UiRect panel = basePanelRect();
    renderer.drawText(panel.pos + Vec2{202.0f, 214.0f}, "本棚", {246, 235, 255, 255}, 3);

    char buffer[256];
    const auto menuName = [](int index) {
        switch (index) {
        case 0:
            return "アイテム図鑑";
        case 1:
            return "宝図鑑";
        case 2:
            return "敵図鑑";
        default:
            return "";
        }
    };
    const auto objectAt = [this](BookshelfPage page, int targetIndex) -> const ObjectDefinition* {
        int index = 0;
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((page == BookshelfPage::Treasures && treasure) ||
                (page == BookshelfPage::Items && !treasure)) {
                if (index == targetIndex) {
                    return &object;
                }
                ++index;
            }
        }
        return nullptr;
    };

    if (bookshelfPage_ == BookshelfPage::Menu) {
        renderer.drawText(panel.pos + Vec2{76.0f, 224.0f}, "図鑑を選んでください", {198, 198, 206, 255}, 2);
        for (int i = 0; i < BookshelfMenuItemCount; ++i) {
            drawUiButton(renderer, bookshelfItemRect(i), menuName(i), i == bookshelfSelection_);
        }
        return;
    }

    std::vector<std::string> lines;
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        for (const EnemyDefinition& enemy : enemyCatalog_.enemies) {
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (enemy.name.empty() ? enemy.id : enemy.name);
            std::snprintf(buffer, sizeof(buffer), "%s  [%s]", name.c_str(), encyclopediaStageName(stage));
            lines.emplace_back(buffer);
        }
    } else {
        for (const ObjectDefinition& object : objectCatalog_.objects) {
            const bool treasure = object.category == "\xE5\xAE\x9D";
            if ((bookshelfPage_ == BookshelfPage::Treasures && !treasure) ||
                (bookshelfPage_ == BookshelfPage::Items && treasure)) {
                continue;
            }
            const EncyclopediaStage stage = encyclopedia_.objectStage(object.id, treasure);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object.name.empty() ? object.id : object.name);
            std::snprintf(buffer, sizeof(buffer), "%s  [%s]", name.c_str(), encyclopediaStageName(stage));
            lines.emplace_back(buffer);
        }
    }

    const char* title = bookshelfPage_ == BookshelfPage::Items
        ? "アイテム図鑑"
        : (bookshelfPage_ == BookshelfPage::Treasures ? "宝図鑑" : "敵図鑑");
    renderer.drawText(panel.pos + Vec2{74.0f, 224.0f}, title, {198, 198, 206, 255}, 2);
    if (lines.empty()) {
        renderer.drawText(panel.pos + Vec2{94.0f, 276.0f}, "記録対象がありません", {150, 150, 160, 255}, 2);
    } else {
        const int visibleCount = std::min(BookshelfVisibleRows, static_cast<int>(lines.size()));
        const int firstVisibleIndex = std::clamp(
            bookshelfSelection_ - visibleCount / 2,
            0,
            std::max(0, static_cast<int>(lines.size()) - visibleCount));
        for (int i = 0; i < visibleCount; ++i) {
            const int lineIndex = firstVisibleIndex + i;
            drawUiButton(renderer, bookshelfItemRect(i), lines[static_cast<std::size_t>(lineIndex)], lineIndex == bookshelfSelection_);
        }
    }

    const UiRect bookshelfDetailPanel{{414.0f, 548.0f}, {452.0f, 144.0f}};
    const Vec2 bookshelfDetailContent = uiSubPanelContentPos(bookshelfDetailPanel);
    drawUiSubPanel(renderer, bookshelfDetailPanel);
    if (bookshelfPage_ == BookshelfPage::Enemies) {
        if (bookshelfSelection_ >= 0 && bookshelfSelection_ < static_cast<int>(enemyCatalog_.enemies.size())) {
            const EnemyDefinition& enemy = enemyCatalog_.enemies[static_cast<std::size_t>(bookshelfSelection_)];
            const EncyclopediaStage stage = encyclopedia_.enemyStage(enemy.id);
            const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (enemy.name.empty() ? enemy.id : enemy.name);
            std::snprintf(buffer, sizeof(buffer), "%s / %s", name.c_str(), encyclopediaStageName(stage));
            renderer.drawText(bookshelfDetailContent, buffer, {255, 230, 150, 255}, 2);
        }
    } else if (const ObjectDefinition* object = objectAt(bookshelfPage_, bookshelfSelection_)) {
        const bool treasure = object->category == "\xE5\xAE\x9D";
        const EncyclopediaStage stage = encyclopedia_.objectStage(object->id, treasure);
        const std::string name = stage == EncyclopediaStage::Undiscovered ? "????" : (object->name.empty() ? object->id : object->name);
        std::snprintf(buffer, sizeof(buffer), "%s / %s", name.c_str(), encyclopediaStageName(stage));
        renderer.drawText(bookshelfDetailContent, buffer, {255, 230, 150, 255}, 2);
        const std::vector<std::string> effects = encyclopedia_.getObjectEffectDisplayLines(
            object->id,
            objectCatalog_,
            EffectRevealMode::WithUnknown);
        const std::string effectText = joinEffectLines(effects);
        renderer.drawWrappedText(
            bookshelfDetailContent + Vec2{0.0f, 34.0f},
            effectText,
            bookshelfDetailPanel.size.x - ui::SubPanelPadding.x * 2.0f,
            {232, 234, 242, 255},
            2);
    }
}

void Game::renderBaseBackdrop(Renderer& renderer) const
{
    renderer.fillRect({0.0f, 0.0f}, {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())}, {24, 28, 32, 255});
    const UiRect map = baseMapBounds();
    if (baseArea_ == BaseArea::HomeInterior) {
        renderer.fillRect(map.pos, map.size, {74, 58, 52, 255});
        renderer.drawRect(map.pos, map.size, {184, 150, 108, 255});
        renderer.fillRect({76.0f, 72.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 584.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({1160.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({132.0f, 132.0f}, {1016.0f, 436.0f}, {118, 92, 66, 255});
        renderer.drawText({558.0f, 88.0f}, "ルネの家", {246, 235, 255, 255}, 2);
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
    const BaseFacility* nearest = nullptr;
    const BaseFacility* hovered = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    const Vec2 mouse{mouseX, mouseY};
    for (const BaseFacility& facility : facilities) {
        if (!facility.enabled) {
            continue;
        }
        if (facility.rect.contains(mouse)) {
            hovered = &facility;
        }
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }
    for (int pass = 0; pass < 2; ++pass) {
        const bool drawEnabled = pass == 1;
        for (const BaseFacility& facility : facilities) {
            if (facility.enabled != drawEnabled) {
                continue;
            }
            const bool texturedOutdoorBase = baseArea_ == BaseArea::Outdoor && renderer.hasBaseMapTexture();
            Color fill = facility.enabled ? Color{96, 82, 82, 255} : Color{84, 62, 56, 255};
            if (texturedOutdoorBase) {
                fill.a = facility.enabled ? 120 : 80;
            }
            if (!facility.unlocked) {
                fill = {58, 58, 64, 255};
                if (texturedOutdoorBase) {
                    fill.a = 120;
                }
            }
            if (&facility == nearest) {
                fill = {118, 98, 58, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            if (&facility == hovered) {
                fill = {124, 104, 72, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            renderer.fillRect(facility.rect.pos, facility.rect.size, fill);
            renderer.drawRect(facility.rect.pos, facility.rect.size, facility.enabled ? Color{220, 200, 150, 255} : Color{120, 108, 98, 255});
            renderer.drawText(facility.rect.pos + Vec2{8.0f, 8.0f}, facility.displayName, facility.enabled ? Color{248, 238, 214, 255} : Color{154, 146, 138, 255}, 2);
        }
    }
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
        renderer.fillRect(map.pos, map.size, {74, 58, 52, 255});
        renderer.drawRect(map.pos, map.size, {184, 150, 108, 255});
        renderer.fillRect({76.0f, 72.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 584.0f}, {1128.0f, 44.0f}, {96, 68, 62, 255});
        renderer.fillRect({76.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({1160.0f, 116.0f}, {44.0f, 468.0f}, {96, 68, 62, 255});
        renderer.fillRect({132.0f, 132.0f}, {1016.0f, 436.0f}, {118, 92, 66, 255});
        renderer.drawText({558.0f, 88.0f}, "ルネの家", {246, 235, 255, 255}, 2);
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
    const BaseFacility* nearest = nullptr;
    const BaseFacility* hovered = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    SDL_GetMouseState(&mouseX, &mouseY);
    const Vec2 mouse{mouseX, mouseY};
    for (const BaseFacility& facility : facilities) {
        if (!facility.enabled) {
            continue;
        }
        if (facility.rect.contains(mouse)) {
            hovered = &facility;
        }
        if (!baseInteractionAvailable(basePlayerPosition_, facility)) {
            continue;
        }
        const float dist = distanceToRect(basePlayerPosition_, facility.rect);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearest = &facility;
        }
    }
    for (int pass = 0; pass < 2; ++pass) {
        const bool drawEnabled = pass == 1;
        for (const BaseFacility& facility : facilities) {
            if (facility.enabled != drawEnabled) {
                continue;
            }
            const bool texturedOutdoorBase = baseArea_ == BaseArea::Outdoor && renderer.hasBaseMapTexture();
            Color fill = facility.enabled ? Color{96, 82, 82, 255} : Color{84, 62, 56, 255};
            if (texturedOutdoorBase) {
                fill.a = facility.enabled ? 120 : 80;
            }
            if (!facility.unlocked) {
                fill = {58, 58, 64, 255};
                if (texturedOutdoorBase) {
                    fill.a = 120;
                }
            }
            if (&facility == nearest) {
                fill = {118, 98, 58, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            if (&facility == hovered) {
                fill = {124, 104, 72, static_cast<unsigned char>(texturedOutdoorBase ? 170 : 255)};
            }
            renderer.fillRect(facility.rect.pos, facility.rect.size, fill);
            renderer.drawRect(facility.rect.pos, facility.rect.size, facility.enabled ? Color{220, 200, 150, 255} : Color{120, 108, 98, 255});
            renderer.drawText(facility.rect.pos + Vec2{8.0f, 8.0f}, facility.displayName, facility.enabled ? Color{248, 238, 214, 255} : Color{154, 146, 138, 255}, 2);
        }
    }
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
        baseBookshelfActive_ ||
        baseStorageActive_ ||
        baseProcessingActive_ ||
        baseSellActive_ ||
        baseUpgradeActive_ ||
        baseMiningStartChoiceActive_;
    const bool storageActionDialogActive = baseStorageActive_ && baseStorageMode_ == StorageUiMode::ChooseAction;
    const bool merchantActionDialogActive = baseSellActive_ && baseMerchantMode_ == MerchantUiMode::ChooseAction;
    const UiRect panel = storageActionDialogActive
        ? storageActionDialogRect()
        : (merchantActionDialogActive
        ? merchantActionDialogRect()
        : ((baseProcessingActive_ ||
        (baseStorageActive_ && baseStorageMode_ != StorageUiMode::ChooseAction) ||
        (baseSellActive_ && baseMerchantMode_ != MerchantUiMode::ChooseAction))
        ? merchantPanelRect()
        : (baseUpgradeActive_ ? baseUpgradePanelRect() : basePanelRect())));
    std::optional<UiWindowScope> panelWindow;
    std::optional<UiCancelControlScope> panelCancelScope;
    if (panelUiActive) {
        const char* panelHelp = "F/Enter 決定  左クリック 決定  Esc/右クリック 戻る";
        if (baseBookshelfActive_) {
            panelHelp = bookshelfPage_ == BookshelfPage::Menu
                ? "F/Enter 決定  Esc/右クリック 戻る"
                : "Esc/右クリック 戻る";
        } else if (baseSellActive_) {
            panelHelp = baseMerchantMode_ == MerchantUiMode::Buy
                ? "F/Enter 買う  Esc/右クリック 戻る"
                : (baseMerchantMode_ == MerchantUiMode::Sell ? "F/Enter 売る  Z/X・1-5 対象/ページ切替  Esc/右クリック 戻る" : "F/Enter 決定  Esc/右クリック 戻る");
        } else if (baseStorageActive_) {
            panelHelp = baseStorageMode_ == StorageUiMode::Deposit
                ? "F/Enter コマンド  Z/X・1-4 対象切替  Esc/右クリック 戻る"
                : (baseStorageMode_ == StorageUiMode::Withdraw ? "F/Enter コマンド  Z/X 倉庫ページ  Esc/右クリック 戻る" : "F/Enter 決定  Esc/右クリック 戻る");
        } else if (baseProcessingActive_) {
            panelHelp = "F/Enter 確定/実行  Tab 作業選択  Z/X・1-5 対象/ページ切替  Esc/右クリック 戻る";
        } else if (baseUpgradeActive_) {
            panelHelp = "F/Enter 強化  Esc/右クリック 戻る";
        } else if (baseMiningStartChoiceActive_) {
            panelHelp = baseWarpPointSelectActive_
                ? "F/Enter 出発  Z/X・方向キー 選択  Esc/右クリック 戻る"
                : "F/Enter 決定  左クリック 決定  Esc/右クリック 戻る";
        }
        const char* panelTitle = "魔女の拠点";
        if (baseBookshelfActive_) {
            panelTitle = "本棚";
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
                panelTitle = "収納箱 取り出す";
            } else {
                panelTitle = "収納箱";
            }
        } else if (baseUpgradeActive_) {
            panelTitle = "拠点強化炉";
        } else if (baseMiningStartChoiceActive_) {
            panelTitle = "ダンジョン入口";
        }
        const bool panelCancelButton = true;
        if (panelCancelButton) {
            panelCancelScope.emplace(baseCancelState_);
        }
        panelWindow.emplace(renderer, "base.panel", panel, panelTitle, panelHelp, UiWindowOptions{true, panelCancelButton});
    }

    if (baseStorageActive_) {
        if (baseStorageMode_ == StorageUiMode::ChooseAction) {
            std::snprintf(buffer, sizeof(buffer), "収納数：%d/%d", warehouseUsedSlots(), warehouseCapacity());
            renderer.drawText(smallActionInfoTextPos(panel), buffer, {198, 198, 206, 255}, 2);
            constexpr std::array<std::string_view, 3> Choices{"しまう", "取り出す", "並び替え"};
            for (int i = 0; i < static_cast<int>(Choices.size()); ++i) {
                drawUiButton(renderer, storageActionChoiceRect(i), Choices[static_cast<std::size_t>(i)], i == baseStorageActionSelection_, uiActionButtonStyle());
            }
        } else {
            const UiRect detailPanel = merchantDetailPanelRect();
            InventoryUiEntryView detailEntry{};
            const SpellRingItem* selectedRingItem = nullptr;
            if (baseStorageMode_ == StorageUiMode::Deposit) {
                std::array<UiTabItem, StorageDepositSourceCount> sourceTabs{};
                std::array<UiRect, StorageDepositSourceCount> sourceTabRects{};
                for (int i = 0; i < StorageDepositSourceCount; ++i) {
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
                    static_cast<int>(sourceTabs.size()),
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
                }
            } else if (baseStorageMode_ == StorageUiMode::Withdraw) {
                const int warehousePageCount = std::max(1, (warehouseCapacity() + StoragePaneSlotCount - 1) / StoragePaneSlotCount);
                const int warehousePage = std::clamp(baseStorageWarehousePage_, 0, warehousePageCount - 1);
                std::snprintf(buffer, sizeof(buffer), "収納箱 %d/%d", warehouseUsedSlots(), warehouseCapacity());
                renderer.drawText(storageTransferCountTextPos(), buffer, ui::TextMuted, 2);
                drawExternalWarehouseSourceHeader(
                    renderer,
                    storageTransferGridSlotRect,
                    warehousePage,
                    warehousePageCount);
                for (int i = 0; i < StoragePaneSlotCount; ++i) {
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
                    drawInventoryUiSlot(renderer, externalWarehouseSourceSlotRect(storageTransferGridSlotRect, i), view, style);
                }
                detailEntry = storageTransferTargetView(storageWithdrawTargetForSlot(
                    std::clamp(baseStorageWithdrawSelection_, 0, StoragePaneSlotCount - 1)));
            }

            if (detailEntry.item == nullptr && selectedRingItem != nullptr) {
                drawUiSubPanel(renderer, detailPanel);
                float detailLineY = drawUiDetailHeader(renderer, detailPanel, ringItemDisplayName(objectCatalog_, *selectedRingItem));
                drawUiDetailText(renderer, detailPanel, detailLineY, "このアイテムは収納箱にしまえません。");
            } else {
                drawInventoryUiDetailPanel(renderer, detailPanel, detailEntry, objectCatalog_, encyclopedia_, false);
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
        renderer.drawText(panel.pos + Vec2{168.0f, 214.0f}, "リング工房", {246, 235, 255, 255}, 3);
        const int totalPoints = ringLevelUpgradePointTotal();
        std::snprintf(buffer, sizeof(buffer), "強化点 %d  現在: 半径%d / 速度%d / 重量%d  案: 半径%d / 速度%d / 重量%d",
            totalPoints,
            levelRingRadiusPoints_,
            levelRingSpeedPoints_,
            levelRingWeightLimitPoints_,
            ringWorkshopDraftRadiusPoints_,
            ringWorkshopDraftSpeedPoints_,
            ringWorkshopDraftWeightLimitPoints_);
        renderer.drawText(panel.pos + Vec2{54.0f, 224.0f}, buffer, {198, 198, 206, 255}, 2);

        const float currentRadius = effectiveInitialRingRadius(levelRingRadiusPoints_);
        const float currentSpeed = effectiveInitialRingSpeed(levelRingSpeedPoints_);
        const float currentWeightLimit = effectiveInitialRingWeightLimit(levelRingWeightLimitPoints_);
        const float draftRadius = effectiveInitialRingRadius(ringWorkshopDraftRadiusPoints_);
        const float draftSpeed = effectiveInitialRingSpeed(ringWorkshopDraftSpeedPoints_);
        const float draftWeightLimit = effectiveInitialRingWeightLimit(ringWorkshopDraftWeightLimitPoints_);
        std::snprintf(buffer, sizeof(buffer), "案: 半径 %.0f->%.0f / 速度 %.2f->%.2f / 重量 %.1f->%.1fkg  費用 %dG 月のカケラ%d",
            currentRadius,
            draftRadius,
            currentSpeed,
            draftSpeed,
            currentWeightLimit,
            draftWeightLimit,
            ringWorkshopRespecMoneyCost(),
            ringWorkshopRespecMoonCost());
        renderer.drawText(panel.pos + Vec2{54.0f, 532.0f}, buffer, {198, 198, 206, 255}, 2);

        for (int i = 0; i < BaseRingWorkshopItemCount; ++i) {
            if (i >= 0 && i < RingWorkshopRespecTransferCount) {
                const RingWorkshopRespecTransfer transfer = RingWorkshopRespecTransfers[static_cast<std::size_t>(i)];
                std::snprintf(buffer, sizeof(buffer), "%s→%s  半%d 速%d 重%d",
                    ringLevelUpgradePointName(transfer.from),
                    ringLevelUpgradePointName(transfer.to),
                    ringWorkshopDraftRadiusPoints_,
                    ringWorkshopDraftSpeedPoints_,
                    ringWorkshopDraftWeightLimitPoints_);
            } else if (i == RingWorkshopConfirmItemIndex) {
                std::snprintf(buffer, sizeof(buffer), "再調整確定  %dG 月のカケラ%d",
                    ringWorkshopRespecMoneyCost(),
                    ringWorkshopRespecMoonCost());
            } else if (i >= RingWorkshopUpgradeStartIndex &&
                i < RingWorkshopUpgradeStartIndex + RingWorkshopImplementedUpgradeCount) {
                const auto upgrade = static_cast<RingWorkshopUpgrade>(i - RingWorkshopUpgradeStartIndex);
                const int level = ringWorkshopUpgradeLevel(upgrade);
                const int maxLevel = ringWorkshopUpgradeMaxLevel(upgrade);
                if (level >= maxLevel) {
                    std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  上限",
                        ringWorkshopUpgradeName(upgrade),
                        level,
                        maxLevel);
                } else {
                    std::snprintf(buffer, sizeof(buffer), "%s Lv.%d/%d  %.2f -> %.2f  %dG 月のカケラ%d",
                        ringWorkshopUpgradeName(upgrade),
                        level,
                        maxLevel,
                        ringWorkshopUpgradeCurrentValue(upgrade),
                        ringWorkshopUpgradeNextValue(upgrade),
                        ringWorkshopUpgradeMoneyCost(upgrade),
                        ringWorkshopUpgradeMoonCost(upgrade));
                }
            } else {
                const char* futureName = "";
                switch (i) {
                case RingWorkshopUpgradeStartIndex + RingWorkshopImplementedUpgradeCount:
                    futureName = "リング投げ距離強化";
                    break;
                case RingWorkshopUpgradeStartIndex + RingWorkshopImplementedUpgradeCount + 1:
                    futureName = "リング投げクールダウン短縮";
                    break;
                case RingWorkshopUpgradeStartIndex + RingWorkshopImplementedUpgradeCount + 2:
                    futureName = "リング重量ペナルティ軽減";
                    break;
                case RingWorkshopUpgradeStartIndex + RingWorkshopImplementedUpgradeCount + 3:
                    futureName = "リング装着枠増加";
                    break;
                default:
                    futureName = "未解禁項目";
                    break;
                }
                std::snprintf(buffer, sizeof(buffer), "%s  未解禁", futureName);
            }
            drawUiButton(renderer, baseRingWorkshopItemRect(i), buffer, i == baseRingWorkshopSelection_, uiActionButtonStyle());
        }

        std::snprintf(buffer, sizeof(buffer), "所持 %dG / 月のカケラ %d   F/Enter 実行  Esc/右クリック 戻る",
            money_,
            inventory_.materialCount(MaterialType::MoonFragment));
        renderer.drawText(panel.pos + Vec2{54.0f, 558.0f}, buffer, {198, 198, 206, 255}, 2);
        drawUiButton(renderer, ringWorkshopConfirmRect(), "再調整確定", false, uiActionButtonStyle());
    } else if (baseProcessingActive_) {
        std::array<UiTabItem, BaseProcessingModeCount> processingTabs{};
        std::array<UiRect, BaseProcessingModeCount> processingTabRects{};
        for (int i = 0; i < BaseProcessingModeCount; ++i) {
            const auto processingTabMode = static_cast<ProcessingMode>(i);
            processingTabs[static_cast<std::size_t>(i)] = {processingModeName(processingTabMode), processingModeUnlocked(processingTabMode)};
            processingTabRects[static_cast<std::size_t>(i)] = baseProcessingModeRect(i);
        }
        const ProcessingMode mode = static_cast<ProcessingMode>(std::clamp(baseProcessingMode_, 0, BaseProcessingModeCount - 1));
        drawUiTabs(
            renderer,
            baseProcessingTabs_,
            baseProcessingMode_,
            processingTabs.data(),
            static_cast<int>(processingTabs.size()),
            processingTabRects.data());

        const auto processingModeDescription = [](ProcessingMode descriptionMode) -> std::string_view {
            switch (descriptionMode) {
            case ProcessingMode::Repair: return "耐久力が減った道具を所持金で修理します。";
            case ProcessingMode::Attack: return "強化鉱石と所持金を使い、攻撃力補正を上げます。";
            case ProcessingMode::Dig: return "強化鉱石と所持金を使い、抑制力補正を上げます。";
            case ProcessingMode::Durability: return "強化鉱石と所持金を使い、最大耐久力を上げます。";
            case ProcessingMode::ResetEnhancement: return "強化値と補正を初期化します。加工状態は残ります。";
            case ProcessingMode::Lighten: return "強化鉱石と所持金を使い、重量を軽くします。";
            case ProcessingMode::Enlarge: return "強化鉱石と所持金を使い、当たり判定を大きくします。重量も増えます。";
            }
            return "";
        };
        const UiRect firstTabRect = processingTabRects.front();
        const UiRect lastGridSlotRect = baseProcessingGridSlotRect(7);
        const Vec2 descriptionPos{firstTabRect.pos.x, firstTabRect.pos.y + firstTabRect.size.y + 10.0f};
        const float descriptionWidth = std::max(0.0f, lastGridSlotRect.pos.x + lastGridSlotRect.size.x - descriptionPos.x);
        renderer.drawWrappedText(descriptionPos, processingModeDescription(mode), descriptionWidth, ui::TextMuted, 2);

        std::array<UiTabItem, BaseProcessingSourceCount> sourceTabs{};
        std::array<UiRect, BaseProcessingSourceCount> sourceTabRects{};
        for (int i = 0; i < BaseProcessingSourceCount; ++i) {
            sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(i)], true};
            sourceTabRects[static_cast<std::size_t>(i)] = baseProcessingSourceRect(i);
        }
        drawUiTabs(
            renderer,
            baseProcessingSourceTabs_,
            baseProcessingSource_,
            sourceTabs.data(),
            static_cast<int>(sourceTabs.size()),
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
                const bool unavailable = view.item != nullptr && !processingTargetAvailable(processingTargetForScreenSlot(i));
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
                const bool unavailable = view.item != nullptr && !processingTargetAvailable(processingTargetForScreenSlot(i));
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
        drawUiSubPanel(renderer, detailPanel);

        const auto drawSectionLabel = [&renderer](UiRect targetPanel, float& y, std::string_view text) {
            const UiRect content = uiSubPanelContentRect(targetPanel);
            renderer.drawText({content.pos.x, y}, text, ui::Text, 2);
            y += 31.0f;
        };
        const auto drawTextRun = [&renderer](Vec2& pos, std::string_view text, Color color, int scale) {
            renderer.drawText(pos, text, color, scale);
            pos.x += renderer.measureText(text, scale).x;
        };
        const auto drawMoneyCostLine = [&](UiRect targetPanel, float& y, int moneyCost) {
            const UiRect content = uiSubPanelContentRect(targetPanel);
            Vec2 pos{content.pos.x, y};
            const Color numberColor = money_ >= moneyCost ? ui::Text : Color{238, 82, 82, 255};
            const std::string number = std::to_string(moneyCost);
            drawTextRun(pos, number, numberColor, 2);
            drawTextRun(pos, "G", ui::Text, 2);
            y += 31.0f;
        };
        const auto drawOreCostLine = [&](UiRect targetPanel, float& y, int oreCost) {
            const int ownedOre = inventory_.materialCount(MaterialType::EnhancementOre);
            const bool enough = ownedOre >= oreCost;
            const Color numberColor = enough ? ui::Text : Color{238, 82, 82, 255};
            const UiRect content = uiSubPanelContentRect(targetPanel);
            Vec2 pos{content.pos.x, y};
            drawTextRun(pos, "強化鉱石 ×", ui::Text, 2);
            drawTextRun(pos, std::to_string(oreCost), numberColor, 2);
            drawTextRun(pos, " (", ui::Text, 2);
            drawTextRun(pos, std::to_string(ownedOre), numberColor, 2);
            drawTextRun(pos, ")", ui::Text, 2);
            y += 31.0f;
        };

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
            float detailLineY = drawUiDetailHeader(renderer, detailPanel, "アイテム未選択");
            drawUiDetailText(renderer, detailPanel, detailLineY, "加工するアイテムを選択してください。");
        } else {
            const ItemData* item = detailEntry.item;
            std::optional<InventoryUiItemStats> stats = inventoryUiEntryStats(detailEntry);
            if (!stats && selectedRingItem != nullptr) {
                stats = inventoryUiStatsFromRingItem(*selectedRingItem);
            }
            std::string detailTitle = item != nullptr ? item->name : ringItemDisplayName(objectCatalog_, *selectedRingItem);
            if (item != nullptr && !stats && detailEntry.stackCount > 1) {
                std::snprintf(buffer, sizeof(buffer), "%s x%d", item->name.c_str(), detailEntry.stackCount);
                detailTitle = buffer;
            }
            float detailLineY = drawUiDetailHeader(renderer, detailPanel, detailTitle);
            drawUiDetailText(renderer, detailPanel, detailLineY, item != nullptr && !item->description.empty() ? item->description : "-");

            const int enhanceLevel = stats ? stats->enhanceLevel : 0;
            const int attackBonus = stats ? stats->attackBonus : 0;
            const int digBonus = stats ? stats->digBonus : 0;
            const int durabilityBonus = stats ? stats->durabilityBonus : 0;
            const int currentDurability = stats ? stats->currentDurability : (item != nullptr ? item->durability : -1);
            const int maxDurability = stats ? stats->maxDurability : (item != nullptr ? item->durability : -1);
            double weightModifier = 1.0;
            double sizeModifier = 1.0;
            if (detailEntry.instance != nullptr) {
                weightModifier = detailEntry.instance->weightModifier;
                sizeModifier = detailEntry.instance->sizeModifier;
            } else if (selectedRingItem != nullptr) {
                weightModifier = selectedRingItem->weightModifier;
                sizeModifier = selectedRingItem->sizeModifier;
            }

            drawSectionLabel(detailPanel, detailLineY, "現在");
            std::snprintf(buffer, sizeof(buffer), "%d / %d", enhanceLevel, MaxItemEnhanceLevel);
            drawUiDetailLine(renderer, detailPanel, detailLineY, "強化Lv", buffer);
            if (maxDurability < 0) {
                std::snprintf(buffer, sizeof(buffer), "壊れない");
            } else {
                std::snprintf(buffer, sizeof(buffer), "%d/%d", currentDurability, maxDurability);
            }
            drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
            std::snprintf(buffer, sizeof(buffer), "+%d / +%d / +%d", attackBonus, digBonus, durabilityBonus);
            drawUiDetailLine(renderer, detailPanel, detailLineY, "補正", buffer);
            std::snprintf(buffer, sizeof(buffer), "重量%.0f%% / 大きさ%.0f%%", weightModifier * 100.0, sizeModifier * 100.0);
            drawUiDetailLine(renderer, detailPanel, detailLineY, "加工", buffer);

            const bool available = processingTargetAvailable(selectedTarget);
            std::string blockedReason;
            if (!available) {
                if (!processingModeUnlocked(mode)) {
                    blockedReason = "この作業は未解禁です";
                } else if ((mode == ProcessingMode::Repair || mode == ProcessingMode::ResetEnhancement) &&
                    (selectedTarget.source == BaseItemSource::Backpack || selectedTarget.source == BaseItemSource::Warehouse) &&
                    selectedTarget.backpackEntry.kind == StorageEntryKind::Stack) {
                    blockedReason = "この作業はできません";
                } else if (mode == ProcessingMode::ResetEnhancement) {
                    blockedReason = "リセット不要です";
                } else if (mode == ProcessingMode::Lighten || mode == ProcessingMode::Enlarge) {
                    blockedReason = "加工済みです";
                } else {
                    blockedReason = mode == ProcessingMode::Repair ? "修理不要です" : "強化上限です";
                }
            }

            if (mode == ProcessingMode::Repair) {
                drawSectionLabel(detailPanel, detailLineY, "修理後");
                if (maxDurability < 0 ||
                    ((selectedTarget.source == BaseItemSource::Backpack || selectedTarget.source == BaseItemSource::Warehouse) &&
                        selectedTarget.backpackEntry.kind == StorageEntryKind::Stack)) {
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", "-");
                } else {
                    std::snprintf(buffer, sizeof(buffer), "%d/%d -> %d/%d",
                        currentDurability,
                        maxDurability,
                        maxDurability,
                        maxDurability);
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
                }
            } else if (mode == ProcessingMode::ResetEnhancement) {
                drawSectionLabel(detailPanel, detailLineY, "リセット後");
                drawUiDetailLine(renderer, detailPanel, detailLineY, "強化Lv", "0");
                drawUiDetailLine(renderer, detailPanel, detailLineY, "補正", "+0 / +0 / +0");
            } else if (mode == ProcessingMode::Lighten) {
                drawSectionLabel(detailPanel, detailLineY, "加工後");
                std::snprintf(buffer, sizeof(buffer), "%.0f%% -> %.0f%%", weightModifier * 100.0, weightModifier * LightenWeightMultiplier * 100.0);
                drawUiDetailLine(renderer, detailPanel, detailLineY, "重量", buffer);
                std::snprintf(buffer, sizeof(buffer), "%.0f%%", sizeModifier * 100.0);
                drawUiDetailLine(renderer, detailPanel, detailLineY, "大きさ", buffer);
            } else if (mode == ProcessingMode::Enlarge) {
                drawSectionLabel(detailPanel, detailLineY, "加工後");
                std::snprintf(buffer, sizeof(buffer), "%.0f%% -> %.0f%%", sizeModifier * 100.0, sizeModifier * EnlargeSizeMultiplier * 100.0);
                drawUiDetailLine(renderer, detailPanel, detailLineY, "大きさ", buffer);
                std::snprintf(buffer, sizeof(buffer), "%.0f%% -> %.0f%%", weightModifier * 100.0, weightModifier * EnlargeWeightMultiplier * 100.0);
                drawUiDetailLine(renderer, detailPanel, detailLineY, "重量", buffer);
            } else {
                drawSectionLabel(detailPanel, detailLineY, "強化後");
                std::snprintf(buffer, sizeof(buffer), "%d -> %d", enhanceLevel, std::min(MaxItemEnhanceLevel, enhanceLevel + 1));
                drawUiDetailLine(renderer, detailPanel, detailLineY, "強化Lv", buffer);
                if (mode == ProcessingMode::Attack) {
                    std::snprintf(buffer, sizeof(buffer), "+%d -> +%d", attackBonus, attackBonus + 1);
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "攻撃力", buffer);
                } else if (mode == ProcessingMode::Dig) {
                    std::snprintf(buffer, sizeof(buffer), "+%d -> +%d", digBonus, digBonus + 1);
                    drawUiDetailLine(renderer, detailPanel, detailLineY, "抑制力", buffer);
                } else if (mode == ProcessingMode::Durability) {
                    if (maxDurability < 0) {
                        drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", "-");
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%d -> %d", maxDurability, maxDurability + 2);
                        drawUiDetailLine(renderer, detailPanel, detailLineY, "耐久力", buffer);
                    }
                }
            }

            if (available) {
                const int moneyCost = processingMoneyCost(selectedTarget, mode);
                const int oreCost = processingOreCost(selectedTarget, mode);
                drawSectionLabel(detailPanel, detailLineY, "強化素材");
                drawMoneyCostLine(detailPanel, detailLineY, moneyCost);
                if (oreCost > 0) {
                    drawOreCostLine(detailPanel, detailLineY, oreCost);
                }
                if (blockedReason.empty()) {
                    if (money_ < moneyCost) {
                        blockedReason = "所持金が足りません";
                    } else if (inventory_.materialCount(MaterialType::EnhancementOre) < oreCost) {
                        blockedReason = "強化鉱石が足りません";
                    }
                }
            }

            if (blockedReason.empty()) {
                drawUiDetailText(renderer, detailPanel, detailLineY, std::string("Enter ") + processingActionName(mode));
            } else {
                drawUiDetailText(renderer, detailPanel, detailLineY, blockedReason);
            }
        }
        const std::array<UiCommandMenuItem, 1> processingCommandItems{{{processingActionName(mode), true}}};
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
                std::array<UiTabItem, BaseItemSourceCount> sourceTabs{};
                std::array<UiRect, BaseItemSourceCount> sourceTabRects{};
                for (int i = 0; i < BaseItemSourceCount; ++i) {
                    sourceTabs[static_cast<std::size_t>(i)] = {BaseItemSourceLabels[static_cast<std::size_t>(i)], true};
                    sourceTabRects[static_cast<std::size_t>(i)] = merchantSellSourceRect(i);
                }
                drawUiTabs(
                    renderer,
                    baseMerchantSellSourceTabs_,
                    baseMerchantSellSource_,
                    sourceTabs.data(),
                    static_cast<int>(sourceTabs.size()),
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
                drawInventoryUiDetailPanel(renderer, detailPanel, detailEntry, objectCatalog_, encyclopedia_, false, extraLines);
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
        for (int i = 0; i < BaseUpgradeItemCount; ++i) {
            const UiRect rect = baseUpgradeItemRect(i);
            const bool implemented = upgradeImplemented(i);
            const bool maxed = implemented && upgradeMaxed(i);
            const bool hot = i == selected;
            if (!implemented) {
                std::snprintf(buffer, sizeof(buffer), "未実装");
            } else if (maxed) {
                std::snprintf(buffer, sizeof(buffer), "上限");
            } else {
                std::snprintf(buffer, sizeof(buffer), "Lv.%d/%d", upgradeLevel(i), upgradeMaxLevel(i));
            }
            UiSmallSelectButtonStyle selectStyle;
            selectStyle.valueText = maxed ? Color{160, 220, 190, 255} : ui::TextMuted;
            drawUiSmallSelectButton(renderer, rect, shortName(i), buffer, hot, !implemented, selectStyle);
        }

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
                drawDetailTextRow(detailY, "効果", "リング工房スペースを解禁", Color{255, 230, 150, 255});
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
                "F/Enter 出発  Z/X・方向キー 選択  Esc/右クリック 戻る",
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
        const char* promptName = nearest != nullptr ? nearest->displayName : "";
        renderer.fillRect({280.0f, 644.0f}, {720.0f, 34.0f}, ui::FooterFill);
        if (nearest != nullptr) {
            if (const char* specialPrompt = baseInteractionPrompt(*nearest)) {
                std::snprintf(buffer, sizeof(buffer), "%s", specialPrompt);
            } else {
                std::snprintf(buffer, sizeof(buffer), "Enter: %sを調べる / クリック: 近くの施設を調べる / Esc: メニュー", promptName);
            }
            renderer.drawText({300.0f, 652.0f}, buffer, {255, 230, 150, 255}, 2);
        } else {
            renderer.drawText({300.0f, 652.0f}, "Enter: 近くの施設を調べる  Esc: メニュー", {198, 198, 206, 255}, 2);
        }
        if (!baseStatus_.empty()) {
            UiSystemMessageStyle statusStyle;
            statusStyle.fill = {0, 0, 0, 160};
            statusStyle.padding = {20.0f, 3.0f};
            drawUiSystemMessage(renderer, baseStatus_, {300.0f, 612.0f}, statusStyle);
        }
        return;
    }

    if (!baseStatus_.empty()) {
        drawUiSystemMessage(
            renderer,
            baseStatus_,
            baseSystemMessagePos(panel, baseStorageActive_, baseSellActive_, baseProcessingActive_, baseUpgradeActive_));
    }
    if (baseRegenerateConfirmActive_) {
        panelCancelScope.reset();
        panelWindow.reset();

        renderer.fillRect(
            {0.0f, 0.0f},
            {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
            {0, 0, 0, 96});

        const UiRect confirmPanel = baseMiningRegenerateConfirmRect();
        UiWindowScope confirmWindow(
            renderer,
            "base.mining.regenerate.confirm",
            confirmPanel,
            "再生成確認",
            "F/Enter 再生成  Esc/右クリック 戻る",
            UiWindowOptions{true, false});
        renderer.drawWrappedText(
            confirmPanel.pos + Vec2{40.0f, 112.0f},
            "現在の坑道状態を破棄して、地形・敵・宝箱・ワープポイントを作り直します。\n拾っていないドロップがある場合は消えます。\nボス再戦用に地形も作り直します。",
            confirmPanel.size.x - 80.0f,
            {246, 235, 255, 255},
            2);
        drawUiButton(renderer, baseMiningRegenerateConfirmButtonRect(0), "再生成する", true, uiActionButtonStyle());
        drawUiButton(renderer, baseMiningRegenerateConfirmButtonRect(1), "戻る", false, uiCancelButtonStyle());
    }
    if (baseResultDialog_.open) {
        panelCancelScope.reset();
        panelWindow.reset();
        renderer.fillRect(
            {0.0f, 0.0f},
            {static_cast<float>(camera_.width()), static_cast<float>(camera_.height())},
            {0, 0, 0, 96});
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
